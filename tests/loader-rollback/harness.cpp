// Loads the real loader the way OBS does (obs_open_module, obs_init_module) on a headless libobs,
// then either closes cleanly (obs_shutdown, which unloads it) or stops dead, as a crash would.
// Usage: harness <config dir> <loader dll> <loader data dir> clean|crash
#include <obs.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char **argv)
{
  if (argc != 5)
    return 2;
  if (!obs_startup("en-US", argv[1], nullptr))
    return 3;
  obs_module_t *module = nullptr;
  if (obs_open_module(&module, argv[2], argv[3]) != MODULE_SUCCESS)
    return 4;
  const bool loaded = obs_init_module(module);
  std::printf("loader init: %s\n", loaded ? "ok" : "failed");
  std::fflush(stdout);
  if (std::strcmp(argv[4], "crash") == 0)
    std::_Exit(9);
  obs_shutdown();
  return loaded ? 0 : 5;
}
