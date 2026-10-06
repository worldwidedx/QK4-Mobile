#ifndef CTR2MIDIDEVICE_H
#define CTR2MIDIDEVICE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#ifndef Q_OS_ANDROID
#include <memory>
#include <vector>
class RtMidiIn;
#endif

// Raw MIDI connection dedicated to the CTR2-MIDI setup role. The proven CW
// HalikeyDevice remains independent and continues to own Android MIDI session
// zero; this class owns session one.
class Ctr2MidiDevice : public QObject {
    Q_OBJECT

public:
    explicit Ctr2MidiDevice(QObject *parent = nullptr);
    ~Ctr2MidiDevice();

    bool openPort(const QString &deviceKey);
    void closePort();
    bool isConnected() const;
    QString portName() const;
    QString statusMessage() const;

    static QStringList availableMidiDevices();
    static void startMidiScan();

signals:
    void connected();
    void disconnected();
    void connectionError(const QString &error);
    void rawMidiEvent(int status, int data1, int data2);

private:
#ifndef Q_OS_ANDROID
    static void midiCallback(double deltaTime, std::vector<unsigned char> *message, void *userData);
    void handleMidiMessage(const std::vector<unsigned char> &message);

    std::unique_ptr<RtMidiIn> m_midiIn;
    QString m_statusMessage;
#endif
    QString m_portName;
    bool m_connected = false;
    int m_androidConnectionState = 0;
    QTimer *m_eventPollTimer = nullptr;
    QTimer *m_connectionPollTimer = nullptr;
    static constexpr int AndroidSession = 1;
};

#endif // CTR2MIDIDEVICE_H
