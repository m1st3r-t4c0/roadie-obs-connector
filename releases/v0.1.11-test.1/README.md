# Roadie Signal 0.1.11 test release

Tag: `v0.1.11-test.1`. Windows x64, minimum OBS 30.0.0 / Qt 6.5.3.

## What changed

**Tools → Roadie Signal servers…** chooses which Roadie servers this OBS talks to. There are two
choices, each Production, Staging, or Production and staging:

- **Send OBS data to.** Scene, streaming and recording state, microphone level, stream health and
  crash reports.
- **Accept scene changes from.** Whose scene-switch commands OBS follows. Any other server is
  refused at once and is not offered scene switching.

Both default to Production, which is how 0.1.10 behaved. The choices apply at once and are kept
across restarts. Each server in use has its own dock and its own sign-in: **Docks → Roadie Signal**
and **Docks → Roadie Signal (Staging)** (`https://staging.roadie.gg/dock`). **If you accept scene
changes from staging, staging can change your live scene.**

Saving the microphone choice no longer erases the plugin's other settings.

## Install

Close OBS. Replace `roadie-signal.dll` in OBS's `obs-plugins/64bit` directory with
the standalone DLL in this release, then restart OBS. Keep the filename, and do not
leave an older renamed Roadie DLL beside it. Check **Tools > Roadie Signal 0.1.11
status...** and **Docks > Roadie Signal**.

The ZIP has the same DLL, this guide, LICENSE, and the build report. The build is unsigned.

## Verification

Built warning-free against OBS 30.0.0 / Qt 6.5.3 from a clean source tree. The DLL's
imports resolved against OBS 30.0.0 and OBS 32.0.4. These are dependency checks, not an
OBS host run; the servers dialog, the staging dock's sign-in, and closing OBS with both
docks open still need testing in OBS. `build-report.json` records the source commit and
DLL hash.
