#include <QtTest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "ft8/qrzlogbook.h"

class Reply : public QNetworkReply {
public:
    QByteArray data;
    qsizetype offset = 0;
    explicit Reply(const QNetworkRequest &request, QObject *parent) : QNetworkReply(parent) {
        setRequest(request); setUrl(request.url()); open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    }
    void complete(const QByteArray &body, int http = 200, NetworkError error = NoError) {
        data = body;
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, http);
        if (error != NoError) setError(error, "Mock transport failure");
        setFinished(true); emit readyRead(); emit finished();
    }
    void abort() override { if (!isFinished()) complete({}, 0, OperationCanceledError); }
    qint64 bytesAvailable() const override { return data.size() - offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char *out, qint64 max) override {
        const auto count = qMin(max, qint64(data.size() - offset));
        if (count <= 0) return -1;
        memcpy(out, data.constData() + offset, size_t(count)); offset += count; return count;
    }
};
class Network : public QNetworkAccessManager {
public:
    QList<QByteArray> bodies;
    QList<QNetworkRequest> requests;
    Reply *pending = nullptr;
protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoing) override {
        Q_ASSERT(op == PostOperation);
        bodies << outgoing->readAll(); requests << request;
        pending = new Reply(request, this); return pending;
    }
};
struct Keys {
    QString value;
    bool unavailable = false;
    QrzKeyStore store() {
        return {[this](QString *error) {
                    if (unavailable) { if (error) *error = "Secure store unavailable"; return QString(); } return value;
                }, [this](const QString &v, QString *error) {
                    if (unavailable) { if (error) *error = "Secure store unavailable"; return false; } value = v; return true;
                }, [this](QString *) { value.clear(); return true; }};
    }
};
static AdifRecord contact(QString call = "W1AW", QString mode = "FT8") {
    return {{"CALL", call}, {"STATION_CALLSIGN", "AE6LX"}, {"QSO_DATE", "20260910"},
            {"TIME_ON", "123456"}, {"MODE", mode}, {"FREQ", "14.074"}, {"RST_SENT", "-08"}, {"RST_RCVD", "-14"}};
}
class QrzTest : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("QK4-tests");
        QCoreApplication::setApplicationName("QRZ-isolated");
        QCoreApplication::setApplicationVersion("1.0.5");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    }
    void init() { QSettings().clear(); Ft8Logbook::contactAdded = {}; }
    void cleanup() { Ft8Logbook::contactAdded = {}; }
    void confirmedUploadAndImmutableExport() {
        QTemporaryDir dir;
        auto path = dir.filePath("log.json");
        Ft8Logbook log(path);
        QVERIFY(log.append(contact()));
        Network net; Keys keys;
        QrzLogbook qrz(path, nullptr, &net, keys.store());
        QCoreApplication::processEvents();
        QString error;
        const QString secret = "test+key&=value";
        QVERIFY(qrz.configure("ae6lx", secret, false, &error));
        QVERIFY(qrz.send(0, &error));
        QVERIFY(!qrz.send(0, &error));
        QTRY_COMPARE(net.bodies.size(), 1);
        QCOMPARE(net.requests[0].url().toString(), "https://logbook.qrz.com/api");
        QCOMPARE(net.requests[0].rawHeader("User-Agent"), "QK4-Mobile/1.0.5");
        QCOMPARE(net.requests[0].attribute(QNetworkRequest::RedirectPolicyAttribute).toInt(), int(QNetworkRequest::ManualRedirectPolicy));
        QVERIFY(net.bodies[0].contains("KEY=test%2Bkey%26%3Dvalue&ACTION=INSERT&ADIF="));
        QVERIFY(!net.bodies[0].contains("REPLACE"));
        QVERIFY(!net.bodies[0].contains("_QK4_"));
        QVERIFY(log.load());
        QVERIFY(!QrzLogbook::sent(log.records()[0]));
        auto edited = log.records()[0]; edited["CALL"] = "K1ABC";
        QVERIFY(!log.replace(0, edited, &error));
        // Another mode logs while the network request is pending.
        Ft8Logbook sstv(path); QVERIFY(sstv.load()); QVERIFY(sstv.append(contact("K2ABC", "SSTV")));
        net.pending->complete("RESULT=OK&LOGID=12345&COUNT=1");
        QVERIFY(log.load());
        QCOMPARE(log.records().size(), 2);
        QVERIFY(QrzLogbook::sent(log.records()[0]));
        QCOMPARE(log.records()[0].value("QRZCOM_QSO_UPLOAD_DATE"), QDateTime::currentDateTimeUtc().toString("yyyyMMdd"));
        QVERIFY(!qrz.send(0, &error));
        edited = log.records()[0]; edited.remove("QRZCOM_QSO_UPLOAD_STATUS"); edited["COMMENT"] = "Local change";
        QVERIFY(log.replace(0, edited, &error));
        QCOMPARE(log.records()[0].value("QRZCOM_QSO_UPLOAD_STATUS"), "M");
        QVERIFY(QrzLogbook::sent(log.records()[0]));
        const auto adif = log.exportAdif();
        QVERIFY(adif.contains("<QRZCOM_QSO_UPLOAD_STATUS:1>M"));
        QVERIFY(adif.contains("<QRZCOM_QSO_UPLOAD_DATE:8>"));
        QVERIFY(!adif.contains("_QK4_")); QVERIFY(!adif.contains(secret));
        QCOMPARE(Ft8Logbook::parse(adif).records[0].value("QRZCOM_QSO_UPLOAD_STATUS"), "M");
        QFile prefs(QSettings().fileName()); QVERIFY(prefs.open(QIODevice::ReadOnly));
        QVERIFY(!prefs.readAll().contains(secret.toUtf8()));
    }
    void failuresNeverMarkSent_data() {
        QTest::addColumn<QByteArray>("response");
        QTest::newRow("duplicate") << QByteArray("RESULT=FAIL&REASON=Duplicate+QSO");
        QTest::newRow("auth") << QByteArray("RESULT=AUTH");
        QTest::newRow("zero") << QByteArray("RESULT=OK&COUNT=0&LOGID=123");
        QTest::newRow("html") << QByteArray("<html>Proxy login</html>");
        QTest::newRow("conflicting") << QByteArray("RESULT=OK&RESULT=FAIL&COUNT=1&LOGID=123");
        QTest::newRow("replace") << QByteArray("RESULT=REPLACE&COUNT=1&LOGID=123");
        QTest::newRow("missing-id") << QByteArray("RESULT=OK&COUNT=1");
    }
    void failuresNeverMarkSent() {
        QFETCH(QByteArray, response);
        QTemporaryDir dir; const auto path = dir.filePath("log.json");
        Ft8Logbook log(path); QVERIFY(log.append(contact()));
        Network net; Keys keys; QrzLogbook qrz(path, nullptr, &net, keys.store());
        QCoreApplication::processEvents();
        QString error; QVERIFY(qrz.configure("AE6LX", "key", false, &error)); QVERIFY(qrz.send(0, &error));
        QTRY_COMPARE(net.bodies.size(), 1); net.pending->complete(response);
        QVERIFY(log.load()); QVERIFY(!QrzLogbook::sent(log.records()[0]));
        QCOMPARE(log.records()[0].value("_QK4_QRZ_STATE"), "ERROR");
        QVERIFY(qrz.send(0, &error)); QTRY_COMPARE(net.bodies.size(), 2);
        net.pending->complete("RESULT=OK&LOGIDS=456&COUNT=1");
        QVERIFY(log.load()); QVERIFY(QrzLogbook::sent(log.records()[0]));
    }
    void autoSendPracticeMismatchAndSecureFailure() {
        QTemporaryDir dir; const auto path = dir.filePath("log.json");
        Network net; Keys keys; QrzLogbook qrz(path, nullptr, &net, keys.store());
        QCoreApplication::processEvents();
        QString error; keys.unavailable = true;
        QVERIFY(!qrz.configure("AE6LX", "secret", true, &error)); QVERIFY(!qrz.configured());
        keys.unavailable = false; QVERIFY(qrz.configure("AE6LX", "secret", true, &error));
        Ft8Logbook::contactAdded = [&qrz](const QString &path, int index) { qrz.contactAdded(path, index); };
        Ft8Logbook log(path); auto practice = contact("K1ABC"); practice["APP_QK4_PRACTICE"] = "Y";
        QVERIFY(log.append(practice));
        auto mismatch = contact("K2ABC"); mismatch["STATION_CALLSIGN"] = "AE6LX/P";
        QVERIFY(log.append(mismatch));
        Ft8Logbook memory; QVERIFY(memory.append(contact()));
        QVERIFY(log.append(contact("W1AW", "FT4")));
        QTRY_COMPARE(net.bodies.size(), 1);
        QVERIFY(!qrz.send(0, &error)); QVERIFY(!qrz.send(1, &error));
        net.pending->complete("RESULT=FAIL&REASON=Invalid+secret");
        QVERIFY(!qrz.status().contains("secret"));
        QVERIFY(log.load()); QVERIFY(!log.records()[2].value("_QK4_QRZ_ERROR").contains("secret"));
        QVERIFY(qrz.forget(&error)); QVERIFY(keys.value.isEmpty()); QVERIFY(!qrz.automatic());
        QVERIFY(log.load()); QCOMPARE(log.records().size(), 3);
    }
    void interruptedUploadIsNotBlindlyRetried() {
        QTemporaryDir dir; const auto path = dir.filePath("log.json");
        auto r = contact(); r["_QK4_ID"] = "stable-id"; r["_QK4_QRZ_STATE"] = "SENDING";
        Ft8Logbook log(path); QVERIFY(log.append(r));
        Network net; Keys keys; keys.value = "key";
        QSettings().setValue("qrz/callsign", "AE6LX");
        QrzLogbook qrz(path, nullptr, &net, keys.store());
        QCoreApplication::processEvents();
        QCOMPARE(net.bodies.size(), 0); QVERIFY(log.load());
        QCOMPARE(log.records()[0].value("_QK4_QRZ_STATE"), "ERROR");
        QVERIFY(!QrzLogbook::sent(log.records()[0]));
        QString error; QVERIFY(qrz.send(0, &error)); QTRY_COMPARE(net.bodies.size(), 1);
        net.pending->abort(); QVERIFY(log.load()); QVERIFY(!QrzLogbook::sent(log.records()[0]));
        QCoreApplication::processEvents(); QCOMPARE(net.bodies.size(), 1);
    }
    void importedDataCannotQueueUploads() {
        auto text = Ft8Logbook::encode({contact()});
        text.replace("<EOR>", "<_QK4_ID:3>123<_QK4_QRZ_STATE:6>QUEUED<EOR>");
        const auto parsed = Ft8Logbook::parse(text);
        QVERIFY(parsed.errors.isEmpty());
        QVERIFY(!parsed.records[0].contains("_QK4_ID"));
        QVERIFY(!parsed.records[0].contains("_QK4_QRZ_STATE"));
        auto raw = contact(); raw["_QK4_QRZ_STATE"] = "QUEUED";
        AdifImport incoming; incoming.records << raw;
        Ft8Logbook log; QVERIFY(log.importRecords(incoming));
        QVERIFY(!log.records()[0].contains("_QK4_QRZ_STATE"));
    }
};
QTEST_GUILESS_MAIN(QrzTest)
#include "test_qrzlogbook.moc"
