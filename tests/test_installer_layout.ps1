[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$installer = Get-Content -LiteralPath (Join-Path $root 'installer\termite.iss') -Raw
$full_package_rule = 'Source: "..\.tooling\release\vb_cable\*"; DestDir: "{app}\support\vb_cable"; Flags: ignoreversion recursesubdirs createallsubdirs'

if (-not $installer.Contains($full_package_rule)) {
    throw 'The installer must bundle the complete VB-CABLE directory, not only the setup executable.'
}

foreach ($expected_path in @(
    '{app}\support\vb_cable\VBCABLE_Setup_x64.exe',
    '{app}\support\vb_cable\VBCABLE_Setup.exe'
)) {
    if (-not $installer.Contains($expected_path)) {
        throw "Expected VB-CABLE setup path '$expected_path' was not found."
    }
}

Write-Host 'VB-CABLE installer layout checks passed.'
