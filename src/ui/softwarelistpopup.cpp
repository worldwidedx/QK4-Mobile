#include "ui/softwarelistpopup.h"
#include "ui/k4styles.h"
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace { struct Row { const char *cat; const char *label; };
const Row Rows[] = {{"KUI","KUI"},{"KSRV","KSRV"},{"KUP","KUP"},{"KCFG","KCFG"},{"FP","FP"},{"RFB","RF"},
                    {"DSP","DSP"},{"DAP","DAP"},{"DDC0","DDC1"},{"DDC1","DDC2"},{"DUC","DUC"},{"REF","REF"}}; }

SoftwareListPopupWidget::SoftwareListPopupWidget(QWidget *parent) : K4PopupBase(parent) {
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(contentMargins());
    outer->setSpacing(7);
    m_title = new QLabel("SOFTWARE LIST", this);
    m_title->setAlignment(Qt::AlignCenter);
    m_title->setStyleSheet(QString("color:%1;font-size:18px;font-weight:bold;").arg(K4Styles::Colors::TextWhite));
    outer->addWidget(m_title);
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(8); grid->setVerticalSpacing(3);
    for (int i=0; i<12; ++i) {
        const int col=(i/6)*2, row=i%6;
        auto *name=new QLabel(Rows[i].label,this);
        name->setStyleSheet(QString("color:%1;font-size:14px;").arg(K4Styles::Colors::TextGray));
        m_values[i]=new QLabel(QString::fromUtf8("\xE2\x80\x94"),this);
        m_values[i]->setMinimumWidth(105);
        m_values[i]->setStyleSheet(QString("color:%1;font-size:14px;").arg(K4Styles::Colors::TextWhite));
        grid->addWidget(name,row,col); grid->addWidget(m_values[i],row,col+1);
    }
    outer->addLayout(grid);
    auto *close=new QPushButton(QString::fromUtf8("\xE2\x86\xA9"),this);
    close->setMinimumHeight(42); close->setStyleSheet(K4Styles::menuBarButtonSmall()); outer->addWidget(close);
    connect(close,&QPushButton::clicked,this,&K4PopupBase::hidePopup);
    initPopup();
}
QSize SoftwareListPopupWidget::contentSize() const { return QSize(430,245); }
void SoftwareListPopupWidget::setVersions(const QMap<QString, QString> &versions) {
    const QString rev=versions.value("R");
    m_title->setText(rev.isEmpty()?"SOFTWARE LIST":QString("SOFTWARE LIST ( %1 )").arg(rev));
    for(int i=0;i<12;++i) m_values[i]->setText(versions.value(Rows[i].cat,QString::fromUtf8("\xE2\x80\x94")));
}
