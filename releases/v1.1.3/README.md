# Roadie connector 1.1.3 for Windows

Windows x64 · OBS 30 or newer · OBS plugin 1.0.1 · Roadie connector 1.1.3

## What's new

- **Fixed: OBS could still crash on Windows while the dock page moved to another page** (Roadie
  #412, after #304). When the dock page signs in or reloads, OBS's browser can be without the page
  for a moment, and a call into it then crashes OBS. The dock page now says when it is about to
  leave, and the connector makes no call until the next page is there, however long the sign-in
  takes. Until now it waited at most three seconds.
- **The mic level goes to the dock page four times a second instead of ten**, with the loudest
  reading since the last one, so Signal activity misses no peak. Fewer calls into the page means
  fewer chances to meet that moment.
- **Crash reports keep what matters when they are long.** A report from an OBS with many threads
  used to be cut at 64 KB, and the list of loaded plugins never reached Roadie. Now the crashed
  thread and the loaded plugins come first, and other threads fill the rest.
- **Crash reports say which Roadie connector was running** when OBS crashed. Before, Roadie only
  knew the version that sent the report, which after an update is a newer one.
- **Update from the dock.** This release changes only the connector, so the Update button installs it.

## Update

**From the dock:** press **Update** in the Roadie Signal dock when Roadie offers 1.1.3, then restart OBS.
The Update button works from Roadie connector 1.1.1 and newer.

**By hand** (Roadie connector 1.0.0 or 1.1.0): close OBS, download the ZIP for your OBS version below,
copy `roadie-signal-core.dll` from it over the one in this folder, and start OBS. The OBS plugin file stays as it is.

| OBS | Copy `roadie-signal-core.dll` to |
|---|---|
| OBS 33 and newer | `C:\ProgramData\obs-studio\plugins\roadie-signal\data\core` |
| OBS 30 to 32 | `C:\Program Files\obs-studio\data\obs-plugins\roadie-signal\core` |

## New install

Check your OBS version in **Help → About**, then download the ZIP for it under **Assets** (not **Source code**).

### OBS 33 and newer: `roadie-signal-1.1.3-windows-x64-obs33-and-newer.zip`

1. Close OBS.
2. Open `C:\ProgramData\obs-studio\plugins` in File Explorer (paste it into the address bar; create the `plugins` folder if it is missing).
3. Extract the ZIP there. It puts `roadie-signal.dll` in `roadie-signal` and `roadie-signal-core.dll` in `roadie-signal\data\core`.
4. Start OBS, open **Docks → Roadie Signal** and log in to Roadie.

### OBS 30 to 32: `roadie-signal-1.1.3-windows-x64-obs30-to-32.zip`

1. Close OBS.
2. Open `C:\Program Files\obs-studio` in File Explorer.
3. Extract the ZIP there (Windows asks for administrator permission). It puts `roadie-signal.dll` in `obs-plugins\64bit` and `roadie-signal-core.dll` in `data\obs-plugins\roadie-signal\core`.
4. Start OBS, open **Docks → Roadie Signal** and log in to Roadie.

**Tools → Roadie Signal…** then shows "OBS plugin 1.0.1 · Roadie connector 1.1.3". Never put
`roadie-signal-core.dll` beside `roadie-signal.dll`: OBS would load it as a second plugin. Portable OBS and
other installs: see the [README](../../README.md#install-on-windows).

## Release files

| File | For |
|---|---|
| `roadie-signal-1.1.3-windows-x64-obs33-and-newer.zip` | A new install or a hand update on OBS 33 and newer. Extract into `C:\ProgramData\obs-studio\plugins`. |
| `roadie-signal-1.1.3-windows-x64-obs30-to-32.zip` | A new install or a hand update on OBS 30 to 32. Extract into `C:\Program Files\obs-studio`. |
| `roadie-signal-core-windows-x64.dll`, `manifest-windows-x64.json`, `manifest-windows-x64.sig` | The dock's Update button. The manifest names the core's SHA-256, size and minimum OBS; the signature is ECDSA P-256 by the release key built into the plugin. |
| `SHA256SUMS.txt`, `build-report.json` | Checksums and build record. |
