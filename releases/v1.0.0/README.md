# Roadie Signal 1.0.0 for Windows

Tag: `v1.0.0`. Windows x64, minimum OBS 30.0.0 / Qt 6.5.3. The last version you install by hand:
from here on, the dock's **Update** button brings new versions.

## What is new

- **One-click updates.** When Roadie offers a newer version, press **Update** in the dock. It
  downloads, checks the release's signature, and runs from the next OBS start. This is the
  plugin's only outbound request, and it happens only when you press Update.
- **Crash rollback.** If a new version crashes OBS on two starts in a row, the plugin goes back to
  the last version that ran fine, on its own.
- **Take an OBS screenshot without Firebot.** Roadie's _Take an OBS screenshot_ step now works
  through the plugin. Only an OBS that is streaming takes it.

## Install (two files now)

Close OBS, extract the ZIP, and put both files in the folders for your OBS version:

| OBS | `roadie-signal.dll` (from `obs-plugins\64bit`) | `roadie-signal-core.dll` (from `data\obs-plugins\roadie-signal\core`) |
|---|---|---|
| 30–32 | `C:\Program Files\obs-studio\obs-plugins\64bit\` | `C:\Program Files\obs-studio\data\obs-plugins\roadie-signal\core\` |
| 33 and newer | `C:\ProgramData\obs-studio\plugins\roadie-signal\` | `C:\ProgramData\obs-studio\plugins\roadie-signal\data\core\` |
| Portable, 30–32 | `<portable OBS>\obs-plugins\64bit\` | `<portable OBS>\data\obs-plugins\roadie-signal\core\` |
| Portable, 33+ | `<portable OBS>\plugins\roadie-signal\` | `<portable OBS>\plugins\roadie-signal\data\core\` |

`roadie-signal.dll` replaces your current one. Never put `roadie-signal-core.dll` beside it. Then
restart OBS. The full guide is in the [README](../../README.md#install-on-windows).

## Release files

| File | For |
|---|---|
| `roadie-signal-1.0.0-obs30.0.0-windows-x64.zip` | Installing by hand: both DLLs, README, LICENSE, build report. |
| `roadie-signal-core-windows-x64.dll`, `manifest-windows-x64.json`, `manifest-windows-x64.sig` | The dock's Update button. The manifest names the core's SHA-256, size and minimum OBS; the signature is ECDSA P-256 by the release key built into the plugin. |
| `SHA256SUMS.txt`, `build-report.json` | Checksums and build record. |

## Verification

Built warning-free against OBS 30.0.0 / Qt 6.5.3 from a clean tree (`build-report.json` records
the commit and both DLL hashes). The loader's imports resolve against OBS 30.0.0 and 32.0.4. The
loader's update and rollback rules pass 14 cases on a headless libobs with stand-in cores. The
plugin's own signature check accepts this release's manifest and refuses altered manifests,
altered signatures and other keys, and `sign-core.mjs` checked the signature against the key built
into the plugin before writing the files.

**Not verified:** a run in a real OBS, including the dock, sign-in and the update download itself.
These are dependency and unit checks, not an OBS host run. The Mac release stays 0.3.1.
