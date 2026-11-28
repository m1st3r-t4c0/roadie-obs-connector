#include "crash-reports.hpp"
#include "crash-compact.hpp"
#include "versions.hpp"
#include <obs-module.h>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace {
constexpr int maxReports = 3;
// What Roadie accepts per report (MAX_CRASH_TEXT, in UTF-16 units like QString's size).
constexpr int maxChars = 64 * 1024;
// The part of a file whose hash names the report. Until 1.1.3 that was also the text sent, so
// keeping it means a report already sent is still known to the dock page and to Roadie.
constexpr qint64 hashBytes = 64 * 1024;
// A report is read whole (to find its crashed thread and module list) up to this size.
constexpr qint64 readBytes = 4 * 1024 * 1024;
constexpr qint64 maxAgeDays = 14;
constexpr int maxRuns = 40;

// #394: which Roadie connector ran when. One entry per OBS start, newest last, in
// plugin_config/<module>/runs.json. A crash report's time picks the run it happened in.
QString runsPath()
{
  char *path = obs_module_config_path("runs.json");
  if (!path)
    return {};
  const QString result = QString::fromUtf8(path);
  bfree(path);
  return result;
}

QJsonArray readRuns(const QString &path)
{
  QFile file(path);
  if (path.isEmpty() || !file.open(QIODevice::ReadOnly))
    return {};
  const QJsonDocument doc = QJsonDocument::fromJson(file.read(256 * 1024));
  return doc.isArray() ? doc.array() : QJsonArray{};
}

// The runs before this one, and this one added (once per OBS process).
QJsonArray recordRun()
{
  static bool recorded = false;
  static QJsonArray before;
  if (recorded)
    return before;
  recorded = true;
  const QString path = runsPath();
  before = readRuns(path);
  QJsonArray runs = before;
  runs.append(QJsonObject{{"startedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                          {"connector", connectorVersion()},
                          {"loader", loaderVersion()}});
  while (runs.size() > maxRuns)
    runs.removeFirst();
  QFile file(path);
  if (!path.isEmpty() && file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    file.write(QJsonDocument(runs).toJson(QJsonDocument::Compact));
  return before;
}

// The run a crash at `crashedAt` happened in: the latest that started before it.
QJsonObject runAt(const QJsonArray &runs, const QDateTime &crashedAt)
{
  QJsonObject found;
  QDateTime foundAt;
  for (const QJsonValue &value : runs) {
    const QJsonObject run = value.toObject();
    const QDateTime started = QDateTime::fromString(run.value("startedAt").toString(), Qt::ISODate);
    if (!started.isValid() || started > crashedAt)
      continue;
    if (!foundAt.isValid() || started > foundAt) {
      found = run;
      foundAt = started;
    }
  }
  return found;
}

QString crashDirectory()
{
  // obs_module_config_path("") is <config>/obs-studio/plugin_config/<module>/.
  // OBS writes crash reports to <config>/obs-studio/crashes, so walk up two.
  char *own = obs_module_config_path("");
  if (!own)
    return {};
  QDir dir(QString::fromUtf8(own));
  bfree(own);
  if (!dir.cdUp() || !dir.cdUp())
    return {};
  return dir.filePath("crashes");
}
} // namespace

void recordRoadieRun() { recordRun(); }

QString redactUserPaths(QString text)
{
  static const QRegularExpression profile(QStringLiteral(R"(([A-Za-z]:[\\/]+Users[\\/]+)[^\\/\r\n"]+)"),
                                          QRegularExpression::CaseInsensitiveOption);
  return text.replace(profile, QStringLiteral("\\1<user>"));
}

QJsonArray recentCrashReports()
{
  QJsonArray reports;
  const QJsonArray runs = recordRun();
  const QString path = crashDirectory();
  if (path.isEmpty())
    return reports;
  const QDir dir(path);
  if (!dir.exists())
    return reports;
  const QDateTime oldest = QDateTime::currentDateTimeUtc().addDays(-maxAgeDays);
  const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.txt")}, QDir::Files, QDir::Time);
  for (const QFileInfo &info : files) {
    if (reports.size() >= maxReports)
      break;
    if (info.lastModified().toUTC() < oldest)
      break; // sorted newest first
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly))
      continue;
    const QByteArray raw = file.read(readBytes);
    const QString legacy = redactUserPaths(QString::fromUtf8(raw.left(hashBytes)));
    const QByteArray digest = QCryptographicHash::hash(legacy.toUtf8(), QCryptographicHash::Sha256).toHex();
    const QDateTime crashedAt = info.lastModified().toUTC();
    const QJsonObject run = runAt(runs, crashedAt);
    const QString runLine =
        run.isEmpty() ? QString()
                      : QStringLiteral("Roadie connector at crash: %1 (OBS plugin %2)")
                            .arg(run.value("connector").toString(),
                                 run.value("loader").toString().isEmpty() ? QStringLiteral("unknown")
                                                                         : run.value("loader").toString());
    const CrashCompact::Result compacted =
        CrashCompact::compact(redactUserPaths(QString::fromUtf8(raw)), maxChars, runLine);
    QJsonObject report{{"fileName", info.fileName()},
                       {"modifiedAt", crashedAt.toString(Qt::ISODate)},
                       {"sha256", QString::fromLatin1(digest)},
                       {"truncated", compacted.cut || info.size() > readBytes},
                       {"text", compacted.text}};
    if (!run.isEmpty())
      report["connectorAtCrash"] = run.value("connector");
    reports.append(report);
  }
  return reports;
}
