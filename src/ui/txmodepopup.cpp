#include "ui/txmodepopup.h"
#include "ui/k4styles.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

TxModePopupWidget::TxModePopupWidget(QWidget *parent) : K4PopupBase(parent) {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(contentMargins());
    layout->setSpacing(8);
    m_title = new QLabel(this);
    m_title->setFixedSize(120, 36);
    m_title->setAlignment(Qt::AlignCenter);
    m_title->setStyleSheet(QString("color:%1;font-size:14px;font-weight:600;").arg(K4Styles::Colors::TextWhite));
    m_valueLabel = new QLabel(this);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    m_valueLabel->setFixedSize(100, 36);
    m_valueLabel->setStyleSheet(QString("color:%1;font-size:16px;font-weight:600;").arg(K4Styles::Colors::AccentAmber));
    auto makeButton = [this](const QString &text) {
        auto *button = new QPushButton(text, this);
        button->setFixedSize(text == QString::fromUtf8("\xE2\x86\xA9")
                                 ? K4Styles::Dimensions::NavButtonWidth
                                 : K4Styles::Dimensions::ButtonHeightLarge,
                             K4Styles::Dimensions::ButtonHeightMedium);
        button->setStyleSheet(K4Styles::menuBarButtonSmall());
        return button;
    };
    auto *minus = makeButton("-");
    auto *plus = makeButton("+");
    m_modeButton = makeButton("S");
    auto *close = makeButton(QString::fromUtf8("\xE2\x86\xA9"));
    layout->addWidget(m_title);
    layout->addWidget(m_valueLabel);
    layout->addWidget(minus);
    layout->addWidget(plus);
    layout->addWidget(m_modeButton);
    layout->addWidget(close);
    connect(minus, &QPushButton::clicked, this, [this]() { adjust(-1); });
    connect(plus, &QPushButton::clicked, this, [this]() { adjust(1); });
    connect(m_modeButton, &QPushButton::clicked, this, [this]() {
        if (m_editor != FmRepeater) return;
        m_repeaterMode = m_repeaterMode == 'S' ? QChar('+') : (m_repeaterMode == '+' ? QChar('-') : QChar('S'));
        updateDisplay();
        emit repeaterChanged(m_repeaterMode, m_value);
    });
    connect(close, &QPushButton::clicked, this, &K4PopupBase::hidePopup);
    initPopup();
}

QSize TxModePopupWidget::contentSize() const { return QSize(500, 52); }

void TxModePopupWidget::showDataBandwidth(int tenthsKhz) {
    m_editor = DataBandwidth;
    m_value = qBound(20, tenthsKhz, 40);
    m_modeButton->hide();
    updateDisplay();
}

void TxModePopupWidget::showFmRepeater(QChar mode, int offsetKhz, bool editOffset) {
    m_editor = FmRepeater;
    m_repeaterMode = QString("S+-").contains(mode) ? mode : QChar('S');
    m_value = qBound(0, offsetKhz, 99999);
    m_editOffset = editOffset;
    m_modeButton->setVisible(!editOffset);
    updateDisplay();
}

void TxModePopupWidget::adjust(int delta) {
    if (m_editor == DataBandwidth) {
        m_value = qBound(20, m_value + delta, 40);
        emit dataBandwidthChanged(m_value);
    } else if (m_editOffset) {
        // K4 RPT OFS is stored in kHz. 100 kHz steps are touch-friendly and
        // cover the standard repeater offsets without a keyboard.
        m_value = qBound(0, m_value + delta * 100, 99999);
        emit repeaterChanged(m_repeaterMode, m_value);
    } else {
        m_repeaterMode = delta > 0
            ? (m_repeaterMode == 'S' ? QChar('+') : (m_repeaterMode == '+' ? QChar('-') : QChar('S')))
            : (m_repeaterMode == 'S' ? QChar('-') : (m_repeaterMode == '-' ? QChar('+') : QChar('S')));
        emit repeaterChanged(m_repeaterMode, m_value);
    }
    updateDisplay();
}

void TxModePopupWidget::updateDisplay() {
    if (m_editor == DataBandwidth) {
        m_title->setText("DATA BW");
        m_valueLabel->setText(QString("%1 kHz").arg(m_value / 10.0, 0, 'f', 1));
    } else {
        m_title->setText(m_editOffset ? "RPT OFS" : "RPT");
        m_valueLabel->setText(m_editOffset ? QString("%1 kHz").arg(m_value) :
                                               (m_repeaterMode == 'S' ? QStringLiteral("SIMPLEX") : QString(m_repeaterMode)));
        m_modeButton->setText(QString(m_repeaterMode));
    }
}
