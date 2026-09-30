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
stack while testing the published 0.1.4 DLL; it does not test the unreleased
0.1.5 shutdown change.

The dock uses a dedicated CEF cookie manager with session-cookie persistence
enabled. In 0.1.4 its only explicit cookie flush ran after deleting the browser
widget, so the shutdown crash could prevent that flush and force another login.
Version 0.1.5 also flushes every 30 seconds while the dock is open and before
browser destruction. This reduces the chance of losing a recent login if OBS
faults during close. It does not prove cookie persistence on the stream PC;
normal close/restart and crash/restart both need host verification. No scene,
microphone setting, or cookie directory changes are needed.

Compatibility: the event is present in the minimum OBS 30 API and uses the same
baseline SDK. Version 0.1.5 compiled with the pinned OBS 30 / Qt 6.5.3 SDK.
Isolated DLL-loader probes passed against OBS 30.0.0 and 32.0.4, retaining the
30.0.0 module minimum. These checks do not initialize OBS or close a dock; only
an OBS 32.0.4 host close/restart can verify the crash is gone. If shutdown still
crashes, capture its new stack before widening distribution. Rollback is to
restore the prior plugin DLL with OBS closed; the web service is independent.
