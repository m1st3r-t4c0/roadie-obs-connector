#include "stream-health.hpp"
#include <obs.h>
#include <obs-frontend-api.h>
#include <util/platform.h>

QJsonObject sampleStreamHealth()
{
  QJsonObject sample;
  sample["sampledAt"] = static_cast<double>(os_gettime_ns() / 1000000);
  obs_output_t *output = obs_frontend_streaming_active() ? obs_frontend_get_streaming_output() : nullptr;
  if (!output) {
    sample["streaming"] = false;
    return sample;
  }
  // Every counter is cumulative since the output started. JSON numbers are
  // doubles; byte totals stay exact far beyond any single broadcast.
  sample["streaming"] = true;
  sample["outputFrames"] = static_cast<double>(obs_output_get_total_frames(output));
  sample["droppedFrames"] = static_cast<double>(obs_output_get_frames_dropped(output));
  sample["bytesSent"] = static_cast<double>(obs_output_get_total_bytes(output));
  const float congestion = obs_output_get_congestion(output);
  sample["congestion"] = congestion >= 0.0f && congestion <= 1.0f ? static_cast<double>(congestion) : 0.0;
  sample["reconnecting"] = obs_output_reconnecting(output);
  obs_output_release(output);

  // Rendering lag: frames OBS could not compose in time (GPU-bound).
  sample["renderFrames"] = static_cast<double>(obs_get_total_frames());
  sample["laggedFrames"] = static_cast<double>(obs_get_lagged_frames());
  // Encoding lag: frames the encoder skipped (encoder overloaded).
  video_t *video = obs_get_video();
  sample["encodedFrames"] = video ? static_cast<double>(video_output_get_total_frames(video)) : 0.0;
  sample["skippedFrames"] = video ? static_cast<double>(video_output_get_skipped_frames(video)) : 0.0;
  return sample;
}
