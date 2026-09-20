# FT8/FT4 and SSTV transmit level

Updated 2026-09-09 for the 1.0.5 development build. Software implementation is
complete for shared protection, calibration, and SSTV integration; physical K4
acceptance is pending. Live FT8/FT4 QSO transmission is available for device testing.

## Operator workflow

RF power stays in watts on the operating screen. FT8/FT4 has a compact slider
below its frequency, using the existing SSTV/K4 command path. It follows radio
readback and sends a dragged value on release; practice changes are local.

Open **Options → Calibrate TX audio / protection status** in FT8/FT4, or
**CALIBRATE TX AUDIO / PROTECTION** in SSTV's Transmit setup. The shared panel
provides **Use K4 DATA mode**, **Calibrate TX audio**, **STOP**, and **Back**.
Selecting DATA is an explicit operator action using the main mode popup's
DATA-A command. Opening setup only queries input settings.
The panel now follows the actual transmit VFO's RadioState mode/submode,
split, connection, receive state and input readback. It shows **K4 DATA mode
ready · tap Calibrate** and enables Calibrate only when those prerequisites
are met. Readiness is separate from protection/failure status.

Calibration requires a connected, idle K4 in DATA mode. It reads the original
TEST setting, enables TEST, and requires confirmation before keying or sending
a tone. Elecraft documents that TEST suppresses K4 RF while allowing audio ALC
measurement, but downstream equipment may still key. The panel explains this.
STOP, Back, app backgrounding, or a protection fault ends the operation.
Unkey precedes restoration of the original TEST setting. Missing confirmation,
a lost connection, unexpected TEST exit, or reported RF during calibration
fails the operation. A failed or cancelled check retains the previous saved
calibration and reports any unconfirmed TEST restoration.

A successful calibration saves the digital attenuation, timestamp, and schema
version in app settings. FT8 and FT4 share one calibration. The latest saved
level from either mode is adopted automatically when upgrading. SSTV retains
its separate calibration. The matching key includes radio host/port, modem
family, DATA mode, codec, radio input selection/gain, compression and TX EQ.
Station selection, audio offset, RF power, band, streaming latency and firmware
readback do not invalidate calibration. The active ALC guard continues to reduce
drive as needed. Legacy hash-only records are recovered for the current setup,
including a different previously calibrated audio offset. These records are
local app preferences, separate from ADIF logs.
SSTV transmission requires a matching saved result. Protective reductions
remain in use for that setup during the current app session; another radio's
attenuation cannot contaminate the saved value.

The module displays protection status itself. FT8 uses its existing banner,
preserving activity space. SSTV uses a persistent label above both RX and TX,
so an error remains visible after automatic return to receive. The setup panel
also displays progress and errors.

## Shared protection implementation

- Generated program audio uses mono 12 kHz signed 16-bit samples, bypassing
  microphone gain and phone playback volume. Before any PCM/Opus encoding,
  the shared processor rejects full-scale source samples and applies bounded
  attenuation with a 5 ms ramp. It never clips a waveform into compliance.
- The maximum multiplier is 0.5. SSTV's source peak of 26213 becomes at most
  13106 (about −8 dBFS). Calibration starts at 0.03125 (−30.1 dB), but can
  reduce in 3 dB steps below that initial level. A stable raw ALC of 3–5
  succeeds; reaching 5 does not cause another drive adjustment. The numeric floor is
  1/32768 (−90.3 dB), one S16 full-scale quantization step, rather than the
  old arbitrary 1/32 floor. This is a search bound, not a claim of usable
  signal quality at that extreme. Saved levels and subsequent transmissions
  retain the lower gain. Normal transmission
  never raises drive automatically. Voice encoding retains its existing path.
- Protected Opus packets undergo a local floating-point decode. Nonfinite
  output or peaks above −3 dBFS reject the packet and stop transmission.
  Encoder and monitoring decoder histories reset together.
- The I/O thread owns a 50 ms watchdog and the socket gate. It enables TM
  telemetry for deliberate digital transmission, queries every 250 ms, and
  requires a full fresh ALC/compression/power/SWR response within 1500 ms.
  TM0/TM1 acknowledgements do not renew the timer; identical fresh full
  responses do. A late response cannot revive an expired transmission.
- The operator-accepted raw ALC range includes 5. Protection reduces drive
  by 3 dB at 6,
  with 400 ms between reductions, and stop at 10. Continuous high ALC for
  1500 ms during live transmission with at least three spaced high samples, confirmed high ALC at
  minimum drive, any reported compression, malformed or
  missing metering, audio headroom failure, and socket backlog/write failure
  also stop transmission. Faults require explicit operator retry.
- During calibration, moderate raw ALC (6–9) waits for 600 ms after the first
  accepted audio and each gain adjustment. At minimum drive it requires at
  least three high observations spaced by 200 ms over at least 600 ms;
  a lower reading resets the history. One startup reading no longer stops
  calibration. The raw emergency ceiling of 10, RF/compression checks,
  metering deadline and streaming watchdog remain immediate/active.
  Each calibration reduction resets moderate-high history so TEST calibration
  can continue searching lower after settling; the complete operation retains
  its 15-second deadline. Live transmission keeps its sustained-high stop.
- Calibration status and Android `Digital TX calibration` log entries include
  the exact full TM response, raw ALC and current audio drive. Fault status
  retains the last parsed response inside both modules. These diagnostics
  contain no profile credentials and must remain outside Git when captured.
- The I/O thread closes the shared generation and writes RX without waiting
  for the UI. Audio synthesis and queued packets check the same generation.
  Stale packets cannot restart a stopped transmission.
- Calibration raises gain only during its explicit TEST operation,
  after continuing program packets have been accepted and a 600 ms settling
  interval has elapsed. Raw ALC 3–5 must remain stable for 1200 ms; the same
  overload checks apply. No streaming tone, no reachable target within the
  audio limit, or a 15-second timeout fails calibration. Meter readings alone
  cannot create a successful result.

The shared mode/generation API now carries live FT8, FT4, and SSTV. Call/CQ
arms an exclusive FT slot; it never acknowledges a protection fault during
an automatic retry. Explicit operator rearming can acknowledge a stopped
fault. FT transmissions use the matching remembered gain and preserve any
additional downward reductions. The I/O watchdog stops an active FT stream
after 400 ms without accepted audio, or 1200 ms with no first packet.
The audio thread separately bounds key-up, UTC changes, pacing, drain and
unkey completion. See [FT8_FT4_SCOPE.md](FT8_FT4_SCOPE.md) for live TX scope.

## Validation and remaining limits

Automated tests exercise all three protection modes, saved-setting isolation,
PCM headroom/ramping, malformed/stale metering, compression, severe/sustained
ALC, generation cancellation, and the actual threaded TCP/Protocol pipeline.
A fake K4 checks TEST-before-TX, RX-before-restoration, cancellation,
exclusive ownership, missing restoration acknowledgement, and disconnects.
Blocking the UI event loop does not block the network watchdog.

UI checks cover 320×568, 360×696, and landscape setup panels, persistent
SSTV RX faults, and FT8 activity refresh. Codec tests use an optional local
Xiph Opus 1.5.2 source build, verified against the upstream SHA-256 list.
They encode/decode tones from 100 to 3200 Hz at all four streaming frame sizes,
verify decoded headroom, and reject excessive codec output. This host test
does not validate the shipped Android binary or the K4 decoder.

Run ordinary tests with `test-windows.cmd -Action Test`. To include codec
checks, set `QK4_TEST_OPUS_SOURCE` to an extracted official Opus source tree
for that shell session. No source download occurs automatically.

**K4 hardware acceptance is still required.** The operating manual specifies
an ALC indication just below its printed 5 mark for these modes, whereas TM
reports raw bars. This implementation uses conservative provisional raw
thresholds; a single raw-7/front-panel-about-6 observation does not establish
a scale conversion. TM delivery and decreasing ALC with decreasing drive
have now been observed on this K4 in TEST mode. Broader firmware/link behavior,
transport latency, calibration repeatability, and full SSTV/FT tone-range
behavior still need validation. A 1500 ms feedback deadline
deliberately stops radios/links that cannot supply the required readings.

The D14 operating manual explicitly says K4 has twice the K3 bar count and
translates the old four-bar recommendation to eight K4 bars. D4's TM table
only identifies its first three-digit field as ALC bars. This calls the
original raw-5 reduction boundary into question; it is not evidence that
raw 5 equals the printed 5 mark. Following the measurements below, the
operator accepted raw 5; the current reduction boundary is 6. This is an
operator acceptance criterion, not a measured general scale conversion or
proof of RF quality.

### Device observation, 2026-09-09

On the signed correction build, four calibration attempts at 21:16:54,
21:17:05, 21:17:26 and 21:17:51 local time initially reported zero ALC, then
repeated `TM007000000010;` until the guard stopped them. This parses as raw
ALC 7 bars, compression 0 dB, forward-power field 0 and SWR field 010 (1.0).
The app's gain remained 0.03125 (−30.1 dB). The operator reported approximately
6 on the physical K4 ALC scale during these tests. The moderate high value
persisted across multiple samples; it was not just one startup transient.
This confirmed that the original provisional raw-5 policy rejected this
setup. It does not establish a general conversion between TM bars and the
printed scale, or establish acceptable RF quality. Raw logs remain outside Git.

The 21:26 lower-drive calibration confirmed that reducing streamed amplitude
reduces reported ALC: raw 7 at −30.1 dB fell through 6 and 5 as drive dropped.
However, fixed 3 dB steps then repeatedly alternated between raw 5 at −42.1 dB
and raw 2 at −45.2 dB. Intermediate raw 4/3 readings were transient. The
15-second deadline stopped the run without saving a calibration. The operator
then explicitly accepted raw ALC 5 as good enough. The current policy accepts
stable raw 3–5 and reduces at 6, retaining the immediate raw-10 ceiling.
No finer search is required after achieving 5. Automated tests cover settling
at raw 5 after reductions for FT8, FT4 and SSTV. After installing this
correction, the operator reported that calibration is working well on the
Samsung/K4 setup. The final saved drive was not captured in the available
log; RF/spectral acceptance remains separate.

ALC monitoring detects reported overload; it cannot prevent the first transient
or prove clean RF by itself. Independent RF/spectral measurement, a suitable
dummy load, thermal/duty-cycle checks, external-amplifier checks, and
interoperability remain acceptance requirements. SSTV's 1500 Hz calibration
is an initial level baseline, not proof of flat response across all image/ID
tones. The existing Android Opus archive also lacks a reliable recorded
upstream version and needs a tagged rebuild before production release.

## Primary references

- [Elecraft K4 Operating Manual, rev D14](https://ftp.elecraft.com/K4/Manuals%20Downloads/K4%20Built-In%20Operating%20Manual%20rev%20D14/K4BuiltInOperatingManualrevD14.html):
  digital audio ALC target and DATA compression behavior.
- [Elecraft K4 Programmer's Reference, rev D4](https://ftp.elecraft.com/K4/Manuals%20Downloads/K4ProgrammersReferencerev.D4.html):
  PC, TM, TS, TX, and RX; TEST may still key downstream equipment.
- [Xiph release checksums](https://downloads.xiph.org/releases/opus/SHA256SUMS.txt):
  host-test Opus 1.5.2 archive SHA-256
  `65c1d2f78b9f2fb20082c38cbe47c951ad5839345876e46941612ee87f9a7ce1`.
