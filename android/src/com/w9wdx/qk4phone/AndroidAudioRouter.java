package com.w9wdx.qk4phone;

import android.content.Context;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Build;

import java.util.List;

/** Coordinates Android's media and communications routes around K4 transmit. */
public final class AndroidAudioRouter {
    private static boolean communicationRouteActive;

    private AndroidAudioRouter() {
    }

    public static void setTransmitActive(Context context, boolean active) {
        final AudioManager audioManager =
                (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        if (audioManager == null)
            return;

        if (active)
            beginTransmit(audioManager);
        else
            endTransmit(audioManager);
    }

    /**
     * Qt 6.11's Android audio-device list omits USB_HEADSET and many USB_DEVICE
     * endpoints. Return an external wired/USB output id for the AAudio stream
     * builder; return -1 to retain Android's normal media default.
     */
    public static int getPreferredWiredOutputDeviceId(Context context) {
        final AudioManager audioManager =
                (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        if (audioManager == null)
            return -1;

        for (AudioDeviceInfo device : audioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) {
            switch (device.getType()) {
                case AudioDeviceInfo.TYPE_USB_HEADSET:
                case AudioDeviceInfo.TYPE_USB_DEVICE:
                case AudioDeviceInfo.TYPE_USB_ACCESSORY:
                case AudioDeviceInfo.TYPE_WIRED_HEADSET:
                case AudioDeviceInfo.TYPE_WIRED_HEADPHONES:
                    return device.getId();
                default:
                    break;
            }
        }
        return -1;
    }

    /** Returns the endpoint's preferred hardware rate, or 0 when unknown. */
    public static int getOutputSampleRate(Context context, int deviceId) {
        final AudioManager audioManager =
                (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        if (audioManager == null)
            return 0;

        for (AudioDeviceInfo device : audioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) {
            if (device.getId() != deviceId)
                continue;
            for (int rate : device.getSampleRates()) {
                if (rate == 48000)
                    return rate;
            }
            final int[] rates = device.getSampleRates();
            return rates.length > 0 ? rates[0] : 0;
        }
        return 0;
    }

    private static void beginTransmit(AudioManager audioManager) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            final AudioDeviceInfo device = findTwoWayHeadset(
                    audioManager.getAvailableCommunicationDevices());
            if (device != null) {
                audioManager.setMode(AudioManager.MODE_IN_COMMUNICATION);
                communicationRouteActive = audioManager.setCommunicationDevice(device);
                return;
            }
        } else if (hasLegacyBluetoothHeadset(audioManager)) {
            audioManager.setMode(AudioManager.MODE_IN_COMMUNICATION);
            audioManager.startBluetoothSco();
            audioManager.setBluetoothScoOn(true);
            communicationRouteActive = true;
            return;
        }

        // No two-way external endpoint: normal media output remains selected,
        // while Android uses the phone microphone for the K4 TX stream.
        communicationRouteActive = false;
    }

    private static void endTransmit(AudioManager audioManager) {
        if (!communicationRouteActive)
            return;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            audioManager.clearCommunicationDevice();
        } else {
            audioManager.setBluetoothScoOn(false);
            audioManager.stopBluetoothSco();
        }
        audioManager.setMode(AudioManager.MODE_NORMAL);
        communicationRouteActive = false;
    }

    private static AudioDeviceInfo findTwoWayHeadset(List<AudioDeviceInfo> devices) {
        for (AudioDeviceInfo device : devices) {
            // AudioManager communication devices are sink entries. Android
            // selects their matching input automatically after
            // setCommunicationDevice(); requiring isSource() here rejects
            // USB headsets whose microphone is represented separately.
            if (!device.isSink())
                continue;

            switch (device.getType()) {
                case AudioDeviceInfo.TYPE_BLUETOOTH_SCO:
                case AudioDeviceInfo.TYPE_BLE_HEADSET:
                case AudioDeviceInfo.TYPE_USB_HEADSET:
                case AudioDeviceInfo.TYPE_USB_DEVICE:
                case AudioDeviceInfo.TYPE_USB_ACCESSORY:
                case AudioDeviceInfo.TYPE_WIRED_HEADSET:
                    return device;
                default:
                    break;
            }
        }
        return null;
    }

    private static boolean hasLegacyBluetoothHeadset(AudioManager audioManager) {
        for (AudioDeviceInfo device : audioManager.getDevices(AudioManager.GET_DEVICES_INPUTS)) {
            if (device.getType() == AudioDeviceInfo.TYPE_BLUETOOTH_SCO)
                return true;
        }
        return false;
    }
}
