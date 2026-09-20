#include "sstvcomposercanvas.h"

#include <QGestureEvent>
#include <QJsonArray>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPinchGesture>
#include <QPolygonF>
#include <QStringList>
#include <QTransform>

#include <cmath>

namespace {
QRectF textBounds(const QFont &font, const QString &text) {
    const QFontMetricsF metrics(font);
    const QStringList lines = text.split(QLatin1Char('\n'));
    qreal width = 0.0;
    for (const QString &line : lines)
        width = qMax(width, metrics.horizontalAdvance(line));
    return QRectF(0.0, 0.0, width, metrics.lineSpacing() * qMax(1, lines.size()));
}

QPainterPath textPath(const QFont &font, const QPointF &center, const QString &text) {
    const QFontMetricsF metrics(font);
    const QRectF bounds = textBounds(font, text);
    const QPointF topLeft = center - QPointF(bounds.width() * 0.5, bounds.height() * 0.5);
    const QStringList lines = text.split(QLatin1Char('\n'));
    QPainterPath path;
    for (int i = 0; i < lines.size(); ++i) {
        path.addText(QPointF(topLeft.x(),
                             topLeft.y() + metrics.ascent() + i * metrics.lineSpacing()),
                     font, lines.at(i));
    }
    // Font glyphs use contour direction to distinguish counters (holes) from
    // overlapping components. Odd-even simplification can cancel component
    // intersections and turn them into false holes. Winding fill unions those
    // crossings while retaining correctly directed counters.
    path.setFillRule(Qt::WindingFill);
    return path;
}

QString objectTypeName(SstvComposerCanvas::ObjectType type) {
    switch (type) {
    case SstvComposerCanvas::ObjectType::Text: return QStringLiteral("text");
    case SstvComposerCanvas::ObjectType::Stroke: return QStringLiteral("stroke");
    case SstvComposerCanvas::ObjectType::Line: return QStringLiteral("line");
    case SstvComposerCanvas::ObjectType::Arrow: return QStringLiteral("arrow");
    case SstvComposerCanvas::ObjectType::Rectangle: return QStringLiteral("rectangle");
    case SstvComposerCanvas::ObjectType::Ellipse: return QStringLiteral("ellipse");
    case SstvComposerCanvas::ObjectType::None: break;
    }
    return QStringLiteral("none");
}

SstvComposerCanvas::ObjectType objectTypeFromName(const QString &name) {
    if (name == QStringLiteral("text")) return SstvComposerCanvas::ObjectType::Text;
    if (name == QStringLiteral("stroke")) return SstvComposerCanvas::ObjectType::Stroke;
    if (name == QStringLiteral("line")) return SstvComposerCanvas::ObjectType::Line;
    if (name == QStringLiteral("arrow")) return SstvComposerCanvas::ObjectType::Arrow;
    if (name == QStringLiteral("rectangle")) return SstvComposerCanvas::ObjectType::Rectangle;
    if (name == QStringLiteral("ellipse")) return SstvComposerCanvas::ObjectType::Ellipse;
    return SstvComposerCanvas::ObjectType::None;
}

SstvComposerCanvas::ObjectType objectTypeFromShape(SstvComposerCanvas::ShapeType type) {
    switch (type) {
    case SstvComposerCanvas::ShapeType::Line: return SstvComposerCanvas::ObjectType::Line;
    case SstvComposerCanvas::ShapeType::Arrow: return SstvComposerCanvas::ObjectType::Arrow;
    case SstvComposerCanvas::ShapeType::Rectangle: return SstvComposerCanvas::ObjectType::Rectangle;
    case SstvComposerCanvas::ShapeType::Ellipse: return SstvComposerCanvas::ObjectType::Ellipse;
    }
    return SstvComposerCanvas::ObjectType::Line;
}

qreal distanceToSegment(const QPointF &point, const QPointF &start, const QPointF &end) {
    const QPointF delta = end - start;
    const qreal lengthSquared = QPointF::dotProduct(delta, delta);
    if (lengthSquared <= 0.0001)
        return QLineF(point, start).length();
    const qreal projection = qBound(0.0,
                                    QPointF::dotProduct(point - start, delta) / lengthSquared,
                                    1.0);
    return QLineF(point, start + delta * projection).length();
}

qreal normalizedRotation(qreal degrees) {
    qreal result = std::fmod(degrees, 360.0);
    if (result < 0.0)
        result += 360.0;
    return result;
}
}

SstvComposerCanvas::SstvComposerCanvas(QWidget *parent) : QWidget(parent) {
    setMinimumSize(240, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_AcceptTouchEvents);
    grabGesture(Qt::PinchGesture);
    setStyleSheet(QStringLiteral("background: #1b2022; border: 1px solid #f2ad20;"));
}

bool SstvComposerCanvas::event(QEvent *event) {
    if (event->type() == QEvent::Gesture && !m_background.isNull()) {
        auto *gestureEvent = static_cast<QGestureEvent *>(event);
        if (auto *pinch = static_cast<QPinchGesture *>(
                gestureEvent->gesture(Qt::PinchGesture))) {
            if (pinch->state() == Qt::GestureStarted) {
                m_draggingObject = false;
                m_panningBackground = false;
                m_dragUndoCaptured = false;
            } else if (pinch->state() == Qt::GestureUpdated
                       && pinch->changeFlags().testFlag(QPinchGesture::ScaleFactorChanged)) {
                const qreal factor = pinch->scaleFactor();
                const QRectF target = imageRect();
                if (!target.isEmpty() && qAbs(factor - 1.0) > 0.001) {
                    const QPointF center = pinch->centerPoint();
                    const QPointF anchor(
                        qBound(0.0, (center.x() - target.left()) / target.width(), 1.0),
                        qBound(0.0, (center.y() - target.top()) / target.height(), 1.0));
                    emit backgroundZoomRequested(factor, anchor);
                }
            }
            gestureEvent->accept(pinch);
            return true;
        }
    }
    return QWidget::event(event);
}

void SstvComposerCanvas::setBackground(const QImage &image) {
    if (image.isNull())
        return;
    const QSize oldSize = m_background.size();
    m_background = image.convertToFormat(QImage::Format_RGB32);
    if (!oldSize.isEmpty() && oldSize != m_background.size()) {
        const qreal xScale = static_cast<qreal>(m_background.width()) / oldSize.width();
        const qreal yScale = static_cast<qreal>(m_background.height()) / oldSize.height();
        const qreal widthScale = (xScale + yScale) * 0.5;
        for (OverlayObject &object : m_objects) {
            object.width = qMax(1, qRound(object.width * widthScale));
            object.position = QPointF(object.position.x() * xScale,
                                      object.position.y() * yScale);
            object.start = QPointF(object.start.x() * xScale, object.start.y() * yScale);
            object.end = QPointF(object.end.x() * xScale, object.end.y() * yScale);
            for (QPointF &point : object.points)
                point = QPointF(point.x() * xScale, point.y() * yScale);
        }
    }
    update();
}

QImage SstvComposerCanvas::renderedImage() const {
    return renderComposition();
}

void SstvComposerCanvas::setTool(Tool tool) {
    m_tool = tool;
    setCursor(tool == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
    update();
}

void SstvComposerCanvas::setInk(const QColor &color, int width) {
    if (color.isValid())
        m_outlineColor = color;
    m_inkWidth = qBound(1, width, 40);
}

void SstvComposerCanvas::setFillColor(const QColor &color) {
    m_fillColor = color.isValid() ? color : QColor(Qt::transparent);
}

void SstvComposerCanvas::setCallsignValues(const QString &myCall, const QString &toCall) {
    const QString normalizedMyCall = myCall.trimmed().toUpper();
    const QString normalizedToCall = toCall.trimmed().toUpper();
    if (m_myCall == normalizedMyCall && m_toCall == normalizedToCall)
        return;
    m_myCall = normalizedMyCall;
    m_toCall = normalizedToCall;
    update();
}

void SstvComposerCanvas::addTextBlock(const QString &text, const QFont &font,
                                      const QColor &color,
                                      const QPointF &normalizedPosition) {
    if (m_background.isNull() || text.trimmed().isEmpty())
        return;
    saveUndo();
    OverlayObject object;
    object.type = ObjectType::Text;
    object.text = text;
    object.font = font;
    object.outlineColor = m_outlineColor;
    object.fillColor = color.isValid() ? color : m_fillColor;
    object.position = QPointF(qBound(0.0, normalizedPosition.x(), 1.0) * m_background.width(),
                              qBound(0.0, normalizedPosition.y(), 1.0) * m_background.height());
    m_objects.append(object);
    m_selectedObject = m_objects.size() - 1;
    emitChanged();
}

void SstvComposerCanvas::updateSelectedText(const QString &text, const QFont &font,
                                            const QColor &color) {
    if (!hasSelectedText() || text.trimmed().isEmpty())
        return;
    saveUndo();
    OverlayObject &object = m_objects[m_selectedObject];
    object.text = text;
    object.font = font;
    object.fillColor = color.isValid() ? color : object.fillColor;
    emitChanged();
}

void SstvComposerCanvas::updateSelectedTextColor(const QColor &color) {
    updateSelectedFillColor(color);
}

void SstvComposerCanvas::updateSelectedTextFont(const QFont &font) {
    if (!hasSelectedText() || m_objects.at(m_selectedObject).font == font)
        return;
    saveUndo();
    m_objects[m_selectedObject].font = font;
    emitChanged();
}

void SstvComposerCanvas::updateSelectedOutlineColor(const QColor &color) {
    if (!hasSelectedObject() || !color.isValid()
        || m_objects.at(m_selectedObject).outlineColor == color) {
        return;
    }
    saveUndo();
    m_objects[m_selectedObject].outlineColor = color;
    m_outlineColor = color;
    emitChanged();
}

void SstvComposerCanvas::updateSelectedFillColor(const QColor &color) {
    if (!hasSelectedObject())
        return;
    const QColor selected = color.isValid() ? color : QColor(Qt::transparent);
    if (m_objects.at(m_selectedObject).fillColor == selected)
        return;
    saveUndo();
    m_objects[m_selectedObject].fillColor = selected;
    m_fillColor = selected;
    emitChanged();
}

void SstvComposerCanvas::updateSelectedSize(int controlValue) {
    if (!hasSelectedObject())
        return;
    const int boundedValue = qBound(8, controlValue, 160);
    if (m_objects.at(m_selectedObject).type == ObjectType::Text) {
        if (m_objects.at(m_selectedObject).font.pixelSize() == boundedValue)
            return;
        saveUndo();
        m_objects[m_selectedObject].font.setPixelSize(boundedValue);
    } else {
        const int width = qBound(1, qRound(boundedValue / 5.0), 40);
        if (m_objects.at(m_selectedObject).width == width)
            return;
        saveUndo();
        m_objects[m_selectedObject].width = width;
    }
    emitChanged();
}

bool SstvComposerCanvas::hasSelectedObject() const {
    return m_selectedObject >= 0 && m_selectedObject < m_objects.size();
}

SstvComposerCanvas::ObjectType SstvComposerCanvas::selectedObjectType() const {
    return hasSelectedObject() ? m_objects.at(m_selectedObject).type : ObjectType::None;
}

bool SstvComposerCanvas::selectedObjectSupportsFill() const {
    return hasSelectedObject();
}

bool SstvComposerCanvas::hasSelectedText() const {
    return selectedObjectType() == ObjectType::Text;
}

QString SstvComposerCanvas::selectedText() const {
    return hasSelectedText() ? m_objects.at(m_selectedObject).text : QString();
}

QFont SstvComposerCanvas::selectedTextFont() const {
    return hasSelectedText() ? m_objects.at(m_selectedObject).font : QFont();
}

QColor SstvComposerCanvas::selectedTextColor() const {
    return hasSelectedText() ? m_objects.at(m_selectedObject).fillColor : QColor();
}

QColor SstvComposerCanvas::selectedOutlineColor() const {
    return hasSelectedObject() ? m_objects.at(m_selectedObject).outlineColor : QColor();
}

QColor SstvComposerCanvas::selectedFillColor() const {
    return hasSelectedObject() ? m_objects.at(m_selectedObject).fillColor : QColor();
}

int SstvComposerCanvas::selectedSize() const {
    if (!hasSelectedObject())
        return -1;
    const OverlayObject &object = m_objects.at(m_selectedObject);
    return object.type == ObjectType::Text ? object.font.pixelSize() : object.width * 5;
}

void SstvComposerCanvas::deleteSelectedObject() {
    if (!hasSelectedObject())
        return;
    saveUndo();
    m_objects.removeAt(m_selectedObject);
    clearSelection();
    emitChanged();
}

void SstvComposerCanvas::deleteSelectedText() {
    if (hasSelectedText())
        deleteSelectedObject();
}

void SstvComposerCanvas::rotateSelectedObject(int degrees) {
    if (!hasSelectedObject() || degrees == 0)
        return;
    saveUndo();
    m_objects[m_selectedObject].rotation =
        normalizedRotation(m_objects.at(m_selectedObject).rotation + degrees);
    emitChanged();
}

bool SstvComposerCanvas::hasUnresolvedVariables(QString *variable) const {
    for (const OverlayObject &object : m_objects) {
        if (object.type != ObjectType::Text)
            continue;
        if (m_myCall.isEmpty()
            && object.text.contains(myCallToken(), Qt::CaseInsensitive)) {
            if (variable)
                *variable = myCallToken();
            return true;
        }
        if (m_toCall.isEmpty()
            && object.text.contains(toCallToken(), Qt::CaseInsensitive)) {
            if (variable)
                *variable = toCallToken();
            return true;
        }
    }
    return false;
}

QJsonObject SstvComposerCanvas::compositionState() const {
    QJsonArray objects;
    const qreal imageWidth = qMax(1, m_background.width());
    const qreal imageHeight = qMax(1, m_background.height());
    for (const OverlayObject &object : m_objects) {
        QJsonObject encoded{{QStringLiteral("type"), objectTypeName(object.type)},
                            {QStringLiteral("outlineColor"),
                             object.outlineColor.name(QColor::HexArgb)},
                            {QStringLiteral("fillColor"),
                             object.fillColor.name(QColor::HexArgb)},
                            {QStringLiteral("width"), object.width / imageWidth},
                            {QStringLiteral("rotation"), object.rotation}};
        if (object.type == ObjectType::Text) {
            encoded.insert(QStringLiteral("text"), object.text);
            encoded.insert(QStringLiteral("font"), object.font.family());
            encoded.insert(QStringLiteral("pixelSize"), object.font.pixelSize());
            encoded.insert(QStringLiteral("weight"), object.font.weight());
            encoded.insert(QStringLiteral("stretch"), object.font.stretch());
            encoded.insert(QStringLiteral("bold"), object.font.bold());
            encoded.insert(QStringLiteral("italic"), object.font.italic());
            encoded.insert(QStringLiteral("x"), object.position.x() / imageWidth);
            encoded.insert(QStringLiteral("y"), object.position.y() / imageHeight);
        } else if (object.type == ObjectType::Stroke) {
            QJsonArray points;
            for (const QPointF &point : object.points)
                points.append(QJsonArray{point.x() / imageWidth, point.y() / imageHeight});
            encoded.insert(QStringLiteral("points"), points);
        } else {
            encoded.insert(QStringLiteral("x1"), object.start.x() / imageWidth);
            encoded.insert(QStringLiteral("y1"), object.start.y() / imageHeight);
            encoded.insert(QStringLiteral("x2"), object.end.x() / imageWidth);
            encoded.insert(QStringLiteral("y2"), object.end.y() / imageHeight);
        }
        objects.append(encoded);
    }
    return {{QStringLiteral("version"), 3}, {QStringLiteral("objects"), objects}};
}

bool SstvComposerCanvas::restoreCompositionState(const QJsonObject &state) {
    if (m_background.isNull())
        return false;
    QVector<OverlayObject> objects;
    const int stateVersion = state.value(QStringLiteral("version")).toInt(1);
    const qreal imageWidth = qMax(1, m_background.width());
    const qreal imageHeight = qMax(1, m_background.height());

    const auto decodeFont = [](const QJsonObject &encoded) {
        QFont font(encoded.value(QStringLiteral("font"))
                       .toString(QStringLiteral("Sans Serif")));
        font.setPixelSize(qBound(8, encoded.value(QStringLiteral("pixelSize")).toInt(28), 160));
        if (encoded.contains(QStringLiteral("weight"))) {
            font.setWeight(static_cast<QFont::Weight>(
                qBound(static_cast<int>(QFont::Thin),
                       encoded.value(QStringLiteral("weight"))
                           .toInt(static_cast<int>(QFont::Normal)),
                       static_cast<int>(QFont::Black))));
        } else {
            font.setBold(encoded.value(QStringLiteral("bold")).toBool(true));
        }
        const int storedStretch = encoded.value(QStringLiteral("stretch"))
                                      .toInt(QFont::Unstretched);
        font.setStretch(storedStretch > 0 ? qBound(1, storedStretch, 400)
                                          : static_cast<int>(QFont::Unstretched));
        font.setItalic(encoded.value(QStringLiteral("italic")).toBool(false));
        return font;
    };
    const auto decodeColor = [](const QJsonValue &value, const QColor &fallback) {
        const QColor color(value.toString(fallback.name(QColor::HexArgb)));
        return color.isValid() ? color : fallback;
    };
    const auto normalizedPoint = [imageWidth, imageHeight](const QJsonObject &encoded,
                                                           const QString &x,
                                                           const QString &y,
                                                           qreal fallback = 0.5) {
        return QPointF(qBound(0.0, encoded.value(x).toDouble(fallback), 1.0) * imageWidth,
                       qBound(0.0, encoded.value(y).toDouble(fallback), 1.0) * imageHeight);
    };

    if (state.value(QStringLiteral("objects")).isArray()) {
        for (const QJsonValue &value : state.value(QStringLiteral("objects")).toArray()) {
            const QJsonObject encoded = value.toObject();
            OverlayObject object;
            object.type = objectTypeFromName(encoded.value(QStringLiteral("type")).toString());
            if (object.type == ObjectType::None)
                continue;
            object.outlineColor = decodeColor(encoded.value(QStringLiteral("outlineColor")),
                                              QColor(Qt::white));
            object.fillColor = decodeColor(encoded.value(QStringLiteral("fillColor")),
                                           QColor(Qt::transparent));
            // Version 2 used outlineColor as the sole visible color for text
            // and strokes. Version 3 gives those objects a true core/fill plus
            // a separate thin outline, so migrate without changing appearance.
            if (stateVersion < 3
                && object.type != ObjectType::Rectangle
                && object.type != ObjectType::Ellipse) {
                object.fillColor = object.outlineColor;
                object.outlineColor = Qt::transparent;
            }
            object.width = qBound(1, qRound(encoded.value(QStringLiteral("width"))
                                                .toDouble(0.0125) * imageWidth), 80);
            object.rotation = normalizedRotation(
                encoded.value(QStringLiteral("rotation")).toDouble());
            if (object.type == ObjectType::Text) {
                object.text = encoded.value(QStringLiteral("text")).toString();
                if (object.text.trimmed().isEmpty())
                    continue;
                object.font = decodeFont(encoded);
                object.position = normalizedPoint(encoded, QStringLiteral("x"),
                                                   QStringLiteral("y"));
            } else if (object.type == ObjectType::Stroke) {
                for (const QJsonValue &pointValue : encoded.value(QStringLiteral("points")).toArray()) {
                    const QJsonArray point = pointValue.toArray();
                    if (point.size() == 2) {
                        object.points.append(QPointF(
                            qBound(0.0, point.at(0).toDouble(), 1.0) * imageWidth,
                            qBound(0.0, point.at(1).toDouble(), 1.0) * imageHeight));
                    }
                }
                if (object.points.isEmpty())
                    continue;
            } else {
                object.start = normalizedPoint(encoded, QStringLiteral("x1"),
                                                QStringLiteral("y1"), 0.0);
                object.end = normalizedPoint(encoded, QStringLiteral("x2"),
                                              QStringLiteral("y2"), 0.0);
            }
            objects.append(object);
        }
    } else {
        // Version-1 compositions stored type-specific arrays. Import them into
        // the ordered version-2 object model without changing their appearance.
        for (const QJsonValue &value : state.value(QStringLiteral("strokes")).toArray()) {
            const QJsonObject encoded = value.toObject();
            OverlayObject object;
            object.type = ObjectType::Stroke;
            object.fillColor = decodeColor(encoded.value(QStringLiteral("color")),
                                           QColor(Qt::white));
            object.outlineColor = Qt::transparent;
            object.width = qBound(1, qRound(encoded.value(QStringLiteral("width"))
                                                .toDouble(0.0125) * imageWidth), 80);
            for (const QJsonValue &pointValue : encoded.value(QStringLiteral("points")).toArray()) {
                const QJsonArray point = pointValue.toArray();
                if (point.size() == 2) {
                    object.points.append(QPointF(
                        qBound(0.0, point.at(0).toDouble(), 1.0) * imageWidth,
                        qBound(0.0, point.at(1).toDouble(), 1.0) * imageHeight));
                }
            }
            if (!object.points.isEmpty())
                objects.append(object);
        }
        for (const QJsonValue &value : state.value(QStringLiteral("shapes")).toArray()) {
            const QJsonObject encoded = value.toObject();
            OverlayObject object;
            const ShapeType shapeType = static_cast<ShapeType>(
                qBound(0, encoded.value(QStringLiteral("type")).toInt(), 3));
            object.type = objectTypeFromShape(shapeType);
            object.outlineColor = decodeColor(encoded.value(QStringLiteral("color")),
                                              QColor(Qt::white));
            object.fillColor = Qt::transparent;
            if (object.type == ObjectType::Line || object.type == ObjectType::Arrow) {
                object.fillColor = object.outlineColor;
                object.outlineColor = Qt::transparent;
            }
            object.width = qBound(1, qRound(encoded.value(QStringLiteral("width"))
                                                .toDouble(0.0125) * imageWidth), 80);
            object.start = normalizedPoint(encoded, QStringLiteral("x1"),
                                           QStringLiteral("y1"), 0.0);
            object.end = normalizedPoint(encoded, QStringLiteral("x2"),
                                         QStringLiteral("y2"), 0.0);
            objects.append(object);
        }
        for (const QJsonValue &value : state.value(QStringLiteral("texts")).toArray()) {
            const QJsonObject encoded = value.toObject();
            OverlayObject object;
            object.type = ObjectType::Text;
            object.text = encoded.value(QStringLiteral("text")).toString();
            if (object.text.trimmed().isEmpty())
                continue;
            object.font = decodeFont(encoded);
            object.fillColor = decodeColor(encoded.value(QStringLiteral("color")),
                                           QColor(Qt::white));
            object.outlineColor = Qt::transparent;
            object.position = normalizedPoint(encoded, QStringLiteral("x"),
                                               QStringLiteral("y"));
            objects.append(object);
        }
    }

    saveUndo();
    m_objects = objects;
    clearSelection();
    emitChanged();
    return true;
}

void SstvComposerCanvas::undo() {
    if (m_undo.isEmpty())
        return;
    m_redo.append({m_objects});
    restore(m_undo.takeLast());
}

void SstvComposerCanvas::redo() {
    if (m_redo.isEmpty())
        return;
    m_undo.append({m_objects});
    restore(m_redo.takeLast());
}

void SstvComposerCanvas::resetComposition() {
    if (m_objects.isEmpty())
        return;
    saveUndo();
    m_objects.clear();
    clearSelection();
    emitChanged();
}

void SstvComposerCanvas::clearCompositionForNewImage() {
    m_objects.clear();
    m_undo.clear();
    m_redo.clear();
    clearSelection();
    m_draggingObject = false;
    m_panningBackground = false;
    m_dragUndoCaptured = false;
    emitChanged();
}

QRectF SstvComposerCanvas::imageRect() const {
    if (m_background.isNull())
        return QRectF();
    QSizeF size = m_background.size();
    size.scale(this->size(), Qt::KeepAspectRatio);
    return QRectF((width() - size.width()) * 0.5, (height() - size.height()) * 0.5,
                  size.width(), size.height());
}

QPointF SstvComposerCanvas::imagePoint(const QPointF &widgetPoint) const {
    const QRectF target = imageRect();
    if (target.isEmpty() || m_background.isNull())
        return QPointF();
    return QPointF(qBound(0.0, (widgetPoint.x() - target.left())
                                  * m_background.width() / target.width(),
                          static_cast<double>(m_background.width() - 1)),
                   qBound(0.0, (widgetPoint.y() - target.top())
                                  * m_background.height() / target.height(),
                          static_cast<double>(m_background.height() - 1)));
}

QString SstvComposerCanvas::resolvedText(const QString &text, bool showPlaceholders) const {
    QString result = text;
    result.replace(myCallToken(), m_myCall.isEmpty() && showPlaceholders
                                      ? QStringLiteral("[MY CALL]") : m_myCall,
                   Qt::CaseInsensitive);
    result.replace(toCallToken(), m_toCall.isEmpty() && showPlaceholders
                                      ? QStringLiteral("[TO CALL]") : m_toCall,
                   Qt::CaseInsensitive);
    return result;
}

QRectF SstvComposerCanvas::objectBounds(const OverlayObject &object) const {
    if (object.type == ObjectType::Text) {
        const QRectF bounds = textBounds(object.font, resolvedText(object.text));
        return QRectF(object.position.x() - bounds.width() * 0.5,
                      object.position.y() - bounds.height() * 0.5,
                      bounds.width(), bounds.height());
    }
    if (object.type == ObjectType::Stroke) {
        const QRectF bounds = QPolygonF(object.points).boundingRect();
        return bounds.adjusted(-object.width * 0.5, -object.width * 0.5,
                               object.width * 0.5, object.width * 0.5);
    }
    QRectF bounds(object.start, object.end);
    bounds = bounds.normalized();
    if (object.type == ObjectType::Line || object.type == ObjectType::Arrow)
        bounds = bounds.adjusted(-object.width * 0.5, -object.width * 0.5,
                                 object.width * 0.5, object.width * 0.5);
    return bounds;
}

QPointF SstvComposerCanvas::objectCenter(const OverlayObject &object) const {
    if (object.type == ObjectType::Text)
        return object.position;
    return objectBounds(object).center();
}

QTransform SstvComposerCanvas::objectTransform(const OverlayObject &object) const {
    const QPointF center = objectCenter(object);
    QTransform transform;
    transform.translate(center.x(), center.y());
    transform.rotate(object.rotation);
    transform.translate(-center.x(), -center.y());
    return transform;
}

QPainterPath SstvComposerCanvas::objectPath(const OverlayObject &object) const {
    QPainterPath path;
    if (object.type == ObjectType::Text) {
        path.addRect(objectBounds(object));
    } else if (object.type == ObjectType::Stroke) {
        if (!object.points.isEmpty()) {
            path.moveTo(object.points.first());
            for (int index = 1; index < object.points.size(); ++index)
                path.lineTo(object.points.at(index));
        }
    } else if (object.type == ObjectType::Rectangle) {
        path.addRect(QRectF(object.start, object.end).normalized());
    } else if (object.type == ObjectType::Ellipse) {
        path.addEllipse(QRectF(object.start, object.end).normalized());
    } else {
        path.moveTo(object.start);
        path.lineTo(object.end);
    }
    return path;
}

QVector<int> SstvComposerCanvas::objectsAt(const QPointF &point) const {
    QVector<int> result;
    for (int index = m_objects.size() - 1; index >= 0; --index) {
        if (objectContains(m_objects.at(index), point))
            result.append(index);
    }
    return result;
}

bool SstvComposerCanvas::objectContains(const OverlayObject &object,
                                        const QPointF &point) const {
    bool invertible = false;
    const QPointF localPoint = objectTransform(object).inverted(&invertible).map(point);
    if (!invertible)
        return false;
    const QRectF target = imageRect();
    const qreal imagePerWidgetPixel = target.isEmpty()
        ? 1.0 : m_background.width() / qMax<qreal>(1.0, target.width());
    const qreal tolerance = qMax<qreal>(4.0, 12.0 * imagePerWidgetPixel);
    if (object.type == ObjectType::Text)
        return objectBounds(object).adjusted(-tolerance, -tolerance,
                                             tolerance, tolerance).contains(localPoint);
    if (object.type == ObjectType::Stroke) {
        for (int index = 1; index < object.points.size(); ++index) {
            if (distanceToSegment(localPoint, object.points.at(index - 1),
                                  object.points.at(index))
                <= tolerance + object.width * 0.5) {
                return true;
            }
        }
        return object.points.size() == 1
            && QLineF(localPoint, object.points.first()).length() <= tolerance;
    }
    if (object.type == ObjectType::Line || object.type == ObjectType::Arrow) {
        return distanceToSegment(localPoint, object.start, object.end)
            <= tolerance + object.width * 0.5;
    }
    const QPainterPath path = objectPath(object);
    if (object.fillColor.alpha() > 0 && path.contains(localPoint))
        return true;
    QPainterPathStroker stroker;
    stroker.setWidth(qMax<qreal>(object.width + tolerance * 2.0, tolerance * 2.0));
    return stroker.createStroke(path).contains(localPoint);
}

void SstvComposerCanvas::drawObject(QPainter &painter,
                                    const OverlayObject &object) const {
    painter.save();
    painter.setWorldTransform(objectTransform(object), true);
    if (object.type == ObjectType::Text) {
        const QPainterPath glyphs = textPath(object.font, object.position,
                                             resolvedText(object.text));
        if (object.outlineColor.alpha() > 0) {
            // A normal QPen is centered on every raw glyph subpath. That puts
            // outline ink inside the character and along self-intersections
            // such as a Q tail crossing its bowl. Build a dilated silhouette,
            // remove the filled glyph, and paint only the exterior contour.
            QPainterPathStroker stroker;
            stroker.setWidth(3.0);
            stroker.setCapStyle(Qt::RoundCap);
            stroker.setJoinStyle(Qt::RoundJoin);
            QPainterPath exterior = stroker.createStroke(glyphs).subtracted(glyphs);
            exterior.setFillRule(Qt::WindingFill);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QBrush(object.outlineColor));
            painter.drawPath(exterior);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(object.fillColor));
        painter.drawPath(glyphs);
    } else if (object.type == ObjectType::Stroke) {
        if (object.points.size() >= 2) {
            if (object.outlineColor.alpha() > 0) {
                painter.setPen(QPen(object.outlineColor, object.width + 3.0,
                                    Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter.drawPolyline(object.points.constData(), object.points.size());
            }
            if (object.fillColor.alpha() > 0) {
                painter.setPen(QPen(object.fillColor, object.width, Qt::SolidLine,
                                    Qt::RoundCap, Qt::RoundJoin));
                painter.drawPolyline(object.points.constData(), object.points.size());
            }
        }
    } else {
        if (object.type == ObjectType::Rectangle || object.type == ObjectType::Ellipse)
            painter.setBrush(QBrush(object.fillColor));
        else
            painter.setBrush(Qt::NoBrush);

        const auto drawLineOrArrow = [&painter, &object]() {
            const QLineF shaft(object.start, object.end);
            painter.drawLine(shaft);
            if (object.type != ObjectType::Arrow || shaft.length() < 2.0)
                return;
            const qreal headLength = qMin<qreal>(18.0,
                qMax<qreal>(7.0, shaft.length() * 0.18));
            // QLineF::setLength() cannot extend a null line reliably. Build
            // each head segment at a real length and place its origin exactly
            // at the finger-release endpoint.
            QLineF left = QLineF::fromPolar(headLength, shaft.angle() + 150.0);
            left.translate(object.end);
            QLineF right = QLineF::fromPolar(headLength, shaft.angle() - 150.0);
            right.translate(object.end);
            painter.drawLine(left);
            painter.drawLine(right);
        };

        if (object.type == ObjectType::Line || object.type == ObjectType::Arrow) {
            if (object.outlineColor.alpha() > 0) {
                painter.setPen(QPen(object.outlineColor, object.width + 3.0,
                                    Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                drawLineOrArrow();
            }
            if (object.fillColor.alpha() > 0) {
                painter.setPen(QPen(object.fillColor, object.width, Qt::SolidLine,
                                    Qt::RoundCap, Qt::RoundJoin));
                drawLineOrArrow();
            }
        } else if (object.type == ObjectType::Rectangle) {
            painter.setPen(object.outlineColor.alpha() > 0
                               ? QPen(object.outlineColor, object.width, Qt::SolidLine,
                                      Qt::RoundCap, Qt::RoundJoin)
                               : Qt::NoPen);
            painter.drawRect(QRectF(object.start, object.end).normalized());
        } else if (object.type == ObjectType::Ellipse) {
            painter.setPen(object.outlineColor.alpha() > 0
                               ? QPen(object.outlineColor, object.width, Qt::SolidLine,
                                      Qt::RoundCap, Qt::RoundJoin)
                               : Qt::NoPen);
            painter.drawEllipse(QRectF(object.start, object.end).normalized());
        }
    }
    painter.restore();
}

void SstvComposerCanvas::moveObject(OverlayObject &object, const QPointF &delta) {
    if (object.type == ObjectType::Text) {
        object.position += delta;
    } else if (object.type == ObjectType::Stroke) {
        for (QPointF &point : object.points)
            point += delta;
    } else {
        object.start += delta;
        object.end += delta;
    }
}

QImage SstvComposerCanvas::renderComposition() const {
    if (m_background.isNull())
        return QImage();
    QImage frame = m_background.copy();
    QPainter painter(&frame);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    for (const OverlayObject &object : m_objects)
        drawObject(painter, object);
    return frame;
}

void SstvComposerCanvas::saveUndo() {
    m_undo.append({m_objects});
    if (m_undo.size() > MaxHistory)
        m_undo.removeFirst();
    m_redo.clear();
}

void SstvComposerCanvas::restore(const Snapshot &snapshot) {
    m_objects = snapshot.objects;
    clearSelection();
    emitChanged();
}

void SstvComposerCanvas::clearSelection() {
    m_selectedObject = -1;
}

void SstvComposerCanvas::emitChanged() {
    update();
    emit compositionChanged();
    emit selectionChanged(hasSelectedObject());
}

void SstvComposerCanvas::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(QStringLiteral("#1b2022")));
    if (m_background.isNull()) {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter,
                         QStringLiteral("Choose an image from Gallery or Camera"));
        return;
    }
    const QRectF target = imageRect();
    painter.drawImage(target, renderComposition());
    if (hasSelectedObject() && m_tool == Tool::Select) {
        const OverlayObject &object = m_objects.at(m_selectedObject);
        QPainterPath selection;
        selection.addRect(objectBounds(object).adjusted(-4, -4, 4, 4));
        selection = objectTransform(object).map(selection);
        QTransform imageToWidget;
        imageToWidget.translate(target.left(), target.top());
        imageToWidget.scale(target.width() / m_background.width(),
                            target.height() / m_background.height());
        painter.setPen(QPen(QColor(QStringLiteral("#f2ad20")), 2, Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(imageToWidget.map(selection));
    }
}

void SstvComposerCanvas::mousePressEvent(QMouseEvent *event) {
    if (m_background.isNull() || event->button() != Qt::LeftButton)
        return;
    const QPointF point = imagePoint(event->position());
    m_lastPoint = point;
    if (m_tool == Tool::Draw) {
        saveUndo();
        OverlayObject object;
        object.type = ObjectType::Stroke;
        object.outlineColor = m_outlineColor;
        object.fillColor = m_fillColor;
        object.width = m_inkWidth;
        object.points.append(point);
        m_objects.append(object);
        m_selectedObject = m_objects.size() - 1;
        return;
    }
    if (m_tool == Tool::Shape) {
        saveUndo();
        OverlayObject object;
        object.type = objectTypeFromShape(m_shapeType);
        object.outlineColor = m_outlineColor;
        object.fillColor = m_fillColor;
        object.width = m_inkWidth;
        object.start = point;
        object.end = point;
        m_objects.append(object);
        m_selectedObject = m_objects.size() - 1;
        return;
    }
    const QVector<int> hits = objectsAt(point);
    if (hits.contains(m_selectedObject)) {
        // Preserve the current selection so a second touch can drag it instead
        // of unexpectedly selecting an overlapping object underneath.
    } else {
        m_selectedObject = hits.isEmpty() ? -1 : hits.first();
    }
    m_lastSelectionPoint = point;
    m_draggingObject = hasSelectedObject();
    m_panningBackground = !m_draggingObject;
    m_dragUndoCaptured = false;
    emitChanged();
}

void SstvComposerCanvas::mouseMoveEvent(QMouseEvent *event) {
    if (m_background.isNull() || !(event->buttons() & Qt::LeftButton))
        return;
    const QPointF point = imagePoint(event->position());
    if (m_tool == Tool::Draw && !m_objects.isEmpty()
        && m_objects.last().type == ObjectType::Stroke) {
        m_objects.last().points.append(point);
        update();
    } else if (m_tool == Tool::Shape && !m_objects.isEmpty()
               && m_objects.last().type != ObjectType::Text
               && m_objects.last().type != ObjectType::Stroke) {
        m_objects.last().end = point;
        update();
    } else if (m_draggingObject && hasSelectedObject()) {
        if (!m_dragUndoCaptured) {
            saveUndo();
            m_dragUndoCaptured = true;
        }
        moveObject(m_objects[m_selectedObject], point - m_lastPoint);
        m_lastPoint = point;
        update();
    } else if (m_tool == Tool::Select && m_panningBackground) {
        const QPointF delta((point.x() - m_lastPoint.x()) / qMax(1, m_background.width()),
                            (point.y() - m_lastPoint.y()) / qMax(1, m_background.height()));
        m_lastPoint = point;
        if (!qFuzzyIsNull(delta.x()) || !qFuzzyIsNull(delta.y()))
            emit backgroundPanRequested(delta);
    }
}

void SstvComposerCanvas::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton)
        return;
    if (m_tool == Tool::Shape && !m_objects.isEmpty()
        && m_objects.last().type != ObjectType::Text
        && m_objects.last().type != ObjectType::Stroke) {
        // Touch streams do not always deliver a final move event at the same
        // coordinate as release. Finish the shape, including an arrow tip,
        // at the point where the operator actually lifts their finger.
        m_objects.last().end = imagePoint(event->position());
    }
    const bool changed = (m_tool == Tool::Draw && !m_objects.isEmpty())
        || (m_tool == Tool::Shape && !m_objects.isEmpty()) || m_draggingObject;
    m_draggingObject = false;
    m_panningBackground = false;
    m_dragUndoCaptured = false;
    if (changed)
        emitChanged();
}

void SstvComposerCanvas::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    update();
}
