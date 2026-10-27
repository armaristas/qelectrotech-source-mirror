/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "terminalflow.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <algorithm>
#include <limits>

namespace Faramax::TerminalFlow
{
namespace
{
QString tr(const char *text) { return QCoreApplication::translate("TerminalFlow", text); }

QRegularExpression labelPattern(const QString &identifier)
{
    return QRegularExpression(QStringLiteral("^(%1[0-9]+)(?::([0-9]*)([A-Z]*))?$")
        .arg(QRegularExpression::escape(normalizeIdentifier(identifier))),
        QRegularExpression::CaseInsensitiveOption);
}

QString duplicateKey(const Record &record, const QString &label)
{
    QString normalized = label.trimmed().toUpper();
    if (record.kind == Kind::Terminal) {
        static const QRegularExpression numbered(QStringLiteral("^(.*):([0-9]+)([A-Z]*)$"));
        const auto match = numbered.match(normalized);
        bool ok = false;
        const auto value = match.captured(2).toLongLong(&ok);
        if (match.hasMatch() && ok)
            normalized = match.captured(1) + QLatin1Char(':') + QString::number(value) + match.captured(3);
    }
    return QString::number(static_cast<int>(record.kind)) + QChar(0x1f)
        + record.scope + QChar(0x1f) + normalized;
}

// Wire segments of one potential intentionally carry the same number.
QString owner(const Record &record)
{
    return record.kind == Kind::Conductor && !record.potential.isEmpty()
        ? record.potential : record.id;
}

void checkConflicts(const QVector<Record> &records, Plan &plan)
{
    QMap<int, QString> replacements;
    for (const auto &change : plan.changes) replacements.insert(change.record, change.label);
    QMap<QString, QSet<QString>> finalOwners;
    QSet<QString> introduced;
    for (int i = 0; i < records.size(); ++i) {
        const auto &record = records[i];
        const QString label = replacements.value(i, record.label);
        if (label.trimmed().isEmpty()) continue;
        const QString key = duplicateKey(record, label);
        finalOwners[key].insert(owner(record));
        if (replacements.contains(i) && label.compare(record.label, Qt::CaseInsensitive) != 0)
            introduced.insert(key);
    }
    for (const auto &key : introduced) {
        if (finalOwners.value(key).size() > 1)
            plan.errors << tr("Colisión de numeración: %1").arg(key.section(QChar(0x1f), -1));
    }
    plan.errors.removeDuplicates();
}
}

QString normalizeIdentifier(QString identifier)
{
    identifier.remove(QRegularExpression(QStringLiteral("\\s+")));
    return identifier.isEmpty() ? QStringLiteral("X") : identifier.toUpper();
}

QString stripPrefix(const QString &label, const QString &identifier)
{
    const auto match = labelPattern(identifier).match(label.trimmed());
    // A trailing colon without a number or suffix is not a terminal label.
    return match.hasMatch() && !label.trimmed().endsWith(QLatin1Char(':'))
        ? match.captured(1).toUpper() : QString();
}

bool parsePages(const QString &text, int folioCount, QSet<int> &pages, QString &error)
{
    pages.clear();
    error.clear();
    const QString normalized = text.trimmed().toLower();
    if (normalized.isEmpty() || QStringList{QStringLiteral("all"), QStringLiteral("*"),
            QStringLiteral("todas"), QStringLiteral("todo")}.contains(normalized)) {
        for (int i = 0; i < folioCount; ++i) pages.insert(i);
        return true;
    }
    const auto parts = normalized.split(QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
    const QRegularExpression pattern(QStringLiteral("^([0-9]+)(?:-([0-9]+))?$"));
    if (parts.isEmpty()) error = tr("Indica al menos un folio.");
    for (const auto &part : parts) {
        const auto match = pattern.match(part);
        bool firstOk = false, lastOk = false;
        int first = match.captured(1).toInt(&firstOk);
        int last = match.captured(2).isEmpty() ? first : match.captured(2).toInt(&lastOk);
        if (match.captured(2).isEmpty()) lastOk = firstOk;
        if (!match.hasMatch() || !firstOk || !lastOk || first < 1 || last < 1
                || first > folioCount || last > folioCount) {
            error = tr("Folio o rango no válido: %1 (1–%2).").arg(part).arg(folioCount);
            pages.clear();
            return false;
        }
        if (first > last) std::swap(first, last);
        for (int i = first; i <= last; ++i) pages.insert(i - 1);
    }
    return error.isEmpty();
}

QStringList diagnose(const QVector<Record> &records, const QString &identifier)
{
    QMap<QString, QMap<qlonglong, int>> numbers;
    QMap<QString, QMap<QString, int>> labels;
    const auto pattern = labelPattern(identifier);
    for (const auto &record : records) {
        if (record.kind != Kind::Terminal) continue;
        const auto match = pattern.match(record.label.trimmed());
        if (!match.hasMatch() || match.captured(2).isEmpty()) continue;
        bool ok = false;
        const auto value = match.captured(2).toLongLong(&ok);
        if (!ok) continue;
        const auto prefix = match.captured(1).toUpper();
        QString scope = record.scope;
        scope.replace(QChar(0x1e), QStringLiteral(" / "));
        const auto key = scope.isEmpty() ? prefix : scope + QStringLiteral(" / ") + prefix;
        numbers[key][value] = 1;
        // 1A and 1B share a number intentionally; two 1A labels do not.
        labels[key][QString::number(value) + match.captured(3).toUpper()]++;
    }
    QStringList result;
    for (auto strip = numbers.cbegin(); strip != numbers.cend(); ++strip) {
        QStringList duplicates, gaps;
        const auto &stripLabels = labels[strip.key()];
        for (auto label = stripLabels.cbegin(); label != stripLabels.cend(); ++label)
            if (label.value() > 1) duplicates << label.key();
        qlonglong previous = 0;
        for (auto n = strip->cbegin(); n != strip->cend(); ++n) {
            if (n.key() > previous + 1) {
                gaps << (n.key() == previous + 2 ? QString::number(previous + 1)
                    : QStringLiteral("%1–%2").arg(previous + 1).arg(n.key() - 1));
            }
            previous = n.key();
        }
        result << tr("%1: duplicados [%2]; huecos [%3]").arg(strip.key(),
            duplicates.join(QStringLiteral(", ")), gaps.join(QStringLiteral(", ")));
    }
    return result;
}

Plan number(const QVector<Record> &records, int folioCount,
            const QVector<StripConfig> &configs, const QString &identifier,
            bool replaceExisting, bool groupSuffixes)
{
    Plan plan;
    const auto pattern = labelPattern(identifier);
    QSet<QString> processed;
    for (const auto &config : configs) {
        if (!config.enabled) continue;
        const QString prefix = config.prefix.trimmed().toUpper();
        if (processed.contains(prefix)) {
            plan.errors << tr("Configuración repetida: %1.").arg(prefix);
            continue;
        }
        processed.insert(prefix);
        QSet<int> pages;
        QString error;
        if (config.start < 0 || stripPrefix(prefix, identifier) != prefix) {
            plan.errors << tr("Configuración no válida para %1.").arg(prefix);
            continue;
        }
        if (!parsePages(config.pages, folioCount, pages, error)) {
            plan.errors << prefix + QStringLiteral(": ") + error;
            continue;
        }
        QVector<int> indices;
        for (int i = 0; i < records.size(); ++i) {
            const auto &record = records[i];
            if (record.kind != Kind::Terminal || record.locked || !pages.contains(record.folio)) continue;
            const auto match = pattern.match(record.label.trimmed());
            if (stripPrefix(record.label, identifier) != prefix) continue;
            if (!replaceExisting && !match.captured(2).isEmpty()) continue;
            indices << i;
        }
        std::sort(indices.begin(), indices.end(), [&](int a, int b) {
            const auto &left = records[a]; const auto &right = records[b];
            if (left.folio != right.folio) return left.folio < right.folio;
            const auto lx = config.xFirst ? left.x : left.y;
            const auto rx = config.xFirst ? right.x : right.y;
            if (lx != rx) return lx < rx;
            const auto ly = config.xFirst ? left.y : left.x;
            const auto ry = config.xFirst ? right.y : right.x;
            return ly != ry ? ly < ry : left.id < right.id;
        });
        qlonglong counter = config.start, previous = -1;
        QString previousScope;
        for (int index : indices) {
            const auto &record = records[index];
            const auto match = pattern.match(record.label.trimmed());
            const auto suffix = groupSuffixes ? match.captured(3).toUpper() : QString();
            qlonglong assigned;
            if (!suffix.isEmpty()) {
                if (suffix == QLatin1String("A") || previous < 0 || previousScope != record.scope)
                    previous = counter++;
                assigned = previous;
                previousScope = record.scope;
            } else {
                assigned = counter++;
                previous = -1;
            }
            if (assigned > std::numeric_limits<int>::max()) {
                plan.errors << tr("Numeración fuera de rango en %1.").arg(prefix);
                break;
            }
            const QString label = prefix + QLatin1Char(':') + QString::number(assigned) + suffix;
            if (label != record.label || !record.formula.isEmpty()) plan.changes << Change{index, label};
        }
    }
    checkConflicts(records, plan);
    return plan;
}

Plan shift(const QVector<Record> &records, const QVector<Range> &ranges)
{
    Plan plan;
    for (const auto &range : ranges) {
        if (range.first < 0 || range.last < range.first
                || (range.target == Target::Terminals && range.prefix.trimmed().isEmpty())
                || (range.target == Target::Cables && range.delimiter.isEmpty()))
            plan.errors << tr("Rango no válido: %1–%2.").arg(range.first).arg(range.last);
    }
    if (!plan.errors.isEmpty()) return plan;
    for (int i = 0; i < records.size(); ++i) {
        const auto &record = records[i];
        if (record.locked) continue;
        int matches = 0;
        QString proposed;
        for (const auto &range : ranges) {
            if ((range.target == Target::Terminals) != (record.kind == Kind::Terminal)) continue;
            QString pattern;
            if (range.target == Target::Terminals)
                pattern = QStringLiteral("^%1:([0-9]+)$").arg(QRegularExpression::escape(range.prefix.trimmed()));
            else if (range.target == Target::Wires) pattern = QStringLiteral("^([0-9]+)$");
            else pattern = QStringLiteral("^([0-9]+)(%1.*)$").arg(QRegularExpression::escape(range.delimiter));
            const auto match = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption).match(record.label);
            bool ok = false;
            const auto value = match.captured(1).toLongLong(&ok);
            if (!match.hasMatch() || !ok || value < range.first || value > range.last) continue;
            ++matches;
            const qlonglong next = value + static_cast<qlonglong>(range.offset);
            if (next < 0 || next > std::numeric_limits<int>::max()) {
                plan.errors << tr("Resultado fuera de rango: %1.").arg(record.label);
                continue;
            }
            proposed = QString::number(next);
            if (range.target == Target::Terminals) proposed.prepend(range.prefix.trimmed().toUpper() + QLatin1Char(':'));
            if (range.target == Target::Cables) proposed += match.captured(2);
        }
        if (matches > 1) plan.errors << tr("Rangos solapados para %1.").arg(record.label);
        else if (matches == 1 && !proposed.isEmpty() && proposed != record.label)
            plan.changes << Change{i, proposed};
    }
    checkConflicts(records, plan);
    return plan;
}
}
