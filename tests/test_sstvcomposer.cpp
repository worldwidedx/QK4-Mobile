#include "ui/sstvcomposercanvas.h"

#include <QtTest>

class SstvComposerTest : public QObject {
    Q_OBJECT
private slots:
    void rendersTextAtNativeFrameSize();
    void freehandUndoRestoresBackground();
    void compositionStateRestoresMultilineTextAndCanDeleteIt();
    void unspecifiedFontStretchRestoresAtNormalWidth();
    void simpleShapePersistsAndUndoRemovesIt();
    void newImageClearsMarkupAndUndoHistory();
    void selectedTextColorChangesImmediatelyAndIsUndoable();
    void outlineColorAppliesToFreehandAndLine();
    void selectedStrokeSizeChangesImmediatelyAndIsUndoable();
    void selectedTextAcceptsExpandedEditorMaximum();
    void selectedTextFontChangesImmediatelyAndIsUndoable();
    void rectangleFillRotationSelectionAndDeleteAreUndoable();
    void callsignVariablesRemainDynamicAcrossPersistence();
    void restoresVersionOneComposition();
};

void SstvComposerTest::rendersTextAtNativeFrameSize() {
    SstvComposerCanvas canvas;
    QImage background(320, 256, QImage::Format_RGB32);
    background.fill(Qt::black);
    canvas.setBackground(background);
    QFont font(QStringLiteral("Sans Serif"));
    font.setPixelSize(32);
    font.setBold(true);
    canvas.addTextBlock(QStringLiteral("W9WDX"), font, Qt::white);

    const QImage rendered = canvas.renderedImage();
    QCOMPARE(rendered.size(), background.size());
    QVERIFY(rendered != background);
}

void SstvComposerTest::freehandUndoRestoresBackground() {
    SstvComposerCanvas canvas;
    canvas.resize(320, 256);
    canvas.show();
    QImage background(320, 256, QImage::Format_RGB32);
    background.fill(Qt::black);
    canvas.setBackground(background);
    canvas.setTool(SstvComposerCanvas::Tool::Draw);
    canvas.setInk(Qt::red, 8);

    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(50, 50));
    QTest::mouseMove(&canvas, QPoint(250, 180), 20);
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(250, 180));
    QVERIFY(canvas.renderedImage() != background);
    QVERIFY(canvas.canUndo());
    canvas.undo();
    QCOMPARE(canvas.renderedImage(), background);
}

void SstvComposerTest::compositionStateRestoresMultilineTextAndCanDeleteIt() {
    QImage background(320, 256, QImage::Format_RGB32);
    background.fill(Qt::black);
    QFont font(QStringLiteral("Sans Serif"));
    font.setPixelSize(34);
    font.setWeight(QFont::Black);
    font.setStretch(QFont::Condensed);
    font.setItalic(true);

    SstvComposerCanvas original;
    original.setBackground(background);
    original.addTextBlock(QStringLiteral("CQ CQ CQ\nDE W9WDX"), font, Qt::white);
    const QJsonObject state = original.compositionState();

    SstvComposerCanvas restored;
    restored.setBackground(background);
    QVERIFY(restored.restoreCompositionState(state));
    QCOMPARE(restored.renderedImage(), original.renderedImage());
    restored.setTool(SstvComposerCanvas::Tool::Select);
    restored.resize(640, 512);
    restored.show();
    QTest::mouseClick(&restored, Qt::LeftButton, Qt::NoModifier, QPoint(320, 256));
    QVERIFY(restored.hasSelectedText());
    restored.deleteSelectedText();
    QCOMPARE(restored.renderedImage(), background);
    restored.undo();
    QCOMPARE(restored.renderedImage(), original.renderedImage());
}

void SstvComposerTest::unspecifiedFontStretchRestoresAtNormalWidth() {
    QImage background(320, 256, QImage::Format_RGB32);
    background.fill(Qt::black);
    QFont font(QStringLiteral("Sans Serif"));
    font.setPixelSize(32);
    font.setBold(true);

    SstvComposerCanvas original;
    original.setBackground(background);
    original.addTextBlock(QStringLiteral("CQ CQ CQ\nDE AE6LX"), font, Qt::white,
                          QPointF(0.27, 0.26));
    QJsonObject state = original.compositionState();
    QJsonArray objects = state.value(QStringLiteral("objects")).toArray();
    QVERIFY(!objects.isEmpty());
    QJsonObject text = objects.first().toObject();
    text.insert(QStringLiteral("stretch"), 0);
    objects[0] = text;
    state.insert(QStringLiteral("objects"), objects);

    SstvComposerCanvas restored;
    restored.setBackground(background);
    QVERIFY(restored.restoreCompositionState(state));
    QVERIFY(restored.renderedImage() != background);
    const QJsonArray restoredObjects =
        restored.compositionState().value(QStringLiteral("objects")).toArray();
    QCOMPARE(restoredObjects.first().toObject().value(QStringLiteral("stretch")).toInt(),
             static_cast<int>(QFont::Unstretched));
}

void SstvComposerTest::simpleShapePersistsAndUndoRemovesIt() {
    QImage background(320, 256, QImage::Format_RGB32);
    background.fill(Qt::black);
    SstvComposerCanvas canvas;
    canvas.resize(640, 512);
    canvas.setBackground(background);
    canvas.setInk(Qt::yellow, 5);
    canvas.setShapeType(SstvComposerCanvas::ShapeType::Arrow);
    canvas.setTool(SstvComposerCanvas::Tool::Shape);
    canvas.show();
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(80, 256));
    QTest::mouseMove(&canvas, QPoint(500, 256));
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(560, 256));
    const QImage rendered = canvas.renderedImage();
    QVERIFY(rendered != background);
    const auto hasInk = [&rendered](const QRect &region) {
        const QRect bounded = region.intersected(rendered.rect());
        for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
            for (int x = bounded.left(); x <= bounded.right(); ++x) {
                if (rendered.pixelColor(x, y) != QColor(Qt::black))
                    return true;
            }
        }
        return false;
    };
    // The shaft occupies y=128. Ink above and below it near the release point
    // proves that both arrowhead wings were rendered.
    QVERIFY(hasInk(QRect(258, 114, 20, 11)));
    QVERIFY(hasInk(QRect(258, 132, 20, 11)));
    const QJsonArray objects =
        canvas.compositionState().value(QStringLiteral("objects")).toArray();
    QCOMPARE(objects.size(), 1);
    QCOMPARE(objects.first().toObject().value(QStringLiteral("type")).toString(),
             QStringLiteral("arrow"));
    canvas.undo();
    QCOMPARE(canvas.renderedImage(), background);
}

void SstvComposerTest::outlineColorAppliesToFreehandAndLine() {
    QImage background(320, 256, QImage::Format_RGB32);
    background.fill(Qt::black);

    SstvComposerCanvas text;
    text.setBackground(background);
    text.setInk(Qt::transparent, 4);
    QFont textFont(QStringLiteral("Sans Serif"));
    textFont.setPixelSize(48);
    textFont.setBold(true);
    text.addTextBlock(QStringLiteral("W9WDX"), textFont, Qt::white);
    const QImage fillOnlyText = text.renderedImage();
    text.updateSelectedOutlineColor(Qt::green);
    const QImage textImage = text.renderedImage();
    bool hasTextFill = false;
    bool hasTextOutline = false;
    for (int y = 0; y < textImage.height(); ++y) {
        for (int x = 0; x < textImage.width(); ++x) {
            hasTextFill |= textImage.pixelColor(x, y) == QColor(Qt::white);
            hasTextOutline |= textImage.pixelColor(x, y) == QColor(Qt::green);
        }
    }
    QVERIFY(hasTextFill);
    QVERIFY(hasTextOutline);
    // The outline is exterior-only: it must never overwrite solid glyph fill,
    // including where a character's component paths cross each other.
    for (int y = 0; y < textImage.height(); ++y) {
        for (int x = 0; x < textImage.width(); ++x) {
            if (fillOnlyText.pixelColor(x, y) == QColor(Qt::white))
                QCOMPARE(textImage.pixelColor(x, y), QColor(Qt::white));
        }
    }

    SstvComposerCanvas freehand;
    freehand.resize(640, 512);
    freehand.setBackground(background);
    freehand.setInk(Qt::white, 6);
    freehand.setTool(SstvComposerCanvas::Tool::Draw);
    freehand.show();
    QTest::mousePress(&freehand, Qt::LeftButton, Qt::NoModifier, QPoint(80, 80));
    QTest::mouseMove(&freehand, QPoint(300, 220));
    QTest::mouseRelease(&freehand, Qt::LeftButton, Qt::NoModifier, QPoint(300, 220));
    freehand.updateSelectedOutlineColor(Qt::magenta);
    QCOMPARE(freehand.selectedOutlineColor(), QColor(Qt::magenta));
    const QImage freehandImage = freehand.renderedImage();
    bool hasWhiteCore = false;
    bool hasMagentaOutline = false;
    for (int y = 0; y < freehandImage.height(); ++y) {
        for (int x = 0; x < freehandImage.width(); ++x) {
            hasWhiteCore |= freehandImage.pixelColor(x, y) == QColor(Qt::white);
            hasMagentaOutline |= freehandImage.pixelColor(x, y) == QColor(Qt::magenta);
        }
    }
    QVERIFY(hasWhiteCore);
    QVERIFY(hasMagentaOutline);

    SstvComposerCanvas line;
    line.resize(640, 512);
    line.setBackground(background);
    line.setInk(Qt::white, 6);
    line.setShapeType(SstvComposerCanvas::ShapeType::Line);
    line.setTool(SstvComposerCanvas::Tool::Shape);
    line.show();
    QTest::mousePress(&line, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
    QTest::mouseRelease(&line, Qt::LeftButton, Qt::NoModifier, QPoint(500, 350));
    line.updateSelectedOutlineColor(Qt::cyan);
    QCOMPARE(line.selectedOutlineColor(), QColor(Qt::cyan));
    const QImage lineImage = line.renderedImage();
    bool hasCyanOutline = false;
    hasWhiteCore = false;
    for (int y = 0; y < lineImage.height(); ++y) {
        for (int x = 0; x < lineImage.width(); ++x) {
            hasWhiteCore |= lineImage.pixelColor(x, y) == QColor(Qt::white);
            hasCyanOutline |= lineImage.pixelColor(x, y) == QColor(Qt::cyan);
        }
    }
    QVERIFY(hasWhiteCore);
    QVERIFY(hasCyanOutline);
}

void SstvComposerTest::selectedStrokeSizeChangesImmediatelyAndIsUndoable() {
    QImage background(320, 256, QImage::Format_RGB32);
    background.fill(Qt::black);
    SstvComposerCanvas canvas;
    canvas.resize(640, 512);
    canvas.setBackground(background);
    canvas.setInk(Qt::red, 3);
    canvas.setFillColor(Qt::white);
    canvas.setShapeType(SstvComposerCanvas::ShapeType::Line);
    canvas.setTool(SstvComposerCanvas::Tool::Shape);
    canvas.show();
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(500, 350));
    QCOMPARE(canvas.selectedSize(), 15);
    const QImage narrow = canvas.renderedImage();
    const double narrowWidth = canvas.compositionState()
                                   .value(QStringLiteral("objects")).toArray()
                                   .first().toObject()
                                   .value(QStringLiteral("width")).toDouble();

    canvas.updateSelectedSize(50);
    QCOMPARE(canvas.selectedSize(), 50);
    QVERIFY(canvas.renderedImage() != narrow);

    canvas.undo();
    QCOMPARE(canvas.selectedSize(), -1); // Undo restoration deliberately clears selection.
    QCOMPARE(canvas.compositionState().value(QStringLiteral("objects")).toArray()
                 .first().toObject().value(QStringLiteral("width")).toDouble(),
             narrowWidth);
}

void SstvComposerTest::selectedTextAcceptsExpandedEditorMaximum() {
    QImage background(320, 256, QImage::Format_RGB32);
    background.fill(Qt::black);
    QFont font(QStringLiteral("Sans Serif"));
    font.setPixelSize(28);

    SstvComposerCanvas canvas;
    canvas.setBackground(background);
    canvas.addTextBlock(QStringLiteral("W9WDX"), font, Qt::white);
    const QImage original = canvas.renderedImage();

    canvas.updateSelectedSize(112);
    QCOMPARE(canvas.selectedSize(), 112);
    QCOMPARE(canvas.selectedTextFont().pixelSize(), 112);
    QVERIFY(canvas.renderedImage() != original);

    canvas.undo();
    QCOMPARE(canvas.selectedSize(), -1); // Undo restoration deliberately clears selection.
    QCOMPARE(canvas.renderedImage(), original);
}

void SstvComposerTest::newImageClearsMarkupAndUndoHistory() {
    QImage first(320, 256, QImage::Format_RGB32);
    first.fill(Qt::black);
    QImage second(320, 256, QImage::Format_RGB32);
    second.fill(Qt::blue);
    QFont font(QStringLiteral("Sans Serif"));
    font.setPixelSize(32);

    SstvComposerCanvas canvas;
    canvas.setBackground(first);
    canvas.addTextBlock(QStringLiteral("OLD MARKUP"), font, Qt::white);
    QVERIFY(canvas.renderedImage() != first);
    QVERIFY(canvas.canUndo());

    canvas.clearCompositionForNewImage();
    canvas.setBackground(second);
    QCOMPARE(canvas.renderedImage(), second);
    QVERIFY(!canvas.canUndo());
    QVERIFY(!canvas.canRedo());
    QCOMPARE(canvas.compositionState().value(QStringLiteral("objects")).toArray().size(), 0);
}

void SstvComposerTest::selectedTextColorChangesImmediatelyAndIsUndoable() {
    QImage background(320, 256, QImage::Format_RGB32);
    background.fill(Qt::black);
    QFont font(QStringLiteral("Sans Serif"));
    font.setPixelSize(32);
    font.setBold(true);

    SstvComposerCanvas canvas;
    canvas.setBackground(background);
    canvas.addTextBlock(QStringLiteral("W9WDX"), font, Qt::white);
    const QImage whiteText = canvas.renderedImage();
    QCOMPARE(canvas.selectedTextColor(), QColor(Qt::white));

    canvas.updateSelectedTextColor(Qt::red);
    QCOMPARE(canvas.selectedTextColor(), QColor(Qt::red));
    QVERIFY(canvas.renderedImage() != whiteText);

    canvas.undo();
    QCOMPARE(canvas.renderedImage(), whiteText);
}

void SstvComposerTest::selectedTextFontChangesImmediatelyAndIsUndoable() {
    QImage background(320, 256, QImage::Format_RGB32);
    background.fill(Qt::black);
    QFont originalFont(QStringLiteral("Sans Serif"));
    originalFont.setPixelSize(24);

    SstvComposerCanvas canvas;
    canvas.setBackground(background);
    canvas.addTextBlock(QStringLiteral("W9WDX"), originalFont, Qt::white);
    const QImage original = canvas.renderedImage();

    QFont changedFont(QStringLiteral("Serif"));
    changedFont.setPixelSize(48);
    changedFont.setBold(true);
    canvas.updateSelectedTextFont(changedFont);
    QCOMPARE(canvas.selectedTextFont(), changedFont);
    QVERIFY(canvas.renderedImage() != original);

    canvas.undo();
    QCOMPARE(canvas.renderedImage(), original);
}

void SstvComposerTest::rectangleFillRotationSelectionAndDeleteAreUndoable() {
    QImage background(320, 256, QImage::Format_RGB32);
    background.fill(Qt::black);
    SstvComposerCanvas canvas;
    canvas.resize(640, 512);
    canvas.setBackground(background);
    canvas.setInk(Qt::yellow, 5);
    canvas.setFillColor(Qt::blue);
    canvas.setShapeType(SstvComposerCanvas::ShapeType::Rectangle);
    canvas.setTool(SstvComposerCanvas::Tool::Shape);
    canvas.show();
    QTest::mousePress(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(120, 120));
    QTest::mouseMove(&canvas, QPoint(500, 390));
    QTest::mouseRelease(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(500, 390));
    QVERIFY(canvas.hasSelectedObject());
    QVERIFY(canvas.selectedObjectSupportsFill());
    QCOMPARE(canvas.selectedFillColor(), QColor(Qt::blue));

    canvas.rotateSelectedObject(45);
    QJsonObject object = canvas.compositionState()
                             .value(QStringLiteral("objects")).toArray()
                             .first().toObject();
    QCOMPARE(object.value(QStringLiteral("rotation")).toInt(), 45);
    canvas.updateSelectedOutlineColor(Qt::red);
    canvas.updateSelectedFillColor(Qt::green);
    QCOMPARE(canvas.selectedOutlineColor(), QColor(Qt::red));
    QCOMPARE(canvas.selectedFillColor(), QColor(Qt::green));

    canvas.setTool(SstvComposerCanvas::Tool::Select);
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
    QVERIFY(!canvas.hasSelectedObject());
    QTest::mouseClick(&canvas, Qt::LeftButton, Qt::NoModifier, QPoint(310, 255));
    QVERIFY(canvas.hasSelectedObject());
    canvas.deleteSelectedObject();
    QCOMPARE(canvas.renderedImage(), background);
    canvas.undo();
    QVERIFY(canvas.renderedImage() != background);
}

void SstvComposerTest::callsignVariablesRemainDynamicAcrossPersistence() {
    QImage background(320, 256, QImage::Format_RGB32);
    background.fill(Qt::black);
    QFont font(QStringLiteral("Sans Serif"));
    font.setPixelSize(32);
    font.setBold(true);

    SstvComposerCanvas canvas;
    canvas.setBackground(background);
    canvas.addTextBlock(QStringLiteral("CQ {TO_CALL}\nDE {MY_CALL}"), font, Qt::white);
    QString unresolved;
    QVERIFY(canvas.hasUnresolvedVariables(&unresolved));
    QCOMPARE(unresolved, SstvComposerCanvas::myCallToken());
    canvas.setCallsignValues(QStringLiteral("W9WDX"), QStringLiteral("XE2MAM"));
    QVERIFY(!canvas.hasUnresolvedVariables());
    const QImage firstRender = canvas.renderedImage();
    const QJsonObject state = canvas.compositionState();
    QCOMPARE(state.value(QStringLiteral("objects")).toArray().first().toObject()
                 .value(QStringLiteral("text")).toString(),
             QStringLiteral("CQ {TO_CALL}\nDE {MY_CALL}"));

    SstvComposerCanvas restored;
    restored.setBackground(background);
    restored.setCallsignValues(QStringLiteral("AE6LX"), QStringLiteral("K4ABC"));
    QVERIFY(restored.restoreCompositionState(state));
    QVERIFY(!restored.hasUnresolvedVariables());
    QVERIFY(restored.renderedImage() != firstRender);
}

void SstvComposerTest::restoresVersionOneComposition() {
    QImage background(320, 256, QImage::Format_RGB32);
    background.fill(Qt::black);
    const QJsonObject state{
        {QStringLiteral("version"), 1},
        {QStringLiteral("texts"), QJsonArray{QJsonObject{
            {QStringLiteral("text"), QStringLiteral("W9WDX")},
            {QStringLiteral("font"), QStringLiteral("Sans Serif")},
            {QStringLiteral("pixelSize"), 28},
            {QStringLiteral("color"), QStringLiteral("#ffffffff")},
            {QStringLiteral("x"), 0.5}, {QStringLiteral("y"), 0.5}}}}
    };
    SstvComposerCanvas canvas;
    canvas.setBackground(background);
    QVERIFY(canvas.restoreCompositionState(state));
    QVERIFY(canvas.renderedImage() != background);
    QCOMPARE(canvas.compositionState().value(QStringLiteral("version")).toInt(), 3);
}

QTEST_MAIN(SstvComposerTest)
#include "test_sstvcomposer.moc"
