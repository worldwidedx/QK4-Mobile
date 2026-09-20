---
name: qk4-android
description: Maintain and extend the QK4 Android ARM64 phone application, including Elecraft K4 TCP/TLS control, RX/TX audio streaming, radio state, panadapter/waterfall rendering, touch-first UX, Android builds, and connected-device testing. Use for changes, diagnosis, builds, documentation, or reviews in the QK4 Android repository.
---

# QK4 Android

## Begin every task

1. Read the repository `AGENTS.md`.
2. Read `docs/PROJECT_STATUS.md`.
3. Inspect the relevant current source and upstream-compatible behavior before editing.
4. Preserve radio, audio, and protocol plumbing unless the task explicitly requires a verified correction.

## Choose the reference

- For connection, CAT commands, state synchronization, or audio, read [references/architecture.md](references/architecture.md).
- For builds and device validation, read [references/testing.md](references/testing.md).
- For current priorities and accepted behavior, treat `docs/PROJECT_STATUS.md` as authoritative.

## Work safely

- Make source changes independently from build/install actions.
- Build when verification is appropriate; install only when the user asks.
- Exclude radio credentials, signing files, APKs, build trees, logs, and screen captures from commits.
- Report what was verified on a real K4 separately from what was inferred through source inspection.
- Treat Android `TYPE_HEARING_AID` as an RX-only media output. Preserve the
  existing device-change rebuild path, and never add a hearing aid to the TX
  communication-device or microphone-selection paths unless a supported
  two-way endpoint is proven on hardware.
- Until physical tablet validation is available, keep the compact phone layout
  enabled for every screen size. The original tablet-selection branch is
  intentionally commented in `src/ui/k4styles.cpp`; preserve it for later
  restoration rather than deleting or redesigning it without user direction.
