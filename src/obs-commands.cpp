#include "obs-commands.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QHash>
#include <QStringList>
#include <QVector>

namespace {
constexpr int maxName = 256;
constexpr int maxText = 4000;
constexpr int maxSources = 400;
constexpr int maxScenesPerSource = 200;
constexpr int maxFilters = 400;

bool validName(const QString &name) { return !name.isEmpty() && name.size() <= maxName; }

QJsonObject failed(const char *code) { return QJsonObject{{"ok", false}, {"error", QString::fromLatin1(code)}}; }

QJsonObject done() { return QJsonObject{{"ok", true}}; }

// A scene by name, searched only in OBS's own scene list. The caller releases it.
obs_source_t *sceneByName(const QString &wanted)
{
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

QJsonObject setSceneItemVisible(const QJsonObject &command)
{
  const QString sceneName = command.value("scene").toString();
  const QString sourceName = command.value("source").toString();
  const QJsonValue visibleValue = command.value("visible");
  if (!validName(sceneName) || !validName(sourceName) || !visibleValue.isBool())
    return failed("bad-command");
  obs_source_t *sceneSource = sceneByName(sceneName);
  if (!sceneSource)
    return failed("no-such-scene");
  QJsonObject result = failed("no-such-source");
  obs_scene_t *scene = obs_scene_from_source(sceneSource);
  // Recursive, so a source inside a group in that scene is found too.
  obs_sceneitem_t *item = scene ? obs_scene_find_source_recursive(scene, sourceName.toUtf8().constData()) : nullptr;
  if (item) {
    const bool visible = visibleValue.toBool();
    obs_sceneitem_set_visible(item, visible);
    result = obs_sceneitem_visible(item) == visible ? done() : failed("not-applied");
  }
  obs_source_release(sceneSource);
  return result;
}

QJsonObject setText(const QJsonObject &command)
{
  const QString sourceName = command.value("source").toString();
  const QJsonValue textValue = command.value("text");
  if (!validName(sourceName) || !textValue.isString() || textValue.toString().size() > maxText)
    return failed("bad-command");
  obs_source_t *source = obs_get_source_by_name(sourceName.toUtf8().constData());
  if (!source)
    return failed("no-such-source");
  QJsonObject result;
  const QString kind = QString::fromUtf8(obs_source_get_unversioned_id(source));
  if (kind != QLatin1String("text_gdiplus") && kind != QLatin1String("text_ft2_source")) {
    result = failed("not-a-text-source");
  } else {
    obs_data_t *current = obs_source_get_settings(source);
    // A text source reading from a file shows the file, so text set here would never appear.
    const bool fromFile = obs_data_get_bool(current, "read_from_file") || obs_data_get_bool(current, "from_file");
    obs_data_release(current);
    if (fromFile) {
      result = failed("text-from-file");
    } else {
      const QByteArray text = textValue.toString().toUtf8();
      obs_data_t *change = obs_data_create();
      obs_data_set_string(change, "text", text.constData());
      obs_source_update(source, change);
      obs_data_release(change);
      obs_data_t *after = obs_source_get_settings(source);
      result = QByteArray(obs_data_get_string(after, "text")) == text ? done() : failed("not-applied");
      obs_data_release(after);
    }
  }
  obs_source_release(source);
  return result;
}

QJsonObject setMuted(const QJsonObject &command)
{
  const QString inputName = command.value("input").toString();
  const QJsonValue mutedValue = command.value("muted");
  if (!validName(inputName) || !mutedValue.isBool())
    return failed("bad-command");
  obs_source_t *source = obs_get_source_by_name(inputName.toUtf8().constData());
  if (!source)
    return failed("no-such-source");
  QJsonObject result;
  if (!(obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO)) {
    result = failed("not-an-audio-input");
  } else {
    const bool muted = mutedValue.toBool();
    obs_source_set_muted(source, muted);
    result = obs_source_muted(source) == muted ? done() : failed("not-applied");
  }
  obs_source_release(source);
  return result;
}

QJsonObject setFilterEnabled(const QJsonObject &command)
{
  const QString sourceName = command.value("source").toString();
  const QString filterName = command.value("filter").toString();
  const QJsonValue enabledValue = command.value("enabled");
  if (!validName(sourceName) || !validName(filterName) || !enabledValue.isBool())
    return failed("bad-command");
  // Scenes are public sources too, so a filter on a scene is found the same way.
  obs_source_t *source = obs_get_source_by_name(sourceName.toUtf8().constData());
  if (!source)
    return failed("no-such-source");
  QJsonObject result = failed("no-such-filter");
  obs_source_t *filter = obs_source_get_filter_by_name(source, filterName.toUtf8().constData());
  if (filter) {
    const bool enabled = enabledValue.toBool();
    obs_source_set_enabled(filter, enabled);
    result = obs_source_enabled(filter) == enabled ? done() : failed("not-applied");
    obs_source_release(filter);
  }
  obs_source_release(source);
  return result;
}

QJsonObject setScene(const QJsonObject &command)
{
  const QString wanted = command.value("scene").toString();
  if (!validName(wanted))
    return failed("no-such-scene");
  obs_source_t *scene = sceneByName(wanted);
  if (!scene)
    return failed("no-such-scene");
  obs_frontend_set_current_scene(scene);
  obs_source_release(scene);
  // The switch itself lands on OBS's next frame; Roadie confirms it from the scene OBS reports.
  return done();
}

// Collected while walking the scenes, in the order OBS lists them.
struct Inventory {
  QVector<QString> order;
  QHash<QString, QString> kinds;
  QHash<QString, QStringList> scenes;
};

void note(Inventory &inv, obs_source_t *source, const QString &scene)
{
  const QString name = QString::fromUtf8(obs_source_get_name(source));
  if (!validName(name))
    return;
  if (!inv.kinds.contains(name)) {
    if (inv.order.size() >= maxSources)
      return;
    inv.order.append(name);
    const bool scenelike = obs_source_get_type(source) == OBS_SOURCE_TYPE_SCENE;
    inv.kinds.insert(name, scenelike ? QString() : QString::fromUtf8(obs_source_get_id(source)));
    inv.scenes.insert(name, QStringList());
  }
  QStringList &in = inv.scenes[name];
  if (!scene.isEmpty() && !in.contains(scene) && in.size() < maxScenesPerSource)
    in.append(scene);
}

struct Walk {
  Inventory *inv;
  QString scene;
};

bool walkItem(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
  auto *walk = static_cast<Walk *>(param);
  note(*walk->inv, obs_sceneitem_get_source(item), walk->scene);
  // A group's members belong to the scene the group is in, as the visibility command finds them.
  if (obs_sceneitem_is_group(item))
    obs_sceneitem_group_enum_items(item, walkItem, param);
  return true;
}
} // namespace

QJsonArray obsCommandCapabilities()
{
  return QJsonArray{QStringLiteral("set-scene"), QStringLiteral("set-source-visibility"),
                    QStringLiteral("set-text"), QStringLiteral("set-mute"), QStringLiteral("set-filter")};
}

QJsonObject runObsCommand(const QJsonObject &command)
{
  const QString action = command.value("action").toString();
  if (action == QLatin1String("set-scene"))
    return setScene(command);
  if (action == QLatin1String("set-source-visibility"))
    return setSceneItemVisible(command);
  if (action == QLatin1String("set-text"))
    return setText(command);
  if (action == QLatin1String("set-mute"))
    return setMuted(command);
  if (action == QLatin1String("set-filter"))
    return setFilterEnabled(command);
  return failed("unknown-action");
}

QJsonObject obsInventory()
{
  Inventory inv;
  obs_frontend_source_list scenes = {};
  obs_frontend_get_scenes(&scenes);
  for (size_t i = 0; i < scenes.sources.num; ++i) {
    obs_source_t *sceneSource = scenes.sources.array[i];
    Walk walk{&inv, QString::fromUtf8(obs_source_get_name(sceneSource))};
    if (obs_scene_t *scene = obs_scene_from_source(sceneSource))
      obs_scene_enum_items(scene, walkItem, &walk);
  }
  // Inputs in no scene still have audio and filters Roadie can reach.
  obs_enum_sources(
      [](void *param, obs_source_t *source) {
        note(*static_cast<Inventory *>(param), source, QString());
        return true;
      },
      &inv);

  QJsonArray sources;
  QJsonArray filters;
  const auto filtersOf = [&filters](obs_source_t *source) {
    obs_source_enum_filters(
        source,
        [](obs_source_t *parent, obs_source_t *child, void *param) {
          auto *list = static_cast<QJsonArray *>(param);
          if (list->size() >= maxFilters)
            return;
          const QString source = QString::fromUtf8(obs_source_get_name(parent));
          const QString filter = QString::fromUtf8(obs_source_get_name(child));
          if (validName(source) && validName(filter))
            list->append(QJsonObject{{"source", source}, {"filter", filter}});
        },
        &filters);
  };
  // Each scene's own filters, then every source's: a scene nested in another is read once.
  QStringList filtered;
  for (size_t i = 0; i < scenes.sources.num; ++i) {
    filtersOf(scenes.sources.array[i]);
    filtered.append(QString::fromUtf8(obs_source_get_name(scenes.sources.array[i])));
  }
  for (const QString &name : inv.order) {
    sources.append(QJsonObject{{"name", name},
                               {"kind", inv.kinds.value(name)},
                               {"scenes", QJsonArray::fromStringList(inv.scenes.value(name))}});
    if (filtered.contains(name))
      continue;
    obs_source_t *source = obs_get_source_by_name(name.toUtf8().constData());
    if (source) {
      filtersOf(source);
      obs_source_release(source);
    }
  }
  obs_frontend_source_list_free(&scenes);
  return QJsonObject{{"sources", sources}, {"filters", filters}};
}
