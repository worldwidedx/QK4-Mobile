#include "iosaudiosession.h"

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include <QDebug>
#include <QString>

#include <utility>

namespace {
std::function<void()> g_interruptionHandler;

// Route changes that our own category/activation calls cause arrive moments
// later. Ignore a "device lost" within this window of our own session work so
// the app never unkeys itself while preparing the microphone.
constexpr qint64 kSelfRouteChangeGraceMs = 1500;
qint64 g_lastOwnSessionWorkMs = 0;

qint64 nowMs() {
    return static_cast<qint64>([NSDate timeIntervalSinceReferenceDate] * 1000.0);
}

// Notifications can arrive on any thread; the handler touches Qt widgets.
void notifyInterruption() {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (g_interruptionHandler)
            g_interruptionHandler();
    });
}
} // namespace

// Observes AVAudioSession interruptions and route changes. Interruption
// handling is intentionally minimal: on interruption end, if the system says
// we may resume, re-activate the session so RX playback and mic capture come
// back. It does not restart QAudioSink/QAudioSource; a long interruption
// (e.g. a phone call) may still require reconnecting. An interruption, or the
// loss of the active audio device, also calls the registered handler so the
// app can unkey the radio; other route changes are only logged.
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
        notifyInterruption();
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
    // The device in use went away (headset unplugged, Bluetooth lost): the
    // microphone stream is no longer what the operator expects to transmit.
    if (reason && reason.unsignedIntegerValue == AVAudioSessionRouteChangeReasonOldDeviceUnavailable) {
        if (nowMs() - g_lastOwnSessionWorkMs < kSelfRouteChangeGraceMs)
            qInfo() << "IosAudioSession: ignoring route change caused by our own session setup";
        else
            notifyInterruption();
    }
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

    // This runs on every PTT press, on the main thread, while RX audio plays.
    // Re-applying an unchanged category or sample rate can still reconfigure
    // the route and stall the press, so only touch what actually differs.
    const bool categoryCurrent = [session.category isEqualToString:AVAudioSessionCategoryPlayAndRecord]
                                 && session.categoryOptions == options;
    NSError *error = nil;
    if (!categoryCurrent) {
        g_lastOwnSessionWorkMs = nowMs();
        if (![session setCategory:AVAudioSessionCategoryPlayAndRecord
                             mode:AVAudioSessionModeDefault
                          options:options
                            error:&error]) {
            qWarning() << "IosAudioSession: setCategory failed:"
                       << (error ? QString::fromNSString(error.localizedDescription) : QString());
        }
    }

    if (session.preferredSampleRate != 48000.0) {
        g_lastOwnSessionWorkMs = nowMs();
        error = nil;
        if (![session setPreferredSampleRate:48000.0 error:&error]) {
            qWarning() << "IosAudioSession: setPreferredSampleRate failed:"
                       << (error ? QString::fromNSString(error.localizedDescription) : QString());
        }
    }
}

bool IosAudioSession::activate() {
    g_lastOwnSessionWorkMs = nowMs();
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

void IosAudioSession::setInterruptionHandler(std::function<void()> handler) {
    g_interruptionHandler = std::move(handler);
}

void IosAudioSession::deactivate() {
    NSError *error = nil;
    [[AVAudioSession sharedInstance] setActive:NO
                                  withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
                                        error:&error];
}
