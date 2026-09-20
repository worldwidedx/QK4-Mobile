#pragma once
#include "inwindowdialog.h"
class QLabel;
class QPushButton;

class DigitalAudioSetupDialog : public InWindowDialog {
    Q_OBJECT
public:
    explicit DigitalAudioSetupDialog(QWidget *parent, bool saved);
    QLabel *statusLabel() const { return m_status; }
    void setBusy(bool busy);
    void setRadioReadiness(bool available, bool dataMode, bool inputKnown);
signals:
    void calibrateRequested();
    void stopRequested();
    void dataModeRequested();
private:
    QLabel *m_status;
    QLabel *m_readiness;
    bool m_busy = false, m_available = false, m_dataMode = false, m_inputKnown = false;
    QPushButton *m_start, *m_stop, *m_data;
};
