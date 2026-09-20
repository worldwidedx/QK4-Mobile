#include "sidetonegenerator.h"
#include <QAudioFormat>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QDebug>
#include <QTimer>
#include <QtMath>
#include <cmath>
#ifdef Q_OS_ANDROID
#include <QtMultimedia/private/qaudiodevice_p.h>
#include <QJniObject>
#include <qcoreapplication_platform.h>
#endif

#ifdef Q_OS_ANDROID
namespace {
void startAndroidSidetoneRouteMonitor()
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid()) {
        QJniObject::callStaticMethod<void>(
                "com/w9wdx/qk4phone/AndroidSidetoneRouteMonitor", "start",
                "(Landroid/content/Context;)V", context.object());
    }
}

void stopAndroidSidetoneRouteMonitor()
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid()) {
        QJniObject::callStaticMethod<void>(
                "com/w9wdx/qk4phone/AndroidSidetoneRouteMonitor", "stop",
                "(Landroid/content/Context;)V", context.object());
    }
}

int androidSidetoneRouteGeneration()
{
    return QJniObject::callStaticMethod<jint>(
            "com/w9wdx/qk4phone/AndroidSidetoneRouteMonitor", "getGeneration", "()I");
}

int androidSidetoneDirectOutputDeviceId()
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return -1;

    return QJniObject::callStaticMethod<jint>(
            "com/w9wdx/qk4phone/AndroidSidetoneRouteMonitor",
            "getPreferredDirectOutputDeviceId", "(Landroid/content/Context;)I",
            context.object());
}

QAudioDevice androidSidetoneOutputDevice(int deviceId, const QAudioFormat &format,
                                        bool systemSelected = false)
{
    QAudioDevicePrivate::AudioDeviceFormat deviceFormat;
    deviceFormat.preferredFormat = format;
    deviceFormat.minimumSampleRate = 8000;
    deviceFormat.maximumSampleRate = 192000;
    deviceFormat.minimumChannelCount = 1;
    deviceFormat.maximumChannelCount = 8;
    deviceFormat.supportedSampleFormats = qAllSupportedSampleFormats();
    deviceFormat.channelConfiguration = QAudioFormat::ChannelConfigUnknown;

    return QAudioDevicePrivate::createQAudioDevice(std::make_unique<QAudioDevicePrivate>(
            QByteArray::number(deviceId), QAudioDevice::Output,
            systemSelected ? QStringLiteral("Android sidetone system output")
                           : QStringLiteral("Android sidetone external output"),
            false,
            std::move(deviceFormat)));
}
} // namespace
#endif

SidetoneGenerator::SidetoneGenerator(QObject *parent) : QObject(parent) {
    // Repeat timer created here (moves with parent via moveToThread)
    // Audio init deferred to start() which runs on the sidetone thread
    m_repeatTimer = new QTimer(this);
    m_repeatTimer->setTimerType(Qt::PreciseTimer);
    connect(m_repeatTimer, &QTimer::timeout, this, &SidetoneGenerator::onRepeatTimer);

    m_straightKeyTimer = new QTimer(this);
    m_straightKeyTimer->setTimerType(Qt::PreciseTimer);
    // Refill more often than the write-ahead window. Android is not a
    // real-time scheduler, so a timer that writes exactly one interval of
    // sound can underrun and make a held straight-key tone sound raspy.
    m_straightKeyTimer->setInterval(3);
    connect(m_straightKeyTimer, &QTimer::timeout,
            this, &SidetoneGenerator::onStraightKeyTimer);

    // Recreate the same low-latency sink after Android's media route settles.
    // This changes route lifecycle only; sidetone samples never enter the
    // buffered Android RX AudioTrack that caused the v1.0.4 delay.
    m_routeRefreshTimer = new QTimer(this);
    m_routeRefreshTimer->setSingleShot(true);
    connect(m_routeRefreshTimer, &QTimer::timeout,
            this, &SidetoneGenerator::refreshAudioRoute);

    m_mediaDevices = new QMediaDevices(this);
    connect(m_mediaDevices, &QMediaDevices::audioOutputsChanged,
            this, &SidetoneGenerator::scheduleAudioRouteRefresh);

#ifdef Q_OS_ANDROID
    // Android exposes hearing aids and some wired/USB endpoints outside Qt's
    // public device list. Poll a lightweight generation counter updated by an
    // AudioDeviceCallback so those changes follow the same debounced rebuild.
    m_androidRoutePollTimer = new QTimer(this);
    m_androidRoutePollTimer->setInterval(250);
    connect(m_androidRoutePollTimer, &QTimer::timeout,
            this, &SidetoneGenerator::pollAndroidAudioRoute);
#endif
}

SidetoneGenerator::~SidetoneGenerator() {
    // Audio sink should already be cleaned up by stop() on the correct thread.
    // Guard against missing stop() call, but this runs on the wrong thread.
    if (m_audioSink) {
        qWarning() << "SidetoneGenerator: audio sink not cleaned up by stop() — destroying from wrong thread";
        destroyAudio();
    }
}

void SidetoneGenerator::initAudio() {
    destroyAudio();

    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    QAudioDevice device;
#ifdef Q_OS_ANDROID
    const int directOutputId = androidSidetoneDirectOutputDeviceId();
    m_pendingAndroidOutputId = directOutputId;
    if (directOutputId >= 0) {
        device = androidSidetoneOutputDevice(directOutputId, format);
    } else {
        // QMediaDevices can retain the removed USB endpoint as its default
        // device after Android has already routed media back to the speaker.
        // Device ID 0 is AAUDIO_UNSPECIFIED: it keeps the low-latency native
        // sink while asking Android audio policy to choose the current media
        // output (speaker or Bluetooth) instead of reopening that stale ID.
        device = androidSidetoneOutputDevice(0, format, true);
    }
    qDebug() << "SidetoneGenerator: selected Android output device id"
             << device.id() << "direct endpoint" << (directOutputId >= 0);
#endif
    if (device.isNull())
        device = QMediaDevices::defaultAudioOutput();

    if (device.isNull()) {
        qWarning() << "SidetoneGenerator: No audio output device available";
        return;
    }

    if (!device.isFormatSupported(format)) {
        qWarning() << "SidetoneGenerator: Default format not supported, trying nearest";
        format = device.preferredFormat();
    }

    auto *sink = new QAudioSink(device, format, this);
    m_audioSink = sink;
    sink->setBufferSize(131072); // 128KB - handles even 5 WPM dah (720ms = ~69KB)

    // A route change or Android lifecycle transition can stop the backend and
    // destroy the push-mode QIODevice returned by start(). Invalidate our
    // handle immediately so the next element rebuilds the complete sink.
    connect(sink, &QAudioSink::stateChanged, this,
            [this, sink](QAudio::State state) {
        if (sink != m_audioSink || state != QAudio::StoppedState)
            return;
        m_pushDevice.clear();
        if (sink->error() != QAudio::NoError)
            qWarning() << "SidetoneGenerator: audio sink stopped with error"
                       << sink->error();
    });

    // Start audio sink immediately and keep it running
    m_pushDevice = sink->start();
    if (!m_pushDevice) {
        qWarning() << "SidetoneGenerator: Failed to start audio sink:" << sink->error();
        return;
    }

    QIODevice *const pushDevice = m_pushDevice.data();
    connect(pushDevice, &QObject::destroyed, this, [this, pushDevice]() {
        if (m_pushDevice.data() == pushDevice)
            m_pushDevice.clear();
    });

    // Straight-key queue accounting is relative to the current sink. Route
    // changes replace the sink, so the next refill begins a fresh continuous
    // stream with a short attack instead of carrying stale timing forward.
    m_straightProcessedBaseUs = sink->processedUSecs();
    m_straightQueuedFrames = 0;
    m_straightNeedsFadeIn = true;
}

void SidetoneGenerator::destroyAudio() {
    m_pushDevice.clear();
    QAudioSink *const sink = m_audioSink;
    m_audioSink = nullptr;
    if (!sink)
        return;
    disconnect(sink, nullptr, this, nullptr);
    sink->stop();
    delete sink;
}

bool SidetoneGenerator::ensureAudioReady() {
    if (m_audioSink && m_audioSink->state() != QAudio::StoppedState
        && !m_pushDevice.isNull()) {
        return true;
    }

    initAudio();
    return m_audioSink && m_audioSink->state() != QAudio::StoppedState
        && !m_pushDevice.isNull();
}

void SidetoneGenerator::start() {
    m_running = true;
#ifdef Q_OS_ANDROID
    startAndroidSidetoneRouteMonitor();
    m_lastAndroidRouteGeneration = androidSidetoneRouteGeneration();
    m_androidRoutePollTimer->start();
#endif
    initAudio();
}

void SidetoneGenerator::stop() {
    m_running = false;
    m_repeatTimer->stop();
    m_straightKeyTimer->stop();
    m_straightKeyDown = false;
    m_routeRefreshTimer->stop();
#ifdef Q_OS_ANDROID
    m_androidRoutePollTimer->stop();
    stopAndroidSidetoneRouteMonitor();
#endif
    destroyAudio();
}

void SidetoneGenerator::scheduleAudioRouteRefresh() {
    if (!m_running)
        return;

    // Android emits a burst of topology/default-output changes during a
    // connection. Restarting this single-shot timer rebuilds only after the
    // selected media route is stable.
    m_routeRefreshTimer->start(900);
}

void SidetoneGenerator::refreshAudioRoute() {
    if (!m_running)
        return;

    initAudio();
    if (m_audioSink && !m_pushDevice.isNull())
        qDebug() << "SidetoneGenerator: refreshed Android audio route";
}

#ifdef Q_OS_ANDROID
void SidetoneGenerator::pollAndroidAudioRoute() {
    if (!m_running)
        return;

    const int generation = androidSidetoneRouteGeneration();
    const int directOutputId = androidSidetoneDirectOutputDeviceId();
    const bool topologyChanged = generation != m_lastAndroidRouteGeneration;
    const bool directOutputChanged = directOutputId != m_pendingAndroidOutputId;
    if (!topologyChanged && !directOutputChanged)
        return;

    m_lastAndroidRouteGeneration = generation;
    m_pendingAndroidOutputId = directOutputId;
    scheduleAudioRouteRefresh();
}
#endif

void SidetoneGenerator::setFrequency(int hz) {
    m_frequency.store(hz, std::memory_order_relaxed);
}

void SidetoneGenerator::setVolume(float volume) {
    m_volume.store(volume, std::memory_order_relaxed);
}

void SidetoneGenerator::setKeyerSpeed(int wpm) {
    m_keyerWpm.store(qBound(5, wpm, 60), std::memory_order_relaxed);
}

void SidetoneGenerator::startDit() {
    m_currentElement = ElementDit;
    m_repeatTimer->stop(); // Stop any existing repeat timer
    playElement(ditDurationMs());

    // Start repeat timer: element + inter-element space
    int repeatInterval = ditDurationMs() * 2; // dit + space
    m_repeatTimer->start(repeatInterval);
}

void SidetoneGenerator::startDah() {
    m_currentElement = ElementDah;
    m_repeatTimer->stop(); // Stop any existing repeat timer
    playElement(dahDurationMs());

    // Start repeat timer: element + inter-element space
    int repeatInterval = dahDurationMs() + ditDurationMs(); // dah + space
    m_repeatTimer->start(repeatInterval);
}

void SidetoneGenerator::stopElement() {
    m_currentElement = ElementNone;
    m_repeatTimer->stop();
}

void SidetoneGenerator::playSingleDit() {
    m_currentElement = ElementNone; // No repeat
    m_repeatTimer->stop();
    playElement(ditDurationMs());
}

void SidetoneGenerator::playSingleDah() {
    m_currentElement = ElementNone; // No repeat
    m_repeatTimer->stop();
    playElement(dahDurationMs());
}

void SidetoneGenerator::startStraightKey() {
    if (m_straightKeyDown)
        return;
    m_currentElement = ElementNone;
    m_repeatTimer->stop();
    m_straightKeyDown = true;
    m_straightNeedsFadeIn = true;
    if (ensureAudioReady()) {
        m_straightProcessedBaseUs = m_audioSink->processedUSecs();
        m_straightQueuedFrames = 0;
        refillStraightKeyBuffer();
    }
    m_straightKeyTimer->start();
}

void SidetoneGenerator::stopStraightKey() {
    if (!m_straightKeyDown)
        return;
    m_straightKeyDown = false;
    m_straightKeyTimer->stop();
    // The 3 ms fall follows at most 12 ms of queued tone. This retains a
    // prompt key-up while avoiding both an abrupt click and timer underruns.
    writeStraightKeyFrames((48000 * 3) / 1000, false, true);
}

void SidetoneGenerator::onStraightKeyTimer() {
    if (m_straightKeyDown)
        refillStraightKeyBuffer();
}

void SidetoneGenerator::onRepeatTimer() {
    if (m_currentElement == ElementDit) {
        playElement(ditDurationMs());
        emit ditRepeated(); // Signal for mainwindow to send KZ.; again
    } else if (m_currentElement == ElementDah) {
        playElement(dahDurationMs());
        emit dahRepeated(); // Signal for mainwindow to send KZ-; again
    }
}

int SidetoneGenerator::ditDurationMs() const {
    return 1200 / m_keyerWpm.load(std::memory_order_relaxed);
}

int SidetoneGenerator::dahDurationMs() const {
    return ditDurationMs() * 3;
}

void SidetoneGenerator::playElement(int durationMs) {
    if (!ensureAudioReady()) {
        qWarning() << "SidetoneGenerator: Cannot play - no audio device";
        return;
    }

    const int sampleRate = 48000;
    int toneSamples = (sampleRate * durationMs) / 1000;
    int spaceSamples = (sampleRate * ditDurationMs()) / 1000; // Inter-element space = 1 dit
    int totalSamples = toneSamples + spaceSamples;

    // Add short rise/fall time to avoid clicks (3ms each)
    const int riseTimeSamples = (sampleRate * 3) / 1000;
    const int fallTimeSamples = riseTimeSamples;

    QByteArray buffer(totalSamples * sizeof(qint16), 0);
    qint16 *samples = reinterpret_cast<qint16 *>(buffer.data());

    int freq = m_frequency.load(std::memory_order_relaxed);
    float vol = m_volume.load(std::memory_order_relaxed);
    double phaseIncrement = 2.0 * M_PI * freq / sampleRate;

    // Generate tone samples
    for (int i = 0; i < toneSamples; ++i) {
        float envelope = 1.0f;
        if (i < riseTimeSamples) {
            envelope = 0.5f * (1.0f - qCos(M_PI * i / riseTimeSamples));
        } else if (i >= toneSamples - fallTimeSamples) {
            int fallIndex = i - (toneSamples - fallTimeSamples);
            envelope = 0.5f * (1.0f + qCos(M_PI * fallIndex / fallTimeSamples));
        }

        double sample = qSin(m_phase) * vol * envelope * 32767.0;
        samples[i] = static_cast<qint16>(sample);
        m_phase += phaseIncrement;
        if (m_phase >= 2.0 * M_PI) {
            m_phase -= 2.0 * M_PI;
        }
    }

    // Silence samples are already zero from QByteArray initialization

    // Keep a guarded local reference for the duration of the write. QObject
    // destruction clears both QPointers instead of leaving a non-null dangling
    // device like the raw pointer that caused the TinyMIDI sidetone crash.
    const QPointer<QIODevice> pushDevice = m_pushDevice;
    if (!pushDevice) {
        qWarning() << "SidetoneGenerator: audio device disappeared before write";
        return;
    }
    const qint64 written = pushDevice->write(buffer);
    if (written < 0) {
        qWarning() << "SidetoneGenerator: sidetone write failed; rebuilding audio sink";
        destroyAudio();
    } else if (written < buffer.size()) {
        qWarning() << "SidetoneGenerator: partial sidetone write" << written
                   << "of" << buffer.size() << "bytes";
    }
}

void SidetoneGenerator::refillStraightKeyBuffer() {
    if (!m_straightKeyDown || !ensureAudioReady())
        return;

    constexpr qint64 sampleRate = 48000;
    constexpr qint64 targetFrames = (sampleRate * 12) / 1000;
    const qint64 processedUs = qMax<qint64>(m_straightProcessedBaseUs,
                                            m_audioSink->processedUSecs());
    const qint64 processedFrames =
        ((processedUs - m_straightProcessedBaseUs) * sampleRate) / 1000000;

    // If Android consumed past our last submitted frame during a scheduling
    // pause, rebase the queue before refilling it. The attack ramp prevents a
    // discontinuity if an underrun ever exhausts the safety window.
    if (processedFrames >= m_straightQueuedFrames) {
        m_straightQueuedFrames = processedFrames;
        m_straightNeedsFadeIn = true;
    }
    const qint64 pendingFrames = m_straightQueuedFrames - processedFrames;
    const int requestedFrames = static_cast<int>(qMax<qint64>(0, targetFrames - pendingFrames));
    if (requestedFrames <= 0)
        return;

    const qint64 writtenFrames =
        writeStraightKeyFrames(requestedFrames, m_straightNeedsFadeIn, false);
    if (writtenFrames > 0) {
        m_straightQueuedFrames += writtenFrames;
        m_straightNeedsFadeIn = false;
    }
}

qint64 SidetoneGenerator::writeStraightKeyFrames(int frameCount, bool fadeIn, bool fadeOut) {
    if (!ensureAudioReady())
        return 0;

    constexpr int sampleRate = 48000;
    const int sampleCount = qMax(1, frameCount);
    const int edgeSamples = qMin(sampleCount, (sampleRate * 3) / 1000);
    QByteArray buffer(sampleCount * static_cast<int>(sizeof(qint16)), 0);
    auto *samples = reinterpret_cast<qint16 *>(buffer.data());
    const int frequency = m_frequency.load(std::memory_order_relaxed);
    const float volume = m_volume.load(std::memory_order_relaxed);
    const double phaseIncrement = 2.0 * M_PI * frequency / sampleRate;
    const double startPhase = m_phase;
    double phase = startPhase;

    for (int index = 0; index < sampleCount; ++index) {
        float envelope = 1.0f;
        if (fadeIn && index < edgeSamples)
            envelope *= static_cast<float>(index) / qMax(1, edgeSamples - 1);
        if (fadeOut && index >= sampleCount - edgeSamples)
            envelope *= static_cast<float>(sampleCount - 1 - index) / qMax(1, edgeSamples - 1);
        samples[index] = static_cast<qint16>(qSin(phase) * volume * envelope * 32767.0);
        phase += phaseIncrement;
        if (phase >= 2.0 * M_PI)
            phase -= 2.0 * M_PI;
    }

    const QPointer<QIODevice> pushDevice = m_pushDevice;
    if (!pushDevice)
        return 0;
    const qint64 writtenBytes = pushDevice->write(buffer);
    if (writtenBytes < 0) {
        qWarning() << "SidetoneGenerator: straight-key write failed; rebuilding audio sink";
        destroyAudio();
        return 0;
    }
    const qint64 writtenFrames = writtenBytes / static_cast<qint64>(sizeof(qint16));
    m_phase = std::fmod(startPhase + phaseIncrement * static_cast<double>(writtenFrames),
                        2.0 * M_PI);
    if (writtenBytes < buffer.size())
        qWarning() << "SidetoneGenerator: partial straight-key write" << writtenBytes
                   << "of" << buffer.size() << "bytes";
    return writtenFrames;
}
