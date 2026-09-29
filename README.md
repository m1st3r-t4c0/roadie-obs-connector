# Roadie Signal native OBS dock

Development source, version 0.1.0. **Not yet compiled, installed, released or
verified in OBS.** The initial compatibility target is Windows x64 / OBS 32.2.1.
The browser-panel SDK and compiler are build prerequisites; no plugin DLL is
included or claimed.

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

## Build prerequisites

Use a prepared, version-matched OBS developer SDK containing:

- `libobs` and `obs-frontend-api` CMake packages/import libraries;
- OBS-compatible Qt 6 Widgets and a Windows x64 C++ toolchain;
- the matching obs-browser `browser-panel.hpp` and its required local headers.

The SDK browser declarations are included directly; this project does not
reconstruct a private CEF vtable or download build dependencies. Installed OBS
binaries alone do not provide development headers. Verify browser panel ABI,
factory ownership, cookie-manager lifetime and supported OBS versions against
the matching upstream source before compiling/releasing. A header from another
OBS/browser version is not interchangeable.

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="<prepared-sdk>" -DOBS_BROWSER_PANEL_INCLUDE_DIR="<matching-panel-headers>"
cmake --build build --config Release
cmake --install build --config Release --prefix stage
```

The staging directory contains `obs-plugins/64bit/roadie-signal.dll`. No install
into a running OBS, downloads, release signing, updater or production deployment
is performed by these build commands. Close OBS before a separately authorized
installation.

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

1. Verify the matching SDK/CEF ownership API and compile a Release DLL.
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
