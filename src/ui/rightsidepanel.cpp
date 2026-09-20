#include "rightsidepanel.h"
#include "duallinepanelbutton.h"
#include "k4styles.h"
#include <QEvent>
#include <QGridLayout>
#include <QMouseEvent>

RightSidePanel::RightSidePanel(QWidget *parent)
    : QWidget(parent), m_preBtn(nullptr), m_nbBtn(nullptr), m_nrBtn(nullptr), m_ntchBtn(nullptr), m_filBtn(nullptr),
      m_abBtn(nullptr), m_revBtn(nullptr), m_atobBtn(nullptr), m_spotBtn(nullptr), m_modeBtn(nullptr),
      m_bsetBtn(nullptr), m_clrBtn(nullptr), m_ritBtn(nullptr), m_xitBtn(nullptr), m_freqEntBtn(nullptr),
      m_rateBtn(nullptr), m_lockABtn(nullptr), m_subBtn(nullptr) {
    m_longPressTimer = new QTimer(this);
    m_longPressTimer->setSingleShot(true);
    m_longPressTimer->setInterval(550);
    connect(m_longPressTimer, &QTimer::timeout, this, [this]() {
        if (!m_longPressTarget)
            return;
        m_longPressHandled = true;
        triggerSecondary(m_longPressTarget);
    });
    m_revPressTimer = new QTimer(this);
    m_revPressTimer->setSingleShot(true);
    // QScroller delays a child press for 250 ms while it resolves a drag.
    // Keep REV just beyond that interval, otherwise it transmits SW160 as
    // soon as a finger starts a scroll over the REV tile.
    m_revPressTimer->setInterval(300);
    connect(m_revPressTimer, &QTimer::timeout, this, [this]() {
        if (!m_revPressPending || m_revDragging)
            return;
        m_revActive = true;
        emit revPressed();
    });
    setupUi();
}

void RightSidePanel::setupUi() {
    const bool compact = K4Styles::isCompactLayout();
    // Match left panel dimensions exactly
    setFixedWidth(K4Styles::Dimensions::SidePanelWidth);
    QPalette panelPalette = palette();
    panelPalette.setColor(QPalette::Window, QColor(K4Styles::Colors::PopupBackground));
    setPalette(panelPalette);
    setAutoFillBackground(true);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(K4Styles::Dimensions::PaddingSmall, K4Styles::Dimensions::PopupButtonSpacing,
                                 K4Styles::Dimensions::PaddingSmall, K4Styles::Dimensions::PopupButtonSpacing);
    m_layout->setSpacing(K4Styles::Dimensions::PopupButtonSpacing);

    // Create 5×2 button grid
    auto *buttonGrid = new QGridLayout();
    buttonGrid->setContentsMargins(0, 0, 0, 0);
    buttonGrid->setHorizontalSpacing(K4Styles::Dimensions::PopupButtonSpacing);
    buttonGrid->setVerticalSpacing(K4Styles::Dimensions::PopupButtonSpacing);

    auto *preControl = createFunctionButton("PRE", "ATTN", m_preBtn);
    auto *nbControl = createFunctionButton("NB", "LEVEL", m_nbBtn);
    auto *nrControl = createFunctionButton("NR", "ADJ", m_nrBtn);
    auto *ntchControl = createFunctionButton("NTCH", "MANUAL", m_ntchBtn);
    auto *filControl = createFunctionButton("FIL", "APF", m_filBtn);
    auto *abControl = createFunctionButton("A/B", "SPLIT", m_abBtn);
    auto *revControl = createFunctionButton("REV", "", m_revBtn);
    auto *atobControl = createFunctionButton("A->B", "B->A", m_atobBtn);
    auto *spotControl = createFunctionButton("SPOT", "AUTO", m_spotBtn);
    auto *modeControl = createFunctionButton("MODE", "ALT", m_modeBtn);

    if (compact) {
        const QList<QWidget *> controls = {preControl, nbControl, nrControl, ntchControl, filControl,
                                            abControl, revControl, atobControl, spotControl, modeControl};
        for (int i = 0; i < controls.size(); ++i)
            buttonGrid->addWidget(controls[i], i / 4, i % 4);
    } else {
        buttonGrid->addWidget(preControl, 0, 0);
        buttonGrid->addWidget(nbControl, 0, 1);
        buttonGrid->addWidget(nrControl, 1, 0);
        buttonGrid->addWidget(ntchControl, 1, 1);
        buttonGrid->addWidget(filControl, 2, 0);
        buttonGrid->addWidget(abControl, 2, 1);
        buttonGrid->addWidget(revControl, 3, 0);
        buttonGrid->addWidget(atobControl, 3, 1);
        buttonGrid->addWidget(spotControl, 4, 0);
        buttonGrid->addWidget(modeControl, 4, 1);
    }

    m_layout->addLayout(buttonGrid);

    // Connect existing button signals
    connect(m_preBtn, &QPushButton::clicked, this, &RightSidePanel::preClicked);
    connect(m_nbBtn, &QPushButton::clicked, this, &RightSidePanel::nbClicked);
    connect(m_nrBtn, &QPushButton::clicked, this, &RightSidePanel::nrClicked);
    connect(m_ntchBtn, &QPushButton::clicked, this, &RightSidePanel::ntchClicked);
    connect(m_filBtn, &QPushButton::clicked, this, &RightSidePanel::filClicked);
    connect(m_abBtn, &QPushButton::clicked, this, &RightSidePanel::abClicked);
    connect(m_atobBtn, &QPushButton::clicked, this, &RightSidePanel::atobClicked);
    connect(m_spotBtn, &QPushButton::clicked, this, &RightSidePanel::spotClicked);
    connect(m_modeBtn, &QPushButton::clicked, this, &RightSidePanel::modeClicked);

    // Install event filters for right-click handling on main 5x2 grid
    m_preBtn->installEventFilter(this);
    m_nbBtn->installEventFilter(this);
    m_nrBtn->installEventFilter(this);
    m_ntchBtn->installEventFilter(this);
    m_filBtn->installEventFilter(this);
    m_abBtn->installEventFilter(this);
    // REV has no secondary action, but still needs the same scroll guard as
    // every other phone CTRL tile.
    m_revBtn->installEventFilter(this);
    m_atobBtn->installEventFilter(this);
    m_spotBtn->installEventFilter(this);
    m_modeBtn->installEventFilter(this);

    // Add stretch to push remaining buttons to bottom (above PTT)
    if (!compact)
        m_layout->addStretch();

    // Create 2×2 PF button grid (B SET, CLR, RIT, XIT)
    auto *pfGrid = new QGridLayout();
    pfGrid->setContentsMargins(0, 0, 0, 0);
    pfGrid->setHorizontalSpacing(K4Styles::Dimensions::PopupButtonSpacing);
    pfGrid->setVerticalSpacing(K4Styles::Dimensions::PopupButtonSpacing);

    auto *bsetControl = createFunctionButton("B SET", "PF 1", m_bsetBtn, true);
    auto *clrControl = createFunctionButton("CLR", "PF 2", m_clrBtn, true);
    auto *ritControl = createFunctionButton("RIT", "PF 3", m_ritBtn, true);
    auto *xitControl = createFunctionButton("XIT", "PF 4", m_xitBtn, true);
    if (compact) {
        buttonGrid->addWidget(bsetControl, 2, 2);
        buttonGrid->addWidget(clrControl, 2, 3);
        buttonGrid->addWidget(ritControl, 3, 0);
        buttonGrid->addWidget(xitControl, 3, 1);
    } else {
        pfGrid->addWidget(bsetControl, 0, 0);
        pfGrid->addWidget(clrControl, 0, 1);
        pfGrid->addWidget(ritControl, 1, 0);
        pfGrid->addWidget(xitControl, 1, 1);
        m_layout->addLayout(pfGrid);
    }

    // Connect PF button signals
    connect(m_bsetBtn, &QPushButton::clicked, this, &RightSidePanel::bsetClicked);
    connect(m_clrBtn, &QPushButton::clicked, this, &RightSidePanel::clrClicked);
    connect(m_ritBtn, &QPushButton::clicked, this, &RightSidePanel::ritClicked);
    connect(m_xitBtn, &QPushButton::clicked, this, &RightSidePanel::xitClicked);

    // Install event filters for right-click handling on PF row
    m_bsetBtn->installEventFilter(this);
    m_clrBtn->installEventFilter(this);
    m_ritBtn->installEventFilter(this);
    m_xitBtn->installEventFilter(this);

    // Add spacing between PF grid and bottom grid (25px gap)
    if (!compact)
        m_layout->addSpacing(K4Styles::Dimensions::PaddingLarge * 2 + K4Styles::Dimensions::PaddingSmall);

    // Create 2×2 bottom button grid (FREQ ENT, RATE, LOCK A, SUB)
    auto *bottomGrid = new QGridLayout();
    bottomGrid->setContentsMargins(0, 0, 0, 0);
    bottomGrid->setHorizontalSpacing(K4Styles::Dimensions::PopupButtonSpacing);
    bottomGrid->setVerticalSpacing(K4Styles::Dimensions::PopupButtonSpacing);

    auto *freqControl = createFunctionButton("FREQ\nENT", "SCAN", m_freqEntBtn);
    auto *rateControl = createFunctionButton("RATE", "KHZ", m_rateBtn);
    auto *lockControl = createFunctionButton("LOCK A", "LOCK B", m_lockABtn);
    auto *subControl = createFunctionButton("SUB", "DIVERSITY", m_subBtn);
    if (compact) {
        buttonGrid->addWidget(freqControl, 3, 2);
        buttonGrid->addWidget(rateControl, 3, 3);
        buttonGrid->addWidget(lockControl, 4, 0);
        buttonGrid->addWidget(subControl, 4, 1);
    } else {
        bottomGrid->addWidget(freqControl, 0, 0);
        bottomGrid->addWidget(rateControl, 0, 1);
        bottomGrid->addWidget(lockControl, 1, 0);
        bottomGrid->addWidget(subControl, 1, 1);
        m_layout->addLayout(bottomGrid);
    }

    // Connect bottom button signals
    connect(m_freqEntBtn, &QPushButton::clicked, this, &RightSidePanel::freqEntClicked);
    connect(m_rateBtn, &QPushButton::clicked, this, &RightSidePanel::rateClicked);
    connect(m_lockABtn, &QPushButton::clicked, this, &RightSidePanel::lockAClicked);
    connect(m_subBtn, &QPushButton::clicked, this, &RightSidePanel::subClicked);

    // Install event filter for right-click on RATE, LOCK A, and SUB buttons
    m_rateBtn->installEventFilter(this);
    m_lockABtn->installEventFilter(this);
    m_subBtn->installEventFilter(this);
}

QWidget *RightSidePanel::createFunctionButton(const QString &mainText, const QString &subText, QPushButton *&btnOut,
                                              bool isLighter) {
    // The alternate action belongs inside the same touch target as its primary.
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, K4Styles::isCompactLayout() ? 0 : K4Styles::Dimensions::SeparatorHeight + 1,
                               0, K4Styles::isCompactLayout() ? 0 : K4Styles::Dimensions::SeparatorHeight + 1);
    layout->setSpacing(K4Styles::isCompactLayout() ? 0 : K4Styles::Dimensions::PaddingSmall);

    // Button - scaled down from bottom menu bar style (matching left panel TX buttons)
    auto *btn = new DualLinePanelButton(mainText, subText, container);
    btn->setFixedHeight(42);
    btn->setCursor(Qt::PointingHandCursor);

    if (isLighter) {
        btn->setStyleSheet(K4Styles::sidePanelButtonLight());
    } else {
        btn->setStyleSheet(K4Styles::sidePanelButton());
    }
    btnOut = btn;
    layout->addWidget(btn);

    return container;
}

void RightSidePanel::cancelPendingLongPress() {
    if (m_longPressTarget)
        m_suppressNextRelease = true;
    m_longPressTimer->stop();
    if (auto *button = qobject_cast<QPushButton *>(m_longPressTarget))
        button->setDown(false);
    m_longPressTarget = nullptr;
    m_longPressHandled = false;

    // A QScroller state transition can arrive after the REV timer.  Ensure a
    // scroll never leaves the radio in the momentary-reverse state.
    if (m_revPressTimer)
        m_revPressTimer->stop();
    m_revPressPending = false;
    m_revDragging = true;
    if (m_revActive) {
        m_revActive = false;
        emit revReleased();
    }
    if (m_revBtn)
        m_revBtn->setDown(false);
}

bool RightSidePanel::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_revBtn) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_pressPosition = mouseEvent->pos();
                m_revPressPending = true;
                m_revDragging = false;
                m_revActive = false;
                m_revPressTimer->start();
            }
        } else if (event->type() == QEvent::MouseMove && m_revPressPending) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if ((mouseEvent->pos() - m_pressPosition).manhattanLength() > 12) {
                m_revDragging = true;
                m_revPressPending = false;
                m_revPressTimer->stop();
                m_revBtn->setDown(false);
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_revPressTimer->stop();
                m_revPressPending = false;
                if (m_revActive) {
                    m_revActive = false;
                    emit revReleased();
                } else if (!m_revDragging) {
                    // Preserve the original quick-tap semantics.  A tap is
                    // still a momentary REV action; only an actual drag is
                    // discarded as scrolling.
                    emit revPressed();
                    emit revReleased();
                }
                m_revBtn->setDown(false);
            }
        }
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
        // A vertical drag belongs to the scroll area, never to the button
        // under the finger. Suppress the release-generated QPushButton click.
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
                return true; // Do not also invoke the primary click on release.
            }
        }
        m_dragging = false;
    }
    return QWidget::eventFilter(watched, event);
}

void RightSidePanel::triggerSecondary(QObject *watched) {
    if (watched == m_preBtn) emit attnClicked();
    else if (watched == m_nbBtn) emit levelClicked();
    else if (watched == m_nrBtn) emit adjClicked();
    else if (watched == m_ntchBtn) emit manualClicked();
    else if (watched == m_filBtn) emit apfClicked();
    else if (watched == m_abBtn) emit splitClicked();
    else if (watched == m_atobBtn) emit btoaClicked();
    else if (watched == m_spotBtn) emit autoClicked();
    else if (watched == m_modeBtn) emit altClicked();
    else if (watched == m_bsetBtn) emit pf1Clicked();
    else if (watched == m_clrBtn) emit pf2Clicked();
    else if (watched == m_ritBtn) emit pf3Clicked();
    else if (watched == m_xitBtn) emit pf4Clicked();
    else if (watched == m_rateBtn) emit khzClicked();
    else if (watched == m_lockABtn) emit lockBClicked();
    else if (watched == m_subBtn) emit diversityClicked();
}
