package com.w9wdx.qk4phone;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanFilter;
import android.bluetooth.le.ScanResult;
import android.bluetooth.le.ScanSettings;
import android.content.Context;
import android.content.pm.PackageManager;
import android.hardware.usb.UsbDevice;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiManager;
import android.media.midi.MidiOutputPort;
import android.media.midi.MidiReceiver;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelUuid;
import android.os.SystemClock;
import android.util.Base64;
import android.util.Log;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ConcurrentLinkedQueue;

/** Android BLE/USB MIDI discovery and two independent input sessions. */
public final class AndroidBleMidi {
    private static final String TAG = "QK4-Midi";
    private static final UUID MIDI_SERVICE = UUID.fromString("03b80e5a-ede8-4b33-a751-6ce34ec4c700");
    private static final int PERMISSION_REQUEST = 7406;
    private static final Handler MAIN = new Handler(Looper.getMainLooper());
    private static final Map<String, BluetoothDevice> BLE_DEVICES = new LinkedHashMap<>();
    private static final Map<String, Long> BLE_LAST_SEEN = new LinkedHashMap<>();
    private static final Map<String, MidiDeviceInfo> USB_DEVICES = new LinkedHashMap<>();
    private static final int MAX_SESSIONS = 2;
    private static final int MAX_BLE_OPEN_ATTEMPTS = 3;
    private static final long BLE_OPEN_RETRY_DELAY_MS = 300;
    private static final long BLE_DISCOVERY_FRESH_MS = 15000;
    private static final long BLE_VERIFY_TIMEOUT_MS = 6000;

    private static final class Session {
        MidiDevice midiDevice;
        MidiOutputPort outputPort;
        MidiReceiver midiReceiver;
        final ConcurrentLinkedQueue<Integer> events = new ConcurrentLinkedQueue<>();
        volatile int connectionState; // 0 disconnected, 1 connecting, 2 connected, 3 error
        volatile String statusMessage = "Not connected";
        String deviceKey = "";
        String deviceName = "";
        String transport = "";
        int midiDeviceId = -1;
        BluetoothLeScanner verificationScanner;
        ScanCallback verificationCallback;
        int generation;
    }

    // Session 0 remains the established CW Keyer connection. Session 1 is
    // reserved for the independent CTR2-MIDI setup page.
    private static final Session[] SESSIONS = { new Session(), new Session() };

    private static BluetoothLeScanner scanner;
    private static ScanCallback scanCallback;
    private static volatile String scanStatusMessage = "Not connected";
    private static MidiManager callbackManager;
    private static MidiManager.DeviceCallback deviceCallback;

    private AndroidBleMidi() {}

    public static void startScan(Context context) {
        stopScan();
        synchronized (BLE_DEVICES) { BLE_DEVICES.clear(); }
        synchronized (BLE_LAST_SEEN) { BLE_LAST_SEEN.clear(); }
        final int usbCount = refreshUsbDevices(context);

        if (!ensurePermissions(context)) {
            scanStatusMessage = usbCount > 0
                    ? "Found " + usbCount + " USB MIDI device(s); allow Bluetooth to scan BLE MIDI"
                    : "Bluetooth permission requested; tap Scan again after allowing it";
            return;
        }
        final BluetoothManager manager = (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        if (manager == null || manager.getAdapter() == null || !manager.getAdapter().isEnabled()) {
            scanStatusMessage = usbCount > 0
                    ? "Found " + usbCount + " USB MIDI device(s); Bluetooth is off"
                    : "No USB MIDI devices found; Bluetooth is off";
            return;
        }
        scanner = manager.getAdapter().getBluetoothLeScanner();
        if (scanner == null) {
            scanStatusMessage = usbCount > 0
                    ? "Found " + usbCount + " USB MIDI device(s); BLE scanner unavailable"
                    : "BLE scanner unavailable";
            return;
        }
        scanCallback = new ScanCallback() {
            @Override public void onScanResult(int callbackType, ScanResult result) {
                final BluetoothDevice device = result.getDevice();
                if (device == null) return;
                final String address = device.getAddress();
                final boolean isNew;
                synchronized (BLE_DEVICES) { isNew = BLE_DEVICES.put(address, device) == null; }
                synchronized (BLE_LAST_SEEN) {
                    BLE_LAST_SEEN.put(address, SystemClock.elapsedRealtime());
                }
                if (isNew) Log.i(TAG, "Found BLE MIDI device " + safeName(device) + " " + address);
            }

            @Override public void onScanFailed(int errorCode) {
                scanStatusMessage = "BLE scan failed (" + errorCode + ")";
                Log.w(TAG, scanStatusMessage);
            }
        };
        final List<ScanFilter> filters = new ArrayList<>();
        filters.add(new ScanFilter.Builder().setServiceUuid(new ParcelUuid(MIDI_SERVICE)).build());
        scanner.startScan(filters,
                new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), scanCallback);
        scanStatusMessage = usbCount > 0
                ? "Found " + usbCount + " USB MIDI device(s); scanning BLE MIDI..."
                : "Scanning USB and BLE MIDI devices...";
        MAIN.postDelayed(AndroidBleMidi::stopScan, 8000);
    }

    public static String getDevices() {
        final StringBuilder result = new StringBuilder();
        synchronized (USB_DEVICES) {
            for (Map.Entry<String, MidiDeviceInfo> entry : USB_DEVICES.entrySet()) {
                if (result.length() > 0) result.append('\n');
                result.append(deviceName(entry.getValue())).append(" (USB)|").append(entry.getKey());
            }
        }
        synchronized (BLE_DEVICES) {
            for (Map.Entry<String, BluetoothDevice> entry : BLE_DEVICES.entrySet()) {
                if (result.length() > 0) result.append('\n');
                result.append(safeName(entry.getValue())).append(" (BLE)|ble:").append(entry.getKey());
            }
        }
        return result.toString();
    }

    // Compatibility entry point used by the unchanged v1.0.3 CW Keyer path.
    public static boolean connect(Context context, String deviceKey) {
        return connect(context, 0, deviceKey);
    }

    public static boolean connect(Context context, int sessionIndex, String deviceKey) {
        final Session session = session(sessionIndex);
        if (session == null) return false;
        if (deviceKey == null || deviceKey.isEmpty()) {
            session.connectionState = 3;
            session.statusMessage = "No MIDI device selected";
            return false;
        }
        final int otherIndex = sessionIndex == 0 ? 1 : 0;
        final Session other = session(otherIndex);
        if (other != null && (other.connectionState == 1 || other.connectionState == 2)
                && canonicalDeviceKey(deviceKey).equals(canonicalDeviceKey(other.deviceKey))) {
            session.connectionState = 3;
            session.statusMessage = "This MIDI device is already connected in the other setup tab";
            return false;
        }
        if (deviceKey.startsWith("usb:")) return connectUsb(context, sessionIndex, deviceKey);
        if (!ensurePermissions(context)) {
            session.connectionState = 3;
            session.statusMessage = "Bluetooth permission requested; connect again after allowing it";
            return false;
        }

        final String address = deviceKey != null && deviceKey.startsWith("ble:")
                ? deviceKey.substring(4) : deviceKey; // Raw address supports saved pre-USB builds.
        final BluetoothManager bluetoothManager =
                (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        if (bluetoothManager == null || bluetoothManager.getAdapter() == null
                || !bluetoothManager.getAdapter().isEnabled()) {
            session.connectionState = 3;
            session.statusMessage = "Bluetooth is off";
            return false;
        }
        BluetoothDevice foundDevice;
        synchronized (BLE_DEVICES) { foundDevice = BLE_DEVICES.get(address); }
        if (foundDevice == null) {
            try {
                foundDevice = bluetoothManager.getAdapter().getRemoteDevice(address);
            } catch (IllegalArgumentException | SecurityException ignored) {}
        }
        final BluetoothDevice device = foundDevice;
        if (device == null) {
            session.connectionState = 3;
            session.statusMessage = "Saved BLE MIDI device is unavailable; scan again";
            return false;
        }
        disconnect(sessionIndex);
        stopScan();
        final MidiManager manager = (MidiManager) context.getSystemService(Context.MIDI_SERVICE);
        if (manager == null) {
            session.connectionState = 3;
            session.statusMessage = "Android MIDI service unavailable";
            return false;
        }
        ensureDeviceCallback(manager);
        final int generation = ++session.generation;
        session.deviceKey = deviceKey;
        session.deviceName = safeName(device);
        session.transport = "BLE";
        session.connectionState = 1;
        session.statusMessage = "Connecting to " + safeName(device) + " over BLE...";
        if (wasRecentlyDiscovered(address) || isBluetoothConnected(bluetoothManager, device)) {
            openBluetoothDevice(manager, device, sessionIndex, generation, safeName(device), 1);
        } else {
            verifyBluetoothPresence(bluetoothManager, manager, device,
                    sessionIndex, generation, safeName(device));
        }
        return true;
    }

    private static void verifyBluetoothPresence(BluetoothManager bluetoothManager,
                                                MidiManager midiManager,
                                                BluetoothDevice device,
                                                int sessionIndex, int generation,
                                                String name) {
        final Session session = session(sessionIndex);
        final BluetoothLeScanner verifier = bluetoothManager.getAdapter().getBluetoothLeScanner();
        if (session == null || verifier == null) {
            failBluetoothVerification(sessionIndex, generation, name,
                    "BLE scanner unavailable");
            return;
        }
        final String expectedAddress = device.getAddress();
        final ScanCallback callback = new ScanCallback() {
            @Override public void onScanResult(int callbackType, ScanResult result) {
                final BluetoothDevice discovered = result.getDevice();
                if (discovered == null || !expectedAddress.equalsIgnoreCase(discovered.getAddress()))
                    return;
                synchronized (BLE_DEVICES) { BLE_DEVICES.put(expectedAddress, discovered); }
                synchronized (BLE_LAST_SEEN) {
                    BLE_LAST_SEEN.put(expectedAddress, SystemClock.elapsedRealtime());
                }
                stopBluetoothVerification(session);
                if (generation == session.generation && session.connectionState == 1)
                    openBluetoothDevice(midiManager, discovered, sessionIndex, generation, name, 1);
            }

            @Override public void onScanFailed(int errorCode) {
                failBluetoothVerification(sessionIndex, generation, name,
                        "BLE discovery failed (" + errorCode + ")");
            }
        };
        session.verificationScanner = verifier;
        session.verificationCallback = callback;
        session.statusMessage = "Looking for " + name + " over BLE...";
        try {
            final List<ScanFilter> filters = new ArrayList<>();
            filters.add(new ScanFilter.Builder().setDeviceAddress(expectedAddress).build());
            verifier.startScan(filters,
                    new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(),
                    callback);
        } catch (SecurityException | IllegalArgumentException | IllegalStateException exception) {
            Log.w(TAG, "BLE MIDI verification scan failed", exception);
            failBluetoothVerification(sessionIndex, generation, name,
                    "Could not scan for " + name + " over BLE");
            return;
        }
        MAIN.postDelayed(() -> {
            final Session current = session(sessionIndex);
            if (current != null && generation == current.generation
                    && current.connectionState == 1
                    && current.verificationCallback == callback) {
                failBluetoothVerification(sessionIndex, generation, name,
                        name + " was not found; make sure it is turned on");
            }
        }, BLE_VERIFY_TIMEOUT_MS);
    }

    private static void failBluetoothVerification(int sessionIndex, int generation,
                                                  String name, String message) {
        final Session session = session(sessionIndex);
        if (session == null || generation != session.generation || session.connectionState != 1)
            return;
        stopBluetoothVerification(session);
        session.connectionState = 3;
        session.statusMessage = message;
        session.events.clear();
        Log.w(TAG, "Session " + sessionIndex + ": " + message + " (" + name + ")");
    }

    private static void openBluetoothDevice(MidiManager manager, BluetoothDevice device,
                                            int sessionIndex, int generation, String name,
                                            int attempt) {
        final Session session = session(sessionIndex);
        if (session == null || generation != session.generation || session.connectionState != 1)
            return;
        try {
            manager.openBluetoothDevice(device,
                    opened -> finishOpen(sessionIndex, generation, opened, name, "BLE",
                            manager, device, attempt), MAIN);
        } catch (SecurityException | IllegalArgumentException exception) {
            stopBluetoothVerification(session);
            session.connectionState = 3;
            session.statusMessage = "Could not open " + name + " over BLE";
            Log.w(TAG, "BLE MIDI open failed", exception);
        }
    }

    private static boolean connectUsb(Context context, int sessionIndex, String deviceKey) {
        final Session session = session(sessionIndex);
        if (session == null) return false;
        final MidiManager manager = (MidiManager) context.getSystemService(Context.MIDI_SERVICE);
        if (manager == null) {
            session.connectionState = 3;
            session.statusMessage = "Android MIDI service unavailable";
            return false;
        }
        ensureDeviceCallback(manager);
        refreshUsbDevices(context);
        final MidiDeviceInfo info;
        synchronized (USB_DEVICES) { info = USB_DEVICES.get(deviceKey); }
        if (info == null) {
            session.connectionState = 3;
            session.statusMessage = "Saved USB MIDI device is not attached; scan again";
            return false;
        }
        disconnect(sessionIndex);
        stopScan();
        final String name = deviceName(info);
        final int generation = ++session.generation;
        session.deviceKey = deviceKey;
        session.deviceName = name;
        session.transport = "USB";
        session.connectionState = 1;
        session.statusMessage = "Connecting to " + name + " over USB...";
        manager.openDevice(info,
                opened -> finishOpen(sessionIndex, generation, opened, name, "USB",
                        null, null, MAX_BLE_OPEN_ATTEMPTS), MAIN);
        return true;
    }

    private static void finishOpen(int sessionIndex, int generation, MidiDevice opened,
                                   String name, String transport, MidiManager retryManager,
                                   BluetoothDevice retryDevice, int attempt) {
        final Session session = session(sessionIndex);
        if (session == null || generation != session.generation) {
            if (opened != null) {
                try { opened.close(); } catch (IOException ignored) {}
            }
            return;
        }
        final MidiDeviceInfo info = opened == null ? null : opened.getInfo();
        if (info == null) {
            if (opened != null) {
                try { opened.close(); } catch (IOException ignored) {}
            }
            if (retryManager != null && retryDevice != null && attempt < MAX_BLE_OPEN_ATTEMPTS) {
                session.statusMessage = "Waiting for " + name + " BLE MIDI endpoint...";
                Log.w(TAG, "Session " + sessionIndex + ": BLE MIDI port information missing; "
                        + "retrying open (attempt " + (attempt + 1) + ")");
                MAIN.postDelayed(() -> openBluetoothDevice(retryManager, retryDevice,
                                sessionIndex, generation, name, attempt + 1),
                        BLE_OPEN_RETRY_DELAY_MS);
                return;
            }
            session.connectionState = 3;
            session.statusMessage = opened == null
                    ? "Could not open " + name + " over " + transport
                    : name + " returned no MIDI port information";
            Log.w(TAG, "Session " + sessionIndex + ": " + session.statusMessage);
            return;
        }
        session.midiDevice = opened;
        for (MidiDeviceInfo.PortInfo port : info.getPorts()) {
            if (port.getType() != MidiDeviceInfo.PortInfo.TYPE_OUTPUT) continue;
            session.outputPort = opened.openOutputPort(port.getPortNumber());
            if (session.outputPort == null) continue;
            session.midiReceiver = new MidiReceiver() {
                @Override public void onSend(byte[] data, int offset, int count, long timestamp) {
                    parseMidi(session, data, offset, count);
                }
            };
            session.outputPort.connect(session.midiReceiver);
            session.midiDeviceId = info.getId();
            session.deviceName = name;
            session.transport = transport;
            session.connectionState = 2;
            session.statusMessage = "Connected to " + name + " (" + transport + ")";
            Log.i(TAG, "Session " + sessionIndex + ": " + session.statusMessage);
            return;
        }
        session.connectionState = 3;
        session.statusMessage = name + " has no MIDI output port";
        closeDevice(session);
    }

    // Compatibility entry points used by the unchanged v1.0.3 CW Keyer path.
    public static void disconnect() {
        disconnect(0);
    }

    public static void disconnect(int sessionIndex) {
        final Session session = session(sessionIndex);
        if (session == null) return;
        ++session.generation;
        session.connectionState = 0;
        session.statusMessage = "Not connected";
        session.deviceKey = "";
        session.deviceName = "";
        session.transport = "";
        session.events.clear();
        closeDevice(session);
    }

    public static int getConnectionState() { return getConnectionState(0); }
    public static int getConnectionState(int sessionIndex) {
        final Session session = session(sessionIndex);
        return session == null ? 3 : session.connectionState;
    }
    public static String getStatusMessage() { return getStatusMessage(0); }
    public static String getStatusMessage(int sessionIndex) {
        final Session session = session(sessionIndex);
        if (session == null) return "Invalid MIDI session";
        return session.connectionState == 0 && !scanStatusMessage.equals("Not connected")
                ? scanStatusMessage : session.statusMessage;
    }
    public static int pollEvent() {
        return pollEvent(0);
    }
    public static int pollEvent(int sessionIndex) {
        final Session session = session(sessionIndex);
        if (session == null) return -1;
        final Integer event = session.events.poll();
        return event == null ? -1 : event;
    }

    private static void parseMidi(Session session, byte[] data, int offset, int count) {
        for (int i = offset; i + 2 < offset + count; ) {
            final int status = data[i] & 0xff;
            final int kind = status & 0xf0;
            if (kind == 0x80 || kind == 0x90 || kind == 0xb0) {
                final int data1 = data[i + 1] & 0x7f;
                final int data2 = data[i + 2] & 0x7f;
                session.events.offer((status << 16) | (data1 << 8) | data2);
                Log.d(TAG, "MIDI status=" + status + " data1=" + data1 + " data2=" + data2);
                i += 3;
            } else {
                i++;
            }
        }
    }

    private static void stopScan() {
        if (scanner != null && scanCallback != null) {
            try { scanner.stopScan(scanCallback); } catch (SecurityException ignored) {}
        }
        scanCallback = null;
        scanner = null;
    }

    private static void closeDevice(Session session) {
        stopBluetoothVerification(session);
        session.midiDeviceId = -1;
        if (session.outputPort != null) {
            try { session.outputPort.close(); } catch (IOException ignored) {}
            session.outputPort = null;
        }
        session.midiReceiver = null;
        if (session.midiDevice != null) {
            try { session.midiDevice.close(); } catch (IOException ignored) {}
            session.midiDevice = null;
        }
    }

    private static void stopBluetoothVerification(Session session) {
        if (session == null)
            return;
        final BluetoothLeScanner verifier = session.verificationScanner;
        final ScanCallback callback = session.verificationCallback;
        session.verificationScanner = null;
        session.verificationCallback = null;
        if (verifier != null && callback != null) {
            try { verifier.stopScan(callback); } catch (SecurityException | IllegalStateException ignored) {}
        }
    }

    private static boolean wasRecentlyDiscovered(String address) {
        final Long lastSeen;
        synchronized (BLE_LAST_SEEN) { lastSeen = BLE_LAST_SEEN.get(address); }
        return lastSeen != null
                && SystemClock.elapsedRealtime() - lastSeen <= BLE_DISCOVERY_FRESH_MS;
    }

    private static boolean isBluetoothConnected(BluetoothManager manager, BluetoothDevice device) {
        try {
            return manager.getConnectionState(device, BluetoothProfile.GATT)
                    == BluetoothProfile.STATE_CONNECTED;
        } catch (SecurityException | IllegalArgumentException ignored) {
            return false;
        }
    }

    private static synchronized void ensureDeviceCallback(MidiManager manager) {
        if (callbackManager == manager && deviceCallback != null)
            return;
        if (callbackManager != null && deviceCallback != null) {
            try { callbackManager.unregisterDeviceCallback(deviceCallback); }
            catch (IllegalArgumentException ignored) {}
        }
        callbackManager = manager;
        deviceCallback = new MidiManager.DeviceCallback() {
            @Override public void onDeviceRemoved(MidiDeviceInfo info) {
                if (info == null)
                    return;
                synchronized (USB_DEVICES) {
                    USB_DEVICES.entrySet().removeIf(
                            entry -> entry.getValue().getId() == info.getId());
                }
                for (int index = 0; index < MAX_SESSIONS; ++index) {
                    final Session session = SESSIONS[index];
                    if (session.midiDeviceId != info.getId()
                            || (session.connectionState != 1 && session.connectionState != 2))
                        continue;
                    ++session.generation;
                    final String name = session.deviceName.isEmpty()
                            ? "MIDI device" : session.deviceName;
                    final String transport = session.transport.isEmpty()
                            ? "MIDI" : session.transport;
                    session.connectionState = 3;
                    session.statusMessage = name + " disconnected (" + transport + ")";
                    session.events.clear();
                    closeDevice(session);
                    Log.w(TAG, "Session " + index + ": " + session.statusMessage);
                }
            }
        };
        manager.registerDeviceCallback(deviceCallback, MAIN);
    }

    private static Session session(int sessionIndex) {
        return sessionIndex >= 0 && sessionIndex < MAX_SESSIONS ? SESSIONS[sessionIndex] : null;
    }

    private static String canonicalDeviceKey(String deviceKey) {
        if (deviceKey == null) return "";
        if (deviceKey.startsWith("usb:")) return deviceKey;
        final String address = deviceKey.startsWith("ble:") ? deviceKey.substring(4) : deviceKey;
        return "ble:" + address.toUpperCase();
    }

    @SuppressWarnings("deprecation")
    private static int refreshUsbDevices(Context context) {
        final MidiManager manager = (MidiManager) context.getSystemService(Context.MIDI_SERVICE);
        if (manager != null)
            ensureDeviceCallback(manager);
        synchronized (USB_DEVICES) {
            USB_DEVICES.clear();
            if (manager == null) return 0;
            for (MidiDeviceInfo info : manager.getDevices()) {
                if (info.getType() != MidiDeviceInfo.TYPE_USB || info.getOutputPortCount() < 1) continue;
                final String key = usbDeviceKey(info);
                USB_DEVICES.put(key, info);
                Log.i(TAG, "Found USB MIDI device " + deviceName(info));
            }
            return USB_DEVICES.size();
        }
    }

    private static String usbDeviceKey(MidiDeviceInfo info) {
        final Bundle properties = info.getProperties();
        final UsbDevice usb = properties.getParcelable(MidiDeviceInfo.PROPERTY_USB_DEVICE);
        final String identity = (usb == null ? "0:0" : usb.getVendorId() + ":" + usb.getProductId())
                + ":" + propertyString(properties, MidiDeviceInfo.PROPERTY_MANUFACTURER)
                + ":" + propertyString(properties, MidiDeviceInfo.PROPERTY_PRODUCT)
                + ":" + propertyString(properties, MidiDeviceInfo.PROPERTY_SERIAL_NUMBER);
        return "usb:" + Base64.encodeToString(identity.getBytes(StandardCharsets.UTF_8),
                Base64.URL_SAFE | Base64.NO_WRAP);
    }

    private static String deviceName(MidiDeviceInfo info) {
        final Bundle properties = info.getProperties();
        String name = propertyString(properties, MidiDeviceInfo.PROPERTY_NAME);
        if (name.isEmpty()) name = propertyString(properties, MidiDeviceInfo.PROPERTY_PRODUCT);
        return name.isEmpty() ? "USB MIDI" : name;
    }

    private static String propertyString(Bundle properties, String key) {
        final String value = properties.getString(key);
        return value == null ? "" : value;
    }

    private static boolean ensurePermissions(Context context) {
        final String[] permissions;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissions = new String[] { Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT };
        } else {
            permissions = new String[] { Manifest.permission.ACCESS_FINE_LOCATION };
        }
        final List<String> missing = new ArrayList<>();
        for (String permission : permissions) {
            if (context.checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) missing.add(permission);
        }
        if (missing.isEmpty()) return true;
        if (context instanceof Activity) {
            final Activity activity = (Activity) context;
            activity.runOnUiThread(() -> activity.requestPermissions(
                    missing.toArray(new String[0]), PERMISSION_REQUEST));
        }
        return false;
    }

    private static String safeName(BluetoothDevice device) {
        try {
            final String name = device.getName();
            return name == null || name.isEmpty() ? "BLE MIDI" : name;
        } catch (SecurityException ignored) {
            return "BLE MIDI";
        }
    }
}
