#pragma once
#include <QJsonObject>

// Cumulative OBS output, render and encoder counters for the active stream.
// Roadie's server turns consecutive samples into per-minute deltas; the plugin
// never keeps history. Returns an object with "streaming": false and no
// counters when there is no active streaming output. Call on the Qt thread.
QJsonObject sampleStreamHealth();
