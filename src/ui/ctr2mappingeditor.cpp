#include "ctr2mappingeditor.h"

#include "inwindowdialog.h"
#include "k4styles.h"
#include "../hardware/ctr2mididevice.h"
#include "../settings/radiosettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <QScrollerProperties>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QLabel *sectionLabel(const QString &text, QWidget *parent);

QString comboStyle() {
    return QString(
        "QComboBox,QLineEdit { background:%1; color:%2; border:1px solid %3; padding:7px; font-size:%4px; }"
        "QComboBox QAbstractItemView { background:%1; color:%2; selection-background-color:%5; }")
        .arg(K4Styles::Colors::DarkBackground, K4Styles::Colors::TextWhite,
             K4Styles::Colors::DialogBorder)
        .arg(K4Styles::Dimensions::FontSizeLarge)
        .arg(K4Styles::Colors::AccentAmber);
}

// Android's native QComboBox popup creates another EGL surface. CTR2 setup
// keeps every choice inside the existing Options window instead.
class Ctr2InWindowComboBox final : public QComboBox {
public:
    explicit Ctr2InWindowComboBox(const QString &title, QWidget *parent = nullptr)
        : QComboBox(parent), m_title(title) {}

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            // Do not open on press. A press on a selector must remain eligible
            // to become a page-scroll gesture until the finger is released.
            m_pressed = true;
            m_dragged = false;
            m_pressPosition = event->position();
            event->accept();
            return;
        }
        QComboBox::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (m_pressed && (event->buttons() & Qt::LeftButton)) {
            const qreal distance = (event->position() - m_pressPosition).manhattanLength();
            if (distance >= qMax(8, QApplication::startDragDistance()))
                m_dragged = true;
        }
        QComboBox::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            const bool activate = m_pressed && !m_dragged
                                  && rect().contains(event->position().toPoint());
            m_pressed = false;
            m_dragged = false;
            event->accept();
            if (activate)
                QTimer::singleShot(0, this, [this]() { showPopup(); });
            return;
        }
        QComboBox::mouseReleaseEvent(event);
    }

    void showPopup() override {
        if (!isEnabled() || count() == 0)
            return;
        QWidget *dialogParent = window();
        if (!dialogParent)
            dialogParent = parentWidget();
        if (!dialogParent)
            return;

        InWindowDialog dialog(dialogParent);
        QWidget *panel = dialog.contentWidget();
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(10, 8, 10, 8);
        layout->setSpacing(7);
        auto *title = sectionLabel(m_title, panel);
        title->setAlignment(Qt::AlignCenter);
        layout->addWidget(title);

        auto *list = new QListWidget(panel);
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        list->setStyleSheet(QString(
            "QListWidget{background:%1;color:%2;border:1px solid %3;font-size:%4px;}"
            "QListWidget::item{padding:7px;}"
            "QListWidget::item:selected{background:%5;color:%1;}")
            .arg(K4Styles::Colors::DarkBackground, K4Styles::Colors::TextWhite,
                 K4Styles::Colors::DialogBorder)
            .arg(K4Styles::Dimensions::FontSizeLarge)
            .arg(K4Styles::Colors::AccentAmber));
        for (int index = 0; index < count(); ++index) {
            auto *item = new QListWidgetItem(itemText(index), list);
            item->setData(Qt::UserRole, index);
            item->setSizeHint(QSize(0, 40));
        }
        const int initial = qBound(0, currentIndex(), count() - 1);
        list->setCurrentRow(initial);
#ifdef Q_OS_ANDROID
        list->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
        QScroller::grabGesture(list->viewport(), QScroller::TouchGesture);
#endif
        layout->addWidget(list, 1);

        auto *buttons = new QHBoxLayout;
        auto *cancel = new QPushButton(QStringLiteral("CANCEL"), panel);
        auto *use = new QPushButton(QStringLiteral("USE"), panel);
        for (QPushButton *button : {cancel, use}) {
            button->setMinimumHeight(42);
            button->setStyleSheet(K4Styles::menuBarButton());
            buttons->addWidget(button);
        }
        layout->addLayout(buttons);
        QObject::connect(cancel, &QPushButton::clicked, &dialog, &InWindowDialog::reject);
        QObject::connect(use, &QPushButton::clicked, &dialog, &InWindowDialog::accept);
        QObject::connect(list, &QListWidget::itemDoubleClicked, &dialog,
                         [&dialog](QListWidgetItem *) { dialog.accept(); });

        const int panelWidth = qMin(620, qMax(300, dialogParent->width() - 20));
        const int panelHeight = qMin(qMax(210, 105 + qMin(8, count()) * 40),
                                     qMax(210, dialogParent->height() - 20));
        dialog.setPanelSize(QSize(panelWidth, panelHeight));
        QTimer::singleShot(0, list, [list, initial]() {
            if (QListWidgetItem *item = list->item(initial))
                list->scrollToItem(item, QAbstractItemView::PositionAtCenter);
        });
        if (dialog.exec() == InWindowDialog::Accepted && list->currentItem()) {
            const int selected = list->currentItem()->data(Qt::UserRole).toInt();
            setCurrentIndex(selected);
            emit activated(selected);
        }
    }

private:
    QString m_title;
    QPointF m_pressPosition;
    bool m_pressed = false;
    bool m_dragged = false;
};

QLabel *sectionLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:bold;")
                             .arg(K4Styles::Colors::AccentAmber)
                             .arg(K4Styles::Dimensions::FontSizePopup));
    return label;
}

QString knobControlLabel(int cc) {
    static const char *modeNames[] = {"Home", "Mode 1", "Mode 2", "Mode 3"};
    const int index = qBound(0, cc - 100, 7);
    return QString("%1 %2 · CC %3")
        .arg(modeNames[index / 2], (index % 2) == 0 ? "turn" : "push-turn")
        .arg(cc);
}

bool validateCommands(const MidiMapping::DeviceMapping &mapping, QString *error) {
    for (auto it = mapping.buttons.cbegin(); it != mapping.buttons.cend(); ++it) {
        if (it->action != QStringLiteral("macro"))
            continue;
        const QString command = mapping.macros.value(it->macroId).command;
        QString commandError;
        if (!MidiMapping::isValidK4Command(command, &commandError)) {
            if (error)
                *error = QString("Button note %1: %2").arg(it.key()).arg(commandError);
            return false;
        }
    }
    if (error)
        error->clear();
    return true;
}

} // namespace

Ctr2MappingEditor::Ctr2MappingEditor(Ctr2MidiDevice *device, QWidget *parent)
    : QWidget(parent), m_device(device) {
    setStyleSheet("background:transparent;");
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setStyleSheet(QString("QScrollArea{background:%1;border:0;}"
                                    "QScrollArea>QWidget>QWidget{background:%1;}")
                                .arg(K4Styles::Colors::Background));
    auto *content = new QWidget(m_scroll);
    content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto *root = new QVBoxLayout(content);
    root->setContentsMargins(0, 8, 0, 8);
    root->setSpacing(8);

    auto *deviceTitle = sectionLabel("CTR2-MIDI", this);
    deviceTitle->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:bold;")
                                   .arg(K4Styles::Colors::AccentAmber)
                                   .arg(K4Styles::Dimensions::FontSizeTitle));
    root->addWidget(deviceTitle);
    auto *deviceHelp = new QLabel(
        "Independent CTR2 connection. The CW Keyer tab keeps its own TinyMIDI, "
        "HaliKey MIDI, or Custom connection.", this);
    deviceHelp->setWordWrap(true);
    deviceHelp->setStyleSheet(QString("color:%1;font-size:%2px;")
                                  .arg(K4Styles::Colors::TextGray)
                                  .arg(K4Styles::Dimensions::FontSizeLarge));
    root->addWidget(deviceHelp);

    auto *connectionRow = new QHBoxLayout;
    auto *statusTitle = new QLabel("Status:", this);
    statusTitle->setMinimumWidth(90);
    m_connectionStatus = new QLabel("Not connected", this);
    connectionRow->addWidget(statusTitle);
    connectionRow->addWidget(m_connectionStatus, 1);
    root->addLayout(connectionRow);

    auto *deviceRow = new QHBoxLayout;
    auto *deviceLabel = new QLabel("Device:", this);
    deviceLabel->setMinimumWidth(90);
    m_deviceSelector = new Ctr2InWindowComboBox("SELECT CTR2-MIDI DEVICE", this);
    m_deviceSelector->setMinimumHeight(42);
    m_deviceSelector->setStyleSheet(comboStyle());
    m_scanButton = new QPushButton("SCAN", this);
    m_connectButton = new QPushButton("CONNECT", this);
    for (QPushButton *button : {m_scanButton, m_connectButton}) {
        button->setMinimumHeight(42);
        button->setStyleSheet(K4Styles::menuBarButton());
    }
    deviceRow->addWidget(deviceLabel);
    deviceRow->addWidget(m_deviceSelector, 1);
    deviceRow->addWidget(m_scanButton);
    deviceRow->addWidget(m_connectButton);
    root->addLayout(deviceRow);

    auto *connectionSeparator = new QFrame(this);
    connectionSeparator->setFrameShape(QFrame::HLine);
    connectionSeparator->setStyleSheet(QString("background:%1;")
                                           .arg(K4Styles::Colors::DialogBorder));
    connectionSeparator->setFixedHeight(K4Styles::Dimensions::SeparatorHeight);
    root->addWidget(connectionSeparator);

    auto *titleRow = new QHBoxLayout;
    titleRow->addWidget(sectionLabel("CTR2-MIDI mapping", this));
    m_status = new QLabel(this);
    m_status->setStyleSheet(QString("color:%1;font-size:%2px;font-style:italic;")
                                .arg(K4Styles::Colors::TextGray)
                                .arg(K4Styles::Dimensions::FontSizeLarge));
    titleRow->addStretch();
    titleRow->addWidget(m_status);
    root->addLayout(titleRow);

    auto *nameRow = new QHBoxLayout;
    auto *nameLabel = new QLabel("Configuration name:", this);
    nameLabel->setMinimumWidth(180);
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setMinimumHeight(40);
    m_nameEdit->setStyleSheet(comboStyle());
    nameRow->addWidget(nameLabel);
    nameRow->addWidget(m_nameEdit, 1);
    root->addLayout(nameRow);

    auto *keyingRow = new QHBoxLayout;
    m_cwEnabled = new QCheckBox("CW input", this);
    m_keyingMode = new Ctr2InWindowComboBox("CTR2 KEYING MODE", this);
    m_keyingMode->addItem("Paddles", static_cast<int>(MidiMapping::KeyingMode::Paddles));
    m_keyingMode->addItem("Straight key + PTT", static_cast<int>(MidiMapping::KeyingMode::StraightKey));
    m_tipRingSwapped = new QCheckBox("Swap tip/ring", this);
    for (auto *combo : {m_keyingMode}) {
        combo->setMinimumHeight(40);
        combo->setStyleSheet(comboStyle());
    }
    keyingRow->addWidget(m_cwEnabled);
    keyingRow->addWidget(m_keyingMode);
    keyingRow->addWidget(m_tipRingSwapped);
    keyingRow->addStretch();
    root->addLayout(keyingRow);

    root->addWidget(sectionLabel("Knob modes", this));
    auto *knobHelp = new QLabel(
        "Each CTR2 knob mode can have a fixed function. To select functions with buttons like the "
        "radio, assign a knob mode to Selected adjustment (button), then assign buttons to Adjust: actions.",
        this);
    knobHelp->setWordWrap(true);
    knobHelp->setStyleSheet(QString("color:%1;font-size:%2px;")
                                .arg(K4Styles::Colors::TextGray)
                                .arg(K4Styles::Dimensions::FontSizeLarge));
    root->addWidget(knobHelp);
    m_knobGrid = new QGridLayout;
    m_knobGrid->setHorizontalSpacing(8);
    m_knobGrid->setVerticalSpacing(6);
    m_knobGrid->addWidget(new QLabel("CTR2 control", this), 0, 0);
    m_knobGrid->addWidget(new QLabel("Radio action", this), 0, 1);
    m_knobGrid->addWidget(new QLabel("MIDI output", this), 0, 2);
    for (int cc = 100; cc <= 107; ++cc) {
        auto *name = new QLabel(knobControlLabel(cc), this);
        auto *action = new Ctr2InWindowComboBox("SELECT RADIO ACTION", this);
        for (const QString &id : MidiMapping::supportedKnobActions())
            action->addItem(MidiMapping::knobActionLabel(id), id);
        auto *output = new Ctr2InWindowComboBox("SELECT CTR2 MIDI OUTPUT", this);
        for (int value = static_cast<int>(MidiMapping::KnobOutput::WheelA);
             value <= static_cast<int>(MidiMapping::KnobOutput::Button); ++value) {
            const auto type = static_cast<MidiMapping::KnobOutput>(value);
            output->addItem(MidiMapping::knobOutputLabel(type), value);
        }
        for (auto *combo : {action, output}) {
            combo->setMinimumHeight(40);
            combo->setStyleSheet(comboStyle());
        }
        m_knobGrid->addWidget(name, cc - 99, 0);
        m_knobGrid->addWidget(action, cc - 99, 1);
        m_knobGrid->addWidget(output, cc - 99, 2);
        m_knobRows.insert(cc, {action, output});
        connect(action, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, cc](int) {
            if (m_loading) return;
            m_draft.knobs[cc].action = m_knobRows[cc].action->currentData().toString();
            setDirty();
        });
        connect(output, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, cc](int) {
            if (m_loading) return;
            m_draft.knobs[cc].output = static_cast<MidiMapping::KnobOutput>(
                m_knobRows[cc].output->currentData().toInt());
            setDirty();
        });
    }
    root->addLayout(m_knobGrid);

    root->addWidget(sectionLabel("Buttons", this));
    auto *buttonHelp = new QLabel(
        "Short and long presses are independent. Adjust: actions select and open a QK4 control for a "
        "Selected adjustment knob. Choose Custom Command to send a supported K4 Programmer's "
        "Reference command or macro exactly as entered.", this);
    buttonHelp->setWordWrap(true);
    buttonHelp->setStyleSheet(QString("color:%1;font-size:%2px;")
                                  .arg(K4Styles::Colors::TextGray)
                                  .arg(K4Styles::Dimensions::FontSizeLarge));
    root->addWidget(buttonHelp);
    auto *ft8Help = new QLabel(
        "For FT8/FT4, assign Switch RX/TX tone to short press and Set tone frequency to long press. "
        "Short press chooses the tone. Turn the wheel to move its dashed marker. Long press applies that "
        "frequency and finishes adjusting. Setting RX focuses My QSO; setting TX enables Hold TX. "
        "The wheel may use Selected adjustment, Active VFO frequency or Other VFO frequency.", this);
    ft8Help->setWordWrap(true);
    ft8Help->setStyleSheet(buttonHelp->styleSheet());
    root->addWidget(ft8Help);
    m_extendedButtons = new QCheckBox("Extended Button Mode", this);
    root->addWidget(m_extendedButtons);
    auto *extendedButtonHelp = new QLabel(
        "Enable this only when Extended BTN Mode is also enabled in CTR2-MIDI. It expands the "
        "list from 12 shared functions to 48 functions across Home and Knob modes 1-3.", this);
    extendedButtonHelp->setWordWrap(true);
    extendedButtonHelp->setStyleSheet(QString("color:%1;font-size:%2px;")
                                          .arg(K4Styles::Colors::TextGray)
                                          .arg(K4Styles::Dimensions::FontSizeLarge));
    root->addWidget(extendedButtonHelp);
    m_buttonRowsLayout = new QVBoxLayout;
    m_buttonRowsLayout->setSpacing(5);
    root->addLayout(m_buttonRowsLayout);

    auto *actions = new QHBoxLayout;
    auto *restore = new QPushButton("RESTORE DEFAULTS", this);
    auto *load = new QPushButton("LOAD FILE", this);
    auto *exportButton = new QPushButton("SAVE FILE", this);
    auto *apply = new QPushButton("APPLY MAPPING", this);
    for (auto *button : {restore, load, exportButton, apply}) {
        button->setMinimumHeight(42);
        button->setStyleSheet(K4Styles::menuBarButton());
        actions->addWidget(button);
    }
    root->addLayout(actions);

    connect(m_cwEnabled, &QCheckBox::toggled, this, [this](bool enabled) {
        if (m_loading) return;
        m_draft.cwInputEnabled = enabled;
        setDirty();
    });
    connect(m_nameEdit, &QLineEdit::textChanged, this, [this](const QString &name) {
        if (m_loading) return;
        m_draft.name = name.trimmed().isEmpty() ? QStringLiteral("CTR2 Mapping") : name.trimmed();
        setDirty();
    });
    connect(m_keyingMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_loading) return;
        m_draft.keyingMode = static_cast<MidiMapping::KeyingMode>(m_keyingMode->currentData().toInt());
        setDirty();
    });
    connect(m_tipRingSwapped, &QCheckBox::toggled, this, [this](bool swapped) {
        if (m_loading) return;
        m_draft.tipRingSwapped = swapped;
        setDirty();
    });
    connect(m_extendedButtons, &QCheckBox::toggled, this, [this](bool extended) {
        if (m_loading) return;
        m_draft = MidiMapping::withCtr2ButtonMode(m_draft, extended);
        rebuildButtonRows();
        setDirty();
    });
    connect(restore, &QPushButton::clicked, this, &Ctr2MappingEditor::restoreDefaults);
    connect(load, &QPushButton::clicked, this, &Ctr2MappingEditor::importMapping);
    connect(exportButton, &QPushButton::clicked, this, &Ctr2MappingEditor::exportMapping);
    connect(apply, &QPushButton::clicked, this, [this]() { saveCurrent(); });
    connect(m_scanButton, &QPushButton::clicked, this, &Ctr2MappingEditor::scanMidiDevices);
    connect(m_connectButton, &QPushButton::clicked, this, &Ctr2MappingEditor::toggleConnection);
    connect(m_deviceSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
        if (index >= 0)
            RadioSettings::instance()->setCtr2MidiPortName(
                m_deviceSelector->itemData(index).toString());
    });
    if (m_device) {
        connect(m_device, &Ctr2MidiDevice::connected,
                this, &Ctr2MappingEditor::updateConnectionStatus);
        connect(m_device, &Ctr2MidiDevice::disconnected,
                this, &Ctr2MappingEditor::updateConnectionStatus);
        connect(m_device, &Ctr2MidiDevice::connectionError, this,
                [this](const QString &) { updateConnectionStatus(); });
    }

    root->addStretch();
    root->activate();
    content->setMinimumHeight(root->sizeHint().height());
    m_scroll->setWidget(content);
    outer->addWidget(m_scroll);

#ifdef Q_OS_ANDROID
    m_scroll->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    QScroller::grabGesture(m_scroll->viewport(), QScroller::TouchGesture);
    if (QScroller *scroller = QScroller::scroller(m_scroll->viewport())) {
        QScrollerProperties properties = scroller->scrollerProperties();
        properties.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.35);
        properties.setScrollMetric(QScrollerProperties::DragStartDistance, 0.0015);
        scroller->setScrollerProperties(properties);
        connect(scroller, &QScroller::stateChanged, this, [this](QScroller::State state) {
            if (state == QScroller::Inactive) {
                QTimer::singleShot(120, this, [this]() { m_scrollGestureSuppressClick = false; });
                return;
            }
            if (state != QScroller::Dragging && state != QScroller::Scrolling)
                return;
            m_scrollGestureSuppressClick = true;
            for (QAbstractButton *button : findChildren<QAbstractButton *>())
                button->setDown(false);
        });
    }
#endif
    ensureLoaded();
    loadControls();
    populateMidiDevices();
    updateConnectionStatus();
    installInteractiveFilters();
}

bool Ctr2MappingEditor::eventFilter(QObject *watched, QEvent *event) {
    if (m_scrollGestureSuppressClick
        && (event->type() == QEvent::MouseButtonRelease
            || event->type() == QEvent::MouseButtonDblClick)) {
        if (auto *button = qobject_cast<QAbstractButton *>(watched))
            button->setDown(false);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void Ctr2MappingEditor::installInteractiveFilters() {
    for (QWidget *child : findChildren<QWidget *>()) {
        if (qobject_cast<QAbstractButton *>(child)
            || qobject_cast<QComboBox *>(child)
            || qobject_cast<QLineEdit *>(child)) {
            child->installEventFilter(this);
        }
    }
}

void Ctr2MappingEditor::populateMidiDevices() {
    if (!m_deviceSelector)
        return;
    const QSignalBlocker blocker(m_deviceSelector);
    const QString savedKey = RadioSettings::instance()->ctr2MidiPortName();
    m_deviceSelector->clear();
    int selected = -1;
    const QStringList devices = Ctr2MidiDevice::availableMidiDevices();
    for (const QString &entry : devices) {
        const QStringList fields = entry.split('|');
        const QString name = fields.value(0, QStringLiteral("MIDI device"));
        const QString key = fields.value(1);
        if (key.isEmpty())
            continue;
        m_deviceSelector->addItem(name, key);
        if (key == savedKey || (key.startsWith(QStringLiteral("ble:"))
                                && key.mid(4) == savedKey))
            selected = m_deviceSelector->count() - 1;
    }
    if (selected < 0 && !savedKey.isEmpty()) {
        m_deviceSelector->addItem(
            savedKey.startsWith(QStringLiteral("usb:"))
                ? QStringLiteral("Remembered CTR2 USB device (not attached)")
                : QStringLiteral("Remembered CTR2 BLE device (not discovered)"),
            savedKey);
        selected = m_deviceSelector->count() - 1;
    }
    if (selected < 0) {
        for (int index = 0; index < m_deviceSelector->count(); ++index) {
            const QString name = m_deviceSelector->itemText(index);
            if (name.contains(QStringLiteral("CTR2"), Qt::CaseInsensitive)
                || name.contains(QStringLiteral("Lynovation"), Qt::CaseInsensitive)) {
                selected = index;
                break;
            }
        }
    }
    if (selected < 0 && m_deviceSelector->count() > 0)
        selected = 0;
    m_deviceSelector->setCurrentIndex(selected);
    installInteractiveFilters();
}

void Ctr2MappingEditor::scanMidiDevices() {
    Ctr2MidiDevice::startMidiScan();
    if (m_connectionStatus)
        m_connectionStatus->setText(QStringLiteral("Scanning USB and BLE MIDI…"));
    populateMidiDevices();
    for (int delay : {1000, 3000, 6000, 8500})
        QTimer::singleShot(delay, this, &Ctr2MappingEditor::populateMidiDevices);
}

void Ctr2MappingEditor::toggleConnection() {
    if (!m_device)
        return;
    if (m_device->isConnected()) {
        m_device->closePort();
        return;
    }
    if (!m_deviceSelector || m_deviceSelector->currentIndex() < 0) {
        showInWindowMessage(dialogParent(), QStringLiteral("CTR2-MIDI"),
                            QStringLiteral("Scan for and select a CTR2-MIDI device first."));
        return;
    }
    const QString deviceKey = m_deviceSelector->currentData().toString();
    if (deviceKey.isEmpty())
        return;
    RadioSettings::instance()->setCtr2MidiPortName(deviceKey);
    m_device->openPort(deviceKey);
    updateConnectionStatus();
}

void Ctr2MappingEditor::updateConnectionStatus() {
    if (!m_connectionStatus || !m_connectButton)
        return;
    const bool connected = m_device && m_device->isConnected();
    const QString detail = m_device ? m_device->statusMessage()
                                    : QStringLiteral("CTR2 MIDI unavailable");
    m_connectionStatus->setText(detail);
    m_connectionStatus->setStyleSheet(QString("color:%1;font-size:%2px;font-weight:bold;")
                                          .arg(connected ? K4Styles::Colors::StatusGreen
                                                         : K4Styles::Colors::ErrorRed)
                                          .arg(K4Styles::Dimensions::FontSizeLarge));
    m_connectButton->setText(connected ? QStringLiteral("DISCONNECT")
                                       : QStringLiteral("CONNECT"));
    if (m_deviceSelector)
        m_deviceSelector->setEnabled(!connected);
}

QWidget *Ctr2MappingEditor::dialogParent() const {
    return window() ? window() : const_cast<Ctr2MappingEditor *>(this);
}

void Ctr2MappingEditor::ensureLoaded() {
    if (m_initialized) return;
    MidiMapping::DeviceMapping mapping = MidiMapping::ctr2Default();
    const QByteArray savedJson = RadioSettings::instance()->ctr2MidiMappingJson();
    if (!savedJson.isEmpty()) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(savedJson, &parseError);
        MidiMapping::DeviceMapping parsed;
        QString error;
        if (parseError.error == QJsonParseError::NoError && document.isObject() &&
            MidiMapping::fromJson(document.object(), &parsed, &error) &&
            parsed.profile == MidiMapping::Profile::Ctr2) {
            mapping = parsed;
        }
    }
    m_saved = mapping;
    m_draft = mapping;
    m_initialized = true;
}

void Ctr2MappingEditor::loadControls() {
    m_loading = true;
    const auto &mapping = m_draft;
    m_nameEdit->setText(mapping.name);
    m_cwEnabled->setChecked(mapping.cwInputEnabled);
    m_keyingMode->setCurrentIndex(m_keyingMode->findData(static_cast<int>(mapping.keyingMode)));
    m_tipRingSwapped->setChecked(mapping.tipRingSwapped);
    m_extendedButtons->setChecked(mapping.extendedButtons);
    for (int cc = 100; cc <= 107; ++cc) {
        const auto binding = mapping.knobs.value(cc, {QStringLiteral("disabled"), MidiMapping::KnobOutput::WheelA});
        m_knobRows[cc].action->setCurrentIndex(m_knobRows[cc].action->findData(binding.action));
        m_knobRows[cc].output->setCurrentIndex(
            m_knobRows[cc].output->findData(static_cast<int>(binding.output)));
    }
    rebuildButtonRows();
    m_loading = false;
    setDirty(m_dirty);
}

void Ctr2MappingEditor::rebuildButtonRows() {
    while (QLayoutItem *item = m_buttonRowsLayout->takeAt(0)) {
        if (item->widget()) delete item->widget();
        delete item;
    }
    qDeleteAll(m_buttonRows);
    m_buttonRows.clear();

    const QVector<int> notes = MidiMapping::ctr2ButtonNotes(m_draft.extendedButtons);
    for (int note : notes) {
        auto *container = new QWidget(this);
        container->setStyleSheet("background:transparent;");
        auto *rowLayout = new QHBoxLayout(container);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(7);
        auto *label = new QLabel(MidiMapping::ctr2ButtonLabel(m_draft.extendedButtons, note), container);
        label->setMinimumWidth(m_draft.extendedButtons ? 245 : 180);
        auto *actionCombo = new Ctr2InWindowComboBox("SELECT BUTTON ACTION", container);
        for (const QString &id : MidiMapping::supportedButtonActions())
            actionCombo->addItem(MidiMapping::buttonActionLabel(id), id);
        actionCombo->setMinimumHeight(40);
        actionCombo->setMinimumWidth(190);
        actionCombo->setStyleSheet(comboStyle());
        auto *command = new QLineEdit(container);
        command->setMinimumHeight(40);
        command->setPlaceholderText("e.g. SWT13; or KY CQ CQ;");
        command->setStyleSheet(comboStyle());
        rowLayout->addWidget(label);
        rowLayout->addWidget(actionCombo);
        rowLayout->addWidget(command, 1);
        m_buttonRowsLayout->addWidget(container);

        auto *row = new ButtonRow{note, actionCombo, command};
        m_buttonRows.append(row);
        const auto binding = m_draft.buttons.value(note, {QStringLiteral("disabled"), QString()});
        actionCombo->setCurrentIndex(actionCombo->findData(binding.action));
        if (binding.action == QStringLiteral("macro"))
            command->setText(m_draft.macros.value(binding.macroId).command);
        command->setVisible(binding.action == QStringLiteral("macro"));
        connect(actionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, row](int) {
            if (m_loading) return;
            row->command->setVisible(row->action->currentData().toString() == QStringLiteral("macro"));
            updateButtonBinding(row);
        });
        connect(command, &QLineEdit::textChanged, this, [this, row](const QString &) {
            if (!m_loading) updateButtonBinding(row);
        });
    }
    installInteractiveFilters();
    if (m_scroll && m_scroll->widget() && m_scroll->widget()->layout()) {
        m_scroll->widget()->layout()->activate();
        m_scroll->widget()->setMinimumHeight(m_scroll->widget()->layout()->sizeHint().height());
    }
}

void Ctr2MappingEditor::updateButtonBinding(ButtonRow *row) {
    const QString action = row->action->currentData().toString();
    MidiMapping::ButtonBinding binding;
    binding.action = action;
    if (action == QStringLiteral("macro")) {
        binding.macroId = QString("button-%1").arg(row->note);
        m_draft.macros[binding.macroId] = {
            MidiMapping::ctr2ButtonLabel(m_draft.extendedButtons, row->note),
            row->command->text().trimmed()};
    }
    m_draft.buttons[row->note] = binding;
    setDirty();
}

void Ctr2MappingEditor::setDirty(bool dirty) {
    m_dirty = dirty;
    m_status->setText(dirty ? QStringLiteral("Unsaved changes")
                            : QStringLiteral("Applied"));
    m_status->setStyleSheet(QString("color:%1;font-size:%2px;font-style:italic;")
                                .arg(dirty ? K4Styles::Colors::AccentAmber : K4Styles::Colors::StatusGreen)
                                .arg(K4Styles::Dimensions::FontSizeLarge));
}

MidiMapping::DeviceMapping Ctr2MappingEditor::sanitizedDraft() const {
    MidiMapping::DeviceMapping mapping = m_draft;
    QMap<QString, MidiMapping::MacroDefinition> used;
    for (auto it = mapping.buttons.cbegin(); it != mapping.buttons.cend(); ++it) {
        if (it->action == QStringLiteral("macro") && mapping.macros.contains(it->macroId))
            used.insert(it->macroId, mapping.macros.value(it->macroId));
    }
    mapping.macros = used;
    return mapping;
}

bool Ctr2MappingEditor::saveCurrent() {
    MidiMapping::DeviceMapping mapping = sanitizedDraft();
    QString validationError;
    if (!validateCommands(mapping, &validationError)) {
        showInWindowMessage(dialogParent(), "CTR2 mapping", validationError);
        return false;
    }
    const QByteArray json = QJsonDocument(MidiMapping::toJson(mapping)).toJson(QJsonDocument::Compact);
    RadioSettings::instance()->setCtr2MidiMappingJson(json);
    m_saved = mapping;
    m_draft = mapping;
    setDirty(false);
    return true;
}

bool Ctr2MappingEditor::hasUnsavedChanges() const {
    return m_dirty;
}

bool Ctr2MappingEditor::applyChanges() {
    return saveCurrent();
}

void Ctr2MappingEditor::abandonChanges() {
    if (!m_initialized || !m_dirty)
        return;

    m_draft = m_saved;
    m_dirty = false;
    loadControls();
}

void Ctr2MappingEditor::restoreDefaults() {
    m_draft = m_extendedButtons->isChecked()
                  ? MidiMapping::ctr2ExtendedDefault()
                  : MidiMapping::ctr2Default();
    m_dirty = true;
    loadControls();
}

int Ctr2MappingEditor::resolveDirtyBeforeImport() {
    if (!m_dirty) return 3;
    InWindowDialog dialog(dialogParent());
    QWidget *panel = dialog.contentWidget();
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(14, 12, 14, 12);
    auto *title = sectionLabel("Unsaved CTR2 mapping", panel);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);
    auto *message = new QLabel("Save the current mapping to a file before loading another file? Loading replaces the entire mapping; mappings are never merged.", panel);
    message->setWordWrap(true);
    layout->addWidget(message);
    auto *buttons = new QHBoxLayout;
    auto *cancel = new QPushButton("CANCEL", panel);
    auto *discard = new QPushButton("DON'T SAVE", panel);
    auto *save = new QPushButton("SAVE", panel);
    for (auto *button : {cancel, discard, save}) {
        button->setMinimumHeight(42);
        button->setStyleSheet(K4Styles::menuBarButton());
        buttons->addWidget(button);
    }
    layout->addLayout(buttons);
    connect(cancel, &QPushButton::clicked, &dialog, &InWindowDialog::reject);
    connect(discard, &QPushButton::clicked, &dialog, [&dialog]() { dialog.done(3); });
    connect(save, &QPushButton::clicked, &dialog, [&dialog]() { dialog.done(2); });
    dialog.setPanelSize(QSize(qMin(650, dialogParent()->width() - 20), 220));
    const int result = dialog.exec();
    if (result == 2 && !exportMapping()) return 0;
    return result;
}

void Ctr2MappingEditor::importMapping() {
    const QString path = QFileDialog::getOpenFileName(dialogParent(), "Load CTR2 mapping",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "QK4 CTR2 mapping (*.qk4ctr2map *.json);;All files (*)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        showInWindowMessage(dialogParent(), "CTR2 mapping", "The selected mapping file could not be opened.");
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    MidiMapping::DeviceMapping imported;
    QString error;
    if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
        !MidiMapping::fromJson(document.object(), &imported, &error) ||
        imported.profile != MidiMapping::Profile::Ctr2) {
        showInWindowMessage(dialogParent(), "CTR2 mapping",
                            error.isEmpty() ? "This is not a valid CTR2 mapping file." : error);
        return;
    }
    // Only pending edits require a warning. With no pending changes, loading
    // a selected valid file is immediate, exactly like the requested workflow.
    if (resolveDirtyBeforeImport() == 0)
        return;
    m_draft = imported;
    m_dirty = true;
    loadControls();
    saveCurrent();
}

bool Ctr2MappingEditor::exportMapping() {
    const MidiMapping::DeviceMapping mapping = sanitizedDraft();
    QString validationError;
    if (!validateCommands(mapping, &validationError)) {
        showInWindowMessage(dialogParent(), "CTR2 mapping", validationError);
        return false;
    }
    const QString suggested = m_draft.name.simplified().replace(' ', '_') + ".qk4ctr2map";
    const QString path = QFileDialog::getSaveFileName(dialogParent(), "Save CTR2 mapping",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" + suggested,
        "QK4 CTR2 mapping (*.qk4ctr2map)");
    if (path.isEmpty()) return false;
    const QByteArray fileData = QJsonDocument(MidiMapping::toJson(mapping)).toJson(QJsonDocument::Indented);
#ifdef Q_OS_ANDROID
    // Android's document picker can return a content URI. QFile uses Qt's
    // Android content file engine for that URI; QSaveFile's rename-based
    // commit cannot reliably do so.
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || file.write(fileData) != fileData.size()) {
        showInWindowMessage(dialogParent(), "CTR2 mapping", "The mapping file could not be saved.");
        return false;
    }
    file.close();
#else
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        showInWindowMessage(dialogParent(), "CTR2 mapping", "The mapping file could not be created.");
        return false;
    }
    file.write(fileData);
    if (!file.commit()) {
        showInWindowMessage(dialogParent(), "CTR2 mapping", "The mapping file could not be saved.");
        return false;
    }
#endif
    showInWindowMessage(dialogParent(), "CTR2 mapping", "Mapping file saved.");
    return true;
}
