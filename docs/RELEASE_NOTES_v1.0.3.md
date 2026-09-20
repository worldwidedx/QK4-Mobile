# QK4 Mobile v1.0.3

QK4 Mobile v1.0.3 improves SSTV receive reliability, corrects the SSTV
transmit layout in landscape mode, and adds low-latency CW sidetone hot-plug
routing.

## What's new

### Improved SSTV reception

- Better acquisition of weak, noisy, frequency-offset, or partially damaged
  SSTV headers.
- Retains recovery when the beginning of a transmission is clipped.
- Requires three correctly timed line-sync pulses before accepting a
  recovery-only mode detection, substantially reducing false decodes.
- Unconfirmed detections return silently to AUTO RX without announcing a mode
  or starting an image.
- Expanded automated coverage across all 22 supported SSTV modes.
- Ten captured false-trigger recordings replay with no mode detections or
  completed images.

### CW sidetone audio routing

- Sidetone now follows connections and disconnections between the phone
  speaker, Bluetooth earphones, USB-C headphones, and Android hearing aids
  exposed as `TYPE_HEARING_AID`.
- Preserves the original low-latency sidetone path.
- Correctly returns sidetone to the phone speaker after Bluetooth or USB-C
  headphones disconnect.
- Speaker, Bluetooth, and USB-C transitions were physically tested with
  TinyMIDI on a Samsung Galaxy S26 Ultra.
- Hearing-aid sidetone routing is implemented but still awaiting physical
  hardware validation.

### SSTV transmit-screen layout

- In landscape mode, FSK ID, CW ID, and CW speed now appear on a second row
  below MY CALL.
- MY CALL remains left-aligned beneath its label.
- The existing portrait layout remains unchanged.

## Compatibility

- Android 8.0 or later
- ARM64 Android devices
- Android version code 29
- Installs in place over v1.0.2 while preserving application data

## Scope notes

- Hearing-aid sidetone routing still requires physical validation.
