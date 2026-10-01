$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$sdk = Join-Path $repo 'build/sdk/30.0.0'
$cmake = Join-Path $repo 'build/sdk/cmake-3.30.5-windows-x86_64/bin/cmake.exe'
if (!(Test-Path $cmake)) { throw 'The pinned development SDK must already be prepared.' }
& $cmake -S $PSScriptRoot -B "$repo/build/browser-probe-verified" -G 'Visual Studio 17 2022' -A x64 `
    "-DCMAKE_PREFIX_PATH=$sdk/qt6" "-DROADIE_OBS_SOURCE_DIR=$sdk/obs-studio-30.0.0" `
    "-DROADIE_OBS_HOST_DIR=$sdk/obs-host" "-DROADIE_OBS_IMPORT_DIR=$sdk/host-imports" `
    "-DOBS_BROWSER_PANEL_INCLUDE_DIR=$sdk/browser-panel"
if ($LASTEXITCODE) { throw 'Browser probe configuration failed.' }
& $cmake --build "$repo/build/browser-probe-verified" --config Release
if ($LASTEXITCODE) { throw 'Browser probe compilation failed.' }
