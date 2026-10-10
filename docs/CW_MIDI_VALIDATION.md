# CW MIDI accuracy validation

This work targets **the Morse elements and letters generated from the operator's
paddle movements**, independent of RF output quality. QK4 Mobile receives
ordinary MIDI note edges from the CW Keyer connection (TinyMIDI or HaliKey) and
the separate CTR2-MIDI connection. It does not currently decode MoMIDI elapsed
times. Neither TinyMIDI nor CTR2-MIDI has published MoMIDI support that we can
rely on; note numbers 20/21 alone do not establish it.

## Current candidate change

- Android parses MIDI messages across callback boundaries and handles running
  status, so a split callback cannot discard a paddle edge.
- Android CW MIDI paddle releases now go directly to the local iambic keyer.
  The 10 ms release debounce remains for straight-key/external-keyer input.
  CTR2 paddle input already had no software release debounce.
- Connected Android sessions poll their event queues every 4 ms with a precise
  timer, rather than every 8 ms. Normal per-message logging is removed.
- Optional raw callback logging records the entire MIDI byte sequence before
  QK4 filters messages. It is for device characterization, not a timing source
  used by the keyer.

These changes reduce known latency and avoid dropped stream fragments. They do
not establish a maximum usable WPM; device and operator validation is required.
The Android main event loop still polls MIDI, so scheduling stalls can delay
delivery of several edges at once.

## Capture the device's unfiltered MIDI

With a debug/test build installed and one device connected in its usual setup
tab, enable the optional trace from a computer with Android platform tools:

```powershell
adb shell setprop log.tag.QK4-MidiRaw DEBUG
adb logcat -v epoch -s QK4-MidiRaw:D > cw-midi-raw.txt
```

Stop `adb logcat` with Ctrl+C after a short sequence. Then disable the trace:

```powershell
adb shell setprop log.tag.QK4-MidiRaw INFO
```

Keep `cw-midi-raw.txt` outside Git. A line includes the session (0 = CW Keyer,
1 = CTR2), Android callback monotonic time, Android's supplied MIDI timestamp,
and the original hexadecimal bytes. The supplied timestamp is not proof of a
device-measured contact time. Diagnostic logging itself adds work, so measure
normal keying performance with the trace **off**.

To check for MoMIDI, capture short and long gaps separately. Look for
nonstandard timing in the velocity of paddle Note On/Off messages and, for
MoMIDI v0.1 gaps above 126 ms, a preceding polyphonic aftertouch `A0` message.
A `B0` control message by itself is not evidence because CTR2 uses control
messages for knobs. Compare any claimed elapsed times against a known input
sequence and the [MoMIDI decoder specification](https://github.com/NetKeyer/midimon).
Record device model, firmware version, transport, and note mode with the trace.

## Operator acceptance matrix

Use the same short, written test text for each run. Ask an operator who can
send at the indicated speed to record the intended text **before** keying. Have
another operator independently copy the local sidetone or a recording, then
compare letters, extra elements, missing elements, and spacing. Start in K4
TEST mode, then repeat any required on-air check separately. Record iambic A/B,
paddle orientation, WPM, device firmware, phone model/OS, app commit, transport,
and whether other device inputs were connected.

| Device and transport | 30 WPM | 35 WPM | 40 WPM | 45-50 WPM |
|---|---|---|---|---|
| [TinyMIDI BLE, built-in touch](https://n6ara.com/product/n6ara-tinymidi/) | Required | Beyond maker's 30 WPM rating | Beyond rating | Beyond rating |
| TinyMIDI BLE, external key input | Required | Stretch | Maker rates up to 40 WPM | Beyond rating |
| CTR2-MIDI USB | Required | Stretch | Stretch | Stretch |
| CTR2-MIDI BLE | Required | Stretch | Stretch | Stretch |
| HaliKey MIDI, tested by an external operator | Required | Stretch | Stretch | Stretch |

Run both alternating and squeeze-heavy text; repeat after background/resume and
after reconnecting. A pass needs the copied text to match the intended letters
across repeated runs, not just an acceptable average WPM. Record every mismatch
with its position and, if available, a short raw trace. The project owner has
TinyMIDI and CTR2-MIDI but no HaliKey and does not send at 30+ WPM, so external
operators must perform the fast-speed and HaliKey acceptance runs.

If a device proves to emit MoMIDI timing, decode it only for that verified
device/firmware profile. Ordinary MIDI must remain the working path for all
three devices.
