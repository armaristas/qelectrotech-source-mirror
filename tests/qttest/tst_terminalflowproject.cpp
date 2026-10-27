// SPDX-License-Identifier: GPL-2.0-or-later
#include "qetapptest.h"
#include "faramax/terminalflowproject.h"
#include "faramax/terminalflowwidget.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetproject.h"
#include "TerminalStrip/terminalstrip.h"
#include "ui/terminalnumberingdialog.h"
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QDir>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTabWidget>
#include <QSpinBox>
#include <QUndoStack>
#include <memory>
#include <vector>

using namespace Faramax::TerminalFlow;

class tst_terminalflowproject : public QObject
{
    Q_OBJECT
    QString fixture() { return QFINDTESTDATA("fixtures/every_feature.qet"); }
    void prepare(QETProject &project)
    {
        project.loadFolios();
        int number = 1;
        for (auto *folio : project.diagrams()) {
            for (auto *element : folio->elements()) {
                if (element->elementData().m_type != ElementData::Terminal) continue;
                auto info = element->elementInformations();
                info.addValue("label", QStringLiteral("X1:%1").arg(number++));
                info.addValue("formula", QString());
                info.addValue("auto_num_locked", QStringLiteral("false"));
                element->setElementInformations(info);
            }
        }
    }
    Plan proposal(const Snapshot &before)
    {
        StripConfig config; config.prefix = "X1"; config.start = 101;
        return number(before.records, before.folioCount, {config}, "X", true, false);
    }
private slots:
    void liveChangesUndoAndNativeStripNotifications()
    {
        QETProject project(fixture());
        QCOMPARE(project.state(), QETProject::Ok);
        prepare(project);
        const auto initial = snapshot(&project, false);
        QVERIFY(!initial.elements.isEmpty());
        auto *first = initial.elements.first().data();
        auto formulaInfo = first->elementInformations();
        formulaInfo.addValue("formula", QStringLiteral("X1:89"), false);
        formulaInfo.addValue("label", QStringLiteral("X1:89"), false);
        first->setElementInformations(formulaInfo);
        const auto before = snapshot(&project, false);
        QVERIFY(before.records.size() >= 2);
        auto plan = proposal(before);
        QVERIFY2(plan.canApply(), qPrintable(plan.errors.join('\n')));
        QString error;
        std::unique_ptr<QUndoCommand> macro(command(&project, before, plan, error));
        QVERIFY2(macro != nullptr, qPrintable(error));
        QCOMPARE(snapshot(&project, false).information, before.information); // preview changes nothing
        std::vector<std::unique_ptr<QSignalSpy>> stripSpies;
        for (auto *strip : project.terminalStrip())
            stripSpies.emplace_back(std::make_unique<QSignalSpy>(strip, &TerminalStrip::dataChanged));
        const int count = project.undoStack()->count();
        project.undoStack()->push(macro.release());
        QCOMPARE(project.undoStack()->count(), count + 1);
        const auto after = snapshot(&project, false);
        for (const auto &change : plan.changes) {
            const auto id = before.records[change.record].id;
            QCOMPARE(after.elements.value(id)->actualLabel(), change.label);
            QCOMPARE(after.information.value(id).value("formula").toString(), QString());
        }
        bool notified = false;
        for (const auto &spy : stripSpies) notified |= !spy->isEmpty();
        QVERIFY(notified);
        project.undoStack()->undo();
        QCOMPARE(snapshot(&project, false).information, before.information);
        project.undoStack()->redo();
        QCOMPARE(snapshot(&project, false).information, after.information);
    }

    void stalePreviewAndReadOnlyAreRefused()
    {
        QETProject project(fixture()); prepare(project);
        const auto before = snapshot(&project, false);
        QVERIFY(!before.records.isEmpty());
        const auto plan = proposal(before);
        auto *element = before.elements.first().data();
        element->setPos(element->pos() + QPointF(10, 0));
        QString error;
        std::unique_ptr<QUndoCommand> macro(command(&project, before, plan, error));
        QVERIFY(!macro); QVERIFY(!error.isEmpty());
        const auto current = snapshot(&project, false);
        project.setReadOnly(true);
        macro.reset(command(&project, current, proposal(current), error));
        QVERIFY(!macro); QVERIFY(!error.isEmpty());
        QCOMPARE(snapshot(&project, false).information, current.information);
    }

    void legacySharedUuidsAddressBothPlacements()
    {
        QETProject project(fixture()); prepare(project);
        auto initial = snapshot(&project, false);
        QVERIFY(initial.elements.size() >= 2);
        auto elements = initial.elements.values();
        elements[1]->setUuid(elements[0]->uuid());
        const auto before = snapshot(&project, false);
        QCOMPARE(before.elements.size(), before.records.size());
        const auto plan = proposal(before);
        QVERIFY2(plan.canApply(), qPrintable(plan.errors.join('\n')));
        QString error;
        std::unique_ptr<QUndoCommand> macro(command(&project, before, plan, error));
        QVERIFY2(macro != nullptr, qPrintable(error));
        project.undoStack()->push(macro.release());
        for (const auto &change : plan.changes)
            QCOMPARE(before.elements.value(before.records[change.record].id)->actualLabel(), change.label);
        project.undoStack()->undo();
        QCOMPARE(snapshot(&project, false).information, before.information);
    }

    void conductorRangesPreserveOtherPropertiesAndUndo()
    {
        QETProject project(fixture()); prepare(project);
        auto before = snapshot(&project, true);
        QVERIFY(!before.conductors.isEmpty());
        for (auto wire : before.conductors) {
            auto properties = wire->properties();
            properties.text = "42"; properties.m_formula.clear();
            properties.color = QColor("#123456");
            wire->setProperties(properties);
        }
        before = snapshot(&project, true);
        Range range; range.target = Target::Wires; range.first = range.last = 42; range.offset = 3;
        // Unrelated potentials given the same resulting number are a real
        // collision: choose one potential if the fixture has several.
        QVector<Record> one = before.records;
        QString potential;
        for (auto &record : one) {
            if (record.kind != Kind::Conductor) continue;
            if (potential.isEmpty()) potential = record.potential;
            if (record.potential != potential) record.locked = true;
        }
        auto selected = shift(one, {range});
        // Keep untouched potentials at 42; the changed one becomes 45.
        QVERIFY2(selected.canApply(), qPrintable(selected.errors.join('\n')));
        QString error;
        std::unique_ptr<QUndoCommand> macro(command(&project, before, selected, error));
        QVERIFY2(macro != nullptr, qPrintable(error));
        project.undoStack()->push(macro.release());
        for (const auto &change : selected.changes) {
            const auto id = before.records[change.record].id;
            auto expected = before.properties.value(id); expected.text = "45";
            QCOMPARE(before.conductors.value(id)->properties(), expected);
        }
        project.undoStack()->undo();
        QCOMPARE(snapshot(&project, true).properties, before.properties);
    }

    void labelsPersistThroughNormalProjectSerialization()
    {
        const auto path = fixture();
        QFile original(path);
        QVERIFY(original.open(QIODevice::ReadOnly));
        const auto originalBytes = original.readAll();
        original.close();
        QETProject project(path); prepare(project);
        const auto before = snapshot(&project, false);
        const auto plan = proposal(before);
        QString error;
        std::unique_ptr<QUndoCommand> macro(command(&project, before, plan, error));
        QVERIFY2(macro != nullptr, qPrintable(error));
        project.undoStack()->push(macro.release());
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), originalBytes); // apply did not write the .qet
        original.close();
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto savedPath = directory.filePath("renumbered.qet");
        QFile saved(savedPath);
        QVERIFY(saved.open(QIODevice::WriteOnly));
        const auto xml = project.toXml().toByteArray();
        QCOMPARE(saved.write(xml), xml.size()); saved.close();
        QETProject reopened(savedPath);
        QCOMPARE(reopened.state(), QETProject::Ok);
        QStringList expected, actual;
        for (const auto &record : snapshot(&project, false).records) expected << record.label;
        for (const auto &record : snapshot(&reopened, false).records) actual << record.label;
        expected.sort(); actual.sort();
        QCOMPARE(actual, expected);
    }

    void existingNumberingDialogContainsFlowAndDoesNotApplyClassicByEnter()
    {
        QETProject project(fixture()); prepare(project);
        TerminalNumberingDialog dialog(nullptr, &project);
        auto *tabs = dialog.findChild<QTabWidget *>("numberingTabs");
        QVERIFY(tabs); QCOMPARE(tabs->count(), 2);
        auto *flow = dialog.findChild<Faramax::TerminalFlowWidget *>();
        QVERIFY(flow);
        tabs->setCurrentWidget(flow);
        dialog.accept();
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        auto *apply = flow->findChild<QPushButton *>("terminalFlowApplyButton");
        QVERIFY(apply); QVERIFY(!apply->isEnabled());
        auto *preview = flow->findChild<QPushButton *>("terminalFlowPreviewButton");
        auto *strips = flow->findChild<QTableWidget *>("terminalFlowStrips");
        QVERIFY(preview); QVERIFY(strips); QVERIFY(strips->rowCount() > 0);
        // Existing numbered terminals are left alone by default.
        preview->click(); QVERIFY(!apply->isEnabled());
        auto *replace = flow->findChild<QCheckBox *>("terminalFlowReplace");
        QVERIFY(replace); replace->setChecked(true);
        auto *start = qobject_cast<QSpinBox *>(strips->cellWidget(0, 4));
        QVERIFY(start); start->setValue(101);
        const auto before = snapshot(&project, false);
        preview->click(); QVERIFY(apply->isEnabled());
        QCOMPARE(snapshot(&project, false).information, before.information);
        dialog.show();
        QCoreApplication::processEvents();
        const auto previewDirectory = qEnvironmentVariable("QET_FARAMAX_PREVIEW_DIR");
        if (!previewDirectory.isEmpty()) {
            QDir().mkpath(previewDirectory);
            QVERIFY(dialog.grab().save(QDir(previewDirectory).filePath("terminal-flow.png")));
        }
        const int index = project.undoStack()->index();
        apply->click();
        QCOMPARE(project.undoStack()->index(), index + 1);
        QVERIFY(!apply->isEnabled());
        project.undoStack()->undo();
        QCOMPARE(snapshot(&project, false).information, before.information);
    }
};
QET_APP_TEST_MAIN(tst_terminalflowproject)
#include "tst_terminalflowproject.moc"
