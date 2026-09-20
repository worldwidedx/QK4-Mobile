#include "bottommenubar.h"
#include "k4styles.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QList>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSizePolicy>
#include <cmath>

BottomMenuBar::BottomMenuBar(QWidget *parent) : QWidget(parent) {
    setupUi();
}

void BottomMenuBar::setupUi() {
    if (K4Styles::isCompactLayout()) {
        // The phone UI is landscape-first: the live surface gets a small,
        // predictable set of operating controls, while less-frequent radio
        // functions remain in QK4's existing menu and popup system.
        // Two operating rows are deliberately bounded to the landscape phone
        // viewport. Volume remains available in the scrollable Controls page.
        // This dock must fit even when Android reports the shorter usable
        // landscape viewport (system bars visible).  Its previous 66 px
        // minimum could force the tuning row below the canvas on phones.
        // Keep the lower tuning row at its original touch size. The compact
        // height adjustment belongs to the command-button row above it.
        setFixedHeight(60);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(7, 2, 7, 2);
        layout->setSpacing(2);

        auto *menuRow = new QHBoxLayout();
        menuRow->setContentsMargins(0, 0, 0, 0);
        menuRow->setSpacing(4);
        m_connectBtn = createMenuButton("CONN");
        m_menuBtn = createMenuButton("MENU");
        m_frequencyABtn = createMenuButton("A FREQ");
        m_frequencyBBtn = createMenuButton("B FREQ");
        m_controlsBtn = createMenuButton("CTRL");
        m_fnBtn = createMenuButton("Fn");
        m_displayBtn = createMenuButton("DISP");
        m_bandBtn = createMenuButton("BAND");
        m_mainRxBtn = createMenuButton("A RX");
        m_subRxBtn = createMenuButton("B RX");
        m_txBtn = createMenuButton("TX");
        m_pttBtn = createMenuButton("TX / RX");
        const QList<QPushButton *> menuButtons = {m_connectBtn, m_bandBtn, m_frequencyABtn, m_frequencyBBtn,
                                                   m_fnBtn, m_displayBtn, m_mainRxBtn, m_subRxBtn, m_txBtn,
                                                   m_controlsBtn, m_menuBtn, m_pttBtn};
        for (auto *button : menuButtons) {
            button->setFixedHeight(26);
            button->setMinimumWidth(0);
            button->setMaximumWidth(QWIDGETSIZE_MAX);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            menuRow->addWidget(button, 1);
        }
        m_pttBtn->setStyleSheet(K4Styles::menuBarButton());
        layout->addLayout(menuRow);

        auto *tuneRow = new QHBoxLayout();
        tuneRow->setContentsMargins(0, 0, 0, 0);
        tuneRow->setSpacing(4);
        m_tuneADownBtn = createMenuButton("A -");
        m_tuneAUpBtn = createMenuButton("A +");
        m_tuneBDownBtn = createMenuButton("B -");
        m_tuneBUpBtn = createMenuButton("B +");
        for (auto *button : {m_tuneADownBtn, m_tuneAUpBtn, m_tuneBDownBtn, m_tuneBUpBtn}) {
            button->setFixedHeight(26);
            button->setMinimumWidth(0);
            button->setMaximumWidth(QWIDGETSIZE_MAX);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }
        m_settingsBtn = createMenuButton(QString());
        m_settingsBtn->setAccessibleName("QK4 Settings");
        m_settingsBtn->setToolTip("QK4 Settings");
        m_settingsBtn->setFixedSize(34, 26);
        // Draw a monochrome gear so Android cannot substitute a colored emoji.
        QPixmap gearPixmap(16, 16);
        gearPixmap.fill(Qt::transparent);
        QPainter gearPainter(&gearPixmap);
        gearPainter.setRenderHint(QPainter::Antialiasing);
        gearPainter.setPen(QPen(Qt::white, 2.0, Qt::SolidLine, Qt::RoundCap));
        const QPointF center(8.0, 8.0);
        constexpr qreal Pi = 3.14159265358979323846;
        for (int i = 0; i < 8; ++i) {
            const qreal angle = i * Pi / 4.0;
            gearPainter.drawLine(center + QPointF(std::cos(angle) * 4.0, std::sin(angle) * 4.0),
                                 center + QPointF(std::cos(angle) * 6.5, std::sin(angle) * 6.5));
        }
        gearPainter.drawEllipse(center, 4.0, 4.0);
        gearPainter.drawEllipse(center, 1.5, 1.5);
        gearPainter.end();
        m_settingsBtn->setIcon(QIcon(gearPixmap));
        m_settingsBtn->setIconSize(QSize(16, 16));
        tuneRow->addWidget(m_settingsBtn);
        tuneRow->addWidget(m_tuneADownBtn);
        tuneRow->addWidget(m_tuneAUpBtn);
        tuneRow->addWidget(m_tuneBDownBtn);
        tuneRow->addWidget(m_tuneBUpBtn);
        layout->addLayout(tuneRow);

        // Keep these widgets as state mirrors for the existing audio wiring,
        // but place the actual volume controls in the Controls screen. A third
        // row here was the reason PTT and tuning controls fell below canvas.
        m_mainVolumeSlider = new QSlider(Qt::Horizontal, this);
        m_subVolumeSlider = new QSlider(Qt::Horizontal, this);
        for (auto *slider : {m_mainVolumeSlider, m_subVolumeSlider}) {
            slider->setRange(0, 100);
            slider->setValue(75);
            slider->hide();
        }
        m_mainVolumeSlider->setStyleSheet(
            K4Styles::sliderHorizontal(K4Styles::Colors::DarkBackground, K4Styles::Colors::VfoACyan));
        m_subVolumeSlider->setStyleSheet(
            K4Styles::sliderHorizontal(K4Styles::Colors::DarkBackground, K4Styles::Colors::VfoBGreen));
    } else {
    setFixedHeight(K4Styles::Dimensions::MenuBarHeight);

    auto *layout = new QHBoxLayout(this);
    // Left margin matches side panel/scroll width to align with waterfall above
    const int sidePanelInset = K4Styles::Dimensions::SidePanelWidth + (K4Styles::isCompactLayout() ? 14 : 0);
    layout->setContentsMargins(sidePanelInset, K4Styles::Dimensions::PaddingSmall, K4Styles::Dimensions::PaddingMedium,
                               K4Styles::Dimensions::PaddingSmall);
    layout->setSpacing(K4Styles::Dimensions::PopupButtonSpacing);

    // Add stretch before buttons to center them
    layout->addStretch();

    // ===== Menu Buttons =====
    m_menuBtn = createMenuButton("MENU");
    m_fnBtn = createMenuButton("Fn");
    m_displayBtn = createMenuButton("DISPLAY");
    m_bandBtn = createMenuButton("BAND");
    m_mainRxBtn = createMenuButton("MAIN RX");
    m_subRxBtn = createMenuButton("SUB RX");
    m_txBtn = createMenuButton("TX");

    layout->addWidget(m_menuBtn);
    layout->addWidget(m_fnBtn);
    layout->addWidget(m_displayBtn);
    layout->addWidget(m_bandBtn);
    layout->addWidget(m_mainRxBtn);
    layout->addWidget(m_subRxBtn);
    layout->addWidget(m_txBtn);

    // Add stretch after buttons to center them
    layout->addStretch();

    // PTT button at far right (separated from main buttons)
    m_pttBtn = createMenuButton("PTT");
    layout->addWidget(m_pttBtn);
    }

    // ===== Connect Signals =====
    connect(m_menuBtn, &QPushButton::clicked, this, &BottomMenuBar::menuClicked);
    if (m_connectBtn)
        connect(m_connectBtn, &QPushButton::clicked, this, &BottomMenuBar::connectClicked);
    if (m_frequencyABtn) {
        connect(m_frequencyABtn, &QPushButton::clicked, this, &BottomMenuBar::frequencyARequested);
        connect(m_frequencyBBtn, &QPushButton::clicked, this, &BottomMenuBar::frequencyBRequested);
    }
    if (m_controlsBtn)
        connect(m_controlsBtn, &QPushButton::clicked, this, &BottomMenuBar::controlsRequested);
    if (m_settingsBtn)
        connect(m_settingsBtn, &QPushButton::clicked, this, &BottomMenuBar::settingsRequested);
    connect(m_fnBtn, &QPushButton::clicked, this, &BottomMenuBar::fnClicked);
    connect(m_displayBtn, &QPushButton::clicked, this, &BottomMenuBar::displayClicked);
    connect(m_bandBtn, &QPushButton::clicked, this, &BottomMenuBar::bandClicked);
    connect(m_mainRxBtn, &QPushButton::clicked, this, &BottomMenuBar::mainRxClicked);
    connect(m_subRxBtn, &QPushButton::clicked, this, &BottomMenuBar::subRxClicked);
    connect(m_txBtn, &QPushButton::clicked, this, &BottomMenuBar::txClicked);

    // A phone PTT is deliberately latched: touch delivery may produce a
    // press/release pair while the finger is stationary, which is unsafe for
    // a transmitter. Tap once for TX, tap again for RX.
    connect(m_pttBtn, &QPushButton::clicked, this, [this]() {
        if (m_pttLocked) {
            m_pttLocked = false;
            m_pttLockTimer->stop();
            emit pttReleased();
        } else {
            m_pttLocked = true;
            m_pttLockTimer->start();
            emit pttPressed();
        }
    });
    if (m_tuneADownBtn) {
        connect(m_tuneADownBtn, &QPushButton::clicked, this, [this]() { emit tuneARequested(-1); });
        connect(m_tuneAUpBtn, &QPushButton::clicked, this, [this]() { emit tuneARequested(1); });
        connect(m_tuneBDownBtn, &QPushButton::clicked, this, [this]() { emit tuneBRequested(-1); });
        connect(m_tuneBUpBtn, &QPushButton::clicked, this, [this]() { emit tuneBRequested(1); });
        connect(m_mainVolumeSlider, &QSlider::valueChanged, this, &BottomMenuBar::mainVolumeChanged);
        connect(m_subVolumeSlider, &QSlider::valueChanged, this, &BottomMenuBar::subVolumeChanged);
    }

    // Right-click toggle (latch) mode for PTT with 180-second safety timeout
    m_pttLockTimer = new QTimer(this);
    m_pttLockTimer->setSingleShot(true);
    m_pttLockTimer->setInterval(180000);
    connect(m_pttLockTimer, &QTimer::timeout, this, [this]() {
        if (m_pttLocked) {
            m_pttLocked = false;
            setPttActive(false);
            emit pttReleased();
        }
    });
    m_pttBtn->installEventFilter(this);
}

void BottomMenuBar::setMainVolumeValue(int value) {
    if (m_mainVolumeSlider) {
        m_mainVolumeSlider->blockSignals(true);
        m_mainVolumeSlider->setValue(value);
        m_mainVolumeSlider->blockSignals(false);
    }
}

void BottomMenuBar::setSubVolumeValue(int value) {
    if (m_subVolumeSlider) {
        m_subVolumeSlider->blockSignals(true);
        m_subVolumeSlider->setValue(value);
        m_subVolumeSlider->blockSignals(false);
    }
}

namespace {
QString phoneStepLabel(int hertz) {
    if (hertz >= 1000000)
        return QString::number(hertz / 1000000) + "M";
    if (hertz >= 1000)
        return QString::number(hertz / 1000) + "k";
    return QString::number(hertz) + "Hz";
}
}

void BottomMenuBar::setTuneStepA(int hertz) {
    m_tuneStepAHz = hertz;
    if (m_tuneADownBtn)
        m_tuneADownBtn->setText("A - " + phoneStepLabel(hertz));
    if (m_tuneAUpBtn)
        m_tuneAUpBtn->setText("A + " + phoneStepLabel(hertz));
}

void BottomMenuBar::setTuneStepB(int hertz) {
    m_tuneStepBHz = hertz;
    if (m_tuneBDownBtn)
        m_tuneBDownBtn->setText("B - " + phoneStepLabel(hertz));
    if (m_tuneBUpBtn)
        m_tuneBUpBtn->setText("B + " + phoneStepLabel(hertz));
}

QPushButton *BottomMenuBar::createMenuButton(const QString &text) {
    auto *btn = new QPushButton(text, this);
    btn->setFixedSize(K4Styles::Dimensions::MenuBarButtonWidth, K4Styles::Dimensions::ButtonHeightMedium);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(K4Styles::menuBarButton());
    return btn;
}

void BottomMenuBar::setMenuActive(bool active) {
    if (active) {
        m_menuBtn->setStyleSheet(K4Styles::menuBarButtonActive());
    } else {
        m_menuBtn->setStyleSheet(K4Styles::menuBarButton());
    }
}

void BottomMenuBar::setDisplayActive(bool active) {
    if (active) {
        m_displayBtn->setStyleSheet(K4Styles::menuBarButtonActive());
    } else {
        m_displayBtn->setStyleSheet(K4Styles::menuBarButton());
    }
}

void BottomMenuBar::setBandActive(bool active) {
    if (active) {
        m_bandBtn->setStyleSheet(K4Styles::menuBarButtonActive());
    } else {
        m_bandBtn->setStyleSheet(K4Styles::menuBarButton());
    }
}

void BottomMenuBar::setFnActive(bool active) {
    if (active) {
        m_fnBtn->setStyleSheet(K4Styles::menuBarButtonActive());
    } else {
        m_fnBtn->setStyleSheet(K4Styles::menuBarButton());
    }
}

void BottomMenuBar::setMainRxActive(bool active) {
    if (active) {
        m_mainRxBtn->setStyleSheet(K4Styles::menuBarButtonActive());
    } else {
        m_mainRxBtn->setStyleSheet(K4Styles::menuBarButton());
    }
}

void BottomMenuBar::setSubRxActive(bool active) {
    if (active) {
        m_subRxBtn->setStyleSheet(K4Styles::menuBarButtonActive());
    } else {
        m_subRxBtn->setStyleSheet(K4Styles::menuBarButton());
    }
}

void BottomMenuBar::setTxActive(bool active) {
    if (active) {
        m_txBtn->setStyleSheet(K4Styles::menuBarButtonActive());
    } else {
        m_txBtn->setStyleSheet(K4Styles::menuBarButton());
    }
}

void BottomMenuBar::setPttActive(bool active) {
    if (active) {
        m_pttBtn->setText("TX ON");
        m_pttBtn->setStyleSheet(K4Styles::menuBarButtonPttPressed());
    } else {
        m_pttLocked = false;
        m_pttLockTimer->stop();
        m_pttBtn->setText("TX / RX");
        m_pttBtn->setStyleSheet(K4Styles::menuBarButton());
    }
}

bool BottomMenuBar::eventFilter(QObject *watched, QEvent *event) {
    Q_UNUSED(watched)
    Q_UNUSED(event)
    return QWidget::eventFilter(watched, event);
}
