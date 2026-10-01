#include "../../src/scene-events.hpp"
#include "../../third_party/obs-websocket-api.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVector>
#include <cstdio>

// Real libobs sources/signals and Qt dispatch; a recording API endpoint instead
// of CEF. This checks ordering/filtering/lifetime, not host rendering latency.
static QVector<QJsonObject> frames;
static proc_handler_t *api;
static void getApi(void *, calldata_t *cd) { calldata_set_ptr(cd, "ph", api); }
static void call(void *, calldata_t *cd)
{
  const auto request = QJsonDocument::fromJson(calldata_string(cd, "request_data")).object();
  if (request["vendorName"] != "obs-browser" || request["requestType"] != "emit_event") std::abort();
  frames.push_back(request["requestData"].toObject()["event_data"].toObject());
  auto *response = static_cast<obs_websocket_request_response *>(bzalloc(sizeof(obs_websocket_request_response)));
  response->status_code = 100;
  calldata_set_ptr(cd, "response", response);
}
static void signal(obs_source_t *source, const char *name)
{
  calldata_t cd = {};
  calldata_set_ptr(&cd, "source", source);
  signal_handler_signal(obs_source_get_signal_handler(source), name, &cd);
  calldata_free(&cd);
  QCoreApplication::processEvents();
}
static void require(bool ok) { if (!ok) { std::puts("FAIL"); std::exit(1); } }
int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);
  require(obs_startup("en-US", nullptr, nullptr));
  api = proc_handler_create();
  proc_handler_add(obs_get_proc_handler(), "void obs_websocket_api_get_ph(out ptr ph)", getApi, nullptr);
  proc_handler_add(api, "void call_request(string request_type, string request_data, out ptr response)", call, nullptr);
  obs_source_info info = {};
  info.id = "roadie_probe_transition"; info.type = OBS_SOURCE_TYPE_TRANSITION;
  info.get_name = [](void *) { return "Probe"; };
  info.create = [](obs_data_t *, obs_source_t *source) -> void * { return source; };
  info.destroy = [](void *) {};
  info.video_render = [](void *, gs_effect_t *) {};
  info.audio_render = [](void *, uint64_t *, obs_source_audio_mix *, uint32_t, size_t, size_t) { return false; };
  obs_register_source(&info);
  obs_source_info input = {};
  input.id = "roadie_probe_input"; input.type = OBS_SOURCE_TYPE_INPUT;
  input.output_flags = OBS_SOURCE_VIDEO;
  input.get_name = info.get_name; input.create = info.create; input.destroy = info.destroy;
  input.get_width = [](void *) -> uint32_t { return 1920; };
  input.get_height = [](void *) -> uint32_t { return 1080; };
  input.video_render = info.video_render;
  obs_register_source(&input);
  auto *game = obs_source_create_private(input.id, "Gameplay", nullptr);
  auto *brb = obs_source_create_private(input.id, "BRB", nullptr);
  auto *program = obs_source_create_private(info.id, "program", nullptr);
  auto *preview = obs_source_create_private(info.id, "preview", nullptr);
  require(program && preview && obs_source_get_type(program) == OBS_SOURCE_TYPE_TRANSITION);
  obs_set_output_source(0, program);
  {
    SceneEvents events;
    signal(preview, "transition_start");
    require(frames.empty());
    obs_transition_set(program, game);
    require(obs_transition_start(program, OBS_TRANSITION_MODE_AUTO, 300, brb));
    QCoreApplication::processEvents();
    obs_transition_set(program, brb);
    signal(program, "transition_video_stop");
    require(frames.size() == 2);
    require(frames[0]["scene"] == "BRB" && frames[1]["scene"] == "BRB");
    require(frames[0]["phase"] == "start" && frames[1]["phase"] == "end");
    require(frames[0]["transitionId"] == frames[1]["transitionId"]);
    require(frames[0]["sequence"].toInt() < frames[1]["sequence"].toInt());
    // An end from a different output source cannot finish this transition.
    signal(program, "transition_start");
    obs_set_output_source(0, preview);
    signal(preview, "transition_video_stop");
    require(frames.size() == 3);
    // Channel changes attach synchronously, before the new transition starts.
    obs_transition_set(preview, brb);
    require(obs_transition_start(preview, OBS_TRANSITION_MODE_AUTO, 300, game));
    QCoreApplication::processEvents();
    obs_transition_set(preview, game);
    signal(preview, "transition_video_stop");
    require(frames.size() == 5);
    require(frames[3]["scene"] == "Gameplay" && frames[4]["scene"] == "Gameplay");
    require(frames[3]["phase"] == "start" && frames[4]["phase"] == "end");
    require(frames[3]["transitionId"] == frames[4]["transitionId"]);
    signal(program, "transition_start");
    require(frames.size() == 5);
  }
  signal(preview, "transition_start");
  require(frames.size() == 5);
  obs_set_output_source(0, nullptr);
  obs_source_release(program); obs_source_release(preview);
  obs_source_release(game); obs_source_release(brb);
  obs_shutdown(); proc_handler_destroy(api);
  std::puts("PASS native transition filtering, sequencing and shutdown");
}
