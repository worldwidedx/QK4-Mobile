# Phone interaction contract

These requirements apply equally to Android phones and iPhones. They complement
the [orientation policy](ORIENTATION_POLICY.md), which owns rotation and
device/window classification rules. The established v1.0.5 phone workflows are
the migration reference; approved focused changes may improve them.

- Keep live VFO displays, meters, panadapter, and deliberate PTT immediately
  reachable in the radio console. Required controls must never be off-canvas.
- Support touch without a mouse wheel, hover, physical keyboard, or right click.
- On a dual-line control, a normal tap anywhere invokes its primary action;
  deliberate long press invokes the secondary action. Do not infer an alternate
  action merely from which text line the finger hits.
- On every scrollable touch page, treat a child-control press as provisional
  until the gesture is classified. Crossing the vertical drag threshold cancels
  the pending action and suppresses release/click, including when dragging starts
  on a button, checkbox, selector, or editor row. Reuse the proven QScroller
  press-delay and drag-cancellation pattern; a plain QScrollArea with immediately
  active children is insufficient. Long-press timers must also cancel on drag.
- Use the established in-window, touch-scrollable selectors on scrollable
  setup/editor pages. On Android, do not introduce native QComboBox popup windows
  whose repeated creation/destruction adds separate Android/EGL surfaces. iOS
  adapters must preserve the same touch, cancellation, and selection contract.
- Direction-lock sliders so a vertical page scroll does not change their value.
- Show feedback above the active popup when the main console cannot show the
  changed state. Keyboard opening/dismissal and safe-area changes must preserve
  reachable confirmation, cancellation, and return controls.
- Preserve the established CW Keyer, radio-control, audio, and saved-settings
  workflows. A tablet layout or platform port does not authorize removing or
  rearranging phone features. Document each deliberate phone change separately.
- Keep local panadapter functions separate from radio CAT state; no unsolicited
  operator-setting commands on connection. PTT is deliberate transmit state,
  never VOX emulation. Hearing aids remain RX-only where exposed that way.

Review shared styles, widget initialization, signal wiring, painting, and event
handling as well as layout conditionals. An `isCompactLayout()` gate elsewhere
in a file does not prove the entire change preserves phones. See the
[validation matrix](VALIDATION.md) for tests and required device evidence.
