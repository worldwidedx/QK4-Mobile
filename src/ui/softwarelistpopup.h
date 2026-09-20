#ifndef SOFTWARELISTPOPUP_H
#define SOFTWARELISTPOPUP_H

#include "ui/k4popupbase.h"
#include <QMap>
class QLabel;

class SoftwareListPopupWidget : public K4PopupBase {
    Q_OBJECT
public:
    explicit SoftwareListPopupWidget(QWidget *parent = nullptr);
    void setVersions(const QMap<QString, QString> &versions);
protected:
    QSize contentSize() const override;
private:
    QLabel *m_title = nullptr;
    QLabel *m_values[12] = {};
};
#endif
