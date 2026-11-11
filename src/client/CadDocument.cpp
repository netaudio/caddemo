#include "CadDocument.h"

#include <algorithm>

CadDocument::CadDocument(QObject* parent)
    : QObject(parent)
{
}

qint64 CadDocument::addEntity(const cad::Entity& entity)
{
    cad::Entity copy = entity;
    copy.id = m_nextId++;
    m_entities.append(copy);
    emit changed();
    return copy.id;
}

bool CadDocument::insertEntity(const cad::Entity& entity)
{
    cad::Entity copy = entity;
    if (copy.id <= 0)
        copy.id = m_nextId++;
    else
        m_nextId = std::max<qint64>(m_nextId, static_cast<qint64>(copy.id + 1));
    // 同一 id 不重复插入
    if (find(copy.id))
        return false;
    m_entities.append(copy);
    emit changed();
    return true;
}

bool CadDocument::updateEntity(const cad::Entity& entity)
{
    if (cad::Entity* target = find(entity.id)) {
        *target = entity;
        emit changed();
        return true;
    }
    return false;
}

bool CadDocument::removeEntity(qint64 id)
{
    for (int i = 0; i < m_entities.size(); ++i) {
        if (m_entities.at(i).id == id) {
            m_entities.removeAt(i);
            if (m_selection.remove(id))
                emit selectionChanged();
            emit changed();
            return true;
        }
    }
    return false;
}

void CadDocument::clear()
{
    if (m_entities.isEmpty() && m_selection.isEmpty())
        return;
    m_entities.clear();
    m_selection.clear();
    emit selectionChanged();
    emit changed();
}

cad::Entity* CadDocument::find(qint64 id)
{
    for (cad::Entity& entity : m_entities) {
        if (entity.id == id)
            return &entity;
    }
    return nullptr;
}

const cad::Entity* CadDocument::find(qint64 id) const
{
    for (const cad::Entity& entity : m_entities) {
        if (entity.id == id)
            return &entity;
    }
    return nullptr;
}

cad::Bounds CadDocument::bounds() const
{
    cad::Bounds result;
    for (const cad::Entity& entity : m_entities) {
        const cad::Bounds b = cad::entityBounds(entity);
        if (!b.valid)
            continue;
        cad::extend(result, {b.minX, b.minZ});
        cad::extend(result, {b.maxX, b.maxZ});
    }
    return result;
}

void CadDocument::setSelection(const QSet<qint64>& ids)
{
    if (m_selection == ids)
        return;
    m_selection = ids;
    emit selectionChanged();
}

void CadDocument::setEntityVisible(qint64 id, bool visible)
{
    if (cad::Entity* target = find(id)) {
        if (target->visible != visible) {
            target->visible = visible;
            emit changed();
        }
    }
}

void CadDocument::clearSelection()
{
    if (m_selection.isEmpty())
        return;
    m_selection.clear();
    emit selectionChanged();
}

void CadDocument::setCurrentColor(cad::ColorIndex color)
{
    if (m_color != color) {
        m_color = color;
        emit changed();
    }
}

void CadDocument::setCurrentLineType(cad::LineType type)
{
    if (m_lineType != type) {
        m_lineType = type;
        emit changed();
    }
}

void CadDocument::setCurrentLineWidth(double width)
{
    if (!qFuzzyCompare(m_lineWidth + 1.0, width + 1.0)) {
        m_lineWidth = width;
        emit changed();
    }
}
