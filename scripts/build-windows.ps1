[CmdletBinding()]
param(
    [string]$ObsRoot = (Join-Path $env:ProgramFiles 'obs-studio'),
    [string]$Origin = 'https://dashboard.roadie.gg'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$cache = Join-Path $repo 'build/sdk'
$lock = Get-Content (Join-Path $PSScriptRoot 'sdk-lock.json') -Raw | ConvertFrom-Json
$cmake = Join-Path $cache 'cmake-3.30.5-windows-x86_64/bin/cmake.exe'
$obsExe = Join-Path $ObsRoot 'bin/64bit/obs64.exe'
if (!(Test-Path $obsExe) -or (Get-Item $obsExe).VersionInfo.ProductVersion -ne $lock.obsVersion) {
    throw "This test build requires installed OBS $($lock.obsVersion)."
}
$qtDll = Join-Path $ObsRoot 'bin/64bit/Qt6Core.dll'
if ((Get-Item $qtDll).VersionInfo.ProductVersion -ne "$($lock.qt.version).0") {
    throw 'Installed OBS Qt version does not match the pinned SDK.'
}
if (!(Test-Path $cmake)) { throw 'Run scripts/prepare-sdk.ps1 first.' }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path $vswhere)) { throw 'Install Microsoft C++ Build Tools with the VCTools workload first.' }
$vs = & $vswhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'No installed MSVC x64 toolchain found.' }
$devcmd = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
# Capture the developer environment internally. Never print environment values.
$devEnvironment = & $env:ComSpec /d /s /c "`"`"$devcmd`" -arch=x64 -host_arch=x64 >nul && set`""
if ($LASTEXITCODE) { throw 'MSVC environment setup failed.' }
foreach ($line in $devEnvironment) {
    if ($line -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
    }
}
function Invoke-Checked($exe, $arguments) {
    & $exe @arguments
    if ($LASTEXITCODE) { throw "$exe failed with exit code $LASTEXITCODE" }
}
$imports = Join-Path $cache 'host-imports'
New-Item -ItemType Directory -Force $imports | Out-Null
foreach ($name in @('obs', 'obs-frontend-api')) {
    $dll = Join-Path $ObsRoot "bin/64bit/$name.dll"
    $exports = & dumpbin.exe /nologo /exports $dll
    if ($LASTEXITCODE) { throw "Could not inspect exports: $name" }
    $symbols = @($exports | ForEach-Object {
        if ($_ -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)') { $Matches[1] }
    })
    if ($symbols.Count -eq 0) { throw "No native exports found: $name" }
    @("LIBRARY $name.dll", 'EXPORTS') + $symbols | Set-Content "$imports/$name.def" -Encoding ascii
    Invoke-Checked 'lib.exe' @('/nologo', '/machine:x64', "/def:$imports/$name.def", "/out:$imports/$name.lib")
}
$build = Join-Path $repo 'build/plugin'
Invoke-Checked $cmake @('-S', $repo, '-B', $build, '-G', 'Visual Studio 17 2022', '-A', 'x64',
    "-DCMAKE_PREFIX_PATH=$cache/qt6", "-DROADIE_OBS_SOURCE_DIR=$cache/obs-studio-32.2.1",
    "-DROADIE_OBS_HOST_DIR=$ObsRoot", "-DROADIE_OBS_IMPORT_DIR=$imports",
    "-DOBS_BROWSER_PANEL_INCLUDE_DIR=$cache/browser-panel", "-DROADIE_SIGNAL_ORIGIN=$Origin")
Invoke-Checked $cmake @('--build', $build, '--config', 'Release', '--parallel')
Invoke-Checked $cmake @('--install', $build, '--config', 'Release', '--prefix', "$repo/stage")
$dll = Join-Path $repo 'stage/obs-plugins/64bit/roadie-signal.dll'
$dependencies = & dumpbin.exe /nologo /dependents $dll
if ($LASTEXITCODE) { throw 'DLL dependency inspection failed.' }
$dependencies | Set-Content "$repo/stage/dependencies.txt"
$exports = & dumpbin.exe /nologo /exports $dll
if ($LASTEXITCODE -or ($exports -join "`n") -notmatch 'obs_module_load') { throw 'OBS module export missing.' }
$report = [ordered]@{
    obsVersion = $lock.obsVersion
    qtVersion = $lock.qt.version
    origin = $Origin
    dllSha256 = (Get-FileHash $dll).Hash.ToLowerInvariant()
    hostTested = $false
    sourceCommit = (& git -C $repo rev-parse HEAD)
    sourceDirty = [bool](& git -C $repo status --porcelain)
    obsDllSha256 = (Get-FileHash (Join-Path $ObsRoot 'bin/64bit/obs.dll')).Hash.ToLowerInvariant()
}
$report | ConvertTo-Json | Set-Content "$repo/stage/build-report.json"
Write-Host "Release DLL staged at $dll. No plugin installed into OBS."