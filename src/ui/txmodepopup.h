#ifndef TXMODEPOPUP_H
#define TXMODEPOPUP_H

#include "ui/k4popupbase.h"

class QLabel;
class QPushButton;

class TxModePopupWidget : public K4PopupBase {
    Q_OBJECT
public:
    enum Editor { DataBandwidth, FmRepeater };
    explicit TxModePopupWidget(QWidget *parent = nullptr);
    void showDataBandwidth(int tenthsKhz);
    void showFmRepeater(QChar mode, int offsetKhz, bool editOffset);

signals:
    void dataBandwidthChanged(int tenthsKhz);
    void repeaterChanged(QChar mode, int offsetKhz);

protected:
    QSize contentSize() const override;

private:
    void adjust(int delta);
    void updateDisplay();
    Editor m_editor = DataBandwidth;
    int m_value = 28;
    QChar m_repeaterMode = 'S';
    bool m_editOffset = false;
    QLabel *m_title = nullptr;
    QLabel *m_valueLabel = nullptr;
    QPushButton *m_modeButton = nullptr;
};

#endif
