#ifndef HALIKEYWORKERBASE_H
#define HALIKEYWORKERBASE_H

#include <QObject>
#include <QString>
#include <atomic>

class HaliKeyWorkerBase : public QObject {
    Q_OBJECT

public:
    explicit HaliKeyWorkerBase(const QString &portName, QObject *parent = nullptr);
    ~HaliKeyWorkerBase() override = default;

    virtual void prepareShutdown() {} // Override for platform-specific unblocking (e.g. Linux TIOCMIWAIT)

public slots:
    virtual void start() = 0; // Called when thread starts
    void stop();              // Sets atomic flag to exit loop

signals:
    void ditStateChanged(bool pressed);
    void dahStateChanged(bool pressed);
    void pttStateChanged(bool pressed);
    void errorOccurred(const QString &error);
    void portOpened();

protected:
    QString m_portName;
    std::atomic<bool> m_running{false};
};

#endif // HALIKEYWORKERBASE_H
