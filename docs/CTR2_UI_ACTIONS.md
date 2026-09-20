# CTR2-MIDI on-screen controls and feedback

QK4 Mobile integrates CTR2-MIDI with the application's operating interface.
Mapped controls do more than send commands to the K4: where QK4 has a suitable
control, the corresponding adjustment appears on screen and follows the CTR2
knob. Other actions update the main operating display or show a brief
confirmation message.

QK4 provides 20 selectable knob adjustments. Fifteen open an on-screen control;
the remaining five use the main operating display and transient feedback.

When a knob mode is mapped directly to an adjustment, turning the knob opens
the appropriate control surface. When a knob mode is mapped to **Selected
adjustment**, pressing an **Adjust:** button opens and selects the surface,
displays `CTR2 KNOB: ...`, and assigns the knob to it.

## FT8 / FT4 tone control

Choose any surrounding button in CTR2 setup. The recommended mapping uses one
physical button and the wheel:

| Input | Action |
|---|---|
| Short press | **FT8/FT4: Switch RX/TX tone** (`adjust_ft8_rx_tx`) |
| Long press | **FT8/FT4: Set tone frequency** (`set_ft8_frequency`) |
| Dial | **Selected adjustment (button)** (`selected_adjustment`) |

The button number is user-selectable, and the actions work in both FT8 and FT4.

1. Short-press the assigned button to choose the RX or TX tone. RX is selected
   when the module opens, so the first short press selects TX.
2. Turn the wheel to move the dashed preview marker for the selected tone.
3. Long-press the assigned button to set that frequency and finish the
   adjustment.

Setting TX enables **Hold TX**. Setting RX focuses **My QSO** on decoded signals
near that audio frequency while **All** continues to show the full band. The
solid markers show the currently set frequencies while a dashed marker shows the
pending adjustment. After a frequency is set, the wheel remains inactive until
the operator selects RX or TX adjustment again.

The RX/TX readout opens the **FT8/FT4 tone tuning** panel. It provides **Select
RX tone**, **Select TX tone**, **Set tone frequency**, and tone steps of 1, 5,
10, 25, and 50 Hz. Touch selection on the waterfall remains available.

While a digital tone is selected, **Rate**, **KHZ**, VFO selection, and band-step
actions cannot change the radio frequency. Tap a radio-frequency digit first to
select deliberate RF tuning. Selecting an RX or TX tone returns the wheel to
tone control.

The optional [FT8 sample mapping](QK4-CTR2-FT8-Sample.qk4ctr2map) demonstrates
Button 6 in Normal mode and the Home Wheel A dial. Importing a mapping replaces
the current mapping, so copy these assignments into an existing custom map when
other controls must be preserved.

## Understanding CTR2 modes

CTR2-MIDI has four knob modes: **Home**, **Knob mode 1**, **Knob mode 2**, and
**Knob mode 3**. Each mode has a normal **turn** control and a **push and turn**
control. These eight controls send CC100 through CC107 respectively.

CTR2 also has two settings whose similar names can be confusing:

- **Extended BTN Mode** changes the six front-panel buttons from 12 shared
  short/long functions to 48 mode-specific functions. The **Extended Button
  Mode** checkbox in QK4 must match this device setting.
- **Extended Paddle Mode** changes the rear paddle jack from left/right iambic
  paddle input to straight-key and PTT input. This is selected in QK4 with the
  **Paddles** or **Straight key + PTT** choice.

Changing one setting on CTR2 does not automatically change the other.

## Front-panel button notes

In Normal Button Mode, the same short/long assignments remain active in every
knob mode.

| Physical control | Short-press note | Long-press note | Knob mode |
|---|---:|---:|---|
| Button 1 | 1 | 11 | All knob modes |
| Button 2 | 2 | 12 | All knob modes |
| Button 3 | 3 | 13 | All knob modes |
| Button 4 | 4 | 14 | All knob modes |
| Button 5 | 5 | 15 | All knob modes |
| Button 6 | 6 | 16 | All knob modes |

In Extended Button Mode, each knob mode has its own six short-press and six
long-press assignments.

When Extended Button Mode is first enabled in QK4, the existing 12 shared
assignments are retained as the Home knob mode assignments. The newly exposed
Knob mode 1, 2, and 3 assignments start as **Disabled**; QK4 does not duplicate
the Home actions into those modes. Turning Extended Button Mode off retains the
Home assignments as the 12 shared Normal Button Mode assignments.

| Physical control | Home short/long | Mode 1 short/long | Mode 2 short/long | Mode 3 short/long |
|---|---:|---:|---:|---:|
| Button 1 | 1 / 25 | 7 / 31 | 13 / 37 | 19 / 43 |
| Button 2 | 2 / 26 | 8 / 32 | 14 / 38 | 20 / 44 |
| Button 3 | 3 / 27 | 9 / 33 | 15 / 39 | 21 / 45 |
| Button 4 | 4 / 28 | 10 / 34 | 16 / 40 | 22 / 46 |
| Button 5 | 5 / 29 | 11 / 35 | 17 / 41 | 23 / 47 |
| Button 6 | 6 / 30 | 12 / 36 | 18 / 42 | 24 / 48 |

QK4 executes front-panel button actions when the button is released, matching
CTR2-MIDI's positive-velocity NoteOn-on-release behavior. No NoteOff is needed;
NoteOff and zero-velocity NoteOn do not repeat a front-panel action. See the
[manufacturer's operation manual](https://ctr2.lynovation.com/wp-content/uploads/2026/02/CTR2-MIDI_Operation_Manual_v20100a.pdf),
page 17. CW/key/PTT inputs still use both MIDI edges.

## Knob output formats and directional Button notes

The output selected in QK4 must match the output programmed for that same knob
control in CTR2-MIDI:

- **Wheel A** is a relative, speed-sensitive value centered on 64. It is the
  normal choice for accelerated VFO tuning.
- **Wheel B** sends one directional step using values 1 and 126. Use
  **Wheel B reversed** if those directions are reversed for the selected app.
- **Slider A** and **Slider B** send absolute positions from 0 through 127.
  QK4 uses position changes as fine directional steps and establishes a new
  baseline when a device connects or a mapping changes, preventing jumps.
- **Button direction pair** sends one NoteOn number for counter-clockwise and
  another for clockwise instead of sending a CC value.

The published CTR2-MIDI manual shows knob Button notes 40–55 for Normal Button
Mode. Lynovation subsequently clarified that Extended BTN Mode moves the knob
Button range to 60–95 so it does not collide with the extended front-panel
buttons. The sequential assignments are also documented in Lynovation's
CTR2-Dial manual. CTR2-MIDI's eight knob controls use the first eight pairs:

| Knob control | CC | Normal CCW/CW notes | Extended CCW/CW notes |
|---|---:|---:|---:|
| Home turn | 100 | 40 / 41 | 60 / 61 |
| Home push and turn | 101 | 42 / 43 | 62 / 63 |
| Knob mode 1 turn | 102 | 44 / 45 | 64 / 65 |
| Knob mode 1 push and turn | 103 | 46 / 47 | 66 / 67 |
| Knob mode 2 turn | 104 | 48 / 49 | 68 / 69 |
| Knob mode 2 push and turn | 105 | 50 / 51 | 70 / 71 |
| Knob mode 3 turn | 106 | 52 / 53 | 72 / 73 |
| Knob mode 3 push and turn | 107 | 54 / 55 | 74 / 75 |

Notes 76–95 are assigned to CC108–CC117 in Lynovation's larger 18-control
CTR2-Dial table. CTR2-MIDI has only the eight CC100–CC107 knob controls, so it
uses notes 60–75.

## Paddle, straight-key, and PTT notes

Extended BTN Mode also relocates the paddle-jack notes so they remain separate
from the 48 front-panel buttons:

| QK4 keying selection | Extended Button Mode off | Extended Button Mode on |
|---|---:|---:|
| Paddles | Left 20, right 21 | Left 96, right 97 |
| Straight key + PTT | Tip 30, ring 31 | Tip 98, ring 99 |

**Swap tip/ring** reverses the two physical jack assignments. Straight-key and
external-keyer input sends the K4 raw key-down/key-up commands and generates a
local QK4 sidetone; K4 monitor audio is not required. Paddle input continues to
use QK4's local iambic keyer and its existing local sidetone.

## Editing a saved mapping file

QK4 saves complete, user-editable `.qk4ctr2map` JSON files. Each button entry
states the physical label, MIDI note, press type, knob mode, and mapped action:

```json
{
    "buttonLabel": "Button 3",
    "knobMode": "Knob mode 2",
    "note": 15,
    "pressType": "short",
    "action": "nr_toggle"
}
```

To use a custom K4 command, set `action` to `macro`, assign a macro id, and
define that id in the file's `macros` list:

```json
{
    "buttonLabel": "Button 3",
    "knobMode": "Knob mode 2",
    "note": 15,
    "pressType": "short",
    "action": "macro",
    "macro": "contest_message"
}
```

```json
{
    "id": "contest_message",
    "label": "Contest message",
    "command": "KY CQ TEST;"
}
```

Edit `action`, optional `macro`, and the corresponding macro definition. The
`buttonLabel`, `knobMode`, and `pressType` fields explain the note assignment;
QK4 derives the physical control from `note` when loading. Set the top-level
`buttonMode` to `normal` or `extended` to match CTR2-MIDI. Loading a file
replaces the complete current mapping rather than merging entries.

Every saved knob entry likewise identifies its control label, knob mode,
gesture, CC number, selected QK4 action, output format, and its Normal and
Extended directional notes if the knob is programmed for Button output. The
file's `_buttonActions`, `_knobActions`, `_knobOutputs`, and guide sections list
the supported keywords and explain their use.

### Understanding `_buttonActions` and `_knobActions`

All top-level names beginning with an underscore are documentation references;
QK4 ignores them when loading the file. The actual assignments are the entries
in the `buttons` and `knobs` arrays.

`_buttonActions` is divided into two named groups so these different behaviors
are not mixed together:

- Immediate actions such as `band_up`, `nr_toggle`, and `tx_rx_toggle` perform
  their function when the button is released.
- Adjustment selectors beginning with `adjust_` do not continuously adjust a
  setting themselves. They select what a knob will control and open the
  corresponding QK4 control or feedback where available.

Using adjustment selectors requires at least one knob whose action is
`selected_adjustment`. This is a behavioral relationship, not a direct pairing
between a particular MIDI note and CC:

| Actual entry | Action | Role |
|---|---|---|
| An entry in `buttons` | `adjust_nr_level` | Select NR when that button is released |
| An entry in `knobs` | `selected_adjustment` | Control whichever `adjust_*` function was selected most recently |

The entries belong in separate arrays and can appear anywhere in those arrays.
Their order and proximity in the file have no effect. This complete minimal
example makes Button 3 select NR and makes the Home knob control the selected
adjustment:

```json
{
    "knobs": [
        {
            "controlLabel": "Home turn",
            "cc": 100,
            "action": "selected_adjustment",
            "output": "wheelA"
        }
    ],
    "buttons": [
        {
            "buttonLabel": "Button 3",
            "note": 3,
            "pressType": "short",
            "action": "adjust_nr_level"
        }
    ]
}
```

The operator presses Button 3 to select NR and open its control, then turns the
Home knob to change the NR level. Pressing a different `adjust_*` button makes
that same knob control the newly selected function.

For a knob that should always control one setting without first pressing a
button, use a direct action from `_knobActions`. For example, assigning
`nr_level` directly to CC105 makes CC105 permanently control NR.

## Adjustments that open QK4 controls

| CTR2 adjustment | QK4 Mobile UI invoked |
|---|---|
| Attenuator | Attenuator adjustment overlay with the current level and enabled state |
| Noise blanker | NB Level overlay with the current level, enabled state, and filter setting |
| Noise reduction | NR Adjust overlay showing the LMS/SSNR selection, state, and level |
| Manual notch | Manual Notch overlay with its enabled state and pitch |
| Main volume | K4 Controls drawer positioned at Main Volume |
| Sub volume | K4 Controls drawer positioned at Sub Volume |
| RIT/XIT frequency | K4 Controls drawer with the RIT/XIT row visible; turning the knob enables RIT when necessary |
| Filter bandwidth | K4 Controls drawer positioned at Filter Bandwidth |
| Filter shift | K4 Controls drawer positioned at Filter Shift |
| Main RF gain | K4 Controls drawer positioned at Main RF Gain |
| Sub RF gain | K4 Controls drawer positioned at Sub RF Gain |
| Main squelch | K4 Controls drawer positioned at Main Squelch |
| Sub squelch | K4 Controls drawer positioned at Sub Squelch |
| RF power | K4 Controls drawer positioned at RF Power |
| CW speed | K4 Controls drawer positioned at CW Speed while operating in CW or CW-R |

The visible sliders, values, and radio indicators update as the CTR2 knob is
turned. Volume, filter, power, and CW-speed adjustments also show brief numeric
confirmation.

## Adjustments shown through the operating display

These adjustments do not open another menu because the affected information is
already visible. Selecting one through an **Adjust:** button still displays the
`CTR2 KNOB: ...` confirmation.

| CTR2 adjustment | Visible feedback |
|---|---|
| Active VFO frequency | The active VFO frequency and panadapter cursor update |
| Other VFO frequency | The other VFO frequency display updates |
| Panadapter zoom | The panadapter changes and a message shows the resulting span |
| Panadapter reference | The spectrum reference changes and a message shows the dBm value |
| Waterfall brightness | The waterfall changes and a message shows the WTR CLRS value |

## Immediate button actions

These predefined button actions perform their function immediately rather than
assigning the knob.

| Button action | QK4 Mobile response |
|---|---|
| Band Up / Band Down | Frequency, band, and panadapter update |
| Mode Next / Mode Previous | The VFO mode display updates |
| Main Mute | Volume controls update and `MAIN AUDIO MUTED` or `MAIN AUDIO RESTORED` appears |
| Attenuator Toggle | The ATT state and level indication update |
| Noise Blanker Toggle | The NB indication updates |
| NR Toggle | The NR/SSNR indication updates |
| Manual Notch Toggle | The normal QK4 notch state follows the K4 response |
| RIT Toggle | The RIT/XIT label and offset display update |
| Split Toggle | SPLIT and transmit-VFO indications update |
| TX/RX Toggle | The transmit UI changes and `TRANSMIT ON` or `RECEIVE` appears |
| Pan Zoom In / Pan Zoom Out | The panadapter changes and a message shows the new span |
| Rate | The VFO tuning-rate indication updates |
| KHZ | The K4 1 kHz rate is selected for VFO A, or VFO B while B-SET is active |
| TUNE | The K4 TUNE function is invoked and normal radio-state indications follow |
| Custom Command | The entered K4 command is sent and QK4 displays any resulting state it understands |
| Disabled | No action |

Custom commands do not automatically open a menu because QK4 deliberately does
not interpret or rewrite arbitrary K4 command sequences.

For shared A/B controls, the current **B-SET** state determines which receiver
is adjusted. Actions explicitly named Main or Sub always operate their named
receiver.

## References

- [CTR2-MIDI Operation Manual v2.01.01a](https://ctr2.lynovation.com/wp-content/uploads/2026/03/CTR2-MIDI_Operation_Manual_v20101a.pdf)
- [CTR2-Dial Firmware Manual v2.05.00](https://ctr2.lynovation.com/wp-content/uploads/2026/02/CTR2-Dial_Firmware_Manual_v20500.pdf)
- [Elecraft K4 Programmer's Reference](https://ftp.elecraft.com/K4/Manuals%20Downloads/K4ProgrammersReferencerev.D12.html)
- [Sample QK4 CTR2 mapping file](QK4-CTR2-Rate-KHZ-Sample.qk4ctr2map)

[Return to the QK4 Android README](../README.md)
