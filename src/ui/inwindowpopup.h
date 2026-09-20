#ifndef INWINDOWPOPUP_H
#define INWINDOWPOPUP_H

#include <QPoint>
#include <QWidget>
#include "overlaybackhandler.h"

namespace InWindowPopup {

inline void configure(QWidget *widget) {
#ifdef Q_OS_ANDROID
    // Android accessibility and QRhi-backed QWidget windows can deadlock
    // while a second native EGL surface is exposed. Keep transient controls
    // in the existing MainWindow surface instead.
    widget->setWindowFlags(Qt::Widget);
    // These objects are constructed before MainWindow is shown. Top-level
    // popups stayed unmapped by default, whereas an ordinary child would be
    // revealed with its parent unless it has an explicit hidden state.
    widget->hide();
    new OverlayBackHandler(widget);
#else
    widget->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
#endif
    widget->setAttribute(Qt::WA_TranslucentBackground);
    widget->setFocusPolicy(Qt::StrongFocus);
}

inline QPoint parentPositionForGlobal(QWidget *widget, const QPoint &globalPosition) {
#ifdef Q_OS_ANDROID
    if (QWidget *parent = widget->parentWidget())
        return parent->mapFromGlobal(globalPosition);
#endif
    return globalPosition;
}

inline void moveFromGlobal(QWidget *widget, const QPoint &globalPosition) {
    widget->move(parentPositionForGlobal(widget, globalPosition));
}

} // namespace InWindowPopup

#endif // INWINDOWPOPUP_H
