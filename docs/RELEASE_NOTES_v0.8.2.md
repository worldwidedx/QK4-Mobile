# QK4 Mobile v0.8.2

QK4 Mobile v0.8.2 improves received-audio quality on Android, particularly
for CW and digital signals.

## Improvements

- Eliminated the raspy or rough sound previously noticeable on steady CW and
  digital tones.
- Added continuous 12 kHz to 48 kHz resampling across K4 audio-packet
  boundaries.
- Prevented partial Android audio writes from dropping received samples.
- Preserved audio packet order when Android's playback buffer temporarily
  accepts only part of a packet.
- Improved RX audio continuity without changing K4 radio settings, network
  protocol behavior, or operator controls.

## Version and compatibility

- Version: **0.8.2**
- Android version code: **24**
- Package: `com.ai5qk.qk4phone`
- ABI: ARM64 (`arm64-v8a`)
- Minimum Android version: Android 8.0 / API 26

The release-signed APK was signature-verified, installed as an in-place update
over v0.8.1 on the Samsung Galaxy S26 Ultra, and launched with existing
application data preserved. The RX audio improvement was confirmed while
receiving CW on the connected K4.
