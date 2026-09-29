# Downloads public, pinned prerequisites only. Does not install tools or OBS plugins.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$repo = Split-Path $PSScriptRoot -Parent
$tools = Join-Path $repo 'build/sdk'
$lock = Get-Content (Join-Path $PSScriptRoot 'sdk-lock.json') -Raw | ConvertFrom-Json
$cache = Join-Path $tools $lock.obsVersion
New-Item -ItemType Directory -Force $cache | Out-Null
function Get-VerifiedFile($item, $name) {
    $path = Join-Path $cache $name
    if (!(Test-Path -LiteralPath $path)) {
        Invoke-WebRequest -Uri $item.url -OutFile $path
    }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $item.sha256) {
        throw "Checksum mismatch: $name. Inspect the cached download before retrying."
    }
    return $path
}
$obs = Get-VerifiedFile $lock.obsSource 'obs-source.zip'
$qt = Get-VerifiedFile $lock.qt 'qt6.zip'
$cmake = Join-Path $tools 'cmake.zip'
if (!(Test-Path $cmake)) { Invoke-WebRequest $lock.cmake.url -OutFile $cmake }
if ((Get-FileHash $cmake).Hash -ne $lock.cmake.sha256) { throw 'CMake checksum mismatch.' }
$hostArchive = Get-VerifiedFile $lock.obsHost 'obs-host.zip'
if (!(Test-Path "$cache/obs-host/bin/64bit/obs64.exe")) {
    Expand-Archive -LiteralPath $hostArchive -DestinationPath "$cache/obs-host"
}
if ((Get-AuthenticodeSignature "$cache/obs-host/bin/64bit/obs64.exe").Status -ne 'Valid') {
    throw 'Official baseline OBS executable signature is not valid.'
}
if (!(Test-Path "$cache/obs-studio-$($lock.obsVersion)/libobs/obs.h")) {
    Expand-Archive -LiteralPath $obs -DestinationPath $cache
}
if (!(Test-Path "$cache/qt6/lib/cmake/Qt6/Qt6Config.cmake")) {
    Expand-Archive -LiteralPath $qt -DestinationPath "$cache/qt6"
}
if (!(Test-Path "$tools/cmake-3.30.5-windows-x86_64/bin/cmake.exe")) {
    Expand-Archive -LiteralPath $cmake -DestinationPath $tools
}
New-Item -ItemType Directory -Force "$cache/browser-panel" | Out-Null
$header = Join-Path $cache 'browser-panel/browser-panel.hpp'
if (!(Test-Path -LiteralPath $header)) {
    Invoke-WebRequest $lock.browserPanel.url -OutFile $header
}
if ((Get-FileHash -LiteralPath $header).Hash -ne $lock.browserPanel.sha256) {
    throw 'Browser panel header checksum mismatch.'
}
Write-Host 'Pinned OBS headers, browser panel, Qt and portable CMake are ready.'