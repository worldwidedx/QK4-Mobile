#include <QtTest>
#include <QSettings>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <cmath>
#include "audio/digitaltxguard.h"
#include "network/tcpclient.h"

using Action = DigitalTxGuard::Action;
using Mode = DigitalTxGuard::Mode;

// Actual TCP/Protocol pipeline; the guard runs independently of the test/UI
// event loop. No physical radio, RF, Android microphone, or station profile.
class TestRadio : public QObject {
public:
    QTcpServer server;
    QThread io;
    TcpClient *client = new TcpClient;
    QTcpSocket *peer = nullptr;
    Protocol parser;
    QByteArray received;
    bool automatic = false, testMode = false, keyedWithoutTest = false;
    bool restoreAcknowledged = true, restoring = false;
    bool responsiveAlc = false;
    quint64 streamGeneration = 0;
    QTimer toneTimer;
    QStringList commands;
    TestRadio() {
        toneTimer.setInterval(20);
        connect(&toneTimer, &QTimer::timeout, this, [this] {
            if (streamGeneration)
                client->sendDigitalAudio(Protocol::buildAudioPacket(QByteArray(960, '\0'), 0, 1),
                                         240, 0, 0, streamGeneration);
        });
        client->moveToThread(&io);
        io.start();
        connect(&server, &QTcpServer::newConnection, this, [this] {
            peer = server.nextPendingConnection();
            connect(peer, &QTcpSocket::readyRead, this, [this] {
                const auto bytes = peer->readAll();
                received += bytes;
                parser.parse(bytes);
            });
            peer->write(Protocol::buildCATPacket("FA14074000;"));
        });
        connect(&parser, &Protocol::catResponseReceived, this, [this](const QString &text) {
            for (auto command : text.split(';', Qt::SkipEmptyParts)) {
                commands << command;
                if (command == "TX" && !testMode) keyedWithoutTest = true;
                if (command == "TX") toneTimer.start();
                if (command == "RX") toneTimer.stop();
                if (!automatic) continue;
                if (command == "TS1") testMode = true;
                if (command == "TS0") { testMode = false; restoring = true; }
                if (command == "TS" && (!restoring || restoreAcknowledged)) send(testMode ? "TS1;" : "TS0;");
                if (command == "TM") send(!testMode ? "TM000000000010;"
                    : responsiveAlc && client->digitalTxControl()->gain.load() > 0.008f
                        ? "TM007000000010;" : "TM003000000010;");
            }
        });
    }
    ~TestRadio() override {
        QMetaObject::invokeMethod(client, [this] { client->disconnectFromHost(); delete client; }, Qt::BlockingQueuedConnection);
        io.quit();
        io.wait();
    }
    bool connectRadio() {
        if (!server.listen(QHostAddress::LocalHost)) return false;
        const quint16 port = server.serverPort();
        QMetaObject::invokeMethod(client, [this, port] { client->connectToHost("127.0.0.1", port, "test"); });
        return true;
    }
    void send(const QString &command) { if (peer) { peer->write(Protocol::buildCATPacket(command)); peer->flush(); } }
};

class DigitalTxTest : public QObject {
    Q_OBJECT
private slots:
    void timedAudioWatchdogWithFreshMeters() {
        auto control = std::make_shared<DigitalTxControl>();
        DigitalTxGuard guard(control);
        control->scheduledGeneration = 499;
        QVERIFY(guard.begin(Mode::Ft8, 499, 0));
        guard.audioAccepted(600);
        QCOMPARE(guard.meter("TM005000025010", 850), Action::None);
        QCOMPARE(guard.tick(999), Action::None);
        QCOMPARE(guard.tick(1000), Action::Tripped);
        QVERIFY(!control->allows(499));
        QVERIFY(guard.reason().contains("not streaming"));
    }
    void scheduledSocketOwnership() {
        TestRadio radio;
        QVERIFY(radio.connectRadio());
        QTRY_VERIFY(radio.client->isConnected());
        auto control = radio.client->digitalTxControl();
        QSignalSpy stopped(radio.client, &TcpClient::scheduledDigitalAudioStopped);
        QSignalSpy keyed(radio.client, &TcpClient::digitalAudioKeyRequested);
        radio.client->beginScheduledDigitalAudio(int(Mode::Ft8), 500); // Revoked before it reaches I/O.
        radio.client->stopScheduledDigitalAudio(500);
        QTRY_COMPARE(stopped.count(), 1);
        QCOMPARE(keyed.count(), 0);
        QVERIFY(!radio.commands.contains("TX"));
        QVERIFY(!radio.commands.contains("RX"));
        control->scheduledGeneration = 501;
        radio.client->beginScheduledDigitalAudio(int(Mode::Ft8), 501);
        QTRY_COMPARE(keyed.count(), 1);
        QVERIFY(control->allows(501));
        radio.client->stopScheduledDigitalAudio(500); // Does not unkey the current owner.
        QTRY_COMPARE(stopped.count(), 2);
        QVERIFY(control->allows(501));
        radio.client->stopScheduledDigitalAudio(501);
        QTRY_COMPARE(stopped.count(), 3);
        QVERIFY(!control->allows(501));
        QTRY_VERIFY(radio.commands.contains("RX"));
        QSignalSpy accepted(radio.client, &TcpClient::sstvAudioAccepted);
        radio.client->sendDigitalAudio("stale-ft-wave", 240, 240, 240, 501);
        QTest::qWait(100);
        QCOMPARE(accepted.count(), 0);
        QVERIFY(!radio.received.contains("stale-ft-wave"));
    }
    void modes_data() {
        QTest::addColumn<int>("mode");
        QTest::newRow("FT8") << int(Mode::Ft8);
        QTest::newRow("FT4") << int(Mode::Ft4);
        QTest::newRow("SSTV") << int(Mode::Sstv);
    }
    void modes() {
        QFETCH(int, mode);
        auto control = std::make_shared<DigitalTxControl>();
        DigitalTxGuard guard(control);
        QVERIFY(guard.begin(Mode(mode), 1, 0));
        QCOMPARE(guard.meter("TM004000025010", 100), Action::None);
        const float original = control->gain;
        QCOMPARE(guard.meter("TM005000025010", 150), Action::None);
        QCOMPARE(control->gain.load(), original);
        QCOMPARE(guard.meter("TM006000025010", 200), Action::Reduced);
        QVERIFY(control->gain < original);
        const float reduced = control->gain;
        QCOMPARE(guard.meter("TM006000025010", 250), Action::None); // Allow the reduction to reach the K4.
        QCOMPARE(control->gain.load(), reduced);
        QCOMPARE(guard.meter("TM003000025010", 600), Action::None);
        QCOMPARE(control->gain.load(), reduced); // No automatic upward pumping.
        guard.stop();
        QVERIFY(guard.begin(Mode(mode), 2, 700));
        QCOMPARE(control->gain.load(), reduced); // A retry cannot restore excessive gain.
        QCOMPARE(guard.meter("TM010000025010", 750), Action::Tripped);
        QVERIFY(!control->allows(2));
        QVERIFY(!guard.begin(Mode(mode), 3, 800)); // Latch blocks automatic retries.
        guard.acknowledge();
        QVERIFY(guard.begin(Mode(mode), 3, 800));
    }
    void sustainedHighAndFeedback() {
        auto c = std::make_shared<DigitalTxControl>();
        DigitalTxGuard g(c);
        QVERIFY(g.begin(Mode::Sstv, 10, 0));
        for (int now = 100; now <= 1300; now += 400)
            QCOMPARE(g.meter("TM006000025010", now), Action::Reduced);
        QCOMPARE(g.tick(1600), Action::Tripped);
        QVERIFY(g.reason().contains("stayed high"));
        g.acknowledge();
        QVERIFY(g.begin(Mode::Ft8, 11, 2000));
        QCOMPARE(g.meter("TM1", 3000), Action::None);
        QCOMPARE(g.tick(3500), Action::Tripped); // An enable echo cannot renew feedback.
        g.acknowledge();
        QVERIFY(g.begin(Mode::Ft4, 12, 4000));
        for (int t = 4200; t < 8000; t += 250)
            QCOMPARE(g.meter("TM003000025010", t), Action::None); // Identical fresh samples remain valid.
        QCOMPARE(g.meter("TM003000025010", 9600), Action::Tripped); // Late feedback cannot revive TX.
    }
    void malformedAndCompression() {
        for (const auto &value : {"TM003001025010", "TM-03000025010", "TM00300002501", "TM?"}) {
            auto c = std::make_shared<DigitalTxControl>();
            DigitalTxGuard g(c);
            QVERIFY(g.begin(Mode::Ft8, 1, 0));
            QCOMPARE(g.meter(value, 100), Action::Tripped);
            QVERIFY(!c->allows(1));
        }
    }
    void audioHeadroomAndGeneration() {
        auto c = std::make_shared<DigitalTxControl>();
        DigitalTxGuard g(c);
        QVERIFY(g.begin(Mode::Sstv, 20, 0));
        DigitalTxAudio audio;
        QVector<qint16> tone(1440);
        for (int i = 0; i < tone.size(); ++i) tone[i] = qRound(26213 * std::sin(2 * M_PI * 1500 * i / 12000.0));
        QVERIFY(audio.process(tone, *c, 20));
        for (auto s : tone) QVERIFY(std::abs(int(s)) <= 13107);
        QCOMPARE(g.meter("TM006000025010", 100), Action::Reduced);
        tone.fill(20000);
        QVERIFY(audio.process(tone, *c, 20));
        QVERIFY(tone[0] > tone[59]);
        QCOMPARE(tone[59], tone[60]); // Five-ms ramp is complete, with no packet-boundary step.
        tone.fill(32767);
        QVERIFY(!audio.process(tone, *c, 20));
        QCOMPARE(g.tick(150), Action::Tripped);
        g.acknowledge();
        QVERIFY(g.begin(Mode::Ft8, 21, 200));
        c->close(20);
        QVERIFY(c->allows(21)); // Old STOP cannot close a later lease.
        QVERIFY(!audio.process(tone, *c, 20));
        QCOMPARE(g.tick(250), Action::None);
    }
    void calibrationAndStorage() {
        auto c = std::make_shared<DigitalTxControl>();
        DigitalTxGuard g(c);
        QVERIFY(g.begin(Mode::Ft8, 1, 0, true));
        QCOMPARE(c->gain.load(), 0.03125f);
        g.audioAccepted(0);
        g.audioAccepted(600);
        QCOMPARE(g.meter("TM000000000010", 600), Action::None);
        QVERIFY(c->gain > 0.03125f);
        for (int now = 800; now < 2400; now += 200) {
            g.audioAccepted(now);
            QCOMPARE(g.meter("TM003000000010", now), Action::None);
        }
        g.audioAccepted(2400);
        QCOMPARE(g.meter("TM003000000010", 2400), Action::Calibrated);
        QTemporaryDir dir;
        const auto file = dir.filePath("calibration.ini");
        QSettings s(file, QSettings::IniFormat);
        QVERIFY(!DigitalTxCalibration::load(s, "radio1/FT8/20m/input1"));
        QVERIFY(DigitalTxCalibration::save(s, "radio1/FT8/20m/input1", c->gain));
        QVERIFY(!DigitalTxCalibration::save(s, "radio1/FT8/20m/input1", NAN));
        QSettings reopened(file, QSettings::IniFormat);
        QCOMPARE(*DigitalTxCalibration::load(reopened, "radio1/FT8/20m/input1"), c->gain.load());
        QVERIFY(!DigitalTxCalibration::load(reopened, "radio1/FT8/20m/input2"));
        QVERIFY(!DigitalTxCalibration::load(reopened, "radio2/SSTV/20m/input1"));
    }
    void calibrationFollowsRadioAudioSetup() {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("levels.ini"), QSettings::IniFormat);
        QJsonObject original{{"profile", "radio-one"}, {"port", 9204}, {"digitalMode", 0},
            {"band", "20m"}, {"radioMode", "DATA"}, {"codec", 3}, {"latency", 0},
            {"tone", 1500}, {"power", 20}, {"micGain", 40}, {"compression", 0},
            {"lineSource", 1}, {"lineUsb", 25}, {"lineJack", 30}};
        const auto json = [](const QJsonObject &object) {
            return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
        };
        // Upgrade a real v1 record, whose context was only stored as a hash.
        const auto legacyKey = "digitalTx/calibration/" + QString::fromLatin1(
            QCryptographicHash::hash(json(original).toUtf8(), QCryptographicHash::Sha256).toHex());
        settings.setValue(legacyKey, QVariantMap{{"version", 1}, {"gain", 0.011f}});
        auto selected = original;
        selected["tone"] = 2347;
        const auto migrated = DigitalTxCalibration::load(settings, json(selected));
        QVERIFY(migrated);
        QCOMPARE(*migrated, 0.011f);
        selected["tone"] = 823;
        selected["power"] = 75;
        selected["band"] = "40m";
        selected["latency"] = 2;
        selected["firmware"] = QJsonObject{{"MCU", "readback"}};
        QCOMPARE(DigitalTxCalibration::matchingContext(json(selected)),
                 DigitalTxCalibration::matchingContext(json(original)));
        QSettings reopened(settings.fileName(), QSettings::IniFormat);
        const auto reused = DigitalTxCalibration::load(reopened, json(selected));
        QVERIFY(reused);
        QCOMPARE(*reused, 0.011f);
        for (const auto *field : {"profile", "port", "digitalMode", "codec", "radioMode",
                                  "lineSource", "lineUsb", "lineJack", "micGain", "compression"}) {
            auto different = selected;
            different[QLatin1String(field)] = "changed";
            QVERIFY(!DigitalTxCalibration::load(reopened, json(different)));
        }
    }
    void ft8AndFt4ShareCalibration_data() {
        QTest::addColumn<int>("savedMode");
        QTest::addColumn<bool>("legacyFull");
        QTest::newRow("ft8-normalized") << 0 << false;
        QTest::newRow("ft4-normalized") << 1 << false;
        QTest::newRow("ft8-original") << 0 << true;
        QTest::newRow("ft4-original") << 1 << true;
    }
    void ft8AndFt4ShareCalibration() {
        QFETCH(int, savedMode);
        QFETCH(bool, legacyFull);
        QTemporaryDir dir;
        QSettings settings(dir.filePath("shared.ini"), QSettings::IniFormat);
        QJsonObject source{{"profile", "radio"}, {"port", 9204}, {"digitalMode", savedMode},
                           {"radioMode", "DATA"}, {"codec", 3}, {"lineSource", 1}};
        const auto json = [](const QJsonObject &o) {
            return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
        };
        const auto legacySave = [&](QJsonObject o, float gain, const QString &utc) {
            const auto key = "digitalTx/calibration/" + QString::fromLatin1(
                QCryptographicHash::hash(json(o).toUtf8(), QCryptographicHash::Sha256).toHex());
            settings.setValue(key, QVariantMap{{"version", 1}, {"gain", gain}, {"utc", utc}});
        };
        if (legacyFull) source["tone"] = 1500;
        legacySave(source, 0.011f, "2026-09-10T21:05:00Z");
        auto other = source;
        other["digitalMode"] = 1 - savedMode;
        if (legacyFull) other["tone"] = 2100;
        legacySave(other, 0.02f, "2026-09-09T20:00:00Z");
        auto requested = other;
        requested["tone"] = 2500;
        const auto gain = DigitalTxCalibration::load(settings, json(requested));
        QVERIFY(gain);
        QCOMPARE(*gain, 0.011f); // Newest completed calibration from either mode.
        QCOMPARE(DigitalTxCalibration::matchingContext(json(source)),
                 DigitalTxCalibration::matchingContext(json(requested)));
        QVERIFY(DigitalTxCalibration::save(settings, json(requested), 0.008f));
        QSettings reopened(settings.fileName(), QSettings::IniFormat);
        const auto shared = DigitalTxCalibration::load(reopened, json(source));
        QVERIFY(shared);
        QCOMPARE(*shared, 0.008f);
        auto sstv = source;
        sstv["digitalMode"] = 2;
        QVERIFY(!DigitalTxCalibration::load(reopened, json(sstv)));
        auto otherRadio = source;
        otherRadio["profile"] = "different radio";
        QVERIFY(!DigitalTxCalibration::load(reopened, json(otherRadio)));
    }
    void calibrationRequiresStreamingAndNoRf() {
        auto c = std::make_shared<DigitalTxControl>();
        DigitalTxGuard g(c);
        QVERIFY(g.begin(Mode::Ft4, 1, 0, true));
        for (int t = 100; t < 1500; t += 100)
            QCOMPARE(g.meter("TM003000000010", t), Action::None);
        QCOMPARE(c->gain.load(), 0.03125f);
        QCOMPARE(g.tick(1500), Action::Tripped);
        QVERIFY(g.reason().contains("not streaming"));
        g.acknowledge();
        QVERIFY(g.begin(Mode::Sstv, 2, 2000, true));
        g.audioAccepted(2000);
        QCOMPARE(g.meter("TM003000001010", 2100), Action::Tripped);
        QVERIFY(g.reason().contains("RF output"));
    }
    void calibrationModerateStartupAndMinimumDrive() {
        auto c = std::make_shared<DigitalTxControl>();
        DigitalTxGuard g(c);
        QVERIFY(g.begin(Mode::Ft8, 1, 0, true));
        c->gain = DigitalTxGuard::MinimumGain;
        QCOMPARE(g.meter("TM007000000010", 50), Action::None); // Before tone.
        g.audioAccepted(100);
        QCOMPARE(g.meter("TM007000000010", 200), Action::None); // Startup transient.
        g.audioAccepted(700);
        QCOMPARE(g.meter("TM007000000010", 700), Action::None);
        QCOMPARE(g.meter("TM007000000010", 701), Action::None); // Burst isn't extra evidence.
        g.audioAccepted(950);
        QCOMPARE(g.meter("TM003000000010", 950), Action::None); // Recovery resets high history.
        for (int t : {1200, 1450, 1700}) {
            g.audioAccepted(t);
            QCOMPARE(g.meter("TM007000000010", t), Action::None);
        }
        g.audioAccepted(1850);
        QCOMPARE(g.meter("TM007000000010", 1850), Action::Tripped);
        QVERIFY(g.reason().contains("repeated readings"));
        QVERIFY(g.reason().contains("TM007000000010;"));
        QVERIFY(!c->allows(1));
    }
    void singleHighReadingDoesNotBecomeSustained() {
        auto c = std::make_shared<DigitalTxControl>();
        DigitalTxGuard g(c);
        QVERIFY(g.begin(Mode::Sstv, 1, 0));
        QCOMPARE(g.meter("TM006000025010", 100), Action::Reduced);
        QCOMPARE(g.tick(1599), Action::None);
        QCOMPARE(g.tick(1600), Action::Tripped);
        QVERIFY(g.reason().contains("unavailable")); // Missing feedback, not fabricated sustained ALC.
    }
    void lowerCalibrationPersistsAndScalesAudio() {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("lower.ini"), QSettings::IniFormat);
        const float lowerGain = 1.0f / 128.0f;
        QVERIFY(DigitalTxCalibration::save(settings, "lower-drive", lowerGain));
        QSettings reopened(dir.filePath("lower.ini"), QSettings::IniFormat);
        QCOMPARE(*DigitalTxCalibration::load(reopened, "lower-drive"), lowerGain);
        QVERIFY(DigitalTxCalibration::save(settings, "lowest", DigitalTxGuard::MinimumGain));
        QVERIFY(!DigitalTxCalibration::save(settings, "zero", 0));
        QVERIFY(!DigitalTxCalibration::save(settings, "sub-resolution", DigitalTxGuard::MinimumGain / 2));
        auto c = std::make_shared<DigitalTxControl>();
        c->gain = *DigitalTxCalibration::load(reopened, "lower-drive");
        DigitalTxGuard g(c);
        QVERIFY(g.begin(Mode::Sstv, 1, 0));
        QCOMPARE(c->gain.load(), lowerGain); // Starting a TX must not raise it back to 1/32.
        DigitalTxAudio audio;
        audio.reset(lowerGain);
        QVector<qint16> pcm(240, 26213);
        QVERIFY(audio.process(pcm, *c, 1));
        for (auto sample : pcm) QCOMPARE(sample, qint16(205));
        QCOMPARE(g.meter("TM007000025010", 100), Action::Reduced);
        QVERIFY(c->gain.load() < lowerGain);
    }
    void calibrationAcceptsRawFiveWithoutHunting() {
        for (Mode mode : {Mode::Ft8, Mode::Ft4, Mode::Sstv}) {
            auto c = std::make_shared<DigitalTxControl>();
            DigitalTxGuard g(c);
            QVERIFY(g.begin(mode, 1, 0, true));
            g.audioAccepted(0);
            for (int t : {600, 1200, 1800}) {
                g.audioAccepted(t);
                QCOMPARE(g.meter(t == 600 ? "TM007000000010" : "TM006000000010", t), Action::Reduced);
            }
            const float acceptedGain = c->gain.load();
            for (int t = 2000; t < 3600; t += 200) {
                g.audioAccepted(t);
                QCOMPARE(g.meter("TM005000000010", t), Action::None);
                QCOMPARE(c->gain.load(), acceptedGain); // Five must not trigger another reduction.
            }
            g.audioAccepted(3600);
            QCOMPARE(g.meter("TM005000000010", 3600), Action::Calibrated);
            QCOMPARE(c->gain.load(), acceptedGain);
        }
    }
    void calibrationSearchRemainsBounded() {
        auto c = std::make_shared<DigitalTxControl>();
        DigitalTxGuard g(c);
        QVERIFY(g.begin(Mode::Ft4, 1, 0, true));
        g.audioAccepted(0);
        Action action = Action::None;
        // Slow fresh metering must still permit successive reductions.
        for (int t = 600; t <= 15000 && g.active(); t += 600) {
            g.audioAccepted(t);
            action = g.meter("TM007000000010", t);
        }
        QCOMPARE(action, Action::Tripped);
        QVERIFY(c->gain.load() < 0.0001f);
        QVERIFY(c->gain.load() >= DigitalTxGuard::MinimumGain);
        QVERIFY(!c->allows(1));
    }
    void emergencyChecksDuringCalibrationSettling() {
        for (const auto &reading : {"TM010000000010", "TM003001000010", "TM003000001010"}) {
            auto c = std::make_shared<DigitalTxControl>();
            DigitalTxGuard g(c);
            QVERIFY(g.begin(Mode::Ft4, 1, 0, true));
            QCOMPARE(g.meter(reading, 50), Action::Tripped);
            QVERIFY(!c->allows(1));
            QVERIFY(g.reason().contains(reading));
        }
    }
    void socketWatchdogWhileUiBlocked() {
        TestRadio radio;
        QVERIFY(radio.connectRadio());
        QTRY_VERIFY(radio.client->isConnected());
        QSignalSpy failed(radio.client, &TcpClient::digitalAudioTransmitFailed);
        radio.client->digitalTxControl()->scheduledGeneration = 100;
        radio.client->beginDigitalAudioTransmit(int(Mode::Ft8), 100);
        QTRY_VERIFY(radio.client->digitalTxControl()->allows(100));
        radio.send("TM003000025010;");
        QTest::qWait(100);
        QThread::msleep(1700); // Deliberately prevent the UI/test event loop from running.
        QVERIFY(!radio.client->digitalTxControl()->allows(100));
        QTRY_VERIFY(!failed.isEmpty());
        QTRY_VERIFY(radio.received.contains("RX;"));
        QSignalSpy accepted(radio.client, &TcpClient::sstvAudioAccepted);
        radio.client->sendDigitalAudio("late-audio", 240, 480, 480, 100);
        QTest::qWait(100);
        QCOMPARE(accepted.count(), 0);
        QVERIFY(!radio.received.contains("late-audio"));
    }
    void calibrationTestModeHandshake() {
        TestRadio radio;
        radio.automatic = true;
        radio.streamGeneration = 200;
        QVERIFY(radio.connectRadio());
        QTRY_VERIFY(radio.client->isConnected());
        QSignalSpy completed(radio.client, &TcpClient::digitalCalibrationFinished);
        radio.client->beginDigitalCalibration(int(Mode::Sstv), 200);
        QTRY_VERIFY_WITH_TIMEOUT(!completed.isEmpty(), 5000);
        QVERIFY2(completed.first()[1].toBool(), qPrintable(completed.first()[3].toString()));
        QVERIFY(!radio.keyedWithoutTest);
        QVERIFY(!radio.testMode);
        QVERIFY(!radio.client->digitalTxControl()->allows(200));
        QVERIFY(radio.commands.indexOf("TS1") < radio.commands.indexOf("TX"));
        QVERIFY(radio.commands.indexOf("RX") < radio.commands.indexOf("TS0"));
    }
    void calibrationReducesBelowStartThroughSocket() {
        TestRadio radio;
        radio.automatic = radio.responsiveAlc = true;
        radio.streamGeneration = 201;
        QVERIFY(radio.connectRadio());
        QTRY_VERIFY(radio.client->isConnected());
        QSignalSpy completed(radio.client, &TcpClient::digitalCalibrationFinished);
        QSignalSpy reduced(radio.client, &TcpClient::digitalAudioDriveReduced);
        radio.client->beginDigitalCalibration(int(Mode::Ft8), 201);
        QTRY_VERIFY_WITH_TIMEOUT(!completed.isEmpty(), 8000);
        QVERIFY2(completed.first()[1].toBool(), qPrintable(completed.first()[3].toString()));
        QVERIFY(completed.first()[2].toFloat() < 0.008f);
        QVERIFY(reduced.count() >= 4);
        QVERIFY(!radio.keyedWithoutTest);
        QVERIFY(!radio.testMode);
        QVERIFY(!radio.client->digitalTxControl()->allows(201));
    }
    void calibrationCancellationAndExclusivity() {
        TestRadio radio;
        QVERIFY(radio.connectRadio());
        QTRY_VERIFY(radio.client->isConnected());
        QSignalSpy completed(radio.client, &TcpClient::digitalCalibrationFinished);
        QSignalSpy rejected(radio.client, &TcpClient::digitalAudioTransmitFailed);
        radio.client->beginDigitalCalibration(int(Mode::Ft8), 210);
        QTRY_VERIFY(radio.commands.contains("TS"));
        QVERIFY2(completed.isEmpty(), completed.isEmpty() ? "" : qPrintable(completed.first()[3].toString()));
        radio.client->beginDigitalAudioTransmit(int(Mode::Sstv), 211);
        QTRY_COMPARE(rejected.size(), 1);
        QVERIFY(!radio.commands.contains("TX"));
        radio.client->stopDigitalAudioAndUnkey(); // All STOP routes cancel pending calibration.
        QTRY_COMPARE(completed.size(), 1);
        QVERIFY(!completed.last()[1].toBool());
        radio.automatic = true;
        radio.streamGeneration = 212;
        radio.client->beginDigitalCalibration(int(Mode::Ft8), 212);
        QTRY_VERIFY(radio.client->digitalTxControl()->allows(212));
        radio.client->stopDigitalAudioAndUnkey();
        QTRY_COMPARE(completed.size(), 2);
        QVERIFY(!completed.last()[1].toBool());
        QVERIFY(!radio.testMode);
        QVERIFY(!radio.keyedWithoutTest);
        QVERIFY(!radio.client->digitalTxControl()->allows(212));
    }
    void calibrationDoesNotSaveWithoutRestoreAck() {
        TestRadio radio;
        radio.automatic = true;
        radio.restoreAcknowledged = false;
        radio.streamGeneration = 220;
        QVERIFY(radio.connectRadio());
        QTRY_VERIFY(radio.client->isConnected());
        QSignalSpy completed(radio.client, &TcpClient::digitalCalibrationFinished);
        radio.client->beginDigitalCalibration(int(Mode::Sstv), 220);
        QTRY_VERIFY_WITH_TIMEOUT(!completed.isEmpty(), 6000);
        QVERIFY(!completed.first()[1].toBool());
        QVERIFY(completed.first()[3].toString().contains("restoration"));
        QVERIFY(!radio.client->digitalTxControl()->allows(220));
    }
    void calibrationDisconnectFails() {
        TestRadio radio;
        radio.automatic = true;
        radio.streamGeneration = 230;
        QVERIFY(radio.connectRadio());
        QTRY_VERIFY(radio.client->isConnected());
        QSignalSpy completed(radio.client, &TcpClient::digitalCalibrationFinished);
        radio.client->beginDigitalCalibration(int(Mode::Ft4), 230);
        QTRY_VERIFY(radio.client->digitalTxControl()->allows(230));
        radio.peer->abort();
        QTRY_COMPARE(completed.size(), 1);
        QVERIFY(!completed.first()[1].toBool());
        QVERIFY(!radio.client->digitalTxControl()->allows(230));
    }
    void missingTestAckDoesNotKey() {
        TestRadio radio;
        QVERIFY(radio.connectRadio());
        QTRY_VERIFY(radio.client->isConnected());
        QSignalSpy completed(radio.client, &TcpClient::digitalCalibrationFinished);
        radio.client->beginDigitalCalibration(int(Mode::Ft4), 300);
        QTRY_VERIFY_WITH_TIMEOUT(!completed.isEmpty(), 4000);
        QVERIFY(!completed.first()[1].toBool());
        QVERIFY(!radio.commands.contains("TX"));
        QVERIFY(!radio.client->digitalTxControl()->allows(300));
    }
};
QTEST_GUILESS_MAIN(DigitalTxTest)
#include "test_digitaltx.moc"
