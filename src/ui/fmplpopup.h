#ifndef FMPLPOPUP_H
#define FMPLPOPUP_H

#include "ui/k4popupbase.h"

class QLabel;
class QPushButton;

class FmPlPopupWidget : public K4PopupBase {
    Q_OBJECT
public:
    explicit FmPlPopupWidget(QWidget *parent = nullptr);
    void setTone(int index, bool enabled);

signals:
    void toneChanged(int index, bool enabled);

protected:
    QSize contentSize() const override;

private:
    void adjustTone(int delta);
    void updateDisplay();
    int m_index = 1;
    bool m_enabled = false;
    QLabel *m_toneLabel = nullptr;
    QPushButton *m_enableButton = nullptr;
};

#endif
