#include "signal-dock.hpp"
#include "crash-reports.hpp"
#include "obs-commands.hpp"
#include "stream-health.hpp"
#include <browser-panel.hpp>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <QJsonDocument>
#include <QApplication>
#include <QThread>
#include <QJsonArray>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>

#ifdef __APPLE__
#include <objc/message.h>
#include <objc/runtime.h>
// A native NSAlert (QMessageBox on macOS) runs its own AppKit modal loop that Qt's
// modal bookkeeping and loop level do not always reflect; ask AppKit directly.
static bool nativeModalActive()
{
  using Send = void *(*)(void *, SEL);
  auto send = reinterpret_cast<Send>(objc_msgSend);
  void *appClass = reinterpret_cast<void *>(objc_getClass("NSApplication"));
  if (!appClass)
    return false;
  void *app = send(appClass, sel_registerName("sharedApplication"));
  return app && send(app, sel_registerName("modalWindow")) != nullptr;
}
#endif

static bool scriptsPaused = false;

void roadieSetScriptsPaused(bool paused) { scriptsPaused = paused; }

// macOS: CEF can drop a browser's main frame while OBS closes (QCefWidget::executeJavaScript then
// dereferences null). Never call into the page during shutdown, under a modal dialog, or from a
// nested event loop: in normal running the main thread sits at loop level 1. Windows only ever
// pauses through roadieSetScriptsPaused, which nothing there calls (see plugin.cpp).
bool roadieScriptsSafe()
{
#ifdef __APPLE__
  return !scriptsPaused && QThread::currentThread()->loopLevel() <= 1 && !QApplication::activeModalWidget() &&
         !QGuiApplication::modalWindow() && !nativeModalActive();
#else
  return !scriptsPaused;
#endif
}

static QString savedMicrophone()
{
  char *config = obs_module_config_path("settings.json");
  obs_data_t *settings = config ? obs_data_create_from_json_file(config) : nullptr;
  bfree(config);
  if (!settings)
    return QString();
  const QString uuid = QString::fromUtf8(obs_data_get_string(settings, "microphoneUuid"));
  obs_data_release(settings);
  return uuid;
}

SignalDock::SignalDock(QCef *cef, const QString &origin, QCefCookieManager *cookies, QWidget *parent)
    : QWidget(parent), origin_(origin), sessionId_(QUuid::createUuid().toString(QUuid::WithoutBraces))
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
  cookieManager_ = cookies;
  if (!cookieManager_) {
    blog(LOG_WARNING, "[roadie-signal] browser cookie manager is unavailable");
    return;
  }
  browser_ = cef->create_widget(this, (origin_ + "/dock").toStdString(), cookieManager_);
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
  signInTimer_.setInterval(5000);
  connect(&signInTimer_, &QTimer::timeout, this, [this] { signInIfSignedOut(); });
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
  selectedUuid_ = savedMicrophone();
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

void SignalDock::setRoles(bool sends, bool obeys)
{
  const bool changed = sends != sends_ || obeys != obeys_;
  sends_ = sends;
  obeys_ = obeys;
  // Tell the server at once: new capabilities, or the first state after sending starts.
  if (changed && sends_)
    publishState();
}

void SignalDock::setBackground(bool background)
{
  background_ = background;
  if (background_ && !shutdownPrepared_)
    signInTimer_.start();
  else
    signInTimer_.stop();
}

void SignalDock::signInIfSignedOut()
{
  if (!browser_ || !trustedPage_ || !background_ || shutdownPrepared_)
    return;
  // At most one attempt a minute, so a sign-in that keeps failing never loops.
  if (lastSignIn_.isValid() && lastSignIn_.elapsed() < 60000)
    return;
  // Signed in, the dock page marks the document (roadieObsState); signed out it shows
  // Roadie's sign-in screen, whose Twitch link is followed here. Only on the trusted page.
  const QByteArray originJson = QJsonDocument(QJsonArray{origin_}).toJson(QJsonDocument::Compact);
  const QByteArray script =
      "(()=>{if(window.top!==window||location.origin!==" + originJson +
      "[0]||location.pathname!=='/dock'||document.documentElement.dataset.roadieObsState==='1')return;"
      "const a=document.querySelector('a[href^=\"/auth/twitch/login\"]');if(a)location.href=a.href;})()";
  lastSignIn_.start();
  if (!roadieScriptsSafe())
    return;
  browser_->executeJavaScript(script.toStdString());
}

void SignalDock::reloadMicrophone()
{
  selectedUuid_ = savedMicrophone();
  if (trustedPage_)
    audio_.select(selectedUuid_);
}

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
  signInTimer_.stop();
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
  const QUrl expected(origin_);
  trustedPage_ = actual.scheme() == expected.scheme() && actual.host() == expected.host() &&
                 actual.port() == expected.port() && actual.path() == "/dock" &&
                 actual.userInfo().isEmpty();
  lastHost_ = actual.host();
  if (!trustedPage_) {
    audio_.detach();
  } else {
    audio_.select(selectedUuid_);
    crashDeliveries_ = 0; // a reloaded page has not seen the crash reports yet
  }
  emit pageChanged();
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

// 0.3.0: a command too long for one title (Chromium cuts a title at 4096 characters, and set-text
// carries up to 4000) arrives in parts, each acknowledged before the page writes the next.
static const QString partPrefix = QStringLiteral("roadie-obs-command-part:");
static constexpr int maxParts = 16;
static constexpr int maxCommandBytes = 32768;

void SignalDock::titleChanged(const QString &title)
{
  // Only the trusted /dock page may command OBS, and only with an exact prefix.
  // Any other title (the page's own name, a redirect's) is ignored.
  if (!browser_ || !trustedPage_ || shutdownPrepared_)
    return;
  if (title.startsWith(partPrefix)) {
    commandPart(title.mid(partPrefix.size()));
    return;
  }
  if (!title.startsWith(commandPrefix))
    return;
  const QByteArray json = title.mid(commandPrefix.size()).toUtf8();
  if (json.size() > 4096)
    return;
  commandJson(json);
}

void SignalDock::commandPart(const QString &body)
{
  QJsonParseError error{};
  const QJsonDocument document = QJsonDocument::fromJson(body.toUtf8(), &error);
  if (error.error != QJsonParseError::NoError || !document.isObject())
    return;
  const QJsonObject part = document.object();
  const QString id = part.value("id").toString();
  const int index = part.value("index").toInt(-1);
  const int count = part.value("count").toInt(0);
  if (part.value("version").toInt() != 1 || !validCommandId(id) || id == lastCommandId_ || count < 2 ||
      count > maxParts || index < 0 || index >= count || !part.value("data").isString())
    return;
  if (index == 0 && partsId_ != id) {
    partsId_ = id;
    partsCount_ = count;
    parts_.clear();
  }
  // Parts arrive in order, one at a time; a repeat of the last one is acknowledged again.
  if (partsId_ != id || partsCount_ != count || index > parts_.size())
    return;
  if (index == parts_.size()) {
    parts_.append(part.value("data").toString());
    int total = 0;
    for (const QString &each : parts_)
      total += each.size();
    if (total > maxCommandBytes) {
      partsId_.clear();
      parts_.clear();
      return;
    }
  }
  dispatch("roadie:obs-command-part", "roadieObsCommand", QJsonObject{{"version", 1}, {"id", id}, {"index", index}});
  if (parts_.size() != partsCount_)
    return;
  const QByteArray json = parts_.join(QString()).toUtf8();
  partsId_.clear();
  parts_.clear();
  commandJson(json, id);
}

void SignalDock::commandJson(const QByteArray &json, const QString &expectedId)
{
  QJsonParseError error{};
  const QJsonDocument document = QJsonDocument::fromJson(json, &error);
  if (error.error != QJsonParseError::NoError || !document.isObject())
    return;
  const QJsonObject command = document.object();
  const QString id = command.value("id").toString();
  // The page holds a command's title until the result arrives, and CEF may report
  // one title more than once, so an id is run at most once.
  if (command.value("version").toInt() != 1 || !validCommandId(id) || id == lastCommandId_ ||
      (!expectedId.isEmpty() && id != expectedId))
    return;
  lastCommandId_ = id;
  runCommand(command);
}

void SignalDock::runCommand(const QJsonObject &command)
{
  const QString id = command.value("id").toString();
  const QString action = command.value("action").toString();
  QJsonObject result{{"version", 1}, {"id", id}, {"action", action}};
  if (!obeys_) {
    // The user chose not to take commands from this server. Answer at once so the
    // page does not wait out its timeout.
    result["ok"] = false;
    result["error"] = QStringLiteral("commands-off");
  } else {
    // Scenes are searched only in OBS's own scene list, so set-scene can never select a source
    // that is not a scene; every other command reads its change back (obs-commands.cpp).
    const QJsonObject outcome = runObsCommand(command);
    result["ok"] = outcome.value("ok");
    if (outcome.contains("error"))
      result["error"] = outcome.value("error");
    if (outcome.value("ok").toBool())
      blog(LOG_INFO, "[roadie-signal] Roadie ran %s", action.toUtf8().constData());
  }
  dispatch("roadie:obs-command-result", "roadieObsCommand", result);
}

void SignalDock::publish()
{
  if (!browser_ || !trustedPage_ || shutdownPrepared_ || !sends_)
    return;
  QJsonObject frame = audio_.reading();
  frame["version"] = 1;
  frame["sessionId"] = sessionId_;
  frame["sequence"] = static_cast<double>(++sequence_);
  const QByteArray originJson = QJsonDocument(QJsonArray{origin_}).toJson(QJsonDocument::Compact);
  const QByteArray payload = QJsonDocument(frame).toJson(QJsonDocument::Compact);
  const QByteArray script =
      "(()=>{if(window.top!==window||location.origin!==" + originJson +
      "[0]||location.pathname!=='/dock'||document.documentElement.dataset.roadieObsAudio!=='1')return;"
      "window.dispatchEvent(new CustomEvent('roadie:obs-audio',{detail:" + payload + "}));})()";
  if (!roadieScriptsSafe())
    return;
  browser_->executeJavaScript(script.toStdString());
}

void SignalDock::publishState()
{
  if (!browser_ || !trustedPage_ || shutdownPrepared_ || !sends_)
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
  // A server the user takes no commands from is not told it can command OBS.
  frame["capabilities"] = obeys_ ? obsCommandCapabilities() : QJsonArray{};
  // 0.3.0: the sources and filters Roadie's OBS steps pick from.
  const QJsonObject inventory = obsInventory();
  frame["sources"] = inventory.value("sources");
  frame["filters"] = inventory.value("filters");
  // State reaches a page only when the user chose its server under "Send OBS data to",
  // and that choice is the consent: Roadie turns sharing on without a second switch.
  frame["share"] = true;
  dispatch("roadie:obs-state", "roadieObsState", frame);
  publishCrashReports();
}

void SignalDock::publishHealth()
{
  if (!browser_ || !trustedPage_ || shutdownPrepared_ || !sends_)
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
  if (!browser_ || !trustedPage_ || shutdownPrepared_ || !sends_ || crashDeliveries_ >= 3)
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
  const QByteArray originJson = QJsonDocument(QJsonArray{origin_}).toJson(QJsonDocument::Compact);
  const QByteArray payload = QJsonDocument(detail).toJson(QJsonDocument::Compact);
  const QByteArray script = QByteArray("(()=>{if(window.top!==window||location.origin!==") + originJson +
                            "[0]||location.pathname!=='/dock'||document.documentElement.dataset." + marker +
                            "!=='1')return;window.dispatchEvent(new CustomEvent('" + event + "',{detail:" + payload +
                            "}));})()";
  if (!roadieScriptsSafe())
    return;
  browser_->executeJavaScript(script.toStdString());
}
