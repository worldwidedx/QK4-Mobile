# CTR2-MIDI integration scope

Status: implemented on `codex/ctr2-midi-v2`; awaiting physical-device validation.

Reference: CTR2-MIDI Operation Manual v2.01.01a:
https://ctr2.lynovation.com/wp-content/uploads/2026/03/CTR2-MIDI_Operation_Manual_v20101a.pdf

## Design boundary

QK4 Mobile will receive and map the MIDI messages emitted by CTR2-MIDI. It
will not duplicate settings that are configured and executed entirely by the
controller.

CTR2-managed settings that do not need QK4 controls:

- Beep Mode (Off, Normal, or Long-Press)
- Speed Tuning selection (Off, Normal, or Fast)
- Knob Map 1/2 selection and control-type configuration
- Bluetooth radio enable/disable
- Extended Button mode enable/disable
- Button calibration, firmware updates, configuration import/export, and
  factory reset
- Flex WiFi, HID, and RemoteTx-specific operation

QK4 must correctly interpret the messages resulting from these settings. In
particular, it must preserve WheelA magnitude so CTR2 proportional/speed tuning
works instead of reducing every encoder report to one tuning step.

## Discovery and persistence

- Add a built-in CTR2-MIDI profile.
- Discover CTR2-MIDI over both Bluetooth LE MIDI and USB MIDI.
- Recognize normal CTR2 BLE names such as `CTR2_####` and accommodate the USB
  ESP32-S3/XIAO identity exposed by Android.
- Remember the selected transport and physical-device identity.
- Remember the independently selected CW and CTR2 device endpoints.
- Remember the active CTR2 mapping and provide Restore Defaults plus complete
  mapping-file save/load in Android-accessible document storage.
- Loading replaces the complete mapping; mappings are never merged. Warn and
  offer Save / Don't Save / Cancel only when the current mapping has pending edits.

## Device-specific keying modes

Keying capabilities are profile-specific, not universal across MIDI devices:

- TinyMIDI and HaliKey MIDI: selectable Paddles or Straight Key / External Keyer. In
  straight-key mode, let the user select the left or right physical input and
  ignore the unused input.
- CTR2-MIDI: selectable Paddles or Straight Key + PTT, following the CTR2
  paddle-jack modes and allowing TIP/RING assignment to be swapped.
- Custom MIDI: uses the same selectable keying mode after its physical DIT/DAH
  inputs have been learned.

Paddle/iambic input continues through QK4 Mobile's existing local iambic keyer
and K4 `KZ` paddle stream. Straight-key or external-keyer input bypasses the
local iambic element generator and preserves the incoming key-down/key-up
timing using the documented K4 `KZD0000;` and `KZU0000;` raw key elements.

The K4's CW VOX (hit-the-key), QSK, and DLY settings continue to control
transmit and receive behavior. Do not force CW VOX or any other operator
setting merely because a MIDI device connects. A mapped PTT input remains an
explicit, separate PTT source.

CTR2 paddle assignments documented by firmware mode:

| CTR2 configuration | Extended BTN off | Extended BTN on | QK4 use |
|---|---:|---:|---|
| Normal paddle mode | Notes 20/21 | Notes 96/97 | Left/right iambic paddles |
| Extended paddle mode | Notes 30/31 | Notes 98/99 | Straight key and PTT |

## Buttons

Support both CTR2 button layouts:

- Normal BTN mode: six short-press actions (notes 1-6) and six long-press
  actions (notes 11-16). CTR2 sends these on button release.
- Extended BTN mode: all 48 documented button actions, selected by the current
  knob mode.
- Place an **Extended Button Mode** checkbox directly below the Buttons
  description. With it off, show `Button 1 short` through `Button 6 long`.
  With it on, show mode-qualified controls for the device's documented four
  states: `Home knob mode`, `Knob mode 1`, `Knob mode 2`, and `Knob mode 3`.
- Warn that the app selection must match the CTR2's own Extended BTN setting;
  QK4 cannot reliably infer that state from otherwise valid incoming notes.
- Allow every action to map to an applicable K4 command or an existing local
  QK4 function. Do not replace local implementations such as GEN with a radio
  command.
- Save the selected layout as `buttonMode: normal` or `buttonMode: extended`.
  Continue accepting older files that used the `extendedButtons` boolean.
- When Extended Button Mode is first enabled, retain the 12 Normal-mode
  assignments under Home and initialize the 36 newly exposed Mode 1-3
  assignments as Disabled. Do not clone Home actions into every mode.
- Provide typed `Adjust:` button actions for every predefined continuous knob
  action. A knob assigned to **Selected adjustment (button)** follows the last
  such button selection, matching the radio-like workflow where a control is
  selected and the wheel then adjusts it. This is independent of fixed knob
  assignments and does not change the supplied K4-Control defaults.
- Provide a predefined **TX/RX toggle** button action using QK4 Mobile's
  deliberate PTT path; do not emulate it with VOX.

## Knob controls

Support all four CTR2 knob modes and both actions in each mode:

- Turn: CC 100, 102, 104, and 106
- Push and turn: CC 101, 103, 105, and 107

Initial mapping (matching the supplied K4-Control map):

| CTR2 action | QK4 default |
|---|---|
| Home turn | Active VFO tuning |
| Home push-turn | Main AF gain |
| Mode 1 turn | Other VFO tuning |
| Mode 1 push-turn | Filter bandwidth |
| Mode 2 turn | RIT/XIT adjustment |
| Mode 2 push-turn | Noise-reduction level |
| Mode 3 turn | RF power |
| Mode 3 push-turn | CW speed |

All eight assignments remain user-configurable.

CTR2 Map 1 defines CC100 as the speed-sensitive WheelA control and CC101-107 as
absolute SliderA controls. The supplied mapping therefore uses SliderA pickup
for those seven modes and converts every changed position report to exactly one
signed step; the first sample and duplicates do not create movement. Skipped or
coalesced position counts must not multiply the radio-control step. Device tests
of CC102-CC106 confirmed that using raw position differences, or treating these
positions as centered WheelA values, causes large or reversed adjustments.

Support every documented knob-output format. The knob Button range depends on
the device's Extended BTN setting; it must never steal notes 40-48 from the
extended physical buttons:

- Button, Normal BTN mode: directional NoteOn pairs, notes 40-55
- Button, Extended BTN mode: manufacturer-defined range 60-95; the current
  CC100-107 controls use sequential direction pairs 60/61 through 74/75
- SliderA: absolute CC values 0-127
- SliderB: absolute CC values 0-127
- WheelA: relative values centered on 64, including accelerated magnitude
- WheelB: direction values 1 and 126
- WheelB-r: reversed WheelB direction

For absolute SliderA/SliderB mappings, prevent a newly selected knob mode from
jumping the associated radio setting. Establish synchronization on the first
value or require meaningful movement before applying a new absolute value.

## Setup UX and feedback

- Provide a dedicated **CTR2** tab, while retaining the complete v1.0.3 CW
  Keyer tab and behavior as a separate device role.
- Keep controls touch-sized and vertically scrollable without horizontal pan.
- Ensure scrolling does not capture slider or learn-control gestures.
- A predefined wheel action must use the same visible adjustment path as the
  equivalent QK4 touch control. ATTN, NB LEVEL, NR ADJUST, and NTCH MANUAL use
  their existing compact adjustment panels. Volume, filter bandwidth/shift,
  RF power, CW speed, Main/Sub squelch, and Main/Sub RF gain select and reveal
  their existing controls in the K4 Controls drawer. Values already evident on
  the live console, such as VFO frequency and panadapter span, retain that
  visible main-screen feedback. Opaque user-entered K4 commands cannot select
  a semantic surface automatically.
- Show a visible saved confirmation when mappings change.
- When **Return to Operate** is pressed with pending CTR2 mapping edits, offer
  **Apply**, **Abandon**, and **Cancel** before leaving setup. Leave immediately
  without prompting when the mapping is unchanged.
- Keep all live console controls, meters, panadapter, and PTT immediately
  recoverable after closing setup.

## Validation checklist

- BLE and USB discovery, connection, reconnection, and per-device persistence
- Normal and Extended button layouts, including short/long release behavior
- All eight knob actions with Button, SliderA, SliderB, WheelA, WheelB, and
  WheelB-r formats
- Fixed knob actions and button-selected adjustment mode open the correct QK4
  adjustment surface and keep its value synchronized while the wheel moves
- Slow, Normal, and Fast WheelA proportional tuning without lost magnitude
- Iambic paddle timing over USB and BLE
- CTR2 straight key and PTT with TIP/RING swapped both ways
- HaliKey paddle and straight-key/external-keyer modes
- TinyMIDI paddle and straight-key/external-keyer modes
- Sustained straight-key sidetone remains smooth under ordinary Android timer
  jitter while retaining a short attack, fall, and bounded key-up delay
- K4 CW VOX on/off, QSK, and DLY behavior without connection-side mutation
- TEST TX first for keying/PTT safety, followed by controlled on-air validation
