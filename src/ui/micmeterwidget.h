#ifndef MICMETERWIDGET_H
#define MICMETERWIDGET_H

#include <QWidget>

/**
 * MicMeterWidget - Horizontal microphone level meter
 *
 * Displays audio input level as a horizontal bar with gradient coloring:
 * - Green: low level (0-60%)
 * - Yellow: medium level (60-80%)
 * - Red: high level (80-100%)
 *
 * The meter smoothly decays when the level drops (peak hold behavior).
 */
class MicMeterWidget : public QWidget {
    Q_OBJECT

public:
    explicit MicMeterWidget(QWidget *parent = nullptr);
    ~MicMeterWidget() = default;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setLevel(float level); // 0.0 to 1.0

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    float m_currentLevel = 0.0f;
    float m_peakLevel = 0.0f;
    int m_peakHoldFrames = 0;
    static constexpr int PEAK_HOLD_FRAMES = 30; // Hold peak for ~30 frames then decay
};

#endif // MICMETERWIDGET_H
