# Automatic FT8/FT4 signal reports

The reference is **WSJT-X 2.7.0, revision b4f9a4**, matching the installed
`jt9.exe` used for comparison. Reports use signal-to-noise ratio in dB with
a **2500 Hz reference noise bandwidth**. They are not the decoder's sync
score, audio amplitude, ALC or RF power. See the
[WSJT-X user guide](https://wsjt.sourceforge.io/wsjtx-doc/wsjtx-main-2.7.0-rc8.html).

## Source and attribution

`src/ft8/ft8snr.cpp` is a C++ adaptation of the WSJT-X project's GPL-3.0
code, credited to the WSJT-X authors and contributors, under the same
GPL-3.0-or-later terms as QK4 Mobile. Reference files at the pinned revision:

- [get_spectrum_baseline.f90](https://github.com/WSJTX/wsjtx/blob/b4f9a4/lib/ft8/get_spectrum_baseline.f90),
  [baseline.f90](https://github.com/WSJTX/wsjtx/blob/b4f9a4/lib/ft8/baseline.f90)
- [ft8_downsample.f90](https://github.com/WSJTX/wsjtx/blob/b4f9a4/lib/ft8/ft8_downsample.f90),
  [sync8d.f90](https://github.com/WSJTX/wsjtx/blob/b4f9a4/lib/ft8/sync8d.f90),
  [ft8b.f90](https://github.com/WSJTX/wsjtx/blob/b4f9a4/lib/ft8/ft8b.f90)
- [getcandidates4.f90](https://github.com/WSJTX/wsjtx/blob/b4f9a4/lib/ft4/getcandidates4.f90),
  [ft4_baseline.f90](https://github.com/WSJTX/wsjtx/blob/b4f9a4/lib/ft4/ft4_baseline.f90),
  [ft4_decode.f90](https://github.com/WSJTX/wsjtx/blob/b4f9a4/lib/ft4_decode.f90)

FT8 uses the Nuttall-windowed spectrum, ten-segment lower-envelope noise
baseline, fourth-order polynomial fit and +0.65 dB correction. Its 192000-point
FFT is downconverted to 200 samples/second with the reference taper and
normalization. Costas timing/frequency refinement precedes the sum of power
in the 79 decoded tones. The normal `ft8b` estimator is preserved:
`10 log10(xsig / xbase / 3e6 - 1) - 27`, including its guards and -24 dB floor.

FT4 uses 2304-point Nuttall-windowed spectra at 576-sample steps, a 15-bin
smoothed spectrum, the reference lower-envelope baseline, parabolic spectral
peak interpolation and `10 log10(peak / baseline - 1) - 14.8`, floored at
-21 dB. The peak is associated with a CRC-valid decoded station; it is not
the unrelated ft8_lib Costas sync score. Reports are rounded to integer dB
and capped at the protocol's +49 maximum.

The FFT implementation is the existing Kiss FFT. Polynomial coordinates are
scaled for numerical conditioning without changing the unweighted fit.
The fitted frequency range follows this module's 100–3300 Hz audio range;
FT4 retains the reference's 200 Hz lower estimator boundary. Its narrower
reference range is also passed to `jt9` during comparisons. Initial FT8
timing comes from the existing receiver and therefore need not be identical
to WSJT-X's acquisition result. The receiver remains ft8_lib; this change
does not claim WSJT-X decoding sensitivity or advanced-mode support.

## Automatic exchange

Each decoded station carries its measured report into the activity list and
station selection. CQ responses populate the outgoing report automatically;
report and R-report messages use it, and the completed QSO retains the actual
sent/received reports for logging. Manual report entry has been removed from
Options. Merely saving Options cannot erase the measured report. Repeated
early/final decodes cannot overwrite an already-sent report or reopen a QSO.

## Reference validation

The optional comparison runs the real WSJT-X `jt9.exe` on the **same S16 WAV**
fed to QK4. It uses no radio connection or live transmit action:

```powershell
$env:QK4_WSJT_REFERENCE='C:\WSJT\wsjtx\bin\jt9.exe'
.\test-windows.cmd -Action Test
```

The reference executable identifies itself as 2.7.0 b4f9a4 and has SHA-256
`63be300faa1dee757270de01f31e0cddc985549714badc9ab2faede673fc534d`.
Deterministic white-noise fixtures specify signal power relative to the
noise power in 2500 Hz independently of either estimator. Comparisons cover
-18 through +15 dB, 1250/1733/2873 Hz, off-grid start timing and an eightfold
audio-level change. FT4 matches the reference integer reports in all six
cases; FT8 differs by at most 1 dB in its six cases. The regression tolerance
is 2 dB against WSJT-X and 3 dB against injected SNR.

One -15 dB FT4 fixture is below this receiver's acquisition threshold; its
estimator is tested with the known payload, and WSJT-X independently decodes
the WAV. This is an estimator check, not a claim that QK4 decoded that case.
Additional end-to-end tests feed noisy decoded replies through the real
screen/session state, automatically send the measured report, process R-09,
and complete RR73 without entering a report manually.

Real K4 channel filtering, AGC, codec effects, adjacent interference and RF
acceptance still require recordings/device comparisons. Synthetic estimator
agreement does not establish identical on-air sensitivity.
