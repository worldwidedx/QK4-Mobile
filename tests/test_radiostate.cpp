#include "models/radiostate.h"

#include <QSignalSpy>
#include <QtTest>

class RadioStateTest : public QObject {
    Q_OBJECT

private slots:
    void transmitQueryConfirmsState();
    void malformedTransmitQueryIsIgnored();
    void reverseDataSubModesUseDistinctDisplayLabels();
    void attenuatorLevelSettersClampAndNotify();
    void vfoTuningStepsRemainIndependent();
};

void RadioStateTest::transmitQueryConfirmsState() {
    RadioState state;
    QSignalSpy spy(&state, &RadioState::transmitStateChanged);

    state.parseCATCommand(QStringLiteral("TQ1;"));
    QVERIFY(state.isTransmitting());
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), true);

    // Repeated confirmation must not create a false state transition.
    state.parseCATCommand(QStringLiteral("TQ1;"));
    QCOMPARE(spy.size(), 0);

    state.parseCATCommand(QStringLiteral("TQ0;"));
    QVERIFY(!state.isTransmitting());
    QCOMPARE(spy.size(), 1);
    QCOMPARE(spy.takeFirst().at(0).toBool(), false);
}

void RadioStateTest::malformedTransmitQueryIsIgnored() {
    RadioState state;
    QSignalSpy spy(&state, &RadioState::transmitStateChanged);

    state.parseCATCommand(QStringLiteral("TQ;"));
    state.parseCATCommand(QStringLiteral("TQX;"));
    state.parseCATCommand(QStringLiteral("TQ2;"));
    QVERIFY(!state.isTransmitting());
    QCOMPARE(spy.size(), 0);
}

void RadioStateTest::reverseDataSubModesUseDistinctDisplayLabels() {
    RadioState state;

    state.parseCATCommand(QStringLiteral("MD6;"));
    state.parseCATCommand(QStringLiteral("DT0;"));
    QCOMPARE(state.modeStringFull(), QStringLiteral("DATA"));

    state.parseCATCommand(QStringLiteral("MD9;"));
    QCOMPARE(state.modeStringFull(), QStringLiteral("DATA-R"));
    state.parseCATCommand(QStringLiteral("DT1;"));
    QCOMPARE(state.modeStringFull(), QStringLiteral("AFSK-R"));
    state.parseCATCommand(QStringLiteral("DT2;"));
    QCOMPARE(state.modeStringFull(), QStringLiteral("FSK-R"));
    state.parseCATCommand(QStringLiteral("DT3;"));
    QCOMPARE(state.modeStringFull(), QStringLiteral("PSK-R"));

    state.parseCATCommand(QStringLiteral("MD$9;"));
    state.parseCATCommand(QStringLiteral("DT$1;"));
    QCOMPARE(state.modeStringFullB(), QStringLiteral("AFSK-R"));
    state.parseCATCommand(QStringLiteral("MD$6;"));
    QCOMPARE(state.modeStringFullB(), QStringLiteral("AFSK"));
}

void RadioStateTest::attenuatorLevelSettersClampAndNotify() {
    RadioState state;
    QSignalSpy mainSpy(&state, &RadioState::processingChanged);
    QSignalSpy subSpy(&state, &RadioState::processingChangedB);

    state.setAttenuatorLevel(9);
    QCOMPARE(state.attenuatorLevel(), 9);
    QCOMPARE(mainSpy.count(), 1);
    state.setAttenuatorLevel(99);
    QCOMPARE(state.attenuatorLevel(), 21);
    QCOMPARE(mainSpy.count(), 2);

    state.setAttenuatorLevelB(-4);
    QCOMPARE(state.attenuatorLevelB(), 0);
    QCOMPARE(subSpy.count(), 0); // The default is already zero.
    state.setAttenuatorLevelB(6);
    QCOMPARE(state.attenuatorLevelB(), 6);
    QCOMPARE(subSpy.count(), 1);
}

void RadioStateTest::vfoTuningStepsRemainIndependent() {
    RadioState state;
    QSignalSpy mainSpy(&state, &RadioState::tuningStepChanged);
    QSignalSpy subSpy(&state, &RadioState::tuningStepBChanged);

    state.parseCATCommand(QStringLiteral("VT2;"));
    state.parseCATCommand(QStringLiteral("VT$3;"));
    QCOMPARE(state.tuningStep(), 2);
    QCOMPARE(state.tuningStepB(), 3);
    QCOMPARE(mainSpy.count(), 1);
    QCOMPARE(subSpy.count(), 1);

    // Current K4 responses append the mode after the step digit.
    state.parseCATCommand(QStringLiteral("VT$34;"));
    QCOMPARE(state.tuningStepB(), 3);
    QCOMPARE(subSpy.count(), 1);
}

QTEST_MAIN(RadioStateTest)
#include "test_radiostate.moc"
