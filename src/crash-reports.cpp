#include "crash-reports.hpp"
#include <obs-module.h>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QRegularExpression>

namespace {
constexpr int maxReports = 3;
constexpr qint64 maxBytes = 64 * 1024;
constexpr qint64 maxAgeDays = 14;

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

QString redactUserPaths(QString text)
{
  static const QRegularExpression profile(QStringLiteral(R"(([A-Za-z]:[\\/]+Users[\\/]+)[^\\/\r\n"]+)"),
                                          QRegularExpression::CaseInsensitiveOption);
  return text.replace(profile, QStringLiteral("\\1<user>"));
}

QJsonArray recentCrashReports()
{
  QJsonArray reports;
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
    const QByteArray raw = file.read(maxBytes);
    const QString text = redactUserPaths(QString::fromUtf8(raw));
    const QByteArray digest = QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex();
    reports.append(QJsonObject{{"fileName", info.fileName()},
                               {"modifiedAt", info.lastModified().toUTC().toString(Qt::ISODate)},
                               {"sha256", QString::fromLatin1(digest)},
                               {"truncated", info.size() > maxBytes},
                               {"text", text}});
  }
  return reports;
}
