#include "wheelaccumulator.h"

WheelAccumulator::WheelAccumulator(int threshold) : m_threshold(threshold) {}

int WheelAccumulator::accumulate(QWheelEvent *event) {
    if (shouldFilter(event))
        return 0;

    if (event->phase() == Qt::ScrollBegin)
        m_accumulator = 0;

    return computeSteps(m_accumulator, event->angleDelta().y());
}

int WheelAccumulator::accumulate(QWheelEvent *event, int key) {
    if (key < 0 || key > 3)
        key = 0;

    if (shouldFilter(event))
        return 0;

    if (event->phase() == Qt::ScrollBegin) {
        // Reset all keyed accumulators on gesture start
        for (int &acc : m_keyedAccumulators)
            acc = 0;
    }

    return computeSteps(m_keyedAccumulators[key], event->angleDelta().y());
}

void WheelAccumulator::reset() {
    m_accumulator = 0;
    for (int &acc : m_keyedAccumulators)
        acc = 0;
}

void WheelAccumulator::setFilterMomentum(bool filter) {
    m_filterMomentum = filter;
}

int WheelAccumulator::computeSteps(int &accumulator, int delta) {
    accumulator += delta;
    int steps = accumulator / m_threshold;
    accumulator -= steps * m_threshold;
    return steps;
}

bool WheelAccumulator::shouldFilter(QWheelEvent *event) const {
    if (m_filterMomentum && event->phase() == Qt::ScrollMomentum)
        return true;
    return false;
}
