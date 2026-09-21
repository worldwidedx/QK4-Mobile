#ifndef IOSAUDIOSESSION_H
#define IOSAUDIOSESSION_H

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

// Deactivate the session, letting other apps resume.
void deactivate();

} // namespace IosAudioSession

#endif // IOSAUDIOSESSION_H
