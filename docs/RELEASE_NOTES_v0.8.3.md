# QK4 Mobile v0.8.3

QK4 Mobile v0.8.3 improves transverter frequency display and simplifies
direct VFO frequency entry.

## Improvements

- Fixed transverter frequencies losing their leading digit. Frequencies such
  as `144.200.000` now display correctly instead of appearing as
  `44.200.000`.
- Expanded the VFO display to support higher transverter frequencies,
  including values such as `1.296.000.000`.
- Simplified direct frequency entry for both VFO A and VFO B. Trailing zeros
  are now implied:
  - `7.2` tunes to `7.200.000`.
  - `7.215` tunes to `7.215.000`.
  - `144.2` tunes to `144.200.000`.
- Full grouped input such as `7.215.000` remains supported.
- Raw frequency entry in hertz remains supported.
- Updated frequency-entry instructions and validation messages.

## Version and compatibility

- Version: **0.8.3**
- Android version code: **25**
- Package: `com.ai5qk.qk4phone`
- ABI: ARM64 (`arm64-v8a`)
- Minimum Android version: Android 8.0 / API 26

The release-signed APK was signature-verified and installed as an in-place
update on the Samsung Galaxy S26 Ultra with existing application data
preserved. Automated parser and display-format tests passed. Final operating
validation with the K4 and a configured transverter remains pending.
