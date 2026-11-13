#include "CadIcons.h"

#include <QFont>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>

namespace {

constexpr int kSize = 20;

const QColor kRed("#cf2b23");
const QColor kBlue("#2b4f9e");
const QColor kDark("#222222");
const QColor kYellow("#f2c44b");
const QColor kPink("#d96fb5");

void drawCrossArrow(QPainter& p, const QColor& c)
{
    p.setPen(QPen(c, 1.4, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(10, 2, 10, 18);
    p.drawLine(2, 10, 18, 10);
    p.setBrush(c);
    p.setPen(Qt::NoPen);
    p.drawPolygon(QPolygonF() << QPointF(10, 0) << QPointF(7, 4) << QPointF(13, 4));
    p.drawPolygon(QPolygonF() << QPointF(10, 20) << QPointF(7, 16) << QPointF(13, 16));
    p.drawPolygon(QPolygonF() << QPointF(0, 10) << QPointF(4, 7) << QPointF(4, 13));
    p.drawPolygon(QPolygonF() << QPointF(20, 10) << QPointF(16, 7) << QPointF(16, 13));
}

void drawMagnifier(QPainter& p, const QColor& c, bool plus, bool window)
{
    p.setPen(QPen(c, 1.5));
    if (window) {
        p.drawRect(QRectF(4, 4, 10, 10));
        p.drawLine(12, 13, 18, 19);
        p.drawLine(15, 16, 19, 16);
    } else {
        p.drawEllipse(QRectF(2.5, 2.5, 11, 11));
        p.drawLine(12.5, 12.5, 18, 18);
        p.drawLine(6, 8, 10, 8);
        if (plus)
            p.drawLine(8, 6, 8, 10);
    }
}

void drawLetter(QPainter& p, const QString& s, const QColor& c, int pointSize = 12)
{
    QFont f(QStringLiteral("Arial"), pointSize, QFont::Bold);
    p.setFont(f);
    p.setPen(c);
    p.drawText(QRectF(0, 0, kSize, kSize), Qt::AlignCenter, s);
}

void render(const QString& key, QPainter& p)
{
    p.setRenderHint(QPainter::Antialiasing, true);

    if (key == QLatin1String("dxf")) {
        drawLetter(p, QStringLiteral("dxf"), kRed, 7);
    } else if (key == QLatin1String("point")) {
        p.setPen(QPen(kRed, 1.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(5, 5, 15, 15);
        p.drawLine(15, 5, 5, 15);
    } else if (key == QLatin1String("line")) {
        p.setPen(QPen(kRed, 1.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(3, 16, 17, 3);
        p.setBrush(kRed);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(1.5, 14.5, 3, 3));
        p.drawEllipse(QRectF(15.5, 1.5, 3, 3));
    } else if (key == QLatin1String("arc")) {
        p.setPen(QPen(kRed, 1.5));
        p.drawArc(QRectF(2.5, 3, 15, 15), 20 * 16, 140 * 16);
        p.setBrush(kRed);
        p.setPen(Qt::NoPen);
        for (const QPointF& pt : {QPointF(2, 10), QPointF(17, 10), QPointF(9.5, 2)})
            p.drawEllipse(pt, 1.4, 1.4);
    } else if (key == QLatin1String("curve")) {
        QPainterPath path;
        path.moveTo(2, 14);
        path.cubicTo(6, 2, 10, 18, 18, 5);
        p.setPen(QPen(kRed, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        p.setBrush(kRed);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(2, 14), 1.4, 1.4);
        p.drawEllipse(QPointF(18, 5), 1.4, 1.4);
    } else if (key == QLatin1String("text") || key == QLatin1String("textsetting")) {
        drawLetter(p, QStringLiteral("A"), kDark, 13);
    } else if (key == QLatin1String("circle")) {
        p.setPen(QPen(kRed, 1.4));
        p.drawEllipse(QRectF(2.5, 2.5, 15, 15));
        p.setBrush(kRed);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(10, 10), 1.6, 1.6);
    } else if (key == QLatin1String("concentric")) {
        p.setPen(QPen(kRed, 1.2));
        p.drawEllipse(QRectF(1.5, 1.5, 17, 17));
        p.drawEllipse(QRectF(5.5, 5.5, 9, 9));
        p.setBrush(kRed);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(10, 10), 1.3, 1.3);
    } else if (key == QLatin1String("rect")) {
        p.setPen(QPen(kDark, 1.3));
        p.drawRect(QRectF(2.5, 4.5, 15, 11));
    } else if (key == QLatin1String("erase")) {
        p.setPen(QPen(QColor("#8a6b1f"), 1));
        p.setBrush(kYellow);
        p.drawPolygon(QPolygonF() << QPointF(3, 14) << QPointF(10, 3)
                                  << QPointF(17, 8) << QPointF(10, 18));
        p.setPen(QPen(QColor("#8a6b1f"), 1));
        p.drawLine(3, 14, 10, 18);
    } else if (key == QLatin1String("pan")) {
        drawCrossArrow(p, kDark);
    } else if (key == QLatin1String("move")) {
        drawCrossArrow(p, kRed);
    } else if (key == QLatin1String("zoomin")) {
        drawMagnifier(p, kBlue, true, false);
    } else if (key == QLatin1String("zoomout")) {
        drawMagnifier(p, kBlue, false, false);
    } else if (key == QLatin1String("zoomwindow")) {
        drawMagnifier(p, kBlue, false, true);
    } else if (key == QLatin1String("rotate")) {
        p.setPen(QPen(kRed, 1.4));
        p.drawArc(QRectF(3, 3, 14, 14), 30 * 16, 250 * 16);
        p.setBrush(kRed);
        p.setPen(Qt::NoPen);
        p.drawPolygon(QPolygonF() << QPointF(4, 14) << QPointF(3, 8) << QPointF(8, 11));
    } else if (key == QLatin1String("offset")) {
        p.setPen(QPen(kBlue, 1.3));
        p.drawPolyline(QPolygonF() << QPointF(2, 5) << QPointF(8, 5) << QPointF(8, 15) << QPointF(17, 15));
        p.setPen(QPen(kBlue, 1.3, Qt::DashLine));
        p.drawPolyline(QPolygonF() << QPointF(2, 9) << QPointF(12, 9) << QPointF(12, 19));
    } else if (key == QLatin1String("dimension")) {
        p.setPen(QPen(kRed, 1.2));
        p.drawLine(2, 13, 18, 13);
        p.drawLine(3, 10, 3, 16);
        p.drawLine(17, 10, 17, 16);
        p.setBrush(kRed);
        p.setPen(Qt::NoPen);
        p.drawPolygon(QPolygonF() << QPointF(3, 13) << QPointF(7, 11) << QPointF(7, 15));
        p.drawPolygon(QPolygonF() << QPointF(17, 13) << QPointF(13, 11) << QPointF(13, 15));
        drawLetter(p, QStringLiteral("10"), kDark, 6);
    } else if (key == QLatin1String("arrow")) {
        p.setPen(QPen(kBlue, 1.6));
        p.drawLine(3, 10, 16, 10);
        p.setBrush(kBlue);
        p.setPen(Qt::NoPen);
        p.drawPolygon(QPolygonF() << QPointF(18, 10) << QPointF(13, 6) << QPointF(13, 14));
    } else if (key == QLatin1String("color")) {
        p.setPen(QPen(kRed, 1.5));
        p.drawLine(2, 10, 18, 10);
    } else if (key == QLatin1String("linestyle")) {
        p.setPen(QPen(kDark, 1.3));
        p.drawLine(1, 10, 19, 10);
    } else if (key == QLatin1String("linewidth")) {
        p.setPen(QPen(kRed, 3.2));
        p.drawLine(2, 10, 18, 10);
    } else if (key == QLatin1String("undo") || key == QLatin1String("quickundo")) {
        p.setPen(QPen(kBlue, 1.8, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(QRectF(3, 4, 13, 12), 60 * 16, 240 * 16);
        p.setBrush(kBlue);
        p.setPen(Qt::NoPen);
        p.drawPolygon(QPolygonF() << QPointF(3, 3) << QPointF(3, 10) << QPointF(9, 6));
    } else if (key == QLatin1String("trim")) {
        p.setPen(QPen(kRed, 1.4));
        p.drawLine(3, 3, 16, 16);
        p.drawLine(16, 4, 4, 16);
        p.drawEllipse(QRectF(1, 13, 4, 4));
        p.drawEllipse(QRectF(14, 13, 4, 4));
    } else if (key == QLatin1String("extend")) {
        p.setPen(QPen(kRed, 1.6));
        p.drawLine(2, 15, 13, 4);
        p.setPen(QPen(kRed, 1.2, Qt::DashLine));
        p.drawLine(13, 4, 18, 1);
    } else if (key == QLatin1String("mirror")) {
        p.setPen(QPen(kRed, 1.2, Qt::DashLine));
        p.drawLine(10, 1, 10, 19);
        p.setPen(QPen(kDark, 1.3));
        p.drawPolyline(QPolygonF() << QPointF(7, 5) << QPointF(3, 10) << QPointF(7, 15));
        p.drawPolyline(QPolygonF() << QPointF(13, 5) << QPointF(17, 10) << QPointF(13, 15));
    } else if (key == QLatin1String("probe")) {
        p.setPen(Qt::NoPen);
        p.setBrush(kYellow);
        p.drawRoundedRect(QRectF(3, 4, 8, 8), 2, 2);
        p.setBrush(kPink);
        p.drawRoundedRect(QRectF(9, 3, 7, 7), 2, 2);
        p.setBrush(QColor("#4c8ed9"));
        p.drawRoundedRect(QRectF(8, 10, 9, 7), 2, 2);
    } else if (key == QLatin1String("pushpin")) {
        p.setPen(QPen(kDark, 1.3));
        p.drawLine(10, 8, 10, 18);
        p.setBrush(kDark);
        p.drawEllipse(QRectF(7, 3, 6, 5));
    } else if (key == QLatin1String("images")) {
        p.setPen(QPen(kBlue, 1.2));
        p.setBrush(QColor("#dce8fb"));
        p.drawRect(QRectF(2, 3, 13, 11));
        p.setBrush(QColor("#f4f8ff"));
        p.drawRect(QRectF(6, 7, 12, 10));
    } else if (key == QLatin1String("logo")) {
        p.setPen(QPen(QColor("#1b4f9c"), 1.2));
        p.setBrush(QColor("#3e86d6"));
        p.drawEllipse(QRectF(1.5, 1.5, 17, 17));
        p.setPen(QPen(QColor("#d7ecff"), 1));
        p.drawEllipse(QRectF(6, 1.5, 8, 17));
        p.drawLine(2, 10, 18, 10);
        p.drawArc(QRectF(2, 4.5, 16, 11), 0, 180 * 16);
    } else if (key == QLatin1String("save")) {
        p.setPen(QPen(kBlue, 1.2));
        p.setBrush(QColor("#9dbdf0"));
        p.drawRect(QRectF(3, 3, 14, 14));
        p.setBrush(QColor("#eaf2ff"));
        p.drawRect(QRectF(6, 4, 8, 5));
        p.drawRect(QRectF(6, 11, 8, 5));
    } else if (key == QLatin1String("grid")) {
        p.setPen(QPen(QColor("#8aa8d0"), 1));
        for (int i = 4; i <= 16; i += 6) {
            p.drawLine(i, 2, i, 18);
            p.drawLine(2, i, 18, i);
        }
    } else if (key == QLatin1String("folder")) {
        // 打开Part：黄色文件夹，与橡皮/探针的黄色系一致
        p.setPen(QPen(QColor("#8a6b1f"), 1.1));
        p.setBrush(kYellow);
        p.drawPolygon(QPolygonF() << QPointF(2, 6) << QPointF(7, 6) << QPointF(9, 8)
                                  << QPointF(18, 8) << QPointF(18, 16) << QPointF(2, 16));
        p.setPen(QPen(QColor("#a8842f"), 1));
        p.drawLine(2, 10, 17, 10);
    } else if (key == QLatin1String("saveas")) {
        // 另存Part：软盘 + 黄色铅笔
        p.setPen(QPen(kBlue, 1.2));
        p.setBrush(QColor("#9dbdf0"));
        p.drawRect(QRectF(2, 2, 13, 13));
        p.setBrush(QColor("#eaf2ff"));
        p.drawRect(QRectF(5, 3, 7, 4));
        p.drawRect(QRectF(5, 9, 7, 4));
        p.setPen(QPen(QColor("#8a6b1f"), 1));
        p.setBrush(kYellow);
        p.drawPolygon(QPolygonF() << QPointF(12, 17) << QPointF(17, 12)
                                  << QPointF(18.5, 13.5) << QPointF(13.5, 18.5));
        p.setBrush(kDark);
        p.setPen(Qt::NoPen);
        p.drawPolygon(QPolygonF() << QPointF(12, 17) << QPointF(13.5, 18.5)
                                  << QPointF(10.5, 19.5));
    } else if (key == QLatin1String("divide")) {
        // 等分点：直线段 + 等分点（端点圆圈、中间实心点）
        p.setPen(QPen(kRed, 1.4, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(2, 10, 18, 10);
        p.setBrush(kRed);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(1, 8.5, 3, 3));
        p.drawEllipse(QRectF(16, 8.5, 3, 3));
        for (double x : {6.67, 10.0, 13.33})
            p.drawEllipse(QPointF(x, 10), 1.2, 1.2);
    } else if (key == QLatin1String("centerpoint")) {
        // 圆心点：圆 + 十字中心线 + 圆心实心点
        p.setPen(QPen(kRed, 1.2));
        p.drawEllipse(QRectF(3.5, 3.5, 13, 13));
        p.setPen(QPen(kRed, 1, Qt::DashLine));
        p.drawLine(10, 4.5, 10, 15.5);
        p.drawLine(4.5, 10, 15.5, 10);
        p.setBrush(kRed);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(10, 10), 1.6, 1.6);
    } else if (key == QLatin1String("fillet")) {
        // 圆角：直角拐线以圆弧过渡
        p.setPen(QPen(kRed, 1.4, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(5, 3, 5, 13);
        p.drawArc(QRectF(5, 9, 8, 8), 180 * 16, 90 * 16);
        p.drawLine(9, 17, 17, 17);
        p.setBrush(kRed);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(5, 3), 1.4, 1.4);
        p.drawEllipse(QPointF(17, 17), 1.4, 1.4);
    } else if (key == QLatin1String("chamfer")) {
        // 倒角：直角拐线以斜切边过渡
        p.setPen(QPen(kRed, 1.4, Qt::SolidLine, Qt::RoundCap));
        p.drawPolyline(QPolygonF() << QPointF(5, 3) << QPointF(5, 11)
                                   << QPointF(11, 17) << QPointF(17, 17));
        p.setBrush(kRed);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(5, 3), 1.4, 1.4);
        p.drawEllipse(QPointF(17, 17), 1.4, 1.4);
    }
}

}  // namespace

QIcon CadIcons::icon(const QString& key)
{
    static QHash<QString, QIcon> cache;
    if (key.isEmpty())
        return QIcon();

    const auto it = cache.constFind(key);
    if (it != cache.constEnd())
        return it.value();

    QPixmap pm(kSize, kSize);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    render(key, painter);
    painter.end();

    const QIcon result(pm);
    cache.insert(key, result);
    return result;
}
