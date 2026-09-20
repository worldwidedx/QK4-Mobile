#include "sstv/sstvencoder.h"
#include "sstv/sstvdecoder.h"

#include <QtTest>
#include <QtMath>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace {
struct VisFixtureOptions {
    bool includeFirstLeader = true;
    bool weakSecondLeader = false;
    int fadedVisBit = -1;
    double offsetHz = 0.0;
    double driftHz = 0.0;
    double snrDb = 100.0;
    double inBandInterferer = 0.0;
    bool impulses = false;
    int lineSyncCount = 0;
    double tailMs = 80.0;
};

QVector<float> makeVisFixture(const SstvModeSpec &mode,
                              const VisFixtureOptions &options = {}) {
    QVector<float> samples;
    samples.reserve(15000);
    double phase = 0.0;
    double interferencePhase = 0.0;
    qint64 generated = 0;
    quint32 randomState = 0x9e3779b9U ^ static_cast<quint32>(mode.visCode);
    constexpr double pi = 3.14159265358979323846;
    const double signalAmplitude = 0.70;
    const double noiseSigma = options.snrDb >= 90.0
        ? 0.0
        : signalAmplitude / qSqrt(2.0 * qPow(10.0, options.snrDb / 10.0));
    auto gaussianNoise = [&]() {
        double sum = 0.0;
        for (int i = 0; i < 12; ++i) {
            randomState = 1664525U * randomState + 1013904223U;
            sum += static_cast<double>((randomState >> 8) & 0xffffU) / 65535.0;
        }
        return sum - 6.0;
    };
    auto appendTone = [&](double nominalHz, double durationMs) {
        const int count = qRound(durationMs * SstvDecoder::SampleRate / 1000.0);
        for (int i = 0; i < count; ++i, ++generated) {
            const double progress = qBound(0.0,
                static_cast<double>(generated) / (0.95 * SstvDecoder::SampleRate), 1.0);
            const double frequency = nominalHz + options.offsetHz
                                     + options.driftHz * progress;
            phase += 2.0 * pi * frequency / SstvDecoder::SampleRate;
            interferencePhase += 2.0 * pi * 1650.0 / SstvDecoder::SampleRate;
            if (phase >= 2.0 * pi) phase -= 2.0 * pi;
            if (interferencePhase >= 2.0 * pi) interferencePhase -= 2.0 * pi;
            double sample = signalAmplitude * qSin(phase)
                            + options.inBandInterferer * qSin(interferencePhase)
                            + noiseSigma * gaussianNoise();
            if (options.impulses && (generated % 503) == 0)
                sample += ((generated / 503) & 1) ? 1.2 : -1.2;
            samples.append(static_cast<float>(sample));
        }
    };

    if (options.includeFirstLeader) {
        appendTone(1900.0, 300.0);
        appendTone(1200.0, 10.0);
    }
    if (options.weakSecondLeader) {
        // Damage most of the leading half without reducing the protocol-timed
        // second-leader evidence to a coincidental short tone burst.
        appendTone(1500.0, 160.0);
        appendTone(1900.0, 140.0);
    } else {
        appendTone(1900.0, 300.0);
    }
    appendTone(1200.0, 30.0);
    int parity = 0;
    for (int bit = 0; bit < 7; ++bit) {
        const bool one = (mode.visCode & (1 << bit)) != 0;
        const double tone = one ? 1100.0 : 1300.0;
        if (bit == options.fadedVisBit) {
            appendTone(tone, 12.0);
            appendTone(1500.0, 6.0);
            appendTone(tone, 12.0);
        } else {
            appendTone(tone, 30.0);
        }
        parity ^= one ? 1 : 0;
    }
    appendTone(parity ? 1100.0 : 1300.0, 30.0);
    appendTone(1200.0, 30.0);
    if (options.lineSyncCount > 0) {
        appendTone(1500.0, 25.0);
        for (int pulse = 0; pulse < options.lineSyncCount; ++pulse) {
            appendTone(1200.0, mode.lineSyncMs);
            appendTone(1500.0, mode.lineTimeMs - mode.lineSyncMs);
        }
    }
    appendTone(1500.0, options.tailMs);
    return samples;
}

void feedInChunks(SstvDecoder &decoder, const QVector<float> &samples, int chunk = 1200) {
    for (int offset = 0; offset < samples.size(); offset += chunk)
        decoder.consumeMono(samples.mid(offset, qMin(chunk, samples.size() - offset)));
}

bool readCapturedPcm16Wav(const QString &path, QVector<float> *samples, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = file.errorString();
        return false;
    }
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    const QByteArray riff = file.read(4);
    quint32 riffSize = 0;
    stream >> riffSize;
    Q_UNUSED(riffSize)
    const QByteArray wave = file.read(4);
    if (riff != QByteArrayLiteral("RIFF") || wave != QByteArrayLiteral("WAVE")) {
        *error = QStringLiteral("not a RIFF/WAVE file");
        return false;
    }

    quint16 format = 0;
    quint16 channels = 0;
    quint32 sampleRate = 0;
    quint16 bits = 0;
    QByteArray pcm;
    while (!stream.atEnd()) {
        const QByteArray chunkId = file.read(4);
        if (chunkId.size() != 4)
            break;
        quint32 chunkSize = 0;
        stream >> chunkSize;
        if (chunkId == QByteArrayLiteral("fmt ")) {
            if (chunkSize < 16) {
                *error = QStringLiteral("short fmt chunk");
                return false;
            }
            quint32 byteRate = 0;
            quint16 blockAlign = 0;
            stream >> format >> channels >> sampleRate >> byteRate >> blockAlign >> bits;
            Q_UNUSED(byteRate)
            Q_UNUSED(blockAlign)
            if (chunkSize > 16)
                file.seek(file.pos() + chunkSize - 16);
        } else if (chunkId == QByteArrayLiteral("data")) {
            pcm = file.read(chunkSize);
        } else {
            file.seek(file.pos() + chunkSize);
        }
        if ((chunkSize & 1U) != 0)
            file.seek(file.pos() + 1);
    }
    if (format != 1 || channels != 1 || sampleRate != SstvDecoder::SampleRate
        || bits != 16 || pcm.isEmpty() || (pcm.size() & 1) != 0) {
        *error = QStringLiteral("expected mono PCM16 at 12000 Hz");
        return false;
    }

    samples->resize(pcm.size() / 2);
    const uchar *bytes = reinterpret_cast<const uchar *>(pcm.constData());
    for (qsizetype i = 0; i < samples->size(); ++i) {
        const quint16 packed = static_cast<quint16>(bytes[2 * i])
                               | (static_cast<quint16>(bytes[2 * i + 1]) << 8);
        (*samples)[i] = static_cast<float>(static_cast<qint16>(packed)) / 32768.0f;
    }
    return true;
}
}

class SstvEncoderTest : public QObject {
    Q_OBJECT
private slots:
    void registryHasUniqueVisCodes();
    void defaultModeIsScottieS1();
    void sequentialEncoderProducesContinuousFrames();
    void robot36UsesCanonicalChromaOrder();
    void morseIdIsAppendedAtRequestedWpm();
    void postImageFskAndCwIdsRoundTrip();
    void txGuardIntervalsWrapTheCompleteSignal();
    void allRegisteredModesEncodeToExpectedDuration_data();
    void allRegisteredModesEncodeToExpectedDuration();
    void encoderDecoderRoundTrip_data();
    void encoderDecoderRoundTrip();
    void visDetectorToleratesOffsetAndDropouts();
    void visDetectorRecoversFromPartialHeader();
    void visDetectorIntegratesWeakSecondLeader();
    void visDetectorSearchesDelayedVisBoundary();
    void visDetectorCorrelatesAfterStateChurn();
    void visDetectorAcceptsLongFirstLeader();
    void secondLeaderRecoveryRequiresThreeSyncs();
    void allModesAcquireAtFiveDb_data();
    void allModesAcquireAtFiveDb();
    void visAcquisitionNoiseThreshold_data();
    void visAcquisitionNoiseThreshold();
    void visAcquisitionSurvivesHeaderDamage_data();
    void visAcquisitionSurvivesHeaderDamage();
    void unconfirmedVisReturnsToAutoRx();
    void singleLineSyncDoesNotConfirmAcquisition();
    void visAcquisitionDoesNotFalseStartOnNoise();
    void capturedFalseTriggersAreRejected();
    void idleSearchHistoryRemainsBoundedAndRecovers();
    void autoSlantCorrectsSampleClockError();
    void weakSignalRejectsOutOfBandInterference();
    void imageAfcTracksFrequencyDrift();
    void rejectsWrongFrameSize();
};

void SstvEncoderTest::registryHasUniqueVisCodes() {
    QCOMPARE(SstvModeRegistry::all().size(), 22);
    QSet<int> seen;
    for (const SstvModeSpec &mode : SstvModeRegistry::all()) {
        QVERIFY(mode.encoderImplemented);
        QVERIFY(mode.decoderImplemented);
        QVERIFY(mode.lineTimeMs > 0.0);
        QVERIFY2(!seen.contains(mode.visCode), qPrintable(mode.displayName));
        seen.insert(mode.visCode);
        QCOMPARE(SstvModeRegistry::findByVis(mode.visCode)->id, mode.id);
    }
}

void SstvEncoderTest::txGuardIntervalsWrapTheCompleteSignal() {
    const SstvModeSpec *mode = SstvModeRegistry::find(SstvModeId::ScottieS1);
    QVERIFY(mode);
    QImage image(mode->width, mode->height, QImage::Format_RGB32);
    image.fill(qRgb(48, 112, 208));

    QString error;
    SstvEncoder baseline;
    QVERIFY2(baseline.begin(image, mode->id, &error), qPrintable(error));

    constexpr int preRollMs = 400;
    constexpr int postRollMs = 300;
    SstvEncoder guarded;
    QVERIFY2(guarded.begin(image, mode->id, &error, QString(), 20, QString(),
                           preRollMs, postRollMs), qPrintable(error));
    QCOMPARE(guarded.totalSamples() - baseline.totalSamples(),
             (preRollMs + postRollMs) * SstvEncoder::SampleRate / 1000);

    const QVector<qint16> beginning = guarded.nextSamples(
        preRollMs * SstvEncoder::SampleRate / 1000);
    QCOMPARE(beginning.size(), preRollMs * SstvEncoder::SampleRate / 1000);
    QVERIFY(std::all_of(beginning.cbegin(), beginning.cend(), [](qint16 sample) {
        return sample == 0;
    }));

    const QVector<qint16> signal = guarded.nextSamples(baseline.totalSamples());
    QCOMPARE(signal.size(), baseline.totalSamples());
    const QVector<qint16> ending = guarded.nextSamples(
        postRollMs * SstvEncoder::SampleRate / 1000);
    QCOMPARE(ending.size(), postRollMs * SstvEncoder::SampleRate / 1000);
    QVERIFY(std::all_of(ending.cbegin(), ending.cend(), [](qint16 sample) {
        return sample == 0;
    }));
}

void SstvEncoderTest::allRegisteredModesEncodeToExpectedDuration_data() {
    QTest::addColumn<int>("modeId");
    for (const SstvModeSpec &mode : SstvModeRegistry::all())
        QTest::newRow(qPrintable(mode.displayName)) << static_cast<int>(mode.id);
}

void SstvEncoderTest::allRegisteredModesEncodeToExpectedDuration() {
    QFETCH(int, modeId);
    const SstvModeSpec *mode = SstvModeRegistry::find(static_cast<SstvModeId>(modeId));
    QVERIFY(mode);
    QImage image(mode->width, mode->height, QImage::Format_RGB32);
    image.fill(qRgb(32, 128, 224));
    SstvEncoder encoder;
    QString error;
    QVERIFY2(encoder.begin(image, mode->id, &error), qPrintable(error));

    const int scanLines = mode->colorFamily == SstvColorFamily::PdYuv
                              ? mode->height / 2 : mode->height;
    const double imageMs = scanLines * mode->lineTimeMs;
    const double encodedMs = 1000.0 * encoder.totalSamples() / SstvEncoder::SampleRate;
    QVERIFY2(qAbs(encodedMs - (imageMs + 910.0)) < 2.0,
             qPrintable(QStringLiteral("%1 encoded %2 ms, expected about %3 ms")
                            .arg(mode->displayName).arg(encodedMs).arg(imageMs + 910.0)));
}

void SstvEncoderTest::encoderDecoderRoundTrip() {
    QFETCH(int, modeId);
    const SstvModeSpec *mode = SstvModeRegistry::find(static_cast<SstvModeId>(modeId));
    QVERIFY(mode);
    QImage source(mode->width, mode->height, QImage::Format_RGB32);
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            source.setPixel(x, y, qRgb(40 + 120 * x / source.width(),
                                      70 + 130 * y / source.height(),
                                      220 - 100 * x / source.width()));
        }
    }

    SstvEncoder encoder;
    QString error;
    QVERIFY2(encoder.begin(source, mode->id, &error), qPrintable(error));
    SstvDecoder decoder;
    QSignalSpy modeSpy(&decoder, &SstvDecoder::modeDetected);
    QSignalSpy statusSpy(&decoder, &SstvDecoder::statusChanged);
    QSignalSpy completeSpy(&decoder, &SstvDecoder::imageCompleted);
    while (encoder.isActive()) {
        const QVector<qint16> encoded = encoder.nextSamples(2400);
        QVector<float> mono;
        mono.reserve(encoded.size());
        for (qint16 sample : encoded)
            mono.append(static_cast<float>(sample) / 32768.0f);
        decoder.consumeMono(mono);
    }
    // Production RX is continuous; provide a short post-image tail so the
    // streaming decoder can prove the final scan line is complete.
    decoder.consumeMono(QVector<float>(240, 0.0f));

    QStringList statuses;
    for (const QList<QVariant> &event : statusSpy)
        statuses.append(event.at(0).toString());
    QVERIFY2(modeSpy.size() == 1, qPrintable(statuses.join(QStringLiteral(" | "))));
    QCOMPARE(modeSpy.first().at(0).toInt(), modeId);
    QCOMPARE(completeSpy.size(), 1);
    const QImage decoded = qvariant_cast<QImage>(completeSpy.first().at(0));
    QCOMPARE(decoded.size(), source.size());
    const QPoint samplePoint(decoded.width() / 2, decoded.height() / 2);
    const QColor expected(source.pixel(samplePoint));
    const QColor center(decoded.pixel(samplePoint));
    QVERIFY2(qAbs(center.red() - expected.red()) < 30,
             qPrintable(QStringLiteral("decoded %1,%2,%3 expected %4,%5,%6")
                            .arg(center.red()).arg(center.green()).arg(center.blue())
                            .arg(expected.red()).arg(expected.green()).arg(expected.blue())));
    QVERIFY2(qAbs(center.green() - expected.green()) < 30,
             qPrintable(QStringLiteral("decoded %1,%2,%3 expected %4,%5,%6")
                            .arg(center.red()).arg(center.green()).arg(center.blue())
                            .arg(expected.red()).arg(expected.green()).arg(expected.blue())));
    QVERIFY2(qAbs(center.blue() - expected.blue()) < 30,
             qPrintable(QStringLiteral("decoded %1,%2,%3 expected %4,%5,%6")
                            .arg(center.red()).arg(center.green()).arg(center.blue())
                            .arg(expected.red()).arg(expected.green()).arg(expected.blue())));
    QVERIFY(completeSpy.first().at(2).toString().contains(QStringLiteral("LOCKED")));
}

void SstvEncoderTest::encoderDecoderRoundTrip_data() {
    QTest::addColumn<int>("modeId");
    for (const SstvModeSpec &mode : SstvModeRegistry::all())
        QTest::newRow(qPrintable(mode.displayName)) << static_cast<int>(mode.id);
}

void SstvEncoderTest::visDetectorToleratesOffsetAndDropouts() {
    const SstvModeSpec *mode = SstvModeRegistry::find(SstvModeId::ScottieS1);
    QVERIFY(mode);

    QVector<float> samples;
    double phase = 0.0;
    qint64 generated = 0;
    constexpr double offsetHz = 180.0;
    constexpr double pi = 3.14159265358979323846;
    auto appendTone = [&](double nominalHz, double durationMs) {
        const int count = qRound(durationMs * SstvDecoder::SampleRate / 1000.0);
        const double step = 2.0 * pi * (nominalHz + offsetHz) / SstvDecoder::SampleRate;
        for (int i = 0; i < count; ++i, ++generated) {
            // Short periodic fades model packet/radio noise without erasing a
            // real tone transition. They are deliberately shorter than the
            // detector's transition confirmation window.
            const bool faded = (generated % 480) < 6;
            samples.append(faded ? 0.0f : static_cast<float>(0.75 * qSin(phase)));
            phase += step;
            if (phase >= 2.0 * pi)
                phase -= 2.0 * pi;
        }
    };

    appendTone(1900.0, 300.0);
    appendTone(1200.0, 10.0);
    appendTone(1900.0, 300.0);
    appendTone(1200.0, 30.0);
    int parity = 0;
    for (int bit = 0; bit < 7; ++bit) {
        const bool one = (mode->visCode & (1 << bit)) != 0;
        appendTone(one ? 1100.0 : 1300.0, 30.0);
        parity ^= one ? 1 : 0;
    }
    appendTone(parity ? 1100.0 : 1300.0, 30.0);
    appendTone(1200.0, 30.0);
    appendTone(1500.0, 50.0);

    SstvDecoder decoder;
    QSignalSpy modeSpy(&decoder, &SstvDecoder::modeDetected);
    QSignalSpy statusSpy(&decoder, &SstvDecoder::statusChanged);
    for (int offset = 0; offset < samples.size(); offset += 2400)
        decoder.consumeMono(samples.mid(offset, qMin(2400, samples.size() - offset)));

    QStringList statuses;
    for (const QList<QVariant> &event : statusSpy)
        statuses.append(event.at(0).toString());
    QVERIFY2(modeSpy.size() == 1, qPrintable(statuses.join(QStringLiteral(" | "))));
    QCOMPARE(modeSpy.first().at(0).toInt(), static_cast<int>(mode->id));
}

void SstvEncoderTest::visDetectorRecoversFromPartialHeader() {
    const SstvModeSpec *mode = SstvModeRegistry::find(SstvModeId::ScottieS1);
    QVERIFY(mode);

    QVector<float> samples;
    double phase = 0.0;
    constexpr double pi = 3.14159265358979323846;
    auto appendTone = [&](double frequency, double durationMs) {
        const int count = qRound(durationMs * SstvDecoder::SampleRate / 1000.0);
        const double step = 2.0 * pi * frequency / SstvDecoder::SampleRate;
        for (int i = 0; i < count; ++i) {
            samples.append(static_cast<float>(0.75 * qSin(phase)));
            phase += step;
            if (phase >= 2.0 * pi)
                phase -= 2.0 * pi;
        }
    };
    auto appendHeader = [&]() {
        appendTone(1900.0, 300.0);
        appendTone(1200.0, 10.0);
        appendTone(1900.0, 300.0);
        appendTone(1200.0, 30.0);
        int parity = 0;
        for (int bit = 0; bit < 7; ++bit) {
            const bool one = (mode->visCode & (1 << bit)) != 0;
            appendTone(one ? 1100.0 : 1300.0, 30.0);
            parity ^= one ? 1 : 0;
        }
        appendTone(parity ? 1100.0 : 1300.0, 30.0);
        appendTone(1200.0, 30.0);
        appendTone(1500.0, 50.0);
    };

    // Model opening SSTV in the middle of image data that happens to resemble
    // the start of VIS but never completes within the protocol time window.
    appendTone(1900.0, 300.0);
    appendTone(1200.0, 10.0);
    appendTone(1900.0, 800.0);
    appendTone(1500.0, 100.0);
    appendHeader();

    SstvDecoder decoder;
    QSignalSpy modeSpy(&decoder, &SstvDecoder::modeDetected);
    QSignalSpy statusSpy(&decoder, &SstvDecoder::statusChanged);
    for (int offset = 0; offset < samples.size(); offset += 2400)
        decoder.consumeMono(samples.mid(offset, qMin(2400, samples.size() - offset)));

    QStringList statuses;
    for (const QList<QVariant> &event : statusSpy)
        statuses.append(event.at(0).toString());
    QVERIFY2(modeSpy.size() == 1, qPrintable(statuses.join(QStringLiteral(" | "))));
    QCOMPARE(modeSpy.first().at(0).toInt(), static_cast<int>(mode->id));
}

void SstvEncoderTest::visDetectorIntegratesWeakSecondLeader() {
    const SstvModeSpec *mode = SstvModeRegistry::find(SstvModeId::ScottieS1);
    QVERIFY(mode);

    QVector<float> samples;
    double phase = 0.0;
    constexpr double pi = 3.14159265358979323846;
    auto appendTone = [&](double frequency, double durationMs) {
        const int count = qRound(durationMs * SstvDecoder::SampleRate / 1000.0);
        const double step = 2.0 * pi * frequency / SstvDecoder::SampleRate;
        for (int i = 0; i < count; ++i) {
            samples.append(static_cast<float>(0.75 * qSin(phase)));
            phase += step;
            if (phase >= 2.0 * pi)
                phase -= 2.0 * pi;
        }
    };

    appendTone(1900.0, 300.0);
    appendTone(1200.0, 10.0);
    // No individual 1900 Hz run is long enough for the continuous-run gate,
    // but most of the protocol-timed second leader remains usable.
    for (int block = 0; block < 3; ++block) {
        appendTone(1900.0, 50.0);
        appendTone(1600.0, 50.0);
    }
    appendTone(1200.0, 30.0);
    int parity = 0;
    for (int bit = 0; bit < 7; ++bit) {
        const bool one = (mode->visCode & (1 << bit)) != 0;
        appendTone(one ? 1100.0 : 1300.0, 30.0);
        parity ^= one ? 1 : 0;
    }
    appendTone(parity ? 1100.0 : 1300.0, 30.0);
    appendTone(1200.0, 30.0);
    appendTone(1500.0, 50.0);

    SstvDecoder decoder;
    QSignalSpy modeSpy(&decoder, &SstvDecoder::modeDetected);
    QSignalSpy statusSpy(&decoder, &SstvDecoder::statusChanged);
    for (int offset = 0; offset < samples.size(); offset += 2400)
        decoder.consumeMono(samples.mid(offset, qMin(2400, samples.size() - offset)));

    QStringList statuses;
    for (const QList<QVariant> &event : statusSpy)
        statuses.append(event.at(0).toString());
    QVERIFY2(modeSpy.size() == 1, qPrintable(statuses.join(QStringLiteral(" | "))));
    QCOMPARE(modeSpy.first().at(0).toInt(), static_cast<int>(mode->id));
    QVERIFY2(statuses.join(QStringLiteral(" | ")).contains(QStringLiteral("WEAK LEADER")),
             qPrintable(statuses.join(QStringLiteral(" | "))));
}

void SstvEncoderTest::visDetectorSearchesDelayedVisBoundary() {
    const SstvModeSpec *mode = SstvModeRegistry::find(SstvModeId::ScottieS1);
    QVERIFY(mode);

    QVector<float> samples;
    double phase = 0.0;
    constexpr double pi = 3.14159265358979323846;
    auto appendTone = [&](double frequency, double durationMs) {
        const int count = qRound(durationMs * SstvDecoder::SampleRate / 1000.0);
        const double step = 2.0 * pi * frequency / SstvDecoder::SampleRate;
        for (int i = 0; i < count; ++i) {
            samples.append(static_cast<float>(0.75 * qSin(phase)));
            phase += step;
            if (phase >= 2.0 * pi)
                phase -= 2.0 * pi;
        }
    };

    appendTone(1900.0, 300.0);
    appendTone(1200.0, 10.0);
    // The integrated path predicts VIS at 310 ms. Delay the real boundary to
    // model a faded live transition whose detected leader edge is imprecise.
    for (int block = 0; block < 6; ++block) {
        appendTone(1900.0, 50.0);
        appendTone(1600.0, 10.0);
    }
    appendTone(1900.0, 5.0);
    appendTone(1200.0, 30.0);
    int parity = 0;
    for (int bit = 0; bit < 7; ++bit) {
        const bool one = (mode->visCode & (1 << bit)) != 0;
        appendTone(one ? 1100.0 : 1300.0, 30.0);
        parity ^= one ? 1 : 0;
    }
    appendTone(parity ? 1100.0 : 1300.0, 30.0);
    appendTone(1200.0, 30.0);
    appendTone(1500.0, 50.0);

    SstvDecoder decoder;
    QSignalSpy modeSpy(&decoder, &SstvDecoder::modeDetected);
    for (int offset = 0; offset < samples.size(); offset += 2400)
        decoder.consumeMono(samples.mid(offset, qMin(2400, samples.size() - offset)));

    QCOMPARE(modeSpy.size(), 1);
    QCOMPARE(modeSpy.first().at(0).toInt(), static_cast<int>(mode->id));
}

void SstvEncoderTest::visDetectorCorrelatesAfterStateChurn() {
    const SstvModeSpec *mode = SstvModeRegistry::find(SstvModeId::ScottieS2);
    QVERIFY(mode);

    QVector<float> samples;
    double phase = 0.0;
    constexpr double pi = 3.14159265358979323846;
    auto appendTone = [&](double frequency, double durationMs) {
        const int count = qRound(durationMs * SstvDecoder::SampleRate / 1000.0);
        const double step = 2.0 * pi * frequency / SstvDecoder::SampleRate;
        for (int i = 0; i < count; ++i) {
            samples.append(static_cast<float>(0.75 * qSin(phase)));
            phase += step;
            if (phase >= 2.0 * pi)
                phase -= 2.0 * pi;
        }
    };
    auto appendValidHeader = [&]() {
        appendTone(1900.0, 300.0);
        appendTone(1200.0, 10.0);
        appendTone(1900.0, 300.0);
        appendTone(1200.0, 30.0);
        int parity = 0;
        for (int bit = 0; bit < 7; ++bit) {
            const bool one = (mode->visCode & (1 << bit)) != 0;
            appendTone(one ? 1100.0 : 1300.0, 30.0);
            parity ^= one ? 1 : 0;
        }
        appendTone(parity ? 1100.0 : 1300.0, 30.0);
        appendTone(1200.0, 30.0);
    };

    // A plausible leader sequence followed by a bad VIS word puts the
    // run-based detector into transition recovery. End that bad word on 1900
    // Hz and begin the real header without a tone edge, reproducing the live
    // state churn that can hide the real first leader.
    appendTone(1900.0, 300.0);
    appendTone(1200.0, 10.0);
    appendTone(1900.0, 300.0);
    appendTone(1500.0, 270.0);
    appendTone(1900.0, 30.0);
    appendValidHeader();
    appendTone(1500.0, 60.0);

    SstvDecoder decoder;
    QSignalSpy modeSpy(&decoder, &SstvDecoder::modeDetected);
    QSignalSpy statusSpy(&decoder, &SstvDecoder::statusChanged);
    for (int offset = 0; offset < samples.size(); offset += 2400)
        decoder.consumeMono(samples.mid(offset, qMin(2400, samples.size() - offset)));

    QStringList statuses;
    for (const QList<QVariant> &event : statusSpy)
        statuses.append(event.at(0).toString());
    QVERIFY2(modeSpy.size() == 1, qPrintable(statuses.join(QStringLiteral(" | "))));
    QCOMPARE(modeSpy.first().at(0).toInt(), static_cast<int>(mode->id));
    QVERIFY2(statuses.join(QStringLiteral(" | ")).contains(QStringLiteral("RECOVERY")),
             qPrintable(statuses.join(QStringLiteral(" | "))));
}

void SstvEncoderTest::secondLeaderRecoveryRequiresThreeSyncs() {
    const SstvModeSpec *mode = SstvModeRegistry::find(SstvModeId::Robot36);
    QVERIFY(mode);
    VisFixtureOptions options;
    options.includeFirstLeader = false;
    options.lineSyncCount = 3;
    const QVector<float> samples = makeVisFixture(*mode, options);

    SstvDecoder decoder;
    QSignalSpy modeSpy(&decoder, &SstvDecoder::modeDetected);
    QSignalSpy statusSpy(&decoder, &SstvDecoder::statusChanged);
    feedInChunks(decoder, samples);

    QStringList statuses;
    for (const QList<QVariant> &event : statusSpy)
        statuses.append(event.at(0).toString());
    QCOMPARE(modeSpy.size(), 1);
    QCOMPARE(modeSpy.first().at(0).toInt(), static_cast<int>(mode->id));
    QVERIFY2(statuses.join(QStringLiteral(" | ")).contains(
                 QStringLiteral("SYNC CONFIRMED")),
             qPrintable(statuses.join(QStringLiteral(" | "))));
}

void SstvEncoderTest::allModesAcquireAtFiveDb_data() {
    QTest::addColumn<int>("modeId");
    for (const SstvModeSpec &mode : SstvModeRegistry::all())
        QTest::newRow(qPrintable(mode.displayName)) << static_cast<int>(mode.id);
}

void SstvEncoderTest::allModesAcquireAtFiveDb() {
    QFETCH(int, modeId);
    const SstvModeSpec *mode = SstvModeRegistry::find(static_cast<SstvModeId>(modeId));
    QVERIFY(mode);
    VisFixtureOptions options;
    options.snrDb = 5.0;
    const QVector<float> samples = makeVisFixture(*mode, options);

    SstvDecoder decoder;
    QSignalSpy modeSpy(&decoder, &SstvDecoder::modeDetected);
    feedInChunks(decoder, samples);
    QCOMPARE(modeSpy.size(), 1);
    QCOMPARE(modeSpy.first().at(0).toInt(), modeId);
}

void SstvEncoderTest::visAcquisitionNoiseThreshold_data() {
    QTest::addColumn<double>("snrDb");
    QTest::addColumn<bool>("required");
    QTest::newRow("20 dB") << 20.0 << true;
    QTest::newRow("10 dB") << 10.0 << true;
    QTest::newRow("5 dB") << 5.0 << true;
    QTest::newRow("0 dB characterization") << 0.0 << false;
    QTest::newRow("-5 dB characterization") << -5.0 << false;
}

void SstvEncoderTest::visAcquisitionNoiseThreshold() {
    QFETCH(double, snrDb);
    QFETCH(bool, required);
    const SstvModeSpec *mode = SstvModeRegistry::find(SstvModeId::Robot36);
    QVERIFY(mode);
    VisFixtureOptions options;
    options.snrDb = snrDb;
    const QVector<float> samples = makeVisFixture(*mode, options);

    SstvDecoder decoder;
    QSignalSpy modeSpy(&decoder, &SstvDecoder::modeDetected);
    feedInChunks(decoder, samples);
    if (required)
        QCOMPARE(modeSpy.size(), 1);
    else
        QVERIFY(modeSpy.size() <= 1);
    if (!modeSpy.isEmpty())
        QCOMPARE(modeSpy.first().at(0).toInt(), static_cast<int>(mode->id));
}

void SstvEncoderTest::visAcquisitionSurvivesHeaderDamage_data() {
    QTest::addColumn<bool>("includeFirstLeader");
    QTest::addColumn<bool>("weakSecondLeader");
    QTest::addColumn<int>("fadedVisBit");
    QTest::addColumn<double>("offsetHz");
    QTest::addColumn<double>("driftHz");
    QTest::addColumn<double>("interferer");
    QTest::addColumn<bool>("impulses");
    QTest::newRow("clipped first leader") << false << false << -1 << 0.0 << 0.0 << 0.0 << false;
    QTest::newRow("weak second leader") << true << true << -1 << 0.0 << 0.0 << 0.0 << false;
    QTest::newRow("faded VIS bit") << true << false << 3 << 0.0 << 0.0 << 0.0 << false;
    QTest::newRow("positive AFC edge") << false << false << -1 << 330.0 << 0.0 << 0.0 << false;
    QTest::newRow("negative AFC edge") << false << false << -1 << -330.0 << 0.0 << 0.0 << false;
    QTest::newRow("header drift") << false << false << -1 << -120.0 << 240.0 << 0.0 << false;
    QTest::newRow("in-band interferer") << false << false << -1 << 0.0 << 0.0 << 0.18 << false;
    QTest::newRow("impulse bursts") << false << false << -1 << 0.0 << 0.0 << 0.0 << true;
}

void SstvEncoderTest::visAcquisitionSurvivesHeaderDamage() {
    QFETCH(bool, includeFirstLeader);
    QFETCH(bool, weakSecondLeader);
    QFETCH(int, fadedVisBit);
    QFETCH(double, offsetHz);
    QFETCH(double, driftHz);
    QFETCH(double, interferer);
    QFETCH(bool, impulses);
    const SstvModeSpec *mode = SstvModeRegistry::find(SstvModeId::ScottieS2);
    QVERIFY(mode);
    VisFixtureOptions options;
    options.includeFirstLeader = includeFirstLeader;
    options.weakSecondLeader = weakSecondLeader;
    options.fadedVisBit = fadedVisBit;
    options.offsetHz = offsetHz;
    options.driftHz = driftHz;
    options.inBandInterferer = interferer;
    options.impulses = impulses;
    options.snrDb = 18.0;
    options.lineSyncCount = 3;
    const QVector<float> samples = makeVisFixture(*mode, options);

    SstvDecoder decoder;
    QSignalSpy modeSpy(&decoder, &SstvDecoder::modeDetected);
    feedInChunks(decoder, samples);
    QCOMPARE(modeSpy.size(), 1);
    QCOMPARE(modeSpy.first().at(0).toInt(), static_cast<int>(mode->id));
}

void SstvEncoderTest::unconfirmedVisReturnsToAutoRx() {
    const SstvModeSpec *mode = SstvModeRegistry::find(SstvModeId::Robot36);
    QVERIFY(mode);
    VisFixtureOptions options;
    options.includeFirstLeader = false;
    options.tailMs = 900.0;
    const QVector<float> samples = makeVisFixture(*mode, options);

    SstvDecoder decoder;
    QSignalSpy modeSpy(&decoder, &SstvDecoder::modeDetected);
    QSignalSpy statusSpy(&decoder, &SstvDecoder::statusChanged);
    feedInChunks(decoder, samples);
    QCOMPARE(modeSpy.size(), 0);
    QStringList statuses;
    for (const QList<QVariant> &event : statusSpy)
        statuses.append(event.at(0).toString());
    QVERIFY2(statuses.join(QStringLiteral(" | ")).contains(QStringLiteral("NO LINE SYNC")),
             qPrintable(statuses.join(QStringLiteral(" | "))));
}

void SstvEncoderTest::singleLineSyncDoesNotConfirmAcquisition() {
    const SstvModeSpec *mode = SstvModeRegistry::find(SstvModeId::Robot36);
    QVERIFY(mode);
    VisFixtureOptions options;
    options.includeFirstLeader = false;
    options.lineSyncCount = 1;
    options.tailMs = 900.0;
    const QVector<float> samples = makeVisFixture(*mode, options);

    SstvDecoder decoder;
    QSignalSpy modeSpy(&decoder, &SstvDecoder::modeDetected);
    QSignalSpy statusSpy(&decoder, &SstvDecoder::statusChanged);
    feedInChunks(decoder, samples);
    QCOMPARE(modeSpy.size(), 0);
    QStringList statuses;
    for (const QList<QVariant> &event : statusSpy)
        statuses.append(event.at(0).toString());
    QVERIFY2(statuses.join(QStringLiteral(" | ")).contains(QStringLiteral("NO LINE SYNC")),
             qPrintable(statuses.join(QStringLiteral(" | "))));
}

void SstvEncoderTest::visAcquisitionDoesNotFalseStartOnNoise() {
    QVector<float> samples;
    samples.reserve(2 * SstvDecoder::SampleRate);
    quint32 state = 0x12345678U;
    double phase = 0.0;
    constexpr double pi = 3.14159265358979323846;
    for (int i = 0; i < 2 * SstvDecoder::SampleRate; ++i) {
        state = 1664525U * state + 1013904223U;
        const double noise = (static_cast<double>((state >> 8) & 0xffffU) / 32767.5) - 1.0;
        phase += 2.0 * pi * 1650.0 / SstvDecoder::SampleRate;
        if (phase >= 2.0 * pi) phase -= 2.0 * pi;
        samples.append(static_cast<float>(0.50 * noise + 0.25 * qSin(phase)));
    }
    SstvDecoder decoder;
    QSignalSpy modeSpy(&decoder, &SstvDecoder::modeDetected);
    feedInChunks(decoder, samples);
    QCOMPARE(modeSpy.size(), 0);
}

void SstvEncoderTest::capturedFalseTriggersAreRejected() {
    const QString fixtureRoot = qEnvironmentVariable("QK4_SSTV_NEGATIVE_FIXTURE_DIR");
    if (fixtureRoot.isEmpty())
        QSKIP("Set QK4_SSTV_NEGATIVE_FIXTURE_DIR to replay private field captures");

    const QDir directory(fixtureRoot);
    const QStringList files = directory.entryList({QStringLiteral("*.wav")},
                                                  QDir::Files, QDir::Name);
    QVERIFY2(!files.isEmpty(), qPrintable(QStringLiteral("no WAV fixtures in %1")
                                             .arg(directory.absolutePath())));
    for (const QString &name : files) {
        QVector<float> samples;
        QString error;
        QVERIFY2(readCapturedPcm16Wav(directory.filePath(name), &samples, &error),
                 qPrintable(QStringLiteral("%1: %2").arg(name, error)));
        SstvDecoder decoder;
        QSignalSpy modeSpy(&decoder, &SstvDecoder::modeDetected);
        QSignalSpy completeSpy(&decoder, &SstvDecoder::imageCompleted);
        feedInChunks(decoder, samples);
        QVERIFY2(modeSpy.isEmpty(),
                 qPrintable(QStringLiteral("%1 emitted %2 mode events")
                                .arg(name).arg(modeSpy.size())));
        QVERIFY2(completeSpy.isEmpty(),
                 qPrintable(QStringLiteral("%1 completed %2 images")
                                .arg(name).arg(completeSpy.size())));
    }
}

void SstvEncoderTest::visDetectorAcceptsLongFirstLeader() {
    const SstvModeSpec *mode = SstvModeRegistry::find(SstvModeId::ScottieS1);
    QVERIFY(mode);

    QVector<float> samples;
    double phase = 0.0;
    constexpr double pi = 3.14159265358979323846;
    auto appendTone = [&](double frequency, double durationMs) {
        const int count = qRound(durationMs * SstvDecoder::SampleRate / 1000.0);
        const double step = 2.0 * pi * frequency / SstvDecoder::SampleRate;
        for (int i = 0; i < count; ++i) {
            samples.append(static_cast<float>(0.75 * qSin(phase)));
            phase += step;
            if (phase >= 2.0 * pi)
                phase -= 2.0 * pi;
        }
    };

    // Some on-air transmitters send a multi-second 1900 Hz preamble before
    // the normal break/leader/VIS sequence. It is still a valid first leader.
    appendTone(1900.0, 3000.0);
    appendTone(1200.0, 10.0);
    appendTone(1900.0, 300.0);
    appendTone(1200.0, 30.0);
    int parity = 0;
    for (int bit = 0; bit < 7; ++bit) {
        const bool one = (mode->visCode & (1 << bit)) != 0;
        appendTone(one ? 1100.0 : 1300.0, 30.0);
        parity ^= one ? 1 : 0;
    }
    appendTone(parity ? 1100.0 : 1300.0, 30.0);
    appendTone(1200.0, 30.0);
    appendTone(1500.0, 50.0);

    SstvDecoder decoder;
    QSignalSpy modeSpy(&decoder, &SstvDecoder::modeDetected);
    for (int offset = 0; offset < samples.size(); offset += 2400)
        decoder.consumeMono(samples.mid(offset, qMin(2400, samples.size() - offset)));

    QCOMPARE(modeSpy.size(), 1);
    QCOMPARE(modeSpy.first().at(0).toInt(), static_cast<int>(mode->id));
}

void SstvEncoderTest::idleSearchHistoryRemainsBoundedAndRecovers() {
    const SstvModeSpec *mode = SstvModeRegistry::find(SstvModeId::ScottieS1);
    QVERIFY(mode);

    SstvDecoder decoder;
    QSignalSpy modeSpy(&decoder, &SstvDecoder::modeDetected);
    double phase = 0.0;
    constexpr double pi = 3.14159265358979323846;
    auto feedTone = [&](double frequency, double durationMs) {
        int remaining = qRound(durationMs * SstvDecoder::SampleRate / 1000.0);
        const double step = 2.0 * pi * frequency / SstvDecoder::SampleRate;
        while (remaining > 0) {
            const int count = qMin(2400, remaining);
            QVector<float> samples;
            samples.reserve(count);
            for (int i = 0; i < count; ++i) {
                samples.append(static_cast<float>(0.75 * qSin(phase)));
                phase += step;
                if (phase >= 2.0 * pi)
                    phase -= 2.0 * pi;
            }
            decoder.consumeMono(samples);
            QVERIFY(decoder.bufferedFrequencySampleCount() <= 10LL * SstvDecoder::SampleRate);
            remaining -= count;
        }
    };

    // AUTO RX may remain open indefinitely between transmissions. Model a
    // long non-VIS tone, then prove that a valid header is still acquired.
    feedTone(1500.0, 30000.0);
    QVERIFY(decoder.bufferedFrequencySampleCount() <= 8LL * SstvDecoder::SampleRate);

    feedTone(1900.0, 300.0);
    feedTone(1200.0, 10.0);
    feedTone(1900.0, 300.0);
    feedTone(1200.0, 30.0);
    int parity = 0;
    for (int bit = 0; bit < 7; ++bit) {
        const bool one = (mode->visCode & (1 << bit)) != 0;
        feedTone(one ? 1100.0 : 1300.0, 30.0);
        parity ^= one ? 1 : 0;
    }
    feedTone(parity ? 1100.0 : 1300.0, 30.0);
    feedTone(1200.0, 30.0);
    feedTone(1500.0, 50.0);

    QCOMPARE(modeSpy.size(), 1);
    QCOMPARE(modeSpy.first().at(0).toInt(), static_cast<int>(mode->id));
}

void SstvEncoderTest::autoSlantCorrectsSampleClockError() {
    const SstvModeSpec *mode = SstvModeRegistry::find(SstvModeId::Robot36);
    QVERIFY(mode);
    QImage source(mode->width, mode->height, QImage::Format_RGB32);
    source.fill(qRgb(70, 140, 210));
    SstvEncoder encoder;
    QString error;
    QVERIFY2(encoder.begin(source, mode->id, &error), qPrintable(error));

    QVector<float> nominal;
    nominal.reserve(encoder.totalSamples());
    while (encoder.isActive()) {
        const QVector<qint16> packet = encoder.nextSamples(2400);
        for (qint16 sample : packet)
            nominal.append(static_cast<float>(sample) / 32768.0f);
    }

    constexpr double clockScale = 1.002; // +2000 ppm line-time error
    QVector<float> stretched;
    stretched.reserve(qRound(nominal.size() * clockScale));
    const int outputSamples = qRound(nominal.size() * clockScale);
    for (int i = 0; i < outputSamples; ++i) {
        const double sourcePosition = i / clockScale;
        const int left = qMin(static_cast<int>(sourcePosition), nominal.size() - 1);
        const int right = qMin(left + 1, nominal.size() - 1);
        const double fraction = sourcePosition - left;
        stretched.append(static_cast<float>(nominal[left] * (1.0 - fraction) + nominal[right] * fraction));
    }

    SstvDecoder decoder;
    QSignalSpy completeSpy(&decoder, &SstvDecoder::imageCompleted);
    for (int offset = 0; offset < stretched.size(); offset += 2400)
        decoder.consumeMono(stretched.mid(offset, qMin(2400, stretched.size() - offset)));
    decoder.consumeMono(QVector<float>(240, 0.0f));
    QCOMPARE(completeSpy.size(), 1);
    const QString slant = completeSpy.first().at(2).toString();
    QVERIFY2(slant.contains(QStringLiteral("LOCKED")), qPrintable(slant));
    const QRegularExpressionMatch ppmMatch = QRegularExpression(QStringLiteral("(-?\\d+) ppm")).match(slant);
    QVERIFY2(ppmMatch.hasMatch(), qPrintable(slant));
    QVERIFY2(qAbs(ppmMatch.captured(1).toInt() - 2000) < 100, qPrintable(slant));
}

void SstvEncoderTest::weakSignalRejectsOutOfBandInterference() {
    const SstvModeSpec *mode = SstvModeRegistry::find(SstvModeId::Robot36);
    QVERIFY(mode);
    QImage source(mode->width, mode->height, QImage::Format_RGB32);
    source.fill(qRgb(80, 145, 205));
    SstvEncoder encoder;
    QString error;
    QVERIFY2(encoder.begin(source, mode->id, &error), qPrintable(error));

    SstvDecoder decoder;
    QSignalSpy completeSpy(&decoder, &SstvDecoder::imageCompleted);
    qint64 sampleIndex = 0;
    constexpr double pi = 3.14159265358979323846;
    while (encoder.isActive()) {
        const QVector<qint16> encoded = encoder.nextSamples(1200);
        QVector<float> impaired;
        impaired.reserve(encoded.size());
        for (qint16 value : encoded) {
            // The desired waveform is deliberately much weaker than two
            // receiver/audio-path interferers outside the SSTV band. Small,
            // deterministic impulses exercise the robust sync/pixel paths.
            double sample = 0.20 * static_cast<double>(value) / 32768.0;
            sample += 0.55 * qSin(2.0 * pi * 3450.0 * sampleIndex / SstvDecoder::SampleRate);
            sample += 0.35 * qSin(2.0 * pi * 420.0 * sampleIndex / SstvDecoder::SampleRate);
            if ((sampleIndex % 4093) == 0)
                sample += ((sampleIndex / 4093) & 1) ? 1.4 : -1.4;
            impaired.append(static_cast<float>(sample));
            ++sampleIndex;
        }
        decoder.consumeMono(impaired);
    }
    decoder.consumeMono(QVector<float>(480, 0.0f));

    QCOMPARE(completeSpy.size(), 1);
    const QImage decoded = qvariant_cast<QImage>(completeSpy.first().at(0));
    const QColor actual(decoded.pixel(decoded.width() / 2, decoded.height() / 2));
    QVERIFY2(qAbs(actual.red() - 80) < 35,
             qPrintable(QStringLiteral("weak RX decoded %1,%2,%3")
                            .arg(actual.red()).arg(actual.green()).arg(actual.blue())));
    QVERIFY(qAbs(actual.green() - 145) < 35);
    QVERIFY(qAbs(actual.blue() - 205) < 35);
}

void SstvEncoderTest::imageAfcTracksFrequencyDrift() {
    const SstvModeSpec *mode = SstvModeRegistry::find(SstvModeId::Robot36);
    QVERIFY(mode);

    QVector<float> samples;
    samples.reserve(qRound((910.0 + mode->height * mode->lineTimeMs)
                           * SstvDecoder::SampleRate / 1000.0));
    double phase = 0.0;
    constexpr double pi = 3.14159265358979323846;
    auto appendTone = [&](double frequency, double durationMs, double offsetHz = 0.0) {
        const int count = qRound(durationMs * SstvDecoder::SampleRate / 1000.0);
        const double step = 2.0 * pi * (frequency + offsetHz) / SstvDecoder::SampleRate;
        for (int i = 0; i < count; ++i) {
            samples.append(static_cast<float>(0.72 * qSin(phase)));
            phase += step;
            if (phase >= 2.0 * pi)
                phase -= 2.0 * pi;
        }
    };

    appendTone(1900.0, 300.0);
    appendTone(1200.0, 10.0);
    appendTone(1900.0, 300.0);
    appendTone(1200.0, 30.0);
    int parity = 0;
    for (int bit = 0; bit < 7; ++bit) {
        const bool one = (mode->visCode & (1 << bit)) != 0;
        appendTone(one ? 1100.0 : 1300.0, 30.0);
        parity ^= one ? 1 : 0;
    }
    appendTone(parity ? 1100.0 : 1300.0, 30.0);
    appendTone(1200.0, 30.0);

    // Constant neutral chroma and luminance make any uncompensated frequency
    // drift visible as a top-to-bottom brightness/color change. The offset
    // reaches +120 Hz slowly enough to model oscillator or tuning drift.
    for (int line = 0; line < mode->height; ++line) {
        const double driftHz = 120.0 * line / (mode->height - 1);
        appendTone(1200.0, 9.0, driftHz);
        appendTone(1500.0, 3.0, driftHz);
        appendTone(1800.0, 88.0, driftHz);
        appendTone((line & 1) ? 2300.0 : 1500.0, 4.5, driftHz);
        appendTone(1900.0, 1.5, driftHz);
        appendTone(1900.0, 44.0, driftHz);
    }

    SstvDecoder decoder;
    QSignalSpy completeSpy(&decoder, &SstvDecoder::imageCompleted);
    for (int offset = 0; offset < samples.size(); offset += 1200)
        decoder.consumeMono(samples.mid(offset, qMin(1200, samples.size() - offset)));
    decoder.consumeMono(QVector<float>(480, 0.0f));

    QCOMPARE(completeSpy.size(), 1);
    const QImage decoded = qvariant_cast<QImage>(completeSpy.first().at(0));
    const int top = qGray(decoded.pixel(decoded.width() / 2, 20));
    const int bottom = qGray(decoded.pixel(decoded.width() / 2, decoded.height() - 20));
    QVERIFY2(qAbs(bottom - top) < 20,
             qPrintable(QStringLiteral("AFC brightness drift top=%1 bottom=%2").arg(top).arg(bottom)));
    QVERIFY2(qAbs(bottom - 96) < 25,
             qPrintable(QStringLiteral("AFC final brightness=%1 expected about 96").arg(bottom)));
}

void SstvEncoderTest::defaultModeIsScottieS1() {
    QCOMPARE(SstvModeRegistry::defaultTransmitMode().id, SstvModeId::ScottieS1);
}

void SstvEncoderTest::sequentialEncoderProducesContinuousFrames() {
    QImage image(320, 256, QImage::Format_RGB32);
    image.fill(qRgb(16, 128, 240));
    SstvEncoder encoder;
    QString error;
    QVERIFY2(encoder.begin(image, SstvModeId::ScottieS1, &error), qPrintable(error));
    QVERIFY(encoder.totalSamples() > 1000000);

    int produced = 0;
    while (encoder.isActive()) {
        const QVector<qint16> packet = encoder.nextSamples(240);
        QVERIFY(!packet.isEmpty());
        QVERIFY(packet.size() <= 240);
        produced += packet.size();
    }
    QVERIFY(encoder.isComplete());
    QCOMPARE(produced, encoder.totalSamples());
    QCOMPARE(encoder.progressPercent(), 100);
}

void SstvEncoderTest::robot36UsesCanonicalChromaOrder() {
    const SstvModeSpec *mode = SstvModeRegistry::find(SstvModeId::Robot36);
    QVERIFY(mode);
    QImage image(mode->width, mode->height, QImage::Format_RGB32);
    image.fill(qRgb(255, 0, 0));

    SstvEncoder encoder;
    QString error;
    QVERIFY2(encoder.begin(image, mode->id, &error), qPrintable(error));
    QVector<qint16> samples;
    while (samples.size() < qRound(1.21 * SstvEncoder::SampleRate) && encoder.isActive())
        samples += encoder.nextSamples(2400);

    auto estimateHz = [&samples](double centerMs) {
        const int begin = qRound((centerMs - 10.0) * SstvEncoder::SampleRate / 1000.0);
        const int end = qRound((centerMs + 10.0) * SstvEncoder::SampleRate / 1000.0);
        int risingCrossings = 0;
        for (int i = begin + 1; i < end; ++i) {
            if (samples.at(i - 1) <= 0 && samples.at(i) > 0)
                ++risingCrossings;
        }
        return risingCrossings * static_cast<double>(SstvEncoder::SampleRate) / (end - begin);
    };

    // The 910 ms header is followed by two 150 ms halves. Solid red has high
    // Cr and low Cb, so canonical Robot 36 must put the higher chroma tone in
    // the even half and the lower one in the odd half.
    const double evenCrHz = estimateHz(1038.0);
    const double oddCbHz = estimateHz(1188.0);
    QVERIFY2(evenCrHz > oddCbHz + 350.0,
             qPrintable(QStringLiteral("Robot 36 Cr/Cb order is reversed: %1 Hz then %2 Hz")
                            .arg(evenCrHz).arg(oddCbHz)));
}

void SstvEncoderTest::morseIdIsAppendedAtRequestedWpm() {
    QImage image(320, 256, QImage::Format_RGB32);
    image.fill(qRgb(16, 128, 240));
    QString error;

    SstvEncoder imageOnly;
    QVERIFY2(imageOnly.begin(image, SstvModeId::ScottieS1, &error), qPrintable(error));

    SstvEncoder eAt20;
    QVERIFY2(eAt20.begin(image, SstvModeId::ScottieS1, &error,
                         QStringLiteral("E"), 20), qPrintable(error));
    QCOMPARE(eAt20.imageSamples(), imageOnly.totalSamples());
    // 300 ms separator + one 60 ms dot + 300 ms anti-clipping tail.
    QCOMPARE(eAt20.totalSamples() - eAt20.imageSamples(), 7920);

    SstvEncoder eAt10;
    QVERIFY2(eAt10.begin(image, SstvModeId::ScottieS1, &error,
                         QStringLiteral("E"), 10), qPrintable(error));
    QCOMPARE(eAt10.imageSamples(), imageOnly.totalSamples());
    QCOMPARE(eAt10.totalSamples() - eAt10.imageSamples(), 8640);

    SstvEncoder slash;
    QVERIFY2(slash.begin(image, SstvModeId::ScottieS1, &error,
                         QStringLiteral("W9WDX/P"), 20), qPrintable(error));
    QVERIFY(slash.totalSamples() > slash.imageSamples());
}

void SstvEncoderTest::postImageFskAndCwIdsRoundTrip() {
    const SstvModeSpec *mode = SstvModeRegistry::find(SstvModeId::Robot36);
    QVERIFY(mode);
    QImage image(mode->width, mode->height, QImage::Format_RGB32);
    image.fill(qRgb(40, 120, 210));
    QString error;
    SstvEncoder encoder;
    QVERIFY2(encoder.begin(image, mode->id, &error, QStringLiteral("W9WDX"), 20,
                           QStringLiteral("W9WDX")), qPrintable(error));
    const int expectedFskSamples = (522 + 132 * (5 + 3)) * SstvEncoder::SampleRate / 1000;
    QCOMPARE(encoder.fskIdEndSamples() - encoder.imageSamples(), expectedFskSamples);
    QVERIFY(encoder.totalSamples() > encoder.fskIdEndSamples());

    SstvDecoder decoder;
    QSignalSpy completeSpy(&decoder, &SstvDecoder::imageCompleted);
    QSignalSpy idSpy(&decoder, &SstvDecoder::callsignDetected);
    while (encoder.isActive()) {
        const QVector<qint16> encoded = encoder.nextSamples(480);
        QVector<float> mono;
        mono.reserve(encoded.size());
        for (qint16 sample : encoded)
            mono.append(static_cast<float>(sample) / 32768.0f);
        decoder.consumeMono(mono);
    }
    decoder.consumeMono(QVector<float>(SstvEncoder::SampleRate, 0.0f));
    QCOMPARE(completeSpy.size(), 1);
    QVERIFY2(idSpy.size() >= 2, qPrintable(QStringLiteral("decoded %1 IDs").arg(idSpy.size())));
    QCOMPARE(idSpy.at(0).at(0).toString(), QStringLiteral("W9WDX"));
    QCOMPARE(idSpy.at(0).at(1).toString(), QStringLiteral("FSK ID"));
    QCOMPARE(idSpy.at(1).at(0).toString(), QStringLiteral("W9WDX"));
    QCOMPARE(idSpy.at(1).at(1).toString(), QStringLiteral("CW ID"));
}

void SstvEncoderTest::rejectsWrongFrameSize() {
    SstvEncoder encoder;
    QString error;
    QVERIFY(!encoder.begin(QImage(10, 10, QImage::Format_RGB32), SstvModeId::MartinM1, &error));
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(SstvEncoderTest)
#include "test_sstvencoder.moc"
