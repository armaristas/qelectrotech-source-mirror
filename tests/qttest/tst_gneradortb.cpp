// SPDX-License-Identifier: GPL-2.0-or-later
#include "TerminalStrip/GraphicsItem/terminalstripitem.h"
#include "TerminalStrip/UndoCommand/addterminaltostripcommand.h"
#include "TerminalStrip/physicalterminal.h"
#include "TerminalStrip/realterminal.h"
#include "TerminalStrip/terminalstrip.h"
#include "cable/cable.h"
#include "diagram.h"
#include "factory/elementfactory.h"
#include "faramax/gneradortb.h"
#include "qetapptest.h"
#include "qetgraphicsitem/conductor.h"
#include "qetgraphicsitem/element.h"
#include "qetgraphicsitem/terminal.h"
#include "qetproject.h"
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QUndoStack>
#include <memory>
using namespace Faramax::TB;

class tst_gneradortb : public QObject
{
    Q_OBJECT
    QString fixture()
    {
        return QFINDTESTDATA("fixtures/every_feature.qet");
    }
    QVector<Row> wired(TerminalStrip *strip)
    {
        auto result = rows(strip);
        for (int i = result.size() - 1; i >= 0; --i)
            if (!wire(result[i]))
                result.removeAt(i);
        return result;
    }
    Conductor *wire(const Row &r)
    {
        if (r.terminal->element())
            for (auto *pin : r.terminal->element()->terminals())
                if (!pin->conductors().isEmpty())
                    return pin->conductors().first();
        return nullptr;
    }
    TerminalStrip *spares(QETProject &p, int count)
    {
        auto *s = p.newTerminalStrip({}, {}, "XT");
        QVector<QSharedPointer<RealTerminal>> terminals;
        for (int i = 1; i <= count; ++i)
        {
            ElementData d;
            d.m_informations.addValue("label", QString("XT:%1").arg(i));
            d.m_informations.addValue("manufacturer_reference", "BL");
            terminals << RealTerminal::createSpare(d);
        }
        s->insertSpareTerminals(0, terminals);
        return s;
    }
    void push(QETProject &p, TerminalStrip *s, QVector<Row> edits, QJsonObject cfg)
    {
        QString error;
        std::unique_ptr<QUndoCommand> cmd(editCommand(s, edits, cfg, error));
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(cmd);
        p.undoStack()->push(cmd.release());
    }
  private slots:
    void classification()
    {
        auto cfg = defaults();
        QCOMPARE(classify("DEV:3", true, false, cfg), QString("EXT"));
        QCOMPARE(classify("K1:3", false, false, cfg), QString("INT"));
        cfg["external_name_exceptions"] = "K*|EXT;DEV=>INT;M*";
        QCOMPARE(classify("K1:3", false, false, cfg), QString("EXT"));
        QCOMPARE(classify("DEV:3", true, false, cfg), QString("INT"));
        QCOMPARE(classify("M2:3", false, false, cfg), QString("EXT"));
        QCOMPARE(classify("K1:3", true, true, cfg), QString("INT"));
    }
    void manualOverrideUndoAndResume()
    {
        QETProject p(fixture());
        p.loadFolios();
        auto *s = p.terminalStrip().first();
        auto r = wired(s).first();
        auto *conductor = wire(r);
        QVERIFY(conductor);
        auto edits = rows(s);
        int index = -1;
        for (int i = 0; i < edits.size(); ++i)
            if (edits[i].id == r.id)
                index = i;
        QVERIFY(index >= 0);
        edits[index].edits = {{"disp_ext", "MANUAL:1"}};
        edits[index].values["disp_ext"] = "MANUAL:1";
        const auto original = r.before;
        push(p, s, edits, settings(s));
        QCOMPARE(rows(s)[index].values["disp_ext"].toString(), QString("MANUAL:1"));
        auto prop = conductor->properties();
        prop.text = "W99-17";
        prop.m_formula.clear();
        conductor->setProperties(prop);
        QCOMPARE(rows(s)[index].values["disp_ext"].toString(), QString("MANUAL:1"));
        p.undoStack()->undo();
        QCOMPARE(r.terminal->terminalData(), original);
        p.undoStack()->redo();
        auto again = rows(s);
        again[index].edits = {{"disp_ext", ""}};
        push(p, s, again, settings(s));
        QVERIFY(!rows(s)[index].values["disp_ext"].toString().contains("MANUAL"));
        QVERIFY(!r.terminal->terminalData().m_informations.contains(key("disp_ext")));
    }
    void projectChangesNotifyWithoutUndoNoise()
    {
        QETProject p(fixture());
        p.loadFolios();
        auto *s = p.terminalStrip().first();
        auto r = wired(s).first();
        auto cfg = settings(s);
        cfg["enabled"] = true;
        push(p, s, rows(s), cfg);
        watch(&p);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        TerminalStripItem item(s);
        const auto before = item.boundingRect();
        QSignalSpy spy(s, &TerminalStrip::dataChanged);
        auto *conductor = wire(r);
        auto prop = conductor->properties();
        prop.text = "W10-2";
        prop.m_formula.clear();
        const int count = p.undoStack()->count();
        conductor->setProperties(prop);
        QTRY_VERIFY(!spy.isEmpty());
        QCOMPARE(p.undoStack()->count(), count);
        bool found = false;
        for (const auto &row : rows(s))
            found |= row.values["cable_int"].toString().contains("W10-2") ||
                     row.values["cable_ext"].toString().contains("W10-2");
        QVERIFY(found);
        QVERIFY(item.boundingRect().isValid());
        QVERIFY(before.isValid());
        auto *peer = (conductor->terminal1->parentElement() == r.terminal->element() ? conductor->terminal2
                                                                                     : conductor->terminal1)
                         ->parentElement();
        spy.clear();
        auto info = peer->elementInformations();
        info.addValue("formula", QString());
        info.addValue("label", QString(200, 'L'));
        peer->setElementInformations(info);
        QTRY_VERIFY(!spy.isEmpty());
        QCOMPARE(peer->actualLabel(),QString(200,'L'));
        QTRY_VERIFY2(item.boundingRect().height() > before.height(),qPrintable(QString("before=%1 after=%2 rows=%3").arg(before.height()).arg(item.boundingRect().height()).arg(encode(rows(s)[0].values))));
        auto *cable = p.newCable();
        cable->setDesignation("W_NATIVE");
        cable->setCoreCount(2);
        CableCore assignment;
        assignment.core = 1;
        assignment.conductor = conductor->uuid();
        cable->setCore(assignment);
        QCoreApplication::processEvents();
        spy.clear();
        cable->setDesignation("W_RENAMED");
        QTRY_VERIFY(!spy.isEmpty());
        found = false;
        for (const auto &row : rows(s))
            found |= row.values["mang_int"].toString().contains("W_RENAMED") ||
                     row.values["mang_ext"].toString().contains("W_RENAMED");
        QVERIFY(found);
        spy.clear();
        const int ownIndex = p.undoStack()->index();
        QTest::qWait(20);
        QCOMPARE(spy.count(), 0);
        QCOMPARE(p.undoStack()->index(), ownIndex);
    }
    void nativeReservesBridgesLevelsAndPersistence()
    {
        QETProject p(fixture());
        p.loadFolios();
        auto *s = spares(p, 3);
        auto cfg = settings(s);
        cfg["enabled"] = true;
        cfg["reserve_count"] = 2;
        auto edits = rows(s);
        edits[0].edits = {{"puente", true}};
        edits[0].values["puente"] = true;
        push(p, s, edits, cfg);
        QCOMPARE(s->realTerminals().size(), 5);
        QVERIFY(s->realTerminals()[0]->isBridged());
        QVERIFY(s->realTerminals()[1]->isBridged());
        auto uuid = s->realTerminals().last()->elementUuid();
        p.undoStack()->undo();
        QCOMPARE(s->realTerminals().size(), 3);
        QVERIFY(!s->realTerminals()[0]->isBridged());
        p.undoStack()->redo();
        QCOMPARE(s->realTerminals().last()->elementUuid(), uuid);
        QString error;
        std::unique_ptr<QUndoCommand> multi(expandCommand(s, rows(s)[2], {"A", "B", "C"}, error));
        QVERIFY2(multi, qPrintable(error));
        const auto physical = s->physicalTerminal()[2];
        p.undoStack()->push(multi.release());
        QCOMPARE(physical->realTerminalCount(), 3);
        QCOMPARE(physical->realTerminals()[2]->level(), 2);
        p.undoStack()->undo();
        QCOMPARE(physical->realTerminalCount(), 1);
        p.undoStack()->redo();
        QCOMPARE(physical->realTerminalCount(), 3);
        auto xml = p.toXml();
        QTemporaryDir directory;
        QString path = directory.filePath("roundtrip.qet");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(xml.toByteArray());
        file.close();
        QETProject reopened(path);
        QCOMPARE(reopened.state(), QETProject::Ok);
        TerminalStrip *restored = nullptr;
        for (auto *candidate : reopened.terminalStrip())
            if (candidate->name() == "XT")
                restored = candidate;
        QVERIFY(restored);
        QCOMPARE(settings(restored), cfg);
        QCOMPARE(restored->realTerminals().size(), s->realTerminals().size());
        QCOMPARE(restored->physicalTerminal()[2]->realTerminalCount(), 3);
        QVERIFY(restored->realTerminals()[0]->isBridged());
    }
    void bridgeExtensionAndCombUndoRestorePreviousGroups()
    {
        QETProject p(fixture());
        p.loadFolios();
        auto *s = spares(p, 4);
        auto original = s->realTerminals();
        QVERIFY(s->setBridge({original[0], original[1]}));
        auto oldBridge = original[0]->bridge();
        auto edited = rows(s);
        edited[1].edits = {{"puente", true}};
        edited[1].values["puente"] = true;
        push(p, s, edited, settings(s));
        QCOMPARE(original[2]->bridge(), original[0]->bridge());
        p.undoStack()->undo();
        QCOMPARE(original[0]->bridge(), oldBridge);
        QVERIFY(!original[2]->isBridged());
        p.undoStack()->redo();
        QCOMPARE(original[2]->bridge(), original[0]->bridge());
        edited = rows(s);
        edited[0].edits = {{"peine", true}, {"peine_grupo", 4}};
        edited[0].values["peine"] = true;
        edited[0].values["peine_grupo"] = 4;
        push(p, s, edited, settings(s));
        QVERIFY(original[3]->isBridged());
        p.undoStack()->undo();
        QVERIFY(!original[3]->isBridged());
        QVERIFY(original[2]->isBridged());
    }
    void removeRestoresNativeIdentityOrderAndBridges()
    {
        QETProject p(fixture());
        p.loadFolios();
        auto *s = spares(p, 3);
        auto original = rows(s);
        QVERIFY(s->setBridge({original[0].terminal, original[1].terminal}));
        auto bridge = original[0].terminal->bridge();
        std::unique_ptr<QUndoCommand> command(removeCommand(s, {original[0]}));
        QVERIFY(command);
        p.undoStack()->push(command.release());
        QCOMPARE(rows(s).size(), 2);
        p.undoStack()->undo();
        auto restored = rows(s);
        QCOMPARE(restored.size(), 3);
        for (int i = 0; i < original.size(); ++i)
            QCOMPARE(restored[i].id, original[i].id);
        QCOMPARE(restored[0].terminal->bridge(), bridge);
        QCOMPARE(restored[1].terminal->bridge(), bridge);
        p.undoStack()->redo();
        QCOMPARE(rows(s).size(), 2);
    }
    void changingReserveCountKeepsExistingIdentityAndManualReserves()
    {
        QETProject p(fixture());
        p.loadFolios();
        auto *s = spares(p, 1);
        auto cfg = settings(s);
        cfg["reserve_count"] = 2;
        push(p, s, rows(s), cfg);
        auto before = rows(s);
        const auto first = before[1].id, second = before[2].id;
        cfg["reserve_count"] = 3;
        push(p, s, rows(s), cfg);
        QCOMPARE(rows(s)[1].id, first);
        QCOMPARE(rows(s)[2].id, second);
        auto edited = rows(s);
        edited[1].edits = {{"auto_reserve", false}};
        edited[1].values["auto_reserve"] = false;
        cfg["reserve_count"] = 2;
        push(p, s, edited, cfg);
        QCOMPARE(rows(s)[1].id, first);
        QCOMPARE(rows(s)[2].id, second);
        QVERIFY(!rows(s)[1].values["auto_reserve"].toBool());
        cfg["reserve_count"] = 0;
        push(p, s, rows(s), cfg);
        QCOMPARE(rows(s).size(), 2);
        QCOMPARE(rows(s)[1].id, first);
        p.undoStack()->undo();
        QCOMPARE(rows(s)[2].id, second);
    }
    void doubleTerminalsAndAnnotationTypesSurviveSave()
    {
        QETProject p(fixture());
        p.loadFolios();
        auto *s = spares(p, 2);
        QString error;
        std::unique_ptr<QUndoCommand> command(expandCommand(s, rows(s)[0], {"=", "="}, error));
        QVERIFY2(command, qPrintable(error));
        p.undoStack()->push(command.release());
        QCOMPARE(s->physicalTerminal()[0]->realTerminalCount(), 2);
        QCOMPARE(s->realTerminals()[0]->label(), s->realTerminals()[1]->label());
        QVERIFY(rows(s)[0].values["double"].toBool());
        auto edited = rows(s);
        edited[0].edits = {{"corte", true}, {"shl_ext", true}, {"peine_grupo", 4}};
        for (auto it = edited[0].edits.begin(); it != edited[0].edits.end(); ++it)
            edited[0].values[it.key()] = it.value();
        push(p, s, edited, settings(s));
        auto dom = p.toXml();
        QTemporaryDir dir;
        QFile file(dir.filePath("types.qet"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(dom.toByteArray());
        file.close();
        QETProject restored(file.fileName());
        QCOMPARE(restored.state(), QETProject::Ok);
        TerminalStrip *copy = nullptr;
        for (auto *candidate : restored.terminalStrip())
            if (candidate->name() == "XT")
                copy = candidate;
        QVERIFY(copy);
        const auto output = rows(copy);
        QVERIFY(output[0].values["corte"].toBool());
        QVERIFY(output[0].values["shl_ext"].toBool());
        QCOMPARE(output[0].values["peine_grupo"].toInt(), 4);
        QVERIFY(output[0].values["double"].toBool());
    }
    void remoteFolioRenameDisconnectAndDeletedTerminal()
    {
        QETProject source(fixture());
        source.loadFolios();
        auto templateRow = wired(source.terminalStrip().first()).first();
        QETProject p(QFINDTESTDATA("../../examples/iso_sfc_example.qet"));
        QCOMPARE(p.state(), QETProject::Ok);
        p.loadFolios();
        Element *report = nullptr, *target = nullptr;
        Terminal *remote = nullptr;
        Conductor *farWire = nullptr;
        for (auto *folio : p.diagrams())
            for (auto *element : folio->elements())
            {
                if (report || element->linkType() != Element::NextReport ||
                    element->linkedElements().isEmpty())
                    continue;
                auto *partner = element->linkedElements().first();
                if (partner->terminals().size() != 1 ||
                    partner->terminals().first()->conductors().size() != 1)
                    continue;
                remote = partner->terminals().first();
                farWire = remote->conductors().first();
                auto *other = farWire->terminal1 == remote ? farWire->terminal2 : farWire->terminal1;
                if (other->parentElement()->linkType() & Element::AllReport)
                    continue;
                report = element;
                target = other->parentElement();
            }
        QVERIFY(report);
        auto *terminal =
            ElementFactory::Instance()->createElement(templateRow.terminal->element()->location());
        QVERIFY(terminal);
        QVERIFY(terminal->elementData().m_type == ElementData::Terminal);
        report->diagram()->addItem(terminal);
        auto *strip = p.newTerminalStrip({}, {}, "XR");
        QVERIFY(strip->addTerminal(terminal));
        auto info = terminal->elementInformations();
        info.addValue("label", "XR:1");
        info.addValue("formula", QString());
        terminal->setElementInformations(info);
        auto *nearWire = new Conductor(terminal->terminals().first(), report->terminals().first());
        QVERIFY(nearWire->isValid());
        report->diagram()->addItem(nearWire);
        auto targetInfo = target->elementInformations();
        targetInfo.addValue("label", "REMOTE:17");
        targetInfo.addValue("formula", QString());
        target->setElementInformations(targetInfo);
        auto cfg = settings(strip);
        cfg["external_name_exceptions"] = "REMOTE*|EXT";
        strip->setProperty(settingsProperty, encode(cfg));
        watch(&p);
        QCoreApplication::processEvents();
        QVERIFY(rows(strip)[0].values["disp_ext"].toString().contains("REMOTE:17"));
        QSignalSpy spy(strip, &TerminalStrip::dataChanged);
        targetInfo.addValue("label", "REMOTE:18");
        target->setElementInformations(targetInfo);
        QTRY_VERIFY(!spy.isEmpty());
        QVERIFY(rows(strip)[0].values["disp_ext"].toString().contains("REMOTE:18"));
        remote->removeConductor(farWire);
        QTRY_VERIFY(rows(strip)[0].values["disp_ext"].toString().isEmpty());
        QVERIFY(remote->addConductor(farWire));
        QTRY_VERIFY(rows(strip)[0].values["disp_ext"].toString().contains("REMOTE:18"));
        auto *folio = terminal->diagram();
        const auto identity = rows(strip)[0].id;
        folio->removeItem(terminal);
        QTRY_VERIFY(rows(strip).isEmpty());
        folio->addItem(terminal);
        QTRY_COMPARE(rows(strip).size(), 1);
        QCOMPARE(rows(strip)[0].id, identity);
    }
    void staleAndInvalidPlansDoNotMutate()
    {
        QETProject p(fixture());
        p.loadFolios();
        auto *s = spares(p, 2);
        auto edits = rows(s);
        edits[0].edits = {{"num_borna", "5"}};
        auto changed = edits[0].before;
        changed.m_informations.addValue("label", "XT:999");
        edits[0].terminal->setSpareData(changed);
        QString error;
        std::unique_ptr<QUndoCommand> cmd(editCommand(s, edits, settings(s), error));
        QVERIFY(!cmd);
        QVERIFY(!error.isEmpty());
        edits = rows(s);
        edits[0].edits = {{"color", "invalid color"}};
        const auto cfg = s->property(settingsProperty);
        cmd.reset(editCommand(s, edits, settings(s), error));
        QVERIFY(!cmd);
        QCOMPARE(s->property(settingsProperty), cfg);
        QVERIFY(!s->realTerminals()[0]->isBridged());
    }
    void legacyWorkbookProducedByOpenpyxl()
    {
        QJsonArray data;
        QJsonObject config;
        QString error;
        QVERIFY2(readWorkbook(QFINDTESTDATA("fixtures/gneradortb_legacy.xlsx"), data, config, error),
                 qPrintable(error));
        QCOMPARE(data.size(), 2);
        QCOMPARE(data[0].toObject()["tipo_borna"].toString(), QString("ground"));
        QCOMPARE(data[1].toObject()["tipo_borna"].toString(), QString("generic"));
        QVERIFY(data[0].toObject()["corte"].toBool());
        QCOMPARE(data[0].toObject()["nivel"].toInt(), 0);
        QCOMPARE(data[1].toObject()["nivel"].toInt(), 1);
        QVERIFY(!data[0].toObject().contains("_manual"));
        QVERIFY(!data[0].toObject().contains("_id"));
    }
    void workbookRoundtripAndFormulaText()
    {
        QETProject p(fixture());
        p.loadFolios();
        auto *s = spares(p, 2);
        auto data = rows(s);
        data[0].values["modelo"] = "=1+1 & <test>";
        data[0].edits = {{"disp_ext", "MANUAL:2"}};
        data[0].values["disp_ext"] = "MANUAL:2";
        QTemporaryDir dir;
        QString error;
        auto cfg = settings(s);
        QVERIFY2(writeWorkbook(dir.filePath("history.xlsx"), data, cfg, "Project", error), qPrintable(error));
        const auto external = qEnvironmentVariable("QET_TB_WORKBOOK_PREVIEW");
        if (!external.isEmpty())
        {
            QFile::remove(external);
            QVERIFY(QFile::copy(dir.filePath("history.xlsx"), external));
        }
        QJsonArray imported;
        QJsonObject config;
        QVERIFY2(readWorkbook(dir.filePath("history.xlsx"), imported, config, error), qPrintable(error));
        QCOMPARE(imported.size(), 2);
        QCOMPARE(imported[0].toObject()["modelo"].toString(), QString("=1+1 & <test>"));
        QCOMPARE(imported[0].toObject()["_id"].toString(), data[0].id);
        QCOMPARE(imported[0].toObject()["_manual"].toObject()["disp_ext"].toString(), QString("MANUAL:2"));
        QCOMPARE(cfg, config);
    }
    void previewAndEditorUseNativeRows()
    {
        QETProject p(fixture());
        p.loadFolios();
        auto *s = p.terminalStrip().first();
        auto data = rows(s);
        QVERIFY(data.size() > 1);
        for(int i=0;i<data.size();++i){data[i].values["num_borna"]=QString::number(i+1);data[i].values["posicion"]=QString("1-A%1").arg(i+1);data[i].values["modelo"]="BL";data[i].values["color"]="Auto";for(const auto &field:{"disp_int","disp_ext","cable_int","cable_ext","mang_int","mang_ext"})data[i].values[field]="";}
        auto cfg = settings(s);
        cfg["enabled"] = true;
        data[0].values["corte"] = true;
        data[0].values["shl_ext"] = true;
        data[0].values["mang_shl_ext"] = "1/2";
        data[0].values["disp_ext"] = "SENSOR:1";
        data[0].values["mang_ext"] = "W1";
        data[0].values["modelo"] = "PE";
        data[1].values["disp_int"] = "K1:2";
        const auto box = render(nullptr, s, data, cfg);
        QVERIFY(box.isValid());
        QVERIFY(box.height() > 800);
        QImage image(box.size().toSize() + QSize(20, 20), QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        painter.translate(-box.topLeft() + QPointF(10, 10));
        render(&painter, s, data, cfg);
        painter.end();
        QVERIFY(image != QImage());
        QString path = qEnvironmentVariable("QET_TB_PREVIEW");
        if (!path.isEmpty())
        {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QVERIFY(image.save(path));
        }
        const auto count = p.undoStack()->count();
        openEditor(&p, s, nullptr);
        QDialog *dialog = nullptr;
        for (auto *widget : QApplication::topLevelWidgets())
            if (widget->objectName() == "faramaxGNeradorTB")
                dialog = qobject_cast<QDialog *>(widget);
        QVERIFY(dialog);
        auto *table = dialog->findChild<QTableWidget *>("gneradorTBTable");
        QVERIFY(table);
        QCOMPARE(table->rowCount(), data.size());
        QCOMPARE(table->columnCount(), 22);
        QCOMPARE(p.undoStack()->count(), count);
        QString screenshot = qEnvironmentVariable("QET_TB_EDITOR_PREVIEW");
        if (!screenshot.isEmpty())
        {
            QTest::qWait(100);
            QVERIFY(dialog->grab().save(screenshot));
        }
        dialog->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
};
QET_APP_TEST_MAIN(tst_gneradortb)
#include "tst_gneradortb.moc"
