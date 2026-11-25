# Roadie connector 1.1.2 for Windows

Windows x64 · OBS 30 or newer · OBS plugin 1.0.1 · Roadie connector 1.1.2

## What's new

- **Fixed: the dock never appeared on OBS 32.2 and newer.** OBS 32.2.0 changed how Windows looks
  for DLLs, and OBS plugin 1.0.0 could no longer open its connector. The OBS log said `core bundled
  did not load` and `no core could be loaded`. OBS plugin 1.0.1 loads the connector the same way
  OBS loads its own plugins, on every OBS from 30 on.
- **Reinstall once, by hand.** The fix is in the OBS plugin file, and the Update button only replaces
  the connector. Close OBS and extract the ZIP for your OBS version as for a new install.
- When the connector cannot load, the OBS log now says whether the file is missing or Windows
  refused it, with the path and the Windows error.
- **Fixed: OBS could crash on Windows while the dock page reloaded or a dialog was open** (Roadie
  #304). The connector now waits until the dock page has loaded again, any dialog has closed, and
  never calls into a page that is closing. Audio and status updates in that moment are skipped;
  command results wait and go once the page is ready. This also covers the dock's first page, while
  OBS is still opening it.
- **Fixed: stream health showed no encoder frames for multitrack streams** (Enhanced Broadcasting).
  Each track's encoder reads its own video, so the connector now adds up the frames of every track.
  A single-track stream reports the same numbers as before.
- **Fixed: Roadie got no OBS data while the Roadie Signal dock was closed.** OBS's browser starts a
  dock's page only the first time the dock is shown, so a dock that was closed when OBS started
  (unticked under **Docks**, or never placed) sent no scene, stream, audio or health data until it
  was opened. The connector now starts the page out of sight two seconds after OBS loads, and Roadie
  sees OBS whether or not the dock is open. A closed dock also signs itself in, like the page behind
  it. The OBS log says `the Roadie Signal dock is closed; starting … out of sight` when this happens.
- **New: Take an OBS screenshot works while you are not live.** Before, a screenshot step failed with
  "OBS is not live" whenever OBS was not streaming, so a run while offline never took one.

## Update

**This release changes the OBS plugin (1.0.0 to 1.0.1), so everyone updates by hand once:**
close OBS, download the ZIP for your OBS version below, extract it into the same folder as a new install
and let it replace both files, then start OBS. The dock's **Update** button changes only the Roadie
connector, never the OBS plugin.

| OBS | Extract the ZIP into |
|---|---|
| OBS 33 and newer | `C:\ProgramData\obs-studio\plugins` |
| OBS 30 to 32 | `C:\Program Files\obs-studio` |

## New install

Check your OBS version in **Help → About**, then download the ZIP for it under **Assets** (not **Source code**).

### OBS 33 and newer: `roadie-signal-1.1.2-windows-x64-obs33-and-newer.zip`

1. Close OBS.
2. Open `C:\ProgramData\obs-studio\plugins` in File Explorer (paste it into the address bar; create the `plugins` folder if it is missing).
3. Extract the ZIP there. It puts `roadie-signal.dll` in `roadie-signal` and `roadie-signal-core.dll` in `roadie-signal\data\core`.
4. Start OBS, open **Docks → Roadie Signal** and log in to Roadie.

### OBS 30 to 32: `roadie-signal-1.1.2-windows-x64-obs30-to-32.zip`

1. Close OBS.
2. Open `C:\Program Files\obs-studio` in File Explorer.
3. Extract the ZIP there (Windows asks for administrator permission). It puts `roadie-signal.dll` in `obs-plugins\64bit` and `roadie-signal-core.dll` in `data\obs-plugins\roadie-signal\core`.
4. Start OBS, open **Docks → Roadie Signal** and log in to Roadie.

**Tools → Roadie Signal…** then shows "OBS plugin 1.0.1 · Roadie connector 1.1.2". Never put
`roadie-signal-core.dll` beside `roadie-signal.dll`: OBS would load it as a second plugin. Portable OBS and
other installs: see the [README](../../README.md#install-on-windows).

## Release files

| File | For |
|---|---|
| `roadie-signal-1.1.2-windows-x64-obs33-and-newer.zip` | A new install or a hand update on OBS 33 and newer. Extract into `C:\ProgramData\obs-studio\plugins`. |
| `roadie-signal-1.1.2-windows-x64-obs30-to-32.zip` | A new install or a hand update on OBS 30 to 32. Extract into `C:\Program Files\obs-studio`. |
| `roadie-signal-core-windows-x64.dll`, `manifest-windows-x64.json`, `manifest-windows-x64.sig` | The dock's Update button. The manifest names the core's SHA-256, size and minimum OBS; the signature is ECDSA P-256 by the release key built into the plugin. |
| `SHA256SUMS.txt`, `build-report.json` | Checksums and build record. |
