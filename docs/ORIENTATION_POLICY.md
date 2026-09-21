# Screen orientation policy

This policy defines the phone orientation requirements for Android phones and
iPhones, and the scope of Android tablet orientation work. It applies to
contributors and coding assistants. iOS phone contributions must preserve the
same per-module orientation behavior as Android phones. These requirements do
not establish that an iOS implementation has been completed or device-validated.

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

## FT8 and FT4

- Android phones and iPhones must keep FT8 and FT4 portrait-only.
- Landscape FT8/FT4 support for tablet-class devices is in scope and may be accepted.
- A tablet implementation must use the shared application device-class decision. Do not add an independent width, resolution, or model check inside the FT8/FT4 module.
- The tablet path must not enable FT8/FT4 landscape mode on phones or change the phone flow that enters FT8/FT4 in portrait and restores the radio screen orientation on exit.
- A responsive tablet implementation may support both tablet orientations when the layout remains usable. Landscape support does not require enabling landscape on phones.

## Current Android module behavior

- The main radio console uses landscape orientation.
- SSTV supports portrait and landscape orientation.
- FT8/FT4 uses portrait orientation on phones.
- The shared logbook follows the orientation rules of its invoking module and its documented rotation behavior.

The phone requirements above carry this behavior forward to iPhone. Verify it
on a physical iPhone before claiming iOS orientation parity; sharing compact
UI code alone is not acceptance evidence.

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

## Acceptance checks for tablet FT8/FT4 work

Before tablet landscape support is merged:

- Verify it on at least one physical tablet in landscape.
- Verify a phone-class device still opens FT8/FT4 in portrait and returns cleanly to the radio screen.
- Confirm the waterfall, activity list, My QSO view, RX/TX tone controls, Call/CQ/Halt controls, Receive control, audio setup, and logbook remain reachable without clipped or off-canvas controls.
- Confirm Android Back, software-keyboard dismissal, safe-area insets, rotation, receive restart, and return-to-radio behavior.
- Keep tablet-specific layout work isolated from radio protocol, CAT, audio, decoding, and transmit timing unless a separate functional change requires it.

Until this validation exists, the production Android build continues to use the compact phone layout on all display sizes as described in `AGENTS.md` and `docs/PROJECT_STATUS.md`.
