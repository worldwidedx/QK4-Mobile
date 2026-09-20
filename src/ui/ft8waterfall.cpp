#include "ft8waterfall.h"
#include "dsp/panadapter_rhi.h"
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QGestureEvent>
#include <QPinchGesture>
#include <QTimer>
#include <QTouchEvent>
#include <cmath>

// Only RX/TX markers are an overlay. Spectrum, history, palette
// and GPU shaders are the same PanadapterRhiWidget used by the radio console.
class Ft8WaterfallOverlay : public QWidget {
public:
    explicit Ft8WaterfallOverlay(Ft8Waterfall *owner) : QWidget(owner), m_owner(owner) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const int boundary = m_owner->spectrumHeight();
        // Standard RX green and TX red markers; their audio offsets are independent.
        for (int i = 0; i < 2; ++i) {
            const int x = m_owner->xAt(i ? m_owner->m_tx : m_owner->m_rx);
            if (x < 0 || x > width())
                continue;
            const QColor color(i ? "#ff5050" : "#66ff66");
            p.setPen(QPen(color, 1));
            p.drawLine(x, boundary + 9, x, height());
            p.setPen(QPen(color, 2));
            p.drawLine(x, boundary - 11, x, boundary - 3);
            p.drawLine(x, boundary - 3, x + 8, boundary - 3);
            p.drawLine(x + 8, boundary - 11, x + 8, boundary - 3);
        }
        if (m_owner->m_previewHz > 0) {
            const int x = m_owner->xAt(m_owner->m_previewHz);
            const QColor color(m_owner->m_previewTx ? "#ff5050" : "#66ff66");
            p.setPen(QPen(color, 2, Qt::DashLine));
            p.drawLine(x, 0, x, height());
            p.drawRect(QRect(x - 5, boundary - 14, 10, 12));
        }
    }

private:
    Ft8Waterfall *m_owner;
};

Ft8Waterfall::Ft8Waterfall(QWidget *parent) : QWidget(parent) {
    setObjectName("ft8Waterfall");
    // Return the former 24-pixel callsign-label band to the traffic list.
    // Keep the waterfall history height; shrink the spectrum above it.
    setMinimumHeight(136);
    setMaximumHeight(216);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setAccessibleName(
        "FT8 audio spectrum and waterfall. Tap a station to receive. Hold to set TX. Pinch to zoom; drag to pan.");
    m_renderer = new PanadapterRhiWidget(this);
    m_renderer->setObjectName("ft8Qk4Panadapter");
    m_renderer->setMinimumSize(0, 0);
    m_renderer->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_renderer->setAudioView(m_low, m_span);
    m_overlay = new Ft8WaterfallOverlay(this);
    m_overlay->setObjectName("ft8WaterfallMarkers");
    m_holdTimer = new QTimer(this);
    m_holdTimer->setSingleShot(true);
    m_holdTimer->setInterval(550);
    connect(m_holdTimer, &QTimer::timeout, this, [this] {
        if (!m_pressed || m_drag || m_pinching || !isVisible())
            return;
        m_held = true;
        emit txFrequencySelected(qBound(100, qRound(frequencyAt(m_press.x())), 3200));
    });
    grabGesture(Qt::PinchGesture);
}
void Ft8Waterfall::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    m_renderer->setGeometry(rect());
    m_renderer->setSpectrumRatio(float(spectrumHeight()) / qMax(1, height()));
    m_overlay->setGeometry(rect());
    m_overlay->raise();
}
void Ft8Waterfall::reset() {
    cancelPress();
    m_previewHz = 0;
    m_decodes.clear();
    m_renderer->clear();
    m_renderer->setAudioView(m_low, m_span);
    m_overlay->update();
}
void Ft8Waterfall::addSpectrum(const QVector<float> &db, double firstHz, double binHz) {
    m_renderer->updateAudioSpectrum(db, firstHz, binHz);
}
void Ft8Waterfall::setAppearance(int palette, int range) {
    m_renderer->setWaterfallColor(palette);
    m_renderer->setWaterfallColorRange(range);
}
void Ft8Waterfall::setDecodes(const QVector<Ft8::Decode> &decodes) {
    m_decodes = decodes;
    m_overlay->update();
}
void Ft8Waterfall::setMarkers(int rx, int tx) {
    if (m_rx == rx && m_tx == tx)
        return;
    m_rx = rx;
    m_tx = tx;
    m_overlay->update();
}
void Ft8Waterfall::setPreview(int hz, bool tx) {
    if (m_previewHz == hz && m_previewTx == tx)
        return;
    m_previewHz = hz;
    m_previewTx = tx;
    m_overlay->update();
}
void Ft8Waterfall::clamp() {
    m_span = qBound(200.0, m_span, 3000.0);
    m_low = qBound(0.0, m_low, 3300.0 - m_span);
}
void Ft8Waterfall::updateView() {
    m_renderer->setAudioView(m_low, m_span);
    m_overlay->update();
    emit viewChanged();
}
void Ft8Waterfall::zoom(double factor, double anchor) {
    if (!std::isfinite(factor) || !std::isfinite(anchor) || factor <= 0)
        return;
    anchor = qBound(0.0, anchor, 1.0);
    const double a = m_low + anchor * m_span;
    m_span = qBound(200.0, m_span / factor, 3000.0);
    m_low = a - anchor * m_span;
    clamp();
    updateView();
}
void Ft8Waterfall::fit() {
    m_low = 0;
    m_span = 3000;
    updateView();
}
double Ft8Waterfall::frequencyAt(int x) const {
    return m_low + qBound(0.0, double(x) / qMax(1, width()), 1.0) * m_span;
}
int Ft8Waterfall::xAt(double hz) const {
    return qRound((hz - m_low) / m_span * width());
}
void Ft8Waterfall::cancelPress() {
    m_holdTimer->stop();
    m_pressed = false;
    m_drag = true;
}
void Ft8Waterfall::mousePressEvent(QMouseEvent *e) {
    if (e->button() != Qt::LeftButton)
        return;
    m_press = e->pos();
    m_pressLow = m_low;
    m_pressed = true;
    m_held = m_drag = m_double = false;
    m_holdTimer->start();
}
void Ft8Waterfall::mouseMoveEvent(QMouseEvent *e) {
    if (!m_pressed || !(e->buttons() & Qt::LeftButton) || m_pinching || m_held)
        return;
    if ((e->pos() - m_press).manhattanLength() > 10) {
        m_drag = true;
        m_holdTimer->stop();
    }
    if (m_drag) {
        m_low = m_pressLow - double(e->pos().x() - m_press.x()) / qMax(1, width()) * m_span;
        clamp();
        updateView();
    }
}
void Ft8Waterfall::mouseReleaseEvent(QMouseEvent *e) {
    m_holdTimer->stop();
    const bool tap = m_pressed && !m_held && !m_drag && !m_double && !m_pinching;
    m_pressed = false;
    if (tap && rect().contains(e->pos()))
        choose(e->pos(), false);
}
void Ft8Waterfall::mouseDoubleClickEvent(QMouseEvent *e) {
    m_holdTimer->stop();
    m_double = true;
    if (!m_held && !m_drag && !m_pinching)
        choose(e->pos(), true);
}
void Ft8Waterfall::choose(const QPoint &point, bool call) {
    QVector<Ft8::Decode> hits;
    const double hz = frequencyAt(point.x());
    for (const auto &d : m_decodes) {
        const auto message = Ft8::parseMessage(d.message);
        if (!message.valid)
            continue;
        const double bandwidth = d.mode == Ft8::Mode::FT4 ? 90 : 50;
        if (hz >= d.audioHz - 8 && hz <= d.audioHz + bandwidth + 8)
            hits << d;
    }
    if (hits.size() == 1)
        emit stationSelected(hits[0], call);
    else if (hits.size() > 1)
        emit ambiguousStations(hits);
    else
        emit frequencySelected(qBound(100, qRound(hz), 3200));
}
bool Ft8Waterfall::event(QEvent *event) {
    if ((event->type() == QEvent::TouchBegin || event->type() == QEvent::TouchUpdate) &&
        static_cast<QTouchEvent *>(event)->points().size() > 1)
        cancelPress();
    if (event->type() == QEvent::Hide || event->type() == QEvent::WindowDeactivate ||
        event->type() == QEvent::TouchCancel || event->type() == QEvent::UngrabMouse)
        cancelPress();
    if (event->type() == QEvent::Gesture) {
        auto *ge = static_cast<QGestureEvent *>(event);
        if (auto *pinch = static_cast<QPinchGesture *>(ge->gesture(Qt::PinchGesture))) {
            m_pinching = true;
            cancelPress();
            if (pinch->changeFlags() & QPinchGesture::ScaleFactorChanged)
                zoom(pinch->scaleFactor() / qMax(0.01, pinch->lastScaleFactor()),
                     double(mapFromGlobal(pinch->centerPoint().toPoint()).x()) / qMax(1, width()));
            if (pinch->state() == Qt::GestureFinished || pinch->state() == Qt::GestureCanceled)
                m_pinching = false;
            ge->accept(pinch);
            return true;
        }
    }
    return QWidget::event(event);
}
