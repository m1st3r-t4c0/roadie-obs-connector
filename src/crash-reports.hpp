#pragma once
#include <QJsonArray>
#include <QString>

// OBS's own crash reports from the last 14 days (newest first, at most three),
// read from the crashes folder beside this plugin's config root so portable
// installs work. Each entry carries the file name, its modified time, a
// sha256 of the redacted text and the redacted text itself, capped at 64 KB.
// Windows user-profile paths are replaced with <user> before hashing.
// Reads files: call off the audio thread, once, and cache the result.
QJsonArray recentCrashReports();

// Exposed for tests: replaces C:\Users\<name> (either slash, any case) with
// C:\Users\<user>.
QString redactUserPaths(QString text);
