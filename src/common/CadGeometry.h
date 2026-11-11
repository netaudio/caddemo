#pragma once

// 纯 C++ 几何工具（不依赖 Qt），客户端渲染/拾取与后续服务端计算共用
// 阶段 2：补充绘图命令需要的构造几何（圆弧、折线等分、倒角、圆角）
#include "CadProtocol.h"

#include <algorithm>
#include <cmath>

namespace cad {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEps = 1e-9;

inline Vec2 operator+(const Vec2& a, const Vec2& b) { return {a.x + b.x, a.z + b.z}; }
inline Vec2 operator-(const Vec2& a, const Vec2& b) { return {a.x - b.x, a.z - b.z}; }
inline Vec2 operator*(const Vec2& a, double k)      { return {a.x * k, a.z * k}; }
inline Vec2 operator*(double k, const Vec2& a)      { return a * k; }

inline double dot(const Vec2& a, const Vec2& b)  { return a.x * b.x + a.z * b.z; }
inline double cross(const Vec2& a, const Vec2& b) { return a.x * b.z - a.z * b.x; }
inline double length(const Vec2& a)              { return std::hypot(a.x, a.z); }
inline double distance(const Vec2& a, const Vec2& b) { return length(a - b); }

// 逆时针旋转 90°（在 X-Z 平面内，Z 向上）
inline Vec2 perp(const Vec2& v) { return {-v.z, v.x}; }

inline Vec2 normalized(const Vec2& v)
{
    const double len = length(v);
    return len > kEps ? v * (1.0 / len) : Vec2{0.0, 0.0};
}

inline double degToRad(double degrees) { return degrees * kPi / 180.0; }
inline double radToDeg(double radians) { return radians * 180.0 / kPi; }
inline double wrap360(double angle)
{
    double result = std::fmod(angle, 360.0);
    if (result < 0.0)
        result += 360.0;
    return result;
}

inline double angleOf(const Vec2& v) { return radToDeg(std::atan2(v.z, v.x)); }

// ---------------------------------------------------------------------------
// 包围盒
// ---------------------------------------------------------------------------
struct Bounds {
    double minX = 0.0;
    double maxX = 0.0;
    double minZ = 0.0;
    double maxZ = 0.0;
    bool   valid = false;

    double width()  const { return valid ? maxX - minX : 0.0; }
    double height() const { return valid ? maxZ - minZ : 0.0; }
    Vec2 center()   const { return {(minX + maxX) / 2.0, (minZ + maxZ) / 2.0}; }
};

inline void extend(Bounds& bounds, const Vec2& p)
{
    if (!bounds.valid) {
        bounds = {p.x, p.x, p.z, p.z, true};
        return;
    }
    bounds.minX = std::min(bounds.minX, p.x);
    bounds.maxX = std::max(bounds.maxX, p.x);
    bounds.minZ = std::min(bounds.minZ, p.z);
    bounds.maxZ = std::max(bounds.maxZ, p.z);
}

inline Bounds entityBounds(const Entity& entity)
{
    Bounds bounds;
    for (const Vec2& p : entity.points)
        extend(bounds, p);

    const bool radial = entity.type == EntityType::Circle
                     || entity.type == EntityType::Arc
                     || entity.type == EntityType::Concentric;
    if (radial && !entity.points.empty() && entity.radius > 0.0) {
        const Vec2 c = entity.points.front();
        extend(bounds, {c.x - entity.radius, c.z - entity.radius});
        extend(bounds, {c.x + entity.radius, c.z + entity.radius});
    }
    if (entity.type == EntityType::Text && !entity.points.empty()) {
        const Vec2 p = entity.points.front();
        extend(bounds, {p.x + entity.textHeight * entity.text.size(), p.z + entity.textHeight});
    }
    return bounds;
}

// ---------------------------------------------------------------------------
// 圆 / 圆弧构造
// startAngle 单位为度（数学正向，X 轴向右为零度），span 带符号：
//   span > 0 逆时针，span < 0 顺时针；span = 360 表示整圆
// ---------------------------------------------------------------------------
struct CircleData {
    Vec2   center;
    double radius = 0.0;
    bool   valid = false;
};

struct ArcData {
    Vec2   center;
    double radius = 0.0;
    double startAngle = 0.0;
    double span = 0.0;
    bool   valid = false;
};

inline CircleData circleFrom3Points(const Vec2& a, const Vec2& b, const Vec2& c)
{
    const double den = 2.0 * (a.x * (b.z - c.z) + b.x * (c.z - a.z) + c.x * (a.z - b.z));
    if (std::abs(den) < kEps)
        return {};   // 三点共线
    const double a2 = a.x * a.x + a.z * a.z;
    const double b2 = b.x * b.x + b.z * b.z;
    const double c2 = c.x * c.x + c.z * c.z;
    const Vec2 center{
        (a2 * (b.z - c.z) + b2 * (c.z - a.z) + c2 * (a.z - b.z)) / den,
        (a2 * (c.x - b.x) + b2 * (a.x - c.x) + c2 * (b.x - a.x)) / den,
    };
    return {center, distance(center, a), true};
}

inline ArcData arcFrom3Points(const Vec2& p1, const Vec2& p2, const Vec2& p3)
{
    const CircleData circle = circleFrom3Points(p1, p2, p3);
    if (!circle.valid)
        return {};
    const double a1 = angleOf(p1 - circle.center);
    const double a2 = angleOf(p2 - circle.center);
    const double a3 = angleOf(p3 - circle.center);
    const double ccw = wrap360(a3 - a1);
    const double middle = wrap360(a2 - a1);
    if (std::abs(ccw) < kEps)
        return {circle.center, circle.radius, a1, 360.0, true};
    const double span = (middle <= ccw) ? ccw : ccw - 360.0;
    return {circle.center, circle.radius, a1, span, true};
}

// 由弦两端点与半径构造劣弧；radius 为负时取另一侧（顺时针）
inline ArcData arcFromChordRadius(const Vec2& p1, const Vec2& p2, double radius)
{
    const double chord = distance(p1, p2);
    const double sign = radius < 0.0 ? -1.0 : 1.0;
    const double r = std::max(std::abs(radius), chord / 2.0);
    if (chord < kEps || !std::isfinite(r) || r < kEps)
        return {};
    const Vec2 mid = (p1 + p2) * 0.5;
    const double h = std::sqrt(std::max(0.0, r * r - chord * chord / 4.0));
    const Vec2 center = mid + perp(normalized(p2 - p1)) * (h * sign);
    const double start = angleOf(p1 - center);
    const double span = sign * 2.0 * radToDeg(
        std::asin(std::clamp(chord / (2.0 * r), 0.0, 1.0)));
    return {center, r, start, span, true};
}

// 由弦两端点与圆心角（度）构造圆弧；正角度逆时针，负角度顺时针
inline ArcData arcFromChordAngle(const Vec2& p1, const Vec2& p2, double angleDegrees)
{
    const double chord = distance(p1, p2);
    if (chord < kEps)
        return {};
    const double sign = angleDegrees < 0.0 ? -1.0 : 1.0;
    const double theta = std::clamp(std::abs(angleDegrees), 1.0, 179.0);
    const double half = degToRad(theta) / 2.0;
    const double r = chord / (2.0 * std::sin(half));
    const double h = chord / (2.0 * std::tan(half));
    const Vec2 center = (p1 + p2) * 0.5 + perp(normalized(p2 - p1)) * (h * sign);
    return {center, r, angleOf(p1 - center), sign * theta, true};
}

// ---------------------------------------------------------------------------
// 折线等分
// ---------------------------------------------------------------------------
inline double polylineLength(const std::vector<Vec2>& points)
{
    double total = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i)
        total += distance(points[i - 1], points[i]);
    return total;
}

// 沿折线按弧长取点（target 超出总长时返回末端）
inline Vec2 pointAtLength(const std::vector<Vec2>& points, double target)
{
    if (points.empty())
        return {};
    if (points.size() == 1 || target <= 0.0)
        return points.front();
    double remaining = target;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const double seg = distance(points[i - 1], points[i]);
        if (remaining <= seg)
            return points[i - 1] + normalized(points[i] - points[i - 1]) * remaining;
        remaining -= seg;
    }
    return points.back();
}

// Catmull-Rom 插值：把稀疏控制点变成平滑曲线（以密集折线存储，便于拾取/服务端交换）
inline std::vector<Vec2> smoothCatmullRom(const std::vector<Vec2>& points,
                                          int samplesPerSegment = 10)
{
    if (points.size() < 3)
        return points;
    std::vector<Vec2> result;
    result.reserve((points.size() - 1) * static_cast<std::size_t>(samplesPerSegment) + 1);
    const auto at = [&points](long long index) -> Vec2 {
        const long long last = static_cast<long long>(points.size()) - 1;
        return points[static_cast<std::size_t>(std::clamp(index, 0LL, last))];
    };
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        const Vec2 p0 = at(static_cast<long long>(i) - 1);
        const Vec2 p1 = points[i];
        const Vec2 p2 = points[i + 1];
        const Vec2 p3 = at(static_cast<long long>(i) + 2);
        for (int s = 0; s < samplesPerSegment; ++s) {
            const double t = static_cast<double>(s) / samplesPerSegment;
            const double t2 = t * t;
            const double t3 = t2 * t;
            result.push_back(0.5 * ((2.0 * p1) + (p2 - p0) * t
                                    + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2
                                    + (p0 * -1.0 + 3.0 * p1 - 3.0 * p2 + p3) * t3));
        }
    }
    result.push_back(points.back());
    return result;
}

// ---------------------------------------------------------------------------
// 直线求交（无限延长）
// ---------------------------------------------------------------------------
inline bool lineIntersection(const Vec2& a, const Vec2& b,
                             const Vec2& c, const Vec2& d, Vec2& out)
{
    const Vec2 u = b - a;
    const Vec2 v = d - c;
    const double den = cross(u, v);
    if (std::abs(den) < kEps)
        return false;
    const double t = cross(c - a, v) / den;
    out = a + u * t;
    return true;
}

// ---------------------------------------------------------------------------
// 倒角 / 圆角：输入两条线段图元，输出修剪后的两条线与连接图元
// near 端（靠近交点的端点）被替换为切点，far 端保持不变
// ---------------------------------------------------------------------------
struct CornerResult {
    Entity lineA;
    Entity lineB;
    Entity corner;      // 倒角：Line；圆角：Arc
    bool   valid = false;
};

inline Entity cloneStyle(const Entity& from)
{
    Entity style;
    style.color = from.color;
    style.lineType = from.lineType;
    style.lineWidth = from.lineWidth;
    style.textHeight = from.textHeight;
    style.layer = from.layer;
    return style;
}

// 把靠近交点 intersection 的端点替换为 trimmed，另一端保持不变
inline Entity trimLineTowards(const Entity& line, const Vec2& intersection,
                              const Vec2& trimmed)
{
    Entity result = line;
    if (result.points.size() < 2)
        return result;
    if (distance(result.points.front(), intersection) <= distance(result.points.back(), intersection))
        result.points.front() = trimmed;
    else
        result.points.back() = trimmed;
    return result;
}

// 线段上「靠近交点」的那个端点的方向单位向量（从交点指向端点）
inline Vec2 nearDirection(const Entity& line, const Vec2& intersection)
{
    const Vec2 near = distance(line.points.front(), intersection) <=
                              distance(line.points.back(), intersection)
                          ? line.points.front() : line.points.back();
    return normalized(near - intersection);
}

inline CornerResult chamferLines(const Entity& a, const Entity& b, double distance1,
                                 double distance2)
{
    CornerResult result;
    if (a.points.size() < 2 || b.points.size() < 2)
        return result;
    Vec2 p;
    if (!lineIntersection(a.points[0], a.points[1], b.points[0], b.points[1], p))
        return result;
    const Vec2 u1 = nearDirection(a, p);
    const Vec2 u2 = nearDirection(b, p);
    if (length(u1) < kEps || length(u2) < kEps)
        return result;
    const double d1 = std::max(0.0, distance1);
    const double d2 = distance2 < 0.0 ? d1 : std::max(0.0, distance2);
    const Vec2 t1 = p + u1 * d1;
    const Vec2 t2 = p + u2 * d2;

    result.lineA = trimLineTowards(a, p, t1);
    result.lineB = trimLineTowards(b, p, t2);
    result.corner = cloneStyle(a);
    result.corner.type = EntityType::Line;
    result.corner.points = {t1, t2};
    result.valid = true;
    return result;
}

inline CornerResult filletLines(const Entity& a, const Entity& b, double radius)
{
    CornerResult result;
    if (a.points.size() < 2 || b.points.size() < 2 || radius <= 0.0)
        return result;
    Vec2 p;
    if (!lineIntersection(a.points[0], a.points[1], b.points[0], b.points[1], p))
        return result;
    const Vec2 u1 = nearDirection(a, p);
    const Vec2 u2 = nearDirection(b, p);
    if (length(u1) < kEps || length(u2) < kEps)
        return result;
    const double cosPhi = std::clamp(dot(u1, u2), -1.0, 1.0);
    const double phi = std::acos(cosPhi);
    if (phi < 1e-3 || phi > kPi - 1e-3)
        return result;                       // 平行或共线
    const double half = phi / 2.0;
    const double tangent = radius / std::tan(half);
    const Vec2 t1 = p + u1 * tangent;
    const Vec2 t2 = p + u2 * tangent;
    const Vec2 bisector = normalized(u1 + u2);
    const Vec2 center = p + bisector * (radius / std::sin(half));

    result.lineA = trimLineTowards(a, p, t1);
    result.lineB = trimLineTowards(b, p, t2);

    const double a1 = angleOf(t1 - center);
    const double a2 = angleOf(t2 - center);
    double span = wrap360(a2 - a1);
    if (span > 180.0)
        span -= 360.0;
    result.corner = cloneStyle(a);
    result.corner.type = EntityType::Arc;
    result.corner.points = {center};
    result.corner.radius = radius;
    result.corner.startAngle = a1;
    result.corner.endAngle = a1 + span;   // 约定：endAngle - startAngle = 带符号 span
    result.valid = true;
    return result;
}

// ---------------------------------------------------------------------------
// 拾取距离
// ---------------------------------------------------------------------------
inline double distancePointToSegment(const Vec2& p, const Vec2& a, const Vec2& b)
{
    const Vec2 ab = b - a;
    const double len2 = dot(ab, ab);
    if (len2 <= 0.0)
        return distance(p, a);
    const double t = std::clamp(dot(p - a, ab) / len2, 0.0, 1.0);
    return distance(p, a + ab * t);
}

// 判断世界点角度是否落在圆弧范围内（span 带符号）
inline bool angleWithinArc(const Vec2& p, const Vec2& center, double start, double span)
{
    const double a = angleOf(p - center);
    if (std::abs(std::abs(span) - 360.0) < 1e-6)
        return true;
    return span >= 0.0 ? wrap360(a - start) <= span
                       : wrap360(start - a) <= -span;
}

inline double distanceToEntity(const Vec2& p, const Entity& entity)
{
    if (entity.points.empty())
        return 1e12;

    const double span = entity.endAngle - entity.startAngle;
    double best = 1e12;
    switch (entity.type) {
    case EntityType::Point:
    case EntityType::Text:
        best = distance(p, entity.points.front());
        break;
    case EntityType::Circle:
    case EntityType::Arc:
    case EntityType::Concentric: {
        const Vec2 c = entity.points.front();
        const double d = distance(p, c);
        if (entity.type == EntityType::Arc) {
            if (angleWithinArc(p, c, entity.startAngle, span))
                best = std::abs(d - entity.radius);
            best = std::min(best, distance(p, c + Vec2{
                entity.radius * std::cos(degToRad(entity.startAngle)),
                entity.radius * std::sin(degToRad(entity.startAngle))}));
            best = std::min(best, distance(p, c + Vec2{
                entity.radius * std::cos(degToRad(entity.endAngle)),
                entity.radius * std::sin(degToRad(entity.endAngle))}));
        } else {
            best = std::abs(d - entity.radius);
        }
        if (entity.type == EntityType::Concentric) {
            const double inner = entity.innerRadius > 0.0
                                     ? entity.innerRadius : entity.radius * 0.5;
            best = std::min(best, std::abs(d - inner));
        }
        break;
    }
    case EntityType::Rectangle: {
        if (entity.points.size() >= 2) {
            const Vec2 a = entity.points[0];
            const Vec2 b = entity.points[1];
            best = std::min({distancePointToSegment(p, a, {b.x, a.z}),
                             distancePointToSegment(p, {b.x, a.z}, b),
                             distancePointToSegment(p, b, {a.x, b.z}),
                             distancePointToSegment(p, {a.x, b.z}, a)});
        }
        break;
    }
    case EntityType::Line:
    case EntityType::Curve:
    case EntityType::Dimension:
        for (std::size_t i = 1; i < entity.points.size(); ++i)
            best = std::min(best, distancePointToSegment(p, entity.points[i - 1], entity.points[i]));
        break;
    }
    return best;
}

}  // namespace cad
