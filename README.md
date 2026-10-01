# Roadie Signal native OBS dock

Source version **0.1.6** is in development against the minimum
**OBS 30.0.0 / Qt 6.5.3** SDK. No OBS update is required for the reported issue on
OBS 32.0.4. Roadie's matching web receiver and server endpoint are live; the native
plugin must send state and the signed-in owner must enable sharing in Signal's gear.

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
running version under **Tools → Roadie Signal 0.1.6 status…**, even if dock startup
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
or publish it**. Research reproduced an access violation using OBS 32.0.4's real
browser library when a queued paint ran during parent-dock destruction. Current
source calls the public `closeBrowser()` method while the dock tree is still
alive, then destroys the widgets normally. The isolated probe passes that order;
the full plugin still needs integration validation. No replacement package has
been prepared. See [the reproducible probe](tests/browser-probe/README.md).

Authentication persistence is a separate web-side defect: Roadie issues a
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

The web receiver validates frames and freshness but does not yet render the
waveform or silence warning. The hosted web receiver remains unpublished, so
current hosted pages will not consume the native measurements. Native source
selection, microphone behavior, cookies and shutdown still require OBS host testing.

## Repository boundary

This repo owns the native wrapper, compiler/SDK preparation and packaging.
Roadie owns `/dock` and `apps/web/src/dock/native`. The version 1 event below is
the contract between them; coordinate contract changes across both repos.

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
- `build/roadie-signal-0.1.6-obs30.0.0-minimum-windows-x64.zip`

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

`scene` is null when OBS has no current scene. Frontend status is read on the Qt
timer, never in an audio callback. State changes arrive within one second; an
unchanged snapshot is sent every five seconds to recover after page reloads.
The script checks the top-level Roadie origin, exact `/dock` path and the state
provider marker. The web page alone owns the optional, owner-authenticated relay
to Roadie. Its gear switch defaults off and sends only these state fields, not
microphone data. OBS's built-in WebSocket server is not needed for this bridge.

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

Product scope: [Signal activity spec](https://github.com/m1st3r-t4c0/roadie/blob/main/docs/specs/signal-activity.md).
