#ifndef WHEELACCUMULATOR_H
#define WHEELACCUMULATOR_H

#include <QWheelEvent>

/**
 * @brief Accumulates QWheelEvent angle deltas into discrete steps.
 *
 * Standard notched mice send +-120 per click (one step immediately).
 * Trackpads and Magic Mouse send many small deltas (2-20 per event);
 * without accumulation these either cause value bursts or get dropped.
 *
 * Usage:
 *   int steps = m_wheelAccumulator.accumulate(event);
 *   if (steps != 0)
 *       doSomething(steps);
 *
 * Keyed variant for widgets that use modifier keys to select different
 * parameters (e.g., panadapter: plain=freq, Shift=scale, Ctrl=refLevel):
 *   int steps = m_wheelAccumulator.accumulate(event, key);
 */
class WheelAccumulator {
public:
    explicit WheelAccumulator(int threshold = 120);

    /**
     * @brief Accumulate a wheel event and return discrete step count.
     * @return +N (scroll up), -N (scroll down), 0 (still accumulating)
     */
    int accumulate(QWheelEvent *event);

    /**
     * @brief Keyed variant — maintains independent accumulators per key.
     * @param key Index 0-3 (caller maps modifier combos to keys)
     */
    int accumulate(QWheelEvent *event, int key);

    void reset();
    void setFilterMomentum(bool filter);

private:
    int computeSteps(int &accumulator, int delta);
    bool shouldFilter(QWheelEvent *event) const;

    int m_threshold;
    bool m_filterMomentum = true;
    int m_accumulator = 0;
    int m_keyedAccumulators[4] = {};
};

#endif // WHEELACCUMULATOR_H
