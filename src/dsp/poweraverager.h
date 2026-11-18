#ifndef DSP_POWERAVERAGER_H
#define DSP_POWERAVERAGER_H

#include <QVector>

// Rolling mean of the last #AVG frames in linear power, expressed as dBm.
class PowerAverager {
public:
    void setLength(int frames);
    void reset();
    void add(const QVector<float> &frameDbm, QVector<float> &outDbm);
    void addEased(const QVector<float> &frameDbm, QVector<float> &shownDbm);

private:
    void restart(int bins);
    int m_length = 1;
    int m_bins = 0;
    int m_count = 0;
    int m_head = 0;
    int m_sinceSum = 0;
    QVector<double> m_ring;
    QVector<double> m_sum;
    QVector<float> m_average;
};

#endif
