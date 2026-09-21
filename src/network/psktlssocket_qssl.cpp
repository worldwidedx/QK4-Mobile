// PskTlsSocket backend that adapts QSslSocket. This is the original QK4
// TLS/PSK path and needs Qt's OpenSSL TLS plugin plus an OpenSSL runtime.
#include "psktlssocket.h"

#include <QDebug>
#include <QSslCipher>
#include <QSslConfiguration>
#include <QSslPreSharedKeyAuthenticator>
#include <QSslSocket>

PskTlsSocket::PskTlsSocket(QObject *parent) : QIODevice(parent), m_qssl(new QSslSocket(this)) {
    connect(m_qssl, &QSslSocket::connected, this, [this]() {
        if (!m_useTls)
            open(QIODevice::ReadWrite | QIODevice::Unbuffered);
        emit connected();
    });
    connect(m_qssl, &QSslSocket::encrypted, this, [this]() {
        open(QIODevice::ReadWrite | QIODevice::Unbuffered);
        emit encrypted();
    });
    connect(m_qssl, &QSslSocket::disconnected, this, [this]() {
        close();
        emit disconnected();
    });
    connect(m_qssl, &QSslSocket::readyRead, this, &PskTlsSocket::readyRead);
    connect(m_qssl, &QSslSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
        setErrorString(m_qssl->errorString());
        emit errorOccurred(error);
    });
    connect(m_qssl, &QSslSocket::sslErrors, this, [this](const QList<QSslError> &errors) {
        // PSK doesn't use certificates, so certificate errors are expected.
        for (const QSslError &error : errors)
            qDebug() << "SSL error (ignored for PSK):" << error.errorString();
        m_qssl->ignoreSslErrors();
    });
    connect(m_qssl, &QSslSocket::preSharedKeyAuthenticationRequired, this,
            [this](QSslPreSharedKeyAuthenticator *authenticator) {
                qDebug() << "PSK authentication requested, identity hint:" << authenticator->identityHint();
                authenticator->setIdentity(m_identity);
                authenticator->setPreSharedKey(m_psk);
            });
}

PskTlsSocket::~PskTlsSocket() = default;

bool PskTlsSocket::tlsAvailable() {
    return QSslSocket::supportsSsl();
}

QString PskTlsSocket::tlsLibraryVersion() {
    return QStringLiteral("%1 (built against %2)")
        .arg(QSslSocket::sslLibraryVersionString(), QSslSocket::sslLibraryBuildVersionString());
}

void PskTlsSocket::setPreSharedKey(const QByteArray &identity, const QByteArray &psk) {
    m_identity = identity;
    m_psk = psk;
}

void PskTlsSocket::connectToHost(const QString &host, quint16 port) {
    m_useTls = false;
    m_qssl->connectToHost(host, port);
}

void PskTlsSocket::connectToHostEncrypted(const QString &host, quint16 port) {
    m_useTls = true;

    // Configure TLS for PSK authentication - require TLS 1.2 minimum
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone); // PSK doesn't use certificates

    // Filter to only TLS 1.2+ PSK ciphers
    QList<QSslCipher> tls12PskCiphers;
    for (const QSslCipher &cipher : QSslConfiguration::supportedCiphers()) {
        if (cipher.name().contains("PSK")
            && (cipher.protocol() == QSsl::TlsV1_2 || cipher.protocol() == QSsl::TlsV1_3)) {
            tls12PskCiphers.append(cipher);
        }
    }
    qDebug() << "=== Offering" << tls12PskCiphers.size() << "TLS 1.2+ PSK ciphers ===";
    for (const QSslCipher &cipher : tls12PskCiphers)
        qDebug() << "  " << cipher.name() << "(" << cipher.protocolString() << ")";
    if (!tls12PskCiphers.isEmpty())
        sslConfig.setCiphers(tls12PskCiphers);

    m_qssl->setSslConfiguration(sslConfig);
    m_qssl->connectToHostEncrypted(host, port);
}

void PskTlsSocket::disconnectFromHost() {
    m_qssl->disconnectFromHost();
}

void PskTlsSocket::abort() {
    m_qssl->abort();
}

bool PskTlsSocket::flush() {
    return m_qssl->flush();
}

void PskTlsSocket::setSocketOption(QAbstractSocket::SocketOption option, const QVariant &value) {
    m_qssl->setSocketOption(option, value);
}

QAbstractSocket::SocketState PskTlsSocket::state() const {
    return m_qssl->state();
}

bool PskTlsSocket::isEncrypted() const {
    return m_qssl->isEncrypted();
}

QString PskTlsSocket::sessionCipher() const {
    const QSslCipher cipher = m_qssl->sessionCipher();
    if (cipher.isNull())
        return QString();
    return QStringLiteral("%1 (%2, kx %3, enc %4)")
        .arg(cipher.name(), cipher.protocolString(), cipher.keyExchangeMethod(), cipher.encryptionMethod());
}

qint64 PskTlsSocket::bytesAvailable() const {
    return m_qssl->bytesAvailable() + QIODevice::bytesAvailable();
}

qint64 PskTlsSocket::bytesToWrite() const {
    return m_qssl->bytesToWrite();
}

qint64 PskTlsSocket::readData(char *data, qint64 maxSize) {
    return m_qssl->read(data, maxSize);
}

qint64 PskTlsSocket::writeData(const char *data, qint64 size) {
    return m_qssl->write(data, size);
}
