# QK4 Mobile v1.0.4.1

QK4 Mobile v1.0.4.1 refines CTR2-MIDI Extended Button Mode and improves the
local sidetone used with straight keys and external keyers.

## CTR2 Extended Button Mode

- Use all 48 mode-specific short and long button assignments provided by
  CTR2-MIDI Extended BTN Mode.
- Keep the existing 12 Normal-mode assignments under Home when Extended Button
  Mode is enabled. The 36 newly available Knob mode 1–3 assignments start
  disabled so actions are not unexpectedly duplicated.
- Retain the documented directional knob-note ranges when changing modes:
  notes 40–55 in Normal Button Mode and notes 60–75 in Extended Button Mode.
- Preserve notes 40–48 for the corresponding physical CTR2 buttons while
  Extended Button Mode is active.

## User-editable mapping files

- Save whether the mapping uses Normal or Extended Button Mode.
- Identify every button by its physical label, MIDI note, short or long press,
  knob mode, and assigned action or K4 command.
- Include guidance for Wheel A, Wheel B, Slider A, Slider B, and directional
  Button output so the file can be edited confidently on a PC.
- Automatically correct the exact duplicated button layout created by the
  earlier 1.0.4.1 test candidate while preserving operator-customized layouts.

## Straight-key sidetone

- Improve the continuity of the locally generated sidetone for straight keys,
  bugs, and external keyers to reduce rasp caused by Android audio timing.
