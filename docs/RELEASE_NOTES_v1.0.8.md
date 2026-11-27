# QK4 Mobile v1.0.8 for Android

This update improves timing for remote CW sending with MIDI paddles, adds an
adjustable timing option for TinyMIDI’s built-in touch paddles, and clarifies
how to record CW and data messages from the tablet console.

- **Improved CW paddle timing.** QK4 Mobile uses the original timing of paddle
  presses and releases when Android delivers MIDI events in batches. This helps
  preserve element spacing and reduces unintended extra dits or dahs during
  quick paddle taps.
- **Optional TinyMIDI touch timing.** The new **TINYMIDI TOUCH** control in
  CW Keyer settings can help prevent a slightly long tap from sending an
  unwanted repeat. It’s off by default and adjustable in 5 ms steps up to
  100 ms. Leave it off when using TinyMIDI’s external paddle jack.
- **Clearer REC feedback on tablets.** In CW and data modes, tapping **REC**
  opens the K4’s message editor and prompts you to enter the text on the radio.
- **Opt-in CAT diagnostics.** CAT tracing can be enabled for troubleshooting
  radio-control issues. It’s off by default.

The release package is for ARM64 Android devices on Android 8.0 or later
(`com.w9wdx.qk4phone`, version code 36). Physical CW verification of the
final timing changes at higher speeds remains pending.
