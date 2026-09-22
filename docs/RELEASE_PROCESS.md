# Release, experimental support, and recovery

## Experimental support

Production defaults preserve the accepted phone behavior. Current `main` forces
compact layout. Tablet PRs must not replace this with automatic activation of an
unvalidated layout. New experimental selectors must be opt-in, default OFF,
and compiled only into explicitly enabled development builds; production builds
must reject attempts to enable development-only overrides.

The regular-layout contribution currently proposes runtime
`QK4_FORCE_COMPACT_UI` / `QK4_FORCE_REGULAR_UI` variables. These are not a production
safety boundary by themselves. Before accepting that activation, the PR must
provide the development-build gate, retain the compact production default, and
test that release configuration cannot honor a force-regular override.
An explicit maintainer-approved activation PR may change the production default
for a validated device/window class, with phone regressions covered.

iOS integration is experimental until physical device acceptance is recorded.
An unsigned CI compile or simulator launch does not enable an iPhone/iPad release.
Do not enable live transmit or alter radio configuration as a layout experiment.

## Release checklist

1. Select an exact accepted `main` commit. Resolve required checks and relevant
   [device acceptance](VALIDATION.md), including enabled flags and limitations.
2. Use a short-lived `release/<version>` branch only when stabilization needs it;
   apply the same review/check protection before use. Keep new feature work on
   `main`. Fix on `main` first and backport with provenance where practical;
   otherwise return a release fix to `main` immediately through a PR.
3. Record the common product version and monotonically increasing platform build
   numbers (Android version code; iOS bundle build number). Platform releases may
   ship at different times; each must name its exact source commit and support matrix.
4. Build through the supported scripts and documented toolchain versions. Sign
   only in the release environment using external credentials. CI debug APKs
   are test artifacts, never production releases. Preserve package/bundle identity.
5. Record source SHA, toolchain versions, dependency versions/checksums and source
   provenance, flags, artifact SHA-256, and signing identity in release notes or
   a release manifest. Verify the signature and tested artifact identity.
6. Create an immutable annotated product tag such as `v1.0.6` for the accepted
   source. Platform-only follow-ups require a new tag/build identifier; never
   move a published tag. Attach APK/AAB/iOS distribution artifacts to release
   storage, not Git. iOS publishing requires its own documented signing and
   App Store/TestFlight procedure before the first upload.
7. Publish only the device/platform support actually validated. Update project
   status and operator release notes. Retire a release branch only after all
   fixes are on `main`, the release tag exists, and no open work depends on it.

## Regression recovery

- Stop promotion of the affected candidate; preserve the known-good release.
- Identify the offending focused PR. Prepare a revert on current `main` (for a
  merge commit, select the correct mainline), check its dependencies, and run
  relevant CI/device regressions. Do not reset history or restore an entire
  obsolete Android/iOS branch.
- Ship a corrective version with a higher platform build number when needed.
  Do not assume Android/iOS permits in-place downgrade or compatible app data.
  Never uninstall, clear data, rotate signing identity, or install a rollback
  on the user's device without explicit authorization.
- Record the cause and add a regression test where meaningful. Keep the PR,
  accepted tag, and artifact identities sufficient to reproduce the result.
