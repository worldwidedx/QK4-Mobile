# Project status

Last updated: 2026-09-11

## FT8/FT4 band-change DATA-A retention

Phone testing found that selecting another band inside FT8 could leave the K4
in that band's previously recalled radio mode. While the digital screen is
active, its mode-session controller now reasserts DATA-A when radio readback
reports any other mode or DATA submode. It retains the mode saved on entry and
restores that original mode only when leaving FT8/FT4. A pending-readback guard
prevents repeated mode commands. The focused FT8 suite passes and covers
voice-mode and alternate-DATA recalls, request suppression, rearming after
DATA-A readback, and original-mode restoration. The release-signed build was
installed on the Samsung, and the user confirmed that FT8 band changes retain
DATA-A, the original mode restores on exit, and the other current phone tests pass.

## Macro radio-state refresh (GitHub issue #2)

Fn/app macros and CTR2 macros now use a shared I/O-thread dispatch that sends
the saved command once, followed by `RDY;` and the supplemental state GETs
already used at connection time. This requests the complete radio/display
state, including filters, modes, levels and menu values, rather than only
VFO frequencies. The supplemental GET list is shared with connection setup.
The refresh uses normal incoming RadioState/MenuModel updates and does not
infer state from macro text or rerun audio/connection setup.

This follows current upstream QK4's startup-macro-before-RDY sequence and the
K4 Programmer's Reference Rev D12 (`RDY`, Entire Radio State; `AI4` excludes
the issuing client's own changes). The original `FA3702;` macro stays valid;
users do not need to append their own GET commands.

The TCP/Protocol regression suite covers the reported abbreviated frequency,
full Hz, both VFOs, swaps, band changes, rejected SETs, unchanged CW text,
existing GETs, empty/disconnected execution, filter width/shift, mode, power,
RIT, display freeze, tuning rates and repeated menu definitions. Actual reply
values drive the model and UI signals. All 16 Windows suites pass (98.49 s),
and the Android ARM64 application library builds successfully.
The release-signed phone build includes this correction. During the current
phone acceptance run, the user reported that the macro refresh and all other
checks outside the separately reported FT8 band-mode issue pass.

## One FT8/FT4 tone adjustment workflow

There are only two assignable FT8/FT4 actions: Switch RX/TX tone and Set tone
frequency. One physical button provides both through short/long press. The
separate RX and TX actions are removed from the selector. Older saved CTR2
RX/TX assignments migrate to Switch for short press and Set for long press,
preserving other assignments. Ordinary tone wheel movement also starts a
preview. Set ends adjustment; wheel movement then leaves the committed tone
unchanged until Switch is used again. Touch waterfall selection remains
available. Step/KHZ/RF-selection guards and all three wheel mappings remain.
The mapping setup help and tone sheet use the new names. A rollback checkpoint
preserves the preceding installed build (see FT8_CTR2_ROLLBACK.md).

All 15 Windows suites pass with the two-action change, including import
migration across Normal/Extended short/long notes and all three wheel routes.
The phone's pre-update Button 4 assignments were confirmed as ft8_tx short and
ft8_rx long. The signed ARM64 v1.0.5/code 32 build was installed in place at
17:01 local time on September 11 (SHA-256
`6CF6E67F427472E438E8C90DA62FD223F666C648C082A40D80D17F52235E8E39`).
On-device setup verified Button 4 short = Switch RX/TX tone and long = Set tone
frequency after migration. The selector shows only these two FT8/FT4 actions,
with complete readable labels; it was cancelled without other edits. Back
returned to the radio, reconnect succeeded, and FT8 was left receiving at
14.074 MHz DATA. The user subsequently confirmed on September 11 that this
two-action CTR2 tone workflow is working properly on the phone. This accepts
the Switch → wheel adjustment → Set interaction in the installed build; it
does not establish additional on-air transmit validation. No RF transmission
was initiated during the installation checks.

## CTR2 tone wheel regression correction

The September 10 guardrail APK incorrectly consumed Active VFO frequency and
Other VFO frequency wheel events without moving the selected RX/TX tone.
FT8 RX/TX buttons still selected their targets, but the wheel appeared dead.
The correction restores all three existing wheel routes (both VFO actions and
Selected adjustment) to the module's selected tone or explicitly selected RF
digit. Rate/KHZ, VFO-selection and band-step button guards remain in place.
The FT8 RX/TX actions now show a tone-selection message. Saved mappings and
the Adjust/Set workflow are retained. Tests now exercise all three wheel
mappings in both button modes and both digital modes, covering immediate
RX/TX tuning, preview/commit and accidental step-button presses.

The expanded FT8 suite passes. The phone's saved Home turn mapping was inspected
without editing: Active VFO frequency, CC100, Wheel A. This confirms it uses the
route blocked by the previous build. The signed ARM64 v1.0.5/code 32 update
was installed in place on the Samsung at 16:22 local time on September 11
(SHA-256 `75DCDA61B79F8F0DAA46FF5FDC0C383C546B8D6EBC9EE2D9129CA49083904BF1`).
Launch/reconnect succeeded and the phone was left receiving in FT8 at
14.074 MHz DATA with a live spectrum/waterfall. Saved assignments were not
edited. Physical CTR2 wheel acceptance remains pending; no transmission was
initiated during verification.

## FT8/FT4 tone tuning recovery

The user reported an unusably coarse tone step and RF tuning being selected
while trying to recover; tapping the waterfall restored tone tuning but was
awkward. RF dial, Rate, KHZ, VFO-selection and band-step MIDI actions now require
an explicit tap on a radio-frequency digit. Tone selection disarms them again.
Blocked actions are consumed before console dispatch and cannot change a tone
step, target, preview or committed frequency. Selected adjustment follows the
explicitly selected RF digit or tone; direct VFO dial mappings require RF
selection. The tone sheet has explicit Adjust RX tone, Adjust TX tone,
Set tone frequency and Tone step choices. Choosing a step resumes tone control
and preserves an active preview. Tone steps are limited to 1/5/10/25/50 Hz;
KHZ no longer sets a 1,000 Hz tone step, and old coarse saved steps recover to
5 Hz. Rate and KHZ never adjust the tone step. Short-press Adjust / long-press
Set and all saved assignments are retained.

The tone-recovery FT8/FT4 tests pass, including legacy coarse-step recovery,
preview preservation, TX hold, RX focus and no RF commands from tone controls.
Additional guardrail replays pass for normal/extended CTR2 messages in FT8/FT4,
repeated accidental RF actions during idle, preview and committed states,
explicit RF opt-in and disarming RF through tone selection or module exit.
The compact tone sheet render was checked. The second rollback checkpoint is
documented in [FT8_CTR2_ROLLBACK.md](FT8_CTR2_ROLLBACK.md). The Android ARM64
v1.0.5/code 32 release APK builds and verifies with the existing signing
certificate (SHA-256
`36978C1B6EDA7C137ACDD73224C5FCF9FC8DA96CF29CA8DEE1E6601C2273AF51`).
The signed artifact is `QK4-Mobile-v1.0.5-release.apk`. After the user reattached
the Samsung, it was installed in place at 22:18 local time on September 10.
Launch and reconnect succeeded. The phone was left in FT8 at 14.074 MHz DATA
with a live spectrum/waterfall, received decode rows and the RX tone / 5 Hz
readout. Physical CTR2 button/step acceptance remains for the user's testing;
no transmission was initiated during installation verification.

## CTR2 assignable FT8/FT4 Adjust/Set workflow

New assignable actions `adjust_ft8_rx_tx` (Adjust: FT8/FT4 RX/TX) and
`set_ft8_frequency` (Set FT8/FT4 frequency) support any surrounding button.
Map them to short/long press and map a dial to Selected adjustment. Short
press toggles RX/TX, dial movement positions a dashed preview, and long press
commits the frequency. Solid markers and operating frequencies do not follow
preview movement. Commit stops further dial changes until Adjust is selected.
TX commit enables Hold TX. RX commit leaves TX unchanged and opens My QSO,
filtering within ±25 Hz for FT8 or ±45 Hz for FT4 plus own TX messages. All
still shows every decoded frequency. No saved button assignments are changed.
The source checkpoint and previous signed APK are documented in
[FT8_CTR2_ROLLBACK.md](FT8_CTR2_ROLLBACK.md).

Automated workflow tests cover FT8/FT4, Normal/Extended mode, non-default
button and dial assignments, preview isolation, commit, held TX, RX focus,
full-band All, new decodes, repeated/ignored MIDI edges and TX/disconnect
guards. All 15 Windows suites pass; preview and focused-list desktop renders
were visually checked. The final FT8 and mapping suites were rerun after the
focus-preservation and export-grouping corrections and passed. The signed
ARM64 v1.0.5/code 32 APK was installed in place on the Samsung at 21:46 local
time (SHA-256 `9462B755789D649DC8EED21AD2B2027B11BDDE35A9A8709C0C4FB63BAFAC9D3A`).
Both new actions were visible in the phone's scrollable mapping selector;
selection was cancelled without editing saved assignments. Setup returned
to the radio with Back, and the K4 reconnected at 14.074 MHz DATA. The final
APK was left in FT8 with a live spectrum/waterfall and received decode rows;
the callsign labels are gone and the traffic list has the reclaimed height.
Physical CTR2 preview/commit acceptance remains for the user's testing.

## CTR2 button events and FT8 traffic space

CTR2 front-panel short/long actions now execute on positive NoteOn, the
message the hardware sends on physical release. The router previously waited
for NoteOff, so NoteOn-only buttons did nothing even while knob tuning worked.
CW/key/PTT edges and other MIDI profiles retain their existing handling.
Regression replays cover every normal/extended button, repeated NoteOn-only
gestures, ignored NoteOff/zero-velocity events, macros and adjustment selectors.
The FT8 RX/TX dial test now uses the documented hardware messages.

Removed the CQ callsign labels above the FT8/FT4 waterfall. Its spectrum area
is 24 logical pixels shorter, returning that height to the traffic list while
retaining waterfall history height and RX/TX markers. Station selection uses
signal frequency or the full traffic list. All 15 Windows suites pass; the
updated phone-size desktop render has been visually checked. The ARM64 release
APK builds and verifies with the existing WorldWideDX signing certificate
(SHA-256 `28A121A32CB9B85C191E736D4C8D1431160EF8F1A831546C7705290A29ADF8BA`).
Installation and physical CTR2 verification are pending; ADB detects no phone.
The subsequent proposal for separate Adjust/Set RX and TX actions with a
preview marker and long-press commit has not been implemented in this APK.

## Setup Back navigation

All Options pages (including CTR2, CW Keyer and Fn Key Setup), connection setup,
macro setup, the radio MENU, CTRL drawer and transient radio setup popups now
consume the complete Android Back/Escape action before focused controls can
propagate it to activity exit. Closing occurs on release. The first Back hides
an open keyboard; nested selectors/messages consume their own Back. CTR2 uses
the existing Return to Operate path, preserving its unsaved mapping review.
Opening Settings also dismisses underlying radio menus so returning reveals
the main controls. FT8/FT4, SSTV, logbook and TX-audio setup sheets already use
the protected InWindowDialog route. All 15 Windows suites pass, including a
new suite for focused controls, reopen cycles, nested overlays, unsaved-change
review and native-window isolation. The release-signed v1.0.5/code 32 update
was installed on the Samsung at 21:05 local time (SHA-256
`B9C4D6D52626083363E45315DAB8B33A9422B241B53C63921A3F6FDD59DE3622`).
Physical navigation-Back taps verified return to the main radio screen from
About, CW Keyer, CTR2, Fn Key Setup, connection setup, CTRL, MENU and Display.
With the CTR2 configuration-name field focused, the first Back hid Gboard and
the second returned to the radio. Its nested device selector returned to CTR2
before the next Back returned to the radio. No settings were edited. The K4
was reconnected afterward and left on the main screen with live receive data.

## FT8 / FT4 development preview

FT8/FT4 entry now selects DATA-A and saves the prior main-VFO mode/data
submode for restoration when returning to the radio screen. Restoration waits
for active TX to stop. Call now uses the current matching transmit period when
still eligible, following WSJT-X's late-start method: skip elapsed waveform
samples and retain the original UTC symbol positions and end time. FT4's
nominal audio offset is corrected to 300 ms; FT8 remains 500 ms. Existing K4
key-up settling, calibrated drive, audio pacing and stop protections remain.
All 14 Windows suites pass, including 57 FT8 checks covering mode restoration,
late sample alignment, drain completion, immediate Call dispatch, duplicate
suppression, opposite-period waiting and calibration fault handling.
Android ARM64 debug compilation and APK packaging also pass, using the existing
`qk4-ft8-android` temporary build directory.
The user explicitly authorized reuse of the previously supplied signing
credentials after the initial approval-review block. The release-signed update
was installed in place on the Samsung at 20:48 local time on September 10
(v1.0.5/code 32, SHA-256
`1F484BDD6DA3C35EB8A21EEEFC41EACA4BB984409D6FE7FCCBE88CF01B2084D7`).
After unlocking, connected-K4 checks at 14.074 MHz verified USB → DATA on FT8
entry and USB restoration through the Radio button. A second check verified
AFSK → DATA on entry and AFSK restoration through Android Back. Live RX audio
and waterfall resumed in the module. The radio was left connected on the main
screen in its original DATA mode. On-air late-start acceptance remains pending.

The SSTV Log QSO review now uses a compact content-sized panel, capped at
300 logical pixels wide, with a 180-pixel callsign editor and a clear cyan
border. Save/Cancel sit directly below the contact details. The panel recalculates
its height for orientation changes and validation feedback; the redundant
review prompt is hidden until an error needs to be shown. Portrait and landscape
desktop captures were inspected, and both logbook regression suites pass.
The signed update was installed on the Samsung at 16:09 local time (APK SHA-256
`2D34A05CED0BE9E5AB7BC6C85471E301B9ADF90F2DE31F20BD39A5F8FCAC44C1`).
Device screenshots verified the compact bordered review in both orientations,
with no unnecessary scrollbar or blank area. The radio was reconnected and
SSTV showed STREAM OK. Temporary rotation overrides were restored afterward.

The logbook Back correction consumes the complete Back/Escape key action in
the active in-window sheet before focused child controls or the invoking screen
can handle it. Nested Setup/editor sheets close one level at a time, and focus
returns to the invoking control. When the keyboard is visible, the first Back
only hides it. Regression coverage includes focused lists/editors, unaccepted
key events, complete press/release handling, nested Setup and focus restoration.
All 14 desktop suites passed before the keyboard follow-up, with the logbook
and QRZ suites passing again after that follow-up.
The final signed v1.0.5/code 32 APK was installed in place on the Samsung at
15:40 local time (SHA-256
`6CBC7FA3B936D3C08723CAD5061E036FEEF0B6295397ED6C6F6BD040C36822DB`).
Device checks verified return to radio, SSTV Receive, and the FT8/FT4 screen;
nested QRZ Setup and Add returned to the logbook. The keyboard follow-up was
verified with the search field focused: first Back hides Gboard and keeps the
log open; the next tap of Android's navigation Back returns to FT4. The app
remained running. These were navigation checks with the radio disconnected.

The September 10 logbook correction gives the read-only QRZ checkbox a fixed
gutter in both selected and unselected rows. A tap anywhere in a contact row
commits selection on release; a drag cancels it. Send/Edit no longer silently
discard separate button taps because of a stale list-scroll flag. Setup adds
Show/Hide for typed or saved API keys, with masking when the app loses focus.
Targeted logbook and QRZ regression suites pass. The signed correction was
installed on the Samsung at 15:19 local time (APK SHA-256
`30C3FDB769B071ED9844716549C8D1744A178B55F3A0CF6308F95E8E3513C946`).
Device taps verified K3NT selection, clear checkbox/text separation, enabled
Send to QRZ, and Show/Hide with temporary test text discarded through Cancel.
The existing encrypted API key remained available after the update. No QSO
was uploaded during this UI check.

QRZ integration adds Logbook Setup (callsign, Android Keystore-encrypted API
key, auto-send new contacts), manual Send to QRZ, and read-only per-contact
upload checkboxes. Only a confirmed INSERT response marks success. Errors
remain retryable; interrupted uploads do not blindly resend or replace remote
QSOs. ADIF exports the standard QRZ upload status/date, including modified-after-
upload status for local edits. All 14 desktop suites pass, including final UI
checks and rejection of imported private upload-queue metadata.
Live QRZ upload acceptance remains pending an operator-provided API key.
See [QRZ Logbook](QRZ_LOGBOOK.md) for setup, storage and transport details.
The release-signed v1.0.5/code 32 update was installed in place on the Samsung
on September 10 at 15:00 local time (APK SHA-256
`FE87488D0C98496E48DD00738AE98CFE286204F9045FB71DA4C2F08B728B62E5`).
Device checks showed the preserved K3NT record, DXLIST long-press Log,
landscape and FT portrait log/QRZ setup layouts, both SSTV Log QSO buttons,
and the RX callsign review dialog. The radio was reconnected and FT4 reception
showed fresh decodes afterward. Physical sensor-turn acceptance and a live
QRZ upload/Keystore persistence cycle with an operator key remain pending.

The ADIF logbook is now shared across FT8/FT4, SSTV and the main radio screen.
SSTV Receive and Transmit have compact Log QSO actions that review an editable
RX CALL or TO CALL respectively before Save/Cancel. RX history supplies the
received image's frequency/time; TX uses the transmit VFO. Both pages can open
the logbook, whose Add form supports arbitrary modes, reports, dates, band or
frequency, station details, notes and extra ADIF fields. DXLIST retains its tap
action and offers Log on long press. FT8/FT4 stays portrait-only on phones;
tablet-specific landscape support is an accepted contribution direction under
the device-class rules in [Screen orientation policy](ORIENTATION_POLICY.md).
SSTV logging
follows either sensor orientation. The radio log opens in landscape, enables
both orientations after a phone turn, and restores landscape on exit. Shared
storage refreshes before writes so separately opened screens preserve each
other's contacts.

FT8 and FT4 now share one saved TX audio calibration for the same radio/audio
setup. Upgrades adopt the latest completed level from either mode, and live
drive reductions carry across mode switches. SSTV remains separate.
The shared-calibration migration and digital UI tests pass. The release-signed
update was installed in place on the Samsung after its FT4 QSO with K3NT
completed and the screen confirmed the contact was saved. That preceding
device run showed successful timed TX completion, received R-14, sent RR73,
automatic return to RX and contact logging.

The September 10 correction removes the station-selection decode age limit,
refreshes FT reception on the K4's TX-to-RX transition after calibration or a
QSO transmission, and preserves partial receive slots during unchanged capture
readiness checks. Calibration matching now ignores audio offset, RF power,
band, packet latency and firmware readback; actual audio gain settings remain
matched. Existing hash-only records can migrate across an audio-offset change.
The phone's preceding calibration reached raw ALC 5 at -39.1 dB. Regression
tests cover older calls, capture resume, and saved-level reuse; all 12 suites
passed. The signed update was installed on the Samsung on September 10 at
14:01 local time. The device recognizes the existing saved calibration and
received 23 FT8 decodes in one period. A fresh calibration-to-call cycle and
on-air transmission acceptance remain pending.

FT8 work began on 2026-09-08 in `codex/ft8-ft4-portrait`, initially using the
older `v1.0.3` tag. CTR2 work already existed; its absence in that preview was
a baseline-selection error, not the order in which the features were started.
The preview now incorporates the existing CTR2 source through `830cdb0` on
`codex/ctr2-midi-v2` (the v1.0.4.1 line), including its Android MIDI sessions,
mapping editor, radio controls, and regression tests. Other worktrees remain
untouched. The integrated test package is version 1.0.5 (Android version code
32). It has not been published as a repository release.
It adds a portrait FT8/FT4 screen, native reception from
the existing main-RX audio, waterfall zoom and station selection, common
band frequencies plus custom entry, practice QSO sequences, and an ADIF
logbook. The former SSTV Fn button opens FT8/FT4 on tap and SSTV on hold.
The revised 3 kHz view uses QK4's actual main spectrum/waterfall renderer and
follows its palette and WTR CLRS settings. Tap a decoded station for RX; hold
an open frequency to set and hold TX independently. Drag/pinch cancels a hold,
and releasing a completed hold cannot select RX. Activity backgrounds retain
WSJT-X color meanings, including when selected.
Hide waterfall / Show waterfall toggles the spectrum and expands the station list.
Rows offers Comfortable, Compact and Dense layouts with saved preferences.
Decode batches preserve the reading position and tapped station; a new-message
button returns to the latest activity. At 390x800 with the waterfall hidden,
the preview with its power slider shows 9 full Comfortable rows or 22 Dense
rows. The idle QSO panel is 56 logical pixels tall; unused exchange rows and
Clear are hidden.
Activity filters and QSO buttons use compact 28-pixel rows. The waterfall
retains its phone height, so all recovered space goes to received stations.
At the device's 360x696 content size, Dense shows 8 complete rows with the
waterfall, 17 without it, or 14 with an active QSO and waterfall hidden.
The compact RF power slider below the frequency uses the existing SSTV/K4
power control, follows radio updates, and commits a drag on release. Practice
changes remain local. Both digital modules now offer explicit TEST-mode TX
audio calibration and remember levels for matching radio/mode/input
settings. SSTV requires a matching calibration and uses shared automatic
drive reduction, headroom checks, and an I/O-thread stop on excessive ALC or
missing metering. Protection messages stay visible within each module.
Automated policy, network, UI, and host Opus tests pass; actual K4 thresholds,
TM query support, TEST behavior, and RF quality still require device evidence.
Live FT8/FT4 QSO transmission is now implemented for device testing. Call/CQ
arms the selected even/odd period; a dedicated audio-thread scheduler sends
standard FT8/FT4 GFSK through the existing calibrated PCM/Opus transport.
Halt, departure, disconnect, settings changes, missed deadlines and shared
protection cancel the generation. The I/O watchdog also stops missing timed
audio independently of the audio/UI threads. The final packet drains before
the playback tail and RX command; only successful completion advances QSO
state. Own TX/RX transitions preserve the exchange. Early receive decoding
allows an adjacent-slot reply. DATA-A, split off, TEST off and matching audio
calibration are required. Reports are now measured automatically using the
WSJT-X 2.7.0 FT8/FT4 estimators; manual report entry has been removed. Free text, hashed/nonstandard
calls and special contest modes are rejected. All twelve normal test suites
passed, including 35 FT8 checks; the final digital watchdog revision passes
25 protection/network checks. On-air decode and timing acceptance are pending.
Implementation limits are in [DIGITAL_TX_LEVEL.md](DIGITAL_TX_LEVEL.md).
The automatic-report correction is documented in
[FT8_SNR_REFERENCE.md](FT8_SNR_REFERENCE.md). Identical-WAV comparisons against
the installed WSJT-X 2.7.0 decoder match all six FT4 reports and agree within
1 dB for six FT8 reports, including weak/strong signals and gain changes.
Measured reports feed station selection and the standard automatic exchange.
All 12 suites pass, including 49 FT8/FT4 checks and the real WSJT-X comparison.
The automatic-report APK was release-signed and installed in place on the
Samsung at 22:26 on 2026-09-09, retaining app data and calibration. Android
reports 1.0.5/code 32. The phone remains locked; live radio acceptance is pending.
APK SHA-256:
`d462b6ea541d18f8899cdd05447f26d614c638dfedd7c5436c9b42169ee704de`.

The live-TX revision was release-signed with the WorldWideDX certificate and
installed in place on Samsung SM-S948U on 2026-09-09 at 22:07 local time.
Android confirms 1.0.5/code 32 and the application process starts; the phone
is locked, so the final on-device screen and on-air checks await the operator.
App data was retained. APK SHA-256:
`af0e2bef10d0ac17fa46646a0ff27ca105d26c08d3d8d0c6cb097c8b73bf9ded`.

The compact layout, power slider, calibration and protection revisions are
now packaged together in a WorldWideDX release-signed 1.0.5 APK. This revision
was installed in place and launched on the Samsung SM-S948U on 2026-09-09
at 19:40 local time; Android reports version 1.0.5/code 32. App data was
retained. K4 calibration and RF behavior still require physical validation.
Automated checks pass: all twelve normal test suites
(including 25 FT8, 16 digital protection/network and 6 digital UI checks),
plus seven optional Opus round-trip/headroom checks.

Follow-up calibration correction: DATA readiness now follows RadioState
readback and gates Calibrate. Moderate startup ALC settles for 600 ms; a
minimum-drive stop needs three spaced high samples over 600 ms. Emergency
raw ALC 10, RF/compression, missing-meter and missing-tone stops remain active.
Exact TM/raw ALC/drive diagnostics are visible during calibration and retained
on faults. All 12 normal suites (19 digital TX/network checks) and optional
Opus checks pass; the updated ARM64 release APK builds and verifies against
the WorldWideDX certificate. Installed in place and launched on Samsung
SM-S948U (R3GL30BM96Y) on 2026-09-09 at 21:14 local time, retaining app data;
Android reports 1.0.5/code 32. The APK SHA-256 is
`67ab1bc40c9a73397c18f7b41005b7c9b0db83723420b088edd4d2df276f347f`.
Subsequent live calibration captures repeatedly report `TM007000000010;`
(raw ALC 7, CMP 0, power 0, SWR 1.0) at minimum gain −30.1 dB; the operator
reports about 6 on the physical ALC scale. Four attempts confirm sustained
raw 7 still trips the provisional raw-5 policy after sample validation.
This is a device observation, not a validated scale conversion or RF result.
Raw thresholds are still provisional; see DIGITAL_TX_LEVEL.md for the
manual's eight-K4-bars guidance and the unresolved mapping.

The next correction separates the −30.1 dB calibration starting level from
its lower bound. TEST calibration now continues reducing drive in 3 dB
steps, with 600 ms settling per adjustment, down to the S16 quantization
bound (1/32768) or its existing 15-second timeout. It saves and reuses lower
levels without clamping them back to 1/32. Raw emergency ALC, RF/compression,
fresh-feedback, audio headroom and live-TX sustained-high protections remain.
All 12 normal suites pass (22 digital TX/network checks), including a fake
K4 that reports raw 7 until drive falls below −42 dB and then allows successful
calibration. Eight optional Opus checks pass, including lower-drive amplitude
through encoding/decoding. These are software results; K4 response to the
extended attenuation range still requires the next device test.
The correction was release-signed with the existing WorldWideDX certificate,
installed in place on the Samsung and launched at 21:25 local time on
2026-09-09 (1.0.5/code 32), preserving app data. APK SHA-256:
`1d27fc7d89ac7beff720cdf9658093c4af51df5ff6a19cde2b68375c36a61420`.

The 21:26 K4 run confirmed lower drive reduces ALC, but the raw-3–4 target
caused repeated switching between raw 5 at −42.1 dB and raw 2 at −45.2 dB,
ending in the 15-second timeout without a saved result. The operator explicitly
accepted raw ALC 5 as sufficient. The next correction therefore accepts stable
raw 3–5, reduces at 6 and retains the raw-10 emergency stop; no finer search
is performed after reaching 5. All 23 digital TX/network checks pass, including
raw-5 acceptance after reductions in each digital mode. Hardware completion
and the final saved drive await the next K4 calibration.
This raw-5 acceptance correction was release-signed, installed in place and
launched on the Samsung at 21:32 local time on 2026-09-09 (1.0.5/code 32),
retaining app data. APK SHA-256:
`fba12e6628a3a8fa22a1e8f446a6eedbe0c3a0cff252c102ab279c973ccf8b0d`.
After this installation the operator reported that calibration is working
well on the Samsung/K4 setup. Retain the accepted raw-3–5 behavior and lower
drive range. The final saved gain was not captured in the available log;
this workflow acceptance does not establish RF/spectral quality.

CTR2's VFO tuning knob follows the visible **Dial RX / TX / RF** target.
Map **FT8/FT4 RX** to a button's short press and **FT8/FT4 TX** to its long
press. Either opens the module if needed. RX/TX changes only the corresponding
audio offset; tapping a digit of the shared main frequency display selects
RF tuning at that digit's step. Holding the display opens the shared-format
frequency entry. Rate/KHZ and zoom operate locally while FT8 is visible.
RX is the default on entry and after connection/mode changes. A sample mapping
and the operating details are in [CTR2_UI_ACTIONS.md](CTR2_UI_ACTIONS.md#ft8--ft4-dial).

Live FT8/FT4 QSO transmission is not connected in this first milestone. Practice
uses simulated stations and a separate logbook. Android ARM64 debug packaging
passed, together with all ten native test suites. The layout revision passes
24 FT8 checks including setup/cleanup. The WorldWideDX release-signed 1.0.5
package was installed in place and launched on the Samsung SM-S948U on
2026-09-09; Android retained the
original installation and app data. FT8/FT4 and CTR2 operation still require
hands-on device/K4 acceptance testing.
See [FT8_FT4_SCOPE.md](FT8_FT4_SCOPE.md) for
implementation boundaries, sources, and the remaining engineering work.

## Released build

![QK4 Mobile v1.0.5 confirmed FT4 QSO](images/QK4-Mobile-v1.0.5-FT4-Confirmed-QSO.png)

The September 11 device capture shows a confirmed FT4 QSO with DJ6OI at
14.080 MHz, including sent −05 and received −14 reports, RR73, calibrated TX
protection, live spectrum/waterfall activity, and the integrated Log QSO action.

**QK4 Mobile v1.0.5 test build** integrates the FT8/FT4 portrait preview with
the existing CTR2-MIDI implementation. It uses Android version code 32. The
WorldWideDX release certificate matches the installed v1.0.4.1 certificate;
the package upgraded in place and launched on the Samsung SM-S948U. This build
has not been published as a repository release.

**QK4 Mobile v1.0.3** balances weak-signal SSTV recovery with stronger false-
start rejection. A damaged-header recovery remains provisional until three
consecutive, tightly timed line-sync pulses confirm it; unconfirmed candidates
return silently to AUTO RX. Ten private Main-RX false-trigger captures replay
with no mode events or completed images, while deterministic coverage includes
all 22 supported modes at 5 dB input SNR and a range of header impairments. The
SSTV transmit screen keeps its original portrait layout and moves FSK ID, CW ID,
and CW speed below the left-aligned MY CALL field in landscape. Low-latency CW
sidetone now follows speaker, Bluetooth, and USB-C output changes in both
directions; those routes passed physical testing with TinyMIDI on the Samsung
Galaxy S26 Ultra. Android hearing-aid sidetone routing is implemented where the
device is exposed as `TYPE_HEARING_AID`, but still awaits hardware validation.
Android version code is 29. The WorldWideDX-signed ARM64 APK was installed in
place, verified byte-for-byte against the source package, and cold-launched.

**QK4 Mobile v1.0.2** adds complete SSTV templates that can optionally retain
their source image, crop, zoom, position, text, and markup while preserving
layout-only templates for use over the current TX image. Built-in CQ, REPORT,
and 73 templates can be customized directly and restored to their factory
versions. SSTV mode, font, template, and retention selectors now render inside
the existing SSTV window, avoiding the Android EGL-surface crash caused by
rapid native popup creation and teardown. The release also guards and rebuilds
the local CW sidetone audio device when Android invalidates its output handle,
correcting the TinyMIDI test dit/dah crash. Android version code is 28. The
release-signed ARM64 package was signature-verified and installed in place on
the Samsung Galaxy S26 Ultra; complete image-template recall and TinyMIDI
dit/dah testing passed on the device.

**QK4 Mobile v1.0.1** adds K4-compatible reverse alternate taps for DATA,
AFSK, FSK, and PSK, including reverse-aware VFO labels and panadapter/mini-pan
orientation. It also refines both SSTV image editors with selectable ordered
objects, shape fill/outline controls, 45-degree object rotation, true
undo/redo, dynamic callsign variables, responsive portrait/landscape layouts,
and a shared 12-112 px text-size range while retaining existing saved-template
compatibility. Android version code is 27. All 16 focused SSTV composer tests
pass, and the release-signed ARM64 package was installed in place and tested
successfully on the Samsung Galaxy S26 Ultra.

**QK4 Mobile v1.0** is the first complete QK4 Mobile release with integrated
CW device support and integrated SSTV. The new SSTV workspace provides
automatic receive, transmit, exact-mode image composition, reusable templates,
post-image FSK/CW identification, callsign-aware reply, and retained RX history
across 22 modes. SSTV reception was tested through extended live monitoring
and interactive debugging while remotely connected to an Elecraft K4 and
receiving varied real-world signals. Transmit audio was captured over the air
by a remote WebSDR and successfully processed through a local SSTV decoder.
This release also adds the searchable DX Prefix List, changes the Android
application ID to `com.w9wdx.qk4phone`, keeps the display awake while the app
is active, and simplifies the mobile CTRL drawer by removing MON and BAL.
The release-signed ARM64 APK was signature-verified, confirmed as version code
26 / version 1.0, and installed on the Samsung Galaxy S26 Ultra.

**QK4 Mobile v0.8.3** corrects transverter frequency rendering so leading
digits are retained at VHF, UHF, and higher displayed frequencies. Direct
frequency entry for VFO A and VFO B now accepts ordinary radio-style MHz
shorthand: `7.2`, `7.215`, and `144.2` imply the omitted trailing zeros, while
fully grouped values and raw-Hz input remain supported. The release-signed
candidate was installed in place on the Samsung Galaxy S26 Ultra; automated
parser and display-format tests passed. Final K4/XVTR operating validation is
still pending.

**QK4 Mobile v0.8.2** corrects raspy Android receive audio that was most
noticeable on steady CW and digital signals. RX resampling now remains
continuous across K4 packet boundaries, partial non-blocking Android playback
writes are preserved, and later packets remain queued until earlier audio has
been accepted. The release-signed build was installed in place and the
improvement was confirmed while receiving CW on the Samsung Galaxy S26 Ultra.

**QK4 Mobile v0.8.1** adds combined USB and Bluetooth LE MIDI
discovery for the CW Keyer setup. Opening the screen no longer starts a BLE
scan; SCAN enumerates attached USB MIDI devices immediately while discovering
BLE MIDI devices, labels both transports in one selector, and remembers the
selected transport and USB identity for reconnecting.

**QK4 Mobile v0.8.0** is the current major-release source state. It fixes
the EQ preset-name editor so it appears above the graphic-EQ popup, enlarges
the preset recall/save controls for touch use, aligns the dB and Hz labels,
and adds deliberate long-press clearing for populated presets. The shared EQ
popup applies these changes to Main RX, Sub RX, and TX.

The preceding v0.7.6.5
corrects the Main RX, Sub RX, and TX graphic-EQ **FLAT** controls: the first
tap sets flat response and the second restores the exact prior eight-band
curve. Main and Sub RX share their RX EQ restore curve, matching the K4's
shared RX EQ behavior, while TX EQ restores independently. These controls
update K4 radio EQ settings. For mobile use, start with K4 RX EQ flat and use
phone/headset tone controls for personal listening preference.

The preceding v0.7.7.0 gives the formerly unused GEN BAND control a mobile-only
shortwave-listening bank: the 14 broadcast-band labels tune the active VFO to
AM defaults and retain a persistent, local last-used frequency per GEN band.
GEN preserves the K4's normal direct-frequency and nearest amateur-band-stack
behavior; it does not alter regular BN amateur-band selection or stacking.

The preceding v0.7.6.4 fixes the four right-side CTRL long-press adjustment
editors (ATTN, NB LEVEL, NR ADJ, and NTCH MANUAL): CTRL now dismisses before
the requested popup opens, and each editor provides a visible **↩** close
control. This is a touch-layout fix only; K4 radio/audio/protocol behavior is
unchanged.

The preceding v0.7.6.3 adds an RX-only Android hearing-aid output preference
for endpoints reported as `TYPE_HEARING_AID`. The change uses the existing
native Android media playback track and device-change rebuild path; it does
not change K4 audio streaming, TX, PTT, Bluetooth headset behavior, USB-C
behavior, or microphone selection. Field validation with Starkey Livio 2400
hearing aids is pending.

The preceding v0.7.6.2 point release temporarily forces the proven compact
landscape phone layout on every Android display size, including tablets, so
unvalidated alternate tablet geometry is not selected. The original detection
logic remains commented in `src/ui/k4styles.cpp` for restoration after physical
tablet testing.

The preceding release-signed ARM64 build, v0.7.6.1, added USB-C headset RX/TX
hot-swap after the radio session begins while preserving the Android TLS runtime
and Bluetooth headset routing across TX/RX transitions. Where Android supports
independent routes, Bluetooth RX can remain active while a USB-C headset
microphone provides TX audio. The product package is `com.ai5qk.qk4phone`,
with Android API 26 minimum and API 34 target.

Version 0.7.4 adds an **experimental** in-window TX input shield. During a
phone-initiated transmit state, it blocks all other console touch input while
leaving the red TX ON control available to return to RX. It is UI-only and
does not alter K4 PTT, CAT, microphone, or audio-stream behavior.

Version 0.7.5 adds Android spectrum-renderer compatibility for devices that
could render waterfall data while omitting the normal spectrum trace. It also
adds touch-first B SET cancellation, receiver-specific filter cycling from the
displayed filter shapes, and more forgiving A/B MODE touch targets. The normal
spectrum renderer remains the only rendering path changed; waterfall, K4
protocol, audio, and PTT behavior are unchanged.

The only physical UI acceptance device so far is a Samsung Galaxy S26 Ultra in
landscape. Until tablet testing is available, all screen sizes deliberately use
the compact layout; broader device validation is still required.

## Verified Android behavior

- K4 profile management, TCP/TLS connection, and disconnect/error handling.
- K4 RX audio streaming plus microphone TX audio and deliberate phone PTT
  operation, physically retested on the development K4 after the v0.7.3 fix.
- USB-C headset receive and transmit hot-swap, physically tested after radio
  connection. Android reports the active USB headset microphone input during
  transmit; Bluetooth RX remains available when Android maintains a split route.
- Experimental TX input shield, physically tested during a successful contact
  on the Samsung Galaxy S26 Ultra; broader device and field testing remains
  required before treating it as fully validated.
- VFO A/B operation, transmission-VFO selection, tuning digit selection,
  direct frequency entry, and panadapter tuning at the selected VFO step.
- Spectrum, waterfall, mini-pan, 50/50 initial spectrum/waterfall split, and
  user-adjustable waterfall height.
- Phone-oriented Control, TX, DISP, FN, Main RX, and Sub RX touch menus;
  touch-safe scrolling and long-press alternate actions.
- TX secondary editors dismiss with their parent menu after confirmation; the
  right CTRL-bank REV control is guarded against accidental activation while
  vertically scrolling.
- AF controls for main/sub receiver; relevant slider controls and mode-aware
  filter shift/bandwidth ranges.
- RIT/XIT activation and long-press jog control.
- CW text decode screen and F1–F8 macro editor/execution.
- Local non-decaying red Peak Hold trace, reset on toggle/geometry changes.
- Local WTR CLRS 5–30 brightness adjustment; it intentionally does not send a
  CAT command because it maps the application's local waterfall LUT.

## Known boundaries / next validation

### Next-build enhancements

- Add CTR2 MIDI support. Confirm the CTR2 MIDI transport, messages, control
  mapping, discovery, persistence, and touch-setup requirements against the
  device documentation before implementation. The completed functional scope,
  device-specific keying boundaries, mappings, and validation checklist are in
  `docs/CTR2_MIDI_SCOPE.md`; resume from that document.

### Included in v1.0

- FN > DX LIST now opens a local **DX PREFIX LIST** reference screen rather
  than the former informational placeholder. The supplied prefix/country data
  is sorted with numbered prefixes first followed by A-Z, supports direct
  touch-drag scrolling, and searches both prefixes and country/area text with
  previous/next navigation through multiple matches. It is read-only local UI
  and sends no CAT command or radio setting.
- The Android application ID is now `com.w9wdx.qk4phone`. A guarded one-time
  development-device migration copies the former `com.ai5qk.qk4phone`
  package's private QK4 settings and SSTV data, verifies every file by SHA-256,
  and removes the old package only after successful verification. Because
  Android isolates package data, this `run-as` migration applies to debuggable
  builds; a public release migration would require an old-package export
  bridge signed with the release key. On the Samsung Galaxy S26 Ultra, the
  three existing settings/draft files were hash-verified in the new package,
  the old debug package was removed, and the renamed activity cold-launched
  successfully.
- Android now requests keep-screen-on while the QK4 activity is in the
  foreground and releases the request when the activity is paused. This does
  not alter the user's system-wide screen-timeout setting. The behavior was
  observed on the Samsung Galaxy S26 Ultra during live testing.
- The in-window SSTV foundation now supports automatic Main-RX VIS detection,
  progressive receive decode with automatic slant correction, and image RX/TX
  in all 22 registered modes: Robot 36; Martin M1/M2/M3/M4; Scottie
  S1/S2/DX/S3/S4; PD-50/90/120/160/180/240/290; Wraase SC2-120/180; and
  Pasokon P3/P5/P7. Robot 36 now uses canonical even-Cr/odd-Cb paired chroma.
  The codec is native C++; mode timing and the Robot correction were
  cross-checked against Open-SSTV by Kevin, W0AEZ. TX uses an exclusive,
  generation-gated program-audio path with microphone exclusion, socket
  backpressure failure, immediate STOP/RX cleanup, final-packet progress, and
  automatic return to RX. Android Photo Picker/camera cancellation and activity
  state are handled without replacing the previous prepared image. All native
  tests and the ARM64 APK build pass. Live receive testing and WebSDR-captured
  transmit decoding validate the end-to-end K4 path, although every one of the
  22 modes has not been independently exercised over the air. Open-SSTV's
  image-linked QSO log and ADIF export are planned in
  detail as a deferred, dedicated future branch in `docs/SSTV_SCOPE.md`; no log
  implementation belongs in the current SSTV codec/RX/TX work.
- SSTV TX keying uses the previously proven K4 `TX;` path followed by a fixed
  500 ms key-up guard before program audio; it does not require a `TQ1` echo to
  begin or cancel merely because that echo is absent. Each send resets the
  program-audio codec and sequence, carries 400 ms of silent audio before the
  complete VIS header, and carries 300 ms of silent audio plus an
  SL-frame-aware drain before `RX;`. Immediate STOP still closes the audio gate
  and unkeys without waiting. A USB-connected device test completed the full
  application TX lifecycle and automatic return to RX without a socket stall,
  packet rejection, or early RX-state cancellation. Subsequent transmissions
  were recorded over the air by a remote WebSDR and successfully processed by
  a local SSTV decoder, validating the leader, VIS header, image data, and
  ending sequence. The TX screen reports ALC at or above 5 so the operator can
  reduce K4 DATA/LINE input gain; automatic gain changes are intentionally not
  made without radio measurements. The `MY CALL` and `TO CALL` fields share the
  same compact nine-character width.
- K4 MON and BAL controls are intentionally omitted from the mobile CTRL
  drawer. Delayed transmit-monitor return audio is not a useful representation
  of the transmitted signal and would be objectionable to monitor while
  speaking, so the former MON button, overlay, `SW128`, and `ML` adjustment path
  have been removed. NORM now sits with BW/SHFT and invokes the K4
  nominal-filter action. A AF and B AF remain independent volume controls, and
  M.RF/S.SQL cannot be replaced by a BAL overlay.
- SSTV TX now includes a touch composition canvas. It keeps the selected
  background intact while adding editable multiline text, selected fonts,
  color, pen strokes, line/arrow/rectangle/ellipse markup, source rotation,
  movement, object deletion, undo/redo, and reset. The preview confirmation
  freezes the native-size rendered frame that is passed to the existing SSTV
  encoder. The mode selector now precedes the editing controls and immediately
  defines the editor's exact native pixel canvas; source zoom/position and all
  overlays render against those transmitted pixels. Preview/send perform no
  late image resize, and the live canvas identifies its mode and dimensions.
  CQ, REPORT, and 73 starter layouts, user templates, the selected TX
  mode, and a debounced recovery draft persist locally. User templates now
  open in a dedicated in-window editor that loads the
  selected layout over the current mode frame, provides the complete text,
  font, size, color, drawing, shape, undo, and reset controls, and saves back
  to that same named template before loading it onto the TX canvas for review.
  The built-in CQ, REPORT, and 73 starters open in the same editor and SAVE
  TEMPLATE directly persists an editable default override, including an
  optional background image. RESET TEMPLATES restores the factory starters;
  the main save icon remains the separate save-as-new-template action.
  Complete-image templates now normalize legacy/default zero font stretch to
  normal width on restore, preserving their visible text and markup; the image
  gallery now presents the live TX composition as its first selectable tile,
  enables actions from actual selection state, and keeps saved thumbnails
  separate from the current composition. Its single responsive list replaces
  the former empty-state panel that could overlap the title after rotation.
  The TO CALL placeholder is explicitly left-aligned in the compact TX form.
  The operator callsign
  is an editable, validated, persistent SSTV setting used dynamically by the
  built-in templates and TX status; no product callsign is hard-coded. On
  Android, SSTV now owns an opaque full-window canvas and hides the console
  widget tree until BACK TO RADIO is selected. The TX editor keeps the image
  large while its compact control pane scrolls vertically. A newly imported
  Gallery or Camera source now clears the prior source's markup and undo/redo
  history. The markup color control now opens a compact touch palette with four
  shades each of the seven rainbow color families, plus black, white, charcoal,
  several greys, and silver. Choosing
  a color immediately recolors selected text and remains undoable. The font
  selector now offers 16 curated RF-readable family, weight, width, and italic
  variants; font and size changes apply immediately to selected text and are
  undoable. Zoom, text-size, and power sliders use the main console's larger
  touch geometry, a shorter horizontal activation distance, tap-to-position,
  and vertical-swipe direction locking. The permanent SSTV header now shows
  live RX and TX frequency/mode on both pages; RX follows Main/VFO A and TX
  follows the K4 split-selected transmit VFO. Independent persistent FSK ID
  and CW ID options append the operator callsign after the image while
  retaining the current K4 mode and the same immediate STOP/automatic-RX
  lifecycle. Fresh installs enable both; an existing SSTV installation
  preserves its CW choice and initially leaves the new FSK ID off. FSK uses
  MMSSTV/QSSTV-compatible 1900/2100 Hz six-bit checksum framing; CW remains
  shaped 700 Hz Morse at the remembered 5-40 WPM setting and its speed control
  appears only while CW is selected. The CW-speed field suppresses Android's
  clipped native stepper artifacts and uses separate visible one-WPM
  minus/plus touch buttons. When both IDs are enabled, progress visibly
  advances through image, FSK ID, then CW ID. The TX control pane now uses the main
  phone drawer's delayed-press/scroll-cancellation behavior plus direction-
  locked sliders. Both rotate glyphs use the conventional arrowhead position,
  and the compact power row gives more width to the slider with smaller step
  buttons.
- Completed SSTV RX images are atomically retained as app-private lossless PNG
  files with UTC/mode/frequency/slant and callsign metadata. After image
  completion, a bounded tail detector recognizes checksum-valid FSK ID first
  and callsign-shaped CW second without disarming AUTO RX; a new VIS header
  preempts the tail. RX CALL remains editable, records source/confidence and
  raw FSK/CW candidates, and manual correction is preserved. REPLY preloads
  the received mode and TO CALL in the TX editor without keying the K4; REPORT
  and 73 templates resolve that station while retaining the normal preview and
  deliberate TX confirmation. The receive screen also provides
  selectable retention, starring, individual deletion, protected history
  clearing, and explicit read-only Android sharing. Starred and exported images
  are not auto-deleted. The storage, draft, template, codec, and composition
  tests pass, and the complete ARM64 APK builds. The receive, callsign, reply,
  template, and transmit workflows were exercised on the physical Android/K4
  setup during extended live monitoring.
- The SSTV receive screen now distinguishes a live decoded K4 PCM stream from a
  stopped/missing stream and shows its input level, while keeping the image and
  history controls within the compact phone canvas. On the Galaxy S26 Ultra,
  live Main-RX PCM and the Android `FULL_SENSOR` orientation request were
  observed on-device. VIS acquisition now debounces noisy tone transitions,
  follows up to 300 Hz of leader-derived AFC through header/image decoding,
  recognizes short breaks hidden by discriminator transitions, and times out a
  partial/false leader without contaminating the next header. Offset/dropout
  and false-header recovery regressions pass. The receive DSP now also applies
  a streaming 1.0-2.5 kHz linear-phase prefilter, impulse-resistant adaptive
  line-sync detection, sample-rate-aware robust pixel estimation, and slow
  per-line AFC interpolation from accepted 1200 Hz sync pulses. Deterministic
  tests cover stronger out-of-band interferers plus clicks and a 0 to +120 Hz
  image-body drift; all 22 clean mode round trips remain covered. Portrait and
  landscape reflow and live over-the-air reception were exercised on the
  Samsung Galaxy S26 Ultra; not every supported mode received separate live
  over-the-air validation.

The FM PL/FM-T, DTMF CMD-memory, DATA/AFSK IIP, mode-aware Main/Sub RX,
Sub-RX targeting, APF availability, and informational-dialog CLOSE items from
the 2026-08-12 device findings were completed in v0.8.0 and are no longer
pending work.

- Validate landscape usability, system insets, font scaling, touch scrolling,
  and audio behavior on smaller Android phones, a Pixel/non-Samsung phone, and
  a foldable or tablet. Tablet devices temporarily use the compact phone layout;
  do not call them supported until tested.
- Android sideloading can still show a Play Protect/unknown-source notice even
  for the release-signed APK. Google Play distribution requires a signed AAB
  and Play App Signing.
- Hearing-aid RX routing is unverified on physical hardware. It applies only
  where Android exposes a `TYPE_HEARING_AID` output, and TX remains on the
  phone microphone unless Android supplies a separately supported input route.
- DR+ indication remains deferred until its source/state semantics are proven.
- Peak Hold is intentionally local: K4 stream data does not provide a rendered
  radio peak trace. WTR CLRS is local too; do not conflate it with `#WFC`.

Version 0.7.5.1 restores the original A/B VFO mode-control geometry. The
v0.7.5 extra touch padding pushed the SUB/DIV badges toward the VFO B frequency
and meter display on some layouts; this point release removes only that padding.

## References

- [Elecraft K4 manuals](https://elecraft.com/pages/k4-high-performance-direct-sampling-sdr-manuals)
- [Elecraft K4 Programmer's Reference](https://ftp.elecraft.com/K4/Manuals%20Downloads/K4ProgrammersReferencerev.D12.html)
- [Upstream QK4](https://github.com/mikeg-dal/QK4)
