#include "ft8screen.h"
#include "logbookdialog.h"
#include "ft8/ft8transmitter.h"
#include "ft8waterfall.h"
#include "ft8activitystyle.h"
#include "frequencydisplaywidget.h"
#include "frequencyentryparser.h"
#include "inwindowdialog.h"
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QBoxLayout>
#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <QScrollerProperties>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QStandardPaths>
#include <QStyledItemDelegate>
#include <QStyleOptionSlider>
#include <QPainter>
#include <QTimer>
#include <cmath>
extern "C" {
#include <ft8/encode.h>
#include <ft8/message.h>
}

namespace {
// Touch anywhere on the track to choose a value, but commit only on release.
// Some platform slider styles otherwise issue an immediate absolute change
// on touch-down even when QAbstractSlider tracking is disabled.
class Ft8PowerSlider : public QSlider {
public:
    explicit Ft8PowerSlider(QWidget *parent) : QSlider(Qt::Horizontal, parent) {}
protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton)
            return;
        setSliderDown(true);
        moveTo(event->position().x());
        event->accept();
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (isSliderDown())
            moveTo(event->position().x());
        event->accept();
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && isSliderDown()) {
            moveTo(event->position().x());
            setSliderDown(false);
        }
        event->accept();
    }
private:
    void moveTo(qreal x) {
        QStyleOptionSlider option;
        initStyleOption(&option);
        const int handleWidth = style()->subControlRect(QStyle::CC_Slider, &option,
                                                        QStyle::SC_SliderHandle, this).width();
        setSliderPosition(QStyle::sliderValueFromPosition(minimum(), maximum(),
                                                         qRound(x) - handleWidth / 2,
                                                         qMax(1, width() - handleWidth), option.upsideDown));
    }
};
// Preserve message meaning even while selected. Selection is an outline,
// not a theme highlight that hides CQ/own-call/TX background colors.
class Ft8ActivityDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem styled(option);
        initStyleOption(&styled, index);
        p->save();
        p->setClipRect(option.rect);
        p->fillRect(option.rect, index.data(Qt::BackgroundRole).value<QBrush>());
        p->setPen(Qt::black);
        p->setFont(styled.font);
        const QRect textRect = option.rect.adjusted(6, 1, -6, -1);
        const QFontMetrics metrics(styled.font);
        if (index.data(Qt::UserRole + 4).toInt() == 0) {
            auto lines = index.data(Qt::DisplayRole).toString().split('\n');
            for (auto &line : lines)
                line = metrics.elidedText(line, Qt::ElideRight, textRect.width());
            p->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, lines.join('\n'));
        } else {
            const auto d = index.data(Qt::UserRole).value<Ft8::Decode>();
            const bool tx = index.data(Qt::UserRole + 1).toBool();
            const auto report = tx ? QString("TX") : d.snr ? Ft8::reportText(*d.snr) : QString("—");
            // Use fixed-width numeric columns with the app font.
            QRect column(textRect.left(), textRect.top(), metrics.horizontalAdvance("-99"), textRect.height());
            p->drawText(column, Qt::AlignRight | Qt::AlignVCenter, report);
            column.moveLeft(column.right() + 7);
            column.setWidth(metrics.horizontalAdvance("3300"));
            p->drawText(column, Qt::AlignRight | Qt::AlignVCenter, QString::number(d.audioHz));
            const QRect messageRect(column.right() + 8, textRect.top(), qMax(0, textRect.right() - column.right() - 7),
                                    textRect.height());
            p->drawText(messageRect, Qt::AlignLeft | Qt::AlignVCenter,
                        metrics.elidedText(d.message, Qt::ElideRight, messageRect.width()));
        }
        if (option.state & QStyle::State_Selected) {
            p->setPen(QPen(QColor("#176299"), 2));
            p->setBrush(Qt::NoBrush);
            p->drawRect(option.rect.adjusted(1, 1, -2, -2));
        }
        p->restore();
    }
};
QPushButton *button(const QString &text, QWidget *parent, const QString &name = {}) {
    auto *b = new QPushButton(text, parent);
    b->setObjectName(name);
    b->setMinimumHeight(34);
    b->setCursor(Qt::PointingHandCursor);
    b->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    return b;
}
QPushButton *compactButton(const QString &text, QWidget *parent, const QString &name = {}, int height = 28) {
    auto *b = button(text, parent, name);
    b->setProperty("ft8Compact", true);
    b->setFixedHeight(height);
    return b;
}
QLabel *label(const QString &text, QWidget *parent) {
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    l->setTextFormat(Qt::PlainText);
    return l;
}
void touchScroll(QAbstractScrollArea *area) {
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QScroller::grabGesture(area->viewport(), QScroller::TouchGesture);
    auto *scroller = QScroller::scroller(area->viewport());
    auto p = scroller->scrollerProperties();
    p.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.25);
    p.setScrollMetric(QScrollerProperties::DragStartDistance, 0.0015);
    scroller->setScrollerProperties(p);
    QObject::connect(scroller, &QScroller::stateChanged, area, [area](QScroller::State s) {
        if (s == QScroller::Dragging || s == QScroller::Scrolling) {
            area->setProperty("ft8Dragging", true);
            for (auto *b : area->findChildren<QAbstractButton *>())
                b->setDown(false);
        } else if (s == QScroller::Inactive) {
            QTimer::singleShot(120, area, [area] { area->setProperty("ft8Dragging", false); });
        }
    });
}
struct Sheet {
    InWindowDialog dialog;
    QVBoxLayout *layout;
    QScrollArea *scroll;
    QWidget *body;
    QVBoxLayout *contents;
    explicit Sheet(QWidget *parent, const QString &title) : dialog(parent) {
        dialog.setPanelSize(QSize(420, qMax(260, parent->height() - 32)));
        auto *panel = dialog.contentWidget();
        layout = new QVBoxLayout(panel);
        layout->setSpacing(10);
        auto *heading = label(title, panel);
        heading->setStyleSheet("font-size:16px;font-weight:700;color:#6ce1ed;");
        layout->addWidget(heading);
        scroll = new QScrollArea(panel);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        touchScroll(scroll);
        body = new QWidget(scroll);
        body->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        contents = new QVBoxLayout(body);
        contents->setContentsMargins(2, 2, 2, 2);
        contents->setSpacing(9);
        scroll->setWidget(body);
        layout->addWidget(scroll, 1);
        auto *close = button("Back", panel);
        layout->addWidget(close);
        QObject::connect(close, &QPushButton::clicked, &dialog, &InWindowDialog::reject);
    }
    QPushButton *action(const QString &text) {
        auto *b = button(text, body);
        contents->addWidget(b);
        return b;
    }
    bool dragging() const { return scroll->property("ft8Dragging").toBool(); }
};
QString mhz(qint64 hz) {
    return QString::number(double(hz) / 1e6, 'f', 6);
}
QString snr(const Ft8::Decode &d) {
    return d.snr ? Ft8::reportText(*d.snr) + " dB" : QString("SNR —");
}
} // namespace

Ft8Screen::Ft8Screen(QWidget *parent, const QString &storageRoot)
    : QWidget(parent),
      m_logbook((storageRoot.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logbook"
                                       : storageRoot) +
                "/contacts.json"),
      m_storageRoot(storageRoot) {
    setObjectName("ft8Screen");
    setAttribute(Qt::WA_StyledBackground);
    setFocusPolicy(Qt::StrongFocus);
    setStyleSheet(
        "QWidget#ft8Screen { background:#0b121c; color:#e8eef5; }"
        "QLabel { color:#b7c9d9; font-size:11px; background:transparent; }"
        "QPushButton { color:#dbe8f3; background:#172331; border:1px solid #2d4154; border-radius:6px; padding:5px "
        "8px; font-size:11px; }"
        "QPushButton[ft8Compact=\"true\"] { padding:0 5px; border-radius:4px; }"
        "QPushButton:pressed { background:#29445a; } QPushButton:checked { color:#08171b; background:#6ce1ed; "
        "border-color:#6ce1ed; }"
        "QPushButton:disabled { color:#77828e; background:#141c27; border-color:#25303f; }"
        "QPushButton#ft8Call { background:#255642; border-color:#56b88d; color:#e4ffef; font-weight:bold; }"
        "QPushButton#ft8Halt { color:#ffb3ab; border-color:#a04e49; }"
        "QListWidget { color:#e8eef5; background:#0e1824; border:1px solid #26394a; border-radius:6px; font-size:12px; "
        "outline:0; }"
        "QListWidget::item { padding:5px; border-bottom:1px solid #1b2b3c; }"
        "QListWidget::item:selected { background:#1e485c; color:#fff; }"
        "QLineEdit,QPlainTextEdit { background:#111f2b; color:#fff; border:1px solid #405569; border-radius:5px; "
        "padding:8px; font-size:13px; }"
        "QCheckBox { color:#e2edf5; padding:7px 0; font-size:12px; }"
        "QCheckBox::indicator { width:22px; height:22px; border:1px solid #6ce1ed; border-radius:4px; "
        "background:#14222f; }"
        "QCheckBox::indicator:checked { background:#6ce1ed; image:url(:/icons/check.svg); }"
        "QFrame#inWindowDialogPanel { background:#101b28; border:1px solid #395468; border-radius:10px; }"
        "QScrollArea { background:transparent; border:0; }"
        "QScrollArea > QWidget > QWidget { background:#101b28; }"
        "QProgressBar { height:3px; background:#1b2c3c; border:0; } QProgressBar::chunk { background:#5ce0ec; }"
        "QScrollBar:vertical { width:5px; background:#12202e; } QScrollBar::handle:vertical { background:#425d74; "
        "min-height:24px; }");
    QSettings s("QK4", "QK4");
    m_session.myCall = s.value("ft8/call").toString();
    m_session.myGrid = s.value("ft8/grid").toString();
    m_session.mode = s.value("ft8/mode").toString() == "FT4" ? Ft8::Mode::FT4 : Ft8::Mode::FT8;
    m_session.holdTx = s.value("ft8/holdTx", true).toBool();
    m_session.txHz = qBound(100, s.value("ft8/txHz", 1500).toInt(), 3200);
    m_session.rxHz = qBound(100, s.value("ft8/rxHz", 1500).toInt(), 3200);
    m_session.maxRetries = qBound(1, s.value("ft8/retries", 6).toInt(), 20);
    m_session.callFirst = s.value("ft8/callFirst", true).toBool();
    m_session.useRr73 = s.value("ft8/rr73", true).toBool();
    m_autoLog = s.value("ft8/autoLog", false).toBool();
    m_cqDirection = s.value("ft8/direction").toString();
    m_frequency = s.value("ft8/frequency", 14074000).toLongLong();
    m_showWaterfall = s.value("ft8/showWaterfall", true).toBool();
    m_rowDensity = qBound(0, s.value("ft8/rowDensity", 0).toInt(), 2);
    const int savedAudioStep = s.value("ft8/audioStepHz", 5).toInt();
    if (QList<int>{1, 5, 10, 25, 50}.contains(savedAudioStep))
        m_audioStepHz = savedAudioStep;
    QString error;
    m_logReady = m_logbook.load(&error);
    if (!m_logReady)
        m_notice = error;
    setupUi();
    updateDisplay();
    m_timer = new QTimer(this);
    m_timer->setInterval(100);
    connect(m_timer, &QTimer::timeout, this, &Ft8Screen::tick);
    m_timer->start();
    refresh();
}
Ft8Screen::~Ft8Screen() {
    savePreferences();
}
void Ft8Screen::savePreferences() {
    if (!m_storageRoot.isEmpty())
        return; // Test/preview instances do not change station preferences.
    QSettings s("QK4", "QK4");
    if (!m_practice) {
        s.setValue("ft8/call", m_session.myCall);
        s.setValue("ft8/grid", m_session.myGrid);
    }
    s.setValue("ft8/mode", Ft8::modeName(m_session.mode));
    s.setValue("ft8/frequency", m_frequency);
    s.setValue("ft8/holdTx", m_session.holdTx);
    s.setValue("ft8/txHz", m_session.txHz);
    s.setValue("ft8/rxHz", m_session.rxHz);
    s.setValue("ft8/retries", m_session.maxRetries);
    s.setValue("ft8/callFirst", m_session.callFirst);
    s.setValue("ft8/rr73", m_session.useRr73);
    s.setValue("ft8/autoLog", m_autoLog);
    s.setValue("ft8/direction", m_cqDirection);
    s.setValue("ft8/showWaterfall", m_showWaterfall);
    s.setValue("ft8/rowDensity", m_rowDensity);
    s.setValue("ft8/audioStepHz", m_audioStepHz);
}
void Ft8Screen::setupUi() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 8, 10, 8);
    root->setSpacing(4);
    root->setSizeConstraint(QLayout::SetNoConstraint);
    auto *top = new QHBoxLayout;
    auto *back = button("‹ Radio", this, "ft8Back");
    m_waterfallToggle = button("Hide waterfall", this, "ft8ToggleWaterfall");
    m_waterfallToggle->setCheckable(true);
    m_waterfallToggle->setChecked(m_showWaterfall);
    m_waterfallToggle->setAccessibleName("Show waterfall");
    auto *rows = button("Rows ▾", this, "ft8Rows");
    auto *opts = button("Options", this, "ft8Options");
    top->addWidget(back);
    top->addWidget(m_waterfallToggle, 1);
    top->addWidget(rows);
    top->addWidget(opts);
    root->addLayout(top);
    connect(back, &QPushButton::clicked, this, [this] {
        suspend();
        emit closeRequested();
    });
    connect(opts, &QPushButton::clicked, this, &Ft8Screen::options);
    connect(rows, &QPushButton::clicked, this, &Ft8Screen::rowOptions);
    connect(m_waterfallToggle, &QPushButton::toggled, this, [this](bool shown) {
        m_showWaterfall = shown;
        updateDisplay();
        savePreferences();
    });
    auto *tuning = new QHBoxLayout;
    m_band = button("20m ▾", this, "ft8Band");
    m_mode = button("FT8", this, "ft8Mode");
    m_frequencyDisplay = new FrequencyDisplayWidget(this);
    m_frequencyDisplay->setObjectName("ft8Frequency");
    m_frequencyDisplay->setAutoFit(true);
    m_frequencyDisplay->setTouchTuningEnabled(true);
    auto frequencyPalette = m_frequencyDisplay->palette();
    frequencyPalette.setColor(QPalette::Window, QColor("#0b121c"));
    m_frequencyDisplay->setPalette(frequencyPalette);
    m_frequencyDisplay->setAutoFillBackground(true);
    m_frequencyDisplay->setAccessibleName("Radio frequency. Tap a digit for dial step; hold to enter a frequency.");
    m_frequencyDisplay->setFixedHeight(34);
    tuning->addWidget(m_band);
    tuning->addWidget(m_frequencyDisplay, 1);
    tuning->addWidget(m_mode);
    root->addLayout(tuning);
    connect(m_band, &QPushButton::clicked, this, &Ft8Screen::selectBand);
    connect(m_frequencyDisplay, &FrequencyDisplayWidget::tuningDigitSelected, this, [this](int digit) {
        m_rfDigit = qBound(0, digit, FrequencyDisplayWidget::kMaxDigitIndex);
        m_rfStepHz = 1;
        for (int i = 0; i < m_rfDigit; ++i)
            m_rfStepHz *= 10;
        selectDialTarget(DialTarget::RadioFrequency);
    });
    connect(m_frequencyDisplay, &FrequencyDisplayWidget::directEntryRequested, this, &Ft8Screen::enterFrequency);
    connect(m_frequencyDisplay, &FrequencyDisplayWidget::frequencyScrolled, this, &Ft8Screen::tuneDial);
    connect(m_mode, &QPushButton::clicked, this, &Ft8Screen::changeMode);
    auto *powerRow = new QHBoxLayout;
    powerRow->setSpacing(6);
    auto *powerTitle = label("RF power", this);
    powerTitle->setWordWrap(false);
    powerRow->addWidget(powerTitle);
    m_powerSlider = new Ft8PowerSlider(this);
    m_powerSlider->setObjectName("ft8Power");
    m_powerSlider->setAccessibleName("RF power in watts");
    m_powerSlider->setRange(1, 110); // Same operating range as the SSTV power control.
    m_powerSlider->setPageStep(5);
    m_powerSlider->setTracking(false); // Commit a drag on release, without flooding CAT.
    m_powerSlider->setFixedHeight(28);
    m_powerSlider->setStyleSheet(
        "QSlider {background:transparent;}"
        "QSlider::groove:horizontal {height:4px;background:#33465b;border-radius:2px;}"
        "QSlider::sub-page:horizontal {background:#65ccf0;border-radius:2px;}"
        "QSlider::handle:horizontal {width:22px;margin:-10px 0;background:#65ccf0;"
        "border:1px solid #b3eaff;border-radius:11px;}"
        "QSlider::handle:horizontal:disabled {background:#526171;border-color:#647180;}"
        "QSlider::sub-page:horizontal:disabled {background:#526171;}");
    powerRow->addWidget(m_powerSlider, 1);
    m_powerLabel = label("— W", this);
    m_powerLabel->setObjectName("ft8PowerValue");
    m_powerLabel->setWordWrap(false);
    m_powerLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_powerLabel->setFixedWidth(48);
    powerRow->addWidget(m_powerLabel);
    root->addLayout(powerRow);
    connect(m_powerSlider, &QSlider::sliderMoved, this,
            [this](int watts) { m_powerLabel->setText(QString("%1 W").arg(watts)); });
    connect(m_powerSlider, &QSlider::valueChanged, this, [this](int watts) {
        if (!m_powerSlider->isEnabled()) {
            updatePowerUi();
            return;
        }
        if (m_practice)
            m_practicePowerWatts = watts;
        else {
            m_rfPowerWatts = watts;
            emit powerRequested(watts);
        }
        updatePowerUi();
    });
    m_banner = label("Receive only", this);
    m_banner->setObjectName("ft8Banner");
    root->addWidget(m_banner);
    m_waterfall = new Ft8Waterfall(this);
    // Give remaining vertical space to traffic. The waterfall has a compact
    // height limit and does not grow when the QSO is idle.
    root->addWidget(m_waterfall);
    connect(m_waterfall, &Ft8Waterfall::stationSelected, this, &Ft8Screen::selectStation);
    connect(m_waterfall, &Ft8Waterfall::ambiguousStations, this, &Ft8Screen::chooseStation);
    connect(m_waterfall, &Ft8Waterfall::frequencySelected, this, [this](int hz) {
        selectDialTarget(DialTarget::RxAudio);
        m_rxFocusHz = 0;
        m_session.rxHz = hz;
        if (!m_session.holdTx)
            m_session.txHz = hz;
        filterActivity();
        refresh();
    });
    connect(m_waterfall, &Ft8Waterfall::txFrequencySelected, this, [this](int hz) {
        selectDialTarget(DialTarget::TxAudio);
        m_session.txHz = hz;
        m_session.holdTx = true;
        m_notice = QString("TX %1 Hz · held. Tap a station to receive.").arg(hz);
        savePreferences();
        refresh();
    });
    auto *waterControls = new QHBoxLayout;
    m_offsets = button("RX 1500 · TX 1500", this, "ft8Offsets");
    m_offsets->setStyleSheet("font-size:10px;padding:1px 4px;");
    auto *out = button("−", this, "ft8ZoomOut"), *in = button("+", this, "ft8ZoomIn"),
         *fit = button("Fit", this, "ft8Fit");
    for (auto *b : {out, in, fit})
        b->setMaximumWidth(44);
    m_zoomControls = {out, in, fit};
    waterControls->addWidget(m_offsets, 1);
    waterControls->addWidget(out);
    waterControls->addWidget(in);
    waterControls->addWidget(fit);
    root->addLayout(waterControls);
    connect(m_offsets, &QPushButton::clicked, this, &Ft8Screen::dialOptions);
    connect(out, &QPushButton::clicked, this, [this] { m_waterfall->zoom(0.5); });
    connect(in, &QPushButton::clicked, this, [this] { m_waterfall->zoom(2); });
    connect(fit, &QPushButton::clicked, m_waterfall, &Ft8Waterfall::fit);
    auto *tabs = new QHBoxLayout;
    QStringList filters{"All", "CQ", "My QSO"};
    for (int i = 0; i < filters.size(); ++i) {
        auto *b = compactButton(filters[i], this, "ft8Filter" + QString::number(i));
        b->setCheckable(true);
        b->setChecked(i == 0);
        tabs->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, i, b] {
            m_filter = i;
            for (int j = 0; j < 3; ++j)
                findChild<QPushButton *>("ft8Filter" + QString::number(j))->setChecked(j == i);
            b->setChecked(true);
            m_unreadActivity = 0;
            filterActivity();
            updateNewActivity();
        });
    }
    auto *logs = compactButton("Logbook", this, "ft8Logbook");
    tabs->addWidget(logs);
    connect(logs, &QPushButton::clicked, this, &Ft8Screen::showLogbook);
    root->addLayout(tabs);
    m_activity = new QListWidget(this);
    m_activity->setObjectName("ft8Activity");
    m_activity->setItemDelegate(new Ft8ActivityDelegate(m_activity));
    m_activity->setMinimumHeight(80);
    m_activity->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_activity->setTextElideMode(Qt::ElideRight);
    m_activity->setSelectionMode(QAbstractItemView::SingleSelection);
    m_activity->setUniformItemSizes(true);
    touchScroll(m_activity);
    m_activity->viewport()->installEventFilter(this);
    root->addWidget(m_activity, 1);
    m_newActivity = button("New messages ↓", m_activity->viewport(), "ft8NewActivity");
    m_newActivity->hide();
    connect(m_newActivity, &QPushButton::clicked, this, [this] {
        m_activity->scrollToBottom();
        m_unreadActivity = 0;
        updateNewActivity();
    });
    connect(m_activity->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] {
        if (m_updatingActivity)
            return;
        auto *bar = m_activity->verticalScrollBar();
        if (bar->value() >= bar->maximum() - 5)
            m_unreadActivity = 0;
        updateNewActivity();
    });
    connect(m_activity, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (m_ignoreNextClick || m_activity->property("ft8Dragging").toBool())
            return;
        selectStation(item->data(Qt::UserRole).value<Ft8::Decode>(), false);
    });
    connect(m_activity, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (m_ignoreNextClick || m_activity->property("ft8Dragging").toBool())
            return;
        selectStation(item->data(Qt::UserRole).value<Ft8::Decode>(), true);
    });
    auto *card = new QFrame(this);
    card->setObjectName("ft8QsoCard");
    card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    card->setStyleSheet("QFrame#ft8QsoCard { background:#142232;border:1px solid #2b4356;border-radius:8px; }");
    auto *qso = new QVBoxLayout(card);
    qso->setContentsMargins(6, 4, 6, 4);
    qso->setSpacing(2);
    auto *qsoTop = new QHBoxLayout;
    auto *summary = new QVBoxLayout;
    summary->setSpacing(0);
    m_partner = label("Choose a station to call", card);
    m_partner->setObjectName("ft8Partner");
    m_partner->setStyleSheet("font-size:12px;font-weight:600;color:#f0f7fc;");
    m_partner->setWordWrap(false);
    m_partner->setMinimumHeight(18);
    m_partner->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    summary->addWidget(m_partner);
    m_exchange = label({}, card);
    m_exchange->setObjectName("ft8Exchange");
    m_exchange->setStyleSheet("font-size:10px;");
    m_exchange->setWordWrap(false);
    summary->addWidget(m_exchange);
    auto *clear = compactButton("Clear", card, "ft8Clear", 28);
    qsoTop->addLayout(summary, 1);
    qsoTop->addWidget(clear);
    qso->addLayout(qsoTop);
    connect(clear, &QPushButton::clicked, this, [this] {
        (stopLiveTransmit(), m_session.clear());
        ++m_practiceGeneration;
        m_notice.clear();
        m_completionShown = false;
        filterActivity();
        refresh();
    });
    m_next = compactButton("Next message", card, "ft8Next", 28);
    qso->addWidget(m_next);
    connect(m_next, &QPushButton::clicked, this, &Ft8Screen::chooseMessage);
    auto *qsoSettings = new QHBoxLayout;
    m_auto = compactButton("Auto", card, "ft8Auto");
    m_auto->setCheckable(true);
    m_auto->setChecked(true);
    m_period = compactButton("Even / 1st", card, "ft8Period");
    auto *log = compactButton("Log QSO", card, "ft8LogQso");
    qsoSettings->addWidget(m_auto);
    qsoSettings->addWidget(m_period);
    qsoSettings->addWidget(log);
    qso->addLayout(qsoSettings);
    root->addWidget(card);
    connect(m_auto, &QPushButton::toggled, this, [this](bool enabled) {
        m_session.autoSequence = enabled;
        refresh();
    });
    connect(m_period, &QPushButton::clicked, this, [this] {
        m_session.even = !m_session.even;
        refresh();
    });
    connect(log, &QPushButton::clicked, this, [this] { logContact(); });
    m_status = label("Connect to the K4 for live reception, or open Options to practice.", this);
    m_status->setMinimumHeight(0);
    m_status->setObjectName("ft8Status");
    root->addWidget(m_status);
    m_cycle = new QProgressBar(this);
    m_cycle->setTextVisible(false);
    m_cycle->setFixedHeight(3);
    root->addWidget(m_cycle);
    auto *actions = new QHBoxLayout;
    m_rx = button("Receive", this, "ft8Receive");
    m_rx->setCheckable(true);
    m_rx->setChecked(true);
    m_cq = button("CQ", this, "ft8Cq");
    m_call = button("Call", this, "ft8Call");
    m_halt = button("Halt TX", this, "ft8Halt");
    for (auto *b : {m_rx, m_cq, m_call, m_halt}) {
        b->setMinimumHeight(42);
        actions->addWidget(b, 1);
    }
    root->addLayout(actions);
    connect(m_rx, &QPushButton::toggled, this, [this](bool on) {
        m_receiving = on;
        if (!on)
            (stopLiveTransmit(), m_session.halt());
        emit captureChanged();
        refresh();
    });
    connect(m_call, &QPushButton::clicked, this, &Ft8Screen::startCall);
    connect(m_cq, &QPushButton::clicked, this, [this] {
        if (m_session.callCq(m_cqDirection)) {
            m_completionShown = false;
            ++m_practiceGeneration;
            startCall();
        } else {
            m_notice = "Set your callsign and grid in Options.";
            refresh();
        }
    });
    connect(m_halt, &QPushButton::clicked, this, [this] {
        (stopLiveTransmit(), m_session.halt());
        ++m_practiceGeneration;
        m_notice = "Stopped · listening";
        refresh();
    });
}
bool Ft8Screen::dialAvailable() const {
    if (!isVisible() || !isEnabled() || m_radioTx || m_livePending)
        return false;
    for (auto *dialog : findChildren<InWindowDialog *>())
        if (dialog->isVisible())
            return false;
    return true;
}
void Ft8Screen::selectDialTarget(DialTarget target) {
    m_dialPreviewActive = false;
    m_dialCommitRequired = false;
    m_dialTarget = target;
    if (target != DialTarget::RadioFrequency)
        m_lastToneTarget = target;
    refresh();
}
void Ft8Screen::beginDialAdjustment() {
    beginDialAdjustment(m_lastToneTarget == DialTarget::RxAudio ? DialTarget::TxAudio : DialTarget::RxAudio);
}
void Ft8Screen::beginDialAdjustment(DialTarget target) {
    m_dialTarget = m_lastToneTarget = target;
    if (!QList<int>{1, 5, 10, 25, 50}.contains(m_audioStepHz))
        m_audioStepHz = 5;
    m_dialCommitRequired = true;
    m_dialPreviewActive = true;
    m_dialPreviewHz = m_dialTarget == DialTarget::TxAudio ? m_session.txHz : m_session.rxHz;
    m_notice = QString("%1 tone selected · turn dial, then Set tone frequency")
                   .arg(target == DialTarget::TxAudio ? "TX" : "RX");
    refresh();
}
void Ft8Screen::commitDialAdjustment() {
    if (!m_dialPreviewActive) {
        m_notice = "Select an RX or TX tone, turn the dial, then Set tone frequency.";
        refresh();
        return;
    }
    m_dialPreviewActive = false;
    if (m_dialTarget == DialTarget::TxAudio) {
        m_session.txHz = m_dialPreviewHz;
        m_session.holdTx = true;
        m_notice = QString("TX set: %1 Hz · Hold TX").arg(m_session.txHz);
    } else {
        m_session.rxHz = m_dialPreviewHz;
        m_rxFocusHz = m_session.rxHz;
        m_notice = QString("RX set: %1 Hz · My QSO").arg(m_rxFocusHz);
        findChild<QPushButton *>("ft8Filter2")->click();
    }
    savePreferences();
    refresh();
}
void Ft8Screen::tuneDial(int steps) {
    if (!steps || !dialAvailable())
        return;
    if (m_dialTarget != DialTarget::RadioFrequency) {
        // Wheel movement always previews a tone. A committed tone stays put
        // until a selector starts the next adjustment, regardless of mapping.
        if (!m_dialCommitRequired)
            beginDialAdjustment(m_dialTarget);
        if (m_dialPreviewActive) {
            m_dialPreviewHz = int(qBound(100LL, qint64(m_dialPreviewHz) + qint64(steps) * m_audioStepHz, 3200LL));
            refresh();
        }
        return;
    }
    if (m_dialTarget == DialTarget::RadioFrequency) {
        // Accumulate fast detents while awaiting CAT confirmation. Older
        // echoes do not become the next request's starting frequency.
        if (QDateTime::currentMSecsSinceEpoch() - m_rfRequestUtc > 1000)
            m_pendingRfHz = 0;
        const qint64 current = m_pendingRfHz > 0 ? m_pendingRfHz : m_frequency;
        const qint64 next = current + qint64(steps) * m_rfStepHz;
        if (next > 0 && next <= 9999999999LL)
            setFrequency(next);
        return;
    }
}
bool Ft8Screen::handleMidiKnobAction(const QString &action, int value, bool absolute) {
    // Only the user's explicit target selection owns this dial. RF selectors
    // and step buttons cannot redirect it away from an RX/TX tone.
    // Existing CTR2 maps use VFO actions for the wheel. In this module those
    // wheel events follow the selected tone too; the RF guard belongs on the
    // buttons that could change that selection or its step, not on rotation.
    if (action == "selected_adjustment" || action == "active_vfo_frequency" ||
        action == "other_vfo_frequency") {
        if (!absolute)
            tuneDial(value);
        return true;
    }
    if (action == "pan_zoom") {
        if (!absolute && dialAvailable())
            m_waterfall->zoom(std::pow(1.2, qBound(-64, value, 64)));
        return true;
    }
    return false;
}
bool Ft8Screen::handleMidiButtonAction(const QString &action) {
    const bool rfSelector = action == "adjust_active_vfo_frequency" || action == "adjust_other_vfo_frequency";
    const bool rfButton = rfSelector || action == "tune_step" || action == "khz" ||
                          action == "band_up" || action == "band_down";
    // Consume blocked actions so they cannot fall through to console CAT or
    // change its selected adjustment. A frequency-digit tap is the RF opt-in.
    if (rfButton && (!dialAvailable() || m_dialTarget != DialTarget::RadioFrequency))
        return true;
    if (rfSelector)
        return true; // The tapped RF digit already selected the module's dial.
    if (action != "adjust_ft8_rx_tx" && action != "set_ft8_frequency" &&
        action != "pan_zoom_in" && action != "pan_zoom_out" &&
        action != "tune_step" && action != "khz")
        return false;
    if (!dialAvailable())
        return true;
    if (action == "adjust_ft8_rx_tx") {
        beginDialAdjustment();
        return true;
    }
    if (action == "set_ft8_frequency") {
        commitDialAdjustment();
        return true;
    }
    if (action == "pan_zoom_in" || action == "pan_zoom_out")
        m_waterfall->zoom(action == "pan_zoom_in" ? 2 : 0.5);
    else if (m_dialTarget == DialTarget::RadioFrequency) {
        m_rfDigit = action == "khz" ? 3 : m_rfDigit >= 6 ? 0 : m_rfDigit + 1;
        m_rfStepHz = 1;
        for (int i = 0; i < m_rfDigit; ++i)
            m_rfStepHz *= 10;
    }
    refresh();
    return true;
}
void Ft8Screen::dialOptions() {
    Sheet sheet(this, "FT8/FT4 tone tuning");
    sheet.contents->addWidget(label("Choose the RX or TX tone, then turn the dial to position its dashed marker. "
                                    "Use Set tone frequency here or your assigned Set button to commit. "
                                    "These controls do not change the radio's dial frequency.", sheet.body));
    for (const auto target : {DialTarget::RxAudio, DialTarget::TxAudio}) {
        auto *choice = sheet.action(target == DialTarget::RxAudio ? "Select RX tone" : "Select TX tone");
        choice->setCheckable(true);
        choice->setChecked(m_dialTarget == target);
        connect(choice, &QPushButton::clicked, &sheet.dialog, [&, target] {
            if (sheet.dragging() || m_radioTx || m_livePending)
                return;
            beginDialAdjustment(target);
            sheet.dialog.accept();
        });
    }
    auto *setTone = sheet.action("Set tone frequency");
    setTone->setEnabled(m_dialPreviewActive && !m_radioTx && !m_livePending);
    connect(setTone, &QPushButton::clicked, &sheet.dialog, [&] {
        if (sheet.dragging() || m_radioTx || m_livePending)
            return;
        commitDialAdjustment();
        sheet.dialog.accept();
    });
    sheet.contents->addWidget(label("Tone step — selects tone control, never RF", sheet.body));
    for (int step : {1, 5, 10, 25, 50}) {
        auto *choice = sheet.action(QString("Tone step: %1 Hz").arg(step));
        choice->setCheckable(true);
        choice->setChecked(m_audioStepHz == step);
        connect(choice, &QPushButton::clicked, &sheet.dialog, [&, step] {
            if (sheet.dragging() || m_radioTx || m_livePending)
                return;
            m_audioStepHz = step;
            if (!m_dialPreviewActive)
                beginDialAdjustment(m_lastToneTarget);
            savePreferences();
            refresh();
            sheet.dialog.accept();
        });
    }
    sheet.dialog.exec();
}
void Ft8Screen::formatActivityItem(QListWidgetItem *item) {
    const auto d = item->data(Qt::UserRole).value<Ft8::Decode>();
    const bool tx = item->data(Qt::UserRole + 1).toBool();
    const bool worked = item->data(Qt::UserRole + 3).toBool();
    const auto full = d.message + "\n" + d.utc.toUTC().toString("HH:mm:ss") + "   " + (tx ? "TX" : snr(d)) +
                      QString("   %1 Hz").arg(d.audioHz) + (worked ? "   Worked" : "");
    QFont font = m_activity->font();
    font.setPixelSize(m_rowDensity == 2 ? 10 : 12);
    item->setFont(font);
    item->setData(Qt::UserRole + 4, m_rowDensity);
    item->setSizeHint(QSize(0, m_rowDensity == 0 ? 47 : m_rowDensity == 1 ? 28 : 20));
    item->setText(m_rowDensity == 0 ? full
                                    : QString("%1 %2  %3")
                                          .arg(tx      ? QString("TX")
                                               : d.snr ? Ft8::reportText(*d.snr)
                                                       : QString("—"),
                                               3)
                                          .arg(d.audioHz, 4)
                                          .arg(d.message));
    item->setData(Qt::AccessibleTextRole, full);
    item->setToolTip(full);
}
void Ft8Screen::updateDisplay() {
    m_waterfallToggle->setText(m_showWaterfall ? "Hide waterfall" : "Show waterfall");
    m_waterfallToggle->setAccessibleName(m_waterfallToggle->text());
    auto *bar = m_activity->verticalScrollBar();
    const bool following = bar->value() >= bar->maximum() - 5;
    auto *anchor = m_activity->itemAt(QPoint(2, 2));
    const int top = anchor ? m_activity->visualItemRect(anchor).top() : 0;
    m_updatingActivity = true;
    m_waterfall->setVisible(m_showWaterfall);
    for (auto *control : m_zoomControls)
        control->setVisible(m_showWaterfall);
    for (int i = 0; i < m_activity->count(); ++i)
        formatActivityItem(m_activity->item(i));
    layout()->activate();
    m_activity->doItemsLayout();
    if (following)
        m_activity->scrollToBottom();
    else if (anchor) {
        m_activity->scrollToItem(anchor, QAbstractItemView::PositionAtTop);
        bar->setValue(bar->value() - top);
    }
    m_updatingActivity = false;
    updateNewActivity();
}
void Ft8Screen::rowOptions() {
    Sheet sheet(this, "Station rows");
    sheet.contents->addWidget(label("Choose how many messages fit on screen. Compact and Dense show SNR, "
                                    "audio Hz, and the message on one line. Comfortable also shows UTC.",
                                    sheet.body));
    const QStringList names{"Comfortable · two lines", "Compact · one line", "Dense · most messages"};
    for (int i = 0; i < names.size(); ++i) {
        auto *choice = sheet.action(names[i]);
        choice->setObjectName("ft8Density" + QString::number(i));
        choice->setCheckable(true);
        choice->setChecked(m_rowDensity == i);
        connect(choice, &QPushButton::clicked, &sheet.dialog, [&, this, i] {
            if (sheet.dragging())
                return;
            m_rowDensity = i;
            updateDisplay();
            savePreferences();
            sheet.dialog.accept();
        });
    }
    sheet.contents->addWidget(
        label("Dense uses smaller text and touch targets. Tap a row to select its station; "
              "check the partner below before calling. Hide Waterfall to give the list more room.",
              sheet.body));
    sheet.contents->addStretch();
    sheet.dialog.exec();
}
void Ft8Screen::updateNewActivity() {
    if (!m_newActivity)
        return;
    const auto *bar = m_activity->verticalScrollBar();
    if (!m_updatingActivity && bar->value() >= bar->maximum() - 5)
        m_unreadActivity = 0;
    m_newActivity->setVisible(m_unreadActivity > 0);
    if (!m_unreadActivity)
        return;
    m_newActivity->setText(QString("%1 new ↓").arg(m_unreadActivity));
    m_newActivity->adjustSize();
    m_newActivity->move(qMax(0, m_activity->viewport()->width() - m_newActivity->width() - 5),
                        qMax(0, m_activity->viewport()->height() - m_newActivity->height() - 5));
    m_newActivity->raise();
}
void Ft8Screen::refresh() {
    updatePowerUi();
    m_mode->setText(Ft8::modeName(m_session.mode));
    m_frequencyDisplay->setFrequency(QString::number(m_frequency));
    m_frequencyDisplay->setSelectedTuningDigit(m_dialTarget == DialTarget::RadioFrequency ? m_rfDigit : -1);
    QString band = Ft8::bandFor(m_frequency);
    bool standard = false;
    for (const auto &b : Ft8::bands())
        if (m_frequency == (m_session.mode == Ft8::Mode::FT4 ? b.ft4Hz : b.ft8Hz))
            standard = true;
    m_band->setText((band.isEmpty() ? QString("Custom") : band) + " ▾");
    m_banner->setText(m_practice ? "PRACTICE · No RF · Separate practice log"
                                 : QString("%1 · %2%3")
                                       .arg(m_connected ? "K4 · Live FT8/FT4" : "K4 disconnected", m_radioMode,
                                            standard ? "" : " · Custom frequency"));
    m_banner->setStyleSheet(m_practice ? "color:#ffca80;" : "color:#87a9c3;");
    if (!m_transmitProtection.isEmpty() && !m_practice) {
        m_banner->setText(m_transmitProtection);
        m_banner->setStyleSheet(m_transmitProtectionFault ? "color:#ff8e8e;font-weight:700;"
                                                        : "color:#9ee4af;");
    }
    m_partner->setText(m_session.callingCq && m_session.dxCall.isEmpty() ? "Calling CQ"
                       : m_session.dxCall.isEmpty()                      ? "Choose a station to call"
                                                                         : m_session.dxCall + "  " + m_session.dxGrid);
    const bool haveExchange = !m_session.dxCall.isEmpty();
    const bool haveMessage = !m_session.nextMessage.isEmpty();
    m_exchange->setVisible(haveExchange);
    m_next->setVisible(haveMessage);
    findChild<QPushButton *>("ft8Clear")->setVisible(haveExchange || haveMessage);
    const QString target = m_dialTarget == DialTarget::RxAudio   ? "RX"
                           : m_dialTarget == DialTarget::TxAudio ? "TX"
                                                                 : "RF";
    const QString dialState = m_dialPreviewActive
        ? QString("Adjust %1: %2 Hz · %3 Hz step").arg(target).arg(m_dialPreviewHz).arg(m_audioStepHz)
        : m_dialCommitRequired ? (m_dialTarget == DialTarget::TxAudio ? QString("TX set · Hold TX")
                                                                     : QString("RX set · My QSO"))
                              : QString("%1 %2 · %3 Hz").arg(target)
                                    .arg(m_dialTarget == DialTarget::RadioFrequency ? "dial" : "tone")
                                    .arg(dialStepHz());
    m_offsets->setText(QString("RX %1   TX %2\n%3")
                           .arg(m_session.rxHz)
                           .arg(m_session.txHz)
                           .arg(dialState));
    m_offsets->setToolTip(m_session.holdTx ? "TX frequency is held" : "RX and TX frequencies are linked");
    m_waterfall->setMarkers(m_session.rxHz, m_session.txHz);
    m_waterfall->setPreview(m_dialPreviewActive ? m_dialPreviewHz : 0, m_dialTarget == DialTarget::TxAudio);
    auto *qsoFilter = findChild<QPushButton *>("ft8Filter2");
    qsoFilter->setToolTip(m_rxFocusHz > 0
        ? QString("RX traffic within ±%1 Hz of %2 Hz, plus your TX messages")
              .arg(m_session.mode == Ft8::Mode::FT4 ? 45 : 25).arg(m_rxFocusHz)
        : "Messages to or from the selected callsign, plus your TX messages");
    m_next->setText(m_session.nextMessage.isEmpty() ? "Next · Select a station" : "Next · " + m_session.nextMessage);
    m_period->setText(m_session.even ? "Even / 1st" : "Odd / 2nd");
    if (!m_session.dxCall.isEmpty())
        m_exchange->setText(QString("Sent %1   Received %2%3")
                                .arg(m_session.sentReport.isEmpty() ? "—" : m_session.sentReport,
                                     m_session.receivedReport.isEmpty() ? "—" : m_session.receivedReport,
                                     m_session.complete ? "   · Complete" : ""));
    m_call->setText("Call");
    const bool canCall = !m_livePending && (m_practice || (m_connected && !m_radioTx));
    m_call->setEnabled(canCall && !m_session.dxCall.isEmpty() && !m_session.complete);
    m_cq->setEnabled(canCall);
    m_halt->setEnabled(m_session.armed || m_livePending);
    m_next->setEnabled(!m_livePending && !m_session.nextMessage.isEmpty());
    m_period->setEnabled(!m_livePending);
    m_mode->setEnabled(!m_radioTx && !m_livePending);
    m_band->setEnabled(!m_radioTx && !m_livePending);
    m_frequencyDisplay->setEnabled(!m_radioTx && !m_livePending);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const int period = Ft8::periodMs(m_session.mode);
    m_cycle->setRange(0, period);
    m_cycle->setValue(int(now % period));
    QString status = m_notice;
    if (m_session.armed) {
        qint64 nextSlot = Ft8Transmitter::nextSlot(now, m_session.mode, m_session.even) / period;
        if (nextSlot == m_lastTxSlot && !m_livePending) nextSlot += 2;
        status = QString("%1 in %2 s · %3")
                     .arg(m_practice ? "Practice TX" : "TX")
                     .arg(qMax<qint64>(0, nextSlot * period - now) / 1000.0, 0, 'f', 1)
                     .arg(m_session.nextMessage);
    } else if (status.isEmpty()) {
        if (m_session.complete)
            status = "QSO complete · ready to log";
        else if (m_practice)
            status = "Practice · select a station or call CQ";
        else if (!m_connected)
            status = "Connect to the K4, or open Options to practice.";
        else if (!m_receiving)
            status = "Receive paused";
        else if (m_radioTx)
            status = "K4 transmitting · receive paused";
        else if (m_lastAudioUtc > 0 && now - m_lastAudioUtc > 3000)
            status = "Waiting for K4 audio";
        else
            status = m_receiveStatus.isEmpty() ? "Listening · waiting for a full receive period" : m_receiveStatus;
    }
    if (m_livePending)
        status = QString("%1 · %2").arg(m_liveTransmitting ? "Transmitting" : "Preparing TX", m_liveMessage.message);
    m_status->setText(status);
}
void Ft8Screen::setRfPower(double watts) {
    m_rfPowerWatts = std::isfinite(watts) && watts >= 0.0 ? watts : -1.0;
    updatePowerUi();
}
void Ft8Screen::setTransmitProtection(const QString &text, bool fault) {
    m_transmitProtection = text;
    m_transmitProtectionFault = fault;
    if (fault) {
        (stopLiveTransmit(), m_session.halt());
        ++m_practiceGeneration;
    }
    refresh();
}
void Ft8Screen::updatePowerUi() {
    const double watts = m_practice ? m_practicePowerWatts : m_rfPowerWatts;
    const bool enabled = !m_radioTx && !m_livePending && (m_practice || (m_connected && watts >= 0.0));
    const QSignalBlocker blocker(m_powerSlider);
    if (!enabled)
        m_powerSlider->setSliderDown(false);
    m_powerSlider->setEnabled(enabled);
    if (m_powerSlider->isSliderDown())
        return; // Keep the operator's drag position through incoming K4 updates.
    m_powerSlider->setValue(watts >= 0.0 ? qBound(1, qRound(watts), 110) : 1);
    m_powerLabel->setText(watts < 0.0 || (!m_connected && !m_practice)
                             ? QString("— W")
                             : QString("%1 W").arg(watts, 0, 'f', watts == std::floor(watts) ? 0 : 1));
}
void Ft8Screen::setRadioState(bool connected, qint64 hz, const QString &radioMode, bool transmitting) {
    if (connected != m_connected || radioMode != m_radioMode || transmitting != m_radioTx) {
        const QSignalBlocker blocker(m_powerSlider);
        m_powerSlider->setSliderDown(false);
        if (connected != m_connected)
            m_rfPowerWatts = -1.0;
        m_pendingRfHz = 0;
        m_dialPreviewActive = false;
        if (connected != m_connected || !m_dialCommitRequired)
            m_dialTarget = m_lastToneTarget = DialTarget::RxAudio;
    } else if (hz == m_pendingRfHz)
        m_pendingRfHz = 0;
    const bool changed =
        (connected != m_connected || hz != m_radioFrequency || radioMode != m_radioMode);
    const bool txChanged = transmitting != m_radioTx;
    if (!m_practice && transmitting && !m_radioTx && !m_livePending)
        (stopLiveTransmit(), m_session.halt());
    m_connected = connected;
    m_radioTx = transmitting;
    m_radioMode = radioMode;
    m_radioFrequency = hz;
    if (!m_practice && changed) {
        (stopLiveTransmit(), m_session.clear());
        clearActivity();
        if (hz > 0)
            m_frequency = hz;
    }
    // Calibration and our own timed TX both pause capture. Resume when the
    // radio reports RX, even when frequency/mode and the QSO stay unchanged.
    if (!m_practice && (changed || txChanged))
        emit captureChanged();
    refresh();
}
void Ft8Screen::setReceiveStatus(const QString &text) {
    if (!m_practice) {
        m_receiveStatus = text;
        refresh();
    }
}
void Ft8Screen::setWaterfallAppearance(int palette, int range) {
    m_waterfall->setAppearance(palette, range);
}
void Ft8Screen::addSpectrum(const QVector<float> &db, double firstHz, double binHz) {
    if (!m_practice && m_receiving) {
        m_lastAudioUtc = QDateTime::currentMSecsSinceEpoch();
        m_waterfall->addSpectrum(db, firstHz, binHz);
    }
}
void Ft8Screen::clearActivity() {
    m_dialPreviewActive = false;
    m_dialCommitRequired = false;
    m_rxFocusHz = 0;
    m_activity->clear();
    m_pending.clear();
    m_recent.clear();
    m_seen.clear();
    m_unreadActivity = 0;
    updateNewActivity();
    m_waterfall->reset();
}
void Ft8Screen::addDecodes(const QVector<Ft8::Decode> &decodes) {
    QVector<Ft8::Decode> ready;
    for (const auto &d : decodes) {
        if (d.practice != m_practice || d.mode != m_session.mode)
            continue;
        if (m_livePending) {
            if (m_liveReplies.size() < 300) m_liveReplies.append(d);
        } else
            m_session.receive(d);
        if (m_interacting || m_activity->property("ft8Dragging").toBool()) {
            if (m_pending.size() < 300)
                m_pending.append(d);
        } else
            ready.append(d);
    }
    appendDecodes(ready);
    refresh();
}
void Ft8Screen::appendDecode(const Ft8::Decode &d, bool transmitted) {
    appendDecodes({d}, transmitted);
}
void Ft8Screen::appendDecodes(const QVector<Ft8::Decode> &decodes, bool transmitted) {
    if (decodes.isEmpty())
        return;
    bool atBottom = m_activity->verticalScrollBar()->value() >= m_activity->verticalScrollBar()->maximum() - 5;
    const int oldScroll = m_activity->verticalScrollBar()->value();
    QVector<QListWidgetItem *> added;
    m_updatingActivity = true;
    m_activity->setUpdatesEnabled(false);
    for (const auto &d : decodes) {
        const QString id = d.id() + (transmitted ? ":TX" : "");
        if (m_seen.contains(id))
            continue;
        m_seen.insert(id);
        auto *item = new QListWidgetItem;
        item->setData(Qt::UserRole, QVariant::fromValue(d));
        item->setData(Qt::UserRole + 1, transmitted);
        item->setData(Qt::UserRole + 2, id);
        auto parsed = Ft8::parseMessage(d.message);
        bool worked = logbook().worked(parsed.from, Ft8::bandFor(m_frequency), Ft8::modeName(d.mode));
        item->setData(Qt::UserRole + 3, worked);
        formatActivityItem(item);
        item->setBackground(Ft8ActivityStyle::background(d, m_session.myCall, transmitted, worked));
        item->setForeground(Qt::black);
        if (transmitted)
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        m_activity->addItem(item);
        added.append(item);
        if (!transmitted) {
            auto now = d.utc.toMSecsSinceEpoch();
            for (int i = m_recent.size() - 1; i >= 0; --i)
                if (now - m_recent[i].utc.toMSecsSinceEpoch() > 2 * Ft8::periodMs(d.mode) ||
                    Ft8::parseMessage(m_recent[i].message).from == parsed.from)
                    m_recent.removeAt(i);
            m_recent.append(d);
            if (m_recent.size() > 100)
                m_recent.removeFirst();
        }
    }
    filterActivity();
    if (!atBottom)
        for (auto *item : added)
            if (!item->isHidden())
                ++m_unreadActivity;
    // Retain more history while browsing, but bound memory during long sessions.
    const int historyLimit = atBottom ? 300 : 1000;
    int removedHeight = 0;
    while (m_activity->count() > historyLimit) {
        auto *old = m_activity->takeItem(0);
        if (!old->isHidden())
            removedHeight += old->sizeHint().height();
        m_seen.remove(old->data(Qt::UserRole + 2).toString());
        delete old;
    }
    if (!transmitted)
        m_waterfall->setDecodes(m_recent);
    m_activity->doItemsLayout();
    if (atBottom)
        m_activity->scrollToBottom();
    else
        m_activity->verticalScrollBar()->setValue(qMax(0, oldScroll - removedHeight));
    m_activity->setUpdatesEnabled(true);
    m_updatingActivity = false;
    updateNewActivity();
}
void Ft8Screen::filterActivity() {
    for (int i = 0; i < m_activity->count(); ++i) {
        auto *item = m_activity->item(i);
        auto d = item->data(Qt::UserRole).value<Ft8::Decode>();
        auto p = Ft8::parseMessage(d.message);
        bool tx = item->data(Qt::UserRole + 1).toBool();
        const bool inQso = m_rxFocusHz > 0
            ? qAbs(d.audioHz - m_rxFocusHz) <= (m_session.mode == Ft8::Mode::FT4 ? 45 : 25)
            : !m_session.dxCall.isEmpty() && (p.from == m_session.dxCall || p.to == m_session.dxCall);
        item->setHidden(m_filter == 1 ? (!p.cq || tx) : m_filter == 2 ? (!tx && !inQso) : false);
    }
}
void Ft8Screen::selectStation(const Ft8::Decode &d, bool call) {
    if (m_livePending) return;
    if (d.practice != m_practice)
        return;
    if (!m_session.select(d))
        return;
    if (m_rxFocusHz > 0 && qAbs(d.audioHz - m_rxFocusHz) <= (m_session.mode == Ft8::Mode::FT4 ? 45 : 25))
        m_session.rxHz = m_rxFocusHz;
    else
        m_rxFocusHz = 0;
    selectDialTarget(DialTarget::RxAudio);
    if (m_rxFocusHz > 0)
        m_dialCommitRequired = true;
    ++m_practiceGeneration;
    m_completionShown = false;
    m_notice.clear();
    filterActivity();
    if (call)
        startCall();
    refresh();
}
void Ft8Screen::startCall() {
    if (m_livePending) return;
    m_rx->setChecked(true);
    if (!m_session.arm())
        m_notice = "Set a valid callsign and grid in Options, then select a station.";
    else {
        m_notice.clear();
        m_lastTxSlot = -1;
        if (!m_practice) emit liveArmRequested();
        tick();
    }
    refresh();
}
void Ft8Screen::setFrequency(qint64 hz) {
    if (hz <= 0 || hz > 9999999999LL || m_radioTx)
        return;
    (stopLiveTransmit(), m_session.clear());
    ++m_practiceGeneration;
    clearActivity();
    m_notice.clear();
    if (m_practice || !m_connected)
        m_frequency = hz;
    if (!m_practice) {
        m_pendingRfHz = m_connected ? hz : 0;
        m_rfRequestUtc = QDateTime::currentMSecsSinceEpoch();
        emit frequencyRequested(hz);
    }
    emit captureChanged();
    savePreferences();
    if (m_practice)
        seedPractice();
    refresh();
}
void Ft8Screen::selectBand() {
    Sheet s(this, "Band · " + Ft8::modeName(m_session.mode));
    s.contents->addWidget(label("Common working frequencies. Custom entry remains available on every band.", s.body));
    for (const auto &b : Ft8::bands()) {
        qint64 hz = m_session.mode == Ft8::Mode::FT4 ? b.ft4Hz : b.ft8Hz;
        auto *choice = s.action(b.name + "   " + (hz ? mhz(hz) + " MHz" : "Custom frequency…"));
        connect(choice, &QPushButton::clicked, &s.dialog, [this, &s, hz] {
            if (s.dragging())
                return;
            s.dialog.accept();
            if (hz)
                setFrequency(hz);
            else
                QTimer::singleShot(0, this, &Ft8Screen::enterFrequency);
        });
    }
    auto *custom = s.action("Enter another frequency…");
    connect(custom, &QPushButton::clicked, &s.dialog, [this, &s] {
        if (s.dragging())
            return;
        s.dialog.accept();
        QTimer::singleShot(0, this, &Ft8Screen::enterFrequency);
    });
    s.dialog.exec();
}
void Ft8Screen::enterFrequency() {
    Sheet s(this, "Operating frequency");
    auto *input = new QLineEdit(mhz(m_frequency), s.body);
    input->setInputMethodHints(Qt::ImhFormattedNumbersOnly);
    input->setObjectName("ft8FrequencyInput");
    s.contents->addWidget(label("Frequency · MHz (14.074), grouped (14.074.000), or Hz (14074000)", s.body));
    s.contents->addWidget(input);
    auto *error = label("Presets are shortcuts. Any frequency accepted by the K4 can be entered.", s.body);
    s.contents->addWidget(error);
    auto *save = s.action("Use frequency");
    save->setObjectName("ft8UseFrequency");
    connect(save, &QPushButton::clicked, &s.dialog, [this, &s, input, error] {
        if (s.dragging())
            return;
        quint64 hz = 0;
        if (!FrequencyEntryParser::parse(input->text(), &hz) || hz == 0 || hz > 9999999999ULL) {
            error->setText("Enter a valid frequency, for example 14.074 or 14074000.");
            return;
        }
        setFrequency(qint64(hz));
        s.dialog.accept();
    });
    s.contents->addStretch();
    s.dialog.exec();
}
void Ft8Screen::changeMode() {
    if (m_livePending) return;
    selectDialTarget(DialTarget::RxAudio);
    m_pendingRfHz = 0;
    bool custom = true;
    auto band = Ft8::bandFor(m_frequency);
    for (const auto &b : Ft8::bands())
        if (m_frequency == (m_session.mode == Ft8::Mode::FT4 ? b.ft4Hz : b.ft8Hz))
            custom = false;
    (stopLiveTransmit(), m_session.clear());
    m_session.mode = m_session.mode == Ft8::Mode::FT8 ? Ft8::Mode::FT4 : Ft8::Mode::FT8;
    ++m_practiceGeneration;
    clearActivity();
    if (!custom)
        for (const auto &b : Ft8::bands())
            if (b.name == band) {
                qint64 hz = m_session.mode == Ft8::Mode::FT4 ? b.ft4Hz : b.ft8Hz;
                if (hz)
                    setFrequency(hz);
            }
    emit captureChanged();
    savePreferences();
    if (m_practice)
        seedPractice();
    refresh();
}
void Ft8Screen::options() {
    if (m_livePending) return;
    Sheet s(this, "FT8 / FT4 options");
    auto *audioSetup = s.action("Calibrate TX audio / protection status");
    audioSetup->setObjectName("ft8AudioSetup");
    audioSetup->setEnabled(!m_practice);
    connect(audioSetup, &QPushButton::clicked, &s.dialog, [&, this] {
        if (s.dragging()) return;
        s.dialog.accept();
        QTimer::singleShot(0, this, [this] { emit audioSetupRequested(); });
    });
    s.contents->addWidget(label("CTR2: assign FT8/FT4 RX to a short press and FT8/FT4 TX to its long press. "
                                "The normal tuning knob follows Dial RX/TX/RF. Tap a radio-frequency digit to tune RF; "
                                "hold that display for direct entry. Tap the RX/TX readout for dial choices.",
                                s.body));
    s.contents->addWidget(label("Waterfall: tap a decoded station for RX. Hold an open spot to set and hold TX. "
                                "Drag to pan; pinch or use + / − to zoom.",
                                s.body));
    s.contents->addWidget(label("Station colors: green CQ, red your callsign, yellow TX, gray worked, "
                                "white other activity. A blue outline marks your selection.",
                                s.body));
    auto field = [&s](const QString &title, const QString &value) {
        s.contents->addWidget(label(title, s.body));
        auto *edit = new QLineEdit(value, s.body);
        s.contents->addWidget(edit);
        return edit;
    };
    auto *call = field("My callsign", m_session.myCall), *grid = field("My grid", m_session.myGrid);
    call->setObjectName("ft8MyCall");
    grid->setObjectName("ft8MyGrid");
    call->setInputMethodHints(Qt::ImhNoPredictiveText | Qt::ImhUppercaseOnly);
    grid->setInputMethodHints(Qt::ImhNoPredictiveText | Qt::ImhUppercaseOnly);
    auto *rx = field("RX audio frequency · Hz", QString::number(m_session.rxHz)),
         *tx = field("TX audio frequency · Hz", QString::number(m_session.txHz));
    s.contents->addWidget(label("Signal reports are measured automatically in dB relative to 2500 Hz noise bandwidth.", s.body));
    auto *retries = field("Maximum repeated transmissions", QString::number(m_session.maxRetries));
    auto *direction = field("CQ destination · optional, e.g. DX or JA", m_cqDirection);
    auto check = [&s](const QString &title, bool on) {
        auto *c = new QCheckBox(title, s.body);
        c->setChecked(on);
        s.contents->addWidget(c);
        return c;
    };
    auto *hold = check("Hold TX frequency when selecting a station", m_session.holdTx);
    auto *first = check("CQ: answer the first responder", m_session.callFirst);
    auto *rr73 = check("Finish with RR73", m_session.useRr73);
    auto *autoLog = check("Save completed QSOs automatically", m_autoLog);
    auto *practice = check("Practice · simulated stations, no RF", m_practice);
    practice->setObjectName("ft8PracticeOption");
    auto *help =
        label("Call or CQ arms live FT8/FT4 transmission at the next selected period. Halt TX stops it. "
              "Calibrate TX audio before going on air. Measured signal reports populate the exchange automatically. "
              "Practice uses simulated stations and a separate log, with no RF.",
              s.body);
    s.contents->addWidget(help);
    auto *save = button("Save options", s.dialog.contentWidget());
    s.layout->insertWidget(s.layout->count() - 1, save);
    save->setObjectName("ft8SaveOptions");
    connect(save, &QPushButton::clicked, &s.dialog, [&, this] {
        const auto newCall = call->text().trimmed().toUpper(), newGrid = grid->text().trimmed().toUpper();
        bool a, b, c;
        int newRx = rx->text().toInt(&a), newTx = tx->text().toInt(&b), limit = retries->text().toInt(&c);
        if ((!newCall.isEmpty() && !Ft8::validCall(newCall)) || (!newGrid.isEmpty() && !Ft8::validGrid(newGrid)) ||
            !a || !b || !c || newRx < 100 || newRx > 3200 || newTx < 100 || newTx > 3200 || limit < 1 || limit > 20) {
            help->setText("Check callsign/grid, audio frequencies (100–3200 Hz), and retries (1–20).");
            s.scroll->ensureWidgetVisible(help);
            return;
        }
        auto dir = direction->text().trimmed().toUpper();
        if (!dir.isEmpty() && !QRegularExpression("^[A-Z]{1,4}$").match(dir).hasMatch()) {
            help->setText("CQ destination must contain 1–4 letters.");
            return;
        }
        const bool leavingPractice = m_practice && !practice->isChecked();
        const bool enteringPractice = !m_practice && practice->isChecked();
        const bool identityChanged = newCall != m_session.myCall || newGrid != m_session.myGrid;
        (stopLiveTransmit(), m_session.halt());
        ++m_practiceGeneration;
        if (!leavingPractice) {
            if (identityChanged)
                (stopLiveTransmit(), m_session.clear());
            m_session.myCall = newCall;
            m_session.myGrid = newGrid;
        }
        if (enteringPractice || leavingPractice)
            setPractice(practice->isChecked());
        m_session.rxHz = newRx;
        m_session.txHz = newTx;
        m_session.holdTx = hold->isChecked();
        m_session.callFirst = first->isChecked();
        m_session.useRr73 = rr73->isChecked();
        m_session.maxRetries = limit;
        m_autoLog = autoLog->isChecked();
        m_cqDirection = dir;
        savePreferences();
        m_notice.clear();
        refresh();
        s.dialog.accept();
    });
    s.dialog.exec();
}
void Ft8Screen::chooseMessage() {
    if (m_livePending) return;
    Sheet s(this, "Next message");
    QString prefix = m_session.dxCall + " " + m_session.myCall + " ";
    QStringList messages;
    if (!m_session.dxCall.isEmpty()) {
        messages << prefix + m_session.myGrid.left(4);
        if (!m_session.sentReport.isEmpty())
            messages << prefix + m_session.sentReport << prefix + "R" + m_session.sentReport;
        messages << prefix + "RRR" << prefix + "RR73" << prefix + "73";
    }
    messages << "CQ " + m_session.myCall + " " + m_session.myGrid.left(4);
    for (const auto &message : messages) {
        auto *b = s.action(message);
        connect(b, &QPushButton::clicked, &s.dialog, [&, message] {
            if (s.dragging())
                return;
            m_session.nextMessage = message;
            m_session.retries = 0;
            refresh();
            s.dialog.accept();
        });
    }
    s.contents->addStretch();
    s.dialog.exec();
}
void Ft8Screen::chooseStation(const QVector<Ft8::Decode> &decodes) {
    Sheet s(this, "Stations at this frequency");
    for (const auto &d : decodes) {
        auto *b = s.action(d.message);
        connect(b, &QPushButton::clicked, &s.dialog, [&, d] {
            if (s.dragging())
                return;
            selectStation(d, false);
            s.dialog.accept();
        });
    }
    s.contents->addStretch();
    s.dialog.exec();
}
void Ft8Screen::setPractice(bool enabled) {
    const QSignalBlocker blocker(m_powerSlider);
    m_powerSlider->setSliderDown(false);
    if (enabled == m_practice)
        return;
    m_pendingRfHz = 0;
    m_dialTarget = DialTarget::RxAudio;
    if (enabled) {
        m_stationCall = m_session.myCall;
        m_stationGrid = m_session.myGrid;
        savePreferences();
    }
    (stopLiveTransmit(), m_session.clear());
    m_practice = enabled;
    ++m_practiceGeneration;
    clearActivity();
    m_completionShown = false;
    m_notice.clear();
    if (enabled) {
        if (m_session.myCall.isEmpty())
            m_session.myCall = "N0CALL";
        if (m_session.myGrid.isEmpty())
            m_session.myGrid = "EM00";
        seedPractice();
    } else {
        m_session.myCall = m_stationCall;
        m_session.myGrid = m_stationGrid;
        if (m_radioFrequency > 0)
            m_frequency = m_radioFrequency;
    }
    emit captureChanged();
    refresh();
}
void Ft8Screen::seedPractice() {
    const auto now = QDateTime::currentDateTimeUtc();
    QVector<Ft8::Decode> calls;
    const QStringList messages{"CQ K1ABC FN42", "CQ W9XYZ EN37", "CQ DX G4ABC IO91", "CQ W7ABC CN87"};
    for (int i = 0; i < messages.size(); ++i) {
        Ft8::Decode d;
        d.utc = now;
        d.mode = m_session.mode;
        d.message = messages[i];
        d.audioHz = 750 + i * 560;
        d.snr = -8 - i * 4;
        d.practice = true;
        calls << d;
    }
    addDecodes(calls);
    // Practice uses actual encoded tone sequences for realistic FT8/FT4 traces.
    // These synthetic spectrum rows never enter the live receiver or radio.
    QVector<QByteArray> tones;
    const bool ft4 = m_session.mode == Ft8::Mode::FT4;
    for (const auto &d : calls) {
        ftx_message_t message{};
        QByteArray encoded(105, 0);
        if (ftx_message_encode(&message, nullptr, d.message.toLatin1().constData()) == FTX_MESSAGE_RC_OK) {
            if (ft4)
                ft4_encode(message.payload, reinterpret_cast<uint8_t *>(encoded.data()));
            else
                ft8_encode(message.payload, reinterpret_cast<uint8_t *>(encoded.data()));
        }
        tones << encoded;
    }
    QRandomGenerator random(0xf8f4);
    const double symbolSeconds = ft4 ? 0.048 : 0.160;
    for (int y = 0; y < 256; ++y) {
        QVector<float> bins(1025, -101);
        for (int x = 0; x < bins.size(); ++x) {
            bins[x] += float(random.generateDouble() * 12);
            for (int c = 0; c < calls.size(); ++c) {
                const double time = y * 0.16 + (c % 2) * (ft4 ? 7.5 : 15);
                const double within = std::fmod(time, ft4 ? 15.0 : 30.0) - (ft4 ? .2 : .5);
                const int symbol = int(within / symbolSeconds);
                if (within < 0 || symbol < 0 || symbol >= (ft4 ? 105 : 79))
                    continue;
                const double toneHz = calls[c].audioHz + quint8(tones[c][symbol]) / symbolSeconds;
                const double delta = (100 + x * 3.125 - toneHz) / (ft4 ? 12 : 4.2);
                bins[x] += float((47 - c * 5) * std::exp(-0.5 * delta * delta));
            }
        }
        m_waterfall->addSpectrum(bins, 100, 3.125);
    }
}
void Ft8Screen::tick() {
    if (!isVisible())
        return;
    if (!m_interacting && !m_activity->property("ft8Dragging").toBool() && !m_pending.isEmpty()) {
        const auto pending = m_pending;
        m_pending.clear();
        appendDecodes(pending);
    }
    const auto utc = QDateTime::currentDateTimeUtc();
    const auto now = utc.toMSecsSinceEpoch();
    const auto slot = Ft8::slot(now, m_session.mode);
    if (!m_practice && m_receiving && m_connected && !m_radioTx && m_session.armed && !m_livePending && dialAvailable()) {
        const auto period = Ft8::periodMs(m_session.mode);
        const auto next = Ft8Transmitter::nextSlot(now, m_session.mode, m_session.even) / period;
        const auto until = next * period - now;
        if (next != m_lastTxSlot && until <= 600) {
            m_lastTxSlot = next;
            m_dialPreviewActive = false;
            m_livePending = true;
            m_liveTransmitting = false;
            m_liveMessage = {};
            m_liveMessage.utc = QDateTime::fromMSecsSinceEpoch(next * period, QTimeZone::UTC);
            m_liveMessage.message = m_session.nextMessage;
            m_liveMessage.mode = m_session.mode;
            m_liveMessage.audioHz = m_session.txHz;
            emit liveTransmitRequested(m_liveMessage.message, int(m_session.mode), m_session.txHz, next * period);
        }
    }
    if (m_practice && m_receiving && m_session.armed && slot != m_lastTxSlot && (slot % 2 == 0) == m_session.even &&
        now % Ft8::periodMs(m_session.mode) < 500) {
        m_lastTxSlot = slot;
        practiceTransmit(utc);
    }
    if (m_session.complete && !m_completionShown) {
        m_completionShown = true;
        if (m_autoLog)
            QTimer::singleShot(0, this, [this] { logContact(); });
        else
            m_notice = "QSO complete · tap Log QSO to review and save";
    }
    if (m_practice && !m_session.armed && m_session.dxCall.isEmpty() && (!m_recent.isEmpty()) &&
        m_recent.first().utc.msecsTo(utc) > Ft8::periodMs(m_session.mode))
        seedPractice();
    refresh();
}
void Ft8Screen::stopLiveTransmit() {
    if (m_livePending) emit liveStopRequested();
}
void Ft8Screen::liveTransmitting() {
    m_liveTransmitting = true;
    refresh();
}
void Ft8Screen::finishLiveTransmit(bool success, const QString &reason) {
    if (!m_livePending) return;
    m_livePending = m_liveTransmitting = false;
    if (success && m_session.armed && m_session.nextMessage == m_liveMessage.message) {
        appendDecode(m_liveMessage, true);
        m_session.sent(m_liveMessage.utc);
        for (const auto &reply : m_liveReplies) m_session.receive(reply);
    } else {
        (stopLiveTransmit(), m_session.halt());
    }
    m_liveReplies.clear();
    m_notice = reason;
    refresh();
}
void Ft8Screen::practiceTransmit(const QDateTime &utc) {
    const QString sent = m_session.nextMessage;
    Ft8::Decode tx;
    tx.utc = utc;
    tx.mode = m_session.mode;
    tx.message = sent;
    tx.audioHz = m_session.txHz;
    tx.practice = true;
    appendDecode(tx, true);
    m_session.sent(utc);
    if (!m_session.armed)
        return;
    const auto generation = m_practiceGeneration;
    QTimer::singleShot(Ft8::periodMs(m_session.mode), this, [this, generation, sent] {
        if (!m_practice || generation != m_practiceGeneration || !m_session.armed || !m_receiving)
            return;
        Ft8::Decode reply;
        reply.utc = QDateTime::currentDateTimeUtc();
        reply.mode = m_session.mode;
        reply.practice = true;
        reply.snr = -12;
        reply.audioHz = m_session.rxHz;
        QString partner = m_session.dxCall.isEmpty() ? "K1ABC" : m_session.dxCall;
        QString prefix = m_session.myCall + " " + partner + " ";
        if (sent.startsWith("CQ "))
            reply.message = prefix + "FN42";
        else if (sent.endsWith(" RRR"))
            reply.message = prefix + "73";
        else if (sent.contains(" R-") || sent.contains(" R+"))
            reply.message = prefix + "RR73";
        else if (Ft8::validGrid(sent.section(' ', -1)))
            reply.message = prefix + "-09";
        else
            reply.message = prefix + "R-09";
        addDecodes({reply});
    });
}
Ft8Logbook &Ft8Screen::logbook() {
    return m_practice ? m_practiceLog : m_logbook;
}
void Ft8Screen::logContact(int editIndex) {
    QString loadError;
    m_logReady = logbook().load(&loadError);
    if (!m_practice && !m_logReady) {
        showInWindowMessage(this, "Logbook", "The existing logbook needs recovery before new contacts can be saved.");
        return;
    }
    AdifRecord record;
    if (editIndex >= 0 && editIndex < logbook().records().size())
        record = logbook().records()[editIndex];
    else {
        if (m_session.dxCall.isEmpty()) {
            m_notice = "Select a station before logging a contact.";
            refresh();
            return;
        }
        const auto start = m_session.started.isValid() ? m_session.started : QDateTime::currentDateTimeUtc();
        const auto end = m_session.ended.isValid() ? m_session.ended : QDateTime::currentDateTimeUtc();
        record = {{"CALL", m_session.dxCall},
                  {"STATION_CALLSIGN", m_session.myCall},
                  {"GRIDSQUARE", m_session.dxGrid},
                  {"MY_GRIDSQUARE", m_session.myGrid},
                  {"MODE", Ft8::modeName(m_session.mode)},
                  {"QSO_DATE", start.toUTC().toString("yyyyMMdd")},
                  {"TIME_ON", start.toUTC().toString("HHmmss")},
                  {"QSO_DATE_OFF", end.toUTC().toString("yyyyMMdd")},
                  {"TIME_OFF", end.toUTC().toString("HHmmss")},
                  {"FREQ", mhz(m_frequency + m_session.txHz)},
                  {"BAND", Ft8::bandFor(m_frequency)},
                  {"RST_SENT", m_session.sentReport},
                  {"RST_RCVD", m_session.receivedReport}};
        if (m_practice)
            record["APP_QK4_PRACTICE"] = "Y";
    }
    if (m_autoLog && m_session.complete && editIndex < 0) {
        QString error;
        if (logbook().append(record, &error))
            m_notice = "Contact saved · open Logbook to edit";
        else
            m_notice = error;
        refresh();
        return;
    }
    if (LogbookUi::edit(this, logbook(), record, editIndex)) {
        m_notice = "Contact saved · open Logbook to edit";
        refresh();
    }
}
void Ft8Screen::showLogbook() {
    auto defaults = LogbookUi::contact({}, Ft8::modeName(m_session.mode),
                                       m_frequency + m_session.txHz, m_session.myCall);
    defaults["MY_GRIDSQUARE"] = m_session.myGrid;
    LogbookUi::show(this, logbook(), defaults, m_practice);
    refresh();
}
void Ft8Screen::suspend() {
    const QSignalBlocker blocker(m_powerSlider);
    m_powerSlider->setSliderDown(false);
    updatePowerUi();
    (stopLiveTransmit(), m_session.halt());
    ++m_practiceGeneration;
    m_pendingRfHz = 0;
    selectDialTarget(DialTarget::RxAudio);
    savePreferences();
}
void Ft8Screen::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    const bool compact = height() < 640;
    layout()->setSpacing(compact ? 2 : 4);
    layout()->setContentsMargins(10, compact ? 4 : 6, 10, compact ? 4 : 6);
    m_waterfall->setFixedHeight(compact ? 86 : 146);
    m_activity->setMinimumHeight(compact ? 48 : 80);
}
void Ft8Screen::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Back || e->key() == Qt::Key_Escape) {
        suspend();
        emit closeRequested();
        e->accept();
        return;
    }
    QWidget::keyPressEvent(e);
}
bool Ft8Screen::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_activity->viewport()) {
        if (event->type() == QEvent::Resize)
            updateNewActivity();
        if (event->type() == QEvent::MouseButtonPress) {
            m_interacting = true;
            m_ignoreNextClick = false;
            m_pressPosition = static_cast<QMouseEvent *>(event)->pos();
        } else if (event->type() == QEvent::MouseMove && m_interacting &&
                   (static_cast<QMouseEvent *>(event)->pos() - m_pressPosition).manhattanLength() > 10)
            m_ignoreNextClick = true;
        else if (event->type() == QEvent::MouseButtonRelease) {
            m_interacting = false;
            QTimer::singleShot(120, this, [this] { m_ignoreNextClick = false; });
        } else if (event->type() == QEvent::TouchCancel) {
            m_interacting = false;
            m_ignoreNextClick = true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
