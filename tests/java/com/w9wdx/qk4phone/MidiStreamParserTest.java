package com.w9wdx.qk4phone;

import java.util.ArrayList;
import java.util.List;

public final class MidiStreamParserTest {
    private static void expect(List<String> actual, String... expected) {
        if (actual.size() != expected.length)
            throw new AssertionError("Expected " + expected.length + " messages, got " + actual);
        for (int i = 0; i < expected.length; ++i) {
            if (!expected[i].equals(actual.get(i)))
                throw new AssertionError("Expected " + expected[i] + " at " + i + ", got " + actual);
        }
    }

    private static void feed(MidiStreamParser parser, List<String> messages, int... values) {
        final byte[] bytes = new byte[values.length];
        for (int i = 0; i < values.length; ++i)
            bytes[i] = (byte) values[i];
        parser.feed(bytes, 0, bytes.length,
                (status, data1, data2) -> messages.add(status + ":" + data1 + ":" + data2));
    }

    public static void main(String[] args) {
        final MidiStreamParser parser = new MidiStreamParser();
        final List<String> messages = new ArrayList<>();

        // A callback can stop in the middle of an edge, and later callbacks
        // may omit the status byte under MIDI running status.
        feed(parser, messages, 0x90, 20);
        expect(messages);
        feed(parser, messages, 100, 21, 80, 0xf8, 20, 0);
        expect(messages, "144:20:100", "144:21:80", "144:20:0");

        // Polyphonic aftertouch is consumed but not mistaken for a paddle edge.
        feed(parser, messages, 0xa0, 20, 5, 0x80, 21, 0);
        expect(messages, "144:20:100", "144:21:80", "144:20:0", "128:21:0");

        // SysEx and one-byte channel messages cannot shift following notes.
        feed(parser, messages, 0xf0, 0x7d, 0x02, 0xf7,
                0xc0, 4, 0x90, 20, 70);
        expect(messages, "144:20:100", "144:21:80", "144:20:0",
                "128:21:0", "144:20:70");

        parser.reset();
        feed(parser, messages, 21, 10, 0x90, 21, 127);
        expect(messages, "144:20:100", "144:21:80", "144:20:0",
                "128:21:0", "144:20:70", "144:21:127");

        parser.feed(new byte[] {99, (byte) 0x80, 21, 0, 99}, 1, 3,
                (status, data1, data2) -> messages.add(status + ":" + data1 + ":" + data2));
        expect(messages, "144:20:100", "144:21:80", "144:20:0",
                "128:21:0", "144:20:70", "144:21:127", "128:21:0");

        feed(parser, messages, 0xf0, 0x7d, 0x90, 20, 90);
        expect(messages, "144:20:100", "144:21:80", "144:20:0",
                "128:21:0", "144:20:70", "144:21:127", "128:21:0",
                "144:20:90");

        System.out.println("MidiStreamParserTest passed");
    }
}
