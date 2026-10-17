#include "iosorientation.h"

#import <UIKit/UIKit.h>

#include <QDebug>
#include <QString>

namespace {
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
        UIWindowScene *scene = activeWindowScene();
        if (!scene)
            return;
        if (@available(iOS 16.0, *)) {
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
