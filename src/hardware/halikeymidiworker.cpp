#include "halikeymidiworker.h"
#include <RtMidi.h>
#include <QDebug>

// MIDI note assignments from HaliKey MIDI user guide
static constexpr unsigned char NOTE_LEFT_PADDLE = 20;
static constexpr unsigned char NOTE_RIGHT_PADDLE = 21;
static constexpr unsigned char NOTE_STRAIGHT_KEY = 30;
static constexpr unsigned char NOTE_PTT = 31;

HaliKeyMidiWorker::HaliKeyMidiWorker(const QString &deviceName, QObject *parent)
    : HaliKeyWorkerBase(deviceName, parent) {}

HaliKeyMidiWorker::~HaliKeyMidiWorker() {
    m_midiIn.reset();
}

void HaliKeyMidiWorker::prepareShutdown() {
    // Close the MIDI port and stop RtMidi's internal callback thread BEFORE
    // the QThread is torn down. RtMidi::closePort() blocks until any in-progress
    // callback finishes, so after this returns no more callbacks can fire.
    // This is safe to call from the main thread — RtMidi synchronizes internally.
    m_midiIn.reset();
    qDebug() << "HaliKeyMidiWorker: MIDI port closed during shutdown";
}

void HaliKeyMidiWorker::start() {
    try {
        m_midiIn = std::make_unique<RtMidiIn>();
    } catch (RtMidiError &error) {
        QString msg = QString("Failed to create MIDI input: %1").arg(QString::fromStdString(error.getMessage()));
        qWarning() << "HaliKeyMidiWorker:" << msg;
        emit errorOccurred(msg);
        return;
    }

    // Find the MIDI port matching our device name
    unsigned int portCount = m_midiIn->getPortCount();
    qDebug() << "HaliKeyMidiWorker: searching for device" << m_portName << "among" << portCount << "MIDI ports";
    int foundPort = -1;

    for (unsigned int i = 0; i < portCount; i++) {
        std::string name = m_midiIn->getPortName(i);
        QString portName = QString::fromStdString(name);
        qDebug() << "HaliKeyMidiWorker: MIDI port" << i << ":" << portName;
        if (portName.contains(m_portName, Qt::CaseInsensitive)) {
            foundPort = static_cast<int>(i);
            break;
        }
    }

    if (foundPort < 0) {
        QString msg = QString("MIDI device '%1' not found (%2 ports available)").arg(m_portName).arg(portCount);
        qWarning() << "HaliKeyMidiWorker:" << msg;
        emit errorOccurred(msg);
        m_midiIn.reset();
        return;
    }

    try {
        m_midiIn->openPort(static_cast<unsigned int>(foundPort));
    } catch (RtMidiError &error) {
        QString msg = QString("Failed to open MIDI port: %1").arg(QString::fromStdString(error.getMessage()));
        qWarning() << "HaliKeyMidiWorker:" << msg;
        emit errorOccurred(msg);
        m_midiIn.reset();
        return;
    }

    // Ignore sysex, timing, and active sensing messages
    m_midiIn->ignoreTypes(true, true, true);

    // Set callback — RtMidi calls this from its internal thread
    m_midiIn->setCallback(&HaliKeyMidiWorker::midiCallback, this);

    m_running = true;
    qDebug() << "HaliKeyMidiWorker: opened MIDI port" << foundPort << "for device" << m_portName;
    emit portOpened();
}

void HaliKeyMidiWorker::midiCallback(double deltaTime, std::vector<unsigned char> *message, void *userData) {
    auto *self = static_cast<HaliKeyMidiWorker *>(userData);
    if (!self->m_running)
        return;
    if (message && !message->empty()) {
        self->handleMidiMessage(deltaTime, *message);
    }
}

void HaliKeyMidiWorker::handleMidiMessage(double deltaTime, const std::vector<unsigned char> &message) {
    if (message.size() < 3)
        return;

    unsigned char status = message[0] & 0xF0; // Strip channel nibble
    unsigned char note = message[1];
    unsigned char velocity = message[2];

    bool pressed = false;
    if (status == 0x90 && velocity > 0) {
        pressed = true; // Note On
    } else if (status == 0x80 || (status == 0x90 && velocity == 0)) {
        pressed = false; // Note Off
    } else {
        return; // Not a note event
    }

    switch (note) {
    case NOTE_LEFT_PADDLE:
        emit ditStateChanged(pressed);
        break;
    case NOTE_RIGHT_PADDLE:
        emit dahStateChanged(pressed);
        break;
    default:
        break; // Ignore PTT (Note 31), straight key (Note 30), and unknown notes
    }
}
