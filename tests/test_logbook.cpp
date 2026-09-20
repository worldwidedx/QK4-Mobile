#include <QtTest>
#include <QTemporaryDir>
#include <QPushButton>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QTimer>
#include <QLabel>
#include <QSettings>
#include "ft8/qrzlogbook.h"
#include "ui/logbookdialog.h"
#include "ui/inwindowdialog.h"

class LogbookTest : public QObject {
    Q_OBJECT
    QTemporaryDir settings;
private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("QK4-tests");
        QCoreApplication::setApplicationName("Logbook-UI-isolated");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    }
    void backReturnsToInvoker_data() {
        QTest::addColumn<int>("key");
        QTest::addColumn<bool>("editorFocus");
        QTest::newRow("android-back-list") << int(Qt::Key_Back) << false;
        QTest::newRow("android-back-editor") << int(Qt::Key_Back) << true;
        QTest::newRow("escape-editor") << int(Qt::Key_Escape) << true;
    }
    void backReturnsToInvoker() {
        QFETCH(int, key);
        QFETCH(bool, editorFocus);
        QWidget host; host.resize(360, 696); host.show();
        QPushButton invoker("Logbook", &host); invoker.show(); invoker.setFocus();
        Ft8Logbook log;
        bool completed = false;
        QTimer::singleShot(50, &host, [&] {
            auto *sheet = host.findChild<InWindowDialog *>("logbookDialog");
            QVERIFY(sheet);
            QWidget *target = editorFocus ? static_cast<QWidget *>(host.findChild<QLineEdit *>())
                                          : host.findChild<QListWidget *>("logContacts");
            QVERIFY(target);
            target->setFocus();
            QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
            press.ignore(); // A focused child may have already ignored Back.
            QApplication::sendEvent(target, &press);
            QVERIFY(press.isAccepted());
            QVERIFY(sheet->isVisible()); // Consume the full press/release pair.
            QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
            release.ignore();
            QApplication::sendEvent(target, &release);
            QVERIFY(release.isAccepted());
            QVERIFY(!sheet->isVisible());
            completed = true;
        });
        QTimer::singleShot(1500, &host, [&] {
            for (auto *sheet : host.findChildren<InWindowDialog *>()) sheet->reject();
        });
        LogbookUi::show(&host, log);
        QVERIFY(completed);
        QVERIFY(host.isVisible());
        QCOMPARE(QApplication::focusWidget(), &invoker);
    }
    void backDismissesOnlyNestedSetup() {
        QTemporaryDir dir;
        QrzKeyStore keys{[](QString *) { return QString(); }, [](const QString &, QString *) { return false; },
            [](QString *) { return true; }};
        QrzLogbook qrz(dir.filePath("log.json"), nullptr, nullptr, keys);
        Ft8Logbook log(dir.filePath("log.json"));
        QWidget host; host.resize(360, 696); host.show();
        bool completed = false;
        QTimer::singleShot(50, &host, [&] {
            QTimer::singleShot(50, &host, [&] {
                auto *setup = host.findChild<InWindowDialog *>("qrzSetupDialog");
                auto *field = host.findChild<QLineEdit *>("qrzApiKey");
                QVERIFY(setup); QVERIFY(field);
                field->setFocus();
                QTest::keyClick(field, Qt::Key_Back);
                QVERIFY(!setup->isVisible());
                QVERIFY(host.findChild<InWindowDialog *>("logbookDialog")->isVisible());
                completed = true;
            });
            host.findChild<QPushButton *>("logSetup")->click();
            QTest::keyClick(host.findChild<QListWidget *>("logContacts"), Qt::Key_Back);
        });
        QTimer::singleShot(1500, &host, [&] {
            for (auto *sheet : host.findChildren<InWindowDialog *>()) sheet->reject();
        });
        LogbookUi::show(&host, log, {}, false, &qrz);
        QVERIFY(completed);
        QVERIFY(host.isVisible());
    }
    void tapSelectsAndSeparateSendIgnoresOldScrollState() {
        QTemporaryDir dir;
        Ft8Logbook log(dir.filePath("log.json"));
        QVERIFY(log.append(LogbookUi::contact("K3NT", "FT4", 14080000, "AE6LX")));
        auto sent = LogbookUi::contact("W1AW", "SSTV", 14230000, "AE6LX");
        sent["QRZCOM_QSO_UPLOAD_STATUS"] = "Y";
        QVERIFY(log.append(sent));
        QrzKeyStore keys{[](QString *) { return QString(); }, [](const QString &, QString *) { return false; }, [](QString *) { return true; }};
        QrzLogbook qrz(dir.filePath("log.json"), nullptr, nullptr, keys);
        QWidget host; host.resize(360, 696); host.show();
        bool selected = false, opened = false;
        QTimer::singleShot(60, &host, [&] {
            auto *list = host.findChild<QListWidget *>("logContacts");
            auto *send = host.findChild<QPushButton *>("logQrzSend");
            const auto unsent = list->visualItemRect(list->item(1));
            const auto uploaded = list->visualItemRect(list->item(0));
            const auto tap = unsent.topLeft() + QPoint(18, unsent.height() / 2); // Checkbox area selects the whole row.
            QTest::mousePress(list->viewport(), Qt::LeftButton, {}, tap);
            QVERIFY(!list->currentItem());
            QTest::mouseRelease(list->viewport(), Qt::LeftButton, {}, tap);
            selected = list->currentItem() == list->item(1) && send->isEnabled();
            QCOMPARE(list->item(1)->checkState(), Qt::Unchecked);
            QVERIFY(host.findChild<QLabel *>("logQrzStatus")->text().contains("Selected K3NT"));
            QVERIFY(host.grab().save("log-selected-portrait.png"));
            QTest::mousePress(list->viewport(), Qt::LeftButton, {}, uploaded.center());
            QTest::mouseMove(list->viewport(), unsent.center());
            QTest::mouseRelease(list->viewport(), Qt::LeftButton, {}, unsent.center());
            QCOMPARE(list->currentItem(), list->item(1)); // Drag does not select another contact.
            list->setProperty("logDragging", true); // Regression: this used to silently discard the action.
            QTimer::singleShot(40, &host, [&] {
                const auto dialogs = host.findChildren<InWindowDialog *>();
                for (auto *dialog : dialogs) {
                    if (dialog->objectName() == "inWindowDialogOverlay" && dialog->isVisible()) {
                        opened = true; dialog->reject();
                    }
                }
            });
            send->click(); // Unconfigured mock service produces feedback; no HTTP request can be sent.
            QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, uploaded.center());
            QCOMPARE(list->currentItem(), list->item(0));
            QVERIFY(!send->isEnabled());
            host.resize(696, 360);
            QTest::qWait(30);
            QVERIFY(host.grab().save("log-selected-landscape.png"));
            host.findChild<QPushButton *>("logBack")->click();
        });
        LogbookUi::show(&host, log, {}, false, &qrz);
        QVERIFY(selected); QVERIFY(opened);
    }
    void showHideTypedAndSavedApiKey() {
        QTemporaryDir dir;
        QString secret = "TEST-KEY-FOR-UI-ONLY";
        QrzKeyStore keys{[&](QString *) { return secret; }, [&](const QString &value, QString *) { secret = value; return true; },
            [&](QString *) { secret.clear(); return true; }};
        QrzLogbook qrz(dir.filePath("log.json"), nullptr, nullptr, keys);
        QString error;
        QVERIFY(qrz.configure("AE6LX", secret, false, &error));
        Ft8Logbook log(dir.filePath("log.json"));
        QWidget host; host.resize(360, 696); host.show();
        bool verified = false;
        QTimer::singleShot(60, &host, [&] {
            QTimer::singleShot(50, &host, [&] {
                auto *key = host.findChild<QLineEdit *>("qrzApiKey");
                auto *toggle = host.findChild<QPushButton *>("qrzShowKey");
                QCOMPARE(key->echoMode(), QLineEdit::Password);
                QVERIFY(key->text().isEmpty());
                key->setText("TYPED-TEST-KEY");
                toggle->click(); QCOMPARE(key->echoMode(), QLineEdit::Normal); QCOMPARE(toggle->text(), "Hide");
                QCOMPARE(key->text(), "TYPED-TEST-KEY");
                toggle->click(); QCOMPARE(key->echoMode(), QLineEdit::Password); QCOMPARE(toggle->text(), "Show");
                key->clear(); toggle->click(); QCOMPARE(key->text(), secret); QCOMPARE(key->echoMode(), QLineEdit::Normal);
                toggle->click(); QCOMPARE(key->echoMode(), QLineEdit::Password);
                verified = true;
                host.findChild<QPushButton *>("qrzCancel")->click();
            });
            host.findChild<QPushButton *>("logSetup")->click();
            host.findChild<QPushButton *>("logBack")->click();
        });
        LogbookUi::show(&host, log, {}, false, &qrz);
        QVERIFY(verified);
        QCOMPARE(secret, "TEST-KEY-FOR-UI-ONLY"); // Reveal/Cancel did not replace the stored credential.
        QSettings().clear();
    }
    void sentCheckboxCannotBeChanged() {
        Ft8Logbook log;
        auto record = LogbookUi::contact("W1AW", "SSTV", 14230000, "AE6LX");
        record["QRZCOM_QSO_UPLOAD_STATUS"] = "Y";
        record["QRZCOM_QSO_UPLOAD_DATE"] = "20260910";
        QVERIFY(log.append(record));
        QWidget host; host.resize(360, 696); host.show();
        bool checked = false;
        QTimer::singleShot(60, &host, [&] {
            auto *list = host.findChild<QListWidget *>("logContacts");
            auto *item = list->item(0);
            QVERIFY(!(item->flags() & Qt::ItemIsUserCheckable));
            list->setCurrentItem(item);
            QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, list->visualItemRect(item).topLeft() + QPoint(12, 24));
            QTest::keyClick(list, Qt::Key_Space);
            checked = item->checkState() == Qt::Checked;
            QVERIFY(!host.findChild<QPushButton *>("logQrzSend")->isEnabled());
            host.findChild<QPushButton *>("logBack")->click();
        });
        LogbookUi::show(&host, log);
        QVERIFY(checked);
    }
    void reviewCallsignSaveCancel() {
        QTemporaryDir dir;
        Ft8Logbook log(dir.filePath("contacts.json"));
        QVERIFY(log.load());
        QWidget host;
        host.resize(360, 696);
        host.show();
        const auto original = LogbookUi::contact("K1ABC", "SSTV", 14230000, "AE6LX");
        QTimer::singleShot(50, &host, [&] {
            QVERIFY(host.grab().save("sstv-qso-review-portrait.png"));
            host.resize(696, 360);
            QTest::qWait(30);
            QVERIFY(host.grab().save("sstv-qso-review-landscape.png"));
            host.findChild<QLineEdit *>("logField_CALL")->setText("VE3ABC/P");
            host.findChild<QPushButton *>("logCancel")->click();
        });
        QVERIFY(!LogbookUi::edit(&host, log, original, -1, true));
        QVERIFY(log.records().isEmpty());
        QTimer::singleShot(50, &host, [&] {
            host.findChild<QLineEdit *>("logField_CALL")->setText("ve3abc/p");
            host.findChild<QPushButton *>("logSave")->click();
        });
        QVERIFY(LogbookUi::edit(&host, log, original, -1, true));
        QCOMPARE(log.records().size(), 1);
        QCOMPARE(log.records()[0].value("CALL"), "VE3ABC/P");
        QCOMPARE(log.records()[0].value("MODE"), "SSTV");
        QCOMPARE(log.records()[0].value("FREQ"), "14.230000");
    }
    void addAnyModeAndRotate_data() {
        QTest::addColumn<QSize>("size");
        QTest::newRow("portrait") << QSize(360, 696);
        QTest::newRow("landscape") << QSize(696, 360);
    }
    void addAnyModeAndRotate() {
        QFETCH(QSize, size);
        Ft8Logbook log;
        QWidget host;
        host.resize(size);
        host.show();
        bool added = false, rotated = false;
        QTimer::singleShot(80, &host, [&] {
            QTimer::singleShot(80, &host, [&] {
                host.findChild<QLineEdit *>("logField_CALL")->setText("F/VE3ABC/P");
                host.findChild<QLineEdit *>("logField_MODE")->setText("CW");
                host.findChild<QLineEdit *>("logField_FREQ")->setText("7.030");
                host.findChild<QLineEdit *>("logField_RST_SENT")->setText("599");
                host.findChild<QPlainTextEdit *>("logExtraFields")->setPlainText("IOTA=EU-001\nAPP_MY_FIELD=any notes");
                host.resize(size.height(), size.width());
                QTest::qWait(30);
                auto *editor = host.findChild<InWindowDialog *>("logContactEditor");
                auto *save = host.findChild<QPushButton *>("logSave");
                rotated = editor && editor->rect().contains(QRect(save->mapTo(editor, QPoint()), save->size()));
                QVERIFY(host.grab().save(QString("log-add-%1.png").arg(QTest::currentDataTag())));
                save->click();
            });
            host.findChild<QPushButton *>("logAdd")->click();
            auto *list = host.findChild<QListWidget *>("logContacts");
            added = list && list->count() == 1;
            QVERIFY(host.grab().save(QString("log-list-%1.png").arg(QTest::currentDataTag())));
            host.findChild<QPushButton *>("logBack")->click();
        });
        LogbookUi::show(&host, log);
        QVERIFY(added);
        QVERIFY(rotated);
        QCOMPARE(log.records()[0].value("MODE"), "CW");
        QCOMPARE(log.records()[0].value("CALL"), "F/VE3ABC/P");
        QCOMPARE(log.records()[0].value("BAND"), "40m");
        QCOMPARE(log.records()[0].value("IOTA"), "EU-001");
        QCOMPARE(log.records()[0].value("APP_MY_FIELD"), "any notes");
    }
    void independentScreensPreserveContacts() {
        QTemporaryDir dir;
        const auto path = dir.filePath("contacts.json");
        Ft8Logbook ft8(path), sstv(path);
        QVERIFY(ft8.load());
        QVERIFY(sstv.load());
        QVERIFY(ft8.append(LogbookUi::contact("K1ABC", "FT8", 14074000)));
        QVERIFY(sstv.append(LogbookUi::contact("W1AW", "SSTV", 14230000)));
        QVERIFY(ft8.append(LogbookUi::contact("K2XYZ", "CW", 7030000)));
        QVERIFY(sstv.load());
        QCOMPARE(sstv.records().size(), 3);
        const auto roundtrip = Ft8Logbook::parse(sstv.exportAdif());
        QVERIFY(roundtrip.errors.isEmpty());
        QCOMPARE(roundtrip.records.size(), 3);
    }
};
QTEST_MAIN(LogbookTest)
#include "test_logbook.moc"
