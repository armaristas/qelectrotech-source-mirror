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
void vertical(QPainter &p, QRectF &extent, qreal x, qreal y, const QString &text, const QFont &f, bool frame = false)
{
    if (text.isEmpty())
        return;
    p.save();
    p.setFont(f);
    p.translate(x, y);
    p.rotate(-90);
    const auto box = QFontMetricsF(f,p.device()).boundingRect(text).adjusted(-2, -2, 2, 2);
    extent=extent.united(p.worldTransform().mapRect(box));
    p.drawText(QPointF(0, 0), text);
    if (frame)
        p.drawRect(box);
    p.restore();
}
void ground(QPainter &p, qreal x, qreal y, int sign)
{
    p.drawLine(QPointF(x, y), QPointF(x, y + sign * 8));
    for (int i = 0; i < 3; ++i)
        p.drawLine(QPointF(x - 6 + i * 2, y + sign * (8 + i * 3)),
                   QPointF(x + 6 - i * 2, y + sign * (8 + i * 3)));
}
QVector<int> shieldRange(const QString &spec, const QVector<Row> &rows, const QString &side, const QString &separator)
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
        qreal maxText = 80;
        p.save();
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(x0-50,baseline-60,44,120),10,10);
        p.drawRect(QRectF(x0-6,baseline-35,6,70));
        p.setPen(QColor("#636363"));
        vertical(p, textExtent,x0-43,baseline+17,strip?strip->name():QString(),QFont("Arial",15));
        p.restore();
        QMap<QUuid, QVector<int>> bridgeGroups;
        QMap<QUuid, QColor> bridgeColors;
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
                    p.fillRect(body, pair.first);
                    if (pair.second.isValid())
                    {
                        p.save();
                        p.setClipRect(body);
                        p.setPen(QPen(pair.second, 4));
                        for (qreal sy = baseline - 110; sy < baseline + 110; sy += 10)
                            p.drawLine(QPointF(x, sy), QPointF(x + step, sy + step));
                        p.restore();
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
                vertical(p, textExtent, x + step - 2, baseline + 105, "R", numberFont);
            if (row.terminal && row.terminal->bridge())
            {
                auto bridge = row.terminal->bridge();
                bridgeGroups[bridge->uuid()] << i;
                bridgeColors[bridge->uuid()] = bridge->color();
            }
            else if (v.value("puente").toBool() && i + 1 < segment.size())
                bridgeGroups[QUuid::createUuid()] = {i, i + 1};
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
                p.drawLine(QPointF(cx, origin), QPointF(cx, end));
                const qreal cableLen = qMax(0.0, cfg.value("cable_line_len_" + side).toDouble(100));
                const qreal hoseLen = qMax(0.0, cfg.value("hose_line_len_" + side).toDouble(50));
                p.drawLine(QPointF(cx - 2, origin + sign * cableLen),
                           QPointF(cx + 2, origin + sign * cableLen));
                p.drawLine(QPointF(cx - 2, end - sign * hoseLen), QPointF(cx + 2, end - sign * hoseLen));
                vertical(p, textExtent, cx - 2, origin + sign * near + cfg.value("cable_text_offset_" + side).toDouble(),
                         wire, wireFont);
                const qreal shift = cfg.value("offset_" + side).toDouble();
                const auto f = font(cfg, side);
                const qreal measured = QFontMetricsF(f).horizontalAdvance(device);
                const qreal advance =
                    qMax(measured, device.size() * qMax(0.0, cfg.value("char_adv_ext").toDouble(5.2)));
                maxText = qMax(maxText, advance + qAbs(shift));
                vertical(p, textExtent, x + 2, end + sign * 10 + shift + (sign < 0 ? 0 : advance), device, f,
                         cfg.value("draw_frame").toBool());
                if (!hose.isEmpty())
                    vertical(p, textExtent, cx + 3,
                             origin + sign * (near + hoseLength / 2) +
                                 cfg.value("hose_text_offset_" + side).toDouble(),
                             hose, hoseFont);
                if (device.split(" & ").contains("PE"))
                    ground(p, cx, end, sign);
            }
        }
        for (auto bridge = bridgeGroups.begin(); bridge != bridgeGroups.end(); ++bridge)
        {
            const auto indices = bridge.value();
            if (!indices.isEmpty())
            {
                p.save();
                p.setPen(QPen(bridgeColors.value(bridge.key(), Qt::red), 1.6));
                for (int i : indices)
                    p.drawEllipse(QPointF(x0 + i * step + step / 2, baseline - 20), 1.5, 1.5);
                p.drawLine(QPointF(x0 + indices.first() * step + step / 2, baseline - 20),
                           QPointF(x0 + indices.last() * step + step / 2, baseline - 20));
                // The other end may be in another cut of the same strip.
                // Keep its bridge contact visible at the cut boundary.
                if(indices.size()==1)p.drawLine(QPointF(x0+indices.first()*step+step/2,baseline-20),QPointF(x0+(indices.first()+1)*step,baseline-20));
                p.restore();
            }
        }
        // Group hoses on each side independently, contiguous or compact.
        for (const auto &side : {QString("int"), QString("ext")})
        {
            QMap<QString, QVector<int>> groups;
            QString previous;
            int run = 0;
            for (int i = 0; i < segment.size(); ++i)
            {
                const auto hose = segment[i].values.value("mang_" + side).toString();
                if (hose != previous)
                    ++run;
                previous = hose;
                if (hose.isEmpty())
                    continue;
                const auto group =
                    cfg.value("hose_group_mode").toString().compare("Compacta", Qt::CaseInsensitive) == 0
                        ? hose
                        : QString::number(run) + ":" + hose;
                groups[group] << i;
            }
            const int sign = side == "int" ? -1 : 1;
            for (const auto &indices : groups)
                if (indices.size() > 1)
                {
                    const qreal a = x0 + indices.first() * step + step / 2,
                                b = x0 + indices.last() * step + step / 2,
                                y = baseline + sign * (90 + near + hoseLength / 2);
                    p.save();
                    p.setBrush(Qt::NoBrush);
                    p.drawRoundedRect(QRectF(a - 5, y - 7, b - a + 10, 14), 4, 4);
                    p.restore();
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
                        auto indices = shieldRange(spec.trimmed(), segment, side, cfg.value("multilevel_sep").toString(QString::fromUtf8("·")));
                        if (indices.isEmpty())
                            indices << i;
                        const qreal a = x0 + indices.first() * step, b = x0 + (indices.last() + 1) * step,
                                    y = baseline + sign * 130;
                        p.drawEllipse(QRectF(a - 2, y - 7, b - a + 4, 14));
                        p.drawLine(QPointF(x0 + i * step + step / 2, baseline + sign * 90),
                                   QPointF(x0 + i * step + step / 2, y));
                        ground(p, b + 8, y, sign);
                    }
                }
        }
        if (cfg.value("draw_ground_line").toBool())
        {
            QVector<int> pe;
            for (int i = 0; i < segment.size(); ++i)
                if (segment[i].values.value("tipo_borna").toString() == "ground")
                    pe << i;
            if (!pe.isEmpty())
            {
                p.save();
                p.setPen(QPen(QColor("#23852a"), 1.5));
                p.drawLine(QPointF(x0 + pe.first() * step, baseline + 75),
                           QPointF(x0 + (pe.last() + 1) * step, baseline + 75));
                p.restore();
            }
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
QPair<QColor,QColor> terminalColors(const Row &row,const QJsonObject &config){return colors(row,config);}
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
