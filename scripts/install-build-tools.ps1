# Run this from a normal PowerShell terminal. Windows will request elevation.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$repo = Split-Path $PSScriptRoot -Parent
$tools = Join-Path $repo 'build/tools'
New-Item -ItemType Directory -Force $tools | Out-Null
$installer = Join-Path $tools 'vs_BuildTools-17.14.41.exe'
$url = 'https://download.visualstudio.microsoft.com/download/pr/bc92e2cb-33de-4a0c-995d-efa817f16b16/37bb0fb429d163ecebd272a865d11a37b906d152bef960da2ddb29c2e2fd6eeb/vs_BuildTools.exe'
$expected = '37bb0fb429d163ecebd272a865d11a37b906d152bef960da2ddb29c2e2fd6eeb'
if (!(Test-Path $installer)) { Invoke-WebRequest $url -OutFile $installer }
if ((Get-FileHash $installer).Hash -ne $expected) { throw 'Build Tools installer checksum mismatch.' }
$signature = Get-AuthenticodeSignature $installer
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Microsoft Corporation') {
    throw 'Build Tools installer Microsoft signature could not be verified.'
}
$process = Start-Process -FilePath $installer -Verb RunAs -WindowStyle Hidden -Wait -PassThru `
    -ArgumentList '--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended'
if ($process.ExitCode -eq 3010) {
    Write-Host 'Build Tools installed; Windows reports a pending restart. Try the build before restarting.'
} elseif ($process.ExitCode -ne 0) {
    throw "Build Tools setup failed with exit code $($process.ExitCode)."
} else {
    Write-Host 'C++ Build Tools installed. No restart requested.'
}