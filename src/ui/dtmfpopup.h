#ifndef DTMFPOPUP_H
#define DTMFPOPUP_H

#include "ui/k4popupbase.h"

class DtmfPopupWidget : public K4PopupBase {
    Q_OBJECT
public:
    explicit DtmfPopupWidget(QWidget *parent = nullptr);

signals:
    void digitRequested(QChar digit);
    void sequenceRequested(const QString &sequence);

protected:
    QSize contentSize() const override;

private:
    void beginCommandEdit(int index);
    void finishCommandEdit();
    void updateCommandButtons();
    class QLabel *m_status = nullptr;
    class QPushButton *m_commandButtons[6] = {};
    int m_editingCommand = -1;
    QString m_editSequence;
};

#endif
