#include "dualcontrolbutton.h"
#include "k4styles.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QLinearGradient>

DualControlButton::DualControlButton(QWidget *parent) : QWidget(parent) {
    setFixedSize(K4Styles::Dimensions::MenuBarButtonWidth, K4Styles::Dimensions::ButtonHeightLarge);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
    m_longPressTimer = new QTimer(this);
    m_longPressTimer->setSingleShot(true);
    m_longPressTimer->setInterval(550);
    connect(m_longPressTimer, &QTimer::timeout, this, [this]() {
        if (!K4Styles::isCompactLayout() || m_dragging)
            return;
        if (!m_showIndicator)
            emit becameActive();
        swapFunctions();
        emit swapped();
        m_longPressHandled = true;
    });
}

void DualControlButton::setPrimaryLabel(const QString &label) {
    m_primaryLabel = label;
    update();
}

void DualControlButton::setPrimaryValue(const QString &value) {
    m_primaryValue = value;
    update();
}

void DualControlButton::setAlternateLabel(const QString &label) {
    m_alternateLabel = label;
    update();
}

void DualControlButton::setAlternateValue(const QString &value) {
    m_alternateValue = value;
    update();
}

void DualControlButton::setContext(Context context) {
    m_context = context;
    update();
}

void DualControlButton::setShowIndicator(bool show) {
    m_showIndicator = show;
    update();
}

void DualControlButton::swapFunctions() {
    // Swap labels
    QString tempLabel = m_primaryLabel;
    m_primaryLabel = m_alternateLabel;
    m_alternateLabel = tempLabel;

    // Swap values
    QString tempValue = m_primaryValue;
    m_primaryValue = m_alternateValue;
    m_alternateValue = tempValue;

    update();
}

void DualControlButton::cancelPendingTouch() {
    m_longPressTimer->stop();
    // Suppress the eventual release click after QScroller takes this gesture.
    m_dragging = true;
    m_longPressHandled = false;
}

QColor DualControlButton::contextColor() const {
    switch (m_context) {
    case Global:
        return QColor(K4Styles::Colors::AccentAmber);
    case MainRx:
        return QColor(K4Styles::Colors::VfoACyan);
    case SubRx:
        return QColor(K4Styles::Colors::VfoBGreen);
    }
    return QColor(K4Styles::Colors::VfoACyan);
}

void DualControlButton::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int barWidth = 5; // Context indicator bar width
    int radius = 4;   // Subtle rounded corners
    int margin = 1;

    // Background with subtle gradient
    QLinearGradient bgGradient(0, 0, 0, h);
    if (m_isHovered) {
        bgGradient.setColorAt(0.0, QColor(K4Styles::Colors::HoverMid2));
        bgGradient.setColorAt(1.0, QColor(K4Styles::Colors::GradientMid2));
    } else {
        bgGradient.setColorAt(0.0, QColor(K4Styles::Colors::GradientMid1));
        bgGradient.setColorAt(1.0, QColor(K4Styles::Colors::GradientBottom));
    }

    // Main button area (always reserve space for indicator bar so button doesn't shift)
    int buttonLeft = barWidth + margin + 2;
    QRectF buttonRect(buttonLeft, margin, w - buttonLeft - margin, h - margin * 2);
    QPainterPath buttonPath;
    buttonPath.addRoundedRect(buttonRect, radius, radius);

    painter.fillPath(buttonPath, bgGradient);

    // Border (slightly brighter when indicator is shown)
    QColor borderColor =
        m_showIndicator ? QColor(K4Styles::Colors::BorderHover) : QColor(K4Styles::Colors::BorderNormal);
    painter.setPen(QPen(borderColor, 2));
    painter.drawPath(buttonPath);

    // Context indicator bar on the left (only if active in group)
    if (m_showIndicator) {
        QRectF barRect(margin, margin + 4, barWidth, h - margin * 2 - 8);
        QPainterPath barPath;
        barPath.addRoundedRect(barRect, 2, 2);
        painter.fillPath(barPath, contextColor());
    }

    // Text setup
    QFont labelFont = font();
    labelFont.setPixelSize(K4Styles::Dimensions::FontSizeLarge);
    labelFont.setBold(true);

    QFont valueFont = font();
    valueFont.setPixelSize(K4Styles::Dimensions::FontSizeButton);
    valueFont.setBold(true);

    QFont altFont = font();
    altFont.setPixelSize(K4Styles::Dimensions::FontSizeNormal);

    int textLeft = buttonLeft + 6;
    int textRight = w - 6;

    // Calculate vertical positions using font metrics for cross-platform consistency
    // Button height is divided into upper (primary) and lower (alt) regions
    QFontMetrics fmLabel(labelFont);
    QFontMetrics fmValue(valueFont);
    QFontMetrics fmAlt(altFont);

    int topMargin = 4;
    int bottomMargin = 4;

    // Primary baseline: top margin + ascent (vertically centered in upper half)
    int primaryBaseline = topMargin + qMax(fmLabel.ascent(), fmValue.ascent());

    // Alt baseline: button height - bottom margin - descent
    int altBaseline = h - bottomMargin - fmAlt.descent();

    // Primary label (left side, white) - e.g., "WPM"
    painter.setFont(labelFont);
    painter.setPen(QColor(K4Styles::Colors::TextWhite));
    painter.drawText(textLeft, primaryBaseline, m_primaryLabel);

    // Primary value (right side, white) - e.g., "15"
    painter.setFont(valueFont);
    int valueWidth = fmValue.horizontalAdvance(m_primaryValue);
    painter.drawText(textRight - valueWidth, primaryBaseline, m_primaryValue);

    // Alternate label and value (bottom, yellow/amber) - e.g., "PTCH 600"
    painter.setFont(altFont);
    painter.setPen(QColor(K4Styles::Colors::AccentAmber));

    QString altText = m_alternateLabel;
    if (!m_alternateValue.isEmpty()) {
        altText += " " + m_alternateValue;
    }
    painter.drawText(textLeft, altBaseline, altText);
}

void DualControlButton::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_pressPosition = event->pos();
        m_lastDragY = event->pos().y();
        m_dragging = false;
        m_longPressHandled = false;
        if (K4Styles::isCompactLayout())
            m_longPressTimer->start();
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void DualControlButton::mouseMoveEvent(QMouseEvent *event) {
    if (!(event->buttons() & Qt::LeftButton)) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    constexpr int pixelsPerStep = 14;
    const int totalY = event->pos().y() - m_pressPosition.y();
    if (!m_dragging && qAbs(totalY) < pixelsPerStep)
        return;

    if (!m_dragging) {
        m_dragging = true;
        m_longPressTimer->stop();
        if (!m_showIndicator)
            emit becameActive();
    }

    const int deltaY = event->pos().y() - m_lastDragY;
    const int steps = -deltaY / pixelsPerStep; // Drag up = increase.
    if (steps != 0) {
        emit valueScrolled(steps);
        m_lastDragY += (-steps * pixelsPerStep);
    }
    event->accept();
}

void DualControlButton::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_longPressTimer->stop();
        if (m_longPressHandled) {
            event->accept();
            return;
        }
        if (!m_dragging) {
            if (!m_showIndicator) {
                emit becameActive();
            } else if (!K4Styles::isCompactLayout()) {
                swapFunctions();
                emit swapped();
            }
            emit clicked();
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void DualControlButton::wheelEvent(QWheelEvent *event) {
    int steps = m_wheelAccumulator.accumulate(event);
    if (steps != 0) {
        if (!m_showIndicator)
            emit becameActive();
        emit valueScrolled(steps);
    }
    event->accept();
}

void DualControlButton::enterEvent(QEnterEvent *event) {
    m_isHovered = true;
    update();
    QWidget::enterEvent(event);
}

void DualControlButton::leaveEvent(QEvent *event) {
    m_isHovered = false;
    update();
    QWidget::leaveEvent(event);
}
