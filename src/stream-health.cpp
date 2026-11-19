#include "stream-health.hpp"
#include "encoder-frames.hpp"
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
  // Encoding lag: frames the encoders skipped (encoder overloaded), read from the video each of the
  // stream's encoders takes its frames from. L10: with multitrack, that is not the main mix.
  video_t *videos[MAX_OUTPUT_VIDEO_ENCODERS] = {};
  for (size_t i = 0; i < MAX_OUTPUT_VIDEO_ENCODERS; ++i) {
    obs_encoder_t *encoder = obs_output_get_video_encoder2(output, i);
    videos[i] = encoder ? obs_encoder_video(encoder) : nullptr;
  }
  obs_output_release(output);
  const EncoderFrames encoder =
      sumEncoderFrames<video_t *>(obs_get_video(), videos, MAX_OUTPUT_VIDEO_ENCODERS, [](video_t *video) {
        return EncoderFrames{video_output_get_total_frames(video), video_output_get_skipped_frames(video)};
      });

  // Rendering lag: frames OBS could not compose in time (GPU-bound).
  sample["renderFrames"] = static_cast<double>(obs_get_total_frames());
  sample["laggedFrames"] = static_cast<double>(obs_get_lagged_frames());
  sample["encodedFrames"] = static_cast<double>(encoder.encoded);
  sample["skippedFrames"] = static_cast<double>(encoder.skipped);
  return sample;
}
