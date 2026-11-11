#include "MainWindow.h"

#include "CadCanvas.h"
#include "CadCommands.h"
#include "CadDocument.h"
#include "CadDrawController.h"
#include "CadDxf.h"
#include "CadEditController.h"
#include "CadEntityTree.h"
#include "CadIcons.h"
#include "CadPartFile.h"
#include "RibbonBar.h"

#include <QApplication>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QSet>
#include <QShortcut>
#include <QSizeGrip>
#include <QSplitter>
#include <QToolButton>
#include <QTreeWidget>
#include <QUndoStack>
#include <QVBoxLayout>

namespace {

const char* kWindowTitle =
    "ScanPro - V6.0.0 - 超声扫描检测系统 - 中国航空工业集团公司北京航空材料研究院";

// 「当前属性」下拉框文字 -> 图元样式枚举
cad::ColorIndex colorIndexFromName(const QString& name)
{
    if (name == QStringLiteral("红")) return cad::ColorIndex::Red;
    if (name == QStringLiteral("绿")) return cad::ColorIndex::Green;
    if (name == QStringLiteral("蓝")) return cad::ColorIndex::Blue;
    if (name == QStringLiteral("黄")) return cad::ColorIndex::Yellow;
    if (name == QStringLiteral("白")) return cad::ColorIndex::White;
    return cad::ColorIndex::Black;
}

cad::LineType lineTypeFromName(const QString& name)
{
    if (name == QStringLiteral("虚线"))   return cad::LineType::Dashed;
    if (name == QStringLiteral("点划线")) return cad::LineType::DashDot;
    if (name == QStringLiteral("双点划线")) return cad::LineType::DashDotDot;
    return cad::LineType::Solid;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_StyledBackground, true);

    applyStyleSheet();

    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("appRoot"));
    root->setAttribute(Qt::WA_StyledBackground, true);
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // 标题栏 / 选项卡 + 功能区
    rootLayout->addWidget(buildTitleBar());
    m_ribbon = new RibbonBar(root);
    rootLayout->addWidget(m_ribbon);

    // 主内容区：左侧工具条 | 绘图区 | 右侧扫查计划面板
    m_splitter = new QSplitter(Qt::Horizontal, root);
    m_splitter->setObjectName(QStringLiteral("mainSplitter"));
    m_splitter->setHandleWidth(2);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->addWidget(buildLeftRail());
    m_document = new CadDocument(this);
    m_canvas = new CadCanvas(m_splitter);
    m_canvas->setDocument(m_document);
    m_undo = new QUndoStack(this);
    m_drawController = new CadDrawController(m_document, m_canvas, m_undo, this);
    m_canvas->setDrawController(m_drawController);
    m_editController = new CadEditController(m_document, m_canvas, m_undo, this);
    m_canvas->setEditController(m_editController);
    m_splitter->addWidget(m_canvas);
    m_splitter->addWidget(buildRightArea());
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);
    m_splitter->setSizes({30, 1200, 256});
    rootLayout->addWidget(m_splitter, 1);

    rootLayout->addWidget(buildStatusBar());
    setCentralWidget(root);

    connect(m_ribbon, &RibbonBar::commandTriggered, this, &MainWindow::onCommand);
    connect(m_ribbon, &RibbonBar::propertyChanged, this, &MainWindow::onPropertyChanged);
    connect(m_canvas, &CadCanvas::cursorMoved, this, &MainWindow::onCursorMoved);
    connect(m_canvas, &CadCanvas::viewChanged, this, &MainWindow::onViewChanged);
    connect(m_canvas, &CadCanvas::statusMessage, this, &MainWindow::onCanvasStatus);
    connect(m_drawController, &CadDrawController::statusMessage,
            this, &MainWindow::onCanvasStatus);
    connect(m_drawController, &CadDrawController::toolChanged,
            this, &MainWindow::onToolChanged);
    connect(m_editController, &CadEditController::statusMessage,
            this, &MainWindow::onCanvasStatus);
    connect(m_editController, &CadEditController::toolChanged,
            this, &MainWindow::onToolChanged);

    // Esc 全局结束绘图命令；命令栏有焦点时先清空输入
    auto* escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escape, &QShortcut::activated, this, [this]() {
        if (m_drawController->isActive())
            m_drawController->cancel();
        else if (m_commandEdit->hasFocus())
            m_commandEdit->clear();
    });
}

// ---------------------------------------------------------------------------
// 自绘标题栏（圆形 Logo + 快速访问工具栏 + 标题 + 窗口按钮）
// ---------------------------------------------------------------------------
QWidget* MainWindow::buildTitleBar()
{
    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName(QStringLiteral("titleBar"));
    m_titleBar->setAttribute(Qt::WA_StyledBackground, true);
    m_titleBar->setFixedHeight(30);
    m_titleBar->installEventFilter(this);

    auto* layout = new QHBoxLayout(m_titleBar);
    layout->setContentsMargins(4, 1, 4, 1);
    layout->setSpacing(2);

    auto* logo = new QToolButton(m_titleBar);
    logo->setObjectName(QStringLiteral("appLogo"));
    logo->setIcon(CadIcons::icon(QStringLiteral("logo")));
    logo->setIconSize(QSize(26, 26));
    logo->setAutoRaise(true);
    logo->setFixedSize(30, 28);
    logo->setToolTip(QStringLiteral("ScanPro"));
    connect(logo, &QToolButton::clicked, this,
            [this]() { onCommand(QStringLiteral("关于 ScanPro")); });

    auto* quick = new QFrame(m_titleBar);
    quick->setObjectName(QStringLiteral("quickAccess"));
    quick->setAttribute(Qt::WA_StyledBackground, true);
    quick->setFixedHeight(23);
    auto* quickLayout = new QHBoxLayout(quick);
    quickLayout->setContentsMargins(4, 0, 2, 0);
    quickLayout->setSpacing(0);

    auto quickButton = [this, quick, quickLayout](const QString& iconKey,
                                                  const QString& tip,
                                                  const QString& command) {
        auto* button = new QToolButton(quick);
        button->setObjectName(QStringLiteral("quickButton"));
        button->setIcon(CadIcons::icon(iconKey));
        button->setIconSize(QSize(16, 16));
        button->setAutoRaise(true);
        button->setFixedSize(20, 20);
        button->setToolTip(tip);
        if (!command.isEmpty()) {
            connect(button, &QToolButton::clicked, this,
                    [this, command]() { onCommand(command); });
        }
        quickLayout->addWidget(button);
        return button;
    };
    quickButton(QStringLiteral("save"), QStringLiteral("保存"), QStringLiteral("保存Part"));
    quickButton(QStringLiteral("quickundo"), QStringLiteral("撤销"), QStringLiteral("撤销"));

    auto* quickMenu = new QToolButton(quick);
    quickMenu->setObjectName(QStringLiteral("quickButton"));
    quickMenu->setText(QStringLiteral("▾"));
    quickMenu->setAutoRaise(true);
    quickMenu->setFixedSize(16, 20);
    quickLayout->addWidget(quickMenu);

    auto* more = new QToolButton(m_titleBar);
    more->setObjectName(QStringLiteral("windowButton"));
    more->setText(QStringLiteral("≡"));
    more->setAutoRaise(true);
    more->setFixedSize(22, 24);

    auto* title = new QLabel(QString::fromUtf8(kWindowTitle), m_titleBar);
    title->setObjectName(QStringLiteral("titleLabel"));
    title->setAlignment(Qt::AlignCenter);

    auto windowButton = [this](const QString& text, const QString& name) {
        auto* button = new QToolButton(m_titleBar);
        button->setObjectName(name);
        button->setText(text);
        button->setAutoRaise(true);
        button->setFixedSize(28, 24);
        button->setFocusPolicy(Qt::NoFocus);
        return button;
    };
    auto* minButton = windowButton(QStringLiteral("—"), QStringLiteral("windowButton"));
    m_maxButton = windowButton(QStringLiteral("□"), QStringLiteral("windowButton"));
    auto* closeButton = windowButton(QStringLiteral("✕"), QStringLiteral("closeButton"));
    connect(minButton, &QToolButton::clicked, this, &QWidget::showMinimized);
    connect(m_maxButton, &QToolButton::clicked, this, &MainWindow::toggleMaximize);
    connect(closeButton, &QToolButton::clicked, this, &QWidget::close);

    layout->addWidget(logo);
    layout->addWidget(quick);
    layout->addWidget(more);
    layout->addSpacing(8);
    layout->addWidget(title, 1, Qt::AlignCenter);
    layout->addWidget(minButton);
    layout->addWidget(m_maxButton);
    layout->addWidget(closeButton);
    return m_titleBar;
}

// ---------------------------------------------------------------------------
// 左侧竖排工具条
// ---------------------------------------------------------------------------
QWidget* MainWindow::buildLeftRail()
{
    auto* rail = new QWidget(this);
    rail->setObjectName(QStringLiteral("leftRail"));
    rail->setAttribute(Qt::WA_StyledBackground, true);
    rail->setFixedWidth(30);

    auto* layout = new QVBoxLayout(rail);
    layout->setContentsMargins(2, 4, 2, 4);
    layout->setSpacing(2);

    auto* probeTab = new QToolButton(rail);
    probeTab->setObjectName(QStringLiteral("verticalTab"));
    probeTab->setText(QStringLiteral("探\n头\n库"));
    probeTab->setIcon(CadIcons::icon(QStringLiteral("probe")));
    probeTab->setIconSize(QSize(18, 18));
    probeTab->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    probeTab->setFixedSize(26, 152);
    probeTab->setToolTip(QStringLiteral("探头库：载入探头后可进入 CAD 绘制"));
    connect(probeTab, &QToolButton::clicked, this,
            [this]() { onCommand(QStringLiteral("探头库")); });

    auto* pin = new QToolButton(rail);
    pin->setObjectName(QStringLiteral("leftTool"));
    pin->setIcon(CadIcons::icon(QStringLiteral("pushpin")));
    pin->setIconSize(QSize(16, 16));
    pin->setAutoRaise(true);
    pin->setFixedSize(26, 22);
    pin->setToolTip(QStringLiteral("固定工具条"));

    auto* images = new QToolButton(rail);
    images->setObjectName(QStringLiteral("leftTool"));
    images->setIcon(CadIcons::icon(QStringLiteral("images")));
    images->setIconSize(QSize(20, 20));
    images->setAutoRaise(true);
    images->setFixedSize(26, 28);
    images->setToolTip(QStringLiteral("图像列表"));
    connect(images, &QToolButton::clicked, this,
            [this]() { onCommand(QStringLiteral("图像列表")); });

    layout->addWidget(probeTab);
    layout->addWidget(pin);
    layout->addSpacing(4);
    layout->addWidget(images);
    layout->addStretch(1);
    return rail;
}

// ---------------------------------------------------------------------------
// 右侧「盘面/平面扫查计划定制」面板（可折叠）
// ---------------------------------------------------------------------------
QWidget* MainWindow::buildRightArea()
{
    auto* area = new QWidget(this);
    area->setObjectName(QStringLiteral("rightArea"));
    auto* layout = new QHBoxLayout(area);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* strip = new QWidget(area);
    strip->setObjectName(QStringLiteral("collapseStrip"));
    strip->setAttribute(Qt::WA_StyledBackground, true);
    strip->setFixedWidth(16);
    auto* stripLayout = new QVBoxLayout(strip);
    stripLayout->setContentsMargins(0, 0, 0, 0);
    stripLayout->addStretch(1);
    m_collapseButton = new QToolButton(strip);
    m_collapseButton->setObjectName(QStringLiteral("collapseButton"));
    m_collapseButton->setText(QStringLiteral("›"));
    m_collapseButton->setFixedSize(16, 40);
    m_collapseButton->setToolTip(QStringLiteral("折叠/展开扫查计划面板"));
    connect(m_collapseButton, &QToolButton::clicked,
            this, &MainWindow::toggleRightPanel);
    stripLayout->addWidget(m_collapseButton);
    stripLayout->addStretch(1);

    m_rightPanel = new QWidget(area);
    m_rightPanel->setObjectName(QStringLiteral("rightPanel"));
    m_rightPanel->setAttribute(Qt::WA_StyledBackground, true);
    m_rightPanel->setFixedWidth(240);
    auto* panelLayout = new QVBoxLayout(m_rightPanel);
    panelLayout->setContentsMargins(8, 6, 6, 6);
    panelLayout->setSpacing(4);

    auto* panelTitle = new QLabel(QStringLiteral("盘面/平面扫查计划定制"), m_rightPanel);
    panelTitle->setObjectName(QStringLiteral("panelTitle"));

    auto* planBox = new QFrame(m_rightPanel);
    planBox->setObjectName(QStringLiteral("planBox"));
    planBox->setAttribute(Qt::WA_StyledBackground, true);
    planBox->setFrameShape(QFrame::Box);
    auto* boxLayout = new QVBoxLayout(planBox);
    boxLayout->setContentsMargins(4, 4, 4, 4);

    m_entityTree = new CadEntityTree(m_document, planBox);
    m_entityTree->setObjectName(QStringLiteral("planTree"));
    boxLayout->addWidget(m_entityTree);
    boxLayout->addStretch(1);

    connect(m_entityTree, &CadEntityTree::deleteRequested,
            this, &MainWindow::deleteFromTree);

    panelLayout->addWidget(panelTitle);
    panelLayout->addWidget(planBox, 1);

    layout->addWidget(strip);
    layout->addWidget(m_rightPanel);
    return area;
}

// ---------------------------------------------------------------------------
// 底部状态条：命令提示 / 键盘输入 / 坐标
// ---------------------------------------------------------------------------
QWidget* MainWindow::buildStatusBar()
{
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("statusBar"));
    bar->setAttribute(Qt::WA_StyledBackground, true);
    bar->setFixedHeight(26);
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(6, 2, 4, 2);
    layout->setSpacing(6);

    auto* commandLabel = new QLabel(QStringLiteral("命令:"), bar);
    commandLabel->setObjectName(QStringLiteral("statusLabel"));

    m_commandEdit = new QLineEdit(bar);
    m_commandEdit->setObjectName(QStringLiteral("commandEdit"));
    m_commandEdit->setFixedHeight(20);
    m_commandEdit->setFixedWidth(300);
    m_commandEdit->setPlaceholderText(
        QStringLiteral("命令/坐标：直线 / x=10,y=20 / dx=*,dy=* / L=30,A=45 / R=50"));
    connect(m_commandEdit, &QLineEdit::returnPressed,
            this, &MainWindow::onCommandEntered);

    m_hintLabel = new QLabel(
        QStringLiteral("就绪 — 阶段 2：14 个绘图命令已就绪，点按钮后在画布上拾取点位（右键/Esc 结束）"), bar);
    m_hintLabel->setObjectName(QStringLiteral("statusHint"));

    m_coordLabel = new QLabel(QStringLiteral("X: 0.0000    Z: 0.0000"), bar);
    m_coordLabel->setObjectName(QStringLiteral("coordLabel"));
    m_scaleLabel = new QLabel(QStringLiteral("比例 1.0000"), bar);
    m_scaleLabel->setObjectName(QStringLiteral("coordLabel"));

    layout->addWidget(commandLabel);
    layout->addWidget(m_commandEdit);
    layout->addSpacing(6);
    layout->addWidget(m_hintLabel, 1);
    layout->addWidget(m_coordLabel);
    layout->addSpacing(12);
    layout->addWidget(m_scaleLabel);
    layout->addWidget(new QSizeGrip(bar));
    return bar;
}

// ---------------------------------------------------------------------------
// 命令分发（阶段 2：绘图 / 视图 / 撤销 / 删除）
// ---------------------------------------------------------------------------
void MainWindow::onCommand(const QString& command)
{
    // 14 个绘图命令：交给绘图状态机
    if (m_drawController->startCommand(command)) {
        if (m_editController->isActive())
            m_editController->cancel();
        return;
    }
    // 编辑 / 工具命令：交给编辑状态机
    if (m_editController->startCommand(command)) {
        if (m_drawController->isActive())
            m_drawController->cancel();
        return;
    }
    // 点击其它功能按钮：结束任何进行中的命令
    if (m_drawController->isActive())
        m_drawController->cancel();
    if (m_editController->isActive())
        m_editController->cancel();
    m_ribbon->setActiveCommand({});

    if (command == QStringLiteral("放大")) {
        m_canvas->zoomIn();
        m_hintLabel->setText(QStringLiteral("视图：放大（滚轮可以光标为锚点缩放）"));
        return;
    }
    if (command == QStringLiteral("缩小")) {
        m_canvas->zoomOut();
        m_hintLabel->setText(QStringLiteral("视图：缩小（滚轮可以光标为锚点缩放）"));
        return;
    }
    if (command == QStringLiteral("窗口充满")) {
        m_canvas->zoomFit();
        m_hintLabel->setText(QStringLiteral("视图：窗口充满（按图元范围适配）"));
        return;
    }
    if (command == QStringLiteral("窗口移动")) {
        m_canvas->setInteraction(CadCanvas::Interaction::Pan);
        m_hintLabel->setText(QStringLiteral("窗口移动：按住左键或中键拖动，右键结束"));
        return;
    }
    if (command == QStringLiteral("窗口缩放")) {
        m_canvas->setInteraction(CadCanvas::Interaction::ZoomWindow);
        m_hintLabel->setText(QStringLiteral("窗口缩放：拖出矩形框放大到该区域，右键结束"));
        return;
    }
    if (command == QStringLiteral("撤销")) {
        if (m_undo->canUndo()) {
            const QString text = m_undo->undoText();
            m_undo->undo();
            m_hintLabel->setText(QStringLiteral("撤销：%1  —  文档中剩余 %2 个图元")
                                     .arg(text).arg(m_document->count()));
        } else {
            m_hintLabel->setText(QStringLiteral("撤销：没有可撤销的操作"));
        }
        return;
    }
    if (command == QStringLiteral("删除")) {
        deleteSelection();
        return;
    }
    if (command == QStringLiteral("颜色") || command == QStringLiteral("线型")
        || command == QStringLiteral("线宽")) {
        applyStyleToSelection(command);
        return;
    }

    // 阶段 4：文件读写
    if (command == QStringLiteral("打开dxf")) { openDxfFile(); return; }
    if (command == QStringLiteral("打开Part")) { openPartFile(); return; }
    if (command == QStringLiteral("保存Part")) { savePartFile(false); return; }
    if (command == QStringLiteral("另存Part")) { savePartFile(true); return; }

    m_hintLabel->setText(
        QStringLiteral("命令：%1  —  将在阶段 5 接入特定功能逻辑").arg(command));
}

void MainWindow::onToolChanged()
{
    QString active;
    if (m_drawController->isActive())
        active = m_drawController->label();
    else if (m_editController->isActive())
        active = m_editController->label();
    m_ribbon->setActiveCommand(active);
}

void MainWindow::applyStyleToSelection(const QString& which)
{
    const QSet<qint64> ids = m_document->selection();
    if (ids.isEmpty()) {
        m_hintLabel->setText(
            QStringLiteral("%1：请先选中图元；未选中时该样式将作为新建图元默认样式")
                .arg(which));
        return;
    }
    QVector<cad::Entity> before, after;
    for (qint64 id : ids) {
        const cad::Entity* e = m_document->find(id);
        if (!e)
            continue;
        cad::Entity a = *e;
        if (which == QStringLiteral("颜色"))
            a.color = m_document->currentColor();
        else if (which == QStringLiteral("线型"))
            a.lineType = m_document->currentLineType();
        else if (which == QStringLiteral("线宽"))
            a.lineWidth = m_document->currentLineWidth();
        before.append(*e);
        after.append(a);
    }
    if (after.isEmpty())
        return;
    m_undo->push(new cadcmd::ModifyEntities(m_document, before, after, which));
    m_hintLabel->setText(
        QStringLiteral("%1：已将 %2 个选中图元的%3设为当前默认值（可撤销）")
            .arg(which).arg(after.size()).arg(which));
}

void MainWindow::deleteSelection()
{
    const QSet<qint64> ids = m_document->selection();
    if (ids.isEmpty()) {
        m_hintLabel->setText(QStringLiteral("删除：请先在画布上单击选择图元"));
        return;
    }
    QVector<cad::Entity> entities;
    for (qint64 id : ids) {
        if (const cad::Entity* entity = m_document->find(id))
            entities.append(*entity);
    }
    m_undo->push(new cadcmd::RemoveEntities(m_document, entities,
                                            QStringLiteral("删除图元")));
    m_document->clearSelection();
    m_hintLabel->setText(QStringLiteral("删除：已移除 %1 个图元（可用「撤销」恢复）")
                             .arg(entities.size()));
}

void MainWindow::deleteFromTree(const QList<qint64>& ids)
{
    if (ids.isEmpty())
        return;
    QVector<cad::Entity> entities;
    for (qint64 id : ids) {
        if (const cad::Entity* e = m_document->find(id))
            entities.append(*e);
    }
    if (entities.isEmpty())
        return;
    m_undo->push(new cadcmd::RemoveEntities(m_document, entities,
                                            QStringLiteral("删除图元")));
    m_document->clearSelection();
    m_hintLabel->setText(QStringLiteral("删除：已移除 %1 个图元（可用「撤销」恢复）")
                             .arg(entities.size()));
}

void MainWindow::openPartFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开 Part2D 文件"), QString(),
        QStringLiteral("Part2D 文件 (*.part2d *.json);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;
    if (!cadfile::loadPart(path, m_document)) {
        m_hintLabel->setText(QStringLiteral("打开Part：文件读取失败"));
        return;
    }
    m_undo->clear();
    m_currentFile = path;
    m_currentFormat = QStringLiteral("part2d");
    m_canvas->zoomFit();
    m_hintLabel->setText(QStringLiteral("打开Part：已载入 %1 个图元（%2）")
                             .arg(m_document->count()).arg(path));
}

void MainWindow::openDxfFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开 DXF 文件"), QString(),
        QStringLiteral("DXF 文件 (*.dxf);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;
    if (!cadfile::importDxf(path, m_document)) {
        m_hintLabel->setText(QStringLiteral("打开dxf：文件读取失败"));
        return;
    }
    m_undo->clear();
    m_currentFile = path;
    m_currentFormat = QStringLiteral("dxf");
    m_canvas->zoomFit();
    m_hintLabel->setText(QStringLiteral("打开dxf：已导入 %1 个图元（%2）")
                             .arg(m_document->count()).arg(path));
}

void MainWindow::savePartFile(bool asCopy)
{
    QString path = m_currentFile;
    if (asCopy || path.isEmpty() || m_currentFormat != QLatin1String("part2d")) {
        path = QFileDialog::getSaveFileName(
            this, QStringLiteral("保存 Part2D 文件"),
            m_currentFile.isEmpty() ? QStringLiteral("未命名.part2d") : m_currentFile,
            QStringLiteral("Part2D 文件 (*.part2d)"));
        if (path.isEmpty())
            return;
        if (!path.endsWith(QStringLiteral(".part2d"), Qt::CaseInsensitive))
            path += QStringLiteral(".part2d");
    }
    if (cadfile::savePart(path, m_document)) {
        m_currentFile = path;
        m_currentFormat = QStringLiteral("part2d");
        m_hintLabel->setText(QStringLiteral("保存Part：已写出 %1 个图元（%2）")
                                 .arg(m_document->count()).arg(path));
    } else {
        m_hintLabel->setText(QStringLiteral("保存Part：写入失败"));
    }
}

void MainWindow::onPropertyChanged(const QString& name, const QString& value)
{
    if (name == QStringLiteral("颜色")) {
        m_document->setCurrentColor(colorIndexFromName(value));
    } else if (name == QStringLiteral("线型")) {
        m_document->setCurrentLineType(lineTypeFromName(value));
    } else if (name == QStringLiteral("线宽")) {
        bool ok = false;
        const double width = value.toDouble(&ok);
        if (ok)
            m_document->setCurrentLineWidth(width);
    } else if (name == QStringLiteral("坐标系")) {
        m_canvas->setCoordinateSystem(value);
        onCursorMoved(m_lastCursorX, m_lastCursorZ);  // 立即刷新状态栏轴标签
    } else if (name == QStringLiteral("deltaΔ")) {
        bool ok = false;
        const double delta = value.toDouble(&ok);
        if (ok)
            m_canvas->setDelta(delta);
    }
    const QString detail = (name == QStringLiteral("坐标系"))
                               ? QStringLiteral("已切换绘图坐标系")
                           : (name == QStringLiteral("deltaΔ"))
                               ? QStringLiteral("已设为坐标吸附步长（0 = 关闭吸附）")
                               : QStringLiteral("已设为新建图元默认样式");
    m_hintLabel->setText(QStringLiteral("当前属性：%1 = %2  —  %3")
                             .arg(name).arg(value).arg(detail));
}

void MainWindow::onCommandEntered()
{
    const QString input = m_commandEdit->text().trimmed();
    m_commandEdit->clear();
    if (input.isEmpty()) {
        if (m_drawController->isActive())
            m_drawController->finishCurrent();
        return;
    }
    // 活动命令中：输入被解释为点位 / 数值 / 文本参数
    if (m_drawController->handleInput(input))
        return;
    if (m_editController->handleInput(input))
        return;
    // 空闲时：把输入当作命令名（含 U 撤销、视图命令等）
    if (input.compare(QStringLiteral("U"), Qt::CaseInsensitive) == 0)
        onCommand(QStringLiteral("撤销"));
    else
        onCommand(input);
}

void MainWindow::onCursorMoved(double x, double z)
{
    m_lastCursorX = x;
    m_lastCursorZ = z;
    const QString cs = m_canvas->coordinateSystem();
    const QString first  = (cs == QLatin1String("YZ")) ? QStringLiteral("Y") : QStringLiteral("X");
    const QString second = (cs == QLatin1String("XY")) ? QStringLiteral("Y") : QStringLiteral("Z");
    m_coordLabel->setText(QStringLiteral("%1: %2    %3: %4")
                              .arg(first).arg(x, 0, 'f', 4)
                              .arg(second).arg(z, 0, 'f', 4));
}

void MainWindow::onViewChanged(double scale)
{
    if (m_scaleLabel)
        m_scaleLabel->setText(QStringLiteral("比例 %1").arg(scale, 0, 'f', 4));
}

void MainWindow::onCanvasStatus(const QString& text)
{
    m_hintLabel->setText(text);
}

void MainWindow::toggleRightPanel()
{
    const bool show = !m_rightPanel->isVisible();
    m_rightPanel->setVisible(show);
    m_collapseButton->setText(show ? QStringLiteral("›") : QStringLiteral("‹"));
    const int rail = 30;
    const int right = show ? 256 : 16;
    const int canvas = qMax(300, width() - rail - right - 4);
    m_splitter->setSizes({rail, canvas, right});
}

void MainWindow::toggleMaximize()
{
    if (isMaximized()) {
        showNormal();
        m_maxButton->setText(QStringLiteral("□"));
    } else {
        showMaximized();
        m_maxButton->setText(QStringLiteral("▣"));
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_titleBar) {
        if (auto* mouse = dynamic_cast<QMouseEvent*>(event)) {
            switch (event->type()) {
            case QEvent::MouseButtonPress:
                if (mouse->button() == Qt::LeftButton && !isMaximized()) {
                    m_dragOffset = mouse->globalPosition().toPoint() - frameGeometry().topLeft();
                    m_dragging = true;
                    return true;
                }
                break;
            case QEvent::MouseMove:
                if (m_dragging && (mouse->buttons() & Qt::LeftButton)) {
                    move(mouse->globalPosition().toPoint() - m_dragOffset);
                    return true;
                }
                break;
            case QEvent::MouseButtonRelease:
                m_dragging = false;
                break;
            case QEvent::MouseButtonDblClick:
                if (mouse->button() == Qt::LeftButton) {
                    toggleMaximize();
                    return true;
                }
                break;
            default:
                break;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

// ---------------------------------------------------------------------------
// 样式：贴近参考图的 Office 2007 / ScanPro 浅蓝 Ribbon 风格
// ---------------------------------------------------------------------------
void MainWindow::applyStyleSheet()
{
    qApp->setStyleSheet(R"(
QWidget#appRoot {
    background: #ffffff;
    border: 1px solid #93b0d4;
}
QWidget#titleBar, QWidget#tabRow {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #eef5ff, stop:0.55 #d7e7fb, stop:1 #bed5f2);
    border-bottom: 1px solid #a8c2e4;
}
QLabel#titleLabel {
    color: #213a5c;
    font-size: 12px;
}
QToolButton#appLogo, QToolButton#quickButton {
    background: transparent;
    border: none;
    color: #1c3f7c;
}
QFrame#quickAccess {
    background: #f2f7ff;
    border: 1px solid #a9c3e4;
    border-radius: 10px;
}
QToolButton#windowButton, QToolButton#closeButton {
    color: #1c3f7c;
    border: none;
    background: transparent;
    font-size: 12px;
}
QToolButton#windowButton:hover { background: #cfe0f8; border-radius: 2px; }
QToolButton#closeButton:hover { background: #d9534f; color: #ffffff; border-radius: 2px; }

QWidget#ribbonBar, QScrollArea#ribbonScroll {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #dcebfc, stop:1 #bcd4f2);
    border: none;
}
QWidget#ribbonPage {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #e7f1fd, stop:0.6 #d5e6fa, stop:1 #c5daf4);
}
QScrollBar:horizontal {
    height: 7px;
    background: #dceafb;
    border: none;
}
QScrollBar::handle:horizontal { background: #a9c3e4; border-radius: 3px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

QToolButton#ribbonTab {
    color: #1c3f7c;
    font-size: 12px;
    padding: 0 10px;
    border: 1px solid transparent;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    background: transparent;
}
QToolButton#ribbonTab:hover { background: #e9f2fe; }
QToolButton#ribbonTab:checked {
    background: #ffffff;
    border: 1px solid #e4c16a;
    border-bottom: 1px solid #ffffff;
}

QFrame#ribbonGroup {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #edf4fe, stop:1 #cfdef5);
    border: 1px solid #adc6e8;
    border-radius: 5px;
}
QLabel#groupTitle {
    color: #2b4f8e;
    font-size: 11px;
    background: #c5d9f2;
    border-top: 1px solid #adc6e8;
    border-radius: 2px;
}
QToolButton#ribbonButton {
    color: #1c3f7c;
    font-size: 11px;
    border: 1px solid transparent;
    border-radius: 3px;
    padding: 1px 4px;
    background: transparent;
}
QToolButton#ribbonButton:hover { background: #fff2cd; border: 1px solid #e6c168; }
QToolButton#ribbonButton:pressed { background: #f6d68a; }
QToolButton#ribbonButton:checked {
    background: #ffe2a0;
    border: 1px solid #d8a83c;
    color: #6b4a00;
}
QLabel#propertyLabel { color: #1c3f7c; font-size: 11px; }
QComboBox#propertyCombo, QLineEdit#propertyEdit {
    background: #f8fbff;
    border: 1px solid #9fb8dc;
    border-radius: 2px;
    color: #1c2f4f;
    font-size: 11px;
    padding-left: 3px;
}
QComboBox#propertyCombo::drop-down { border: none; width: 14px; }
QLabel#placeholderLabel { color: #6b83a8; font-size: 12px; }

QSplitter#mainSplitter::handle { background: #c6d9f2; }
QWidget#leftRail, QWidget#collapseStrip {
    background: #c9ddf6;
    border: none;
}
QToolButton#verticalTab {
    background: #ffe9a8;
    border: 1px solid #e3c46f;
    border-radius: 2px;
    color: #4a3c12;
    font-size: 11px;
    padding: 2px 0;
}
QToolButton#verticalTab:hover { background: #ffdf85; }
QToolButton#leftTool { background: transparent; border: none; }
QToolButton#collapseButton {
    background: transparent;
    border: none;
    color: #2b4f8e;
    font-size: 12px;
}
QToolButton#collapseButton:hover { background: #b4cdf0; }

QWidget#rightPanel { background: #f1f2f4; border-left: 1px solid #cfd7e2; }
QLabel#panelTitle { color: #222222; font-size: 12px; font-weight: bold; }
QFrame#planBox { background: #ffffff; border: 1px solid #a9b4c4; }
QTreeWidget#planTree { background: #ffffff; border: none; font-size: 12px; color: #222222; }
QTreeWidget#planTree::item { padding: 3px 2px; }

QWidget#statusBar {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                stop:0 #f8fafd, stop:1 #e7eef7);
    border-top: 1px solid #c2cfe0;
}
QLabel#statusLabel, QLabel#statusHint, QLabel#coordLabel { font-size: 11px; }
QLabel#statusLabel { color: #1c3f7c; }
QLabel#statusHint { color: #2b4f8e; }
QLabel#coordLabel { color: #333333; }
QLineEdit#commandEdit {
    background: #ffffff;
    border: 1px solid #b9c8dc;
    border-radius: 2px;
    color: #223650;
    font-size: 11px;
    padding: 1px 4px;
}
)");
}
