#ifndef TEXTDECODEWINDOW_H
#define TEXTDECODEWINDOW_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include "wheelaccumulator.h"

class TextDecodeWindow : public QWidget {
    Q_OBJECT

public:
    enum Receiver { MainRx, SubRx };
    enum OperatingMode { ModeCW, ModeData, ModeSSB, ModeOther };

    explicit TextDecodeWindow(Receiver rx, QWidget *parent = nullptr);

    void appendText(const QString &text);
    void clearText();
    void setMaxLines(int lines);
    int maxLines() const { return m_maxLines; }
    Receiver receiver() const { return m_receiver; }

    // Decode state getters
    bool isDecodeEnabled() const { return m_decodeEnabled; }
    int wpmRange() const { return m_wpmRange; }
    bool autoThreshold() const { return m_autoThreshold; }
    int threshold() const { return m_threshold; }
    OperatingMode operatingMode() const { return m_operatingMode; }

    // Decode state setters (for syncing from radio)
    void setDecodeEnabled(bool enabled);
    void setWpmRange(int range);
    void setAutoThreshold(bool isAuto);
    void setThreshold(int value);
    void setOperatingMode(OperatingMode mode);

signals:
    void closeRequested();
    void enabledChanged(bool on);
    void wpmRangeChanged(int range);
    void thresholdModeChanged(bool isAuto);
    void thresholdChanged(int value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void setupUi();
    void trimToMaxLines();
    QRect titleBarRect() const;
    QRect resizeGripRect() const;
    void updateButtonStates();
    void updateWpmButton();
    void updateThresholdControls();
    void updateModeVisibility();
    QString controlButtonStyle(bool selected = false) const;

    Receiver m_receiver;
    OperatingMode m_operatingMode = ModeCW;
    // TD's line-count field is a single CAT digit (1-9).
    int m_maxLines = 9;

    // Decode state
    bool m_decodeEnabled = false;
    int m_wpmRange = 0; // 0=8-45, 1=8-60, 2=8-90
    bool m_autoThreshold = true;
    int m_threshold = 5; // 1-9

    // Title bar controls
    QLabel *m_titleLabel;
    QPushButton *m_onOffBtn;
    QPushButton *m_wpmBtn;
    QPushButton *m_autoManualBtn;
    QPushButton *m_thresholdMinusBtn;
    QLabel *m_thresholdValueLabel;
    QPushButton *m_thresholdPlusBtn;
    QPushButton *m_closeBtn;
    QPlainTextEdit *m_textDisplay;

    // Drag/resize state
    QPoint m_dragPosition;
    bool m_dragging = false;
    bool m_resizing = false;
    QPoint m_resizeStartPos;
    QSize m_resizeStartSize;
    WheelAccumulator m_wheelAccumulator;
};

#endif // TEXTDECODEWINDOW_H
