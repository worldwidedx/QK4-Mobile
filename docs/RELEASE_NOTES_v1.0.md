# QK4 Mobile v1.0

QK4 Mobile v1.0 is the first complete QK4 Mobile release with integrated CW
device support and integrated SSTV.

## New Features

### 1. Complete SSTV Transmit and Receive Module

QK4 Mobile now includes a complete Slow Scan Television operating environment
for transmitting and receiving images directly through an Elecraft K4.

#### Transmit

- Select an image from the Android gallery or take a photograph with the
  camera.
- Select the SSTV mode before editing so the canvas shows the exact transmitted
  resolution and aspect ratio.
- Crop, fill, pan, rotate, center, zoom, and pinch-zoom images.
- Add editable text with multiple fonts, sizes, styles, and an expanded color
  palette.
- Draw freehand or add lines, arrows, rectangles, and ellipses.
- Move, edit, recolor, rotate, delete, undo, redo, and reset composition
  objects.
- Start with built-in CQ, report, and 73 layouts or create reusable custom
  templates.
- Save complete compositions in the SSTV Image Gallery and automatically
  restore unfinished transmit drafts.
- Store the operator's callsign and apply it dynamically to templates.
- Enter an optional destination callsign for directed transmissions and
  replies.
- Append FSK ID, CW ID, or both after the image, with adjustable CW
  identification speed from 5-40 WPM.
- Adjust and synchronize K4 transmit power from the SSTV screen.
- Preview the exact completed image before keying the radio.
- Follow visible progress through image transmission, FSK ID, CW ID, and
  completion.
- Immediately terminate transmission using the persistent STOP SSTV control.
- Automatically return to AUTO RX after transmission.

<img src="https://github.com/worldwidedx/QK4-Android/releases/download/v1.0/QK4-Mobile-v1.0-SSTV-TX-1.jpg" alt="SSTV transmit setup and exact-mode image canvas" width="300">

<img src="https://github.com/worldwidedx/QK4-Android/releases/download/v1.0/QK4-Mobile-v1.0-SSTV-TX-2.jpg" alt="SSTV composition, template, and transmit-power controls" width="300">

#### Receive

- AUTO RX starts whenever the SSTV screen opens and resumes automatically after
  transmitting.
- Progressive image display during reception.
- Automatic VIS detection, mode selection, frequency correction, and slant
  correction.
- Weak-signal processing with streaming filtering, AFC tracking,
  impulse-resistant synchronization, and robust pixel estimation.
- Support for 22 SSTV modes: Robot 36; Martin M1, M2, M3, and M4; Scottie S1,
  S2, DX, S3, and S4; PD-50, PD-90, PD-120, PD-160, PD-180, PD-240, and PD-290;
  Wraase SC2-120 and SC2-180; and Pasokon P3, P5, and P7.
- Correct Robot 36 paired-chroma decoding and encoding.
- Detect received callsigns from checksum-validated FSK ID, with CW ID as a
  secondary source.
- Prioritize FSK ID when both identification methods are detected.
- Manually enter or correct a callsign when automatic identification is
  unavailable.
- Use REPLY to transfer the received callsign and SSTV mode into the transmitter
  without keying the radio.
- Retain received images as lossless PNG files with time, frequency, mode,
  slant, and callsign metadata.
- Configurable receive-history retention with starring, sharing, deletion, and
  protected history clearing.

<img src="https://github.com/worldwidedx/QK4-Android/releases/download/v1.0/QK4-Mobile-v1.0-SSTV-RX.jpg" alt="SSTV automatic receive, callsign recognition, history, and reply controls" width="300">

#### Mobile Integration

- Dedicated opaque SSTV workspace provides a standalone operating environment.
- Supports portrait and landscape orientation while preserving the active
  image and editing state.
- RX and TX frequency and radio mode remain visible in the SSTV header.
- BACK TO RADIO closes SSTV and restores the normal QK4 console.

#### Live Testing and Validation

The SSTV module underwent thorough live-device testing while remotely connected
to an Elecraft K4. Receive testing included extended live monitoring and
interactive debugging while receiving a variety of real-world SSTV
transmissions, including strong, weak, fading, and partially impaired signals.

Transmit testing used over-the-air recordings captured by a remote WebSDR while
QK4 Mobile transmitted through the K4. The captured WAV files were then
processed through a local SSTV decoder to verify that the complete transmitted
signal--including the leader, VIS header, image data, and ending sequence--could
be successfully received and decoded.

### 2. DX Prefix List

The FN menu's DX LIST button now opens a complete international prefix
reference.

- Naturally sorted with numeric prefixes followed by A-Z.
- Search by prefix, country, territory, or alternate prefix.
- Touch-scrollable list with previous and next match navigation.
- Android keyboard handling submits the search and closes the keyboard
  correctly.
- Read-only reference that does not change any K4 settings.

### 3. New Android Application Identity

- The Android application ID is now `com.w9wdx.qk4phone`.
- A guarded development-device migration utility can transfer settings and
  SSTV data from the former package.
- Migrated files are verified before the old development package can be
  removed.

## Fixes & Refinements

### Touch and Layout

- Improved touch scrolling throughout the mobile interface.
- Reduced accidental button and menu activation during vertical gestures.
- Standardized control icons and compact touch geometry.
- Improved slider responsiveness and tap-to-position behavior.
- Corrected clipped, oversized, or off-screen controls in portrait and
  landscape layouts.
- Improved the sizing and readability of confirmation dialogs, labels, and
  actions.
- Improved callsign, CW-speed, template, TX-power, receive-history, and
  reply-control alignment.

### Application Behavior

- Keeps the screen awake while QK4 Mobile is running in the foreground and
  releases the request when the app is paused, without changing the device's
  system-wide screen-timeout setting.

### Main QK4 Controls

- Removed the MON control because delayed remote monitor audio does not provide
  useful real-time transmit monitoring.
- Removed BAL from the mobile control drawer; A AF and B AF remain independent
  receiver-volume controls.
- M.RF and S.SQL can no longer be replaced by a BAL overlay.
- M.RF alternates only with M.SQL, and S.SQL alternates only with S.RF.
- Moved NORM beside the bandwidth and shift controls it affects.

## Version and Compatibility

- Version: **1.0**
- Android version code: **26**
- Package: `com.w9wdx.qk4phone`
- ABI: ARM64 (`arm64-v8a`)
- Minimum Android version: Android 8.0 / API 26

Because the Android package identity changed, Android installs v1.0 separately
from releases using `com.ai5qk.qk4phone`. The guarded one-time migration tool is
available for debuggable development installations; normal public Android
package isolation does not permit an in-place settings migration from the old
application identity.
