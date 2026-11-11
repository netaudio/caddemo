#include "CadEntityPainter.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QRectF>

#include <algorithm>
#include <cmath>

namespace cadpainter {

QColor colorOf(cad::ColorIndex color)
{
    switch (color) {
    case cad::ColorIndex::Red:    return QColor("#cf2b23");
    case cad::ColorIndex::Green:  return QColor("#1e8a4c");
    case cad::ColorIndex::Blue:   return QColor("#2b4f9e");
    case cad::ColorIndex::Yellow: return QColor("#d99a16");
    case cad::ColorIndex::White:  return QColor("#ffffff");
    case cad::ColorIndex::Black:
    default:                      return QColor("#303030");
    }
}

Qt::PenStyle penStyleOf(cad::LineType type)
{
    switch (type) {
    case cad::LineType::Dashed:     return Qt::DashLine;
    case cad::LineType::DashDot:    return Qt::DashDotLine;
    case cad::LineType::DashDotDot: return Qt::DashDotDotLine;
    case cad::LineType::Solid:
    default:                        return Qt::SolidLine;
    }
}

QPen entityPen(const cad::Entity& entity)
{
    const double width = entity.lineWidth > 0.0 ? std::max(1.0, entity.lineWidth) : 1.0;
    QPen pen(colorOf(entity.color), width, penStyleOf(entity.lineType));
    pen.setCosmetic(true);
    return pen;
}

void drawEntity(QPainter& painter, const cad::Entity& entity,
                const CadViewTransform& view, const QSizeF& viewport)
{
    if (entity.points.empty())
        return;

    const QPointF p0 = view.toScreen(entity.points.front(), viewport);
    switch (entity.type) {
    case cad::EntityType::Point: {
        painter.drawLine(p0.x() - 5, p0.y(), p0.x() + 5, p0.y());
        painter.drawLine(p0.x(), p0.y() - 5, p0.x(), p0.y() + 5);
        break;
    }
    case cad::EntityType::Line:
    case cad::EntityType::Curve: {
        QPainterPath path;
        path.moveTo(p0);
        for (std::size_t i = 1; i < entity.points.size(); ++i)
            path.lineTo(view.toScreen(entity.points[i], viewport));
        painter.drawPath(path);
        break;
    }
    case cad::EntityType::Dimension: {
        if (entity.points.size() >= 2) {
            const QPointF a = view.toScreen(entity.points[0], viewport);
            const QPointF b = view.toScreen(entity.points[1], viewport);
            painter.drawLine(a, b);
            const QPointF dir = b - a;
            const double len = std::hypot(dir.x(), dir.y());
            if (len > 1.0) {
                const QPointF u = dir / len;
                const QPointF perp(-u.y(), u.x());
                painter.drawLine(a, a + perp * 5.0);
                painter.drawLine(b, b + perp * 5.0);
            }
            QFont font(QStringLiteral("Microsoft YaHei"));
            font.setPixelSize(std::clamp(static_cast<int>(entity.textHeight * view.scale), 8, 96));
            painter.setFont(font);
            painter.drawText((a + b) / 2.0 + QPointF(4, -4), QString::fromStdString(entity.text));
        }
        break;
    }
    case cad::EntityType::Rectangle: {
        if (entity.points.size() >= 2)
            painter.drawRect(QRectF(p0, view.toScreen(entity.points[1], viewport)).normalized());
        break;
    }
    case cad::EntityType::Circle:
    case cad::EntityType::Concentric: {
        const double r = std::abs(entity.radius * view.scale);
        painter.drawEllipse(p0, r, r);
        if (entity.type == cad::EntityType::Concentric) {
            const double inner = entity.innerRadius > 0.0
                                     ? entity.innerRadius : entity.radius * 0.5;
            const double ri = std::abs(inner * view.scale);
            painter.drawEllipse(p0, ri, ri);
        }
        break;
    }
    case cad::EntityType::Arc: {
        const double r = std::abs(entity.radius * view.scale);
        const QRectF box(p0.x() - r, p0.y() - r, 2 * r, 2 * r);
        // span 带符号：Qt 正值为逆时针、负值为顺时针，与数学角度一致
        const double span = entity.endAngle - entity.startAngle;
        painter.drawArc(box, static_cast<int>(entity.startAngle * 16),
                        static_cast<int>(std::clamp(span, -360.0, 360.0) * 16));
        break;
    }
    case cad::EntityType::Text: {
        QFont font(QStringLiteral("Microsoft YaHei"));
        font.setPixelSize(std::clamp(static_cast<int>(entity.textHeight * view.scale), 8, 96));
        painter.setFont(font);
        painter.drawText(p0, QString::fromStdString(entity.text));
        break;
    }
    }
}

}  // namespace cadpainter
