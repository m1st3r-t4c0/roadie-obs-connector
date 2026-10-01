# Roadie Signal 0.1.6 test build

This Windows x64 candidate tests a workaround for the OBS 32.0.4 close crash.
The 0.1.5 crash report shows Qt faulting inside OBS Browser's
`QCefWidgetInternal::closeBrowser` while Roadie removes its dock. Version 0.1.6
stops the dock's timers and audio tap, flushes cookies, detaches the hidden
browser widget, and then removes the dock. The browser widget remains alive
until process exit to avoid running that destructor during OBS shutdown.
This workaround is not yet verified in an OBS host.

The DLL was compiled from source commit
`9a8ce2a549ca61f9094454b19d607c5e192f00a4` against OBS 30.0.0 / Qt 6.5.3.
The build succeeded, and isolated Windows dependency-load checks passed against
OBS 30.0.0 and 32.0.4. These checks do not open a dock or test shutdown.
`build-report.json` records the DLL checksum and `hostTested: false`.

With OBS closed, replace only the installed `roadie-signal.dll` with
`obs-plugins/64bit/roadie-signal.dll` from this directory or the ZIP. Restart
OBS and check **Tools → Roadie Signal 0.1.6 status…** to confirm which version
loaded. Open **Docks → Roadie Signal** and sign in. In the top-right Signal
gear, confirm **Native plugin connected**, then enable **Share OBS state with
Roadie**. The OBS card on Roadie's Connectors page should show a heartbeat.

For the shutdown test, leave the signed-in dock open for at least 30 seconds,
close OBS normally, and check that it exits cleanly. Restart OBS and check
whether the dock remains signed in. Please also check Task Manager for a
lingering OBS Browser process after OBS exits. If OBS crashes, send the new
crash report and the OBS log lines containing `[roadie-signal]`.

No OBS update is required for the OBS 30.0.0+ baseline. This test package
does not change microphone audio capture or the Roadie web application.
