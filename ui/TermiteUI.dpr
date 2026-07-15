program TermiteUI;

uses
  System.IOUtils,
  System.SysUtils,
  Winapi.ShellAPI,
  Winapi.Windows,
  Vcl.Forms,
  Vcl.Styles,
  Vcl.Themes,
  BufferedChart in 'BufferedChart.pas',
  RouteAppsDialog in 'RouteAppsDialog.pas',
  StyleCatalog in 'StyleCatalog.pas',
  TermiteBridge in 'TermiteBridge.pas',
  MainForm in 'MainForm.pas';

{$R *.res}
{$R 'Carbon.res'}

function StartedByTermiteHost: Boolean;
var
  I: Integer;
begin
  Result := False;
  for I := 1 to ParamCount do
    if SameText(ParamStr(I), '--termite-hosted') then
      Exit(True);
end;

function RunningHostIsAvailable: Boolean;
begin
  // A directly launched developer build can join an already-running host.
  // The normal production path below launches the host first.
  Result := WaitNamedPipe('\\.\pipe\termite.eq.v1', 100);
end;

function LaunchHostForStandaloneFrontend: Boolean;
var
  FrontendDirectory, HostPath: string;
begin
  Result := False;
  FrontendDirectory := ExtractFilePath(ParamStr(0));
  HostPath := TPath.Combine(FrontendDirectory, 'Termite.exe');
  if not FileExists(HostPath) then
    Exit;
  Result := NativeInt(ShellExecute(0, 'open', PChar(HostPath), nil,
    PChar(ExtractFilePath(HostPath)), SW_SHOWNORMAL)) > 32;
end;

var
  InitialThemeIndex: Integer;

begin
  // TermiteUI.exe is the host's private frontend.  Letting a second copy open
  // against the first host made an extracted release look like it had a
  // different UI: it was just a restored, unhosted window over the original.
  if not StartedByTermiteHost then
  begin
    if RunningHostIsAvailable then
    begin
      MessageBox(0, 'Termite is already running. Close it before opening another copy.',
        'Termite', MB_OK or MB_ICONINFORMATION);
      Halt(0);
    end;
    if LaunchHostForStandaloneFrontend then
      Halt(0);
  end;
  // Do not rely on automatic resource discovery. It can silently omit styles
  // when the host VCL version differs from the executable that supplied them.
  TStyleManager.AutoDiscoverStyleResources := False;
  Application.Initialize;
  Application.MainFormOnTaskbar := True;
  LoadReferenceStyles;
  InitialThemeIndex := SavedReferenceThemeIndex;
  if not ActivateReferenceStyle(InitialThemeIndex) then
  begin
    InitialThemeIndex := 0;
    if not ActivateReferenceStyle(InitialThemeIndex) then
      raise Exception.Create('The embedded Glossy style could not be loaded.');
  end;
  Application.Title := 'Termite';
  Application.CreateForm(TVSoundRemakeForm, VSoundRemakeForm);
  Application.Run;
end.
