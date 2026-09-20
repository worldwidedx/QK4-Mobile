#ifndef OPUSDECODER_H
#define OPUSDECODER_H

#include <QObject>
#include <array>
#include <opus/opus.h>

class OpusDecoder : public QObject {
    Q_OBJECT

public:
    explicit OpusDecoder(QObject *parent = nullptr);
    ~OpusDecoder();

    // Initialize decoder (K4 uses 12000Hz stereo)
    bool initialize(int sampleRate = 12000, int channels = 2);

    // Decode K4 audio packet payload, returns raw normalized stereo Float32 PCM
    // Output is interleaved [main, sub, main, sub, ...] with gain boost applied
    // Volume/routing/balance is NOT applied here — that happens at playback time
    QByteArray decodeK4Packet(const QByteArray &packet);

    // Raw decode for testing (returns S16LE stereo PCM)
    QByteArray decode(const QByteArray &opusData);

    // Decode to float (returns float32 stereo PCM)
    QByteArray decodeFloat(const QByteArray &opusData);

    static constexpr float NORMALIZE_16BIT = 1.0f / 32768.0f;

private:
    ::OpusDecoder *m_decoder;
    int m_sampleRate;
    int m_channels;

    static constexpr int MAX_FRAME_SAMPLES_PER_CHANNEL = 1440;
    static constexpr int MAX_SCRATCH_SAMPLES = MAX_FRAME_SAMPLES_PER_CHANNEL * 2;
    std::array<opus_int16, MAX_SCRATCH_SAMPLES> m_pcmIntScratch{};
    std::array<float, MAX_SCRATCH_SAMPLES> m_pcmFloatScratch{};

    // The K4's EM0 payload uses a 32-bit container but empirically has a
    // roughly 17-bit signal range. This is the normalization used by mainline.
    static constexpr float NORMALIZE_K4_RAW = 1.0f / 131072.0f;

    static constexpr float K4_GAIN_BOOST = 32.0f;
    static constexpr float K4_EM1_GAIN_BOOST = 16.0f;
};

#endif // OPUSDECODER_H
