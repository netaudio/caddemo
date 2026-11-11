#include "CadDxf.h"

#include "CadDocument.h"
#include "CadProtocol.h"

#include <QFile>
#include <QSaveFile>
#include <QTextStream>
#include <vector>

namespace cadfile {
namespace {

int colorToDxf(cad::ColorIndex c)
{
    switch (c) {
    case cad::ColorIndex::Red:    return 1;
    case cad::ColorIndex::Yellow: return 2;
    case cad::ColorIndex::Green:  return 3;
    case cad::ColorIndex::Blue:   return 5;
    case cad::ColorIndex::White:  return 7;
    case cad::ColorIndex::Black:  return 7;  // DXF 7 = 黑/白（随背景）
    }
    return 7;
}

cad::ColorIndex dxfToColor(int c)
{
    switch (c) {
    case 1:  return cad::ColorIndex::Red;
    case 2:  return cad::ColorIndex::Yellow;
    case 3:  return cad::ColorIndex::Green;
    case 4:  return cad::ColorIndex::Blue;   // DXF 青 -> 蓝
    case 5:  return cad::ColorIndex::Blue;
    case 6:  return cad::ColorIndex::Red;    // DXF 品红 -> 红
    default: return cad::ColorIndex::Black;
    }
}

QString layerOf(const cad::Entity& e)
{
    return QString::fromStdString(e.layer.empty() ? "0" : e.layer);
}

// 矩形对角点 -> 4 角点
std::vector<cad::Vec2> rectCorners(const cad::Entity& e)
{
    const cad::Vec2 a = e.points.empty() ? cad::Vec2{0, 0} : e.points[0];
    const cad::Vec2 b = e.points.size() > 1 ? e.points[1] : a;
    return { {a.x, a.z}, {b.x, a.z}, {b.x, b.z}, {a.x, b.z} };
}

}  // namespace

bool exportDxf(const QString& path, const CadDocument* doc)
{
    if (!doc)
        return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QTextStream ts(&file);

    auto W  = [&](int code, const QString& value) {
        ts << code << "\n" << value << "\n";
    };
    auto WD = [&](int code, double value) {
        ts << code << "\n" << QString::number(value, 'f', 6) << "\n";
    };
    auto head = [&](const cad::Entity& e, const QString& type) {
        ts << "0\n" << type << "\n";
        W(8, layerOf(e));
        W(62, QString::number(colorToDxf(e.color)));
    };
    auto poly = [&](const std::vector<cad::Vec2>& verts, bool closed, const cad::Entity& e) {
        ts << "0\nLWPOLYLINE\n";
        W(8, layerOf(e));
        W(90, QString::number(closed ? static_cast<long long>(verts.size()) + 1
                                     : static_cast<long long>(verts.size())));
        W(70, QString::number(closed ? 1 : 0));
        for (const cad::Vec2& v : verts) {
            WD(10, v.x);
            WD(20, v.z);
        }
        if (closed) {
            WD(10, verts.front().x);
            WD(20, verts.front().z);
        }
        W(62, QString::number(colorToDxf(e.color)));
    };

    ts << "0\nSECTION\n2\nHEADER\n0\nENDSEC\n";
    ts << "0\nSECTION\n2\nENTITIES\n";

    for (const cad::Entity& e : doc->entities()) {
        switch (e.type) {
        case cad::EntityType::Point:
            if (e.points.empty()) break;
            head(e, QStringLiteral("POINT"));
            WD(10, e.points.front().x); WD(20, e.points.front().z); WD(30, 0.0);
            break;
        case cad::EntityType::Line:
            if (e.points.size() < 2) break;
            head(e, QStringLiteral("LINE"));
            WD(10, e.points[0].x); WD(20, e.points[0].z); WD(30, 0.0);
            WD(11, e.points[1].x); WD(21, e.points[1].z); WD(31, 0.0);
            break;
        case cad::EntityType::Circle:
        case cad::EntityType::Concentric:
            if (e.points.empty()) break;
            head(e, QStringLiteral("CIRCLE"));
            WD(10, e.points.front().x); WD(20, e.points.front().z); WD(40, e.radius);
            break;
        case cad::EntityType::Arc:
            if (e.points.empty()) break;
            head(e, QStringLiteral("ARC"));
            WD(10, e.points.front().x); WD(20, e.points.front().z); WD(40, e.radius);
            WD(50, e.startAngle); WD(51, e.endAngle);
            break;
        case cad::EntityType::Text:
            if (e.points.empty()) break;
            head(e, QStringLiteral("TEXT"));
            WD(10, e.points.front().x); WD(20, e.points.front().z);
            WD(40, e.textHeight);
            W(1, QString::fromStdString(e.text));
            WD(50, 0.0);
            W(7, QStringLiteral("STANDARD"));
            break;
        case cad::EntityType::Rectangle:
            poly(rectCorners(e), true, e);
            break;
        case cad::EntityType::Curve:
            poly(e.points, false, e);
            break;
        case cad::EntityType::Dimension:
            // 标注为本软件自定义图元，DXF 无直接等价，跳过
            break;
        }
    }

    ts << "0\nENDSEC\n0\nEOF\n";
    return file.commit();
}

namespace {

struct Pair {
    int code = 0;
    QString value;
};

QVector<Pair> readPairs(QFile& file)
{
    QTextStream ts(&file);
    QVector<Pair> pairs;
    while (!ts.atEnd()) {
        const QString cstr = ts.readLine().trimmed();
        if (cstr.isEmpty())
            continue;
        bool ok = false;
        const int code = cstr.toInt(&ok);
        if (!ok)
            continue;
        pairs.append({code, ts.readLine().trimmed()});
    }
    return pairs;
}

// 从一段（保留重复组码的）DXF 图元数据中提取多段线顶点
std::vector<cad::Vec2> extractVertices(const QVector<Pair>& run)
{
    std::vector<cad::Vec2> verts;
    for (int k = 0; k < run.size(); ++k) {
        if (run[k].code == 10) {
            double x = run[k].value.toDouble();
            double y = 0.0;
            if (k + 1 < run.size() && run[k + 1].code == 20)
                y = run[k + 1].value.toDouble();
            verts.push_back({x, y});
        }
    }
    return verts;
}

double findCode(const QVector<Pair>& run, int code)
{
    for (const Pair& p : run)
        if (p.code == code)
            return p.value.toDouble();
    return 0.0;
}

void applyCommon(const QVector<Pair>& run, cad::Entity& e)
{
    for (const Pair& p : run) {
        if (p.code == 8)
            e.layer = p.value.toStdString();
        else if (p.code == 62)
            e.color = dxfToColor(p.value.toInt());
    }
}

}  // namespace

bool importDxf(const QString& path, CadDocument* doc)
{
    if (!doc)
        return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QVector<Pair> pairs = readPairs(file);
    doc->clear();

    bool inEntities = false;
    int i = 0;
    const int n = pairs.size();
    while (i < n) {
        if (pairs[i].code != 0) { ++i; continue; }
        const QString tag = pairs[i].value;

        if (tag == QLatin1String("SECTION")) {
            if (i + 1 < n && pairs[i + 1].code == 2)
                inEntities = (pairs[i + 1].value == QLatin1String("ENTITIES"));
            ++i;
            continue;
        }
        if (tag == QLatin1String("ENDSEC")) { inEntities = false; ++i; continue; }
        if (tag == QLatin1String("EOF")) break;
        if (!inEntities) { ++i; continue; }

        // 收集该图元的数据段（直到下一个组码 0）
        QVector<Pair> run;
        int j = i + 1;
        while (j < n && pairs[j].code != 0) {
            run.append(pairs[j]);
            ++j;
        }

        if (tag == QLatin1String("LWPOLYLINE")) {
            std::vector<cad::Vec2> verts = extractVertices(run);
            bool closed = false;
            for (const Pair& p : run)
                if (p.code == 70 && (p.value.toInt() & 1))
                    closed = true;
            if (!verts.empty()) {
                cad::Entity e;
                e.type = cad::EntityType::Curve;
                e.points = verts;
                if (closed)
                    e.points.push_back(verts.front());
                applyCommon(run, e);
                doc->addEntity(e);
            }
            i = j;
            continue;
        }
        if (tag == QLatin1String("POLYLINE")) {
            int flags = 0;
            for (const Pair& p : run)
                if (p.code == 70) flags = p.value.toInt();
            const bool closed = (flags & 1) != 0;
            std::vector<cad::Vec2> verts;
            int k = j;
            while (k < n) {
                if (pairs[k].code != 0) { ++k; continue; }
                const QString t = pairs[k].value;
                if (t == QLatin1String("VERTEX")) {
                    QVector<Pair> vr;
                    int m = k + 1;
                    while (m < n && pairs[m].code != 0) { vr.append(pairs[m]); ++m; }
                    const std::vector<cad::Vec2> vv = extractVertices(vr);
                    verts.insert(verts.end(), vv.begin(), vv.end());
                    k = m;
                    continue;
                } else if (t == QLatin1String("SEQEND")) {
                    ++k;
                    break;
                }
                ++k;
            }
            if (!verts.empty()) {
                cad::Entity e;
                e.type = cad::EntityType::Curve;
                e.points = verts;
                if (closed)
                    e.points.push_back(verts.front());
                doc->addEntity(e);
            }
            i = k;
            continue;
        }

        cad::Entity e;
        if (tag == QLatin1String("LINE")) {
            e.type = cad::EntityType::Line;
            e.points = { {findCode(run, 10), findCode(run, 20)},
                         {findCode(run, 11), findCode(run, 21)} };
        } else if (tag == QLatin1String("POINT")) {
            e.type = cad::EntityType::Point;
            e.points = { {findCode(run, 10), findCode(run, 20)} };
        } else if (tag == QLatin1String("CIRCLE")) {
            e.type = cad::EntityType::Circle;
            e.points = { {findCode(run, 10), findCode(run, 20)} };
            e.radius = findCode(run, 40);
        } else if (tag == QLatin1String("ARC")) {
            e.type = cad::EntityType::Arc;
            e.points = { {findCode(run, 10), findCode(run, 20)} };
            e.radius = findCode(run, 40);
            e.startAngle = findCode(run, 50);
            e.endAngle = findCode(run, 51);
        } else if (tag == QLatin1String("TEXT")) {
            e.type = cad::EntityType::Text;
            e.points = { {findCode(run, 10), findCode(run, 20)} };
            e.textHeight = findCode(run, 40);
            for (const Pair& p : run)
                if (p.code == 1) e.text = p.value.toStdString();
        } else {
            i = j;   // 其它图元（如 INSERT/3DFACE）跳过
            continue;
        }
        applyCommon(run, e);
        doc->addEntity(e);
        i = j;
    }
    return true;
}

}  // namespace cadfile
