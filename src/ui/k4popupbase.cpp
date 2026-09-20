#include "k4popupbase.h"
#include "k4styles.h"
#include "inwindowpopup.h"
#include <QApplication>
#include <QHideEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>

K4PopupBase::K4PopupBase(QWidget *parent) : QWidget(parent) {
    InWindowPopup::configure(this);
}

QMargins K4PopupBase::contentMargins() const {
    int sm = K4Styles::Dimensions::ShadowMargin;
    int cm = K4Styles::Dimensions::PopupContentMargin;

    return QMargins(sm + cm, // left
                    sm + cm, // top
                    sm + cm, // right
                    sm + cm  // bottom
    );
}

void K4PopupBase::initPopup() {
    QSize cs = contentSize();
    int totalWidth = cs.width() + 2 * K4Styles::Dimensions::ShadowMargin;
    int totalHeight = cs.height() + 2 * K4Styles::Dimensions::ShadowMargin;
    setFixedSize(totalWidth, totalHeight);
}

QRect K4PopupBase::contentRect() const {
    QSize cs = contentSize();
    return QRect(K4Styles::Dimensions::ShadowMargin, K4Styles::Dimensions::ShadowMargin, cs.width(), cs.height());
}

void K4PopupBase::showAboveButton(QWidget *triggerButton) {
    showAboveWidget(triggerButton);
}

void K4PopupBase::showAboveWidget(QWidget *referenceWidget) {
    if (!referenceWidget)
        return;

    // Ensure our geometry is set before positioning
    adjustSize();

    // Get the reference widget's parent (typically button bar) for centering
    QWidget *parentBar = referenceWidget->parentWidget();
    if (!parentBar)
        parentBar = referenceWidget;

    // Get global positions
    QPoint barGlobal = parentBar->mapToGlobal(QPoint(0, 0));
    QPoint refGlobal = referenceWidget->mapToGlobal(QPoint(0, 0));

    int barCenterX = barGlobal.x() + parentBar->width() / 2;

    // Content dimensions (use calculated size, not widget height() which may not be realized)
    QSize cs = contentSize();
    int sm = K4Styles::Dimensions::ShadowMargin;
    int totalHeight = cs.height() + 2 * sm;

    // Center content area above the parent bar (account for shadow margin offset)
    int popupX = barCenterX - cs.width() / 2 - sm;

    // Position popup so its bottom edge (including shadow margin) is at the top of the reference widget
    int popupY = refGlobal.y() - totalHeight;

    // Ensure popup stays on screen
    QRect screenGeom = referenceWidget->screen()->availableGeometry();

    // Check left edge
    if (popupX < screenGeom.left() - K4Styles::Dimensions::ShadowMargin) {
        popupX = screenGeom.left() - K4Styles::Dimensions::ShadowMargin;
    }
    // Check right edge
    else if (popupX + width() > screenGeom.right() + K4Styles::Dimensions::ShadowMargin) {
        popupX = screenGeom.right() + K4Styles::Dimensions::ShadowMargin - width();
    }

    // Check top edge (if popup would go off top of screen)
    if (popupY < screenGeom.top() - K4Styles::Dimensions::ShadowMargin) {
        popupY = screenGeom.top() - K4Styles::Dimensions::ShadowMargin;
    }

    // Position the popup - move after show to override Qt's popup positioning
    InWindowPopup::moveFromGlobal(this, QPoint(popupX, popupY));
    show();
#ifndef Q_OS_ANDROID
    move(popupX, popupY); // Move again after show in case Qt repositioned it
#endif
    raise();
    setFocus();
}

void K4PopupBase::hidePopup() {
    hide();
    // closed() signal is emitted by hideEvent()
}

void K4PopupBase::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    emit closed();
}

void K4PopupBase::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        hidePopup();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void K4PopupBase::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Get content rect for drawing
    QRect cr = contentRect();

    // Draw drop shadow
    K4Styles::drawDropShadow(painter, cr, K4Styles::Dimensions::BorderRadiusLarge);

    // Main popup background
    painter.setBrush(QColor(K4Styles::Colors::PopupBackground));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(cr, K4Styles::Dimensions::BorderRadiusLarge, K4Styles::Dimensions::BorderRadiusLarge);

    // Allow subclass to draw additional content
    paintContent(painter, cr);
}

void K4PopupBase::paintContent(QPainter &painter, const QRect &contentRect) {
    // Default implementation does nothing
    // Subclasses can override for custom painting
    Q_UNUSED(painter)
    Q_UNUSED(contentRect)
}
