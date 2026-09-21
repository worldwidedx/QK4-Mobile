#include "adjustoverlay.h"
#include "k4styles.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSlider>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

AdjustOverlay::AdjustOverlay(QWidget *parent) : QWidget(parent, Qt::Popup) {
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedWidth(240);

    auto *outer = new QVBoxLayout(this);
    // Leave room for the indicator bar on the left.
    outer->setContentsMargins(IndicatorBarWidth + 12, 10, 12, 12);
    outer->setSpacing(8);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setStyleSheet(
        QString("color: %1; font-size: 12px; font-weight: bold;").arg(K4Styles::Colors::TextWhite));
    outer->addWidget(m_titleLabel);

    m_valueLabel = new QLabel(this);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    m_valueLabel->setStyleSheet(
        QString("color: %1; font-size: 22px; font-weight: bold;").arg(K4Styles::Colors::TextWhite));
    outer->addWidget(m_valueLabel);

    auto *row = new QHBoxLayout();
    row->setSpacing(10);

    const QString stepBtnStyle =
        QString("QPushButton { color: %1; background: %2; border: 1px solid %3; border-radius: 6px; "
                "font-size: 22px; font-weight: bold; } QPushButton:pressed { background: %3; }")
            .arg(K4Styles::Colors::TextWhite, K4Styles::Colors::DarkBackground, K4Styles::Colors::BorderNormal);

    m_minusBtn = new QPushButton(QStringLiteral("−"), this); // minus sign
    m_minusBtn->setFixedSize(44, 44);
    m_minusBtn->setStyleSheet(stepBtnStyle);
    row->addWidget(m_minusBtn);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setMinimumHeight(40);
    // A raw QSlider treats a touch that lands on the groove as a page-step and
    // does not track the drag. Map x->value directly instead (the popup is not
    // inside a scroll area, so there is no scroll-vs-adjust ambiguity).
    m_slider->installEventFilter(this);
    row->addWidget(m_slider, 1);

    m_plusBtn = new QPushButton(QStringLiteral("+"), this);
    m_plusBtn->setFixedSize(44, 44);
    m_plusBtn->setStyleSheet(stepBtnStyle);
    row->addWidget(m_plusBtn);

    outer->addLayout(row);

    connect(m_minusBtn, &QPushButton::clicked, this, [this]() {
        m_slider->setValue(m_slider->value() - m_slider->singleStep());
        pokeActivity();
    });
    connect(m_plusBtn, &QPushButton::clicked, this, [this]() {
        m_slider->setValue(m_slider->value() + m_slider->singleStep());
        pokeActivity();
    });
    connect(m_slider, &QSlider::valueChanged, this, [this](int value) {
        m_valueLabel->setText(formatValue(value));
        pokeActivity();
    });

    m_inactivityTimer = new QTimer(this);
    m_inactivityTimer->setSingleShot(true);
    m_inactivityTimer->setInterval(InactivityMs);
    connect(m_inactivityTimer, &QTimer::timeout, this, &QWidget::hide);
}

void AdjustOverlay::configure(const QString &title, DualControlButton::Context context) {
    m_context = context;
    m_titleLabel->setText(title);
    m_slider->setStyleSheet(
        K4Styles::sliderHorizontal(K4Styles::Colors::DarkBackground, barColor().name()));
    update();
}

void AdjustOverlay::setValueText(const QString &text) {
    m_valueLabel->setText(text);
}

void AdjustOverlay::setValueFormatter(std::function<QString(int)> formatter) {
    m_formatter = std::move(formatter);
    if (m_slider)
        m_valueLabel->setText(formatValue(m_slider->value()));
}

QString AdjustOverlay::formatValue(int value) const {
    return m_formatter ? m_formatter(value) : QString::number(value);
}

void AdjustOverlay::pokeActivity() {
    if (m_inactivityTimer)
        m_inactivityTimer->start();
}

void AdjustOverlay::showOver(QWidget *anchor) {
    adjustSize();
    QPoint pos;
    if (anchor) {
        // Sit just to the right of the tile, vertically centred on it.
        const QPoint tl = anchor->mapToGlobal(QPoint(anchor->width(), 0));
        pos = QPoint(tl.x() + 8, tl.y() + anchor->height() / 2 - height() / 2);
    } else {
        pos = QCursor::pos();
    }
    // Keep on screen.
    if (QScreen *screen = QGuiApplication::screenAt(pos) ? QGuiApplication::screenAt(pos)
                                                         : QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        int x = qBound(avail.left() + 4, pos.x(), avail.right() - width() - 4);
        int y = qBound(avail.top() + 4, pos.y(), avail.bottom() - height() - 4);
        pos = QPoint(x, y);
    }
    move(pos);
    show();
    raise();
    pokeActivity();
}

QColor AdjustOverlay::barColor() const {
    switch (m_context) {
    case DualControlButton::MainRx:
        return QColor(K4Styles::Colors::VfoACyan);
    case DualControlButton::SubRx:
        return QColor(K4Styles::Colors::VfoBGreen);
    case DualControlButton::Global:
    default:
        return QColor(K4Styles::Colors::AccentAmber);
    }
}

bool AdjustOverlay::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_slider) {
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->buttons() & Qt::LeftButton || event->type() == QEvent::MouseButtonPress) {
                setSliderFromX(me->pos().x());
                pokeActivity();
                return true; // consume: we position absolutely, not by page-step
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void AdjustOverlay::setSliderFromX(int xPosition) {
    if (!m_slider)
        return;
    const int handleWidth = qMax(12, m_slider->height() / 2);
    const int span = qMax(1, m_slider->width() - handleWidth);
    const int position = qBound(0, xPosition - handleWidth / 2, span);
    m_slider->setValue(QStyle::sliderValueFromPosition(m_slider->minimum(), m_slider->maximum(), position, span,
                                                       m_slider->invertedAppearance()));
}

void AdjustOverlay::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRect r = rect();

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(K4Styles::Colors::DarkBackground));
    painter.drawRoundedRect(r, CornerRadius, CornerRadius);

    QRect barRect(0, 0, IndicatorBarWidth, r.height());
    painter.setBrush(barColor());
    painter.drawRoundedRect(barRect, CornerRadius / 2, CornerRadius / 2);

    painter.setPen(QPen(QColor(K4Styles::Colors::BorderNormal), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(r.adjusted(0, 0, -1, -1), CornerRadius, CornerRadius);
}
