# Screen orientation policy

This is the authoritative source for screen orientation and layout-selection
requirements on Android phones, Android tablets, iPhones, and iPads. It applies
to contributors and coding assistants. README, AGENTS, scope documents, and PRs
must link here and keep any summaries consistent with this policy. Historical
test reports describe the tested build; they do not override these requirements.

Requirements do not establish implementation, release, or device-validation
status. Record evidence in the
[device-validation table](PROJECT_STATUS.md#device-and-orientation-validation).
iPhone contributions must preserve the same per-module behavior as Android
phones. A proposed exception must identify the affected module and device class
and obtain explicit maintainer approval before implementation; a platform port
or tablet-layout change does not implicitly authorize a phone behavior change.

## Phone requirements (Android and iPhone)

| Screen or entry point | Required orientation behavior on both phone platforms |
|---|---|
| Main radio console | Landscape. |
| SSTV | Portrait and landscape; restore the landscape radio console on exit. |
| FT8/FT4 | Portrait-only; restore the landscape radio console on exit. |
| Logbook opened from FT8/FT4 | Remain portrait-only, including when returning to FT8/FT4. |
| Logbook opened from SSTV | Follow either sensor orientation; return to SSTV with its portrait/landscape behavior. |
| Logbook opened from the radio console | Open in landscape, enable both orientations after a phone turn, and restore landscape on exit. |

Compact layout is a presentation choice, not an orientation lock. Using the
compact layout must not override these per-module rules. A tablet orientation
exception must not change Android-phone or iPhone behavior.

## Tablet requirements (Android tablet and iPad)

| Screen or entry point | Required behavior on both tablet platforms |
|---|---|
| Main radio console | Landscape. A portrait console is not authorized by tablet support. |
| SSTV | Portrait and landscape; restore the landscape radio console on exit. |
| FT8/FT4 | Portrait is the baseline. Landscape, or both orientations, may be enabled after tablet acceptance for the affected platform and window configuration. Return to the landscape radio console on exit. |
| Logbook opened from FT8/FT4 | Follow the orientations validated and enabled for the invoking FT8/FT4 screen; restore that screen's orientation behavior on return. |
| Logbook opened from SSTV | Follow either sensor orientation; return to SSTV with its portrait/landscape behavior. |
| Logbook opened from the radio console | Open in landscape, allow both orientations after a device turn, and restore landscape on exit. |

Android tablet validation does not establish iPad validation, or vice versa.
Preserve the production Android compact-layout override until the relevant
tablet acceptance checks pass. iPad contributions must likewise retain a
compact fallback until their regular layout is validated. Neither fallback
changes the module orientation rules above.

## Device class, available space, and multitasking

- Use one shared application decision for device class. Individual modules
  must not invent independent width, resolution, or model tests. Treat unknown
  device classes conservatively: compact layout and phone orientation rules
  until explicitly classified and validated. A phone must not gain tablet
  orientation exceptions merely because it has a large screen or external display.
- Device class and layout size are separate decisions. A tablet in a narrow
  window remains a tablet, but must use compact presentation when its usable
  window cannot fit the regular layout. Compact presentation alone does not
  change which module orientations are allowed for that tablet.
- Base layout fit on the current usable application window in logical units,
  accounting for safe areas, system bars, and the software keyboard. Physical
  screen diagonal or full-screen resolution alone is not sufficient.
- Re-evaluate fit when the window resizes, rotates, enters or leaves split-screen
  or iPad multitasking, or its insets change. Required controls must remain
  reachable through reflow or touch-safe scrolling; do not clip controls or
  shrink touch targets merely to retain the regular layout.
- Device rotation and window resizing are different events. If the operating
  system cannot provide a window that meets a module's orientation requirement,
  do not silently declare that configuration supported or reinterpret it as
  permission for a new orientation. Keep the supported window mode as the
  release target and document the limitation and any proposed exception.
- Claim split-screen, resizable-window, or iPad multitasking support only for
  configurations that pass physical acceptance. Record both the full-screen
  and constrained-window results, including keyboard-open behavior.

## FT8 and FT4

- Android phones and iPhones must keep FT8 and FT4 portrait-only.
- Landscape FT8/FT4 support for tablet-class devices is in scope and may be accepted.
- A tablet implementation must use the shared application device-class decision. Do not add an independent width, resolution, or model check inside the FT8/FT4 module.
- The tablet path must not enable FT8/FT4 landscape mode on phones or change the phone flow that enters FT8/FT4 in portrait and restores the radio screen orientation on exit.
- A responsive tablet implementation may support both tablet orientations when the layout remains usable. Landscape support does not require enabling landscape on phones.

## Validation records

The phone requirements carry the established Android behavior forward to
iPhone. Verify it on a physical iPhone before claiming iOS orientation parity;
sharing compact UI code alone is not acceptance evidence. Record current
results in [project status](PROJECT_STATUS.md#device-and-orientation-validation)
instead of maintaining a second support-status list here.

## Acceptance checks for iPhone orientation work

- On a physical iPhone, exercise every entry point in the phone requirements
  table, rotate the device in each screen, and verify the orientation on exit.
- Confirm FT8/FT4 and its logbook remain portrait-only, including after visiting
  SSTV or the radio logbook, and return to the landscape radio console on exit.
- Confirm in-app Back/close controls, software-keyboard dismissal, safe-area
  insets, and rotation keep required controls reachable without clipping.
- Confirm backgrounding and resuming the app preserves the active module's
  orientation requirements.
- Record the device, OS version, tested commit, and results. Keep iPhone
  acceptance evidence separate from Android-phone acceptance evidence.

## Acceptance checks for tablet layout and orientation work

Before a regular tablet layout or tablet FT8/FT4 landscape support is merged:

- Verify it on at least one physical tablet of each platform claimed by the
  change. Test every enabled orientation and claimed window configuration.
- Verify a phone of each affected platform still follows the phone requirements,
  including FT8/FT4 portrait entry and return to the landscape radio console.
  Shared changes need Android-phone and iPhone checks once both are implemented;
  record unavailable platforms as pending, not passed.
- Confirm live VFO displays, meters, panadapter, PTT, control panels, and popups
  remain reachable in the regular layout and the compact fallback.
- Confirm the waterfall, activity list, My QSO view, RX/TX tone controls, Call/CQ/Halt controls, Receive control, audio setup, and logbook remain reachable without clipped or off-canvas controls.
- Confirm Android Back where applicable, in-app Back/close, software-keyboard
  dismissal, safe-area insets, rotation, receive restart, and return-to-radio behavior.
- Exercise all affected logbook entry points, window resizing, and app
  background/resume. Do not infer multitasking support from a full-screen test.
- Keep tablet-specific layout work isolated from radio protocol, CAT, audio, decoding, and transmit timing unless a separate functional change requires it.

Until this validation exists, the production Android build continues to use the compact phone layout on all display sizes as described in `AGENTS.md` and `docs/PROJECT_STATUS.md`.
