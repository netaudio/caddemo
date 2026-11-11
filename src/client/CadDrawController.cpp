#include "CadDrawController.h"

#include "CadCanvas.h"
#include "CadCommands.h"
#include "CadDocument.h"
#include "CadEntityPainter.h"
#include "CadInputParser.h"

#include <QFont>
#include <QPainter>
#include <QPen>
#include <QRectF>
#include <QUndoStack>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr double kTextHeight = 14.0;    // 世界单位（scale=1 时约 14 px）
const QColor kPreviewColor("#2b6cd9");

struct ToolInfo {
    DrawTool tool;
    const char* label;
    const char* alias;      // 以 '|' 分隔的键盘别名
};

const ToolInfo kTools[] = {
    {DrawTool::Point,         "点",                 "POINT|PO"},
    {DrawTool::Line,          "直线",               "LINE|L"},
    {DrawTool::Circle,        "圆",                 "CIRCLE|C"},
    {DrawTool::Arc3Point,     "圆弧(三点)",         "ARC3|3P"},
    {DrawTool::ArcEndRadius,  "圆弧(端点+半径)",    "ARCR|ARC-R"},
    {DrawTool::ArcEndAngle,   "圆弧(端点+角度)",    "ARCA|ARC-A"},
    {DrawTool::Rectangle,     "矩形",               "RECTANGLE|REC|RECT"},
    {DrawTool::Curve,         "曲线",               "SPLINE|SPL|CURVE"},
    {DrawTool::Text,          "文本",               "TEXT|T"},
    {DrawTool::Concentric,    "同心圆",             "CONCENTRIC|CC"},
    {DrawTool::Divide,        "等分点",             "DIVIDE|DIV"},
    {DrawTool::CenterPoint,   "圆心点",             "CENTERPOINT|CP"},
    {DrawTool::Chamfer,       "倒角",               "CHAMFER|CHA"},
    {DrawTool::Fillet,        "圆角",               "FILLET|F"},
};

bool matchesAlias(const QString& input, const char* aliasList)
{
    const QStringList aliases = QString::fromLatin1(aliasList).split(QLatin1Char('|'));
    for (const QString& alias : aliases) {
        if (input.compare(alias, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

}  // namespace

// ---------------------------------------------------------------------------
DrawTool toolFromCommand(const QString& command)
{
    const QString name = command.trimmed();
    for (const ToolInfo& info : kTools) {
        if (name == QString::fromUtf8(info.label) || matchesAlias(name, info.alias))
            return info.tool;
    }
    return DrawTool::None;
}

QString toolLabel(DrawTool tool)
{
    for (const ToolInfo& info : kTools) {
        if (info.tool == tool)
            return QString::fromUtf8(info.label);
    }
    return {};
}

// ---------------------------------------------------------------------------
CadDrawController::CadDrawController(CadDocument* document, CadCanvas* canvas,
                                     QUndoStack* undoStack, QObject* parent)
    : QObject(parent)
    , m_document(document)
    , m_canvas(canvas)
    , m_undo(undoStack)
{
}

bool CadDrawController::startTool(DrawTool tool)
{
    if (tool == DrawTool::None) {
        cancel();
        return false;
    }
    m_tool = tool;
    m_label = toolLabel(tool);
    resetStep();
    emit toolChanged();
    emitPrompt(QStringLiteral("命令开始"));
    if (m_canvas) {
        m_canvas->setInteraction(CadCanvas::Interaction::Select);
        m_canvas->setCursor(Qt::CrossCursor);
        m_canvas->setFocus();
        m_canvas->update();
    }
    return true;
}

bool CadDrawController::startCommand(const QString& command)
{
    const DrawTool tool = toolFromCommand(command);
    if (tool == DrawTool::None)
        return false;
    return startTool(tool);
}

void CadDrawController::cancel()
{
    const bool wasActive = isActive();
    m_tool = DrawTool::None;
    m_label.clear();
    resetStep();
    emit toolChanged();
    if (m_canvas) {
        // 恢复与当前视图交互模式一致的光标
        m_canvas->setInteraction(m_canvas->interaction());
        m_canvas->update();
    }
    if (wasActive)
        emit statusMessage(QStringLiteral("命令已结束（右键 / Esc）"));
}

void CadDrawController::resetStep()
{
    m_points.clear();
    m_picked.clear();
    m_number = 0.0;
    m_hasNumber = false;
    m_text.clear();
    m_hoverValid = false;
}

cad::Vec2 CadDrawController::snap(const cad::Vec2& point) const
{
    const double delta = m_canvas ? m_canvas->delta() : 0.0;
    if (delta <= 1e-9 || !std::isfinite(delta))
        return point;
    const double x = std::round(point.x / delta) * delta;
    const double z = std::round(point.z / delta) * delta;
    return {std::isfinite(x) ? x : point.x, std::isfinite(z) ? z : point.z};
}

cad::Vec2 CadDrawController::referencePoint() const
{
    if (!m_points.empty())
        return m_points.back();
    if (!m_picked.isEmpty()) {
        if (const cad::Entity* entity = m_document->find(m_picked.first()))
            if (!entity->points.empty())
                return entity->points.front();
    }
    return m_hover;
}

// ---------------------------------------------------------------------------
// 步骤判定
// ---------------------------------------------------------------------------
CadDrawController::Step CadDrawController::currentStep() const
{
    switch (m_tool) {
    case DrawTool::Point:
    case DrawTool::Line:
    case DrawTool::Rectangle:
    case DrawTool::Curve:
    case DrawTool::Arc3Point:
        return Step::Point;
    case DrawTool::Circle:
    case DrawTool::Concentric:
        return m_points.empty() ? Step::Point : Step::Number;
    case DrawTool::ArcEndRadius:
    case DrawTool::ArcEndAngle:
        return m_points.size() < 2 ? Step::Point : Step::Number;
    case DrawTool::Text:
        return m_points.empty() ? Step::Point : Step::Text;
    case DrawTool::Divide:
        return m_picked.isEmpty() ? Step::Pick : Step::Number;
    case DrawTool::CenterPoint:
        return Step::Pick;
    case DrawTool::Chamfer:
    case DrawTool::Fillet:
        return m_picked.size() < 2 ? Step::Pick : Step::Number;
    case DrawTool::None:
        break;
    }
    return Step::Point;
}

bool CadDrawController::wantsPick() const   { return currentStep() == Step::Pick; }
bool CadDrawController::wantsNumber() const { return currentStep() == Step::Number; }
bool CadDrawController::wantsText() const   { return currentStep() == Step::Text; }

bool CadDrawController::pickAccepted(cad::EntityType type) const
{
    switch (m_tool) {
    case DrawTool::Divide:
        return type == cad::EntityType::Line || type == cad::EntityType::Curve;
    case DrawTool::CenterPoint:
        return type == cad::EntityType::Circle || type == cad::EntityType::Arc
            || type == cad::EntityType::Concentric;
    case DrawTool::Chamfer:
    case DrawTool::Fillet:
        return type == cad::EntityType::Line;
    default:
        return true;
    }
}

void CadDrawController::setPrompt(const QString& text)
{
    m_prompt = text;
    emit statusMessage(text);
}

void CadDrawController::emitPrompt(const QString& prefix)
{
    QString body;
    switch (currentStep()) {
    case Step::Pick:
        switch (m_tool) {
        case DrawTool::Divide:
            body = QStringLiteral("请单击选择要等分的直线或曲线"); break;
        case DrawTool::CenterPoint:
            body = QStringLiteral("请单击选择圆 / 圆弧 / 同心圆"); break;
        case DrawTool::Chamfer:
        case DrawTool::Fillet:
            body = QStringLiteral("请选择第 %1 条直线").arg(m_picked.size() + 1); break;
        default:
            body = QStringLiteral("请单击选择对象"); break;
        }
        break;
    case Step::Number:
        switch (m_tool) {
        case DrawTool::Circle:
        case DrawTool::Concentric:
            body = QStringLiteral("输入半径（R=数值）或单击确定半径"); break;
        case DrawTool::ArcEndRadius:
            body = QStringLiteral("输入半径 R（负数换方向），回车用默认值"); break;
        case DrawTool::ArcEndAngle:
            body = QStringLiteral("输入圆心角 A（度，负数换方向），回车用默认值"); break;
        case DrawTool::Divide:
            body = QStringLiteral("输入等分数 N，回车默认 %1 段").arg(static_cast<int>(m_lastCount)); break;
        case DrawTool::Chamfer:
            body = QStringLiteral("输入倒角距离 D，回车默认 %1").arg(m_lastDistance, 0, 'f', 1); break;
        case DrawTool::Fillet:
            body = QStringLiteral("输入圆角半径 R，回车默认 %1").arg(m_lastFillet, 0, 'f', 1); break;
        default:
            body = QStringLiteral("请输入数值"); break;
        }
        break;
    case Step::Text:
        body = QStringLiteral("在命令栏输入文字（T=文字 或直接输入），回车确认"); break;
    case Step::Point:
        switch (m_tool) {
        case DrawTool::Point:
            body = QStringLiteral("单击指定点位置（可键盘输入 x=,y=）"); break;
        case DrawTool::Line:
            body = m_points.empty()
                       ? QStringLiteral("指定直线起点（可用 x=,y= / L=,A=）")
                       : QStringLiteral("指定直线终点（链式绘制，右键结束）"); break;
        case DrawTool::Circle:
        case DrawTool::Concentric:
            body = QStringLiteral("指定圆心"); break;
        case DrawTool::Rectangle:
            body = m_points.empty() ? QStringLiteral("指定矩形第一个角点")
                                    : QStringLiteral("指定矩形对角点"); break;
        case DrawTool::Arc3Point:
            body = QStringLiteral("指定圆弧第 %1 点").arg(m_points.size() + 1); break;
        case DrawTool::ArcEndRadius:
        case DrawTool::ArcEndAngle:
            body = m_points.empty() ? QStringLiteral("指定圆弧起点")
                                    : QStringLiteral("指定圆弧终点"); break;
        case DrawTool::Curve:
            body = QStringLiteral("指定曲线第 %1 个控制点（回车 / 右键结束，至少 2 点）")
                       .arg(m_points.size() + 1); break;
        case DrawTool::Text:
            body = QStringLiteral("指定文字插入点"); break;
        default:
            body = QStringLiteral("指定点"); break;
        }
        break;
    }
    const QString head = prefix.isEmpty() ? m_label : QStringLiteral("%1 · %2").arg(m_label, prefix);
    setPrompt(QStringLiteral("%1：%2  |  右键 / Esc 结束命令").arg(head, body));
}

double CadDrawController::defaultNumber() const
{
    switch (m_tool) {
    case DrawTool::Circle:
    case DrawTool::Concentric:
    case DrawTool::ArcEndRadius:  return m_lastRadius;
    case DrawTool::ArcEndAngle:   return m_lastAngle;
    case DrawTool::Divide:        return m_lastCount;
    case DrawTool::Chamfer:       return m_lastDistance;
    case DrawTool::Fillet:        return m_lastFillet;
    default:                      return 1.0;
    }
}

void CadDrawController::applyNumber(double value)
{
    if (!std::isfinite(value))
        value = defaultNumber();
    m_number = value;
    m_hasNumber = true;
    switch (m_tool) {
    case DrawTool::Circle:
    case DrawTool::Concentric:
    case DrawTool::ArcEndRadius:  m_lastRadius = std::abs(value); break;
    case DrawTool::ArcEndAngle:   m_lastAngle = value; break;
    case DrawTool::Divide:        m_lastCount = value; break;
    case DrawTool::Chamfer:       m_lastDistance = value; break;
    case DrawTool::Fillet:        m_lastFillet = value; break;
    default: break;
    }
}

// ---------------------------------------------------------------------------
// 鼠标 / 键盘输入
// ---------------------------------------------------------------------------
void CadDrawController::onHover(const cad::Vec2& world)
{
    if (!isActive())
        return;
    m_hover = snap(world);
    m_hoverValid = true;
    if (m_canvas)
        m_canvas->update();
}

void CadDrawController::onPoint(const cad::Vec2& world, qint64 hitId)
{
    if (!isActive())
        return;
    const cad::Vec2 point = snap(world);
    m_hover = point;
    m_hoverValid = true;

    switch (currentStep()) {
    case Step::Pick: {
        if (hitId == 0) {
            setPrompt(QStringLiteral("%1：未拾取到对象，请点选图元本身").arg(m_label));
            return;
        }
        const cad::Entity* entity = m_document->find(hitId);
        if (!entity || !pickAccepted(entity->type)) {
            setPrompt(QStringLiteral("%1：所选对象类型不适用，请重新选择").arg(m_label));
            return;
        }
        if (m_picked.contains(hitId)) {
            setPrompt(QStringLiteral("%1：该对象已选过，请选择另一个").arg(m_label));
            return;
        }
        m_picked.append(hitId);
        break;
    }
    case Step::Number:
        applyNumber(cad::distance(point, referencePoint()));
        break;
    case Step::Text:
        setPrompt(QStringLiteral("%1：请在命令栏输入文字后回车").arg(m_label));
        return;
    case Step::Point:
        m_points.push_back(point);
        break;
    }

    if (m_canvas)
        m_canvas->update();
    advance();
}

bool CadDrawController::handleInput(const QString& input)
{
    const QString text = input.trimmed();
    if (!isActive()) {
        if (text.isEmpty())
            return false;
        return startCommand(text);
    }
    if (text.isEmpty()) {
        switch (currentStep()) {
        case Step::Number: applyNumber(defaultNumber()); advance(); break;
        case Step::Text:   m_text = m_lastText;          advance(); break;
        default:           finishCurrent();                        break;
        }
        return true;
    }
    if (text.compare(QStringLiteral("ESC"), Qt::CaseInsensitive) == 0) {
        cancel();
        return true;
    }

    const cadinput::Result parsed = cadinput::parse(text, m_hover, referencePoint());
    switch (currentStep()) {
    case Step::Point:
        if (parsed.kind == cadinput::Kind::Point) {
            m_points.push_back(parsed.point);
            if (m_canvas)
                m_canvas->update();
            advance();
            emit statusMessage(QStringLiteral("%1 · %2  |  %3")
                                   .arg(m_label, parsed.description, m_prompt));
        } else {
            setPrompt(QStringLiteral("%1：此处需要点坐标（x=,y= / dx=,dy= / L=,A=）").arg(m_label));
        }
        return true;
    case Step::Number:
        if (parsed.kind == cadinput::Kind::Number) {
            applyNumber(parsed.number);
            advance();
            emit statusMessage(QStringLiteral("%1 · %2  |  %3")
                                   .arg(m_label, parsed.description, m_prompt));
        } else {
            setPrompt(QStringLiteral("%1：此处需要数值参数，如 R=50 / N=5 / D=8").arg(m_label));
        }
        return true;
    case Step::Text:
        if (parsed.kind == cadinput::Kind::Text) {
            m_text = parsed.text;
            m_lastText = parsed.text;
        } else {
            m_text = m_lastText;
        }
        advance();
        return true;
    case Step::Pick:
        setPrompt(QStringLiteral("%1：此步骤需要在绘图区单击选择对象（输入 %2 被忽略）")
                      .arg(m_label, text));
        return true;
    }
    return true;
}

void CadDrawController::onRightClick()
{
    if (!isActive())
        return;
    if (m_tool == DrawTool::Curve && m_points.size() >= 2) {
        commit();
        return;
    }
    cancel();
}

void CadDrawController::finishCurrent()
{
    if (!isActive())
        return;
    if (m_tool == DrawTool::Curve) {
        if (m_points.size() >= 2)
            commit();
        else
            cancel();
        return;
    }
    switch (currentStep()) {
    case Step::Number: applyNumber(defaultNumber()); advance(); break;
    case Step::Text:   m_text = m_lastText;          advance(); break;
    default:           cancel();                               break;
    }
}

// ---------------------------------------------------------------------------
// 提交
// ---------------------------------------------------------------------------
void CadDrawController::advance()
{
    bool ready = false;
    switch (m_tool) {
    case DrawTool::Point:         ready = m_points.size() >= 1; break;
    case DrawTool::Line:          ready = m_points.size() >= 2; break;
    case DrawTool::Circle:
    case DrawTool::Concentric:    ready = m_points.size() >= 1 && m_hasNumber; break;
    case DrawTool::Arc3Point:     ready = m_points.size() >= 3; break;
    case DrawTool::ArcEndRadius:
    case DrawTool::ArcEndAngle:   ready = m_points.size() >= 2 && m_hasNumber; break;
    case DrawTool::Rectangle:     ready = m_points.size() >= 2; break;
    case DrawTool::Curve:         ready = false; break;   // 由回车 / 右键结束
    case DrawTool::Text:          ready = m_points.size() >= 1 && !m_text.isEmpty(); break;
    case DrawTool::Divide:        ready = m_picked.size() >= 1 && m_hasNumber; break;
    case DrawTool::CenterPoint:   ready = m_picked.size() >= 1; break;
    case DrawTool::Chamfer:
    case DrawTool::Fillet:        ready = m_picked.size() >= 2 && m_hasNumber; break;
    case DrawTool::None:          break;
    }
    if (ready)
        commit();
    else
        emitPrompt();
}

cad::Entity CadDrawController::styled(cad::EntityType type) const
{
    cad::Entity entity;
    entity.type = type;
    entity.color = m_document->currentColor();
    entity.lineType = m_document->currentLineType();
    entity.lineWidth = m_document->currentLineWidth();
    entity.textHeight = kTextHeight;
    return entity;
}

void CadDrawController::pushEntities(const QVector<cad::Entity>& entities)
{
    if (entities.isEmpty())
        return;
    cadcmd::addEntities(m_document, m_undo, entities, m_label);
}

void CadDrawController::commit()
{
    if (!isActive())
        return;

    QVector<cad::Entity> created;
    const auto arcEntity = [this](const cad::ArcData& data) {
        cad::Entity entity = styled(cad::EntityType::Arc);
        entity.points = {data.center};
        entity.radius = data.radius;
        entity.startAngle = data.startAngle;
        entity.endAngle = data.startAngle + data.span;
        return entity;
    };

    bool oneShot = false;
    switch (m_tool) {
    case DrawTool::Point: {
        cad::Entity entity = styled(cad::EntityType::Point);
        entity.points = {m_points[0]};
        created.append(entity);
        break;
    }
    case DrawTool::Line: {
        cad::Entity entity = styled(cad::EntityType::Line);
        entity.points = {m_points[0], m_points[1]};
        created.append(entity);
        break;
    }
    case DrawTool::Rectangle: {
        cad::Entity entity = styled(cad::EntityType::Rectangle);
        entity.points = {m_points[0], m_points[1]};
        created.append(entity);
        break;
    }
    case DrawTool::Circle: {
        cad::Entity entity = styled(cad::EntityType::Circle);
        entity.points = {m_points[0]};
        entity.radius = std::max(1e-6, std::abs(m_number));
        created.append(entity);
        break;
    }
    case DrawTool::Concentric: {
        cad::Entity entity = styled(cad::EntityType::Concentric);
        entity.points = {m_points[0]};
        entity.radius = std::max(1e-6, std::abs(m_number));
        entity.innerRadius = entity.radius * 0.5;
        created.append(entity);
        break;
    }
    case DrawTool::Arc3Point: {
        const cad::ArcData data = cad::arcFrom3Points(m_points[0], m_points[1], m_points[2]);
        if (!data.valid) {
            setPrompt(QStringLiteral("圆弧(三点)：三点共线，无法构造圆弧，请重新输入"));
            resetStep();
            emitPrompt();
            return;
        }
        created.append(arcEntity(data));
        break;
    }
    case DrawTool::ArcEndRadius: {
        const cad::ArcData data = cad::arcFromChordRadius(m_points[0], m_points[1], m_number);
        if (!data.valid) {
            setPrompt(QStringLiteral("圆弧(端点+半径)：参数无效，请重新输入"));
            resetStep();
            emitPrompt();
            return;
        }
        created.append(arcEntity(data));
        break;
    }
    case DrawTool::ArcEndAngle: {
        const cad::ArcData data = cad::arcFromChordAngle(m_points[0], m_points[1], m_number);
        if (!data.valid) {
            setPrompt(QStringLiteral("圆弧(端点+角度)：参数无效，请重新输入"));
            resetStep();
            emitPrompt();
            return;
        }
        created.append(arcEntity(data));
        break;
    }
    case DrawTool::Curve: {
        cad::Entity entity = styled(cad::EntityType::Curve);
        entity.points = cad::smoothCatmullRom(m_points, 10);
        created.append(entity);
        oneShot = true;
        break;
    }
    case DrawTool::Text: {
        cad::Entity entity = styled(cad::EntityType::Text);
        entity.points = {m_points[0]};
        entity.text = m_text.toStdString();
        created.append(entity);
        break;
    }
    case DrawTool::Divide: {
        const cad::Entity* source = m_document->find(m_picked[0]);
        if (!source || source->points.size() < 2)
            break;
        const int segments = std::clamp(static_cast<int>(std::llround(m_number)), 2, 200);
        const double total = cad::polylineLength(source->points);
        if (total < cad::kEps)
            break;
        for (int i = 1; i < segments; ++i) {
            cad::Entity point = styled(cad::EntityType::Point);
            point.points = {cad::pointAtLength(source->points, total * i / segments)};
            created.append(point);
        }
        oneShot = true;
        break;
    }
    case DrawTool::CenterPoint: {
        const cad::Entity* source = m_document->find(m_picked[0]);
        if (!source || source->points.empty())
            break;
        cad::Entity point = styled(cad::EntityType::Point);
        point.points = {source->points.front()};
        created.append(point);
        oneShot = true;
        break;
    }
    case DrawTool::Chamfer:
    case DrawTool::Fillet: {
        const cad::Entity* a = m_document->find(m_picked[0]);
        const cad::Entity* b = m_document->find(m_picked[1]);
        if (!a || !b)
            break;
        const bool fillet = m_tool == DrawTool::Fillet;
        const cad::CornerResult result = fillet
            ? cad::filletLines(*a, *b, std::abs(m_number))
            : cad::chamferLines(*a, *b, std::abs(m_number), std::abs(m_number));
        if (!result.valid) {
            setPrompt(QStringLiteral("%1：两直线平行 / 共线或参数无效，命令取消").arg(m_label));
            cancel();
            return;
        }
        QVector<cad::Entity> before = {*a, *b};
        QVector<cad::Entity> after = {result.lineA, result.lineB};
        if (m_undo) {
            m_undo->beginMacro(m_label);
            m_undo->push(new cadcmd::ModifyEntities(m_document, before, after,
                                                    QStringLiteral("修剪两条直线")));
            m_undo->push(new cadcmd::AddEntity(m_document, result.corner,
                                               fillet ? QStringLiteral("圆角弧")
                                                      : QStringLiteral("倒角连接线")));
            m_undo->endMacro();
        }
        created.append(result.corner);
        oneShot = true;
        break;
    }
    case DrawTool::None:
        return;
    }

    // 倒角 / 圆角已通过宏提交，避免重复插入连接图元
    if (m_tool != DrawTool::Chamfer && m_tool != DrawTool::Fillet)
        pushEntities(created);

    const QString summary = QStringLiteral("%1：已生成 %2 个图元").arg(m_label).arg(created.size());
    const DrawTool finishedTool = m_tool;
    const cad::Vec2 chainStart = (finishedTool == DrawTool::Line && !m_points.empty())
                                     ? m_points.back() : cad::Vec2{};
    resetStep();

    if (oneShot) {
        cancel();
        emit statusMessage(QStringLiteral("%1  —  命令完成").arg(summary));
        return;
    }
    if (finishedTool == DrawTool::Line)
        m_points.push_back(chainStart);   // 链式连续绘制
    emitPrompt();
    emit statusMessage(QStringLiteral("%1  |  %2").arg(summary, m_prompt));
}

// ---------------------------------------------------------------------------
// 预览
// ---------------------------------------------------------------------------
void CadDrawController::drawPickMarks(QPainter& painter, const CadViewTransform& view,
                                      const QSizeF& viewport)
{
    if (m_picked.isEmpty() || !m_document)
        return;
    QPen pen(kPreviewColor, 1, Qt::DotLine);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    for (qint64 id : m_picked) {
        const cad::Entity* entity = m_document->find(id);
        if (!entity)
            continue;
        const cad::Bounds bounds = cad::entityBounds(*entity);
        if (!bounds.valid)
            continue;
        const QPointF p0 = view.toScreen({bounds.minX, bounds.maxZ}, viewport);
        const QPointF p1 = view.toScreen({bounds.maxX, bounds.minZ}, viewport);
        painter.drawRect(QRectF(p0, p1).normalized().adjusted(-4, -4, 4, 4));
    }
}

void CadDrawController::drawPreview(QPainter& painter, const CadViewTransform& view,
                                    const QSizeF& viewport)
{
    if (!isActive())
        return;

    const cad::Vec2 hover = m_hoverValid ? m_hover : referencePoint();

    // 十字光标
    if (m_hoverValid) {
        const QPointF h = view.toScreen(hover, viewport);
        QPen crossPen(kPreviewColor, 1, Qt::DashLine);
        crossPen.setCosmetic(true);
        painter.setPen(crossPen);
        painter.drawLine(h.x() - 10, h.y(), h.x() + 10, h.y());
        painter.drawLine(h.x(), h.y() - 10, h.x(), h.y() + 10);
    }

    // 已确认点
    painter.setPen(QPen(kPreviewColor, 1));
    painter.setBrush(QColor("#ffffff"));
    for (const cad::Vec2& point : m_points) {
        const QPointF s = view.toScreen(point, viewport);
        painter.drawRect(QRectF(s.x() - 3, s.y() - 3, 6, 6));
    }
    painter.setBrush(Qt::NoBrush);
    drawPickMarks(painter, view, viewport);

    auto draw = [&](const cad::Entity& entity) {
        QPen pen(kPreviewColor, 1, Qt::DashLine);
        pen.setCosmetic(true);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        cadpainter::drawEntity(painter, entity, view, viewport);
    };
    auto plain = [](cad::EntityType type) {
        cad::Entity entity;
        entity.type = type;
        entity.textHeight = kTextHeight;
        return entity;
    };

    switch (m_tool) {
    case DrawTool::Line:
    case DrawTool::Rectangle:
        if (!m_points.empty()) {
            cad::Entity entity = plain(m_tool == DrawTool::Line
                                           ? cad::EntityType::Line
                                           : cad::EntityType::Rectangle);
            entity.points = {m_points.back(), hover};
            draw(entity);
        }
        break;
    case DrawTool::Circle:
    case DrawTool::Concentric:
        if (!m_points.empty()) {
            cad::Entity entity = plain(m_tool == DrawTool::Circle
                                           ? cad::EntityType::Circle
                                           : cad::EntityType::Concentric);
            entity.points = {m_points.back()};
            const double radius = m_hasNumber ? std::abs(m_number)
                                              : cad::distance(m_points.back(), hover);
            entity.radius = std::max(1e-6, radius);
            entity.innerRadius = entity.radius * 0.5;
            draw(entity);
        }
        break;
    case DrawTool::Arc3Point: {
        if (m_points.size() == 1) {
            cad::Entity line = plain(cad::EntityType::Line);
            line.points = {m_points[0], hover};
            draw(line);
        } else if (m_points.size() == 2) {
            const cad::ArcData data = cad::arcFrom3Points(m_points[0], m_points[1], hover);
            if (data.valid) {
                cad::Entity arc = plain(cad::EntityType::Arc);
                arc.points = {data.center};
                arc.radius = data.radius;
                arc.startAngle = data.startAngle;
                arc.endAngle = data.startAngle + data.span;
                draw(arc);
            }
        }
        break;
    }
    case DrawTool::ArcEndRadius:
    case DrawTool::ArcEndAngle: {
        if (m_points.size() == 1) {
            cad::Entity line = plain(cad::EntityType::Line);
            line.points = {m_points[0], hover};
            draw(line);
        } else if (m_points.size() >= 2) {
            const double value = m_hasNumber ? m_number
                                             : (m_tool == DrawTool::ArcEndRadius
                                                    ? m_lastRadius : m_lastAngle);
            const cad::ArcData data = m_tool == DrawTool::ArcEndRadius
                ? cad::arcFromChordRadius(m_points[0], m_points[1], value)
                : cad::arcFromChordAngle(m_points[0], m_points[1], value);
            if (data.valid) {
                cad::Entity arc = plain(cad::EntityType::Arc);
                arc.points = {data.center};
                arc.radius = data.radius;
                arc.startAngle = data.startAngle;
                arc.endAngle = data.startAngle + data.span;
                draw(arc);
            }
        }
        break;
    }
    case DrawTool::Curve:
        if (!m_points.empty()) {
            cad::Entity entity = plain(cad::EntityType::Curve);
            entity.points = m_points;
            entity.points.push_back(hover);
            if (entity.points.size() >= 3)
                entity.points = cad::smoothCatmullRom(entity.points, 8);
            draw(entity);
        }
        break;
    case DrawTool::Text: {
        const QPointF s = view.toScreen(m_points.empty() ? hover : m_points.back(), viewport);
        QPen pen(kPreviewColor, 1, Qt::DotLine);
        painter.setPen(pen);
        QFont font(QStringLiteral("Microsoft YaHei"));
        font.setPixelSize(std::clamp(static_cast<int>(kTextHeight * view.scale), 8, 96));
        painter.setFont(font);
        painter.drawText(s, m_text.isEmpty() ? m_lastText : m_text);
        break;
    }
    case DrawTool::Point:
    case DrawTool::Divide:
    case DrawTool::CenterPoint:
    case DrawTool::Chamfer:
    case DrawTool::Fillet:
    case DrawTool::None:
        break;
    }

    // 实时尺寸回显
    if (m_hoverValid && !m_points.empty()) {
        const QPointF s = view.toScreen(hover, viewport);
        const double dx = hover.x - m_points.back().x;
        const double dz = hover.z - m_points.back().z;
        painter.setPen(QColor("#6a7d99"));
        QFont font(QStringLiteral("Microsoft YaHei"));
        font.setPixelSize(11);
        painter.setFont(font);
        painter.drawText(s + QPointF(12, -10),
                         QStringLiteral("Δ %1, %2   L %3")
                             .arg(dx, 0, 'f', 2).arg(dz, 0, 'f', 2)
                             .arg(std::hypot(dx, dz), 0, 'f', 2));
    }
}
