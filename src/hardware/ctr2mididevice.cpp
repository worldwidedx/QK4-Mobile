#include "ctr2mididevice.h"

#ifdef Q_OS_ANDROID

#include <QJniObject>
#include <qcoreapplication_platform.h>
#include <chrono>
#include "iambickeyer.h"

namespace {
QJniObject androidContext() {
    return QNativeInterface::QAndroidApplication::context();
}
}

class AndroidCtr2MidiPump : public QObject {
public:
    AndroidCtr2MidiPump(Ctr2MidiDevice *device, IambicKeyer *keyer)
        : m_device(device), m_keyer(keyer) {
        m_poll = new QTimer(this);
        m_poll->setInterval(4);
        m_poll->setTimerType(Qt::PreciseTimer);
        connect(m_poll, &QTimer::timeout, this, [this]() { poll(); });
    }

    void configure(bool paddlesEnabled, bool extendedButtons, bool swapped) {
        if (m_paddlesEnabled != paddlesEnabled || m_extendedButtons != extendedButtons
            || m_swapped != swapped)
            releaseAll();
        m_paddlesEnabled = paddlesEnabled;
        m_extendedButtons = extendedButtons;
        m_swapped = swapped;
    }
    void start() { m_poll->start(); }
    void stop() { m_poll->stop(); releaseAll(); }

private:
    static qint64 nowNs() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    void setPaddle(int index, bool pressed, qint64 eventNs) {
        if (index == 0)
            m_keyer->setCtr2DitPaddle(pressed, eventNs);
        else
            m_keyer->setCtr2DahPaddle(pressed, eventNs);
    }
    void releaseAll() {
        for (int i = 0; i < 2; ++i) {
            if (m_down[i]) {
                m_down[i] = false;
                setPaddle(i, false, nowNs());
            }
        }
    }
    void poll() {
        for (int count = 0; count < 64; ++count) {
            const jlong packed = QJniObject::callStaticMethod<jlong>(
                    "com/w9wdx/qk4phone/AndroidBleMidi", "pollEventTimed", "(I)J", 1);
            if (packed < 0)
                break;
            const int event = static_cast<int>(packed & 0x00ffffff);
            const qint64 eventNs = nowNs() - static_cast<qint64>(packed >> 24);
            QMetaObject::invokeMethod(m_device,
                    [device = m_device, event]() { device->deliverMidiEvent(event); },
                    Qt::QueuedConnection);
            if (!m_paddlesEnabled)
                continue;
            const int status = (event >> 16) & 0xff;
            const int kind = status & 0xf0;
            if (kind != 0x80 && kind != 0x90)
                continue;
            const int note = (event >> 8) & 0x7f;
            const int left = m_extendedButtons ? 96 : 20;
            const int right = m_extendedButtons ? 97 : 21;
            if (note != left && note != right)
                continue;
            const bool pressed = kind == 0x90 && (event & 0x7f) > 0;
            const int index = ((note == left) != m_swapped) ? 0 : 1;
            if (m_down[index] != pressed) {
                m_down[index] = pressed;
                setPaddle(index, pressed, eventNs);
            }
        }
    }

    Ctr2MidiDevice *m_device;
    IambicKeyer *m_keyer;
    QTimer *m_poll = nullptr;
    bool m_down[2]{};
    bool m_paddlesEnabled = true;
    bool m_extendedButtons = false;
    bool m_swapped = false;
};

void Ctr2MidiDevice::setMapping(const MidiMapping::DeviceMapping &mapping) {
    m_mapping = mapping;
    configurePump();
}

bool Ctr2MidiDevice::isPaddleEvent(int status, int data1) const {
    if (!m_androidPump || !m_mapping.cwInputEnabled
        || m_mapping.profile != MidiMapping::Profile::Ctr2
        || m_mapping.keyingMode != MidiMapping::KeyingMode::Paddles)
        return false;
    const int kind = status & 0xf0;
    if (kind != 0x80 && kind != 0x90)
        return false;
    return data1 == (m_mapping.extendedButtons ? 96 : 20)
           || data1 == (m_mapping.extendedButtons ? 97 : 21);
}

void Ctr2MidiDevice::attachKeyer(IambicKeyer *keyer, QThread *keyerThread) {
    detachKeyer();
    m_androidPump = new AndroidCtr2MidiPump(this, keyer);
    m_androidPump->moveToThread(keyerThread);
    configurePump();
    if (m_connected)
        QMetaObject::invokeMethod(m_androidPump, [pump = m_androidPump]() { pump->start(); },
                                  Qt::QueuedConnection);
}

void Ctr2MidiDevice::detachKeyer() {
    if (!m_androidPump)
        return;
    QMetaObject::invokeMethod(m_androidPump, [pump = m_androidPump]() {
        pump->stop();
        pump->deleteLater();
    }, Qt::BlockingQueuedConnection);
    m_androidPump = nullptr;
}

void Ctr2MidiDevice::configurePump() {
    if (!m_androidPump)
        return;
    const bool paddles = m_mapping.cwInputEnabled
            && m_mapping.profile == MidiMapping::Profile::Ctr2
            && m_mapping.keyingMode == MidiMapping::KeyingMode::Paddles;
    const bool extended = m_mapping.extendedButtons;
    const bool swapped = m_mapping.tipRingSwapped;
    QMetaObject::invokeMethod(m_androidPump,
            [pump = m_androidPump, paddles, extended, swapped]() {
                pump->configure(paddles, extended, swapped);
            }, Qt::QueuedConnection);
}
Ctr2MidiDevice::Ctr2MidiDevice(QObject *parent) : QObject(parent) {
    m_connectionPollTimer = new QTimer(this);
    m_connectionPollTimer->setInterval(250);
    connect(m_connectionPollTimer, &QTimer::timeout, this, [this]() {
        const int state = QJniObject::callStaticMethod<jint>(
            "com/w9wdx/qk4phone/AndroidBleMidi", "getConnectionState", "(I)I",
            static_cast<jint>(AndroidSession));
        if (state == m_androidConnectionState)
            return;
        const int previous = m_androidConnectionState;
        m_androidConnectionState = state;
        m_connected = state == 2;
        if (state == 2) {
            if (m_androidPump)
                QMetaObject::invokeMethod(m_androidPump,
                        [pump = m_androidPump]() { pump->start(); }, Qt::QueuedConnection);
            emit connected();
        } else {
            if (m_androidPump)
                QMetaObject::invokeMethod(m_androidPump,
                        [pump = m_androidPump]() { pump->stop(); }, Qt::QueuedConnection);
            if (previous == 2)
                emit disconnected();
            if (state == 3)
                emit connectionError(statusMessage());
        }
    });
    m_connectionPollTimer->start();
}

Ctr2MidiDevice::~Ctr2MidiDevice() {
    closePort();
    detachKeyer();
}

void Ctr2MidiDevice::deliverMidiEvent(int event) {
    if (!m_connected)
        return;
    const int status = (event >> 16) & 0xff;
    const int kind = status & 0xf0;
    if (kind == 0x80 || kind == 0x90 || kind == 0xb0)
        emit rawMidiEvent(status, (event >> 8) & 0x7f, event & 0x7f);
}

bool Ctr2MidiDevice::openPort(const QString &deviceKey) {
    m_portName = deviceKey;
    const QJniObject context = androidContext();
    const QJniObject key = QJniObject::fromString(deviceKey);
    if (!context.isValid() || !QJniObject::callStaticMethod<jboolean>(
            "com/w9wdx/qk4phone/AndroidBleMidi", "connect",
            "(Landroid/content/Context;ILjava/lang/String;)Z", context.object(),
            static_cast<jint>(AndroidSession), key.object<jstring>())) {
        emit connectionError(statusMessage());
        return false;
    }
    m_androidConnectionState = 1;
    return true;
}

void Ctr2MidiDevice::closePort() {
    QJniObject::callStaticMethod<void>(
        "com/w9wdx/qk4phone/AndroidBleMidi", "disconnect", "(I)V",
        static_cast<jint>(AndroidSession));
    if (m_androidPump)
        QMetaObject::invokeMethod(m_androidPump,
                [pump = m_androidPump]() { pump->stop(); }, Qt::BlockingQueuedConnection);
    const bool wasConnected = m_connected;
    m_androidConnectionState = 0;
    m_connected = false;
    if (wasConnected)
        emit disconnected();
}

bool Ctr2MidiDevice::isConnected() const { return m_connected; }
QString Ctr2MidiDevice::portName() const { return m_portName; }

QString Ctr2MidiDevice::statusMessage() const {
    const QJniObject message = QJniObject::callStaticObjectMethod(
        "com/w9wdx/qk4phone/AndroidBleMidi", "getStatusMessage",
        "(I)Ljava/lang/String;", static_cast<jint>(AndroidSession));
    return message.isValid() ? message.toString()
                             : QStringLiteral("Android MIDI unavailable");
}

QStringList Ctr2MidiDevice::availableMidiDevices() {
    const QJniObject devices = QJniObject::callStaticObjectMethod(
        "com/w9wdx/qk4phone/AndroidBleMidi", "getDevices", "()Ljava/lang/String;");
    return devices.isValid() ? devices.toString().split('\n', Qt::SkipEmptyParts)
                             : QStringList{};
}

void Ctr2MidiDevice::startMidiScan() {
    const QJniObject context = androidContext();
    if (context.isValid())
        QJniObject::callStaticMethod<void>(
            "com/w9wdx/qk4phone/AndroidBleMidi", "startScan",
            "(Landroid/content/Context;)V", context.object());
}

#else

#include <RtMidi.h>
#include <QDebug>

#ifdef Q_OS_IOS
#include "../ios/iosbluetoothmidi.h"
#endif

namespace {
// System virtual endpoints that are never a CTR2-MIDI controller.
const QStringList kExcludedPortPrefixes = {
    QStringLiteral("IAC Driver"),
};

bool isExcludedPort(const QString &name) {
    for (const QString &prefix : kExcludedPortPrefixes) {
        if (name.startsWith(prefix, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

// The CTR2 setup UI persists the selector key, so the key must survive a
// replug. Port indexes do not; port names do.
QString strippedDeviceKey(const QString &deviceKey) {
    if (deviceKey.startsWith(QStringLiteral("usb:")))
        return deviceKey.mid(4);
    if (deviceKey.startsWith(QStringLiteral("ble:")))
        return deviceKey.mid(4);
    return deviceKey;
}
}

Ctr2MidiDevice::Ctr2MidiDevice(QObject *parent)
    : QObject(parent), m_statusMessage(QStringLiteral("Not connected")) {}

void Ctr2MidiDevice::setMapping(const MidiMapping::DeviceMapping &mapping) {
    m_mapping = mapping;
}

bool Ctr2MidiDevice::isPaddleEvent(int, int) const { return false; }

Ctr2MidiDevice::~Ctr2MidiDevice() {
    // Release the RtMidi port before any other teardown. closePort() blocks
    // until an in-progress callback returns, so no callback can reference a
    // partially destroyed object afterwards.
    m_midiIn.reset();
}

bool Ctr2MidiDevice::openPort(const QString &deviceKey) {
    closePort();
    m_portName = deviceKey;

    const QString wanted = strippedDeviceKey(deviceKey);
    if (wanted.isEmpty()) {
        m_statusMessage = QStringLiteral("No CTR2-MIDI device selected");
        emit connectionError(m_statusMessage);
        return false;
    }

    try {
        m_midiIn = std::make_unique<RtMidiIn>();
    } catch (RtMidiError &error) {
        m_statusMessage = QStringLiteral("MIDI unavailable: %1")
                              .arg(QString::fromStdString(error.getMessage()));
        qWarning() << "Ctr2MidiDevice:" << m_statusMessage;
        emit connectionError(m_statusMessage);
        return false;
    }

    int foundPort = -1;
    const unsigned int portCount = m_midiIn->getPortCount();
    for (unsigned int index = 0; index < portCount; ++index) {
        const QString name = QString::fromStdString(m_midiIn->getPortName(index));
        if (name.compare(wanted, Qt::CaseInsensitive) == 0) {
            foundPort = static_cast<int>(index);
            break;
        }
    }
    if (foundPort < 0) {
        // A remembered key can carry a trailing CoreMIDI disambiguator that a
        // later enumeration drops, so fall back to a substring match.
        for (unsigned int index = 0; index < portCount; ++index) {
            const QString name = QString::fromStdString(m_midiIn->getPortName(index));
            if (name.contains(wanted, Qt::CaseInsensitive)) {
                foundPort = static_cast<int>(index);
                break;
            }
        }
    }

    if (foundPort < 0) {
        m_statusMessage = QStringLiteral("CTR2-MIDI device '%1' not found (%2 MIDI ports)")
                              .arg(wanted)
                              .arg(portCount);
        qWarning() << "Ctr2MidiDevice:" << m_statusMessage;
        m_midiIn.reset();
        emit connectionError(m_statusMessage);
        return false;
    }

    try {
        m_midiIn->openPort(static_cast<unsigned int>(foundPort));
    } catch (RtMidiError &error) {
        m_statusMessage = QStringLiteral("Failed to open CTR2-MIDI port: %1")
                              .arg(QString::fromStdString(error.getMessage()));
        qWarning() << "Ctr2MidiDevice:" << m_statusMessage;
        m_midiIn.reset();
        emit connectionError(m_statusMessage);
        return false;
    }

    // CTR2 sends note and control-change traffic only.
    m_midiIn->ignoreTypes(true, true, true);
    m_midiIn->setCallback(&Ctr2MidiDevice::midiCallback, this);

    m_connected = true;
    m_statusMessage = QStringLiteral("Connected to %1")
                          .arg(QString::fromStdString(m_midiIn->getPortName(
                              static_cast<unsigned int>(foundPort))));
    qDebug() << "Ctr2MidiDevice: opened MIDI port" << foundPort << "for" << wanted;
    emit connected();
    return true;
}

void Ctr2MidiDevice::closePort() {
    const bool wasConnected = m_connected;
    m_connected = false;
    // RtMidiIn destruction closes the port and joins the callback thread.
    m_midiIn.reset();
    if (wasConnected) {
        m_statusMessage = QStringLiteral("Not connected");
        emit disconnected();
    }
}

bool Ctr2MidiDevice::isConnected() const { return m_connected; }
QString Ctr2MidiDevice::portName() const { return m_portName; }
QString Ctr2MidiDevice::statusMessage() const { return m_statusMessage; }

QStringList Ctr2MidiDevice::availableMidiDevices() {
    // The CTR2 setup UI parses "display name|selector key" and skips entries
    // with an empty key, so both fields are always supplied here.
    QStringList devices;
    try {
        RtMidiIn midi;
        const unsigned int portCount = midi.getPortCount();
        for (unsigned int index = 0; index < portCount; ++index) {
            const QString name = QString::fromStdString(midi.getPortName(index));
            if (name.isEmpty() || isExcludedPort(name))
                continue;
            // The prefix drives the "remembered device" wording in the setup
            // UI, so report the real transport where it can be identified.
            QString prefix = QStringLiteral("usb:");
#ifdef Q_OS_IOS
            if (iosMidiTransportForName(name) == QStringLiteral("BLE"))
                prefix = QStringLiteral("ble:");
#endif
            devices.append(QStringLiteral("%1|%2%3").arg(name, prefix, name));
        }
    } catch (RtMidiError &error) {
        qWarning() << "Ctr2MidiDevice: MIDI enumeration failed:"
                   << QString::fromStdString(error.getMessage());
    }
    return devices;
}

void Ctr2MidiDevice::startMidiScan() {
    // CoreMIDI and the desktop backends enumerate synchronously inside
    // availableMidiDevices(); no asynchronous discovery pass is needed.
}

void Ctr2MidiDevice::midiCallback(double deltaTime, std::vector<unsigned char> *message,
                                  void *userData) {
    Q_UNUSED(deltaTime)
    auto *self = static_cast<Ctr2MidiDevice *>(userData);
    if (!self || !self->m_connected)
        return;
    if (message && !message->empty())
        self->handleMidiMessage(*message);
}

void Ctr2MidiDevice::handleMidiMessage(const std::vector<unsigned char> &message) {
    if (message.size() < 3)
        return;
    const int status = message[0];
    const int kind = status & 0xf0;
    if (kind != 0x80 && kind != 0x90 && kind != 0xb0)
        return;
    // data2 is passed through unmasked so CTR2 WheelA magnitude (and therefore
    // proportional/speed tuning) survives instead of collapsing to one step.
    // RtMidi runs this on its own thread; the queued connection to the main
    // thread is what makes the emit safe.
    emit rawMidiEvent(status, static_cast<int>(message[1]), static_cast<int>(message[2]));
}

#endif
