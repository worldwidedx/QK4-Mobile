#pragma once
#include <QWidget>
#include "ft8/ft8types.h"

class PanadapterRhiWidget;
class QTimer;
class Ft8WaterfallOverlay;

class Ft8Waterfall : public QWidget {
    Q_OBJECT
public:
    explicit Ft8Waterfall(QWidget *parent = nullptr);
    void addSpectrum(const QVector<float> &db, double firstHz = 100, double binHz = 6.25);
    void setDecodes(const QVector<Ft8::Decode> &decodes);
    void setMarkers(int rx, int tx);
    void setPreview(int hz, bool tx); // hz == 0 hides the uncommitted marker.
    void setAppearance(int palette, int range);
    void reset();
    void zoom(double factor, double anchor = 0.5);
    void fit();
    double lowerHz() const { return m_low; }
    double spanHz() const { return m_span; }
    PanadapterRhiWidget *renderer() const { return m_renderer; }
signals:
    void frequencySelected(int hz);
    void txFrequencySelected(int hz);
    void stationSelected(const Ft8::Decode &decode, bool call);
    void ambiguousStations(const QVector<Ft8::Decode> &decodes);
    void viewChanged();

protected:
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    bool event(QEvent *) override;

private:
    friend class Ft8WaterfallOverlay;
    void choose(const QPoint &point, bool call);
    void cancelPress();
    void updateView();
    void clamp();
    double frequencyAt(int x) const;
    int xAt(double hz) const;
    int spectrumHeight() const { return qMax(1, (height() - 24) / 2); }
    PanadapterRhiWidget *m_renderer;
    Ft8WaterfallOverlay *m_overlay;
    QTimer *m_holdTimer;
    QVector<Ft8::Decode> m_decodes;
    int m_rx = 1500, m_tx = 1500;
    int m_previewHz = 0;
    bool m_previewTx = false;
    double m_low = 0, m_span = 3000, m_pressLow = 0;
    QPoint m_press;
    bool m_drag = false, m_double = false, m_pinching = false;
    bool m_pressed = false, m_held = false;
};
