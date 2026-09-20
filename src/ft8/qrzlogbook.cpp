#include "qrzlogbook.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>
#include <QUuid>
// Keep this service's meta-object separate from the optional FT widget objects
// so the logbook/transport tests do not link the whole radio display.
#include "moc_qrzlogbook.cpp"
#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

namespace {
QPointer<QrzLogbook> service;
bool fail(QString *error, const QString &text) { if (error) *error = text; return false; }
const char *secureError = "The QRZ key could not be accessed securely. Enter the API key again in Setup.";
QString readKey(QString *error) {
#ifdef Q_OS_ANDROID
    const auto context = QNativeInterface::QAndroidApplication::context();
    const auto result = QJniObject::callStaticObjectMethod(
        "com/w9wdx/qk4phone/QrzCredentials", "read", "(Landroid/content/Context;)Ljava/lang/String;",
        context.object()).toString();
    if (result.startsWith("OK:")) return result.mid(3);
#endif
    fail(error, secureError);
    return {};
}
bool writeKey(const QString &key, QString *error) {
#ifdef Q_OS_ANDROID
    const auto context = QNativeInterface::QAndroidApplication::context();
    const auto value = QJniObject::fromString(key);
    if (QJniObject::callStaticMethod<jboolean>("com/w9wdx/qk4phone/QrzCredentials", "write",
            "(Landroid/content/Context;Ljava/lang/String;)Z", context.object(), value.object())) return true;
#else
    Q_UNUSED(key)
#endif
    return fail(error, "Cannot save the API key securely. Android Keystore is required.");
}
bool clearKey(QString *error) {
#ifdef Q_OS_ANDROID
    const auto context = QNativeInterface::QAndroidApplication::context();
    if (QJniObject::callStaticMethod<jboolean>("com/w9wdx/qk4phone/QrzCredentials", "clear",
            "(Landroid/content/Context;)Z", context.object())) return true;
#endif
    return fail(error, "Cannot remove the encrypted QRZ key.");
}
int findId(const Ft8Logbook &log, const QString &id) {
    for (int i = 0; i < log.records().size(); ++i)
        if (log.records()[i].value("_QK4_ID") == id) return i;
    return -1;
}
}
QrzKeyStore QrzKeyStore::android() { return {readKey, writeKey, clearKey}; }
QrzLogbook::QrzLogbook(const QString &path, QObject *parent, QNetworkAccessManager *network, QrzKeyStore keys)
    : QObject(parent), m_path(path), m_keys(std::move(keys)),
      m_network(network ? network : new QNetworkAccessManager(this)) {
    QSettings settings;
    m_call = settings.value("qrz/callsign").toString();
    m_auto = settings.value("qrz/automatic", false).toBool();
    QString error;
    m_hasKey = !m_keys.read(&error).isEmpty();
    if (!m_call.isEmpty() && !error.isEmpty()) m_status = error;
    QTimer::singleShot(0, this, &QrzLogbook::resume);
}
QrzLogbook *QrzLogbook::instance() { return service; }
void QrzLogbook::start(const QString &path, QObject *parent) {
    if (service) return;
    service = new QrzLogbook(path, parent);
    Ft8Logbook::contactAdded = [](const QString &path, int index) {
        if (service) service->contactAdded(path, index);
    };
}
bool QrzLogbook::sent(const AdifRecord &r) {
    const auto status = r.value("QRZCOM_QSO_UPLOAD_STATUS").toUpper();
    return status == "Y" || status == "M";
}
bool QrzLogbook::managedField(const QString &field) {
    return field.startsWith("_QK4_") || field.startsWith("QRZCOM_QSO_UPLOAD_") || field == "APP_QRZLOG_LOGID";
}
bool QrzLogbook::configure(const QString &call, const QString &replacementKey, bool automatic, QString *error) {
    if (m_busy || !m_queue.isEmpty()) return fail(error, "Wait for QRZ sending to finish before changing Setup.");
    const auto normalized = call.trimmed().toUpper();
    static const QRegularExpression valid("^[A-Z0-9]+(?:/[A-Z0-9]+)*$");
    if (normalized.size() > 64 || !valid.match(normalized).hasMatch()) return fail(error, "Enter your QRZ logbook callsign.");
    if (normalized != m_call && replacementKey.trimmed().isEmpty())
        return fail(error, "Enter the API key for this callsign's QRZ logbook.");
    if (!replacementKey.trimmed().isEmpty()) {
        if (!m_keys.write(replacementKey.trimmed(), error)) return false;
    } else {
        QString readError;
        if (m_keys.read(&readError).isEmpty()) return fail(error, readError.isEmpty() ? "Enter the QRZ logbook API key." : readError);
    }
    m_call = normalized;
    m_auto = automatic;
    m_hasKey = true;
    QSettings settings;
    settings.setValue("qrz/callsign", m_call);
    settings.setValue("qrz/automatic", m_auto);
    m_status = "QRZ setup saved. New contacts " + QString(m_auto ? "will send automatically." : "can be sent manually.");
    emit changed();
    return true;
}
bool QrzLogbook::forget(QString *error) {
    if (m_busy || !m_queue.isEmpty()) return fail(error, "Wait for QRZ sending to finish before removing the key.");
    if (!m_keys.clear(error)) return false;
    m_call.clear(); m_auto = false; m_hasKey = false;
    QSettings settings;
    settings.remove("qrz");
    m_status = "QRZ key removed. Saved contacts and upload history are retained.";
    emit changed();
    return true;
}
void QrzLogbook::contactAdded(const QString &path, int index) {
    if (!m_auto || path != m_path) return;
    QString error;
    if (!send(index, &error)) { m_status = error; emit changed(); }
}
bool QrzLogbook::send(int index, QString *error) {
    if (!configured()) return fail(error, "Open Setup and enter your QRZ logbook callsign and API key.");
    Ft8Logbook log(m_path);
    if (!log.load(error)) return false;
    if (index < 0 || index >= log.records().size()) return fail(error, "Select a contact first.");
    const auto r = log.records()[index];
    if (r.value("APP_QK4_PRACTICE") == "Y") return fail(error, "Practice contacts cannot be sent to QRZ.");
    if (sent(r)) return fail(error, "This contact has already been sent to QRZ.");
    if (!r.value("STATION_CALLSIGN").isEmpty() && r.value("STATION_CALLSIGN") != m_call)
        return fail(error, "This contact's My callsign differs from the QRZ logbook callsign in Setup.");
    const auto state = r.value("_QK4_QRZ_STATE");
    if (state == "QUEUED" || state == "SENDING") return fail(error, "This contact is already queued for QRZ.");
    auto id = r.value("_QK4_ID");
    if (id.isEmpty()) id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!log.patchUpload(index, {{"_QK4_ID", id}, {"_QK4_QRZ_STATE", "QUEUED"},
            {"_QK4_QRZ_ERROR", {}}, {"STATION_CALLSIGN", m_call}}, error)) return false;
    m_queue.enqueue(id);
    m_status = r.value("CALL") + ": queued for QRZ.";
    emit changed();
    QTimer::singleShot(0, this, &QrzLogbook::process);
    return true;
}
void QrzLogbook::resume() {
    Ft8Logbook log(m_path);
    QString error;
    if (!log.load(&error)) { m_status = error; emit changed(); return; }
    for (int i = 0; i < log.records().size(); ++i) {
        const auto r = log.records()[i];
        if (sent(r)) continue;
        if (r.value("_QK4_QRZ_STATE") == "SENDING") {
            // A process interrupted after POST may already have inserted the QSO.
            log.patchUpload(i, {{"_QK4_QRZ_STATE", "ERROR"}, {"_QK4_QRZ_ERROR",
                "Sending was interrupted. Check QRZ before retrying; acceptance is unknown."}}, &error);
        } else if (r.value("_QK4_QRZ_STATE") == "QUEUED" && !r.value("_QK4_ID").isEmpty()) {
            if (!m_queue.contains(r.value("_QK4_ID"))) m_queue.enqueue(r.value("_QK4_ID"));
        }
    }
    emit changed();
    process();
}
QMap<QString, QString> QrzLogbook::parseResponse(const QByteArray &bytes) {
    QMap<QString, QString> fields;
    for (const auto &part : bytes.trimmed().split('&')) {
        const int split = part.indexOf('=');
        if (split <= 0) continue;
        auto value = part.mid(split + 1);
        value.replace('+', ' ');
        const auto key = QString::fromLatin1(part.left(split)).trimmed().toUpper();
        if (fields.contains(key)) return {}; // Ambiguous response must not mark success.
        fields[key] = QUrl::fromPercentEncoding(value);
    }
    return fields;
}
void QrzLogbook::process() {
    if (m_busy || m_queue.isEmpty()) return;
    m_busy = true;
    const auto id = m_queue.dequeue();
    Ft8Logbook log(m_path);
    QString error;
    if (!log.load(&error)) { finish(id, false, {}, error); return; }
    const int index = findId(log, id);
    if (index < 0) { finish(id, false, {}, "Queued contact no longer exists."); return; }
    auto r = log.records()[index];
    if (sent(r)) { m_busy = false; QTimer::singleShot(0, this, &QrzLogbook::process); return; }
    if (r.value("APP_QK4_PRACTICE") == "Y" || r.value("STATION_CALLSIGN") != m_call) {
        finish(id, false, {}, "Contact does not match the configured QRZ callsign."); return;
    }
    const auto key = m_keys.read(&error);
    if (key.isEmpty()) { finish(id, false, {}, error.isEmpty() ? "QRZ API key is missing. Open Setup." : error); return; }
    for (auto it = r.begin(); it != r.end();)
        if (managedField(it.key())) it = r.erase(it); else ++it;
    auto adif = Ft8Logbook::encode({r}, &error);
    if (adif.isEmpty()) { finish(id, false, {}, error); return; }
    adif = adif.mid(adif.indexOf("<EOH>") + 5).trimmed(); // INSERT accepts one QSO, without a file header.
    if (!log.patchUpload(index, {{"_QK4_QRZ_STATE", "SENDING"}}, &error)) {
        finish(id, false, {}, error); return;
    }
    QNetworkRequest request(QUrl("https://logbook.qrz.com/api"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setRawHeader("User-Agent", "QK4-Mobile/" + QCoreApplication::applicationVersion().toLatin1());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(30000);
    const auto body = "KEY=" + QUrl::toPercentEncoding(key) + "&ACTION=INSERT&ADIF=" + QUrl::toPercentEncoding(adif);
    auto *reply = m_network->post(request, body);
    reply->setReadBufferSize(1024 * 1024 + 1);
    auto *deadline = new QTimer(reply);
    deadline->setSingleShot(true);
    connect(deadline, &QTimer::timeout, reply, &QNetworkReply::abort);
    deadline->start(35000);
    connect(reply, &QNetworkReply::readyRead, reply, [reply] {
        if (reply->bytesAvailable() > 1024 * 1024) reply->abort();
    });
    m_status = r.value("CALL") + ": sending to QRZ…";
    emit changed();
    connect(reply, &QNetworkReply::finished, this, [this, reply, id, key] {
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto fields = parseResponse(reply->read(1024 * 1024));
        const auto remoteId = fields.value("LOGID", fields.value("LOGIDS"));
        static const QRegularExpression numericId("^[1-9][0-9]*$");
        const bool success = reply->error() == QNetworkReply::NoError && http >= 200 && http < 300 &&
            fields.value("RESULT") == "OK" && fields.value("COUNT") == "1" && numericId.match(remoteId).hasMatch();
        QString reason;
        if (!success) {
            if (reply->error() != QNetworkReply::NoError || http >= 300 || http < 200)
                reason = "QRZ did not confirm the upload. Check connectivity and QRZ before retrying; acceptance may be unknown.";
            else reason = fields.value("REASON", fields.value("RESULT") == "AUTH"
                ? "QRZ rejected access. Check your API key and subscription."
                : "QRZ did not confirm acceptance. Check the QRZ logbook before retrying.");
            reason.replace(key, "[redacted]", Qt::CaseInsensitive);
            reason.replace(QString::fromLatin1(QUrl::toPercentEncoding(key)), "[redacted]", Qt::CaseInsensitive);
            reason = reason.left(400);
        }
        reply->deleteLater();
        finish(id, success, remoteId, reason);
    });
}
void QrzLogbook::finish(const QString &id, bool success, const QString &remoteId, const QString &message) {
    Ft8Logbook log(m_path);
    QString error, call;
    if (log.load(&error)) {
        const int index = findId(log, id);
        if (index >= 0) {
            call = log.records()[index].value("CALL");
            AdifRecord fields{{"_QK4_QRZ_STATE", success ? "SENT" : "ERROR"}, {"_QK4_QRZ_ERROR", message}};
            if (success) {
                fields["QRZCOM_QSO_UPLOAD_STATUS"] = "Y";
                fields["QRZCOM_QSO_UPLOAD_DATE"] = QDateTime::currentDateTimeUtc().toString("yyyyMMdd");
                fields["APP_QRZLOG_LOGID"] = remoteId;
            }
            log.patchUpload(index, fields, &error);
        } else error = "Contact no longer exists.";
    }
    m_status = error.isEmpty() ? call + ": " + (success ? "sent to QRZ." : message)
        : "QRZ status could not be saved: " + error + " Check QRZ before retrying.";
    m_busy = false;
    emit changed();
    QTimer::singleShot(0, this, &QrzLogbook::process);
}
