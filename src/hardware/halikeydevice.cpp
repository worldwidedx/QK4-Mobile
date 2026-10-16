#include "halikeydevice.h"

#ifdef Q_OS_ANDROID

#include <QDebug>
#include <QJniObject>
#include <qcoreapplication_platform.h>
#include <chrono>
#include "iambickeyer.h"
#include "settings/radiosettings.h"

namespace {
// TinyMIDI's BLE firmware reports the physical left paddle as note 20 and the
// physical right paddle as note 21. Keep this physical mapping separate from
// the K4 KP orientation setting, which is applied later by MainWindow.
QJniObject androidContext() {
    return QNativeInterface::QAndroidApplication::context();
}
}

// This object lives on the keyer thread. Android's MIDI receiver can deliver
// several old edges together; polling on the UI thread would add another,
// unbounded source of delay before the keyer sees those edges.
class AndroidCwMidiPump : public QObject {
public:
    AndroidCwMidiPump(HalikeyDevice *device, IambicKeyer *keyer)
        : m_device(device), m_keyer(keyer) {
        m_poll = new QTimer(this);
        m_poll->setTimerType(Qt::PreciseTimer);
        m_poll->setInterval(4);
        connect(m_poll, &QTimer::timeout, this, [this]() { poll(); });
        for (int i = 0; i < 2; ++i) {
            m_release[i] = new QTimer(this);
            m_release[i]->setSingleShot(true);
            m_release[i]->setTimerType(Qt::PreciseTimer);
            m_release[i]->setInterval(10);
            connect(m_release[i], &QTimer::timeout, this, [this, i]() {
                if (!m_raw[i] && m_confirmed[i]) {
                    m_confirmed[i] = false;
                    setKeyerPaddle(i, false, m_releaseTime[i]);
                }
            });
        }
    }

    void configure(int ditStatus, int ditNote, int dahStatus, int dahNote,
                   bool paddlesEnabled) {
        const bool mappingChanged = m_ditStatus != ditStatus || m_ditNote != ditNote
                || m_dahStatus != dahStatus || m_dahNote != dahNote;
        if (mappingChanged || m_paddlesEnabled != paddlesEnabled)
            releaseAll();
        m_ditStatus = ditStatus;
        m_ditNote = ditNote;
        m_dahStatus = dahStatus;
        m_dahNote = dahNote;
        m_paddlesEnabled = paddlesEnabled;
    }

    void start() { m_poll->start(); }
    void stop() { m_poll->stop(); releaseAll(); }

private:
    static qint64 nowNs() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    void setKeyerPaddle(int index, bool pressed, qint64 timeNs) {
        if (index == 0)
            m_keyer->setMidiDitPaddle(pressed, timeNs);
        else
            m_keyer->setMidiDahPaddle(pressed, timeNs);
    }

    void releaseAll() {
        for (int i = 0; i < 2; ++i) {
            m_release[i]->stop();
            m_raw[i] = false;
            if (m_confirmed[i]) {
                m_confirmed[i] = false;
                setKeyerPaddle(i, false, nowNs());
            }
        }
    }

    void poll() {
        for (int count = 0; count < 64; ++count) {
            const jlong packed = QJniObject::callStaticMethod<jlong>(
                    "com/w9wdx/qk4phone/AndroidBleMidi", "pollEventTimed", "(I)J", 0);
            if (packed < 0)
                break;
            const int event = static_cast<int>(packed & 0x00ffffff);
            const qint64 ageNs = static_cast<qint64>(packed >> 24);
            const qint64 eventNs = nowNs() - ageNs;
            QMetaObject::invokeMethod(m_device,
                    [device = m_device, event]() { device->processAndroidMidiEvent(event); },
                    Qt::QueuedConnection);
            if (!m_paddlesEnabled)
                continue;
            const int status = (event >> 16) & 0xff;
            const int note = (event >> 8) & 0xff;
            const int velocity = event & 0xff;
            const int kind = status & 0xf0;
            if (kind != 0x80 && kind != 0x90 && kind != 0xb0)
                continue;
            const int matchKind = kind == 0x80 ? 0x90 : kind;
            const int index = matchKind == m_ditStatus && note == m_ditNote ? 0
                    : matchKind == m_dahStatus && note == m_dahNote ? 1 : -1;
            if (index < 0)
                continue;
            const bool pressed = (kind == 0x90 || kind == 0xb0) && velocity > 0;
            m_raw[index] = pressed;
            if (pressed) {
                m_release[index]->stop();
                if (!m_confirmed[index]) {
                    m_confirmed[index] = true;
                    setKeyerPaddle(index, true, eventNs);
                }
            } else if (m_confirmed[index]) {
                m_releaseTime[index] = eventNs;
                m_release[index]->start();
            }
        }
    }

    HalikeyDevice *m_device;
    IambicKeyer *m_keyer;
    QTimer *m_poll = nullptr;
    QTimer *m_release[2]{};
    qint64 m_releaseTime[2]{};
    bool m_raw[2]{};
    bool m_confirmed[2]{};
    bool m_paddlesEnabled = true;
    int m_ditStatus = 0x90;
    int m_dahStatus = 0x90;
    int m_ditNote = 20;
    int m_dahNote = 21;
};

void HalikeyDevice::attachKeyer(IambicKeyer *keyer, QThread *keyerThread) {
    detachKeyer();
    m_androidPump = new AndroidCwMidiPump(this, keyer);
    m_androidPump->moveToThread(keyerThread);
    configureAndroidPump();
    if (m_connected)
        QMetaObject::invokeMethod(m_androidPump, [pump = m_androidPump]() { pump->start(); },
                                  Qt::QueuedConnection);
}

void HalikeyDevice::detachKeyer() {
    if (!m_androidPump)
        return;
    QMetaObject::invokeMethod(m_androidPump, [pump = m_androidPump]() {
        pump->stop();
        pump->deleteLater();
    }, Qt::BlockingQueuedConnection);
    m_androidPump = nullptr;
}

void HalikeyDevice::configureAndroidPump() {
    if (!m_androidPump)
        return;
    const auto *settings = RadioSettings::instance();
    const bool defaults = settings->midiMappingProfile() < 2;
    const int ditStatus = defaults ? 0x90 : settings->midiDitStatus();
    const int ditNote = defaults ? 20 : settings->midiDitData1();
    const int dahStatus = defaults ? 0x90 : settings->midiDahStatus();
    const int dahNote = defaults ? 21 : settings->midiDahData1();
    const bool paddlesEnabled = settings->cwMidiKeyingMode() == 0;
    QMetaObject::invokeMethod(m_androidPump,
            [pump = m_androidPump, ditStatus, ditNote, dahStatus, dahNote, paddlesEnabled]() {
                pump->configure(ditStatus, ditNote, dahStatus, dahNote, paddlesEnabled);
            }, Qt::QueuedConnection);
}
HalikeyDevice::HalikeyDevice(QObject *parent) : QObject(parent) {
    m_ditDebounceTimer = new QTimer(this);
    m_dahDebounceTimer = new QTimer(this);
    m_pttDebounceTimer = new QTimer(this);
    for (QTimer *timer : {m_ditDebounceTimer, m_dahDebounceTimer, m_pttDebounceTimer}) {
        timer->setSingleShot(true);
        timer->setInterval(DEBOUNCE_MS);
    }
    connect(m_ditDebounceTimer, &QTimer::timeout, this, [this]() {
        if (m_rawDitState != m_confirmedDitState) {
            m_confirmedDitState = m_rawDitState;
            emitMappedPaddle(true, m_confirmedDitState);
        }
    });
    connect(m_dahDebounceTimer, &QTimer::timeout, this, [this]() {
        if (m_rawDahState != m_confirmedDahState) {
            m_confirmedDahState = m_rawDahState;
            emitMappedPaddle(false, m_confirmedDahState);
        }
    });
    connect(m_pttDebounceTimer, &QTimer::timeout, this, [this]() {
        if (m_rawPttState != m_confirmedPttState) {
            m_confirmedPttState = m_rawPttState;
            emit pttStateChanged(m_confirmedPttState);
        }
    });

    // Connection transitions do not need paddle-rate polling. Keeping this
    // separate avoids a JNI status call every few milliseconds for the life
    // of the app, including while no MIDI device is connected.
    m_androidConnectionPollTimer = new QTimer(this);
    m_androidConnectionPollTimer->setInterval(250);
    connect(m_androidConnectionPollTimer, &QTimer::timeout, this, [this]() {
        configureAndroidPump();
        const int state = QJniObject::callStaticMethod<jint>(
                "com/w9wdx/qk4phone/AndroidBleMidi", "getConnectionState", "()I");
        if (state != m_androidConnectionState) {
            const int previous = m_androidConnectionState;
            m_androidConnectionState = state;
            m_connected = state == 2;
            if (state == 2) {
                if (m_androidPump)
                    QMetaObject::invokeMethod(m_androidPump,
                            [pump = m_androidPump]() { pump->start(); }, Qt::QueuedConnection);
                emit connected();
            } else if (state == 3) {
                if (m_androidPump)
                    QMetaObject::invokeMethod(m_androidPump,
                            [pump = m_androidPump]() { pump->stop(); }, Qt::QueuedConnection);
                resetInputState();
                emit connectionError(statusMessage());
                if (previous == 2)
                    emit disconnected();
            } else if (state == 0 && previous == 2) {
                if (m_androidPump)
                    QMetaObject::invokeMethod(m_androidPump,
                            [pump = m_androidPump]() { pump->stop(); }, Qt::QueuedConnection);
                resetInputState();
                emit disconnected();
            }
        }
    });

    m_androidConnectionPollTimer->start();
}

HalikeyDevice::~HalikeyDevice() {
    closePort();
    detachKeyer();
}

void HalikeyDevice::processAndroidMidiEvent(int event) {
    if (!m_connected)
        return;
    const int status = (event >> 16) & 0xff;
    const int note = (event >> 8) & 0xff;
    const int velocity = event & 0xff;
    const int kind = status & 0xf0;
    if (kind != 0x80 && kind != 0x90 && kind != 0xb0)
        return;
    const bool pressed = (kind == 0x90 || kind == 0xb0) && velocity > 0;
    emit rawMidiEvent(status, note, velocity, pressed);
    const auto *settings = RadioSettings::instance();
    const bool defaults = settings->midiMappingProfile() < 2;
    const int ditStatus = defaults ? 0x90 : settings->midiDitStatus();
    const int ditNote = defaults ? 20 : settings->midiDitData1();
    const int dahStatus = defaults ? 0x90 : settings->midiDahStatus();
    const int dahNote = defaults ? 21 : settings->midiDahData1();
    const int matchKind = kind == 0x80 ? 0x90 : kind;
    if (matchKind == ditStatus && note == ditNote)
        onRawDit(pressed);
    else if (matchKind == dahStatus && note == dahNote)
        onRawDah(pressed);
}

void HalikeyDevice::onRawDit(bool pressed) {
    m_rawDitState = pressed;
    if (pressed && !m_confirmedDitState) {
        m_confirmedDitState = true;
        m_ditDebounceTimer->stop();
        emitMappedPaddle(true, true);
    } else {
        m_ditDebounceTimer->start();
    }
}

void HalikeyDevice::onRawDah(bool pressed) {
    m_rawDahState = pressed;
    if (pressed && !m_confirmedDahState) {
        m_confirmedDahState = true;
        m_dahDebounceTimer->stop();
        emitMappedPaddle(false, true);
    } else {
        m_dahDebounceTimer->start();
    }
}

void HalikeyDevice::onRawPtt(bool pressed) {
    m_rawPttState = pressed;
    if (pressed && !m_confirmedPttState) {
        m_confirmedPttState = true;
        m_pttDebounceTimer->stop();
        emit pttStateChanged(true);
    } else {
        m_pttDebounceTimer->start();
    }
}

bool HalikeyDevice::openPort(const QString &portName) {
    m_portName = portName;
    const QJniObject context = androidContext();
    const QJniObject address = QJniObject::fromString(portName);
    if (!context.isValid() || !QJniObject::callStaticMethod<jboolean>(
            "com/w9wdx/qk4phone/AndroidBleMidi", "connect",
            "(Landroid/content/Context;Ljava/lang/String;)Z", context.object(), address.object<jstring>())) {
        emit connectionError(statusMessage());
        return false;
    }
    m_androidConnectionState = 1;
    return true;
}

void HalikeyDevice::closePort() {
    QJniObject::callStaticMethod<void>("com/w9wdx/qk4phone/AndroidBleMidi", "disconnect", "()V");
    if (m_androidPump)
        QMetaObject::invokeMethod(m_androidPump,
                [pump = m_androidPump]() { pump->stop(); }, Qt::BlockingQueuedConnection);
    resetInputState();
    bool wasConnected = m_connected;
    m_androidConnectionState = 0;
    m_connected = false;

    if (wasConnected) {
        emit disconnected();
    }
}

bool HalikeyDevice::isConnected() const {
    return m_connected;
}

QString HalikeyDevice::portName() const {
    return m_portName;
}

QStringList HalikeyDevice::availablePorts() {
    return {};
}

QList<HaliKeyPortInfo> HalikeyDevice::availablePortsDetailed() {
    return {};
}

QStringList HalikeyDevice::availableMidiDevices() {
    const QJniObject devices = QJniObject::callStaticObjectMethod(
            "com/w9wdx/qk4phone/AndroidBleMidi", "getDevices", "()Ljava/lang/String;");
    return devices.isValid() ? devices.toString().split('\n', Qt::SkipEmptyParts) : QStringList{};
}

void HalikeyDevice::startMidiScan() {
    const QJniObject context = androidContext();
    if (context.isValid())
        QJniObject::callStaticMethod<void>("com/w9wdx/qk4phone/AndroidBleMidi", "startScan",
                                           "(Landroid/content/Context;)V", context.object());
}

QString HalikeyDevice::statusMessage() const {
    const QJniObject message = QJniObject::callStaticObjectMethod(
            "com/w9wdx/qk4phone/AndroidBleMidi", "getStatusMessage", "()Ljava/lang/String;");
    return message.isValid() ? message.toString() : QStringLiteral("Android MIDI unavailable");
}

bool HalikeyDevice::ditPressed() const {
    return m_confirmedDitState;
}

bool HalikeyDevice::dahPressed() const {
    return m_confirmedDahState;
}

#else

#include "halikeymidiworker.h"
#ifndef Q_OS_IOS
#include "halikeyv14worker.h" // serial/HID V14 keyer — desktop only (no serial on iOS)
#endif
#include "halikeyworkerbase.h"
#include "../settings/radiosettings.h"
#include <QDebug>
#include <RtMidi.h>

HalikeyDevice::HalikeyDevice(QObject *parent) : QObject(parent) {
    // Debounce timers — single-shot, fire once after DEBOUNCE_MS of stable state
    m_ditDebounceTimer = new QTimer(this);
    m_ditDebounceTimer->setSingleShot(true);
    m_ditDebounceTimer->setInterval(DEBOUNCE_MS);
    connect(m_ditDebounceTimer, &QTimer::timeout, this, [this]() {
        if (m_rawDitState != m_confirmedDitState) {
            m_confirmedDitState = m_rawDitState;
            emitMappedPaddle(true, m_confirmedDitState);
        }
    });

    m_dahDebounceTimer = new QTimer(this);
    m_dahDebounceTimer->setSingleShot(true);
    m_dahDebounceTimer->setInterval(DEBOUNCE_MS);
    connect(m_dahDebounceTimer, &QTimer::timeout, this, [this]() {
        if (m_rawDahState != m_confirmedDahState) {
            m_confirmedDahState = m_rawDahState;
            emitMappedPaddle(false, m_confirmedDahState);
        }
    });

    m_pttDebounceTimer = new QTimer(this);
    m_pttDebounceTimer->setSingleShot(true);
    m_pttDebounceTimer->setInterval(DEBOUNCE_MS);
    connect(m_pttDebounceTimer, &QTimer::timeout, this, [this]() {
        if (m_rawPttState != m_confirmedPttState) {
            m_confirmedPttState = m_rawPttState;
            emit pttStateChanged(m_confirmedPttState);
        }
    });
}

HalikeyDevice::~HalikeyDevice() {
    closePort();
}

void HalikeyDevice::onRawDit(bool pressed) {
    m_rawDitState = pressed;
    if (pressed && !m_confirmedDitState) {
        // Key down — emit immediately for zero latency
        m_confirmedDitState = true;
        m_ditDebounceTimer->stop();
        emitMappedPaddle(true, true);
    } else {
        // Key up or redundant key down — debounce
        m_ditDebounceTimer->start();
    }
}

void HalikeyDevice::onRawDah(bool pressed) {
    m_rawDahState = pressed;
    if (pressed && !m_confirmedDahState) {
        m_confirmedDahState = true;
        m_dahDebounceTimer->stop();
        emitMappedPaddle(false, true);
    } else {
        m_dahDebounceTimer->start();
    }
}

void HalikeyDevice::onRawPtt(bool pressed) {
    m_rawPttState = pressed;
    if (pressed && !m_confirmedPttState) {
        m_confirmedPttState = true;
        m_pttDebounceTimer->stop();
        emit pttStateChanged(true);
    } else {
        m_pttDebounceTimer->start();
    }
}

bool HalikeyDevice::openPort(const QString &portName) {
    if (m_connected) {
        closePort();
    }

    m_portName = portName;
    m_rawDitState = false;
    m_rawDahState = false;
    m_rawPttState = false;
    m_confirmedDitState = false;
    m_confirmedDahState = false;
    m_confirmedPttState = false;

    // Create worker based on configured device type.
#ifdef Q_OS_IOS
    // iOS keyer is MIDI-only (CoreMIDI via RtMidi); the serial/HID V14 path is
    // not built on iOS, so always use the MIDI worker regardless of the setting.
    m_worker = new HaliKeyMidiWorker(portName);
#else
    int deviceType = RadioSettings::instance()->halikeyDeviceType();
    if (deviceType == 1) {
        m_worker = new HaliKeyMidiWorker(portName);
    } else {
        m_worker = new HaliKeyV14Worker(portName);
    }
#endif

    m_workerThread = new QThread(this);
    m_worker->moveToThread(m_workerThread);

    // Wire worker signals through debounce handlers
    connect(m_worker, &HaliKeyWorkerBase::ditStateChanged, this, &HalikeyDevice::onRawDit);
    connect(m_worker, &HaliKeyWorkerBase::dahStateChanged, this, &HalikeyDevice::onRawDah);
    connect(m_worker, &HaliKeyWorkerBase::pttStateChanged, this, &HalikeyDevice::onRawPtt);
    connect(m_worker, &HaliKeyWorkerBase::portOpened, this, [this]() {
        m_connected = true;
        emit connected();
    });
    connect(m_worker, &HaliKeyWorkerBase::errorOccurred, this, [this](const QString &error) {
        qWarning() << "HalikeyDevice: Worker error -" << error;
        closePort();
        emit connectionError(error);
    });

    // Start worker when thread starts
    connect(m_workerThread, &QThread::started, m_worker, &HaliKeyWorkerBase::start);

    // Clean up worker when thread finishes
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_workerThread->start();
    return true;
}

void HalikeyDevice::closePort() {
    if (m_worker) {
        m_worker->stop();
        m_worker->prepareShutdown();
    }

    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(2000);
        m_workerThread->deleteLater();
        m_workerThread = nullptr;
    }

    m_worker = nullptr; // Deleted by QThread::finished -> deleteLater

    resetInputState();
    bool wasConnected = m_connected;
    m_connected = false;

    if (wasConnected) {
        emit disconnected();
    }
}

bool HalikeyDevice::isConnected() const {
    return m_connected;
}

QString HalikeyDevice::portName() const {
    return m_portName;
}

QStringList HalikeyDevice::availablePorts() {
#ifdef Q_OS_IOS
    // No serial ports on iOS; the keyer is MIDI-only (see availableMidiDevices).
    return {};
#else
    QStringList ports;
    const auto portInfos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : portInfos) {
        ports.append(info.portName());
    }
    return ports;
#endif
}

QList<HaliKeyPortInfo> HalikeyDevice::availablePortsDetailed() {
#ifdef Q_OS_IOS
    return {};
#else
    QList<HaliKeyPortInfo> ports;
    const auto portInfos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : portInfos) {
        HaliKeyPortInfo pi;
        pi.portName = info.portName();
        ports.append(pi);
    }
    return ports;
#endif
}

QStringList HalikeyDevice::availableMidiDevices() {
    // System virtual MIDI devices to exclude from the list
    static const QStringList excludedPrefixes = {"IAC Driver"};

    QStringList devices;
    try {
        RtMidiIn midi;
        unsigned int portCount = midi.getPortCount();
        for (unsigned int i = 0; i < portCount; i++) {
            QString name = QString::fromStdString(midi.getPortName(i));
            bool excluded = false;
            for (const QString &prefix : excludedPrefixes) {
                if (name.startsWith(prefix, Qt::CaseInsensitive)) {
                    excluded = true;
                    break;
                }
            }
            if (!excluded) {
                devices.append(name);
            }
        }
    } catch (RtMidiError &error) {
        qWarning() << "HalikeyDevice: MIDI enumeration failed:" << QString::fromStdString(error.getMessage());
    }
    return devices;
}

void HalikeyDevice::startMidiScan() {
    // Desktop RtMidi enumeration is synchronous in availableMidiDevices().
}

QString HalikeyDevice::statusMessage() const {
    return m_connected ? QString("Connected to %1").arg(m_portName) : QStringLiteral("Not connected");
}

bool HalikeyDevice::ditPressed() const {
    return m_confirmedDitState;
}

bool HalikeyDevice::dahPressed() const {
    return m_confirmedDahState;
}

#endif

void HalikeyDevice::resetInputState() {
    m_ditDebounceTimer->stop();
    m_dahDebounceTimer->stop();
    m_pttDebounceTimer->stop();
    m_rawDitState = false;
    m_rawDahState = false;
    m_rawPttState = false;
    m_confirmedDitState = false;
    m_confirmedDahState = false;
    m_confirmedPttState = false;
}

void HalikeyDevice::emitMappedPaddle(bool physicalLeft, bool pressed) {
    const RadioSettings *settings = RadioSettings::instance();
    if (settings->cwMidiKeyingMode() == 0) {
        if (physicalLeft)
            emit ditStateChanged(pressed);
        else
            emit dahStateChanged(pressed);
        return;
    }

    const bool selectedLeft = settings->cwMidiStraightKeyInput() == 0;
    if (physicalLeft == selectedLeft)
        emit straightKeyStateChanged(pressed);
}
