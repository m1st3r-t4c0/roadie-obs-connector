# Stream health encoder-frame probe

Checks how `src/encoder-frames.hpp` adds up the stream's encoder frame counters: a single-track
stream reads the main video mix as before, and a multitrack stream (Enhanced Broadcasting), whose
encoders each read their own mix, adds every distinct mix once. Plain C++, no OBS needed.

```powershell
cmake -S tests/stream-health -B build/stream-health-probe
cmake --build build/stream-health-probe --config Release
build/stream-health-probe/Release/stream-health-probe.exe
```

Exit code 0 when every case passes. It does not start libobs, so it does not prove which video
mix OBS gives each encoder; that comes from libobs' `obs_encoder_video` (OBS 30.0 and newer).
