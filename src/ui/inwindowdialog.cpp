#include "inwindowdialog.h"

#include "k4styles.h"

#include <QApplication>
#include <QEventLoop>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QInputMethod>
#include <QLabel>
#include <QPushButton>
#include <QPointer>
#include <QVBoxLayout>
#include <QWindow>

InWindowDialog::InWindowDialog(QWidget *parent)
    : QWidget(parent) {
    if (parent)
        parent->installEventFilter(this);
    setObjectName("inWindowDialogOverlay");
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet(QString(
        "#inWindowDialogOverlay { background-color: rgba(0, 0, 0, 150); }"
        "#inWindowDialogPanel { background-color: %1; border: 1px solid %2; border-radius: 7px; }")
                          .arg(K4Styles::Colors::Background, K4Styles::Colors::DialogBorder));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 6, 8, 6);
    outer->addStretch(1);

    auto *row = new QHBoxLayout();
    row->addStretch(1);
    m_panel = new QFrame(this);
    m_panel->setObjectName("inWindowDialogPanel");
    m_panel->setAttribute(Qt::WA_StyledBackground, true);
    row->addWidget(m_panel);
    row->addStretch(1);
    outer->addLayout(row);
    outer->addStretch(1);
    hide();
}

QWidget *InWindowDialog::contentWidget() const {
    return m_panel;
}

void InWindowDialog::setPanelSize(const QSize &size) {
    m_preferredPanelSize = size;
    fitToParent();
}

int InWindowDialog::exec() {
    if (!parentWidget())
        return Rejected;

    fitToParent();
    m_result = Rejected;
    m_backHidKeyboard = false;
    const QPointer<QWidget> previousFocus = QApplication::focusWidget();
    show();
    raise();
    setFocus(Qt::OtherFocusReason);

    QEventLoop eventLoop;
    m_eventLoop = &eventLoop;
    // The overlay is a child widget, not a native modal window. Intercept Back
    // before focused editors/lists can ignore it and propagate it to the screen
    // underneath. Qt calls the most recently installed filter first, so nested
    // sheets handle their own Back without dismissing the logbook as well.
    qApp->installEventFilter(this);
    eventLoop.exec();
    qApp->removeEventFilter(this);
    m_eventLoop = nullptr;
    if (previousFocus && previousFocus->isVisible() && previousFocus->isEnabled())
        previousFocus->setFocus(Qt::OtherFocusReason);
    return m_result;
}

bool InWindowDialog::eventFilter(QObject *watched, QEvent *event) {
    if (m_eventLoop && isVisible()
        && (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress
            || event->type() == QEvent::KeyRelease)) {
        const auto *widget = qobject_cast<QWidget *>(watched);
        if (watched == window()->windowHandle() || (widget && widget->window() == window())) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Back || key->key() == Qt::Key_Escape) {
                event->accept();
                if (event->type() == QEvent::KeyPress && !key->isAutoRepeat()) {
                    m_backHidKeyboard = QGuiApplication::inputMethod()->isVisible();
                    if (m_backHidKeyboard)
                        QGuiApplication::inputMethod()->hide();
                }
                // Keep the sheet alive through the key release; otherwise the
                // second half of Android Back can reach the invoking screen.
                if (event->type() == QEvent::KeyRelease && !key->isAutoRepeat()) {
                    if (!m_backHidKeyboard)
                        reject();
                    m_backHidKeyboard = false;
                }
                return true;
            }
        }
    }
    if (watched == parentWidget() && event->type() == QEvent::Resize)
        fitToParent();
    return QWidget::eventFilter(watched, event);
}

void InWindowDialog::fitToParent() {
    if (!parentWidget() || !m_panel)
        return;
    setGeometry(parentWidget()->rect());
    if (!m_preferredPanelSize.isValid())
        return;
    const QSize available(qMax(1, parentWidget()->width() - 16),
                          qMax(1, parentWidget()->height() - 12));
    const QSize panelSize = m_preferredPanelSize.boundedTo(available);
    if (m_panel->size() != panelSize) {
        m_panel->setFixedSize(panelSize);
        emit panelResized(panelSize);
    }
}

void InWindowDialog::accept() {
    done(Accepted);
}

void InWindowDialog::reject() {
    done(Rejected);
}

void InWindowDialog::done(int result) {
    m_result = result;
    hide();
    if (result == Accepted)
        emit accepted();
    else
        emit rejected();
    emit finished(result);
    if (m_eventLoop)
        m_eventLoop->quit();
}

void showInWindowMessage(QWidget *parent, const QString &title, const QString &message) {
    if (!parent)
        return;

    InWindowDialog dialog(parent);
    QWidget *panel = dialog.contentWidget();
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(10);

    auto *titleLabel = new QLabel(title, panel);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(QString("color: %1; font-size: 17px; font-weight: bold;")
                                  .arg(K4Styles::Colors::AccentAmber));
    layout->addWidget(titleLabel);

    auto *messageLabel = new QLabel(message, panel);
    messageLabel->setWordWrap(true);
    messageLabel->setTextFormat(Qt::AutoText);
    messageLabel->setOpenExternalLinks(true);
    messageLabel->setStyleSheet(QString("color: %1; font-size: 13px;").arg(K4Styles::Colors::TextWhite));
    layout->addWidget(messageLabel, 1);

    auto *close = new QPushButton("CLOSE", panel);
    close->setMinimumHeight(36);
    close->setStyleSheet(K4Styles::menuBarButton());
    layout->addWidget(close);
    QObject::connect(close, &QPushButton::clicked, &dialog, &InWindowDialog::accept);

    const QSize available = parent->size() - QSize(20, 16);
    const int width = qMin(560, qMax(280, available.width()));
    const int height = qMin(360, qMax(150, layout->sizeHint().height()));
    dialog.setPanelSize(QSize(width, qMin(height, available.height())));
    dialog.exec();
}
