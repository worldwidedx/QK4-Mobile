#include "digitalaudiosetupdialog.h"
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

DigitalAudioSetupDialog::DigitalAudioSetupDialog(QWidget *parent, bool saved) : InWindowDialog(parent) {
    setPanelSize(QSize(qMin(420, parent->width() - 24), qMin(420, parent->height() - 24)));
    auto *panel = contentWidget();
    panel->setStyleSheet("QLabel {color:#e4edf3;} QPushButton {color:#e4edf3;background:#263641;"
                        "border:1px solid #50616e;border-radius:4px;padding:4px;}"
                        "QPushButton:disabled {color:#71808a;} QScrollArea {border:0;}");
    auto *layout = new QVBoxLayout(panel);
    auto *title = new QLabel("TX audio calibration & protection", panel);
    title->setWordWrap(true);
    title->setStyleSheet("font-size:16px;font-weight:700;color:#6dd4ef;");
    layout->addWidget(title);
    auto *scroll = new QScrollArea(panel);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *information = new QLabel(
        "Calibration uses the K4's TEST mode: no RF output from the K4, but connected amplifiers or other equipment may still key. "
        "The app verifies TEST mode, streams a low-level tone, measures ALC, then restores the previous TEST setting.\n\n"
        "FT8 and FT4 share one calibration for this radio and audio input setup. SSTV keeps its own level. Changing stations, audio frequency, "
        "band or RF power keeps that level. Automatic overdrive protection remains on. You can stop at any time.", scroll);
    information->setWordWrap(true);
    information->setStyleSheet("background:#1b272f;color:#e4edf3;");
    information->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    information->setTextFormat(Qt::PlainText);
    information->setMargin(5);
    scroll->setWidget(information);
    layout->addWidget(scroll, 1);
    m_readiness = new QLabel(panel);
    m_readiness->setObjectName("digitalCalibrationReadiness");
    m_readiness->setWordWrap(true);
    m_readiness->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(m_readiness);
    m_status = new QLabel(saved ? "Saved calibration available for this setup · protection on"
                               : "Calibration required for this setup · protection on", panel);
    m_status->setObjectName("digitalCalibrationStatus");
    m_status->setWordWrap(true);
    m_status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(m_status);
    m_data = new QPushButton("Use K4 DATA mode", panel);
    m_data->setObjectName("digitalCalibrationData");
    m_data->setMinimumHeight(34);
    layout->addWidget(m_data);
    connect(m_data, &QPushButton::clicked, this, &DigitalAudioSetupDialog::dataModeRequested);
    auto *row = new QHBoxLayout;
    m_start = new QPushButton("Calibrate TX audio", panel);
    m_stop = new QPushButton("STOP", panel);
    m_start->setObjectName("digitalCalibrateStart");
    m_stop->setObjectName("digitalCalibrateStop");
    m_stop->setStyleSheet("color:#ffffff;background:#7c272b;");
    for (auto *button : {m_start, m_stop}) button->setMinimumHeight(40);
    row->addWidget(m_start, 1);
    row->addWidget(m_stop);
    layout->addLayout(row);
    connect(m_start, &QPushButton::clicked, this, &DigitalAudioSetupDialog::calibrateRequested);
    connect(m_stop, &QPushButton::clicked, this, &DigitalAudioSetupDialog::stopRequested);
    auto *back = new QPushButton("Back", panel);
    back->setObjectName("digitalCalibrationBack");
    back->setMinimumHeight(34);
    layout->addWidget(back);
    connect(back, &QPushButton::clicked, this, &InWindowDialog::reject);
    setBusy(false);
    setRadioReadiness(false, false, false);
}
void DigitalAudioSetupDialog::setBusy(bool busy) {
    m_busy = busy;
    m_readiness->setVisible(!busy);
    m_start->setEnabled(!busy && m_available && m_dataMode && m_inputKnown);
    m_data->setEnabled(!busy && m_available && !m_dataMode);
    m_stop->setEnabled(busy);
}
void DigitalAudioSetupDialog::setRadioReadiness(bool available, bool dataMode, bool inputKnown) {
    m_available = available;
    m_dataMode = dataMode;
    m_inputKnown = inputKnown;
    m_readiness->setText(!available ? "Connect the K4 and return to receive to calibrate"
        : !dataMode ? "Select K4 DATA mode to calibrate"
        : !inputKnown ? "K4 DATA mode active · waiting for input settings"
        : "K4 DATA mode ready · tap Calibrate");
    m_readiness->setVisible(!m_busy);
    setBusy(m_busy);
}
