#include "iosorientation.h"

#import <UIKit/UIKit.h>
#import <objc/runtime.h>

#include <QDebug>
#include <QString>

// Qt's iOS platform plugin (QIOSViewController/QIOSApplicationDelegate) does
// not override -supportedInterfaceOrientations or
// -application:supportedInterfaceOrientationsForWindow:, so without this
// category the app falls back to Info.plist's static UISupportedInterfaceOrientations
// list (which must list every orientation any screen uses, e.g. portrait for
// SSTV/FT8) with nothing to hold the radio console to landscape-only in
// between -- matching the reported regression: the console can be physically
// rotated to portrait at any time. This category adds the one hook iOS
// actually consults on every rotation, restoring the persistent lock that a
// one-shot geometry request alone cannot provide. Safe to add (not method
// swizzling/UB) because Qt's delegate class never implements this selector
// itself -- verified against Qt 6.11's qiosapplicationdelegate.mm.
@interface QIOSOrientationLockCategoryHost : NSObject
@end
@implementation QIOSOrientationLockCategoryHost
@end

namespace {
// Landscape is the default and the only orientation ever authorized for the
// main radio console per docs/ORIENTATION_POLICY.md; screens that need
// portrait (SSTV, FT8, some logbook entries) widen this via
// IosOrientation::requestPortrait()/allowPortraitAndLandscape() and narrow it
// back to requestLandscape() on exit, mirroring the Android call sites in
// src/mainwindow.cpp through src/android/sstvorientation.cpp.
UIInterfaceOrientationMask g_allowedMask = UIInterfaceOrientationMaskLandscape;

UIWindowScene *activeWindowScene() {
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
        if ([scene isKindOfClass:[UIWindowScene class]]
            && scene.activationState == UISceneActivationStateForegroundActive)
            return (UIWindowScene *)scene;
    }
    return nil;
}

void requestOrientations(UIInterfaceOrientationMask mask) {
    dispatch_async(dispatch_get_main_queue(), ^{
        g_allowedMask = mask;

        UIWindowScene *scene = activeWindowScene();
        if (!scene)
            return;

        // Force iOS to re-query -application:supportedInterfaceOrientationsForWindow:
        // (and any view controller in the chain) now that g_allowedMask changed;
        // without this the mask above only takes effect on the next rotation
        // iOS happens to re-evaluate on its own.
        if (@available(iOS 16.0, *)) {
            for (UIWindow *window in scene.windows) {
                if (window.rootViewController)
                    [window.rootViewController setNeedsUpdateOfSupportedInterfaceOrientations];
            }
            UIWindowSceneGeometryPreferencesIOS *preferences =
                [[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:mask];
            [scene requestGeometryUpdateWithPreferences:preferences errorHandler:^(NSError *error) {
                qWarning() << "IosOrientation: geometry update failed:"
                           << QString::fromNSString(error.localizedDescription);
            }];
        }
        [UIViewController attemptRotationToDeviceOrientation];
    });
}
}

// Adds the app-delegate hook iOS consults on every rotation attempt. Qt's own
// QIOSApplicationDelegate doesn't implement this selector (see block comment
// above), so this category is additive, not an override.
@interface NSObject (QK4IosOrientationLock)
- (UIInterfaceOrientationMask)application:(UIApplication *)application
    supportedInterfaceOrientationsForWindow:(UIWindow *)window;
@end

@implementation NSObject (QK4IosOrientationLock)
- (UIInterfaceOrientationMask)application:(UIApplication *)application
    supportedInterfaceOrientationsForWindow:(UIWindow *)window {
    Q_UNUSED(application)
    Q_UNUSED(window)
    return g_allowedMask;
}
@end

void IosOrientation::requestLandscape() {
    requestOrientations(UIInterfaceOrientationMaskLandscape);
}

void IosOrientation::requestPortrait() {
    requestOrientations(UIInterfaceOrientationMaskPortrait);
}

void IosOrientation::allowPortraitAndLandscape() {
    requestOrientations(UIInterfaceOrientationMaskAll);
}
