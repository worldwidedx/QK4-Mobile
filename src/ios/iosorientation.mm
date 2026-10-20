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
// portrait (SSTV, FT8, some logbook entries) change this through the
// IosOrientation functions. The radio logbook keeps landscape until a
// physical device turn and restores it on exit, matching the Android path.
UIInterfaceOrientationMask g_allowedMask = UIInterfaceOrientationMaskLandscape;
id g_radioLogbookObserver = nil;
bool g_radioLogbookActive = false;
UIDeviceOrientation g_initialRadioLogbookOrientation = UIDeviceOrientationUnknown;

void onMainThread(dispatch_block_t action) {
    if ([NSThread isMainThread])
        action();
    else
        dispatch_async(dispatch_get_main_queue(), action);
}

UIWindowScene *activeWindowScene() {
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
        if ([scene isKindOfClass:[UIWindowScene class]]
            && scene.activationState == UISceneActivationStateForegroundActive)
            return (UIWindowScene *)scene;
    }
    return nil;
}

bool activeSceneHasRegularSizeClasses() {
    UIWindowScene *scene = activeWindowScene();
    if (!scene)
        return false;
    UIWindow *activeWindow = nil;
    for (UIWindow *window in scene.windows) {
        if (window.isKeyWindow) {
            activeWindow = window;
            break;
        }
    }
    if (!activeWindow)
        activeWindow = scene.windows.firstObject;
    if (!activeWindow)
        return false;
    UITraitCollection *traits = activeWindow.traitCollection;
    return traits.horizontalSizeClass == UIUserInterfaceSizeClassRegular
           && traits.verticalSizeClass == UIUserInterfaceSizeClassRegular;
}

void applyOrientations(UIInterfaceOrientationMask mask) {
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
}

void requestOrientations(UIInterfaceOrientationMask mask) {
    onMainThread(^{ applyOrientations(mask); });
}

void stopRadioLogbookObservation() {
    if (g_radioLogbookObserver) {
        [[NSNotificationCenter defaultCenter] removeObserver:g_radioLogbookObserver];
        g_radioLogbookObserver = nil;
    }
    if (g_radioLogbookActive) {
        [[UIDevice currentDevice] endGeneratingDeviceOrientationNotifications];
        g_radioLogbookActive = false;
    }
    g_initialRadioLogbookOrientation = UIDeviceOrientationUnknown;
}
}

bool IosOrientation::activeWindowHasRegularSizeClasses() {
    return activeSceneHasRegularSizeClasses();
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

int IosWindow::controlsTopInset() {
    // iPadOS draws its window controls over the top-left of a windowed app and
    // does not report them as a safe-area inset, so hand-placed screens would
    // put their Back button underneath. A window that fills the screen has none.
    UIWindowScene *scene = activeWindowScene();
    UIWindow *window = scene.windows.firstObject;
    if (!window || !scene.screen)
        return 0;
    const CGSize win = window.bounds.size;
    const CGSize scr = scene.screen.bounds.size;
    const bool windowed = qAbs(win.width - scr.width) > 1.0 || qAbs(win.height - scr.height) > 1.0;
    return windowed ? 44 : 0;
}

void IosOrientation::beginRadioLogbookRotation() {
    onMainThread(^{
        stopRadioLogbookObservation();
        // Keep the entry landscape even if the device is already held upright.
        applyOrientations(UIInterfaceOrientationMaskLandscape);

        UIDevice *device = [UIDevice currentDevice];
        [device beginGeneratingDeviceOrientationNotifications];
        g_radioLogbookActive = true;
        const UIDeviceOrientation initial = device.orientation;
        if (UIDeviceOrientationIsValidInterfaceOrientation(initial))
            g_initialRadioLogbookOrientation = initial;

        g_radioLogbookObserver = [[NSNotificationCenter defaultCenter]
            addObserverForName:UIDeviceOrientationDidChangeNotification
                        object:device
                         queue:[NSOperationQueue mainQueue]
                    usingBlock:^(NSNotification *notification) {
            Q_UNUSED(notification)
            if (!g_radioLogbookActive || !g_radioLogbookObserver)
                return;
            const UIDeviceOrientation current = device.orientation;
            if (!UIDeviceOrientationIsValidInterfaceOrientation(current))
                return; // Flat/unknown readings are not a deliberate turn.
            if (g_initialRadioLogbookOrientation == UIDeviceOrientationUnknown) {
                g_initialRadioLogbookOrientation = current;
                return;
            }
            if (current == g_initialRadioLogbookOrientation)
                return;

            // Keep sensor updates active until the logbook closes so UIKit can
            // follow subsequent turns after portrait is allowed.
            [[NSNotificationCenter defaultCenter] removeObserver:g_radioLogbookObserver];
            g_radioLogbookObserver = nil;
            applyOrientations(UIInterfaceOrientationMaskAll);
        }];
    });
}

void IosOrientation::endRadioLogbookRotation() {
    onMainThread(^{
        stopRadioLogbookObservation();
        applyOrientations(UIInterfaceOrientationMaskLandscape);
    });
}
