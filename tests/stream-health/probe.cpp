// L10: a multitrack stream (Enhanced Broadcasting) feeds each track's encoder from its own video mix,
// so the main mix's counters stay 0 while four tracks stream. Plain C++: fake video handles and
// counters stand in for libobs' video_t and video_output_get_total/skipped_frames.
#include "../../src/encoder-frames.hpp"
#include <cstdio>
#include <cstdlib>
#include <map>

struct FakeVideo {
  std::uint64_t encoded;
  std::uint64_t skipped;
};

static int failures = 0;
static void expect(const char *name, EncoderFrames got, std::uint64_t encoded, std::uint64_t skipped)
{
  const bool ok = got.encoded == encoded && got.skipped == skipped;
  std::printf("%s %s: encoded=%llu skipped=%llu (want %llu/%llu)\n", ok ? "PASS" : "FAIL", name,
              static_cast<unsigned long long>(got.encoded), static_cast<unsigned long long>(got.skipped),
              static_cast<unsigned long long>(encoded), static_cast<unsigned long long>(skipped));
  if (!ok)
    ++failures;
}

int main()
{
  auto read = [](const FakeVideo *video) { return EncoderFrames{video->encoded, video->skipped}; };
  // The main mix: a single-track stream's encoder reads it directly.
  FakeVideo main{3600, 12};

  // Single track on the main mix: the same numbers as before.
  const FakeVideo *single[] = {&main, nullptr, nullptr};
  expect("single track", sumEncoderFrames<const FakeVideo *>(&main, single, 3, read), 3600, 12);

  // No encoder slot filled (never expected while streaming): fall back to the main mix.
  const FakeVideo *none[] = {nullptr, nullptr};
  expect("no encoders", sumEncoderFrames<const FakeVideo *>(&main, none, 2, read), 3600, 12);

  // Multitrack (roylroostr): four renditions, each on its own GPU-scaled mix; the main mix has no
  // encoder and counts nothing.
  FakeVideo idleMain{0, 0};
  FakeVideo t1080{3580, 40}, t720{3580, 0}, t480{3580, 5}, t360{3580, 0};
  const FakeVideo *multi[] = {&t1080, &t720, &t480, &t360, nullptr, nullptr};
  expect("multitrack", sumEncoderFrames<const FakeVideo *>(&idleMain, multi, 6, read), 4 * 3580, 45);

  // Two tracks on one mix are one set of counters, counted once.
  const FakeVideo *shared[] = {&main, &t720, &main};
  expect("shared mix", sumEncoderFrames<const FakeVideo *>(&idleMain, shared, 3, read), 3600 + 3580, 12);

  std::printf(failures ? "FAILED %d\n" : "ALL PASSED\n", failures);
  return failures ? 1 : 0;
}
