# Shared mobile code boundaries

One implementation serves Android phone/tablet and iPhone/iPad. This defines
responsibilities in the existing source tree; it does not require a directory
rewrite before accepting useful contributions.

| Responsibility | Existing area / rule |
|---|---|
| K4 transport, packet parsing, CAT, and state | `src/network`, `src/models`, controllers; shared semantics and sequencing |
| Codecs, DSP, program audio, transmit coordination | `src/audio`, `src/dsp`, `src/sstv`, `src/ft8`; shared algorithms and state |
| Phone presentation | Shared widgets in `src/ui` and console construction in `src/mainwindow.cpp`; one compact path for both phone platforms |
| Tablet presentation | Shared regular-layout construction and widgets with compact fallback; no Android-tablet/iPad UI copies |
| Device and usable-window classification | Central policy, currently rooted in `K4Styles::configureForScreen`; modules consume the result |
| Native services | Android Java/JNI and `src/android`; iOS adapters under `src/ios` as integrated; audio routes, permissions, lifecycle, credentials, keyboard and orientation requests |
| Build and packaging | CMake and supported platform scripts; platform dependencies selected explicitly |

Use `isCompactLayout()` for presentation differences, never as a proxy for
whether touch gestures should work. Use OS conditionals for native APIs, not to
choose a phone versus tablet UX. Separate device class from available layout
space as required by the [orientation policy](ORIENTATION_POLICY.md).

Native adapters may differ, but their shared contract must be explicit: input,
output, ownership, error handling, interruption/disconnect behavior, and lifecycle.
Preserve Android's established TLS backend when introducing an iOS backend;
validate both separately. Shared UI code does not prove equal keyboard, audio,
safe-area, or background/resume behavior on different operating systems.

Tablet PRs must not silently alter shared dimensions, painting, signal wiring,
or phone initialization. Review the entire diff against the accepted phone
baseline, then verify behavior. Improvements affecting phones belong in explicit
PRs with product approval, not hidden inside a platform port.

Do not couple layout activation to protocol/audio rewrites. Experimental UI is
gated independently from native platform services and from release validation.
See [development workflow](../CONTRIBUTING.md) and
[experimental activation](RELEASE_PROCESS.md#experimental-support).
