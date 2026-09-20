#ifndef DUALLINEPANELBUTTON_H
#define DUALLINEPANELBUTTON_H

#include "k4styles.h"
#include <QPainter>
#include <QPushButton>
#include <QStyleOptionButton>

// Touch counterpart of QK4's dual-line controls.  It retains QPushButton's
// input behavior while rendering the alternate action inside the button.
class DualLinePanelButton : public QPushButton {
public:
    explicit DualLinePanelButton(const QString &primary, const QString &alternate, QWidget *parent = nullptr)
        : QPushButton(parent), m_primary(primary), m_alternate(alternate) {}

protected:
    void paintEvent(QPaintEvent *) override {
        QStyleOptionButton option;
        initStyleOption(&option);
        option.text.clear();
        QPainter painter(this);
        style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);
        painter.setRenderHint(QPainter::TextAntialiasing);

        QFont primaryFont = font();
        primaryFont.setPixelSize(K4Styles::Dimensions::FontSizeSmall);
        primaryFont.setBold(true);
        painter.setFont(primaryFont);
        painter.setPen(QColor(K4Styles::Colors::TextWhite));
        painter.drawText(QRect(3, 1, width() - 6, height() * 3 / 5), Qt::AlignCenter, m_primary);

        QFont alternateFont = font();
        alternateFont.setPixelSize(K4Styles::Dimensions::FontSizeTiny + 1);
        alternateFont.setBold(false);
        painter.setFont(alternateFont);
        painter.setPen(QColor(K4Styles::Colors::AccentAmber));
        painter.drawText(QRect(3, height() * 3 / 5 - 1, width() - 6, height() * 2 / 5 - 2), Qt::AlignCenter,
                         m_alternate);
    }

private:
    QString m_primary;
    QString m_alternate;
};

#endif // DUALLINEPANELBUTTON_H
