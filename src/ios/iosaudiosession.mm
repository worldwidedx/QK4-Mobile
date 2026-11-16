#include "iosaudiosession.h"

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include <QDebug>
#include <QString>
#include <QStringList>

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

// One line naming the active input/output port types (BuiltInMic,
// BluetoothA2DP, BluetoothHFP, ...) plus the session's category, mode and rate.
// Port types only, not device names, so logs can be shared. This answers "which
// microphone is the radio transmit audio actually coming from?".
void logRoute(const char *when) {
    AVAudioSession *session = [AVAudioSession sharedInstance];
    auto describe = [](NSArray<AVAudioSessionPortDescription *> *ports) {
        QStringList types;
        for (AVAudioSessionPortDescription *port in ports)
            types << QString::fromNSString(port.portType);
        return types.isEmpty() ? QStringLiteral("none") : types.join(QLatin1Char(','));
    };
    AVAudioSessionRouteDescription *route = session.currentRoute;
    qInfo().noquote() << "IosAudioSession: route" << when << "input" << describe(route.inputs) << "output"
                      << describe(route.outputs) << "category" << QString::fromNSString(session.category) << "mode"
                      << QString::fromNSString(session.mode) << "sampleRate" << session.sampleRate;
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
    logRoute("after change");
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
    // earpiece). Voice-chat mode applies Apple's echo cancellation and noise
    // suppression, and with HFP allowed a Bluetooth headset (AirPods) is used
    // for both the microphone and RX audio instead of the far-away built-in
    // mic. HFP is narrowband (typically 16-24 kHz); amateur voice audio is
    // 2-3 kHz wide, so nothing is lost. A2DP stays allowed for output-only
    // Bluetooth devices. In this mode the hardware rate follows the headset,
    // so AudioEngine's 48 kHz mic format is converted by Qt.
    const AVAudioSessionCategoryOptions options =
        AVAudioSessionCategoryOptionDefaultToSpeaker | AVAudioSessionCategoryOptionAllowBluetoothHFP |
        AVAudioSessionCategoryOptionAllowBluetoothA2DP;
    const AVAudioSessionMode mode = AVAudioSessionModeVoiceChat;

    // This runs on every PTT press, on the main thread, while RX audio plays.
    // Re-applying an unchanged category or sample rate can still reconfigure
    // the route and stall the press, so only touch what actually differs.
    const bool categoryCurrent = [session.category isEqualToString:AVAudioSessionCategoryPlayAndRecord]
                                 && [session.mode isEqualToString:mode] && session.categoryOptions == options;
    NSError *error = nil;
    if (!categoryCurrent) {
        g_lastOwnSessionWorkMs = nowMs();
        if (![session setCategory:AVAudioSessionCategoryPlayAndRecord
                             mode:mode
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
        logRoute("on activate");
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
