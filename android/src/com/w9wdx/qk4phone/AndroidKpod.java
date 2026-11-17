package com.w9wdx.qk4phone;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbEndpoint;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.util.Log;

import java.util.concurrent.ConcurrentLinkedQueue;

/**
 * Elecraft K-Pod (USB HID 04D8:F12D) access through Android's USB host API.
 *
 * hidapi/libusb are not available to an unrooted Android app, so this class
 * performs the same exchange as the desktop KpodHidWorker: write an 8-byte
 * report beginning with 'u', read the 8-byte reply. A background thread polls
 * the device; the native side drains the replies with pollEvent() and runs the
 * shared decoder, so the K-Pod protocol logic is not duplicated.
 */
public final class AndroidKpod {
    private static final String TAG = "QK4Kpod";
    private static final int VENDOR_ID = 0x04D8;
    private static final int PRODUCT_ID = 0xF12D;
    private static final String ACTION_PERMISSION = "com.w9wdx.qk4phone.KPOD_USB_PERMISSION";
    private static final int REPORT_SIZE = 8;
    private static final int POLL_INTERVAL_MS = 20;
    private static final int READ_TIMEOUT_MS = 10;
    private static final int INFO_TIMEOUT_MS = 200;
    private static final int MAX_QUEUE = 256;
    private static final int FLAG_MUTABLE = 0x02000000; // PendingIntent.FLAG_MUTABLE, API 31

    // Control-transfer values for HID SET_REPORT (output report 0).
    private static final int REQ_TYPE_SET_REPORT = 0x21;
    private static final int REQ_SET_REPORT = 0x09;
    private static final int OUTPUT_REPORT_0 = 0x0200;

    private static final ConcurrentLinkedQueue<Long> EVENTS = new ConcurrentLinkedQueue<>();
    private static boolean receiverRegistered;
    private static String permissionRequestedFor;
    private static volatile boolean permissionDenied;

    private static UsbDeviceConnection connection;
    private static UsbInterface claimedInterface;
    private static UsbEndpoint endpointIn;
    private static UsbEndpoint endpointOut;
    private static Thread pollThread;
    private static volatile boolean running;
    private static String cachedInfo = "";
    private static String cachedInfoDevice = "";

    private AndroidKpod() { }

    private static UsbDevice findDevice(UsbManager manager) {
        if (manager == null)
            return null;
        for (UsbDevice device : manager.getDeviceList().values()) {
            if (device.getVendorId() == VENDOR_ID && device.getProductId() == PRODUCT_ID)
                return device;
        }
        return null;
    }

    private static synchronized void ensureReceiver(Context context) {
        if (receiverRegistered)
            return;
        final BroadcastReceiver receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context ctx, Intent intent) {
                final String action = intent.getAction();
                if (ACTION_PERMISSION.equals(action)) {
                    permissionDenied = !intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false);
                    if (permissionDenied)
                        Log.w(TAG, "USB permission denied for K-Pod");
                } else if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(action)) {
                    final UsbDevice device = Build.VERSION.SDK_INT >= 33
                            ? intent.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice.class)
                            : intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                    if (device != null && device.getVendorId() == VENDOR_ID && device.getProductId() == PRODUCT_ID) {
                        stop();
                        synchronized (AndroidKpod.class) {
                            permissionRequestedFor = null;
                            permissionDenied = false;
                            cachedInfo = "";
                            cachedInfoDevice = "";
                        }
                    }
                }
            }
        };
        final IntentFilter filter = new IntentFilter(ACTION_PERMISSION);
        filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
        final Context app = context.getApplicationContext();
        if (Build.VERSION.SDK_INT >= 33)
            app.registerReceiver(receiver, filter, Context.RECEIVER_NOT_EXPORTED);
        else
            app.registerReceiver(receiver, filter);
        receiverRegistered = true;
    }

    /**
     * 0 = no K-Pod attached, 1 = attached and waiting for USB permission,
     * 2 = attached and permitted. Asks Android for permission once per attach.
     */
    public static synchronized int state(Context context) {
        ensureReceiver(context);
        final UsbManager manager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        final UsbDevice device = findDevice(manager);
        if (device == null)
            return 0;
        if (manager.hasPermission(device))
            return 2;
        if (!device.getDeviceName().equals(permissionRequestedFor)) {
            permissionRequestedFor = device.getDeviceName();
            permissionDenied = false;
            final Intent intent = new Intent(ACTION_PERMISSION).setPackage(context.getPackageName());
            int flags = PendingIntent.FLAG_UPDATE_CURRENT;
            if (Build.VERSION.SDK_INT >= 31)
                flags |= FLAG_MUTABLE;
            manager.requestPermission(device, PendingIntent.getBroadcast(context, 0, intent, flags));
        }
        return 1;
    }

    /** True if the user declined the USB permission prompt for the attached K-Pod. */
    public static boolean permissionDenied() {
        return permissionDenied;
    }

    /** "product\tmanufacturer\tfirmware\tdeviceId", or "" when unavailable. */
    public static synchronized String info(Context context) {
        final UsbManager manager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        final UsbDevice device = findDevice(manager);
        if (device == null || !manager.hasPermission(device))
            return "";
        if (device.getDeviceName().equals(cachedInfoDevice) && !cachedInfo.isEmpty())
            return cachedInfo;

        final boolean wasRunning = connection != null;
        if (!wasRunning && !openLocked(manager, device))
            return "";
        final String id = query((byte) '=', false);
        final String fw = query((byte) 'v', true);
        if (!wasRunning)
            closeLocked();

        final String product = device.getProductName() == null ? "K-Pod" : device.getProductName();
        final String maker = device.getManufacturerName() == null ? "Elecraft" : device.getManufacturerName();
        cachedInfo = product + "\t" + maker + "\t" + fw + "\t" + id;
        cachedInfoDevice = device.getDeviceName();
        return cachedInfo;
    }

    /** Opens the device and starts the polling thread. */
    public static synchronized boolean start(Context context) {
        if (running && connection != null)
            return true;
        final UsbManager manager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        final UsbDevice device = findDevice(manager);
        if (device == null || !manager.hasPermission(device))
            return false;
        if (connection == null && !openLocked(manager, device))
            return false;
        EVENTS.clear();
        running = true;
        pollThread = new Thread(AndroidKpod::pollLoop, "QK4-Kpod");
        pollThread.setDaemon(true);
        pollThread.start();
        return true;
    }

    public static void stop() {
        final Thread thread;
        synchronized (AndroidKpod.class) {
            running = false;
            thread = pollThread;
            pollThread = null;
        }
        if (thread != null && thread != Thread.currentThread()) {
            try {
                thread.join(500);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
        synchronized (AndroidKpod.class) {
            closeLocked();
            EVENTS.clear();
        }
    }

    /** True while the device is open and the polling thread is healthy. */
    public static boolean isOpen() {
        return running && connection != null;
    }

    /**
     * Next reply, or -1 when none. Bit 32 flags validity; bits 0-31 hold reply
     * bytes 0-3 (command, ticks low, ticks high, controls) in little-endian
     * order. Bytes 4-7 are spare in the K-Pod protocol.
     */
    public static long pollEvent() {
        final Long event = EVENTS.poll();
        return event == null ? -1L : event;
    }

    private static boolean openLocked(UsbManager manager, UsbDevice device) {
        for (int i = 0; i < device.getInterfaceCount(); i++) {
            final UsbInterface iface = device.getInterface(i);
            if (iface.getInterfaceClass() != UsbConstants.USB_CLASS_HID)
                continue;
            UsbEndpoint in = null;
            UsbEndpoint out = null;
            for (int e = 0; e < iface.getEndpointCount(); e++) {
                final UsbEndpoint ep = iface.getEndpoint(e);
                if (ep.getType() != UsbConstants.USB_ENDPOINT_XFER_INT)
                    continue;
                if (ep.getDirection() == UsbConstants.USB_DIR_IN && in == null)
                    in = ep;
                else if (ep.getDirection() == UsbConstants.USB_DIR_OUT && out == null)
                    out = ep;
            }
            if (in == null)
                continue;
            final UsbDeviceConnection conn = manager.openDevice(device);
            if (conn == null) {
                Log.w(TAG, "openDevice failed");
                return false;
            }
            // true = detach any kernel HID driver, as hidapi-libusb does on Linux.
            if (!conn.claimInterface(iface, true)) {
                Log.w(TAG, "claimInterface failed");
                conn.close();
                return false;
            }
            connection = conn;
            claimedInterface = iface;
            endpointIn = in;
            endpointOut = out;
            return true;
        }
        Log.w(TAG, "no HID interface with an interrupt IN endpoint");
        return false;
    }

    private static void closeLocked() {
        if (connection != null) {
            if (claimedInterface != null)
                connection.releaseInterface(claimedInterface);
            connection.close();
        }
        connection = null;
        claimedInterface = null;
        endpointIn = null;
        endpointOut = null;
    }

    private static boolean sendCommand(UsbDeviceConnection conn, byte command) {
        final byte[] report = new byte[REPORT_SIZE];
        report[0] = command;
        final int result;
        if (endpointOut != null)
            result = conn.bulkTransfer(endpointOut, report, REPORT_SIZE, INFO_TIMEOUT_MS);
        else
            result = conn.controlTransfer(REQ_TYPE_SET_REPORT, REQ_SET_REPORT, OUTPUT_REPORT_0,
                    claimedInterface.getId(), report, REPORT_SIZE, INFO_TIMEOUT_MS);
        return result >= 0;
    }

    /** One request/reply used for the '=' (device ID) and 'v' (firmware) commands. */
    private static String query(byte command, boolean firmware) {
        final UsbDeviceConnection conn = connection;
        if (conn == null || !sendCommand(conn, command))
            return "";
        final byte[] reply = new byte[REPORT_SIZE];
        if (conn.bulkTransfer(endpointIn, reply, REPORT_SIZE, INFO_TIMEOUT_MS) != REPORT_SIZE)
            return "";
        if (firmware) {
            final int bcd = (reply[1] & 0xFF) | ((reply[2] & 0xFF) << 8);
            return String.format("%d.%02d", bcd / 100, bcd % 100);
        }
        final StringBuilder id = new StringBuilder();
        for (int i = 1; i < REPORT_SIZE && reply[i] != 0; i++)
            id.append((char) (reply[i] & 0xFF));
        return id.toString().trim();
    }

    private static void pollLoop() {
        final byte[] reply = new byte[REPORT_SIZE];
        int failures = 0;
        while (running) {
            final UsbDeviceConnection conn = connection;
            if (conn == null)
                break;
            if (!sendCommand(conn, (byte) 'u')) {
                if (++failures > 5) {
                    Log.w(TAG, "write failed, device gone?");
                    break;
                }
            } else {
                final int n = conn.bulkTransfer(endpointIn, reply, REPORT_SIZE, READ_TIMEOUT_MS);
                if (n < 0) {
                    // Timeout and "no data" both report -1; only treat a run of
                    // consecutive write failures as a lost device.
                    failures = 0;
                } else if (n == REPORT_SIZE) {
                    failures = 0;
                    final long event = (1L << 32)
                            | (reply[0] & 0xFFL)
                            | ((reply[1] & 0xFFL) << 8)
                            | ((reply[2] & 0xFFL) << 16)
                            | ((reply[3] & 0xFFL) << 24);
                    EVENTS.add(event);
                    while (EVENTS.size() > MAX_QUEUE)
                        EVENTS.poll();
                }
            }
            try {
                Thread.sleep(POLL_INTERVAL_MS);
            } catch (InterruptedException e) {
                break;
            }
        }
        running = false;
    }
}
