# Missing dock: cookie directory startup failure

## Report and evidence

The streamer reported Roadie enabled in Plugin Manager but absent from the Docks
menu on a separate streaming PC. The supplied September 29, 2026 session log
confirms OBS 32.0.4 with Qt 6.8.3 and Roadie Signal 0.1.1. Relevant diagnostic
lines are retained verbatim below. Unrelated log content, private source URLs,
local paths and machine configuration are excluded from this record.

```text
08:45:17.763: [roadie-signal] native dock 0.1.1; OBS 32.0.4; Qt 6.8.3; baseline OBS 30.0.0 / Qt 6.5.3
08:45:28.261: Failed to create cookie manager: Failed to create cookie directory
08:45:28.261: [roadie-signal] dock registration failed
```

The DLL loaded and passed the OBS/Qt/browser checks. The old combined failure
message obscured the earlier failure: SignalDock never initialized its browser,
so the short-circuited condition never called the dock registration API.

## Cause and correction

`SignalDock` passed its absolute `obs_module_config_path("browser-cookies")` to
`QCef::create_cookie_manager`. The upstream implementation prefixes the supplied
storage name with obs-browser's own module configuration directory. On Windows,
the resulting path embeds a second drive-qualified path and cannot be created.
The upstream OBS frontend also passes a relative name to this API.

Version 0.1.2 passes the dedicated relative `roadie-signal/browser-cookies` name.
OBS browser code resolves and creates the cookie directory. The Roadie settings
root is still created independently for microphone selection persistence.
Initialization and registration now have separate diagnostics, including a success
message. No local paths, cookies or credentials are logged.

## Compatibility and rollout

The same OBS 30.0.0 / Qt 6.5.3 SDK and panel API v3 remain in use; no dependency
updates. Existing microphone settings are preserved. No OBS settings, scenes,
other browser cookies or running installation are modified by the build.
The reported 0.1.1 failure never opened a browser, so there is no successful login
store to migrate for this case. The corrected private cookie namespace stays
stable across future builds. Do not delete unrelated browser storage.

Version 0.1.2 was published as a test prerelease. A later September 30 host log
still reported 0.1.1 with the same cookie failure, establishing that OBS was
loading an older DLL. Version 0.1.3 adds a Tools-menu status action before dock
initialization, so the running version and startup result remain visible even
when the dock fails. The streamer performs the real OBS check after installing
the replacement package:
close OBS, replace only the plugin DLL, restart, confirm Docks -> Roadie Signal,
check Tools -> Roadie Signal status and verify the new success line. Then check
sign-in persistence, microphone settings and shutdown. Reverting to 0.1.1 would
restore the reported failure;
removing the test DLL with OBS closed removes the plugin without resetting OBS.

## Validation

- Release build passed with the pinned OBS 30.0.0 / Qt 6.5.3 toolchain.
- DLL export and x64 dependency inspection passed.
- Isolated DLL-loader probes passed against OBS 30.0.0, 32.0.4 and 32.2.1;
  the exported minimum remains 30.0.0. No OBS application was started.
- A focused Windows reproduction using the real baseline `obs.dll` export
  `os_mkdirs` returned -1 for a browser-prefixed absolute argument and 0 for
  the corrected relative argument; the latter directory existed afterwards.
  It ran only under the ignored build directory, never live OBS configuration.
- `git diff --check` passed.
- Version 0.1.3 compiled against the same baseline SDK, and isolated DLL-loader
  probes passed with the OBS 30.0.0 and 32.0.4 distributions. Its status menu
  still requires OBS host verification.

Real OBS host behavior, cookies across restart and microphone behavior remain
unverified. Static loading alone did not catch the original cookie-path error,
so a fresh host log with `Roadie Signal dock registered` is required.
