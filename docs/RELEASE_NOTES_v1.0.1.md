# QK4 Mobile v1.0.1

QK4 Mobile v1.0.1 adds K4-style reverse operation for digital modes and
substantially refines SSTV image and template editing.

## New Digital-Mode Feature

### Digital-mode REV operation

- Added reverse operation for DATA, AFSK, FSK, and PSK.
- Tapping the currently selected digital-mode button again toggles normal and
  reverse operation.
- The affected VFO displays `DATA-R`, `AFSK-R`, `FSK-R`, or `PSK-R`.
- The selected digital submode remains unchanged when REV is toggled.
- Main VFO and B SET operation use the appropriate K4 `MD` and `DT` commands.
- Panadapter and mini-pan passbands, secondary-VFO passbands, frequency
  direction, and manual-notch placement follow the correct LSB-like
  orientation in reverse modes.

## SSTV Refinements

### Image and template editing

- Expanded both SSTV editors to support selection and continued editing of
  text, freehand drawing, lines, arrows, rectangles, and ellipses.
- Selected objects can be moved, deleted, recolored, resized, and rotated in
  45-degree increments.
- Added true undo and redo actions with conventional mirrored icons.
- Standardized rotate controls and behavior across both editors.
- Added stretchable rectangles and circular or elliptical shapes.
- Added independent outline and fill colors for shapes.
- Added adjustable outlines for text, freehand drawing, lines, and arrows.
- Refined text-outline rendering to follow the exterior of complete
  characters.
- Refined arrow drawing so the arrowhead appears at the finger-release
  endpoint.
- Size changes apply immediately to the selected text, line, drawing, arrow,
  or shape.
- Expanded the shared text-size range to 12-112 pixels in both editors.
- Expanded available font and color choices.
- Added direct `{MY_CALL}` and `{TO_CALL}` insertion controls.
- Callsign variables remain dynamic in saved templates and resolve from the
  current MY CALL and TO CALL fields.
- Transmission is held until every required callsign variable has a value.
- Improved pinch-to-zoom interaction alongside the zoom slider.
- Refined portrait and landscape template-editor layouts to preserve a larger
  image workspace.
- Reorganized editing controls into consistent, compact rows with improved
  spacing, alignment, sizing, and labeling.
- Retained compatibility with existing version-1 and version-2 SSTV templates.

## Validation

- Release-signed ARM64 build installed and tested successfully on the Samsung
  Galaxy S26 Ultra.
- Digital-mode REV and SSTV editing workflows passed device testing.
- All 16 focused SSTV composer regression tests passed.

## Version and Compatibility

- Version: **1.0.1**
- Android version code: **27**
- Package: `com.w9wdx.qk4phone`
- ABI: ARM64 (`arm64-v8a`)
- Minimum Android version: Android 8.0 / API 26
