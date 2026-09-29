#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "ft8/ft8radiomode.h"

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QMenuBar>
#include <QMenu>
#include <QProgressBar>
#include <QTimer>
#include <QThread>
#include <QStackedWidget>
#include <atomic>
#include <QPointer>
#include "network/tcpclient.h"
#include "settings/radiosettings.h"
#include "models/radiostate.h"
#include "ui/vfowidget.h"
#include "ui/wheelaccumulator.h"

class PanadapterRhiWidget;
class AudioEngine;
class OpusDecoder;
class SideControlPanel;
class RightSidePanel;
class BottomMenuBar;
class QDialog;
class QScrollArea;
class MenuModel;
class MenuOverlayWidget;
class BandPopupWidget;
class ButtonRowPopup;
class DisplayPopupWidget;
class FnPopupWidget;
class RxEqPopupWidget;
class AntennaCfgPopupWidget;
class LineOutPopupWidget;
class LineInPopupWidget;
class MicInputPopupWidget;
class MicConfigPopupWidget;
class VoxPopupWidget;
class SsbBwPopupWidget;
class KeyingWeightPopupWidget;
class FmPlPopupWidget;
class DtmfPopupWidget;
class TxModePopupWidget;
class SoftwareListPopupWidget;
class TextDecodeWindow;
class MacroDialog;
class FilterIndicatorWidget;
class FeatureMenuBar;
class ModePopupWidget;
class KpodDevice;
class HalikeyDevice;
class Ctr2MidiDevice;
class MidiInputRouter;
class IambicKeyer;
class TxMeterWidget;
class KPA1500Client;
class KPA1500Window;
class CatServer;
class OptionsDialog;
class NotificationWidget;
class VfoRowWidget;
class SidetoneGenerator;
class RadioManagerDialog;
class QResizeEvent;
class QImage;
class SstvScreen;
class SstvDecoder;
class Ft8Screen;
class Ft8Receiver;
class Ft8Transmitter;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    // Panadapter display modes
    enum class PanadapterMode { MainOnly, Dual, SubOnly };

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // Switch between Main only, Dual (A+B), and Sub only display
    void setPanadapterMode(PanadapterMode mode);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onStateChanged(TcpClient::ConnectionState state);
    void onError(const QString &error);
    void onAuthenticated();
    void onAuthenticationFailed();
    void onCatResponse(const QString &response);
    void onFrequencyChanged(quint64 freq);
    void onFrequencyBChanged(quint64 freq);
    void onModeChanged(RadioState::Mode mode);
    void onModeBChanged(RadioState::Mode mode);
    void onSMeterChanged(double value);
    void onSMeterBChanged(double value);
    void onBandwidthChanged(int bw);
    void onBandwidthBChanged(int bw);
    void onRfPowerChanged(double watts, bool isQrp);
    void onSupplyVoltageChanged(double volts);
    void onSupplyCurrentChanged(double amps);
    void onSwrChanged(double swr);
    void onSplitChanged(bool enabled);
    void onAntennaChanged(int txAnt, int rxAntMain, int rxAntSub);
    void onAntennaNameChanged(int index, const QString &name);
    void onVoxChanged(bool enabled);
    void onQskEnabledChanged(bool enabled);
    void onTestModeChanged(bool enabled);
    void onAtuModeChanged(int mode);
    void onRitXitChanged(bool ritEnabled, bool xitEnabled, int offset);
    void onMessageBankChanged(int bank);
    void onProcessingChanged();
    void onProcessingChangedB();
    void onSpectrumData(int receiver, const QByteArray &data, qint64 centerFreq, qint32 sampleRate, float noiseFloor);
    void onMiniSpectrumData(int receiver, const QByteArray &data);
    void showRadioManager();
    void connectToRadio(const RadioEntry &radio);
    void updateDateTime();
    void showMenuOverlay();
    void onMenuValueChangeRequested(int menuId, const QString &action);
    void onMenuModelValueChanged(int menuId, int newValue);
    void onBandSelected(const QString &bandName);
    void updateBandSelection(int bandNum);
    void updateBandSelectionB(int bandNum);
    void toggleDisplayPopup();
    void toggleBandPopup();
    void toggleFnPopup();
    void toggleMainRxPopup();
    void toggleSubRxPopup();
    void toggleTxPopup();
    void closeAllPopups();
    void closeSecondaryPopups();

    // KPOD slots
    void onKpodEncoderRotated(int ticks);
    void onKpodRockerChanged(int position);
    void onKpodPollError(const QString &error);
    void onKpodEnabledChanged(bool enabled);

    // KPA1500 slots
    void onKpa1500Connected();
    void onKpa1500Disconnected();
    void onKpa1500Error(const QString &error);
    void onKpa1500EnabledChanged(bool enabled);
    void onKpa1500SettingsChanged();
    void updateKpa1500Status();

    // Error/notification from K4 (ERxx: messages)
    void onErrorNotification(int errorCode, const QString &message);

    // PTT slots
    void onPttPressed();
    void onPttReleased();

    // Display FPS (synthetic menu item)
    void onDisplayFpsChanged(int fps);

    // Fn popup / macro slots
    void onFnFunctionTriggered(const QString &functionId);
    void executeMacro(const QString &functionId);
    void executeMidiMacro(const QString &command);
    void handleMidiKnobAction(const QString &action, int value, bool absolute);
    void handleMidiButtonAction(const QString &action);
    void openMacroDialog();

    // MAIN RX / SUB RX popup slots
    void onMainRxButtonClicked(int index);
    void onMainRxButtonRightClicked(int index);
    void onSubRxButtonClicked(int index);
    void onSubRxButtonRightClicked(int index);

private:
    void setupMenuBar();
    void showSettings();
    void showAboutDialog();
    void setupUi();
    void setupTopStatusBar(QWidget *parent);
    void setupVfoSection(QWidget *parent);
    void setupSpectrumPlaceholder(QWidget *parent);
    void showFrequencyEntry(bool vfoB);
    void showPhoneControls();
    void showPhoneControlAdjustment(const QString &action);
    void showMidiAdjustmentSurface(const QString &action);
    void showRitXitAdjustment(bool preferXit);
    void showFeatureAdjustment(int feature);
    void updateConnectionState(TcpClient::ConnectionState state);
    QString formatFrequency(quint64 freq);
    void updateModeLabels();
    void refreshTxPopupForMode();
    void refreshRxPopupsForModes();
    RadioState::Mode txOperatingMode() const;
    void queueControlFeedback(const QString &key, const QString &fallback);
    void completeControlFeedback(const QString &key, const QString &message);
    void showControlFeedback(const QString &message);
    QString requestText(const QString &title, const QString &label, const QString &initial, bool *accepted);
    void setPhoneTxInputShieldActive(bool active);
    void updatePhoneTxInputShieldGeometry();
    void openSstvScreen();
    void openLogbook(bool fromSstv = false);
    void openFt8Screen();
    void updateFt8RadioMode();
    void refreshFt8Capture();
    QString ft8TransmitReadiness() const;
    void startFt8Transmit(const QString &message, int mode, int hz, qint64 slotUtc);
    void stopFt8Transmit();
    void refreshSstvRadioHeader();
    void startSstvTransmit(const QImage &frame, int modeId);
    void stopSstvTransmission();
    void beginSstvTransmitDrain();
    void finishSstvTransmission();
    void setSstvRfPower(double watts);
    void showDigitalAudioSetup(int mode, QWidget *surface);
    void startDigitalCalibration(int mode);
    void cancelDigitalCalibration();
    QString digitalCalibrationContext(int mode) const;
    void showDigitalProtection(int mode, const QString &text, bool fault);

    // Band and mini pan helpers
    int getBandFromFrequency(quint64 freq);
    bool areVfosOnDifferentBands();
    void checkAndHideMiniPanB();

    TcpClient *m_tcpClient;
    RadioState *m_radioState;
    QTimer *m_clockTimer;

    // I/O thread (TcpClient + Protocol + OpusDecoder)
    QThread *m_ioThread = nullptr;

    // Audio
    AudioEngine *m_audioEngine;
    QThread *m_audioThread = nullptr;
    OpusDecoder *m_opusDecoder;
    SstvDecoder *m_sstvDecoder = nullptr;
    QThread *m_sstvDecoderThread = nullptr;
    std::atomic<bool> m_sstvRxArmed{false};
    Ft8Receiver *m_ft8Receiver = nullptr;
    Ft8Transmitter *m_ft8Transmitter = nullptr;
    quint64 m_ft8TxGeneration = 0;
    QString m_ft8TransmitContext;
    qint64 m_ft8TransmitFrequency = 0;
    QThread *m_ft8Thread = nullptr;
    Ft8Screen *m_ft8Screen = nullptr;
    Ft8RadioMode m_ft8RadioMode;

    // PTT state
    bool m_pttActive = false;
    int m_phoneTuneStepAHz = -1;
    int m_phoneTuneStepBHz = -1;
    QString m_pendingControlFeedback;
    QString m_pendingControlFallback;
    int m_controlFeedbackGeneration = 0;
    QTimer *m_ritXitLongPressTimer = nullptr;
    QObject *m_ritXitPressTarget = nullptr;
    bool m_ritXitLongPressHandled = false;

    // Top status bar
    QLabel *m_titleLabel;
    QLabel *m_dateTimeLabel;
    QLabel *m_powerLabel;
    QLabel *m_swrLabel;
    QLabel *m_voltageLabel;
    QLabel *m_currentLabel;
    QLabel *m_lpaTempLabel;
    QLabel *m_paTempLabel;
    QLabel *m_connectionStatusLabel;
    QLabel *m_kpa1500StatusLabel;
    KPA1500Window *m_kpa1500Window;

    // VFO widgets (modular, reusable components)
    VFOWidget *m_vfoA;
    VFOWidget *m_vfoB;

    // NOTE: TX meters are now integrated into VFOWidgets as multifunction S/Po meters
    // (see VFOWidget::m_txMeter - displays S-meter when RX, Po when TX)

    // Mode labels (in center section, not in VFOWidget)
    QLabel *m_modeALabel;
    QLabel *m_modeBLabel;

    // RX Antenna labels (in antenna row below VFOs)
    QLabel *m_rxAntALabel;
    QLabel *m_rxAntBLabel;

    // Center section - first row with absolute positioning
    VfoRowWidget *m_vfoRow;

    // Center section labels (pointers to VfoRowWidget children)
    QWidget *m_vfoASquare; // VfoSquareWidget - used for event filter
    QLabel *m_txTriangle;  // Left triangle (pointing at A) - shown when split OFF
    QLabel *m_txTriangleB; // Right triangle (pointing at B) - shown when split ON
    QLabel *m_txIndicator;
    QWidget *m_vfoBSquare; // VfoSquareWidget - used for event filter
    QLabel *m_splitLabel;
    QLabel *m_bSetLabel;
    QLabel *m_subLabel; // SUB indicator (green when sub RX enabled)
    QLabel *m_divLabel; // DIV indicator (green when diversity enabled)
    QLabel *m_msgBankLabel;
    QWidget *m_ritXitBox;
    QLabel *m_ritLabel;
    QLabel *m_xitLabel;
    QLabel *m_ritXitValueLabel;
    QLabel *m_atuLabel;
    FilterIndicatorWidget *m_filterAWidget; // VFO A filter indicator
    FilterIndicatorWidget *m_filterBWidget; // VFO B filter indicator

    // Memory buttons (M1-M4, REC, STORE, RCL)
    QPushButton *m_m1Btn;
    QPushButton *m_m2Btn;
    QPushButton *m_m3Btn;
    QPushButton *m_m4Btn;
    QPushButton *m_recBtn;
    QPushButton *m_storeBtn;
    QPushButton *m_rclBtn;
    QLabel *m_voxLabel;
    QLabel *m_qskLabel;
    QLabel *m_testLabel;
    QLabel *m_txAntennaLabel;

    // Spectrum/Waterfall displays (QRhiWidget - Metal/DirectX/Vulkan)
    PanadapterRhiWidget *m_panadapterA; // VFO A (Main RX)
    PanadapterRhiWidget *m_panadapterB; // VFO B (Sub RX) - for future use
    QWidget *m_panAFrame = nullptr;     // thin bordered wrapper around panadapter A
    QWidget *m_panBFrame = nullptr;     // thin bordered wrapper around panadapter B
    QWidget *m_spectrumContainer;

    // Span control buttons (overlay on panadapter A)
    QPushButton *m_spanUpBtn;
    QPushButton *m_spanDownBtn;
    QPushButton *m_centerBtn;

    // Span control buttons (overlay on panadapter B)
    QPushButton *m_spanUpBtnB;
    QPushButton *m_spanDownBtnB;
    QPushButton *m_centerBtnB;

    // VFO indicator badges (bottom-left corner of waterfall)
    QLabel *m_vfoIndicatorA;
    QLabel *m_vfoIndicatorB;

    // Panadapter display mode
    PanadapterMode m_panadapterMode = PanadapterMode::MainOnly;

    // Control panels (L-shaped layout)
    SideControlPanel *m_sideControlPanel;
    RightSidePanel *m_rightSidePanel;
    BottomMenuBar *m_bottomMenuBar;
    QScrollArea *m_leftPanelScroll = nullptr;
    QScrollArea *m_rightPanelScroll = nullptr;
    QWidget *m_phoneControlsDialog = nullptr;
    QWidget *m_phoneTxInputShield = nullptr;
    QPushButton *m_phoneTxReleaseButton = nullptr;
    NotificationWidget *m_controlNotificationWidget = nullptr;
    RadioManagerDialog *m_radioManager = nullptr;
    SstvScreen *m_sstvScreen = nullptr;
    bool m_sstvTxStarting = false;
    bool m_sstvTxActive = false;
    bool m_sstvTxDraining = false;
    bool m_sstvProtectionFault = false, m_sstvProtectionReduced = false;
    quint64 m_sstvGeneration = 0;
    bool m_digitalCalibrating = false;
    bool m_digitalCalibrationStarted = false;
    int m_digitalCalibrationMode = 0;
    quint64 m_digitalCalibrationGeneration = 0;
    QString m_digitalCalibrationContext;
    QString m_sstvTransmitContext;
    QMap<QString, float> m_digitalReducedGains;
    QPointer<QLabel> m_digitalSetupStatus;

    // Menu system
    MenuModel *m_menuModel;
    MenuOverlayWidget *m_menuOverlay;
    BandPopupWidget *m_bandPopup;
    DisplayPopupWidget *m_displayPopup;
    // Top-level popups need their own overlay; a MainWindow child cannot
    // appear above a Qt::Popup. Keep the same presentation used by CTRL.
    NotificationWidget *m_displayNotificationWidget = nullptr;
    FnPopupWidget *m_fnPopup;
    MacroDialog *m_macroDialog;
    ButtonRowPopup *m_mainRxPopup;
    ButtonRowPopup *m_subRxPopup;
    ButtonRowPopup *m_txPopup;
    RxEqPopupWidget *m_rxEqPopup;
    RxEqPopupWidget *m_txEqPopup;
    LineOutPopupWidget *m_lineOutPopup;
    LineInPopupWidget *m_lineInPopup;
    MicInputPopupWidget *m_micInputPopup;
    MicConfigPopupWidget *m_micConfigPopup;
    VoxPopupWidget *m_voxPopup;
    SsbBwPopupWidget *m_ssbBwPopup;
    KeyingWeightPopupWidget *m_keyingWeightPopup;
    FmPlPopupWidget *m_fmPlPopup;
    DtmfPopupWidget *m_dtmfPopup;
    TxModePopupWidget *m_txModePopup;
    SoftwareListPopupWidget *m_softwareListPopup;
    TextDecodeWindow *m_textDecodeWindowMain;
    TextDecodeWindow *m_textDecodeWindowSub;
    AntennaCfgPopupWidget *m_mainRxAntCfgPopup;
    AntennaCfgPopupWidget *m_subRxAntCfgPopup;
    AntennaCfgPopupWidget *m_txAntCfgPopup;
    FeatureMenuBar *m_featureMenuBar;
    ModePopupWidget *m_modePopup;

    RadioEntry m_currentRadio;
    TcpClient::ConnectionState m_connectionState = TcpClient::Disconnected;
    // The phone begins each connection with an even local panadapter split.
    // Once the operator changes WTR HEIGHT, the normal radio-backed setting
    // again drives the phone renderer and popup.
    int m_phoneWaterfallHeight = 50;
    bool m_phoneWaterfallHeightAdjusted = false;
    float m_autoRefLevelA = 0.0f;
    float m_autoRefLevelB = 0.0f;
    bool m_autoRefLevelAValid = false;
    bool m_autoRefLevelBValid = false;
    int m_currentBandNum = -1;  // Current band number for VFO A (BN command)
    int m_currentBandNumB = -1; // Current band number for VFO B (BN$ command)

    // KPOD device
    KpodDevice *m_kpodDevice;

    // HaliKey CW paddle device
    HalikeyDevice *m_halikeyDevice;
    // Independent CTR2-MIDI role. It does not replace or reconfigure the
    // established CW Keyer device/session.
    Ctr2MidiDevice *m_ctr2MidiDevice = nullptr;
    MidiInputRouter *m_midiInputRouter = nullptr;
    int m_midiMainVolumeBeforeMute = -1;
    int m_midiWaterfallColorRange = 10;
    QString m_midiSelectedKnobAction = QStringLiteral("active_vfo_frequency");
    // Accumulate a physical pan-zoom turn locally and send one final SPN
    // command after the brief burst. This mirrors a discrete on-screen span
    // change and prevents delayed SPN echoes from becoming the next knob
    // event's starting point.
    int m_midiPanZoomPendingSpanA = 0;
    int m_midiPanZoomPendingSpanB = 0;
    int m_midiPanZoomGenerationA = 0;
    int m_midiPanZoomGenerationB = 0;
    quint64 m_midiPanZoomAnchorFrequencyA = 0;
    quint64 m_midiPanZoomAnchorFrequencyB = 0;
    int m_midiPanZoomAnchorStepAHz = 1000;
    int m_midiPanZoomAnchorStepBHz = 1000;
    IambicKeyer *m_iambicKeyer = nullptr;
    QThread *m_keyerThread = nullptr;

    // Local sidetone generator for CW keying
    SidetoneGenerator *m_sidetoneGenerator;
    QThread *m_sidetoneThread = nullptr;

    // KPA1500 amplifier client
    KPA1500Client *m_kpa1500Client;

    // CAT server for external app integration (WSJT-X, MacLoggerDX, etc.)
    CatServer *m_catServer;

    // Persistent Options dialog (lazy-created on first open)
    OptionsDialog *m_optionsDialog = nullptr;

    // Notification popup for K4 error/status messages (ERxx:)
    NotificationWidget *m_notificationWidget;

    // Debounce timer for RX EQ slider changes
    QTimer *m_rxEqDebounceTimer;

    // The shared Main/Sub RX EQ FLAT control follows the K4 behavior: first
    // tap sets flat; the next tap restores the curve that preceded FLAT.
    QVector<int> m_rxEqBeforeFlat;
    bool m_rxEqFlatActive = false;

    // Debounce timer for TX EQ slider changes
    QTimer *m_txEqDebounceTimer;

    // The K4's TX EQ FLAT control is a toggle: first tap recalls a flat
    // response, second tap restores the curve that was active before FLAT.
    QVector<int> m_txEqBeforeFlat;
    bool m_txEqFlatActive = false;

    // K4 "Mouse L/R Button QSY" menu setting
    int m_mouseQsyMode = 0;      // 0=Left Only, 1=L=A R=B
    int m_mouseQsyMenuId = -999; // Menu ID from MEDF (sentinel = not yet discovered)

    WheelAccumulator m_ritWheelAccumulator;
    bool m_closingTransientMenus = false;
};

#endif // MAINWINDOW_H
