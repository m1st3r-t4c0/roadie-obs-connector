[CmdletBinding()]
param(
    [string]$ObsRoot = '',
    [string]$Origin = 'https://dashboard.roadie.gg'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$projectDeclaration = Get-Content (Join-Path $repo 'CMakeLists.txt') -Raw
if ($projectDeclaration -notmatch 'project\(roadie-signal VERSION ([0-9]+\.[0-9]+\.[0-9]+)') {
    throw 'Could not read the connector version from CMakeLists.txt.'
}
$version = $Matches[1]
$tools = Join-Path $repo 'build/sdk'
$lock = Get-Content (Join-Path $PSScriptRoot 'sdk-lock.json') -Raw | ConvertFrom-Json
$cache = Join-Path $tools $lock.obsVersion
if (!$ObsRoot) { $ObsRoot = Join-Path $cache 'obs-host' }
$cmake = Join-Path $tools 'cmake-3.30.5-windows-x86_64/bin/cmake.exe'
$obsExe = Join-Path $ObsRoot 'bin/64bit/obs64.exe'
if (!(Test-Path $obsExe) -or (Get-Item $obsExe).VersionInfo.ProductVersion -ne $lock.obsVersion) {
    throw "This test build must link against baseline OBS $($lock.obsVersion)."
}
$qtDll = Join-Path $ObsRoot 'bin/64bit/Qt6Core.dll'
if ((Get-Item $qtDll).VersionInfo.ProductVersion -ne "$($lock.qt.version).0") {
    throw 'Baseline OBS Qt version does not match the pinned SDK.'
}
if (!(Test-Path $cmake)) { throw 'Run scripts/prepare-sdk.ps1 first.' }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path $vswhere)) { throw 'Install Microsoft C++ Build Tools with the VCTools workload first.' }
$vs = & $vswhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'No installed MSVC x64 toolchain found.' }
$devcmd = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
# Capture the developer environment internally. Never print environment values.
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = $env:ComSpec
$start.Arguments = "/d /s /c `"`"$devcmd`" -arch=x64 -host_arch=x64 >nul && set`""
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.RedirectStandardOutput = $true
$process = [Diagnostics.Process]::Start($start)
$devEnvironment = $process.StandardOutput.ReadToEnd() -split "`r?`n"
$process.WaitForExit()
if ($process.ExitCode) { throw 'MSVC environment setup failed.' }
$process.Dispose()
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
$build = Join-Path $repo "build/plugin-obs$($lock.obsVersion)"
Invoke-Checked $cmake @('-S', $repo, '-B', $build, '-G', 'Visual Studio 17 2022', '-A', 'x64',
    "-DCMAKE_PREFIX_PATH=$cache/qt6", "-DROADIE_OBS_SOURCE_DIR=$cache/obs-studio-$($lock.obsVersion)",
    "-DROADIE_OBS_HOST_DIR=$ObsRoot", "-DROADIE_OBS_IMPORT_DIR=$imports",
    "-DOBS_BROWSER_PANEL_INCLUDE_DIR=$cache/browser-panel", "-DROADIE_SIGNAL_ORIGIN=$Origin")
Invoke-Checked $cmake @('--build', $build, '--config', 'Release', '--parallel')
Invoke-Checked $cmake @('--install', $build, '--config', 'Release', '--prefix', "$repo/stage/obs$($lock.obsVersion)")
$stage = Join-Path $repo "stage/obs$($lock.obsVersion)"
$dll = Join-Path $stage 'obs-plugins/64bit/roadie-signal.dll'
$dependencies = & dumpbin.exe /nologo /dependents $dll
if ($LASTEXITCODE) { throw 'DLL dependency inspection failed.' }
$dependencies | Set-Content "$stage/dependencies.txt"
$exports = & dumpbin.exe /nologo /exports $dll
if ($LASTEXITCODE) { throw 'DLL export inspection failed.' }
foreach ($symbol in @('obs_module_load', 'obs_module_unload', 'obs_module_ver', 'obs_module_set_pointer', 'obs_module_description')) {
    if (($exports -join "`n") -notmatch "\b$symbol\b") { throw "OBS module export missing: $symbol" }
}
$headers = & dumpbin.exe /nologo /headers $dll
if ($LASTEXITCODE -or ($headers -join "`n") -notmatch '8664 machine') { throw 'Expected an x64 plugin DLL.' }
$report = [ordered]@{
    obsVersion = $lock.obsVersion
    qtVersion = $lock.qt.version
    compiler = 'MSVC ' + (Get-Item (Get-Command cl.exe).Source).VersionInfo.FileVersion
    origin = $Origin
    dllSha256 = (Get-FileHash $dll).Hash.ToLowerInvariant()
    hostTested = $false
    sourceCommit = (& git -C $repo rev-parse HEAD)
    sourceDirty = [bool](& git -C $repo status --porcelain)
    obsDllSha256 = (Get-FileHash (Join-Path $ObsRoot 'bin/64bit/obs.dll')).Hash.ToLowerInvariant()
}
$report | ConvertTo-Json | Set-Content "$stage/build-report.json"
Write-Host "Release DLL staged at $dll. No plugin installed into OBS."
Copy-Item -LiteralPath "$repo/README.md" -Destination "$stage/README.md" -Force
Copy-Item -LiteralPath "$repo/LICENSE" -Destination "$stage/LICENSE" -Force
$archive = Join-Path $repo "build/roadie-signal-$version-obs$($lock.obsVersion)-minimum-windows-x64.zip"
Compress-Archive -Path "$stage/obs-plugins", "$stage/README.md", "$stage/LICENSE", "$stage/build-report.json" -DestinationPath $archive -Force
Copy-Item -LiteralPath $dll -Destination (Join-Path $repo 'build/roadie-signal.dll') -Force
Write-Host "Standalone DLL ready at $repo/build/roadie-signal.dll"
Write-Host "Test package ready at $archive"
