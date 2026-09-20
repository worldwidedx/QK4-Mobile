#include "notificationwidget.h"
#include "k4styles.h"
#include <QPainter>
#include <QFontMetrics>
#include <QEvent>

namespace {
const QString BackgroundColor = "#2a2a2a";
const QString BorderColor = "#FFB000";
const QString TextColor = "#FFFFFF";
const int Padding = 20;
const int BorderRadius = 8;
const int BorderWidth = 2;
} // namespace

NotificationWidget::NotificationWidget(QWidget *parent) : QWidget(parent), m_timer(new QTimer(this)) {
    // Create label for message text
    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setStyleSheet(QString("QLabel { color: %1; background: transparent; }").arg(TextColor));

    QFont font = m_label->font();
    font.setPixelSize(K4Styles::Dimensions::FontSizePopup);
    font.setBold(true);
    m_label->setFont(font);

    // Setup timer for auto-dismiss
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &NotificationWidget::onTimeout);

    // Install event filter on parent to track resize
    if (parent) {
        parent->installEventFilter(this);
    }

    // Initially hidden
    hide();

    // Make sure we're above other widgets
    raise();
}

void NotificationWidget::showMessage(const QString &message, int durationMs) {
    m_label->setText(message);

    // Calculate size based on text
    QFontMetrics fm(m_label->font());
    int textWidth = fm.horizontalAdvance(message);
    int textHeight = fm.height();

    int width = textWidth + (Padding * 2);
    int height = textHeight + (Padding * 2);

    // Minimum size
    width = qMax(width, 200);
    height = qMax(height, 50);

    setFixedSize(width, height);
    m_label->setGeometry(Padding, Padding, width - (Padding * 2), height - (Padding * 2));

    // Position in center of parent
    updatePosition();

    // Show and start timer
    show();
    raise();
    m_timer->start(durationMs);
}

void NotificationWidget::updatePosition() {
    if (parentWidget()) {
        QPoint center = parentWidget()->rect().center();
        move(center.x() - width() / 2, center.y() - height() / 2);
    }
}

void NotificationWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw rounded rectangle background
    QRect bgRect = rect().adjusted(BorderWidth / 2, BorderWidth / 2, -BorderWidth / 2, -BorderWidth / 2);

    // Background fill
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(BackgroundColor));
    painter.drawRoundedRect(bgRect, BorderRadius, BorderRadius);

    // Border
    painter.setPen(QPen(QColor(BorderColor), BorderWidth));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(bgRect, BorderRadius, BorderRadius);
}

bool NotificationWidget::eventFilter(QObject *obj, QEvent *event) {
    if (obj == parentWidget() && event->type() == QEvent::Resize) {
        if (isVisible()) {
            updatePosition();
        }
    }
    return QWidget::eventFilter(obj, event);
}

void NotificationWidget::onTimeout() {
    hide();
}
