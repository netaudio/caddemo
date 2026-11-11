#pragma once

// 撤销栈命令（阶段 2）：绘图命令产生的所有文档变更都经过这里，
// 阶段 3 的编辑算法可直接复用 ModifyEntities / RemoveEntities。
#include <QString>
#include <QUndoCommand>
#include <QVector>

#include "CadProtocol.h"

class CadDocument;

namespace cadcmd {

class AddEntity : public QUndoCommand
{
public:
    AddEntity(CadDocument* document, const cad::Entity& entity,
              const QString& label = {}, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    CadDocument* m_document;
    cad::Entity  m_entity;
};

class RemoveEntities : public QUndoCommand
{
public:
    RemoveEntities(CadDocument* document, const QVector<cad::Entity>& entities,
                   const QString& label = {}, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    CadDocument*         m_document;
    QVector<cad::Entity> m_entities;
};

// before / after 按 id 一一对应；用于修剪、倒角、圆角等对已有图元的修改
class ModifyEntities : public QUndoCommand
{
public:
    ModifyEntities(CadDocument* document, const QVector<cad::Entity>& before,
                   const QVector<cad::Entity>& after, const QString& label = {},
                   QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;

private:
    void apply(const QVector<cad::Entity>& snapshot);

    CadDocument*         m_document;
    QVector<cad::Entity> m_before;
    QVector<cad::Entity> m_after;
};

// 把「新建图元」批量压进一个宏，撤销一次即可回退整条命令
QString addEntities(CadDocument* document, class QUndoStack* stack,
                    const QVector<cad::Entity>& entities, const QString& label);

}  // namespace cadcmd
