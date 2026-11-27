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
        QCOMPARE(fast.m_waterfallSpectrum, fast.m_currentSpectrum);
        // Equal numbers of -62 and -52 dBm frames averaged in linear power.
        QVERIFY(qAbs(slow.m_waterfallSpectrum[0] - (-54.5964f)) < 0.001f);
        QCOMPARE(slow.m_currentSpectrum[0], (-62.0f + slow.m_waterfallSpectrum[0]) / 2.0f);
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
        QVERIFY(qAbs(fast.m_smoothedSpectrum[0] - 11.04f) < 0.01f);
        QVERIFY(qAbs(slow.m_smoothedSpectrum[0] - 10.60f) < 0.01f);
        QVERIFY(fast.m_smoothedSpectrum[0] > slow.m_smoothedSpectrum[0]);
    }

    void avgChangeDiscardsOnlyPendingWaterfallRow() {
        PanadapterRhiWidget panadapter;
        const QByteArray base(64, char(100));
        const QByteArray signal(64, char(110));
        panadapter.updateSpectrum(base, 14074000, 0, -120);
        // Simulate already displayed history without opening a GPU surface.
        panadapter.m_waterfallWriteRow = 1;
        panadapter.m_waterfallData = {42};
        QVERIFY(panadapter.m_waterfallNeedsUpdate);

        panadapter.setAveraging(8);
        QVERIFY(!panadapter.m_waterfallNeedsUpdate);
        QVERIFY(panadapter.m_waterfallSpectrum.isEmpty());
        QCOMPARE(panadapter.m_waterfallWriteRow, 1);
        QCOMPARE(panadapter.m_waterfallData[0], quint8(42));

        panadapter.updateSpectrum(signal, 14074000, 0, -120);
        QVERIFY(panadapter.m_waterfallNeedsUpdate);
        QCOMPARE(panadapter.m_waterfallSpectrum[0], -52.0f);
        QCOMPARE(panadapter.m_currentSpectrum, panadapter.m_waterfallSpectrum);
    }

    void averagingRestartsAfterPassThroughBinCountChanges() {
        PowerAverager average;
        QVector<float> output;
        average.setLength(2);
        average.add(QVector<float>{-70.0f}, output);

        average.setLength(1);
        const QVector<float> widerFrame{-60.0f, -70.0f, -80.0f};
        average.add(widerFrame, output);
        QCOMPARE(output, widerFrame);

        average.setLength(2);
        const QVector<float> freshFrame{-90.0f, -100.0f, -110.0f};
        average.add(freshFrame, output);
        QCOMPARE(output, freshFrame);
        average.add(widerFrame, output);
        QCOMPARE(output.size(), widerFrame.size());
        for (qsizetype i = 0; i < output.size(); ++i)
            QVERIFY(qAbs(output[i] - (widerFrame[i] - 3.00596f)) < 0.001f);
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
