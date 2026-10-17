#include "ctr2mididevice.h"

#ifdef Q_OS_ANDROID

#include <QJniObject>
#include <qcoreapplication_platform.h>

namespace {
QJniObject androidContext() {
    return QNativeInterface::QAndroidApplication::context();
}
}

Ctr2MidiDevice::Ctr2MidiDevice(QObject *parent) : QObject(parent) {
    m_eventPollTimer = new QTimer(this);
    m_eventPollTimer->setInterval(8);
    connect(m_eventPollTimer, &QTimer::timeout, this, [this]() {
        for (int count = 0; count < 64; ++count) {
            const int event = QJniObject::callStaticMethod<jint>(
                "com/w9wdx/qk4phone/AndroidBleMidi", "pollEvent", "(I)I",
                static_cast<jint>(AndroidSession));
            if (event < 0)
                break;
            const int status = (event >> 16) & 0xff;
            const int data1 = (event >> 8) & 0x7f;
            const int data2 = event & 0x7f;
            const int kind = status & 0xf0;
            if (kind == 0x80 || kind == 0x90 || kind == 0xb0)
                emit rawMidiEvent(status, data1, data2);
        }
    });

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
            m_eventPollTimer->start();
            emit connected();
        } else {
            m_eventPollTimer->stop();
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
    m_eventPollTimer->stop();
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
