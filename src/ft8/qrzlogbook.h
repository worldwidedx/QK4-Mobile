#pragma once
#include "ft8logbook.h"
#include <QObject>
#include <QQueue>
#include <functional>
class QNetworkAccessManager;

// Credentials never enter QSettings, ADIF records, URLs, or diagnostics.
struct QrzKeyStore {
    std::function<QString(QString *)> read;
    std::function<bool(const QString &, QString *)> write;
    std::function<bool(QString *)> clear;
    static QrzKeyStore platform();
};
class QrzLogbook : public QObject {
    Q_OBJECT
public:
    explicit QrzLogbook(const QString &path, QObject *parent = nullptr,
                        QNetworkAccessManager *network = nullptr, QrzKeyStore keys = QrzKeyStore::platform());
    static QrzLogbook *instance();
    static void start(const QString &path, QObject *parent);
    QString callsign() const { return m_call; }
    bool automatic() const { return m_auto; }
    bool configured() const { return !m_call.isEmpty() && m_hasKey; }
    // Used only after the operator presses Show in Setup; never cached or logged.
    QString revealKey(QString *error) const { return m_keys.read(error); }
    bool configure(const QString &call, const QString &replacementKey, bool automatic, QString *error);
    bool forget(QString *error);
    bool send(int index, QString *error);
    void contactAdded(const QString &path, int index);
    QString status() const { return m_status; }
    static bool sent(const AdifRecord &record);
    static bool managedField(const QString &field);
    static QMap<QString, QString> parseResponse(const QByteArray &bytes);
signals:
    void changed();
private:
    void process();
    void finish(const QString &id, bool success, const QString &remoteId, const QString &message);
    void resume();
    QString m_path, m_call, m_status;
    bool m_auto = false, m_hasKey = false, m_busy = false;
    QrzKeyStore m_keys;
    QNetworkAccessManager *m_network;
    QQueue<QString> m_queue;
};
