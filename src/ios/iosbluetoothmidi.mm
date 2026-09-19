#include "iosbluetoothmidi.h"

#import <CoreAudioKit/CoreAudioKit.h>
#import <CoreMIDI/CoreMIDI.h>
#import <UIKit/UIKit.h>

#include <QDebug>

namespace {

UIWindowScene *activeWindowScene() {
    for (UIScene *scene in UIApplication.sharedApplication.connectedScenes) {
        if ([scene isKindOfClass:[UIWindowScene class]]
            && scene.activationState == UISceneActivationStateForegroundActive)
            return (UIWindowScene *)scene;
    }
    return nil;
}

// Qt owns the UIWindow and installs its own root view controller. Presenting
// on a controller that is already presenting silently does nothing, which
// would read as a dead button, so walk to the topmost presented controller.
UIViewController *topPresenter() {
    UIWindowScene *scene = activeWindowScene();
    if (!scene)
        return nil;

    UIWindow *window = nil;
    for (UIWindow *candidate in scene.windows) {
        if (candidate.isKeyWindow) {
            window = candidate;
            break;
        }
    }
    if (!window)
        window = scene.windows.firstObject;
    if (!window)
        return nil;

    UIViewController *controller = window.rootViewController;
    while (controller.presentedViewController)
        controller = controller.presentedViewController;
    return controller;
}

NSString *stringProperty(MIDIObjectRef object, CFStringRef property) {
    if (object == 0)
        return nil;
    CFStringRef value = NULL;
    if (MIDIObjectGetStringProperty(object, property, &value) != noErr || value == NULL)
        return nil;
    return (__bridge_transfer NSString *)value;
}

// RtMidi composes a port name from the endpoint, entity and device names, so
// match against each form it can produce rather than assuming RtMidi's port
// index matches MIDIGetSource ordering. Coupling display text to the vendored
// backend's indexing would break silently if that indexing ever changed.
bool sourceMatchesName(MIDIEndpointRef source, NSString *target, MIDIDeviceRef *outDevice) {
    NSString *endpointName = [stringProperty(source, kMIDIPropertyName)
        stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];

    MIDIEntityRef entity = 0;
    MIDIEndpointGetEntity(source, &entity);
    MIDIDeviceRef device = 0;
    if (entity != 0)
        MIDIEntityGetDevice(entity, &device);
    if (outDevice)
        *outDevice = device;

    NSString *entityName = stringProperty(entity, kMIDIPropertyName);
    NSString *deviceName = stringProperty(device, kMIDIPropertyName);

    NSMutableArray<NSString *> *candidates = [NSMutableArray array];
    if (endpointName.length > 0)
        [candidates addObject:endpointName];
    if (entityName.length > 0)
        [candidates addObject:entityName];
    if (deviceName.length > 0)
        [candidates addObject:deviceName];
    if (deviceName.length > 0 && endpointName.length > 0
        && ![endpointName hasPrefix:deviceName]) {
        [candidates addObject:[NSString stringWithFormat:@"%@ %@", deviceName, endpointName]];
    }

    for (NSString *candidate in candidates) {
        if ([candidate isEqualToString:target])
            return true;
    }
    return false;
}

QString transportForDriver(NSString *driver) {
    if (driver.length == 0)
        return QString();
    if ([driver rangeOfString:@"Bluetooth" options:NSCaseInsensitiveSearch].location != NSNotFound)
        return QStringLiteral("BLE");
    if ([driver rangeOfString:@"USB" options:NSCaseInsensitiveSearch].location != NSNotFound)
        return QStringLiteral("USB");
    if ([driver rangeOfString:@"RTP" options:NSCaseInsensitiveSearch].location != NSNotFound
        || [driver rangeOfString:@"Network" options:NSCaseInsensitiveSearch].location != NSNotFound)
        return QStringLiteral("Network");
    return QString();
}

}

void iosShowBluetoothMidiPicker(std::function<void()> onDismiss) {
    dispatch_async(dispatch_get_main_queue(), ^{
        UIViewController *presenter = topPresenter();
        if (!presenter) {
            qWarning() << "IosBluetoothMidi: no view controller available to present the picker";
            if (onDismiss)
                onDismiss();
            return;
        }

        CABTMIDICentralViewController *picker =
            [[CABTMIDICentralViewController alloc] init];
        picker.title = @"Bluetooth MIDI";

        // CABTMIDICentralViewController is a bare table view controller with no
        // dismiss affordance of its own, so wrap it and supply one.
        UINavigationController *navigation =
            [[UINavigationController alloc] initWithRootViewController:picker];
        navigation.modalPresentationStyle = UIModalPresentationFormSheet;

        __weak UINavigationController *weakNavigation = navigation;
        UIAction *done = [UIAction actionWithHandler:^(UIAction *) {
            [weakNavigation dismissViewControllerAnimated:YES
                                               completion:^{
                                                   if (onDismiss)
                                                       onDismiss();
                                               }];
        }];
        picker.navigationItem.rightBarButtonItem =
            [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                                                   primaryAction:done];

        [presenter presentViewController:navigation animated:YES completion:nil];
    });
}

QString iosMidiTransportForName(const QString &portName) {
    if (portName.isEmpty())
        return QString();

    NSString *target = portName.toNSString();
    const ItemCount sourceCount = MIDIGetNumberOfSources();
    for (ItemCount index = 0; index < sourceCount; ++index) {
        MIDIEndpointRef source = MIDIGetSource(index);
        if (source == 0)
            continue;
        MIDIDeviceRef device = 0;
        if (!sourceMatchesName(source, target, &device))
            continue;

        // The driver identifies the transport. Report nothing unless the
        // driver string is one that is positively recognized.
        QString transport = transportForDriver(stringProperty(device, kMIDIPropertyDriverOwner));
        if (transport.isEmpty())
            transport = transportForDriver(stringProperty(source, kMIDIPropertyDriverOwner));
        return transport;
    }
    return QString();
}
