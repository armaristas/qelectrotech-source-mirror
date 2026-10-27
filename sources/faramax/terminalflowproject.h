/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FARAMAX_TERMINALFLOWPROJECT_H
#define FARAMAX_TERMINALFLOWPROJECT_H

#include "terminalflow.h"
#include "../conductorproperties.h"
#include "../diagramcontext.h"
#include <QPointer>

class QETProject;
class Element;
class Conductor;
class QUndoCommand;

namespace Faramax::TerminalFlow
{
struct Snapshot
{
    QVector<Record> records;
    QMap<QString, QPointer<Element>> elements;
    QMap<QString, QPointer<Conductor>> conductors;
    QMap<QString, DiagramContext> information;
    QMap<QString, ConductorProperties> properties;
    int folioCount = 0;
};

Snapshot snapshot(QETProject *project, bool includeConductors);
// Returns an unapplied macro, or nullptr with an error. All validation happens
// before any child command is made; applying it is the caller's responsibility.
QUndoCommand *command(QETProject *project, const Snapshot &before, const Plan &plan, QString &error);
}
#endif
