#ifndef ACCESSIBILITYTEARDOWN_H
#define ACCESSIBILITYTEARDOWN_H

#include <QAccessible>
#include <QWidget>

// Qt 6.11.1 sends the accessibility "destroyed" event from ~QWidget, after the
// subclass destructors (for example ~QAbstractButton) have already run. The
// iOS bridge answers it by calling QAccessibleInterface::role(), which for a
// button reads the dead button's private data and crashes (SIGSEGV at 0x8)
// once an assistive client such as VoiceOver has touched the widget.
//
// Call this from the destructor body of a widget whose subtree is about to be
// deleted. Dropping each cached interface first means Qt sends no destroyed
// event for those widgets. Interfaces are created lazily on the next query, so
// this is harmless if the widgets live on.
//
// Only effective for a subtree that is already hidden: if the widget is still
// visible, ~QWidget hides it after the destructor body runs, and the resulting
// ObjectHide events re-create an interface for every child before the
// children are deleted. For the visible main window at exit, use
// suspendAccessibilityForShutdown() instead.
inline void releaseAccessibilityInterfaces(QWidget *root) {
    if (!root || !QAccessible::isActive())
        return;
    auto release = [](QObject *object) {
        if (QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(object))
            QAccessible::deleteAccessibleInterface(QAccessible::uniqueId(iface));
    };
    const auto widgets = root->findChildren<QWidget *>();
    for (QWidget *widget : widgets)
        release(widget);
    release(root);
}

// Application exit only. ~QWidget sends destroyed events only while
// QAccessible::isActive(), and the iOS bridge ignores every event while
// inactive, so turning accessibility off before the main window's widgets
// are torn down prevents the crash regardless of interfaces re-created by
// ~QWidget's own hide step. Never call this while the UI is still in use.
inline void suspendAccessibilityForShutdown() {
    if (QAccessible::isActive())
        QAccessible::setActive(false);
}

#endif // ACCESSIBILITYTEARDOWN_H
