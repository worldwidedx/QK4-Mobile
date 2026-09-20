#pragma once
#include <QString>
#include <QVector>
#include <atomic>
#include <memory>
#include <optional>
class QSettings;

class DigitalTxCalibration {
public:
    static QString matchingContext(const QString &context);
    static std::optional<float> load(QSettings &settings, const QString &context);
    static bool save(QSettings &settings, const QString &context, float gain);
};

// Shared by the I/O and audio threads. Closing a generation immediately denies
// both further synthesis and socket delivery, without waiting for the UI.
struct DigitalTxControl {
    std::atomic<quint64> generation{0};
    std::atomic<quint64> scheduledGeneration{0}; // Revocable FT slot, including before key-up.
    std::atomic<float> gain{0.5f};
    std::atomic<quint64> audioFault{0};
    std::atomic<bool> calibrating{false};
    bool allows(quint64 id) const { return id != 0 && generation.load(std::memory_order_acquire) == id; }
    void close(quint64 id) { generation.compare_exchange_strong(id, 0, std::memory_order_acq_rel); }
};

// Pure, monotonic-time policy, used by every digital modem. TM's documented
// raw ALC bars are deliberately not presented as calibrated front-panel marks.
class DigitalTxGuard {
public:
    enum class Mode { Ft8, Ft4, Sstv };
    enum class Action { None, Reduced, Tripped, Calibrated };
    // The starting level is not a lower limit. Continue attenuation down to
    // one S16 full-scale quantization step; zero cannot calibrate a tone.
    static constexpr float MinimumGain = 1.0f / 32768.0f;
    static constexpr float CalibrationStartGain = 1.0f / 32.0f;
    static constexpr float MaximumGain = 0.5f;
    static constexpr int ReduceAlc = 6; // Operator-accepted raw ALC 5 is within target.
    static constexpr int StopAlc = 10;
    static constexpr int MeterTimeoutMs = 1500;
    static constexpr int HighTimeoutMs = 1500;
    static constexpr int SettleMs = 400;
    static constexpr int CalibrationSettleMs = 600;
    static constexpr int MinimumHighSamples = 3;
    explicit DigitalTxGuard(std::shared_ptr<DigitalTxControl> control) : m_control(std::move(control)) {}
    bool begin(Mode mode, quint64 generation, qint64 now, bool calibration = false);
    void stop();
    void acknowledge(); // Explicit operator retry only; never a QSO scheduler.
    void audioAccepted(qint64 now);
    Action meter(const QString &command, qint64 now);
    Action tick(qint64 now);
    Action trip(const QString &reason);
    bool active() const { return m_active; }
    bool latched() const { return m_latched; }
    Mode mode() const { return m_mode; }
    quint64 generation() const { return m_generation; }
    QString reason() const { return m_reason; }
    bool calibrating() const { return m_calibrating; }
    bool reduced() const { return m_reduced; }
    QString meterDiagnostic() const { return m_meterDiagnostic; }
private:
    std::shared_ptr<DigitalTxControl> m_control;
    Mode m_mode = Mode::Sstv;
    quint64 m_generation = 0;
    bool m_active = false, m_latched = false;
    bool m_calibrating = false;
    bool m_reduced = false;
    qint64 m_started = 0, m_stableSince = -1, m_adjusted = 0;
    qint64 m_firstAudio = -1, m_lastAudio = -1;
    qint64 m_lastMeter = 0, m_highSince = -1, m_lastReduction = -1;
    int m_highSamples = 0;
    qint64 m_lastHighSample = -1;
    QString m_meterDiagnostic;
    QString m_reason;
};

// Smooth downward gain changes instead of clipping the waveform. Used before
// all PCM/Opus encodings; the voice microphone path does not use this stage.
class DigitalTxAudio {
public:
    void reset(float gain) { m_gain = gain; }
    bool process(QVector<qint16> &samples, DigitalTxControl &control, quint64 generation);
private:
    float m_gain = 0.5f;
};
