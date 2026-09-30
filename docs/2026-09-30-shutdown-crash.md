# OBS shutdown crash — 2026-09-30

The supplied OBS 32.0.4 crash report records an access violation in Qt Widgets
while `obs_frontend_remove_dock` destroys the Roadie dock's QCef browser widget.
Roadie calls that API from `OBS_FRONTEND_EVENT_EXIT`, which OBS emits after
`ClearSceneData()` during main-window shutdown. The stack directly implicates
this teardown path; it does not establish whether another plugin also affects
Qt's state.

Roadie now removes its dock on `OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN`. OBS 30's
shutdown source emits that event before `ClearSceneData()`, then emits `EXIT`.
The module-unload fallback remains idempotent. No scene, microphone setting, or
cookie directory changes are needed.

Compatibility: the event is present in the minimum OBS 30 API and uses the same
baseline SDK. Version 0.1.5 compiled with the pinned OBS 30 / Qt 6.5.3 SDK.
Isolated DLL-loader probes passed against OBS 30.0.0 and 32.0.4, retaining the
30.0.0 module minimum. These checks do not initialize OBS or close a dock; only
an OBS 32.0.4 host close/restart can verify the crash is gone. If shutdown still
crashes, capture its new stack before widening distribution. Rollback is to
restore the prior plugin DLL with OBS closed; the web service is independent.
