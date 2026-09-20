#include "ui/frequencyentryparser.h"

#include <QtTest>

class TestFrequencyEntryParser : public QObject {
    Q_OBJECT

private slots:
    void accepted_data();
    void accepted();
    void rejected_data();
    void rejected();
};

void TestFrequencyEntryParser::accepted_data() {
    QTest::addColumn<QString>("input");
    QTest::addColumn<quint64>("expected");

    QTest::newRow("short tenth MHz") << QStringLiteral("7.2") << 7200000ULL;
    QTest::newRow("short kHz") << QStringLiteral("7.215") << 7215000ULL;
    QTest::newRow("short Hz") << QStringLiteral("7.2155") << 7215500ULL;
    QTest::newRow("full grouped") << QStringLiteral("7.215.000") << 7215000ULL;
    QTest::newRow("partial grouped") << QStringLiteral("7.2.5") << 7200500ULL;
    QTest::newRow("VHF") << QStringLiteral("144.2") << 144200000ULL;
    QTest::newRow("L band") << QStringLiteral("1296.0") << 1296000000ULL;
    QTest::newRow("raw Hz") << QStringLiteral("7215000") << 7215000ULL;
}

void TestFrequencyEntryParser::accepted() {
    QFETCH(QString, input);
    QFETCH(quint64, expected);
    quint64 actual = 0;
    QVERIFY(FrequencyEntryParser::parse(input, &actual));
    QCOMPARE(actual, expected);
}

void TestFrequencyEntryParser::rejected_data() {
    QTest::addColumn<QString>("input");
    QTest::newRow("empty") << QString();
    QTest::newRow("missing MHz") << QStringLiteral(".215");
    QTest::newRow("missing fraction") << QStringLiteral("7.");
    QTest::newRow("too precise") << QStringLiteral("7.1234567");
    QTest::newRow("too many groups") << QStringLiteral("7.1.2.3");
    QTest::newRow("letters") << QStringLiteral("7.2MHz");
}

void TestFrequencyEntryParser::rejected() {
    QFETCH(QString, input);
    quint64 actual = 0;
    QVERIFY(!FrequencyEntryParser::parse(input, &actual));
}

QTEST_APPLESS_MAIN(TestFrequencyEntryParser)
#include "test_frequencyentryparser.moc"
