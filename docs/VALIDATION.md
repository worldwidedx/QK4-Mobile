# Validation and acceptance

## Change-based matrix

| Change | Automated checks | Physical acceptance before enabling/releasing affected behavior |
|---|---|---|
| Documentation only | Whitespace, links, consistency; repository CI still runs | None; do not invent device results |
| Shared widgets/layout/gesture code | Native regressions, Android build, iOS compile when present; focused behavior regressions | Affected compact/regular paths on Android and iOS once implemented; phone tap/hold/scroll, reachability, keyboard, orientation, return paths |
| Tablet layout activation | Above plus classification and constrained-window checks | Named Android tablet and/or iPad for each claimed platform; unaffected phone checks |
| Native platform service | Platform build and applicable regressions | Affected platform's permissions, native inputs/outputs, route changes, interruptions, background/resume |
| Shared connection/TLS/audio/PTT | Native transport/state/transmit regressions; both platform builds | Android/K4 baseline plus new backend: plain TCP where supported, TLS/PSK, authentication failure, reconnect, RX, deliberate TX/PTT and return to RX; relevant route/interruption behavior |
| Approved phone presentation change | Focused regression plus required builds | Affected Android phone/iPhone workflows against the approved before/after behavior |

An unavailable platform is pending, not passed. Additive dormant code may be
reviewed before device acceptance, but do not activate or claim support without
the applicable evidence. Shared changes to already enabled behavior require its
regression acceptance. Use the [orientation checks](ORIENTATION_POLICY.md) and
[phone interaction contract](PHONE_UX_CONTRACT.md), not screenshot similarity alone.

## Record template

For each acceptance session, record in the PR or a linked text report:

- Date, tester, exact commit/build version, artifact SHA-256, signing type.
- Device model, OS version, platform/device class, and relevant K4 configuration
  without credentials or private network details.
- Module, entry/exit path, enabled feature flags, orientations, usable window
  configuration, keyboard/insets, peripherals and audio routes exercised.
- Each check's pass/fail/pending result and known limitations; separate simulator,
  desktop, and physical tests. Do not extrapolate to other devices or backends.
- Whether later changes require rerunning the affected checks.

Summarize results in [project status](PROJECT_STATUS.md#device-and-orientation-validation).
Do not commit logs, private profiles, captures, signing keys, or credentials.
Installation and live transmit testing require the operator's authorization;
builds alone do not authorize either.

## Automated entry points

- Windows native suite: `test-windows.cmd -Action Test`.
- Android debug package: `build-android.cmd -Action Apk`.
- iOS compile (once platform sources are integrated):
  `scripts/ci/build-ios.sh`, with matching Qt host/iOS kits on macOS. It builds
  an unsigned ARM64 device target; it is not an install or TestFlight upload.
- Policy checks: `scripts/ci/check-policy.ps1`.

CI deliberately does not use signing secrets or contact a radio. It proves
build/regression results, not physical usability or over-the-air correctness.
The optional native Opus round-trip target requires `QK4_TEST_OPUS_SOURCE`;
it is not part of the default CI suite and must be recorded separately when used.
