#include "buttonrowpopup.h"
#include "k4styles.h"
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

namespace {
// Layout constants
const int ButtonWidth = 70;
const int ButtonHeight = 44;
const int ButtonSpacing = 8;
} // namespace

// ============================================================================
// RxMenuButton Implementation
// ============================================================================

RxMenuButton::RxMenuButton(const QString &primaryText, const QString &alternateText, QWidget *parent)
    : QWidget(parent), m_primaryText(primaryText), m_alternateText(alternateText) {
    setFixedSize(ButtonWidth, ButtonHeight);
    setCursor(Qt::PointingHandCursor);

    m_longPressTimer.setSingleShot(true);
    m_longPressTimer.setInterval(550);
    connect(&m_longPressTimer, &QTimer::timeout, this, [this]() {
        if (m_leftPressed && !m_pressCancelled && m_hasAlternateFunction &&
            !m_alternateText.isEmpty()) {
            m_longPressHandled = true;
            emit rightClicked();
        }
    });
}

void RxMenuButton::setPrimaryText(const QString &text) {
    if (m_primaryText != text) {
        m_primaryText = text;
        update();
    }
}

void RxMenuButton::setAlternateText(const QString &text) {
    if (m_alternateText != text) {
        m_alternateText = text;
        update();
    }
}

void RxMenuButton::setHasAlternateFunction(bool has) {
    if (m_hasAlternateFunction != has) {
        m_hasAlternateFunction = has;
        update();
    }
}

void RxMenuButton::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background - subtle gradient
    QLinearGradient grad = K4Styles::buttonGradient(0, height(), m_hovered);
    painter.setBrush(grad);
    painter.setPen(QPen(K4Styles::borderColor(), 2));
    painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 5, 5);

    // Check if we have alternate text
    bool hasAlternate = !m_alternateText.isEmpty();

    if (hasAlternate) {
        // Dual-line mode: Primary text (white) - top
        QFont primaryFont = font();
        primaryFont.setPixelSize(K4Styles::Dimensions::FontSizeButton);
        primaryFont.setBold(false);
        painter.setFont(primaryFont);
        painter.setPen(Qt::white);

        QRect primaryRect(0, 4, width(), height() / 2 - 2);
        painter.drawText(primaryRect, Qt::AlignCenter, m_primaryText);

        // Alternate text - bottom (amber if has alternate function, white if just label)
        QFont altFont = font();
        altFont.setPixelSize(K4Styles::Dimensions::FontSizeMedium);
        altFont.setBold(false);
        painter.setFont(altFont);
        painter.setPen(m_hasAlternateFunction ? QColor(K4Styles::Colors::AccentAmber) : Qt::white);

        QRect altRect(0, height() / 2, width(), height() / 2 - 4);
        painter.drawText(altRect, Qt::AlignCenter, m_alternateText);
    } else {
        // Single-line mode: Center the primary text
        QFont primaryFont = font();
        primaryFont.setPixelSize(K4Styles::Dimensions::FontSizeButton);
        primaryFont.setBold(true);
        painter.setFont(primaryFont);
        painter.setPen(Qt::white);

        painter.drawText(rect(), Qt::AlignCenter, m_primaryText);
    }
}

void RxMenuButton::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        if (K4Styles::isCompactLayout()) {
            m_pressPosition = event->pos();
            m_leftPressed = true;
            m_longPressHandled = false;
            m_pressCancelled = false;
            m_longPressTimer.start();
        } else {
            emit clicked();
        }
    } else if (event->button() == Qt::RightButton) {
        m_longPressTimer.stop();
        emit rightClicked();
    }
    event->accept();
}

void RxMenuButton::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && K4Styles::isCompactLayout()) {
        m_longPressTimer.stop();
        const bool triggerPrimary = m_leftPressed && !m_longPressHandled &&
                                    !m_pressCancelled && rect().contains(event->pos());
        m_leftPressed = false;
        if (triggerPrimary) {
            emit clicked();
        }
    }
    event->accept();
}

void RxMenuButton::mouseMoveEvent(QMouseEvent *event) {
    if (m_leftPressed && (event->pos() - m_pressPosition).manhattanLength() > 12) {
        m_pressCancelled = true;
        m_longPressTimer.stop();
    }
    event->accept();
}

void RxMenuButton::enterEvent(QEnterEvent *event) {
    Q_UNUSED(event)
    m_hovered = true;
    update();
}

void RxMenuButton::leaveEvent(QEvent *event) {
    Q_UNUSED(event)
    if (m_leftPressed) {
        m_pressCancelled = true;
        m_longPressTimer.stop();
    }
    m_hovered = false;
    update();
}

// ============================================================================
// ButtonRowPopup Implementation
// ============================================================================

ButtonRowPopup::ButtonRowPopup(QWidget *parent) : K4PopupBase(parent) {
    setupUi();
}

QSize ButtonRowPopup::contentSize() const {
    int cm = K4Styles::Dimensions::PopupContentMargin;

    int width = 7 * ButtonWidth + 6 * ButtonSpacing + 2 * cm;
    int height = ButtonHeight + 2 * cm;
    return QSize(width, height);
}

void ButtonRowPopup::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(contentMargins());
    mainLayout->setSpacing(0);

    // Single row of 7 buttons
    auto *rowLayout = new QHBoxLayout();
    rowLayout->setSpacing(ButtonSpacing);

    for (int i = 0; i < 7; ++i) {
        auto *btn = new RxMenuButton(QString::number(i + 1), QString(), this);
        btn->setProperty("buttonIndex", i);

        connect(btn, &RxMenuButton::clicked, this, [this, i]() { emit buttonClicked(i); });
        connect(btn, &RxMenuButton::rightClicked, this, [this, i]() { emit buttonRightClicked(i); });

        m_buttons.append(btn);
        rowLayout->addWidget(btn);
    }

    mainLayout->addLayout(rowLayout);

    // Initialize popup size from base class
    initPopup();
}

void ButtonRowPopup::setButtonLabels(const QStringList &labels) {
    for (int i = 0; i < qMin(labels.size(), m_buttons.size()); ++i) {
        m_buttons[i]->setPrimaryText(labels[i]);
        m_buttons[i]->setAlternateText(QString()); // Clear alternate
    }
}

void ButtonRowPopup::setButtonLabel(int index, const QString &primary, const QString &alternate,
                                    bool hasAlternateFunction) {
    if (index >= 0 && index < m_buttons.size()) {
        m_buttons[index]->setPrimaryText(primary);
        m_buttons[index]->setAlternateText(alternate);
        m_buttons[index]->setHasAlternateFunction(hasAlternateFunction);
    }
}

QString ButtonRowPopup::buttonLabel(int index) const {
    if (index >= 0 && index < m_buttons.size()) {
        return m_buttons[index]->primaryText();
    }
    return QString();
}

QString ButtonRowPopup::buttonAlternateLabel(int index) const {
    if (index >= 0 && index < m_buttons.size()) {
        return m_buttons[index]->alternateText();
    }
    return QString();
}
