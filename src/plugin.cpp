#include "signal-dock.hpp"
#include "scene-events.hpp"
#include <browser-panel.hpp>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <QAction>
#include <QMessageBox>
#include <QPointer>
#include <memory>
#include <QVersionNumber>

OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_description()
{
  return "Roadie Signal " ROADIE_SIGNAL_VERSION " native dock with a local OBS audio bridge";
}

namespace {
constexpr const char *dockId = "roadie-signal";
// The staging dock reports the same OBS state to the staging server, for testing
// Roadie there. It exists only while the user has turned it on from the Tools menu.
constexpr const char *stagingDockId = "roadie-signal-staging";
std::unique_ptr<QCef> browser;
std::unique_ptr<SceneEvents> sceneEvents;
QPointer<SignalDock> dock;
QPointer<SignalDock> stagingDock;
QPointer<QAction> microphoneAction;
QPointer<QAction> statusAction;
QPointer<QAction> stagingAction;
const char *dockStatus = "Dock startup has not run";
const char *stagingStatus = "Off";

QString stagingOrigin() { return QString::fromUtf8(ROADIE_SIGNAL_STAGING_ORIGIN); }

bool stagingWanted()
{
  char *path = obs_module_config_path("settings.json");
  obs_data_t *settings = path ? obs_data_create_from_json_file(path) : nullptr;
  bfree(path);
  if (!settings)
    return false;
  const bool wanted = obs_data_get_bool(settings, "stagingDock");
  obs_data_release(settings);
  return wanted;
}

void saveStagingWanted(bool wanted)
{
  char *path = obs_module_config_path("settings.json");
  if (!path)
    return;
  // Read, change, write: settings.json also holds the microphone selection.
  obs_data_t *settings = obs_data_create_from_json_file(path);
  if (!settings)
    settings = obs_data_create();
  obs_data_set_bool(settings, "stagingDock", wanted);
  if (!obs_data_save_json_safe(settings, path, "tmp", "bak"))
    blog(LOG_WARNING, "[roadie-signal] staging dock setting could not be saved");
  obs_data_release(settings);
  bfree(path);
}

template <typename Fn> void eachDock(Fn fn)
{
  if (dock)
    fn(dock.data());
  if (stagingDock)
    fn(stagingDock.data());
}

void addStatusAction()
{
  if (statusAction)
    return;
  statusAction = static_cast<QAction *>(
      obs_frontend_add_tools_menu_qaction("Roadie Signal " ROADIE_SIGNAL_VERSION " status…"));
  if (statusAction) {
    QObject::connect(statusAction, &QAction::triggered, [] {
      QMessageBox::information(nullptr, "Roadie Signal status",
                               QString("Roadie Signal %1\nDock: %2\nStaging dock: %3\nOBS: %4\nQt: %5")
                                   .arg(ROADIE_SIGNAL_VERSION, dockStatus, stagingStatus, obs_get_version_string(),
                                        qVersion()));
    });
  }
}

bool startStagingDock()
{
  if (stagingDock)
    return true;
  if (!browser || !dock) {
    stagingStatus = "Unavailable: the main dock did not start";
    return false;
  }
  stagingDock = new SignalDock(browser.get(), stagingOrigin(), "roadie-signal-staging/browser-cookies");
  if (!stagingDock->initialized()) {
    delete stagingDock.data();
    stagingStatus = "Dock initialization failed; check the OBS log";
    blog(LOG_WARNING, "[roadie-signal] staging dock initialization failed before registration");
    return false;
  }
  if (!obs_frontend_add_dock_by_id(stagingDockId, "Roadie Signal (Staging)", stagingDock)) {
    delete stagingDock.data();
    stagingStatus = "OBS rejected dock registration";
    blog(LOG_WARNING, "[roadie-signal] staging dock registration failed");
    return false;
  }
  stagingStatus = "On; see Docks > Roadie Signal (Staging)";
  blog(LOG_INFO, "[roadie-signal] staging dock registered");
  return true;
}

void stopStagingDock()
{
  if (!stagingDock)
    return;
  // Same order as the main dock: close CEF with the widget tree whole, then unregister.
  stagingDock->prepareForShutdown();
  obs_frontend_remove_dock(stagingDockId);
  if (stagingDock)
    delete stagingDock.data();
  stagingStatus = "Off";
  blog(LOG_INFO, "[roadie-signal] staging dock removed");
}

void addStagingAction()
{
  if (stagingAction || stagingOrigin().isEmpty())
    return;
  stagingAction = static_cast<QAction *>(
      obs_frontend_add_tools_menu_qaction("Roadie Signal: also report to staging"));
  if (!stagingAction)
    return;
  stagingAction->setCheckable(true);
  stagingAction->setChecked(stagingWanted() && stagingDock);
  // triggered fires only for the user's click, so the setChecked rollback below cannot re-enter.
  QObject::connect(stagingAction, &QAction::triggered, [](bool on) {
    if (on && !startStagingDock()) {
      if (stagingAction)
        stagingAction->setChecked(false);
      QMessageBox::warning(nullptr, "Roadie Signal", QString("The staging dock could not start: %1").arg(stagingStatus));
      return;
    }
    if (!on)
      stopStagingDock();
    saveStagingWanted(on);
  });
}

void startDock()
{
  if (dock)
    return;
  // The matching upstream helper looks up the module by its binary identity,
  // rather than the human-readable name returned by obs_get_module_name.
  if (obs_browser_qcef_version() != 3) {
    dockStatus = "Browser panel API version 3 is unavailable";
    blog(LOG_WARNING, "[roadie-signal] matching browser panel API version 3 required");
    return;
  }
  browser.reset(obs_browser_init_panel());
  if (browser)
    browser->init_browser(); // may already be initialized by OBS
  if (!browser || !browser->wait_for_browser_init()) {
    browser.reset();
    dockStatus = "Browser initialization failed";
    blog(LOG_WARNING, "[roadie-signal] browser initialization failed");
    return;
  }
  dock = new SignalDock(browser.get(), QString::fromUtf8(ROADIE_SIGNAL_ORIGIN), "roadie-signal/browser-cookies");
  if (!dock->initialized()) {
    delete dock.data();
    browser.reset();
    dockStatus = "Dock initialization failed; check the OBS log";
    blog(LOG_WARNING, "[roadie-signal] dock initialization failed before registration");
    return;
  }
  if (!obs_frontend_add_dock_by_id(dockId, "Roadie Signal", dock)) {
    delete dock.data();
    browser.reset();
    dockStatus = "OBS rejected dock registration";
    blog(LOG_WARNING, "[roadie-signal] dock registration failed");
    return;
  }
  dockStatus = "Registered; see Docks > Roadie Signal";
  blog(LOG_INFO, "[roadie-signal] Roadie Signal dock registered");
  microphoneAction = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction("Roadie Signal microphone…"));
  if (microphoneAction) {
    QObject::connect(microphoneAction, &QAction::triggered, dock, [] {
      if (dock)
        dock->chooseMicrophone();
      // Both docks read one saved selection; the staging dock follows the chooser.
      if (stagingDock)
        stagingDock->reloadMicrophone();
    });
  }
  if (!stagingOrigin().isEmpty() && stagingWanted())
    startStagingDock();
  addStagingAction();
}

void stopDock()
{
  // closeBrowser runs a nested event loop; a second shutdown callback must
  // not remove the parent while the first close is still in progress.
  static bool stopping = false;
  if (stopping)
    return;
  stopping = true;
  sceneEvents.reset();
  if (statusAction)
    delete statusAction.data();
  if (microphoneAction)
    delete microphoneAction.data();
  if (stagingAction)
    delete stagingAction.data();
  stopStagingDock();
  if (dock) {
    dock->prepareForShutdown();
    obs_frontend_remove_dock(dockId);
    if (dock)
      delete dock.data();
  }
  browser.reset();
  stopping = false;
}

void frontendEvent(enum obs_frontend_event event, void *)
{
  if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
    addStatusAction();
    startDock();
    sceneEvents = std::make_unique<SceneEvents>();
  }
  if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED && sceneEvents)
    QMetaObject::invokeMethod(sceneEvents.get(), [] {
      if (sceneEvents) sceneEvents->currentScene(true);
    }, Qt::QueuedConnection);
  switch (event) {
  case OBS_FRONTEND_EVENT_SCENE_CHANGED:
  case OBS_FRONTEND_EVENT_STREAMING_STARTED:
  case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
  case OBS_FRONTEND_EVENT_RECORDING_STARTED:
  case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
  case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
  case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
    eachDock([](SignalDock *each) {
      QMetaObject::invokeMethod(each, [target = QPointer<SignalDock>(each)] {
        if (target) target->publishState();
      }, Qt::QueuedConnection);
    });
    break;
  default: break;
  }
  // The output still exists while stopping: take the night's last health sample.
  if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPING)
    eachDock([](SignalDock *each) { each->publishHealth(); });
  // Close CEF while the complete widget tree is alive, then unregister the dock.
  // Starting CEF close from the dock destructor permits nested paint events to
  // visit a partially destroyed Qt tree.
  if (event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN)
    stopDock();
}
}

bool obs_module_load()
{
  if (obs_get_version() < MAKE_SEMANTIC_VERSION(30, 0, 0)) {
    blog(LOG_WARNING, "[roadie-signal] OBS 30.0.0 or newer required");
    return false;
  }
  const auto runtimeQt = QVersionNumber::fromString(QString::fromLatin1(qVersion()));
  const QVersionNumber minimumQt(QT_VERSION_MAJOR, QT_VERSION_MINOR, QT_VERSION_PATCH);
  if (runtimeQt.majorVersion() != QT_VERSION_MAJOR || runtimeQt < minimumQt) {
    blog(LOG_WARNING, "[roadie-signal] compatible Qt 6.5.3 or newer required; runtime is %s", qVersion());
    return false;
  }
  blog(LOG_INFO, "[roadie-signal] native dock %s; OBS %s; Qt %s; baseline OBS 30.0.0 / Qt 6.5.3",
       ROADIE_SIGNAL_VERSION, obs_get_version_string(), qVersion());
  obs_frontend_add_event_callback(frontendEvent, nullptr);
  return true;
}

void obs_module_unload()
{
  obs_frontend_remove_event_callback(frontendEvent, nullptr);
  stopDock();
}
