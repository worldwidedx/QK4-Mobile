#ifndef DISPLAYPOPUPWIDGET_H
#define DISPLAYPOPUPWIDGET_H

#include "k4popupbase.h"
#include "wheelaccumulator.h"
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>

class DisplayMenuButton;
class ToggleGroupWidget;
class ControlGroupWidget;

class DisplayPopupWidget : public K4PopupBase {
    Q_OBJECT

public:
    enum MenuItem { PanWaterfall = 0, NbWtrClrs, RefLvlScale, SpanCenter, AveragePeak, FixedFreeze, CursAB };

    explicit DisplayPopupWidget(QWidget *parent = nullptr);

    MenuItem selectedItem() const { return m_selectedItem; }
    bool isLcdEnabled() const { return m_lcdEnabled; }
    bool isExtEnabled() const { return m_extEnabled; }
    bool isVfoAEnabled() const { return m_vfoAEnabled; }
    bool isVfoBEnabled() const { return m_vfoBEnabled; }
    bool autoRefLevelEnabled() const { return m_autoRef; }

public slots:
    // Span VALUES are per-VFO (A and B can have different spans)
    void setSpanValueA(double spanKHz);
    void setSpanValueB(double spanKHz);
    void setRefLevelValue(int dB);      // Legacy: sets A
    void setAutoRefLevel(bool enabled); // GLOBAL - affects both VFOs
    void setScale(int scale);           // GLOBAL - affects both panadapters (10-150)
    // Ref level VALUES are per-VFO (A and B can have different levels)
    void setRefLevelValueA(int dB);
    void setRefLevelValueB(int dB);

    // State setters for RadioState updates (separate LCD and EXT)
    void setDualPanModeLcd(int mode);
    void setDualPanModeExt(int mode);
    void setDisplayModeLcd(int mode);
    void setDisplayModeExt(int mode);
    void setWaterfallColor(int color);
    void setAveraging(int value);
    void setPeakMode(bool enabled);
    void setFixedTuneMode(int fxt, int fxa);
    void setFreeze(bool enabled);
    void setVfoACursor(int mode);
    void setVfoBCursor(int mode);
    void setDdcNbMode(int mode);   // 0=OFF, 1=ON, 2=AUTO
    void setDdcNbLevel(int level); // 0-14

    // Waterfall height percentage (0-100, default 50 = 50/50 split)
    void setWaterfallHeight(int percent);    // LCD: #WFHxx;
    void setWaterfallHeightExt(int percent); // EXT: #HWFHxx;
    void setWaterfallColorRange(int range);  // local WTR CLRS: 5-30

signals:
    // Note: closed() signal is inherited from K4PopupBase

    // Target toggle signals
    void lcdToggled(bool enabled);
    void extToggled(bool enabled);
    void vfoAToggled(bool enabled);
    void vfoBToggled(bool enabled);

    // Menu item signals
    void menuItemSelected(MenuItem item);
    void alternateItemClicked(MenuItem item);

    // Span control signals
    void spanIncrementRequested();
    void spanDecrementRequested();

    // Ref level control signals
    void refLevelIncrementRequested();
    void refLevelDecrementRequested();
    void autoRefLevelToggled(bool enabled);

    // Averaging control signals
    void averagingIncrementRequested();
    void averagingDecrementRequested();

    // Scale control signals (GLOBAL - applies to both panadapters)
    void scaleIncrementRequested();
    void scaleDecrementRequested();

    // DDC NB control signals
    void nbToggleRequested();
    void nbLevelIncrementRequested();
    void nbLevelDecrementRequested();

    // Waterfall height control signals
    void waterfallHeightIncrementRequested();
    void waterfallHeightDecrementRequested();

    // Local-only WTR CLRS control. This changes the app's waterfall mapping;
    // it deliberately does not issue a CAT command to the radio.
    void waterfallColorRangeLocallyChanged(int range);

    // CAT command signal (for MainWindow to forward to TcpClient)
    void catCommandRequested(const QString &cmd);
    void waterfallColorLocallyChanged(int color);

    // Pan mode changed (for MainWindow to update panadapter display)
    // K4 doesn't echo #DPM commands, so we notify directly
    void dualPanModeChanged(int mode);

    // Long-press CENTER is a radio command with no persistent, immediately
    // visible change in the phone layout. Request the standard feedback toast.
    void panadapterCentered();

    // #PKM toggle needs an immediate local renderer update; some K4 firmware
    // revisions do not echo the toggle back to the client promptly.
    void peakModeLocallyChanged(bool enabled);

protected:
    QSize contentSize() const override;

private:
    void setupUi();
    void setupTopRow();
    void setupBottomRow();

    QWidget *createSpanControlPage();
    QWidget *createRefLevelControlPage();
    QWidget *createScaleControlPage();
    QWidget *createAverageControlPage();
    QWidget *createNbControlPage();
    QWidget *createWaterfallColorRangeControlPage();
    QWidget *createWaterfallControlPage();
    QWidget *createDefaultControlPage();
    void updateNbControlGroupValue();
    void updateWaterfallColorRangeControlGroup();
    void updateWaterfallControlGroup();

    void updateToggleStyles();
    void updateMenuButtonStyles();
    void updateMenuButtonLabels();
    void onMenuItemClicked(MenuItem item);
    void onMenuItemRightClicked(MenuItem item);

    // Helper to get command prefix based on LCD/EXT selection
    QString getCommandPrefix() const;

    // Target toggle groups (bordered containers with triangle)
    ToggleGroupWidget *m_lcdExtGroup;
    ToggleGroupWidget *m_vfoAbGroup;
    bool m_lcdEnabled = true;
    bool m_extEnabled = false;
    bool m_vfoAEnabled = true;
    bool m_vfoBEnabled = false;
    bool m_vfoBAvailable = true;

    // Context-dependent controls
    QStackedWidget *m_controlStack;
    QWidget *m_spanControlPage;
    QWidget *m_refLevelControlPage;
    QWidget *m_averageControlPage;
    QWidget *m_nbControlPage;
    QWidget *m_waterfallColorRangeControlPage;
    QWidget *m_defaultControlPage;

    // Span controls
    ControlGroupWidget *m_spanControlGroup;

    // Ref level controls
    ControlGroupWidget *m_refLevelControlGroup;

    // Scale controls
    QWidget *m_scaleControlPage;
    ControlGroupWidget *m_scaleControlGroup;
    int m_scale = 75; // 10-150, default 75
    void updateScaleControlGroup();

    // Average controls
    ControlGroupWidget *m_averageControlGroup;
    QPushButton *m_peakButton = nullptr;

    // DDC NB controls
    ControlGroupWidget *m_nbControlGroup;
    ControlGroupWidget *m_waterfallColorRangeControlGroup;

    // Waterfall height controls
    QWidget *m_waterfallControlPage;
    ControlGroupWidget *m_waterfallControlGroup;

    // Menu buttons
    QList<DisplayMenuButton *> m_menuButtons;
    MenuItem m_selectedItem = SpanCenter;

    // Span state - values are per-VFO (A and B can have different spans)
    double m_spanA = 100.0;
    double m_spanB = 100.0;

    // Helper to update span control group based on A/B selection
    void updateSpanControlGroup();

    // Ref level state
    // Values are per-VFO (A and B can have different levels)
    int m_refLevelA = -108;
    int m_refLevelB = -108;
    // Auto-ref mode is GLOBAL (affects both VFOs simultaneously)
    bool m_autoRef = true; // Default to auto

    // Helper to update ref level control group based on A/B selection
    void updateRefLevelControlGroup();

    // Display state tracking (for menu button labels and CAT commands)
    // Initial values are -1 to ensure first update from radio triggers label refresh
    // Separate LCD and EXT state
    int m_dualPanModeLcd = -1; // #DPM: LCD 0=A, 1=B, 2=Dual
    int m_dualPanModeExt = -1; // #HDPM: EXT 0=A, 1=B, 2=Dual
    int m_displayModeLcd = -1; // #DSM: LCD 0=spectrum, 1=spectrum+waterfall
    int m_displayModeExt = -1; // #HDSM: EXT 0=spectrum, 1=spectrum+waterfall
    int m_waterfallColor = -1; // 0-4
    int m_waterfallColorRange = 10; // local WTR CLRS: 5-30, step 1
    int m_averaging = -1;      // 1-20
    int m_peakMode = -1;       // 0=off, 1=on (int for -1 init)
    int m_fixedTuneMode = -1;  // Combined FXT+FXA state (0-5: SLIDE1, SLIDE2, FIXED1, FIXED2, STATIC, TRACK)
    int m_freeze = -1;         // 0=run, 1=freeze (int for -1 init)
    int m_vfaMode = -1;        // 0=OFF, 1=ON, 2=AUTO, 3=HIDE
    int m_vfbMode = -1;

    // DDC NB state (#NB$ and #NBL$ commands)
    int m_ddcNbMode = -1;  // 0=OFF, 1=ON, 2=AUTO
    int m_ddcNbLevel = -1; // 0-14

    // Waterfall height state (#WFHxx and #HWFHxx commands)
    // Global setting - applies to both VFO A and B
    int m_waterfallHeight = 50;    // LCD: 0-100% (default 50%)
    int m_waterfallHeightExt = 50; // EXT: 0-100% (default 50%)
};

// Helper class for dual-line menu buttons
class DisplayMenuButton : public QWidget {
    Q_OBJECT

public:
    explicit DisplayMenuButton(const QString &primaryText, const QString &alternateText, QWidget *parent = nullptr);

    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }

    QString primaryText() const { return m_primaryText; }
    QString alternateText() const { return m_alternateText; }

    void setPrimaryText(const QString &text);
    void setAlternateText(const QString &text);

signals:
    void clicked();
    void rightClicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_primaryText;
    QString m_alternateText;
    bool m_selected = false;
    bool m_hovered = false;
    QTimer m_longPressTimer;
    QPoint m_pressPosition;
    bool m_leftPressed = false;
    bool m_longPressHandled = false;
    bool m_pressCancelled = false;
};

// Helper class for control groups (SPAN, REF) with bordered container and demarcation lines
class ControlGroupWidget : public QWidget {
    Q_OBJECT

public:
    explicit ControlGroupWidget(const QString &label, QWidget *parent = nullptr);

    void setValue(const QString &value);
    QString value() const { return m_value; }

    void setShowAutoButton(bool show);
    // NB needs only a short label. Reclaim width without shrinking the +/-
    // hit areas shared by all control groups.
    void setCompactLabel(bool compact);
    void setAutoEnabled(bool enabled);
    bool isAutoEnabled() const { return m_autoEnabled; }
    void setValueFaded(bool faded); // Faded grey text when in auto mode
    bool isValueFaded() const { return m_valueFaded; }

signals:
    void incrementClicked();
    void decrementClicked();
    void autoClicked();
    void valueClicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QString m_label;
    QString m_value;
    bool m_showAutoButton = false;
    bool m_compactLabel = false;
    bool m_autoEnabled = false;
    bool m_valueFaded = false; // Grey text when auto mode
    QRect m_autoRect;
    QRect m_valueRect;
    QRect m_minusRect;
    QRect m_plusRect;
    WheelAccumulator m_wheelAccumulator;
};

// Helper class for bordered toggle groups with rounded corners
class ToggleGroupWidget : public QWidget {
    Q_OBJECT

public:
    explicit ToggleGroupWidget(const QString &leftLabel, const QString &rightLabel, QWidget *parent = nullptr);

    void setLeftSelected(bool selected);
    void setRightSelected(bool selected);
    void setRightEnabled(bool enabled);

    bool isLeftSelected() const { return m_leftSelected; }
    bool isRightSelected() const { return m_rightSelected; }
    bool isRightEnabled() const { return m_rightEnabled; }

signals:
    void leftClicked();
    void rightClicked();
    void bothClicked(); // When & is clicked, select both

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QString m_leftLabel;
    QString m_rightLabel;
    bool m_leftSelected = true;
    bool m_rightSelected = false;
    bool m_rightEnabled = true;

    // Hit test rectangles (calculated in paintEvent)
    QRect m_leftRect;
    QRect m_rightRect;
    QRect m_ampRect;
};

#endif // DISPLAYPOPUPWIDGET_H
