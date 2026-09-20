#ifndef IAMBICKEYER_H
#define IAMBICKEYER_H

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <atomic>

// Element-timed iambic A/B keyer adapted from current upstream QK4. Physical
// paddle edges are atomic so brief opposite-paddle taps survive until the next
// legal element boundary.
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
    Q_INVOKABLE void stop();
    void setDitPaddle(bool pressed);
    void setDahPaddle(bool pressed);

signals:
    void elementStarted(bool isDit);
    void characterSpace();
    void restartAfterPause(int ms);

private:
    enum State { Idle, PlayingDit, PlayingDah };
    void handlePaddleChange();
    void enterElement(bool isDit);
    void onTimerFired();
    void goIdle();
    bool ditDown() const;
    bool dahDown() const;

    QTimer *m_elementTimer = nullptr;
    State m_state = Idle;
    Mode m_mode = IambicA;
    bool m_reversed = false;
    bool m_squeezed = false;
    bool m_enabled = true;
    int m_ditMs = 60;
    QElapsedTimer m_idleSince;
    QElapsedTimer m_clock;
    qint64 m_nextDeadlineNs = 0;
    std::atomic<bool> m_physDit{false};
    std::atomic<bool> m_physDah{false};
    std::atomic<bool> m_ditLatch{false};
    std::atomic<bool> m_dahLatch{false};
};

#endif
