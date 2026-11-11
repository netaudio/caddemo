#include "CadEntityTree.h"

#include "CadDocument.h"
#include "CadPartFile.h"
#include "CadProtocol.h"

#include <QColor>
#include <QMenu>

namespace {
QColor entityQColor(cad::ColorIndex c)
{
    switch (c) {
    case cad::ColorIndex::Red:    return QColor("#d12d2d");
    case cad::ColorIndex::Yellow: return QColor("#c9a200");
    case cad::ColorIndex::Green:  return QColor("#1f8a3b");
    case cad::ColorIndex::Blue:   return QColor("#2b6cd9");
    case cad::ColorIndex::White: return QColor("#777777");
    case cad::ColorIndex::Black:  return QColor("#222222");
    }
    return QColor("#222222");
}
}  // namespace

CadEntityTree::CadEntityTree(CadDocument* document, QWidget* parent)
    : QTreeWidget(parent)
    , m_document(document)
{
    setObjectName(QStringLiteral("entityTree"));
    setHeaderHidden(true);
    setFrameShape(QFrame::NoFrame);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setContextMenuPolicy(Qt::CustomContextMenu);

    auto* root = new QTreeWidgetItem(this);
    root->setText(0, QStringLiteral("工件图元"));
    root->setFlags(root->flags() & ~Qt::ItemIsSelectable);
    addTopLevelItem(root);

    connect(this, &QTreeWidget::itemClicked, this, &CadEntityTree::onItemClicked);
    connect(this, &QTreeWidget::itemChanged, this, &CadEntityTree::onItemChanged);
    connect(this, &QWidget::customContextMenuRequested, this, &CadEntityTree::onContextMenu);

    if (m_document) {
        connect(m_document, &CadDocument::changed, this, &CadEntityTree::refresh);
        connect(m_document, &CadDocument::selectionChanged, this, &CadEntityTree::syncSelection);
        refresh();
    }
}

void CadEntityTree::refresh()
{
    m_refreshing = true;
    QTreeWidgetItem* root = rootItem();
    while (root->childCount() > 0)
        root->takeChild(0);

    if (m_document) {
        for (const cad::Entity& e : m_document->entities()) {
            auto* item = new QTreeWidgetItem(root);
            QString label = QStringLiteral("%1 #%2")
                               .arg(cadfile::entityTypeName(e.type))
                               .arg(e.id);
            if (!e.text.empty()
                && (e.type == cad::EntityType::Text
                    || e.type == cad::EntityType::Dimension))
                label += QStringLiteral(" \"%1\"").arg(QString::fromStdString(e.text));
            item->setText(0, label);
            item->setData(0, Qt::UserRole, static_cast<qint64>(e.id));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, e.visible ? Qt::Checked : Qt::Unchecked);
            item->setForeground(0, entityQColor(e.color));
        }
        root->setText(0, QStringLiteral("工件图元 (%1)").arg(m_document->count()));
    }
    expandAll();
    m_refreshing = false;
    syncSelection();
}

void CadEntityTree::syncSelection()
{
    if (!m_document)
        return;
    const QSet<qint64> sel = m_document->selection();
    for (int i = 0; i < rootItem()->childCount(); ++i) {
        QTreeWidgetItem* it = rootItem()->child(i);
        const qint64 id = it->data(0, Qt::UserRole).toLongLong();
        it->setSelected(sel.contains(id));
    }
}

void CadEntityTree::onItemClicked(QTreeWidgetItem* item, int /*column*/)
{
    if (m_refreshing || item == rootItem() || !m_document)
        return;
    const qint64 id = item->data(0, Qt::UserRole).toLongLong();
    m_document->setSelection({id});
}

void CadEntityTree::onItemChanged(QTreeWidgetItem* item, int column)
{
    if (m_refreshing || column != 0 || item == rootItem() || !m_document)
        return;
    const qint64 id = item->data(0, Qt::UserRole).toLongLong();
    m_document->setEntityVisible(id, item->checkState(0) == Qt::Checked);
}

void CadEntityTree::onContextMenu(const QPoint& pos)
{
    QList<qint64> ids;
    for (QTreeWidgetItem* it : selectedItems()) {
        if (it == rootItem())
            continue;
        ids.append(it->data(0, Qt::UserRole).toLongLong());
    }
    if (ids.isEmpty())
        return;
    QMenu menu(this);
    QAction* act = menu.addAction(QStringLiteral("删除选中图元"));
    if (act == menu.exec(mapToGlobal(pos)))
        emit deleteRequested(ids);
}
