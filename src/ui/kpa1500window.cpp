#include "kpa1500window.h"
#include "kpa1500panel.h"
#include "k4styles.h"
#include "../settings/radiosettings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

namespace {
const int TitleBarHeight = 28;
const int BorderWidth = 2;
const int CloseButtonSize = 20;
} // namespace

KPA1500Window::KPA1500Window(QWidget *parent)
#ifdef Q_OS_ANDROID
    : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);
#else
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) {
    setAttribute(Qt::WA_TranslucentBackground);
#endif
    setupUi();
    restorePosition();
}

void KPA1500Window::setupUi() {
    // Main layout
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(BorderWidth, BorderWidth, BorderWidth, BorderWidth);
    mainLayout->setSpacing(0);

    // Title bar
    auto *titleBar = new QWidget(this);
    titleBar->setFixedHeight(TitleBarHeight);
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(8, 4, 4, 4);
    titleLayout->setSpacing(4);

    // Title label
    auto *titleLabel = new QLabel("KPA1500", titleBar);
    titleLabel->setStyleSheet(
        QString("QLabel { color: %1; font-size: 11px; font-weight: bold; }").arg(K4Styles::Colors::AccentAmber));

    // Close button
    m_closeBtn = new QPushButton(titleBar);
    m_closeBtn->setFixedSize(CloseButtonSize, CloseButtonSize);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(QString("QPushButton { "
                                      "  background-color: transparent; "
                                      "  color: %1; "
                                      "  border: none; "
                                      "  font-size: %3px; "
                                      "  font-weight: bold; "
                                      "} "
                                      "QPushButton:hover { "
                                      "  background-color: %2; "
                                      "  border-radius: 3px; "
                                      "}")
                                  .arg(K4Styles::Colors::TextGray)
                                  .arg(K4Styles::Colors::BorderNormal)
                                  .arg(K4Styles::Dimensions::FontSizePopup));
    m_closeBtn->setText(QString::fromUtf8("\u00D7")); // × symbol

    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        hide();
        emit closeRequested();
    });

    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_closeBtn);

    // KPA1500 Panel content
    m_panel = new KPA1500Panel(this);

    mainLayout->addWidget(titleBar);
    mainLayout->addWidget(m_panel);

    // Set fixed size based on panel + title bar + borders
    setFixedSize(m_panel->width() + 2 * BorderWidth, m_panel->height() + TitleBarHeight + 2 * BorderWidth);
}

void KPA1500Window::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor bgColor(K4Styles::Colors::Background);
    QColor borderColor(K4Styles::Colors::BorderNormal);

    // Background with rounded corners
    QPainterPath path;
    path.addRoundedRect(QRectF(1, 1, width() - 2, height() - 2), 6, 6);
    painter.fillPath(path, bgColor);

    // Border
    painter.setPen(QPen(borderColor, 1));
    painter.drawPath(path);

    // Title bar separator line
    painter.setPen(QPen(borderColor, 1));
    painter.drawLine(BorderWidth, TitleBarHeight + BorderWidth, width() - BorderWidth, TitleBarHeight + BorderWidth);
}

QRect KPA1500Window::titleBarRect() const {
    return QRect(BorderWidth, BorderWidth, width() - 2 * BorderWidth, TitleBarHeight);
}

void KPA1500Window::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && titleBarRect().contains(event->pos())) {
        m_dragging = true;
#ifdef Q_OS_ANDROID
        m_dragPosition = event->position().toPoint();
#else
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
#endif
    }
    QWidget::mousePressEvent(event);
}

void KPA1500Window::mouseMoveEvent(QMouseEvent *event) {
    if (m_dragging) {
#ifdef Q_OS_ANDROID
        const QPoint parentPoint = parentWidget()->mapFromGlobal(event->globalPosition().toPoint());
        move(parentPoint - m_dragPosition);
#else
        move(event->globalPosition().toPoint() - m_dragPosition);
#endif
    } else {
        // Update cursor for title bar
        if (titleBarRect().contains(event->pos())) {
            setCursor(Qt::SizeAllCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    }
    QWidget::mouseMoveEvent(event);
}

void KPA1500Window::mouseReleaseEvent(QMouseEvent *event) {
    if (m_dragging) {
        m_dragging = false;
        savePosition();
    }
    QWidget::mouseReleaseEvent(event);
}

void KPA1500Window::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    restorePosition();
}

void KPA1500Window::hideEvent(QHideEvent *event) {
    savePosition();
    QWidget::hideEvent(event);
}

void KPA1500Window::savePosition() {
    RadioSettings::instance()->setKpa1500WindowPosition(pos());
}

void KPA1500Window::restorePosition() {
    QPoint savedPos = RadioSettings::instance()->kpa1500WindowPosition();
    if (!savedPos.isNull()) {
        move(savedPos);
    }
}
