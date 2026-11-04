# Roadie Signal for OBS

Roadie Signal is a native OBS plugin that adds a **Roadie Signal** dock to OBS. The dock opens
Roadie's signed-in `/dock` page and gives Roadie what it needs from OBS without OBS WebSocket or
a bot: streaming, recording and scene state, a microphone level, stream health, crash reports,
and commands such as switching scenes, setting text, showing sources, muting inputs, toggling
filters and taking screenshots.

- [Download](#download)
- [Install on Windows](#install-on-windows)
- [Install on a Mac](#install-on-a-mac)
- [1.0.0: one-click updates](#100-one-click-updates)
- [Settings](#settings)
- [Build from source](#build-from-source)
- [Bridge protocol](docs/bridge-protocol.md)
- [Changelog](CHANGELOG.md)

## Download

| Your computer | Download | Needs |
|---|---|---|
| **Windows** (x64) | [Roadie Signal 1.0.0 for Windows](https://github.com/m1st3r-t4c0/roadie-obs-connector/releases/latest) | OBS 30.0.0 or newer |
| **Mac with Apple Silicon** (M1 or newer) | [Roadie Signal 0.3.1 for macOS](https://github.com/m1st3r-t4c0/roadie-obs-connector/releases/tag/macos-v0.3.1-test.1) | OBS 32.2 or newer |
| **Intel Mac** | Not supported yet | |

Windows and Mac have separate releases, and the Mac build can be a version behind. The "Latest"
release on GitHub is always the Windows one. Older release tags keep their original `-test.N`
names, but those releases are no longer test builds.

## Install on Windows

The OBS connector is the recommended way to connect Roadie to your stream. Use the Firebot
plugin only if you plan to connect Roadie's queue system to Firebot.

1. **Download** the Windows ZIP under the release's **Assets** (not **Source code**). No
   compilation is needed.
2. **Install.** Check your OBS version in **Help → About**, close OBS completely and extract the
   ZIP. Since 1.0.0 the plugin is **two files**, and both go in the folders for your OBS version
   below. Create the folders if they do not exist. Windows may ask for administrator permission.
3. **Restart OBS.**
4. **Open the dock:** **Docks → Roadie Signal**.
5. **Log in** to Roadie inside the dock.

| OBS | `roadie-signal.dll` (from the ZIP's `obs-plugins\64bit`) | `roadie-signal-core.dll` (from the ZIP's `data\obs-plugins\roadie-signal\core`) |
|---|---|---|
| 30–32 | `C:\Program Files\obs-studio\obs-plugins\64bit\` | `C:\Program Files\obs-studio\data\obs-plugins\roadie-signal\core\` |
| 33 and newer | `C:\ProgramData\obs-studio\plugins\roadie-signal\` | `C:\ProgramData\obs-studio\plugins\roadie-signal\data\core\` |
| Portable, 30–32 | `<portable OBS>\obs-plugins\64bit\` | `<portable OBS>\data\obs-plugins\roadie-signal\core\` |
| Portable, 33+ | `<portable OBS>\plugins\roadie-signal\` | `<portable OBS>\plugins\roadie-signal\data\core\` |

For OBS 30–32, use the OBS installation you actually run. OBS 29 and earlier are not supported.
**Never put `roadie-signal-core.dll` beside `roadie-signal.dll`**: OBS would try to load it as a
second plugin.

**OBS 33+:** paste `C:\ProgramData\obs-studio\plugins` into File Explorer's address bar if
ProgramData is hidden. Do not put either file in OBS's `core` folder. The old Program Files
location is flagged Legacy in OBS 33 and will not load in OBS 34 (see
[OBS's plugin location guidance](https://obsproject.com/kb/legacy-plugin-developer-info)).
Portable OBS ignores ProgramData plugins.

**Moving an existing install:** close OBS and remove only the old Roadie DLL (from
`obs-plugins\64bit`, or the old ProgramData `roadie-signal\bin\64bit` folder), including renamed
copies. Keep one `roadie-signal.dll` with its original name. Your OBS settings and scenes are
untouched.

**Update:** from 1.0.0, press **Update** in the dock when Roadie offers one; it applies the next
time OBS starts. Coming from 0.3.x or earlier, install 1.0.0 by hand as above (both files).
**Uninstall** by closing OBS and removing both files. The Windows build is unsigned.

### After login and troubleshooting

The channel owner turns on **Share OBS state with Roadie** in Signal's gear. Open
**Connectors → OBS** in Roadie and switch a scene in OBS to check that the scene updates.
OBS WebSocket is not required.

If the dock is missing, check the OBS version, x64 package, destination folder and the loaded DLL
path in the OBS log, then close and reopen OBS. Do not add a Custom Browser Dock: the plugin
registers its own. **Tools → Roadie Signal…** shows the loaded version and dock status; if that
menu entry is absent, OBS did not load the file you copied. Since 0.1.12 the plugin also warns
when another Roadie Signal DLL is loaded beside it and names the file to delete.

## Install on a Mac

Download the [macOS release](https://github.com/m1st3r-t4c0/roadie-obs-connector/releases/tag/macos-v0.3.1-test.1),
open its ZIP and follow its `README.txt`: quit OBS, right-click `install.command`, choose
**Open**. It needs an Apple Silicon Mac and the OBS version named in the package's
`build-report.json`, or newer. The plugin is ad-hoc signed, not notarized, so macOS warns before
the first run. The Mac port is by **CtrlGremlin** (see [Credits](#credits)).

## 1.0.0: one-click updates

1.0.0 is the last version anyone installs by hand. After it, the dock's **Update** button fetches
a new version, verifies it, and runs it from the next OBS start, with automatic rollback if the new
version crashes OBS. Design: roadie `docs/specs/obs-plugin.md` §24.

**The install becomes two files:**

| File | What it is |
|---|---|
| `obs-plugins/64bit/roadie-signal.dll` | The **loader**, what OBS loads. Small, libobs only. It almost never changes; a loader change is a reinstall. |
| `data/obs-plugins/roadie-signal/core/roadie-signal-core.dll` | The **core**, the plugin itself, as the installer shipped it. Never beside the loader, so OBS never loads it as a plugin. |

Updated cores go in OBS's config folder for the plugin, `plugin_config/roadie-signal/cores/<version>/`,
with `update.json` beside them recording which core runs, which waits for the next start, the last
one that ran fine, and any start in progress.

**Update.** The dock sends `update-plugin { version }`. The core downloads
`manifest-<platform>.json`, its `.sig`, and the core named in the manifest from this repo's GitHub
release `v<version>`, using the system's own `curl` (OBS ships Qt's network library without TLS).
It checks the manifest's ECDSA P-256 signature against the key built into the plugin
(`src/update-key.hpp`) with the OS's own crypto (Windows CNG, macOS Security), then the core's
SHA-256 and size, then the lowest OBS it supports. Only then is the core staged for the next start.
The dock hears how it went through `roadie:obs-update`. **This is the plugin's only outbound
request, and it happens only when the streamer presses Update.** `revert-plugin` makes the next
start use the last good core.

**Rollback.** Before a core starts, the loader records it as starting. A core counts as good after
two minutes, or when OBS closes cleanly. A core whose starts never get there twice in a row is
marked bad and never run again; the last good one runs instead. A core that will not load at all
falls back in the same start. The installer's core is always there as the floor.

**Also in 1.0.0:** the `take-screenshot` command (first built as 0.3.4, never released), so
Roadie's _Take an OBS screenshot_ step no longer needs Firebot. See the [changelog](CHANGELOG.md#100--one-click-updates-with-crash-rollback).

**Status:** the loader's rollback rules (14 cases on headless libobs), the signature check and
`sign-core.mjs` are tested, and both DLLs build against OBS 30.0.0 and pass the loader check on
30.0.0 and 32.0.4. **Tested in OBS by t4c0 on 2026-11-04: it works.** An update from 1.0.0 to a
later release, and a rollback, can first be tried once that release is published. The macOS build is Roadie to-do #294; the Mac release stays 0.3.1 and updates by hand.

### Releasing a core

```powershell
node scripts/sign-core.mjs <version> <platform> <core> <out>
```

With `ROADIE_SIGNAL_SIGNING_KEY` set (from the KB's `.secrets`), this writes the three release
files: the manifest, its signature and the core. It refuses a key that does not match the one
built into the plugin.

## Settings

Everything lives in one window, **Tools → Roadie Signal…**:

- The running plugin, OBS and Qt versions, and browser and dock status.
- **Microphone level from:** one OBS audio source, or None (the default). Only peak/RMS levels
  reach the dock, every 100 ms; no audio is captured or uploaded.
- **Send OBS data to:** Production, Staging, or both.
- **Accept OBS changes from:** which servers' commands OBS follows (Production, Staging, or both).

Both server choices default to Production. A server that accepts changes but gets no OBS data
cannot see the scene list, so it offers nothing to switch; the window says so, and warns when
staging may change the live scene.

## Build from source

### Windows

```powershell
./scripts/prepare-sdk.ps1
# Skip if Visual Studio 2022 C++ tools are already installed.
./scripts/install-build-tools.ps1
./scripts/build-windows.ps1
```

Preparation verifies pinned public downloads under ignored `build/sdk/30.0.0`. It extracts the
official OBS 30 archive for import libraries and loader testing; it never installs or launches it,
and never changes an installed OBS. Build tools use portable CMake 3.30.5 plus MSVC 19.44 /
Windows SDK 10.0.26100.0. The Qt checksum comes from OBS 30's official `buildspec.json`, and the
official OBS executable's Authenticode signature is verified.

The build generates import libraries from the baseline OBS DLL export tables and includes official
SDK/browser declarations. It does not rebuild OBS or invent a CEF vtable.

Outputs:

- `stage/obs30.0.0/obs-plugins/64bit/roadie-signal.dll` (the loader)
- `stage/obs30.0.0/data/obs-plugins/roadie-signal/core/roadie-signal-core.dll` (the core)
- `stage/obs30.0.0/build-report.json` and DLL inspection reports
- `build/roadie-signal-<version>-obs30.0.0-minimum-windows-x64.zip`

Build scripts do not install into OBS, upload, sign, publish or deploy anything. Do not copy the
build SDK's OBS/Qt DLLs into your installation.

An optional loader probe uses standard-library Python on Windows. Run each OBS distribution in a
separate process to isolate Qt runtimes:

```powershell
python scripts/check-dll-load.py --dll stage/obs30.0.0/obs-plugins/64bit/roadie-signal.dll --obs-root build/sdk/30.0.0/obs-host
```

The trusted origin defaults to `https://dashboard.roadie.gg`; a local build can specify a loopback
HTTP origin. The staging origin is fixed at build time (`ROADIE_SIGNAL_STAGING_ORIGIN`,
`-StagingOrigin`) and must differ from the main origin; an empty value builds a production-only
plugin. No API keys are needed: sign-in uses the existing web session.

### macOS (Apple Silicon)

Same source, different build. On a Mac with Xcode Command Line Tools and OBS installed:

```bash
bash scripts/build-macos.sh            # build and package; installs nothing
bash scripts/build-macos.sh --install  # also install for this user (OBS must be closed)
```

The script links against the installed **OBS.app**'s own libobs, obs-frontend-api and Qt. It
fetches what the build needs into ignored `build/macos`: OBS's source at the tag matching OBS.app,
the matching `browser-panel.hpp`, Qt from obs-deps (hash-checked and refused when its minor version
differs from OBS.app's), SIMDe headers, and portable CMake when no CMake 3.28+ is installed. It
checks the binary is arm64 with only OBS.app dependencies, signs it ad-hoc, and packages
`stage/macos/roadie-signal-<version>-obs<obs>-macos-arm64.zip` with `SHA256SUMS-macos.txt`.

A Mac build supports the OBS version in its build report and newer, so build releases against the
oldest OBS.app they should support. Intel Macs are not supported.

## Compatibility

Target the oldest APIs a feature needs, not the builder's installed OBS. The dock
registration/removal API sets the minimum at **OBS 30.0.0**. The native browser interface is panel
API **version 3**, present in OBS 30.0.0, 32.0.4 and 32.2.1 with matching virtual method order.
At load the plugin checks OBS >= 30.0.0, Qt 6 with runtime >= 6.5.3, and browser panel API 3.
OBS owns its Qt/browser DLLs; the package ships no substitute runtime.

Windows x64 static and isolated DLL-loader checks:

| OBS distribution | Qt runtime | DLL dependency resolution | Actual OBS host behavior |
|---|---|---|---|
| 30.0.0 | 6.5.3 | passed | not tested |
| 32.0.4 | 6.8.3 | passed | not tested |
| 32.2.1 | 6.11.1 | passed | not tested |

The loader probe reads `obs_module_ver` and confirms the exported API minimum. It does not call
`obs_module_load`, create UI or start OBS, so it does not establish authentication, meter behavior
or callback/UI lifecycle.

## Acceptance checks still required

Web tests prove contract and freshness handling with fixture measurements. They do not prove the
native ABI, authentication, cookie persistence, callback lifetime or behavior in OBS. Native host
verification remains required:

1. Compile against the minimum SDK and inspect exports/dependencies (passed; verify the staged
   build report before installing).
2. Install into a test OBS. Confirm one dock, the Docks-menu toggle, docking, resizing,
   close/reopen, restart and normal Signal navigation/raid preemption.
3. Confirm sign-in and cookies survive a restart; test expired login, OAuth redirects, page reload
   and links that open Soundcheck.
4. Select a real microphone. Compare zero signal, speech, OBS mute, source rename/removal,
   scene-collection changes and a stopped audio device.
5. Confirm no audio reaches login/OAuth/other-origin or other-path pages, and that unload/shutdown
   removes callbacks and destroys the browser safely.
6. 1.0.0: installed and working in OBS (t4c0, 2026-11-04). Still to try: an update to the next
   release, and a rollback.

## Repository boundary

This repo owns the native wrapper, compiler/SDK preparation and packaging. Roadie
(`m1st3r-t4c0/roadie`) owns `/dock` and `apps/web/src/dock/native`. The
[bridge protocol](docs/bridge-protocol.md) is the contract between them; coordinate contract
changes across both repos.

## Credits

- **CtrlGremlin** ported the plugin to macOS: the CMake macOS target and `.plugin` bundle, the Mac
  build script, the Mac installer, and the diagnosis and fix of three crashes on quitting OBS on a
  Mac (see 0.3.3 in the [changelog](CHANGELOG.md)).

## License

**GPL-2.0-or-later**; see [LICENSE](LICENSE). The plugin links to OBS and includes an upstream
obs-websocket API header, whose copyright and GPL notice remain in
[third_party/obs-websocket-api.h](third_party/obs-websocket-api.h). The license covers this
repository's source and packaged plugin; Roadie's hosted web application is a separate project.
The checked-in release ZIPs include LICENSE.
