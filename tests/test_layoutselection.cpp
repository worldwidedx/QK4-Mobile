#include "ui/k4styles.h"
#include "ui/layoutsettingspage.h"
#include "settings/radiosettings.h"
#include <QCheckBox>
#include <QLabel>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

class TestLayoutSelection : public QObject {
    Q_OBJECT
private:
    QTemporaryDir settingsDirectory;
private slots:
    void initTestCase() {
        QVERIFY(settingsDirectory.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsDirectory.path());
    }
    void init() {
        qunsetenv("QK4_FORCE_REGULAR_UI");
        qunsetenv("QK4_FORCE_COMPACT_UI");
        RadioSettings::instance()->setAlwaysCompactLayout(false);
    }
    void androidSelection_data() {
        QTest::addColumn<QSize>("size");
        QTest::addColumn<double>("diagonal");
        QTest::addColumn<bool>("forceCompact");
        QTest::addColumn<bool>("expectedCompact");
        // Representative logical sizes, not measurements of the reported Fold.
        QTest::newRow("fold-cover") << QSize(900, 380) << 6.5 << true << true;
        QTest::newRow("fold-inner") << QSize(960, 865) << 8.0 << true << true;
        QTest::newRow("fold-inner-portrait") << QSize(865, 960) << 8.0 << true << true;
        QTest::newRow("fold-unknown-diagonal") << QSize(960, 865) << 0.0 << true << true;
        QTest::newRow("fold-reported-as-large-tablet") << QSize(1340, 840) << 12.0 << true << true;
        QTest::newRow("ordinary-phone") << QSize(900, 380) << 6.9 << false << true;
        QTest::newRow("existing-tablet") << QSize(1340, 840) << 10.0 << false << false;
        QTest::newRow("tablet-manual-override") << QSize(1340, 840) << 10.0 << true << true;
        QTest::newRow("tablet-unknown-diagonal") << QSize(1340, 840) << 0.0 << false << false;
        QTest::newRow("phone-unknown-diagonal") << QSize(900, 380) << 0.0 << false << true;
    }
    void androidSelection() {
        QFETCH(QSize, size);
        QFETCH(double, diagonal);
        QFETCH(bool, forceCompact);
        QFETCH(bool, expectedCompact);
        K4Styles::configureForScreen(size, 2.5, diagonal, forceCompact);
        QCOMPARE(K4Styles::isCompactLayout(), expectedCompact);
        QCOMPARE(K4Styles::Dimensions::FontSizeFrequency, expectedCompact ? 20 : 32);
    }
    void forcedCompactWinsOverDevelopmentOverride() {
        qputenv("QK4_FORCE_REGULAR_UI", "1");
        K4Styles::configureForScreen(QSize(1340, 840), 1, 10, true);
        QVERIFY(K4Styles::isCompactLayout());
        K4Styles::configureForScreen(QSize(1340, 840), 1, 10, false);
        QVERIFY(!K4Styles::isCompactLayout());
    }
    void preferencePersistsWithoutChangingLiveLayout() {
        K4Styles::configureForScreen(QSize(1340, 840), 1, 10, false);
        std::unique_ptr<QWidget> page(createLayoutSettingsPage());
        auto *checkbox = page->findChild<QCheckBox *>("alwaysCompactLayout");
        auto *notice = page->findChild<QLabel *>("layoutRestartNotice");
        QVERIFY(checkbox && notice);
        QVERIFY(!checkbox->isChecked());
        checkbox->click();
        QVERIFY(checkbox->isChecked());
        QVERIFY(notice->text().contains("Saved"));
        QSettings diskSettings(QSettings::IniFormat, QSettings::UserScope, "QK4", "QK4");
        QVERIFY(diskSettings.fileName().startsWith(settingsDirectory.path()));
        QVERIFY(RadioSettings::instance()->alwaysCompactLayout());
        QCOMPARE(diskSettings.value("ui/alwaysCompactLayout").toBool(), true);
        QVERIFY(notice->text().contains("Restart"));
        QVERIFY(!K4Styles::isCompactLayout());
        page.reset(createLayoutSettingsPage());
        QVERIFY(page->findChild<QCheckBox *>("alwaysCompactLayout")->isChecked());
        // Next startup reads the persisted preference before constructing UI.
        K4Styles::configureForScreen(QSize(1340, 840), 1, 10,
                                    RadioSettings::instance()->alwaysCompactLayout());
        QVERIFY(K4Styles::isCompactLayout());
        page->findChild<QCheckBox *>("alwaysCompactLayout")->click();
        diskSettings.sync();
        QCOMPARE(diskSettings.value("ui/alwaysCompactLayout").toBool(), false);
        QVERIFY(K4Styles::isCompactLayout());
    }
    void settingsPageFitsCompactViewport() {
        K4Styles::configureForScreen(QSize(900, 380), 1, 6.5, true);
        std::unique_ptr<QWidget> page(createLayoutSettingsPage());
        page->resize(400, 220);
        page->show();
        QTest::qWait(10);
        QCOMPARE(page->size(), QSize(400, 220));
        const auto controls = page->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly);
        for (QWidget *control : controls)
            QVERIFY2(page->rect().contains(control->geometry()), qPrintable(control->objectName()));
        if (qEnvironmentVariableIsSet("QK4_LAYOUT_PREVIEW"))
            QVERIFY(page->grab().save(qEnvironmentVariable("QK4_LAYOUT_PREVIEW")));
    }
};

QTEST_MAIN(TestLayoutSelection)
#include "test_layoutselection.moc"
