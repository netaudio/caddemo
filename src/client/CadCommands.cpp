#include "CadCommands.h"

#include "CadDocument.h"

#include <QUndoStack>

namespace cadcmd {

namespace {
QString defaultLabel(const QString& label, const QString& fallback)
{
    return label.isEmpty() ? fallback : label;
}
}  // namespace

// ---------------------------------------------------------------------------
AddEntity::AddEntity(CadDocument* document, const cad::Entity& entity,
                     const QString& label, QUndoCommand* parent)
    : QUndoCommand(defaultLabel(label, QStringLiteral("新建图元")), parent)
    , m_document(document)
    , m_entity(entity)
{
}

void AddEntity::redo()
{
    if (m_document->insertEntity(m_entity)) {
        // 首次 push 时 id 由文档分配，记录下来以便后续 redo 复用
        if (m_entity.id <= 0)
            m_entity.id = m_document->entities().last().id;
    }
}

void AddEntity::undo()
{
    m_document->removeEntity(m_entity.id);
}

// ---------------------------------------------------------------------------
RemoveEntities::RemoveEntities(CadDocument* document,
                               const QVector<cad::Entity>& entities,
                               const QString& label, QUndoCommand* parent)
    : QUndoCommand(defaultLabel(label, QStringLiteral("删除图元")), parent)
    , m_document(document)
    , m_entities(entities)
{
}

void RemoveEntities::redo()
{
    for (const cad::Entity& entity : m_entities)
        m_document->removeEntity(entity.id);
}

void RemoveEntities::undo()
{
    for (const cad::Entity& entity : m_entities)
        m_document->insertEntity(entity);
}

// ---------------------------------------------------------------------------
ModifyEntities::ModifyEntities(CadDocument* document,
                               const QVector<cad::Entity>& before,
                               const QVector<cad::Entity>& after,
                               const QString& label, QUndoCommand* parent)
    : QUndoCommand(defaultLabel(label, QStringLiteral("修改图元")), parent)
    , m_document(document)
    , m_before(before)
    , m_after(after)
{
}

void ModifyEntities::apply(const QVector<cad::Entity>& snapshot)
{
    for (const cad::Entity& entity : snapshot)
        m_document->updateEntity(entity);
}

void ModifyEntities::redo()  { apply(m_after); }
void ModifyEntities::undo()  { apply(m_before); }

// ---------------------------------------------------------------------------
QString addEntities(CadDocument* document, QUndoStack* stack,
                    const QVector<cad::Entity>& entities, const QString& label)
{
    if (entities.isEmpty() || !stack)
        return {};
    const QString text = defaultLabel(label, QStringLiteral("绘制图元"));
    if (entities.size() == 1) {
        stack->push(new AddEntity(document, entities.first(), text));
    } else {
        stack->beginMacro(text);
        for (const cad::Entity& entity : entities)
            stack->push(new AddEntity(document, entity, text));
        stack->endMacro();
    }
    return text;
}

}  // namespace cadcmd
