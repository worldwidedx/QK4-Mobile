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
        QVERIFY(qAbs(fast.m_currentSpectrum[0] - (-40.8f)) < 0.01f);
        QVERIFY(qAbs(slow.m_currentSpectrum[0] - (-43.0f)) < 0.01f);
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
        QVERIFY(qAbs(fast.m_smoothedSpectrum[0] - 11.04f) < 0.01f);
        QVERIFY(qAbs(slow.m_smoothedSpectrum[0] - 10.60f) < 0.01f);
        QVERIFY(fast.m_smoothedSpectrum[0] > slow.m_smoothedSpectrum[0]);
    }
};

QTEST_MAIN(PanAverageTest)
#include "test_panaverage.moc"
