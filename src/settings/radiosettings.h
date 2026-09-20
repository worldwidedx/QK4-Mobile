#ifndef RADIOSETTINGS_H
#define RADIOSETTINGS_H

#include <QMap>
#include <QObject>
#include <QPoint>
#include <QSettings>
#include <QString>
#include <QVector>

// Macro entry for programmable function keys
struct MacroEntry {
    QString functionId; // "PF1", "Fn.F1", "K-pod.1T", etc.
    QString label;      // Custom label or empty
    QString command;    // CAT command or empty

    bool isEmpty() const { return command.isEmpty(); }
    QString displayLabel() const {
        if (command.isEmpty())
            return "Unused";
        return label.isEmpty() ? "Mapped" : label;
    }
    bool operator==(const MacroEntry &other) const {
        return functionId == other.functionId && label == other.label && command == other.command;
    }
};

// RX EQ preset entry (8-band graphic equalizer)
struct EqPreset {
    QString name;       // User-defined name ("SSB", "CW", etc.)
    QVector<int> bands; // 8 values, -16 to +16 dB

    bool isEmpty() const { return bands.isEmpty() || name.isEmpty(); }
    QString displayName() const { return isEmpty() ? "---" : name; }
};

struct RadioEntry {
    QString name;
    QString host;
    QString password; // Password (used as PSK when TLS enabled)
    quint16 port;
    bool useTls = false;      // Use TLS/PSK encryption (port 9204)
    QString identity;         // TLS-PSK identity (optional, empty = default)
    int encodeMode = 3;       // Audio encode mode: 0=RAW32, 1=RAW16, 2=Opus Int, 3=Opus Float (default)
    int streamingLatency = 3; // Remote streaming audio latency: 0-7 (default 3)
    int displayFps = 30;      // Display FPS: 12-30 (default 30)

    bool operator==(const RadioEntry &other) const {
        return name == other.name && host == other.host && port == other.port;
    }
};

class RadioSettings : public QObject {
    Q_OBJECT

public:
    static RadioSettings *instance();

    QVector<RadioEntry> radios() const;
    void addRadio(const RadioEntry &radio);
    void removeRadio(int index);
    void updateRadio(int index, const RadioEntry &radio);

    int lastSelectedIndex() const;
    void setLastSelectedIndex(int index);

    bool kpodEnabled() const;
    void setKpodEnabled(bool enabled);

    // KPA1500 Amplifier settings
    QString kpa1500Host() const;
    void setKpa1500Host(const QString &host);
    quint16 kpa1500Port() const;
    void setKpa1500Port(quint16 port);
    bool kpa1500Enabled() const;
    void setKpa1500Enabled(bool enabled);
    int kpa1500PollInterval() const;
    void setKpa1500PollInterval(int intervalMs);
    QPoint kpa1500WindowPosition() const;
    void setKpa1500WindowPosition(const QPoint &pos);

    // Audio output settings
    int volume() const;
    void setVolume(int value); // 0-100, default 45
    int subVolume() const;
    void setSubVolume(int value); // 0-100, default 45 (Sub RX / VFO B)

    // Audio input (microphone) settings
    int micGain() const;
    void setMicGain(int value); // 0-100, default 25
    QString micDevice() const;
    void setMicDevice(const QString &deviceId);

    // Audio output (speaker) settings
    QString speakerDevice() const;
    void setSpeakerDevice(const QString &deviceId);

    // Mobile GEN/SWL band-bank local frequency memory. This is intentionally
    // independent of the K4's amateur-band stack and CAT state.
    quint64 swlBandFrequency(const QString &bandName, quint64 fallbackHz) const;
    void setSwlBandFrequency(const QString &bandName, quint64 frequencyHz);

    // CAT Server settings (local TCP server for external apps)
    bool catServerEnabled() const;
    void setCatServerEnabled(bool enabled);
    quint16 catServerPort() const;
    void setCatServerPort(quint16 port);

    // Macro settings
    QMap<QString, MacroEntry> macros() const;
    MacroEntry macro(const QString &functionId) const;
    void setMacro(const QString &functionId, const QString &label, const QString &command);
    void clearMacro(const QString &functionId);
    void replaceMacros(const QMap<QString, MacroEntry> &macros);

    // HaliKey CW Keyer settings
    QString halikeyPortName() const;
    void setHalikeyPortName(const QString &portName);
    bool halikeyEnabled() const;
    void setHalikeyEnabled(bool enabled);
    int halikeyDeviceType() const;
    void setHalikeyDeviceType(int type); // 0=V14, 1=MiDi
    int sidetoneVolume() const;
    void setSidetoneVolume(int value); // 0-100, default 30
    int cwKeyerSpeed() const;
    void setCwKeyerSpeed(int wpm); // 8-40 WPM, default 20
    bool cwPaddlesReversed() const;
    void setCwPaddlesReversed(bool reversed);
    int midiMappingProfile() const;
    void setMidiMappingProfile(int profile); // 0=TinyMIDI, 1=HaliKey MIDI, 2=Custom
    int midiDitStatus() const;
    int midiDitData1() const;
    int midiDahStatus() const;
    int midiDahData1() const;
    void setMidiCustomMapping(int ditStatus, int ditData1, int dahStatus, int dahData1);
    int cwMidiKeyingMode() const; // 0=paddles, 1=straight key/external keyer
    void setCwMidiKeyingMode(int mode);
    int cwMidiStraightKeyInput() const; // 0=left/tip, 1=right/ring
    void setCwMidiStraightKeyInput(int input);

    // Dedicated CTR2-MIDI role. These settings are intentionally separate
    // from the established CW Keyer connection and profile settings.
    QString ctr2MidiPortName() const;
    void setCtr2MidiPortName(const QString &portName);
    QByteArray ctr2MidiMappingJson() const;
    void setCtr2MidiMappingJson(const QByteArray &json);

    // RX EQ Presets (4 slots)
    EqPreset rxEqPreset(int index) const;                  // Get preset 0-3
    void setRxEqPreset(int index, const EqPreset &preset); // Set preset 0-3
    void clearRxEqPreset(int index);                       // Clear preset 0-3

    // TX EQ Presets (4 slots)
    EqPreset txEqPreset(int index) const;                  // Get preset 0-3
    void setTxEqPreset(int index, const EqPreset &preset); // Set preset 0-3
    void clearTxEqPreset(int index);                       // Clear preset 0-3

    // Local copies of the six K4-style DTMF command memories. The documented
    // CAT protocol transmits digits but does not expose the radio's CMD store.
    QString dtmfCommand(int index) const;
    void setDtmfCommand(int index, const QString &sequence);

signals:
    void radiosChanged();
    void kpodEnabledChanged(bool enabled);
    void kpa1500EnabledChanged(bool enabled);
    void kpa1500SettingsChanged();
    void kpa1500PollIntervalChanged(int intervalMs);
    void micGainChanged(int value);
    void micDeviceChanged(const QString &deviceId);
    void speakerDeviceChanged(const QString &deviceId);
    void catServerEnabledChanged(bool enabled);
    void catServerPortChanged(quint16 port);
    void macrosChanged();
    void halikeyEnabledChanged(bool enabled);
    void halikeyPortNameChanged(const QString &portName);
    void halikeyDeviceTypeChanged(int type);
    void sidetoneVolumeChanged(int value);
    void cwPaddlesReversedChanged(bool reversed);
    void cwMidiKeyingModeChanged(int mode);
    void cwMidiStraightKeyInputChanged(int input);
    void ctr2MidiPortNameChanged(const QString &portName);
    void ctr2MidiMappingChanged();
    void rxEqPresetsChanged();
    void txEqPresetsChanged();

private:
    explicit RadioSettings(QObject *parent = nullptr);
    void load();
    void save();

    QVector<RadioEntry> m_radios;
    int m_lastSelectedIndex;
    bool m_kpodEnabled;

    // KPA1500 settings
    QString m_kpa1500Host;
    quint16 m_kpa1500Port = 1500;
    bool m_kpa1500Enabled = false;
    int m_kpa1500PollInterval = 300; // Default: 300ms for responsive meters

    // CAT Server settings
    bool m_catServerEnabled = false;
    quint16 m_catServerPort = 9299;

    // HaliKey settings
    QString m_halikeyPortName;
    bool m_halikeyEnabled = false;
    int m_halikeyDeviceType = 0; // 0=V14, 1=MiDi
    int m_sidetoneVolume = 30;   // Default 30%
    int m_cwKeyerSpeed = 20;     // Default 20 WPM
    bool m_cwPaddlesReversed = false; // Normal: left=dit, right=dah
    int m_midiMappingProfile = 0;
    int m_midiDitStatus = 0x90;
    int m_midiDitData1 = 20;
    int m_midiDahStatus = 0x90;
    int m_midiDahData1 = 21;
    int m_cwMidiKeyingMode = 0;
    int m_cwMidiStraightKeyInput = 0;

    QString m_ctr2MidiPortName;
    QByteArray m_ctr2MidiMappingJson;

    // Macro settings
    QMap<QString, MacroEntry> m_macros;

    // RX EQ Presets (4 slots)
    EqPreset m_rxEqPresets[4];

    // TX EQ Presets (4 slots)
    EqPreset m_txEqPresets[4];
    QString m_dtmfCommands[6];

    QSettings m_settings;
};

#endif // RADIOSETTINGS_H
