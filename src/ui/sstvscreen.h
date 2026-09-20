#ifndef SSTVSCREEN_H
#define SSTVSCREEN_H

#include <QWidget>
#include <QFont>
#include <QImage>
#include "sstv/sstvstorage.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QComboBox;
class QCheckBox;
class QSlider;
class QSpinBox;
class QStackedWidget;
class QProgressBar;
class QBoxLayout;
class QScrollArea;
class QTimer;
class QKeyEvent;
class SstvComposerCanvas;

class SstvScreen : public QWidget {
    Q_OBJECT
public:
    explicit SstvScreen(QWidget *parent = nullptr);

    void setRfPower(double watts);
    void setTransmitProgress(int emittedSamples, int totalSamples, int imageSamples);
    void setTransmitting(bool active, const QString &detail = QString());
    void setTransmitStatus(const QString &detail);
    void setTransmitWarning(const QString &warning);
    void setTransmitProtection(const QString &text, bool fault);
    void setRadioOperatingState(const QString &rxFrequency, const QString &rxMode,
                                const QString &txFrequency, const QString &txMode);
    void returnToAutoReceive();
    void setReceiveStatus(const QString &status);
    void setReceiveInputLevel(int percent);
    void setReceiveStreamActive(bool active);
    void setReceiveImage(const QImage &image, int completedRows, int totalRows,
                         const QString &slantStatus);
    void completeReceiveImage(const QImage &image, int modeId, const QString &slantStatus,
                              qint64 frequencyHz);
    void receiveCallsignDetected(const QString &callsign, const QString &source, int confidence);
    void flushDraft();
    bool isTransmitting() const { return m_transmitting; }
    QString operatorCallsign() const { return m_operatorCallsign; }
    bool sendCallsignCw() const;
    bool sendCallsignFsk() const;
    int callsignCwWpm() const;

signals:
    void closeRequested();
    void transmitRequested(const QImage &frame, int modeId);
    void stopRequested();
    void powerRequested(double watts);
    void audioSetupRequested();
    void logQsoRequested(const QString &callsign, bool transmit, qint64 receivedFrequencyHz,
                         const QDateTime &receivedUtc);
    void logbookRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUi();
    void requestLogQso(bool transmit);
    void selectTab(bool transmit);
    void chooseImage();
    void chooseCameraImage();
    void pollImportedImage();
    void importImage(const QString &path);
    void rotateSource(int degrees);
    void resetFraming();
    void panFraming(const QPointF &normalizedDelta);
    void zoomFraming(double scaleFactor, const QPointF &normalizedAnchor);
    void refreshModeFrame();
    void refreshReceiveHistory(const QString &selectId = QString());
    void selectReceiveHistory(int index);
    void shareCurrentReceive();
    void toggleCurrentReceiveStar();
    void deleteCurrentReceive();
    void clearReceiveHistory();
    void saveCurrentReceiveCallsign(bool manual);
    void replyToCurrentReceive();
    void addOrEditText();
    void addVariableText(const QString &token);
    void chooseInkColor();
    void chooseFillColor();
    void refreshTemplates(const QString &selectName = QString());
    void applySelectedTemplate();
    void saveUserTemplate();
    void editSelectedTemplate();
    void deleteUserTemplate();
    void resetUserTemplates();
    void updateTemplateActionUi();
    void openImageTemplateGallery();
    bool saveCurrentImageTemplate(const QString &name, QString *error = nullptr);
    bool loadImageTemplate(const QString &name, QString *error = nullptr);
    QJsonObject currentTxState() const;
    void restoreTxState(const QImage &source, const QJsonObject &state,
                        const QString &status);
    void scheduleDraftSave();
    void saveDraft();
    void restoreDraft();
    void clearDraft();
    void updateComposerControls();
    void updatePowerUi();
    void updateTransmitUi();
    void updateCallsignControlsLayout(bool portrait);
    void cancelTransmitConfirmation();
    void setSliderValueFromTouchPosition(QSlider *slider, int xPosition);
    QFont composerFontFromControls() const;
    void syncTextControlsFromSelection();

    QPushButton *m_receiveTab = nullptr;
    QPushButton *m_transmitTab = nullptr;
    QPushButton *m_backButton = nullptr;
    QStackedWidget *m_pages = nullptr;
    QBoxLayout *m_transmitLayout = nullptr;
    QBoxLayout *m_callsignRowLayout = nullptr;
    QBoxLayout *m_callsignIdRowLayout = nullptr;
    QWidget *m_callsignIdRowContainer = nullptr;
    QScrollArea *m_txControlsScroll = nullptr;
    QLabel *m_receiveStatus = nullptr;
    QLabel *m_rxRadioState = nullptr;
    QLabel *m_txRadioState = nullptr;
    QLabel *m_receiveStreamState = nullptr;
    QLabel *m_receiveImage = nullptr;
    QComboBox *m_receiveHistoryCombo = nullptr;
    QComboBox *m_retentionCombo = nullptr;
    QPushButton *m_starReceiveButton = nullptr;
    QPushButton *m_shareReceiveButton = nullptr;
    QPushButton *m_deleteReceiveButton = nullptr;
    QPushButton *m_clearReceiveHistoryButton = nullptr;
    QLineEdit *m_receiveCallsignEdit = nullptr;
    QLabel *m_receiveCallsignSource = nullptr;
    QPushButton *m_replyReceiveButton = nullptr;
    SstvComposerCanvas *m_composer = nullptr;
    QLabel *m_txFrameLabel = nullptr;
    QLabel *m_modeDetail = nullptr;
    QLabel *m_powerLabel = nullptr;
    QLabel *m_protectionLabel = nullptr;
    QLabel *m_txStateLabel = nullptr;
    QLabel *m_frameZoomLabel = nullptr;
    QProgressBar *m_txProgress = nullptr;
    QProgressBar *m_receiveLevel = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QComboBox *m_fontCombo = nullptr;
    QComboBox *m_templateCombo = nullptr;
    QLineEdit *m_callsignEdit = nullptr;
    QLineEdit *m_toCallsignEdit = nullptr;
    QCheckBox *m_fskIdCheck = nullptr;
    QCheckBox *m_cwIdCheck = nullptr;
    QSpinBox *m_cwWpmSpin = nullptr;
    QPushButton *m_cwWpmMinus = nullptr;
    QPushButton *m_cwWpmPlus = nullptr;
    QSlider *m_textSizeSlider = nullptr;
    QSlider *m_powerSlider = nullptr;
    QSlider *m_frameZoomSlider = nullptr;
    QPushButton *m_powerMinus = nullptr;
    QPushButton *m_powerPlus = nullptr;
    QPushButton *m_galleryButton = nullptr;
    QPushButton *m_imageTemplateGalleryButton = nullptr;
    QPushButton *m_cameraButton = nullptr;
    QPushButton *m_rotateLeftButton = nullptr;
    QPushButton *m_rotateRightButton = nullptr;
    QPushButton *m_resetFrameButton = nullptr;
    QPushButton *m_fitModeButton = nullptr;
    QPushButton *m_transmitButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_selectToolButton = nullptr;
    QPushButton *m_drawToolButton = nullptr;
    QPushButton *m_shapeToolButton = nullptr;
    QPushButton *m_arrowToolButton = nullptr;
    QPushButton *m_rectangleToolButton = nullptr;
    QPushButton *m_ellipseToolButton = nullptr;
    QPushButton *m_textButton = nullptr;
    QPushButton *m_myCallVariableButton = nullptr;
    QPushButton *m_toCallVariableButton = nullptr;
    QPushButton *m_colorButton = nullptr;
    QPushButton *m_fillColorButton = nullptr;
    QPushButton *m_rotateObjectLeftButton = nullptr;
    QPushButton *m_rotateObjectRightButton = nullptr;
    QPushButton *m_undoButton = nullptr;
    QPushButton *m_redoButton = nullptr;
    QPushButton *m_resetCompositionButton = nullptr;
    QPushButton *m_deleteObjectButton = nullptr;
    QPushButton *m_saveTemplateButton = nullptr;
    QPushButton *m_editTemplateButton = nullptr;
    QPushButton *m_deleteTemplateButton = nullptr;
    QPushButton *m_resetTemplatesButton = nullptr;
    QPushButton *m_clearDraftButton = nullptr;
    QTimer *m_mediaPollTimer = nullptr;
    QTimer *m_draftTimer = nullptr;
    SstvStorage m_storage;
    QVector<SstvRxRecord> m_receiveRecords;
    QString m_currentReceiveId;
    QString m_pendingReceiveCallsignId;
    QString m_operatorCallsign;
    QString m_replyCallsign;
    QString m_transmitWarning;
    QString m_editingTemplateName;
    QString m_receiveCallsignSourceValue;
    int m_receiveCallsignConfidence = 0;
    int m_callsignControlsPortrait = -1;
    QImage m_currentReceiveImage;
    QImage m_sourceImage;
    QImage m_modeFrame;
    QImage m_frozenTransmitFrame;
    QPointF m_frameCenter = QPointF(0.5, 0.5);
    double m_frameZoom = 1.0;
    double m_powerWatts = 25.0;
    bool m_transmitting = false;
    bool m_transmitConfirmationPending = false;
    bool m_mediaRequestPending = false;
    bool m_fitBars = false;
    bool m_restoringDraft = false;
    bool m_draftSourceDirty = true;
    QColor m_composerColor = Qt::black;
    QColor m_composerFillColor = Qt::white;
    QSlider *m_txSliderDragTarget = nullptr;
    QPoint m_txSliderPressPosition;
    int m_txSliderLastY = 0;
    bool m_txSliderScrolling = false;
    bool m_txSliderAdjusting = false;
    bool m_txScrollGestureSuppressClick = false;
};

#endif // SSTVSCREEN_H
