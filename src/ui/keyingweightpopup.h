#ifndef KEYINGWEIGHTPOPUP_H
#define KEYINGWEIGHTPOPUP_H

#include "ui/k4popupbase.h"

class QLabel;
class QPushButton;

class KeyingWeightPopupWidget : public K4PopupBase {
    Q_OBJECT
public:
    explicit KeyingWeightPopupWidget(QWidget *parent = nullptr);
    void setWeight(int weight);

signals:
    void weightChanged(int weight);

protected:
    QSize contentSize() const override;

private:
    void adjustWeight(int delta);
    void updateValue();

    int m_weight = 100;
    QLabel *m_valueLabel = nullptr;
};

#endif
