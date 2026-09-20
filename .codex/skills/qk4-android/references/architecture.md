# Architecture reference

## Core data path

```text
K4 TCP/TLS connection
  -> Protocol packet parser
  -> RadioState and controllers
  -> Touch-oriented widgets

K4 RX Opus stream
  -> Opus decoder
  -> Audio engine
  -> Android audio output

Android microphone
  -> Audio engine
  -> Opus encoder
  -> K4 TX stream

K4 dB/bin stream
  -> spectrum controller
  -> panadapter renderer
  -> local spectrum/waterfall display features
```

## Android external audio routing

- RX playback uses a native Android `AudioTrack` with media attributes and
  rebuilds when Android reports output-device changes.
- USB/wired endpoints and Android `TYPE_HEARING_AID` outputs are preferred RX
  devices when present. This does not alter the received K4 Opus stream.
- A hearing aid is RX-only unless Android explicitly exposes a supported
  two-way communication input. Do not include `TYPE_HEARING_AID` in
  `AndroidAudioRouter` TX device selection.

## Reference policy

Use current upstream QK4 as the behavioral reference for:

- Connection sequencing and keepalive
- TLS/PSK handling
- Audio stream format and timing
- CAT command semantics
- Radio state and unsolicited updates

The Android proof-of-concept fork is historical reference only. Do not use it as the authority when it conflicts with current QK4 mainline.

Local Android rendering may intentionally diverge from radio display commands. In particular, a CAT command that changes the K4 LCD does not provide rendered output to the client.

## Important distinction

- `#WBS` controls waterfall color range.
- `#WFC` selects a waterfall palette.
- `#PKM` controls the radio's peak-mode state but does not provide a peak trace.
