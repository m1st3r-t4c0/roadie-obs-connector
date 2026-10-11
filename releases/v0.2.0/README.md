# Roadie Signal 0.2.0

Tag: `v0.2.0`. Windows x64, minimum OBS 30.0.0 / Qt 6.5.3.

The first canonical release of the Roadie OBS connector. Every build before it was a test
prerelease (`vX.Y.Z-test.N`).

## What is in it

0.2.0 is 0.1.12's source with a new version number; nothing else changed. It carries everything
up to and including 0.1.12:

- **One settings window**, **Tools → Roadie Signal…**: the plugin, OBS and Qt versions, dock
  status, **Microphone level from**, and **Send OBS data to** / **Accept scene changes from**
  (Production, Staging, or both).
- **One Roadie Signal dock.** It shows the production page. With Staging turned on, a staging page
  runs out of sight behind it, signs itself in with your Twitch login, and shares OBS state with
  nothing else to click.
- **Scene switching from Roadie**, OBS streaming / recording / scene state, stream health, OBS
  crash reports (while the dock's switch is on), the microphone meter, and Stage transition events.
- **A warning when another Roadie Signal DLL is loaded**, naming the file to delete.

Coming from 0.1.12, the dock behaves the same; Roadie will show 0.2.0 as the running version.

## Install

Close OBS. Replace `roadie-signal.dll` in OBS's `obs-plugins/64bit` directory with
the standalone DLL in this release, then restart OBS. Keep the filename, and delete any
older renamed Roadie DLL beside it. Check **Tools > Roadie Signal...** and
**Docks > Roadie Signal**.

The ZIP has the same DLL, this guide, LICENSE, and the build report. The build is unsigned.

## Verification

Built warning-free against OBS 30.0.0 / Qt 6.5.3 from a clean source tree. The DLL's
imports resolved against OBS 30.0.0 and OBS 32.0.4. These are dependency checks, not an
OBS host run. `build-report.json` records the source commit and DLL hash.
