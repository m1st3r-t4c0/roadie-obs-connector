#include "signal-dock.hpp"
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
std::unique_ptr<QCef> browser;
QPointer<SignalDock> dock;
QPointer<QAction> microphoneAction;
QPointer<QAction> statusAction;
const char *dockStatus = "Dock startup has not run";

void addStatusAction()
{
  if (statusAction)
    return;
  statusAction = static_cast<QAction *>(
      obs_frontend_add_tools_menu_qaction("Roadie Signal " ROADIE_SIGNAL_VERSION " status…"));
  if (statusAction) {
    QObject::connect(statusAction, &QAction::triggered, [] {
      QMessageBox::information(nullptr, "Roadie Signal status",
                               QString("Roadie Signal %1\nDock: %2\nOBS: %3\nQt: %4")
                                   .arg(ROADIE_SIGNAL_VERSION, dockStatus, obs_get_version_string(), qVersion()));
    });
  }
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
  dock = new SignalDock(browser.get());
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
    });
  }
}

void stopDock()
{
  // closeBrowser runs a nested event loop; a second shutdown callback must
  // not remove the parent while the first close is still in progress.
  static bool stopping = false;
  if (stopping)
    return;
  stopping = true;
  if (statusAction)
    delete statusAction.data();
  if (microphoneAction)
    delete microphoneAction.data();
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
  }
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
