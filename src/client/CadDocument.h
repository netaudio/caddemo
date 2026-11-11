#pragma once

#include <QObject>
#include <QSet>
#include <QVector>

#include "CadGeometry.h"
#include "CadProtocol.h"

// 图元文档：持有所有图元、选择集与「当前属性」默认样式。
// 阶段 5 中同一份数据将由服务端持久化，客户端只保留本地镜像。
class CadDocument : public QObject
{
    Q_OBJECT

public:
    explicit CadDocument(QObject* parent = nullptr);

    qint64 addEntity(const cad::Entity& entity);
    // 保留 entity.id 插入（撤销栈 redo 时复用原 id）；id <= 0 时自动分配
    bool   insertEntity(const cad::Entity& entity);
    // 按 id 整体替换（编辑命令 undo/redo 使用）
    bool   updateEntity(const cad::Entity& entity);
    bool   removeEntity(qint64 id);
    void   clear();
    int    count() const { return m_entities.size(); }
    const QVector<cad::Entity>& entities() const { return m_entities; }
    cad::Entity* find(qint64 id);
    const cad::Entity* find(qint64 id) const;
    cad::Bounds bounds() const;

    const QSet<qint64>& selection() const { return m_selection; }
    bool isSelected(qint64 id) const { return m_selection.contains(id); }
    void setSelection(const QSet<qint64>& ids);
    void clearSelection();

    // 当前绘图属性：阶段 2 的绘图命令创建图元时套用
    cad::ColorIndex currentColor() const { return m_color; }
    cad::LineType   currentLineType() const { return m_lineType; }
    double          currentLineWidth() const { return m_lineWidth; }
    void setCurrentColor(cad::ColorIndex color);
    void setCurrentLineType(cad::LineType type);
    void setCurrentLineWidth(double width);

signals:
    void changed();
    void selectionChanged();

private:
    QVector<cad::Entity> m_entities;
    QSet<qint64>         m_selection;
    qint64               m_nextId = 1;
    cad::ColorIndex      m_color = cad::ColorIndex::Black;
    cad::LineType        m_lineType = cad::LineType::Solid;
    double               m_lineWidth = 0.0;
};
