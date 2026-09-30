#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QObject>
#include <QAbstractSocket>
#include <QThread>
#include <QTimer>
#include <QElapsedTimer>
#include <atomic>
#include "protocol.h"
#include "audio/digitaltxguard.h"
#include "psktlssocket.h"

class TcpClient : public QObject {
    Q_OBJECT

public:
    enum ConnectionState { Disconnected, Connecting, Authenticating, Connected };
    Q_ENUM(ConnectionState)

    explicit TcpClient(QObject *parent = nullptr);
    ~TcpClient();

    Q_INVOKABLE void connectToHost(const QString &host, quint16 port, const QString &password, bool useTls = false,
                                   const QString &identity = QString(), int encodeMode = 3, int streamingLatency = 3);
    Q_INVOKABLE void disconnectFromHost();
    bool isConnected() const;
    ConnectionState connectionState() const;
    bool isUsingTls() const { return m_useTls; }

    Q_INVOKABLE void sendCAT(const QString &command);
    // Send one operator macro, then refresh the full radio/display state.
    Q_INVOKABLE void sendMacro(const QString &command);
    Q_INVOKABLE void sendRaw(const QByteArray &data);
    // Program SSTV audio has a dedicated gate so a queued callback cannot
    // revive TX after STOP.  Microphone packets continue to use sendRaw().
    Q_INVOKABLE void beginSstvAudioTransmit(quint64 generation);
    Q_INVOKABLE void sendSstvAudio(const QByteArray &data, int emittedSamples, int totalSamples,
                                   int imageSamples, quint64 generation);
    Q_INVOKABLE void stopSstvAudioAndUnkey();
    // FT8/FT4 and SSTV must use this guarded lease for generated audio.
    // mode is DigitalTxGuard::Mode. A fault requires an operator acknowledgement.
    Q_INVOKABLE void beginDigitalAudioTransmit(int mode, quint64 generation);
    void beginScheduledDigitalAudio(int mode, quint64 generation);
    void stopScheduledDigitalAudio(quint64 generation);
    void confirmScheduledAudioDrained(int emitted, int total, int image, quint64 generation, qint64 deadline);
    Q_INVOKABLE void acknowledgeDigitalTxFault();
    Q_INVOKABLE void beginDigitalCalibration(int mode, quint64 generation);
    Q_INVOKABLE void cancelDigitalCalibration();
    Q_INVOKABLE void sendDigitalAudio(const QByteArray &data, int emittedSamples, int totalSamples,
                                      int imageSamples, quint64 generation);
    Q_INVOKABLE void stopDigitalAudioAndUnkey();
    std::shared_ptr<DigitalTxControl> digitalTxControl() const { return m_digitalControl; }

    Protocol *protocol() { return m_protocol; }

signals:
    void stateChanged(ConnectionState state);
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);
    void authenticated();
    void authenticationFailed();
    void latencyChanged(int milliseconds);
    // This acknowledges that TX; was written on the I/O thread. MainWindow
    // applies a short fixed key-up guard before releasing program audio.
    void sstvAudioKeyRequested(quint64 generation);
    void sstvAudioTransmitFailed(const QString &reason, quint64 generation);
    void sstvAudioAccepted(int emittedSamples, int totalSamples, int imageSamples,
                           quint64 generation);
    void digitalAudioKeyRequested(int mode, quint64 generation);
    void scheduledDigitalAudioStopped(quint64 generation);
    void digitalAudioTransmitFailed(int mode, const QString &reason, quint64 generation);
    void digitalAudioDriveReduced(int mode, float gain, quint64 generation);
    void digitalTxProtectionStatus(int mode, const QString &text, bool fault, quint64 generation);
    void digitalCalibrationFinished(int mode, bool success, float gain, const QString &text, quint64 generation);

private slots:
    void onSocketConnected();
    void onSocketEncrypted();
    void onSocketDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void onConnectTimeout();
    void onAuthTimeout();
    void onPingTimer();
    void onCatResponse(const QString &response);
    void serviceDigitalTxProtection();

private:
    void setState(ConnectionState state);
    void sendAuthentication();
    void startPingTimer();
    void stopPingTimer();
    void attemptConnection();
    void handleDigitalTxAction(DigitalTxGuard::Action action);
    void finishDigitalCalibration(bool success, const QString &text);
    void completeDigitalCalibration();

    PskTlsSocket *m_socket;
    Protocol *m_protocol;
    QTimer *m_connectTimer;
    QTimer *m_authTimer;
    QTimer *m_pingTimer;
    QElapsedTimer m_pingElapsed;

    QString m_host;
    quint16 m_port;
    QString m_password; // Also used as PSK when TLS enabled
    bool m_useTls;
    QString m_identity;     // TLS-PSK identity (optional)
    int m_encodeMode;       // Audio encode mode (0-3)
    int m_streamingLatency; // Remote streaming audio latency (0-7)
    // Written on the I/O thread and read from the UI/audio threads.
    std::atomic<ConnectionState> m_state{Disconnected};
    std::atomic<bool> m_connected{false}; // Thread-safe read for isConnected()
    bool m_authResponseReceived;
    bool m_sstvAudioGate = false; // I/O-thread only
    quint64 m_sstvAudioGeneration = 0; // I/O-thread only
    std::shared_ptr<DigitalTxControl> m_digitalControl = std::make_shared<DigitalTxControl>();
    DigitalTxGuard m_digitalGuard{m_digitalControl};
    QTimer *m_digitalTimer = nullptr;
    QElapsedTimer m_digitalClock;
    qint64 m_nextMeterQuery = 0;
    enum class CalibrationPhase { None, ReadTest, EnableTest, Running, RestoreTest };
    CalibrationPhase m_calibrationPhase = CalibrationPhase::None;
    int m_calibrationMode = 0;
    quint64 m_calibrationGeneration = 0;
    bool m_restoreTest = false, m_calibrationSuccess = false;
    float m_calibrationGain = 0.03125f;
    QString m_calibrationText;
    qint64 m_calibrationDeadline = 0;
};

#endif // TCPCLIENT_H
