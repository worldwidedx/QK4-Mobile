#pragma once

#include <QApplication>
#include <QInputMethod>
#include <QKeyEvent>
#include <QPointer>
#include <QWidget>
#include <QWindow>
#include <functional>
#include <utility>

// Child overlays share the radio's native window. Own the entire Back action
// before a focused editor can propagate it to Android's activity exit handler.
class OverlayBackHandler final : public QObject {
public:
    explicit OverlayBackHandler(QWidget *overlay, std::function<void()> dismiss = {})
        : QObject(overlay), m_overlay(overlay), m_dismiss(std::move(dismiss)) {
        overlay->installEventFilter(this);
        if (overlay->isVisible()) activate();
    }
    ~OverlayBackHandler() override { if (qApp) qApp->removeEventFilter(this); }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == m_overlay) {
            if (event->type() == QEvent::Show) activate();
            else if (event->type() == QEvent::Hide) {
                qApp->removeEventFilter(this);
                m_active = false;
            }
        }
        if (!m_active || !m_overlay->isVisible()) return false;
        if (event->type() != QEvent::ShortcutOverride && event->type() != QEvent::KeyPress
            && event->type() != QEvent::KeyRelease) return false;
        const auto *widget = qobject_cast<QWidget *>(watched);
        if (watched != m_overlay->window()->windowHandle()
            && (!widget || widget->window() != m_overlay->window())) return false;
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() != Qt::Key_Back && key->key() != Qt::Key_Escape) return false;
        event->accept();
        if (event->type() == QEvent::KeyPress && !key->isAutoRepeat()) {
            m_keyboard = QGuiApplication::inputMethod()->isVisible();
            if (m_keyboard) QGuiApplication::inputMethod()->hide();
        }
        if (event->type() == QEvent::KeyRelease && !key->isAutoRepeat()) {
            const bool dismiss = !m_keyboard;
            m_keyboard = false;
            if (dismiss) {
                // The existing close path may open an unsaved-changes sheet.
                // It gets the next Back action, not the release of this one.
                const QPointer<QWidget> overlay = m_overlay, focus = m_previousFocus;
                const auto callback = m_dismiss;
                if (callback) callback(); else overlay->hide();
                if (overlay && !overlay->isVisible() && focus && focus->isVisible() && focus->isEnabled())
                    focus->setFocus(Qt::OtherFocusReason);
            }
        }
        return true;
    }

private:
    void activate() {
        if (m_active) return;
        m_active = true;
        m_keyboard = false;
        m_previousFocus = QApplication::focusWidget();
        // Qt runs the most recently installed filter first: nested sheets win.
        qApp->installEventFilter(this);
    }
    QWidget *m_overlay;
    QPointer<QWidget> m_previousFocus;
    std::function<void()> m_dismiss;
    bool m_active = false, m_keyboard = false;
};
