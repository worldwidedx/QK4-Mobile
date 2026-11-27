#include <QTest>
#include "settings/k4macrobackup.h"
#include "ui/fnpopupwidget.h"

// Fixture: verbatim excerpt of a k4macros.json written by the K4's backup utility
// (Fn > hold BACKUP). Note the slot names — "Kpod.1T", "REM.ANT" — differ from QK4's
// MacroIds ("K-pod.1T", "REM_ANT"); mistranslating them would import macros into
// slots nothing dispatches, which is what these tests pin down.
static const QByteArray kBackupJson = R"({
    "Fn.F1": ["", ""],
    "Kpod.1H": ["M1 hold", "SW162;"],
    "Kpod.1T": ["M1", "SW17;"],
    "Kpod.8H": ["", "RC;"],
    "Kpod.8T": ["Restore Pileup", "SB0;AG/;#FXT$2;#SP60;AB4;VX0;"],
    "PF1": ["", ""],
    "REM.ANT": ["Auto Ref Adj", "#AR1;"],
    "Bogus.9Z": ["nope", "XX;"]
})";

class TestK4MacroBackup : public QObject {
    Q_OBJECT

private slots:
    // K-Pod tap slots land on the IDs HardwareController emits on a button press.
    void mapsKpodTapSlot() {
        const auto macros = K4MacroBackup::parseJson(kBackupJson);
        QVERIFY(macros.contains(MacroIds::Kpod1T));
        QCOMPARE(macros.value(MacroIds::Kpod1T).label, QString("M1"));
        QCOMPARE(macros.value(MacroIds::Kpod1T).command, QString("SW17;"));
    }

    void mapsKpodHoldSlot() {
        const auto macros = K4MacroBackup::parseJson(kBackupJson);
        QCOMPARE(macros.value(MacroIds::Kpod1H).command, QString("SW162;"));
    }

    // The backup's "Kpod." prefix must not survive into the store.
    void doesNotKeepRawBackupSlotNames() {
        const auto macros = K4MacroBackup::parseJson(kBackupJson);
        QVERIFY(!macros.contains("Kpod.1T"));
        QVERIFY(!macros.contains("Kpod.8T"));
    }

    void mapsRemAnt() {
        const auto macros = K4MacroBackup::parseJson(kBackupJson);
        QCOMPARE(macros.value(MacroIds::RemAnt).command, QString("#AR1;"));
        QVERIFY(!macros.contains("REM.ANT"));
    }

    // Unassigned slots carry an empty command — RadioSettings drops those, so the
    // importer must not offer them.
    void skipsEmptyCommands() {
        const auto macros = K4MacroBackup::parseJson(kBackupJson);
        QVERIFY(!macros.contains(MacroIds::FnF1));
        QVERIFY(!macros.contains(MacroIds::PF1));
    }

    // An empty label with a real command is still a macro.
    void keepsCommandWithEmptyLabel() {
        const auto macros = K4MacroBackup::parseJson(kBackupJson);
        QVERIFY(macros.contains(MacroIds::Kpod8H));
        QCOMPARE(macros.value(MacroIds::Kpod8H).label, QString());
        QCOMPARE(macros.value(MacroIds::Kpod8H).command, QString("RC;"));
    }

    // Commands are imported verbatim — no rewriting of K4 editor syntax.
    void preservesCommandVerbatim() {
        const auto macros = K4MacroBackup::parseJson(kBackupJson);
        QCOMPARE(macros.value(MacroIds::Kpod8T).command, QString("SB0;AG/;#FXT$2;#SP60;AB4;VX0;"));
    }

    void skipsUnknownSlots() {
        const auto macros = K4MacroBackup::parseJson(kBackupJson);
        QCOMPARE(macros.size(), 5);
    }

    void reportsMalformedJson() {
        QString error;
        const auto macros = K4MacroBackup::parseJson("{ not json", &error);
        QVERIFY(macros.isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void functionIdIsSetOnEntry() {
        const auto macros = K4MacroBackup::parseJson(kBackupJson);
        QCOMPARE(macros.value(MacroIds::Kpod1T).functionId, MacroIds::Kpod1T);
    }
};

QTEST_MAIN(TestK4MacroBackup)
#include "test_k4macrobackup.moc"
