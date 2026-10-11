# Roadie Signal native OBS dock

## Current test release

Version **0.1.10** is a Windows x64 test build targeting OBS **30.0.0 or newer**
and Qt **6.5.3 or newer**. The [0.1.10 package](releases/v0.1.10-test.1/README.md)
contains the standalone DLL, ZIP, checksum and build report. It is unsigned.
Dependency loading passed on OBS 30.0.0 and 32.2.1; full OBS host behavior,
sign-in persistence, microphone behavior and shutdown still need testing.

Close OBS before installing. Replace only `roadie-signal.dll` in OBS's
`obs-plugins/64bit` directory, restart OBS, then check
**Tools → Roadie Signal 0.1.10 status…** and **Docks → Roadie Signal**.
Do not leave an older renamed Roadie DLL alongside it. Roadie's hosted dock and
server changes deploy separately from this plugin.

## Earlier releases

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
running version under **Tools → Roadie Signal 0.1.10 status…**, even if dock startup
fails. The status dialog also reports the dock result. If that Tools entry is absent,
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
**Tools → Roadie Signal microphone…** selects one OBS audio source by UUID; None
is the default. Source rename preserves selection, and removal produces unavailable
measurements. Selection persists in OBS's plugin runtime configuration.

A native OBS meter supplies local peak/RMS measurements to the dock every 100 ms.
The callback updates a snapshot without UI, JavaScript, allocation or network work.
No raw microphone audio is captured or uploaded. Only the signed-in `/dock` provider
accepts measurements; navigation away detaches the tap.

The hosted web receiver validates frames and freshness but does not yet render the
waveform or silence warning. Native source selection, microphone behavior,
cookies and shutdown still require OBS host testing.

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
No supported-version claim extends to non-Windows or non-x64 builds.

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
can specify a loopback HTTP origin. Browser cookies live in OBS's browser-plugin config under the stable
`roadie-signal/browser-cookies` namespace; microphone settings remain in the
Roadie plugin's own config. No API keys are needed and sign-in uses the existing web session.

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

## 0.1.11 additions — a second dock for Roadie staging

**Tools → Roadie Signal: also report to staging** adds a second dock, **Docks → Roadie Signal
(Staging)**, on `https://staging.roadie.gg/dock`. It is off by default and the switch is saved in
the plugin's `settings.json` (`stagingDock`), so it comes back the next time OBS starts. Turning it
off closes and removes the dock at once.

The staging dock is a full second copy of the dock. It has its own trusted origin, its own
`roadie-signal-staging/browser-cookies` sign-in, and its own session id and sequences. It sends the
same `roadie:obs-state`, `roadie:obs-audio`, `roadie:obs-health` and `roadie:obs-crash` frames, so
every scene change reaches both servers. It also accepts `set-scene` from staging. **While the switch
is on, staging can change the live OBS scene.** Both docks use the one saved microphone selection.

The staging origin is fixed at build time (`ROADIE_SIGNAL_STAGING_ORIGIN`, `-StagingOrigin`), the
same as the main origin. It must differ from the main origin. An empty value builds without the
staging dock or its menu entry.

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
