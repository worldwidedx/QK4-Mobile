#include "hardware/iambickeyer.h"

#include <QSignalSpy>
#include <QThread>
#include <QtTest>
#include <chrono>

namespace {
qint64 monotonicNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

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

    void delayedMidiBatchKeepsNextCharacterSeparate() {
        IambicKeyer keyer;
        keyer.setSpeed(30);
        QSignalSpy elements(&keyer, &IambicKeyer::elementStarted);
        QSignalSpy spaces(&keyer, &IambicKeyer::characterSpace);
        const qint64 firstPress = monotonicNs();
        keyer.setMidiDitPaddle(true, firstPress);
        QCOMPARE(elements.size(), 1);

        // Keep the event loop from servicing the 80-ms dit boundary. Deliver
        // the release and the next character's press in one late batch.
        QThread::msleep(150);
        keyer.setMidiDitPaddle(false, firstPress + 20'000'000);
        keyer.setMidiDahPaddle(true, firstPress + 110'000'000);
        keyer.setMidiDahPaddle(false, firstPress + 130'000'000);
        QTRY_COMPARE_WITH_TIMEOUT(spaces.size(), 1, 150);
        QTRY_COMPARE_WITH_TIMEOUT(elements.size(), 2, 150);
        QCOMPARE(elements.at(0).at(0).toBool(), true);
        QCOMPARE(elements.at(1).at(0).toBool(), false);
    }

    void oldBatchArrivingWhileIdleReplaysLetters() {
        IambicKeyer keyer;
        keyer.setSpeed(30);
        QSignalSpy elements(&keyer, &IambicKeyer::elementStarted);
        QSignalSpy spaces(&keyer, &IambicKeyer::characterSpace);
        const qint64 firstPress = monotonicNs() - 600'000'000;
        for (int i = 0; i < 3; ++i) {
            keyer.setMidiDitPaddle(true, firstPress + i * 80'000'000);
            keyer.setMidiDitPaddle(false, firstPress + i * 80'000'000 + 20'000'000);
        }
        keyer.setMidiDahPaddle(true, firstPress + 400'000'000);
        keyer.setMidiDahPaddle(false, firstPress + 500'000'000);

        QTRY_COMPARE_WITH_TIMEOUT(elements.size(), 4, 1000);
        QCOMPARE(elements.at(0).at(0).toBool(), true);
        QCOMPARE(elements.at(1).at(0).toBool(), true);
        QCOMPARE(elements.at(2).at(0).toBool(), true);
        QCOMPARE(elements.at(3).at(0).toBool(), false);
        QVERIFY(spaces.size() >= 1);
    }

    void missedDeadlineDoesNotBurstElements() {
        IambicKeyer keyer;
        keyer.setSpeed(30);
        QSignalSpy elements(&keyer, &IambicKeyer::elementStarted);
        keyer.setDitPaddle(true);
        QCOMPARE(elements.size(), 1);
        QThread::msleep(260);
        QTRY_COMPARE_WITH_TIMEOUT(elements.size(), 2, 100);
        QTest::qWait(25);
        QCOMPARE(elements.size(), 2);
        keyer.setDitPaddle(false);
    }

    void oneMidiDeviceCannotReleaseTheOther() {
        IambicKeyer keyer;
        keyer.setSpeed(50);
        QSignalSpy elements(&keyer, &IambicKeyer::elementStarted);
        int emitted = 0;
        connect(&keyer, &IambicKeyer::elementStarted, &keyer, [&](bool) {
            if (++emitted == 2)
                keyer.setCtr2DitPaddle(false, monotonicNs());
        });
        const qint64 started = monotonicNs();
        keyer.setMidiDitPaddle(true, started);
        keyer.setCtr2DitPaddle(true, started + 1'000'000);
        keyer.setMidiDitPaddle(false, started + 2'000'000);
        QTRY_COMPARE_WITH_TIMEOUT(elements.size(), 2, 180);
        QCOMPARE(elements.at(1).at(0).toBool(), true);
        QTest::qWait(150);
        QCOMPARE(elements.size(), 2);
    }
};

QTEST_GUILESS_MAIN(TestIambicKeyer)
#include "test_iambickeyer.moc"
