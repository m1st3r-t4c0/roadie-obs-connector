# Native scene-event regression probe

Configure this directory as a standalone CMake project with the same
`CMAKE_PREFIX_PATH`, `ROADIE_OBS_SOURCE_DIR`, `ROADIE_OBS_HOST_DIR` and
`ROADIE_OBS_IMPORT_DIR` used for the plugin. Build Release, then run
`scene-events-probe.exe` with the chosen extracted OBS runtime's `bin/64bit`
first in PATH. Run each runtime in a separate process.

This uses real libobs source objects, signals and Qt queued delivery, with a
recording endpoint in place of obs-websocket/CEF. It starts real private transition
objects and checks main-output filtering, synchronous reattachment on output swaps,
ordered transition/sequence IDs, unrelated end rejection and callback removal.
The isolated executable has no frontend or GPU context; expected diagnostics
mention the absent frontend callback and graphics context during cleanup.

It does not install anything, load the user's profile, or establish real OBS
rendering performance. `tests/browser-probe` separately exercises actual CEF
teardown and cookie retention. Full-host acceptance still requires cut, fade,
stinger, Studio preview, interrupted transition, reload, sign-out/restart, and
callback-to-frame timing under the stream's actual load.
