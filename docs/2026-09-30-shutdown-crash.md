# OBS shutdown crash — 2026-09-30

The supplied OBS 32.0.4 crash report records an access violation in Qt Widgets
while `obs_frontend_remove_dock` destroys the Roadie dock's QCef browser widget.
Roadie calls that API from `OBS_FRONTEND_EVENT_EXIT`, which OBS emits after
`ClearSceneData()` during main-window shutdown. The stack directly implicates
this teardown path; it does not establish whether another plugin also affects
Qt's state.

Roadie now removes its dock on `OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN`. OBS 30's
shutdown source emits that event before `ClearSceneData()`, then emits `EXIT`.
The module-unload fallback remains idempotent. The new host report at 18:20:43
shows the same `obs_frontend_remove_dock` → `QCefWidgetInternal::closeBrowser`
stack while testing the published 0.1.4 DLL; it does not test the 0.1.5
shutdown change. A subsequent host screenshot confirms Roadie Signal 0.1.5
loaded and registered its dock in OBS 32.0.4 / Qt 6.8.3. The user reports that
OBS still crashes on close with 0.1.5. The 22:25:56 crash report confirms the
same call path: `obs_frontend_remove_dock` → `SignalDock` destructor →
`QCefWidgetInternal::closeBrowser` → Qt Widgets access violation. Changing the
shutdown event did not solve browser destruction.

The dock requests a dedicated CEF cookie manager with session-cookie persistence.
OBS 32.0.4 ignores that flag (confirmed below). In 0.1.4 the explicit cookie
flush ran after deleting the browser widget. Version 0.1.5 also flushes every
30 seconds while the dock is open and before browser destruction. This does
not turn Roadie's session-only cookie into a persistent cookie: the clean
restart probe below disproves the previous flushing-based auth fix. No scene,
microphone setting, or cookie directory changes are needed.

Compatibility: the event is present in the minimum OBS 30 API and uses the same
baseline SDK. Version 0.1.5 compiled with the pinned OBS 30 / Qt 6.5.3 SDK.
Isolated DLL-loader probes passed against OBS 30.0.0 and 32.0.4, retaining the
30.0.0 module minimum. These checks do not initialize OBS or close a dock. The
OBS 32.0.4 host check confirmed loading but did not clear the shutdown crash.
The first 0.1.6 candidate detached and leaked the browser until process exit.
It is withdrawn, remains unpublished, and must not be used for testing.

## October 1: source research and actual browser-library reproduction

OBS 32.0.4 pins obs-browser commit
`a776dd6a1a0ded4a8a723f2f572f3f8a9707f5a8`. Its
[public browser-panel API](https://github.com/obsproject/obs-browser/blob/a776dd6a1a0ded4a8a723f2f572f3f8a9707f5a8/panel/browser-panel.hpp)
exposes `closeBrowser()`. Its
[implementation](https://github.com/obsproject/obs-browser/blob/a776dd6a1a0ded4a8a723f2f572f3f8a9707f5a8/panel/browser-panel.cpp)
runs a nested Qt event loop until CEF finishes closing or the timeout expires.
The same implementation ignores `persist_session_cookies` and only configures
the request-context cache path.

The [isolated probe](../tests/browser-probe/README.md) loads the actual extracted
OBS 32.0.4 / Qt 6.8.3 libraries, with a fresh test profile and loopback HTTP page.
It does not load the user's configuration or use real authentication data.

- With no queued paint, both teardown orders completed. That simple test alone
  did not reproduce the host crash.
- With a queued paint of the live main window, parent-first deletion failed with
  `0xc0000005` in both runs. The browser close loop processed the paint during
  the parent destructor. Explicit browser close before parent deletion completed
  in both runs; a second run of the probe repeated these results.
- After clean pre-close and restart, the session-only synthetic cookie was gone
  and the synthetic `Max-Age` cookie survived. Flush returned success. The
  nested `roadie-signal/browser-cookies` path worked for the persistent cookie
  on this runtime; changing that path is not supported by this evidence.
- The same probe passed on OBS 30.0.0 / Qt 6.5.3: pre-close completed and only
  the persistent cookie survived restart. Parent-first deletion also completed
  on that older runtime; the reproduced crash is specific to the newer fixture.

Current source stops timers/audio, disconnects navigation callbacks, and calls
`closeBrowser()` before `obs_frontend_remove_dock`. A shutdown guard prevents
reentrant dock removal while CEF pumps events. The later widget destructor has
no live CEF browser left to close. It neither leaks the browser nor changes the
minimum OBS version. The full plugin host test remains unperformed.

The upstream [shutdown fix #13056](https://github.com/obsproject/obs-studio/pull/13056)
supports careful browser ownership and shutdown ordering, but its macOS-specific
quit changes are not proof of a Windows fix for Roadie. No OBS update is required
for the source-level approach being investigated here.

Roadie's `issueSessionCookie` currently sets HttpOnly, SameSite, path, signature,
and Secure flags, but no expiry. The supported auth direction is an expiring,
persistent dock session issued by Roadie, with signed server-enforced expiry and
normal sign-out/revocation. It must retain HttpOnly and avoid copying auth into
JavaScript or plugin settings. This web change has not yet been implemented.

## Stage latency: implementation still outstanding

The native connector samples status once per second, then the dock posts frames
to Roadie. Its web provider skips a frame while an earlier request is in flight;
it does not automatically resend the newest frame when that request completes.
The Connectors page's five-second status refresh is separate from Stage output.

The checked Stage scene-view path receives Firebot scene events, passes through
Roadie's server and scene mapping, then pushes `stage_view` to the browser source.
Native dock status routes do not drive that mapping. Stage has no local
transition-start handler, and some layout CSS transitions last 450–600 ms.
The observed four-second delay has not been instrumented end to end.

The intended fast path is local: OBS transition-start → browser-source event →
hide old Stage content; transition completion → apply a preloaded scene mapping
and reveal the new view. OBS 30 already provides `source_transition_start`,
`source_transition_video_stop`, and `source_transition_stop`. Its browser module
already registers the `obs-browser/emit_event` vendor request, callable through
obs-websocket's in-process API without a network connection. Built-in
`obsSceneChanged` can reconcile the final program scene. This is a design to
implement and measure, not a delivered sub-100-ms claim.

Keep telemetry off the rendering path, use transition IDs to reject stale
completion/cloud echoes, and handle cut, fade, stinger, interrupted transitions,
Studio Mode preview, source reload and reconnect. Measure callback → JS receipt
→ next animation frame, with a p95 under 100 ms target for the first visible
response. The OBS transition's configured duration remains separate. A 60-fps
rendering target gives roughly 16.7 ms per frame; it is not a network-latency fix.

The selected Roadie checkout is dirty and diverged. Integration work needs an
authorized isolated checkout; no unrelated Stage edits have been changed.
