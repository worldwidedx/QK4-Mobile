#include "optionsdialog.h"
#include "overlaybackhandler.h"
#include "k4styles.h"
#include "micmeterwidget.h"
#include "inwindowdialog.h"
#include "fnpopupwidget.h"
#include "ctr2mappingeditor.h"
#include "../models/radiostate.h"
#include "../hardware/kpoddevice.h"
#include "../hardware/halikeydevice.h"
#include "../hardware/ctr2mididevice.h"
#include "../settings/fnkeymemory.h"
#include "../settings/radiosettings.h"
#include "../network/catserver.h"
#include "../audio/audioengine.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QScrollArea>
#include <QScrollBar>
#include <QStringList>
#include <QCheckBox>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QComboBox>
#include <QSlider>
#include <QPushButton>
#include <QApplication>
#include <QSignalBlocker>
#include <QPointer>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QScroller>
#include <QScrollerProperties>
#include <QMouseEvent>
#include <QStyle>
#include <QStandardPaths>
#ifdef Q_OS_ANDROID
#include <QPermissions>
#endif

// Use K4Styles::Colors::DialogBorder for dialog-specific borders

namespace {
QString fnFunctionId(const QString &key) {
    return QStringLiteral("Fn.") + key;
}
} // namespace

OptionsDialog::OptionsDialog(RadioState *radioState, AudioEngine *audioEngine, KpodDevice *kpodDevice,
                             CatServer *catServer, HalikeyDevice *halikeyDevice,
                             Ctr2MidiDevice *ctr2MidiDevice, QWidget *parent)
    : OptionsDialogBase(parent), m_radioState(radioState), m_audioEngine(audioEngine), m_kpodDevice(kpodDevice),
      m_catServer(catServer), m_halikeyDevice(halikeyDevice), m_ctr2MidiDevice(ctr2MidiDevice),
      m_micDeviceCombo(nullptr), m_micGainSlider(nullptr),
      m_micGainValueLabel(nullptr), m_micTestBtn(nullptr), m_micMeter(nullptr), m_speakerDeviceCombo(nullptr),
      m_catServerEnableCheckbox(nullptr), m_catServerPortEdit(nullptr), m_catServerStatusLabel(nullptr),
      m_catServerClientsLabel(nullptr), m_cwKeyerDeviceTypeCombo(nullptr), m_cwKeyerDescLabel(nullptr),
      m_cwKeyerPortCombo(nullptr), m_cwKeyerRefreshBtn(nullptr), m_cwKeyerConnectBtn(nullptr),
      m_cwKeyerStatusLabel(nullptr) {
#ifndef Q_OS_ANDROID
    setWindowModality(Qt::ApplicationModal);
#else
    setObjectName("optionsOverlay");
    setAttribute(Qt::WA_StyledBackground, true);
#endif
    setupUi();

    // Connect to KPOD device signals for real-time status updates
    if (m_kpodDevice) {
        connect(m_kpodDevice, &KpodDevice::deviceConnected, this, &OptionsDialog::updateKpodStatus);
        connect(m_kpodDevice, &KpodDevice::deviceDisconnected, this, &OptionsDialog::updateKpodStatus);
    }

    // Connect to CatServer signals for status updates
    if (m_catServer) {
        connect(m_catServer, &CatServer::started, this, &OptionsDialog::updateCatServerStatus);
        connect(m_catServer, &CatServer::stopped, this, &OptionsDialog::updateCatServerStatus);
        connect(m_catServer, &CatServer::clientConnected, this, &OptionsDialog::updateCatServerStatus);
        connect(m_catServer, &CatServer::clientDisconnected, this, &OptionsDialog::updateCatServerStatus);
    }

    // Connect to HalikeyDevice signals for status updates
    if (m_halikeyDevice) {
        connect(m_halikeyDevice, &HalikeyDevice::connected, this, &OptionsDialog::updateCwKeyerStatus);
        connect(m_halikeyDevice, &HalikeyDevice::disconnected, this, &OptionsDialog::updateCwKeyerStatus);
        connect(m_halikeyDevice, &HalikeyDevice::connectionError, this,
                [this](const QString &) { updateCwKeyerStatus(); });
    }
}

OptionsDialog::~OptionsDialog() {
    // Make sure to stop mic test if dialog is closed
    if (m_micTestActive && m_audioEngine) {
        QMetaObject::invokeMethod(m_audioEngine, "setMicEnabled", Qt::QueuedConnection, Q_ARG(bool, false));
    }
}

void OptionsDialog::setupUi() {
    new OverlayBackHandler(this, [this] { requestReturnToOperate(); });
    setWindowTitle("Options");
#ifdef Q_OS_ANDROID
    setMinimumSize(0, 0);
#else
    setMinimumSize(700, 550);
    resize(800, 650);
#endif

    // Dark theme styling
    setStyleSheet(QString("QDialog, QWidget#optionsOverlay { background-color: %1; }"
                          "QLabel { color: %2; }"
                          "QListWidget { background-color: %3; color: %2; border: 1px solid %4; "
                          "             font-size: %6px; outline: none; }"
                          "QListWidget::item { padding: 10px 15px; border-bottom: 1px solid %4; }"
                          "QListWidget::item:selected { background-color: %5; color: %3; }"
                          "QListWidget::item:hover { background-color: %7; }")
                      .arg(K4Styles::Colors::Background, K4Styles::Colors::TextWhite, K4Styles::Colors::DarkBackground,
                           K4Styles::Colors::DialogBorder, K4Styles::Colors::AccentAmber)
                      .arg(K4Styles::Dimensions::FontSizePopup)
                      .arg(K4Styles::Colors::GradientBottom));

#ifdef Q_OS_ANDROID
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(6, 4, 6, 6);
    outerLayout->setSpacing(4);
    auto *header = new QHBoxLayout();
    auto *title = new QLabel("QK4 SETTINGS", this);
    title->setStyleSheet(QString("color: %1; font-size: 16px; font-weight: bold;")
                             .arg(K4Styles::Colors::AccentAmber));
    auto *close = new QPushButton("RETURN TO OPERATE", this);
    close->setMinimumHeight(30);
    close->setStyleSheet(K4Styles::menuBarButton());
    connect(close, &QPushButton::clicked, this, &OptionsDialog::requestReturnToOperate);
    header->addWidget(title);
    header->addStretch(1);
    header->addWidget(close);
    outerLayout->addLayout(header);
    auto *mainLayout = new QHBoxLayout();
    outerLayout->addLayout(mainLayout, 1);
#else
    auto *mainLayout = new QHBoxLayout(this);
#endif
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Left side: vertical tab list
    m_tabList = new QListWidget(this);
    m_tabList->setFixedWidth(K4Styles::Dimensions::TabListWidth);
    m_tabList->addItem("About");
    m_tabList->addItem("Audio Input");
    m_tabList->addItem("Audio Output");
    m_tabList->addItem("Rig Control");
    m_tabList->addItem("CW Keyer");
    m_tabList->addItem("CTR2");
    m_tabList->addItem("K-Pod");
    m_tabList->addItem("Fn Key Setup");
#ifdef Q_OS_ANDROID
    // The phone settings surface intentionally contains app information plus
    // the independent CW Keyer and CTR2-MIDI device/setup roles.
    for (Page hiddenPage : {PageAudioInput, PageAudioOutput, PageRigControl, PageKpod})
        m_tabList->item(hiddenPage)->setHidden(true);
#endif
    m_tabList->setCurrentRow(0);

    // Right side: stacked pages (lazy — only About is created eagerly)
    m_pageStack = new QStackedWidget(this);
    m_pageStack->addWidget(createAboutPage());
    m_pageCreated[PageAbout] = true;
    for (int i = 1; i < PageCount; ++i)
        m_pageStack->addWidget(new QWidget(this));

    // Connect tab selection to page switching with lazy creation
    connect(m_tabList, &QListWidget::currentRowChanged, this, [this](int index) {
        ensurePageCreated(index);
        m_pageStack->setCurrentIndex(index);
    });

    mainLayout->addWidget(m_tabList);
    mainLayout->addWidget(m_pageStack, 1);
}

void OptionsDialog::ensurePageCreated(int index) {
    if (index < 0 || index >= PageCount || m_pageCreated[index])
        return;

    QWidget *page = nullptr;
    switch (index) {
    case PageAudioInput:
        page = createAudioInputPage();
        break;
    case PageAudioOutput:
        page = createAudioOutputPage();
        break;
    case PageRigControl:
        page = createRigControlPage();
        break;
    case PageCwKeyer:
        page = createCwKeyerPage();
        break;
    case PageCtr2Midi:
        page = createCtr2MidiPage();
        break;
    case PageKpod:
        page = createKpodPage();
        break;
    case PageFnKeySetup:
        page = createFnKeySetupPage();
        break;
    default:
        return;
    }

    // Swap out the placeholder widget at this index
    QWidget *placeholder = m_pageStack->widget(index);
    m_pageStack->removeWidget(placeholder);
    delete placeholder;
    m_pageStack->insertWidget(index, page);
    m_pageCreated[index] = true;
}

QWidget *OptionsDialog::createCtr2MidiPage() {
    auto *page = new QWidget(this);
    page->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::Background));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(K4Styles::Dimensions::DialogMargin,
                               K4Styles::Dimensions::PaddingSmall,
                               K4Styles::Dimensions::DialogMargin,
                               K4Styles::Dimensions::PaddingSmall);
    m_ctr2MappingEditor = new Ctr2MappingEditor(m_ctr2MidiDevice, page);
    layout->addWidget(m_ctr2MappingEditor);
    return page;
}

void OptionsDialog::requestReturnToOperate() {
    if (!m_ctr2MappingEditor || !m_ctr2MappingEditor->hasUnsavedChanges()) {
        hide();
        return;
    }

    InWindowDialog dialog(this);
    QWidget *panel = dialog.contentWidget();
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 12, 14, 12);

    auto *title = new QLabel("Unsaved CTR2 mapping", panel);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:bold;")
                             .arg(K4Styles::Colors::AccentAmber)
                             .arg(K4Styles::Dimensions::FontSizeTitle));
    layout->addWidget(title);

    auto *message = new QLabel(
        "The CTR2 mapping has changes that have not been applied. Apply them before returning to operate?",
        panel);
    message->setWordWrap(true);
    layout->addWidget(message);

    auto *buttons = new QHBoxLayout;
    auto *cancel = new QPushButton("CANCEL", panel);
    auto *abandon = new QPushButton("ABANDON", panel);
    auto *apply = new QPushButton("APPLY", panel);
    for (auto *button : {cancel, abandon, apply}) {
        button->setMinimumHeight(42);
        button->setStyleSheet(K4Styles::menuBarButton());
        buttons->addWidget(button);
    }
    layout->addLayout(buttons);

    connect(cancel, &QPushButton::clicked, &dialog, &InWindowDialog::reject);
    connect(abandon, &QPushButton::clicked, &dialog, [&dialog]() { dialog.done(2); });
    connect(apply, &QPushButton::clicked, &dialog, [&dialog]() { dialog.done(3); });
    dialog.setPanelSize(QSize(qMin(650, width() - 20), 220));

    const int result = dialog.exec();
    if (result == 3) {
        if (m_ctr2MappingEditor->applyChanges())
            hide();
    } else if (result == 2) {
        m_ctr2MappingEditor->abandonChanges();
        hide();
    }
}

QWidget *OptionsDialog::createFnKeySetupPage() {
    auto *page = new QWidget(this);
    page->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::Background));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(K4Styles::Dimensions::DialogMargin, K4Styles::Dimensions::DialogMargin,
                               K4Styles::Dimensions::DialogMargin, K4Styles::Dimensions::DialogMargin);
    layout->setSpacing(6);

    auto *titleRow = new QHBoxLayout();
    auto *title = new QLabel("Fn Key Setup", page);
    title->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                             .arg(K4Styles::Colors::AccentAmber)
                             .arg(K4Styles::Dimensions::FontSizeTitle));
    m_fnKeySaveStatus = new QLabel("Changes save automatically", page);
    m_fnKeySaveStatus->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_fnKeySaveStatus->setStyleSheet(QString("color: %1; font-size: %2px; font-style: italic;")
                                         .arg(K4Styles::Colors::TextGray)
                                         .arg(K4Styles::Dimensions::FontSizeLarge));
    titleRow->addWidget(title);
    titleRow->addStretch();
    titleRow->addWidget(m_fnKeySaveStatus);
    layout->addLayout(titleRow);

    auto *fileActions = new QHBoxLayout();
    fileActions->setSpacing(6);
    auto *loadFile = new QPushButton("LOAD FILE", page);
    auto *saveFile = new QPushButton("SAVE FILE", page);
    for (auto *button : {loadFile, saveFile}) {
        button->setMinimumHeight(42);
        button->setStyleSheet(K4Styles::menuBarButton());
        fileActions->addWidget(button);
    }
    auto *fileHelp = new QLabel("Loading replaces all eight FN key assignments; files are never merged.", page);
    fileHelp->setWordWrap(true);
    fileHelp->setStyleSheet(QString("color: %1; font-size: %2px;")
                                .arg(K4Styles::Colors::TextGray)
                                .arg(K4Styles::Dimensions::FontSizeLarge));
    fileActions->addWidget(fileHelp, 1);
    layout->addLayout(fileActions);
    connect(loadFile, &QPushButton::clicked, this, &OptionsDialog::loadFnKeyFile);
    connect(saveFile, &QPushButton::clicked, this, [this]() { saveFnKeyFile(); });

    auto *columnHeader = new QWidget(page);
    auto *headerLayout = new QHBoxLayout(columnHeader);
    headerLayout->setContentsMargins(12, 0, 12, 0);
    for (const QString &heading : {QStringLiteral("Function"), QStringLiteral("Button label"),
                                   QStringLiteral("CAT command")}) {
        auto *label = new QLabel(heading, columnHeader);
        label->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                 .arg(K4Styles::Colors::TextGray)
                                 .arg(K4Styles::Dimensions::FontSizeLarge));
        headerLayout->addWidget(label, 1);
    }
    layout->addWidget(columnHeader);

    auto *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QString("QScrollArea { border: 1px solid %1; background: %2; }")
                              .arg(K4Styles::Colors::DialogBorder, K4Styles::Colors::DarkBackground));
    scroll->viewport()->setStyleSheet(QString("background: %1;").arg(K4Styles::Colors::DarkBackground));
    auto *rows = new QWidget(scroll);
    rows->setStyleSheet(QString("background: %1;").arg(K4Styles::Colors::DarkBackground));
    auto *rowsLayout = new QVBoxLayout(rows);
    rowsLayout->setContentsMargins(0, 0, 0, 0);
    rowsLayout->setSpacing(1);

    const QVector<QPair<QString, QString>> macroSlots = {
        {MacroIds::FnF1, "F1"}, {MacroIds::FnF2, "F2"}, {MacroIds::FnF3, "F3"}, {MacroIds::FnF4, "F4"},
        {MacroIds::FnF5, "F5"}, {MacroIds::FnF6, "F6"}, {MacroIds::FnF7, "F7"}, {MacroIds::FnF8, "F8"}};
    const QString editStyle = QString("QLineEdit { background: %1; color: %2; border: 1px solid %3; "
                                      "padding: 7px; font-size: %4px; }")
                                  .arg(K4Styles::Colors::GradientMid1, K4Styles::Colors::TextWhite,
                                       K4Styles::Colors::DialogBorder)
                                  .arg(K4Styles::Dimensions::FontSizePopup);
    for (const auto &slot : macroSlots) {
        auto *row = new QWidget(rows);
        row->setMinimumHeight(52);
        row->setStyleSheet(QString("background: %1;").arg(K4Styles::Colors::OverlayItemBg));
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(12, 5, 12, 5);
        rowLayout->setSpacing(10);
        auto *function = new QLabel(slot.second, row);
        function->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                    .arg(K4Styles::Colors::AccentAmber)
                                    .arg(K4Styles::Dimensions::FontSizePopup));
        auto *labelEdit = new QLineEdit(row);
        auto *commandEdit = new QLineEdit(row);
        const MacroEntry macro = RadioSettings::instance()->macro(slot.first);
        labelEdit->setText(macro.label);
        labelEdit->setPlaceholderText("F-key label");
        labelEdit->setMaxLength(12);
        commandEdit->setText(macro.command);
        commandEdit->setPlaceholderText("CAT command");
        commandEdit->setMaxLength(64);
        labelEdit->setStyleSheet(editStyle);
        commandEdit->setStyleSheet(editStyle);
        rowLayout->addWidget(function, 1);
        rowLayout->addWidget(labelEdit, 1);
        rowLayout->addWidget(commandEdit, 1);
        m_fnKeyEditors.insert(slot.first, FnKeyEditors{labelEdit, commandEdit});
        const auto saveRow = [this, id = slot.first]() { saveFnKeyEditor(id); };
        connect(labelEdit, &QLineEdit::editingFinished, row, saveRow);
        connect(commandEdit, &QLineEdit::editingFinished, row, saveRow);
        rowsLayout->addWidget(row);
    }
    rowsLayout->addStretch();
    scroll->setWidget(rows);
    layout->addWidget(scroll, 1);
#ifdef Q_OS_ANDROID
    scroll->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    QScroller::grabGesture(scroll->viewport(), QScroller::TouchGesture);
    if (QScroller *scroller = QScroller::scroller(scroll->viewport())) {
        QScrollerProperties properties = scroller->scrollerProperties();
        properties.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.25);
        properties.setScrollMetric(QScrollerProperties::DragStartDistance, 0.0015);
        scroller->setScrollerProperties(properties);
    }
#endif
    return page;
}

void OptionsDialog::saveFnKeyEditor(const QString &functionId) {
    if (m_loadingFnKeyFile)
        return;
    const auto editors = m_fnKeyEditors.constFind(functionId);
    if (editors == m_fnKeyEditors.cend() || !editors->label || !editors->command)
        return;
    RadioSettings::instance()->setMacro(functionId, editors->label->text().trimmed(),
                                        editors->command->text().trimmed());
    if (m_fnKeySaveStatus) {
        m_fnKeySaveStatus->setText("Saved");
        m_fnKeySaveStatus->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                             .arg(K4Styles::Colors::AccentAmber)
                                             .arg(K4Styles::Dimensions::FontSizeLarge));
    }
}

void OptionsDialog::persistFnKeyEditors() {
    for (auto it = m_fnKeyEditors.cbegin(); it != m_fnKeyEditors.cend(); ++it)
        saveFnKeyEditor(it.key());
}

void OptionsDialog::refreshFnKeyEditors() {
    m_loadingFnKeyFile = true;
    for (auto it = m_fnKeyEditors.cbegin(); it != m_fnKeyEditors.cend(); ++it) {
        if (!it->label || !it->command)
            continue;
        const MacroEntry macro = RadioSettings::instance()->macro(it.key());
        const QSignalBlocker labelBlocker(it->label);
        const QSignalBlocker commandBlocker(it->command);
        it->label->setText(macro.label);
        it->command->setText(macro.command);
    }
    m_loadingFnKeyFile = false;
}

void OptionsDialog::loadFnKeyFile() {
    // FN setup edits are auto-saved. Commit any active editor before opening
    // the picker so there is never an untracked draft requiring a warning.
    persistFnKeyEditors();
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Load FN key setup"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QStringLiteral("QK4 FN key setup (*.qk4fnmap *.json);;All files (*)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        showInWindowMessage(this, QStringLiteral("FN Key Setup"),
                            QStringLiteral("The selected FN key file could not be opened."));
        return;
    }
    const QByteArray fileData = file.readAll();
    if (fileData.size() > 64 * 1024) {
        showInWindowMessage(this, QStringLiteral("FN Key Setup"),
                            QStringLiteral("The selected file is too large to be an FN key setup."));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(fileData, &parseError);
    FnKeyMemory::Setup imported;
    QString error;
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || !FnKeyMemory::fromJson(document.object(), &imported, &error)) {
        showInWindowMessage(this, QStringLiteral("FN Key Setup"),
                            error.isEmpty() ? QStringLiteral("This is not a valid QK4 FN key file.")
                                            : error);
        return;
    }

    // Replace only Fn.F1 through Fn.F8. PF, keyboard, K-Pod, and other macro
    // assignments remain untouched, and an imported file is never merged with
    // the previous FN bank.
    QMap<QString, MacroEntry> macros = RadioSettings::instance()->macros();
    for (const QString &key : FnKeyMemory::keyNames()) {
        const QString functionId = fnFunctionId(key);
        macros.remove(functionId);
        const FnKeyMemory::Entry entry = imported.keys.value(key);
        if (!entry.command.isEmpty())
            macros.insert(functionId, MacroEntry{functionId, entry.label, entry.command});
    }
    RadioSettings::instance()->replaceMacros(macros);
    m_fnKeyMemoryName = imported.name;
    refreshFnKeyEditors();
    if (m_fnKeySaveStatus)
        m_fnKeySaveStatus->setText("Loaded from file");
    showInWindowMessage(this, QStringLiteral("FN Key Setup"),
                        QStringLiteral("FN key file loaded. All eight FN assignments were replaced."));
}

bool OptionsDialog::saveFnKeyFile() {
    persistFnKeyEditors();

    FnKeyMemory::Setup setup;
    setup.name = m_fnKeyMemoryName;
    for (const QString &key : FnKeyMemory::keyNames()) {
        const MacroEntry macro = RadioSettings::instance()->macro(fnFunctionId(key));
        setup.keys.insert(key, FnKeyMemory::Entry{macro.label, macro.command});
    }
    QString error;
    if (!FnKeyMemory::validate(setup, &error)) {
        showInWindowMessage(this, QStringLiteral("FN Key Setup"), error);
        return false;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save FN key setup"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            + QStringLiteral("/QK4-FN-Keys.qk4fnmap"),
        QStringLiteral("QK4 FN key setup (*.qk4fnmap)"));
    if (path.isEmpty())
        return false;

    const QByteArray fileData =
        QJsonDocument(FnKeyMemory::toJson(setup)).toJson(QJsonDocument::Indented);
#ifdef Q_OS_ANDROID
    // Android's document picker may return a content URI. QFile supports that
    // URI through Qt's Android file engine; QSaveFile's rename commit does not.
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(fileData) != fileData.size()) {
        showInWindowMessage(this, QStringLiteral("FN Key Setup"),
                            QStringLiteral("The FN key file could not be saved."));
        return false;
    }
    file.close();
#else
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        showInWindowMessage(this, QStringLiteral("FN Key Setup"),
                            QStringLiteral("The FN key file could not be created."));
        return false;
    }
    if (file.write(fileData) != fileData.size() || !file.commit()) {
        showInWindowMessage(this, QStringLiteral("FN Key Setup"),
                            QStringLiteral("The FN key file could not be saved."));
        return false;
    }
#endif

    if (m_fnKeySaveStatus)
        m_fnKeySaveStatus->setText("File saved");
    showInWindowMessage(this, QStringLiteral("FN Key Setup"),
                        QStringLiteral("FN key file saved."));
    return true;
}

void OptionsDialog::showEvent(QShowEvent *event) {
    OptionsDialogBase::showEvent(event);
    refreshCurrentPage();
}

void OptionsDialog::hideEvent(QHideEvent *event) {
    // Stop mic test when dialog is hidden
    if (m_micTestActive && m_audioEngine) {
        QMetaObject::invokeMethod(m_audioEngine, "setMicEnabled", Qt::QueuedConnection, Q_ARG(bool, false));
        m_micTestActive = false;
        if (m_micTestBtn)
            m_micTestBtn->setChecked(false);
        if (m_micMeter)
            m_micMeter->setLevel(0.0f);
    }
    OptionsDialogBase::hideEvent(event);
}

bool OptionsDialog::eventFilter(QObject *watched, QEvent *event) {
#ifdef Q_OS_ANDROID
    if (m_cwControlsScroll && watched->property("cwScrollSurface").toBool()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_cwScrollTouchTarget = watched;
                m_cwScrollPressY = qRound(mouseEvent->globalPosition().y());
                m_cwScrollStartValue = m_cwControlsScroll->verticalScrollBar()->value();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && watched == m_cwScrollTouchTarget) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            const int deltaY = qRound(mouseEvent->globalPosition().y()) - m_cwScrollPressY;
            m_cwControlsScroll->verticalScrollBar()->setValue(m_cwScrollStartValue - deltaY);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && watched == m_cwScrollTouchTarget) {
            m_cwScrollTouchTarget = nullptr;
            return true;
        } else if ((event->type() == QEvent::Leave || event->type() == QEvent::UngrabMouse) &&
                   watched == m_cwScrollTouchTarget) {
            m_cwScrollTouchTarget = nullptr;
        }
    }

    auto *slider = qobject_cast<QSlider *>(watched);
    if (slider && (slider == m_cwSpeedSlider || slider == m_sidetoneVolumeSlider)) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_touchSlider = slider;
                setTouchSliderValue(slider, mouseEvent->position().x());
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && slider == m_touchSlider) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            setTouchSliderValue(slider, mouseEvent->position().x());
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && slider == m_touchSlider) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            setTouchSliderValue(slider, mouseEvent->position().x());
            if (slider == m_cwSpeedSlider)
                emit keyerSpeedRequested(slider->value());
            m_touchSlider = nullptr;
            return true;
        } else if ((event->type() == QEvent::Leave || event->type() == QEvent::UngrabMouse) &&
                   slider == m_touchSlider) {
            m_touchSlider = nullptr;
        }
    }
#endif
    return OptionsDialogBase::eventFilter(watched, event);
}

void OptionsDialog::setTouchSliderValue(QSlider *slider, int xPosition) {
    if (!slider)
        return;

    const int handleWidth = qMax(12, slider->height() / 2);
    const int span = qMax(1, slider->width() - handleWidth);
    const int position = qBound(0, xPosition - handleWidth / 2, span);
    slider->setValue(QStyle::sliderValueFromPosition(slider->minimum(), slider->maximum(), position, span,
                                                     slider->invertedAppearance()));
}

void OptionsDialog::refreshCurrentPage() {
    refreshPage(m_pageStack->currentIndex());
}

void OptionsDialog::refreshPage(int index) {
    if (index < 0 || index >= PageCount || !m_pageCreated[index])
        return;

    switch (index) {
    case PageAudioInput:
        populateMicDevices();
        break;
    case PageAudioOutput:
        populateSpeakerDevices();
        break;
    case PageRigControl:
        updateCatServerStatus();
        break;
    case PageCwKeyer:
        populateCwKeyerPorts();
        updateCwKeyerStatus();
        break;
    case PageKpod:
        updateKpodStatus();
        break;
    case PageFnKeySetup:
        refreshFnKeyEditors();
        break;
    case PageAbout:
    default:
        break;
    }
}

namespace {
QStringList decodeOptionModules(const QString &om) {
    QStringList options;
    if (om.length() > 0 && om[0] == 'A')
        options << "KAT4 (ATU)";
    if (om.length() > 1 && om[1] == 'P')
        options << "KPA4 (PA)";
    if (om.length() > 2 && om[2] == 'X')
        options << "XVTR";
    if (om.length() > 3 && om[3] == 'S')
        options << "KRX4 (Sub RX)";
    if (om.length() > 4 && om[4] == 'H')
        options << "KHDR4 (HDR)";
    if (om.length() > 5 && om[5] == 'M')
        options << "K40 (Mini)";
    if (om.length() > 6 && om[6] == 'L')
        options << "Linear Amp";
    if (om.length() > 7 && om[7] == '1')
        options << "KPA1500";
    return options;
}
} // namespace

QWidget *OptionsDialog::createAboutPage() {
    auto *page = new QWidget(this);
    page->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::Background));

    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(K4Styles::Dimensions::DialogMargin, K4Styles::Dimensions::DialogMargin,
                               K4Styles::Dimensions::DialogMargin, K4Styles::Dimensions::DialogMargin);
    layout->setSpacing(K4Styles::Dimensions::PaddingLarge);

#ifdef Q_OS_ANDROID
    auto *title = new QLabel("QK4 Mobile", page);
    title->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                             .arg(K4Styles::Colors::AccentAmber)
                             .arg(K4Styles::Dimensions::FontSizeTitle));
    auto *version = new QLabel(QString("Version %1").arg(QCoreApplication::applicationVersion()), page);
    version->setStyleSheet(QString("color: %1; font-size: %2px;")
                               .arg(K4Styles::Colors::TextWhite)
                               .arg(K4Styles::Dimensions::FontSizePopup));
    layout->addWidget(title);
    layout->addWidget(version);
    auto *credits = new QLabel(
        "Android development and phone UX by WorldwideDX.com\n"
        "Based on the original QK4 by Mike Garcia, KF5O\n"
        "Licensed under GNU GPL v3.0 or later", page);
    credits->setStyleSheet(QString("color: %1; font-size: %2px;")
                               .arg(K4Styles::Colors::TextGray)
                               .arg(K4Styles::Dimensions::FontSizeButton));
    credits->setWordWrap(true);
    layout->addWidget(credits);
    layout->addStretch();
    return page;
#endif

    // Title
    auto *titleLabel = new QLabel("Connected Radio", page);
    titleLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                  .arg(K4Styles::Colors::AccentAmber)
                                  .arg(K4Styles::Dimensions::FontSizeTitle));
    layout->addWidget(titleLabel);

    // Separator line
    auto *line = new QFrame(page);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::DialogBorder));
    line->setFixedHeight(K4Styles::Dimensions::SeparatorHeight);
    layout->addWidget(line);

    // Two-column layout for Radio Info and Installed Options
    auto *infoRow = new QHBoxLayout();
    infoRow->setSpacing(K4Styles::Dimensions::DialogMargin);

    // Left column widget: Radio ID and Model
    auto *leftWidget = new QWidget(page);
    auto *leftColumn = new QVBoxLayout(leftWidget);
    leftColumn->setContentsMargins(0, 0, 0, 0);
    leftColumn->setSpacing(K4Styles::Dimensions::PopupButtonSpacing);

    // Radio ID
    auto *idLayout = new QHBoxLayout();
    auto *idTitleLabel = new QLabel("Radio ID:", leftWidget);
    idTitleLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                    .arg(K4Styles::Colors::TextGray)
                                    .arg(K4Styles::Dimensions::FontSizePopup));
    idTitleLabel->setFixedWidth(K4Styles::Dimensions::FormLabelWidth);

    QString radioId = m_radioState ? m_radioState->radioID() : "Not connected";
    auto *idValueLabel = new QLabel(radioId, leftWidget);
    idValueLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                    .arg(K4Styles::Colors::TextWhite)
                                    .arg(K4Styles::Dimensions::FontSizePopup));

    idLayout->addWidget(idTitleLabel);
    idLayout->addWidget(idValueLabel);
    idLayout->addStretch();
    leftColumn->addLayout(idLayout);

    // Radio Model
    auto *modelLayout = new QHBoxLayout();
    auto *modelTitleLabel = new QLabel("Model:", leftWidget);
    modelTitleLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                       .arg(K4Styles::Colors::TextGray)
                                       .arg(K4Styles::Dimensions::FontSizePopup));
    modelTitleLabel->setFixedWidth(K4Styles::Dimensions::FormLabelWidth);

    QString radioModel = m_radioState ? m_radioState->radioModel() : "Unknown";
    auto *modelValueLabel = new QLabel(radioModel, leftWidget);
    modelValueLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                       .arg(K4Styles::Colors::TextWhite)
                                       .arg(K4Styles::Dimensions::FontSizePopup));

    modelLayout->addWidget(modelTitleLabel);
    modelLayout->addWidget(modelValueLabel);
    modelLayout->addStretch();
    leftColumn->addLayout(modelLayout);
    leftColumn->addStretch();

    // Vertical demarcation line
    auto *vline = new QFrame(page);
    vline->setFrameShape(QFrame::VLine);
    vline->setFrameShadow(QFrame::Plain);
    vline->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::DialogBorder));
    vline->setFixedWidth(K4Styles::Dimensions::SeparatorHeight);

    // Right column widget: Installed Options
    auto *rightWidget = new QWidget(page);
    auto *rightColumn = new QVBoxLayout(rightWidget);
    rightColumn->setContentsMargins(0, 0, 0, 0);
    rightColumn->setSpacing(K4Styles::Dimensions::PaddingSmall);

    auto *optionsTitle = new QLabel("Installed Options", rightWidget);
    optionsTitle->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                    .arg(K4Styles::Colors::AccentAmber)
                                    .arg(K4Styles::Dimensions::FontSizePopup));
    rightColumn->addWidget(optionsTitle);

    if (m_radioState && !m_radioState->optionModules().isEmpty()) {
        QStringList options = decodeOptionModules(m_radioState->optionModules());
        if (options.isEmpty()) {
            auto *noOptionsLabel = new QLabel("No additional options", rightWidget);
            noOptionsLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-style: italic;")
                                              .arg(K4Styles::Colors::TextGray)
                                              .arg(K4Styles::Dimensions::FontSizeButton));
            rightColumn->addWidget(noOptionsLabel);
        } else {
            for (const QString &opt : options) {
                auto *optLabel = new QLabel(QString::fromUtf8("\u2022 ") + opt, rightWidget);
                optLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                            .arg(K4Styles::Colors::TextWhite)
                                            .arg(K4Styles::Dimensions::FontSizeButton));
                rightColumn->addWidget(optLabel);
            }
        }
    } else {
        auto *noDataLabel = new QLabel("Not connected", rightWidget);
        noDataLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-style: italic;")
                                       .arg(K4Styles::Colors::TextGray)
                                       .arg(K4Styles::Dimensions::FontSizeButton));
        rightColumn->addWidget(noDataLabel);
    }
    rightColumn->addStretch();

    // Assemble the info row
    infoRow->addWidget(leftWidget, 1);
    infoRow->addWidget(vline);
    infoRow->addWidget(rightWidget, 1);

    layout->addLayout(infoRow);

    // Software Versions section
    layout->addSpacing(K4Styles::Dimensions::PaddingMedium);

    auto *versionsTitle = new QLabel("Software Versions", page);
    versionsTitle->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                     .arg(K4Styles::Colors::AccentAmber)
                                     .arg(K4Styles::Dimensions::FontSizePopup));
    layout->addWidget(versionsTitle);

    auto *line2 = new QFrame(page);
    line2->setFrameShape(QFrame::HLine);
    line2->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::DialogBorder));
    line2->setFixedHeight(K4Styles::Dimensions::SeparatorHeight);
    layout->addWidget(line2);

    // Firmware versions list
    if (m_radioState) {
        QMap<QString, QString> versions = m_radioState->firmwareVersions();

        // Component name mappings for readable display
        QMap<QString, QString> componentNames = {
            {"DDC0", "DDC 0"},   {"DDC1", "DDC 1"},    {"DUC", "DUC"},   {"FP", "Front Panel"}, {"DSP", "DSP"},
            {"RFB", "RF Board"}, {"REF", "Reference"}, {"DAP", "DAP"},   {"KSRV", "K Server"},  {"KUI", "K UI"},
            {"KUP", "K Update"}, {"KCFG", "K Config"}, {"R", "Revision"}};

        for (auto it = versions.constBegin(); it != versions.constEnd(); ++it) {
            auto *versionLayout = new QHBoxLayout();

            QString displayName = componentNames.value(it.key(), it.key());
            auto *nameLabel = new QLabel(displayName + ":", page);
            nameLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                         .arg(K4Styles::Colors::TextGray)
                                         .arg(K4Styles::Dimensions::FontSizeButton));
            nameLabel->setFixedWidth(K4Styles::Dimensions::InputFieldWidthMedium);

            auto *valueLabel = new QLabel(it.value(), page);
            valueLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                          .arg(K4Styles::Colors::TextWhite)
                                          .arg(K4Styles::Dimensions::FontSizeButton));

            versionLayout->addWidget(nameLabel);
            versionLayout->addWidget(valueLabel);
            versionLayout->addStretch();
            layout->addLayout(versionLayout);
        }
    } else {
        auto *noDataLabel = new QLabel("Connect to a radio to view version information", page);
        noDataLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-style: italic;")
                                       .arg(K4Styles::Colors::TextGray)
                                       .arg(K4Styles::Dimensions::FontSizeButton));
        layout->addWidget(noDataLabel);
    }

    layout->addStretch();
    return page;
}

QWidget *OptionsDialog::createKpodPage() {
    auto *page = new QWidget(this);
    page->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::Background));

    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(K4Styles::Dimensions::DialogMargin, K4Styles::Dimensions::DialogMargin,
                               K4Styles::Dimensions::DialogMargin, K4Styles::Dimensions::DialogMargin);
    layout->setSpacing(K4Styles::Dimensions::PaddingLarge);

#ifdef Q_OS_ANDROID
    auto *androidTitleLabel = new QLabel("K-Pod", page);
    androidTitleLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                         .arg(K4Styles::Colors::AccentAmber)
                                         .arg(K4Styles::Dimensions::FontSizeTitle));
    layout->addWidget(androidTitleLabel);

    auto *helpLabel = new QLabel("K-Pod USB HID control is not available on Android in this build.", page);
    helpLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                 .arg(K4Styles::Colors::TextGray)
                                 .arg(K4Styles::Dimensions::FontSizeButton));
    helpLabel->setWordWrap(true);
    layout->addWidget(helpLabel);
    layout->addStretch();
    return page;
#endif

    // Status indicator
    auto *statusLayout = new QHBoxLayout();
    auto *statusLabel = new QLabel("Status:", page);
    statusLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                   .arg(K4Styles::Colors::TextGray)
                                   .arg(K4Styles::Dimensions::FontSizePopup));
    statusLabel->setFixedWidth(K4Styles::Dimensions::FormLabelWidth);

    m_kpodStatusLabel = new QLabel("Not Detected", page);
    statusLayout->addWidget(statusLabel);
    statusLayout->addWidget(m_kpodStatusLabel);
    statusLayout->addStretch();
    layout->addLayout(statusLayout);

    // Separator line
    auto *line = new QFrame(page);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::DialogBorder));
    line->setFixedHeight(K4Styles::Dimensions::SeparatorHeight);
    layout->addWidget(line);

    // Device Summary title
    auto *titleLabel = new QLabel("Device Summary", page);
    titleLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                  .arg(K4Styles::Colors::AccentAmber)
                                  .arg(K4Styles::Dimensions::FontSizeTitle));
    layout->addWidget(titleLabel);

    // Device info table using grid layout
    auto *tableWidget = new QWidget(page);
    auto *tableLayout = new QGridLayout(tableWidget);
    tableLayout->setContentsMargins(0, K4Styles::Dimensions::PaddingMedium, 0, K4Styles::Dimensions::PaddingMedium);
    tableLayout->setHorizontalSpacing(K4Styles::Dimensions::DialogMargin);
    tableLayout->setVerticalSpacing(K4Styles::Dimensions::PopupButtonSpacing);

    // Table styling
    QString headerStyle =
        QString("color: %1; font-size: 12px; font-weight: bold; padding: 5px;").arg(K4Styles::Colors::TextGray);

    // Create labels with property names
    QStringList properties = {"Product Name", "Manufacturer",     "Vendor ID", "Product ID",
                              "Device Type",  "Firmware Version", "Device ID"};
    QVector<QLabel **> valueLabels = {&m_kpodProductLabel,   &m_kpodManufacturerLabel, &m_kpodVendorIdLabel,
                                      &m_kpodProductIdLabel, &m_kpodDeviceTypeLabel,   &m_kpodFirmwareLabel,
                                      &m_kpodDeviceIdLabel};

    for (int row = 0; row < properties.size(); ++row) {
        auto *propLabel = new QLabel(properties[row], tableWidget);
        propLabel->setStyleSheet(headerStyle);

        *valueLabels[row] = new QLabel("N/A", tableWidget);

        tableLayout->addWidget(propLabel, row, 0, Qt::AlignLeft);
        tableLayout->addWidget(*valueLabels[row], row, 1, Qt::AlignLeft);
    }

    tableLayout->setColumnStretch(1, 1);
    layout->addWidget(tableWidget);

    // Another separator
    auto *line2 = new QFrame(page);
    line2->setFrameShape(QFrame::HLine);
    line2->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::DialogBorder));
    line2->setFixedHeight(K4Styles::Dimensions::SeparatorHeight);
    layout->addWidget(line2);

    // Enable checkbox
    m_kpodEnableCheckbox = new QCheckBox("Enable K-Pod", page);
    m_kpodEnableCheckbox->setChecked(RadioSettings::instance()->kpodEnabled());

    connect(m_kpodEnableCheckbox, &QCheckBox::toggled, this,
            [](bool checked) { RadioSettings::instance()->setKpodEnabled(checked); });

    layout->addWidget(m_kpodEnableCheckbox);

    // Help text
    m_kpodHelpLabel = new QLabel("Connect a K-Pod device to enable this feature.", page);
    m_kpodHelpLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-style: italic;")
                                       .arg(K4Styles::Colors::TextGray)
                                       .arg(K4Styles::Dimensions::FontSizeLarge));
    m_kpodHelpLabel->setWordWrap(true);
    layout->addWidget(m_kpodHelpLabel);

    layout->addStretch();

    // Initialize with current status
    updateKpodStatus();

    return page;
}

void OptionsDialog::updateKpodStatus() {
    if (!m_kpodDevice || !m_kpodStatusLabel || !m_kpodEnableCheckbox || !m_kpodHelpLabel || !m_kpodProductLabel ||
        !m_kpodManufacturerLabel || !m_kpodVendorIdLabel || !m_kpodProductIdLabel || !m_kpodDeviceTypeLabel ||
        !m_kpodFirmwareLabel || !m_kpodDeviceIdLabel) {
        return;
    }

    KpodDeviceInfo info = m_kpodDevice->deviceInfo();
    bool detected = info.detected;

    // Styling
    QString valueStyle = QString("color: %1; font-size: %2px; padding: 5px;")
                             .arg(K4Styles::Colors::TextWhite)
                             .arg(K4Styles::Dimensions::FontSizeButton);
    QString notDetectedStyle = QString("color: %1; font-size: %2px; font-style: italic; padding: 5px;")
                                   .arg(K4Styles::Colors::TextGray)
                                   .arg(K4Styles::Dimensions::FontSizeButton);

    // Update status label
    QString statusText = detected ? "Detected" : "Not Detected";
    QString statusColor = detected ? K4Styles::Colors::StatusGreen : K4Styles::Colors::ErrorRed;
    m_kpodStatusLabel->setText(statusText);
    m_kpodStatusLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                         .arg(statusColor)
                                         .arg(K4Styles::Dimensions::FontSizePopup));

    // Update device info labels
    auto setLabel = [&](QLabel *label, const QString &value) {
        QString displayValue = value.isEmpty() ? "N/A" : value;
        label->setText(displayValue);
        label->setStyleSheet(displayValue == "N/A" ? notDetectedStyle : valueStyle);
    };

    setLabel(m_kpodProductLabel, detected ? info.productName : "");
    setLabel(m_kpodManufacturerLabel, detected ? info.manufacturer : "");
    setLabel(m_kpodVendorIdLabel,
             detected ? QString("%1 (0x%2)").arg(info.vendorId).arg(info.vendorId, 4, 16, QChar('0')).toUpper() : "");
    setLabel(m_kpodProductIdLabel,
             detected ? QString("%1 (0x%2)").arg(info.productId).arg(info.productId, 4, 16, QChar('0')).toUpper() : "");
    setLabel(m_kpodDeviceTypeLabel, detected ? "USB HID (Human Interface Device)" : "");
    setLabel(m_kpodFirmwareLabel, detected ? info.firmwareVersion : "");
    setLabel(m_kpodDeviceIdLabel, detected ? info.deviceId : "");

    // Update checkbox enabled state and styling
    m_kpodEnableCheckbox->setEnabled(detected);
    if (detected) {
        m_kpodEnableCheckbox->setStyleSheet(QString("QCheckBox { color: %1; font-size: %2px; spacing: %3px; }"
                                                    "QCheckBox::indicator { width: %4px; height: %4px; }")
                                                .arg(K4Styles::Colors::TextWhite)
                                                .arg(K4Styles::Dimensions::FontSizePopup)
                                                .arg(K4Styles::Dimensions::BorderRadiusLarge)
                                                .arg(K4Styles::Dimensions::CheckboxSize));
    } else {
        m_kpodEnableCheckbox->setStyleSheet(QString("QCheckBox { color: %1; font-size: %2px; spacing: %3px; }"
                                                    "QCheckBox::indicator { width: %4px; height: %4px; }")
                                                .arg(K4Styles::Colors::TextGray)
                                                .arg(K4Styles::Dimensions::FontSizePopup)
                                                .arg(K4Styles::Dimensions::BorderRadiusLarge)
                                                .arg(K4Styles::Dimensions::CheckboxSize));
    }

    // Update help text
    m_kpodHelpLabel->setText(detected ? "When enabled, the K-Pod VFO knob and buttons will control the radio."
                                      : "Connect a K-Pod device to enable this feature.");
}

QWidget *OptionsDialog::createAudioInputPage() {
    auto *page = new QWidget(this);
    page->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::Background));

    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(K4Styles::Dimensions::DialogMargin, K4Styles::Dimensions::DialogMargin,
                               K4Styles::Dimensions::DialogMargin, K4Styles::Dimensions::DialogMargin);
    layout->setSpacing(K4Styles::Dimensions::PaddingLarge);

    // Title
    auto *titleLabel = new QLabel("Audio Input", page);
    titleLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                  .arg(K4Styles::Colors::AccentAmber)
                                  .arg(K4Styles::Dimensions::FontSizeTitle));
    layout->addWidget(titleLabel);

    // Separator line
    auto *line = new QFrame(page);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::DialogBorder));
    line->setFixedHeight(K4Styles::Dimensions::SeparatorHeight);
    layout->addWidget(line);

    // === Microphone Device Selection ===
    auto *deviceLabel = new QLabel("Microphone:", page);
    deviceLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                   .arg(K4Styles::Colors::TextGray)
                                   .arg(K4Styles::Dimensions::FontSizePopup));
    layout->addWidget(deviceLabel);

    m_micDeviceCombo = new QComboBox(page);
    m_micDeviceCombo->setStyleSheet(
        QString("QComboBox { background-color: %1; color: %2; border: 1px solid %3; "
                "           padding: %6px; font-size: %5px; border-radius: %7px; }"
                "QComboBox:focus { border-color: %4; }"
                "QComboBox::drop-down { border: none; width: 20px; }"
                "QComboBox::down-arrow { image: none; border-left: 5px solid transparent; "
                "           border-right: 5px solid transparent; border-top: 5px solid %2; }"
                "QComboBox QAbstractItemView { background-color: %1; color: %2; selection-background-color: %4; }")
            .arg(K4Styles::Colors::DarkBackground, K4Styles::Colors::TextWhite, K4Styles::Colors::DialogBorder,
                 K4Styles::Colors::AccentAmber)
            .arg(K4Styles::Dimensions::FontSizePopup)
            .arg(K4Styles::Dimensions::PaddingSmall)
            .arg(K4Styles::Dimensions::SliderBorderRadius));
    populateMicDevices();
    connect(m_micDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &OptionsDialog::onMicDeviceChanged);
    layout->addWidget(m_micDeviceCombo);

    layout->addSpacing(K4Styles::Dimensions::PaddingMedium);

    // === Microphone Gain ===
    auto *gainLayout = new QHBoxLayout();
    auto *gainLabel = new QLabel("Mic Gain:", page);
    gainLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                 .arg(K4Styles::Colors::TextGray)
                                 .arg(K4Styles::Dimensions::FontSizePopup));
    gainLabel->setFixedWidth(K4Styles::Dimensions::FormLabelWidth);
    gainLayout->addWidget(gainLabel);

    m_micGainSlider = new QSlider(Qt::Horizontal, page);
    m_micGainSlider->setRange(0, 100);
    m_micGainSlider->setValue(RadioSettings::instance()->micGain());
    m_micGainSlider->setStyleSheet(
        K4Styles::sliderHorizontal(K4Styles::Colors::TextDark, K4Styles::Colors::AccentAmber));
    connect(m_micGainSlider, &QSlider::valueChanged, this, &OptionsDialog::onMicGainChanged);
    gainLayout->addWidget(m_micGainSlider, 1);

    m_micGainValueLabel = new QLabel(QString("%1%").arg(m_micGainSlider->value()), page);
    m_micGainValueLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                           .arg(K4Styles::Colors::TextWhite)
                                           .arg(K4Styles::Dimensions::FontSizePopup));
    m_micGainValueLabel->setFixedWidth(K4Styles::Dimensions::SliderValueLabelWidth);
    m_micGainValueLabel->setAlignment(Qt::AlignRight);
    gainLayout->addWidget(m_micGainValueLabel);

    layout->addLayout(gainLayout);

    auto *gainHelpLabel = new QLabel("Adjust the microphone input level. 50% is unity gain.", page);
    gainHelpLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-style: italic;")
                                     .arg(K4Styles::Colors::TextGray)
                                     .arg(K4Styles::Dimensions::FontSizeLarge));
    layout->addWidget(gainHelpLabel);

    layout->addSpacing(K4Styles::Dimensions::PaddingLarge);

    // === Microphone Test Section ===
    auto *line2 = new QFrame(page);
    line2->setFrameShape(QFrame::HLine);
    line2->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::DialogBorder));
    line2->setFixedHeight(K4Styles::Dimensions::SeparatorHeight);
    layout->addWidget(line2);

    auto *testSectionLabel = new QLabel("Microphone Test", page);
    testSectionLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                        .arg(K4Styles::Colors::TextWhite)
                                        .arg(K4Styles::Dimensions::FontSizePopup));
    layout->addWidget(testSectionLabel);

    auto *testHelpLabel =
        new QLabel("Click the Test button to activate the microphone and check the input level.", page);
    testHelpLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                     .arg(K4Styles::Colors::TextGray)
                                     .arg(K4Styles::Dimensions::FontSizeButton));
    testHelpLabel->setWordWrap(true);
    layout->addWidget(testHelpLabel);

    layout->addSpacing(5); // Small gap before meter

    // Mic Level Meter
    auto *meterLayout = new QHBoxLayout();
    auto *meterLabel = new QLabel("Level:", page);
    meterLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                  .arg(K4Styles::Colors::TextGray)
                                  .arg(K4Styles::Dimensions::FontSizePopup));
    meterLabel->setFixedWidth(50);
    meterLayout->addWidget(meterLabel);

    m_micMeter = new MicMeterWidget(page);
    meterLayout->addWidget(m_micMeter, 1);

    layout->addLayout(meterLayout);

    layout->addSpacing(K4Styles::Dimensions::PaddingMedium);

    // Test Button
    m_micTestBtn = new QPushButton("Test Microphone", page);
    m_micTestBtn->setCheckable(true);
    m_micTestBtn->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; border: 1px solid %3; "
                                        "             padding: 10px 20px; font-size: %5px; border-radius: 4px; }"
                                        "QPushButton:hover { background-color: %6; }"
                                        "QPushButton:checked { background-color: %4; color: %1; border-color: %4; }")
                                    .arg(K4Styles::Colors::DarkBackground, K4Styles::Colors::TextWhite,
                                         K4Styles::Colors::DialogBorder, K4Styles::Colors::AccentAmber)
                                    .arg(K4Styles::Dimensions::FontSizePopup)
                                    .arg(K4Styles::Colors::GradientBottom));
    connect(m_micTestBtn, &QPushButton::toggled, this, &OptionsDialog::onMicTestToggled);
    layout->addWidget(m_micTestBtn);

    // Connect to AudioEngine for mic level updates (queued — AudioEngine lives on audio thread)
    if (m_audioEngine) {
        connect(m_audioEngine, &AudioEngine::micLevelChanged, this, &OptionsDialog::onMicLevelChanged,
                Qt::QueuedConnection);
    }

    layout->addStretch();
    return page;
}

void OptionsDialog::populateMicDevices() {
    if (!m_micDeviceCombo)
        return;

    m_micDeviceCombo->clear();

    auto devices = AudioEngine::availableInputDevices();
    QString savedDevice = RadioSettings::instance()->micDevice();
    int selectedIndex = 0;

    for (int i = 0; i < devices.size(); i++) {
        const auto &device = devices[i];
        m_micDeviceCombo->addItem(device.second, device.first);

        // Find the saved device
        if (device.first == savedDevice) {
            selectedIndex = i;
        }
    }

    m_micDeviceCombo->setCurrentIndex(selectedIndex);
}

void OptionsDialog::onMicDeviceChanged(int index) {
    if (!m_micDeviceCombo || index < 0)
        return;

    QString deviceId = m_micDeviceCombo->currentData().toString();
    RadioSettings::instance()->setMicDevice(deviceId);

    if (m_audioEngine) {
        QMetaObject::invokeMethod(m_audioEngine, "setMicDevice", Qt::QueuedConnection, Q_ARG(QString, deviceId));
    }
}

void OptionsDialog::onMicGainChanged(int value) {
    if (m_micGainValueLabel) {
        m_micGainValueLabel->setText(QString("%1%").arg(value));
    }

    RadioSettings::instance()->setMicGain(value);

    if (m_audioEngine) {
        m_audioEngine->setMicGain(value / 100.0f);
    }
}

void OptionsDialog::onMicTestToggled(bool checked) {
#ifdef Q_OS_ANDROID
    if (checked) {
        QMicrophonePermission permission;
        const Qt::PermissionStatus status = qApp->checkPermission(permission);

        if (status == Qt::PermissionStatus::Undetermined) {
            QPointer<OptionsDialog> safeThis(this);
            qApp->requestPermission(permission, this, [safeThis](const QPermission &result) {
                if (result.status() == Qt::PermissionStatus::Denied && safeThis) {
                    showInWindowMessage(safeThis, "Microphone Permission Required",
                                        "Grant microphone permission to use microphone test.");
                }
            });
            if (m_micTestBtn) {
                QSignalBlocker block(m_micTestBtn);
                m_micTestBtn->setChecked(false);
                m_micTestBtn->setText("Test Microphone");
            }
            return;
        }

        if (status == Qt::PermissionStatus::Denied) {
            showInWindowMessage(this, "Microphone Permission Required",
                                "Microphone permission is currently denied. Enable it in Android Settings.");
            if (m_micTestBtn) {
                QSignalBlocker block(m_micTestBtn);
                m_micTestBtn->setChecked(false);
                m_micTestBtn->setText("Test Microphone");
            }
            return;
        }
    }
#endif

    m_micTestActive = checked;

    if (m_micTestBtn) {
        m_micTestBtn->setText(checked ? "Stop Test" : "Test Microphone");
    }

    if (m_audioEngine) {
        QMetaObject::invokeMethod(m_audioEngine, "setMicEnabled", Qt::QueuedConnection, Q_ARG(bool, checked));
    }

    // Reset meter when stopping
    if (!checked && m_micMeter) {
        m_micMeter->setLevel(0.0f);
    }
}

void OptionsDialog::onMicLevelChanged(float level) {
    if (m_micTestActive && m_micMeter) {
        // Scale level for better visualization (RMS tends to be low)
        float scaledLevel = qBound(0.0f, level * 5.0f, 1.0f);
        m_micMeter->setLevel(scaledLevel);
    }
}

QWidget *OptionsDialog::createAudioOutputPage() {
    auto *page = new QWidget(this);
    page->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::Background));

    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(K4Styles::Dimensions::DialogMargin, K4Styles::Dimensions::DialogMargin,
                               K4Styles::Dimensions::DialogMargin, K4Styles::Dimensions::DialogMargin);
    layout->setSpacing(K4Styles::Dimensions::PaddingLarge);

    // Title
    auto *titleLabel = new QLabel("Audio Output", page);
    titleLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                  .arg(K4Styles::Colors::AccentAmber)
                                  .arg(K4Styles::Dimensions::FontSizeTitle));
    layout->addWidget(titleLabel);

    // Separator line
    auto *line = new QFrame(page);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::DialogBorder));
    line->setFixedHeight(K4Styles::Dimensions::SeparatorHeight);
    layout->addWidget(line);

    // === Speaker Device Selection ===
    auto *deviceLabel = new QLabel("Speaker:", page);
    deviceLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                   .arg(K4Styles::Colors::TextGray)
                                   .arg(K4Styles::Dimensions::FontSizePopup));
    layout->addWidget(deviceLabel);

    m_speakerDeviceCombo = new QComboBox(page);
    m_speakerDeviceCombo->setStyleSheet(
        QString("QComboBox { background-color: %1; color: %2; border: 1px solid %3; "
                "           padding: %6px; font-size: %5px; border-radius: %7px; }"
                "QComboBox:focus { border-color: %4; }"
                "QComboBox::drop-down { border: none; width: 20px; }"
                "QComboBox::down-arrow { image: none; border-left: 5px solid transparent; "
                "           border-right: 5px solid transparent; border-top: 5px solid %2; }"
                "QComboBox QAbstractItemView { background-color: %1; color: %2; selection-background-color: %4; }")
            .arg(K4Styles::Colors::DarkBackground, K4Styles::Colors::TextWhite, K4Styles::Colors::DialogBorder,
                 K4Styles::Colors::AccentAmber)
            .arg(K4Styles::Dimensions::FontSizePopup)
            .arg(K4Styles::Dimensions::PaddingSmall)
            .arg(K4Styles::Dimensions::SliderBorderRadius));
    populateSpeakerDevices();
    connect(m_speakerDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &OptionsDialog::onSpeakerDeviceChanged);
    layout->addWidget(m_speakerDeviceCombo);

    layout->addSpacing(K4Styles::Dimensions::PaddingMedium);

    // Help text
    auto *helpLabel = new QLabel("Select the audio output device for radio receive audio. "
                                 "Volume is controlled by the MAIN and SUB sliders on the side panel.",
                                 page);
    helpLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-style: italic;")
                                 .arg(K4Styles::Colors::TextGray)
                                 .arg(K4Styles::Dimensions::FontSizeLarge));
    helpLabel->setWordWrap(true);
    layout->addWidget(helpLabel);

    layout->addStretch();
    return page;
}

void OptionsDialog::populateSpeakerDevices() {
    if (!m_speakerDeviceCombo)
        return;

    m_speakerDeviceCombo->clear();

    auto devices = AudioEngine::availableOutputDevices();
    QString savedDevice = RadioSettings::instance()->speakerDevice();
    int selectedIndex = 0;

    for (int i = 0; i < devices.size(); i++) {
        const auto &device = devices[i];
        m_speakerDeviceCombo->addItem(device.second, device.first);

        // Find the saved device
        if (device.first == savedDevice) {
            selectedIndex = i;
        }
    }

    m_speakerDeviceCombo->setCurrentIndex(selectedIndex);
}

void OptionsDialog::onSpeakerDeviceChanged(int index) {
    if (!m_speakerDeviceCombo || index < 0)
        return;

    QString deviceId = m_speakerDeviceCombo->currentData().toString();
    RadioSettings::instance()->setSpeakerDevice(deviceId);

    if (m_audioEngine) {
        QMetaObject::invokeMethod(m_audioEngine, "setOutputDevice", Qt::QueuedConnection, Q_ARG(QString, deviceId));
    }
}

QWidget *OptionsDialog::createRigControlPage() {
    auto *page = new QWidget(this);
    page->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::Background));

    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(K4Styles::Dimensions::DialogMargin, K4Styles::Dimensions::DialogMargin,
                               K4Styles::Dimensions::DialogMargin, K4Styles::Dimensions::DialogMargin);
    layout->setSpacing(K4Styles::Dimensions::PaddingLarge);

    // Title
    auto *titleLabel = new QLabel("CAT Server", page);
    titleLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                  .arg(K4Styles::Colors::AccentAmber)
                                  .arg(K4Styles::Dimensions::FontSizeTitle));
    layout->addWidget(titleLabel);

    // Description
    auto *descLabel = new QLabel("Enable the CAT server to allow external applications (WSJT-X, MacLoggerDX, fldigi) "
                                 "to connect using their native Elecraft K4 support. No protocol translation needed.",
                                 page);
    descLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                 .arg(K4Styles::Colors::TextGray)
                                 .arg(K4Styles::Dimensions::FontSizeButton));
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    // Separator line
    auto *line = new QFrame(page);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::DialogBorder));
    line->setFixedHeight(K4Styles::Dimensions::SeparatorHeight);
    layout->addWidget(line);

    // Status indicator
    auto *statusLayout = new QHBoxLayout();
    auto *statusTitleLabel = new QLabel("Status:", page);
    statusTitleLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                        .arg(K4Styles::Colors::TextGray)
                                        .arg(K4Styles::Dimensions::FontSizePopup));
    statusTitleLabel->setFixedWidth(K4Styles::Dimensions::FormLabelWidth);

    m_catServerStatusLabel = new QLabel("Not running", page);
    m_catServerStatusLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                              .arg(K4Styles::Colors::ErrorRed)
                                              .arg(K4Styles::Dimensions::FontSizePopup));

    statusLayout->addWidget(statusTitleLabel);
    statusLayout->addWidget(m_catServerStatusLabel);
    statusLayout->addStretch();
    layout->addLayout(statusLayout);

    // Clients indicator
    auto *clientsLayout = new QHBoxLayout();
    auto *clientsTitleLabel = new QLabel("Clients:", page);
    clientsTitleLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                         .arg(K4Styles::Colors::TextGray)
                                         .arg(K4Styles::Dimensions::FontSizePopup));
    clientsTitleLabel->setFixedWidth(K4Styles::Dimensions::FormLabelWidth);

    m_catServerClientsLabel = new QLabel("0 connected", page);
    m_catServerClientsLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                               .arg(K4Styles::Colors::TextWhite)
                                               .arg(K4Styles::Dimensions::FontSizePopup));

    clientsLayout->addWidget(clientsTitleLabel);
    clientsLayout->addWidget(m_catServerClientsLabel);
    clientsLayout->addStretch();
    layout->addLayout(clientsLayout);

    // Separator line
    auto *line2 = new QFrame(page);
    line2->setFrameShape(QFrame::HLine);
    line2->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::DialogBorder));
    line2->setFixedHeight(K4Styles::Dimensions::SeparatorHeight);
    layout->addWidget(line2);

    // Connection Settings section
    auto *sectionLabel = new QLabel("Settings", page);
    sectionLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                    .arg(K4Styles::Colors::TextWhite)
                                    .arg(K4Styles::Dimensions::FontSizePopup));
    layout->addWidget(sectionLabel);

    // Port input
    auto *portLayout = new QHBoxLayout();
    auto *portLabel = new QLabel("Port:", page);
    portLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                 .arg(K4Styles::Colors::TextGray)
                                 .arg(K4Styles::Dimensions::FontSizePopup));
    portLabel->setFixedWidth(K4Styles::Dimensions::FormLabelWidth);

    m_catServerPortEdit = new QLineEdit(page);
    m_catServerPortEdit->setPlaceholderText("9299");
    m_catServerPortEdit->setFixedWidth(K4Styles::Dimensions::InputFieldWidthSmall);
    m_catServerPortEdit->setStyleSheet(QString("QLineEdit { background-color: %1; color: %2; border: 1px solid %3; "
                                               "           padding: %6px; font-size: %5px; border-radius: %7px; }"
                                               "QLineEdit:focus { border-color: %4; }")
                                           .arg(K4Styles::Colors::DarkBackground, K4Styles::Colors::TextWhite,
                                                K4Styles::Colors::DialogBorder, K4Styles::Colors::AccentAmber)
                                           .arg(K4Styles::Dimensions::FontSizePopup)
                                           .arg(K4Styles::Dimensions::PaddingSmall)
                                           .arg(K4Styles::Dimensions::SliderBorderRadius));
    m_catServerPortEdit->setText(QString::number(RadioSettings::instance()->catServerPort()));

    auto *portHint = new QLabel("(default: 9299)", page);
    portHint->setStyleSheet(QString("color: %1; font-size: %2px;")
                                .arg(K4Styles::Colors::TextGray)
                                .arg(K4Styles::Dimensions::FontSizeLarge));

    portLayout->addWidget(portLabel);
    portLayout->addWidget(m_catServerPortEdit);
    portLayout->addWidget(portHint);
    portLayout->addStretch();
    layout->addLayout(portLayout);

    // Separator line
    auto *line3 = new QFrame(page);
    line3->setFrameShape(QFrame::HLine);
    line3->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::DialogBorder));
    line3->setFixedHeight(K4Styles::Dimensions::SeparatorHeight);
    layout->addWidget(line3);

    // Enable checkbox
    m_catServerEnableCheckbox = new QCheckBox("Enable CAT server", page);
    m_catServerEnableCheckbox->setStyleSheet(QString("QCheckBox { color: %1; font-size: %2px; spacing: %3px; }"
                                                     "QCheckBox::indicator { width: %4px; height: %4px; }")
                                                 .arg(K4Styles::Colors::TextWhite)
                                                 .arg(K4Styles::Dimensions::FontSizePopup)
                                                 .arg(K4Styles::Dimensions::BorderRadiusLarge)
                                                 .arg(K4Styles::Dimensions::CheckboxSize));
    m_catServerEnableCheckbox->setChecked(RadioSettings::instance()->catServerEnabled());
    layout->addWidget(m_catServerEnableCheckbox);

    // Help text
    auto *helpLabel = new QLabel("Configure external apps to use Elecraft K4, host 127.0.0.1, and the port above. "
                                 "Commands are forwarded to the real K4.",
                                 page);
    helpLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-style: italic;")
                                 .arg(K4Styles::Colors::TextGray)
                                 .arg(K4Styles::Dimensions::FontSizeLarge));
    helpLabel->setWordWrap(true);
    layout->addWidget(helpLabel);

    // Connect signals to save settings
    connect(m_catServerPortEdit, &QLineEdit::editingFinished, this, [this]() {
        bool ok;
        quint16 port = m_catServerPortEdit->text().toUShort(&ok);
        if (ok && port >= 1024) {
            RadioSettings::instance()->setCatServerPort(port);
        } else {
            // Reset to current value if invalid
            m_catServerPortEdit->setText(QString::number(RadioSettings::instance()->catServerPort()));
        }
    });

    connect(m_catServerEnableCheckbox, &QCheckBox::toggled, this,
            [](bool checked) { RadioSettings::instance()->setCatServerEnabled(checked); });

    layout->addStretch();

    // Initialize status display
    updateCatServerStatus();

    return page;
}

void OptionsDialog::updateCatServerStatus() {
    if (!m_catServerStatusLabel || !m_catServerClientsLabel) {
        return;
    }

    bool isListening = m_catServer && m_catServer->isListening();

    if (isListening) {
        m_catServerStatusLabel->setText(QString("Listening on port %1").arg(m_catServer->port()));
        m_catServerStatusLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                                  .arg(K4Styles::Colors::StatusGreen)
                                                  .arg(K4Styles::Dimensions::FontSizePopup));
    } else {
        m_catServerStatusLabel->setText("Not running");
        m_catServerStatusLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                                  .arg(K4Styles::Colors::ErrorRed)
                                                  .arg(K4Styles::Dimensions::FontSizePopup));
    }

    int clientCount = m_catServer ? m_catServer->clientCount() : 0;
    m_catServerClientsLabel->setText(QString("%1 connected").arg(clientCount));
}

QWidget *OptionsDialog::createCwKeyerPage() {
    auto *page = new QWidget(this);
#ifndef Q_OS_ANDROID
    page->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::Background));
#endif

    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(K4Styles::Dimensions::DialogMargin, K4Styles::Dimensions::DialogMargin,
                               K4Styles::Dimensions::DialogMargin, K4Styles::Dimensions::DialogMargin);
    layout->setSpacing(K4Styles::Dimensions::PaddingLarge);

#ifdef Q_OS_ANDROID
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);
    auto *androidTitleLabel = new QLabel("CW Keyer", page);
    androidTitleLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                         .arg(K4Styles::Colors::AccentAmber)
                                         .arg(K4Styles::Dimensions::FontSizeTitle));
    layout->addWidget(androidTitleLabel);

    auto *helpLabel = new QLabel("Connect a USB MIDI paddle interface or make a Bluetooth LE MIDI device "
                                 "discoverable, then tap SCAN. USB and BLE MIDI devices appear together below.", page);
    helpLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                 .arg(K4Styles::Colors::TextGray)
                                 .arg(K4Styles::Dimensions::FontSizeButton));
    helpLabel->setWordWrap(true);
    layout->addWidget(helpLabel);

    m_cwKeyerDeviceTypeCombo = new QComboBox(page);
    m_cwKeyerDeviceTypeCombo->addItem("Android MIDI", 1);
    m_cwKeyerDeviceTypeCombo->hide();

    auto *androidStatusLayout = new QHBoxLayout();
    auto *statusTitle = new QLabel("Status:", page);
    statusTitle->setStyleSheet(QString("color: %1; font-size: %2px;")
                                   .arg(K4Styles::Colors::TextGray)
                                   .arg(K4Styles::Dimensions::FontSizePopup));
    m_cwKeyerStatusLabel = new QLabel("Not connected", page);
    androidStatusLayout->addWidget(statusTitle);
    androidStatusLayout->addWidget(m_cwKeyerStatusLabel, 1);
    layout->addLayout(androidStatusLayout);

    auto *deviceLayout = new QHBoxLayout();
    deviceLayout->setSpacing(8);
    m_cwKeyerPortCombo = new QComboBox(page);
    m_cwKeyerPortCombo->setMinimumHeight(42);
    m_cwKeyerPortCombo->setStyleSheet(QString(
        "QComboBox { background-color: %1; color: %2; border: 1px solid %3; padding: 8px; font-size: %4px; }"
        "QComboBox QAbstractItemView { background-color: %1; color: %2; selection-background-color: %5; }")
        .arg(K4Styles::Colors::DarkBackground, K4Styles::Colors::TextWhite,
             K4Styles::Colors::DialogBorder)
        .arg(K4Styles::Dimensions::FontSizePopup)
        .arg(K4Styles::Colors::AccentAmber));
    m_cwKeyerRefreshBtn = new QPushButton("SCAN", page);
    m_cwKeyerRefreshBtn->setFixedSize(110, 42);
    m_cwKeyerRefreshBtn->setStyleSheet(K4Styles::menuBarButton());
    connect(m_cwKeyerRefreshBtn, &QPushButton::clicked, this, &OptionsDialog::onCwKeyerRefreshClicked);
    deviceLayout->addWidget(m_cwKeyerPortCombo, 1);
    deviceLayout->addWidget(m_cwKeyerRefreshBtn);

    m_cwKeyerConnectBtn = new QPushButton("CONNECT", page);
    m_cwKeyerConnectBtn->setFixedSize(140, 42);
    m_cwKeyerConnectBtn->setStyleSheet(K4Styles::menuBarButton());
    connect(m_cwKeyerConnectBtn, &QPushButton::clicked, this, &OptionsDialog::onCwKeyerConnectClicked);
    deviceLayout->addWidget(m_cwKeyerConnectBtn);
    layout->addLayout(deviceLayout);

    auto *controlsScroll = new QScrollArea(page);
    m_cwControlsScroll = controlsScroll;
    controlsScroll->setWidgetResizable(true);
    controlsScroll->setFrameShape(QFrame::NoFrame);
    controlsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Scrolling is touch-driven. Hiding the bar prevents Android's overlay
    // scrollbar from covering the rightmost controls.
    controlsScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    controlsScroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    auto *controlsWidget = new QWidget(controlsScroll);
    controlsWidget->setStyleSheet("background: transparent;");
    auto *controlsLayout = new QVBoxLayout(controlsWidget);
    controlsWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    controlsLayout->setContentsMargins(0, 6, 18, 6);
    controlsLayout->setSpacing(8);
    controlsScroll->setWidget(controlsWidget);
    layout->addWidget(controlsScroll, 1);

    auto *mappingLayout = new QHBoxLayout();
    mappingLayout->setSpacing(6);
    auto *mappingTitle = new QLabel("MIDI profile:", page);
    mappingTitle->setFixedWidth(135);
    mappingTitle->setStyleSheet(QString("color:%1;font-size:%2px;")
                                    .arg(K4Styles::Colors::TextGray)
                                    .arg(K4Styles::Dimensions::FontSizePopup));
    m_midiMappingProfileCombo = new QComboBox(page);
    m_midiMappingProfileCombo->setMinimumHeight(42);
    m_midiMappingProfileCombo->addItem("TinyMIDI", 0);
    m_midiMappingProfileCombo->addItem("HaliKey MIDI", 1);
    m_midiMappingProfileCombo->addItem("Custom / Learn", 2);
    m_midiMappingProfileCombo->setCurrentIndex(RadioSettings::instance()->midiMappingProfile());
    m_midiMappingProfileCombo->hide();
    auto *tinyMidiProfileButton = new QPushButton("TinyMIDI", page);
    auto *haliKeyProfileButton = new QPushButton("HaliKey", page);
    auto *customProfileButton = new QPushButton("CUSTOM", page);
    for (auto *button : {tinyMidiProfileButton, haliKeyProfileButton, customProfileButton}) {
        button->setMinimumSize(105, 38);
        button->setStyleSheet(K4Styles::menuBarButton());
    }
    m_midiLearnDitButton = new QPushButton("LEARN DIT", page);
    m_midiLearnDahButton = new QPushButton("LEARN DAH", page);
    for (auto *button : {m_midiLearnDitButton, m_midiLearnDahButton}) {
        button->setFixedSize(125, 38);
        button->setStyleSheet(K4Styles::menuBarButton());
    }
    mappingLayout->addWidget(mappingTitle);
    mappingLayout->addWidget(tinyMidiProfileButton);
    mappingLayout->addWidget(haliKeyProfileButton);
    mappingLayout->addWidget(customProfileButton);
    mappingLayout->addStretch(1);
    controlsLayout->addLayout(mappingLayout);

    auto *learnLayout = new QHBoxLayout();
    learnLayout->setSpacing(6);
    learnLayout->addSpacing(141);
    learnLayout->addWidget(m_midiLearnDitButton);
    learnLayout->addWidget(m_midiLearnDahButton);
    learnLayout->addStretch(1);
    controlsLayout->addLayout(learnLayout);

    // Add straight-key/external-keyer support without replacing any part of
    // the proven profile/learn controls. These are two-state touch buttons,
    // avoiding another native combo popup on Android.
    auto *keyingLayout = new QHBoxLayout();
    keyingLayout->setSpacing(6);
    auto *keyingTitle = new QLabel("Key input:", page);
    keyingTitle->setFixedWidth(135);
    keyingTitle->setStyleSheet(QString("color:%1;font-size:%2px;")
                                   .arg(K4Styles::Colors::TextGray)
                                   .arg(K4Styles::Dimensions::FontSizePopup));
    auto *keyingModeButton = new QPushButton(page);
    auto *straightInputButton = new QPushButton(page);
    for (QPushButton *button : {keyingModeButton, straightInputButton}) {
        button->setMinimumHeight(38);
        button->setStyleSheet(K4Styles::menuBarButton());
    }
    keyingModeButton->setMinimumWidth(190);
    straightInputButton->setMinimumWidth(170);
    const auto updateKeyingButtons = [keyingModeButton, straightInputButton]() {
        auto *settings = RadioSettings::instance();
        const bool straight = settings->cwMidiKeyingMode() == 1;
        keyingModeButton->setText(straight ? "STRAIGHT KEY / KEYER" : "IAMBIC PADDLES");
        straightInputButton->setText(settings->cwMidiStraightKeyInput() == 0
                                         ? "KEY = LEFT / TIP" : "KEY = RIGHT / RING");
        straightInputButton->setVisible(straight);
        keyingModeButton->setStyleSheet(straight ? K4Styles::menuBarButtonActive()
                                                 : K4Styles::menuBarButton());
    };
    updateKeyingButtons();
    connect(keyingModeButton, &QPushButton::clicked, this,
            [this, updateKeyingButtons]() {
        // Disconnect first so a held paddle cannot remain logically down
        // while the meaning of its physical input changes.
        if (m_halikeyDevice && m_halikeyDevice->isConnected())
            m_halikeyDevice->closePort();
        auto *settings = RadioSettings::instance();
        settings->setCwMidiKeyingMode(settings->cwMidiKeyingMode() == 0 ? 1 : 0);
        updateKeyingButtons();
    });
    connect(straightInputButton, &QPushButton::clicked, this,
            [this, updateKeyingButtons]() {
        if (m_halikeyDevice && m_halikeyDevice->isConnected())
            m_halikeyDevice->closePort();
        auto *settings = RadioSettings::instance();
        settings->setCwMidiStraightKeyInput(settings->cwMidiStraightKeyInput() == 0 ? 1 : 0);
        updateKeyingButtons();
    });
    keyingLayout->addWidget(keyingTitle);
    keyingLayout->addWidget(keyingModeButton);
    keyingLayout->addWidget(straightInputButton);
    keyingLayout->addStretch(1);
    controlsLayout->addLayout(keyingLayout);

    auto *keyingHelp = new QLabel(
        "Straight key / external keyer preserves key-down and key-up timing. "
        "Changing key mode disconnects the CW MIDI device; reconnect after changing it.", page);
    keyingHelp->setWordWrap(true);
    keyingHelp->setStyleSheet(QString("color:%1;font-size:%2px;font-style:italic;")
                                  .arg(K4Styles::Colors::TextGray)
                                  .arg(K4Styles::Dimensions::FontSizeLarge));
    controlsLayout->addWidget(keyingHelp);

    const auto updateMidiProfileControls = [this, tinyMidiProfileButton, haliKeyProfileButton,
                                            customProfileButton]() {
        const int profile = m_midiMappingProfileCombo->currentData().toInt();
        const bool custom = profile == 2;
        m_midiLearnDitButton->setVisible(custom);
        m_midiLearnDahButton->setVisible(custom);
        const auto setSelected = [](QPushButton *button, bool selected) {
            button->setStyleSheet(selected ? K4Styles::menuBarButtonActive()
                                           : K4Styles::menuBarButton());
        };
        setSelected(tinyMidiProfileButton, profile == 0);
        setSelected(haliKeyProfileButton, profile == 1);
        setSelected(customProfileButton, custom);
        if (m_midiMappingLabel) {
            if (custom) {
                auto *s = RadioSettings::instance();
                m_midiMappingLabel->setText(QString("MIDI mapping: DIT %1/%2, DAH %3/%4")
                    .arg(s->midiDitStatus(), 2, 16, QChar('0')).arg(s->midiDitData1())
                    .arg(s->midiDahStatus(), 2, 16, QChar('0')).arg(s->midiDahData1()).toUpper());
            } else {
                m_midiMappingLabel->setText("MIDI mapping: note 20 = DIT, note 21 = DAH");
            }
        }
    };
    connect(tinyMidiProfileButton, &QPushButton::clicked, m_midiMappingProfileCombo,
            [this]() { m_midiMappingProfileCombo->setCurrentIndex(0); });
    connect(haliKeyProfileButton, &QPushButton::clicked, m_midiMappingProfileCombo,
            [this]() { m_midiMappingProfileCombo->setCurrentIndex(1); });
    connect(customProfileButton, &QPushButton::clicked, m_midiMappingProfileCombo,
            [this]() { m_midiMappingProfileCombo->setCurrentIndex(2); });
    connect(m_midiMappingProfileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, updateMidiProfileControls](int) {
                RadioSettings::instance()->setMidiMappingProfile(m_midiMappingProfileCombo->currentData().toInt());
                m_midiLearningTarget = 0;
                updateMidiProfileControls();
            });
    connect(m_midiLearnDitButton, &QPushButton::clicked, this, [this]() {
        m_midiLearningTarget = 1;
        m_cwKeyerStatusLabel->setText("Press and release the DIT paddle now");
    });
    connect(m_midiLearnDahButton, &QPushButton::clicked, this, [this]() {
        m_midiLearningTarget = 2;
        m_cwKeyerStatusLabel->setText("Press and release the DAH paddle now");
    });

    auto *speedLayout = new QHBoxLayout();
    speedLayout->setSpacing(8);
    auto *speedTitle = new QLabel("CW speed:", page);
    speedTitle->setFixedWidth(135);
    speedTitle->setStyleSheet(QString("color: %1; font-size: %2px;")
                                  .arg(K4Styles::Colors::TextGray)
                                  .arg(K4Styles::Dimensions::FontSizePopup));
    auto *speedDown = new QPushButton(QString(QChar(0x2212)), page);
    auto *speedUp = new QPushButton("+", page);
    for (auto *button : {speedDown, speedUp}) {
        button->setFixedSize(42, 38);
        button->setStyleSheet(K4Styles::menuBarButton());
    }
    m_cwSpeedSlider = new QSlider(Qt::Horizontal, page);
    m_cwSpeedSlider->setRange(8, 40);
    m_cwSpeedSlider->setSingleStep(1);
    m_cwSpeedSlider->setPageStep(5);
    m_cwSpeedSlider->setTracking(true);
    m_cwSpeedSlider->setFixedWidth(210);
    const int currentWpm = RadioSettings::instance()->cwKeyerSpeed();
    m_cwSpeedSlider->setValue(currentWpm);
    m_cwSpeedSlider->setStyleSheet(
        K4Styles::sliderHorizontal(K4Styles::Colors::DarkBackground, K4Styles::Colors::AccentAmber));
    // A drag that begins on a slider belongs exclusively to that slider. Do
    // not allow an unhandled/cancelled mouse event to bubble to the kinetic
    // scroll viewport and turn a horizontal adjustment into vertical scroll.
    m_cwSpeedSlider->setAttribute(Qt::WA_NoMousePropagation);
    m_cwSpeedSlider->installEventFilter(this);
    m_cwSpeedValueLabel = new QLabel(QString("%1 WPM").arg(currentWpm), page);
    m_cwSpeedValueLabel->setAlignment(Qt::AlignCenter);
    m_cwSpeedValueLabel->setMinimumWidth(110);
    m_cwSpeedValueLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                           .arg(K4Styles::Colors::AccentAmber)
                                           .arg(K4Styles::Dimensions::FontSizePopup));
    speedLayout->addWidget(speedTitle);
    speedLayout->addWidget(speedDown);
    speedLayout->addWidget(m_cwSpeedSlider, 1);
    speedLayout->addWidget(m_cwSpeedValueLabel);
    speedLayout->addWidget(speedUp);
    speedLayout->addStretch(1);
    controlsLayout->addLayout(speedLayout);

    auto *speedHelp = new QLabel("Built-in touch paddles: up to 30 WPM. External paddle/key: up to 40 WPM.", page);
    speedHelp->setWordWrap(true);
    speedHelp->setStyleSheet(QString("color: %1; font-size: %2px; font-style: italic;")
                                 .arg(K4Styles::Colors::TextGray)
                                 .arg(K4Styles::Dimensions::FontSizeLarge));
    controlsLayout->addWidget(speedHelp);

    connect(speedDown, &QPushButton::clicked, this, [this]() {
        m_cwSpeedSlider->setValue(m_cwSpeedSlider->value() - 1);
        emit keyerSpeedRequested(m_cwSpeedSlider->value());
    });
    connect(speedUp, &QPushButton::clicked, this, [this]() {
        m_cwSpeedSlider->setValue(m_cwSpeedSlider->value() + 1);
        emit keyerSpeedRequested(m_cwSpeedSlider->value());
    });
    connect(m_cwSpeedSlider, &QSlider::valueChanged, this, [this](int wpm) {
        m_cwSpeedValueLabel->setText(QString("%1 WPM").arg(wpm));
        RadioSettings::instance()->setCwKeyerSpeed(wpm);
    });
    if (m_radioState) {
        connect(m_radioState, &RadioState::keyerSpeedChanged, m_cwSpeedSlider, [this](int wpm) {
            if (m_touchSlider != m_cwSpeedSlider && wpm >= m_cwSpeedSlider->minimum() &&
                wpm <= m_cwSpeedSlider->maximum()) {
                QSignalBlocker blocker(m_cwSpeedSlider);
                m_cwSpeedSlider->setValue(wpm);
                m_cwSpeedValueLabel->setText(QString("%1 WPM").arg(wpm));
                RadioSettings::instance()->setCwKeyerSpeed(wpm);
            }
        });
    }

    auto *inputTestLayout = new QHBoxLayout();
    auto *inputTestTitle = new QLabel("Paddle test:", page);
    inputTestTitle->setStyleSheet(QString("color: %1; font-size: %2px;")
                                      .arg(K4Styles::Colors::TextGray)
                                      .arg(K4Styles::Dimensions::FontSizePopup));
    auto *ditIndicator = new QLabel("DIT", page);
    auto *dahIndicator = new QLabel("DAH", page);
    const auto setInputIndicator = [](QLabel *indicator, bool pressed) {
        indicator->setStyleSheet(QString("background-color: %1; color: %2; border: 1px solid %3; "
                                         "padding: 8px 18px; font-size: %4px; font-weight: bold;")
                                     .arg(pressed ? K4Styles::Colors::AccentAmber
                                                  : K4Styles::Colors::DarkBackground,
                                          pressed ? K4Styles::Colors::DarkBackground
                                                  : K4Styles::Colors::TextGray,
                                          K4Styles::Colors::DialogBorder)
                                     .arg(K4Styles::Dimensions::FontSizePopup));
    };
    setInputIndicator(ditIndicator, false);
    setInputIndicator(dahIndicator, false);
    const auto updateInputLabels = [ditIndicator, dahIndicator]() {
        const auto *settings = RadioSettings::instance();
        if (settings->cwMidiKeyingMode() == 0) {
            ditIndicator->setText("DIT");
            dahIndicator->setText("DAH");
        } else if (settings->cwMidiStraightKeyInput() == 0) {
            ditIndicator->setText("KEY");
            dahIndicator->setText("UNUSED");
        } else {
            ditIndicator->setText("UNUSED");
            dahIndicator->setText("KEY");
        }
    };
    updateInputLabels();
    connect(RadioSettings::instance(), &RadioSettings::cwMidiKeyingModeChanged,
            page, [updateInputLabels](int) { updateInputLabels(); });
    connect(RadioSettings::instance(), &RadioSettings::cwMidiStraightKeyInputChanged,
            page, [updateInputLabels](int) { updateInputLabels(); });
    inputTestTitle->setFixedWidth(135);
    inputTestLayout->addWidget(inputTestTitle);
    inputTestLayout->addWidget(ditIndicator);
    inputTestLayout->addWidget(dahIndicator);
    m_paddleMappingLabel = new QLabel(page);
    const auto updatePaddleMappingLabel = [this]() {
        if (!m_paddleMappingLabel)
            return;
        const bool reversed = RadioSettings::instance()->cwPaddlesReversed();
        m_paddleMappingLabel->setText(reversed
            ? "Paddles: REVERSED (synced from K4; saved)"
            : "Paddles: NORMAL — left DIT / right DAH (K4-synced; saved)");
    };
    m_paddleMappingLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-style: italic;")
                                            .arg(K4Styles::Colors::TextGray)
                                            .arg(K4Styles::Dimensions::FontSizeLarge));
    m_paddleMappingLabel->setWordWrap(true);
    m_paddleMappingLabel->setMinimumWidth(0);
    m_paddleMappingLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    updatePaddleMappingLabel();
    connect(RadioSettings::instance(), &RadioSettings::cwPaddlesReversedChanged,
            m_paddleMappingLabel, [updatePaddleMappingLabel](bool) { updatePaddleMappingLabel(); });
    inputTestLayout->addStretch();
    controlsLayout->addLayout(inputTestLayout);
    auto *paddleMappingLayout = new QHBoxLayout();
    paddleMappingLayout->setContentsMargins(0, 0, 0, 0);
    paddleMappingLayout->addSpacing(141);
    paddleMappingLayout->addWidget(m_paddleMappingLabel, 1);
    controlsLayout->addLayout(paddleMappingLayout);
    if (m_halikeyDevice) {
        connect(m_halikeyDevice, &HalikeyDevice::ditStateChanged, ditIndicator,
                [ditIndicator, dahIndicator, setInputIndicator](bool pressed) {
                    setInputIndicator(RadioSettings::instance()->cwPaddlesReversed() ? dahIndicator : ditIndicator,
                                      pressed);
                });
        connect(m_halikeyDevice, &HalikeyDevice::dahStateChanged, dahIndicator,
                [ditIndicator, dahIndicator, setInputIndicator](bool pressed) {
                    setInputIndicator(RadioSettings::instance()->cwPaddlesReversed() ? ditIndicator : dahIndicator,
                                      pressed);
                });
        connect(m_halikeyDevice, &HalikeyDevice::straightKeyStateChanged, page,
                [ditIndicator, dahIndicator, setInputIndicator](bool pressed) {
                    QLabel *indicator = RadioSettings::instance()->cwMidiStraightKeyInput() == 0
                                            ? ditIndicator : dahIndicator;
                    setInputIndicator(indicator, pressed);
                });
        connect(m_halikeyDevice, &HalikeyDevice::disconnected, page, [=]() {
            setInputIndicator(ditIndicator, false);
            setInputIndicator(dahIndicator, false);
        });
    }

    auto *sidetoneLayout = new QHBoxLayout();
    auto *sidetoneTitle = new QLabel("Local sidetone:", page);
    sidetoneTitle->setFixedWidth(135);
    sidetoneTitle->setStyleSheet(QString("color: %1; font-size: %2px;")
                                     .arg(K4Styles::Colors::TextGray)
                                     .arg(K4Styles::Dimensions::FontSizePopup));
    m_sidetoneVolumeSlider = new QSlider(Qt::Horizontal, page);
    m_sidetoneVolumeSlider->setRange(0, 100);
    m_sidetoneVolumeSlider->setTracking(true);
    m_sidetoneVolumeSlider->setFixedWidth(300);
    m_sidetoneVolumeSlider->setValue(RadioSettings::instance()->sidetoneVolume());
    m_sidetoneVolumeSlider->setStyleSheet(
        K4Styles::sliderHorizontal(K4Styles::Colors::DarkBackground, K4Styles::Colors::AccentAmber));
    m_sidetoneVolumeSlider->setAttribute(Qt::WA_NoMousePropagation);
    m_sidetoneVolumeSlider->installEventFilter(this);
    m_sidetoneVolumeValueLabel =
        new QLabel(QString("%1%").arg(RadioSettings::instance()->sidetoneVolume()), page);
    m_sidetoneVolumeValueLabel->setMinimumWidth(70);
    m_sidetoneVolumeValueLabel->setAlignment(Qt::AlignCenter);
    m_sidetoneVolumeValueLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                                   .arg(K4Styles::Colors::TextWhite)
                                                   .arg(K4Styles::Dimensions::FontSizePopup));
    connect(m_sidetoneVolumeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_sidetoneVolumeValueLabel->setText(QString("%1%").arg(value));
        RadioSettings::instance()->setSidetoneVolume(value);
    });
    sidetoneLayout->addWidget(sidetoneTitle);
    sidetoneLayout->addWidget(m_sidetoneVolumeSlider, 1);
    sidetoneLayout->addWidget(m_sidetoneVolumeValueLabel);
    sidetoneLayout->addStretch(1);
    controlsLayout->addLayout(sidetoneLayout);

    m_midiMappingLabel = new QLabel(page);
    m_midiMappingLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-style: italic;")
                               .arg(K4Styles::Colors::TextGray)
                               .arg(K4Styles::Dimensions::FontSizeLarge));
    m_midiMappingLabel->setWordWrap(true);
    controlsLayout->addWidget(m_midiMappingLabel);
    updateMidiProfileControls();
    if (m_halikeyDevice) {
        connect(m_halikeyDevice, &HalikeyDevice::rawMidiEvent, page,
                [this, updateMidiProfileControls](int status, int data1, int, bool pressed) {
            if (!pressed || m_midiLearningTarget == 0) return;
            auto *s = RadioSettings::instance();
            int ditStatus = s->midiDitStatus(), ditData1 = s->midiDitData1();
            int dahStatus = s->midiDahStatus(), dahData1 = s->midiDahData1();
            if (m_midiLearningTarget == 1) { ditStatus = status; ditData1 = data1; }
            else { dahStatus = status; dahData1 = data1; }
            s->setMidiCustomMapping(ditStatus, ditData1, dahStatus, dahData1);
            m_midiMappingProfileCombo->setCurrentIndex(2);
            m_cwKeyerStatusLabel->setText(m_midiLearningTarget == 1 ? "DIT learned" : "DAH learned");
            m_midiLearningTarget = 0;
            updateMidiProfileControls();
        });
    }
    // Scroll only from inert background/text surfaces. Interactive controls,
    // especially sliders, are never observed by the scroll handler and retain
    // exclusive ownership of every touch sequence.
    controlsWidget->setProperty("cwScrollSurface", true);
    controlsWidget->installEventFilter(this);
    for (QLabel *label : controlsWidget->findChildren<QLabel *>()) {
        label->setProperty("cwScrollSurface", true);
        label->installEventFilter(this);
    }
    // Opening this page must not automatically start the eight-second BLE
    // scan. SCAN refreshes attached USB MIDI immediately and BLE MIDI
    // asynchronously; initial population retains the remembered selection.
    populateCwKeyerPorts();
    updateCwKeyerStatus();
    controlsLayout->addStretch(1);
    return page;
#endif

    // Title
    auto *titleLabel = new QLabel("CW Keyer", page);
    titleLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                  .arg(K4Styles::Colors::AccentAmber)
                                  .arg(K4Styles::Dimensions::FontSizeTitle));
    layout->addWidget(titleLabel);

    // Description (dynamic based on device type)
    m_cwKeyerDescLabel = new QLabel(page);
    m_cwKeyerDescLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                          .arg(K4Styles::Colors::TextGray)
                                          .arg(K4Styles::Dimensions::FontSizeButton));
    m_cwKeyerDescLabel->setWordWrap(true);
    layout->addWidget(m_cwKeyerDescLabel);

    // Device Type selector
    auto *deviceTypeLayout = new QHBoxLayout();
    auto *deviceTypeLabel = new QLabel("Device Type:", page);
    deviceTypeLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                       .arg(K4Styles::Colors::TextGray)
                                       .arg(K4Styles::Dimensions::FontSizePopup));
    deviceTypeLabel->setFixedWidth(K4Styles::Dimensions::FormLabelWidth);

    m_cwKeyerDeviceTypeCombo = new QComboBox(page);
    m_cwKeyerDeviceTypeCombo->setStyleSheet(
        QString("QComboBox { background-color: %1; color: %2; border: 1px solid %3; "
                "           padding: %6px; font-size: %5px; border-radius: %7px; }"
                "QComboBox:focus { border-color: %4; }"
                "QComboBox::drop-down { border: none; width: 20px; }"
                "QComboBox::down-arrow { image: none; border-left: 5px solid transparent; "
                "           border-right: 5px solid transparent; border-top: 5px solid %2; }"
                "QComboBox QAbstractItemView { background-color: %1; color: %2; selection-background-color: %4; }")
            .arg(K4Styles::Colors::DarkBackground, K4Styles::Colors::TextWhite, K4Styles::Colors::DialogBorder,
                 K4Styles::Colors::AccentAmber)
            .arg(K4Styles::Dimensions::FontSizePopup)
            .arg(K4Styles::Dimensions::PaddingSmall)
            .arg(K4Styles::Dimensions::SliderBorderRadius));
    m_cwKeyerDeviceTypeCombo->addItem("HaliKey V1.4", 0);
    m_cwKeyerDeviceTypeCombo->addItem("HaliKey MIDI", 1);

    int savedDeviceType = RadioSettings::instance()->halikeyDeviceType();
    m_cwKeyerDeviceTypeCombo->setCurrentIndex(savedDeviceType);
    updateCwKeyerDescription();

    connect(m_cwKeyerDeviceTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        int type = m_cwKeyerDeviceTypeCombo->itemData(index).toInt();
        RadioSettings::instance()->setHalikeyDeviceType(type);
        // Disconnect if connected, since device type changed
        if (m_halikeyDevice && m_halikeyDevice->isConnected()) {
            m_halikeyDevice->closePort();
        }
        updateCwKeyerDescription();
        populateCwKeyerPorts();
    });

    deviceTypeLayout->addWidget(deviceTypeLabel);
    deviceTypeLayout->addWidget(m_cwKeyerDeviceTypeCombo, 1);
    layout->addLayout(deviceTypeLayout);

    // Separator line
    auto *line = new QFrame(page);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::DialogBorder));
    line->setFixedHeight(K4Styles::Dimensions::SeparatorHeight);
    layout->addWidget(line);

    // Status indicator
    auto *statusLayout = new QHBoxLayout();
    auto *statusTitleLabel = new QLabel("Status:", page);
    statusTitleLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                        .arg(K4Styles::Colors::TextGray)
                                        .arg(K4Styles::Dimensions::FontSizePopup));
    statusTitleLabel->setFixedWidth(K4Styles::Dimensions::FormLabelWidth);

    m_cwKeyerStatusLabel = new QLabel("Not Connected", page);
    m_cwKeyerStatusLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                            .arg(K4Styles::Colors::ErrorRed)
                                            .arg(K4Styles::Dimensions::FontSizePopup));

    statusLayout->addWidget(statusTitleLabel);
    statusLayout->addWidget(m_cwKeyerStatusLabel);
    statusLayout->addStretch();
    layout->addLayout(statusLayout);

    // Separator line
    auto *line2 = new QFrame(page);
    line2->setFrameShape(QFrame::HLine);
    line2->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::DialogBorder));
    line2->setFixedHeight(K4Styles::Dimensions::SeparatorHeight);
    layout->addWidget(line2);

    // Connection Settings section
    auto *sectionLabel = new QLabel("Connection Settings", page);
    sectionLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                    .arg(K4Styles::Colors::TextWhite)
                                    .arg(K4Styles::Dimensions::FontSizePopup));
    layout->addWidget(sectionLabel);

    // Port selection
    auto *portLayout = new QHBoxLayout();
    auto *portLabel = new QLabel("Port:", page);
    portLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                 .arg(K4Styles::Colors::TextGray)
                                 .arg(K4Styles::Dimensions::FontSizePopup));
    portLabel->setFixedWidth(K4Styles::Dimensions::FormLabelWidth);

    m_cwKeyerPortCombo = new QComboBox(page);
    m_cwKeyerPortCombo->setStyleSheet(
        QString("QComboBox { background-color: %1; color: %2; border: 1px solid %3; "
                "           padding: %6px; font-size: %5px; border-radius: %7px; }"
                "QComboBox:focus { border-color: %4; }"
                "QComboBox::drop-down { border: none; width: 20px; }"
                "QComboBox::down-arrow { image: none; border-left: 5px solid transparent; "
                "           border-right: 5px solid transparent; border-top: 5px solid %2; }"
                "QComboBox QAbstractItemView { background-color: %1; color: %2; selection-background-color: %4; }")
            .arg(K4Styles::Colors::DarkBackground, K4Styles::Colors::TextWhite, K4Styles::Colors::DialogBorder,
                 K4Styles::Colors::AccentAmber)
            .arg(K4Styles::Dimensions::FontSizePopup)
            .arg(K4Styles::Dimensions::PaddingSmall)
            .arg(K4Styles::Dimensions::SliderBorderRadius));
    populateCwKeyerPorts();

    m_cwKeyerRefreshBtn = new QPushButton("Refresh", page);
    m_cwKeyerRefreshBtn->setStyleSheet(
        QString("QPushButton { background-color: %1; color: %2; border: 1px solid %3; "
                "             padding: %6px 12px; font-size: %4px; border-radius: %7px; }"
                "QPushButton:hover { background-color: %5; }")
            .arg(K4Styles::Colors::DarkBackground, K4Styles::Colors::TextWhite, K4Styles::Colors::DialogBorder)
            .arg(K4Styles::Dimensions::FontSizePopup)
            .arg(K4Styles::Colors::GradientBottom)
            .arg(K4Styles::Dimensions::PaddingSmall)
            .arg(K4Styles::Dimensions::SliderBorderRadius));
    connect(m_cwKeyerRefreshBtn, &QPushButton::clicked, this, &OptionsDialog::onCwKeyerRefreshClicked);

    portLayout->addWidget(portLabel);
    portLayout->addWidget(m_cwKeyerPortCombo, 1);
    portLayout->addWidget(m_cwKeyerRefreshBtn);
    layout->addLayout(portLayout);

    // Connect/Disconnect button
    m_cwKeyerConnectBtn = new QPushButton("Connect", page);
    m_cwKeyerConnectBtn->setStyleSheet(
        QString("QPushButton { background-color: %1; color: %2; border: 1px solid %3; "
                "             padding: 10px 20px; font-size: %4px; border-radius: 4px; }"
                "QPushButton:hover { background-color: %5; }")
            .arg(K4Styles::Colors::DarkBackground, K4Styles::Colors::TextWhite, K4Styles::Colors::DialogBorder)
            .arg(K4Styles::Dimensions::FontSizePopup)
            .arg(K4Styles::Colors::GradientBottom));
    connect(m_cwKeyerConnectBtn, &QPushButton::clicked, this, &OptionsDialog::onCwKeyerConnectClicked);
    layout->addWidget(m_cwKeyerConnectBtn);

    // Separator line
    auto *line3 = new QFrame(page);
    line3->setFrameShape(QFrame::HLine);
    line3->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::DialogBorder));
    line3->setFixedHeight(K4Styles::Dimensions::SeparatorHeight);
    layout->addWidget(line3);

    // Separator line
    auto *line5 = new QFrame(page);
    line5->setFrameShape(QFrame::HLine);
    line5->setStyleSheet(QString("background-color: %1;").arg(K4Styles::Colors::DialogBorder));
    line5->setFixedHeight(K4Styles::Dimensions::SeparatorHeight);
    layout->addWidget(line5);

    // Sidetone Settings section
    auto *sidetoneLabel = new QLabel("Sidetone Settings", page);
    sidetoneLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                     .arg(K4Styles::Colors::TextWhite)
                                     .arg(K4Styles::Dimensions::FontSizePopup));
    layout->addWidget(sidetoneLabel);

    // Sidetone volume slider
    auto *volumeLayout = new QHBoxLayout();
    auto *volumeLabel = new QLabel("Volume:", page);
    volumeLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                   .arg(K4Styles::Colors::TextGray)
                                   .arg(K4Styles::Dimensions::FontSizePopup));
    volumeLabel->setFixedWidth(K4Styles::Dimensions::FormLabelWidth);

    m_sidetoneVolumeSlider = new QSlider(Qt::Horizontal, page);
    m_sidetoneVolumeSlider->setRange(0, 100);
    m_sidetoneVolumeSlider->setValue(RadioSettings::instance()->sidetoneVolume());
    m_sidetoneVolumeSlider->setStyleSheet(
        K4Styles::sliderHorizontal(K4Styles::Colors::DarkBackground, K4Styles::Colors::AccentAmber));

    m_sidetoneVolumeValueLabel = new QLabel(QString("%1%").arg(RadioSettings::instance()->sidetoneVolume()), page);
    m_sidetoneVolumeValueLabel->setStyleSheet(QString("color: %1; font-size: %2px;")
                                                  .arg(K4Styles::Colors::TextWhite)
                                                  .arg(K4Styles::Dimensions::FontSizePopup));
    m_sidetoneVolumeValueLabel->setFixedWidth(K4Styles::Dimensions::SliderValueLabelWidth);

    connect(m_sidetoneVolumeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_sidetoneVolumeValueLabel->setText(QString("%1%").arg(value));
        RadioSettings::instance()->setSidetoneVolume(value);
    });

    volumeLayout->addWidget(volumeLabel);
    volumeLayout->addWidget(m_sidetoneVolumeSlider, 1);
    volumeLayout->addWidget(m_sidetoneVolumeValueLabel);
    layout->addLayout(volumeLayout);

    // Sidetone help text
    auto *sidetoneHelpLabel =
        new QLabel("Local sidetone volume for CW keying feedback. Frequency is linked to K4's CW pitch setting.", page);
    sidetoneHelpLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-style: italic;")
                                         .arg(K4Styles::Colors::TextGray)
                                         .arg(K4Styles::Dimensions::FontSizeLarge));
    sidetoneHelpLabel->setWordWrap(true);
    layout->addWidget(sidetoneHelpLabel);

    layout->addStretch();

    // Initialize status
    updateCwKeyerStatus();

    return page;
}

void OptionsDialog::populateCwKeyerPorts() {
    if (!m_cwKeyerPortCombo)
        return;

    // Block signals to avoid triggering save during repopulation
    m_cwKeyerPortCombo->blockSignals(true);
    m_cwKeyerPortCombo->clear();

    int deviceType = m_cwKeyerDeviceTypeCombo ? m_cwKeyerDeviceTypeCombo->currentData().toInt() : 0;
    QString savedPort = RadioSettings::instance()->halikeyPortName();
    int selectedIndex = -1;

    if (deviceType == 1) {
        // MIDI device — enumerate MIDI ports
        QStringList midiDevices = HalikeyDevice::availableMidiDevices();
        for (int i = 0; i < midiDevices.size(); i++) {
#ifdef Q_OS_ANDROID
            const QStringList fields = midiDevices[i].split('|');
            const QString name = fields.value(0, "MIDI device");
            const QString deviceKey = fields.value(1);
            if (deviceKey.isEmpty())
                continue;
            m_cwKeyerPortCombo->addItem(name, deviceKey);
            if (deviceKey == savedPort || (deviceKey.startsWith("ble:") && deviceKey.mid(4) == savedPort)) {
                selectedIndex = m_cwKeyerPortCombo->count() - 1;
            }
#else
            m_cwKeyerPortCombo->addItem(midiDevices[i], midiDevices[i]);
            if (midiDevices[i] == savedPort) {
                selectedIndex = i;
            }
#endif
        }
#ifdef Q_OS_ANDROID
        if (selectedIndex < 0 && !savedPort.isEmpty()) {
            const QString rememberedName = savedPort.startsWith("usb:")
                ? "Remembered USB MIDI device (not attached)"
                : "Remembered BLE MIDI device (not discovered)";
            m_cwKeyerPortCombo->addItem(rememberedName, savedPort);
            selectedIndex = m_cwKeyerPortCombo->count() - 1;
        }
#endif
        // Auto-select first HaliKey MIDI device if no saved selection matched
        if (selectedIndex < 0) {
            for (int i = 0; i < midiDevices.size(); i++) {
                if (midiDevices[i].contains("HaliKey", Qt::CaseInsensitive)) {
                    selectedIndex = i;
                    break;
                }
            }
        }
    } else {
        // V14 device — enumerate serial ports
        auto ports = HalikeyDevice::availablePortsDetailed();
        for (int i = 0; i < ports.size(); i++) {
            m_cwKeyerPortCombo->addItem(ports[i].portName, ports[i].portName);
            if (ports[i].portName == savedPort) {
                selectedIndex = i;
            }
        }
    }

    if (selectedIndex >= 0) {
        m_cwKeyerPortCombo->setCurrentIndex(selectedIndex);
    }
    m_cwKeyerPortCombo->blockSignals(false);

    // Save selection when changed (reconnect since we may be called multiple times)
    m_cwKeyerPortCombo->disconnect(this);
    connect(m_cwKeyerPortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index >= 0) {
            QString portName = m_cwKeyerPortCombo->itemData(index).toString();
            RadioSettings::instance()->setHalikeyPortName(portName);
        }
    });
}

void OptionsDialog::updateCwKeyerDescription() {
    if (!m_cwKeyerDescLabel || !m_cwKeyerDeviceTypeCombo)
        return;

    int type = m_cwKeyerDeviceTypeCombo->currentData().toInt();
    if (type == 1) {
        m_cwKeyerDescLabel->setText("Connect a HaliKey MIDI paddle interface to send CW via the K4's keyer. "
                                    "The HaliKey MIDI uses standard MIDI note events to detect paddle and PTT inputs.");
    } else {
        m_cwKeyerDescLabel->setText("Connect a HaliKey paddle interface to send CW via the K4's keyer. "
                                    "The HaliKey uses serial port flow control signals to detect paddle inputs.");
    }
}

void OptionsDialog::onCwKeyerRefreshClicked() {
#ifdef Q_OS_ANDROID
    HalikeyDevice::startMidiScan();
    if (m_cwKeyerStatusLabel)
        m_cwKeyerStatusLabel->setText(m_halikeyDevice ? m_halikeyDevice->statusMessage()
                                                      : "Scanning USB and BLE MIDI...");
    populateCwKeyerPorts(); // USB enumeration is synchronous and available immediately.
    for (int delay : {1000, 3000, 6000, 8500})
        QTimer::singleShot(delay, this, &OptionsDialog::populateCwKeyerPorts);
#else
    populateCwKeyerPorts();
#endif
}

void OptionsDialog::onCwKeyerConnectClicked() {
    if (!m_halikeyDevice)
        return;

    if (m_halikeyDevice->isConnected()) {
        // Disconnect
        m_halikeyDevice->closePort();
    } else {
        // Connect — use port name from item data (without annotation suffix)
        QString portName = m_cwKeyerPortCombo->currentData().toString();
        if (!portName.isEmpty()) {
            RadioSettings::instance()->setHalikeyPortName(portName);
            m_halikeyDevice->openPort(portName);
        }
    }
}

void OptionsDialog::updateCwKeyerStatus() {
    if (!m_cwKeyerStatusLabel || !m_cwKeyerConnectBtn)
        return;

    bool isConnected = m_halikeyDevice && m_halikeyDevice->isConnected();

    if (isConnected) {
#ifdef Q_OS_ANDROID
        m_cwKeyerStatusLabel->setText(m_halikeyDevice->statusMessage());
#else
        m_cwKeyerStatusLabel->setText(QString("Connected to %1").arg(m_halikeyDevice->portName()));
#endif
        m_cwKeyerStatusLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                                .arg(K4Styles::Colors::StatusGreen)
                                                .arg(K4Styles::Dimensions::FontSizePopup));
        m_cwKeyerConnectBtn->setText("Disconnect");
    } else {
        const QString status = m_halikeyDevice ? m_halikeyDevice->statusMessage() : QStringLiteral("Not connected");
        m_cwKeyerStatusLabel->setText(status);
        m_cwKeyerStatusLabel->setStyleSheet(QString("color: %1; font-size: %2px; font-weight: bold;")
                                                .arg(K4Styles::Colors::ErrorRed)
                                                .arg(K4Styles::Dimensions::FontSizePopup));
        m_cwKeyerConnectBtn->setText("Connect");
    }
}
