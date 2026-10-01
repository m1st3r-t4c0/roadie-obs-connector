#pragma once
#include <QObject>
#include <QTimer>
#include <QString>
#include <obs.h>
#include <mutex>
#include <atomic>

// OBS callbacks only capture metadata and enqueue work. CEF runs on the Qt thread.
class SceneEvents final : public QObject {
public:
  SceneEvents();
  ~SceneEvents() override;
  void currentScene(bool completed = false);
private:
  static void channelChanged(void *, calldata_t *);
  void observe(obs_source_t *);
  std::mutex sourceMutex_;
  obs_source_t *observed_ = nullptr;
  std::atomic<obs_source_t *> observedIdentity_{nullptr};
  static void started(void *, calldata_t *);
  static void ended(void *, calldata_t *);
  void capture(calldata_t *, bool start);
  void publishEvent(const char *phase, const QString &scene, qint64 at);
  QTimer heartbeat_;
  QString session_;
  QString scene_;
  quint64 sequence_ = 0;
  quint64 transition_ = 0;
  quintptr activeSource_ = 0;
  bool warned_ = false;
};
