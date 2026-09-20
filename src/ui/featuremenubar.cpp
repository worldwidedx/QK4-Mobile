#include "featuremenubar.h"
#include "k4styles.h"
#include "inwindowpopup.h"
#include <QHBoxLayout>
#include <QPainter>
#include <QApplication>
#include <QScreen>
#include <QHideEvent>
#include <QKeyEvent>
#include <QWheelEvent>

namespace {
const int ContentHeight = 52; // Height of content area (was setFixedHeight)
const int ContentMargin = 12; // Horizontal margin inside content
} // namespace

FeatureMenuBar::FeatureMenuBar(QWidget *parent) : QWidget(parent) {
    InWindowPopup::configure(this);
    setupUi();
    hide(); // Hidden by default
}

void FeatureMenuBar::setupUi() {
    // Height includes shadow margins on top and bottom
    setFixedHeight(ContentHeight + 2 * K4Styles::Dimensions::ShadowMargin);

    auto *layout = new QHBoxLayout(this);
    // Margins include shadow space on all sides
    layout->setContentsMargins(
        K4Styles::Dimensions::ShadowMargin + ContentMargin, K4Styles::Dimensions::ShadowMargin + 6,
        K4Styles::Dimensions::ShadowMargin + ContentMargin, K4Styles::Dimensions::ShadowMargin + 6);
    layout->setSpacing(8);

    // Title label (framed box with centered text)
    m_titleLabel = new QLabel("ATTENUATOR", this);
    m_titleLabel->setFixedSize(140, 36);
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

    // OFF/ON toggle button
    m_toggleBtn = new QPushButton("OFF", this);
    m_toggleBtn->setMinimumWidth(60);
    m_toggleBtn->setFixedHeight(K4Styles::Dimensions::ButtonHeightMedium);
    m_toggleBtn->setCursor(Qt::PointingHandCursor);
    m_toggleBtn->setStyleSheet(K4Styles::menuBarButton());

    // NR has two mutually-exclusive engines. Make the selected engine
    // visible and switchable without requiring a mouse/keyboard gesture.
    m_nrEngineBtn = new QPushButton("LMS", this);
    m_nrEngineBtn->setMinimumWidth(60);
    m_nrEngineBtn->setFixedHeight(K4Styles::Dimensions::ButtonHeightMedium);
    m_nrEngineBtn->setCursor(Qt::PointingHandCursor);
    m_nrEngineBtn->setStyleSheet(K4Styles::menuBarButton());
    m_nrEngineBtn->hide();

    // Extra button (only shown for NB LEVEL - "FILTER NONE/NARROW/WIDE")
    // Uses ButtonHeightLarge (44px) to fit two lines of text
    m_extraBtn = new QPushButton("FILTER\nNONE", this);
    m_extraBtn->setMinimumWidth(90);
    m_extraBtn->setFixedHeight(K4Styles::Dimensions::ButtonHeightLarge);
    m_extraBtn->setCursor(Qt::PointingHandCursor);
    m_extraBtn->setStyleSheet(K4Styles::menuBarButton());
    m_extraBtn->hide(); // Hidden by default

    // Value label (center, bold)
    m_valueLabel = new QLabel("0", this);
    m_valueLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: 600;")
                                    .arg(K4Styles::Colors::TextWhite)
                                    .arg(K4Styles::Dimensions::PopupValueSize));
    m_valueLabel->setAlignment(Qt::AlignCenter);
    m_valueLabel->setMinimumWidth(80);

    // Decrement button
    m_decrementBtn = new QPushButton("-", this);
    m_decrementBtn->setFixedSize(K4Styles::Dimensions::ButtonHeightLarge, K4Styles::Dimensions::ButtonHeightMedium);
    m_decrementBtn->setCursor(Qt::PointingHandCursor);
    m_decrementBtn->setStyleSheet(K4Styles::menuBarButtonSmall());

    // Increment button
    m_incrementBtn = new QPushButton("+", this);
    m_incrementBtn->setFixedSize(K4Styles::Dimensions::ButtonHeightLarge, K4Styles::Dimensions::ButtonHeightMedium);
    m_incrementBtn->setCursor(Qt::PointingHandCursor);
    m_incrementBtn->setStyleSheet(K4Styles::menuBarButtonSmall());

    // Match the return control used by the other phone adjustment popups.
    m_closeBtn = new QPushButton("\u21A9", this); // ↩
    m_closeBtn->setFixedSize(K4Styles::Dimensions::NavButtonWidth,
                             K4Styles::Dimensions::ButtonHeightMedium);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(K4Styles::menuBarButton());

    // Layout - compact, no stretches (popup is centered by showAboveWidget)
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_toggleBtn);
    layout->addWidget(m_nrEngineBtn);
    layout->addWidget(m_extraBtn);
    layout->addWidget(m_valueLabel);
    layout->addWidget(m_decrementBtn);
    layout->addWidget(m_incrementBtn);
    layout->addWidget(m_closeBtn);

    // Connect signals
    connect(m_toggleBtn, &QPushButton::clicked, this, &FeatureMenuBar::toggleRequested);
    connect(m_nrEngineBtn, &QPushButton::clicked, this, &FeatureMenuBar::nrEngineToggleRequested);
    connect(m_decrementBtn, &QPushButton::clicked, this, &FeatureMenuBar::decrementRequested);
    connect(m_incrementBtn, &QPushButton::clicked, this, &FeatureMenuBar::incrementRequested);
    connect(m_extraBtn, &QPushButton::clicked, this, &FeatureMenuBar::extraButtonClicked);
    connect(m_closeBtn, &QPushButton::clicked, this, &FeatureMenuBar::hideMenu);
}

void FeatureMenuBar::showForFeature(Feature feature) {
    m_currentFeature = feature;
    updateForFeature();
    // Force layout recalculation before showing (extra button changes width)
    layout()->activate();
    adjustSize(); // Calculate proper size for content

    // Position above reference widget if set
    if (m_referenceWidget) {
        showAboveWidget(m_referenceWidget);
    } else {
        update();
        show();
        setFocus();
    }
}

void FeatureMenuBar::showAboveWidget(QWidget *referenceWidget) {
    if (!referenceWidget)
        return;

    m_referenceWidget = referenceWidget;

    // Force size calculation
    layout()->activate();
    adjustSize();

    // Get the reference widget's global position
    QPoint refGlobal = referenceWidget->mapToGlobal(QPoint(0, 0));
    int refCenterX = refGlobal.x() + referenceWidget->width() / 2;

    // Content width (widget width minus shadow margins)
    int contentWidth = width() - 2 * K4Styles::Dimensions::ShadowMargin;

    // Center content area horizontally above reference widget (account for shadow margin)
    int popupX = refCenterX - contentWidth / 2 - K4Styles::Dimensions::ShadowMargin;
    // Position popup so its bottom edge (including shadow margin) is 4px above the reference widget
    int popupY = refGlobal.y() - height() - 4;

    // Ensure popup stays on screen
    QRect screenGeom = referenceWidget->screen()->availableGeometry();
    if (popupX < screenGeom.left() - K4Styles::Dimensions::ShadowMargin) {
        popupX = screenGeom.left() - K4Styles::Dimensions::ShadowMargin;
    } else if (popupX + width() > screenGeom.right() + K4Styles::Dimensions::ShadowMargin) {
        popupX = screenGeom.right() + K4Styles::Dimensions::ShadowMargin - width();
    }
    if (popupY < screenGeom.top() - K4Styles::Dimensions::ShadowMargin) {
        // If not enough room above, show below instead
        popupY = refGlobal.y() + referenceWidget->height() + 4 - K4Styles::Dimensions::ShadowMargin;
    }

    InWindowPopup::moveFromGlobal(this, QPoint(popupX, popupY));
    show();
    setFocus();
    update();
}

void FeatureMenuBar::hideMenu() {
    hide();
    // closed() signal is emitted by hideEvent()
}

void FeatureMenuBar::hideEvent(QHideEvent *event) {
    QWidget::hideEvent(event);
    emit closed();
}

void FeatureMenuBar::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        hideMenu();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void FeatureMenuBar::wheelEvent(QWheelEvent *event) {
    int steps = m_wheelAccumulator.accumulate(event);
    for (int i = 0; i < qAbs(steps); ++i) {
        if (steps > 0)
            emit incrementRequested();
        else
            emit decrementRequested();
    }
    event->accept();
}

void FeatureMenuBar::updateForFeature() {
    switch (m_currentFeature) {
    case Attenuator:
        m_titleLabel->setText("ATTENUATOR");
        m_extraBtn->hide();
        m_nrEngineBtn->hide();
        m_valueUnit = " dB";
        break;
    case NbLevel:
        m_titleLabel->setText("NB LEVEL");
        m_extraBtn->setText("FILTER\nNONE");
        m_extraBtn->show();
        m_nrEngineBtn->hide();
        m_valueUnit = "";
        break;
    case NrAdjust:
        m_titleLabel->setText("NR ADJUST");
        m_extraBtn->hide();
        m_nrEngineBtn->setText(m_nrEngine == Ssnr ? "SSNR" : "LMS");
        m_nrEngineBtn->show();
        m_valueUnit = "";
        break;
    case ManualNotch:
        m_titleLabel->setText("MANUAL NOTCH");
        m_extraBtn->hide();
        m_nrEngineBtn->hide();
        m_valueUnit = " Hz";
        break;
    }

    // Update value display with unit
    setValue(m_value);
}

void FeatureMenuBar::setNrEngine(NrEngine engine) {
    m_nrEngine = engine;
    if (m_currentFeature == NrAdjust)
        m_nrEngineBtn->setText(m_nrEngine == Ssnr ? "SSNR" : "LMS");
}

void FeatureMenuBar::setFeatureEnabled(bool enabled) {
    m_featureEnabled = enabled;
    m_toggleBtn->setText(enabled ? "ON" : "OFF");
}

void FeatureMenuBar::setValue(int value) {
    m_value = value;
    m_valueLabel->setText(QString::number(value) + m_valueUnit);
}

void FeatureMenuBar::setValueUnit(const QString &unit) {
    m_valueUnit = unit;
    setValue(m_value); // Refresh display
}

void FeatureMenuBar::setNbFilter(int filter) {
    m_nbFilter = qBound(0, filter, 2);
    // Update extra button text to show current filter
    static const char *filterNames[] = {"FILTER\nNONE", "FILTER\nNARROW", "FILTER\nWIDE"};
    m_extraBtn->setText(filterNames[m_nbFilter]);
}

void FeatureMenuBar::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Calculate tight bounding box from first to last visible widget
    int left = m_titleLabel->geometry().left() - 8; // 8px padding
    int right = m_closeBtn->geometry().right() + 8;
    QRect contentRect(left, K4Styles::Dimensions::ShadowMargin + 1, right - left, ContentHeight - 3);

    // Draw drop shadow
    K4Styles::drawDropShadow(painter, contentRect, 8);

    // Gradient background (matches ControlGroupWidget style)
    QLinearGradient grad = K4Styles::buttonGradient(contentRect.top(), contentRect.bottom());

    // Draw rounded rectangle with border around content area only
    painter.setBrush(grad);
    painter.setPen(QPen(K4Styles::borderColor(), 1));
    painter.drawRoundedRect(contentRect, 8, 8);

    // Draw vertical delimiter lines between widget groups
    painter.setPen(QPen(K4Styles::borderColor(), 1));
    int lineTop = contentRect.top() + 7;
    int lineBottom = contentRect.bottom() - 7;

    // Helper to draw delimiter after a widget
    auto drawDelimiter = [&](QWidget *widget) {
        if (widget && widget->isVisible()) {
            int x = widget->geometry().right() + 4; // 4px after widget (half of spacing)
            painter.drawLine(x, lineTop, x, lineBottom);
        }
    };

    // Draw delimiters after each section
    drawDelimiter(m_titleLabel); // After title
    drawDelimiter(m_toggleBtn);  // After OFF/ON
    drawDelimiter(m_nrEngineBtn); // After LMS/SSNR (if shown)
    if (m_extraBtn->isVisible()) {
        drawDelimiter(m_extraBtn); // After FILTER NONE (if shown)
    }
    drawDelimiter(m_valueLabel); // After value
    drawDelimiter(m_incrementBtn); // Before the return control
    // No delimiter after the return control (it is at the end).
}
