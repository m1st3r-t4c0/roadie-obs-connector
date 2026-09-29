# Roadie Signal native OBS dock

Development test build, version 0.1.0. **Compiled and published as an unsigned test
prerelease; not installed or verified in OBS.** The compatibility target is Windows
x64 / OBS 32.2.1. The web receiver remains an unpublished local change in Roadie.

Download the [v0.1.0-test.1 prerelease](https://github.com/m1st3r-t4c0/roadie-obs-connector/releases/tag/v0.1.0-test.1)
on the testing machine while signed into GitHub with access to this private repo.
The ZIP, standalone DLL and SHA256SUMS.txt were published on 2026-09-29. The exact
compiled source is commit `4a23c8c5f500fa1e378b3bafebd7c4014803123f`; GitHub asset
digests match the local package checksums. This is a test handoff, not a stable release.

## What it does

The module registers **Roadie Signal** in OBS's **Docks** menu. Its browser opens
the existing authenticated Roadie `/dock` application; Signal continues to ship
as the web app. The dock is private OBS workspace UI, never a scene source.

**Tools → Roadie Signal microphone…** chooses one OBS audio source. None is the
default and switches sharing off. Selection is persisted by source UUID in OBS's
plugin config; renaming the source does not change the selection, and deleting it
produces unavailable data. The dialog is initial implementation and has not been
reviewed on-screen.

A native OBS volume meter supplies a peak/RMS envelope. A 100 ms GUI timer sends
versioned local measurements to this dock's page using its browser widget's
JavaScript execution interface. The audio callback only updates a small snapshot
with a non-blocking lock. It never executes JavaScript, allocates UI objects or
makes network calls. No raw microphone audio is captured or uploaded. Actual
meter behavior relative to filters/faders and mute still needs host verification.

The current web implementation mounts an audio provider on `/dock` only. It
validates input, rejects duplicates, bounds envelope history to 32 peaks and marks
stale samples unavailable. **It does not yet render the deferred waveform or
silence warning.** Ordinary browser docks and the Soundcheck preview retain their
existing behavior. Silence alert thresholds remain an open product decision.

## Repository boundary

This repository owns the native wrapper, compiler/SDK preparation and packaging.
The hosted `/dock` UI and `apps/web/src/dock/native` receiver stay in the Roadie
application. The version 1 event below is the contract between the two repositories;
coordinate changes to it across both before publishing.

## Windows build

The initial test target is **OBS 32.2.1, Windows x64, Qt 6.11.1**. This build rejects
other OBS versions at module load and requires browser panel API version 3. Broader
compatibility needs host verification and matching headers; do not update OBS just
to build this plugin.

```powershell
# Public downloads, locked hashes, ignored cache. No administrator access needed.
./scripts/prepare-sdk.ps1

# Microsoft C++ Build Tools + Windows SDK. Windows asks for administrator approval.
# Skip if Visual Studio 2022 C++ tools are already installed.
./scripts/install-build-tools.ps1

# Compile Release, inspect exports/dependencies and stage the plugin. No OBS install.
./scripts/build-windows.ps1
```

The preparation script downloads official OBS 32.2.1 source, its exact obs-browser
panel header, OBS's matching Qt bundle and portable CMake 3.30.5. The SDK lock file
records SHA-256 checksums. Qt and CMake hashes were verified against official
upstream metadata; OBS source/header hashes lock the initially retrieved bytes.
The Microsoft installer helper checks its pinned bootstrapper hash and Authenticode
signature. Component versions come from Microsoft's installer channel; this is
not a hermetic compiler snapshot.

The build script resolves the installed VS 2022 x64 compiler and uses the installed
OBS DLL export tables to generate local import libraries. CMake uses the official
OBS headers and a generated `obsconfig.h`; it does not rebuild OBS or substitute CEF
vtable declarations. The Qt SDK version is checked against the installed OBS Qt DLL.
OBS binaries and SDK assets are never committed or included in the plugin package.

The browser factory and lifecycle were checked against the matching upstream
implementation: the factory returns a new `QCef`, the cookie manager has a virtual
destructor, and destroying the browser widget closes its browser. Native compilation
and actual unload behavior remain separate verification steps.

The output is `stage/obs-plugins/64bit/roadie-signal.dll`, a build report and DLL
inspection report. Build scripts do not install into OBS, upload, sign, publish or
deploy anything. For manual host testing, close OBS, copy the staged DLL into your
OBS installation's `obs-plugins/64bit` directory, then reopen OBS. Remove that DLL
with OBS closed to uninstall. The test build is unsigned.

Current local validation (2026-09-29): the user installed Microsoft C++ Build Tools
successfully. Release compilation passed using MSVC 19.44 and Windows SDK
10.0.26100.0. DLL dependency, x64 architecture and OBS module-export checks are
part of the build script. Host behavior is unverified; the user will test in OBS.
The earlier installer cancellation is resolved and no Codex restart was needed.

The build also creates `build/roadie-signal-0.1.0-obs32.2.1-windows-x64.zip` with
the plugin folder layout, this README and a source/hash build report. The package
is for manual testing only. Without publication of Roadie's matching web provider,
the hosted page will not consume native audio measurements. Dock loading and
sign-in can still be checked; no microphone waveform or silence warning is shown.

The default origin is `https://dashboard.roadie.gg`. A separate local build may
set `ROADIE_SIGNAL_ORIGIN` to a loopback HTTP origin. Browser cookies live under
the plugin's OBS runtime config directory. No API keys are required by the
plugin; sign-in remains the ordinary web session.

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

## Acceptance checks still required

1. Compile a Release DLL with the matching SDK — passed locally; verify the staged build report before installing.
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
