[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$installer = Get-Content -LiteralPath (Join-Path $root 'installer\termite.iss') -Raw
$resources = Get-Content -LiteralPath (Join-Path $root 'src\app\termite.rc') -Raw
$window_source = Get-Content -LiteralPath (Join-Path $root 'src\app\console_window.cpp') -Raw
$full_package_rule = 'Source: "..\.tooling\release\vb_cable\*"; DestDir: "{app}\support\vb_cable"; Flags: ignoreversion recursesubdirs createallsubdirs'

if (-not $installer.Contains($full_package_rule)) {
    throw 'The installer must bundle the complete VB-CABLE directory, not only the setup executable.'
}

if (-not (Test-Path -LiteralPath (Join-Path $root 'assets\termite.ico')) -or -not $resources.Contains('IDI_TERMITE ICON "../../assets/termite.ico"')) {
    throw 'Termite must embed assets\termite.ico as its application icon resource.'
}

if (-not $window_source.Contains('skin_->draw_title_icon(console_layout::title_icon());') -or
    $window_source.Contains('skin_->draw_text(L"T", console_layout::title_icon()')) {
    throw 'The custom title bar must draw the embedded Termite icon instead of a text glyph.'
}

if (-not $window_source.Contains('case WM_CLOSE:') -or -not $window_source.Contains('hide_to_tray();') -or
    -not $window_source.Contains('tray_quit_command')) {
    throw 'Closing Termite must hide it to the notification area and retain an explicit Quit command.'
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
