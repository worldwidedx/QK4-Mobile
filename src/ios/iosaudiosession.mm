#include "iosaudiosession.h"

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include <QDebug>
#include <QString>

// Observes AVAudioSession interruptions and route changes. Interruption
// handling is intentionally minimal: on interruption end, if the system says
// we may resume, re-activate the session so RX playback and mic capture come
// back. It does not restart QAudioSink/QAudioSource; a long interruption
// (e.g. a phone call) may still require reconnecting. Route changes are logged
// only for now.
@interface Qk4AudioSessionObserver : NSObject
@end

@implementation Qk4AudioSessionObserver

- (instancetype)init {
    if ((self = [super init])) {
        NSNotificationCenter *nc = [NSNotificationCenter defaultCenter];
        [nc addObserver:self
               selector:@selector(handleInterruption:)
                   name:AVAudioSessionInterruptionNotification
                 object:nil];
        [nc addObserver:self
               selector:@selector(handleRouteChange:)
                   name:AVAudioSessionRouteChangeNotification
                 object:nil];
    }
    return self;
}

- (void)handleInterruption:(NSNotification *)note {
    NSNumber *typeValue = note.userInfo[AVAudioSessionInterruptionTypeKey];
    if (!typeValue)
        return;
    const AVAudioSessionInterruptionType type =
        static_cast<AVAudioSessionInterruptionType>(typeValue.unsignedIntegerValue);
    if (type == AVAudioSessionInterruptionTypeBegan) {
        qInfo() << "IosAudioSession: audio interrupted";
        return;
    }
    if (type == AVAudioSessionInterruptionTypeEnded) {
        NSNumber *optsValue = note.userInfo[AVAudioSessionInterruptionOptionKey];
        const bool shouldResume =
            optsValue && (optsValue.unsignedIntegerValue & AVAudioSessionInterruptionOptionShouldResume);
        if (shouldResume) {
            NSError *error = nil;
            if (![[AVAudioSession sharedInstance] setActive:YES error:&error]) {
                qWarning() << "IosAudioSession: reactivate after interruption failed:"
                           << (error ? QString::fromNSString(error.localizedDescription) : QString());
            } else {
                qInfo() << "IosAudioSession: audio resumed after interruption";
            }
        }
    }
}

- (void)handleRouteChange:(NSNotification *)note {
    NSNumber *reason = note.userInfo[AVAudioSessionRouteChangeReasonKey];
    qInfo() << "IosAudioSession: audio route changed, reason" << (reason ? int(reason.unsignedIntegerValue) : -1);
}

@end

namespace {
// Held for the process lifetime; the session outlives any AudioEngine.
Qk4AudioSessionObserver *g_observer = nil;

void ensureObserver() {
    if (g_observer == nil)
        g_observer = [[Qk4AudioSessionObserver alloc] init];
}
} // namespace

void IosAudioSession::configureForVoice() {
    ensureObserver();
    AVAudioSession *session = [AVAudioSession sharedInstance];

    // playAndRecord so the mic can be captured while RX plays. defaultToSpeaker
    // keeps RX on the loudspeaker (playAndRecord otherwise routes to the
    // earpiece). Only A2DP is allowed for Bluetooth: it is output-only, so the
    // built-in mic stays at 48 kHz and matches AudioEngine's input format.
    // HFP (allowBluetooth) would drop the hardware to 8/16 kHz narrowband and
    // can make QAudioSource's 48 kHz format check fail; add it later as a
    // deliberate choice.
    const AVAudioSessionCategoryOptions options =
        AVAudioSessionCategoryOptionDefaultToSpeaker | AVAudioSessionCategoryOptionAllowBluetoothA2DP;
    NSError *error = nil;
    if (![session setCategory:AVAudioSessionCategoryPlayAndRecord
                         mode:AVAudioSessionModeDefault
                      options:options
                        error:&error]) {
        qWarning() << "IosAudioSession: setCategory failed:"
                   << (error ? QString::fromNSString(error.localizedDescription) : QString());
    }

    error = nil;
    if (![session setPreferredSampleRate:48000.0 error:&error]) {
        qWarning() << "IosAudioSession: setPreferredSampleRate failed:"
                   << (error ? QString::fromNSString(error.localizedDescription) : QString());
    }
}

bool IosAudioSession::activate() {
    NSError *error = nil;
    const bool ok = [[AVAudioSession sharedInstance] setActive:YES error:&error];
    if (!ok) {
        qWarning() << "IosAudioSession: activate failed:"
                   << (error ? QString::fromNSString(error.localizedDescription) : QString());
    } else {
        AVAudioSession *session = [AVAudioSession sharedInstance];
        qInfo() << "IosAudioSession: active, sampleRate" << session.sampleRate
                << "inputs" << (session.isInputAvailable ? "available" : "none");
    }
    return ok;
}

void IosAudioSession::deactivate() {
    NSError *error = nil;
    [[AVAudioSession sharedInstance] setActive:NO
                                  withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
                                        error:&error];
}
