#include "digitaltxguard.h"
#include <algorithm>
#include <cmath>
#include <QSettings>
#include <QDateTime>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace {
QString calibrationKey(const QString &context) {
    return "digitalTx/calibration/" + QString::fromLatin1(
        QCryptographicHash::hash(context.toUtf8(), QCryptographicHash::Sha256).toHex());
}
std::optional<float> calibrationGain(const QVariantMap &record) {
    bool ok = false;
    const float gain = record.value("gain").toFloat(&ok);
    if (!ok || record.value("version").toInt() != 1 || !std::isfinite(gain)
        || gain < DigitalTxGuard::MinimumGain || gain > DigitalTxGuard::MaximumGain)
        return {};
    return gain;
}
}
QString DigitalTxCalibration::matchingContext(const QString &context) {
    const auto document = QJsonDocument::fromJson(context.toUtf8());
    if (!document.isObject()) return context;
    auto object = document.object();
    // A station/audio-frequency or RF power change does not change the
    // calibrated remote audio gain. Live ALC protection follows actual drive.
    // Firmware readback and packet timing likewise are not gain settings.
    for (const auto *key : {"tone", "power", "band", "latency", "firmware"})
        object.remove(QLatin1String(key));
    const int mode = object.value("digitalMode").toInt(-1);
    if (mode == int(DigitalTxGuard::Mode::Ft8) || mode == int(DigitalTxGuard::Mode::Ft4))
        object["digitalMode"] = "FT8/FT4";
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}
std::optional<float> DigitalTxCalibration::load(QSettings &settings, const QString &context) {
    const auto key = calibrationKey(matchingContext(context));
    if (const auto gain = calibrationGain(settings.value(key).toMap())) return gain;

    // Both FT modes use the same calibrated audio path. Adopt the latest
    // completed calibration from either mode, including pre-sharing records.
    const auto keys = settings.allKeys();
    const QSet<QString> stored(keys.cbegin(), keys.cend());
    std::optional<float> recovered;
    QString latestUtc;
    const auto recover = [&](const QString &legacy) {
        const auto legacyKey = calibrationKey(legacy);
        if (!stored.contains(legacyKey)) return;
        const auto record = settings.value(legacyKey).toMap();
        const auto gain = calibrationGain(record);
        const auto utc = record.value("utc").toString();
        if (gain && (!recovered || utc > latestUtc || (utc == latestUtc && *gain < *recovered))) {
            recovered = gain;
            latestUtc = utc;
        }
    };
    const auto json = [](const QJsonObject &object) {
        return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    };
    const auto original = QJsonDocument::fromJson(context.toUtf8()).object();
    const int mode = original.value("digitalMode").toInt(-1);
    const bool ft = mode == int(DigitalTxGuard::Mode::Ft8) || mode == int(DigitalTxGuard::Mode::Ft4);
    const QList<int> modes = ft ? QList<int>{0, 1} : QList<int>{mode};
    for (int legacyMode : modes) {
        auto object = QJsonDocument::fromJson(matchingContext(context).toUtf8()).object();
        if (ft) object["digitalMode"] = legacyMode;
        recover(json(object));
    }
    const bool normalizedRecord = recovered.has_value();
    recover(context);
    if (!normalizedRecord && original.contains("tone") && std::any_of(keys.cbegin(), keys.cend(), [](const QString &k) {
            return k.startsWith("digitalTx/calibration/");
        })) {
        // The oldest records stored only a hash of all settings and offset.
        for (int legacyMode : modes) {
            auto object = original;
            if (ft) object["digitalMode"] = legacyMode;
            for (int hz = 100; hz <= 3200; ++hz) {
                object["tone"] = hz;
                recover(json(object));
            }
        }
    }
    if (recovered) save(settings, context, *recovered);
    return recovered;
}
bool DigitalTxCalibration::save(QSettings &settings, const QString &context, float gain) {
    if (context.isEmpty() || !std::isfinite(gain)
        || gain < DigitalTxGuard::MinimumGain || gain > DigitalTxGuard::MaximumGain)
        return false;
    settings.setValue(calibrationKey(matchingContext(context)), QVariantMap{{"version", 1}, {"gain", gain},
                      {"utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}});
    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool DigitalTxGuard::begin(Mode mode, quint64 generation, qint64 now, bool calibration) {
    if (m_active || m_latched || generation == 0)
        return false;
    m_mode = mode;
    m_calibrating = calibration;
    m_reduced = false;
    m_started = m_adjusted = now;
    m_stableSince = -1;
    m_firstAudio = m_lastAudio = -1;
    m_control->calibrating.store(calibration, std::memory_order_release);
    m_generation = generation;
    m_lastMeter = now;
    m_highSince = m_lastReduction = -1;
    m_highSamples = 0;
    m_lastHighSample = -1;
    m_meterDiagnostic.clear();
    m_reason.clear();
    m_control->audioFault.store(0, std::memory_order_release);
    // Retain reductions between transmissions. Only downward adjustment is
    // automatic; restarting a message must not restore previously excessive drive.
    const float gain = m_control->gain.load(std::memory_order_relaxed);
    m_control->gain.store(calibration ? CalibrationStartGain
        : std::isfinite(gain) ? qBound(MinimumGain, gain, MaximumGain) : MinimumGain);
    m_control->generation.store(generation, std::memory_order_release);
    m_active = true;
    return true;
}
void DigitalTxGuard::stop() {
    m_control->close(m_generation);
    m_active = false;
}
void DigitalTxGuard::acknowledge() {
    if (!m_active) {
        m_latched = false;
        m_reason.clear();
    }
}
void DigitalTxGuard::audioAccepted(qint64 now) {
    if (!m_active) return;
    if (m_firstAudio < 0) {
        m_firstAudio = now;
        m_adjusted = now;
    }
    m_lastAudio = now;
}
DigitalTxGuard::Action DigitalTxGuard::trip(const QString &reason) {
    if (!m_active)
        return Action::None;
    stop();
    m_latched = true;
    m_reason = reason + (m_meterDiagnostic.isEmpty() ? QString() : "\n" + m_meterDiagnostic);
    return Action::Tripped;
}
DigitalTxGuard::Action DigitalTxGuard::tick(qint64 now) {
    if (!m_active)
        return Action::None;
    if (m_control->audioFault.load(std::memory_order_acquire) == m_generation)
        return trip("TX stopped: digital audio exceeded its headroom limit.");
    if (!m_control->allows(m_generation))
        return trip("TX stopped: digital audio was cancelled.");
    if (now < m_lastMeter || now - m_lastMeter >= MeterTimeoutMs)
        return trip("TX stopped: fresh K4 ALC readings are unavailable. Check the connection and metering.");
    if (m_highSamples >= MinimumHighSamples && m_highSince >= 0 && now - m_highSince >= HighTimeoutMs)
        return trip("TX stopped: ALC stayed high after automatic audio-drive reduction. Check K4 input settings.");
    if (m_calibrating && now - m_started >= 15000)
        return trip("Calibration stopped: a stable audio level could not be established.");
    if (m_calibrating && now - (m_lastAudio < 0 ? m_started : m_lastAudio) >= 1500)
        return trip("Calibration stopped: the test tone is not streaming.");
    if (m_control->scheduledGeneration.load() == m_generation
        && now - (m_lastAudio < 0 ? m_started : m_lastAudio) >= (m_lastAudio < 0 ? 1200 : 400))
        return trip("TX stopped: timed FT8/FT4 audio is not streaming.");
    return Action::None;
}
DigitalTxGuard::Action DigitalTxGuard::meter(const QString &command, qint64 now) {
    if (!m_active || !command.startsWith("TM"))
        return Action::None;
    // An enable acknowledgement is not a meter observation.
    if (command == "TM0" || command == "TM1")
        return Action::None;
    if (command.size() != 14)
        return trip("TX stopped: the K4 returned an invalid ALC meter response.");
    for (int i = 2; i < command.size(); ++i)
        if (command[i] < QLatin1Char('0') || command[i] > QLatin1Char('9'))
            return trip("TX stopped: the K4 returned an invalid ALC meter response.");
    // A late reply must not revive expired protection.
    const auto expired = tick(now);
    if (expired == Action::Tripped)
        return expired;
    m_lastMeter = now;
    const int alc = command.mid(2, 3).toInt();
    const int compression = command.mid(5, 3).toInt();
    m_meterDiagnostic = QString("%1; · raw ALC %2 · drive %3 dB")
        .arg(command).arg(alc).arg(20 * std::log10(m_control->gain.load()), 0, 'f', 1);
    if (m_calibrating && command.mid(8, 3).toInt() > 0)
        return trip("Calibration stopped: RF output was reported in K4 TEST mode.");
    if (compression > 0)
        return trip("TX stopped: speech compression was detected in digital audio. Use DATA with compression off.");
    if (alc >= StopAlc)
        return trip("TX stopped: K4 ALC is excessively high. Check the radio's input level.");
    // Only moderate ALC is provisional during tone startup and gain settling.
    // RF, compression, emergency ALC and the I/O watchdog always remain active.
    if (m_calibrating && (m_firstAudio < 0 || now - m_firstAudio < CalibrationSettleMs
                         || now - m_adjusted < CalibrationSettleMs)) {
        m_highSince = m_stableSince = m_lastHighSample = -1;
        m_highSamples = 0;
        return Action::None;
    }
    if (alc < ReduceAlc) {
        m_highSince = -1;
        m_highSamples = 0;
        m_lastHighSample = -1;
        if (m_calibrating) {
            // Meter activity alone cannot prove that our tone reached the radio.
            // Require a continuing stream, then allow transport/ALC to settle.
            if (m_firstAudio < 0 || now - m_firstAudio < 600 || now - m_lastAudio > 500
                || now - m_adjusted < 600) {
                m_stableSince = -1;
                return Action::None;
            }
            if (alc >= 3) {
                if (m_stableSince < 0)
                    m_stableSince = now;
                if (now - m_stableSince >= 1200)
                    return Action::Calibrated;
            } else {
                m_stableSince = -1;
                if (now - m_adjusted >= 600) {
                    const float gain = m_control->gain.load();
                    if (gain >= 0.5f)
                        return trip("Calibration stopped: input gain or passband needs adjustment; safe audio limit reached.");
                    m_control->gain.store(qMin(MaximumGain, gain * 1.41421356f), std::memory_order_release);
                    m_adjusted = now;
                }
            }
        }
        return Action::None;
    }
    m_stableSince = -1;
    if (m_highSince < 0)
        m_highSince = now;
    // Duplicate/burst packets at the same instant are not independent evidence.
    if (m_lastHighSample < 0 || now - m_lastHighSample >= 200) {
        ++m_highSamples;
        m_lastHighSample = now;
    }
    if (m_lastReduction >= 0 && now - m_lastReduction < SettleMs)
        return Action::None;
    const float gain = m_control->gain.load(std::memory_order_relaxed);
    if (gain <= MinimumGain) {
        if (m_highSamples >= MinimumHighSamples && now - m_highSince >= CalibrationSettleMs)
            return trip("TX stopped: ALC remains high at the PCM resolution limit after settling and repeated readings. Check K4 input gain.");
        return Action::None;
    }
    m_control->gain.store(qMax(MinimumGain, gain * 0.70710678f), std::memory_order_release);
    m_adjusted = now;
    m_reduced = true;
    m_lastReduction = now;
    if (m_calibrating) {
        // TEST calibration must be allowed to search below the initial level.
        // Validate each new gain after settling, even with slower TM delivery.
        // Live transmission retains the continuous-high watchdog above.
        m_highSince = m_lastHighSample = -1;
        m_highSamples = 0;
    }
    return Action::Reduced;
}
bool DigitalTxAudio::process(QVector<qint16> &samples, DigitalTxControl &control, quint64 generation) {
    if (!control.allows(generation))
        return false;
    const float target = control.gain.load(std::memory_order_acquire);
    bool valid = std::isfinite(target) && target >= 0.0f && target <= 0.5f
                 && std::isfinite(m_gain) && m_gain >= 0.0f && m_gain <= 0.5f;
    for (auto sample : samples)
        valid = valid && std::abs(int(sample)) < 32767;
    if (!valid) {
        control.audioFault.store(generation, std::memory_order_release);
        control.close(generation);
        return false;
    }
    // Never increase drive within a transmission. A 5 ms transition at 12 kHz
    // avoids a sharp amplitude step while making protective attenuation fast.
    const float next = control.calibrating.load(std::memory_order_acquire) ? target : qMin(m_gain, target);
    const int ramp = qMin(60, int(samples.size()));
    const float previous = m_gain;
    for (int i = 0; i < samples.size(); ++i) {
        const float gain = i < ramp ? previous + (next - previous) * (i + 1) / ramp : next;
        samples[i] = qint16(qRound(samples[i] * gain));
    }
    m_gain = next;
    return control.allows(generation);
}
