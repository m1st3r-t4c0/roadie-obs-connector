# Roadie Signal 0.3.1 (test build)

Tag: `v0.3.1-test.1`. Windows x64, minimum OBS 30.0.0 / Qt 6.5.3.

A test prerelease: built and dependency-checked, not yet run in a real OBS.

## What is new

- **Roadie's Firebot import finds OBS sources Firebot saved by number**, with no Firebot connector.
  The plugin now tells Roadie OBS's own number for each source in each scene or group.
- **Imported videos turn themselves off.** The plugin tells Roadie a video source's length when
  OBS has its file open, so an imported video plays, waits that long, and hides again.

Everything from 0.3.0 is unchanged.

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
