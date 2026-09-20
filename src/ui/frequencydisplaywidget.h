#ifndef FREQUENCYDISPLAYWIDGET_H
#define FREQUENCYDISPLAYWIDGET_H

#include <QWidget>
#include <QColor>
#include <QFont>
#include "wheelaccumulator.h"
class QTimer;

/**
 * FrequencyDisplayWidget - Inline frequency display with segment-based editing.
 *
 * Features:
 * - Custom-painted frequency with dot separators (XX.XXX.XXX)
 * - Click any digit to enter edit mode at that position
 * - All digits change to edit color (VFO theme: cyan for A, green for B)
 * - Type digits to replace at cursor position (auto-advances)
 * - Arrow keys to navigate between digits
 * - Enter to send frequency, Escape/click-outside to cancel
 *
 * Usage:
 *   auto *freq = new FrequencyDisplayWidget(parent);
 *   freq->setEditModeColor(QColor("#00BFFF")); // Cyan for VFO A
 *   freq->setFrequency("7.024.980");
 *   connect(freq, &FrequencyDisplayWidget::frequencyEntered, ...);
 */
class FrequencyDisplayWidget : public QWidget {
    Q_OBJECT

public:
    // Ten digits covers frequencies through 9.999 GHz, including VHF/UHF
    // transverter dial frequencies such as 144.200.000 and 1.296.000.000.
    static constexpr int kDigits = 10;
    static constexpr int kMaxDigitIndex = kDigits - 1;

    explicit FrequencyDisplayWidget(QWidget *parent = nullptr);

    // Set the displayed frequency (with or without dots)
    // Examples: "7.024.980", "7024980", "14.024.980"
    void setFrequency(const QString &frequency);

    // Get the current frequency as plain digits (no dots)
    QString frequency() const;

    // Get the displayed text with dots (e.g., "7.024.980")
    QString displayText() const;

    // Set the color used when in edit mode (typically VFO theme color)
    void setEditModeColor(const QColor &color);

    // Set the color used when NOT in edit mode (normally white, gray when dimmed)
    void setNormalColor(const QColor &color);

    // Set tuning rate indicator - digits at this position and below show in gray
    // digitFromRight: 0=1Hz, 1=10Hz, 2=100Hz, 3=1kHz, 4=10kHz, -1=none
    // e.g., rate 2 (100Hz) grays out digits 0,1,2 (1s, 10s, 100s places)
    void setTuningRateDigit(int digitFromRight);

    // Check if currently in edit mode
    bool isEditing() const;
    // Opt-in fitting and phone gestures for embedded frequency displays.
    void setAutoFit(bool enabled);
    void setTouchTuningEnabled(bool enabled);
    void setSelectedTuningDigit(int digitFromRight);

signals:
    // Emitted when user presses Enter to confirm frequency entry
    // digits is the frequency as plain digits (e.g., "7024980")
    void frequencyEntered(const QString &digits);

    // Emitted when editing is cancelled (Escape or click outside)
    void editingCancelled();

    // Emitted when user scrolls mouse wheel over frequency (not in edit mode)
    void frequencyScrolled(int steps);

    // Emitted on Android when a displayed digit is chosen for touch stepping.
    // digitFromRight is 0 for 1 Hz, 3 for 1 kHz, 6 for 1 MHz, etc.
    void tuningDigitSelected(int digitFromRight);
    void directEntryRequested();

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    // Enter edit mode with cursor at the specified digit position.
    void enterEditMode(int digitPosition);

    // Exit edit mode, optionally sending the frequency
    void exitEditMode(bool send);

    // Convert mouse X coordinate to a digit position, or -1 if not on a digit.
    int digitPositionFromX(int x) const;

    // Get the bounding rectangle for a character at the given index
    QRect charRectAt(int charIndex) const;

    // Convert character index in display string to a digit index.
    // Returns -1 for dot characters
    int digitIndexFromCharIndex(int charIndex) const;

    // Index of the first stored digit currently visible. Normal HF display
    // retains its compact width; higher-frequency leading digits are revealed.
    int displayStartIndex() const;

    // Format m_digits as display string with dots (e.g., "7.024.980")
    QString formatWithDots() const;

    // Parse frequency string and normalize to kDigits.
    void parseFrequency(const QString &freq);
    void updateFontMetrics();

    // Member variables
    QString m_digits;          // kDigits-wide string, left-padded with zeros
    QString m_originalDigits;  // Backup for cancel operation
    int m_cursorPosition = -1; // -1 = not editing
    int m_touchStepPosition = -1; // Android touch-step selection

    QColor m_normalColor; // White - normal display color
    QColor m_editColor;   // Cyan/Green - edit mode color
    QFont m_font;         // Inter with tabular figures, 32px bold

    // Tuning rate indicator: digits from this position to 0 show in gray
    int m_tuningRateDigit = -1; // -1 = no indicator, 0-4 = position from right

    WheelAccumulator m_wheelAccumulator;

    // Cached character metrics for click detection
    int m_charWidth = 0;
    int m_dotWidth = 0;
    bool m_autoFit = false;
#ifdef Q_OS_ANDROID
    bool m_touchTuningEnabled = true;
#else
    bool m_touchTuningEnabled = false;
#endif
    QPoint m_touchPress;
    QTimer *m_holdTimer = nullptr;
};

#endif // FREQUENCYDISPLAYWIDGET_H
