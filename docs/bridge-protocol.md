# Bridge protocol

The contract between this plugin and Roadie's signed-in `/dock` page (roadie
`apps/web/src/dock/native`). The plugin sends `CustomEvent`s into the page; the page sends commands
back through `document.title`. All additions are additive: a Roadie page that predates a field or
event ignores it.

Every script checks the actual top-level page's origin, the exact `/dock` path and a readiness
marker set only by the signed-in dock provider before it dispatches. These checks isolate the
bridge; they are not a sandbox against a compromised trusted Roadie page. Apart from the 1.0.0
update download, the plugin makes no network request: anything that reaches Roadie's server goes
through the page's own signed-in socket.

## `roadie:obs-audio` (version 1)

```json
{
  "version": 1,
  "sessionId": "plugin-instance-id",
  "sequence": 1,
  "status": "ready",
  "source": { "id": "obs-source-uuid", "name": "Mic/Aux" },
  "peak": 0.4,
  "rms": 0.1,
  "sampleAgeMs": 10
}
```

`ready` requires finite linear levels in [0, 1], a source and a sample no older than 1000 ms. Zero
is a valid reading. Other states are `muted`, `unavailable` and `unselected`, with null levels and
sample age. `unselected` has a null source; `muted` requires a source. Every frame has a monotonic
sequence within one plugin session. The receiver expires a missing bridge after 2000 ms and expires
sample freshness independently; duplicate/invalid data cannot refresh it.

A native OBS meter supplies peak/RMS every 100 ms. The callback updates a snapshot without UI,
JavaScript, allocation or network work. No raw audio is captured or uploaded. Navigation away
detaches the tap; navigating back re-resolves the saved source. Source rename preserves the
selection; removal produces `unavailable`. Source names and readings do not enter cloud connector
history. The hosted receiver validates frames and freshness but does not yet render the waveform or
silence warning.

## `roadie:obs-state` (version 1)

Same plugin session id, its own monotonic sequence:

```json
{
  "version": 1,
  "sessionId": "plugin-instance-id",
  "sequence": 1,
  "streaming": false,
  "recording": false,
  "scene": "Gameplay"
}
```

`scene` is null when OBS has no current scene. Frontend callbacks queue delivery on the Qt thread,
never in an audio callback. Changes are sent immediately; a snapshot is also sent every five seconds
for presence and page-reload recovery. OBS's WebSocket server is not needed.

Fields added later:

| Since | Field | Meaning |
|---|---|---|
| 0.1.8 | `pluginVersion`, `obsVersion` | Strings, so the dock can show the running build and offer updates. |
| 0.1.9 | `scenes` | OBS's scene names in OBS's order, at most 200. |
| 0.1.9 | `capabilities` | The commands this server may send (see [Commands](#commands)). Empty when the server is not chosen under **Accept OBS changes from**. |
| 0.1.12 | `share: true` | State reaches a page only when its server is chosen under **Send OBS data to**, so the plugin menu is the consent. A Roadie dock built with `sharingFromPlugin` shares without the gear's switch; an older one still waits for it. |
| 0.3.0 | `sources` | `[{"name","kind","scenes":[...]}]`, OBS's input id as `kind` (empty for a scene or group), at most 400. |
| 0.3.0 | `filters` | `[{"source","filter"}]`, at most 400. |
| 0.3.1 | `sources[].items` | `[{"scene","id"}]`, one per place the source sits; `id` is `obs_sceneitem_get_id` within that scene or group. At most 200 per source. Used by Roadie's Firebot import. |
| 0.3.1 | `sources[].durationMs` | A media source's length, when OBS has its file open. Omitted when OBS does not know it. |
| 1.0.0 | `core` | The running core's update state (roadie `docs/specs/obs-plugin.md` §24). |

## `roadie:obs-health` (0.1.8, marker `roadieObsHealth`)

Every five seconds while streaming, and once more on `STREAMING_STOPPING`, from the Qt thread:

```json
{
  "version": 1,
  "sessionId": "plugin-instance-id",
  "sequence": 1,
  "sampledAt": 1234567,
  "streaming": true,
  "outputFrames": 54000, "droppedFrames": 12, "bytesSent": 1350000000,
  "congestion": 0.02, "reconnecting": false,
  "renderFrames": 54010, "laggedFrames": 3,
  "encodedFrames": 54000, "skippedFrames": 0
}
```

Counters are cumulative since the output started; `sampledAt` is a monotonic millisecond clock, not
wall time. The plugin keeps no history; Roadie's server folds samples into per-minute deltas for the
stream recap. With no active output the frame is just `streaming: false`.

## `roadie:obs-crash` (0.1.8, marker `roadieObsCrash`)

Sent with the first three state heartbeats after each trusted page load: OBS's own crash reports
from the last 14 days, newest first, at most three, read once per OBS session from the `crashes`
folder beside this plugin's config root (portable installs included).

```json
{
  "version": 1, "sessionId": "...", "pluginVersion": "0.1.8", "obsVersion": "32.0.4",
  "reports": [{ "fileName": "Crash 2026-09-30 21-14-05.txt", "modifiedAt": "2026-09-30T21:14:05Z",
                "sha256": "...", "truncated": false, "text": "..." }]
}
```

Each report is capped at 64 KB. `C:\Users\<name>` paths are replaced with `C:\Users\<user>` before
hashing, so the hash is of what Roadie receives. The page sends them to Roadie only while its
crash-report switch is on; Roadie dedupes by `sha256`.

## Commands

`browser-panel.hpp` gives the plugin no JavaScript-to-native call, only the `titleChanged` and
`urlChanged` signals, so the trusted `/dock` page writes a command into `document.title`:

```text
roadie-obs-command:{"version":1,"id":"<1-64 of A-Z a-z 0-9 ->","action":"set-scene","scene":"BRB"}
```

The plugin reads it only while the page is trusted (exact origin and `/dock`), ignores a title
without the prefix, runs each `id` once (CEF may report one title twice, and the page holds the
title until answered), and runs every command on the Qt thread. It answers with
**`roadie:obs-command-result`** (marker `roadieObsCommand`):

```json
{ "version": 1, "id": "...", "action": "set-scene", "ok": true }
```

or `"ok": false` with an `error`. The page restores its title after the answer, or after four
seconds without one. A server not chosen under **Accept OBS changes from** is answered
`commands-off` at once.

| Since | Action | Fields |
|---|---|---|
| 0.1.9 | `set-scene` | `scene`. Searches only `obs_frontend_get_scenes`, so it can never select a non-scene source. |
| 0.3.0 | `set-source-visibility` | `scene`, `source`, `visible` |
| 0.3.0 | `set-text` | `source`, `text`. Only a GDI+ or FreeType text source, and not one set to read from a file. |
| 0.3.0 | `set-mute` | `input`, `muted` |
| 0.3.0 | `set-filter` | `source`, `filter`, `enabled` |
| 1.0.0 | `take-screenshot` | `source`, `format` (`png` or `jpg`), `width` (16–1920) |
| 1.0.0 | `update-plugin` | `version` |
| 1.0.0 | `revert-plugin` | |

The 0.3.0 commands **read the change back before answering**, so `"ok": true` means OBS holds the
new value. A source in a group inside the scene is found too (`obs_scene_find_source_recursive`).

`take-screenshot` runs **only in an OBS that is streaming**: every open dock is sent the command, and
an OBS that is not live answers `not-live`, so a streamer with OBS on two PCs gets the picture from
the one viewers are watching. The source or scene is rendered into its own texture at `width`, aspect
kept, the way obs-websocket's `GetSourceScreenshot` does it; the program and preview are untouched.
The answer carries `image` as a `data:image/png;base64,…` or `image/jpeg` URL, which the page sends
to Roadie with the step's one-use ticket.

`update-plugin` and `revert-plugin` report through **`roadie:obs-update`**; see the
[README](../README.md#100-one-click-updates).

Errors: `no-such-scene`, `no-such-source`, `no-such-filter`, `not-a-text-source`, `text-from-file`,
`not-an-audio-input`, `not-applied`, `bad-command`, `unknown-action`, `commands-off`, `not-live`,
`no-picture` (no video or no size), `not-rendered`, `not-encoded`.

### Long commands come in parts

Chromium cuts a document title at 4096 characters, and a text command carries up to 4000. The page
then writes `roadie-obs-command-part:` titles:

```text
roadie-obs-command-part:{"version":1,"id":"...","index":0,"count":3,"data":"<a slice of the command JSON>"}
```

The plugin acknowledges each part with **`roadie:obs-command-part`** (`{"version":1,"id","index"}`,
marker `roadieObsCommand`), and the page writes the next only after that. Parts arrive in order; at
most 16, 32 KB in all. The joined JSON is then checked and run like a one-title command.

## Stage transition events (0.1.7)

The plugin attaches to the main program's transition start/video-stop signals, including private
sources, and reattaches on output channel changes, then calls the obs-browser `emit_event` vendor
endpoint through obs-websocket's in-process API. No localhost server, password or network connection
is involved. The event carries only scene, phase, session/sequence/transition ids and callback time.
Progress heartbeats keep long/manual transitions active; scene completion recovers a missed end; the
web receiver reveals output after 15 seconds without progress or completion. Callbacks enqueue on
the Qt thread and are removed before dock shutdown.

`tests/scene-events` exercises real libobs signals and Qt dispatch with a recording vendor endpoint
(preview exclusion, ordered start/end ids, stale end filtering, callback removal) on OBS 30.0.0 and
32.0.4. It does not load CEF or measure GPU/host latency. The web receiver exposes its last 50
callback-to-animation-frame samples and start-only p95 at `window.roadieObsTiming`; the target is
under 100 ms to the first visual reaction.
