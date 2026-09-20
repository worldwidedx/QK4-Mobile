#ifndef OPUSENCODER_H
#define OPUSENCODER_H

#include <QObject>
#include <opus/opus.h>

class OpusEncoder : public QObject {
    Q_OBJECT

public:
    static constexpr int MAX_PACKET_SIZE = 4000;                           // Max Opus packet size

    explicit OpusEncoder(QObject *parent = nullptr);
    ~OpusEncoder();

    bool initialize(int sampleRate = 12000, int channels = 1, int bitrate = 24000);
    bool reset();
    // K4 streaming latency tiers use 240/480/720/1440 samples at 12 kHz.
    QByteArray encode(const QByteArray &pcmData, int frameSamples, bool protectedProgram = false);

private:
    ::OpusEncoder *m_encoder;
    ::OpusDecoder *m_monitor = nullptr;
    int m_sampleRate;
    int m_channels;
};

#endif // OPUSENCODER_H
