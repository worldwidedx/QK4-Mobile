# Android Opus dependency

This directory contains the Opus public headers and the prebuilt ARM64 Android static library used by the verified QK4 Android development build.

The prebuilt library was copied from the working Android build environment during repository packaging. Its generated package metadata did not retain a meaningful upstream version number, so it should be rebuilt from a tagged [Xiph Opus](https://github.com/xiph/opus) release before a production release.

Expected layout:

```text
include/opus/*.h
lib/libopus.a
```

Set `QK4_OPUS_ROOT` to use a separately built Opus installation.

