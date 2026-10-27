#!/usr/bin/env bash
# Builds Roadie Signal for macOS (Apple Silicon) from this checkout, against the installed
# OBS.app, and packages it the way the Windows build does: a release zip, a build report and
# SHA256SUMS in stage/macos. Nothing is installed into OBS unless --install is passed.
#
# Usage:  bash scripts/build-macos.sh             (build + package)
#         bash scripts/build-macos.sh --install   (also install for this user, with a backup)
#
# Needs: an Apple Silicon Mac, Xcode Command Line Tools, OBS.app (OBS_APP=/path to override).
# The build links against OBS.app's own libobs, obs-frontend-api and Qt, with headers from the
# matching OBS tag. The plugin then needs that OBS version's Qt or newer at runtime (plugin.cpp
# refuses an older Qt), so build against the OLDEST OBS the Mac release should support.
#
# Based on the build script CtrlGremlin contributed with the first macOS build (2026-10-27).
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
OBS_APP="${OBS_APP:-/Applications/OBS.app}"
WORK="$REPO/build/macos"
STAGE="$REPO/stage/macos"
PLUGINS="$HOME/Library/Application Support/obs-studio/plugins"
SIMDE_TAG="v0.8.2"
CMAKE_FALLBACK_VER="3.30.5"
CMAKE_FALLBACK_SHA="3d603e507c7579b13518ef752b4ffcf3ed479fba80ee171d7d85da8153e869d0"
INSTALL=0
[ "${1:-}" = "--install" ] && INSTALL=1

say()  { printf '\n\033[1;36m==> %s\033[0m\n' "$*"; }
die()  { printf '\n\033[1;31mERROR: %s\033[0m\n' "$*" >&2; exit 1; }
sha()  { shasum -a 256 "$1" | awk '{print $1}'; }

# ---------------------------------------------------------------- preflight
say "Checking toolchain"
[ "$(uname -s)" = "Darwin" ] || die "Run this on macOS."
[ "$(uname -m)" = "arm64" ]  || die "This script builds arm64 only."
xcode-select -p >/dev/null 2>&1 || die "Xcode Command Line Tools missing: run  xcode-select --install  then re-run."
command -v git >/dev/null && command -v python3 >/dev/null && command -v clang++ >/dev/null \
  || die "git/python3/clang++ not found (Command Line Tools incomplete?)"
[ -d "$OBS_APP" ] || die "OBS.app not found at $OBS_APP (set OBS_APP=/path/to/OBS.app)"

VERSION="$(sed -n 's/^project(roadie-signal VERSION \([0-9.]*\) .*/\1/p' "$REPO/CMakeLists.txt")"
[ -n "$VERSION" ] || die "Could not read the plugin version from CMakeLists.txt"
OBS_VER="$(defaults read "$OBS_APP/Contents/Info.plist" CFBundleShortVersionString)"
OBS_MAJOR="${OBS_VER%%.*}"
[ "$OBS_MAJOR" -ge 30 ] 2>/dev/null || die "OBS $OBS_VER is older than 30."
echo "Roadie Signal $VERSION; OBS.app $OBS_VER ($OBS_APP)"
FW="$OBS_APP/Contents/Frameworks"
[ -e "$FW/libobs.framework/libobs" ]  || die "libobs.framework missing in OBS.app"
[ -e "$FW/obs-frontend-api.dylib" ]   || die "obs-frontend-api.dylib missing in OBS.app"

mkdir -p "$WORK"
cd "$WORK"

CMAKE=cmake
need_cmake=1
if command -v cmake >/dev/null; then
  cv="$(cmake --version 2>/dev/null | awk 'NR==1 {print $3}' || true)"
  if [ "$(printf '%s\n3.28\n' "$cv" | sort -V | head -1)" = "3.28" ]; then need_cmake=0; fi
fi
if [ $need_cmake -eq 1 ]; then
  say "Fetching portable CMake $CMAKE_FALLBACK_VER (no system changes)"
  f="cmake-$CMAKE_FALLBACK_VER-macos-universal.tar.gz"
  [ -f "$f" ] || curl -fL --progress-bar -o "$f" "https://github.com/Kitware/CMake/releases/download/v$CMAKE_FALLBACK_VER/$f"
  [ "$(sha "$f")" = "$CMAKE_FALLBACK_SHA" ] || { rm -f "$f"; die "CMake archive checksum mismatch"; }
  [ -d "cmake-$CMAKE_FALLBACK_VER-macos-universal" ] || tar xzf "$f"
  CMAKE="$WORK/cmake-$CMAKE_FALLBACK_VER-macos-universal/CMake.app/Contents/bin/cmake"
fi
echo "cmake: $("$CMAKE" --version | awk 'NR==1')"

# ---------------------------------------------------------------- SDK
say "OBS $OBS_VER headers (matching the installed OBS)"
OBS_SRC="$WORK/obs-studio-$OBS_VER"
[ -d "$OBS_SRC/libobs" ] || git -c advice.detachedHead=false clone -q --depth 1 --branch "$OBS_VER" \
  https://github.com/obsproject/obs-studio.git "$OBS_SRC"

# browser-panel.hpp from the obs-browser commit this OBS tag pins, not obs-browser's latest.
BP_COMMIT="$(git -C "$OBS_SRC" ls-tree HEAD plugins/obs-browser | awk '{print $3}')"
[ -n "$BP_COMMIT" ] || die "Could not resolve the obs-browser submodule commit for $OBS_VER"
BP_DIR="$WORK/obs-browser-$BP_COMMIT"
mkdir -p "$BP_DIR"
[ -s "$BP_DIR/browser-panel.hpp" ] || curl -fsSL -o "$BP_DIR/browser-panel.hpp" \
  "https://raw.githubusercontent.com/obsproject/obs-browser/$BP_COMMIT/panel/browser-panel.hpp"
echo "browser-panel.hpp from obs-browser@$BP_COMMIT"

say "Qt 6 matching OBS $OBS_VER (obs-deps, checksum from OBS's buildspec)"
QT_INFO="$(python3 -I - "$OBS_SRC" <<'PY'
import json, os, sys
src = sys.argv[1]
bs = os.path.join(src, "buildspec.json")
if os.path.exists(bs):
    spec = json.load(open(bs))["dependencies"]
else:
    spec = None
    for p in json.load(open(os.path.join(src, "CMakePresets.json")))["configurePresets"]:
        if p["name"] == "dependencies":
            spec = p["vendor"]["obsproject.com/obs-studio"]["dependencies"]
q = spec["qt6"]
rev = (q.get("revision") or {}).get("macos-universal")
name = "macos-deps-qt6-%s-universal%s.tar.xz" % (q["version"], "-v%s" % rev if rev else "")
print(q["baseUrl"], q["version"], name, q["hashes"]["macos-universal"])
PY
)"
read -r QT_BASE QT_DEPVER QT_FILE QT_SHA <<<"$QT_INFO"
QT_DIR="$WORK/obs-deps-qt6-$QT_DEPVER"
if [ ! -f "$QT_DIR/lib/cmake/Qt6/Qt6Config.cmake" ]; then
  [ -f "$QT_FILE" ] || curl -fL --progress-bar -o "$QT_FILE" "$QT_BASE/$QT_DEPVER/$QT_FILE"
  [ "$(sha "$QT_FILE")" = "$QT_SHA" ] || { rm -f "$QT_FILE"; die "Qt archive checksum mismatch"; }
  mkdir -p "$QT_DIR"
  tar xf "$QT_FILE" -C "$QT_DIR"
  xattr -r -d com.apple.quarantine "$QT_DIR" 2>/dev/null || true
fi
# The real Qt version in each QtCore binary ("Qt 6.x.y (...)"), so both sides are measured the
# same way. No grep -m1 / early-exit pipes: under pipefail a SIGPIPE'd strings aborts silently.
qt_bin_ver() {
  local all
  all="$(strings -a "$1" 2>/dev/null | grep -oE '^Qt 6\.[0-9]+\.[0-9]+ \(' || true)"
  all="${all%%$'\n'*}"
  all="${all#Qt }"
  printf '%s' "${all% (}"
}
QT_SDK_VER="$(qt_bin_ver "$QT_DIR/lib/QtCore.framework/QtCore")"
[ -n "$QT_SDK_VER" ] || QT_SDK_VER="$(sed -n 's/.*PACKAGE_VERSION "\(.*\)".*/\1/p' "$QT_DIR/lib/cmake/Qt6/Qt6ConfigVersion.cmake" 2>/dev/null || true)"
QT_SDK_VER="${QT_SDK_VER%%$'\n'*}"
QT_APP_VER="$(qt_bin_ver "$FW/QtCore.framework/QtCore")"
echo "Qt SDK ${QT_SDK_VER:-unknown}   |   Qt inside OBS.app ${QT_APP_VER:-unknown}"
if [ -z "$QT_APP_VER" ]; then
  echo "WARNING: could not read OBS.app's Qt version; continuing with the Qt OBS $OBS_VER pins."
elif [ "${QT_SDK_VER%.*}" != "${QT_APP_VER%.*}" ]; then
  [ "${ROADIE_SKIP_QT_CHECK:-0}" = "1" ] || die "Qt mismatch: SDK $QT_SDK_VER vs OBS.app $QT_APP_VER"
  echo "WARNING: Qt mismatch ignored (ROADIE_SKIP_QT_CHECK=1)"
fi

SIMDE_ARG=()
if [ ! -d "$OBS_SRC/libobs/util/simde" ]; then
  say "SIMDe $SIMDE_TAG headers (OBS 31+ libobs headers need them; the tree no longer carries them)"
  [ -f "$WORK/simde/simde/x86/sse2.h" ] || git -c advice.detachedHead=false clone -q --depth 1 \
    --branch "$SIMDE_TAG" https://github.com/simd-everywhere/simde.git "$WORK/simde"
  SIMDE_ARG=(-DROADIE_SIMDE_INCLUDE_DIR="$WORK/simde")
fi

# ---------------------------------------------------------------- build
DEPLOY=11.0; [ "$OBS_MAJOR" -ge 31 ] && DEPLOY=12.0
say "Configuring (arm64, macOS $DEPLOY+)"
rm -rf "$WORK/plugin"
"$CMAKE" -S "$REPO" -B "$WORK/plugin" -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$DEPLOY" \
  -DCMAKE_PREFIX_PATH="$QT_DIR" \
  -DROADIE_OBS_SOURCE_DIR="$OBS_SRC" \
  -DROADIE_OBS_APP_DIR="$OBS_APP" \
  -DOBS_BROWSER_PANEL_INCLUDE_DIR="$BP_DIR" \
  ${SIMDE_ARG[@]+"${SIMDE_ARG[@]}"}

say "Building"
"$CMAKE" --build "$WORK/plugin" -j"$(sysctl -n hw.ncpu)"

BUNDLE="$WORK/plugin/roadie-signal.plugin"
BIN="$BUNDLE/Contents/MacOS/roadie-signal"
[ -f "$BIN" ] || die "Build finished but $BIN is missing"

say "Verifying the bundle"
file "$BIN"
ARCHS="$(lipo -archs "$BIN")"
[ "$ARCHS" = "arm64" ] || die "Binary is not arm64-only (got: $ARCHS)"
SYMS="$(nm -gU "$BIN")"
for symbol in obs_module_load obs_module_unload obs_module_ver obs_module_set_pointer obs_module_description; do
  case "$SYMS" in *"_$symbol"*) ;; *) die "OBS module export missing: $symbol" ;; esac
done
missing=0
while read -r dep; do
  case "$dep" in
    @rpath/*) rel="${dep#@rpath/}"
              if [ -e "$FW/$rel" ]; then echo "  ok   $dep"; else echo "  MISSING in OBS.app: $dep"; missing=1; fi ;;
    /usr/lib/*|/System/*) ;;
    *) echo "  UNEXPECTED absolute dependency: $dep"; missing=1 ;;
  esac
done < <(otool -L "$BIN" | tail -n +2 | awk '{print $1}')
[ $missing -eq 0 ] || die "Plugin links against something OBS.app does not provide"
codesign --force --sign - --timestamp=none "$BUNDLE"
codesign --verify --verbose=1 "$BUNDLE"

# ---------------------------------------------------------------- package
say "Packaging"
rm -rf "$STAGE"
PKG="$STAGE/Roadie Signal $VERSION for Mac"
mkdir -p "$PKG"
ditto "$BUNDLE" "$PKG/roadie-signal.plugin"
cp "$REPO/packaging/macos/install.command" "$PKG/install.command"
chmod +x "$PKG/install.command"
cp "$REPO/packaging/macos/README.txt" "$PKG/README.txt"
cp "$REPO/LICENSE" "$PKG/LICENSE"
DIRTY=false; [ -n "$(git -C "$REPO" status --porcelain)" ] && DIRTY=true
cat > "$PKG/build-report.json" <<JSON
{
  "version": "$VERSION",
  "platform": "macos",
  "architecture": "arm64",
  "obsVersion": "$OBS_VER",
  "qtVersion": "${QT_SDK_VER:-unknown}",
  "compiler": "$(clang++ --version | awk 'NR==1')",
  "deploymentTarget": "$DEPLOY",
  "signing": "ad-hoc, not notarized",
  "binarySha256": "$(sha "$BIN")",
  "sourceCommit": "$(git -C "$REPO" rev-parse HEAD)",
  "sourceDirty": $DIRTY,
  "builtAt": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "hostTested": false
}
JSON
ARCHIVE="roadie-signal-$VERSION-obs$OBS_VER-macos-arm64.zip"
ditto -c -k --keepParent "$PKG" "$STAGE/$ARCHIVE"
cp "$PKG/build-report.json" "$STAGE/build-report.json"
(cd "$STAGE" && shasum -a 256 "$ARCHIVE" build-report.json > SHA256SUMS-macos.txt)
cat "$STAGE/SHA256SUMS-macos.txt"
if [ "$DIRTY" = true ]; then echo "WARNING: the checkout has uncommitted changes; do not release this build."; fi
echo "Package ready: $STAGE/$ARCHIVE"

if [ $INSTALL -eq 0 ]; then
  say "Built and packaged. Nothing installed into OBS."; exit 0
fi

# ---------------------------------------------------------------- install
if pgrep -xq OBS; then die "OBS is running. Quit it, then re-run with --install."; fi
say "Installing for this user"
if [ -d "$PLUGINS/roadie-signal.plugin" ]; then
  RB="$HOME/Library/Application Support/obs-studio/plugins-backup"
  mkdir -p "$RB"
  STAMP="$(date +%Y%m%d-%H%M%S)"
  mv "$PLUGINS/roadie-signal.plugin" "$RB/roadie-signal.plugin.$STAMP"
  echo "Previous plugin moved to: $RB/roadie-signal.plugin.$STAMP"
fi
mkdir -p "$PLUGINS"
ditto "$BUNDLE" "$PLUGINS/roadie-signal.plugin"
codesign --verify "$PLUGINS/roadie-signal.plugin"
say "Installed: $PLUGINS/roadie-signal.plugin. OBS was not launched."
