#include "iambickeyer.h"

#ifdef QK4_TIMESTAMPED_ANDROID_KEYER
#include <QThread>
#include <algorithm>
#include <chrono>

namespace {
constexpr unsigned RouterSource = 1u;
constexpr unsigned AndroidMidiSource = 2u;
constexpr unsigned Ctr2MidiSource = 4u;
constexpr qint64 NanosecondsPerMillisecond = 1000000;
}

qint64 IambicKeyer::nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

IambicKeyer::IambicKeyer(QObject *parent) : QObject(parent) {
    m_elementTimer = new QTimer(this);
    m_elementTimer->setSingleShot(true);
    m_elementTimer->setTimerType(Qt::PreciseTimer);
    connect(m_elementTimer, &QTimer::timeout, this, &IambicKeyer::onTimerFired);
}

void IambicKeyer::setEnabled(bool enabled) {
    m_enabled = enabled;
    if (!enabled)
        stop();
}

void IambicKeyer::setMode(Mode mode) { m_mode = mode; }
void IambicKeyer::setReversed(bool reversed) { m_reversed = reversed; }
void IambicKeyer::setSpeed(int wpm) {
    if (wpm > 0)
        m_ditMs = 1200 / wpm;
}

void IambicKeyer::setMidiTouchLookahead(int milliseconds) {
    m_midiTouchLookaheadMs = std::clamp(milliseconds, 0, 100);
    if (m_state == Idle)
        handlePaddleChange();
}

void IambicKeyer::setDitPaddle(bool pressed) {
    setPaddle(true, RouterSource, pressed, nowNs());
}

void IambicKeyer::setDahPaddle(bool pressed) {
    setPaddle(false, RouterSource, pressed, nowNs());
}

void IambicKeyer::setMidiDitPaddle(bool pressed, qint64 eventTimeNs) {
    setPaddle(true, AndroidMidiSource, pressed, eventTimeNs);
}

void IambicKeyer::setMidiDahPaddle(bool pressed, qint64 eventTimeNs) {
    setPaddle(false, AndroidMidiSource, pressed, eventTimeNs);
}

void IambicKeyer::setCtr2DitPaddle(bool pressed, qint64 eventTimeNs) {
    setPaddle(true, Ctr2MidiSource, pressed, eventTimeNs);
}

void IambicKeyer::setCtr2DahPaddle(bool pressed, qint64 eventTimeNs) {
    setPaddle(false, Ctr2MidiSource, pressed, eventTimeNs);
}

void IambicKeyer::setPaddle(bool dit, unsigned sourceBit, bool pressed,
                            qint64 eventTimeNs) {
    const qint64 now = nowNs();
    // A malformed or suspended-device clock must not pin a paddle ten seconds
    // in the past or schedule it in the future.
    if (eventTimeNs <= 0 || eventTimeNs > now
        || now - eventTimeNs > 10LL * 1000000000)
        eventTimeNs = now;

    auto &sources = dit ? m_ditSources : m_dahSources;
    const unsigned oldSources = pressed
            ? sources.fetch_or(sourceBit, std::memory_order_acq_rel)
            : sources.fetch_and(~sourceBit, std::memory_order_acq_rel);
    const unsigned newSources = pressed ? oldSources | sourceBit : oldSources & ~sourceBit;
    if ((oldSources != 0) == (newSources != 0))
        return;

    {
        std::lock_guard<std::mutex> lock(m_edgesMutex);
        // Preserve out-of-order timestamps among pending edges. Only move an
        // edge forward if its original boundary has already been processed.
        if (eventTimeNs <= m_processedThroughNs)
            eventTimeNs = m_processedThroughNs + 1;
        const PaddleEdge edge{eventTimeNs, dit, pressed, sourceBit};
        const auto position = std::upper_bound(m_edges.begin(), m_edges.end(), eventTimeNs,
                [](qint64 time, const PaddleEdge &item) { return time < item.timeNs; });
        m_edges.insert(position, edge);
    }
    if (QThread::currentThread() == thread())
        handlePaddleChange();
    else
        QMetaObject::invokeMethod(this, &IambicKeyer::handlePaddleChange,
                                  Qt::QueuedConnection);
}

IambicKeyer::PaddleSnapshot IambicKeyer::stateAt(qint64 boundaryNs) const {
    std::lock_guard<std::mutex> lock(m_edgesMutex);
    bool dit = m_historyBaseDit;
    bool dah = m_historyBaseDah;
    for (const PaddleEdge &edge : m_edges) {
        if (edge.timeNs > m_elementStartNs)
            break;
        (edge.dit ? dit : dah) = edge.pressed;
    }
    bool squeezed = dit && dah;
    bool ditPressed = false;
    bool dahPressed = false;
    for (const PaddleEdge &edge : m_edges) {
        if (edge.timeNs <= m_elementStartNs)
            continue;
        if (edge.timeNs > boundaryNs)
            break;
        (edge.dit ? dit : dah) = edge.pressed;
        if (edge.pressed)
            (edge.dit ? ditPressed : dahPressed) = true;
        squeezed |= dit && dah;
    }
    return {dit, dah, squeezed, ditPressed, dahPressed};
}

IambicKeyer::PendingPress IambicKeyer::nextPressAfter(qint64 boundaryNs) const {
    std::lock_guard<std::mutex> lock(m_edgesMutex);
    for (const PaddleEdge &edge : m_edges) {
        if (edge.pressed && edge.timeNs > boundaryNs)
            return {edge.timeNs, edge.dit, edge.sourceBit};
    }
    return {};
}

bool IambicKeyer::releasedBetween(bool dit, qint64 afterNs, qint64 throughNs) const {
    std::lock_guard<std::mutex> lock(m_edgesMutex);
    for (const PaddleEdge &edge : m_edges) {
        if (edge.timeNs > throughNs)
            break;
        if (edge.timeNs > afterNs && edge.dit == dit && !edge.pressed)
            return true;
    }
    return false;
}

void IambicKeyer::pruneThrough(qint64 boundaryNs) {
    std::lock_guard<std::mutex> lock(m_edgesMutex);
    // Java accepts MIDI timestamps up to 10 seconds old. An arrival-age or
    // fixed-count limit here could discard an edge before delayed playback
    // reaches its boundary, so retire only history already evaluated.
    while (!m_edges.empty() && m_edges.front().timeNs <= boundaryNs) {
        const PaddleEdge oldest = m_edges.front();
        (oldest.dit ? m_historyBaseDit : m_historyBaseDah) = oldest.pressed;
        m_edges.pop_front();
    }
    m_processedThroughNs = std::max(m_processedThroughNs, boundaryNs);
}

void IambicKeyer::handlePaddleChange() {
    if (!m_enabled || m_state != Idle)
        return;
    const PendingPress press = nextPressAfter(m_lastIdleBoundaryNs);
    if (!press.timeNs)
        return;
    const qint64 now = nowNs();
    const bool touchAssist = press.sourceBit == AndroidMidiSource
            && m_midiTouchLookaheadMs > 0;
    const qint64 touchReadyNs = touchAssist
            ? press.timeNs + qint64(m_midiTouchLookaheadMs) * NanosecondsPerMillisecond : 0;
    const qint64 readyNs = std::max(m_replayReadyNs, touchReadyNs);
    if (now < readyNs) {
        QTimer::singleShot(static_cast<int>((readyNs - now + 999999)
                                            / NanosecondsPerMillisecond),
                           this, &IambicKeyer::handlePaddleChange);
        return;
    }
    m_touchAssistedElement = touchAssist;
    enterElement(press.dit == !m_reversed, press.timeNs);
}

void IambicKeyer::enterElement(bool isDit, qint64 idlePressNs) {
    const bool fromIdle = m_state == Idle;
    const qint64 now = nowNs();
    if (fromIdle && m_lastIdleBoundaryNs > 0) {
        const qint64 gapNs = idlePressNs - m_lastIdleBoundaryNs;
        if (gapNs >= 0 && gapNs <= 2LL * 1000000000)
            emit restartAfterPause(static_cast<int>(gapNs / NanosecondsPerMillisecond));
    }
    m_state = isDit ? PlayingDit : PlayingDah;
    m_elementStartNs = fromIdle ? idlePressNs : m_nextBoundaryNs;

    const qint64 intervalNs = qint64(isDit ? m_ditMs * 2 : m_ditMs * 4)
                              * NanosecondsPerMillisecond;
    if (fromIdle) {
        m_nextBoundaryNs = m_elementStartNs + intervalNs;
        m_nextDeadlineNs = now + intervalNs;
    } else {
        m_nextBoundaryNs += intervalNs;
        const qint64 nextDeadline = m_nextDeadlineNs + intervalNs;
        // A missed *next* deadline begins a fresh full-length element. A
        // zero-ms catch-up timer made queued elements sound in a burst.
        m_nextDeadlineNs = nextDeadline <= now ? now + intervalNs : nextDeadline;
    }
    const qint64 remaining = m_nextDeadlineNs - now;
    m_elementTimer->start(static_cast<int>((remaining + 999999)
                                            / NanosecondsPerMillisecond));
    emit elementStarted(isDit);
}

void IambicKeyer::onTimerFired() {
    const qint64 now = nowNs();
    if (now < m_nextDeadlineNs) {
        m_elementTimer->start(static_cast<int>((m_nextDeadlineNs - now + 999999)
                                                / NanosecondsPerMillisecond));
        return;
    }
    const qint64 boundary = m_nextBoundaryNs;
    const PaddleSnapshot snapshot = stateAt(boundary);
    const bool liveDit = m_reversed ? snapshot.dah : snapshot.dit;
    const bool liveDah = m_reversed ? snapshot.dit : snapshot.dah;
    const bool wasDit = m_state == PlayingDit;
    const bool samePaddleReleasedInLookahead = m_touchAssistedElement
            && releasedBetween(wasDit != m_reversed, boundary,
                    boundary + qint64(m_midiTouchLookaheadMs) * NanosecondsPerMillisecond);
    // Remember a brief press of the opposite paddle, which cannot sound until
    // this element ends. A released re-press of the paddle already sounding
    // must not queue another element (for example, after contact bounce).
    const bool dit = (liveDit && !(wasDit && samePaddleReleasedInLookahead))
            || (!wasDit && (m_reversed ? snapshot.dahPressed : snapshot.ditPressed));
    const bool dah = (liveDah && !(!wasDit && samePaddleReleasedInLookahead))
            || (wasDit && (m_reversed ? snapshot.ditPressed : snapshot.dahPressed));

    if (snapshot.squeezed && !liveDit && !liveDah) {
        if (m_mode == IambicB)
            enterElement(!wasDit);
        else
            goIdle(boundary);
    } else if (dit && dah) {
        enterElement(!wasDit);
    } else if (wasDit && dah) {
        enterElement(false);
    } else if (!wasDit && dit) {
        enterElement(true);
    } else if (wasDit && dit) {
        enterElement(true);
    } else if (!wasDit && dah) {
        enterElement(false);
    } else if (snapshot.squeezed && m_mode == IambicB) {
        enterElement(!wasDit);
    } else {
        goIdle(boundary);
    }
    pruneThrough(boundary);
}

void IambicKeyer::goIdle(qint64 boundaryNs) {
    m_state = Idle;
    m_elementTimer->stop();
    m_lastIdleBoundaryNs = boundaryNs;
    emit characterSpace();
    // A later press in a received batch belongs to the next character. Replay
    // its original gap at a bounded length; otherwise a backlogged batch could
    // produce the right elements but merge the letters in local sidetone.
    const PendingPress next = nextPressAfter(boundaryNs);
    if (next.timeNs) {
        const qint64 gapNs = std::clamp(next.timeNs - boundaryNs, qint64(0),
                                      qint64(m_ditMs) * 7 * NanosecondsPerMillisecond);
        m_replayReadyNs = nowNs() + gapNs;
        QMetaObject::invokeMethod(this, &IambicKeyer::handlePaddleChange,
                                  Qt::QueuedConnection);
    } else {
        m_replayReadyNs = 0;
    }
}

void IambicKeyer::stop() {
    m_state = Idle;
    m_elementTimer->stop();
    m_ditSources.store(0);
    m_dahSources.store(0);
    m_lastIdleBoundaryNs = 0;
    m_replayReadyNs = 0;
    m_touchAssistedElement = false;
    std::lock_guard<std::mutex> lock(m_edgesMutex);
    m_edges.clear();
    m_processedThroughNs = 0;
    m_historyBaseDit = false;
    m_historyBaseDah = false;
}

#else
IambicKeyer::IambicKeyer(QObject *parent) : QObject(parent) {
    m_elementTimer = new QTimer(this);
    m_elementTimer->setSingleShot(true);
    m_elementTimer->setTimerType(Qt::PreciseTimer);
    connect(m_elementTimer, &QTimer::timeout, this, &IambicKeyer::onTimerFired);
    m_clock.start();
}

void IambicKeyer::setEnabled(bool enabled) {
    m_enabled = enabled;
    if (!enabled)
        stop();
}

void IambicKeyer::setMode(Mode mode) { m_mode = mode; }
void IambicKeyer::setReversed(bool reversed) { m_reversed = reversed; }
void IambicKeyer::setSpeed(int wpm) {
    if (wpm > 0)
        m_ditMs = 1200 / wpm;
}

void IambicKeyer::setDitPaddle(bool pressed) {
    m_physDit.store(pressed, std::memory_order_release);
    if (pressed)
        m_ditLatch.store(true, std::memory_order_release);
    QMetaObject::invokeMethod(this, &IambicKeyer::handlePaddleChange, Qt::QueuedConnection);
}

void IambicKeyer::setDahPaddle(bool pressed) {
    m_physDah.store(pressed, std::memory_order_release);
    if (pressed)
        m_dahLatch.store(true, std::memory_order_release);
    QMetaObject::invokeMethod(this, &IambicKeyer::handlePaddleChange, Qt::QueuedConnection);
}

bool IambicKeyer::ditDown() const {
    const bool dit = m_physDit.load(std::memory_order_acquire);
    const bool dah = m_physDah.load(std::memory_order_acquire);
    return m_reversed ? dah : dit;
}

bool IambicKeyer::dahDown() const {
    const bool dit = m_physDit.load(std::memory_order_acquire);
    const bool dah = m_physDah.load(std::memory_order_acquire);
    return m_reversed ? dit : dah;
}

void IambicKeyer::handlePaddleChange() {
    if (!m_enabled)
        return;
    const bool dit = ditDown() || (m_reversed ? m_dahLatch.load() : m_ditLatch.load());
    const bool dah = dahDown() || (m_reversed ? m_ditLatch.load() : m_dahLatch.load());
    if (m_state != Idle && dit && dah)
        m_squeezed = true;
    if (m_state == Idle) {
        if (dit && !dah)
            enterElement(true);
        else if (dah && !dit)
            enterElement(false);
        else if (dit && dah)
            enterElement(true);
    }
}

void IambicKeyer::enterElement(bool isDit) {
    const bool fromIdle = m_state == Idle;
    if (fromIdle && m_idleSince.isValid()) {
        const int elapsed = static_cast<int>(m_idleSince.elapsed());
        if (elapsed <= 2000)
            emit restartAfterPause(elapsed);
    }
    m_state = isDit ? PlayingDit : PlayingDah;
    m_squeezed = false;
    if (isDit != m_reversed)
        m_ditLatch.store(false, std::memory_order_release);
    else
        m_dahLatch.store(false, std::memory_order_release);
    if (ditDown() && dahDown())
        m_squeezed = true;

    const int intervalMs = isDit ? m_ditMs * 2 : m_ditMs * 4;
    const qint64 now = m_clock.nsecsElapsed();
    const qint64 intervalNs = qint64(intervalMs) * 1000000;
    m_nextDeadlineNs = fromIdle ? now + intervalNs : m_nextDeadlineNs + intervalNs;
    qint64 remaining = m_nextDeadlineNs - now;
    if (remaining < 0) {
        m_nextDeadlineNs = now;
        remaining = 0;
    }
    m_elementTimer->start(static_cast<int>((remaining + 500000) / 1000000));
    emit elementStarted(isDit);
}

void IambicKeyer::onTimerFired() {
    const bool liveDit = ditDown();
    const bool liveDah = dahDown();
    const bool latchDit = m_reversed ? m_dahLatch.load() : m_ditLatch.load();
    const bool latchDah = m_reversed ? m_ditLatch.load() : m_dahLatch.load();
    const bool dit = liveDit || latchDit;
    const bool dah = liveDah || latchDah;
    const bool wasDit = m_state == PlayingDit;

    if (m_squeezed && !liveDit && !liveDah) {
        if (m_mode == IambicB)
            enterElement(!wasDit);
        else
            goIdle();
    } else if (dit && dah) {
        enterElement(!wasDit);
    } else if (wasDit && dah) {
        enterElement(false);
    } else if (!wasDit && dit) {
        enterElement(true);
    } else if (wasDit && dit) {
        enterElement(true);
    } else if (!wasDit && dah) {
        enterElement(false);
    } else if (m_squeezed && m_mode == IambicB) {
        enterElement(!wasDit);
    } else {
        goIdle();
    }
}

void IambicKeyer::goIdle() {
    m_state = Idle;
    m_elementTimer->stop();
    m_squeezed = false;
    m_ditLatch.store(false);
    m_dahLatch.store(false);
    m_idleSince.start();
    emit characterSpace();
}

void IambicKeyer::stop() {
    m_state = Idle;
    m_elementTimer->stop();
    m_squeezed = false;
    m_physDit.store(false);
    m_physDah.store(false);
    m_ditLatch.store(false);
    m_dahLatch.store(false);
}

#endif
