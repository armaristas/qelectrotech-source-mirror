// SPDX-License-Identifier: GPL-2.0-or-later
#include <QtTest>
#include "faramax/terminalflow.h"

using namespace Faramax::TerminalFlow;

namespace
{
Record terminal(QString id, QString label, int page = 0, double x = 0, double y = 0)
{
    Record record;
    record.id = id; record.label = label; record.folio = page; record.x = x; record.y = y;
    return record;
}
Record wire(QString id, QString label, QString potential)
{
    auto record = terminal(id, label);
    record.kind = Kind::Conductor; record.potential = potential;
    return record;
}
}

class tst_terminalflow : public QObject
{
    Q_OBJECT
private slots:
    void pageRanges()
    {
        QSet<int> pages; QString error;
        QVERIFY(parsePages("2,5-3; 7", 7, pages, error));
        QCOMPARE(pages, (QSet<int>{1, 2, 3, 4, 6}));
        QVERIFY(!parsePages("1,8", 7, pages, error));
        QVERIFY(pages.isEmpty());
        QVERIFY(!parsePages("1-99999999999999999999", 7, pages, error));
        QVERIFY(parsePages("todas", 2, pages, error));
        QCOMPARE(pages, (QSet<int>{0, 1}));
        QVERIFY(!parsePages(";;;", 2, pages, error));
    }

    void identifiersAreLiteral()
    {
        QCOMPARE(stripPrefix("-x12:3A", " -x "), QString("-X12"));
        QCOMPARE(stripPrefix("X+1:B", "X+"), QString("X+1"));
        QVERIFY(stripPrefix("XX1:3", "X+").isEmpty());
        QVERIFY(stripPrefix("X1:", "X").isEmpty());
        QVERIFY(stripPrefix("X1:2:3", "X").isEmpty());
    }

    void spatialOrderAndPages()
    {
        QVector<Record> records{terminal("a", "X1", 0, 20, 0), terminal("b", "X1", 0, 10, 50),
            terminal("c", "X1", 1, 0, 0), terminal("d", "X2", 0, 0, 0)};
        StripConfig config; config.prefix = "X1"; config.start = 10; config.pages = "1";
        auto plan = number(records, 2, {config}, "X", false, false);
        QVERIFY2(plan.canApply(), qPrintable(plan.errors.join('\n')));
        QCOMPARE(plan.changes.size(), 2);
        QCOMPARE(plan.changes[0].record, 1); QCOMPARE(plan.changes[0].label, QString("X1:10"));
        config.xFirst = false;
        plan = number(records, 2, {config}, "X", false, false);
        QCOMPARE(plan.changes[0].record, 0);
        QCOMPARE(records[0].label, QString("X1")); // planning is read-only
    }

    void existingAndLockedTerminals()
    {
        QVector<Record> records{terminal("a", "X1:1"), terminal("b", "X1:9A"), terminal("c", "X1")};
        records[0].locked = true;
        StripConfig config; config.prefix = "X1"; config.start = 10;
        auto plan = number(records, 1, {config}, "X", false, true);
        QCOMPARE(plan.changes.size(), 1); QCOMPARE(plan.changes[0].record, 2);
        plan = number(records, 1, {config}, "X", true, true);
        QCOMPARE(plan.changes.size(), 2);
        for (const auto &change : plan.changes) QVERIFY(change.record != 0);
    }

    void multipointAndIdempotence()
    {
        QVector<Record> records{terminal("a", "X1:A", 0, 0), terminal("b", "X1:B", 0, 10),
            terminal("c", "X1:C", 0, 20), terminal("d", "X1:A", 0, 30), terminal("e", "X1:B", 0, 40)};
        StripConfig config; config.prefix = "X1"; config.start = 5;
        auto plan = number(records, 1, {config}, "X", true, true);
        QVERIFY(plan.canApply());
        QStringList expected{"X1:5A", "X1:5B", "X1:5C", "X1:6A", "X1:6B"};
        for (int i = 0; i < plan.changes.size(); ++i) {
            QCOMPARE(plan.changes[i].label, expected[i]);
            records[plan.changes[i].record].label = plan.changes[i].label;
        }
        plan = number(records, 1, {config}, "X", true, true);
        QVERIFY(plan.changes.isEmpty());
    }

    void collisionsWithUntouchedPagesAndLocks()
    {
        QVector<Record> records{terminal("a", "X1:1", 1), terminal("b", "X1", 0)};
        StripConfig config; config.prefix = "X1"; config.pages = "1";
        auto plan = number(records, 2, {config}, "X", false, false);
        QVERIFY(!plan.canApply()); QVERIFY(!plan.errors.isEmpty());
        records[0].label = "X1:001";
        QVERIFY(!number(records, 2, {config}, "X", false, false).canApply());
        // Same label in a different plant is not a collision.
        records[0].scope = "other plant";
        plan = number(records, 2, {config}, "X", false, false);
        QVERIFY(plan.canApply());
    }

    void formulasOnlyAndDisabledConfig()
    {
        QVector<Record> records{terminal("a", "X1:1")}; records[0].formula = "%sequ_1";
        StripConfig config; config.prefix = "X1";
        auto plan = number(records, 1, {config}, "X", true, false);
        QCOMPARE(plan.changes.size(), 1); QCOMPARE(plan.changes[0].label, QString("X1:1"));
        config.enabled = false;
        QVERIFY(number(records, 1, {config}, "X", true, false).changes.isEmpty());
    }

    void shiftFromOriginalNumbers()
    {
        QVector<Record> records{terminal("a", "X1:10"), terminal("b", "X1:11"), terminal("c", "X2:10")};
        Range range; range.prefix = "X1"; range.first = 10; range.last = 11; range.offset = 2;
        auto plan = shift(records, {range});
        QVERIFY(plan.canApply()); QCOMPARE(plan.changes.size(), 2);
        QCOMPARE(plan.changes[0].label, QString("X1:12"));
        QCOMPARE(plan.changes[1].label, QString("X1:13"));
        range.first = 12; range.last = 13;
        Range original; original.prefix = "X1"; original.first = 10; original.last = 11; original.offset = 2;
        plan = shift(records, {original, range});
        QCOMPARE(plan.changes[0].label, QString("X1:12")); // no cascading
    }

    void shiftWiresAndCablePrefix()
    {
        QVector<Record> records{wire("a", "12", "p1"), wire("b", "12", "p1"),
            wire("c", "12-3", "p2"), wire("d", "12.4", "p3")};
        Range simple; simple.target = Target::Wires; simple.first = simple.last = 12; simple.offset = 2;
        auto plan = shift(records, {simple});
        QVERIFY(plan.canApply()); QCOMPARE(plan.changes.size(), 2);
        QCOMPARE(plan.changes[0].label, QString("14"));
        Range cable = simple; cable.target = Target::Cables;
        plan = shift(records, {cable});
        QVERIFY(plan.canApply()); QCOMPARE(plan.changes.size(), 1);
        QCOMPARE(plan.changes[0].label, QString("14-3"));
        cable.delimiter = ".";
        QCOMPARE(shift(records, {cable}).changes[0].label, QString("14.4"));
    }

    void wireCollisionsRespectPotentials()
    {
        QVector<Record> records{wire("a", "10", "p1"), wire("b", "12", "p2")};
        Range range; range.target = Target::Wires; range.first = range.last = 10; range.offset = 2;
        QVERIFY(!shift(records, {range}).canApply());
        records[1].potential = "p1";
        QVERIFY(shift(records, {range}).canApply());
    }

    void badAndOverlappingRanges()
    {
        QVector<Record> records{terminal("a", "X1:1")};
        Range range; range.prefix = "X1"; range.offset = -2;
        QVERIFY(!shift(records, {range}).canApply());
        range.offset = 1;
        QVERIFY(!shift(records, {range, range}).canApply());
        range.first = 3; range.last = 1;
        QVERIFY(!shift(records, {range}).canApply());
    }

    void diagnosticDoesNotExpandHugeGaps()
    {
        QVector<Record> records{terminal("a", "X1:1"), terminal("b", "X1:1"),
            terminal("c", "X1:1000000000"), terminal("d", "X2:1A"), terminal("e", "X2:1B")};
        const auto messages = diagnose(records, "X");
        QCOMPARE(messages.size(), 2);
        QVERIFY(messages[0].contains("duplicados [1]"));
        QVERIFY(messages[0].contains(QString::fromUtf8("2–999999999")));
        QVERIFY(messages[1].contains("duplicados []"));
        records << terminal("f", "X2:1A");
        QVERIFY(diagnose(records, "X")[1].contains("duplicados [1A]"));
    }
};
QTEST_GUILESS_MAIN(tst_terminalflow)
#include "tst_terminalflow.moc"
