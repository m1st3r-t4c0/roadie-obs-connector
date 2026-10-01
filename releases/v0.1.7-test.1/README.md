# Roadie Signal 0.1.7 test release

Tag: `v0.1.7-test.1`. Windows x64, minimum OBS 30.0.0 / Qt 6.5.3.

## Downloads and installation

The standalone [`roadie-signal.dll`](obs-plugins/64bit/roadie-signal.dll) is available
outside the ZIP in this directory and as a separate GitHub release asset.
The ZIP contains the same DLL, this guide and the build report.

With OBS closed, replace the existing Roadie Signal DLL in OBS's
`obs-plugins/64bit` directory. Keep its filename `roadie-signal.dll`; do not leave
an older renamed Roadie DLL alongside it. No OBS or Qt runtime update is required.
Start OBS and verify **Tools > Roadie Signal 0.1.7 status...**. Open
**Docks > Roadie Signal**. In Signal's gear, enable **Share OBS state with Roadie**
to send status and presence to the server.

## Changes

- Closes CEF while the complete dock tree is alive, before dock destruction.
- Sends streaming/recording/scene telemetry from OBS callbacks, with five-second
  presence heartbeats.
- Emits local transition start/end events from the program transition, including
  private transitions. The matching Stage renderer fades out at start and reveals
  the cached mapped view at completion without a server round trip.
- Disconnects native transition callbacks before CEF shutdown. Output changes
  reattach synchronously and callbacks avoid the OBS output-lock deadlock.

The matching Roadie commit is `296bb6fd90b07cff6ce6c94dc192ba9c29327cb5` (including
implementation `decf045b`). It adds persistent 30-day admin/dock sessions and the
local Stage renderer. **That Roadie server/web update must be deployed separately.**
The DLL alone cannot change the server's login cookie or an already loaded page.
Refresh the dock and Stage browser source after the web update. OBS's WebSocket
network server does not need enabling; local events use its in-process API.

## Verification and remaining host checks

Built from clean source commit `4985e1eba8679ba194a92f75c051e9e13d2c5024`. The source and third-party API header
are in this repository. Dependency-load checks and real-libobs transition probes
pass on OBS 30.0.0 and 32.0.4. The probe covers private sources, output changes,
sequence ordering and callback removal. Roadie's focused tests and builds pass.

Full OBS host shutdown/restart, sign-in retention, audio ownership and under-100 ms
rendering remain unverified. The previous isolated CEF probe supports the shutdown
fix; it is not a complete streaming-host test. This is a prerelease, not a claim of
full-host validation. The withdrawn 0.1.6 candidate must not be used.

After the Roadie deployment, check cut/fade/stinger transitions, Studio preview,
manual views, normal OBS exit and restart. Leave the signed-in dock open at least
30 seconds before the restart check so the periodic cookie flush can run.
`window.roadieObsTiming` in the Stage page reports the last 50 timing samples and
p95; it measures callback-to-animation-frame opportunity, not GPU presentation.

`build-report.json` records the DLL hash and verification limits. `SHA256SUMS.txt`
uses the flat GitHub release asset names; the repository's nested DLL is identical.
