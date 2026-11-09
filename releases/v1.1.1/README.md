# Roadie connector 1.1.1 for Windows

Tag: `v1.1.1`. Windows x64, minimum OBS 30.0.0. OBS plugin (loader) 1.0.0.

**On Roadie connector 1.0.0 or 1.1.0?** Their Update button cannot work (see below), so install this
one by hand, once: close OBS, put `roadie-signal-core.dll` from the ZIP's `data/obs-plugins/roadie-signal/core`
folder over the one in your plugin's core folder, and start OBS. For OBS 33 and newer that folder is
`C:/ProgramData/obs-studio/plugins/roadie-signal/data/core`; for OBS 30 to 32 it is
`C:/Program Files/obs-studio/data/obs-plugins/roadie-signal/core`. The OBS plugin file stays as it is.
Every update after this one is the dock's **Update** button.

## What is new

Roadie connector 1.1.1; the OBS plugin (loader) stays 1.0.0.

- **Fixed: Update never reached the plugin.** `update-plugin` carried the version to update to as
  `version`, the same key as the message format (always 1). The page's target overwrote it, and the
  plugin dropped every update command as malformed, so the dock said the plugin did not answer.
  The target is now `target`, and `update-core` in the capabilities tells the dock this build reads
  it. **1.0.0 and 1.1.0 cannot be updated from the dock: install 1.1.1 by hand once** (both files, as
  for 1.0.0); every update after it is one click (t4c0, 2026-11-08).

Tested: both DLLs build against OBS 30.0.0 with no warnings and pass the loader check against 30.0.0
and 32.0.4. **Not tested in OBS yet.**

## Release files

| File | For |
|---|---|
| `roadie-signal-core-windows-x64.dll`, `manifest-windows-x64.json`, `manifest-windows-x64.sig` | The dock's Update button. The manifest names the core's SHA-256, size and minimum OBS; the signature is ECDSA P-256 by the release key built into the plugin. |
| `roadie-signal-1.1.1-obs30.0.0-windows-x64.zip` | A new install by hand: both DLLs, README, LICENSE, build report. See the [README](../../README.md#install-on-windows). |
| `SHA256SUMS.txt`, `build-report.json` | Checksums and build record. |
