Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:spinner_frames = 0x280B, 0x2819, 0x2839, 0x2838, 0x283C, 0x2834, 0x2826, 0x2827, 0x2807, 0x280F | ForEach-Object { [char]$_ }
$script:success_symbol = [char]0x2713
$script:failure_symbol = [char]0x2717

function Write-TermiteBanner {
    Write-Host ''
    Write-Host '  TERMITE' -ForegroundColor Green
    Write-Host '  Windows selected-app EQ setup' -ForegroundColor DarkGray
    Write-Host ''
}

function Write-TermiteSection {
    param([Parameter(Mandatory = $true)][string]$Title)

    Write-Host "`n  $Title" -ForegroundColor Cyan
}

function Invoke-TermiteProcess {
    param(
        [Parameter(Mandatory = $true)][string]$Title,
        [Parameter(Mandatory = $true)][string]$FilePath,
        [string[]]$Arguments = @(),
        [string]$WorkingDirectory = (Get-TermiteRoot)
    )

    $argument_line = ($Arguments | ForEach-Object {
        if ($_ -match '\s') { '"{0}"' -f $_ } else { $_ }
    }) -join ' '
    Write-Host ("  {0} {1}" -f $script:spinner_frames[0], $Title) -ForegroundColor Yellow
    $process = Start-Process -FilePath $FilePath -ArgumentList $argument_line -WorkingDirectory $WorkingDirectory -Wait -PassThru -NoNewWindow
    $exit_code = $process.ExitCode

    if ($exit_code -eq 0) {
        Write-Host ("  {0} {1}" -f $script:success_symbol, $Title) -ForegroundColor Green
        return
    }

    Write-Host ("  {0} {1}" -f $script:failure_symbol, $Title) -ForegroundColor Red
    throw "$Title failed with exit code $exit_code."
}

function Resolve-TermiteCommand {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$FallbackPath
    )

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -ne $command) { return $command.Source }
    if (Test-Path -LiteralPath $FallbackPath) { return $FallbackPath }
    throw "Could not find $Name. Run bootstrap without -skip_tool_install."
}

function Resolve-InnoSetupCompiler {
    $command = Get-Command iscc -ErrorAction SilentlyContinue
    if ($null -ne $command) { return $command.Source }

    $candidates = @(
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
        (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe')
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    throw 'Inno Setup compiler was not found. Run bootstrap without -skip_tool_install.'
}

function Get-TermiteRoot {
    return (Split-Path -Parent $PSScriptRoot)
}

function Get-ToolingManifest {
    $manifest_path = Join-Path $PSScriptRoot 'tooling_manifest.json'
    return Get-Content -LiteralPath $manifest_path -Raw | ConvertFrom-Json
}

function Assert-WorkspaceChild {
    param([Parameter(Mandatory = $true)][string]$Path)

    $root = [System.IO.Path]::GetFullPath((Get-TermiteRoot))
    $candidate = [System.IO.Path]::GetFullPath($Path)
    if (-not $candidate.StartsWith($root + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to write outside the workspace: $candidate"
    }
    return $candidate
}

function Get-VerifiedDownload {
    param(
        [Parameter(Mandatory = $true)]$Package,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    $destination_path = Assert-WorkspaceChild -Path $Destination
    $directory = Split-Path -Parent $destination_path
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    if (Test-Path -LiteralPath $destination_path) {
        $existing_hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $destination_path).Hash
        if ($existing_hash -eq $Package.sha256) { return $destination_path }
        Remove-Item -LiteralPath $destination_path -Force
    }
    $previous_progress_preference = $ProgressPreference
    $ProgressPreference = 'SilentlyContinue'
    try {
        Invoke-WebRequest -Uri $Package.url -OutFile $destination_path
    } finally {
        $ProgressPreference = $previous_progress_preference
    }
    $actual_hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $destination_path).Hash
    if ($actual_hash -ne $Package.sha256) {
        Remove-Item -LiteralPath $destination_path -Force
        throw "SHA-256 mismatch for $destination_path. Expected $($Package.sha256), got $actual_hash."
    }
    return $destination_path
}

Export-ModuleMember -Function Get-TermiteRoot, Get-ToolingManifest, Assert-WorkspaceChild, Get-VerifiedDownload, Write-TermiteBanner, Write-TermiteSection, Invoke-TermiteProcess, Resolve-TermiteCommand, Resolve-InnoSetupCompiler
