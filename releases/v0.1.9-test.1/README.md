# Roadie Signal 0.1.9 test release

Tag: `v0.1.9-test.1`. Windows x64, minimum OBS 30.0.0 / Qt 6.5.3.

## Downloads and installation

The standalone [`roadie-signal.dll`](obs-plugins/64bit/roadie-signal.dll) is in this directory and
is a separate GitHub release asset. The ZIP contains the same DLL, this guide and the build report.

With OBS closed, replace the existing `roadie-signal.dll` in OBS's `obs-plugins/64bit` directory.
Keep the filename; do not leave an older renamed Roadie DLL beside it. No OBS or Qt runtime update is
required. Start OBS and check **Tools > Roadie Signal 0.1.9 status...**, then open
**Docks > Roadie Signal**. Scene switching needs **Share OBS state with Roadie** on in Signal's gear.

## Changes

- **Roadie can switch OBS scenes.** The OBS connector page in Roadie lists this OBS's scenes and
  switches to the one you pick. The command reaches the plugin through the signed-in Signal dock
  (the page's title is the only page-to-plugin signal OBS's browser panel offers); the plugin acts
  only for the trusted `/dock` page, runs each command once, and only ever selects one of OBS's own
  scenes.
- `roadie:obs-state` gains `scenes` and `capabilities: ["set-scene"]`.
- Includes 0.1.8, which was built but never released: plugin and OBS version in the state frame,
  stream health samples for the recap, and OBS's own crash reports handed to the dock.

The matching Roadie commit is `3ed8e611` (roadie `docs/specs/obs-plugin.md` §16). **That Roadie
update must be deployed separately.** After it is live, refresh the Signal dock.

## Verification and remaining host checks

Built from clean source commit `db21912449968007e308f87703d0358fca372574`, warning-free. Dependency-load checks pass on
OBS 30.0.0 and 32.0.4. **Not host-tested.** Whether the dock's title changes reach the plugin in a
real OBS is the check only a running OBS can do.

In OBS with sharing on: the OBS connector page shows no update notice; **Switch scene** changes OBS's
scene, the page's Scene line follows, and Signal's gear says *Roadie switched OBS to ...*. Also
check normal OBS exit and restart.

`build-report.json` records the DLL hash and verification limits. `SHA256SUMS.txt` uses the flat
GitHub release asset names; the repository's nested DLL is identical.
