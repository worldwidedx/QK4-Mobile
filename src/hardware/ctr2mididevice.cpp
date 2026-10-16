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

Ctr2MidiDevice::Ctr2MidiDevice(QObject *parent) : QObject(parent) {}
Ctr2MidiDevice::~Ctr2MidiDevice() = default;
void Ctr2MidiDevice::setMapping(const MidiMapping::DeviceMapping &mapping) {
    m_mapping = mapping;
}
bool Ctr2MidiDevice::isPaddleEvent(int, int) const { return false; }
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
