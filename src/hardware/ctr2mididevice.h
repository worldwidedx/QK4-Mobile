#ifndef CTR2MIDIDEVICE_H
#define CTR2MIDIDEVICE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include "midimapping.h"

class IambicKeyer;
class QThread;
#ifdef Q_OS_ANDROID
class AndroidCtr2MidiPump;
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
    void setMapping(const MidiMapping::DeviceMapping &mapping);
    bool isPaddleEvent(int status, int data1) const;
#ifdef Q_OS_ANDROID
    void attachKeyer(IambicKeyer *keyer, QThread *keyerThread);
    void detachKeyer();
#endif

signals:
    void connected();
    void disconnected();
    void connectionError(const QString &error);
    void rawMidiEvent(int status, int data1, int data2);

private:
#ifdef Q_OS_ANDROID
    friend class AndroidCtr2MidiPump;
    void deliverMidiEvent(int event);
    void configurePump();
    AndroidCtr2MidiPump *m_androidPump = nullptr;
#endif
    MidiMapping::DeviceMapping m_mapping;
    QString m_portName;
    bool m_connected = false;
    int m_androidConnectionState = 0;
    QTimer *m_connectionPollTimer = nullptr;
    static constexpr int AndroidSession = 1;
};

#endif // CTR2MIDIDEVICE_H
