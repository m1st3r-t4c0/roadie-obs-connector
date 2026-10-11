# Roadie Signal 0.1.12 test release

Tag: `v0.1.12-test.1`. Windows x64, minimum OBS 30.0.0 / Qt 6.5.3.

## What changed

**One Roadie entry in the Tools menu.** **Tools → Roadie Signal…** opens one window with:

- the plugin version, OBS and Qt versions, and whether each dock is running;
- **Microphone level from**;
- **Send OBS data to** and **Accept scene changes from**, each Production, Staging, or Production
  and staging (added in 0.1.11).

It replaces the separate status, microphone and servers entries of 0.1.11.

**A warning when another Roadie Signal DLL is loaded.** At startup the plugin looks for any other
Roadie Signal plugin OBS loaded, usually an older copy left in the plugin folder under another
name. If it finds one, it names the file to delete in a warning, in the OBS log, and in the
settings window. Two loaded copies each add their own Tools entries and compete for the same dock.
Plugins before 0.1.12 do not check, so an older copy is reported only by this one.

**One dock for both servers.** The separate **Roadie Signal (Staging)** dock of 0.1.11 is gone.
When both Production and Staging are in use, the **Roadie Signal** dock shows a **Production /
Staging** switch at the top; with one server, the dock is just that page. Both pages keep running
whichever one is in front, so both servers get your scene. Each page has its own sign-in and its
own **Share OBS state with Roadie** switch: sign in to staging and turn its switch on once, from
the Staging page.

(The tag was moved on 2026-10-11 to add the one-dock change; the first 0.1.12 build had a
separate staging dock.)

## Install

Close OBS. Replace `roadie-signal.dll` in OBS's `obs-plugins/64bit` directory with
the standalone DLL in this release, then restart OBS. Keep the filename, and delete any
older renamed Roadie DLL beside it. Check **Tools > Roadie Signal...** and
**Docks > Roadie Signal**.

The ZIP has the same DLL, this guide, LICENSE, and the build report. The build is unsigned.

## Verification

Built warning-free against OBS 30.0.0 / Qt 6.5.3 from a clean source tree. The DLL's
imports resolved against OBS 30.0.0 and OBS 32.0.4. These are dependency checks, not an
OBS host run; the settings window, the duplicate warning, the staging page's sign-in, the
page behind the switch still sending, and closing OBS with both pages open still need testing
in OBS. `build-report.json` records the
source commit and DLL hash.
