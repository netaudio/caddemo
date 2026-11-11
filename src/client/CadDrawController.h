#pragma once

// 绘图命令状态机（阶段 2）
// 负责 14 个「绘图」按钮的点位收集、对象拾取、橡皮筋预览、
// 键盘参数解析、连续绘制与右键 / Esc / Enter 结束命令。
// 所有文档变更都通过 QUndoStack 提交，保证可撤销。
#include <QObject>
#include <QSizeF>
#include <QString>
#include <QVector>

#include <vector>

#include "CadProtocol.h"
#include "CadViewTransform.h"

class CadCanvas;
class CadDocument;
class QPainter;
class QUndoStack;

enum class DrawTool : int {
    None = 0,
    Point,
    Line,
    Circle,
    Arc3Point,
    ArcEndRadius,
    ArcEndAngle,
    Rectangle,
    Curve,
    Text,
    Concentric,
    Divide,
    CenterPoint,
    Chamfer,
    Fillet,
};

// 按钮文字 / 键盘别名 -> 工具；不是绘图命令时返回 None
DrawTool toolFromCommand(const QString& command);
QString  toolLabel(DrawTool tool);

class CadDrawController : public QObject
{
    Q_OBJECT

public:
    CadDrawController(CadDocument* document, CadCanvas* canvas,
                      QUndoStack* undoStack, QObject* parent = nullptr);

    bool startTool(DrawTool tool);
    bool startCommand(const QString& command);
    void cancel();
    void onRightClick();
    void finishCurrent();                 // Enter：提交当前动态命令

    bool     isActive() const { return m_tool != DrawTool::None; }
    DrawTool tool() const { return m_tool; }
    QString  label() const { return m_label; }
    QString  prompt() const { return m_prompt; }
    bool     wantsPick() const;
    bool     wantsNumber() const;
    bool     wantsText() const;

    void onPoint(const cad::Vec2& world, qint64 hitId);
    void onHover(const cad::Vec2& world);
    bool handleInput(const QString& input);
    void drawPreview(QPainter& painter, const CadViewTransform& view,
                     const QSizeF& viewport);
    cad::Vec2 snap(const cad::Vec2& point) const;

signals:
    void statusMessage(const QString& text);
    void toolChanged();

private:
    enum class Step { Point, Pick, Number, Text };

    Step currentStep() const;
    void advance();
    void commit();
    void resetStep();
    void setPrompt(const QString& text);
    void emitPrompt(const QString& prefix = {});
    double defaultNumber() const;
    void applyNumber(double value);
    cad::Entity styled(cad::EntityType type) const;
    void pushEntities(const QVector<cad::Entity>& entities);
    bool pickAccepted(cad::EntityType type) const;
    cad::Vec2 referencePoint() const;
    void drawPickMarks(QPainter& painter, const CadViewTransform& view,
                       const QSizeF& viewport);

    CadDocument*             m_document = nullptr;
    CadCanvas*               m_canvas = nullptr;
    QUndoStack*              m_undo = nullptr;
    DrawTool                 m_tool = DrawTool::None;
    QString                  m_label;
    QString                  m_prompt;
    std::vector<cad::Vec2>   m_points;
    QVector<qint64>          m_picked;
    double                   m_number = 0.0;
    bool                     m_hasNumber = false;
    QString                  m_text;
    cad::Vec2                m_hover;
    bool                     m_hoverValid = false;
    double                   m_lastRadius = 25.0;
    double                   m_lastAngle = 90.0;
    double                   m_lastDistance = 10.0;
    double                   m_lastCount = 5.0;
    double                   m_lastFillet = 15.0;
    QString                  m_lastText = QStringLiteral("文本");
};
