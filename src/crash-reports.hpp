#pragma once
#include <QJsonArray>
#include <QString>

// OBS's own crash reports from the last 14 days (newest first, at most three),
// read from the crashes folder beside this plugin's config root so portable
// installs work. Each entry carries the file name, its modified time, a
// sha256 of the redacted first 64 KB (the name a report has had since 0.1.8),
// and the redacted text, at most 64 KB: a longer one keeps its header, crashed
// thread and module list and leaves out other threads (crash-compact.hpp).
// 1.1.3: the text names the Roadie connector that was running at the crash
// (runs.json), and connectorAtCrash says it too.
// Windows user-profile paths are replaced with <user> before hashing.
// Reads files: call off the audio thread, once, and cache the result.
QJsonArray recentCrashReports();

// Adds this OBS start to runs.json (once per process). Called when OBS has finished loading, so a
// crash before any dock page loads is still matched to the connector that ran.
void recordRoadieRun();

// Exposed for tests: replaces C:\Users\<name> (either slash, any case) with
// C:\Users\<user>.
QString redactUserPaths(QString text);
