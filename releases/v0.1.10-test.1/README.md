# Roadie Signal 0.1.10 test release

Tag: `v0.1.10-test.1`. Windows x64, minimum OBS 30.0.0 / Qt 6.5.3.

## What changed

This build carries the 0.1.10 version label. The native scene bridge and OBS action
behavior are unchanged from 0.1.9. The matching Roadie server and web update removes
the OBS connector page's state poll and pushes scene changes to open Stage outputs over
Roadie's WebSocket. **The Roadie update must be deployed separately**; installing this
DLL alone does not speed up the Stage or connector page. Plugin 0.1.9 is compatible
with the Roadie update.

## Install

Close OBS. Replace `roadie-signal.dll` in OBS's `obs-plugins/64bit` directory with
the standalone DLL in this release, then restart OBS. Keep the filename, and do not
leave an older renamed Roadie DLL beside it. Check **Tools > Roadie Signal 0.1.10
status...** and **Docks > Roadie Signal**.

The ZIP has the same DLL, this guide, LICENSE, and the build report. The build is unsigned.

## Verification

Built warning-free against OBS 30.0.0 / Qt 6.5.3. The DLL's imports resolved
against OBS 30.0.0 and OBS 32.2.1. These are dependency checks, not an OBS host
run; scene timing and full plugin behavior still need testing in OBS after Roadie
is deployed. `build-report.json` records the source commit and DLL hash.
