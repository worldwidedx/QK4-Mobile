#include "opusencoder.h"
#include <QDebug>
#include <QVector>
#include <cmath>

OpusEncoder::OpusEncoder(QObject *parent) : QObject(parent), m_encoder(nullptr), m_sampleRate(12000), m_channels(1) {}

OpusEncoder::~OpusEncoder() {
    if (m_monitor)
        opus_decoder_destroy(m_monitor);
    if (m_encoder) {
        opus_encoder_destroy(m_encoder);
    }
}

bool OpusEncoder::initialize(int sampleRate, int channels, int bitrate) {
    if (m_monitor) opus_decoder_destroy(m_monitor);
    if (m_encoder) opus_encoder_destroy(m_encoder);
    m_monitor = nullptr;
    m_encoder = nullptr;
    m_sampleRate = sampleRate;
    m_channels = channels;

    int error;
    m_encoder = opus_encoder_create(sampleRate, channels, OPUS_APPLICATION_VOIP, &error);
    if (error != OPUS_OK) {
        qWarning() << "OpusEncoder: Failed to create encoder:" << opus_strerror(error);
        return false;
    }

    opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(bitrate));
    m_monitor = opus_decoder_create(sampleRate, channels, &error);
    if (!m_monitor || error != OPUS_OK)
        return false;
    return true;
}

bool OpusEncoder::reset() {
    if (!m_encoder || !m_monitor)
        return false;
    const int result = opus_encoder_ctl(m_encoder, OPUS_RESET_STATE);
    const int monitorResult = opus_decoder_ctl(m_monitor, OPUS_RESET_STATE);
    if (result != OPUS_OK || monitorResult != OPUS_OK) {
        qWarning() << "OpusEncoder: Failed to reset encoder:" << opus_strerror(result);
        return false;
    }
    return true;
}

QByteArray OpusEncoder::encode(const QByteArray &pcmData, int frameSamples, bool protectedProgram) {
    if (!m_encoder) {
        return QByteArray();
    }

    const int expectedBytes = frameSamples * m_channels * static_cast<int>(sizeof(opus_int16));
    if (pcmData.size() != expectedBytes) {
        qWarning() << "OpusEncoder: Invalid frame size" << pcmData.size() << "bytes, expected" << expectedBytes;
        return QByteArray();
    }

    QByteArray encoded(MAX_PACKET_SIZE, Qt::Uninitialized);

    const opus_int16 *pcm = reinterpret_cast<const opus_int16 *>(pcmData.constData());
    int bytes =
        opus_encode(m_encoder, pcm, frameSamples, reinterpret_cast<unsigned char *>(encoded.data()), MAX_PACKET_SIZE);

    if (bytes < 0) {
        qWarning() << "OpusEncoder: Encode failed:" << opus_strerror(bytes);
        return QByteArray();
    }

    encoded.resize(bytes);
    if (protectedProgram) {
        if (!m_monitor)
            return {};
        QVector<float> decoded(frameSamples * m_channels);
        const int count = opus_decode_float(m_monitor,
                                            reinterpret_cast<const unsigned char *>(encoded.constData()),
                                            bytes, decoded.data(), frameSamples, 0);
        if (count != frameSamples)
            return {};
        for (float sample : decoded)
            if (!std::isfinite(sample) || std::abs(sample) > 0.70710678f)
                return {}; // Retain 3 dB of headroom after codec overshoot.
    }
    return encoded;
}
