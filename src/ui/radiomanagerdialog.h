#ifndef RADIOMANAGERDIALOG_H
#define RADIOMANAGERDIALOG_H

#include <QCheckBox>
#include <QComboBox>
#include <QWidget>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include "settings/radiosettings.h"

class QResizeEvent;

class RadioManagerDialog : public QWidget {
    Q_OBJECT

public:
    explicit RadioManagerDialog(QWidget *parent = nullptr);

    RadioEntry selectedRadio() const;
    bool hasSelection() const;

    // Set the currently connected radio host (empty string if disconnected)
    void setConnectedHost(const QString &host);

signals:
    void connectRequested(const RadioEntry &radio);
    void disconnectRequested();
    void closeRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onConnectClicked();
    void onNewClicked();
    void onSaveClicked();
    void onDeleteClicked();
    void onBackClicked();
    void onSelectionChanged();
    void onItemDoubleClicked(QListWidgetItem *item);
    void onTlsCheckboxToggled(bool checked);
    void refreshList();

private:
    void setupUi();
    void updateButtonStates();
    void clearFields();
    void populateFieldsFromSelection();

    QListWidget *m_radioList;
    QLineEdit *m_nameEdit;
    QLineEdit *m_hostEdit;
    QLineEdit *m_portEdit;
    QLineEdit *m_passwordEdit;
    QCheckBox *m_tlsCheckbox;
    QLineEdit *m_identityEdit;
    QLabel *m_identityLabel;
    QComboBox *m_encodeModeCombo;
    QComboBox *m_streamingLatencyCombo;

    QPushButton *m_connectButton;
    QPushButton *m_newButton;
    QPushButton *m_saveButton;
    QPushButton *m_deleteButton;
    QPushButton *m_backButton;

    int m_currentIndex;
    QString m_connectedHost; // Host of currently connected radio (empty if disconnected)
};

#endif // RADIOMANAGERDIALOG_H
