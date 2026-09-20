# FT8 / FT4 module

Started 2026-09-08 on `codex/ft8-ft4-portrait`, based on tag `v1.0.3`.
The original `codex/keep-awake-ctr2-midi` working directory and its uncommitted
SSTV/CW/CTR2 changes are untouched. This module is not a published release.
CTR2 work predates this module. The initial v1.0.3 starting point omitted it;
on 2026-09-09 the existing implementation through `830cdb0` on
`codex/ctr2-midi-v2` was incorporated, preserving the FT8 work. This includes
the released-line MIDI transport, mapping editor, radio controls, and tests.
The integrated test package uses v1.0.5 metadata and Android version code 32;
no repository release is implied.

## First build: native reception and operating UX

- Existing Fn SSTV control becomes **FT8/FT4** on tap and **SSTV** on hold.
- An opaque portrait application screen covers the radio console. Radio Back
  restores its landscape orientation. Opening the module never keys the radio.
- Shared FT8/FT4 screen with band presets, direct MHz entry, a zoomable audio
  waterfall, activity filters, current QSO, next message,
  Auto, odd/even, CQ, Call, Halt, and Log QSO.
- Common HF/6 m frequencies are shortcuts, not a tuning whitelist. A custom
  frequency survives a mode change; preset changes are explicit. No common
  FT4 preset is invented for 160 m or 60 m. Regional frequency-table editing,
  additional 6 m channels, and persistent favorites remain follow-up work.
- The main frequency uses QK4's actual `FrequencyDisplayWidget`, with the
  same grouped digits and selection underline, fitted to portrait width.
  Tap a digit to select RF tuning at its increment; hold for direct entry.
  The shared parser accepts MHz shorthand, grouped MHz/kHz/Hz, or raw Hz.
- Existing CTR2 VFO/Selected-adjustment knob bindings follow the visible
  **Dial RX / TX / RF** target while the module is open. RX is the initial
  target. RX/TX changes only the chosen audio offset, bounded to 100–3200 Hz;
  choosing TX enables Hold TX. These movements do not retune the carrier,
  clear decodes, or restart reception. Wheel A acceleration is retained.
- New mapping actions **FT8/FT4 RX** (`ft8_rx`) and **FT8/FT4 TX** (`ft8_tx`)
  are intended for the same CTR2 button's short and long press. Either opens
  the module if needed. Existing mappings are retained; the optional
  [sample map](QK4-CTR2-FT8-Sample.qk4ctr2map) uses Button 6 in normal mode.
- Tap the RX/TX readout to choose the target or audio increment (1, 5, 10,
  25, 50, 100 or 1000 Hz). The readout remains visible with the waterfall hidden.
  Rate cycles the target's increments; KHZ selects 1000 Hz. CTR2 zoom adjusts
  the FT8 viewport. RF tuning requires digit selection and awaits CAT echoes;
  fast detents accumulate while older echoes arrive. Module sheets consume
  tuning input, and transmission blocks it. Entry, suspension, connection,
  practice, and mode changes return the target to RX.
- The waterfall embeds the main QK4 `PanadapterRhiWidget`: the same GPU
  spectrum, history shader, palettes, and WTR CLRS mapping. It follows the
  main waterfall's palette and color-range settings. The default viewport is
  0–3000 Hz, with the spectrum above the waterfall and audio-Hz labels between
  them. Audio levels are relative; they are not labeled as radio dBm.
- Waterfall zoom/pan/Fit changes only its viewport. The receiver continues
  analyzing 100–3300 Hz of the available main-RX audio. RX and TX offsets are
  independent with Hold TX enabled; the radio's RF split is a separate feature.
- A short tap on a decoded CQ row, waterfall label, or matching decoded trace
  selects the RX station. A trace without a matching decode selects RX audio
  frequency only. Ambiguous labels/traces open a chooser.
- A 550 ms hold on the waterfall sets TX audio frequency and enables Hold TX,
  leaving RX and the selected partner unchanged. Releasing the hold cannot
  trigger RX selection. Dragging or pinching cancels the hold. RX uses a green
  marker, TX red; selecting another RX station retains a held TX frequency.
- Station backgrounds follow the familiar WSJT-X meanings: green CQ, red
  messages containing your callsign, yellow transmissions, neutral white other
  activity. Worked calls use gray. Selection adds a blue outline without
  replacing the semantic background. TX and own-call colors take priority.
- The main-screen **Hide waterfall / Show waterfall** button toggles the spectrum/waterfall
  and zoom controls. The incoming station list takes the freed space; RX/TX
  offsets remain accessible. Reception, audio history, zoom and tuning remain
  independent of visibility.
- The idle QSO panel contains only the station prompt and Auto/Even/Log row,
  using 56 logical pixels of height. Selecting a station reveals reports,
  Next message, and Clear in a compact panel. The redundant idle help and
  disabled Next row no longer reserve space. Filter/Logbook and QSO buttons
  use 28-pixel rows with the same 11-pixel control text. Receive/CQ/Call/Halt
  retain their larger touch targets. Status uses only its required text height.
  Recovered space goes to the list; the waterfall keeps its established height.
- **Rows** offers Comfortable (two lines, 12 px text, 47 px rows), Compact
  (one line, 12 px text, 28 px rows), and Dense (one line, 10 px text, 20 px
  rows). One-line rows align SNR, audio Hz and message; Comfortable also shows
  UTC and worked text. Full text remains in accessibility data. Colors are
  preserved in every layout. Visibility and density preferences are saved.
- Decode bursts update the list as a batch. Scrolling retains the current
  position; a **N new** button returns to the latest activity. Arrivals during
  a tap or flick wait for the interaction to finish. Density/visibility changes
  retain the reading position and do not arm or halt an exchange.
- Decodes append to the activity list; updates are held while a touch is being
  classified. Selecting a row never silently arms transmission.
- Live main-RX float PCM comes from the existing Opus decode tap, upstream of
  playback routing and volume. The native decoder runs on its own thread with
  a bounded input queue. No microphone, TCP/TLS, or SSTV codec is replaced.
- Mode changes, radio changes, disconnect, and app inactivity invalidate old
  decoder work. Foreground operation only in this milestone.
- Explicit **Practice** mode generates simulated stations and exercises CQ,
  replying, standard report/acknowledgment exchanges, manual message selection,
  Auto, retries, stopping, and logging. Simulated contacts are held in a separate
  in-memory practice log, clearly marked, and never written to the station log.
- Live Call/CQ arms timed standard FT8/FT4 transmission. The audio thread
  generates shaped GFSK (FT8 BT=2, FT4 BT=1), keys ahead of the selected UTC
  period and starts audio at period +500 ms (FT8) or +300 ms (FT4). A late Call
  starts within the current eligible period using the remaining waveform.
  Existing K4 PCM/Opus packet
  transport and calibrated drive are reused. The microphone stays closed.
- Halt cancels both pending and active audio, including queued network work.
  Settings changes, disconnect, screen departure, clock jumps, missed pacing,
  missing audio and the shared meter watchdog stop transmission. The final
  packet must leave the local socket queue before the playback tail and RX
  command. This is transport completion, not a K4 RF-quality acknowledgment.
- Own TX/RX transitions preserve QSO state. Only completed transmissions
  increment retries or complete/log the exchange. An early decoder pass
  finishes normal on-time signals 1.5 seconds before the period boundary.
- This device-test implementation requires DATA-A, split off, TEST off and a
  matching saved audio calibration. It accepts only standard messages whose
  packed payload decodes back to exactly the displayed text. Free text and
  hashed/nonstandard calls are rejected. Reports are calculated automatically
  using the WSJT-X FT8/FT4 estimators, referenced to 2500 Hz noise bandwidth.

## Logging

The shared logbook stores records atomically in `logbook/contacts.json` under
the application's private data directory. It supports search, edit, a review
before saving, optional completion auto-save, ADIF import preview, duplicate
checking, and export of the displayed records. Unfamiliar ADIF fields survive
round trips. A malformed import does not partially modify the log.

FT8 exports as MODE=FT8. FT4 exports as MODE=MFSK and SUBMODE=FT4; legacy
MODE=FT4 imports are normalized. Frequencies are MHz and times are UTC.
Contact RF frequency includes the TX audio offset. ADI export currently
requires ASCII values and reports non-ASCII text instead of losing it silently.
The importer accepts files up to 20 MB. Duplicate identity includes station,
counterparty, band, canonical mode, and UTC start down to the second. Ambiguous
near-duplicates and cross-logger timestamps need a later merge-review workflow.

To transfer WSJT-X history, import a copy of `wsjtx_log.adi`. To transfer back,
export ADIF, back up the desktop log, merge records using a logger or a tested
ADIF merge workflow, then rescan the WSJT-X ADIF log. Do not blindly replace an
existing desktop log. Live UDP logging and multi-device sync are later work.

## Decoder boundaries

The experimental C receiver uses kgoba/ft8_lib at the pinned revision in
`third_party/ft8_lib/UPSTREAM.md`. Upstream MIT/FFT notices are retained.
Normal FT8 and FT4 payloads decode natively; advanced contest/Hound/SuperFox
parity and WSJT-X sensitivity are not claimed. Sync score is kept separate
from measured SNR. [FT8_SNR_REFERENCE.md](FT8_SNR_REFERENCE.md) describes the
ported WSJT-X estimators and comparison tests against its real decoder.
Incoming UTC is estimated from packet arrival and the 12 kHz sample clock;
displayed time offset is uncalibrated for K4 buffering and network latency.

## Next engineering milestones

The screen now includes RF power immediately below the frequency, using the
existing K4/SSTV command path. This controls watts, not streamed audio gain.
[DIGITAL_TX_LEVEL.md](DIGITAL_TX_LEVEL.md) describes the implemented shared
protection and remembered TEST-mode audio calibration, available in both
modules. Device validation of thresholds, metering and RF quality remains
required. Call/CQ now enables live FT8/FT4 QSO transmission after calibration.

1. Review the actual portrait screen on the Galaxy S26 Ultra: dense activity,
   waterfall gestures, small-screen reachability, Options,
   and entry/exit orientation. Confirm SSTV hold action never opens FT8 on release.
2. Compare captured K4 audio and known WSJT-X recordings in both modes. Measure
   decode sensitivity, false positives, SNR, sample clock, network jitter, CPU,
   thermal behavior, and deadline margin. Test portable/nonstandard callsigns.
   Add deliberate receive setup for the K4 data mode and passband; this preview
   decodes the radio's current main-RX audio and does not change its mode/filter.
3. Validate generated FT8/FT4 audio at an independent receiver: frequency,
   UTC offset, complete decodes and spectral quality across supported codecs
   and latency settings. Automated waveform/decoder round trips and scheduler
   completion/cancellation checks pass; real RF acceptance remains pending.
4. Validate RRR/RR73 recovery, repeated acknowledgments, interrupted sessions,
   clock jumps, packet stalls, and exactly-once completion logging on real RF.
5. Add regional/favorite frequency tables, free text,
   PSK Reporter, UDP logging, and deliberate advanced-mode expansion.

## References

- K4-Control: https://documents.roskosch.de/ham-control-elecraft-ios/
- WSJT-X: https://wsjt.sourceforge.io/wsjtx-main_en.html
- FT8/FT4 protocol: https://wsjt.sourceforge.io/FT4_FT8_QEX.pdf
- ADIF: https://adif.org/314/ADIF_314.htm
- Codec: https://github.com/kgoba/ft8_lib

The [baseline feature comparison](FT8_FT4_BASELINES.md) maps the operating
requirements to this preview and later handheld milestones.

## Validation

Entering FT8/FT4 temporarily selects K4 DATA-A. Leaving restores the main-VFO
mode and data submode captured on entry, after any active transmission stops.
Call can start immediately in the current matching transmit period. Following
WSJT-X `mainwindow.cpp` and `Modulator::start`, late transmission skips elapsed
waveform samples to retain UTC symbol timing and the original end time. Nominal
audio starts at 500 ms for FT8 and 300 ms for FT4. Start eligibility ends at 75%
of a period, or earlier when needed to leave audio after the K4's existing
500 ms key-up settling time. Outside that window, Call uses the next matching
period. Timing, drain completion, mode restoration and immediate Call dispatch
are covered by desktop regression tests; simulated timing is not on-air evidence.

Run `test-windows.cmd -Action Test`. `test_ft8` covers the QSO state machine,
retry/stop behavior, unrelated messages, FT8/FT4 native decoding, ADIF field
preservation/normalization, malformed imports, zoom/tuning separation, separate
RX tap/TX hold behavior, drag cancellation, activity colors, CTR2 MIDI routing,
RF digit selection, delayed CAT echoes, shared main/mini-pan
waterfall shaders, and the
portrait screen at 390x800, 360x740, and 320x568 logical pixels. It generates
practice-screen PNGs in the ignored test build directory for visual review.
These capture the actual GPU waterfall together with the native UI. Practice
traces use encoded FT8/FT4 tones in a generated spectrum; they are not RF evidence.

Build with `build-android.cmd -Action Apk`. The release-signed v1.0.5 package
was installed in place and launched on the Samsung SM-S948U on 2026-09-09.
The compact-list follow-up was built and signed later that day, but the phone
disconnected before installation. That layout still awaits device review.
The later power-slider revision passes the Android ARM64 release-target build
and the 24 FT8 checks; that revision has not yet been packaged or installed.
Hands-on FT8/FT4, CTR2, and live-radio acceptance remains pending.

Results on 2026-09-09: all ten Windows suites pass. The FT8 suite reports
24 passing checks including setup/cleanup and both protocol data rows. It also
verifies streamed stereo PCM assembly, capture invalidation, preservation of
an unreadable log, practice/real station identity isolation, independent TX
holding, station colors, and nonblank main/mini-pan waterfall rendering. The three
portrait sizes were rendered and visually reviewed; busy-list checks exercise
40- and 48-message bursts, preserve the reading anchor across density changes,
and defer arrival display until a tap completes. Waterfall-hidden previews at
390x800 show 9 full Comfortable rows or 22 Dense rows with the power slider.
At 360x696 (the Samsung phone's content size), Dense fits 8 full rows with the
waterfall, 17 with it hidden, or 14 with it hidden and a station selected.
At 320x568 those counts are 6, 12, and 9. These counts use practice data; the live status text can
consume another line. Tests verify QSO controls fit inside their panel and
that clearing a selection restores list space. Options keeps Save and
Back reachable while its fields scroll. Android ARM64 debug packaging passed.
Normal and extended CTR2 button formats were replayed through the real MIDI
router into the screen. Tests cover independent audio tuning, explicit RF
tuning, delayed echoes, hidden-waterfall tuning, popup blocking, mode resets,
and long-press frequency entry. The shared frequency-display suite also checks
phone digit selection and hold cancellation. Physical CTR2/K4 operation with
the new FT8 routing remains unvalidated.
The local preview APK is `QK4-FT8-FT4-preview-debug.apk` (ignored by Git).
