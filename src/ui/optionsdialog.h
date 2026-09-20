#ifndef OPTIONSDIALOG_H
#define OPTIONSDIALOG_H

#include <QDialog>
#include <QWidget>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QSlider>
#include <QPushButton>
#include <QShowEvent>
#include <QHideEvent>
#include <QMap>
#include <QPoint>

class RadioState;
class AudioEngine;
class MicMeterWidget;
class KpodDevice;
class CatServer;
class HalikeyDevice;
class Ctr2MidiDevice;
class Ctr2MappingEditor;
class QScrollArea;

#ifdef Q_OS_ANDROID
using OptionsDialogBase = QWidget;
#else
using OptionsDialogBase = QDialog;
#endif

class OptionsDialog : public OptionsDialogBase {
    Q_OBJECT

public:
    enum Page {
        PageAbout = 0,
        PageAudioInput,
        PageAudioOutput,
        PageRigControl,
        PageCwKeyer,
        PageCtr2Midi,
        PageKpod,
        PageFnKeySetup,
        PageCount
    };

    explicit OptionsDialog(RadioState *radioState, AudioEngine *audioEngine, KpodDevice *kpodDevice,
                           CatServer *catServer, HalikeyDevice *halikeyDevice,
                           Ctr2MidiDevice *ctr2MidiDevice, QWidget *parent = nullptr);
    ~OptionsDialog();

signals:
    void keyerSpeedRequested(int wpm);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onMicTestToggled(bool checked);
    void onMicLevelChanged(float level);
    void onMicDeviceChanged(int index);
    void onMicGainChanged(int value);
    void updateKpodStatus();
    void updateCwKeyerStatus();
    void onCwKeyerConnectClicked();
    void onCwKeyerRefreshClicked();

private:
    void setupUi();
    void ensurePageCreated(int index);
    void refreshCurrentPage();
    void refreshPage(int index);
    QWidget *createAboutPage();
    QWidget *createKpodPage();
    QWidget *createAudioInputPage();
    QWidget *createAudioOutputPage();
    QWidget *createRigControlPage();
    QWidget *createCwKeyerPage();
    QWidget *createCtr2MidiPage();
    QWidget *createFnKeySetupPage();
    void requestReturnToOperate();
    void updateCatServerStatus();
    void populateMicDevices();
    void populateSpeakerDevices();
    void populateCwKeyerPorts();
    void setTouchSliderValue(QSlider *slider, int xPosition);
    void saveFnKeyEditor(const QString &functionId);
    void persistFnKeyEditors();
    void refreshFnKeyEditors();
    void loadFnKeyFile();
    bool saveFnKeyFile();

    RadioState *m_radioState;
    AudioEngine *m_audioEngine;
    KpodDevice *m_kpodDevice;
    CatServer *m_catServer;
    HalikeyDevice *m_halikeyDevice;
    Ctr2MidiDevice *m_ctr2MidiDevice;
    Ctr2MappingEditor *m_ctr2MappingEditor = nullptr;
    QListWidget *m_tabList;
    QStackedWidget *m_pageStack;
    bool m_pageCreated[PageCount] = {};

    // KPOD page elements (for real-time updates)
    QCheckBox *m_kpodEnableCheckbox;
    QLabel *m_kpodStatusLabel;
    QLabel *m_kpodProductLabel;
    QLabel *m_kpodManufacturerLabel;
    QLabel *m_kpodVendorIdLabel;
    QLabel *m_kpodProductIdLabel;
    QLabel *m_kpodDeviceTypeLabel;
    QLabel *m_kpodFirmwareLabel;
    QLabel *m_kpodDeviceIdLabel;
    QLabel *m_kpodHelpLabel;

    // Audio Input settings
    QComboBox *m_micDeviceCombo;
    QSlider *m_micGainSlider;
    QLabel *m_micGainValueLabel;
    QPushButton *m_micTestBtn;
    MicMeterWidget *m_micMeter;
    bool m_micTestActive = false;

    // Audio Output settings
    QComboBox *m_speakerDeviceCombo;

    // CAT Server page elements
    QCheckBox *m_catServerEnableCheckbox;
    QLineEdit *m_catServerPortEdit;
    QLabel *m_catServerStatusLabel;
    QLabel *m_catServerClientsLabel;

    void onSpeakerDeviceChanged(int index);

    // CW Keyer page elements
    QComboBox *m_cwKeyerDeviceTypeCombo = nullptr;
    QLabel *m_cwKeyerDescLabel = nullptr;
    QComboBox *m_cwKeyerPortCombo;
    QPushButton *m_cwKeyerRefreshBtn;
    QPushButton *m_cwKeyerConnectBtn;
    QLabel *m_cwKeyerStatusLabel;
    QSlider *m_cwSpeedSlider = nullptr;
    QLabel *m_cwSpeedValueLabel = nullptr;
    QSlider *m_sidetoneVolumeSlider = nullptr;
    QLabel *m_sidetoneVolumeValueLabel = nullptr;
    QLabel *m_paddleMappingLabel = nullptr;
    QLabel *m_midiMappingLabel = nullptr;
    QComboBox *m_midiMappingProfileCombo = nullptr;
    QPushButton *m_midiLearnDitButton = nullptr;
    QPushButton *m_midiLearnDahButton = nullptr;
    int m_midiLearningTarget = 0; // 0=none, 1=dit, 2=dah
    QSlider *m_touchSlider = nullptr;
    QScrollArea *m_cwControlsScroll = nullptr;
    QObject *m_cwScrollTouchTarget = nullptr;
    int m_cwScrollPressY = 0;
    int m_cwScrollStartValue = 0;
    void updateCwKeyerDescription();

    struct FnKeyEditors {
        QLineEdit *label = nullptr;
        QLineEdit *command = nullptr;
    };
    QMap<QString, FnKeyEditors> m_fnKeyEditors;
    QLabel *m_fnKeySaveStatus = nullptr;
    QString m_fnKeyMemoryName = QStringLiteral("QK4 FN Key Setup");
    bool m_loadingFnKeyFile = false;
};

#endif // OPTIONSDIALOG_H
