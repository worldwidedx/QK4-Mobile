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

    void releasedSamePaddleTapDoesNotRepeat() {
        IambicKeyer keyer;
        keyer.setSpeed(40);
        QSignalSpy elements(&keyer, &IambicKeyer::elementStarted);

        const qint64 firstPress = monotonicNs() - 30'000'000;
        keyer.setMidiDitPaddle(true, firstPress);
        QCOMPARE(elements.size(), 1);

        // A release and second brief press within this dit can be contact
        // bounce or a tap too early to form another element. Only the
        // opposite paddle is remembered across an element boundary.
        keyer.setMidiDitPaddle(false, firstPress + 8'000'000);
        keyer.setMidiDitPaddle(true, firstPress + 13'000'000);
        keyer.setMidiDitPaddle(false, firstPress + 18'000'000);

        QTest::qWait(180);
        QCOMPARE(elements.size(), 1);
    }

    void tinyMidiTouchTapCanEndJustPastRepeatBoundary_data() {
        QTest::addColumn<int>("wpm");
        QTest::addColumn<int>("pressMs");
        QTest::newRow("20-wpm-measured-tap") << 20 << 135;
        QTest::newRow("30-wpm-late-tap") << 30 << 95;
    }

    void tinyMidiTouchTapCanEndJustPastRepeatBoundary() {
        QFETCH(int, wpm);
        QFETCH(int, pressMs);
        IambicKeyer keyer;
        keyer.setSpeed(wpm);
        keyer.setMidiTouchLookahead(40);
        QSignalSpy elements(&keyer, &IambicKeyer::elementStarted);

        const qint64 pressed = monotonicNs();
        keyer.setMidiDitPaddle(true, pressed);
        // Model the 10 ms release debounce as well as the observed touch.
        QTimer::singleShot(pressMs + 10, &keyer, [&keyer, pressed, pressMs]() {
            keyer.setMidiDitPaddle(false, pressed + qint64(pressMs) * 1000000);
        });
        QTest::qWait(15);
        QCOMPARE(elements.size(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(elements.size(), 1, 100);

        // Both taps cross the ordinary repeat boundary. Lookahead lets their
        // releases arrive before a second element sounds.
        QTest::qWait(200);
        QCOMPARE(elements.size(), 1);
    }

    void tinyMidiTouchHoldRepeatsAtSelectedSpeed() {
        IambicKeyer keyer;
        keyer.setSpeed(30);
        keyer.setMidiTouchLookahead(40);
        QSignalSpy elements(&keyer, &IambicKeyer::elementStarted);
        QVector<qint64> starts;
        connect(&keyer, &IambicKeyer::elementStarted, &keyer,
                [&starts](bool) { starts.append(monotonicNs()); });

        keyer.setMidiDitPaddle(true, monotonicNs());
        QTimer::singleShot(230, &keyer, [&keyer]() {
            keyer.setMidiDitPaddle(false, monotonicNs());
        });
        QTest::qWait(340);
        QCOMPARE(elements.size(), 3);
        QCOMPARE(starts.size(), 3);
        for (int i = 1; i < starts.size(); ++i) {
            const qint64 intervalMs = (starts[i] - starts[i - 1]) / 1000000;
            QVERIFY2(intervalMs >= 65 && intervalMs <= 100,
                     "A held 30-WPM DIT must keep an approximately 80-ms cadence");
        }
    }

    void tinyMidiTouchAssistDoesNotDelayCtr2() {
        IambicKeyer keyer;
        keyer.setMidiTouchLookahead(40);
        QSignalSpy elements(&keyer, &IambicKeyer::elementStarted);
        keyer.setCtr2DitPaddle(true, monotonicNs());
        QCOMPARE(elements.size(), 1);
        keyer.setCtr2DitPaddle(false, monotonicNs());
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
