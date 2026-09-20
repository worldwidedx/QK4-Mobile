#ifndef KPA1500WINDOW_H
#define KPA1500WINDOW_H

#include <QWidget>
#include <QPoint>

class KPA1500Panel;
class QPushButton;

/**
 * KPA1500Window - Floating window container for KPA1500 amplifier panel.
 *
 * Features:
 * - Custom dark title bar with "KPA1500" label and close button
 * - Draggable by title bar
 * - Always on top of main window
 * - Position saved/restored via RadioSettings
 * - Close button hides window (doesn't disconnect amp)
 */
class KPA1500Window : public QWidget {
    Q_OBJECT

public:
    explicit KPA1500Window(QWidget *parent = nullptr);

    KPA1500Panel *panel() const { return m_panel; }

signals:
    void closeRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void setupUi();
    QRect titleBarRect() const;
    void savePosition();
    void restorePosition();

    KPA1500Panel *m_panel = nullptr;
    QPushButton *m_closeBtn = nullptr;

    // Dragging state
    bool m_dragging = false;
    QPoint m_dragPosition;
};

#endif // KPA1500WINDOW_H
