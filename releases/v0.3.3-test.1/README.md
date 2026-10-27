# Roadie Signal 0.3.3 (test build)

Tag: `v0.3.3-test.1`. Windows x64, minimum OBS 30.0.0 / Qt 6.5.3. The first release with a
**macOS (Apple Silicon)** build, thanks to **CtrlGremlin**, who ported the plugin to the Mac.

A test prerelease: built and dependency-checked, not yet run in a real OBS.

## What is new

- **Roadie Signal on the Mac.** CtrlGremlin ported the plugin to Apple Silicon Macs. The Mac
  package is a separate download on this release: `roadie-signal-0.3.3-obs<version>-macos-arm64.zip`,
  with its own install guide inside.
- **No crash when quitting OBS on a Mac.** CtrlGremlin also found and fixed three crashes that hit
  every quit with the dock open. This fix is macOS only.
- **Windows is unchanged.** The Windows DLL differs from 0.3.2 only in its version number, so a
  Windows user on 0.3.2 does not need this update.

Everything from 0.3.2 is unchanged, including staying signed in on OBS 33.

## Install on Windows

Close OBS. Replace `roadie-signal.dll` in OBS's `obs-plugins/64bit` directory with
the standalone DLL in this release, then restart OBS. Keep the filename, and delete any
older renamed Roadie DLL beside it. Check **Tools > Roadie Signal...** and
**Docks > Roadie Signal**.

The ZIP has the same DLL, this guide, LICENSE, and the build report. The build is unsigned.

## Install on a Mac

Download the macOS ZIP, open it, and follow its `README.txt`: quit OBS, right-click
`install.command`, choose **Open**. It needs an Apple Silicon Mac and the OBS version named in
the package's `build-report.json`, or newer. The plugin is ad-hoc signed, not notarized, so macOS
warns before the first run.

## Verification

Windows: built warning-free against OBS 30.0.0 / Qt 6.5.3 from a clean source tree. The DLL's
imports resolved against OBS 30.0.0 and OBS 32.0.4. These are dependency checks, not an OBS host
run. `build-report.json` records the source commit and DLL hash.

macOS: built with `scripts/build-macos.sh` from the same commit; its own `build-report.json` and
`SHA256SUMS-macos.txt` record the binary hash and the OBS version it was built against.
