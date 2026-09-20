#ifndef MODEPOPUPWIDGET_H
#define MODEPOPUPWIDGET_H

#include "k4popupbase.h"
#include <QMap>
#include <QPushButton>

/**
 * @brief Mode selection popup for QK4.
 *
 * Layout (2x4 grid):
 * Row 1: CW, SSB (LSB/USB toggle), DATA, AFSK
 * Row 2: AM, FM, PSK, FSK
 *
 * Features:
 * - Shows current mode highlighted
 * - SSB button shows current state (LSB or USB) and toggles
 * - DATA sub-modes (DATA, AFSK, PSK, FSK) send MD6 + DT command
 * - B SET support: sends MD$ commands when B SET is enabled
 */
class ModePopupWidget : public K4PopupBase {
    Q_OBJECT

public:
    explicit ModePopupWidget(QWidget *parent = nullptr);

    // Update current mode display (call when mode changes)
    void setCurrentMode(int modeCode);       // MD value: 1=LSB, 2=USB, 3=CW, etc.
    void setCurrentDataSubMode(int subMode); // DT value: 0=DATA-A, 1=AFSK-A, 2=FSK-D, 3=PSK-D
    void setBSetEnabled(bool enabled);       // When true, commands target Sub RX
    void setFrequency(quint64 freqHz);       // For band-appropriate SSB default (LSB <10MHz, USB >=10MHz)

signals:
    void modeSelected(const QString &catCommand); // Emits the CAT command(s) to send

protected:
    QSize contentSize() const override;

private:
    void setupUi();
    void updateButtonStyles();
    void onModeButtonClicked();
    QPushButton *createModeButton(const QString &text);
    int bandDefaultSideband() const; // Returns MODE_LSB or MODE_USB based on frequency

    // Button pointers
    QPushButton *m_cwBtn;
    QPushButton *m_ssbBtn; // Shows LSB or USB
    QPushButton *m_dataBtn;
    QPushButton *m_afskBtn;
    QPushButton *m_amBtn;
    QPushButton *m_fmBtn;
    QPushButton *m_pskBtn;
    QPushButton *m_fskBtn;

    QMap<QString, QPushButton *> m_buttonMap;

    int m_currentMode = 2;        // Default USB
    int m_currentDataSubMode = 0; // Default DATA-A
    bool m_bSetEnabled = false;
    quint64 m_frequency = 14000000; // Default 14 MHz (USB band)
};

#endif // MODEPOPUPWIDGET_H
