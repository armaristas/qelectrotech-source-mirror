// SPDX-License-Identifier: GPL-2.0-or-later
// Port of GNerador_TB v5.6 onto QET's RealTerminal / PhysicalTerminal model.
#include "gneradortb.h"
#include "../QPropertyUndoCommand/qpropertyundocommand.h"
#include "../TerminalStrip/UndoCommand/addterminaltostripcommand.h"
#include "../TerminalStrip/UndoCommand/bridgeterminalscommand.h"
#include "../TerminalStrip/UndoCommand/changeterminallevel.h"
#include "../TerminalStrip/physicalterminal.h"
#include "../TerminalStrip/realterminal.h"
#include "../TerminalStrip/terminalcolor.h"
#include "../TerminalStrip/terminalstrip.h"
#include "../TerminalStrip/terminalstripbridge.h"
#include "../cable/cable.h"
#include "../diagram.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/terminal.h"
#include "../qetproject.h"
#include "../undocommand/changeelementdatacommand.h"
#include "arrowtexts.h"
#include <QCoreApplication>
#include <QDynamicPropertyChangeEvent>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTimer>
#include <QUndoStack>
#include <algorithm>
#include <memory>

namespace Faramax::TB
{
namespace
{
QString tr(const char *s)
{
    return QCoreApplication::translate("GNeradorTB", s);
}
QString join(const QStringList &values)
{
    return values.join(QStringLiteral(" & "));
}
void unique(QStringList &values, const QString &value)
{
    if (!value.isEmpty() && !values.contains(value))
        values << value;
}
QString number(const QString &label, const QJsonObject &cfg)
{
    const auto delim = cfg.value("borna_delim").toString(":");
    const int pos = label.indexOf(delim);
    return pos < 0 ? label : label.mid(pos + delim.size());
}
QString hose(const QString &text, const QJsonObject &cfg)
{
    const auto delimiter = cfg.value("hose_delim").toString("-");
    const int pos = text.indexOf(delimiter);
    return pos > 0 ? text.left(pos) : QString();
}
QString model(const ElementData &data)
{
    for (const auto &name :
         {"manufacturer_reference", "machine_manufacturer_reference", "terminal_catalog_entry"})
    {
        auto value = data.m_informations.value(QLatin1String(name)).toString();
        if (!value.isEmpty())
            return value;
    }
    return {};
}
QString endpoint(Terminal *terminal)
{
    auto *element = terminal ? terminal->parentElement() : nullptr;
    if (!element)
        return {};
    auto label = element->actualLabel().trimmed();
    if (label == "PE" || label == "P.E." || element->elementData().terminalType() == ElementData::TTGround)
        return "PE";
    if (label.isEmpty())
        return terminal->name().isEmpty() ? QString() : ":" + terminal->name();
    return label.contains(':') || terminal->name().isEmpty() ? label : label + ':' + terminal->name();
}
QVector<Terminal *> endpoints(Terminal *peer)
{
    if (!peer || !peer->parentElement())
        return {};
    auto *source = peer->parentElement();
    if (!(source->linkType() & Element::AllReport))
        return {peer};
    QVector<Terminal *> result;
    QList<Element *> pending = source->linkedElements();
    QSet<Element *> visited{source};
    while (!pending.isEmpty())
    {
        auto *report = pending.takeFirst();
        if (!report || visited.contains(report))
            continue;
        visited.insert(report);
        for (auto *pin : report->terminals())
            for (auto *wire : pin->conductors())
            {
                auto *other = wire->terminal1 == pin ? wire->terminal2 : wire->terminal1;
                if (!other || !other->parentElement())
                    continue;
                auto *element = other->parentElement();
                if (element->linkType() & Element::AllReport)
                {
                    if (!visited.contains(element))
                    {
                        pending << element << element->linkedElements();
                    }
                }
                else if (!result.contains(other))
                    result << other;
            }
    }
    return result;
}
} // namespace

QJsonObject defaults()
{
    return {{"enabled", false},
            {"x_start", 50},
            {"y_base", 0},
            {"paso_x", 20},
            {"hose_delim", "-"},
            {"borna_delim", ":"},
            {"dark_mode", false},
            {"char_adv_ext", 5.2},
            {"offset_ext", 0},
            {"offset_int", 0},
            {"line_borna_len", 20},
            {"line_manguera_len", 120},
            {"line_dispositivo_len", 20},
            {"cable_text_offset_int", 0},
            {"hose_text_offset_int", 0},
            {"cable_text_offset_ext", 0},
            {"hose_text_offset_ext", -40},
            {"cable_line_len_ext", 100},
            {"cable_line_len_int", 100},
            {"hose_line_len_ext", 50},
            {"hose_line_len_int", 50},
            {"open_after_export", true},
            {"draw_ground_line", true},
            {"draw_frame", false},
            {"font_family", "Courier New"},
            {"font_family_ext", "Courier New"},
            {"font_family_int", "Courier New"},
            {"font_size_ext", 7},
            {"font_size_int", 7},
            {"font_bold_ext", true},
            {"font_bold_int", true},
            {"hose_font_family", "Courier New"},
            {"hose_font_size", 7},
            {"hose_font_bold", true},
            {"hose_group_mode", "Normal"},
            {"enable_multilevel", true},
            {"draw_shield_symbol", true},
            {"reserve_count", 0},
            {"reserve_label_prefix", "R"},
            {"auto_detect_reserves", true},
            {"external_name_exceptions", ""},
            {"enable_terminal_colors", true},
            {"terminal_default_color", "lightgray"},
            {"terminal_color_rules_json",
             "[{\"text\":\"PE\",\"color\":\"green|yellow\"},{\"text\":\"BL\",\"color\":\"dodgerblue\"},{"
             "\"text\":\"RT\",\"color\":\"red\"},{\"text\":\"SW\",\"color\":\"darkslategray\"}]"},
            {"terminal_color_gray_codes", ""},
            {"terminal_color_blue_codes", "BL"},
            {"terminal_color_red_codes", "RT"},
            {"terminal_color_black_codes", "SW"},
            {"terminal_color_pe_codes", "PE"},
            {"multilevel_sep", QString::fromUtf8("·")}};
}
QString encode(const QJsonObject &config)
{
    return QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Compact));
}
QString key(const QString &field)
{
    return QStringLiteral("faramax_tb_") + field;
}
QString nativeType(const QString &name)
{
    const QMap<QString, QString> names{{"De Paso", "generic"},
                                       {"Seccionable", "sectional"},
                                       {"Tierra", "ground"},
                                       {"Fusible", "fuse"},
                                       {"Diodo", "diode"}};
    return names.value(name, name);
}
QString displayType(const QString &name)
{
    const QMap<QString, QString> names{{"generic", "De Paso"},
                                       {"sectional", "Seccionable"},
                                       {"ground", "Tierra"},
                                       {"fuse", "Fusible"},
                                       {"diode", "Diodo"}};
    return names.value(name, name);
}
QJsonObject settings(TerminalStrip *strip)
{
    auto config = defaults();
    if (strip)
    {
        const auto stored =
            QJsonDocument::fromJson(strip->property(settingsProperty).toString().toUtf8()).object();
        for (auto it = stored.begin(); it != stored.end(); ++it)
            config.insert(it.key(), it.value());
    }
    return config;
}
bool enabled(TerminalStrip *strip)
{
    return strip && settings(strip).value("enabled").toBool();
}

QString classify(const QString &device, bool simple, bool ground, const QJsonObject &config)
{
    if (ground || device.trimmed() == "PE")
        return "INT";
    const auto base = device.section(':', 0, 0).trimmed().toUpper();
    for (auto part : config.value("external_name_exceptions")
                         .toString()
                         .split(QRegularExpression("[\\n,;]+"), Qt::SkipEmptyParts))
    {
        part = part.trimmed().toUpper();
        QString pattern = part, side = "EXT";
        int split = part.lastIndexOf('|'), length = 1;
        if (split < 0)
        {
            split = part.lastIndexOf("=>");
            length = 2;
        }
        if (split >= 0)
        {
            pattern = part.left(split).trimmed();
            side = part.mid(split + length).trimmed();
        }
        if (side != "EXT" && side != "INT")
            continue;
        if ((!pattern.isEmpty() && pattern.endsWith('*') &&
             base.startsWith(pattern.left(pattern.size() - 1))) ||
            base == pattern)
            return side;
    }
    return simple ? "EXT" : "INT";
}

QVector<Row> rows(TerminalStrip *strip, const QJsonObject &given)
{
    QVector<Row> result;
    if (!strip)
        return result;
    auto config = settings(strip);
    for (auto it = given.begin(); it != given.end(); ++it)
        config.insert(it.key(), it.value());
    for (const auto &physical : strip->physicalTerminal())
        for (const auto &real : physical->realTerminals())
        {
            if (!real->isSpare() && (!real->element() || !real->element()->diagram() ||
                                     real->element()->diagram()->project() != strip->project()))
                continue;
            Row row;
            row.terminal = real;
            row.physical = physical;
            row.before = real->terminalData();
            row.id = physical->uuid().toString() + "/" + real->elementUuid().toString();
            auto &v = row.values;
            v = {{"posicion", real->Xref()},
                 {"tipo_borna", ElementData::terminalTypeToString(real->type())},
                 {"puente", real->isBridged()},
                 {"bornero", strip->name()},
                 {"num_borna", number(real->label(), config)},
                 {"modelo", model(row.before)},
                 {"nivel", real->level()},
                 {"color", TerminalColor::toSpec(real->color(), real->stripeColor())},
                 {"corte", false},
                 {"peine", false},
                 {"peine_grupo", 0},
                 {"shl_ext", false},
                 {"shl_int", real->isShield()},
                 {"mang_shl_ext", ""},
                 {"mang_shl_int", ""}};
            if (auto bridge = real->bridge())
                v.insert("_bridge", bridge->uuid().toString() + bridge->color().name());
            QStringList external, internal, wiresExt, wiresInt, hosesExt, hosesInt;
            bool connected = false;
            if (auto *element = real->element())
            {
                v.insert("x", element->scenePos().x());
                v.insert("y", element->scenePos().y());
                // Only the actual electrical graph is used: equal wire numbers
                // on unrelated potentials never create a fictitious connection.
                for (auto *pin : element->terminals())
                    for (auto *wire : pin->conductors())
                    {
                        connected = true;
                        auto *peer = wire->terminal1 == pin ? wire->terminal2 : wire->terminal1;
                        if (!peer || !peer->parentElement())
                            continue;
                        auto properties = wire->properties();
                        auto devices = endpoints(peer);
                        if (devices.isEmpty())
                            devices << nullptr;
                        QString cable;
                        const auto own = wire->ownProperties();
                        if (!own.m_cable_uuid.isNull() && strip->project())
                        {
                            for (auto *nativeCable : strip->project()->cables())
                                if (nativeCable->uuid() == own.m_cable_uuid)
                                {
                                    cable = nativeCable->designation();
                                    break;
                                }
                        }
                        if (cable.isEmpty())
                            cable = hose(properties.text, config);
                        for (auto *destination : devices)
                        {
                            auto *other = destination ? destination->parentElement() : nullptr;
                            const auto device = destination ? endpoint(destination) : QStringLiteral("?");
                            const bool ground =
                                device == "PE" ||
                                (other && other->elementData().terminalType() == ElementData::TTGround);
                            const bool simple = other && other->elementData().m_type == ElementData::Simple;
                            const bool ext = classify(device, simple, ground, config) == "EXT";
                            unique(ext ? external : internal, device);
                            unique(ext ? wiresExt : wiresInt, properties.text);
                            unique(ext ? hosesExt : hosesInt, cable);
                        }
                    }
            }
            v.insert("disp_ext", join(external));
            v.insert("disp_int", join(internal));
            v.insert("disp_ext_options", QJsonArray::fromStringList(external));
            v.insert("disp_int_options", QJsonArray::fromStringList(internal));
            v.insert("cable_ext", join(wiresExt));
            v.insert("cable_int", join(wiresInt));
            v.insert("mang_ext", join(hosesExt));
            v.insert("mang_int", join(hosesInt));
            v.insert("reserva",
                     real->isSpare() || (config.value("auto_detect_reserves").toBool() && !connected));
            for (const auto &name : row.before.m_informations.keys())
                if (name.startsWith("faramax_tb_") && name != key("puente") && name != key("shl_int"))
                {
                    // DiagramContext serializes values as text. Restore their
                    // meaning explicitly; QJsonValue("false").toBool() is false,
                    // but QJsonValue("true").toBool() is also false.
                    const auto field = name.mid(11);
                    const auto value = row.before.m_informations.value(name);
                    const QSet<QString> flags{"corte",   "peine",  "reserva",
                                              "shl_ext", "double", "auto_reserve"};
                    if (flags.contains(field))
                        v.insert(field, value.toBool());
                    else if (field == "peine_grupo")
                        v.insert(field, value.toInt());
                    else
                        v.insert(field, QJsonValue::fromVariant(value));
                }
            if (v.value("color").toString().isEmpty())
                v.insert("color", "Auto");
            result << row;
        }
    for (int i = 0; i < result.size(); ++i)
    {
        const int count = result[i].values.value("peine_grupo").toInt();
        if (count <= 0)
            continue;
        int active = 0;
        for (int j = i; j < qMin(i + count, int(result.size())); ++j)
        {
            const bool legacy = j > i && result[j].values.value("peine_grupo").toInt() == 0;
            if (legacy && result[j].terminal->bridge() &&
                result[j].terminal->bridge() == result[i].terminal->bridge())
                result[j].values["peine"] = true;
            if (result[j].values.value("peine").toBool())
                ++active;
        }
        if (active > 1)
            for (int j = i; j < qMin(i + count, int(result.size())); ++j)
                if (!result[j].terminal->isBridged())
                    result[j].values["peine"] = false;
    }
    // A bridge cell describes the link to the next terminal, not membership
    // of an entire electrical group. Comb contacts have their own metadata.
    for (int i = 0; i < result.size(); ++i)
    {
        auto &r = result[i];
        bool link = false;
        int next = i + 1;
        while (next < result.size() && result[next].terminal->level() != r.terminal->level())
            ++next;
        if (next < result.size() && r.terminal->bridge() &&
            r.terminal->bridge() == result[next].terminal->bridge())
        {
            bool comb = false;
            for (const auto &member : r.terminal->bridge()->realTerminals())
                comb |= member->terminalData().m_informations.value(key("peine")).toBool();
            const auto info = r.before.m_informations;
            link = info.contains(key("link_next")) ? info.value(key("link_next")).toBool() : !comb;
        }
        r.values["puente"] = link;
    }
    return result;
}

QStringList validate(const QVector<Row> &data, const QJsonObject &config)
{
    QStringList messages;
    QMap<QString, int> labels;
    QSet<QString> doubleLabels;
    QSet<qlonglong> numbers;
    for (const auto &row : data)
    {
        const auto v = row.values;
        const auto label = v.value("num_borna").toString();
        const auto doubleId = row.physical ? row.physical->uuid().toString() + label : QString();
        if (!v.value("double").toBool() || doubleId.isEmpty() || !doubleLabels.contains(doubleId))
            labels[label]++;
        if (v.value("double").toBool())
            doubleLabels.insert(doubleId);
        bool ok = false;
        const auto n = label.toLongLong(&ok);
        if (ok && n > 0)
            numbers.insert(n);
        const auto full = v.value("bornero").toString() + config.value("borna_delim").toString(":") + label;
        if (v.value("modelo").toString().isEmpty())
            messages << tr("%1: sin modelo de borna.").arg(full);
        for (const auto &side : {"disp_ext", "disp_int"})
        {
            for (const auto &device : v.value(side).toString().split(" & ", Qt::SkipEmptyParts))
                if (device == "?" || device.startsWith(':') || device.endsWith(':'))
                    messages << tr("%1: destino incompleto (%2).").arg(full, device);
        }
        if (row.terminal && row.terminal->element())
        {
            const auto p = row.terminal->element()->scenePos();
            if (qAbs(p.x() / 10.0 - qRound(p.x() / 10.0)) > 0.01 ||
                qAbs(p.y() / 10.0 - qRound(p.y() / 10.0)) > 0.01)
                messages << tr("%1: fuera de la malla de 10 px.").arg(full);
        }
        const bool pe = v.value("modelo").toString().contains("PE", Qt::CaseInsensitive);
        if (pe != (v.value("tipo_borna").toString() == "ground"))
            messages << tr("%1: revisar correspondencia modelo PE / tipo tierra.").arg(full);
        if (v.value("reserva").toBool() &&
            (!v.value("disp_ext").toString().isEmpty() || !v.value("disp_int").toString().isEmpty()))
            messages << tr("%1: reserva con conexiones, revisar.").arg(full);
    }
    for (auto it = labels.begin(); it != labels.end(); ++it)
        if (it.value() > 1)
            messages << tr("Etiqueta repetida %1 (%2 filas).").arg(it.key()).arg(it.value());
    auto sorted = numbers.values();
    std::sort(sorted.begin(), sorted.end());
    for (int i = 1; i < sorted.size(); ++i)
        if (sorted[i] > sorted[i - 1] + 1)
            messages << tr("Hueco de numeración: %1–%2.").arg(sorted[i - 1] + 1).arg(sorted[i] - 1);
    return messages;
}

namespace
{
class OrderCommand final : public QUndoCommand
{
    QPointer<TerminalStrip> strip;
    QVector<QSharedPointer<PhysicalTerminal>> oldOrder, newOrder;

  public:
    OrderCommand(TerminalStrip *s, const QVector<QSharedPointer<PhysicalTerminal>> &o)
        : strip(s), oldOrder(s->physicalTerminal()), newOrder(o)
    {
        setText(tr("Ordenar bornero GNerador TB"));
    }
    void redo() override
    {
        if (strip)
            strip->setOrderTo(newOrder);
    }
    void undo() override
    {
        if (strip)
            strip->setOrderTo(oldOrder);
    }
};
class ExpandCommand final : public QUndoCommand
{
    QPointer<TerminalStrip> strip;
    QSharedPointer<PhysicalTerminal> receiver;
    QVector<QSharedPointer<RealTerminal>> spares;
    QSharedPointer<RealTerminal> first;
    ElementData before, after;

  public:
    ExpandCommand(TerminalStrip *s, const Row &r, const QStringList &levels, const QJsonObject &cfg)
        : strip(s), receiver(r.physical), first(r.terminal), before(r.before), after(r.before)
    {
        setText(tr("Ampliar borna multinivel"));
        const bool doubled = levels.first() == "=";
        const auto base =
            s->name() + cfg.value("borna_delim").toString(":") + r.values.value("num_borna").toString();
        after.m_informations.addValue(
            "label", base + (doubled ? QString() : cfg.value("multilevel_sep").toString() + levels.first()));
        after.m_informations.addValue("formula", QString());
        if (doubled)
            after.m_informations.addValue(key("double"), true, false);
        for (int i = 1; i < levels.size(); ++i)
        {
            auto d = r.before;
            for (const auto &k : d.m_informations.keys())
                if (k.startsWith("faramax_tb_"))
                    d.m_informations.remove(k);
            d.m_informations.addValue(
                "label", base + (doubled ? QString() : cfg.value("multilevel_sep").toString() + levels[i]));
            d.m_informations.addValue("formula", QString());
            if (doubled)
                d.m_informations.addValue(key("double"), true, false);
            spares << RealTerminal::createSpare(d);
        }
    }
    void setFirst(const ElementData &data)
    {
        if (first->element())
            first->element()->setElementData(data);
        else
            first->setSpareData(data);
    }
    void redo() override
    {
        if (strip)
        {
            setFirst(after);
            strip->insertSpareTerminals(strip->physicalTerminalCount(), spares);
            strip->groupTerminals(receiver, spares);
        }
    }
    void undo() override
    {
        if (strip)
        {
            strip->removeTerminals(spares);
            setFirst(before);
        }
    }
};
using TerminalGroup = QVector<QSharedPointer<RealTerminal>>;
struct BridgeState
{
    QSharedPointer<TerminalStripBridge> bridge;
    TerminalGroup members;
};
QVector<BridgeState> bridges(TerminalStrip *strip)
{
    QVector<BridgeState> state;
    QSet<QUuid> seen;
    for (const auto &real : strip->realTerminals())
        if (auto b = real->bridge(); b && !seen.contains(b->uuid()))
        {
            state.append({b, b->realTerminals()});
            seen.insert(b->uuid());
        }
    return state;
}
// The strip owns all electrical relationships. This command only keeps the
// before/after native groups needed by Undo, including bridges being extended.
class BridgesCommand final : public QUndoCommand
{
    QPointer<TerminalStrip> strip;
    QVector<BridgeState> before, after;
    void install(QVector<BridgeState> &state)
    {
        if (!strip)
            return;
        for (const auto &existing : bridges(strip))
            strip->unBridge(existing.members);
        for (auto &b : state)
        {
            if (b.bridge)
                strip->setBridge(b.bridge, b.members, true);
            else
            {
                strip->setBridge(b.members, true);
                b.bridge = strip->isBridged(b.members.first());
            }
        }
    }

  public:
    BridgesCommand(TerminalStrip *s, QVector<BridgeState> next, QUndoCommand *parent)
        : QUndoCommand(parent), strip(s), before(bridges(s)), after(std::move(next))
    {
    }
    void redo() override
    {
        install(after);
    }
    void undo() override
    {
        install(before);
    }
};
class RemoveCommand final : public QUndoCommand
{
    QPointer<TerminalStrip> strip;
    QVector<QSharedPointer<PhysicalTerminal>> removed, order;
    QVector<BridgeState> before, after;
    bool captured = false;
    void clearBridges()
    {
        for (const auto &b : bridges(strip))
            strip->unBridge(b.members);
    }
    void restore(const QVector<BridgeState> &state)
    {
        for (const auto &b : state)
            strip->setBridge(b.bridge, b.members, true);
    }

  public:
    RemoveCommand(TerminalStrip *s, const QVector<QSharedPointer<PhysicalTerminal>> &selected,
                  QUndoCommand *parent = nullptr)
        : QUndoCommand(parent), strip(s), removed(selected), order(s->physicalTerminal())
    {
        setText(tr("Retirar bornas del bornero"));
    }
    void redo() override
    {
        if (strip)
        {
            if (!captured)
            {
                before = bridges(strip);
                for (const auto &b : before)
                {
                    auto remaining = b.members;
                    for (int i = remaining.size() - 1; i >= 0; --i)
                        if (removed.contains(remaining[i]->physicalTerminal()))
                            remaining.removeAt(i);
                    if (remaining.size() > 1)
                        after.append({b.bridge, remaining});
                }
                captured = true;
            }
            clearBridges();
            strip->removeTerminals(removed);
            restore(after);
        }
    }
    void undo() override
    {
        if (strip)
        {
            clearBridges();
            strip->insertTerminals(0, removed);
            strip->setOrderTo(order);
            restore(before);
        }
    }
};
} // namespace

QUndoCommand *removeCommand(TerminalStrip *strip, const QVector<Row> &selection)
{
    if (!strip || !strip->project() || strip->project()->isReadOnly())
        return nullptr;
    QVector<QSharedPointer<PhysicalTerminal>> selected;
    for (const auto &r : selection)
        if (r.physical && r.physical->terminalStrip() == strip && !selected.contains(r.physical))
            selected << r.physical;
    return selected.isEmpty() ? nullptr : new RemoveCommand(strip, selected);
}

QUndoCommand *orderCommand(TerminalStrip *strip, const QVector<Row> &order)
{
    if (!strip || !strip->project() || strip->project()->isReadOnly())
        return nullptr;
    QVector<QSharedPointer<PhysicalTerminal>> sorted;
    for (const auto &r : order)
        if (r.physical && !sorted.contains(r.physical))
            sorted << r.physical;
    for (const auto &p : strip->physicalTerminal())
        if (!sorted.contains(p))
            sorted << p;
    return sorted == strip->physicalTerminal() ? nullptr : new OrderCommand(strip, sorted);
}
QUndoCommand *expandCommand(TerminalStrip *strip, const Row &row, const QStringList &levels, QString &error)
{
    error.clear();
    const bool doubled = levels == QStringList{"=", "="};
    if (!strip || !strip->project() || strip->project()->isReadOnly() || !row.terminal || !row.physical ||
        row.physical->realTerminals().size() != 1 || levels.size() < 2 || levels.size() > 4 ||
        (!doubled && QSet<QString>(levels.begin(), levels.end()).size() != levels.size()))
    {
        error = tr("Selecciona una borna simple y de 2 a 4 niveles distintos.");
        return nullptr;
    }
    for (const auto &level : levels)
        if (level.trimmed().isEmpty())
        {
            error = tr("El nombre de nivel no puede estar vacío.");
            return nullptr;
        }
    if (row.terminal->element() && row.terminal->element()->diagram()->isReadOnly())
    {
        error = tr("El folio es de solo lectura.");
        return nullptr;
    }
    const auto cfg = settings(strip);
    for (const auto &level : levels)
    {
        const auto label = strip->name() + cfg.value("borna_delim").toString() +
                           row.values.value("num_borna").toString() +
                           (doubled ? QString() : cfg.value("multilevel_sep").toString() + level);
        for (const auto &real : strip->realTerminals())
            if (real != row.terminal && real->label() == label)
            {
                error = tr("Ya existe la etiqueta %1.").arg(label);
                return nullptr;
            }
    }
    return new ExpandCommand(strip, row, levels, settings(strip));
}

QUndoCommand *editCommand(TerminalStrip *strip, const QVector<Row> &edited, const QJsonObject &config,
                          QString &error)
{
    error.clear();
    if (!strip || !strip->project() || strip->project()->isReadOnly() ||
        !strip->project()->terminalStrip().contains(strip))
    {
        error = tr("El proyecto es de solo lectura.");
        return nullptr;
    }
    if (config.value("paso_x").toDouble() < 10 || config.value("reserve_count").toInt() < 0 ||
        config.value("reserve_count").toInt() > 10000 || config.value("borna_delim").toString().isEmpty() ||
        config.value("hose_delim").toString().isEmpty())
    {
        error = tr("Revisa el paso, los delimitadores y la cantidad de reservas.");
        return nullptr;
    }
    for (auto it = config.begin(); it != config.end(); ++it)
        if (it->isDouble() && qAbs(it->toDouble()) > 100000)
        {
            error = tr("Valor fuera de rango en %1.").arg(it.key());
            return nullptr;
        }
    const auto rules = config.value("terminal_color_rules_json").toString();
    if (!rules.isEmpty() && !QJsonDocument::fromJson(rules.toUtf8()).isArray())
    {
        error = tr("Las reglas de color deben ser una lista JSON válida.");
        return nullptr;
    }
    const auto current = strip->realTerminals();
    for (const auto &r : edited)
        if (!r.edits.isEmpty() &&
            (!r.terminal || !current.contains(r.terminal) || r.terminal->terminalData() != r.before))
        {
            error = tr("Una borna ha cambiado o se ha eliminado. Revisa la tabla antes de aplicar.");
            return nullptr;
        }
    for (const auto &r : edited)
        if (!r.edits.isEmpty() && r.terminal->element() && r.terminal->element()->diagram() &&
            r.terminal->element()->diagram()->isReadOnly())
        {
            error = tr("Una borna pertenece a un folio de solo lectura.");
            return nullptr;
        }
    QVector<BridgeState> plannedBridges;
    QSet<QUuid> graphTouched;
    bool bridgeEdited = false;
    for (const auto &r : edited)
        bridgeEdited |=
            r.edits.contains("puente") || r.edits.contains("peine") || r.edits.contains("peine_grupo");
    auto effective = rows(strip, config);
    for (auto &r : effective)
        for (const auto &edit : edited)
            if (edit.terminal == r.terminal)
            {
                for (auto it = edit.edits.begin(); it != edit.edits.end(); ++it)
                    r.values[it.key()] = it.value();
                break;
            }
    if (bridgeEdited)
    {
        QVector<int> parent(effective.size());
        for (int i = 0; i < parent.size(); ++i)
            parent[i] = i;
        auto root = [&](int n)
        {
            while (parent[n] != n)
                n = parent[n];
            return n;
        };
        auto unite = [&](int a, int b)
        {
            parent[root(b)] = root(a);
        };
        // Native bridges can be sparse and may have no GNerador annotations.
        // An edit elsewhere must preserve their exact membership.

        for (const auto &edit : edited)
            if (edit.edits.contains("puente") || edit.edits.contains("peine") ||
                edit.edits.contains("peine_grupo"))
                if (edit.terminal->bridge())
                    graphTouched.insert(edit.terminal->bridge()->uuid());
        QMap<QUuid, int> nativeFirst;
        for (int i = 0; i < effective.size(); ++i)
            if (auto bridge = effective[i].terminal->bridge();
                bridge && !graphTouched.contains(bridge->uuid()))
            {
                if (nativeFirst.contains(bridge->uuid()))
                    unite(nativeFirst.value(bridge->uuid()), i);
                else
                    nativeFirst[bridge->uuid()] = i;
            }
        for (int i = 0; i < effective.size(); ++i)
            if (effective[i].values.value("puente").toBool())
            {
                int j = i + 1;
                while (j < effective.size() &&
                       effective[j].terminal->level() != effective[i].terminal->level())
                    ++j;
                if (j == effective.size() || effective[j].physical->pos() != effective[i].physical->pos() + 1)
                {
                    error = tr("El puente necesita la siguiente borna del mismo nivel.");
                    return nullptr;
                }
                unite(i, j);
            }
        for (int i = 0; i < effective.size(); ++i)
        {
            const int count = effective[i].values.value("peine_grupo").toInt();
            if (count <= 0)
                continue;
            if (i + count > effective.size())
            {
                error = tr("El peine supera el final del bornero.");
                return nullptr;
            }
            int first = -1;
            for (int j = i; j < i + count; ++j)
            {
                const bool legacy = j > i && effective[j].values.value("peine_grupo").toInt() == 0;
                if (!effective[j].values.value("peine").toBool() && !legacy)
                    continue;
                if (effective[j].terminal->level() != effective[i].terminal->level() ||
                    (j > i && effective[j].physical == effective[i].physical))
                {
                    error =
                        tr("Los contactos del peine deben pertenecer a bornas distintas del mismo nivel.");
                    return nullptr;
                }
                if (first < 0)
                    first = j;
                else
                    unite(first, j);
            }
        }
        QMap<int, TerminalGroup> groups;
        for (int i = 0; i < parent.size(); ++i)
            groups[root(i)] << effective[i].terminal;
        const auto existing = bridges(strip);
        for (const auto &members : groups)
            if (members.size() > 1)
            {
                QSharedPointer<TerminalStripBridge> preserved;
                for (const auto &previous : existing)
                    if (previous.members.size() == members.size())
                    {
                        bool same = true;
                        for (const auto &member : members)
                            same &= previous.members.contains(member);
                        if (same)
                        {
                            preserved = previous.bridge;
                            break;
                        }
                    }
                plannedBridges.append({preserved, members});
            }
    }
    auto macro = std::make_unique<QUndoCommand>(tr("GNerador TB: editar bornero"));
    const auto encoded = encode(config);
    if (strip->property(settingsProperty).toString() != encoded)
        new QPropertyUndoCommand(strip, settingsProperty, strip->property(settingsProperty), encoded,
                                 macro.get());
    for (const auto &r : edited)
    {
        const bool affectedBridge =
            bridgeEdited &&
            (r.edits.contains("puente") || r.edits.contains("peine") || r.edits.contains("peine_grupo") ||
             (r.terminal->bridge() && graphTouched.contains(r.terminal->bridge()->uuid())));
        if (r.edits.isEmpty() && !affectedBridge)
            continue;
        if (affectedBridge && r.terminal->element() && r.terminal->element()->diagram() &&
            r.terminal->element()->diagram()->isReadOnly())
        {
            error = tr("El puente afecta a una borna de un folio de solo lectura.");
            return nullptr;
        }
        auto data = r.edits.isEmpty() ? r.terminal->terminalData() : r.before;
        if (affectedBridge)
            for (const auto &proposal : effective)
                if (proposal.terminal == r.terminal)
                {
                    data.m_informations.addValue(key("link_next"), proposal.values.value("puente").toBool(),
                                                 false);
                    break;
                }
        for (auto change = r.edits.begin(); change != r.edits.end(); ++change)
        {
            const auto field = change.key();
            if (field == "num_borna")
            {
                if (change.value().toString().trimmed().isEmpty())
                {
                    error = tr("El número de borna no puede estar vacío.");
                    return nullptr;
                }
                data.m_informations.addValue("label", strip->name() +
                                                          config.value("borna_delim").toString(":") +
                                                          change.value().toString());
                data.m_informations.addValue("formula", QString());
                // A deliberately renumbered reserve is now user-owned.
                if (r.terminal->isSpare())
                    data.m_informations.remove(key("auto_reserve"));
            }
            else if (field == "tipo_borna")
            {
                const auto type = nativeType(change.value().toString());
                const QSet<QString> valid{"generic",     "sectional",           "ground",   "fuse",
                                          "diode",       "antiparallel_diodes", "resistor", "varistor",
                                          "test_socket", "diode_reversed"};
                if (!valid.contains(type))
                {
                    error = tr("Tipo de borna no reconocido: %1").arg(type);
                    return nullptr;
                }
                data.setTerminalType(ElementData::terminalTypeFromString(type));
            }
            else if (field == "modelo")
                data.m_informations.addValue("manufacturer_reference", change.value().toString());
            else if (field == "shl_int")
                data.m_informations.addValue(RealTerminal::shieldInfoKey(), change.value().toBool(), false);
            else if (field == "color")
            {
                QColor fill, stripe;
                QString spec = change.value().toString();
                spec.replace('|', '/');
                spec.replace("HTMLGrayLightGray", "lightgray");
                spec.replace("HTMLBlueDodgerBlue", "dodgerblue");
                spec.replace("HTMLGrayDarkSlateGray", "darkslategray");
                if (spec.compare("Auto", Qt::CaseInsensitive) == 0)
                    spec.clear();
                if (!TerminalColor::parse(spec, &fill, &stripe))
                {
                    error = tr("Color no válido: %1").arg(spec);
                    return nullptr;
                }
                data.m_informations.addValue(RealTerminal::colorInfoKey(),
                                             TerminalColor::toSpec(fill, stripe));
            }
            else if (field == "nivel")
            {
                const int level = change.value().toInt(-1);
                if (level < 0 || level > 3 || level >= r.physical->realTerminalCount())
                {
                    error = tr("Ese nivel no existe. Utiliza Multinivel para ampliar la borna.");
                    return nullptr;
                }
                if (level != r.terminal->level() &&
                    (bridgeEdited || r.terminal->isBridged() || r.physical->realTerminal(level)->isBridged()))
                {
                    error = tr("Retira primero los puentes antes de cambiar el nivel de sus bornas.");
                    return nullptr;
                }
                new ChangeTerminalLevel(strip, r.terminal.toWeakRef(), level, macro.get());
            }
            else if (field == "puente" || field == "peine")
            {
                if (field == "peine")
                    data.m_informations.addValue(key(field), change.value().toVariant(), false);
            }
            else if (field != "posicion" && field != "bornero")
            {
                // Empty manual text clears the override and resumes automatic calculation.
                if (change.value().isString() && change.value().toString().isEmpty())
                    data.m_informations.remove(key(field));
                else
                    data.m_informations.addValue(key(field), change.value().toVariant(), false);
            }
        }
        if (data != r.terminal->terminalData())
        {
            if (auto *element = r.terminal->element())
                new ChangeElementDataCommand(element, data, macro.get());
            else
                new ChangeSpareTerminalDataCommand(r.terminal, data, macro.get());
        }
    }
    if (bridgeEdited)
        new BridgesCommand(strip, plannedBridges, macro.get());
    const int wanted = config.value("reserve_count").toInt();
    QVector<QSharedPointer<RealTerminal>> automatic;
    QSet<QString> used;
    for (const auto &real : current)
    {
        bool managed =
            real->isSpare() && real->terminalData().m_informations.value(key("auto_reserve")).toBool();
        QString label = real->label();
        for (const auto &r : edited)
            if (r.terminal == real)
            {
                if (r.edits.contains("num_borna"))
                {
                    managed = false;
                    label = strip->name() + config.value("borna_delim").toString(":") +
                            r.edits.value("num_borna").toString();
                }
                if (r.edits.contains("auto_reserve"))
                    managed = r.edits.value("auto_reserve").toBool();
                break;
            }
        if (managed)
            automatic << real;
        else
            used.insert(label);
    }
    auto prefix = config.value("reserve_label_prefix").toString("R").trimmed();
    if (prefix.isEmpty())
        prefix = "R";
    QStringList desired;
    for (int n = 1; desired.size() < wanted; ++n)
    {
        const auto label =
            strip->name() + config.value("borna_delim").toString(":") + prefix + QString::number(n);
        if (!used.contains(label))
            desired << label;
    }
    QSet<QString> kept;
    QVector<QSharedPointer<PhysicalTerminal>> remove;
    for (const auto &real : automatic)
    {
        if (desired.contains(real->label()) && !kept.contains(real->label()))
        {
            kept.insert(real->label());
            continue;
        }
        if (real->physicalTerminal()->realTerminalCount() != 1)
        {
            error = tr("Una reserva automática está agrupada: utiliza Reservas manuales antes de cambiar la "
                       "cantidad.");
            return nullptr;
        }
        remove << real->physicalTerminal();
    }
    if (!remove.isEmpty())
        new RemoveCommand(strip, remove, macro.get());
    QVector<QSharedPointer<RealTerminal>> add;
    for (const auto &label : desired)
        if (!kept.contains(label))
        {
            ElementData data;
            data.setTerminalType(ElementData::TTGeneric);
            data.m_informations.addValue("label", label);
            data.m_informations.addValue("manufacturer_reference", "RESERVA");
            data.m_informations.addValue(key("auto_reserve"), true, false);
            add << RealTerminal::createSpare(data);
        }
    if (!add.isEmpty())
        new AddSpareTerminalsCommand(strip, -1, add, macro.get());
    return macro->childCount() ? macro.release() : nullptr;
}

namespace
{
class LiveProject final : public QObject
{
    QPointer<QETProject> project;
    QTimer timer;
    QVector<QMetaObject::Connection> connections;
    QMap<QUuid, QByteArray> signatures;

  public:
    explicit LiveProject(QETProject *p) : QObject(p), project(p)
    {
        setObjectName("faramaxTBWatcher");
        timer.setSingleShot(true);
        timer.setInterval(0);
        connect(&timer, &QTimer::timeout, this,
                [this]()
                {
                    refresh();
                });
        request();
    }
    void request()
    {
        if (!timer.isActive())
            timer.start();
    }
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::DynamicPropertyChange &&
            static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName() == settingsProperty)
            request();
        return false;
    }
    void refresh()
    {
        if (!project)
            return;
        for (const auto &c : connections)
            disconnect(c);
        connections.clear();
        connections << connect(project->undoStack(), &QUndoStack::indexChanged, this,
                               [this]()
                               {
                                   request();
                               });
        connections << connect(project, &QETProject::diagramAdded, this,
                               [this]()
                               {
                                   request();
                               });
        connections << connect(project, &QETProject::diagramRemoved, this,
                               [this]()
                               {
                                   request();
                               });
        connections << connect(project, &QETProject::projectDiagramsOrderChanged, this,
                               [this]()
                               {
                                   request();
                               });
        connections << connect(project, &QETProject::cableAdded, this,
                               [this]()
                               {
                                   request();
                               });
        connections << connect(project, &QETProject::cableRemoved, this,
                               [this]()
                               {
                                   request();
                               });
        for (auto *cable : project->cables())
            connections << connect(cable, &Cable::changed, this,
                                   [this]()
                                   {
                                       request();
                                   });
        for (auto *diagram : project->builtFolios())
        {
            connections << connect(&diagram->border_and_titleblock, &BorderTitleBlock::informationChanged,
                                   this,
                                   [this]()
                                   {
                                       request();
                                   });
            for (auto *element : diagram->elements())
            {
                connections << connect(element, &Element::elementInfoChange, this,
                                       [this]()
                                       {
                                           request();
                                       });
                connections << connect(element, &Element::linkedElementChanged, this,
                                       [this]()
                                       {
                                           request();
                                       });
                connections << connect(element, &Element::xChanged, this,
                                       [this]()
                                       {
                                           request();
                                       });
                connections << connect(element, &Element::yChanged, this,
                                       [this]()
                                       {
                                           request();
                                       });
                for (auto *pin : element->terminals())
                {
                    connections << connect(pin, &Terminal::conductorWasAdded, this,
                                           [this]()
                                           {
                                               request();
                                           });
                    connections << connect(pin, &Terminal::conductorWasRemoved, this,
                                           [this]()
                                           {
                                               request();
                                           });
                    for (auto *wire : pin->conductors())
                        connections << connect(wire, &Conductor::propertiesChange, this,
                                               [this]()
                                               {
                                                   request();
                                               });
                }
            }
        }
        for (auto *strip : project->terminalStrip())
        {
            strip->installEventFilter(this);
            connections << connect(strip, &TerminalStrip::orderChanged, this,
                                   [this]()
                                   {
                                       request();
                                   });
            connections << connect(strip, &TerminalStrip::bridgeChanged, this,
                                   [this]()
                                   {
                                       request();
                                   });
            connections << connect(strip, &TerminalStrip::bridgeColorChanged, this,
                                   [this]()
                                   {
                                       request();
                                   });
            connections << connect(strip, &TerminalStrip::dataChanged, this,
                                   [this]()
                                   {
                                       request();
                                   });
            QJsonArray state;
            for (const auto &r : rows(strip))
                state.append(QJsonObject{{"id", r.id}, {"values", r.values}});
            state.append(settings(strip));
            state.append(strip->installation());
            state.append(strip->location());
            state.append(strip->name());
            const auto fingerprint = QJsonDocument(state).toJson(QJsonDocument::Compact);
            const auto uuid = strip->uuid();
            if (signatures.value(uuid) != fingerprint)
            {
                signatures[uuid] = fingerprint;
                strip->terminalDataChanged();
            }
        }
    }
};
} // namespace
void watch(QETProject *project)
{
    if (!project)
        return;
    if (auto *existing = project->findChild<QObject *>("faramaxTBWatcher", Qt::FindDirectChildrenOnly))
        static_cast<LiveProject *>(existing)->request();
    else
        new LiveProject(project);
}
} // namespace Faramax::TB
