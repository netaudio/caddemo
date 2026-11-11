#pragma once

// 编辑 / 工具命令状态机（阶段 3）
// 负责「编辑」组（平移 / 旋转 / 缩放 / 偏移 / 箭头 / 标注 / 文本设置 / 修剪 / 延伸 / 对称）
// 与「工具」组（修剪 / 延伸 / 对称）的命令流程：对象拾取、点位收集、
// 键盘参数、橡皮筋预览，所有文档变更经 QUndoStack 提交以保证可撤销。
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

enum class EditTool : int {
    None = 0,
    Move,         // 平移
    Rotate,       // 旋转
    Scale,        // 缩放
    Offset,       // 偏移
    Mirror,       // 对称（镜像）
    Trim,         // 修剪
    Extend,       // 延伸
    Arrow,        // 箭头
    Dimension,    // 标注
    TextSetting,  // 文本设置
};

EditTool editToolFromCommand(const QString& command);
QString  editToolLabel(EditTool tool);

class CadEditController : public QObject
{
    Q_OBJECT

public:
    CadEditController(CadDocument* document, CadCanvas* canvas,
                      QUndoStack* undoStack, QObject* parent = nullptr);

    bool startTool(EditTool tool);
    bool startCommand(const QString& command);
    void cancel();
    void onRightClick();
    void finishCurrent();

    bool     isActive() const { return m_tool != EditTool::None; }
    EditTool tool() const { return m_tool; }
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
    // ---- 外部依赖 ----
    CadDocument*       m_document = nullptr;
    CadCanvas*         m_canvas = nullptr;
    QUndoStack*        m_undo = nullptr;

    // ---- 状态 ----
    EditTool          m_tool = EditTool::None;
    int               m_phase = 0;             // 当前步骤
    QString           m_label;
    QString           m_prompt;
    QVector<cad::Entity> m_work;                // 待编辑图元的副本
    QVector<qint64>   m_workIds;                // 对应原图元 id
    std::vector<cad::Vec2> m_points;            // 收集到的点
    double            m_value = 0.0;
    bool              m_haveValue = false;
    QString           m_text;
    cad::Vec2         m_hover{0, 0};
    bool              m_hoverValid = false;
    QVector<qint64>   m_picked;                 // 对象拾取阶段临时选中
    qint64            m_boundaryId = 0;         // 修剪 / 延伸的边界图元
    qint64            m_targetId = 0;           // 修剪 / 延伸的目标图元

    // ---- 记住上次输入 ----
    double m_lastOffset = 20.0;
    double m_lastAngle = 45.0;
    double m_lastScale = 2.0;
    double m_lastTextH = 8.0;

    // ---- 内部辅助 ----
    void loadSelection();                        // 把文档当前选择载入 m_work
    void beginSelectPhase(const QString& tip);   // 进入对象拾取阶段
    void setPrompt(const QString& text);
    void setActive(EditTool tool, const QString& label);
    void endCommand();
    void commitModify(const QVector<cad::Entity>& after, const QString& label);
    void commitAdd(const QVector<cad::Entity>& created, const QString& label);
    QVector<cad::Entity> snapshot() const;       // 复制 m_work 当前内容

    // ---- 各工具拾取逻辑 ----
    void onSelectPhasePick(qint64 hitId);
    void onTransformPick(const cad::Vec2& world);
    void applyTransform(const cad::Vec2& world);
    void onMirrorPick(const cad::Vec2& world);
    void onDimensionPick(const cad::Vec2& world);
    void opOffsetPick(const cad::Vec2& world, qint64 hitId);
    void opTrimExtendPick(const cad::Vec2& world, qint64 hitId);
    void opArrowPick(qint64 hitId);
    void opTextPick(qint64 hitId);
    void applyTextSetting(double height, const QString& content);

    // ---- 预览 ----
    QVector<cad::Entity> previewEntities() const;
    void drawArrowHead(QPainter& painter, const cad::Vec2& tip, const cad::Vec2& back,
                       const CadViewTransform& view, const QSizeF& vp, const QPen& pen);
};

