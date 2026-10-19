# Roadie Signal 0.3.0 (test build)

Tag: `v0.3.0-test.1`. Windows x64, minimum OBS 30.0.0 / Qt 6.5.3.

A test prerelease: built and dependency-checked, not yet run in a real OBS.

## What is new

- **Every Roadie OBS action works through the plugin**, with no bot connected to OBS. Roadie's
  automation can now set a text source, show or hide a source, mute or unmute an audio input, and
  turn a filter on or off, as well as switch scenes. Each change is read back from OBS before Roadie
  counts the step as done.
- **Roadie's OBS steps list your sources and filters** from the plugin, so you can pick them
  instead of typing names.
- **Tools → Roadie Signal…** now says **Accept OBS changes from**, since that choice covers every
  change, not only scenes. Your saved choice carries over.

Everything from 0.2.0 is unchanged.

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
