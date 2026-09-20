#include "sidecontroloverlay.h"
#include "overlaybackhandler.h"
#include "k4styles.h"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>

SideControlOverlay::SideControlOverlay(Context ctx, QWidget *parent) : QWidget(parent), m_context(ctx) {
    new OverlayBackHandler(this);
    // Start hidden
    hide();
}

void SideControlOverlay::showOverGroup(QWidget *topWidget, QWidget *bottomWidget) {
    if (!topWidget || !bottomWidget || !parentWidget()) {
        return;
    }

    // Calculate geometry to cover both widgets
    QPoint topLeft = topWidget->mapTo(parentWidget(), QPoint(0, 0));
    QPoint bottomRight = bottomWidget->mapTo(parentWidget(), QPoint(bottomWidget->width(), bottomWidget->height()));

    int x = topLeft.x();
    int y = topLeft.y();
    int w = bottomRight.x() - topLeft.x();
    int h = bottomRight.y() - topLeft.y();

    setGeometry(x, y, w, h);
    raise(); // Bring to front
    show();
}

void SideControlOverlay::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect r = rect();

    // Draw dark background with rounded corners
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(K4Styles::Colors::DarkBackground));
    painter.drawRoundedRect(r, CornerRadius, CornerRadius);

    // Draw indicator bar on the left
    QRect barRect(0, 0, IndicatorBarWidth, r.height());
    painter.setBrush(indicatorColor());
    painter.drawRoundedRect(barRect, CornerRadius / 2, CornerRadius / 2);

    // Draw subtle border
    painter.setPen(QPen(QColor(K4Styles::Colors::BorderNormal), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(r.adjusted(0, 0, -1, -1), CornerRadius, CornerRadius);
}

void SideControlOverlay::wheelEvent(QWheelEvent *event) {
    int steps = m_wheelAccumulator.accumulate(event);
    if (steps != 0)
        emit valueScrolled(steps);
    event->accept();
}

void SideControlOverlay::mousePressEvent(QMouseEvent *event) {
    // Close on any click within the overlay
    // Subclasses can override for different behavior
    Q_UNUSED(event)
    hide();
}

void SideControlOverlay::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    emit closed();
}

QColor SideControlOverlay::indicatorColor() const {
    switch (m_context) {
    case Global:
        return QColor(K4Styles::Colors::AccentAmber);
    case MainRx:
        return QColor(K4Styles::Colors::VfoACyan);
    case SubRx:
        return QColor(K4Styles::Colors::VfoBGreen);
    default:
        return QColor(K4Styles::Colors::AccentAmber);
    }
}
