# Roadie connector 1.1.1 for Windows

Windows x64 · OBS 30 or newer · OBS plugin 1.0.0 · Roadie connector 1.1.1

## What's new

- **Fixed: the dock's Update button.** On connector 1.0.0 and 1.1.0, pressing Update ended with "The
  plugin did not answer". From 1.1.1, Update downloads the new connector and it runs the next time
  OBS starts. Coming from 1.0.0 or 1.1.0, update by hand once (see **Update** below); every update
  after that is the button.
- Two ZIPs from now on: one for OBS 33 and newer, one for OBS 30 to 32, each laid out as the folder
  you extract it into.

For developers: the dock's `update-plugin` command now carries its target as `target`, because
`version` is the message format's own field. Connectors that read it list `update-core`.

## Update

**From the dock:** press **Update** in the Roadie Signal dock when Roadie offers 1.1.1, then restart OBS.
The Update button works from Roadie connector 1.1.1 and newer.

**By hand** (Roadie connector 1.0.0 or 1.1.0): close OBS, download the ZIP for your OBS version below,
copy `roadie-signal-core.dll` from it over the one in this folder, and start OBS. The OBS plugin file stays as it is.

| OBS | Copy `roadie-signal-core.dll` to |
|---|---|
| OBS 33 and newer | `C:\ProgramData\obs-studio\plugins\roadie-signal\data\core` |
| OBS 30 to 32 | `C:\Program Files\obs-studio\data\obs-plugins\roadie-signal\core` |

## New install

Check your OBS version in **Help → About**, then download the ZIP for it under **Assets** (not **Source code**).

### OBS 33 and newer: `roadie-signal-1.1.1-windows-x64-obs33-and-newer.zip`

1. Close OBS.
2. Open `C:\ProgramData\obs-studio\plugins` in File Explorer (paste it into the address bar; create the `plugins` folder if it is missing).
3. Extract the ZIP there. It puts `roadie-signal.dll` in `roadie-signal` and `roadie-signal-core.dll` in `roadie-signal\data\core`.
4. Start OBS, open **Docks → Roadie Signal** and log in to Roadie.

### OBS 30 to 32: `roadie-signal-1.1.1-windows-x64-obs30-to-32.zip`

1. Close OBS.
2. Open `C:\Program Files\obs-studio` in File Explorer.
3. Extract the ZIP there (Windows asks for administrator permission). It puts `roadie-signal.dll` in `obs-plugins\64bit` and `roadie-signal-core.dll` in `data\obs-plugins\roadie-signal\core`.
4. Start OBS, open **Docks → Roadie Signal** and log in to Roadie.

**Tools → Roadie Signal…** then shows "OBS plugin 1.0.0 · Roadie connector 1.1.1". Never put
`roadie-signal-core.dll` beside `roadie-signal.dll`: OBS would load it as a second plugin. Portable OBS and
other installs: see the [README](../../README.md#install-on-windows).

## Release files

| File | For |
|---|---|
| `roadie-signal-1.1.1-windows-x64-obs33-and-newer.zip` | A new install or a hand update on OBS 33 and newer. Extract into `C:\ProgramData\obs-studio\plugins`. |
| `roadie-signal-1.1.1-windows-x64-obs30-to-32.zip` | A new install or a hand update on OBS 30 to 32. Extract into `C:\Program Files\obs-studio`. |
| `roadie-signal-core-windows-x64.dll`, `manifest-windows-x64.json`, `manifest-windows-x64.sig` | The dock's Update button. The manifest names the core's SHA-256, size and minimum OBS; the signature is ECDSA P-256 by the release key built into the plugin. |
| `SHA256SUMS.txt`, `build-report.json` | Checksums and build record. |
