# Roadie Signal 0.1.5 test build

This Windows x64 OBS host test build retains the 0.1.4 streaming, recording,
and current-scene bridge. It moves dock removal earlier in OBS shutdown and
flushes the dock's persisted session cookies periodically and before browser
destruction. The 0.1.4 DLL was implicated in an OBS 32.0.4 shutdown crash;
0.1.5 has not yet been tested by closing OBS on the stream PC.

The DLL was compiled from source commit
`6a2de82f0ec99c8dd446f777b71e42716f532425` against OBS 30.0.0 / Qt 6.5.3.
The build succeeded, and isolated Windows dependency-load checks passed against
OBS 30.0.0 and 32.0.4. Those checks do not start OBS or verify login persistence.
See `build-report.json` for the DLL checksum and build details.

With OBS closed, copy `obs-plugins/64bit/roadie-signal.dll` into the matching OBS
plugin directory, replacing the old Roadie Signal DLL. Restart OBS and check
**Tools → Roadie Signal 0.1.5 status…** to confirm which version actually loaded.
Open **Docks → Roadie Signal**, sign in, and open the top-right Signal gear.
The gear should show **Native plugin connected**. Turn on **Share OBS state with
Roadie**; it should then show **Sharing with Roadie**. The OBS card on Roadie's
Connectors page should become connected after a heartbeat, typically within
five seconds. If the gear says **Waiting for native OBS plugin**, send the OBS
log with its `[roadie-signal]` lines. If it says **Roadie could not receive OBS
state**, send that result and the OBS log. Do not share cookies or credentials.

For persistence, leave the signed-in dock open at least 30 seconds, close OBS
normally, restart, and check whether the dock is still signed in. Then close
OBS again and check whether it exits without a crash. Please send a fresh crash
report if it still fails; do not assume the 0.1.4 stack and 0.1.5 stack match.

The ZIP and this directory contain the same DLL. No OBS update is required for
the supported OBS 30.0.0+ baseline. The plugin does not need OBS's built-in
WebSocket server, and it does not send microphone audio to Roadie.
