#ifndef IOSAUDIOSESSION_H
#define IOSAUDIOSESSION_H

#include <functional>

// iOS AVAudioSession configuration for the K4 audio path.
//
// RX playback works under iOS's default session, but capturing the microphone
// for TX needs the session in the playAndRecord category and activated, or
// CoreAudio hands QAudioSource silence. These helpers own that session state;
// they are no-ops on every other platform (the .mm only builds for iOS).
namespace IosAudioSession {

// Put the shared session in playAndRecord, routed to the loudspeaker, at
// 48 kHz to match AudioEngine's input format. Safe to call before any
// QAudioSource/QAudioSink exists. Registers interruption/route observers on
// first call. Logs (does not throw) on failure.
void configureForVoice();

// Activate the session. Returns false and logs the underlying error on failure.
bool activate();

// Register a callback run on the main thread when the session is interrupted
// (call, Siri, alarm) or the active audio device disappears (e.g. a headset is
// unplugged). The app uses it to unkey the radio, since the microphone stream
// can no longer be trusted. Replaces any previous handler.
void setInterruptionHandler(std::function<void()> handler);

// Deactivate the session, letting other apps resume.
void deactivate();

} // namespace IosAudioSession

#endif // IOSAUDIOSESSION_H
