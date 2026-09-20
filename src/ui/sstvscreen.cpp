#include "sstvscreen.h"

#include "k4styles.h"
#include "sstv/sstvencoder.h"
#include "sstv/sstvmoderegistry.h"
#include "android/sstvmedia.h"
#include "inwindowdialog.h"
#include "sstvcomposercanvas.h"

#include <QComboBox>
#include <QCheckBox>
#include <QAbstractButton>
#include <QBoxLayout>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QListView>
#include <QProgressBar>
#include <QPushButton>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScroller>
#include <QScrollerProperties>
#include <QSignalBlocker>
#include <QTimer>
#include <QImageReader>
#include <QKeyEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QJsonObject>
#include <QSettings>
#include <QSlider>
#include <QScrollBar>
#include <QStackedWidget>
#include <QSpinBox>
#include <QSpacerItem>
#include <QStyle>
#include <QTransform>
#include <QVBoxLayout>
#include <QVariantMap>
#include <QtMath>

#include <algorithm>

namespace {
// Qt logical pixels render at roughly 3 device pixels on the reference phone.
// Keep every SSTV popup on this compact scale instead of inventing per-dialog
// desktop-sized fonts and touch targets.
constexpr int SstvDialogTitlePx = 13;
constexpr int SstvDialogBodyPx = 11;
constexpr int SstvDialogInputPx = 12;
constexpr int SstvDialogButtonPx = 11;
constexpr int SstvDialogControlHeight = 34;
// Keep the main TX composer and saved-template editor on one native-frame
// text-size scale. A 112 px ceiling lets a callsign occupy most of a 320 px
// SSTV frame while the common constants prevent the two editors drifting.
constexpr int SstvTextSizeMinimumPx = 12;
constexpr int SstvTextSizeMaximumPx = 112;
constexpr int SstvTextSizeDefaultPx = 28;

QString sstvBuiltinOverrideName(const QString &key) {
    if (key == QStringLiteral("builtin:cq"))
        return QStringLiteral("__QK4_DEFAULT_CQ__");
    if (key == QStringLiteral("builtin:report"))
        return QStringLiteral("__QK4_DEFAULT_REPORT__");
    if (key == QStringLiteral("builtin:73"))
        return QStringLiteral("__QK4_DEFAULT_73__");
    return QString();
}

bool sstvIsBuiltinOverrideName(const QString &name) {
    return name == QStringLiteral("__QK4_DEFAULT_CQ__")
        || name == QStringLiteral("__QK4_DEFAULT_REPORT__")
        || name == QStringLiteral("__QK4_DEFAULT_73__");
}

QString sstvBuiltinBaseName(const QString &key) {
    if (key == QStringLiteral("builtin:cq"))
        return QStringLiteral("CQ");
    if (key == QStringLiteral("builtin:report"))
        return QStringLiteral("REPORT");
    return QStringLiteral("73");
}

QString sstvBuiltinText(const QString &key) {
    if (key == QStringLiteral("builtin:cq"))
        return QStringLiteral("CQ CQ CQ\nDE {MY_CALL}");
    if (key == QStringLiteral("builtin:report"))
        return QStringLiteral("{TO_CALL}\nDE {MY_CALL}\nRST 595");
    return QStringLiteral("73 {TO_CALL}\nDE {MY_CALL}");
}

QString sstvVisibleCheckBoxStyle(int fontSizePx = 10, int indicatorSizePx = 20) {
    return QStringLiteral(
        "QCheckBox { color: #ffffff; font-size: %1px; font-weight: 700; "
        "spacing: 7px; padding: 2px; }"
        "QCheckBox::indicator { width: %2px; height: %2px; border: 2px solid #6dd4ef; "
        "border-radius: 3px; background-color: #101314; }"
        "QCheckBox::indicator:checked { border-color: #f2ad20; "
        "background-color: #f2ad20; image: url(:/icons/check.svg); }"
        "QCheckBox::indicator:unchecked:hover { border-color: #ffffff; }"
        "QCheckBox::indicator:checked:hover { border-color: #ffffff; }")
        .arg(fontSizePx)
        .arg(indicatorSizePx);
}

enum class SstvGlyph {
    Move, Draw, Line, Arrow, Rectangle, Ellipse, Color, RotateLeft, RotateRight,
    Camera, Undo, Redo, Trash, Save, Edit, Star, Share, ChevronDown
};

QIcon sstvGlyph(SstvGlyph glyph, const QColor &accent = QColor(QStringLiteral("#f0f0f0"))) {
    QPixmap pixmap(48, 48);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(accent, 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    if (glyph == SstvGlyph::RotateLeft || glyph == SstvGlyph::RotateRight) {
        // Match the familiar mobile-editor placement of the rotate arrowhead.
        // This changes only the glyph orientation; the left/right actions stay
        // bound to their existing counterclockwise/clockwise operations.
        painter.translate(24.0, 24.0);
        painter.rotate(180.0);
        painter.translate(-24.0, -24.0);
    }

    const auto arrowHead = [&painter, &accent](const QPointF &tip, const QPointF &a,
                                               const QPointF &b) {
        painter.setBrush(accent);
        painter.drawPolygon(QPolygonF{tip, a, b});
        painter.setBrush(Qt::NoBrush);
    };

    switch (glyph) {
    case SstvGlyph::Move:
        painter.drawLine(24, 7, 24, 41);
        painter.drawLine(7, 24, 41, 24);
        arrowHead({24, 4}, {18, 12}, {30, 12});
        arrowHead({24, 44}, {18, 36}, {30, 36});
        arrowHead({4, 24}, {12, 18}, {12, 30});
        arrowHead({44, 24}, {36, 18}, {36, 30});
        break;
    case SstvGlyph::Draw: {
        QPainterPath path;
        path.moveTo(6, 33);
        path.cubicTo(13, 10, 19, 42, 27, 20);
        path.cubicTo(31, 10, 35, 36, 43, 14);
        painter.drawPath(path);
        break;
    }
    case SstvGlyph::Line:
        painter.drawLine(8, 38, 40, 10);
        break;
    case SstvGlyph::Arrow:
        painter.drawLine(8, 38, 40, 10);
        arrowHead({42, 8}, {31, 11}, {39, 19});
        break;
    case SstvGlyph::Rectangle:
        painter.drawRect(QRectF(8, 11, 32, 26));
        break;
    case SstvGlyph::Ellipse:
        painter.drawEllipse(QRectF(7, 11, 34, 26));
        break;
    case SstvGlyph::Color:
        painter.setPen(QPen(QColor(QStringLiteral("#f0f0f0")), 3));
        painter.setBrush(accent);
        painter.drawRect(QRectF(9, 9, 30, 30));
        break;
    case SstvGlyph::RotateLeft:
    case SstvGlyph::RotateRight: {
        const bool left = glyph == SstvGlyph::RotateLeft;
        QPainterPath rotation;
        rotation.moveTo(39, 27);
        rotation.cubicTo(39, 38, 27, 44, 17, 39);
        rotation.cubicTo(7, 34, 5, 20, 13, 12);
        if (left) {
            painter.drawPath(rotation);
            arrowHead({11, 9}, {13, 22}, {2, 15});
        } else {
            QTransform mirror;
            mirror.translate(48, 0);
            mirror.scale(-1, 1);
            painter.drawPath(mirror.map(rotation));
            arrowHead({37, 9}, {35, 22}, {46, 15});
        }
        break;
    }
    case SstvGlyph::Camera:
        painter.drawRoundedRect(QRectF(6, 14, 36, 27), 4, 4);
        painter.drawEllipse(QRectF(17, 20, 14, 14));
        painter.drawLine(14, 14, 18, 9);
        painter.drawLine(18, 9, 29, 9);
        painter.drawLine(29, 9, 33, 14);
        break;
    case SstvGlyph::Undo:
    case SstvGlyph::Redo: {
        // Draw one canonical undo symbol and mirror the entire painter for
        // redo. This guarantees that redo is an exact horizontal mirror,
        // including its arc and arrowhead.
        if (glyph == SstvGlyph::Redo) {
            painter.translate(48.0, 0.0);
            painter.scale(-1.0, 1.0);
        }
        painter.drawArc(QRectF(10, 12, 29, 27), -45 * 16, 245 * 16);
        arrowHead({6, 15}, {18, 10}, {15, 23});
        break;
    }
    case SstvGlyph::Trash:
        painter.drawRoundedRect(QRectF(13, 15, 22, 27), 2, 2);
        painter.drawLine(10, 12, 38, 12);
        painter.drawLine(19, 8, 29, 8);
        painter.drawLine(20, 21, 20, 35);
        painter.drawLine(28, 21, 28, 35);
        break;
    case SstvGlyph::Save:
        painter.drawRoundedRect(QRectF(8, 7, 32, 34), 2, 2);
        painter.drawRect(QRectF(14, 8, 19, 11));
        painter.drawRect(QRectF(14, 27, 20, 13));
        break;
    case SstvGlyph::Edit: {
        const QPolygonF pencil{QPointF(10, 34), QPointF(31, 13),
                               QPointF(38, 20), QPointF(17, 41)};
        painter.drawPolygon(pencil);
        painter.drawLine(28, 16, 35, 23);
        painter.drawLine(10, 34, 17, 41);
        painter.drawLine(9, 42, 17, 41);
        break;
    }
    case SstvGlyph::Star: {
        QPolygonF star;
        for (int i = 0; i < 10; ++i) {
            const double angle = qDegreesToRadians(-90.0 + i * 36.0);
            const double radius = (i % 2 == 0) ? 18.0 : 8.0;
            star << QPointF(24 + qCos(angle) * radius, 24 + qSin(angle) * radius);
        }
        painter.drawPolygon(star);
        break;
    }
    case SstvGlyph::Share:
        painter.drawLine(15, 23, 33, 13);
        painter.drawLine(15, 25, 33, 35);
        painter.setBrush(accent);
        painter.drawEllipse(QPointF(11, 24), 5, 5);
        painter.drawEllipse(QPointF(37, 11), 5, 5);
        painter.drawEllipse(QPointF(37, 37), 5, 5);
        break;
    case SstvGlyph::ChevronDown:
        painter.drawLine(11, 18, 24, 31);
        painter.drawLine(24, 31, 37, 18);
        break;
    }
    return QIcon(pixmap);
}

void makeIconButton(QPushButton *button, SstvGlyph glyph, const QString &label,
                    const QColor &accent = QColor(QStringLiteral("#f0f0f0"))) {
    button->setText(QString());
    button->setIcon(sstvGlyph(glyph, accent));
    button->setIconSize(QSize(13, 13));
    button->setToolTip(label);
    button->setAccessibleName(label);
    button->setFixedSize(QSize(26, 26));
}

QString buttonStyle(const QString &color, bool active = false) {
    return QStringLiteral("QPushButton { background: %1; color: %2; border: 1px solid %3; "
                          "border-radius: 6px; padding: 8px; font-weight: bold; }"
                          "QPushButton:disabled { background: #191c1e; color: #aaa; border-color: #555; }")
        .arg(active ? color : QStringLiteral("#202426"), active ? QStringLiteral("#111") : QStringLiteral("#f0f0f0"), color);
}

QString sstvDialogButtonStyle(const QString &accent) {
    return buttonStyle(accent)
           + QStringLiteral("QPushButton { font-size: %1px; font-weight: 700; padding: 4px 8px; }")
                 .arg(SstvDialogButtonPx);
}

QSize sstvDialogPanelSize(QWidget *parent, int preferredWidth, int preferredHeight) {
    const QSize available = parent ? parent->size() - QSize(24, 20) : QSize(560, 300);
    const bool portrait = available.height() > available.width();
    const int widthLimit = portrait ? qRound(available.width() * 0.94)
                                    : qRound(available.width() * 0.74);
    return QSize(qMin(available.width(), qMin(preferredWidth, qMax(300, widthLimit))),
                 qMin(available.height(), preferredHeight));
}

bool askSstvQuestion(QWidget *parent, const QString &title, const QString &message,
                     const QString &actionLabel) {
    InWindowDialog dialog(parent);
    QWidget *panel = dialog.contentWidget();
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(7);

    auto *titleLabel = new QLabel(title.toUpper(), panel);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setWordWrap(true);
    titleLabel->setStyleSheet(
        QStringLiteral("color: #f2ad20; font-size: %1px; font-weight: 700;")
            .arg(SstvDialogTitlePx));
    layout->addWidget(titleLabel);

    auto *messageLabel = new QLabel(message, panel);
    messageLabel->setWordWrap(true);
    messageLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    messageLabel->setStyleSheet(
        QStringLiteral("color: #f0f0f0; font-size: %1px;").arg(SstvDialogBodyPx));
    layout->addWidget(messageLabel, 1);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(12);
    auto *cancel = new QPushButton(QStringLiteral("CANCEL"), panel);
    auto *confirm = new QPushButton(actionLabel, panel);
    for (QPushButton *button : {cancel, confirm}) {
        button->setFixedHeight(SstvDialogControlHeight);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
    cancel->setStyleSheet(sstvDialogButtonStyle(QStringLiteral("#6dd4ef")));
    confirm->setStyleSheet(sstvDialogButtonStyle(QStringLiteral("#f2ad20")));
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &InWindowDialog::reject);
    QObject::connect(confirm, &QPushButton::clicked, &dialog, &InWindowDialog::accept);
    buttons->addWidget(cancel, 1);
    buttons->addWidget(confirm, 1);
    layout->addLayout(buttons);

    const int preferredHeight = qMax(160, layout->sizeHint().height());
    dialog.setPanelSize(sstvDialogPanelSize(parent, 560, preferredHeight));
    return dialog.exec() == InWindowDialog::Accepted;
}

QString promptSstvTxText(QWidget *parent, const QString &title, const QString &label,
                         const QString &initialText, bool multiline, bool *accepted,
                         bool allowCallsignTokens = false) {
    if (accepted)
        *accepted = false;

    InWindowDialog dialog(parent);
    QWidget *panel = dialog.contentWidget();
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(7);

    auto *titleLabel = new QLabel(title.toUpper(), panel);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setWordWrap(true);
    titleLabel->setStyleSheet(
        QStringLiteral("color: #f2ad20; font-size: %1px; font-weight: 700;")
            .arg(SstvDialogTitlePx));
    layout->addWidget(titleLabel);

    auto *promptLabel = new QLabel(label, panel);
    promptLabel->setStyleSheet(
        QStringLiteral("color: #f0f0f0; font-size: %1px; font-weight: 600;")
            .arg(SstvDialogBodyPx));
    layout->addWidget(promptLabel);

    QPlainTextEdit *multiEditor = nullptr;
    QLineEdit *singleEditor = nullptr;
    if (multiline) {
        multiEditor = new QPlainTextEdit(initialText, panel);
        multiEditor->setMinimumHeight(72);
        multiEditor->setStyleSheet(QStringLiteral(
            "QPlainTextEdit { background: #f4f4f4; color: #111; border: 2px solid #6b7377; "
            "border-radius: 5px; padding: 5px; font-size: %1px; selection-background-color: #28bde8; }"
            "QPlainTextEdit:focus { border-color: #6dd4ef; }").arg(SstvDialogInputPx));
        layout->addWidget(multiEditor, 1);
    } else {
        singleEditor = new QLineEdit(initialText, panel);
        singleEditor->setFixedHeight(SstvDialogControlHeight);
        singleEditor->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #f4f4f4; color: #111; border: 2px solid #6b7377; "
            "border-radius: 5px; padding: 5px; font-size: %1px; selection-background-color: #28bde8; }"
            "QLineEdit:focus { border-color: #6dd4ef; }").arg(SstvDialogInputPx));
        layout->addWidget(singleEditor);
    }

    if (allowCallsignTokens && multiEditor) {
        auto *tokenButtons = new QHBoxLayout;
        tokenButtons->setSpacing(12);
        auto *myCall = new QPushButton(QStringLiteral("MY CALL"), panel);
        auto *toCall = new QPushButton(QStringLiteral("TO CALL"), panel);
        for (QPushButton *button : {myCall, toCall}) {
            button->setFixedHeight(SstvDialogControlHeight);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            button->setStyleSheet(sstvDialogButtonStyle(QStringLiteral("#6dd4ef")));
        }
        myCall->setToolTip(QStringLiteral("Insert {MY_CALL}"));
        toCall->setToolTip(QStringLiteral("Insert {TO_CALL}"));
        QObject::connect(myCall, &QPushButton::clicked, multiEditor, [multiEditor]() {
            multiEditor->insertPlainText(SstvComposerCanvas::myCallToken());
            multiEditor->setFocus(Qt::OtherFocusReason);
        });
        QObject::connect(toCall, &QPushButton::clicked, multiEditor, [multiEditor]() {
            multiEditor->insertPlainText(SstvComposerCanvas::toCallToken());
            multiEditor->setFocus(Qt::OtherFocusReason);
        });
        tokenButtons->addWidget(myCall, 1);
        tokenButtons->addWidget(toCall, 1);
        layout->addLayout(tokenButtons);
    }

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(12);
    auto *cancel = new QPushButton(QStringLiteral("CANCEL"), panel);
    auto *ok = new QPushButton(QStringLiteral("OK"), panel);
    for (QPushButton *button : {cancel, ok}) {
        button->setFixedHeight(SstvDialogControlHeight);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
    cancel->setStyleSheet(sstvDialogButtonStyle(QStringLiteral("#6dd4ef")));
    ok->setStyleSheet(sstvDialogButtonStyle(QStringLiteral("#f2ad20")));
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &InWindowDialog::reject);
    QObject::connect(ok, &QPushButton::clicked, &dialog, &InWindowDialog::accept);
    buttons->addWidget(cancel, 1);
    buttons->addWidget(ok, 1);
    layout->addLayout(buttons);

    const int preferredHeight = multiline ? (allowCallsignTokens ? 260 : 220) : 165;
    dialog.setPanelSize(sstvDialogPanelSize(parent, multiline ? 590 : 520, preferredHeight));
    if (multiEditor) {
        QTimer::singleShot(0, multiEditor, [multiEditor]() {
            multiEditor->setFocus(Qt::OtherFocusReason);
            multiEditor->selectAll();
        });
    } else {
        QTimer::singleShot(0, singleEditor, [singleEditor]() {
            singleEditor->setFocus(Qt::OtherFocusReason);
            singleEditor->selectAll();
        });
    }

    if (dialog.exec() != InWindowDialog::Accepted)
        return QString();
    if (accepted)
        *accepted = true;
    return multiline ? multiEditor->toPlainText() : singleEditor->text();
}

struct SstvTemplateSaveChoice {
    QString name;
    bool includeImage = false;
    bool accepted = false;
};

SstvTemplateSaveChoice promptSstvTemplateSave(QWidget *parent,
                                              const QString &initialName = QString(),
                                              bool initiallyIncludeImage = false) {
    SstvTemplateSaveChoice result;
    InWindowDialog dialog(parent);
    QWidget *panel = dialog.contentWidget();
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(7);

    auto *title = new QLabel(QStringLiteral("SAVE SSTV TEMPLATE"), panel);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        QStringLiteral("color: #f2ad20; font-size: %1px; font-weight: 700;")
            .arg(SstvDialogTitlePx));
    layout->addWidget(title);

    auto *nameLabel = new QLabel(QStringLiteral("TEMPLATE NAME"), panel);
    nameLabel->setStyleSheet(
        QStringLiteral("color: #f0f0f0; font-size: %1px; font-weight: 600;")
            .arg(SstvDialogBodyPx));
    layout->addWidget(nameLabel);

    auto *nameEdit = new QLineEdit(initialName, panel);
    nameEdit->setMaxLength(40);
    nameEdit->setFixedHeight(SstvDialogControlHeight);
    nameEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #f4f4f4; color: #111; border: 2px solid #6b7377; "
        "border-radius: 5px; padding: 5px; font-size: %1px; }"
        "QLineEdit:focus { border-color: #6dd4ef; }").arg(SstvDialogInputPx));
    layout->addWidget(nameEdit);

    auto *includeImage = new QCheckBox(QStringLiteral("SAVE CURRENT IMAGE WITH TEMPLATE"), panel);
    includeImage->setChecked(initiallyIncludeImage);
    includeImage->setToolTip(QStringLiteral(
        "Checked: applying the template replaces the TX image. "
        "Unchecked: only text and markup are applied to the current TX image."));
    includeImage->setStyleSheet(sstvVisibleCheckBoxStyle(10, 20));
    layout->addWidget(includeImage);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    auto *cancel = new QPushButton(QStringLiteral("CANCEL"), panel);
    auto *save = new QPushButton(QStringLiteral("SAVE"), panel);
    for (QPushButton *button : {cancel, save}) {
        button->setFixedHeight(SstvDialogControlHeight);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
    cancel->setStyleSheet(sstvDialogButtonStyle(QStringLiteral("#6dd4ef")));
    save->setStyleSheet(sstvDialogButtonStyle(QStringLiteral("#f2ad20")));
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &InWindowDialog::reject);
    QObject::connect(save, &QPushButton::clicked, &dialog, &InWindowDialog::accept);
    QObject::connect(nameEdit, &QLineEdit::returnPressed, &dialog, &InWindowDialog::accept);
    buttons->addWidget(cancel);
    buttons->addWidget(save);
    layout->addLayout(buttons);

    dialog.setPanelSize(sstvDialogPanelSize(parent, 520, 205));
    QTimer::singleShot(0, nameEdit, [nameEdit]() {
        nameEdit->setFocus(Qt::OtherFocusReason);
        nameEdit->selectAll();
    });
    if (dialog.exec() == InWindowDialog::Accepted) {
        result.name = nameEdit->text().trimmed();
        result.includeImage = includeImage->isChecked();
        result.accepted = !result.name.isEmpty();
    }
    return result;
}

QImage promptSstvGalleryImage(QWidget *parent, QString *error, bool *cancelled) {
    if (error)
        error->clear();
    if (cancelled)
        *cancelled = false;
    QString path;
#ifdef Q_OS_ANDROID
    if (!SstvMedia::openGallery(error))
        return QImage();
    QEventLoop waitLoop;
    QTimer pollTimer;
    pollTimer.setInterval(150);
    QObject::connect(&pollTimer, &QTimer::timeout, &waitLoop, [&]() {
        path = SstvMedia::takeCompletedImagePath();
        if (path.isEmpty() && SstvMedia::isOperationActive())
            return;
        pollTimer.stop();
        waitLoop.quit();
    });
    pollTimer.start();
    waitLoop.exec();
    if (path.isEmpty()) {
        const QString mediaError = SstvMedia::takeOperationError();
        if (mediaError.isEmpty()) {
            if (cancelled)
                *cancelled = true;
        } else if (error) {
            *error = mediaError;
        }
        return QImage();
    }
#else
    path = QFileDialog::getOpenFileName(
        parent, QStringLiteral("Choose template image"), QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
    if (path.isEmpty()) {
        if (cancelled)
            *cancelled = true;
        return QImage();
    }
#endif

    QImageReader reader(path);
    reader.setAutoTransform(true);
    QSize decodedSize = reader.size();
    if (decodedSize.isValid() && qMax(decodedSize.width(), decodedSize.height()) > 4096) {
        decodedSize.scale(QSize(4096, 4096), Qt::KeepAspectRatio);
        reader.setScaledSize(decodedSize);
    }
    const QImage image = reader.read();
#ifdef Q_OS_ANDROID
    QFile::remove(path);
#endif
    if (image.isNull() && error)
        *error = reader.errorString();
    return image;
}

QColor promptSstvMarkupColor(QWidget *parent, const QColor &currentColor,
                            const QString &dialogTitle = QStringLiteral("MARKUP COLOR"),
                            bool allowNoFill = false) {
    struct PaletteEntry {
        const char *name;
        const char *hex;
    };
    static const PaletteEntry palette[] = {
        {"DEEP RED", "#7A0019"},       {"DARK RED", "#B5122A"},
        {"RED", "#E3262E"},            {"LIGHT RED", "#FF6B6B"},
        {"BURNT ORANGE", "#8F3F00"},   {"DARK ORANGE", "#C65D00"},
        {"ORANGE", "#FF8C00"},         {"LIGHT ORANGE", "#FFB347"},
        {"DARK YELLOW", "#806300"},    {"OCHRE", "#B88A00"},
        {"YELLOW", "#FFD700"},         {"LIGHT YELLOW", "#FFF176"},
        {"FOREST GREEN", "#0B5D1E"},   {"DARK GREEN", "#16823A"},
        {"GREEN", "#2EBD59"},          {"LIGHT GREEN", "#7DDF8B"},
        {"NAVY BLUE", "#0B1F5E"},      {"ROYAL BLUE", "#1D4ED8"},
        {"BLUE", "#1E88E5"},           {"SKY BLUE", "#64B5F6"},
        {"DEEP INDIGO", "#2E1065"},    {"DARK INDIGO", "#4338CA"},
        {"INDIGO", "#5B5CE2"},         {"LIGHT INDIGO", "#818CF8"},
        {"DEEP VIOLET", "#4A044E"},    {"DARK VIOLET", "#7E22CE"},
        {"VIOLET", "#A855F7"},         {"LIGHT VIOLET", "#D8B4FE"},
        {"BLACK", "#000000"},          {"CHARCOAL", "#303030"},
        {"DARK GREY", "#606060"},      {"GREY", "#909090"},
        {"SILVER", "#C0C0C0"},        {"LIGHT GREY", "#E0E0E0"},
        {"WHITE", "#FFFFFF"}
    };

    InWindowDialog dialog(parent);
    QWidget *panel = dialog.contentWidget();
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(5);

    auto *title = new QLabel(dialogTitle, panel);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        QStringLiteral("color: white; font-size: %1px; font-weight: 700;")
            .arg(SstvDialogTitlePx));
    layout->addWidget(title);

    auto *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(5);
    grid->setVerticalSpacing(5);
    // Four columns keep each shade family together in portrait; landscape
    // places two four-shade families on each row without growing the dialog.
    const int columns = parent && parent->width() < 520 ? 4 : 8;
    QColor selected;
    int cellIndex = 0;
    if (allowNoFill) {
        const bool choosingOutline = dialogTitle.contains(QStringLiteral("OUTLINE"),
                                                          Qt::CaseInsensitive);
        const QString transparentLabel = choosingOutline
            ? QStringLiteral("NO OUTLINE") : QStringLiteral("TRANSPARENT");
        auto *noFill = new QPushButton(transparentLabel, panel);
        noFill->setMinimumHeight(32);
        noFill->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        noFill->setAccessibleName(QStringLiteral("Select %1").arg(transparentLabel.toLower()));
        noFill->setToolTip(transparentLabel);
        noFill->setStyleSheet(
            QStringLiteral("QPushButton { background: #202426; color: white; border: 3px solid %1; "
                           "border-radius: 6px; padding: 3px; font-size: 9px; font-weight: 700; }")
                .arg(currentColor.alpha() == 0 ? QStringLiteral("#f2ad20")
                                               : QStringLiteral("#858585")));
        grid->addWidget(noFill, 0, 0);
        QObject::connect(noFill, &QPushButton::clicked, &dialog,
                         [&dialog, &selected]() {
            selected = QColor(Qt::transparent);
            dialog.accept();
        });
        ++cellIndex;
    }
    for (int index = 0; index < static_cast<int>(std::size(palette)); ++index) {
        const PaletteEntry &entry = palette[index];
        const QColor color(QString::fromLatin1(entry.hex));
        auto *swatch = new QPushButton(panel);
        swatch->setMinimumHeight(32);
        swatch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        swatch->setAccessibleName(QStringLiteral("Select %1 markup color")
                                      .arg(QString::fromLatin1(entry.name).toLower()));
        swatch->setToolTip(QStringLiteral("%1 (%2)")
                               .arg(QString::fromLatin1(entry.name), color.name().toUpper()));
        const bool dark = color.lightness() < 135;
        const QString border = color == currentColor
                                   ? QStringLiteral("#f2ad20")
                                   : QStringLiteral("#858585");
        swatch->setStyleSheet(
            QStringLiteral("QPushButton { background: %1; color: %2; border: 3px solid %3; "
                           "border-radius: 6px; padding: 3px; } "
                           "QPushButton:pressed { border-color: white; }")
                .arg(color.name(), dark ? QStringLiteral("white") : QStringLiteral("black"), border));
        grid->addWidget(swatch, cellIndex / columns, cellIndex % columns);
        ++cellIndex;
        QObject::connect(swatch, &QPushButton::clicked, &dialog,
                         [&dialog, &selected, color]() {
            selected = color;
            dialog.accept();
        });
    }
    layout->addLayout(grid);

    auto *cancel = new QPushButton(QStringLiteral("CANCEL"), panel);
    cancel->setFixedHeight(SstvDialogControlHeight);
    cancel->setStyleSheet(K4Styles::menuBarButton());
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &InWindowDialog::reject);
    layout->addWidget(cancel);

    const QSize available = parent ? parent->size() - QSize(20, 16) : QSize(560, 320);
    const int panelWidth = qMin(620, qMax(280, available.width()));
    const int panelHeight = qMin(available.height(), qMax(220, layout->sizeHint().height()));
    dialog.setPanelSize(QSize(panelWidth, panelHeight));
    return dialog.exec() == InWindowDialog::Accepted ? selected : QColor();
}

int promptSstvFont(QWidget *parent, const QComboBox *fontOptions, int currentIndex) {
    if (!parent || !fontOptions || fontOptions->count() == 0)
        return -1;

    InWindowDialog dialog(parent);
    QWidget *panel = dialog.contentWidget();
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(5);

    auto *title = new QLabel(QStringLiteral("SELECT FONT"), panel);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        QStringLiteral("color: #f2ad20; font-size: %1px; font-weight: 700;")
            .arg(SstvDialogTitlePx));
    layout->addWidget(title);

    auto *list = new QListWidget(panel);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setStyleSheet(QStringLiteral(
        "QListWidget { background: #171b1d; color: white; border: 1px solid #516067; "
        "border-radius: 5px; font-size: 11px; }"
        "QListWidget::item { padding: 4px 7px; }"
        "QListWidget::item:selected { background: #245463; border: 1px solid #6dd4ef; }"));
    for (int index = 0; index < fontOptions->count(); ++index) {
        auto *item = new QListWidgetItem(fontOptions->itemText(index), list);
        item->setData(Qt::UserRole, index);
        item->setSizeHint(QSize(0, 28));
        QFont preview = fontOptions->itemData(index, Qt::FontRole).value<QFont>();
        preview.setPixelSize(SstvDialogBodyPx);
        item->setFont(preview);
    }
    list->setCurrentRow(qBound(0, currentIndex, list->count() - 1));
#ifdef Q_OS_ANDROID
    list->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    QScroller::grabGesture(list->viewport(), QScroller::TouchGesture);
#endif
    layout->addWidget(list, 1);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(7);
    auto *cancel = new QPushButton(QStringLiteral("CANCEL"), panel);
    auto *use = new QPushButton(QStringLiteral("USE FONT"), panel);
    for (QPushButton *button : {cancel, use}) {
        button->setFixedHeight(SstvDialogControlHeight);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
    cancel->setStyleSheet(sstvDialogButtonStyle(QStringLiteral("#6dd4ef")));
    use->setStyleSheet(sstvDialogButtonStyle(QStringLiteral("#f2ad20")));
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &InWindowDialog::reject);
    QObject::connect(use, &QPushButton::clicked, &dialog, &InWindowDialog::accept);
    buttons->addWidget(cancel);
    buttons->addWidget(use);
    layout->addLayout(buttons);

    dialog.setPanelSize(sstvDialogPanelSize(parent, 360, 430));
    if (dialog.exec() != InWindowDialog::Accepted || !list->currentItem())
        return -1;
    return list->currentItem()->data(Qt::UserRole).toInt();
}

// QComboBox creates a separate native popup window on Android. With Qt's
// OpenGL-backed Android platform plugin, rapidly opening or dismissing that
// window can race eglSurface() teardown and abort the whole process. Keep the
// familiar combo presentation and model API, but render its choices inside the
// existing SSTV window so no second EGL surface is ever created.
class SstvInWindowComboBox final : public QComboBox {
public:
    explicit SstvInWindowComboBox(const QString &dialogTitle, QWidget *parent = nullptr)
        : QComboBox(parent), m_dialogTitle(dialogTitle) {}

protected:
    void showPopup() override {
        if (!isEnabled() || count() == 0)
            return;

        QWidget *dialogParent = parentWidget();
        for (QWidget *candidate = parentWidget(); candidate; candidate = candidate->parentWidget()) {
            if (candidate->objectName() == QStringLiteral("sstvScreen")) {
                dialogParent = candidate;
                break;
            }
        }
        if (!dialogParent)
            return;

        InWindowDialog dialog(dialogParent);
        QWidget *panel = dialog.contentWidget();
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(8, 6, 8, 6);
        layout->setSpacing(5);

        auto *title = new QLabel(m_dialogTitle, panel);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet(
            QStringLiteral("color: #f2ad20; font-size: %1px; font-weight: 700;")
                .arg(SstvDialogTitlePx));
        layout->addWidget(title);

        auto *list = new QListWidget(panel);
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        list->setStyleSheet(QStringLiteral(
            "QListWidget { background: #171b1d; color: white; border: 1px solid #516067; "
            "border-radius: 5px; font-size: 11px; }"
            "QListWidget::item { padding: 4px 7px; }"
            "QListWidget::item:selected { background: #245463; border: 1px solid #6dd4ef; }"));
        for (int index = 0; index < count(); ++index) {
            auto *item = new QListWidgetItem(itemIcon(index), itemText(index), list);
            item->setData(Qt::UserRole, index);
            item->setSizeHint(QSize(0, 30));
            const QVariant fontData = itemData(index, Qt::FontRole);
            if (fontData.canConvert<QFont>()) {
                QFont preview = fontData.value<QFont>();
                preview.setPixelSize(SstvDialogBodyPx);
                item->setFont(preview);
            }
        }
        const int initialIndex = qBound(0, currentIndex(), list->count() - 1);
        list->setCurrentRow(initialIndex);
#ifdef Q_OS_ANDROID
        list->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
        QScroller::grabGesture(list->viewport(), QScroller::TouchGesture);
#endif
        layout->addWidget(list, 1);

        auto *buttons = new QHBoxLayout;
        buttons->setSpacing(7);
        auto *cancel = new QPushButton(QStringLiteral("CANCEL"), panel);
        auto *use = new QPushButton(QStringLiteral("USE"), panel);
        for (QPushButton *button : {cancel, use}) {
            button->setFixedHeight(SstvDialogControlHeight);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        }
        cancel->setStyleSheet(sstvDialogButtonStyle(QStringLiteral("#6dd4ef")));
        use->setStyleSheet(sstvDialogButtonStyle(QStringLiteral("#f2ad20")));
        QObject::connect(cancel, &QPushButton::clicked, &dialog, &InWindowDialog::reject);
        QObject::connect(use, &QPushButton::clicked, &dialog, &InWindowDialog::accept);
        QObject::connect(list, &QListWidget::itemDoubleClicked, &dialog,
                         [&dialog](QListWidgetItem *) { dialog.accept(); });
        buttons->addWidget(cancel);
        buttons->addWidget(use);
        layout->addLayout(buttons);

        const int visibleRows = qMin(9, count());
        const int preferredHeight = 86 + visibleRows * 30;
        dialog.setPanelSize(sstvDialogPanelSize(dialogParent, 560, preferredHeight));
        QTimer::singleShot(0, list, [list, initialIndex]() {
            if (QListWidgetItem *item = list->item(initialIndex))
                list->scrollToItem(item, QAbstractItemView::PositionAtCenter);
        });
        if (dialog.exec() == InWindowDialog::Accepted && list->currentItem()) {
            const int selectedIndex = list->currentItem()->data(Qt::UserRole).toInt();
            setCurrentIndex(selectedIndex);
            // Match a native combo box's explicit user-selection signal even
            // when the operator chooses the item that is already displayed.
            // Consumers such as the TX template selector use this to make USE
            // the complete action instead of requiring a second check button.
            emit activated(selectedIndex);
        }
    }

private:
    QString m_dialogTitle;
};

QImage fitWithBars(const QImage &source, const QSize &target) {
    QImage frame(target, QImage::Format_RGB32);
    frame.fill(QColor(QStringLiteral("#15191b")));
    const QImage scaled = source.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPainter painter(&frame);
    painter.drawImage((target.width() - scaled.width()) / 2, (target.height() - scaled.height()) / 2, scaled);
    return frame;
}

QRectF framingCrop(const QSize &sourceSize, const QSize &target, bool fitBars,
                   double zoom, const QPointF &normalizedCenter) {
    if (sourceSize.isEmpty() || target.isEmpty())
        return QRectF();
    double baseWidth = sourceSize.width();
    double baseHeight = sourceSize.height();
    if (!fitBars) {
        const double sourceAspect = baseWidth / baseHeight;
        const double targetAspect = static_cast<double>(target.width()) / target.height();
        if (sourceAspect > targetAspect)
            baseWidth = baseHeight * targetAspect;
        else
            baseHeight = baseWidth / targetAspect;
    }
    zoom = qBound(1.0, zoom, 4.0);
    const double cropWidth = qMax(1.0, baseWidth / zoom);
    const double cropHeight = qMax(1.0, baseHeight / zoom);
    const double centerX = qBound(cropWidth * 0.5,
                                  normalizedCenter.x() * sourceSize.width(),
                                  sourceSize.width() - cropWidth * 0.5);
    const double centerY = qBound(cropHeight * 0.5,
                                  normalizedCenter.y() * sourceSize.height(),
                                  sourceSize.height() - cropHeight * 0.5);
    return QRectF(centerX - cropWidth * 0.5, centerY - cropHeight * 0.5,
                  cropWidth, cropHeight);
}

QImage frameSource(const QImage &source, const QSize &target, bool fitBars,
                   double zoom, const QPointF &normalizedCenter) {
    const QRect crop = framingCrop(source.size(), target, fitBars, zoom, normalizedCenter)
                           .toAlignedRect().intersected(source.rect());
    if (crop.isEmpty())
        return QImage();
    const QImage framedSource = source.copy(crop);
    return fitBars ? fitWithBars(framedSource, target)
                   : framedSource.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

}

SstvScreen::SstvScreen(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("sstvScreen"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(true);
    QPalette surfacePalette = palette();
    surfacePalette.setColor(QPalette::Window, QColor(QStringLiteral("#101314")));
    setPalette(surfacePalette);
    setStyleSheet(QStringLiteral(
        "#sstvScreen { background-color: #101314; }"
        "#sstvScreen QLabel { color: #f0f0f0; }"
        "#sstvScreen QLineEdit, #sstvScreen QComboBox, #sstvScreen QSpinBox {"
        "  background-color: #f4f4f4; color: #111111; border: 2px solid #555b5e;"
        "  border-radius: 5px; padding: 5px 8px; selection-background-color: #28bde8;"
        "  selection-color: #111111; placeholder-text-color: #666666; }"
        "#sstvScreen QLineEdit:focus, #sstvScreen QComboBox:focus, #sstvScreen QSpinBox:focus { border-color: #6dd4ef; }"
        "#sstvScreen QLineEdit:disabled, #sstvScreen QComboBox:disabled, #sstvScreen QSpinBox:disabled {"
        "  background-color: #d0d0d0; color: #666666; border-color: #555555; }"
        "#sstvScreen QCheckBox { color: #f0f0f0; spacing: 6px; font-weight: bold; }"
        "#sstvScreen QCheckBox::indicator { width: 24px; height: 24px; }"
        "#sstvScreen QComboBox QAbstractItemView {"
        "  background-color: #f4f4f4; color: #111111; selection-background-color: #28bde8;"
        "  selection-color: #111111; }"));
    m_mediaPollTimer = new QTimer(this);
    m_mediaPollTimer->setInterval(150);
    connect(m_mediaPollTimer, &QTimer::timeout, this, &SstvScreen::pollImportedImage);
    m_draftTimer = new QTimer(this);
    m_draftTimer->setSingleShot(true);
    m_draftTimer->setInterval(900);
    connect(m_draftTimer, &QTimer::timeout, this, &SstvScreen::saveDraft);
    setupUi();
    refreshReceiveHistory();
    refreshTemplates();
    restoreDraft();
}

void SstvScreen::requestLogQso(bool transmit) {
    qint64 frequency = 0;
    QDateTime receivedUtc;
    if (!transmit) {
        for (const auto &record : m_receiveRecords)
            if (record.id == m_currentReceiveId) {
                frequency = record.frequencyHz;
                receivedUtc = record.receivedUtc;
                break;
            }
    }
    emit logQsoRequested((transmit ? m_toCallsignEdit : m_receiveCallsignEdit)->text().trimmed().toUpper(),
                         transmit, frequency, receivedUtc);
}
void SstvScreen::setupUi() {
    auto *root = new QVBoxLayout(this);
    // The stacked TX editor has a large natural width. Do not let its hidden
    // page impose that width on the complete SSTV surface and push visible RX
    // controls under Android's navigation inset on narrower canvases.
    root->setSizeConstraint(QLayout::SetNoConstraint);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(8);

    auto *header = new QHBoxLayout;
    m_backButton = new QPushButton(QStringLiteral("BACK TO RADIO"), this);
    m_backButton->setStyleSheet(buttonStyle(QStringLiteral("#28bde8")));
    connect(m_backButton, &QPushButton::clicked, this, [this]() {
        flushDraft();
        emit closeRequested();
    });
    auto *title = new QLabel(QStringLiteral("SSTV"), this);
    title->setStyleSheet(QStringLiteral("font-size: 24px; font-weight: bold; color: #ffffff;"));
    auto *radioState = new QVBoxLayout;
    radioState->setContentsMargins(0, 0, 0, 0);
    radioState->setSpacing(0);
    m_rxRadioState = new QLabel(QStringLiteral("RX —.---.--- —"), this);
    m_txRadioState = new QLabel(QStringLiteral("TX —.---.--- —"), this);
    m_rxRadioState->setStyleSheet(
        QStringLiteral("font-size: 13px; font-weight: 700; color: #6dd4ef;"));
    m_txRadioState->setStyleSheet(
        QStringLiteral("font-size: 13px; font-weight: 700; color: #f2ad20;"));
    for (QLabel *label : {m_rxRadioState, m_txRadioState}) {
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        label->setMinimumWidth(0);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }
    radioState->addWidget(m_rxRadioState);
    radioState->addWidget(m_txRadioState);
    header->addWidget(m_backButton);
    header->addWidget(title);
    header->addLayout(radioState, 1);
    root->addLayout(header);
    m_protectionLabel = new QLabel("TX protection ready · calibrate audio before first use", this);
    m_protectionLabel->setObjectName("sstvTxProtection");
    m_protectionLabel->setWordWrap(true);
    m_protectionLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_protectionLabel->setMinimumWidth(0);
    root->addWidget(m_protectionLabel);

    auto *tabs = new QHBoxLayout;
    m_receiveTab = new QPushButton(QStringLiteral("RECEIVE"), this);
    m_transmitTab = new QPushButton(QStringLiteral("TRANSMIT"), this);
    m_receiveTab->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_transmitTab->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    connect(m_receiveTab, &QPushButton::clicked, this, [this]() { selectTab(false); });
    connect(m_transmitTab, &QPushButton::clicked, this, [this]() { selectTab(true); });
    tabs->addWidget(m_receiveTab, 1);
    tabs->addWidget(m_transmitTab, 1);
    auto *logbook = new QPushButton(QStringLiteral("Logbook"), this);
    logbook->setObjectName("sstvLogbook");
    logbook->setStyleSheet(buttonStyle(QStringLiteral("#6dd4ef")));
    logbook->setFixedHeight(30);
    tabs->addWidget(logbook);
    connect(logbook, &QPushButton::clicked, this, &SstvScreen::logbookRequested);
    root->addLayout(tabs);

    m_pages = new QStackedWidget(this);
    m_pages->setMinimumSize(0, 0);
    m_pages->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto *receivePage = new QWidget(m_pages);
    receivePage->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto *receiveLayout = new QVBoxLayout(receivePage);
    m_receiveStatus = new QLabel(QStringLiteral("AUTO RX ON • MODE AUTO • SYNC WAITING • SLANT AUTO"), receivePage);
    m_receiveStatus->setStyleSheet(QStringLiteral("color: #63df55; font-weight: bold;"));
    m_receiveStatus->setWordWrap(true);
    m_receiveStatus->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto *receiveStatusRow = new QHBoxLayout;
    receiveStatusRow->addWidget(m_receiveStatus, 1);
    receiveStatusRow->addWidget(new QLabel(QStringLiteral("AUDIO"), receivePage));
    m_receiveStreamState = new QLabel(QStringLiteral("NO STREAM"), receivePage);
    m_receiveStreamState->setStyleSheet(QStringLiteral("color: #ff786e; font-weight: bold;"));
    receiveStatusRow->addWidget(m_receiveStreamState);
    m_receiveLevel = new QProgressBar(receivePage);
    m_receiveLevel->setRange(0, 100);
    m_receiveLevel->setValue(0);
    m_receiveLevel->setTextVisible(false);
    m_receiveLevel->setFixedSize(110, 10);
    m_receiveLevel->setStyleSheet(QStringLiteral(
        "QProgressBar { background: #25292b; border: 1px solid #555; border-radius: 4px; }"
        "QProgressBar::chunk { background: #63df55; border-radius: 3px; }"));
    receiveStatusRow->addWidget(m_receiveLevel);
    m_receiveImage = new QLabel(QStringLiteral("Waiting for an SSTV VIS header"), receivePage);
    m_receiveImage->setAlignment(Qt::AlignCenter);
    m_receiveImage->setMinimumSize(160, 80);
    m_receiveImage->setStyleSheet(QStringLiteral("background: #1b2022; border: 1px solid #28bde8; font-size: 18px;"));
    receiveLayout->addLayout(receiveStatusRow);
    receiveLayout->addWidget(m_receiveImage, 1);
    // Give each RX data row the same label column. This moves the RX CALL
    // editor and KEEP selector onto the RX HISTORY control line instead of
    // leaving three visually unrelated starting positions.
    const int receiveFieldLabelWidth =
        QFontMetrics(receivePage->font()).horizontalAdvance(QStringLiteral("RX HISTORY")) + 8;
    auto makeReceiveFieldLabel = [receivePage, receiveFieldLabelWidth](const QString &text) {
        auto *label = new QLabel(text, receivePage);
        label->setFixedWidth(receiveFieldLabelWidth);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        return label;
    };
    auto *receiveCallsignRow = new QHBoxLayout;
    receiveCallsignRow->setSpacing(5);
    m_receiveCallsignEdit = new QLineEdit(receivePage);
    m_receiveCallsignEdit->setPlaceholderText(QStringLiteral("NOT DETECTED"));
    m_receiveCallsignEdit->setMaxLength(16);
    m_receiveCallsignEdit->setMinimumWidth(120);
    m_receiveCallsignEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_receiveCallsignEdit->setAccessibleName(QStringLiteral("Received station callsign"));
    m_receiveCallsignSource = new QLabel(QStringLiteral("—"), receivePage);
    m_receiveCallsignSource->setStyleSheet(QStringLiteral("color: #aeb8bc; font-weight: 700;"));
    m_replyReceiveButton = new QPushButton(QStringLiteral("REPLY"), receivePage);
    m_replyReceiveButton->setStyleSheet(buttonStyle(QStringLiteral("#f2ad20")));
    m_replyReceiveButton->setEnabled(false);
    receiveCallsignRow->addWidget(makeReceiveFieldLabel(QStringLiteral("RX CALL")));
    receiveCallsignRow->addWidget(m_receiveCallsignEdit, 1);
    receiveCallsignRow->addWidget(m_receiveCallsignSource);
    receiveCallsignRow->addWidget(m_replyReceiveButton);
    receiveLayout->addLayout(receiveCallsignRow);
    connect(m_receiveCallsignEdit, &QLineEdit::editingFinished, this,
            [this]() { saveCurrentReceiveCallsign(true); });
    connect(m_replyReceiveButton, &QPushButton::clicked,
            this, &SstvScreen::replyToCurrentReceive);
    auto *receiveHistoryRow = new QHBoxLayout;
    m_receiveHistoryCombo = new SstvInWindowComboBox(
        QStringLiteral("SELECT RX HISTORY"), receivePage);
    m_receiveHistoryCombo->setMinimumContentsLength(10);
    m_receiveHistoryCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_receiveHistoryCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_starReceiveButton = new QPushButton(receivePage);
    m_shareReceiveButton = new QPushButton(receivePage);
    m_deleteReceiveButton = new QPushButton(receivePage);
    for (QPushButton *button : {m_starReceiveButton, m_shareReceiveButton, m_deleteReceiveButton})
        button->setStyleSheet(buttonStyle(QStringLiteral("#6dd4ef")));
    makeIconButton(m_starReceiveButton, SstvGlyph::Star, QStringLiteral("Star received image"));
    makeIconButton(m_shareReceiveButton, SstvGlyph::Share, QStringLiteral("Share received image"));
    makeIconButton(m_deleteReceiveButton, SstvGlyph::Trash, QStringLiteral("Delete received image"));
    receiveHistoryRow->addWidget(makeReceiveFieldLabel(QStringLiteral("RX HISTORY")));
    receiveHistoryRow->addWidget(m_receiveHistoryCombo, 1);
    receiveHistoryRow->addWidget(m_starReceiveButton);
    receiveHistoryRow->addWidget(m_shareReceiveButton);
    receiveHistoryRow->addWidget(m_deleteReceiveButton);
    receiveLayout->addLayout(receiveHistoryRow);
    auto *receiveSettingsRow = new QHBoxLayout;
    m_retentionCombo = new SstvInWindowComboBox(
        QStringLiteral("KEEP RECEIVED IMAGES"), receivePage);
    for (int limit : {10, 25, 50, 100})
        m_retentionCombo->addItem(QString::number(limit), limit);
    m_retentionCombo->addItem(QStringLiteral("UNLIMITED"), 0);
    m_clearReceiveHistoryButton = new QPushButton(QStringLiteral("CLEAR RX HISTORY"), receivePage);
    m_clearReceiveHistoryButton->setStyleSheet(buttonStyle(QStringLiteral("#6dd4ef")));
    // REPLY and CLEAR RX HISTORY are the paired station/history actions. Keep
    // their touch targets identical while allowing the RX CALL editor to give
    // up the small amount of horizontal space needed by the wider action.
    const int receiveActionWidth = qMax(m_replyReceiveButton->sizeHint().width(),
                                        m_clearReceiveHistoryButton->sizeHint().width());
    m_replyReceiveButton->setFixedWidth(receiveActionWidth);
    m_clearReceiveHistoryButton->setFixedWidth(receiveActionWidth);
    receiveSettingsRow->addWidget(makeReceiveFieldLabel(QStringLiteral("KEEP")));
    receiveSettingsRow->addWidget(m_retentionCombo);
    receiveSettingsRow->addStretch();
    auto *rxLog = new QPushButton(QStringLiteral("Log QSO"), receivePage);
    rxLog->setObjectName("sstvRxLogQso");
    rxLog->setStyleSheet(buttonStyle(QStringLiteral("#6dd4ef")));
    rxLog->setFixedHeight(28);
    receiveSettingsRow->addWidget(rxLog);
    connect(rxLog, &QPushButton::clicked, this, [this] { requestLogQso(false); });
    receiveSettingsRow->addWidget(m_clearReceiveHistoryButton);
    receiveLayout->addLayout(receiveSettingsRow);
    connect(m_receiveHistoryCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SstvScreen::selectReceiveHistory);
    connect(m_starReceiveButton, &QPushButton::clicked, this, &SstvScreen::toggleCurrentReceiveStar);
    connect(m_shareReceiveButton, &QPushButton::clicked, this, &SstvScreen::shareCurrentReceive);
    connect(m_deleteReceiveButton, &QPushButton::clicked, this, &SstvScreen::deleteCurrentReceive);
    connect(m_clearReceiveHistoryButton, &QPushButton::clicked, this, &SstvScreen::clearReceiveHistory);
    QSettings sstvSettings(QStringLiteral("QK4"), QStringLiteral("QK4"));
    const bool hadSstvPreferences = sstvSettings.contains(QStringLiteral("sstv/cwIdEnabled"))
        || sstvSettings.contains(QStringLiteral("sstv/operatorCallsign"))
        || sstvSettings.contains(QStringLiteral("sstv/txMode"))
        || sstvSettings.contains(QStringLiteral("sstv/rxRetention"));
    if (!sstvSettings.contains(QStringLiteral("sstv/fskIdEnabled"))) {
        // Fresh installs start with both interoperable IDs enabled. Existing
        // SSTV users retain their CW behavior and explicitly opt into the new
        // on-air FSK identifier.
        sstvSettings.setValue(QStringLiteral("sstv/fskIdEnabled"), !hadSstvPreferences);
    }
    const int retentionLimit = sstvSettings.value(QStringLiteral("sstv/rxRetention"), 50).toInt();
    m_storage.setRetentionLimit(retentionLimit);
    const int retentionIndex = m_retentionCombo->findData(retentionLimit);
    m_retentionCombo->setCurrentIndex(retentionIndex >= 0 ? retentionIndex : 2);
    connect(m_retentionCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        const int limit = m_retentionCombo->currentData().toInt();
        QSettings settings(QStringLiteral("QK4"), QStringLiteral("QK4"));
        settings.setValue(QStringLiteral("sstv/rxRetention"), limit);
        QString error;
        if (!m_storage.applyRetentionLimit(limit, &error))
            m_receiveStatus->setText(QStringLiteral("RX STORAGE WARNING • %1").arg(error));
        refreshReceiveHistory(m_currentReceiveId);
    });
    m_pages->addWidget(receivePage);

    auto *transmitPage = new QWidget(m_pages);
    transmitPage->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_transmitLayout = new QBoxLayout(QBoxLayout::LeftToRight, transmitPage);
    auto *canvasColumn = new QVBoxLayout;
    m_txFrameLabel = new QLabel(QStringLiteral("TX FRAME • SELECT MODE"), transmitPage);
    m_txFrameLabel->setStyleSheet(
        QStringLiteral("color: #f2ad20; font-size: 13px; font-weight: 700;"));
    m_txFrameLabel->setAlignment(Qt::AlignCenter);
    m_txFrameLabel->setAccessibleName(QStringLiteral("Exact SSTV transmit frame"));
    canvasColumn->addWidget(m_txFrameLabel);
    m_composer = new SstvComposerCanvas(transmitPage);
    canvasColumn->addWidget(m_composer, 1);
    m_txStateLabel = new QLabel(QStringLiteral("PREPARE AN IMAGE"), transmitPage);
    m_txStateLabel->setStyleSheet(QStringLiteral("color: #f2ad20; font-weight: bold;"));
    canvasColumn->addWidget(m_txStateLabel);
    m_txProgress = new QProgressBar(transmitPage);
    m_txProgress->setRange(0, 100);
    m_txProgress->setValue(0);
    m_txProgress->setTextVisible(false);
    m_txProgress->setFixedHeight(8);
    m_txProgress->setStyleSheet(QStringLiteral(
        "QProgressBar { background: #25292b; border: 0; border-radius: 4px; }"
        "QProgressBar::chunk { background: #f2ad20; border-radius: 4px; }"));
    canvasColumn->addWidget(m_txProgress);
    auto *txLog = new QPushButton(QStringLiteral("Log QSO"), transmitPage);
    txLog->setObjectName("sstvTxLogQso");
    txLog->setStyleSheet(buttonStyle(QStringLiteral("#6dd4ef")));
    txLog->setFixedHeight(28);
    canvasColumn->addWidget(txLog);
    connect(txLog, &QPushButton::clicked, this, [this] { requestLogQso(true); });
    m_transmitLayout->addLayout(canvasColumn, 3);

    // Preserve useful image area on a compact phone while keeping every TX
    // control reachable. Without a scroll viewport QBoxLayout compresses the
    // control rows until labels vanish and the send/stop buttons are clipped.
    auto *controlsScroll = new QScrollArea(transmitPage);
    m_txControlsScroll = controlsScroll;
    controlsScroll->setWidgetResizable(true);
    controlsScroll->setFrameShape(QFrame::NoFrame);
    controlsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    controlsScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    controlsScroll->setStyleSheet(QStringLiteral(
        "QScrollArea { background: #101314; border: 0; }"
        "QScrollArea > QWidget > QWidget { background: #101314; }"));
#ifdef Q_OS_ANDROID
    QScroller::grabGesture(controlsScroll->viewport(), QScroller::TouchGesture);
    if (QScroller *scroller = QScroller::scroller(controlsScroll->viewport())) {
        QScrollerProperties properties = scroller->scrollerProperties();
        // Match the proven phone control drawer: defer child presses while a
        // touch is still eligible to become a vertical scroll.
        properties.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.25);
        properties.setScrollMetric(QScrollerProperties::DragStartDistance, 0.0015);
        scroller->setScrollerProperties(properties);
        connect(scroller, &QScroller::stateChanged, controlsScroll,
                [this, controlsScroll](QScroller::State state) {
            if (state == QScroller::Inactive) {
                // The release that ends a drag can arrive at the child control
                // just before QScroller becomes inactive. Keep suppressing it
                // through the end of this event turn.
                QTimer::singleShot(120, this, [this]() {
                    m_txScrollGestureSuppressClick = false;
                });
                return;
            }
            if (state != QScroller::Dragging && state != QScroller::Scrolling)
                return;
            m_txScrollGestureSuppressClick = true;
            // A delayed button press may already be visually active when the
            // drag threshold is crossed. Cancel it exactly as the main QK4
            // phone drawer does. Do not call QComboBox::hidePopup() here:
            // Android implements the popup as another native OpenGL window,
            // and destroying it synchronously from QScroller::stateChanged()
            // can deadlock Qt's EGL-surface protector while the popup is
            // being painted. MousePressEventDelay already cancels a pending
            // combo tap when the gesture becomes a scroll.
            for (QAbstractButton *button : controlsScroll->findChildren<QAbstractButton *>())
                button->setDown(false);
        });
    }
#endif
    auto *controlsWidget = new QWidget(controlsScroll);
    auto *controls = new QVBoxLayout(controlsWidget);
    controls->setContentsMargins(4, 0, 4, 0);
    controls->setSpacing(6);
    controlsWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    controls->addWidget(new QLabel(QStringLiteral("TX MODE • DEFINES EXACT IMAGE"), transmitPage));
    m_modeCombo = new SstvInWindowComboBox(
        QStringLiteral("SELECT SSTV MODE"), transmitPage);
    for (const SstvModeSpec &mode : SstvModeRegistry::all()) {
        if (mode.encoderImplemented) {
            m_modeCombo->addItem(QStringLiteral("%1 • %2 × %3 • %4 s")
                                     .arg(mode.displayName).arg(mode.width).arg(mode.height)
                                     .arg(mode.durationMs / 1000.0, 0, 'f', 0),
                                 static_cast<int>(mode.id));
        }
    }
    const int savedMode = sstvSettings.value(QStringLiteral("sstv/txMode"),
                                              static_cast<int>(SstvModeId::ScottieS1)).toInt();
    const int savedModeIndex = m_modeCombo->findData(savedMode);
    m_modeCombo->setCurrentIndex(savedModeIndex >= 0 ? savedModeIndex : 0);
    connect(m_modeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int) {
        QSettings settings(QStringLiteral("QK4"), QStringLiteral("QK4"));
        settings.setValue(QStringLiteral("sstv/txMode"), m_modeCombo->currentData());
        resetFraming();
    });
    controls->addWidget(m_modeCombo);
    m_fitBars = sstvSettings.value(QStringLiteral("sstv/fitBars"), false).toBool();
    m_fitModeButton = new QPushButton(m_fitBars ? QStringLiteral("FIT / BARS")
                                               : QStringLiteral("FILL / CROP"), transmitPage);
    m_fitModeButton->setStyleSheet(buttonStyle(QStringLiteral("#6dd4ef")));
    connect(m_fitModeButton, &QPushButton::clicked, this, [this]() {
        m_fitBars = !m_fitBars;
        QSettings settings(QStringLiteral("QK4"), QStringLiteral("QK4"));
        settings.setValue(QStringLiteral("sstv/fitBars"), m_fitBars);
        m_fitModeButton->setText(m_fitBars ? QStringLiteral("FIT / BARS") : QStringLiteral("FILL / CROP"));
        resetFraming();
    });
    controls->addWidget(m_fitModeButton);
    m_modeDetail = new QLabel(transmitPage);
    m_modeDetail->setWordWrap(true);
    m_modeDetail->setStyleSheet(QStringLiteral("color: #6dd4ef; font-weight: 700;"));
    controls->addWidget(m_modeDetail);

    m_callsignRowLayout = new QHBoxLayout;
    m_callsignRowLayout->setSpacing(4);
    m_callsignIdRowContainer = new QWidget(transmitPage);
    m_callsignIdRowLayout = new QHBoxLayout(m_callsignIdRowContainer);
    m_callsignIdRowLayout->setContentsMargins(0, 0, 0, 0);
    m_callsignIdRowLayout->setSpacing(4);
    m_operatorCallsign = sstvSettings.value(QStringLiteral("sstv/operatorCallsign"))
                             .toString().trimmed().toUpper();
    m_callsignEdit = new QLineEdit(m_operatorCallsign, transmitPage);
    m_callsignEdit->setPlaceholderText(QStringLiteral("SET CALLSIGN"));
    m_callsignEdit->setMaxLength(16);
    // Reserve enough visible text width for a representative nine-character
    // portable callsign plus the edit padding.
    const int callsignWidth = QFontMetrics(m_callsignEdit->font())
                                  .horizontalAdvance(QStringLiteral("W9WDX/ABC")) + 22;
    m_callsignEdit->setFixedWidth(callsignWidth);
    m_callsignEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_callsignEdit->setAccessibleName(QStringLiteral("SSTV operator callsign"));
    m_callsignRowLayout->addWidget(m_callsignEdit, 1);
    m_fskIdCheck = new QCheckBox(QStringLiteral("FSK ID"), transmitPage);
    m_fskIdCheck->setChecked(sstvSettings.value(QStringLiteral("sstv/fskIdEnabled")).toBool());
    m_fskIdCheck->setToolTip(QStringLiteral("Send the MMSSTV-compatible FSK callsign ID after the image"));
    m_fskIdCheck->setAccessibleName(QStringLiteral("Send callsign as FSK ID after SSTV image"));
    m_cwIdCheck = new QCheckBox(QStringLiteral("CW ID"), transmitPage);
    m_cwIdCheck->setChecked(sstvSettings.value(QStringLiteral("sstv/cwIdEnabled"), true).toBool());
    m_cwIdCheck->setToolTip(QStringLiteral("Send My Call as a 700 Hz Morse audio ID after the SSTV image"));
    m_cwIdCheck->setAccessibleName(QStringLiteral("Send callsign in CW after SSTV image"));
    // Keep the entire label as the touch target while making the visual check
    // indicator proportionate to this dense single-line control row.
    const QString compactCheckStyle = QStringLiteral(
        "QCheckBox { spacing: 4px; }"
        "QCheckBox::indicator { width: 16px; height: 16px; }");
    m_fskIdCheck->setStyleSheet(compactCheckStyle);
    m_cwIdCheck->setStyleSheet(compactCheckStyle);
    m_cwWpmSpin = new QSpinBox(transmitPage);
    m_cwWpmSpin->setRange(5, 40);
    m_cwWpmSpin->setValue(qBound(5, sstvSettings.value(QStringLiteral("sstv/cwIdWpm"), 20).toInt(), 40));
    m_cwWpmSpin->setSuffix(QStringLiteral(" WPM"));
    // Android's native compact spin-box steppers render as clipped slivers at
    // this density while still keeping active (and overly sensitive) hit
    // targets. Use an unambiguous one-step button on each side instead.
    m_cwWpmSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_cwWpmSpin->setAlignment(Qt::AlignCenter);
    m_cwWpmSpin->setFixedWidth(62);
    m_cwWpmSpin->setAccessibleName(QStringLiteral("CW callsign speed"));
    m_cwWpmMinus = new QPushButton(QStringLiteral("−"), transmitPage);
    m_cwWpmPlus = new QPushButton(QStringLiteral("+"), transmitPage);
    for (QPushButton *button : {m_cwWpmMinus, m_cwWpmPlus}) {
        button->setFixedSize(26, 30);
        button->setStyleSheet(QStringLiteral(
            "QPushButton { background: #202426; color: #f0f0f0; border: 1px solid #6dd4ef; "
            "border-radius: 5px; padding: 0; font-size: 14px; font-weight: 700; }"
            "QPushButton:pressed { background: #245463; }"
            "QPushButton:disabled { color: #777; border-color: #555; }"));
    }
    m_cwWpmMinus->setAccessibleName(QStringLiteral("Decrease CW callsign speed by one WPM"));
    m_cwWpmPlus->setAccessibleName(QStringLiteral("Increase CW callsign speed by one WPM"));
    m_cwWpmSpin->setVisible(m_cwIdCheck->isChecked());
    m_cwWpmMinus->setVisible(m_cwIdCheck->isChecked());
    m_cwWpmPlus->setVisible(m_cwIdCheck->isChecked());
    controls->addWidget(new QLabel(QStringLiteral("MY CALL"), transmitPage));
    controls->addLayout(m_callsignRowLayout);
    controls->addWidget(m_callsignIdRowContainer);
    updateCallsignControlsLayout(height() > width());
    connect(m_fskIdCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        QSettings settings(QStringLiteral("QK4"), QStringLiteral("QK4"));
        settings.setValue(QStringLiteral("sstv/fskIdEnabled"), enabled);
        settings.sync();
        cancelTransmitConfirmation();
        updateTransmitUi();
    });
    connect(m_cwIdCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        QSettings settings(QStringLiteral("QK4"), QStringLiteral("QK4"));
        settings.setValue(QStringLiteral("sstv/cwIdEnabled"), enabled);
        settings.sync();
        m_cwWpmSpin->setVisible(enabled);
        m_cwWpmMinus->setVisible(enabled);
        m_cwWpmPlus->setVisible(enabled);
        cancelTransmitConfirmation();
        updateTransmitUi();
    });
    connect(m_cwWpmMinus, &QPushButton::clicked, this, [this]() {
        m_cwWpmSpin->setValue(m_cwWpmSpin->value() - 1);
    });
    connect(m_cwWpmPlus, &QPushButton::clicked, this, [this]() {
        m_cwWpmSpin->setValue(m_cwWpmSpin->value() + 1);
    });
    connect(m_cwWpmSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int wpm) {
        QSettings settings(QStringLiteral("QK4"), QStringLiteral("QK4"));
        settings.setValue(QStringLiteral("sstv/cwIdWpm"), wpm);
        settings.sync();
        const bool adjustable = !m_transmitting && !m_mediaRequestPending
                                && m_cwIdCheck->isChecked();
        m_cwWpmMinus->setEnabled(adjustable && wpm > m_cwWpmSpin->minimum());
        m_cwWpmPlus->setEnabled(adjustable && wpm < m_cwWpmSpin->maximum());
        cancelTransmitConfirmation();
    });
    connect(m_callsignEdit, &QLineEdit::editingFinished, this, [this]() {
        const QString candidate = m_callsignEdit->text().trimmed().toUpper();
        static const QRegularExpression validCallsign(
            QStringLiteral("^[A-Z0-9]+(?:/[A-Z0-9]+)*$"));
        if (!candidate.isEmpty()
            && (candidate.size() < 3 || !validCallsign.match(candidate).hasMatch())) {
            m_callsignEdit->setText(m_operatorCallsign);
            m_txStateLabel->setText(QStringLiteral("CALLSIGN MUST USE LETTERS, NUMBERS, OR /"));
            return;
        }
        if (candidate == m_operatorCallsign) {
            m_callsignEdit->setText(candidate);
            m_composer->setCallsignValues(candidate, m_replyCallsign);
            return;
        }
        m_operatorCallsign = candidate;
        m_callsignEdit->setText(m_operatorCallsign);
        QSettings settings(QStringLiteral("QK4"), QStringLiteral("QK4"));
        settings.setValue(QStringLiteral("sstv/operatorCallsign"), m_operatorCallsign);
        settings.sync();
        m_composer->setCallsignValues(m_operatorCallsign, m_replyCallsign);
        refreshTemplates();
        cancelTransmitConfirmation();
        m_txStateLabel->setText(m_operatorCallsign.isEmpty()
                                    ? QStringLiteral("SET MY CALL BEFORE USING A BUILT-IN TEMPLATE")
                                    : QStringLiteral("MY CALL SAVED • %1").arg(m_operatorCallsign));
    });
    auto *toCallRow = new QHBoxLayout;
    m_toCallsignEdit = new QLineEdit(transmitPage);
    m_toCallsignEdit->setObjectName("sstvToCall");
    m_toCallsignEdit->setPlaceholderText(QStringLiteral("OPTIONAL REPLY STATION"));
    m_toCallsignEdit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_toCallsignEdit->setMaxLength(16);
    m_toCallsignEdit->setFixedWidth(callsignWidth);
    m_toCallsignEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_toCallsignEdit->setText(sstvSettings.value(QStringLiteral("sstv/lastToCallsign")).toString());
    m_replyCallsign = m_toCallsignEdit->text().trimmed().toUpper();
    toCallRow->addWidget(m_toCallsignEdit, 0, Qt::AlignLeft | Qt::AlignVCenter);
    toCallRow->addStretch(1);
    controls->addWidget(new QLabel(QStringLiteral("TO CALL"), transmitPage));
    controls->addLayout(toCallRow);
    connect(m_toCallsignEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_replyCallsign = text.trimmed().toUpper();
        m_composer->setCallsignValues(m_operatorCallsign, m_replyCallsign);
        QSettings settings(QStringLiteral("QK4"), QStringLiteral("QK4"));
        settings.setValue(QStringLiteral("sstv/lastToCallsign"), m_replyCallsign);
    });
    m_composer->setCallsignValues(m_operatorCallsign, m_replyCallsign);
    m_galleryButton = new QPushButton(QStringLiteral("GALLERY"), transmitPage);
    m_galleryButton->setStyleSheet(buttonStyle(QStringLiteral("#f2ad20")));
    connect(m_galleryButton, &QPushButton::clicked, this, &SstvScreen::chooseImage);
    m_cameraButton = new QPushButton(transmitPage);
    m_cameraButton->setStyleSheet(buttonStyle(QStringLiteral("#f2ad20")));
    makeIconButton(m_cameraButton, SstvGlyph::Camera, QStringLiteral("Take a photo"));
    connect(m_cameraButton, &QPushButton::clicked, this, &SstvScreen::chooseCameraImage);
    m_rotateLeftButton = new QPushButton(transmitPage);
    m_rotateRightButton = new QPushButton(transmitPage);
    for (QPushButton *button : {m_rotateLeftButton, m_rotateRightButton})
        button->setStyleSheet(buttonStyle(QStringLiteral("#6dd4ef")));
    makeIconButton(m_rotateLeftButton, SstvGlyph::RotateLeft, QStringLiteral("Rotate image left"));
    makeIconButton(m_rotateRightButton, SstvGlyph::RotateRight, QStringLiteral("Rotate image right"));
    auto *imageRow = new QHBoxLayout;
    imageRow->addWidget(m_galleryButton, 1);
    imageRow->addWidget(m_cameraButton);
    imageRow->addWidget(m_rotateLeftButton);
    imageRow->addWidget(m_rotateRightButton);
    controls->addLayout(imageRow);
    controls->addWidget(new QLabel(QStringLiteral("IMAGE TEMPLATE GALLERY"), transmitPage));
    m_imageTemplateGalleryButton = new QPushButton(QStringLiteral("SSTV IMAGE GALLERY"), transmitPage);
    m_imageTemplateGalleryButton->setStyleSheet(buttonStyle(QStringLiteral("#f2ad20")));
    m_imageTemplateGalleryButton->setAccessibleName(QStringLiteral("Open saved SSTV image templates"));
    connect(m_imageTemplateGalleryButton, &QPushButton::clicked,
            this, &SstvScreen::openImageTemplateGallery);
    controls->addWidget(m_imageTemplateGalleryButton);
    connect(m_rotateLeftButton, &QPushButton::clicked, this, [this]() { rotateSource(-90); });
    connect(m_rotateRightButton, &QPushButton::clicked, this, [this]() { rotateSource(90); });

    auto *frameRow = new QHBoxLayout;
    m_frameZoomSlider = new QSlider(Qt::Horizontal, transmitPage);
    m_frameZoomSlider->setRange(100, 400);
    m_frameZoomSlider->setValue(100);
    m_frameZoomSlider->setMinimumHeight(32);
    m_frameZoomSlider->setStyleSheet(
        K4Styles::sliderHorizontal(K4Styles::Colors::DarkBackground,
                                   K4Styles::Colors::AccentAmber));
    m_frameZoomSlider->installEventFilter(this);
    m_frameZoomLabel = new QLabel(QStringLiteral("1.0×"), transmitPage);
    m_frameZoomLabel->setMinimumWidth(48);
    m_resetFrameButton = new QPushButton(QStringLiteral("CENTER"), transmitPage);
    m_resetFrameButton->setStyleSheet(buttonStyle(QStringLiteral("#6dd4ef")));
    frameRow->addWidget(new QLabel(QStringLiteral("ZOOM"), transmitPage));
    frameRow->addWidget(m_frameZoomSlider, 1);
    frameRow->addWidget(m_frameZoomLabel);
    frameRow->addWidget(m_resetFrameButton);
    controls->addLayout(frameRow);
    connect(m_frameZoomSlider, &QSlider::valueChanged, this, [this](int value) {
        m_frameZoom = value / 100.0;
        m_frameZoomLabel->setText(QStringLiteral("%1×").arg(m_frameZoom, 0, 'f', 1));
        cancelTransmitConfirmation();
        refreshModeFrame();
        scheduleDraftSave();
    });
    connect(m_resetFrameButton, &QPushButton::clicked, this, &SstvScreen::resetFraming);
    connect(m_composer, &SstvComposerCanvas::backgroundPanRequested,
            this, &SstvScreen::panFraming);
    connect(m_composer, &SstvComposerCanvas::backgroundZoomRequested,
            this, &SstvScreen::zoomFraming);

    controls->addWidget(new QLabel(QStringLiteral("IMAGE MARKUP"), transmitPage));
    auto *toolRow = new QHBoxLayout;
    m_selectToolButton = new QPushButton(transmitPage);
    m_drawToolButton = new QPushButton(transmitPage);
    m_shapeToolButton = new QPushButton(transmitPage);
    m_arrowToolButton = new QPushButton(transmitPage);
    m_rectangleToolButton = new QPushButton(transmitPage);
    m_ellipseToolButton = new QPushButton(transmitPage);
    m_textButton = new QPushButton(QStringLiteral("ADD TEXT"), transmitPage);
    m_textButton->setFixedWidth(82);
    m_myCallVariableButton = new QPushButton(QStringLiteral("MY CALL"), transmitPage);
    m_toCallVariableButton = new QPushButton(QStringLiteral("TO CALL"), transmitPage);
    m_colorButton = new QPushButton(transmitPage);
    m_fillColorButton = new QPushButton(transmitPage);
    for (QPushButton *button : {m_selectToolButton, m_drawToolButton, m_shapeToolButton,
                                m_arrowToolButton, m_rectangleToolButton, m_ellipseToolButton,
                                m_textButton, m_myCallVariableButton, m_toCallVariableButton,
                                m_colorButton, m_fillColorButton})
        button->setStyleSheet(buttonStyle(QStringLiteral("#6dd4ef")));
    makeIconButton(m_selectToolButton, SstvGlyph::Move, QStringLiteral("Move/select objects"));
    makeIconButton(m_drawToolButton, SstvGlyph::Draw, QStringLiteral("Freehand draw"));
    makeIconButton(m_shapeToolButton, SstvGlyph::Line, QStringLiteral("Draw line"));
    makeIconButton(m_arrowToolButton, SstvGlyph::Arrow, QStringLiteral("Draw arrow"));
    makeIconButton(m_rectangleToolButton, SstvGlyph::Rectangle, QStringLiteral("Draw rectangle"));
    makeIconButton(m_ellipseToolButton, SstvGlyph::Ellipse, QStringLiteral("Draw ellipse"));
    makeIconButton(m_colorButton, SstvGlyph::Color,
                   QStringLiteral("Choose the thin object outline color"),
                   m_composerColor);
    makeIconButton(m_fillColorButton, SstvGlyph::Color,
                   QStringLiteral("Choose text/line color or rectangle/ellipse fill color"),
                   m_composerFillColor);
    toolRow->addWidget(m_selectToolButton);
    toolRow->addWidget(m_drawToolButton);
    toolRow->addWidget(m_shapeToolButton);
    toolRow->addWidget(m_arrowToolButton);
    toolRow->addWidget(m_rectangleToolButton);
    toolRow->addWidget(m_ellipseToolButton);
    toolRow->addWidget(m_colorButton);
    toolRow->addWidget(m_fillColorButton);
    controls->addLayout(toolRow);
    auto *objectStyleRow = new QHBoxLayout;
    objectStyleRow->addWidget(m_textButton);
    objectStyleRow->addWidget(m_myCallVariableButton);
    objectStyleRow->addWidget(m_toCallVariableButton);
    objectStyleRow->addStretch(1);
    controls->addLayout(objectStyleRow);
    connect(m_selectToolButton, &QPushButton::clicked, this, [this]() {
        m_composer->setTool(SstvComposerCanvas::Tool::Select);
        updateComposerControls();
    });
    connect(m_drawToolButton, &QPushButton::clicked, this, [this]() {
        m_composer->setTool(SstvComposerCanvas::Tool::Draw);
        updateComposerControls();
    });
    const auto activateShapeTool = [this](SstvComposerCanvas::ShapeType type) {
        m_composer->setShapeType(type);
        m_composer->setTool(SstvComposerCanvas::Tool::Shape);
        updateComposerControls();
    };
    connect(m_shapeToolButton, &QPushButton::clicked, this,
            [activateShapeTool]() { activateShapeTool(SstvComposerCanvas::ShapeType::Line); });
    connect(m_arrowToolButton, &QPushButton::clicked, this,
            [activateShapeTool]() { activateShapeTool(SstvComposerCanvas::ShapeType::Arrow); });
    connect(m_rectangleToolButton, &QPushButton::clicked, this,
            [activateShapeTool]() { activateShapeTool(SstvComposerCanvas::ShapeType::Rectangle); });
    connect(m_ellipseToolButton, &QPushButton::clicked, this,
            [activateShapeTool]() { activateShapeTool(SstvComposerCanvas::ShapeType::Ellipse); });
    connect(m_textButton, &QPushButton::clicked, this, &SstvScreen::addOrEditText);
    connect(m_myCallVariableButton, &QPushButton::clicked, this, [this]() {
        addVariableText(SstvComposerCanvas::myCallToken());
    });
    connect(m_toCallVariableButton, &QPushButton::clicked, this, [this]() {
        addVariableText(SstvComposerCanvas::toCallToken());
    });
    connect(m_colorButton, &QPushButton::clicked, this, &SstvScreen::chooseInkColor);
    connect(m_fillColorButton, &QPushButton::clicked, this, &SstvScreen::chooseFillColor);
    m_composer->setInk(m_composerColor, 4);
    m_composer->setFillColor(m_composerFillColor);

    auto *typeRow = new QHBoxLayout;
    m_fontCombo = new SstvInWindowComboBox(
        QStringLiteral("SELECT FONT"), transmitPage);
    const auto addComposerFont = [this](const QString &label, const QString &family,
                                        QFont::Weight weight = QFont::Normal,
                                        bool italic = false, int stretch = QFont::Unstretched) {
        const QVariantMap descriptor{
            {QStringLiteral("family"), family},
            {QStringLiteral("weight"), static_cast<int>(weight)},
            {QStringLiteral("italic"), italic},
            {QStringLiteral("stretch"), stretch}
        };
        m_fontCombo->addItem(label, descriptor);
        QFont preview(family);
        preview.setWeight(weight);
        preview.setItalic(italic);
        preview.setStretch(stretch);
        m_fontCombo->setItemData(m_fontCombo->count() - 1, preview, Qt::FontRole);
    };
    addComposerFont(QStringLiteral("SANS"), QStringLiteral("Sans Serif"));
    addComposerFont(QStringLiteral("SANS LIGHT"), QStringLiteral("Sans Serif"), QFont::Light);
    addComposerFont(QStringLiteral("SANS BOLD"), QStringLiteral("Sans Serif"), QFont::Bold);
    addComposerFont(QStringLiteral("SANS BLACK"), QStringLiteral("Sans Serif"), QFont::Black);
    addComposerFont(QStringLiteral("CONDENSED"), QStringLiteral("Sans Serif"), QFont::Normal,
                    false, QFont::Condensed);
    addComposerFont(QStringLiteral("COND BOLD"), QStringLiteral("Sans Serif"), QFont::Bold,
                    false, QFont::Condensed);
    addComposerFont(QStringLiteral("WIDE"), QStringLiteral("Sans Serif"), QFont::Normal,
                    false, QFont::Expanded);
    addComposerFont(QStringLiteral("SERIF"), QStringLiteral("Serif"));
    addComposerFont(QStringLiteral("SERIF BOLD"), QStringLiteral("Serif"), QFont::Bold);
    addComposerFont(QStringLiteral("MONO"), QStringLiteral("Monospace"));
    addComposerFont(QStringLiteral("MONO BOLD"), QStringLiteral("Monospace"), QFont::Bold);
    addComposerFont(QStringLiteral("ITALIC"), QStringLiteral("Sans Serif"), QFont::Normal, true);
    addComposerFont(QStringLiteral("BOLD ITALIC"), QStringLiteral("Sans Serif"), QFont::Bold, true);
    addComposerFont(QStringLiteral("CURSIVE"), QStringLiteral("Cursive"));
    addComposerFont(QStringLiteral("INTER"), QStringLiteral("Inter"));
    // Add distinctive faces only when Android actually exposes them. The
    // reference Samsung includes these system fonts; conditional discovery
    // prevents other phones from showing several labels that all fall back to
    // the same generic face.
    const QStringList installedFamilies = QFontDatabase::families();
    const auto addInstalledFont = [this, &addComposerFont, &installedFamilies](
                                      const QString &label,
                                      const QStringList &candidateFamilies,
                                      QFont::Weight weight = QFont::Normal,
                                      bool italic = false) {
        for (const QString &candidate : candidateFamilies) {
            const auto match = std::find_if(installedFamilies.cbegin(), installedFamilies.cend(),
                                            [&candidate](const QString &family) {
                return family.compare(candidate, Qt::CaseInsensitive) == 0;
            });
            if (match != installedFamilies.cend()) {
                addComposerFont(label, *match, weight, italic);
                return;
            }
        }
    };
    addInstalledFont(QStringLiteral("ONE UI"),
                     {QStringLiteral("One UI Sans"), QStringLiteral("OneUISans")});
    addInstalledFont(QStringLiteral("ROBOTO FLEX"), {QStringLiteral("Roboto Flex")});
    addInstalledFont(QStringLiteral("SOURCE SANS"), {QStringLiteral("Source Sans Pro")});
    addInstalledFont(QStringLiteral("NOTO SERIF"), {QStringLiteral("Noto Serif")});
    addInstalledFont(QStringLiteral("SMALL CAPS"), {QStringLiteral("Carrois Gothic SC")});
    addInstalledFont(QStringLiteral("HANDWRITING"), {QStringLiteral("Coming Soon")});
    addInstalledFont(QStringLiteral("SCRIPT"), {QStringLiteral("Dancing Script")});
    addInstalledFont(QStringLiteral("TYPEWRITER"), {QStringLiteral("Cutive Mono")});
    addComposerFont(QStringLiteral("POSTER"), QStringLiteral("Sans Serif"), QFont::Black,
                    false, QFont::Condensed);
    addComposerFont(QStringLiteral("SERIF ITALIC"), QStringLiteral("Serif"),
                    QFont::Normal, true);
    addComposerFont(QStringLiteral("INTER BOLD"), QStringLiteral("Inter"), QFont::Bold);
    m_textSizeSlider = new QSlider(Qt::Horizontal, transmitPage);
    m_textSizeSlider->setRange(SstvTextSizeMinimumPx, SstvTextSizeMaximumPx);
    m_textSizeSlider->setValue(SstvTextSizeDefaultPx);
    m_textSizeSlider->setMinimumHeight(32);
    m_textSizeSlider->setStyleSheet(
        K4Styles::sliderHorizontal(K4Styles::Colors::DarkBackground,
                                   K4Styles::Colors::VfoACyan));
    m_textSizeSlider->installEventFilter(this);
    m_fontCombo->setMinimumWidth(125);
    m_fontCombo->setMaximumWidth(180);
    typeRow->addWidget(m_fontCombo, 1);
    typeRow->addWidget(m_textSizeSlider, 3);
    controls->addLayout(typeRow);
    auto *historyRow = new QHBoxLayout;
    m_rotateObjectLeftButton = new QPushButton(transmitPage);
    m_rotateObjectRightButton = new QPushButton(transmitPage);
    m_undoButton = new QPushButton(transmitPage);
    m_redoButton = new QPushButton(transmitPage);
    m_deleteObjectButton = new QPushButton(transmitPage);
    m_resetCompositionButton = new QPushButton(QStringLiteral("RESET"), transmitPage);
    for (QPushButton *button : {m_rotateObjectLeftButton, m_rotateObjectRightButton,
                                m_undoButton, m_redoButton, m_deleteObjectButton,
                                m_resetCompositionButton})
        button->setStyleSheet(buttonStyle(QStringLiteral("#6dd4ef")));
    makeIconButton(m_rotateObjectLeftButton, SstvGlyph::RotateLeft,
                   QStringLiteral("Rotate selected object left 45 degrees"));
    makeIconButton(m_rotateObjectRightButton, SstvGlyph::RotateRight,
                   QStringLiteral("Rotate selected object right 45 degrees"));
    makeIconButton(m_undoButton, SstvGlyph::Undo, QStringLiteral("Undo last edit"));
    makeIconButton(m_redoButton, SstvGlyph::Redo, QStringLiteral("Redo last edit"));
    makeIconButton(m_deleteObjectButton, SstvGlyph::Trash, QStringLiteral("Delete selected object"));
    historyRow->addWidget(m_rotateObjectRightButton);
    historyRow->addWidget(m_rotateObjectLeftButton);
    historyRow->addWidget(m_undoButton);
    historyRow->addWidget(m_redoButton);
    historyRow->addWidget(m_resetCompositionButton);
    historyRow->addWidget(m_deleteObjectButton);
    controls->addLayout(historyRow);
    connect(m_rotateObjectLeftButton, &QPushButton::clicked, m_composer,
            [this]() { m_composer->rotateSelectedObject(-45); });
    connect(m_rotateObjectRightButton, &QPushButton::clicked, m_composer,
            [this]() { m_composer->rotateSelectedObject(45); });
    connect(m_undoButton, &QPushButton::clicked, m_composer, &SstvComposerCanvas::undo);
    connect(m_redoButton, &QPushButton::clicked, m_composer, &SstvComposerCanvas::redo);
    connect(m_deleteObjectButton, &QPushButton::clicked,
            m_composer, &SstvComposerCanvas::deleteSelectedObject);
    connect(m_resetCompositionButton, &QPushButton::clicked, this, [this]() {
        if (askSstvQuestion(this, QStringLiteral("Reset composition"),
                              QStringLiteral("Remove all added text and drawing?"),
                              QStringLiteral("RESET")))
            m_composer->resetComposition();
    });
    connect(m_fontCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        m_composer->setInk(m_composerColor, qMax(2, m_textSizeSlider->value() / 5));
        m_composer->updateSelectedTextFont(composerFontFromControls());
    });
    connect(m_textSizeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_composer->setInk(m_composerColor, qMax(2, value / 5));
        m_composer->updateSelectedSize(value);
    });
    connect(m_composer, &SstvComposerCanvas::selectionChanged, this,
            [this](bool) {
        syncTextControlsFromSelection();
        updateComposerControls();
    });
    connect(m_composer, &SstvComposerCanvas::compositionChanged, this, [this]() {
        cancelTransmitConfirmation();
        updateComposerControls();
        scheduleDraftSave();
    });

    auto *templateRow = new QHBoxLayout;
    m_templateCombo = new SstvInWindowComboBox(
        QStringLiteral("SELECT TEMPLATE"), transmitPage);
    m_saveTemplateButton = new QPushButton(transmitPage);
    m_editTemplateButton = new QPushButton(transmitPage);
    m_deleteTemplateButton = new QPushButton(transmitPage);
    for (QPushButton *button : {m_saveTemplateButton, m_editTemplateButton,
                                m_deleteTemplateButton})
        button->setStyleSheet(buttonStyle(QStringLiteral("#6dd4ef")));
    makeIconButton(m_saveTemplateButton, SstvGlyph::Save, QStringLiteral("Save current layout as a new template"));
    makeIconButton(m_editTemplateButton, SstvGlyph::Edit, QStringLiteral("Edit selected user template"));
    makeIconButton(m_deleteTemplateButton, SstvGlyph::Trash, QStringLiteral("Delete template"));
    templateRow->addWidget(m_templateCombo, 1);
    templateRow->addWidget(m_saveTemplateButton);
    templateRow->addWidget(m_editTemplateButton);
    templateRow->addWidget(m_deleteTemplateButton);
    controls->addWidget(new QLabel(QStringLiteral("TEMPLATE"), transmitPage));
    controls->addLayout(templateRow);
    auto *recoveryRow = new QHBoxLayout;
    m_clearDraftButton = new QPushButton(QStringLiteral("CLEAR TX DRAFT"), transmitPage);
    m_resetTemplatesButton = new QPushButton(QStringLiteral("RESET TEMPLATES"), transmitPage);
    m_clearDraftButton->setStyleSheet(buttonStyle(QStringLiteral("#6dd4ef")));
    m_resetTemplatesButton->setStyleSheet(buttonStyle(QStringLiteral("#6dd4ef")));
    recoveryRow->addWidget(m_clearDraftButton);
    recoveryRow->addWidget(m_resetTemplatesButton);
    controls->addLayout(recoveryRow);
    connect(m_saveTemplateButton, &QPushButton::clicked, this, &SstvScreen::saveUserTemplate);
    connect(m_editTemplateButton, &QPushButton::clicked, this, &SstvScreen::editSelectedTemplate);
    connect(m_deleteTemplateButton, &QPushButton::clicked, this, &SstvScreen::deleteUserTemplate);
    connect(m_resetTemplatesButton, &QPushButton::clicked, this, &SstvScreen::resetUserTemplates);
    connect(m_clearDraftButton, &QPushButton::clicked, this, &SstvScreen::clearDraft);
    connect(m_templateCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        updateTemplateActionUi();
    });
    connect(m_templateCombo, qOverload<int>(&QComboBox::activated), this, [this](int) {
        applySelectedTemplate();
    });

    controls->addWidget(new QLabel(QStringLiteral("TX POWER • SYNCED"), transmitPage));
    auto *audioSetup = new QPushButton("CALIBRATE TX AUDIO / PROTECTION", transmitPage);
    audioSetup->setObjectName("sstvAudioSetup");
    audioSetup->setStyleSheet(buttonStyle(QStringLiteral("#6dd4ef")));
    controls->addWidget(audioSetup);
    connect(audioSetup, &QPushButton::clicked, this, &SstvScreen::audioSetupRequested);
    auto *powerRow = new QHBoxLayout;
    auto *minus = new QPushButton(QStringLiteral("−"), transmitPage);
    auto *plus = new QPushButton(QStringLiteral("+"), transmitPage);
    m_powerSlider = new QSlider(Qt::Horizontal, transmitPage);
    m_powerMinus = minus;
    m_powerPlus = plus;
    m_powerSlider->setRange(1, 110);
    m_powerSlider->setValue(25);
    m_powerSlider->setMinimumHeight(32);
    m_powerSlider->setStyleSheet(
        K4Styles::sliderHorizontal(K4Styles::Colors::DarkBackground,
                                   K4Styles::Colors::AccentAmber));
    m_powerSlider->installEventFilter(this);
    m_powerSlider->setMinimumWidth(140);
    m_powerSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_powerLabel = new QLabel(transmitPage);
    m_powerLabel->setMinimumWidth(46);
    for (QPushButton *button : {minus, plus}) {
        button->setFixedSize(30, 28);
        button->setStyleSheet(QStringLiteral(
            "QPushButton { background: #202426; color: #f0f0f0; border: 1px solid #6dd4ef;"
            " border-radius: 5px; padding: 0; font-size: 16px; font-weight: bold; }"
            "QPushButton:disabled { color: #777; border-color: #555; }"));
    }
    minus->setAccessibleName(QStringLiteral("Decrease SSTV transmit power"));
    plus->setAccessibleName(QStringLiteral("Increase SSTV transmit power"));
    powerRow->addWidget(minus);
    powerRow->addWidget(m_powerLabel);
    powerRow->addWidget(m_powerSlider, 1);
    powerRow->addWidget(plus);
    controls->addLayout(powerRow);
    connect(minus, &QPushButton::clicked, m_powerSlider, [this]() { m_powerSlider->setValue(qMax(1, m_powerSlider->value() - 1)); });
    connect(plus, &QPushButton::clicked, m_powerSlider, [this]() { m_powerSlider->setValue(qMin(110, m_powerSlider->value() + 1)); });
    connect(m_powerSlider, &QSlider::valueChanged, this, [this](int watts) {
        m_powerWatts = watts;
        cancelTransmitConfirmation();
        updatePowerUi();
        emit powerRequested(m_powerWatts);
    });

    m_transmitButton = new QPushButton(QStringLiteral("PREVIEW & SEND"), transmitPage);
    m_transmitButton->setStyleSheet(buttonStyle(QStringLiteral("#f2ad20"), true));
    connect(m_transmitButton, &QPushButton::clicked, this, [this]() {
        if (m_modeFrame.isNull())
            return;
        if (!m_transmitConfirmationPending) {
            QString unresolvedVariable;
            if (m_composer->hasUnresolvedVariables(&unresolvedVariable)) {
                if (unresolvedVariable.compare(SstvComposerCanvas::myCallToken(),
                                               Qt::CaseInsensitive) == 0) {
                    m_txStateLabel->setText(
                        QStringLiteral("ENTER MY CALL BEFORE TRANSMITTING THIS TEMPLATE"));
                    m_callsignEdit->setFocus();
                } else {
                    m_txStateLabel->setText(
                        QStringLiteral("ENTER TO CALL BEFORE TRANSMITTING THIS TEMPLATE"));
                    m_toCallsignEdit->setFocus();
                }
                return;
            }
            const SstvModeSpec *mode = SstvModeRegistry::find(
                static_cast<SstvModeId>(m_modeCombo->currentData().toInt()));
            m_frozenTransmitFrame = m_composer->renderedImage();
            if (!mode || m_frozenTransmitFrame.size() != QSize(mode->width, mode->height)) {
                m_frozenTransmitFrame = QImage();
                m_txStateLabel->setText(QStringLiteral("TX FRAME ERROR • SELECT MODE AGAIN"));
                updateTransmitUi();
                return;
            }
            // This is a pixel-for-pixel freeze only. Source framing and mode
            // resizing already happened on the live editor canvas.
            m_transmitConfirmationPending = true;
            const QString cwDetail = sendCallsignCw()
                ? QStringLiteral(" • CW %1 @ %2 WPM").arg(m_operatorCallsign).arg(callsignCwWpm())
                : QString();
            const QString fskDetail = sendCallsignFsk()
                ? QStringLiteral(" • FSK ID %1").arg(m_operatorCallsign) : QString();
            m_txStateLabel->setText(QStringLiteral("READY TO KEY K4 • %1 • %2 W%3%4 • TAP TRANSMIT SSTV")
                                        .arg(mode ? mode->displayName : QStringLiteral("SSTV"))
                                        .arg(m_powerWatts, 0, 'f', m_powerWatts <= 10.0 ? 1 : 0)
                                        .arg(fskDetail)
                                        .arg(cwDetail));
            updateTransmitUi();
        } else {
            emit transmitRequested(m_frozenTransmitFrame, m_modeCombo->currentData().toInt());
        }
    });
    m_stopButton = new QPushButton(QStringLiteral("STOP SSTV"), transmitPage);
    m_stopButton->setStyleSheet(buttonStyle(QStringLiteral("#e3262e"), true));
    connect(m_stopButton, &QPushButton::pressed, this, &SstvScreen::stopRequested);
    controls->addStretch();
    controls->addWidget(m_transmitButton);
    controls->addWidget(m_stopButton);
    // QScroller owns vertical drags, but a child button can still receive the
    // release that ends the drag. Filtering the interactive children lets the
    // scroller's state flag suppress that release before it becomes a click.
    for (QWidget *child : controlsWidget->findChildren<QWidget *>()) {
        if (qobject_cast<QAbstractButton *>(child)
            || qobject_cast<QComboBox *>(child)
            || qobject_cast<QLineEdit *>(child)
            || qobject_cast<QSpinBox *>(child))
            child->installEventFilter(this);
    }
    controls->activate();
    // Constrain only the vertical dimension. The scroll area must be allowed
    // to shrink the pane horizontally to the phone viewport.
    controlsWidget->setMinimumHeight(controls->sizeHint().height());
    controlsScroll->setWidget(controlsWidget);
    m_transmitLayout->addWidget(controlsScroll, 2);
    m_pages->addWidget(transmitPage);
    root->addWidget(m_pages, 1);

    selectTab(false);
    updatePowerUi();
    updateComposerControls();
    refreshModeFrame();
    updateTransmitUi();
}

void SstvScreen::setRfPower(double watts) {
    if (m_transmitting || watts <= 0.0)
        return;
    const bool changed = !qFuzzyCompare(m_powerWatts + 1.0, watts + 1.0);
    m_powerWatts = watts;
    const QSignalBlocker blocker(m_powerSlider);
    m_powerSlider->setValue(qBound(1, qRound(watts), 110));
    updatePowerUi();
    if (changed)
        cancelTransmitConfirmation();
}

void SstvScreen::setTransmitProgress(int emittedSamples, int totalSamples, int imageSamples) {
    if (!m_transmitting || totalSamples <= 0)
        return;
    const auto showProgressStatus = [this](const QString &detail) {
        m_txStateLabel->setText(m_transmitWarning.isEmpty()
                                    ? detail : detail + QStringLiteral("\n") + m_transmitWarning);
    };
    const int percent = qBound(0, static_cast<int>((100LL * emittedSamples) / totalSamples), 100);
    m_txProgress->setValue(percent);
    if (imageSamples > 0 && totalSamples > imageSamples && emittedSamples >= imageSamples) {
        const int fskSamples = sendCallsignFsk()
            ? (522 + 132 * (m_operatorCallsign.size() + 3)) * SstvEncoder::SampleRate / 1000
            : 0;
        const int fskEnd = qMin(totalSamples, imageSamples + fskSamples);
        if (fskSamples > 0 && emittedSamples < fskEnd) {
            const int fskPercent = qBound(0, static_cast<int>(
                (100LL * (emittedSamples - imageSamples)) / fskSamples), 100);
            showProgressStatus(QStringLiteral("SENDING FSK ID • %1 • %2%")
                                   .arg(m_operatorCallsign).arg(fskPercent));
            return;
        }
        if (!sendCallsignCw()) {
            showProgressStatus(sendCallsignFsk()
                                   ? QStringLiteral("FSK ID COMPLETE • FINAL AUDIO BUFFER")
                                   : QStringLiteral("FINAL AUDIO BUFFER • HOLDING TX"));
            return;
        }
        const int cwStart = fskEnd;
        const int cwPercent = qBound(0, static_cast<int>(
            (100LL * (emittedSamples - cwStart)) / qMax(1, totalSamples - cwStart)), 100);
        showProgressStatus(QStringLiteral("SENDING CW ID • %1 • %2 WPM • %3%")
                               .arg(m_operatorCallsign)
                               .arg(callsignCwWpm())
                               .arg(cwPercent));
        return;
    }
    showProgressStatus(QStringLiteral("TRANSMITTING SSTV • %1% • %2 / %3 s")
                           .arg(percent)
                           .arg(emittedSamples / SstvEncoder::SampleRate)
                           .arg(totalSamples / SstvEncoder::SampleRate));
}

void SstvScreen::setTransmitting(bool active, const QString &detail) {
    m_transmitting = active;
    if (!active) {
        m_transmitConfirmationPending = false;
        m_frozenTransmitFrame = QImage();
    }
    const QString state = active ? (detail.isEmpty() ? QStringLiteral("TRANSMITTING SSTV") : detail)
                                 : QStringLiteral("PREPARE AN IMAGE");
    m_txStateLabel->setText(active && !m_transmitWarning.isEmpty()
                                ? state + QStringLiteral("\n") + m_transmitWarning : state);
    m_txProgress->setValue(0);
    updateTransmitUi();
}

void SstvScreen::setTransmitStatus(const QString &detail) {
    m_txStateLabel->setText(m_transmitting && !m_transmitWarning.isEmpty()
                                ? detail + QStringLiteral("\n") + m_transmitWarning : detail);
}

void SstvScreen::setTransmitWarning(const QString &warning) {
    m_transmitWarning = warning;
    if (m_transmitting && !warning.isEmpty() && !m_txStateLabel->text().contains(warning))
        m_txStateLabel->setText(m_txStateLabel->text() + QStringLiteral("\n") + warning);
}

void SstvScreen::setRadioOperatingState(const QString &rxFrequency, const QString &rxMode,
                                        const QString &txFrequency, const QString &txMode) {
    const QString unknownFrequency = QStringLiteral("—.---.---");
    const QString unknownMode = QStringLiteral("—");
    const QString rxFrequencyText = rxFrequency.isEmpty() ? unknownFrequency : rxFrequency;
    const QString txFrequencyText = txFrequency.isEmpty() ? unknownFrequency : txFrequency;
    const QString rxModeText = rxMode.isEmpty() ? unknownMode : rxMode;
    const QString txModeText = txMode.isEmpty() ? unknownMode : txMode;
    m_rxRadioState->setText(QStringLiteral("RX %1  %2").arg(rxFrequencyText, rxModeText));
    m_txRadioState->setText(QStringLiteral("TX %1  %2").arg(txFrequencyText, txModeText));
    m_rxRadioState->setAccessibleName(
        QStringLiteral("Receive frequency %1, mode %2").arg(rxFrequencyText, rxModeText));
    m_txRadioState->setAccessibleName(
        QStringLiteral("Transmit frequency %1, mode %2").arg(txFrequencyText, txModeText));
}

void SstvScreen::returnToAutoReceive() {
    selectTab(false);
    m_receiveStatus->setText(QStringLiteral("AUTO RX ON • MODE AUTO • SYNC WAITING • SLANT AUTO"));
}

void SstvScreen::setReceiveStatus(const QString &status) {
    m_receiveStatus->setText(status);
}
void SstvScreen::setTransmitProtection(const QString &text, bool fault) {
    m_protectionLabel->setText(text);
    m_protectionLabel->setAccessibleName(text);
    m_protectionLabel->setStyleSheet(fault ? "color:#ff8e8e;font-weight:700;font-size:12px;"
                                         : "color:#9ee4af;font-size:12px;");
}

void SstvScreen::setReceiveInputLevel(int percent) {
    if (m_receiveLevel)
        m_receiveLevel->setValue(qBound(0, percent, 100));
}

void SstvScreen::setReceiveStreamActive(bool active) {
    if (!m_receiveStreamState)
        return;
    m_receiveStreamState->setText(active ? QStringLiteral("STREAM OK") : QStringLiteral("NO STREAM"));
    m_receiveStreamState->setStyleSheet(active
        ? QStringLiteral("color: #63df55; font-weight: bold;")
        : QStringLiteral("color: #ff786e; font-weight: bold;"));
}

void SstvScreen::setReceiveImage(const QImage &image, int completedRows, int totalRows,
                                 const QString &slantStatus) {
    if (image.isNull())
        return;
    m_receiveImage->setText(QString());
    m_receiveImage->setPixmap(QPixmap::fromImage(image).scaled(m_receiveImage->size(),
                                                  Qt::KeepAspectRatio,
                                                  Qt::SmoothTransformation));
    m_receiveStatus->setText(QStringLiteral("AUTO RX • %1 / %2 ROWS • %3")
                                 .arg(completedRows).arg(totalRows).arg(slantStatus));
}

void SstvScreen::completeReceiveImage(const QImage &image, int modeId, const QString &slantStatus,
                                      qint64 frequencyHz) {
    if (image.isNull())
        return;
    const SstvModeSpec *mode = SstvModeRegistry::find(static_cast<SstvModeId>(modeId));
    const QString modeName = mode ? mode->displayName : QStringLiteral("AUTO");
    SstvRxRecord record;
    QString error;
    if (!m_storage.saveReceived(image, modeId, modeName, slantStatus, frequencyHz, &record, &error)) {
        setReceiveImage(image, image.height(), image.height(), slantStatus);
        m_receiveStatus->setText(QStringLiteral("RX COMPLETE • SAVE FAILED • %1").arg(error));
        return;
    }
    m_currentReceiveImage = image;
    refreshReceiveHistory(record.id);
    m_pendingReceiveCallsignId = record.id;
    m_receiveCallsignEdit->clear();
    m_receiveCallsignSourceValue.clear();
    m_receiveCallsignConfidence = 0;
    m_receiveCallsignSource->setText(QStringLiteral("LISTENING"));
    m_replyReceiveButton->setEnabled(false);
    m_receiveStatus->setText(QStringLiteral("RX SAVED • %1 • %2").arg(modeName, slantStatus));
}

void SstvScreen::receiveCallsignDetected(const QString &callsign, const QString &source,
                                         int confidence) {
    if (m_pendingReceiveCallsignId.isEmpty() || callsign.trimmed().isEmpty())
        return;
    const bool showingPending = m_currentReceiveId == m_pendingReceiveCallsignId;
    const auto pending = std::find_if(m_receiveRecords.cbegin(), m_receiveRecords.cend(),
                                      [this](const SstvRxRecord &record) {
        return record.id == m_pendingReceiveCallsignId;
    });
    const QString existingSource = showingPending ? m_receiveCallsignSourceValue
        : (pending != m_receiveRecords.cend() ? pending->callsignSource : QString());
    const QString existingCall = showingPending ? m_receiveCallsignEdit->text().trimmed()
        : (pending != m_receiveRecords.cend() ? pending->callsign : QString());
    const QString normalizedSource = source.trimmed().toUpper();
    // An operator correction is authoritative. Otherwise checksum-validated
    // FSK ID replaces a tentative CW result, while CW never displaces FSK.
    if (existingSource == QStringLiteral("MANUAL")
        || (existingSource == QStringLiteral("FSK ID")
            && normalizedSource != QStringLiteral("FSK ID")))
        return;
    if (!existingCall.isEmpty()
        && normalizedSource == QStringLiteral("CW ID")
        && existingSource != QStringLiteral("CW ID"))
        return;
    QString error;
    if (!m_storage.setCallsign(m_pendingReceiveCallsignId, callsign, normalizedSource,
                               confidence, &error)) {
        m_receiveStatus->setText(QStringLiteral("RX CALL SAVE FAILED • %1").arg(error));
        return;
    }
    refreshReceiveHistory(showingPending ? m_pendingReceiveCallsignId : m_currentReceiveId);
}

void SstvScreen::refreshReceiveHistory(const QString &selectId) {
    QString error;
    m_receiveRecords = m_storage.received(&error);
    const QSignalBlocker blocker(m_receiveHistoryCombo);
    m_receiveHistoryCombo->clear();
    int selectedIndex = -1;
    for (int i = 0; i < m_receiveRecords.size(); ++i) {
        const SstvRxRecord &record = m_receiveRecords.at(i);
        const QString star = record.starred ? QStringLiteral("★ ") : QString();
        m_receiveHistoryCombo->addItem(
            QStringLiteral("%1%2 • %3").arg(star,
                record.receivedUtc.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                record.modeName), record.id);
        if (!selectId.isEmpty() && record.id == selectId)
            selectedIndex = i;
    }
    if (selectedIndex < 0 && !m_receiveRecords.isEmpty())
        selectedIndex = 0;
    m_receiveHistoryCombo->setCurrentIndex(selectedIndex);
    selectReceiveHistory(selectedIndex);
    const bool available = selectedIndex >= 0;
    m_starReceiveButton->setEnabled(available);
    m_shareReceiveButton->setEnabled(available);
    m_deleteReceiveButton->setEnabled(available);
    m_clearReceiveHistoryButton->setEnabled(!m_receiveRecords.isEmpty());
    if (!error.isEmpty())
        m_receiveStatus->setText(QStringLiteral("RX HISTORY WARNING • %1").arg(error));
}

void SstvScreen::selectReceiveHistory(int index) {
    if (index < 0 || index >= m_receiveRecords.size()) {
        m_currentReceiveId.clear();
        m_currentReceiveImage = QImage();
        m_starReceiveButton->setIcon(sstvGlyph(SstvGlyph::Star));
        m_starReceiveButton->setToolTip(QStringLiteral("Star received image"));
        m_starReceiveButton->setAccessibleName(QStringLiteral("Star received image"));
        m_receiveCallsignEdit->clear();
        m_receiveCallsignSource->setText(QStringLiteral("—"));
        m_replyReceiveButton->setEnabled(false);
        return;
    }
    const SstvRxRecord &record = m_receiveRecords.at(index);
    QImageReader reader(record.imagePath);
    const QImage image = reader.read();
    if (image.isNull()) {
        m_receiveStatus->setText(QStringLiteral("RX HISTORY ERROR • %1").arg(reader.errorString()));
        return;
    }
    m_currentReceiveId = record.id;
    m_currentReceiveImage = image;
    m_receiveImage->setText(QString());
    m_receiveImage->setPixmap(QPixmap::fromImage(image).scaled(m_receiveImage->size(),
                                                  Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_starReceiveButton->setIcon(sstvGlyph(SstvGlyph::Star,
        record.starred ? QColor(QStringLiteral("#f2ad20")) : QColor(QStringLiteral("#f0f0f0"))));
    const QString starAction = record.starred ? QStringLiteral("Unstar received image")
                                               : QStringLiteral("Star received image");
    m_starReceiveButton->setToolTip(starAction);
    m_starReceiveButton->setAccessibleName(starAction);
    m_receiveCallsignEdit->setText(record.callsign);
    m_receiveCallsignSourceValue = record.callsignSource;
    m_receiveCallsignConfidence = record.callsignConfidence;
    m_receiveCallsignSource->setText(record.callsignSource.isEmpty()
        ? QStringLiteral("—")
        : QStringLiteral("%1 • %2%").arg(record.callsignSource)
                                     .arg(record.callsignConfidence));
    m_receiveCallsignSource->setAccessibleName(record.callsignSource.isEmpty()
        ? QStringLiteral("Callsign source not available")
        : QStringLiteral("Callsign source %1, confidence %2 percent")
              .arg(record.callsignSource).arg(record.callsignConfidence));
    m_replyReceiveButton->setEnabled(!record.callsign.isEmpty());
    const QString frequency = record.frequencyHz > 0
        ? QStringLiteral(" • %1 MHz").arg(record.frequencyHz / 1000000.0, 0, 'f', 6)
        : QString();
    m_receiveStatus->setText(QStringLiteral("SAVED RX • %1%2 • %3")
                                 .arg(record.modeName, frequency, record.slantStatus));
}

void SstvScreen::saveCurrentReceiveCallsign(bool manual) {
    if (m_currentReceiveId.isEmpty())
        return;
    const QString callsign = m_receiveCallsignEdit->text().trimmed().toUpper();
    static const QRegularExpression validCallsign(QStringLiteral("^[A-Z0-9]+(?:/[A-Z0-9]+)*$"));
    if (!callsign.isEmpty()
        && (callsign.size() < 3 || !validCallsign.match(callsign).hasMatch())) {
        m_receiveStatus->setText(QStringLiteral("RX CALL MUST USE LETTERS, NUMBERS, OR /"));
        return;
    }
    if (manual) {
        m_receiveCallsignSourceValue = callsign.isEmpty() ? QString() : QStringLiteral("MANUAL");
        m_receiveCallsignConfidence = callsign.isEmpty() ? 0 : 100;
    }
    QString error;
    if (!m_storage.setCallsign(m_currentReceiveId, callsign,
                               m_receiveCallsignSourceValue, m_receiveCallsignConfidence, &error)) {
        m_receiveStatus->setText(QStringLiteral("RX CALL SAVE FAILED • %1").arg(error));
        return;
    }
    m_receiveCallsignEdit->setText(callsign);
    m_receiveCallsignSource->setText(m_receiveCallsignSourceValue.isEmpty()
        ? QStringLiteral("—")
        : QStringLiteral("%1 • %2%").arg(m_receiveCallsignSourceValue)
                                     .arg(m_receiveCallsignConfidence));
    m_replyReceiveButton->setEnabled(!callsign.isEmpty());
    refreshReceiveHistory(m_currentReceiveId);
}

void SstvScreen::replyToCurrentReceive() {
    const auto it = std::find_if(m_receiveRecords.cbegin(), m_receiveRecords.cend(),
                                 [this](const SstvRxRecord &record) {
        return record.id == m_currentReceiveId;
    });
    if (it == m_receiveRecords.cend() || it->callsign.isEmpty())
        return;
    const int modeIndex = m_modeCombo->findData(it->modeId);
    if (modeIndex >= 0)
        m_modeCombo->setCurrentIndex(modeIndex);
    m_replyCallsign = it->callsign;
    m_toCallsignEdit->setText(m_replyCallsign);
    const int reportIndex = m_templateCombo->findData(QStringLiteral("builtin:report"));
    if (reportIndex >= 0)
        m_templateCombo->setCurrentIndex(reportIndex);
    selectTab(true);
    m_txStateLabel->setText(QStringLiteral("REPLY TO %1 • %2 MODE PRELOADED • CHOOSE IMAGE OR APPLY TEMPLATE")
                                .arg(m_replyCallsign, it->modeName));
}

void SstvScreen::shareCurrentReceive() {
    const auto it = std::find_if(m_receiveRecords.cbegin(), m_receiveRecords.cend(),
                                 [this](const SstvRxRecord &record) { return record.id == m_currentReceiveId; });
    if (it == m_receiveRecords.cend())
        return;
#ifdef Q_OS_ANDROID
    QString error;
    if (!SstvMedia::shareImage(it->imagePath, QStringLiteral("Share SSTV image"), &error))
        m_receiveStatus->setText(QStringLiteral("SHARE FAILED • %1").arg(error));
#else
    const QString suggested = QDir(QDir::homePath()).filePath(QFileInfo(it->imagePath).fileName());
    const QString target = QFileDialog::getSaveFileName(this, QStringLiteral("Export SSTV image"),
                                                         suggested, QStringLiteral("PNG image (*.png)"));
    if (!target.isEmpty() && !m_currentReceiveImage.save(target, "PNG"))
        m_receiveStatus->setText(QStringLiteral("EXPORT FAILED"));
#endif
}

void SstvScreen::toggleCurrentReceiveStar() {
    const auto it = std::find_if(m_receiveRecords.cbegin(), m_receiveRecords.cend(),
                                 [this](const SstvRxRecord &record) { return record.id == m_currentReceiveId; });
    if (it == m_receiveRecords.cend())
        return;
    QString error;
    if (!m_storage.setStarred(it->id, !it->starred, &error)) {
        m_receiveStatus->setText(QStringLiteral("RX STORAGE ERROR • %1").arg(error));
        return;
    }
    refreshReceiveHistory(it->id);
}

void SstvScreen::deleteCurrentReceive() {
    if (m_currentReceiveId.isEmpty())
        return;
    if (QMessageBox::question(this, QStringLiteral("Delete received image"),
                              QStringLiteral("Delete this app-private SSTV image? Exported copies are not affected."))
        != QMessageBox::Yes)
        return;
    QString error;
    if (!m_storage.removeReceived(m_currentReceiveId, &error))
        m_receiveStatus->setText(QStringLiteral("DELETE FAILED • %1").arg(error));
    refreshReceiveHistory();
}

void SstvScreen::clearReceiveHistory() {
    if (!askSstvQuestion(this, QStringLiteral("Clear RX history"),
                         QStringLiteral("Delete every unstarred app-private RX image? Starred and exported images are preserved."),
                         QStringLiteral("CLEAR HISTORY")))
        return;
    QString error;
    if (!m_storage.clearUnstarred(&error))
        m_receiveStatus->setText(QStringLiteral("CLEAR FAILED • %1").arg(error));
    refreshReceiveHistory();
}

void SstvScreen::selectTab(bool transmit) {
    m_pages->setCurrentIndex(transmit ? 1 : 0);
    m_receiveTab->setStyleSheet(buttonStyle(QStringLiteral("#28bde8"), !transmit));
    m_transmitTab->setStyleSheet(buttonStyle(QStringLiteral("#f2ad20"), transmit));
}

void SstvScreen::chooseImage() {
#ifdef Q_OS_ANDROID
    QString error;
    if (SstvMedia::openGallery(&error)) {
        m_mediaRequestPending = true;
        m_txStateLabel->setText(QStringLiteral("OPENING ANDROID PHOTO PICKER"));
        updateTransmitUi();
        m_mediaPollTimer->start();
        return;
    }
    m_txStateLabel->setText(error);
#else
    const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("Choose SSTV image"), QString(),
                                                       QStringLiteral("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
    if (file.isEmpty())
        return;
    importImage(file);
#endif
}

void SstvScreen::chooseCameraImage() {
    QString error;
    if (SstvMedia::openCamera(&error)) {
        m_mediaRequestPending = true;
        m_txStateLabel->setText(QStringLiteral("OPENING ANDROID CAMERA"));
        updateTransmitUi();
        m_mediaPollTimer->start();
        return;
    }
    m_txStateLabel->setText(error);
}

void SstvScreen::pollImportedImage() {
    const QString path = SstvMedia::takeCompletedImagePath();
    if (path.isEmpty() && SstvMedia::isOperationActive())
        return;
    m_mediaPollTimer->stop();
    m_mediaRequestPending = false;
    updateTransmitUi();
    if (!path.isEmpty()) {
        importImage(path);
        return;
    }
    const QString error = SstvMedia::takeOperationError();
    m_txStateLabel->setText(error.isEmpty() ? QStringLiteral("IMAGE SELECTION CANCELLED") : error);
}

void SstvScreen::importImage(const QString &path) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QSize decodedSize = reader.size();
    if (decodedSize.isValid() && qMax(decodedSize.width(), decodedSize.height()) > 4096) {
        decodedSize.scale(QSize(4096, 4096), Qt::KeepAspectRatio);
        reader.setScaledSize(decodedSize);
    }
    const QImage image = reader.read();
#ifdef Q_OS_ANDROID
    // The bridge always returns an app-private cache copy. Once QImage owns
    // the pixels, remove the full-resolution picker/camera temporary.
    QFile::remove(path);
#endif
    if (image.isNull()) {
        m_txStateLabel->setText(QStringLiteral("IMAGE IMPORT FAILED • %1").arg(reader.errorString()));
        return;
    }
    // A gallery/camera selection starts a new composition. Clear both the
    // visible objects and their undo history so markup from the prior source
    // cannot be restored onto the replacement image.
    m_editingTemplateName.clear();
    updateTemplateActionUi();
    m_composer->clearCompositionForNewImage();
    m_sourceImage = image;
    m_draftSourceDirty = true;
    resetFraming();
}

void SstvScreen::rotateSource(int degrees) {
    if (m_transmitting || m_sourceImage.isNull())
        return;
    QTransform rotation;
    rotation.rotate(degrees);
    m_sourceImage = m_sourceImage.transformed(rotation, Qt::SmoothTransformation);
    m_draftSourceDirty = true;
    resetFraming();
}

void SstvScreen::resetFraming() {
    m_frameZoom = 1.0;
    m_frameCenter = QPointF(0.5, 0.5);
    if (m_frameZoomSlider) {
        const QSignalBlocker blocker(m_frameZoomSlider);
        m_frameZoomSlider->setValue(100);
    }
    if (m_frameZoomLabel)
        m_frameZoomLabel->setText(QStringLiteral("1.0×"));
    cancelTransmitConfirmation();
    refreshModeFrame();
    scheduleDraftSave();
}

void SstvScreen::panFraming(const QPointF &normalizedDelta) {
    if (m_sourceImage.isNull() || m_transmitting)
        return;
    const SstvModeSpec *mode = SstvModeRegistry::find(
        static_cast<SstvModeId>(m_modeCombo->currentData().toInt()));
    if (!mode)
        return;
    const QRectF crop = framingCrop(m_sourceImage.size(), QSize(mode->width, mode->height),
                                    m_fitBars, m_frameZoom, m_frameCenter);
    if (crop.isEmpty())
        return;
    const QPointF shiftedCenter = crop.center()
        - QPointF(normalizedDelta.x() * crop.width(), normalizedDelta.y() * crop.height());
    m_frameCenter = QPointF(shiftedCenter.x() / m_sourceImage.width(),
                            shiftedCenter.y() / m_sourceImage.height());
    cancelTransmitConfirmation();
    refreshModeFrame();
    scheduleDraftSave();
}

void SstvScreen::zoomFraming(double scaleFactor, const QPointF &normalizedAnchor) {
    if (m_sourceImage.isNull() || m_transmitting || scaleFactor <= 0.0)
        return;
    const SstvModeSpec *mode = SstvModeRegistry::find(
        static_cast<SstvModeId>(m_modeCombo->currentData().toInt()));
    if (!mode)
        return;

    const QSize target(mode->width, mode->height);
    const QRectF oldCrop = framingCrop(m_sourceImage.size(), target, m_fitBars,
                                        m_frameZoom, m_frameCenter);
    if (oldCrop.isEmpty())
        return;
    const double newZoom = qBound(1.0, m_frameZoom * scaleFactor, 4.0);
    if (qFuzzyCompare(newZoom, m_frameZoom))
        return;

    const QPointF anchor(qBound(0.0, normalizedAnchor.x(), 1.0),
                         qBound(0.0, normalizedAnchor.y(), 1.0));
    const QPointF sourceAnchor(oldCrop.left() + anchor.x() * oldCrop.width(),
                               oldCrop.top() + anchor.y() * oldCrop.height());
    m_frameZoom = newZoom;
    const QRectF resizedCrop = framingCrop(m_sourceImage.size(), target, m_fitBars,
                                            m_frameZoom, m_frameCenter);
    const QPointF desiredCenter(
        sourceAnchor.x() + (0.5 - anchor.x()) * resizedCrop.width(),
        sourceAnchor.y() + (0.5 - anchor.y()) * resizedCrop.height());
    m_frameCenter = QPointF(
        qBound(0.0, desiredCenter.x() / m_sourceImage.width(), 1.0),
        qBound(0.0, desiredCenter.y() / m_sourceImage.height(), 1.0));

    if (m_frameZoomSlider) {
        const QSignalBlocker blocker(m_frameZoomSlider);
        m_frameZoomSlider->setValue(qRound(m_frameZoom * 100.0));
    }
    if (m_frameZoomLabel)
        m_frameZoomLabel->setText(QStringLiteral("%1×").arg(m_frameZoom, 0, 'f', 1));
    cancelTransmitConfirmation();
    refreshModeFrame();
    scheduleDraftSave();
}

void SstvScreen::refreshModeFrame() {
    const SstvModeSpec *mode = SstvModeRegistry::find(static_cast<SstvModeId>(m_modeCombo->currentData().toInt()));
    if (!mode)
        return;
    m_modeDetail->setText(QStringLiteral("EDITING EXACT %1 × %2 PX • %3 s")
                              .arg(mode->width).arg(mode->height)
                              .arg(mode->durationMs / 1000.0, 0, 'f', 0));
    m_txFrameLabel->setText(QStringLiteral("TX FRAME • %1 • %2 × %3 PX")
                                .arg(mode->displayName).arg(mode->width).arg(mode->height));
    m_txFrameLabel->setAccessibleName(
        QStringLiteral("Exact %1 transmit frame, %2 by %3 pixels")
            .arg(mode->displayName).arg(mode->width).arg(mode->height));
    if (m_sourceImage.isNull()) {
        m_modeFrame = QImage();
        m_frozenTransmitFrame = QImage();
        updateTransmitUi();
        return;
    }
    m_modeFrame = frameSource(m_sourceImage, QSize(mode->width, mode->height),
                              m_fitBars, m_frameZoom, m_frameCenter);
    m_frozenTransmitFrame = QImage();
    if (m_modeFrame.size() == QSize(mode->width, mode->height)) {
        m_composer->setBackground(m_modeFrame);
    } else {
        m_txStateLabel->setText(QStringLiteral("COULD NOT PREPARE EXACT MODE FRAME"));
    }
    updateTransmitUi();
}

void SstvScreen::updatePowerUi() {
    m_powerLabel->setText(QStringLiteral("%1 W").arg(m_powerWatts, 0, 'f', m_powerWatts <= 10.0 ? 1 : 0));
}

void SstvScreen::updateTransmitUi() {
    m_transmitButton->setText(m_transmitConfirmationPending ? QStringLiteral("TRANSMIT SSTV")
                                                             : QStringLiteral("PREVIEW & SEND"));
    m_transmitButton->setEnabled(!m_transmitting && !m_mediaRequestPending && !m_modeFrame.isNull());
    m_stopButton->setVisible(m_transmitting);
    const bool controlsEnabled = !m_transmitting && !m_mediaRequestPending;
    m_powerSlider->setEnabled(controlsEnabled);
    m_powerMinus->setEnabled(controlsEnabled);
    m_powerPlus->setEnabled(controlsEnabled);
    m_galleryButton->setEnabled(controlsEnabled);
    m_imageTemplateGalleryButton->setEnabled(controlsEnabled);
    m_cameraButton->setEnabled(controlsEnabled);
    m_rotateLeftButton->setEnabled(controlsEnabled && !m_sourceImage.isNull());
    m_rotateRightButton->setEnabled(controlsEnabled && !m_sourceImage.isNull());
    m_frameZoomSlider->setEnabled(controlsEnabled && !m_sourceImage.isNull());
    m_resetFrameButton->setEnabled(controlsEnabled && !m_sourceImage.isNull());
    m_modeCombo->setEnabled(controlsEnabled);
    m_fitModeButton->setEnabled(controlsEnabled);
    m_composer->setEnabled(controlsEnabled);
    m_selectToolButton->setEnabled(controlsEnabled);
    m_drawToolButton->setEnabled(controlsEnabled);
    m_shapeToolButton->setEnabled(controlsEnabled);
    m_arrowToolButton->setEnabled(controlsEnabled);
    m_rectangleToolButton->setEnabled(controlsEnabled);
    m_ellipseToolButton->setEnabled(controlsEnabled);
    m_textButton->setEnabled(controlsEnabled && !m_modeFrame.isNull());
    m_myCallVariableButton->setEnabled(controlsEnabled && !m_modeFrame.isNull());
    m_toCallVariableButton->setEnabled(controlsEnabled && !m_modeFrame.isNull());
    m_colorButton->setEnabled(controlsEnabled);
    m_fillColorButton->setEnabled(controlsEnabled);
    m_fontCombo->setEnabled(controlsEnabled);
    m_textSizeSlider->setEnabled(controlsEnabled);
    m_resetCompositionButton->setEnabled(controlsEnabled);
    m_deleteObjectButton->setEnabled(controlsEnabled && m_composer->hasSelectedObject());
    m_rotateObjectLeftButton->setEnabled(controlsEnabled && m_composer->hasSelectedObject());
    m_rotateObjectRightButton->setEnabled(controlsEnabled && m_composer->hasSelectedObject());
    m_templateCombo->setEnabled(controlsEnabled);
    m_saveTemplateButton->setEnabled(controlsEnabled && !m_modeFrame.isNull());
    updateTemplateActionUi();
    m_resetTemplatesButton->setEnabled(controlsEnabled);
    m_clearDraftButton->setEnabled(controlsEnabled);
    m_callsignEdit->setEnabled(controlsEnabled);
    m_toCallsignEdit->setEnabled(controlsEnabled);
    m_fskIdCheck->setEnabled(controlsEnabled);
    m_cwIdCheck->setEnabled(controlsEnabled);
    m_cwWpmSpin->setEnabled(controlsEnabled && m_cwIdCheck->isChecked());
    m_cwWpmMinus->setEnabled(controlsEnabled && m_cwIdCheck->isChecked()
                             && m_cwWpmSpin->value() > m_cwWpmSpin->minimum());
    m_cwWpmPlus->setEnabled(controlsEnabled && m_cwIdCheck->isChecked()
                            && m_cwWpmSpin->value() < m_cwWpmSpin->maximum());
    updateComposerControls();
}

bool SstvScreen::sendCallsignCw() const {
    return m_cwIdCheck && m_cwIdCheck->isChecked() && !m_operatorCallsign.isEmpty();
}

bool SstvScreen::sendCallsignFsk() const {
    return m_fskIdCheck && m_fskIdCheck->isChecked() && !m_operatorCallsign.isEmpty();
}

int SstvScreen::callsignCwWpm() const {
    return m_cwWpmSpin ? m_cwWpmSpin->value() : 20;
}

bool SstvScreen::eventFilter(QObject *watched, QEvent *event) {
    if (m_txScrollGestureSuppressClick
        && (event->type() == QEvent::MouseButtonRelease
            || event->type() == QEvent::MouseButtonDblClick)) {
        if (auto *button = qobject_cast<QAbstractButton *>(watched))
            button->setDown(false);
        if (watched == m_txSliderDragTarget) {
            m_txSliderDragTarget = nullptr;
            m_txSliderScrolling = false;
            m_txSliderAdjusting = false;
        }
        return true;
    }

    auto *slider = qobject_cast<QSlider *>(watched);
    if (!slider || (slider != m_frameZoomSlider && slider != m_textSizeSlider
                    && slider != m_powerSlider)) {
        return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            m_txSliderDragTarget = slider;
            m_txSliderPressPosition = mouseEvent->pos();
            m_txSliderLastY = mouseEvent->pos().y();
            m_txSliderScrolling = false;
            m_txSliderAdjusting = false;
            // Wait for a direction before changing the value. A vertical
            // gesture belongs to the scrolling control pane.
            return true;
        }
    } else if (event->type() == QEvent::MouseMove && watched == m_txSliderDragTarget) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        const QPoint delta = mouseEvent->pos() - m_txSliderPressPosition;
        if (!m_txSliderScrolling && !m_txSliderAdjusting
            && delta.manhattanLength() > 6) {
            m_txSliderScrolling = qAbs(delta.y()) > qAbs(delta.x());
            m_txSliderAdjusting = !m_txSliderScrolling;
        }
        if (m_txSliderScrolling) {
            if (m_txControlsScroll) {
                m_txControlsScroll->verticalScrollBar()->setValue(
                    m_txControlsScroll->verticalScrollBar()->value()
                    - (mouseEvent->pos().y() - m_txSliderLastY));
            }
            m_txSliderLastY = mouseEvent->pos().y();
            return true;
        }
        if (m_txSliderAdjusting) {
            setSliderValueFromTouchPosition(slider, mouseEvent->pos().x());
            return true;
        }
        return true;
    } else if (event->type() == QEvent::MouseButtonRelease
               && watched == m_txSliderDragTarget) {
        if (!m_txSliderScrolling && !m_txSliderAdjusting) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            // A stationary tap selects the touched value. Direction locking
            // still protects vertical swipes from changing the control.
            setSliderValueFromTouchPosition(slider, mouseEvent->pos().x());
        }
        m_txSliderDragTarget = nullptr;
        m_txSliderScrolling = false;
        m_txSliderAdjusting = false;
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void SstvScreen::setSliderValueFromTouchPosition(QSlider *slider, int xPosition) {
    if (!slider)
        return;
    const int handleWidth = qMax(12, slider->height() / 2);
    const int span = qMax(1, slider->width() - handleWidth);
    const int position = qBound(0, xPosition - handleWidth / 2, span);
    slider->setValue(QStyle::sliderValueFromPosition(slider->minimum(), slider->maximum(),
                                                     position, span,
                                                     slider->invertedAppearance()));
}

void SstvScreen::addOrEditText() {
    if (m_transmitting || !m_composer->hasBackground())
        return;
    bool accepted = false;
    const bool editing = m_composer->hasSelectedText();
    const QString initialText = editing ? m_composer->selectedText()
                                        : (m_operatorCallsign.isEmpty()
                                               ? QStringLiteral("TEXT") : m_operatorCallsign);
    const QString text = promptSstvTxText(this,
                                          editing ? QStringLiteral("Edit SSTV text")
                                                  : QStringLiteral("Add SSTV text"),
                                          QStringLiteral("Text to transmit:"),
                                          initialText, true, &accepted, true);
    if (!accepted || text.trimmed().isEmpty())
        return;
    const QFont font = composerFontFromControls();
    if (editing)
        m_composer->updateSelectedText(text, font, m_composerFillColor);
    else
        m_composer->addTextBlock(text, font, m_composerFillColor);
}

void SstvScreen::addVariableText(const QString &token) {
    if (m_transmitting || !m_composer->hasBackground())
        return;
    m_composer->addTextBlock(token, composerFontFromControls(), m_composerFillColor);
    m_composer->setTool(SstvComposerCanvas::Tool::Select);
    syncTextControlsFromSelection();
    updateComposerControls();
}

QFont SstvScreen::composerFontFromControls() const {
    const QVariantMap descriptor = m_fontCombo ? m_fontCombo->currentData().toMap()
                                                : QVariantMap();
    QFont font(descriptor.value(QStringLiteral("family"), QStringLiteral("Sans Serif")).toString());
    font.setWeight(static_cast<QFont::Weight>(
        descriptor.value(QStringLiteral("weight"), static_cast<int>(QFont::Normal)).toInt()));
    font.setItalic(descriptor.value(QStringLiteral("italic"), false).toBool());
    font.setStretch(descriptor.value(QStringLiteral("stretch"), QFont::Unstretched).toInt());
    font.setPixelSize(m_textSizeSlider ? m_textSizeSlider->value() : 28);
    return font;
}

void SstvScreen::syncTextControlsFromSelection() {
    if (!m_composer || !m_fontCombo || !m_textSizeSlider)
        return;
    if (m_composer->hasSelectedObject()) {
        const QColor selectedOutline = m_composer->selectedOutlineColor();
        if (selectedOutline.isValid()) {
            m_composerColor = selectedOutline;
            m_colorButton->setIcon(sstvGlyph(SstvGlyph::Color, m_composerColor));
            m_colorButton->setToolTip(
                m_composerColor.alpha() == 0
                    ? QStringLiteral("Object outline: NO OUTLINE")
                    : QStringLiteral("Object outline: %1")
                          .arg(m_composerColor.name().toUpper()));
        }
        if (m_composer->selectedObjectSupportsFill()) {
            m_composerFillColor = m_composer->selectedFillColor();
            m_fillColorButton->setIcon(
                sstvGlyph(SstvGlyph::Color, m_composerFillColor));
            m_fillColorButton->setToolTip(
                m_composerFillColor.alpha() == 0
                    ? QStringLiteral("Object fill / line color: TRANSPARENT")
                    : QStringLiteral("Object fill / line color: %1")
                          .arg(m_composerFillColor.name().toUpper()));
        }
        const int selectedSize = m_composer->selectedSize();
        if (selectedSize > 0) {
            const QSignalBlocker sizeBlocker(m_textSizeSlider);
            m_textSizeSlider->setValue(qBound(m_textSizeSlider->minimum(), selectedSize,
                                              m_textSizeSlider->maximum()));
        }
    }
    if (!m_composer->hasSelectedText())
        return;
    const QFont selected = m_composer->selectedTextFont();
    int familyMatch = -1;
    int exactMatch = -1;
    for (int index = 0; index < m_fontCombo->count(); ++index) {
        const QVariantMap descriptor = m_fontCombo->itemData(index).toMap();
        if (descriptor.value(QStringLiteral("family")).toString().compare(
                selected.family(), Qt::CaseInsensitive) != 0) {
            continue;
        }
        if (familyMatch < 0)
            familyMatch = index;
        if (descriptor.value(QStringLiteral("weight")).toInt() == selected.weight()
            && descriptor.value(QStringLiteral("italic")).toBool() == selected.italic()
            && descriptor.value(QStringLiteral("stretch")).toInt() == selected.stretch()) {
            exactMatch = index;
            break;
        }
    }
    const QSignalBlocker fontBlocker(m_fontCombo);
    if (exactMatch >= 0 || familyMatch >= 0)
        m_fontCombo->setCurrentIndex(exactMatch >= 0 ? exactMatch : familyMatch);
}

void SstvScreen::chooseInkColor() {
    const QColor selected = promptSstvMarkupColor(
        this, m_composerColor, QStringLiteral("OBJECT OUTLINE COLOR"), true);
    if (!selected.isValid())
        return;

    m_composerColor = selected;
    m_composer->setInk(m_composerColor, qMax(2, m_textSizeSlider->value() / 5));
    m_composer->updateSelectedOutlineColor(m_composerColor);
    m_colorButton->setIcon(sstvGlyph(SstvGlyph::Color, m_composerColor));
    m_colorButton->setToolTip(
        m_composerColor.alpha() == 0
            ? QStringLiteral("Object outline: NO OUTLINE")
            : QStringLiteral("Object outline: %1")
                  .arg(m_composerColor.name().toUpper()));
}

void SstvScreen::chooseFillColor() {
    const QColor selected = promptSstvMarkupColor(
        this, m_composerFillColor, QStringLiteral("FILL / LINE COLOR"), true);
    if (!selected.isValid())
        return;
    m_composerFillColor = selected;
    m_composer->setFillColor(selected);
    m_composer->updateSelectedFillColor(selected);
    m_fillColorButton->setIcon(sstvGlyph(SstvGlyph::Color, selected));
    m_fillColorButton->setToolTip(
        selected.alpha() == 0 ? QStringLiteral("Object fill / line color: TRANSPARENT")
                              : QStringLiteral("Object fill / line color: %1")
                                    .arg(selected.name().toUpper()));
}

void SstvScreen::refreshTemplates(const QString &selectName) {
    const QSignalBlocker blocker(m_templateCombo);
    const QString previousKey = m_templateCombo->currentData().toString();
    m_templateCombo->clear();
    const QString callLabel = m_operatorCallsign.isEmpty()
                                  ? QStringLiteral("SET MY CALL") : m_operatorCallsign;
    QString error;
    const QStringList names = m_storage.userTemplateNames(&error);
    const auto addBuiltin = [this, &names, &callLabel](const QString &key) {
        const QString overrideName = sstvBuiltinOverrideName(key);
        bool includesImage = false;
        if (names.contains(overrideName)) {
            QJsonObject state;
            includesImage = m_storage.loadUserTemplate(overrideName, &state)
                && !state.value(QStringLiteral("sourceFile")).toString().isEmpty();
        }
        QString label = QStringLiteral("%1 • %2").arg(sstvBuiltinBaseName(key), callLabel);
        if (includesImage)
            label += QStringLiteral(" • IMAGE");
        m_templateCombo->addItem(label, key);
    };
    addBuiltin(QStringLiteral("builtin:cq"));
    addBuiltin(QStringLiteral("builtin:report"));
    addBuiltin(QStringLiteral("builtin:73"));
    int selectedIndex = selectName.isEmpty() ? m_templateCombo->findData(previousKey) : -1;
    for (const QString &name : names) {
        if (sstvIsBuiltinOverrideName(name))
            continue;
        QJsonObject templateState;
        QString templateError;
        const bool includesImage = m_storage.loadUserTemplate(
            name, &templateState, &templateError)
            && !templateState.value(QStringLiteral("sourceFile")).toString().isEmpty();
        m_templateCombo->addItem(
            includesImage ? QStringLiteral("%1 • IMAGE").arg(name) : name,
            QStringLiteral("user:") + name);
        if (name.compare(selectName, Qt::CaseInsensitive) == 0)
            selectedIndex = m_templateCombo->count() - 1;
    }
    if (selectName.isEmpty())
        selectedIndex = m_templateCombo->findData(previousKey);
    m_templateCombo->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);
    updateTemplateActionUi();
    if (!error.isEmpty())
        m_txStateLabel->setText(QStringLiteral("TEMPLATE WARNING • %1").arg(error));
}

void SstvScreen::applySelectedTemplate() {
    const QString key = m_templateCombo->currentData().toString();
    QString storedName;
    if (key.startsWith(QStringLiteral("user:"))) {
        storedName = key.mid(5);
    } else {
        const QString overrideName = sstvBuiltinOverrideName(key);
        if (!overrideName.isEmpty()
            && m_storage.userTemplateNames().contains(overrideName)) {
            storedName = overrideName;
        }
    }
    if (!storedName.isEmpty()) {
        QJsonObject state;
        QImage templateImage;
        QString error;
        if (!m_storage.loadUserTemplate(storedName, &state, &templateImage, &error)) {
            m_txStateLabel->setText(QStringLiteral("TEMPLATE FAILED • %1").arg(error));
            return;
        }
        if (!templateImage.isNull()) {
            // Use the same complete restore path as the image-template gallery
            // and recovery draft. Applying the source and composition through
            // separate UI updates could leave the old canvas background visible
            // while the newly restored objects were already painted over it.
            // Layout templates intentionally retain the currently selected mode.
            state.insert(QStringLiteral("modeId"), m_modeCombo->currentData().toInt());
            restoreTxState(
                templateImage, state,
                QStringLiteral("TEMPLATE APPLIED • IMAGE INCLUDED • REVIEW BEFORE PREVIEW"));
            m_editingTemplateName.clear();
            updateTemplateActionUi();
            return;
        } else if (!m_composer->hasBackground()) {
            m_txStateLabel->setText(
                QStringLiteral("TEMPLATE NEEDS A TX IMAGE • SELECT GALLERY OR CAMERA"));
            return;
        }
        if (!m_composer->restoreCompositionState(
                state.value(QStringLiteral("composition")).toObject())) {
            m_txStateLabel->setText(QStringLiteral("TEMPLATE FAILED • INVALID TEMPLATE DATA"));
            return;
        }
    } else {
        if (!m_composer->hasBackground()) {
            m_txStateLabel->setText(
                QStringLiteral("TEMPLATE NEEDS A TX IMAGE • SELECT GALLERY OR CAMERA"));
            return;
        }
        m_composer->restoreCompositionState(QJsonObject());
        QFont font(QStringLiteral("Sans Serif"));
        font.setPixelSize(qMax(24, m_composer->renderedImage().width() / 10));
        font.setBold(true);
        m_composer->addTextBlock(sstvBuiltinText(key), font, Qt::white,
                                 QPointF(0.5, 0.5));
    }
    m_editingTemplateName.clear();
    updateTemplateActionUi();
    m_txStateLabel->setText(QStringLiteral("TEMPLATE APPLIED • REVIEW BEFORE PREVIEW"));
}

void SstvScreen::saveUserTemplate() {
    if (!m_composer->hasBackground())
        return;
    const SstvTemplateSaveChoice choice = promptSstvTemplateSave(this);
    if (!choice.accepted)
        return;
    const QString name = choice.name;
    const QStringList existing = m_storage.userTemplateNames();
    if (existing.contains(name, Qt::CaseInsensitive)) {
        m_txStateLabel->setText(
            QStringLiteral("TEMPLATE NAME EXISTS • SELECT IT AND USE EDIT TO UPDATE"));
        return;
    }
    QJsonObject state{{QStringLiteral("composition"), m_composer->compositionState()}};
    if (choice.includeImage) {
        state.insert(QStringLiteral("fitBars"), m_fitBars);
        state.insert(QStringLiteral("frameZoom"), m_frameZoom);
        state.insert(QStringLiteral("frameCenterX"), m_frameCenter.x());
        state.insert(QStringLiteral("frameCenterY"), m_frameCenter.y());
    }
    QString error;
    if (!m_storage.saveUserTemplate(name, state,
                                    choice.includeImage ? m_sourceImage : QImage(), &error)) {
        m_txStateLabel->setText(QStringLiteral("TEMPLATE SAVE FAILED • %1").arg(error));
        return;
    }
    m_editingTemplateName.clear();
    refreshTemplates(name);
    m_txStateLabel->setText(
        QStringLiteral("TEMPLATE SAVED • %1 • %2")
            .arg(name, choice.includeImage ? QStringLiteral("IMAGE INCLUDED")
                                           : QStringLiteral("LAYOUT ONLY")));
}

void SstvScreen::editSelectedTemplate() {
    const QString key = m_templateCombo->currentData().toString();
    const bool userTemplate = key.startsWith(QStringLiteral("user:"));
    const QString overrideName = userTemplate ? QString()
                                              : sstvBuiltinOverrideName(key);
    const bool hasBuiltinOverride = !overrideName.isEmpty()
        && m_storage.userTemplateNames().contains(overrideName);
    const bool hasStoredTemplate = userTemplate || hasBuiltinOverride;
    const QString name = userTemplate ? key.mid(5) : sstvBuiltinBaseName(key);
    const QString storageName = userTemplate ? name : overrideName;
    QJsonObject state;
    QImage storedTemplateImage;
    QString error;
    if (hasStoredTemplate
        && !m_storage.loadUserTemplate(storageName, &state, &storedTemplateImage, &error)) {
        m_txStateLabel->setText(QStringLiteral("TEMPLATE EDIT FAILED • %1").arg(error));
        return;
    }
    QImage editorSourceImage = storedTemplateImage.isNull() ? m_sourceImage
                                                             : storedTemplateImage;
    if (editorSourceImage.isNull()) {
        m_txStateLabel->setText(
            QStringLiteral("TEMPLATE EDIT NEEDS AN IMAGE • SELECT GALLERY OR CAMERA"));
        return;
    }
    bool editorFitBars = storedTemplateImage.isNull()
                             ? m_fitBars : state.value(QStringLiteral("fitBars")).toBool(false);
    double editorFrameZoom = storedTemplateImage.isNull()
                                 ? m_frameZoom
                                 : qBound(1.0,
                                          state.value(QStringLiteral("frameZoom")).toDouble(1.0),
                                          4.0);
    QPointF editorFrameCenter = storedTemplateImage.isNull()
                                    ? m_frameCenter
                                    : QPointF(
                                          qBound(0.0,
                                                 state.value(QStringLiteral("frameCenterX"))
                                                     .toDouble(0.5),
                                                 1.0),
                                          qBound(0.0,
                                                 state.value(QStringLiteral("frameCenterY"))
                                                     .toDouble(0.5),
                                                 1.0));
    InWindowDialog dialog(this);
    QWidget *panel = dialog.contentWidget();
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("EDIT SSTV TEMPLATE • %1").arg(name), panel);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(
        QStringLiteral("color: #f2ad20; font-size: 12px; font-weight: 700;"));
    layout->addWidget(title);

    auto *hint = new QLabel(
        userTemplate
            ? QStringLiteral("Edit the saved layout. INCLUDE IMAGE recalls this picture with the template; unchecked recalls only text and markup over the current TX image.")
            : QStringLiteral("Edit this default directly. SAVE TEMPLATE replaces the default layout; INCLUDE IMAGE also recalls this picture."),
        panel);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #cbd3d6; font-size: 9px;"));
    layout->addWidget(hint);

    auto *workspaceWidget = new QWidget(panel);
    auto *workspaceLayout = new QBoxLayout(QBoxLayout::TopToBottom, workspaceWidget);
    workspaceLayout->setContentsMargins(0, 0, 0, 0);
    workspaceLayout->setSpacing(8);
    auto *editor = new SstvComposerCanvas(workspaceWidget);
    editor->setMinimumSize(220, 100);
    editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    const SstvModeSpec *editorMode = SstvModeRegistry::find(
        static_cast<SstvModeId>(m_modeCombo->currentData().toInt()));
    const QSize editorTargetSize = editorMode ? QSize(editorMode->width, editorMode->height)
                                               : m_modeFrame.size();
    editor->setBackground(frameSource(editorSourceImage, editorTargetSize,
                                      editorFitBars, editorFrameZoom,
                                      editorFrameCenter));
    editor->setCallsignValues(m_operatorCallsign, m_replyCallsign);
    if (hasStoredTemplate && !editor->restoreCompositionState(
            state.value(QStringLiteral("composition")).toObject())) {
        m_txStateLabel->setText(QStringLiteral("TEMPLATE EDIT FAILED • INVALID TEMPLATE DATA"));
        return;
    }
    if (!hasStoredTemplate) {
        QFont font(QStringLiteral("Sans Serif"));
        font.setPixelSize(qMax(24, editor->renderedImage().width() / 10));
        font.setBold(true);
        editor->addTextBlock(sstvBuiltinText(key), font, Qt::white,
                             QPointF(0.5, 0.5));
    }
    workspaceLayout->addWidget(editor, 1);
    auto *controlPanel = new QWidget(workspaceWidget);
    auto *controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->setSpacing(5);
    workspaceLayout->addWidget(controlPanel);
    layout->addWidget(workspaceWidget, 1);

    QColor editorColor = Qt::black;
    QColor editorFillColor = Qt::white;
    auto *moveButton = new QPushButton(QStringLiteral("MOVE"), panel);
    auto *drawButton = new QPushButton(QStringLiteral("DRAW"), panel);
    auto *shapeButton = new QPushButton(QStringLiteral("LINE"), panel);
    auto *arrowButton = new QPushButton(panel);
    auto *rectangleButton = new QPushButton(panel);
    auto *ellipseButton = new QPushButton(panel);
    auto *textButton = new QPushButton(QStringLiteral("ADD TEXT"), panel);
    auto *myCallButton = new QPushButton(QStringLiteral("MY CALL"), panel);
    auto *toCallButton = new QPushButton(QStringLiteral("TO CALL"), panel);
    auto *colorButton = new QPushButton(panel);
    auto *fillColorButton = new QPushButton(panel);
    auto *deleteButton = new QPushButton(QStringLiteral("DELETE"), panel);
    auto *rotateLeftButton = new QPushButton(panel);
    auto *rotateRightButton = new QPushButton(panel);
    auto *undoButton = new QPushButton(panel);
    auto *redoButton = new QPushButton(panel);
    auto *resetButton = new QPushButton(QStringLiteral("RESET"), panel);
    for (QPushButton *button : {moveButton, drawButton, shapeButton, arrowButton,
                                rectangleButton, ellipseButton, textButton, myCallButton,
                                toCallButton, colorButton, fillColorButton, deleteButton,
                                rotateLeftButton, rotateRightButton, undoButton, redoButton,
                                resetButton}) {
        QFont compactFont = button->font();
        compactFont.setPixelSize(10);
        button->setFont(compactFont);
        button->setFixedHeight(30);
        button->setStyleSheet(buttonStyle(QStringLiteral("#6dd4ef"))
                              + QStringLiteral("QPushButton { padding: 4px 6px; border-radius: 4px; }"));
    }
    moveButton->setIcon(sstvGlyph(SstvGlyph::Move));
    drawButton->setIcon(sstvGlyph(SstvGlyph::Draw));
    shapeButton->setIcon(sstvGlyph(SstvGlyph::Line));
    arrowButton->setIcon(sstvGlyph(SstvGlyph::Arrow));
    rectangleButton->setIcon(sstvGlyph(SstvGlyph::Rectangle));
    ellipseButton->setIcon(sstvGlyph(SstvGlyph::Ellipse));
    colorButton->setIcon(sstvGlyph(SstvGlyph::Color, editorColor));
    fillColorButton->setIcon(sstvGlyph(SstvGlyph::Color, editorFillColor));
    deleteButton->setIcon(sstvGlyph(SstvGlyph::Trash));
    rotateLeftButton->setIcon(sstvGlyph(SstvGlyph::RotateLeft));
    rotateRightButton->setIcon(sstvGlyph(SstvGlyph::RotateRight));
    undoButton->setIcon(sstvGlyph(SstvGlyph::Undo));
    redoButton->setIcon(sstvGlyph(SstvGlyph::Redo));
    for (QPushButton *button : {moveButton, drawButton, shapeButton, arrowButton,
                                rectangleButton, ellipseButton, colorButton, fillColorButton,
                                deleteButton, rotateLeftButton, rotateRightButton,
                                undoButton, redoButton}) {
        button->setText(QString());
        button->setFixedSize(26, 26);
        button->setIconSize(QSize(10, 10));
    }
    moveButton->setToolTip(QStringLiteral("Move or select an object"));
    drawButton->setToolTip(QStringLiteral("Draw freehand"));
    shapeButton->setToolTip(QStringLiteral("Draw line"));
    arrowButton->setToolTip(QStringLiteral("Draw arrow"));
    rectangleButton->setToolTip(QStringLiteral("Draw rectangle"));
    ellipseButton->setToolTip(QStringLiteral("Draw ellipse"));
    colorButton->setToolTip(QStringLiteral("Choose the thin object outline color"));
    fillColorButton->setToolTip(
        QStringLiteral("Choose text/line color or rectangle/ellipse fill color"));
    deleteButton->setToolTip(QStringLiteral("Delete selected object"));
    rotateLeftButton->setToolTip(QStringLiteral("Rotate selected object left 45 degrees"));
    rotateRightButton->setToolTip(QStringLiteral("Rotate selected object right 45 degrees"));
    undoButton->setToolTip(QStringLiteral("Undo last edit"));
    redoButton->setToolTip(QStringLiteral("Redo last edit"));
    textButton->setFixedWidth(78);
    myCallButton->setFixedWidth(56);
    toCallButton->setFixedWidth(56);
    resetButton->setFixedWidth(56);

    auto *toolWidget = new QWidget(panel);
    auto *toolRows = new QVBoxLayout(toolWidget);
    toolRows->setContentsMargins(0, 0, 0, 0);
    toolRows->setSpacing(3);

    auto *drawingRow = new QHBoxLayout;
    drawingRow->setSpacing(4);
    drawingRow->addStretch(1);
    for (QPushButton *button : {moveButton, drawButton, shapeButton, arrowButton,
                                rectangleButton, ellipseButton, colorButton, fillColorButton})
        drawingRow->addWidget(button);
    drawingRow->addStretch(1);
    toolRows->addLayout(drawingRow);

    auto *editRow = new QHBoxLayout;
    editRow->setSpacing(4);
    editRow->addStretch(1);
    // Match the visual ordering used by mobile editors: clockwise/right first,
    // then counterclockwise/left. Keep every related pair adjacent.
    editRow->addWidget(rotateRightButton);
    editRow->addWidget(rotateLeftButton);
    editRow->addSpacing(5);
    editRow->addWidget(undoButton);
    editRow->addWidget(redoButton);
    editRow->addWidget(deleteButton);
    editRow->addStretch(1);
    toolRows->addLayout(editRow);

    auto *objectRow = new QHBoxLayout;
    objectRow->setSpacing(4);
    // Portrait has enough width to balance RESET with an equal left spacer,
    // keeping the three text actions centered over the whole row. The compact
    // landscape side panel removes this spacer so fixed-width buttons cannot
    // collide.
    auto *objectRowBalance = new QSpacerItem(resetButton->width() + objectRow->spacing(), 0,
                                             QSizePolicy::Fixed, QSizePolicy::Minimum);
    objectRow->addItem(objectRowBalance);
    objectRow->addStretch(1);
    objectRow->addWidget(textButton);
    objectRow->addWidget(myCallButton);
    objectRow->addWidget(toCallButton);
    objectRow->addStretch(1);
    objectRow->addWidget(resetButton);
    toolRows->addLayout(objectRow);
    toolWidget->setFixedHeight(88);
    controlLayout->addWidget(toolWidget);

    int editorFontIndex = qMax(0, m_fontCombo->currentIndex());
    auto *fontButton = new QPushButton(m_fontCombo->itemText(editorFontIndex), panel);
    fontButton->setFixedSize(108, 30);
    fontButton->setIcon(sstvGlyph(SstvGlyph::ChevronDown, QColor(QStringLiteral("#303030"))));
    fontButton->setIconSize(QSize(8, 8));
    fontButton->setLayoutDirection(Qt::RightToLeft);
    fontButton->setAccessibleName(QStringLiteral("Choose template text font"));
    fontButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #f4f4f4; color: #111; border: 2px solid #555b5e; "
        "border-radius: 5px; padding: 4px 7px; text-align: left; font-size: 10px; }"
        "QPushButton:pressed { border-color: #6dd4ef; }"));
    auto *sizeSlider = new QSlider(Qt::Horizontal, panel);
    sizeSlider->setRange(SstvTextSizeMinimumPx, SstvTextSizeMaximumPx);
    sizeSlider->setValue(SstvTextSizeDefaultPx);
    sizeSlider->setMinimumHeight(26);
    sizeSlider->setStyleSheet(
        K4Styles::sliderHorizontal(K4Styles::Colors::DarkBackground,
                                   K4Styles::Colors::VfoACyan));
    auto *sizeLabel = new QLabel(QStringLiteral("28 PX"), panel);
    sizeLabel->setFixedWidth(42);
    sizeLabel->setAlignment(Qt::AlignCenter);

    for (QWidget *control : {static_cast<QWidget *>(sizeLabel)}) {
        QFont compactFont = control->font();
        compactFont.setPixelSize(10);
        control->setFont(compactFont);
    }

    auto fontFromEditorControls = [this, &editorFontIndex, sizeSlider]() {
        const QVariantMap descriptor = m_fontCombo->itemData(editorFontIndex).toMap();
        QFont font(descriptor.value(QStringLiteral("family"),
                                    QStringLiteral("Sans Serif")).toString());
        font.setWeight(static_cast<QFont::Weight>(
            descriptor.value(QStringLiteral("weight"),
                             static_cast<int>(QFont::Normal)).toInt()));
        font.setItalic(descriptor.value(QStringLiteral("italic"), false).toBool());
        font.setStretch(descriptor.value(QStringLiteral("stretch"),
                                         QFont::Unstretched).toInt());
        font.setPixelSize(sizeSlider->value());
        return font;
    };
    auto *formatWidget = new QWidget(panel);
    auto *formatGrid = new QGridLayout(formatWidget);
    formatGrid->setContentsMargins(0, 0, 0, 0);
    formatGrid->setHorizontalSpacing(5);
    formatGrid->setVerticalSpacing(3);
    formatGrid->setAlignment(Qt::AlignHCenter);
    auto *fontLabel = new QLabel(QStringLiteral("FONT"), panel);
    auto *sizeCaption = new QLabel(QStringLiteral("SIZE"), panel);
    for (QLabel *label : {fontLabel, sizeCaption})
        label->setStyleSheet(QStringLiteral("font-size: 9px; color: #f0f0f0;"));
    const auto clearGrid = [](QGridLayout *grid) {
        while (QLayoutItem *item = grid->takeAt(0))
            delete item;
    };
    const auto reflowFormatGrid = [formatGrid, clearGrid, fontLabel, fontButton,
                                   sizeCaption, sizeSlider, sizeLabel](bool) {
        clearGrid(formatGrid);
        for (int column = 0; column < 5; ++column)
            formatGrid->setColumnStretch(column, 0);
        formatGrid->addWidget(fontLabel, 0, 0);
        formatGrid->addWidget(fontButton, 0, 1);
        formatGrid->addWidget(sizeCaption, 0, 2);
        formatGrid->addWidget(sizeSlider, 0, 3);
        formatGrid->addWidget(sizeLabel, 0, 4);
        formatGrid->setColumnStretch(3, 1);
        formatGrid->invalidate();
        formatGrid->activate();
    };
    controlLayout->addWidget(formatWidget);

    auto *zoomRow = new QHBoxLayout;
    zoomRow->setSpacing(5);
    auto *zoomCaption = new QLabel(QStringLiteral("ZOOM"), panel);
    zoomCaption->setStyleSheet(QStringLiteral("font-size: 9px; color: #f0f0f0;"));
    auto *editorZoomSlider = new QSlider(Qt::Horizontal, panel);
    editorZoomSlider->setRange(100, 400);
    editorZoomSlider->setValue(qRound(editorFrameZoom * 100.0));
    editorZoomSlider->setMinimumHeight(26);
    editorZoomSlider->setStyleSheet(
        K4Styles::sliderHorizontal(K4Styles::Colors::DarkBackground,
                                   K4Styles::Colors::AccentAmber));
    auto *editorZoomLabel = new QLabel(
        QStringLiteral("%1×").arg(editorFrameZoom, 0, 'f', 1), panel);
    editorZoomLabel->setFixedWidth(42);
    editorZoomLabel->setAlignment(Qt::AlignCenter);
    editorZoomLabel->setStyleSheet(QStringLiteral("font-size: 10px; color: #f0f0f0;"));
    auto *centerButton = new QPushButton(QStringLiteral("CENTER"), panel);
    centerButton->setFixedSize(58, 30);
    centerButton->setStyleSheet(
        buttonStyle(QStringLiteral("#6dd4ef"))
        + QStringLiteral("QPushButton { padding: 4px 6px; border-radius: 4px; font-size: 10px; }"));
    zoomRow->addWidget(zoomCaption);
    zoomRow->addWidget(editorZoomSlider, 1);
    zoomRow->addWidget(editorZoomLabel);
    zoomRow->addWidget(centerButton);
    controlLayout->addLayout(zoomRow);

    auto *imageRow = new QHBoxLayout;
    imageRow->setSpacing(6);
    auto *templateGalleryButton = new QPushButton(QStringLiteral("GALLERY IMAGE"), panel);
    templateGalleryButton->setFixedHeight(30);
    templateGalleryButton->setStyleSheet(
        buttonStyle(QStringLiteral("#6dd4ef"))
        + QStringLiteral("QPushButton { padding: 4px 7px; border-radius: 4px; font-size: 10px; }"));
    auto *includeImageCheck = new QCheckBox(QStringLiteral("INCLUDE IMAGE"), panel);
    includeImageCheck->setChecked(!storedTemplateImage.isNull());
    includeImageCheck->setToolTip(QStringLiteral(
        "Checked: this image replaces the main TX image when the template is applied. "
        "Unchecked: only text and markup are recalled."));
    includeImageCheck->setStyleSheet(sstvVisibleCheckBoxStyle(10, 20));
    imageRow->addWidget(templateGalleryButton);
    imageRow->addWidget(includeImageCheck);
    imageRow->addStretch(1);
    controlLayout->addLayout(imageRow);
    controlLayout->addStretch(1);

    const auto syncEditorFraming = [editor, editorZoomSlider, editorZoomLabel,
                                    editorTargetSize, &editorSourceImage,
                                    &editorFitBars, &editorFrameZoom,
                                    &editorFrameCenter]() {
        const QSignalBlocker blocker(editorZoomSlider);
        editorZoomSlider->setValue(qRound(editorFrameZoom * 100.0));
        editorZoomLabel->setText(
            QStringLiteral("%1×").arg(editorFrameZoom, 0, 'f', 1));
        editor->setBackground(frameSource(editorSourceImage, editorTargetSize,
                                          editorFitBars, editorFrameZoom,
                                          editorFrameCenter));
    };
    const auto zoomEditorFraming = [editorTargetSize, &editorSourceImage,
                                    &editorFitBars, &editorFrameZoom,
                                    &editorFrameCenter](double scaleFactor,
                                                       const QPointF &normalizedAnchor) {
        if (editorSourceImage.isNull() || scaleFactor <= 0.0)
            return;
        const QRectF oldCrop = framingCrop(editorSourceImage.size(), editorTargetSize,
                                            editorFitBars, editorFrameZoom,
                                            editorFrameCenter);
        if (oldCrop.isEmpty())
            return;
        const double newZoom = qBound(1.0, editorFrameZoom * scaleFactor, 4.0);
        if (qFuzzyCompare(newZoom, editorFrameZoom))
            return;
        const QPointF anchor(qBound(0.0, normalizedAnchor.x(), 1.0),
                             qBound(0.0, normalizedAnchor.y(), 1.0));
        const QPointF sourceAnchor(oldCrop.left() + anchor.x() * oldCrop.width(),
                                   oldCrop.top() + anchor.y() * oldCrop.height());
        editorFrameZoom = newZoom;
        const QRectF resizedCrop = framingCrop(editorSourceImage.size(), editorTargetSize,
                                                editorFitBars, editorFrameZoom,
                                                editorFrameCenter);
        const QPointF desiredCenter(
            sourceAnchor.x() + (0.5 - anchor.x()) * resizedCrop.width(),
            sourceAnchor.y() + (0.5 - anchor.y()) * resizedCrop.height());
        editorFrameCenter = QPointF(
            qBound(0.0, desiredCenter.x() / editorSourceImage.width(), 1.0),
            qBound(0.0, desiredCenter.y() / editorSourceImage.height(), 1.0));
    };
    connect(editorZoomSlider, &QSlider::valueChanged, panel,
            [syncEditorFraming, zoomEditorFraming, &editorFrameZoom](int value) {
        const double requestedZoom = value / 100.0;
        zoomEditorFraming(requestedZoom / qMax(0.01, editorFrameZoom),
                          QPointF(0.5, 0.5));
        syncEditorFraming();
    });
    connect(centerButton, &QPushButton::clicked, panel,
            [syncEditorFraming, &editorFrameZoom, &editorFrameCenter]() {
        editorFrameZoom = 1.0;
        editorFrameCenter = QPointF(0.5, 0.5);
        syncEditorFraming();
    });
    connect(templateGalleryButton, &QPushButton::clicked, panel,
            [this, hint, includeImageCheck, syncEditorFraming,
             &editorSourceImage, &editorFrameZoom, &editorFrameCenter]() {
        QString pickerError;
        bool cancelled = false;
        const QImage selected = promptSstvGalleryImage(this, &pickerError, &cancelled);
        if (selected.isNull()) {
            if (!cancelled)
                hint->setText(QStringLiteral("IMAGE SELECTION FAILED • %1").arg(pickerError));
            return;
        }
        editorSourceImage = selected;
        editorFrameZoom = 1.0;
        editorFrameCenter = QPointF(0.5, 0.5);
        includeImageCheck->setChecked(true);
        hint->setText(QStringLiteral(
            "Gallery image loaded. Leave INCLUDE IMAGE checked to store it with this template."));
        syncEditorFraming();
    });

    connect(moveButton, &QPushButton::clicked, editor, [editor]() {
        editor->setTool(SstvComposerCanvas::Tool::Select);
    });
    connect(editor, &SstvComposerCanvas::backgroundPanRequested, panel,
            [syncEditorFraming, editorTargetSize, &editorSourceImage,
             &editorFitBars, &editorFrameZoom,
             &editorFrameCenter](const QPointF &delta) {
        const QRectF crop = framingCrop(editorSourceImage.size(), editorTargetSize,
                                        editorFitBars, editorFrameZoom,
                                        editorFrameCenter);
        if (crop.isEmpty())
            return;
        const QPointF shiftedCenter = crop.center()
            - QPointF(delta.x() * crop.width(), delta.y() * crop.height());
        editorFrameCenter = QPointF(
            qBound(0.0, shiftedCenter.x() / editorSourceImage.width(), 1.0),
            qBound(0.0, shiftedCenter.y() / editorSourceImage.height(), 1.0));
        syncEditorFraming();
    });
    connect(editor, &SstvComposerCanvas::backgroundZoomRequested, panel,
            [syncEditorFraming, zoomEditorFraming](qreal factor,
                                                   const QPointF &anchor) {
        zoomEditorFraming(factor, anchor);
        syncEditorFraming();
    });
    connect(drawButton, &QPushButton::clicked, editor, [editor]() {
        editor->setTool(SstvComposerCanvas::Tool::Draw);
    });
    const auto activateEditorShape = [editor](SstvComposerCanvas::ShapeType type) {
        editor->setShapeType(type);
        editor->setTool(SstvComposerCanvas::Tool::Shape);
    };
    connect(shapeButton, &QPushButton::clicked, editor, [activateEditorShape]() {
        activateEditorShape(SstvComposerCanvas::ShapeType::Line);
    });
    connect(arrowButton, &QPushButton::clicked, editor, [activateEditorShape]() {
        activateEditorShape(SstvComposerCanvas::ShapeType::Arrow);
    });
    connect(rectangleButton, &QPushButton::clicked, editor, [activateEditorShape]() {
        activateEditorShape(SstvComposerCanvas::ShapeType::Rectangle);
    });
    connect(ellipseButton, &QPushButton::clicked, editor, [activateEditorShape]() {
        activateEditorShape(SstvComposerCanvas::ShapeType::Ellipse);
    });
    connect(textButton, &QPushButton::clicked, this,
            [this, editor, &editorFillColor, fontFromEditorControls]() {
        bool accepted = false;
        const bool editing = editor->hasSelectedText();
        const QString initial = editing ? editor->selectedText()
                                        : (m_operatorCallsign.isEmpty()
                                               ? QStringLiteral("TEXT") : m_operatorCallsign);
        const QString text = promptSstvTxText(
            this, editing ? QStringLiteral("Edit template text")
                          : QStringLiteral("Add template text"),
            QStringLiteral("Text to transmit:"), initial, true, &accepted, true);
        if (!accepted || text.trimmed().isEmpty())
            return;
        if (editing)
            editor->updateSelectedText(text, fontFromEditorControls(), editorFillColor);
        else
            editor->addTextBlock(text, fontFromEditorControls(), editorFillColor);
    });
    connect(myCallButton, &QPushButton::clicked, editor,
            [editor, &editorFillColor, fontFromEditorControls]() {
        editor->addTextBlock(SstvComposerCanvas::myCallToken(),
                             fontFromEditorControls(), editorFillColor);
        editor->setTool(SstvComposerCanvas::Tool::Select);
    });
    connect(toCallButton, &QPushButton::clicked, editor,
            [editor, &editorFillColor, fontFromEditorControls]() {
        editor->addTextBlock(SstvComposerCanvas::toCallToken(),
                             fontFromEditorControls(), editorFillColor);
        editor->setTool(SstvComposerCanvas::Tool::Select);
    });
    connect(deleteButton, &QPushButton::clicked, editor,
            &SstvComposerCanvas::deleteSelectedObject);
    connect(rotateLeftButton, &QPushButton::clicked, editor,
            [editor]() { editor->rotateSelectedObject(-45); });
    connect(rotateRightButton, &QPushButton::clicked, editor,
            [editor]() { editor->rotateSelectedObject(45); });
    connect(undoButton, &QPushButton::clicked, editor, &SstvComposerCanvas::undo);
    connect(redoButton, &QPushButton::clicked, editor, &SstvComposerCanvas::redo);
    connect(resetButton, &QPushButton::clicked, editor,
            &SstvComposerCanvas::resetComposition);
    connect(fontButton, &QPushButton::clicked, this,
            [this, editor, fontButton, &editorFontIndex, fontFromEditorControls]() {
        const int selectedIndex = promptSstvFont(this, m_fontCombo, editorFontIndex);
        if (selectedIndex < 0)
            return;
        editorFontIndex = selectedIndex;
        fontButton->setText(m_fontCombo->itemText(editorFontIndex));
        editor->updateSelectedTextFont(fontFromEditorControls());
    });
    connect(sizeSlider, &QSlider::valueChanged, editor,
            [editor, sizeLabel, &editorColor](int value) {
        sizeLabel->setText(QStringLiteral("%1 PX").arg(value));
        editor->setInk(editorColor, qMax(2, value / 5));
        editor->updateSelectedSize(value);
    });
    connect(colorButton, &QPushButton::clicked, this,
            [this, editor, colorButton, &editorColor, sizeSlider]() {
        const QColor selected = promptSstvMarkupColor(
            this, editorColor, QStringLiteral("OBJECT OUTLINE COLOR"), true);
        if (!selected.isValid())
            return;
        editorColor = selected;
        editor->setInk(editorColor, qMax(2, sizeSlider->value() / 5));
        editor->updateSelectedOutlineColor(editorColor);
        colorButton->setIcon(sstvGlyph(SstvGlyph::Color, editorColor));
        colorButton->setToolTip(
            editorColor.alpha() == 0
                ? QStringLiteral("Object outline: NO OUTLINE")
                : QStringLiteral("Object outline: %1")
                      .arg(editorColor.name().toUpper()));
    });
    connect(fillColorButton, &QPushButton::clicked, this,
            [this, editor, fillColorButton, &editorFillColor]() {
        const QColor selected = promptSstvMarkupColor(
            this, editorFillColor, QStringLiteral("FILL / LINE COLOR"), true);
        if (!selected.isValid())
            return;
        editorFillColor = selected;
        editor->setFillColor(selected);
        editor->updateSelectedFillColor(selected);
        fillColorButton->setIcon(sstvGlyph(SstvGlyph::Color, selected));
        fillColorButton->setToolTip(
            selected.alpha() == 0 ? QStringLiteral("Object fill / line color: TRANSPARENT")
                                  : QStringLiteral("Object fill / line color: %1")
                                        .arg(selected.name().toUpper()));
    });
    connect(editor, &SstvComposerCanvas::selectionChanged, panel,
            [this, editor, fontButton, sizeSlider, sizeLabel, colorButton, fillColorButton,
             textButton, deleteButton, rotateLeftButton, rotateRightButton,
             &editorFontIndex, &editorColor, &editorFillColor](bool selected) {
        textButton->setText(editor->hasSelectedText() ? QStringLiteral("EDIT TEXT")
                                                      : QStringLiteral("ADD TEXT"));
        deleteButton->setEnabled(selected);
        rotateLeftButton->setEnabled(selected);
        rotateRightButton->setEnabled(selected);
        if (!selected)
            return;
        const QColor selectedOutline = editor->selectedOutlineColor();
        if (selectedOutline.isValid()) {
            editorColor = selectedOutline;
            colorButton->setIcon(sstvGlyph(SstvGlyph::Color, editorColor));
            colorButton->setToolTip(
                editorColor.alpha() == 0
                    ? QStringLiteral("Object outline: NO OUTLINE")
                    : QStringLiteral("Object outline: %1")
                          .arg(editorColor.name().toUpper()));
        }
        if (editor->selectedObjectSupportsFill()) {
            editorFillColor = editor->selectedFillColor();
            fillColorButton->setIcon(sstvGlyph(SstvGlyph::Color, editorFillColor));
            fillColorButton->setToolTip(
                editorFillColor.alpha() == 0
                    ? QStringLiteral("Object fill / line color: TRANSPARENT")
                    : QStringLiteral("Object fill / line color: %1")
                          .arg(editorFillColor.name().toUpper()));
        }
        const int selectedSize = editor->selectedSize();
        if (selectedSize > 0) {
            const QSignalBlocker blocker(sizeSlider);
            sizeSlider->setValue(qBound(sizeSlider->minimum(), selectedSize,
                                        sizeSlider->maximum()));
            sizeLabel->setText(QStringLiteral("%1 PX").arg(sizeSlider->value()));
        }
        if (!editor->hasSelectedText())
            return;
        const QFont font = editor->selectedTextFont();
        for (int index = 0; index < m_fontCombo->count(); ++index) {
            const QVariantMap descriptor = m_fontCombo->itemData(index).toMap();
            if (descriptor.value(QStringLiteral("family")).toString().compare(
                    font.family(), Qt::CaseInsensitive) == 0
                && descriptor.value(QStringLiteral("weight")).toInt() == font.weight()
                && descriptor.value(QStringLiteral("italic")).toBool() == font.italic()
                && descriptor.value(QStringLiteral("stretch")).toInt() == font.stretch()) {
                editorFontIndex = index;
                fontButton->setText(m_fontCombo->itemText(editorFontIndex));
                break;
            }
        }
    });
    deleteButton->setEnabled(false);
    rotateLeftButton->setEnabled(false);
    rotateRightButton->setEnabled(false);
    const auto updateEditorHistory = [editor, undoButton, redoButton]() {
        undoButton->setEnabled(editor->canUndo());
        redoButton->setEnabled(editor->canRedo());
    };
    connect(editor, &SstvComposerCanvas::compositionChanged, panel, updateEditorHistory);
    updateEditorHistory();
    editor->setInk(editorColor, qMax(2, sizeSlider->value() / 5));
    editor->setFillColor(editorFillColor);

    auto *dialogButtons = new QHBoxLayout;
    dialogButtons->setSpacing(10);
    auto *cancelButton = new QPushButton(QStringLiteral("CANCEL"), panel);
    auto *saveButton = new QPushButton(QStringLiteral("SAVE TEMPLATE"), panel);
    saveButton->setIcon(sstvGlyph(SstvGlyph::Save));
    saveButton->setIconSize(QSize(13, 13));
    for (QPushButton *button : {cancelButton, saveButton}) {
        QFont compactFont = button->font();
        compactFont.setPixelSize(11);
        button->setFont(compactFont);
        button->setFixedHeight(34);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
    cancelButton->setStyleSheet(
        buttonStyle(QStringLiteral("#6dd4ef"))
        + QStringLiteral("QPushButton { font-size: 11px; padding: 4px 8px; }"));
    saveButton->setStyleSheet(
        buttonStyle(QStringLiteral("#f2ad20"))
        + QStringLiteral("QPushButton { font-size: 11px; padding: 4px 8px; }"));
    connect(cancelButton, &QPushButton::clicked, &dialog, &InWindowDialog::reject);
    connect(saveButton, &QPushButton::clicked, &dialog, &InWindowDialog::accept);
    dialogButtons->addWidget(cancelButton);
    dialogButtons->addWidget(saveButton);
    layout->addLayout(dialogButtons);

    const auto reflowEditor = [editor, controlPanel, workspaceLayout, objectRow,
                               objectRowBalance, resetButton,
                               reflowFormatGrid](const QSize &size) {
        const bool landscape = size.width() > size.height();
        reflowFormatGrid(!landscape);
        workspaceLayout->setDirection(landscape ? QBoxLayout::LeftToRight
                                                : QBoxLayout::TopToBottom);
        workspaceLayout->setStretch(0, landscape ? 3 : 1);
        workspaceLayout->setStretch(1, landscape ? 2 : 0);
        controlPanel->setMinimumWidth(landscape ? 300 : 0);
        controlPanel->setMaximumWidth(landscape ? 390 : QWIDGETSIZE_MAX);
        controlPanel->setSizePolicy(landscape ? QSizePolicy::Preferred
                                              : QSizePolicy::Expanding,
                                    QSizePolicy::Preferred);
        objectRowBalance->changeSize(landscape ? 0
                                              : resetButton->width() + objectRow->spacing(),
                                     0, QSizePolicy::Fixed, QSizePolicy::Minimum);
        objectRow->invalidate();
        editor->setMinimumHeight(landscape ? 150 : 180);
        editor->setMaximumHeight(QWIDGETSIZE_MAX);
        editor->updateGeometry();
        controlPanel->updateGeometry();
    };
    connect(&dialog, &InWindowDialog::panelResized, panel, reflowEditor);

    // InWindowDialog clamps this preferred size to the current orientation and
    // re-clamps it whenever Android reports an orientation resize.
    dialog.setPanelSize(QSize(1600, 1500));
    if (dialog.exec() != InWindowDialog::Accepted)
        return;

    QJsonObject updatedState{
        {QStringLiteral("composition"), editor->compositionState()}};
    if (includeImageCheck->isChecked()) {
        updatedState.insert(QStringLiteral("fitBars"), editorFitBars);
        updatedState.insert(QStringLiteral("frameZoom"), editorFrameZoom);
        updatedState.insert(QStringLiteral("frameCenterX"), editorFrameCenter.x());
        updatedState.insert(QStringLiteral("frameCenterY"), editorFrameCenter.y());
    }
    if (!m_storage.saveUserTemplate(
            storageName, updatedState,
            includeImageCheck->isChecked() ? editorSourceImage : QImage(), &error)) {
        m_txStateLabel->setText(QStringLiteral("TEMPLATE UPDATE FAILED • %1").arg(error));
        return;
    }
    m_editingTemplateName.clear();
    if (includeImageCheck->isChecked()) {
        m_sourceImage = editorSourceImage;
        m_draftSourceDirty = true;
        m_fitBars = editorFitBars;
        m_fitModeButton->setText(m_fitBars ? QStringLiteral("FIT / BARS")
                                          : QStringLiteral("FILL / CROP"));
        m_frameZoom = editorFrameZoom;
        m_frameCenter = editorFrameCenter;
        {
            const QSignalBlocker blocker(m_frameZoomSlider);
            m_frameZoomSlider->setValue(qRound(m_frameZoom * 100.0));
        }
        m_frameZoomLabel->setText(
            QStringLiteral("%1×").arg(m_frameZoom, 0, 'f', 1));
        refreshModeFrame();
    }
    m_composer->restoreCompositionState(editor->compositionState());
    if (userTemplate)
        refreshTemplates(name);
    else
        refreshTemplates();
    const QString savedKind = includeImageCheck->isChecked()
                                  ? QStringLiteral("IMAGE INCLUDED")
                                  : QStringLiteral("LAYOUT ONLY");
    m_txStateLabel->setText(
        userTemplate
            ? QStringLiteral("TEMPLATE UPDATED • %1 • %2 • LOADED FOR REVIEW")
                  .arg(name, savedKind)
            : QStringLiteral("DEFAULT %1 SAVED • %2 • LOADED FOR REVIEW")
                  .arg(name, savedKind));
}

void SstvScreen::deleteUserTemplate() {
    const QString key = m_templateCombo->currentData().toString();
    if (!key.startsWith(QStringLiteral("user:")))
        return;
    const QString name = key.mid(5);
    if (!askSstvQuestion(this, QStringLiteral("Delete SSTV template"),
                           QStringLiteral("Delete the user template %1?").arg(name),
                           QStringLiteral("DELETE")))
        return;
    QString error;
    if (!m_storage.removeUserTemplate(name, &error))
        m_txStateLabel->setText(QStringLiteral("TEMPLATE DELETE FAILED • %1").arg(error));
    if (m_editingTemplateName == name)
        m_editingTemplateName.clear();
    refreshTemplates();
}

void SstvScreen::resetUserTemplates() {
    if (!askSstvQuestion(this, QStringLiteral("Reset SSTV templates"),
                           QStringLiteral("Delete all user templates and restore the factory CQ, REPORT, and 73 defaults?"),
                           QStringLiteral("RESET")))
        return;
    QString error;
    if (!m_storage.resetUserTemplates(&error))
        m_txStateLabel->setText(QStringLiteral("TEMPLATE RESET FAILED • %1").arg(error));
    else
        m_editingTemplateName.clear();
    refreshTemplates();
}

void SstvScreen::updateTemplateActionUi() {
    if (!m_templateCombo || !m_editTemplateButton || !m_deleteTemplateButton)
        return;
    const QString key = m_templateCombo->currentData().toString();
    const bool userTemplate = key.startsWith(QStringLiteral("user:"));
    bool includesImage = false;
    const QString storedName = userTemplate ? key.mid(5)
                                            : sstvBuiltinOverrideName(key);
    if (!storedName.isEmpty()
        && m_storage.userTemplateNames().contains(storedName)) {
        QJsonObject state;
        includesImage = m_storage.loadUserTemplate(storedName, &state)
            && !state.value(QStringLiteral("sourceFile")).toString().isEmpty();
    }
    const bool controlsEnabled = !m_transmitting && !m_mediaRequestPending;
    m_deleteTemplateButton->setEnabled(controlsEnabled && userTemplate);
    m_editTemplateButton->setEnabled(
        controlsEnabled && (!m_modeFrame.isNull() || includesImage));
    m_editTemplateButton->setIcon(sstvGlyph(SstvGlyph::Edit));
    const QString action = userTemplate
        ? QStringLiteral("Open selected user template editor")
        : QStringLiteral("Edit and overwrite this default template");
    m_editTemplateButton->setToolTip(action);
    m_editTemplateButton->setAccessibleName(action);
}

QJsonObject SstvScreen::currentTxState() const {
    return QJsonObject{{QStringLiteral("modeId"), m_modeCombo->currentData().toInt()},
                       {QStringLiteral("fitBars"), m_fitBars},
                       {QStringLiteral("frameZoom"), m_frameZoom},
                       {QStringLiteral("frameCenterX"), m_frameCenter.x()},
                       {QStringLiteral("frameCenterY"), m_frameCenter.y()},
                       {QStringLiteral("composition"), m_composer->compositionState()}};
}

void SstvScreen::restoreTxState(const QImage &source, const QJsonObject &state,
                                const QString &status) {
    if (source.isNull())
        return;
    m_restoringDraft = true;
    m_sourceImage = source;
    m_draftSourceDirty = true;
    const QSignalBlocker modeBlocker(m_modeCombo);
    const int modeIndex = m_modeCombo->findData(state.value(QStringLiteral("modeId")).toInt());
    if (modeIndex >= 0)
        m_modeCombo->setCurrentIndex(modeIndex);
    m_fitBars = state.value(QStringLiteral("fitBars")).toBool(false);
    m_fitModeButton->setText(m_fitBars ? QStringLiteral("FIT / BARS")
                                      : QStringLiteral("FILL / CROP"));
    m_frameZoom = qBound(1.0, state.value(QStringLiteral("frameZoom")).toDouble(1.0), 4.0);
    m_frameCenter = QPointF(
        qBound(0.0, state.value(QStringLiteral("frameCenterX")).toDouble(0.5), 1.0),
        qBound(0.0, state.value(QStringLiteral("frameCenterY")).toDouble(0.5), 1.0));
    {
        const QSignalBlocker zoomBlocker(m_frameZoomSlider);
        m_frameZoomSlider->setValue(qRound(m_frameZoom * 100.0));
    }
    m_frameZoomLabel->setText(QStringLiteral("%1×").arg(m_frameZoom, 0, 'f', 1));
    refreshModeFrame();
    m_composer->restoreCompositionState(state.value(QStringLiteral("composition")).toObject());
    m_restoringDraft = false;
    cancelTransmitConfirmation();
    m_txStateLabel->setText(status);
    scheduleDraftSave();
    updateTransmitUi();
}

bool SstvScreen::saveCurrentImageTemplate(const QString &name, QString *error) {
    if (m_sourceImage.isNull() || !m_composer->hasBackground()) {
        if (error)
            *error = QStringLiteral("Select or photograph an image first.");
        return false;
    }
    QImage source = m_sourceImage;
    if (qMax(source.width(), source.height()) > 2048)
        source = source.scaled(QSize(2048, 2048), Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
    QImage preview = m_composer->renderedImage();
    if (qMax(preview.width(), preview.height()) > 640)
        preview = preview.scaled(QSize(640, 640), Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation);
    return m_storage.saveImageTemplate(name, source, preview, currentTxState(), error);
}

bool SstvScreen::loadImageTemplate(const QString &name, QString *error) {
    QImage source;
    QJsonObject state;
    if (!m_storage.loadImageTemplate(name, &source, nullptr, &state, error))
        return false;
    restoreTxState(source, state, QStringLiteral("IMAGE TEMPLATE LOADED • %1 • EDIT BEFORE SENDING")
                                      .arg(name));
    return true;
}

void SstvScreen::openImageTemplateGallery() {
    InWindowDialog dialog(this);
    QWidget *panel = dialog.contentWidget();
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("SSTV IMAGE TEMPLATE GALLERY"), panel);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("color: white; font-size: 14px; font-weight: 700;"));
    layout->addWidget(title);
    auto *hint = new QLabel(
        QStringLiteral("Save and reopen complete editable CQ pictures, including crop, text, and drawing."),
        panel);
    hint->setWordWrap(true);
    hint->setMinimumHeight(30);
    hint->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
    hint->setStyleSheet(QStringLiteral("color: #cbd3d6; font-size: 10px;"));
    layout->addWidget(hint);

    auto *list = new QListWidget(panel);
    list->setViewMode(QListView::IconMode);
    list->setResizeMode(QListView::Adjust);
    list->setMovement(QListView::Static);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setWordWrap(true);
    list->setTextElideMode(Qt::ElideNone);
    list->setIconSize(QSize(140, 105));
    list->setGridSize(QSize(160, 142));
    list->setSpacing(6);
    list->setStyleSheet(QStringLiteral(
        "QListWidget { background: #171b1d; color: white; border: 1px solid #516067; "
        "border-radius: 5px; font-size: 11px; }"
        "QListWidget::item { padding: 4px; }"
        "QListWidget::item:selected { background: #245463; border: 2px solid #6dd4ef; }"));
    QString error;
    const QStringList names = m_storage.imageTemplateNames(&error);
    const QString currentKey = QStringLiteral("__current_tx_composition__");
    const bool hasCurrent = !m_sourceImage.isNull() && m_composer->hasBackground();
    if (hasCurrent) {
        auto *current = new QListWidgetItem(
            QIcon(QPixmap::fromImage(m_composer->renderedImage())),
            QStringLiteral("CURRENT TX\nCOMPOSITION"), list);
        current->setData(Qt::UserRole, currentKey);
        current->setTextAlignment(Qt::AlignHCenter);
    }
    for (const QString &name : names) {
        QImage preview;
        QString loadError;
        m_storage.loadImageTemplate(name, nullptr, &preview, nullptr, &loadError);
        auto *item = new QListWidgetItem(QIcon(QPixmap::fromImage(preview)), name, list);
        item->setData(Qt::UserRole, name);
        item->setTextAlignment(Qt::AlignHCenter);
    }
    if (list->count() == 0) {
        auto *empty = new QListWidgetItem(
            QStringLiteral("NO IMAGE TEMPLATES\nCompose an image on TX, then choose SAVE CURRENT."),
            list);
        empty->setFlags(Qt::NoItemFlags);
        empty->setTextAlignment(Qt::AlignCenter);
        empty->setSizeHint(QSize(320, 110));
    }
#ifdef Q_OS_ANDROID
    list->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    QScroller::grabGesture(list->viewport(), QScroller::TouchGesture);
#endif
    layout->addWidget(list, 1);

    auto *use = new QPushButton(QStringLiteral("USE SELECTED"), panel);
    auto *save = new QPushButton(QStringLiteral("SAVE CURRENT"), panel);
    auto *rename = new QPushButton(QStringLiteral("RENAME"), panel);
    auto *remove = new QPushButton(QStringLiteral("DELETE"), panel);
    auto *close = new QPushButton(QStringLiteral("CLOSE"), panel);
    for (QPushButton *button : {use, save, rename, remove, close}) {
        button->setMinimumHeight(42);
        button->setStyleSheet(buttonStyle(button == remove ? QStringLiteral("#e3262e")
                                                           : QStringLiteral("#6dd4ef")));
    }
    use->setEnabled(false);
    rename->setEnabled(false);
    remove->setEnabled(false);
    save->setEnabled(hasCurrent);
    auto *primaryRow = new QHBoxLayout;
    primaryRow->addWidget(use, 1);
    primaryRow->addWidget(save, 1);
    layout->addLayout(primaryRow);
    auto *manageRow = new QHBoxLayout;
    manageRow->addWidget(rename);
    manageRow->addWidget(remove);
    manageRow->addStretch();
    manageRow->addWidget(close);
    layout->addLayout(manageRow);

    QString selectedAction;
    QString selectedName;
    const auto updateGalleryActions = [list, use, rename, remove, currentKey]() {
        QListWidgetItem *item = list->currentItem();
        const bool selectable = item && item->flags().testFlag(Qt::ItemIsSelectable);
        const bool saved = selectable
                           && item->data(Qt::UserRole).toString() != currentKey;
        use->setEnabled(selectable);
        rename->setEnabled(saved);
        remove->setEnabled(saved);
    };
    connect(list, &QListWidget::currentItemChanged, &dialog,
            [updateGalleryActions](QListWidgetItem *, QListWidgetItem *) {
        updateGalleryActions();
    });
    if (hasCurrent || !names.isEmpty())
        list->setCurrentRow(0);
    updateGalleryActions();
    connect(use, &QPushButton::clicked, &dialog, [&]() {
        if (!list->currentItem())
            return;
        selectedName = list->currentItem()->data(Qt::UserRole).toString();
        if (selectedName != currentKey) {
            QString loadError;
            if (!loadImageTemplate(selectedName, &loadError)) {
                hint->setText(
                    QStringLiteral("IMAGE TEMPLATE LOAD FAILED • %1").arg(loadError));
                return;
            }
            selectedAction = QStringLiteral("used");
        } else {
            selectedAction = QStringLiteral("current");
        }
        dialog.accept();
    });
    connect(save, &QPushButton::clicked, &dialog, [&]() {
        bool accepted = false;
        const QString name = promptSstvTxText(this, QStringLiteral("Save image template"),
                                              QStringLiteral("Image template name:"), QString(),
                                              false, &accepted).trimmed();
        if (!accepted || name.isEmpty())
            return;
        if (names.contains(name, Qt::CaseInsensitive)
            && !askSstvQuestion(this, QStringLiteral("Overwrite image template"),
                                  QStringLiteral("Replace the saved image template named %1?").arg(name),
                                  QStringLiteral("OVERWRITE")))
            return;
        QString saveError;
        if (!saveCurrentImageTemplate(name, &saveError)) {
            m_txStateLabel->setText(QStringLiteral("IMAGE TEMPLATE SAVE FAILED • %1").arg(saveError));
            return;
        }
        selectedAction = QStringLiteral("saved");
        selectedName = name;
        dialog.accept();
    });
    connect(rename, &QPushButton::clicked, &dialog, [&]() {
        if (!list->currentItem())
            return;
        const QString oldName = list->currentItem()->data(Qt::UserRole).toString();
        if (oldName == currentKey)
            return;
        bool accepted = false;
        const QString newName = promptSstvTxText(this, QStringLiteral("Rename image template"),
                                                 QStringLiteral("New image template name:"), oldName,
                                                 false, &accepted).trimmed();
        if (!accepted || newName.isEmpty() || newName == oldName)
            return;
        if (names.contains(newName, Qt::CaseInsensitive)
            && !askSstvQuestion(this, QStringLiteral("Overwrite image template"),
                                  QStringLiteral("Replace the saved image template named %1?").arg(newName),
                                  QStringLiteral("OVERWRITE")))
            return;
        QImage source;
        QImage preview;
        QJsonObject state;
        QString renameError;
        const bool sameStorageKey = oldName.compare(newName, Qt::CaseInsensitive) == 0;
        if (!m_storage.loadImageTemplate(oldName, &source, &preview, &state, &renameError)
            || !m_storage.saveImageTemplate(newName, source, preview, state, &renameError)
            || (!sameStorageKey && !m_storage.removeImageTemplate(oldName, &renameError))) {
            m_txStateLabel->setText(QStringLiteral("IMAGE TEMPLATE RENAME FAILED • %1").arg(renameError));
            return;
        }
        selectedAction = QStringLiteral("renamed");
        selectedName = newName;
        dialog.accept();
    });
    connect(remove, &QPushButton::clicked, &dialog, [&]() {
        if (!list->currentItem())
            return;
        const QString name = list->currentItem()->data(Qt::UserRole).toString();
        if (name == currentKey)
            return;
        if (!askSstvQuestion(this, QStringLiteral("Delete image template"),
                               QStringLiteral("Delete the saved image template %1?").arg(name),
                               QStringLiteral("DELETE")))
            return;
        QString removeError;
        if (!m_storage.removeImageTemplate(name, &removeError)) {
            m_txStateLabel->setText(QStringLiteral("IMAGE TEMPLATE DELETE FAILED • %1").arg(removeError));
            return;
        }
        selectedAction = QStringLiteral("deleted");
        selectedName = name;
        dialog.accept();
    });
    connect(close, &QPushButton::clicked, &dialog, &InWindowDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, use, [use](QListWidgetItem *) {
        if (use->isEnabled())
            use->click();
    });

    // The shared in-window dialog clamps and reflows this preferred size on
    // every Android orientation resize.
    dialog.setPanelSize(QSize(1400, 1200));
    dialog.exec();
    if (selectedAction == QStringLiteral("saved")) {
        m_txStateLabel->setText(QStringLiteral("IMAGE TEMPLATE SAVED • %1").arg(selectedName));
    } else if (selectedAction == QStringLiteral("current")) {
        m_txStateLabel->setText(QStringLiteral("CURRENT TX COMPOSITION RETAINED • READY TO EDIT"));
    } else if (selectedAction == QStringLiteral("renamed")) {
        m_txStateLabel->setText(QStringLiteral("IMAGE TEMPLATE RENAMED • %1").arg(selectedName));
    } else if (selectedAction == QStringLiteral("deleted")) {
        m_txStateLabel->setText(QStringLiteral("IMAGE TEMPLATE DELETED • %1").arg(selectedName));
    } else if (!error.isEmpty()) {
        m_txStateLabel->setText(QStringLiteral("IMAGE TEMPLATE WARNING • %1").arg(error));
    }
}

void SstvScreen::scheduleDraftSave() {
    if (!m_restoringDraft && m_draftTimer && !m_sourceImage.isNull())
        m_draftTimer->start();
}

void SstvScreen::saveDraft() {
    if (m_sourceImage.isNull() || !m_composer->hasBackground())
        return;
    const QJsonObject state = currentTxState();
    QString error;
    QImage draftSource = m_sourceImage;
    if (qMax(draftSource.width(), draftSource.height()) > 2048)
        draftSource = draftSource.scaled(QSize(2048, 2048), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (!m_storage.saveDraft(draftSource, state, &error, m_draftSourceDirty))
        m_txStateLabel->setText(QStringLiteral("DRAFT SAVE WARNING • %1").arg(error));
    else
        m_draftSourceDirty = false;
}

void SstvScreen::restoreDraft() {
    QImage source;
    QJsonObject state;
    QString error;
    if (!m_storage.loadDraft(&source, &state, &error)) {
        if (!error.isEmpty())
            m_txStateLabel->setText(QStringLiteral("DRAFT RECOVERY WARNING • %1").arg(error));
        return;
    }
    restoreTxState(source, state, QStringLiteral("TX DRAFT RESTORED • REVIEW BEFORE PREVIEW"));
    // Loading the recovery draft does not make its source newly dirty.
    m_draftSourceDirty = false;
}

void SstvScreen::clearDraft() {
    if (!askSstvQuestion(this, QStringLiteral("Clear TX draft"),
                           QStringLiteral("Delete the saved recovery draft? The current on-screen composition will remain."),
                           QStringLiteral("CLEAR")))
        return;
    m_draftTimer->stop();
    QString error;
    if (!m_storage.clearDraft(&error))
        m_txStateLabel->setText(QStringLiteral("CLEAR DRAFT FAILED • %1").arg(error));
    else
        m_txStateLabel->setText(QStringLiteral("SAVED TX DRAFT CLEARED"));
    m_draftSourceDirty = true;
}

void SstvScreen::flushDraft() {
    if (m_draftTimer && m_draftTimer->isActive()) {
        m_draftTimer->stop();
        saveDraft();
    }
}

void SstvScreen::updateComposerControls() {
    if (!m_composer)
        return;
    const bool enabled = !m_transmitting && !m_mediaRequestPending;
    m_selectToolButton->setStyleSheet(buttonStyle(QStringLiteral("#6dd4ef"),
                                                   m_composer->tool() == SstvComposerCanvas::Tool::Select));
    m_drawToolButton->setStyleSheet(buttonStyle(QStringLiteral("#6dd4ef"),
                                                 m_composer->tool() == SstvComposerCanvas::Tool::Draw));
    const bool shapeTool = m_composer->tool() == SstvComposerCanvas::Tool::Shape;
    const auto shapeStyle = [this, shapeTool](QPushButton *button,
                                              SstvComposerCanvas::ShapeType type) {
        button->setStyleSheet(buttonStyle(QStringLiteral("#6dd4ef"),
                                          shapeTool && m_composer->shapeType() == type));
    };
    shapeStyle(m_shapeToolButton, SstvComposerCanvas::ShapeType::Line);
    shapeStyle(m_arrowToolButton, SstvComposerCanvas::ShapeType::Arrow);
    shapeStyle(m_rectangleToolButton, SstvComposerCanvas::ShapeType::Rectangle);
    shapeStyle(m_ellipseToolButton, SstvComposerCanvas::ShapeType::Ellipse);
    m_undoButton->setEnabled(enabled && m_composer->canUndo());
    m_redoButton->setEnabled(enabled && m_composer->canRedo());
    m_textButton->setText(m_composer->hasSelectedText() ? QStringLiteral("EDIT TEXT")
                                                        : QStringLiteral("ADD TEXT"));
    const bool selected = m_composer->hasSelectedObject();
    m_deleteObjectButton->setEnabled(enabled && selected);
    m_rotateObjectLeftButton->setEnabled(enabled && selected);
    m_rotateObjectRightButton->setEnabled(enabled && selected);
}

void SstvScreen::cancelTransmitConfirmation() {
    m_frozenTransmitFrame = QImage();
    if (!m_transmitConfirmationPending)
        return;
    m_transmitConfirmationPending = false;
    if (!m_transmitting)
        m_txStateLabel->setText(QStringLiteral("PREPARE AN IMAGE"));
    updateTransmitUi();
}

void SstvScreen::updateCallsignControlsLayout(bool portrait) {
    if (!m_callsignRowLayout || !m_callsignIdRowLayout || !m_callsignIdRowContainer
        || !m_callsignEdit || !m_fskIdCheck || !m_cwIdCheck || !m_cwWpmMinus
        || !m_cwWpmSpin || !m_cwWpmPlus) {
        return;
    }

    const int requestedOrientation = portrait ? 1 : 0;
    if (m_callsignControlsPortrait == requestedOrientation)
        return;

    for (QWidget *widget : {static_cast<QWidget *>(m_callsignEdit),
                            static_cast<QWidget *>(m_fskIdCheck),
                            static_cast<QWidget *>(m_cwIdCheck),
                            static_cast<QWidget *>(m_cwWpmMinus),
                            static_cast<QWidget *>(m_cwWpmSpin),
                            static_cast<QWidget *>(m_cwWpmPlus)}) {
        m_callsignRowLayout->removeWidget(widget);
        m_callsignIdRowLayout->removeWidget(widget);
    }

    if (portrait) {
        // Preserve the original portrait presentation: callsign, ID options,
        // and CW speed remain together on one line.
        m_callsignRowLayout->addWidget(m_callsignEdit, 1);
        m_callsignRowLayout->addWidget(m_fskIdCheck);
        m_callsignRowLayout->addWidget(m_cwIdCheck);
        m_callsignRowLayout->addWidget(m_cwWpmMinus);
        m_callsignRowLayout->addWidget(m_cwWpmSpin);
        m_callsignRowLayout->addWidget(m_cwWpmPlus);
        m_callsignIdRowContainer->hide();
    } else {
        // The landscape controls pane is narrower. Keep MY CALL unobstructed
        // and place both ID options and the complete speed control below it.
        m_callsignRowLayout->addWidget(
            m_callsignEdit, 0, Qt::AlignLeft | Qt::AlignVCenter);
        m_callsignIdRowLayout->addWidget(m_fskIdCheck);
        m_callsignIdRowLayout->addWidget(m_cwIdCheck);
        m_callsignIdRowLayout->addWidget(m_cwWpmMinus);
        m_callsignIdRowLayout->addWidget(m_cwWpmSpin);
        m_callsignIdRowLayout->addWidget(m_cwWpmPlus);
        m_callsignIdRowContainer->show();
    }

    m_callsignControlsPortrait = requestedOrientation;
    m_callsignRowLayout->invalidate();
    m_callsignIdRowLayout->invalidate();
}

void SstvScreen::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (!m_transmitLayout)
        return;

    const bool portrait = event->size().height() > event->size().width();
    m_transmitLayout->setDirection(portrait ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    m_transmitLayout->setStretch(0, portrait ? 4 : 3);
    m_transmitLayout->setStretch(1, portrait ? 3 : 2);
    updateCallsignControlsLayout(portrait);
    if (!m_currentReceiveImage.isNull())
        m_receiveImage->setPixmap(QPixmap::fromImage(m_currentReceiveImage).scaled(
            m_receiveImage->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void SstvScreen::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Back || event->key() == Qt::Key_Escape) {
        if (m_transmitting)
            emit stopRequested();
        else {
            flushDraft();
            emit closeRequested();
        }
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}
