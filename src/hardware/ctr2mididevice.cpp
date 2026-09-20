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

Ctr2MidiDevice::Ctr2MidiDevice(QObject *parent) : QObject(parent) {}
Ctr2MidiDevice::~Ctr2MidiDevice() = default;
bool Ctr2MidiDevice::openPort(const QString &deviceKey) {
    m_portName = deviceKey;
    emit connectionError(QStringLiteral("CTR2-MIDI is available on Android builds."));
    return false;
}
void Ctr2MidiDevice::closePort() {}
bool Ctr2MidiDevice::isConnected() const { return false; }
QString Ctr2MidiDevice::portName() const { return m_portName; }
QString Ctr2MidiDevice::statusMessage() const {
    return QStringLiteral("CTR2-MIDI is available on Android builds.");
}
QStringList Ctr2MidiDevice::availableMidiDevices() { return {}; }
void Ctr2MidiDevice::startMidiScan() {}

#endif
