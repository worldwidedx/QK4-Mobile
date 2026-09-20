#include "settings/fnkeymemory.h"

#include <QJsonArray>
#include <QtTest>

class TestFnKeyMemory : public QObject {
    Q_OBJECT

private slots:
    void roundTripKeepsOrderedFnBank();
    void rejectsWrongFileTypeAndVersion();
    void rejectsIncompleteOrDuplicateBank();
    void validatesOperatorEditableFields();
};

namespace {
FnKeyMemory::Setup validSetup() {
    FnKeyMemory::Setup setup;
    setup.name = QStringLiteral("Field Day");
    for (const QString &key : FnKeyMemory::keyNames())
        setup.keys.insert(key, FnKeyMemory::Entry{});
    setup.keys[QStringLiteral("F1")] = {QStringLiteral("CQ"), QStringLiteral("KY CQ CQ;")};
    setup.keys[QStringLiteral("F8")] = {QStringLiteral("MUTE"), QStringLiteral("AG000;AG;")};
    return setup;
}
} // namespace

void TestFnKeyMemory::roundTripKeepsOrderedFnBank() {
    const FnKeyMemory::Setup original = validSetup();
    QString error;
    QVERIFY2(FnKeyMemory::validate(original, &error), qPrintable(error));

    const QJsonObject json = FnKeyMemory::toJson(original);
    QCOMPARE(json.value(QStringLiteral("format")).toString(),
             QStringLiteral("qk4-fn-key-mapping"));
    QCOMPARE(json.value(QStringLiteral("version")).toInt(), 1);
    const QJsonArray keys = json.value(QStringLiteral("keys")).toArray();
    QCOMPARE(keys.size(), 8);
    QCOMPARE(keys.first().toObject().value(QStringLiteral("key")).toString(),
             QStringLiteral("F1"));
    QCOMPARE(keys.last().toObject().value(QStringLiteral("key")).toString(),
             QStringLiteral("F8"));

    FnKeyMemory::Setup decoded;
    QVERIFY2(FnKeyMemory::fromJson(json, &decoded, &error), qPrintable(error));
    QVERIFY(decoded == original);
}

void TestFnKeyMemory::rejectsWrongFileTypeAndVersion() {
    const QJsonObject json = FnKeyMemory::toJson(validSetup());
    FnKeyMemory::Setup decoded;
    QString error;

    QJsonObject wrongType = json;
    wrongType.insert(QStringLiteral("format"), QStringLiteral("qk4-ctr2-midi-mapping"));
    QVERIFY(!FnKeyMemory::fromJson(wrongType, &decoded, &error));
    QVERIFY(error.contains(QStringLiteral("not a QK4 FN"), Qt::CaseInsensitive));

    QJsonObject wrongVersion = json;
    wrongVersion.insert(QStringLiteral("version"), 99);
    QVERIFY(!FnKeyMemory::fromJson(wrongVersion, &decoded, &error));
    QVERIFY(error.contains(QStringLiteral("version"), Qt::CaseInsensitive));
}

void TestFnKeyMemory::rejectsIncompleteOrDuplicateBank() {
    const QJsonObject json = FnKeyMemory::toJson(validSetup());
    FnKeyMemory::Setup decoded;
    QString error;

    QJsonObject missing = json;
    QJsonArray missingKeys = missing.value(QStringLiteral("keys")).toArray();
    missingKeys.removeLast();
    missing.insert(QStringLiteral("keys"), missingKeys);
    QVERIFY(!FnKeyMemory::fromJson(missing, &decoded, &error));

    QJsonObject duplicate = json;
    QJsonArray duplicateKeys = duplicate.value(QStringLiteral("keys")).toArray();
    duplicateKeys[7] = duplicateKeys[0];
    duplicate.insert(QStringLiteral("keys"), duplicateKeys);
    QVERIFY(!FnKeyMemory::fromJson(duplicate, &decoded, &error));
    QVERIFY(error.contains(QStringLiteral("more than once"), Qt::CaseInsensitive));
}

void TestFnKeyMemory::validatesOperatorEditableFields() {
    FnKeyMemory::Setup setup = validSetup();
    QString error;

    setup.keys[QStringLiteral("F1")].command = QStringLiteral("KY CQ CQ");
    QVERIFY(!FnKeyMemory::validate(setup, &error));
    QVERIFY(error.contains(QStringLiteral("semicolon"), Qt::CaseInsensitive));

    setup = validSetup();
    setup.keys[QStringLiteral("F1")].label = QStringLiteral("TOO-LONG-LABEL");
    QVERIFY(!FnKeyMemory::validate(setup, &error));
    QVERIFY(error.contains(QStringLiteral("12-character"), Qt::CaseInsensitive));

    setup = validSetup();
    setup.keys[QStringLiteral("F1")].command = QStringLiteral("KY CQ\nCQ;");
    QVERIFY(!FnKeyMemory::validate(setup, &error));
    QVERIFY(error.contains(QStringLiteral("printable ASCII"), Qt::CaseInsensitive));
}

QTEST_APPLESS_MAIN(TestFnKeyMemory)
#include "test_fnkeymemory.moc"
