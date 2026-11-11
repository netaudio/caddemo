#pragma once

#include <QPointF>
#include <QWidget>

#include "CadViewTransform.h"

class CadDocument;
class CadDrawController;
class CadEditController;
class QKeyEvent;
class QPainter;

// CAD 绘图区（阶段 2）：渲染图元文档 + X-Z 坐标轴 + 辅助栅格，
// 提供缩放 / 平移 / 框选缩放 / 命中拾取；绘图命令由 CadDrawController 接管鼠标事件。
class CadCanvas : public QWidget
{
    Q_OBJECT

public:
    enum class Interaction { Select, Pan, ZoomWindow };

    explicit CadCanvas(QWidget* parent = nullptr);

    void setDocument(CadDocument* document);
    CadDocument* document() const { return m_document; }
    void setDrawController(CadDrawController* controller);
    CadDrawController* drawController() const { return m_drawController; }
    void setEditController(CadEditController* controller);
    CadEditController* editController() const { return m_editController; }

    const CadViewTransform& view() const { return m_view; }

    void setGridVisible(bool visible);
    bool isGridVisible() const { return m_gridVisible; }

    void setCoordinateSystem(const QString& name);
    QString coordinateSystem() const { return m_coordinateSystem; }
    void setDelta(double value) { m_delta = value; }
    double delta() const { return m_delta; }

    void setInteraction(Interaction mode);
    Interaction interaction() const { return m_interaction; }

public slots:
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void resetView();

signals:
    // 鼠标位置（世界坐标），用于底部状态条
    void cursorMoved(double x, double z);
    // 视图比例变化，用于状态条「比例」
    void viewChanged(double scale);
    // 交互反馈，显示在状态条提示区
    void statusMessage(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QSizeF viewport() const;
    void drawGrid(QPainter& painter);
    void drawAxes(QPainter& painter);
    void drawEntities(QPainter& painter);
    void drawRubberBand(QPainter& painter);
    void applyZoomWindow(const QRectF& rect);

    qint64 hitTest(const QPointF& screenPos) const;

    CadDocument*        m_document = nullptr;
    CadDrawController*  m_drawController = nullptr;
    CadEditController*  m_editController = nullptr;
    CadViewTransform  m_view;
    Interaction       m_interaction = Interaction::Select;
    bool              m_gridVisible = false;
    QString           m_coordinateSystem = QStringLiteral("XZ");
    double            m_delta = 1.0;

    bool    m_panning = false;
    QPointF m_lastMouse;
    bool    m_rubberBand = false;
    QPointF m_rubberStart;
    QPointF m_rubberEnd;
};
