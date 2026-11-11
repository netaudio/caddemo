#include "RibbonBar.h"

#include "CadIcons.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// RibbonGroup
// ---------------------------------------------------------------------------
RibbonGroup::RibbonGroup(const QString& title, QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("ribbonGroup"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFrameShape(QFrame::NoFrame);
    setFixedHeight(106);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 3, 4, 0);
    layout->setSpacing(0);

    auto* content = new QWidget(this);
    content->setAttribute(Qt::WA_StyledBackground, true);
    m_grid = new QGridLayout(content);
    m_grid->setContentsMargins(2, 1, 2, 1);
    m_grid->setHorizontalSpacing(2);
    m_grid->setVerticalSpacing(0);
    layout->addWidget(content, 1);

    auto* titleLabel = new QLabel(title, this);
    titleLabel->setObjectName(QStringLiteral("groupTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setFixedHeight(17);
    layout->addWidget(titleLabel);
}

// ---------------------------------------------------------------------------
// RibbonBar
// ---------------------------------------------------------------------------
RibbonBar::RibbonBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ribbonBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(150);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ---- 选项卡行 ----
    auto* tabRow = new QWidget(this);
    tabRow->setObjectName(QStringLiteral("tabRow"));
    tabRow->setAttribute(Qt::WA_StyledBackground, true);
    tabRow->setFixedHeight(28);
    m_tabLayout = new QHBoxLayout(tabRow);
    m_tabLayout->setContentsMargins(8, 2, 8, 0);
    m_tabLayout->setSpacing(0);

    m_tabGroup = new QButtonGroup(this);
    m_tabGroup->setExclusive(true);

    const QStringList tabNames = {
        QStringLiteral("开始"),   QStringLiteral("扫描"), QStringLiteral("图像"),
        QStringLiteral("测量"),   QStringLiteral("工具"), QStringLiteral("选项"),
        QStringLiteral("CAD绘制"), QStringLiteral("超声卡"),
    };

    m_pages = new QStackedWidget(this);
    m_pages->setFixedHeight(122);

    for (int i = 0; i < tabNames.size(); ++i) {
        const QString& name = tabNames.at(i);
        auto* button = new QToolButton(tabRow);
        button->setText(name);
        button->setObjectName(QStringLiteral("ribbonTab"));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setFixedHeight(26);
        button->setFocusPolicy(Qt::NoFocus);
        m_tabGroup->addButton(button, i);
        m_tabLayout->addWidget(button);

        if (name == QStringLiteral("CAD绘制"))
            m_pages->addWidget(buildCadPage());
        else
            m_pages->addWidget(buildPlaceholderPage(name));
    }
    m_tabLayout->addStretch(1);

    connect(m_tabGroup, &QButtonGroup::idClicked,
            m_pages, &QStackedWidget::setCurrentIndex);

    mainLayout->addWidget(tabRow);
    mainLayout->addWidget(m_pages);

    selectTab(QStringLiteral("CAD绘制"));
}

void RibbonBar::selectTab(const QString& name)
{
    for (QAbstractButton* button : m_tabGroup->buttons()) {
        if (auto* toolButton = qobject_cast<QToolButton*>(button);
            toolButton && toolButton->text() == name) {
            toolButton->setChecked(true);
            m_pages->setCurrentIndex(m_tabGroup->id(button));
            return;
        }
    }
}

QString RibbonBar::currentTab() const
{
    if (QAbstractButton* button = m_tabGroup->checkedButton())
        return button->text();
    return {};
}

void RibbonBar::setActiveCommand(const QString& command)
{
    const QList<QToolButton*> buttons = findChildren<QToolButton*>();
    for (QToolButton* button : buttons) {
        if (button->objectName() == QLatin1String("ribbonButton"))
            button->setChecked(!command.isEmpty() && button->text() == command);
    }
}

QToolButton* RibbonBar::makeButton(const QString& text, const QString& iconKey)
{
    auto* button = new QToolButton(this);
    button->setText(text);
    button->setObjectName(QStringLiteral("ribbonButton"));
    button->setIcon(CadIcons::icon(iconKey));
    button->setIconSize(QSize(18, 18));
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setAutoRaise(true);
    button->setCheckable(true);
    button->setFixedHeight(24);
    button->setFocusPolicy(Qt::NoFocus);
    button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(button, &QToolButton::clicked, this,
            [this, text]() { emit commandTriggered(text); });
    return button;
}

void RibbonBar::addButton(QGridLayout* grid, int row, int col,
                          const QString& text, const QString& iconKey)
{
    grid->addWidget(makeButton(text, iconKey), row, col);
}

QComboBox* RibbonBar::makeCombo(const QString& property, const QStringList& items)
{
    auto* combo = new QComboBox(this);
    combo->setObjectName(QStringLiteral("propertyCombo"));
    combo->addItems(items);
    combo->setFixedHeight(21);
    combo->setFixedWidth(100);
    combo->setFocusPolicy(Qt::NoFocus);
    connect(combo, &QComboBox::currentTextChanged, this,
            [this, property](const QString& value) { emit propertyChanged(property, value); });
    return combo;
}

QWidget* RibbonBar::makePropertyBlock()
{
    auto* group = new RibbonGroup(QStringLiteral("当前属性"), this);
    QGridLayout* grid = group->grid();
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(2);

    auto label = [this](const QString& text) {
        auto* l = new QLabel(text, this);
        l->setObjectName(QStringLiteral("propertyLabel"));
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return l;
    };

    grid->addWidget(label(QStringLiteral("颜色")), 0, 0);
    grid->addWidget(makeCombo(QStringLiteral("颜色"),
                              {QStringLiteral("黑"), QStringLiteral("红"), QStringLiteral("绿"),
                               QStringLiteral("蓝"), QStringLiteral("黄"), QStringLiteral("白")}), 0, 1);
    grid->addWidget(label(QStringLiteral("坐标系")), 0, 2);
    grid->addWidget(makeCombo(QStringLiteral("坐标系"),
                              {QStringLiteral("XZ"), QStringLiteral("XY"), QStringLiteral("YZ")}), 0, 3);

    grid->addWidget(label(QStringLiteral("线型")), 1, 0);
    grid->addWidget(makeCombo(QStringLiteral("线型"),
                              {QStringLiteral("实线"), QStringLiteral("虚线"),
                               QStringLiteral("点划线"), QStringLiteral("双点划线")}), 1, 1);
    grid->addWidget(label(QStringLiteral("deltaΔ")), 1, 2);
    auto* delta = new QLineEdit(QStringLiteral("1.000000"), this);
    delta->setObjectName(QStringLiteral("propertyEdit"));
    delta->setFixedHeight(21);
    delta->setFixedWidth(100);
    connect(delta, &QLineEdit::editingFinished, this,
            [this, delta]() { emit propertyChanged(QStringLiteral("deltaΔ"), delta->text()); });
    grid->addWidget(delta, 1, 3);

    grid->addWidget(label(QStringLiteral("线宽")), 2, 0);
    grid->addWidget(makeCombo(QStringLiteral("线宽"),
                              {QStringLiteral("0"), QStringLiteral("0.25"), QStringLiteral("0.5"),
                               QStringLiteral("1.0"), QStringLiteral("2.0")}), 2, 1);

    return group;
}

QWidget* RibbonBar::buildPlaceholderPage(const QString& tabName)
{
    auto* page = new QWidget(this);
    page->setObjectName(QStringLiteral("ribbonPage"));
    page->setAttribute(Qt::WA_StyledBackground, true);
    auto* layout = new QHBoxLayout(page);
    layout->setContentsMargins(12, 4, 12, 4);
    auto* label = new QLabel(
        QStringLiteral("「%1」功能页 —— 后续阶段实现").arg(tabName), page);
    label->setObjectName(QStringLiteral("placeholderLabel"));
    layout->addWidget(label);
    layout->addStretch(1);
    return page;
}

QWidget* RibbonBar::buildCadPage()
{
    auto* page = new QWidget(this);
    page->setObjectName(QStringLiteral("ribbonPage"));
    page->setAttribute(Qt::WA_StyledBackground, true);

    auto* contentLayout = new QHBoxLayout(page);
    contentLayout->setContentsMargins(4, 3, 4, 3);
    contentLayout->setSpacing(2);

    // ---- 文档 ----
    auto* docGroup = new RibbonGroup(QStringLiteral("文档"), page);
    addButton(docGroup->grid(), 0, 0, QStringLiteral("打开dxf"), QStringLiteral("dxf"));
    addButton(docGroup->grid(), 0, 1, QStringLiteral("另存Part"));
    addButton(docGroup->grid(), 1, 0, QStringLiteral("保存Part"));
    addButton(docGroup->grid(), 2, 0, QStringLiteral("打开Part"));
    contentLayout->addWidget(docGroup);

    // ---- 绘图 ----
    auto* drawGroup = new RibbonGroup(QStringLiteral("绘图"), page);
    addButton(drawGroup->grid(), 0, 0, QStringLiteral("点"), QStringLiteral("point"));
    addButton(drawGroup->grid(), 0, 1, QStringLiteral("圆弧(三点)"), QStringLiteral("arc"));
    addButton(drawGroup->grid(), 0, 2, QStringLiteral("曲线"), QStringLiteral("curve"));
    addButton(drawGroup->grid(), 0, 3, QStringLiteral("文本"), QStringLiteral("text"));
    addButton(drawGroup->grid(), 0, 4, QStringLiteral("等分点"));

    addButton(drawGroup->grid(), 1, 0, QStringLiteral("直线"), QStringLiteral("line"));
    addButton(drawGroup->grid(), 1, 1, QStringLiteral("圆弧(端点+半径)"), QStringLiteral("arc"));
    addButton(drawGroup->grid(), 1, 2, QStringLiteral("矩形"), QStringLiteral("rect"));
    addButton(drawGroup->grid(), 1, 3, QStringLiteral("圆角"));
    addButton(drawGroup->grid(), 1, 4, QStringLiteral("圆心点"));

    addButton(drawGroup->grid(), 2, 0, QStringLiteral("圆"), QStringLiteral("circle"));
    addButton(drawGroup->grid(), 2, 1, QStringLiteral("圆弧(端点+角度)"), QStringLiteral("arc"));
    addButton(drawGroup->grid(), 2, 2, QStringLiteral("同心圆"), QStringLiteral("concentric"));
    addButton(drawGroup->grid(), 2, 3, QStringLiteral("倒角"));
    contentLayout->addWidget(drawGroup);

    // ---- 编辑 ----
    auto* editGroup = new RibbonGroup(QStringLiteral("编辑"), page);
    addButton(editGroup->grid(), 0, 0, QStringLiteral("删除"), QStringLiteral("erase"));
    addButton(editGroup->grid(), 0, 1, QStringLiteral("缩放"), QStringLiteral("zoomin"));
    addButton(editGroup->grid(), 0, 2, QStringLiteral("箭头"), QStringLiteral("arrow"));
    addButton(editGroup->grid(), 0, 3, QStringLiteral("线型"), QStringLiteral("linestyle"));

    addButton(editGroup->grid(), 1, 0, QStringLiteral("平移"), QStringLiteral("pan"));
    addButton(editGroup->grid(), 1, 1, QStringLiteral("偏移"), QStringLiteral("offset"));
    addButton(editGroup->grid(), 1, 2, QStringLiteral("颜色"), QStringLiteral("color"));
    addButton(editGroup->grid(), 1, 3, QStringLiteral("线宽"), QStringLiteral("linewidth"));

    addButton(editGroup->grid(), 2, 0, QStringLiteral("旋转"), QStringLiteral("rotate"));
    addButton(editGroup->grid(), 2, 1, QStringLiteral("标注"), QStringLiteral("dimension"));
    addButton(editGroup->grid(), 2, 2, QStringLiteral("文本设置"), QStringLiteral("textsetting"));
    addButton(editGroup->grid(), 2, 3, QStringLiteral("撤销"), QStringLiteral("undo"));
    contentLayout->addWidget(editGroup);

    // ---- 工具 ----
    auto* toolGroup = new RibbonGroup(QStringLiteral("工具"), page);
    addButton(toolGroup->grid(), 0, 0, QStringLiteral("修剪"), QStringLiteral("trim"));
    addButton(toolGroup->grid(), 1, 0, QStringLiteral("延伸"), QStringLiteral("extend"));
    addButton(toolGroup->grid(), 2, 0, QStringLiteral("对称"), QStringLiteral("mirror"));
    contentLayout->addWidget(toolGroup);

    // ---- 视图 ----
    auto* viewGroup = new RibbonGroup(QStringLiteral("视图"), page);
    addButton(viewGroup->grid(), 0, 0, QStringLiteral("放大"), QStringLiteral("zoomin"));
    addButton(viewGroup->grid(), 0, 1, QStringLiteral("窗口移动"), QStringLiteral("move"));
    addButton(viewGroup->grid(), 1, 0, QStringLiteral("缩小"), QStringLiteral("zoomout"));
    addButton(viewGroup->grid(), 1, 1, QStringLiteral("窗口缩放"), QStringLiteral("zoomwindow"));
    addButton(viewGroup->grid(), 2, 0, QStringLiteral("窗口充满"));
    contentLayout->addWidget(viewGroup);

    // ---- 特定功能 ----
    auto* specialGroup = new RibbonGroup(QStringLiteral("特定功能"), page);
    addButton(specialGroup->grid(), 0, 0, QStringLiteral("图元属性"));
    addButton(specialGroup->grid(), 0, 1, QStringLiteral("切线过原点"));
    addButton(specialGroup->grid(), 1, 0, QStringLiteral("设定位点"));
    addButton(specialGroup->grid(), 1, 1, QStringLiteral("工件对齐"));
    addButton(specialGroup->grid(), 2, 0, QStringLiteral("轮廓跟踪"));
    addButton(specialGroup->grid(), 2, 1, QStringLiteral("工件测量"));
    contentLayout->addWidget(specialGroup);

    // ---- 当前属性 ----
    contentLayout->addWidget(makePropertyBlock());
    contentLayout->addStretch(1);
    // 固定内容最小宽度：窗口较窄时由 QScrollArea 提供横向滚动条，
    // 避免「当前属性」等分组被压缩截断
    page->setMinimumWidth(contentLayout->sizeHint().width());

    // 窄屏时功能区可横向滚动，保证按钮文字不被压缩
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("ribbonScroll"));
    scroll->setWidget(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setAttribute(Qt::WA_StyledBackground, true);
    return scroll;
}
