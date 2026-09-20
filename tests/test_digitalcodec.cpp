#include <QtTest>
#include <cmath>
#include "audio/opusencoder.h"

class DigitalCodecTest : public QObject {
    Q_OBJECT
private slots:
    void streamedToneHeadroom_data() {
        QTest::addColumn<int>("frameSamples");
        for (int n : {240, 480, 720, 1440}) QTest::newRow(qPrintable(QString::number(n))) << n;
    }
    void streamedToneHeadroom() {
        QFETCH(int, frameSamples);
        OpusEncoder encoder;
        QVERIFY(encoder.initialize());
        for (int hz : {100, 300, 650, 1000, 1200, 1500, 1900, 2300, 2700, 3000, 3200}) {
            QVERIFY(encoder.reset());
            int error = 0;
            auto *decoder = opus_decoder_create(12000, 1, &error);
            QVERIFY(decoder && error == OPUS_OK);
            QByteArray pcm(frameSamples * 2, '\0');
            QVector<float> decoded(frameSamples);
            // Actual encode + independent decode across tone/packet boundaries.
            for (int frame = 0; frame < 20; ++frame) {
                auto *samples = reinterpret_cast<qint16 *>(pcm.data());
                for (int i = 0; i < frameSamples; ++i)
                    samples[i] = qRound(13106 * std::sin(2 * M_PI * hz * (frame * frameSamples + i) / 12000.0));
                const auto packet = encoder.encode(pcm, frameSamples, true);
                QVERIFY2(!packet.isEmpty(), qPrintable(QString("Rejected safe tone %1 Hz / %2 samples").arg(hz).arg(frameSamples)));
                QCOMPARE(opus_decode_float(decoder, reinterpret_cast<const unsigned char *>(packet.constData()),
                                            packet.size(), decoded.data(), frameSamples, 0), frameSamples);
                for (auto sample : decoded) QVERIFY(std::isfinite(sample) && std::abs(sample) <= 0.70710678f);
            }
            opus_decoder_destroy(decoder);
        }
    }
    void lowerDriveSurvivesCodec() {
        constexpr int count = 240;
        double previousRms = 1.0;
        for (float gain : {1.0f / 32, 1.0f / 128, 1.0f / 1024}) {
            OpusEncoder encoder;
            QVERIFY(encoder.initialize());
            int error = 0;
            auto *decoder = opus_decoder_create(12000, 1, &error);
            QVERIFY(decoder && error == OPUS_OK);
            QByteArray pcm(count * 2, '\0');
            QVector<float> decoded(count);
            double energy = 0;
            for (int frame = 0; frame < 30; ++frame) {
                auto *samples = reinterpret_cast<qint16 *>(pcm.data());
                for (int i = 0; i < count; ++i)
                    samples[i] = qRound(26213 * gain * std::sin(2 * M_PI * 1500 * i / 12000.0));
                const auto packet = encoder.encode(pcm, count, true);
                QVERIFY(!packet.isEmpty());
                QCOMPARE(opus_decode_float(decoder, reinterpret_cast<const unsigned char *>(packet.constData()),
                                           packet.size(), decoded.data(), count, 0), count);
                if (frame >= 10)
                    for (float sample : decoded) energy += sample * sample;
            }
            opus_decoder_destroy(decoder);
            const double rms = std::sqrt(energy / (20 * count));
            QVERIFY(rms > 0);
            QVERIFY(rms < previousRms / 2); // Attenuation must remain attenuation after encoding.
            previousRms = rms;
        }
    }
    void excessiveCodecOutputIsRejected() {
        OpusEncoder encoder;
        QVERIFY(encoder.initialize());
        QByteArray pcm(480, '\0');
        auto *samples = reinterpret_cast<qint16 *>(pcm.data());
        for (int i = 0; i < 240; ++i) samples[i] = qRound(30000 * std::sin(2 * M_PI * 1500 * i / 12000.0));
        QVERIFY(!encoder.encode(pcm, 240, false).isEmpty()); // Existing voice path remains available.
        QVERIFY(encoder.reset());
        QVERIFY(encoder.encode(pcm, 240, true).isEmpty());
        QVERIFY(encoder.initialize()); // Reinitialization and monitor reset are supported.
        QVERIFY(encoder.reset());
    }
};
QTEST_GUILESS_MAIN(DigitalCodecTest)
#include "test_digitalcodec.moc"
