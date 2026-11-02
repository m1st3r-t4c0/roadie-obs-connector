#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <functional>

// 1.0.0: the core's half of updating itself (roadie docs/specs/obs-plugin.md §24). The loader picks
// which core runs; this downloads, checks and stages a new one when the streamer presses Update,
// and asks for the last good one on Back to the previous version. Both take effect on the next
// OBS start. `update.json` in the plugin's config folder is shared with the loader (§24.9).

// What `roadie:obs-state` reports as `core`: { running, pending, lastGood, rolledBack }.
QJsonObject coreUpdateState();

// Called with each `roadie:obs-update` detail: downloading, then ready or failed.
using UpdateEvent = std::function<void(const QJsonObject &)>;

// Starts an update to `version`. Answers at once: "" when started, else an error code
// (`bad-version`, `already-running`, `busy`). The work goes on after, reported through `onEvent`.
QString startCoreUpdate(const QString &version, UpdateEvent onEvent);

// The next start uses the last good core. "" when set, else `nothing-to-revert`.
QString revertCore();

// Checks an ECDSA P-256 signature (raw r||s) over `message` against the built-in update key, with
// the operating system's own crypto. Exposed for the release check.
bool verifyUpdateSignature(const QByteArray &message, const QByteArray &signature);
