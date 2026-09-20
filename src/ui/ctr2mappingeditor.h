#ifndef CTR2MAPPINGEDITOR_H
#define CTR2MAPPINGEDITOR_H

#include "../hardware/midimapping.h"

#include <QMap>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class Ctr2MidiDevice;

class Ctr2MappingEditor : public QWidget {
    Q_OBJECT

public:
    explicit Ctr2MappingEditor(Ctr2MidiDevice *device, QWidget *parent = nullptr);
    bool hasUnsavedChanges() const;
    bool applyChanges();
    void abandonChanges();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct KnobRow {
        QComboBox *action = nullptr;
        QComboBox *output = nullptr;
    };
    struct ButtonRow {
        int note = 0;
        QComboBox *action = nullptr;
        QLineEdit *command = nullptr;
    };

    void ensureLoaded();
    void loadControls();
    void rebuildButtonRows();
    void installInteractiveFilters();
    void populateMidiDevices();
    void updateConnectionStatus();
    void scanMidiDevices();
    void toggleConnection();
    void updateButtonBinding(ButtonRow *row);
    void setDirty(bool dirty = true);
    bool saveCurrent();
    void restoreDefaults();
    void importMapping();
    bool exportMapping();
    int resolveDirtyBeforeImport();
    MidiMapping::DeviceMapping sanitizedDraft() const;
    QWidget *dialogParent() const;

    bool m_loading = false;
    bool m_initialized = false;
    bool m_dirty = false;
    bool m_scrollGestureSuppressClick = false;
    MidiMapping::DeviceMapping m_saved;
    MidiMapping::DeviceMapping m_draft;
    Ctr2MidiDevice *m_device = nullptr;

    QLabel *m_status = nullptr;
    QLabel *m_connectionStatus = nullptr;
    QComboBox *m_deviceSelector = nullptr;
    QPushButton *m_scanButton = nullptr;
    QPushButton *m_connectButton = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QCheckBox *m_cwEnabled = nullptr;
    QComboBox *m_keyingMode = nullptr;
    QCheckBox *m_tipRingSwapped = nullptr;
    QCheckBox *m_extendedButtons = nullptr;
    QGridLayout *m_knobGrid = nullptr;
    QVBoxLayout *m_buttonRowsLayout = nullptr;
    QScrollArea *m_scroll = nullptr;
    QMap<int, KnobRow> m_knobRows;
    QVector<ButtonRow *> m_buttonRows;
};

#endif // CTR2MAPPINGEDITOR_H
