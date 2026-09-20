# SSTV scope

Status: implementation complete for the current Android/device test build,
including the post-image ID and Reply workflow. Both RX and TX are wired
through the QK4 audio path. QSO logging and ADIF export remain intentionally
deferred to their dedicated future branch.
The source includes the shared 22-mode registry and streaming encoders/decoders
for Robot 36; Martin M1/M2/M3/M4; Scottie S1/S2/DX/S3/S4;
PD-50/90/120/160/180/240/290; Wraase SC2-120/180; and Pasokon P3/P5/P7. It also
includes VIS auto-detection,
regression-based automatic slant correction, progressive receive rendering,
generation-gated program-audio TX with immediate unkey, orientation reflow,
and Android gallery/full-resolution camera acquisition. The TX screen now has
a mode-native composition canvas with editable multiline text blocks,
constrained font choices, color palette, finger/stylus freehand ink, simple
line/arrow/rectangle/ellipse objects, movement, rotation of the source image,
undo/redo, reset, and a frozen-frame confirmation boundary. CQ, REPORT, and 73
starter layouts plus user templates and a debounced recovery draft persist in
app-private storage. Completed RX images are atomically saved as lossless PNG
with privacy-safe metadata, selectable 10/25/50/100/unlimited retention,
starred-image protection, deletion, and explicit scoped Android sharing.
The RX tail recognizes checksum-validated MMSSTV/QSSTV FSK ID and callsign-
shaped CW, persists the selected and raw candidates, and exposes editable
RX CALL plus Reply mode/TO CALL prefill. TX independently remembers FSK ID and
CW ID, sends both in that order when selected, and reports each progress phase.
Abandoned picker/camera cache files are cleaned without touching retained or
exported images. All five native test targets pass and the ARM64 debug APK
builds. Independent cross-codec fixtures and controlled physical K4
calibration/interoperability testing remain release acceptance requirements;
they are not claims made by the source-level or automated validation.

The native RX path includes weak-signal DSP adapted from the Open-SSTV design
to QK4's continuous 12 kHz K4 stream. A fixed-delay 650-2750 Hz FIR preserves
the complete SSTV tone range at the supported +/-350 Hz AFC limits. A flat
49-tap complex-baseband FIR and conjugate-product discriminator replace the
former eight-sample boxcar that disproportionately weakened the low VIS and
sync tones. VIS bit decisions use central-window robust percentiles rather
than arithmetic means. The normal complete-preamble detector remains the
preferred path; an independent guarded fallback can identify a known,
parity-valid VIS candidate from second-leader evidence even when the first
leader or break was clipped. That fallback uses non-overlapping VIS tone
limits and does not announce a mode or render an image until three consecutive,
tightly timed one-line-period sync pulses confirm it. An unconfirmed candidate
returns to AUTO RX. Complete-preamble acquisition keeps its established
two-pulse timing confirmation. The image path retains a
mode-scaled median line-sync track with a bounded adaptive threshold,
winsorized pixel-frequency sampling sized for the low sample count, and slow
sync-derived AFC stored per line and interpolated during final rendering.
Open-SSTV's batch zero-phase SciPy filter is not embedded or copied; QK4 retains
its native streaming C++ codec. Synthetic coverage now includes all 22 VIS
codes at 5 dB input SNR, 20/10/5/0/-5 dB characterization, clipped and weak
leaders, faded VIS bits, +/-330 Hz offset, header drift, in-band and out-of-band
interference, impulse clicks, false-start rejection, and 120 Hz of gradual
image-body drift. Ten private false-trigger Main-RX captures are also replayed
as negative fixtures outside Git. These are automated impairment results, not
a claim of on-air weak-signal performance until K4 tests confirm it.

## Product boundary

SSTV is a local QK4 Mobile feature. It consumes the decoded Main-RX audio and
renders a local image; it does not add CAT commands or assume the K4 supplies
an SSTV image. Transmission generates local audio, then uses the established
K4 TX packet path. It must not change connection setup, stream-latency
negotiation, Android route selection, microphone routing, or K4 operator
settings as a connection side effect.

The implemented mode registry contains **22 RX/TX modes**: Robot 36; Martin
M1/M2/M3/M4; Scottie S1/S2/DX/S3/S4; PD-50/90/120/160/180/240/290; Wraase
SC2-120/180; and Pasokon P3/P5/P7. Scottie S1 remains the first-run TX default.
Keep one shared mode registry containing VIS code, color order/model,
dimensions, line layout, sync and porch timing, scan timing, expected duration,
and display name. Automated native round trips cover every entry, but a mode is
not considered independently interoperable until external fixtures and an
over-the-air test cover it.

## Codec and dependency decision

- Implement a small QK4-native, streaming SSTV codec core in C++ rather than
  embedding a desktop application or routing audio through files. Keep it
  independent of Qt widgets, Android, CAT, sockets, and audio devices so the
  same deterministic core can be unit-tested on the host and Android.
- `libsstv` may be used as an MIT-licensed independent TX fixture/reference,
  but its current release has no decoder and is not the application codec.
  QSSTV is a GPL-compatible interoperability reference, not a linked dependency;
  its desktop audio, Hamlib, PulseAudio, Qwt, FFTW, and image stack must not be
  imported into QK4 Mobile merely to obtain SSTV DSP.
- Open-SSTV by Kevin, W0AEZ (GPL-3.0-or-later) is an interoperability and mode-
  timing reference. QK4 retains its native streaming C++ codec and does not
  bundle Open-SSTV, PySSTV, Python, SciPy, or its desktop UI. The canonical
  paired-chroma Robot 36 correction is applied in the QK4 encoder and decoder.
- The decoder consumes continuous mono PCM plus sample timestamps and emits
  VIS/mode state, sync confidence, progressive scan lines, slant state, and a
  completed frame. The encoder consumes only a frozen mode-native frame and
  incrementally emits bounded PCM blocks; it never constructs a whole WAV file.
- Commit synthetic reference images and short, deterministic PCM fixtures that
  are safe to redistribute. Keep third-party off-air recordings out of Git
  unless their provenance and redistribution permission are documented.

## Receive workflow

- Add an SSTV entry that opens a full touch screen while keeping a clear route
  back to the live console and its PTT control. Unlike the compact main
  console, the SSTV screen supports both portrait and landscape device
  orientation and reflows without restarting the decoder or losing an active
  composition.
- Every time the SSTV screen opens, start in **AUTO RX** with the Main-RX
  decoder armed. Do not restore a previously selected TX tab or leave the
  screen idle waiting for the operator to enable decoding.
- Keep RX and TX frequency/mode visible in the SSTV header on both pages and
  throughout preparation, transmission, and automatic return to RX. RX follows
  Main/VFO A; TX follows VFO A normally and VFO B frequency, mode, and data
  sub-mode when K4 split is active. Update from existing radio state only; the
  display must not send CAT commands.
- Decode Main RX only in v1. The screen shows a live, progressively rendered
  image, selected or detected mode, sync/decoding status, and an explicit
  Reset action.
- Provide Auto and manual-mode selection. Auto must report its selected mode
  visibly and must not silently overwrite a completed image.
- Enable **AUTO SLANT** whenever AUTO RX is armed. Slant correction applies to
  the progressive preview, completed image, saved file, and shared file; the
  operator must not need to calibrate the phone or enable it for each receive
  session.
- Keep a small local gallery of completed images with timestamp, frequency,
  mode, and save/share actions. Image retention and user-selected output
  location are application settings; no received media belongs in Git.
- Deliver RX samples to the decoder before volume, balance, output resampling,
  and Android playback. Decoding must continue when the receiver is muted or
  an Android playback endpoint changes.

### RX callsign recognition and Reply workflow

- Keep a permanent **RX CALLSIGN** field on the Receive screen. Show the
  recognized call beside its source (`FSK ID`, `CW ID`, or `MANUAL`) and keep
  the value editable because weak-signal identification is never infallible.
- After an image completes, continue examining the Main-RX PCM tail without
  leaving AUTO RX. Decode an MMSSTV/QSSTV-compatible FSK ID and a post-image CW
  ID, while continuing to watch for the next valid SSTV header. A new valid VIS
  acquisition preempts tail-ID processing immediately.
- Callsign-source priority is **valid FSK ID first, CW ID second, manual
  entry third**. A later CW candidate must never overwrite a valid FSK ID.
  Preserve the raw candidates and confidence for diagnostics, but expose one
  confirmed callsign to the operator.
- Implement FSK ID as its own deterministic codec using the interoperable
  MMSSTV signaling contract (45.45 baud, 1900/2100 Hz audio tones, legacy
  six-bit character/checksum framing). Validate it with synthetic fixtures and
  independent MMSSTV/QSSTV interoperability captures before calling it proven.
- Treat CW ID as audio Morse in the radio's existing demodulation mode, normally
  USB. Use a bounded post-image recognition window and callsign-aware syntax
  validation; do not switch the K4 to CW or invoke its keyer/decoder merely to
  recognize the ID.
- Store the confirmed RX callsign, source, confidence, and raw ID candidates in
  the image's app-private metadata. Older records without these fields remain
  valid and display an empty editable callsign.
- Put a prominent **REPLY** button on the Receive screen for the selected RX
  image. It opens the TX editor without keying and preloads the received SSTV
  mode, `TO CALL`, persistent `MY CALL`, an editable RSV/report, and an optional
  inset of the received image. The operator may choose a Reply template, edit
  the complete transmitted frame, and must still use the normal deliberate TX
  confirmation. Never auto-reply or auto-key from a recognized callsign.

### Automatic slant correction

Slant is a receive time-base error: a small difference between the transmitted
line period and the decoder's assumed sample clock accumulates horizontally
from line to line. Treat it separately from audio-frequency/tuning correction,
which primarily affects decoded color or brightness.

- Record the sample position and confidence of every valid mode-specific
  horizontal-sync pulse. Estimate actual line period from sync position versus
  line number using robust linear regression with iterative outlier rejection;
  do not let one false or faded sync pulse move the image.
- Compare the fitted period with the selected mode's nominal line period and
  derive one per-image timing scale. Apply that scale when locating each line
  and sampling its pixels, rather than rotating or shearing an already rendered
  bitmap.
- Retain a bounded raw demodulated receive buffer for the active image. As the
  fit improves, periodically re-render the progressive image from that buffer
  so early lines and newly decoded lines share the same correction.
- Show compact status such as `SLANT: AUTO`, then `CALIBRATING`, `LOCKED`, or
  `LOW SYNC`. Do not interrupt image reception with a dialog.
- Lock the estimate after enough inliers produce a stable, low-jitter fit so a
  late fade cannot tilt an otherwise good image. Unlock only after a sustained,
  statistically significant timing change, not a single missing sync.
- Reset calibration for every new VIS/image acquisition and when the operator
  resets RX or forces a different mode. Do not persist one transmitter's clock
  correction as a global phone or K4 setting.
- When sync evidence is insufficient, continue with nominal mode timing and
  label the correction as uncertain. Preserve the raw active-image buffer so a
  later recovery of sync can correct the complete image.
- Provide an image-adjustment fallback after reception for rare recordings with
  unusable sync: **AUTO**, fine left/right slant adjustment, and **RESET**.
  Manual adjustment changes only the local received image and never CAT, K4
  state, RX audio, or future images.

## Transmit workflow

- Open the TX tab with **GALLERY** as the primary image-source action and
  **CAMERA** beside it. Gallery opens Android's system photo picker by default;
  Camera opens the installed camera application and returns a full-resolution
  image to QK4 Mobile.
- Preserve the source image and its aspect ratio. After selection or capture,
  make the selected SSTV mode define the editor's native pixel canvas
  immediately. Show the exact mode-sized image while the operator positions,
  zooms, rotates, and marks it up. Changing SSTV mode must rebuild that canvas
  non-destructively from the original rather than repeatedly resizing an
  already reduced image.
- Treat every successfully selected Gallery image or newly captured Camera
  photo as a new composition: clear all text, strokes, shapes, selection, and
  undo/redo history from the preceding source. Cancellation or import failure
  leaves the existing source and its markup unchanged.
- Let the operator choose only a supported mode. Display the mode, image
  dimensions, estimated duration, current transmit frequency, and synchronized
  K4 RF power before the final action.
- Provide a large touch mode selector on TX preparation and confirmation.
  Default a first-time TX session to **Scottie S1**, a widely used general/HF
  calling mode, while keeping Martin M1 and every other released mode one tap
  away. After the operator explicitly chooses a different mode, remember that
  choice locally for later TX sessions; never change it automatically because
  of frequency, region, received VIS, image orientation, or a connection event.
  Each choice shows its exact registered dimensions and estimated duration
  before it is applied, and changing it rebuilds the preview non-destructively.
- Keep an always-visible, compact **TX POWER** row on the TX preparation and
  confirmation screens: synchronized-state label, minus button, numeric watt
  readout, short finger-safe slider, and plus button. It must not materially
  reduce the image canvas. Use the existing QK4 RF-power state and
  command path so changes made on the K4 or main console update this screen and
  changes made here update the radio. Changing power is an explicit operator
  action and must never occur merely because the SSTV screen opened, a mode or
  image was selected, or TX completed.
- In the vertically scrolling TX control pane, use the same delayed child-press
  cancellation as the main phone control drawer. A vertical gesture must not
  open a combo or click a button. Sliders direction-lock after deliberate
  movement: vertical motion scrolls the pane and horizontal motion adjusts the
  value. Keep power step buttons compact so the slider retains useful width.
- Disable TX POWER as soon as SSTV keys the radio and show the frozen keyed
  value on the active-TX status panel. Do not let power adjustment compete with
  the transmit input shield or immediate **STOP SSTV** action. After return to
  AUTO RX, display subsequent synchronized K4 power changes normally.
- Start only through an explicit `TRANSMIT SSTV` confirmation. The confirmation
  must state that it keys the radio. A visible red `STOP SSTV` control is
  always present while transmitting; cancel, disconnect, or application
  shutdown must release the K4 immediately.
- `STOP SSTV` is a one-touch safety action, not a confirmation or ordinary
  queued cancellation. Make it the largest red control in the TX screen,
  fixed in the persistent bottom-right TX/RX position and rendered above every
  SSTV overlay. Activate the stop on button press so it does not wait for a
  release, long press, dialog, scrolling gesture, encoder callback, or UI
  repaint. It remains enabled for the complete keyed interval.
- SSTV TX uses one controlled program-audio source. It feeds PCM to the
  existing AudioEngine resampling, Opus encoding, sequencing, and TcpClient
  send path; it must never contend with microphone capture or create a second
  raw-packet sender.
- Keep the existing **CW ID** checkbox and add an independent **FSK ID**
  checkbox beside it. They are not radio-mode choices and are not mutually
  exclusive: either, both, or neither may be selected. Fresh installations
  default both on. During a one-time upgrade, preserve the existing CW choice
  and default the new FSK choice off so the upgrade does not silently add a new
  on-air identifier; once the operator changes either checkbox, remember it.
- Save both checkbox states immediately as global operator preferences, outside
  the current image draft and template. Restore them for every ordinary TX and
  **REPLY** session. Image replacement, mode changes, successful TX, STOP,
  failed TX, return to the radio, orientation changes, and application restart
  must not reset them. Remember CW WPM separately even while CW ID is unchecked.
- Keep **MY CALL** visibly wide enough for at least nine characters in
  landscape. Show the adjustable 5-40 WPM control, default 20 WPM, only when
  CW ID is checked. FSK ID uses its fixed interoperable timing and therefore
  presents no speed control. Reflow the row for portrait without shrinking the
  callsign editor.
- When at least one ID is checked and My Call is valid, retain the same TX lease
  after the image and append program audio in this fixed order: separator, FSK
  ID when checked, short inter-ID silence when both are checked, CW ID when
  checked, then the anti-clipping tail. FSK ID uses the same MMSSTV/QSSTV-
  compatible codec required by RX; CW ID sends shaped 700 Hz Morse. Both remain
  in the current K4 demodulation mode, normally USB: never issue a mode, keyer,
  VOX, RTTY, or paddle command.
- Progress identifies `SENDING FSK ID` and then `SENDING CW ID` when both are
  checked, includes both choices and added duration in preflight, and keeps STOP
  SSTV immediate through the image, every separator, both IDs, and final tail.
- During SSTV TX, use the same deliberate CAT `TX;` / `RX;` ownership and
  in-window input shield as phone PTT. Disable the regular PTT control while
  the SSTV sender owns TX, except that the visible stop control releases it.
- Successful completion, STOP SSTV, encoder failure, or interrupted TX must
  release program-audio ownership, send `RX;` when connected, clear the SSTV
  TX shield, and return the screen directly to **AUTO RX**. The operator must
  never have to re-enable the decoder after transmitting.
- Do not emulate PTT with VOX. Do not automatically set power, mode, filter,
  VOX, EQ, or any other radio setting. If the selected radio state is unsafe or
  unsuitable, warn the operator and require them to correct it themselves.

### TX preflight and confirmation

`PREVIEW & SEND` opens a final confirmation sheet; it does not key immediately.
The sheet shows the frozen image, TX frequency and VFO, K4 demodulation mode,
SSTV mode/duration, synchronized RF power, and program-audio readiness.

- Block TX for no K4 connection, unknown/stale radio state, another local TX
  owner, an external/radio-originated keyed state, unavailable TX stream, or an
  invalid/stale frozen frame. State the exact reason inline.
- Accept USB, LSB, and supported DATA modes only after bench verification of
  their audio passband. Warn for an unverified mode/filter combination and let
  the operator return to the console; never change mode or filter automatically.
- Frequency is informational, not a hard-coded band-plan gate. Warn when it is
  outside a configurable SSTV calling-frequency hint, but do not claim legal
  authority or prevent lawful operator-selected operation.
- Require one deliberate **TRANSMIT SSTV** action in the confirmation sheet.
  Recheck every blocking condition after that touch and before sending `TX;`.
  A changed power/frequency/mode invalidates the confirmation but not the image.

### TX progress and completion

- Show a scan line over the frozen image; completed lines remain normal and the
  unsent portion is dimmed. Beneath it show a progress bar, percentage, and
  elapsed/estimated total time such as `00:42 / 01:50`.
- Advance progress only when the corresponding paced program-audio frame has
  been accepted by the gated K4 send path. Encoder production or UI time alone
  must not advance it. Pause the visual progress if delivery stalls.
- Keep mode, frequency, frozen RF power, `MIC OFF`, program-audio level, K4 TX
  state, and the fixed red **STOP SSTV** visible throughout transmission.
- On normal completion, close the send gate, issue `RX;`, await or time-bound
  the K4 RX acknowledgement, show **TRANSMISSION COMPLETE** briefly, and return
  directly to AUTO RX. A missing acknowledgement becomes a prominent
  **K4 TX STATE UNKNOWN** error while continuing safe RX-command retries; it
  must never be presented as successful completion.

### Immediate TX stop contract

All manual and automatic SSTV TX termination uses one idempotent
`stopSstvTransmit()` path. Its safety-critical order is:

1. Atomically close the SSTV program-audio send gate and invalidate the active
   generation token so no later worker callback can emit another TX frame.
2. Stop the real-time pacer and clear generated PCM plus any unsent SSTV frame
   queue. Keep program-audio buffering deliberately shallow so a long encoded
   transmission is never staged into the network socket.
3. Queue a dedicated stop operation on `TcpClient`'s I/O thread that rechecks
   the closed gate, drops already-queued SSTV audio deliveries, writes the CAT
   `RX;` packet, and flushes the socket immediately.
4. Release SSTV TX ownership, clear the TX shield/indicator, and return to
   AUTO RX. UI cleanup must not delay steps 1-3.

Use a distinct SSTV-audio delivery method rather than connecting it to the
unconditional generic `sendRaw()` slot. That lets the network boundary reject
stale SSTV packets after STOP even if they were queued by another thread before
the button press. Packets already accepted by the operating-system socket
cannot be recalled, so the pacer must keep at most the current real-time frame
in flight; `RX;` remains the authoritative immediate unkey command.

STOP must also be triggered by disconnect, activity pause/destruction,
application shutdown, encoder failure, TX timeout, and confirmed BACK TO RADIO
while keyed. Repeated calls are harmless and must never toggle back into TX.

## Screen lifecycle and return to console

- Implement SSTV as an in-window operating screen owned by `MainWindow`, not a
  separate Android activity or a desktop-style top-level Qt window.
- Provide a large, visible **BACK TO RADIO** control in the SSTV header. The
  Android system Back gesture/button performs the same action. This returns to
  the existing QK4 console without disconnecting the radio or rebuilding the
  TCP, audio, panadapter, or Android route state.
- In AUTO RX, leaving the SSTV screen stops the local SSTV decoder, discards
  only an incomplete receive image after confirmation when appropriate, and
  restores the console immediately. It sends no CAT command and changes no K4
  operating setting.
- A prepared but untransmitted TX image may remain in the local SSTV session so
  it is available if the operator reopens the screen; reopening still lands on
  AUTO RX rather than the TX editor.
- While SSTV owns TX, BACK TO RADIO and Android Back first show a safety prompt:
  **STOP SSTV AND RETURN TO RADIO?** Confirmation follows the normal STOP path,
  sends `RX;`, returns to AUTO RX, and then closes the screen. Cancellation
  keeps the SSTV TX screen and visible STOP control active.
- App pause, activity destruction, disconnect, and application shutdown use
  the same idempotent TX-release routine before screen state is destroyed.

The controller state model is:

`Closed -> AutoRx -> TxPrepare -> TxConfirm -> Transmitting -> AutoRx`

Every TX terminal path converges on `AutoRx`; only an explicit BACK TO RADIO
action transitions from `AutoRx` to `Closed`. Opening always transitions from
`Closed` to `AutoRx`.

### Orientation contract

- The main QK4 console remains sensor-landscape. Entering SSTV changes the
  activity request to full user-sensor orientation; leaving SSTV restores
  sensor-landscape. Do this dynamically through `Qk4Activity`, because the
  manifest's activity-wide landscape lock cannot express an in-window exception.
- Treat rotation as layout reflow, not an SSTV state transition. Preserve the
  decoder and raw RX buffer, completed image, immutable source, composition
  objects/undo history, frozen TX revision, selected SSTV mode, and power state.
- Rotation is allowed during active TX. Reflow around the same controller and
  sender without interrupting PCM timing; the fixed STOP control must be laid
  out first and remain visible. If Android destroys/recreates the activity
  despite declared configuration handling, invoke the stop contract before
  destruction rather than attempting to resume a keyed transmission.
- Device orientation never rotates encoded pixels implicitly. Apply source
  EXIF orientation once; explicit editor rotate-left/right controls determine
  image orientation. Portrait and landscape UI layouts show the same frozen
  mode-native transmit frame.

## Android image acquisition

- Add a small Android bridge owned by `Qk4Activity`; keep Android intents and
  URI permission handling out of the SSTV codec and UI classes.
- On Android 13 and later, Gallery uses the system Photo Picker. On older
  supported Android versions, use the Storage Access Framework with an
  `image/*` document request. Do not request broad media-library access merely
  to select one image, and do not depend on a filesystem path being available.
- Camera uses the system camera intent with a one-time writable `content://`
  destination backed by the existing application `FileProvider`. Request the
  app's declared camera runtime permission before launch and require a
  full-resolution output; do not use the small bitmap thumbnail commonly
  returned in intent extras.
- Return the selected/captured content URI asynchronously to C++. Preserve a
  pending camera URI across activity recreation, grant only the URI permissions
  required for the operation, and revoke/clean temporary files after import or
  cancellation.
- Cancellation, camera failure, unreadable content, unsupported formats, and
  activity recreation return to the SSTV TX screen without losing the previous
  prepared image.

## Local storage, privacy, and recovery

- Store application-managed RX images, TX drafts, thumbnails, and templates in
  app-private storage. No network/cloud upload, contact access, or location
  lookup occurs. Sharing/export happens only through an explicit Android share
  or document-creation action using scoped `content://` access.
- Default the RX gallery to the newest **50** completed images with an optional
  setting of 10, 25, 50, 100, or unlimited. At a finite limit, delete the
  oldest app-private unstarred entry only after a newer image has been committed
  successfully. Starred images and user-exported files are never auto-deleted.
- Save completed RX images losslessly as PNG with a sidecar/local database row
  containing UTC timestamp, frequency, detected/forced mode, slant status, and
  optional callsign/note. Do not embed credentials, IP addresses, or radio
  profile names. Export filenames use `SSTV_yyyyMMdd_HHmmss_MODE.png`.
- Autosave one TX composition draft transactionally after meaningful edits,
  debounced so drawing does not write per point. On low storage or a failed
  write, preserve the in-memory session, show a nonmodal warning, and disable
  capture/export actions that require more space; RX and STOP remain functional.
- Delete abandoned camera temporary files after import/cancel and expire
  unreferenced temporary files on the next clean startup. Provide **CLEAR RX
  HISTORY**, **CLEAR TX DRAFT**, and **RESET TEMPLATES** as separate confirmed
  actions; never combine them with general cache cleanup.

## Image preparation

There is no universal SSTV image size. The selected mode's registry entry is
the sole source of truth for output width, height, aspect ratio, and transmit
duration. For the proposed baseline, Martin and Scottie frames are 320 x 256,
Robot 36 is 320 x 240, and PD120/PD180 are 640 x 496. These values require
codec-fixture verification before release.

The preparation pipeline is:

1. Decode the source off the UI thread with a bounded-memory image reader and
   apply EXIF orientation before displaying the crop editor.
2. Select the SSTV mode first (defaulting to Scottie S1) and immediately create
   an edit canvas at that mode's exact registered pixel dimensions.
3. Retain an immutable source. Re-render it directly into that exact native
   canvas whenever the operator pans, zooms, rotates, changes mode, or changes
   the fit policy; never use a previously reduced frame as the new source.
4. Default to **FILL / CROP**, using the mode's aspect ratio and never
   stretching the image. Also offer **FIT / BARS** for operators who prefer the
   complete source image; use a neutral dark fill for unused pixels.
5. Render text, drawing, and shape overlays directly in the native canvas pixel
   coordinates so their size, placement, clipping, and detail are visible as
   they will be transmitted.
6. On **PREVIEW**, freeze the current mode-native canvas pixel-for-pixel for
   both confirmation and encoding. Preview and send perform no image scaling.

If an input is smaller than the selected mode, allow upscaling because the
encoder still requires an exact frame size, but show a visible low-resolution
warning. Never modify the user's original gallery image or camera photo.

## Planned QSO log and ADIF export

Open-SSTV's image-linked QSO logbook and ADIF export are the functional
reference for a later QK4 Mobile phase. The log remains local, touch-first, and
operator-confirmed. It does not send CAT commands, change radio settings, or
upload contacts.

### Branch boundary

- This design is saved for later implementation in its own branch; do not mix
  QSO database, editor, or ADIF source changes into the current SSTV codec/RX/TX
  work. The recommended future branch name is `codex/sstv-qso-log-adif`.
- Start that branch only after an explicit operator request. Treat this section,
  its implementation-sequence item, and its validation checklist as the branch
  handoff specification. No logging implementation is part of the current
  build.

### Entry points and operator workflow

- Show **LOG QSO** on the completed-image RX screen, the selected RX-history
  item, and the post-TX return-to-RX summary. Each entry point opens the same
  editable draft; it never creates a confirmed contact merely because an image
  was decoded or transmitted.
- Prefill `CALL` from the confirmed RX callsign using the existing priority of
  FSK ID, then CW ID, then manual entry. Prefill `STATION CALLSIGN` from
  persistent My Call, UTC start/end time from the current SSTV exchange,
  frequency and band from captured K4 state, `MODE=SSTV`, the exact SSTV mode,
  reports, grids, and attached RX/TX image identifiers when known. Every field
  remains editable before **SAVE QSO**.
- Keep one recoverable current-QSO draft until the operator saves or explicitly
  discards it. Additional received and transmitted images may be attached only
  through an explicit **ADD TO CURRENT QSO** action; do not merge contacts by a
  callsign/time heuristic.
- After save, return to AUTO RX and show a brief confirmation above the active
  SSTV screen. Saving, editing, deleting, or exporting a log entry never leaves
  AUTO RX or changes K4 frequency, mode, PTT, audio routes, or TX power.

### Touch UI

- Add **QSO LOG** to the SSTV screen as a full-window page with **BACK TO RX**,
  search, date/callsign filtering, and a newest-first touch-scrollable list.
  Each compact row shows UTC date/time, callsign, frequency/band, SSTV mode,
  reports, and attachment indicators.
- Tapping a row opens a readable detail/editor page. Required first-screen
  fields are callsign, UTC date/time, frequency, band, SSTV mode, RSV sent,
  RSV received, and notes. Put optional name, QTH, grids, and image attachments
  in a collapsible secondary section so the phone UI does not become a desktop
  form.
- Provide **NEW**, **EDIT**, **SAVE QSO**, **DELETE**, **EXPORT ADIF**, and
  **BACK TO RX** with normal touch targets and scroll/tap disambiguation.
  Deletion requires confirmation and removes only the log record; attached
  images follow their existing independent retention rules.

### Local data model and integrity

- Store contacts transactionally in an app-private SQLite database with schema
  versioning and stable UUIDs. Keep images in the existing SSTV image store and
  link them by stable image record ID and role (`RX` or `TX`), never by a public
  filesystem path. Deleting an image clears its attachment but preserves the
  contact; deleting a contact does not delete an image.
- Retain created/modified UTC timestamps, QSO start/end UTC, callsign, station
  callsign, frequency in integer Hz, derived band, radio mode, ADIF mode, exact
  SSTV mode, sent/received reports, grids, name, QTH, notes, selected TX IDs, RX ID
  source/confidence, and attachment links. Database migrations must be atomic
  and must leave an older valid log readable after an interrupted upgrade.
- Normalize callsigns and grids for searching while preserving operator-entered
  display text. Validate required fields before save, but allow incomplete
  recoverable drafts. Never store radio credentials or profile identities.

### ADIF export action

- **EXPORT ADIF** offers `Selected QSO`, `Filtered Results`, or `Entire Log`,
  then invokes Android's system document-creation flow with a suggested name
  such as `QK4_SSTV_Log_20260821.adi`. Cancellation leaves the database and UI
  unchanged; success reports the record count and destination display name.
- Write UTF-8 ADIF with an application header and one `<EOR>` record per QSO.
  Map standard fields including `QSO_DATE`, `TIME_ON`, optional `TIME_OFF`,
  `CALL`, `STATION_CALLSIGN`, TX `FREQ`, optional split `FREQ_RX`, `BAND`,
  `MODE` (`SSTV`), `RST_SENT`, `RST_RCVD`, `GRIDSQUARE`, `MY_GRIDSQUARE`,
  `NAME`, `QTH`, and `COMMENT`. Export the exact SSTV variant, selected post-
  image identifiers, and captured K4 demodulation mode as
  `APP_QK4_SSTV_MODE`, `APP_QK4_TX_IDS`, and `APP_QK4_RADIO_MODE` rather than
  inventing values for standard ADIF fields.
- Do not export app-private image paths, callsign confidence diagnostics, or
  other device-local identifiers by default. An optional plain attachment
  filename may be included later only after interoperability and privacy review.
- Generate the file from a consistent read transaction, validate all field
  lengths and record terminators, and fail without producing a misleading
  partial-success message. ADIF import, duplicate reconciliation, LoTW/eQSL/
  QRZ uploads, and third-party logger networking remain later independent work.

## TX audio level and timing

The shared FT8/FT4/SSTV calibration and protection implementation is documented
in [DIGITAL_TX_LEVEL.md](DIGITAL_TX_LEVEL.md). SSTV now requires a remembered
calibration matching the radio setup, attenuates program audio, checks codec
headroom, automatically reduces drive for high ALC, and stops on overload or
missing feedback. Its protection status persists above both RX and TX pages.
TEST-mode calibration and provisional raw ALC thresholds still require K4
hardware and RF acceptance; the requirements below remain the acceptance scope.

- Generate mono signed 16-bit PCM at the K4 TX stream's native 12 kHz rate.
  Use a continuous phase accumulator across every tone and packet boundary;
  quantize segment duration with carried fractional-sample error so total line
  and image timing do not drift.
- Add one SSTV program-level setting expressed as a bounded digital percentage,
  not microphone gain. First release default is established by loopback and K4
  TEST-mode measurement; until calibrated, mark it **CALIBRATION REQUIRED** and
  block over-the-air release builds from claiming a production default.
- Hard-limit generated samples below full scale and detect clipping before Opus
  encoding. The normal operating target is zero clipped samples and stable K4
  ALC behavior; never add compression, EQ, noise reduction, AGC, or microphone
  processing to program audio.
- Measure accepted sample count and monotonic timestamps at the gated send
  boundary. A shallow pacer keeps at most the current packet in flight. Treat a
  sustained underrun, overrun, stale generation token, or timing error beyond
  the per-mode fixture tolerance as a TX failure and run immediate STOP.
- Provide a developer-only local loopback/capture mode that encodes without
  keying the K4, decodes the result, and compares pixels, VIS, frequencies,
  duration, and line timing. Captures and logs remain outside Git unless they
  are sanitized deterministic fixtures.

## Interruptions, power, and accessibility

- Keep the screen awake while SSTV is visible and hold the minimum Android
  processing protection required during an active TX. Do not run keyed SSTV as
  a background service: activity pause, screen lock, task removal, call/audio
  interruption that suspends the app, or loss of network invokes immediate STOP.
- Thermal or battery warnings do not alter the K4 automatically. Before TX,
  block when Android reports a critical shutdown condition; during TX, abort on
  an inability to maintain real-time pacing. A merely low battery produces a
  warning with the estimated TX duration.
- All essential state uses text/icon/shape as well as color: `RX`, `TX`,
  `MIC OFF`, sync/slant state, and progress. Provide accessible names for every
  icon, logical focus order, scalable text without clipping, and at least
  48 dp touch targets for required actions. STOP remains larger than 48 dp.
- Respect the system's reduced-motion setting by removing decorative animation;
  the scan line may step discretely because it conveys real progress. Do not
  rely on haptics or sound for safety feedback, though a short haptic response
  may accompany STOP when available.
- Preserve image-editing usability with TalkBack where practical, but drawing
  is an optional pointer/stylus feature. Every composition must remain possible
  using image selection, templates, and keyboard-editable text without drawing.

## TX image composition editor

Provide a focused, non-destructive editor between image preparation and TX
confirmation. Its canvas always has the selected SSTV mode's exact aspect ratio
and previews the actual pixels that will be encoded. The background photo,
text, drawing, and simple shape objects remain editable until the operator
approves the final frame; do not repeatedly flatten edits into the source.

### Mobile interaction

- Keep the image canvas dominant. Use one bottom tool row: **IMAGE**, **TEXT**,
  **DRAW**, **SHAPE**, **TEMPLATE**, **UNDO**, **REDO**, and **PREVIEW**.
  Selecting a tool opens a compact contextual sheet rather than a permanent
  desktop toolbar.
- In selection mode, tap an object to select it, drag to move, pinch to scale,
  and use a visible rotation handle when rotation is applicable. Provide
  duplicate, bring forward, send backward, and delete in the contextual sheet;
  do not require a layer-management window.
- Two-finger pan/zoom navigates the canvas. One finger edits the selected object
  or draws while DRAW is active. Stylus input draws directly and uses pressure
  for width only when Android/Qt reports reliable pressure; finger drawing must
  remain fully functional.
- Provide a bounded multi-step undo/redo history covering image crop/rotation,
  object creation/deletion, movement, text changes, and drawing strokes. CLEAR
  DRAWING affects ink only; RESET COMPOSITION requires confirmation and returns
  to the prepared background image.

### Text blocks

- Support multiple independent, multiline text blocks. Tap **TEXT**, then tap
  the canvas or use **ADD TEXT**; the Android keyboard opens immediately. Tap a
  block to edit its content and properties without deleting/recreating it.
- Offer a curated, device-independent set optimized for low resolution. The
  first release exposes 16 combinations across Sans, Condensed/Wide Sans,
  Serif, Monospace, Cursive, and bundled Inter, including Light, Bold, Black,
  and Italic variants where useful. Font-family/style and size selections apply
  immediately to the selected text block and each remains undoable. Do not
  expose hundreds of device-dependent fonts or download fonts from the network.
- Provide text size, foreground color, left/center/right alignment, line
  spacing, and opacity. For RF readability, allow either a simple solid or
  translucent background plate or a thin contrasting outline. Do not add drop
  shadows, bevels, gradients, textures, animated text, or decorative 3-D
  effects.
- The first-release touch palette includes explicit Red, Orange, Yellow, Green,
  Blue, Indigo, and Violet choices plus black, white, charcoal, multiple greys,
  and silver. Selecting a swatch immediately applies it to the selected text
  block and creates one undo step. A full color picker and recently used colors
  can follow after device validation. Warn when text is likely to become
  unreadable at the selected mode's final pixel resolution.
- Text stays inside the transmitted frame by default. Show alignment/snap guides
  for edges and center, but allow deliberate partial clipping after a warning.

### Drawing and simple markup

- DRAW provides Pen and Marker, selectable color and width, Eraser, and Clear
  Drawing. Smooth sampled touch/stylus points into a stable stroke while
  preserving deliberate corners; render strokes at final-frame resolution for
  consistent TX output.
- SHAPE is deliberately limited to line, arrow, rectangle, and ellipse with
  color and width. A rectangle/ellipse may optionally use a plain solid or
  translucent fill. Do not include stickers, emoji packs, clip-art libraries,
  airbrushes, artistic filters, or AI effects.
- Keep ink as editable stroke groups until final approval. The eraser affects
  ink and selected shapes, not the source photograph or text layers.

### QSO templates and dynamic fields

- Save and recall reusable overlay templates independently from the background
  photograph. Supply simple starter layouts for **CQ**, **REPORT**, and **73**;
  users can create, rename, overwrite, duplicate, and delete their own.
- Templates may contain text blocks, shapes, and drawing but no hidden radio
  commands. Support only useful local substitutions such as `{MYCALL}`,
  `{HISCALL}`, `{RST_SENT}`, `{RST_RCVD}`, `{GRID}`, `{UTC}`, and `{FREQ}`.
  Show resolved text directly on the canvas and flag any missing value before
  TX confirmation.
- Remember the last composition draft, template, and editable objects locally
  so an interrupted QSO can resume. After SSTV TX returns to AUTO RX, preserve
  the draft but do not reopen the editor automatically.

### Finalization boundary

- **PREVIEW** temporarily hides edit handles and shows the exact mode-native
  frame. Provide **BACK TO EDIT** and **USE THIS IMAGE**.
- **USE THIS IMAGE** resolves template fields, renders all objects in the
  already mode-native canvas, converts color as required by the codec, and
  freezes those pixels and their composition revision for TX confirmation. It
  does not resize or rescale the image.
- Any edit after preview invalidates the frozen frame and requires a new
  preview. The encoder accepts only that frozen revision, ensuring What You See
  Is What You Transmit.

## Implementation sequence

1. Select or implement a GPL-compatible SSTV codec and add deterministic unit
   fixtures for every enabled mode. Verify licensing and Android ARM64 build
   integration before adding UI.
2. Add a thread-safe, non-playback RX PCM tap plus decoder worker and live
   image model. Include raw active-image buffering, robust sync-line timing,
   and live automatic slant correction. Keep the real-time audio output path
   allocation-free.
3. Implement the receive screen, auto/manual selection, gallery, and save/share
   flow; validate using recorded SSTV audio and synthetic fixtures.
4. Add the Android gallery/camera bridge, non-destructive crop/resize pipeline,
   layer-based TX composition editor, exact output preview, and encoder worker.
5. Add exclusive program-audio TX ownership, the confirmation/stop UX, and
   failure cleanup. Test TX timing with a local audio capture before keying a
   radio.
6. Perform controlled K4 TEST/low-power validation, then on-air receive and TX
   tests for each released mode and Android route.
7. Implement the app-private QSO database, draft/editor workflow, image links,
   and ADIF export only after RX callsign recognition and Reply metadata are
   stable, so logging does not hard-code temporary decoder/UI interfaces.

## Validation checklist

- Cross-decode every QK4-generated v1 mode with at least two independent SSTV
  implementations, and decode their generated fixtures in QK4. Verify VIS,
  tone frequencies, component order, dimensions, duration, and color bars.
- Decode each supported mode from clean, weak, clipped, and interrupted audio
  fixtures without blocking normal RX playback.
- Generate deterministic fixtures with positive and negative sample-clock
  error, missing syncs, false sync candidates, fading, jitter, and mid-image
  recovery. Verify AUTO SLANT converges without visible jumps, corrects both
  preview and saved output, locks against late fades, and falls back safely to
  nominal timing when confidence is inadequate.
- Compare decoded reference grids before and after correction and establish
  quantitative release limits for residual horizontal drift, sync inlier rate,
  convergence time, and supported clock-error range for every released mode.
- Confirm a new image, RX reset, and forced mode change clear the prior
  calibration, while manual post-receive adjustment changes only that image.
- Confirm mode detection and manual override leave completed images intact.
- Confirm landscape touch controls, gallery scrolling, preview, confirmation,
  and stop controls remain reachable on the compact phone layout. Repeat the
  complete workflow in portrait; verify rotation in AUTO RX and TX preparation
  preserves decoder/composition state, reflows rather than clips controls, and
  keeps STOP SSTV visible without scrolling while keyed.
- Confirm Android Photo Picker and Storage Access Framework fallback behavior,
  full-resolution camera capture, cancellation, permission denial, activity
  recreation, EXIF rotation, and temporary-file cleanup on API 26 through the
  current target API.
- Confirm portrait, landscape, square, panoramic, very large, and undersized
  source images produce the exact registered dimensions for every TX mode;
  verify both FILL / CROP and FIT / BARS without aspect-ratio distortion.
- Confirm multiple multiline text blocks, every bundled font/style, colors,
  background plates/outlines, alignment, move/scale/rotation, clipping warnings,
  pen/marker/eraser behavior, pressure and non-pressure input, supported shapes,
  object order, and bounded undo/redo on finger and stylus-capable devices.
- Confirm CQ/REPORT/73 and user templates resolve all supported fields, identify
  missing values, remain independent of the background image, persist locally,
  and never execute a CAT or radio command.
- Compare the editor's handle-free PREVIEW pixel-for-pixel with the frozen frame
  passed to every mode encoder. Verify edits invalidate stale previews and that
  text/strokes remain legible at 320-pixel-wide modes as well as PD modes.
- Confirm USB-C, Bluetooth, speaker, and hearing-aid RX routes do not change
  decoding. Hearing aids remain RX-only.
- Confirm transmit audio frame continuity and correct encoded duration for
  every released mode, including UI repaint and temporary network jitter.
- Confirm a replacement Gallery/Camera source has no markup or undo history
  from the previous source. Verify fresh installs default FSK ID and CW ID on;
  upgrade migration preserves the prior CW choice and initially leaves the new
  FSK choice off; both checkboxes then persist independently across Reply,
  image/mode changes, STOP, failure, return to radio, orientation, and restart;
  remembered 20 WPM appears only while CW is checked; the both-enabled waveform
  is image, FSK ID, CW ID, then RX; neither ID sends a K4 mode command; and STOP
  works during either ID phase.
- Verify QSO drafts never become confirmed records without **SAVE QSO**. Test
  database creation/migration rollback, draft recovery, callsign-source prefill,
  UTC and band-edge handling, attachment independence, edit/delete confirmation,
  filtered/selected/full ADIF export, Unicode and ADIF length handling, Android
  document-picker cancellation/failure, and parsing every exported file with an
  independent ADIF reader.
- Calibrate program-audio level with local capture and K4 TEST mode before any
  antenna-connected test. Verify zero digital clipping, acceptable K4 ALC,
  frequency accuracy, continuous phase, bounded packet depth, and that progress
  pauses on a deliberately stalled send path.
- Exercise every TX preflight blocker and warning, including stale radio state,
  external TX ownership, unsupported mode/filter, invalid frozen preview, and
  a state change between confirmation and keying. Confirm no warning silently
  changes the K4.
- Confirm Stop, cancel, disconnect, TX failure, activity pause, and app exit
  all release `RX;` and prevent further SSTV frames.
- Instrument STOP from touch-down through audio-gate closure, final accepted TX
  audio packet, CAT `RX;` write, and K4 receive-state acknowledgement. Verify
  under every streaming-latency tier and while the UI and network are busy that
  no stale SSTV packet is accepted after the stop gate closes and that STOP is
  never hidden or disabled while keyed.
- Confirm first open, reopen, completed TX, stopped TX, and failed TX all land
  in AUTO RX with Main-RX decoding armed.
- Confirm BACK TO RADIO and Android Back return to the live QK4 console without
  disconnecting or changing frequency, mode, filter, power, antenna, or audio
  routes; verify the TX-active safety prompt and its cancel/confirm paths.
- Confirm no SSTV action mutates K4 settings without an explicit operator
  action, and record physical K4/device evidence separately from automated
  tests.
- Confirm first-run TX defaults to Scottie S1, every released mode remains
  touch-selectable, an explicit selection persists locally, and RX mode
  detection never silently changes the TX choice.
- Confirm TX POWER follows unsolicited K4 state, uses the existing RF-power
  command path, is changed only by an explicit operator gesture, is frozen and
  disabled throughout keyed SSTV TX, and remains consistent with the main
  console before and after returning to AUTO RX.
- Confirm RX retention limits, starring, PNG metadata, explicit export/share,
  draft crash recovery, low-storage behavior, and cleanup of camera temporary
  files without deleting exported or starred images.
- Test screen lock, Home/task switch, incoming call/audio interruption, network
  loss, activity destruction, thermal pressure, low battery, and rotation at
  several scan positions. Every unsafe TX interruption must use the same STOP
  path and no orientation change may advance or reset progress.
- Test TalkBack labels/focus, 48 dp targets, large fonts, display scaling,
  color-blind use, reduced motion, keyboard-only text composition, and STOP
  reachability in both orientations.
