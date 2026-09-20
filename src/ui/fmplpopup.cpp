#include "ui/fmplpopup.h"
#include "ui/k4styles.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace {
const char *const PlTones[] = {
    "67.0", "69.3", "71.9", "74.4", "77.0", "79.7", "82.5", "85.4", "88.5", "91.5",
    "94.8", "97.4", "100.0", "103.5", "107.2", "110.9", "114.8", "118.8", "123.0", "127.3",
    "131.8", "136.5", "141.3", "146.2", "151.4", "156.7", "159.8", "162.2", "165.5", "167.9",
    "171.3", "173.8", "177.3", "179.9", "183.5", "186.2", "189.9", "192.8", "196.6", "199.5",
    "203.5", "206.5", "210.7", "218.1", "225.7", "229.1", "233.6", "241.8", "250.3", "254.1"
};
}

FmPlPopupWidget::FmPlPopupWidget(QWidget *parent) : K4PopupBase(parent) {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(contentMargins());
    layout->setSpacing(8);
    auto *title = new QLabel("FM PL TONE", this);
    title->setFixedSize(120, 36);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QString("color:%1;font-size:14px;font-weight:600;").arg(K4Styles::Colors::TextWhite));
    m_toneLabel = new QLabel(this);
    m_toneLabel->setAlignment(Qt::AlignCenter);
    m_toneLabel->setFixedSize(100, 36);
    m_toneLabel->setStyleSheet(QString("color:%1;font-size:16px;font-weight:600;").arg(K4Styles::Colors::AccentAmber));
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
    m_enableButton = makeButton("OFF");
    auto *close = makeButton(QString::fromUtf8("\xE2\x86\xA9"));
    layout->addWidget(title);
    layout->addWidget(m_toneLabel);
    layout->addWidget(minus);
    layout->addWidget(plus);
    layout->addWidget(m_enableButton);
    layout->addWidget(close);
    connect(minus, &QPushButton::clicked, this, [this]() { adjustTone(-1); });
    connect(plus, &QPushButton::clicked, this, [this]() { adjustTone(1); });
    connect(m_enableButton, &QPushButton::clicked, this, [this]() {
        m_enabled = !m_enabled;
        updateDisplay();
        emit toneChanged(m_index, m_enabled);
    });
    connect(close, &QPushButton::clicked, this, &K4PopupBase::hidePopup);
    updateDisplay();
    initPopup();
}

QSize FmPlPopupWidget::contentSize() const { return QSize(500, 52); }

void FmPlPopupWidget::setTone(int index, bool enabled) {
    m_index = qBound(1, index, 50);
    m_enabled = enabled;
    updateDisplay();
}

void FmPlPopupWidget::adjustTone(int delta) {
    const int next = qBound(1, m_index + delta, 50);
    if (next == m_index) return;
    m_index = next;
    updateDisplay();
    emit toneChanged(m_index, m_enabled);
}

void FmPlPopupWidget::updateDisplay() {
    m_toneLabel->setText(QString("%1 Hz").arg(PlTones[m_index - 1]));
    m_enableButton->setText(m_enabled ? "ON" : "OFF");
}
