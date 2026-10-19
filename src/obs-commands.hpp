#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// 0.3.0: Roadie's OBS actions through the plugin, for a channel whose bot cannot reach OBS
// (roadie docs/specs/obs-plugin.md §19). Every command changes OBS and reads the change back,
// so `ok` means OBS holds the new value, not that a request was sent.

// The commands this build accepts, as `roadie:obs-state` lists them under `capabilities`.
QJsonArray obsCommandCapabilities();

// Runs one command (`action` plus its fields) on the UI thread. Answers {"ok": true} or
// {"ok": false, "error": <code>}; the codes are Roadie's to word for the streamer.
QJsonObject runObsCommand(const QJsonObject &command);

// The names Roadie's pickers offer: `sources` (name, OBS kind, the scenes it sits in) and
// `filters` (source, filter). Bounded, so a huge collection cannot grow the state frame.
QJsonObject obsInventory();
