# Roadie Signal 0.3.2 (test build)

Tag: `v0.3.2-test.1`. Windows x64, minimum OBS 30.0.0 / Qt 6.5.3.

A test prerelease: built and dependency-checked, not yet run in a real OBS.

## What is new

- **You stay signed in on OBS 33.** On OBS 33 the dock asked for a sign-in every time OBS
  started. OBS 33's browser only saves cookies for a store that sits directly in its own config
  folder, and Roadie's was one folder deeper, so nothing was saved. The store has moved up a
  level.
- **Your current sign-in carries over.** The first start copies the old store across, so an OBS
  30–32 user stays signed in. The old folder is kept, so going back to an older build still works.

Everything from 0.3.1 is unchanged.

## Install

Close OBS. Replace `roadie-signal.dll` in OBS's `obs-plugins/64bit` directory with
the standalone DLL in this release, then restart OBS. Keep the filename, and delete any
older renamed Roadie DLL beside it. Check **Tools > Roadie Signal...** and
**Docks > Roadie Signal**.

On OBS 33, sign in once more after installing (the old build never saved a sign-in there), then
restart OBS: the dock should come back signed in.

The ZIP has the same DLL, this guide, LICENSE, and the build report. The build is unsigned.

## Verification

Built warning-free against OBS 30.0.0 / Qt 6.5.3 from a clean source tree. The DLL's
imports resolved against OBS 30.0.0 and OBS 32.0.4. These are dependency checks, not an
OBS host run. `build-report.json` records the source commit and DLL hash.
