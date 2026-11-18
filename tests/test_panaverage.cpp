#include <QtTest>

#include "dsp/panadapter_rhi.h"
#include "dsp/minipan_rhi.h"

// Exercise the real renderers without creating a GPU surface. Both traces and
// waterfall rows consume these smoothed spectrum arrays.
class PanAverageTest : public QObject {
    Q_OBJECT
private slots:
    void mainPanFollowsRadioAverage() {
        PanadapterRhiWidget fast;
        PanadapterRhiWidget slow;
        fast.setAveraging(1);
        slow.setAveraging(20);

        const QByteArray base(64, char(100));
        const QByteArray signal(64, char(110));
        fast.updateSpectrum(base, 14074000, 0, -120);
        slow.updateSpectrum(base, 14074000, 0, -120);
        fast.updateSpectrum(signal, 14074000, 0, -120);
        slow.updateSpectrum(signal, 14074000, 0, -120);

        QCOMPARE(fast.m_currentSpectrum.size(), 64);
        QCOMPARE(slow.m_currentSpectrum.size(), 64);
        QCOMPARE(fast.m_currentSpectrum[0], -52.0f);
        QVERIFY(slow.m_currentSpectrum[0] > -62.0f);
        QVERIFY(slow.m_currentSpectrum[0] < -52.0f);
        QVERIFY(fast.m_currentSpectrum[0] > slow.m_currentSpectrum[0]);
    }

    void miniPanFollowsRadioAverage() {
        MiniPanRhiWidget fast;
        MiniPanRhiWidget slow;
        fast.setAveraging(1);
        slow.setAveraging(20);

        const QByteArray base(64, char(100));
        const QByteArray signal(64, char(120));
        fast.updateSpectrum(base);
        slow.updateSpectrum(base);
        fast.updateSpectrum(signal);
        slow.updateSpectrum(signal);

        QCOMPARE(fast.m_smoothedSpectrum.size(), 64);
        QCOMPARE(slow.m_smoothedSpectrum.size(), 64);
        QCOMPARE(fast.m_smoothedSpectrum[0], 12.0f);
        QVERIFY(slow.m_smoothedSpectrum[0] > 10.0f);
        QVERIFY(slow.m_smoothedSpectrum[0] < 12.0f);
        QVERIFY(fast.m_smoothedSpectrum[0] > slow.m_smoothedSpectrum[0]);
    }

    void averagingDropsExpiredCarrier() {
        PowerAverager average;
        average.setLength(2);
        QVector<float> output;
        average.add(QVector<float>{-100.0f}, output);
        average.add(QVector<float>{-60.0f}, output);
        QVERIFY(output[0] > -64.0f);
        average.add(QVector<float>{-100.0f}, output);
        average.add(QVector<float>{-100.0f}, output);
        QCOMPARE(output[0], -100.0f);
    }

    void dBmScaleUsesOnlyK4ReferenceAndScale() {
        PanadapterRhiWidget panadapter;
        panadapter.setRefLevel(-120);
        panadapter.setScale(80);

        QCOMPARE(panadapter.normalizeDb(-120.0f), 0.0f);
        QCOMPARE(panadapter.normalizeDb(-75.0f), 0.5625f);
        QCOMPARE(panadapter.normalizeDb(-40.0f), 1.0f);
    }
};

QTEST_MAIN(PanAverageTest)
#include "test_panaverage.moc"
