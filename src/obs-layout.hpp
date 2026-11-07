#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>

// 1.1.0 (roadie docs/specs/stage-attach-to-obs-source.md §4.1): where every source in a scene sits
// on OBS's canvas.
//
// Each item is reported by its upright bounding box in canvas pixels, with OBS's own box transform
// (position, scale, rotation, bounds, alignment and crop) folded in. Groups and nested scenes are
// walked, with each parent's transform applied. Items whose eye is off are reported too, marked
// `visible: false` (t4c0, 2026-11-07, D3), because a source that only shows during an effect still
// has a place on the canvas.
//
// `sceneName` empty means the scene on air (the program scene). Any other name is looked up in
// OBS's own scene list, so any scene can be read at any time (t4c0, 2026-11-07, D1).
// Returns {ok:false,error} for an unknown scene, else {ok:true, layout:{...}}.
QJsonObject obsSceneLayout(const QString &sceneName, int maxItems);

// The program scene's layout as compact JSON, for change detection. Empty when OBS has no scene.
QByteArray programLayoutJson(int maxItems);

// The live layout of the scene on air, delivered to Roadie's Stage pages in this same OBS through
// obs-browser (the path SceneEvents uses for transitions), only when it changes. This is the fast
// path: a camera moved or resized, or moved by a filter, reaches the Stage page in this OBS within
// a tick, with no server round trip.
class LayoutEvents final : public QObject {
public:
  LayoutEvents();
  ~LayoutEvents() override;
  // A scene switch: read now instead of waiting for the next tick.
  void poke();

private:
  void tick(bool force);
  QTimer timer_;
  QByteArray last_;
  QString session_;
  quint64 sequence_ = 0;
  int sinceSent_ = 0;
  bool warned_ = false;
};
