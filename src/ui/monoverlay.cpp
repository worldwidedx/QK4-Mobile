#include "monoverlay.h"
#include "k4styles.h"
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QFont>

MonOverlay::MonOverlay(QWidget *parent) : SideControlOverlay(Global, parent) {
    setupUi();
}

void MonOverlay::setupUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(IndicatorBarWidth + 8, 8, 8, 8);
    layout->setSpacing(0);

    // Title "MON"
    m_titleLabel = new QLabel("MON", this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPixelSize(K4Styles::Dimensions::FontSizeButton);
    titleFont.setBold(false);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet(QString("color: %1;").arg(K4Styles::Colors::TextWhite));
    layout->addWidget(m_titleLabel);

    // Subtitle "LEVEL"
    m_subtitleLabel = new QLabel("LEVEL", this);
    QFont subtitleFont = m_subtitleLabel->font();
    subtitleFont.setPixelSize(K4Styles::Dimensions::FontSizeButton);
    subtitleFont.setBold(false);
    m_subtitleLabel->setFont(subtitleFont);
    m_subtitleLabel->setStyleSheet(QString("color: %1;").arg(K4Styles::Colors::TextWhite));
    layout->addWidget(m_subtitleLabel);

    // Spacer
    layout->addStretch();

    // Value display
    m_valueLabel = new QLabel("0", this);
    QFont valueFont = m_valueLabel->font();
    valueFont.setPixelSize(K4Styles::Dimensions::FontSizeButton);
    valueFont.setBold(false);
    m_valueLabel->setFont(valueFont);
    m_valueLabel->setStyleSheet(QString("color: %1;").arg(K4Styles::Colors::TextWhite));
    layout->addWidget(m_valueLabel);
}

void MonOverlay::setValue(int value) {
    m_value = qBound(0, value, 100);
    updateValueDisplay();
}

void MonOverlay::setMode(int mode) {
    m_mode = qBound(0, mode, 2);
}

void MonOverlay::updateValueDisplay() {
    m_valueLabel->setText(QString::number(m_value));
}

void MonOverlay::wheelEvent(QWheelEvent *event) {
    int steps = m_wheelAccumulator.accumulate(event);
    if (steps != 0) {
        int newValue = qBound(0, m_value + steps, 100);
        if (newValue != m_value) {
            m_value = newValue;
            updateValueDisplay();
            emit levelChangeRequested(m_mode, m_value);
        }
    }
    event->accept();
}

void MonOverlay::mousePressEvent(QMouseEvent *event) {
    m_dragActive = true;
    m_dragMoved = false;
    m_dragStartX = event->position().x();
    m_dragStartY = event->position().y();
    event->accept();
}

void MonOverlay::mouseMoveEvent(QMouseEvent *event) {
    if (!m_dragActive)
        return;
    const qreal x = event->position().x();
    const qreal y = event->position().y();
    if (!m_dragMoved && (qAbs(y - m_dragStartY) > 4 || qAbs(x - m_dragStartX) > 4))
        m_dragMoved = true;
    if (m_dragMoved) {
        // Top of the overlay is 100, bottom is 0 (vertical slider feel).
        const qreal h = qMax(1, height());
        const qreal frac = 1.0 - qBound(0.0, y, h) / h;
        const int newValue = qBound(0, int(qRound(frac * 100.0)), 100);
        if (newValue != m_value) {
            m_value = newValue;
            updateValueDisplay();
            emit levelChangeRequested(m_mode, m_value);
        }
    }
    event->accept();
}

void MonOverlay::mouseReleaseEvent(QMouseEvent *event) {
    // A tap (press with no drag) dismisses the overlay; a drag adjusted it.
    if (m_dragActive && !m_dragMoved)
        hide();
    m_dragActive = false;
    event->accept();
}
