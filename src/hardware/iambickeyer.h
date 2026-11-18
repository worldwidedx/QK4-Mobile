#ifndef IAMBICKEYER_H
#define IAMBICKEYER_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <atomic>
#include <deque>
#include <mutex>

// Android uses timestamped MIDI history; other platforms retain the
// upstream-derived physical-paddle latch until they have device validation.
#if defined(Q_OS_ANDROID) || defined(QK4_TEST_ANDROID_KEYER)
#define QK4_TIMESTAMPED_ANDROID_KEYER
#endif

class IambicKeyer : public QObject {
    Q_OBJECT
public:
    enum Mode { IambicA, IambicB };
    Q_ENUM(Mode)

    explicit IambicKeyer(QObject *parent = nullptr);
    Q_INVOKABLE void setEnabled(bool enabled);
    Q_INVOKABLE void setMode(Mode mode);
    Q_INVOKABLE void setReversed(bool reversed);
    Q_INVOKABLE void setSpeed(int wpm);
#ifdef QK4_TIMESTAMPED_ANDROID_KEYER
    Q_INVOKABLE void setMidiTouchLookahead(int milliseconds);
#endif
    Q_INVOKABLE void stop();
    void setDitPaddle(bool pressed);
    void setDahPaddle(bool pressed);
#ifdef QK4_TIMESTAMPED_ANDROID_KEYER
    // Session 0 Android MIDI edges arrive on the keyer thread. Their source
    // time is retained so a late batch cannot change an earlier boundary.
    void setMidiDitPaddle(bool pressed, qint64 eventTimeNs);
    void setMidiDahPaddle(bool pressed, qint64 eventTimeNs);
    void setCtr2DitPaddle(bool pressed, qint64 eventTimeNs);
    void setCtr2DahPaddle(bool pressed, qint64 eventTimeNs);
#endif

signals:
    void elementStarted(bool isDit);
    void characterSpace();
    void restartAfterPause(int ms);

private:
    enum State { Idle, PlayingDit, PlayingDah };
    void handlePaddleChange();
#ifdef QK4_TIMESTAMPED_ANDROID_KEYER
    void enterElement(bool isDit, qint64 idlePressNs = 0);
    void goIdle(qint64 boundaryNs);
    void setPaddle(bool dit, unsigned sourceBit, bool pressed, qint64 eventTimeNs);
    struct PaddleEdge { qint64 timeNs; bool dit; bool pressed; unsigned sourceBit; };
    struct PaddleSnapshot { bool dit; bool dah; bool squeezed; bool ditPressed; bool dahPressed; };
    struct PendingPress { qint64 timeNs = 0; bool dit = false; unsigned sourceBit = 0; };
    PaddleSnapshot stateAt(qint64 boundaryNs) const;
    PendingPress nextPressAfter(qint64 boundaryNs) const;
    bool releasedBetween(bool dit, qint64 afterNs, qint64 throughNs) const;
    void pruneThrough(qint64 boundaryNs);
    static qint64 nowNs();
#else
    void enterElement(bool isDit);
    void goIdle();
    bool ditDown() const;
    bool dahDown() const;
#endif
    void onTimerFired();

    QTimer *m_elementTimer = nullptr;
    State m_state = Idle;
    Mode m_mode = IambicA;
    bool m_reversed = false;
    bool m_enabled = true;
    int m_ditMs = 60;
#ifdef QK4_TIMESTAMPED_ANDROID_KEYER
    int m_midiTouchLookaheadMs = 0;
    bool m_touchAssistedElement = false;
    qint64 m_nextDeadlineNs = 0;
    qint64 m_nextBoundaryNs = 0;
    qint64 m_elementStartNs = 0;
    qint64 m_lastIdleBoundaryNs = 0;
    qint64 m_replayReadyNs = 0;
    std::atomic<unsigned> m_ditSources{0};
    std::atomic<unsigned> m_dahSources{0};
    mutable std::mutex m_edgesMutex;
    std::deque<PaddleEdge> m_edges;
    qint64 m_processedThroughNs = 0; // Guarded by m_edgesMutex.
    bool m_historyBaseDit = false;
    bool m_historyBaseDah = false;
#else
    QElapsedTimer m_clock;
    QElapsedTimer m_idleSince;
    qint64 m_nextDeadlineNs = 0;
    bool m_squeezed = false;
    std::atomic<bool> m_physDit{false};
    std::atomic<bool> m_physDah{false};
    std::atomic<bool> m_ditLatch{false};
    std::atomic<bool> m_dahLatch{false};
#endif
};

#endif
