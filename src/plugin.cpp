#include "signal-dock.hpp"
#include <browser-panel.hpp>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <QAction>
#include <QPointer>
#include <memory>

OBS_DECLARE_MODULE()
MODULE_EXPORT const char *obs_module_description()
{
  return "Roadie Signal native dock with a local OBS audio bridge";
}

namespace {
constexpr const char *dockId = "roadie-signal";
std::unique_ptr<QCef> browser;
QPointer<SignalDock> dock;
QPointer<QAction> microphoneAction;

void startDock()
{
  if (dock)
    return;
  // The matching upstream helper looks up the module by its binary identity,
  // rather than the human-readable name returned by obs_get_module_name.
  if (obs_browser_qcef_version() != 3) {
    blog(LOG_WARNING, "[roadie-signal] matching browser panel API version 3 required");
    return;
  }
  browser.reset(obs_browser_init_panel());
  if (browser)
    browser->init_browser(); // may already be initialized by OBS
  if (!browser || !browser->wait_for_browser_init()) {
    browser.reset();
    blog(LOG_WARNING, "[roadie-signal] browser initialization failed");
    return;
  }
  dock = new SignalDock(browser.get());
  if (!dock->initialized() || !obs_frontend_add_dock_by_id(dockId, "Roadie Signal", dock)) {
    delete dock.data();
    browser.reset();
    blog(LOG_WARNING, "[roadie-signal] dock registration failed");
    return;
  }
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
  if (microphoneAction)
    delete microphoneAction.data();
  if (dock) {
    obs_frontend_remove_dock(dockId);
    if (dock)
      delete dock.data();
  }
  browser.reset();
}

void frontendEvent(enum obs_frontend_event event, void *)
{
  if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING)
    startDock();
  if (event == OBS_FRONTEND_EVENT_EXIT)
    stopDock();
}
}

bool obs_module_load()
{
  if (obs_get_version() != LIBOBS_API_VER) {
    blog(LOG_WARNING, "[roadie-signal] this test build requires OBS 32.2.1");
    return false;
  }
  obs_frontend_add_event_callback(frontendEvent, nullptr);
  return true;
}

void obs_module_unload()
{
  obs_frontend_remove_event_callback(frontendEvent, nullptr);
  stopDock();
}
