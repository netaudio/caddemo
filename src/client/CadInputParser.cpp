#include "CadInputParser.h"

#include "CadGeometry.h"

#include <QMap>
#include <QRegularExpression>
#include <QStringList>

#include <cmath>

namespace cadinput {

// Qt 5 / Qt 6 双版本兼容：Qt 6 把 SkipEmptyParts 移到了 Qt:: 命名空间，
// Qt 5.15 仍保留在 QString:: 下。麒麟 V10 随系统分发的是 Qt 5.15.2，
// Windows 开发环境是 Qt 6.7.3，因此这里统一到一个别名。
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
static constexpr auto kSkipEmptyParts = Qt::SkipEmptyParts;
#else
static constexpr auto kSkipEmptyParts = QString::SkipEmptyParts;
#endif

namespace {

struct Fields {
    QMap<QString, double> numbers;
    QMap<QString, QString> strings;
    bool has(const QString& key) const { return numbers.contains(key); }
    double value(const QString& key, double fallback = 0.0) const
    {
        return numbers.value(key, fallback);
    }
};

bool parseNumber(const QString& token, double& out)
{
    bool ok = false;
    const double value = token.trimmed().toDouble(&ok);
    if (!ok || !std::isfinite(value))
        return false;
    out = value;
    return true;
}

QString axisSecond(const Fields& f)
{
    return f.has(QStringLiteral("z")) ? QStringLiteral("z") : QStringLiteral("y");
}

}  // namespace

Result parse(const QString& input, const cad::Vec2& cursor, const cad::Vec2& base)
{
    Q_UNUSED(cursor)
    Result result;
    const QString text = input.trimmed();
    if (text.isEmpty())
        return result;

    // ---- key=value 形式 ----
    if (text.contains(QLatin1Char('='))) {
        Fields fields;
        const QStringList tokens = text.split(QRegularExpression(QStringLiteral("[,;\\s]+")),
                                              kSkipEmptyParts);
        for (const QString& token : tokens) {
            const int equal = token.indexOf(QLatin1Char('='));
            if (equal <= 0)
                continue;
            const QString key = token.left(equal).toLower();
            const QString raw = token.mid(equal + 1);
            double number = 0.0;
            if (parseNumber(raw, number))
                fields.numbers.insert(key, number);
            else
                fields.strings.insert(key, raw);
        }

        // 绝对坐标
        if (fields.has(QStringLiteral("x")) &&
            (fields.has(QStringLiteral("y")) || fields.has(QStringLiteral("z")))) {
            const QString second = axisSecond(fields);
            result.kind = Kind::Point;
            result.point = {fields.value(QStringLiteral("x")), fields.value(second)};
            result.description = QStringLiteral("绝对坐标 (%1, %2)")
                                     .arg(result.point.x, 0, 'f', 3)
                                     .arg(result.point.z, 0, 'f', 3);
            return result;
        }
        // 相对坐标
        if (fields.has(QStringLiteral("dx")) &&
            (fields.has(QStringLiteral("dy")) || fields.has(QStringLiteral("dz")))) {
            const QString second = fields.has(QStringLiteral("dz"))
                                       ? QStringLiteral("dz") : QStringLiteral("dy");
            result.kind = Kind::Point;
            result.point = {base.x + fields.value(QStringLiteral("dx")),
                            base.z + fields.value(second)};
            result.description = QStringLiteral("相对坐标 (%1, %2)")
                                     .arg(result.point.x, 0, 'f', 3)
                                     .arg(result.point.z, 0, 'f', 3);
            return result;
        }
        // 长度 + 角度
        if (fields.has(QStringLiteral("l"))) {
            const double length = fields.value(QStringLiteral("l"));
            const double angle = fields.value(QStringLiteral("a"), 0.0);
            const double radians = angle * cad::kPi / 180.0;
            result.kind = Kind::Point;
            result.point = {base.x + length * std::cos(radians),
                            base.z + length * std::sin(radians)};
            result.description = QStringLiteral("长度 %1 / 角度 %2° → (%3, %4)")
                                     .arg(length, 0, 'f', 3).arg(angle, 0, 'f', 2)
                                     .arg(result.point.x, 0, 'f', 3)
                                     .arg(result.point.z, 0, 'f', 3);
            return result;
        }
        // 单值数值参数
        for (const QString& key : {QStringLiteral("r"), QStringLiteral("d"),
                                   QStringLiteral("n"), QStringLiteral("a"),
                                   QStringLiteral("h"), QStringLiteral("v")}) {
            if (fields.has(key)) {
                result.kind = Kind::Number;
                result.number = fields.value(key);
                result.description = QStringLiteral("%1 = %2")
                                         .arg(key.toUpper()).arg(result.number, 0, 'f', 4);
                return result;
            }
        }
        // 文本内容
        if (fields.strings.contains(QStringLiteral("t"))) {
            result.kind = Kind::Text;
            result.text = fields.strings.value(QStringLiteral("t"));
            result.description = QStringLiteral("文本：%1").arg(result.text);
            return result;
        }
        return result;
    }

    // ---- @dx,dz 相对坐标简写 ----
    if (text.startsWith(QLatin1Char('@'))) {
        const QStringList parts = text.mid(1).split(
            QRegularExpression(QStringLiteral("[,;\\s]+")), kSkipEmptyParts);
        double first = 0.0, second = 0.0;
        if (parts.size() >= 2 && parseNumber(parts[0], first) && parseNumber(parts[1], second)) {
            result.kind = Kind::Point;
            result.point = {base.x + first, base.z + second};
            result.description = QStringLiteral("相对坐标 (%1, %2)")
                                     .arg(result.point.x, 0, 'f', 3)
                                     .arg(result.point.z, 0, 'f', 3);
            return result;
        }
        return result;
    }

    // ---- x,z 纯数字对 ----
    const QStringList parts = text.split(QRegularExpression(QStringLiteral("[,;\\s]+")),
                                        kSkipEmptyParts);
    double first = 0.0, second = 0.0;
    if (parts.size() >= 2 && parseNumber(parts[0], first) && parseNumber(parts[1], second)) {
        result.kind = Kind::Point;
        result.point = {first, second};
        result.description = QStringLiteral("绝对坐标 (%1, %2)")
                                 .arg(first, 0, 'f', 3).arg(second, 0, 'f', 3);
        return result;
    }

    // ---- 单个数值 ----
    if (parts.size() == 1 && parseNumber(parts[0], first)) {
        result.kind = Kind::Number;
        result.number = first;
        result.description = QStringLiteral("数值 %1").arg(first, 0, 'f', 4);
        return result;
    }

    // ---- 其余一律作为文本 ----
    result.kind = Kind::Text;
    result.text = text;
    if (result.text.startsWith(QLatin1Char('"')) && result.text.endsWith(QLatin1Char('"'))
        && result.text.size() > 1)
        result.text = result.text.mid(1, result.text.size() - 2);
    result.description = QStringLiteral("文本：%1").arg(result.text);
    return result;
}

}  // namespace cadinput
