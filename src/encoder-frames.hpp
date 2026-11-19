#pragma once
// Encoder frame counters for the stream output, plain C++ so tests/stream-health checks it without
// libobs.
#include <cstddef>
#include <cstdint>

struct EncoderFrames {
  std::uint64_t encoded = 0;
  std::uint64_t skipped = 0;
};

// main: OBS's main video mix. videos: the video each of the stream output's video encoders takes its
// frames from (null for an empty slot). read(video) returns that video's EncoderFrames.
//
// L10: a single-track stream's encoder reads the main mix, so this is the main mix's count, as
// before. A multitrack stream (Enhanced Broadcasting) gives each track its own GPU-scaled mix and
// leaves the main mix at 0, so every distinct mix the encoders read is added, each once.
template <typename Video, typename Read>
EncoderFrames sumEncoderFrames(Video main, const Video *videos, std::size_t count, Read read)
{
  EncoderFrames total;
  bool any = false;
  for (std::size_t i = 0; i < count; ++i) {
    if (!videos[i])
      continue;
    bool seen = false;
    for (std::size_t j = 0; j < i && !seen; ++j)
      seen = videos[j] == videos[i];
    if (seen)
      continue;
    const EncoderFrames frames = read(videos[i]);
    total.encoded += frames.encoded;
    total.skipped += frames.skipped;
    any = true;
  }
  if (!any && main)
    return read(main);
  return total;
}
