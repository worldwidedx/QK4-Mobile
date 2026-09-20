# QK4 Mobile changelog

This changelog records releases of **QK4 Mobile for Android** only. QK4 Mobile
is derived from [QK4 by Mike Garcia, KF5O](https://github.com/mikeg-dal/QK4),
but upstream QK4 history is intentionally not repeated here.

## Release-note policy

- Add a new version section for every published QK4 Mobile release.
- Describe user-visible changes and important compatibility or upgrade notes.
- Use **Added**, **Changed**, **Fixed**, and **Known limitations** as needed.
- Do not list unverified radio behavior as fixed.

## [0.8.3] - 2026-08-15

### Fixed: transverter frequency display

- Retained leading digits in VHF, UHF, and higher transverter frequencies.
  For example, `144.200.000` is no longer truncated to `44.200.000`.
- Expanded the VFO display capacity through 9.999 GHz, including frequencies
  such as `1.296.000.000`, while retaining the compact HF presentation.

### Changed: direct frequency entry

- VFO A and VFO B frequency entry now accept radio-style MHz shorthand and
  imply omitted trailing zeros: `7.2` becomes `7.200.000`, `7.215` becomes
  `7.215.000`, and `144.2` becomes `144.200.000`.
- Fully grouped input such as `7.215.000` and raw-Hz input remain supported.
- Updated the entry instructions and validation feedback.

## [0.8.2] - 2026-08-14

### Fixed: CW and digital RX audio quality

- Eliminated the raspy or rough sound previously noticeable on steady CW and
  digital receive audio through Android devices.
- Made the K4's 12 kHz to Android's 48 kHz playback resampling continuous
  across network-packet boundaries.
- Preserved the unwritten portion of non-blocking Android audio writes instead
  of dropping samples when the playback buffer accepts only part of a packet.
- Kept later RX packets queued until the current packet has been fully accepted,
  preserving sample order and audio continuity.

## [0.7.6.5] - 2026-08-11

### Fixed: K4-style EQ FLAT restore

- Corrected the **FLAT** control in Main RX EQ, Sub RX EQ, and TX EQ.
- First tap sets the selected equalizer to a flat response. Tapping **FLAT** a
  second time restores the exact eight-band EQ curve that was active before
  FLAT was selected.
- Main and Sub RX retain one shared RX EQ restore curve, matching their shared
  K4 RX EQ behavior. TX EQ retains its own independent restore curve.
- These controls update the associated **K4 radio EQ settings**; they are not
  merely local phone-display changes.

### Mobile operating recommendation

- For mobile operation, set the K4 RX EQ flat, then use the phone or headset's
  own tone/EQ controls for personal listening preference. This keeps the K4's
  receive-audio path clean and predictable while allowing local tailoring.

## [0.7.7.0] - 2026-08-10

### Added: GEN shortwave-listening band bank

- The previously unused **GEN** control in the BAND menu is now a mobile-only
  Shortwave Listening (SWL) band-bank selector.
- Tap **GEN** to replace the normal amateur-band button grid with 120m, 90m,
  75m, 60m, 49m, 41m, 31m, 25m, 22m, 19m, 16m, 15m, 13m, and 11m.
- GEN remains visible and white while the SWL grid is active; tap it again to
  restore the normal K4 amateur-band grid.
- The first selection of an SWL band tunes the active VFO to its supplied
  center-frequency default and selects **AM** mode. GEN respects **B SET**,
  tuning VFO B when B SET is active and VFO A otherwise.

### Added: per-band GEN frequency recall

- Each GEN/SWL band remembers the last frequency tuned while that GEN band is
  active. For example, tune 11m to 27.185 MHz AM, select another GEN band, and
  return to 11m to recall 27.185 MHz.
- Saved GEN frequencies persist across app restarts. The supplied SWL values
  are first-use defaults only; they do not restrict an operator's chosen
  listening frequency.

### Compatibility and intended radio behavior

- GEN is separate from normal Elecraft amateur-band selection and does not
  replace, clear, or simulate the K4's native **BN** band-stack behavior.
- The normal 1.8–50 MHz amateur-band buttons, MEM, XVTR, and K4 band-stack
  handling remain unchanged.
- GEN selections use direct VFO frequency commands, so the K4 may add the
  selected frequency to its nearest applicable native amateur-band stack. This
  expected K4 behavior is intentionally preserved.

## [0.7.6.4] - 2026-08-10

### Fixed

- Long-pressing **ATTN**, **NB LEVEL**, **NR ADJ**, or **NTCH MANUAL** in the
  right-side K4 Controls panel now closes that panel before opening its
  adjustment popup, so the editor and its controls remain fully accessible.
- Added a standard **↩** return control to those adjustment popups so an
  operator can explicitly close the editor when finished.

## [0.7.6.3] - 2026-08-10

### Added

- Added an Android RX audio preference for devices exposed by the operating
  system as a dedicated hearing-aid output (`TYPE_HEARING_AID`). This follows
  the existing external-output route and its reconnect handling, while keeping
  the established K4 RX audio stream unchanged.

### Known limitations

- Hearing-aid routing is device- and phone-specific and awaits field
  validation with Starkey Livio 2400 aids. This is an RX-only route; QK4
  continues to use the phone microphone for TX unless Android exposes a
  separate supported two-way input device.

## [0.7.6.2] - 2026-08-09

### Changed

- Temporarily use the proven compact landscape phone layout on every Android
  display size, including tablets. This prevents devices from selecting the
  unvalidated alternate tablet layout while dedicated tablet testing is not
  available.
- The original screen-size, density, physical-size, and environment-override
  selection logic remains commented directly beside the temporary override for
  restoration after tablet validation.

### Known limitations

- Tablet-specific layout has not yet been visually tested on physical hardware.
  Tablets currently use the phone layout as an interim compatibility fallback.

## [0.7.6.1] - 2026-08-08

### Fixed

- Fixed USB-C headset hot-swap for both receive and transmit audio.
  - A USB-C headset can now be connected or disconnected after the radio
    session is already active.
  - RX audio follows the available audio route.
  - When a USB-C headset microphone is present, TX uses that microphone rather
    than the phone's built-in microphone.
  - TX/RX switching remains stable while using USB-C audio.

### Improved mixed-headset routing

- If Bluetooth RX audio is already active and a USB-C headset/microphone is
  connected, Android may keep RX on Bluetooth while using the USB-C microphone
  for TX. This permits separate receive and transmit audio paths when
  supported by the phone's audio system.

## [0.7.6] - 2026-08-08

### Fixed

- TLS connections to K4 radios now bundle the required Android OpenSSL 3
  runtime and select the application-packaged libraries reliably.
- Bluetooth audio routing now retains headset RX audio through TX/RX changes
  and selects the active two-way Bluetooth endpoint for transmit audio.

### Known limitations

- Hot-swapping a USB-C audio headset after connecting to the radio is not yet
  supported. Connect the headset before connecting to the K4. Bluetooth audio
  routing is unaffected.

## [0.7.5.1] - 2026-08-04

### Fixed

- Restored the original A/B VFO mode-control geometry by removing the extra
  v0.7.5 touch padding. This keeps the SUB/DIV indicators clear of the VFO B
  frequency and meter area.

## [0.7.5] - 2026-08-03

### Fixed

- Android spectrum-renderer compatibility: use a universally supported RGBA8
  normalized spectrum texture on Android, while preserving QK4's R32F desktop
  renderer unchanged. This targets devices that render waterfall data but omit
  the normal spectrum trace.

### Improved touch operation

- Tap the green on-screen **B SET** indicator to turn B SET off quickly. B
  SET remains enabled only from CTRL to avoid accidental activation.
- Tap either displayed filter shape to cycle that receiver's setting like the
  K4 console: **FIL1 → FIL3 → FIL2 → FIL1**. The visible FIL label updates
  immediately; no redundant feedback popup is shown.
- Expanded the A and B VFO/mode touch targets without extending either one
  toward the central TX selector. Tapping the colored VFO block, its mode
  label, or a small outer/bottom margin opens that receiver's MODE popup.
- Removed the redundant “MODE controls opened” feedback flash from CTRL MODE.

## [0.7.4] - 2026-08-02

### Added

- **Experimental:** while the phone's touch-latched TX/RX control is in TX,
  an in-window shield blocks all other console touch, scroll, and button input.
  The red TX ON control remains available in the same position so the operator
  can always return immediately to RX. The shield is UI-only and does not
  alter K4 PTT, CAT, microphone, or audio-stream behavior.

### Known limitations

- The TX input shield has been physically tested during a successful contact
  on the development phone, but remains experimental pending broader field
  testing and Android-device coverage.

## [0.7.3] - 2026-08-02

### Fixed

- Restored reliable phone TX/RX operation. The touch-latched TX/RX control now
  explicitly keys the K4 with `TX;` and releases it with `RX;`, while retaining
  the upstream microphone-stream path for voice audio. This is deliberate PTT,
  not VOX.
- Corrected the Android panadapter span controls to match QK4/K4 convention:
  `+` increases span and `-` reduces span.
- Restored the VFO B cursor indicator when it is selected from the DISP menu.

### Added

- Added a local PHONE MIC slider in CTRL for phone/headset input level. It
  adjusts the pre-encode Android microphone gain and does not change the K4's
  radio MIC setting.

## [0.7.2] - 2026-08-01

### Fixed

- Reworked Android transient dialogs and menus as in-window overlays. This
  prevents the Android 17/Pixel crash caused by opening a separate Qt window
  from the live GPU-rendered radio console, while retaining the panadapter,
  TCP/TLS connection, radio control, and TX/RX audio paths.
- Closing or switching a primary phone menu now also dismisses its related
  secondary editor. In particular, confirming SSB bandwidth from TX closes the
  bandwidth editor and TX menu together, retaining the selected setting.
- Right CTRL-bank scrolling now suppresses accidental REV activation. An
  intentional tap or hold retains the original momentary REV behavior.

## [0.7.1] - 2026-08-01

### Fixed

- Removed app-side uppercase/lowercase conversion from all connection-profile
  fields. Android keyboards, including Gboard, now retain full control of
  manual Shift and case-sensitive password entry.
- Removed the redundant in-app CASE button. Name, Host/IP, Password, and TLS
  Identity fields use standard Android text input without forced case.

## [0.7.0] - 2026-08-01

First QK4 Mobile release for Android ARM64 phones.

### Added

- Landscape, touch-first K4 console optimized for phone operation.
- K4 connection-profile management, TCP/TLS radio connection, RX streaming
  audio, microphone TX audio, and PTT.
- VFO A/B controls, transmission-VFO selection, direct frequency entry,
  selectable tuning digits/steps, and step-synchronized panadapter tuning.
- Spectrum/waterfall display, mini-pan, touch panadapter tuning, and
  user-adjustable waterfall height.
- Touch panels for CTRL, TX, DISP, FN, Main RX, and Sub RX functionality.
- Main/sub AF controls; mode-aware filter BW/shift controls; RIT/XIT jog.
- CW text decode and editable/executable F1–F8 message macros.
- Local persistent red Peak Hold trace and local WTR CLRS waterfall brightness
  adjustment using Elecraft's 5–30 range.
- QK4 Mobile About screen and WorldwideDX.com attribution.
- Release-signed ARM64 APK distribution using a private PKCS#12 key.

### Fixed

- Connection-profile save/connect sequence and Android keyboard case behavior.
- Touch scrolling versus accidental button activation in control panels.
- Control-panel reachability, overlay feedback placement, macro-editor theme,
  and access to the F8 macro field.
- Compact-layout sizing, antenna/status/filter visibility, and waterfall/scope
  initial 50/50 split.

### Known limitations

- Physically validated on the Samsung Galaxy S26 Ultra only; other Android
  phone sizes and manufacturers need validation.
- The release APK can still display standard Android sideloading/Play Protect
  messaging until the app is distributed through Google Play.
- DR+ display support is deferred pending verified K4 state semantics.
