#include <QtTest>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include "ui/digitalaudiosetupdialog.h"
#include "ui/sstvscreen.h"

class DigitalUiTest : public QObject {
    Q_OBJECT
    QTemporaryDir settingsDir;
private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setApplicationName("QK4DigitalUiTests");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    }
    void calibrationFits_data() {
        QTest::addColumn<QSize>("size");
        QTest::newRow("small") << QSize(320, 568);
        QTest::newRow("device") << QSize(360, 696);
        QTest::newRow("landscape") << QSize(696, 360);
    }
    void calibrationFits() {
        QFETCH(QSize, size);
        QWidget host;
        host.resize(size);
        host.show();
        DigitalAudioSetupDialog dialog(&host, false);
        dialog.show();
        QTest::qWait(50);
        QVERIFY(host.grab().save(QString("digital-calibration-%1-ready.png").arg(QTest::currentDataTag())));
        auto *start = dialog.findChild<QPushButton *>("digitalCalibrateStart");
        auto *stop = dialog.findChild<QPushButton *>("digitalCalibrateStop");
        auto *data = dialog.findChild<QPushButton *>("digitalCalibrationData");
        auto *back = dialog.findChild<QPushButton *>("digitalCalibrationBack");
        QSignalSpy starts(&dialog, &DigitalAudioSetupDialog::calibrateRequested);
        QSignalSpy stops(&dialog, &DigitalAudioSetupDialog::stopRequested);
        QSignalSpy changes(&dialog, &DigitalAudioSetupDialog::dataModeRequested);
        QVERIFY(start && stop && data && back);
        QVERIFY(!start->isEnabled());
        QVERIFY(!data->isEnabled());
        dialog.setRadioReadiness(true, false, true);
        data->click();
        QVERIFY(!start->isEnabled());
        dialog.setRadioReadiness(true, true, false);
        QVERIFY(!start->isEnabled());
        dialog.setRadioReadiness(true, true, true);
        QVERIFY(!data->isEnabled());
        QCOMPARE(dialog.findChild<QLabel *>("digitalCalibrationReadiness")->text(),
                 QString("K4 DATA mode ready · tap Calibrate"));
        start->click();
        QCOMPARE(changes.size(), 1);
        QCOMPARE(starts.size(), 1);
        dialog.setBusy(true);
        QVERIFY(!start->isEnabled());
        QVERIFY(!data->isEnabled());
        QVERIFY(stop->isEnabled());
        stop->click();
        QCOMPARE(stops.size(), 1);
        dialog.setBusy(false);
        QVERIFY(start->isEnabled());
        dialog.setRadioReadiness(true, false, true); // TX VFO changed out of DATA.
        QVERIFY(!start->isEnabled());
        QVERIFY(data->isEnabled());
        dialog.setRadioReadiness(false, true, true); // Disconnected or externally keyed.
        QVERIFY(!start->isEnabled());
        QVERIFY(!data->isEnabled());
        dialog.statusLabel()->setText("TX stopped: ALC remains high at minimum automatic audio drive after settling and repeated readings.\nTM007000000010; · raw ALC 7 · drive -30.1 dB");
        QTest::qWait(30);
        for (auto *button : {start, stop, data, back}) {
            QVERIFY(button->height() >= 34);
            QVERIFY(dialog.contentWidget()->rect().contains(QRect(button->mapTo(dialog.contentWidget(), QPoint()), button->size())));
        }
        QVERIFY(dialog.statusLabel()->height() >= dialog.statusLabel()->heightForWidth(dialog.statusLabel()->width()));
        QVERIFY(host.grab().save(QString("digital-calibration-%1.png").arg(QTest::currentDataTag())));
        QSignalSpy closed(&dialog, &InWindowDialog::rejected);
        back->click();
        QCOMPARE(closed.size(), 1);
    }
    void sstvLogUsesPageCallsign() {
        SstvScreen screen;
        screen.resize(696, 360);
        screen.show();
        auto *rx = screen.findChild<QPushButton *>("sstvRxLogQso");
        auto *tx = screen.findChild<QPushButton *>("sstvTxLogQso");
        QVERIFY(rx && tx);
        QLineEdit *rxCall = nullptr;
        for (auto *edit : screen.findChildren<QLineEdit *>())
            if (edit->accessibleName() == "Received station callsign") rxCall = edit;
        QVERIFY(rxCall);
        rxCall->setText("K1ABC");
        QSignalSpy requested(&screen, &SstvScreen::logQsoRequested);
        rx->click();
        QCOMPARE(requested.count(), 1);
        QCOMPARE(requested[0][0].toString(), "K1ABC");
        QVERIFY(!requested[0][1].toBool());
        auto *to = screen.findChild<QLineEdit *>("sstvToCall");
        QVERIFY(to);
        to->setText("W1AW");
        tx->click();
        QCOMPARE(requested.count(), 2);
        QCOMPARE(requested[1][0].toString(), "W1AW");
        QVERIFY(requested[1][1].toBool());
    }
    void sstvFaultSurvivesReturnToReceive() {
        SstvScreen screen;
        screen.resize(360, 696);
        screen.show();
        screen.setTransmitting(true, "Streaming SSTV");
        const QString fault = "TX stopped: ALC stayed high after automatic audio-drive reduction. Check K4 input settings.";
        screen.setTransmitProtection(fault, true);
        screen.setTransmitting(false);
        screen.returnToAutoReceive();
        screen.setReceiveStatus("Waiting for SSTV");
        QTest::qWait(60);
        auto *status = screen.findChild<QLabel *>("sstvTxProtection");
        QVERIFY(status && status->isVisible());
        QCOMPARE(status->text(), fault);
        QVERIFY(screen.rect().contains(QRect(status->mapTo(&screen, QPoint()), status->size())));
        QVERIFY(status->height() >= status->heightForWidth(status->width()));
        QVERIFY(screen.grab().save("sstv-protection-receive.png"));
        auto *setup = screen.findChild<QPushButton *>("sstvAudioSetup");
        QVERIFY(setup);
        QSignalSpy requested(&screen, &SstvScreen::audioSetupRequested);
        setup->click();
        QCOMPARE(requested.size(), 1);
    }
};
QTEST_MAIN(DigitalUiTest)
#include "test_digitalui.moc"
