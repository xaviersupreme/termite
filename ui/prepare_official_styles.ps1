[CmdletBinding()]
param(
    [string] $StyleRoot = 'C:\Program Files (x86)\Embarcadero\Studio\23.0\Redist\styles\vcl',
    [string] $ResourceCompiler = 'C:\Program Files (x86)\Embarcadero\Studio\23.0\bin\brcc32.exe'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSCommandPath
$assetRoot = Join-Path $projectRoot 'public_assets'
$styles = @(
    'Amakrits.vsf',
    'AquaGraphite.vsf',
    'AquaLightSlate.vsf',
    'Auric.vsf',
    'Carbon.vsf',
    'CobaltXEMedia.vsf',
    'CyanDusk.vsf',
    'CyanNight.vsf',
    'Glossy.vsf',
    'GoldenGraphite.vsf',
    'Luna.vsf',
    'RubyGraphite.vsf',
    'Silver.vsf',
    'SlateClassico.vsf'
)

if (-not (Test-Path -LiteralPath $StyleRoot)) {
    throw "RAD Studio redistributable styles were not found: $StyleRoot"
}
if (-not (Test-Path -LiteralPath $ResourceCompiler)) {
    throw "RAD Studio resource compiler was not found: $ResourceCompiler"
}

New-Item -ItemType Directory -Force -Path $assetRoot | Out-Null
foreach ($style in $styles) {
    $source = Join-Path $StyleRoot $style
    if (-not (Test-Path -LiteralPath $source)) {
        throw "Required official VCL style is missing: $source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $assetRoot $style) -Force
}

& $ResourceCompiler "-fo$projectRoot\Carbon.res" "$projectRoot\Carbon.rc"
if ($LASTEXITCODE -ne 0) {
    throw "The RAD Studio resource compiler failed with exit code $LASTEXITCODE."
}

Write-Host 'Prepared official VCL style resources for TermiteUI.'
