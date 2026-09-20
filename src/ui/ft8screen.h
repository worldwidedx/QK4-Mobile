#pragma once
#include <QWidget>
#include <QSet>
#include "ft8/ft8session.h"
#include "ft8/ft8logbook.h"

class QLabel;
class QPushButton;
class QListWidget;
class QListWidgetItem;
class QProgressBar;
class QTimer;
class QSlider;
class Ft8Waterfall;
class FrequencyDisplayWidget;

class Ft8Screen : public QWidget {
    Q_OBJECT
public:
    enum class DialTarget { RxAudio, TxAudio, RadioFrequency };
    explicit Ft8Screen(QWidget *parent = nullptr, const QString &storageRoot = {});
    ~Ft8Screen() override;
    void setRadioState(bool connected, qint64 frequencyHz, const QString &radioMode, bool transmitting);
    void setRfPower(double watts);
    void setTransmitProtection(const QString &text, bool fault);
    void addDecodes(const QVector<Ft8::Decode> &decodes);
    void addSpectrum(const QVector<float> &db, double firstHz = 100, double binHz = 6.25);
    void setWaterfallAppearance(int palette, int range);
    void setReceiveStatus(const QString &text);
    void suspend();
    void finishLiveTransmit(bool success, const QString &reason);
    void liveTransmitting();
    bool practice() const { return m_practice; }
    bool receiving() const { return m_receiving && !m_practice && isVisible(); }
    Ft8::Mode mode() const { return m_session.mode; }
    const Ft8Session &session() const { return m_session; }
    void setPractice(bool enabled);
    void selectDialTarget(DialTarget target);
    void tuneDial(int steps);
    bool handleMidiKnobAction(const QString &action, int value, bool absolute);
    bool handleMidiButtonAction(const QString &action);
    DialTarget dialTarget() const { return m_dialTarget; }
    qint64 dialStepHz() const { return m_dialTarget == DialTarget::RadioFrequency ? m_rfStepHz : m_audioStepHz; }
    QSize minimumSizeHint() const override { return QSize(280, 480); }
signals:
    void closeRequested();
    void captureChanged();
    void frequencyRequested(qint64 hz);
    void powerRequested(double watts);
    void audioSetupRequested();
    void liveArmRequested();
    void liveTransmitRequested(const QString &message, int mode, int hz, qint64 slotUtc);
    void liveStopRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    friend class Ft8Test;
    void setupUi();
    void refresh();
    void selectStation(const Ft8::Decode &decode, bool call);
    void startCall();
    void stopLiveTransmit();
    void selectBand();
    void enterFrequency();
    void setFrequency(qint64 hz);
    void changeMode();
    void options();
    void rowOptions();
    void dialOptions();
    bool dialAvailable() const;
    void beginDialAdjustment();
    void beginDialAdjustment(DialTarget target);
    void commitDialAdjustment();
    void updateDisplay();
    void updatePowerUi();
    void formatActivityItem(QListWidgetItem *item);
    void updateNewActivity();
    void chooseMessage();
    void chooseStation(const QVector<Ft8::Decode> &decodes);
    void tick();
    void practiceTransmit(const QDateTime &utc);
    void seedPractice();
    void appendDecode(const Ft8::Decode &decode, bool transmitted = false);
    void appendDecodes(const QVector<Ft8::Decode> &decodes, bool transmitted = false);
    void filterActivity();
    void clearActivity();
    void logContact(int editIndex = -1);
    void showLogbook();
    Ft8Logbook &logbook();
    void savePreferences();
    Ft8Session m_session;
    Ft8Logbook m_logbook, m_practiceLog;
    bool m_logReady = true;
    QString m_storageRoot, m_radioMode, m_receiveStatus, m_notice, m_cqDirection;
    QString m_stationCall, m_stationGrid;
    QString m_transmitProtection;
    bool m_transmitProtectionFault = false;
    bool m_connected = false, m_radioTx = false, m_practice = false, m_receiving = true;
    double m_rfPowerWatts = -1.0, m_practicePowerWatts = 25.0;
    bool m_autoLog = false, m_interacting = false, m_ignoreNextClick = false;
    bool m_completionShown = false;
    bool m_livePending = false, m_liveTransmitting = false;
    Ft8::Decode m_liveMessage;
    QVector<Ft8::Decode> m_liveReplies;
    bool m_showWaterfall = true, m_updatingActivity = false;
    int m_rowDensity = 0, m_unreadActivity = 0;
    DialTarget m_dialTarget = DialTarget::RxAudio;
    DialTarget m_lastToneTarget = DialTarget::RxAudio;
    bool m_dialCommitRequired = false, m_dialPreviewActive = false;
    int m_dialPreviewHz = 1500, m_rxFocusHz = 0;
    int m_audioStepHz = 5, m_rfDigit = 2;
    qint64 m_rfStepHz = 100, m_pendingRfHz = 0, m_rfRequestUtc = 0;
    QPoint m_pressPosition;
    quint64 m_practiceGeneration = 0;
    qint64 m_frequency = 14074000, m_radioFrequency = 0, m_lastTxSlot = -1;
    qint64 m_lastAudioUtc = 0;
    int m_filter = 0;
    QSet<QString> m_seen;
    QVector<Ft8::Decode> m_pending, m_recent;
    QLabel *m_banner = nullptr, *m_status = nullptr, *m_partner = nullptr, *m_exchange = nullptr;
    QPushButton *m_band = nullptr, *m_mode = nullptr;
    FrequencyDisplayWidget *m_frequencyDisplay = nullptr;
    QSlider *m_powerSlider = nullptr;
    QLabel *m_powerLabel = nullptr;
    QPushButton *m_rx = nullptr, *m_call = nullptr, *m_cq = nullptr, *m_halt = nullptr;
    QPushButton *m_next = nullptr, *m_auto = nullptr, *m_period = nullptr, *m_offsets = nullptr;
    QPushButton *m_waterfallToggle = nullptr, *m_newActivity = nullptr;
    QVector<QPushButton *> m_zoomControls;
    QListWidget *m_activity = nullptr;
    Ft8Waterfall *m_waterfall = nullptr;
    QProgressBar *m_cycle = nullptr;
    QTimer *m_timer = nullptr;
};
