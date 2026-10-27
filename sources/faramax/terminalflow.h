/* SPDX-License-Identifier: GPL-2.0-or-later
 * QET Faramax: native port of the user's QET_Terminal_Flow v2 rules.
 */
#ifndef FARAMAX_TERMINALFLOW_H
#define FARAMAX_TERMINALFLOW_H

#include <QMap>
#include <QSet>
#include <QStringList>
#include <QVector>

namespace Faramax::TerminalFlow
{
enum class Kind { Terminal, Conductor };
enum class Target { Terminals, Wires, Cables };

// Value-only input: planning never touches a project or writes files.
struct Record
{
    QString id, label, formula, scope, potential;
    Kind kind = Kind::Terminal;
    int folio = 0;
    double x = 0, y = 0;
    bool locked = false;
};

struct StripConfig
{
    QString prefix, pages = QStringLiteral("all");
    int start = 1;
    bool enabled = true, xFirst = true;
};

struct Range
{
    Target target = Target::Terminals;
    QString prefix, delimiter = QStringLiteral("-");
    int first = 1, last = 1, offset = 1;
};

struct Change
{
    int record = -1;
    QString label;
};

struct Plan
{
    QVector<Change> changes;
    QStringList errors, warnings;
    bool canApply() const { return errors.isEmpty() && !changes.isEmpty(); }
};

QString normalizeIdentifier(QString identifier);
QString stripPrefix(const QString &label, const QString &identifier);
bool parsePages(const QString &text, int folioCount, QSet<int> &pages, QString &error);
QStringList diagnose(const QVector<Record> &records, const QString &identifier);
Plan number(const QVector<Record> &records, int folioCount,
            const QVector<StripConfig> &configs, const QString &identifier,
            bool replaceExisting, bool groupSuffixes);
Plan shift(const QVector<Record> &records, const QVector<Range> &ranges);
}
#endif
