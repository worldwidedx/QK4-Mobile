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

#endif // ACCESSIBILITYTEARDOWN_H
