#pragma once
#include <obs.h>
#include <obs-audio-controls.h>
#include <QJsonObject>
#include <QString>
#include <chrono>
#include <mutex>

class AudioTap {
public:
  ~AudioTap();
  void select(const QString &uuid);
  void detach();
  QJsonObject reading();

private:
  static void capture(void *param, const float *magnitude, const float *peak, const float *inputPeak);
  obs_source_t *source_ = nullptr;
  obs_volmeter_t *meter_ = nullptr;
  QString uuid_;
  QString lastName_;
  size_t channels_ = 0;
  std::mutex mutex_;
  float peak_ = 0;
  float rms_ = 0;
  bool sampled_ = false;
  std::chrono::steady_clock::time_point sampledAt_;
};
