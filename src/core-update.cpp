#include "core-update.hpp"
#include "update-verify.hpp"
#include <obs-module.h>
#include <util/bmem.h>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>
#include <QVersionNumber>
#include <memory>


namespace {
// Where a release's files are. A fixed host: Roadie only ever names a version, never a URL (§24.5).
const char *kReleases = "https://github.com/m1st3r-t4c0/roadie-obs-connector/releases/download/v";

#ifdef _WIN32
const char *kPlatform = "windows-x64";
const char *kCoreFile = "roadie-signal-core.dll";
#elif defined(__aarch64__) || defined(__arm64__)
const char *kPlatform = "macos-arm64";
const char *kCoreFile = "roadie-signal-core.dylib";
#else
const char *kPlatform = "macos-x64";
const char *kCoreFile = "roadie-signal-core.dylib";
#endif

// A core is at most this big; a manifest or signature far less.
constexpr qint64 kMaxCore = 64 * 1024 * 1024;
constexpr qint64 kMaxSmall = 16 * 1024;

bool busy = false;
// The loader's note that it fell back, read once at start and reported for this session only.
QJsonObject rolledBack;
bool rolledBackRead = false;

bool validVersion(const QString &v)
{
  static const QRegularExpression pattern(QStringLiteral("^\\d{1,4}\\.\\d{1,4}\\.\\d{1,4}$"));
  return pattern.match(v).hasMatch();
}

QString configDir()
{
  char *path = obs_module_config_path("");
  QString out = QString::fromUtf8(path ? path : "");
  bfree(path);
  return out;
}

QString statePath() { return QDir(configDir()).filePath(QStringLiteral("update.json")); }

QJsonObject readState()
{
  QFile file(statePath());
  if (!file.open(QIODevice::ReadOnly))
    return {};
  return QJsonDocument::fromJson(file.readAll()).object();
}

// Read again just before writing: the loader writes this file too (§24.9).
bool updateState(const std::function<void(QJsonObject &)> &change)
{
  QJsonObject state = readState();
  change(state);
  QDir().mkpath(configDir());
  QSaveFile file(statePath());
  if (!file.open(QIODevice::WriteOnly))
    return false;
  file.write(QJsonDocument(state).toJson(QJsonDocument::Indented));
  return file.commit();
}

bool isBad(const QJsonObject &state, const QString &version)
{
  for (const QJsonValue &item : state.value("bad").toArray())
    if (item.toObject().value("version").toString() == version)
      return true;
  return false;
}

QString curlPath()
{
#ifdef _WIN32
  // Windows 10 and later ship curl, which uses the system's own TLS.
  return QDir(qEnvironmentVariable("SystemRoot", QStringLiteral("C:\\Windows")))
      .filePath(QStringLiteral("System32/curl.exe"));
#else
  return QStringLiteral("/usr/bin/curl");
#endif
}

// One HTTPS download to a file, with the platform's curl: OBS ships Qt's network library without a
// TLS backend, and curl needs no library of ours. Calls `done(true)` when the file is there.
void fetch(const QString &url, const QString &path, qint64 maxBytes, std::function<void(bool)> done)
{
  auto *process = new QProcess();
  process->setProgram(curlPath());
  process->setArguments({QStringLiteral("--fail"), QStringLiteral("--silent"), QStringLiteral("--show-error"),
                         QStringLiteral("--location"), QStringLiteral("--proto"), QStringLiteral("=https"),
                         QStringLiteral("--proto-redir"), QStringLiteral("=https"), QStringLiteral("--max-time"),
                         QStringLiteral("300"), QStringLiteral("--max-filesize"), QString::number(maxBytes),
                         QStringLiteral("--output"), path, url});
  QObject::connect(process, &QProcess::finished, process, [process, path, done](int code, QProcess::ExitStatus status) {
    process->deleteLater();
    done(status == QProcess::NormalExit && code == 0 && QFileInfo(path).isFile());
  });
  QObject::connect(process, &QProcess::errorOccurred, process, [process, done](QProcess::ProcessError error) {
    if (error != QProcess::FailedToStart)
      return;
    process->deleteLater();
    done(false);
  });
  process->start();
}

QByteArray readAll(const QString &path)
{
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// Every core folder but the one running, the one waiting, and the last good one goes.
void prune()
{
  const QJsonObject state = readState();
  const QStringList keep{QString::fromLatin1(ROADIE_SIGNAL_VERSION), state.value("current").toString(),
                         state.value("pending").toString(), state.value("lastGood").toString()};
  QDir cores(QDir(configDir()).filePath(QStringLiteral("cores")));
  for (const QString &name : cores.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    if (!keep.contains(name))
      QDir(cores.filePath(name)).removeRecursively();
}

// One update's work, step by step. Each step fails with the code the dock words (§24.9).
struct Update : std::enable_shared_from_this<Update> {
  QString version;
  UpdateEvent onEvent;
  QString work;
  QJsonObject manifest;

  void report(const char *state, const char *error = nullptr)
  {
    QJsonObject detail{{"version", 1}, {"target", version}, {"state", QString::fromLatin1(state)}};
    if (error)
      detail["error"] = QString::fromLatin1(error);
    if (onEvent)
      onEvent(detail);
  }

  void fail(const char *error)
  {
    QDir(work).removeRecursively();
    busy = false;
    blog(LOG_WARNING, "[roadie-signal] update to %s failed: %s", version.toUtf8().constData(), error);
    report("failed", error);
  }

  QString url(const QString &file) const { return QString::fromLatin1(kReleases) + version + "/" + file; }

  void start()
  {
    work = QDir(configDir()).filePath(QStringLiteral("cores/.download-") + version);
    QDir(work).removeRecursively();
    if (!QDir().mkpath(work))
      return fail("write-failed");
    report("downloading");
    const QString name = QStringLiteral("manifest-") + QString::fromLatin1(kPlatform);
    auto self = shared_from_this();
    fetch(url(name + ".json"), QDir(work).filePath("manifest.json"), kMaxSmall, [self, name](bool ok) {
      if (!ok)
        return self->fail("not-found");
      fetch(self->url(name + ".sig"), QDir(self->work).filePath("manifest.sig"), kMaxSmall,
            [self](bool ok) { ok ? self->checkManifest() : self->fail("not-found"); });
    });
  }

  void checkManifest()
  {
    const QByteArray bytes = readAll(QDir(work).filePath("manifest.json"));
    const QByteArray signature = QByteArray::fromBase64(readAll(QDir(work).filePath("manifest.sig")).trimmed());
    if (!verifyUpdateSignature(bytes, signature))
      return fail("bad-signature");
    manifest = QJsonDocument::fromJson(bytes).object();
    const QString file = manifest.value("file").toString();
    if (manifest.value("version").toString() != version ||
        manifest.value("platform").toString() != QString::fromLatin1(kPlatform) ||
        !QRegularExpression(QStringLiteral("^[A-Za-z0-9._-]{1,100}$")).match(file).hasMatch())
      return fail("bad-signature");
    const QVersionNumber minimum = QVersionNumber::fromString(manifest.value("minObs").toString());
    const QVersionNumber running = QVersionNumber::fromString(QString::fromLatin1(obs_get_version_string()));
    if (!minimum.isNull() && running < minimum)
      return fail("unsupported-obs");
    auto self = shared_from_this();
    fetch(url(file), QDir(work).filePath(QString::fromLatin1(kCoreFile)), kMaxCore,
          [self](bool ok) { ok ? self->install() : self->fail("download-failed"); });
  }

  void install()
  {
    const QString downloaded = QDir(work).filePath(QString::fromLatin1(kCoreFile));
    const QByteArray bytes = readAll(downloaded);
    const QByteArray hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
    if (bytes.size() != manifest.value("bytes").toInteger(-1) ||
        hash != manifest.value("sha256").toString().toLatin1().toLower())
      return fail("bad-hash");
    const QString folder = QDir(configDir()).filePath(QStringLiteral("cores/") + version);
    QDir(folder).removeRecursively();
    if (!QDir().rename(work, folder))
      return fail("write-failed");
    const QString target = version;
    if (!updateState([&target](QJsonObject &state) { state["pending"] = target; }))
      return fail("write-failed");
    prune();
    busy = false;
    blog(LOG_INFO, "[roadie-signal] core %s is ready for the next OBS start", version.toUtf8().constData());
    report("ready");
  }
};
} // namespace

QJsonObject coreUpdateState()
{
  if (!rolledBackRead) {
    rolledBackRead = true;
    rolledBack = readState().value("rolledBack").toObject();
    // Told once: this session reports it, the next does not.
    if (!rolledBack.isEmpty())
      updateState([](QJsonObject &state) { state.remove("rolledBack"); });
  }
  const QJsonObject state = readState();
  const auto version = [](const QJsonValue &v) {
    const QString s = v.toString();
    return validVersion(s) ? QJsonValue(s) : QJsonValue(QJsonValue::Null);
  };
  QJsonObject out{{"running", QString::fromLatin1(ROADIE_SIGNAL_VERSION)},
                  {"pending", version(state.value("pending"))},
                  {"lastGood", state.value("lastGood").toString() == QLatin1String("bundled")
                                   ? QJsonValue(QStringLiteral("bundled"))
                                   : version(state.value("lastGood"))}};
  out["rolledBack"] = rolledBack.isEmpty() ? QJsonValue(QJsonValue::Null)
                                           : QJsonValue(QJsonObject{{"from", rolledBack.value("from")},
                                                                    {"to", rolledBack.value("to")}});
  return out;
}

QString startCoreUpdate(const QString &version, UpdateEvent onEvent)
{
  if (!validVersion(version) || isBad(readState(), version))
    return QStringLiteral("bad-version");
  if (version == QLatin1String(ROADIE_SIGNAL_VERSION))
    return QStringLiteral("already-running");
  if (busy)
    return QStringLiteral("busy");
  busy = true;
  auto update = std::make_shared<Update>();
  update->version = version;
  update->onEvent = std::move(onEvent);
  update->start();
  return {};
}

QString revertCore()
{
  const QJsonObject state = readState();
  const QString lastGood = state.value("lastGood").toString();
  const QString running = QString::fromLatin1(ROADIE_SIGNAL_VERSION);
  if (lastGood.isEmpty() || lastGood == running || lastGood == state.value("current").toString())
    return QStringLiteral("nothing-to-revert");
  const bool ok = updateState([&lastGood](QJsonObject &next) {
    if (lastGood == QLatin1String("bundled")) {
      // The loader runs the installer's core when neither pending nor current names a version.
      next.remove("pending");
      next["current"] = lastGood;
    } else {
      next["pending"] = lastGood;
    }
  });
  return ok ? QString() : QStringLiteral("write-failed");
}

bool verifyUpdateSignature(const QByteArray &message, const QByteArray &signature)
{
  if (signature.size() != 64 || message.isEmpty())
    return false;
  const QByteArray digest = QCryptographicHash::hash(message, QCryptographicHash::Sha256);
  return verifyUpdateDigest(reinterpret_cast<const unsigned char *>(digest.constData()),
                            static_cast<unsigned long>(digest.size()),
                            reinterpret_cast<const unsigned char *>(signature.constData()),
                            static_cast<unsigned long>(signature.size()));
}
