# QK4 Mobile v1.0.6

QK4 Mobile v1.0.6 improves Android Radio Manager reliability and clarity when
configuring a K4 connection.

## Radio Manager

- Audio Mode and Streaming Latency choices now open in the existing QK4 Mobile
  window instead of an Android native combo-box popup. This avoids the
  Android EGL-surface teardown path that could abort the application while
  changing Streaming Latency.
- The choice sheets use compact, phone-appropriate sizing while retaining
  touch scrolling, **CANCEL**, and **USE** actions.
- When **Use TLS (Encrypted)** is selected with an empty Password, Radio
  Manager now explains that the K4 TLS pre-shared key is required and keeps
  the form open. Non-TLS profiles can still connect without a password.

## Validation and scope

This ARM64 Android release uses version code 33. The Radio Manager selector
and TLS-password prompt workflows were tested on a Samsung Galaxy S26 Ultra.
The v1.0.6 source passed the required policy, native-test, Android-build, and
iOS-compile checks. This release does not alter K4 CAT, TLS, or audio protocol
behavior.
