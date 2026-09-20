#include "ui/keyingweightpopup.h"
#include "ui/k4styles.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

KeyingWeightPopupWidget::KeyingWeightPopupWidget(QWidget *parent) : K4PopupBase(parent) {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(contentMargins());
    layout->setSpacing(8);

    auto *title = new QLabel("KEYING WEIGHT", this);
    title->setFixedSize(140, 36);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: 600;")
                             .arg(K4Styles::Colors::TextWhite));

    m_valueLabel = new QLabel(this);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    m_valueLabel->setFixedSize(80, 36);
    m_valueLabel->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: 600;")
                                    .arg(K4Styles::Colors::AccentAmber));

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
    auto *close = makeButton(QString::fromUtf8("\xE2\x86\xA9"));

    layout->addWidget(title);
    layout->addWidget(m_valueLabel);
    layout->addWidget(minus);
    layout->addWidget(plus);
    layout->addWidget(close);

    connect(minus, &QPushButton::clicked, this, [this]() { adjustWeight(-5); });
    connect(plus, &QPushButton::clicked, this, [this]() { adjustWeight(5); });
    connect(close, &QPushButton::clicked, this, &K4PopupBase::hidePopup);

    updateValue();
    initPopup();
}

QSize KeyingWeightPopupWidget::contentSize() const {
    return QSize(440, 52);
}

void KeyingWeightPopupWidget::setWeight(int weight) {
    m_weight = qBound(90, weight, 125);
    updateValue();
}

void KeyingWeightPopupWidget::adjustWeight(int delta) {
    const int weight = qBound(90, m_weight + delta, 125);
    if (weight == m_weight)
        return;
    m_weight = weight;
    updateValue();
    emit weightChanged(weight);
}

void KeyingWeightPopupWidget::updateValue() {
    m_valueLabel->setText(QString::number(m_weight / 100.0, 'f', 2));
}
