#include "touchtextedit.h"

#include <QAbstractScrollArea>
#include <QEvent>
#include <QLineEdit>
#include <QObject>
#include <QPointer>
#include <QScroller>
#include <QScrollerProperties>
#include <QTimer>
#include <QTouchEvent>

#ifdef Q_OS_IOS
namespace {

// Re-applies the same press delay/drag distance the pages use elsewhere.
void regrabScroller(QWidget *viewport, const QScrollerProperties &properties) {
    QScroller::grabGesture(viewport, QScroller::TouchGesture);
    if (QScroller *scroller = QScroller::scroller(viewport))
        scroller->setScrollerProperties(properties);
}

class FieldTouchFilter : public QObject {
public:
    FieldTouchFilter(QWidget *viewport, const QScrollerProperties &properties)
        : QObject(viewport), m_viewport(viewport), m_properties(properties) {}

protected:
    bool eventFilter(QObject *, QEvent *event) override {
        switch (event->type()) {
        case QEvent::TouchBegin: {
            auto *touch = static_cast<QTouchEvent *>(event);
            if (touch->points().isEmpty() || m_bypassed)
                break;
            const QPoint pos = touch->points().first().position().toPoint();
            if (auto *edit = qobject_cast<QLineEdit *>(m_viewport->childAt(pos))) {
                // Fields are often created after the page is set up, so pick
                // them up on first touch (the first tap precedes any double-click).
                TouchTextEdit::selectAllOnDoubleTap(edit);
                m_bypassed = true;
                QScroller::ungrabGesture(m_viewport);
            }
            break;
        }
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
            if (m_bypassed) {
                m_bypassed = false;
                QTimer::singleShot(0, this, [this]() { regrabScroller(m_viewport, m_properties); });
            }
            break;
        default:
            break;
        }
        return false;
    }

private:
    QWidget *m_viewport;
    QScrollerProperties m_properties;
    bool m_bypassed = false;
};

class SelectAllFilter : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::MouseButtonDblClick) {
            if (auto *edit = qobject_cast<QLineEdit *>(watched)) {
                edit->selectAll();
                return true;
            }
        }
        return false;
    }
};

} // namespace
#endif

namespace TouchTextEdit {

void enableNativeEditMenu(QAbstractScrollArea *area) {
#ifdef Q_OS_IOS
    if (!area)
        return;
    QWidget *viewport = area->viewport();
    // A strong-focus scroll area takes focus when a touch lands on its
    // background or QScroller replays a delayed press. That pulls focus from
    // the text field mid-tap: the selection is cleared and Qt's iOS edit-menu
    // tap recognizer can be torn down while it still has a menu queued
    // (crash in showEditMenu with a nil view on Qt 6.11.1).
    area->setFocusPolicy(Qt::NoFocus);
    viewport->setFocusPolicy(Qt::NoFocus);
    QScrollerProperties properties;
    if (QScroller *scroller = QScroller::scroller(viewport))
        properties = scroller->scrollerProperties();
    viewport->installEventFilter(new FieldTouchFilter(viewport, properties));
#else
    Q_UNUSED(area);
#endif
}

void selectAllOnDoubleTap(QLineEdit *edit) {
#ifdef Q_OS_IOS
    if (!edit || edit->property("qk4SelectAllOnDoubleTap").toBool())
        return;
    edit->setProperty("qk4SelectAllOnDoubleTap", true);
    edit->installEventFilter(new SelectAllFilter(edit));
#else
    Q_UNUSED(edit);
#endif
}

} // namespace TouchTextEdit
