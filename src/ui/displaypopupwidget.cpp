#include "displaypopupwidget.h"
#include "k4styles.h"
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QHideEvent>
#include <QApplication>
#include <QSignalBlocker>
#include <QScreen>

namespace {
// Layout constants
const int MenuButtonWidth = 80;
const int MenuButtonHeight = 44;
const int TopRowHeight = 36;
const int ButtonSpacing = 4;
const int RowSpacing = 4;
const int ContentMargin = 8; // Margin around content
} // namespace

// ============================================================================
// DisplayMenuButton Implementation
// ============================================================================

DisplayMenuButton::DisplayMenuButton(const QString &primaryText, const QString &alternateText, QWidget *parent)
    : QWidget(parent), m_primaryText(primaryText), m_alternateText(alternateText) {
    setFixedSize(MenuButtonWidth, MenuButtonHeight);
    setCursor(Qt::PointingHandCursor);

    // The desktop UI uses a right-click for the amber (alternate) action.
    // On a phone, a deliberate hold is the equivalent gesture.
    m_longPressTimer.setSingleShot(true);
    m_longPressTimer.setInterval(550);
    connect(&m_longPressTimer, &QTimer::timeout, this, [this]() {
        if (m_leftPressed && !m_pressCancelled) {
            m_longPressHandled = true;
            emit rightClicked();
        }
    });
}

void DisplayMenuButton::setSelected(bool selected) {
    if (m_selected != selected) {
        m_selected = selected;
        update();
    }
}

void DisplayMenuButton::setPrimaryText(const QString &text) {
    if (m_primaryText != text) {
        m_primaryText = text;
        update();
    }
}

void DisplayMenuButton::setAlternateText(const QString &text) {
    if (m_alternateText != text) {
        m_alternateText = text;
        update();
    }
}

void DisplayMenuButton::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background - subtle gradient
    QLinearGradient grad = K4Styles::buttonGradient(0, height(), m_hovered);
    painter.setBrush(grad);
    painter.setPen(QPen(K4Styles::borderColor(), 2));
    painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 5, 5);

    // Primary text (white) - top
    QFont primaryFont = font();
    primaryFont.setPixelSize(K4Styles::Dimensions::FontSizeButton);
    primaryFont.setBold(m_selected);
    painter.setFont(primaryFont);
    painter.setPen(Qt::white);

    QRect primaryRect(0, 4, width(), height() / 2 - 2);
    painter.drawText(primaryRect, Qt::AlignCenter, m_primaryText);

    // Alternate text (orange) - bottom
    QFont altFont = font();
    altFont.setPixelSize(K4Styles::Dimensions::FontSizeMedium);
    altFont.setBold(false);
    painter.setFont(altFont);
    painter.setPen(QColor(K4Styles::Colors::AccentAmber));

    QRect altRect(0, height() / 2, width(), height() / 2 - 4);
    painter.drawText(altRect, Qt::AlignCenter, m_alternateText);
}

void DisplayMenuButton::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_pressPosition = event->pos();
        m_leftPressed = true;
        m_longPressHandled = false;
        m_pressCancelled = false;
        m_longPressTimer.start();
    } else if (event->button() == Qt::RightButton) {
        emit rightClicked();
    }
    event->accept();
}

void DisplayMenuButton::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_longPressTimer.stop();
        const bool activate = m_leftPressed && !m_longPressHandled && !m_pressCancelled && rect().contains(event->pos());
        // Phone controls must not require a precise tap on one half of a
        // compact dual-line button. A normal tap always chooses the primary
        // (white) action; the desktop alternate action remains a deliberate
        // long-press, which is how WTR CLRS is reached from NB/WTR CLRS.
        m_leftPressed = false;
        if (activate)
            emit clicked();
    }
    event->accept();
}

void DisplayMenuButton::mouseMoveEvent(QMouseEvent *event) {
    // A finger drag must not turn into an alternate control activation.
    if (m_leftPressed && (event->pos() - m_pressPosition).manhattanLength() > 12) {
        m_pressCancelled = true;
        m_longPressTimer.stop();
    }
    event->accept();
}

void DisplayMenuButton::enterEvent(QEnterEvent *event) {
    Q_UNUSED(event)
    m_hovered = true;
    update();
}

void DisplayMenuButton::leaveEvent(QEvent *event) {
    Q_UNUSED(event)
    if (m_leftPressed) {
        m_pressCancelled = true;
        m_longPressTimer.stop();
    }
    m_hovered = false;
    update();
}

// ============================================================================
// ControlGroupWidget Implementation
// ============================================================================

namespace {
const int ControlGroupHeight = 32;
} // namespace

ControlGroupWidget::ControlGroupWidget(const QString &label, QWidget *parent)
    : QWidget(parent), m_label(label), m_value("100.0") {
    // Fixed width for control group: label + value + - + + with generous spacing
    setFixedSize(180, ControlGroupHeight);
    setCursor(Qt::PointingHandCursor);
}

void ControlGroupWidget::setValue(const QString &value) {
    if (m_value != value) {
        m_value = value;
        update();
    }
}

void ControlGroupWidget::setShowAutoButton(bool show) {
    if (m_showAutoButton != show) {
        m_showAutoButton = show;
        // Resize widget to accommodate AUTO button
        if (show) {
            setFixedSize(220, ControlGroupHeight);
        } else {
            setFixedSize(180, ControlGroupHeight);
        }
        update();
    }
}

void ControlGroupWidget::setCompactLabel(bool compact) {
    if (m_compactLabel == compact)
        return;

    m_compactLabel = compact;
    // AUTO groups need their fixed 220 px form. The compact form is used by
    // NB only and preserves its 32 px plus/minus touch targets.
    if (!m_showAutoButton)
        setFixedSize(m_compactLabel ? 132 : 180, ControlGroupHeight);
    update();
}

void ControlGroupWidget::setAutoEnabled(bool enabled) {
    if (m_autoEnabled != enabled) {
        m_autoEnabled = enabled;
        update();
    }
}

void ControlGroupWidget::setValueFaded(bool faded) {
    if (m_valueFaded != faded) {
        m_valueFaded = faded;
        update();
    }
}

void ControlGroupWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Layout depends on whether AUTO button is shown
    const int labelWidth = m_compactLabel ? 24 : 60; // NB fits the compact form
    const int autoWidth = m_showAutoButton ? 40 : 0;
    const int valueWidth = m_compactLabel ? 40 : 52;
    const int buttonWidth = 32;

    // Draw container background with gradient
    QLinearGradient grad = K4Styles::buttonGradient(0, height());

    // Rounded rectangle with softer corners
    int cornerRadius = 6;
    painter.setBrush(grad);
    painter.setPen(QPen(K4Styles::borderColor(), 2));
    painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), cornerRadius, cornerRadius);

    // Calculate positions
    int x = 4;
    QRect labelRect(x, 2, labelWidth, height() - 4);
    x += labelWidth;

    // Vertical line after label
    painter.drawLine(x, 4, x, height() - 4);

    // AUTO button (if shown)
    if (m_showAutoButton) {
        m_autoRect = QRect(x, 2, autoWidth, height() - 4);
        x += autoWidth;

        // Vertical line after AUTO
        painter.drawLine(x, 4, x, height() - 4);
    }

    m_valueRect = QRect(x, 2, valueWidth, height() - 4);
    x += valueWidth;

    // Vertical line after value
    painter.drawLine(x, 4, x, height() - 4);

    m_minusRect = QRect(x, 2, buttonWidth, height() - 4);
    x += buttonWidth;

    // Vertical line after minus
    painter.drawLine(x, 4, x, height() - 4);

    m_plusRect = QRect(x, 2, buttonWidth, height() - 4);

    // Draw label
    QFont labelFont = font();
    labelFont.setPixelSize(K4Styles::Dimensions::FontSizeLarge);
    labelFont.setBold(true);
    painter.setFont(labelFont);
    painter.setPen(Qt::white);
    painter.drawText(labelRect, Qt::AlignCenter, m_label);

    // Draw AUTO button if shown
    if (m_showAutoButton) {
        if (m_autoEnabled) {
            painter.fillRect(m_autoRect, Qt::white);
            painter.setPen(QColor(K4Styles::Colors::InactiveGray));
        } else {
            painter.setPen(Qt::white);
        }
        QFont autoFont = font();
        autoFont.setPixelSize(K4Styles::Dimensions::FontSizeMedium);
        autoFont.setBold(true);
        painter.setFont(autoFont);
        painter.drawText(m_autoRect, Qt::AlignCenter, m_autoEnabled ? "AUTO" : "MAN");
        painter.setFont(labelFont);
        painter.setPen(Qt::white);
    }

    // Draw value (faded grey when auto mode)
    if (m_valueFaded) {
        painter.setPen(QColor(K4Styles::Colors::TextFaded));
    } else {
        painter.setPen(Qt::white);
    }
    painter.drawText(m_valueRect, Qt::AlignCenter, m_value);
    painter.setPen(Qt::white); // Reset for buttons

    // Draw minus button with larger font
    QFont buttonFont = font();
    buttonFont.setPixelSize(K4Styles::Dimensions::FontSizeTitle);
    buttonFont.setBold(true);
    painter.setFont(buttonFont);
    painter.drawText(m_minusRect, Qt::AlignCenter, "-");

    // Draw plus button
    painter.drawText(m_plusRect, Qt::AlignCenter, "+");
}

void ControlGroupWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        QPoint pos = event->pos();
        if (m_showAutoButton && m_autoRect.contains(pos)) {
            emit autoClicked();
        } else if (m_valueRect.contains(pos)) {
            emit valueClicked();
        } else if (m_minusRect.contains(pos)) {
            emit decrementClicked();
        } else if (m_plusRect.contains(pos)) {
            emit incrementClicked();
        }
    }
}

void ControlGroupWidget::wheelEvent(QWheelEvent *event) {
    int steps = m_wheelAccumulator.accumulate(event);
    for (int i = 0; i < qAbs(steps); ++i) {
        if (steps > 0)
            emit incrementClicked();
        else
            emit decrementClicked();
    }
    event->accept();
}

// ============================================================================
// ToggleGroupWidget Implementation
// ============================================================================

namespace {
const int ToggleGroupHeight = 32;
const int TogglePadding = 4;
const int ToggleTriangleWidth = 10; // Triangle pointing to next element
const int ToggleButtonSpacing = 2;  // Gap between buttons
} // namespace

ToggleGroupWidget::ToggleGroupWidget(const QString &leftLabel, const QString &rightLabel, QWidget *parent)
    : QWidget(parent), m_leftLabel(leftLabel), m_rightLabel(rightLabel), m_leftSelected(true), m_rightSelected(false),
      m_rightEnabled(true) {
    // Calculate equal width for all three buttons (left, &, right)
    QFontMetrics fm(font());
    int leftTextWidth = fm.horizontalAdvance(leftLabel) + 16;
    int rightTextWidth = fm.horizontalAdvance(rightLabel) + 16;
    int ampTextWidth = fm.horizontalAdvance("&") + 16;

    // Use the maximum width for all three buttons
    int buttonWidth = qMax(qMax(leftTextWidth, rightTextWidth), ampTextWidth);
    buttonWidth = qMax(buttonWidth, 36); // Minimum button width

    int totalWidth = buttonWidth * 3 + ToggleButtonSpacing * 2 + TogglePadding * 2 + ToggleTriangleWidth;

    setFixedSize(totalWidth, ToggleGroupHeight);
    setCursor(Qt::PointingHandCursor);
}

void ToggleGroupWidget::setLeftSelected(bool selected) {
    if (m_leftSelected != selected) {
        m_leftSelected = selected;
        update();
    }
}

void ToggleGroupWidget::setRightSelected(bool selected) {
    if (m_rightSelected != selected) {
        m_rightSelected = selected;
        update();
    }
}

void ToggleGroupWidget::setRightEnabled(bool enabled) {
    if (m_rightEnabled != enabled) {
        m_rightEnabled = enabled;
        update();
    }
}

void ToggleGroupWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Calculate equal button width for all three buttons
    int mainWidth = width() - ToggleTriangleWidth;
    int availableWidth = mainWidth - TogglePadding * 2 - ToggleButtonSpacing * 2;
    int buttonWidth = availableWidth / 3;
    int buttonHeight = height() - 6;

    // Calculate button rectangles with equal widths
    int x = TogglePadding;
    m_leftRect = QRect(x, 3, buttonWidth, buttonHeight);
    x += buttonWidth + ToggleButtonSpacing;
    m_ampRect = QRect(x, 3, buttonWidth, buttonHeight);
    x += buttonWidth + ToggleButtonSpacing;
    m_rightRect = QRect(x, 3, buttonWidth, buttonHeight);

    // Draw container background with gradient
    QLinearGradient grad = K4Styles::buttonGradient(0, height());

    // Create path with rounded left corners and triangle on right
    QPainterPath containerPath;
    int cornerRadius = 6;

    // Start from top-left corner
    containerPath.moveTo(cornerRadius, 0);
    // Top edge to where triangle starts
    containerPath.lineTo(mainWidth, 0);
    // Triangle pointing right
    containerPath.lineTo(mainWidth, height() / 2 - 6);
    containerPath.lineTo(width(), height() / 2);
    containerPath.lineTo(mainWidth, height() / 2 + 6);
    // Continue to bottom
    containerPath.lineTo(mainWidth, height());
    // Bottom edge
    containerPath.lineTo(cornerRadius, height());
    // Bottom-left rounded corner
    containerPath.quadTo(0, height(), 0, height() - cornerRadius);
    // Left edge
    containerPath.lineTo(0, cornerRadius);
    // Top-left rounded corner
    containerPath.quadTo(0, 0, cornerRadius, 0);
    containerPath.closeSubpath();

    painter.setBrush(grad);
    painter.setPen(QPen(K4Styles::borderColor(), 2));
    painter.drawPath(containerPath);

    // Draw individual button backgrounds with rounded corners and borders
    QFont labelFont = font();
    labelFont.setPixelSize(K4Styles::Dimensions::FontSizeLarge);
    labelFont.setBold(true);

    auto drawButton = [&](const QRect &rect, bool selected, bool enabled, const QString &text) {
        QPainterPath buttonPath;
        buttonPath.addRoundedRect(rect, 4, 4);

        // qDebug() << "drawButton:" << text << "selected=" << selected << "enabled=" << enabled;

        if (selected) {
            // Selected: light grey gradient (matches BandPopupWidget selected style)
            QLinearGradient selGrad = K4Styles::selectedGradient(rect.top(), rect.bottom());
            painter.setBrush(selGrad);
            painter.setPen(QPen(K4Styles::borderColorSelected(), 1));
            painter.drawPath(buttonPath);
            painter.setPen(QColor(K4Styles::Colors::TextDark));
        } else {
            // Unselected: subtle gradient with light border
            QLinearGradient btnGrad(rect.topLeft(), rect.bottomLeft());
            btnGrad.setColorAt(0, QColor(K4Styles::Colors::GradientMid1));
            btnGrad.setColorAt(1, QColor(K4Styles::Colors::GradientBottom));
            painter.setBrush(btnGrad);
            painter.setPen(QPen(QColor(K4Styles::Colors::BorderHover), 1));
            painter.drawPath(buttonPath);
            painter.setPen(enabled ? Qt::white : QColor(K4Styles::Colors::InactiveGray));
        }

        painter.setFont(labelFont);
        painter.drawText(rect, Qt::AlignCenter, text);
    };

    // Draw left button (always enabled)
    drawButton(m_leftRect, m_leftSelected, true, m_leftLabel);

    // Draw & button (selected when both left and right are selected)
    bool ampSelected = m_leftSelected && m_rightSelected;
    drawButton(m_ampRect, ampSelected, m_rightEnabled, "&");

    // Draw right button
    drawButton(m_rightRect, m_rightSelected, m_rightEnabled, m_rightLabel);
}

void ToggleGroupWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        QPoint pos = event->pos();
        if (m_leftRect.contains(pos)) {
            emit leftClicked();
        } else if (m_ampRect.contains(pos) && m_rightEnabled) {
            emit bothClicked();
        } else if (m_rightRect.contains(pos) && m_rightEnabled) {
            emit rightClicked();
        }
    }
}

// ============================================================================
// DisplayPopupWidget Implementation
// ============================================================================

DisplayPopupWidget::DisplayPopupWidget(QWidget *parent) : K4PopupBase(parent) {
    setupUi();
}

QSize DisplayPopupWidget::contentSize() const {
    int cm = ContentMargin;

    // The compact phone layout must remain inside the reachable viewport.
    // Its widest control page is NB + WTR CLRS, not the old palette selector.
    const int minimumWidth = K4Styles::isCompactLayout() ? 700 : 820;
    int width = qMax(7 * MenuButtonWidth + 6 * ButtonSpacing + 2 * cm, minimumWidth);
    int height = TopRowHeight + MenuButtonHeight + RowSpacing + 2 * cm;
    return QSize(width, height);
}

void DisplayPopupWidget::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    // Use base class content margins (includes shadow space)
    QMargins margins = contentMargins();
    // Adjust for DisplayPopupWidget's smaller ContentMargin (8) vs base default (12)
    int marginDiff = K4Styles::Dimensions::PopupContentMargin - ContentMargin;
    margins.setLeft(margins.left() - marginDiff);
    margins.setTop(margins.top() - marginDiff);
    margins.setRight(margins.right() - marginDiff);
    margins.setBottom(margins.bottom() - marginDiff);
    mainLayout->setContentsMargins(margins);
    mainLayout->setSpacing(RowSpacing);

    setupTopRow();
    setupBottomRow();

    // Initial selection
    updateMenuButtonStyles();
    updateToggleStyles();
    updateMenuButtonLabels();

    // Initialize popup size from base class
    initPopup();
}

void DisplayPopupWidget::setupTopRow() {
    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(K4Styles::Dimensions::PopupButtonSpacing);

    // === Target Toggle Groups (left side) ===

    // LCD & EXT toggle group
    m_lcdExtGroup = new ToggleGroupWidget("LCD", "EXT", this);
    m_lcdExtGroup->setLeftSelected(m_lcdEnabled);
    m_lcdExtGroup->setRightSelected(m_extEnabled);
    connect(m_lcdExtGroup, &ToggleGroupWidget::leftClicked, this, [this]() {
        m_lcdEnabled = true;
        m_extEnabled = false;
        m_lcdExtGroup->setLeftSelected(true);
        m_lcdExtGroup->setRightSelected(false);
        emit lcdToggled(true);
        emit extToggled(false);
    });
    connect(m_lcdExtGroup, &ToggleGroupWidget::rightClicked, this, [this]() {
        m_lcdEnabled = false;
        m_extEnabled = true;
        m_lcdExtGroup->setLeftSelected(false);
        m_lcdExtGroup->setRightSelected(true);
        emit lcdToggled(false);
        emit extToggled(true);
    });
    connect(m_lcdExtGroup, &ToggleGroupWidget::bothClicked, this, [this]() {
        m_lcdEnabled = true;
        m_extEnabled = true;
        m_lcdExtGroup->setLeftSelected(true);
        m_lcdExtGroup->setRightSelected(true);
        emit lcdToggled(true);
        emit extToggled(true);
    });
    topRow->addWidget(m_lcdExtGroup);

    // A & B toggle group
    m_vfoAbGroup = new ToggleGroupWidget("A", "B", this);
    m_vfoAbGroup->setLeftSelected(m_vfoAEnabled);
    m_vfoAbGroup->setRightSelected(m_vfoBEnabled);
    m_vfoAbGroup->setRightEnabled(m_vfoBAvailable);
    connect(m_vfoAbGroup, &ToggleGroupWidget::leftClicked, this, [this]() {
        m_vfoAEnabled = true;
        m_vfoBEnabled = false;
        m_vfoAbGroup->setLeftSelected(true);
        m_vfoAbGroup->setRightSelected(false);
        updateRefLevelControlGroup(); // Sync ref level display
        updateSpanControlGroup();     // Sync span display
        emit vfoAToggled(true);
        emit vfoBToggled(false);
    });
    connect(m_vfoAbGroup, &ToggleGroupWidget::rightClicked, this, [this]() {
        if (m_vfoBAvailable) {
            m_vfoAEnabled = false;
            m_vfoBEnabled = true;
            m_vfoAbGroup->setLeftSelected(false);
            m_vfoAbGroup->setRightSelected(true);
            updateRefLevelControlGroup(); // Sync ref level display
            updateSpanControlGroup();     // Sync span display
            emit vfoAToggled(false);
            emit vfoBToggled(true);
        }
    });
    connect(m_vfoAbGroup, &ToggleGroupWidget::bothClicked, this, [this]() {
        if (m_vfoBAvailable) {
            m_vfoAEnabled = true;
            m_vfoBEnabled = true;
            m_vfoAbGroup->setLeftSelected(true);
            m_vfoAbGroup->setRightSelected(true);
            updateRefLevelControlGroup(); // Sync ref level display
            updateSpanControlGroup();     // Sync span display
            emit vfoAToggled(true);
            emit vfoBToggled(true);
        }
    });
    topRow->addWidget(m_vfoAbGroup);

    topRow->addSpacing(8);

    // === Context-Dependent Control Area (right side) ===
    m_controlStack = new QStackedWidget(this);

    m_spanControlPage = createSpanControlPage();
    m_refLevelControlPage = createRefLevelControlPage();
    m_scaleControlPage = createScaleControlPage();
    m_averageControlPage = createAverageControlPage();
    m_nbControlPage = createNbControlPage();
    m_waterfallColorRangeControlPage = createWaterfallColorRangeControlPage();
    m_waterfallControlPage = createWaterfallControlPage();
    m_defaultControlPage = createDefaultControlPage();

    m_controlStack->addWidget(m_spanControlPage);
    m_controlStack->addWidget(m_refLevelControlPage);
    m_controlStack->addWidget(m_scaleControlPage);
    m_controlStack->addWidget(m_averageControlPage);
    m_controlStack->addWidget(m_nbControlPage);
    m_controlStack->addWidget(m_waterfallColorRangeControlPage);
    m_controlStack->addWidget(m_waterfallControlPage);
    m_controlStack->addWidget(m_defaultControlPage);

    // Default to SPAN page
    m_controlStack->setCurrentWidget(m_spanControlPage);

    topRow->addWidget(m_controlStack);
    topRow->addStretch();

    // Add top row to main layout
    static_cast<QVBoxLayout *>(layout())->addLayout(topRow);
}

QWidget *DisplayPopupWidget::createSpanControlPage() {
    auto *page = new QWidget(this);
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_spanControlGroup = new ControlGroupWidget("SPAN", page);
    m_spanControlGroup->setValue("100.0");
    connect(m_spanControlGroup, &ControlGroupWidget::decrementClicked, this,
            &DisplayPopupWidget::spanDecrementRequested);
    connect(m_spanControlGroup, &ControlGroupWidget::incrementClicked, this,
            &DisplayPopupWidget::spanIncrementRequested);
    layout->addWidget(m_spanControlGroup);

    return page;
}

QWidget *DisplayPopupWidget::createRefLevelControlPage() {
    auto *page = new QWidget(this);
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_refLevelControlGroup = new ControlGroupWidget("REF", page);
    m_refLevelControlGroup->setShowAutoButton(true);
    m_refLevelControlGroup->setValue("-108");
    // Ref level increment/decrement - MainWindow handles CAT commands with absolute values
    connect(m_refLevelControlGroup, &ControlGroupWidget::decrementClicked, this,
            &DisplayPopupWidget::refLevelDecrementRequested);
    connect(m_refLevelControlGroup, &ControlGroupWidget::incrementClicked, this,
            &DisplayPopupWidget::refLevelIncrementRequested);
    connect(m_refLevelControlGroup, &ControlGroupWidget::autoClicked, this, [this]() {
        // Auto-ref is GLOBAL - affects both VFOs, no suffix needed
        // Toggle, then explicitly read back the server-authoritative state.
        emit catCommandRequested(QString("#AR/;#AR;"));

        // Optimistic local update
        m_autoRef = !m_autoRef;
        updateRefLevelControlGroup();
        emit autoRefLevelToggled(m_autoRef);
    });
    layout->addWidget(m_refLevelControlGroup);

    // Sync initial AUTO button state with m_autoRef
    updateRefLevelControlGroup();

    return page;
}

QWidget *DisplayPopupWidget::createScaleControlPage() {
    auto *page = new QWidget(this);
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_scaleControlGroup = new ControlGroupWidget("SCALE", page);
    m_scaleControlGroup->setValue(QString::number(m_scale));
    connect(m_scaleControlGroup, &ControlGroupWidget::decrementClicked, this,
            &DisplayPopupWidget::scaleDecrementRequested);
    connect(m_scaleControlGroup, &ControlGroupWidget::incrementClicked, this,
            &DisplayPopupWidget::scaleIncrementRequested);
    layout->addWidget(m_scaleControlGroup);

    return page;
}

void DisplayPopupWidget::updateScaleControlGroup() {
    if (m_scaleControlGroup) {
        m_scaleControlGroup->setValue(QString::number(m_scale));
    }
}

void DisplayPopupWidget::setScale(int scale) {
    if (scale >= 10 && scale <= 150 && scale != m_scale) {
        m_scale = scale;
        updateScaleControlGroup();
    }
}

QWidget *DisplayPopupWidget::createAverageControlPage() {
    auto *page = new QWidget(this);
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_averageControlGroup = new ControlGroupWidget("AVERAGE", page);
    m_averageControlGroup->setValue(QString::number(m_averaging > 0 ? m_averaging : 5));
    connect(m_averageControlGroup, &ControlGroupWidget::decrementClicked, this,
            &DisplayPopupWidget::averagingDecrementRequested);
    connect(m_averageControlGroup, &ControlGroupWidget::incrementClicked, this,
            &DisplayPopupWidget::averagingIncrementRequested);
    layout->addWidget(m_averageControlGroup);

    // Make Peak an explicit phone control.  The desktop uses the amber
    // alternate action on this dual-line button; a dedicated button avoids
    // depending on mouse/right-click semantics on Android.
    m_peakButton = new QPushButton(page);
    m_peakButton->setFixedSize(120, ControlGroupHeight);
    m_peakButton->setText("PEAK OFF");
    connect(m_peakButton, &QPushButton::clicked, this, [this]() {
        m_peakMode = m_peakMode > 0 ? 0 : 1;
        if (m_peakButton)
            m_peakButton->setText(m_peakMode > 0 ? "PEAK ON" : "PEAK OFF");
        updateMenuButtonLabels();
        emit peakModeLocallyChanged(m_peakMode > 0);
        emit catCommandRequested("#PKM/;");
    });
    layout->addWidget(m_peakButton);

    return page;
}

QWidget *DisplayPopupWidget::createNbControlPage() {
    auto *page = new QWidget(this);
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_nbControlGroup = new ControlGroupWidget("NB", page);
    m_nbControlGroup->setCompactLabel(K4Styles::isCompactLayout());
    updateNbControlGroupValue();
    connect(m_nbControlGroup, &ControlGroupWidget::decrementClicked, this,
            &DisplayPopupWidget::nbLevelDecrementRequested);
    connect(m_nbControlGroup, &ControlGroupWidget::incrementClicked, this,
            &DisplayPopupWidget::nbLevelIncrementRequested);
    connect(m_nbControlGroup, &ControlGroupWidget::valueClicked, this,
            &DisplayPopupWidget::nbToggleRequested);
    m_nbControlGroup->setToolTip("Tap the center value to cycle OFF, ON, and AUTO; use -/+ to adjust the level");
    m_nbControlGroup->setAccessibleName("Waterfall noise blanker mode and level");
    layout->addWidget(m_nbControlGroup);

    return page;
}

QWidget *DisplayPopupWidget::createWaterfallColorRangeControlPage() {
    auto *page = new QWidget(this);
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_waterfallColorRangeControlGroup = new ControlGroupWidget("WTR CLRS", page);
    updateWaterfallColorRangeControlGroup();
    connect(m_waterfallColorRangeControlGroup, &ControlGroupWidget::decrementClicked, this, [this]() {
        m_waterfallColorRange = qMax(5, m_waterfallColorRange - 1);
        updateWaterfallColorRangeControlGroup();
        updateMenuButtonLabels();
        emit waterfallColorRangeLocallyChanged(m_waterfallColorRange);
    });
    connect(m_waterfallColorRangeControlGroup, &ControlGroupWidget::incrementClicked, this, [this]() {
        m_waterfallColorRange = qMin(30, m_waterfallColorRange + 1);
        updateWaterfallColorRangeControlGroup();
        updateMenuButtonLabels();
        emit waterfallColorRangeLocallyChanged(m_waterfallColorRange);
    });
    layout->addWidget(m_waterfallColorRangeControlGroup);

    return page;
}

void DisplayPopupWidget::updateNbControlGroupValue() {
    if (m_nbControlGroup) {
        QString modeText;
        switch (m_ddcNbMode) {
        case 0:
            modeText = "OFF";
            break;
        case 1:
            modeText = "ON";
            break;
        case 2:
            modeText = "AUTO";
            break;
        default:
            modeText = "OFF";
            break;
        }
        m_nbControlGroup->setValue(QString("%1  %2").arg(modeText).arg(m_ddcNbLevel));
    }
}

void DisplayPopupWidget::updateWaterfallColorRangeControlGroup() {
    if (m_waterfallColorRangeControlGroup)
        m_waterfallColorRangeControlGroup->setValue(QString::number(m_waterfallColorRange));
}

QWidget *DisplayPopupWidget::createWaterfallControlPage() {
    auto *page = new QWidget(this);
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_waterfallControlGroup = new ControlGroupWidget("WTRFALL", page);
    updateWaterfallControlGroup();
    // Just emit signals - MainWindow handles CAT commands and state updates
    connect(m_waterfallControlGroup, &ControlGroupWidget::decrementClicked, this,
            &DisplayPopupWidget::waterfallHeightDecrementRequested);
    connect(m_waterfallControlGroup, &ControlGroupWidget::incrementClicked, this,
            &DisplayPopupWidget::waterfallHeightIncrementRequested);
    layout->addWidget(m_waterfallControlGroup);

    return page;
}

void DisplayPopupWidget::updateWaterfallControlGroup() {
    if (m_waterfallControlGroup) {
        // Show percentage based on LCD/EXT selection
        int height = (m_extEnabled && !m_lcdEnabled) ? m_waterfallHeightExt : m_waterfallHeight;
        m_waterfallControlGroup->setValue(QString("%1%").arg(height));
    }
}

QWidget *DisplayPopupWidget::createDefaultControlPage() {
    auto *page = new QWidget(this);
    auto *layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);

    // Placeholder - empty for now
    auto *placeholder = new QLabel("", page);
    layout->addWidget(placeholder);

    return page;
}

void DisplayPopupWidget::setupBottomRow() {
    auto *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(ButtonSpacing);

    // Menu items with primary/alternate labels
    struct MenuItemDef {
        QString primary;
        QString alternate;
        MenuItem item;
    };

    QList<MenuItemDef> items = {{"PAN = A", "WTRFALL", PanWaterfall}, {"NB", "WTR CLRS", NbWtrClrs},
                                {"REF LVL", "SCALE", RefLvlScale},    {"SPAN", "CENTER", SpanCenter},
                                {"AVERAGE", "PEAK OFF", AveragePeak}, {"FIXED2", "FREEZE", FixedFreeze},
                                {"CURS A+", "CURS B+", CursAB}};

    for (const auto &def : items) {
        auto *btn = new DisplayMenuButton(def.primary, def.alternate, this);
        m_menuButtons.append(btn);

        connect(btn, &DisplayMenuButton::clicked, this, [this, item = def.item]() { onMenuItemClicked(item); });
        connect(btn, &DisplayMenuButton::rightClicked, this,
                [this, item = def.item]() { onMenuItemRightClicked(item); });

        bottomRow->addWidget(btn);
    }

    static_cast<QVBoxLayout *>(layout())->addLayout(bottomRow);
}

void DisplayPopupWidget::onMenuItemClicked(MenuItem item) {
    m_selectedItem = item;
    updateMenuButtonStyles();

    // Switch control page for items that show controls
    switch (item) {
    case SpanCenter:
        m_controlStack->setCurrentWidget(m_spanControlPage);
        break;
    case RefLvlScale:
        m_controlStack->setCurrentWidget(m_refLevelControlPage);
        break;
    case AveragePeak:
        m_controlStack->setCurrentWidget(m_averageControlPage);
        break;
    case NbWtrClrs:
        // A normal press is the NB action on this dual-line phone button.
        m_controlStack->setCurrentWidget(m_nbControlPage);
        break;
    default:
        m_controlStack->setCurrentWidget(m_defaultControlPage);
        break;
    }

    // Emit CAT commands based on menu item
    switch (item) {
    case PanWaterfall: {
        // Cycle DPM: 0 → 1 → 2 → 0 (use current mode based on LCD/EXT selection)
        int currentMode = (m_extEnabled && !m_lcdEnabled) ? m_dualPanModeExt : m_dualPanModeLcd;
        if (currentMode < 0)
            currentMode = 0; // Handle uninitialized state
        int newMode = (currentMode + 1) % 3;

        // Send CAT command to radio
        if (m_lcdEnabled) {
            emit catCommandRequested(QString("#DPM%1;").arg(newMode));
        }
        if (m_extEnabled) {
            emit catCommandRequested(QString("#HDPM%1;").arg(newMode));
        }

        // Update local state immediately (K4 doesn't echo #DPM commands)
        if (m_lcdEnabled) {
            m_dualPanModeLcd = newMode;
        }
        if (m_extEnabled) {
            m_dualPanModeExt = newMode;
        }
        updateMenuButtonLabels();

        // Auto-sync A/B toggle with PAN mode (so Center/Span target correct VFO)
        switch (newMode) {
        case 0: // A only
            m_vfoAEnabled = true;
            m_vfoBEnabled = false;
            break;
        case 1: // B only
            m_vfoAEnabled = false;
            m_vfoBEnabled = true;
            break;
        case 2: // Dual (A+B)
            m_vfoAEnabled = true;
            m_vfoBEnabled = true;
            break;
        }
        updateToggleStyles();
        updateRefLevelControlGroup(); // Sync ref level display with A/B
        updateSpanControlGroup();     // Sync span display with A/B

        // Notify MainWindow to update panadapter display
        emit dualPanModeChanged(newMode);
        break;
    }
    case NbWtrClrs:
        // The page contains explicit NB controls. Do not silently toggle NB
        // when the user selects the menu item.
        break;
    // Note: AveragePeak only shows control page (first switch) - no CAT command on click
    // The +/- buttons in the control page handle averaging changes via averagingIncrement/DecrementRequested signals
    case FixedFreeze: {
        // Cycle: STATIC(5) → SLIDE2(2) → TRACK(0) → FIXED1(3) → FIXED2(4) → SLIDE1(1) → repeat
        int newMode;
        switch (m_fixedTuneMode) {
        case 5:
            newMode = 2;
            break; // STATIC → SLIDE2
        case 2:
            newMode = 0;
            break; // SLIDE2 → TRACK
        case 0:
            newMode = 3;
            break; // TRACK → FIXED1
        case 3:
            newMode = 4;
            break; // FIXED1 → FIXED2
        case 4:
            newMode = 1;
            break; // FIXED2 → SLIDE1
        case 1:
            newMode = 5;
            break; // SLIDE1 → STATIC
        default:
            newMode = 5;
            break; // Start at STATIC
        }

        // Send optimized CAT commands
        switch (newMode) {
        case 5:
            emit catCommandRequested("#FXA3;");
            break; // STATIC
        case 2:
            emit catCommandRequested("#FXA4;");
            break; // SLIDE2
        case 0:
            emit catCommandRequested("#FXA0;#FXT0;");
            break; // TRACK
        case 3:
            emit catCommandRequested("#FXT1;");
            break; // FIXED1
        case 4:
            emit catCommandRequested("#FXA1;");
            break; // FIXED2
        case 1:
            emit catCommandRequested("#FXA2;");
            break; // SLIDE1
        }

        // Optimistic update (K4 doesn't echo #FXT/#FXA commands)
        m_fixedTuneMode = newMode;
        updateMenuButtonLabels();
        break;
    }
    case CursAB:
        // Cycle VFO A cursor mode
        emit catCommandRequested("#VFA/;");
        break;
    default:
        break;
    }

    emit menuItemSelected(item);
}

void DisplayPopupWidget::onMenuItemRightClicked(MenuItem item) {
    // Handle right-click (alternate action) for each menu item
    switch (item) {
    case PanWaterfall: {
        // Show WATERFALL height control page (right-click = WTRFALL)
        m_selectedItem = PanWaterfall;
        updateMenuButtonStyles();
        m_controlStack->setCurrentWidget(m_waterfallControlPage);
        updateWaterfallControlGroup();
        break;
    }
    case NbWtrClrs: {
        // Long-press selects only the local WTR CLRS adjustment field. Do not
        // send #WFC or #WBS: this is local rendering only.
        m_selectedItem = NbWtrClrs;
        updateMenuButtonStyles();
        m_controlStack->setCurrentWidget(m_waterfallColorRangeControlPage);
        updateWaterfallColorRangeControlGroup();
        break;
    }
    case RefLvlScale:
        // Show SCALE control page (right-click on REF LVL/SCALE button)
        m_selectedItem = RefLvlScale;
        updateMenuButtonStyles();
        m_controlStack->setCurrentWidget(m_scaleControlPage);
        updateScaleControlGroup();
        break;
    case SpanCenter: {
        // Center on VFO
        QString suffix = m_vfoBEnabled && !m_vfoAEnabled ? "$" : "";
        emit catCommandRequested(QString("FC%1;").arg(suffix));
        emit panadapterCentered();
        break;
    }
    case AveragePeak:
        // Toggle peak mode.  Do this optimistically because the K4 may not
        // echo #PKM/ immediately; the local panadapter must follow the same
        // state as the radio instead of retaining its previous peak trace.
        m_peakMode = m_peakMode > 0 ? 0 : 1;
        updateMenuButtonLabels();
        emit peakModeLocallyChanged(m_peakMode > 0);
        emit catCommandRequested("#PKM/;");
        break;
    case FixedFreeze: {
        // Toggle freeze
        int newFreeze = m_freeze > 0 ? 0 : 1;
        m_freeze = newFreeze;     // Optimistic update
        updateMenuButtonLabels(); // Update button text to FREEZE/FROZEN
        emit catCommandRequested(QString("#FRZ%1;").arg(newFreeze));
        break;
    }
    case CursAB:
        // Cycle VFO B cursor mode
        emit catCommandRequested("#VFB/;");
        break;
    default:
        break;
    }

    emit alternateItemClicked(item);
}

void DisplayPopupWidget::updateToggleStyles() {
    // Update toggle group widgets
    if (m_lcdExtGroup) {
        m_lcdExtGroup->setLeftSelected(m_lcdEnabled);
        m_lcdExtGroup->setRightSelected(m_extEnabled);
    }
    if (m_vfoAbGroup) {
        m_vfoAbGroup->setLeftSelected(m_vfoAEnabled);
        m_vfoAbGroup->setRightSelected(m_vfoBEnabled);
        m_vfoAbGroup->setRightEnabled(m_vfoBAvailable);
    }
}

void DisplayPopupWidget::updateMenuButtonStyles() {
    for (int i = 0; i < m_menuButtons.size(); ++i) {
        m_menuButtons[i]->setSelected(i == static_cast<int>(m_selectedItem));
    }
}

void DisplayPopupWidget::setSpanValueA(double spanKHz) {
    m_spanA = spanKHz;
    updateSpanControlGroup();
}

void DisplayPopupWidget::setSpanValueB(double spanKHz) {
    m_spanB = spanKHz;
    updateSpanControlGroup();
}

void DisplayPopupWidget::updateSpanControlGroup() {
    if (!m_spanControlGroup)
        return;
    // Show correct value based on A/B toggle
    bool useB = m_vfoBEnabled && !m_vfoAEnabled;
    double value = useB ? m_spanB : m_spanA;
    m_spanControlGroup->setValue(QString::number(value, 'f', 1));
}

void DisplayPopupWidget::setRefLevelValue(int dB) {
    // Legacy: forward to A
    setRefLevelValueA(dB);
}

void DisplayPopupWidget::setAutoRefLevel(bool enabled) {
    // Auto-ref is GLOBAL - affects both VFOs
    m_autoRef = enabled;
    updateRefLevelControlGroup();
}

void DisplayPopupWidget::setRefLevelValueA(int dB) {
    m_refLevelA = dB;
    updateRefLevelControlGroup();
}

void DisplayPopupWidget::setRefLevelValueB(int dB) {
    m_refLevelB = dB;
    updateRefLevelControlGroup();
}

void DisplayPopupWidget::updateRefLevelControlGroup() {
    if (!m_refLevelControlGroup)
        return;

    // Determine which ref level VALUE to show based on A/B toggle
    // (Values are per-VFO, but auto mode is GLOBAL)
    bool useB = m_vfoBEnabled && !m_vfoAEnabled;
    int value = useB ? m_refLevelB : m_refLevelA;

    m_refLevelControlGroup->setValue(QString::number(value));
    m_refLevelControlGroup->setAutoEnabled(m_autoRef); // GLOBAL auto mode
    m_refLevelControlGroup->setValueFaded(m_autoRef);  // Faded when auto
}

// ============================================================================
// State Setters (called by RadioState signals)
// ============================================================================

void DisplayPopupWidget::setDualPanModeLcd(int mode) {
    if (m_dualPanModeLcd != mode) {
        m_dualPanModeLcd = mode;
        // Update labels if LCD is selected
        if (m_lcdEnabled) {
            updateMenuButtonLabels();

            // Auto-sync A/B toggle with PAN mode (for initial connect sync)
            switch (mode) {
            case 0: // A only
                m_vfoAEnabled = true;
                m_vfoBEnabled = false;
                break;
            case 1: // B only
                m_vfoAEnabled = false;
                m_vfoBEnabled = true;
                break;
            case 2: // Dual (A+B)
                m_vfoAEnabled = true;
                m_vfoBEnabled = true;
                break;
            }
            updateToggleStyles();
            updateRefLevelControlGroup(); // Sync ref level display with A/B
            updateSpanControlGroup();     // Sync span display with A/B
        }
    }
}

void DisplayPopupWidget::setDualPanModeExt(int mode) {
    if (m_dualPanModeExt != mode) {
        m_dualPanModeExt = mode;
        // Update labels if EXT only is selected
        if (m_extEnabled && !m_lcdEnabled) {
            updateMenuButtonLabels();
        }
    }
}

void DisplayPopupWidget::setDisplayModeLcd(int mode) {
    if (m_displayModeLcd != mode) {
        m_displayModeLcd = mode;
        if (m_lcdEnabled) {
            updateMenuButtonLabels();
        }
    }
}

void DisplayPopupWidget::setDisplayModeExt(int mode) {
    if (m_displayModeExt != mode) {
        m_displayModeExt = mode;
        if (m_extEnabled && !m_lcdEnabled) {
            updateMenuButtonLabels();
        }
    }
}

void DisplayPopupWidget::setWaterfallColor(int color) {
    color = qBound(0, color, 4);
    m_waterfallColor = color;
    updateMenuButtonLabels();
}

void DisplayPopupWidget::setAveraging(int value) {
    if (m_averaging != value) {
        m_averaging = value;
        if (m_averageControlGroup) {
            m_averageControlGroup->setValue(QString::number(value));
        }
        updateMenuButtonLabels();
    }
}

void DisplayPopupWidget::setPeakMode(bool enabled) {
    if (m_peakMode != enabled) {
        m_peakMode = enabled;
        if (m_peakButton)
            m_peakButton->setText(enabled ? "PEAK ON" : "PEAK OFF");
        updateMenuButtonLabels();
    }
}

void DisplayPopupWidget::setFixedTuneMode(int fxt, int fxa) {
    // Combine FXT and FXA into internal state (0-5)
    // TRACK=0: FXT=0 (doesn't matter what FXA is)
    // SLIDE1=1: FXT=1, FXA=0 (FULL SPAN)
    // SLIDE2=2: FXT=1, FXA=4 (SLIDE NEAR EDGE)
    // FIXED1=3: FXT=1, FXA=1 (HALF SPAN)
    // FIXED2=4: FXT=1, FXA=2 (SLIDE EDGE)
    // STATIC=5: FXT=1, FXA=3 (STATIC)
    int newMode;
    if (fxt == 0) {
        newMode = 0; // TRACK
    } else {
        // FXT=1, map FXA to internal state
        switch (fxa) {
        case 0:
            newMode = 1;
            break; // SLIDE1
        case 4:
            newMode = 2;
            break; // SLIDE2
        case 1:
            newMode = 3;
            break; // FIXED1
        case 2:
            newMode = 4;
            break; // FIXED2
        case 3:
            newMode = 5;
            break; // STATIC
        default:
            newMode = 0;
            break;
        }
    }

    if (m_fixedTuneMode != newMode) {
        m_fixedTuneMode = newMode;
        updateMenuButtonLabels();
    }
}

void DisplayPopupWidget::setFreeze(bool enabled) {
    if (m_freeze != enabled) {
        m_freeze = enabled;
        updateMenuButtonLabels();
    }
}

void DisplayPopupWidget::setVfoACursor(int mode) {
    if (m_vfaMode != mode) {
        m_vfaMode = mode;
        updateMenuButtonLabels();
    }
}

void DisplayPopupWidget::setVfoBCursor(int mode) {
    if (m_vfbMode != mode) {
        m_vfbMode = mode;
        updateMenuButtonLabels();
    }
}

void DisplayPopupWidget::setDdcNbMode(int mode) {
    if (m_ddcNbMode != mode) {
        m_ddcNbMode = mode;
        updateNbControlGroupValue();
    }
}

void DisplayPopupWidget::setDdcNbLevel(int level) {
    if (m_ddcNbLevel != level) {
        m_ddcNbLevel = level;
        updateNbControlGroupValue();
    }
}

void DisplayPopupWidget::setWaterfallHeight(int percent) {
    // LCD waterfall height (#WFHxx;)
    if (m_waterfallHeight != percent) {
        m_waterfallHeight = percent;
        updateWaterfallControlGroup();
    }
}

void DisplayPopupWidget::setWaterfallHeightExt(int percent) {
    // External HDMI waterfall height (#HWFHxx;)
    if (m_waterfallHeightExt != percent) {
        m_waterfallHeightExt = percent;
        updateWaterfallControlGroup();
    }
}

void DisplayPopupWidget::setWaterfallColorRange(int range) {
    m_waterfallColorRange = qBound(5, range, 30);
    if (m_waterfallColorRangeControlGroup)
        m_waterfallColorRangeControlGroup->setValue(QString::number(m_waterfallColorRange));
    if (m_menuButtons.size() > 1 && m_menuButtons[1])
        m_menuButtons[1]->setAlternateText(QString("WTR CLRS %1").arg(m_waterfallColorRange));
}

// ============================================================================
// Button Label Updates
// ============================================================================

void DisplayPopupWidget::updateMenuButtonLabels() {
    if (m_menuButtons.size() < 7)
        return;

    // Use LCD or EXT state based on selection
    int panMode = (m_extEnabled && !m_lcdEnabled) ? m_dualPanModeExt : m_dualPanModeLcd;
    int displayMode = (m_extEnabled && !m_lcdEnabled) ? m_displayModeExt : m_displayModeLcd;

    // PanWaterfall button (index 0)
    QString panText;
    switch (panMode) {
    case 0:
        panText = "PAN = A";
        break;
    case 1:
        panText = "PAN = B";
        break;
    case 2:
        panText = "PAN = A+B";
        break;
    default:
        panText = "PAN = A";
        break;
    }
    m_menuButtons[0]->setPrimaryText(panText);
    m_menuButtons[0]->setAlternateText(displayMode == 0 ? "SPECTRUM" : "WTRFALL");

    // NbWtrClrs button (index 1) - WTR CLRS is a local intensity range, not
    // a remote palette command. Show the live value so it is discoverable.
    m_menuButtons[1]->setAlternateText(QString("WTR CLRS %1").arg(m_waterfallColorRange));

    // RefLvlScale button (index 2) - no dynamic text for now

    // SpanCenter button (index 3) - no dynamic text for now

    // AveragePeak button (index 4) - primary text stays as "AVERAGE"
    m_menuButtons[4]->setAlternateText(m_peakMode > 0 ? "PEAK ON" : "PEAK OFF");

    // FixedFreeze button (index 5)
    static const char *fixedModeNames[] = {"TRACK", "SLIDE1", "SLIDE2", "FIXED1", "FIXED2", "STATIC"};
    if (m_fixedTuneMode >= 0 && m_fixedTuneMode <= 5) {
        m_menuButtons[5]->setPrimaryText(fixedModeNames[m_fixedTuneMode]);
    }
    m_menuButtons[5]->setAlternateText(m_freeze > 0 ? "FROZEN" : "FREEZE");

    // CursAB button (index 6)
    // OFF=hide, ON=show, AUTO=show, HIDE=hide
    static const char *cursorNames[] = {"CURS A-", "CURS A+", "CURS A+", "CURS A-"};
    static const char *cursorBNames[] = {"CURS B-", "CURS B+", "CURS B+", "CURS B-"};
    if (m_vfaMode >= 0 && m_vfaMode <= 3) {
        m_menuButtons[6]->setPrimaryText(cursorNames[m_vfaMode]);
    }
    if (m_vfbMode >= 0 && m_vfbMode <= 3) {
        m_menuButtons[6]->setAlternateText(cursorBNames[m_vfbMode]);
    }
}

QString DisplayPopupWidget::getCommandPrefix() const {
    // Returns "H" for EXT-only mode, empty string otherwise
    if (m_extEnabled && !m_lcdEnabled) {
        return "H";
    }
    return "";
}
