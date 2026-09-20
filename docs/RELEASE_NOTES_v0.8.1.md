# QK4 Mobile v0.8.1

QK4 Mobile v0.8.1 expands the external CW keyer setup to support standard
USB MIDI paddle interfaces alongside the existing Bluetooth LE MIDI path.

## CW keyer MIDI discovery

- The CW Keyer screen no longer starts an eight-second BLE scan merely because
  the screen was opened.
- **SCAN** now enumerates attached USB MIDI devices immediately while also
  discovering nearby BLE MIDI devices.
- USB and BLE devices appear together in the device selector with their
  transport clearly identified.
- The selected transport is stored with the device selection, so a remembered
  USB device reconnects through Android USB MIDI rather than being treated as
  Bluetooth.
- USB selections use the device's USB identity properties instead of Android's
  temporary MIDI service ID, which changes when a device is unplugged.
- Previously saved BLE device addresses remain compatible.

## HaliKey MIDI

- HaliKey MIDI is supported through its class-compliant USB MIDI interface.
- The built-in HaliKey profile retains the documented MIDI note assignments:
  note 20 for the left/DIT paddle and note 21 for the right/DAH paddle.
- TinyMIDI, HaliKey MIDI, and custom learned mappings remain selectable
  independently of whether the device transport is USB or BLE.

## Version and compatibility

- Version: **0.8.1**
- Android version code: **23**
- Package: `com.ai5qk.qk4phone`
- ABI: ARM64 (`arm64-v8a`)
- Minimum Android version: Android 8.0 / API 26

The release-signed APK was built successfully, signature-verified, installed
as an in-place update over v0.8.0 on the Samsung Galaxy S26 Ultra, and launched
with existing application data preserved. Physical input testing with an
attached HaliKey MIDI remains dependent on access to that device.
