#ifndef BUTTONROWPOPUP_H
#define BUTTONROWPOPUP_H

#include "k4popupbase.h"
#include <QList>
#include <QPoint>
#include <QTimer>

/**
 * RxMenuButton - Dual-line button with white primary text and amber secondary text.
 * Similar to FnMenuButton but for MAIN RX / SUB RX popup buttons.
 */
class RxMenuButton : public QWidget {
    Q_OBJECT

public:
    explicit RxMenuButton(const QString &primaryText, const QString &alternateText = QString(),
                          QWidget *parent = nullptr);

    void setPrimaryText(const QString &text);
    void setAlternateText(const QString &text);
    void setHasAlternateFunction(bool has); // If false, alternate text is white (just label), not amber
    QString primaryText() const { return m_primaryText; }
    QString alternateText() const { return m_alternateText; }
    bool hasAlternateFunction() const { return m_hasAlternateFunction; }

signals:
    void clicked();
    void rightClicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_primaryText;
    QString m_alternateText;
    bool m_hovered = false;
    bool m_hasAlternateFunction = true; // If true, alternate text is amber; if false, white
    QTimer m_longPressTimer;
    QPoint m_pressPosition;
    bool m_leftPressed = false;
    bool m_longPressHandled = false;
    bool m_pressCancelled = false;
};

/**
 * ButtonRowPopup - A single-row popup menu with 7 dual-line buttons
 *
 * Based on K4PopupBase for consistent popup behavior.
 * Used for MAIN RX, SUB RX, and TX bottom menu popups.
 * Features:
 * - 7 buttons in a horizontal row with primary/alternate text
 * - Gray bottom strip with triangle indicator pointing to trigger button
 * - Customizable button labels (primary white, alternate amber)
 */
class ButtonRowPopup : public K4PopupBase {
    Q_OBJECT

public:
    explicit ButtonRowPopup(QWidget *parent = nullptr);

    // Configure button labels (primary text only, clears alternate)
    void setButtonLabels(const QStringList &labels);

    // Configure individual button (primary and alternate text)
    // If hasAlternateFunction is false, alternate text is white (just a label, no right-click function)
    void setButtonLabel(int index, const QString &primary, const QString &alternate = QString(),
                        bool hasAlternateFunction = true);
    QString buttonLabel(int index) const;
    QString buttonAlternateLabel(int index) const;

signals:
    void buttonClicked(int index);      // Emits 0-6 for left-click
    void buttonRightClicked(int index); // Emits 0-6 for right-click

protected:
    QSize contentSize() const override;

private:
    void setupUi();

    QList<RxMenuButton *> m_buttons; // 7 buttons
};

#endif // BUTTONROWPOPUP_H
