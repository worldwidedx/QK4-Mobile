#include "ui/modepopupwidget.h"

#include <QPushButton>
#include <QSignalSpy>
#include <QtTest>

class ModePopupWidgetTest : public QObject {
    Q_OBJECT

private slots:
    void activeDataButtonTogglesNormalAndReverse();
    void changingDataSubModeStartsInNormalDirection();
    void reverseButtonAndBSetLabelsAreCorrect();

private:
    static QPushButton *buttonFor(ModePopupWidget &popup, const QString &modeType);
};

QPushButton *ModePopupWidgetTest::buttonFor(ModePopupWidget &popup,
                                            const QString &modeType) {
    const QList<QPushButton *> buttons = popup.findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (button->property("modeType").toString() == modeType)
            return button;
    }
    return nullptr;
}

void ModePopupWidgetTest::activeDataButtonTogglesNormalAndReverse() {
    const struct {
        const char *type;
        int subMode;
    } cases[] = {{"DATA", 0}, {"AFSK", 1}, {"FSK", 2}, {"PSK", 3}};

    for (const auto &test : cases) {
        ModePopupWidget popup;
        QSignalSpy spy(&popup, &ModePopupWidget::modeSelected);
        QPushButton *button = buttonFor(popup, QString::fromLatin1(test.type));
        QVERIFY(button);

        popup.setCurrentMode(6);
        popup.setCurrentDataSubMode(test.subMode);
        button->click();
        QCOMPARE(spy.takeFirst().at(0).toString(),
                 QStringLiteral("MD9;DT%1;").arg(test.subMode));

        popup.setCurrentMode(9);
        button->click();
        QCOMPARE(spy.takeFirst().at(0).toString(),
                 QStringLiteral("MD6;DT%1;").arg(test.subMode));
    }
}

void ModePopupWidgetTest::changingDataSubModeStartsInNormalDirection() {
    ModePopupWidget popup;
    QSignalSpy spy(&popup, &ModePopupWidget::modeSelected);
    popup.setCurrentMode(9);
    popup.setCurrentDataSubMode(0);
    QPushButton *fsk = buttonFor(popup, QStringLiteral("FSK"));
    QVERIFY(fsk);
    fsk->click();
    QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("MD6;DT2;"));
}

void ModePopupWidgetTest::reverseButtonAndBSetLabelsAreCorrect() {
    ModePopupWidget popup;
    popup.setCurrentMode(9);
    popup.setCurrentDataSubMode(3);
    QPushButton *psk = buttonFor(popup, QStringLiteral("PSK"));
    QVERIFY(psk);
    QCOMPARE(psk->text(), QStringLiteral("PSK-R"));

    popup.setBSetEnabled(true);
    QSignalSpy spy(&popup, &ModePopupWidget::modeSelected);
    psk->click();
    QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("MD$6;DT$3;"));
}

QTEST_MAIN(ModePopupWidgetTest)
#include "test_modepopupwidget.moc"
