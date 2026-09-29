#include "signal-dock.hpp"
#include <browser-panel.hpp>
#include <obs-module.h>
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
  char *cookies = obs_module_config_path("browser-cookies");
  if (!cookies)
    return;
  if (os_mkdirs(cookies) < 0) {
    bfree(cookies);
    return;
  }
  // Keep the manager alive until after the browser; store cookies only in the
  // plugin's runtime config directory. No dashboard credentials in tracked config.
  cookieManager_.reset(cef->create_cookie_manager(cookies, true));
  bfree(cookies);
  if (!cookieManager_)
    return;
  browser_ = cef->create_widget(this, (origin() + "/dock").toStdString(), cookieManager_.get());
  if (!browser_)
    return;
  layout->addWidget(browser_);
  // String-based Qt connection avoids linking to obs-browser's private metaobject
  // symbols. The SDK's actual QCefWidget type supplies the virtual browser methods.
  QObject::connect(browser_, SIGNAL(urlChanged(QString)), this, SLOT(navigationChanged(QString)));
  timer_.setInterval(100);
  connect(&timer_, &QTimer::timeout, this, [this] { publish(); });
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
}

bool SignalDock::initialized() const { return !browser_.isNull(); }

SignalDock::~SignalDock()
{
  timer_.stop();
  audio_.detach();
  // Deleting the child browser must precede tearing down the native callback state.
  if (browser_)
    delete browser_.data();
  if (cookieManager_)
    cookieManager_->FlushStore();
}

void SignalDock::navigationChanged(const QString &url)
{
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
  if (!browser_ || !trustedPage_)
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
