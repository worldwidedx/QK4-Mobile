#include "fnpopupwidget.h"
#include "../settings/radiosettings.h"
#include "k4styles.h"
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>

namespace {
// Layout constants
const int ButtonWidth = 70;
const int ButtonHeight = 44;
const int ButtonSpacing = 8;
} // namespace

// ============================================================================
// FnMenuButton Implementation
// ============================================================================

FnMenuButton::FnMenuButton(const QString &primaryText, const QString &alternateText, QWidget *parent)
    : QWidget(parent), m_primaryText(primaryText), m_alternateText(alternateText) {
    setFixedSize(ButtonWidth, ButtonHeight);
    setCursor(Qt::PointingHandCursor);
    m_longPressTimer = new QTimer(this);
    m_longPressTimer->setSingleShot(true);
    m_longPressTimer->setInterval(550);
    connect(m_longPressTimer, &QTimer::timeout, this, [this]() {
        if (m_alternateFunctionId.isEmpty())
            return;
        m_longPressTriggered = true;
        emit rightClicked();
    });
}

void FnMenuButton::setPrimaryText(const QString &text) {
    if (m_primaryText != text) {
        m_primaryText = text;
        update();
    }
}

void FnMenuButton::setAlternateText(const QString &text) {
    if (m_alternateText != text) {
        m_alternateText = text;
        update();
    }
}

void FnMenuButton::paintEvent(QPaintEvent *event) {
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
    primaryFont.setBold(false);
    painter.setFont(primaryFont);
    painter.setPen(Qt::white);

    QRect primaryRect(0, 4, width(), height() / 2 - 2);
    painter.drawText(primaryRect, Qt::AlignCenter, m_primaryText);

    // Alternate text (amber) - bottom (only if not empty)
    if (!m_alternateText.isEmpty()) {
        QFont altFont = font();
        altFont.setPixelSize(K4Styles::Dimensions::FontSizeMedium);
        altFont.setBold(false);
        painter.setFont(altFont);
        painter.setPen(QColor(K4Styles::Colors::AccentAmber));

        QRect altRect(0, height() / 2, width(), height() / 2 - 4);
        painter.drawText(altRect, Qt::AlignCenter, m_alternateText);
    }
}

void FnMenuButton::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // Android has no right-click. A long press invokes the amber lower
        // function (F2/F4/F6/F8), matching the mobile convention used by the
        // rest of the phone UI.
        if (K4Styles::isCompactLayout()) {
            m_pressPosition = event->pos();
            m_longPressTriggered = false;
            m_longPressTimer->start();
        } else {
            emit clicked();
        }
    } else if (event->button() == Qt::RightButton) {
        m_longPressTimer->stop();
        emit rightClicked();
    }
}

void FnMenuButton::mouseMoveEvent(QMouseEvent *event) {
    if (m_longPressTimer->isActive() && (event->pos() - m_pressPosition).manhattanLength() > 12)
        m_longPressTimer->stop();
    QWidget::mouseMoveEvent(event);
}

void FnMenuButton::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        const bool triggerPrimary = m_longPressTimer->isActive() && !m_longPressTriggered;
        m_longPressTimer->stop();
        if (triggerPrimary)
            emit clicked();
    }
    QWidget::mouseReleaseEvent(event);
}

void FnMenuButton::enterEvent(QEnterEvent *event) {
    Q_UNUSED(event)
    m_hovered = true;
    update();
}

void FnMenuButton::leaveEvent(QEvent *event) {
    Q_UNUSED(event)
    m_hovered = false;
    update();
}

// ============================================================================
// FnPopupWidget Implementation
// ============================================================================

FnPopupWidget::FnPopupWidget(QWidget *parent) : K4PopupBase(parent) {
    setupButtons();

    // Connect to settings changes
    connect(RadioSettings::instance(), &RadioSettings::macrosChanged, this, &FnPopupWidget::updateButtonLabels);
}

QSize FnPopupWidget::contentSize() const {
    int cm = K4Styles::Dimensions::PopupContentMargin;

    const int buttonCount = m_buttons.isEmpty() ? 8 : m_buttons.size();
    int width = buttonCount * ButtonWidth + (buttonCount - 1) * ButtonSpacing + 2 * cm;
    int height = ButtonHeight + 2 * cm;
    return QSize(width, height);
}

void FnPopupWidget::setupButtons() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(contentMargins());
    mainLayout->setSpacing(0);

    auto *rowLayout = new QHBoxLayout();
    rowLayout->setSpacing(ButtonSpacing);

    // Button definitions: primary/alternate text and function IDs
    struct ButtonDef {
        QString primary;
        QString alternate;
        QString primaryId;
        QString alternateId;
    };

    QVector<ButtonDef> buttonDefs = {{"F1", "F2", MacroIds::FnF1, MacroIds::FnF2},
                                     {"F3", "F4", MacroIds::FnF3, MacroIds::FnF4},
                                     {"F5", "F6", MacroIds::FnF5, MacroIds::FnF6},
                                     {"F7", "F8", MacroIds::FnF7, MacroIds::FnF8},
                                     {"SCRN CAP", "", MacroIds::ScrnCap, ""},
                                     {"SW LIST", "UPDATE", MacroIds::SwList, MacroIds::Update},
                                     {"DXLIST", "Log", MacroIds::DxList, MacroIds::Log},
                                     {"FT8/FT4", "SSTV", MacroIds::Ft8, MacroIds::Sstv}};

    for (int i = 0; i < buttonDefs.size(); ++i) {
        auto btn = new FnMenuButton(buttonDefs[i].primary, buttonDefs[i].alternate, this);
        btn->setPrimaryFunctionId(buttonDefs[i].primaryId);
        btn->setAlternateFunctionId(buttonDefs[i].alternateId);
        if (buttonDefs[i].primaryId == MacroIds::DxList) {
            btn->setObjectName("dxListLogButton");
            btn->setAccessibleName("DXLIST. Hold for Log.");
            btn->setToolTip("Tap: DXLIST · Hold: Logbook");
        }
        if (buttonDefs[i].primaryId == MacroIds::Ft8) {
            btn->setObjectName("digitalModesButton");
            btn->setAccessibleName("FT8 or FT4. Hold for SSTV.");
            btn->setToolTip("Tap: FT8/FT4 · Hold: SSTV");
        }

        connect(btn, &FnMenuButton::clicked, this, [this, i]() { onButtonClicked(i); });
        connect(btn, &FnMenuButton::rightClicked, this, [this, i]() { onButtonRightClicked(i); });

        m_buttons.append(btn);
        rowLayout->addWidget(btn);
    }

    mainLayout->addLayout(rowLayout);

    // Update labels from saved macros
    updateButtonLabels();

    // Initialize popup size from base class
    initPopup();
}

void FnPopupWidget::updateButtonLabels() {
    auto settings = RadioSettings::instance();

    // Update buttons 1-4 (Fn.F1-F8) with custom labels if defined
    for (int i = 0; i < 4 && i < m_buttons.size(); ++i) {
        auto btn = m_buttons[i];

        // Get macro labels for primary and alternate
        MacroEntry primaryMacro = settings->macro(btn->primaryFunctionId());
        MacroEntry alternateMacro = settings->macro(btn->alternateFunctionId());

        // Use custom label if defined, otherwise default (F1, F2, etc.)
        QString primaryLabel = primaryMacro.label.isEmpty() ? QString("F%1").arg(i * 2 + 1) : primaryMacro.label;
        QString alternateLabel = alternateMacro.label.isEmpty() ? QString("F%1").arg(i * 2 + 2) : alternateMacro.label;

        btn->setPrimaryText(primaryLabel);
        btn->setAlternateText(alternateLabel);
    }
}

void FnPopupWidget::onButtonClicked(int buttonIndex) {
    if (buttonIndex >= 0 && buttonIndex < m_buttons.size()) {
        QString functionId = m_buttons[buttonIndex]->primaryFunctionId();
        if (!functionId.isEmpty()) {
            emit functionTriggered(functionId);
        }
    }
    hidePopup();
}

void FnPopupWidget::onButtonRightClicked(int buttonIndex) {
    if (buttonIndex >= 0 && buttonIndex < m_buttons.size()) {
        QString functionId = m_buttons[buttonIndex]->alternateFunctionId();
        if (!functionId.isEmpty()) {
            emit functionTriggered(functionId);
        }
    }
    hidePopup();
}
