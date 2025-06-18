[CmdletBinding()]
param(
    [switch]$skip_tool_install
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'common.psm1') -Force

function Test-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Ensure-WinGetPackage {
    param([scriptblock]$is_installed, [string]$package_id, [string]$override = '')

    if (& $is_installed) { return }
    $installed = & winget list --exact --id $package_id --source winget 2>$null
    if ($LASTEXITCODE -eq 0 -and $installed -match [regex]::Escape($package_id)) {
        Write-Host ("  {0} {1} is already installed" -f ([char]0x2713), $package_id) -ForegroundColor DarkGreen
        return
    }
    $arguments = @('install', '--exact', '--id', $package_id, '--source', 'winget', '--accept-package-agreements', '--accept-source-agreements')
    if ($override) { $arguments += @('--override', $override) }
    Invoke-TermiteProcess -Title "Installing $package_id" -FilePath 'winget' -Arguments $arguments
}

$root = Get-TermiteRoot
Write-TermiteBanner

if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    throw 'WinGet is required. Install Microsoft App Installer, then run this script again.'
}
if (-not $skip_tool_install -and -not (Test-Administrator)) {
    $arguments = "-NoExit -NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""
    Start-Process -FilePath powershell.exe -Verb RunAs -Wait -ArgumentList $arguments
    exit $LASTEXITCODE
}

if (-not $skip_tool_install) {
    Write-TermiteSection 'Build tools'
    Ensure-WinGetPackage -is_installed { Get-Command git -ErrorAction SilentlyContinue } -package_id 'Git.Git'
    Ensure-WinGetPackage -is_installed { Get-Command cmake -ErrorAction SilentlyContinue } -package_id 'Kitware.CMake'
    Ensure-WinGetPackage -is_installed { try { Resolve-InnoSetupCompiler | Out-Null; $true } catch { $false } } -package_id 'JRSoftware.InnoSetup'
    $vsdevcmd_path = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat'
    Ensure-WinGetPackage -is_installed { Test-Path -LiteralPath $vsdevcmd_path } -package_id 'Microsoft.VisualStudio.2022.BuildTools' -override "--wait --passive --norestart --config `"$(Join-Path $root 'scripts\termite.vsconfig')`""
}

$cmake = Resolve-TermiteCommand -Name 'cmake' -FallbackPath (Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe')
$ctest = Resolve-TermiteCommand -Name 'ctest' -FallbackPath (Join-Path $env:ProgramFiles 'CMake\bin\ctest.exe')
$build = Assert-WorkspaceChild -Path (Join-Path $root 'build')
if (Test-Path -LiteralPath $build) { Remove-Item -LiteralPath $build -Recurse -Force }
Write-TermiteSection 'Build, test, and package'
Invoke-TermiteProcess -Title 'Configuring CMake and Direct2D' -FilePath $cmake -Arguments @('-S', $root, '-B', $build, '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DTERMITE_BUILD_APP=ON', '-DTERMITE_BUILD_TESTS=ON')
Invoke-TermiteProcess -Title 'Building Termite' -FilePath $cmake -Arguments @('--build', $build, '--config', 'Release', '--parallel')
Invoke-TermiteProcess -Title 'Running tests' -FilePath $ctest -Arguments @('--test-dir', $build, '-C', 'Release', '--output-on-failure')
Invoke-TermiteProcess -Title 'Packaging installer' -FilePath 'powershell.exe' -Arguments @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $PSScriptRoot 'package_release.ps1'), '-build_directory', $build)
Write-Host ("`n  {0} Done: artifacts\termite_setup.exe`n" -f ([char]0x2713)) -ForegroundColor Green
