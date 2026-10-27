/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "terminalflowwidget.h"

#include "../qetproject.h"
#include <QCheckBox>
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QUndoStack>
#include <QVBoxLayout>

using namespace Faramax::TerminalFlow;

namespace
{
QTableWidget *table(const QStringList &headers, QWidget *parent)
{
    auto *result = new QTableWidget(0, headers.size(), parent);
    result->setHorizontalHeaderLabels(headers);
    result->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    result->horizontalHeader()->setStretchLastSection(true);
    result->setSelectionBehavior(QAbstractItemView::SelectRows);
    return result;
}
QTableWidgetItem *fixedItem(const QString &text)
{
    auto *result = new QTableWidgetItem(text);
    result->setFlags(result->flags() & ~Qt::ItemIsEditable);
    return result;
}
}

namespace Faramax
{
TerminalFlowWidget::TerminalFlowWidget(QETProject *project, QWidget *parent)
    : QWidget(parent), m_project(project)
{
    setObjectName(QStringLiteral("terminalFlowWidget"));
    auto *layout = new QVBoxLayout(this);
    auto *description = new QLabel(tr("Trabaja sobre el proyecto abierto. Revisa la propuesta antes de aplicarla; "
        "todos los cambios se deshacen juntos. Las bornas bloqueadas se conservan."), this);
    description->setWordWrap(true);
    layout->addWidget(description);
    auto *toolbar = new QHBoxLayout;
    toolbar->addWidget(new QLabel(tr("Identificador de bornas:"), this));
    m_identifier = new QLineEdit(QStringLiteral("X"), this);
    m_identifier->setObjectName(QStringLiteral("terminalFlowIdentifier"));
    m_identifier->setMaximumWidth(100);
    m_identifier->setToolTip(tr("Por ejemplo X o -X. Pulsa Analizar para actualizar los borneros."));
    toolbar->addWidget(m_identifier);
    auto *scanButton = new QPushButton(tr("Analizar proyecto"), this);
    scanButton->setAutoDefault(false);
    toolbar->addWidget(scanButton);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    m_modes = new QTabWidget(this);
    auto *automatic = new QWidget(m_modes);
    auto *automaticLayout = new QVBoxLayout(automatic);
    m_strips = table({tr("Usar"), tr("Bornero"), tr("Folios (1, 3-5 o all)"), tr("Orden"), tr("Inicio")}, automatic);
    m_strips->setObjectName(QStringLiteral("terminalFlowStrips"));
    automaticLayout->addWidget(m_strips);
    m_replace = new QCheckBox(tr("Sustituir numeraciones existentes"), automatic);
    m_replace->setObjectName(QStringLiteral("terminalFlowReplace"));
    m_suffixes = new QCheckBox(tr("Agrupar multipunto: A inicia grupo; B/C… comparten número"), automatic);
    automaticLayout->addWidget(m_replace);
    automaticLayout->addWidget(m_suffixes);
    m_modes->addTab(automatic, tr("Numeración por bornero"));

    auto *ranges = new QWidget(m_modes);
    auto *rangesLayout = new QVBoxLayout(ranges);
    auto *rangeDescription = new QLabel(tr("Cables: números simples. Mangueras: número anterior al delimitador "
        "(12-3 → 14-3 con incremento 2). Los rangos se evalúan sobre los valores originales."), ranges);
    rangeDescription->setWordWrap(true);
    rangesLayout->addWidget(rangeDescription);
    m_ranges = table({tr("Destino"), tr("Bornero / delimitador"), tr("Desde"), tr("Hasta"), tr("Incremento")}, ranges);
    m_ranges->setObjectName(QStringLiteral("terminalFlowRanges"));
    rangesLayout->addWidget(m_ranges);
    auto *rangeButtons = new QHBoxLayout;
    auto *add = new QPushButton(tr("Añadir rango"), ranges);
    auto *remove = new QPushButton(tr("Quitar rango seleccionado"), ranges);
    add->setAutoDefault(false); remove->setAutoDefault(false);
    rangeButtons->addWidget(add); rangeButtons->addWidget(remove); rangeButtons->addStretch();
    rangesLayout->addLayout(rangeButtons);
    m_modes->addTab(ranges, tr("Modificar rangos"));
    layout->addWidget(m_modes, 2);

    auto *splitter = new QSplitter(Qt::Vertical, this);
    m_preview = table({tr("Folio"), tr("Destino"), tr("Actual"), tr("Propuesto"), tr("Fórmula")}, splitter);
    m_preview->setObjectName(QStringLiteral("terminalFlowPreview"));
    m_preview->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_diagnostics = new QTextEdit(splitter);
    m_diagnostics->setObjectName(QStringLiteral("terminalFlowDiagnostics"));
    m_diagnostics->setReadOnly(true);
    splitter->addWidget(m_preview); splitter->addWidget(m_diagnostics);
    splitter->setStretchFactor(0, 3); splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 3);
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    auto *buttons = new QHBoxLayout;
    auto *previewButton = new QPushButton(tr("Vista previa"), this);
    previewButton->setObjectName(QStringLiteral("terminalFlowPreviewButton"));
    previewButton->setAutoDefault(false);
    m_apply = new QPushButton(tr("Aplicar al proyecto"), this);
    m_apply->setObjectName(QStringLiteral("terminalFlowApplyButton"));
    m_apply->setAutoDefault(false);
    buttons->addStretch(); buttons->addWidget(previewButton); buttons->addWidget(m_apply);
    layout->addLayout(buttons);
    connect(scanButton, &QPushButton::clicked, this, &TerminalFlowWidget::scan);
    connect(previewButton, &QPushButton::clicked, this, &TerminalFlowWidget::preview);
    connect(m_apply, &QPushButton::clicked, this, &TerminalFlowWidget::apply);
    connect(add, &QPushButton::clicked, this, &TerminalFlowWidget::addRange);
    connect(remove, &QPushButton::clicked, this, [this]() {
        if (m_ranges->currentRow() >= 0) m_ranges->removeRow(m_ranges->currentRow());
        invalidate();
    });
    connect(m_identifier, &QLineEdit::textChanged, this, &TerminalFlowWidget::invalidate);
    connect(m_replace, &QCheckBox::toggled, this, &TerminalFlowWidget::invalidate);
    connect(m_suffixes, &QCheckBox::toggled, this, &TerminalFlowWidget::invalidate);
    connect(m_strips, &QTableWidget::itemChanged, this, &TerminalFlowWidget::invalidate);
    connect(m_ranges, &QTableWidget::itemChanged, this, &TerminalFlowWidget::invalidate);
    connect(m_modes, &QTabWidget::currentChanged, this, &TerminalFlowWidget::invalidate);
    if (project) connect(project->undoStack(), &QUndoStack::indexChanged, this, &TerminalFlowWidget::invalidate);
    scan();
}

void TerminalFlowWidget::invalidate()
{
    m_plan = {};
    m_apply->setEnabled(false);
    m_preview->setRowCount(0);
    m_status->setText(tr("Genera una vista previa para revisar los cambios."));
}

void TerminalFlowWidget::scan()
{
    invalidate();
    m_snapshot = snapshot(m_project, false);
    const auto identifier = normalizeIdentifier(m_identifier->text());
    QSet<QString> prefixes;
    int locked = 0;
    for (const auto &record : m_snapshot.records) {
        const auto prefix = stripPrefix(record.label, identifier);
        if (!prefix.isEmpty()) prefixes.insert(prefix);
        if (record.locked) ++locked;
    }
    auto sorted = prefixes.values(); sorted.sort(Qt::CaseInsensitive);
    m_strips->setRowCount(0);
    for (const auto &prefix : sorted) {
        const int row = m_strips->rowCount(); m_strips->insertRow(row);
        auto *enabled = fixedItem(QString());
        enabled->setFlags(enabled->flags() | Qt::ItemIsUserCheckable);
        enabled->setCheckState(Qt::Checked);
        m_strips->setItem(row, 0, enabled);
        m_strips->setItem(row, 1, fixedItem(prefix));
        m_strips->setItem(row, 2, new QTableWidgetItem(QStringLiteral("all")));
        auto *axis = new QComboBox(m_strips); axis->addItems({QStringLiteral("X"), QStringLiteral("Y")});
        m_strips->setCellWidget(row, 3, axis);
        connect(axis, &QComboBox::currentIndexChanged, this, &TerminalFlowWidget::invalidate);
        auto *start = new QSpinBox(m_strips); start->setRange(0, 1000000000); start->setValue(1);
        m_strips->setCellWidget(row, 4, start);
        connect(start, &QSpinBox::valueChanged, this, &TerminalFlowWidget::invalidate);
    }
    m_diagnostics->setPlainText(diagnose(m_snapshot.records, identifier).join(QLatin1Char('\n')));
    m_status->setText(tr("%1 bornas, %2 borneros, %3 bloqueadas. Los folios se cuentan desde 1.")
        .arg(m_snapshot.records.size()).arg(prefixes.size()).arg(locked));
}

void TerminalFlowWidget::addRange()
{
    const int row = m_ranges->rowCount(); m_ranges->insertRow(row);
    auto *target = new QComboBox(m_ranges);
    target->addItems({tr("Bornas"), tr("Cables"), tr("Mangueras")});
    m_ranges->setCellWidget(row, 0, target);
    m_ranges->setItem(row, 1, new QTableWidgetItem(QStringLiteral("X1")));
    for (int col = 2; col <= 4; ++col) {
        auto *spin = new QSpinBox(m_ranges);
        spin->setRange(col == 4 ? -1000000000 : 0, 1000000000); spin->setValue(1);
        m_ranges->setCellWidget(row, col, spin);
        connect(spin, &QSpinBox::valueChanged, this, &TerminalFlowWidget::invalidate);
    }
    connect(target, &QComboBox::currentIndexChanged, this, [this, target]() {
        // Find its current row: deleting earlier rows changes row indices.
        for (int r = 0; r < m_ranges->rowCount(); ++r) {
            if (m_ranges->cellWidget(r, 0) == target) {
                if (target->currentIndex() == 2) m_ranges->item(r, 1)->setText(QStringLiteral("-"));
                break;
            }
        }
        invalidate();
    });
    invalidate();
}

void TerminalFlowWidget::preview()
{
    invalidate();
    if (!m_project || m_project->isReadOnly()) return;
    const bool ranges = m_modes->currentIndex() == 1;
    m_snapshot = snapshot(m_project, ranges);
    if (!ranges) {
        QVector<StripConfig> configs;
        for (int row = 0; row < m_strips->rowCount(); ++row) {
            StripConfig config;
            config.enabled = m_strips->item(row, 0)->checkState() == Qt::Checked;
            config.prefix = m_strips->item(row, 1)->text();
            config.pages = m_strips->item(row, 2)->text();
            config.xFirst = qobject_cast<QComboBox *>(m_strips->cellWidget(row, 3))->currentIndex() == 0;
            config.start = qobject_cast<QSpinBox *>(m_strips->cellWidget(row, 4))->value();
            configs << config;
        }
        m_plan = number(m_snapshot.records, m_snapshot.folioCount, configs,
            m_identifier->text(), m_replace->isChecked(), m_suffixes->isChecked());
    } else {
        QVector<Range> rules;
        for (int row = 0; row < m_ranges->rowCount(); ++row) {
            Range rule;
            rule.target = static_cast<Target>(qobject_cast<QComboBox *>(m_ranges->cellWidget(row, 0))->currentIndex());
            rule.prefix = rule.delimiter = m_ranges->item(row, 1)->text();
            rule.first = qobject_cast<QSpinBox *>(m_ranges->cellWidget(row, 2))->value();
            rule.last = qobject_cast<QSpinBox *>(m_ranges->cellWidget(row, 3))->value();
            rule.offset = qobject_cast<QSpinBox *>(m_ranges->cellWidget(row, 4))->value();
            rules << rule;
        }
        m_plan = shift(m_snapshot.records, rules);
    }
    // Validate the native potential and project constraints during preview too.
    if (m_plan.canApply()) {
        QString error;
        auto *unapplied = command(m_project, m_snapshot, m_plan, error);
        delete unapplied;
        if (!error.isEmpty()) m_plan.errors << error;
    }
    showPlan();
}

void TerminalFlowWidget::showPlan()
{
    m_preview->setRowCount(m_plan.changes.size());
    for (int row = 0; row < m_plan.changes.size(); ++row) {
        const auto &change = m_plan.changes[row];
        const auto &record = m_snapshot.records[change.record];
        m_preview->setItem(row, 0, fixedItem(QString::number(record.folio + 1)));
        m_preview->setItem(row, 1, fixedItem(record.kind == Kind::Terminal ? tr("Borna") : tr("Conductor")));
        m_preview->setItem(row, 2, fixedItem(record.label));
        m_preview->setItem(row, 3, fixedItem(change.label));
        m_preview->setItem(row, 4, fixedItem(record.formula.isEmpty() ? QString() : tr("Se elimina: %1").arg(record.formula)));
    }
    QStringList messages = diagnose(m_snapshot.records, m_identifier->text());
    messages << m_plan.warnings << m_plan.errors;
    m_diagnostics->setPlainText(messages.join(QLatin1Char('\n')));
    m_status->setText(tr("%1 cambios propuestos. %2 errores.").arg(m_plan.changes.size()).arg(m_plan.errors.size()));
    m_apply->setEnabled(m_plan.canApply());
}

void TerminalFlowWidget::apply()
{
    QString error;
    auto *macro = command(m_project, m_snapshot, m_plan, error);
    if (!macro) {
        invalidate();
        m_status->setText(error);
        return;
    }
    const int count = m_plan.changes.size();
    m_project->undoStack()->push(macro);
    invalidate();
    m_status->setText(tr("%1 cambios aplicados al proyecto abierto. Ctrl+Z deshace toda la operación.").arg(count));
}
}
