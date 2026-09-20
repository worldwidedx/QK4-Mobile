# QK4 Mobile Android development instructions

## Current release and scope

- Current release: **QK4 Mobile v1.0.4.1** (`com.w9wdx.qk4phone`).
- Product target: ARM64 Android touch devices in landscape orientation, API 26+.
- Physical acceptance testing: Samsung Galaxy S26 Ultra. Other Android phones
  are expected to use the compact layout but remain a validation priority.
- Upstream lineage: QK4 by Mike Garcia, KF5O. QK4 Mobile is a WorldwideDX.com
  derivative under GPL-3.0-or-later.

## Start every task

1. Read `README.md`, `docs/PROJECT_STATUS.md`, and the relevant source.
2. For Android builds, read `docs/BUILD_ANDROID_WINDOWS.md`.
3. Preserve existing QK4 protocol, CAT, TCP/TLS, RX/TX audio, and state-sync
   methods unless a correction is verified against upstream QK4 and the K4.

## Product and UX invariants

- The live VFO display, meters, panadapter, and PTT stay immediately reachable.
- Design for touch: no mouse wheel, hover, keyboard, or right-click dependency.
- Use deliberate long press for a secondary action on a dual-line control; a
  normal tap anywhere on that control invokes its primary action.
- Distinguish scrolling from tapping. Required controls must remain reachable
  through scrolling or a touch panel—never off-canvas.
- Provide visible feedback when a state change is not otherwise visible on the
  main console. Feedback must appear above, not behind, an active popup.
- Keep local-rendered panadapter functions separate from radio CAT state.
- Do not issue operator-setting commands merely as a connection side effect.
- PTT represents deliberate transmit state; never emulate PTT using VOX.
- Android hearing aids are an RX-only output route when Android exposes them as
  `TYPE_HEARING_AID`; do not add them to TX communication-device selection.
- Until tablet-specific validation is available, retain the compact phone layout
  override for all display sizes. The original tablet-selection logic is
  intentionally commented in `src/ui/k4styles.cpp`; do not remove it or
  re-enable it without a tested tablet UX plan.
- FT8/FT4 is portrait-only on phone-class devices. Landscape FT8/FT4 support is
  an accepted tablet-specific contribution when it is gated by the shared
  tablet classification, physically validated on a tablet, and leaves phone
  behavior portrait-only. Follow [the screen orientation policy](docs/ORIENTATION_POLICY.md).

## Current verified functionality

v0.7.1 has been device-tested for radio connection, RX/TX audio, touch tuning,
VFO tuning-step synchronization, direct frequency entry, core menus, CW text
decode, F1–F8 macros, RIT/XIT jog, local Peak Hold, and local WTR CLRS.
The v0.7.1 Gboard case-handling correction still requires device validation.
See `docs/PROJECT_STATUS.md` for boundaries and pending validation.

## Build, install, and release rules

- README updates are additive. Preserve all existing screenshots, established
  feature sections, and useful operator documentation unless the user explicitly
  requests removal; add new release material in the appropriate sections.
- Public README and release-note text describes the finished functionality.
  Omit intermediate refinement history unless it materially affects operators.
- Use `build-android.cmd`; do not replace it with machine-specific commands.
- Build independently from source changes. Install only when the user asks.
- Debug: `build-android.cmd -Action Apk`.
- Release: `build-android.cmd -Action Apk -DeploymentType Release`, with the
  documented signing environment variables set only for that shell session.
- Never commit profiles, credentials, keystores, signing passwords, APK/AABs,
  build trees, logs, or device captures. The release keystore is outside Git.
- A release-signed APK cannot update an installed debug-signed APK with the
  same package name; Android requires uninstalling the debug app first.
- Do not claim radio behavior is fixed without device evidence.
