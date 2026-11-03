# Changelog

Windows release packages (DLL, ZIP, checksums, build report) are checked in under
[`releases/`](releases). Protocol details are in [docs/bridge-protocol.md](docs/bridge-protocol.md).

## 1.0.0 — one-click updates, with crash rollback

Not released yet.

- **One-click updates.** The plugin is now a loader (what OBS loads, libobs only) and a core (the
  plugin itself). The dock's **Update** button downloads a signed core from this repo's GitHub
  release, verifies its ECDSA P-256 signature and SHA-256, and runs it from the next OBS start.
  This is the plugin's only outbound request, and only when the streamer presses Update.
- **Crash rollback.** A core that never reaches two minutes (or a clean close) on two starts in a
  row is marked bad, and the last good core runs instead. `revert-plugin` asks for the last good
  core on demand.
- **Take an OBS screenshot without Firebot** (first built as 0.3.4, which was never released): the
  `take-screenshot` command, answered only by an OBS that is streaming.
- `obs-state` reports the core's update state; `scripts/sign-core.mjs` signs a release.

Tested: the loader's rules on headless libobs (14 cases: fresh install, update, a core that crashes
OBS on start twice, one that crashes later twice, a bad core named again, a missing core); the
signature check against the release key; both DLLs build against OBS 30.0.0 with no warnings and
pass the loader check against 30.0.0 and 32.0.4. **Not yet tried in OBS**, and the download path
needs a published release. The macOS build is Roadie to-do #294.

See the [README](README.md#100-one-click-updates) for the install layout and update design.

## 0.3.3 — the first macOS build, and no crash when quitting OBS on a Mac

Tag `v0.3.3-test.1`. Adds the macOS (Apple Silicon) build contributed by **CtrlGremlin**, with a
fix for three crashes CtrlGremlin found in macOS crash reports when quitting OBS with the dock open:

1. The 100 ms audio publish kept calling into the dock page during shutdown and while OBS's quit
   dialog was open, after CEF had already dropped the page's main frame.
2. Resuming those calls after a fixed delay was unsafe while the quit dialog stayed open.
3. With the dock floating, Cmd+Q closes the dock's window before OBS's scripting-shutdown event, so
   the later browser close touched a freed view.

On macOS the plugin now stops calling into the page once OBS starts closing, under any modal dialog
(including native alerts) and inside nested event loops, resumes only when a quit is cancelled, and
closes its pages on Quit while every window still exists (unless an output is running, when OBS asks
first and the normal shutdown runs).

**Windows is unchanged** apart from the version: it already closes the pages at scripting shutdown,
before OBS removes the dock (see [docs/2026-09-30-shutdown-crash.md](docs/2026-09-30-shutdown-crash.md)).
Pausing under every dialog there would drop scene events and command results for no gain.

Verified: the Windows build and DLL-loader probes, and that CtrlGremlin's patches apply to 0.3.1's
source. Not verified here: the Mac build itself, the quit crashes being gone, and the dock on a Mac.

The published Mac package is a separate release, `macos-v0.3.1-test.1`.

## 0.3.2 — the sign-in survives a restart on OBS 33

On OBS 33 the dock asked for a sign-in at every start. OBS 33's browser moved to CEF's Chrome
runtime, which keeps cookies only for a store whose folder sits **directly** in obs-browser's config
folder; a deeper folder is silently treated as incognito (upstream obs-browser `bb9b32e`). The
plugin's store was `roadie-signal/browser-cookies`, one level too deep.

- The store is now **`roadie-signal-cookies`**, a direct child, on every OBS version.
- On the first start without it, the plugin copies the old store across (into a `.copying` folder,
  then renamed into place, so a failed copy leaves nothing half-made). An existing sign-in on OBS
  30–32 carries over. The old folder is left alone, so an older build still finds its sign-in.
- The log says `sign-in moved to the roadie-signal-cookies store` once, or that the copy failed and
  one more sign-in is needed. No paths or cookies are logged.

Verified: the build and DLL-loader probes. Not verified here: a sign-in surviving an OBS 33 restart,
and the carried-over sign-in on OBS 32.

## 0.3.1 — OBS's source numbers, for the Firebot import

A Firebot effect often names an OBS source by its number in a scene alone. Roadie's Firebot import
finds the name from the plugin, so a channel needs no Firebot connector for it (roadie
`docs/specs/obs-plugin.md` §20). `obs-state`'s `sources` gain `items` (scene item ids) and
`durationMs` (media length).

## 0.3.0 — every Roadie OBS action

Roadie's automation can set a text source, show or hide a source, mute or unmute an input, and turn
a filter on or off through the plugin, so a channel needs no bot connected to OBS for them (roadie
`docs/specs/obs-plugin.md` §19). Long commands arrive in acknowledged parts. `obs-state` gains
`sources` and `filters`. The **Tools → Roadie Signal…** choice is now labelled **Accept OBS changes
from**; it governs every command. The saved setting (`acceptFrom`) is unchanged.

## 0.2.0

The first release without a test suffix. Same source as 0.1.12.

## 0.1.12 — one settings window

- The Tools menu has one Roadie entry, **Roadie Signal…**, showing the plugin, OBS and Qt versions,
  browser and dock status, and holding the microphone and server choices.
- At startup the plugin lists every other loaded OBS module whose description starts with "Roadie
  Signal" or whose file name contains `roadie-signal`, logs each path, and warns naming the files to
  delete. Plugins before 0.1.12 do not check.
- **One dock for every server.** The **Roadie Signal** dock shows production's page, or staging's
  when staging is the only server. With both on, the staging page runs out of sight behind it
  (`QStackedLayout::StackAll`, so its browser keeps loading and sending).
- **One cookie store for every page.** Roadie's session cookie is per site, so the servers never see
  each other's; the shared Twitch login lets the hidden page sign in without a click.
- **The hidden page signs itself in.** Every five seconds, at most one attempt a minute, a trusted
  `/dock` page showing Roadie's sign-in screen follows its own Twitch login link. If Twitch wants a
  click, the settings window offers **Show the Staging sign-in**.
- **The plugin menu is the consent:** `obs-state` carries `share: true`.

## 0.1.11 — choose Production, Staging or both

**Send OBS data to** and **Accept scene changes from**, each Production, Staging, or both, both
defaulting to Production. Saved in the plugin's `settings.json` (`sendTo`, `acceptFrom`: 1
production, 2 staging, 3 both) and applied at once. Each server has its own trusted origin, sign-in,
session id and sequences; staging's page is `https://staging.roadie.gg/dock`. A server outside
**Accept** is answered `commands-off`, and its `obs-state` has empty `capabilities`.

## 0.1.10

A version label over 0.1.9 only.

## 0.1.9 — scene switching from Roadie

`obs-state` gains `scenes` and `capabilities`; the page can ask the plugin to switch scene through
the `document.title` command channel, answered by `roadie:obs-command-result`.

## 0.1.8 — version, stream health and crash reports

`obs-state` gains `pluginVersion` and `obsVersion`; new `roadie:obs-health` and `roadie:obs-crash`
events. All additive, so 0.1.8 is safe against an older hosted web app.

## 0.1.7 — shutdown fix and Stage transition events

Calls the public `closeBrowser()` while the dock tree is still alive, then destroys the widgets
normally, fixing the shutdown crash (see [tests/browser-probe](tests/browser-probe/README.md)).
Adds local Stage transition events. Streaming/recording/scene status now publishes from frontend
callbacks; the timer is only a five-second heartbeat. Roadie's matching web change adds a persistent
30-day dock login and cached Stage views.

## 0.1.6 — withdrawn

The first 0.1.6 candidate is **withdrawn; do not install or publish it**. Research reproduced an
access violation with OBS 32.0.4's real browser library when a queued paint ran during parent-dock
destruction. Its DLL and ZIP were removed from the tree.

## 0.1.5

Attempts to remove the browser dock before OBS clears scene/UI data during shutdown, and flushes
the dock's cookie store periodically and before browser destruction. A host screenshot confirmed it
loaded on OBS 32.0.4 / Qt 6.8.3, but it still crashed on close in the browser destructor (see
[docs/2026-09-30-shutdown-crash.md](docs/2026-09-30-shutdown-crash.md)).

Sign-in persistence needed a separate web-side fix: Roadie issued a session-only cookie, and OBS
32.0.4 ignores the cookie-manager persistence flag.

## 0.1.4

Adds a second local bridge for OBS streaming, recording and current-scene state, published within
one second plus a five-second heartbeat, with an opt-in in the page's Signal gear. Fixes a startup
failure on OBS 32.0.4: the QCef cookie API requires a relative storage name, not an absolute Windows
path (see [docs/cookie-directory-startup.md](docs/cookie-directory-startup.md)). Shows the running
version at the top of the Tools window, even if dock startup fails. Its shutdown crash was
reproduced on OBS 32.0.4.

## 0.1.0 – 0.1.3

Historical test builds:
[v0.1.3-test.1](https://github.com/m1st3r-t4c0/roadie-obs-connector/releases/tag/v0.1.3-test.1),
[v0.1.2-test.1](https://github.com/m1st3r-t4c0/roadie-obs-connector/releases/tag/v0.1.2-test.1),
[v0.1.1-test.1](https://github.com/m1st3r-t4c0/roadie-obs-connector/releases/tag/v0.1.1-test.1).
0.1.1 contains the cookie-path bug. The v0.1.0-test.1 DLL was tied to OBS 32.2.1 / Qt 6.11.1 and
cannot load on 32.0.4 (Windows error 127).
