#include "CadCanvas.h"

#include "CadDocument.h"
#include "CadDrawController.h"
#include "CadEntityPainter.h"
#include "CadGeometry.h"

#include <QFont>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRectF>
#include <QSet>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {
constexpr double kPickTolerancePx = 7.0;   // 拾取半径（屏幕像素）
constexpr double kZoomStep = 1.25;
}  // namespace

// ---------------------------------------------------------------------------
CadCanvas::CadCanvas(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("cadCanvas"));
    setAttribute(Qt::WA_StyledBackground, true);
    setMouseTracking(true);
    setMinimumSize(400, 300);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);
}

QSizeF CadCanvas::viewport() const
{
    return QSizeF(size());
}

void CadCanvas::setDocument(CadDocument* document)
{
    if (m_document == document)
        return;
    if (m_document)
        disconnect(m_document, nullptr, this, nullptr);
    m_document = document;
    if (m_document) {
        connect(m_document, &CadDocument::changed, this, [this]() { update(); });
        connect(m_document, &CadDocument::selectionChanged, this, [this]() { update(); });
    }
    update();
}

void CadCanvas::setDrawController(CadDrawController* controller)
{
    m_drawController = controller;
    update();
}

void CadCanvas::setGridVisible(bool visible)
{
    if (m_gridVisible == visible)
        return;
    m_gridVisible = visible;
    update();
}

void CadCanvas::setCoordinateSystem(const QString& name)
{
    m_coordinateSystem = name;
    update();
}

void CadCanvas::setInteraction(Interaction mode)
{
    m_interaction = mode;
    m_panning = false;
    m_rubberBand = false;
    switch (mode) {
    case Interaction::Pan:
        setCursor(Qt::OpenHandCursor);
        break;
    case Interaction::ZoomWindow:
        setCursor(Qt::CrossCursor);
        break;
    case Interaction::Select:
        setCursor(Qt::ArrowCursor);
        break;
    }
    update();
}

// ---------------------------------------------------------------------------
// 视图命令
// ---------------------------------------------------------------------------
void CadCanvas::zoomIn()
{
    m_view.zoomAt(QPointF(width() / 2.0, height() / 2.0), kZoomStep, viewport());
    update();
    emit viewChanged(m_view.scale);
}

void CadCanvas::zoomOut()
{
    m_view.zoomAt(QPointF(width() / 2.0, height() / 2.0), 1.0 / kZoomStep, viewport());
    update();
    emit viewChanged(m_view.scale);
}

void CadCanvas::zoomFit()
{
    const cad::Bounds bounds = m_document ? m_document->bounds() : cad::Bounds{};
    if (!bounds.valid) {
        resetView();
        emit statusMessage(QStringLiteral("窗口充满：文档为空，已恢复默认视图"));
        return;
    }
    m_view.fit(bounds, viewport());
    update();
    emit viewChanged(m_view.scale);
    emit statusMessage(QStringLiteral("窗口充满：按图元范围适配视图"));
}

void CadCanvas::resetView()
{
    m_view.scale = 1.0;
    m_view.centerX = 0.0;
    m_view.centerZ = 0.0;
    update();
    emit viewChanged(m_view.scale);
}

void CadCanvas::applyZoomWindow(const QRectF& rect)
{
    const QRectF r = rect.normalized();
    if (r.width() < 8.0 || r.height() < 8.0) {
        setInteraction(Interaction::Select);
        return;
    }
    const QSizeF vp = viewport();
    const cad::Vec2 a = m_view.toWorld(r.topLeft(), vp);
    const cad::Vec2 b = m_view.toWorld(r.bottomRight(), vp);
    cad::Bounds bounds;
    cad::extend(bounds, a);
    cad::extend(bounds, b);
    m_view.fit(bounds, vp, 1.0);
    setInteraction(Interaction::Select);
    update();
    emit viewChanged(m_view.scale);
    emit statusMessage(QStringLiteral("窗口缩放：已放大到框选区域"));
}

// ---------------------------------------------------------------------------
// 渲染（图元绘制统一交给 CadEntityPainter，橡皮筋预览复用同一套逻辑）
// ---------------------------------------------------------------------------
void CadCanvas::drawGrid(QPainter& painter)
{
    if (!m_gridVisible)
        return;

    const QSizeF vp = viewport();
    const double rawStep = 40.0 / m_view.scale;
    const double magnitude = std::pow(10.0, std::floor(std::log10(rawStep)));
    double step = magnitude;
    for (double factor : {1.0, 2.0, 5.0, 10.0}) {
        if (magnitude * factor >= rawStep) {
            step = magnitude * factor;
            break;
        }
    }

    const cad::Vec2 topLeft = m_view.toWorld(QPointF(0, 0), vp);
    const cad::Vec2 bottomRight = m_view.toWorld(QPointF(vp.width(), vp.height()), vp);
    painter.setPen(QPen(QColor("#e3ecf7"), 1));

    int count = 0;
    for (double x = std::floor(topLeft.x / step) * step;
         x <= bottomRight.x && count < 500; x += step, ++count) {
        const double sx = m_view.toScreen({x, 0.0}, vp).x();
        painter.drawLine(sx, 0, sx, height());
    }
    for (double z = std::floor(bottomRight.z / step) * step;
         z <= topLeft.z && count < 1000; z += step, ++count) {
        const double sy = m_view.toScreen({0.0, z}, vp).y();
        painter.drawLine(0, sy, width(), sy);
    }
}

void CadCanvas::drawAxes(QPainter& painter)
{
    const QSizeF vp = viewport();
    const QPointF origin = m_view.toScreen({0.0, 0.0}, vp);
    const QString horizontal = (m_coordinateSystem == QLatin1String("YZ"))
                                   ? QStringLiteral("Y") : QStringLiteral("X");
    const QString vertical = (m_coordinateSystem == QLatin1String("XY"))
                                 ? QStringLiteral("Y") : QStringLiteral("Z");

    // X（或 Y）轴：世界 z = 0 的水平线
    painter.setPen(QPen(QColor("#e2e2e2"), 1));
    painter.drawLine(0, origin.y(), width(), origin.y());
    // Z（或 Y）轴：世界 x = 0 的中轴线
    painter.setPen(QPen(QColor("#d94b3a"), 1));
    painter.drawLine(origin.x(), 0, origin.x(), height());

    QFont axisFont(QStringLiteral("Arial"), 10, QFont::Normal, true);
    painter.setFont(axisFont);
    if (origin.y() >= 0 && origin.y() <= height()) {
        painter.setPen(QColor("#b8b8b8"));
        painter.drawText(width() - 22, origin.y() - 5, horizontal);
    }
    if (origin.x() >= 0 && origin.x() <= width()) {
        painter.setPen(QColor("#d94b3a"));
        painter.drawText(origin.x() + 5, 18, vertical);
    }
}

void CadCanvas::drawEntities(QPainter& painter)
{
    if (!m_document)
        return;

    const QSizeF vp = viewport();
    for (const cad::Entity& entity : m_document->entities()) {
        QPen pen = cadpainter::entityPen(entity);
        if (m_document->isSelected(entity.id)) {
            pen.setColor(QColor("#2b6cd9"));
            pen.setStyle(Qt::SolidLine);
            pen.setWidthF(pen.widthF() + 1.5);
        }
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        cadpainter::drawEntity(painter, entity, m_view, vp);
    }
}

void CadCanvas::drawRubberBand(QPainter& painter)
{
    if (!m_rubberBand)
        return;
    const QRectF r = QRectF(m_rubberStart, m_rubberEnd).normalized();
    painter.setPen(QPen(QColor("#2b6cd9"), 1, Qt::DashLine));
    painter.setBrush(QColor(43, 108, 217, 30));
    painter.drawRect(r);
}

void CadCanvas::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor("#ffffff"));
    drawGrid(painter);
    drawAxes(painter);
    drawEntities(painter);
    drawRubberBand(painter);
    if (m_drawController)
        m_drawController->drawPreview(painter, m_view, viewport());
}

// ---------------------------------------------------------------------------
// 命中测试与鼠标交互
// ---------------------------------------------------------------------------
qint64 CadCanvas::hitTest(const QPointF& screenPos) const
{
    if (!m_document)
        return 0;

    const QSizeF vp = viewport();
    const cad::Vec2 world = m_view.toWorld(screenPos, vp);
    const double tolerance = kPickTolerancePx / m_view.scale;
    double best = tolerance;
    qint64 bestId = 0;
    for (const cad::Entity& entity : m_document->entities()) {
        const double d = cad::distanceToEntity(world, entity);
        if (d < best) {
            best = d;
            bestId = entity.id;
        }
    }
    return bestId;
}

void CadCanvas::mousePressEvent(QMouseEvent* event)
{
    const QPointF pos = event->position();

    // 绘图命令进行中：左键收集点位 / 拾取对象，右键结束命令
    if (m_drawController && m_drawController->isActive()) {
        if (event->button() == Qt::RightButton) {
            m_drawController->onRightClick();
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton) {
            const cad::Vec2 world = m_view.toWorld(pos, viewport());
            m_drawController->onPoint(world, hitTest(pos));
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::MiddleButton
        || (event->button() == Qt::LeftButton && m_interaction == Interaction::Pan)) {
        m_panning = true;
        m_lastMouse = pos;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton) {
        if (m_interaction != Interaction::Select) {
            setInteraction(Interaction::Select);
            emit statusMessage(QStringLiteral("已退出视图交互模式（右键取消）"));
        }
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (m_interaction == Interaction::ZoomWindow) {
            m_rubberBand = true;
            m_rubberStart = pos;
            m_rubberEnd = pos;
            update();
            return;
        }
        const qint64 id = hitTest(pos);
        QSet<qint64> selection;
        if (id != 0)
            selection.insert(id);
        if (m_document) {
            m_document->setSelection(selection);
            emit statusMessage(id != 0
                                   ? QStringLiteral("已拾取图元 #%1（可「删除」或进入编辑命令）").arg(id)
                                   : QStringLiteral("空白处单击：已清空选择"));
        }
    }
    QWidget::mousePressEvent(event);
}

void CadCanvas::mouseMoveEvent(QMouseEvent* event)
{
    const QPointF pos = event->position();
    const cad::Vec2 world = m_view.toWorld(pos, viewport());
    emit cursorMoved(world.x, world.z);
    if (m_drawController && m_drawController->isActive())
        m_drawController->onHover(world);

    if (m_panning) {
        m_view.panByPixels(pos - m_lastMouse);
        m_lastMouse = pos;
        update();
    } else if (m_rubberBand) {
        m_rubberEnd = pos;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void CadCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_panning && (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton)) {
        m_panning = false;
        setCursor(m_interaction == Interaction::Pan ? Qt::OpenHandCursor : Qt::ArrowCursor);
    }
    if (m_rubberBand && event->button() == Qt::LeftButton) {
        m_rubberEnd = event->position();
        m_rubberBand = false;
        applyZoomWindow(QRectF(m_rubberStart, m_rubberEnd));
    }
    QWidget::mouseReleaseEvent(event);
}

void CadCanvas::wheelEvent(QWheelEvent* event)
{
    const double notches = event->angleDelta().y() / 120.0;
    if (qFuzzyIsNull(notches))
        return;
    m_view.zoomAt(event->position(), std::pow(kZoomStep, notches), viewport());
    update();
    emit viewChanged(m_view.scale);
    event->accept();
}

void CadCanvas::keyPressEvent(QKeyEvent* event)
{
    if (m_drawController && m_drawController->isActive()) {
        if (event->key() == Qt::Key_Escape) {
            m_drawController->cancel();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            m_drawController->finishCurrent();
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void CadCanvas::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
}
