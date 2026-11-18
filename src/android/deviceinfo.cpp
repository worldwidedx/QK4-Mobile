#include "deviceinfo.h"
#include <QtGlobal>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

bool AndroidDeviceInfo::hasFoldingHardware() {
#ifdef Q_OS_ANDROID
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return false;

    const QJniObject packageManager = context.callObjectMethod(
        "getPackageManager", "()Landroid/content/pm/PackageManager;");
    const QJniObject feature = QJniObject::fromString("android.hardware.sensor.hinge_angle");
    if (packageManager.isValid()
        && packageManager.callMethod<jboolean>("hasSystemFeature", "(Ljava/lang/String;)Z", feature.object()))
        return true;

    // Some manufacturers expose the sensor without declaring the package
    // feature. Merely query its presence; do not register a listener or read
    // the angle (which would make startup depend on the current posture).
    const QJniObject service = QJniObject::fromString("sensor");
    const QJniObject sensors = context.callObjectMethod(
        "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;", service.object());
    if (sensors.isValid()) {
        // Sensor.TYPE_HINGE_ANGLE = 36, introduced in API 30. Earlier devices
        // return null for this unsupported sensor type; getDefaultSensor itself
        // is available throughout our API 26+ range.
        return sensors.callObjectMethod("getDefaultSensor", "(I)Landroid/hardware/Sensor;", jint(36)).isValid();
    }
#endif
    return false;
}
