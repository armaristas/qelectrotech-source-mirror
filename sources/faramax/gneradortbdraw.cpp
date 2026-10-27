// SPDX-License-Identifier: GPL-2.0-or-later
#include "../TerminalStrip/GraphicsItem/terminalstripdrawerhelpers.h"
#include "../TerminalStrip/physicalterminal.h"
#include "../TerminalStrip/realterminal.h"
#include "../TerminalStrip/terminalcolor.h"
#include "../TerminalStrip/terminalstrip.h"
#include "../TerminalStrip/terminalstripbridge.h"
#include "gneradortb.h"
#include <QFontMetricsF>
#include <QJsonDocument>
#include <QPainter>
#include <QPicture>
#include <QRegularExpression>
#include <algorithm>

namespace Faramax::TB
{
namespace
{
QString normalizeColor(QString color)
{
    const QMap<QString, QString> legacy{{"HTMLGrayLightGray", "lightgray"},
                                        {"HTMLBlueDodgerBlue", "dodgerblue"},
                                        {"HTMLGrayDarkSlateGray", "darkslategray"}};
    return legacy.value(color, color);
}
QPair<QColor, QColor> colors(const Row &row, const QJsonObject &cfg)
{
    QString spec = row.values.value("color").toString();
    if (spec.isEmpty() || spec.compare("Auto", Qt::CaseInsensitive) == 0)
    {
        spec.clear();
        auto rules =
            QJsonDocument::fromJson(cfg.value("terminal_color_rules_json").toString().toUtf8()).array();
        if (rules.isEmpty())
            for (const auto &entry :
                 QVector<QPair<QString, QString>>{{"terminal_color_pe_codes", "green|yellow"},
                                                  {"terminal_color_blue_codes", "dodgerblue"},
                                                  {"terminal_color_red_codes", "red"},
                                                  {"terminal_color_black_codes", "darkslategray"},
                                                  {"terminal_color_gray_codes", "lightgray"}})
                for (const auto &token : cfg.value(entry.first)
                                             .toString()
                                             .split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts))
                    rules.append(QJsonObject{{"text", token}, {"color", entry.second}});
        const auto model = row.values.value("modelo").toString();
        for (const auto &rule : rules)
        {
            const auto r = rule.toObject();
            if (!r.value("text").toString().isEmpty() &&
                model.contains(r.value("text").toString(), Qt::CaseInsensitive))
            {
                spec = r.value("color").toString();
                break;
            }
        }
        if (spec.isEmpty() && row.values.value("tipo_borna").toString() == "ground")
            spec = "green|yellow";
        if (spec.isEmpty())
            spec = cfg.value("terminal_default_color").toString("lightgray");
    }
    const auto parts = spec.split(QRegularExpression("[|/]"));
    return {QColor(normalizeColor(parts.value(0))),
            parts.size() > 1 ? QColor(normalizeColor(parts.value(1))) : QColor()};
}
QFont font(const QJsonObject &cfg, const QString &side)
{
    QFont result(cfg.value("font_family_" + side).toString(cfg.value("font_family").toString("Courier New")));
    result.setPointSize(qBound(3, cfg.value("font_size_" + side).toInt(7), 72));
    result.setBold(cfg.value("font_bold_" + side).toBool());
    return result;
}
void vertical(QPainter &p, QRectF &extent, qreal x, qreal y, const QString &text, const QFont &f,
              bool frame = false)
{
    if (text.isEmpty())
        return;
    p.save();
    p.setFont(f);
    p.translate(x, y);
    p.rotate(-90);
    const auto box = QFontMetricsF(f, p.device()).boundingRect(text).adjusted(-2, -2, 2, 2);
    extent = extent.united(p.worldTransform().mapRect(box));
    p.drawText(QPointF(0, 0), text);
    if (frame)
        p.drawRoundedRect(box, 3, 3);
    p.restore();
}
void ground(QPainter &p, qreal x, qreal y, int sign)
{
    p.drawLine(QPointF(x, y), QPointF(x, y + sign * 8));
    for (int i = 0; i < 3; ++i)
        p.drawLine(QPointF(x - 6 + i * 2, y + sign * (8 + i * 3)),
                   QPointF(x + 6 - i * 2, y + sign * (8 + i * 3)));
}
QVector<int> shieldRange(const QString &spec, const QVector<Row> &rows, const QString &side,
                         const QString &separator)
{
    QVector<int> result;
    const auto ends = spec.split('/');
    if (ends.size() == 2)
    {
        int a = -1, b = -1;
        for (int i = 0; i < rows.size(); ++i)
        {
            const auto n = rows[i].values.value("num_borna").toString();
            if (n == ends[0].trimmed() || n.section(separator, 0, 0) == ends[0].trimmed())
                if (a < 0)
                    a = i;
            if (n == ends[1].trimmed() || n.section(separator, 0, 0) == ends[1].trimmed())
                b = i;
        }
        if (a >= 0 && b >= 0)
        {
            if (a > b)
                std::swap(a, b);
            for (int i = a; i <= b; ++i)
                result << i;
        }
    }
    else
        for (int i = 0; i < rows.size(); ++i)
            if (rows[i].values.value("mang_" + side).toString().split(" & ").contains(spec.trimmed()))
                result << i;
    return result;
}
QPicture picture(TerminalStrip *strip, const QVector<Row> &all, const QJsonObject &cfg,
                 QVector<Reference> *refs = nullptr)
{
    QPicture output;
    QRectF textExtent;
    QPainter p(&output);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Qt::black, 0.8));
    const qreal step = qMax(10.0, cfg.value("paso_x").toDouble(20));
    const qreal x0 = cfg.value("x_start").toDouble(50);
    qreal baseline = cfg.value("y_base").toDouble();
    const qreal near = qMax(0.0, cfg.value("line_borna_len").toDouble(20)),
                hoseLength = qMax(0.0, cfg.value("line_manguera_len").toDouble(120));
    const qreal total = near + hoseLength + qMax(0.0, cfg.value("line_dispositivo_len").toDouble(20));
    QFont numberFont("Arial", 9), positionFont("Arial", 6), modelFont("Arial", 5),
        wireFont(cfg.value("font_family").toString("Courier New"), 7);
    QFont hoseFont(cfg.value("hose_font_family").toString("Courier New"),
                   qBound(3, cfg.value("hose_font_size").toInt(7), 72));
    hoseFont.setBold(cfg.value("hose_font_bold").toBool());
    int offset = 0;
    while (offset < all.size())
    {
        int last = offset;
        while (last + 1 < all.size() && !all[last].values.value("corte").toBool())
            ++last;
        const auto segment = all.mid(offset, last - offset + 1);
        struct HoseGroup
        {
            QString name;
            QVector<int> members;
        };
        QMap<QString, QVector<HoseGroup>> hoses;
        QMap<QString, QMap<int, int>> hoseIndex;
        for (const auto &side : {QString("int"), QString("ext")})
        {
            const bool compact =
                cfg.value("hose_group_mode").toString().compare("Compacta", Qt::CaseInsensitive) == 0;
            QVector<HoseGroup> groups;
            for (int i = 0; i < segment.size(); ++i)
            {
                const auto name = segment[i].values.value("mang_" + side).toString().trimmed();
                if (name.isEmpty())
                    continue;
                int found = -1;
                for (int g = 0; g < groups.size(); ++g)
                    if (groups[g].name == name && (compact || groups[g].members.last() == i - 1))
                    {
                        found = g;
                        break;
                    }
                if (found < 0)
                {
                    groups.append({name, {}});
                    found = groups.size() - 1;
                }
                groups[found].members << i;
            }
            for (const auto &group : groups)
                if (group.members.size() > 1)
                {
                    int g = hoses[side].size();
                    hoses[side] << group;
                    for (int i : group.members)
                        hoseIndex[side][i] = g;
                }
        }
        qreal maxText = 80;
        p.save();
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(x0 - 50, baseline - 60, 44, 120), 10, 10);
        p.drawRect(QRectF(x0 - 6, baseline - 35, 6, 70));
        p.setPen(QColor("#636363"));
        vertical(p, textExtent, x0 - 43, baseline + 17, strip ? strip->name() : QString(),
                 QFont("Arial", 15));
        p.restore();
        for (int i = 0; i < segment.size(); ++i)
        {
            const auto &row = segment[i];
            const auto &v = row.values;
            const qreal x = x0 + i * step, cx = x + step / 2;
            const QRectF body(x, baseline - 90, step, 180);
            const bool multi = cfg.value("enable_multilevel").toBool() && row.physical &&
                               row.physical->realTerminalCount() > 1;
            const bool previous = multi && i > 0 && segment[i - 1].physical == row.physical;
            const bool next = multi && i + 1 < segment.size() && segment[i + 1].physical == row.physical;
            const bool doubled = multi && v.value("double").toBool();
            if (cfg.value("enable_terminal_colors").toBool())
            {
                const auto pair = colors(row, cfg);
                if (pair.first.isValid())
                {
                    const QRectF band(x, baseline - 90, step, 45);
                    p.fillRect(band, pair.first);
                    if (pair.second.isValid())
                    {
                        p.fillRect(QRectF(x + step / 2, baseline - 90, step / 2, 45), pair.second);
                    }
                }
            }
            p.setPen(QPen(Qt::black, 0.8));
            p.setBrush(Qt::NoBrush);
            if (!multi)
                p.drawRect(body);
            else
            {
                p.drawLine(body.topLeft(), body.topRight());
                p.drawLine(body.bottomLeft(), body.bottomRight());
                if (!previous)
                    p.drawLine(body.topLeft(), body.bottomLeft());
                else if (!doubled)
                {
                    p.save();
                    p.setPen(QPen(QColor("#666666"), 0.8, Qt::DashLine));
                    p.drawLine(body.topLeft(), body.bottomLeft());
                    p.restore();
                }
                if (!next)
                    p.drawLine(body.topRight(), body.bottomRight());
            }
            if (!doubled || !previous)
            {
                const qreal center = doubled ? x + step : cx;
                p.drawEllipse(QPointF(center, baseline - 30), 2, 2);
                p.drawEllipse(QPointF(center, baseline - 10), 2, 2);
                if (ElementData::terminalTypeFromString(v.value("tipo_borna").toString()) == ElementData::TTGeneric)
                    p.drawLine(QPointF(center, baseline - 28), QPointF(center, baseline - 12));
                const qreal levelOffset = multi && !doubled ? row.terminal->level() * 10 : 0;
                TerminalStripDrawer::Helpers::drawTypeSymbol(
                    &p, QRectF(center - 5, baseline - 27 - levelOffset, 10, 14),
                    ElementData::terminalTypeFromString(v.value("tipo_borna").toString()),
                    row.terminal && row.terminal->isLed());
                vertical(p, textExtent, x + 4, baseline + 85, v.value("num_borna").toString(), numberFont);
            }
            if (!previous)
                vertical(p, textExtent, x + 6, baseline - 48, v.value("posicion").toString(), positionFont);
            if (refs && row.terminal && !previous && !v.value("posicion").toString().isEmpty())
            {
                QTransform transform;
                transform.translate(x + 6, baseline - 48);
                transform.rotate(-90);
                refs->append({transform.mapRect(QFontMetricsF(positionFont)
                                                    .boundingRect(v.value("posicion").toString())
                                                    .adjusted(-2, -2, 2, 2)),
                              row.terminal});
            }
            if (!previous)
                vertical(p, textExtent, x + 6, baseline + 40, v.value("modelo").toString(), modelFont);
            if (v.value("reserva").toBool())
            {
                const bool external = !v.value("disp_ext").toString().isEmpty(),
                           internal = !v.value("disp_int").toString().isEmpty();
                const int sign = external && !internal ? -1 : 1;
                if (!external || !internal)
                {
                    p.drawLine(QPointF(cx, baseline + sign * 90), QPointF(cx, baseline + sign * 110));
                    vertical(p, textExtent, x, baseline + sign * 132, "R", numberFont);
                }
            }
            int jumperEnd = i + 1;
            while (jumperEnd < segment.size() && row.terminal && segment[jumperEnd].terminal &&
                   row.terminal->level() != segment[jumperEnd].terminal->level())
                ++jumperEnd;
            if (v.value("puente").toBool() && jumperEnd < segment.size())
            {
                const qreal endX = x0 + jumperEnd * step + step / 2;
                p.save();
                p.setPen(QPen(Qt::red, 1.6));
                p.drawRect(QRectF(cx - 1, baseline - 16, 2, 2));
                p.drawLine(QPointF(cx, baseline - 15), QPointF(endX, baseline - 15));
                p.drawRect(QRectF(endX - 1, baseline - 16, 2, 2));
                p.restore();
            }
            for (const auto &side : {QString("int"), QString("ext")})
            {
                const int sign = side == "int" ? -1 : 1;
                const qreal origin = baseline + sign * 90;
                const auto device = v.value("disp_" + side).toString(),
                           wire = v.value("cable_" + side).toString(),
                           hose = v.value("mang_" + side).toString();
                if (device.isEmpty() && wire.isEmpty() && hose.isEmpty())
                    continue;
                const qreal end = origin + sign * total;
                const bool grouped = hoseIndex[side].contains(i);
                const bool hasHose = !hose.isEmpty() && !device.isEmpty();
                const bool compact =
                    cfg.value("hose_group_mode").toString().compare("Compacta", Qt::CaseInsensitive) == 0;
                const qreal shift = grouped && compact ? hoseIndex[side].value(i) * 10 : 0;
                const qreal barA = origin + sign * (near + shift), barB = barA + sign * hoseLength;
                const bool pe = device.trimmed() == "PE";
                if (pe)
                {
                    if (cfg.value("draw_ground_line").toBool())
                        ground(p, cx, end, sign);
                    p.drawLine(QPointF(cx, origin), QPointF(cx, end));
                }
                else if (grouped)
                {
                    p.drawLine(QPointF(cx, origin), QPointF(cx, barA));
                    p.drawLine(QPointF(cx, barB), QPointF(cx, end));
                }
                else
                    p.drawLine(QPointF(cx, origin), QPointF(cx, end));
                const qreal wireY = ((hasHose || grouped ? barB : origin) + end) / 2 +
                                    cfg.value("cable_text_offset_" + side).toDouble() * (sign < 0 ? 1 : -1);
                if (!wire.isEmpty())
                    vertical(p, textExtent, x - 4, wireY, wire, QFont("Arial", 6));
                if (hasHose && !grouped)
                    vertical(p, textExtent, x - 5,
                             (barA + barB) / 2 +
                                 cfg.value("hose_text_offset_" + side).toDouble() * (sign < 0 ? 1 : -1),
                             hose, hoseFont);
                const auto f = font(cfg, side);
                const qreal advance = QFontMetricsF(f, p.device()).horizontalAdvance(" " + device + " ");
                maxText = qMax(maxText, advance + qAbs(cfg.value("offset_" + side).toDouble()));
                if (!device.isEmpty())
                    vertical(p, textExtent, x + 2,
                             end + (sign < 0 ? 0 : advance + 6) + cfg.value("offset_" + side).toDouble(),
                             pe ? device : " " + device + " ", f, !pe);
            }
        }
        for (int anchor = 0; anchor < segment.size(); ++anchor)
        {
            const int count = segment[anchor].values.value("peine_grupo").toInt();
            if (count <= 0)
                continue;
            QVector<int> active;
            for (int i = anchor; i < qMin(int(segment.size()), anchor + count); ++i)
                if (segment[i].values.value("peine").toBool() ||
                    (i > anchor && segment[i].values.value("peine_grupo").toInt() == 0))
                    active << i;
            p.save();
            p.setPen(QPen(QColor("orange"), 1.5));
            for (int i : active)
                p.drawLine(QPointF(x0 + i * step + step / 2, baseline - 90),
                           QPointF(x0 + (i + 1) * step, baseline - 100));
            if (active.size() > 1)
                p.drawLine(QPointF(x0 + (active.first() + 1) * step, baseline - 100),
                           QPointF(x0 + (active.last() + 1) * step, baseline - 100));
            p.restore();
        }
        // Hoses use the two horizontal buses and central stem of v5.6.
        for (const auto &side : {QString("int"), QString("ext")})
        {
            const int sign = side == "int" ? -1 : 1;
            const bool compact =
                cfg.value("hose_group_mode").toString().compare("Compacta", Qt::CaseInsensitive) == 0;
            for (int g = 0; g < hoses[side].size(); ++g)
            {
                const auto &group = hoses[side][g];
                const qreal a = x0 + group.members.first() * step + step / 2,
                            b = x0 + group.members.last() * step + step / 2, m = (a + b) / 2;
                const qreal yA = baseline + sign * (90 + near + (compact ? g * 10 : 0)),
                            yB = yA + sign * hoseLength;
                p.drawLine(QPointF(a, yA), QPointF(b, yA));
                p.drawLine(QPointF(a, yB), QPointF(b, yB));
                p.drawLine(QPointF(m, yA), QPointF(m, yB));
                vertical(p, textExtent, m - 5,
                         (compact ? yB : (yA + yB) / 2) +
                             cfg.value("hose_text_offset_" + side).toDouble() * (sign < 0 ? 1 : -1),
                         group.name, hoseFont);
            }
            if (cfg.value("draw_shield_symbol").toBool())
                for (int i = 0; i < segment.size(); ++i)
                {
                    const auto &v = segment[i].values;
                    if (!v.value("shl_" + side).toBool())
                        continue;
                    auto specs = v.value("mang_shl_" + side).toString().split('&', Qt::SkipEmptyParts);
                    if (specs.isEmpty())
                        specs << v.value("mang_" + side).toString();
                    for (const auto &spec : specs)
                    {
                        auto indices =
                            shieldRange(spec.trimmed(), segment, side,
                                        cfg.value("multilevel_sep").toString(QString::fromUtf8("·")));
                        if (indices.isEmpty())
                            indices << i;
                        const qreal a = x0 + indices.first() * step, b = x0 + (indices.last() + 1) * step,
                                    y = baseline + sign * 130;
                        p.save();
                        p.setPen(QPen(Qt::black, 0.8, Qt::DashLine));
                        p.drawEllipse(QRectF(a + 1, y - 7, qMax(18.0, b - a - 2), 14));
                        const qreal owner = x0 + i * step + step / 2,
                                    edge = owner >= (a + b) / 2 ? b - 1 : a + 1;
                        p.drawLine(QPointF(edge, y), QPointF(owner, y));
                        p.drawLine(QPointF(owner, y), QPointF(owner, baseline + sign * 90));
                        p.restore();
                    }
                }
        }
        if (cfg.value("draw_frame").toBool())
        {
            p.save();
            p.setPen(QPen(QColor("#777777"), 0.6, Qt::DashLine));
            p.drawRoundedRect(QRectF(x0 - 55, baseline - 95, 55 + segment.size() * step, 210), 6, 6);
            p.restore();
        }
        baseline += 180 + 2 * total + 2 * maxText + 100;
        offset = last + 1;
    }
    if (all.isEmpty())
    {
        p.setFont(QFont("Arial", 9));
        p.drawText(QPointF(x0, baseline), strip ? strip->name() : QStringLiteral("Bornero"));
    }
    p.end();
    // QPicture's automatic rectangle omits text extents on some Qt/Windows
    // paint engines. Keep the same measured glyphs used by the renderer, so
    // longer destinations cannot be clipped by a scene, preview or PDF.
    output.setBoundingRect(QRectF(output.boundingRect()).united(textExtent).toAlignedRect());
    return output;
}
class Cache final : public QObject
{
  public:
    QPicture image;
    QVector<Reference> refs;
    bool dirty = true;
    QPointer<TerminalStrip> strip;
    explicit Cache(TerminalStrip *s) : QObject(s), strip(s)
    {
        setObjectName("faramaxTBDrawing");
        auto invalidate = [this]()
        {
            dirty = true;
        };
        connect(s, &TerminalStrip::dataChanged, this, invalidate);
        connect(s, &TerminalStrip::orderChanged, this, invalidate);
        connect(s, &TerminalStrip::bridgeChanged, this, invalidate);
        connect(s, &TerminalStrip::bridgeColorChanged, this, invalidate);
    }
    void update()
    {
        if (dirty && strip)
        {
            refs.clear();
            image = picture(strip, rows(strip), settings(strip), &refs);
            dirty = false;
        }
    }
};
Cache *cache(TerminalStrip *strip)
{
    auto *found = strip->findChild<QObject *>("faramaxTBDrawing", Qt::FindDirectChildrenOnly);
    auto *result = found ? static_cast<Cache *>(found) : new Cache(strip);
    result->update();
    return result;
}
} // namespace
QPair<QColor, QColor> terminalColors(const Row &row, const QJsonObject &config)
{
    return colors(row, config);
}
QRectF render(QPainter *painter, TerminalStrip *strip, const QVector<Row> &data, const QJsonObject &config)
{
    const auto drawing = picture(strip, data, config);
    if (painter)
        painter->drawPicture(0, 0, drawing);
    return QRectF(drawing.boundingRect()).adjusted(-10, -10, 10, 10);
}
QVector<Reference> references(TerminalStrip *strip)
{
    return strip ? cache(strip)->refs : QVector<Reference>();
}
QRectF bounds(TerminalStrip *strip)
{
    return strip ? QRectF(cache(strip)->image.boundingRect()).adjusted(-10, -10, 10, 10) : QRectF();
}
void invalidateDrawing(TerminalStrip *strip)
{
    if (!strip)
        return;
    if (auto *existing = strip->findChild<QObject *>("faramaxTBDrawing", Qt::FindDirectChildrenOnly))
        static_cast<Cache *>(existing)->dirty = true;
}
void paint(QPainter *painter, TerminalStrip *strip)
{
    if (strip)
        painter->drawPicture(0, 0, cache(strip)->image);
}
} // namespace Faramax::TB
