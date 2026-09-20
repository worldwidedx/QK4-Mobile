#include "audioengine.h"
#include "opusencoder.h"
#include "../network/protocol.h"
#include <QMediaDevices>
#include <QAudioDevice>
#ifdef Q_OS_ANDROID
#include <QtMultimedia/private/qaudiodevice_p.h>
#endif
#include <QDebug>
#include <algorithm>
#include <cmath>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#include <qcoreapplication_platform.h>
#endif

#ifdef Q_OS_ANDROID
namespace {
void setAndroidTransmitRoute(bool active) {
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return;

    QJniObject::callStaticMethod<void>(
            "com/w9wdx/qk4phone/AndroidAudioRouter", "setTransmitActive",
            "(Landroid/content/Context;Z)V", context.object(), active);
}

int androidWiredOutputDeviceId() {
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return -1;

    return QJniObject::callStaticMethod<jint>(
            "com/w9wdx/qk4phone/AndroidAudioRouter", "getPreferredWiredOutputDeviceId",
            "(Landroid/content/Context;)I", context.object());
}

int androidOutputSampleRate(int deviceId) {
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid() || deviceId < 0)
        return 0;

    return QJniObject::callStaticMethod<jint>(
            "com/w9wdx/qk4phone/AndroidAudioRouter", "getOutputSampleRate",
            "(Landroid/content/Context;I)I", context.object(), deviceId);
}

bool startAndroidPlayback()
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    return context.isValid() && QJniObject::callStaticMethod<jboolean>(
            "com/w9wdx/qk4phone/AndroidAudioPlayback", "start",
            "(Landroid/content/Context;II)Z", context.object(), 48000, 2);
}

void stopAndroidPlayback()
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid())
        QJniObject::callStaticMethod<void>("com/w9wdx/qk4phone/AndroidAudioPlayback", "stop",
                                            "(Landroid/content/Context;)V", context.object());
}

qint64 writeAndroidPlayback(const QByteArray &data)
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid() || data.isEmpty())
        return -1;
    QJniEnvironment env;
    jbyteArray bytes = env->NewByteArray(data.size());
    if (!bytes || env.checkAndClearExceptions())
        return -1;
    env->SetByteArrayRegion(bytes, 0, data.size(),
                            reinterpret_cast<const jbyte *>(data.constData()));
    const jint result = QJniObject::callStaticMethod<jint>(
            "com/w9wdx/qk4phone/AndroidAudioPlayback", "write",
            "(Landroid/content/Context;[BI)I", context.object(), bytes, data.size());
    env->DeleteLocalRef(bytes);
    return result;
}

bool startAndroidUsbMicrophone()
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    return context.isValid() && QJniObject::callStaticMethod<jboolean>(
            "com/w9wdx/qk4phone/AndroidUsbMicrophone", "start",
            "(Landroid/content/Context;)Z", context.object());
}

void stopAndroidUsbMicrophone()
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid())
        QJniObject::callStaticMethod<void>(
                "com/w9wdx/qk4phone/AndroidUsbMicrophone", "stop",
                "(Landroid/content/Context;)V", context.object());
}

QByteArray readAndroidUsbMicrophone(int maximumBytes)
{
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid() || maximumBytes <= 0)
        return {};

    QJniEnvironment env;
    jbyteArray bytes = env->NewByteArray(maximumBytes);
    if (!bytes || env.checkAndClearExceptions())
        return {};

    const jint read = QJniObject::callStaticMethod<jint>(
            "com/w9wdx/qk4phone/AndroidUsbMicrophone", "read",
            "(Landroid/content/Context;[BI)I", context.object(), bytes, maximumBytes);

    QByteArray pcm16;
    if (read > 0) {
        pcm16.resize(read);
        env->GetByteArrayRegion(bytes, 0, read,
                                reinterpret_cast<jbyte *>(pcm16.data()));
    }
    env->DeleteLocalRef(bytes);
    return pcm16;
}

QAudioDevice androidExplicitOutputDevice(int deviceId, const QAudioFormat &format) {
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
            QStringLiteral("Android wired/USB media output"), false, std::move(deviceFormat)));
}
} // namespace
#endif

AudioEngine::AudioEngine(QObject *parent)
    : QObject(parent), m_audioSink(nullptr), m_audioSinkDevice(nullptr), m_audioSource(nullptr),
      m_audioSourceDevice(nullptr), m_opusEncoder(new OpusEncoder(nullptr)), m_micPollTimer(nullptr) {

    m_opusEncoder->initialize(12000, 1);

    // Output format: K4 uses 12kHz stereo Float32 PCM (L=Main RX, R=Sub RX)
    m_outputFormat.setSampleRate(12000);
    m_outputFormat.setChannelCount(2);
    m_outputFormat.setSampleFormat(QAudioFormat::Float);
    m_sinkFormat = m_outputFormat;

    // Input format: Use native 48kHz for microphone capture (most hardware supports this)
    // We'll resample to 12kHz before encoding for K4 TX
    m_inputFormat.setSampleRate(48000);
    m_inputFormat.setChannelCount(1);
    m_inputFormat.setSampleFormat(QAudioFormat::Float);

    // Timers are children of AudioEngine so moveToThread() moves them too
    m_micPollTimer = new QTimer(this);
    m_micPollTimer->setInterval(10); // Poll every 10ms for low latency
    connect(m_micPollTimer, &QTimer::timeout, this, &AudioEngine::onMicDataReady);

    m_sstvPacerTimer = new QTimer(this);
    m_sstvPacerTimer->setTimerType(Qt::PreciseTimer);
    connect(m_sstvPacerTimer, &QTimer::timeout, this, &AudioEngine::onSstvPacer);

    m_feedTimer = new QTimer(this);
    m_feedTimer->setInterval(FEED_INTERVAL_MS);
    connect(m_feedTimer, &QTimer::timeout, this, &AudioEngine::feedAudioDevice);

    // Android may switch a Bluetooth endpoint between media playback and
    // headset capture without changing Qt's device identifier.  Coalesce
    // route notifications and rebuild only our local sink after it settles.
    m_routeRefreshTimer = new QTimer(this);
    m_routeRefreshTimer->setSingleShot(true);
    connect(m_routeRefreshTimer, &QTimer::timeout, this, &AudioEngine::refreshSystemAudioRoute);

#ifdef Q_OS_ANDROID
    // QMediaDevices omits several USB-C audio classes on Android. Poll the
    // platform device list at a low rate so attach/remove is detected even
    // when Qt never emits audioOutputsChanged(). The actual rebuild remains
    // debounced by m_routeRefreshTimer.
    m_androidRoutePollTimer = new QTimer(this);
    m_androidRoutePollTimer->setInterval(300);
    connect(m_androidRoutePollTimer, &QTimer::timeout,
            this, &AudioEngine::pollAndroidOutputRoute);
#endif

    // Size the microphone hot-path buffers before the first PTT.
    m_micBuffer.reserve(2 * 1440 * static_cast<int>(sizeof(qint16)));
    m_resampleBuf12k.reserve(INPUT_BUFFER_SIZE / 4);
    m_feedBatch.reserve(4);

    m_mediaDevices = new QMediaDevices(this);
    connect(m_mediaDevices, &QMediaDevices::audioInputsChanged, this, &AudioEngine::onSystemDefaultInputChanged);
    connect(m_mediaDevices, &QMediaDevices::audioOutputsChanged, this, &AudioEngine::onSystemDefaultOutputChanged);
}

AudioEngine::~AudioEngine() {
    stop();
    delete m_opusEncoder;
    m_opusEncoder = nullptr;
}

bool AudioEngine::start() {
    bool outputOk = setupAudioOutput();

    if (outputOk) {
        m_feedTimer->start();
#ifdef Q_OS_ANDROID
        m_androidRoutePollTimer->start();
#endif
    }

    // Keep input setup lazy. Opening it with the first PTT prevents Android's
    // audio backend from being torn down and renegotiated on every hold.

    return outputOk;
}

void AudioEngine::stop() {
    // Stop feed timer and clear jitter buffer
    if (m_feedTimer) {
        m_feedTimer->stop();
    }
#ifdef Q_OS_ANDROID
    if (m_androidRoutePollTimer)
        m_androidRoutePollTimer->stop();
    if (m_routeRefreshTimer)
        m_routeRefreshTimer->stop();
    m_pendingAndroidWiredOutputId = -1;
#endif
    {
        QMutexLocker lock(&m_queueMutex);
        m_audioQueue.clear();
        m_queueBytes = 0;
        m_prebuffering = true;
    }
    m_writeBuffer.clear();
    resetOutputResampler();

    m_pttActive.store(false, std::memory_order_release);
    stopSstvTransmit();
    m_txSequence = 0;
    closeMic();

    if (m_audioSink) {
        m_audioSink->stop();
        delete m_audioSink;
        m_audioSink = nullptr;
        m_audioSinkDevice = nullptr;
    }
#ifdef Q_OS_ANDROID
    if (m_androidUsbMicrophoneActive) {
        stopAndroidUsbMicrophone();
        m_androidUsbMicrophoneActive = false;
    }
    if (m_androidNativePlaybackActive)
        stopAndroidPlayback();
    m_androidNativePlaybackActive = false;
#endif
    if (m_audioSource) {
        delete m_audioSource;
        m_audioSource = nullptr;
        m_audioSourceDevice = nullptr;
    }

    m_micBuffer.clear();
    m_micReadOffset = 0;
}

bool AudioEngine::setupAudioOutput(bool resetPlayback) {
#ifdef Q_OS_ANDROID
    // Qt 6.11's AAudio sink tears itself down when a USB endpoint is attached
    // during receive. Use Android's media AudioTrack only for Android output;
    // K4 decode, mix, resample, TCP/TLS, and TX input remain unchanged.
    m_sinkSampleRate = 48000;
    if (!startAndroidPlayback()) {
        qWarning() << "AudioEngine: Failed to start Android audio playback";
        return false;
    }
    m_androidNativePlaybackActive = true;
    if (resetPlayback)
        flushQueue();
    if (!resetPlayback)
        feedAudioDevice();
    return true;
#endif
    // Find the output device - use selected device or fall back to default
    QAudioDevice outputDevice;

#ifdef Q_OS_ANDROID
    // Qt's Android device enumerator omits USB_HEADSET and some USB_DEVICE
    // types. Query Android directly and feed its id to the AAudio stream so a
    // USB-C headset is used from app launch as well as when hot-plugged.
    const int wiredOutputId = androidWiredOutputDeviceId();
    m_activeAndroidWiredOutputId = wiredOutputId;
    m_pendingAndroidWiredOutputId = wiredOutputId;
    if (wiredOutputId >= 0) {
        outputDevice = androidExplicitOutputDevice(wiredOutputId, m_outputFormat);
        const int nativeRate = androidOutputSampleRate(wiredOutputId);
        if (nativeRate >= 8000)
            m_sinkSampleRate = nativeRate;
    }
#endif

    if (!m_selectedOutputDeviceId.isEmpty()) {
        // Try to find the selected device
        for (const QAudioDevice &device : QMediaDevices::audioOutputs()) {
            if (device.id() == m_selectedOutputDeviceId) {
                outputDevice = device;
                break;
            }
        }
    }

    // Fall back to default if selected device not found
    if (outputDevice.isNull()) {
        outputDevice = QMediaDevices::defaultAudioOutput();
        m_sinkSampleRate = m_outputFormat.sampleRate();
    }

    if (outputDevice.isNull()) {
        qWarning() << "AudioEngine: No audio output device available";
        return false;
    }

    m_sinkFormat = m_outputFormat;
    m_sinkFormat.setSampleRate(m_sinkSampleRate);
    if (!outputDevice.isFormatSupported(m_sinkFormat)) {
        qWarning() << "AudioEngine: output format not supported by device" << m_sinkFormat;
        return false;
    }

    m_activeOutputDeviceId = outputDevice.id();
    m_audioSink = new QAudioSink(outputDevice, m_sinkFormat, this);
    const int outputBytesPerMs = (m_sinkSampleRate * m_sinkFormat.channelCount()
                                  * static_cast<int>(sizeof(float))) / 1000;
    m_audioSink->setBufferSize(500 * outputBytesPerMs);

    QAudioSink *const observedSink = m_audioSink;
    connect(observedSink, &QAudioSink::stateChanged, this,
            [this, observedSink](QtAudio::State state) {
#ifdef Q_OS_ANDROID
        // Ignore queued state changes from a sink that has already been
        // retired.  Android otherwise reports the old sink as stopped after
        // its successor has opened, which immediately destroys the successor.
        if (observedSink != m_audioSink)
            return;

        // A physical route/profile transition can stop the active Android
        // sink.  Recreate only after the device-change burst has settled; do
        // not affect the K4 stream or PTT gate.
        if (!m_replacingAudioOutput && state == QtAudio::StoppedState &&
            observedSink->error() != QtAudio::NoError) {
            scheduleSystemAudioRouteRefresh(900);
        }
#else
        Q_UNUSED(state)
#endif
    });

    m_audioSinkDevice = m_audioSink->start();
    if (!m_audioSinkDevice) {
        qWarning() << "AudioEngine: Failed to start audio output";
        delete m_audioSink;
        m_audioSink = nullptr;
        return false;
    }

    // Receiver volume is applied by the per-channel mixer. Keep the platform
    // sink at unity, matching current QK4 mainline.
    m_audioSink->setVolume(1.0f);

    if (resetPlayback)
        flushQueue();

    // Android's AAudio backend asks for data immediately after start. Prime
    // the local sink so a route-replacement stream cannot be closed before
    // the next K4 RX packet arrives.
    const QByteArray initialSilence(50 * outputBytesPerMs, '\0');
    m_audioSinkDevice->write(initialSilence);

    if (!resetPlayback) {
        // Preserve the K4 RX jitter buffer through a physical device change.
        // m_writeBuffer is cleared by the caller because it may have been
        // resampled for the retired device's rate.
        feedAudioDevice();
    }

    return true;
}

bool AudioEngine::setupAudioInput() {
    // Find the input device - use selected device or fall back to default
    QAudioDevice inputDevice;

    if (!m_selectedMicDeviceId.isEmpty()) {
        // Try to find the selected device
        for (const QAudioDevice &device : QMediaDevices::audioInputs()) {
            if (device.id() == m_selectedMicDeviceId) {
                inputDevice = device;
                break;
            }
        }
    }

    // Fall back to default if selected device not found
    if (inputDevice.isNull()) {
        inputDevice = QMediaDevices::defaultAudioInput();
    }

    if (inputDevice.isNull()) {
        qWarning() << "AudioEngine: No audio input device available";
        return false;
    }

    if (!inputDevice.isFormatSupported(m_inputFormat)) {
        qWarning() << "AudioEngine: 48kHz input format not supported by device";
        return false;
    }

    m_activeMicDeviceId = inputDevice.id();
    m_audioSource = new QAudioSource(inputDevice, m_inputFormat, this);
    m_audioSource->setBufferSize(INPUT_BUFFER_SIZE);

    // Don't start mic by default - user must enable
    return true;
}

void AudioEngine::enqueueAudio(const QByteArray &pcmData) {
    if (pcmData.isEmpty())
        return;

    QMutexLocker lock(&m_queueMutex);

    const int pktBytes = pcmData.size();

    // Mainline's self-correcting jitter buffer: after a transient stall,
    // discard oldest audio until latency returns to the normal target depth.
    const int highWaterBytes = pktBytes * 5;
    const int targetBytes = pktBytes * 2;
    if (m_queueBytes + pktBytes > highWaterBytes) {
        while (m_queueBytes > targetBytes && !m_audioQueue.isEmpty())
            m_queueBytes -= m_audioQueue.dequeue().size();
    }

    while (m_queueBytes + pktBytes > MAX_QUEUE_BYTES && !m_audioQueue.isEmpty()) {
        m_queueBytes -= m_audioQueue.dequeue().size();
    }

    m_audioQueue.enqueue(pcmData);
    m_queueBytes += pktBytes;
}

void AudioEngine::flushQueue() {
    QMutexLocker lock(&m_queueMutex);
    m_audioQueue.clear();
    m_queueBytes = 0;
    m_prebuffering = true;
    m_writeBuffer.clear();
    resetOutputResampler();
}

void AudioEngine::resetOutputResampler() {
    m_outputResamplerPrimed = false;
    m_outputResamplerPreviousLeft = 0.0f;
    m_outputResamplerPreviousRight = 0.0f;
    m_outputResampleBuffer.clear();
}

const QByteArray &AudioEngine::resampleOutputPacket(const QByteArray &packet) {
    const int inputFrames = packet.size() / (2 * static_cast<int>(sizeof(float)));
    if (inputFrames <= 0) {
        m_outputResampleBuffer.clear();
        return m_outputResampleBuffer;
    }

    // Android playback is a fixed, exact 4:1 conversion. Carry the previous
    // stereo frame across K4 packets so interpolation never restarts at a
    // packet boundary. Resetting interpolation independently for every packet
    // creates a periodic click/rasp that is especially obvious on CW tones.
    if (m_sinkSampleRate == 48000 && m_outputFormat.sampleRate() == 12000) {
        constexpr int ratio = 4;
        m_outputResampleBuffer.resize(inputFrames * ratio * 2 * static_cast<int>(sizeof(float)));
        const float *input = reinterpret_cast<const float *>(packet.constData());
        float *output = reinterpret_cast<float *>(m_outputResampleBuffer.data());
        int outputFrame = 0;
        int inputFrame = 0;

        if (!m_outputResamplerPrimed) {
            m_outputResamplerPreviousLeft = input[0];
            m_outputResamplerPreviousRight = input[1];
            m_outputResamplerPrimed = true;
            for (int phase = 0; phase < ratio; ++phase) {
                output[outputFrame * 2] = m_outputResamplerPreviousLeft;
                output[outputFrame * 2 + 1] = m_outputResamplerPreviousRight;
                ++outputFrame;
            }
            inputFrame = 1;
        }

        for (; inputFrame < inputFrames; ++inputFrame) {
            const float currentLeft = input[inputFrame * 2];
            const float currentRight = input[inputFrame * 2 + 1];
            for (int phase = 0; phase < ratio; ++phase) {
                const float fraction = static_cast<float>(phase) / ratio;
                output[outputFrame * 2] = m_outputResamplerPreviousLeft
                        + (currentLeft - m_outputResamplerPreviousLeft) * fraction;
                output[outputFrame * 2 + 1] = m_outputResamplerPreviousRight
                        + (currentRight - m_outputResamplerPreviousRight) * fraction;
                ++outputFrame;
            }
            m_outputResamplerPreviousLeft = currentLeft;
            m_outputResamplerPreviousRight = currentRight;
        }
        return m_outputResampleBuffer;
    }

    // Retain the established general-rate fallback for non-Android sinks.
    const int outputFrames = qRound(static_cast<double>(inputFrames) * m_sinkSampleRate
                                    / m_outputFormat.sampleRate());
    m_outputResampleBuffer.resize(outputFrames * 2 * static_cast<int>(sizeof(float)));
    const float *input = reinterpret_cast<const float *>(packet.constData());
    float *output = reinterpret_cast<float *>(m_outputResampleBuffer.data());
    for (int frame = 0; frame < outputFrames; ++frame) {
        const double position = static_cast<double>(frame) * m_outputFormat.sampleRate()
                                / m_sinkSampleRate;
        const int before = qBound(0, static_cast<int>(position), inputFrames - 1);
        const int after = qMin(before + 1, inputFrames - 1);
        const float fraction = static_cast<float>(position - before);
        output[frame * 2] = input[before * 2]
                + (input[after * 2] - input[before * 2]) * fraction;
        output[frame * 2 + 1] = input[before * 2 + 1]
                + (input[after * 2 + 1] - input[before * 2 + 1]) * fraction;
    }
    return m_outputResampleBuffer;
}

void AudioEngine::feedAudioDevice() {
#ifdef Q_OS_ANDROID
    if (!m_androidNativePlaybackActive)
        return;
#else
    if (!m_audioSinkDevice)
        return;
#endif

    // Drain any leftover write buffer from a previous partial write
    if (!m_writeBuffer.isEmpty()) {
        int bytesFree = 0;
#ifdef Q_OS_ANDROID
        bytesFree = m_writeBuffer.size();
#else
        bytesFree = m_audioSink->bytesFree();
#endif
        if (bytesFree > 0) {
            qint64 toWrite = qMin(static_cast<qint64>(m_writeBuffer.size()), static_cast<qint64>(bytesFree));
            qint64 written = 0;
#ifdef Q_OS_ANDROID
            written = writeAndroidPlayback(m_writeBuffer.left(toWrite));
#else
            written = m_audioSinkDevice->write(m_writeBuffer.constData(), toWrite);
#endif
            if (written > 0)
                m_writeBuffer.remove(0, static_cast<int>(written));
        }
        if (!m_writeBuffer.isEmpty())
            return; // Still have leftover — don't pull more from queue yet
    }

    // Query sink capacity (audio-thread-only, no mutex needed)
    int bytesFree = 0;
#ifdef Q_OS_ANDROID
    bytesFree = MAX_QUEUE_BYTES;
#else
    bytesFree = m_audioSink->bytesFree();
#endif

    // Reuse the batch instead of allocating at the 100 Hz feed rate.
    m_feedBatch.clear();
    int preDrainQueueBytes = 0;
    bool snapshotPrebuffering = true;
    {
        QMutexLocker lock(&m_queueMutex);

        if (m_audioQueue.isEmpty())
            return;

        // Wait for at least one packet before starting playback
        if (m_prebuffering) {
            if (m_audioQueue.size() < PREBUFFER_PACKETS)
                return;
            m_prebuffering = false;
        }

        preDrainQueueBytes = m_queueBytes;
        snapshotPrebuffering = m_prebuffering;

        // Drain packets that fit in the sink's free space
        while (!m_audioQueue.isEmpty()) {
            int headSize = m_audioQueue.head().size();
            if (bytesFree < headSize)
                break;

            QByteArray pkt = m_audioQueue.dequeue();
            m_queueBytes -= pkt.size();
            bytesFree -= headSize;
            m_feedBatch.append(std::move(pkt));
#ifdef Q_OS_ANDROID
            // A non-blocking AudioTrack write can be partial. Keep later
            // packets queued until this packet (including any staged tail)
            // has been accepted, so returning on a partial write cannot drop
            // already-dequeued audio. At 100 Hz this still drains four times
            // faster than the observed 40 ms K4 packet cadence.
            break;
#endif
        }
    }

    emit bufferStatus(preDrainQueueBytes, MAX_QUEUE_BYTES, snapshotPrebuffering);

    // Apply mix/volume and write to audio sink without holding the lock
    for (QByteArray &packet : m_feedBatch) {
        applyMixAndVolume(packet);
        const QByteArray *playbackPacket = &packet;
        if (m_sinkSampleRate != m_outputFormat.sampleRate())
            playbackPacket = &resampleOutputPacket(packet);

        qint64 written = 0;
#ifdef Q_OS_ANDROID
        QByteArray pcm16;
        const int sampleCount = playbackPacket->size() / static_cast<int>(sizeof(float));
        pcm16.resize(sampleCount * static_cast<int>(sizeof(qint16)));
        const float *input = reinterpret_cast<const float *>(playbackPacket->constData());
        qint16 *output = reinterpret_cast<qint16 *>(pcm16.data());
        for (int i = 0; i < sampleCount; ++i)
            output[i] = static_cast<qint16>(qBound(-1.0f, input[i], 1.0f) * 32767.0f);
        written = writeAndroidPlayback(pcm16);
#else
        written = m_audioSinkDevice->write(playbackPacket->constData(), playbackPacket->size());
#endif
#ifdef Q_OS_ANDROID
        if (written < 0) {
            qWarning() << "AudioEngine: Android playback write failed";
            return;
        }
        if (written < pcm16.size()) {
            // WRITE_NON_BLOCKING may accept only part of a packet. Preserve
            // every remaining byte and stop this batch so later audio cannot
            // overtake it. Dropping this tail creates an audible discontinuity.
            m_writeBuffer.append(pcm16.constData() + written,
                                 pcm16.size() - static_cast<int>(written));
            return;
        }
#else
        if (written < playbackPacket->size()) {
            // Partial write — save remainder for next feed cycle
            m_writeBuffer.append(playbackPacket->constData() + written,
                                 playbackPacket->size() - static_cast<int>(written));
        }
#endif
    }
}

// Compute one output channel's mix from main/sub sources
static inline float mixChannel(float mainSample, float subSample, AudioEngine::MixSource src, float mainVol,
                               float subVol) {
    switch (src) {
    case AudioEngine::MixA:
        return mainSample * mainVol;
    case AudioEngine::MixB:
        return subSample * subVol;
    case AudioEngine::MixAB:
        return mainSample * mainVol + subSample * subVol;
    case AudioEngine::MixNegA:
        return -mainSample * mainVol;
    }
    return 0.0f;
}

void AudioEngine::applyMixAndVolume(QByteArray &packet) {
    float *samples = reinterpret_cast<float *>(packet.data());
    int totalFloats = packet.size() / sizeof(float);
    int sampleCount = totalFloats / 2;

    // Load atomic/guarded values once per packet (not per sample)
    const float mainVol = m_mainVolume.load(std::memory_order_relaxed);
    const float subVol = m_subVolume.load(std::memory_order_relaxed);
    const bool subMuted = m_subMuted.load(std::memory_order_relaxed);
    const int balMode = m_balanceMode.load(std::memory_order_relaxed);
    const int balOffset = m_balanceOffset.load(std::memory_order_relaxed);

    MixSource mixL, mixR;
    {
        QMutexLocker lock(&m_mixMutex);
        mixL = m_mixLeft;
        mixR = m_mixRight;
    }

    // Pre-compute BL balance gains (BAL mode only, applied after MX routing)
    float balLeftGain = 1.0f, balRightGain = 1.0f;
    if (balMode == 1) {
        balLeftGain = qBound(0.0f, (50.0f - balOffset) / 50.0f, 1.0f);
        balRightGain = qBound(0.0f, (50.0f + balOffset) / 50.0f, 1.0f);
    }

    for (int i = 0; i < sampleCount; i++) {
        float mainSample = samples[i * 2];    // Left channel (Main RX / VFO A)
        float subSample = samples[i * 2 + 1]; // Right channel (Sub RX / VFO B)

        // Step 1: SUB RX off — both channels get main audio only, sub slider has no effect
        // BL balance still applies (L/R gain is independent of SUB RX state)
        if (subMuted) {
            float s = mainSample * mainVol;
            samples[i * 2] = qBound(-1.0f, s * balLeftGain, 1.0f);
            samples[i * 2 + 1] = qBound(-1.0f, s * balRightGain, 1.0f);
            continue;
        }

        // Step 2: SUB RX on — apply MX routing
        float left, right;
        if (balMode == 0) {
            // NOR mode: main slider controls main, sub slider controls sub
            left = mixChannel(mainSample, subSample, mixL, mainVol, subVol);
            right = mixChannel(mainSample, subSample, mixR, mainVol, subVol);
        } else {
            // BAL mode: mainVolume controls both receivers (sub slider repurposed as balance)
            left = mixChannel(mainSample, subSample, mixL, mainVol, mainVol);
            right = mixChannel(mainSample, subSample, mixR, mainVol, mainVol);

            // Step 3: Apply BL balance (L/R gain adjustment after MX routing)
            left *= balLeftGain;
            right *= balRightGain;
        }

        // Step 4: Clamp
        samples[i * 2] = qBound(-1.0f, left, 1.0f);
        samples[i * 2 + 1] = qBound(-1.0f, right, 1.0f);
    }
}

void AudioEngine::openMic() {
#ifdef Q_OS_ANDROID
    if (m_micEnabled.load(std::memory_order_relaxed) && m_androidUsbMicrophoneActive)
        return;

    // Native Android capture is used only when a physical wired/USB input is
    // present and accepts explicit selection. Otherwise retain QAudioSource
    // exactly for the phone microphone and Bluetooth headsets.
    if (startAndroidUsbMicrophone()) {
        m_androidUsbMicrophoneActive = true;
        m_micEnabled.store(true, std::memory_order_relaxed);
        m_micPollTimer->start();
        return;
    }
#endif
    // Idempotent: after the first PTT, subsequent presses must be instant.
    if (m_micEnabled.load(std::memory_order_relaxed) && m_audioSourceDevice)
        return;

    if (!m_audioSource && !setupAudioInput()) {
        qWarning() << "AudioEngine: microphone input is unavailable";
        return;
    }

    m_audioSourceDevice = m_audioSource->start();
    if (!m_audioSourceDevice) {
        qWarning() << "AudioEngine: Failed to start microphone device";
        return;
    }

    m_micEnabled.store(true, std::memory_order_relaxed);
    m_micPollTimer->start();
}

void AudioEngine::closeMic() {
    if (!m_micEnabled.exchange(false, std::memory_order_acq_rel))
        return;

    m_micPollTimer->stop();
#ifdef Q_OS_ANDROID
    if (m_androidUsbMicrophoneActive) {
        stopAndroidUsbMicrophone();
        m_androidUsbMicrophoneActive = false;
    } else
#endif
    if (m_audioSource)
        m_audioSource->stop();
    m_audioSourceDevice = nullptr;
    m_micBuffer.clear();
    m_micReadOffset = 0;
}

void AudioEngine::setPttActive(bool active) {
    if (active && (m_sstvPrepared || (m_digitalControl && (m_digitalControl->generation.load() != 0
        || m_digitalControl->scheduledGeneration.load() != 0))))
        return;
    m_pttActive.store(active, std::memory_order_release);
    if (active) {
#ifdef Q_OS_ANDROID
        // Ask Android to select a two-way external endpoint before opening
        // capture, so QAudioSource receives the headset microphone.
        setAndroidTransmitRoute(true);
#endif
        m_txSequence = 0;
        openMic();
        m_micBuffer.clear();
        m_micReadOffset = 0;
    }
#ifdef Q_OS_ANDROID
    else {
        // Android must release capture after TX. Keeping it open pins
        // Bluetooth in communications mode and leaves media RX silent. This
        // is local device lifecycle only; the K4 PTT and packet protocol stay
        // unchanged.
        closeMic();
        delete m_audioSource;
        m_audioSource = nullptr;
        m_audioSourceDevice = nullptr;
        m_activeMicDeviceId.clear();
        setAndroidTransmitRoute(false);
        scheduleSystemAudioRouteRefresh(150);
    }
#endif
    // On release capture remains running; complete frames are consumed but
    // not sent. This is the current QK4 mainline lifecycle.
}

void AudioEngine::setEncodeMode(int mode) {
    m_encodeMode.store(qBound(0, mode, 3), std::memory_order_relaxed);
}

void AudioEngine::setFrameSamples(int samples) {
    m_frameSamples.store(samples, std::memory_order_relaxed);
}

void AudioEngine::beginFt8Encoding(quint64 generation) {
    if (!m_digitalControl || !m_digitalControl->allows(generation)) return;
    if (m_sstvPrepared || m_pttActive.load() || (m_opusEncoder && !m_opusEncoder->reset())) {
        m_digitalControl->audioFault.store(generation);
        m_digitalControl->close(generation);
        return;
    }
    m_ft8EncodingGeneration = generation;
    m_txSequence = 0;
    m_digitalAudio.reset(m_digitalControl->gain.load());
}
void AudioEngine::encodeFt8Frame(const QVector<qint16> &input, int emitted, int total, quint64 generation) {
    if (generation != m_ft8EncodingGeneration || !m_digitalControl->allows(generation)) return;
    auto samples = input;
    if (!m_digitalAudio.process(samples, *m_digitalControl, generation)) return;
    m_ft8Emitted = emitted;
    m_ft8Total = total;
    const QByteArray frame(reinterpret_cast<const char *>(samples.constData()), samples.size() * sizeof(qint16));
    encodeAndSendFrame(frame, samples.size(), m_encodeMode.load(), true);
}
void AudioEngine::finishFt8Encoding(quint64 generation) {
    if (m_ft8EncodingGeneration == generation) m_ft8EncodingGeneration = 0;
}
void AudioEngine::prepareSstvTransmit(const QImage &frame, int modeId,
                                      const QString &morseId, int morseWpm,
                                      const QString &fskId,
                                      quint64 generation) {
    m_sstvGeneration = generation;
    m_calibrationTone = false;
    if (m_sstvActive.load(std::memory_order_acquire)) {
        emit sstvPrepared(false, QStringLiteral("SSTV transmission is already active."), 0, generation);
        return;
    }
    QString error;
    constexpr int SstvPreRollMs = 400;
    constexpr int SstvPostRollMs = 300;
    const bool ready = m_sstvEncoder.begin(frame, static_cast<SstvModeId>(modeId), &error,
                                           morseId, morseWpm, fskId,
                                           SstvPreRollMs, SstvPostRollMs);
    m_sstvPrepared = ready;
    emit sstvPrepared(ready, error, ready ? m_sstvEncoder.totalSamples() : 0, generation);
}

void AudioEngine::beginSstvTransmit() {
    if (!m_digitalControl || !m_digitalControl->allows(m_sstvGeneration.load()))
        return;
    if (!m_sstvPrepared || m_sstvActive.exchange(true, std::memory_order_acq_rel))
        return;

    // Each modem transmission starts with a clean codec history and packet
    // sequence. Speech from an earlier PTT session must not influence the
    // first SSTV leader packet.
    m_txSequence = 0;
    m_digitalAudio.reset(m_digitalControl->gain.load(std::memory_order_acquire));
    if (m_opusEncoder && !m_opusEncoder->reset()) {
        const quint64 generation = m_sstvGeneration;
        m_digitalControl->audioFault.store(generation, std::memory_order_release);
        m_digitalControl->close(generation);
        m_sstvActive.store(false, std::memory_order_release);
        m_sstvPrepared = false;
        emit sstvFailed(QStringLiteral("The K4 audio encoder could not be reset for SSTV."), generation);
        return;
    }

    // Do not call setPttActive(true): it opens Android microphone capture.
    // SSTV is program audio and must keep the mic fully out of the path.
    const int frameSamples = m_frameSamples.load(std::memory_order_relaxed);
    m_sstvPacerTimer->start(qMax(1, qRound(1000.0 * frameSamples / SstvEncoder::SampleRate)));
}

void AudioEngine::stopSstvTransmit() {
    const bool wasActive = m_sstvActive.exchange(false, std::memory_order_acq_rel);
    if (m_sstvPacerTimer)
        m_sstvPacerTimer->stop();
    m_sstvPrepared = false;
    m_sstvEncoder = SstvEncoder();
    if (wasActive)
        emit sstvFinished(m_sstvGeneration);
}

void AudioEngine::requestSstvStop() {
    if (m_digitalControl)
        m_digitalControl->close(m_sstvGeneration.load(std::memory_order_acquire));
    m_sstvActive.store(false, std::memory_order_release);
}

void AudioEngine::prepareDigitalCalibration(int toneHz, quint64 generation) {
    if (m_sstvActive.load(std::memory_order_acquire) || m_pttActive.load()) {
        emit digitalCalibrationPreparationFailed("Calibration unavailable: another audio transmission is active.", generation);
        return;
    }
    m_sstvGeneration = generation;
    m_calibrationHz = qBound(100, toneHz, 3200);
    m_calibrationPhase = 0;
    m_calibrationTone = true;
    m_sstvPrepared = true;
    emit digitalCalibrationPrepared(generation);
}

const QByteArray &AudioEngine::resample48kTo12k(const QByteArray &input48k) {
    // Simple 4:1 decimation with averaging filter (48kHz / 4 = 12kHz).
    const float *inputSamples = reinterpret_cast<const float *>(input48k.constData());
    int inputCount = input48k.size() / sizeof(float);
    int outputCount = inputCount / 4;
    m_resampleBuf12k.resize(outputCount * static_cast<int>(sizeof(float)));
    float *output = reinterpret_cast<float *>(m_resampleBuf12k.data());

    for (int i = 0; i < outputCount; i++) {
        // Average 4 samples for simple low-pass filtering
        int srcIdx = i * 4;
        float sum = 0.0f;
        int count = 0;
        for (int j = 0; j < 4 && (srcIdx + j) < inputCount; j++) {
            sum += inputSamples[srcIdx + j];
            count++;
        }
        output[i] = (count > 0) ? (sum / count) : 0.0f;
    }
    return m_resampleBuf12k;
}

void AudioEngine::onMicDataReady() {
    if (!m_micEnabled.load(std::memory_order_relaxed))
        return;

    QByteArray data48k;
#ifdef Q_OS_ANDROID
    if (m_androidUsbMicrophoneActive) {
        // Android provides S16 PCM at 48 kHz. Convert it to the Float32 48 kHz
        // representation which the established QK4 resample/encode path uses.
        const QByteArray pcm16 = readAndroidUsbMicrophone(INPUT_BUFFER_SIZE / 2);
        const int sampleCount = pcm16.size() / static_cast<int>(sizeof(qint16));
        if (sampleCount > 0) {
            data48k.resize(sampleCount * static_cast<int>(sizeof(float)));
            const qint16 *input = reinterpret_cast<const qint16 *>(pcm16.constData());
            float *output = reinterpret_cast<float *>(data48k.data());
            for (int i = 0; i < sampleCount; ++i)
                output[i] = static_cast<float>(input[i]) / 32768.0f;
        }
    } else
#endif
    {
        if (!m_audioSourceDevice)
            return;
        data48k = m_audioSourceDevice->readAll();
    }
    if (data48k.isEmpty()) {
        // No data available yet - this is normal, just wait for next poll
        return;
    }

    // Resample from 48kHz to 12kHz into the reusable hot-path buffer.
    const QByteArray &data12k = resample48kTo12k(data48k);

    // Convert Float32 to S16LE, apply gain, and buffer for frame-based emission
    const float *floatData = reinterpret_cast<const float *>(data12k.constData());
    int floatSamples = data12k.size() / sizeof(float);

    const float gain = m_micGain.load(std::memory_order_relaxed);

    // Convert and append to the frame buffer with gain applied.
    for (int i = 0; i < floatSamples; i++) {
        float sample = qBound(-1.0f, floatData[i] * gain, 1.0f);
        qint16 s16Sample = static_cast<qint16>(sample * 32767.0f);
        m_micBuffer.append(reinterpret_cast<const char *>(&s16Sample), sizeof(qint16));
    }

    const int frameSamples = m_frameSamples.load(std::memory_order_relaxed);
    const int frameBytes = frameSamples * static_cast<int>(sizeof(qint16));
    const bool pttActive = m_pttActive.load(std::memory_order_acquire);
    const bool sstvActive = m_sstvActive.load(std::memory_order_acquire);
    const int encodeMode = m_encodeMode.load(std::memory_order_relaxed);

    while (m_micBuffer.size() - m_micReadOffset >= frameBytes) {
        if (pttActive && !sstvActive) {
            const QByteArray frame = QByteArray::fromRawData(m_micBuffer.constData() + m_micReadOffset, frameBytes);
            encodeAndSendFrame(frame, frameSamples, encodeMode);
        }
        m_micReadOffset += frameBytes;
    }
    if (m_micReadOffset > 0 && m_micReadOffset * 2 >= m_micBuffer.size()) {
        m_micBuffer.remove(0, m_micReadOffset);
        m_micReadOffset = 0;
    }
}

void AudioEngine::onSstvPacer() {
    if (!m_sstvActive.load(std::memory_order_acquire))
        return;

    const int frameSamples = m_frameSamples.load(std::memory_order_relaxed);
    QVector<qint16> samples;
    if (m_calibrationTone) {
        samples.resize(frameSamples);
        for (auto &sample : samples) {
            sample = qint16(qRound(std::sin(m_calibrationPhase) * 26213.0));
            m_calibrationPhase = std::fmod(m_calibrationPhase + 2.0 * M_PI * m_calibrationHz / 12000.0, 2.0 * M_PI);
        }
    } else
        samples = m_sstvEncoder.nextSamples(frameSamples);
    if (!m_digitalControl || !m_digitalAudio.process(samples, *m_digitalControl, m_sstvGeneration.load())) {
        stopSstvTransmit();
        return; // I/O watchdog owns the fault report and immediate unkey.
    }
    if (!samples.isEmpty()) {
        // K4/Opus packetization requires the configured SL frame size. Pad
        // only the final packet; encoder progress still reports on-air image
        // samples and therefore reaches exactly totalSamples.
        if (samples.size() < frameSamples) {
            const qsizetype imageSamples = samples.size();
            samples.resize(frameSamples);
            std::fill(samples.begin() + imageSamples, samples.end(), 0);
        }
        const QByteArray frame(reinterpret_cast<const char *>(samples.constData()),
                               samples.size() * static_cast<int>(sizeof(qint16)));
        encodeAndSendFrame(frame, frameSamples, m_encodeMode.load(std::memory_order_relaxed), true);
    }
    if (!m_calibrationTone && m_sstvEncoder.isComplete())
        stopSstvTransmit();
}

void AudioEngine::encodeAndSendFrame(const QByteArray &s16leData, int frameSamples, int encodeMode,
                                     bool sstvProgram) {
    QByteArray audioData;
    const qint16 *samples = reinterpret_cast<const qint16 *>(s16leData.constData());
    const int sampleCount = s16leData.size() / static_cast<int>(sizeof(qint16));

    switch (encodeMode) {
    case 0: { // EM0: stereo Float32
        audioData.resize(sampleCount * 2 * static_cast<int>(sizeof(float)));
        float *output = reinterpret_cast<float *>(audioData.data());
        for (int i = 0; i < sampleCount; ++i) {
            const float normalized = static_cast<float>(samples[i]) / 32768.0f;
            output[i * 2] = normalized;
            output[i * 2 + 1] = normalized;
        }
        break;
    }
    case 1: { // EM1: stereo S16LE
        audioData.resize(sampleCount * 2 * static_cast<int>(sizeof(qint16)));
        qint16 *output = reinterpret_cast<qint16 *>(audioData.data());
        for (int i = 0; i < sampleCount; ++i) {
            output[i * 2] = samples[i];
            output[i * 2 + 1] = samples[i];
        }
        break;
    }
    case 2:
    case 3:
    default:
        audioData = m_opusEncoder ? m_opusEncoder->encode(s16leData, frameSamples, sstvProgram) : QByteArray();
        break;
    }

    if (!audioData.isEmpty()) {
        const QByteArray packet = Protocol::buildAudioPacket(audioData, m_txSequence++, encodeMode, frameSamples);
        if (sstvProgram && m_ft8EncodingGeneration)
            emit sstvPacketReady(packet, m_ft8Emitted, m_ft8Total, m_ft8Total, m_ft8EncodingGeneration);
        else if (sstvProgram)
            emit sstvPacketReady(packet, m_sstvEncoder.emittedSamples(),
                                 m_sstvEncoder.totalSamples(), m_sstvEncoder.imageSamples(),
                                 m_sstvGeneration);
        else
            emit txPacketReady(packet);
    } else if (sstvProgram) {
        const quint64 generation = m_ft8EncodingGeneration ? m_ft8EncodingGeneration : m_sstvGeneration.load();
        if (m_digitalControl) {
            m_digitalControl->audioFault.store(generation, std::memory_order_release);
            m_digitalControl->close(generation);
        }
        emit sstvFailed(QStringLiteral("The selected K4 audio encoding could not carry SSTV program audio."),
                        generation);
        stopSstvTransmit();
    }
}

void AudioEngine::setMainVolume(float volume) {
    m_mainVolume.store(qBound(0.0f, volume, 1.0f), std::memory_order_relaxed);
}

void AudioEngine::setSubVolume(float volume) {
    m_subVolume.store(qBound(0.0f, volume, 1.0f), std::memory_order_relaxed);
}

void AudioEngine::setSubMuted(bool muted) {
    m_subMuted.store(muted, std::memory_order_relaxed);
}

void AudioEngine::setAudioMix(int left, int right) {
    QMutexLocker lock(&m_mixMutex);
    m_mixLeft = static_cast<MixSource>(qBound(0, left, 3));
    m_mixRight = static_cast<MixSource>(qBound(0, right, 3));
}

void AudioEngine::setBalanceMode(int mode) {
    m_balanceMode.store(qBound(0, mode, 1), std::memory_order_relaxed);
}

void AudioEngine::setBalanceOffset(int offset) {
    m_balanceOffset.store(qBound(-50, offset, 50), std::memory_order_relaxed);
}

void AudioEngine::setMicGain(float gain) {
    const float cubic = gain * gain * gain;
    m_micGain.store(qBound(0.0f, cubic, 1.0f), std::memory_order_relaxed);
}

void AudioEngine::setMicDevice(const QString &deviceId) {
#ifdef Q_OS_ANDROID
    Q_UNUSED(deviceId)
    // Android headset and USB routes are dynamic.  Always use its current
    // default input rather than pinning an old, device-specific Qt id.
    return;
#else
    if (m_selectedMicDeviceId != deviceId) {
        m_selectedMicDeviceId = deviceId;

        bool wasEnabled = m_micEnabled.load(std::memory_order_relaxed);
        if (wasEnabled)
            closeMic();

        // Recreate the audio source with the new device
        if (m_audioSource) {
            delete m_audioSource;
            m_audioSource = nullptr;
        }
        // setupAudioInput remains lazy, matching mainline's device lifecycle.
        if (wasEnabled)
            openMic();
    }
#endif
}

QString AudioEngine::micDeviceId() const {
    return m_selectedMicDeviceId;
}

QList<QPair<QString, QString>> AudioEngine::availableInputDevices() {
    QList<QPair<QString, QString>> devices;

    // Add "System Default" as the first option
    devices.append(qMakePair(QString(""), QString("System Default")));

    // Add all available input devices
    for (const QAudioDevice &device : QMediaDevices::audioInputs()) {
        devices.append(qMakePair(QString(device.id()), device.description()));
    }

    return devices;
}

void AudioEngine::setOutputDevice(const QString &deviceId) {
#ifdef Q_OS_ANDROID
    Q_UNUSED(deviceId)
    // See setMicDevice(): mobile routes must remain under Android's control.
    return;
#else
    if (m_selectedOutputDeviceId != deviceId) {
        m_selectedOutputDeviceId = deviceId;

        // Restart audio output with the new device if currently running
        if (m_audioSink) {
            m_audioSink->stop();
            delete m_audioSink;
            m_audioSink = nullptr;
            m_audioSinkDevice = nullptr;

            setupAudioOutput();
        }
    }
#endif
}

void AudioEngine::onSystemDefaultInputChanged() {
#ifdef Q_OS_ANDROID
    // Recreate capture only when Android reports an input-device change.  This
    // adopts a headset microphone (when it has one) while retaining the
    // established PTT packet gate and its normal open-capture lifecycle.
    if (m_audioSource) {
        const bool wasOpen = m_micEnabled.load(std::memory_order_relaxed);
        if (wasOpen)
            closeMic();
        delete m_audioSource;
        m_audioSource = nullptr;
        m_audioSourceDevice = nullptr;
        m_activeMicDeviceId.clear();
        if (wasOpen)
            openMic();
    }
    scheduleSystemAudioRouteRefresh(900);
    return;
#endif
    if (!m_selectedMicDeviceId.isEmpty() || !m_audioSource)
        return;

    const QString newDefault = QMediaDevices::defaultAudioInput().id();
    if (newDefault.isEmpty() || newDefault == m_activeMicDeviceId)
        return;

    const bool wasOpen = m_micEnabled.load(std::memory_order_relaxed);
    if (wasOpen)
        closeMic();
    delete m_audioSource;
    m_audioSource = nullptr;
    if (wasOpen)
        openMic();
}

void AudioEngine::onSystemDefaultOutputChanged() {
#ifdef Q_OS_ANDROID
    scheduleSystemAudioRouteRefresh(900);
    return;
#endif
    if (!m_selectedOutputDeviceId.isEmpty() || !m_audioSink)
        return;

    const QString newDefault = QMediaDevices::defaultAudioOutput().id();
    if (newDefault.isEmpty() || newDefault == m_activeOutputDeviceId)
        return;

    m_audioSink->stop();
    delete m_audioSink;
    m_audioSink = nullptr;
    m_audioSinkDevice = nullptr;
    setupAudioOutput();
}

void AudioEngine::scheduleSystemAudioRouteRefresh(int delayMs) {
#ifdef Q_OS_ANDROID
    if (!m_routeRefreshTimer)
        return;

    // Android commonly emits several input/output notifications while a
    // USB-C or Bluetooth endpoint is being attached or detached.  Restart
    // the single-shot timer on every event; the sink is rebuilt once, only
    // after the last event has been quiet long enough for AudioPolicy/AAudio
    // to expose a stable route.
    m_routeRefreshTimer->start(qMax(900, delayMs));
#else
    Q_UNUSED(delayMs)
#endif
}

void AudioEngine::refreshSystemAudioRoute() {
#ifdef Q_OS_ANDROID
    if (!m_audioSink)
        return;

    m_replacingAudioOutput = true;
    QAudioSink *const retiredSink = m_audioSink;
    m_audioSink = nullptr;
    m_audioSinkDevice = nullptr;
    m_activeOutputDeviceId.clear();
    retiredSink->stop();
    delete retiredSink;

    // Partial writes are encoded at the retired sink's rate and cannot carry
    // across the route change. Keep the undecoded K4 RX jitter buffer instead
    // so the replacement sink is primed with real receive audio.
    m_writeBuffer.clear();

    if (setupAudioOutput(false))
        qDebug() << "AudioEngine: refreshed Android playback route";

    m_replacingAudioOutput = false;
#endif
}

void AudioEngine::pollAndroidOutputRoute() {
#ifdef Q_OS_ANDROID
    if (!m_audioSink || m_replacingAudioOutput)
        return;

    const int currentWiredOutputId = androidWiredOutputDeviceId();
    if (currentWiredOutputId == m_activeAndroidWiredOutputId ||
        currentWiredOutputId == m_pendingAndroidWiredOutputId) {
        return;
    }

    // A direct Android route change is new. Do not restart the debounce timer
    // on each poll; only restart it if the selected endpoint changes again.
    m_pendingAndroidWiredOutputId = currentWiredOutputId;
    scheduleSystemAudioRouteRefresh(900);
#endif
}

QString AudioEngine::outputDeviceId() const {
    return m_selectedOutputDeviceId;
}

QList<QPair<QString, QString>> AudioEngine::availableOutputDevices() {
    QList<QPair<QString, QString>> devices;

    // Add "System Default" as the first option
    devices.append(qMakePair(QString(""), QString("System Default")));

    // Add all available output devices
    for (const QAudioDevice &device : QMediaDevices::audioOutputs()) {
        devices.append(qMakePair(QString(device.id()), device.description()));
    }

    return devices;
}
