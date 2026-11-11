#pragma once

// 视图变换：世界坐标（X-Z，单位 mm，Z 向上） <-> 屏幕像素
// 屏幕 Y 轴向下，因此 Z 方向取负号
#include <QPointF>
#include <QSizeF>

#include <algorithm>

#include "CadGeometry.h"
#include "CadProtocol.h"

struct CadViewTransform
{
    double scale = 1.0;      // 每世界单位对应的屏幕像素数
    double centerX = 0.0;    // 视口中心对应的世界 X
    double centerZ = 0.0;    // 视口中心对应的世界 Z

    static constexpr double kMinScale = 1e-4;
    static constexpr double kMaxScale = 1e6;

    QPointF toScreen(const cad::Vec2& p, const QSizeF& viewport) const
    {
        return QPointF(viewport.width() / 2.0 + (p.x - centerX) * scale,
                       viewport.height() / 2.0 - (p.z - centerZ) * scale);
    }

    cad::Vec2 toWorld(const QPointF& p, const QSizeF& viewport) const
    {
        return {centerX + (p.x() - viewport.width() / 2.0) / scale,
                centerZ - (p.y() - viewport.height() / 2.0) / scale};
    }

    double clampedScale(double value) const
    {
        return std::clamp(value, kMinScale, kMaxScale);
    }

    // 以屏幕锚点为中心缩放：缩放前后锚点下的世界坐标保持不变
    void zoomAt(const QPointF& anchor, double factor, const QSizeF& viewport)
    {
        const cad::Vec2 before = toWorld(anchor, viewport);
        scale = clampedScale(scale * factor);
        const cad::Vec2 after = toWorld(anchor, viewport);
        centerX += before.x - after.x;
        centerZ += before.z - after.z;
    }

    void panByPixels(const QPointF& delta)
    {
        centerX -= delta.x() / scale;
        centerZ += delta.y() / scale;
    }

    // 把世界包围框完整装入视口
    void fit(const cad::Bounds& bounds, const QSizeF& viewport, double margin = 0.85)
    {
        if (!bounds.valid || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
            scale = 1.0;
            centerX = centerZ = 0.0;
            return;
        }
        scale = clampedScale(std::min(viewport.width() / bounds.width(),
                                     viewport.height() / bounds.height()) * margin);
        const cad::Vec2 c = bounds.center();
        centerX = c.x;
        centerZ = c.z;
    }
};
