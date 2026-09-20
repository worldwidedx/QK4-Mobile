#ifndef KPA1500PANEL_H
#define KPA1500PANEL_H

#include <QWidget>
#include <QPushButton>
#include <QTimer>

/**
 * KPA1500Panel - Compact control panel for KPA1500 amplifier.
 *
 * Layout (270×185px):
 * - Header: "KPA1500" title + status labels (OPER/STBY, ATU IN/BYP)
 * - Hero meter: FWD power bar (0-1500W) with scale
 * - Mini meters: REF (0-100W), SWR (1.0-3.0+), TMP (0-100°C)
 * - Button row: OPER/STBY, IN/BYP, ANT1-3, TUNE, CLR
 */
class KPA1500Panel : public QWidget {
    Q_OBJECT

public:
    explicit KPA1500Panel(QWidget *parent = nullptr);

    // State setters (called from KPA1500Client responses)
    void setMode(bool operate);          // true=OPER, false=STBY
    void setAtuMode(bool in);            // true=IN, false=BYP
    void setAntenna(int ant);            // 1, 2, or 3
    void setForwardPower(float watts);   // 0-1500
    void setReflectedPower(float watts); // 0-100
    void setSWR(float swr);              // 1.0-3.0+
    void setTemperature(float celsius);  // 0-100
    void setFault(bool fault);           // Fault indicator
    void setConnected(bool connected);   // Connection state

signals:
    void modeToggled(bool operate); // User toggled MODE button
    void atuTuneRequested();        // User clicked TUNE button
    void atuModeToggled(bool in);   // User toggled ATU button
    void antennaChanged(int ant);   // User clicked ANT button

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void setupUi();
    void updateButtonStates();

    // Drawing helpers
    void drawMeter(QPainter &painter, int y, const QString &label, const QString &valueStr, float displayRatio,
                   float peakRatio, const QStringList &scaleLabels, bool large = true);
    void drawStatusLabels(QPainter &painter, int y, int height);
    void drawPeakMarker(QPainter &painter, int barX, int barY, int barWidth, int barHeight, float peakRatio);

    // Peak decay
    void startDecayTimer();
    void onDecayTimer();

    // State
    bool m_operate = false;
    bool m_atuIn = false;
    int m_antenna = 1;
    float m_forwardPower = 0.0f;
    float m_reflectedPower = 0.0f;
    float m_swr = 1.0f;
    float m_temperature = 0.0f;
    bool m_fault = false;
    bool m_connected = false;

    // Display values (smoothed for animation)
    float m_displayForwardPower = 0.0f;
    float m_displayReflectedPower = 0.0f;
    float m_displaySwr = 1.0f;
    float m_displayTemperature = 0.0f;

    // Peak hold values (for meter decay)
    float m_peakForwardPower = 0.0f;
    float m_peakReflectedPower = 0.0f;
    float m_peakSwr = 1.0f;

    // Animation timer
    QTimer *m_decayTimer = nullptr;
    static constexpr int DECAY_INTERVAL_MS = 33;    // ~30fps update
    static constexpr float ATTACK_RATE = 0.35f;     // 35% per interval (fast rise ~100ms)
    static constexpr float DECAY_RATE = 0.06f;      // 6% per interval (slow fall ~600ms)
    static constexpr float PEAK_DECAY_RATE = 0.04f; // 4% per interval for peak markers (~800ms)

    // Buttons
    QPushButton *m_modeBtn = nullptr;
    QPushButton *m_atuBtn = nullptr;
    QPushButton *m_antBtn = nullptr;
    QPushButton *m_tuneBtn = nullptr;
};

#endif // KPA1500PANEL_H
