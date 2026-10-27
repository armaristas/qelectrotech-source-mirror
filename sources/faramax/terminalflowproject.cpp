/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "terminalflowproject.h"

#include "../QPropertyUndoCommand/qpropertyundocommand.h"
#include "../diagram.h"
#include "../qetproject.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/terminalelement.h"
#include "../undocommand/changeelementinformationcommand.h"

#include <QUndoCommand>
#include <QCoreApplication>
#include <algorithm>
#include <memory>

namespace Faramax::TerminalFlow
{
namespace
{
QString tr(const char *text) { return QCoreApplication::translate("TerminalFlow", text); }
QString scopePart(QString value, const QString &empty)
{
    value = value.trimmed();
    return value == empty ? QString() : value;
}
QString instanceId(const QString &kind, const QObject *object, const QUuid &uuid)
{
    // Legacy projects can contain two placements sharing a UUID. The plan
    // addresses live instances, without changing either persisted UUID.
    return kind + uuid.toString() + QLatin1Char('@')
        + QString::number(reinterpret_cast<quintptr>(object), 16);
}
}

Snapshot snapshot(QETProject *project, bool includeConductors)
{
    Snapshot result;
    if (!project) return result;
    // Build all relevant lazy folios before enumerating items or potentials.
    if (includeConductors) project->loadFolios();
    else project->buildFoliosHolding(ElementData::Terminal, false);
    const auto folios = project->folios();
    const auto builtFolios = project->builtFolios();
    result.folioCount = folios.size();
    for (int i = 0; i < folios.size(); ++i) {
        auto *folio = folios[i];
        if (!builtFolios.contains(folio)) continue;
        for (auto *item : folio->items()) {
            if (auto *element = qgraphicsitem_cast<Element *>(item)) {
                if (element->elementData().m_type != ElementData::Terminal) continue;
                const auto info = element->elementInformations();
                Record record;
                record.id = instanceId(QStringLiteral("e:"), element, element->uuid());
                record.label = element->actualLabel();
                record.formula = info.value(QStringLiteral("formula")).toString();
                QString plant = scopePart(info.value(QStringLiteral("plant")).toString(), QStringLiteral("="));
                QString location = scopePart(info.value(QStringLiteral("location")).toString(), QStringLiteral("+"));
                if (auto *terminal = dynamic_cast<TerminalElement *>(element)) {
                    if (auto *strip = terminal->parentTerminalStrip()) {
                        if (plant.isEmpty()) plant = scopePart(strip->installation(), QStringLiteral("="));
                        if (location.isEmpty()) location = scopePart(strip->location(), QStringLiteral("+"));
                    }
                }
                if (!plant.isEmpty() || !location.isEmpty()) record.scope = plant + QChar(0x1e) + location;
                record.folio = i;
                record.x = element->scenePos().x(); record.y = element->scenePos().y();
                record.locked = folio->isReadOnly()
                    || QET::infoFlagIsTrue(info.value(QStringLiteral("auto_num_locked")).toString());
                result.records << record;
                result.elements.insert(record.id, element);
                result.information.insert(record.id, info);
            } else if (includeConductors) {
                if (auto *wire = qgraphicsitem_cast<Conductor *>(item)) {
                    Record record;
                    record.id = instanceId(QStringLiteral("c:"), wire, wire->uuid());
                    const auto properties = wire->properties();
                    record.kind = Kind::Conductor;
                    record.label = properties.text;
                    record.formula = properties.m_formula;
                    record.folio = i;
                    record.locked = folio->isReadOnly();
                    result.records << record;
                    result.conductors.insert(record.id, wire);
                    result.properties.insert(record.id, properties);
                }
            }
        }
    }
    QMap<Conductor *, QString> potentials;
    for (auto wire : result.conductors) {
        if (potentials.contains(wire.data())) continue;
        auto related = wire->relatedPotentialConductors(true);
        related.insert(wire);
        QStringList ids;
        for (auto *part : related) ids << instanceId(QStringLiteral("c:"), part, part->uuid());
        std::sort(ids.begin(), ids.end());
        for (auto *part : related) potentials.insert(part, ids.first());
    }
    for (auto &record : result.records) {
        if (record.kind == Kind::Conductor)
            record.potential = potentials.value(result.conductors.value(record.id));
    }
    std::sort(result.records.begin(), result.records.end(), [](const Record &a, const Record &b) {
        return a.id < b.id;
    });
    return result;
}

QUndoCommand *command(QETProject *project, const Snapshot &before, const Plan &plan, QString &error)
{
    error.clear();
    if (!project || project->isReadOnly() || !plan.canApply()) {
        error = tr("El proyecto no permite aplicar esta propuesta.");
        return nullptr;
    }
    const auto now = snapshot(project, !before.conductors.isEmpty());
    const auto stale = [&]() {
        error = tr("El proyecto ha cambiado. Genera de nuevo la vista previa.");
        return static_cast<QUndoCommand *>(nullptr);
    };
    if (now.folioCount != before.folioCount || now.records.size() != before.records.size()
            || now.elements != before.elements || now.conductors != before.conductors
            || now.information != before.information || now.properties != before.properties)
        return stale();
    for (int i = 0; i < now.records.size(); ++i) {
        const auto &a = now.records[i]; const auto &b = before.records[i];
        if (a.id != b.id || a.label != b.label || a.formula != b.formula || a.scope != b.scope
                || a.potential != b.potential || a.folio != b.folio || a.x != b.x || a.y != b.y
                || a.locked != b.locked) return stale();
    }
    QMap<int, QString> proposed;
    for (const auto &change : plan.changes) {
        if (change.record < 0 || change.record >= before.records.size()
                || before.records[change.record].locked || proposed.contains(change.record)) {
            error = tr("La propuesta contiene un destino no válido o bloqueado.");
            return nullptr;
        }
        proposed.insert(change.record, change.label);
    }
    // Renumber every segment of a potential consistently. Do not silently
    // propagate to a wire the user did not see in the preview, or overwrite
    // inconsistent numbers: refuse the proposal so it can be corrected.
    QMap<QString, QSet<QString>> potentialLabels;
    QSet<QString> changedPotentials;
    for (int i = 0; i < before.records.size(); ++i) {
        const auto &record = before.records[i];
        if (record.kind != Kind::Conductor) continue;
        potentialLabels[record.potential].insert(proposed.value(i, record.label));
        if (proposed.contains(i)) changedPotentials.insert(record.potential);
    }
    for (const auto &potential : changedPotentials) {
        if (potentialLabels.value(potential).size() > 1) {
            error = tr("Un potencial tiene números incompatibles. Revisa sus conductores antes de aplicar el rango.");
            return nullptr;
        }
    }
    auto macro = std::make_unique<QUndoCommand>(tr("Terminal Flow: numeración"));
    for (const auto &change : plan.changes) {
        const auto &record = before.records[change.record];
        if (record.kind == Kind::Terminal) {
            const auto oldInfo = before.information.value(record.id);
            auto newInfo = oldInfo;
            newInfo.addValue(QStringLiteral("label"), change.label, oldInfo.keyMustShow(QStringLiteral("label")));
            newInfo.addValue(QStringLiteral("formula"), QString(), oldInfo.keyMustShow(QStringLiteral("formula")));
            new ChangeElementInformationCommand(before.elements.value(record.id), oldInfo, newInfo, macro.get());
        } else {
            const auto oldProperties = before.properties.value(record.id);
            auto newProperties = oldProperties;
            newProperties.text = change.label;
            newProperties.m_formula.clear();
            new QPropertyUndoCommand(before.conductors.value(record.id), "properties",
                QVariant::fromValue(oldProperties), QVariant::fromValue(newProperties), macro.get());
        }
    }
    return macro.release();
}
}
