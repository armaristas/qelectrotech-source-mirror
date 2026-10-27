// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef FARAMAX_GNERADORTB_H
#define FARAMAX_GNERADORTB_H
#include "../properties/elementdata.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QRectF>
#include <QSharedPointer>
class TerminalStrip;
class RealTerminal;
class PhysicalTerminal;
class QETProject;
class QPainter;
class QWidget;
class QUndoCommand;

namespace Faramax::TB
{
inline constexpr const char *settingsProperty = "faramaxTBSettings";
struct Row
{
    QString id;
    QSharedPointer<RealTerminal> terminal;
    QSharedPointer<PhysicalTerminal> physical;
    ElementData before;
    QJsonObject values;
    QJsonObject edits;
};
QJsonObject defaults();
QJsonObject settings(TerminalStrip *strip);
QString encode(const QJsonObject &config);
QString key(const QString &field);
QString nativeType(const QString &name);
QString displayType(const QString &name);
QVector<Row> rows(TerminalStrip *strip, const QJsonObject &config = {});
QStringList validate(const QVector<Row> &rows, const QJsonObject &config);
QString classify(const QString &device, bool simple, bool ground, const QJsonObject &config);
QUndoCommand *editCommand(TerminalStrip *strip, const QVector<Row> &edited, const QJsonObject &config,
                          QString &error);
QUndoCommand *orderCommand(TerminalStrip *strip, const QVector<Row> &order);
QUndoCommand *removeCommand(TerminalStrip *strip, const QVector<Row> &selection);
QUndoCommand *expandCommand(TerminalStrip *strip, const Row &row, const QStringList &levels, QString &error);
void watch(QETProject *project);
bool enabled(TerminalStrip *strip);
struct Reference
{
    QRectF rect;
    QSharedPointer<RealTerminal> terminal;
};
QVector<Reference> references(TerminalStrip *strip);
QRectF bounds(TerminalStrip *strip);
void invalidateDrawing(TerminalStrip *strip);
void paint(QPainter *painter, TerminalStrip *strip);
QRectF render(QPainter *painter, TerminalStrip *strip, const QVector<Row> &rows, const QJsonObject &config);
bool writeWorkbook(const QString &path, const QVector<Row> &rows, const QJsonObject &config,
                   const QString &project, QString &error);
bool readWorkbook(const QString &path, QJsonArray &rows, QJsonObject &config, QString &error);
void openEditor(QETProject *project, TerminalStrip *strip, QWidget *parent);
} // namespace Faramax::TB
#endif
