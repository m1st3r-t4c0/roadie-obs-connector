#include "obs-layout.hpp"
#include "../third_party/obs-websocket-api.h"
#include <obs-frontend-api.h>
#include <obs.h>
#include <graphics/matrix4.h>
#include <graphics/vec3.h>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>
#include <QUuid>
#include <vector>
#include <algorithm>
#include <cmath>

namespace {
constexpr int maxName = 256;
// A group inside a scene inside a scene is as deep as a real collection goes; this only stops a
// pathological one from walking forever.
constexpr int maxDepth = 6;

bool validName(const QString &name) { return !name.isEmpty() && name.size() <= maxName; }

double tenth(double v) { return std::round(v * 10.0) / 10.0; }

// The Roadie Stage browser source: the page that draws Stage sources, so the Stage page can turn
// canvas pixels into its own (spec A4). Only the fact is reported, never the URL, which carries the
// Stage key.
bool isRoadieStage(obs_source_t *source)
{
  if (QString::fromUtf8(obs_source_get_unversioned_id(source)) != QLatin1String("browser_source"))
    return false;
  obs_data_t *settings = obs_source_get_settings(source);
  const QUrl url(QString::fromUtf8(obs_data_get_string(settings, "url")));
  const bool local = obs_data_get_bool(settings, "is_local_file");
  obs_data_release(settings);
  if (local || !url.isValid())
    return false;
  const QString origin = url.scheme() + QStringLiteral("://") + url.authority();
  const bool ours = origin == QLatin1String(ROADIE_SIGNAL_ORIGIN) || origin == QLatin1String(ROADIE_SIGNAL_STAGING_ORIGIN);
  const QString path = url.path();
  return ours && (path == QLatin1String("/stage-output") || path == QLatin1String("/stage-trial-output"));
}

struct Walk {
  QJsonArray *items;
  int maxItems;
  bool *truncated;
  // Draw transforms of the groups and nested scenes above, outermost first.
  std::vector<matrix4> chain;
  bool visible;
  int depth;
  QString holder;
};

// A point in the innermost holder's own pixels, carried out to the canvas.
vec3 toCanvas(const Walk &walk, vec3 point)
{
  for (size_t i = walk.chain.size(); i-- > 0;) {
    vec3 out;
    vec3_transform(&out, &point, &walk.chain[i]);
    point = out;
  }
  return point;
}

bool walkItem(obs_scene_t *, obs_sceneitem_t *item, void *param);

void walkScene(obs_scene_t *scene, Walk &walk)
{
  if (scene)
    obs_scene_enum_items(scene, walkItem, &walk);
}

bool walkItem(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
  auto *walk = static_cast<Walk *>(param);
  if (walk->items->size() >= walk->maxItems) {
    *walk->truncated = true;
    return false;
  }
  obs_source_t *source = obs_sceneitem_get_source(item);
  if (!source || !(obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO))
    return true;
  const QString name = QString::fromUtf8(obs_source_get_name(source));
  const bool visible = walk->visible && obs_sceneitem_visible(item);

  // The box transform maps the unit square onto the item's box in its holder's pixels, with
  // position, scale, rotation, bounds, alignment and crop already folded in.
  matrix4 box;
  obs_sceneitem_get_box_transform(item, &box);
  double left = 0, top = 0, right = 0, bottom = 0;
  for (int c = 0; c < 4; ++c) {
    vec3 corner;
    vec3_set(&corner, static_cast<float>(c & 1), static_cast<float>((c >> 1) & 1), 0.0f);
    vec3 inHolder;
    vec3_transform(&inHolder, &corner, &box);
    const vec3 p = toCanvas(*walk, inHolder);
    if (c == 0) {
      left = right = p.x;
      top = bottom = p.y;
    } else {
      left = std::min<double>(left, p.x);
      right = std::max<double>(right, p.x);
      top = std::min<double>(top, p.y);
      bottom = std::max<double>(bottom, p.y);
    }
  }
  const double width = right - left;
  const double height = bottom - top;

  const bool group = obs_sceneitem_is_group(item);
  const QString kind = group ? QStringLiteral("group") : QString::fromUtf8(obs_source_get_unversioned_id(source));
  if (validName(name) && width > 0.5 && height > 0.5) {
    QJsonObject entry{{"id", static_cast<qint64>(obs_sceneitem_get_id(item))},
                      {"name", name},
                      {"kind", kind},
                      {"in", walk->holder},
                      {"x", tenth(left)},
                      {"y", tenth(top)},
                      {"w", tenth(width)},
                      {"h", tenth(height)},
                      {"visible", visible}};
    // A rotated item is reported by its upright bounding box (spec A10); say so.
    if (std::fmod(std::fabs(obs_sceneitem_get_rot(item)), 360.0f) > 0.01f)
      entry["rotated"] = true;
    if (isRoadieStage(source))
      entry["stage"] = true;
    walk->items->append(entry);
  }

  // A group's members, or a nested scene's items, sit in that holder's own pixels; its draw
  // transform carries them into the holder's parent.
  obs_scene_t *inner = group ? obs_sceneitem_group_get_scene(item)
                             : (obs_source_get_type(source) == OBS_SOURCE_TYPE_SCENE ? obs_scene_from_source(source)
                                                                                      : nullptr);
  if (inner && walk->depth < maxDepth) {
    Walk nested{walk->items, walk->maxItems, walk->truncated, walk->chain, visible, walk->depth + 1, name};
    matrix4 draw;
    obs_sceneitem_get_draw_transform(item, &draw);
    nested.chain.push_back(draw);
    walkScene(inner, nested);
    if (walk->items->size() >= walk->maxItems)
      return false;
  }
  return true;
}

obs_source_t *sceneNamed(const QString &wanted)
{
  if (wanted.isEmpty())
    return obs_frontend_get_current_scene();
  obs_source_t *found = nullptr;
  obs_frontend_source_list scenes = {};
  obs_frontend_get_scenes(&scenes);
  for (size_t i = 0; i < scenes.sources.num; ++i) {
    obs_source_t *scene = scenes.sources.array[i];
    if (QString::fromUtf8(obs_source_get_name(scene)) == wanted) {
      found = obs_source_get_ref(scene);
      break;
    }
  }
  obs_frontend_source_list_free(&scenes);
  return found;
}

QJsonObject layoutOf(obs_source_t *sceneSource, int maxItems)
{
  obs_video_info video = {};
  obs_get_video_info(&video);
  QJsonArray items;
  bool truncated = false;
  Walk walk{&items, maxItems, &truncated, {}, true, 0, QString::fromUtf8(obs_source_get_name(sceneSource))};
  walkScene(obs_scene_from_source(sceneSource), walk);
  QJsonObject layout{{"scene", walk.holder},
                     {"canvas", QJsonObject{{"width", static_cast<int>(video.base_width)},
                                            {"height", static_cast<int>(video.base_height)}}},
                     {"items", items}};
  if (truncated)
    layout["truncated"] = true;
  return layout;
}
} // namespace

QJsonObject obsSceneLayout(const QString &sceneName, int maxItems)
{
  if (!sceneName.isEmpty() && !validName(sceneName))
    return QJsonObject{{"ok", false}, {"error", QStringLiteral("bad-command")}};
  obs_source_t *scene = sceneNamed(sceneName);
  if (!scene)
    return QJsonObject{{"ok", false}, {"error", QStringLiteral("no-such-scene")}};
  const QJsonObject layout = layoutOf(scene, maxItems);
  obs_source_release(scene);
  return QJsonObject{{"ok", true}, {"layout", layout}};
}

QByteArray programLayoutJson(int maxItems)
{
  obs_source_t *scene = obs_frontend_get_current_scene();
  if (!scene)
    return QByteArray();
  const QJsonObject layout = layoutOf(scene, maxItems);
  obs_source_release(scene);
  return QJsonDocument(layout).toJson(QJsonDocument::Compact);
}

// Ten reads a second of a few dozen scene items is a walk of OBS's own lists, no rendering. Only a
// change is sent; a repeat every five seconds lets a Stage page that loaded late catch up.
LayoutEvents::LayoutEvents() : session_(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
  timer_.setInterval(100);
  connect(&timer_, &QTimer::timeout, this, [this] { tick(false); });
  timer_.start();
}

LayoutEvents::~LayoutEvents() { timer_.stop(); }

void LayoutEvents::poke() { tick(false); }

void LayoutEvents::tick(bool force)
{
  const QByteArray json = programLayoutJson(64);
  if (json.isEmpty())
    return;
  if (json == last_ && !force && ++sinceSent_ < 50)
    return;
  last_ = json;
  sinceSent_ = 0;
  const QJsonObject event{{"version", 1},
                          {"sessionId", session_},
                          {"sequence", static_cast<double>(++sequence_)},
                          {"at", static_cast<double>(QDateTime::currentMSecsSinceEpoch())},
                          {"layout", QJsonDocument::fromJson(json).object()}};
  const QJsonObject request{{"vendorName", "obs-browser"},
                            {"requestType", "emit_event"},
                            {"requestData", QJsonObject{{"event_name", "roadie:obs-layout"}, {"event_data", event}}}};
  const auto bytes = QJsonDocument(request).toJson(QJsonDocument::Compact);
  obs_data_t *data = obs_data_create_from_json(bytes.constData());
  auto *response = obs_websocket_call_request("CallVendorRequest", data);
  const bool ok = response && response->status_code == 100;
  if (!ok && !warned_) {
    blog(LOG_WARNING, "[roadie-signal] local Stage layout delivery unavailable; obs-browser and obs-websocket required");
    warned_ = true;
  }
  if (ok)
    warned_ = false;
  obs_websocket_request_response_free(response);
  obs_data_release(data);
}
