unit StyleCatalog;

interface

uses
  Vcl.Themes;

const
  ReferenceThemeCount = 14;
  // These names correspond only to the official VCL redistributables prepared
  // by prepare_official_styles.ps1. Do not substitute extracted third-party
  // resources here.
  ReferenceThemeCaptions: array[0..ReferenceThemeCount - 1] of string = (
    'Glossy',
    'Amakrits',
    'Aqua Graphite',
    'Aqua Light Slate',
    'Auric',
    'Carbon',
    'Cobalt XE Media',
    'Cyan Dusk',
    'Cyan Night',
    'Golden Graphite',
    'Luna',
    'Ruby Graphite',
    'Silver',
    'Slate Classico');
  ReferenceThemeResources: array[0..ReferenceThemeCount - 1] of string = (
    'GLOSSY',
    'AMAKRITS',
    'AQUAGRAPHITE',
    'AQUALIGHTSLATE',
    'AURIC',
    'CARBON',
    'COBALTXEMEDIA',
    'CYANDUSK',
    'CYANNIGHT',
    'GOLDENGRAPHITE',
    'LUNA',
    'RUBYGRAPHITE',
    'SILVER',
    'SLATECLASSICO');
  ReferenceThemeFiles: array[0..ReferenceThemeCount - 1] of string = (
    'Glossy.vsf',
    'Amakrits.vsf',
    'AquaGraphite.vsf',
    'AquaLightSlate.vsf',
    'Auric.vsf',
    'Carbon.vsf',
    'CobaltXEMedia.vsf',
    'CyanDusk.vsf',
    'CyanNight.vsf',
    'GoldenGraphite.vsf',
    'Luna.vsf',
    'RubyGraphite.vsf',
    'Silver.vsf',
    'SlateClassico.vsf');

procedure LoadReferenceStyles;
function ActivateReferenceStyle(AIndex: Integer): Boolean;
function ReferenceThemeIsAvailable(AIndex: Integer): Boolean;
function SavedReferenceThemeIndex: Integer;
function ActiveReferenceThemeIndex: Integer;
procedure SaveReferenceThemeIndex(AIndex: Integer);

implementation

uses
  System.Classes,
  System.IOUtils,
  System.SysUtils,
  System.Win.Registry,
  Winapi.Windows;

var
  ReferenceStyleHandles: array[0..ReferenceThemeCount - 1] of
    TStyleManager.TStyleServicesHandle;
  CurrentReferenceThemeIndex: Integer = 0;

const
  ThemeRegistryKey = '\Software\Termite';
  LegacyThemeRegistryKey = '\Software\Termite\VSound UI Lab';
  ThemeRegistryValue = 'ThemeIndex';

procedure LoadReferenceStyles;
var
  I: Integer;
  ThemeFile: string;
begin
  for I := Low(ReferenceStyleHandles) to High(ReferenceStyleHandles) do
  begin
    ThemeFile := TPath.Combine(ExtractFilePath(ParamStr(0)),
      'themes\' + ReferenceThemeFiles[I]);
    if FileExists(ThemeFile) then
      try
        ReferenceStyleHandles[I] := TStyleManager.LoadFromFile(ThemeFile);
      except
        ReferenceStyleHandles[I] := nil;
      end;
    if (ReferenceStyleHandles[I] = nil) and
      not TStyleManager.TryLoadFromResource(GetModuleHandle(nil),
        ReferenceThemeResources[I], 'VCLSTYLE', ReferenceStyleHandles[I]) then
      ReferenceStyleHandles[I] := nil;
  end;
end;

function ReferenceThemeIsAvailable(AIndex: Integer): Boolean;
begin
  Result := (AIndex >= Low(ReferenceStyleHandles)) and
    (AIndex <= High(ReferenceStyleHandles)) and
    (ReferenceStyleHandles[AIndex] <> nil);
end;

function SavedReferenceThemeIndex: Integer;
var
  Registry: TRegistry;
begin
  Result := 0;
  Registry := TRegistry.Create(KEY_READ);
  try
    Registry.RootKey := HKEY_CURRENT_USER;
    if Registry.OpenKeyReadOnly(ThemeRegistryKey) and
      Registry.ValueExists(ThemeRegistryValue) then
      Result := Registry.ReadInteger(ThemeRegistryValue)
    else if Registry.OpenKeyReadOnly(LegacyThemeRegistryKey) and
      Registry.ValueExists(ThemeRegistryValue) then
      Result := Registry.ReadInteger(ThemeRegistryValue);
  except
    Result := 0;
  end;
  Registry.Free;
  if (Result < 0) or (Result >= ReferenceThemeCount) then
    Result := 0;
end;

function ActiveReferenceThemeIndex: Integer;
begin
  Result := CurrentReferenceThemeIndex;
end;

procedure SaveReferenceThemeIndex(AIndex: Integer);
var
  Registry: TRegistry;
begin
  if (AIndex < 0) or (AIndex >= ReferenceThemeCount) then
    Exit;
  Registry := TRegistry.Create(KEY_READ or KEY_WRITE);
  try
    Registry.RootKey := HKEY_CURRENT_USER;
    if Registry.OpenKey(ThemeRegistryKey, True) then
      Registry.WriteInteger(ThemeRegistryValue, AIndex);
  except
    // A non-writable user profile should not prevent the frontend opening.
  end;
  Registry.Free;
end;

function ActivateReferenceStyle(AIndex: Integer): Boolean;
begin
  Result := False;
  if not ReferenceThemeIsAvailable(AIndex) then
    Exit;
  try
    // A style handle is actually the manager's TMemoryStream.  SetStyle reads
    // it to EOF, but does not rewind it.  Calling SetStyle with the exhausted
    // stream returns nil and silently activates the white system style.
    TStream(ReferenceStyleHandles[AIndex]).Position := 0;
    TStyleManager.SetStyle(ReferenceStyleHandles[AIndex]);
    Result := TStyleManager.IsCustomStyleActive;
    if Result then
      CurrentReferenceThemeIndex := AIndex;
  except
    Result := False;
  end;
end;

end.
