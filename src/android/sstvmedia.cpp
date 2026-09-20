#include "sstvmedia.h"

#ifdef Q_OS_ANDROID
#include <QJniObject>

namespace {
bool invokeRequest(const char *method, QString *error) {
    const bool started = QJniObject::callStaticMethod<jboolean>(
        "com/w9wdx/qk4phone/Qk4Activity", method, "()Z");
    if (!started && error)
        *error = QStringLiteral("Android could not open the requested image source.");
    return started;
}
}

namespace SstvMedia {
bool openGallery(QString *error) { return invokeRequest("openSstvGallery", error); }
bool openCamera(QString *error) { return invokeRequest("openSstvCamera", error); }

bool shareImage(const QString &path, const QString &title, QString *error) {
    const QJniObject javaPath = QJniObject::fromString(path);
    const QJniObject javaTitle = QJniObject::fromString(title);
    const bool shared = QJniObject::callStaticMethod<jboolean>(
        "com/w9wdx/qk4phone/Qk4Activity", "shareSstvImage",
        "(Ljava/lang/String;Ljava/lang/String;)Z", javaPath.object<jstring>(), javaTitle.object<jstring>());
    if (!shared && error)
        *error = QStringLiteral("Android could not share the selected SSTV image.");
    return shared;
}

QString takeCompletedImagePath() {
    const QJniObject path = QJniObject::callStaticObjectMethod(
        "com/w9wdx/qk4phone/Qk4Activity", "takeSstvImportedImagePath", "()Ljava/lang/String;");
    return path.isValid() ? path.toString() : QString();
}

bool isOperationActive() {
    return QJniObject::callStaticMethod<jboolean>(
        "com/w9wdx/qk4phone/Qk4Activity", "isSstvMediaOperationActive", "()Z");
}

QString takeOperationError() {
    const QJniObject error = QJniObject::callStaticObjectMethod(
        "com/w9wdx/qk4phone/Qk4Activity", "takeSstvMediaError", "()Ljava/lang/String;");
    return error.isValid() ? error.toString() : QString();
}
}
#else
namespace SstvMedia {
bool openGallery(QString *error) {
    if (error)
        *error = QStringLiteral("The system photo picker is available on Android only.");
    return false;
}
bool openCamera(QString *error) {
    if (error)
        *error = QStringLiteral("The system camera is available on Android only.");
    return false;
}
bool shareImage(const QString &, const QString &, QString *error) {
    if (error)
        *error = QStringLiteral("Android sharing is available on Android only.");
    return false;
}
QString takeCompletedImagePath() { return QString(); }
bool isOperationActive() { return false; }
QString takeOperationError() { return QString(); }
}
#endif
