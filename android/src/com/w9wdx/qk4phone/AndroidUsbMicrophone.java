package com.w9wdx.qk4phone;

import android.content.Context;
import android.media.AudioDeviceInfo;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.util.Log;

/**
 * USB headset capture using Android's device-routing API.
 *
 * Qt 6.11 does not reliably enumerate or select USB microphone endpoints on
 * Android. This class supplies only that physical capture endpoint. QK4 keeps
 * its established 48 kHz -> 12 kHz, gain, framing, and encoding pipeline.
 */
public final class AndroidUsbMicrophone {
    private static final String TAG = "QK4UsbMic";
    private static final int SAMPLE_RATE = 48000;
    private static AudioRecord recorder;
    private static boolean routeReported;

    private AndroidUsbMicrophone() { }

    /** Starts only when Android exposes a real wired/USB microphone input. */
    public static synchronized boolean start(Context context) {
        stop(context);
        final AudioManager manager =
                (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        final AudioDeviceInfo input = findUsbInput(manager);
        if (input == null)
            return false;

        final int minBuffer = AudioRecord.getMinBufferSize(SAMPLE_RATE,
                AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
        if (minBuffer <= 0)
            return false;

        recorder = new AudioRecord.Builder()
                // Match Qt's current Android source rather than applying
                // voice-call gain/noise processing to radio audio.
                .setAudioSource(MediaRecorder.AudioSource.VOICE_RECOGNITION)
                .setAudioFormat(new AudioFormat.Builder()
                        .setSampleRate(SAMPLE_RATE)
                        .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                        .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                        .build())
                .setBufferSizeInBytes(Math.max(minBuffer * 4, SAMPLE_RATE / 5))
                .build();

        if (recorder.getState() != AudioRecord.STATE_INITIALIZED ||
                !recorder.setPreferredDevice(input)) {
            Log.w(TAG, "USB microphone endpoint rejected: " + input.getProductName());
            release();
            return false;
        }

        recorder.startRecording();
        if (recorder.getRecordingState() != AudioRecord.RECORDSTATE_RECORDING) {
            Log.w(TAG, "USB microphone did not start recording");
            release();
            return false;
        }

        routeReported = false;
        Log.i(TAG, "Requested USB microphone: " + input.getProductName());
        return true;
    }

    /** Reads 48 kHz, mono, signed 16-bit PCM. Returns a negative error on loss. */
    public static synchronized int read(Context context, byte[] data, int length) {
        if (recorder == null || data == null || length <= 0)
            return -1;

        final AudioDeviceInfo routed = recorder.getRoutedDevice();
        if (routed == null || !isUsbInput(routed)) {
            if (routeReported) {
                Log.w(TAG, "USB microphone route was lost");
                routeReported = false;
            }
            return -2;
        }
        if (!routeReported) {
            Log.i(TAG, "USB microphone active: " + routed.getProductName());
            routeReported = true;
        }

        return recorder.read(data, 0, Math.min(length, data.length),
                AudioRecord.READ_NON_BLOCKING);
    }

    public static synchronized void stop(Context context) {
        release();
    }

    private static void release() {
        routeReported = false;
        if (recorder == null)
            return;
        try { recorder.stop(); } catch (IllegalStateException ignored) { }
        recorder.release();
        recorder = null;
    }

    private static AudioDeviceInfo findUsbInput(AudioManager manager) {
        if (manager == null)
            return null;
        for (AudioDeviceInfo device : manager.getDevices(AudioManager.GET_DEVICES_INPUTS)) {
            if (isUsbInput(device))
                return device;
        }
        return null;
    }

    private static boolean isUsbInput(AudioDeviceInfo device) {
        switch (device.getType()) {
            case AudioDeviceInfo.TYPE_USB_HEADSET:
            case AudioDeviceInfo.TYPE_USB_DEVICE:
            case AudioDeviceInfo.TYPE_USB_ACCESSORY:
            case AudioDeviceInfo.TYPE_WIRED_HEADSET:
                return true;
            default:
                return false;
        }
    }
}
