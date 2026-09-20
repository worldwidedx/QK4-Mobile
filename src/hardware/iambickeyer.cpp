#include "iambickeyer.h"

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
