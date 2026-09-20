#include "sidecontrolpanel.h"
#include "dualcontrolbutton.h"
#include "duallinepanelbutton.h"
#include "k4styles.h"
#include "../settings/radiosettings.h"
#include <QVBoxLayout>
#include <QSizePolicy>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSpacerItem>
#include <QEvent>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyle>

SideControlPanel::SideControlPanel(QWidget *parent) : QWidget(parent) {
    m_longPressTimer = new QTimer(this);
    m_longPressTimer->setSingleShot(true);
    m_longPressTimer->setInterval(550);
    connect(m_longPressTimer, &QTimer::timeout, this, [this]() {
        if (!m_longPressTarget)
            return;
        m_longPressHandled = true;
        triggerSecondary(m_longPressTarget);
    });
    setupUi();
}

void SideControlPanel::selectAdjustment(Adjustment adjustment) {
    const auto setPrimary = [this](DualControlButton *button, QSlider *slider,
                                   bool &isPrimary, bool wantPrimary) {
        if (isPrimary != wantPrimary) {
            button->swapFunctions();
            isPrimary = wantPrimary;
        }
        configureAdjustmentSlider(button, slider);
    };

    switch (adjustment) {
    case Adjustment::MainVolume:
    case Adjustment::SubVolume:
        break;
    case Adjustment::CwSpeed:
        if (m_isCWMode) {
            setPrimary(m_wpmBtn, m_wpmSlider, m_wpmIsPrimary, true);
            setGroup1Active(m_wpmBtn);
        }
        break;
    case Adjustment::RfPower:
        setPrimary(m_pwrBtn, m_pwrSlider, m_pwrIsPrimary, true);
        setGroup1Active(m_pwrBtn);
        break;
    case Adjustment::FilterBandwidth:
    case Adjustment::FilterShift:
        if (!m_bwIsPrimary) {
            m_bwBtn->swapFunctions();
            m_shiftBtn->swapFunctions();
            m_bwIsPrimary = true;
            m_shiftIsPrimary = true;
        }
        configureAdjustmentSlider(m_bwBtn, m_bwSlider);
        configureAdjustmentSlider(m_shiftBtn, m_shiftSlider);
        setGroup2Active(adjustment == Adjustment::FilterBandwidth ? m_bwBtn : m_shiftBtn);
        break;
    case Adjustment::MainRfGain:
        setPrimary(m_mainRfBtn, m_mainRfSlider, m_mainRfIsPrimary, true);
        setGroup3Active(m_mainRfBtn);
        break;
    case Adjustment::MainSquelch:
        setPrimary(m_mainRfBtn, m_mainRfSlider, m_mainRfIsPrimary, false);
        setGroup3Active(m_mainRfBtn);
        break;
    case Adjustment::SubSquelch:
        setPrimary(m_subSqlBtn, m_subSqlSlider, m_subSqlIsPrimary, true);
        setGroup3Active(m_subSqlBtn);
        break;
    case Adjustment::SubRfGain:
        setPrimary(m_subSqlBtn, m_subSqlSlider, m_subSqlIsPrimary, false);
        setGroup3Active(m_subSqlBtn);
        break;
    }
}

QWidget *SideControlPanel::adjustmentWidget(Adjustment adjustment) const {
    switch (adjustment) {
    case Adjustment::MainVolume: return m_volumeSlider;
    case Adjustment::SubVolume: return m_subVolumeSlider;
    case Adjustment::CwSpeed: return m_isCWMode ? m_wpmBtn : nullptr;
    case Adjustment::RfPower: return m_pwrBtn;
    case Adjustment::FilterBandwidth: return m_bwBtn;
    case Adjustment::FilterShift: return m_shiftBtn;
    case Adjustment::MainRfGain:
    case Adjustment::MainSquelch: return m_mainRfBtn;
    case Adjustment::SubSquelch:
    case Adjustment::SubRfGain: return m_subSqlBtn;
    }
    return nullptr;
}

void SideControlPanel::setupUi() {
    setFixedWidth(K4Styles::Dimensions::SidePanelWidth);
    QPalette panelPalette = palette();
    panelPalette.setColor(QPalette::Window, QColor(K4Styles::Colors::PopupBackground));
    setPalette(panelPalette);
    setAutoFillBackground(true);
    // Note: No explicit size policy - let Qt handle vertical expansion like RightSidePanel

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(K4Styles::Dimensions::PaddingSmall, K4Styles::Dimensions::PopupButtonSpacing,
                               K4Styles::Dimensions::PaddingSmall, K4Styles::Dimensions::PopupButtonSpacing);
    layout->setSpacing(4); // Default spacing between buttons in a group

    auto addAdjustmentRow = [this, layout](DualControlButton *button, QSlider *&slider, const QString &color) {
        auto *row = new QWidget(this);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(6);
        rowLayout->addWidget(button);
        slider = new QSlider(Qt::Horizontal, row);
        slider->setMinimumHeight(32);
        slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        slider->setStyleSheet(K4Styles::sliderHorizontal(K4Styles::Colors::DarkBackground, color));
        slider->installEventFilter(this);
        rowLayout->addWidget(slider, 1);
        layout->addWidget(row);
    };

    // ===== Receiver AF controls: always first in the phone CTRL bank =====
    m_volumeLabel = new QLabel("A AF", this);
    m_volumeLabel->setStyleSheet(
        QString("color: %1; font-size: 10px; font-weight: bold;").arg(K4Styles::Colors::VfoACyan));
    m_volumeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_volumeLabel);

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(RadioSettings::instance()->volume());
    m_volumeSlider->setMinimumHeight(K4Styles::isCompactLayout() ? 32 : 24);
    m_volumeSlider->setStyleSheet(
        K4Styles::sliderHorizontal(K4Styles::Colors::DarkBackground, K4Styles::Colors::VfoACyan));
    m_volumeSlider->installEventFilter(this);
    layout->addWidget(m_volumeSlider);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &SideControlPanel::volumeChanged);

    m_subVolumeLabel = new QLabel("B AF", this);
    m_subVolumeLabel->setStyleSheet(
        QString("color: %1; font-size: 10px; font-weight: bold;").arg(K4Styles::Colors::VfoBGreen));
    m_subVolumeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_subVolumeLabel);

    m_subVolumeSlider = new QSlider(Qt::Horizontal, this);
    m_subVolumeSlider->setRange(0, 100);
    m_subVolumeSlider->setValue(RadioSettings::instance()->subVolume());
    m_subVolumeSlider->setMinimumHeight(K4Styles::isCompactLayout() ? 32 : 24);
    m_subVolumeSlider->setStyleSheet(
        K4Styles::sliderHorizontal(K4Styles::Colors::DarkBackground, K4Styles::Colors::VfoBGreen));
    m_subVolumeSlider->installEventFilter(this);
    layout->addWidget(m_subVolumeSlider);
    connect(m_subVolumeSlider, &QSlider::valueChanged, this, &SideControlPanel::subVolumeChanged);

    // Local input gain is intentionally separate from the K4 MIC control.
    // It scales the phone/headset microphone stream before Opus encoding.
    m_phoneMicGainLabel = new QLabel("PHONE MIC", this);
    m_phoneMicGainLabel->setStyleSheet(
        QString("color: %1; font-size: 10px; font-weight: bold;").arg(K4Styles::Colors::AccentAmber));
    m_phoneMicGainLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_phoneMicGainLabel);

    m_phoneMicGainSlider = new QSlider(Qt::Horizontal, this);
    m_phoneMicGainSlider->setRange(0, 100);
    m_phoneMicGainSlider->setValue(RadioSettings::instance()->micGain());
    m_phoneMicGainSlider->setMinimumHeight(K4Styles::isCompactLayout() ? 32 : 24);
    m_phoneMicGainSlider->setStyleSheet(
        K4Styles::sliderHorizontal(K4Styles::Colors::DarkBackground, K4Styles::Colors::AccentAmber));
    m_phoneMicGainSlider->installEventFilter(this);
    layout->addWidget(m_phoneMicGainSlider);
    connect(m_phoneMicGainSlider, &QSlider::valueChanged, this, &SideControlPanel::phoneMicGainChanged);

    layout->addSpacing(K4Styles::Dimensions::PaddingMedium);

    // ===== TX Function Buttons (2x3 grid) =====
    auto *txGrid = new QGridLayout();
    txGrid->setContentsMargins(0, 0, 0, 0);
    txGrid->setHorizontalSpacing(K4Styles::Dimensions::PopupButtonSpacing);
    txGrid->setVerticalSpacing(K4Styles::Dimensions::PopupButtonSpacing);

    // Row 0: TUNE, XMIT
    txGrid->addWidget(createTxFunctionButton("TUNE", "TUNE LP", m_tuneBtn), 0, 0);
    txGrid->addWidget(createTxFunctionButton("XMIT", "TEST", m_xmitBtn), 0, 1);

    // Row 1: ATU TUNE, VOX
    txGrid->addWidget(createTxFunctionButton("ATU\nTUNE", "ATU", m_atuTuneBtn), 1, 0);
    txGrid->addWidget(createTxFunctionButton("VOX", "QSK", m_voxBtn), 1, 1);

    // Row 2: ANT, RX ANT
    txGrid->addWidget(createTxFunctionButton("ANT", "REM ANT", m_antBtn), 2, 0);
    txGrid->addWidget(createTxFunctionButton("RX ANT", "SUB ANT", m_rxAntBtn), 2, 1);

    layout->addLayout(txGrid);

    // ===== Connect TX function button signals and install event filters =====
    // Left-click signals
    connect(m_tuneBtn, &QPushButton::clicked, this, &SideControlPanel::tuneClicked);
    connect(m_xmitBtn, &QPushButton::clicked, this, &SideControlPanel::xmitClicked);
    connect(m_atuTuneBtn, &QPushButton::clicked, this, &SideControlPanel::atuTuneClicked);
    connect(m_voxBtn, &QPushButton::clicked, this, &SideControlPanel::voxClicked);
    connect(m_antBtn, &QPushButton::clicked, this, &SideControlPanel::antClicked);
    connect(m_rxAntBtn, &QPushButton::clicked, this, &SideControlPanel::rxAntClicked);

    // Install event filters for right-click handling
    m_tuneBtn->installEventFilter(this);
    m_xmitBtn->installEventFilter(this);
    m_atuTuneBtn->installEventFilter(this);
    m_voxBtn->installEventFilter(this);
    m_antBtn->installEventFilter(this);
    m_rxAntBtn->installEventFilter(this);

    // ===== Spacing after TX buttons =====
    layout->addSpacing(K4Styles::Dimensions::PaddingLarge);

    // ===== Group 1: Global (CW/Power) - Orange bar =====
    m_wpmBtn = new DualControlButton(this);
    m_wpmBtn->setPrimaryLabel("WPM");
    m_wpmBtn->setPrimaryValue("--");
    m_wpmBtn->setAlternateLabel("PTCH");
    m_wpmBtn->setAlternateValue("--");
    m_wpmBtn->setContext(DualControlButton::Global);
    m_wpmBtn->setShowIndicator(true); // First button in group is active by default
    addAdjustmentRow(m_wpmBtn, m_wpmSlider, K4Styles::Colors::AccentAmber);

    m_pwrBtn = new DualControlButton(this);
    m_pwrBtn->setPrimaryLabel("PWR");
    m_pwrBtn->setPrimaryValue("--");
    m_pwrBtn->setAlternateLabel("DLY");
    m_pwrBtn->setAlternateValue("--");
    m_pwrBtn->setContext(DualControlButton::Global);
    m_pwrBtn->setShowIndicator(false); // Second button starts inactive
    addAdjustmentRow(m_pwrBtn, m_pwrSlider, K4Styles::Colors::AccentAmber);

    // ===== Spacing between groups =====
    layout->addSpacing(K4Styles::Dimensions::PaddingLarge);

    // ===== Group 2: Filter (BW/Shift) - Cyan bar - LINKED PAIR =====
    m_bwBtn = new DualControlButton(this);
    m_bwBtn->setPrimaryLabel("BW");
    m_bwBtn->setPrimaryValue("--");
    m_bwBtn->setAlternateLabel("HI");
    m_bwBtn->setAlternateValue("--");
    m_bwBtn->setContext(DualControlButton::MainRx);
    m_bwBtn->setShowIndicator(true); // First button in group is active
    addAdjustmentRow(m_bwBtn, m_bwSlider, K4Styles::Colors::VfoACyan);

    m_shiftBtn = new DualControlButton(this);
    m_shiftBtn->setPrimaryLabel("SHFT");
    m_shiftBtn->setPrimaryValue("--");
    m_shiftBtn->setAlternateLabel("LO");
    m_shiftBtn->setAlternateValue("--");
    m_shiftBtn->setContext(DualControlButton::MainRx);
    m_shiftBtn->setShowIndicator(false); // Second button starts inactive
    addAdjustmentRow(m_shiftBtn, m_shiftSlider, K4Styles::Colors::VfoACyan);

    // NORM affects only the filter passband, so keep it in the filter group.
    m_normBtn = new QPushButton(QStringLiteral("NORM"), this);
    m_normBtn->setFixedHeight(32);
    m_normBtn->setStyleSheet(K4Styles::compactButton());
    m_normBtn->setAccessibleName(QStringLiteral("Normalize receive filter passband"));
    m_normBtn->setToolTip(QStringLiteral("Restore the current mode's nominal filter passband"));
    m_normBtn->installEventFilter(this);
    layout->addWidget(m_normBtn);
    connect(m_normBtn, &QPushButton::clicked, this, &SideControlPanel::normalizeFilterRequested);

    // ===== Spacing between groups =====
    layout->addSpacing(K4Styles::Dimensions::PaddingLarge);

    // ===== Group 3: RF/Squelch =====
    m_mainRfBtn = new DualControlButton(this);
    m_mainRfBtn->setPrimaryLabel("M.RF");
    m_mainRfBtn->setPrimaryValue("--");
    m_mainRfBtn->setAlternateLabel("M.SQL");
    m_mainRfBtn->setAlternateValue("--");
    m_mainRfBtn->setContext(DualControlButton::MainRx);
    m_mainRfBtn->setShowIndicator(true); // First button in group is active
    addAdjustmentRow(m_mainRfBtn, m_mainRfSlider, K4Styles::Colors::VfoACyan);

    m_subSqlBtn = new DualControlButton(this);
    m_subSqlBtn->setPrimaryLabel("S.SQL");
    m_subSqlBtn->setPrimaryValue("--");
    m_subSqlBtn->setAlternateLabel("S.RF");
    m_subSqlBtn->setAlternateValue("--");
    m_subSqlBtn->setContext(DualControlButton::SubRx);
    m_subSqlBtn->setShowIndicator(false); // Second button starts inactive
    addAdjustmentRow(m_subSqlBtn, m_subSqlSlider, K4Styles::Colors::VfoBGreen);

    // ===== Stretch to push status/icons to bottom =====
    layout->addStretch();

    // ===== Status Area (mirrors header data) =====
    m_timeLabel = new QLabel("00:00:00 Z", this);
    m_timeLabel->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::TextWhite));
    layout->addWidget(m_timeLabel);

    m_powerSwrLabel = new QLabel("0.0W  1.0:1", this);
    m_powerSwrLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(K4Styles::Colors::TextWhite));
    layout->addWidget(m_powerSwrLabel);

    m_voltageCurrentLabel = new QLabel("--.-V  -.-A", this);
    m_voltageCurrentLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(K4Styles::Colors::TextWhite));
    layout->addWidget(m_voltageCurrentLabel);

    layout->addSpacing(K4Styles::Dimensions::PopupButtonSpacing);

    // ===== Connect Group 1 signals (WPM/PWR) =====
    connect(m_wpmBtn, &DualControlButton::becameActive, this, &SideControlPanel::onWpmBecameActive);
    connect(m_pwrBtn, &DualControlButton::becameActive, this, &SideControlPanel::onPwrBecameActive);
    connect(m_wpmBtn, &DualControlButton::valueScrolled, this, &SideControlPanel::onWpmScrolled);
    connect(m_pwrBtn, &DualControlButton::valueScrolled, this, &SideControlPanel::onPwrScrolled);
    connect(m_wpmBtn, &DualControlButton::swapped, this, [this]() {
        m_wpmIsPrimary = !m_wpmIsPrimary; // Track swap
        configureAdjustmentSlider(m_wpmBtn, m_wpmSlider);
    });
    connect(m_pwrBtn, &DualControlButton::swapped, this, [this]() {
        m_pwrIsPrimary = !m_pwrIsPrimary; // Track swap
        configureAdjustmentSlider(m_pwrBtn, m_pwrSlider);
    });

    // ===== Connect Group 2 signals (BW/SHFT) - LINKED PAIR =====
    connect(m_bwBtn, &DualControlButton::becameActive, this, &SideControlPanel::onBwBecameActive);
    connect(m_shiftBtn, &DualControlButton::becameActive, this, &SideControlPanel::onShiftBecameActive);
    connect(m_bwBtn, &DualControlButton::valueScrolled, this, &SideControlPanel::onBwScrolled);
    connect(m_shiftBtn, &DualControlButton::valueScrolled, this, &SideControlPanel::onShiftScrolled);
    connect(m_bwBtn, &DualControlButton::swapped, this, &SideControlPanel::onBwClicked);
    connect(m_shiftBtn, &DualControlButton::swapped, this, &SideControlPanel::onShiftClicked);

    // ===== Connect Group 3 signals (MainRf/SubSql) =====
    connect(m_mainRfBtn, &DualControlButton::becameActive, this, &SideControlPanel::onMainRfBecameActive);
    connect(m_subSqlBtn, &DualControlButton::becameActive, this, &SideControlPanel::onSubSqlBecameActive);
    connect(m_mainRfBtn, &DualControlButton::valueScrolled, this, &SideControlPanel::onMainRfScrolled);
    connect(m_subSqlBtn, &DualControlButton::valueScrolled, this, &SideControlPanel::onSubSqlScrolled);
    connect(m_mainRfBtn, &DualControlButton::swapped, this, [this]() {
        m_mainRfIsPrimary = !m_mainRfIsPrimary; // Track swap
        configureAdjustmentSlider(m_mainRfBtn, m_mainRfSlider);
    });
    connect(m_subSqlBtn, &DualControlButton::swapped, this, [this]() {
        m_subSqlIsPrimary = !m_subSqlIsPrimary; // Track swap
        configureAdjustmentSlider(m_subSqlBtn, m_subSqlSlider);
    });

    // Phone interaction: a normal tap always selects the primary function;
    // holding the button selects its amber alternate (handled by
    // DualControlButton).  The adjacent slider follows that selection.
    connect(m_wpmBtn, &DualControlButton::clicked, this, [this]() {
        if (!m_wpmIsPrimary) {
            m_wpmBtn->swapFunctions();
            m_wpmIsPrimary = true;
        }
        configureAdjustmentSlider(m_wpmBtn, m_wpmSlider);
    });
    connect(m_pwrBtn, &DualControlButton::clicked, this, [this]() {
        if (!m_pwrIsPrimary) {
            m_pwrBtn->swapFunctions();
            m_pwrIsPrimary = true;
        }
        configureAdjustmentSlider(m_pwrBtn, m_pwrSlider);
    });
    auto selectPrimaryFilters = [this]() {
        if (!m_bwIsPrimary) {
            m_bwBtn->swapFunctions();
            m_shiftBtn->swapFunctions();
            m_bwIsPrimary = true;
            m_shiftIsPrimary = true;
        }
        configureAdjustmentSlider(m_bwBtn, m_bwSlider);
        configureAdjustmentSlider(m_shiftBtn, m_shiftSlider);
    };
    connect(m_bwBtn, &DualControlButton::clicked, this, selectPrimaryFilters);
    connect(m_shiftBtn, &DualControlButton::clicked, this, selectPrimaryFilters);
    connect(m_mainRfBtn, &DualControlButton::clicked, this, [this]() {
        if (!m_mainRfIsPrimary) {
            m_mainRfBtn->swapFunctions();
            m_mainRfIsPrimary = true;
        }
        configureAdjustmentSlider(m_mainRfBtn, m_mainRfSlider);
    });
    connect(m_subSqlBtn, &DualControlButton::clicked, this, [this]() {
        if (!m_subSqlIsPrimary) {
            m_subSqlBtn->swapFunctions();
            m_subSqlIsPrimary = true;
        }
        configureAdjustmentSlider(m_subSqlBtn, m_subSqlSlider);
    });

    auto connectSlider = [this](QSlider *slider, auto handler) {
        connect(slider, &QSlider::valueChanged, this, [slider, handler](int value) {
            const int previous = slider->property("lastRadioValue").toInt();
            slider->setProperty("lastRadioValue", value);
            const int delta = value - previous;
            if (delta != 0)
                handler(delta);
        });
    };
    connectSlider(m_wpmSlider, [this](int delta) { onWpmScrolled(delta); });
    connectSlider(m_pwrSlider, [this](int delta) { onPwrScrolled(delta); });
    connectSlider(m_bwSlider, [this](int delta) { onBwScrolled(delta); });
    connect(m_shiftSlider, &QSlider::valueChanged, this, [this](int value) {
        const int previous = m_shiftSlider->property("lastRadioValue").toInt();
        m_shiftSlider->setProperty("lastRadioValue", value);
        const int delta = value - previous;
        if (delta == 0)
            return;

        if (m_sliderDragTarget == m_shiftSlider && m_sliderAdjusting && m_shiftIsPrimary) {
            // Preview the exact native-Hz setting locally while the user is
            // dragging.  The radio is updated once, on release.
            m_shiftValue = value * 10;
            m_shiftBtn->setPrimaryValue(QString::number(m_shiftValue / 1000.0, 'f', 2));
            return;
        }
        onShiftScrolled(delta);
    });
    connectSlider(m_mainRfSlider, [this](int delta) { onMainRfScrolled(delta); });
    connectSlider(m_subSqlSlider, [this](int delta) { onSubSqlScrolled(delta); });

    configureAdjustmentSlider(m_wpmBtn, m_wpmSlider);
    configureAdjustmentSlider(m_pwrBtn, m_pwrSlider);
    configureAdjustmentSlider(m_bwBtn, m_bwSlider);
    configureAdjustmentSlider(m_shiftBtn, m_shiftSlider);
    configureAdjustmentSlider(m_mainRfBtn, m_mainRfSlider);
    configureAdjustmentSlider(m_subSqlBtn, m_subSqlSlider);
}

void SideControlPanel::configureAdjustmentSlider(DualControlButton *button, QSlider *slider) {
    if (!button || !slider)
        return;

    int minimum = 0;
    int maximum = 100;
    int value = 0;

    if (button == m_wpmBtn) {
        if (m_isCWMode) {
            if (m_wpmIsPrimary) {
                minimum = 8;
                maximum = 50;
                value = m_wpmValue;
            } else {
                minimum = 30;
                maximum = 99;
                value = m_pitchValue / 10;
            }
        } else if (m_wpmIsPrimary) {
            minimum = 0;
            maximum = 80;
            value = m_micValue;
        } else {
            minimum = 0;
            maximum = 30;
            value = m_compressionValue;
        }
    } else if (button == m_pwrBtn) {
        if (m_pwrIsPrimary) {
            minimum = 0;
            maximum = 110;
            value = qRound(m_powerValue);
        } else {
            minimum = 0;
            maximum = 255;
            value = m_delayValue;
        }
    } else if (button == m_bwBtn) {
        if (m_bwIsPrimary) {
            // BW moves in 50 Hz increments, matching current QK4.
            minimum = qMax(1, (m_filterBandwidthMinHz + 49) / 50);
            maximum = qMax(minimum, m_filterBandwidthMaxHz / 50);
            value = qRound(m_bandwidthValue / 50.0);
        } else {
            // HI is an edge in 10 Hz units with LO held fixed.
            minimum = qMax(0, m_lowCutValue / 10);
            maximum = qMax(minimum, (m_lowCutValue + m_filterBandwidthMaxHz) / 10);
            value = m_highCutValue / 10;
        }
    } else if (button == m_shiftBtn) {
        if (m_shiftIsPrimary) {
            minimum = m_filterCenterMinDah;
            maximum = m_filterCenterMaxDah;
            value = m_shiftValue / 10;
        } else {
            // With HI fixed, LO spans zero to the displayed high edge.
            minimum = 0;
            maximum = qMax(0, m_highCutValue / 10);
            value = m_lowCutValue / 10;
        }
        slider->setEnabled(!m_filterCenterLocked);
    } else if (button == m_mainRfBtn) {
        if (m_mainRfIsPrimary) {
            minimum = 0;
            maximum = 60;
            value = 60 - m_mainRfValue;
        } else {
            minimum = 0;
            maximum = 29;
            value = m_mainSqlValue;
        }
    } else if (button == m_subSqlBtn) {
        if (m_subSqlIsPrimary) {
            minimum = 0;
            maximum = 29;
            value = m_subSqlValue;
        } else {
            minimum = 0;
            maximum = 60;
            value = 60 - m_subRfValue;
        }
    }

    slider->blockSignals(true);
    slider->setRange(minimum, maximum);
    slider->setValue(qBound(minimum, value, maximum));
    slider->setProperty("lastRadioValue", slider->value());
    slider->blockSignals(false);
    slider->setToolTip(QString("Adjust %1").arg(button->primaryLabel()));
}

// ===== Group Management =====

void SideControlPanel::setGroup1Active(DualControlButton *activeBtn) {
    m_wpmBtn->setShowIndicator(activeBtn == m_wpmBtn);
    m_pwrBtn->setShowIndicator(activeBtn == m_pwrBtn);
}

void SideControlPanel::setGroup2Active(DualControlButton *activeBtn) {
    m_bwBtn->setShowIndicator(activeBtn == m_bwBtn);
    m_shiftBtn->setShowIndicator(activeBtn == m_shiftBtn);
}

void SideControlPanel::setGroup3Active(DualControlButton *activeBtn) {
    m_mainRfBtn->setShowIndicator(activeBtn == m_mainRfBtn);
    m_subSqlBtn->setShowIndicator(activeBtn == m_subSqlBtn);
}

// ===== Group 1 Slots =====

void SideControlPanel::onWpmBecameActive() {
    setGroup1Active(m_wpmBtn);
}

void SideControlPanel::onPwrBecameActive() {
    setGroup1Active(m_pwrBtn);
}

void SideControlPanel::onWpmScrolled(int delta) {
    if (m_isCWMode) {
        // CW mode: WPM/PTCH
        if (m_wpmIsPrimary) {
            emit wpmChanged(delta);
        } else {
            emit pitchChanged(delta);
        }
    } else {
        // Voice mode: MIC/CMP
        if (m_wpmIsPrimary) {
            emit micGainChanged(delta);
        } else {
            emit compressionChanged(delta);
        }
    }
}

void SideControlPanel::onPwrScrolled(int delta) {
    if (m_pwrIsPrimary) {
        emit powerChanged(delta);
    } else {
        emit delayChanged(delta);
    }
}

// ===== Group 2 Slots (BW/SHFT - LINKED PAIR) =====

void SideControlPanel::onBwBecameActive() {
    setGroup2Active(m_bwBtn);
}

void SideControlPanel::onShiftBecameActive() {
    setGroup2Active(m_shiftBtn);
}

void SideControlPanel::onBwScrolled(int delta) {
    if (m_bwIsPrimary) {
        emit bandwidthChanged(delta);
    } else {
        emit highCutChanged(delta);
    }
}

void SideControlPanel::onShiftScrolled(int delta) {
    if (m_shiftIsPrimary) {
        emit shiftChanged(delta);
    } else {
        emit lowCutChanged(delta);
    }
}

void SideControlPanel::onBwClicked() {
    // BW and SHFT are linked - when one swaps, the other swaps too
    m_bwIsPrimary = !m_bwIsPrimary;
    m_shiftIsPrimary = !m_shiftIsPrimary;
    m_shiftBtn->swapFunctions(); // Also swap the shift button
    configureAdjustmentSlider(m_bwBtn, m_bwSlider);
    configureAdjustmentSlider(m_shiftBtn, m_shiftSlider);
}

void SideControlPanel::onShiftClicked() {
    // BW and SHFT are linked - when one swaps, the other swaps too
    m_shiftIsPrimary = !m_shiftIsPrimary;
    m_bwIsPrimary = !m_bwIsPrimary;
    m_bwBtn->swapFunctions(); // Also swap the BW button
    configureAdjustmentSlider(m_bwBtn, m_bwSlider);
    configureAdjustmentSlider(m_shiftBtn, m_shiftSlider);
}

// ===== Group 3 Slots =====

void SideControlPanel::onMainRfBecameActive() {
    setGroup3Active(m_mainRfBtn);
}

void SideControlPanel::onSubSqlBecameActive() {
    setGroup3Active(m_subSqlBtn);
}

void SideControlPanel::onMainRfScrolled(int delta) {
    if (m_mainRfIsPrimary) {
        emit mainRfGainChanged(delta);
    } else {
        emit mainSquelchChanged(delta);
    }
}

void SideControlPanel::onSubSqlScrolled(int delta) {
    if (m_subSqlIsPrimary) {
        emit subSquelchChanged(delta);
    } else {
        emit subRfGainChanged(delta);
    }
}

// ===== Mode Switching =====

void SideControlPanel::setDisplayMode(bool isCWMode) {
    if (m_isCWMode == isCWMode)
        return;
    m_isCWMode = isCWMode;

    if (isCWMode) {
        m_wpmBtn->setPrimaryLabel("WPM");
        m_wpmBtn->setAlternateLabel("PTCH");
    } else {
        m_wpmBtn->setPrimaryLabel("MIC");
        m_wpmBtn->setAlternateLabel("CMP");
    }
    // Reset to show primary value
    m_wpmIsPrimary = true;
    m_wpmBtn->setPrimaryValue("--");
    m_wpmBtn->setAlternateValue("--");
    configureAdjustmentSlider(m_wpmBtn, m_wpmSlider);
}

// ===== Value Setters =====

void SideControlPanel::setWpm(int wpm) {
    if (!m_isCWMode)
        return; // Only set in CW mode
    m_wpmValue = wpm;
    if (m_wpmIsPrimary) {
        m_wpmBtn->setPrimaryValue(QString::number(wpm));
    } else {
        m_wpmBtn->setAlternateValue(QString::number(wpm));
    }
    configureAdjustmentSlider(m_wpmBtn, m_wpmSlider);
}

void SideControlPanel::setPitch(double pitch) {
    if (!m_isCWMode)
        return; // Only set in CW mode
    // The UI receives pitch in kHz for display, while the adjustment slider
    // uses 10 Hz units over the K4's 300-990 Hz range.
    m_pitchValue = qRound(pitch * 1000.0);
    QString pitchStr = QString::number(pitch, 'f', 2);
    if (!m_wpmIsPrimary) {
        m_wpmBtn->setPrimaryValue(pitchStr);
    } else {
        m_wpmBtn->setAlternateValue(pitchStr);
    }
    configureAdjustmentSlider(m_wpmBtn, m_wpmSlider);
}

void SideControlPanel::setMicGain(int gain) {
    if (m_isCWMode)
        return; // Only set in Voice mode
    m_micValue = gain;
    if (m_wpmIsPrimary) {
        m_wpmBtn->setPrimaryValue(QString::number(gain));
    } else {
        m_wpmBtn->setAlternateValue(QString::number(gain));
    }
    configureAdjustmentSlider(m_wpmBtn, m_wpmSlider);
}

void SideControlPanel::setCompression(int comp) {
    if (m_isCWMode)
        return; // Only set in Voice mode
    m_compressionValue = comp;
    if (!m_wpmIsPrimary) {
        m_wpmBtn->setPrimaryValue(QString::number(comp));
    } else {
        m_wpmBtn->setAlternateValue(QString::number(comp));
    }
    configureAdjustmentSlider(m_wpmBtn, m_wpmSlider);
}

void SideControlPanel::setPower(double power) {
    m_powerValue = power;
    // Show decimal for QRP (≤10W), whole number for QRO (>10W)
    QString powerStr = (power <= 10.0) ? QString::number(power, 'f', 1) : QString::number(static_cast<int>(power));
    if (m_pwrIsPrimary) {
        m_pwrBtn->setPrimaryValue(powerStr);
    } else {
        m_pwrBtn->setAlternateValue(powerStr);
    }
    configureAdjustmentSlider(m_pwrBtn, m_pwrSlider);
}

void SideControlPanel::setDelay(double delay) {
    m_delayValue = qRound(delay * 100.0);
    QString delayStr = QString::number(delay, 'f', 2);
    if (!m_pwrIsPrimary) {
        m_pwrBtn->setPrimaryValue(delayStr);
    } else {
        m_pwrBtn->setAlternateValue(delayStr);
    }
    configureAdjustmentSlider(m_pwrBtn, m_pwrSlider);
}

void SideControlPanel::setBandwidth(double bw) {
    m_bandwidthValue = qRound(bw * 1000.0);
    QString bwStr = QString::number(bw, 'f', 2);
    if (m_bwIsPrimary) {
        m_bwBtn->setPrimaryValue(bwStr);
    } else {
        m_bwBtn->setAlternateValue(bwStr);
    }
    configureAdjustmentSlider(m_bwBtn, m_bwSlider);
}

void SideControlPanel::setHighCut(double hi) {
    m_highCutValue = qRound(hi * 1000.0);
    QString hiStr = QString::number(hi, 'f', 2);
    if (!m_bwIsPrimary) {
        m_bwBtn->setPrimaryValue(hiStr);
    } else {
        m_bwBtn->setAlternateValue(hiStr);
    }
    configureAdjustmentSlider(m_bwBtn, m_bwSlider);
}

void SideControlPanel::setShift(double shift) {
    m_shiftValue = qRound(shift * 1000.0);
    QString shiftStr = QString::number(shift, 'f', 2);
    if (m_shiftIsPrimary) {
        m_shiftBtn->setPrimaryValue(shiftStr);
    } else {
        m_shiftBtn->setAlternateValue(shiftStr);
    }
    configureAdjustmentSlider(m_shiftBtn, m_shiftSlider);
}

void SideControlPanel::setLowCut(double lo) {
    m_lowCutValue = qRound(lo * 1000.0);
    QString loStr = QString::number(lo, 'f', 2);
    if (!m_shiftIsPrimary) {
        m_shiftBtn->setPrimaryValue(loStr);
    } else {
        m_shiftBtn->setAlternateValue(loStr);
    }
    configureAdjustmentSlider(m_shiftBtn, m_shiftSlider);
}

void SideControlPanel::setFilterControlRanges(int bandwidthMinHz, int bandwidthMaxHz, int centerMinDah,
                                              int centerMaxDah, bool centerLocked) {
    m_filterBandwidthMinHz = bandwidthMinHz;
    m_filterBandwidthMaxHz = bandwidthMaxHz;
    m_filterCenterMinDah = centerMinDah;
    m_filterCenterMaxDah = centerMaxDah;
    m_filterCenterLocked = centerLocked;
    configureAdjustmentSlider(m_bwBtn, m_bwSlider);
    configureAdjustmentSlider(m_shiftBtn, m_shiftSlider);
}

void SideControlPanel::setMainRfGain(int gain) {
    m_mainRfValue = gain;
    QString value = gain > 0 ? QString("-%1").arg(gain) : "0";
    if (m_mainRfIsPrimary) {
        m_mainRfBtn->setPrimaryValue(value);
    } else {
        m_mainRfBtn->setAlternateValue(value);
    }
    configureAdjustmentSlider(m_mainRfBtn, m_mainRfSlider);
}

void SideControlPanel::setMainSquelch(int sql) {
    m_mainSqlValue = sql;
    if (!m_mainRfIsPrimary) {
        m_mainRfBtn->setPrimaryValue(QString::number(sql));
    } else {
        m_mainRfBtn->setAlternateValue(QString::number(sql));
    }
    configureAdjustmentSlider(m_mainRfBtn, m_mainRfSlider);
}

void SideControlPanel::setSubSquelch(int sql) {
    m_subSqlValue = sql;
    if (m_subSqlIsPrimary) {
        m_subSqlBtn->setPrimaryValue(QString::number(sql));
    } else {
        m_subSqlBtn->setAlternateValue(QString::number(sql));
    }
    configureAdjustmentSlider(m_subSqlBtn, m_subSqlSlider);
}

void SideControlPanel::setSubRfGain(int gain) {
    m_subRfValue = gain;
    QString value = gain > 0 ? QString("-%1").arg(gain) : "0";
    if (!m_subSqlIsPrimary) {
        m_subSqlBtn->setPrimaryValue(value);
    } else {
        m_subSqlBtn->setAlternateValue(value);
    }
    configureAdjustmentSlider(m_subSqlBtn, m_subSqlSlider);
}

void SideControlPanel::setTime(const QString &time) {
    m_timeLabel->setText(time);
}

void SideControlPanel::setPowerReading(double watts) {
    QString current = m_powerSwrLabel->text();
    int spaceIdx = current.indexOf(' ');
    QString swrPart = (spaceIdx > 0) ? current.mid(spaceIdx) : " 1.0:1";
    m_powerSwrLabel->setText(QString("%1W%2").arg(watts, 0, 'f', 1).arg(swrPart));
}

void SideControlPanel::setSwr(double swr) {
    QString current = m_powerSwrLabel->text();
    int wIdx = current.indexOf('W');
    QString powerPart = (wIdx > 0) ? current.left(wIdx + 1) : "0.0W";
    m_powerSwrLabel->setText(QString("%1 %2:1").arg(powerPart).arg(swr, 0, 'f', 1));
}

void SideControlPanel::setActiveReceiver(bool isSubRx) {
    // Update context colors for filter buttons based on active receiver
    DualControlButton::Context ctx = isSubRx ? DualControlButton::SubRx : DualControlButton::MainRx;
    m_bwBtn->setContext(ctx);
    m_shiftBtn->setContext(ctx);
}

void SideControlPanel::setVoltage(double volts) {
    QString current = m_voltageCurrentLabel->text();
    int vIdx = current.indexOf('V');
    QString currentPart = (vIdx > 0) ? current.mid(vIdx + 1).trimmed() : "-.-A";
    m_voltageCurrentLabel->setText(QString("%1V  %2").arg(volts, 0, 'f', 1).arg(currentPart));
}

void SideControlPanel::setCurrent(double amps) {
    QString current = m_voltageCurrentLabel->text();
    int vIdx = current.indexOf('V');
    QString voltsPart = (vIdx > 0) ? current.left(vIdx + 1) : "--.-V";
    m_voltageCurrentLabel->setText(QString("%1  %2A").arg(voltsPart).arg(amps, 0, 'f', 1));
}

QWidget *SideControlPanel::createTxFunctionButton(const QString &mainText, const QString &subText,
                                                  QPushButton *&btnOut) {
    // Container widget for button + sub-text label
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, K4Styles::Dimensions::SeparatorHeight + 1, 0, K4Styles::Dimensions::SeparatorHeight + 1);
    layout->setSpacing(K4Styles::Dimensions::PaddingSmall);

    // Keep both the primary and amber alternate action inside one touch target.
    auto *btn = new DualLinePanelButton(mainText, subText, container);
    btn->setFixedHeight(42);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(K4Styles::sidePanelButtonLight());
    btnOut = btn;
    layout->addWidget(btn);

    return container;
}

int SideControlPanel::volume() const {
    return m_volumeSlider ? m_volumeSlider->value() : 100;
}

int SideControlPanel::subVolume() const {
    return m_subVolumeSlider ? m_subVolumeSlider->value() : 100;
}

void SideControlPanel::setVolume(int value) {
    if (!m_volumeSlider)
        return;
    const QSignalBlocker blocker(m_volumeSlider);
    m_volumeSlider->setValue(qBound(0, value, 100));
}

void SideControlPanel::setSubVolume(int value) {
    if (!m_subVolumeSlider)
        return;
    const QSignalBlocker blocker(m_subVolumeSlider);
    m_subVolumeSlider->setValue(qBound(0, value, 100));
}

void SideControlPanel::setPhoneMicGain(int value) {
    if (!m_phoneMicGainSlider)
        return;
    m_phoneMicGainSlider->blockSignals(true);
    m_phoneMicGainSlider->setValue(qBound(0, value, 100));
    m_phoneMicGainSlider->blockSignals(false);
}

void SideControlPanel::cancelPendingLongPress() {
    if (m_longPressTarget)
        m_suppressNextRelease = true;
    m_longPressTimer->stop();
    if (auto *button = qobject_cast<QPushButton *>(m_longPressTarget))
        button->setDown(false);
    m_longPressTarget = nullptr;
    m_longPressHandled = false;
    // DualControlButton owns its alternate-function hold timer. Cancel it
    // explicitly when QScroller decides that this touch is a panel scroll.
    for (DualControlButton *button : {m_wpmBtn, m_pwrBtn, m_bwBtn, m_shiftBtn, m_mainRfBtn, m_subSqlBtn})
        button->cancelPendingTouch();
}

bool SideControlPanel::eventFilter(QObject *watched, QEvent *event) {
    if (auto *slider = qobject_cast<QSlider *>(watched)) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_sliderDragTarget = slider;
                m_sliderPressPosition = mouseEvent->pos();
                m_sliderLastY = mouseEvent->pos().y();
                m_sliderScrolling = false;
                m_sliderAdjusting = false;
                // Do not pass the press to QSlider yet.  On a touch device a
                // vertical swipe often begins directly on the slider track;
                // forwarding this press would change the radio before we can
                // determine that the user intended to scroll the CTRL bank.
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && watched == m_sliderDragTarget) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            const QPoint delta = mouseEvent->pos() - m_sliderPressPosition;
            if (!m_sliderScrolling && !m_sliderAdjusting && delta.manhattanLength() > 12) {
                m_sliderScrolling = qAbs(delta.y()) > qAbs(delta.x());
                m_sliderAdjusting = !m_sliderScrolling;
            }
            if (m_sliderScrolling) {
                if (QScrollArea *scroll = containingScrollArea()) {
                    scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->value() -
                                                          (mouseEvent->pos().y() - m_sliderLastY));
                }
                m_sliderLastY = mouseEvent->pos().y();
                return true;
            }
            if (m_sliderAdjusting) {
                // Preserve the radio's existing full, native slider range;
                // this only changes when the adjustment begins, not its
                // resolution or the emitted value.
                setSliderValueFromTouchPosition(slider, mouseEvent->pos().x());
                return true;
            }
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && watched == m_sliderDragTarget) {
            const bool commitShift = slider == m_shiftSlider && m_sliderAdjusting && m_shiftIsPrimary;
            const int shiftTarget = slider->value();
            m_sliderDragTarget = nullptr;
            m_sliderScrolling = false;
            m_sliderAdjusting = false;
            // A tap without deliberate horizontal motion is intentionally a
            // no-op, avoiding an accidental setting change while scrolling.
            if (commitShift)
                emit shiftSliderCommitted(shiftTarget);
            return true;
        }
        // Let non-input events, especially Paint and Resize, reach QSlider.
        // Only the deliberate touch-gesture paths above are consumed.
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::RightButton) {
            triggerSecondary(watched);
            return true;
        }
        if (mouseEvent->button() == Qt::LeftButton) {
            m_longPressTarget = watched;
            m_longPressHandled = false;
            m_dragging = false;
            m_pressPosition = mouseEvent->pos();
            m_longPressTimer->start();
        }
    } else if (event->type() == QEvent::MouseMove && watched == m_longPressTarget) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if ((mouseEvent->pos() - m_pressPosition).manhattanLength() > 12) {
            m_dragging = true;
            m_suppressNextRelease = true;
            m_longPressTimer->stop();
            if (auto *button = qobject_cast<QPushButton *>(watched))
                button->setDown(false);
            m_longPressTarget = nullptr;
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && m_suppressNextRelease) {
            m_suppressNextRelease = false;
            if (auto *button = qobject_cast<QPushButton *>(watched))
                button->setDown(false);
            return true;
        }
        if (mouseEvent->button() == Qt::LeftButton && watched == m_longPressTarget) {
            m_longPressTimer->stop();
            m_longPressTarget = nullptr;
            if (m_longPressHandled) {
                if (auto *button = qobject_cast<QPushButton *>(watched))
                    button->setDown(false);
                return true;
            }
        }
        m_dragging = false;
    }
    return QWidget::eventFilter(watched, event);
}

void SideControlPanel::setSliderValueFromTouchPosition(QSlider *slider, int xPosition) {
    if (!slider)
        return;

    const int handleWidth = qMax(12, slider->height() / 2);
    const int span = qMax(1, slider->width() - handleWidth);
    const int position = qBound(0, xPosition - handleWidth / 2, span);
    slider->setValue(QStyle::sliderValueFromPosition(slider->minimum(), slider->maximum(), position, span,
                                                      slider->invertedAppearance()));
}

QScrollArea *SideControlPanel::containingScrollArea() const {
    for (QWidget *ancestor = parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
        if (auto *scroll = qobject_cast<QScrollArea *>(ancestor))
            return scroll;
    }
    return nullptr;
}

void SideControlPanel::triggerSecondary(QObject *watched) {
    if (watched == m_tuneBtn) emit tuneLpClicked();
    else if (watched == m_xmitBtn) emit testClicked();
    else if (watched == m_atuTuneBtn) emit atuClicked();
    else if (watched == m_voxBtn) emit qskClicked();
    else if (watched == m_antBtn) emit remAntClicked();
    else if (watched == m_rxAntBtn) emit subAntClicked();
}
