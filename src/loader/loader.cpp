// The Roadie Signal loader (roadie docs/specs/obs-plugin.md §24): the one file OBS loads. It picks a
// core (the plugin itself), loads it, and hands it this module, so the core's config folder, data
// folder and sign-in are the plugin's as they always were. Everything else is the core's.
//
// It also keeps the streamer safe from a bad update (§24.3): before a core starts it is written
// down as "starting"; a core that runs two minutes, or until OBS closes cleanly, becomes the last
// good one. A core whose starts never got there twice is marked bad and the last good one runs
// instead. A core that will not load at all falls back at once.
//
// Deliberately small, with no Qt and no network: a loader change is a reinstall, so it should
// almost never change. Only libobs (for paths and JSON) and the platform's library loader.

#include <obs-module.h>
#include <util/bmem.h>
#include <util/platform.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

OBS_DECLARE_MODULE()

MODULE_EXPORT const char *obs_module_description()
{
  return "Roadie Signal " ROADIE_SIGNAL_VERSION " (loader)";
}

namespace {

constexpr const char *kBundled = "bundled";
// A core that never reached "good" this many starts in a row is put aside.
constexpr long long kStrikes = 2;
constexpr std::chrono::minutes kHealthy{2};

#ifdef _WIN32
constexpr const char *kCoreFile = "roadie-signal-core.dll";
#else
constexpr const char *kCoreFile = "roadie-signal-core.dylib";
#endif

using SetPointerFn = void (*)(obs_module_t *);
using LoadFn = bool (*)();
using VoidFn = void (*)();
using LocaleFn = void (*)(const char *);

struct Core {
  void *handle = nullptr;
  std::string version;
  LoadFn load = nullptr;
  VoidFn postLoad = nullptr;
  VoidFn unload = nullptr;
  LocaleFn setLocale = nullptr;
  VoidFn freeLocale = nullptr;
};

Core core;
std::mutex stateLock;
std::mutex waitLock;
std::condition_variable wake;
bool stopping = false;
std::thread healthyTimer;

// Only digits and dots, three parts: a version never becomes a path that leaves cores/.
bool validVersion(const std::string &v)
{
  int dots = 0;
  bool digit = false;
  for (char c : v) {
    if (c == '.') {
      if (!digit)
        return false;
      ++dots;
      digit = false;
    } else if (c >= '0' && c <= '9') {
      digit = true;
    } else {
      return false;
    }
  }
  return dots == 2 && digit && v.size() <= 32;
}

std::string configPath(const char *file)
{
  char *path = obs_module_get_config_path(obs_current_module(), file);
  std::string out = path ? path : "";
  bfree(path);
  return out;
}

std::string corePath(const std::string &version)
{
  if (version == kBundled) {
    char *path = obs_find_module_file(obs_current_module(), (std::string("core/") + kCoreFile).c_str());
    std::string out = path ? path : "";
    bfree(path);
    return out;
  }
  return configPath(("cores/" + version + "/" + kCoreFile).c_str());
}

std::string str(obs_data_t *data, const char *key)
{
  const char *value = obs_data_get_string(data, key);
  return value ? value : "";
}

bool isBad(obs_data_t *state, const std::string &version)
{
  obs_data_array_t *bad = obs_data_get_array(state, "bad");
  bool found = false;
  for (size_t i = 0; bad && i < obs_data_array_count(bad) && !found; ++i) {
    obs_data_t *item = obs_data_array_item(bad, i);
    found = str(item, "version") == version;
    obs_data_release(item);
  }
  obs_data_array_release(bad);
  return found;
}

void markBad(obs_data_t *state, const std::string &version)
{
  if (version == kBundled || isBad(state, version))
    return;
  obs_data_array_t *bad = obs_data_get_array(state, "bad");
  if (!bad)
    bad = obs_data_array_create();
  obs_data_t *item = obs_data_create();
  obs_data_set_string(item, "version", version.c_str());
  obs_data_array_push_back(bad, item);
  obs_data_release(item);
  obs_data_set_array(state, "bad", bad);
  obs_data_array_release(bad);
}

void noteRollback(obs_data_t *state, const std::string &from, const std::string &to)
{
  obs_data_t *rolled = obs_data_create();
  obs_data_set_string(rolled, "from", from.c_str());
  obs_data_set_string(rolled, "to", to.c_str());
  obs_data_set_int(rolled, "at", static_cast<long long>(time(nullptr)));
  obs_data_set_obj(state, "rolledBack", rolled);
  obs_data_release(rolled);
  blog(LOG_WARNING, "[roadie-signal] core %s put aside; running %s", from.c_str(), to.c_str());
}

obs_data_t *readState()
{
  const std::string path = configPath("update.json");
  obs_data_t *state = os_file_exists(path.c_str()) ? obs_data_create_from_json_file(path.c_str()) : nullptr;
  return state ? state : obs_data_create();
}

void writeState(obs_data_t *state)
{
  const std::string folder = configPath("");
  os_mkdirs(folder.c_str());
  obs_data_save_json_safe(state, configPath("update.json").c_str(), "tmp", "bak");
}

void *openLibrary(const std::string &path)
{
  if (path.empty() || !os_file_exists(path.c_str()))
    return nullptr;
#ifdef _WIN32
  const int size = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
  std::wstring wide(size > 0 ? static_cast<size_t>(size) : 0, L'\0');
  if (size <= 0 || !MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wide.data(), size))
    return nullptr;
  // The core's own folder is searched for anything it needs; OBS's DLLs are already loaded.
  return LoadLibraryExW(wide.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
  return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void *symbol(void *handle, const char *name)
{
#ifdef _WIN32
  return reinterpret_cast<void *>(GetProcAddress(static_cast<HMODULE>(handle), name));
#else
  return dlsym(handle, name);
#endif
}

void closeLibrary(void *handle)
{
#ifdef _WIN32
  FreeLibrary(static_cast<HMODULE>(handle));
#else
  dlclose(handle);
#endif
}

// Opens one core and finds its entry points. False leaves nothing open.
bool open(const std::string &version, Core &out)
{
  void *handle = openLibrary(corePath(version));
  if (!handle) {
    blog(LOG_WARNING, "[roadie-signal] core %s did not load", version.c_str());
    return false;
  }
  auto setPointer = reinterpret_cast<SetPointerFn>(symbol(handle, "obs_module_set_pointer"));
  auto load = reinterpret_cast<LoadFn>(symbol(handle, "obs_module_load"));
  if (!setPointer || !load) {
    blog(LOG_WARNING, "[roadie-signal] core %s is not a Roadie Signal core", version.c_str());
    closeLibrary(handle);
    return false;
  }
  // The core is the plugin: its config, data and locale are this module's.
  setPointer(obs_current_module());
  out.handle = handle;
  out.version = version;
  out.load = load;
  out.postLoad = reinterpret_cast<VoidFn>(symbol(handle, "obs_module_post_load"));
  out.unload = reinterpret_cast<VoidFn>(symbol(handle, "obs_module_unload"));
  out.setLocale = reinterpret_cast<LocaleFn>(symbol(handle, "obs_module_set_locale"));
  out.freeLocale = reinterpret_cast<VoidFn>(symbol(handle, "obs_module_free_locale"));
  return true;
}

// This start reached "good": the core is the last good one, and no start is in progress.
void markGood(const std::string &version)
{
  std::lock_guard<std::mutex> hold(stateLock);
  obs_data_t *state = readState();
  obs_data_set_string(state, "lastGood", version.c_str());
  obs_data_erase(state, "starting");
  writeState(state);
  obs_data_release(state);
}

} // namespace

namespace {
// Picks a core, opens it and records the start. False: none could be opened.
bool pickCore()
{
  std::lock_guard<std::mutex> hold(stateLock);
  obs_data_t *state = readState();

  std::string lastGood = str(state, "lastGood");
  if (lastGood != kBundled && !validVersion(lastGood))
    lastGood = kBundled;
  std::string pending = str(state, "pending");
  std::string current = str(state, "current");
  std::string want = validVersion(pending) ? pending : validVersion(current) ? current : kBundled;
  if (want != kBundled && isBad(state, want))
    want = lastGood;

  // How many starts of this core in a row never reached "good".
  long long strikes = 0;
  obs_data_t *starting = obs_data_get_obj(state, "starting");
  if (starting && str(starting, "version") == want)
    strikes = obs_data_get_int(starting, "strikes") + 1;
  obs_data_release(starting);

  if (strikes >= kStrikes && want != lastGood) {
    markBad(state, want);
    noteRollback(state, want, lastGood);
    want = lastGood;
    strikes = 0;
  }

  // The core to run, then the last good one, then the one the installer shipped.
  std::vector<std::string> order{want, lastGood, kBundled};
  order.erase(std::unique(order.begin(), order.end()), order.end());
  bool opened = false;
  for (size_t i = 0; i < order.size() && !opened; ++i) {
    if (i > 0 && order[i] != order[i - 1]) {
      markBad(state, order[i - 1]);
      noteRollback(state, order[i - 1], order[i]);
      strikes = 0;
    }
    opened = open(order[i], core);
  }
  if (!opened) {
    writeState(state);
    obs_data_release(state);
    blog(LOG_ERROR, "[roadie-signal] no core could be loaded; reinstall Roadie Signal");
    return false;
  }

  obs_data_set_string(state, "current", core.version.c_str());
  if (pending == core.version || isBad(state, pending))
    obs_data_erase(state, "pending");
  obs_data_t *mark = obs_data_create();
  obs_data_set_string(mark, "version", core.version.c_str());
  obs_data_set_int(mark, "strikes", strikes);
  obs_data_set_obj(state, "starting", mark);
  obs_data_release(mark);
  writeState(state);
  obs_data_release(state);
  blog(LOG_INFO, "[roadie-signal] loader %s running core %s", ROADIE_SIGNAL_VERSION, core.version.c_str());
  return true;
}
} // namespace

bool obs_module_load()
{
  if (!pickCore())
    return false;
  if (!core.load()) {
    // The core refused this OBS (too old, say): not the core's fault, so no strike.
    std::lock_guard<std::mutex> hold(stateLock);
    obs_data_t *state = readState();
    obs_data_erase(state, "starting");
    writeState(state);
    obs_data_release(state);
    return false;
  }

  const std::string version = core.version;
  healthyTimer = std::thread([version] {
    std::unique_lock<std::mutex> wait(waitLock);
    if (!wake.wait_for(wait, kHealthy, [] { return stopping; }))
      markGood(version);
  });
  return true;
}

void obs_module_post_load()
{
  if (core.postLoad)
    core.postLoad();
}

void obs_module_unload()
{
  {
    std::lock_guard<std::mutex> hold(waitLock);
    stopping = true;
  }
  wake.notify_all();
  if (healthyTimer.joinable())
    healthyTimer.join();
  if (core.unload)
    core.unload();
  // A clean close counts as good, however short the session was.
  if (!core.version.empty())
    markGood(core.version);
}

void obs_module_set_locale(const char *locale)
{
  if (core.setLocale)
    core.setLocale(locale);
}

void obs_module_free_locale()
{
  if (core.freeLocale)
    core.freeLocale();
}
