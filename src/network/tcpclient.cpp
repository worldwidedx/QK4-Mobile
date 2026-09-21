#include "tcpclient.h"
#include <QDebug>
#include <QHostAddress>
#include <QHostInfo>
#include <QDateTime>
#include <cmath>

TcpClient::TcpClient(QObject *parent)
    : QObject(parent), m_socket(new PskTlsSocket(this)), m_protocol(new Protocol(this)), m_connectTimer(new QTimer(this)),
      m_authTimer(new QTimer(this)),
      m_pingTimer(new QTimer(this)), m_port(K4Protocol::DEFAULT_PORT), m_useTls(false), m_encodeMode(3),
      m_streamingLatency(3), m_authResponseReceived(false) {
    // Socket signals
    connect(m_socket, &PskTlsSocket::connected, this, &TcpClient::onSocketConnected);
    connect(m_socket, &PskTlsSocket::encrypted, this, &TcpClient::onSocketEncrypted);
    connect(m_socket, &PskTlsSocket::disconnected, this, &TcpClient::onSocketDisconnected);
    connect(m_socket, &PskTlsSocket::readyRead, this, &TcpClient::onReadyRead);
    connect(m_socket, &PskTlsSocket::errorOccurred, this, &TcpClient::onSocketError);

    // Connect timeout timer (single shot) - covers TCP/TLS handshake phase
    m_connectTimer->setSingleShot(true);
    connect(m_connectTimer, &QTimer::timeout, this, &TcpClient::onConnectTimeout);

    // Auth timeout timer (single shot)
    m_authTimer->setSingleShot(true);
    connect(m_authTimer, &QTimer::timeout, this, &TcpClient::onAuthTimeout);

    // Ping timer for keep-alive
    m_pingTimer->setInterval(K4Protocol::PING_INTERVAL_MS);
    connect(m_pingTimer, &QTimer::timeout, this, &TcpClient::onPingTimer);

    // Protocol signals - any packet means auth succeeded
    connect(m_protocol, &Protocol::packetReceived, this, [this](quint8 type, const QByteArray &payload) {
        Q_UNUSED(payload)
        if (m_state.load(std::memory_order_acquire) == Authenticating && !m_authResponseReceived) {
            m_authResponseReceived = true;
            m_connectTimer->stop();
            m_authTimer->stop();
            qDebug() << "Authentication successful, received packet type:" << type;
            setState(Connected);
            emit authenticated();
            startPingTimer();

            // Send initialization sequence
            // RDY triggers comprehensive state dump containing all radio state:
            // FA, FB, MD, MD$, BW, BW$, IS, CW, KS, PC, SD (per mode), SQ, RG, SQ$, RG$,
            // #SPN, #REF, VXC, VXV, VXD, and all menu definitions (MEDF)
            sendCAT(K4Protocol::Commands::READY);              // Triggers comprehensive state dump
            sendCAT(K4Protocol::Commands::ENABLE_K4_MODE);     // Enable advanced K4 protocol mode
            sendCAT(K4Protocol::Commands::ENABLE_LONG_ERRORS); // Request long format error messages
            // Set audio encode mode (0=RAW32, 1=RAW16, 2=Opus Int, 3=Opus Float)
            qDebug() << "Sending:" << QString("EM%1;").arg(m_encodeMode);
            sendCAT(QString("EM%1;").arg(m_encodeMode));
            // Set streaming audio latency (0-7, higher values for high-latency connections)
            qDebug() << "Sending:" << QString("SL%1;").arg(m_streamingLatency);
            sendCAT(QString("SL%1;").arg(m_streamingLatency));
        }
    });
    connect(m_protocol, &Protocol::catResponseReceived, this, &TcpClient::onCatResponse);
    m_digitalTimer = new QTimer(this);
    m_digitalTimer->setInterval(50);
    m_digitalTimer->setTimerType(Qt::PreciseTimer);
    connect(m_digitalTimer, &QTimer::timeout, this, &TcpClient::serviceDigitalTxProtection);
    m_digitalClock.start();
}

TcpClient::~TcpClient() {
    stopPingTimer();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
}

void TcpClient::connectToHost(const QString &host, quint16 port, const QString &password, bool useTls,
                              const QString &identity, int encodeMode, int streamingLatency) {
    if (m_state.load(std::memory_order_acquire) != Disconnected) {
        disconnectFromHost();
    }

    m_host = host;
    m_port = port;
    m_password = password; // Also used as PSK when TLS enabled
    m_useTls = useTls;
    m_identity = identity;                 // TLS-PSK identity (optional)
    m_encodeMode = encodeMode;             // Audio encode mode (0=RAW32, 1=RAW16, 2=Opus Int, 3=Opus Float)
    m_streamingLatency = streamingLatency; // Remote streaming audio latency (0-7)
    m_authResponseReceived = false;

    setState(Connecting);
    // Start this before a possible .local lookup as well as the TCP/TLS phase:
    // an unreachable hostname must not leave the phone permanently Connecting.
    m_connectTimer->start(K4Protocol::CONNECTION_TIMEOUT_MS);

    // K4 servers are commonly advertised as <name>.local. Resolve those names
    // explicitly and prefer IPv4 because the K4 remote server listens on IPv4.
    // Qt's SSL socket does not reliably use Android's mDNS resolver directly.
    if (m_host.endsWith(QStringLiteral(".local"), Qt::CaseInsensitive)) {
        const QString requestedHost = m_host;
        qDebug() << "Resolving mDNS hostname:" << requestedHost;
        QHostInfo::lookupHost(requestedHost, this, [this, requestedHost](const QHostInfo &info) {
            // The user may have cancelled or the overall connection timeout may
            // have fired while name resolution was in progress.
            if (m_state.load(std::memory_order_acquire) != Connecting)
                return;

            if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
                m_connectTimer->stop();
                emit errorOccurred(QString("Could not resolve %1: %2").arg(requestedHost, info.errorString()));
                setState(Disconnected);
                return;
            }

            QString resolvedHost;
            for (const QHostAddress &address : info.addresses()) {
                if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                    resolvedHost = address.toString();
                    break;
                }
            }
            if (resolvedHost.isEmpty())
                resolvedHost = info.addresses().first().toString();

            qDebug() << "Resolved" << requestedHost << "to" << resolvedHost;
            m_host = resolvedHost;
            attemptConnection();
        });
        return;
    }

    attemptConnection();
}

void TcpClient::attemptConnection() {
    if (m_useTls) {
        // Fail before opening the K4 socket if no PSK-capable TLS backend is
        // available, rather than reporting an opaque socket error later.
        if (!PskTlsSocket::tlsAvailable()) {
            m_connectTimer->stop();
            emit errorOccurred(QStringLiteral("TLS is unavailable: the OpenSSL runtime could not be loaded."));
            setState(Disconnected);
            return;
        }
        qDebug() << "TLS library:" << PskTlsSocket::tlsLibraryVersion();
        m_socket->setPreSharedKey(m_identity.toUtf8(), m_password.toUtf8());
        qDebug() << "Connecting with TLS/PSK to" << m_host << ":" << m_port
                 << "identity:" << (m_identity.isEmpty() ? QStringLiteral("(empty)") : m_identity);
        m_socket->connectToHostEncrypted(m_host, m_port);
    } else {
        qDebug() << "Connecting (unencrypted) to" << m_host << ":" << m_port;
        m_socket->connectToHost(m_host, m_port);
    }
}

void TcpClient::disconnectFromHost() {
    m_digitalGuard.stop();
    m_digitalTimer->stop();
    m_sstvAudioGate = false;
    stopPingTimer();
    m_connectTimer->stop();
    m_authTimer->stop();

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        // Send graceful disconnect command
        if (m_state.load(std::memory_order_acquire) == Connected) {
            sendCAT(K4Protocol::Commands::DISCONNECT);
        }
        m_socket->disconnectFromHost();
    }

    setState(Disconnected);
}

bool TcpClient::isConnected() const {
    return m_connected.load(std::memory_order_relaxed);
}

TcpClient::ConnectionState TcpClient::connectionState() const {
    return m_state.load(std::memory_order_acquire);
}

void TcpClient::sendCAT(const QString &command) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "sendCAT", Qt::QueuedConnection, Q_ARG(QString, command));
        return;
    }
    if (m_state.load(std::memory_order_acquire) == Connected) {
        QByteArray packet = Protocol::buildCATPacket(command);
        m_socket->write(packet);
        m_socket->flush(); // Ensure immediate send
    }
}

void TcpClient::sendMacro(const QString &command) {
    if (command.trimmed().isEmpty())
        return;
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "sendMacro", Qt::QueuedConnection, Q_ARG(QString, command));
        return;
    }
    // AI4 does not echo this client's SETs. Reuse the connection's full state
    // readback after the complete macro, including the extra display queries.
    // Keep these writes together on the I/O thread; do not rerun connection
    // setup or infer state from arbitrary macro text (switches, toggles, etc.).
    sendCAT(command);
    sendCAT(K4Protocol::Commands::READY);
    sendCAT(K4Protocol::Commands::ADDITIONAL_STATE_QUERIES);
}

void TcpClient::sendRaw(const QByteArray &data) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "sendRaw", Qt::QueuedConnection, Q_ARG(QByteArray, data));
        return;
    }
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(data);
    }
}

void TcpClient::beginSstvAudioTransmit(quint64 generation) {
    beginDigitalAudioTransmit(int(DigitalTxGuard::Mode::Sstv), generation);
}

void TcpClient::acknowledgeDigitalTxFault() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "acknowledgeDigitalTxFault", Qt::QueuedConnection);
        return;
    }
    m_digitalGuard.acknowledge();
}

void TcpClient::beginScheduledDigitalAudio(int mode, quint64 generation) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, mode, generation] { beginScheduledDigitalAudio(mode, generation); });
        return;
    }
    if (m_digitalControl->scheduledGeneration.load() != generation) return;
    beginDigitalAudioTransmit(mode, generation);
}
void TcpClient::stopScheduledDigitalAudio(quint64 generation) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, generation] { stopScheduledDigitalAudio(generation); });
        return;
    }
    auto expected = generation;
    m_digitalControl->scheduledGeneration.compare_exchange_strong(expected, 0);
    if (m_sstvAudioGate && m_sstvAudioGeneration == generation)
        stopDigitalAudioAndUnkey();
    emit scheduledDigitalAudioStopped(generation);
}
void TcpClient::beginDigitalAudioTransmit(int mode, quint64 generation) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "beginDigitalAudioTransmit", Qt::QueuedConnection,
                                  Q_ARG(int, mode), Q_ARG(quint64, generation));
        return;
    }
    const auto reject = [this, mode, generation](const QString &reason) {
        emit digitalAudioTransmitFailed(mode, reason, generation);
        if (mode == int(DigitalTxGuard::Mode::Sstv))
            emit sstvAudioTransmitFailed(reason, generation);
        if (m_calibrationPhase == CalibrationPhase::Running && mode == m_calibrationMode
            && generation == m_calibrationGeneration)
            finishDigitalCalibration(false, reason);
    };
    if (m_calibrationPhase != CalibrationPhase::None
        && (m_calibrationPhase != CalibrationPhase::Running || mode != m_calibrationMode
            || generation != m_calibrationGeneration)) {
        reject(QStringLiteral("TX not started: audio calibration is in progress."));
        return;
    }
    if (mode < int(DigitalTxGuard::Mode::Ft8) || mode > int(DigitalTxGuard::Mode::Sstv)) {
        reject(QStringLiteral("Digital transmission requested an unknown mode."));
        return;
    }
    if (m_state.load(std::memory_order_acquire) != Connected
        || m_socket->state() != QAbstractSocket::ConnectedState) {
        reject(QStringLiteral("The K4 connection is no longer available."));
        return;
    }
    if (!m_digitalGuard.begin(DigitalTxGuard::Mode(mode), generation, m_digitalClock.elapsed(),
                             m_calibrationPhase == CalibrationPhase::Running)) {
        reject(m_digitalGuard.latched() ? m_digitalGuard.reason()
                                       : QStringLiteral("Digital transmit protection is already in use."));
        return;
    }
    // CAT and gate changes execute on the same I/O thread, keeping the
    // lifecycle ordered relative to program-audio writes. MainWindow applies
    // a fixed key-up guard before releasing SSTV audio; a state query is not
    // required because some working K4 connections do not echo TQ1 here.
    // Enable/query meters only for a deliberate digital transmission. A radio
    // that cannot supply fresh TM readings fails closed through the watchdog.
    if (mode != int(DigitalTxGuard::Mode::Sstv) && m_calibrationPhase == CalibrationPhase::None
        && m_digitalControl->scheduledGeneration.load() != generation) {
        m_digitalGuard.stop();
        return;
    }
    m_socket->write(Protocol::buildCATPacket(QStringLiteral("TM1;TM;TX;")));
    m_socket->flush();
    m_sstvAudioGate = true;
    m_sstvAudioGeneration = generation;
    m_nextMeterQuery = m_digitalClock.elapsed() + 250;
    m_digitalTimer->start();
    emit digitalTxProtectionStatus(mode, QStringLiteral("TX protection: waiting for K4 ALC readings"), false, generation);
    qInfo() << "SSTV TX CAT command queued" << "generation" << generation;
    emit digitalAudioKeyRequested(mode, generation);
    if (mode == int(DigitalTxGuard::Mode::Sstv))
        emit sstvAudioKeyRequested(generation);
}

void TcpClient::sendSstvAudio(const QByteArray &data, int emittedSamples, int totalSamples,
                              int imageSamples, quint64 generation) {
    sendDigitalAudio(data, emittedSamples, totalSamples, imageSamples, generation);
}
void TcpClient::sendDigitalAudio(const QByteArray &data, int emittedSamples, int totalSamples,
                                int imageSamples, quint64 generation) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "sendDigitalAudio", Qt::QueuedConnection,
                                  Q_ARG(QByteArray, data), Q_ARG(int, emittedSamples), Q_ARG(int, totalSamples),
                                  Q_ARG(int, imageSamples),
                                  Q_ARG(quint64, generation));
        return;
    }
    if (m_sstvAudioGate && generation == m_sstvAudioGeneration
        && m_socket->state() == QAbstractSocket::ConnectedState) {
        handleDigitalTxAction(m_digitalGuard.tick(m_digitalClock.elapsed()));
        if (!m_sstvAudioGate || !m_digitalControl->allows(generation))
            return;
        // Bound queued program audio if the K4 link stalls. Brief TCP
        // buffering is already covered by this limit; once exceeded, stop
        // rather than dropping a tone packet and transmitting a corrupt image.
        const qint64 backlogLimit = qMax<qint64>(8192, data.size() * 4LL);
        if (m_socket->bytesToWrite() > backlogLimit) {
            handleDigitalTxAction(m_digitalGuard.trip(QStringLiteral("TX stopped: the K4 audio link stalled.")));
            return;
        }
        const qint64 written = m_socket->write(data);
        if (written == data.size()) {
            if (!data.isEmpty()) m_digitalGuard.audioAccepted(m_digitalClock.elapsed());
            if (totalSamples > 0 && emittedSamples >= totalSamples
                && m_digitalControl->scheduledGeneration.load() == generation)
                confirmScheduledAudioDrained(emittedSamples, totalSamples, imageSamples, generation, m_digitalClock.elapsed() + 500);
            else
                emit sstvAudioAccepted(emittedSamples, totalSamples, imageSamples, generation);
        } else {
            handleDigitalTxAction(m_digitalGuard.trip(QStringLiteral("TX stopped: the K4 audio link rejected a packet.")));
        }
    }
}

void TcpClient::confirmScheduledAudioDrained(int emitted, int total, int image, quint64 gen, qint64 deadline) {
    if (!m_sstvAudioGate || !m_digitalControl->allows(gen) || m_sstvAudioGeneration != gen) return;
    if (m_socket->bytesToWrite() == 0) {
        emit sstvAudioAccepted(emitted, total, image, gen);
    } else if (m_digitalClock.elapsed() >= deadline) {
        handleDigitalTxAction(m_digitalGuard.trip("TX stopped: the final audio packet did not drain."));
    } else {
        QTimer::singleShot(10, this, [this, emitted, total, image, gen, deadline] {
            confirmScheduledAudioDrained(emitted, total, image, gen, deadline);
        });
    }
}
void TcpClient::stopSstvAudioAndUnkey() {
    stopDigitalAudioAndUnkey();
}
void TcpClient::stopDigitalAudioAndUnkey() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "stopDigitalAudioAndUnkey", Qt::QueuedConnection);
        return;
    }
    if (m_calibrationPhase != CalibrationPhase::None) {
        cancelDigitalCalibration();
        return;
    }
    m_sstvAudioGate = false;
    m_digitalGuard.stop();
    m_digitalTimer->stop();
    ++m_sstvAudioGeneration;
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(Protocol::buildCATPacket(QStringLiteral("RX;")));
        m_socket->flush();
    }
}

void TcpClient::setState(ConnectionState state) {
    if (m_state.load(std::memory_order_acquire) != state) {
        m_state.store(state, std::memory_order_release);
        m_connected.store(state == Connected, std::memory_order_relaxed);
        emit stateChanged(state);

        if (state == Connected) {
            emit connected();
        } else if (state == Disconnected) {
            m_digitalGuard.stop();
            m_digitalTimer->stop();
            m_sstvAudioGate = false;
            if (m_calibrationPhase != CalibrationPhase::None) {
                m_calibrationSuccess = false;
                m_calibrationText = "Calibration stopped: connection lost. Verify K4 TEST mode; previous saved level retained.";
                completeDigitalCalibration();
            }
            emit disconnected();
        }
    }
}

void TcpClient::onSocketConnected() {
    // Match current QK4: audio and CAT are small, timing-sensitive writes.
    // Android's default Nagle behavior can combine them behind the K4's
    // delayed ACKs long enough to starve TX audio and make the radio unkey.
    // Apply these after connect so they target the live socket descriptor.
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    if (m_useTls) {
        // TLS connection: TCP connected, now waiting for TLS handshake to complete
        // The encrypted() signal will fire when TLS is fully established
        qDebug() << "TCP connected, starting TLS handshake...";
        // Don't change state yet - wait for encrypted() signal
    } else {
        // Non-TLS: need to send SHA-384 password hash
        qDebug() << "Socket connected, sending authentication...";
        m_connectTimer->stop();
        setState(Authenticating);
        sendAuthentication();
        m_authTimer->start(K4Protocol::AUTH_TIMEOUT_MS);
    }
}

void TcpClient::onSocketEncrypted() {
    // TLS handshake completed successfully
    qDebug() << "=== TLS/PSK Connection Established ===";
    qDebug() << "  Negotiated cipher:" << m_socket->sessionCipher();
    m_connectTimer->stop();
    setState(Authenticating);
    // Start auth timeout - waiting for first packet to confirm connection works
    m_authTimer->start(K4Protocol::AUTH_TIMEOUT_MS);
    // Note: For TLS/PSK, no additional password auth needed - data flows immediately
}

void TcpClient::onSocketDisconnected() {
    qDebug() << "Socket disconnected";
    m_sstvAudioGate = false;
    stopPingTimer();
    m_connectTimer->stop();
    m_authTimer->stop();

    if (m_state.load(std::memory_order_acquire) == Authenticating && !m_authResponseReceived) {
        emit authenticationFailed();
        emit errorOccurred("Authentication failed - connection closed by radio");
    }

    setState(Disconnected);
}

void TcpClient::onReadyRead() {
    QByteArray data = m_socket->readAll();
    m_protocol->parse(data);
}

void TcpClient::onSocketError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error)
    stopPingTimer();
    m_connectTimer->stop();
    m_authTimer->stop();

    QString errorMsg = m_socket->errorString();
    qDebug() << "Socket error:" << errorMsg;

    if (m_state.load(std::memory_order_acquire) == Authenticating) {
        emit authenticationFailed();
    }

    emit errorOccurred(errorMsg);
    setState(Disconnected);
}

void TcpClient::onConnectTimeout() {
    if (m_state.load(std::memory_order_acquire) == Connecting) {
        qDebug() << "Connection timeout for" << m_host << ":" << m_port;
        emit errorOccurred(QString("Connection timeout - cannot reach %1:%2").arg(m_host).arg(m_port));
        disconnectFromHost();
    }
}

void TcpClient::onAuthTimeout() {
    if (m_state.load(std::memory_order_acquire) == Authenticating && !m_authResponseReceived) {
        qDebug() << "Authentication timeout";
        emit authenticationFailed();
        emit errorOccurred("Authentication timeout - no response from radio");
        disconnectFromHost();
    }
}

void TcpClient::onPingTimer() {
    if (m_state.load(std::memory_order_acquire) == Connected) {
        sendCAT(QString("PING%1;").arg(QDateTime::currentSecsSinceEpoch()));
        m_pingElapsed.start();
    }
}

void TcpClient::onCatResponse(const QString &response) {
    for (const auto &part : response.split(';', Qt::SkipEmptyParts)) {
        const auto command = part.trimmed();
        if (command == "TS0" || command == "TS1") {
            if (m_calibrationPhase == CalibrationPhase::ReadTest) {
                m_restoreTest = command == "TS0";
                m_calibrationPhase = CalibrationPhase::EnableTest;
                m_calibrationDeadline = m_digitalClock.elapsed() + 2500;
                sendCAT(QStringLiteral("TS1;TS;"));
            } else if (m_calibrationPhase == CalibrationPhase::EnableTest && command == "TS1") {
                m_calibrationPhase = CalibrationPhase::Running;
                beginDigitalAudioTransmit(m_calibrationMode, m_calibrationGeneration);
            } else if (m_calibrationPhase == CalibrationPhase::Running && command == "TS0") {
                handleDigitalTxAction(m_digitalGuard.trip("Calibration stopped: K4 TEST mode was disabled."));
            } else if (m_calibrationPhase == CalibrationPhase::RestoreTest && command == "TS0")
                completeDigitalCalibration();
        }
        const bool calibrationMeter = m_digitalGuard.active() && m_digitalGuard.calibrating()
            && command.startsWith("TM") && command.size() == 14;
        const auto action = m_digitalGuard.meter(command, m_digitalClock.elapsed());
        if (calibrationMeter)
            qInfo().noquote() << "Digital TX calibration" << m_digitalClock.elapsed()
                             << m_digitalGuard.meterDiagnostic() << "action" << int(action);
        handleDigitalTxAction(action);
        if (m_digitalGuard.active() && command.startsWith("TM") && command.size() == 14) {
            const float gain = m_digitalControl->gain.load();
            const QString status = m_digitalGuard.calibrating()
                ? QString("Calibrating in K4 TEST mode · settling / validating\n%1").arg(m_digitalGuard.meterDiagnostic())
                : m_digitalGuard.reduced()
                    ? QString("Audio drive reduced automatically · %1 dB · protection on").arg(20 * std::log10(gain), 0, 'f', 0)
                    : QString("TX protection active");
            if (action != DigitalTxGuard::Action::Reduced)
                emit digitalTxProtectionStatus(int(m_digitalGuard.mode()), status, false, m_digitalGuard.generation());
        }
    }
    if (response.startsWith("PONG") && m_pingElapsed.isValid())
        emit latencyChanged(static_cast<int>(m_pingElapsed.elapsed()));
}

void TcpClient::serviceDigitalTxProtection() {
    const qint64 now = m_digitalClock.elapsed();
    if (m_calibrationPhase != CalibrationPhase::None && m_calibrationPhase != CalibrationPhase::Running
        && now >= m_calibrationDeadline) {
        if (m_calibrationPhase == CalibrationPhase::RestoreTest) {
            m_calibrationSuccess = false;
            m_calibrationText = "Calibration stopped: verify the K4 TEST setting; restoration was not confirmed.";
            completeDigitalCalibration();
        } else
            finishDigitalCalibration(false, "Calibration stopped: K4 TEST mode could not be confirmed.");
        return;
    }
    handleDigitalTxAction(m_digitalGuard.tick(now));
    if (m_digitalGuard.active() && now >= m_nextMeterQuery) {
        m_nextMeterQuery = now + 250;
        sendCAT(QStringLiteral("TM;"));
    }
}
void TcpClient::handleDigitalTxAction(DigitalTxGuard::Action action) {
    const int mode = int(m_digitalGuard.mode());
    const quint64 generation = m_digitalGuard.generation();
    if (action == DigitalTxGuard::Action::Reduced) {
        emit digitalAudioDriveReduced(mode, m_digitalControl->gain.load(std::memory_order_acquire), generation);
        emit digitalTxProtectionStatus(mode, "Audio drive reduced automatically · TX protection active", false, generation);
    } else if (action == DigitalTxGuard::Action::Calibrated) {
        finishDigitalCalibration(true, "TX audio calibrated and saved · automatic protection remains active");
    } else if (action == DigitalTxGuard::Action::Tripped) {
        const QString reason = m_digitalGuard.reason();
        emit digitalTxProtectionStatus(mode, reason, true, generation);
        if (m_calibrationPhase == CalibrationPhase::Running) {
            finishDigitalCalibration(false, reason);
            return;
        }
        // Unkey on the socket's own thread before notifying the UI. Closing the
        // shared generation also stops the audio producer and rejects its queue.
        stopSstvAudioAndUnkey();
        emit digitalAudioTransmitFailed(mode, reason, generation);
        if (mode == int(DigitalTxGuard::Mode::Sstv))
            emit sstvAudioTransmitFailed(reason, generation);
    }
}

void TcpClient::beginDigitalCalibration(int mode, quint64 generation) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "beginDigitalCalibration", Qt::QueuedConnection,
                                  Q_ARG(int, mode), Q_ARG(quint64, generation));
        return;
    }
    if (!isConnected() || m_digitalGuard.active() || m_calibrationPhase != CalibrationPhase::None
        || generation == 0 || mode < 0 || mode > int(DigitalTxGuard::Mode::Sstv)) {
        emit digitalCalibrationFinished(mode, false, 0, "Calibration unavailable: connect and return the K4 to receive.", generation);
        return;
    }
    m_calibrationMode = mode;
    m_calibrationGeneration = generation;
    m_restoreTest = false;
    m_calibrationPhase = CalibrationPhase::ReadTest;
    m_calibrationDeadline = m_digitalClock.elapsed() + 2500;
    m_digitalTimer->start();
    sendCAT(QStringLiteral("TS;"));
    emit digitalTxProtectionStatus(mode, "Calibration: checking K4 TEST mode", false, generation);
}
void TcpClient::cancelDigitalCalibration() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "cancelDigitalCalibration", Qt::QueuedConnection);
        return;
    }
    if (m_calibrationPhase != CalibrationPhase::None && m_calibrationPhase != CalibrationPhase::RestoreTest)
        finishDigitalCalibration(false, "Calibration cancelled · previous saved level retained");
}
void TcpClient::finishDigitalCalibration(bool success, const QString &text) {
    m_calibrationSuccess = success;
    m_calibrationGain = m_digitalControl->gain.load();
    m_calibrationText = text;
    m_calibrationPhase = CalibrationPhase::None;
    stopSstvAudioAndUnkey(); // RX precedes restoration; no tone can reach normal TX.
    if (m_restoreTest && isConnected()) {
        m_calibrationPhase = CalibrationPhase::RestoreTest;
        m_calibrationDeadline = m_digitalClock.elapsed() + 2500;
        m_digitalTimer->start();
        sendCAT(QStringLiteral("TS0;TS;"));
    } else
        completeDigitalCalibration();
}
void TcpClient::completeDigitalCalibration() {
    m_calibrationPhase = CalibrationPhase::None;
    m_digitalTimer->stop();
    emit digitalCalibrationFinished(m_calibrationMode, m_calibrationSuccess, m_calibrationGain,
                                     m_calibrationText, m_calibrationGeneration);
}

void TcpClient::sendAuthentication() {
    // Build SHA-384 hash of password as hex string
    QByteArray authData = Protocol::buildAuthData(m_password);
    qDebug() << "Sending auth hash (" << authData.size() << "bytes)";

    // Send raw auth data (not wrapped in K4 packet - just the hex string)
    m_socket->write(authData);
    m_socket->flush();
    // Radio will respond with packets, which triggers auth success and init sequence
}

void TcpClient::startPingTimer() {
    m_pingTimer->start();
}

void TcpClient::stopPingTimer() {
    m_pingTimer->stop();
}
