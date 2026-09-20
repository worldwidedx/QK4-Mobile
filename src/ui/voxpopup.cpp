#include "voxpopup.h"
#include "k4styles.h"
#include "inwindowpopup.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QWheelEvent>

namespace {
const int ContentHeight = 52;
const int ContentMargin = 12;
const int MaxLevel = 60;
const int TitleWidthVoxGain = 160; // "VOX GAIN, VOICE" or "VOX GAIN, DATA"
const int TitleWidthAntiVox = 110; // "ANTI-VOX"
} // namespace

VoxPopupWidget::VoxPopupWidget(QWidget *parent) : QWidget(parent) {
    InWindowPopup::configure(this);
    setupUi();
    hide();
}

void VoxPopupWidget::setupUi() {
    setFixedHeight(ContentHeight + 2 * K4Styles::Dimensions::ShadowMargin);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(
        K4Styles::Dimensions::ShadowMargin + ContentMargin, K4Styles::Dimensions::ShadowMargin + 6,
        K4Styles::Dimensions::ShadowMargin + ContentMargin, K4Styles::Dimensions::ShadowMargin + 6);
    layout->setSpacing(6);

    // Title label - will be updated based on mode
    m_titleLabel = new QLabel("VOX GAIN, VOICE", this);
    m_titleLabel->setFixedSize(TitleWidthVoxGain, K4Styles::Dimensions::ButtonHeightMedium);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet(QString("QLabel {"
                                        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
                                        "    stop:0 %1, stop:0.4 %2, stop:0.6 %3, stop:1 %4);"
                                        "  color: %5;"
                                        "  border: %6px solid %7;"
                                        "  border-radius: %8px;"
                                        "  font-size: %9px;"
                                        "  font-weight: 600;"
                                        "}")
                                    .arg(K4Styles::Colors::GradientTop)
                                    .arg(K4Styles::Colors::GradientMid1)
                                    .arg(K4Styles::Colors::GradientMid2)
                                    .arg(K4Styles::Colors::GradientBottom)
                                    .arg(K4Styles::Colors::TextWhite)
                                    .arg(K4Styles::Dimensions::BorderWidth)
                                    .arg(K4Styles::Colors::BorderNormal)
                                    .arg(K4Styles::Dimensions::BorderRadius)
                                    .arg(K4Styles::Dimensions::PopupTitleSize));

    // VOX toggle button
    m_voxBtn = new QPushButton("VOX\nOFF", this);
    m_voxBtn->setFixedSize(K4Styles::Dimensions::PopupButtonWidth, K4Styles::Dimensions::ButtonHeightMedium);
    m_voxBtn->setCursor(Qt::PointingHandCursor);
    m_voxBtn->setStyleSheet(K4Styles::popupButtonNormal());

    // Value display label
    m_valueLabel = new QLabel(QString::number(m_value), this);
    m_valueLabel->setFixedSize(K4Styles::Dimensions::NavButtonWidth, K4Styles::Dimensions::ButtonHeightMedium);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    m_valueLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 600;")
                                    .arg(K4Styles::Colors::TextWhite)
                                    .arg(K4Styles::Dimensions::PopupValueSize));

    // Decrement button
    m_decrementBtn = new QPushButton("-", this);
    m_decrementBtn->setFixedSize(K4Styles::Dimensions::NavButtonWidth, K4Styles::Dimensions::ButtonHeightMedium);
    m_decrementBtn->setCursor(Qt::PointingHandCursor);
    m_decrementBtn->setStyleSheet(K4Styles::menuBarButtonSmall());

    // Increment button
    m_incrementBtn = new QPushButton("+", this);
    m_incrementBtn->setFixedSize(K4Styles::Dimensions::NavButtonWidth, K4Styles::Dimensions::ButtonHeightMedium);
    m_incrementBtn->setCursor(Qt::PointingHandCursor);
    m_incrementBtn->setStyleSheet(K4Styles::menuBarButtonSmall());

    // Close button
    m_closeBtn = new QPushButton("\u21A9", this); // U+21A9 leftwards arrow with hook
    m_closeBtn->setFixedSize(K4Styles::Dimensions::NavButtonWidth, K4Styles::Dimensions::ButtonHeightMedium);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(K4Styles::menuBarButton());

    // Add to layout
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_voxBtn);
    layout->addWidget(m_valueLabel);
    layout->addWidget(m_decrementBtn);
    layout->addWidget(m_incrementBtn);
    layout->addWidget(m_closeBtn);

    // Connect signals
    connect(m_voxBtn, &QPushButton::clicked, this, [this]() {
        m_voxEnabled = !m_voxEnabled;
        updateVoxButton();
        emit voxToggled(m_voxEnabled);
    });

    connect(m_decrementBtn, &QPushButton::clicked, this, [this]() {
        // Shift held = adjust by 10, otherwise by 1
        int delta = (QApplication::keyboardModifiers() & Qt::ShiftModifier) ? -10 : -1;
        adjustValue(delta);
    });

    connect(m_incrementBtn, &QPushButton::clicked, this, [this]() {
        // Shift held = adjust by 10, otherwise by 1
        int delta = (QApplication::keyboardModifiers() & Qt::ShiftModifier) ? 10 : 1;
        adjustValue(delta);
    });

    connect(m_closeBtn, &QPushButton::clicked, this, &VoxPopupWidget::hidePopup);

    updateTitle();
    updateVoxButton();
    updateValueDisplay();
}

void VoxPopupWidget::setPopupMode(PopupMode mode) {
    if (mode != m_popupMode) {
        m_popupMode = mode;
        updateTitle();
        // Resize to fit new layout
        layout()->activate();
        adjustSize();
    }
}

void VoxPopupWidget::setDataMode(bool isDataMode) {
    if (isDataMode != m_isDataMode) {
        m_isDataMode = isDataMode;
        updateTitle();
    }
}

void VoxPopupWidget::updateTitle() {
    QString title;
    int width;

    if (m_popupMode == VoxGain) {
        title = m_isDataMode ? "VOX GAIN, DATA" : "VOX GAIN, VOICE";
        width = TitleWidthVoxGain;
    } else {
        title = "ANTI-VOX";
        width = TitleWidthAntiVox;
    }

    m_titleLabel->setText(title);
    m_titleLabel->setFixedWidth(width);
}

void VoxPopupWidget::updateVoxButton() {
    m_voxBtn->setText(m_voxEnabled ? "VOX\nON" : "VOX\nOFF");
    m_voxBtn->setStyleSheet(m_voxEnabled ? K4Styles::popupButtonSelected() : K4Styles::popupButtonNormal());
}

void VoxPopupWidget::updateValueDisplay() {
    m_valueLabel->setText(QString::number(m_value));
}

void VoxPopupWidget::adjustValue(int delta) {
    int newValue = qBound(0, m_value + delta, MaxLevel);
    if (newValue != m_value) {
        m_value = newValue;
        updateValueDisplay();
        emit valueChanged(newValue);
    }
}

void VoxPopupWidget::setValue(int value) {
    m_value = qBound(0, value, MaxLevel);
    updateValueDisplay();
}

void VoxPopupWidget::setVoxEnabled(bool enabled) {
    if (enabled != m_voxEnabled) {
        m_voxEnabled = enabled;
        updateVoxButton();
    }
}

void VoxPopupWidget::showAboveWidget(QWidget *referenceWidget) {
    if (!referenceWidget)
        return;

    m_referenceWidget = referenceWidget;

    // Make sure layout is updated for current mode
    updateTitle();

    layout()->activate();
    adjustSize();

    QPoint refGlobal = referenceWidget->mapToGlobal(QPoint(0, 0));
    int refCenterX = refGlobal.x() + referenceWidget->width() / 2;

    int contentWidth = width() - 2 * K4Styles::Dimensions::ShadowMargin;
    int popupX = refCenterX - contentWidth / 2 - K4Styles::Dimensions::ShadowMargin;
    int popupY = refGlobal.y() - height() - 4;

    QRect screenGeom = referenceWidget->screen()->availableGeometry();
    if (popupX < screenGeom.left() - K4Styles::Dimensions::ShadowMargin) {
        popupX = screenGeom.left() - K4Styles::Dimensions::ShadowMargin;
    } else if (popupX + width() > screenGeom.right() + K4Styles::Dimensions::ShadowMargin) {
        popupX = screenGeom.right() + K4Styles::Dimensions::ShadowMargin - width();
    }
    if (popupY < screenGeom.top() - K4Styles::Dimensions::ShadowMargin) {
        popupY = refGlobal.y() + referenceWidget->height() + 4 - K4Styles::Dimensions::ShadowMargin;
    }

    InWindowPopup::moveFromGlobal(this, QPoint(popupX, popupY));
    show();
    setFocus();
    update();
}

void VoxPopupWidget::hidePopup() {
    hide();
}

void VoxPopupWidget::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    emit closed();
}

void VoxPopupWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        hidePopup();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void VoxPopupWidget::wheelEvent(QWheelEvent *event) {
    int steps = m_wheelAccumulator.accumulate(event);
    if (steps != 0)
        adjustValue(steps);
    event->accept();
}

void VoxPopupWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Calculate tight bounding box
    int left = m_titleLabel->geometry().left() - 8;
    int right = m_closeBtn->geometry().right() + 8;
    QRect contentRect(left, K4Styles::Dimensions::ShadowMargin + 1, right - left, ContentHeight - 3);

    // Draw drop shadow
    K4Styles::drawDropShadow(painter, contentRect, 8);

    // Gradient background
    QLinearGradient grad = K4Styles::buttonGradient(contentRect.top(), contentRect.bottom());

    painter.setBrush(grad);
    painter.setPen(QPen(K4Styles::borderColor(), 1));
    painter.drawRoundedRect(contentRect, 8, 8);

    // Draw vertical delimiter lines
    painter.setPen(QPen(K4Styles::borderColor(), 1));
    int lineTop = contentRect.top() + 7;
    int lineBottom = contentRect.bottom() - 7;

    auto drawDelimiter = [&](QWidget *widget) {
        if (widget && widget->isVisible()) {
            int x = widget->geometry().right() + 3;
            painter.drawLine(x, lineTop, x, lineBottom);
        }
    };

    drawDelimiter(m_titleLabel);
    drawDelimiter(m_voxBtn);
    drawDelimiter(m_incrementBtn);
}
