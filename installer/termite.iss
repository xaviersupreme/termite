#define app_name "Termite"
#define app_version "0.1.0"
#define app_publisher "Termite"
#define app_url "https://vb-audio.com/Cable/"

[Setup]
AppId={{8B9418C8-A7DD-4E70-9A4B-12A6A9D5871B}
AppName={#app_name}
AppVersion={#app_version}
AppPublisher={#app_publisher}
DefaultDirName={autopf}\Termite
DefaultGroupName=Termite
OutputDir=output
OutputBaseFilename=termite_setup
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible

[Files]
Source: "..\dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\.tooling\release\vb_cable\VBCABLE_Setup_x64.exe"; DestDir: "{app}\support"; Flags: ignoreversion; Check: IsWin64
Source: "..\.tooling\release\vb_cable\VBCABLE_Setup.exe"; DestDir: "{app}\support"; Flags: ignoreversion; Check: not IsWin64
Source: "..\.tooling\release\vc_redist\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall; Check: IsWin64

[Icons]
Name: "{group}\Termite"; Filename: "{app}\Termite.exe"
Name: "{group}\Install or repair VB-CABLE"; Filename: "{app}\support\VBCABLE_Setup_x64.exe"; Parameters: "-i"; Check: IsWin64
Name: "{group}\Install or repair VB-CABLE"; Filename: "{app}\support\VBCABLE_Setup.exe"; Parameters: "-i"; Check: not IsWin64
Name: "{autodesktop}\Termite"; Filename: "{app}\Termite.exe"; Tasks: desktop_icon

[Tasks]
Name: "desktop_icon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "Installing Microsoft Visual C++ runtime..."; Flags: waituntilterminated; Check: NeedsVcRedist
Filename: "{app}\Termite.exe"; Description: "Launch Termite"; Flags: nowait postinstall skipifsilent; Check: DriverReady

[Code]
var
  driver_ready: Boolean;
  driver_reboot_required: Boolean;

function NeedsVcRedist: Boolean;
var
  Installed: Cardinal;
begin
  Result := True;
  if RegQueryDWordValue(HKLM, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed) then
    Result := Installed <> 1;
end;

function VbCableInstallerPath: String;
begin
  if IsWin64 then
    Result := ExpandConstant('{app}\support\VBCABLE_Setup_x64.exe')
  else
    Result := ExpandConstant('{app}\support\VBCABLE_Setup.exe');
end;

function VbCableProbeParams: String;
begin
  Result := '-NoProfile -NonInteractive -ExecutionPolicy Bypass -Command "' +
    '$ErrorActionPreference=''Stop''; ' +
    '$driver = @(Get-PnpDevice -PresentOnly -ErrorAction Stop | Where-Object { $_.FriendlyName -match ''VB-Audio|CABLE'' }); ' +
    '$property = ''{a45c254e-df1c-4efd-8020-67d146a850e0},2''; ' +
    '$render = @(Get-ChildItem ''HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render'' -ErrorAction Stop | ForEach-Object { (Get-ItemProperty -LiteralPath $_.PSPath -ErrorAction Stop).$property }); ' +
    '$capture = @(Get-ChildItem ''HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Capture'' -ErrorAction Stop | ForEach-Object { (Get-ItemProperty -LiteralPath $_.PSPath -ErrorAction Stop).$property }); ' +
    'if (($driver.Count -gt 0) -and (@($render -match ''^CABLE Input'').Count -gt 0) -and (@($capture -match ''^CABLE Output'').Count -gt 0)) { exit 0 } else { exit 1 }"';
end;

function HasVbCableEndpoints: Boolean;
var
  ResultCode: Integer;
begin
  Result := Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'), VbCableProbeParams, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) and (ResultCode = 0);
  if Result then
    Log('VB-CABLE driver and CABLE Input/Output endpoints are present.')
  else
    Log('VB-CABLE driver or CABLE Input/Output endpoints are not present.');
end;

procedure InstallVbCableIfNeeded;
var
  ResultCode: Integer;
  InstallerPath: String;
  FailureMessage: String;
begin
  driver_ready := HasVbCableEndpoints;
  if driver_ready then begin
    Log('VB-CABLE is already ready; skipping the vendor installer.');
    exit;
  end;

  InstallerPath := VbCableInstallerPath;
  if not FileExists(InstallerPath) then begin
    MsgBox('The bundled VB-CABLE installer is missing. Termite was installed, but it will not launch until VB-CABLE is repaired.', mbError, MB_OK);
    exit;
  end;

  if not Exec(InstallerPath, '-i', ExtractFileDir(InstallerPath), SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode) then begin
    FailureMessage := 'VB-CABLE setup could not be launched: ' + SysErrorMessage(ResultCode) + '.';
  end else if ResultCode <> 0 then begin
    FailureMessage := 'VB-CABLE setup exited with code ' + IntToStr(ResultCode) + '.';
  end else begin
    driver_ready := HasVbCableEndpoints;
    if driver_ready then begin
      MsgBox('VB-CABLE is ready. CABLE Input and CABLE Output were detected.', mbInformation, MB_OK);
      exit;
    end;
    driver_reboot_required := True;
    FailureMessage := 'VB-CABLE setup completed, but CABLE Input and CABLE Output are not active yet. Restart Windows, then use the Install or repair VB-CABLE shortcut if they are still missing.';
  end;

  MsgBox(FailureMessage + #13#10 + #13#10 + 'Termite will not launch automatically. You can retry from Start Menu > Termite > Install or repair VB-CABLE.', mbError, MB_OK);
end;

function DriverReady: Boolean;
begin
  Result := driver_ready;
end;

function NeedRestart: Boolean;
begin
  Result := driver_reboot_required;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    InstallVbCableIfNeeded;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = wpWelcome then
    MsgBox('Termite installs VB-CABLE by VB-Audio. VB-CABLE is donationware. You must be able to identify it as a VB-Audio product and may donate if you find it useful.' + #13#10 + #13#10 + 'The VB-Audio setup window will be shown if the driver is missing. Termite launches only after CABLE Input and CABLE Output are detected. Restart Windows if requested.', mbInformation, MB_OK);
end;
