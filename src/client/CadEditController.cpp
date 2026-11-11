#include "CadEditController.h"

#include "CadCanvas.h"
#include "CadCommands.h"
#include "CadDocument.h"
#include "CadEntityPainter.h"
#include "CadGeometry.h"

#include <QPainter>
#include <QPen>
#include <QSet>

#include <cmath>

// ---------------------------------------------------------------------------
// 命令解析
// ---------------------------------------------------------------------------
EditTool editToolFromCommand(const QString& command)
{
    if (command == QStringLiteral("平移"))        return EditTool::Move;
    if (command == QStringLiteral("旋转"))        return EditTool::Rotate;
    if (command == QStringLiteral("缩放"))        return EditTool::Scale;
    if (command == QStringLiteral("偏移"))        return EditTool::Offset;
    if (command == QStringLiteral("对称"))        return EditTool::Mirror;
    if (command == QStringLiteral("修剪"))        return EditTool::Trim;
    if (command == QStringLiteral("延伸"))        return EditTool::Extend;
    if (command == QStringLiteral("箭头"))        return EditTool::Arrow;
    if (command == QStringLiteral("标注"))        return EditTool::Dimension;
    if (command == QStringLiteral("文本设置"))    return EditTool::TextSetting;
    return EditTool::None;
}

QString editToolLabel(EditTool tool)
{
    switch (tool) {
    case EditTool::Move:        return QStringLiteral("平移");
    case EditTool::Rotate:      return QStringLiteral("旋转");
    case EditTool::Scale:       return QStringLiteral("缩放");
    case EditTool::Offset:      return QStringLiteral("偏移");
    case EditTool::Mirror:      return QStringLiteral("对称");
    case EditTool::Trim:        return QStringLiteral("修剪");
    case EditTool::Extend:      return QStringLiteral("延伸");
    case EditTool::Arrow:       return QStringLiteral("箭头");
    case EditTool::Dimension:   return QStringLiteral("标注");
    case EditTool::TextSetting: return QStringLiteral("文本设置");
    default:                    return {};
    }
}

// ---------------------------------------------------------------------------
// 控制器
// ---------------------------------------------------------------------------
CadEditController::CadEditController(CadDocument* document, CadCanvas* canvas,
                                     QUndoStack* undoStack, QObject* parent)
    : QObject(parent)
    , m_document(document)
    , m_canvas(canvas)
    , m_undo(undoStack)
{
}

void CadEditController::setActive(EditTool tool, const QString& label)
{
    m_tool = tool;
    m_phase = 0;
    m_label = label;
    m_work.clear();
    m_workIds.clear();
    m_points.clear();
    m_picked.clear();
    m_boundaryId = 0;
    m_targetId = 0;
    m_haveValue = false;
    m_text.clear();
    m_hoverValid = false;
    if (m_canvas) {
        m_canvas->setInteraction(CadCanvas::Interaction::Select);
        m_canvas->setCursor(Qt::CrossCursor);
        m_canvas->update();
    }
    emit toolChanged();
}

void CadEditController::setPrompt(const QString& text)
{
    m_prompt = text;
    emit statusMessage(text);
}

bool CadEditController::startTool(EditTool tool)
{
    if (tool == EditTool::None)
        return false;
    setActive(tool, editToolLabel(tool));
    const bool needsSelection = (tool == EditTool::Move || tool == EditTool::Rotate
                                 || tool == EditTool::Scale || tool == EditTool::Mirror);
    if (needsSelection) {
        if (m_document && !m_document->selection().isEmpty()) {
            loadSelection();
            m_phase = 1;
            setPrompt(QStringLiteral("%1：已载入 %2 个选中对象，拾取基点")
                          .arg(m_label).arg(m_work.size()));
        } else {
            m_phase = -1;
            setPrompt(QStringLiteral("%1：拾取要操作的对象（可多选），右键/Esc 结束选择")
                          .arg(m_label));
        }
    } else {
        m_phase = 0;
        switch (tool) {
        case EditTool::Offset:
            setPrompt(QStringLiteral("偏移：拾取要偏移的图元，再输入距离 D=* 或点选偏移侧"));
            break;
        case EditTool::Trim:
            setPrompt(QStringLiteral("修剪：先拾取作为边界的图元，再拾取要修剪的图元（点选保留侧的反向）"));
            break;
        case EditTool::Extend:
            setPrompt(QStringLiteral("延伸：先拾取边界图元，再拾取要延伸的图元"));
            break;
        case EditTool::Arrow:
            setPrompt(QStringLiteral("箭头：拾取要添加箭头的直线（或先选中直线）"));
            break;
        case EditTool::Dimension:
            setPrompt(QStringLiteral("标注：拾取第一点"));
            break;
        case EditTool::TextSetting:
            setPrompt(QStringLiteral("文本设置：拾取要修改的文本（或先选中文本）"));
            break;
        default:
            break;
        }
    }
    return true;
}

bool CadEditController::startCommand(const QString& command)
{
    return startTool(editToolFromCommand(command));
}

void CadEditController::loadSelection()
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

void CadEditController::beginSelectPhase(const QString& tip)
{
    m_phase = -1;
    m_picked.clear();
    setPrompt(tip);
}

QVector<cad::Entity> CadEditController::snapshot() const
{
    return m_work;
}

void CadEditController::commitModify(const QVector<cad::Entity>& after, const QString& label)
{
    m_undo->push(new cadcmd::ModifyEntities(m_document, m_work, after, label));
    m_document->setSelection({});
    endCommand();
}

void CadEditController::commitAdd(const QVector<cad::Entity>& created, const QString& label)
{
    cadcmd::addEntities(m_document, m_undo, created, label);
    m_document->setSelection({});
    endCommand();
}

void CadEditController::endCommand()
{
    const bool wasActive = isActive();
    m_tool = EditTool::None;
    m_phase = 0;
    m_work.clear();
    m_workIds.clear();
    m_points.clear();
    m_picked.clear();
    m_boundaryId = 0;
    m_targetId = 0;
    if (m_canvas) {
        m_canvas->setCursor(Qt::ArrowCursor);
        m_canvas->update();
    }
    emit toolChanged();
    if (wasActive)
        emit statusMessage(QStringLiteral("%1：命令结束（右键 / Esc）").arg(m_label));
}

void CadEditController::cancel()
{
    if (!isActive())
        return;
    endCommand();
}

void CadEditController::onRightClick()
{
    if (m_phase == -1) {                       // 选择阶段结束
        if (m_picked.isEmpty()) {
            cancel();
            return;
        }
        m_work.clear();
        m_workIds = m_picked;
        QVector<cad::Entity> loaded;
        for (qint64 id : m_picked) {
            if (const cad::Entity* e = m_document->find(id)) {
                m_work.append(*e);
                loaded.append(*e);
            }
        }
        m_phase = 1;
        setPrompt(QStringLiteral("%1：已选择 %2 个对象，拾取基点")
                      .arg(m_label).arg(loaded.size()));
    } else {
        cancel();
    }
}

void CadEditController::finishCurrent()
{
    if (m_phase == -1) {
        onRightClick();
        return;
    }
    if (m_hoverValid) {
        onPoint(m_hover, 0);
        return;
    }
    cancel();
}

// ---------------------------------------------------------------------------
// 点位收集
// ---------------------------------------------------------------------------
void CadEditController::onPoint(const cad::Vec2& world, qint64 hitId)
{
    if (!isActive())
        return;

    if (m_phase == -1) {                        // 选择阶段
        onSelectPhasePick(hitId);
        return;
    }

    switch (m_tool) {
    case EditTool::Move:
    case EditTool::Rotate:
    case EditTool::Scale:
        onTransformPick(world);
        break;
    case EditTool::Offset:
        opOffsetPick(world, hitId);
        break;
    case EditTool::Trim:
    case EditTool::Extend:
        opTrimExtendPick(world, hitId);
        break;
    case EditTool::Arrow:
        opArrowPick(hitId);
        break;
    case EditTool::Dimension:
        onDimensionPick(world);
        break;
    case EditTool::Mirror:
        onMirrorPick(world);
        break;
    case EditTool::TextSetting:
        opTextPick(hitId);
        break;
    default:
        break;
    }
}

void CadEditController::onSelectPhasePick(qint64 hitId)
{
    if (!m_document)
        return;
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
}

void CadEditController::onTransformPick(const cad::Vec2& world)
{
    if (m_phase == 1) {                         // 基点
        m_points.push_back(world);
        m_phase = 2;
        const QString extra = (m_tool == EditTool::Move) ? QStringLiteral("目标点 / dx,dy")
                             : (m_tool == EditTool::Rotate) ? QStringLiteral("参考点（定义起始角）")
                             : QStringLiteral("参考点（定义基准长度）");
        setPrompt(QStringLiteral("%1：拾取%2").arg(m_label).arg(extra));
    } else if (m_phase == 2) {                  // 参考点
        m_points.push_back(world);
        m_phase = 3;
        const QString extra = (m_tool == EditTool::Rotate)
                                  ? QStringLiteral("目标点 或 输入角度 A=*（度）")
                                  : QStringLiteral("目标点 或 输入比例因子 S=*");
        setPrompt(QStringLiteral("%1：拾取%2").arg(m_label).arg(extra));
    } else if (m_phase == 3) {                  // 终值点
        applyTransform(world);
    }
}

void CadEditController::applyTransform(const cad::Vec2& world)
{
    const cad::Vec2 base = m_points[0];
    const cad::Vec2 ref = m_points[1];
    QVector<cad::Entity> after = snapshot();
    QString label;
    if (m_tool == EditTool::Move) {
        const cad::Vec2 d = world - base;
        for (cad::Entity& e : after)
            cad::transformTranslate(e, d);
        label = QStringLiteral("平移");
    } else if (m_tool == EditTool::Rotate) {
        const double a0 = cad::angleOf(ref - base);
        const double a1 = cad::angleOf(world - base);
        const double deg = a1 - a0;
        for (cad::Entity& e : after)
            cad::transformRotate(e, base, deg);
        m_lastAngle = deg;
        label = QStringLiteral("旋转");
    } else {  // Scale
        const double r0 = cad::distance(base, ref);
        const double r1 = cad::distance(base, world);
        const double factor = (r0 > cad::kEps) ? r1 / r0 : 1.0;
        for (cad::Entity& e : after)
            cad::transformScale(e, base, factor);
        m_lastScale = factor;
        label = QStringLiteral("缩放");
    }
    commitModify(after, label);
}

void CadEditController::onMirrorPick(const cad::Vec2& world)
{
    if (m_phase == 1) {
        m_points.push_back(world);
        m_phase = 2;
        setPrompt(QStringLiteral("对称：拾取镜像线第二点"));
    } else if (m_phase == 2) {
        const cad::Vec2 a = m_points[0];
        QVector<cad::Entity> created;
        for (const cad::Entity& src : m_work) {
            cad::Entity mirrored = src;
            mirrored.id = 0;                     // 新建副本
            cad::transformMirror(mirrored, a, world);
            created.append(mirrored);
        }
        commitAdd(created, QStringLiteral("对称（镜像）"));
    }
}

void CadEditController::onDimensionPick(const cad::Vec2& world)
{
    if (m_phase == 0) {
        m_points.push_back(world);
        m_phase = 1;
        setPrompt(QStringLiteral("标注：拾取第二点"));
    } else if (m_phase == 1) {
        const cad::Vec2 a = m_points[0];
        const cad::Vec2 b = world;
        cad::Entity dim;
        dim.type = cad::EntityType::Dimension;
        dim.points = {a, b};
        dim.textHeight = m_lastTextH;
        dim.color = m_document ? m_document->currentColor() : cad::ColorIndex::Black;
        dim.lineType = m_document ? m_document->currentLineType() : cad::LineType::Solid;
        dim.lineWidth = m_document ? m_document->currentLineWidth() : 0.0;
        dim.text = QStringLiteral("%1").arg(cad::distance(a, b), 0, 'f', 2).toStdString();
        commitAdd({dim}, QStringLiteral("标注"));
    }
}

void CadEditController::opOffsetPick(const cad::Vec2& world, qint64 hitId)
{
    if (m_phase == 0) {
        if (hitId == 0 || !m_document) {
            setPrompt(QStringLiteral("偏移：请单击要偏移的图元"));
            return;
        }
        if (const cad::Entity* e = m_document->find(hitId)) {
            m_work.clear();
            m_work.append(*e);
            m_workIds.clear();
            m_workIds.append(hitId);
            m_phase = 1;
            setPrompt(QStringLiteral("偏移：输入距离 D=*（正=左，负=右）或直接点选偏移侧"));
        }
    } else if (m_phase == 1) {
        // 点选偏移侧：距离 = 点到图元最近边的垂直距离，符号由所在侧决定
        if (m_work.isEmpty())
            return;
        const cad::Entity& src = m_work.first();
        double dist = m_lastOffset;
        bool left = true;
        if (src.type == cad::EntityType::Line && src.points.size() >= 2) {
            const cad::Vec2 a = src.points[0], b = src.points[1];
            const cad::Vec2 ab = b - a;
            const double len = cad::length(ab);
            if (len > cad::kEps) {
                const cad::Vec2 n = cad::perp(ab) * (1.0 / len);   // 单位法线（左）
                const double signed0 = cad::dot(world - a, n);
                dist = std::abs(signed0);
                left = signed0 >= 0.0;
            }
        } else if (src.type == cad::EntityType::Circle || src.type == cad::EntityType::Arc
                   || src.type == cad::EntityType::Concentric) {
            const cad::Vec2 c = src.points.front();
            const double d = cad::distance(world, c);
            dist = std::abs(d - src.radius);
            left = d >= src.radius;
        }
        if (dist < cad::kEps)
            return;
        m_lastOffset = dist;
        cad::Entity out;
        if (cad::offsetEntity(src, dist, left, out)) {
            out.id = 0;
            commitAdd({out}, QStringLiteral("偏移"));
        } else {
            setPrompt(QStringLiteral("偏移：该图元类型暂不支持偏移"));
        }
    }
}

void CadEditController::opTrimExtendPick(const cad::Vec2& world, qint64 hitId)
{
    Q_UNUSED(world)
    if (m_phase == 0) {                          // 选边界
        if (hitId == 0 || !m_document) {
            setPrompt(QStringLiteral("请单击作为边界的图元"));
            return;
        }
        m_boundaryId = hitId;
        m_phase = 1;
        setPrompt(m_tool == EditTool::Trim
                      ? QStringLiteral("修剪：拾取要修剪的图元（点选保留侧的反向）")
                      : QStringLiteral("延伸：拾取要延伸的图元"));
    } else if (m_phase == 1) {                   // 选目标
        if (hitId == 0 || !m_document)
            return;
        const cad::Entity* boundary = m_document->find(m_boundaryId);
        const cad::Entity* target = m_document->find(hitId);
        if (!boundary || !target)
            return;
        cad::Entity result;
        bool ok = false;
        if (m_tool == EditTool::Trim)
            ok = cad::trimLineAt(*target, *boundary, m_hoverValid ? m_hover : world, result);
        else
            ok = cad::extendLineTo(*target, *boundary, result);
        if (!ok) {
            setPrompt(m_tool == EditTool::Trim
                          ? QStringLiteral("修剪：未找到交点（仅支持直线 / 圆 / 圆弧）")
                          : QStringLiteral("延伸：未找到交点（仅支持直线 / 圆 / 圆弧）"));
            return;
        }
        // before/after 仅修改目标图元
        QVector<cad::Entity> before, after;
        before.append(*target);
        after.append(result);
        m_undo->push(new cadcmd::ModifyEntities(m_document, before, after,
                                               m_tool == EditTool::Trim
                                                   ? QStringLiteral("修剪")
                                                   : QStringLiteral("延伸")));
        m_document->clearSelection();
        endCommand();
    }
}

void CadEditController::opArrowPick(qint64 hitId)
{
    QVector<qint64> ids;
    if (!m_document->selection().isEmpty()) {
        for (qint64 id : m_document->selection())
            ids.append(id);
    } else if (hitId != 0) {
        ids.append(hitId);
    } else {
        setPrompt(QStringLiteral("箭头：请单击一条直线，或先框选多条直线"));
        return;
    }
    QVector<cad::Entity> created;
    for (qint64 id : ids) {
        const cad::Entity* e = m_document->find(id);
        if (!e || e->type != cad::EntityType::Line || e->points.size() < 2)
            continue;
        const cad::Vec2 tip = e->points.back();
        const cad::Vec2 back = e->points.front();
        const cad::Vec2 dir = cad::normalized(tip - back);
        const double len = std::max(8.0, std::min(20.0, cad::distance(tip, back) * 0.25));
        const cad::Vec2 perp = cad::perp(dir);
        cad::Entity arrow;
        arrow.type = cad::EntityType::Curve;
        arrow.id = 0;
        arrow.color = e->color;
        arrow.lineType = e->lineType;
        arrow.lineWidth = e->lineWidth;
        arrow.points = {tip - dir * len + perp * len * 0.5, tip,
                        tip - dir * len - perp * len * 0.5};
        created.append(arrow);
    }
    if (created.isEmpty()) {
        setPrompt(QStringLiteral("箭头：所选图元不是直线，无法添加箭头"));
        return;
    }
    commitAdd(created, QStringLiteral("箭头"));
}

void CadEditController::opTextPick(qint64 hitId)
{
    qint64 id = 0;
    if (!m_document->selection().isEmpty()) {
        for (qint64 sid : m_document->selection()) {
            if (const cad::Entity* e = m_document->find(sid);
                e && e->type == cad::EntityType::Text) {
                id = sid;
                break;
            }
        }
    } else {
        id = hitId;
    }
    if (id == 0) {
        setPrompt(QStringLiteral("文本设置：请单击一个文本图元"));
        return;
    }
    if (const cad::Entity* e = m_document->find(id); !e || e->type != cad::EntityType::Text) {
        setPrompt(QStringLiteral("文本设置：所选图元不是文本"));
        return;
    }
    m_targetId = id;
    m_phase = 1;
    setPrompt(QStringLiteral("文本设置：输入 H=字高 或 T=文字内容，如 H=10 / T=新标注"));
}

// ---------------------------------------------------------------------------
// 悬停与键盘
// ---------------------------------------------------------------------------
void CadEditController::onHover(const cad::Vec2& world)
{
    m_hover = world;
    m_hoverValid = true;
    if (isActive())
        m_canvas->update();
}

cad::Vec2 CadEditController::snap(const cad::Vec2& point) const
{
    if (!m_canvas || m_canvas->delta() <= 0.0)
        return point;
    const double d = m_canvas->delta();
    return {std::round(point.x / d) * d, std::round(point.z / d) * d};
}

bool CadEditController::handleInput(const QString& input)
{
    if (!isActive())
        return false;
    const QString text = input.trimmed();
    if (text.isEmpty())
        return false;

    // 选择阶段不接受数值
    if (m_phase == -1)
        return false;

    // 文本设置：H= 或 T=
    if (m_tool == EditTool::TextSetting && m_phase == 1) {
        if (text.startsWith(QStringLiteral("H=")) || text.startsWith(QStringLiteral("h="))) {
            bool ok = false;
            const double h = text.mid(2).toDouble(&ok);
            if (ok && h > 0.0) {
                applyTextSetting(h, {});
                return true;
            }
        } else if (text.startsWith(QStringLiteral("T=")) || text.startsWith(QStringLiteral("t="))) {
            applyTextSetting(0.0, text.mid(2).trimmed());
            return true;
        } else {
            applyTextSetting(0.0, text);          // 直接当作内容
            return true;
        }
        return false;
    }

    // 偏移：D= 距离
    if (m_tool == EditTool::Offset && m_phase == 1) {
        if (text.startsWith(QStringLiteral("D=")) || text.startsWith(QStringLiteral("d="))) {
            bool ok = false;
            const double v = text.mid(2).toDouble(&ok);
            if (ok && std::abs(v) > cad::kEps && !m_work.isEmpty()) {
                m_lastOffset = std::abs(v);
                const bool left = v >= 0.0;
                cad::Entity out;
                if (cad::offsetEntity(m_work.first(), std::abs(v), left, out)) {
                    out.id = 0;
                    commitAdd({out}, QStringLiteral("偏移"));
                    return true;
                }
            }
        }
        return false;
    }

    // 旋转 / 缩放 终值：A= 角度 或 S= 比例
    if (m_tool == EditTool::Rotate && m_phase == 3) {
        if (text.startsWith(QStringLiteral("A=")) || text.startsWith(QStringLiteral("a="))) {
            bool ok = false;
            const double deg = text.mid(2).toDouble(&ok);
            if (ok) {
                const cad::Vec2 base = m_points[0];
                const cad::Vec2 ref = m_points[1];
                const double a0 = cad::angleOf(ref - base);
                QVector<cad::Entity> after = snapshot();
                for (cad::Entity& e : after)
                    cad::transformRotate(e, base, deg - a0);
                m_lastAngle = deg - a0;
                commitModify(after, QStringLiteral("旋转"));
                return true;
            }
        }
        return false;
    }
    if (m_tool == EditTool::Scale && m_phase == 3) {
        if (text.startsWith(QStringLiteral("S=")) || text.startsWith(QStringLiteral("s="))) {
            bool ok = false;
            const double f = text.mid(2).toDouble(&ok);
            if (ok && f > cad::kEps) {
                QVector<cad::Entity> after = snapshot();
                for (cad::Entity& e : after)
                    cad::transformScale(e, m_points[0], f);
                m_lastScale = f;
                commitModify(after, QStringLiteral("缩放"));
                return true;
            }
        }
        return false;
    }

    return false;
}

void CadEditController::applyTextSetting(double height, const QString& content)
{
    if (m_targetId == 0)
        return;
    const cad::Entity* src = m_document->find(m_targetId);
    if (!src)
        return;
    cad::Entity after = *src;
    if (height > 0.0) {
        after.textHeight = height;
        m_lastTextH = height;
    }
    if (!content.isEmpty())
        after.text = content.toStdString();
    m_undo->push(new cadcmd::ModifyEntities(m_document,
                                           QVector<cad::Entity>{*src},
                                           QVector<cad::Entity>{after},
                                           QStringLiteral("文本设置")));
    m_document->clearSelection();
    endCommand();
}

// ---------------------------------------------------------------------------
// 预览
// ---------------------------------------------------------------------------
QVector<cad::Entity> CadEditController::previewEntities() const
{
    if (!m_hoverValid)
        return {};
    QVector<cad::Entity> result;
    const cad::Vec2 hover = snap(m_hover);

    switch (m_tool) {
    case EditTool::Move:
        if (m_phase == 2) {
            const cad::Vec2 d = hover - m_points[0];
            result = snapshot();
            for (cad::Entity& e : result)
                cad::transformTranslate(e, d);
        }
        break;
    case EditTool::Rotate:
        if (m_phase == 2 || m_phase == 3) {
            const double a0 = cad::angleOf(m_points[1] - m_points[0]);
            const double a1 = cad::angleOf(hover - m_points[0]);
            result = snapshot();
            for (cad::Entity& e : result)
                cad::transformRotate(e, m_points[0], a1 - a0);
        }
        break;
    case EditTool::Scale:
        if (m_phase == 2 || m_phase == 3) {
            const double r0 = cad::distance(m_points[0], m_points[1]);
            const double r1 = cad::distance(m_points[0], hover);
            const double factor = (r0 > cad::kEps) ? r1 / r0 : 1.0;
            result = snapshot();
            for (cad::Entity& e : result)
                cad::transformScale(e, m_points[0], factor);
        }
        break;
    case EditTool::Mirror:
        if (m_phase == 1) {                       // 预览镜像线
            cad::Entity line;
            line.type = cad::EntityType::Line;
            line.points = {m_points[0], hover};
            result.append(line);
        } else if (m_phase == 2) {
            for (const cad::Entity& src : m_work) {
                cad::Entity mirrored = src;
                cad::transformMirror(mirrored, m_points[0], hover);
                result.append(mirrored);
            }
        }
        break;
    case EditTool::Offset:
        if (m_phase == 1 && !m_work.isEmpty()) {
            const cad::Entity& src = m_work.first();
            const cad::Vec2 world = hover;
            double dist = m_lastOffset;
            bool left = true;
            if (src.type == cad::EntityType::Line && src.points.size() >= 2) {
                const cad::Vec2 a = src.points[0], b = src.points[1];
                const cad::Vec2 ab = b - a;
                const double len = cad::length(ab);
                if (len > cad::kEps) {
                    const cad::Vec2 n = cad::perp(ab) * (1.0 / len);
                    const double s0 = cad::dot(world - a, n);
                    dist = std::max(cad::kEps, std::abs(s0));
                    left = s0 >= 0.0;
                }
            } else if (src.type == cad::EntityType::Circle || src.type == cad::EntityType::Arc
                       || src.type == cad::EntityType::Concentric) {
                const double d = cad::distance(world, src.points.front());
                dist = std::max(cad::kEps, std::abs(d - src.radius));
                left = d >= src.radius;
            }
            cad::Entity out;
            if (cad::offsetEntity(src, dist, left, out))
                result.append(out);
        }
        break;
    case EditTool::Dimension:
        if (m_phase == 1) {
            cad::Entity dim;
            dim.type = cad::EntityType::Dimension;
            dim.points = {m_points[0], hover};
            dim.textHeight = m_lastTextH;
            dim.text = QStringLiteral("%1").arg(cad::distance(m_points[0], hover), 0, 'f', 2)
                           .toStdString();
            result.append(dim);
        }
        break;
    default:
        break;
    }
    return result;
}

void CadEditController::drawArrowHead(QPainter& painter, const cad::Vec2& tip,
                                     const cad::Vec2& back, const CadViewTransform& view,
                                     const QSizeF& vp, const QPen& pen)
{
    const cad::Vec2 dir = cad::normalized(tip - back);
    const double len = 10.0;
    const cad::Vec2 perp = cad::perp(dir);
    const QPointF t = view.toScreen(tip, vp);
    const QPointF p1 = view.toScreen(tip - dir * len + perp * len * 0.5, vp);
    const QPointF p2 = view.toScreen(tip - dir * len - perp * len * 0.5, vp);
    painter.setPen(pen);
    painter.drawLine(t, p1);
    painter.drawLine(t, p2);
}

void CadEditController::drawPreview(QPainter& painter, const CadViewTransform& view,
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
            // 标注：端点短划线 + 长度文字
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
