#pragma once

#include <QFrame>
#include <QWidget>

class QButtonGroup;
class QComboBox;
class QGridLayout;
class QHBoxLayout;
class QLineEdit;
class QStackedWidget;
class QToolButton;

// Ribbon 功能组：圆角边框 + 内容网格 + 底部组标题
class RibbonGroup : public QFrame
{
public:
    explicit RibbonGroup(const QString& title, QWidget* parent = nullptr);
    QGridLayout* grid() const { return m_grid; }

private:
    QGridLayout* m_grid = nullptr;
};

// 顶部选项卡 + 功能区（Office 2007 风格，参照参考图 CAD 绘制页）
class RibbonBar : public QWidget
{
    Q_OBJECT

public:
    explicit RibbonBar(QWidget* parent = nullptr);

    void selectTab(const QString& name);
    QString currentTab() const;
    // 高亮当前正在执行的绘图命令（空字符串 = 全部取消）
    void setActiveCommand(const QString& command);

signals:
    // 按钮点击（UI 阶段仅把命令名传给状态条，后续接入绘图逻辑）
    void commandTriggered(const QString& command);
    // 「当前属性」下拉框 / 输入框变更
    void propertyChanged(const QString& name, const QString& value);

private:
    QWidget* buildCadPage();
    QWidget* buildPlaceholderPage(const QString& tabName);
    QToolButton* makeButton(const QString& text, const QString& iconKey = QString());
    void addButton(QGridLayout* grid, int row, int col,
                   const QString& text, const QString& iconKey = QString());
    QComboBox* makeCombo(const QString& property, const QStringList& items);
    QWidget* makePropertyBlock();

    QHBoxLayout*   m_tabLayout = nullptr;
    QButtonGroup*  m_tabGroup = nullptr;
    QStackedWidget* m_pages = nullptr;
};
