#include <QtTest>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include "ui/overlaybackhandler.h"
#include "ui/inwindowdialog.h"

class RadioWindow : public QWidget {
public:
    int backEvents = 0;
    void keyPressEvent(QKeyEvent *event) override { ++backEvents; event->ignore(); }
    void keyReleaseEvent(QKeyEvent *event) override { ++backEvents; event->ignore(); }
};

class OverlayBackTest : public QObject {
    Q_OBJECT
private slots:
    void focusedControl_data() {
        QTest::addColumn<int>("kind");
        QTest::addColumn<int>("key");
        for (int kind = 0; kind < 4; ++kind)
            for (int key : {int(Qt::Key_Back), int(Qt::Key_Escape)})
                QTest::newRow(qPrintable(QString("%1-%2").arg(kind).arg(key))) << kind << key;
    }
    void focusedControl() {
        QFETCH(int, kind); QFETCH(int, key);
        RadioWindow radio;
        QWidget setup(&radio);
        setup.hide();
        new OverlayBackHandler(&setup);
        QWidget *control = kind == 0 ? static_cast<QWidget *>(new QLineEdit(&setup))
                         : kind == 1 ? static_cast<QWidget *>(new QListWidget(&setup))
                         : kind == 2 ? static_cast<QWidget *>(new QComboBox(&setup))
                                     : static_cast<QWidget *>(new QPushButton(&setup));
        radio.show(); setup.show(); control->setFocus();
        QTest::keyPress(control, Qt::Key(key));
        QVERIFY(setup.isVisible()); // Closing on press lets release escape to Android.
        QTest::keyRelease(control, Qt::Key(key));
        QVERIFY(setup.isHidden());
        QCOMPARE(radio.backEvents, 0);
        QTest::keyClick(&radio, Qt::Key_Back);
        QCOMPARE(radio.backEvents, 2); // Hidden setup must not change normal radio Back.
        setup.show();
        QTest::keyClick(control, Qt::Key(key));
        QVERIFY(setup.isHidden()); // Registration survives reopening.
    }
    void nestedOverlay() {
        RadioWindow radio;
        QWidget setup(&radio), selector(&setup);
        setup.hide(); selector.hide();
        new OverlayBackHandler(&setup);
        new OverlayBackHandler(&selector);
        radio.show(); setup.show(); selector.show();
        QTest::keyClick(&radio, Qt::Key_Back); // Android may deliver to the shared window.
        QVERIFY(selector.isHidden()); QVERIFY(setup.isVisible());
        QCOMPARE(radio.backEvents, 0);
        QTest::keyClick(&radio, Qt::Key_Back);
        QVERIFY(setup.isHidden()); QCOMPARE(radio.backEvents, 0);
    }
    void unsavedChangesSheet() {
        RadioWindow radio;
        QWidget setup(&radio);
        setup.hide();
        bool reviewed = false;
        new OverlayBackHandler(&setup, [&] {
            InWindowDialog confirm(&setup);
            QTimer::singleShot(20, &confirm, [&] {
                QVERIFY(confirm.isVisible());
                QTest::keyClick(&radio, Qt::Key_Back);
                reviewed = true;
            });
            QCOMPARE(confirm.exec(), int(InWindowDialog::Rejected));
        });
        radio.show(); setup.show();
        QTest::keyClick(&radio, Qt::Key_Back);
        QVERIFY(reviewed); QVERIFY(setup.isVisible());
        QCOMPARE(radio.backEvents, 0);
    }
    void nativeWindowIsSeparate() {
        RadioWindow radio, nativePicker;
        QWidget setup(&radio);
        setup.hide(); new OverlayBackHandler(&setup);
        radio.show(); setup.show(); nativePicker.show();
        QTest::keyClick(&nativePicker, Qt::Key_Back);
        QCOMPARE(nativePicker.backEvents, 2);
        QVERIFY(setup.isVisible());
    }
};
QTEST_MAIN(OverlayBackTest)
#include "test_overlayback.moc"
