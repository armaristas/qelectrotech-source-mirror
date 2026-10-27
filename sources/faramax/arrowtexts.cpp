// SPDX-License-Identifier: GPL-2.0-or-later
#include "arrowtexts.h"
#include "../diagram.h"
#include "../qetproject.h"
#include "../qetgraphicsitem/element.h"
#include "../qetgraphicsitem/terminal.h"
#include "../qetgraphicsitem/conductor.h"
#include "../qetgraphicsitem/dynamicelementtextitem.h"
#include "../QPropertyUndoCommand/qpropertyundocommand.h"
#include "../undocommand/addelementtextcommand.h"
#include "../undocommand/changeelementinformationcommand.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTimer>
#include <QUndoStack>
#include <QVBoxLayout>
#include <memory>

namespace Faramax::ArrowTexts
{
namespace
{
QString tr(const char *text) { return QCoreApplication::translate("ArrowTexts", text); }
constexpr const char *choiceKey = "faramax_arrow_choice";
constexpr const char *choiceIdKey = "faramax_arrow_choice_id";
struct Resolution
{
    QSet<QString> labels;
    QMap<QString, QString> targets;
    QSet<Element *> elements;
    QSet<Terminal *> terminals;
};

QStringList endpointLabels(Terminal *terminal, Resolution &result)
{
    auto *element = terminal->parentElement();
    if (!element) return {};
    auto label = element->actualLabel().trimmed();
    if (element->location().fileName() == QLatin1String("qfqfawe.elmt") && element->diagram()) {
        // The reference tool overlays this empty connection symbol on a
        // terminal. Read the real terminal in memory instead of writing
        // redundant element1_label/element2_label XML attributes.
        QStringList labels;
        for (auto *other : element->diagram()->elements()) {
            if (other->elementData().m_type == ElementData::Terminal
                    && QLineF(other->scenePos(), element->scenePos()).length() < 0.01) {
                result.elements.insert(other);
                if (!other->actualLabel().trimmed().isEmpty()) labels << other->actualLabel().trimmed();
            }
        }
        labels.removeDuplicates();
        if (!labels.isEmpty()) return labels;
    }
    if (label.isEmpty()) return {};
    if (!label.contains(QLatin1Char(':')) && !terminal->name().trimmed().isEmpty())
        label += QLatin1Char(':') + terminal->name().trimmed();
    return {label};
}

Resolution resolve(Element *source)
{
    Resolution result;
    if (!source || !(source->linkType() & Element::AllReport)) return result;
    result.elements.insert(source);
    for (auto *terminal : source->terminals()) result.terminals.insert(terminal);
    QList<Element *> reports = source->linkedElements();
    QSet<Element *> visited{source};
    while (!reports.isEmpty()) {
        auto *report = reports.takeFirst();
        if (!report || visited.contains(report) || !(report->linkType() & Element::AllReport)) continue;
        visited.insert(report); result.elements.insert(report);
        for (auto *terminal : report->terminals()) {
            result.terminals.insert(terminal);
            for (auto *wire : terminal->conductors()) {
                auto *peer = wire->terminal1 == terminal ? wire->terminal2 : wire->terminal1;
                if (!peer || !peer->parentElement()) continue;
                result.terminals.insert(peer);
                auto *element = peer->parentElement(); result.elements.insert(element);
                if (element->linkType() & Element::AllReport) {
                    // A wire between two reports continues through the next
                    // report's linked partner; do not walk back to the source.
                    if (!visited.contains(element)) {
                        reports << element;
                        reports << element->linkedElements();
                    }
                } else {
                    for (const auto &label : endpointLabels(peer, result)) {
                        result.labels.insert(label);
                        const auto id = element->uuid().toString() + QLatin1Char('/') + peer->stableUuid().toString();
                        result.targets.insert(id, label);
                    }
                }
            }
        }
    }
    return result;
}

bool usesTarget(DynamicElementTextItem *text)
{
    return text && text->parentElement() && (text->parentElement()->linkType() & Element::AllReport)
        && text->textFrom() == DynamicElementTextItem::CompositeText && text->compositeText().contains(token());
}

class Watcher final : public QObject
{
public:
    explicit Watcher(DynamicElementTextItem *text) : QObject(text), m_text(text)
    {
        setObjectName(QStringLiteral("faramaxArrowWatcher"));
        m_timer.setSingleShot(true);
        connect(&m_timer, &QTimer::timeout, this, [this]() { refresh(); });
        connect(text, &DynamicElementTextItem::compositeTextChanged, this, [this]() { request(); });
        connect(text, &DynamicElementTextItem::textFromChanged, this, [this]() { request(); });
        request();
    }
    void request() { if (!m_timer.isActive()) m_timer.start(0); }
private:
    void refresh()
    {
        for (const auto &connection : m_connections) disconnect(connection);
        m_connections.clear();
        if (!usesTarget(m_text)) return;
        const auto resolution = resolve(m_text->parentElement());
        for (auto *element : resolution.elements) {
            m_connections << connect(element, &Element::elementInfoChange, this, [this]() { request(); });
            m_connections << connect(element, &Element::linkedElementChanged, this, [this]() { request(); });
            m_connections << connect(element, &QObject::destroyed, this, [this]() { request(); });
        }
        for (auto *terminal : resolution.terminals) {
            m_connections << connect(terminal, &Terminal::conductorWasAdded, this, [this]() { request(); });
            m_connections << connect(terminal, &Terminal::conductorWasRemoved, this, [this]() { request(); });
        }
        if (auto *diagram = m_text->parentElement()->diagram()) {
            if (auto *project = diagram->project()) {
                m_connections << connect(project->undoStack(), &QUndoStack::indexChanged, this, [this]() { request(); });
                m_connections << connect(project, &QETProject::diagramAdded, this, [this]() { request(); });
            }
        }
        m_text->refreshReportText();
    }
    QPointer<DynamicElementTextItem> m_text;
    QTimer m_timer;
    QVector<QMetaObject::Connection> m_connections;
};

DynamicElementTextItem *availableField(Element *report, bool replaceManual)
{
    for (auto *text : report->dynamicTextItems()) if (usesTarget(text)) return text;
    for (auto *text : report->dynamicTextItems())
        if (text->textFrom() == DynamicElementTextItem::UserText
                && (replaceManual || text->text().trimmed().isEmpty())) return text;
    return nullptr;
}

DynamicElementTextItem *newField(Element *report)
{
    auto *text = new DynamicElementTextItem(report);
    text->setTextFrom(DynamicElementTextItem::CompositeText);
    text->setCompositeText(token());
    text->setPos(report->drawingRect().left(), report->drawingRect().bottom() + 5);
    return text;
}
}

QString token() { return QStringLiteral("%{faramax_target}"); }

QStringList candidates(Element *report)
{
    auto values = resolve(report).labels.values();
    values.sort(Qt::CaseInsensitive);
    return values;
}

QString resolved(Element *report)
{
    const auto resolution = resolve(report);
    const auto values = resolution.labels.values();
    if (values.size() == 1) return values.first();
    if (values.isEmpty()) return QString();
    const auto info = report->elementInformations();
    const auto id = info.value(QString::fromLatin1(choiceIdKey)).toString();
    if (!id.isEmpty()) return resolution.targets.value(id, QStringLiteral("?"));
    const auto choice = info.value(QString::fromLatin1(choiceKey)).toString();
    return values.contains(choice) ? choice : QStringLiteral("?");
}

void watch(DynamicElementTextItem *text)
{
    if (!usesTarget(text)) return;
    auto *existing = text->findChild<QObject *>(QStringLiteral("faramaxArrowWatcher"), Qt::FindDirectChildrenOnly);
    if (existing) static_cast<Watcher *>(existing)->request();
    else new Watcher(text);
}

void prepareNewReport(Element *report)
{
    if (!report || !(report->linkType() & Element::AllReport) || !report->diagram()) return;
    auto *project = report->diagram()->project();
    if (!project || project->isReadOnly() || !project->property(autoProperty).toBool()) return;
    for (auto *text : report->dynamicTextItems()) if (usesTarget(text)) { watch(text); return; }
    auto *text = availableField(report, false);
    if (!text) { text = newField(report); report->addDynamicTextItem(text); }
    else {
        text->setCompositeText(token());
        text->setTextFrom(DynamicElementTextItem::CompositeText);
    }
    watch(text);
}

QUndoCommand *command(QETProject *project, const QVector<Selection> &selection, bool automatic)
{
    if (!project || project->isReadOnly()) return nullptr;
    for (const auto &item : selection)
        if (!item.report || !item.report->diagram() || item.report->diagram()->project() != project
                || item.report->diagram()->isReadOnly()) return nullptr;
    auto macro = std::make_unique<QUndoCommand>(tr("GNerador Flechas: textos de destino"));
    if (project->property(autoProperty).toBool() != automatic)
        new QPropertyUndoCommand(project, autoProperty, project->property(autoProperty).toBool(), automatic, macro.get());
    QSet<Element *> processed;
    for (const auto &item : selection) {
        auto *report = item.report.data();
        if (processed.contains(report) || !(report->linkType() & Element::AllReport)) continue;
        processed.insert(report);
        auto *field = availableField(report, item.replaceManual);
        if (!field) {
            field = newField(report);
            // Keep it outside the scene until the native add-text command runs.
            field->setParentItem(nullptr);
            if (field->scene()) field->scene()->removeItem(field);
            new AddElementTextCommand(report, field, macro.get());
        } else if (!usesTarget(field)) {
            new QPropertyUndoCommand(field, "compositeText", field->compositeText(), token(), macro.get());
            new QPropertyUndoCommand(field, "textFrom", QVariant::fromValue(field->textFrom()),
                QVariant::fromValue(DynamicElementTextItem::CompositeText), macro.get());
        }
        auto oldInfo = report->elementInformations(); auto newInfo = oldInfo;
        const auto values = candidates(report);
        const auto choice = values.contains(item.choice) ? item.choice : QString();
        if (choice.isEmpty()) {
            newInfo.remove(QString::fromLatin1(choiceKey));
            newInfo.remove(QString::fromLatin1(choiceIdKey));
        } else {
            newInfo.addValue(QString::fromLatin1(choiceKey), choice, false);
            const auto resolution = resolve(report);
            for (auto target = resolution.targets.cbegin(); target != resolution.targets.cend(); ++target)
                if (target.value() == choice) {
                    newInfo.addValue(QString::fromLatin1(choiceIdKey), target.key(), false);
                    break;
                }
        }
        if (oldInfo != newInfo) new ChangeElementInformationCommand(report, oldInfo, newInfo, macro.get());
    }
    return macro->childCount() ? macro.release() : nullptr;
}

void openDialog(QETProject *project, QWidget *parent)
{
    if (!project || project->isReadOnly()) return;
    project->loadFolios();
    QDialog dialog(parent);
    dialog.setWindowTitle(tr("GNerador Flechas — destinos de conexión")); dialog.resize(900, 570);
    auto *layout = new QVBoxLayout(&dialog);
    auto *description = new QLabel(tr("Las flechas mostrarán el dispositivo y borne del otro extremo. "
        "El campo se actualiza al conectar, desconectar o cambiar etiquetas. Selecciona el destino cuando haya varias opciones; "
        "un signo ? indica una ambigüedad pendiente."), &dialog);
    description->setWordWrap(true); layout->addWidget(description);
    auto *table = new QTableWidget(0, 4, &dialog);
    table->setObjectName(QStringLiteral("faramaxArrowTable"));
    table->setHorizontalHeaderLabels({tr("Usar"), tr("Folio / flecha"), tr("Destino"), tr("Estado")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);
    QVector<QPointer<Element>> reports;
    for (auto *diagram : project->diagrams()) for (auto *element : diagram->elements()) {
        if (!(element->linkType() & Element::AllReport)) continue;
        const int row = table->rowCount(); table->insertRow(row); reports << element;
        auto *enabled = new QTableWidgetItem;
        enabled->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        enabled->setCheckState(Qt::Checked); table->setItem(row, 0, enabled);
        auto *identity = new QTableWidgetItem(QStringLiteral("%1 / %2 (%3, %4)").arg(diagram->folioIndex() + 1)
            .arg(element->actualLabel()).arg(element->pos().x()).arg(element->pos().y()));
        identity->setFlags(identity->flags() & ~Qt::ItemIsEditable); table->setItem(row, 1, identity);
        const auto values = candidates(element);
        auto *choice = new QComboBox(table); choice->addItem(tr("Sin selección"), QString());
        for (const auto &value : values) choice->addItem(value, value);
        const auto current = resolved(element);
        if (values.size() == 1) choice->setCurrentIndex(1);
        else if (values.contains(current)) choice->setCurrentIndex(values.indexOf(current) + 1);
        table->setCellWidget(row, 2, choice);
        QString state = values.isEmpty() ? tr("Sin destino conectado")
            : values.size() == 1 ? tr("Destino único") : tr("Varios destinos: elige uno");
        for (auto *text : element->dynamicTextItems())
            if (text->textFrom() == DynamicElementTextItem::UserText && !text->text().trimmed().isEmpty()) {
                state += tr("; texto manual conservado"); break;
            }
        auto *status = new QTableWidgetItem(state); status->setFlags(status->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, 3, status);
    }
    layout->addWidget(table);
    auto *automatic = new QCheckBox(tr("Añadir automáticamente este campo a nuevas flechas del proyecto"), &dialog);
    automatic->setChecked(project->property(autoProperty).toBool()); layout->addWidget(automatic);
    auto *replace = new QCheckBox(tr("Sustituir el primer texto manual de las flechas seleccionadas"), &dialog);
    layout->addWidget(replace);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, &dialog, [&]() {
        QVector<Selection> selection;
        for (int row = 0; row < reports.size(); ++row) {
            if (table->item(row, 0)->checkState() != Qt::Checked) continue;
            const auto choice = qobject_cast<QComboBox *>(table->cellWidget(row, 2))->currentData().toString();
            selection << Selection{reports[row], choice, replace->isChecked()};
        }
        if (auto *macro = command(project, selection, automatic->isChecked())) {
            project->undoStack()->push(macro);
            dialog.accept();
        }
    });
    dialog.exec();
}
}
