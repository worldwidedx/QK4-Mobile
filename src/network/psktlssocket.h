#ifndef PSKTLSSOCKET_H
#define PSKTLSSOCKET_H

#include <QAbstractSocket>
#include <QByteArray>
#include <QIODevice>
#include <QString>
#include <QVariant>

class QTcpSocket;
class QSslSocket;
#ifdef QK4_PSK_TLS_OPENSSL
struct ssl_ctx_st;
struct ssl_st;
struct bio_st;
#endif

// TLS-PSK client socket for the K4 remote link.
//
// The K4 authenticates remote clients with TLS 1.2 pre-shared keys. Qt's
// native TLS backends (Secure Transport on iOS/macOS, Schannel on Windows)
// have no PSK support, so QSslSocket only works where Qt's OpenSSL backend
// plus an OpenSSL runtime are available. This class hides that difference:
//
//   - QK4_PSK_TLS_OPENSSL: drives OpenSSL directly over a QTcpSocket using
//     memory BIOs. Used on iOS, where OpenSSL is linked statically and Qt
//     does not ship its OpenSSL TLS plugin.
//   - otherwise: thin adapter over QSslSocket (Android, Windows, macOS, Linux).
//
// Plain (unencrypted) connections pass straight through to the TCP socket.
class PskTlsSocket : public QIODevice {
    Q_OBJECT

public:
    explicit PskTlsSocket(QObject *parent = nullptr);
    ~PskTlsSocket() override;

    static bool tlsAvailable();
    static QString tlsLibraryVersion();

    void setPreSharedKey(const QByteArray &identity, const QByteArray &psk);

    void connectToHost(const QString &host, quint16 port);
    void connectToHostEncrypted(const QString &host, quint16 port);
    void disconnectFromHost();
    void abort();
    bool flush();
    void setSocketOption(QAbstractSocket::SocketOption option, const QVariant &value);

    QAbstractSocket::SocketState state() const;
    bool isEncrypted() const;
    QString sessionCipher() const;

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override;
    qint64 bytesToWrite() const override;

signals:
    void connected();
    void encrypted();
    void disconnected();
    void errorOccurred(QAbstractSocket::SocketError error);

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 size) override;

private:
    QByteArray m_identity;
    QByteArray m_psk;
    bool m_useTls = false;

#ifdef QK4_PSK_TLS_OPENSSL
    void onTcpConnected();
    void onTcpReadyRead();
    void onTcpDisconnected();
    void onTcpError(QAbstractSocket::SocketError error);
    bool pumpTls();
    void flushOutgoing();
    void failTls(const QString &what);
    void teardownTls();
    static unsigned int pskClientCallback(ssl_st *ssl, const char *hint, char *identity, unsigned int maxIdentityLen,
                                          unsigned char *psk, unsigned int maxPskLen);

    QTcpSocket *m_tcp = nullptr;
    ssl_ctx_st *m_ctx = nullptr;
    ssl_st *m_ssl = nullptr;
    bio_st *m_readBio = nullptr;  // ciphertext from the radio, fed to OpenSSL
    bio_st *m_writeBio = nullptr; // ciphertext from OpenSSL, sent to the radio
    bool m_handshakeDone = false;
    QByteArray m_plaintext; // decrypted bytes not yet read by the caller
#else
    QSslSocket *m_qssl = nullptr;
#endif
};

#endif // PSKTLSSOCKET_H
