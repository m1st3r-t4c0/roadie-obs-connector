#include "scene-events.hpp"
#include "../third_party/obs-websocket-api.h"
#include <obs-frontend-api.h>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

SceneEvents::SceneEvents() : session_(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
  signal_handler_connect(obs_get_signal_handler(), "channel_change", channelChanged, this);
  auto *program = obs_get_output_source(0);
  observe(program);
  obs_source_release(program);
  heartbeat_.setInterval(5000);
  connect(&heartbeat_, &QTimer::timeout, this, [this] { currentScene(); });
  heartbeat_.start();
  currentScene();
}

SceneEvents::~SceneEvents()
{
  heartbeat_.stop();
  signal_handler_disconnect(obs_get_signal_handler(), "channel_change", channelChanged, this);
  observe(nullptr);
}

// OBS's standard transitions are private sources, so global source_transition_*
// signals are not emitted. Bind the source signal handler before a channel swap
// returns; queuing this attachment would miss an immediately following cut.
void SceneEvents::channelChanged(void *self, calldata_t *data)
{
  if (calldata_int(data, "channel") == 0)
    static_cast<SceneEvents *>(self)->observe(static_cast<obs_source_t *>(calldata_ptr(data, "source")));
}

void SceneEvents::observe(obs_source_t *source)
{
  std::lock_guard<std::mutex> lock(sourceMutex_);
  if (source == observed_) return;
  observedIdentity_.store(nullptr);
  if (observed_) {
    auto *handler = obs_source_get_signal_handler(observed_);
    signal_handler_disconnect(handler, "transition_start", started, this);
    signal_handler_disconnect(handler, "transition_video_stop", ended, this);
    obs_source_release(observed_);
    observed_ = nullptr;
  }
  if (source && obs_source_get_type(source) == OBS_SOURCE_TYPE_TRANSITION) {
    observed_ = obs_source_get_ref(source);
    observedIdentity_.store(observed_);
    auto *handler = obs_source_get_signal_handler(observed_);
    signal_handler_connect(handler, "transition_start", started, this);
    signal_handler_connect(handler, "transition_video_stop", ended, this);
  }
}

void SceneEvents::started(void *self, calldata_t *data) { static_cast<SceneEvents *>(self)->capture(data, true); }
void SceneEvents::ended(void *self, calldata_t *data) { static_cast<SceneEvents *>(self)->capture(data, false); }

void SceneEvents::capture(calldata_t *data, bool start)
{
  auto *source = static_cast<obs_source_t *>(calldata_ptr(data, "source"));
  // channel_change holds OBS's output lock while disconnecting these handlers.
  // Never acquire that lock here: a video-thread signal could otherwise deadlock
  // the UI thread during a channel swap. The retained binding identifies program.
  if (!source || source != observedIdentity_.load()) return;
  const qint64 at = QDateTime::currentMSecsSinceEpoch();
  auto *target = obs_transition_get_source(source, start ? OBS_TRANSITION_SOURCE_B : OBS_TRANSITION_SOURCE_A);
  const QString name = target ? QString::fromUtf8(obs_source_get_name(target)) : QString();
  obs_source_release(target);
  const auto identity = reinterpret_cast<quintptr>(source);
  QMetaObject::invokeMethod(this, [this, start, name, identity, at] {
    if (start) {
      ++transition_;
      activeSource_ = identity;
      scene_ = name;
      publishEvent("start", name, at);
    } else if (activeSource_ == identity) {
      activeSource_ = 0;
      if (!name.isEmpty()) scene_ = name;
      publishEvent("end", scene_, at);
    }
  }, Qt::QueuedConnection);
}

void SceneEvents::currentScene(bool completed)
{
  if (activeSource_ && !completed) {
    auto *program = obs_get_output_source(0);
    auto *target = program && reinterpret_cast<quintptr>(program) == activeSource_
      ? obs_transition_get_source(program, OBS_TRANSITION_SOURCE_B) : nullptr;
    const bool running = target != nullptr;
    obs_source_release(target);
    obs_source_release(program);
    if (running) {
      publishEvent("progress", scene_, QDateTime::currentMSecsSinceEpoch());
      return; // Keeps long/manual transitions alive; not a visual-change poll.
    }
  }
  auto *scene = obs_frontend_get_current_scene();
  const QString name = scene ? QString::fromUtf8(obs_source_get_name(scene)) : QString();
  obs_source_release(scene);
  if (name.isEmpty()) return;
  scene_ = name;
  const bool ending = activeSource_ != 0;
  activeSource_ = 0;
  publishEvent(ending ? "end" : "state", name, QDateTime::currentMSecsSinceEpoch());
}

void SceneEvents::publishEvent(const char *phase, const QString &scene, qint64 at)
{
  const QJsonObject event{{"version", 1}, {"sessionId", session_},
    {"sequence", static_cast<double>(++sequence_)}, {"transitionId", static_cast<double>(transition_)},
    {"phase", phase}, {"scene", scene}, {"at", static_cast<double>(at)}};
  const QJsonObject request{{"vendorName", "obs-browser"}, {"requestType", "emit_event"},
    {"requestData", QJsonObject{{"event_name", "roadie:obs-transition"}, {"event_data", event}}}};
  const auto bytes = QJsonDocument(request).toJson(QJsonDocument::Compact);
  obs_data_t *data = obs_data_create_from_json(bytes.constData());
  auto *response = obs_websocket_call_request("CallVendorRequest", data);
  const bool ok = response && response->status_code == 100;
  if (!ok && !warned_) {
    blog(LOG_WARNING, "[roadie-signal] local Stage event delivery unavailable; obs-browser and obs-websocket required");
    warned_ = true;
  }
  if (ok) warned_ = false;
  obs_websocket_request_response_free(response);
  obs_data_release(data);
}
