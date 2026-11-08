# Roadie connector 1.1.0 for Windows

Tag: `v1.1.0`. Windows x64, minimum OBS 30.0.0. OBS plugin (loader) 1.0.0.

**On OBS plugin 1.0.0 or newer?** Press **Update** in the Roadie Signal dock when Roadie offers
1.1.0, then restart OBS. Nothing to download by hand.

## What is new

Roadie connector 1.1.0; the OBS plugin (loader) stays 1.0.0. Tag `v1.1.0`. Delivered by the dock's
**Update** button; nobody reinstalls.

- **Two numbers** (t4c0, 2026-11-08). The **OBS plugin** is the loader people install by hand; it
  keeps its own number (1.0.0) and changes only when a change needs a reinstall. The **Roadie
  connector** is the core the Update button downloads; this release and every one after it raise
  only that number. Tools → Roadie Signal… shows both, and `roadie:obs-state` sends the loader's as
  `loaderVersion` (`pluginVersion` stays the connector's, the number updates and features key on).
- **`obs-request`**: any obs-websocket request, run inside this OBS through obs-websocket's plugin
  API, so a new OBS feature in Roadie needs no new connector (t4c0, 2026-11-08). No network and no
  WebSocket password: the call never touches obs-websocket's server. Everything is allowed except a
  block list: profiles, scene-collection switches, the stream service's settings and key, the canvas
  size, writing files at a named path, and other plugins' vendor requests. The list is sent as
  `obsRequestBlocked`.
- **`scripts/release.mjs`**: `prepare` builds, signs and stages a release; `publish` tags it and
  creates the GitHub release. Roadie reads the newest release from GitHub, so a connector release
  needs no Roadie deploy.

For roadie `docs/specs/stage-attach-to-obs-source.md` (Attach Stage sources to OBS sources):

- **`get-layout`**: every source's rectangle on OBS's canvas in any scene, on air or not, including
  sources inside groups and nested scenes, and sources whose eye is off (t4c0, 2026-11-07).
- **`roadie:obs-layout`**: the layout of the scene on air, sent to the dock only when it changes, and
  straight to Roadie's Stage pages in the same OBS through obs-browser.
- The Roadie Stage browser source is marked in the layout, never by its URL.

Tested: both DLLs build against OBS 30.0.0 with no warnings and pass the loader check against 30.0.0
and 32.0.4. **Not tested in OBS yet.**

## Release files

| File | For |
|---|---|
| `roadie-signal-core-windows-x64.dll`, `manifest-windows-x64.json`, `manifest-windows-x64.sig` | The dock's Update button. The manifest names the core's SHA-256, size and minimum OBS; the signature is ECDSA P-256 by the release key built into the plugin. |
| `roadie-signal-1.1.0-obs30.0.0-windows-x64.zip` | A new install by hand: both DLLs, README, LICENSE, build report. See the [README](../../README.md#install-on-windows). |
| `SHA256SUMS.txt`, `build-report.json` | Checksums and build record. |
