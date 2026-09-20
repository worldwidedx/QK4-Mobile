#include "micmeterwidget.h"
#include "k4styles.h"
#include <QPainter>
#include <QLinearGradient>

MicMeterWidget::MicMeterWidget(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(20);
    setMaximumHeight(30);
}

QSize MicMeterWidget::sizeHint() const {
    return QSize(200, 24);
}

QSize MicMeterWidget::minimumSizeHint() const {
    return QSize(100, 20);
}

void MicMeterWidget::setLevel(float level) {
    m_currentLevel = qBound(0.0f, level, 1.0f);

    // Peak hold logic
    if (m_currentLevel >= m_peakLevel) {
        m_peakLevel = m_currentLevel;
        m_peakHoldFrames = PEAK_HOLD_FRAMES;
    } else {
        if (m_peakHoldFrames > 0) {
            m_peakHoldFrames--;
        } else {
            // Decay peak level
            m_peakLevel = qMax(m_currentLevel, m_peakLevel - 0.02f);
        }
    }

    update();
}

void MicMeterWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect r = rect().adjusted(1, 1, -1, -1);

    // Draw background
    painter.fillRect(r, QColor(K4Styles::Colors::Background));

    // Draw border
    painter.setPen(QPen(QColor(K4Styles::Colors::TextDark), 1));
    painter.drawRect(r);

    // Calculate meter width based on level
    int meterWidth = static_cast<int>(r.width() * m_currentLevel);
    if (meterWidth < 1 && m_currentLevel > 0) {
        meterWidth = 1;
    }

    if (meterWidth > 0) {
        QRect meterRect(r.left() + 1, r.top() + 1, meterWidth, r.height() - 2);

        // Use standard meter gradient
        QLinearGradient gradient = K4Styles::meterGradient(meterRect.left(), 0, meterRect.right(), 0);

        painter.fillRect(meterRect, gradient);
    }

    // Draw peak indicator line
    if (m_peakLevel > 0.01f) {
        int peakX = r.left() + static_cast<int>(r.width() * m_peakLevel);
        peakX = qMin(peakX, r.right() - 1);

        // Peak color based on level
        QColor peakColor;
        if (m_peakLevel > 0.8f) {
            peakColor = QColor(K4Styles::Colors::MeterRed);
        } else if (m_peakLevel > 0.6f) {
            peakColor = QColor(K4Styles::Colors::MeterYellow);
        } else {
            peakColor = QColor(K4Styles::Colors::MeterGreen);
        }

        painter.setPen(QPen(peakColor, 2));
        painter.drawLine(peakX, r.top() + 2, peakX, r.bottom() - 2);
    }

    // Draw level markers (vertical lines at 25%, 50%, 75%)
    painter.setPen(QPen(QColor(K4Styles::Colors::DisabledBackground), 1));
    for (float mark : {0.25f, 0.5f, 0.75f}) {
        int x = r.left() + static_cast<int>(r.width() * mark);
        painter.drawLine(x, r.top() + 2, x, r.bottom() - 2);
    }
}
