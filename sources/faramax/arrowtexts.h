// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef FARAMAX_ARROWTEXTS_H
#define FARAMAX_ARROWTEXTS_H
#include <QStringList>
#include <QVector>
#include <QPointer>

class Element;
class DynamicElementTextItem;
class QETProject;
class QUndoCommand;
class QWidget;

namespace Faramax::ArrowTexts
{
inline constexpr const char *autoProperty = "faramaxAutoArrowText";
QString token();
QStringList candidates(Element *report);
QString resolved(Element *report);
void watch(DynamicElementTextItem *text);
// Called when a report joins a scene. The new field belongs to the symbol
// being inserted, so its creation is undone with that insertion.
void prepareNewReport(Element *report);
struct Selection { QPointer<Element> report; QString choice; bool replaceManual = false; };
QUndoCommand *command(QETProject *project, const QVector<Selection> &selection, bool automatic);
void openDialog(QETProject *project, QWidget *parent);
}
#endif
