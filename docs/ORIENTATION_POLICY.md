# Screen orientation policy

This policy defines which orientation changes are in scope for each Android device class. It applies to contributors and coding assistants.

## FT8 and FT4

- Phone-class devices must keep FT8 and FT4 portrait-only.
- Landscape FT8/FT4 support for tablet-class devices is in scope and may be accepted.
- A tablet implementation must use the shared application device-class decision. Do not add an independent width, resolution, or model check inside the FT8/FT4 module.
- The tablet path must not enable FT8/FT4 landscape mode on phones or change the phone flow that enters FT8/FT4 in portrait and restores the radio screen orientation on exit.
- A responsive tablet implementation may support both tablet orientations when the layout remains usable. Landscape support does not require enabling landscape on phones.

## Current module behavior

- The main radio console uses landscape orientation.
- SSTV supports portrait and landscape orientation.
- FT8/FT4 uses portrait orientation on phones.
- The shared logbook follows the orientation rules of its invoking module and its documented rotation behavior.

## Acceptance checks for tablet FT8/FT4 work

Before tablet landscape support is merged:

- Verify it on at least one physical tablet in landscape.
- Verify a phone-class device still opens FT8/FT4 in portrait and returns cleanly to the radio screen.
- Confirm the waterfall, activity list, My QSO view, RX/TX tone controls, Call/CQ/Halt controls, Receive control, audio setup, and logbook remain reachable without clipped or off-canvas controls.
- Confirm Android Back, software-keyboard dismissal, safe-area insets, rotation, receive restart, and return-to-radio behavior.
- Keep tablet-specific layout work isolated from radio protocol, CAT, audio, decoding, and transmit timing unless a separate functional change requires it.

Until this validation exists, the production Android build continues to use the compact phone layout on all display sizes as described in `AGENTS.md` and `docs/PROJECT_STATUS.md`.
