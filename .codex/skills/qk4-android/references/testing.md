# Build and test reference

## Windows build

Use the repository script:

```powershell
.\build-android.cmd -Action Configure
.\build-android.cmd -Action Build
.\build-android.cmd -Action Apk
```

Run desktop unit tests through the environment-safe repository wrapper:

```powershell
.\test-windows.cmd -Action Doctor
.\test-windows.cmd -Action Test
```

Do not invoke the Qt MinGW compiler from an unprepared shell. The wrapper
temporarily supplies the Qt and MinGW runtime DLL paths needed by `cc1plus.exe`
without changing the user's global `PATH`.

Read `docs/BUILD_ANDROID_WINDOWS.md` when dependency discovery fails.

## Device installation

Install only when requested:

```powershell
.\build-android.cmd -Action Install
```

Use `adb devices` first when more than one device may be attached.

## Validation levels

1. Source inspection: confirm signal/slot wiring, state ownership, and command selection.
2. Build: compile the Android ARM64 target and package an APK.
3. Visual test: inspect the landscape phone UI for clipping, unreachable controls, and gesture conflicts.
4. Radio test: verify commands, RX audio, TX audio, and resulting K4 state on actual hardware.

Do not promote a result from one level as proof of the next.

## Tablet-layout interim policy

QK4 Mobile v0.7.6.2 deliberately uses the compact landscape phone layout on
all Android screen sizes. This is an interim fallback while physical tablet
testing is unavailable. The original screen-size selection code is preserved as
comments in `src/ui/k4styles.cpp`; do not remove it. Restoring or replacing the
tablet layout requires a visual test on a real tablet before release.

## Hearing-aid RX validation

Android hearing aids may be exposed as `TYPE_HEARING_AID` rather than a normal
Bluetooth headset. For QK4 Mobile v0.7.6.3, verify that normal media and QK4
RX both reach the aids after they are paired through Android's hearing-device
settings. Verify that reconnecting the aids rebuilds RX playback. TX remains on
the phone microphone unless Android separately reports a supported two-way
input endpoint.
