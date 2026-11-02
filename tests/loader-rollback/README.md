# Loader rollback probe

The loader's update and rollback rules (roadie `docs/specs/obs-plugin.md` §24.3) on a headless
libobs: `harness.cpp` loads the real `roadie-signal.dll` the way OBS does, then closes cleanly or
stops dead as a crash would; `fake-core.cpp` is a stand-in core that ends the process on start when
its path contains `FAKE_CORE_CRASH`.

Build both against the same headers and `obs.lib` import library as the plugin (`/MD`; the core
with `/LD`), then:

    node run.mjs <harness.exe> <fake-core.dll> <roadie-signal.dll> <OBS bin/64bit>

Each "start" is one process, so a crash leaves `update.json` exactly as a crashed OBS would.
