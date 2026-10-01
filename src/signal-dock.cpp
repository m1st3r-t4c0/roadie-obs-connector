#include "signal-dock.hpp"
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
  timer_.setInterval(100);
  connect(&timer_, &QTimer::timeout, this, [this] { publish(); });
  stateTimer_.setInterval(5000);
  connect(&stateTimer_, &QTimer::timeout, this, [this] { publishState(); });
  // Flush persistent cookies while running. OBS ignores the session-cookie
  // persistence flag; Roadie must issue an expiring cookie to survive restart.
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
  cookieFlushTimer_.start();
}

bool SignalDock::initialized() const { return !browser_.isNull(); }

SignalDock::~SignalDock()
{
  timer_.stop();
  stateTimer_.stop();
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
  }
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
  const QByteArray originJson = QJsonDocument(QJsonArray{origin()}).toJson(QJsonDocument::Compact);
  const QByteArray payload = QJsonDocument(frame).toJson(QJsonDocument::Compact);
  const QByteArray script =
      "(()=>{if(window.top!==window||location.origin!==" + originJson +
      "[0]||location.pathname!=='/dock'||document.documentElement.dataset.roadieObsState!=='1')return;"
      "window.dispatchEvent(new CustomEvent('roadie:obs-state',{detail:" + payload + "}));})()";
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
