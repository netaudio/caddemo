#pragma once

// 特定功能命令状态机（阶段 5）
// 负责「特定功能」组的命令：切线过原点 / 设定位点 / 工件对齐 / 轮廓跟踪 / 工件测量。
// 与编辑控制器类似，对象拾取 / 点位收集 / 橡皮筋预览均走 QUndoStack 可撤销。
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

enum class FeatureTool : int {
    None = 0,
    Tangent,        // 切线过原点
    PositionPoint,  // 设定位点
    Align,          // 工件对齐
    Contour,        // 轮廓跟踪
    Measure,        // 工件测量
};

FeatureTool featureToolFromCommand(const QString& command);
QString  featureToolLabel(FeatureTool tool);

class CadFeatureController : public QObject
{
    Q_OBJECT

public:
    CadFeatureController(CadDocument* document, CadCanvas* canvas,
                         QUndoStack* undoStack, QObject* parent = nullptr);

    bool startCommand(const QString& command);
    void cancel();
    void onRightClick();
    bool isActive() const { return m_tool != FeatureTool::None; }
    QString  label() const { return m_label; }
    QString  prompt() const { return m_prompt; }

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
    void setActive(FeatureTool tool, const QString& label);
    void setPrompt(const QString& text);
    void endCommand();
    void commitAdd(const QVector<cad::Entity>& created, const QString& label);
    QVector<cad::Entity> snapshot() const;
    void loadSelection();
    void beginSelectPhase(const QString& tip);

    // 各命令逻辑
    void opTangentPick(qint64 hitId);
    void opPositionPick(const cad::Vec2& world);
    void opAlignPick(const cad::Vec2& world);
    void opContour();
    void opMeasurePick(const cad::Vec2& world, qint64 hitId);

    // 预览
    QVector<cad::Entity> previewEntities() const;

    CadDocument* m_document = nullptr;
    CadCanvas*   m_canvas = nullptr;
    QUndoStack*  m_undo = nullptr;

    FeatureTool m_tool = FeatureTool::None;
    int         m_phase = 0;
    QString     m_label;
    QString     m_prompt;
    QVector<cad::Entity> m_work;          // 待操作的图元副本
    QVector<qint64>      m_workIds;       // 对应原图元 id
    QVector<qint64>      m_picked;        // 选择阶段的临时选中
    std::vector<cad::Vec2> m_points;      // 收集到的点
    cad::Vec2    m_hover{0, 0};
    bool         m_hoverValid = false;
    bool         m_havePoint = false;     // 测量是否已收集第一点
};
