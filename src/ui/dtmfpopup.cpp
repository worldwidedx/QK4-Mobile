#include "ui/dtmfpopup.h"
#include "ui/k4styles.h"
#include "settings/radiosettings.h"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

DtmfPopupWidget::DtmfPopupWidget(QWidget *parent) : K4PopupBase(parent) {
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(contentMargins());
    grid->setHorizontalSpacing(7);
    grid->setVerticalSpacing(6);

    auto *title = new QLabel("DTMF", this);
    title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    title->setStyleSheet(QString("color:%1;font-size:18px;font-weight:bold;")
                             .arg(K4Styles::Colors::TextWhite));
    grid->addWidget(title, 0, 0, 1, 3);

    const QStringList keys = {"1", "2", "3", "A", "4", "5", "6", "B",
                              "7", "8", "9", "C", "*", "0", "#", "D"};
    for (int i = 0; i < keys.size(); ++i) {
        auto *button = new QPushButton(keys.at(i), this);
        button->setMinimumSize(68, 48);
        button->setStyleSheet(K4Styles::menuBarButtonSmall());
        grid->addWidget(button, i / 4, 4 + i % 4);
        connect(button, &QPushButton::clicked, this, [this, key = keys.at(i)]() {
            if (m_editingCommand >= 0) {
                if (m_editSequence.size() < 32) m_editSequence.append(key);
                m_status->setText(QString("CMD%1: %2").arg(m_editingCommand + 1).arg(m_editSequence));
            } else {
                emit digitRequested(key.at(0));
            }
        });
    }
    m_status = new QLabel("Hold CMD1-CMD6 to program", this);
    m_status->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_status->setMinimumHeight(44);
    m_status->setStyleSheet(QString("background:%1;color:%2;border:1px solid %3;"
                                    "border-radius:4px;padding:4px;font-size:13px;")
                                .arg(K4Styles::Colors::DarkBackground,
                                     K4Styles::Colors::AccentAmber,
                                     K4Styles::Colors::DialogBorder));
    grid->addWidget(m_status, 3, 0, 1, 3);
    for (int i = 0; i < 6; ++i) {
        auto *button = new QPushButton(this);
        button->setMinimumSize(82, 48);
        button->setStyleSheet(K4Styles::menuBarButtonSmall());
        m_commandButtons[i] = button;
        grid->addWidget(button, 1 + i / 3, i % 3);
        auto *hold = new QTimer(button);
        hold->setSingleShot(true);
        hold->setInterval(600);
        connect(button, &QPushButton::pressed, button, [button, hold]() {
            button->setProperty("dtmfLongPressed", false); hold->start();
        });
        connect(hold, &QTimer::timeout, button, [this, i, button]() {
            button->setProperty("dtmfLongPressed", true); beginCommandEdit(i);
        });
        connect(button, &QPushButton::released, button, [this, i, button, hold]() {
            hold->stop();
            if (!button->property("dtmfLongPressed").toBool() && m_editingCommand < 0) {
                const QString sequence = RadioSettings::instance()->dtmfCommand(i);
                if (!sequence.isEmpty()) emit sequenceRequested(sequence);
            }
        });
    }
    auto *returnButton = new QPushButton(QString::fromUtf8("\xE2\x86\xA9"), this);
    returnButton->setMinimumSize(68, 48);
    returnButton->setStyleSheet(K4Styles::menuBarButtonSmall());
    grid->addWidget(returnButton, 3, 3);
    connect(returnButton, &QPushButton::clicked, this, [this]() {
        if (m_editingCommand >= 0) finishCommandEdit(); else hidePopup();
    });
    updateCommandButtons();
    initPopup();
}

QSize DtmfPopupWidget::contentSize() const { return QSize(650, 235); }

void DtmfPopupWidget::beginCommandEdit(int index) {
    m_editingCommand = index;
    m_editSequence.clear();
    m_status->setText(QString("CMD%1: enter sequence, then tap return").arg(index + 1));
}

void DtmfPopupWidget::finishCommandEdit() {
    RadioSettings::instance()->setDtmfCommand(m_editingCommand, m_editSequence);
    m_editingCommand = -1;
    m_status->setText("Saved. Hold CMD1-CMD6 to program");
    updateCommandButtons();
}

void DtmfPopupWidget::updateCommandButtons() {
    for (int i = 0; i < 6; ++i) {
        const QString sequence = RadioSettings::instance()->dtmfCommand(i);
        m_commandButtons[i]->setText(sequence.isEmpty() ? QString("CMD%1").arg(i + 1)
                                                        : QString("CMD%1\n%2").arg(i + 1).arg(sequence.left(8)));
    }
}
