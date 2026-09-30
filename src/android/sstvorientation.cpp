#include "sstvorientation.h"

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#elif defined(Q_OS_IOS)
#include "ios/iosorientation.h"
#endif

void setSstvOrientationEnabled(bool enabled) {
#ifdef Q_OS_ANDROID
    const jint requested = enabled ? 10 /* SCREEN_ORIENTATION_FULL_SENSOR */
                                   : 6  /* SCREEN_ORIENTATION_SENSOR_LANDSCAPE */;
    const QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (activity.isValid())
        activity.callMethod<void>("setRequestedOrientation", "(I)V", requested);
#elif defined(Q_OS_IOS)
    if (enabled)
        IosOrientation::allowPortraitAndLandscape();
    else
        IosOrientation::requestLandscape();
#else
    Q_UNUSED(enabled)
#endif
}

void setFt8PortraitEnabled(bool enabled) {
#ifdef Q_OS_ANDROID
    const QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (activity.isValid())
        activity.callMethod<void>("setRequestedOrientation", "(I)V",
                                  jint(enabled ? 1 /* PORTRAIT */ : 6 /* SENSOR_LANDSCAPE */));
#elif defined(Q_OS_IOS)
    if (enabled)
        IosOrientation::requestPortrait();
    else
        IosOrientation::requestLandscape();
#else
    Q_UNUSED(enabled)
#endif
}

void setRadioLogbookOrientationEnabled(bool enabled) {
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>("com/w9wdx/qk4phone/Qk4Activity",
                                     "setRadioLogbookRotation", "(Z)V", jboolean(enabled));
#elif defined(Q_OS_IOS)
    if (enabled)
        IosOrientation::allowPortraitAndLandscape();
    else
        IosOrientation::requestLandscape();
#else
    Q_UNUSED(enabled)
#endif
}
