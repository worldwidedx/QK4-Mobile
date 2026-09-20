#ifndef BOTTOMMENUBAR_H
#define BOTTOMMENUBAR_H

#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QWidget>

/**
 * BottomMenuBar - Horizontal menu bar at bottom of QK4
 *
 * Contains 7 menu buttons: MENU, Fn, DISPLAY, BAND, MAIN RX, SUB RX, TX
 * Plus PTT button at far right with stretch separator.
 * (Icon buttons moved to SideControlPanel)
 *
 * Each menu button triggers a popup menu or panel when pressed.
 * PTT uses press/release for momentary microphone activation.
 * Styled with subtle rounded edges, gradient background, white border.
 */
class BottomMenuBar : public QWidget {
    Q_OBJECT

public:
    explicit BottomMenuBar(QWidget *parent = nullptr);
    ~BottomMenuBar() = default;

    // Getters for button positioning (for popup placement)
    QPushButton *bandButton() const { return m_bandBtn; }
    QPushButton *displayButton() const { return m_displayBtn; }
    QPushButton *fnButton() const { return m_fnBtn; }
    QPushButton *mainRxButton() const { return m_mainRxBtn; }
    QPushButton *subRxButton() const { return m_subRxBtn; }
    QPushButton *txButton() const { return m_txBtn; }
    QPushButton *pttButton() const { return m_pttBtn; }

public slots:
    void setMenuActive(bool active);    // Toggle MENU button inverse colors
    void setDisplayActive(bool active); // Toggle DISPLAY button inverse colors
    void setBandActive(bool active);    // Toggle BAND button inverse colors
    void setFnActive(bool active);      // Toggle Fn button inverse colors
    void setMainRxActive(bool active);  // Toggle MAIN RX button inverse colors
    void setSubRxActive(bool active);   // Toggle SUB RX button inverse colors
    void setTxActive(bool active);      // Toggle TX button inverse colors
    void setPttActive(bool active);     // Toggle PTT button inverse colors
    void setMainVolumeValue(int value);
    void setSubVolumeValue(int value);
    void setTuneStepA(int hertz);
    void setTuneStepB(int hertz);

signals:
    void menuClicked();
    void connectClicked();
    void fnClicked();
    void displayClicked();
    void bandClicked();
    void mainRxClicked();
    void subRxClicked();
    void txClicked();
    void pttPressed();  // PTT button pressed (start TX audio)
    void pttReleased(); // PTT button released (stop TX audio)
    void tuneARequested(int steps);
    void tuneBRequested(int steps);
    void frequencyARequested();
    void frequencyBRequested();
    void controlsRequested();
    void settingsRequested();
    void mainVolumeChanged(int value);
    void subVolumeChanged(int value);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUi();
    QPushButton *createMenuButton(const QString &text);

    // Menu buttons
    QPushButton *m_menuBtn;
    QPushButton *m_connectBtn = nullptr;
    QPushButton *m_frequencyABtn = nullptr;
    QPushButton *m_frequencyBBtn = nullptr;
    QPushButton *m_controlsBtn = nullptr;
    QPushButton *m_settingsBtn = nullptr;
    QPushButton *m_fnBtn;
    QPushButton *m_displayBtn;
    QPushButton *m_bandBtn;
    QPushButton *m_mainRxBtn;
    QPushButton *m_subRxBtn;
    QPushButton *m_txBtn;
    QPushButton *m_pttBtn;
    QPushButton *m_tuneADownBtn = nullptr;
    QPushButton *m_tuneAUpBtn = nullptr;
    QPushButton *m_tuneBDownBtn = nullptr;
    QPushButton *m_tuneBUpBtn = nullptr;
    QSlider *m_mainVolumeSlider = nullptr;
    QSlider *m_subVolumeSlider = nullptr;
    int m_tuneStepAHz = -1;
    int m_tuneStepBHz = -1;

    bool m_pttLocked = false;
    QTimer *m_pttLockTimer = nullptr;
};

#endif // BOTTOMMENUBAR_H
