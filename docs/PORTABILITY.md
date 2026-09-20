# Moving QK4 Android to another PC

This repository is the portable source-of-truth for QK4 Android. It includes the application source, Android project files, build wrappers, repository-local Codex skill, project instructions, and the ARM64 Opus development library required by the current Android build.

## What to transfer

Preferred method: clone the private repository.

```powershell
git clone https://github.com/worldwidedx/QK4-Android.git
cd QK4-Android
```

Then install the build prerequisites described in [BUILD_ANDROID_WINDOWS.md](BUILD_ANDROID_WINDOWS.md), and run:

```powershell
.\build-android.cmd -Action Doctor
.\build-android.cmd -Action Apk
```

`Doctor` only verifies discovery of the required paths. It does not configure, compile, package, or install the app.

## Required on the destination PC

- Windows with PowerShell.
- Qt 6.11.1: Android ARM64 and matching MinGW 64-bit host kits, CMake, and Ninja.
- Android SDK with platform tools, API 34 platform/build tools, and NDK 27.x.
- Android Studio's bundled JBR or another compatible JDK.
- A USB-connected Android phone with developer options and USB debugging enabled only when installing with `-Action Install`.

The build script detects the normal Qt, Android SDK, Android Studio, and Java locations. For nonstandard installs, use the environment overrides documented in [BUILD_ANDROID_WINDOWS.md](BUILD_ANDROID_WINDOWS.md).

## Included repository guidance

- `README.md` — project overview, provenance, security rules, and quick build.
- `AGENTS.md` — constraints for any developer or coding agent working on the app.
- `docs/PROJECT_STATUS.md` — current verified behavior and outstanding work.
- `docs/BUILD_ANDROID_WINDOWS.md` — complete Windows build instructions.
- `.codex/skills/qk4-android/` — repository-local Codex skill and focused references.
- `build-android.cmd` / `build-android.ps1` — portable build, APK, and install entry points.

## Deliberately not included

The following stay local and must never be copied into Git:

- Radio connection profiles, usernames, passwords, or captured logs containing them.
- Android keystores, aliases, and signing passwords.
- APK/AAB files and build directories (rebuild them on the destination PC).
- Phone screenshots, screen recordings, UI dumps, and debugging captures.
- Personal IDE configuration and private notes.

For a signed distribution build, securely transfer the PKCS#12 signing key
outside Git and supply its values through the documented Qt Android signing
variables. Keep the keystore and passphrase in separate protected backups: the
same certificate is required for every direct-APK update. Use
`build-android.cmd -Action Apk -DeploymentType Release`; do not copy a keystore
into the repository.
