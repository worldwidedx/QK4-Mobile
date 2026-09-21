#include "vforowwidget.h"
#include "filterindicatorwidget.h"
#include "k4styles.h"
#include <QHBoxLayout>
#include <QResizeEvent>

// ============== VfoSquareWidget Implementation ==============

VfoSquareWidget::VfoSquareWidget(const QString &text, const QColor &color, QWidget *parent)
    : QWidget(parent), m_text(text), m_color(color) {
    // Size includes square + lock-arc headroom
    setFixedSize(K4Styles::Dimensions::VfoSquareWidgetSize, K4Styles::Dimensions::VfoSquareWidgetTotalHeight);
    setCursor(Qt::PointingHandCursor);
}

void VfoSquareWidget::setLocked(bool locked) {
    if (m_locked != locked) {
        m_locked = locked;
        update();
    }
}

void VfoSquareWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int squareSize = K4Styles::Dimensions::VfoSquareWidgetSize;
    const int arcHeight = qMax(0, K4Styles::Dimensions::VfoSquareWidgetTotalHeight - squareSize);
    const int borderRadius = 4;

    // Draw the rounded square (offset down by arcHeight)
    QRectF squareRect(0, arcHeight, squareSize, squareSize);
    p.setBrush(m_color);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(squareRect, borderRadius, borderRadius);

    // Draw "A" or "B" text
    p.setPen(QColor(K4Styles::Colors::DarkBackground));
    QFont font;
    font.setPixelSize(K4Styles::Dimensions::FontSizeTitle);
    font.setBold(true);
    p.setFont(font);
    p.drawText(squareRect, Qt::AlignCenter, m_text);

    // Draw lock arc if locked (creates padlock shackle effect)
    if (m_locked) {
        QPen arcPen(m_color, 4, Qt::SolidLine, Qt::RoundCap);
        p.setPen(arcPen);
        p.setBrush(Qt::NoBrush);

        // Arc rect: centered horizontally, connects to top of square
        // Arc should look like the top of a padlock
        int arcWidth = 18;
        int arcX = (squareSize - arcWidth) / 2;
        QRectF arcRect(arcX, 0, arcWidth, arcHeight * 2);
        // Draw top half of ellipse (180 degrees starting from 0)
        p.drawArc(arcRect, 0, 180 * 16);
    }
}

// ============== VfoRowWidget Implementation ==============

VfoRowWidget::VfoRowWidget(QWidget *parent) : QWidget(parent) {
    setupWidgets();
    recomputeHeight();
}

void VfoRowWidget::recomputeHeight() {
    // Tall enough for the tallest of the three columns: the A/B square + mode +
    // filter stacks, and the centre column (TX glyph + any SPLIT/MSG/RIT stack).
    m_vfoAContainer->adjustSize();
    m_vfoBContainer->adjustSize();
    m_txContainer->adjustSize();
    const int stacked = qMax(m_txContainer->sizeHint().height(),
                             qMax(m_vfoAContainer->sizeHint().height(),
                                  m_vfoBContainer->sizeHint().height()));
    setFixedHeight(qMax(K4Styles::Dimensions::VfoRowHeight, stacked));
}

void VfoRowWidget::addToCenterColumn(QWidget *w) {
    if (!m_txColumn)
        return;
    m_txColumn->addWidget(w, 0, Qt::AlignHCenter);
    recomputeHeight();
    positionWidgets();
}

void VfoRowWidget::setLockA(bool locked) {
    m_vfoASquare->setLocked(locked);
}

void VfoRowWidget::setLockB(bool locked) {
    m_vfoBSquare->setLocked(locked);
}

void VfoRowWidget::setupWidgets() {
    // No layout manager - we use absolute positioning
    // All containers are children of this widget
    // === VFO A Container (square + mode label) ===
    // Regular/tablet: the VFO column is as wide as the filter indicator that
    // sits under it (filter indicators moved here from beside RIT/XIT). Phone
    // is unchanged from v1.0.5 -- no filter indicator in this container, and
    // the column is only as wide as the square itself.
    const bool compact = K4Styles::isCompactLayout();
    const int filterW = 62;
    const int vfoColWidth = compact ? K4Styles::Dimensions::VfoSquareSize : filterW;
    m_vfoAContainer = new QWidget(this);
    m_vfoAContainer->setFixedWidth(vfoColWidth);
    auto *vfoAColumn = new QVBoxLayout(m_vfoAContainer);
    vfoAColumn->setContentsMargins(0, 0, 0, 0);
    vfoAColumn->setSpacing(compact ? 2 : 1);

    m_vfoASquare = new VfoSquareWidget("A", QColor(K4Styles::Colors::VfoACyan), m_vfoAContainer);
    vfoAColumn->addWidget(m_vfoASquare, 0, Qt::AlignHCenter);

    m_modeALabel = new QLabel("USB", m_vfoAContainer);
    m_modeALabel->setFixedWidth(vfoColWidth);
    m_modeALabel->setAlignment(Qt::AlignCenter);
    m_modeALabel->setCursor(Qt::PointingHandCursor);
    m_modeALabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                    .arg(K4Styles::Colors::TextWhite)
                                    .arg(K4Styles::Dimensions::FontSizeLarge));
    vfoAColumn->addWidget(m_modeALabel, 0, Qt::AlignHCenter);

    if (!compact) {
        // VFO A filter indicator, directly under the square+mode (like the
        // radio). On the phone the filter indicator stays in MainWindow,
        // flanking the RIT/XIT box, exactly as in v1.0.5.
        m_filterAWidget = new FilterIndicatorWidget(m_vfoAContainer);
        vfoAColumn->addWidget(m_filterAWidget, 0, Qt::AlignHCenter);
    }

    // === TX Container (TEST label + triangles + TX) ===
    m_txContainer = new QWidget(this);
    auto *txVLayout = new QVBoxLayout(m_txContainer);
    txVLayout->setContentsMargins(0, 0, 0, 0);
    txVLayout->setSpacing(0);
    m_txColumn = txVLayout; // widgets stacked here sit under the TX glyph

    // TEST indicator - hidden by default
    // TEST is positioned independently from the TX container below. Keeping it
    // out of the TX layout ensures its adjustment cannot move the TX glyph.
    m_testLabel = new QLabel("TEST", this);
    m_testLabel->setAlignment(Qt::AlignCenter);
    m_testLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                   .arg(K4Styles::Colors::TxRed)
                                   .arg(K4Styles::Dimensions::FontSizePopup));
    m_testLabel->setVisible(false);

    // TX row (triangles + TX label)
    // Stretches keep the TX glyph centred when the column widens to hold the
    // SPLIT/MSG/RIT stack beneath it.
    auto *txIndicatorRow = new QHBoxLayout();
    txIndicatorRow->setSpacing(0);
    txIndicatorRow->addStretch();

    m_txTriangle = new QLabel(QString::fromUtf8("\u25C0"), m_txContainer); //◀
    m_txTriangle->setFixedSize(K4Styles::Dimensions::ButtonHeightMini, K4Styles::Dimensions::ButtonHeightMini);
    m_txTriangle->setAlignment(Qt::AlignCenter);
    m_txTriangle->setStyleSheet(QString("color: %1; font-size: 18px;").arg(K4Styles::Colors::AccentAmber));
    txIndicatorRow->addWidget(m_txTriangle);

    m_txIndicator = new QLabel("TX", m_txContainer);
    m_txIndicator->setStyleSheet(
        QString("color: %1; font-size: 18px; font-weight: bold;").arg(K4Styles::Colors::AccentAmber));
    txIndicatorRow->addWidget(m_txIndicator);

    m_txTriangleB = new QLabel("", m_txContainer); // Empty by default
    m_txTriangleB->setFixedSize(K4Styles::Dimensions::ButtonHeightMini, K4Styles::Dimensions::ButtonHeightMini);
    m_txTriangleB->setAlignment(Qt::AlignCenter);
    m_txTriangleB->setStyleSheet(QString("color: %1; font-size: 18px;").arg(K4Styles::Colors::AccentAmber));
    txIndicatorRow->addWidget(m_txTriangleB);
    txIndicatorRow->addStretch();

    txVLayout->addLayout(txIndicatorRow);

    // Adjust size to fit content
    m_txContainer->adjustSize();

    // === VFO B Container (square + mode label) ===
    m_vfoBContainer = new QWidget(this);
    m_vfoBContainer->setFixedWidth(vfoColWidth);
    auto *vfoBColumn = new QVBoxLayout(m_vfoBContainer);
    vfoBColumn->setContentsMargins(0, 0, 0, 0);
    vfoBColumn->setSpacing(compact ? 2 : 1);

    m_vfoBSquare = new VfoSquareWidget("B", QColor(K4Styles::Colors::VfoBGreen), m_vfoBContainer);
    vfoBColumn->addWidget(m_vfoBSquare, 0, Qt::AlignHCenter);

    m_modeBLabel = new QLabel("USB", m_vfoBContainer);
    m_modeBLabel->setFixedWidth(vfoColWidth);
    m_modeBLabel->setAlignment(Qt::AlignCenter);
    m_modeBLabel->setCursor(Qt::PointingHandCursor);
    m_modeBLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                    .arg(K4Styles::Colors::TextWhite)
                                    .arg(K4Styles::Dimensions::FontSizeLarge));
    vfoBColumn->addWidget(m_modeBLabel, 0, Qt::AlignHCenter);

    if (!compact) {
        // VFO B filter indicator, directly under the square+mode.
        m_filterBWidget = new FilterIndicatorWidget(m_vfoBContainer);
        vfoBColumn->addWidget(m_filterBWidget, 0, Qt::AlignHCenter);
    }

    // === SUB/DIV Container ===
    m_subDivContainer = new QWidget(this);
    auto *subDivStack = new QVBoxLayout(m_subDivContainer);
    subDivStack->setSpacing(4);
    subDivStack->setContentsMargins(0, 0, 0, 0);

    m_subLabel = new QLabel("SUB", m_subDivContainer);
    m_subLabel->setAlignment(Qt::AlignCenter);
    m_subLabel->setFixedSize(K4Styles::Dimensions::VfoSubDivLabelWidth, K4Styles::Dimensions::VfoSubDivLabelHeight);
    m_subLabel->setStyleSheet(QString("background-color: %1;"
                                      "color: %2;"
                                      "font-size: %3px;"
                                      "font-weight: bold;"
                                      "border-radius: 2px;")
                                  .arg(K4Styles::Colors::DisabledBackground)
                                  .arg(K4Styles::Colors::LightGradientTop)
                                  .arg(K4Styles::Dimensions::FontSizeNormal));
    subDivStack->addWidget(m_subLabel);

    m_divLabel = new QLabel("DIV", m_subDivContainer);
    m_divLabel->setAlignment(Qt::AlignCenter);
    m_divLabel->setFixedSize(K4Styles::Dimensions::VfoSubDivLabelWidth, K4Styles::Dimensions::VfoSubDivLabelHeight);
    m_divLabel->setStyleSheet(QString("background-color: %1;"
                                      "color: %2;"
                                      "font-size: %3px;"
                                      "font-weight: bold;"
                                      "border-radius: 2px;")
                                  .arg(K4Styles::Colors::DisabledBackground)
                                  .arg(K4Styles::Colors::LightGradientTop)
                                  .arg(K4Styles::Dimensions::FontSizeNormal));
    subDivStack->addWidget(m_divLabel);

    m_subDivContainer->adjustSize();
    if (!compact) {
        // Regular/tablet: not shown in the centre VFO area (the radio shows
        // them as LEDs on the right panel instead, reflecting the same
        // state). Phone keeps its original at-a-glance SUB/DIV indicators.
        m_subDivContainer->hide();
    }
}

void VfoRowWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    positionWidgets();
}

void VfoRowWidget::positionWidgets() {
    int w = width();
    int centerX = w / 2;
    int y = 0; // Top of row

    // TX container - centered at widget center, offset down to align with squares
    m_txContainer->adjustSize();
    int txWidth = m_txContainer->width();
    m_testLabel->adjustSize();
    // The original TX row began after TEST's layout height. Preserve that TX
    // position while lifting only TEST toward the top of the VFO row.
    int txY = m_testLabel->height();
    m_txContainer->move(centerX - txWidth / 2, txY);
    m_testLabel->move(centerX - m_testLabel->width() / 2, 2);

    // A container - left of TX with gap
    int gap = K4Styles::Dimensions::PaddingLarge;
    m_vfoAContainer->move(centerX - txWidth / 2 - gap - m_vfoAContainer->width(), y);

    // B container - right of TX with gap (symmetric with A)
    m_vfoBContainer->move(centerX + txWidth / 2 + gap, y);

    // SUB/DIV - to right of B (doesn't affect centering), offset down to align
    m_subDivContainer->move(m_vfoBContainer->x() + m_vfoBContainer->width() + K4Styles::Dimensions::PaddingMedium, txY);
}
