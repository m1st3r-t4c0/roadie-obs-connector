# OBS load failure and minimum-version fix — 2026-09-29

The streamer supplied an OBS startup log showing:

```text
08:22:49.199: Qt Version: 6.8.3 (runtime), 6.8.3 (compiled)
08:22:49.333: OBS 32.0.4 (64-bit, windows)
08:22:50.143: Module '../../obs-plugins/64bit/roadie-signal.dll' not loaded
```

The first test DLL used OBS 32.2.1 / Qt 6.11.1 and an exact-version guard.
A Windows loader probe reproduced its dependency failure against official OBS
32.0.4 with error 127, before plugin initialization. This is a native compatibility
failure; missing scene assets in the rest of the log are unrelated.

The user's direction, verbatim:

> updating OBS is dangerous, can we make this work on older versions easily. is there anything that requires this version?

> can we just target the minimum level

The required dock registration API starts at OBS 30.0.0. Its browser submodule
(e397df52e70392ebb9146e0ab6317c0d1a30bce4) provides panel API 3, including JavaScript
execution; the required virtual signatures match the 32.0.4 and 32.2.1 interfaces.
The minimum distribution bundles Qt 6.5.3. Build 0.1.1 compiles/links against these
baseline binaries and accepts compatible newer runtimes rather than exact versions.

Release compilation, x64 and export checks passed. The new DLL resolves dependencies
and exports OBS API 30.0.0 under isolated OBS 30.0.0, 32.0.4 and 32.2.1 distributions.
The checks do not initialize OBS, test the browser, or validate the microphone meter.
Streamer host verification remains required. Current compatibility/build details
belong in the repository README; this is the dated diagnostic record.
