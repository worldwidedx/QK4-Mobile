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
- The first candidate removed the 10 ms HaliKey-path paddle release debounce
  and changed both Android MIDI sessions from 8 ms to 4 ms precise polling.
  A HaliKey operator reported cumulative delay and burst-like characters at
  25 WPM, worse than their 1.0.5 baseline. Those timing changes were reverted.
  The current candidate keeps the 10 ms CW-session release debounce and polls
  both Android MIDI sessions at 4 ms on the high-priority keyer thread. CTR2
  retains its established UI path for knobs, buttons, straight key, and PTT;
  its paddle notes use the timestamped keyer path. Normal per-message logging
  remains removed.
- Android now retains each MIDI callback timestamp. The keyer records paddle
  history and evaluates it at scheduled element boundaries, including when
  several edges arrive together after a delay. Old batches are replayed at
  legal element speed with character gaps. A missed timer deadline cannot arm
  an immediate extra element.
- Optional raw callback logging records the entire MIDI byte sequence before
  QK4 filters messages. It is for device characterization, not a timing source
  used by the keyer.

The stream parser and timestamped keyer address software delivery delays, but
they do not establish a maximum usable WPM. Android callback timestamps are
not necessarily measured contact times at the MIDI device. The HaliKey report
did not identify whether the burst was heard in local sidetone, K4-returned
audio, or both; physical acceptance and a short trace remain necessary.

The next HaliKey report says the new candidate no longer stacks characters
and its local sidetone is usable to about 30 WPM. The remaining extra final
dit is in the local sidetone, especially above 30 WPM. A focused regression
test now covers a brief same-paddle re-press released before the element
boundary; only an opposite-paddle press is remembered. This correction still
needs operator validation. The same report says the K4 did not key, and the
test app connected on 9205 but not 9204. Compare the test app's separate TLS
profile with the production app's before diagnosing a network regression.

## TinyMIDI built-in touch capture (2026-10-17)

On the Samsung Galaxy S26 Ultra with the PR #27 test app, the owner sent ten
intended separate DIT taps at 20 WPM, paused, then held the built-in TinyMIDI
DIT touch pad. The raw Android callback trace contains one note 20 Note On and
one Note Off per contact, with no duplicate Note On burst. The ten tap durations
measured between Android callbacks were 60.6, 70.7, 40.4, 63.2, 87.5, 135.4,
100.8, 31.8, 28.6, and 123.7 ms. Four later contacts in the trace lasted
360.0, 405.8, 416.1, and 405.5 ms. These are callback intervals, not
device-measured touch times. The operator was asked for two holds; the trace
contains four, so do not infer four intentional holds without a new labeled run.

At 20 WPM, a DIT is 60 ms and a continuously held DIT repeats at the 120 ms
element boundary. Two of the ten intended quick taps remained down across that
boundary, explaining unwanted second DITs without duplicate MIDI messages.
The 10 ms release debounce does not alter the recorded release timestamp, but
it can delay when the keyer receives the release edge. An intended 135 ms tap
and the first 135 ms of an intentional hold present identical MIDI input until
release. A one-element-per-touch rule would suppress held-paddle repeats and is
therefore unsuitable. Any touch-specific first-repeat grace must be tested for
both single-tap accuracy and held-DIT rhythm at 20 and 30 WPM before use as a
default. The current MIDI mapping cannot distinguish TinyMIDI's built-in touch
pad from a paddle connected to its external key input, so a touch-specific
setting must leave the external input's normal behavior selectable.

An experimental Android TinyMIDI touch control now offers OFF (0 ms) through
100 ms lookahead in 5 ms steps. It starts each touch-initiated sequence after
that delay but keeps the selected WPM cadence for a held paddle. At a DIT/DAH
boundary, only a release of the currently sounding paddle during the lookahead window can
cancel a repeat; an opposite-paddle press is still evaluated at the original
boundary. The control applies only to session 0's TinyMIDI paddle profile.
OFF is the default, adds no touch lookahead, and should be selected for
TinyMIDI's external key jack. The existing 10 ms release debounce remains
active even when touch lookahead is OFF.
CTR2 and HaliKey paths retain their current timing. The timing control has
preliminary owner feedback, but is not a demonstrated 30 WPM touch solution.

With test build `f97f098` on the Samsung Galaxy S26 Ultra, the owner reported
that 20 ms worked well for CQ at 20 WPM. A somewhat higher setting worked at
25 WPM; the exact value was not recorded. The owner could not achieve accurate
30 WPM with any tested value and noted that practice might change that result.
This does not establish the device's maximum reliable speed because the owner
does not send 30 WPM reliably. Treat 30 WPM on TinyMIDI's built-in capacitive
touch as a stretch target requiring a proficient operator and scored copied
text. Do not generalize this report to TinyMIDI's external key jack, HaliKey,
or CTR2.

For acceptance, have an operator copy the same written test text with OFF,
40 ms, 60 ms, and nearby 5 ms increments at 20, 25, and 30 WPM using built-in
touch. Include separate single-DIT taps, a held DIT, alternating paddles,
and squeeze-heavy text.
Measure wrong letters and the delay from touch to local sidetone. Use a physical
key on TinyMIDI's external jack with OFF to confirm normal repeat behavior.
Record the chosen setting with each trace; select the smallest delay that
removes extra elements without making letter spacing or paddle feel worse.

For the next radio test, use K4 CW/CW-R and TX TEST mode, confirm the app shows
live radio state, then key a short pattern and check the K4's own TX/monitor
indication separately from the phone sidetone. Record the test app's connection
error verbatim, port, TLS setting, and whether its PSK identity/password match
the working production profile. Do not include credentials in the report.

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
