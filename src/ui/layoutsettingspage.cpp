#include "layoutsettingspage.h"
#include "k4styles.h"
#include "../settings/radiosettings.h"
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>

QWidget *createLayoutSettingsPage(QWidget *parent) {
    auto *page = new QWidget(parent);
    page->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::Background));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    auto *title = new QLabel("Console layout", page);
    title->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: bold;")
                             .arg(K4Styles::Colors::AccentAmber));
    layout->addWidget(title);

    auto *compact = new QCheckBox("Always use compact layout", page);
    compact->setObjectName("alwaysCompactLayout");
    compact->setMinimumHeight(44);
    compact->setStyleSheet(QString("QCheckBox { color: %1; font-size: 14px; spacing: 8px; }"
                                  "QCheckBox::indicator { width: 24px; height: 24px; "
                                  "border: 2px solid #808080; border-radius: 4px; }"
                                  "QCheckBox::indicator:checked { background-color: %2; "
                                  "image: url(:/icons/check.svg); }")
                              .arg(K4Styles::Colors::TextWhite, K4Styles::Colors::AccentAmber));
    auto *settings = RadioSettings::instance();
    compact->setChecked(settings->alwaysCompactLayout());
    layout->addWidget(compact);

    auto *help = new QLabel("Detected folding devices always use the compact console, open or closed. "
                           "Enable this option if your device is not detected or you prefer compact controls.", page);
    help->setWordWrap(true);
    help->setStyleSheet(QString("color: %1; font-size: 12px;").arg(K4Styles::Colors::TextGray));
    layout->addWidget(help);

    auto *status = new QLabel("Changes save automatically and take effect after restarting QK4 Mobile.", page);
    status->setObjectName("layoutRestartNotice");
    status->setWordWrap(true);
    status->setStyleSheet(QString("color: %1; font-size: 12px;").arg(K4Styles::Colors::AccentAmber));
    layout->addWidget(status);
    layout->addStretch();

    // This short page fits without scrolling. Do not reconfigure global styles
    // or rebuild the live console while a radio session may be active.
    QObject::connect(compact, &QCheckBox::toggled, page, [settings, status](bool enabled) {
        settings->setAlwaysCompactLayout(enabled);
        status->setText("Saved. Restart QK4 Mobile to apply the layout preference.");
    });
    return page;
}
