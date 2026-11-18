# QK4 Mobile v1.0.7

QK4 Mobile v1.0.7 improves the Android radio console on folding devices and
adds setup and control refinements shared with the iPhone and iPad builds.

## Android

- Android devices that report folding hardware use the compact radio console
  on both displays. The console and panadapter refresh when the display changes.
  **Settings > Layout > Always use compact layout** provides a saved fallback
  for other devices; restart QK4 Mobile to apply the choice.
- When no radio is saved, the console prompts you to tap **CONN** to add one.
- On the regular tablet layout, **FREQ ENT** opens a touch keypad when no
  hardware keyboard is available and follows **B SET** when selecting the VFO.

## iPhone and iPad

- The main radio console holds its landscape orientation. FT8/FT4 and SSTV
  controls stay within the iPhone safe area across their supported rotations.
- Touch scrolling reaches settings and SSTV controls on iPhone, and Radio
  Manager fits inside the usable screen. Opening Setup from the radio logbook
  no longer triggers the reported crash.
- The CW Keyer settings page supports MIDI device selection and Bluetooth
  pairing. CTR2-MIDI supports USB and Bluetooth LE connections.
- QRZ Logbook stores its API key in the iOS Keychain.

The console header and About screen use consistent QK4 Mobile branding on both
platforms. A console with no saved radio explains how to get started.

## Availability and validation

This release provides an Android ARM64 APK (`com.w9wdx.qk4phone`, version code
35). The owner confirmed that the v1.0.7 behavior was tested in the prior test
builds on the usual Samsung Galaxy S26 Ultra. The release-signed APK was built
and signature-verified separately, then installed as an update on that phone
(Android 17). The device reports version 1.0.7, code 35; no separate radio test
was run after this installation.

The shared iOS source passed the unsigned macOS CI compile. A signed iPhone or
iPad package is not included in this release, and physical iOS acceptance is
not recorded.
