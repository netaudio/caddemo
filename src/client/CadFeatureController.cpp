#include "CadFeatureController.h"

#include "CadCanvas.h"
#include "CadCommands.h"
#include "CadDocument.h"
#include "CadEntityPainter.h"
#include "CadGeometry.h"

#include <QPainter>
#include <QPen>

#include <cmath>

// ---------------------------------------------------------------------------
// 命令解析
// ---------------------------------------------------------------------------
FeatureTool featureToolFromCommand(const QString& command)
{
    if (command == QStringLiteral("切线过原点"))  return FeatureTool::Tangent;
    if (command == QStringLiteral("设定位点"))    return FeatureTool::PositionPoint;
    if (command == QStringLiteral("工件对齐"))    return FeatureTool::Align;
    if (command == QStringLiteral("轮廓跟踪"))    return FeatureTool::Contour;
    if (command == QStringLiteral("工件测量"))    return FeatureTool::Measure;
    return FeatureTool::None;
}

QString featureToolLabel(FeatureTool tool)
{
    switch (tool) {
    case FeatureTool::Tangent:       return QStringLiteral("切线过原点");
    case FeatureTool::PositionPoint: return QStringLiteral("设定位点");
    case FeatureTool::Align:         return QStringLiteral("工件对齐");
    case FeatureTool::Contour:       return QStringLiteral("轮廓跟踪");
    case FeatureTool::Measure:       return QStringLiteral("工件测量");
    default:                         return {};
    }
}

// ---------------------------------------------------------------------------
// 控制器
// ---------------------------------------------------------------------------
CadFeatureController::CadFeatureController(CadDocument* document, CadCanvas* canvas,
                                         QUndoStack* undoStack, QObject* parent)
    : QObject(parent)
    , m_document(document)
    , m_canvas(canvas)
    , m_undo(undoStack)
{
}

void CadFeatureController::setActive(FeatureTool tool, const QString& label)
{
    m_tool = tool;
    m_phase = 0;
    m_label = label;
    m_work.clear();
    m_workIds.clear();
    m_points.clear();
    m_picked.clear();
    m_havePoint = false;
    m_hoverValid = false;
    if (m_canvas) {
        m_canvas->setInteraction(CadCanvas::Interaction::Select);
        m_canvas->setCursor(Qt::CrossCursor);
        m_canvas->update();
    }
    emit toolChanged();
}

void CadFeatureController::setPrompt(const QString& text)
{
    m_prompt = text;
    emit statusMessage(text);
}

bool CadFeatureController::startCommand(const QString& command)
{
    const FeatureTool tool = featureToolFromCommand(command);
    if (tool == FeatureTool::None)
        return false;
    setActive(tool, featureToolLabel(tool));

    const bool needsSelection = (tool == FeatureTool::Align || tool == FeatureTool::Contour);
    if (needsSelection) {
        if (m_document && !m_document->selection().isEmpty()) {
            loadSelection();
            if (tool == FeatureTool::Contour) {
                opContour();              // 已选中则直接跟踪
                return true;
            }
            m_phase = 1;                 // Align：进入参考点拾取
            setPrompt(QStringLiteral("工件对齐：拾取参考点，该点将被移到原点 (0,0)"));
        } else {
            beginSelectPhase(tool == FeatureTool::Contour
                                 ? QStringLiteral("轮廓跟踪：框选相互衔接的图元，右键/Esc 结束选择")
                                 : QStringLiteral("工件对齐：框选要移动的图元，右键/Esc 结束选择"));
        }
        return true;
    }

    switch (tool) {
    case FeatureTool::Tangent:
        setPrompt(QStringLiteral("切线过原点：拾取圆/圆弧（以原点 0,0 作切线）"));
        break;
    case FeatureTool::PositionPoint:
        setPrompt(QStringLiteral("设定位点：拾取定位点位置（默认原点 0,0）"));
        break;
    case FeatureTool::Measure:
        setPrompt(QStringLiteral("工件测量：拾取第一点，或直接点选圆/圆弧/直线"));
        break;
    default:
        break;
    }
    return true;
}

void CadFeatureController::loadSelection()
{
    if (!m_document)
        return;
    m_work.clear();
    m_workIds.clear();
    for (qint64 id : m_document->selection()) {
        if (const cad::Entity* e = m_document->find(id)) {
            m_work.append(*e);
            m_workIds.append(id);
        }
    }
}

void CadFeatureController::beginSelectPhase(const QString& tip)
{
    m_phase = -1;
    m_picked.clear();
    setPrompt(tip);
}

QVector<cad::Entity> CadFeatureController::snapshot() const
{
    return m_work;
}

void CadFeatureController::commitAdd(const QVector<cad::Entity>& created, const QString& label)
{
    cadcmd::addEntities(m_document, m_undo, created, label);
    m_document->setSelection({});
    endCommand();
}

void CadFeatureController::endCommand()
{
    const bool wasActive = isActive();
    m_tool = FeatureTool::None;
    m_phase = 0;
    m_work.clear();
    m_workIds.clear();
    m_points.clear();
    m_picked.clear();
    m_havePoint = false;
    if (m_canvas) {
        m_canvas->setCursor(Qt::ArrowCursor);
        m_canvas->update();
    }
    emit toolChanged();
    if (wasActive)
        emit statusMessage(QStringLiteral("%1：命令结束（右键 / Esc）").arg(m_label));
}

void CadFeatureController::cancel()
{
    if (!isActive())
        return;
    endCommand();
}

void CadFeatureController::onRightClick()
{
    if (m_phase == -1) {
        if (m_picked.isEmpty()) {
            cancel();
            return;
        }
        m_work.clear();
        m_workIds.clear();
        for (qint64 id : m_picked) {
            if (const cad::Entity* e = m_document->find(id)) {
                m_work.append(*e);
                m_workIds.append(id);
            }
        }
        if (m_tool == FeatureTool::Contour) {
            opContour();
            return;
        }
        if (m_tool == FeatureTool::Align) {
            m_phase = 1;
            setPrompt(QStringLiteral("工件对齐：拾取参考点，该点将被移到原点 (0,0)"));
            return;
        }
        cancel();
        return;
    }
    cancel();
}

void CadFeatureController::onPoint(const cad::Vec2& world, qint64 hitId)
{
    if (!isActive())
        return;
    if (m_phase == -1) {
        if (hitId == 0) {
            setPrompt(QStringLiteral("%1：空白处无效，请单击图元加入选择（右键/Esc 结束选择）")
                          .arg(m_label));
            return;
        }
        if (m_picked.contains(hitId))
            m_picked.removeAll(hitId);
        else
            m_picked.append(hitId);
        QSet<qint64> sel;
        for (qint64 id : m_picked)
            sel.insert(id);
        m_document->setSelection(sel);
        setPrompt(QStringLiteral("%1：已选 %2 个对象，右键/Esc 结束选择")
                      .arg(m_label).arg(m_picked.size()));
        return;
    }

    switch (m_tool) {
    case FeatureTool::Tangent:       opTangentPick(hitId); break;
    case FeatureTool::PositionPoint:  opPositionPick(world); break;
    case FeatureTool::Align:         opAlignPick(world); break;
    case FeatureTool::Measure:       opMeasurePick(world, hitId); break;
    default: break;
    }
}

// ---- 切线过原点 ----
void CadFeatureController::opTangentPick(qint64 hitId)
{
    if (hitId == 0 || !m_document) {
        setPrompt(QStringLiteral("切线过原点：请单击一个圆 / 圆弧"));
        return;
    }
    const cad::Entity* e = m_document->find(hitId);
    if (!e || (e->type != cad::EntityType::Circle && e->type != cad::EntityType::Arc
               && e->type != cad::EntityType::Concentric)) {
        setPrompt(QStringLiteral("切线过原点：所选不是圆 / 圆弧"));
        return;
    }
    const cad::Vec2 center = e->points.empty() ? cad::Vec2{0, 0} : e->points.front();
    const double r = e->radius;
    std::vector<cad::Vec2> tangpts = cad::tangentsFromPoint({0.0, 0.0}, center, r);
    // 圆弧：仅保留切点落在弧角范围内的切线
    if (e->type == cad::EntityType::Arc) {
        std::vector<cad::Vec2> filtered;
        for (const cad::Vec2& t : tangpts) {
            if (cad::angleWithinArc(t, center, e->startAngle, e->endAngle - e->startAngle))
                filtered.push_back(t);
        }
        tangpts = filtered;
    }
    if (tangpts.empty()) {
        setPrompt(QStringLiteral("切线过原点：原点在圆内，无切线（请选不含原点的圆）"));
        return;
    }
    QVector<cad::Entity> created;
    const cad::ColorIndex col = cad::ColorIndex::Blue;
    const cad::LineType lt = cad::LineType::Solid;
    const double lw = 0.0;
    for (const cad::Vec2& t : tangpts) {
        cad::Entity line;
        line.type = cad::EntityType::Line;
        line.id = 0;
        line.color = col;
        line.lineType = lt;
        line.lineWidth = lw;
        // 从原点出发，过切点并略向外延伸
        const cad::Vec2 dir = (t.x == 0.0 && t.z == 0.0) ? cad::Vec2{1, 0} : t;
        const cad::Vec2 end = t + cad::normalized(dir) * (cad::distance({0, 0}, t) * 0.2 + 10.0);
        line.points = {{0.0, 0.0}, end};
        created.append(line);
    }
    commitAdd(created, QStringLiteral("切线过原点"));
}

// ---- 设定位点 ----
void CadFeatureController::opPositionPick(const cad::Vec2& world)
{
    QVector<cad::Entity> created;
    cad::Entity pt;
    pt.type = cad::EntityType::Point;
    pt.id = 0;
    pt.color = cad::ColorIndex::Yellow;
    pt.lineWidth = 0.0;
    pt.points = {world};
    created.append(pt);

    cad::Entity label;
    label.type = cad::EntityType::Text;
    label.id = 0;
    label.color = cad::ColorIndex::Yellow;
    label.textHeight = 6.0;
    label.points = {world.x + 4.0, world.z + 8.0};
    label.text = QStringLiteral("定位点(%1,%2)")
                     .arg(world.x, 0, 'f', 1).arg(world.z, 0, 'f', 1).toStdString();
    created.append(label);

    commitAdd(created, QStringLiteral("设定位点"));
}

// ---- 工件对齐：参考点移到原点 ----
void CadFeatureController::opAlignPick(const cad::Vec2& world)
{
    if (m_work.isEmpty())
        return;
    const cad::Vec2 delta = {0.0 - world.x, 0.0 - world.z};
    QVector<cad::Entity> after = snapshot();
    for (cad::Entity& e : after)
        cad::transformTranslate(e, delta);
    m_undo->push(new cadcmd::ModifyEntities(m_document, m_work, after,
                                           QStringLiteral("工件对齐")));
    m_document->setSelection({});
    endCommand();
}

// ---- 轮廓跟踪 ----
void CadFeatureController::opContour()
{
    if (m_work.isEmpty()) {
        setPrompt(QStringLiteral("轮廓跟踪：请先选中相互衔接的图元"));
        return;
    }
    std::vector<cad::Segment> segs;
    for (const cad::Entity& e : m_work) {
        const std::vector<cad::Segment> es = cad::entitySegments(e);
        segs.insert(segs.end(), es.begin(), es.end());
    }
    if (segs.empty()) {
        setPrompt(QStringLiteral("轮廓跟踪：所选图元不含可衔接的线段（请选直线/曲线/矩形/圆弧）"));
        return;
    }
    const std::vector<cad::Vec2> contour = cad::chainContour(segs);
    if (contour.size() < 2) {
        setPrompt(QStringLiteral("轮廓跟踪：端点无法衔接成连续轮廓"));
        return;
    }
    cad::Entity curve;
    curve.type = cad::EntityType::Curve;
    curve.id = 0;
    curve.color = m_document ? m_document->currentColor() : cad::ColorIndex::Black;
    curve.lineType = m_document ? m_document->currentLineType() : cad::LineType::Solid;
    curve.lineWidth = m_document ? m_document->currentLineWidth() : 0.0;
    for (const cad::Vec2& p : contour)
        curve.points.push_back(p);
    commitAdd({curve}, QStringLiteral("轮廓跟踪"));
}

// ---- 工件测量 ----
void CadFeatureController::opMeasurePick(const cad::Vec2& world, qint64 hitId)
{
    auto makeDim = [this](const cad::Vec2& a, const cad::Vec2& b, const QString& txt) {
        cad::Entity dim;
        dim.type = cad::EntityType::Dimension;
        dim.id = 0;
        dim.color = m_document ? m_document->currentColor() : cad::ColorIndex::Black;
        dim.lineType = m_document ? m_document->currentLineType() : cad::LineType::Solid;
        dim.lineWidth = m_document ? m_document->currentLineWidth() : 0.0;
        dim.textHeight = 6.0;
        dim.points = {a, b};
        dim.text = txt.toStdString();
        return dim;
    };

    if (m_phase == 0) {
        // 直接点选圆/圆弧/直线：就地测量并标注
        if (hitId != 0) {
            const cad::Entity* e = m_document->find(hitId);
            if (e) {
                if (e->type == cad::EntityType::Circle || e->type == cad::EntityType::Concentric) {
                    const cad::Vec2 c = e->points.empty() ? cad::Vec2{0, 0} : e->points.front();
                    cad::Entity t;
                    t.type = cad::EntityType::Text;
                    t.id = 0;
                    t.color = e->color;
                    t.textHeight = 6.0;
                    t.points = {c.x + e->radius + 4.0, c.z};
                    t.text = QStringLiteral("R=%1").arg(e->radius, 0, 'f', 2).toStdString();
                    commitAdd({t}, QStringLiteral("工件测量"));
                    return;
                }
                if (e->type == cad::EntityType::Arc) {
                    const cad::Vec2 c = e->points.empty() ? cad::Vec2{0, 0} : e->points.front();
                    cad::Entity t;
                    t.type = cad::EntityType::Text;
                    t.id = 0;
                    t.color = e->color;
                    t.textHeight = 6.0;
                    t.points = {c.x + e->radius + 4.0, c.z};
                    t.text = QStringLiteral("R=%1 ∠=%2°")
                                 .arg(e->radius, 0, 'f', 2)
                                 .arg(cad::arcSpanDeg(*e), 0, 'f', 1).toStdString();
                    commitAdd({t}, QStringLiteral("工件测量"));
                    return;
                }
                if (e->type == cad::EntityType::Line && e->points.size() >= 2) {
                    const double len = cad::distance(e->points[0], e->points[1]);
                    commitAdd({makeDim(e->points[0], e->points[1],
                                      QStringLiteral("%1").arg(len, 0, 'f', 2))},
                              QStringLiteral("工件测量"));
                    return;
                }
            }
        }
        // 否则进入两点测量
        m_points.clear();
        m_points.push_back(world);
        m_havePoint = true;
        m_phase = 1;
        setPrompt(QStringLiteral("工件测量：拾取第二点"));
        return;
    }

    if (m_phase == 1) {
        const cad::Vec2 a = m_points[0];
        const double d = cad::distance(a, world);
        commitAdd({makeDim(a, world, QStringLiteral("%1").arg(d, 0, 'f', 2))},
                  QStringLiteral("工件测量"));
    }
}

void CadFeatureController::onHover(const cad::Vec2& world)
{
    m_hover = world;
    m_hoverValid = true;
    if (isActive())
        m_canvas->update();
}

cad::Vec2 CadFeatureController::snap(const cad::Vec2& point) const
{
    if (!m_canvas || m_canvas->delta() <= 0.0)
        return point;
    const double d = m_canvas->delta();
    return {std::round(point.x / d) * d, std::round(point.z / d) * d};
}

bool CadFeatureController::handleInput(const QString&)
{
    // 特定功能命令暂不支持键盘数值输入（直接点选即可）
    return false;
}

// ---------------------------------------------------------------------------
// 预览
// ---------------------------------------------------------------------------
QVector<cad::Entity> CadFeatureController::previewEntities() const
{
    if (!m_hoverValid)
        return {};
    const cad::Vec2 hover = snap(m_hover);
    QVector<cad::Entity> result;

    if (m_tool == FeatureTool::Measure && m_phase == 1 && m_havePoint) {
        cad::Entity dim;
        dim.type = cad::EntityType::Dimension;
        dim.points = {m_points[0], hover};
        dim.textHeight = 6.0;
        dim.text = QStringLiteral("%1").arg(cad::distance(m_points[0], hover), 0, 'f', 2)
                       .toStdString();
        result.append(dim);
    } else if (m_tool == FeatureTool::Align && m_phase == 1) {
        const cad::Vec2 delta = {0.0 - hover.x, 0.0 - hover.z};
        result = snapshot();
        for (cad::Entity& e : result)
            cad::transformTranslate(e, delta);
    }
    return result;
}

void CadFeatureController::drawPreview(QPainter& painter, const CadViewTransform& view,
                                      const QSizeF& vp)
{
    const QVector<cad::Entity> preview = previewEntities();
    if (preview.isEmpty())
        return;
    QPen pen(QColor("#2b6cd9"), 1, Qt::DashLine);
    pen.setCosmetic(true);
    for (const cad::Entity& e : preview) {
        painter.setPen(pen);
        cadpainter::drawEntity(painter, e, view, vp);
        if (e.type == cad::EntityType::Dimension && e.points.size() >= 2) {
            const QPointF a = view.toScreen(e.points[0], vp);
            const QPointF b = view.toScreen(e.points[1], vp);
            const QPointF dir = (b - a);
            const double len = std::hypot(dir.x(), dir.y());
            if (len > 1.0) {
                const QPointF u = dir / len;
                const QPointF perp(-u.y(), u.x());
                painter.drawLine(a, a + perp * 5.0);
                painter.drawLine(b, b + perp * 5.0);
            }
            QFont font(QStringLiteral("Microsoft YaHei"));
            font.setPixelSize(std::clamp(static_cast<int>(e.textHeight * view.scale), 8, 96));
            painter.setFont(font);
            painter.setPen(QColor("#2b6cd9"));
            painter.drawText((a + b) / 2.0 + QPointF(4, -4), QString::fromStdString(e.text));
        }
    }
}
