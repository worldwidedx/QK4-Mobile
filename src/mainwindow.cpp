#include "mainwindow.h"
#include "ui/overlaybackhandler.h"
#include "ui/radiomanagerdialog.h"
#include "ui/sidecontrolpanel.h"
#include "ui/rightsidepanel.h"
#include "ui/bottommenubar.h"
#include "ui/featuremenubar.h"
#include "ui/modepopupwidget.h"
#include "ui/menuoverlay.h"
#include "ui/bandpopupwidget.h"
#include "ui/displaypopupwidget.h"
#include "ui/buttonrowpopup.h"
#include "ui/fnpopupwidget.h"
#include "ui/rxeqpopupwidget.h"
#include "ui/macrodialog.h"
#include "ui/optionsdialog.h"
#include "ui/txmeterwidget.h"
#include "ui/notificationwidget.h"
#include "ui/vforowwidget.h"
#include "ui/filterindicatorwidget.h"
#include "ui/k4styles.h"
#include "ui/inwindowdialog.h"
#include "ui/digitalaudiosetupdialog.h"
#include "ui/inwindowpopup.h"
#include "ui/antennacfgpopup.h"
#include "ui/lineoutpopup.h"
#include "ui/lineinpopup.h"
#include "ui/micinputpopup.h"
#include "ui/micconfigpopup.h"
#include "ui/voxpopup.h"
#include "ui/ssbbwpopup.h"
#include "ui/keyingweightpopup.h"
#include "ui/fmplpopup.h"
#include "ui/dtmfpopup.h"
#include "ui/txmodepopup.h"
#include "ui/softwarelistpopup.h"
#include "ui/dxprefixlistdialog.h"
#include "ui/sstvscreen.h"
#include "ui/ft8screen.h"
#include "ui/logbookdialog.h"
#include "ft8/qrzlogbook.h"
#include "ft8/ft8receiver.h"
#include "ft8/ft8transmitter.h"
#include "ui/textdecodewindow.h"
#include "ui/frequencydisplaywidget.h"
#include "ui/frequencyentryparser.h"
#include "models/menumodel.h"
#include "dsp/panadapter_rhi.h"
#include "dsp/minipan_rhi.h"
#include "audio/audioengine.h"
#include "audio/opusdecoder.h"
#include "audio/sidetonegenerator.h"
#include "android/sstvorientation.h"
#include "sstv/sstvdecoder.h"
#include "hardware/kpoddevice.h"
#include "hardware/halikeydevice.h"
#include "hardware/ctr2mididevice.h"
#include "hardware/midiinputrouter.h"
#include "hardware/midimapping.h"
#include "hardware/iambickeyer.h"
#include "network/kpa1500client.h"
#include "ui/kpa1500window.h"
#include "ui/kpa1500panel.h"
#include "network/catserver.h"
#include "settings/radiosettings.h"
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QAction>
#include <QCoreApplication>
#include <QDebug>
#include <QFont>
#include <QDateTime>
#include <QPainter>
#include <QFrame>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <QScrollerProperties>
#include <QTabWidget>
#include <QEvent>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QMouseEvent>
#include <algorithm>
#include <QShowEvent>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QVector>
#include <cmath>
#ifdef Q_OS_ANDROID
#include <QPermissions>
#endif

// K4 Span range: 5 kHz to 368 kHz
// UP (zoom out): +1 kHz until 144, then +4 kHz until 368
// DOWN (zoom in): -4 kHz until 140, then -1 kHz until 5
static constexpr int SPAN_MIN = 5000;
static constexpr int SPAN_MAX = 368000;
static constexpr int SPAN_THRESHOLD_UP = 144000;   // Switch to 4kHz steps above this
static constexpr int SPAN_THRESHOLD_DOWN = 140000; // Switch to 1kHz steps below this

namespace {
// Phone-sized version of current QK4's NetHealthWidget. These bars describe
// the complete K4 streaming link (RTT, jitter, audio continuity and buffered
// latency), not merely the handset's Wi-Fi RSSI.
class PhoneNetHealthWidget final : public QWidget {
public:
    explicit PhoneNetHealthWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setFixedSize(22, 16);
        setToolTip("K4 network health");
        m_summary.setInterval(2000);
        connect(&m_summary, &QTimer::timeout, this, [this]() { summarize(); });
    }

    void setConnected(bool connected) {
        m_connected = connected;
        m_lastSequence = -1;
        m_audioActive = m_audioWasActive = false;
        m_stalePings = 0;
        m_rttSamples.clear();
        m_tier = connected ? 0 : 3;
        connected ? m_summary.start() : m_summary.stop();
        refreshToolTip();
        update();
    }

    void setLatency(int milliseconds) {
        m_rtt = milliseconds;
        m_stalePings = 0;
        m_rttSamples.append(milliseconds);
        while (m_rttSamples.size() > 30)
            m_rttSamples.removeFirst();
    }

    void setAudioSequence(quint8 sequence) {
        m_audioActive = true;
        if (m_lastSequence >= 0) {
            const int expected = (m_lastSequence + 1) & 0xff;
            const int gap = (static_cast<int>(sequence) - expected) & 0xff;
            if (gap > 0 && gap < 128)
                m_lostPackets += gap;
        }
        m_lastSequence = sequence;
    }

    void setBufferStatus(int bytes, int, bool) { m_bufferMs = bytes / 96.0; }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        const QColor dim(K4Styles::Colors::InactiveGray);
        const QColor colors[] = {QColor(K4Styles::Colors::StatusGreen), QColor(K4Styles::Colors::MeterYellow),
                                 QColor(K4Styles::Colors::MeterOrange), QColor(K4Styles::Colors::TxRed)};
        const int lit = m_connected ? 4 - m_tier : 0;
        for (int i = 0; i < 4; ++i) {
            const int heightPx = 4 + i * 4;
            painter.fillRect(1 + i * 5, height() - heightPx, 3, heightPx, i < lit ? colors[m_tier] : dim);
        }
    }

private:
    void summarize() {
        if (!m_connected)
            return;
        double jitter = 0.0;
        if (!m_rttSamples.isEmpty()) {
            double mean = 0.0;
            for (int sample : m_rttSamples)
                mean += sample;
            mean /= m_rttSamples.size();
            for (int sample : m_rttSamples)
                jitter += (sample - mean) * (sample - mean);
            jitter = std::sqrt(jitter / m_rttSamples.size());
        }
        int tier = 0;
        if (m_rtt > 200) tier = 3;
        else if (m_rtt > 100) tier = 2;
        else if (m_rtt > 50) tier = 1;
        if (jitter > 50.0) tier = qMax(tier, 2);
        else if (jitter > 20.0) tier = qMax(tier, 1);
        if (m_bufferMs > 500.0) tier = qMax(tier, 2);
        else if (m_bufferMs > 200.0) tier = qMax(tier, 1);
        if (m_audioWasActive && !m_audioActive) tier = qMax(tier, 2);
        if (m_stalePings >= 2) tier = 3;
        else if (m_stalePings >= 1) tier = qMax(tier, 2);
        m_tier = tier;
        ++m_stalePings;
        if (m_audioActive) m_audioWasActive = true;
        m_audioActive = false;
        refreshToolTip(jitter);
        update();
    }

    void refreshToolTip(double jitter = 0.0) {
        setToolTip(m_connected
                       ? QString("K4 link: %1 ms RTT, %2 ms jitter, %3 ms audio buffer, %4 lost packets")
                             .arg(m_rtt).arg(jitter, 0, 'f', 1).arg(m_bufferMs, 0, 'f', 0).arg(m_lostPackets)
                       : QString("K4 link disconnected"));
    }

    QTimer m_summary;
    QVector<int> m_rttSamples;
    bool m_connected = false;
    bool m_audioActive = false;
    bool m_audioWasActive = false;
    int m_tier = 3;
    int m_rtt = -1;
    int m_stalePings = 0;
    int m_lastSequence = -1;
    int m_lostPackets = 0;
    double m_bufferMs = 0.0;
};

// Full-window input shield used only while the phone's deliberate TX latch is
// active.  It consumes touch, mouse, and wheel events before they reach radio
// controls beneath it.  The separate TX/RX proxy button remains available as
// the sole in-app route back to receive.
class PhoneTxInputShield final : public QWidget {
public:
    explicit PhoneTxInputShield(QWidget *parent = nullptr) : QWidget(parent) { setFocusPolicy(Qt::NoFocus); }

protected:
    bool event(QEvent *event) override {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseMove:
        case QEvent::Wheel:
            event->accept();
            return true;
        default:
            return QWidget::event(event);
        }
    }
};

QString temperatureStyle(int celsius) {
    const QString color = celsius >= 75 ? K4Styles::Colors::TxRed
                           : celsius >= 60 ? K4Styles::Colors::MeterOrange
                                           : K4Styles::Colors::AccentAmber;
    return QString("color: %1; font-size: %2px;").arg(color).arg(K4Styles::Dimensions::FontSizeButton);
}

// The K4 TX command was already proven to key the radio.  Hold program audio
// briefly so the SSTV leader does not arrive while the RF/audio path is still
// changing over; do not make transmission depend on a CAT state echo.
constexpr int SstvKeyUpGuardMs = 500;
constexpr int SstvDrainMarginMs = 150;
} // namespace

// Convert K4 tuning step index (VT command, 0-5) to Hz
static int tuningStepToHz(int step) {
    static const int table[] = {1, 10, 100, 1000, 10000, 100};
    return (step >= 0 && step <= 5) ? table[step] : 1000;
}

static int streamingLatencyToFrameSamples(int tier) {
    if (tier <= 0)
        return 240;
    if (tier <= 2)
        return 480;
    if (tier <= 5)
        return 720;
    return 1440;
}

static int getNextSpanUp(int currentSpan) {
    if (currentSpan >= SPAN_MAX)
        return SPAN_MAX;
    int increment = (currentSpan < SPAN_THRESHOLD_UP) ? 1000 : 4000;
    int newSpan = currentSpan + increment;
    return qMin(newSpan, SPAN_MAX);
}

static int getNextSpanDown(int currentSpan) {
    if (currentSpan <= SPAN_MIN)
        return SPAN_MIN;
    int decrement = (currentSpan > SPAN_THRESHOLD_DOWN) ? 4000 : 1000;
    int newSpan = currentSpan - decrement;
    return qMax(newSpan, SPAN_MIN);
}

#ifdef Q_OS_ANDROID
static bool ensureMicrophonePermission(QWidget *parent) {
    QMicrophonePermission permission;
    Qt::PermissionStatus status = qApp->checkPermission(permission);

    if (status == Qt::PermissionStatus::Granted) {
        return true;
    }

    if (status == Qt::PermissionStatus::Undetermined) {
        QPointer<QWidget> safeParent(parent);
        qApp->requestPermission(permission, parent, [safeParent](const QPermission &result) {
            if (result.status() == Qt::PermissionStatus::Denied && safeParent) {
                showInWindowMessage(safeParent, "Microphone Permission Required",
                                    "Grant microphone permission to transmit audio.");
            }
        });
        return false;
    }

    showInWindowMessage(parent, "Microphone Permission Required",
                        "Microphone permission is currently denied. Enable it in Android Settings to transmit audio.");
    return false;
}
#endif

// ============== MainWindow Implementation ==============
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_tcpClient(new TcpClient(nullptr)), m_radioState(new RadioState(this)),
      m_clockTimer(new QTimer(this)), m_audioEngine(new AudioEngine(nullptr)), m_opusDecoder(new OpusDecoder(nullptr)),
      m_menuModel(new MenuModel(this)), m_menuOverlay(nullptr) {
    QrzLogbook::start(LogbookUi::storagePath(), this);
    m_audioEngine->setDigitalTxControl(m_tcpClient->digitalTxControl());
    m_ft8Transmitter = new Ft8Transmitter(m_tcpClient->digitalTxControl(), m_audioEngine);
    connect(m_ft8Transmitter, &Ft8Transmitter::keyRequested, m_tcpClient, &TcpClient::beginScheduledDigitalAudio);
    connect(m_ft8Transmitter, &Ft8Transmitter::unkeyRequested, m_tcpClient, &TcpClient::stopScheduledDigitalAudio);
    connect(m_tcpClient, &TcpClient::digitalAudioKeyRequested, m_ft8Transmitter, &Ft8Transmitter::keyed);
    connect(m_tcpClient, &TcpClient::sstvAudioAccepted, m_ft8Transmitter, &Ft8Transmitter::accepted);
    connect(m_tcpClient, &TcpClient::scheduledDigitalAudioStopped, m_ft8Transmitter, &Ft8Transmitter::unkeyed);
    connect(m_ft8Transmitter, &Ft8Transmitter::encodingStarted, m_audioEngine, &AudioEngine::beginFt8Encoding);
    connect(m_ft8Transmitter, &Ft8Transmitter::frameReady, m_audioEngine, &AudioEngine::encodeFt8Frame);
    connect(m_ft8Transmitter, &Ft8Transmitter::finished, m_audioEngine,
            [this](bool, const QString &, quint64 gen) { m_audioEngine->finishFt8Encoding(gen); });
    connect(m_ft8Transmitter, &Ft8Transmitter::transmitting, this, [this](quint64 gen) {
        if (gen == m_ft8TxGeneration && m_ft8Screen) m_ft8Screen->liveTransmitting();
    });
    connect(m_ft8Transmitter, &Ft8Transmitter::finished, this,
            [this](bool success, const QString &reason, quint64 gen) {
        if (gen != m_ft8TxGeneration) return;
        auto expected = gen;
        m_tcpClient->digitalTxControl()->scheduledGeneration.compare_exchange_strong(expected, 0);
        m_ft8TxGeneration = 0;
        if (m_ft8Screen) m_ft8Screen->finishLiveTransmit(success, reason);
        refreshFt8Capture();
        qInfo() << "FT TX finished" << gen << success << reason;
    });
    connect(m_tcpClient, &TcpClient::digitalAudioTransmitFailed, m_ft8Transmitter,
            [this](int, const QString &, quint64 gen) { m_ft8Transmitter->cancel(gen); });
    auto *ft8SettingsGuard = new QTimer(this);
    ft8SettingsGuard->setInterval(100);
    connect(ft8SettingsGuard, &QTimer::timeout, this, [this] {
        updateFt8RadioMode();
        if (!m_ft8TxGeneration) return;
        if (!m_tcpClient->isConnected() || m_radioState->splitEnabled() || m_radioState->testMode()
            || qint64(m_radioState->vfoA()) != m_ft8TransmitFrequency
            || DigitalTxCalibration::matchingContext(digitalCalibrationContext(int(m_ft8Screen->mode())))
                != DigitalTxCalibration::matchingContext(m_ft8TransmitContext)) {
            stopFt8Transmit();
            m_ft8Screen->setTransmitProtection("TX halted: radio settings changed. Check TX audio setup.", true);
        }
    });
    ft8SettingsGuard->start();
    connect(m_tcpClient, &TcpClient::disconnected, this, &MainWindow::stopFt8Transmit);
    connect(m_tcpClient, &TcpClient::disconnected, this, [this] { m_ft8RadioMode.connectionLost(); });
    connect(m_radioState, &RadioState::transmitStateChanged, this, [this](bool transmitting) {
        if (!transmitting && m_ft8TxGeneration && m_tcpClient->digitalTxControl()->allows(m_ft8TxGeneration))
            stopFt8Transmit();
    });
    connect(m_tcpClient, &TcpClient::digitalAudioDriveReduced, this,
            [this](int mode, float gain, quint64 generation) {
        if (!m_digitalCalibrating && mode == int(DigitalTxGuard::Mode::Sstv)
            && generation == m_sstvGeneration && !m_sstvTransmitContext.isEmpty())
            m_digitalReducedGains.insert(DigitalTxCalibration::matchingContext(m_sstvTransmitContext), gain);
        if (generation == m_ft8TxGeneration && !m_ft8TransmitContext.isEmpty())
            m_digitalReducedGains.insert(DigitalTxCalibration::matchingContext(m_ft8TransmitContext), gain);
    });
    connect(m_audioEngine, &AudioEngine::digitalCalibrationPreparationFailed, this,
            [this](const QString &reason, quint64 generation) {
        if (!m_digitalCalibrating || generation != m_digitalCalibrationGeneration) return;
        cancelDigitalCalibration();
        showDigitalProtection(m_digitalCalibrationMode, reason, true);
    });
    connect(m_tcpClient, &TcpClient::digitalTxProtectionStatus, this,
            [this](int mode, const QString &text, bool fault, quint64 generation) {
        if ((m_digitalCalibrating && generation == m_digitalCalibrationGeneration)
            || (mode == int(DigitalTxGuard::Mode::Sstv) && generation == m_sstvGeneration))
            showDigitalProtection(mode, text, fault);
        else if (generation == m_ft8TxGeneration && m_ft8Screen)
            m_ft8Screen->setTransmitProtection(text, fault);
    });
    connect(m_audioEngine, &AudioEngine::digitalCalibrationPrepared, this, [this](quint64 generation) {
        if (m_digitalCalibrating && generation == m_digitalCalibrationGeneration) {
            m_digitalCalibrationStarted = true;
            m_tcpClient->beginDigitalCalibration(m_digitalCalibrationMode, generation);
        }
    });
    connect(m_tcpClient, &TcpClient::digitalAudioKeyRequested, this, [this](int, quint64 generation) {
        if (!m_digitalCalibrating || generation != m_digitalCalibrationGeneration) return;
        QTimer::singleShot(SstvKeyUpGuardMs, this, [this, generation] {
            if (m_digitalCalibrating && generation == m_digitalCalibrationGeneration)
                QMetaObject::invokeMethod(m_audioEngine, "beginSstvTransmit", Qt::QueuedConnection);
        });
    });
    connect(m_tcpClient, &TcpClient::digitalCalibrationFinished, this,
            [this](int mode, bool success, float gain, QString text, quint64 generation) {
        if (!m_digitalCalibrating || generation != m_digitalCalibrationGeneration) return;
        m_audioEngine->requestSstvStop();
        QMetaObject::invokeMethod(m_audioEngine, "stopSstvTransmit", Qt::QueuedConnection);
        if (success && DigitalTxCalibration::matchingContext(digitalCalibrationContext(mode))
                != DigitalTxCalibration::matchingContext(m_digitalCalibrationContext)) {
            success = false;
            text = "Calibration not saved: radio settings changed. Previous saved level retained.";
        }
        if (success) {
            QSettings settings("QK4", "QK4");
            if (!DigitalTxCalibration::save(settings, m_digitalCalibrationContext, gain)) {
                success = false;
                text = "Calibration finished but could not be saved. Check app storage.";
            } else
                m_digitalReducedGains.insert(DigitalTxCalibration::matchingContext(m_digitalCalibrationContext), gain);
        }
        m_digitalCalibrating = false;
        qInfo() << "Digital TX calibration result" << success << "gain" << gain << text;
        showDigitalProtection(mode, text, !success);
        refreshFt8Capture();
        if (m_sstvScreen && m_sstvScreen->isVisible()) {
            m_sstvRxArmed.store(true, std::memory_order_release);
            QMetaObject::invokeMethod(m_sstvDecoder, "resetAuto", Qt::QueuedConnection);
        }
    });
    // Initialize Opus decoder (K4 sends 12kHz stereo: left=Main, right=Sub)
    m_opusDecoder->initialize(12000, 2);

    // Load saved audio device settings BEFORE moveToThread (only stores strings/floats,
    // no Qt audio objects exist yet, so direct calls are safe).
    // Android deliberately follows the operating system's active route rather
    // than a persisted desktop device id: USB-C and Bluetooth endpoints are
    // dynamic and Android is the authority for their microphone capability.
#ifndef Q_OS_ANDROID
    QString savedMicDevice = RadioSettings::instance()->micDevice();
    if (!savedMicDevice.isEmpty()) {
        m_audioEngine->setMicDevice(savedMicDevice);
    }
    QString savedSpeakerDevice = RadioSettings::instance()->speakerDevice();
    if (!savedSpeakerDevice.isEmpty()) {
        m_audioEngine->setOutputDevice(savedSpeakerDevice);
    }
#endif
    m_audioEngine->setMicGain(RadioSettings::instance()->micGain() / 100.0f);

    // Move AudioEngine to dedicated thread for glitch-free audio playback
    m_audioThread = new QThread(this);
    m_audioThread->setObjectName("AudioEngine");
    m_audioEngine->moveToThread(m_audioThread);
    m_audioThread->start();

    m_sstvDecoder = new SstvDecoder(nullptr);
    m_sstvDecoderThread = new QThread(this);
    m_sstvDecoderThread->setObjectName("SSTV Decoder");
    m_sstvDecoder->moveToThread(m_sstvDecoderThread);
    m_sstvDecoderThread->start();

    qRegisterMetaType<Ft8::Decode>();
    qRegisterMetaType<QVector<Ft8::Decode>>();
    m_ft8Receiver = new Ft8Receiver;
    m_ft8Thread = new QThread(this);
    m_ft8Thread->setObjectName("FT8 FT4 Receiver");
    m_ft8Receiver->moveToThread(m_ft8Thread);
    connect(m_ft8Thread, &QThread::finished, m_ft8Receiver, &QObject::deleteLater);
    m_ft8Thread->start();

    // Move TcpClient (+ Protocol, socket, timers as children) to dedicated I/O thread
    // so network data flows independently of UI work
    m_ioThread = new QThread(this);
    m_ioThread->setObjectName("I/O");
    m_tcpClient->moveToThread(m_ioThread);
    m_ioThread->start();

    // IMPORTANT: setupUi() MUST be called BEFORE setupMenuBar()!
    // Qt 6.10.1 bug on macOS Tahoe: calling menuBar() before creating QRhiWidget
    // prevents the RHI backing store from being set up correctly, causing
    // "QRhiWidget: No QRhi" errors and blank panadapter display.
    setupUi();
    setupMenuBar();

    connect(qApp, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive && m_sstvScreen)
            m_sstvScreen->flushDraft();
        if (state != Qt::ApplicationActive && (m_sstvTxActive || m_sstvTxStarting))
            stopSstvTransmission();
        if (state != Qt::ApplicationActive)
            cancelDigitalCalibration();
        if (m_ft8Screen) {
            if (state != Qt::ApplicationActive) m_ft8Screen->suspend();
            refreshFt8Capture();
        }
    });

    // Phone replacement for hovering over RIT/XIT and rolling a mouse wheel.
    // A normal tap still toggles the label; a deliberate hold opens the
    // relative offset jog control without changing the toggle state first.
    m_ritXitLongPressTimer = new QTimer(this);
    m_ritXitLongPressTimer->setSingleShot(true);
    m_ritXitLongPressTimer->setInterval(550);
    connect(m_ritXitLongPressTimer, &QTimer::timeout, this, [this]() {
        if (!m_ritXitPressTarget)
            return;
        const bool ritActive = m_radioState->bSetEnabled()
            ? m_radioState->ritEnabledB() : m_radioState->ritEnabled();
        // Pressing the offset value/border selects the sole active control,
        // or RIT by default when both controls are active.
        const bool preferXit = (m_ritXitPressTarget == m_xitLabel)
            || (m_ritXitPressTarget != m_ritLabel && m_radioState->xitEnabled() && !ritActive);
        const bool requestedActive = preferXit
            ? m_radioState->xitEnabled()
            : ritActive;
        m_ritXitLongPressHandled = true;
        if (requestedActive)
            showRitXitAdjustment(preferXit);
        else
            showControlFeedback(preferXit ? "Enable XIT before adjusting" : "Enable RIT before adjusting");
    });

    // Menu items are populated from MEDF responses in onCatResponse()
    // when the radio sends RDY; after connection

    // Create menu overlay (positioned over spectrum container)
    m_menuOverlay = new MenuOverlayWidget(m_menuModel, this);
    m_menuOverlay->hide();

    // Connect menu overlay signals
    connect(m_menuOverlay, &MenuOverlayWidget::menuValueChangeRequested, this, &MainWindow::onMenuValueChangeRequested);
    connect(m_menuOverlay, &MenuOverlayWidget::aboutRequested, this, &MainWindow::showAboutDialog);
    connect(m_menuOverlay, &MenuOverlayWidget::closed, this, [this]() {
        if (m_bottomMenuBar) {
            m_bottomMenuBar->setMenuActive(false);
        }
    });

    // Connect menu model value changes for display settings
    connect(m_menuModel, &MenuModel::menuValueChanged, this, &MainWindow::onMenuModelValueChanged);

    // Also check initial values when menu items are first loaded from MEDF
    connect(m_menuModel, &MenuModel::menuItemAdded, this, [this](int menuId) {
        const MenuItem *item = m_menuModel->getMenuItem(menuId);
        if (item && item->name == "Spectrum Amplitude Units") {
            bool useSUnits = (item->currentValue == 1);
            qDebug() << "Initial spectrum amplitude units:" << (useSUnits ? "S-UNITS" : "dBm");
            if (m_panadapterA) {
                m_panadapterA->setAmplitudeUnits(useSUnits);
            }
            if (m_panadapterB) {
                m_panadapterB->setAmplitudeUnits(useSUnits);
            }
        }
        if (item && item->name == "Mouse L/R Button QSY") {
            m_mouseQsyMenuId = item->id;
            m_mouseQsyMode = item->currentValue;
            qDebug() << "Mouse L/R Button QSY: menuId=" << m_mouseQsyMenuId << "mode=" << m_mouseQsyMode;
        }
    });

    // Create band selection popup
    m_bandPopup = new BandPopupWidget(this);
    connect(m_bandPopup, &BandPopupWidget::bandSelected, this, &MainWindow::onBandSelected);
    connect(m_radioState, &RadioState::frequencyChanged, this,
            [this](quint64 frequencyHz) { m_bandPopup->rememberVfoFrequency(false, frequencyHz); });
    connect(m_radioState, &RadioState::frequencyBChanged, this,
            [this](quint64 frequencyHz) { m_bandPopup->rememberVfoFrequency(true, frequencyHz); });
    connect(m_bandPopup, &BandPopupWidget::bandBankChanged, this, [this]() {
        // GEN changes the popup's geometry (2 x 7 amateur grid / 3 x 5 SWL
        // grid); re-position it above BAND after the size change.
        if (m_bottomMenuBar && m_bandPopup->isVisible())
            m_bandPopup->showAboveButton(m_bottomMenuBar->bandButton());
    });
    connect(m_bandPopup, &BandPopupWidget::closed, this, [this]() {
        if (m_bottomMenuBar) {
            m_bottomMenuBar->setBandActive(false);
        }
    });

    // Create display popup
    m_displayPopup = new DisplayPopupWidget(this);
    // DISP is a top-level Qt popup, so feedback owned by MainWindow would sit
    // behind it. Use the same NotificationWidget treatment as the CTRL drawer.
    m_displayNotificationWidget = new NotificationWidget(m_displayPopup);
    connect(m_displayPopup, &DisplayPopupWidget::closed, this, [this]() {
        if (m_bottomMenuBar) {
            m_bottomMenuBar->setDisplayActive(false);
        }
    });
    // DisplayPopup pan mode changed -> update panadapter display
    // (K4 doesn't echo #DPM commands, so DisplayPopup notifies us directly)
    connect(m_displayPopup, &DisplayPopupWidget::dualPanModeChanged, this, [this](int mode) {
        switch (mode) {
        case 0: // A only
            setPanadapterMode(PanadapterMode::MainOnly);
            break;
        case 1: // B only
            setPanadapterMode(PanadapterMode::SubOnly);
            break;
        case 2: // Dual (A+B)
            setPanadapterMode(PanadapterMode::Dual);
            break;
        }
    });

    // DisplayPopup CAT commands -> TcpClient
    connect(m_displayPopup, &DisplayPopupWidget::catCommandRequested, m_tcpClient, &TcpClient::sendCAT);
    connect(m_displayPopup, &DisplayPopupWidget::panadapterCentered, this,
            [this]() { showControlFeedback("Panadapter Centered"); });
    connect(m_displayPopup, &DisplayPopupWidget::waterfallColorLocallyChanged, this, [this](int color) {
        m_panadapterA->setWaterfallColor(color);
        m_panadapterB->setWaterfallColor(color);
        static const char *names[] = {"WTR GRAY", "WTR COLOR", "WTR TEAL", "WTR BLUE", "WTR SEPIA"};
        showControlFeedback(QString("WATERFALL: %1").arg((color >= 0 && color <= 4) ? names[color] : "COLOR"));
    });
    connect(m_displayPopup, &DisplayPopupWidget::waterfallColorRangeLocallyChanged, this, [this](int range) {
        m_midiWaterfallColorRange = range;
        m_panadapterA->setWaterfallColorRange(range);
        m_panadapterB->setWaterfallColorRange(range);
    });

    // Create Fn popup with dual-action buttons (macro system)
    m_fnPopup = new FnPopupWidget(this);
    connect(m_fnPopup, &FnPopupWidget::closed, this, [this]() {
        if (m_bottomMenuBar) {
            m_bottomMenuBar->setFnActive(false);
        }
    });
    connect(m_fnPopup, &FnPopupWidget::functionTriggered, this, &MainWindow::onFnFunctionTriggered);

    // Create macro configuration dialog (full-screen overlay)
    m_macroDialog = new MacroDialog(this);
    m_macroDialog->hide();

    // Create button row popups for MAIN RX, SUB RX, TX

    m_mainRxPopup = new ButtonRowPopup(this);
    // Set button labels: primary (white), alternate (amber if has right-click function, white otherwise)
    m_mainRxPopup->setButtonLabel(0, "ANT", "CFG", false);     // No alternate function - all white
    m_mainRxPopup->setButtonLabel(1, "RX", "EQ", false);       // No alternate function - all white
    m_mainRxPopup->setButtonLabel(2, "LINE OUT", "VFO LINK");  // Right-click toggles VFO LINK
    m_mainRxPopup->setButtonLabel(3, "AFX OFF", "OFF");        // Right-click same as left
    m_mainRxPopup->setButtonLabel(4, "AGC-S", "ON");           // Right-click toggles AGC on/off
    m_mainRxPopup->setButtonLabel(5, "APF", "OFF");            // No alternate function but shows state
    m_mainRxPopup->setButtonLabel(6, "TEXT", "DECODE", false); // No alternate function - all white
    connect(m_mainRxPopup, &ButtonRowPopup::closed, this, [this]() {
        if (m_bottomMenuBar) {
            m_bottomMenuBar->setMainRxActive(false);
        }
    });
    connect(m_mainRxPopup, &ButtonRowPopup::buttonClicked, this, &MainWindow::onMainRxButtonClicked);
    connect(m_mainRxPopup, &ButtonRowPopup::buttonRightClicked, this, &MainWindow::onMainRxButtonRightClicked);

    m_subRxPopup = new ButtonRowPopup(this);
    // Set button labels: primary (white), alternate (amber if has right-click function, white otherwise)
    m_subRxPopup->setButtonLabel(0, "ANT", "CFG", false);     // No alternate function - all white
    m_subRxPopup->setButtonLabel(1, "RX", "EQ", false);       // No alternate function - all white
    m_subRxPopup->setButtonLabel(2, "LINE OUT", "VFO LINK");  // Right-click toggles VFO LINK
    m_subRxPopup->setButtonLabel(3, "AFX OFF", "OFF");        // Right-click same as left
    m_subRxPopup->setButtonLabel(4, "AGC-S", "ON");           // Right-click toggles AGC on/off
    m_subRxPopup->setButtonLabel(5, "APF", "OFF");            // No alternate function but shows state
    m_subRxPopup->setButtonLabel(6, "TEXT", "DECODE", false); // No alternate function - all white
    connect(m_subRxPopup, &ButtonRowPopup::closed, this, [this]() {
        if (m_bottomMenuBar) {
            m_bottomMenuBar->setSubRxActive(false);
        }
    });
    connect(m_subRxPopup, &ButtonRowPopup::buttonClicked, this, &MainWindow::onSubRxButtonClicked);
    connect(m_subRxPopup, &ButtonRowPopup::buttonRightClicked, this, &MainWindow::onSubRxButtonRightClicked);

    m_txPopup = new ButtonRowPopup(this);
    m_txPopup->setButtonLabel(0, "ANT", "CFG", false);        // TX Antenna config
    m_txPopup->setButtonLabel(1, "TX", "EQ", false);          // TX Equalizer (future)
    m_txPopup->setButtonLabel(2, "LINE", "IN", false);        // LINE IN control
    m_txPopup->setButtonLabel(3, "MIC INP", "MIC CFG", true); // Mic input/config
    m_txPopup->setButtonLabel(4, "VOX GN", "ANTIVOX", true);  // VOX Gain / Anti-VOX
    m_txPopup->setButtonLabel(5, "SSB BW", "2.8k", false);    // SSB TX Bandwidth
    m_txPopup->setButtonLabel(6, "ESSB", "OFF", false);       // ESSB toggle
    connect(m_txPopup, &ButtonRowPopup::closed, this, [this]() {
        if (m_bottomMenuBar) {
            m_bottomMenuBar->setTxActive(false);
        }
    });
    connect(m_txPopup, &ButtonRowPopup::buttonClicked, this, [this](int index) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        switch (index) {
        case 0: // ANT CFG - show TX antenna config popup
            if (m_txAntCfgPopup && m_txPopup) {
                closeSecondaryPopups();
                m_txAntCfgPopup->showAboveWidget(m_txPopup);
            }
            break;
        case 1: // TX EQ - show TX graphic equalizer popup
            if (m_txEqPopup && m_txPopup) {
                closeSecondaryPopups();
                m_txEqPopup->setAllBands(m_radioState->txEqBands());
                m_txEqPopup->showAboveWidget(m_txPopup);
            }
            break;
        case 2: // LINE IN - show line in control popup
            if (m_lineInPopup && m_txPopup) {
                closeSecondaryPopups();
                m_lineInPopup->setSoundCardLevel(m_radioState->lineInSoundCard());
                m_lineInPopup->setLineInJackLevel(m_radioState->lineInJack());
                m_lineInPopup->setSource(m_radioState->lineInSource());
                m_lineInPopup->showAboveWidget(m_txPopup);
            }
            break;
        case 3: // MIC INP - show mic input selection popup
            if (m_micInputPopup && m_txPopup) {
                closeSecondaryPopups();
                m_micInputPopup->setCurrentInput(m_radioState->micInput());
                m_micInputPopup->showAboveWidget(m_txPopup);
            }
            break;
        case 4: // VOX GN - show VOX Gain popup
            if (m_voxPopup && m_txPopup) {
                closeSecondaryPopups();
                const RadioState::Mode txMode = txOperatingMode();
                const bool isDataMode = txMode == RadioState::DATA || txMode == RadioState::DATA_R;
                m_voxPopup->setPopupMode(VoxPopupWidget::VoxGain);
                m_voxPopup->setDataMode(isDataMode);
                m_voxPopup->setValue(isDataMode ? m_radioState->voxGainData() : m_radioState->voxGainVoice());
                m_voxPopup->setVoxEnabled(txMode == RadioState::CW || txMode == RadioState::CW_R
                                              ? m_radioState->voxCW()
                                              : (isDataMode ? m_radioState->voxData()
                                                            : m_radioState->voxVoice()));
                m_voxPopup->showAboveWidget(m_txPopup);
            }
            break;
        case 5: {
            const auto mode = txOperatingMode();
            const int dataSubMode = m_radioState->splitEnabled()
                                        ? m_radioState->dataSubModeB()
                                        : m_radioState->dataSubMode();
            const bool usesKeyer = mode == RadioState::CW || mode == RadioState::CW_R
                                   || ((mode == RadioState::DATA || mode == RadioState::DATA_R)
                                       && (dataSubMode == 2 || dataSubMode == 3));
            if (usesKeyer) {
                const QChar iambic = m_radioState->iambicMode();
                const QChar paddle = m_radioState->paddleOrientation() == 'R' ? QChar('R') : QChar('N');
                const QChar nextPaddle = paddle == 'R' ? QChar('N') : QChar('R');
                const QString command = QString("KP%1%2%3;").arg(iambic).arg(nextPaddle)
                                            .arg(m_radioState->keyingWeight(), 3, 10, QChar('0'));
                m_tcpClient->sendCAT(command);
                m_radioState->parseCATCommand(command);
                refreshTxPopupForMode();
            } else if ((mode == RadioState::LSB || mode == RadioState::USB) && m_ssbBwPopup && m_txPopup) {
                closeSecondaryPopups();
                m_ssbBwPopup->setEssbEnabled(m_radioState->essbEnabled());
                int bw = m_radioState->ssbTxBw();
                if (bw >= 24 && bw <= 45) {
                    m_ssbBwPopup->setBandwidth(bw);
                }
                m_ssbBwPopup->showAboveWidget(m_txPopup);
            } else if ((mode == RadioState::DATA || mode == RadioState::DATA_R)
                       && (dataSubMode == 0 || dataSubMode == 1)) {
                closeAllPopups();
                showInWindowMessage(centralWidget(), "DATA BW", "Implementation in progress on the K4.");
            } else if (mode == RadioState::FM) {
                closeSecondaryPopups();
                m_txModePopup->showFmRepeater(m_radioState->repeaterMode(),
                                              m_radioState->repeaterOffsetKhz(), false);
                m_txModePopup->showAboveWidget(m_txPopup);
            } else if (mode == RadioState::AM) {
                closeAllPopups();
                showInWindowMessage(centralWidget(), "AM BW",
                                    "AM TX bandwidth is not exposed by the documented K4 remote protocol.");
            }
            break;
        }
        case 6: {
            const auto mode = txOperatingMode();
            const int dataSubMode = m_radioState->splitEnabled()
                                        ? m_radioState->dataSubModeB()
                                        : m_radioState->dataSubMode();
            const bool usesKeyer = mode == RadioState::CW || mode == RadioState::CW_R
                                   || ((mode == RadioState::DATA || mode == RadioState::DATA_R)
                                       && (dataSubMode == 2 || dataSubMode == 3));
            if (usesKeyer) {
                if (m_keyingWeightPopup && m_txPopup) {
                    closeSecondaryPopups();
                    m_keyingWeightPopup->setWeight(m_radioState->keyingWeight());
                    m_keyingWeightPopup->showAboveWidget(m_txPopup);
                }
                break;
            }
            if (mode == RadioState::FM) {
                closeSecondaryPopups();
                const bool txB = m_radioState->splitEnabled();
                m_fmPlPopup->setTone(txB ? m_radioState->plToneIndexB() : m_radioState->plToneIndex(),
                                     txB ? m_radioState->plToneEnabledB() : m_radioState->plToneEnabled());
                m_fmPlPopup->showAboveWidget(m_txPopup);
                break;
            }
            if (mode != RadioState::LSB && mode != RadioState::USB)
                break;
            bool newState = !m_radioState->essbEnabled();
            int bw = m_radioState->ssbTxBw();
            // Ensure bw is valid for the new mode
            // SSB: 24-28, ESSB: 30-45
            if (newState) {
                // Switching to ESSB - use 30 if bw is outside ESSB range
                if (bw < 30 || bw > 45)
                    bw = 30;
            } else {
                // Switching to SSB - use 28 if bw is outside SSB range
                if (bw < 24 || bw > 28)
                    bw = 28;
            }
            m_tcpClient->sendCAT(QString("ES%1%2;").arg(newState ? 1 : 0).arg(bw, 2, 10, QChar('0')));
            // Optimistic update
            m_radioState->setEssbEnabled(newState);
            m_radioState->setSsbTxBw(bw);
            refreshTxPopupForMode();
            break;
        }
        default:
            break;
        }
    });

    // TX popup right-click handler for MIC CFG and ANTIVOX
    connect(m_txPopup, &ButtonRowPopup::buttonRightClicked, this, [this](int index) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        if (index == 4) { // ANTIVOX
            if (m_voxPopup && m_txPopup) {
                closeSecondaryPopups();
                m_voxPopup->setPopupMode(VoxPopupWidget::AntiVox);
                m_voxPopup->setValue(m_radioState->antiVox());
                m_voxPopup->setVoxEnabled(m_radioState->voxForCurrentMode());
                m_voxPopup->showAboveWidget(m_txPopup);
            }
        } else if (index == 6 && txOperatingMode() == RadioState::FM) {
            closeSecondaryPopups();
            m_dtmfPopup->showAboveWidget(m_txPopup);
        } else if (index == 5) {
            const auto mode = txOperatingMode();
            const int dataSubMode = m_radioState->splitEnabled()
                                        ? m_radioState->dataSubModeB()
                                        : m_radioState->dataSubMode();
            const bool usesKeyer = mode == RadioState::CW || mode == RadioState::CW_R
                                   || ((mode == RadioState::DATA || mode == RadioState::DATA_R)
                                       && (dataSubMode == 2 || dataSubMode == 3));
            if (usesKeyer) {
                const QChar nextIambic = m_radioState->iambicMode() == 'B' ? QChar('A') : QChar('B');
                const QChar paddle = m_radioState->paddleOrientation() == 'R' ? QChar('R') : QChar('N');
                const QString command = QString("KP%1%2%3;").arg(nextIambic).arg(paddle)
                                            .arg(m_radioState->keyingWeight(), 3, 10, QChar('0'));
                m_tcpClient->sendCAT(command);
                m_radioState->parseCATCommand(command);
                refreshTxPopupForMode();
            } else if (mode == RadioState::FM) {
                closeSecondaryPopups();
                m_txModePopup->showFmRepeater(m_radioState->repeaterMode(),
                                              m_radioState->repeaterOffsetKhz(), true);
                m_txModePopup->showAboveWidget(m_txPopup);
            }
        } else if (index == 3) { // MIC CFG
            int input = m_radioState->micInput();
            // LINE IN only (input=2) has no mic config
            if (input == 2)
                return;

            // Determine if Front or Rear mic
            bool isFront = (input == 0 || input == 3); // 0=front, 3=front+line
            if (m_micConfigPopup && m_txPopup) {
                closeSecondaryPopups();
                m_micConfigPopup->setMicType(isFront ? MicConfigPopupWidget::Front : MicConfigPopupWidget::Rear);
                if (isFront) {
                    m_micConfigPopup->setBias(m_radioState->micFrontBias());
                    m_micConfigPopup->setPreamp(m_radioState->micFrontPreamp());
                    m_micConfigPopup->setButtons(m_radioState->micFrontButtons());
                } else {
                    m_micConfigPopup->setBias(m_radioState->micRearBias());
                    m_micConfigPopup->setPreamp(m_radioState->micRearPreamp());
                }
                m_micConfigPopup->showAboveWidget(m_txPopup);
            }
        }
    });

    // Create RX EQ popup (Main RX - cyan theme)
    m_rxEqPopup = new RxEqPopupWidget("RX GRAPHIC EQUALIZER", K4Styles::Colors::VfoACyan, this);
    connect(m_rxEqPopup, &RxEqPopupWidget::closed, this, [this]() {
        // Close the MAIN RX button row popup when EQ popup closes
    });

    // Debounce timer for RX EQ - sends command 100ms after last slider change
    m_rxEqDebounceTimer = new QTimer(this);
    m_rxEqDebounceTimer->setSingleShot(true);
    m_rxEqDebounceTimer->setInterval(100);
    connect(m_rxEqDebounceTimer, &QTimer::timeout, this, [this]() {
        // Build RE command with all 8 bands: RE+00+00+00+00+00+00+00+00;
        QString cmd = "RE";
        for (int i = 0; i < 8; i++) {
            int value = m_radioState->rxEqBand(i);
            cmd += QString("%1%2").arg(value >= 0 ? '+' : '-').arg(qAbs(value), 2, 10, QChar('0'));
        }
        m_tcpClient->sendCAT(cmd);
    });

    connect(m_rxEqPopup, &RxEqPopupWidget::bandValueChanged, this, [this](int bandIndex, int dB) {
        // A direct band adjustment supersedes any curve retained for the
        // FLAT-toggle restore.
        m_rxEqFlatActive = false;
        m_rxEqBeforeFlat.clear();
        // Update optimistic state immediately (UI stays responsive)
        m_radioState->setRxEqBand(bandIndex, dB);
        // Restart debounce timer - will send after 100ms of no changes
        m_rxEqDebounceTimer->start();
    });
    connect(m_rxEqPopup, &RxEqPopupWidget::flatRequested, this, [this]() {
        // Mirror the K4's FLAT touch behavior for both Main and Sub RX EQ.
        // RX EQ is shared by the two receivers, therefore they share the
        // same retained pre-flat curve.
        m_rxEqDebounceTimer->stop();
        const QVector<int> current = m_radioState->rxEqBands();
        const bool isFlat = std::all_of(current.cbegin(), current.cend(), [](int dB) { return dB == 0; });

        if (m_rxEqFlatActive && isFlat && m_rxEqBeforeFlat.size() == 8) {
            const QVector<int> restore = m_rxEqBeforeFlat;
            m_rxEqFlatActive = false;
            m_rxEqBeforeFlat.clear();
            m_radioState->setRxEqBands(restore);

            QString cmd = "RE";
            for (int value : restore)
                cmd += QString("%1%2").arg(value >= 0 ? '+' : '-').arg(qAbs(value), 2, 10, QChar('0'));
            m_tcpClient->sendCAT(cmd);
            return;
        }

        // Reset all bands to 0 and send CAT command.
        m_rxEqBeforeFlat = current;
        m_rxEqFlatActive = !isFlat;
        QVector<int> flat(8, 0);
        m_radioState->setRxEqBands(flat);
        m_tcpClient->sendCAT("RE+00+00+00+00+00+00+00+00");
    });

    // Preset load: get preset from RadioSettings, apply to sliders, send CAT
    connect(m_rxEqPopup, &RxEqPopupWidget::presetLoadRequested, this, [this](int index) {
        EqPreset preset = RadioSettings::instance()->rxEqPreset(index);
        if (!preset.isEmpty() && preset.bands.size() == 8) {
            m_rxEqPopup->setAllBands(preset.bands);
            m_radioState->setRxEqBands(preset.bands);

            // Send CAT command
            QString cmd = "RE";
            for (int i = 0; i < 8; i++) {
                int value = preset.bands[i];
                cmd += QString("%1%2").arg(value >= 0 ? '+' : '-').arg(qAbs(value), 2, 10, QChar('0'));
            }
            m_tcpClient->sendCAT(cmd);
        }
    });

    // Preset save: show name dialog, save current EQ to preset
    connect(m_rxEqPopup, &RxEqPopupWidget::presetSaveRequested, this, [this](int index) {
        EqPreset existing = RadioSettings::instance()->rxEqPreset(index);
        QString defaultName = existing.name.isEmpty() ? QString("Preset %1").arg(index + 1) : existing.name;

        // Store current EQ bands before dialog (popup may close)
        QVector<int> currentBands = m_radioState->rxEqBands();

        bool ok;
        QString name = requestText("Save Preset", "Preset name:", defaultName, &ok);

        // Re-show the EQ popup after dialog closes
        if (m_bottomMenuBar) {
            m_rxEqPopup->showAboveButton(m_bottomMenuBar->mainRxButton());
        }

        if (ok) {
            // Use default name if user cleared it
            if (name.isEmpty()) {
                name = QString("Preset %1").arg(index + 1);
            }
            EqPreset preset;
            preset.name = name;
            preset.bands = currentBands;
            RadioSettings::instance()->setRxEqPreset(index, preset);
            m_rxEqPopup->updatePresetName(index, name);
        }
    });

    // Preset clear: remove preset from RadioSettings
    connect(m_rxEqPopup, &RxEqPopupWidget::presetClearRequested, this, [this](int index) {
        RadioSettings::instance()->clearRxEqPreset(index);
        m_rxEqPopup->updatePresetName(index, "");
    });

    // Load preset names on popup creation
    for (int i = 0; i < 4; i++) {
        EqPreset preset = RadioSettings::instance()->rxEqPreset(i);
        m_rxEqPopup->updatePresetName(i, preset.name);
    }

    // Create TX EQ popup (amber theme)
    m_txEqPopup = new RxEqPopupWidget("TX GRAPHIC EQUALIZER", K4Styles::Colors::AccentAmber, this);
    connect(m_txEqPopup, &RxEqPopupWidget::closed, this, [this]() {
        // Close the TX button row popup when EQ popup closes
    });

    // Debounce timer for TX EQ - sends command 100ms after last slider change
    m_txEqDebounceTimer = new QTimer(this);
    m_txEqDebounceTimer->setSingleShot(true);
    m_txEqDebounceTimer->setInterval(100);
    connect(m_txEqDebounceTimer, &QTimer::timeout, this, [this]() {
        // Build TE command with all 8 bands: TE+00+00+00+00+00+00+00+00;
        QString cmd = "TE";
        for (int i = 0; i < 8; i++) {
            int value = m_radioState->txEqBand(i);
            cmd += QString("%1%2").arg(value >= 0 ? '+' : '-').arg(qAbs(value), 2, 10, QChar('0'));
        }
        m_tcpClient->sendCAT(cmd);
    });

    connect(m_txEqPopup, &RxEqPopupWidget::bandValueChanged, this, [this](int bandIndex, int dB) {
        // A direct band adjustment creates a new curve, so it supersedes any
        // curve retained for the FLAT toggle restore.
        m_txEqFlatActive = false;
        m_txEqBeforeFlat.clear();
        // Update optimistic state immediately (UI stays responsive)
        m_radioState->setTxEqBand(bandIndex, dB);
        // Restart debounce timer - will send after 100ms of no changes
        m_txEqDebounceTimer->start();
    });
    connect(m_txEqPopup, &RxEqPopupWidget::flatRequested, this, [this]() {
        // Match the K4 FLAT touch control: first tap stores the active curve
        // and sets flat; the next tap restores the stored curve.  TE has no
        // documented radio-side toggle form, so preserve this locally.
        m_txEqDebounceTimer->stop();
        const QVector<int> current = m_radioState->txEqBands();
        const bool isFlat = std::all_of(current.cbegin(), current.cend(), [](int dB) { return dB == 0; });

        if (m_txEqFlatActive && isFlat && m_txEqBeforeFlat.size() == 8) {
            const QVector<int> restore = m_txEqBeforeFlat;
            m_txEqFlatActive = false;
            m_txEqBeforeFlat.clear();
            m_radioState->setTxEqBands(restore);

            QString cmd = "TE";
            for (int value : restore)
                cmd += QString("%1%2").arg(value >= 0 ? '+' : '-').arg(qAbs(value), 2, 10, QChar('0'));
            m_tcpClient->sendCAT(cmd);
            return;
        }

        // Reset all bands to 0 and send CAT command.
        m_txEqBeforeFlat = current;
        m_txEqFlatActive = !isFlat;
        QVector<int> flat(8, 0);
        m_radioState->setTxEqBands(flat);
        m_tcpClient->sendCAT("TE+00+00+00+00+00+00+00+00");
    });

    // TX EQ Preset load: get preset from RadioSettings, apply to sliders, send CAT
    connect(m_txEqPopup, &RxEqPopupWidget::presetLoadRequested, this, [this](int index) {
        EqPreset preset = RadioSettings::instance()->txEqPreset(index);
        if (!preset.isEmpty() && preset.bands.size() == 8) {
            m_txEqPopup->setAllBands(preset.bands);
            m_radioState->setTxEqBands(preset.bands);

            // Send CAT command
            QString cmd = "TE";
            for (int i = 0; i < 8; i++) {
                int value = preset.bands[i];
                cmd += QString("%1%2").arg(value >= 0 ? '+' : '-').arg(qAbs(value), 2, 10, QChar('0'));
            }
            m_tcpClient->sendCAT(cmd);
        }
    });

    // TX EQ Preset save: show name dialog, save current EQ to preset
    connect(m_txEqPopup, &RxEqPopupWidget::presetSaveRequested, this, [this](int index) {
        EqPreset existing = RadioSettings::instance()->txEqPreset(index);
        QString defaultName = existing.name.isEmpty() ? QString("Preset %1").arg(index + 1) : existing.name;

        // Store current EQ bands before dialog (popup may close)
        QVector<int> currentBands = m_radioState->txEqBands();

        bool ok;
        QString name = requestText("Save TX Preset", "Preset name:", defaultName, &ok);

        // Re-show the EQ popup after dialog closes
        if (m_bottomMenuBar) {
            m_txEqPopup->showAboveButton(m_bottomMenuBar->txButton());
        }

        if (ok) {
            // Use default name if user cleared it
            if (name.isEmpty()) {
                name = QString("Preset %1").arg(index + 1);
            }
            EqPreset preset;
            preset.name = name;
            preset.bands = currentBands;
            RadioSettings::instance()->setTxEqPreset(index, preset);
            m_txEqPopup->updatePresetName(index, name);
        }
    });

    // TX EQ Preset clear: remove preset from RadioSettings
    connect(m_txEqPopup, &RxEqPopupWidget::presetClearRequested, this, [this](int index) {
        RadioSettings::instance()->clearTxEqPreset(index);
        m_txEqPopup->updatePresetName(index, "");
    });

    // Load TX EQ preset names on popup creation
    for (int i = 0; i < 4; i++) {
        EqPreset preset = RadioSettings::instance()->txEqPreset(i);
        m_txEqPopup->updatePresetName(i, preset.name);
    }

    // Create antenna configuration popups (MAIN RX, SUB RX, TX)
    m_mainRxAntCfgPopup = new AntennaCfgPopupWidget(AntennaCfgVariant::MainRx, this);
    connect(m_mainRxAntCfgPopup, &AntennaCfgPopupWidget::configChanged, this,
            [this](bool displayAll, QVector<bool> mask) {
                if (!m_tcpClient || !m_tcpClient->isConnected())
                    return;
                // Build ACM command: ACMzabcdefg where z=displayAll, a-g=antenna enables
                QString cmd = QString("ACM%1").arg(displayAll ? '1' : '0');
                for (int i = 0; i < 7; i++) {
                    cmd += (i < mask.size() && mask[i]) ? '1' : '0';
                }
                m_tcpClient->sendCAT(cmd);
            });

    m_subRxAntCfgPopup = new AntennaCfgPopupWidget(AntennaCfgVariant::SubRx, this);
    connect(m_subRxAntCfgPopup, &AntennaCfgPopupWidget::configChanged, this,
            [this](bool displayAll, QVector<bool> mask) {
                if (!m_tcpClient || !m_tcpClient->isConnected())
                    return;
                // Build ACS command: ACSzabcdefg where z=displayAll, a-g=antenna enables
                QString cmd = QString("ACS%1").arg(displayAll ? '1' : '0');
                for (int i = 0; i < 7; i++) {
                    cmd += (i < mask.size() && mask[i]) ? '1' : '0';
                }
                m_tcpClient->sendCAT(cmd);
            });

    m_txAntCfgPopup = new AntennaCfgPopupWidget(AntennaCfgVariant::Tx, this);
    connect(m_txAntCfgPopup, &AntennaCfgPopupWidget::configChanged, this, [this](bool displayAll, QVector<bool> mask) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        // Build ACT command: ACTzabc where z=displayAll, a-c=antenna enables
        QString cmd = QString("ACT%1").arg(displayAll ? '1' : '0');
        for (int i = 0; i < 3; i++) {
            cmd += (i < mask.size() && mask[i]) ? '1' : '0';
        }
        m_tcpClient->sendCAT(cmd);
    });

    // Create Line Out popup (shared by MAIN RX and SUB RX)
    m_lineOutPopup = new LineOutPopupWidget(this);
    connect(m_lineOutPopup, &LineOutPopupWidget::leftLevelChanged, this, [this](int level) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        // Send full LO command with current state
        QString cmd = QString("LO%1%2%3;")
                          .arg(level, 3, 10, QChar('0'))
                          .arg(m_radioState->lineOutRight(), 3, 10, QChar('0'))
                          .arg(m_radioState->lineOutRightEqualsLeft() ? 1 : 0);
        m_tcpClient->sendCAT(cmd);
    });
    connect(m_lineOutPopup, &LineOutPopupWidget::rightLevelChanged, this, [this](int level) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        QString cmd = QString("LO%1%2%3;")
                          .arg(m_radioState->lineOutLeft(), 3, 10, QChar('0'))
                          .arg(level, 3, 10, QChar('0'))
                          .arg(m_radioState->lineOutRightEqualsLeft() ? 1 : 0);
        m_tcpClient->sendCAT(cmd);
    });
    connect(m_lineOutPopup, &LineOutPopupWidget::rightEqualsLeftChanged, this, [this](bool enabled) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        int left = m_radioState->lineOutLeft();
        int right = enabled ? left : m_radioState->lineOutRight();
        QString cmd =
            QString("LO%1%2%3;").arg(left, 3, 10, QChar('0')).arg(right, 3, 10, QChar('0')).arg(enabled ? 1 : 0);
        m_tcpClient->sendCAT(cmd);
    });
    // Connect RadioState to update popup when K4 sends LO response
    connect(m_radioState, &RadioState::lineOutChanged, this, [this]() {
        if (m_lineOutPopup) {
            m_lineOutPopup->setLeftLevel(m_radioState->lineOutLeft());
            m_lineOutPopup->setRightLevel(m_radioState->lineOutRight());
            m_lineOutPopup->setRightEqualsLeft(m_radioState->lineOutRightEqualsLeft());
        }
    });

    // Create Line In popup (TX menu button index 3)
    m_lineInPopup = new LineInPopupWidget(this);
    connect(m_lineInPopup, &LineInPopupWidget::soundCardLevelChanged, this, [this](int level) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        m_radioState->setLineInSoundCard(level);
        QString cmd = QString("LI%1%2%3;")
                          .arg(level, 3, 10, QChar('0'))
                          .arg(m_radioState->lineInJack(), 3, 10, QChar('0'))
                          .arg(m_radioState->lineInSource());
        m_tcpClient->sendCAT(cmd);
    });
    connect(m_lineInPopup, &LineInPopupWidget::lineInJackLevelChanged, this, [this](int level) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        m_radioState->setLineInJack(level);
        QString cmd = QString("LI%1%2%3;")
                          .arg(m_radioState->lineInSoundCard(), 3, 10, QChar('0'))
                          .arg(level, 3, 10, QChar('0'))
                          .arg(m_radioState->lineInSource());
        m_tcpClient->sendCAT(cmd);
    });
    connect(m_lineInPopup, &LineInPopupWidget::sourceChanged, this, [this](int source) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        m_radioState->setLineInSource(source);
        QString cmd = QString("LI%1%2%3;")
                          .arg(m_radioState->lineInSoundCard(), 3, 10, QChar('0'))
                          .arg(m_radioState->lineInJack(), 3, 10, QChar('0'))
                          .arg(source);
        m_tcpClient->sendCAT(cmd);
    });
    // Connect RadioState to update popup when K4 sends LI response
    connect(m_radioState, &RadioState::lineInChanged, this, [this]() {
        if (m_lineInPopup) {
            m_lineInPopup->setSoundCardLevel(m_radioState->lineInSoundCard());
            m_lineInPopup->setLineInJackLevel(m_radioState->lineInJack());
            m_lineInPopup->setSource(m_radioState->lineInSource());
        }
    });

    // Create Mic Input popup (TX menu button index 3, left-click)
    m_micInputPopup = new MicInputPopupWidget(this);
    connect(m_micInputPopup, &MicInputPopupWidget::inputChanged, this, [this](int input) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        m_radioState->setMicInput(input);
        m_tcpClient->sendCAT(QString("MI%1;").arg(input));
    });
    // Connect RadioState to update popup when K4 sends MI response
    connect(m_radioState, &RadioState::micInputChanged, this, [this](int input) {
        if (m_micInputPopup) {
            m_micInputPopup->setCurrentInput(input);
        }
    });

    // Create Mic Config popup (TX menu button index 3, right-click)
    m_micConfigPopup = new MicConfigPopupWidget(this);
    connect(m_micConfigPopup, &MicConfigPopupWidget::biasChanged, this, [this](int bias) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        // Use individual SET command based on mic type
        if (m_micConfigPopup->micType() == MicConfigPopupWidget::Front) {
            m_radioState->setMicFrontBias(bias);
            m_tcpClient->sendCAT(QString("MSB%1;").arg(bias));
        } else {
            m_radioState->setMicRearBias(bias);
            m_tcpClient->sendCAT(QString("MSE%1;").arg(bias));
        }
    });
    connect(m_micConfigPopup, &MicConfigPopupWidget::preampChanged, this, [this](int preamp) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        if (m_micConfigPopup->micType() == MicConfigPopupWidget::Front) {
            m_radioState->setMicFrontPreamp(preamp);
            m_tcpClient->sendCAT(QString("MSA%1;").arg(preamp));
        } else {
            m_radioState->setMicRearPreamp(preamp);
            m_tcpClient->sendCAT(QString("MSD%1;").arg(preamp));
        }
    });
    connect(m_micConfigPopup, &MicConfigPopupWidget::buttonsChanged, this, [this](int buttons) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        // Buttons only applies to Front mic
        m_radioState->setMicFrontButtons(buttons);
        m_tcpClient->sendCAT(QString("MSC%1;").arg(buttons));
    });
    // Connect RadioState to update popup when K4 sends MS response
    connect(m_radioState, &RadioState::micSetupChanged, this, [this]() {
        if (m_micConfigPopup) {
            if (m_micConfigPopup->micType() == MicConfigPopupWidget::Front) {
                m_micConfigPopup->setBias(m_radioState->micFrontBias());
                m_micConfigPopup->setPreamp(m_radioState->micFrontPreamp());
                m_micConfigPopup->setButtons(m_radioState->micFrontButtons());
            } else {
                m_micConfigPopup->setBias(m_radioState->micRearBias());
                m_micConfigPopup->setPreamp(m_radioState->micRearPreamp());
            }
        }
    });

    // Create VOX Gain / Anti-VOX popup (TX menu button index 4)
    m_voxPopup = new VoxPopupWidget(this);
    connect(m_voxPopup, &VoxPopupWidget::valueChanged, this, [this](int value) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        if (m_voxPopup->popupMode() == VoxPopupWidget::VoxGain) {
            // VOX Gain: VGVnnn or VGDnnn depending on mode
            const RadioState::Mode txMode = txOperatingMode();
            const bool isDataMode = txMode == RadioState::DATA || txMode == RadioState::DATA_R;
            QString modeChar = isDataMode ? "D" : "V";
            if (isDataMode) {
                m_radioState->setVoxGainData(value);
            } else {
                m_radioState->setVoxGainVoice(value);
            }
            m_tcpClient->sendCAT(QString("VG%1%2;").arg(modeChar).arg(value, 3, 10, QChar('0')));
        } else {
            // Anti-VOX: VInnn
            m_radioState->setAntiVox(value);
            m_tcpClient->sendCAT(QString("VI%1;").arg(value, 3, 10, QChar('0')));
        }
    });
    connect(m_voxPopup, &VoxPopupWidget::voxToggled, this, [this](bool enabled) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        // VXmn where m=C/V/D, n=0/1
        const RadioState::Mode mode = txOperatingMode();
        QString modeChar;
        if (mode == RadioState::CW || mode == RadioState::CW_R) {
            modeChar = "C";
        } else if (mode == RadioState::DATA || mode == RadioState::DATA_R) {
            modeChar = "D";
        } else {
            modeChar = "V";
        }
        m_tcpClient->sendCAT(QString("VX%1%2;").arg(modeChar).arg(enabled ? 1 : 0));
    });
    // Connect RadioState to update popup when K4 sends VG/VI/VX response
    connect(m_radioState, &RadioState::voxGainChanged, this, [this](int mode, int gain) {
        if (m_voxPopup && m_voxPopup->popupMode() == VoxPopupWidget::VoxGain) {
            const RadioState::Mode txMode = txOperatingMode();
            const bool isDataMode = txMode == RadioState::DATA || txMode == RadioState::DATA_R;
            if ((mode == 1 && isDataMode) || (mode == 0 && !isDataMode)) {
                m_voxPopup->setValue(gain);
            }
        }
    });
    connect(m_radioState, &RadioState::antiVoxChanged, this, [this](int level) {
        if (m_voxPopup && m_voxPopup->popupMode() == VoxPopupWidget::AntiVox) {
            m_voxPopup->setValue(level);
        }
    });
    connect(m_radioState, &RadioState::voxChanged, this, [this](bool enabled) {
        Q_UNUSED(enabled)
        if (m_voxPopup) {
            const RadioState::Mode txMode = txOperatingMode();
            if (txMode == RadioState::CW || txMode == RadioState::CW_R)
                m_voxPopup->setVoxEnabled(m_radioState->voxCW());
            else if (txMode == RadioState::DATA || txMode == RadioState::DATA_R)
                m_voxPopup->setVoxEnabled(m_radioState->voxData());
            else
                m_voxPopup->setVoxEnabled(m_radioState->voxVoice());
        }
    });

    // Create SSB TX Bandwidth popup (TX menu button index 5)
    m_ssbBwPopup = new SsbBwPopupWidget(this);
    m_keyingWeightPopup = new KeyingWeightPopupWidget(this);
    m_fmPlPopup = new FmPlPopupWidget(this);
    m_dtmfPopup = new DtmfPopupWidget(this);
    m_txModePopup = new TxModePopupWidget(this);
    m_softwareListPopup = new SoftwareListPopupWidget(this);
    connect(m_txModePopup, &TxModePopupWidget::dataBandwidthChanged, this, [this](int value) {
        const QString command = QString("DW%1;").arg(value, 2, 10, QChar('0'));
        m_tcpClient->sendCAT(command);
        m_radioState->parseCATCommand(command);
        refreshTxPopupForMode();
    });
    connect(m_txModePopup, &TxModePopupWidget::repeaterChanged, this, [this](QChar mode, int offset) {
        const QString command = QString("RP%1%2;").arg(mode).arg(offset, 5, 10, QChar('0'));
        m_tcpClient->sendCAT(command);
        m_radioState->parseCATCommand(command);
        refreshTxPopupForMode();
    });
    connect(m_fmPlPopup, &FmPlPopupWidget::toneChanged, this, [this](int index, bool enabled) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        const QString target = m_radioState->splitEnabled() ? "PL$" : "PL";
        const QString command = QString("%1%2%3;").arg(target).arg(index, 2, 10, QChar('0')).arg(enabled ? 1 : 0);
        m_tcpClient->sendCAT(command);
        // PL state must be confirmed by the K4. Do not optimistically paint
        // FM/T or ON/OFF when the radio has rejected or deferred the setting.
        QTimer::singleShot(250, this, [this]() {
            if (m_tcpClient && m_tcpClient->isConnected())
                m_tcpClient->sendCAT(m_radioState->splitEnabled() ? "PL$;" : "PL;");
        });
    });
    connect(m_dtmfPopup, &DtmfPopupWidget::digitRequested, this, [this](QChar digit) {
        if (!m_radioState->isTransmitting()) {
            closeAllPopups();
            showInWindowMessage(centralWidget(), "DTMF", "Enter transmit mode before sending DTMF tones.");
            return;
        }
        if (m_tcpClient && m_tcpClient->isConnected()) m_tcpClient->sendCAT(QString("DM%1;").arg(digit));
    });
    connect(m_dtmfPopup, &DtmfPopupWidget::sequenceRequested, this, [this](const QString &sequence) {
        if (!m_radioState->isTransmitting()) {
            closeAllPopups();
            showInWindowMessage(centralWidget(), "DTMF", "Enter transmit mode before playing a DTMF command.");
            return;
        }
        QString commands;
        for (const QChar digit : sequence) commands += QString("DM%1;").arg(digit);
        if (m_tcpClient && m_tcpClient->isConnected()) m_tcpClient->sendCAT(commands);
    });
    connect(m_radioState, &RadioState::plToneChanged, this, [this](bool subRx, int index, bool enabled) {
        if (subRx == m_radioState->splitEnabled())
            m_fmPlPopup->setTone(index, enabled);
        refreshTxPopupForMode();
        updateModeLabels();
    });
    connect(m_radioState, &RadioState::repeaterChanged, this, [this](QChar mode, int offsetKhz) {
        Q_UNUSED(mode)
        Q_UNUSED(offsetKhz)
        refreshTxPopupForMode();
        updateModeLabels();
    });
    connect(m_keyingWeightPopup, &KeyingWeightPopupWidget::weightChanged, this, [this](int weight) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        const QChar iambic = m_radioState->iambicMode() == 'B' ? QChar('B') : QChar('A');
        const QChar paddle = m_radioState->paddleOrientation() == 'R' ? QChar('R') : QChar('N');
        const QString command = QString("KP%1%2%3;").arg(iambic).arg(paddle)
                                    .arg(weight, 3, 10, QChar('0'));
        m_tcpClient->sendCAT(command);
        m_radioState->parseCATCommand(command);
        refreshTxPopupForMode();
    });
    connect(m_ssbBwPopup, &SsbBwPopupWidget::doneRequested, this, [this]() {
        // Bandwidth changes are applied immediately. Done returns from the
        // editor and closes the complete TX operating-menu stack.
        closeAllPopups();
    });
    connect(m_ssbBwPopup, &SsbBwPopupWidget::bandwidthChanged, this, [this](int bw) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        // ES command: ESnbb where n=essb mode, bb=bandwidth
        int essbMode = m_radioState->essbEnabled() ? 1 : 0;
        m_radioState->setSsbTxBw(bw);
        m_tcpClient->sendCAT(QString("ES%1%2;").arg(essbMode).arg(bw, 2, 10, QChar('0')));
        // Keep the row mode-aware if the radio changes mode while this
        // editor is closing or an ES response is still in flight.
        refreshTxPopupForMode();
    });
    // Connect RadioState to update popup and ESSB button when K4 sends ES response
    // SSB: 24-28 (2.4-2.8 kHz), ESSB: 30-45 (3.0-4.5 kHz)
    connect(m_radioState, &RadioState::essbChanged, this, [this](bool enabled, int bw) {
        if (m_ssbBwPopup) {
            m_ssbBwPopup->setEssbEnabled(enabled);
            if (bw >= 24 && bw <= 45) {
                m_ssbBwPopup->setBandwidth(bw);
            }
        }
        // ES replies are asynchronous and may arrive after an operating-mode
        // change. Never let one repaint CW, AM, FM, or DATA as an SSB menu.
        refreshTxPopupForMode();
        // Update mode labels to show USB+/LSB+ when ESSB enabled
        updateModeLabels();
    });

    // Create floating text decode windows (separate for MAIN RX and SUB RX)
    // Controls integrated in title bar - no separate popup needed
    m_textDecodeWindowMain = new TextDecodeWindow(TextDecodeWindow::MainRx, this);
    m_textDecodeWindowSub = new TextDecodeWindow(TextDecodeWindow::SubRx, this);

    // Helper lambda to send TD command
    auto sendTextDecodeCmd = [this](TextDecodeWindow *window, bool isMainRx) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        int mode = window->isDecodeEnabled() ? (2 + window->wpmRange()) : 0;
        int threshold = window->autoThreshold() ? 0 : window->threshold();
        QString cmdPrefix = isMainRx ? "TD" : "TD$";
        // TD is exactly TD$mtl; l is a single digit. Sending the old default
        // of 10 produced TD...10; which the K4 rejects instead of enabling
        // its decoder.
        const int lines = qBound(1, window->maxLines(), 9);
        QString cmd = QString("%1%2%3%4;").arg(cmdPrefix).arg(mode).arg(threshold).arg(lines);
        qDebug() << "Sending TD command:" << cmd;
        m_tcpClient->sendCAT(cmd);
    };

    // Wire MAIN RX window signals
    connect(m_textDecodeWindowMain, &TextDecodeWindow::enabledChanged, this,
            [this, sendTextDecodeCmd](bool) { sendTextDecodeCmd(m_textDecodeWindowMain, true); });
    connect(m_textDecodeWindowMain, &TextDecodeWindow::wpmRangeChanged, this, [this, sendTextDecodeCmd](int) {
        if (m_textDecodeWindowMain->isDecodeEnabled()) {
            sendTextDecodeCmd(m_textDecodeWindowMain, true);
        }
    });
    connect(m_textDecodeWindowMain, &TextDecodeWindow::thresholdModeChanged, this, [this, sendTextDecodeCmd](bool) {
        if (m_textDecodeWindowMain->isDecodeEnabled()) {
            sendTextDecodeCmd(m_textDecodeWindowMain, true);
        }
    });
    connect(m_textDecodeWindowMain, &TextDecodeWindow::thresholdChanged, this, [this, sendTextDecodeCmd](int) {
        if (m_textDecodeWindowMain->isDecodeEnabled()) {
            sendTextDecodeCmd(m_textDecodeWindowMain, true);
        }
    });
    connect(m_textDecodeWindowMain, &TextDecodeWindow::closeRequested, this, [this, sendTextDecodeCmd]() {
        // Disable decode and hide window
        m_textDecodeWindowMain->setDecodeEnabled(false);
        sendTextDecodeCmd(m_textDecodeWindowMain, true);
        m_textDecodeWindowMain->clearText();
        m_textDecodeWindowMain->hide();
    });

    // Wire SUB RX window signals
    connect(m_textDecodeWindowSub, &TextDecodeWindow::enabledChanged, this,
            [this, sendTextDecodeCmd](bool) { sendTextDecodeCmd(m_textDecodeWindowSub, false); });
    connect(m_textDecodeWindowSub, &TextDecodeWindow::wpmRangeChanged, this, [this, sendTextDecodeCmd](int) {
        if (m_textDecodeWindowSub->isDecodeEnabled()) {
            sendTextDecodeCmd(m_textDecodeWindowSub, false);
        }
    });
    connect(m_textDecodeWindowSub, &TextDecodeWindow::thresholdModeChanged, this, [this, sendTextDecodeCmd](bool) {
        if (m_textDecodeWindowSub->isDecodeEnabled()) {
            sendTextDecodeCmd(m_textDecodeWindowSub, false);
        }
    });
    connect(m_textDecodeWindowSub, &TextDecodeWindow::thresholdChanged, this, [this, sendTextDecodeCmd](int) {
        if (m_textDecodeWindowSub->isDecodeEnabled()) {
            sendTextDecodeCmd(m_textDecodeWindowSub, false);
        }
    });
    connect(m_textDecodeWindowSub, &TextDecodeWindow::closeRequested, this, [this, sendTextDecodeCmd]() {
        // Disable decode and hide window
        m_textDecodeWindowSub->setDecodeEnabled(false);
        sendTextDecodeCmd(m_textDecodeWindowSub, false);
        m_textDecodeWindowSub->clearText();
        m_textDecodeWindowSub->hide();
    });

    // Connect RadioState text decode signals to sync window state
    connect(m_radioState, &RadioState::textDecodeChanged, this, [this]() {
        int mode = m_radioState->textDecodeMode();
        bool enabled = (mode > 0);
        m_textDecodeWindowMain->setDecodeEnabled(enabled);
        if (mode >= 2 && mode <= 4) {
            m_textDecodeWindowMain->setWpmRange(mode - 2);
        }
        int threshold = m_radioState->textDecodeThreshold();
        m_textDecodeWindowMain->setAutoThreshold(threshold == 0);
        if (threshold > 0) {
            m_textDecodeWindowMain->setThreshold(threshold);
        }
        m_textDecodeWindowMain->setMaxLines(m_radioState->textDecodeLines());
    });
    connect(m_radioState, &RadioState::textDecodeBChanged, this, [this]() {
        int mode = m_radioState->textDecodeModeB();
        bool enabled = (mode > 0);
        m_textDecodeWindowSub->setDecodeEnabled(enabled);
        if (mode >= 2 && mode <= 4) {
            m_textDecodeWindowSub->setWpmRange(mode - 2);
        }
        int threshold = m_radioState->textDecodeThresholdB();
        m_textDecodeWindowSub->setAutoThreshold(threshold == 0);
        if (threshold > 0) {
            m_textDecodeWindowSub->setThreshold(threshold);
        }
        m_textDecodeWindowSub->setMaxLines(m_radioState->textDecodeLinesB());
    });

    // Connect decoded text buffer to windows
    connect(m_radioState, &RadioState::textBufferReceived, this, [this](const QString &text, bool isSubRx) {
        if (isSubRx) {
            m_textDecodeWindowSub->appendText(text);
        } else {
            m_textDecodeWindowMain->appendText(text);
        }
    });

    // Create notification popup for K4 error/status messages (ERxx:)
    m_notificationWidget = new NotificationWidget(this);

    // TcpClient signals
    connect(m_tcpClient, &TcpClient::stateChanged, this, &MainWindow::onStateChanged);
    connect(m_tcpClient, &TcpClient::errorOccurred, this, &MainWindow::onError);
    connect(m_tcpClient, &TcpClient::authenticated, this, &MainWindow::onAuthenticated);
    connect(m_tcpClient, &TcpClient::authenticationFailed, this, &MainWindow::onAuthenticationFailed);

    // Protocol CAT responses -> RadioState
    connect(m_tcpClient->protocol(), &Protocol::catResponseReceived, this, &MainWindow::onCatResponse);

    // RadioState signals -> UI updates (VFO A)
    connect(m_radioState, &RadioState::frequencyChanged, this, &MainWindow::onFrequencyChanged);
    connect(m_radioState, &RadioState::modeChanged, this, &MainWindow::onModeChanged);
    connect(m_radioState, &RadioState::modeChanged, this, [this](RadioState::Mode) {
        onVoxChanged(false); // Refresh VOX display when mode changes (VOX is mode-specific)
    });
    // Data sub-mode changes also update mode label (AFSK, FSK, PSK, DATA)
    connect(m_radioState, &RadioState::dataSubModeChanged, this, [this](int) {
        updateModeLabels();
        refreshTxPopupForMode();
        refreshRxPopupsForModes();
    });
    connect(m_radioState, &RadioState::sMeterChanged, this, &MainWindow::onSMeterChanged);
    connect(m_radioState, &RadioState::filterBandwidthChanged, this, &MainWindow::onBandwidthChanged);
    // RX EQ state -> popup (Main and Sub RX share the same EQ)
    connect(m_radioState, &RadioState::rxEqChanged, this, [this]() {
        if (m_rxEqPopup) {
            m_rxEqPopup->setAllBands(m_radioState->rxEqBands());
        }
    });
    // TX EQ state -> popup
    connect(m_radioState, &RadioState::txEqChanged, this, [this]() {
        if (m_txEqPopup) {
            m_txEqPopup->setAllBands(m_radioState->txEqBands());
        }
    });

    // Antenna configuration signals - update popups when state changes
    connect(m_radioState, &RadioState::mainRxAntCfgChanged, this, [this]() {
        if (m_mainRxAntCfgPopup) {
            m_mainRxAntCfgPopup->setDisplayAll(m_radioState->mainRxDisplayAll());
            m_mainRxAntCfgPopup->setAntennaMask(m_radioState->mainRxAntMask());
        }
    });
    connect(m_radioState, &RadioState::subRxAntCfgChanged, this, [this]() {
        if (m_subRxAntCfgPopup) {
            m_subRxAntCfgPopup->setDisplayAll(m_radioState->subRxDisplayAll());
            m_subRxAntCfgPopup->setAntennaMask(m_radioState->subRxAntMask());
        }
    });
    connect(m_radioState, &RadioState::txAntCfgChanged, this, [this]() {
        if (m_txAntCfgPopup) {
            m_txAntCfgPopup->setDisplayAll(m_radioState->txDisplayAll());
            m_txAntCfgPopup->setAntennaMask(m_radioState->txAntMask());
        }
    });

    // RadioState signals -> UI updates (VFO B)
    connect(m_radioState, &RadioState::frequencyBChanged, this, &MainWindow::onFrequencyBChanged);
    connect(m_radioState, &RadioState::modeBChanged, this, &MainWindow::onModeBChanged);
    // Data sub-mode changes also update mode label (AFSK, FSK, PSK, DATA)
    connect(m_radioState, &RadioState::dataSubModeBChanged, this, [this](int) {
        updateModeLabels();
        refreshTxPopupForMode();
        refreshRxPopupsForModes();
    });
    connect(m_radioState, &RadioState::sMeterBChanged, this, &MainWindow::onSMeterBChanged);
    connect(m_radioState, &RadioState::filterBandwidthBChanged, this, &MainWindow::onBandwidthBChanged);

    // Auto-hide mini pan B when VFOs move to different bands (and SUB RX is off)
    connect(m_radioState, &RadioState::frequencyChanged, this, &MainWindow::checkAndHideMiniPanB);
    connect(m_radioState, &RadioState::frequencyBChanged, this, &MainWindow::checkAndHideMiniPanB);

    // RadioState signals -> Status bar updates
    connect(m_radioState, &RadioState::rfPowerChanged, this, &MainWindow::onRfPowerChanged);
    connect(m_radioState, &RadioState::supplyVoltageChanged, this, &MainWindow::onSupplyVoltageChanged);
    connect(m_radioState, &RadioState::supplyCurrentChanged, this, &MainWindow::onSupplyCurrentChanged);
    connect(m_radioState, &RadioState::lpaTemperatureChanged, this, [this](int celsius) {
        m_lpaTempLabel->setText(QString("LPA %1°C").arg(celsius));
        m_lpaTempLabel->setStyleSheet(temperatureStyle(celsius));
    });
    connect(m_radioState, &RadioState::paTemperatureChanged, this, [this](int celsius) {
        m_paTempLabel->setText(QString("PA %1°C").arg(celsius));
        m_paTempLabel->setStyleSheet(temperatureStyle(celsius));
    });
    connect(m_radioState, &RadioState::swrChanged, this, &MainWindow::onSwrChanged);

    // Display FPS (synthetic menu item)
    connect(m_radioState, &RadioState::displayFpsChanged, this, &MainWindow::onDisplayFpsChanged);

    // Error/notification messages from K4 (ERxx: format) -> show notification popup
    connect(m_radioState, &RadioState::errorNotificationReceived, this, &MainWindow::onErrorNotification);

    // TX Meter data -> update power displays and VFO multifunction meters during TX
    connect(m_radioState, &RadioState::txMeterChanged, this, [this](int alc, int comp, double fwdPower, double swr) {
        // Update status bar power label
        QString powerStr;
        if (fwdPower < 10.0) {
            powerStr = QString("%1 W").arg(fwdPower, 0, 'f', 1);
        } else {
            powerStr = QString("%1 W").arg(static_cast<int>(fwdPower));
        }
        m_powerLabel->setText(powerStr);
        // Update side panel power reading
        m_sideControlPanel->setPowerReading(fwdPower);

        // Calculate PA drain current (Id) from forward power and supply voltage
        // Formula: Id = ForwardPower / (Voltage × Efficiency)
        // K4 PA efficiency is approximately 34% (measured: 80W @ 17A @ 13.8V)
        double voltage = m_radioState->supplyVoltage();
        double paCurrent = 0.0;
        if (voltage > 0 && fwdPower > 0) {
            paCurrent = fwdPower / (voltage * 0.34);
        }

        // Update TX meters only on the active TX VFO
        // SPLIT OFF: VFO A transmits, SPLIT ON: VFO B transmits
        if (m_radioState->splitEnabled()) {
            m_vfoB->setTxMeters(alc, comp, fwdPower, swr);
            m_vfoB->setTxMeterCurrent(paCurrent);
        } else {
            m_vfoA->setTxMeters(alc, comp, fwdPower, swr);
            m_vfoA->setTxMeterCurrent(paCurrent);
        }
    });

    // TX state changes -> switch VFO meters between S-meter (RX) and Po (TX) mode
    // Also change TX indicator color to red when transmitting
    connect(m_radioState, &RadioState::transmitStateChanged, this, [this](bool transmitting) {
        // Only the active TX VFO switches to TX meter mode
        // SPLIT OFF: VFO A transmits, SPLIT ON: VFO B transmits
        // The non-TX VFO stays in S-meter mode (showing received signal)
        if (m_radioState->splitEnabled()) {
            m_vfoA->setTransmitting(false); // VFO A stays in RX mode
            m_vfoB->setTransmitting(transmitting);
        } else {
            m_vfoA->setTransmitting(transmitting);
            m_vfoB->setTransmitting(false); // VFO B stays in RX mode
        }

        // TX indicator and triangles turn red when transmitting
        QString color = transmitting ? "#FF0000" : K4Styles::Colors::AccentAmber;
        m_txIndicator->setStyleSheet(QString("color: %1; font-size: 18px; font-weight: bold;").arg(color));
        m_txTriangle->setStyleSheet(QString("color: %1; font-size: 18px;").arg(color));
        m_txTriangleB->setStyleSheet(QString("color: %1; font-size: 18px;").arg(color));
    });

    // SUB indicator - green when sub RX enabled, grey when off
    // Also updates DIV indicator since DIV requires SUB to be on
    // Also dims VFO B frequency and mode labels when SUB RX is off
    connect(m_radioState, &RadioState::subRxEnabledChanged, this, [this](bool enabled) {
        if (enabled) {
            m_subLabel->setStyleSheet(QString("background-color: %1;"
                                              "color: black;"
                                              "font-size: 9px;"
                                              "font-weight: bold;"
                                              "border-radius: 2px;")
                                          .arg(K4Styles::Colors::StatusGreen));
            // If DIV is also on, light up the DIV indicator (handles timing when SB3 comes after DV1)
            if (m_radioState->diversityEnabled()) {
                m_divLabel->setStyleSheet(QString("background-color: %1;"
                                                  "color: black;"
                                                  "font-size: 9px;"
                                                  "font-weight: bold;"
                                                  "border-radius: 2px;")
                                              .arg(K4Styles::Colors::StatusGreen));
            }
            // Restore VFO B frequency and mode to normal white
            m_vfoB->frequencyDisplay()->setNormalColor(QColor(K4Styles::Colors::TextWhite));
            m_modeBLabel->setStyleSheet(
                QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::TextWhite));
        } else {
            m_subLabel->setStyleSheet(
                QString("background-color: %1;"
                        "color: %2;"
                        "font-size: 9px;"
                        "font-weight: bold;"
                        "border-radius: 2px;")
                    .arg(K4Styles::Colors::DisabledBackground, K4Styles::Colors::LightGradientTop));
            // DIV requires SUB - turn off DIV indicator when SUB is off
            m_divLabel->setStyleSheet(
                QString("background-color: %1;"
                        "color: %2;"
                        "font-size: 9px;"
                        "font-weight: bold;"
                        "border-radius: 2px;")
                    .arg(K4Styles::Colors::DisabledBackground, K4Styles::Colors::LightGradientTop));
            // Dim VFO B frequency and mode to indicate SUB RX is off
            m_vfoB->frequencyDisplay()->setNormalColor(QColor(K4Styles::Colors::InactiveGray));
            m_modeBLabel->setStyleSheet(
                QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::InactiveGray));

            // Auto-hide mini pan B if VFOs are on different bands (can't have mini pan B without SUB RX)
            checkAndHideMiniPanB();
        }

        // Mute/unmute sub RX audio channel
        if (m_audioEngine) {
            m_audioEngine->setSubMuted(!enabled);
        }
    });

    // DIV indicator - green only when BOTH diversity AND sub RX are enabled
    // (DIV requires SUB to be on - can't have DIV without SUB)
    connect(m_radioState, &RadioState::diversityChanged, this, [this](bool enabled) {
        // DIV only shows green if both diversity is enabled AND sub RX is enabled
        bool showActive = enabled && m_radioState->subReceiverEnabled();
        if (showActive) {
            m_divLabel->setStyleSheet(QString("background-color: %1;"
                                              "color: black;"
                                              "font-size: 9px;"
                                              "font-weight: bold;"
                                              "border-radius: 2px;")
                                          .arg(K4Styles::Colors::StatusGreen));
        } else {
            m_divLabel->setStyleSheet(
                QString("background-color: %1;"
                        "color: %2;"
                        "font-size: 9px;"
                        "font-weight: bold;"
                        "border-radius: 2px;")
                    .arg(K4Styles::Colors::DisabledBackground, K4Styles::Colors::LightGradientTop));
        }
    });

    // VFO Lock indicators - show lock arc on VFO A/B squares when locked
    connect(m_radioState, &RadioState::lockAChanged, this, [this](bool locked) { m_vfoRow->setLockA(locked); });
    connect(m_radioState, &RadioState::lockBChanged, this, [this](bool locked) { m_vfoRow->setLockB(locked); });

    // NOTE: KPA1500 amplifier integration groundwork is in the KPA1500 section (after m_kpa1500Client creation)

    // RadioState signals -> Side control panel updates (BW/SHFT/HI/LO)
    // Helper to update all 4 filter display values (called on BW or SHFT change)
    // When B SET is enabled, shows VFO B (Sub RX) filter values instead of VFO A
    auto updateFilterDisplay = [this]() {
        bool bSet = m_radioState->bSetEnabled();

        // Get bandwidth and shift from correct VFO
        int bwHz = bSet ? m_radioState->filterBandwidthB() : m_radioState->filterBandwidth();
        int shiftHz = bSet ? m_radioState->shiftBHz() : m_radioState->shiftHz();

        // BW/SHFT in kHz
        m_sideControlPanel->setBandwidth(bwHz / 1000.0);
        m_sideControlPanel->setShift(shiftHz / 1000.0);

        // IS is the K4's AF center-pitch. Clamp LO at zero, then place HI
        // one bandwidth above it, matching the core QK4 filter presentation.
        int lowHz = qMax(0, shiftHz - (bwHz / 2));
        int highHz = lowHz + bwHz;
        m_sideControlPanel->setHighCut(highHz / 1000.0);
        m_sideControlPanel->setLowCut(lowHz / 1000.0);

        const RadioState::Mode mode = bSet ? m_radioState->modeB() : m_radioState->mode();
        const int subMode = bSet ? m_radioState->dataSubModeB() : m_radioState->dataSubMode();
        int bwMinHz = 50;
        int bwMaxHz = 5000;
        int centerMaxDah = (mode == RadioState::CW || mode == RadioState::CW_R) ? 200 : 300;
        bool centerLocked = mode == RadioState::AM || mode == RadioState::FM;
        if (mode == RadioState::DATA || mode == RadioState::DATA_R) {
            if (subMode == 2) {
                bwMinHz = 150;
                bwMaxHz = 800;
                centerLocked = true;
            } else if (subMode == 3) {
                bwMaxHz = 200;
                centerLocked = true;
            }
        }
        m_sideControlPanel->setFilterControlRanges(bwMinHz, bwMaxHz, 30, centerMaxDah, centerLocked);
    };
    connect(m_radioState, &RadioState::filterBandwidthChanged, this, updateFilterDisplay);
    connect(m_radioState, &RadioState::ifShiftChanged, this, updateFilterDisplay);
    connect(m_radioState, &RadioState::filterBandwidthBChanged, this, updateFilterDisplay);
    connect(m_radioState, &RadioState::ifShiftBChanged, this, updateFilterDisplay);
    connect(m_radioState, &RadioState::bSetChanged, this, updateFilterDisplay);
    connect(m_radioState, &RadioState::modeChanged, this, updateFilterDisplay);
    connect(m_radioState, &RadioState::modeBChanged, this, updateFilterDisplay);
    connect(m_radioState, &RadioState::dataSubModeChanged, this, updateFilterDisplay);
    connect(m_radioState, &RadioState::dataSubModeBChanged, this, updateFilterDisplay);
    connect(m_radioState, &RadioState::keyerSpeedChanged, m_sideControlPanel, &SideControlPanel::setWpm);
    connect(m_radioState, &RadioState::cwPitchChanged, this, [this](int pitch) {
        m_sideControlPanel->setPitch(pitch / 1000.0); // Hz to kHz (500Hz = 0.50)
    });
    connect(m_radioState, &RadioState::rfPowerChanged, this,
            [this](double watts, bool) { m_sideControlPanel->setPower(watts); });
    connect(m_radioState, &RadioState::qskDelayChanged, this, [this](int delay) {
        m_sideControlPanel->setDelay(delay / 100.0); // 10ms units to seconds (20 -> 0.20)
    });
    connect(m_radioState, &RadioState::rfGainChanged, m_sideControlPanel, &SideControlPanel::setMainRfGain);
    connect(m_radioState, &RadioState::squelchChanged, m_sideControlPanel, &SideControlPanel::setMainSquelch);
    connect(m_radioState, &RadioState::rfGainBChanged, m_sideControlPanel, &SideControlPanel::setSubRfGain);
    connect(m_radioState, &RadioState::squelchBChanged, m_sideControlPanel, &SideControlPanel::setSubSquelch);
    connect(m_radioState, &RadioState::micGainChanged, m_sideControlPanel, &SideControlPanel::setMicGain);
    connect(m_radioState, &RadioState::compressionChanged, m_sideControlPanel, &SideControlPanel::setCompression);
    // Mode-dependent WPM/PTCH vs MIC/CMP display
    connect(m_radioState, &RadioState::modeChanged, this, [this](RadioState::Mode mode) {
        bool isCW = (mode == RadioState::CW || mode == RadioState::CW_R);
        m_sideControlPanel->setDisplayMode(isCW);
        // Refresh values after mode switch
        if (isCW) {
            m_sideControlPanel->setWpm(m_radioState->keyerSpeed());
            m_sideControlPanel->setPitch(m_radioState->cwPitch() / 1000.0);
        } else {
            m_sideControlPanel->setMicGain(m_radioState->micGain());
            m_sideControlPanel->setCompression(m_radioState->compression());
        }
    });

    // RadioState signals -> Center section updates
    connect(m_radioState, &RadioState::splitChanged, this, &MainWindow::onSplitChanged);
    connect(m_radioState, &RadioState::antennaChanged, this, &MainWindow::onAntennaChanged);
    connect(m_radioState, &RadioState::antennaNameChanged, this, &MainWindow::onAntennaNameChanged);
    connect(m_radioState, &RadioState::voxChanged, this, &MainWindow::onVoxChanged);
    connect(m_radioState, &RadioState::qskEnabledChanged, this, &MainWindow::onQskEnabledChanged);
    connect(m_radioState, &RadioState::testModeChanged, this, &MainWindow::onTestModeChanged);
    connect(m_radioState, &RadioState::atuModeChanged, this, &MainWindow::onAtuModeChanged);
    connect(m_radioState, &RadioState::ritXitChanged, this, &MainWindow::onRitXitChanged);
    connect(m_radioState, &RadioState::ritXitBChanged, this, [this](bool, int) {
        onRitXitChanged(m_radioState->ritEnabled(), m_radioState->xitEnabled(), m_radioState->ritXitOffset());
    });
    connect(m_radioState, &RadioState::messageBankChanged, this, &MainWindow::onMessageBankChanged);

    // Filter position indicators
    connect(m_radioState, &RadioState::filterPositionChanged, this,
            [this](int pos) { m_filterAWidget->setFilterPosition(pos); });
    connect(m_radioState, &RadioState::filterPositionBChanged, this,
            [this](int pos) { m_filterBWidget->setFilterPosition(pos); });

    // Filter bandwidth and shift → FilterIndicatorWidget shape
    connect(m_radioState, &RadioState::filterBandwidthChanged, this,
            [this](int bw) { m_filterAWidget->setBandwidth(bw); });
    connect(m_radioState, &RadioState::filterBandwidthBChanged, this,
            [this](int bw) { m_filterBWidget->setBandwidth(bw); });
    connect(m_radioState, &RadioState::ifShiftChanged, this, [this](int shift) { m_filterAWidget->setShift(shift); });
    connect(m_radioState, &RadioState::ifShiftBChanged, this, [this](int shift) { m_filterBWidget->setShift(shift); });
    // Mode affects filter indicator shift center calculation
    connect(m_radioState, &RadioState::modeChanged, this,
            [this](RadioState::Mode mode) { m_filterAWidget->setMode(RadioState::modeToString(mode)); });
    connect(m_radioState, &RadioState::modeBChanged, this,
            [this](RadioState::Mode mode) { m_filterBWidget->setMode(RadioState::modeToString(mode)); });

    // RadioState signals -> Processing state updates (AGC, PRE, ATT, NB, NR)
    connect(m_radioState, &RadioState::processingChanged, this, &MainWindow::onProcessingChanged);
    connect(m_radioState, &RadioState::processingChangedB, this, &MainWindow::onProcessingChangedB);
    // Auto-notch is reported on dedicated signals rather than the general
    // processing signal. Complete feedback from the K4's echoed state.
    connect(m_radioState, &RadioState::notchChanged, this, [this]() {
        if (!m_radioState->bSetEnabled())
            completeControlFeedback("NTCH", m_radioState->autoNotchEnabled()
                                                ? "AUTO NOTCH ON" : "AUTO NOTCH OFF");
    });
    connect(m_radioState, &RadioState::notchBChanged, this, [this]() {
        if (m_radioState->bSetEnabled())
            completeControlFeedback("NTCH", m_radioState->autoNotchEnabledB()
                                                ? "SUB AUTO NOTCH ON" : "SUB AUTO NOTCH OFF");
    });

    // RadioState signals -> MAIN RX / SUB RX popup button label updates
    // AFX button: primary = "AFX ON/OFF", alternate = mode (DELAY/PITCH/OFF)
    connect(m_radioState, &RadioState::afxModeChanged, this, [this](int mode) {
        QString primary = (mode == 0) ? "AFX OFF" : "AFX ON";
        QString alternate;
        switch (mode) {
        case 0:
            alternate = "OFF";
            break;
        case 1:
            alternate = "DELAY";
            break;
        case 2:
            alternate = "PITCH";
            break;
        }
        if (m_mainRxPopup)
            m_mainRxPopup->setButtonLabel(3, primary, alternate);
        if (m_subRxPopup)
            m_subRxPopup->setButtonLabel(3, primary, alternate);
    });

    // AGC button: primary = speed (AGC-S/AGC-F), alternate = ON/OFF
    connect(m_radioState, &RadioState::processingChanged, this, [this]() {
        QString primary;
        QString alternate;
        switch (m_radioState->agcSpeed()) {
        case RadioState::AGC_Off:
            primary = "AGC";
            alternate = "OFF";
            break;
        case RadioState::AGC_Slow:
            primary = "AGC-S";
            alternate = "ON";
            break;
        case RadioState::AGC_Fast:
            primary = "AGC-F";
            alternate = "ON";
            break;
        }
        if (m_mainRxPopup)
            m_mainRxPopup->setButtonLabel(4, primary, alternate);
    });

    connect(m_radioState, &RadioState::processingChangedB, this, [this]() {
        QString primary;
        QString alternate;
        switch (m_radioState->agcSpeedB()) {
        case RadioState::AGC_Off:
            primary = "AGC";
            alternate = "OFF";
            break;
        case RadioState::AGC_Slow:
            primary = "AGC-S";
            alternate = "ON";
            break;
        case RadioState::AGC_Fast:
            primary = "AGC-F";
            alternate = "ON";
            break;
        }
        if (m_subRxPopup)
            m_subRxPopup->setButtonLabel(4, primary, alternate);
    });

    // APF button: Main RX APF state -> MAIN RX popup and VFO A indicator
    connect(m_radioState, &RadioState::apfChanged, this, [this](bool enabled, int width) {
        QString alternate;
        if (!enabled) {
            alternate = "OFF";
        } else {
            static const char *bwNames[] = {"30Hz", "50Hz", "150Hz"};
            alternate = bwNames[qBound(0, width, 2)];
        }
        Q_UNUSED(alternate);
        refreshRxPopupsForModes();
        m_vfoA->setApf(enabled, width);
    });

    // APF button: Sub RX APF state -> SUB RX popup and VFO B indicator
    connect(m_radioState, &RadioState::apfBChanged, this, [this](bool enabled, int width) {
        QString alternate;
        if (!enabled) {
            alternate = "OFF";
        } else {
            static const char *bwNames[] = {"30Hz", "50Hz", "150Hz"};
            alternate = bwNames[qBound(0, width, 2)];
        }
        Q_UNUSED(alternate);
        refreshRxPopupsForModes();
        m_vfoB->setApf(enabled, width);
    });

    // RadioState REF level -> Panadapter (for dynamic waterfall color scaling)
    connect(m_radioState, &RadioState::refLevelChanged, this, [this](int level) { m_panadapterA->setRefLevel(level); });
    connect(m_radioState, &RadioState::refLevelBChanged, this,
            [this](int level) { m_panadapterB->setRefLevel(level); });

    // RadioState scale -> Panadapter (for display gain/range adjustment)
    // Note: #SCL is a GLOBAL setting - applies to both panadapters (no #SCL$ variant exists)
    connect(m_radioState, &RadioState::scaleChanged, this, [this](int scale) {
        m_panadapterA->setScale(scale);
        m_panadapterB->setScale(scale);
    });

    // RadioState span -> Panadapter (for frequency labels and bin extraction)
    connect(m_radioState, &RadioState::spanChanged, this, [this](int spanHz) { m_panadapterA->setSpan(spanHz); });
    connect(m_radioState, &RadioState::spanBChanged, this, [this](int spanHz) { m_panadapterB->setSpan(spanHz); });

    // RadioState waterfall height -> Panadapter (global setting applies to both)
    connect(m_radioState, &RadioState::waterfallHeightChanged, this, [this](int percent) {
        // Retain the intentional 50/50 phone starting point through the
        // initial radio-state dump.  Thereafter this follows WTR HEIGHT
        // normally, including the +/- controls in the DISP popup.
        if (K4Styles::isCompactLayout() && !m_phoneWaterfallHeightAdjusted)
            return;
        m_panadapterA->setWaterfallHeight(percent);
        m_panadapterB->setWaterfallHeight(percent);
    });

    // RadioState display state -> DisplayPopup (for button face updates)
    // Separate LCD and EXT signals
    connect(m_radioState, &RadioState::dualPanModeLcdChanged, m_displayPopup, &DisplayPopupWidget::setDualPanModeLcd);
    connect(m_radioState, &RadioState::dualPanModeExtChanged, m_displayPopup, &DisplayPopupWidget::setDualPanModeExt);

    // RadioState dual pan mode -> Panadapter widget visibility
    // Sync app's panadapter display with radio's #DPM mode
    connect(m_radioState, &RadioState::dualPanModeLcdChanged, this, [this](int mode) {
        switch (mode) {
        case 0: // A only
            setPanadapterMode(PanadapterMode::MainOnly);
            break;
        case 1: // B only
            setPanadapterMode(PanadapterMode::SubOnly);
            break;
        case 2: // Dual (A+B)
            setPanadapterMode(PanadapterMode::Dual);
            break;
        }
    });
    connect(m_radioState, &RadioState::displayModeLcdChanged, m_displayPopup, &DisplayPopupWidget::setDisplayModeLcd);
    connect(m_radioState, &RadioState::displayModeExtChanged, m_displayPopup, &DisplayPopupWidget::setDisplayModeExt);
    connect(m_radioState, &RadioState::waterfallColorChanged, m_displayPopup, &DisplayPopupWidget::setWaterfallColor);
    connect(m_radioState, &RadioState::waterfallColorChanged, this, [this](int color) {
        m_panadapterA->setWaterfallColor(color);
        m_panadapterB->setWaterfallColor(color);
    });
    connect(m_radioState, &RadioState::averagingChanged, m_displayPopup, &DisplayPopupWidget::setAveraging);
    connect(m_radioState, &RadioState::peakModeChanged, this, [this](bool enabled) {
        // #PKM controls both the radio's peak trace and the locally rendered
        // panadapter.  The popup label was previously updated, but the two
        // local renderers remained at their default peak-hold setting.
        m_displayPopup->setPeakMode(enabled);
        m_panadapterA->setPeakHoldEnabled(enabled);
        m_panadapterB->setPeakHoldEnabled(enabled);
    });
    connect(m_displayPopup, &DisplayPopupWidget::peakModeLocallyChanged, this, [this](bool enabled) {
        m_panadapterA->setPeakHoldEnabled(enabled);
        m_panadapterB->setPeakHoldEnabled(enabled);
    });
    // The radio can report #PKM during the initial handshake before these
    // UI signal connections are installed.  Seed both renderers from the
    // already-authoritative RadioState so the local red trace cannot lag the
    // PEAK ON label in the DISP popup.
    m_panadapterA->setPeakHoldEnabled(m_radioState->peakMode());
    m_panadapterB->setPeakHoldEnabled(m_radioState->peakMode());
    connect(m_radioState, &RadioState::fixedTuneChanged, m_displayPopup, &DisplayPopupWidget::setFixedTuneMode);
    connect(m_radioState, &RadioState::freezeChanged, m_displayPopup, &DisplayPopupWidget::setFreeze);
    connect(m_radioState, &RadioState::vfoACursorChanged, m_displayPopup, &DisplayPopupWidget::setVfoACursor);
    connect(m_radioState, &RadioState::vfoBCursorChanged, m_displayPopup, &DisplayPopupWidget::setVfoBCursor);
    // Each panadapter renders its own VFO as the primary cursor and the other
    // VFO as the secondary cursor. This keeps CURS B visible on panadapter A
    // whenever the B-only panadapter is hidden.
    connect(m_radioState, &RadioState::vfoACursorChanged, this, [this](int mode) {
        const bool visible = mode == 1 || mode == 2;
        m_panadapterA->setCursorVisible(visible);
        m_panadapterB->setSecondaryVisible(visible);
    });
    connect(m_radioState, &RadioState::vfoBCursorChanged, this, [this](int mode) {
        const bool visible = mode == 1 || mode == 2;
        m_panadapterB->setCursorVisible(visible);
        m_panadapterA->setSecondaryVisible(visible);
    });
    connect(m_radioState, &RadioState::autoRefLevelChanged, m_displayPopup, &DisplayPopupWidget::setAutoRefLevel);
    connect(m_radioState, &RadioState::scaleChanged, m_displayPopup, &DisplayPopupWidget::setScale);
    connect(m_radioState, &RadioState::ddcNbModeChanged, m_displayPopup, &DisplayPopupWidget::setDdcNbMode);
    connect(m_radioState, &RadioState::ddcNbLevelChanged, m_displayPopup, &DisplayPopupWidget::setDdcNbLevel);
    connect(m_radioState, &RadioState::waterfallHeightChanged, this, [this](int percent) {
        m_displayPopup->setWaterfallHeight(K4Styles::isCompactLayout() && !m_phoneWaterfallHeightAdjusted
                                                ? m_phoneWaterfallHeight
                                                : percent);
    });
    connect(m_radioState, &RadioState::waterfallHeightExtChanged, m_displayPopup,
            &DisplayPopupWidget::setWaterfallHeightExt);
    // Also update span/ref values in popup
    connect(m_radioState, &RadioState::spanChanged, this, [this](int spanHz) {
        m_displayPopup->setSpanValueA(spanHz / 1000.0); // Hz to kHz
    });
    connect(m_radioState, &RadioState::spanBChanged, this, [this](int spanHz) {
        m_displayPopup->setSpanValueB(spanHz / 1000.0); // Hz to kHz
    });
    connect(m_radioState, &RadioState::refLevelChanged, m_displayPopup, &DisplayPopupWidget::setRefLevelValueA);
    connect(m_radioState, &RadioState::refLevelBChanged, m_displayPopup, &DisplayPopupWidget::setRefLevelValueB);

    // Averaging control +/- -> CAT commands (range 1-20, step by 1)
    connect(m_displayPopup, &DisplayPopupWidget::averagingIncrementRequested, this, [this]() {
        int current = m_radioState->averaging();
        int next = qMin(current + 1, 20);
        m_radioState->setAveraging(next); // Optimistic update
        m_tcpClient->sendCAT(QString("#AVG%1;").arg(next, 2, 10, QChar('0')));
    });
    connect(m_displayPopup, &DisplayPopupWidget::averagingDecrementRequested, this, [this]() {
        int current = m_radioState->averaging();
        int next = qMax(current - 1, 1);
        m_radioState->setAveraging(next); // Optimistic update
        m_tcpClient->sendCAT(QString("#AVG%1;").arg(next, 2, 10, QChar('0')));
    });

    // DDC NB level control +/- -> CAT commands
    connect(m_displayPopup, &DisplayPopupWidget::nbToggleRequested, this, [this]() {
        // This is the panadapter NB shown beside the WTR CLRS controls, not
        // the receiver's generic NB switch. The K4 DDC blanker has all three
        // OFF/ON/AUTO modes, so cycle through them rather than reducing it to
        // a two-state toggle.
        const int current = m_radioState->ddcNbMode();
        const int next = current < 0 ? 0 : (current + 1) % 3;
        static const char *modeNames[] = {"OFF", "ON", "AUTO"};
        showControlFeedback(QString("PAN NB %1").arg(modeNames[next]));
        const QString command = QString("#NB$%1;").arg(next);
        m_tcpClient->sendCAT(command);
        m_radioState->parseCATCommand(command);
    });
    connect(m_displayPopup, &DisplayPopupWidget::nbLevelIncrementRequested, this, [this]() {
        int current = m_radioState->ddcNbLevel();
        int next = qMin(current + 1, 14);
        const QString command = QString("#NBL$%1;").arg(next, 2, 10, QChar('0'));
        m_tcpClient->sendCAT(command);
        m_radioState->parseCATCommand(command);
    });
    connect(m_displayPopup, &DisplayPopupWidget::nbLevelDecrementRequested, this, [this]() {
        int current = m_radioState->ddcNbLevel();
        int next = qMax(current - 1, 0);
        const QString command = QString("#NBL$%1;").arg(next, 2, 10, QChar('0'));
        m_tcpClient->sendCAT(command);
        m_radioState->parseCATCommand(command);
    });

    // Waterfall height control +/- -> CAT commands (respects LCD/EXT selection)
    // LCD controls our app's panadapter, EXT is just for external HDMI display
    connect(m_displayPopup, &DisplayPopupWidget::waterfallHeightIncrementRequested, this, [this]() {
        bool isExt = m_displayPopup->isExtEnabled() && !m_displayPopup->isLcdEnabled();
        int current = isExt ? m_radioState->waterfallHeightExt()
                            : (K4Styles::isCompactLayout() ? m_phoneWaterfallHeight : m_radioState->waterfallHeight());
        int next = qMin(current + 1, 90); // 1% steps, max 90%
        QString cmd =
            isExt ? QString("#HWFH%1;").arg(next, 2, 10, QChar('0')) : QString("#WFH%1;").arg(next, 2, 10, QChar('0'));
        m_tcpClient->sendCAT(cmd);
        // Optimistically update RadioState and UI (K4 may not echo this command)
        if (!isExt) {
            m_phoneWaterfallHeight = next;
            m_phoneWaterfallHeightAdjusted = true;
            m_radioState->setWaterfallHeight(next);
            m_panadapterA->setWaterfallHeight(next);
            m_panadapterB->setWaterfallHeight(next);
            m_displayPopup->setWaterfallHeight(next);
        } else {
            m_radioState->setWaterfallHeightExt(next);
            m_displayPopup->setWaterfallHeightExt(next);
        }
    });
    connect(m_displayPopup, &DisplayPopupWidget::waterfallHeightDecrementRequested, this, [this]() {
        bool isExt = m_displayPopup->isExtEnabled() && !m_displayPopup->isLcdEnabled();
        int current = isExt ? m_radioState->waterfallHeightExt()
                            : (K4Styles::isCompactLayout() ? m_phoneWaterfallHeight : m_radioState->waterfallHeight());
        int next = qMax(current - 1, 10); // 1% steps, min 10%
        QString cmd =
            isExt ? QString("#HWFH%1;").arg(next, 2, 10, QChar('0')) : QString("#WFH%1;").arg(next, 2, 10, QChar('0'));
        m_tcpClient->sendCAT(cmd);
        // Optimistically update RadioState and UI (K4 may not echo this command)
        if (!isExt) {
            m_phoneWaterfallHeight = next;
            m_phoneWaterfallHeightAdjusted = true;
            m_radioState->setWaterfallHeight(next);
            m_panadapterA->setWaterfallHeight(next);
            m_panadapterB->setWaterfallHeight(next);
            m_displayPopup->setWaterfallHeight(next);
        } else {
            m_radioState->setWaterfallHeightExt(next);
            m_displayPopup->setWaterfallHeightExt(next);
        }
    });

    // Span control from display popup -> CAT commands (respects A/B selection)
    // Match QK4/K4 convention: + increases span and - reduces it.
    connect(m_displayPopup, &DisplayPopupWidget::spanIncrementRequested, this, [this]() {
        bool vfoA = m_displayPopup->isVfoAEnabled();
        bool vfoB = m_displayPopup->isVfoBEnabled();
        int currentSpan = (vfoB && !vfoA) ? m_radioState->spanHzB() : m_radioState->spanHz();
        int newSpan = getNextSpanUp(currentSpan); // + increases span
        if (newSpan != currentSpan) {
            if (vfoA) {
                m_radioState->setSpanHz(newSpan);
                m_tcpClient->sendCAT(QString("#SPN%1;").arg(newSpan));
            }
            if (vfoB) {
                m_radioState->setSpanHzB(newSpan);
                m_tcpClient->sendCAT(QString("#SPN$%1;").arg(newSpan));
            }
        }
    });
    connect(m_displayPopup, &DisplayPopupWidget::spanDecrementRequested, this, [this]() {
        bool vfoA = m_displayPopup->isVfoAEnabled();
        bool vfoB = m_displayPopup->isVfoBEnabled();
        int currentSpan = (vfoB && !vfoA) ? m_radioState->spanHzB() : m_radioState->spanHz();
        int newSpan = getNextSpanDown(currentSpan); // - reduces span
        if (newSpan != currentSpan) {
            if (vfoA) {
                m_radioState->setSpanHz(newSpan);
                m_tcpClient->sendCAT(QString("#SPN%1;").arg(newSpan));
            }
            if (vfoB) {
                m_radioState->setSpanHzB(newSpan);
                m_tcpClient->sendCAT(QString("#SPN$%1;").arg(newSpan));
            }
        }
    });

    // Scale control (GLOBAL - affects both panadapters, no A/B variants)
    connect(m_displayPopup, &DisplayPopupWidget::scaleIncrementRequested, this, [this]() {
        int currentScale = m_radioState->scale();
        if (currentScale < 0)
            currentScale = 75;                      // Default if not yet received
        int newScale = qMin(currentScale + 1, 150); // Increment by 1, max 150
        if (newScale != currentScale) {
            m_tcpClient->sendCAT(QString("#SCL%1;").arg(newScale));
            // Optimistic update (scale is global, may not echo back)
            m_radioState->setScale(newScale); // Also updates panadapters via signal
        }
    });
    connect(m_displayPopup, &DisplayPopupWidget::scaleDecrementRequested, this, [this]() {
        int currentScale = m_radioState->scale();
        if (currentScale < 0)
            currentScale = 75;                     // Default if not yet received
        int newScale = qMax(currentScale - 1, 10); // Decrement by 1, min 10
        if (newScale != currentScale) {
            m_tcpClient->sendCAT(QString("#SCL%1;").arg(newScale));
            // Optimistic update (scale is global, may not echo back)
            m_radioState->setScale(newScale); // Also updates panadapters via signal
        }
    });

    // Ref level control (LCD only for now, respects A/B selection)
    // #REF for Main RX, #REF$ for Sub RX - absolute values from -200 to 60
    connect(m_displayPopup, &DisplayPopupWidget::refLevelIncrementRequested, this, [this]() {
        bool vfoA = m_displayPopup->isVfoAEnabled();
        bool vfoB = m_displayPopup->isVfoBEnabled();
        if (vfoA) {
            int currentLevel = m_radioState->refLevel();
            int newLevel = qMin(currentLevel + 1, 60); // Increment by 1 dB, max 60
            if (newLevel != currentLevel) {
                m_radioState->setRefLevel(newLevel);
                m_tcpClient->sendCAT(QString("#REF%1;").arg(newLevel));
            }
        }
        if (vfoB) {
            int currentLevel = m_radioState->refLevelB();
            int newLevel = qMin(currentLevel + 1, 60);
            if (newLevel != currentLevel) {
                m_radioState->setRefLevelB(newLevel);
                m_tcpClient->sendCAT(QString("#REF$%1;").arg(newLevel));
            }
        }
    });
    connect(m_displayPopup, &DisplayPopupWidget::refLevelDecrementRequested, this, [this]() {
        bool vfoA = m_displayPopup->isVfoAEnabled();
        bool vfoB = m_displayPopup->isVfoBEnabled();
        if (vfoA) {
            int currentLevel = m_radioState->refLevel();
            int newLevel = qMax(currentLevel - 1, -200); // Decrement by 1 dB, min -200
            if (newLevel != currentLevel) {
                m_radioState->setRefLevel(newLevel);
                m_tcpClient->sendCAT(QString("#REF%1;").arg(newLevel));
            }
        }
        if (vfoB) {
            int currentLevel = m_radioState->refLevelB();
            int newLevel = qMax(currentLevel - 1, -200);
            if (newLevel != currentLevel) {
                m_radioState->setRefLevelB(newLevel);
                m_tcpClient->sendCAT(QString("#REF$%1;").arg(newLevel));
            }
        }
    });

    // Protocol spectrum data -> Panadapter
    connect(m_tcpClient->protocol(), &Protocol::spectrumDataReady, this, &MainWindow::onSpectrumData);
    connect(m_tcpClient->protocol(), &Protocol::miniSpectrumDataReady, this, &MainWindow::onMiniSpectrumData);

    // Protocol audio data -> direct decode + enqueue on I/O thread (bypasses main thread entirely)
    // m_opusDecoder is only called from this lambda → single-threaded access on I/O thread
    // m_audioEngine->enqueueAudio() is mutex-protected → safe from any thread
    connect(m_tcpClient->protocol(), &Protocol::audioDataReady, m_tcpClient->protocol(),
            [this](const QByteArray &payload) {
                QByteArray pcmData = m_opusDecoder->decodeK4Packet(payload);
                if (!pcmData.isEmpty()) {
                    m_ft8Receiver->enqueue(pcmData, QDateTime::currentMSecsSinceEpoch());
                    if (m_sstvRxArmed.load(std::memory_order_acquire)) {
                        QMetaObject::invokeMethod(m_sstvDecoder, "consumeStereoFloat", Qt::QueuedConnection,
                                                  Q_ARG(QByteArray, pcmData));
                    }
                    m_audioEngine->enqueueAudio(pcmData);
                }
            });

    // Clock timer for date/time display
    connect(m_clockTimer, &QTimer::timeout, this, &MainWindow::updateDateTime);
    m_clockTimer->start(1000);
    updateDateTime();

    // KPOD device
    m_kpodDevice = new KpodDevice(this);

    // Connect KPOD signals
    connect(m_kpodDevice, &KpodDevice::encoderRotated, this, &MainWindow::onKpodEncoderRotated);
    connect(m_kpodDevice, &KpodDevice::rockerPositionChanged, this,
            [this](KpodDevice::RockerPosition pos) { onKpodRockerChanged(static_cast<int>(pos)); });
    connect(m_kpodDevice, &KpodDevice::pollError, this, &MainWindow::onKpodPollError);

    // Connect KPOD button signals to macro execution
    connect(m_kpodDevice, &KpodDevice::buttonTapped, this, [this](int buttonNum) {
        QString functionId = QString("K-pod.%1T").arg(buttonNum);
        executeMacro(functionId);
    });
    connect(m_kpodDevice, &KpodDevice::buttonHeld, this, [this](int buttonNum) {
        QString functionId = QString("K-pod.%1H").arg(buttonNum);
        executeMacro(functionId);
    });

    // Connect KPOD hotplug signals - auto-start polling when device arrives
    connect(m_kpodDevice, &KpodDevice::deviceConnected, this, [this]() {
        if (RadioSettings::instance()->kpodEnabled() && !m_kpodDevice->isPolling()) {
            m_kpodDevice->startPolling();
        }
    });

    // Connect to settings for KPOD enable/disable
    connect(RadioSettings::instance(), &RadioSettings::kpodEnabledChanged, this, &MainWindow::onKpodEnabledChanged);

    // Start KPOD polling if enabled and detected
    if (RadioSettings::instance()->kpodEnabled() && m_kpodDevice->isDetected()) {
        m_kpodDevice->startPolling();
    }

    // Keep the v1.0.3 CW connection as Android MIDI session 0. CTR2-MIDI has
    // a separate connection object backed by session 1; neither setup can
    // overwrite the other device selection or mapping.
    m_halikeyDevice = new HalikeyDevice(this);
    m_ctr2MidiDevice = new Ctr2MidiDevice(this);
    m_midiInputRouter = new MidiInputRouter(this);

    const auto loadCtr2Mapping = [this]() {
        MidiMapping::DeviceMapping mapping = MidiMapping::ctr2Default();
        const QByteArray json = RadioSettings::instance()->ctr2MidiMappingJson();
        if (!json.isEmpty()) {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
            MidiMapping::DeviceMapping stored;
            QString mappingError;
            if (parseError.error == QJsonParseError::NoError && document.isObject()
                && MidiMapping::fromJson(document.object(), &stored, &mappingError)
                && stored.profile == MidiMapping::Profile::Ctr2) {
                mapping = stored;
            } else {
                qWarning() << "Ignoring invalid saved CTR2-MIDI mapping:" << mappingError;
            }
        }
        m_midiInputRouter->setMapping(QStringLiteral("ctr2-midi"), mapping);
    };
    loadCtr2Mapping();
    connect(RadioSettings::instance(), &RadioSettings::ctr2MidiMappingChanged,
            this, loadCtr2Mapping);

    // Feed the logical CW edges from the unchanged CW device path and raw
    // CTR2 events into one source-aware aggregator. Releasing/disconnecting
    // either device cannot release an input that the other device still holds.
    connect(m_halikeyDevice, &HalikeyDevice::ditStateChanged, this, [this](bool pressed) {
        m_midiInputRouter->setLogicalInput(QStringLiteral("cw-midi"),
                                           MidiMapping::LogicalInput::Dit, pressed);
    });
    connect(m_halikeyDevice, &HalikeyDevice::dahStateChanged, this, [this](bool pressed) {
        m_midiInputRouter->setLogicalInput(QStringLiteral("cw-midi"),
                                           MidiMapping::LogicalInput::Dah, pressed);
    });
    connect(m_halikeyDevice, &HalikeyDevice::straightKeyStateChanged, this, [this](bool pressed) {
        m_midiInputRouter->setLogicalInput(QStringLiteral("cw-midi"),
                                           MidiMapping::LogicalInput::StraightKey, pressed);
    });
    connect(m_halikeyDevice, &HalikeyDevice::pttStateChanged, this, [this](bool pressed) {
        m_midiInputRouter->setLogicalInput(QStringLiteral("cw-midi"),
                                           MidiMapping::LogicalInput::Ptt, pressed);
    });
    connect(m_halikeyDevice, &HalikeyDevice::disconnected, this, [this]() {
        m_midiInputRouter->clearSourceState(QStringLiteral("cw-midi"));
        if (!m_midiInputRouter->logicalInputActive(MidiMapping::LogicalInput::Dit)
            && !m_midiInputRouter->logicalInputActive(MidiMapping::LogicalInput::Dah)
            && m_iambicKeyer) {
            QMetaObject::invokeMethod(m_iambicKeyer, "stop", Qt::QueuedConnection);
        }
    });
    connect(m_ctr2MidiDevice, &Ctr2MidiDevice::rawMidiEvent, this,
            [this](int status, int data1, int data2) {
                m_midiInputRouter->processEvent(QStringLiteral("ctr2-midi"), status, data1, data2);
            });
    connect(m_ctr2MidiDevice, &Ctr2MidiDevice::disconnected, this, [this]() {
        m_midiInputRouter->clearSourceState(QStringLiteral("ctr2-midi"));
        if (!m_midiInputRouter->logicalInputActive(MidiMapping::LogicalInput::Dit)
            && !m_midiInputRouter->logicalInputActive(MidiMapping::LogicalInput::Dah)
            && m_iambicKeyer) {
            QMetaObject::invokeMethod(m_iambicKeyer, "stop", Qt::QueuedConnection);
        }
    });

    // Match current upstream QK4: raw paddle edges feed a high-priority local
    // iambic keyer. It aligns alternating paddles to element boundaries and
    // emits the complete K4 KZ stream instead of treating every edge as an
    // unrelated one-shot command.
    m_iambicKeyer = new IambicKeyer(nullptr);
    m_keyerThread = new QThread(this);
    m_keyerThread->setObjectName("CW Keyer");
    m_iambicKeyer->moveToThread(m_keyerThread);
    m_keyerThread->start(QThread::HighPriority);
    const int initialWpm = m_radioState->keyerSpeed() > 0 ? m_radioState->keyerSpeed() : 20;
    QMetaObject::invokeMethod(m_iambicKeyer, "setSpeed", Qt::QueuedConnection, Q_ARG(int, initialWpm));
    QMetaObject::invokeMethod(m_iambicKeyer, "setReversed", Qt::QueuedConnection,
                              Q_ARG(bool, RadioSettings::instance()->cwPaddlesReversed()));
    QMetaObject::invokeMethod(m_iambicKeyer, "setMode", Qt::QueuedConnection,
                              Q_ARG(IambicKeyer::Mode, m_radioState->iambicMode() == 'B'
                                                           ? IambicKeyer::IambicB : IambicKeyer::IambicA));

    // Local sidetone generator for CW keying (low-latency local audio feedback)
    // MUST be created BEFORE HaliKey signal connections that use it
    m_sidetoneGenerator = new SidetoneGenerator(nullptr);
    m_sidetoneThread = new QThread(this);
    m_sidetoneThread->setObjectName("Sidetone");
    m_sidetoneGenerator->moveToThread(m_sidetoneThread);
    m_sidetoneThread->start();
    QMetaObject::invokeMethod(m_sidetoneGenerator, "start", Qt::QueuedConnection);

    // Set initial sidetone frequency from radio state if available
    if (m_radioState->cwPitch() > 0) {
        m_sidetoneGenerator->setFrequency(m_radioState->cwPitch());
    }

    // Update sidetone frequency when CW pitch changes
    connect(m_radioState, &RadioState::cwPitchChanged, this,
            [this](int pitchHz) { m_sidetoneGenerator->setFrequency(pitchHz); });

    // Set initial sidetone volume from RadioSettings (independent of K4's MON level)
    m_sidetoneGenerator->setVolume(RadioSettings::instance()->sidetoneVolume() / 100.0f);

    // Update sidetone volume when changed in Options
    connect(RadioSettings::instance(), &RadioSettings::sidetoneVolumeChanged, this,
            [this](int value) { m_sidetoneGenerator->setVolume(value / 100.0f); });

    // Set initial keyer speed from radio state if available
    if (m_radioState->keyerSpeed() > 0) {
        m_sidetoneGenerator->setKeyerSpeed(m_radioState->keyerSpeed());
    }

    connect(m_radioState, &RadioState::keyerSpeedChanged, this, [this](int wpm) {
        m_sidetoneGenerator->setKeyerSpeed(wpm);
        QMetaObject::invokeMethod(m_iambicKeyer, "setSpeed", Qt::QueuedConnection, Q_ARG(int, wpm));
        if (m_tcpClient->isConnected())
            m_tcpClient->sendCAT(QString("KZL%1;").arg(1200 / wpm, 2, 10, QChar('0')));
    });

    connect(m_midiInputRouter, &MidiInputRouter::ditStateChanged, m_iambicKeyer,
            [this](bool pressed) { m_iambicKeyer->setDitPaddle(pressed); }, Qt::DirectConnection);
    connect(m_midiInputRouter, &MidiInputRouter::dahStateChanged, m_iambicKeyer,
            [this](bool pressed) { m_iambicKeyer->setDahPaddle(pressed); }, Qt::DirectConnection);
    connect(m_midiInputRouter, &MidiInputRouter::straightKeyStateChanged, this, [this](bool pressed) {
        if (m_tcpClient->isConnected())
            m_tcpClient->sendCAT(pressed ? QStringLiteral("KZD0000;")
                                         : QStringLiteral("KZU0000;"));
        QMetaObject::invokeMethod(m_sidetoneGenerator,
                                  pressed ? "startStraightKey" : "stopStraightKey",
                                  Qt::QueuedConnection);
    });
    connect(m_midiInputRouter, &MidiInputRouter::pttStateChanged, this,
            [this](bool pressed) { pressed ? onPttPressed() : onPttReleased(); });
    connect(m_midiInputRouter, &MidiInputRouter::knobActionRequested,
            this, &MainWindow::handleMidiKnobAction);
    connect(m_midiInputRouter, &MidiInputRouter::buttonActionRequested,
            this, &MainWindow::handleMidiButtonAction);
    connect(m_midiInputRouter, &MidiInputRouter::macroRequested, this,
            [this](const QString &, const QString &command) { executeMidiMacro(command); });
    connect(m_iambicKeyer, &IambicKeyer::elementStarted, m_tcpClient, [this](bool isDit) {
        if (m_tcpClient->isConnected())
            m_tcpClient->sendCAT(isDit ? "KZ.;" : "KZ-;");
    }, Qt::QueuedConnection);
    connect(m_iambicKeyer, &IambicKeyer::elementStarted, m_sidetoneGenerator, [this](bool isDit) {
        isDit ? m_sidetoneGenerator->playSingleDit() : m_sidetoneGenerator->playSingleDah();
    }, Qt::QueuedConnection);
    connect(m_iambicKeyer, &IambicKeyer::characterSpace, m_tcpClient, [this]() {
        if (m_tcpClient->isConnected())
            m_tcpClient->sendCAT("KZ ;");
    }, Qt::QueuedConnection);
    connect(m_iambicKeyer, &IambicKeyer::restartAfterPause, m_tcpClient, [this](int ms) {
        if (m_tcpClient->isConnected())
            m_tcpClient->sendCAT(QString("KZP%1;").arg(ms, 4, 10, QChar('0')));
    }, Qt::QueuedConnection);

    connect(m_radioState, &RadioState::keyerPaddleChanged, this, [this](QChar orientation) {
        const bool reversed = orientation == 'R';
        RadioSettings::instance()->setCwPaddlesReversed(reversed);
        QMetaObject::invokeMethod(m_iambicKeyer, "setReversed", Qt::QueuedConnection, Q_ARG(bool, reversed));
        refreshTxPopupForMode();
    });
    connect(m_radioState, &RadioState::iambicModeChanged, this, [this](QChar mode) {
        QMetaObject::invokeMethod(m_iambicKeyer, "setMode", Qt::QueuedConnection,
                                  Q_ARG(IambicKeyer::Mode, mode == 'B'
                                                               ? IambicKeyer::IambicB : IambicKeyer::IambicA));
        refreshTxPopupForMode();
    });

    // KPA1500 amplifier client
    m_kpa1500Client = new KPA1500Client(this);

    // Connect KPA1500 signals
    connect(m_kpa1500Client, &KPA1500Client::connected, this, &MainWindow::onKpa1500Connected);
    connect(m_kpa1500Client, &KPA1500Client::disconnected, this, &MainWindow::onKpa1500Disconnected);
    connect(m_kpa1500Client, &KPA1500Client::errorOccurred, this, &MainWindow::onKpa1500Error);

    // Connect KPA1500 data signals to panel
    connect(m_kpa1500Client, &KPA1500Client::powerChanged, this, [this](double fwd, double ref, double) {
        m_kpa1500Window->panel()->setForwardPower(static_cast<float>(fwd));
        m_kpa1500Window->panel()->setReflectedPower(static_cast<float>(ref));
    });
    connect(m_kpa1500Client, &KPA1500Client::swrChanged, this,
            [this](double swr) { m_kpa1500Window->panel()->setSWR(static_cast<float>(swr)); });
    connect(m_kpa1500Client, &KPA1500Client::paTemperatureChanged, this,
            [this](double tempC) { m_kpa1500Window->panel()->setTemperature(static_cast<float>(tempC)); });
    connect(m_kpa1500Client, &KPA1500Client::operatingStateChanged, this, [this](KPA1500Client::OperatingState state) {
        m_kpa1500Window->panel()->setMode(state == KPA1500Client::StateOperate);
    });
    connect(m_kpa1500Client, &KPA1500Client::atuInlineChanged, this,
            [this](bool inline_) { m_kpa1500Window->panel()->setAtuMode(inline_); });
    connect(m_kpa1500Client, &KPA1500Client::antennaChanged, this,
            [this](int antenna) { m_kpa1500Window->panel()->setAntenna(antenna); });
    connect(m_kpa1500Client, &KPA1500Client::faultStatusChanged, this,
            [this](KPA1500Client::FaultStatus status, const QString &) {
                // Only show FAULT for active faults, not fault history
                m_kpa1500Window->panel()->setFault(status == KPA1500Client::FaultActive);
            });

    // Connect panel signals to send KPA1500 commands
    connect(m_kpa1500Window->panel(), &KPA1500Panel::modeToggled, this,
            [this](bool operate) { m_kpa1500Client->sendCommand(operate ? "^OS1;" : "^OS0;"); });
    connect(m_kpa1500Window->panel(), &KPA1500Panel::atuTuneRequested, this,
            [this]() { m_kpa1500Client->sendCommand("^FT;"); });
    connect(m_kpa1500Window->panel(), &KPA1500Panel::atuModeToggled, this,
            [this](bool in) { m_kpa1500Client->sendCommand(in ? "^AI1;" : "^AI0;"); });
    connect(m_kpa1500Window->panel(), &KPA1500Panel::antennaChanged, this,
            [this](int ant) { m_kpa1500Client->sendCommand(QString("^AN%1;").arg(ant)); });

    // Connect to settings for KPA1500 enable/disable and settings changes
    connect(RadioSettings::instance(), &RadioSettings::kpa1500EnabledChanged, this,
            &MainWindow::onKpa1500EnabledChanged);
    connect(RadioSettings::instance(), &RadioSettings::kpa1500SettingsChanged, this,
            &MainWindow::onKpa1500SettingsChanged);

    // KPA1500 connects when K4 connects (in onAuthenticated), not on app start

    // Initialize KPA1500 status display
    updateKpa1500Status();

    // CAT server for external app integration (WSJT-X, MacLoggerDX, etc.)
    // Apps connect using their built-in K4 support - no protocol translation needed
    m_catServer = new CatServer(m_radioState, this);
    m_catServer->setTcpClient(m_tcpClient);

    // Forward CAT commands from external apps to the real K4
    connect(m_catServer, &CatServer::catCommandReceived, this,
            [this](const QString &command) {
        const QString normalized = command.trimmed().toUpper();
        if (m_sstvTxActive || m_sstvTxStarting || m_digitalCalibrating || m_ft8TxGeneration) {
            // SSTV exclusively owns TX. Do not let the generic CAT forwarder
            // bypass the pttRequested guard below.
            if (normalized.startsWith(QStringLiteral("TX")))
                return;
            if (normalized.startsWith(QStringLiteral("RX"))) {
                stopFt8Transmit();
                cancelDigitalCalibration();
                stopSstvTransmission();
                return;
            }
        }
        m_tcpClient->sendCAT(command);
    });

    // TX;/RX; from external apps controls audio input gate
    // Audio stream itself triggers K4 TX - timing-critical for FT8/FT4
    connect(m_catServer, &CatServer::pttRequested, this, [this](bool on) {
        if (on && (m_sstvTxActive || m_sstvTxStarting || m_digitalCalibrating || m_ft8TxGeneration))
            return;
        // Match AudioController in mainline: reject PTT-on while disconnected,
        // but always honor PTT-off so a stale gate can be cleared.
        if (on && !m_tcpClient->isConnected())
            return;
#ifdef Q_OS_ANDROID
        if (on && !ensureMicrophonePermission(this)) {
            m_pttActive = false;
            m_bottomMenuBar->setPttActive(false);
            QMetaObject::invokeMethod(m_audioEngine, "setPttActive", Qt::QueuedConnection, Q_ARG(bool, false));
            return;
        }
#endif
        m_pttActive = on;
        QMetaObject::invokeMethod(m_audioEngine, "setPttActive", Qt::QueuedConnection, Q_ARG(bool, on));
        m_bottomMenuBar->setPttActive(on);
    });

    // Connect to settings for CAT server enable/disable
    connect(RadioSettings::instance(), &RadioSettings::catServerEnabledChanged, this, [this](bool enabled) {
        if (enabled) {
            m_catServer->start(RadioSettings::instance()->catServerPort());
        } else {
            m_catServer->stop();
        }
    });
    connect(RadioSettings::instance(), &RadioSettings::catServerPortChanged, this, [this](quint16 port) {
        if (RadioSettings::instance()->catServerEnabled()) {
            m_catServer->stop();
            m_catServer->start(port);
        }
    });

    // Start CAT server if enabled
    if (RadioSettings::instance()->catServerEnabled()) {
        m_catServer->start(RadioSettings::instance()->catServerPort());
    }

#ifdef Q_OS_ANDROID
    // Prime Android runtime permission early, before the first TX attempt.
    ensureMicrophonePermission(this);
#endif

    // resize directly instead of deferring - testing if deferred resize affects QRhi
    // QTimer::singleShot(0, this, [this]() { resize(1340, 800); });
}

MainWindow::~MainWindow() {
    stopFt8Transmit();
    m_ft8Receiver->setCapture(false, Ft8::Mode::FT8);
    // SSTV owns a keyed program-audio path. Close its producer before the
    // network connection is torn down so shutdown cannot leave a queued tone
    // stream or a keyed K4 behind.
    if (m_audioEngine && m_audioThread) {
        m_audioEngine->requestSstvStop();
        QMetaObject::invokeMethod(m_audioEngine, "stopSstvTransmit", Qt::BlockingQueuedConnection);
    }
    if (m_tcpClient && m_ioThread) {
        QMetaObject::invokeMethod(m_tcpClient, "stopSstvAudioAndUnkey", Qt::BlockingQueuedConnection);
    }
    setSstvOrientationEnabled(false);

    // Close HaliKey FIRST — its closePort() emits disconnected(), which triggers
    // lambdas that call invokeMethod on m_sidetoneGenerator/m_tcpClient.
    // Must happen while those objects are still alive.
    m_sstvRxArmed.store(false, std::memory_order_release);
    if (m_sstvDecoderThread) {
        m_sstvDecoderThread->quit();
        m_sstvDecoderThread->wait(2000);
    }
    delete m_sstvDecoder;
    m_sstvDecoder = nullptr;

    if (m_halikeyDevice) {
        m_halikeyDevice->closePort();
    }
    if (m_ctr2MidiDevice) {
        m_ctr2MidiDevice->closePort();
    }

    // Stop the KZ producer before the network thread it targets.
    if (m_keyerThread) {
        QMetaObject::invokeMethod(m_iambicKeyer, "stop", Qt::BlockingQueuedConnection);
        m_keyerThread->quit();
        m_keyerThread->wait(2000);
    }
    delete m_iambicKeyer;

    // Shut down I/O thread first (stop producing audio before stopping consumer)
    if (m_ioThread) {
        QMetaObject::invokeMethod(m_tcpClient, "disconnectFromHost", Qt::BlockingQueuedConnection);
        m_ioThread->quit();
        m_ioThread->wait(2000);
    }
    delete m_tcpClient;   // No parent, must delete manually
    delete m_opusDecoder; // No parent, must delete manually

    // The I/O producer is stopped before its FT8 consumer is destroyed.
    m_ft8Thread->requestInterruption();
    m_ft8Thread->quit();
    m_ft8Thread->wait();

    // Shut down sidetone thread
    if (m_sidetoneThread) {
        QMetaObject::invokeMethod(m_sidetoneGenerator, "stop", Qt::BlockingQueuedConnection);
        m_sidetoneThread->quit();
        m_sidetoneThread->wait(2000);
    }
    delete m_sidetoneGenerator; // No parent, must delete manually

    // Shut down audio thread (consumer stops after producer)
    if (m_audioThread) {
        QMetaObject::invokeMethod(m_audioEngine, "stop", Qt::BlockingQueuedConnection);
        m_audioThread->quit();
        m_audioThread->wait(2000);
    }
    delete m_audioEngine; // No parent, must delete manually
    m_audioEngine = nullptr;

    // Disconnect KPA1500 signals before child destruction to prevent
    // callbacks accessing destroyed widgets during cleanup
    if (m_kpa1500Client) {
        disconnect(m_kpa1500Client, nullptr, this, nullptr);
        m_kpa1500Client->disconnectFromHost();
    }
}

void MainWindow::setupMenuBar() {
    // Standard menu bar order: File, Connect, Tools, View, Help
    // On macOS, Qt automatically creates the app menu with About/Preferences
    menuBar()->setStyleSheet(QString("QMenuBar { background-color: %1; color: %2; }"
                                     "QMenuBar::item:selected { background-color: #333; }")
                                 .arg(K4Styles::Colors::DarkBackground, K4Styles::Colors::TextWhite));

    // File menu (first, per Windows convention)
    QMenu *fileMenu = menuBar()->addMenu("&File");
    QAction *quitAction = new QAction("E&xit", this);
    quitAction->setMenuRole(QAction::QuitRole); // macOS: moves to app menu
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QMainWindow::close);
    fileMenu->addAction(quitAction);

    // Tools menu
    QMenu *toolsMenu = menuBar()->addMenu("&Tools");
    QAction *optionsAction = new QAction("&Settings...", this);
    optionsAction->setMenuRole(QAction::PreferencesRole); // macOS: moves to app menu as Preferences
    connect(optionsAction, &QAction::triggered, this, &MainWindow::showSettings);
    toolsMenu->addAction(optionsAction);

    // View menu
    QMenu *viewMenu = menuBar()->addMenu("&View");
    Q_UNUSED(viewMenu)

    // Help menu
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    QAction *aboutAction = new QAction("&About QK4 Mobile", this);
    aboutAction->setMenuRole(QAction::AboutRole); // macOS: moves to app menu
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);
    helpMenu->addAction(aboutAction);
}

void MainWindow::showSettings() {
    closeAllPopups();
    if (m_phoneControlsDialog) m_phoneControlsDialog->hide();
    if (!m_optionsDialog) {
        m_optionsDialog = new OptionsDialog(m_radioState, m_audioEngine, m_kpodDevice, m_catServer,
                                            m_halikeyDevice, m_ctr2MidiDevice, centralWidget());
        connect(m_optionsDialog, &OptionsDialog::keyerSpeedRequested, this, [this](int wpm) {
            const int boundedWpm = qBound(8, wpm, 40);
            m_tcpClient->sendCAT(QString("KS%1;").arg(boundedWpm, 3, 10, QChar('0')));
            m_radioState->setKeyerSpeed(boundedWpm);
        });
    }
#ifdef Q_OS_ANDROID
    m_optionsDialog->setGeometry(centralWidget()->rect());
#endif
    m_optionsDialog->show();
    m_optionsDialog->raise();
#ifndef Q_OS_ANDROID
    m_optionsDialog->activateWindow();
#else
    m_optionsDialog->setFocus(Qt::OtherFocusReason);
#endif
}

void MainWindow::showFeatureAdjustment(int featureValue) {
    if (!m_featureMenuBar || !m_bottomMenuBar)
        return;
    const auto feature = static_cast<FeatureMenuBar::Feature>(featureValue);
    if (m_phoneControlsDialog)
        m_phoneControlsDialog->hide();
    const bool alreadyVisible = m_featureMenuBar->isMenuVisible()
                                && m_featureMenuBar->currentFeature() == feature;
    if (m_featureMenuBar->isMenuVisible() && !alreadyVisible)
        m_featureMenuBar->hideMenu();

    const bool bSet = m_radioState->bSetEnabled();
    switch (feature) {
    case FeatureMenuBar::Attenuator:
        m_featureMenuBar->setFeatureEnabled(bSet ? m_radioState->attenuatorEnabledB()
                                                 : m_radioState->attenuatorEnabled());
        m_featureMenuBar->setValue(bSet ? m_radioState->attenuatorLevelB()
                                        : m_radioState->attenuatorLevel());
        break;
    case FeatureMenuBar::NbLevel:
        m_featureMenuBar->setFeatureEnabled(bSet ? m_radioState->noiseBlankerEnabledB()
                                                 : m_radioState->noiseBlankerEnabled());
        m_featureMenuBar->setValue(bSet ? m_radioState->noiseBlankerLevelB()
                                        : m_radioState->noiseBlankerLevel());
        m_featureMenuBar->setNbFilter(bSet ? m_radioState->noiseBlankerFilterWidthB()
                                           : m_radioState->noiseBlankerFilterWidth());
        break;
    case FeatureMenuBar::NrAdjust: {
        const bool lmsOn = bSet ? m_radioState->noiseReductionEnabledB()
                                : m_radioState->noiseReductionEnabled();
        const bool ssnrOn = bSet ? m_radioState->ssnrEnabledB()
                                 : m_radioState->ssnrEnabled();
        if (ssnrOn && !lmsOn)
            m_featureMenuBar->setNrEngine(FeatureMenuBar::Ssnr);
        else if (lmsOn && !ssnrOn)
            m_featureMenuBar->setNrEngine(FeatureMenuBar::Lms);
        if (m_featureMenuBar->currentNrEngine() == FeatureMenuBar::Ssnr) {
            m_featureMenuBar->setFeatureEnabled(ssnrOn);
            m_featureMenuBar->setValue(bSet ? m_radioState->ssnrLevelB()
                                            : m_radioState->ssnrLevel());
        } else {
            m_featureMenuBar->setFeatureEnabled(lmsOn);
            m_featureMenuBar->setValue(bSet ? m_radioState->noiseReductionLevelB()
                                            : m_radioState->noiseReductionLevel());
        }
        break;
    }
    case FeatureMenuBar::ManualNotch:
        m_featureMenuBar->setFeatureEnabled(bSet ? m_radioState->manualNotchEnabledB()
                                                 : m_radioState->manualNotchEnabled());
        m_featureMenuBar->setValue(bSet ? m_radioState->manualNotchPitchB()
                                        : m_radioState->manualNotchPitch());
        break;
    }
    if (!alreadyVisible) {
        m_featureMenuBar->showForFeature(feature);
        m_featureMenuBar->showAboveWidget(m_bottomMenuBar);
    } else {
        m_featureMenuBar->raise();
    }
}

void MainWindow::showAboutDialog() {
    showInWindowMessage(centralWidget(), "About QK4 Mobile",
                        QString("<h2>QK4 Mobile for Android</h2>"
                                "<p>Version %1</p>"
                                "<p>Remote control application for Elecraft K4 radios.</p>"
                                "<p>By <a href='https://worldwidedx.com'>WorldwideDX.com</a></p>"
                                "<p>Based on QK4 by Mike Garcia (KF5O).</p>"
                                "<p>Licensed under GNU GPL v3.0 or later.</p>"
                                "<p><a href='https://github.com/mikeg-dal/QK4'>QK4 source</a></p>")
                            .arg(QCoreApplication::applicationVersion()));
}

QString MainWindow::requestText(const QString &title, const QString &label, const QString &initial, bool *accepted) {
    // Popups are direct MainWindow children on Android. Keep this overlay at
    // the same parent level so raise() can place the editor above them.
    InWindowDialog dialog(this);
    QWidget *panel = dialog.contentWidget();
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(10);

    auto *titleLabel = new QLabel(title, panel);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(QString("color: %1; font-size: 17px; font-weight: bold;")
                                  .arg(K4Styles::Colors::AccentAmber));
    layout->addWidget(titleLabel);

    auto *prompt = new QLabel(label, panel);
    prompt->setStyleSheet(QString("color: %1; font-size: 13px;").arg(K4Styles::Colors::TextWhite));
    layout->addWidget(prompt);

    auto *entry = new QLineEdit(initial, panel);
    entry->setInputMethodHints(Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText);
    entry->setStyleSheet(QString("background: %1; color: %2; border: 1px solid %3; padding: 6px;")
                             .arg(K4Styles::Colors::DarkBackground, K4Styles::Colors::TextWhite,
                                  K4Styles::Colors::DialogBorder));
    layout->addWidget(entry);

    auto *buttons = new QHBoxLayout();
    auto *cancel = new QPushButton("CANCEL", panel);
    auto *save = new QPushButton("SAVE", panel);
    for (QPushButton *button : {cancel, save}) {
        button->setMinimumHeight(36);
        button->setStyleSheet(K4Styles::menuBarButton());
        buttons->addWidget(button, 1);
    }
    save->setStyleSheet(K4Styles::menuBarButtonActive());
    layout->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, &dialog, &InWindowDialog::reject);
    connect(save, &QPushButton::clicked, &dialog, &InWindowDialog::accept);
    connect(entry, &QLineEdit::returnPressed, &dialog, &InWindowDialog::accept);

    const int panelWidth = qMin(500, qMax(300, width() - 24));
    dialog.setPanelSize(QSize(panelWidth, qMin(220, height() - 16)));
    const bool wasAccepted = dialog.exec() == InWindowDialog::Accepted;
    if (accepted)
        *accepted = wasAccepted;
    return wasAccepted ? entry->text() : QString();
}

void MainWindow::setupUi() {
    setWindowTitle("QK4 Mobile");
    if (K4Styles::isCompactLayout()) {
        setMinimumSize(0, 0);
    } else {
        setMinimumSize(1340, 840);
        resize(1340, 840); // Default to minimum size on launch
    }

    // NOTE: Do NOT set WA_NativeWindow here!
    // Qt 6.10.1 bug on macOS Tahoe: WA_NativeWindow forces native window creation
    // before QRhiWidget can configure it for MetalSurface, causing
    // "QMetalSwapChain only supports MetalSurface windows" crash.

    setStyleSheet(QString("QMainWindow { background-color: %1; }").arg(K4Styles::Colors::Background));

    auto *centralWidget = new QWidget(this);
    centralWidget->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::Background));
    setCentralWidget(centralWidget);

    // Main vertical layout
    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Top status bar
    setupTopStatusBar(centralWidget);
    QWidget *topStatusBar = m_titleLabel->parentWidget();
    mainLayout->addWidget(topStatusBar);
    // This stays visible on phone layouts: it is the at-a-glance radio and
    // connection readout, not a desktop-only title bar.

    // Middle section: Side Panel + Main Content (L-shaped)
    auto *middleWidget = new QWidget(centralWidget);
    auto *middleLayout = new QHBoxLayout(middleWidget);
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->setSpacing(0);

    // Side Control Panel (left) in a scroll container for short phone screens
    const int sidePanelScrollExtra = K4Styles::isCompactLayout() ? 18 : 2;
    m_leftPanelScroll = new QScrollArea(middleWidget);
    m_leftPanelScroll->setFrameShape(QFrame::NoFrame);
    m_leftPanelScroll->setWidgetResizable(false);
    m_leftPanelScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_leftPanelScroll->setVerticalScrollBarPolicy(K4Styles::isCompactLayout() ? Qt::ScrollBarAlwaysOn
                                                                               : Qt::ScrollBarAlwaysOff);
    m_leftPanelScroll->setFixedWidth(K4Styles::Dimensions::SidePanelWidth + sidePanelScrollExtra);
    m_sideControlPanel = new SideControlPanel(m_leftPanelScroll);
    m_leftPanelScroll->setWidget(m_sideControlPanel);
    if (K4Styles::isCompactLayout()) {
        m_sideControlPanel->setMinimumHeight(m_sideControlPanel->sizeHint().height());
        m_sideControlPanel->resize(m_sideControlPanel->sizeHint());
        m_leftPanelScroll->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
        QScroller::grabGesture(m_leftPanelScroll->viewport(), QScroller::TouchGesture);
        QScroller::grabGesture(m_sideControlPanel, QScroller::TouchGesture);
    }
    if (K4Styles::isCompactLayout()) {
        // Phone controls live in the bottom touch dock; retain this panel as
        // the settings/state source without sacrificing the panadapter width.
        m_leftPanelScroll->hide();
    } else {
        middleLayout->addWidget(m_leftPanelScroll);
    }

    // Main content (VFO + Spectrum)
    auto *contentWidget = new QWidget(middleWidget);
    auto *contentLayout = new QVBoxLayout(contentWidget);
    // The phone console has a fixed two-row dock.  Avoid spending vertical
    // space on decorative gutters above/below the VFO row: that space is more
    // useful for the live spectrum and keeps every dock control reachable.
    const int contentVerticalMargin = K4Styles::isCompactLayout() ? 0 : K4Styles::Dimensions::PaddingSmall;
    contentLayout->setContentsMargins(K4Styles::Dimensions::PaddingSmall, contentVerticalMargin,
                                      K4Styles::Dimensions::PaddingSmall, contentVerticalMargin);
    contentLayout->setSpacing(K4Styles::isCompactLayout() ? 1 : 2);

    // VFO section (A | Center | B)
    auto *vfoWidget = new QWidget(contentWidget);
    setupVfoSection(vfoWidget);
    contentLayout->addWidget(vfoWidget);

    // Spectrum/Waterfall display
    setupSpectrumPlaceholder(contentWidget);
    contentLayout->addWidget(m_spectrumContainer, 1);

    middleLayout->addWidget(contentWidget, 1);

    // Right Side Panel (mirrors left panel dimensions)
    m_rightPanelScroll = new QScrollArea(middleWidget);
    m_rightPanelScroll->setFrameShape(QFrame::NoFrame);
    m_rightPanelScroll->setWidgetResizable(false);
    m_rightPanelScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_rightPanelScroll->setVerticalScrollBarPolicy(K4Styles::isCompactLayout() ? Qt::ScrollBarAlwaysOn
                                                                                : Qt::ScrollBarAlwaysOff);
    m_rightPanelScroll->setFixedWidth(K4Styles::Dimensions::SidePanelWidth + sidePanelScrollExtra);
    m_rightSidePanel = new RightSidePanel(m_rightPanelScroll);
    m_rightPanelScroll->setWidget(m_rightSidePanel);
    if (K4Styles::isCompactLayout()) {
        m_rightSidePanel->setMinimumHeight(m_rightSidePanel->sizeHint().height());
        m_rightSidePanel->resize(m_rightSidePanel->sizeHint());
        m_rightPanelScroll->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
        QScroller::grabGesture(m_rightPanelScroll->viewport(), QScroller::TouchGesture);
        QScroller::grabGesture(m_rightSidePanel, QScroller::TouchGesture);
    }
    if (K4Styles::isCompactLayout()) {
        m_rightPanelScroll->hide();
    } else {
        middleLayout->addWidget(m_rightPanelScroll);
    }

    mainLayout->addWidget(middleWidget, 1);

    // Feature Menu Bar (popup, positioned above bottom menu bar when shown)
    m_featureMenuBar = new FeatureMenuBar(this);

    // Feature Menu Bar CAT commands - send appropriate command based on current feature
    connect(m_featureMenuBar, &FeatureMenuBar::toggleRequested, this, [this]() {
        bool bSet = m_radioState->bSetEnabled();
        switch (m_featureMenuBar->currentFeature()) {
        case FeatureMenuBar::Attenuator: {
            // Optimistic UI update - flip enabled state (use Sub RX state if B SET)
            bool newState = bSet ? !m_radioState->attenuatorEnabledB() : !m_radioState->attenuatorEnabled();
            m_featureMenuBar->setFeatureEnabled(newState);
            m_tcpClient->sendCAT(bSet ? "RA$/;" : "RA/;");
            break;
        }
        case FeatureMenuBar::NbLevel: {
            // Toggle NB on/off
            bool curState = bSet ? m_radioState->noiseBlankerEnabledB() : m_radioState->noiseBlankerEnabled();
            m_featureMenuBar->setFeatureEnabled(!curState);
            m_tcpClient->sendCAT(bSet ? "NB$/;" : "NB/;");
            break;
        }
        case FeatureMenuBar::NrAdjust: {
            const bool ssnr = m_featureMenuBar->currentNrEngine() == FeatureMenuBar::Ssnr;
            bool newState = ssnr ? (bSet ? !m_radioState->ssnrEnabledB() : !m_radioState->ssnrEnabled())
                                 : (bSet ? !m_radioState->noiseReductionEnabledB()
                                         : !m_radioState->noiseReductionEnabled());
            m_featureMenuBar->setFeatureEnabled(newState);
            m_tcpClient->sendCAT(ssnr ? (bSet ? "NRS$/;" : "NRS/;") : (bSet ? "NR$/;" : "NR/;"));
            break;
        }
        case FeatureMenuBar::ManualNotch: {
            // Toggle notch on/off for correct VFO
            bool curState = bSet ? m_radioState->manualNotchEnabledB() : m_radioState->manualNotchEnabled();
            m_featureMenuBar->setFeatureEnabled(!curState);
            m_tcpClient->sendCAT(bSet ? "NM$/;" : "NM/;");
            break;
        }
        }
    });
    connect(m_featureMenuBar, &FeatureMenuBar::incrementRequested, this, [this]() {
        bool bSet = m_radioState->bSetEnabled();
        switch (m_featureMenuBar->currentFeature()) {
        case FeatureMenuBar::Attenuator: {
            // Optimistic: show next step (RA+ adds 3dB, max 21)
            int curLevel = bSet ? m_radioState->attenuatorLevelB() : m_radioState->attenuatorLevel();
            int newLevel = qMin(curLevel + 3, 21);
            m_featureMenuBar->setValue(newLevel);
            m_tcpClient->sendCAT(bSet ? "RA$+;" : "RA+;");
            break;
        }
        case FeatureMenuBar::NbLevel: {
            int curLevel = bSet ? m_radioState->noiseBlankerLevelB() : m_radioState->noiseBlankerLevel();
            int newLevel = qMin(curLevel + 1, 15);
            int enabled =
                bSet ? (m_radioState->noiseBlankerEnabledB() ? 1 : 0) : (m_radioState->noiseBlankerEnabled() ? 1 : 0);
            int filter = bSet ? m_radioState->noiseBlankerFilterWidthB() : m_radioState->noiseBlankerFilterWidth();
            // Optimistic state + UI update
            if (bSet) {
                m_radioState->setNoiseBlankerLevelB(newLevel);
            } else {
                m_radioState->setNoiseBlankerLevel(newLevel);
            }
            m_featureMenuBar->setValue(newLevel);
            QString prefix = bSet ? "NB$" : "NB";
            QString cmd = QString("%1%2%3%4;").arg(prefix).arg(newLevel, 2, 10, QChar('0')).arg(enabled).arg(filter);
            m_tcpClient->sendCAT(cmd);
            break;
        }
        case FeatureMenuBar::NrAdjust: {
            const bool ssnr = m_featureMenuBar->currentNrEngine() == FeatureMenuBar::Ssnr;
            int curLevel = ssnr ? (bSet ? m_radioState->ssnrLevelB() : m_radioState->ssnrLevel())
                                : (bSet ? m_radioState->noiseReductionLevelB() : m_radioState->noiseReductionLevel());
            int newLevel = qMin(curLevel + 1, ssnr ? 20 : 10);
            int enabled = ssnr ? (bSet ? (m_radioState->ssnrEnabledB() ? 1 : 0)
                                         : (m_radioState->ssnrEnabled() ? 1 : 0))
                               : (bSet ? (m_radioState->noiseReductionEnabledB() ? 1 : 0)
                                       : (m_radioState->noiseReductionEnabled() ? 1 : 0));
            if (ssnr) {
                if (bSet) m_radioState->setSsnrLevelB(newLevel);
                else m_radioState->setSsnrLevel(newLevel);
            } else {
                if (bSet) m_radioState->setNoiseReductionLevelB(newLevel);
                else m_radioState->setNoiseReductionLevel(newLevel);
            }
            m_featureMenuBar->setValue(newLevel);
            QString prefix = ssnr ? (bSet ? "NRS$" : "NRS") : (bSet ? "NR$" : "NR");
            QString cmd = QString("%1%2%3;").arg(prefix).arg(newLevel, 2, 10, QChar('0')).arg(enabled);
            m_tcpClient->sendCAT(cmd);
            break;
        }
        case FeatureMenuBar::ManualNotch: {
            // Use correct VFO's pitch state
            int curPitch = bSet ? m_radioState->manualNotchPitchB() : m_radioState->manualNotchPitch();
            int newPitch = qMin(curPitch + 10, 5000);
            int enabled =
                bSet ? (m_radioState->manualNotchEnabledB() ? 1 : 0) : (m_radioState->manualNotchEnabled() ? 1 : 0);
            // Optimistic state + UI update
            if (bSet) {
                m_radioState->setManualNotchPitchB(newPitch);
            } else {
                m_radioState->setManualNotchPitch(newPitch);
            }
            m_featureMenuBar->setValue(newPitch);
            QString prefix = bSet ? "NM$" : "NM";
            m_tcpClient->sendCAT(QString("%1%2%3;").arg(prefix).arg(newPitch, 4, 10, QChar('0')).arg(enabled));
            break;
        }
        }
    });
    connect(m_featureMenuBar, &FeatureMenuBar::decrementRequested, this, [this]() {
        bool bSet = m_radioState->bSetEnabled();
        switch (m_featureMenuBar->currentFeature()) {
        case FeatureMenuBar::Attenuator: {
            // Optimistic: show next step (RA- subtracts 3dB, min 0)
            int curLevel = bSet ? m_radioState->attenuatorLevelB() : m_radioState->attenuatorLevel();
            int newLevel = qMax(curLevel - 3, 0);
            m_featureMenuBar->setValue(newLevel);
            m_tcpClient->sendCAT(bSet ? "RA$-;" : "RA-;");
            break;
        }
        case FeatureMenuBar::NbLevel: {
            int curLevel = bSet ? m_radioState->noiseBlankerLevelB() : m_radioState->noiseBlankerLevel();
            int newLevel = qMax(curLevel - 1, 0);
            int enabled =
                bSet ? (m_radioState->noiseBlankerEnabledB() ? 1 : 0) : (m_radioState->noiseBlankerEnabled() ? 1 : 0);
            int filter = bSet ? m_radioState->noiseBlankerFilterWidthB() : m_radioState->noiseBlankerFilterWidth();
            // Optimistic state + UI update
            if (bSet) {
                m_radioState->setNoiseBlankerLevelB(newLevel);
            } else {
                m_radioState->setNoiseBlankerLevel(newLevel);
            }
            m_featureMenuBar->setValue(newLevel);
            QString prefix = bSet ? "NB$" : "NB";
            m_tcpClient->sendCAT(
                QString("%1%2%3%4;").arg(prefix).arg(newLevel, 2, 10, QChar('0')).arg(enabled).arg(filter));
            break;
        }
        case FeatureMenuBar::NrAdjust: {
            const bool ssnr = m_featureMenuBar->currentNrEngine() == FeatureMenuBar::Ssnr;
            int curLevel = ssnr ? (bSet ? m_radioState->ssnrLevelB() : m_radioState->ssnrLevel())
                                : (bSet ? m_radioState->noiseReductionLevelB() : m_radioState->noiseReductionLevel());
            int newLevel = qMax(curLevel - 1, 0);
            int enabled = ssnr ? (bSet ? (m_radioState->ssnrEnabledB() ? 1 : 0)
                                         : (m_radioState->ssnrEnabled() ? 1 : 0))
                               : (bSet ? (m_radioState->noiseReductionEnabledB() ? 1 : 0)
                                       : (m_radioState->noiseReductionEnabled() ? 1 : 0));
            if (ssnr) {
                if (bSet) m_radioState->setSsnrLevelB(newLevel);
                else m_radioState->setSsnrLevel(newLevel);
            } else {
                if (bSet) m_radioState->setNoiseReductionLevelB(newLevel);
                else m_radioState->setNoiseReductionLevel(newLevel);
            }
            m_featureMenuBar->setValue(newLevel);
            QString prefix = ssnr ? (bSet ? "NRS$" : "NRS") : (bSet ? "NR$" : "NR");
            m_tcpClient->sendCAT(QString("%1%2%3;").arg(prefix).arg(newLevel, 2, 10, QChar('0')).arg(enabled));
            break;
        }
        case FeatureMenuBar::ManualNotch: {
            // Use correct VFO's pitch state
            int curPitch = bSet ? m_radioState->manualNotchPitchB() : m_radioState->manualNotchPitch();
            int newPitch = qMax(curPitch - 10, 150);
            int enabled =
                bSet ? (m_radioState->manualNotchEnabledB() ? 1 : 0) : (m_radioState->manualNotchEnabled() ? 1 : 0);
            // Optimistic state + UI update
            if (bSet) {
                m_radioState->setManualNotchPitchB(newPitch);
            } else {
                m_radioState->setManualNotchPitch(newPitch);
            }
            m_featureMenuBar->setValue(newPitch);
            QString prefix = bSet ? "NM$" : "NM";
            m_tcpClient->sendCAT(QString("%1%2%3;").arg(prefix).arg(newPitch, 4, 10, QChar('0')).arg(enabled));
            break;
        }
        }
    });
    connect(m_featureMenuBar, &FeatureMenuBar::extraButtonClicked, this, [this]() {
        // Extra button cycles NB filter: NONE(0) -> NARROW(1) -> WIDE(2) -> NONE(0)
        if (m_featureMenuBar->currentFeature() == FeatureMenuBar::NbLevel) {
            bool bSet = m_radioState->bSetEnabled();
            int curFilter = bSet ? m_radioState->noiseBlankerFilterWidthB() : m_radioState->noiseBlankerFilterWidth();
            int newFilter = (curFilter + 1) % 3;
            int level = bSet ? m_radioState->noiseBlankerLevelB() : m_radioState->noiseBlankerLevel();
            int enabled =
                bSet ? (m_radioState->noiseBlankerEnabledB() ? 1 : 0) : (m_radioState->noiseBlankerEnabled() ? 1 : 0);
            // Optimistic state + UI update
            if (bSet) {
                m_radioState->setNoiseBlankerFilterB(newFilter);
            } else {
                m_radioState->setNoiseBlankerFilter(newFilter);
            }
            m_featureMenuBar->setNbFilter(newFilter);
            QString prefix = bSet ? "NB$" : "NB";
            m_tcpClient->sendCAT(
                QString("%1%2%3%4;").arg(prefix).arg(level, 2, 10, QChar('0')).arg(enabled).arg(newFilter));
        }
    });

    connect(m_featureMenuBar, &FeatureMenuBar::nrEngineToggleRequested, this, [this]() {
        const bool bSet = m_radioState->bSetEnabled();
        const auto next = m_featureMenuBar->currentNrEngine() == FeatureMenuBar::Lms
                              ? FeatureMenuBar::Ssnr
                              : FeatureMenuBar::Lms;
        m_featureMenuBar->setNrEngine(next);
        if (next == FeatureMenuBar::Ssnr) {
            m_featureMenuBar->setFeatureEnabled(bSet ? m_radioState->ssnrEnabledB() : m_radioState->ssnrEnabled());
            m_featureMenuBar->setValue(bSet ? m_radioState->ssnrLevelB() : m_radioState->ssnrLevel());
        } else {
            m_featureMenuBar->setFeatureEnabled(
                bSet ? m_radioState->noiseReductionEnabledB() : m_radioState->noiseReductionEnabled());
            m_featureMenuBar->setValue(
                bSet ? m_radioState->noiseReductionLevelB() : m_radioState->noiseReductionLevel());
        }
        // The K4 makes LMS NR and SSNR mutually exclusive. Toggling the newly
        // selected engine lets the radio perform that handoff and echo state.
        m_tcpClient->sendCAT(next == FeatureMenuBar::Ssnr ? (bSet ? "NRS$/;" : "NRS/;")
                                                           : (bSet ? "NR$/;" : "NR/;"));
    });

    // Update feature menu bar from RadioState - helper lambda
    auto updateFeatureMenuBarState = [this]() {
        if (!m_featureMenuBar->isMenuVisible())
            return;
        bool bSet = m_radioState->bSetEnabled();
        switch (m_featureMenuBar->currentFeature()) {
        case FeatureMenuBar::Attenuator:
            if (bSet) {
                m_featureMenuBar->setFeatureEnabled(m_radioState->attenuatorEnabledB());
                m_featureMenuBar->setValue(m_radioState->attenuatorLevelB());
            } else {
                m_featureMenuBar->setFeatureEnabled(m_radioState->attenuatorEnabled());
                m_featureMenuBar->setValue(m_radioState->attenuatorLevel());
            }
            break;
        case FeatureMenuBar::NbLevel:
            if (bSet) {
                m_featureMenuBar->setFeatureEnabled(m_radioState->noiseBlankerEnabledB());
                m_featureMenuBar->setValue(m_radioState->noiseBlankerLevelB());
                m_featureMenuBar->setNbFilter(m_radioState->noiseBlankerFilterWidthB());
            } else {
                m_featureMenuBar->setFeatureEnabled(m_radioState->noiseBlankerEnabled());
                m_featureMenuBar->setValue(m_radioState->noiseBlankerLevel());
                m_featureMenuBar->setNbFilter(m_radioState->noiseBlankerFilterWidth());
            }
            break;
        case FeatureMenuBar::NrAdjust:
            {
                const bool lmsOn = bSet ? m_radioState->noiseReductionEnabledB() : m_radioState->noiseReductionEnabled();
                const bool ssnrOn = bSet ? m_radioState->ssnrEnabledB() : m_radioState->ssnrEnabled();
                if (ssnrOn && !lmsOn)
                    m_featureMenuBar->setNrEngine(FeatureMenuBar::Ssnr);
                else if (lmsOn && !ssnrOn)
                    m_featureMenuBar->setNrEngine(FeatureMenuBar::Lms);
                if (m_featureMenuBar->currentNrEngine() == FeatureMenuBar::Ssnr) {
                    m_featureMenuBar->setFeatureEnabled(ssnrOn);
                    m_featureMenuBar->setValue(bSet ? m_radioState->ssnrLevelB() : m_radioState->ssnrLevel());
                } else {
                    m_featureMenuBar->setFeatureEnabled(lmsOn);
                    m_featureMenuBar->setValue(
                        bSet ? m_radioState->noiseReductionLevelB() : m_radioState->noiseReductionLevel());
                }
            }
            break;
        case FeatureMenuBar::ManualNotch:
            // Use correct VFO's notch state
            if (bSet) {
                m_featureMenuBar->setFeatureEnabled(m_radioState->manualNotchEnabledB());
                m_featureMenuBar->setValue(m_radioState->manualNotchPitchB());
            } else {
                m_featureMenuBar->setFeatureEnabled(m_radioState->manualNotchEnabled());
                m_featureMenuBar->setValue(m_radioState->manualNotchPitch());
            }
            break;
        }
    };
    // Connect both Main and Sub RX processing changes
    connect(m_radioState, &RadioState::processingChanged, this, updateFeatureMenuBarState);
    connect(m_radioState, &RadioState::processingChangedB, this, updateFeatureMenuBarState);
    connect(m_radioState, &RadioState::notchChanged, this, updateFeatureMenuBarState);
    connect(m_radioState, &RadioState::notchBChanged, this, updateFeatureMenuBarState);
    // Also update when B SET changes to refresh display with correct VFO's state
    connect(m_radioState, &RadioState::bSetChanged, this, updateFeatureMenuBarState);

    // Mode Popup Widget (popup, positioned above bottom menu bar when shown)
    m_modePopup = new ModePopupWidget(this);

    connect(m_modePopup, &ModePopupWidget::modeSelected, this, [this](const QString &catCmd) {
        // Send the command to the radio
        m_tcpClient->sendCAT(catCmd);

        // Keep the affected VFO label and the next mode-popup tap responsive
        // while waiting for K4 auto-info. This mirrors the established DT
        // optimistic path below; a later MD/MD$ response remains authoritative.
        QRegularExpression mdRegex(QStringLiteral("MD(\\$?)([69]);"));
        QRegularExpressionMatch mdMatch = mdRegex.match(catCmd);
        if (mdMatch.hasMatch()) {
            const QString optimisticMode = mdMatch.captured(1).isEmpty()
                ? QStringLiteral("MD%1;").arg(mdMatch.captured(2))
                : QStringLiteral("MD$%1;").arg(mdMatch.captured(2));
            m_radioState->parseCATCommand(optimisticMode);
        }

        // Optimistically update data sub-mode (K4 doesn't echo DT SET commands)
        // Parse DT or DT$ from command like "MD6;DT1;" or "MD$6;DT$3;"
        QRegularExpression dtRegex("DT(\\$?)(\\d)");
        QRegularExpressionMatch match = dtRegex.match(catCmd);
        if (match.hasMatch()) {
            bool isSubRx = !match.captured(1).isEmpty(); // DT$ = Sub RX
            int subMode = match.captured(2).toInt();
            qDebug() << "Optimistic DT update: isSubRx=" << isSubRx << "subMode=" << subMode;
            if (isSubRx) {
                m_radioState->setDataSubModeB(subMode);
            } else {
                m_radioState->setDataSubMode(subMode);
            }
        }
    });
    // Update mode popup when mode changes - use A or B based on B SET state
    connect(m_radioState, &RadioState::modeChanged, this, [this](RadioState::Mode mode) {
        if (!m_radioState->bSetEnabled()) {
            m_modePopup->setCurrentMode(static_cast<int>(mode));
        }
    });
    connect(m_radioState, &RadioState::modeBChanged, this, [this](RadioState::Mode mode) {
        if (m_radioState->bSetEnabled()) {
            m_modePopup->setCurrentMode(static_cast<int>(mode));
        }
    });
    connect(m_radioState, &RadioState::dataSubModeChanged, this, [this](int subMode) {
        if (!m_radioState->bSetEnabled()) {
            m_modePopup->setCurrentDataSubMode(subMode);
        }
    });
    connect(m_radioState, &RadioState::dataSubModeBChanged, this, [this](int subMode) {
        if (m_radioState->bSetEnabled()) {
            m_modePopup->setCurrentDataSubMode(subMode);
        }
    });
    // Update B SET state for mode popup - also refresh mode/submode/frequency display
    connect(m_radioState, &RadioState::bSetChanged, this, [this](bool enabled) {
        m_modePopup->setBSetEnabled(enabled);
        // Update displayed mode and frequency to match the new target VFO
        if (enabled) {
            m_modePopup->setFrequency(m_radioState->vfoB());
            m_modePopup->setCurrentMode(static_cast<int>(m_radioState->modeB()));
            m_modePopup->setCurrentDataSubMode(m_radioState->dataSubModeB());
        } else {
            m_modePopup->setFrequency(m_radioState->vfoA());
            m_modePopup->setCurrentMode(static_cast<int>(m_radioState->mode()));
            m_modePopup->setCurrentDataSubMode(m_radioState->dataSubMode());
        }
    });

    // B SET indicator visibility and side panel indicator color
    connect(m_radioState, &RadioState::bSetChanged, this, [this](bool enabled) {
        qDebug() << "B SET changed:" << enabled;
        // Show/hide B SET indicator (hide SPLIT when B SET active)
        m_bSetLabel->setVisible(enabled);
        m_splitLabel->setVisible(!enabled);

        // Change side panel BW/SHFT indicator color (cyan=MainRx, green=SubRx)
        m_sideControlPanel->setActiveReceiver(enabled);
    });

    // Bottom Menu Bar
    m_bottomMenuBar = new BottomMenuBar(centralWidget);
    mainLayout->addWidget(m_bottomMenuBar);
    if (K4Styles::isCompactLayout()) {
        m_bottomMenuBar->setMainVolumeValue(RadioSettings::instance()->volume());
        m_bottomMenuBar->setSubVolumeValue(RadioSettings::instance()->subVolume());

        connect(m_bottomMenuBar, &BottomMenuBar::tuneARequested, this, [this](int steps) {
            if (!m_tcpClient->isConnected())
                return;
            const qint64 next = static_cast<qint64>(m_radioState->vfoA()) +
                                static_cast<qint64>(steps) *
                                    (m_phoneTuneStepAHz > 0 ? m_phoneTuneStepAHz
                                                            : tuningStepToHz(m_radioState->tuningStep()));
            if (next > 0) {
                const QString command = QString("FA%1;").arg(static_cast<quint64>(next));
                m_tcpClient->sendCAT(command);
                m_radioState->parseCATCommand(command);
            }
        });
        connect(m_bottomMenuBar, &BottomMenuBar::tuneBRequested, this, [this](int steps) {
            if (!m_tcpClient->isConnected())
                return;
            const qint64 next = static_cast<qint64>(m_radioState->vfoB()) +
                                static_cast<qint64>(steps) *
                                    (m_phoneTuneStepBHz > 0 ? m_phoneTuneStepBHz
                                                            : tuningStepToHz(m_radioState->tuningStepB()));
            if (next > 0) {
                const QString command = QString("FB%1;").arg(static_cast<quint64>(next));
                m_tcpClient->sendCAT(command);
                m_radioState->parseCATCommand(command);
            }
        });
        connect(m_bottomMenuBar, &BottomMenuBar::mainVolumeChanged, this, [this](int value) {
            if (m_audioEngine)
                m_audioEngine->setMainVolume(value / 100.0f);
            RadioSettings::instance()->setVolume(value);
        });
        connect(m_bottomMenuBar, &BottomMenuBar::subVolumeChanged, this, [this](int value) {
            if (m_audioEngine)
                m_audioEngine->setSubVolume(value / 100.0f);
            RadioSettings::instance()->setSubVolume(value);
        });
        connect(m_vfoA, &VFOWidget::tuningDigitSelected, this, [this](int digitFromRight) {
            static constexpr int steps[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000};
            const int digit = qBound(0, digitFromRight, 7);
            m_phoneTuneStepAHz = steps[digit];
            m_bottomMenuBar->setTuneStepA(m_phoneTuneStepAHz);
            // Keep the K4's actual VFO tuning rate in step with the selected
            // display digit whenever CAT has an equivalent VT rate (1 Hz
            // through 10 kHz). Larger digit positions remain a phone tuning
            // step, since K4 VT has no matching 100 kHz/MHz value.
            if (digit <= 4 && m_tcpClient->isConnected()) {
                const QString command = QString("VT%1;").arg(digit);
                m_tcpClient->sendCAT(command);
                m_radioState->parseCATCommand(command);
            }
        });
        connect(m_vfoB, &VFOWidget::tuningDigitSelected, this, [this](int digitFromRight) {
            static constexpr int steps[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000};
            const int digit = qBound(0, digitFromRight, 7);
            m_phoneTuneStepBHz = steps[digit];
            m_bottomMenuBar->setTuneStepB(m_phoneTuneStepBHz);
            if (digit <= 4 && m_tcpClient->isConnected()) {
                const QString command = QString("VT$%1;").arg(digit);
                m_tcpClient->sendCAT(command);
                m_radioState->parseCATCommand(command);
            }
        });
    }

    // During deliberate phone TX, cover the console with an input shield.  A
    // proxy at the exact TX/RX location forwards the return tap to the same
    // existing button and signal path; it does not contain any radio logic.
    m_phoneTxInputShield = new PhoneTxInputShield(centralWidget);
    m_phoneTxInputShield->setObjectName("phoneTxInputShield");
    m_phoneTxInputShield->setStyleSheet("#phoneTxInputShield { background-color: rgba(0, 0, 0, 48); }");
    m_phoneTxInputShield->hide();

    m_phoneTxReleaseButton = new QPushButton("TX ON", m_phoneTxInputShield);
    m_phoneTxReleaseButton->setStyleSheet(K4Styles::menuBarButtonPttPressed());
    m_phoneTxReleaseButton->setFocusPolicy(Qt::NoFocus);
    connect(m_phoneTxReleaseButton, &QPushButton::clicked, this, [this]() {
        // Hide first so the original control is immediately reachable again;
        // click() reuses its existing latched PTT signal wiring unchanged.
        setPhoneTxInputShieldActive(false);
        m_bottomMenuBar->pttButton()->click();
    });

    connect(m_bottomMenuBar, &BottomMenuBar::connectClicked, this, &MainWindow::showRadioManager);
    connect(m_bottomMenuBar, &BottomMenuBar::frequencyARequested, this,
            [this]() { showFrequencyEntry(false); });
    connect(m_bottomMenuBar, &BottomMenuBar::frequencyBRequested, this,
            [this]() { showFrequencyEntry(true); });
    connect(m_bottomMenuBar, &BottomMenuBar::controlsRequested, this, &MainWindow::showPhoneControls);
    connect(m_bottomMenuBar, &BottomMenuBar::settingsRequested, this, &MainWindow::showSettings);
    // Connect volume slider to AudioEngine (Main RX / VFO A)
    connect(m_sideControlPanel, &SideControlPanel::volumeChanged, this, [this](int value) {
        if (m_audioEngine) {
            m_audioEngine->setMainVolume(value / 100.0f);
        }
        RadioSettings::instance()->setVolume(value); // Persist setting
    });

    // The B AF slider always controls Sub RX / VFO B volume. BAL is not exposed
    // by the mobile control drawer and must never repurpose this control.
    connect(m_sideControlPanel, &SideControlPanel::subVolumeChanged, this, [this](int value) {
        if (m_audioEngine)
            m_audioEngine->setSubVolume(value / 100.0f);
        RadioSettings::instance()->setSubVolume(value); // Persist setting
    });
    connect(m_sideControlPanel, &SideControlPanel::phoneMicGainChanged, this, [this](int value) {
        // PHONE MIC is local pre-encode gain for the phone/headset input. Do
        // not send MG; that command belongs exclusively to the K4 MIC control.
        RadioSettings::instance()->setMicGain(value);
        m_audioEngine->setMicGain(value / 100.0f);
    });

    // Connect side control panel scroll signals to CAT commands
    // After sending CAT, update RadioState optimistically (radio doesn't echo these commands)
    // Group 1: WPM/PTCH (CW mode) and MIC/CMP (Voice mode)
    connect(m_sideControlPanel, &SideControlPanel::wpmChanged, this, [this](int delta) {
        int newWpm = qBound(8, m_radioState->keyerSpeed() + delta, 50);
        m_tcpClient->sendCAT(QString("KS%1;").arg(newWpm, 3, 10, QChar('0')));
        m_radioState->setKeyerSpeed(newWpm);
    });
    connect(m_sideControlPanel, &SideControlPanel::pitchChanged, this, [this](int delta) {
        int currentPitch = m_radioState->cwPitch(); // In Hz
        int newPitch = qBound(300, currentPitch + (delta * 10), 990);
        m_tcpClient->sendCAT(QString("CW%1;").arg(newPitch / 10, 2, 10, QChar('0')));
        m_radioState->setCwPitch(newPitch);
    });
    connect(m_sideControlPanel, &SideControlPanel::micGainChanged, this, [this](int delta) {
        int newGain = qBound(0, m_radioState->micGain() + delta, 80);
        m_tcpClient->sendCAT(QString("MG%1;").arg(newGain, 3, 10, QChar('0')));
        m_radioState->setMicGain(newGain);
    });
    connect(m_sideControlPanel, &SideControlPanel::compressionChanged, this, [this](int delta) {
        int newComp = qBound(0, m_radioState->compression() + delta, 30);
        m_tcpClient->sendCAT(QString("CP%1;").arg(newComp, 3, 10, QChar('0')));
        m_radioState->setCompression(newComp);
    });
    // Group 1: PWR/DLY
    // PC command uses PCnnnr; format: L=QRP (0.1-10W), H=QRO (11-110W)
    // QRP (≤10W): 0.1W increments, e.g., 10.0, 9.9, 9.8, ... 0.1
    // QRO (>10W): 1W increments, e.g., 11, 12, 13, ... 110
    connect(m_sideControlPanel, &SideControlPanel::powerChanged, this, [this](int delta) {
        double currentPower = m_radioState->rfPower();
        double newPower;

        if (currentPower <= 10.0) {
            // Currently in QRP range: 0.1W increments
            newPower = currentPower + (delta * 0.1);
            if (newPower > 10.0) {
                // Transition to QRO at 11W
                newPower = 11.0;
                int powerVal = static_cast<int>(newPower);
                m_tcpClient->sendCAT(QString("PC%1H;").arg(powerVal, 3, 10, QChar('0')));
            } else {
                newPower = qBound(0.1, newPower, 10.0);
                int powerVal = static_cast<int>(qRound(newPower * 10)); // 9.9W = 099
                m_tcpClient->sendCAT(QString("PC%1L;").arg(powerVal, 3, 10, QChar('0')));
            }
        } else {
            // Currently in QRO range: 1W increments
            newPower = currentPower + delta;
            if (newPower <= 10.0) {
                // Transition to QRP at 10.0W
                newPower = 10.0;
                int powerVal = static_cast<int>(qRound(newPower * 10)); // 10.0W = 100
                m_tcpClient->sendCAT(QString("PC%1L;").arg(powerVal, 3, 10, QChar('0')));
            } else {
                newPower = qBound(11.0, newPower, 110.0);
                int powerVal = static_cast<int>(newPower);
                m_tcpClient->sendCAT(QString("PC%1H;").arg(powerVal, 3, 10, QChar('0')));
            }
        }
        m_radioState->setRfPower(newPower);
    });
    connect(m_sideControlPanel, &SideControlPanel::delayChanged, this, [this](int delta) {
        int currentDelay = m_radioState->delayForCurrentMode();
        if (currentDelay < 0)
            currentDelay = 0;                                // Handle uninitialized
        int newDelay = qBound(0, currentDelay + delta, 255); // 0-255 = 0.00 to 2.55 seconds

        // Optimistic update - update local state immediately
        m_radioState->setDelayForCurrentMode(newDelay);

        // SD command format: SDxyzzz where x=QSK flag, y=mode (C/V/D), zzz=delay in 10ms
        // Determine mode character based on current operating mode
        QChar modeChar = 'V'; // Default to Voice
        RadioState::Mode mode = m_radioState->mode();
        if (mode == RadioState::CW || mode == RadioState::CW_R) {
            modeChar = 'C';
        } else if (mode == RadioState::DATA || mode == RadioState::DATA_R) {
            modeChar = 'D';
        }
        // x=0 means use specified delay (not full QSK)
        m_tcpClient->sendCAT(QString("SD0%1%2;").arg(modeChar).arg(newDelay, 3, 10, QChar('0')));
    });
    // Group 2: BW/HI and SHFT/LO
    // BW command uses 10Hz units (divide by 10)
    connect(m_sideControlPanel, &SideControlPanel::bandwidthChanged, this, [this](int delta) {
        bool bSet = m_radioState->bSetEnabled();
        int currentBw = bSet ? m_radioState->filterBandwidthB() : m_radioState->filterBandwidth();
        int bwMin = 50, bwMax = 5000;
        const RadioState::Mode mode = bSet ? m_radioState->modeB() : m_radioState->mode();
        const int subMode = bSet ? m_radioState->dataSubModeB() : m_radioState->dataSubMode();
        if (mode == RadioState::DATA || mode == RadioState::DATA_R) {
            if (subMode == 2) { bwMin = 150; bwMax = 800; }
            else if (subMode == 3) { bwMax = 200; }
        }
        int newBw = qBound(bwMin, currentBw + (delta * 50), bwMax);
        QString cmd = bSet ? "BW$" : "BW";
        m_tcpClient->sendCAT(QString("%1%2;").arg(cmd).arg(newBw / 10, 4, 10, QChar('0')));
        if (bSet) {
            m_radioState->setFilterBandwidthB(newBw);
        } else {
            m_radioState->setFilterBandwidth(newBw);
        }
    });
    auto adjustFilterEdge = [this](bool adjustHi, int delta) {
        const bool bSet = m_radioState->bSetEnabled();
        const RadioState::Mode mode = bSet ? m_radioState->modeB() : m_radioState->mode();
        const int subMode = bSet ? m_radioState->dataSubModeB() : m_radioState->dataSubMode();
        const int bwDah = (bSet ? m_radioState->filterBandwidthB() : m_radioState->filterBandwidth()) / 10;
        const int isDah = bSet ? m_radioState->ifShiftB() : m_radioState->ifShift();
        int bwMinDah = 5, bwMaxDah = 500;
        bool isLocked = mode == RadioState::AM || mode == RadioState::FM;
        if (mode == RadioState::DATA || mode == RadioState::DATA_R) {
            if (subMode == 2) { bwMinDah = 15; bwMaxDah = 80; isLocked = true; }
            else if (subMode == 3) { bwMaxDah = 20; isLocked = true; }
        }
        const int loDah = qMax(0, isDah - bwDah / 2);
        const int hiDah = loDah + bwDah;
        const int requestedHi = adjustHi ? hiDah + delta : hiDah;
        const int requestedLo = adjustHi ? loDah : loDah + delta;
        if (requestedHi <= requestedLo)
            return;
        const int newBwDah = qBound(bwMinDah, requestedHi - requestedLo, bwMaxDah);
        const QString bwCmd = bSet ? "BW$" : "BW";
        m_tcpClient->sendCAT(QString("%1%2;").arg(bwCmd).arg(newBwDah, 4, 10, QChar('0')));
        if (bSet) m_radioState->setFilterBandwidthB(newBwDah * 10);
        else m_radioState->setFilterBandwidth(newBwDah * 10);
        if (!isLocked) {
            const int maxIsDah = (mode == RadioState::CW || mode == RadioState::CW_R) ? 200 : 300;
            const int newIsDah = qBound(30, (requestedHi + requestedLo) / 2, maxIsDah);
            const QString isCmd = bSet ? "IS$" : "IS";
            m_tcpClient->sendCAT(QString("%1+%2;").arg(isCmd).arg(newIsDah, 4, 10, QChar('0')));
            if (bSet) m_radioState->setIfShiftB(newIsDah);
            else m_radioState->setIfShift(newIsDah);
        }
    };
    connect(m_sideControlPanel, &SideControlPanel::highCutChanged, this,
            [adjustFilterEdge](int delta) { adjustFilterEdge(true, delta); });
    connect(m_sideControlPanel, &SideControlPanel::shiftChanged, this, [this](int delta) {
        bool bSet = m_radioState->bSetEnabled();
        const RadioState::Mode mode = bSet ? m_radioState->modeB() : m_radioState->mode();
        const int subMode = bSet ? m_radioState->dataSubModeB() : m_radioState->dataSubMode();
        if (mode == RadioState::AM || mode == RadioState::FM ||
            ((mode == RadioState::DATA || mode == RadioState::DATA_R) && (subMode == 2 || subMode == 3)))
            return;
        int currentShift = bSet ? m_radioState->ifShiftB() : m_radioState->ifShift();
        const int maxShift = (mode == RadioState::CW || mode == RadioState::CW_R) ? 200 : 300;
        int newShift = qBound(30, currentShift + delta, maxShift);
        QString prefix = bSet ? "IS$" : "IS";
        QString cmd = QString("%1+%2;").arg(prefix).arg(newShift, 4, 10, QChar('0'));
        m_tcpClient->sendCAT(cmd);
        if (bSet) {
            m_radioState->setIfShiftB(newShift);
        } else {
            m_radioState->setIfShift(newShift);
        }
    });
    connect(m_sideControlPanel, &SideControlPanel::shiftSliderCommitted, this, [this](int targetDah) {
        const bool bSet = m_radioState->bSetEnabled();
        const RadioState::Mode mode = bSet ? m_radioState->modeB() : m_radioState->mode();
        const int maxShift = (mode == RadioState::CW || mode == RadioState::CW_R) ? 200 : 300;
        const int newShift = qBound(30, targetDah, maxShift);
        const QString prefix = bSet ? "IS$" : "IS";
        const QString cmd = QString("%1+%2;").arg(prefix).arg(newShift, 4, 10, QChar('0'));
        m_tcpClient->sendCAT(cmd);
        if (bSet)
            m_radioState->setIfShiftB(newShift);
        else
            m_radioState->setIfShift(newShift);
    });
    connect(m_sideControlPanel, &SideControlPanel::lowCutChanged, this,
            [adjustFilterEdge](int delta) { adjustFilterEdge(false, delta); });
    // Group 3: M.RF/M.SQL and S.RF/S.SQL
    // RF Gain uses RG-nn; format where nn is 00-60 (representing -0 to -60 dB attenuation)
    // Scroll up = less attenuation = decrease value, scroll down = more attenuation = increase value
    connect(m_sideControlPanel, &SideControlPanel::mainRfGainChanged, this, [this](int delta) {
        int newGain = qBound(0, m_radioState->rfGain() - delta, 60);
        m_tcpClient->sendCAT(QString("RG-%1;").arg(newGain, 2, 10, QChar('0')));
        m_radioState->setRfGain(newGain);
    });
    connect(m_sideControlPanel, &SideControlPanel::mainSquelchChanged, this, [this](int delta) {
        int newSql = qBound(0, m_radioState->squelchLevel() + delta, 29);
        m_tcpClient->sendCAT(QString("SQ%1;").arg(newSql, 3, 10, QChar('0')));
        m_radioState->setSquelchLevel(newSql);
    });
    connect(m_sideControlPanel, &SideControlPanel::subRfGainChanged, this, [this](int delta) {
        int newGain = qBound(0, m_radioState->rfGainB() - delta, 60);
        m_tcpClient->sendCAT(QString("RG$-%1;").arg(newGain, 2, 10, QChar('0')));
        m_radioState->setRfGainB(newGain);
    });
    connect(m_sideControlPanel, &SideControlPanel::subSquelchChanged, this, [this](int delta) {
        int newSql = qBound(0, m_radioState->squelchLevelB() + delta, 29);
        m_tcpClient->sendCAT(QString("SQ$%1;").arg(newSql, 3, 10, QChar('0')));
        m_radioState->setSquelchLevelB(newSql);
    });

    // Connect TX function button signals to CAT commands
    connect(m_sideControlPanel, &SideControlPanel::tuneClicked, this, [this]() {
        queueControlFeedback("TUNE", "TUNE switch activated");
        m_tcpClient->sendCAT("SW16;");
    });
    connect(m_sideControlPanel, &SideControlPanel::tuneLpClicked, this, [this]() {
        queueControlFeedback("TUNE_LP", "TUNE low-power activated");
        m_tcpClient->sendCAT("SW131;");
    });
    connect(m_sideControlPanel, &SideControlPanel::xmitClicked, this, [this]() {
        if (!m_tcpClient->isConnected())
            return;
        // Match QK4 main: XMIT is a real CAT PTT toggle, not a front-panel
        // switch-code shortcut and not VOX/audio-packet keying.
        const bool goTx = !m_radioState->isTransmitting();
        queueControlFeedback("XMIT", goTx ? "TRANSMIT ON" : "RECEIVE");
        m_tcpClient->sendCAT(goTx ? "TX;" : "RX;");
        m_pttActive = goTx;
        QMetaObject::invokeMethod(m_audioEngine, "setPttActive", Qt::QueuedConnection, Q_ARG(bool, goTx));
        m_bottomMenuBar->setPttActive(goTx);
    });
    connect(m_sideControlPanel, &SideControlPanel::testClicked, this, [this]() {
        queueControlFeedback("TEST", "TEST switch activated");
        m_tcpClient->sendCAT("SW132;");
    });
    connect(m_sideControlPanel, &SideControlPanel::atuClicked, this, [this]() {
        queueControlFeedback("ATU", "ATU mode changed");
        m_tcpClient->sendCAT("SW158;");
    });
    connect(m_sideControlPanel, &SideControlPanel::atuTuneClicked, this, [this]() {
        queueControlFeedback("ATU_TUNE", "ATU TUNE started");
        m_tcpClient->sendCAT("SW40;");
    });
    connect(m_sideControlPanel, &SideControlPanel::voxClicked, this, [this]() {
        queueControlFeedback("VOX", "VOX changed");
        m_tcpClient->sendCAT("SW50;");
    });
    connect(m_sideControlPanel, &SideControlPanel::qskClicked, this, [this]() {
        queueControlFeedback("QSK", "QSK changed");
        m_tcpClient->sendCAT("SW134;");
    });
    connect(m_sideControlPanel, &SideControlPanel::antClicked, this, [this]() {
        queueControlFeedback("TX_ANT", "TX antenna changed");
        m_tcpClient->sendCAT("SW60;");
    });
    connect(m_sideControlPanel, &SideControlPanel::remAntClicked, this, [this]() {
        queueControlFeedback("REM_ANT", "REMOTE ANTENNA changed");
        m_tcpClient->sendCAT("SW135;");
    });
    connect(m_sideControlPanel, &SideControlPanel::rxAntClicked, this, [this]() {
        queueControlFeedback("RX_ANT", "Main RX antenna changed");
        m_tcpClient->sendCAT("SW70;");
    });
    connect(m_sideControlPanel, &SideControlPanel::subAntClicked, this, [this]() {
        queueControlFeedback("SUB_ANT", "Sub RX antenna changed");
        m_tcpClient->sendCAT("SW157;");
    });

    // NORM is placed with BW/SHFT and performs the K4's nominal-passband
    // action. K4 MON is intentionally not exposed in the remote control UI.
    connect(m_sideControlPanel, &SideControlPanel::normalizeFilterRequested, this, [this]() {
        queueControlFeedback("FILTER_NORM", "Filter passband normalized");
        m_tcpClient->sendCAT("SW129;");
    });

    // Forward audio mix routing (MX command) to audio engine
    connect(m_radioState, &RadioState::audioMixChanged, this, [this](int left, int right) {
        if (m_audioEngine) {
            m_audioEngine->setAudioMix(left, right);
        }
    });

    // Connect right side panel button signals to CAT commands
    // Primary (left-click) signals
    connect(m_rightSidePanel, &RightSidePanel::preClicked, this, [this]() {
        queueControlFeedback("PRE", "PRE changed"); m_tcpClient->sendCAT("SW61;");
    });
    connect(m_rightSidePanel, &RightSidePanel::nbClicked, this, [this]() {
        queueControlFeedback("NB", "NOISE BLANKER changed"); m_tcpClient->sendCAT("SW32;");
    });
    connect(m_rightSidePanel, &RightSidePanel::nrClicked, this, [this]() {
        queueControlFeedback("NR", "NOISE REDUCTION changed"); m_tcpClient->sendCAT("SW62;");
    });
    connect(m_rightSidePanel, &RightSidePanel::ntchClicked, this, [this]() {
        const bool targetsSub = m_radioState->bSetEnabled();
        const bool willEnable = targetsSub ? !m_radioState->autoNotchEnabledB()
                                           : !m_radioState->autoNotchEnabled();
        // SW31 does not consistently echo NA0 when switching notch off on all
        // K4 firmware versions. Report the deterministic requested state now,
        // then query the appropriate NA register to resynchronize RadioState.
        showControlFeedback(QString("%1AUTO NOTCH %2")
                                .arg(targetsSub ? "SUB " : "", willEnable ? "ON" : "OFF"));
        m_tcpClient->sendCAT("SW31;");
        QTimer::singleShot(180, this, [this, targetsSub]() {
            m_tcpClient->sendCAT(targetsSub ? "NA$;" : "NA;");
        });
    });
    connect(m_rightSidePanel, &RightSidePanel::filClicked, this, [this]() {
        queueControlFeedback("FIL", "RX filter changed"); m_tcpClient->sendCAT("SW33;");
    });
    connect(m_rightSidePanel, &RightSidePanel::abClicked, this, [this]() {
        showControlFeedback("VFO A / B swapped"); m_tcpClient->sendCAT("SW41;");
    });
    connect(m_rightSidePanel, &RightSidePanel::revPressed, this, [this]() {
        showControlFeedback("REV active"); m_tcpClient->sendCAT("SW160;");
    });
    connect(m_rightSidePanel, &RightSidePanel::revReleased, this, [this]() {
        showControlFeedback("REV released"); m_tcpClient->sendCAT("SW161;");
    });
    connect(m_rightSidePanel, &RightSidePanel::atobClicked, this, [this]() {
        showControlFeedback("VFO A copied to B"); m_tcpClient->sendCAT("SW72;");
    });
    connect(m_rightSidePanel, &RightSidePanel::spotClicked, this, [this]() {
        showControlFeedback("SPOT changed"); m_tcpClient->sendCAT("SW42;");
    });
    connect(m_rightSidePanel, &RightSidePanel::modeClicked, this, [this]() {
        // Toggle mode popup - if open, close it; otherwise show it
        if (m_modePopup->isVisible()) {
            m_modePopup->hidePopup();
        } else {
            // Update current state before showing - use A or B based on B SET
            bool bSet = m_radioState->bSetEnabled();
            if (bSet) {
                m_modePopup->setFrequency(m_radioState->vfoB());
                m_modePopup->setCurrentMode(static_cast<int>(m_radioState->modeB()));
                m_modePopup->setCurrentDataSubMode(m_radioState->dataSubModeB());
            } else {
                m_modePopup->setFrequency(m_radioState->vfoA());
                m_modePopup->setCurrentMode(static_cast<int>(m_radioState->mode()));
                m_modePopup->setCurrentDataSubMode(m_radioState->dataSubMode());
            }
            m_modePopup->setBSetEnabled(bSet);
            m_modePopup->showAboveWidget(m_bottomMenuBar);
        }
    });

    // Secondary (right-click) signals - these show feature menus with toggle behavior
    // These editors are invoked from long presses inside the compact CTRL
    // drawer. Hide that top-level drawer first; otherwise it stacks above the
    // main-window FeatureMenuBar and makes its touch controls inaccessible.
    auto showFeatureAdjustment = [this](FeatureMenuBar::Feature feature) {
        this->showFeatureAdjustment(static_cast<int>(feature));
    };
    connect(m_rightSidePanel, &RightSidePanel::attnClicked, this,
            [=]() { showFeatureAdjustment(FeatureMenuBar::Attenuator); });
    connect(m_rightSidePanel, &RightSidePanel::levelClicked, this,
            [=]() { showFeatureAdjustment(FeatureMenuBar::NbLevel); });
    connect(m_rightSidePanel, &RightSidePanel::adjClicked, this,
            [=]() { showFeatureAdjustment(FeatureMenuBar::NrAdjust); });
    connect(m_rightSidePanel, &RightSidePanel::manualClicked, this,
            [=]() { showFeatureAdjustment(FeatureMenuBar::ManualNotch); });
    connect(m_rightSidePanel, &RightSidePanel::apfClicked, this, [this]() {
        // Toggle APF on/off for Main RX or Sub RX based on B SET state
        queueControlFeedback("APF", "APF changed");
        if (m_radioState->bSetEnabled()) {
            m_tcpClient->sendCAT("AP$/;"); // Sub RX toggle
        } else {
            m_tcpClient->sendCAT("AP/;"); // Main RX toggle
        }
    });
    connect(m_rightSidePanel, &RightSidePanel::splitClicked, this, [this]() {
        queueControlFeedback("SPLIT", "SPLIT changed"); m_tcpClient->sendCAT("SW145;");
    });
    connect(m_rightSidePanel, &RightSidePanel::btoaClicked, this, [this]() {
        showControlFeedback("VFO B copied to A"); m_tcpClient->sendCAT("SW147;");
    });
    connect(m_rightSidePanel, &RightSidePanel::autoClicked, this, [this]() {
        showControlFeedback("AUTO SPOT changed"); m_tcpClient->sendCAT("SW146;");
    });
    // altClicked (MODE/ALT right-click) - send SW148 for ALT function
    connect(m_rightSidePanel, &RightSidePanel::altClicked, this, [this]() {
        showControlFeedback("Alternate mode selected"); m_tcpClient->sendCAT("SW148;");
    });

    // PF row primary (left-click) signals
    connect(m_rightSidePanel, &RightSidePanel::bsetClicked, this, [this]() {
        queueControlFeedback("BSET", "B SET changed"); m_tcpClient->sendCAT("SW44;");
    });
    connect(m_rightSidePanel, &RightSidePanel::clrClicked, this, [this]() {
        queueControlFeedback("RITXIT", "RIT / XIT offset cleared"); m_tcpClient->sendCAT("SW64;");
    });
    connect(m_rightSidePanel, &RightSidePanel::ritClicked, this, [this]() {
        queueControlFeedback("RITXIT", "RIT changed"); m_tcpClient->sendCAT("SW54;");
    });
    connect(m_rightSidePanel, &RightSidePanel::xitClicked, this, [this]() {
        queueControlFeedback("RITXIT", "XIT changed"); m_tcpClient->sendCAT("SW74;");
    });

    // PF row secondary (right-click) signals
    // PF1-PF4 execute user-configured macros (or default K4 PF functions if no macro set)
    connect(m_rightSidePanel, &RightSidePanel::pf1Clicked, this, [this]() {
        MacroEntry macro = RadioSettings::instance()->macro(MacroIds::PF1);
        if (!macro.command.isEmpty()) {
            executeMacro(MacroIds::PF1);
        } else {
            m_tcpClient->sendCAT("SW153;"); // Default: K4 PF1
        }
    });
    connect(m_rightSidePanel, &RightSidePanel::pf2Clicked, this, [this]() {
        MacroEntry macro = RadioSettings::instance()->macro(MacroIds::PF2);
        if (!macro.command.isEmpty()) {
            executeMacro(MacroIds::PF2);
        } else {
            m_tcpClient->sendCAT("SW154;"); // Default: K4 PF2
        }
    });
    connect(m_rightSidePanel, &RightSidePanel::pf3Clicked, this, [this]() {
        MacroEntry macro = RadioSettings::instance()->macro(MacroIds::PF3);
        if (!macro.command.isEmpty()) {
            executeMacro(MacroIds::PF3);
        } else {
            m_tcpClient->sendCAT("SW155;"); // Default: K4 PF3
        }
    });
    connect(m_rightSidePanel, &RightSidePanel::pf4Clicked, this, [this]() {
        MacroEntry macro = RadioSettings::instance()->macro(MacroIds::PF4);
        if (!macro.command.isEmpty()) {
            executeMacro(MacroIds::PF4);
        } else {
            m_tcpClient->sendCAT("SW156;"); // Default: K4 PF4
        }
    });

    // Bottom row signals (SUB, DIVERSITY, RATE, LOCK)
    connect(m_rightSidePanel, &RightSidePanel::subClicked, this, [this]() {
        queueControlFeedback("SUB", "SUB RX changed"); m_tcpClient->sendCAT("SW83;");
    });
    connect(m_rightSidePanel, &RightSidePanel::diversityClicked, this, [this]() {
        queueControlFeedback("DIV", "DIVERSITY changed"); m_tcpClient->sendCAT("SW152;");
    });
    connect(m_rightSidePanel, &RightSidePanel::rateClicked, this, [this]() {
        // Cycle fine rates: 1 Hz → 10 Hz → 100 Hz → 1 Hz
        // B-SET aware: targets VFO B (VT$) when B SET is engaged
        bool bSet = m_radioState->bSetEnabled();
        int current = bSet ? m_radioState->tuningStepB() : m_radioState->tuningStep();
        int next = (current >= 0 && current < 2) ? current + 1 : 0;
        QString cmd = QString("%1%2;").arg(bSet ? "VT$" : "VT").arg(next);
        m_tcpClient->sendCAT(cmd);
        m_radioState->parseCATCommand(cmd);
        static const int rates[] = {1, 10, 100, 1000, 10000, 100};
        showControlFeedback(QString("TUNING RATE: %1 Hz").arg(rates[next]));
    });
    connect(m_rightSidePanel, &RightSidePanel::khzClicked, this, [this]() {
        // Set tuning step to 1 kHz (VT3)
        // B-SET aware: targets VFO B (VT$) when B SET is engaged
        bool bSet = m_radioState->bSetEnabled();
        QString cmd = bSet ? QStringLiteral("VT$3;") : QStringLiteral("VT3;");
        m_tcpClient->sendCAT(cmd);
        m_radioState->parseCATCommand(cmd);
        showControlFeedback("TUNING RATE: 1 kHz");
    });
    connect(m_rightSidePanel, &RightSidePanel::lockAClicked, this,
            [this]() { queueControlFeedback("LOCK_A", "VFO A lock changed"); m_tcpClient->sendCAT("SW63;"); });
    connect(m_rightSidePanel, &RightSidePanel::lockBClicked, this,
            [this]() { queueControlFeedback("LOCK_B", "VFO B lock changed"); m_tcpClient->sendCAT("SW151;"); });

    // Resolve CTRL-panel actions from the state echoed by the K4.  These
    // confirmations remain useful after the drawer has closed, particularly
    // for controls whose state is not represented in the compact phone view.
    connect(m_radioState, &RadioState::filterPositionChanged, this, [this](int position) {
        completeControlFeedback("FIL", QString("MAIN RX FILTER: FIL%1").arg(position));
    });
    connect(m_radioState, &RadioState::filterPositionBChanged, this, [this](int position) {
        completeControlFeedback("FIL", QString("SUB RX FILTER: FIL%1").arg(position));
    });
    connect(m_radioState, &RadioState::bSetChanged, this, [this](bool enabled) {
        completeControlFeedback("BSET", enabled
            ? QStringLiteral("B SET ON - controls target VFO B")
            : QStringLiteral("B SET OFF - controls target VFO A"));
    });
    connect(m_radioState, &RadioState::subRxEnabledChanged, this, [this](bool enabled) {
        completeControlFeedback("SUB", enabled ? "SUB RECEIVER ON" : "SUB RECEIVER OFF");
    });
    connect(m_radioState, &RadioState::diversityChanged, this, [this](bool enabled) {
        completeControlFeedback("DIV", enabled ? "DIVERSITY ON" : "DIVERSITY OFF");
    });
    connect(m_radioState, &RadioState::apfChanged, this, [this](bool enabled, int width) {
        static const int widths[] = {30, 50, 150};
        completeControlFeedback("APF", enabled
            ? QString("MAIN APF ON: %1 Hz").arg(widths[qBound(0, width, 2)])
            : QStringLiteral("MAIN APF OFF"));
    });
    connect(m_radioState, &RadioState::apfBChanged, this, [this](bool enabled, int width) {
        static const int widths[] = {30, 50, 150};
        completeControlFeedback("APF", enabled
            ? QString("SUB APF ON: %1 Hz").arg(widths[qBound(0, width, 2)])
            : QStringLiteral("SUB APF OFF"));
    });
    connect(m_radioState, &RadioState::lockAChanged, this, [this](bool locked) {
        completeControlFeedback("LOCK_A", locked ? "VFO A LOCKED" : "VFO A UNLOCKED");
    });
    connect(m_radioState, &RadioState::lockBChanged, this, [this](bool locked) {
        completeControlFeedback("LOCK_B", locked ? "VFO B LOCKED" : "VFO B UNLOCKED");
    });
    connect(m_radioState, &RadioState::transmitStateChanged, this, [this](bool transmitting) {
        completeControlFeedback("XMIT", transmitting ? "TRANSMIT ON" : "RECEIVE");
    });

    // Connect memory buttons (M1-M4, REC, STORE, RCL)
    // Primary actions (left click)
    connect(m_m1Btn, &QPushButton::clicked, this, [this]() { showControlFeedback("MESSAGE M1 activated"); m_tcpClient->sendCAT("SW17;"); });
    connect(m_m2Btn, &QPushButton::clicked, this, [this]() { showControlFeedback("MESSAGE M2 activated"); m_tcpClient->sendCAT("SW51;"); });
    connect(m_m3Btn, &QPushButton::clicked, this, [this]() { showControlFeedback("MESSAGE M3 activated"); m_tcpClient->sendCAT("SW18;"); });
    connect(m_m4Btn, &QPushButton::clicked, this, [this]() { showControlFeedback("MESSAGE M4 activated"); m_tcpClient->sendCAT("SW52;"); });
    connect(m_recBtn, &QPushButton::clicked, this, [this]() { showControlFeedback("MESSAGE RECORD activated"); m_tcpClient->sendCAT("SW19;"); });
    connect(m_storeBtn, &QPushButton::clicked, this, [this]() { showControlFeedback("MESSAGE STORE activated"); m_tcpClient->sendCAT("SW20;"); });
    connect(m_rclBtn, &QPushButton::clicked, this, [this]() { showControlFeedback("MESSAGE RECALL activated"); m_tcpClient->sendCAT("SW34;"); });

    // Install event filters for right-click (alternate actions)
    m_recBtn->installEventFilter(this);
    m_storeBtn->installEventFilter(this);
    m_rclBtn->installEventFilter(this);

    // Connect bottom menu bar signals
    connect(m_bottomMenuBar, &BottomMenuBar::menuClicked, this, &MainWindow::showMenuOverlay);
    connect(m_bottomMenuBar, &BottomMenuBar::fnClicked, this, &MainWindow::toggleFnPopup);
    connect(m_bottomMenuBar, &BottomMenuBar::displayClicked, this, &MainWindow::toggleDisplayPopup);
    connect(m_bottomMenuBar, &BottomMenuBar::bandClicked, this, &MainWindow::toggleBandPopup);
    connect(m_bottomMenuBar, &BottomMenuBar::mainRxClicked, this, &MainWindow::toggleMainRxPopup);
    connect(m_bottomMenuBar, &BottomMenuBar::subRxClicked, this, &MainWindow::toggleSubRxPopup);
    connect(m_bottomMenuBar, &BottomMenuBar::txClicked, this, &MainWindow::toggleTxPopup);

    // PTT button connections
    connect(m_bottomMenuBar, &BottomMenuBar::pttPressed, this, &MainWindow::onPttPressed);
    connect(m_bottomMenuBar, &BottomMenuBar::pttReleased, this, &MainWindow::onPttReleased);

    // TX packets are encoded on AudioEngine's thread (matching current QK4),
    // then TcpClient safely marshals them to its I/O thread. This keeps PTT
    // continuous even while the phone UI is repainting the panadapter.
    connect(m_audioEngine, &AudioEngine::txPacketReady, m_tcpClient, &TcpClient::sendRaw);
    connect(m_audioEngine, &AudioEngine::sstvPacketReady, m_tcpClient, &TcpClient::sendSstvAudio);

    // The K4 does not echo SL changes. RadioState is updated optimistically on
    // connect and remains the single source for TX frame sizing thereafter.
    connect(m_radioState, &RadioState::streamingLatencyChanged, this,
            [this](int tier) { m_audioEngine->setFrameSamples(streamingLatencyToFrameSamples(tier)); });

    // Note: audio buffer flushing on mode/filter changes was removed — AudioEngine now runs
    // on a dedicated thread with a properly sized jitter buffer, so stale audio lag no longer
    // occurs. Flushing would cause a brief audio dropout on every mode/filter switch.
}

void MainWindow::setupTopStatusBar(QWidget *parent) {
    auto *statusBar = new QWidget(parent);
    statusBar->setFixedHeight(K4Styles::Dimensions::ButtonHeightSmall);
    statusBar->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::DarkBackground));

    auto *layout = new QHBoxLayout(statusBar);
    layout->setContentsMargins(K4Styles::Dimensions::PaddingSmall, 2, K4Styles::Dimensions::PaddingSmall, 2);
    layout->setSpacing(K4Styles::Dimensions::PaddingLarge);

    // Elecraft K4 title
    m_titleLabel = new QLabel("Elecraft K4", statusBar);
    m_titleLabel->setStyleSheet(
        QString("color: %1; font-weight: bold; font-size: %2px;")
            .arg(K4Styles::Colors::TextWhite)
            .arg(K4Styles::Dimensions::FontSizePopup));
    layout->addWidget(m_titleLabel);

    // Date/Time
    m_dateTimeLabel = new QLabel("--/-- --:--:-- Z", statusBar);
    m_dateTimeLabel->setStyleSheet(
        QString("color: %1; font-size: %2px;").arg(K4Styles::Colors::TextGray).arg(K4Styles::Dimensions::FontSizeButton));
    layout->addWidget(m_dateTimeLabel);

    layout->addStretch();

    // Power
    m_powerLabel = new QLabel("--- W", statusBar);
    m_powerLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                    .arg(K4Styles::Colors::AccentAmber)
                                    .arg(K4Styles::Dimensions::FontSizeButton));
    layout->addWidget(m_powerLabel);

    // SWR
    m_swrLabel = new QLabel("-.-:1", statusBar);
    m_swrLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                  .arg(K4Styles::Colors::AccentAmber)
                                  .arg(K4Styles::Dimensions::FontSizeButton));
    layout->addWidget(m_swrLabel);

    // Voltage
    m_voltageLabel = new QLabel("--.- V", statusBar);
    m_voltageLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                      .arg(K4Styles::Colors::AccentAmber)
                                      .arg(K4Styles::Dimensions::FontSizeButton));
    layout->addWidget(m_voltageLabel);

    // Current
    m_currentLabel = new QLabel("-.- A", statusBar);
    m_currentLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                      .arg(K4Styles::Colors::AccentAmber)
                                      .arg(K4Styles::Dimensions::FontSizeButton));
    layout->addWidget(m_currentLabel);

    // Current mainline QK4 exposes both RF-deck temperatures from SIRF.
    m_lpaTempLabel = new QLabel("LPA --°C", statusBar);
    m_lpaTempLabel->setStyleSheet(temperatureStyle(0));
    layout->addWidget(m_lpaTempLabel);

    m_paTempLabel = new QLabel("PA --°C", statusBar);
    m_paTempLabel->setStyleSheet(temperatureStyle(0));
    layout->addWidget(m_paTempLabel);

    layout->addStretch();

    // KPA1500 status (to left of K4 status)
    m_kpa1500StatusLabel = new QLabel("", statusBar);
    m_kpa1500StatusLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                            .arg(K4Styles::Colors::InactiveGray)
                                            .arg(K4Styles::Dimensions::FontSizeButton));
    m_kpa1500StatusLabel->hide(); // Hidden when not enabled
    layout->addWidget(m_kpa1500StatusLabel);

    // K4 Connection status
    m_connectionStatusLabel = new QLabel("K4", statusBar);
    m_connectionStatusLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                               .arg(K4Styles::Colors::InactiveGray)
                                               .arg(K4Styles::Dimensions::FontSizeButton));
    layout->addWidget(m_connectionStatusLabel);

    auto *netHealth = new PhoneNetHealthWidget(statusBar);
    layout->addWidget(netHealth);
    connect(m_tcpClient, &TcpClient::latencyChanged, netHealth,
            [netHealth](int ms) { netHealth->setLatency(ms); });
    connect(m_tcpClient, &TcpClient::stateChanged, netHealth, [netHealth](TcpClient::ConnectionState state) {
        netHealth->setConnected(state == TcpClient::Connected);
    });
    connect(m_tcpClient->protocol(), &Protocol::audioSequenceReceived, netHealth,
            [netHealth](quint8 sequence) { netHealth->setAudioSequence(sequence); });
    connect(m_audioEngine, &AudioEngine::bufferStatus, netHealth,
            [netHealth](int bytes, int maximum, bool prebuffering) {
                netHealth->setBufferStatus(bytes, maximum, prebuffering);
            });

    if (K4Styles::isCompactLayout()) {
        // Keep the complete at-a-glance desktop header on a landscape phone.
        // Compact typography preserves room for radio identity, UTC clock,
        // electrical readings, temperatures and link health on one row.
        m_titleLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 9px;")
                                        .arg(K4Styles::Colors::TextWhite));
        m_dateTimeLabel->setStyleSheet(QString("color: %1; font-size: 8px;").arg(K4Styles::Colors::TextGray));
        layout->setSpacing(8);
        m_kpa1500StatusLabel->hide();
    }
}

void MainWindow::setupVfoSection(QWidget *parent) {
    // Main vertical layout: VFO row on top, antenna row below
    auto *mainVLayout = new QVBoxLayout(parent);
    // Align the complete VFO block with the top of the live console on a
    // phone.  The desktop keeps its visual breathing room.
    const int vfoVerticalMargin = K4Styles::isCompactLayout() ? 0 : K4Styles::Dimensions::PaddingSmall;
    mainVLayout->setContentsMargins(K4Styles::Dimensions::PaddingSmall, vfoVerticalMargin,
                                    K4Styles::Dimensions::PaddingSmall, vfoVerticalMargin);
    // Keep the antenna state directly beneath the receiver indicators on a
    // phone; the old compact-layout gap crowded the filter row below.
    mainVLayout->setSpacing(K4Styles::isCompactLayout() ? 0 : 4);

    // Top row: VFO A | Center | VFO B
    auto *vfoRowWidget = new QWidget(parent);
    auto *layout = new QHBoxLayout(vfoRowWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(K4Styles::Dimensions::PopupButtonSpacing);

    // ===== VFO A (Left - Amber) - Using VFOWidget =====
    m_vfoA = new VFOWidget(VFOWidget::VFO_A, parent);

    // Connect VFO A click to toggle mini-pan (send CAT to enable Mini-Pan streaming)
    connect(m_vfoA, &VFOWidget::normalContentClicked, this, [this]() {
        m_vfoA->showMiniPan();
        m_radioState->setMiniPanAEnabled(true); // Set state BEFORE sending CAT (K4 doesn't echo)
        m_tcpClient->sendCAT("#MP1;");          // Enable Mini-Pan A streaming
    });
    connect(m_vfoA, &VFOWidget::miniPanClicked, this, [this]() {
        m_radioState->setMiniPanAEnabled(false); // Set state BEFORE sending CAT
        m_tcpClient->sendCAT("#MP0;");           // Disable Mini-Pan A streaming
    });

    // Connect VFO A frequency entry - send FA command then query to refresh display
    connect(m_vfoA, &VFOWidget::frequencyEntered, this, [this](const QString &freqString) {
        // FA accepts 1-11 digits: 1-2 = MHz, 3-5 = kHz, 6+ = Hz
        m_tcpClient->sendCAT(QString("FA%1;FA;").arg(freqString));
    });

    // Connect VFO A wheel tuning - same pattern as panadapter wheel tuning
    connect(m_vfoA, &VFOWidget::frequencyScrolled, this, [this](int steps) {
        if (!m_tcpClient->isConnected())
            return;
        quint64 currentFreq = m_radioState->vfoA();
        int stepHz = tuningStepToHz(m_radioState->tuningStep());
        qint64 newFreq = static_cast<qint64>(currentFreq) + static_cast<qint64>(steps) * stepHz;
        if (newFreq > 0) {
            QString cmd = QString("FA%1;").arg(static_cast<quint64>(newFreq));
            m_tcpClient->sendCAT(cmd);
            m_radioState->parseCATCommand(cmd);
        }
    });

    // Set Mini-Pan A colors to cyan (matching VFO A theme)
    m_vfoA->setMiniPanSpectrumColor(QColor(K4Styles::Colors::VfoACyan));
    QColor vfoAPassband(K4Styles::Colors::VfoACyan);
    vfoAPassband.setAlpha(64);
    m_vfoA->setMiniPanPassbandColor(vfoAPassband);

    layout->addWidget(m_vfoA, 1, Qt::AlignTop);

    // ===== Center Section =====
    auto *centerWidget = new QWidget(parent);
    centerWidget->setFixedWidth(K4Styles::Dimensions::CenterPanelWidth);
    centerWidget->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::Background));
    auto *centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setContentsMargins(K4Styles::isCompactLayout() ? 2 : 4,
                                     K4Styles::isCompactLayout() ? 0 : 4,
                                     K4Styles::isCompactLayout() ? 2 : 4,
                                     K4Styles::isCompactLayout() ? 0 : 4);
    centerLayout->setSpacing(K4Styles::isCompactLayout() ? 0 : 3);
    QHBoxLayout *compactStatusRow = nullptr;

    // Row 1: VFO Row with absolute positioning for perfect TX centering
    // Uses VfoRowWidget to position A, TX, B, SUB/DIV independently
    m_vfoRow = new VfoRowWidget(centerWidget);
    centerLayout->addWidget(m_vfoRow);

    // Get pointers to VfoRowWidget children for signal connections
    m_vfoASquare = m_vfoRow->vfoASquare();
    m_vfoBSquare = m_vfoRow->vfoBSquare();
    m_modeALabel = m_vfoRow->modeALabel();
    m_modeBLabel = m_vfoRow->modeBLabel();
    m_txIndicator = m_vfoRow->txIndicator();
    // Main QK4 uses the central TX label to select the transmit VFO by
    // toggling SPLIT.  It is an operating control, not the PTT button.
    m_txIndicator->setCursor(Qt::PointingHandCursor);
    m_txIndicator->installEventFilter(this);
    m_txTriangle = m_vfoRow->txTriangle();
    m_txTriangleB = m_vfoRow->txTriangleB();
    m_testLabel = m_vfoRow->testLabel();
    m_subLabel = m_vfoRow->subLabel();
    m_divLabel = m_vfoRow->divLabel();

    // Install event filters for clickable labels
    m_vfoASquare->installEventFilter(this);
    m_vfoBSquare->installEventFilter(this);
    m_modeALabel->installEventFilter(this);
    m_modeBLabel->installEventFilter(this);

    // SPLIT indicator
    m_splitLabel = new QLabel("SPLIT OFF", centerWidget);
    m_splitLabel->setAlignment(Qt::AlignCenter);
    m_splitLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(K4Styles::Colors::AccentAmber));
    if (!K4Styles::isCompactLayout())
        centerLayout->addWidget(m_splitLabel);

    // B SET indicator (green rounded rect with black text, hidden by default)
    m_bSetLabel = new QLabel("B SET", centerWidget);
    m_bSetLabel->setAlignment(Qt::AlignCenter);
    m_bSetLabel->setStyleSheet(QString("background-color: %1;"
                                       "color: black;"
                                       "font-size: %2px;"
                                       "font-weight: bold;"
                                       "border-radius: 4px;"
                                       "padding: 2px 8px;")
                                   .arg(K4Styles::Colors::StatusGreen)
                                   .arg(K4Styles::Dimensions::FontSizeButton));
    // The indicator is an off-only shortcut. Activating B SET remains a CTRL
    // panel action so the nearby TX transmit-VFO selector keeps its own target.
    m_bSetLabel->setCursor(Qt::PointingHandCursor);
    m_bSetLabel->installEventFilter(this);
    m_bSetLabel->setVisible(false);
    centerLayout->addWidget(m_bSetLabel, 0, Qt::AlignHCenter);

    // Message Bank indicator
    m_msgBankLabel = new QLabel("MSG: I", centerWidget);
    m_msgBankLabel->setAlignment(Qt::AlignCenter);
    m_msgBankLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(K4Styles::Colors::TextGray));
    if (K4Styles::isCompactLayout()) {
        // Share one shallow status line on a phone. It is inserted below the
        // filter block so the filter shapes occupy the highest available band.
        compactStatusRow = new QHBoxLayout();
        compactStatusRow->setContentsMargins(0, 0, 0, 0);
        compactStatusRow->setSpacing(6);
        compactStatusRow->addWidget(m_splitLabel, 1);
        compactStatusRow->addWidget(m_msgBankLabel, 1);
    } else {
        centerLayout->addWidget(m_msgBankLabel);
    }

    // RIT/XIT Box with border - constrained size
    // Supports mouse wheel to adjust RIT/XIT offset
    m_ritXitBox = new QWidget(centerWidget);
    m_ritXitBox->setObjectName("ritXitBox");
    m_ritXitBox->setStyleSheet(QString("#ritXitBox { border: 1px solid %1; }").arg(K4Styles::Colors::InactiveGray));
    m_ritXitBox->setMaximumWidth(80);
    m_ritXitBox->setMaximumHeight(40);
    m_ritXitBox->installEventFilter(this);
    m_ritXitBox->setCursor(Qt::PointingHandCursor);
    auto *ritXitLayout = new QVBoxLayout(m_ritXitBox);
    ritXitLayout->setContentsMargins(1, 2, 1, 2);
    ritXitLayout->setSpacing(1);

    auto *ritXitLabelsRow = new QHBoxLayout();
    ritXitLabelsRow->setContentsMargins(11, 0, 11, 0);
    ritXitLabelsRow->setSpacing(8);

    m_ritLabel = new QLabel("RIT", m_ritXitBox);
    m_ritLabel->setStyleSheet(QString("color: %1; font-size: 10px; border: none;").arg(K4Styles::Colors::InactiveGray));
    m_ritLabel->setCursor(Qt::PointingHandCursor);
    m_ritLabel->installEventFilter(this);
    ritXitLabelsRow->addWidget(m_ritLabel);

    m_xitLabel = new QLabel("XIT", m_ritXitBox);
    m_xitLabel->setStyleSheet(QString("color: %1; font-size: 10px; border: none;").arg(K4Styles::Colors::InactiveGray));
    m_xitLabel->setCursor(Qt::PointingHandCursor);
    m_xitLabel->installEventFilter(this);
    ritXitLabelsRow->addWidget(m_xitLabel);

    ritXitLabelsRow->setAlignment(Qt::AlignCenter);
    ritXitLayout->addLayout(ritXitLabelsRow);

    // Separator line between labels and value (spans full width)
    auto *ritXitSeparator = new QFrame(m_ritXitBox);
    ritXitSeparator->setFrameShape(QFrame::HLine);
    ritXitSeparator->setFrameShadow(QFrame::Plain);
    ritXitSeparator->setStyleSheet(QString("background-color: %1; border: none;").arg(K4Styles::Colors::InactiveGray));
    ritXitSeparator->setFixedHeight(K4Styles::Dimensions::SeparatorHeight);
    ritXitLayout->addWidget(ritXitSeparator);

    m_ritXitValueLabel = new QLabel("+0.00", m_ritXitBox);
    m_ritXitValueLabel->setAlignment(Qt::AlignCenter);
    m_ritXitValueLabel->setStyleSheet(
        QString("color: %1; font-size: %2px; font-weight: bold; border: none; padding: 0 11px;")
            .arg(K4Styles::Colors::InactiveGray)
            .arg(K4Styles::Dimensions::FontSizePopup)); // Grey until RIT/XIT is enabled
    m_ritXitValueLabel->installEventFilter(this);
    m_ritXitValueLabel->setCursor(Qt::PointingHandCursor);
    ritXitLayout->addWidget(m_ritXitValueLabel);

    // Create filter/RIT/XIT row - filter indicators flanking the RIT/XIT box
    auto *filterRitXitRow = new QHBoxLayout();
    filterRitXitRow->setContentsMargins(0, 0, 0, 0);
    filterRitXitRow->setSpacing(0);

    // VFO A filter indicator (left side, cyan #00BFFF to match VFO A square/slider)
    m_filterAWidget = new FilterIndicatorWidget(centerWidget);
    m_filterAWidget->setShapeColor(QColor(0x00, 0xBF, 0xFF), QColor(0x00, 0xBF, 0xFF)); // Cyan solid
    m_filterAWidget->setCursor(Qt::PointingHandCursor);
    m_filterAWidget->installEventFilter(this);
    filterRitXitRow->addWidget(m_filterAWidget);
    filterRitXitRow->addStretch();

    // RIT/XIT box (centered)
    filterRitXitRow->addWidget(m_ritXitBox);

    filterRitXitRow->addStretch();

    // VFO B filter indicator (right side, green #00FF00 to match VFO B square/slider)
    m_filterBWidget = new FilterIndicatorWidget(centerWidget);
    m_filterBWidget->setShapeColor(QColor(0x00, 0xFF, 0x00), QColor(0x00, 0xFF, 0x00)); // Green solid
    m_filterBWidget->setCursor(Qt::PointingHandCursor);
    m_filterBWidget->installEventFilter(this);
    filterRitXitRow->addWidget(m_filterBWidget);

    centerLayout->addLayout(filterRitXitRow);

    if (compactStatusRow)
        centerLayout->addLayout(compactStatusRow);

    // VOX / ATU / QSK indicator row (fixed-height container so visibility toggles don't shift layout)
    auto *indicatorContainer = new QWidget(centerWidget);
    indicatorContainer->setFixedHeight(K4Styles::Dimensions::DialogMargin);
    auto *indicatorLayout = new QHBoxLayout(indicatorContainer);
    indicatorLayout->setContentsMargins(0, 0, 0, 0);
    indicatorLayout->setSpacing(8);

    indicatorLayout->addStretch();

    // VOX indicator - orange when on, grey when off
    m_voxLabel = new QLabel("VOX", indicatorContainer);
    m_voxLabel->setAlignment(Qt::AlignCenter);
    m_voxLabel->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::TextGray));
    indicatorLayout->addWidget(m_voxLabel);

    // ATU indicator (orange when AUTO, grey when off)
    m_atuLabel = new QLabel("ATU", indicatorContainer);
    m_atuLabel->setAlignment(Qt::AlignCenter);
    m_atuLabel->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::TextGray));
    indicatorLayout->addWidget(m_atuLabel);

    // QSK indicator - white when on, grey when off
    m_qskLabel = new QLabel("QSK", indicatorContainer);
    m_qskLabel->setAlignment(Qt::AlignCenter);
    m_qskLabel->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::TextGray));
    indicatorLayout->addWidget(m_qskLabel);

    indicatorLayout->addStretch();

    centerLayout->addWidget(indicatorContainer);

    if (K4Styles::isCompactLayout()) {
        // These are operating feedback, not optional controls.  They must stay
        // visible while a phone operator changes filters or offsets from a
        // drawer, otherwise there is no confirmation of what changed on the
        // radio.
        m_msgBankLabel->show();
        m_ritXitBox->show();
        m_filterAWidget->show();
        m_filterBWidget->show();
        indicatorContainer->setFixedHeight(14);
    }

    // ===== Memory Buttons Row (M1-M4, REC, STORE, RCL) =====
    centerLayout->addStretch(); // Push buttons to vertical center

    // Helper lambda to create memory button with optional sub-label
    // Uses sidePanelButton/sidePanelButtonLight styles for consistency
    // Container: VBox with 2px spacing, button centered, sub-label below
    // Button: MemoryButtonWidth x ButtonHeightSmall (42x28)
    // Sub-label: FontSizeSmall (8px), AccentAmber color
    auto createMemoryButton = [centerWidget](const QString &label, const QString &subLabel,
                                             bool isLighter) -> QWidget * {
        auto *container = new QWidget(centerWidget);
        auto *layout = new QVBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);

        auto *btn = new QPushButton(label, container);
        btn->setFixedSize(K4Styles::Dimensions::MemoryButtonWidth, K4Styles::Dimensions::ButtonHeightSmall);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(isLighter ? K4Styles::sidePanelButtonLight() : K4Styles::sidePanelButton());
        layout->addWidget(btn, 0, Qt::AlignHCenter);

        // Add sub-label if provided
        if (!subLabel.isEmpty()) {
            auto *sub = new QLabel(subLabel, container);
            sub->setStyleSheet(QString("color: %1; font-size: %2px;")
                                   .arg(K4Styles::Colors::AccentAmber)
                                   .arg(K4Styles::Dimensions::FontSizeSmall));
            sub->setAlignment(Qt::AlignCenter);
            layout->addWidget(sub);
        }

        return container;
    };

    // Single row: M1-M4 group, REC, STORE, RCL (all centered)
    auto *memoryRow = new QHBoxLayout();
    memoryRow->setContentsMargins(0, 0, 0, 0);
    memoryRow->setSpacing(4);

    memoryRow->addStretch();

    // M1-M4 group with MESSAGE label underneath
    auto *messageGroup = new QWidget(centerWidget);
    auto *messageGroupLayout = new QVBoxLayout(messageGroup);
    messageGroupLayout->setContentsMargins(0, 0, 0, 0);
    messageGroupLayout->setSpacing(2);

    // M1-M4 button row
    auto *m1m4Row = new QHBoxLayout();
    m1m4Row->setContentsMargins(0, 0, 0, 0);
    m1m4Row->setSpacing(4);

    // Helper to create just a button (no sub-label container)
    // Button: MemoryButtonWidth x ButtonHeightSmall (42x28), dark sidePanelButton style
    auto createSimpleButton = [centerWidget](const QString &label) -> QPushButton * {
        auto *btn = new QPushButton(label, centerWidget);
        btn->setFixedSize(K4Styles::Dimensions::MemoryButtonWidth, K4Styles::Dimensions::ButtonHeightSmall);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(K4Styles::sidePanelButton());
        return btn;
    };

    m_m1Btn = createSimpleButton("M1");
    m1m4Row->addWidget(m_m1Btn);

    m_m2Btn = createSimpleButton("M2");
    m1m4Row->addWidget(m_m2Btn);

    m_m3Btn = createSimpleButton("M3");
    m1m4Row->addWidget(m_m3Btn);

    m_m4Btn = createSimpleButton("M4");
    m1m4Row->addWidget(m_m4Btn);

    messageGroupLayout->addLayout(m1m4Row);

    // MESSAGE label with connecting lines: ——— MESSAGE ———
    auto *messageLabel = new QWidget(messageGroup);
    auto *messageLabelLayout = new QHBoxLayout(messageLabel);
    messageLabelLayout->setContentsMargins(0, 0, 0, 0);
    messageLabelLayout->setSpacing(2);

    auto *leftLine = new QFrame(messageLabel);
    leftLine->setFrameShape(QFrame::HLine);
    leftLine->setStyleSheet(QString("background-color: %1; max-height: 1px;").arg(K4Styles::Colors::BorderSelected));
    leftLine->setFixedHeight(K4Styles::Dimensions::SeparatorHeight);

    auto *msgText = new QLabel("MESSAGE", messageLabel);
    msgText->setStyleSheet(QString("color: %1; font-size: %2px;")
                               .arg(K4Styles::Colors::BorderSelected)
                               .arg(K4Styles::Dimensions::FontSizeSmall));
    msgText->setAlignment(Qt::AlignCenter);

    auto *rightLine = new QFrame(messageLabel);
    rightLine->setFrameShape(QFrame::HLine);
    rightLine->setStyleSheet(QString("background-color: %1; max-height: 1px;").arg(K4Styles::Colors::BorderSelected));
    rightLine->setFixedHeight(K4Styles::Dimensions::SeparatorHeight);

    messageLabelLayout->addWidget(leftLine, 1);
    messageLabelLayout->addWidget(msgText, 0);
    messageLabelLayout->addWidget(rightLine, 1);

    messageGroupLayout->addWidget(messageLabel);
    memoryRow->addWidget(messageGroup);

    // REC (dark grey like M keys, BANK sub-label)
    auto *recContainer = createMemoryButton("REC", "BANK", false);
    m_recBtn = recContainer->findChild<QPushButton *>();
    memoryRow->addWidget(recContainer);

    // STORE (lighter grey, AF REC sub-label)
    auto *storeContainer = createMemoryButton("STORE", "AF REC", true);
    m_storeBtn = storeContainer->findChild<QPushButton *>();
    memoryRow->addWidget(storeContainer);

    // RCL (lighter grey, AF PLAY sub-label)
    auto *rclContainer = createMemoryButton("RCL", "AF PLAY", true);
    m_rclBtn = rclContainer->findChild<QPushButton *>();
    memoryRow->addWidget(rclContainer);

    if (K4Styles::isCompactLayout()) {
        // On phones, reclaim vertical space by hiding the least-used memory strip.
        messageGroup->hide();
        recContainer->hide();
        storeContainer->hide();
        rclContainer->hide();
    }

    memoryRow->addStretch();
    centerLayout->addLayout(memoryRow);

    centerLayout->addStretch(); // Balance below
    layout->addWidget(centerWidget);

    // ===== VFO B (Right - Cyan) - Using VFOWidget =====
    m_vfoB = new VFOWidget(VFOWidget::VFO_B, parent);

    // Set Mini-Pan B colors to green (matching VFO B theme)
    m_vfoB->setMiniPanSpectrumColor(QColor(K4Styles::Colors::VfoBGreen));
    QColor vfoBPassband(K4Styles::Colors::VfoBGreen);
    vfoBPassband.setAlpha(64);
    m_vfoB->setMiniPanPassbandColor(vfoBPassband);

    // Connect VFO B click to toggle mini-pan (send CAT to enable Mini-Pan streaming)
    // Only allow mini pan B if SUB RX is on or VFOs are on the same band
    connect(m_vfoB, &VFOWidget::normalContentClicked, this, [this]() {
        // Block mini pan B if VFOs are on different bands and SUB RX is off
        // (K4 cannot provide separate Sub RX spectrum without SUB RX enabled)
        if (areVfosOnDifferentBands() && !m_radioState->subReceiverEnabled()) {
            qDebug() << "Mini-Pan B blocked: VFOs on different bands and SUB RX is off";
            return;
        }
        m_vfoB->showMiniPan();
        m_radioState->setMiniPanBEnabled(true); // Set state BEFORE sending CAT (K4 doesn't echo)
        m_tcpClient->sendCAT("#MP$1;");         // Enable Mini-Pan B (Sub RX) streaming
    });
    connect(m_vfoB, &VFOWidget::miniPanClicked, this, [this]() {
        m_radioState->setMiniPanBEnabled(false); // Set state BEFORE sending CAT
        m_tcpClient->sendCAT("#MP$0;");          // Disable Mini-Pan B streaming
    });

    // Connect VFO B frequency entry - send FB command then query to refresh display
    connect(m_vfoB, &VFOWidget::frequencyEntered, this, [this](const QString &freqString) {
        // FB accepts 1-11 digits: 1-2 = MHz, 3-5 = kHz, 6+ = Hz
        m_tcpClient->sendCAT(QString("FB%1;FB;").arg(freqString));
    });

    // Connect VFO B wheel tuning - same pattern as panadapter wheel tuning
    connect(m_vfoB, &VFOWidget::frequencyScrolled, this, [this](int steps) {
        if (!m_tcpClient->isConnected())
            return;
        quint64 currentFreq = m_radioState->vfoB();
        int stepHz = tuningStepToHz(m_radioState->tuningStepB());
        qint64 newFreq = static_cast<qint64>(currentFreq) + static_cast<qint64>(steps) * stepHz;
        if (newFreq > 0) {
            QString cmd = QString("FB%1;").arg(static_cast<quint64>(newFreq));
            m_tcpClient->sendCAT(cmd);
            m_radioState->parseCATCommand(cmd);
        }
    });

    layout->addWidget(m_vfoB, 1, Qt::AlignTop);

    // ===== KPA1500 Floating Window =====
    // Created as a separate floating window, not in the VFO row layout
    m_kpa1500Window = new KPA1500Window(this);
    m_kpa1500Window->hide(); // Hidden by default, shown when enabled + connected

    // Add the VFO row to main layout
    mainVLayout->addWidget(vfoRowWidget);

    // NOTE: TX meters are now integrated into VFOWidgets as multifunction S/Po meters
    // (see VFOWidget::m_txMeter - displays S-meter when RX, Po when TX)

    // ===== Antenna Row (below the receiver status labels) =====
    // Match mainline QK4: [Main RX antenna] [TX antenna] [Sub RX antenna].
    // This row is operating state, so it remains visible in compact mode.
    auto *antennaRow = new QHBoxLayout();
    antennaRow->setContentsMargins(K4Styles::isCompactLayout() ? 3 : 8, 0,
                                   K4Styles::isCompactLayout() ? 3 : 8, 0);
    antennaRow->setSpacing(0);
    const int antennaFontSize = K4Styles::isCompactLayout() ? 9 : 11;

    m_rxAntALabel = new QLabel("1:ANT1", parent);
    m_rxAntALabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_rxAntALabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                     .arg(K4Styles::Colors::TextWhite)
                                     .arg(antennaFontSize));

    m_txAntennaLabel = new QLabel("1:ANT1", parent);
    m_txAntennaLabel->setAlignment(Qt::AlignCenter);
    m_txAntennaLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                        .arg(K4Styles::Colors::AccentAmber)
                                        .arg(antennaFontSize));

    m_rxAntBLabel = new QLabel("1:ANT1", parent);
    m_rxAntBLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_rxAntBLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                     .arg(K4Styles::Colors::TextWhite)
                                     .arg(antennaFontSize));

    for (QLabel *label : {m_rxAntALabel, m_txAntennaLabel, m_rxAntBLabel}) {
        label->setMinimumWidth(0);
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        if (K4Styles::isCompactLayout())
            label->setFixedHeight(13);
        antennaRow->addWidget(label, 1);
    }
    mainVLayout->addLayout(antennaRow);
}

void MainWindow::setupSpectrumPlaceholder(QWidget *parent) {
    // Container for spectrum displays
    m_spectrumContainer = new QWidget(parent);
    m_spectrumContainer->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::DarkBackground));
    // Maintain a useful touch target on phones while letting the live console
    // fit above its fixed PTT/tuning dock.
    m_spectrumContainer->setMinimumHeight(K4Styles::isCompactLayout() ? 90
                                                                       : K4Styles::Dimensions::SpectrumMinHeight);

    // Use QHBoxLayout for side-by-side panadapters (Main left, Sub right)
    auto *layout = new QHBoxLayout(m_spectrumContainer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2); // Small gap between panadapters

    // Main panadapter for VFO A (left side) - QRhiWidget with Metal/DirectX/Vulkan
    m_panadapterA = new PanadapterRhiWidget(m_spectrumContainer);
    m_panadapterA->setSpectrumLineColor(QColor(K4Styles::Colors::VfoACyan));
    // dB range set via setScale()/setRefLevel() from radio's #SCL/#REF values
    m_panadapterA->setSpectrumRatio(0.35f);
    m_panadapterA->setGridEnabled(true);
    // Primary VFO A uses default cyan passband
    // Secondary VFO B uses green passband
    QColor vfoBPassbandAlpha(K4Styles::Colors::VfoBGreen);
    vfoBPassbandAlpha.setAlpha(64);
    m_panadapterA->setSecondaryPassbandColor(vfoBPassbandAlpha);
    m_panadapterA->setSecondaryMarkerColor(QColor(K4Styles::Colors::VfoBGreen));
    m_panadapterA->setSecondaryVisible(true);
    layout->addWidget(m_panadapterA);

    // Sub panadapter for VFO B (right side) - QRhiWidget with Metal/DirectX/Vulkan
    m_panadapterB = new PanadapterRhiWidget(m_spectrumContainer);
    m_panadapterB->setSpectrumLineColor(QColor(K4Styles::Colors::VfoBGreen));
    // dB range set via setScale()/setRefLevel() from radio's #SCL/#REF$ values
    m_panadapterB->setSpectrumRatio(0.35f);
    m_panadapterB->setGridEnabled(true);
    // Primary VFO B uses green passband
    m_panadapterB->setPassbandColor(vfoBPassbandAlpha);
    m_panadapterB->setFrequencyMarkerColor(QColor(K4Styles::Colors::VfoBGreen));
    // Secondary VFO A uses cyan passband
    QColor vfoAPassbandAlpha(K4Styles::Colors::VfoACyan);
    vfoAPassbandAlpha.setAlpha(64);
    m_panadapterB->setSecondaryPassbandColor(vfoAPassbandAlpha);
    m_panadapterB->setSecondaryMarkerColor(QColor(K4Styles::Colors::VfoACyan));
    m_panadapterB->setSecondaryVisible(true);
    layout->addWidget(m_panadapterB);
    m_panadapterB->hide(); // Start hidden (MainOnly mode)

    // Span control buttons - overlay on panadapter (lower right, above freq labels)
    // Note: rgba used intentionally for transparent overlay effect on spectrum
    QString btnStyle = QString("QPushButton { background: rgba(0,0,0,0.6); color: %1; "
                               "border: 1px solid %2; border-radius: 4px; "
                               "font-size: %3px; font-weight: bold; min-width: 28px; min-height: 24px; }"
                               "QPushButton:hover { background: rgba(80,80,80,0.8); }")
                           .arg(K4Styles::Colors::TextWhite)
                           .arg(K4Styles::Colors::InactiveGray)
                           .arg(K4Styles::Dimensions::FontSizePopup);

    // Main panadapter (A) buttons
    m_spanDownBtn = new QPushButton("-", m_panadapterA);
    m_spanDownBtn->setStyleSheet(btnStyle);
    m_spanDownBtn->setFixedSize(K4Styles::Dimensions::ButtonHeightSmall, K4Styles::Dimensions::ButtonHeightMini);

    m_spanUpBtn = new QPushButton("+", m_panadapterA);
    m_spanUpBtn->setStyleSheet(btnStyle);
    m_spanUpBtn->setFixedSize(K4Styles::Dimensions::ButtonHeightSmall, K4Styles::Dimensions::ButtonHeightMini);

    m_centerBtn = new QPushButton("C", m_panadapterA);
    m_centerBtn->setStyleSheet(btnStyle);
    m_centerBtn->setFixedSize(K4Styles::Dimensions::ButtonHeightSmall, K4Styles::Dimensions::ButtonHeightMini);

    // Sub panadapter (B) buttons
    m_spanDownBtnB = new QPushButton("-", m_panadapterB);
    m_spanDownBtnB->setStyleSheet(btnStyle);
    m_spanDownBtnB->setFixedSize(K4Styles::Dimensions::ButtonHeightSmall, K4Styles::Dimensions::ButtonHeightMini);

    m_spanUpBtnB = new QPushButton("+", m_panadapterB);
    m_spanUpBtnB->setStyleSheet(btnStyle);
    m_spanUpBtnB->setFixedSize(K4Styles::Dimensions::ButtonHeightSmall, K4Styles::Dimensions::ButtonHeightMini);

    m_centerBtnB = new QPushButton("C", m_panadapterB);
    m_centerBtnB->setStyleSheet(btnStyle);
    m_centerBtnB->setFixedSize(K4Styles::Dimensions::ButtonHeightSmall, K4Styles::Dimensions::ButtonHeightMini);

    // VFO indicator badges - bottom-left corner of waterfall, tab shape with top-right rounded
    QString vfoIndicatorStyle = QString("QLabel { background: %1; color: black; "
                                        "font-size: %2px; font-weight: bold; "
                                        "border-top-left-radius: 0px; border-top-right-radius: %3px; "
                                        "border-bottom-left-radius: 0px; border-bottom-right-radius: 0px; }")
                                    .arg(K4Styles::Colors::OverlayBackground)
                                    .arg(K4Styles::Dimensions::FontSizeTitle)
                                    .arg(K4Styles::Dimensions::BorderRadiusLarge);

    m_vfoIndicatorA = new QLabel("A", m_panadapterA);
    m_vfoIndicatorA->setStyleSheet(vfoIndicatorStyle);
    m_vfoIndicatorA->setFixedSize(K4Styles::Dimensions::VfoIndicatorBadgeWidth,
                                  K4Styles::Dimensions::VfoIndicatorBadgeHeight);
    m_vfoIndicatorA->setAlignment(Qt::AlignCenter);

    m_vfoIndicatorB = new QLabel("B", m_panadapterB);
    m_vfoIndicatorB->setStyleSheet(vfoIndicatorStyle);
    m_vfoIndicatorB->setFixedSize(K4Styles::Dimensions::VfoIndicatorBadgeWidth,
                                  K4Styles::Dimensions::VfoIndicatorBadgeHeight);
    m_vfoIndicatorB->setAlignment(Qt::AlignCenter);

    // Position buttons (will be repositioned in resizeEvent of panadapter)
    // Triangle layout: C centered above, - and + below (bottom-right)
    m_spanDownBtn->move(m_panadapterA->width() - 70, m_panadapterA->height() - 45);
    m_spanUpBtn->move(m_panadapterA->width() - 35, m_panadapterA->height() - 45);
    m_centerBtn->move(m_panadapterA->width() - 52, m_panadapterA->height() - 73);

    m_spanDownBtnB->move(m_panadapterB->width() - 70, m_panadapterB->height() - 45);
    m_spanUpBtnB->move(m_panadapterB->width() - 35, m_panadapterB->height() - 45);
    m_centerBtnB->move(m_panadapterB->width() - 52, m_panadapterB->height() - 73);

    // VFO indicators at bottom-left corner, flush with edges
    m_vfoIndicatorA->move(0, m_panadapterA->height() - K4Styles::Dimensions::VfoIndicatorBadgeHeight);
    m_vfoIndicatorB->move(0, m_panadapterB->height() - K4Styles::Dimensions::VfoIndicatorBadgeHeight);

    // Span adjustment for Main: - reduces span, + increases span.
    connect(m_spanDownBtn, &QPushButton::clicked, this, [this]() {
        int currentSpan = m_radioState->spanHz();
        int newSpan = getNextSpanDown(currentSpan); // - reduces span
        if (newSpan != currentSpan) {
            m_radioState->setSpanHz(newSpan);
            m_tcpClient->sendCAT(QString("#SPN%1;").arg(newSpan));
        }
    });

    connect(m_spanUpBtn, &QPushButton::clicked, this, [this]() {
        int currentSpan = m_radioState->spanHz();
        int newSpan = getNextSpanUp(currentSpan); // + increases span
        if (newSpan != currentSpan) {
            m_radioState->setSpanHz(newSpan);
            m_tcpClient->sendCAT(QString("#SPN%1;").arg(newSpan));
        }
    });

    connect(m_centerBtn, &QPushButton::clicked, this, [this]() { m_tcpClient->sendCAT("FC;"); });

    // Span adjustment for Sub: uses $ suffix for Sub RX commands
    connect(m_spanDownBtnB, &QPushButton::clicked, this, [this]() {
        int currentSpan = m_radioState->spanHzB();
        int newSpan = getNextSpanDown(currentSpan); // - reduces span
        if (newSpan != currentSpan) {
            m_radioState->setSpanHzB(newSpan);
            m_tcpClient->sendCAT(QString("#SPN$%1;").arg(newSpan));
        }
    });

    connect(m_spanUpBtnB, &QPushButton::clicked, this, [this]() {
        int currentSpan = m_radioState->spanHzB();
        int newSpan = getNextSpanUp(currentSpan); // + increases span
        if (newSpan != currentSpan) {
            m_radioState->setSpanHzB(newSpan);
            m_tcpClient->sendCAT(QString("#SPN$%1;").arg(newSpan));
        }
    });

    connect(m_centerBtnB, &QPushButton::clicked, this, [this]() { m_tcpClient->sendCAT("FC$;"); });

    // Install event filter to reposition span buttons when panadapters resize
    m_panadapterA->installEventFilter(this);
    m_panadapterB->installEventFilter(this);

    // Debug: Connect to renderFailed signal to diagnose QRhiWidget issues
    connect(m_panadapterA, &QRhiWidget::renderFailed, this,
            []() { qCritical() << "!!! PanadapterA renderFailed() emitted - QRhi could not be obtained !!!"; });
    connect(m_panadapterB, &QRhiWidget::renderFailed, this,
            []() { qCritical() << "!!! PanadapterB renderFailed() emitted - QRhi could not be obtained !!!"; });

    // Update panadapter when frequency/mode changes
    connect(m_radioState, &RadioState::frequencyChanged, this,
            [this](quint64 freq) { m_panadapterA->setTunedFrequency(freq); });
    connect(m_radioState, &RadioState::modeChanged, this,
            [this](RadioState::Mode mode) { m_panadapterA->setMode(RadioState::modeToString(mode)); });
    connect(m_radioState, &RadioState::filterBandwidthChanged, this,
            [this](int bw) { m_panadapterA->setFilterBandwidth(bw); });
    connect(m_radioState, &RadioState::ifShiftChanged, this, [this](int shift) { m_panadapterA->setIfShift(shift); });
    connect(m_radioState, &RadioState::cwPitchChanged, this, [this](int pitch) { m_panadapterA->setCwPitch(pitch); });

    // Notch filter visualization
    connect(m_radioState, &RadioState::notchChanged, this, [this]() {
        bool enabled = m_radioState->manualNotchEnabled();
        int pitch = m_radioState->manualNotchPitch();
        m_panadapterA->setNotchFilter(enabled, pitch);
        // Update mini-pan too (using forwarding method that handles lazy creation)
        m_vfoA->setMiniPanNotchFilter(enabled, pitch);
        // Update NTCH indicator in VFO processing row
        m_vfoA->setNotch(m_radioState->autoNotchEnabled(), m_radioState->manualNotchEnabled());
    });
    // Also update mini-pan mode when mode changes
    connect(m_radioState, &RadioState::modeChanged, this,
            [this](RadioState::Mode mode) { m_vfoA->setMiniPanMode(RadioState::modeToString(mode)); });

    // Mini-pan filter passband visualization (using forwarding methods)
    connect(m_radioState, &RadioState::filterBandwidthChanged, this,
            [this](int bw) { m_vfoA->setMiniPanFilterBandwidth(bw); });
    connect(m_radioState, &RadioState::ifShiftChanged, this, [this](int shift) { m_vfoA->setMiniPanIfShift(shift); });
    connect(m_radioState, &RadioState::cwPitchChanged, this, [this](int pitch) { m_vfoA->setMiniPanCwPitch(pitch); });

    // Tuning rate indicator (VT command)
    connect(m_radioState, &RadioState::tuningStepChanged, this, [this](int step) {
        m_vfoA->setTuningRate(step);
        m_phoneTuneStepAHz = tuningStepToHz(step);
        if (m_bottomMenuBar)
            m_bottomMenuBar->setTuneStepA(m_phoneTuneStepAHz);
    });
    connect(m_radioState, &RadioState::tuningStepBChanged, this, [this](int step) {
        m_vfoB->setTuningRate(step);
        m_phoneTuneStepBHz = tuningStepToHz(step);
        if (m_bottomMenuBar)
            m_bottomMenuBar->setTuneStepB(m_phoneTuneStepBHz);
    });

    // Mouse control: click to tune
    connect(m_panadapterA, &PanadapterRhiWidget::frequencyClicked, this, [this](qint64 freq) {
        // Guard: only send if connected and frequency is valid
        if (!m_tcpClient->isConnected() || freq <= 0)
            return;
        QString cmd = QString("FA%1;").arg(freq, 11, 10, QChar('0'));
        m_tcpClient->sendCAT(cmd);
        // Request frequency back to update UI (K4 doesn't echo SET commands)
        m_tcpClient->sendCAT("FA;");
    });

    // Mouse control: drag to tune (continuous frequency change while dragging)
    // Frequency is snapped to the current tuning rate step for consistent behavior
    connect(m_panadapterA, &PanadapterRhiWidget::frequencyDragged, this, [this](qint64 freq) {
        // Guard: only send if connected and frequency is valid
        if (!m_tcpClient->isConnected() || freq <= 0)
            return;
        int stepHz = m_phoneTuneStepAHz > 0 ? m_phoneTuneStepAHz : tuningStepToHz(m_radioState->tuningStep());
        qint64 snapped = (freq / stepHz) * stepHz;
        if (snapped <= 0)
            return;
        QString cmd = QString("FA%1;").arg(snapped, 11, 10, QChar('0'));
        m_tcpClient->sendCAT(cmd);
        // Update local state immediately for responsive UI (K4 doesn't echo SET commands)
        m_radioState->parseCATCommand(cmd);
    });

    // Mouse control: scroll wheel to adjust frequency by computed step
    connect(m_panadapterA, &PanadapterRhiWidget::frequencyScrolled, this, [this](int steps) {
        if (!m_tcpClient->isConnected())
            return;
        quint64 currentFreq = m_radioState->vfoA();
        int stepHz = m_phoneTuneStepAHz > 0 ? m_phoneTuneStepAHz : tuningStepToHz(m_radioState->tuningStep());
        qint64 newFreq = static_cast<qint64>(currentFreq) + static_cast<qint64>(steps) * stepHz;
        if (newFreq > 0) {
            QString cmd = QString("FA%1;").arg(static_cast<quint64>(newFreq));
            m_tcpClient->sendCAT(cmd);
            m_radioState->parseCATCommand(cmd);
        }
    });

    // Shift+Wheel: Adjust scale (dB range) - global setting applies to both panadapters
    connect(m_panadapterA, &PanadapterRhiWidget::scaleScrolled, this, [this](int steps) {
        if (!m_tcpClient->isConnected())
            return;
        int currentScale = m_radioState->scale();
        if (currentScale < 0)
            currentScale = 75;                                    // Default if not yet received from radio
        int newScale = qBound(10, currentScale + steps * 5, 150); // 5 dB per step
        m_tcpClient->sendCAT(QString("#SCL%1;").arg(newScale));
        // Optimistic update (scale is global) - updates both panadapters via signal
        m_radioState->setScale(newScale);
    });

    // Ctrl+Wheel: Adjust reference level for Main RX
    connect(m_panadapterA, &PanadapterRhiWidget::refLevelScrolled, this, [this](int steps) {
        if (!m_tcpClient->isConnected())
            return;
        int currentRef = m_radioState->refLevel();
        if (currentRef < -200)
            currentRef = -110; // Default if not yet received
        int newRef = qBound(-140, currentRef + steps, 10);
        m_tcpClient->sendCAT(QString("#REF%1;").arg(newRef));
        // Optimistic update
        m_panadapterA->setRefLevel(newRef);
    });

    // Right-click on panadapter A tunes VFO B (L=A R=B mode)
    connect(m_panadapterA, &PanadapterRhiWidget::frequencyRightClicked, this, [this](qint64 freq) {
        if (m_mouseQsyMode == 0) // Left Only — right-click disabled
            return;
        if (!m_tcpClient->isConnected() || freq <= 0)
            return;
        QString cmd = QString("FB%1;").arg(freq, 11, 10, QChar('0'));
        m_tcpClient->sendCAT(cmd);
        m_tcpClient->sendCAT("FB;");
    });

    connect(m_panadapterA, &PanadapterRhiWidget::frequencyRightDragged, this, [this](qint64 freq) {
        if (m_mouseQsyMode == 0) // Left Only — right-drag disabled
            return;
        if (!m_tcpClient->isConnected() || freq <= 0)
            return;
        QString cmd = QString("FB%1;").arg(freq, 11, 10, QChar('0'));
        m_tcpClient->sendCAT(cmd);
        m_radioState->parseCATCommand(cmd);
    });

    // VFO B connections
    connect(m_radioState, &RadioState::frequencyBChanged, this,
            [this](quint64 freq) { m_panadapterB->setTunedFrequency(freq); });
    connect(m_radioState, &RadioState::modeBChanged, this,
            [this](RadioState::Mode mode) { m_panadapterB->setMode(RadioState::modeToString(mode)); });
    connect(m_radioState, &RadioState::filterBandwidthBChanged, this,
            [this](int bw) { m_panadapterB->setFilterBandwidth(bw); });
    connect(m_radioState, &RadioState::ifShiftBChanged, this, [this](int shift) { m_panadapterB->setIfShift(shift); });
    connect(m_radioState, &RadioState::cwPitchChanged, this, [this](int pitch) { m_panadapterB->setCwPitch(pitch); });
    connect(m_radioState, &RadioState::notchBChanged, this, [this]() {
        bool enabled = m_radioState->manualNotchEnabledB();
        int pitch = m_radioState->manualNotchPitchB();
        m_panadapterB->setNotchFilter(enabled, pitch);
    });

    // VFO B Mini-Pan connections (mode-dependent bandwidth, using forwarding methods)
    connect(m_radioState, &RadioState::modeBChanged, this,
            [this](RadioState::Mode mode) { m_vfoB->setMiniPanMode(RadioState::modeToString(mode)); });
    connect(m_radioState, &RadioState::filterBandwidthBChanged, this,
            [this](int bw) { m_vfoB->setMiniPanFilterBandwidth(bw); });
    connect(m_radioState, &RadioState::ifShiftBChanged, this, [this](int shift) { m_vfoB->setMiniPanIfShift(shift); });
    connect(m_radioState, &RadioState::cwPitchChanged, this, [this](int pitch) { m_vfoB->setMiniPanCwPitch(pitch); });
    connect(m_radioState, &RadioState::notchBChanged, this, [this]() {
        bool enabled = m_radioState->manualNotchEnabledB();
        int pitch = m_radioState->manualNotchPitchB();
        m_vfoB->setMiniPanNotchFilter(enabled, pitch);
        // Update NTCH indicator in VFO B processing row
        m_vfoB->setNotch(m_radioState->autoNotchEnabledB(), m_radioState->manualNotchEnabledB());
    });

    // Secondary VFO passband display: VFO B state → PanadapterA's secondary
    auto updatePanadapterASecondary = [this]() {
        m_panadapterA->setSecondaryVfo(m_radioState->vfoB(), m_radioState->filterBandwidthB(),
                                       RadioState::modeToString(m_radioState->modeB()), m_radioState->ifShiftB(),
                                       m_radioState->cwPitch());
    };
    connect(m_radioState, &RadioState::frequencyBChanged, this, updatePanadapterASecondary);
    connect(m_radioState, &RadioState::modeBChanged, this, updatePanadapterASecondary);
    connect(m_radioState, &RadioState::filterBandwidthBChanged, this, updatePanadapterASecondary);
    connect(m_radioState, &RadioState::ifShiftBChanged, this, updatePanadapterASecondary);
    connect(m_radioState, &RadioState::cwPitchChanged, this, updatePanadapterASecondary);

    // Secondary VFO passband display: VFO A state → PanadapterB's secondary
    auto updatePanadapterBSecondary = [this]() {
        m_panadapterB->setSecondaryVfo(m_radioState->vfoA(), m_radioState->filterBandwidth(),
                                       RadioState::modeToString(m_radioState->mode()), m_radioState->ifShift(),
                                       m_radioState->cwPitch());
    };
    connect(m_radioState, &RadioState::frequencyChanged, this, updatePanadapterBSecondary);
    connect(m_radioState, &RadioState::modeChanged, this, updatePanadapterBSecondary);
    connect(m_radioState, &RadioState::filterBandwidthChanged, this, updatePanadapterBSecondary);
    connect(m_radioState, &RadioState::ifShiftChanged, this, updatePanadapterBSecondary);
    connect(m_radioState, &RadioState::cwPitchChanged, this, updatePanadapterBSecondary);

    // Mouse control for VFO B: click to tune
    connect(m_panadapterB, &PanadapterRhiWidget::frequencyClicked, this, [this](qint64 freq) {
        // Guard: only send if connected and frequency is valid
        if (!m_tcpClient->isConnected() || freq <= 0)
            return;
        // Touch input on Pan B always tunes VFO B. Desktop mouse behavior follows the radio setting.
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
        QString vfo = QStringLiteral("FB");
#else
        QString vfo = (m_mouseQsyMode == 1) ? "FA" : "FB";
#endif
        QString cmd = QString("%1%2;").arg(vfo).arg(freq, 11, 10, QChar('0'));
        m_tcpClient->sendCAT(cmd);
        m_tcpClient->sendCAT(vfo + ";");
    });

    // Mouse control for VFO B: drag to tune (continuous frequency change while dragging)
    // Frequency is snapped to the current tuning rate step for consistent behavior
    connect(m_panadapterB, &PanadapterRhiWidget::frequencyDragged, this, [this](qint64 freq) {
        // Guard: only send if connected and frequency is valid
        if (!m_tcpClient->isConnected() || freq <= 0)
            return;
        // Touch input on Pan B always tunes VFO B. Desktop mouse behavior follows the radio setting.
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
        bool tuneA = false;
#else
        bool tuneA = (m_mouseQsyMode == 1);
#endif
        QString vfo = tuneA ? "FA" : "FB";
        int stepHz = tuneA ? (m_phoneTuneStepAHz > 0 ? m_phoneTuneStepAHz : tuningStepToHz(m_radioState->tuningStep()))
                           : (m_phoneTuneStepBHz > 0 ? m_phoneTuneStepBHz : tuningStepToHz(m_radioState->tuningStepB()));
        qint64 snapped = (freq / stepHz) * stepHz;
        if (snapped <= 0)
            return;
        QString cmd = QString("%1%2;").arg(vfo).arg(snapped, 11, 10, QChar('0'));
        m_tcpClient->sendCAT(cmd);
        m_radioState->parseCATCommand(cmd);
    });

    // Mouse control for VFO B: scroll wheel to adjust frequency by computed step
    connect(m_panadapterB, &PanadapterRhiWidget::frequencyScrolled, this, [this](int steps) {
        if (!m_tcpClient->isConnected())
            return;
        quint64 currentFreq = m_radioState->vfoB();
        int stepHz = m_phoneTuneStepBHz > 0 ? m_phoneTuneStepBHz : tuningStepToHz(m_radioState->tuningStepB());
        qint64 newFreq = static_cast<qint64>(currentFreq) + static_cast<qint64>(steps) * stepHz;
        if (newFreq > 0) {
            QString cmd = QString("FB%1;").arg(static_cast<quint64>(newFreq));
            m_tcpClient->sendCAT(cmd);
            m_radioState->parseCATCommand(cmd);
        }
    });

    // Shift+Wheel on panadapter B: Adjust scale (same as A - global setting)
    connect(m_panadapterB, &PanadapterRhiWidget::scaleScrolled, this, [this](int steps) {
        if (!m_tcpClient->isConnected())
            return;
        int currentScale = m_radioState->scale();
        if (currentScale < 0)
            currentScale = 75;
        int newScale = qBound(10, currentScale + steps * 5, 150);
        m_tcpClient->sendCAT(QString("#SCL%1;").arg(newScale));
        // Optimistic update (scale is global) - updates both panadapters via signal
        m_radioState->setScale(newScale);
    });

    // Ctrl+Wheel on panadapter B: Adjust reference level for Sub RX
    connect(m_panadapterB, &PanadapterRhiWidget::refLevelScrolled, this, [this](int steps) {
        if (!m_tcpClient->isConnected())
            return;
        int currentRef = m_radioState->refLevelB();
        if (currentRef < -200)
            currentRef = -110;
        int newRef = qBound(-140, currentRef + steps, 10);
        m_tcpClient->sendCAT(QString("#REF$%1;").arg(newRef)); // Note: #REF$ for Sub RX
        // Optimistic update
        m_panadapterB->setRefLevel(newRef);
    });

    // Right-click on panadapter B
    connect(m_panadapterB, &PanadapterRhiWidget::frequencyRightClicked, this, [this](qint64 freq) {
        if (m_mouseQsyMode == 0) // Left Only — right-click disabled
            return;
        if (!m_tcpClient->isConnected() || freq <= 0)
            return;
        // L=A R=B mode: right-click always tunes VFO B
        QString cmd = QString("FB%1;").arg(freq, 11, 10, QChar('0'));
        m_tcpClient->sendCAT(cmd);
        m_tcpClient->sendCAT("FB;");
    });

    connect(m_panadapterB, &PanadapterRhiWidget::frequencyRightDragged, this, [this](qint64 freq) {
        if (m_mouseQsyMode == 0) // Left Only — right-drag disabled
            return;
        if (!m_tcpClient->isConnected() || freq <= 0)
            return;
        // L=A R=B mode: right-drag always tunes VFO B
        QString cmd = QString("FB%1;").arg(freq, 11, 10, QChar('0'));
        m_tcpClient->sendCAT(cmd);
        m_radioState->parseCATCommand(cmd);
    });
}

void MainWindow::updateDateTime() {
    QDateTime now = QDateTime::currentDateTimeUtc();
    m_dateTimeLabel->setText(now.toString("M-dd / HH:mm:ss") + " Z");
    m_sideControlPanel->setTime(now.toString("HH:mm:ss") + " Z");
}

void MainWindow::showControlFeedback(const QString &message) {
    if (message.isEmpty())
        return;

    // CTRL and DISP are top-level surfaces. A notification owned by MainWindow
    // is necessarily behind either one, even when raise() is used. Use the
    // same NotificationWidget overlay owned by the visible surface instead.
    NotificationWidget *overlay = m_notificationWidget;
    if (m_phoneControlsDialog && m_phoneControlsDialog->isVisible() && m_controlNotificationWidget)
        overlay = m_controlNotificationWidget;
    else if (m_displayPopup && m_displayPopup->isVisible() && m_displayNotificationWidget)
        overlay = m_displayNotificationWidget;
    if (overlay)
        overlay->showMessage(message, 1600);
}

void MainWindow::queueControlFeedback(const QString &key, const QString &fallback) {
    m_pendingControlFeedback = key;
    m_pendingControlFallback = fallback;
    const int generation = ++m_controlFeedbackGeneration;
    // Prefer the K4's echoed state. If a front-panel switch command has no
    // distinct CAT echo, still acknowledge the touch so an accidental tap is
    // visible to the operator.
    QTimer::singleShot(900, this, [this, key, generation]() {
        if (generation != m_controlFeedbackGeneration || m_pendingControlFeedback != key)
            return;
        const QString message = m_pendingControlFallback;
        m_pendingControlFeedback.clear();
        m_pendingControlFallback.clear();
        showControlFeedback(message);
    });
}

void MainWindow::completeControlFeedback(const QString &key, const QString &message) {
    if (m_pendingControlFeedback != key)
        return;
    ++m_controlFeedbackGeneration;
    m_pendingControlFeedback.clear();
    m_pendingControlFallback.clear();
    showControlFeedback(message);
}

QString MainWindow::formatFrequency(quint64 freq) {
    QString freqStr = QString::number(freq);
    while (freqStr.length() < 8) {
        freqStr.prepend('0');
    }

    // Insert dots: XX.XXX.XXX
    QString formatted;
    int len = freqStr.length();
    for (int i = 0; i < len; i++) {
        formatted.append(freqStr[i]);
        int posFromEnd = len - i - 1;
        if (posFromEnd > 0 && posFromEnd % 3 == 0) {
            formatted.append('.');
        }
    }

    // Remove leading zero for frequencies < 10 MHz (40m-160m)
    if (formatted.startsWith('0')) {
        formatted = formatted.mid(1);
    }
    return formatted;
}

int MainWindow::getBandFromFrequency(quint64 freq) {
    // Convert frequency (Hz) to K4 band number
    // Returns -1 for out-of-band frequencies
    if (freq >= 1800000 && freq <= 2000000)
        return 0; // 160m
    if (freq >= 3500000 && freq <= 4000000)
        return 1; // 80m
    if (freq >= 5330500 && freq <= 5405500)
        return 2; // 60m
    if (freq >= 7000000 && freq <= 7300000)
        return 3; // 40m
    if (freq >= 10100000 && freq <= 10150000)
        return 4; // 30m
    if (freq >= 14000000 && freq <= 14350000)
        return 5; // 20m
    if (freq >= 18068000 && freq <= 18168000)
        return 6; // 17m
    if (freq >= 21000000 && freq <= 21450000)
        return 7; // 15m
    if (freq >= 24890000 && freq <= 24990000)
        return 8; // 12m
    if (freq >= 28000000 && freq <= 29700000)
        return 9; // 10m
    if (freq >= 50000000 && freq <= 54000000)
        return 10; // 6m
    if (freq >= 144000000)
        return 16; // XVTR (transverter bands 16-25)
    return -1;     // Out of band / GEN coverage
}

bool MainWindow::areVfosOnDifferentBands() {
    int bandA = getBandFromFrequency(m_radioState->vfoA());
    int bandB = getBandFromFrequency(m_radioState->vfoB());
    // Consider them on different bands if either is out-of-band (-1) or they differ
    return (bandA != bandB);
}

void MainWindow::checkAndHideMiniPanB() {
    // Auto-hide mini pan B if SUB RX is off and VFOs are on different bands
    if (!m_radioState->subReceiverEnabled() && areVfosOnDifferentBands()) {
        if (m_radioState->miniPanBEnabled()) {
            m_radioState->setMiniPanBEnabled(false);
            m_tcpClient->sendCAT("#MP$0;"); // Disable Mini-Pan B streaming
        }
        if (m_vfoB->isMiniPanVisible()) {
            m_vfoB->showNormal();
        }
    }
}

void MainWindow::showRadioManager() {
    if (!m_radioManager) {
        // Keep the manager inside the existing Android window. Creating a
        // top-level QDialog here adds a second EGL/RHI surface, which can
        // deadlock with Android accessibility while the panadapter is active.
        m_radioManager = new RadioManagerDialog(centralWidget());
        m_radioManager->hide();
        connect(m_radioManager, &RadioManagerDialog::connectRequested, this, &MainWindow::connectToRadio);
        connect(m_radioManager, &RadioManagerDialog::disconnectRequested, this, [this]() {
            // TcpClient::disconnectFromHost() sends RRN; automatically
            QMetaObject::invokeMethod(m_tcpClient, "disconnectFromHost", Qt::QueuedConnection);
        });
        connect(m_radioManager, &RadioManagerDialog::closeRequested, m_radioManager, &QWidget::hide);
    }

    // Set the connected host so the manager can show "Disconnect" for the
    // active connection, and clear stale state after a disconnect.
    m_radioManager->setConnectedHost(m_tcpClient->isConnected() ? m_currentRadio.host : QString());
    m_radioManager->setGeometry(centralWidget()->rect());
    m_radioManager->show();
    m_radioManager->raise();
    m_radioManager->setFocus(Qt::OtherFocusReason);
}

void MainWindow::connectToRadio(const RadioEntry &radio) {
    if (m_connectionState == TcpClient::Connecting || m_connectionState == TcpClient::Authenticating) {
        if (m_notificationWidget) {
            const QString target = m_currentRadio.name.isEmpty() ? m_currentRadio.host : m_currentRadio.name;
            m_notificationWidget->showMessage(QString("Still connecting to %1...").arg(target), 2200);
        }
        return;
    }

    if (m_tcpClient->isConnected()) {
        QMetaObject::invokeMethod(m_tcpClient, "disconnectFromHost", Qt::QueuedConnection);
    }

    m_currentRadio = radio;
    m_connectionState = TcpClient::Connecting;
    m_titleLabel->setText("Elecraft K4 - " + radio.name);

    qDebug() << "Connecting to" << radio.host << ":" << radio.port << (radio.useTls ? "(TLS/PSK)" : "(unencrypted)")
             << "encodeMode:" << radio.encodeMode << "streamingLatency:" << radio.streamingLatency;
    if (m_notificationWidget) {
        m_notificationWidget->showMessage(QString("Connecting to %1...").arg(radio.host), 6000);
    }
    QMetaObject::invokeMethod(m_tcpClient, "connectToHost", Qt::QueuedConnection, Q_ARG(QString, radio.host),
                              Q_ARG(quint16, radio.port), Q_ARG(QString, radio.password), Q_ARG(bool, radio.useTls),
                              Q_ARG(QString, radio.identity), Q_ARG(int, radio.encodeMode),
                              Q_ARG(int, radio.streamingLatency));
}

void MainWindow::onConnectClicked() {
    showRadioManager();
}

void MainWindow::onDisconnectClicked() {
    if (m_pttActive) {
        onPttReleased();
    }
    QMetaObject::invokeMethod(m_tcpClient, "disconnectFromHost", Qt::QueuedConnection);
}

void MainWindow::onStateChanged(TcpClient::ConnectionState state) {
    m_connectionState = state;
    updateConnectionState(state);
}

void MainWindow::onError(const QString &error) {
    m_connectionStatusLabel->setText("Error: " + error);
    m_connectionStatusLabel->setStyleSheet(
        QString("color: %1; font-size: 12px; font-weight: bold;").arg(K4Styles::Colors::TxRed));
    if (m_notificationWidget) {
        m_notificationWidget->showMessage(QString("Connection Error: %1").arg(error), 5000);
    }
}

void MainWindow::onAuthenticated() {
    qDebug() << "Successfully authenticated with K4 radio";
    if (m_notificationWidget) {
        const QString target = m_currentRadio.name.isEmpty() ? m_currentRadio.host : m_currentRadio.name;
        m_notificationWidget->showMessage(QString("Connected to %1").arg(target), 3500);
    }

    if (K4Styles::isCompactLayout()) {
        // A short landscape phone needs a readable live trace on first open.
        // This is a local initial view only; WTR HEIGHT immediately regains
        // normal control as soon as the operator adjusts it.
        m_phoneWaterfallHeight = 50;
        m_phoneWaterfallHeightAdjusted = false;
        m_panadapterA->setWaterfallHeight(m_phoneWaterfallHeight);
        m_panadapterB->setWaterfallHeight(m_phoneWaterfallHeight);
        m_displayPopup->setWaterfallHeight(m_phoneWaterfallHeight);
    }

    // Reassert SL after TcpClient's RDY request, then update local state because
    // the K4 silently applies SL and does not echo it. This is current mainline's
    // synchronization contract and prevents K4/local TX frame-size drift.
    const QString slCommand = QString("SL%1;").arg(m_currentRadio.streamingLatency);
    m_tcpClient->sendCAT(slCommand);
    m_radioState->parseCATCommand(slCommand);

    // Start audio engine on its dedicated thread (BlockingQueued to get return value)
    bool audioStarted = false;
    QMetaObject::invokeMethod(m_audioEngine, "start", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, audioStarted));
    if (audioStarted) {
        qDebug() << "Audio engine started for RX audio";
        // Volume setters are atomic — safe as direct calls from any thread
        m_audioEngine->setMainVolume(m_sideControlPanel->volume() / 100.0f);
        m_audioEngine->setSubVolume(m_sideControlPanel->subVolume() / 100.0f);
        m_audioEngine->setMicGain(RadioSettings::instance()->micGain() / 100.0f);
        m_audioEngine->setEncodeMode(m_currentRadio.encodeMode);
        m_audioEngine->setFrameSamples(streamingLatencyToFrameSamples(m_currentRadio.streamingLatency));
    } else {
        qWarning() << "Failed to start audio engine";
    }

    // RDY supplies most state. Share supplemental GETs with macro refresh so
    // display, VFO rates, paddle orientation and FM controls stay in sync.
    m_tcpClient->sendCAT(K4Protocol::Commands::ADDITIONAL_STATE_QUERIES);
    m_tcpClient->sendCAT("SIRC1;"); // Enable 1-second client stats updates
    // Note: ML commands (monitor levels) come in RDY; dump - no need to query

    // Create synthetic "Display FPS" menu item with stored preference
    m_menuModel->addSyntheticDisplayFpsItem(m_currentRadio.displayFps);

    // Connect KPA1500 if enabled and configured
    if (RadioSettings::instance()->kpa1500Enabled() && !RadioSettings::instance()->kpa1500Host().isEmpty()) {
        m_kpa1500Client->connectToHost(RadioSettings::instance()->kpa1500Host(),
                                       RadioSettings::instance()->kpa1500Port());
    }
}

void MainWindow::onAuthenticationFailed() {
    qDebug() << "Authentication failed";
    m_connectionStatusLabel->setText("Auth Failed");
    m_connectionStatusLabel->setStyleSheet(
        QString("color: %1; font-size: 12px; font-weight: bold;").arg(K4Styles::Colors::TxRed));
    if (m_notificationWidget) {
        m_notificationWidget->showMessage("Authentication Failed", 5000);
    }
}

void MainWindow::onCatResponse(const QString &response) {

    // Parse CAT commands (may contain multiple commands separated by ;)
    QStringList commands = response.split(';', Qt::SkipEmptyParts);
    for (const QString &cmd : commands) {
        m_radioState->parseCATCommand(cmd + ";");

        // Parse MEDF (menu definitions) from RDY response
        if (cmd.startsWith("MEDF")) {
            m_menuModel->parseMEDF(cmd + ";");
        }
        // Route ME (menu value) commands to MenuModel for real-time updates
        else if (cmd.startsWith("ME")) {
            m_menuModel->parseME(cmd + ";");
        }
        // Parse BN$ (Band Number) response for VFO B (Sub RX)
        else if (cmd.startsWith("BN$")) {
            // VFO B band number: BN$nn where nn is 00-10 or 16-25
            bool ok;
            int bandNum = cmd.mid(3, 2).toInt(&ok);
            if (ok) {
                updateBandSelectionB(bandNum);
            }
        }
        // Parse BN (Band Number) response for VFO A
        else if (cmd.startsWith("BN")) {
            // VFO A band number: BNnn where nn is 00-10 or 16-25
            bool ok;
            int bandNum = cmd.mid(2, 2).toInt(&ok);
            if (ok) {
                updateBandSelection(bandNum);
            }
        }
    }
}

void MainWindow::onFrequencyChanged(quint64 freq) {
    m_vfoA->setFrequency(formatFrequency(freq));
}

void MainWindow::onFrequencyBChanged(quint64 freq) {
    m_vfoB->setFrequency(formatFrequency(freq));
}

void MainWindow::onModeChanged(RadioState::Mode mode) {
    Q_UNUSED(mode)
    // Use full mode string which includes data sub-mode (AFSK, FSK, PSK, DATA)
    // Also adds "+" suffix for USB/LSB when ESSB is enabled
    updateModeLabels();
    refreshTxPopupForMode();
    refreshRxPopupsForModes();
}

void MainWindow::refreshTxPopupForMode() {
    if (!m_txPopup)
        return;

    const RadioState::Mode mode = txOperatingMode();
    const int dataSubMode = m_radioState->splitEnabled()
                                ? m_radioState->dataSubModeB()
                                : m_radioState->dataSubMode();
    const bool usesKeyer = mode == RadioState::CW || mode == RadioState::CW_R
                           || ((mode == RadioState::DATA || mode == RadioState::DATA_R)
                               && (dataSubMode == 2 || dataSubMode == 3));
    if (usesKeyer) {
        const QString paddle = m_radioState->paddleOrientation() == 'R' ? "PDL REV" : "PDL NOR";
        m_txPopup->setButtonLabel(5, paddle, QString("IAMB %1").arg(m_radioState->iambicMode()), true);
        m_txPopup->setButtonLabel(6, "WEIGHT",
                                  QString::number(m_radioState->keyingWeight() / 100.0, 'f', 2), false);
    } else if (mode == RadioState::LSB || mode == RadioState::USB) {
        const int bw = m_radioState->ssbTxBw();
        const QString bwText = (bw >= 24 && bw <= 45)
                                   ? QString("%1k").arg(bw / 10.0, 0, 'f', 1)
                                   : QStringLiteral("2.8k");
        m_txPopup->setButtonLabel(5, m_radioState->essbEnabled() ? "ESSB BW" : "SSB BW", bwText, false);
        m_txPopup->setButtonLabel(6, "ESSB", m_radioState->essbEnabled() ? "ON" : "OFF", false);
    } else if ((mode == RadioState::DATA || mode == RadioState::DATA_R)
               && (dataSubMode == 0 || dataSubMode == 1)) {
        // Although DW appears in the reference, the physical K4 reports this
        // front-panel function as implementation in progress.
        m_txPopup->setButtonLabel(5, "DATA BW", "IIP", false);
        m_txPopup->setButtonLabel(6, "", "", false);
    } else if (mode == RadioState::FM) {
        const QString rpt = m_radioState->repeaterMode() == 'S' ? "S" : QString(m_radioState->repeaterMode());
        m_txPopup->setButtonLabel(5, QString("RPT %1").arg(rpt),
                                  QString("%1 kHz").arg(m_radioState->repeaterOffsetKhz()), true);
        m_txPopup->setButtonLabel(6, "PL TONE", "DTMF", true);
    } else {
        // SSB bandwidth and ESSB are not valid TX controls for AM, FM, or
        // data modes. Keep the slots visibly inactive instead of dispatching
        // an unrelated SSB editor.
        const QString modeText = m_radioState->splitEnabled()
                                     ? m_radioState->modeStringFullB()
                                     : m_radioState->modeStringFull();
        if (mode == RadioState::AM) {
            m_txPopup->setButtonLabel(5, "AM BW", "REMOTE N/A", false);
            m_txPopup->setButtonLabel(6, "", "", false);
        } else {
            m_txPopup->setButtonLabel(5, "MODE TX", modeText, false);
            m_txPopup->setButtonLabel(6, "", "", false);
        }
    }
}

void MainWindow::refreshRxPopupsForModes() {
    if (!m_mainRxPopup || !m_subRxPopup)
        return;

    const auto configure = [this](ButtonRowPopup *popup, RadioState::Mode mode,
                                  int dataSubMode, bool sub) {
        const bool cw = mode == RadioState::CW || mode == RadioState::CW_R;
        const bool dataDecode = (mode == RadioState::DATA || mode == RadioState::DATA_R)
                                && dataSubMode >= 1 && dataSubMode <= 3;
        if (cw) {
            const bool enabled = sub ? m_radioState->apfEnabledB() : m_radioState->apfEnabled();
            const int width = sub ? m_radioState->apfBandwidthB() : m_radioState->apfBandwidth();
            static const int widths[] = {30, 50, 150};
            popup->setButtonLabel(5, "APF BW",
                                  enabled ? QString("%1 Hz").arg(widths[qBound(0, width, 2)]) : "OFF",
                                  true);
            popup->setButtonLabel(6, "TEXT", "DECODE", false);
        } else {
            popup->setButtonLabel(5, "N/A", "", false);
            popup->setButtonLabel(6, dataDecode ? "TEXT" : "N/A",
                                  dataDecode ? "DECODE" : "", false);
        }
    };
    configure(m_mainRxPopup, m_radioState->mode(), m_radioState->dataSubMode(), false);
    configure(m_subRxPopup, m_radioState->modeB(), m_radioState->dataSubModeB(), true);
}

RadioState::Mode MainWindow::txOperatingMode() const {
    // With split enabled the K4 transmits on VFO B; otherwise it transmits on
    // VFO A. TX controls must describe the actual transmit mode, not always A.
    return m_radioState->splitEnabled() ? m_radioState->modeB() : m_radioState->mode();
}

void MainWindow::onModeBChanged(RadioState::Mode mode) {
    Q_UNUSED(mode)
    // Use full mode string which includes data sub-mode (AFSK, FSK, PSK, DATA)
    updateModeLabels();
    refreshTxPopupForMode();
    refreshRxPopupsForModes();
}

void MainWindow::updateModeLabels() {
    const auto fmModeLabel = [this](bool toneEnabled) {
        QString label = "FM";
        const QChar repeaterMode = m_radioState->repeaterMode();
        if (repeaterMode == '+' || repeaterMode == '-')
            label += repeaterMode;
        if (toneEnabled)
            label += "/T";
        return label;
    };

    // VFO A mode label
    QString modeA = m_radioState->modeStringFull();
    RadioState::Mode mode = m_radioState->mode();
    if (m_radioState->essbEnabled() && (mode == RadioState::USB || mode == RadioState::LSB)) {
        modeA += "+";
    } else if (mode == RadioState::FM) {
        modeA = fmModeLabel(m_radioState->plToneEnabled());
    }
    m_modeALabel->setText(modeA);

    // VFO B mode label
    QString modeB = m_radioState->modeStringFullB();
    RadioState::Mode modeVfoB = m_radioState->modeB();
    if (m_radioState->essbEnabled() && (modeVfoB == RadioState::USB || modeVfoB == RadioState::LSB)) {
        modeB += "+";
    } else if (modeVfoB == RadioState::FM) {
        modeB = fmModeLabel(m_radioState->plToneEnabledB());
    }
    m_modeBLabel->setText(modeB);
}

void MainWindow::onSMeterChanged(double value) {
    m_vfoA->setSMeterValue(value);
}

void MainWindow::onSMeterBChanged(double value) {
    m_vfoB->setSMeterValue(value);
}

void MainWindow::onBandwidthChanged(int bw) {
    Q_UNUSED(bw)
    // Could update a bandwidth display if needed
}

void MainWindow::onBandwidthBChanged(int bw) {
    Q_UNUSED(bw)
    // Could update a bandwidth display if needed
}

void MainWindow::updateConnectionState(TcpClient::ConnectionState state) {
    switch (state) {
    case TcpClient::Disconnected:
        // Clear the local TX gate on every disconnect, including unexpected
        // radio/network closure. Never leave the next connection latched TX.
        m_pttActive = false;
        m_phoneWaterfallHeight = 50;
        m_phoneWaterfallHeightAdjusted = false;
        m_bottomMenuBar->setPttActive(false);
        QMetaObject::invokeMethod(m_audioEngine, "setPttActive", Qt::QueuedConnection, Q_ARG(bool, false));
        m_connectionStatusLabel->setText("K4");
        m_connectionStatusLabel->setStyleSheet(
            QString("color: %1; font-size: 12px;").arg(K4Styles::Colors::InactiveGray));
        m_titleLabel->setText("Elecraft K4");
        // Stop audio engine to prevent accessing invalid data
        if (m_audioEngine) {
            QMetaObject::invokeMethod(m_audioEngine, "stop", Qt::QueuedConnection);
        }

        // Clear all UI state to avoid showing stale data
        // Clear spectrum displays
        m_panadapterA->clear();
        m_panadapterB->clear();

        // Clear mini-pan displays
        if (m_vfoA->miniPan())
            m_vfoA->miniPan()->clear();
        if (m_vfoB->miniPan())
            m_vfoB->miniPan()->clear();

        // Reset VFO displays and embedded meters
        m_vfoA->setFrequency(0);
        m_vfoA->setSMeterValue(0);
        m_vfoA->setTransmitting(false);
        m_vfoA->setTxMeters(0, 0, 0, 1.0);
        m_vfoB->setFrequency(0);
        m_vfoB->setSMeterValue(0);
        m_vfoB->setTransmitting(false);
        m_vfoB->setTxMeters(0, 0, 0, 1.0);

        // Reset model state so all change-guards fire on reconnect
        m_radioState->reset();

        // --- Reset all remaining UI to clean default state ---

        // Mode labels
        m_modeALabel->setText("");
        m_modeBLabel->setText("");

        // Antenna labels
        m_txAntennaLabel->setText("");
        m_rxAntALabel->setText("");
        m_rxAntBLabel->setText("");

        // Split
        m_splitLabel->setText("SPLIT OFF");
        m_splitLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(K4Styles::Colors::AccentAmber));

        // TX indicators (default: left triangle, amber)
        m_txTriangle->setText("◀");
        m_txTriangleB->setText("");

        // B SET
        m_bSetLabel->setVisible(false);

        // SUB/DIV (disabled state)
        m_subLabel->setStyleSheet(
            QString("background-color: %1; color: %2; font-size: 9px; font-weight: bold; border-radius: 2px;")
                .arg(K4Styles::Colors::DisabledBackground, K4Styles::Colors::LightGradientTop));
        m_divLabel->setStyleSheet(
            QString("background-color: %1; color: %2; font-size: 9px; font-weight: bold; border-radius: 2px;")
                .arg(K4Styles::Colors::DisabledBackground, K4Styles::Colors::LightGradientTop));

        // Dim VFO B (SUB off state)
        m_vfoB->frequencyDisplay()->setNormalColor(QColor(K4Styles::Colors::InactiveGray));
        m_modeBLabel->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::InactiveGray));

        // Message bank
        m_msgBankLabel->setText("MSG: I");
        m_msgBankLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(K4Styles::Colors::TextGray));

        // RIT/XIT (disabled state)
        m_ritLabel->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::InactiveGray));
        m_xitLabel->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::InactiveGray));
        m_ritXitValueLabel->setText("+0.00");
        m_ritXitValueLabel->setStyleSheet(
            QString("color: %1; font-size: 14px; font-weight: bold;").arg(K4Styles::Colors::InactiveGray));

        // ATU (grey/inactive)
        m_atuLabel->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::TextGray));

        // VOX / QSK (grey/inactive)
        m_voxLabel->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::TextGray));
        m_qskLabel->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::TextGray));

        // TEST (hidden)
        m_testLabel->setVisible(false);

        // VFO indicators (AGC, PRE, ATT, NB, NR, Notch, APF, Tuning Rate)
        m_vfoA->setAGC("AGC");
        m_vfoA->setPreamp(false, 0);
        m_vfoA->setAtt(false, 0);
        m_vfoA->setNB(false);
        m_vfoA->setNR(false, false);
        m_vfoA->setNotch(false, false);
        m_vfoA->setApf(false, 0);
        m_vfoA->setTuningRate(0);

        m_vfoB->setAGC("AGC");
        m_vfoB->setPreamp(false, 0);
        m_vfoB->setAtt(false, 0);
        m_vfoB->setNB(false);
        m_vfoB->setNR(false, false);
        m_vfoB->setNotch(false, false);
        m_vfoB->setApf(false, 0);
        m_vfoB->setTuningRate(0);

        // VFO locks (both unlocked)
        m_vfoRow->setLockA(false);
        m_vfoRow->setLockB(false);

        // Side control panel values
        m_sideControlPanel->setBandwidth(0);
        m_sideControlPanel->setShift(0);
        m_sideControlPanel->setHighCut(0);
        m_sideControlPanel->setLowCut(0);
        m_sideControlPanel->setPower(0);
        m_sideControlPanel->setDelay(0);
        m_sideControlPanel->setWpm(0);
        m_sideControlPanel->setPitch(0);
        m_sideControlPanel->setMicGain(0);
        m_sideControlPanel->setCompression(0);
        m_sideControlPanel->setMainRfGain(0);
        m_sideControlPanel->setMainSquelch(0);
        m_sideControlPanel->setSubRfGain(0);
        m_sideControlPanel->setSubSquelch(0);

        // Status bar values
        m_powerLabel->setText("--- W");
        m_swrLabel->setText("-.-:1");
        m_voltageLabel->setText("--.- V");
        m_currentLabel->setText("-.- A");
        m_lpaTempLabel->setText("LPA --°C");
        m_paTempLabel->setText("PA --°C");
        m_lpaTempLabel->setStyleSheet(temperatureStyle(0));
        m_paTempLabel->setStyleSheet(temperatureStyle(0));
        m_sideControlPanel->setPowerReading(0);
        m_sideControlPanel->setSwr(1.0);
        m_sideControlPanel->setVoltage(0);
        m_sideControlPanel->setCurrent(0);

        // Filter indicator widgets
        m_filterAWidget->setBandwidth(0);
        m_filterAWidget->setShift(50);
        m_filterAWidget->setFilterPosition(1);
        m_filterAWidget->setMode("");
        m_filterBWidget->setBandwidth(0);
        m_filterBWidget->setShift(50);
        m_filterBWidget->setFilterPosition(1);
        m_filterBWidget->setMode("");

        // VFO mini-pan overlays (reset mode/filter state)
        m_vfoA->setMiniPanMode("USB");
        m_vfoA->setMiniPanFilterBandwidth(2400);
        m_vfoA->setMiniPanIfShift(50);
        m_vfoA->setMiniPanCwPitch(600);
        m_vfoA->setMiniPanNotchFilter(false, 0);
        m_vfoB->setMiniPanMode("USB");
        m_vfoB->setMiniPanFilterBandwidth(2400);
        m_vfoB->setMiniPanIfShift(50);
        m_vfoB->setMiniPanCwPitch(600);
        m_vfoB->setMiniPanNotchFilter(false, 0);

        // Clear menu model
        m_menuModel->clear();

        // Disconnect KPA1500 when K4 disconnects
        if (m_kpa1500Client->isConnected()) {
            m_kpa1500Client->disconnectFromHost();
        }

        break;

    case TcpClient::Connecting:
        m_connectionStatusLabel->setText("K4...");
        m_connectionStatusLabel->setStyleSheet(
            QString("color: %1; font-size: 12px; font-weight: bold;").arg(K4Styles::Colors::AccentAmber));
        break;

    case TcpClient::Authenticating:
        m_connectionStatusLabel->setText("AUTH");
        m_connectionStatusLabel->setStyleSheet(
            QString("color: %1; font-size: 12px; font-weight: bold;").arg(K4Styles::Colors::AccentAmber));
        break;

    case TcpClient::Connected:
        m_connectionStatusLabel->setText("K4 OK");
        m_connectionStatusLabel->setStyleSheet(
            QString("color: %1; font-size: 12px; font-weight: bold;").arg(K4Styles::Colors::StatusGreen));
        break;
    }
}

void MainWindow::onRfPowerChanged(double watts, bool isQrp) {
    Q_UNUSED(watts)
    Q_UNUSED(isQrp)
    // NOTE: This is the power SETTING (PC command), not actual TX power.
    // The power display is updated from txMeterChanged signal during TX.
    // We don't update the display here - it should show 0 when not transmitting.
}

void MainWindow::onSupplyVoltageChanged(double volts) {
    m_voltageLabel->setText(QString("%1 V").arg(volts, 0, 'f', 1));
    m_sideControlPanel->setVoltage(volts);
}

void MainWindow::onSupplyCurrentChanged(double amps) {
    m_currentLabel->setText(QString("%1 A").arg(amps, 0, 'f', 1));
    m_sideControlPanel->setCurrent(amps);
}

void MainWindow::onSwrChanged(double swr) {
    m_swrLabel->setText(QString("%1:1").arg(swr, 0, 'f', 1));
    m_sideControlPanel->setSwr(swr);
}

void MainWindow::onDisplayFpsChanged(int fps) {
    // Update synthetic menu item value
    m_menuModel->updateValue(MenuModel::SYNTHETIC_DISPLAY_FPS_ID, fps);

    // Compare to stored preference and send if different
    if (m_tcpClient->isConnected() && m_currentRadio.displayFps != fps) {
        qDebug() << "Display FPS mismatch: stored=" << m_currentRadio.displayFps << "radio=" << fps << "-> sending #FPS"
                 << m_currentRadio.displayFps;
        m_tcpClient->sendCAT(QString("#FPS%1;").arg(m_currentRadio.displayFps));
    }
}

void MainWindow::onSplitChanged(bool enabled) {
    if (enabled) {
        m_splitLabel->setText("SPLIT ON");
        m_splitLabel->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::StatusGreen));
        // When split is on, TX goes to VFO B - clear left triangle, show right triangle
        m_txTriangle->setText("");
        m_txTriangleB->setText("▶");
    } else {
        m_splitLabel->setText("SPLIT OFF");
        m_splitLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(K4Styles::Colors::AccentAmber));
        // When split is off, TX stays on VFO A - show left triangle, clear right triangle
        m_txTriangle->setText("◀");
        m_txTriangleB->setText("");
    }
    completeControlFeedback("SPLIT", enabled ? "SPLIT ON" : "SPLIT OFF");
    refreshTxPopupForMode();
}

void MainWindow::onAntennaChanged(int txAnt, int rxAntMain, int rxAntSub) {
    // Format Main RX antenna display based on AR command value
    // K4 AR command values (from official K4 protocol documentation):
    // 0 = Disconnected (all RX RF sources disconnected)
    // 1 = EXT. XVTR IN / RX ANT IN2 (external transverter jack)
    // 2 = RX USES TX ANT (follows TX antenna selection) - show resolved value
    // 3 = INT. XVTR IN (internal transverter)
    // 4 = RX ANT IN1 (receive antenna jack)
    // 5 = ATU RX ANT1 (TX antenna 1 via ATU)
    // 6 = ATU RX ANT2 (TX antenna 2 via ATU)
    // 7 = ATU RX ANT3 (TX antenna 3 via ATU)
    auto formatMainRxAntenna = [this, txAnt](int arValue) -> QString {
        switch (arValue) {
        case 0: // Disconnected
            return "OFF";
        case 1: // EXT. XVTR IN / RX ANT IN2
            return QString("RX2:%1").arg(m_radioState->antennaName(5));
        case 2: // RX USES TX ANT - show resolved value like K4 front panel
            return QString("%1:%2").arg(txAnt).arg(m_radioState->antennaName(txAnt));
        case 3: // INT. XVTR IN
            return "INT XVTR";
        case 4: // RX ANT IN1
            return QString("RX1:%1").arg(m_radioState->antennaName(4));
        case 5: // ATU RX ANT1
            return QString("1:%1").arg(m_radioState->antennaName(1));
        case 6: // ATU RX ANT2
            return QString("2:%1").arg(m_radioState->antennaName(2));
        case 7: // ATU RX ANT3
            return QString("3:%1").arg(m_radioState->antennaName(3));
        default:
            return QString("AR%1").arg(arValue);
        }
    };

    // Format Sub RX antenna display based on AR$ command value
    // K4 AR$ command values (from official K4 protocol documentation):
    // 0 = Disconnected (all RX RF sources disconnected)
    // 1 = EXT. XVTR IN / RX ANT IN2 (external transverter jack)
    // 2 = RX USES TX ANT (follows TX antenna selection) - show resolved value
    // 3 = INT. XVTR IN (internal transverter)
    // 4 = RX ANT IN1 (receive antenna jack)
    // 5 = ATU RX ANT1 (TX antenna 1 via ATU)
    // 6 = ATU RX ANT2 (TX antenna 2 via ATU)
    // 7 = ATU RX ANT3 (TX antenna 3 via ATU)
    auto formatSubRxAntenna = [this, txAnt](int arValue) -> QString {
        switch (arValue) {
        case 0: // Disconnected
            return "OFF";
        case 1: // EXT. XVTR IN / RX ANT IN2
            return QString("RX2:%1").arg(m_radioState->antennaName(5));
        case 2: // RX USES TX ANT - show resolved value like K4 front panel
            return QString("%1:%2").arg(txAnt).arg(m_radioState->antennaName(txAnt));
        case 3: // INT. XVTR IN
            return "INT XVTR";
        case 4: // RX ANT IN1
            return QString("RX1:%1").arg(m_radioState->antennaName(4));
        case 5: // ATU RX ANT1
            return QString("1:%1").arg(m_radioState->antennaName(1));
        case 6: // ATU RX ANT2
            return QString("2:%1").arg(m_radioState->antennaName(2));
        case 7: // ATU RX ANT3
            return QString("3:%1").arg(m_radioState->antennaName(3));
        default:
            return QString("AR$%1").arg(arValue);
        }
    };

    // TX antenna (AN command) - always 1-3, format as "N:name"
    m_txAntennaLabel->setText(QString("%1:%2").arg(txAnt).arg(m_radioState->antennaName(txAnt)));

    // RX antennas - Main (AR) and Sub (AR$) have different value mappings
    m_rxAntALabel->setText(formatMainRxAntenna(rxAntMain));
    m_rxAntBLabel->setText(formatSubRxAntenna(rxAntSub));
    completeControlFeedback("TX_ANT", QString("TX ANT: %1").arg(m_txAntennaLabel->text()));
    completeControlFeedback("RX_ANT", QString("MAIN RX ANT: %1").arg(m_rxAntALabel->text()));
    completeControlFeedback("SUB_ANT", QString("SUB RX ANT: %1").arg(m_rxAntBLabel->text()));
}

void MainWindow::onAntennaNameChanged(int index, const QString &name) {
    // Refresh antenna displays when a name changes
    onAntennaChanged(m_radioState->txAntenna(), m_radioState->rxAntennaMain(), m_radioState->rxAntennaSub());

    // Update antenna config popups with custom names (ANT1-3 only)
    // Note: index is 1-based from ACN command (ACN1, ACN2, ACN3)
    // Popup labels are 0-based (0=ANT1, 1=ANT2, 2=ANT3)
    if (index >= 1 && index <= 3) {
        int popupIndex = index - 1; // Convert to 0-based
        if (m_mainRxAntCfgPopup)
            m_mainRxAntCfgPopup->setAntennaName(popupIndex, name);
        if (m_subRxAntCfgPopup)
            m_subRxAntCfgPopup->setAntennaName(popupIndex, name);
        if (m_txAntCfgPopup)
            m_txAntCfgPopup->setAntennaName(popupIndex, name);
    }
}

void MainWindow::onVoxChanged(bool enabled) {
    Q_UNUSED(enabled)
    // Use mode-specific VOX state (CW modes use VXC, Voice modes use VXV, Data modes use VXD)
    bool voxOn = m_radioState->voxForCurrentMode();
    if (voxOn) {
        m_voxLabel->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::AccentAmber));
    } else {
        m_voxLabel->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::TextGray));
    }
    completeControlFeedback("VOX", voxOn ? "VOX ON" : "VOX OFF");
}

void MainWindow::onQskEnabledChanged(bool enabled) {
    // QSK indicator: white when enabled, grey when disabled
    if (enabled) {
        m_qskLabel->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::TextWhite));
    } else {
        m_qskLabel->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::TextGray));
    }
    completeControlFeedback("QSK", enabled ? "QSK ON" : "QSK OFF");
}

void MainWindow::onTestModeChanged(bool enabled) {
    // TEST indicator: visible in red when test mode is on
    m_testLabel->setVisible(enabled);
    completeControlFeedback("TEST", enabled ? "TEST MODE ON" : "TEST MODE OFF");
}

void MainWindow::onAtuModeChanged(int mode) {
    // ATU indicator: orange when AUTO mode (2), grey otherwise
    if (mode == 2) {
        m_atuLabel->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::AccentAmber));
    } else {
        m_atuLabel->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: bold;").arg(K4Styles::Colors::TextGray));
    }
    completeControlFeedback("ATU", mode == 2 ? "ATU AUTO" : mode == 1 ? "ATU BYPASS" : "ATU OFF");
}

void MainWindow::onRitXitChanged(bool ritEnabled, bool xitEnabled, int offset) {
    // Match current mainline QK4 register routing. B SET displays RO$/RT$;
    // split XIT uses the TX VFO's RO$ register when RIT itself is not active.
    if (m_radioState->bSetEnabled()) {
        ritEnabled = m_radioState->ritEnabledB();
        offset = xitEnabled && !m_radioState->splitEnabled()
            ? m_radioState->ritXitOffset()
            : m_radioState->ritXitOffsetB();
    } else if (m_radioState->splitEnabled() && xitEnabled && !ritEnabled) {
        offset = m_radioState->ritXitOffsetB();
    }

    // Update RIT label
    if (ritEnabled) {
        m_ritLabel->setStyleSheet(
            QString("color: %1; font-size: 10px; font-weight: bold; border: none;").arg(K4Styles::Colors::TextWhite));
    } else {
        m_ritLabel->setStyleSheet(
            QString("color: %1; font-size: 10px; border: none;").arg(K4Styles::Colors::InactiveGray));
    }

    // Update XIT label
    if (xitEnabled) {
        m_xitLabel->setStyleSheet(
            QString("color: %1; font-size: 10px; font-weight: bold; border: none;").arg(K4Styles::Colors::TextWhite));
    } else {
        m_xitLabel->setStyleSheet(
            QString("color: %1; font-size: 10px; border: none;").arg(K4Styles::Colors::InactiveGray));
    }

    // Update offset value (in kHz)
    // Value is white if RIT or XIT is on, grey if both are off
    double offsetKHz = offset / 1000.0;
    QString sign = (offset >= 0) ? "+" : "";
    m_ritXitValueLabel->setText(QString("%1%2").arg(sign).arg(offsetKHz, 0, 'f', 2));

    QString valueColor = (ritEnabled || xitEnabled) ? K4Styles::Colors::TextWhite : K4Styles::Colors::InactiveGray;
    m_ritXitValueLabel->setStyleSheet(
        QString("color: %1; font-size: 14px; font-weight: bold; border: none; padding: 0 11px;").arg(valueColor));
    const QString active = ritEnabled && xitEnabled ? "RIT + XIT"
                           : ritEnabled ? "RIT"
                           : xitEnabled ? "XIT" : "RIT / XIT OFF";
    completeControlFeedback("RITXIT", QString("%1  %2%3 Hz").arg(active).arg(offset >= 0 ? "+" : "").arg(offset));
}

void MainWindow::onMessageBankChanged(int bank) {
    if (bank == 1) {
        m_msgBankLabel->setText("MSG: I");
    } else {
        m_msgBankLabel->setText("MSG: II");
    }
}

void MainWindow::onProcessingChanged() {
    // AGC
    QString agcText;
    switch (m_radioState->agcSpeed()) {
    case RadioState::AGC_Off:
        agcText = "AGC";
        break;
    case RadioState::AGC_Slow:
        agcText = "AGC-S";
        break;
    case RadioState::AGC_Fast:
        agcText = "AGC-F";
        break;
    }
    m_vfoA->setAGC(agcText);

    // Preamp (level 0-3)
    m_vfoA->setPreamp(m_radioState->preampEnabled() && m_radioState->preamp() > 0, m_radioState->preamp());

    // Attenuator (level 0-21 dB)
    m_vfoA->setAtt(m_radioState->attenuatorEnabled() && m_radioState->attenuatorLevel() > 0,
                   m_radioState->attenuatorLevel());

    // Noise Blanker
    m_vfoA->setNB(m_radioState->noiseBlankerEnabled());

    // Noise Reduction
    m_vfoA->setNR(m_radioState->noiseReductionEnabled(), m_radioState->ssnrEnabled());
    const bool feedbackTargetsMain = !m_radioState->bSetEnabled();
    if (feedbackTargetsMain && m_pendingControlFeedback == "PRE") {
        completeControlFeedback("PRE", m_radioState->preampEnabled()
                                           ? QString("PREAMP %1 ON").arg(m_radioState->preamp()) : "PREAMP OFF");
    } else if (feedbackTargetsMain && m_pendingControlFeedback == "NB") {
        completeControlFeedback("NB", m_radioState->noiseBlankerEnabled()
                                         ? QString("NOISE BLANKER ON  LEVEL %1").arg(m_radioState->noiseBlankerLevel())
                                         : "NOISE BLANKER OFF");
    } else if (feedbackTargetsMain && m_pendingControlFeedback == "NR") {
        const QString engine = m_radioState->ssnrEnabled() ? "SSNR" : "NOISE REDUCTION";
        completeControlFeedback("NR", (m_radioState->noiseReductionEnabled() || m_radioState->ssnrEnabled())
                                         ? QString("%1 ON").arg(engine) : QString("%1 OFF").arg(engine));
    } else if (feedbackTargetsMain && m_pendingControlFeedback == "NTCH") {
        completeControlFeedback("NTCH", m_radioState->autoNotchEnabled() ? "AUTO NOTCH ON" : "AUTO NOTCH OFF");
    }
}

void MainWindow::onProcessingChangedB() {
    // AGC
    QString agcText;
    switch (m_radioState->agcSpeedB()) {
    case RadioState::AGC_Off:
        agcText = "AGC";
        break;
    case RadioState::AGC_Slow:
        agcText = "AGC-S";
        break;
    case RadioState::AGC_Fast:
        agcText = "AGC-F";
        break;
    }
    m_vfoB->setAGC(agcText);

    // Preamp (level 0-3)
    m_vfoB->setPreamp(m_radioState->preampEnabledB() && m_radioState->preampB() > 0, m_radioState->preampB());

    // Attenuator (level 0-21 dB)
    m_vfoB->setAtt(m_radioState->attenuatorEnabledB() && m_radioState->attenuatorLevelB() > 0,
                   m_radioState->attenuatorLevelB());

    // Noise Blanker
    m_vfoB->setNB(m_radioState->noiseBlankerEnabledB());

    // Noise Reduction
    m_vfoB->setNR(m_radioState->noiseReductionEnabledB(), m_radioState->ssnrEnabledB());
    const bool feedbackTargetsSub = m_radioState->bSetEnabled();
    if (feedbackTargetsSub && m_pendingControlFeedback == "PRE") {
        completeControlFeedback("PRE", m_radioState->preampEnabledB()
                                           ? QString("SUB PREAMP %1 ON").arg(m_radioState->preampB()) : "SUB PREAMP OFF");
    } else if (feedbackTargetsSub && m_pendingControlFeedback == "NB") {
        completeControlFeedback("NB", m_radioState->noiseBlankerEnabledB()
                                         ? QString("SUB NOISE BLANKER ON  LEVEL %1").arg(m_radioState->noiseBlankerLevelB())
                                         : "SUB NOISE BLANKER OFF");
    } else if (feedbackTargetsSub && m_pendingControlFeedback == "NR") {
        const QString engine = m_radioState->ssnrEnabledB() ? "SUB SSNR" : "SUB NOISE REDUCTION";
        completeControlFeedback("NR", (m_radioState->noiseReductionEnabledB() || m_radioState->ssnrEnabledB())
                                         ? QString("%1 ON").arg(engine) : QString("%1 OFF").arg(engine));
    } else if (feedbackTargetsSub && m_pendingControlFeedback == "NTCH") {
        completeControlFeedback("NTCH", m_radioState->autoNotchEnabledB() ? "SUB AUTO NOTCH ON" : "SUB AUTO NOTCH OFF");
    }
}

void MainWindow::onSpectrumData(int receiver, const QByteArray &data, qint64 centerFreq, qint32 sampleRate,
                                float noiseFloor) {
    // AUTO is based on the actual decompressed bins used by this renderer.
    // The PAN header's noise-floor field is on a different calibration curve
    // and made the phone waterfall much too bright when used as REF directly.
    // A low percentile ignores signals while following the visible noise floor.
    const bool autoRef = m_displayPopup && m_displayPopup->autoRefLevelEnabled();
    float &smoothedRef = receiver == 0 ? m_autoRefLevelA : m_autoRefLevelB;
    bool &autoRefValid = receiver == 0 ? m_autoRefLevelAValid : m_autoRefLevelBValid;
    PanadapterRhiWidget *panadapter = receiver == 0 ? m_panadapterA : m_panadapterB;
    if (autoRef && !data.isEmpty()) {
        int histogram[256] = {};
        for (char bin : data)
            ++histogram[static_cast<quint8>(bin)];
        const int percentileCount = qMax(1, static_cast<int>(data.size() / 5)); // 20th percentile
        int accumulated = 0;
        int floorBin = 0;
        for (; floorBin < 255; ++floorBin) {
            accumulated += histogram[floorBin];
            if (accumulated >= percentileCount)
                break;
        }
        constexpr float K4BinOffsetDb = 146.0f;
        const float measuredFloor = static_cast<float>(floorBin) - K4BinOffsetDb;
        // Put ordinary noise just above black while retaining signal contrast.
        const float target = qBound(-200.0f, measuredFloor - 4.0f, 60.0f);
        smoothedRef = autoRefValid ? (0.12f * target + 0.88f * smoothedRef) : target;
        autoRefValid = true;
        const int displayedRef = qRound(smoothedRef);
        panadapter->setRefLevel(displayedRef);
        if (receiver == 0)
            m_displayPopup->setRefLevelValueA(displayedRef);
        else
            m_displayPopup->setRefLevelValueB(displayedRef);
    } else if (autoRefValid) {
        // Leaving AUTO restores the K4's manual REF value immediately.
        autoRefValid = false;
        const int manualRef = receiver == 0 ? m_radioState->refLevel() : m_radioState->refLevelB();
        panadapter->setRefLevel(manualRef);
        if (receiver == 0)
            m_displayPopup->setRefLevelValueA(manualRef);
        else
            m_displayPopup->setRefLevelValueB(manualRef);
    }

    // Route spectrum data to appropriate panadapter
    // receiver: 0 = Main (VFO A), 1 = Sub (VFO B)
    if (receiver == 0) {
        m_panadapterA->updateSpectrum(data, centerFreq, sampleRate, noiseFloor);
    } else if (receiver == 1) {
        m_panadapterB->updateSpectrum(data, centerFreq, sampleRate, noiseFloor);
    }
}

void MainWindow::onMiniSpectrumData(int receiver, const QByteArray &data) {
    // Route Mini-PAN data based on receiver byte (0=Main/A, 1=Sub/B)
    if (receiver == 0 && m_vfoA->isMiniPanVisible()) {
        m_vfoA->updateMiniPan(data);
    } else if (receiver == 1 && m_vfoB->isMiniPanVisible()) {
        m_vfoB->updateMiniPan(data);
    }
}

void MainWindow::showRitXitAdjustment(bool preferXit) {
    if (!m_tcpClient->isConnected())
        return;

    const bool bSet = m_radioState->bSetEnabled();
    const bool ritActive = bSet ? m_radioState->ritEnabledB() : m_radioState->ritEnabled();
    const bool xitActive = m_radioState->xitEnabled();
    if (!ritActive && !xitActive) {
        showControlFeedback("Enable RIT or XIT before adjusting");
        return;
    }

    // Refresh both registers before presenting a target. Current mainline QK4
    // keeps RO and RO$ distinct because split XIT uses the TX VFO register.
    m_tcpClient->sendCAT("RT;");
    m_tcpClient->sendCAT("XT;");
    m_tcpClient->sendCAT("RO;");
    m_tcpClient->sendCAT("RT$;");
    m_tcpClient->sendCAT("RO$;");

    InWindowDialog dialog(centralWidget());
    QWidget *panel = dialog.contentWidget();
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);

    auto *title = new QLabel("OFFSET JOG", panel);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QString("color: %1; font-size: 17px; font-weight: bold;")
                             .arg(K4Styles::Colors::AccentAmber));
    layout->addWidget(title);

    auto *targetRow = new QHBoxLayout();
    targetRow->setSpacing(8);
    auto *ritTarget = new QPushButton("RIT", panel);
    auto *xitTarget = new QPushButton("XIT", panel);
    for (QPushButton *button : {ritTarget, xitTarget}) {
        button->setCheckable(true);
        button->setMinimumHeight(36);
        targetRow->addWidget(button, 1);
    }
    ritTarget->setEnabled(ritActive);
    xitTarget->setEnabled(xitActive);
    layout->addLayout(targetRow);

    bool adjustXit = xitActive && (preferXit || !ritActive);
    auto *targetDescription = new QLabel(panel);
    targetDescription->setAlignment(Qt::AlignCenter);
    targetDescription->setStyleSheet(QString("color: %1; font-size: 11px;").arg(K4Styles::Colors::TextGray));
    layout->addWidget(targetDescription);

    auto *offsetValue = new QLabel(panel);
    offsetValue->setAlignment(Qt::AlignCenter);
    offsetValue->setMinimumHeight(42);
    offsetValue->setStyleSheet(QString("color: %1; background: %2; border: 1px solid %3;"
                                       "border-radius: 5px; font-size: 25px; font-weight: bold;")
                                   .arg(K4Styles::Colors::TextWhite, K4Styles::Colors::DarkBackground,
                                        K4Styles::Colors::InactiveGray));
    layout->addWidget(offsetValue);

    auto usesBRegister = [this, &adjustXit]() {
        return adjustXit ? m_radioState->splitEnabled() : m_radioState->bSetEnabled();
    };
    auto refreshTarget = [this, &adjustXit, ritTarget, xitTarget, targetDescription, offsetValue,
                          &usesBRegister]() {
        ritTarget->setChecked(!adjustXit);
        xitTarget->setChecked(adjustXit);
        ritTarget->setStyleSheet(!adjustXit ? K4Styles::menuBarButtonActive() : K4Styles::menuBarButton());
        xitTarget->setStyleSheet(adjustXit ? K4Styles::menuBarButtonActive() : K4Styles::menuBarButton());
        const bool registerB = usesBRegister();
        const int offset = registerB ? m_radioState->ritXitOffsetB() : m_radioState->ritXitOffset();
        targetDescription->setText(QString("Adjusting %1 - %2 register")
                                       .arg(adjustXit ? "XIT" : "RIT", registerB ? "VFO B" : "VFO A"));
        offsetValue->setText(QString("%1%2 kHz")
                                 .arg(offset >= 0 ? "+" : "")
                                 .arg(offset / 1000.0, 0, 'f', 2));
    };
    connect(ritTarget, &QPushButton::clicked, &dialog, [&adjustXit, &refreshTarget]() {
        adjustXit = false;
        refreshTarget();
    });
    connect(xitTarget, &QPushButton::clicked, &dialog, [&adjustXit, &refreshTarget]() {
        adjustXit = true;
        refreshTarget();
    });

    auto *hint = new QLabel("Tap or hold - / + for continuous 10 Hz steps", panel);
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet(QString("color: %1; font-size: 11px;").arg(K4Styles::Colors::TextGray));
    layout->addWidget(hint);

    auto *jogRow = new QHBoxLayout();
    jogRow->setSpacing(10);
    auto *down = new QPushButton("-", panel);
    auto *up = new QPushButton("+", panel);
    for (QPushButton *button : {down, up}) {
        button->setMinimumHeight(54);
        button->setAutoRepeat(true);
        button->setAutoRepeatDelay(350);
        button->setAutoRepeatInterval(85);
        button->setStyleSheet(K4Styles::menuBarButton());
        QFont font = button->font();
        font.setPixelSize(28);
        font.setBold(true);
        button->setFont(font);
        jogRow->addWidget(button, 1);
    }
    layout->addLayout(jogRow);

    auto sendJog = [this, &adjustXit](bool upward) {
        // This is the exact mainline wheel path except for the one genuinely
        // ambiguous case: split with both RIT and XIT active. Plain RU/RD is
        // routed by the K4 to XIT's RO$ register, so direct RO is used only
        // when the operator explicitly selects RIT's RO register.
        const bool separateSplitOffsets = !m_radioState->bSetEnabled() && m_radioState->splitEnabled()
            && m_radioState->ritEnabled() && m_radioState->xitEnabled();
        if (!adjustXit && separateSplitOffsets) {
            const int next = qBound(-9999, m_radioState->ritXitOffset() + (upward ? 10 : -10), 9999);
            const QString command = QString("RO%1%2;")
                                        .arg(next >= 0 ? "+" : "-")
                                        .arg(qAbs(next), 4, 10, QChar('0'));
            m_tcpClient->sendCAT(command);
            m_radioState->parseCATCommand(command);
            return;
        }

        const bool adjustB = !adjustXit && m_radioState->bSetEnabled() && !m_radioState->xitEnabled();
        m_tcpClient->sendCAT(upward ? (adjustB ? "RU$;" : "RU;")
                                    : (adjustB ? "RD$;" : "RD;"));
    };
    connect(down, &QPushButton::clicked, &dialog, [&sendJog]() { sendJog(false); });
    connect(up, &QPushButton::clicked, &dialog, [&sendJog]() { sendJog(true); });

    auto querySelectedOffset = [this, &usesBRegister]() {
        m_tcpClient->sendCAT(usesBRegister() ? "RO$;" : "RO;");
    };
    connect(down, &QPushButton::released, &dialog, querySelectedOffset);
    connect(up, &QPushButton::released, &dialog, querySelectedOffset);
    connect(m_radioState, &RadioState::ritXitChanged, &dialog,
            [&refreshTarget](bool, bool, int) { refreshTarget(); });
    connect(m_radioState, &RadioState::ritXitBChanged, &dialog,
            [&refreshTarget](bool, int) { refreshTarget(); });

    auto *bottomRow = new QHBoxLayout();
    auto *zero = new QPushButton("ZERO", panel);
    auto *close = new QPushButton("DONE", panel);
    for (QPushButton *button : {zero, close}) {
        button->setMinimumHeight(36);
        button->setStyleSheet(K4Styles::menuBarButton());
        bottomRow->addWidget(button, 1);
    }
    layout->addLayout(bottomRow);
    connect(zero, &QPushButton::clicked, &dialog, [this, &usesBRegister]() {
        m_tcpClient->sendCAT(usesBRegister() ? "RC$;" : "RC;");
        m_tcpClient->sendCAT(usesBRegister() ? "RO$;" : "RO;");
    });
    connect(close, &QPushButton::clicked, &dialog, &InWindowDialog::accept);

    refreshTarget();
    layout->activate();
    dialog.setPanelSize(QSize(qMin(480, centralWidget()->width() - 20),
                              qMin(layout->sizeHint().height(), centralWidget()->height() - 12)));
    dialog.exec();
}

void MainWindow::showPhoneControls() {
    if (!K4Styles::isCompactLayout() || !m_leftPanelScroll || !m_rightPanelScroll)
        return;

    const bool wasAlreadyVisible = m_phoneControlsDialog && m_phoneControlsDialog->isVisible();

    // Keep the persistent CTRL drawer in sync if PHONE MIC was adjusted from
    // the Settings audio-input page since it was last opened.
    m_sideControlPanel->setPhoneMicGain(RadioSettings::instance()->micGain());

    const QRect available = screen() ? screen()->availableGeometry() : QRect(0, 0, 800, 360);
    const int drawerWidth = qMax(360, available.width() - 8);
    // Leave the complete VFO/filter/RIT block exposed above the drawer.  The
    // control banks scroll vertically, so they do not need to cover the live
    // operating feedback to gain more height.
    const int drawerHeight = qMax(200, qRound(available.height() * 0.54));

    if (!m_phoneControlsDialog) {
        // Keep the proven QK4 control widgets and signal plumbing, but present
        // them as a bottom drawer.  The live VFO/filter/status area remains
        // visible above the drawer while the operator changes the radio.
        m_phoneControlsDialog = new QWidget(this);
        new OverlayBackHandler(m_phoneControlsDialog);
        m_phoneControlsDialog->setObjectName("phoneControlsOverlay");
        m_phoneControlsDialog->setAttribute(Qt::WA_StyledBackground, true);
        m_phoneControlsDialog->setStyleSheet(
            QString("#phoneControlsOverlay { background-color: %1; border: 1px solid %2; }")
                .arg(K4Styles::Colors::Background, K4Styles::Colors::BorderSelected));

        // A MainWindow child cannot stack above this top-level dialog.
        // Keep a dedicated transient feedback overlay inside the CTRL drawer.
        m_controlNotificationWidget = new NotificationWidget(m_phoneControlsDialog);

        auto *layout = new QVBoxLayout(m_phoneControlsDialog);
        layout->setContentsMargins(8, 6, 8, 6);
        layout->setSpacing(4);

        auto *header = new QHBoxLayout();
        auto *title = new QLabel("K4 CONTROLS", m_phoneControlsDialog);
        title->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: bold;")
                                 .arg(K4Styles::Colors::AccentAmber));
        auto *hint = new QLabel("Live control drawer - scroll either bank", m_phoneControlsDialog);
        hint->setStyleSheet(QString("color: %1; font-size: 11px;").arg(K4Styles::Colors::TextGray));
        auto *close = new QPushButton("RETURN TO OPERATE", m_phoneControlsDialog);
        close->setFixedHeight(28);
        close->setStyleSheet(K4Styles::menuBarButton());
        header->addWidget(title);
        header->addWidget(hint, 1);
        header->addWidget(close);
        layout->addLayout(header);

        // Message controls are part of the radio's operating surface.  Compact
        // mode used to hide them entirely; expose every primary action here and
        // keep the current message bank visible beside them.
        auto *messageRow = new QHBoxLayout();
        messageRow->setContentsMargins(0, 0, 0, 0);
        messageRow->setSpacing(4);
        auto *messageBank = new QLabel(m_radioState->messageBank() == 2 ? "MSG II" : "MSG I",
                                       m_phoneControlsDialog);
        messageBank->setMinimumWidth(48);
        messageBank->setAlignment(Qt::AlignCenter);
        messageBank->setStyleSheet(QString("color: %1; font-weight: bold;").arg(K4Styles::Colors::AccentAmber));
        messageRow->addWidget(messageBank);

        const QList<QPair<QString, QString>> messageCommands = {
            {"BANK", "SW137;"}, {"M1", "SW17;"},   {"M2", "SW51;"},    {"M3", "SW18;"},
            {"M4", "SW52;"},   {"REC", "SW19;"},  {"STORE", "SW20;"}, {"RCL", "SW34;"}};
        for (const auto &item : messageCommands) {
            auto *button = new QPushButton(item.first, m_phoneControlsDialog);
            button->setFixedHeight(28);
            button->setMinimumWidth(0);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            button->setStyleSheet(K4Styles::menuBarButton());
            connect(button, &QPushButton::clicked, this, [this, label = item.first, command = item.second]() {
                showControlFeedback(label == "BANK" ? "MESSAGE BANK changed"
                                                       : QString("MESSAGE %1 activated").arg(label));
                m_tcpClient->sendCAT(command);
            });
            messageRow->addWidget(button, 1);
        }
        connect(m_radioState, &RadioState::messageBankChanged, messageBank, [messageBank](int bank) {
            messageBank->setText(bank == 2 ? "MSG II" : "MSG I");
        });
        layout->addLayout(messageRow);

        // The desktop display exposes these by mouse-wheel only. Give the
        // phone Controls page explicit touch targets for both toggles and
        // offset adjustment.
        auto *offsetRow = new QWidget(m_phoneControlsDialog);
        auto *offsetLayout = new QHBoxLayout(offsetRow);
        offsetLayout->setContentsMargins(0, 0, 0, 0);
        offsetLayout->setSpacing(6);
        auto *rit = new QPushButton("RIT", offsetRow);
        auto *xit = new QPushButton("XIT", offsetRow);
        auto *offsetDown = new QPushButton("OFFSET -", offsetRow);
        auto *offsetValue = new QLabel("+0.00 kHz", offsetRow);
        auto *offsetUp = new QPushButton("OFFSET +", offsetRow);
        for (auto *button : {rit, xit, offsetDown, offsetUp}) {
            button->setFixedHeight(28);
            button->setStyleSheet(K4Styles::menuBarButton());
            offsetLayout->addWidget(button);
        }
        offsetValue->setMinimumWidth(90);
        offsetValue->setAlignment(Qt::AlignCenter);
        offsetValue->setStyleSheet(QString("color: %1; font-weight: bold;").arg(K4Styles::Colors::TextWhite));
        offsetLayout->insertWidget(3, offsetValue);
        layout->addWidget(offsetRow);

        connect(rit, &QPushButton::clicked, this, [this]() {
            queueControlFeedback("RITXIT", "RIT changed");
            m_tcpClient->sendCAT(m_radioState->bSetEnabled() ? "SW54;" : "RT/;");
        });
        connect(xit, &QPushButton::clicked, this, [this]() {
            queueControlFeedback("RITXIT", "XIT changed");
            m_tcpClient->sendCAT("XT/;");
        });
        connect(offsetDown, &QPushButton::clicked, this, [this]() {
            queueControlFeedback("RITXIT", "RIT / XIT offset decreased");
            const bool adjustB = m_radioState->bSetEnabled() && !m_radioState->xitEnabled();
            m_tcpClient->sendCAT(adjustB ? "RD$;" : "RD;");
        });
        connect(offsetUp, &QPushButton::clicked, this, [this]() {
            queueControlFeedback("RITXIT", "RIT / XIT offset increased");
            const bool adjustB = m_radioState->bSetEnabled() && !m_radioState->xitEnabled();
            m_tcpClient->sendCAT(adjustB ? "RU$;" : "RU;");
        });
        auto updateOffsetControls = [rit, xit, offsetValue](bool ritOn, bool xitOn, int offset) {
            rit->setText(ritOn ? "RIT ON" : "RIT");
            xit->setText(xitOn ? "XIT ON" : "XIT");
            offsetValue->setText(QString("%1%2 kHz").arg(offset >= 0 ? "+" : "").arg(offset / 1000.0, 0, 'f', 2));
        };
        connect(m_radioState, &RadioState::ritXitChanged, offsetRow, updateOffsetControls);
        updateOffsetControls(m_radioState->ritEnabled(), m_radioState->xitEnabled(), m_radioState->ritXitOffset());

        auto *columns = new QHBoxLayout();
        columns->setContentsMargins(0, 0, 0, 0);
        columns->setSpacing(10);
        const int bankWidth = qMax(K4Styles::Dimensions::SidePanelWidth + 18, (drawerWidth - 28) / 2);
        for (auto *scroll : {m_leftPanelScroll, m_rightPanelScroll}) {
            scroll->setParent(m_phoneControlsDialog);
            scroll->setFixedWidth(bankWidth);
            scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
            scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            scroll->setWidgetResizable(false);
            QPalette viewportPalette = scroll->viewport()->palette();
            viewportPalette.setColor(QPalette::Window, QColor(K4Styles::Colors::PopupBackground));
            scroll->viewport()->setPalette(viewportPalette);
            scroll->viewport()->setAutoFillBackground(true);
            scroll->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
            QScroller::grabGesture(scroll->viewport(), QScroller::TouchGesture);
            // Briefly delay child-button press delivery while Qt determines
            // whether the finger is tapping or beginning a vertical scroll.
            if (QScroller *scroller = QScroller::scroller(scroll->viewport())) {
                QScrollerProperties properties = scroller->scrollerProperties();
                // The left bank contains dual controls whose deliberate hold
                // selects the alternate adjustment. Deliver their press
                // immediately; an actual drag is still claimed below.
                properties.setScrollMetric(QScrollerProperties::MousePressEventDelay,
                                           scroll == m_leftPanelScroll ? 0.0 : 0.25);
                properties.setScrollMetric(QScrollerProperties::DragStartDistance, 0.0015);
                scroller->setScrollerProperties(properties);
                connect(scroller, &QScroller::stateChanged, scroll,
                        [this, scroll](QScroller::State state) {
                            if (state != QScroller::Dragging && state != QScroller::Scrolling)
                                return;
                            if (scroll == m_leftPanelScroll)
                                m_sideControlPanel->cancelPendingLongPress();
                            else if (scroll == m_rightPanelScroll)
                                m_rightSidePanel->cancelPendingLongPress();
                            // A button may already be visually down when the
                            // drag threshold is crossed. Cancel that pending
                            // click before the eventual finger release.
                            for (QPushButton *button : scroll->findChildren<QPushButton *>())
                                button->setDown(false);
                        });
            }
            columns->addWidget(scroll);
            scroll->show();
        }
        m_sideControlPanel->setFixedWidth(bankWidth - 18);
        m_rightSidePanel->setFixedWidth(bankWidth - 18);
        layout->addLayout(columns, 1);

        connect(close, &QPushButton::clicked, m_phoneControlsDialog, &QWidget::hide);
    }

    m_phoneControlsDialog->resize(drawerWidth, qMin(drawerHeight, available.height() - 4));
    const QPoint drawerGlobal(available.left() + (available.width() - m_phoneControlsDialog->width()) / 2,
                              available.bottom() - m_phoneControlsDialog->height() + 1);
    InWindowPopup::moveFromGlobal(m_phoneControlsDialog, drawerGlobal);
    m_phoneControlsDialog->show();
    // Begin a newly opened drawer at the operating controls. If an external
    // knob is already driving a selected row, do not jump to the top again on
    // every encoder report.
    if (!wasAlreadyVisible) {
        m_leftPanelScroll->verticalScrollBar()->setValue(m_leftPanelScroll->verticalScrollBar()->minimum());
        m_rightPanelScroll->verticalScrollBar()->setValue(m_rightPanelScroll->verticalScrollBar()->minimum());
    }
    m_phoneControlsDialog->raise();
    m_phoneControlsDialog->setFocus(Qt::OtherFocusReason);
}

void MainWindow::showPhoneControlAdjustment(const QString &action) {
    if (!m_sideControlPanel || !m_leftPanelScroll)
        return;
    if (m_featureMenuBar && m_featureMenuBar->isMenuVisible())
        m_featureMenuBar->hideMenu();

    // RIT/XIT has its own always-visible row above the two scrolling banks.
    if (action == QStringLiteral("rit_xit_frequency")) {
        showPhoneControls();
        return;
    }

    using Adjustment = SideControlPanel::Adjustment;
    Adjustment adjustment = Adjustment::MainVolume;
    if (action == QStringLiteral("main_volume")) adjustment = Adjustment::MainVolume;
    else if (action == QStringLiteral("sub_volume")) adjustment = Adjustment::SubVolume;
    else if (action == QStringLiteral("cw_speed")) {
        if (!m_sideControlPanel->isCwDisplayMode())
            return;
        adjustment = Adjustment::CwSpeed;
    }
    else if (action == QStringLiteral("rf_power")) adjustment = Adjustment::RfPower;
    else if (action == QStringLiteral("filter_bandwidth")) adjustment = Adjustment::FilterBandwidth;
    else if (action == QStringLiteral("filter_shift")) adjustment = Adjustment::FilterShift;
    else if (action == QStringLiteral("main_rf_gain")) adjustment = Adjustment::MainRfGain;
    else if (action == QStringLiteral("main_squelch")) adjustment = Adjustment::MainSquelch;
    else if (action == QStringLiteral("sub_squelch")) adjustment = Adjustment::SubSquelch;
    else if (action == QStringLiteral("sub_rf_gain")) adjustment = Adjustment::SubRfGain;
    else return;

    m_sideControlPanel->selectAdjustment(adjustment);
    QPointer<QWidget> control = m_sideControlPanel->adjustmentWidget(adjustment);
    showPhoneControls();
    if (control) {
        QTimer::singleShot(0, this, [this, control]() {
            if (control && m_leftPanelScroll)
                m_leftPanelScroll->ensureWidgetVisible(control, 8, 8);
        });
    }
}

void MainWindow::showMidiAdjustmentSurface(const QString &action) {
    if (action == QStringLiteral("attenuator_level")) {
        showFeatureAdjustment(static_cast<int>(FeatureMenuBar::Attenuator));
    } else if (action == QStringLiteral("noise_blanker_level")) {
        showFeatureAdjustment(static_cast<int>(FeatureMenuBar::NbLevel));
    } else if (action == QStringLiteral("nr_level")) {
        showFeatureAdjustment(static_cast<int>(FeatureMenuBar::NrAdjust));
    } else if (action == QStringLiteral("manual_notch_pitch")) {
        showFeatureAdjustment(static_cast<int>(FeatureMenuBar::ManualNotch));
    } else {
        showPhoneControlAdjustment(action);
    }
}

void MainWindow::showFrequencyEntry(bool vfoB) {
    InWindowDialog dialog(centralWidget());
    QWidget *panel = dialog.contentWidget();
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 6, 8, 8);
    layout->setSpacing(5);

    auto *hint = new QLabel("MHz: 7.2, 7.215, or 7.215.000 (trailing zeros implied)", panel);
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet(QString("color: %1; font-size: 14px;").arg(K4Styles::Colors::TextGray));
    layout->addWidget(hint);

    auto *entry = new QLineEdit(panel);
    entry->setReadOnly(true);
    entry->setAlignment(Qt::AlignCenter);
    auto formatFrequency = [](quint64 hertz) {
        QString text = QString::number(hertz);
        for (int position = text.size() - 3; position > 0; position -= 3)
            text.insert(position, '.');
        return text;
    };
    entry->setText(formatFrequency(vfoB ? m_radioState->vfoB() : m_radioState->vfoA()));
    entry->setMaxLength(13);
    entry->setStyleSheet(QString("background: %1; color: %2; border: 1px solid %3;"
                                 "border-radius: 6px; font-size: 25px; font-weight: bold; padding: 4px;")
                             .arg(K4Styles::Colors::DarkBackground, K4Styles::Colors::TextWhite,
                                  vfoB ? K4Styles::Colors::VfoBGreen : K4Styles::Colors::VfoACyan));
    layout->addWidget(entry);

    auto *keypad = new QGridLayout();
    keypad->setSpacing(4);
    auto addKey = [panel, keypad](const QString &text, int row, int column) {
        auto *button = new QPushButton(text, panel);
        button->setMinimumSize(72, 34);
        button->setStyleSheet(K4Styles::menuBarButton());
        keypad->addWidget(button, row, column);
        return button;
    };

    for (int digit = 1; digit <= 9; ++digit) {
        auto *button = addKey(QString::number(digit), (digit - 1) / 3, (digit - 1) % 3);
        connect(button, &QPushButton::clicked, &dialog, [entry, digit]() { entry->insert(QString::number(digit)); });
    }
    auto *clear = addKey("CLEAR", 3, 0);
    auto *zero = addKey("0", 3, 1);
    auto *backspace = addKey("DEL", 3, 2);
    connect(clear, &QPushButton::clicked, entry, &QLineEdit::clear);
    connect(zero, &QPushButton::clicked, &dialog, [entry]() { entry->insert("0"); });
    connect(backspace, &QPushButton::clicked, &dialog, [entry]() { entry->backspace(); });
    auto *cancel = addKey("CANCEL", 4, 0);
    auto *period = addKey(".", 4, 1);
    auto *set = addKey("ENTER", 4, 2);
    set->setStyleSheet(K4Styles::menuBarButtonActive());
    connect(period, &QPushButton::clicked, &dialog, [entry]() {
        if (!entry->text().endsWith('.') && entry->text().count('.') < 2)
            entry->insert(".");
    });
    layout->addLayout(keypad);
    connect(cancel, &QPushButton::clicked, &dialog, &InWindowDialog::reject);
    connect(set, &QPushButton::clicked, &dialog, &InWindowDialog::accept);

    layout->activate();
    dialog.setPanelSize(QSize(qMin(400, centralWidget()->width() - 16),
                              qMin(layout->sizeHint().height(), centralWidget()->height() - 12)));

    if (dialog.exec() != InWindowDialog::Accepted || !m_tcpClient->isConnected())
        return;

    quint64 hertz = 0;
    constexpr quint64 minimumFrequencyHz = 100000ULL;
    constexpr quint64 maximumFrequencyHz = 9999999999ULL;
    if (!FrequencyEntryParser::parse(entry->text(), &hertz) ||
        hertz < minimumFrequencyHz || hertz > maximumFrequencyHz) {
        showInWindowMessage(centralWidget(), "Invalid frequency",
                            "Enter MHz with at least one period (7.2 or 7.215), "
                            "a full grouped value (7.215.000), or raw Hz. "
                            "Trailing zeros are implied.");
        return;
    }

    const QString command = QString("%1%2;").arg(vfoB ? "FB" : "FA").arg(hertz, 11, 10, QChar('0'));
    m_tcpClient->sendCAT(command);
    m_radioState->parseCATCommand(command);
}

void MainWindow::onPttPressed() {
    if (m_sstvTxActive || m_sstvTxStarting || m_digitalCalibrating || m_ft8TxGeneration) {
        showControlFeedback(m_ft8TxGeneration ? "FT8/FT4 TRANSMIT ACTIVE · USE HALT TX"
                                            : "DIGITAL TRANSMIT ACTIVE · USE STOP");
        return;
    }
    if (!m_tcpClient->isConnected()) {
        return;
    }

#ifdef Q_OS_ANDROID
    if (!ensureMicrophonePermission(this)) {
        return;
    }
#endif

    // The Android control is a deliberate tap-to-TX latch rather than the
    // desktop's momentary PTT.  Key the K4 explicitly before enabling its
    // microphone stream so the radio state, meters, and UI enter TX even
    // before the first encoded frame arrives.  This is a PTT command, not
    // VOX; the upstream audio stream remains the voice-audio path.
    m_pttActive = true;
    m_tcpClient->sendCAT("TX;");
    QMetaObject::invokeMethod(m_audioEngine, "setPttActive", Qt::QueuedConnection, Q_ARG(bool, true));
    m_bottomMenuBar->setPttActive(true);
    setPhoneTxInputShieldActive(true);
    qDebug() << "Phone PTT enabled - K4 TX command and microphone stream active";
}

void MainWindow::onPttReleased() {
    stopFt8Transmit();
    // Always release the K4 first, including after a partial TX start.  The
    // separate audio gate is then closed so no more microphone frames follow.
    if (m_tcpClient && m_tcpClient->isConnected())
        m_tcpClient->sendCAT("RX;");
    m_pttActive = false;
    QMetaObject::invokeMethod(m_audioEngine, "setPttActive", Qt::QueuedConnection, Q_ARG(bool, false));
    m_bottomMenuBar->setPttActive(false);
    setPhoneTxInputShieldActive(false);
    qDebug() << "Phone PTT released - K4 RX command and microphone stream disabled";
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    // Match the K4 console: tapping either displayed filter shape selects the
    // next smaller filter setting: FIL1 -> FIL3 -> FIL2 -> FIL1.
    if ((watched == m_filterAWidget || watched == m_filterBWidget)
        && event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && m_tcpClient && m_tcpClient->isConnected()) {
            const bool isSub = watched == m_filterBWidget;
            const int current = isSub ? m_radioState->filterPositionB() : m_radioState->filterPosition();
            const int next = current >= 1 && current <= 3 ? (current == 1 ? 3 : current - 1) : 3;
            const QString command = QString("FP%1%2;").arg(isSub ? "$" : "").arg(next);
            m_tcpClient->sendCAT(command);
            // The K4 does not reliably echo an FP set immediately. Update
            // the normal state model now so the next tap advances again and
            // the existing position signal refreshes the FIL label.
            m_radioState->parseCATCommand(command);
        }
        return true;
    }

    // B SET can only be enabled from CTRL. When it is already active, tapping
    // its separate on-screen indicator is a quick way to return to VFO A.
    if (watched == m_bSetLabel && event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && m_tcpClient && m_tcpClient->isConnected()
            && m_radioState->bSetEnabled()) {
            queueControlFeedback("BSET", "B SET changed");
            m_tcpClient->sendCAT("SW44;");
        }
        return true;
    }

    // Match main QK4: tapping the central TX label switches the transmit VFO
    // (SPLIT OFF = VFO A TX, SPLIT ON = VFO B TX).  PTT remains separate.
    if (watched == m_txIndicator && event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && m_tcpClient && m_tcpClient->isConnected()) {
            m_tcpClient->sendCAT("SW145;");
        }
        return true;
    }

    // Handle clicks on VFO A square/mode label -> open mode popup for VFO A
    if ((watched == m_vfoASquare || watched == m_modeALabel) && event->type() == QEvent::MouseButtonPress) {
        // Toggle popup - close if open, otherwise show for VFO A
        if (m_modePopup->isVisible()) {
            m_modePopup->hidePopup();
        } else {
            m_modePopup->setFrequency(m_radioState->vfoA());
            m_modePopup->setCurrentMode(static_cast<int>(m_radioState->mode()));
            m_modePopup->setCurrentDataSubMode(m_radioState->dataSubMode());
            m_modePopup->setBSetEnabled(false); // Commands target VFO A
            m_modePopup->showAboveWidget(m_bottomMenuBar);
        }
        return true;
    }

    // Handle clicks on VFO B square/mode label -> open mode popup for VFO B
    if ((watched == m_vfoBSquare || watched == m_modeBLabel) && event->type() == QEvent::MouseButtonPress) {
        // Toggle popup - close if open, otherwise show for VFO B
        if (m_modePopup->isVisible()) {
            m_modePopup->hidePopup();
        } else {
            m_modePopup->setFrequency(m_radioState->vfoB());
            m_modePopup->setCurrentMode(static_cast<int>(m_radioState->modeB()));
            m_modePopup->setCurrentDataSubMode(m_radioState->dataSubModeB());
            m_modePopup->setBSetEnabled(true); // Commands target VFO B (MD$, DT$)
            m_modePopup->showAboveWidget(m_bottomMenuBar);
        }
        return true;
    }

    // Reposition span control buttons and VFO indicator when panadapter A resizes
    if (watched == m_panadapterA && event->type() == QEvent::Resize) {
        QResizeEvent *resizeEvent = static_cast<QResizeEvent *>(event);
        int w = resizeEvent->size().width();
        int h = resizeEvent->size().height();

        // Position buttons at lower right, above the frequency label bar (20px)
        // Triangle layout: C centered above, - and + below
        m_spanDownBtn->move(w - 70, h - 45);
        m_spanUpBtn->move(w - 35, h - 45);
        m_centerBtn->move(w - 52, h - 73);

        // VFO indicator at bottom-left corner
        m_vfoIndicatorA->move(0, h - 30);
    }

    // Reposition span control buttons and VFO indicator when panadapter B resizes
    if (watched == m_panadapterB && event->type() == QEvent::Resize) {
        QResizeEvent *resizeEvent = static_cast<QResizeEvent *>(event);
        int w = resizeEvent->size().width();
        int h = resizeEvent->size().height();

        // Position B buttons at lower right, above the frequency label bar (20px)
        // Triangle layout: C centered above, - and + below
        m_spanDownBtnB->move(w - 70, h - 45);
        m_spanUpBtnB->move(w - 35, h - 45);
        m_centerBtnB->move(w - 52, h - 73);

        // VFO indicator at bottom-left corner
        m_vfoIndicatorB->move(0, h - 30);
    }

    // Handle right-click on memory buttons (alternate actions)
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::RightButton) {
            if (watched == m_recBtn) {
                m_tcpClient->sendCAT("SW137;"); // BANK
                return true;
            } else if (watched == m_storeBtn) {
                m_tcpClient->sendCAT("SW138;"); // AF REC
                return true;
            } else if (watched == m_rclBtn) {
                m_tcpClient->sendCAT("SW139;"); // AF PLAY
                return true;
            }
        }
    }

    // RIT/XIT status: desktop keeps click + wheel behavior. On a phone, a
    // short tap toggles while a long press opens the offset jog control.
    if (watched == m_ritXitBox || watched == m_ritLabel || watched == m_xitLabel
        || watched == m_ritXitValueLabel) {
        if (K4Styles::isCompactLayout() && event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_ritXitPressTarget = watched;
                m_ritXitLongPressHandled = false;
                m_ritXitLongPressTimer->start();
                return true;
            }
        }
        if (K4Styles::isCompactLayout() && event->type() == QEvent::MouseButtonRelease) {
            m_ritXitLongPressTimer->stop();
            const bool handled = m_ritXitLongPressHandled;
            QObject *pressed = m_ritXitPressTarget;
            m_ritXitPressTarget = nullptr;
            m_ritXitLongPressHandled = false;
            if (!handled && pressed == watched && (watched == m_ritLabel || watched == m_xitLabel)) {
                if (watched == m_ritLabel) {
                    const bool bSet = m_radioState->bSetEnabled();
                    m_tcpClient->sendCAT(bSet ? "SW54;" : "RT/;");
                    if (bSet) {
                        m_tcpClient->sendCAT("RT$;");
                        m_tcpClient->sendCAT("RO$;");
                    }
                } else {
                    m_tcpClient->sendCAT("XT/;");
                }
            }
            return true;
        }
        if (!K4Styles::isCompactLayout() && event->type() == QEvent::MouseButtonPress) {
            if (watched == m_ritLabel) {
                const bool bSet = m_radioState->bSetEnabled();
                m_tcpClient->sendCAT(bSet ? "SW54;" : "RT/;");
                if (bSet) {
                    m_tcpClient->sendCAT("RT$;");
                    m_tcpClient->sendCAT("RO$;");
                }
            } else {
                m_tcpClient->sendCAT("XT/;");
            }
            return true;
        }
    }

    // Mouse wheel on RIT/XIT box (or its child widgets) - adjust offset using RU/RD commands
    // B SET aware: use $ suffix when targeting Sub RX
    if (event->type() == QEvent::Wheel &&
        (watched == m_ritXitBox || watched == m_ritLabel || watched == m_xitLabel || watched == m_ritXitValueLabel)) {
        auto *wheelEvent = static_cast<QWheelEvent *>(event);
        int steps = m_ritWheelAccumulator.accumulate(wheelEvent);
        if (steps != 0) {
            // Mainline routing: XIT makes plain RU/RD follow the TX VFO.
            // Only B SET + RIT-only needs the explicit $ register.
            const bool adjustB = m_radioState->bSetEnabled() && !m_radioState->xitEnabled();
            QString upCmd = adjustB ? "RU$;" : "RU;";
            QString downCmd = adjustB ? "RD$;" : "RD;";
            for (int i = 0; i < qAbs(steps); ++i)
                m_tcpClient->sendCAT(steps > 0 ? upCmd : downCmd);
        }
        return true;
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (m_radioManager && centralWidget()) {
        m_radioManager->setGeometry(centralWidget()->rect());
    }
    if (m_sstvScreen && m_sstvScreen->isVisible()) {
        m_sstvScreen->setGeometry(rect());
    }
    if (m_ft8Screen && m_ft8Screen->isVisible()) m_ft8Screen->setGeometry(rect());
    updatePhoneTxInputShieldGeometry();
}

void MainWindow::setPhoneTxInputShieldActive(bool active) {
    if (!m_phoneTxInputShield || !m_phoneTxReleaseButton)
        return;

    if (!active) {
        m_phoneTxInputShield->hide();
        return;
    }

    updatePhoneTxInputShieldGeometry();
    m_phoneTxInputShield->show();
    m_phoneTxInputShield->raise();
}

void MainWindow::updatePhoneTxInputShieldGeometry() {
    if (!m_phoneTxInputShield || !m_phoneTxReleaseButton || !centralWidget() || !m_bottomMenuBar)
        return;

    m_phoneTxInputShield->setGeometry(centralWidget()->rect());
    QPushButton *pttButton = m_bottomMenuBar->pttButton();
    const QPoint pttTopLeft = centralWidget()->mapFromGlobal(pttButton->mapToGlobal(QPoint(0, 0)));
    m_phoneTxReleaseButton->setGeometry(QRect(pttTopLeft, pttButton->size()));
}

void MainWindow::changeEvent(QEvent *event) {
    // Audio runs on its own thread now — no flush needed on minimize/restore.
    // The audio thread keeps playing smoothly; the waterfall catches up visually on restore.
    QMainWindow::changeEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    // Handle F1-F12 for keyboard macros
    if (event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F12) {
        int fKeyNum = event->key() - Qt::Key_F1 + 1; // 1-12
        QString functionId = QString("Keyboard-F%1").arg(fKeyNum);
        executeMacro(functionId);
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::setPanadapterMode(PanadapterMode mode) {
    m_panadapterMode = mode;
    switch (mode) {
    case PanadapterMode::MainOnly:
        m_panadapterA->show();
        m_panadapterB->hide();
        break;
    case PanadapterMode::Dual:
        m_panadapterA->show();
        m_panadapterB->show();
        break;
    case PanadapterMode::SubOnly:
        m_panadapterA->hide();
        m_panadapterB->show();
        break;
    }
}

void MainWindow::showMenuOverlay() {
    const bool wasVisible = m_menuOverlay && m_menuOverlay->isVisible();
    closeAllPopups();

    // Toggle menu overlay visibility
    if (m_spectrumContainer && m_menuOverlay) {
        if (!wasVisible) {
            // Show the overlay
            QPoint pos = m_spectrumContainer->mapTo(this, QPoint(0, 0));
            m_menuOverlay->setGeometry(pos.x(), pos.y(), m_spectrumContainer->width(), m_spectrumContainer->height());
            m_menuOverlay->show();
            m_menuOverlay->raise();

            // Set MENU button to active (inverse colors)
            if (m_bottomMenuBar) {
                m_bottomMenuBar->setMenuActive(true);
            }
        }
    }
}

void MainWindow::onMenuValueChangeRequested(int menuId, const QString &action) {
    // Handle synthetic menu items (negative IDs)
    if (menuId == MenuModel::SYNTHETIC_DISPLAY_FPS_ID) {
        MenuItem *item = m_menuModel->getMenuItem(menuId);
        if (!item)
            return;

        int newValue = item->currentValue;
        if (action == "+") {
            newValue = qMin(item->currentValue + 1, 30);
        } else if (action == "-") {
            newValue = qMax(item->currentValue - 1, 12);
        }

        // Update menu model
        m_menuModel->updateValue(menuId, newValue);

        // Send #FPS command (not ME command)
        if (m_tcpClient->isConnected()) {
            qDebug() << "Display FPS change:" << QString("#FPS%1;").arg(newValue);
            m_tcpClient->sendCAT(QString("#FPS%1;").arg(newValue));
        }

        // Update stored preference
        m_currentRadio.displayFps = newValue;
        return;
    }

    // Build and send ME command for real K4 menu items
    // action: "+" = increment, "-" = decrement, "/" = toggle
    QString cmd = QString("ME%1.%2;").arg(menuId, 4, 10, QChar('0')).arg(action);
    qDebug() << "Menu value change:" << cmd;

    // For prototype: update local model immediately (optimistic update)
    MenuItem *item = m_menuModel->getMenuItem(menuId);
    if (item) {
        int newValue = item->currentValue;
        if (action == "+") {
            newValue = qMin(item->currentValue + item->step, item->maxValue);
        } else if (action == "-") {
            newValue = qMax(item->currentValue - item->step, item->minValue);
        } else if (action == "/") {
            // Toggle binary
            newValue = (item->currentValue == 0) ? 1 : 0;
        }
        m_menuModel->updateValue(menuId, newValue);
    }

    // When connected to radio, send the command
    if (m_tcpClient->isConnected()) {
        m_tcpClient->sendCAT(cmd);
    }
}

void MainWindow::onMenuModelValueChanged(int menuId, int newValue) {
    // Check if this is the "Spectrum Amplitude Units" menu item
    const MenuItem *item = m_menuModel->getMenuItem(menuId);
    if (item && item->name == "Spectrum Amplitude Units") {
        // 0 = dBm, 1 = S-UNITS
        bool useSUnits = (newValue == 1);
        qDebug() << "Spectrum amplitude units changed:" << (useSUnits ? "S-UNITS" : "dBm");

        if (m_panadapterA) {
            m_panadapterA->setAmplitudeUnits(useSUnits);
        }
        if (m_panadapterB) {
            m_panadapterB->setAmplitudeUnits(useSUnits);
        }
    }

    // Track "Mouse L/R Button QSY" setting changes (from radio or menu overlay)
    if (menuId == m_mouseQsyMenuId) {
        m_mouseQsyMode = newValue;
        qDebug() << "Mouse L/R Button QSY changed to:" << m_mouseQsyMode;
    }
}

void MainWindow::toggleDisplayPopup() {
    bool wasVisible = m_displayPopup && m_displayPopup->isVisible();
    closeAllPopups();

    if (!wasVisible && m_displayPopup && m_bottomMenuBar) {
        m_displayPopup->showAboveButton(m_bottomMenuBar->displayButton());
        m_bottomMenuBar->setDisplayActive(true);
    }
}

void MainWindow::closeAllPopups() {
    if (m_closingTransientMenus)
        return;
    m_closingTransientMenus = true;

    closeSecondaryPopups();

    // Close menu overlay
    if (m_menuOverlay && m_menuOverlay->isVisible()) {
        m_menuOverlay->hide();
        if (m_bottomMenuBar) {
            m_bottomMenuBar->setMenuActive(false);
        }
    }

    // Close band popup
    if (m_bandPopup && m_bandPopup->isVisible()) {
        m_bandPopup->hidePopup();
        if (m_bottomMenuBar) {
            m_bottomMenuBar->setBandActive(false);
        }
    }

    // Close display popup
    if (m_displayPopup && m_displayPopup->isVisible()) {
        m_displayPopup->hidePopup();
        if (m_bottomMenuBar) {
            m_bottomMenuBar->setDisplayActive(false);
        }
    }

    // Close Fn popup
    if (m_fnPopup && m_fnPopup->isVisible()) {
        m_fnPopup->hidePopup();
        if (m_bottomMenuBar) {
            m_bottomMenuBar->setFnActive(false);
        }
    }

    // Close Main RX popup
    if (m_mainRxPopup && m_mainRxPopup->isVisible()) {
        m_mainRxPopup->hidePopup();
        if (m_bottomMenuBar) {
            m_bottomMenuBar->setMainRxActive(false);
        }
    }

    // Close Sub RX popup
    if (m_subRxPopup && m_subRxPopup->isVisible()) {
        m_subRxPopup->hidePopup();
        if (m_bottomMenuBar) {
            m_bottomMenuBar->setSubRxActive(false);
        }
    }

    // Close TX popup
    if (m_txPopup && m_txPopup->isVisible()) {
        m_txPopup->hidePopup();
        if (m_bottomMenuBar) {
            m_bottomMenuBar->setTxActive(false);
        }
    }

    m_closingTransientMenus = false;
}

void MainWindow::closeSecondaryPopups() {
    const bool wasClosing = m_closingTransientMenus;
    m_closingTransientMenus = true;

    // These are all sibling overlays on Android. Closing a parent row does
    // not implicitly hide one of its editors, so close the complete operating
    // menu layer explicitly.
    if (m_modePopup && m_modePopup->isVisible()) m_modePopup->hidePopup();
    if (m_featureMenuBar && m_featureMenuBar->isVisible()) m_featureMenuBar->hideMenu();
    if (m_mainRxAntCfgPopup && m_mainRxAntCfgPopup->isVisible()) m_mainRxAntCfgPopup->hidePopup();
    if (m_subRxAntCfgPopup && m_subRxAntCfgPopup->isVisible()) m_subRxAntCfgPopup->hidePopup();
    if (m_txAntCfgPopup && m_txAntCfgPopup->isVisible()) m_txAntCfgPopup->hidePopup();
    if (m_rxEqPopup && m_rxEqPopup->isVisible()) m_rxEqPopup->hidePopup();
    if (m_txEqPopup && m_txEqPopup->isVisible()) m_txEqPopup->hidePopup();
    if (m_lineOutPopup && m_lineOutPopup->isVisible()) m_lineOutPopup->hidePopup();
    if (m_lineInPopup && m_lineInPopup->isVisible()) m_lineInPopup->hidePopup();
    if (m_micInputPopup && m_micInputPopup->isVisible()) m_micInputPopup->hidePopup();
    if (m_micConfigPopup && m_micConfigPopup->isVisible()) m_micConfigPopup->hidePopup();
    if (m_voxPopup && m_voxPopup->isVisible()) m_voxPopup->hidePopup();
    if (m_ssbBwPopup && m_ssbBwPopup->isVisible()) m_ssbBwPopup->hidePopup();
    if (m_keyingWeightPopup && m_keyingWeightPopup->isVisible()) m_keyingWeightPopup->hidePopup();
    if (m_fmPlPopup && m_fmPlPopup->isVisible()) m_fmPlPopup->hidePopup();
    if (m_dtmfPopup && m_dtmfPopup->isVisible()) m_dtmfPopup->hidePopup();
    if (m_txModePopup && m_txModePopup->isVisible()) m_txModePopup->hidePopup();
    if (m_softwareListPopup && m_softwareListPopup->isVisible()) m_softwareListPopup->hidePopup();

    m_closingTransientMenus = wasClosing;
}

void MainWindow::toggleBandPopup() {
    bool wasVisible = m_bandPopup && m_bandPopup->isVisible();
    closeAllPopups();

    if (!wasVisible && m_bandPopup && m_bottomMenuBar) {
        m_bandPopup->showAboveButton(m_bottomMenuBar->bandButton());
        m_bottomMenuBar->setBandActive(true);
    }
}

void MainWindow::toggleFnPopup() {
    bool wasVisible = m_fnPopup && m_fnPopup->isVisible();
    closeAllPopups();

    if (!wasVisible && m_fnPopup && m_bottomMenuBar) {
        m_fnPopup->showAboveButton(m_bottomMenuBar->fnButton());
        m_bottomMenuBar->setFnActive(true);
    }
}

void MainWindow::toggleMainRxPopup() {
    bool wasVisible = m_mainRxPopup && m_mainRxPopup->isVisible();
    closeAllPopups();

    if (!wasVisible && m_mainRxPopup && m_bottomMenuBar) {
        if (m_tcpClient && m_tcpClient->isConnected())
            m_tcpClient->sendCAT("MD;DT;AP;");
        refreshRxPopupsForModes();
        m_mainRxPopup->showAboveButton(m_bottomMenuBar->mainRxButton());
        m_bottomMenuBar->setMainRxActive(true);
    }
}

void MainWindow::toggleSubRxPopup() {
    bool wasVisible = m_subRxPopup && m_subRxPopup->isVisible();
    closeAllPopups();

    if (!wasVisible && m_subRxPopup && m_bottomMenuBar) {
        // Query VFO B directly. This provides B SET-equivalent targeting for
        // the menu without changing the operator's actual B SET state.
        if (m_tcpClient && m_tcpClient->isConnected())
            m_tcpClient->sendCAT("MD$;DT$;AP$;");
        refreshRxPopupsForModes();
        m_subRxPopup->showAboveButton(m_bottomMenuBar->subRxButton());
        m_bottomMenuBar->setSubRxActive(true);
    }
}

void MainWindow::toggleTxPopup() {
    bool wasVisible = m_txPopup && m_txPopup->isVisible();
    closeAllPopups();

    if (!wasVisible && m_txPopup && m_bottomMenuBar) {
        refreshTxPopupForMode();
        m_txPopup->showAboveButton(m_bottomMenuBar->txButton());
        m_bottomMenuBar->setTxActive(true);
    }
}

void MainWindow::onBandSelected(const QString &bandName) {
    qDebug() << "Band selected:" << bandName;

    // GEN's mobile SWL bank uses direct, deliberate frequency/mode selection.
    // It is separate from K4 BN band-stack state so switching back to the ham
    // bank preserves the normal K4 band behavior.
    if (m_bandPopup->isShortwaveBand(bandName)) {
        const quint64 frequencyHz = m_bandPopup->shortwaveBandTuneHz(bandName);
        if (m_tcpClient->isConnected() && frequencyHz > 0) {
            const bool bSetEnabled = m_radioState->bSetEnabled();
            m_bandPopup->setActiveShortwaveBand(bSetEnabled, bandName);
            const QString frequencyPrefix = bSetEnabled ? "FB" : "FA";
            const QString modePrefix = bSetEnabled ? "MD$" : "MD";
            const QString queryFrequency = bSetEnabled ? "FB;" : "FA;";
            const QString queryMode = bSetEnabled ? "MD$;" : "MD;";

            // International shortwave broadcast bands are AM service.
            m_tcpClient->sendCAT(QString("%1%2;%3%4;%5%6")
                                     .arg(frequencyPrefix)
                                     .arg(frequencyHz, 11, 10, QChar('0'))
                                     .arg(modePrefix)
                                     .arg(5)
                                     .arg(queryFrequency)
                                     .arg(queryMode));
        }
        return;
    }

    // Get band number from name
    int newBandNum = m_bandPopup->getBandNumber(bandName);

    // GEN and MEM are special modes, not band changes (-1 returned)
    if (newBandNum < 0) {
        qDebug() << "Special mode selected (GEN/MEM) - no BN command";
        return;
    }

    // A normal K4 band selection ends the GEN-only local memory context even
    // if the radio is currently disconnected.
    const bool bSetEnabled = m_radioState->bSetEnabled();
    m_bandPopup->clearActiveShortwaveBand(bSetEnabled);

    if (m_tcpClient->isConnected()) {
        // Check if BSET is enabled - target VFO B (Sub RX) instead of VFO A
        int currentBand = bSetEnabled ? m_currentBandNumB : m_currentBandNum;
        QString cmdPrefix = bSetEnabled ? "BN$" : "BN";

        if (newBandNum == currentBand) {
            // Same band tapped - invoke band stack
            QString bandStackCmd = bSetEnabled ? "BN$^;" : "BN^;";
            qDebug() << "Same band - invoking band stack with" << bandStackCmd;
            m_tcpClient->sendCAT(bandStackCmd);
        } else {
            // Different band selected - change band
            QString cmd = QString("%1%2;").arg(cmdPrefix).arg(newBandNum, 2, 10, QChar('0'));
            qDebug() << "Changing band:" << cmd;
            m_tcpClient->sendCAT(cmd);
        }
        // Request current band to update UI
        QString queryCmd = bSetEnabled ? "BN$;" : "BN;";
        m_tcpClient->sendCAT(queryCmd);
    }
}

void MainWindow::updateBandSelection(int bandNum) {
    m_currentBandNum = bandNum;

    // Update the band popup to show the current band as selected (only when not in BSET mode)
    if (m_bandPopup && !m_radioState->bSetEnabled()) {
        m_bandPopup->setSelectedBandByNumber(bandNum);
    }
}

void MainWindow::updateBandSelectionB(int bandNum) {
    m_currentBandNumB = bandNum;

    // Update the band popup to show the current band as selected (only when in BSET mode)
    if (m_bandPopup && m_radioState->bSetEnabled()) {
        m_bandPopup->setSelectedBandByNumber(bandNum);
    }
}

// ============== KPOD Event Handlers ==============

void MainWindow::onKpodEncoderRotated(int ticks) {
    if (!m_tcpClient->isConnected()) {
        return;
    }

    // Action depends on rocker position
    switch (m_kpodDevice->rockerPosition()) {
    case KpodDevice::RockerLeft: // VFO A
    {
        quint64 currentFreq = m_radioState->vfoA();
        int stepHz = tuningStepToHz(m_radioState->tuningStep());
        qint64 newFreq = static_cast<qint64>(currentFreq) + static_cast<qint64>(ticks) * stepHz;
        if (newFreq > 0) {
            QString cmd = QString("FA%1;").arg(static_cast<quint64>(newFreq));
            m_tcpClient->sendCAT(cmd);
            m_radioState->parseCATCommand(cmd);
        }
    } break;

    case KpodDevice::RockerCenter: // VFO B
    {
        quint64 currentFreq = m_radioState->vfoB();
        int stepHz = tuningStepToHz(m_radioState->tuningStepB());
        qint64 newFreq = static_cast<qint64>(currentFreq) + static_cast<qint64>(ticks) * stepHz;
        if (newFreq > 0) {
            QString cmd = QString("FB%1;").arg(static_cast<quint64>(newFreq));
            m_tcpClient->sendCAT(cmd);
            m_radioState->parseCATCommand(cmd);
        }
    } break;

    case KpodDevice::RockerRight: // RIT/XIT
        // Adjust RIT/XIT offset using RU/RD commands
        {
            QString cmd = (ticks > 0) ? "RU;" : "RD;";
            int count = qAbs(ticks);
            for (int i = 0; i < count; i++) {
                m_tcpClient->sendCAT(cmd);
            }
        }
        break;
    }
}

void MainWindow::onKpodRockerChanged(int position) {
    QString posName;
    switch (static_cast<KpodDevice::RockerPosition>(position)) {
    case KpodDevice::RockerLeft:
        posName = "VFO A";
        break;
    case KpodDevice::RockerCenter:
        posName = "VFO B";
        break;
    case KpodDevice::RockerRight:
        posName = "XIT/RIT";
        break;
    default:
        posName = "Unknown";
        break;
    }
    Q_UNUSED(posName)
}

void MainWindow::onKpodPollError(const QString &error) {
    qWarning() << "KPOD error:" << error;
}

void MainWindow::onKpodEnabledChanged(bool enabled) {
    if (enabled) {
        if (m_kpodDevice->isDetected()) {
            m_kpodDevice->startPolling();
        }
    } else {
        m_kpodDevice->stopPolling();
    }
}

// ============== K4 Error/Notification Slots ==============

void MainWindow::onErrorNotification(int errorCode, const QString &message) {
    Q_UNUSED(errorCode)
    // Show the notification message in a centered popup
    // The message contains the text after "ERxx:" (e.g., "KPA1500 Status: operate.")
    if (m_notificationWidget) {
        m_notificationWidget->showMessage(message, 2000);
    }
}

// ============== KPA1500 Amplifier Slots ==============

void MainWindow::onKpa1500Connected() {
    qDebug() << "KPA1500: Connected to amplifier";
    // Start polling with configured interval
    int pollInterval = RadioSettings::instance()->kpa1500PollInterval();
    m_kpa1500Client->startPolling(pollInterval);
    updateKpa1500Status();
}

void MainWindow::onKpa1500Disconnected() {
    qDebug() << "KPA1500: Disconnected from amplifier";
    updateKpa1500Status();
}

void MainWindow::onKpa1500Error(const QString &error) {
    qWarning() << "KPA1500: Error -" << error;
}

void MainWindow::onKpa1500EnabledChanged(bool enabled) {
    if (enabled) {
        // Connect if host is configured
        QString host = RadioSettings::instance()->kpa1500Host();
        if (!host.isEmpty()) {
            m_kpa1500Client->connectToHost(host, RadioSettings::instance()->kpa1500Port());
        }
    } else {
        m_kpa1500Client->disconnectFromHost();
    }
    updateKpa1500Status();
}

void MainWindow::onKpa1500SettingsChanged() {
    // Reconnect with new settings if enabled
    if (RadioSettings::instance()->kpa1500Enabled()) {
        m_kpa1500Client->disconnectFromHost();
        QString host = RadioSettings::instance()->kpa1500Host();
        if (!host.isEmpty()) {
            m_kpa1500Client->connectToHost(host, RadioSettings::instance()->kpa1500Port());
        }
    }
    updateKpa1500Status();
}

void MainWindow::updateKpa1500Status() {
    bool enabled = RadioSettings::instance()->kpa1500Enabled();
    bool connected = m_kpa1500Client && m_kpa1500Client->isConnected();

    if (!enabled) {
        m_kpa1500StatusLabel->hide();
    } else {
        m_kpa1500StatusLabel->show();
        if (connected) {
            m_kpa1500StatusLabel->setText("KPA1500");
            m_kpa1500StatusLabel->setStyleSheet(
                QString("color: %1; font-size: 12px; font-weight: bold;").arg(K4Styles::Colors::StatusGreen));
        } else {
            m_kpa1500StatusLabel->setText("KPA1500");
            m_kpa1500StatusLabel->setStyleSheet(
                QString("color: %1; font-size: 12px;").arg(K4Styles::Colors::InactiveGray));
        }
    }

    // Show KPA1500 window only when enabled AND connected
    m_kpa1500Window->setVisible(enabled && connected);
    m_kpa1500Window->panel()->setConnected(connected);
}

// ============== Fn Popup / Macro Slots ==============

void MainWindow::onFnFunctionTriggered(const QString &functionId) {
    qDebug() << "Fn function triggered:" << functionId;

    // Handle built-in functions
    if (functionId == MacroIds::Ft8) {
        openFt8Screen();
    } else if (functionId == MacroIds::Sstv) {
        openSstvScreen();
    } else if (functionId == MacroIds::ScrnCap) {
        // SS0; triggers K4 screenshot (saved to internal SD card)
        if (m_tcpClient && m_tcpClient->isConnected()) {
            m_tcpClient->sendCAT("SS0;");
            qDebug() << "Screenshot captured (SS0;)";
        }
    } else if (functionId == MacroIds::Macros) {
        openMacroDialog();
    } else if (functionId == MacroIds::SwList) {
        closeAllPopups();
        m_softwareListPopup->setVersions(m_radioState->firmwareVersions());
        m_softwareListPopup->showAboveWidget(m_bottomMenuBar);
    } else if (functionId == MacroIds::Update) {
        showInWindowMessage(centralWidget(), "UPDATE",
                            "Radio firmware installation is intentionally not started remotely from QK4 Mobile.");
    } else if (functionId == MacroIds::DxList) {
        closeAllPopups();
        showDxPrefixList(centralWidget());
    } else if (functionId == MacroIds::Log) {
        openLogbook();
    } else {
        // User-configurable macro - execute CAT command
        executeMacro(functionId);
    }
}

QString MainWindow::ft8TransmitReadiness() const {
    if (!m_tcpClient->isConnected()) return "Connect to the K4 before transmitting.";
    if (m_pttActive || m_radioState->isTransmitting() || m_sstvTxActive || m_sstvTxStarting || m_digitalCalibrating || m_ft8TxGeneration)
        return "Wait for the current transmission or calibration to finish.";
    if (m_radioState->testMode()) return "Turn K4 TEST off to transmit on air.";
    if (m_radioState->splitEnabled()) return "Turn split off for FT8/FT4 transmission.";
    if (m_radioState->modeStringFull() != "DATA") return "Select DATA-A mode before transmitting FT8/FT4.";
    QSettings settings("QK4", "QK4");
    if (!DigitalTxCalibration::load(settings, digitalCalibrationContext(int(m_ft8Screen->mode()))))
        return "Calibrate TX audio for this radio setup first (Options → TX audio setup).";
    return {};
}
void MainWindow::startFt8Transmit(const QString &message, int mode, int hz, qint64 slotUtc) {
    const auto reason = ft8TransmitReadiness();
    if (!reason.isEmpty() || m_ft8TxGeneration) {
        m_ft8Screen->finishLiveTransmit(false, reason.isEmpty() ? "FT transmission already active." : reason);
        return;
    }
    m_ft8TransmitContext = digitalCalibrationContext(mode);
    m_ft8TransmitFrequency = m_radioState->vfoA();
    QSettings settings("QK4", "QK4");
    const auto gain = DigitalTxCalibration::load(settings, m_ft8TransmitContext);
    if (!gain) { m_ft8Screen->finishLiveTransmit(false, "TX calibration is unavailable."); return; }
    const auto gen = m_ft8TxGeneration = ++m_sstvGeneration;
    auto control = m_tcpClient->digitalTxControl();
    control->gain.store(qMin(*gain, m_digitalReducedGains.value(DigitalTxCalibration::matchingContext(m_ft8TransmitContext), *gain)));
    control->scheduledGeneration.store(gen);
    const int frameSamples = streamingLatencyToFrameSamples(m_currentRadio.streamingLatency);
    QMetaObject::invokeMethod(m_ft8Transmitter, [this, message, mode, hz, slotUtc, frameSamples, gen] {
        m_ft8Transmitter->schedule(message, mode, hz, slotUtc, frameSamples, gen);
    }, Qt::QueuedConnection);
}
void MainWindow::stopFt8Transmit() {
    if (!m_ft8TxGeneration) return;
    const auto gen = m_ft8TxGeneration;
    auto expected = gen;
    m_tcpClient->digitalTxControl()->scheduledGeneration.compare_exchange_strong(expected, 0);
    m_tcpClient->digitalTxControl()->close(gen);
    QMetaObject::invokeMethod(m_ft8Transmitter, [this, gen] { m_ft8Transmitter->cancel(gen); }, Qt::QueuedConnection);
    QMetaObject::invokeMethod(m_tcpClient, [this, gen] { m_tcpClient->stopScheduledDigitalAudio(gen); }, Qt::QueuedConnection);
}
void MainWindow::refreshFt8Capture() {
    const bool enabled = m_ft8Screen && m_ft8Screen->receiving()
        && m_tcpClient->isConnected() && !m_radioState->isTransmitting()
        && !m_sstvTxActive && !m_sstvTxStarting && !m_digitalCalibrating
        && QGuiApplication::applicationState() == Qt::ApplicationActive;
    m_ft8Receiver->setCapture(enabled, m_ft8Screen ? m_ft8Screen->mode() : Ft8::Mode::FT8);
}

void MainWindow::updateFt8RadioMode() {
    const auto command = m_ft8RadioMode.update(m_tcpClient->isConnected(), int(m_radioState->mode()),
        m_radioState->dataSubMode(), m_radioState->isTransmitting() || m_pttActive || m_ft8TxGeneration
            || m_sstvTxActive || m_sstvTxStarting || m_digitalCalibrating);
    if (!command.isEmpty()) m_tcpClient->sendCAT(command);
}
void MainWindow::openFt8Screen() {
    if (m_sstvTxActive || m_sstvTxStarting || m_pttActive || m_radioState->isTransmitting()) {
        showControlFeedback("Stop transmission before opening FT8/FT4");
        return;
    }
    closeAllPopups();
    m_ft8RadioMode.enter();
    updateFt8RadioMode();
    m_sstvRxArmed.store(false, std::memory_order_release);
    if (m_sstvScreen) m_sstvScreen->hide();
    if (!m_ft8Screen) {
        m_ft8Screen = new Ft8Screen(this);
        connect(m_ft8Screen, &Ft8Screen::liveArmRequested, this, [this] {
            const auto reason = ft8TransmitReadiness();
            if (!reason.isEmpty()) m_ft8Screen->setTransmitProtection(reason, true);
            else {
                m_tcpClient->acknowledgeDigitalTxFault(); // Explicit operator Call/CQ only.
                m_ft8Screen->setTransmitProtection("TX armed · calibrated audio protection active", false);
            }
        });
        connect(m_ft8Screen, &Ft8Screen::liveTransmitRequested, this, &MainWindow::startFt8Transmit);
        connect(m_ft8Screen, &Ft8Screen::liveStopRequested, this, &MainWindow::stopFt8Transmit);
        connect(m_panadapterA, &PanadapterRhiWidget::waterfallAppearanceChanged,
                m_ft8Screen, &Ft8Screen::setWaterfallAppearance);
        connect(m_ft8Screen, &Ft8Screen::closeRequested, this, [this] {
            m_ft8Screen->suspend();
            m_ft8Screen->hide();
            m_ft8RadioMode.leave();
            updateFt8RadioMode();
            refreshFt8Capture();
            setFt8PortraitEnabled(false);
            if (centralWidget()) centralWidget()->show();
        });
        connect(m_ft8Screen, &Ft8Screen::captureChanged, this, [this] {
            // Explicit screen changes (including retuning within the same
            // mode) discard the old slot. Routine readiness refreshes do not.
            m_ft8Receiver->setCapture(false, m_ft8Screen->mode());
            refreshFt8Capture();
        });
        connect(m_ft8Screen, &Ft8Screen::audioSetupRequested, this, [this] {
            showDigitalAudioSetup(m_ft8Screen->mode() == Ft8::Mode::FT4 ? int(DigitalTxGuard::Mode::Ft4)
                                                                      : int(DigitalTxGuard::Mode::Ft8), m_ft8Screen);
        });
        connect(m_ft8Screen, &Ft8Screen::powerRequested, this, [this](double watts) {
            if (!m_ft8Screen->practice() && !m_pttActive && !m_radioState->isTransmitting())
                setSstvRfPower(watts); // Reuse the established digital-module PC command path.
        });
        connect(m_radioState, &RadioState::rfPowerChanged, m_ft8Screen,
                [this](double watts, bool) { m_ft8Screen->setRfPower(watts); });
        connect(m_ft8Screen, &Ft8Screen::frequencyRequested, this, [this](qint64 hz) {
            if (m_tcpClient->isConnected() && !m_radioState->isTransmitting()
                && !m_sstvTxActive && !m_sstvTxStarting)
                m_tcpClient->sendCAT(QString("FA%1;FA;").arg(hz, 11, 10, QChar('0')));
        });
        const auto updateRadio = [this] {
            m_ft8Screen->setRadioState(m_tcpClient->isConnected(), qint64(m_radioState->vfoA()),
                                       m_radioState->modeStringFull(), m_radioState->isTransmitting());
            m_ft8Screen->setRfPower(m_radioState->rfPower());
        };
        connect(m_radioState, &RadioState::frequencyChanged, m_ft8Screen, updateRadio);
        connect(m_radioState, &RadioState::modeChanged, m_ft8Screen, updateRadio);
        connect(m_radioState, &RadioState::dataSubModeChanged, m_ft8Screen, updateRadio);
        connect(m_radioState, &RadioState::transmitStateChanged, m_ft8Screen, updateRadio);
        connect(m_tcpClient, &TcpClient::stateChanged, m_ft8Screen, updateRadio);
        connect(m_ft8Receiver, &Ft8Receiver::decoded, m_ft8Screen,
                [this](const QVector<Ft8::Decode> &messages, quint64 generation) {
            if (generation == m_ft8Receiver->generation() && m_ft8Screen->receiving()) m_ft8Screen->addDecodes(messages);
        });
        connect(m_ft8Receiver, &Ft8Receiver::spectrum, m_ft8Screen,
                [this](const QVector<float> &db, double firstHz, double binHz, quint64 generation) {
            if (generation == m_ft8Receiver->generation() && m_ft8Screen->receiving()) m_ft8Screen->addSpectrum(db, firstHz, binHz);
        });
        connect(m_ft8Receiver, &Ft8Receiver::streamStatus, m_ft8Screen,
                [this](const QString &status, quint64 generation) {
            if (generation == m_ft8Receiver->generation()) m_ft8Screen->setReceiveStatus(status);
        });
    }
    m_ft8Screen->setRadioState(m_tcpClient->isConnected(), qint64(m_radioState->vfoA()),
                               m_radioState->modeStringFull(), m_radioState->isTransmitting());
    m_ft8Screen->setRfPower(m_radioState->rfPower());
    if (centralWidget()) centralWidget()->hide();
    m_ft8Screen->setWaterfallAppearance(m_panadapterA->waterfallColor(), m_panadapterA->waterfallColorRange());
    m_ft8Screen->selectDialTarget(Ft8Screen::DialTarget::RxAudio);
    m_ft8Screen->setGeometry(rect());
    m_ft8Screen->show(); m_ft8Screen->raise(); m_ft8Screen->setFocus();
    setFt8PortraitEnabled(true);
    QTimer::singleShot(250, this, [this] {
        if (m_ft8Screen->isVisible()) setFt8PortraitEnabled(true);
    });
    refreshFt8Capture();
}

void MainWindow::openLogbook(bool fromSstv) {
    closeAllPopups();
    Ft8Logbook log(LogbookUi::storagePath());
    QSettings settings("QK4", "QK4");
    auto defaults = LogbookUi::contact({}, fromSstv ? "SSTV" : m_radioState->modeStringFull(),
        qint64(m_radioState->vfoA()), fromSstv ? m_sstvScreen->operatorCallsign()
                                            : settings.value("ft8/call").toString());
    defaults["MY_GRIDSQUARE"] = settings.value("ft8/grid").toString();
    if (!fromSstv) setRadioLogbookOrientationEnabled(true);
    LogbookUi::show(fromSstv ? static_cast<QWidget *>(m_sstvScreen) : this, log, defaults);
    if (!fromSstv) setRadioLogbookOrientationEnabled(false);
}
void MainWindow::openSstvScreen() {
    if (m_ft8Screen) {
        m_ft8Screen->suspend();
        m_ft8Screen->hide();
        m_ft8RadioMode.leave();
        updateFt8RadioMode();
        refreshFt8Capture();
    }
    closeAllPopups();
    if (!m_sstvScreen) {
        // SSTV is an application screen, not a console overlay. Parent it to
        // the main window so its opaque canvas covers every radio-console
        // child rather than inheriting the central console's rounded/inset
        // surface and exposing sibling widgets at the edges.
        m_sstvScreen = new SstvScreen(this);
        connect(m_sstvScreen, &SstvScreen::logbookRequested, this, [this] { openLogbook(true); });
        connect(m_sstvScreen, &SstvScreen::logQsoRequested, this,
                [this](const QString &call, bool transmit, qint64 receivedHz, const QDateTime &receivedUtc) {
            const qint64 frequency = !transmit && receivedHz > 0 ? receivedHz
                : transmit && m_radioState->splitEnabled() ? qint64(m_radioState->vfoB()) : qint64(m_radioState->vfoA());
            auto record = LogbookUi::contact(call, "SSTV", frequency, m_sstvScreen->operatorCallsign());
            if (!transmit && receivedUtc.isValid()) {
                record["QSO_DATE"] = receivedUtc.toUTC().toString("yyyyMMdd");
                record["TIME_ON"] = receivedUtc.toUTC().toString("HHmmss");
            }
            Ft8Logbook log(LogbookUi::storagePath());
            QString error;
            if (!log.load(&error)) { showInWindowMessage(m_sstvScreen, "Logbook", error); return; }
            if (LogbookUi::edit(m_sstvScreen, log, record, -1, true))
                showInWindowMessage(m_sstvScreen, "Log QSO", "Contact saved. Open Logbook to review or edit.");
        });
        connect(m_sstvScreen, &SstvScreen::closeRequested, this, [this]() {
            if (m_sstvTxActive || m_sstvTxStarting)
                stopSstvTransmission();
            setSstvOrientationEnabled(false);
            m_sstvRxArmed.store(false, std::memory_order_release);
            m_sstvScreen->hide();
            if (centralWidget())
                centralWidget()->show();
        });
        connect(m_sstvScreen, &SstvScreen::transmitRequested, this, &MainWindow::startSstvTransmit);
        connect(m_sstvScreen, &SstvScreen::stopRequested, this, &MainWindow::stopSstvTransmission);
        connect(m_sstvScreen, &SstvScreen::powerRequested, this, &MainWindow::setSstvRfPower);
        connect(m_sstvScreen, &SstvScreen::audioSetupRequested, this, [this] {
            showDigitalAudioSetup(int(DigitalTxGuard::Mode::Sstv), m_sstvScreen);
        });
        connect(m_radioState, &RadioState::rfPowerChanged, m_sstvScreen,
                [this](double watts, bool) { m_sstvScreen->setRfPower(watts); });
        const auto refreshSstvHeader = [this]() { refreshSstvRadioHeader(); };
        connect(m_radioState, &RadioState::frequencyChanged, m_sstvScreen,
                refreshSstvHeader);
        connect(m_radioState, &RadioState::frequencyBChanged, m_sstvScreen,
                refreshSstvHeader);
        connect(m_radioState, &RadioState::modeChanged, m_sstvScreen,
                refreshSstvHeader);
        connect(m_radioState, &RadioState::modeBChanged, m_sstvScreen,
                refreshSstvHeader);
        connect(m_radioState, &RadioState::dataSubModeChanged, m_sstvScreen,
                refreshSstvHeader);
        connect(m_radioState, &RadioState::dataSubModeBChanged, m_sstvScreen,
                refreshSstvHeader);
        connect(m_radioState, &RadioState::splitChanged, m_sstvScreen,
                refreshSstvHeader);

        connect(m_audioEngine, &AudioEngine::sstvPrepared, this,
                [this](bool ready, const QString &error, int totalSamples, quint64 generation) {
            Q_UNUSED(totalSamples)
            if (generation != m_sstvGeneration || !m_sstvTxStarting)
                return;
            if (!ready) {
                m_sstvTxStarting = false;
                m_sstvScreen->setTransmitting(false);
                m_sstvScreen->setTransmitStatus(error.isEmpty() ? QStringLiteral("SSTV image preparation failed.") : error);
                const bool resumeRx = m_sstvScreen && m_sstvScreen->isVisible();
                m_sstvRxArmed.store(resumeRx, std::memory_order_release);
                if (resumeRx)
                    QMetaObject::invokeMethod(m_sstvDecoder, "resetAuto", Qt::QueuedConnection);
                return;
            }
            m_sstvScreen->setTransmitStatus(QStringLiteral("KEYING K4 • PROGRAM AUDIO READY"));
            QMetaObject::invokeMethod(m_tcpClient, "beginSstvAudioTransmit", Qt::QueuedConnection,
                                      Q_ARG(quint64, generation));
        });
        connect(m_tcpClient, &TcpClient::sstvAudioKeyRequested, this, [this](quint64 generation) {
            if (generation != m_sstvGeneration || !m_sstvTxStarting)
                return;
            m_sstvScreen->setTransmitStatus(QStringLiteral("K4 KEY-UP GUARD • PROGRAM AUDIO READY"));
            qInfo() << "SSTV TX command written; holding audio for" << SstvKeyUpGuardMs << "ms"
                    << "generation" << generation;
            QTimer::singleShot(SstvKeyUpGuardMs, this, [this, generation]() {
                if (generation != m_sstvGeneration || !m_sstvTxStarting)
                    return;
                if (!m_tcpClient->isConnected()) {
                    stopSstvTransmission();
                    if (m_sstvScreen)
                        m_sstvScreen->setTransmitStatus(
                            QStringLiteral("K4 CONNECTION LOST • TRANSMISSION CANCELLED"));
                    return;
                }
                m_sstvTxStarting = false;
                m_sstvTxActive = true;
                m_sstvTxDraining = false;
                m_sstvScreen->setTransmitWarning(QString());
                const QString callsign = m_sstvScreen->operatorCallsign();
                m_sstvScreen->setTransmitting(
                    true, callsign.isEmpty()
                              ? QStringLiteral("SSTV AUDIO PREROLL")
                              : QStringLiteral("SSTV AUDIO PREROLL • %1").arg(callsign));
                qInfo() << "SSTV TX key-up guard complete; starting program audio"
                        << "generation" << generation;
                QMetaObject::invokeMethod(m_audioEngine, "beginSstvTransmit", Qt::QueuedConnection);
            });
        });
        connect(m_tcpClient, &TcpClient::sstvAudioTransmitFailed, this,
                [this](const QString &reason, quint64 generation) {
            if (generation != m_sstvGeneration || (!m_sstvTxStarting && !m_sstvTxActive))
                return;
            stopSstvTransmission();
            if (m_sstvScreen) {
                m_sstvScreen->setTransmitStatus(reason);
                showDigitalProtection(int(DigitalTxGuard::Mode::Sstv), reason, true);
            }
        });
        connect(m_audioEngine, &AudioEngine::sstvFailed, this,
                [this](const QString &reason, quint64 generation) {
            if (generation != m_sstvGeneration || (!m_sstvTxStarting && !m_sstvTxActive))
                return;
            stopSstvTransmission();
            if (m_sstvScreen) {
                m_sstvScreen->setTransmitStatus(reason);
                showDigitalProtection(int(DigitalTxGuard::Mode::Sstv), reason, true);
            }
        });
        connect(m_tcpClient, &TcpClient::sstvAudioAccepted, this,
                [this](int emittedSamples, int totalSamples, int imageSamples,
                       quint64 generation) {
            if (generation != m_sstvGeneration || !m_sstvTxActive || !m_sstvScreen)
                return;
            m_sstvScreen->setTransmitProgress(emittedSamples, totalSamples, imageSamples);
            // The encoder includes a post-roll silence interval. Once its
            // final packet reaches the K4 socket, retain PTT long enough for
            // the final SL-sized packet and remote buffer to play completely.
            if (totalSamples > 0 && emittedSamples >= totalSamples)
                beginSstvTransmitDrain();
        });
        connect(m_audioEngine, &AudioEngine::sstvFinished, this,
                [](quint64) {
            // Normal completion is acknowledged by sstvAudioAccepted above.
            // STOP and failure paths already own their lifecycle cleanup.
        });
        connect(m_tcpClient, &TcpClient::disconnected, this, [this]() {
            if (m_sstvTxActive || m_sstvTxStarting)
                stopSstvTransmission();
        });
        connect(m_radioState, &RadioState::transmitStateChanged, m_sstvScreen,
                [this](bool transmitting) {
            // A reported RX transition still stops an active transmission,
            // but an initial/stale RX state must not defeat the key-up guard.
            if (!transmitting && m_sstvTxActive) {
                stopSstvTransmission();
                if (m_sstvScreen)
                    m_sstvScreen->setTransmitStatus(
                        QStringLiteral("K4 RETURNED TO RX • SSTV TRANSMISSION STOPPED"));
            }
        });
        connect(m_sstvDecoder, &SstvDecoder::statusChanged,
                m_sstvScreen, &SstvScreen::setReceiveStatus);
        connect(m_sstvDecoder, &SstvDecoder::inputLevelChanged,
                m_sstvScreen, &SstvScreen::setReceiveInputLevel);
        connect(m_sstvDecoder, &SstvDecoder::inputStreamChanged,
                m_sstvScreen, &SstvScreen::setReceiveStreamActive);
        connect(m_sstvDecoder, &SstvDecoder::modeDetected, m_sstvScreen,
                [this](int, const QString &) {
            // Auto RX remains active while the operator prepares a TX image.
            // Bring an identified incoming image to the foreground unless
            // this station is already starting or sending SSTV.
            if (m_sstvScreen && m_sstvScreen->isVisible()
                && !m_sstvTxActive && !m_sstvTxStarting)
                m_sstvScreen->returnToAutoReceive();
        });
        connect(m_sstvDecoder, &SstvDecoder::imageUpdated,
                m_sstvScreen, &SstvScreen::setReceiveImage);
        connect(m_sstvDecoder, &SstvDecoder::imageCompleted, this,
                [this](const QImage &image, int modeId, const QString &slantStatus) {
            if (m_sstvScreen)
                m_sstvScreen->completeReceiveImage(image, modeId, slantStatus,
                                                   static_cast<qint64>(m_radioState->frequency()));
        });
        connect(m_sstvDecoder, &SstvDecoder::callsignDetected,
                m_sstvScreen, &SstvScreen::receiveCallsignDetected);
    }

    m_sstvScreen->setRfPower(m_radioState->rfPower());
    refreshSstvRadioHeader();
    m_sstvScreen->returnToAutoReceive();
    m_sstvRxArmed.store(true, std::memory_order_release);
    QMetaObject::invokeMethod(m_sstvDecoder, "resetAuto", Qt::QueuedConnection);
    // Suspend the complete radio-console widget tree while SSTV owns the app
    // surface. This prevents independently managed console children from
    // bleeding through at the sides of the SSTV canvas.
    if (centralWidget())
        centralWidget()->hide();
    m_sstvScreen->setGeometry(rect());
    m_sstvScreen->show();
    m_sstvScreen->raise();
    m_sstvScreen->setFocus();
    // Request sensor orientation after the SSTV surface is visible. Qt can
    // reapply the manifest orientation while updating its top-level window;
    // a short second request makes the Activity policy authoritative.
    setSstvOrientationEnabled(true);
    QTimer::singleShot(250, this, [this]() {
        if (m_sstvScreen && m_sstvScreen->isVisible())
            setSstvOrientationEnabled(true);
    });
}

void MainWindow::refreshSstvRadioHeader() {
    if (!m_sstvScreen || !m_radioState)
        return;
    const bool split = m_radioState->splitEnabled();
    const auto frequencyText = [this](quint64 frequency) {
        return frequency > 0 ? formatFrequency(frequency) : QString();
    };
    m_sstvScreen->setRadioOperatingState(
        frequencyText(m_radioState->vfoA()), m_radioState->modeStringFull(),
        frequencyText(split ? m_radioState->vfoB() : m_radioState->vfoA()),
        split ? m_radioState->modeStringFullB() : m_radioState->modeStringFull());
}

void MainWindow::startSstvTransmit(const QImage &frame, int modeId) {
    if (!m_sstvScreen || frame.isNull())
        return;
    if (!m_tcpClient->isConnected()) {
        m_sstvScreen->setTransmitStatus(QStringLiteral("CONNECT TO THE K4 BEFORE TRANSMITTING."));
        return;
    }
    if (m_pttActive || m_radioState->isTransmitting() || m_sstvTxActive || m_sstvTxStarting || m_digitalCalibrating || m_ft8TxGeneration) {
        m_sstvScreen->setTransmitStatus(QStringLiteral("K4 IS ALREADY TRANSMITTING."));
        return;
    }

    const quint64 generation = ++m_sstvGeneration;
    QSettings settings("QK4", "QK4");
    m_sstvTransmitContext = digitalCalibrationContext(int(DigitalTxGuard::Mode::Sstv));
    const auto calibrated = DigitalTxCalibration::load(settings, m_sstvTransmitContext);
    if (!calibrated) {
        showDigitalProtection(int(DigitalTxGuard::Mode::Sstv), "TX not started: calibrate TX audio for this radio setup first.", true);
        return;
    }
    m_tcpClient->digitalTxControl()->gain.store(qMin(*calibrated, m_digitalReducedGains.value(DigitalTxCalibration::matchingContext(m_sstvTransmitContext), *calibrated)));
    m_tcpClient->acknowledgeDigitalTxFault(); // This path follows the operator's PREVIEW & SEND action.
    showDigitalProtection(int(DigitalTxGuard::Mode::Sstv), "TX protection: preparing calibrated audio", false);
    m_sstvTxStarting = true;
    m_sstvTxActive = false;
    m_sstvTxDraining = false;
    m_sstvProtectionFault = m_sstvProtectionReduced = false;
    m_sstvRxArmed.store(false, std::memory_order_release);
    m_sstvScreen->setTransmitWarning(QString());
    m_sstvScreen->setTransmitting(true, QStringLiteral("PREPARING SSTV PROGRAM AUDIO"));
    const QString morseId = m_sstvScreen->sendCallsignCw()
                                ? m_sstvScreen->operatorCallsign() : QString();
    const QString fskId = m_sstvScreen->sendCallsignFsk()
                              ? m_sstvScreen->operatorCallsign() : QString();
    const int morseWpm = m_sstvScreen->callsignCwWpm();
    QMetaObject::invokeMethod(m_audioEngine, "prepareSstvTransmit", Qt::QueuedConnection,
                              Q_ARG(QImage, frame), Q_ARG(int, modeId),
                              Q_ARG(QString, morseId), Q_ARG(int, morseWpm),
                              Q_ARG(QString, fskId),
                              Q_ARG(quint64, generation));
}

void MainWindow::stopSstvTransmission() {
    if (!m_sstvTxActive && !m_sstvTxStarting)
        return;

    // First close the producer's atomic gate, then close the I/O gate and
    // unkey. Queued audio packets that arrive afterward are ignored by TCP.
    m_audioEngine->requestSstvStop();
    ++m_sstvGeneration;
    QMetaObject::invokeMethod(m_tcpClient, "stopSstvAudioAndUnkey", Qt::QueuedConnection);
    QMetaObject::invokeMethod(m_audioEngine, "stopSstvTransmit", Qt::QueuedConnection);
    m_sstvTxStarting = false;
    m_sstvTxActive = false;
    m_sstvTxDraining = false;
    if (m_sstvScreen) {
        m_sstvScreen->setTransmitting(false);
        if (!m_sstvProtectionFault)
            showDigitalProtection(int(DigitalTxGuard::Mode::Sstv), m_sstvProtectionReduced
                ? "TX stopped · audio drive was reduced automatically" : "TX stopped · automatic protection ready", false);
        m_sstvScreen->returnToAutoReceive();
    }
    const bool resumeRx = m_sstvScreen && m_sstvScreen->isVisible();
    m_sstvRxArmed.store(resumeRx, std::memory_order_release);
    if (resumeRx)
        QMetaObject::invokeMethod(m_sstvDecoder, "resetAuto", Qt::QueuedConnection);
}

void MainWindow::beginSstvTransmitDrain() {
    if (!m_sstvTxActive || m_sstvTxDraining)
        return;
    m_sstvTxDraining = true;
    if (m_sstvScreen)
        m_sstvScreen->setTransmitStatus(QStringLiteral("FINAL AUDIO BUFFER • HOLDING K4 IN TX"));

    const int frameSamples = streamingLatencyToFrameSamples(m_currentRadio.streamingLatency);
    const int frameDurationMs = (1000 * frameSamples + SstvEncoder::SampleRate - 1)
                                / SstvEncoder::SampleRate;
    const int drainMs = frameDurationMs + SstvDrainMarginMs;
    const quint64 generation = m_sstvGeneration;
    QTimer::singleShot(drainMs, this, [this, generation]() {
        if (generation == m_sstvGeneration && m_sstvTxActive && m_sstvTxDraining)
            finishSstvTransmission();
    });
}

void MainWindow::finishSstvTransmission() {
    ++m_sstvGeneration;
    QMetaObject::invokeMethod(m_tcpClient, "stopSstvAudioAndUnkey", Qt::QueuedConnection);
    m_sstvTxStarting = false;
    m_sstvTxActive = false;
    m_sstvTxDraining = false;
    if (m_sstvScreen) {
        m_sstvScreen->setTransmitting(false);
        showDigitalProtection(int(DigitalTxGuard::Mode::Sstv), m_sstvProtectionReduced
            ? "TX complete · audio drive was reduced automatically" : "TX complete · protection monitored ALC", false);
        m_sstvScreen->returnToAutoReceive();
    }
    const bool resumeRx = m_sstvScreen && m_sstvScreen->isVisible();
    m_sstvRxArmed.store(resumeRx, std::memory_order_release);
    if (resumeRx)
        QMetaObject::invokeMethod(m_sstvDecoder, "resetAuto", Qt::QueuedConnection);
}

void MainWindow::setSstvRfPower(double watts) {
    if (!m_tcpClient || !m_tcpClient->isConnected() || m_sstvTxActive || m_sstvTxStarting || m_digitalCalibrating || m_ft8TxGeneration)
        return;
    const double power = qBound(1.0, watts, 110.0);
    if (power <= 10.0) {
        m_tcpClient->sendCAT(QString("PC%1L;").arg(qRound(power * 10), 3, 10, QChar('0')));
    } else {
        m_tcpClient->sendCAT(QString("PC%1H;").arg(qRound(power), 3, 10, QChar('0')));
    }
    m_radioState->setRfPower(power);
}

QString MainWindow::digitalCalibrationContext(int mode) const {
    const bool split = m_radioState->splitEnabled();
    const qint64 frequency = split ? m_radioState->vfoB() : m_radioState->vfoA();
    const int tone = mode == int(DigitalTxGuard::Mode::Sstv) ? 1500
                     : m_ft8Screen ? m_ft8Screen->session().txHz : 1500;
    QJsonObject context{{"profile", m_currentRadio.host}, {"port", m_currentRadio.port},
                        {"digitalMode", mode}, {"band", Ft8::bandFor(frequency)},
                        {"radioMode", split ? m_radioState->modeStringFullB() : m_radioState->modeStringFull()},
                        {"codec", m_currentRadio.encodeMode}, {"latency", m_currentRadio.streamingLatency},
                        {"tone", tone}, {"power", m_radioState->rfPower()},
                        {"micGain", m_radioState->micGain()}, {"compression", m_radioState->compression()},
                        {"lineSource", m_radioState->lineInSource()},
                        {"lineUsb", m_radioState->lineInSoundCard()}, {"lineJack", m_radioState->lineInJack()}};
    QJsonArray eq;
    for (int band : m_radioState->txEqBands()) eq.append(band);
    context.insert("txEq", eq);
    QJsonObject firmware;
    const auto versions = m_radioState->firmwareVersions();
    for (auto it = versions.cbegin(); it != versions.cend(); ++it) firmware.insert(it.key(), it.value());
    context.insert("firmware", firmware);
    return QString::fromUtf8(QJsonDocument(context).toJson(QJsonDocument::Compact));
}
void MainWindow::showDigitalProtection(int mode, const QString &text, bool fault) {
    if (mode == int(DigitalTxGuard::Mode::Sstv)) {
        m_sstvProtectionFault = fault;
        if (text.contains("reduced automatically", Qt::CaseInsensitive)) m_sstvProtectionReduced = true;
        if (m_sstvScreen) m_sstvScreen->setTransmitProtection(text, fault);
    } else if (m_ft8Screen)
        m_ft8Screen->setTransmitProtection(text, fault);
    if (m_digitalSetupStatus)
        m_digitalSetupStatus->setText(text);
}
void MainWindow::showDigitalAudioSetup(int mode, QWidget *surface) {
    QSettings settings("QK4", "QK4");
    const auto saved = DigitalTxCalibration::load(settings, digitalCalibrationContext(mode));
    DigitalAudioSetupDialog dialog(surface, saved.has_value());
    m_digitalSetupStatus = dialog.statusLabel();
    const auto refreshReadiness = [this, &dialog] {
        const QString txMode = m_radioState->splitEnabled() ? m_radioState->modeStringFullB()
                                                           : m_radioState->modeStringFull();
        dialog.setRadioReadiness(m_tcpClient->isConnected() && !m_radioState->isTransmitting()
            && !m_pttActive && !m_sstvTxStarting && !m_sstvTxActive,
            txMode.startsWith("DATA"), m_radioState->lineInSource() >= 0);
    };
    connect(m_radioState, &RadioState::modeChanged, &dialog, refreshReadiness);
    connect(m_radioState, &RadioState::modeBChanged, &dialog, refreshReadiness);
    connect(m_radioState, &RadioState::dataSubModeChanged, &dialog, refreshReadiness);
    connect(m_radioState, &RadioState::dataSubModeBChanged, &dialog, refreshReadiness);
    connect(m_radioState, &RadioState::splitChanged, &dialog, refreshReadiness);
    connect(m_radioState, &RadioState::lineInChanged, &dialog, refreshReadiness);
    connect(m_radioState, &RadioState::transmitStateChanged, &dialog, refreshReadiness);
    connect(m_tcpClient, &TcpClient::stateChanged, &dialog, refreshReadiness);
    refreshReadiness();
    connect(&dialog, &DigitalAudioSetupDialog::calibrateRequested, &dialog, [this, mode, &dialog] {
        startDigitalCalibration(mode);
        dialog.setBusy(m_digitalCalibrating);
    });
    connect(&dialog, &DigitalAudioSetupDialog::stopRequested, &dialog, [this, &dialog] {
        cancelDigitalCalibration();
        dialog.setBusy(m_digitalCalibrating);
    });
    connect(&dialog, &DigitalAudioSetupDialog::dataModeRequested, &dialog, [this, &dialog] {
        if (!m_tcpClient->isConnected() || m_digitalCalibrating || m_radioState->isTransmitting()
            || m_pttActive || m_sstvTxStarting || m_sstvTxActive || m_ft8TxGeneration) return;
        // Same DATA-A selection as the main mode popup; this is an explicit tap.
        m_tcpClient->sendCAT(m_radioState->splitEnabled() ? "MD$6;DT$0;MD$;DT$;LI;MG;CP;TE;"
                                                        : "MD6;DT0;MD;DT;LI;MG;CP;TE;");
        dialog.findChild<QLabel *>("digitalCalibrationReadiness")->setText("DATA mode requested · waiting for K4 readback");
    });
    connect(m_tcpClient, &TcpClient::digitalCalibrationFinished, &dialog, [&dialog] { dialog.setBusy(false); });
    connect(m_audioEngine, &AudioEngine::digitalCalibrationPreparationFailed, &dialog, [&dialog] { dialog.setBusy(false); });
    m_tcpClient->sendCAT("MD;DT;MD$;DT$;LI;MG;CP;TE;"); // Read only; never set on opening.
    dialog.exec();
    cancelDigitalCalibration();
    m_digitalSetupStatus = nullptr;
    refreshFt8Capture();
}
void MainWindow::startDigitalCalibration(int mode) {
    if (!m_tcpClient->isConnected() || m_pttActive || m_radioState->isTransmitting()
        || m_sstvTxActive || m_sstvTxStarting || m_digitalCalibrating || m_ft8TxGeneration) {
        showDigitalProtection(mode, "Calibration unavailable: connect and return the K4 to receive.", true);
        return;
    }
    const QString radioMode = m_radioState->splitEnabled() ? m_radioState->modeStringFullB()
                                                          : m_radioState->modeStringFull();
    if (!radioMode.startsWith("DATA") || m_radioState->lineInSource() < 0) {
        showDigitalProtection(mode, "Select K4 DATA mode and wait for input settings before calibrating.", true);
        return;
    }
    m_digitalCalibrating = true;
    m_digitalCalibrationStarted = false;
    m_digitalCalibrationMode = mode;
    m_digitalCalibrationGeneration = ++m_sstvGeneration;
    m_digitalCalibrationContext = digitalCalibrationContext(mode);
    m_sstvRxArmed.store(false, std::memory_order_release);
    refreshFt8Capture();
    m_tcpClient->acknowledgeDigitalTxFault();
    showDigitalProtection(mode, "Preparing TX audio calibration · STOP cancels", false);
    const int tone = mode == int(DigitalTxGuard::Mode::Sstv) ? 1500 : m_ft8Screen->session().txHz;
    QMetaObject::invokeMethod(m_audioEngine, "prepareDigitalCalibration", Qt::QueuedConnection,
                              Q_ARG(int, tone), Q_ARG(quint64, m_digitalCalibrationGeneration));
}
void MainWindow::cancelDigitalCalibration() {
    if (!m_digitalCalibrating) return;
    m_audioEngine->requestSstvStop();
    QMetaObject::invokeMethod(m_audioEngine, "stopSstvTransmit", Qt::QueuedConnection);
    m_tcpClient->cancelDigitalCalibration();
    if (!m_digitalCalibrationStarted) {
        m_digitalCalibrating = false;
        showDigitalProtection(m_digitalCalibrationMode, "Calibration cancelled · previous saved level retained", false);
        refreshFt8Capture();
        if (m_sstvScreen && m_sstvScreen->isVisible()) {
            m_sstvRxArmed.store(true, std::memory_order_release);
            QMetaObject::invokeMethod(m_sstvDecoder, "resetAuto", Qt::QueuedConnection);
        }
    }
}

void MainWindow::executeMacro(const QString &functionId) {
    MacroEntry macro = RadioSettings::instance()->macro(functionId);
    if (!macro.command.isEmpty()) {
        qDebug() << "Executing macro" << functionId << ":" << macro.command;
        if (m_tcpClient && m_tcpClient->isConnected()) {
            m_tcpClient->sendMacro(macro.command);
        }
    } else {
        qDebug() << "No macro configured for" << functionId;
    }
}

void MainWindow::executeMidiMacro(const QString &command) {
    if (command.isEmpty() || !m_tcpClient || !m_tcpClient->isConnected())
        return;
    // Execute this assignment once, then refresh radio state just as Fn macros do.
    m_tcpClient->sendMacro(command);
}

void MainWindow::handleMidiKnobAction(const QString &action, int value, bool absolute) {
    if (action == QStringLiteral("disabled") || (!absolute && value == 0))
        return;
    // The visible module owns the tuning dial before console VFO dispatch.
    if (m_ft8Screen && m_ft8Screen->isVisible() &&
        m_ft8Screen->handleMidiKnobAction(action, value, absolute))
        return;
    if (action == QStringLiteral("selected_adjustment")) {
        handleMidiKnobAction(m_midiSelectedKnobAction, value, absolute);
        return;
    }

    const auto scaled = [absolute, value](int minimum, int maximum, int current, int step) {
        if (absolute)
            return minimum + qRound((maximum - minimum) * (value / 127.0));
        return qBound(minimum, current + value * step, maximum);
    };
    const bool targetB = m_radioState->bSetEnabled();

    if (action == QStringLiteral("main_volume") || action == QStringLiteral("sub_volume")) {
        const bool sub = action == QStringLiteral("sub_volume");
        const int current = sub ? RadioSettings::instance()->subVolume()
                                : RadioSettings::instance()->volume();
        const int next = scaled(0, 100, current, 1);
        if (sub) {
            RadioSettings::instance()->setSubVolume(next);
            m_audioEngine->setSubVolume(next / 100.0f);
            if (m_sideControlPanel)
                m_sideControlPanel->setSubVolume(next);
            if (m_bottomMenuBar)
                m_bottomMenuBar->setSubVolumeValue(next);
        } else {
            RadioSettings::instance()->setVolume(next);
            m_audioEngine->setMainVolume(next / 100.0f);
            if (m_sideControlPanel)
                m_sideControlPanel->setVolume(next);
            if (m_bottomMenuBar)
                m_bottomMenuBar->setMainVolumeValue(next);
            m_midiMainVolumeBeforeMute = -1;
        }
        showMidiAdjustmentSurface(action);
        showControlFeedback(QString("%1 VOLUME %2").arg(sub ? "SUB" : "MAIN").arg(next));
        return;
    }

    if (action == QStringLiteral("waterfall_brightness")) {
        m_midiWaterfallColorRange = scaled(5, 30, m_midiWaterfallColorRange, 1);
        m_panadapterA->setWaterfallColorRange(m_midiWaterfallColorRange);
        m_panadapterB->setWaterfallColorRange(m_midiWaterfallColorRange);
        m_displayPopup->setWaterfallColorRange(m_midiWaterfallColorRange);
        showControlFeedback(QString("WTR CLRS %1").arg(m_midiWaterfallColorRange));
        return;
    }

    if (!m_tcpClient || !m_tcpClient->isConnected())
        return;

    if (action == QStringLiteral("other_vfo_frequency")) {
        bool tuneB = targetB;
        tuneB = !tuneB;

        // Let the K4 apply the target VFO's current, mode-specific tuning
        // step.  This avoids guessing from a stale or not-yet-received VT/VT$
        // value and matches one physical tuning detent to one radio step.
        const QString command = value > 0
                                    ? (tuneB ? QStringLiteral("UPB;") : QStringLiteral("UP;"))
                                    : (tuneB ? QStringLiteral("DNB;") : QStringLiteral("DN;"));
        for (int index = 0; index < qMin(qAbs(value), 64); ++index)
            m_tcpClient->sendCAT(command);
        m_tcpClient->sendCAT(tuneB ? QStringLiteral("FB;") : QStringLiteral("FA;"));
        return;
    }

    if (action == QStringLiteral("active_vfo_frequency")) {
        const bool tuneB = targetB;
        const quint64 current = tuneB ? m_radioState->vfoB() : m_radioState->vfoA();
        // A physical controller follows the K4's current VT/VT$ tuning rate.
        // Do not use the phone-only digit-selection step, which can be much
        // larger than the step currently selected on the radio.
        const int step = tuneB ? tuningStepToHz(m_radioState->tuningStepB())
                               : tuningStepToHz(m_radioState->tuningStep());
        const qint64 next = static_cast<qint64>(current)
                            + static_cast<qint64>(value) * step;
        if (next > 0) {
            const QString command = QString("%1%2;")
                                        .arg(tuneB ? "FB" : "FA")
                                        .arg(next, 11, 10, QChar('0'));
            m_tcpClient->sendCAT(command);
            m_radioState->parseCATCommand(command);
        }
        return;
    }

    if (action == QStringLiteral("rit_xit_frequency")) {
        // Match the front-panel workflow: reveal the RIT/XIT controls, ensure
        // RIT is enabled deterministically, then apply the encoder movement.
        // The operator can subsequently enable XIT from the visible drawer.
        showMidiAdjustmentSurface(action);
        const bool ritEnabled = targetB ? m_radioState->ritEnabledB()
                                        : m_radioState->ritEnabled();
        if (!ritEnabled) {
            const QString enableCommand = targetB ? QStringLiteral("RT$1;")
                                                  : QStringLiteral("RT1;");
            m_tcpClient->sendCAT(enableCommand);
            m_radioState->parseCATCommand(enableCommand);
        }
        const bool adjustB = targetB && !m_radioState->xitEnabled();
        const QString command = value > 0 ? (adjustB ? QStringLiteral("RU$;") : QStringLiteral("RU;"))
                                          : (adjustB ? QStringLiteral("RD$;") : QStringLiteral("RD;"));
        for (int index = 0; index < qMin(qAbs(value), 64); ++index)
            m_tcpClient->sendCAT(command);
        return;
    }

    if (action == QStringLiteral("filter_bandwidth")) {
        int minimum = 50;
        int maximum = 5000;
        const RadioState::Mode mode = targetB ? m_radioState->modeB() : m_radioState->mode();
        const int dataMode = targetB ? m_radioState->dataSubModeB() : m_radioState->dataSubMode();
        if ((mode == RadioState::DATA || mode == RadioState::DATA_R) && dataMode == 2) {
            minimum = 150;
            maximum = 800;
        } else if ((mode == RadioState::DATA || mode == RadioState::DATA_R) && dataMode == 3) {
            maximum = 200;
        }
        const int current = targetB ? m_radioState->filterBandwidthB()
                                    : m_radioState->filterBandwidth();
        // BW is expressed in 10 Hz units by the K4. Use that finest native
        // increment so a physical dial can make precise passband changes.
        const int next = scaled(minimum, maximum, qMax(current, minimum), 10);
        const QString command = QString("BW%1%2;")
                                    .arg(targetB ? "$" : "")
                                    .arg(next / 10, 4, 10, QChar('0'));
        m_tcpClient->sendCAT(command);
        targetB ? m_radioState->setFilterBandwidthB(next)
                : m_radioState->setFilterBandwidth(next);
        showMidiAdjustmentSurface(action);
        showControlFeedback(QString("%1 FILTER %2 Hz").arg(targetB ? "B" : "A").arg(next));
        return;
    }

    if (action == QStringLiteral("filter_shift")) {
        const RadioState::Mode mode = targetB ? m_radioState->modeB() : m_radioState->mode();
        const int maximum = (mode == RadioState::CW || mode == RadioState::CW_R) ? 200 : 300;
        const int current = targetB ? m_radioState->ifShiftB() : m_radioState->ifShift();
        const int next = scaled(30, maximum, qMax(current, 30), 1);
        const QString command = QString("IS%1+%2;")
                                    .arg(targetB ? "$" : "")
                                    .arg(next, 4, 10, QChar('0'));
        m_tcpClient->sendCAT(command);
        targetB ? m_radioState->setIfShiftB(next) : m_radioState->setIfShift(next);
        showMidiAdjustmentSurface(action);
        showControlFeedback(QString("%1 SHIFT %2").arg(targetB ? "B" : "A").arg(next));
        return;
    }

    if (action == QStringLiteral("attenuator_level")) {
        const int current = targetB ? m_radioState->attenuatorLevelB()
                                    : m_radioState->attenuatorLevel();
        int next = scaled(0, 21, current, 3);
        next = qBound(0, qRound(next / 3.0) * 3, 21);
        const int direction = next >= current ? 1 : -1;
        const QString command = direction > 0 ? (targetB ? QStringLiteral("RA$+;")
                                                         : QStringLiteral("RA+;"))
                                               : (targetB ? QStringLiteral("RA$-;")
                                                          : QStringLiteral("RA-;"));
        for (int index = 0; index < qAbs(next - current) / 3; ++index)
            m_tcpClient->sendCAT(command);
        targetB ? m_radioState->setAttenuatorLevelB(next)
                : m_radioState->setAttenuatorLevel(next);
        showMidiAdjustmentSurface(action);
        return;
    }

    if (action == QStringLiteral("noise_blanker_level")) {
        const int current = targetB ? m_radioState->noiseBlankerLevelB()
                                    : m_radioState->noiseBlankerLevel();
        const int next = scaled(0, 15, current, 1);
        const int enabled = targetB ? (m_radioState->noiseBlankerEnabledB() ? 1 : 0)
                                    : (m_radioState->noiseBlankerEnabled() ? 1 : 0);
        const int filter = targetB ? m_radioState->noiseBlankerFilterWidthB()
                                   : m_radioState->noiseBlankerFilterWidth();
        const QString prefix = targetB ? QStringLiteral("NB$") : QStringLiteral("NB");
        m_tcpClient->sendCAT(QString("%1%2%3%4;")
                                 .arg(prefix)
                                 .arg(next, 2, 10, QChar('0'))
                                 .arg(enabled)
                                 .arg(filter));
        targetB ? m_radioState->setNoiseBlankerLevelB(next)
                : m_radioState->setNoiseBlankerLevel(next);
        showMidiAdjustmentSurface(action);
        return;
    }

    if (action == QStringLiteral("nr_level")) {
        const bool ssnr = targetB ? (m_radioState->ssnrEnabledB()
                                     && !m_radioState->noiseReductionEnabledB())
                                  : (m_radioState->ssnrEnabled()
                                     && !m_radioState->noiseReductionEnabled());
        const int current = ssnr ? (targetB ? m_radioState->ssnrLevelB()
                                             : m_radioState->ssnrLevel())
                                 : (targetB ? m_radioState->noiseReductionLevelB()
                                            : m_radioState->noiseReductionLevel());
        const int next = scaled(0, ssnr ? 20 : 10, current, 1);
        const bool enabled = ssnr ? (targetB ? m_radioState->ssnrEnabledB()
                                              : m_radioState->ssnrEnabled())
                                  : (targetB ? m_radioState->noiseReductionEnabledB()
                                             : m_radioState->noiseReductionEnabled());
        const QString prefix = QString("%1%2").arg(ssnr ? "NRS" : "NR", targetB ? "$" : "");
        m_tcpClient->sendCAT(QString("%1%2%3;")
                                 .arg(prefix)
                                 .arg(next, 2, 10, QChar('0'))
                                 .arg(enabled ? 1 : 0));
        if (ssnr)
            targetB ? m_radioState->setSsnrLevelB(next) : m_radioState->setSsnrLevel(next);
        else
            targetB ? m_radioState->setNoiseReductionLevelB(next)
                    : m_radioState->setNoiseReductionLevel(next);
        showMidiAdjustmentSurface(action);
        return;
    }

    if (action == QStringLiteral("manual_notch_pitch")) {
        const int current = targetB ? m_radioState->manualNotchPitchB()
                                    : m_radioState->manualNotchPitch();
        const int next = scaled(150, 5000, qMax(current, 150), 10);
        const int enabled = targetB ? (m_radioState->manualNotchEnabledB() ? 1 : 0)
                                    : (m_radioState->manualNotchEnabled() ? 1 : 0);
        const QString prefix = targetB ? QStringLiteral("NM$") : QStringLiteral("NM");
        m_tcpClient->sendCAT(QString("%1%2%3;")
                                 .arg(prefix)
                                 .arg(next, 4, 10, QChar('0'))
                                 .arg(enabled));
        targetB ? m_radioState->setManualNotchPitchB(next)
                : m_radioState->setManualNotchPitch(next);
        showMidiAdjustmentSurface(action);
        return;
    }

    if (action == QStringLiteral("main_squelch") || action == QStringLiteral("sub_squelch")) {
        const bool sub = action == QStringLiteral("sub_squelch");
        const int current = sub ? m_radioState->squelchLevelB() : m_radioState->squelchLevel();
        const int next = scaled(0, 29, current, 1);
        m_tcpClient->sendCAT(QString("SQ%1%2;")
                                 .arg(sub ? "$" : "")
                                 .arg(next, 3, 10, QChar('0')));
        sub ? m_radioState->setSquelchLevelB(next) : m_radioState->setSquelchLevel(next);
        showMidiAdjustmentSurface(action);
        return;
    }

    if (action == QStringLiteral("main_rf_gain") || action == QStringLiteral("sub_rf_gain")) {
        const bool sub = action == QStringLiteral("sub_rf_gain");
        const int current = sub ? m_radioState->rfGainB() : m_radioState->rfGain();
        // The K4 RG value is attenuation: turning the user-facing RF gain up
        // reduces the numeric value, matching the existing touch control.
        const int next = absolute ? 60 - qRound(60.0 * value / 127.0)
                                  : qBound(0, current - value, 60);
        m_tcpClient->sendCAT(QString("RG%1-%2;")
                                 .arg(sub ? "$" : "")
                                 .arg(next, 2, 10, QChar('0')));
        sub ? m_radioState->setRfGainB(next) : m_radioState->setRfGain(next);
        showMidiAdjustmentSurface(action);
        return;
    }

    if (action == QStringLiteral("rf_power")) {
        const double current = qMax(0.1, m_radioState->rfPower());
        const double next = absolute ? 0.1 + (109.9 * value / 127.0)
                                     : qBound(0.1, current + value * (current <= 10.0 ? 0.1 : 1.0), 110.0);
        if (next <= 10.0)
            m_tcpClient->sendCAT(QString("PC%1L;").arg(qRound(next * 10), 3, 10, QChar('0')));
        else
            m_tcpClient->sendCAT(QString("PC%1H;").arg(qRound(next), 3, 10, QChar('0')));
        m_radioState->setRfPower(next);
        showMidiAdjustmentSurface(action);
        showControlFeedback(QString("POWER %1 W").arg(next, 0, 'f', next <= 10.0 ? 1 : 0));
        return;
    }

    if (action == QStringLiteral("cw_speed")) {
        const int next = scaled(8, 40, qMax(8, m_radioState->keyerSpeed()), 1);
        m_tcpClient->sendCAT(QString("KS%1;").arg(next, 3, 10, QChar('0')));
        m_radioState->setKeyerSpeed(next);
        showMidiAdjustmentSurface(action);
        showControlFeedback(QString("CW SPEED %1 WPM").arg(next));
        return;
    }

    if (action == QStringLiteral("pan_zoom")) {
        int &pendingSpan = targetB ? m_midiPanZoomPendingSpanB
                                   : m_midiPanZoomPendingSpanA;
        int &generation = targetB ? m_midiPanZoomGenerationB
                                  : m_midiPanZoomGenerationA;
        quint64 &anchorFrequency = targetB ? m_midiPanZoomAnchorFrequencyB
                                           : m_midiPanZoomAnchorFrequencyA;
        int &anchorStepHz = targetB ? m_midiPanZoomAnchorStepBHz
                                    : m_midiPanZoomAnchorStepAHz;
        const int radioSpan = targetB ? m_radioState->spanHzB()
                                      : m_radioState->spanHz();
        if (pendingSpan <= 0) {
            anchorFrequency = targetB ? m_radioState->vfoB() : m_radioState->vfoA();
            anchorStepHz = tuningStepToHz(targetB ? m_radioState->tuningStepB()
                                                   : m_radioState->tuningStep());
        }
        const int current = pendingSpan > 0 ? pendingSpan
                                            : (radioSpan > 0 ? radioSpan : 50000);
        int next = current;
        for (int index = 0; index < qAbs(value); ++index)
            next = value > 0 ? getNextSpanUp(next) : getNextSpanDown(next);
        if (next != current) {
            pendingSpan = next;
            targetB ? m_radioState->setSpanHzB(next) : m_radioState->setSpanHz(next);
            showControlFeedback(QString("%1 PAN SPAN %2 kHz")
                                    .arg(targetB ? "B" : "A")
                                    .arg(next / 1000.0, 0, 'f', 1));

            const int requestedGeneration = ++generation;
            QTimer::singleShot(120, this, [this, targetB, requestedGeneration]() {
                int &activeGeneration = targetB ? m_midiPanZoomGenerationB
                                                : m_midiPanZoomGenerationA;
                if (requestedGeneration != activeGeneration)
                    return;

                int &activePendingSpan = targetB ? m_midiPanZoomPendingSpanB
                                                 : m_midiPanZoomPendingSpanA;
                quint64 &activeAnchorFrequency = targetB ? m_midiPanZoomAnchorFrequencyB
                                                         : m_midiPanZoomAnchorFrequencyA;
                int &activeAnchorStepHz = targetB ? m_midiPanZoomAnchorStepBHz
                                                  : m_midiPanZoomAnchorStepAHz;
                const int finalSpan = activePendingSpan;
                const quint64 anchorFrequency = activeAnchorFrequency;
                const int anchorStepHz = qMax(1, activeAnchorStepHz);
                activePendingSpan = 0;
                activeAnchorFrequency = 0;
                if (finalSpan <= 0 || !m_tcpClient || !m_tcpClient->isConnected())
                    return;

                // SPN is the same command used by the on-screen +/- controls.
                // Send one command when the physical turn settles instead of
                // flooding the K4 with every intermediate slider position.
                m_tcpClient->sendCAT(QString("#SPN%1%2;")
                                         .arg(targetB ? "$" : "")
                                         .arg(finalSpan));

                // Match the normal panadapter interaction: keep the frequency
                // that was under the VFO cursor when zoom began, aligned to
                // the active radio tuning rate.  This deliberately does not
                // send FC/FI (which would move the pan center) or VT (which
                // would change the operator's selected rate).
                if (anchorFrequency > 0) {
                    const quint64 snappedFrequency =
                        ((anchorFrequency + static_cast<quint64>(anchorStepHz / 2))
                         / static_cast<quint64>(anchorStepHz))
                        * static_cast<quint64>(anchorStepHz);
                    const QString frequencyCommand = QString("%1%2;")
                                                         .arg(targetB ? "FB" : "FA")
                                                         .arg(snappedFrequency, 11, 10, QChar('0'));
                    m_tcpClient->sendCAT(frequencyCommand);
                    m_radioState->parseCATCommand(frequencyCommand);
                }
            });
        }
        return;
    }

    if (action == QStringLiteral("pan_reference_level")) {
        const int rawCurrent = targetB ? m_radioState->refLevelB() : m_radioState->refLevel();
        const int current = rawCurrent < -200 ? -110 : rawCurrent;
        const int next = scaled(-140, 10, current, 1);
        m_tcpClient->sendCAT(QString("#REF%1%2;").arg(targetB ? "$" : "").arg(next));
        targetB ? m_radioState->setRefLevelB(next) : m_radioState->setRefLevel(next);
        showControlFeedback(QString("%1 PAN REF %2 dBm").arg(targetB ? "B" : "A").arg(next));
    }
}

void MainWindow::handleMidiButtonAction(const QString &action) {
    if (action == QStringLiteral("disabled"))
        return;
    if (action == QStringLiteral("adjust_ft8_rx_tx")) {
        if (!m_ft8Screen || !m_ft8Screen->isVisible())
            openFt8Screen();
        if (m_ft8Screen && m_ft8Screen->isVisible())
            m_ft8Screen->handleMidiButtonAction(action);
        return;
    }
    if (m_ft8Screen && m_ft8Screen->isVisible() && m_ft8Screen->handleMidiButtonAction(action))
        return;
    const QString selectedAction = MidiMapping::knobActionForButtonAction(action);
    if (!selectedAction.isEmpty()) {
        m_midiSelectedKnobAction = selectedAction;
        showMidiAdjustmentSurface(selectedAction);
        showControlFeedback(QString("CTR2 KNOB: %1")
                                .arg(MidiMapping::knobActionLabel(selectedAction).toUpper()));
        return;
    }
    if (action == QStringLiteral("main_mute")) {
        const int current = RadioSettings::instance()->volume();
        const int next = current > 0 ? 0 : qMax(1, m_midiMainVolumeBeforeMute);
        if (current > 0)
            m_midiMainVolumeBeforeMute = current;
        RadioSettings::instance()->setVolume(next);
        m_audioEngine->setMainVolume(next / 100.0f);
        if (m_sideControlPanel)
            m_sideControlPanel->setVolume(next);
        if (m_bottomMenuBar)
            m_bottomMenuBar->setMainVolumeValue(next);
        showControlFeedback(next == 0 ? QStringLiteral("MAIN AUDIO MUTED")
                                      : QStringLiteral("MAIN AUDIO RESTORED"));
        return;
    }
    if (action == QStringLiteral("tx_rx_toggle")) {
        if (!m_tcpClient || !m_tcpClient->isConnected())
            return;
        if (m_sstvTxActive || m_sstvTxStarting) {
            showControlFeedback("SSTV TRANSMIT ACTIVE — USE STOP SSTV");
            return;
        }
        const bool goTx = !(m_pttActive || m_radioState->isTransmitting());
        if (goTx) {
            onPttPressed();
            // Permission denial can cancel Android PTT before it becomes active.
            if (!m_pttActive)
                return;
        } else {
            onPttReleased();
        }
        showControlFeedback(goTx ? QStringLiteral("TRANSMIT ON")
                                 : QStringLiteral("RECEIVE"));
        return;
    }
    if (!m_tcpClient || !m_tcpClient->isConnected())
        return;

    const bool targetB = m_radioState->bSetEnabled();
    QString command;
    if (action == QStringLiteral("mode_next")) command = targetB ? "MD$+;" : "MD+;";
    else if (action == QStringLiteral("mode_previous")) command = targetB ? "MD$-;" : "MD-;";
    else if (action == QStringLiteral("band_up")) command = targetB ? "BN$+;" : "BN+;";
    else if (action == QStringLiteral("band_down")) command = targetB ? "BN$-;" : "BN-;";
    else if (action == QStringLiteral("nr_toggle")) command = QStringLiteral("SW62;");
    else if (action == QStringLiteral("attenuator_toggle")) command = targetB ? "RA$/;" : "RA/;";
    else if (action == QStringLiteral("noise_blanker_toggle")) command = targetB ? "NB$/;" : "NB/;";
    else if (action == QStringLiteral("manual_notch_toggle")) command = targetB ? "NM$/;" : "NM/;";
    else if (action == QStringLiteral("rit_toggle")) command = targetB ? "RT$/;" : "RT/;";
    else if (action == QStringLiteral("split_toggle")) command = QStringLiteral("FT/;");
    else if (action == QStringLiteral("tune_step")) command = targetB ? "VT$/;" : "VT/;";
    else if (action == QStringLiteral("khz")) command = targetB ? "VT$\\;" : "VT\\;";
    else if (action == QStringLiteral("tune")) command = QStringLiteral("SW16;");
    else if (action == QStringLiteral("pan_zoom_in") || action == QStringLiteral("pan_zoom_out")) {
        const int current = targetB ? m_radioState->spanHzB() : m_radioState->spanHz();
        const int safeCurrent = current > 0 ? current : 50000;
        const int next = action == QStringLiteral("pan_zoom_in")
                             ? getNextSpanDown(safeCurrent) : getNextSpanUp(safeCurrent);
        if (next != current) {
            command = QString("#SPN%1%2;").arg(targetB ? "$" : "").arg(next);
            targetB ? m_radioState->setSpanHzB(next) : m_radioState->setSpanHz(next);
            showControlFeedback(QString("%1 PAN SPAN %2 kHz")
                                    .arg(targetB ? "B" : "A")
                                    .arg(next / 1000.0, 0, 'f', 1));
        }
    }
    if (!command.isEmpty())
        m_tcpClient->sendCAT(command);
}

void MainWindow::openMacroDialog() {
    if (m_macroDialog) {
        // Close any open popups first
        closeAllPopups();

        // Size to fill the spectrum container (same as menu overlay)
        if (m_spectrumContainer) {
            QPoint pos = m_spectrumContainer->mapTo(this, QPoint(0, 0));
            m_macroDialog->setGeometry(pos.x(), pos.y(), m_spectrumContainer->width(), m_spectrumContainer->height());
        }

        m_macroDialog->show();
        m_macroDialog->raise();
        m_macroDialog->setFocus();
    }
}

// ============== MAIN RX / SUB RX Popup Slots ==============

void MainWindow::onMainRxButtonClicked(int index) {
    if (!m_tcpClient || !m_tcpClient->isConnected())
        return;

    switch (index) {
    case 0: // ANT CFG - show Main RX antenna config popup
        if (m_mainRxAntCfgPopup && m_mainRxPopup) {
            closeSecondaryPopups();
            m_mainRxAntCfgPopup->showAboveWidget(m_mainRxPopup);
        }
        break;
    case 1: // RX EQ - show graphic equalizer popup
        if (m_rxEqPopup && m_mainRxPopup) {
            // Show EQ popup above the MAIN RX popup (keep both visible)
            closeSecondaryPopups();
            m_rxEqPopup->showAboveWidget(m_mainRxPopup);
        }
        break;
    case 2: // LINE OUT - show Line Out popup
        if (m_lineOutPopup && m_mainRxPopup) {
            closeSecondaryPopups();
            m_lineOutPopup->showAboveWidget(m_mainRxPopup);
        }
        break;
    case 3: // AFX - cycle OFF → DELAY → PITCH → OFF
    {
        int nextMode = m_radioState->afxMode() == 0 ? 1 : 0;
        m_tcpClient->sendCAT(QString("FX%1;").arg(nextMode));
        break;
    }
    case 4: // AGC - toggle Fast ↔ Slow
    {
        RadioState::AGCSpeed current = m_radioState->agcSpeed();
        // Toggle between Fast (2) and Slow (1), skip Off
        int next = (current == RadioState::AGC_Fast) ? 1 : 2;
        m_tcpClient->sendCAT(QString("GT%1;").arg(next));
        break;
    }
    case 5: // APF - toggle on/off (Main RX)
        if (m_radioState->mode() == RadioState::CW || m_radioState->mode() == RadioState::CW_R)
            m_tcpClient->sendCAT("AP/;");
        break;
    case 6: // TEXT DECODE - open window directly for Main RX
        if (m_radioState->mode() != RadioState::CW && m_radioState->mode() != RadioState::CW_R
            && !((m_radioState->mode() == RadioState::DATA || m_radioState->mode() == RadioState::DATA_R)
                 && m_radioState->dataSubMode() >= 1 && m_radioState->dataSubMode() <= 3))
            break;
        if (m_textDecodeWindowMain) {
            // Set operating mode based on current radio mode
            RadioState::Mode radioMode = m_radioState->mode();
            if (radioMode == RadioState::CW || radioMode == RadioState::CW_R) {
                m_textDecodeWindowMain->setOperatingMode(TextDecodeWindow::ModeCW);
            } else if (radioMode == RadioState::DATA || radioMode == RadioState::DATA_R) {
                m_textDecodeWindowMain->setOperatingMode(TextDecodeWindow::ModeData);
            } else if (radioMode == RadioState::LSB || radioMode == RadioState::USB) {
                m_textDecodeWindowMain->setOperatingMode(TextDecodeWindow::ModeSSB);
            } else {
                m_textDecodeWindowMain->setOperatingMode(TextDecodeWindow::ModeOther);
            }
            // Show window and enable decode
            m_textDecodeWindowMain->show();
            m_textDecodeWindowMain->raise();
            m_textDecodeWindowMain->activateWindow();
            if (!m_textDecodeWindowMain->isDecodeEnabled()) {
                m_textDecodeWindowMain->setDecodeEnabled(true);
                emit m_textDecodeWindowMain->enabledChanged(true);
            }
        }
        break;
    }
}

void MainWindow::onMainRxButtonRightClicked(int index) {
    if (!m_tcpClient || !m_tcpClient->isConnected())
        return;

    switch (index) {
    case 2: // LINE OUT → VFO LINK toggle
    {
        bool linked = m_radioState->vfoLink();
        m_tcpClient->sendCAT(QString("LN%1;").arg(linked ? 0 : 1));
        break;
    }
    case 3: // AFX hold selects DELAY/PITCH
        m_tcpClient->sendCAT(QString("FX%1;").arg(m_radioState->afxMode() == 2 ? 1 : 2));
        break;
    case 4: // AGC - toggle ON/OFF
    {
        RadioState::AGCSpeed current = m_radioState->agcSpeed();
        if (current == RadioState::AGC_Off) {
            // Turn on (default to Slow)
            m_tcpClient->sendCAT("GT1;");
        } else {
            // Turn off
            m_tcpClient->sendCAT("GT0;");
        }
        break;
    }
    case 5: // APF - cycle bandwidth (Main RX)
        if (m_radioState->mode() == RadioState::CW || m_radioState->mode() == RadioState::CW_R)
            m_tcpClient->sendCAT("AP+;");
        break;
    default:
        break;
    }
}

void MainWindow::onSubRxButtonClicked(int index) {
    if (!m_tcpClient || !m_tcpClient->isConnected())
        return;

    switch (index) {
    case 0: // ANT CFG - show Sub RX antenna config popup
        if (m_subRxAntCfgPopup && m_subRxPopup) {
            closeSecondaryPopups();
            m_subRxAntCfgPopup->showAboveWidget(m_subRxPopup);
        }
        break;
    case 1: // RX EQ - show graphic equalizer popup (shares same EQ as Main RX)
        if (m_rxEqPopup && m_subRxPopup) {
            closeSecondaryPopups();
            m_rxEqPopup->showAboveWidget(m_subRxPopup);
        }
        break;
    case 2: // LINE OUT - show Line Out popup
        if (m_lineOutPopup && m_subRxPopup) {
            closeSecondaryPopups();
            m_lineOutPopup->showAboveWidget(m_subRxPopup);
        }
        break;
    case 3: // AFX - cycle (same command, affects audio)
    {
        int nextMode = m_radioState->afxMode() == 0 ? 1 : 0;
        m_tcpClient->sendCAT(QString("FX%1;").arg(nextMode));
        break;
    }
    case 4: // AGC Sub - toggle Fast ↔ Slow
    {
        RadioState::AGCSpeed current = m_radioState->agcSpeedB();
        int next = (current == RadioState::AGC_Fast) ? 1 : 2;
        m_tcpClient->sendCAT(QString("GT$%1;").arg(next));
        break;
    }
    case 5: // APF - toggle on/off (Sub RX)
        if (m_radioState->modeB() == RadioState::CW || m_radioState->modeB() == RadioState::CW_R)
            m_tcpClient->sendCAT("AP$/;");
        break;
    case 6: // TEXT DECODE - open window directly for Sub RX
        if (m_radioState->modeB() != RadioState::CW && m_radioState->modeB() != RadioState::CW_R
            && !((m_radioState->modeB() == RadioState::DATA || m_radioState->modeB() == RadioState::DATA_R)
                 && m_radioState->dataSubModeB() >= 1 && m_radioState->dataSubModeB() <= 3))
            break;
        if (m_textDecodeWindowSub) {
            // Set operating mode based on Sub RX mode
            RadioState::Mode radioMode = m_radioState->modeB();
            if (radioMode == RadioState::CW || radioMode == RadioState::CW_R) {
                m_textDecodeWindowSub->setOperatingMode(TextDecodeWindow::ModeCW);
            } else if (radioMode == RadioState::DATA || radioMode == RadioState::DATA_R) {
                m_textDecodeWindowSub->setOperatingMode(TextDecodeWindow::ModeData);
            } else if (radioMode == RadioState::LSB || radioMode == RadioState::USB) {
                m_textDecodeWindowSub->setOperatingMode(TextDecodeWindow::ModeSSB);
            } else {
                m_textDecodeWindowSub->setOperatingMode(TextDecodeWindow::ModeOther);
            }
            // Show window and enable decode
            m_textDecodeWindowSub->show();
            m_textDecodeWindowSub->raise();
            m_textDecodeWindowSub->activateWindow();
            if (!m_textDecodeWindowSub->isDecodeEnabled()) {
                m_textDecodeWindowSub->setDecodeEnabled(true);
                emit m_textDecodeWindowSub->enabledChanged(true);
            }
        }
        break;
    }
}

void MainWindow::onSubRxButtonRightClicked(int index) {
    if (!m_tcpClient || !m_tcpClient->isConnected())
        return;

    switch (index) {
    case 2: // LINE OUT → VFO LINK toggle
    {
        bool linked = m_radioState->vfoLink();
        m_tcpClient->sendCAT(QString("LN%1;").arg(linked ? 0 : 1));
        break;
    }
    case 3: // AFX hold selects DELAY/PITCH
        m_tcpClient->sendCAT(QString("FX%1;").arg(m_radioState->afxMode() == 2 ? 1 : 2));
        break;
    case 4: // AGC Sub - toggle ON/OFF
    {
        RadioState::AGCSpeed current = m_radioState->agcSpeedB();
        if (current == RadioState::AGC_Off) {
            m_tcpClient->sendCAT("GT$1;");
        } else {
            m_tcpClient->sendCAT("GT$0;");
        }
        break;
    }
    case 5: // APF - cycle bandwidth (Sub RX)
        if (m_radioState->modeB() == RadioState::CW || m_radioState->modeB() == RadioState::CW_R)
            m_tcpClient->sendCAT("AP$+;");
        break;
    default:
        break;
    }
}
