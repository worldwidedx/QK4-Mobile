// PskTlsSocket backend that drives OpenSSL directly over a QTcpSocket.
//
// OpenSSL never touches the network here: it reads ciphertext from a memory
// BIO that we fill from the TCP socket, and writes ciphertext into a second
// memory BIO that we drain into the TCP socket. All work happens on the
// socket's thread, so no locking is needed.
#include "psktlssocket.h"

#include <QDebug>
#include <QTcpSocket>
#include <climits>
#include <cstring>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

namespace {
QString drainOpenSslErrors() {
    QString text;
    unsigned long code;
    const char *file = nullptr;
    const char *func = nullptr;
    const char *data = nullptr;
    int line = 0;
    int flags = 0;
    while ((code = ERR_get_error_all(&file, &line, &func, &data, &flags)) != 0) {
        char buf[256];
        ERR_error_string_n(code, buf, sizeof buf);
        if (!text.isEmpty())
            text += QLatin1String("; ");
        text += QString::fromLatin1(buf);
        if (file)
            text += QStringLiteral(" [%1:%2 %3]").arg(QString::fromLatin1(file)).arg(line).arg(QString::fromLatin1(func ? func : ""));
        if (data && (flags & ERR_TXT_STRING) && *data)
            text += QStringLiteral(" (%1)").arg(QString::fromLatin1(data));
    }
    return text;
}
} // namespace

PskTlsSocket::PskTlsSocket(QObject *parent) : QIODevice(parent), m_tcp(new QTcpSocket(this)) {
    connect(m_tcp, &QTcpSocket::connected, this, &PskTlsSocket::onTcpConnected);
    connect(m_tcp, &QTcpSocket::readyRead, this, &PskTlsSocket::onTcpReadyRead);
    connect(m_tcp, &QTcpSocket::disconnected, this, &PskTlsSocket::onTcpDisconnected);
    connect(m_tcp, &QTcpSocket::errorOccurred, this, &PskTlsSocket::onTcpError);
}

PskTlsSocket::~PskTlsSocket() {
    teardownTls();
}

bool PskTlsSocket::tlsAvailable() {
    return true;
}

QString PskTlsSocket::tlsLibraryVersion() {
    return QString::fromLatin1(OpenSSL_version(OPENSSL_VERSION));
}

void PskTlsSocket::setPreSharedKey(const QByteArray &identity, const QByteArray &psk) {
    m_identity = identity;
    m_psk = psk;
}

void PskTlsSocket::connectToHost(const QString &host, quint16 port) {
    teardownTls();
    m_useTls = false;
    m_plaintext.clear();
    m_tcp->connectToHost(host, port);
}

void PskTlsSocket::connectToHostEncrypted(const QString &host, quint16 port) {
    teardownTls();
    m_useTls = true;
    m_plaintext.clear();

    m_ctx = SSL_CTX_new(TLS_client_method());
    if (!m_ctx) {
        failTls(QStringLiteral("SSL_CTX_new"));
        return;
    }
    // The K4 speaks TLS 1.2 with PSK cipher suites. Pin exactly that: with
    // TLS 1.3 enabled, OpenSSL 3 fails inside tls_construct_ctos_early_data
    // ("internal error") when the key comes from the legacy
    // psk_client_callback, so the ClientHello never leaves the device.
    SSL_CTX_set_min_proto_version(m_ctx, TLS1_2_VERSION);
    SSL_CTX_set_max_proto_version(m_ctx, TLS1_2_VERSION);
    SSL_CTX_set_verify(m_ctx, SSL_VERIFY_NONE, nullptr); // PSK: no certificates
    if (SSL_CTX_set_cipher_list(m_ctx, "PSK") != 1) {
        failTls(QStringLiteral("no PSK cipher suites available"));
        return;
    }
    SSL_CTX_set_mode(m_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER | SSL_MODE_AUTO_RETRY);
    SSL_CTX_set_psk_client_callback(m_ctx, &PskTlsSocket::pskClientCallback);

    m_ssl = SSL_new(m_ctx);
    m_readBio = BIO_new(BIO_s_mem());
    m_writeBio = BIO_new(BIO_s_mem());
    if (!m_ssl || !m_readBio || !m_writeBio) {
        failTls(QStringLiteral("SSL_new"));
        return;
    }
    // An empty memory BIO must report "retry", not end-of-stream, so the
    // handshake and reads simply wait for more bytes from the socket.
    BIO_set_mem_eof_return(m_readBio, -1);
    BIO_set_mem_eof_return(m_writeBio, -1);
    SSL_set_bio(m_ssl, m_readBio, m_writeBio); // SSL now owns both BIOs
    SSL_set_app_data(m_ssl, this);
    SSL_set_connect_state(m_ssl);
    m_handshakeDone = false;

    m_tcp->connectToHost(host, port);
}

void PskTlsSocket::disconnectFromHost() {
    if (m_useTls && m_ssl && m_handshakeDone) {
        SSL_shutdown(m_ssl); // best-effort close_notify
        flushOutgoing();
    }
    m_tcp->disconnectFromHost();
}

void PskTlsSocket::abort() {
    m_tcp->abort();
    teardownTls();
}

bool PskTlsSocket::flush() {
    return m_tcp->flush();
}

void PskTlsSocket::setSocketOption(QAbstractSocket::SocketOption option, const QVariant &value) {
    m_tcp->setSocketOption(option, value);
}

QAbstractSocket::SocketState PskTlsSocket::state() const {
    return m_tcp->state();
}

bool PskTlsSocket::isEncrypted() const {
    return m_useTls && m_handshakeDone;
}

QString PskTlsSocket::sessionCipher() const {
    if (!isEncrypted() || !m_ssl)
        return QString();
    return QStringLiteral("%1 (%2)").arg(QString::fromLatin1(SSL_get_cipher_name(m_ssl)),
                                          QString::fromLatin1(SSL_get_version(m_ssl)));
}

qint64 PskTlsSocket::bytesAvailable() const {
    const qint64 pending = m_useTls ? m_plaintext.size() : m_tcp->bytesAvailable();
    return pending + QIODevice::bytesAvailable();
}

qint64 PskTlsSocket::bytesToWrite() const {
    return m_tcp->bytesToWrite();
}

qint64 PskTlsSocket::readData(char *data, qint64 maxSize) {
    if (!m_useTls)
        return m_tcp->read(data, maxSize);
    const qint64 n = qMin<qint64>(maxSize, m_plaintext.size());
    if (n > 0) {
        std::memcpy(data, m_plaintext.constData(), size_t(n));
        m_plaintext.remove(0, int(n));
    }
    return n;
}

qint64 PskTlsSocket::writeData(const char *data, qint64 size) {
    if (!m_useTls)
        return m_tcp->write(data, size);
    if (!m_ssl || !m_handshakeDone)
        return -1;
    qint64 total = 0;
    while (total < size) {
        const int chunk = int(qMin<qint64>(size - total, INT_MAX));
        const int n = SSL_write(m_ssl, data + total, chunk);
        if (n <= 0) {
            failTls(QStringLiteral("SSL_write"));
            return -1;
        }
        total += n;
    }
    flushOutgoing();
    return total;
}

void PskTlsSocket::onTcpConnected() {
    // Plain connections are writable from inside the connected() slot
    // (TcpClient sends the auth hash there), so open before emitting.
    if (!m_useTls)
        open(QIODevice::ReadWrite | QIODevice::Unbuffered);
    emit connected();
    if (m_useTls)
        pumpTls(); // sends ClientHello
}

void PskTlsSocket::onTcpReadyRead() {
    if (!m_useTls) {
        emit readyRead();
        return;
    }
    if (!m_ssl)
        return;
    const QByteArray ciphertext = m_tcp->readAll();
    int offset = 0;
    while (offset < ciphertext.size()) {
        const int n = BIO_write(m_readBio, ciphertext.constData() + offset, ciphertext.size() - offset);
        if (n <= 0) {
            failTls(QStringLiteral("BIO_write"));
            return;
        }
        offset += n;
    }
    pumpTls();
}

void PskTlsSocket::onTcpDisconnected() {
    close();
    teardownTls();
    m_plaintext.clear();
    emit disconnected();
}

void PskTlsSocket::onTcpError(QAbstractSocket::SocketError error) {
    setErrorString(m_tcp->errorString());
    emit errorOccurred(error);
}

// Advance the handshake and/or decrypt whatever the read BIO holds.
// Returns false when the TLS session failed and has been torn down.
bool PskTlsSocket::pumpTls() {
    if (!m_ssl)
        return false;

    if (!m_handshakeDone) {
        const int r = SSL_do_handshake(m_ssl);
        const size_t pendingOut = BIO_ctrl_pending(m_writeBio);
        flushOutgoing();
        if (r != 1) {
            const int err = SSL_get_error(m_ssl, r);
            qDebug() << "TLS handshake step: rc" << r << "ssl_error" << err << "wrote" << pendingOut
                     << "bytes, state" << SSL_state_string_long(m_ssl);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
                return true; // need more bytes from the radio
            failTls(QStringLiteral("handshake"));
            return false;
        }
        m_handshakeDone = true;
        open(QIODevice::ReadWrite | QIODevice::Unbuffered);
        emit encrypted();
        if (!m_ssl)
            return false; // slot disconnected us
    }

    bool gotData = false;
    for (;;) {
        char buf[16384];
        const int n = SSL_read(m_ssl, buf, sizeof buf);
        if (n > 0) {
            m_plaintext.append(buf, n);
            gotData = true;
            continue;
        }
        const int err = SSL_get_error(m_ssl, n);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE)
            break;
        if (err == SSL_ERROR_ZERO_RETURN) {
            // Peer sent close_notify: let the TCP teardown deliver disconnected().
            m_tcp->disconnectFromHost();
            break;
        }
        failTls(QStringLiteral("SSL_read"));
        return false;
    }
    flushOutgoing();
    if (gotData)
        emit readyRead();
    return true;
}

void PskTlsSocket::flushOutgoing() {
    if (!m_writeBio)
        return;
    while (BIO_ctrl_pending(m_writeBio) > 0) {
        char buf[16384];
        const int n = BIO_read(m_writeBio, buf, sizeof buf);
        if (n <= 0)
            break;
        m_tcp->write(buf, n);
    }
}

void PskTlsSocket::failTls(const QString &what) {
    QString detail = drainOpenSslErrors();
    if (detail.isEmpty())
        detail = QStringLiteral("no further detail");
    const QString message = QStringLiteral("TLS/PSK failure (%1): %2").arg(what, detail);
    qWarning() << message;
    setErrorString(message);
    teardownTls();
    emit errorOccurred(QAbstractSocket::SslHandshakeFailedError);
    m_tcp->abort();
}

void PskTlsSocket::teardownTls() {
    if (m_ssl) {
        SSL_free(m_ssl); // frees the BIOs handed over by SSL_set_bio
        m_ssl = nullptr;
        m_readBio = nullptr;
        m_writeBio = nullptr;
    } else {
        BIO_free(m_readBio);
        BIO_free(m_writeBio);
        m_readBio = nullptr;
        m_writeBio = nullptr;
    }
    if (m_ctx) {
        SSL_CTX_free(m_ctx);
        m_ctx = nullptr;
    }
    m_handshakeDone = false;
}

unsigned int PskTlsSocket::pskClientCallback(ssl_st *ssl, const char *hint, char *identity, unsigned int maxIdentityLen,
                                             unsigned char *psk, unsigned int maxPskLen) {
    Q_UNUSED(hint)
    auto *self = static_cast<PskTlsSocket *>(SSL_get_app_data(ssl));
    if (!self || self->m_psk.isEmpty())
        return 0;
    const unsigned int identityLen = unsigned(self->m_identity.size());
    const unsigned int pskLen = unsigned(self->m_psk.size());
    if (identityLen >= maxIdentityLen || pskLen > maxPskLen)
        return 0;
    std::memcpy(identity, self->m_identity.constData(), identityLen);
    identity[identityLen] = '\0';
    std::memcpy(psk, self->m_psk.constData(), pskLen);
    return pskLen;
}
