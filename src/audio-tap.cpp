#include "audio-tap.hpp"
#include <media-io/audio-io.h>
#include <algorithm>
#include <cmath>

AudioTap::~AudioTap() { detach(); }

void AudioTap::detach()
{
  // Remove the callback and meter before releasing the source or callback state.
  if (meter_) {
    obs_volmeter_remove_callback(meter_, capture, this);
    obs_volmeter_detach_source(meter_);
    obs_volmeter_destroy(meter_);
    meter_ = nullptr;
  }
  if (source_) {
    obs_source_release(source_);
    source_ = nullptr;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  sampled_ = false;
  peak_ = rms_ = 0;
}

void AudioTap::select(const QString &uuid)
{
  detach();
  if (uuid_ != uuid)
    lastName_.clear();
  uuid_ = uuid;
  if (uuid.isEmpty())
    return;
  source_ = obs_get_source_by_uuid(uuid.toUtf8().constData());
  if (!source_)
    return;
  if (!(obs_source_get_output_flags(source_) & OBS_SOURCE_AUDIO) || obs_source_removed(source_)) {
    obs_source_release(source_);
    source_ = nullptr;
    return;
  }
  obs_audio_info info{};
  if (!obs_get_audio_info(&info)) {
    obs_source_release(source_);
    source_ = nullptr;
    return;
  }
  channels_ = get_audio_channels(info.speakers);
  lastName_ = QString::fromUtf8(obs_source_get_name(source_));
  meter_ = obs_volmeter_create(OBS_FADER_LOG);
  if (!meter_ || !obs_volmeter_attach_source(meter_, source_)) {
    detach();
    return;
  }
  obs_volmeter_add_callback(meter_, capture, this);
}

static float amplitude(float db)
{
  if (db == -INFINITY)
    return 0; // real zero signal; not unavailable
  return std::clamp(std::pow(10.0f, db / 20.0f), 0.0f, 1.0f);
}

void AudioTap::capture(void *param, const float *magnitude, const float *peaks, const float *)
{
  auto &tap = *static_cast<AudioTap *>(param);
  float peak = 0;
  float rms = 0;
  bool valid = false;
  // Read the native meter's envelope, never PCM. No allocation, Qt, JavaScript,
  // or network work on the callback; source mute is read on the GUI timer.
  for (size_t channel = 0; channel < tap.channels_ && channel < MAX_AUDIO_CHANNELS; ++channel) {
    if (std::isnan(peaks[channel]) || std::isnan(magnitude[channel]) ||
        peaks[channel] == INFINITY || magnitude[channel] == INFINITY)
      continue;
    peak = std::max(peak, amplitude(peaks[channel]));
    rms = std::max(rms, amplitude(magnitude[channel]));
    valid = true;
  }
  if (!valid)
    return;
  std::unique_lock<std::mutex> lock(tap.mutex_, std::try_to_lock);
  if (!lock.owns_lock())
    return; // drop a measurement rather than block OBS's audio thread
  tap.peak_ = peak;
  tap.rms_ = rms;
  tap.sampledAt_ = std::chrono::steady_clock::now();
  tap.sampled_ = true;
}

QJsonObject AudioTap::reading()
{
  if (source_ && obs_source_removed(source_))
    detach();
  // A source can appear again after a scene collection change. Re-resolve its UUID;
  // do not silently select a differently named microphone.
  if (!source_ && !uuid_.isEmpty()) {
    const QString selected = uuid_;
    select(selected);
  }
  QJsonObject result{{"status", uuid_.isEmpty() ? "unselected" : "unavailable"},
                     {"source", QJsonValue::Null}, {"peak", QJsonValue::Null},
                     {"rms", QJsonValue::Null}, {"sampleAgeMs", QJsonValue::Null}};
  if (uuid_.isEmpty())
    return result;
  if (source_)
    lastName_ = QString::fromUtf8(obs_source_get_name(source_));
  result["source"] = QJsonObject{{"id", uuid_}, {"name", lastName_}};
  if (!source_)
    return result;
  if (obs_source_muted(source_)) {
    result["status"] = "muted";
    return result;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (!sampled_)
    return result;
  const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - sampledAt_).count();
  if (age > 1000)
    return result;
  result["status"] = "ready";
  result["peak"] = peak_;
  result["rms"] = rms_;
  result["sampleAgeMs"] = static_cast<double>(age);
  return result;
}
