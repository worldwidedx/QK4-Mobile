#include "poweraverager.h"

#include <algorithm>
#include <cmath>

void PowerAverager::setLength(int frames) {
    const int length = std::clamp(frames, 1, 20);
    if (length == m_length)
        return;
    m_length = length;
    reset();
}

void PowerAverager::reset() {
    restart(m_bins);
}

void PowerAverager::restart(int bins) {
    m_bins = bins;
    m_count = 0;
    m_head = 0;
    m_sinceSum = 0;
    m_ring.fill(0.0, static_cast<qsizetype>(m_length) * bins);
    m_sum.fill(0.0, bins);
}

void PowerAverager::add(const QVector<float> &frameDbm, QVector<float> &outDbm) {
    const int bins = static_cast<int>(frameDbm.size());
    if (m_length == 1) {
        m_bins = bins;
        outDbm = frameDbm;
        return;
    }
    if (bins != m_bins)
        restart(bins);
    if (bins == 0) {
        outDbm.clear();
        return;
    }

    double *slot = m_ring.data() + static_cast<qsizetype>(m_head) * bins;
    const bool full = m_count == m_length;
    for (int i = 0; i < bins; ++i) {
        const double power = std::pow(10.0, static_cast<double>(frameDbm[i]) / 10.0);
        if (full)
            m_sum[i] -= slot[i];
        slot[i] = power;
        m_sum[i] += power;
    }
    m_head = (m_head + 1) % m_length;
    m_count = std::min(m_count + 1, m_length);

    if (++m_sinceSum >= 1024) {
        m_sinceSum = 0;
        std::fill(m_sum.begin(), m_sum.end(), 0.0);
        for (int f = 0; f < m_count; ++f) {
            const double *row = m_ring.constData() + static_cast<qsizetype>(f) * bins;
            for (int i = 0; i < bins; ++i)
                m_sum[i] += row[i];
        }
    }

    outDbm.resize(bins);
    for (int i = 0; i < bins; ++i)
        outDbm[i] = static_cast<float>(10.0 * std::log10(std::max(m_sum[i] / m_count, 1e-300)));
}
