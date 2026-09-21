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
