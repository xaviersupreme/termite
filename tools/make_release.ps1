[CmdletBinding()]
param(
    [string]$OutputDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'release')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build'
$hostExe = Join-Path $build 'Termite.exe'
$uiExe = Join-Path $build 'TermiteUI.exe'

if (-not (Test-Path -LiteralPath $hostExe -PathType Leaf)) {
    throw "Missing $hostExe. Build the C++ host first."
}
if (-not (Test-Path -LiteralPath $uiExe -PathType Leaf)) {
    throw 'Missing TermiteUI.exe. Build ui\TermiteUI.dproj as Debug | Win64 first.'
}

$latestUiSource = Get-ChildItem -LiteralPath (Join-Path $root 'ui') -File -Recurse |
    Where-Object { $_.Extension -in @('.dpr', '.dproj', '.pas', '.res') } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if ($latestUiSource -and ((Get-Item -LiteralPath $uiExe).LastWriteTime -lt $latestUiSource.LastWriteTime)) {
    throw "TermiteUI.exe is older than $($latestUiSource.Name). Compile ui\TermiteUI.dproj in Delphi before packaging."
}

$stage = Join-Path $OutputDirectory 'termite-package-stage'
$payload = Join-Path $stage 'Termite'
$archive = Join-Path $OutputDirectory 'Termite-win64.zip'

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
if (Test-Path -LiteralPath $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}
if (Test-Path -LiteralPath $archive) {
    Remove-Item -LiteralPath $archive -Force
}

New-Item -ItemType Directory -Force -Path $payload | Out-Null
Copy-Item -LiteralPath $hostExe -Destination (Join-Path $payload 'Termite.exe')
Copy-Item -LiteralPath $uiExe -Destination (Join-Path $payload 'TermiteUI.exe')

$assets = Join-Path $build 'assets'
if (Test-Path -LiteralPath $assets -PathType Container) {
    Copy-Item -LiteralPath $assets -Destination (Join-Path $payload 'assets') -Recurse
}

# VCL styles are compiled into TermiteUI.  Do not redistribute their source
# .vsf files as loose files in the release archive.
$standaloneStyles = Get-ChildItem -LiteralPath $payload -Filter '*.vsf' -File -Recurse
if ($standaloneStyles) {
    throw 'Release payload contains standalone .vsf theme files.'
}

Compress-Archive -Path (Join-Path $payload '*') -DestinationPath $archive -CompressionLevel Optimal
Remove-Item -LiteralPath $stage -Recurse -Force
Write-Host "Made $archive"
