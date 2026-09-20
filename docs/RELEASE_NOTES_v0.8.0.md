# QK4 Mobile v0.8.0

QK4 Mobile v0.8.0 is a major feature release focused on external CW keying,
touch-first setup, expanded K4 controls, and mode-aware radio menus.

## Bluetooth MIDI CW keying

- Added native Android Bluetooth LE MIDI discovery and connection.
- Added live CW paddle input through the K4 keyer, local sidetone feedback,
  and an on-screen paddle test.
- Added persistent CW speed and local sidetone volume settings.
- Added preconfigured TinyMIDI and HaliKey MIDI profiles.
- Added a Custom/Learn profile that learns DIT and DAH messages from other
  MIDI paddle interfaces.
- Honors the K4 paddle-orientation setting and saves the synchronized mapping.
- Improved CW element timing and TX/RX hold behavior for smoother sending.
- Remembers the selected MIDI device and mapping.
- Added a touch-scrollable CW setup panel with slider and scrolling gestures
  kept on separate touch surfaces.

## Mobile Setup screen

- Added a dedicated Setup screen from the lower-left gear button.
- Moved FN Key Setup into Setup and added CW Keyer configuration.
- Removed mobile-irrelevant audio input/output, K-Pod, and rig-control pages.
- Improved landscape layout, touch scrolling, control sizing, and state
  preservation when switching Setup sections.

## Expanded K4 controls

- Made Main RX, Sub RX, and TX menus mode-aware for CW, SSB, AM, FM, DATA,
  AFSK, FSK, and PSK.
- Added FM repeater mode and offset controls, PL tone configuration, and a
  K4-style DTMF keypad with six programmable command memories.
- Added CW paddle orientation, Iambic mode, and keying-weight controls.
- Corrected Sub RX targeting so VFO B state is queried independently.
- Prevented unsupported controls from presenting false functionality.
- DATA and AFSK TX bandwidth report the K4's current
  "Implementation in progress" status.
- AM TX bandwidth reports that no documented remote command is available.
- Added close controls to informational messages.

## FM display improvements

- Added K4-style `FM`, `FM+`, and `FM-` repeater indications.
- Appends `/T` when PL tone encoding is enabled.
- VFO mode labels now update when repeater or PL state changes.
- Maintains independent PL state for VFO A and VFO B.

## Display and waterfall controls

- Corrected Waterfall NB mode behavior with OFF, ON, and AUTO states.
- Corrected Reference Level AUTO behavior and AUTO/MAN presentation.
- Improved automatic reference-level calculation to avoid an excessively
  bright waterfall while retaining manual adjustment.
- Checked DISP Average command/state integration.
- Removed an experimental waterfall-smoothing filter after device testing
  showed no useful improvement.

## Additional mobile improvements

- Added a way to close the Connection screen without changing connection state.
- Removed the obsolete F1-F8 setup button from the operating screen.
- Corrected TX and TEST indicator alignment.
- Preserved the compact landscape phone layout for all Android screen sizes
  pending tablet-specific validation.

## Persistence and compatibility

The app now remembers the selected MIDI device, MIDI profile and custom
DIT/DAH mapping, CW speed, local sidetone volume, K4-synchronized paddle
orientation, and DTMF command memories.

QK4 Mobile v0.8.0 targets ARM64 Android devices running Android 8.0/API 26 or
later. Bluetooth MIDI devices are discovered inside QK4 Mobile and may not
appear in Android's normal Bluetooth pairing screen. DTMF transmission requires
the radio to be in transmit mode. BAND/MEM remains unavailable because Elecraft
does not expose an implementation for it.
