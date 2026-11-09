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
| **Windows** (x64) | [Roadie Signal for Windows](https://github.com/m1st3r-t4c0/roadie-obs-connector/releases/latest) | OBS 30.0.0 or newer |
| **Mac with Apple Silicon** (M1 or newer) | [Roadie Signal 0.3.1 for macOS](https://github.com/m1st3r-t4c0/roadie-obs-connector/releases/tag/macos-v0.3.1-test.1) | OBS 32.2 or newer |
| **Intel Mac** | Not supported yet | |

Windows and Mac have separate releases, and the Mac build can be a version behind. The "Latest"
release on GitHub is always the Windows one. Older release tags keep their original `-test.N`
names, but those releases are no longer test builds.

## Install on Windows

The OBS connector is the recommended way to connect Roadie to your stream. Use the Firebot
plugin only if you plan to connect Roadie's queue system to Firebot.

Every Windows release has **two ZIPs**, one per OBS version. Check yours in **Help → About**, then
download that ZIP under the [latest release](https://github.com/m1st3r-t4c0/roadie-obs-connector/releases/latest)'s
**Assets** (not **Source code**). Each ZIP is laid out as the folder you extract it into. OBS 29
and earlier are not supported.

### OBS 33 and newer: `roadie-signal-<version>-windows-x64-obs33-and-newer.zip`

1. Close OBS.
2. Open `C:\ProgramData\obs-studio\plugins` in File Explorer (paste it into the address bar;
   create the `plugins` folder if it is missing).
3. Extract the ZIP there. It puts `roadie-signal.dll` in `roadie-signal` and
   `roadie-signal-core.dll` in `roadie-signal\data\core`.
4. Start OBS, open **Docks → Roadie Signal** and log in to Roadie.

### OBS 30 to 32: `roadie-signal-<version>-windows-x64-obs30-to-32.zip`

1. Close OBS.
2. Open `C:\Program Files\obs-studio` in File Explorer.
3. Extract the ZIP there (Windows asks for administrator permission). It puts `roadie-signal.dll`
   in `obs-plugins\64bit` and `roadie-signal-core.dll` in `data\obs-plugins\roadie-signal\core`.
4. Start OBS, open **Docks → Roadie Signal** and log in to Roadie.

**Tools → Roadie Signal…** then shows the OBS plugin and Roadie connector versions. **Never put
`roadie-signal-core.dll` beside `roadie-signal.dll`**: OBS would load it as a second plugin.

**Portable OBS:** extract the OBS 33 ZIP into `<portable OBS>\plugins`, or the OBS 30 to 32 ZIP
into `<portable OBS>`. Portable OBS ignores ProgramData plugins.

**OBS 33 and newer** flag the old Program Files location as Legacy, and OBS 34 will not load from
it (see [OBS's plugin location guidance](https://obsproject.com/kb/legacy-plugin-developer-info)).
Moving from it: close OBS, remove the old `roadie-signal.dll` from `obs-plugins\64bit` (and any
renamed copy), then install with the OBS 33 ZIP. Your OBS settings and scenes are untouched.

### Update

**From the dock:** press **Update** in the Roadie Signal dock when Roadie offers a new version,
then restart OBS. The Update button works from Roadie connector 1.1.1 and newer.

**By hand** (Roadie connector 1.0.0 or 1.1.0): close OBS, download the ZIP for your OBS version,
copy `roadie-signal-core.dll` from it over the one in this folder, and start OBS. The OBS plugin
file stays as it is.

| OBS | Copy `roadie-signal-core.dll` to |
|---|---|
| 33 and newer | `C:\ProgramData\obs-studio\plugins\roadie-signal\data\core` |
| 30 to 32 | `C:\Program Files\obs-studio\data\obs-plugins\roadie-signal\core` |

Coming from 0.3.x or earlier, do a new install with the ZIP for your OBS version.
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

From 1.0.0 the plugin is two files, and from connector 1.1.1 the dock's **Update** button fetches
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

**Update.** The dock sends `update-plugin { target }` to connectors that list `update-core` (1.1.1
and newer); 1.0.0 and 1.1.0 are updated by hand once. The core downloads
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

The macOS build is Roadie to-do #294; the Mac release stays 0.3.1 and updates by hand.

### Two numbers: the OBS plugin and the Roadie connector

Since 1.1.0 (t4c0, 2026-11-08) the two parts have their own numbers, and Tools → Roadie Signal…
and the dock's gear show both.

- **OBS plugin** (the loader, `ROADIE_SIGNAL_LOADER_VERSION` in `CMakeLists.txt`): what people
  install by hand. It changes only when a change needs a reinstall, which should be rare.
- **Roadie connector** (the core, the project version): what the Update button downloads. Every
  release raises it. It keeps the `x.y.z` form and the `v<x.y.z>` tag, because that is how the
  1.0.0 updater finds a release.

**Most new OBS features need no connector release at all.** `obs-request` runs any obs-websocket
request but a short block list (see [docs/bridge-protocol.md](docs/bridge-protocol.md#commands)), so
Roadie can read and change OBS in new ways with a Roadie deploy alone. A connector release is for
what obs-websocket cannot do.

### Releasing a connector

```powershell
node scripts/release.mjs prepare   # with ROADIE_SIGNAL_SIGNING_KEY set
# read releases/v<version>/README.md, then commit releases/v<version>
node scripts/release.mjs publish   # with GITHUB_TOKEN set
```

`prepare` builds from a clean tree, signs the core with `sign-core.mjs` (which refuses a key that
does not match the one built into the plugin), and writes `releases/v<version>/`, with notes taken
from the version's CHANGELOG section. `publish` tags that commit, pushes `main` and the tag,
creates the GitHub release with its files, and downloads the three update files back to check them.
Roadie reads the newest release from GitHub, so docks offer Update as soon as it is published. Both
keys come from the KB's `.secrets` through the environment, never as arguments.

**Every release ships the same way** (t4c0, 2026-11-09): two ZIPs, one for OBS 33 and newer and one
for OBS 30 to 32, and release notes from the one template in `release.mjs`, which always carry both
install sets and both update sets. Only *What's new* changes, taken from the CHANGELOG section; it
says what changed and never carries test status (`prepare` refuses a section that does). To
fix an already published release's notes or ZIPs, edit the template or CHANGELOG, run
`node scripts/release.mjs refresh`, commit, and run `publish` again: it replaces the notes and files
and keeps the signed update files as they are.

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

| OBS distribution | Qt runtime | DLL dependency resolution |
|---|---|---|
| 30.0.0 | 6.5.3 | passed |
| 32.0.4 | 6.8.3 | passed |
| 32.2.1 | 6.11.1 | passed |

The loader probe reads `obs_module_ver` and confirms the exported API minimum without starting OBS.

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
