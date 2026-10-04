#include "signal-dock.hpp"
#include "crash-reports.hpp"
#include "stream-health.hpp"
#include <browser-panel.hpp>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLabel>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>

static QString origin() { return QString::fromUtf8(ROADIE_SIGNAL_ORIGIN); }

SignalDock::SignalDock(QCef *cef, QWidget *parent)
    : QWidget(parent), sessionId_(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  // Settings belong to this plugin. Browser cookies use obs-browser's own
  // configuration root, so its API must receive a relative storage name.
  char *settingsDirectory = obs_module_config_path("");
  if (!settingsDirectory) {
    blog(LOG_WARNING, "[roadie-signal] settings directory is unavailable");
    return;
  }
  const int directoryResult = os_mkdirs(settingsDirectory);
  bfree(settingsDirectory);
  if (directoryResult == MKDIR_ERROR) {
    blog(LOG_WARNING, "[roadie-signal] settings directory could not be created");
    return;
  }
  // QCef prefixes this name with obs-browser's config path. Passing an absolute
  // path creates an invalid nested drive path on Windows and returns nullptr.
  // Keep this namespace stable so sign-in cookies survive plugin upgrades.
  cookieManager_.reset(cef->create_cookie_manager("roadie-signal/browser-cookies", true));
  if (!cookieManager_) {
    blog(LOG_WARNING, "[roadie-signal] browser cookie manager could not be created");
    return;
  }
  browser_ = cef->create_widget(this, (origin() + "/dock").toStdString(), cookieManager_.get());
  if (!browser_) {
    blog(LOG_WARNING, "[roadie-signal] browser widget could not be created");
    return;
  }
  layout->addWidget(browser_);
  // String-based Qt connection avoids linking to obs-browser's private metaobject
  // symbols. The SDK's actual QCefWidget type supplies the virtual browser methods.
  QObject::connect(browser_, SIGNAL(urlChanged(QString)), this, SLOT(navigationChanged(QString)));
  // 0.1.9: the page's only way back to the plugin. browser-panel.hpp exposes no
  // JS-to-native call, so a command rides the document title (see titleChanged).
  QObject::connect(browser_, SIGNAL(titleChanged(QString)), this, SLOT(titleChanged(QString)));
  timer_.setInterval(100);
  connect(&timer_, &QTimer::timeout, this, [this] { publish(); });
  stateTimer_.setInterval(5000);
  connect(&stateTimer_, &QTimer::timeout, this, [this] { publishState(); });
  // Flush persistent cookies while running. OBS ignores the session-cookie
  // persistence flag; Roadie must issue an expiring cookie to survive restart.
  // Stream health is sampled only while streaming; Roadie folds samples into minutes.
  healthTimer_.setInterval(5000);
  connect(&healthTimer_, &QTimer::timeout, this, [this] {
    if (obs_frontend_streaming_active())
      publishHealth();
  });
  cookieFlushTimer_.setInterval(30000);
  connect(&cookieFlushTimer_, &QTimer::timeout, this, [this] {
    if (cookieManager_)
      cookieManager_->FlushStore();
  });
  char *config = obs_module_config_path("settings.json");
  obs_data_t *settings = config ? obs_data_create_from_json_file(config) : nullptr;
  bfree(config);
  if (settings) {
    selectedUuid_ = QString::fromUtf8(obs_data_get_string(settings, "microphoneUuid"));
    obs_data_release(settings);
  }
  audio_.select(selectedUuid_);
  // A freshly constructed browser starts on our exact URL. Every injected script
  // also checks the actual frame origin/path, covering redirects before urlChanged.
  trustedPage_ = true;
  timer_.start();
  stateTimer_.start();
  healthTimer_.start();
  cookieFlushTimer_.start();
}

bool SignalDock::initialized() const { return !browser_.isNull(); }

SignalDock::~SignalDock()
{
  timer_.stop();
  stateTimer_.stop();
  healthTimer_.stop();
  cookieFlushTimer_.stop();
  audio_.detach();
  if (cookieManager_)
    cookieManager_->FlushStore();
  if (browser_)
    delete browser_.data();
}

void SignalDock::prepareForShutdown()
{
  if (shutdownPrepared_)
    return;
  shutdownPrepared_ = true;
  timer_.stop();
  stateTimer_.stop();
  healthTimer_.stop();
  cookieFlushTimer_.stop();
  audio_.detach();
  if (cookieManager_)
    cookieManager_->FlushStore();
  if (!browser_)
    return;

  // closeBrowser pumps Qt events while waiting for CEF. Do this with the whole
  // dock tree alive, before obs_frontend_remove_dock starts destroying it.
  // A queued paint during parent-first deletion reproduces c0000005 in the
  // OBS 32.0.4 browser probe; pre-closing leaves the later destructor a no-op.
  trustedPage_ = false;
  QObject::disconnect(browser_, nullptr, this, nullptr);
  browser_->closeBrowser();
  blog(LOG_INFO, "[roadie-signal] browser closed before dock destruction");
}

void SignalDock::navigationChanged(const QString &url)
{
  if (shutdownPrepared_)
    return;
  const QUrl actual(url);
  const QUrl expected(origin());
  trustedPage_ = actual.scheme() == expected.scheme() && actual.host() == expected.host() &&
                 actual.port() == expected.port() && actual.path() == "/dock" &&
                 actual.userInfo().isEmpty();
  if (!trustedPage_) {
    audio_.detach();
  } else {
    audio_.select(selectedUuid_);
    crashDeliveries_ = 0; // a reloaded page has not seen the crash reports yet
  }
}

static const QString commandPrefix = QStringLiteral("roadie-obs-command:");

static bool validCommandId(const QString &id)
{
  if (id.isEmpty() || id.size() > 64)
    return false;
  for (const QChar c : id) {
    if (!(c.isLetterOrNumber() && c.unicode() < 128) && c != QLatin1Char('-'))
      return false;
  }
  return true;
}

void SignalDock::titleChanged(const QString &title)
{
  // Only the trusted /dock page may command OBS, and only with the exact prefix.
  // Any other title (the page's own name, a redirect's) is ignored.
  if (!browser_ || !trustedPage_ || shutdownPrepared_ || !title.startsWith(commandPrefix))
    return;
  const QByteArray json = title.mid(commandPrefix.size()).toUtf8();
  if (json.size() > 4096)
    return;
  QJsonParseError error{};
  const QJsonDocument document = QJsonDocument::fromJson(json, &error);
  if (error.error != QJsonParseError::NoError || !document.isObject())
    return;
  const QJsonObject command = document.object();
  const QString id = command.value("id").toString();
  // The page holds a command's title until the result arrives, and CEF may report
  // one title more than once, so an id is run at most once.
  if (command.value("version").toInt() != 1 || !validCommandId(id) || id == lastCommandId_)
    return;
  lastCommandId_ = id;
  runCommand(command);
}

void SignalDock::runCommand(const QJsonObject &command)
{
  const QString id = command.value("id").toString();
  const QString action = command.value("action").toString();
  QJsonObject result{{"version", 1}, {"id", id}, {"action", action}};
  if (action == QLatin1String("set-scene")) {
    const QString wanted = command.value("scene").toString();
    bool found = false;
    if (!wanted.isEmpty() && wanted.size() <= 256) {
      // Only OBS's own scene list is searched, so a command can never select a
      // source that is not a scene.
      obs_frontend_source_list scenes = {};
      obs_frontend_get_scenes(&scenes);
      for (size_t i = 0; i < scenes.sources.num; ++i) {
        obs_source_t *scene = scenes.sources.array[i];
        if (QString::fromUtf8(obs_source_get_name(scene)) == wanted) {
          obs_frontend_set_current_scene(scene);
          found = true;
          break;
        }
      }
      obs_frontend_source_list_free(&scenes);
    }
    result["ok"] = found;
    if (!found)
      result["error"] = QStringLiteral("no-such-scene");
    else
      blog(LOG_INFO, "[roadie-signal] Roadie switched the scene");
  } else {
    result["ok"] = false;
    result["error"] = QStringLiteral("unknown-action");
  }
  dispatch("roadie:obs-command-result", "roadieObsCommand", result);
}

void SignalDock::publish()
{
  if (!browser_ || !trustedPage_ || shutdownPrepared_)
    return;
  QJsonObject frame = audio_.reading();
  frame["version"] = 1;
  frame["sessionId"] = sessionId_;
  frame["sequence"] = static_cast<double>(++sequence_);
  const QByteArray originJson = QJsonDocument(QJsonArray{origin()}).toJson(QJsonDocument::Compact);
  const QByteArray payload = QJsonDocument(frame).toJson(QJsonDocument::Compact);
  const QByteArray script =
      "(()=>{if(window.top!==window||location.origin!==" + originJson +
      "[0]||location.pathname!=='/dock'||document.documentElement.dataset.roadieObsAudio!=='1')return;"
      "window.dispatchEvent(new CustomEvent('roadie:obs-audio',{detail:" + payload + "}));})()";
  browser_->executeJavaScript(script.toStdString());
}

void SignalDock::publishState()
{
  if (!browser_ || !trustedPage_ || shutdownPrepared_)
    return;
  obs_source_t *sceneSource = obs_frontend_get_current_scene();
  const QString scene = sceneSource ? QString::fromUtf8(obs_source_get_name(sceneSource)) : QString();
  if (sceneSource)
    obs_source_release(sceneSource);
  const QJsonObject state{{"streaming", obs_frontend_streaming_active()},
                          {"recording", obs_frontend_recording_active()},
                          {"scene", scene.isEmpty() ? QJsonValue::Null : QJsonValue(scene)}};
  // Frontend callbacks publish changes immediately; this timer only supplies presence.
  QJsonObject frame = state;
  frame["version"] = 1;
  frame["sessionId"] = sessionId_;
  frame["sequence"] = static_cast<double>(++stateSequence_);
  // Additive fields: receivers that predate them drop unknown keys.
  frame["pluginVersion"] = QString::fromLatin1(ROADIE_SIGNAL_VERSION);
  frame["obsVersion"] = QString::fromLatin1(obs_get_version_string());
  // 0.1.9: the scene names Roadie may switch to, and what this build can be asked.
  QJsonArray sceneNames;
  obs_frontend_source_list scenes = {};
  obs_frontend_get_scenes(&scenes);
  for (size_t i = 0; i < scenes.sources.num && sceneNames.size() < 200; ++i)
    sceneNames.append(QString::fromUtf8(obs_source_get_name(scenes.sources.array[i])));
  obs_frontend_source_list_free(&scenes);
  frame["scenes"] = sceneNames;
  frame["capabilities"] = QJsonArray{QStringLiteral("set-scene")};
  dispatch("roadie:obs-state", "roadieObsState", frame);
  publishCrashReports();
}

void SignalDock::publishHealth()
{
  if (!browser_ || !trustedPage_ || shutdownPrepared_)
    return;
  QJsonObject frame = sampleStreamHealth();
  frame["version"] = 1;
  frame["sessionId"] = sessionId_;
  frame["sequence"] = static_cast<double>(++healthSequence_);
  dispatch("roadie:obs-health", "roadieObsHealth", frame);
}

void SignalDock::publishCrashReports()
{
  // Sent with the first three state heartbeats after each trusted page load, so
  // a page that mounts late still receives them. Roadie dedupes by sha256.
  if (!browser_ || !trustedPage_ || shutdownPrepared_ || crashDeliveries_ >= 3)
    return;
  if (!crashReportsRead_) {
    crashReports_ = recentCrashReports();
    crashReportsRead_ = true;
  }
  ++crashDeliveries_;
  if (crashReports_.isEmpty())
    return;
  const QJsonObject frame{{"version", 1},
                          {"sessionId", sessionId_},
                          {"pluginVersion", QString::fromLatin1(ROADIE_SIGNAL_VERSION)},
                          {"obsVersion", QString::fromLatin1(obs_get_version_string())},
                          {"reports", crashReports_}};
  dispatch("roadie:obs-crash", "roadieObsCrash", frame);
}

void SignalDock::dispatch(const char *event, const char *marker, const QJsonObject &detail)
{
  // Every script re-checks the live frame's origin, exact /dock path and the
  // marker the signed-in provider sets; nothing reaches any other page.
  const QByteArray originJson = QJsonDocument(QJsonArray{origin()}).toJson(QJsonDocument::Compact);
  const QByteArray payload = QJsonDocument(detail).toJson(QJsonDocument::Compact);
  const QByteArray script = QByteArray("(()=>{if(window.top!==window||location.origin!==") + originJson +
                            "[0]||location.pathname!=='/dock'||document.documentElement.dataset." + marker +
                            "!=='1')return;window.dispatchEvent(new CustomEvent('" + event + "',{detail:" + payload +
                            "}));})()";
  browser_->executeJavaScript(script.toStdString());
}

static bool addAudioSource(void *param, obs_source_t *source)
{
  if (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) {
    auto *choices = static_cast<QComboBox *>(param);
    choices->addItem(QString::fromUtf8(obs_source_get_name(source)),
                     QString::fromUtf8(obs_source_get_uuid(source)));
  }
  return true;
}

void SignalDock::chooseMicrophone()
{
  QDialog dialog(this);
  dialog.setWindowTitle("Roadie Signal microphone");
  auto *layout = new QVBoxLayout(&dialog);
  layout->addWidget(new QLabel("Choose the OBS source whose audio levels Signal can read.", &dialog));
  auto *choices = new QComboBox(&dialog);
  choices->addItem("None — microphone sharing off", "");
  obs_enum_sources(addAudioSource, choices);
  int selected = choices->findData(selectedUuid_);
  if (selected < 0 && !selectedUuid_.isEmpty()) {
    choices->addItem("Selected source unavailable", selectedUuid_);
    selected = choices->count() - 1;
  }
  choices->setCurrentIndex(selected < 0 ? 0 : selected);
  layout->addWidget(choices);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttons);
  if (dialog.exec() != QDialog::Accepted)
    return;
  selectedUuid_ = choices->currentData().toString();
  saveSelection();
  if (trustedPage_)
    audio_.select(selectedUuid_);
}

void SignalDock::saveSelection()
{
  char *path = obs_module_config_path("settings.json");
  if (!path)
    return;
  obs_data_t *settings = obs_data_create();
  obs_data_set_string(settings, "microphoneUuid", selectedUuid_.toUtf8().constData());
  if (!obs_data_save_json_safe(settings, path, "tmp", "bak"))
    blog(LOG_WARNING, "[roadie-signal] microphone selection could not be saved");
  obs_data_release(settings);
  bfree(path);
}
