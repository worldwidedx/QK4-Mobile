#ifndef BANDPOPUPWIDGET_H
#define BANDPOPUPWIDGET_H

#include "k4popupbase.h"
#include <QList>
#include <QMap>
#include <QPushButton>

/**
 * @brief Band selection popup with amateur and shortwave-listening band banks.
 *
 * Layout:
 * Row 1: 1.8, 3.5, 7, 14, 21, 28, MEM
 * Row 2: GEN, 5, 10, 18, 24, 50, XVTR
 *
 * GEN is a mobile-only view switch. It retains the GEN control as the white
 * return toggle and replaces the amateur grid with international broadcast
 * bands. Selecting an SWL band provides its center frequency to MainWindow.
 */
class BandPopupWidget : public K4PopupBase {
    Q_OBJECT

public:
    explicit BandPopupWidget(QWidget *parent = nullptr);

    // Set the currently selected band by name
    void setSelectedBand(const QString &bandName);
    QString selectedBand() const { return m_selectedBand; }

    // Band number methods for K4 BN command
    void setSelectedBandByNumber(int bandNum);
    int getBandNumber(const QString &bandName) const;
    QString getBandName(int bandNum) const;

    bool isShortwaveBand(const QString &bandName) const;
    quint64 shortwaveBandTuneHz(const QString &bandName) const;
    bool shortwaveBandsActive() const { return m_shortwaveBandsActive; }

    // Track the VFO currently using a selected SWL band. Frequency changes are
    // remembered only for this mobile GEN bank, never for regular K4 bands.
    void setActiveShortwaveBand(bool vfoB, const QString &bandName);
    void clearActiveShortwaveBand(bool vfoB);
    void rememberVfoFrequency(bool vfoB, quint64 frequencyHz);

signals:
    void bandSelected(const QString &bandName);
    void bandBankChanged();

protected:
    QSize contentSize() const override;

private:
    void setupUi();
    void rebuildBandGrid();
    QPushButton *createBandButton(const QString &text);
    void updateButtonStyles();
    void onBandButtonClicked();

    class QGridLayout *m_bandGrid = nullptr;
    QMap<QString, QPushButton *> m_buttonMap;

    QString m_selectedBand;
    enum Bank { AmateurBank, ShortwaveBank, TransverterBank };
    Bank m_bank = AmateurBank;
    bool m_shortwaveBandsActive = false;
    QString m_activeShortwaveBandA;
    QString m_activeShortwaveBandB;
};

#endif // BANDPOPUPWIDGET_H
