#ifndef INWINDOWDIALOG_H
#define INWINDOWDIALOG_H

#include <QString>
#include <QWidget>

class QEventLoop;
class QEvent;
class QFrame;
class QKeyEvent;

// A modal presentation that stays inside the existing QWidget top-level.
// Android's QRhi-backed window must not create a second native EGL surface
// while accessibility is inspecting the main window.
class InWindowDialog : public QWidget {
    Q_OBJECT

public:
    enum DialogCode { Rejected = 0, Accepted = 1 };

    explicit InWindowDialog(QWidget *parent);

    QWidget *contentWidget() const;
    void setPanelSize(const QSize &size);
    int exec();

public slots:
    void accept();
    void reject();
    void done(int result);

signals:
    void accepted();
    void rejected();
    void finished(int result);
    void panelResized(const QSize &size);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void fitToParent();
    QFrame *m_panel = nullptr;
    QEventLoop *m_eventLoop = nullptr;
    QSize m_preferredPanelSize;
    int m_result = Rejected;
    bool m_backHidKeyboard = false;
};

void showInWindowMessage(QWidget *parent, const QString &title, const QString &message);

#endif // INWINDOWDIALOG_H
