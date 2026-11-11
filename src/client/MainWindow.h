#pragma once

#include <QMainWindow>
#include <QPoint>

class CadCanvas;
class CadDocument;
class CadDrawController;
class QLabel;
class QLineEdit;
class QSplitter;
class QToolButton;
class QWidget;
class QUndoStack;
class RibbonBar;

// 主窗口（阶段 2）：无边框自绘标题栏 + Ribbon + 绘图区 + 左右面板 + 状态条
// 绘图按钮经 CadDrawController 分发，文档变更经 QUndoStack 提交
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    // 供后续绘图命令 / 冒烟测试访问
    CadDocument*       document() const { return m_document; }
    CadCanvas*         canvas() const { return m_canvas; }
    CadDrawController* drawController() const { return m_drawController; }
    QUndoStack*        undoStack() const { return m_undo; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QWidget* buildTitleBar();
    QWidget* buildLeftRail();
    QWidget* buildRightArea();
    QWidget* buildStatusBar();
    void applyStyleSheet();

    void onCommand(const QString& command);
    void onPropertyChanged(const QString& name, const QString& value);
    void onCommandEntered();
    void onCursorMoved(double x, double z);
    void onViewChanged(double scale);
    void onCanvasStatus(const QString& text);
    void onToolChanged();
    void deleteSelection();
    void toggleRightPanel();
    void toggleMaximize();

    QWidget*     m_titleBar = nullptr;
    RibbonBar*   m_ribbon = nullptr;
    QSplitter*   m_splitter = nullptr;
    CadDocument*       m_document = nullptr;
    CadCanvas*         m_canvas = nullptr;
    QUndoStack*        m_undo = nullptr;
    CadDrawController* m_drawController = nullptr;
    QWidget*     m_rightPanel = nullptr;
    QToolButton* m_collapseButton = nullptr;
    QToolButton* m_maxButton = nullptr;

    QLineEdit*   m_commandEdit = nullptr;
    QLabel*      m_hintLabel = nullptr;
    QLabel*      m_coordLabel = nullptr;
    QLabel*      m_scaleLabel = nullptr;

    bool  m_dragging = false;
    QPoint m_dragOffset;
    double m_lastCursorX = 0.0;
    double m_lastCursorZ = 0.0;
};
