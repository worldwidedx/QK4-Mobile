#include "logbookdialog.h"
#include "inwindowdialog.h"
#include "ft8/ft8types.h"
#include "ft8/qrzlogbook.h"
#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QScroller>
#include <QScrollerProperties>
#include <QStandardPaths>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>

namespace {
// Own the checkbox/text geometry: Android's styled delegate can move the text
// under a read-only indicator when a row becomes selected.
class ContactDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const override {
        return QSize(0, qMax(66, option.fontMetrics.height() * 3 + 16));
    }
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        p->save();
        p->setClipRect(option.rect);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        p->fillRect(option.rect, QColor(selected ? "#285b78" : "#13202d"));
        const QRect box(option.rect.left() + 10, option.rect.center().y() - 9, 18, 18);
        const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(QPen(QColor("#8ba7b9"), 1));
        p->setBrush(QColor(checked ? "#59d9b0" : "#13202d"));
        p->drawRoundedRect(box, 3, 3);
        if (checked) {
            p->setPen(QPen(QColor("#101314"), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->drawPolyline(QPolygonF{QPointF(box.left() + 4, box.top() + 9),
                QPointF(box.left() + 8, box.top() + 13), QPointF(box.left() + 15, box.top() + 5)});
        }
        const QRect text = option.rect.adjusted(40, 6, -10, -6);
        p->setFont(option.font);
        p->setPen(QColor("#e8eef5"));
        const auto lines = index.data(Qt::DisplayRole).toString().split('\n');
        const int height = option.fontMetrics.height();
        for (int i = 0; i < lines.size(); ++i)
            p->drawText(QRect(text.left(), text.top() + i * height, text.width(), height),
                Qt::AlignLeft | Qt::AlignVCenter, option.fontMetrics.elidedText(lines[i], Qt::ElideRight, text.width()));
        p->setPen(QColor("#203343"));
        p->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
        p->restore();
    }
};
class ContactList : public QListWidget {
public:
    explicit ContactList(QWidget *parent) : QListWidget(parent) {
        setItemDelegate(new ContactDelegate(this));
        setSelectionMode(QAbstractItemView::SingleSelection);
        setSelectionBehavior(QAbstractItemView::SelectRows);
        setEditTriggers(QAbstractItemView::NoEditTriggers);
    }
protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() != Qt::LeftButton) { QListWidget::mousePressEvent(event); return; }
        m_start = event->position().toPoint();
        m_pending = indexAt(m_start);
        m_dragged = false;
        event->accept(); // Selection remains provisional until release.
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if ((event->position().toPoint() - m_start).manhattanLength() > QApplication::startDragDistance())
            m_dragged = true;
        event->accept(); // QScroller owns touch scrolling.
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        const auto position = event->position().toPoint();
        const bool tap = event->button() == Qt::LeftButton && !m_dragged && m_pending.isValid() &&
            (position - m_start).manhattanLength() <= QApplication::startDragDistance() && indexAt(position) == m_pending;
        if (tap) {
            setCurrentIndex(m_pending);
            selectionModel()->select(m_pending, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            emit itemClicked(itemFromIndex(m_pending));
        }
        m_pending = QPersistentModelIndex();
        event->accept();
    }
private:
    QPoint m_start;
    QPersistentModelIndex m_pending;
    bool m_dragged = false;
};
QLabel *label(const QString &text, QWidget *parent) {
    auto *l = new QLabel(text, parent);
    l->setWordWrap(true);
    l->setTextFormat(Qt::PlainText);
    l->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    return l;
}
QPushButton *button(const QString &text, const QString &name, QWidget *parent) {
    auto *b = new QPushButton(text, parent);
    b->setObjectName(name);
    b->setMinimumHeight(34);
    return b;
}
void touchScroll(QAbstractScrollArea *area) {
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QScroller::grabGesture(area->viewport(), QScroller::TouchGesture);
    auto *scroller = QScroller::scroller(area->viewport());
    auto properties = scroller->scrollerProperties();
    properties.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.25);
    properties.setScrollMetric(QScrollerProperties::DragStartDistance, 0.0015);
    scroller->setScrollerProperties(properties);
    QObject::connect(scroller, &QScroller::stateChanged, area, [area](QScroller::State state) {
        if (state == QScroller::Dragging || state == QScroller::Scrolling) {
            area->setProperty("logDragging", true);
            for (auto *b : area->findChildren<QAbstractButton *>()) b->setDown(false);
        } else if (state == QScroller::Inactive) {
            QTimer::singleShot(120, area, [area] { area->setProperty("logDragging", false); });
        }
    });
}
struct Sheet {
    InWindowDialog dialog;
    QVBoxLayout *layout;
    QScrollArea *scroll;
    QWidget *body;
    QVBoxLayout *fields;
    void fitToContents(int maximumWidth) {
        auto *panel = dialog.contentWidget();
        panel->ensurePolished();
        body->ensurePolished();
        const int width = qMin(maximumWidth, qMax(1, dialog.parentWidget()->width() - 16));
        const auto margins = layout->contentsMargins();
        const auto border = panel->contentsMargins();
        const int innerWidth = qMax(1, width - margins.left() - margins.right() - border.left() - border.right());
        int height = margins.top() + margins.bottom() + border.top() + border.bottom();
        int count = 0;
        for (int i = 0; i < layout->count(); ++i) {
            auto *item = layout->itemAt(i);
            if (item->isEmpty()) continue;
            if (count++) height += layout->spacing();
            if (item->widget() == scroll) {
                const int frame = scroll->frameWidth() * 2;
                const int bodyHeight = fields->hasHeightForWidth()
                    ? fields->totalHeightForWidth(qMax(1, innerWidth - frame)) : fields->totalSizeHint().height();
                height += bodyHeight + frame;
            } else {
                height += item->hasHeightForWidth() ? item->heightForWidth(innerWidth) : item->sizeHint().height();
            }
        }
        dialog.setPanelSize(QSize(width, height));
    }
    explicit Sheet(QWidget *parent, const QString &title, const QString &name) : dialog(parent) {
        dialog.setObjectName(name);
        dialog.setStyleSheet(dialog.styleSheet() + QString("#%1 {background:rgba(0,0,0,150);}").arg(name));
        // Fit again on every parent resize, including portrait/landscape and IME.
        dialog.setPanelSize(QSize(1100, 1600));
        auto *panel = dialog.contentWidget();
        panel->setStyleSheet(
            "QWidget {background:#191919;color:#e8eef5;font-size:12px;} QLabel {background:transparent;}"
            "QLineEdit,QPlainTextEdit,QListWidget {background:#13202d;color:#e8eef5;border:1px solid #43596a;"
            "border-radius:4px;padding:5px;}"
            "QPushButton {background:#203343;color:#e8eef5;border:1px solid #506b7f;border-radius:5px;padding:4px;}"
            "QPushButton:pressed {background:#38627b;} QPushButton:disabled {color:#7c8e99;}"
            "QCheckBox::indicator,QListView::indicator {width:16px;height:16px;border:1px solid #8ba7b9;"
            "border-radius:3px;background:#13202d;}"
            "QCheckBox::indicator:checked,QListView::indicator:checked {background:#59d9b0;image:url(:/icons/check.svg);}"
            "QScrollArea {border:0;} QListWidget::item:selected {background:#285b78;}");
        layout = new QVBoxLayout(panel);
        layout->setSpacing(7);
        auto *heading = label(title, panel);
        heading->setStyleSheet("font-size:17px;font-weight:700;color:#6ce1ed;");
        layout->addWidget(heading);
        scroll = new QScrollArea(panel);
        scroll->setWidgetResizable(true);
        touchScroll(scroll);
        body = new QWidget(scroll);
        body->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        fields = new QVBoxLayout(body);
        fields->setContentsMargins(2, 2, 2, 2);
        fields->setSpacing(7);
        scroll->setWidget(body);
        layout->addWidget(scroll, 1);
    }
};
void importLog(QWidget *parent, Ft8Logbook &log) {
    const auto path = QFileDialog::getOpenFileName(parent, "Import ADIF log", {}, "ADIF (*.adi *.adif);;All files (*)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        showInWindowMessage(parent, "Import", file.errorString()); return;
    }
    if (file.size() > 20 * 1024 * 1024) {
        showInWindowMessage(parent, "Import", "This import is larger than 20 MB. Export a smaller part of the log."); return;
    }
    const auto preview = log.preview(QString::fromUtf8(file.readAll()));
    Sheet s(parent, "Review ADIF import", "logImportReview");
    auto *status = label(QString("%1 new contacts · %2 duplicates · %3 errors")
        .arg(preview.records.size()).arg(preview.duplicates).arg(preview.errors.size()), s.body);
    s.fields->addWidget(status);
    for (const auto &error : preview.errors.mid(0, 12)) s.fields->addWidget(label(error, s.body));
    s.fields->addStretch();
    auto *save = button("Import contacts", "logImportConfirm", s.dialog.contentWidget());
    save->setEnabled(preview.errors.isEmpty() && !preview.records.isEmpty());
    auto *cancel = button("Cancel", "logImportCancel", s.dialog.contentWidget());
    s.layout->addWidget(save);
    s.layout->addWidget(cancel);
    QObject::connect(cancel, &QPushButton::clicked, &s.dialog, &InWindowDialog::reject);
    QObject::connect(save, &QPushButton::clicked, &s.dialog, [&] {
        QString error;
        if (log.importRecords(preview, &error)) s.dialog.accept();
        else status->setText(error);
    });
    s.dialog.exec();
}
void exportLog(QWidget *parent, const QVector<AdifRecord> &records, bool practice) {
    QString error;
    const auto text = Ft8Logbook::encode(records, &error);
    if (text.isEmpty()) { showInWindowMessage(parent, "Export", error); return; }
    const auto path = QFileDialog::getSaveFileName(parent, "Export ADIF", practice ? "qk4-practice.adi" : "qk4-log.adi", "ADIF (*.adi)");
    if (path.isEmpty()) return;
    QSaveFile file(path);
    file.setDirectWriteFallback(true);
    const auto bytes = text.toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        showInWindowMessage(parent, "Export", file.errorString());
}
void qrzSetup(QWidget *parent, QrzLogbook *qrz) {
    Sheet s(parent, "QRZ logbook setup", "qrzSetupDialog");
    s.fields->addWidget(label("Use the API key from QRZ Logbook → Settings → API Access. "
        "QRZ requires an upload subscription. Each callsign, including /P, has its own logbook and key.", s.body));
    s.fields->addWidget(label("My QRZ logbook callsign", s.body));
    auto *call = new QLineEdit(qrz->callsign(), s.body);
    call->setObjectName("qrzCallsign");
    call->setMinimumHeight(34);
    s.fields->addWidget(call);
    s.fields->addWidget(label("Logbook API key", s.body));
    auto *key = new QLineEdit(s.body);
    key->setObjectName("qrzApiKey");
    key->setMinimumHeight(34);
    key->setEchoMode(QLineEdit::Password);
    key->setInputMethodHints(Qt::ImhHiddenText | Qt::ImhSensitiveData | Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase);
    key->setMaxLength(4096);
    key->setMinimumWidth(0);
    key->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    key->setPlaceholderText(qrz->configured() ? "Saved securely · leave blank to keep" : "Enter the QRZ logbook API key");
    auto *keyRow = new QHBoxLayout;
    auto *showKey = button("Show", "qrzShowKey", s.body);
    showKey->setMinimumWidth(60);
    keyRow->addWidget(key, 1);
    keyRow->addWidget(showKey);
    s.fields->addLayout(keyRow);
    s.fields->addWidget(label("The API key is encrypted on this phone with Android Keystore. "
        "Existing and imported contacts can be selected in the log and sent manually.", s.body));
    s.fields->addStretch();
    // Outside the scroll area: toggling cannot accidentally occur during a drag.
    auto *automatic = new QCheckBox("Auto-send new contacts to QRZ", s.dialog.contentWidget());
    automatic->setObjectName("qrzAutomatic");
    automatic->setMinimumHeight(34);
    automatic->setChecked(qrz->automatic());
    s.layout->addWidget(automatic);
    auto *status = label("", s.dialog.contentWidget());
    status->setObjectName("qrzSetupStatus");
    s.layout->addWidget(status);
    auto hideKey = [key, showKey] {
        key->setEchoMode(QLineEdit::Password);
        showKey->setText("Show");
    };
    QObject::connect(showKey, &QPushButton::clicked, &s.dialog, [&] {
        if (s.scroll->property("logDragging").toBool()) return;
        if (key->echoMode() == QLineEdit::Normal) { hideKey(); return; }
        if (key->text().isEmpty() && qrz->configured()) {
            QString error;
            const auto saved = qrz->revealKey(&error);
            if (!error.isEmpty()) { status->setText(error); return; }
            key->setText(saved);
            key->setCursorPosition(0);
        }
        key->setEchoMode(QLineEdit::Normal);
        showKey->setText("Hide");
    });
    QObject::connect(qApp, &QGuiApplication::applicationStateChanged, &s.dialog, [hideKey](Qt::ApplicationState state) {
        if (state != Qt::ApplicationActive) hideKey();
    });
    auto *row = new QHBoxLayout;
    auto *save = button("Save", "qrzSave", s.dialog.contentWidget());
    auto *forget = button("Remove key", "qrzForget", s.dialog.contentWidget());
    auto *cancel = button("Cancel", "qrzCancel", s.dialog.contentWidget());
    for (auto *b : {save, forget, cancel}) row->addWidget(b);
    s.layout->addLayout(row);
    QObject::connect(cancel, &QPushButton::clicked, &s.dialog, &InWindowDialog::reject);
    QObject::connect(save, &QPushButton::clicked, &s.dialog, [&] {
        QString error;
        if (qrz->configure(call->text(), key->text(), automatic->isChecked(), &error)) {
            key->clear(); s.dialog.accept();
        } else status->setText(error);
    });
    QObject::connect(forget, &QPushButton::clicked, &s.dialog, [&] {
        QString error;
        if (qrz->forget(&error)) { key->clear(); s.dialog.accept(); }
        else status->setText(error);
    });
    s.dialog.exec();
    key->clear();
}
}

QString LogbookUi::storagePath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logbook/contacts.json";
}
AdifRecord LogbookUi::contact(const QString &call, const QString &mode, qint64 frequencyHz,
                             const QString &operatorCall) {
    const auto now = QDateTime::currentDateTimeUtc();
    AdifRecord r{{"CALL", call.trimmed().toUpper()}, {"MODE", mode}, {"STATION_CALLSIGN", operatorCall},
                 {"QSO_DATE", now.toString("yyyyMMdd")}, {"TIME_ON", now.toString("HHmmss")}};
    if (frequencyHz > 0) {
        r["FREQ"] = QString::number(double(frequencyHz) / 1e6, 'f', 6);
        r["BAND"] = Ft8::bandFor(frequencyHz);
    }
    if (mode == "USB" || mode == "LSB") { r["MODE"] = "SSB"; r["SUBMODE"] = mode; }
    return r;
}
bool LogbookUi::edit(QWidget *parent, Ft8Logbook &log, const AdifRecord &source, int index, bool callsignOnly) {
    const auto record = source; // Network updates may reload the caller's log while this dialog is open.
    Sheet s(parent, callsignOnly ? "Log SSTV QSO" : index < 0 ? "Add contact" : "Edit contact", "logContactEditor");
    if (callsignOnly) {
        auto *panel = s.dialog.contentWidget();
        panel->setStyleSheet(panel->styleSheet()
            + "#inWindowDialogPanel {border:2px solid #6ce1ed;border-radius:8px;}");
        s.layout->setContentsMargins(12, 10, 12, 10);
    }
    QMap<QString, QLineEdit *> fields;
    const QList<QPair<QString, QString>> entries{
        {"CALL", "Callsign"}, {"QSO_DATE", "UTC date · YYYYMMDD"}, {"TIME_ON", "UTC start · HHMMSS"},
        {"FREQ", "Frequency · MHz"}, {"BAND", "Band"}, {"MODE", "Mode"}, {"SUBMODE", "Submode (optional)"},
        {"RST_SENT", "Report sent"}, {"RST_RCVD", "Report received"}, {"GRIDSQUARE", "Station grid"},
        {"STATION_CALLSIGN", "My callsign"}, {"MY_GRIDSQUARE", "My grid"}, {"QSO_DATE_OFF", "UTC end date (optional)"},
        {"TIME_OFF", "UTC end time (optional)"}, {"NAME", "Name"}, {"QTH", "Location"}, {"COMMENT", "Comments"}};
    for (const auto &entry : entries) {
        if (callsignOnly && entry.first != "CALL") continue;
        s.fields->addWidget(label(entry.second, s.body));
        auto *field = new QLineEdit(record.value(entry.first), s.body);
        field->setObjectName("logField_" + entry.first);
        field->setMinimumHeight(34);
        field->setMinimumWidth(0);
        field->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        if (callsignOnly) field->setMaximumWidth(180);
        s.fields->addWidget(field);
        fields[entry.first] = field;
    }
    QPlainTextEdit *extra = nullptr;
    if (!callsignOnly) {
        s.fields->addWidget(label("Additional ADIF fields · one FIELD=value per line", s.body));
        extra = new QPlainTextEdit(s.body);
        extra->setObjectName("logExtraFields");
        extra->setFixedHeight(100);
        QStringList values;
        for (auto it = record.cbegin(); it != record.cend(); ++it)
            if (!fields.contains(it.key()) && !QrzLogbook::managedField(it.key())) values << it.key() + "=" + it.value();
        extra->setPlainText(values.join('\n'));
        s.fields->addWidget(extra);
    } else {
        s.fields->addWidget(label(QString("%1 · %2 MHz · %3 UTC")
            .arg(record.value("MODE"), record.value("FREQ"), record.value("TIME_ON")), s.body));
    }
    s.fields->addStretch();
    auto *status = label(QrzLogbook::sent(record)
        ? "Already sent to QRZ. Edits here stay local; the sent checkbox remains locked."
        : "Review before saving.", s.dialog.contentWidget());
    status->setObjectName("logEditorStatus");
    if (callsignOnly) status->hide();
    s.layout->addWidget(status);
    auto *actions = new QHBoxLayout;
    auto *save = button("Save", "logSave", s.dialog.contentWidget());
    auto *cancel = button("Cancel", "logCancel", s.dialog.contentWidget());
    actions->addWidget(save);
    actions->addWidget(cancel);
    s.layout->addLayout(actions);
    bool saved = false;
    const auto showError = [&](const QString &message) {
        status->setText(message);
        status->show();
        if (callsignOnly) s.fitToContents(300);
    };
    QObject::connect(cancel, &QPushButton::clicked, &s.dialog, &InWindowDialog::reject);
    QObject::connect(save, &QPushButton::clicked, &s.dialog, [&] {
        auto updated = callsignOnly ? record : AdifRecord{};
        for (auto it = fields.cbegin(); it != fields.cend(); ++it) {
            const auto value = it.value()->text().trimmed();
            if (value.isEmpty()) updated.remove(it.key());
            else updated[it.key()] = value;
        }
        if (extra) {
            static const QRegularExpression keyPattern("^[A-Z][A-Z0-9_]*$");
            for (const auto &line : extra->toPlainText().split('\n')) {
                if (line.trimmed().isEmpty()) continue;
                const int split = line.indexOf('=');
                const auto key = line.left(split).trimmed().toUpper();
                if (split <= 0 || !keyPattern.match(key).hasMatch() || fields.contains(key) ||
                    QrzLogbook::managedField(key) || key == "EOR" || key == "EOH") {
                    showError("Use FIELD=value for additional fields; edit the listed fields above."); return;
                }
                updated[key] = line.mid(split + 1).trimmed();
            }
        }
        QString error;
        const bool ok = index < 0 ? log.append(updated, &error) : log.replace(index, updated, &error);
        if (!ok) { showError(error); return; }
        saved = true;
        s.dialog.accept();
    });
    if (callsignOnly) {
        s.fitToContents(300);
        QObject::connect(&s.dialog, &InWindowDialog::panelResized, &s.dialog,
                         [&](const QSize &) { s.fitToContents(300); });
    }
    s.dialog.exec();
    return saved;
}
void LogbookUi::show(QWidget *parent, Ft8Logbook &log, const AdifRecord &defaults, bool practice, QrzLogbook *uploads) {
    QString error;
    if (!log.load(&error)) { showInWindowMessage(parent, "Logbook", error); return; }
    Sheet s(parent, practice ? "Practice logbook" : "Logbook", "logbookDialog");
    auto *qrz = practice ? nullptr : uploads ? uploads : QrzLogbook::instance();
    // The list scrolls independently; action buttons remain reachable at all sizes.
    s.layout->removeWidget(s.scroll);
    s.scroll->hide();
    auto *search = new QLineEdit(s.dialog.contentWidget());
    search->setObjectName("logSearch");
    search->setPlaceholderText("Search callsign, band, mode, notes");
    search->setMinimumHeight(34);
    s.layout->addWidget(search);
    auto *list = new ContactList(s.dialog.contentWidget());
    list->setObjectName("logContacts");
    list->setMinimumHeight(40);
    touchScroll(list);
    s.layout->addWidget(list, 1);
    auto populate = [&] {
        const int selected = list->currentItem() ? list->currentItem()->data(Qt::UserRole).toInt() : -1;
        list->clear();
        for (int i = log.records().size() - 1; i >= 0; --i) {
            const auto &r = log.records()[i];
            const auto upload = r.value("QRZCOM_QSO_UPLOAD_STATUS").toUpper();
            const auto state = r.value("_QK4_QRZ_STATE");
            const auto qrzState = upload == "M" ? "QRZ sent · edited locally" : QrzLogbook::sent(r) ? "QRZ sent"
                : state == "SENDING" ? "QRZ sending" : state == "QUEUED" ? "QRZ queued"
                : state == "ERROR" ? "QRZ failed · select to retry" : "QRZ unsent";
            const auto summary = r.value("CALL") + " · " + r.value("BAND") + " · " + Ft8Logbook::canonicalMode(r)
                + "\n" + r.value("QSO_DATE") + " " + r.value("TIME_ON") + " UTC\n" + qrzState;
            if (!(summary + " " + r.value("COMMENT")).contains(search->text(), Qt::CaseInsensitive)) continue;
            auto *item = new QListWidgetItem(summary, list);
            item->setData(Qt::UserRole, i);
            item->setCheckState(QrzLogbook::sent(r) ? Qt::Checked : Qt::Unchecked);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            if (i == selected) list->setCurrentItem(item);
        }
    };
    populate();
    QObject::connect(search, &QLineEdit::textChanged, &s.dialog, populate);
    auto *row = new QHBoxLayout;
    auto *add = button("Add", "logAdd", s.dialog.contentWidget());
    auto *editButton = button("Edit", "logEdit", s.dialog.contentWidget());
    auto *import = button("Import", "logImport", s.dialog.contentWidget());
    auto *exportButton = button("Export", "logExport", s.dialog.contentWidget());
    for (auto *b : {add, editButton, import, exportButton}) row->addWidget(b);
    s.layout->addLayout(row);
    auto *qrzStatus = label(practice ? "Practice contacts are never uploaded." : "Tap a contact to select it. Checkbox = sent to QRZ.", s.dialog.contentWidget());
    qrzStatus->setObjectName("logQrzStatus");
    qrzStatus->setMaximumHeight(48);
    s.layout->addWidget(qrzStatus);
    auto *qrzRow = new QHBoxLayout;
    auto *send = button("Send to QRZ", "logQrzSend", s.dialog.contentWidget());
    auto *setup = button("Setup", "logSetup", s.dialog.contentWidget());
    setup->setEnabled(qrz);
    send->setEnabled(false);
    qrzRow->addWidget(send);
    qrzRow->addWidget(setup);
    s.layout->addLayout(qrzRow);
    auto updateSelection = [&] {
        const int index = list->currentItem() ? list->currentItem()->data(Qt::UserRole).toInt() : -1;
        bool canSend = qrz && index >= 0 && index < log.records().size();
        if (canSend) {
            const auto &r = log.records()[index];
            canSend = !QrzLogbook::sent(r) && r.value("_QK4_QRZ_STATE") != "SENDING" && r.value("_QK4_QRZ_STATE") != "QUEUED";
            qrzStatus->setText("Selected " + r.value("CALL") + " · " + (r.value("_QK4_QRZ_ERROR").isEmpty()
                ? QrzLogbook::sent(r) ? "already sent to QRZ" : "not sent to QRZ" : r.value("_QK4_QRZ_ERROR")));
        } else if (!practice) {
            qrzStatus->setText("Tap a contact to select it. Checkbox = sent to QRZ.");
        }
        send->setEnabled(canSend);
    };
    QObject::connect(list, &QListWidget::currentRowChanged, &s.dialog, updateSelection);
    QObject::connect(list, &QListWidget::itemClicked, &s.dialog, updateSelection);
    QObject::connect(setup, &QPushButton::clicked, &s.dialog, [&] { if (qrz) qrzSetup(&s.dialog, qrz); });
    QObject::connect(send, &QPushButton::clicked, &s.dialog, [&] {
        if (!qrz || !list->currentItem()) return;
        QString error;
        if (!qrz->send(list->currentItem()->data(Qt::UserRole).toInt(), &error))
            showInWindowMessage(&s.dialog, "Send to QRZ", error);
    });
    if (qrz) QObject::connect(qrz, &QrzLogbook::changed, &s.dialog, [&] {
        QString error;
        if (!log.load(&error)) { qrzStatus->setText(error); return; }
        populate(); updateSelection();
        qrzStatus->setText(qrz->status());
    });
    auto *close = button("Back", "logBack", s.dialog.contentWidget());
    s.layout->addWidget(close);
    QObject::connect(close, &QPushButton::clicked, &s.dialog, &InWindowDialog::reject);
    QObject::connect(add, &QPushButton::clicked, &s.dialog, [&] {
        auto r = contact({}, defaults.value("MODE"), 0, defaults.value("STATION_CALLSIGN"));
        for (const auto *field : {"FREQ", "BAND", "MY_GRIDSQUARE"})
            if (defaults.contains(field)) r[field] = defaults.value(field);
        if (practice) r["APP_QK4_PRACTICE"] = "Y";
        edit(&s.dialog, log, r);
        populate();
    });
    QObject::connect(editButton, &QPushButton::clicked, &s.dialog, [&] {
        if (!list->currentItem()) return;
        const int index = list->currentItem()->data(Qt::UserRole).toInt();
        edit(&s.dialog, log, log.records()[index], index);
        populate();
    });
    QObject::connect(import, &QPushButton::clicked, &s.dialog, [&] { importLog(&s.dialog, log); populate(); });
    QObject::connect(exportButton, &QPushButton::clicked, &s.dialog, [&] {
        QVector<AdifRecord> records;
        for (int i = 0; i < list->count(); ++i) records << log.records()[list->item(i)->data(Qt::UserRole).toInt()];
        exportLog(&s.dialog, records, practice);
    });
    s.dialog.exec();
}
