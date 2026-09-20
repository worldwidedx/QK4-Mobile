# QK4 Mobile v1.0.2

QK4 Mobile v1.0.2 expands SSTV template capabilities and improves Android
stability for SSTV editing and TinyMIDI CW operation.

## New features

### Complete SSTV templates

- SSTV transmit templates can now optionally include a background image.
- Images can be selected directly from the Android gallery while creating or
  editing a template.
- Image-enabled templates preserve the picture, crop, zoom, position, text,
  and markup.
- Applying an image-enabled template automatically replaces the current TX
  image and restores the complete composition.
- Layout-only templates remain available for applying text and markup over the
  image currently selected for transmission.
- Image-enabled templates are identified with an `IMAGE` label in the template
  selector.
- Built-in CQ, REPORT, and 73 templates can now be customized and saved
  directly under their original names.
- `RESET TEMPLATES` restores the original factory versions of the built-in
  templates.

## Fixes & refinements

### SSTV template workflow

- Selecting a template and tapping `USE` now applies the complete template
  immediately without requiring a redundant checkbox or second action.
- `SAVE TEMPLATE` in the template editor now completes the save operation
  directly.
- Simplified the main template controls by removing the redundant Apply
  button.
- Improved template image framing with independent pan, pinch zoom, slider
  zoom, and centering inside the template editor.
- Improved the visibility of image-inclusion checkboxes.
- Deleting or resetting a template now also removes its associated stored
  image.
- Enlarged the `CLEAR RX HISTORY` confirmation dialog to match other SSTV
  dialogs.

### SSTV selector stability

- Replaced Android's separate native popup windows for SSTV mode, font,
  template, and retention selectors with touch-scrollable in-window selection
  panels.
- Prevents the Android EGL-surface crash that could occur when opening or
  dismissing the font or template selector.
- Provides consistent sizing, scrolling, selection highlighting, and
  Cancel/Use actions.

### TinyMIDI CW stability

- Corrected a crash that could occur after connecting a TinyMIDI device and
  using the test dit/dah controls.
- Hardened local sidetone audio handling against Android audio-route and
  lifecycle changes.
- Automatically rebuilds the sidetone output if Android invalidates or stops
  the underlying audio device.

## Validation

- TinyMIDI connection and test dit/dah operation passed on the physical
  Android device.
- Complete SSTV templates with stored images were tested successfully.
- The release-signed ARM64 APK was built, signature-verified, and installed in
  place on the Samsung Galaxy S26 Ultra.

## Version and compatibility

- Version: **1.0.2**
- Android version code: **28**
- Package: `com.w9wdx.qk4phone`
- ABI: ARM64 (`arm64-v8a`)
- Minimum Android version: Android 8.0 / API 26
