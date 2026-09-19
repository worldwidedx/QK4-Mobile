#include "halikeydevice.h"

#ifdef Q_OS_ANDROID

#include <QDebug>
#include <QJniObject>
#include <qcoreapplication_platform.h>
#include "settings/radiosettings.h"

namespace {
// TinyMIDI's BLE firmware reports the physical left paddle as note 20 and the
// physical right paddle as note 21. Keep this physical mapping separate from
// the K4 KP orientation setting, which is applied later by MainWindow.
QJniObject androidContext() {
    return QNativeInterface::QAndroidApplication::context();
}
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
        const int state = QJniObject::callStaticMethod<jint>(
                "com/w9wdx/qk4phone/AndroidBleMidi", "getConnectionState", "()I");
        if (state != m_androidConnectionState) {
            const int previous = m_androidConnectionState;
            m_androidConnectionState = state;
            m_connected = state == 2;
            if (state == 2) {
                m_androidMidiPollTimer->start();
                emit connected();
            } else if (state == 3) {
                m_androidMidiPollTimer->stop();
                emit connectionError(statusMessage());
                if (previous == 2)
                    emit disconnected();
            } else if (state == 0 && previous == 2) {
                m_androidMidiPollTimer->stop();
                emit disconnected();
            }
        }
    });

    // Eight milliseconds keeps paddle latency below one hundredth of a second
    // while reducing main-thread JNI traffic. This timer runs only while a
    // BLE or USB MIDI endpoint is actually connected.
    m_androidMidiPollTimer = new QTimer(this);
    m_androidMidiPollTimer->setInterval(8);
    connect(m_androidMidiPollTimer, &QTimer::timeout, this, [this]() {
        for (int count = 0; count < 64; ++count) {
            const int event = QJniObject::callStaticMethod<jint>(
                    "com/w9wdx/qk4phone/AndroidBleMidi", "pollEvent", "()I");
            if (event < 0)
                break;
            const int status = (event >> 16) & 0xff;
            const int note = (event >> 8) & 0xff;
            const int velocity = event & 0xff;
            const int kind = status & 0xf0;
            const bool pressed = (kind == 0x90 || kind == 0xb0) && velocity > 0;
            if (kind != 0x80 && kind != 0x90 && kind != 0xb0)
                continue;
            emit rawMidiEvent(status, note, velocity, pressed);

            const auto *settings = RadioSettings::instance();
            const int profile = settings->midiMappingProfile();
            const int ditStatus = profile < 2 ? 0x90 : settings->midiDitStatus();
            const int ditData1 = profile < 2 ? 20 : settings->midiDitData1();
            const int dahStatus = profile < 2 ? 0x90 : settings->midiDahStatus();
            const int dahData1 = profile < 2 ? 21 : settings->midiDahData1();
            // Note-off (0x80) releases the corresponding note-on mapping.
            const int matchKind = kind == 0x80 ? 0x90 : kind;
            if (matchKind == ditStatus && note == ditData1)
                onRawDit(pressed);
            else if (matchKind == dahStatus && note == dahData1)
                onRawDah(pressed);
            else
                qDebug() << "Android MIDI event" << note << "pressed" << pressed;
        }
    });
    m_androidConnectionPollTimer->start();
}

HalikeyDevice::~HalikeyDevice() {
    closePort();
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
    m_androidMidiPollTimer->stop();
    bool wasConnected = m_connected;
    m_androidConnectionState = 0;
    m_connected = false;
    m_rawDitState = false;
    m_rawDahState = false;
    m_rawPttState = false;
    m_confirmedDitState = false;
    m_confirmedDahState = false;
    m_confirmedPttState = false;

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

    // Stop any pending debounce timers
    m_ditDebounceTimer->stop();
    m_dahDebounceTimer->stop();
    m_pttDebounceTimer->stop();

    bool wasConnected = m_connected;
    m_connected = false;
    m_rawDitState = false;
    m_rawDahState = false;
    m_rawPttState = false;
    m_confirmedDitState = false;
    m_confirmedDahState = false;
    m_confirmedPttState = false;

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
