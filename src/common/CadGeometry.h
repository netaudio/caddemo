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

// ---------------------------------------------------------------------------
// 编辑变换：直接修改图元副本（平移 / 旋转 / 缩放 / 镜像）
// 供阶段 3 的编辑命令使用，全部在数学坐标（X-Z，Z 向上）下完成
// ---------------------------------------------------------------------------
inline Vec2 rotateAround(const Vec2& p, const Vec2& c, double deg)
{
    const double r = degToRad(deg);
    const double cs = std::cos(r), sn = std::sin(r);
    const Vec2 d = p - c;
    return {c.x + d.x * cs - d.z * sn, c.z + d.x * sn + d.z * cs};
}

inline void transformTranslate(Entity& e, const Vec2& d)
{
    for (Vec2& p : e.points)
        p = p + d;
}

inline void transformRotate(Entity& e, const Vec2& c, double deg)
{
    for (Vec2& p : e.points)
        p = rotateAround(p, c, deg);
    if (e.type == EntityType::Arc)
        e.startAngle = wrap360(e.startAngle + deg);
}

inline void transformScale(Entity& e, const Vec2& c, double factor)
{
    for (Vec2& p : e.points)
        p = c + (p - c) * factor;
    if (e.type == EntityType::Circle || e.type == EntityType::Arc
        || e.type == EntityType::Concentric) {
        e.radius *= factor;
        e.innerRadius *= factor;
    }
    if (e.type == EntityType::Text)
        e.textHeight *= factor;
}

// 关于直线 a-b 镜像（反射）。圆弧通过反射起/中/末三点重建，保持方向正确
inline void transformMirror(Entity& e, const Vec2& a, const Vec2& b)
{
    auto reflect = [a, b](Vec2 p) {
        const Vec2 v = b - a;
        const double den = dot(v, v);
        if (den < kEps)
            return p;
        const double t = dot(p - a, v) / den;
        return p - v * (2.0 * t);
    };
    for (Vec2& p : e.points)
        p = reflect(p);
    if (e.type == EntityType::Arc) {
        const Vec2 c = e.points[0];
        const double r = e.radius;
        const double a1 = degToRad(e.startAngle);
        const double am = degToRad(e.startAngle + (e.endAngle - e.startAngle) * 0.5);
        const Vec2 s  = c + Vec2{r * std::cos(a1), r * std::sin(a1)};
        const Vec2 m  = c + Vec2{r * std::cos(am), r * std::sin(am)};
        const Vec2 en = c + Vec2{r * std::cos(degToRad(e.endAngle)),
                                r * std::sin(degToRad(e.endAngle))};
        ArcData arc = arcFrom3Points(reflect(s), reflect(m), reflect(en));
        if (arc.valid) {
            e.points[0] = arc.center;
            e.radius = arc.radius;
            e.startAngle = arc.startAngle;
            e.endAngle = arc.startAngle + arc.span;
        }
    }
}

// ---------------------------------------------------------------------------
// 偏移：生成与源图元平行/同心的新图元
// ---------------------------------------------------------------------------
inline std::vector<Vec2> offsetPolyline(const std::vector<Vec2>& pts, double dist,
                                        bool left, bool closed = false)
{
    const int sgn = left ? 1 : -1;
    const std::size_t n = pts.size();
    if (n < 2)
        return pts;
    struct Seg { Vec2 a, b; };
    std::vector<Seg> segs;
    const std::size_t count = closed ? n : n - 1;
    for (std::size_t i = 0; i < count; ++i) {
        const Vec2 a = pts[i];
        const Vec2 b = pts[(i + 1) % n];
        const Vec2 dir = normalized(b - a);
        const Vec2 normal = perp(dir) * (dist * sgn);
        segs.push_back({a + normal, b + normal});
    }
    if (segs.size() == 1)
        return {segs[0].a, segs[0].b};
    std::vector<Vec2> result;
    result.push_back(segs.front().a);
    for (std::size_t i = 0; i + 1 < segs.size(); ++i) {
        Vec2 join;
        if (lineIntersection(segs[i].a, segs[i].b, segs[i + 1].a, segs[i + 1].b, join))
            result.push_back(join);
        else
            result.push_back(segs[i].b);
    }
    result.push_back(segs.back().b);
    if (closed) {
        Vec2 join;
        if (lineIntersection(segs.back().a, segs.back().b,
                             segs.front().a, segs.front().b, join))
            result.push_back(join);
    }
    return result;
}

inline bool offsetEntity(const Entity& src, double dist, bool left, Entity& out)
{
    out = src;
    if (src.type == EntityType::Line || src.type == EntityType::Curve) {
        if (src.points.size() < 2)
            return false;
        out.points = offsetPolyline(src.points, dist, left, false);
        return out.points.size() >= 2;
    }
    if (src.type == EntityType::Rectangle) {
        if (src.points.size() < 2)
            return false;
        const Vec2 a = src.points[0], c = src.points[1];
        const Vec2 b{c.x, a.z}, d{a.x, c.z};
        std::vector<Vec2> rect{a, b, c, d};
        auto off = offsetPolyline(rect, dist, left, true);
        if (off.size() < 4)
            return false;
        out.points = {off[0], off[2]};
        return true;
    }
    if (src.type == EntityType::Circle || src.type == EntityType::Concentric) {
        out.radius = std::max(0.0, src.radius + (left ? dist : -dist));
        if (src.type == EntityType::Concentric)
            out.innerRadius = std::max(0.0, src.innerRadius + (left ? dist : -dist));
        return out.radius > 0.0;
    }
    if (src.type == EntityType::Arc) {
        out.radius = std::max(0.0, src.radius + (left ? dist : -dist));
        return out.radius > 0.0;
    }
    return false;
}

// ---------------------------------------------------------------------------
// 修剪 / 延伸：实体求交与裁切
// ---------------------------------------------------------------------------
inline bool pointOnSegment(const Vec2& p, const Vec2& a, const Vec2& b)
{
    const Vec2 ab = b - a, ap = p - a;
    const double denom = dot(ab, ab);
    if (denom < kEps)
        return distance(p, a) < kEps * 10.0;
    const double t = dot(ap, ab) / denom;
    if (t < -kEps || t > 1.0 + kEps)
        return false;
    return distance(p, a + ab * t) < kEps * 10.0;
}

// 直线（无限延长）与圆的所有实交点，t 不裁剪
inline std::vector<Vec2> lineCircleRaw(const Vec2& p1, const Vec2& p2,
                                       const Vec2& c, double r)
{
    std::vector<Vec2> res;
    const Vec2 d = p2 - p1;
    const Vec2 f = p1 - c;
    const double a = dot(d, d);
    if (a < kEps)
        return res;
    const double b = 2.0 * dot(f, d);
    const double cc = dot(f, f) - r * r;
    double disc = b * b - 4.0 * a * cc;
    if (disc < 0.0)
        return res;
    disc = std::sqrt(disc);
    for (double t : {(-b - disc) / (2.0 * a), (-b + disc) / (2.0 * a)})
        res.push_back(p1 + d * t);
    return res;
}

// 两图元的所有交点：线段类裁剪到线段内，圆弧按角度过滤
inline std::vector<Vec2> intersectPair(const Entity& a, const Entity& b)
{
    std::vector<Vec2> res;
    const bool aLine = a.type == EntityType::Line || a.type == EntityType::Curve;
    const bool bLine = b.type == EntityType::Line || b.type == EntityType::Curve;
    const bool aC = a.type == EntityType::Circle || a.type == EntityType::Arc
                 || a.type == EntityType::Concentric;
    const bool bC = b.type == EntityType::Circle || b.type == EntityType::Arc
                 || b.type == EntityType::Concentric;
    if (aLine && bLine && a.points.size() >= 2 && b.points.size() >= 2) {
        Vec2 p;
        if (lineIntersection(a.points[0], a.points[1], b.points[0], b.points[1], p))
            res.push_back(p);
    } else if (aLine && bC && a.points.size() >= 2) {
        const std::vector<Vec2> raw = lineCircleRaw(a.points[0], a.points[1],
                                                    b.points[0], b.radius);
        const bool arc = b.type == EntityType::Arc;
        for (const Vec2& p : raw) {
            if (!pointOnSegment(p, a.points[0], a.points[1]))
                continue;
            if (arc && !angleWithinArc(p, b.points[0], b.startAngle,
                                       b.endAngle - b.startAngle))
                continue;
            res.push_back(p);
        }
    } else if (bLine && aC && b.points.size() >= 2) {
        const std::vector<Vec2> raw = lineCircleRaw(b.points[0], b.points[1],
                                                    a.points[0], a.radius);
        const bool arc = a.type == EntityType::Arc;
        for (const Vec2& p : raw) {
            if (!pointOnSegment(p, b.points[0], b.points[1]))
                continue;
            if (arc && !angleWithinArc(p, a.points[0], a.startAngle,
                                       a.endAngle - a.startAngle))
                continue;
            res.push_back(p);
        }
    }
    return res;
}

// 用 cutting 图元修剪 target 线段，丢弃靠近 pick 的那一端
inline bool trimLineAt(const Entity& target, const Entity& cutting,
                       const Vec2& pick, Entity& out)
{
    if (target.type != EntityType::Line || target.points.size() < 2)
        return false;
    const std::vector<Vec2> pts = intersectPair(target, cutting);
    if (pts.empty())
        return false;
    Vec2 best;
    double bestD = 1e12;
    for (const Vec2& p : pts) {
        const double dd = distance(p, pick);
        if (dd < bestD) { bestD = dd; best = p; }
    }
    const Vec2 a = target.points[0], b = target.points[1];
    out = target;
    if (distance(a, pick) < distance(b, pick))
        out.points = {best, b};
    else
        out.points = {a, best};
    return true;
}

// 把 target 线段延伸到与 boundary 相交处（取段外、最近的交点）
inline bool extendLineTo(const Entity& target, const Entity& boundary, Entity& out)
{
    if (target.type != EntityType::Line || target.points.size() < 2)
        return false;
    const Vec2 a = target.points[0], b = target.points[1];
    std::vector<Vec2> pts;
    if (boundary.type == EntityType::Line && boundary.points.size() >= 2) {
        Vec2 p;
        if (lineIntersection(a, b, boundary.points[0], boundary.points[1], p))
            pts.push_back(p);
    } else if (boundary.type == EntityType::Circle || boundary.type == EntityType::Arc
               || boundary.type == EntityType::Concentric) {
        pts = lineCircleRaw(a, b, boundary.points[0], boundary.radius);
        if (boundary.type == EntityType::Arc) {
            std::vector<Vec2> f;
            for (const Vec2& p : pts)
                if (angleWithinArc(p, boundary.points[0], boundary.startAngle,
                                   boundary.endAngle - boundary.startAngle))
                    f.push_back(p);
            pts = f;
        }
    }
    Vec2 best;
    bool found = false;
    double bestScore = 1e12;
    const Vec2 ab = b - a;
    const double denom = dot(ab, ab);
    for (const Vec2& p : pts) {
        const double t = denom > kEps ? dot(p - a, ab) / denom : 0.0;
        if (t >= -kEps && t <= 1.0 + kEps)
            continue;                       // 段内不算延伸
        const double score = std::min(distance(p, a), distance(p, b));
        if (score < bestScore) { bestScore = score; best = p; found = true; }
    }
    if (!found)
        return false;
    out = target;
    if (distance(best, a) < distance(best, b))
        out.points = {best, b};
    else
        out.points = {a, best};
    return true;
}

// ---------------------------------------------------------------------------
// 阶段 5：特定功能几何辅助（切线 / 轮廓 / 测量）
// ---------------------------------------------------------------------------

// 折线段（用于轮廓串联与长度测量）
struct Segment {
    Vec2 a;
    Vec2 b;
};

// 从 from 点向以 c 为圆心、r 为半径的圆作切线，返回两个切点（圆上）。
// from 在圆上或圆内时无切线（返回空）。切线即过 from 与各切点的直线。
inline std::vector<Vec2> tangentsFromPoint(const Vec2& from, const Vec2& c, double r)
{
    std::vector<Vec2> res;
    const Vec2 d = c - from;
    const double dist = length(d);
    if (dist <= r + kEps || r <= 0.0)          // 原点在圆上/内或半径非法
        return res;
    const double L = std::sqrt(dist * dist - r * r);   // |from - 切点|
    const Vec2 dir = d / dist;
    const double a = std::asin(std::min(1.0, r / dist));   // 切线与 OC 的夹角
    for (double s : {1.0, -1.0}) {
        const double ang = s * a;
        const double cs = std::cos(ang), sn = std::sin(ang);
        const Vec2 u{dir.x * cs - dir.z * sn, dir.x * sn + dir.z * cs};
        res.push_back(from + u * L);
    }
    return res;
}

// 圆弧的带符号角度跨度（度），非圆弧返回 0
inline double arcSpanDeg(const Entity& e)
{
    if (e.type != EntityType::Arc)
        return 0.0;
    return std::abs(e.endAngle - e.startAngle);
}

// 把图元拆成若干直线段（线段 / 曲线按现有点；矩形 4 边；圆弧按跨度的均匀采样）
// 圆 / 同心圆 / 点 / 文本 / 标注不返回开放链段
inline std::vector<Segment> entitySegments(const Entity& e)
{
    std::vector<Segment> segs;
    auto push = [&](const Vec2& a, const Vec2& b) {
        if (distance(a, b) > kEps)
            segs.push_back({a, b});
    };
    switch (e.type) {
    case EntityType::Line:
    case EntityType::Curve:
        for (std::size_t i = 1; i < e.points.size(); ++i)
            push(e.points[i - 1], e.points[i]);
        break;
    case EntityType::Rectangle:
        if (e.points.size() >= 2) {
            const Vec2 a = e.points[0], c = e.points[1];
            const Vec2 b{c.x, a.z}, d{a.x, c.z};
            push(a, b); push(b, c); push(c, d); push(d, a);
        }
        break;
    case EntityType::Arc: {
        if (e.points.empty() || e.radius <= 0.0)
            break;
        const Vec2 c = e.points.front();
        const double span = e.endAngle - e.startAngle;
        const int n = std::max(2, static_cast<int>(std::abs(span) / 6.0) + 1);
        Vec2 prev = c + Vec2{e.radius * std::cos(degToRad(e.startAngle)),
                             e.radius * std::sin(degToRad(e.startAngle))};
        for (int i = 1; i <= n; ++i) {
            const double ang = degToRad(e.startAngle + span * static_cast<double>(i) / n);
            const Vec2 p = c + Vec2{e.radius * std::cos(ang), e.radius * std::sin(ang)};
            push(prev, p);
            prev = p;
        }
        break;
    }
    default:
        break;
    }
    return segs;
}

// 把若干折线段按端点衔接串联成一条有序折线（eps 为衔接容差，单位 mm）。
// 优先从「自由端点」（仅一处衔接）起步；无法衔接的段丢弃。
inline std::vector<Vec2> chainContour(std::vector<Segment> segs, double eps = 1e-3)
{
    if (segs.empty())
        return {};
    const auto eq = [eps](const Vec2& p, const Vec2& q) { return distance(p, q) <= eps; };

    // 统计每段端点的衔接次数，挑选一处衔接最少的段作为起点
    std::vector<int> deg(segs.size(), 0);
    for (std::size_t i = 0; i < segs.size(); ++i)
        for (std::size_t j = 0; j < segs.size(); ++j)
            if (i != j && (eq(segs[j].a, segs[i].a) || eq(segs[j].b, segs[i].a)
                            || eq(segs[j].a, segs[i].b) || eq(segs[j].b, segs[i].b)))
                deg[i]++;
    int seed = 0;
    for (std::size_t i = 0; i < segs.size(); ++i)
        if (deg[i] <= 1) { seed = static_cast<int>(i); break; }

    auto findNext = [&](const Vec2& end) -> int {
        for (std::size_t i = 0; i < segs.size(); ++i) {
            if (used[i])
                continue;
            if (eq(segs[i].a, end))
                return static_cast<int>(i);
            if (eq(segs[i].b, end)) {
                std::swap(segs[i].a, segs[i].b);
                return static_cast<int>(i);
            }
        }
        return -1;
    };

    std::vector<bool> used(segs.size(), false);
    std::vector<Vec2> pts;
    pts.push_back(segs[seed].a);
    pts.push_back(segs[seed].b);
    used[seed] = true;
    while (true) {
        const int nxt = findNext(pts.back());
        if (nxt < 0)
            break;
        used[nxt] = true;
        pts.push_back(segs[nxt].b);
    }
    return pts;
}

}  // namespace cad
