#include "ui/frequencydisplaywidget.h"

#include <QtTest>

class TestFrequencyDisplayWidget : public QObject {
    Q_OBJECT

private slots:
    void formats_data();
    void formats();
    void phoneDigitGestures();
};

void TestFrequencyDisplayWidget::formats_data() {
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("40 metres") << QStringLiteral("7024980") << QStringLiteral("7.024.980");
    QTest::newRow("20 metres") << QStringLiteral("14074000") << QStringLiteral("14.074.000");
    QTest::newRow("2 metres") << QStringLiteral("144200000") << QStringLiteral("144.200.000");
    QTest::newRow("23 centimetres") << QStringLiteral("1296000000") << QStringLiteral("1.296.000.000");
}

void TestFrequencyDisplayWidget::formats() {
    QFETCH(QString, input);
    QFETCH(QString, expected);
    FrequencyDisplayWidget widget;
    widget.setFrequency(input);
    QCOMPARE(widget.displayText(), expected);
}

void TestFrequencyDisplayWidget::phoneDigitGestures() {
    QWidget host;
    host.resize(220, 70);
    FrequencyDisplayWidget widget(&host);
    widget.setAutoFit(true);
    widget.setTouchTuningEnabled(true);
    widget.resize(180, 34);
    widget.setFrequency("1296000000");
    widget.show();
    host.show();
    QSignalSpy digits(&widget, &FrequencyDisplayWidget::tuningDigitSelected);
    QSignalSpy entry(&widget, &FrequencyDisplayWidget::directEntryRequested);
    QSignalSpy changed(&widget, &FrequencyDisplayWidget::frequencyEntered);
    // The first GHz digit must remain reachable after shrinking to phone width.
    QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(2, 15));
    QCOMPARE(digits.size(), 1);
    QCOMPARE(digits[0][0].toInt(), 9);
    QVERIFY(!widget.isEditing());
    QVERIFY(changed.isEmpty());
    QTest::qWait(600);
    QVERIFY(entry.isEmpty());
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(2, 15));
    QTest::qWait(600);
    QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(2, 15));
    QCOMPARE(entry.size(), 1);
    entry.clear();
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(2, 15));
    QTest::mouseMove(&widget, QPoint(60, 15));
    QTest::qWait(600);
    QTest::mouseRelease(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(60, 15));
    QVERIFY(entry.isEmpty());
    QTest::mousePress(&widget, Qt::LeftButton, Qt::NoModifier, QPoint(2, 15));
    widget.hide();
    QTest::qWait(600);
    QVERIFY(entry.isEmpty());
    QVERIFY(changed.isEmpty());
}

QTEST_MAIN(TestFrequencyDisplayWidget)
#include "test_frequencydisplaywidget.moc"
