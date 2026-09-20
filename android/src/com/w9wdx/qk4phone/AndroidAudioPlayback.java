package com.w9wdx.qk4phone;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioDeviceCallback;
import android.media.AudioDeviceInfo;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Handler;
import android.os.Looper;

/** Receiver playback using Android's normal media routing, independent of Qt AAudio. */
public final class AndroidAudioPlayback {
    private static AudioTrack track;
    private static AudioManager audioManager;
    private static int sampleRate;
    private static int channelCount;
    private static AudioDeviceCallback callback;

    private AndroidAudioPlayback() { }

    public static synchronized boolean start(Context context, int rate, int channels) {
        stop(context);
        audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        if (audioManager == null)
            return false;
        sampleRate = rate;
        channelCount = channels;
        callback = new AudioDeviceCallback() {
            @Override public void onAudioDevicesAdded(AudioDeviceInfo[] devices) { rebuild(); }
            @Override public void onAudioDevicesRemoved(AudioDeviceInfo[] devices) { rebuild(); }
        };
        audioManager.registerAudioDeviceCallback(callback, new Handler(Looper.getMainLooper()));
        return createTrack();
    }

    public static synchronized void stop(Context context) {
        if (audioManager != null && callback != null)
            audioManager.unregisterAudioDeviceCallback(callback);
        callback = null;
        releaseTrack();
        audioManager = null;
    }

    public static synchronized int write(Context context, byte[] data, int length) {
        if (track == null || data == null || length <= 0)
            return -1;
        return track.write(data, 0, Math.min(length, data.length), AudioTrack.WRITE_NON_BLOCKING);
    }

    private static synchronized void rebuild() {
        if (audioManager == null)
            return;
        releaseTrack();
        createTrack();
    }

    private static boolean createTrack() {
        final int mask = channelCount == 1 ? AudioFormat.CHANNEL_OUT_MONO : AudioFormat.CHANNEL_OUT_STEREO;
        final int min = AudioTrack.getMinBufferSize(sampleRate, mask, AudioFormat.ENCODING_PCM_16BIT);
        if (min <= 0)
            return false;
        track = new AudioTrack.Builder()
                .setAudioAttributes(new AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
                .setAudioFormat(new AudioFormat.Builder().setSampleRate(sampleRate)
                        .setChannelMask(mask).setEncoding(AudioFormat.ENCODING_PCM_16BIT).build())
                .setBufferSizeInBytes(Math.max(min * 4, sampleRate * channelCount))
                .setTransferMode(AudioTrack.MODE_STREAM).build();
        final AudioDeviceInfo output = preferredExternalOutput();
        if (output != null)
            track.setPreferredDevice(output);
        track.play();
        return track.getState() == AudioTrack.STATE_INITIALIZED;
    }

    private static void releaseTrack() {
        if (track == null)
            return;
        try { track.pause(); } catch (IllegalStateException ignored) { }
        try { track.flush(); } catch (IllegalStateException ignored) { }
        track.release();
        track = null;
    }

    /**
     * Prefer direct receive-only endpoints which Qt's Android device list can
     * omit. Hearing aids remain an RX-only route; transmit input selection is
     * intentionally handled separately by AndroidAudioRouter.
     */
    private static AudioDeviceInfo preferredExternalOutput() {
        if (audioManager == null)
            return null;
        for (AudioDeviceInfo device : audioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS)) {
            switch (device.getType()) {
                case AudioDeviceInfo.TYPE_USB_HEADSET:
                case AudioDeviceInfo.TYPE_USB_DEVICE:
                case AudioDeviceInfo.TYPE_USB_ACCESSORY:
                case AudioDeviceInfo.TYPE_WIRED_HEADSET:
                case AudioDeviceInfo.TYPE_WIRED_HEADPHONES:
                case AudioDeviceInfo.TYPE_HEARING_AID:
                    return device;
                default:
                    break;
            }
        }
        return null;
    }
}
