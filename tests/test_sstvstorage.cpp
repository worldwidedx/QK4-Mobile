#include "sstv/sstvstorage.h"

#include <QJsonArray>
#include <QTemporaryDir>
#include <QTest>

class SstvStorageTest : public QObject {
    Q_OBJECT
private slots:
    void receivedImagesPersistWithMetadataAndRetention();
    void draftRoundTripsImageAndComposition();
    void userTemplatesCanBeSavedAndReset();
    void imageTemplatesRoundTripEditableState();
};

void SstvStorageTest::receivedImagesPersistWithMetadataAndRetention() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    SstvStorage storage(temporary.path());
    storage.setRetentionLimit(2);
    QImage image(32, 24, QImage::Format_RGB32);
    image.fill(Qt::cyan);

    SstvRxRecord first;
    QString error;
    QVERIFY2(storage.saveReceived(image, 0, QStringLiteral("Scottie S1"),
                                  QStringLiteral("SLANT: AUTO LOCKED"), 14230000,
                                  &first, &error), qPrintable(error));
    QVERIFY(QFileInfo::exists(first.imagePath));
    QVERIFY(QFileInfo::exists(first.metadataPath));
    QVERIFY2(storage.setStarred(first.id, true, &error), qPrintable(error));
    QVERIFY2(storage.setCallsign(first.id, QStringLiteral("w9wdx"), QStringLiteral("fsk id"),
                                 100, &error), qPrintable(error));
    QTest::qWait(2);
    QVERIFY2(storage.saveReceived(image, 1, QStringLiteral("Scottie S2"), QString(), 0,
                                  nullptr, &error), qPrintable(error));
    QTest::qWait(2);
    QVERIFY2(storage.saveReceived(image, 2, QStringLiteral("Martin M1"), QString(), 0,
                                  nullptr, &error), qPrintable(error));

    const QVector<SstvRxRecord> records = storage.received(&error);
    QCOMPARE(records.size(), 2);
    const auto restored = std::find_if(records.cbegin(), records.cend(),
                                       [&first](const SstvRxRecord &record) {
        return record.id == first.id;
    });
    QVERIFY(restored != records.cend());
    QVERIFY(restored->starred);
    QCOMPARE(restored->callsign, QStringLiteral("W9WDX"));
    QCOMPARE(restored->callsignSource, QStringLiteral("FSK ID"));
    QCOMPARE(restored->callsignConfidence, 100);
}

void SstvStorageTest::draftRoundTripsImageAndComposition() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    SstvStorage storage(temporary.path());
    QImage source(20, 10, QImage::Format_RGB32);
    source.fill(QColor(QStringLiteral("#123456")));
    const QJsonObject composition{{QStringLiteral("texts"), QJsonArray{QJsonObject{{QStringLiteral("text"), QStringLiteral("W9WDX")}}}}};
    const QJsonObject state{{QStringLiteral("modeId"), 3},
                            {QStringLiteral("fitBars"), true},
                            {QStringLiteral("composition"), composition}};
    QString error;
    QVERIFY2(storage.saveDraft(source, state, &error), qPrintable(error));

    QImage restored;
    QJsonObject restoredState;
    QVERIFY2(storage.loadDraft(&restored, &restoredState, &error), qPrintable(error));
    QCOMPARE(restored, source);
    QCOMPARE(restoredState.value(QStringLiteral("modeId")).toInt(), 3);
    QCOMPARE(restoredState.value(QStringLiteral("fitBars")).toBool(), true);
    QCOMPARE(restoredState.value(QStringLiteral("composition")).toObject(), composition);
    QVERIFY2(storage.clearDraft(&error), qPrintable(error));
    QVERIFY(!storage.loadDraft(nullptr, nullptr));
}

void SstvStorageTest::userTemplatesCanBeSavedAndReset() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    SstvStorage storage(temporary.path());
    const QJsonObject state{{QStringLiteral("composition"), QJsonObject{{QStringLiteral("version"), 1}}}};
    QString error;
    QVERIFY2(storage.saveUserTemplate(QStringLiteral("Field Day"), state, &error), qPrintable(error));
    QCOMPARE(storage.userTemplateNames(), QStringList{QStringLiteral("Field Day")});
    QJsonObject restored;
    QVERIFY2(storage.loadUserTemplate(QStringLiteral("Field Day"), &restored, &error), qPrintable(error));
    QCOMPARE(restored.value(QStringLiteral("composition")).toObject(), state.value(QStringLiteral("composition")).toObject());
    QImage templateImage(48, 32, QImage::Format_RGB32);
    templateImage.fill(QColor(QStringLiteral("#336699")));
    const QJsonObject updatedState{
        {QStringLiteral("composition"), QJsonObject{{QStringLiteral("version"), 2}}}};
    QVERIFY2(storage.saveUserTemplate(QStringLiteral("Field Day"), updatedState,
                                      templateImage, &error),
             qPrintable(error));
    QCOMPARE(storage.userTemplateNames(), QStringList{QStringLiteral("Field Day")});
    QImage restoredImage;
    QVERIFY2(storage.loadUserTemplate(QStringLiteral("Field Day"), &restored,
                                      &restoredImage, &error),
             qPrintable(error));
    QCOMPARE(restored.value(QStringLiteral("composition")).toObject(),
             updatedState.value(QStringLiteral("composition")).toObject());
    QCOMPARE(restoredImage, templateImage);
    QVERIFY(!restored.value(QStringLiteral("sourceFile")).toString().isEmpty());

    // Saving without the optional image deliberately converts the same
    // template back to a layout-only template and removes the prior PNG.
    QVERIFY2(storage.saveUserTemplate(QStringLiteral("Field Day"), state, &error),
             qPrintable(error));
    restoredImage = templateImage;
    QVERIFY2(storage.loadUserTemplate(QStringLiteral("Field Day"), &restored,
                                      &restoredImage, &error), qPrintable(error));
    QVERIFY(restoredImage.isNull());
    QVERIFY(restored.value(QStringLiteral("sourceFile")).toString().isEmpty());
    QVERIFY2(storage.resetUserTemplates(&error), qPrintable(error));
    QVERIFY(storage.userTemplateNames().isEmpty());
}

void SstvStorageTest::imageTemplatesRoundTripEditableState() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    SstvStorage storage(temporary.path());
    QImage source(40, 30, QImage::Format_RGB32);
    source.fill(QColor(QStringLiteral("#224466")));
    QImage preview(320, 256, QImage::Format_RGB32);
    preview.fill(QColor(QStringLiteral("#f2ad20")));
    const QJsonObject composition{{QStringLiteral("version"), 1},
                                  {QStringLiteral("texts"), QJsonArray{
                                      QJsonObject{{QStringLiteral("text"), QStringLiteral("CQ W9WDX")}}
                                  }}};
    const QJsonObject state{{QStringLiteral("modeId"), 3},
                            {QStringLiteral("fitBars"), false},
                            {QStringLiteral("frameZoom"), 1.8},
                            {QStringLiteral("frameCenterX"), 0.4},
                            {QStringLiteral("frameCenterY"), 0.6},
                            {QStringLiteral("composition"), composition}};
    QString error;
    QVERIFY2(storage.saveImageTemplate(QStringLiteral("CQ Field Day"), source, preview,
                                       state, &error), qPrintable(error));
    QCOMPARE(storage.imageTemplateNames(), QStringList{QStringLiteral("CQ Field Day")});

    QImage restoredSource;
    QImage restoredPreview;
    QJsonObject restoredState;
    QVERIFY2(storage.loadImageTemplate(QStringLiteral("CQ Field Day"), &restoredSource,
                                       &restoredPreview, &restoredState, &error), qPrintable(error));
    QCOMPARE(restoredSource, source);
    QCOMPARE(restoredPreview, preview);
    QCOMPARE(restoredState.value(QStringLiteral("composition")).toObject(), composition);
    QCOMPARE(restoredState.value(QStringLiteral("frameZoom")).toDouble(), 1.8);
    QVERIFY2(storage.removeImageTemplate(QStringLiteral("CQ Field Day"), &error), qPrintable(error));
    QVERIFY(storage.imageTemplateNames().isEmpty());
}

QTEST_MAIN(SstvStorageTest)
#include "test_sstvstorage.moc"
