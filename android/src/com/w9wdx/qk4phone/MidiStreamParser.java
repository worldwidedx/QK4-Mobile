package com.w9wdx.qk4phone;

/** Parses a MIDI byte stream across arbitrary Android MidiReceiver callbacks. */
final class MidiStreamParser {
    interface MessageSink {
        void onMessage(int status, int data1, int data2);
    }

    private int runningStatus = -1;
    private int firstData = -1;
    private boolean inSysEx;

    synchronized void reset() {
        runningStatus = -1;
        firstData = -1;
        inSysEx = false;
    }

    synchronized void feed(byte[] bytes, int offset, int count, MessageSink sink) {
        for (int i = offset; i < offset + count; ++i) {
            final int value = bytes[i] & 0xff;
            if (value >= 0xf8) // Real-time messages do not interrupt another message.
                continue;
            if (value == 0xf0) {
                reset();
                inSysEx = true;
                continue;
            }
            if (value >= 0xf0) {
                reset(); // System-common messages cancel running status.
                continue;
            }
            if (inSysEx && (value & 0x80) == 0)
                continue;
            // A new channel status also terminates a malformed/truncated
            // SysEx message; do not lose subsequent paddle edges forever.
            inSysEx = false;
            if ((value & 0x80) != 0) {
                runningStatus = value;
                firstData = -1;
                continue;
            }
            if (runningStatus < 0)
                continue;
            final int kind = runningStatus & 0xf0;
            if (kind == 0xc0 || kind == 0xd0) {
                // Program change and channel pressure have one data byte.
                firstData = -1;
                continue;
            }
            if (firstData < 0) {
                firstData = value;
                continue;
            }
            if (kind == 0x80 || kind == 0x90 || kind == 0xb0)
                sink.onMessage(runningStatus, firstData, value);
            firstData = -1;
        }
    }
}
