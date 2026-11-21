# Isolated OBS Browser lifecycle probe

This executable loads the real `obs-browser.dll` from an extracted OBS fixture
into a small Qt application. It never loads Roadie or touches an installed OBS
profile. A loopback HTTP server issues two synthetic, HttpOnly cookies: one
session-only and one with `Max-Age`. Each test uses a fresh profile under `build/`.
The window is marked `WA_DontShowOnScreen`; HTTP requests prove CEF loaded it.

The pre-close case calls the public `QCefWidget::closeBrowser()` before deleting
the dock. The negative control deletes the parent first, matching Roadie 0.1.5.
Both schedule a paint of the still-live main window. On OBS 32.0.4 the negative
control crashes with `0xc0000005` when CEF's nested loop processes that paint
while the parent dock is partly destroyed. Pre-close completes successfully.
This reproduces the failure mechanism, not the user's exact scene collection.

The script-gate case (#304) calls `executeJavaScript` every 2 ms, the way the
plugin's timers do, while the page reloads, moves to another site (a new
renderer), sits under a nested `QDialog::exec()` loop, and after
`closeBrowser()`. Mode `gate` sends every call through the plugin's own
`ScriptGate` (`src/script-gate.hpp`); it must exit normally, drop calls in each
of those states, resume once the page reports its title, and send nothing after
close. Mode `ungated` is the old Windows path, run for comparison only: the
instant CEF has no main frame is a race, and the isolated probe has not hit it,
so it is not asserted to crash. `--skip-gate` leaves the case out.

The closed-dock case starts with the dock hidden, as OBS keeps a dock unticked under Docks.
obs-browser creates a page's browser only on its first show, so mode `hidden-control` (no show)
must load nothing while the dock is closed. Mode `hidden` sends the plugin's own `HiddenStart`
(`src/hidden-start.hpp`): the page must load and run a script while the dock stays closed, and
opening the dock afterwards must not load it a second time. `--skip-hidden` leaves the case out.

With the repository's pinned SDK and verified OBS fixtures already prepared:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/browser-probe/build.ps1
python tests/browser-probe/run.py --expect-parent-crash
python tests/browser-probe/run.py --host-root build/sdk/30.0.0/obs-host
```

The harness records exit codes, runtime OBS/Qt versions, synthetic-cookie
presence, and teardown markers in `build/browser-probe-verified`. It asserts
that pre-close exits normally and that only the persistent cookie survives a
restart. `--expect-parent-crash` also asserts the OBS 32.0.4 negative control.
OBS 30.0.0 need not reproduce that newer runtime's crash.

This is deeper coverage than a DLL dependency-load check, but it does not prove
the full plugin works in every OBS configuration or measure Stage latency.
