#ifndef FEATUREMENUBAR_H
#define FEATUREMENUBAR_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include "wheelaccumulator.h"

class FeatureMenuBar : public QWidget {
    Q_OBJECT

public:
    enum Feature { Attenuator, NbLevel, NrAdjust, ManualNotch };
    enum NrEngine { Lms, Ssnr };

    explicit FeatureMenuBar(QWidget *parent = nullptr);

    void showForFeature(Feature feature);
    void showAboveWidget(QWidget *referenceWidget); // Position popup above a reference widget
    void hideMenu();
    Feature currentFeature() const { return m_currentFeature; }
    bool isMenuVisible() const { return isVisible(); }

    // State updates (for Phase 2 RadioState integration)
    void setFeatureEnabled(bool enabled);
    void setValue(int value);
    void setValueUnit(const QString &unit);
    void setNbFilter(int filter); // 0=NONE, 1=NARROW, 2=WIDE
    NrEngine currentNrEngine() const { return m_nrEngine; }
    void setNrEngine(NrEngine engine);

signals:
    void toggleRequested();
    void incrementRequested();
    void decrementRequested();
    void extraButtonClicked();
    void nrEngineToggleRequested();
    void closed(); // Emitted when popup is hidden

protected:
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void setupUi();
    void updateForFeature();

    QLabel *m_titleLabel;
    QPushButton *m_toggleBtn;
    QPushButton *m_nrEngineBtn;
    QPushButton *m_extraBtn;
    QLabel *m_valueLabel;
    QPushButton *m_decrementBtn;
    QPushButton *m_incrementBtn;
    QPushButton *m_closeBtn;

    Feature m_currentFeature = Attenuator;
    bool m_featureEnabled = false;
    int m_value = 0;
    QString m_valueUnit;
    int m_nbFilter = 0;                   // 0=NONE, 1=NARROW, 2=WIDE
    NrEngine m_nrEngine = Lms;
    QWidget *m_referenceWidget = nullptr; // Widget to position relative to
    WheelAccumulator m_wheelAccumulator;
};

#endif // FEATUREMENUBAR_H
