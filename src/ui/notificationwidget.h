#ifndef NOTIFICATIONWIDGET_H
#define NOTIFICATIONWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QLabel>

/**
 * NotificationWidget - Center-screen notification overlay
 *
 * Displays a message in the center of the parent widget and
 * automatically dismisses after a configurable timeout (default 2 seconds).
 *
 * Usage:
 *   auto notification = new NotificationWidget(parentWidget);
 *   notification->showMessage("KPA1500 Status: operate.");
 */
class NotificationWidget : public QWidget {
    Q_OBJECT

public:
    explicit NotificationWidget(QWidget *parent = nullptr);

    // Show a message for the specified duration (ms)
    void showMessage(const QString &message, int durationMs = 2000);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onTimeout();

private:
    void updatePosition();

    QLabel *m_label;
    QTimer *m_timer;
};

#endif // NOTIFICATIONWIDGET_H
