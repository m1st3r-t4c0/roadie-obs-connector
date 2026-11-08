#pragma once
#include <QJsonArray>
#include <QJsonObject>

// 1.1.0 (t4c0, 2026-11-08): any obs-websocket request, run inside this OBS through obs-websocket's
// plugin API, so a new OBS feature in Roadie needs no new connector. No network: the call never
// touches obs-websocket's server, and works with that server switched off.
//
// Everything is allowed except the block list in obs-request.cpp: OBS's profiles and scene
// collections, the stream service's settings and key, paths on disk, and calls into other plugins.

// The requests this build refuses, as `roadie:obs-state` lists them under `obsRequestBlocked`.
QJsonArray obsRequestBlocked();

// Runs `{requestType, requestData?}` on the UI thread. Answers {"ok": true, "status": <code>,
// "response": {...}} or {"ok": false, "error": <code>, "status"?, "comment"?}.
QJsonObject runObsRequest(const QJsonObject &command);
