[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$build_directory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'common.psm1') -Force

$root = Get-TermiteRoot
Write-TermiteBanner
Write-TermiteSection 'Release dependencies'
$manifest = Get-ToolingManifest
$vb_archive = Get-VerifiedDownload -Package $manifest.vb_cable -Destination (Join-Path $root '.tooling\downloads\vb_cable.zip')
$vc_redist = Get-VerifiedDownload -Package $manifest.vc_redist_x64 -Destination (Join-Path $root '.tooling\downloads\vc_redist.x64.exe')

$release_stage = Assert-WorkspaceChild -Path (Join-Path $root '.tooling\release')
$vb_stage = Assert-WorkspaceChild -Path (Join-Path $release_stage 'vb_cable')
$vc_stage = Assert-WorkspaceChild -Path (Join-Path $release_stage 'vc_redist')
Remove-Item -LiteralPath $release_stage -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $vb_stage, $vc_stage | Out-Null
Expand-Archive -LiteralPath $vb_archive -DestinationPath $vb_stage -Force
Copy-Item -LiteralPath $vc_redist -Destination (Join-Path $vc_stage 'vc_redist.x64.exe') -Force

$distribution = Assert-WorkspaceChild -Path (Join-Path $root 'dist')
Remove-Item -LiteralPath $distribution -Recurse -Force -ErrorAction SilentlyContinue
Write-TermiteSection 'Release package'
$cmake = Resolve-TermiteCommand -Name 'cmake' -FallbackPath (Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe')
Invoke-TermiteProcess -Title 'Staging application runtime' -FilePath $cmake -Arguments @('--install', $build_directory, '--config', 'Release', '--prefix', $distribution, '--component', 'runtime')

$iscc_path = Resolve-InnoSetupCompiler
$artifacts = Assert-WorkspaceChild -Path (Join-Path $root 'artifacts')
New-Item -ItemType Directory -Force -Path $artifacts | Out-Null
Invoke-TermiteProcess -Title 'Creating termite_setup.exe' -FilePath $iscc_path -Arguments @("/O$artifacts", (Join-Path $root 'installer\termite.iss'))
