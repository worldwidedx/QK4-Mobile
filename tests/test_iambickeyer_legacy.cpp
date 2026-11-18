#include "hardware/iambickeyer.h"

#include <QSignalSpy>
#include <QtTest>

class TestIambicKeyerLegacy : public QObject {
    Q_OBJECT

private slots:
    void briefOppositePaddlePressSurvives() {
        IambicKeyer keyer;
        keyer.setSpeed(30);
        QSignalSpy elements(&keyer, &IambicKeyer::elementStarted);

        keyer.setDitPaddle(true);
        QTRY_COMPARE_WITH_TIMEOUT(elements.size(), 1, 250);
        keyer.setDitPaddle(false);
        keyer.setDahPaddle(true);
        keyer.setDahPaddle(false);

        QTRY_COMPARE_WITH_TIMEOUT(elements.size(), 2, 350);
        QCOMPARE(elements.at(0).at(0).toBool(), true);
        QCOMPARE(elements.at(1).at(0).toBool(), false);
        QTest::qWait(150);
        QCOMPARE(elements.size(), 2);
    }

    void heldPaddleRepeatsAndReleaseStops() {
        IambicKeyer keyer;
        keyer.setSpeed(30);
        QSignalSpy elements(&keyer, &IambicKeyer::elementStarted);

        keyer.setDitPaddle(true);
        QTRY_VERIFY_WITH_TIMEOUT(elements.size() >= 3, 350);
        keyer.setDitPaddle(false);
        const int countAfterRelease = elements.size();
        QTest::qWait(150);
        QCOMPARE(elements.size(), countAfterRelease);
    }
};

QTEST_GUILESS_MAIN(TestIambicKeyerLegacy)
#include "test_iambickeyer_legacy.moc"
