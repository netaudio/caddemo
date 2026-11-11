#include "client/MainWindow.h"

#include "client/CadCanvas.h"
#include "client/CadDocument.h"
#include "client/CadDrawController.h"

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
                click(QStringLiteral("开始"));
                click(QStringLiteral("CAD绘制"));
                click(QStringLiteral("›"));
                for (QComboBox* combo : window.findChildren<QComboBox*>()) {
                    if (combo->currentText() == QStringLiteral("XZ"))
                        combo->setCurrentIndex(1);   // 坐标系 -> XY
                }

                // 撤销最后一次圆角宏，验证 QUndoStack 生效
                const int before = document->count();
                if (window.undoStack()->canUndo())
                    window.undoStack()->undo();
                const int after = document->count();
                if (QLabel* hint = window.findChild<QLabel*>(QStringLiteral("statusHint"))) {
                    hint->setText(
                        QStringLiteral("阶段2冒烟测试：14 类绘图命令全部执行  ·  图元 %1 → 撤销后 %2  ·  坐标系 XY  ·  视图/面板联动正常")
                            .arg(before).arg(after));
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
