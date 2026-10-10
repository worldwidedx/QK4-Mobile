#include "hardware/iambickeyer.h"

#include <QSignalSpy>
#include <QtTest>

class TestIambicKeyer : public QObject {
    Q_OBJECT

private slots:
    void briefOppositePaddleTap_data() {
        QTest::addColumn<int>("wpm");
        QTest::newRow("30-wpm") << 30;
        QTest::newRow("40-wpm") << 40;
        QTest::newRow("50-wpm") << 50;
    }

    void briefOppositePaddleTap() {
        QFETCH(int, wpm);
        IambicKeyer keyer;
        keyer.setSpeed(wpm);
        QSignalSpy elements(&keyer, &IambicKeyer::elementStarted);

        // The opposite paddle is pressed and released before the current
        // element completes. Its latch must survive to the next boundary.
        keyer.setDitPaddle(true);
        QTRY_COMPARE_WITH_TIMEOUT(elements.size(), 1, 250);
        keyer.setDitPaddle(false);
        keyer.setDahPaddle(true);
        keyer.setDahPaddle(false);

        QTRY_COMPARE_WITH_TIMEOUT(elements.size(), 2, 350);
        QCOMPARE(elements.at(0).at(0).toBool(), true);
        QCOMPARE(elements.at(1).at(0).toBool(), false);
        QTest::qWait(1200 / wpm * 5);
        QCOMPARE(elements.size(), 2);
    }
};

QTEST_GUILESS_MAIN(TestIambicKeyer)
#include "test_iambickeyer.moc"
