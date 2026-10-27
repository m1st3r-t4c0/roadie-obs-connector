# Roadie Signal native OBS dock

## Download

| Your computer | Download | Needs |
|---|---|---|
| **Windows** (x64) | [Roadie Signal 0.3.3 for Windows](https://github.com/m1st3r-t4c0/roadie-obs-connector/releases/latest) | OBS 30.0.0 or newer |
| **Mac with Apple Silicon** (M1 or newer) | [Roadie Signal 0.3.1 for macOS](https://github.com/m1st3r-t4c0/roadie-obs-connector/releases/tag/macos-v0.3.1-test.1) | OBS 32.2 or newer |
| **Intel Mac** | Not supported yet | |

Windows and Mac have separate releases, and the Mac build can be a version behind. The "Latest"
release on GitHub is always the Windows one. The Mac release is named "for macOS" and has its
own install guide in the ZIP. The Mac port is by **CtrlGremlin** (see [Credits](#credits)).

Release tags keep their original `-test.N` names, but these releases are no longer test builds.

## Current release

Version **0.3.3** for Windows (tag `v0.3.3-test.1`) targets OBS **30.0.0 or newer** and Qt
**6.5.3 or newer**. The [0.3.3 package](releases/v0.3.3-test.1/README.md) contains the standalone
DLL, ZIP, checksum and build report. It is unsigned.
All settings live in one window, **Tools → Roadie Signal…**: the running version and dock
status, the microphone, and (since 0.1.11) where OBS data goes and whose scene changes OBS
follows: Production, Staging or both. From 0.1.12 the plugin also warns when another Roadie
Signal DLL is loaded beside it, and names the file to delete.

Close OBS before installing. Replace only `roadie-signal.dll` in OBS's
`obs-plugins/64bit` directory, restart OBS, then check
**Tools → Roadie Signal…** and **Docks → Roadie Signal**.
Do not leave an older renamed Roadie DLL alongside it. Roadie's hosted dock and
server changes deploy separately from this plugin.

## Earlier releases

Releases before 0.2.0 are test prereleases tagged `vX.Y.Z-test.N`. The
[0.1.12 test package](releases/v0.1.12-test.1/README.md) has the same source as 0.2.0.
The [0.1.10 package](releases/v0.1.10-test.1/README.md) carried only a version label over 0.1.9.

The 0.1.4 change added a second local bridge for OBS streaming, recording and
current-scene state. It publishes changed state within one second plus a five-second
heartbeat to the signed-in `/dock` page. The page's Signal gear can opt in to
sharing this narrow state with Roadie's server. No microphone measurements or
audio are sent with it. The [0.1.4 test package](releases/v0.1.4-test.1/README.md)
is available in this repository. The [0.1.5 test package](releases/v0.1.5-test.1/README.md)
is also available.

This version fixes a startup failure confirmed in an OBS 32.0.4 log: the module
loaded, but its browser cookie directory could not be created, leaving no dock.
The QCef cookie API requires a relative storage name, not an absolute Windows path.
See [the diagnosis and validation record](docs/cookie-directory-startup.md).
The September 30 host log still identifies the loaded plugin as **0.1.1**, so the
replacement DLL had not taken effect on that machine. The current build displays its
running version at the top of **Tools → Roadie Signal…**, even if dock startup
fails. The same window also reports the dock result. If that Tools entry is absent,
check the loaded DLL path rather than assuming the copied file is the one OBS uses.
The 0.1.4 test prerelease is published, and its shutdown crash was reproduced on
OBS 32.0.4. Version 0.1.5 attempts to remove the browser dock before OBS clears
scene/UI data during shutdown. A host screenshot confirms that 0.1.5 loaded and
registered the dock in OBS 32.0.4 / Qt 6.8.3, but the user reports another crash
on close. The new stack confirms the browser destructor still faulted in Qt.
Version 0.1.5 also flushes the dock's
cookie store periodically and before browser destruction, because
the 0.1.4 shutdown crash could skip the previous final flush. This does not
make session-only cookies persistent. See
[the shutdown diagnosis](docs/2026-09-30-shutdown-crash.md).
The 0.1.5 crash report confirms the same browser-destruction path. The first
[0.1.6 candidate](releases/v0.1.6-test.1/README.md) is **withdrawn; do not install
or publish it**. Its DLL and ZIP were removed from the current tree. Research reproduced an
access violation using OBS 32.0.4's real
browser library when a queued paint ran during parent-dock destruction. Current
source calls the public `closeBrowser()` method while the dock tree is still
alive, then destroys the widgets normally. The isolated probe passes that order;
the full plugin still needs integration validation. The 0.1.7 package includes
that fix. See [the reproducible probe](tests/browser-probe/README.md).

Authentication persistence required a separate web-side fix: Roadie issued a
session-only cookie, and OBS 32.0.4 ignores the cookie-manager persistence flag.
The isolated probe loses a session-only cookie after a clean restart while a
persistent cookie survives. Periodic cookie flushing alone cannot fix this.

The previous test downloads are
[v0.1.3-test.1](https://github.com/m1st3r-t4c0/roadie-obs-connector/releases/tag/v0.1.3-test.1),
[v0.1.2-test.1](https://github.com/m1st3r-t4c0/roadie-obs-connector/releases/tag/v0.1.2-test.1)
and [v0.1.1-test.1](https://github.com/m1st3r-t4c0/roadie-obs-connector/releases/tag/v0.1.1-test.1).
The 0.1.1 build contains the cookie-path bug. The older v0.1.0-test.1 DLL was tied to
OBS 32.2.1 / Qt 6.11.1 and cannot load on 32.0.4. Both are historical test builds.

## What it does

The module registers **Roadie Signal** in OBS's **Docks** menu and opens the
existing authenticated `/dock` web application. It is private workspace UI.
**Microphone level from** in **Tools → Roadie Signal…** selects one OBS audio source by UUID; None
is the default. Source rename preserves selection, and removal produces unavailable
measurements. Selection persists in OBS's plugin runtime configuration.

A native OBS meter supplies local peak/RMS measurements to the dock every 100 ms.
The callback updates a snapshot without UI, JavaScript, allocation or network work.
No raw microphone audio is captured or uploaded. Only the signed-in `/dock` provider
accepts measurements; navigation away detaches the tap.

The hosted web receiver validates frames and freshness but does not yet render the
waveform or silence warning. Native source selection, microphone behavior,
cookies and shutdown still require OBS host testing.

## Credits

- **CtrlGremlin** ported the plugin to macOS: the CMake macOS target and `.plugin` bundle, the
  Mac build script, the Mac installer, and the diagnosis and fix of three crashes on quitting
  OBS on a Mac (see 0.3.3 below). The first Mac release is built from that work.

## Repository boundary

This repo owns the native wrapper, compiler/SDK preparation and packaging.
Roadie owns `/dock` and `apps/web/src/dock/native`. The version 1 event below is
the contract between them; coordinate contract changes across both repos.

## License

This connector is licensed under **GPL-2.0-or-later**; see [LICENSE](LICENSE).
It links to OBS and includes an upstream obs-websocket API header. That header's
copyright and GPL notice remain in
[third_party/obs-websocket-api.h](third_party/obs-websocket-api.h).
The license applies to this repository's source and packaged plugin; Roadie's hosted
web application is a separate project.
The checked-in release ZIPs include LICENSE. Their DLLs and build reports are
unchanged; checksums are recorded beside each package.

## Minimum compatibility policy

Target the oldest APIs required by the feature instead of the builder's installed
OBS version. The dock registration/removal API sets the OBS minimum at **30.0.0**;
source UUID lookup is available earlier. The native browser interface used here
is panel API **version 3**, present in the matching OBS 30.0.0, 32.0.4 and 32.2.1
sources. The required virtual method signatures/order match across those headers.

The plugin checks OBS >= 30.0.0, Qt major 6 with runtime >= 6.5.3, and browser panel
API version 3. It no longer requires an exact OBS release. Compiling and linking
against the minimum distribution avoids importing newer OBS/Qt symbols. OBS owns
its Qt/browser DLLs; this package contains no substitute OBS or Qt runtime.
Future versions or different browser builds still need compatibility verification.
The table below is for Windows x64. The macOS build has its own, narrower claim (see
[macOS build](#macos-build)).

Static Release and isolated Windows DLL-loader checks passed on:

| OBS distribution | Qt runtime | DLL dependency resolution | Actual OBS host behavior |
|---|---|---|---|
| 30.0.0 | 6.5.3 | passed | not tested |
| 32.0.4 | 6.8.3 | passed | not tested |
| 32.2.1 | 6.11.1 | passed | not tested |

The loader probe reads `obs_module_ver` and confirms the exported API minimum is
30.0.0. It does not invoke `obs_module_load`, create UI or start OBS. The previous
0.1.0 DLL fails the same 32.0.4 probe with Windows error 127. Compilation and loading
checks do not establish authentication, meter behavior or callback/UI lifecycle.

## Windows build

```powershell
./scripts/prepare-sdk.ps1
# Skip if Visual Studio 2022 C++ tools are already installed.
./scripts/install-build-tools.ps1
./scripts/build-windows.ps1
```

Preparation verifies pinned public downloads under ignored `build/sdk/30.0.0`.
It extracts the official OBS 30 archive for import libraries and loader testing;
it never installs or launches it, and never changes an installed OBS. Build tools
use portable CMake 3.30.5 plus MSVC 19.44 / Windows SDK 10.0.26100.0.

The Qt checksum comes from OBS 30's official `buildspec.json`; the official OBS
executable's Authenticode signature is also verified. Source, browser-header and
historical OBS archive hashes lock the retrieved bytes; that release did not expose
an upstream archive digest. The original SDK lock is retained for build provenance.

The build generates import libraries from the baseline OBS DLL export tables and
includes official SDK/browser declarations. It does not rebuild OBS or invent a
CEF vtable. Browser objects use the matching factory and virtual destructors.
MSVC environment values remain internal and are never printed.

Outputs:

- `stage/obs30.0.0/obs-plugins/64bit/roadie-signal.dll`
- `stage/obs30.0.0/build-report.json` and DLL inspection report
- `build/roadie-signal-0.1.10-obs30.0.0-minimum-windows-x64.zip`

Build scripts do not install into OBS, upload, sign, publish or deploy anything.
For manual testing, close OBS, replace only `roadie-signal.dll` in your installation's
`obs-plugins/64bit` folder, then reopen OBS and enable **Docks → Roadie Signal**.
To uninstall, remove that DLL with OBS closed. Do not copy the build SDK's OBS/Qt
DLLs into your installation. This is an unsigned test package.

An optional developer loader probe uses standard-library Python on Windows:

```powershell
python scripts/check-dll-load.py --dll stage/obs30.0.0/obs-plugins/64bit/roadie-signal.dll --obs-root build/sdk/30.0.0/obs-host
```

Run each distribution in a separate process to isolate Qt runtimes. The 32.0.4
fixture was verified against its official release digest. Actual tests belong in
OBS on the streaming machine after dependency validation.

The trusted origin defaults to `https://dashboard.roadie.gg`. A separate local build
can specify a loopback HTTP origin. Browser cookies live in OBS's browser-plugin config in the
`roadie-signal-cookies` folder (since 0.3.2; see below); microphone settings remain in the
Roadie plugin's own config. No API keys are needed and sign-in uses the existing web session.

## macOS build

From 0.3.3 the plugin also builds for **Apple Silicon Macs** (arm64). It is the same source;
only the build differs. On a Mac with Xcode Command Line Tools and OBS installed:

```bash
bash scripts/build-macos.sh            # build and package; installs nothing
bash scripts/build-macos.sh --install  # also install for this user (OBS must be closed)
```

The script links against the installed **OBS.app**'s own libobs, obs-frontend-api and Qt,
instead of building OBS. It fetches what the build needs into ignored `build/macos`:

- OBS's source at the tag matching OBS.app, for headers.
- `browser-panel.hpp` from the obs-browser commit that tag pins.
- Qt from obs-deps, checked against the hash in that tag's `buildspec.json`, and refused when its
  minor version differs from the Qt inside OBS.app.
- SIMDe headers, which OBS 31+ libobs headers include but the OBS tree no longer carries.
- Portable CMake 3.30.5 (hash-checked) when no CMake 3.28+ is installed.

It then checks that the binary is arm64, exports the OBS module functions, and depends on
nothing OBS.app does not provide, signs it ad-hoc, and packages
`stage/macos/roadie-signal-<version>-obs<obs>-macos-arm64.zip` (plugin, `install.command`,
README, LICENSE, build report) with `SHA256SUMS-macos.txt`.

**Compatibility is narrower than on Windows.** The plugin needs the Qt it was built against or
newer, so a Mac build supports the OBS version in its build report and newer. Build releases
against the oldest OBS.app they should support. The Mac package is ad-hoc signed, not notarized,
so macOS warns on first open; the installer clears the quarantine flag for the plugin only.
Intel Macs are not supported.

## Local bridge, version 1

The event is `roadie:obs-audio`, with a JSON object as CustomEvent.detail:

```json
{
  "version": 1,
  "sessionId": "plugin-instance-id",
  "sequence": 1,
  "status": "ready",
  "source": { "id": "obs-source-uuid", "name": "Mic/Aux" },
  "peak": 0.4,
  "rms": 0.1,
  "sampleAgeMs": 10
}
```

`ready` requires finite linear levels in [0, 1], a source and a sample no older
than 1000 ms. Zero is a valid reading. Other states are `muted`, `unavailable`
and `unselected`, with null levels and sample age. `unselected` has a null
source; `muted` requires a source. Every frame has a monotonic sequence within
one plugin session. The receiver expires a missing bridge after 2000 ms and
expires sample freshness independently; duplicate/invalid data cannot refresh it.

Before dispatch, every script checks the actual top-level page's origin, exact
`/dock` path and a readiness marker set only by the signed-in dock provider. Navigation away detaches the audio tap; navigating back re-resolves
the saved source. No privileged page-to-native command interface is exposed.
These checks isolate this one-way telemetry; they are not a sandbox against a
compromised trusted Roadie page. Source names and readings do not enter cloud
connector history.

## Local OBS state bridge, version 1

The second CustomEvent is `roadie:obs-state`. It uses the same plugin session id
and its own monotonic sequence:

```json
{
  "version": 1,
  "sessionId": "plugin-instance-id",
  "sequence": 1,
  "streaming": false,
  "recording": false,
  "scene": "Gameplay"
}
```

`scene` is null when OBS has no current scene. Frontend callbacks queue status
delivery on the Qt thread, never in an audio callback. Changes are sent immediately;
a snapshot is also sent every five seconds for presence and page reload recovery.
The script checks the top-level Roadie origin, exact `/dock` path and the state
provider marker. The web page alone owns the optional, owner-authenticated relay
to Roadie. Its gear switch defaults off and sends only these state fields, not
microphone data. OBS's built-in WebSocket server is not needed for this bridge.

## 0.1.8 additions — version, stream health and crash reports

All three are additive. A Roadie page that predates them drops the unknown fields
and ignores the new events, so 0.1.8 is safe against an older hosted web app.

**`roadie:obs-state` gains `pluginVersion` and `obsVersion`** (strings), so the dock
can show which build is running and offer an update when Roadie lists a newer one.

**`roadie:obs-health`** (marker `roadieObsHealth`), every five seconds while
streaming and once more on `STREAMING_STOPPING`, from the Qt thread:

```json
{
  "version": 1,
  "sessionId": "plugin-instance-id",
  "sequence": 1,
  "sampledAt": 1234567,
  "streaming": true,
  "outputFrames": 54000, "droppedFrames": 12, "bytesSent": 1350000000,
  "congestion": 0.02, "reconnecting": false,
  "renderFrames": 54010, "laggedFrames": 3,
  "encodedFrames": 54000, "skippedFrames": 0
}
```

Counters are cumulative since the output started; `sampledAt` is a monotonic
millisecond clock, not wall time. The plugin keeps no history. Roadie's server
folds consecutive samples into per-minute deltas for the stream recap. With no
active output the frame is just `streaming: false`.

**`roadie:obs-crash`** (marker `roadieObsCrash`), with the first three state
heartbeats after each trusted page load: OBS's own crash reports from the last 14
days, newest first, at most three, read once per OBS session from the `crashes`
folder beside this plugin's config root (portable installs included).

```json
{
  "version": 1, "sessionId": "...", "pluginVersion": "0.1.8", "obsVersion": "32.0.4",
  "reports": [{ "fileName": "Crash 2026-09-30 21-14-05.txt", "modifiedAt": "2026-09-30T21:14:05Z",
                "sha256": "...", "truncated": false, "text": "..." }]
}
```

Each report is capped at 64 KB. `C:\Users\<name>` paths are replaced with
`C:\Users\<user>` before hashing, so the hash is of what Roadie receives. The
page sends them to Roadie only while its crash-report switch is on; Roadie
dedupes by `sha256`. The plugin itself makes no network request.

## 0.1.9 additions — scene switching from Roadie

**`roadie:obs-state` gains `scenes` and `capabilities`.** `scenes` is OBS's scene names in OBS's
order, at most 200; `capabilities` is `["set-scene"]`. Both additive.

**The page can now ask the plugin to switch scene.** `browser-panel.hpp` gives the plugin no
JavaScript-to-native call, only the `titleChanged` and `urlChanged` signals, so the trusted `/dock`
page writes a command into `document.title`:

```text
roadie-obs-command:{"version":1,"id":"<1-64 of A-Z a-z 0-9 ->","action":"set-scene","scene":"BRB"}
```

The plugin reads it only while the page is trusted (exact origin and `/dock`), ignores a title
without the prefix or over 4 KB, runs each `id` once (CEF may report one title twice, and the page
holds the title until answered), and searches only `obs_frontend_get_scenes`, so a command can never
select a source that is not a scene. It switches with `obs_frontend_set_current_scene` on the Qt
thread and answers with **`roadie:obs-command-result`** (marker `roadieObsCommand`):

```json
{ "version": 1, "id": "...", "action": "set-scene", "ok": true }
```

or `"ok": false` with `"error": "no-such-scene"` / `"unknown-action"`. The page restores its title
after the answer, or after four seconds without one. The plugin still makes no network request; the
command reaches the page from Roadie over the page's own signed-in socket.

## 0.3.0 additions — every Roadie OBS action

Roadie's automation can set a text source, show or hide a source, mute or unmute an input, and turn
a filter on or off through the plugin, so a channel needs no bot connected to OBS for them (roadie
`docs/specs/obs-plugin.md` §19).

**New commands**, on the same title channel and with the same trust checks as `set-scene`:

```text
{"action":"set-source-visibility","scene":"Gameplay","source":"Cam","visible":false}
{"action":"set-text","source":"Latest follower","text":"Thanks, nacho"}
{"action":"set-mute","input":"Mic/Aux","muted":true}
{"action":"set-filter","source":"Cam","filter":"Blur","enabled":true}
```

Each runs on the Qt thread, changes OBS, and **reads the change back before it answers**, so
`"ok": true` means OBS holds the new value. A source in a group inside the scene is found too
(`obs_scene_find_source_recursive`). `set-text` changes only a GDI+ or FreeType text source, and
refuses one set to read from a file, since that text would never show. Errors: `no-such-scene`,
`no-such-source`, `no-such-filter`, `not-a-text-source`, `text-from-file`, `not-an-audio-input`,
`not-applied`, `bad-command`, `unknown-action`, `commands-off`.

**Long commands come in parts.** Chromium cuts a document title at 4096 characters, and a text
command carries up to 4000. The page then writes `roadie-obs-command-part:` titles:

```text
roadie-obs-command-part:{"version":1,"id":"...","index":0,"count":3,"data":"<a slice of the command JSON>"}
```

The plugin acknowledges each part with **`roadie:obs-command-part`** (`{"version":1,"id","index"}`,
marker `roadieObsCommand`), and the page writes the next only after that acknowledgement. Parts arrive
in order; at most 16, 32 KB in all. The joined JSON is then checked and run like a one-title command.

**`roadie:obs-state` gains `sources` and `filters`**, the names Roadie's OBS steps pick from:
`sources` is `[{"name","kind","scenes":[...]}]` (OBS's input id as `kind`, empty for a scene or
group, at most 400) and `filters` is `[{"source","filter"}]` (at most 400). `capabilities` lists all
five commands. All additive.

The **Tools → Roadie Signal…** choice is now labelled **Accept OBS changes from**; it governs every
command, not only scene switches. The saved setting (`acceptFrom`) is unchanged.

## 0.3.3 — the first macOS build, and no crash when quitting OBS on a Mac

0.3.3 adds the macOS build above, contributed by **CtrlGremlin**, together with a fix for three
crashes CtrlGremlin found in macOS crash reports when quitting OBS with the dock open:

1. The 100 ms audio publish kept calling into the dock page during shutdown and while OBS's quit
   dialog was open, after CEF had already dropped the page's main frame.
2. Resuming those calls after a fixed delay was unsafe while the quit dialog stayed open.
3. With the dock floating, Cmd+Q closes the dock's window before OBS's scripting-shutdown event,
   so the later browser close touched a freed view.

On macOS the plugin now stops calling into the page once OBS starts closing, under any modal
dialog (including native alerts) and inside nested event loops, resumes only when a quit is
cancelled, and closes its pages on Quit while every window still exists (unless an output is
running, when OBS asks first and the normal shutdown runs).

**Windows is unchanged.** It already closes the pages at scripting shutdown, before OBS removes
the dock (see `docs/2026-09-30-shutdown-crash.md`), and no Windows crash report has shown a call
into the page during shutdown. Pausing under every dialog there would drop scene events and
command results for no gain. The Windows DLL differs from 0.3.2 only in its version.

Verified here: the Windows build and DLL-loader probes, and that CtrlGremlin's patches apply to
0.3.1's source. **Not verified here:** the Mac build itself, the quit crashes being gone, and the
dock on a Mac. Those need a Mac build and OBS host testing.

## 0.3.2 — the sign-in survives a restart on OBS 33

On OBS 33 the dock asked for a sign-in at every start. OBS 33's browser moved to CEF's Chrome
runtime, which keeps cookies only for a store whose folder sits **directly** in obs-browser's
config folder; a deeper folder is silently treated as incognito and nothing is saved (upstream
obs-browser `bb9b32e`, *Enable support for CEF 6613+ and Chrome Runtime*). The plugin's store was
`roadie-signal/browser-cookies`, one level too deep. OBS migrated its own service logins, not
other plugins'.

- The store is now **`roadie-signal-cookies`**, a direct child, on every OBS version.
- On the first start without it, the plugin copies the old store across before opening it (into a
  `.copying` folder, then renamed into place, so a failed copy leaves nothing half-made). An
  existing sign-in on OBS 30–32 carries over. The old folder is left alone, so an older build still
  finds its sign-in.
- The log says `sign-in moved to the roadie-signal-cookies store` once, or that the copy failed
  and one more sign-in is needed. No paths or cookies are logged.

Verified here: the build and the DLL-loader probes. **Not verified here:** a sign-in surviving an
OBS 33 restart, and the carried-over sign-in on OBS 32. Both need OBS host testing.

## 0.3.1 additions — OBS's source numbers, for the Firebot import

A Firebot effect often names an OBS source by its number in a scene alone. Roadie's Firebot import
finds the name from the plugin, so a channel needs no Firebot connector for it (roadie
`docs/specs/obs-plugin.md` §20).

Each entry in `roadie:obs-state`'s `sources` may now carry:

- **`items`**: `[{"scene","id"}]`, one per place the source sits. `id` is OBS's own scene item
  number (`obs_sceneitem_get_id`), and `scene` is the scene or group that number belongs to. At most
  200 per source.
- **`durationMs`**: a media source's length, when OBS has its file open. It is left out when OBS
  does not know it, such as a source set to _Close file when inactive_ that is hidden.

Both are additive. A Roadie server that does not read them ignores them.

## 0.1.11 additions — choose Production, Staging or both

**Tools → Roadie Signal…** sets two choices, each Production, Staging, or Production and
staging:

- **Send OBS data to.** Which servers get `roadie:obs-state`, `roadie:obs-audio`,
  `roadie:obs-health` and `roadie:obs-crash`.
- **Accept scene changes from.** Which servers' `set-scene` commands OBS follows. Any other server
  is answered `{"ok": false, "error": "commands-off"}` at once, and its `obs-state` carries empty
  `capabilities`, so Roadie there offers no scene switching.

Both default to Production, the behaviour before 0.1.11. The choices are saved in the plugin's
`settings.json` (`sendTo`, `acceptFrom`: 1 production, 2 staging, 3 both) and apply at once. A server
in neither choice has no page. A server in either has its own Roadie page (staging's is
`https://staging.roadie.gg/dock`), with its own trusted origin, its own sign-in
(`roadie-signal-staging/browser-cookies` for staging), and its own session id and sequences. In 0.1.11
each page was a separate dock; since 0.1.12 both live in the one **Roadie Signal** dock. Every page
uses the one saved microphone selection.

A server that accepts scene changes but gets no OBS data cannot see the scene list, so Roadie there
offers nothing to switch. The dialog says so, and warns when staging may change the live scene.

The staging origin is fixed at build time (`ROADIE_SIGNAL_STAGING_ORIGIN`, `-StagingOrigin`), the
same as the main origin. It must differ from the main origin. An empty value builds a
production-only plugin with no servers menu.

## 0.1.12 additions — one settings window

The Tools menu has one Roadie entry, **Roadie Signal…**. Its window shows the version, OBS
and Qt versions, browser and dock status, and holds the microphone and server choices. It
replaces the separate status, microphone and servers entries of 0.1.11.

At startup the plugin lists every other loaded OBS module whose description starts with
"Roadie Signal" or whose file name contains `roadie-signal`. If it finds one, it logs each
path, shows a warning naming the files to delete, and repeats it in the settings window.
Plugins before 0.1.12 do not check, so an older copy is reported only by a newer one.

**One page, set from the plugin menu.** The **Roadie Signal** dock shows one page: production's,
or staging's when staging is the only server. With both on, the staging page runs out of sight
behind it (stacked with `QStackedLayout::StackAll`, so it stays a visible widget whose browser
loads and keeps sending). There are no tabs; **Tools → Roadie Signal…** is the only setting.

- **One cookie store for every page** (`roadie-signal-cookies` since 0.3.2,
  `roadie-signal/browser-cookies` before). Roadie's session cookie is per site, so the servers never see each other's;
  the Twitch login the pages share lets the hidden page sign in without a click.
- **The hidden page signs itself in.** Every five seconds, at most one attempt a minute, the plugin
  checks the trusted `/dock` page: if it shows Roadie's sign-in screen (no `roadieObsState`
  marker), it follows the page's own Twitch login link.
- **The plugin menu is the consent.** Every `roadie:obs-state` frame carries `share: true`, because
  state reaches a page only when its server is chosen under **Send OBS data to**. Roadie (dock
  build with `sharingFromPlugin`) then shares without the gear's switch, which shows as on and set
  in OBS. An older Roadie ignores the mark and still waits for the switch.
- **If Twitch wants a click** (the first approval of a Roadie server), the settings window lists
  where the hidden page is and offers **Show the Staging sign-in**, which brings it forward under a
  "Signing in…" bar; it goes back out of sight once it returns to Roadie, or on **Done**.

## Acceptance checks still required

1. Compile against the minimum SDK and inspect exports/dependencies — passed; verify the staged build report before installing.
2. Install into a test OBS setup. Confirm one dock, Docks-menu toggle, docking,
   resizing, close/reopen, restart and normal Signal navigation/raid preemption.
3. Confirm sign-in and cookies survive restart; test expired login, OAuth redirects,
   page reload and links that open Soundcheck.
4. Select an actual microphone. Compare real zero signal, speech, OBS mute,
   source rename/removal, scene-collection changes and a stopped audio device.
5. Confirm no audio is delivered to login/OAuth/other-origin or other-path pages,
   and that unload/shutdown removes callbacks and destroys the browser safely.
6. Review the native source-selection dialog; implement/review the deferred
   microphone UI and agree silence warnings before enabling those features.

Web tests prove contract/freshness handling using fixture measurements. They do
not prove the native ABI, authentication, cookie persistence, callback lifetime
or behavior in OBS. Native host verification remains required.

The bridge contracts above describe this plugin’s integration with Roadie.
The hosted dock and server are maintained separately.


## Stage transition bridge

Introduced in 0.1.7, the current source combines the pre-close shutdown fix
with local Stage transition events. It attaches directly to the main program's
transition start/video-stop
signals, including private sources, and reattaches on output channel changes, then
calls the obs-browser `emit_event` vendor endpoint via obs-websocket's in-process
API. No localhost server setting, password or network connection is required.
The event carries only scene, phase, session/sequence/transition IDs and callback
time. A five-second state heartbeat recovers browser reloads; progress heartbeats
keep long/manual transitions active. Scene completion recovers a missed end; the
web receiver reveals output after 15 seconds without transition progress or completion.
Callbacks enqueue delivery on the Qt thread and are removed before dock shutdown.

Native streaming/recording/scene status now publishes from frontend callbacks;
the timer is only a five-second heartbeat. Roadie's matching implementation adds
persistent 30-day dashboard/dock login and cached Stage views. Those web changes
must deploy before the full behavior is available. The native wrapper alone
cannot change Roadie's cookie expiry policy.

Built with OBS 30.0.0 / Qt 6.5.3. Dependency-loader checks pass on OBS 30.0.0 and
32.0.4. `tests/scene-events` exercises real libobs signals and Qt dispatch with a
recording vendor endpoint: preview exclusion, ordered start/end IDs, stale end
filtering and callback removal pass on both runtimes. It does not load CEF or
measure GPU/host latency. The previous actual-browser shutdown probe remains
available under `tests/browser-probe`.

The web receiver exposes its last 50 callback-to-animation-frame samples and
start-only p95 at `window.roadieObsTiming`. The target is under 100 ms for the
first visual reaction. Actual OBS rendering performance and full-plugin restart
behavior remain unverified. The 0.1.7 package was a prerelease for host
validation. The old 0.1.6 candidate
remains withdrawn and must not be used.
