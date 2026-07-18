; Build the package first:
;   powershell -ExecutionPolicy Bypass -File tools\make_release.ps1

#define AppName "Termite"
#define AppVersion "0.1.0"
#define PayloadName "Termite-win64.zip"

[Setup]
AppId={{5E516C17-9A6B-4E48-B6A6-0641ED566D98}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Termite
DefaultDirName={autopf}\Termite
DefaultGroupName=Termite
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\release
OutputBaseFilename=Termite-Setup-win64
SetupIconFile=..\assets\termite.ico
UninstallDisplayIcon={app}\Termite.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Files]
; The installer carries the release ZIP unchanged and expands it into the
; selected install directory.  Do not replace this with files from build\.
Source: "..\release\{#PayloadName}"; Flags: dontcopy; DestName: "{#PayloadName}"

[Icons]
Name: "{autoprograms}\Termite"; Filename: "{app}\Termite.exe"; WorkingDir: "{app}"

[Run]
Filename: "{app}\Termite.exe"; Description: "Launch Termite"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: files; Name: "{app}\Termite.exe"
Type: files; Name: "{app}\TermiteUI.exe"
Type: filesandordirs; Name: "{app}\assets"

[Code]
procedure ExpandPayload;
var
  ArchivePath: String;
  Arguments: String;
  ResultCode: Integer;
begin
  ExtractTemporaryFile('{#PayloadName}');
  ArchivePath := ExpandConstant('{tmp}\{#PayloadName}');
  Arguments := '-NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -LiteralPath ''' +
    ArchivePath + ''' -DestinationPath ''' + ExpandConstant('{app}') + ''' -Force"';

  if not Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
      Arguments, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
    RaiseException('Could not unpack the Termite package.')
  else if ResultCode <> 0 then
    RaiseException('Could not unpack the Termite package (PowerShell exit code ' +
      IntToStr(ResultCode) + ').');
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
    ExpandPayload;
end;
