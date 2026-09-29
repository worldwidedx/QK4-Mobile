# Building QK4 Android on macOS and Linux

This is the macOS/Linux counterpart to [BUILD_ANDROID_WINDOWS.md](BUILD_ANDROID_WINDOWS.md).
`build-android.sh` mirrors `build-android.ps1` exactly — same actions
(`doctor`, `configure`, `build`, `apk`, `install`), same build tree
(`build-android-arm64`), same environment-override contract. Use it as the
canonical macOS/Linux entrypoint; do not substitute machine-specific
CMake/Ninja commands.

Everything below is host-agnostic except a few paths and package sources, which
are called out as **macOS** / **Linux** where they differ.

## Required components

Install through the Qt Maintenance Tool or [`aqt`](https://github.com/miurahr/aqtinstall):

- Qt 6.11.1
- Android ARM64 kit (`android_arm64_v8a`)
- A matching desktop host kit — **macOS:** `macos`; **Linux:** `gcc_64`
- CMake and Ninja

Provide the Android toolchain via Android Studio or standalone command-line tools:

- Android SDK platform tools
- Android API 34 platform
- Android build tools
- Android NDK 27.x
- A compatible JDK (Android Studio's bundled `jbr`, or any JDK on `JAVA_HOME`)

The script defaults to Android API 26 as the minimum runtime and builds `arm64-v8a`.

### Two host-kit gotchas

1. **Qt Shader Tools (`qsb`).** The `android_arm64_v8a` kit may not ship the
   `qsb` host tool that `qt6_add_shaders` needs for the panadapter shaders. The
   script auto-falls back to a same-version `Qt6ShaderTools` from:
   - **macOS:** a Homebrew Qt (`brew install qt`) or the installed `ios` kit.
   - **Linux:** a distro Qt with the Shader Tools module, or install it with
     `aqt ... -m qtshadertools` into the kit.

   If neither is found it stops with a clear message.

2. **Qt SerialPort for Android.** The serial keyer path needs
   `Qt6::SerialPort`, and a fresh `android_arm64_v8a` kit often omits it. Add it
   once with `aqt` (use the host matching your machine — `mac` or `linux`):

   ```bash
   # macOS
   aqt install-qt mac  android 6.11.1 android_arm64_v8a -m qtserialport --outputdir ~/Qt
   # Linux
   aqt install-qt linux android 6.11.1 android_arm64_v8a -m qtserialport --outputdir ~/Qt
   ```

   SerialPort is required off-iOS; iOS is the only platform where it is gated out.

## Automatic path discovery

`build-android.sh` checks environment overrides first, then common locations:

- Qt Android kit: `QK4_QT_ANDROID`, else the `android_arm64_v8a` kit next to the newest `~/Qt/<version>`
- Qt host kit: `QK4_QT_HOST`, else the matching `macos` / `gcc_64` / `clang_64` kit
- Android SDK: `ANDROID_SDK_ROOT`, `ANDROID_HOME`, then common defaults — **macOS:** `/usr/local/share/android-commandlinetools`, `~/Library/Android/sdk`; **Linux:** `~/Android/Sdk`
- Android NDK: `ANDROID_NDK_ROOT`, else the newest NDK under the SDK
- JDK: `QK4_JAVA_HOME`, `JAVA_HOME`, else (**macOS**) `/usr/libexec/java_home`. On **Linux** set `JAVA_HOME` (or `QK4_JAVA_HOME`).
- Opus: `QK4_OPUS_ROOT`, else `third_party/android/opus`
- Optional: `QK4_CMAKE`, `QK4_NINJA`

It stops with a descriptive error if a required path is missing.

## Commands

Verify the toolchain without touching the build tree:

```bash
./build-android.sh doctor
```

Configure a fresh build tree:

```bash
./build-android.sh configure
```

Compile the application library:

```bash
./build-android.sh build
```

Generate a debug-signed APK:

```bash
./build-android.sh apk
```

Install or upgrade on the connected device:

```bash
./build-android.sh install
```

For multiple connected devices, pass the adb serial:

```bash
./build-android.sh install Debug <adb-serial>
```

## Release builds

Generate a release-signed APK in a separate build tree
(`build-android-arm64-release`). Set the keystore variables for the current
shell session only — never persist passwords:

```bash
export QT_ANDROID_KEYSTORE_PATH="/secure-location/qk4-mobile-release.p12"
export QT_ANDROID_KEYSTORE_ALIAS="qk4mobile"
export QT_ANDROID_KEYSTORE_STORE_PASS="<stored securely>"
export QT_ANDROID_KEYSTORE_KEY_PASS="<stored securely>"
./build-android.sh apk Release
```

A release-signed APK cannot update a debug-signed install of the same package;
uninstall the debug build first. Never commit the keystore or its passwords.

## Notes

- The generated `build-android-arm64` tree is intentionally excluded from Git.
- Enable Developer Options and USB debugging on the tablet/phone before
  `install`.
- Launch after install: `adb -s <serial> shell am start -n com.w9wdx.qk4phone/.Qk4Activity`.
