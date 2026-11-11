#include "client/MainWindow.h"

#include "client/CadCanvas.h"
#include "client/CadDocument.h"
#include "client/CadDrawController.h"
#include "client/CadEditController.h"
#include "client/CadPartFile.h"
#include "client/CadDxf.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QFont>
#include <QLabel>
#include <QScreen>
#include <QTimer>
#include <QToolButton>
#include <QUndoStack>

namespace {

const char* kWindowTitle =
    "ScanPro - V6.0.0 - 超声扫描检测系统 - 中国航空工业集团公司北京航空材料研究院";

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ScanPro CAD"));
    QApplication::setOrganizationName(QStringLiteral("ScanPro"));

    // 中文字体（界面文字与参考图一致，使用微软雅黑）
    QFont font(QStringLiteral("Microsoft YaHei"), 9);
    font.setHintingPreference(QFont::PreferDefaultHinting);
    QApplication::setFont(font);

    // 便于无人值守验证界面：--screenshot <path> 会在显示后抓取窗口并退出
    QString screenshotPath;
    bool selfTest = false;
    const QStringList args = QApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args.at(i) == QLatin1String("--screenshot") && i + 1 < args.size())
            screenshotPath = args.at(++i);
        else if (args.at(i) == QLatin1String("--selftest"))
            selfTest = true;
    }

    MainWindow window;
    window.setWindowTitle(QString::fromUtf8(kWindowTitle));
    // 功能区控件较多，按屏幕可用空间取一个尽量大的初始尺寸
    QScreen* screen = QApplication::primaryScreen();
    const QSize available = screen ? screen->availableGeometry().size() : QSize(1760, 920);
    window.resize(qMin(1760, available.width() - 20),
                  qMin(920, available.height() - 40));
    window.show();

    if (!screenshotPath.isEmpty()) {
        // 阶段 2 冒烟测试：通过真实绘图命令创建 14 类图元，并验证撤销 / 视图 / 属性
        if (selfTest) {
            QTimer::singleShot(180, [&window]() {
                CadDocument* document = window.document();
                CadDrawController* controller = window.drawController();
                if (!document || !controller)
                    return;

                const QList<QToolButton*> buttons = window.findChildren<QToolButton*>();
                auto click = [&buttons](const QString& text) {
                    for (QToolButton* button : buttons) {
                        if (button->text() == text) {
                            button->click();
                            break;
                        }
                    }
                };
                auto point = [controller](double x, double z, qint64 hit = 0) {
                    controller->onPoint({x, z}, hit);
                };

                // --- 基础 6 类 ---
                click(QStringLiteral("直线"));
                point(-170, 110); point(-60, 110);
                const qint64 lineId = document->entities().last().id;
                controller->cancel();

                click(QStringLiteral("圆"));
                point(0, 40); controller->handleInput(QStringLiteral("R=46"));
                const qint64 circleId = document->entities().last().id;
                controller->cancel();

                click(QStringLiteral("矩形"));
                point(-170, -30); point(-70, -110);
                controller->cancel();

                click(QStringLiteral("点"));
                point(-180, 10);
                controller->cancel();

                click(QStringLiteral("文本"));
                point(-165, -145);
                controller->handleInput(QStringLiteral("CAD 绘图"));
                controller->cancel();

                click(QStringLiteral("同心圆"));
                point(150, 40);
                controller->handleInput(QStringLiteral("R=44"));
                controller->cancel();

                // --- 3 类圆弧 ---
                click(QStringLiteral("圆弧(三点)"));
                point(20, 150); point(80, 195); point(140, 135);
                controller->cancel();

                click(QStringLiteral("圆弧(端点+半径)"));
                point(30, -45); point(130, -45);
                controller->handleInput(QStringLiteral("R=72"));
                controller->cancel();

                click(QStringLiteral("圆弧(端点+角度)"));
                point(35, -140); point(135, -140);
                controller->handleInput(QStringLiteral("A=110"));
                controller->cancel();

                // --- 曲线 / 等分点 / 圆心点 ---
                click(QStringLiteral("曲线"));
                point(-180, 190); point(-120, 215); point(-60, 180); point(0, 205);
                controller->finishCurrent();

                click(QStringLiteral("等分点"));
                point(-115, 110, lineId);
                controller->handleInput(QStringLiteral("N=5"));

                click(QStringLiteral("圆心点"));
                point(0, 40, circleId);

                // --- 倒角 / 圆角：先画两对相交直线 ---
                auto drawLine = [&](double ax, double az, double bx, double bz) {
                    controller->startCommand(QStringLiteral("直线"));
                    point(ax, az); point(bx, bz);
                    const qint64 id = document->entities().last().id;
                    controller->cancel();
                    return id;
                };
                const qint64 c1 = drawLine(160, -40, 240, 20);
                const qint64 c2 = drawLine(240, -40, 160, 20);
                click(QStringLiteral("倒角"));
                point(200, -10, c1); point(200, -10, c2);
                controller->handleInput(QStringLiteral("D=12"));

                const qint64 f1 = drawLine(160, -110, 240, -60);
                const qint64 f2 = drawLine(240, -110, 160, -60);
                click(QStringLiteral("圆角"));
                point(200, -85, f1); point(200, -85, f2);
                controller->handleInput(QStringLiteral("R=16"));

                // --- 视图 / 选项卡 / 属性 / 面板交互 ---
                window.canvas()->zoomFit();
                click(QStringLiteral("放大"));
                click(QStringLiteral("缩小"));
                click(QStringLiteral("开始"));
                click(QStringLiteral("CAD绘制"));
                click(QStringLiteral("›"));
                for (QComboBox* combo : window.findChildren<QComboBox*>()) {
                    if (combo->currentText() == QStringLiteral("XZ"))
                        combo->setCurrentIndex(1);   // 坐标系 -> XY
                }
                // 结束阶段 2 残留的演示命令
                controller->cancel();
                if (window.editController()->isActive())
                    window.editController()->cancel();

                // 撤销最后一次圆角宏，验证 QUndoStack 生效
                const int beforeUndo = document->count();
                if (window.undoStack()->canUndo())
                    window.undoStack()->undo();
                const int afterUndo = document->count();

                // ============ 阶段 3：编辑 / 工具命令验证（独立分区 + 文字标注）============
                CadEditController* editor = window.editController();
                auto epoint = [editor](double x, double z, qint64 hit = 0) {
                    editor->onPoint({x, z}, hit);
                };
                auto label = [&](double x, double z, const QString& t) {
                    if (editor->isActive())
                        editor->cancel();
                    controller->startCommand(QStringLiteral("文本"));
                    point(x, z);
                    controller->handleInput(t);
                    controller->cancel();
                };
                window.document()->setSelection({});

                // 4 列 x 2 行网格，每个格子独立演示一种编辑，远离阶段 2 图形
                const double colX[4] = {300.0, 400.0, 500.0, 600.0};
                const double rowZ[2] = {70.0, -40.0};

                label(300, 150, QStringLiteral("阶段3 · 编辑/工具命令验证"));

                // 单元格 1：偏移 —— 原始水平线 + 向上偏移副本
                {
                    const qint64 a = drawLine(colX[0]-30, rowZ[0], colX[0]+30, rowZ[0]);
                    click(QStringLiteral("偏移"));
                    epoint(colX[0], rowZ[0], a);
                    editor->handleInput(QStringLiteral("D=20"));
                    label(colX[0], rowZ[0]+34, QStringLiteral("偏移"));
                }
                // 单元格 2：修剪 —— 十字交叉，左段被剪
                {
                    const qint64 h = drawLine(colX[1]-30, rowZ[0], colX[1]+30, rowZ[0]);
                    const qint64 v = drawLine(colX[1], rowZ[0]+25, colX[1], rowZ[0]-25);
                    click(QStringLiteral("修剪"));
                    epoint(colX[1], rowZ[0], v);       // 边界（竖线）
                    epoint(colX[1]-20, rowZ[0], h);    // 目标 + 要剪掉的一侧（左）
                    label(colX[1], rowZ[0]+34, QStringLiteral("修剪"));
                }
                // 单元格 3：延伸 —— 短竖线向下延伸到水平边界
                {
                    const qint64 b = drawLine(colX[2]-25, rowZ[0], colX[2]+25, rowZ[0]);
                    const qint64 s = drawLine(colX[2], rowZ[0], colX[2], rowZ[0]+25);
                    click(QStringLiteral("延伸"));
                    epoint(colX[2], rowZ[0], b);       // 边界（横线）
                    epoint(colX[2], rowZ[0]+10, s);    // 目标（短竖线）
                    label(colX[2], rowZ[0]+34, QStringLiteral("延伸"));
                }
                // 单元格 4：箭头 —— 带 V 形箭头的直线
                {
                    const qint64 ar = drawLine(colX[3]-30, rowZ[0], colX[3]+30, rowZ[0]);
                    window.document()->setSelection({ar});
                    click(QStringLiteral("箭头"));
                    label(colX[3], rowZ[0]+34, QStringLiteral("箭头"));
                }
                // 单元格 5：平移 —— 原始 + 上移后的线
                {
                    const qint64 m = drawLine(colX[0]-30, rowZ[1], colX[0]+30, rowZ[1]);
                    window.document()->setSelection({m});
                    click(QStringLiteral("平移"));
                    epoint(colX[0], rowZ[1]);
                    epoint(colX[0], rowZ[1]+30);
                    label(colX[0], rowZ[1]-22, QStringLiteral("平移"));
                }
                // 单元格 6：对称 —— 原始 + 水平镜像副本
                {
                    const qint64 mi = drawLine(colX[1]-30, rowZ[1], colX[1]+30, rowZ[1]);
                    window.document()->setSelection({mi});
                    click(QStringLiteral("对称"));
                    epoint(colX[1]-30, rowZ[1]-20);
                    epoint(colX[1]+30, rowZ[1]-20);    // 镜像线（水平）
                    label(colX[1], rowZ[1]-44, QStringLiteral("对称"));
                }
                // 单元格 7：标注 —— 两点尺寸线 + 长度文字
                {
                    click(QStringLiteral("标注"));
                    epoint(colX[2]-30, rowZ[1]);
                    epoint(colX[2]+30, rowZ[1]);
                    label(colX[2], rowZ[1]-22, QStringLiteral("标注"));
                }
                // 单元格 8：颜色 —— 红色直线（先切下拉框，再应用到选中）
                {
                    const qint64 c = drawLine(colX[3]-30, rowZ[1], colX[3]+30, rowZ[1]);
                    for (QComboBox* combo : window.findChildren<QComboBox*>()) {
                        if (combo->currentText() == QStringLiteral("黑"))
                            combo->setCurrentIndex(1);   // 颜色 -> 红
                    }
                    window.document()->setSelection({c});
                    click(QStringLiteral("颜色"));
                    label(colX[3], rowZ[1]-22, QStringLiteral("颜色"));
                }

                // 收尾：结束命令并适配视图
                controller->cancel();
                if (editor->isActive())
                    editor->cancel();
                // 展开右侧面板（阶段 2 自测中曾折叠），让截图能展示工件树
                for (QToolButton* b : window.findChildren<QToolButton*>()) {
                    if (b->objectName() == QStringLiteral("collapseButton")) {
                        b->click();
                        break;
                    }
                }
                window.canvas()->zoomFit();

                // ============ 阶段 4：文件读写验证（Part2D / DXF 往返） ============
                int stage4Src = 0, stage4Part = -1, stage4Dxf = -1;
                {
                    CadDocument tmp;
                    auto mkLine = [&](double x1, double z1, double x2, double z2,
                                     cad::ColorIndex c) {
                        cad::Entity e; e.type = cad::EntityType::Line;
                        e.points = {{x1, z1}, {x2, z2}}; e.color = c; tmp.addEntity(e);
                    };
                    auto mkCircle = [&](double x, double z, double r, cad::ColorIndex c) {
                        cad::Entity e; e.type = cad::EntityType::Circle;
                        e.points = {{x, z}}; e.radius = r; e.color = c; tmp.addEntity(e);
                    };
                    auto mkArc = [&](double x, double z, double r, double a0, double a1,
                                    cad::ColorIndex c) {
                        cad::Entity e; e.type = cad::EntityType::Arc;
                        e.points = {{x, z}}; e.radius = r;
                        e.startAngle = a0; e.endAngle = a1; e.color = c; tmp.addEntity(e);
                    };
                    auto mkText = [&](double x, double z, const QString& t, double h) {
                        cad::Entity e; e.type = cad::EntityType::Text;
                        e.points = {{x, z}}; e.textHeight = h;
                        e.text = t.toStdString(); tmp.addEntity(e);
                    };
                    auto mkCurve = [&](const std::vector<cad::Vec2>& v) {
                        cad::Entity e; e.type = cad::EntityType::Curve;
                        e.points = v; tmp.addEntity(e);
                    };
                    auto mkRect = [&](double x1, double z1, double x2, double z2) {
                        cad::Entity e; e.type = cad::EntityType::Rectangle;
                        e.points = {{x1, z1}, {x2, z2}}; tmp.addEntity(e);
                    };
                    auto mkPoint = [&](double x, double z) {
                        cad::Entity e; e.type = cad::EntityType::Point;
                        e.points = {{x, z}}; tmp.addEntity(e);
                    };
                    auto mkConc = [&](double x, double z, double r) {
                        cad::Entity e; e.type = cad::EntityType::Concentric;
                        e.points = {{x, z}}; e.radius = r; tmp.addEntity(e);
                    };

                    mkLine(-10, -10, 10, 10, cad::ColorIndex::Red);
                    mkCircle(0, 0, 30, cad::ColorIndex::Blue);
                    mkArc(40, 0, 20, 0, 90, cad::ColorIndex::Green);
                    mkText(0, 40, QStringLiteral("阶段4"), 8.0);
                    mkCurve({{-50, 0}, {-30, 20}, {-10, 0}, {10, 20}});
                    mkRect(-60, -40, -20, -10);
                    mkPoint(60, 60);
                    mkConc(80, 0, 25);
                    stage4Src = tmp.count();

                    const QString partP = QStringLiteral("E:/test/caddemo/preview/stage4_demo.part2d");
                    const QString dxfP  = QStringLiteral("E:/test/caddemo/preview/stage4_demo.dxf");

                    const bool pOk = cadfile::savePart(partP, &tmp);
                    CadDocument loaded; const bool lOk = cadfile::loadPart(partP, &loaded);
                    const int lc = loaded.count();
                    const bool dOk = cadfile::exportDxf(dxfP, &tmp);
                    CadDocument imp; const bool iOk = cadfile::importDxf(dxfP, &imp);
                    const int ic = imp.count();

                    stage4Part = (pOk && lOk && lc == stage4Src) ? lc : -1;
                    stage4Dxf  = (dOk && iOk && ic == stage4Src) ? ic : -1;

                    // 把真实工程文档也导出成 Part2D，证明保存可用
                    cadfile::savePart(QStringLiteral("E:/test/caddemo/preview/stage4_main.part2d"),
                                      window.document());
                }

                if (QLabel* hint = window.findChild<QLabel*>(QStringLiteral("statusHint"))) {
                    hint->setText(
                        QStringLiteral("阶段3：偏移/修剪/延伸/箭头/平移/对称/标注/颜色已执行 · 撤销 %1→%2 · XY\n"
                                       "阶段4：Part2D 往返 %3/%5 · DXF 往返 %4/%5")
                            .arg(beforeUndo).arg(afterUndo)
                            .arg(stage4Part).arg(stage4Dxf).arg(stage4Src));
                }
            });
        }
        QTimer::singleShot(500, [&window, screenshotPath]() {
            window.grab().save(screenshotPath, "PNG");
            QCoreApplication::quit();
        });
    }

    return QApplication::exec();
}
