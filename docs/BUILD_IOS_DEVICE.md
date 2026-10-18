# Build and run on a physical iPhone or iPad

The CI project under `build-ios-ci` is intentionally unsigned and cannot be
installed on a device. Generate a separate signing-enabled Xcode project for
local device testing instead. Xcode manages the Apple developer certificate
and provisioning profile; do not put signing credentials in the repository or
shell scripts.

## Generate the device project

From the repository root on macOS, set the matching Qt 6.11.1 iOS and macOS
host kit paths, then run the generator:

```bash
export QK4_QT_IOS="$HOME/Qt/6.11.1/ios"
export QK4_QT_HOST="$HOME/Qt/6.11.1/macos"
scripts/build-ios-device.sh
open build-ios-device/QK4.xcodeproj
```

The script targets ARM64 `iphoneos`, enables Xcode automatic code signing, and
uses `build-ios-device/`, separate from the unsigned CI build. If the default
bundle identifier is not registered to your team, supply an identifier you
control before running the script:

```bash
export QK4_IOS_BUNDLE_ID="com.example.qk4mobile"
```

The selected bundle identifier must match the App ID allowed by your Apple
developer team and provisioning profile.

## Sign and run from Xcode

1. Connect and unlock the iPhone or iPad, and accept its Trust prompt if shown.
2. In Xcode, select the **QK4** project, then the **QK4** app target.
3. Under **Signing & Capabilities**, enable **Automatically manage signing**
   and select your Apple developer **Team**.
4. Select the connected physical device in the run-destination menu. It must
   appear under **Devices**, not under **Simulators**.
5. Click **Run**. If iOS asks you to trust the developer, follow its on-screen
   instructions.

To change Qt kits, bundle identifier, or other CMake options, rerun the
generator and reopen the project. The generator clears cached global signing
settings from earlier attempts; signing is configured only on the app target.
Xcode may ask you to select the Team again after CMake regenerates project
files.

This local device build does not replace `scripts/ci/build-ios.sh`, which stays
an unsigned CI compile. A successful build is not physical acceptance; record
device, OS version, commit, and results in the relevant PR or validation
report.
