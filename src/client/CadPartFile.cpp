#include "CadPartFile.h"

#include "CadDocument.h"
#include "CadProtocol.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QVariant>

namespace cadfile {

QString typeName(cad::EntityType t)
{
    switch (t) {
    case cad::EntityType::Point:       return QStringLiteral("点");
    case cad::EntityType::Line:        return QStringLiteral("直线");
    case cad::EntityType::Circle:      return QStringLiteral("圆");
    case cad::EntityType::Arc:         return QStringLiteral("圆弧");
    case cad::EntityType::Curve:       return QStringLiteral("曲线");
    case cad::EntityType::Rectangle:   return QStringLiteral("矩形");
    case cad::EntityType::Concentric:  return QStringLiteral("同心圆");
    case cad::EntityType::Text:        return QStringLiteral("文本");
    case cad::EntityType::Dimension:   return QStringLiteral("标注");
    }
    return QStringLiteral("图元");
}

QString typeKey(cad::EntityType t)
{
    switch (t) {
    case cad::EntityType::Point:       return QStringLiteral("point");
    case cad::EntityType::Line:        return QStringLiteral("line");
    case cad::EntityType::Circle:      return QStringLiteral("circle");
    case cad::EntityType::Arc:         return QStringLiteral("arc");
    case cad::EntityType::Curve:       return QStringLiteral("curve");
    case cad::EntityType::Rectangle:   return QStringLiteral("rect");
    case cad::EntityType::Concentric:  return QStringLiteral("concentric");
    case cad::EntityType::Text:        return QStringLiteral("text");
    case cad::EntityType::Dimension:   return QStringLiteral("dimension");
    }
    return QStringLiteral("entity");
}

cad::EntityType typeFromKey(const QString& k)
{
    if (k == QLatin1String("point"))       return cad::EntityType::Point;
    if (k == QLatin1String("line"))        return cad::EntityType::Line;
    if (k == QLatin1String("circle"))      return cad::EntityType::Circle;
    if (k == QLatin1String("arc"))         return cad::EntityType::Arc;
    if (k == QLatin1String("curve"))       return cad::EntityType::Curve;
    if (k == QLatin1String("rect"))        return cad::EntityType::Rectangle;
    if (k == QLatin1String("concentric"))  return cad::EntityType::Concentric;
    if (k == QLatin1String("text"))        return cad::EntityType::Text;
    if (k == QLatin1String("dimension"))   return cad::EntityType::Dimension;
    return cad::EntityType::Line;
}

QJsonObject entityToJson(const cad::Entity& e)
{
    QJsonObject o;
    o[QStringLiteral("id")]    = static_cast<qint64>(e.id);
    o[QStringLiteral("type")]  = typeKey(e.type);

    QJsonArray pts;
    for (const cad::Vec2& p : e.points) {
        QJsonObject po;
        po[QStringLiteral("x")] = p.x;
        po[QStringLiteral("z")] = p.z;
        pts.append(po);
    }
    o[QStringLiteral("points")]     = pts;
    o[QStringLiteral("radius")]     = e.radius;
    o[QStringLiteral("innerRadius")] = e.innerRadius;
    o[QStringLiteral("startAngle")] = e.startAngle;
    o[QStringLiteral("endAngle")]   = e.endAngle;
    o[QStringLiteral("textHeight")] = e.textHeight;
    o[QStringLiteral("text")]       = QString::fromStdString(e.text);
    o[QStringLiteral("layer")]      = QString::fromStdString(e.layer);
    o[QStringLiteral("color")]      = static_cast<int>(e.color);
    o[QStringLiteral("lineType")]   = static_cast<int>(e.lineType);
    o[QStringLiteral("lineWidth")]  = e.lineWidth;
    o[QStringLiteral("visible")]    = e.visible;
    return o;
}

cad::Entity entityFromJson(const QJsonObject& o)
{
    cad::Entity e;
    e.id          = o[QStringLiteral("id")].toVariant().toLongLong();
    e.type        = typeFromKey(o[QStringLiteral("type")].toString());

    const QJsonArray pts = o[QStringLiteral("points")].toArray();
    for (const QJsonValue& v : pts) {
        const QJsonObject po = v.toObject();
        e.points.push_back({ po[QStringLiteral("x")].toDouble(),
                             po[QStringLiteral("z")].toDouble() });
    }
    e.radius      = o[QStringLiteral("radius")].toDouble();
    e.innerRadius = o[QStringLiteral("innerRadius")].toDouble();
    e.startAngle  = o[QStringLiteral("startAngle")].toDouble();
    e.endAngle    = o[QStringLiteral("endAngle")].toDouble();
    e.textHeight  = o[QStringLiteral("textHeight")].toDouble(5.0);
    e.text        = o[QStringLiteral("text")].toString().toStdString();
    e.layer       = o[QStringLiteral("layer")].toString().toStdString();
    e.color       = static_cast<cad::ColorIndex>(o[QStringLiteral("color")].toInt(0));
    e.lineType    = static_cast<cad::LineType>(o[QStringLiteral("lineType")].toInt(0));
    e.lineWidth   = o[QStringLiteral("lineWidth")].toDouble();
    e.visible     = o[QStringLiteral("visible")].toBool(true);
    return e;
}

QString entityTypeName(cad::EntityType type)
{
    return typeName(type);
}

bool savePart(const QString& path, const CadDocument* doc)
{
    if (!doc)
        return false;

    QJsonObject root;
    root[QStringLiteral("format")]  = QStringLiteral("Part2D");
    root[QStringLiteral("version")] = 2;
    root[QStringLiteral("nextId")]  = static_cast<qint64>(doc->nextId());

    QJsonObject cur;
    cur[QStringLiteral("color")]     = static_cast<int>(doc->currentColor());
    cur[QStringLiteral("lineType")]  = static_cast<int>(doc->currentLineType());
    cur[QStringLiteral("lineWidth")] = doc->currentLineWidth();
    root[QStringLiteral("current")]  = cur;

    QJsonArray ents;
    for (const cad::Entity& e : doc->entities())
        ents.append(entityToJson(e));
    root[QStringLiteral("entities")] = ents;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QJsonDocument docJson(root);
    file.write(docJson.toJson(QJsonDocument::Indented));
    return file.commit();
}

bool loadPart(const QString& path, CadDocument* doc)
{
    if (!doc)
        return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QJsonParseError err;
    const QJsonDocument docJson = QJsonDocument::fromJson(file.readAll(), &err);
    if (docJson.isNull() || !docJson.isObject())
        return false;

    const QJsonObject root = docJson.object();
    if (root[QStringLiteral("format")].toString() != QLatin1String("Part2D"))
        return false;

    doc->clear();
    const QJsonObject cur = root[QStringLiteral("current")].toObject();
    doc->setCurrentColor(static_cast<cad::ColorIndex>(cur[QStringLiteral("color")].toInt(0)));
    doc->setCurrentLineType(static_cast<cad::LineType>(cur[QStringLiteral("lineType")].toInt(0)));
    doc->setCurrentLineWidth(cur[QStringLiteral("lineWidth")].toDouble());

    const QJsonArray ents = root[QStringLiteral("entities")].toArray();
    for (const QJsonValue& v : ents)
        doc->insertEntity(entityFromJson(v.toObject()));
    doc->setNextId(root[QStringLiteral("nextId")].toVariant().toLongLong());
    return true;
}

}  // namespace cadfile
