#ifndef SSTVCOMPOSERCANVAS_H
#define SSTVCOMPOSERCANVAS_H

#include <QColor>
#include <QFont>
#include <QImage>
#include <QJsonObject>
#include <QPainterPath>
#include <QPointF>
#include <QTransform>
#include <QVector>
#include <QWidget>

class QPainter;

// Touch-first, non-destructive SSTV composition canvas. Coordinates are kept
// in the selected mode's native pixel space so the preview and TX frame match.
class SstvComposerCanvas : public QWidget {
    Q_OBJECT
public:
    enum class Tool { Select, Draw, Shape };
    enum class ShapeType { Line, Arrow, Rectangle, Ellipse };
    enum class ObjectType { None, Text, Stroke, Line, Arrow, Rectangle, Ellipse };

    explicit SstvComposerCanvas(QWidget *parent = nullptr);

    void setBackground(const QImage &image);
    QImage renderedImage() const;
    bool hasBackground() const { return !m_background.isNull(); }

    void setTool(Tool tool);
    Tool tool() const { return m_tool; }
    void setInk(const QColor &color, int width);
    void setFillColor(const QColor &color);
    QColor inkColor() const { return m_outlineColor; }
    QColor fillColor() const { return m_fillColor; }
    void setShapeType(ShapeType type) { m_shapeType = type; }
    ShapeType shapeType() const { return m_shapeType; }
    void setCallsignValues(const QString &myCall, const QString &toCall);
    void addTextBlock(const QString &text, const QFont &font, const QColor &color,
                      const QPointF &normalizedPosition = QPointF(0.5, 0.5));
    void updateSelectedText(const QString &text, const QFont &font, const QColor &color);
    void updateSelectedTextColor(const QColor &color);
    void updateSelectedTextFont(const QFont &font);
    void updateSelectedOutlineColor(const QColor &color);
    void updateSelectedFillColor(const QColor &color);
    void updateSelectedSize(int controlValue);
    bool hasSelectedObject() const;
    ObjectType selectedObjectType() const;
    bool selectedObjectSupportsFill() const;
    bool hasSelectedText() const;
    QString selectedText() const;
    QFont selectedTextFont() const;
    QColor selectedTextColor() const;
    QColor selectedOutlineColor() const;
    QColor selectedFillColor() const;
    int selectedSize() const;
    void deleteSelectedObject();
    void deleteSelectedText();
    void rotateSelectedObject(int degrees);
    bool hasUnresolvedVariables(QString *variable = nullptr) const;
    QJsonObject compositionState() const;
    bool restoreCompositionState(const QJsonObject &state);
    void undo();
    void redo();
    void resetComposition();
    // A newly selected source image starts a new editing session. Unlike the
    // user-facing reset command, old markup must not remain in undo history.
    void clearCompositionForNewImage();
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

    static QString myCallToken() { return QStringLiteral("{MY_CALL}"); }
    static QString toCallToken() { return QStringLiteral("{TO_CALL}"); }

signals:
    void compositionChanged();
    void selectionChanged(bool objectSelected);
    void backgroundPanRequested(const QPointF &normalizedDelta);
    void backgroundZoomRequested(qreal scaleFactor, const QPointF &normalizedAnchor);

protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct OverlayObject {
        ObjectType type = ObjectType::Text;
        QString text;
        QFont font;
        QColor outlineColor = Qt::white;
        QColor fillColor = Qt::transparent;
        int width = 4;
        QVector<QPointF> points;
        QPointF position;
        QPointF start;
        QPointF end;
        qreal rotation = 0.0;
    };
    struct Snapshot {
        QVector<OverlayObject> objects;
    };

    QRectF imageRect() const;
    QPointF imagePoint(const QPointF &widgetPoint) const;
    QImage renderComposition() const;
    QString resolvedText(const QString &text, bool showPlaceholders = true) const;
    QRectF objectBounds(const OverlayObject &object) const;
    QPointF objectCenter(const OverlayObject &object) const;
    QTransform objectTransform(const OverlayObject &object) const;
    QPainterPath objectPath(const OverlayObject &object) const;
    QVector<int> objectsAt(const QPointF &imagePoint) const;
    bool objectContains(const OverlayObject &object, const QPointF &imagePoint) const;
    void drawObject(QPainter &painter, const OverlayObject &object) const;
    void moveObject(OverlayObject &object, const QPointF &delta);
    void saveUndo();
    void restore(const Snapshot &snapshot);
    void clearSelection();
    void emitChanged();

    QImage m_background;
    QVector<OverlayObject> m_objects;
    QVector<Snapshot> m_undo;
    QVector<Snapshot> m_redo;
    Tool m_tool = Tool::Select;
    QColor m_outlineColor = Qt::black;
    QColor m_fillColor = Qt::white;
    int m_inkWidth = 4;
    ShapeType m_shapeType = ShapeType::Line;
    int m_selectedObject = -1;
    bool m_draggingObject = false;
    bool m_panningBackground = false;
    bool m_dragUndoCaptured = false;
    QPointF m_lastPoint;
    QPointF m_lastSelectionPoint;
    QString m_myCall;
    QString m_toCall;
    static constexpr int MaxHistory = 20;
};

#endif // SSTVCOMPOSERCANVAS_H
