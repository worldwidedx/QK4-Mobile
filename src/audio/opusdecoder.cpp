#include "opusdecoder.h"
#include <QDebug>

OpusDecoder::OpusDecoder(QObject *parent) : QObject(parent), m_decoder(nullptr), m_sampleRate(12000), m_channels(2) {}

OpusDecoder::~OpusDecoder() {
    if (m_decoder) {
        opus_decoder_destroy(m_decoder);
    }
}

bool OpusDecoder::initialize(int sampleRate, int channels) {
    m_sampleRate = sampleRate;
    m_channels = channels;

    int error;
    m_decoder = opus_decoder_create(sampleRate, channels, &error);

    if (error != OPUS_OK) {
        qWarning() << "OpusDecoder: Failed to create decoder:" << opus_strerror(error);
        return false;
    }

    return true;
}

QByteArray OpusDecoder::decodeK4Packet(const QByteArray &packet) {
    // K4 Audio Packet Structure:
    // Byte 0: TYPE = 1 (Audio)
    // Byte 1: VER = Version number
    // Byte 2: SEQ = Sequence number
    // Byte 3: Encode Mode (0=S32LE, 1=S16LE, 2=Opus Int, 3=Opus Float)
    // Bytes 4-5: Frame size (little-endian UInt16) - samples per channel
    // Byte 6: Sample rate code (0 = 12000 Hz)
    // Byte 7+: Audio data (format depends on encode mode)
    //
    // Note: EM0 is documented as "RAW 32-bit float" but K4 actually sends S32LE integers

    if (packet.size() < 8) {
        return QByteArray();
    }

    // Verify packet type
    if (static_cast<unsigned char>(packet[0]) != 0x01) {
        return QByteArray();
    }

    unsigned char encodeMode = static_cast<unsigned char>(packet[3]);

    // Extract audio data starting at byte 7
    QByteArray audioData = packet.mid(7);

    if (audioData.isEmpty()) {
        return QByteArray();
    }

    // Decode based on encode mode — output raw normalized stereo [main, sub, main, sub, ...]
    // Volume/routing/balance is applied later at playback time in AudioEngine::feedAudioDevice()
    switch (encodeMode) {
    case 0x00: // EM0 - 32-bit container with the K4's empirically observed signal range
    {
        const qint32 *stereoSamples = reinterpret_cast<const qint32 *>(audioData.constData());
        int totalSamples = audioData.size() / sizeof(qint32);

        QByteArray out(totalSamples * sizeof(float), Qt::Uninitialized);
        float *dst = reinterpret_cast<float *>(out.data());
        for (int i = 0; i < totalSamples; i++) {
            dst[i] = static_cast<float>(stereoSamples[i]) * NORMALIZE_K4_RAW;
        }
        return out;
    }

    case 0x01: // EM1 - RAW 16-bit S16LE stereo PCM
    {
        const qint16 *stereoSamples = reinterpret_cast<const qint16 *>(audioData.constData());
        int totalSamples = audioData.size() / sizeof(qint16);

        QByteArray out(totalSamples * sizeof(float), Qt::Uninitialized);
        float *dst = reinterpret_cast<float *>(out.data());
        for (int i = 0; i < totalSamples; i++) {
            dst[i] = static_cast<float>(stereoSamples[i]) * NORMALIZE_16BIT * K4_EM1_GAIN_BOOST;
        }
        return out;
    }

    case 0x02: // EM2 - Opus encoded, decode with opus_decode() (returns S16LE)
    {
        QByteArray stereoPcm = decode(audioData);
        if (stereoPcm.isEmpty()) {
            return QByteArray();
        }

        const qint16 *stereoSamples = reinterpret_cast<const qint16 *>(stereoPcm.constData());
        int totalSamples = stereoPcm.size() / sizeof(qint16);

        QByteArray out(totalSamples * sizeof(float), Qt::Uninitialized);
        float *dst = reinterpret_cast<float *>(out.data());
        for (int i = 0; i < totalSamples; i++) {
            dst[i] = static_cast<float>(stereoSamples[i]) * NORMALIZE_16BIT * K4_GAIN_BOOST;
        }
        return out;
    }

    case 0x03: // EM3 - Opus encoded, decode with opus_decode_float() (returns float)
    {
        QByteArray stereoPcm = decodeFloat(audioData);
        if (stereoPcm.isEmpty()) {
            return QByteArray();
        }

        const float *stereoFloats = reinterpret_cast<const float *>(stereoPcm.constData());
        int totalSamples = stereoPcm.size() / sizeof(float);

        QByteArray out(totalSamples * sizeof(float), Qt::Uninitialized);
        float *dst = reinterpret_cast<float *>(out.data());
        for (int i = 0; i < totalSamples; i++) {
            dst[i] = stereoFloats[i] * K4_GAIN_BOOST;
        }
        return out;
    }

    default:
        qWarning() << "OpusDecoder: Unknown encode mode:" << encodeMode;
        return QByteArray();
    }
}

QByteArray OpusDecoder::decode(const QByteArray &opusData) {
    if (!m_decoder)
        return QByteArray();

    int samples = opus_decode(m_decoder, reinterpret_cast<const unsigned char *>(opusData.constData()), opusData.size(),
                              m_pcmIntScratch.data(), MAX_FRAME_SAMPLES_PER_CHANNEL, 0);

    if (samples < 0) {
        qWarning() << "OpusDecoder: decode failed:" << opus_strerror(samples);
        return QByteArray();
    }

    return QByteArray(reinterpret_cast<const char *>(m_pcmIntScratch.data()),
                      samples * m_channels * sizeof(opus_int16));
}

QByteArray OpusDecoder::decodeFloat(const QByteArray &opusData) {
    if (!m_decoder)
        return QByteArray();

    int samples = opus_decode_float(m_decoder, reinterpret_cast<const unsigned char *>(opusData.constData()),
                                    opusData.size(), m_pcmFloatScratch.data(), MAX_FRAME_SAMPLES_PER_CHANNEL, 0);

    if (samples < 0) {
        qWarning() << "OpusDecoder: decodeFloat failed:" << opus_strerror(samples);
        return QByteArray();
    }

    return QByteArray(reinterpret_cast<const char *>(m_pcmFloatScratch.data()), samples * m_channels * sizeof(float));
}
