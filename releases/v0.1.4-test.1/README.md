# Roadie Signal 0.1.4 test build

This Windows x64 DLL adds OBS streaming, recording, and current-scene telemetry to the Signal dock. It was built from source commit `2e159977513e0997ff1b983c91d034c4bfdd8dff` against OBS 30.0.0 / Qt 6.5.3. It is an OBS host test build, not an official release.

**Known issue:** an OBS 32.0.4 crash report implicated dock removal during shutdown. Version 0.1.5 changes that teardown order; use 0.1.5 for subsequent testing once available. Save your OBS setup before testing 0.1.4 and close OBS before replacing the DLL.

Copy `obs-plugins/64bit/roadie-signal.dll` into the matching OBS plugin location, replacing the old Roadie Signal DLL, then restart OBS. Check **Tools → Roadie Signal 0.1.4 status…** to confirm OBS loaded this version. Open **Docks → Roadie Signal**, sign in, and enable **Share OBS state with Roadie** in the Signal gear. Roadie's Connectors page should show OBS connected after a heartbeat, typically within five seconds. OBS's built-in WebSocket server is not needed.

The ZIP and this directory contain the same DLL. See `build-report.json` for the DLL checksum and build details. The connection requires the live Roadie web and server receiver; both were observed on `dashboard.roadie.gg` on 2026-09-30.
