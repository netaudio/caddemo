#pragma once

#include <QTreeWidget>

class CadDocument;

// 右侧工件树：列出文档图元，支持勾选显隐、点击选中联动、右键删除。
class CadEntityTree : public QTreeWidget
{
    Q_OBJECT

public:
    explicit CadEntityTree(CadDocument* document, QWidget* parent = nullptr);

signals:
    // 用户在树上请求删除若干图元（由主窗口用撤销命令执行）
    void deleteRequested(const QList<qint64>& ids);

private slots:
    void refresh();
    void syncSelection();
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onItemChanged(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);

private:
    QTreeWidgetItem* rootItem() const { return topLevelItem(0); }

    CadDocument* m_document = nullptr;
    bool         m_refreshing = false;
};
