#ifndef ADJUSTOVERLAY_H
#define ADJUSTOVERLAY_H

#include <QWidget>
#include <functional>
#include "dualcontrolbutton.h"

class QSlider;
class QLabel;
class QPushButton;
class QTimer;

/**
 * @brief Touch adjustment popup for a single left-column control tile.
 *
 * Mirrors the macOS SideControlOverlay look (dark rounded panel with a
 * context-coloured indicator bar) but is driven by a large slider plus fine
 * -/+ buttons instead of the mouse wheel, so a DualControlButton value can be
 * set by touch on iPad/iPhone. Opened by a long-press on the tile (the touch
 * equivalent of the macOS right-click/wheel interaction).
 *
 * A Qt::Popup window: tapping anywhere outside dismisses it. It also closes
 * itself after a short period of inactivity.
 */
class AdjustOverlay : public QWidget {
    Q_OBJECT

public:
    explicit AdjustOverlay(QWidget *parent = nullptr);

    /// The slider the owner configures (range/value) and connects to.
    QSlider *slider() const { return m_slider; }

    /// Set the title text and indicator-bar colour for this control.
    void configure(const QString &title, DualControlButton::Context context);

    /// Update the large value readout shown above the slider.
    void setValueText(const QString &text);

    /// Set how the slider value is rendered in the readout (e.g. kHz). Pass an
    /// empty function to fall back to the raw integer.
    void setValueFormatter(std::function<QString(int)> formatter);

    /// Position over @p anchor (global coords) and show. Resets inactivity.
    void showOver(QWidget *anchor);

    /// Restart the inactivity auto-close timer (call on any interaction).
    void pokeActivity();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QColor barColor() const;
    void setSliderFromX(int xPosition);

    DualControlButton::Context m_context = DualControlButton::Global;
    QString formatValue(int value) const;

    std::function<QString(int)> m_formatter;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_valueLabel = nullptr;
    QSlider *m_slider = nullptr;
    QPushButton *m_minusBtn = nullptr;
    QPushButton *m_plusBtn = nullptr;
    QTimer *m_inactivityTimer = nullptr;

    static constexpr int IndicatorBarWidth = 5;
    static constexpr int CornerRadius = 8;
    static constexpr int InactivityMs = 3500;
};

#endif // ADJUSTOVERLAY_H
