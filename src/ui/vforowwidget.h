#ifndef VFOROWWIDGET_H
#define VFOROWWIDGET_H

#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>
#include <QWidget>

class FilterIndicatorWidget;

/**
 * VfoSquareWidget - Custom painted VFO A/B indicator with lock arc
 *
 * Draws a rounded square with "A" or "B" text, and optionally a
 * semi-circular arc on top to indicate lock state (padlock effect).
 */
class VfoSquareWidget : public QWidget {
    Q_OBJECT

public:
    explicit VfoSquareWidget(const QString &text, const QColor &color, QWidget *parent = nullptr);

    void setLocked(bool locked);
    bool isLocked() const { return m_locked; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_text;
    QColor m_color;
    bool m_locked = false;
};

/**
 * VfoRowWidget - First row of center section with absolute positioning
 *
 * Contains: A square, TX indicator, B square, SUB/DIV indicators
 * Uses absolute positioning to perfectly center TX regardless of
 * asymmetric SUB/DIV on B side.
 */
class VfoRowWidget : public QWidget {
    Q_OBJECT

public:
    explicit VfoRowWidget(QWidget *parent = nullptr);

    // Lock state setters
    void setLockA(bool locked);
    void setLockB(bool locked);

    // Accessors for MainWindow to connect signals and install event filters
    VfoSquareWidget *vfoASquare() const { return m_vfoASquare; }
    VfoSquareWidget *vfoBSquare() const { return m_vfoBSquare; }
    QLabel *modeALabel() const { return m_modeALabel; }
    QLabel *modeBLabel() const { return m_modeBLabel; }
    QLabel *txIndicator() const { return m_txIndicator; }
    QLabel *txTriangle() const { return m_txTriangle; }
    QLabel *txTriangleB() const { return m_txTriangleB; }
    QLabel *testLabel() const { return m_testLabel; }
    QLabel *subLabel() const { return m_subLabel; }
    QLabel *divLabel() const { return m_divLabel; }
    // Filter indicators live under each VFO square+mode (like the radio).
    FilterIndicatorWidget *filterAWidget() const { return m_filterAWidget; }
    FilterIndicatorWidget *filterBWidget() const { return m_filterBWidget; }

    // Stack a widget in the centre (TX) column, beneath the TX glyph. Used to
    // pull SPLIT / MSG / RIT-XIT up between the two VFO filters, as on the
    // radio. Reparents w and regrows the row to fit.
    void addToCenterColumn(QWidget *w);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupWidgets();
    void positionWidgets();
    void recomputeHeight();

    // TX (centre) column layout, so extra widgets can be stacked under TX.
    QVBoxLayout *m_txColumn = nullptr;

    // Containers (absolute positioned within this widget)
    QWidget *m_vfoAContainer;
    QWidget *m_txContainer;
    QWidget *m_vfoBContainer;
    QWidget *m_subDivContainer;

    // VFO squares (custom painted for lock arc)
    VfoSquareWidget *m_vfoASquare;
    VfoSquareWidget *m_vfoBSquare;
    // Labels within containers
    QLabel *m_modeALabel;
    QLabel *m_modeBLabel;
    QLabel *m_txIndicator;
    QLabel *m_txTriangle;
    QLabel *m_txTriangleB;
    QLabel *m_testLabel;
    QLabel *m_subLabel;
    QLabel *m_divLabel;
    FilterIndicatorWidget *m_filterAWidget = nullptr;
    FilterIndicatorWidget *m_filterBWidget = nullptr;
};

#endif // VFOROWWIDGET_H
