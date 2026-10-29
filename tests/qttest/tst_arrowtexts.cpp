// SPDX-License-Identifier: GPL-2.0-or-later
#include "qetapptest.h"
#include "faramax/arrowtexts.h"
#include "factory/elementfactory.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/dynamicelementtextitem.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"
#include "qetproject.h"
#include <QUndoStack>
#include <memory>

using namespace Faramax::ArrowTexts;

class tst_arrowtexts : public QObject
{
    Q_OBJECT
    struct Circuit { Element *report = nullptr; Terminal *remote = nullptr; Element *target = nullptr; Conductor *wire = nullptr; };
    static void label(Element *element, const QString &value)
    {
        auto info = element->elementInformations();
        info.addValue("label", value); info.addValue("formula", QString());
        element->setElementInformations(info);
    }
    Circuit circuit(QETProject &project)
    {
        project.loadFolios();
        for (auto *diagram : project.diagrams()) for (auto *report : diagram->elements()) {
            if (report->linkType() != Element::NextReport || report->linkedElements().isEmpty()) continue;
            auto *partner = report->linkedElements().first();
            if (partner->terminals().size() != 1) continue;
            auto *terminal = partner->terminals().first();
            if (terminal->conductors().size() != 1) continue;
            auto *wire = terminal->conductors().first();
            auto *peer = wire->terminal1 == terminal ? wire->terminal2 : wire->terminal1;
            auto *target = peer->parentElement();
            if (!target || (target->linkType() & Element::AllReport)) continue;
            label(target, "X9:42");
            return {report, terminal, target, wire};
        }
        return {};
    }
    QString fixture() { return QFINDTESTDATA("../../examples/iso_sfc_example.qet"); }
    static DynamicElementTextItem *field(Element *report)
    {
        for (auto *text : report->dynamicTextItems())
            if (text->textFrom() == DynamicElementTextItem::CompositeText && text->compositeText().contains(token())) return text;
        return nullptr;
    }
private slots:
    void liveRenameDisconnectReconnectAndUndo()
    {
        QETProject project(fixture()); QCOMPARE(project.state(), QETProject::Ok);
        const auto c = circuit(project); QVERIFY(c.report);
        auto *manual = new DynamicElementTextItem(c.report); c.report->addDynamicTextItem(manual);
        manual->setText("Texto manual");
        QCOMPARE(candidates(c.report), QStringList{"X9:42"});
        std::unique_ptr<QUndoCommand> macro(command(&project, {{c.report, {}, false}}, false));
        QVERIFY(macro); project.undoStack()->push(macro.release());
        auto *text = field(c.report); QVERIFY(text);
        QTRY_COMPARE(text->toPlainText(), QString("X9:42"));
        QCOMPARE(manual->text(), QString("Texto manual"));
        label(c.target, "X9:43");
        QTRY_COMPARE(text->toPlainText(), QString("X9:43"));
        c.remote->removeConductor(c.wire);
        QTRY_COMPARE(text->toPlainText(), QString());
        QVERIFY(c.remote->addConductor(c.wire));
        QTRY_COMPARE(text->toPlainText(), QString("X9:43"));
        project.undoStack()->undo();
        QVERIFY(!field(c.report));
        QCOMPARE(manual->text(), QString("Texto manual"));
        project.undoStack()->redo();
        QVERIFY(field(c.report));
        QTRY_COMPARE(field(c.report)->toPlainText(), QString("X9:43"));
    }

    void ambiguityRequiresAValidChoice()
    {
        QETProject project(fixture()); const auto c = circuit(project); QVERIFY(c.report);
        auto *second = ElementFactory::Instance()->createElement(c.target->location());
        QVERIFY(second); QVERIFY(!second->terminals().isEmpty());
        c.target->diagram()->addItem(second); label(second, "X10:8");
        auto *extra = new Conductor(c.remote, second->terminals().first());
        QVERIFY(extra->isValid()); c.target->diagram()->addItem(extra);
        QCOMPARE(candidates(c.report).size(), 2);
        QCOMPARE(resolved(c.report), QString("?"));
        std::unique_ptr<QUndoCommand> macro(command(&project, {{c.report, "X10:8", false}}, false));
        QVERIFY(macro); project.undoStack()->push(macro.release());
        QTRY_COMPARE(field(c.report)->toPlainText(), QString("X10:8"));
        // The saved choice identifies the terminal, so a rename follows it.
        label(second, "X10:9");
        QTRY_COMPARE(field(c.report)->toPlainText(), QString("X10:9"));
    }

    void automaticNewReportsAndPersistence()
    {
        QETProject project(fixture()); const auto c = circuit(project); QVERIFY(c.report);
        std::unique_ptr<QUndoCommand> macro(command(&project, {{c.report, {}, false}}, true));
        QVERIFY(macro); project.undoStack()->push(macro.release());
        QVERIFY(project.property(autoProperty).toBool());
        auto *newReport = ElementFactory::Instance()->createElement(c.report->location());
        QVERIFY(newReport); c.report->diagram()->addItem(newReport);
        QVERIFY(field(newReport));
        const auto document = project.toXml();
        QCOMPARE(document.documentElement().firstChildElement("properties").attribute("faramax_auto_arrow_text"), QString("true"));
        QTemporaryDir directory; QVERIFY(directory.isValid());
        QFile file(directory.filePath("arrows.qet")); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(document.toByteArray()); file.close();
        QETProject reopened(file.fileName()); QCOMPARE(reopened.state(), QETProject::Ok);
        QVERIFY(reopened.property(autoProperty).toBool()); reopened.loadFolios();
        Element *reopenedReport = nullptr;
        for (auto *diagram : reopened.diagrams()) for (auto *element : diagram->elements())
            if (element->uuid() == c.report->uuid()) reopenedReport = element;
        QVERIFY(reopenedReport); QVERIFY(field(reopenedReport));
        QTRY_COMPARE(field(reopenedReport)->toPlainText(), QString("X9:42"));
    }

    void automaticFieldLoadsWithGuiProgressEvents()
    {
        QTemporaryDir directory;QString path=directory.filePath("automatic-report.qet");QUuid reportId;
        {
            QETProject source(fixture());const auto c=circuit(source);QVERIFY(c.report);reportId=c.report->uuid();
            std::unique_ptr<QUndoCommand> macro(command(&source,{{c.report,{},false}},false));QVERIFY(macro);source.undoStack()->push(macro.release());
            QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write(source.toXml().toByteArray());file.close();
        }
        auto *editor=QetAppTest::editor();QVERIFY(editor);
        QVERIFY(editor->openAndAddProject(path, false));
        auto *loaded=editor->currentProject();QVERIFY(loaded);QCOMPARE(loaded->state(),QETProject::Ok);
        loaded->loadFolios();Element *report=nullptr;
        for(auto *diagram:loaded->diagrams())for(auto *element:diagram->elements())if(element->uuid()==reportId)report=element;
        QVERIFY(report);QVERIFY(field(report));
        QTRY_COMPARE(field(report)->toPlainText(),QString("X9:42"));
        loaded->setReadOnly(true);QVERIFY(editor->closeProject(loaded));
    }

    void noLinkAndReadOnly()
    {
        QETProject project(fixture()); const auto c = circuit(project); QVERIFY(c.report);
        c.report->unlinkAllElements();
        QVERIFY(candidates(c.report).isEmpty());
        QCOMPARE(resolved(c.report), QString());
        project.setReadOnly(true);
        QVERIFY(!command(&project, {{c.report, {}, false}}, true));
    }

    void reportChainsAndCyclesTerminate()
    {
        QETProject project(fixture()); const auto c = circuit(project); QVERIFY(c.report);
        Element *continuation = nullptr;
        Element *farTarget = nullptr;
        for (auto *report : c.remote->diagram()->elements()) {
            if (report == c.remote->parentElement() || !(report->linkType() & Element::AllReport)
                    || report->linkedElements().isEmpty() || report->terminals().size() != 1) continue;
            auto *far = report->linkedElements().first();
            if (far->terminals().size() != 1 || far->terminals().first()->conductors().size() != 1) continue;
            auto *end = far->terminals().first();
            auto *wire = end->conductors().first();
            auto *peer = wire->terminal1 == end ? wire->terminal2 : wire->terminal1;
            if (peer->parentElement()->linkType() & Element::AllReport) continue;
            continuation = report; farTarget = peer->parentElement(); break;
        }
        QVERIFY(continuation); QVERIFY(farTarget);
        label(farTarget, "X7:2");
        c.remote->removeConductor(c.wire);
        auto *next = continuation->terminals().first();
        for (auto *wire : next->conductors()) next->removeConductor(wire);
        auto *bridge = new Conductor(c.remote, next);
        QVERIFY(bridge->isValid()); c.remote->diagram()->addItem(bridge);
        QCOMPARE(candidates(c.report), QStringList{"X7:2"});
        // Complete a cycle on the first folio; revisiting the source is
        // deliberately ignored, so the same target is found once.
        auto *far = continuation->linkedElements().first();
        auto *cycle = new Conductor(c.report->terminals().first(), far->terminals().first());
        QVERIFY(cycle->isValid()); c.report->diagram()->addItem(cycle);
        QCOMPARE(candidates(c.report), QStringList{"X7:2"});
    }
};
QET_APP_TEST_MAIN(tst_arrowtexts)
#include "tst_arrowtexts.moc"
