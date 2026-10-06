# QK4 Mobile v1.0.6.1

This Android ARM64 patch release restores the Elecraft K4 Average setting on
its panadapter. The app queries the K4 value when connecting and after macros,
applies it to the main spectrum, waterfall, and both VFO mini pans, and reads
back changes made in the app.

## Validation and scope

- Android version code: **34**; package: `com.w9wdx.qk4phone`.
- The owner confirmed the correction on a Samsung Galaxy S26 Ultra using a
  v1.0.6 test app with the same Average code. The exact v1.0.6.1 release APK
  was not separately radio-tested.
- Focused state, renderer, and fake-K4 regression tests passed. PR #23 passed
  policy, native, Android build, iOS compile, and aggregate required CI checks
  before the version bump. The release commit's checks are recorded on that PR.
- The same C++ source is used by Android phones/tablets and iPhone/iPad.
  Physical tablet and iOS acceptance for this correction remain pending.
  This release publishes only the ARM64 Android APK.
- No layout, orientation, connection, audio, or transmit behavior changes are
  included.

The panadapter smoothing behavior was adapted from upstream QK4 commit
`d75ed494c7f2a962896ada92b9635d61ad651a50` by Mike Garcia, KF5O.
