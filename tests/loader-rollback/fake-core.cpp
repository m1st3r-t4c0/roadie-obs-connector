// A stand-in core for the loader test. It has a core's entry points and nothing else. When this
// DLL's own path contains FAKE_CORE_CRASH it ends the process in obs_module_load, as a core that
// crashes OBS on start would.
#include <obs-module.h>
#include <cstdlib>
#include <string>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

OBS_DECLARE_MODULE()

static std::string ownPath()
{
  HMODULE self = nullptr;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCSTR>(&ownPath), &self);
  char path[MAX_PATH] = {};
  GetModuleFileNameA(self, path, MAX_PATH);
  return path;
}

bool obs_module_load()
{
  const char *crash = std::getenv("FAKE_CORE_CRASH");
  const std::string path = ownPath();
  if (crash && *crash && path.find(crash) != std::string::npos)
    std::_Exit(7);
  std::printf("core loaded: %s\n", path.c_str());
  std::fflush(stdout);
  return true;
}

void obs_module_unload() {}
