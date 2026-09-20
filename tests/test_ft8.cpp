#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QFontDatabase>
#include <QTimeZone>
#include <QCheckBox>
#include <QTimer>
#include <QPainter>
#include <QProcess>
#include <QDataStream>
#include <QScrollBar>
#include <QSlider>
#include <QStyleOptionSlider>
#include <random>
#include <cmath>
#include "ft8/ft8session.h"
#include "ft8/ft8receiver.h"
#include "ft8/ft8transmitter.h"
#include "ft8/ft8radiomode.h"
#include "ft8/ft8snr.h"
#include "ft8/ft8logbook.h"
#include "ui/ft8waterfall.h"
#include "ui/ft8screen.h"
#include "ui/fnpopupwidget.h"
#include "ui/k4styles.h"
#include "ui/inwindowdialog.h"
#include "dsp/panadapter_rhi.h"
#include "dsp/minipan_rhi.h"
#include "ui/ft8activitystyle.h"
#include "ui/frequencydisplaywidget.h"
#include "hardware/midiinputrouter.h"
extern "C" {
#include <ft8/encode.h>
#include <ft8/message.h>
#include <ft8/constants.h>
}

class Ft8Test : public QObject {
    Q_OBJECT
    // Read the application's GPU texture and paint its actual widget overlays.
    // This works even when the build process has no visible desktop window.
    QImage capture(QWidget &root) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        const qreal dpr = root.devicePixelRatioF();
        QImage image(root.size() * dpr, QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(dpr);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        std::function<void(QWidget *, QPoint)> draw = [&](QWidget *widget, QPoint offset) {
            if (!widget->isVisible())
                return;
            painter.save();
            painter.setClipRect(QRect(offset, widget->size()), Qt::IntersectClip);
            if (auto *gpu = qobject_cast<QRhiWidget *>(widget)) {
                painter.drawImage(QRect(offset, widget->size()), gpu->grabFramebuffer());
            } else
                widget->render(&painter, offset, QRegion(), QWidget::DrawWindowBackground);
            for (auto *child : widget->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
                draw(child, offset + child->pos());
            painter.restore();
        };
        draw(&root, {});
        // The first paint can finish a newly expanded child panel's layout.
        // Capture again after it settles, using the resulting clipping bounds.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        draw(&root, {});
        return image;
    }
    Ft8::Decode decode(const QString &text, int seconds = 0) {
        Ft8::Decode d;
        d.message = text;
        d.utc = QDateTime(QDate(2026, 9, 8), QTime(12, 0, seconds), QTimeZone::UTC);
        d.audioHz = 1240;
        d.snr = -12;
        return d;
    }
    Ft8Session station() {
        Ft8Session s;
        s.myCall = "K2XYZ";
        s.myGrid = "EM10";
        return s;
    }
    AdifRecord contact(QString mode = "FT4") {
        return {
            {"CALL", "K1ABC"}, {"QSO_DATE", "20260908"}, {"TIME_ON", "120000"}, {"FREQ", "14.080750"},
            {"MODE", mode},    {"RST_SENT", "-12"},      {"RST_RCVD", "-09"},   {"APP_TEST_EXTRA", "<literal:4>test"}};
    }
private slots:
    void measuredReportAutomaticExchange_data() {
        QTest::addColumn<int>("protocol");
        QTest::newRow("FT8") << 0;
        QTest::newRow("FT4") << 1;
    }
    void measuredReportAutomaticExchange() {
        QFETCH(int, protocol);
        const auto mode = Ft8::Mode(protocol);
        auto receiveAudio = [&](const QString &message, int period) {
            QString error;
            const auto wave = ft8TransmitWaveform(message, mode, 1500, &error);
            QVector<float> audio(Ft8::periodMs(mode) * 12, 0);
            std::mt19937 rng(81 + period);
            std::normal_distribution<float> noise(0, .02f);
            for (auto &sample : audio) sample = noise(rng);
            for (int i = 0; i < wave.size(); ++i) audio[6000 + i] += .006f * wave[i] / 26213;
            return Ft8Receiver::decodeSamples(audio, mode, decode("").utc.addMSecs(period * Ft8::periodMs(mode)));
        };
        QTemporaryDir dir;
        Ft8Screen screen(nullptr, dir.path());
        screen.m_session = station();
        screen.m_session.mode = mode;
        screen.m_session.callFirst = true;
        QVERIFY(screen.m_session.callCq());
        QVERIFY(screen.m_session.arm());
        QVERIFY(screen.m_session.sent(decode("").utc));
        const auto grid = receiveAudio("K2XYZ K1ABC FN42", 1);
        QVERIFY(!grid.isEmpty());
        QVERIFY(grid.first().snr.has_value());
        screen.addDecodes(grid);
        QCOMPARE(screen.session().sentReport, Ft8::reportText(*grid.first().snr));
        QCOMPARE(screen.session().nextMessage, "K1ABC K2XYZ " + screen.session().sentReport);
        const QString measured = screen.session().sentReport;
        QVERIFY(screen.m_session.sent(decode("").utc.addMSecs(2 * Ft8::periodMs(mode))));
        const auto reply = receiveAudio("K2XYZ K1ABC R-09", 3);
        QVERIFY(!reply.isEmpty());
        screen.addDecodes(reply);
        QCOMPARE(screen.session().nextMessage, "K1ABC K2XYZ RR73");
        QCOMPARE(screen.session().receivedReport, "-09");
        QCOMPARE(screen.session().sentReport, measured);
        QVERIFY(screen.m_session.sent(decode("").utc.addMSecs(4 * Ft8::periodMs(mode))));
        QVERIFY(screen.session().complete);
        QVERIFY(!screen.session().armed);
        // Early and final passes of the same decode cannot change the report
        // already sent or reopen a completed contact.
        screen.addDecodes(reply);
        QVERIFY(screen.session().complete);
        QCOMPARE(screen.session().sentReport, measured);
    }
    void measuredReports_data() {
        QTest::addColumn<int>("protocol");
        QTest::addColumn<int>("level");
        QTest::addColumn<int>("hz");
        QTest::addColumn<float>("gain");
        for (int mode : {0, 1}) {
            for (int level : {-12, 0, 15})
                QTest::newRow(qPrintable(QString("%1_%2").arg(mode).arg(level))) << mode << level << 1250 << 1.0f;
            QTest::newRow(qPrintable(QString("%1_weak").arg(mode))) << mode << (mode ? -15 : -18) << 1733 << 1.0f;
            QTest::newRow(qPrintable(QString("%1_gain").arg(mode))) << mode << -12 << 1250 << .125f;
            QTest::newRow(qPrintable(QString("%1_high_audio").arg(mode))) << mode << -12 << 2873 << 1.0f;
        }
    }
    void measuredReports() {
        QFETCH(int, protocol); QFETCH(int, level); QFETCH(int, hz); QFETCH(float, gain);
        const auto mode = Ft8::Mode(protocol);
        const QString message = "CQ K1ABC FN42";
        QString error;
        const auto wave = ft8TransmitWaveform(message, mode, hz, &error);
        QVERIFY2(!wave.isEmpty(), qPrintable(error));
        constexpr double sigma = .02;
        // White real noise spans 0–6000 Hz. Specify signal power relative to
        // its power in 2500 Hz, independently of either decoder's estimator.
        const double amplitude = std::sqrt(2 * sigma * sigma * (2500.0 / 6000) * std::pow(10.0, level / 10.0));
        QVector<float> audio(Ft8::periodMs(mode) * 12);
        std::mt19937 rng(131);
        std::normal_distribution<float> noise(0, sigma);
        for (auto &v : audio) v = noise(rng);
        const int lead = 6000 + 173;
        for (int i = 0; i < wave.size(); ++i) audio[lead + i] += amplitude * wave[i] / 26213.0;
        QVector<qint16> pcm(audio.size());
        for (int i = 0; i < audio.size(); ++i) {
            pcm[i] = qRound(audio[i] * gain * 32768);
            audio[i] = pcm[i] / 32768.0f;
        }
        const auto decoded = Ft8Receiver::decodeSamples(audio, mode, QDateTime::currentDateTimeUtc());
        const auto found = std::find_if(decoded.cbegin(), decoded.cend(), [&](const auto &d) { return d.message == message; });
        // Also measure below the embedded decoder's sensitivity: a CRC-valid
        // payload supplied by the reference is sufficient for this estimator.
        ftx_message_t packed{};
        QCOMPARE(ftx_message_encode(&packed, nullptr, message.toLatin1().constData()), FTX_MESSAGE_RC_OK);
        Ft8Snr estimator(audio, mode);
        const auto measured = found != decoded.cend() ? found->snr : estimator.estimate(packed.payload, hz, lead / 12000.0);
        if (level >= -12) QVERIFY2(found != decoded.cend(), "Known signal did not decode");
        QVERIFY(measured.has_value());
        qInfo() << "SNR measurement" << Ft8::modeName(mode) << "injected" << level << "measured" << *measured << "Hz" << hz << "gain" << gain;
        QVERIFY2(std::abs(*measured - level) <= 3, qPrintable(QString("Injected %1, measured %2").arg(level).arg(*measured)));

        // Optional real reference executable. No radio control or live audio.
        const auto reference = qEnvironmentVariable("QK4_WSJT_REFERENCE");
        if (!reference.isEmpty()) {
            QTemporaryDir dir;
            const QString path = dir.filePath("260909_120000.wav");
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QDataStream stream(&file); stream.setByteOrder(QDataStream::LittleEndian);
            file.write("RIFF",4); stream << quint32(36 + pcm.size()*2);
            file.write("WAVEfmt ",8); stream << quint32(16) << quint16(1) << quint16(1)
                << quint32(12000) << quint32(24000) << quint16(2) << quint16(16);
            file.write("data",4); stream << quint32(pcm.size()*2);
            for (auto v : pcm) stream << v;
            file.close();
            QProcess decoder;
            decoder.setWorkingDirectory(dir.path());
            decoder.start(reference, {protocol ? "-5" : "-8", "-d", "1", "-L", "100", "-H", "3300", path});
            QVERIFY(decoder.waitForFinished(30000));
            const QString output = QString::fromLocal8Bit(decoder.readAllStandardOutput());
            qInfo().noquote() << "WSJT-X reference:" << output.trimmed();
            QVERIFY2(output.contains(message), qPrintable(output + decoder.readAllStandardError()));
            const QRegularExpression row("\\b\\d{6}\\s+(-?\\d+)\\s+[-\\d.]+\\s+\\d+\\s+[~+]\\s+CQ K1ABC FN42");
            const auto match = row.match(output);
            QVERIFY2(match.hasMatch(), qPrintable(output));
            const int expected = match.captured(1).toInt();
            QVERIFY2(std::abs(*measured - expected) <= 2,
                     qPrintable(QString("QK4 %1, WSJT-X %2").arg(*measured).arg(expected)));
        }
        // Selecting this real decode automatically prepares its measured report.
        if (found != decoded.cend()) {
            auto session = station();
            session.mode = mode;
            QVERIFY(session.select(*found));
            QCOMPARE(session.sentReport, Ft8::reportText(*found->snr));
        }
    }
    void transmitWaveformRoundTrip_data() {
        QTest::addColumn<int>("mode");
        QTest::addColumn<QString>("message");
        for (int mode : {0, 1})
            for (const auto &message : {"CQ K1ABC FN42", "K1ABC K2XYZ R-12", "K1ABC K2XYZ RR73"})
                QTest::newRow(qPrintable(QString::number(mode) + message)) << mode << QString(message);
    }
    void transmitWaveformRoundTrip() {
        QFETCH(int, mode);
        QFETCH(QString, message);
        QString error;
        const auto wave = ft8TransmitWaveform(message, Ft8::Mode(mode), 1250, &error);
        QVERIFY2(!wave.isEmpty(), qPrintable(error));
        QCOMPARE(wave.size(), mode ? 60480 : 151680);
        QCOMPARE(wave.first(), qint16(0));
        QCOMPARE(wave.last(), qint16(0));
        QVector<float> audio(Ft8::periodMs(Ft8::Mode(mode)) * 12, 0);
        for (int i = 0; i < wave.size(); ++i) audio[6000 + i] = wave[i] / 32768.0f;
        const auto decoded = Ft8Receiver::decodeSamples(audio, Ft8::Mode(mode), QDateTime::currentDateTimeUtc());
        QVERIFY(std::any_of(decoded.cbegin(), decoded.cend(), [&](const auto &d) { return d.message == message; }));
        // The early receive pass must decode this waveform before adjacent TX.
        audio.resize((Ft8::periodMs(Ft8::Mode(mode)) - 1500) * 12);
        const auto early = Ft8Receiver::decodeSamples(audio, Ft8::Mode(mode), QDateTime::currentDateTimeUtc());
        QVERIFY(std::any_of(early.cbegin(), early.cend(), [&](const auto &d) { return d.message == message; }));
    }
    void rejectUnsupportedTransmitText() {
        QString error;
        for (const auto &message : {"HELLO WORLD", "CQ <ABC123> FN42", "", "THIS MESSAGE IS FAR TOO LONG"}) {
            QVERIFY(ft8TransmitWaveform(message, Ft8::Mode::FT8, 1500, &error).isEmpty());
            QVERIFY(!error.isEmpty());
        }
        QVERIFY(ft8TransmitWaveform("CQ K1ABC FN42", Ft8::Mode::FT8, 99, &error).isEmpty());
    }
    void timedTransmitterCancellation() {
        auto control = std::make_shared<DigitalTxControl>();
        Ft8Transmitter tx(control);
        QSignalSpy keyed(&tx, &Ft8Transmitter::keyRequested), done(&tx, &Ft8Transmitter::finished);
        const auto slot = Ft8Transmitter::nextSlot(QDateTime::currentMSecsSinceEpoch() + 7500, Ft8::Mode::FT4, true);
        control->scheduledGeneration = 301;
        tx.schedule("CQ K1ABC FN42", 1, 1500, slot, 240, 301);
        tx.cancel(300); // A stale cancellation cannot stop the current slot.
        QCOMPARE(done.count(), 0);
        tx.cancel(301);
        QCOMPARE(done.count(), 1);
        QVERIFY(!done.first()[0].toBool());
        QCOMPARE(keyed.count(), 0);
        tx.schedule("CQ K1ABC FN42", 1, 1500, slot, 240, 301); // Revoked queued request.
        QCOMPARE(done.count(), 2);
        QCOMPARE(keyed.count(), 0);
    }
    void timedTransmitterFinishesAfterDrain() {
        auto control = std::make_shared<DigitalTxControl>();
        Ft8Transmitter tx(control);
        QSignalSpy done(&tx, &Ft8Transmitter::finished);
        int frames = 0, samples = 0;
        qint64 started = 0, stopped = 0;
        connect(&tx, &Ft8Transmitter::keyRequested, &tx, [&](int mode, quint64 gen) {
            control->generation = gen;
            tx.keyed(mode, gen);
        });
        connect(&tx, &Ft8Transmitter::transmitting, &tx, [&](quint64) { started = QDateTime::currentMSecsSinceEpoch(); });
        connect(&tx, &Ft8Transmitter::frameReady, &tx, [&](const auto &pcm, int emitted, int total, quint64 gen) {
            QCOMPARE(pcm.size(), 240);
            ++frames;
            samples = emitted;
            if (emitted == total) {
                QCOMPARE(done.count(), 0);
                tx.accepted(emitted, total, total, gen);
            }
        });
        connect(&tx, &Ft8Transmitter::unkeyRequested, &tx, [&](quint64 gen) {
            stopped = QDateTime::currentMSecsSinceEpoch();
            control->close(gen);
            tx.unkeyed(gen);
        });
        const auto now = QDateTime::currentMSecsSinceEpoch();
        const auto slot = ((now + 1500 + 7499) / 7500) * 7500;
        control->scheduledGeneration = 302;
        tx.schedule("CQ K1ABC FN42", 1, 1500, slot, 240, 302);
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 16000);
        QVERIFY2(done.first()[0].toBool(), qPrintable(done.first()[1].toString()));
        QCOMPARE(samples, 60480);
        QVERIFY(frames >= 248 && frames <= 252);
        QVERIFY(started >= slot + 300 && started <= slot + 380);
        QVERIFY(stopped >= slot + 300 + 5040);
        QVERIFY(stopped < slot + 300 + 5040 + 500);
    }
    void temporaryDataModeRestoresOriginal() {
        for (const int original : {1, 2, 3, 4, 5, 6, 7, 9}) {
            Ft8RadioMode mode;
            QVERIFY(mode.update(true, original, 2, false).isEmpty()); // No module, no setting changes.
            mode.enter();
            QCOMPARE(mode.update(true, original, 2, false), "MD6;DT0;MD;DT;LI;MG;CP;TE;");
            mode.enter(); // Reopening/selecting RX or TX must not replace the saved mode.
            QVERIFY(mode.update(true, 6, 0, false).isEmpty());
            QCOMPARE(mode.update(true, 2, 0, false), "MD6;DT0;MD;DT;LI;MG;CP;TE;");
            QVERIFY(mode.update(true, 2, 0, false).isEmpty()); // Do not flood while readback is pending.
            QVERIFY(mode.update(true, 6, 0, false).isEmpty()); // DATA-A readback rearms enforcement.
            QCOMPARE(mode.update(true, 6, 2, false), "MD6;DT0;MD;DT;LI;MG;CP;TE;");
            QVERIFY(mode.update(true, 6, 2, false).isEmpty());
            mode.leave();
            QVERIFY(mode.update(true, 6, 0, true).isEmpty()); // Wait for TX/cancellation to finish.
            QCOMPARE(mode.update(true, 6, 0, false), QString("MD%1;DT2;MD;DT;LI;MG;CP;TE;").arg(original));
            QVERIFY(mode.update(true, original, 2, false).isEmpty());
        }
        Ft8RadioMode mode;
        mode.enter();
        QVERIFY(mode.update(false, 2, 0, false).isEmpty());
        QVERIFY(mode.update(true, 0, -1, false).isEmpty());
        QVERIFY(mode.update(true, 9, -1, false).isEmpty()); // Wait for the data submode snapshot.
        QVERIFY(!mode.update(true, 9, 3, false).isEmpty());
        mode.connectionLost();
        mode.leave();
        QVERIFY(mode.update(true, 2, 0, false).isEmpty()); // No stale restore on another connection.
    }
    void lateSlotSelection_data() {
        QTest::addColumn<int>("mode");
        QTest::newRow("FT8") << 0;
        QTest::newRow("FT4") << 1;
    }
    void lateSlotSelection() {
        QFETCH(int, mode);
        const auto ftMode = Ft8::Mode(mode);
        const auto period = Ft8::periodMs(ftMode);
        QCOMPARE(Ft8Transmitter::nextSlot(30200, ftMode, true), 30000);
        QCOMPARE(Ft8Transmitter::nextSlot(31500, ftMode, true), 30000);
        QCOMPARE(Ft8Transmitter::nextSlot(29900, ftMode, true), 30000);
        QCOMPARE(Ft8Transmitter::nextSlot(31500, ftMode, false), 30000 + period);
        QCOMPARE(Ft8Transmitter::nextSlot(30000 + Ft8Transmitter::latestStartMs(ftMode), ftMode, true), 30000 + 2 * period);
    }
    void lateTransmitterSkipsElapsedAudio() {
        auto control = std::make_shared<DigitalTxControl>();
        Ft8Transmitter tx(control);
        QSignalSpy keyed(&tx, &Ft8Transmitter::keyRequested), done(&tx, &Ft8Transmitter::finished);
        const auto full = ft8TransmitWaveform("CQ K1ABC FN42", Ft8::Mode::FT4, 1500, nullptr);
        int firstSample = -1, frames = 0;
        qint64 firstUtc = 0, endUtc = 0;
        connect(&tx, &Ft8Transmitter::keyRequested, &tx, [&](int mode, quint64 gen) {
            control->generation = gen;
            tx.keyed(mode, gen);
        });
        connect(&tx, &Ft8Transmitter::frameReady, &tx, [&](const auto &pcm, int emitted, int total, quint64 gen) {
            if (firstSample < 0) {
                firstSample = emitted - pcm.size();
                firstUtc = QDateTime::currentMSecsSinceEpoch();
                QVERIFY(firstSample > 12000);
                QCOMPARE(pcm[0], 0); // Short fade-in at the truncated start.
                for (int i = 60; i < pcm.size(); ++i) QCOMPARE(pcm[i], full[firstSample + i]);
            }
            ++frames;
            if (emitted == total) tx.accepted(emitted, total, total, gen);
        });
        connect(&tx, &Ft8Transmitter::unkeyRequested, &tx, [&](quint64 gen) {
            endUtc = QDateTime::currentMSecsSinceEpoch();
            control->close(gen);
            tx.unkeyed(gen);
        });
        const auto within = QDateTime::currentMSecsSinceEpoch() % 7500;
        if (within < 1100 || within > 2500) QTest::qWait(int((8600 - within) % 7500));
        const auto requested = QDateTime::currentMSecsSinceEpoch();
        const auto slot = requested / 7500 * 7500;
        control->scheduledGeneration = 303;
        tx.schedule("CQ K1ABC FN42", 1, 1500, slot, 240, 303);
        QCOMPARE(keyed.count(), 1); // Key now in the current slot.
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 8000);
        QVERIFY2(done.first()[0].toBool(), qPrintable(done.first()[1].toString()));
        QVERIFY(firstUtc - requested >= 500 && firstUtc - requested < 800);
        QVERIFY(qAbs(firstUtc - (slot + 300 + firstSample / 12)) < 80);
        QVERIFY(frames < 252);
        QVERIFY(endUtc >= slot + 5340 && endUtc < slot + 5840);
    }
    void callUsesCurrentMatchingPeriod() {
        QTemporaryDir dir;
        Ft8Screen screen(nullptr, dir.path());
        screen.show();
        screen.setRadioState(true, 14074000, "DATA", false);
        screen.m_session = station();
        screen.m_session.mode = Ft8::Mode::FT8;
        auto selected = decode("CQ K1ABC FN42");
        selected.mode = Ft8::Mode::FT8;
        QVERIFY(screen.m_session.select(selected));
        const auto offset = QDateTime::currentMSecsSinceEpoch() % 15000;
        if (offset >= 10000) QTest::qWait(int(15000 - offset + 100));
        const auto slot = QDateTime::currentMSecsSinceEpoch() / 15000;
        screen.m_session.even = (slot % 2 == 0);
        QSignalSpy transmit(&screen, &Ft8Screen::liveTransmitRequested);
        screen.startCall();
        QCOMPARE(transmit.count(), 1);
        QCOMPARE(transmit[0][3].toLongLong(), slot * 15000);
        screen.tick();
        QCOMPARE(transmit.count(), 1); // No duplicate while preparing or after this period completes.
        screen.finishLiveTransmit(true, "done");
        screen.tick();
        QCOMPARE(transmit.count(), 1);
        screen.m_session.halt();
        screen.m_session.even = !screen.m_session.even;
        screen.startCall();
        QCOMPARE(transmit.count(), 1); // The other station is transmitting in this period.
        screen.m_session.halt();
        screen.m_session.even = !screen.m_session.even;
        connect(&screen, &Ft8Screen::liveArmRequested, &screen, [&] {
            screen.setTransmitProtection("Calibration required", true);
        });
        screen.startCall();
        QCOMPARE(transmit.count(), 1);
        QVERIFY(!screen.session().armed);
    }
    void liveQsoSurvivesOwnTxAndOnlyAdvancesOnSuccess() {
        QTemporaryDir dir;
        Ft8Screen screen(nullptr, dir.path());
        screen.setRadioState(true, 14074000, "DATA", false);
        screen.m_session = station();
        QVERIFY(screen.m_session.select(decode("CQ K1ABC FN42")));
        QVERIFY(screen.m_session.arm());
        screen.m_livePending = true;
        screen.m_liveMessage = decode(screen.m_session.nextMessage);
        screen.setRadioState(true, 14074000, "DATA", true);
        QCOMPARE(screen.session().dxCall, "K1ABC");
        QVERIFY(screen.session().armed);
        QCOMPARE(screen.session().retries, 0);
        screen.setRadioState(true, 14074000, "DATA", false);
        screen.finishLiveTransmit(true, "done");
        QCOMPARE(screen.session().retries, 1);
        screen.m_livePending = true;
        screen.finishLiveTransmit(false, "halted");
        QCOMPARE(screen.session().retries, 1);
        QVERIFY(!screen.session().armed);
    }
    void olderDecodeCanBeCalled() {
        QTemporaryDir dir;
        Ft8Screen screen(nullptr, dir.path());
        screen.setRadioState(true, 14074000, "DATA", false);
        screen.m_session = station();
        auto old = decode("CQ K1ABC FN42");
        old.utc = QDateTime::currentDateTimeUtc().addSecs(-3600);
        old.practice = false;
        QSignalSpy armed(&screen, &Ft8Screen::liveArmRequested);
        screen.selectStation(old, true);
        QCOMPARE(screen.session().dxCall, "K1ABC");
        QVERIFY(screen.session().armed);
        QCOMPARE(armed.count(), 1);
        QVERIFY(screen.m_notice.isEmpty());
    }
    void captureResumesAfterCalibrationUnkeys() {
        QTemporaryDir dir;
        Ft8Screen screen(nullptr, dir.path());
        screen.show();
        Ft8Receiver receiver;
        bool calibrating = false;
        const auto refresh = [&] {
            receiver.setCapture(screen.receiving() && !screen.m_radioTx && !calibrating, screen.mode());
        };
        connect(&screen, &Ft8Screen::captureChanged, &receiver, refresh);
        screen.setRadioState(true, 14074000, "DATA", false);
        const auto receivingGeneration = receiver.generation();
        calibrating = true;
        refresh();
        screen.setRadioState(true, 14074000, "DATA", true);
        const auto pausedGeneration = receiver.generation();
        QVERIFY(pausedGeneration > receivingGeneration);
        // Completion can arrive before the UI receives the K4's RX readback.
        calibrating = false;
        refresh();
        QCOMPARE(receiver.generation(), pausedGeneration);
        screen.setRadioState(true, 14074000, "DATA", false);
        QVERIFY(screen.receiving());
        QVERIFY(receiver.generation() > pausedGeneration);
        const auto resumedGeneration = receiver.generation();
        refresh(); // Closing setup must not throw away a partially received slot.
        QCOMPARE(receiver.generation(), resumedGeneration);
        // An intentional Receive pause must still be respected.
        screen.m_rx->setChecked(false);
        screen.setRadioState(true, 14074000, "DATA", true);
        const auto disabledGeneration = receiver.generation();
        screen.setRadioState(true, 14074000, "DATA", false);
        QVERIFY(!screen.receiving());
        QCOMPARE(receiver.generation(), disabledGeneration);
    }
    void init() { QTest::failOnWarning(QRegularExpression(".*(No QRhi|belongs to QRhi|Failed to load shader).*")); }
    void initTestCase() {
        int regular = QFontDatabase::addApplicationFont(QFINDTESTDATA("../resources/fonts/Inter-Regular.ttf"));
        QVERIFY(regular >= 0);
        QFontDatabase::addApplicationFont(QFINDTESTDATA("../resources/fonts/Inter-SemiBold.ttf"));
        QFont f(QFontDatabase::applicationFontFamilies(regular).first());
        f.setPixelSize(12);
        QApplication::setFont(f);
    }
    void modeButtonGestures() {
        K4Styles::configureForScreen(QSize(800, 390), 3, 6.8, true);
        QWidget host;
        host.resize(800, 390);
        host.show();
        FnPopupWidget popup(&host);
        auto *button = popup.findChild<FnMenuButton *>("digitalModesButton");
        QVERIFY(button);
        QCOMPARE(button->primaryFunctionId(), MacroIds::Ft8);
        QCOMPARE(button->alternateFunctionId(), MacroIds::Sstv);
        QSignalSpy action(&popup, &FnPopupWidget::functionTriggered);
        popup.show();
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(action.size(), 1);
        QCOMPARE(action[0][0].toString(), MacroIds::Ft8);
        action.clear();
        popup.show();
        QTest::mousePress(button, Qt::LeftButton);
        QTest::qWait(650);
        QTest::mouseRelease(button, Qt::LeftButton);
        QCOMPARE(action.size(), 1);
        QCOMPARE(action[0][0].toString(), MacroIds::Sstv);
        action.clear();
        popup.show();
        QTest::mousePress(button, Qt::LeftButton);
        QTest::mouseMove(button, QPoint(-20, -20));
        QTest::mouseRelease(button, Qt::LeftButton, Qt::NoModifier, QPoint(-20, -20));
        QCOMPARE(action.size(), 0);
    }
    void dxListTapAndLogHold() {
        QWidget host;
        host.resize(800, 390);
        host.show();
        FnPopupWidget popup(&host);
        auto *button = popup.findChild<FnMenuButton *>("dxListLogButton");
        QVERIFY(button);
        QCOMPARE(button->primaryFunctionId(), MacroIds::DxList);
        QCOMPARE(button->alternateFunctionId(), MacroIds::Log);
        QSignalSpy action(&popup, &FnPopupWidget::functionTriggered);
        popup.show();
        QTest::mouseClick(button, Qt::LeftButton, Qt::NoModifier, QPoint(button->width() / 2, button->height() - 4));
        QCOMPARE(action.count(), 1);
        QCOMPARE(action[0][0].toString(), MacroIds::DxList);
        popup.show();
        QTest::mousePress(button, Qt::LeftButton);
        QTest::qWait(650);
        QTest::mouseRelease(button, Qt::LeftButton);
        QCOMPARE(action.count(), 2);
        QCOMPARE(action[1][0].toString(), MacroIds::Log);
    }
    void standardReply() {
        auto s = station();
        QVERIFY(s.select(decode("CQ K1ABC FN42")));
        QCOMPARE(s.nextMessage, "K1ABC K2XYZ EM10");
        QVERIFY(!s.even);
        QCOMPARE(s.txHz, 1500);
        QCOMPARE(s.rxHz, 1240);
        QVERIFY(s.arm());
        QVERIFY(s.sent(decode("", 15).utc));
        QVERIFY(!s.receive(decode("W9XYZ K1ABC -09", 30)));
        QVERIFY(s.receive(decode("K2XYZ K1ABC -09", 30)));
        QCOMPARE(s.nextMessage, "K1ABC K2XYZ R-12");
        QVERIFY(s.sent(decode("", 45).utc));
        auto ack = decode("K2XYZ K1ABC RR73");
        ack.utc = ack.utc.addSecs(60);
        QVERIFY(s.receive(ack));
        QCOMPARE(s.nextMessage, "K1ABC K2XYZ 73");
        QVERIFY(s.sent(ack.utc.addSecs(15)));
        QVERIFY(s.complete);
        QVERIFY(!s.armed);
    }
    void callingCq() {
        auto s = station();
        s.callFirst = true;
        QVERIFY(s.callCq("DX"));
        QCOMPARE(s.nextMessage, "CQ DX K2XYZ EM10");
        QVERIFY(s.arm());
        QVERIFY(s.sent(decode("").utc));
        QVERIFY(s.receive(decode("K2XYZ K1ABC FN42", 15)));
        QCOMPARE(s.nextMessage, "K1ABC K2XYZ -12");
        QVERIFY(s.sent(decode("", 30).utc));
        QVERIFY(s.receive(decode("K2XYZ K1ABC R-09", 45)));
        QCOMPARE(s.nextMessage, "K1ABC K2XYZ RR73");
        QVERIFY(s.sent(decode("").utc.addSecs(60)));
        QVERIFY(s.complete);
    }
    void staleAndUnrelatedAcknowledgments() {
        auto s = station();
        s.select(decode("CQ K1ABC FN42"));
        s.arm();
        s.sent(decode("", 15).utc);
        QVERIFY(!s.receive(decode("K2XYZ K1ABC RR73", 30)));
        QVERIFY(!s.complete);
        auto wrong = decode("K2XYZ K1ABC -09", 30);
        wrong.mode = Ft8::Mode::FT4;
        QVERIFY(!s.receive(wrong));
        auto report = decode("K2XYZ K1ABC -09", 30);
        QVERIFY(s.receive(report));
        QVERIFY(!s.receive(report));
        // Receiving their report alone does not prove they received ours.
        QVERIFY(!s.receive(decode("K2XYZ K1ABC RR73", 45)));
        s.halt();
        QVERIFY(!s.receive(decode("K2XYZ K1ABC RR73", 45)));
        s.select(decode("K2XYZ K1ABC 73"));
        QVERIFY(s.receivedReport.isEmpty());
    }
    void retriesAndManual() {
        auto s = station();
        s.callCq();
        s.maxRetries = 2;
        s.arm();
        QVERIFY(s.sent(decode("").utc));
        QVERIFY(s.armed);
        QVERIFY(s.sent(decode("", 30).utc));
        QVERIFY(!s.armed);
        s.select(decode("CQ K1ABC FN42"));
        s.autoSequence = false;
        s.arm();
        QVERIFY(!s.receive(decode("K2XYZ K1ABC -09", 30)));
        s.holdTx = false;
        s.select(decode("CQ W9XYZ EN37"));
        QCOMPARE(s.txHz, 1240);
        s.mode = Ft8::Mode::FT4;
        QCOMPARE(Ft8::periodMs(s.mode), 7500);
        QCOMPARE(Ft8::slot(22500, s.mode), qint64(3));
    }
    void adifRoundTripAndDuplicates() {
        QTemporaryDir dir;
        Ft8Logbook log(dir.path() + "/contacts.json");
        QString error;
        QVERIFY2(log.append(contact(), &error), qPrintable(error));
        auto adif = log.exportAdif(&error);
        QVERIFY(adif.contains("<MODE:4>MFSK<SUBMODE:3>FT4") ||
                (adif.contains("<MODE:4>MFSK") && adif.contains("<SUBMODE:3>FT4")));
        auto parsed = Ft8Logbook::parse(adif);
        QVERIFY(parsed.errors.isEmpty());
        QCOMPARE(parsed.records.size(), 1);
        QCOMPARE(parsed.records[0].value("APP_TEST_EXTRA"), "<literal:4>test");
        QCOMPARE(parsed.records[0].value("FREQ"), "14.080750");
        QCOMPARE(Ft8Logbook::canonicalMode(parsed.records[0]), "FT4");
        auto preview = log.preview(adif + adif);
        QCOMPARE(preview.duplicates, 2);
        QVERIFY(preview.records.isEmpty());
        Ft8Logbook loaded(dir.path() + "/contacts.json");
        QVERIFY(loaded.load(&error));
        QVERIFY(loaded.worked("k1abc", "20m", "FT4"));
        QVERIFY(!loaded.worked("K1ABC", "20m", "FT8"));
        QVERIFY(!loaded.append(contact(), &error));
        auto ordinary = contact("FT8");
        QVERIFY(loaded.append(ordinary));
        QVERIFY(loaded.exportAdif().contains("<MODE:3>FT8"));
    }
    void malformedImportIsAtomic() {
        Ft8Logbook log;
        QVERIFY(log.append(contact()));
        auto preview = log.preview("<CALL:5>K1ABC<QSO_DATE:8>20261308<TIME_ON:6>120000<MODE:3>FT8<BAND:3>20m<EOR>");
        QVERIFY(!preview.errors.isEmpty());
        QVERIFY(!log.importRecords(preview));
        QCOMPARE(log.records().size(), 1);
        QVERIFY(!Ft8Logbook::parse("<CALL:999>K1ABC").errors.isEmpty());
        QVERIFY(!Ft8Logbook::parse("<CALL:5>K1ABC").errors.isEmpty());
        QTemporaryDir dir;
        QFile f(dir.path() + "/contacts.json");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("broken");
        f.close();
        Ft8Logbook broken(f.fileName());
        QVERIFY(!broken.load());
        QVERIFY(!broken.append(contact()));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("broken"));
    }
    void customFrequenciesAndZoom() {
        Ft8Waterfall waterfall;
        waterfall.resize(360, 150);
        waterfall.show();
        QSignalSpy tuning(&waterfall, &Ft8Waterfall::frequencySelected);
        waterfall.setMarkers(1600, 1700);
        waterfall.zoom(4);
        QCOMPARE(waterfall.spanHz(), 750.0);
        QCOMPARE(tuning.size(), 0);
        waterfall.zoom(100);
        QCOMPARE(waterfall.spanHz(), 200.0);
        waterfall.fit();
        QCOMPARE(waterfall.lowerHz(), 0.0);
        QCOMPARE(waterfall.spanHz(), 3000.0);
        QTest::mouseClick(&waterfall, Qt::LeftButton, Qt::NoModifier, QPoint(180, 90));
        QCOMPARE(tuning.size(), 1);
        QCOMPARE(tuning[0][0].toInt(), 1500);
        QVERIFY(waterfall.renderer()->isAudioView());
        QSignalSpy tx(&waterfall, &Ft8Waterfall::txFrequencySelected);
        tuning.clear();
        QTest::mousePress(&waterfall, Qt::LeftButton, Qt::NoModifier, QPoint(270, 90));
        QTest::qWait(600);
        QTest::mouseRelease(&waterfall, Qt::LeftButton, Qt::NoModifier, QPoint(270, 90));
        QCOMPARE(tx.size(), 1);
        QCOMPARE(tx[0][0].toInt(), 2250);
        QCOMPARE(tuning.size(), 0);
        tx.clear();
        QTest::mousePress(&waterfall, Qt::LeftButton, Qt::NoModifier, QPoint(270, 90));
        QTest::mouseMove(&waterfall, QPoint(220, 90));
        QTest::qWait(600);
        QTest::mouseRelease(&waterfall, Qt::LeftButton, Qt::NoModifier, QPoint(220, 90));
        QCOMPARE(tx.size(), 0);
        QCOMPARE(tuning.size(), 0);
        QCOMPARE(Ft8::bandFor(14091000), "20m");
        QVERIFY(Ft8::bandFor(12345678).isEmpty());
    }
    void activityColors() {
        using Ft8ActivityStyle::background;
        auto cq = decode("CQ K1ABC FN42");
        QCOMPARE(background(cq, "K2XYZ", false, false), QColor("#aaffaa"));
        QCOMPARE(background(cq, "K2XYZ", false, true), QColor("#d0d0d0"));
        auto reply = decode("K2XYZ K1ABC -09");
        QCOMPARE(background(reply, "K2XYZ", false, true), QColor("#ffaaaa"));
        QCOMPARE(background(reply, "K2XYZ", true, true), QColor("#ffff00"));
        QCOMPARE(background(reply, "W9XYZ", false, false), QColor("#ffffff"));
        QCOMPARE(background(decode("<K2XYZ> K1ABC RR73"), "K2XYZ", false, false), QColor("#ffaaaa"));
    }
    void sharedPanadapterShaders() {
        QWidget host;
        host.resize(800, 260);
        PanadapterRhiWidget main(&host);
        main.setGeometry(0, 0, 560, 260);
        main.setSpectrumRatio(0.5f);
        main.setFilterBandwidth(0);
        MiniPanRhiWidget mini(&host);
        mini.setGeometry(570, 0, 220, 200);
        mini.setFilterBandwidth(0);
        main.setSpan(3000);
        main.setTunedFrequency(14074000);
        main.show();
        mini.show();
        host.show();
        QTest::qWait(80);
        QSignalSpy mainFrames(&main, &QRhiWidget::frameSubmitted);
        QSignalSpy miniFrames(&mini, &QRhiWidget::frameSubmitted);
        QByteArray bins(256, 90);
        for (int i = 100; i < 120; ++i)
            bins[i] = 140;
        // Paint enough history for the shared waterfall shader to produce a
        // measurable stripe, rather than accepting an all-black smoke test.
        for (int row = 0; row < 40; ++row) {
            mainFrames.clear();
            miniFrames.clear();
            main.updateSpectrum(bins, 14074000, 3, -120);
            mini.updateSpectrum(QByteArray(128, 0x78));
            QTRY_VERIFY_WITH_TIMEOUT(!mainFrames.isEmpty() && !miniFrames.isEmpty(), 2000);
        }
        auto hasHistory = [](QRhiWidget &widget, double spectrumRatio) {
            const auto pixels = widget.grabFramebuffer();
            const int x = pixels.width() / 4;
            for (int y = qRound(pixels.height() * spectrumRatio) + 3; y < pixels.height() - 1; ++y)
                if (pixels.pixelColor(x, y).value() > 40)
                    return true;
            return false;
        };
        QVERIFY(hasHistory(main, 0.5));
        QVERIFY(hasHistory(mini, 0.4));
        QVERIFY(!main.isAudioView());
        QVERIFY(capture(host).save("ft8-main-panadapter-check.png"));
    }
    void nativeDecode_data() {
        QTest::addColumn<bool>("ft4");
        QTest::newRow("FT8") << false;
        QTest::newRow("FT4") << true;
    }
    void nativeDecode() {
        QFETCH(bool, ft4);
        const auto mode = ft4 ? Ft8::Mode::FT4 : Ft8::Mode::FT8;
        // Independent continuous-phase fixture, generated from the encoded tone
        // sequence at 12 kHz, then mixed with deterministic background noise.
        ftx_message_t message{};
        QCOMPARE(ftx_message_encode(&message, nullptr, "CQ K1ABC FN42"), FTX_MESSAGE_RC_OK);
        uint8_t tones[105]{};
        if (ft4)
            ft4_encode(message.payload, tones);
        else
            ft8_encode(message.payload, tones);
        int symbols = ft4 ? FT4_NN : FT8_NN;
        double symbol = ft4 ? FT4_SYMBOL_PERIOD : FT8_SYMBOL_PERIOD;
        int sps = qRound(12000 * symbol);
        QVector<float> audio(Ft8::periodMs(mode) * 12, 0);
        std::mt19937 rng(81);
        std::normal_distribution<float> noise(0, .01f);
        for (auto &x : audio)
            x = noise(rng);
        double phase = 0;
        int lead = ft4 ? 2400 : 6000;
        for (int i = 0; i < symbols * sps && lead + i < audio.size(); ++i) {
            int tone = tones[i / sps];
            audio[lead + i] += .3f * float(std::sin(phase));
            phase += 2 * 3.141592653589793 * (1250 + tone / symbol) / 12000;
        }
        auto result = Ft8Receiver::decodeSamples(audio, mode, QDateTime::currentDateTimeUtc());
        bool found = false;
        for (const auto &d : result)
            if (d.message == "CQ K1ABC FN42") {
                found = true;
                QVERIFY(qAbs(d.audioHz - 1250) < 10);
                QVERIFY(d.snr.has_value());
            }
        QVERIFY(found);
        QVector<float> silence(audio.size(), 0);
        QVERIFY(Ft8Receiver::decodeSamples(silence, mode, QDateTime::currentDateTimeUtc()).isEmpty());

        // Exercise the actual queued stereo input and UTC period assembly, not
        // just a full-buffer decoder call. The unused right channel is silent.
        Ft8Receiver receiver;
        QSignalSpy streamed(&receiver, &Ft8Receiver::decoded);
        receiver.setCapture(true, mode);
        const qint64 start = decode("").utc.toMSecsSinceEpoch();
        for (int pos = 0; pos < audio.size(); pos += 240) {
            const int frames = qMin(240, int(audio.size()) - pos);
            QVector<float> stereo(frames * 2, 0);
            for (int i = 0; i < frames; ++i)
                stereo[i * 2] = audio[pos + i];
            receiver.enqueue(
                QByteArray(reinterpret_cast<const char *>(stereo.constData()), stereo.size() * sizeof(float)),
                start + (pos + frames) / 12);
            QCoreApplication::processEvents();
        }
        QCOMPARE(streamed.size(), 2);
        found = false;
        for (const auto &d : streamed[0][0].value<QVector<Ft8::Decode>>())
            if (d.message == "CQ K1ABC FN42") {
                found = true;
                QCOMPARE(d.utc.toMSecsSinceEpoch(), start);
            }
        QVERIFY(found);
        const auto generation = receiver.generation();
        receiver.enqueue(QByteArray(240 * 2 * sizeof(float), 0), start + Ft8::periodMs(mode) + 20);
        receiver.setCapture(false, mode);
        QCoreApplication::processEvents();
        QVERIFY(receiver.generation() > generation);
        QCOMPARE(streamed.size(), 2);
    }
    void ctr2DialRouting_data() {
        QTest::addColumn<bool>("extended");
        QTest::addColumn<bool>("ft4");
        QTest::addColumn<QString>("wheelAction");
        for (bool extended : {false, true})
            for (bool ft4 : {false, true})
                for (const QString &action : {QString("active_vfo_frequency"), QString("other_vfo_frequency"),
                                              QString("selected_adjustment")})
                    QTest::newRow(qPrintable(QString("%1-%2-%3").arg(extended).arg(ft4).arg(action)))
                        << extended << ft4 << action;
    }
    void ctr2DialRouting() {
        QFETCH(bool, extended);
        QFETCH(bool, ft4);
        QFETCH(QString, wheelAction);
        QTemporaryDir dir;
        QWidget host;
        host.resize(390, 800);
        Ft8Screen screen(&host, dir.path());
        screen.resize(host.size());
        screen.show();
        host.show();
        screen.setRadioState(true, 14074000, "DATA", false);
        screen.m_timer->stop();
        screen.m_session.mode = ft4 ? Ft8::Mode::FT4 : Ft8::Mode::FT8;
        QTest::qWait(80);
        auto mapping = extended ? MidiMapping::ctr2ExtendedDefault() : MidiMapping::ctr2Default();
        mapping.buttons[6] = {"adjust_ft8_rx_tx", {}};
        mapping.buttons[extended ? 30 : 16] = {"set_ft8_frequency", {}};
        mapping.buttons[5] = {"khz", {}};
        mapping.buttons[4] = {"tune_step", {}};
        mapping.knobs[100] = {wheelAction, MidiMapping::KnobOutput::WheelA};
        MidiMapping::DeviceMapping imported;
        QString error;
        QVERIFY2(MidiMapping::fromJson(MidiMapping::toJson(mapping), &imported, &error), qPrintable(error));
        MidiInputRouter router;
        router.setMapping("ctr2", imported);
        connect(&router, &MidiInputRouter::knobActionRequested, &screen, &Ft8Screen::handleMidiKnobAction);
        connect(&router, &MidiInputRouter::buttonActionRequested, &screen, &Ft8Screen::handleMidiButtonAction);
        QSignalSpy rf(&screen, &Ft8Screen::frequencyRequested);
        QSignalSpy captureChanges(&screen, &Ft8Screen::captureChanged);
        auto turn = [&](int steps) { router.processEvent("ctr2", 0xb0, 100, 64 + steps); };
        // CTR2 emits NoteOn when a physical short/long press is released.
        auto release = [&](int note) { router.processEvent("ctr2", 0x90, note, 127); };
        auto selectTone = [&](Ft8Screen::DialTarget target) {
            release(6);
            if (screen.dialTarget() != target)
                release(6);
        };
        const int rx = screen.session().rxHz, tx = screen.session().txHz;
        const int step = int(screen.dialStepHz());
        QCOMPARE(screen.dialTarget(), Ft8Screen::DialTarget::RxAudio);
        turn(1);
        QCOMPARE(screen.session().rxHz, rx);
        QCOMPARE(screen.m_dialPreviewHz, rx + step);
        release(extended ? 30 : 16);
        QCOMPARE(screen.session().rxHz, rx + step);
        QCOMPARE(screen.session().txHz, tx);
        router.processEvent("ctr2", 0x80, extended ? 30 : 16, 0);
        QCOMPARE(screen.dialTarget(), Ft8Screen::DialTarget::RxAudio);
        selectTone(Ft8Screen::DialTarget::TxAudio);
        QCOMPARE(screen.dialTarget(), Ft8Screen::DialTarget::TxAudio);
        QVERIFY(screen.m_notice.startsWith("TX tone selected"));
        turn(-3);
        QCOMPARE(screen.session().txHz, tx);
        QCOMPARE(screen.m_dialPreviewHz, tx - 3 * step);
        release(extended ? 30 : 16);
        QCOMPARE(screen.session().txHz, tx - 3 * step);
        QCOMPARE(screen.session().rxHz, rx + step);
        QVERIFY(screen.session().holdTx);
        QVERIFY(rf.isEmpty());
        QVERIFY(captureChanges.isEmpty());
        auto *display = screen.findChild<FrequencyDisplayWidget *>("ft8Frequency");
        QVERIFY(display);
        QCOMPARE(display->displayText(), "14.074.000");
        QTest::mouseClick(display, Qt::LeftButton, Qt::NoModifier, QPoint(2, display->height() / 2));
        QCOMPARE(screen.dialTarget(), Ft8Screen::DialTarget::RadioFrequency);
        QCOMPARE(screen.dialStepHz(), 10000000);
        QVERIFY(rf.isEmpty()); // Selecting a digit does not tune.
        release(5);
        QCOMPARE(screen.dialStepHz(), 1000);
        turn(1);
        turn(1);
        QCOMPARE(rf.size(), 2);
        QCOMPARE(rf[0][0].toLongLong(), 14075000);
        QCOMPARE(rf[1][0].toLongLong(), 14076000);
        QCOMPARE(display->displayText(), "14.074.000"); // Await the K4 echo.
        screen.setRadioState(true, 14075000, "DATA", false);
        turn(1); // An older echo must not lose a rapid detent.
        QCOMPARE(rf.size(), 3);
        QCOMPARE(rf[2][0].toLongLong(), 14077000);
        screen.setRadioState(true, 14077000, "DATA", false);
        QCOMPARE(display->displayText(), "14.077.000");
        QCOMPARE(screen.session().rxHz, rx + step);
        QCOMPARE(screen.session().txHz, tx - 3 * step);
        selectTone(Ft8Screen::DialTarget::RxAudio);
        QCOMPARE(screen.dialTarget(), Ft8Screen::DialTarget::RxAudio);
        QVERIFY(screen.m_notice.startsWith("RX tone selected"));
        auto *toggle = screen.findChild<QPushButton *>("ft8ToggleWaterfall");
        auto *waterfall = screen.findChild<Ft8Waterfall *>();
        if (waterfall->isVisible())
            toggle->click();
        turn(2);
        QCOMPARE(screen.session().rxHz, rx + step);
        QCOMPARE(screen.m_dialPreviewHz, rx + 3 * step);
        QVERIFY(screen.findChild<QPushButton *>("ft8Offsets")->text().contains("Adjust RX"));
        release(extended ? 30 : 16);
        QCOMPARE(screen.session().rxHz, rx + 3 * step);
        QCOMPARE(rf.size(), 3);
        QVERIFY(screen.findChild<QPushButton *>("ft8Offsets")->text().contains("RX set"));
        const auto span = waterfall->spanHz();
        QVERIFY(screen.handleMidiButtonAction("pan_zoom_in"));
        QVERIFY(waterfall->spanHz() < span);
        QCOMPARE(rf.size(), 3);
        release(4);
        QCOMPARE(screen.dialStepHz(), step); // RF Rate never changes tone steps.
        release(5);
        QCOMPARE(screen.dialStepHz(), step);
        turn(1);
        QCOMPARE(screen.session().rxHz, rx + 3 * step); // Finished until selected again.
        selectTone(Ft8Screen::DialTarget::RxAudio);
        turn(1); // Accidental step buttons must not disable the wheel either.
        QCOMPARE(screen.session().rxHz, rx + 3 * step);
        QCOMPARE(screen.m_dialPreviewHz, rx + 4 * step);
        release(extended ? 30 : 16);
        QCOMPARE(screen.session().rxHz, rx + 4 * step);
        QCOMPARE(rf.size(), 3);
        const int before = screen.session().rxHz;
        // Module sheets consume dial input; no fallthrough to the console VFO.
        bool checkedSheet = false;
        QTimer::singleShot(40, &screen, [&] {
            auto *dialog = screen.findChild<InWindowDialog *>();
            if (!dialog)
                return;
            checkedSheet = dialog->isVisible();
            turn(3);
            selectTone(Ft8Screen::DialTarget::TxAudio);
            dialog->reject();
        });
        screen.findChild<QPushButton *>("ft8Offsets")->click();
        QVERIFY(checkedSheet);
        QCOMPARE(screen.session().rxHz, before);
        QCOMPARE(screen.dialTarget(), Ft8Screen::DialTarget::RxAudio);
        QCOMPARE(rf.size(), 3);
        selectTone(Ft8Screen::DialTarget::TxAudio);
        screen.setRadioState(true, 14077000, "DATA", true);
        turn(1);
        QCOMPARE(screen.session().rxHz, before);
        QCOMPARE(rf.size(), 3);
        screen.setRadioState(false, 14077000, "DATA", false);
        QCOMPARE(screen.dialTarget(), Ft8Screen::DialTarget::RxAudio);
        screen.selectDialTarget(Ft8Screen::DialTarget::RadioFrequency);
        screen.suspend();
        screen.hide();
        turn(1);
        QCOMPARE(rf.size(), 3);
        screen.show();
        QCOMPARE(screen.dialTarget(), Ft8Screen::DialTarget::RxAudio);
    }
    void ctr2AdjustSetWorkflow_data() {
        ctr2DialRouting_data();
    }
    void ctr2AdjustSetWorkflow() {
        QFETCH(bool, extended);
        QFETCH(bool, ft4);
        QFETCH(QString, wheelAction);
        QTemporaryDir dir;
        QWidget host;
        host.resize(390, 800);
        Ft8Screen screen(&host, dir.path());
        screen.resize(host.size());
        host.show();
        screen.show();
        screen.setRadioState(true, 14074000, "DATA", false);
        screen.m_timer->stop();
        screen.m_session.mode = ft4 ? Ft8::Mode::FT4 : Ft8::Mode::FT8;
        screen.m_session.rxHz = screen.m_session.txHz = 1500;
        screen.m_audioStepHz = 5;
        screen.refresh();
        QTest::qWait(40);
        auto mapping = extended ? MidiMapping::ctr2ExtendedDefault() : MidiMapping::ctr2Default();
        // A user-selected button and knob, including a non-Home extended bank.
        const int shortNote = extended ? 13 : 2, longNote = extended ? 37 : 12;
        mapping.buttons[shortNote] = {"adjust_ft8_rx_tx", {}};
        mapping.buttons[longNote] = {"set_ft8_frequency", {}};
        mapping.knobs[102] = {wheelAction, MidiMapping::KnobOutput::WheelA};
        MidiMapping::DeviceMapping imported;
        QString error;
        QVERIFY2(MidiMapping::fromJson(MidiMapping::toJson(mapping), &imported, &error), qPrintable(error));
        MidiInputRouter router;
        router.setMapping("ctr2", imported);
        connect(&router, &MidiInputRouter::knobActionRequested, &screen, &Ft8Screen::handleMidiKnobAction);
        connect(&router, &MidiInputRouter::buttonActionRequested, &screen, &Ft8Screen::handleMidiButtonAction);
        QSignalSpy rf(&screen, &Ft8Screen::frequencyRequested);
        QSignalSpy captureChanges(&screen, &Ft8Screen::captureChanged);
        QSignalSpy transmit(&screen, &Ft8Screen::liveTransmitRequested);
        auto press = [&](int note) { router.processEvent("ctr2", 0x90, note, 127); };
        auto turn = [&](int steps) { router.processEvent("ctr2", 0xb0, 102, 64 + steps); };
        press(longNote); // Set without Adjust is a no-op.
        QCOMPARE(screen.session().txHz, 1500);
        press(shortNote);
        QCOMPARE(screen.dialTarget(), Ft8Screen::DialTarget::TxAudio);
        turn(60);
        QCOMPARE(screen.m_dialPreviewHz, 1800);
        QCOMPARE(screen.session().txHz, 1500);
        QVERIFY(screen.m_offsets->text().contains("Adjust TX: 1800 Hz"));
        if (!extended && !ft4)
            QVERIFY(capture(screen).save("ft8-ctr2-preview-tx.png"));
        press(longNote);
        QCOMPARE(screen.session().txHz, 1800);
        QVERIFY(screen.session().holdTx);
        QVERIFY(!screen.m_dialPreviewActive);
        turn(5);
        QCOMPARE(screen.session().txHz, 1800);
        router.processEvent("ctr2", 0x80, shortNote, 0);
        QCOMPARE(screen.dialTarget(), Ft8Screen::DialTarget::TxAudio);
        press(shortNote);
        QCOMPARE(screen.dialTarget(), Ft8Screen::DialTarget::RxAudio);
        turn(-50);
        QCOMPARE(screen.session().rxHz, 1500);
        QCOMPARE(screen.m_dialPreviewHz, 1250);
        const int tolerance = ft4 ? 45 : 25;
        auto add = [&](QString call, int hz, bool tx = false) {
            auto d = decode("CQ " + call + " FN42");
            d.mode = screen.mode();
            d.audioHz = hz;
            screen.appendDecode(d, tx);
        };
        add("K1ABC", 1250);
        add("K2ABC", 1250 + tolerance);
        add("K3ABC", 1251 + tolerance);
        add("K4ABC", 1800);
        add("K5ABC", 1800, true);
        press(longNote);
        QCOMPARE(screen.session().rxHz, 1250);
        QCOMPARE(screen.session().txHz, 1800);
        QCOMPARE(screen.m_rxFocusHz, 1250);
        QCOMPARE(screen.m_filter, 2);
        QVERIFY(screen.findChild<QPushButton *>("ft8Filter2")->isChecked());
        for (int i = 0; i < 5; ++i)
            QCOMPARE(screen.m_activity->item(i)->isHidden(), i == 2 || i == 3);
        turn(10);
        QCOMPARE(screen.session().rxHz, 1250);
        add("K6ABC", 1255);
        QVERIFY(!screen.m_activity->item(5)->isHidden());
        if (!extended && !ft4)
            QVERIFY(capture(screen).save("ft8-ctr2-rx-focus.png"));
        screen.findChild<QPushButton *>("ft8Filter0")->click();
        for (int i = 0; i < screen.m_activity->count(); ++i)
            QVERIFY(!screen.m_activity->item(i)->isHidden());
        screen.findChild<QPushButton *>("ft8Filter2")->click();
        QVERIFY(screen.m_activity->item(3)->isHidden());
        // Selecting a decoded station inside the window keeps the committed
        // RX center and held TX; knob movement must still await Adjust.
        auto selected = screen.m_activity->item(1)->data(Qt::UserRole).value<Ft8::Decode>();
        screen.selectStation(selected, false);
        QCOMPARE(screen.session().rxHz, 1250);
        QCOMPARE(screen.session().txHz, 1800);
        turn(5);
        QCOMPARE(screen.session().rxHz, 1250);
        QVERIFY(rf.isEmpty());
        QVERIFY(captureChanges.isEmpty());
        QVERIFY(transmit.isEmpty());
        // Preview toggles discard the uncommitted position; TX blocks commit.
        press(shortNote);
        turn(10);
        screen.m_livePending = true;
        press(longNote);
        QCOMPARE(screen.session().txHz, 1800);
        screen.m_livePending = false;
        press(shortNote);
        QCOMPARE(screen.m_dialPreviewHz, 1250);
        turn(5);
        screen.setRadioState(false, 14074000, "DATA", false);
        QVERIFY(!screen.m_dialPreviewActive);
        QCOMPARE(screen.m_rxFocusHz, 0);
        press(longNote);
        QCOMPARE(screen.session().rxHz, 1250);
    }
    void toneTuningRecovery_data() {
        QTest::addColumn<bool>("ft4");
        QTest::newRow("FT8") << false;
        QTest::newRow("FT4") << true;
    }
    void toneTuningRecovery() {
        QFETCH(bool, ft4);
        QTemporaryDir dir;
        QWidget host;
        host.resize(320, 568);
        Ft8Screen screen(&host, dir.path());
        screen.resize(host.size());
        host.show();
        screen.show();
        screen.setRadioState(true, 14074000, "DATA", false);
        screen.m_timer->stop();
        screen.m_session.mode = ft4 ? Ft8::Mode::FT4 : Ft8::Mode::FT8;
        screen.m_session.rxHz = 1250;
        screen.m_session.txHz = 1800;
        screen.m_audioStepHz = 5;
        QTest::qWait(40);
        QSignalSpy rf(&screen, &Ft8Screen::frequencyRequested);
        auto *frequency = screen.findChild<FrequencyDisplayWidget *>("ft8Frequency");
        auto selectRf = [&] {
            QTest::mouseClick(frequency, Qt::LeftButton, Qt::NoModifier, QPoint(2, 15));
            QCOMPARE(screen.dialTarget(), Ft8Screen::DialTarget::RadioFrequency);
        };
        auto chooseToneOption = [&](const QString &text) {
            bool chosen = false;
            QTimer::singleShot(50, &screen, [&] {
                auto *dialog = screen.findChild<InWindowDialog *>();
                if (!dialog) return;
                for (auto *b : dialog->findChildren<QPushButton *>()) {
                    if (b->text() == text) {
                        chosen = true;
                        b->click();
                        return;
                    }
                }
                dialog->reject();
            });
            screen.findChild<QPushButton *>("ft8Offsets")->click();
            QVERIFY(chosen);
        };
        // The assigned tone selector recovers explicitly from RF selection.
        screen.beginDialAdjustment(Ft8Screen::DialTarget::RxAudio);
        selectRf();
        screen.handleMidiButtonAction("adjust_ft8_rx_tx");
        screen.handleMidiKnobAction("selected_adjustment", 1, false);
        QCOMPARE(screen.dialTarget(), Ft8Screen::DialTarget::TxAudio);
        QCOMPARE(screen.m_dialPreviewHz, 1805);
        QCOMPARE(screen.session().txHz, 1800);
        QVERIFY(rf.isEmpty());
        // Recover explicitly from RF plus a legacy coarse tone step.
        selectRf();
        screen.m_audioStepHz = 1000;
        chooseToneOption("Tone step: 5 Hz");
        QCOMPARE(screen.dialTarget(), Ft8Screen::DialTarget::TxAudio);
        QCOMPARE(screen.dialStepHz(), 5);
        screen.handleMidiKnobAction("selected_adjustment", -1, false);
        QCOMPARE(screen.m_dialPreviewHz, 1795);
        screen.handleMidiButtonAction("khz");
        QCOMPARE(screen.dialStepHz(), 5);
        QCOMPARE(screen.session().txHz, 1800);
        chooseToneOption("Set tone frequency");
        QCOMPARE(screen.session().txHz, 1795);
        QVERIFY(screen.session().holdTx);
        selectRf();
        chooseToneOption("Select RX tone");
        QCOMPARE(screen.dialTarget(), Ft8Screen::DialTarget::RxAudio);
        screen.handleMidiKnobAction("selected_adjustment", 2, false);
        QCOMPARE(screen.session().rxHz, 1250);
        QCOMPARE(screen.m_dialPreviewHz, 1260);
        chooseToneOption("Tone step: 1 Hz");
        QCOMPARE(screen.m_dialPreviewHz, 1260); // Step changes preserve the preview.
        chooseToneOption("Set tone frequency");
        QCOMPARE(screen.session().rxHz, 1260);
        QCOMPARE(screen.session().txHz, 1795);
        QCOMPARE(screen.m_rxFocusHz, 1260);
        QCOMPARE(screen.m_filter, 2);
        screen.handleMidiKnobAction("selected_adjustment", 10, false);
        QCOMPARE(screen.session().rxHz, 1260);
        // Repeated RF Rate presses cannot change the selected tone's step.
        for (int i = 0; i < 15; ++i) {
            screen.handleMidiButtonAction("tune_step");
            QCOMPARE(screen.dialStepHz(), 1);
        }
        QVERIFY(rf.isEmpty());
        QCOMPARE(screen.size(), QSize(320, 568));
        if (!ft4) {
            QTimer::singleShot(50, &screen, [&] {
                auto *dialog = screen.findChild<InWindowDialog *>();
                QVERIFY(dialog);
                QVERIFY(capture(screen).save("ft8-tone-recovery-menu.png"));
                dialog->reject();
            });
            screen.findChild<QPushButton *>("ft8Offsets")->click();
        }
    }
    void ctr2RfGuardrails_data() { ctr2AdjustSetWorkflow_data(); }
    void ctr2RfGuardrails() {
        QFETCH(bool, extended);
        QFETCH(bool, ft4);
        QFETCH(QString, wheelAction);
        QTemporaryDir dir;
        QWidget host;
        host.resize(390, 800);
        Ft8Screen screen(&host, dir.path());
        screen.resize(host.size());
        host.show();
        screen.show();
        screen.setRadioState(true, 14074000, "DATA", false);
        screen.m_timer->stop();
        screen.m_session.mode = ft4 ? Ft8::Mode::FT4 : Ft8::Mode::FT8;
        screen.m_audioStepHz = 5;
        QTest::qWait(40);
        auto mapping = extended ? MidiMapping::ctr2ExtendedDefault() : MidiMapping::ctr2Default();
        const QStringList rfActions{"tune_step", "khz", "adjust_active_vfo_frequency",
                                    "adjust_other_vfo_frequency", "band_up", "band_down"};
        for (int i = 0; i < rfActions.size(); ++i)
            mapping.buttons[i + 1] = {rfActions[i], {}};
        const int adjustNote = extended ? 28 : 14;
        const int setNote = extended ? 29 : 15;
        mapping.buttons[adjustNote] = {"adjust_ft8_rx_tx", {}};
        mapping.buttons[setNote] = {"set_ft8_frequency", {}};
        mapping.knobs[100] = {wheelAction, MidiMapping::KnobOutput::WheelA};
        MidiInputRouter router;
        router.setMapping("ctr2", mapping);
        QStringList fallthrough;
        connect(&router, &MidiInputRouter::buttonActionRequested, &screen, [&](const QString &action) {
            if (!screen.handleMidiButtonAction(action)) fallthrough.append(action);
        });
        connect(&router, &MidiInputRouter::knobActionRequested, &screen, &Ft8Screen::handleMidiKnobAction);
        auto press = [&](int note) { router.processEvent("ctr2", 0x90, note, 127); };
        auto turn = [&] { router.processEvent("ctr2", 0xb0, 100, 65); };
        QSignalSpy rf(&screen, &Ft8Screen::frequencyRequested);
        auto blockedButtons = [&] {
            const int rx = screen.session().rxHz, tx = screen.session().txHz;
            const int preview = screen.m_dialPreviewHz;
            const bool active = screen.m_dialPreviewActive, committed = screen.m_dialCommitRequired;
            const auto target = screen.dialTarget();
            const auto rfStep = screen.m_rfStepHz;
            for (int repeat = 0; repeat < 3; ++repeat)
                for (int note = 1; note <= rfActions.size(); ++note) press(note);
            QCOMPARE(screen.session().rxHz, rx);
            QCOMPARE(screen.session().txHz, tx);
            QCOMPARE(screen.m_dialPreviewHz, preview);
            QCOMPARE(screen.m_dialPreviewActive, active);
            QCOMPARE(screen.m_dialCommitRequired, committed);
            QCOMPARE(screen.dialTarget(), target);
            QCOMPARE(screen.dialStepHz(), 5);
            QCOMPARE(screen.m_rfStepHz, rfStep);
            QVERIFY(rf.isEmpty());
            QVERIFY(fallthrough.isEmpty());
        };
        blockedButtons(); // Entry alone does not opt in to RF tuning.
        press(adjustNote);
        turn();
        blockedButtons(); // An accidental Rate/selector must preserve the preview.
        press(setNote);
        blockedButtons(); // Nor may it unlock or change a committed frequency.
        auto *frequency = screen.findChild<FrequencyDisplayWidget *>("ft8Frequency");
        QTest::mouseClick(frequency, Qt::LeftButton, Qt::NoModifier, QPoint(2, 15));
        QCOMPARE(screen.dialTarget(), Ft8Screen::DialTarget::RadioFrequency);
        press(2); // KHZ is available only after explicit RF selection.
        QCOMPARE(screen.dialStepHz(), 1000);
        press(1);
        QCOMPARE(screen.dialStepHz(), 10000);
        press(3); // VFO selector cannot replace the explicit module target.
        turn();
        QCOMPARE(rf.size(), 1);
        QCOMPARE(rf.first().first().toLongLong(), 14084000);
        rf.clear();
        press(adjustNote);
        QVERIFY(screen.dialTarget() != Ft8Screen::DialTarget::RadioFrequency);
        blockedButtons(); // Tone selection disarms RF controls again.
        QTest::mouseClick(frequency, Qt::LeftButton, Qt::NoModifier, QPoint(2, 15));
        screen.suspend();
        blockedButtons(); // Leaving/re-entering cannot retain an RF opt-in.
    }
    void frequencyHoldEntry() {
        QTemporaryDir dir;
        QWidget host;
        host.resize(390, 800);
        Ft8Screen screen(&host, dir.path());
        screen.resize(host.size());
        screen.show();
        host.show();
        screen.setRadioState(true, 14074000, "DATA", false);
        QTest::qWait(80);
        auto *display = screen.findChild<FrequencyDisplayWidget *>("ft8Frequency");
        QSignalSpy rf(&screen, &Ft8Screen::frequencyRequested);
        bool found = false;
        QTimer::singleShot(650, &screen, [&] {
            auto *dialog = screen.findChild<InWindowDialog *>();
            if (!dialog)
                return;
            auto *input = screen.findChild<QLineEdit *>("ft8FrequencyInput");
            auto *save = screen.findChild<QPushButton *>("ft8UseFrequency");
            if (input && save) {
                found = true;
                input->setText("14.091.250");
                save->click();
            } else
                dialog->reject();
        });
        QTest::mousePress(display, Qt::LeftButton, Qt::NoModifier, QPoint(2, 15));
        QTest::qWait(750);
        QTest::mouseRelease(display, Qt::LeftButton, Qt::NoModifier, QPoint(2, 15));
        QVERIFY(found);
        QCOMPARE(rf.size(), 1);
        QCOMPARE(rf[0][0].toLongLong(), 14091250);
        screen.setPractice(true);
        QCOMPARE(screen.dialTarget(), Ft8Screen::DialTarget::RxAudio);
        screen.selectDialTarget(Ft8Screen::DialTarget::TxAudio);
        screen.findChild<QPushButton *>("ft8Mode")->click();
        QCOMPARE(screen.dialTarget(), Ft8Screen::DialTarget::RxAudio);
    }
    void portraitWorkflow() {
        // Match the app: the module is a child filling the radio window, so
        // Windows top-level minimum tracking sizes cannot alter phone geometry.
        QTemporaryDir dir;
        QWidget host;
        host.resize(390, 800);
        Ft8Screen screen(&host, dir.path());
        screen.resize(390, 800);
        screen.show();
        host.show();
        screen.setPractice(true);
        QTest::qWait(120);
        auto *list = screen.findChild<QListWidget *>("ft8Activity");
        QVERIFY(list);
        QVERIFY(list->count() >= 4);
        QSignalSpy radioFrequency(&screen, &Ft8Screen::frequencyRequested);
        auto *wf = screen.findChild<Ft8Waterfall *>();
        QVERIFY(wf);
        const auto before = screen.session().txHz;
        QTest::mouseClick(screen.findChild<QPushButton *>("ft8ZoomIn"), Qt::LeftButton);
        QCOMPARE(screen.session().txHz, before);
        QCOMPARE(radioFrequency.size(), 0);
        QTest::mouseClick(screen.findChild<QPushButton *>("ft8Fit"), Qt::LeftButton);
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                          list->visualItemRect(list->item(0)).center());
        QCOMPARE(screen.session().dxCall, "K1ABC");
        QVERIFY(!screen.session().armed);
        QTest::mouseClick(screen.findChild<QPushButton *>("ft8Call"), Qt::LeftButton);
        QVERIFY(screen.session().armed);
        QTest::mouseClick(screen.findChild<QPushButton *>("ft8Halt"), Qt::LeftButton);
        QVERIFY(!screen.session().armed);
        QSignalSpy frames(wf->renderer(), &QRhiWidget::frameSubmitted);
        wf->renderer()->update();
        QTRY_VERIFY_WITH_TIMEOUT(!frames.isEmpty(), 2000);
        QCOMPARE(list->item(0)->background().color(), QColor("#aaffaa"));
        const int rxBeforeHold = screen.session().rxHz;
        const QPoint openFrequency(qRound(wf->width() * 0.9), wf->height() - 12);
        QTest::mousePress(wf, Qt::LeftButton, Qt::NoModifier, openFrequency);
        QTest::qWait(600);
        QTest::mouseRelease(wf, Qt::LeftButton, Qt::NoModifier, openFrequency);
        QVERIFY(qAbs(screen.session().txHz - 2700) < 10);
        QCOMPARE(screen.session().rxHz, rxBeforeHold);
        QCOMPARE(screen.session().dxCall, "K1ABC");
        QVERIFY(screen.session().holdTx);
        QCOMPARE(radioFrequency.size(), 0);
        const int heldTx = screen.session().txHz;
        QTest::mouseClick(wf, Qt::LeftButton, Qt::NoModifier,
                          QPoint(qRound(wf->width() * (1310.0 + 20) / 3000), wf->height() - 12));
        QCOMPARE(screen.session().dxCall, "W9XYZ");
        QCOMPARE(screen.session().txHz, heldTx);
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                          list->visualItemRect(list->item(0)).center());
        QVERIFY(capture(screen).save("ft8-portrait-preview.png"));
        auto *frequency = screen.findChild<FrequencyDisplayWidget *>("ft8Frequency");
        QTest::mouseClick(frequency, Qt::LeftButton, Qt::NoModifier, QPoint(2, 15));
        screen.handleMidiButtonAction("khz");
        QVERIFY(capture(screen).save("ft8-rf-dial-preview.png"));
        screen.beginDialAdjustment(Ft8Screen::DialTarget::TxAudio);
        QVERIFY(capture(screen).save("ft8-tx-dial-preview.png"));
        screen.beginDialAdjustment(Ft8Screen::DialTarget::RxAudio);
        for (QSize size : {QSize(360, 740), QSize(320, 568)}) {
            host.resize(size);
            screen.resize(size);
            QTest::qWait(60);
            QCOMPARE(screen.size(), size);
            for (const auto &name : {"ft8Back", "ft8Cq", "ft8Call", "ft8Halt", "ft8LogQso"}) {
                auto *control = screen.findChild<QPushButton *>(name);
                QVERIFY(control);
                QRect bounds(control->mapTo(&screen, QPoint()), control->size());
                QVERIFY2(screen.rect().contains(bounds), name);
            }
        }
        QVERIFY(capture(screen).save("ft8-compact-preview.png"));
        screen.setPractice(false);
        QVERIFY(!screen.findChild<QPushButton *>("ft8Call")->isEnabled());
        QCOMPARE(radioFrequency.size(), 0);
    }
    void activityDensityAndBursts() {
        QTemporaryDir dir;
        QWidget host;
        host.resize(390, 800);
        Ft8Screen screen(&host, dir.path());
        screen.resize(host.size());
        screen.show();
        host.show();
        screen.setPractice(true);
        QTest::qWait(120);
        auto *list = screen.findChild<QListWidget *>("ft8Activity");
        auto *waterfall = screen.findChild<Ft8Waterfall *>();
        auto *toggle = screen.findChild<QPushButton *>("ft8ToggleWaterfall");
        auto *rows = screen.findChild<QPushButton *>("ft8Rows");
        auto *newMessages = screen.findChild<QPushButton *>("ft8NewActivity");
        QVERIFY(list && waterfall && toggle && rows && newMessages);
        QSignalSpy tuning(&screen, &Ft8Screen::frequencyRequested);
        QSignalSpy captureChanges(&screen, &Ft8Screen::captureChanged);
        const int rx = screen.session().rxHz, tx = screen.session().txHz;
        waterfall->zoom(2);
        const double span = waterfall->spanHz();
        const int shownHeight = list->height();
        const int waterfallHeight = waterfall->height();
        toggle->click();
        QTest::qWait(60);
        QVERIFY(waterfall->isHidden());
        QVERIFY(list->height() >= shownHeight + waterfallHeight - 4);
        QVERIFY(screen.findChild<QPushButton *>("ft8ZoomIn")->isHidden());
        QVERIFY(screen.findChild<QPushButton *>("ft8Offsets")->isVisible());
        auto burst = [&](int start, int count) {
            QVector<Ft8::Decode> batch;
            const auto utc = QDateTime::currentDateTimeUtc();
            for (int i = start; i < start + count; ++i) {
                const auto call = QString("K1A%1%2").arg(QChar('A' + i / 26)).arg(QChar('A' + i % 26));
                auto d = decode(i % 7 == 0   ? screen.session().myCall + " " + call + " -12"
                                : i % 7 == 1 ? "W9XYZ " + call + " RR73"
                                             : "CQ " + call + " FN42");
                d.utc = utc;
                d.mode = screen.mode();
                d.practice = true;
                d.audioHz = 300 + (i % 40) * 65;
                d.snr = -6 - i % 18;
                batch.append(d);
            }
            screen.addDecodes(batch);
        };
        burst(0, 40);
        QCOMPARE(list->count(), 44);
        auto chooseDensity = [&](int index) {
            bool found = false;
            QTimer::singleShot(40, &screen, [&] {
                auto *dialog = screen.findChild<InWindowDialog *>();
                auto *choice = screen.findChild<QPushButton *>("ft8Density" + QString::number(index));
                if (choice) {
                    found = true;
                    choice->click();
                } else if (dialog)
                    dialog->reject();
            });
            rows->click();
            return found;
        };
        auto visibleRows = [&] {
            int count = 0;
            for (int i = 0; i < list->count(); ++i)
                if (!list->item(i)->isHidden() &&
                    list->viewport()->rect().contains(list->visualItemRect(list->item(i))))
                    ++count;
            return count;
        };
        QVERIFY(chooseDensity(0));
        const int comfortableRows = visibleRows();
        QVERIFY(capture(screen).save("ft8-stations-comfortable-preview.png"));
        QVERIFY(chooseDensity(1));
        QVERIFY(visibleRows() > comfortableRows);
        QVERIFY(capture(screen).save("ft8-stations-compact-preview.png"));
        QVERIFY(chooseDensity(2));
        QVERIFY(visibleRows() >= comfortableRows * 2);
        const int denseRows = visibleRows();
        qInfo() << "Visible station rows at 390x800, waterfall hidden:" << comfortableRows << "comfortable,"
                << denseRows << "dense";
        QVERIFY(capture(screen).save("ft8-stations-dense-preview.png"));
        // Browsing a busy band must retain the same messages under the finger.
        list->verticalScrollBar()->setValue(120);
        auto *anchor = list->itemAt(QPoint(10, 5));
        QVERIFY(anchor);
        QVERIFY(chooseDensity(1));
        QCOMPARE(list->itemAt(QPoint(10, 5)), anchor);
        QVERIFY(chooseDensity(2));
        QCOMPARE(list->itemAt(QPoint(10, 5)), anchor);
        const QRect anchorRect = list->visualItemRect(anchor);
        const int scroll = list->verticalScrollBar()->value();
        burst(40, 48);
        QCOMPARE(list->count(), 92);
        QCOMPARE(list->verticalScrollBar()->value(), scroll);
        QCOMPARE(list->visualItemRect(anchor), anchorRect);
        QVERIFY(newMessages->isVisible());
        QCOMPARE(newMessages->text(), "48 new ↓");
        QVERIFY(capture(screen).save("ft8-stations-new-preview.png"));
        newMessages->click();
        QCOMPARE(list->verticalScrollBar()->value(), list->verticalScrollBar()->maximum());
        QVERIFY(newMessages->isHidden());
        auto *target = list->item(list->count() - 3);
        const auto targetCall = Ft8::parseMessage(target->data(Qt::UserRole).value<Ft8::Decode>().message).from;
        const auto point = list->visualItemRect(target).center();
        QTest::mousePress(list->viewport(), Qt::LeftButton, Qt::NoModifier, point);
        burst(88, 40);
        QCOMPARE(list->count(), 92); // Queue the batch until this gesture is finished.
        QTest::mouseRelease(list->viewport(), Qt::LeftButton, Qt::NoModifier, point);
        QCOMPARE(screen.session().dxCall, targetCall);
        QVERIFY(!screen.session().armed);
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 132, 1000);
        toggle->click();
        QTest::qWait(60);
        QVERIFY(waterfall->isVisible());
        QCOMPARE(waterfall->spanHz(), span);
        QCOMPARE(screen.session().txHz, tx);
        QVERIFY(captureChanges.isEmpty());
        QVERIFY(tuning.isEmpty());
        toggle->click();
        for (QSize size : {QSize(360, 740), QSize(320, 568)}) {
            host.resize(size);
            screen.resize(size);
            QTest::qWait(60);
            for (const auto &name : {"ft8Back", "ft8ToggleWaterfall", "ft8Rows", "ft8Options", "ft8Call", "ft8Halt"}) {
                auto *control = screen.findChild<QPushButton *>(name);
                QVERIFY(control);
                QVERIFY2(screen.rect().contains(QRect(control->mapTo(&screen, QPoint()), control->size())), name);
            }
        }
        QVERIFY(capture(screen).save("ft8-stations-small-phone-preview.png"));
        // Only an explicit RX station selection changed RX during the test.
        QVERIFY(rx != screen.session().rxHz);
    }
    void rfPowerControl() {
        QTemporaryDir dir;
        QWidget host;
        host.resize(360, 696);
        Ft8Screen screen(&host, dir.path());
        screen.resize(host.size());
        screen.show();
        host.show();
        QTest::qWait(60);
        auto *slider = screen.findChild<QSlider *>("ft8Power");
        auto *value = screen.findChild<QLabel *>("ft8PowerValue");
        auto *frequency = screen.findChild<FrequencyDisplayWidget *>("ft8Frequency");
        QVERIFY(slider && value && frequency);
        QSignalSpy requests(&screen, &Ft8Screen::powerRequested);
        QVERIFY(!slider->isEnabled());
        QCOMPARE(value->text(), QString("— W"));
        screen.setRadioState(true, 14074000, "DATA", false);
        QVERIFY(!slider->isEnabled()); // Wait for the actual radio power state.
        screen.setRfPower(25);
        QVERIFY(slider->isEnabled());
        QCOMPARE(value->text(), QString("25 W"));
        QCOMPARE(requests.count(), 0); // Opening/synchronizing cannot set power.
        QVERIFY(slider->y() > frequency->geometry().bottom());
        QVERIFY(slider->height() >= 28);
        QStyleOptionSlider option;
        option.initFrom(slider);
        option.orientation = Qt::Horizontal;
        option.minimum = slider->minimum();
        option.maximum = slider->maximum();
        option.sliderPosition = option.sliderValue = slider->value();
        const QRect handle = slider->style()->subControlRect(QStyle::CC_Slider, &option,
                                                            QStyle::SC_SliderHandle, slider);
        QTest::mousePress(slider, Qt::LeftButton, Qt::NoModifier, handle.center());
        QVERIFY(slider->isSliderDown());
        QCOMPARE(requests.count(), 0);
        const QPoint destination = handle.center() + QPoint(60, 0);
        QTest::mouseMove(slider, destination);
        const int dragged = slider->sliderPosition();
        QVERIFY(dragged > 25);
        screen.setRfPower(20); // An echo must not jump the handle under a finger.
        QCOMPARE(slider->sliderPosition(), dragged);
        QCOMPARE(requests.count(), 0);
        QTest::mouseRelease(slider, Qt::LeftButton, Qt::NoModifier, destination);
        QCOMPARE(requests.count(), 1);
        QCOMPARE(requests.last().first().toDouble(), double(dragged));
        screen.setRfPower(5.5);
        QCOMPARE(value->text(), QString("5.5 W")); // Preserve fractional K4 readback.
        QCOMPARE(requests.count(), 1);
        screen.setPractice(true);
        QCOMPARE(value->text(), QString("25 W"));
        slider->setValue(10);
        QCOMPARE(value->text(), QString("10 W"));
        screen.setRfPower(40);
        QCOMPARE(value->text(), QString("10 W"));
        screen.setPractice(false);
        QCOMPARE(value->text(), QString("40 W"));
        QCOMPARE(requests.count(), 1); // Practice never changes the K4.
        slider->setSliderDown(true);
        slider->setSliderPosition(75);
        screen.suspend();
        QVERIFY(!slider->isSliderDown());
        QCOMPARE(slider->value(), 40);
        QCOMPARE(requests.count(), 1); // Leaving/backgrounding cancels a drag.
        slider->setSliderDown(true);
        slider->setSliderPosition(80);
        screen.setRadioState(true, 14074000, "DATA", true);
        QVERIFY(!slider->isEnabled());
        QVERIFY(!slider->isSliderDown());
        QCOMPARE(requests.count(), 1); // A TX transition cancels an unfinished drag.
        screen.setRadioState(false, 14074000, "DATA", false);
        QCOMPARE(value->text(), QString("— W"));
        screen.setRadioState(true, 14074000, "DATA", false);
        QVERIFY(!slider->isEnabled());
        QCOMPARE(requests.count(), 1);
    }
    void receivedListSpace_data() {
        QTest::addColumn<QSize>("size");
        QTest::newRow("device") << QSize(360, 696);
        QTest::newRow("small-phone") << QSize(320, 568);
        QTest::newRow("tall-phone") << QSize(390, 800);
    }
    void receivedListSpace() {
        QFETCH(QSize, size);
        QTemporaryDir dir;
        QWidget host;
        host.resize(size);
        Ft8Screen screen(&host, dir.path());
        screen.resize(size);
        screen.show();
        host.show();
        screen.setPractice(true);
        QTest::qWait(80);
        auto *list = screen.findChild<QListWidget *>("ft8Activity");
        auto *waterfall = screen.findChild<Ft8Waterfall *>();
        auto *toggle = screen.findChild<QPushButton *>("ft8ToggleWaterfall");
        auto *card = screen.findChild<QWidget *>("ft8QsoCard");
        auto *next = screen.findChild<QPushButton *>("ft8Next");
        auto *clear = screen.findChild<QPushButton *>("ft8Clear");
        QVERIFY(list && waterfall && toggle && card && next && clear);
        QVector<Ft8::Decode> batch;
        for (int i = 0; i < 44; ++i) {
            auto d = decode(QString("CQ K1A%1%2 FN42").arg(QChar('A' + i / 26)).arg(QChar('A' + i % 26)));
            d.utc = QDateTime::currentDateTimeUtc();
            d.mode = screen.mode();
            d.practice = true;
            d.audioHz = 300 + i * 55;
            batch.append(d);
        }
        screen.addDecodes(batch);
        bool denseSelected = false;
        QTimer::singleShot(40, &screen, [&] {
            auto *dialog = screen.findChild<InWindowDialog *>();
            auto *choice = screen.findChild<QPushButton *>("ft8Density2");
            if (choice) {
                denseSelected = true;
                choice->click();
            } else if (dialog)
                dialog->reject();
        });
        screen.findChild<QPushButton *>("ft8Rows")->click();
        QVERIFY(denseSelected);
        if (waterfall->isHidden())
            toggle->click();
        QTest::qWait(40);
        auto visibleRows = [&] {
            int rows = 0;
            for (int i = 0; i < list->count(); ++i)
                if (list->viewport()->rect().contains(list->visualItemRect(list->item(i))))
                    ++rows;
            return rows;
        };
        auto save = [&](const QString &state) {
            return capture(screen).save(QString("ft8-list-%1-%2.png").arg(QTest::currentDataTag(), state));
        };
        QVERIFY(card->height() <= 60); // An idle QSO must not reserve exchange rows.
        QVERIFY(next->isHidden());
        QVERIFY(clear->isHidden());
        QCOMPARE(toggle->text(), "Hide waterfall");
        const int waterfallHeight = waterfall->height();
        const int shownRows = visibleRows();
        QVERIFY(shownRows >= (size.height() < 640 ? 6 : 8));
        QVERIFY(save("waterfall"));
        toggle->click();
        QTest::qWait(40);
        QCOMPARE(toggle->text(), "Show waterfall");
        const int hiddenRows = visibleRows();
        QVERIFY(hiddenRows >= (size.height() < 640 ? 11 : 17));
        QVERIFY(save("expanded"));
        const int idleListHeight = list->height();
        const auto *item = list->item(list->count() - 2);
        const QString call = Ft8::parseMessage(item->data(Qt::UserRole).value<Ft8::Decode>().message).from;
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, list->visualItemRect(item).center());
        QTest::qWait(40);
        QCOMPARE(screen.session().dxCall, call);
        QVERIFY(next->isVisible());
        QVERIFY(clear->isVisible());
        QVERIFY(card->height() <= 104);
        QVERIFY(idleListHeight - list->height() <= 46);
        QVERIFY(save("selected"));
        const int activeRows = visibleRows();
        for (const auto &name : {"ft8Auto", "ft8Period", "ft8LogQso", "ft8Clear", "ft8Next", "ft8Filter0", "ft8Filter1",
                                 "ft8Filter2", "ft8Logbook", "ft8Receive", "ft8Cq", "ft8Call", "ft8Halt"}) {
            auto *control = screen.findChild<QPushButton *>(name);
            QVERIFY(control && control->isVisible());
            QVERIFY(control->height() >= 28);
            QVERIFY2(screen.rect().contains(QRect(control->mapTo(&screen, QPoint()), control->size())), name);
            if (card->isAncestorOf(control)) {
                const QRect bounds(control->mapTo(card, QPoint()), control->size());
                QVERIFY2(card->rect().contains(bounds),
                         qPrintable(QString("%1 extends beyond the QSO panel: y=%2 height=%3 panel=%4")
                                        .arg(name).arg(bounds.y()).arg(bounds.height()).arg(card->height())));
            }
        }
        auto *period = screen.findChild<QPushButton *>("ft8Period");
        const bool even = screen.session().even;
        period->click();
        QCOMPARE(screen.session().even, !even);
        auto *automatic = screen.findChild<QPushButton *>("ft8Auto");
        automatic->click();
        QVERIFY(!screen.session().autoSequence);
        clear->click();
        QTest::qWait(40);
        QVERIFY(screen.session().dxCall.isEmpty());
        QVERIFY(save("cleared"));
        QCOMPARE(list->height(), idleListHeight);
        toggle->click();
        QTest::qWait(40);
        QCOMPARE(waterfall->height(), waterfallHeight);
        qInfo() << size << "dense rows: waterfall" << shownRows << "hidden" << hiddenRows << "QSO selected"
                << activeRows;
    }
    void transmitProtectionSurvivesActivity() {
        QTemporaryDir dir;
        Ft8Screen screen(nullptr, dir.path());
        screen.resize(320, 568);
        screen.setRadioState(true, 14074000, "DATA", false);
        screen.show();
        const QString fault = "TX stopped: fresh K4 ALC readings are unavailable. Check the connection and metering.";
        screen.setTransmitProtection(fault, true);
        screen.addDecodes({decode("CQ K1ABC FN42")});
        QTest::qWait(60);
        auto *banner = screen.findChild<QLabel *>("ft8Banner");
        QVERIFY(banner && banner->isVisible());
        QCOMPARE(banner->text(), fault);
        QVERIFY(banner->height() >= banner->heightForWidth(banner->width()));
        QVERIFY(capture(screen).save("ft8-protection-preview.png"));
        QSignalSpy requested(&screen, &Ft8Screen::audioSetupRequested);
        bool found = false;
        QTimer::singleShot(50, &screen, [&] {
            auto *setup = screen.findChild<QPushButton *>("ft8AudioSetup");
            if (setup) {
                found = setup->isVisible() && setup->isEnabled();
                setup->click();
            } else if (auto *dialog = screen.findChild<InWindowDialog *>()) dialog->reject();
        });
        screen.findChild<QPushButton *>("ft8Options")->click();
        QVERIFY(found);
        QTRY_COMPARE(requested.size(), 1);
    }
    void practiceIdentityAndOptions() {
        QTemporaryDir dir;
        Ft8Screen screen(nullptr, dir.path());
        screen.resize(320, 568);
        screen.show();
        QTest::qWait(60);
        bool found = false;
        auto editOptions = [&](const QString &call, const QString &grid, bool practice) {
            found = false;
            QTimer::singleShot(60, &screen, [&, call, grid, practice] {
                auto *dialog = screen.findChild<InWindowDialog *>();
                auto *callField = screen.findChild<QLineEdit *>("ft8MyCall");
                auto *gridField = screen.findChild<QLineEdit *>("ft8MyGrid");
                auto *practiceField = screen.findChild<QCheckBox *>("ft8PracticeOption");
                auto *save = screen.findChild<QPushButton *>("ft8SaveOptions");
                if (!dialog)
                    return;
                if (!callField || !gridField || !practiceField || !save) {
                    dialog->reject();
                    return;
                }
                found = screen.rect().contains(QRect(save->mapTo(&screen, QPoint()), save->size()));
                capture(screen).save("ft8-options-preview.png");
                callField->setText(call);
                gridField->setText(grid);
                practiceField->setChecked(practice);
                save->click();
                if (dialog->isVisible())
                    dialog->reject();
            });
            QTest::mouseClick(screen.findChild<QPushButton *>("ft8Options"), Qt::LeftButton);
        };
        editOptions("K2XYZ", "EM10", false);
        QVERIFY(found);
        QCOMPARE(screen.session().myCall, "K2XYZ");
        editOptions("K2XYZ", "EM10", true);
        QVERIFY(found);
        QVERIFY(screen.practice());
        editOptions("N0CALL", "EM00", true);
        QVERIFY(found);
        QCOMPARE(screen.session().myCall, "N0CALL");
        editOptions("N0CALL", "EM00", false);
        QVERIFY(found);
        QVERIFY(!screen.practice());
        QCOMPARE(screen.session().myCall, "K2XYZ");
        QCOMPARE(screen.session().myGrid, "EM10");
        QVERIFY(!QFile::exists(dir.path() + "/contacts.json"));
    }
};
QTEST_MAIN(Ft8Test)
#include "test_ft8.moc"
