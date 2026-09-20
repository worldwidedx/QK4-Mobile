package com.w9wdx.qk4phone;

import android.content.Context;
import android.media.AudioDeviceCallback;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;

/**
 * Reports local sidetone route changes without replacing its low-latency
 * native playback path. Casting is intentionally outside this monitor's scope.
 */
public final class AndroidSidetoneRouteMonitor {
    private static AudioManager audioManager;
    private static AudioDeviceCallback callback;
    private static int generation;

    private AndroidSidetoneRouteMonitor() { }

    public static synchronized void start(Context context) {
        if (callback != null)
            return;

        audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        if (audioManager == null)
            return;

        callback = new AudioDeviceCallback() {
            @Override public void onAudioDevicesAdded(AudioDeviceInfo[] devices) {
                noteRelevantChange(devices);
            }

            @Override public void onAudioDevicesRemoved(AudioDeviceInfo[] devices) {
                noteRelevantChange(devices);
            }
        };
        audioManager.registerAudioDeviceCallback(callback, new Handler(Looper.getMainLooper()));
        generation++;
    }

    public static synchronized void stop(Context context) {
        if (audioManager != null && callback != null)
            audioManager.unregisterAudioDeviceCallback(callback);
        callback = null;
        audioManager = null;
    }

    public static synchronized int getGeneration() {
        return generation;
    }

    /**
     * Return endpoints that Qt's Android output enumeration may omit. Normal
     * Bluetooth and speaker media routing remain Android's policy-selected
     * system output and are selected by rebuilding the native sink with
     * AAUDIO_UNSPECIFIED after a relevant device change.
     */
    public static int getPreferredDirectOutputDeviceId(Context context) {
        final AudioManager manager =
                (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        if (manager == null)
            return -1;

        for (AudioDeviceInfo device : manager.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) {
            switch (device.getType()) {
                case AudioDeviceInfo.TYPE_USB_HEADSET:
                case AudioDeviceInfo.TYPE_USB_DEVICE:
                case AudioDeviceInfo.TYPE_USB_ACCESSORY:
                case AudioDeviceInfo.TYPE_WIRED_HEADSET:
                case AudioDeviceInfo.TYPE_WIRED_HEADPHONES:
                case AudioDeviceInfo.TYPE_HEARING_AID:
                    return device.getId();
                default:
                    break;
            }
        }
        return -1;
    }

    private static synchronized void noteRelevantChange(AudioDeviceInfo[] devices) {
        if (devices == null)
            return;
        for (AudioDeviceInfo device : devices) {
            if (isRelevantOutput(device)) {
                generation++;
                return;
            }
        }
    }

    private static boolean isRelevantOutput(AudioDeviceInfo device) {
        if (device == null || !device.isSink())
            return false;

        switch (device.getType()) {
            case AudioDeviceInfo.TYPE_BUILTIN_SPEAKER:
            case AudioDeviceInfo.TYPE_BLUETOOTH_A2DP:
            case AudioDeviceInfo.TYPE_BLUETOOTH_SCO:
            case AudioDeviceInfo.TYPE_USB_HEADSET:
            case AudioDeviceInfo.TYPE_USB_DEVICE:
            case AudioDeviceInfo.TYPE_USB_ACCESSORY:
            case AudioDeviceInfo.TYPE_WIRED_HEADSET:
            case AudioDeviceInfo.TYPE_WIRED_HEADPHONES:
            case AudioDeviceInfo.TYPE_HEARING_AID:
                return true;
            default:
                break;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            return device.getType() == AudioDeviceInfo.TYPE_BLE_HEADSET
                    || device.getType() == AudioDeviceInfo.TYPE_BLE_SPEAKER;
        }
        return false;
    }
}
