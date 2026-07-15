unit MainForm;

interface

uses
  System.Classes,
  System.Generics.Collections,
  System.IOUtils,
  System.JSON,
  System.Math,
  System.SysUtils,
  System.Types,
  Winapi.Windows,
  Winapi.Dwmapi,
  Winapi.Messages,
  Vcl.ComCtrls,
  Vcl.Controls,
  Vcl.Dialogs,
  Vcl.ExtCtrls,
  Vcl.Forms,
  Vcl.Graphics,
  Vcl.Menus,
  Vcl.StdCtrls,
  Vcl.Themes,
  BufferedChart,
  StyleCatalog,
  TermiteBridge;

type
  TUiCommand = (ucNone, ucDetect, ucReset, ucPause, ucRun, ucClearInfo,
    ucSleep, ucDefaultStart, ucHelp, ucMixUp, ucMixDown, ucVolumeUp,
    ucVolumeDown, ucAllZero, ucResetSmoothing, ucDownload, ucEqOn, ucEqOff,
    ucOpenProfile, ucSaveProfile, ucAddArbitrary, ucRemoveArbitrary,
    ucResetArbitrary, ucResetArbitraryTension, ucRouteApps, ucExit,
    ucHelpAbout, ucHelpProfiles, ucHelpRouting);

  TArbitraryFilterKind = (afPeak, afLowShelf, afHighShelf, afLowCut,
    afHighCut);

  TArbitraryFilter = record
    Kind: TArbitraryFilterKind;
    FrequencyHz: Double;
    GainDb: Double;
    Q: Double;
    Enabled: Boolean;
  end;

  TControlLayout = record
    Control: TControl;
    Bounds: TRect;
    FontHeight: Integer;
  end;

  // TControl exposes Font to descendants, while this layout code needs to
  // apply the same scale to both windowed and graphic controls.
  TControlAccess = class(TControl)
  public
    property Font;
  end;

  TVSoundRemakeForm = class(TForm)
  private
    FEqualizerOn: Boolean;
    FInputGain: Integer;
    FWetMix: Integer;
    FSmoothing: Integer;
    FStatusLog: TRichEdit;
    FVolumeDisplay: TEdit;
    FWetDisplay: TEdit;
    FDryDisplay: TEdit;
    FSmoothingDisplay: TEdit;
    FEqualizerOffButton: TRadioButton;
    FEqualizerOnButton: TRadioButton;
    FFaders: array[0..19] of TScrollBar;
    FFaderDisplays: array[0..19] of TEdit;
    FThemeItems: array[0..ReferenceThemeCount - 1] of TMenuItem;
    FActiveThemeIndex: Integer;
    FLayoutItems: array of TControlLayout;
    FLayoutReady: Boolean;
    FBaseFormFontHeight: Integer;
    FLayoutScaleX: Double;
    FLayoutScaleY: Double;
    FChart: TBufferedChart;
    FGraphicShowGrid: TCheckBox;
    FArbitraryChart: TBufferedChart;
    FArbitraryList: TListBox;
    FArbitraryType: TComboBox;
    FArbitraryFrequency: TEdit;
    FArbitraryGain: TEdit;
    FArbitraryQ: TEdit;
    FArbitraryEnabled: TCheckBox;
    FArbitraryFilters: TList<TArbitraryFilter>;
    FArbitraryUpdating: Boolean;
    // Suppress control callbacks while a profile is replacing related state.
    FProfileLoading: Boolean;
    FArbitraryCursorFrequency: TEdit;
    FArbitraryCursorGain: TEdit;
    FArbitraryTension: TScrollBar;
    FArbitraryLogarithmic: TRadioButton;
    FArbitraryLinear: TRadioButton;
    FArbitraryShowMarkers: TCheckBox;
    FArbitraryShowGrid: TCheckBox;
    FArbitraryShowAxes: TCheckBox;
    FArbitraryShowCrosshairs: TCheckBox;
    FArbitrarySpline: TRadioButton;
    FArbitraryInterpolation: TRadioButton;
    FArbitraryStep: TRadioButton;
    FArbitraryCircles: TRadioButton;
    FArbitrarySquares: TRadioButton;
    FArbitraryPreset: TComboBox;
    FArbitraryXLabels: array[0..9] of TLabel;
    FArbitraryDragIndex: Integer;
    FArbitraryCursorX: Integer;
    FArbitraryCursorY: Integer;
    FArbitraryCursorInside: Boolean;
    FGraphicPage: TPanel;
    FArbitraryPage: TPanel;
    FModePanel: TPanel;
    FGraphicModeButton: TButton;
    FArbitraryModeButton: TButton;
    FBridgeTimer: TTimer;
    FBridgeStatusTimer: TTimer;
    FBridgeClient: TTermiteBridgeClient;
    FBridgeSequence: Cardinal;
    FBridgeConnected: Boolean;
    procedure ButtonClick(Sender: TObject);
    procedure EqualizerClick(Sender: TObject);
    procedure FaderChange(Sender: TObject);
    procedure SmoothingChange(Sender: TObject);
    procedure GraphicOptionsChanged(Sender: TObject);
    procedure PresetChange(Sender: TObject);
    procedure ThemeClick(Sender: TObject);
    procedure WMApplyTheme(var Message: TMessage); message WM_APP + 71;
    procedure WMRefreshAfterTheme(var Message: TMessage); message WM_APP + 72;
    procedure FormResize(Sender: TObject);
    procedure DrawGraphicChart(ACanvas: TCanvas; const AClientRect: TRect);
    procedure DrawArbitraryChart(ACanvas: TCanvas; const AClientRect: TRect);
    procedure ArbitrarySelectionChanged(Sender: TObject);
    procedure ArbitraryControlChanged(Sender: TObject);
    procedure ArbitraryOptionsChanged(Sender: TObject);
    procedure ArbitraryPresetChange(Sender: TObject);
    procedure ArbitraryChartMouseDown(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
    procedure ArbitraryChartMouseMove(Sender: TObject; Shift: TShiftState;
      X, Y: Integer);
    procedure ArbitraryChartMouseUp(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
    procedure EqModeClick(Sender: TObject);
    procedure BridgeTimer(Sender: TObject);
    procedure BridgeStatusTimer(Sender: TObject);
  protected
    procedure CreateParams(var Params: TCreateParams); override;
    procedure ApplyDarkWindowFrame;
    procedure CaptureLayout(AParent: TWinControl);
    procedure ApplyLayout;
    function ScaleX(Value: Integer): Integer;
    function ScaleY(Value: Integer): Integer;
    procedure RefreshControls;
    procedure RefreshChart;
    procedure ApplyThemeColors;
    procedure AddStatus(const S: string);
    procedure CreateMenu;
    procedure CreateLeftPanel;
    procedure CreateGraphicPage(APage: TWinControl);
    procedure CreateArbitraryPage(APage: TWinControl);
    procedure CreateFaders(AParent: TWinControl);
    procedure RefreshArbitraryControls;
    procedure RefreshArbitraryGraph;
    procedure AddArbitraryFilter;
    procedure RemoveArbitraryFilter;
    procedure ResetArbitraryFilters;
    procedure SeedArbitraryEndpoints(const GainDb: Double = 0);
    procedure SetArbitraryFlat(const GainDb: Double);
    procedure UpdateArbitraryAxisLabels;
    function ArbitraryFilterCaption(const Filter: TArbitraryFilter): string;
    function ArbitraryPlotRect: TRect;
    function ArbitraryXForFrequency(const FrequencyHz: Double): Integer;
    function ArbitraryFrequencyForX(const X: Integer): Double;
    function ArbitraryYForGain(const GainDb: Double): Integer;
    function ArbitraryGainForY(const Y: Integer): Double;
    procedure SortArbitraryFilters;
    procedure UpdateArbitraryCursor;
    procedure QueueEngineSync;
    procedure SendEngineSnapshot;
    procedure OpenProfile;
    procedure SaveProfile;
    procedure LoadProfileFromFile(const FileName: string);
    procedure ShowRoutingDialog;
    function ArbitraryResponseAt(const FrequencyHz: Double): Double;
    function GraphicSmoothingQ: Double;
    function GraphicResponseAt(const FrequencyHz: Double): Double;
  public
    constructor Create(AOwner: TComponent); override;
    destructor Destroy; override;
  end;

var
  VSoundRemakeForm: TVSoundRemakeForm;

implementation

uses
  RouteAppsDialog;

const
  BaseClientWidth = 1139;
  BaseClientHeight = 622;
  // Preserve the original control geometry at its intended physical scale.
  // The lab remains resizable, but its default window no longer inflates the
  // entire reference UI by roughly twenty percent.
  DefaultClientWidth = 1139;
  DefaultClientHeight = 622;
  LeftPaneWidth = 273;
  // These are the reference arbitrary-EQ limits.  Keep them here instead of
  // scattering display-only values through the renderer: pointer mapping,
  // marker creation, and the response sent to Termite must agree.
  ArbitraryMinimumFrequency = 40.0;
  ArbitraryMaximumFrequency = 18000.0;
  Frequencies: array[0..19] of string = ('40', '55', '75', '103', '141',
    '194', '265', '364', '498', '683', '937', '1284', '1760', '2412',
    '3306', '4532', '6213', '8516', '11673', '16000');
  GraphicFrequencyHz: array[0..19] of Double = (40, 55, 75, 103, 141,
    194, 265, 364, 498, 683, 937, 1284, 1760, 2412, 3306, 4532, 6213,
    8516, 11673, 16000);
  InitialBands: array[0..19] of Integer = (0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

function ClampDouble(const Value, LowValue, HighValue: Double): Double;
begin
  Result := Max(LowValue, Min(HighValue, Value));
end;

function FilterKindCaption(const Kind: TArbitraryFilterKind): string;
begin
  case Kind of
    afPeak: Result := 'Peak / notch';
    afLowShelf: Result := 'Low shelf';
    afHighShelf: Result := 'High shelf';
    afLowCut: Result := 'Low cut';
    afHighCut: Result := 'High cut';
  else
    Result := 'Peak / notch';
  end;
end;

function TryReadFloat(const Text: string; out Value: Double): Boolean;
var
  Settings: TFormatSettings;
begin
  Settings := TFormatSettings.Create('en-US');
  Result := TryStrToFloat(Trim(Text), Value, Settings);
end;

function FilterResponseAt(const Filter: TArbitraryFilter;
  const FrequencyHz: Double): Double;
var
  Ratio, Shape: Double;
begin
  if not Filter.Enabled then
    Exit(0);

  Ratio := Max(0.0001, FrequencyHz / Max(1, Filter.FrequencyHz));
  Shape := Max(0.1, Filter.Q);
  case Filter.Kind of
    afPeak:
      Result := Filter.GainDb * Exp(-0.5 * Sqr((Ln(Ratio) / Ln(2)) * Shape));
    afLowShelf:
      Result := Filter.GainDb / (1 + Power(Ratio, 2 * Shape));
    afHighShelf:
      Result := Filter.GainDb * (1 - 1 / (1 + Power(Ratio, 2 * Shape)));
    afLowCut:
      Result := -24 / (1 + Power(Ratio, 2 * Shape));
    afHighCut:
      Result := -24 * (1 - 1 / (1 + Power(Ratio, 2 * Shape)));
  else
    Result := 0;
  end;
end;

function JsonObjectValue(AObject: TJSONObject; const AName: string): TJSONObject;
var
  Value: TJSONValue;
begin
  Result := nil;
  if not Assigned(AObject) then
    Exit;
  Value := AObject.GetValue(AName);
  if Value is TJSONObject then
    Result := TJSONObject(Value);
end;

function JsonArrayValue(AObject: TJSONObject; const AName: string): TJSONArray;
var
  Value: TJSONValue;
begin
  Result := nil;
  if not Assigned(AObject) then
    Exit;
  Value := AObject.GetValue(AName);
  if Value is TJSONArray then
    Result := TJSONArray(Value);
end;

function JsonStringValue(AObject: TJSONObject; const AName,
  ADefault: string): string;
var
  Value: TJSONValue;
begin
  Result := ADefault;
  if not Assigned(AObject) then
    Exit;
  Value := AObject.GetValue(AName);
  if Assigned(Value) then
    Result := Value.Value;
end;

function JsonNumberValue(AObject: TJSONObject; const AName: string;
  const ADefault: Double): Double;
var
  Value: TJSONValue;
begin
  Result := ADefault;
  if not Assigned(AObject) then
    Exit;
  Value := AObject.GetValue(AName);
  if Assigned(Value) then
    TryReadFloat(Value.Value, Result);
end;

function JsonBooleanValue(AObject: TJSONObject; const AName: string;
  const ADefault: Boolean): Boolean;
var
  Value: TJSONValue;
begin
  Result := ADefault;
  if not Assigned(AObject) then
    Exit;
  Value := AObject.GetValue(AName);
  if Assigned(Value) then
  begin
    if SameText(Value.Value, 'true') then
      Result := True
    else if SameText(Value.Value, 'false') then
      Result := False;
  end;
end;

procedure AddJsonBoolean(AObject: TJSONObject; const AName: string;
  const AValue: Boolean);
begin
  AObject.AddPair(AName, TJSONBool.Create(AValue));
end;

function NewGroup(AOwner: TComponent; AParent: TWinControl;
  const ACaption: string; X, Y, AWidth, AHeight: Integer): TGroupBox;
begin
  Result := TGroupBox.Create(AOwner);
  Result.Parent := AParent;
  Result.Caption := ACaption;
  Result.Font.Name := 'MS Sans Serif';
  Result.Font.Height := -9;
  Result.SetBounds(X, Y, AWidth, AHeight);
end;

function NewButton(AOwner: TComponent; AParent: TWinControl;
  const ACaption: string; X, Y, AWidth, AHeight: Integer;
  ACommand: TUiCommand; AClick: TNotifyEvent): TButton;
begin
  Result := TButton.Create(AOwner);
  Result.Parent := AParent;
  Result.Caption := ACaption;
  Result.Font.Name := 'MS Sans Serif';
  Result.Font.Height := -10;
  Result.Tag := Ord(ACommand);
  Result.TabStop := False;
  Result.OnClick := AClick;
  Result.SetBounds(X, Y, AWidth, AHeight);
end;

function NewLabel(AOwner: TComponent; AParent: TWinControl;
  const ACaption: string; X, Y, AWidth, AHeight: Integer;
  AAlignment: TAlignment = taLeftJustify): TLabel;
begin
  Result := TLabel.Create(AOwner);
  Result.Parent := AParent;
  Result.Caption := ACaption;
  Result.Font.Name := 'MS Sans Serif';
  Result.Font.Height := -10;
  Result.Alignment := AAlignment;
  Result.SetBounds(X, Y, AWidth, AHeight);
end;

function NewMeterReadout(AOwner: TComponent; AParent: TWinControl;
  X, Y, AWidth, AHeight: Integer): TEdit;
begin
  Result := TEdit.Create(AOwner);
  Result.Parent := AParent;
  Result.ReadOnly := True;
  Result.TabStop := False;
  Result.Color := clBlack;
  Result.Font.Name := 'Arial';
  Result.Font.Height := -21;
  Result.Font.Color := clWindowText;
  Result.Font.Style := [fsBold, fsItalic];
  Result.Alignment := taCenter;
  Result.SetBounds(X, Y, AWidth, AHeight);
end;

function NewValueReadout(AOwner: TComponent; AParent: TWinControl;
  X, Y, AWidth, AHeight: Integer): TEdit;
begin
  Result := TEdit.Create(AOwner);
  Result.Parent := AParent;
  Result.ReadOnly := True;
  Result.TabStop := False;
  Result.Color := clBlack;
  Result.Font.Name := 'MS Sans Serif';
  Result.Font.Height := -12;
  Result.Font.Color := clLime;
  Result.Alignment := taCenter;
  Result.SetBounds(X, Y, AWidth, AHeight);
end;

procedure EnlargeSidebarText(AParent: TWinControl);
var
  I: Integer;
  Control: TControl;
begin
  for I := 0 to AParent.ControlCount - 1 do
  begin
    Control := AParent.Controls[I];
    if (Control is TLabel) or (Control is TButton) or
      (Control is TGroupBox) or (Control is TRadioGroup) then
      TControlAccess(Control).Font.Height := -12;
    if Control is TWinControl then
      EnlargeSidebarText(TWinControl(Control));
  end;
end;

procedure TVSoundRemakeForm.CaptureLayout(AParent: TWinControl);
var
  Control: TControl;
  I, ItemIndex: Integer;
begin
  for I := 0 to AParent.ControlCount - 1 do
  begin
    Control := AParent.Controls[I];
    if Control.Align = alNone then
    begin
      ItemIndex := Length(FLayoutItems);
      SetLength(FLayoutItems, ItemIndex + 1);
      FLayoutItems[ItemIndex].Control := Control;
      FLayoutItems[ItemIndex].Bounds := Control.BoundsRect;
      FLayoutItems[ItemIndex].FontHeight :=
        TControlAccess(Control).Font.Height;
    end;
    if Control is TWinControl then
      CaptureLayout(TWinControl(Control));
  end;
end;

function TVSoundRemakeForm.ScaleX(Value: Integer): Integer;
begin
  Result := Round(Value * FLayoutScaleX);
end;

function TVSoundRemakeForm.ScaleY(Value: Integer): Integer;
begin
  Result := Round(Value * FLayoutScaleY);
end;

procedure TVSoundRemakeForm.CreateParams(var Params: TCreateParams);
begin
  inherited CreateParams(Params);
  // The audio host is intentionally invisible.  Make the real frontend an
  // app window so Windows gives it its own normal taskbar button.
  Params.ExStyle := (Params.ExStyle or WS_EX_APPWINDOW) and not WS_EX_TOOLWINDOW;
end;

procedure TVSoundRemakeForm.ApplyDarkWindowFrame;
const
  DWMWA_USE_IMMERSIVE_DARK_MODE = 20;
  DWMWA_WINDOW_CORNER_PREFERENCE = 33;
  DWMWA_BORDER_COLOR = 34;
  DWMWCP_ROUND = 2;
var
  DarkMode: BOOL;
  CornerPreference: Integer;
  BorderColor: COLORREF;
begin
  if not HandleAllocated then
    Exit;
  // VCL styles paint only the client area. Tell DWM to paint the remaining
  // resize border and rounded corners with the same dark base instead of the
  // white light-mode frame that leaked through as edge pixels.
  DarkMode := True;
  CornerPreference := DWMWCP_ROUND;
  BorderColor := RGB(32, 32, 32);
  DwmSetWindowAttribute(Handle, DWMWA_USE_IMMERSIVE_DARK_MODE, @DarkMode,
    SizeOf(DarkMode));
  DwmSetWindowAttribute(Handle, DWMWA_WINDOW_CORNER_PREFERENCE,
    @CornerPreference, SizeOf(CornerPreference));
  DwmSetWindowAttribute(Handle, DWMWA_BORDER_COLOR, @BorderColor,
    SizeOf(BorderColor));
end;

procedure TVSoundRemakeForm.ApplyLayout;
var
  FontScale: Double;
  I: Integer;
  Item: TControlLayout;
begin
  if not FLayoutReady then
    Exit;

  FLayoutScaleX := ClientWidth / BaseClientWidth;
  FLayoutScaleY := ClientHeight / BaseClientHeight;
  FontScale := Min(FLayoutScaleX, FLayoutScaleY);
  Font.Height := Round(FBaseFormFontHeight * FontScale);

  for I := Low(FLayoutItems) to High(FLayoutItems) do
  begin
    Item := FLayoutItems[I];
    if Assigned(Item.Control) and (Item.Control.Align = alNone) then
    begin
      Item.Control.SetBounds(
        ScaleX(Item.Bounds.Left),
        ScaleY(Item.Bounds.Top),
        Max(1, ScaleX(Item.Bounds.Width)),
        Max(1, ScaleY(Item.Bounds.Height)));
      if Item.FontHeight <> 0 then
        TControlAccess(Item.Control).Font.Height :=
          Round(Item.FontHeight * FontScale);
    end;
  end;
  RefreshChart;
end;

procedure TVSoundRemakeForm.FormResize(Sender: TObject);
begin
  ApplyLayout;
end;

procedure TVSoundRemakeForm.EqModeClick(Sender: TObject);
begin
  if not (Sender is TButton) then
    Exit;
  if TButton(Sender).Tag = 0 then
  begin
    FGraphicPage.Visible := True;
    FArbitraryPage.Visible := False;
  end
  else
  begin
    FGraphicPage.Visible := False;
    FArbitraryPage.Visible := True;
  end;
  FGraphicModeButton.Font.Style := [];
  FArbitraryModeButton.Font.Style := [];
  TButton(Sender).Font.Style := [fsBold];
  FGraphicModeButton.Default := Sender = FGraphicModeButton;
  FArbitraryModeButton.Default := Sender = FArbitraryModeButton;
  RefreshChart;
  RefreshArbitraryGraph;
  QueueEngineSync;
end;

procedure GradientRect(ACanvas: TCanvas; const R: TRect;
  ATopColor, ABottomColor: TColor);
var
  I, AHeight: Integer;
  TopRed, TopGreen, TopBlue, BottomRed, BottomGreen, BottomBlue: Byte;
begin
  TopRed := GetRValue(ATopColor);
  TopGreen := GetGValue(ATopColor);
  TopBlue := GetBValue(ATopColor);
  BottomRed := GetRValue(ABottomColor);
  BottomGreen := GetGValue(ABottomColor);
  BottomBlue := GetBValue(ABottomColor);
  AHeight := Max(1, R.Height - 1);
  for I := 0 to R.Height - 1 do
  begin
    ACanvas.Pen.Color := RGB(
      TopRed + (BottomRed - TopRed) * I div AHeight,
      TopGreen + (BottomGreen - TopGreen) * I div AHeight,
      TopBlue + (BottomBlue - TopBlue) * I div AHeight);
    ACanvas.MoveTo(R.Left, R.Top + I);
    ACanvas.LineTo(R.Right, R.Top + I);
  end;
end;

{ TVSoundRemakeForm }

constructor TVSoundRemakeForm.Create(AOwner: TComponent);
var
  WindowDpi: Cardinal;
begin
  inherited CreateNew(AOwner);
  Scaled := False;
  Caption := 'Termite';
  BorderStyle := bsSizeable;
  BorderIcons := [biSystemMenu, biMinimize, biMaximize];
  Position := poScreenCenter;
  DoubleBuffered := True;
  ClientWidth := BaseClientWidth;
  ClientHeight := BaseClientHeight;
  // The reference skin owns the finished surface.  This is the fallback color
  // used only while VCL changes themes, so it must not flash a white system UI.
  Color := RGB(32, 32, 32);
  Font.Name := 'Tahoma';
  // Tahoma retains the period-correct character of the reference while being
  // substantially clearer than MS Sans Serif on current high-density screens.
  Font.Height := -11;
  FBaseFormFontHeight := Font.Height;
  FLayoutScaleX := 1;
  FLayoutScaleY := 1;
  OnResize := FormResize;

  FEqualizerOn := True;
  FInputGain := 0;
  FWetMix := 100;
  // The original graphic EQ resets its smoothing/Q control to 50%.
  FSmoothing := 50;
  FActiveThemeIndex := ActiveReferenceThemeIndex;
  FArbitraryFilters := TList<TArbitraryFilter>.Create;
  FBridgeClient := TTermiteBridgeClient.Create;
  FBridgeTimer := TTimer.Create(Self);
  FBridgeTimer.Enabled := False;
  FBridgeTimer.Interval := 24;
  FBridgeTimer.OnTimer := BridgeTimer;
  FBridgeStatusTimer := TTimer.Create(Self);
  FBridgeStatusTimer.Interval := 150;
  FBridgeStatusTimer.OnTimer := BridgeStatusTimer;
  FBridgeStatusTimer.Enabled := True;
  CreateMenu;
  CreateLeftPanel;

  FModePanel := TPanel.Create(Self);
  FModePanel.Parent := Self;
  FModePanel.BevelOuter := bvNone;
  FModePanel.Caption := '';
  FModePanel.ParentBackground := True;
  FModePanel.ParentColor := True;
  FModePanel.SetBounds(LeftPaneWidth + 8, 2, 230, 24);
  FGraphicModeButton := NewButton(Self, FModePanel, 'Graphic EQ', 0, 0,
    108, 23, ucNone, EqModeClick);
  FGraphicModeButton.Tag := 0;
  FArbitraryModeButton := NewButton(Self, FModePanel, 'Arbitrary EQ', 112, 0,
    112, 23, ucNone, EqModeClick);
  FArbitraryModeButton.Tag := 1;

  FGraphicPage := TPanel.Create(Self);
  // Direct form children inherit the active style's textured background.
  // Routing them through another TPanel converts that background into the
  // panel's flat client face on bitmap styles such as Glossy.
  FGraphicPage.Parent := Self;
  FGraphicPage.DoubleBuffered := True;
  FGraphicPage.BevelOuter := bvNone;
  FGraphicPage.Caption := '';
  FGraphicPage.ParentBackground := True;
  FGraphicPage.ParentColor := True;
  FGraphicPage.SetBounds(LeftPaneWidth, 28,
    BaseClientWidth - LeftPaneWidth, BaseClientHeight - 28);
  CreateGraphicPage(FGraphicPage);

  FArbitraryPage := TPanel.Create(Self);
  FArbitraryPage.Parent := Self;
  FArbitraryPage.DoubleBuffered := True;
  FArbitraryPage.BevelOuter := bvNone;
  FArbitraryPage.Caption := '';
  FArbitraryPage.ParentBackground := True;
  FArbitraryPage.ParentColor := True;
  FArbitraryPage.SetBounds(LeftPaneWidth, 28,
    BaseClientWidth - LeftPaneWidth, BaseClientHeight - 28);
  CreateArbitraryPage(FArbitraryPage);

  EqModeClick(FGraphicModeButton);
  // These are tab selectors, not ordinary momentary buttons.  Default gives
  // the selected tab the same stable visual state from the first paint onward.
  FGraphicModeButton.Default := True;
  FArbitraryModeButton.Default := False;
  ApplyThemeColors;
  RefreshControls;
  RefreshChart;
  CaptureLayout(Self);
  FLayoutReady := True;
  WindowDpi := GetDpiForWindow(Handle);
  ApplyDarkWindowFrame;
  if WindowDpi = 0 then
    WindowDpi := 96;
  ClientWidth := Round(DefaultClientWidth * WindowDpi / 96);
  ClientHeight := Round(DefaultClientHeight * WindowDpi / 96);
  Constraints.MinWidth := Width;
  Constraints.MinHeight := Height;
  ApplyLayout;
  QueueEngineSync;
end;

destructor TVSoundRemakeForm.Destroy;
begin
  FBridgeStatusTimer.Enabled := False;
  FBridgeClient.Free;
  FArbitraryFilters.Free;
  inherited;
end;

procedure TVSoundRemakeForm.CreateMenu;
var
  MainMenu: TMainMenu;
  ThemeItem: TMenuItem;
  I: Integer;

  procedure AddMenuItem(AParent: TMenuItem; const ACaption: string;
    ACommand: TUiCommand);
  var
    Item: TMenuItem;
  begin
    Item := TMenuItem.Create(MainMenu);
    Item.Caption := ACaption;
    Item.Tag := Ord(ACommand);
    Item.OnClick := ButtonClick;
    AParent.Add(Item);
  end;

  procedure AddSeparator(AParent: TMenuItem);
  begin
    ThemeItem := TMenuItem.Create(MainMenu);
    ThemeItem.Caption := '-';
    AParent.Add(ThemeItem);
  end;
begin
  MainMenu := TMainMenu.Create(Self);
  Menu := MainMenu;
  MainMenu.Items.Add(TMenuItem.Create(MainMenu));
  MainMenu.Items.Add(TMenuItem.Create(MainMenu));
  MainMenu.Items.Add(TMenuItem.Create(MainMenu));
  MainMenu.Items.Add(TMenuItem.Create(MainMenu));
  MainMenu.Items[0].Caption := 'File';
  MainMenu.Items[1].Caption := 'Engine';
  MainMenu.Items[2].Caption := 'Themes';
  MainMenu.Items[3].Caption := 'Help';

  AddMenuItem(MainMenu.Items[0], 'Open profile...', ucOpenProfile);
  AddMenuItem(MainMenu.Items[0], 'Save profile...', ucSaveProfile);
  AddSeparator(MainMenu.Items[0]);
  AddMenuItem(MainMenu.Items[0], 'Exit', ucExit);

  AddMenuItem(MainMenu.Items[1], 'Sync current EQ', ucDetect);
  AddMenuItem(MainMenu.Items[1], 'Enable equalizer', ucRun);
  AddMenuItem(MainMenu.Items[1], 'Bypass equalizer', ucPause);
  AddSeparator(MainMenu.Items[1]);
  AddMenuItem(MainMenu.Items[1], 'Route apps...', ucRouteApps);

  AddMenuItem(MainMenu.Items[3], 'About Termite', ucHelpAbout);
  AddMenuItem(MainMenu.Items[3], 'Profiles', ucHelpProfiles);
  AddMenuItem(MainMenu.Items[3], 'App routing', ucHelpRouting);

  for I := Low(ReferenceThemeCaptions) to High(ReferenceThemeCaptions) do
  begin
    ThemeItem := TMenuItem.Create(MainMenu);
    ThemeItem.Caption := ReferenceThemeCaptions[I];
    ThemeItem.Tag := I;
    ThemeItem.AutoCheck := False;
    ThemeItem.RadioItem := True;
    ThemeItem.GroupIndex := 1;
    ThemeItem.Checked := I = FActiveThemeIndex;
    ThemeItem.Enabled := ReferenceThemeIsAvailable(I);
    ThemeItem.OnClick := ThemeClick;
    FThemeItems[I] := ThemeItem;
    MainMenu.Items[2].Add(ThemeItem);
  end;
end;

procedure TVSoundRemakeForm.CreateLeftPanel;
var
  LeftPanel, ControlPanel, BottomPanel: TPanel;
  EqualizerBox, BlenderBox, VolumeBox: TGroupBox;
begin
  LeftPanel := TPanel.Create(Self);
  LeftPanel.Parent := Self;
  LeftPanel.Align := alNone;
  LeftPanel.SetBounds(0, 0, LeftPaneWidth, BaseClientHeight);
  LeftPanel.BevelOuter := bvNone;
  LeftPanel.Caption := '';
  LeftPanel.ParentBackground := True;
  LeftPanel.ParentColor := True;

  NewLabel(Self, LeftPanel, 'Termite audio host:', 12, 5, 180, 16);
  FStatusLog := TRichEdit.Create(Self);
  FStatusLog.Parent := LeftPanel;
  FStatusLog.SetBounds(12, 25, 249, 176);
  FStatusLog.ReadOnly := True;
  FStatusLog.ScrollBars := ssVertical;
  FStatusLog.Color := clBlack;
  FStatusLog.Font.Name := 'MS Sans Serif';
  FStatusLog.Font.Height := -10;
  FStatusLog.Font.Color := clWhite;
  FStatusLog.Lines.Add('VCL frontend ready.');
  FStatusLog.Lines.Add('EQ: enabled');
  FStatusLog.Lines.Add('Preamp: 0 dB');
  FStatusLog.Lines.Add('Smoothing: 50');
  FStatusLog.Lines.Add('Curve: flat');
  FStatusLog.Lines.Add('');
  FStatusLog.Lines.Add('Waiting for local audio bridge...');
  FStatusLog.Lines.Add('');
  FStatusLog.Lines.Add('Termite owns audio processing.');

  ControlPanel := TPanel.Create(Self);
  // Bitmap styles only paint their texture through direct form children.
  // These two containers used to sit under LeftPanel, so they received its
  // flat panel face instead of the active theme background.
  ControlPanel.Parent := Self;
  ControlPanel.SetBounds(12, 230, 249, 103);
  ControlPanel.BevelOuter := bvLowered;
  ControlPanel.Caption := '';
  ControlPanel.ParentBackground := True;
  ControlPanel.ParentColor := True;
  NewButton(Self, ControlPanel, 'Apply EQ', 4, 7, 78, 25, ucDetect, ButtonClick).Font.Style := [fsBold];
  NewButton(Self, ControlPanel, 'Flat EQ', 85, 7, 78, 25, ucReset, ButtonClick);
  NewButton(Self, ControlPanel, 'Clear log', 166, 7, 78, 25, ucClearInfo, ButtonClick);
  NewButton(Self, ControlPanel, 'Bypass', 4, 38, 78, 25, ucPause, ButtonClick);
  NewButton(Self, ControlPanel, 'Enable', 85, 38, 78, 25, ucRun, ButtonClick);
  NewButton(Self, ControlPanel, 'Graphic', 166, 38, 78, 25, ucSleep, ButtonClick);
  NewButton(Self, ControlPanel, 'Arbitrary', 4, 69, 78, 25, ucDefaultStart, ButtonClick);
  NewButton(Self, ControlPanel, 'Route apps', 85, 69, 78, 25, ucRouteApps, ButtonClick);
  NewButton(Self, ControlPanel, 'Help', 166, 69, 78, 25, ucHelp, ButtonClick);

  BottomPanel := TPanel.Create(Self);
  BottomPanel.Parent := Self;
  BottomPanel.SetBounds(12, 339, 255, 274);
  BottomPanel.BevelOuter := bvLowered;
  BottomPanel.Caption := '';
  BottomPanel.ParentBackground := True;
  BottomPanel.ParentColor := True;

  EqualizerBox := TGroupBox.Create(Self);
  EqualizerBox.Parent := BottomPanel;
  EqualizerBox.SetBounds(18, 8, 111, 65);
  EqualizerBox.Caption := ' Equalizer: ';
  FEqualizerOffButton := TRadioButton.Create(Self);
  FEqualizerOffButton.Parent := EqualizerBox;
  FEqualizerOffButton.Caption := 'Off';
  FEqualizerOffButton.SetBounds(9, 25, 42, 19);
  FEqualizerOffButton.OnClick := EqualizerClick;
  FEqualizerOnButton := TRadioButton.Create(Self);
  FEqualizerOnButton.Parent := EqualizerBox;
  FEqualizerOnButton.Caption := 'On';
  FEqualizerOnButton.Checked := True;
  FEqualizerOnButton.SetBounds(58, 25, 42, 19);
  FEqualizerOnButton.OnClick := EqualizerClick;

  BlenderBox := NewGroup(Self, BottomPanel, ' Blender mix: ', 18, 88, 111, 177);
  NewLabel(Self, BlenderBox, 'Wet:', 10, 18, 42, 16, taCenter);
  NewLabel(Self, BlenderBox, 'Dry:', 57, 18, 42, 16, taCenter);
  FWetDisplay := NewValueReadout(Self, BlenderBox, 7, 38, 46, 24);
  FDryDisplay := NewValueReadout(Self, BlenderBox, 58, 38, 46, 24);
  NewButton(Self, BlenderBox, 'Increase mix', 5, 72, 101, 43, ucMixUp, ButtonClick).Font.Name := 'Arial';
  NewButton(Self, BlenderBox, 'Decrease mix', 5, 121, 101, 43, ucMixDown, ButtonClick).Font.Name := 'Arial';

  VolumeBox := NewGroup(Self, BottomPanel, ' Digital volume: ', 153, 8, 92, 257);
  FVolumeDisplay := NewMeterReadout(Self, VolumeBox, 6, 22, 80, 35);
  NewButton(Self, VolumeBox, 'Up', 6, 70, 80, 85, ucVolumeUp, ButtonClick).Font.Name := 'Arial';
  NewButton(Self, VolumeBox, 'Down', 6, 164, 80, 85, ucVolumeDown, ButtonClick).Font.Name := 'Arial';
  EnlargeSidebarText(LeftPanel);
  EnlargeSidebarText(ControlPanel);
  EnlargeSidebarText(BottomPanel);
end;

procedure TVSoundRemakeForm.CreateGraphicPage(APage: TWinControl);
var
  ProfilesBox, PresetsBox, SmoothingBox, ControlBox, ViewBox: TGroupBox;
  SmoothBar: TScrollBar;
begin
  FChart := TBufferedChart.Create(Self);
  FChart.Parent := APage;
  FChart.SetBounds(0, 0, 854, 280);
  FChart.Height := 280;
  FChart.Color := clBlack;
  FChart.ParentColor := False;
  FChart.OnRender := DrawGraphicChart;

  NewLabel(Self, APage, '20 dB -', 12, 302, 34, 13, taRightJustify);
  NewLabel(Self, APage, '0 dB -', 18, 371, 28, 13, taRightJustify);
  NewLabel(Self, APage, '-20 dB -', 9, 440, 37, 13, taRightJustify);
  CreateFaders(APage);

  ProfilesBox := NewGroup(Self, APage, ' Engine: ', 90, 481, 119, 90);
  NewButton(Self, ProfilesBox, 'Apply EQ', 12, 22, 96, 25, ucDetect, ButtonClick);
  NewButton(Self, ProfilesBox, 'Flat EQ', 12, 53, 96, 25, ucReset, ButtonClick);

  PresetsBox := NewGroup(Self, APage, ' Quick curve: ', 215, 481, 119, 90);
  NewButton(Self, PresetsBox, 'All zero', 12, 22, 96, 25, ucAllZero, ButtonClick);
  with TComboBox.Create(Self) do
  begin
    Parent := PresetsBox;
    Style := csDropDownList;
    Items.Add('Bass boost');
    Items.Add('Bass cut');
    Items.Add('Warmth');
    Items.Add('Vocal clarity');
    Items.Add('Presence');
    Items.Add('Treble boost');
    Items.Add('Loudness');
    ItemIndex := 0;
    OnChange := PresetChange;
    SetBounds(12, 53, 96, 21);
  end;

  SmoothingBox := NewGroup(Self, APage, ' Smoothing: ', 340, 481, 185, 90);
  NewButton(Self, SmoothingBox, 'Reset', 12, 18, 75, 25, ucResetSmoothing, ButtonClick);
  FSmoothingDisplay := NewValueReadout(Self, SmoothingBox, 143, 22, 30, 21);
  SmoothBar := TScrollBar.Create(Self);
  SmoothBar.Parent := SmoothingBox;
  SmoothBar.Kind := sbHorizontal;
  SmoothBar.Min := 0;
  SmoothBar.Max := 100;
  SmoothBar.Position := FSmoothing;
  SmoothBar.OnChange := SmoothingChange;
  SmoothBar.SetBounds(12, 49, 161, 28);

  ControlBox := NewGroup(Self, APage, ' Termite EQ: ', 531, 481, 219, 90);
  NewButton(Self, ControlBox, 'Open profile', 12, 22, 96, 25, ucOpenProfile, ButtonClick).Font.Style := [fsBold];
  NewButton(Self, ControlBox, 'Save profile', 113, 22, 96, 25, ucSaveProfile, ButtonClick);
  NewButton(Self, ControlBox, 'Equalizer on', 12, 53, 95, 25, ucEqOn, ButtonClick);
  NewButton(Self, ControlBox, 'Equalizer off', 113, 53, 96, 25, ucEqOff, ButtonClick);

  ViewBox := NewGroup(Self, APage, ' View: ', 756, 481, 98, 90);
  FGraphicShowGrid := TCheckBox.Create(Self);
  FGraphicShowGrid.Parent := ViewBox;
  FGraphicShowGrid.Caption := 'Grid';
  FGraphicShowGrid.Checked := True;
  FGraphicShowGrid.OnClick := GraphicOptionsChanged;
  FGraphicShowGrid.SetBounds(10, 22, 50, 17);
  NewButton(Self, ViewBox, 'Help', 8, 53, 82, 25, ucHelp, ButtonClick);

end;

procedure TVSoundRemakeForm.CreateArbitraryPage(APage: TWinControl);
var
  TensionBox, ScalingBox, CursorBox, ViewBox, RenderBox,
  MarkerBox, ControlBox, PresetsBox: TGroupBox;
  I: Integer;
  YLabels: array[0..8] of string;
  YLabelTops: array[0..8] of Integer;
  XLabelLefts: array[0..9] of Integer;
begin
  FArbitraryChart := TBufferedChart.Create(Self);
  FArbitraryChart.Parent := APage;
  // This is the original 800 x 400 paint surface, inside its two-pixel
  // frame.  The labels are controls outside it, just as in vSound2.
  FArbitraryChart.SetBounds(50, 0, 802, 402);
  FArbitraryChart.Color := clBlack;
  FArbitraryChart.ParentColor := False;
  FArbitraryChart.Cursor := crCross;
  FArbitraryChart.OnRender := DrawArbitraryChart;
  FArbitraryChart.OnMouseDown := ArbitraryChartMouseDown;
  FArbitraryChart.OnMouseMove := ArbitraryChartMouseMove;
  FArbitraryChart.OnMouseUp := ArbitraryChartMouseUp;

  YLabels[0] := '40 dB';  YLabels[1] := '30 dB';  YLabels[2] := '20 dB';
  YLabels[3] := '10 dB';  YLabels[4] := '0 dB';   YLabels[5] := '-10 dB';
  YLabels[6] := '-20 dB'; YLabels[7] := '-30 dB'; YLabels[8] := '-40 dB';
  YLabelTops[0] := 2; YLabelTops[1] := 45; YLabelTops[2] := 94;
  YLabelTops[3] := 145; YLabelTops[4] := 194; YLabelTops[5] := 245;
  YLabelTops[6] := 294; YLabelTops[7] := 345; YLabelTops[8] := 390;
  for I := Low(YLabels) to High(YLabels) do
    NewLabel(Self, APage, YLabels[I], 14, YLabelTops[I], 31, 13, taRightJustify);

  XLabelLefts[0] := 39;  XLabelLefts[1] := 124; XLabelLefts[2] := 214;
  XLabelLefts[3] := 301; XLabelLefts[4] := 391; XLabelLefts[5] := 480;
  XLabelLefts[6] := 570; XLabelLefts[7] := 657; XLabelLefts[8] := 752;
  XLabelLefts[9] := 818;
  for I := Low(FArbitraryXLabels) to High(FArbitraryXLabels) do
    FArbitraryXLabels[I] := NewLabel(Self, APage, '40',
      XLabelLefts[I], 410, 46, 13, taCenter);

  TensionBox := NewGroup(Self, APage, ' Curve tension: ', 308, 433, 217, 47);
  FArbitraryTension := TScrollBar.Create(Self);
  FArbitraryTension.Parent := TensionBox;
  FArbitraryTension.Kind := sbHorizontal;
  FArbitraryTension.Min := 1;
  FArbitraryTension.Max := 100;
  FArbitraryTension.PageSize := 0;
  FArbitraryTension.Position := 1;
  FArbitraryTension.SetBounds(8, 16, 130, 23);
  FArbitraryTension.OnChange := ArbitraryOptionsChanged;
  NewButton(Self, TensionBox, 'Reset', 144, 14, 66, 25,
    ucResetArbitraryTension, ButtonClick);

  ScalingBox := NewGroup(Self, APage, ' X-axis scaling: ', 531, 433, 219, 47);
  FArbitraryLogarithmic := TRadioButton.Create(Self);
  FArbitraryLogarithmic.Parent := ScalingBox;
  FArbitraryLogarithmic.Caption := 'Logarithmic';
  FArbitraryLogarithmic.Checked := True;
  FArbitraryLogarithmic.SetBounds(10, 19, 96, 18);
  FArbitraryLogarithmic.OnClick := ArbitraryOptionsChanged;
  FArbitraryLinear := TRadioButton.Create(Self);
  FArbitraryLinear.Parent := ScalingBox;
  FArbitraryLinear.Caption := 'Linear';
  FArbitraryLinear.SetBounds(112, 19, 62, 18);
  FArbitraryLinear.OnClick := ArbitraryOptionsChanged;
  NewButton(Self, APage, 'Help', 762, 438, 82, 25, ucHelp, ButtonClick);

  CursorBox := NewGroup(Self, APage, ' Cursor position: ', 3, 481, 130, 90);
  NewLabel(Self, CursorBox, 'Feq., Hz:', 8, 25, 48, 17);
  FArbitraryCursorFrequency := TEdit.Create(Self);
  FArbitraryCursorFrequency.Parent := CursorBox;
  FArbitraryCursorFrequency.ReadOnly := True;
  FArbitraryCursorFrequency.TabStop := False;
  FArbitraryCursorFrequency.Font.Name := 'MS Sans Serif';
  FArbitraryCursorFrequency.Font.Height := -11;
  FArbitraryCursorFrequency.Text := '0';
  FArbitraryCursorFrequency.SetBounds(63, 23, 60, 21);
  NewLabel(Self, CursorBox, 'Gain:', 8, 58, 40, 17);
  FArbitraryCursorGain := TEdit.Create(Self);
  FArbitraryCursorGain.Parent := CursorBox;
  FArbitraryCursorGain.ReadOnly := True;
  FArbitraryCursorGain.TabStop := False;
  FArbitraryCursorGain.Font.Name := 'MS Sans Serif';
  FArbitraryCursorGain.Font.Height := -11;
  FArbitraryCursorGain.Text := '0';
  FArbitraryCursorGain.SetBounds(63, 54, 60, 21);

  ViewBox := NewGroup(Self, APage, ' View: ', 139, 481, 163, 90);
  FArbitraryShowMarkers := TCheckBox.Create(Self);
  FArbitraryShowMarkers.Parent := ViewBox;
  FArbitraryShowMarkers.Caption := 'Markers';
  FArbitraryShowMarkers.Checked := True;
  FArbitraryShowMarkers.SetBounds(13, 24, 61, 17);
  FArbitraryShowMarkers.OnClick := ArbitraryOptionsChanged;
  FArbitraryShowGrid := TCheckBox.Create(Self);
  FArbitraryShowGrid.Parent := ViewBox;
  FArbitraryShowGrid.Caption := 'Grid';
  FArbitraryShowGrid.SetBounds(78, 24, 50, 17);
  FArbitraryShowGrid.OnClick := ArbitraryOptionsChanged;
  FArbitraryShowAxes := TCheckBox.Create(Self);
  FArbitraryShowAxes.Parent := ViewBox;
  FArbitraryShowAxes.Caption := 'Axes';
  FArbitraryShowAxes.SetBounds(13, 56, 56, 17);
  FArbitraryShowAxes.OnClick := ArbitraryOptionsChanged;
  FArbitraryShowCrosshairs := TCheckBox.Create(Self);
  FArbitraryShowCrosshairs.Parent := ViewBox;
  FArbitraryShowCrosshairs.Caption := 'Crosshairs';
  FArbitraryShowCrosshairs.SetBounds(78, 56, 75, 17);
  FArbitraryShowCrosshairs.OnClick := ArbitraryOptionsChanged;

  RenderBox := NewGroup(Self, APage, ' Rendering: ', 308, 481, 126, 90);
  FArbitrarySpline := TRadioButton.Create(Self);
  FArbitrarySpline.Parent := RenderBox;
  FArbitrarySpline.Caption := 'Spline curve';
  FArbitrarySpline.Checked := True;
  FArbitrarySpline.SetBounds(8, 17, 110, 17);
  FArbitrarySpline.OnClick := ArbitraryOptionsChanged;
  FArbitraryInterpolation := TRadioButton.Create(Self);
  FArbitraryInterpolation.Parent := RenderBox;
  FArbitraryInterpolation.Caption := 'Linear interpolation';
  FArbitraryInterpolation.SetBounds(8, 37, 110, 17);
  FArbitraryInterpolation.OnClick := ArbitraryOptionsChanged;
  FArbitraryStep := TRadioButton.Create(Self);
  FArbitraryStep.Parent := RenderBox;
  FArbitraryStep.Caption := 'Step function';
  FArbitraryStep.SetBounds(8, 57, 100, 17);
  FArbitraryStep.OnClick := ArbitraryOptionsChanged;

  MarkerBox := NewGroup(Self, APage, ' Markers: ', 440, 481, 85, 90);
  FArbitraryCircles := TRadioButton.Create(Self);
  FArbitraryCircles.Parent := MarkerBox;
  FArbitraryCircles.Caption := 'Circles';
  FArbitraryCircles.Checked := True;
  FArbitraryCircles.SetBounds(8, 23, 76, 17);
  FArbitraryCircles.OnClick := ArbitraryOptionsChanged;
  FArbitrarySquares := TRadioButton.Create(Self);
  FArbitrarySquares.Parent := MarkerBox;
  FArbitrarySquares.Caption := 'Squares';
  FArbitrarySquares.SetBounds(8, 48, 76, 17);
  FArbitrarySquares.OnClick := ArbitraryOptionsChanged;

  // The VCL frontend uses one consistent Termite control group on both EQ
  // pages.  It replaces the old separate Profiles and vsound-control groups.
  ControlBox := NewGroup(Self, APage, ' Termite EQ: ', 531, 481, 219, 90);
  NewButton(Self, ControlBox, 'Open profile', 12, 22, 95, 25, ucOpenProfile, ButtonClick).Font.Style := [fsBold];
  NewButton(Self, ControlBox, 'Save profile', 113, 22, 95, 25, ucSaveProfile, ButtonClick);
  NewButton(Self, ControlBox, 'Equalizer on', 12, 53, 95, 25, ucEqOn, ButtonClick);
  NewButton(Self, ControlBox, 'Equalizer off', 113, 53, 95, 25, ucEqOff, ButtonClick);

  PresetsBox := NewGroup(Self, APage, ' Pre-sets: ', 756, 481, 95, 90);
  NewButton(Self, PresetsBox, 'All zero', 6, 22, 82, 25, ucResetArbitrary, ButtonClick);
  FArbitraryPreset := TComboBox.Create(Self);
  FArbitraryPreset.Parent := PresetsBox;
  FArbitraryPreset.Style := csDropDownList;
  FArbitraryPreset.Items.Add('All +10 dB');
  FArbitraryPreset.Items.Add('All +20 dB');
  FArbitraryPreset.Items.Add('All +30 dB');
  FArbitraryPreset.Items.Add('All -10 dB');
  FArbitraryPreset.Items.Add('All -20 dB');
  FArbitraryPreset.Items.Add('All -30 dB');
  FArbitraryPreset.ItemIndex := 0;
  FArbitraryPreset.SetBounds(6, 53, 82, 21);
  FArbitraryPreset.OnChange := ArbitraryPresetChange;

  FArbitraryDragIndex := -1;
  FArbitraryCursorInside := False;
  SeedArbitraryEndpoints;
  UpdateArbitraryAxisLabels;
  RefreshArbitraryGraph;
end;

function TVSoundRemakeForm.ArbitraryFilterCaption(
  const Filter: TArbitraryFilter): string;
begin
  Result := Format('%s  %s  %s dB  Q %s', [
    FilterKindCaption(Filter.Kind),
    FormatFloat('0', Filter.FrequencyHz, TFormatSettings.Create('en-US')) + ' Hz',
    FormatFloat('0.0', Filter.GainDb, TFormatSettings.Create('en-US')),
    FormatFloat('0.0', Filter.Q, TFormatSettings.Create('en-US'))]);
  if not Filter.Enabled then
    Result := Result + '  (off)';
end;

procedure TVSoundRemakeForm.RefreshArbitraryControls;
var
  Filter: TArbitraryFilter;
  I, Selected: Integer;
  Settings: TFormatSettings;
begin
  FArbitraryUpdating := True;
  try
    Selected := FArbitraryList.ItemIndex;
    FArbitraryList.Items.BeginUpdate;
    try
      FArbitraryList.Items.Clear;
      for I := 0 to FArbitraryFilters.Count - 1 do
        FArbitraryList.Items.Add(ArbitraryFilterCaption(FArbitraryFilters[I]));
    finally
      FArbitraryList.Items.EndUpdate;
    end;
    if FArbitraryFilters.Count = 0 then
      Selected := -1
    else if Selected < 0 then
      Selected := 0
    else
      Selected := Min(Selected, FArbitraryFilters.Count - 1);
    FArbitraryList.ItemIndex := Selected;

    FArbitraryType.Enabled := Selected >= 0;
    FArbitraryFrequency.Enabled := Selected >= 0;
    FArbitraryGain.Enabled := Selected >= 0;
    FArbitraryQ.Enabled := Selected >= 0;
    FArbitraryEnabled.Enabled := Selected >= 0;
    if Selected >= 0 then
    begin
      Filter := FArbitraryFilters[Selected];
      Settings := TFormatSettings.Create('en-US');
      FArbitraryType.ItemIndex := Ord(Filter.Kind);
      FArbitraryFrequency.Text := FloatToStrF(Filter.FrequencyHz, ffFixed, 8, 1, Settings);
      FArbitraryGain.Text := FloatToStrF(Filter.GainDb, ffFixed, 8, 1, Settings);
      FArbitraryQ.Text := FloatToStrF(Filter.Q, ffFixed, 8, 2, Settings);
      FArbitraryEnabled.Checked := Filter.Enabled;
    end
    else
    begin
      FArbitraryType.ItemIndex := -1;
      FArbitraryFrequency.Clear;
      FArbitraryGain.Clear;
      FArbitraryQ.Clear;
      FArbitraryEnabled.Checked := False;
    end;
  finally
    FArbitraryUpdating := False;
  end;
  RefreshArbitraryGraph;
end;

procedure TVSoundRemakeForm.AddArbitraryFilter;
var
  Filter: TArbitraryFilter;
begin
  Filter.Kind := afPeak;
  Filter.FrequencyHz := 1000;
  Filter.GainDb := 0;
  Filter.Q := 1;
  Filter.Enabled := True;
  FArbitraryFilters.Add(Filter);
end;

procedure TVSoundRemakeForm.SeedArbitraryEndpoints(const GainDb: Double);
var
  Filter: TArbitraryFilter;
begin
  // The original editor always retains these two end markers.  "All zero"
  // deletes only user markers, so an empty response still has a defined
  // start and end over the full 40 Hz..18 kHz hardware range.
  FArbitraryFilters.Clear;
  Filter.Kind := afPeak;
  Filter.GainDb := ClampDouble(GainDb, -40, 40);
  Filter.Q := 1;
  Filter.Enabled := True;
  Filter.FrequencyHz := ArbitraryMinimumFrequency;
  FArbitraryFilters.Add(Filter);
  Filter.FrequencyHz := ArbitraryMaximumFrequency;
  FArbitraryFilters.Add(Filter);
end;

procedure TVSoundRemakeForm.SetArbitraryFlat(const GainDb: Double);
begin
  SeedArbitraryEndpoints(GainDb);
  FArbitraryDragIndex := -1;
  RefreshArbitraryGraph;
  QueueEngineSync;
end;

procedure TVSoundRemakeForm.RemoveArbitraryFilter;
var
  Selected: Integer;
begin
  Selected := FArbitraryList.ItemIndex;
  if Selected < 0 then
    Exit;
  FArbitraryFilters.Delete(Selected);
  if Selected >= FArbitraryFilters.Count then
    Selected := FArbitraryFilters.Count - 1;
  FArbitraryList.ItemIndex := Selected;
  RefreshArbitraryControls;
  AddStatus('Arbitrary EQ filter removed.');
end;

procedure TVSoundRemakeForm.ResetArbitraryFilters;
begin
  SetArbitraryFlat(0);
  AddStatus('Arbitrary EQ markers reset to a flat 0 dB response.');
end;

function TVSoundRemakeForm.ArbitraryResponseAt(
  const FrequencyHz: Double): Double;
var
  LeftPoint, RightPoint: Integer;
  Position, LeftPosition, RightPosition, Ratio, Tension: Double;
  PreviousGain, NextGain, LeftTangent, RightTangent, Square, Cube: Double;
begin
  SortArbitraryFilters;
  if FArbitraryFilters.Count = 0 then
    Exit(0);

  if FArbitraryLogarithmic.Checked then
    Position := Ln(ClampDouble(FrequencyHz, ArbitraryMinimumFrequency,
      ArbitraryMaximumFrequency))
  else
    Position := ClampDouble(FrequencyHz, ArbitraryMinimumFrequency,
      ArbitraryMaximumFrequency);
  if FArbitraryLogarithmic.Checked then
  begin
    LeftPosition := Ln(FArbitraryFilters[0].FrequencyHz);
    RightPosition := Ln(FArbitraryFilters[FArbitraryFilters.Count - 1].FrequencyHz);
  end
  else
  begin
    LeftPosition := FArbitraryFilters[0].FrequencyHz;
    RightPosition := FArbitraryFilters[FArbitraryFilters.Count - 1].FrequencyHz;
  end;
  if Position <= LeftPosition then
    Exit(FArbitraryFilters[0].GainDb);
  if Position >= RightPosition then
    Exit(FArbitraryFilters[FArbitraryFilters.Count - 1].GainDb);

  LeftPoint := 0;
  for RightPoint := 1 to FArbitraryFilters.Count - 1 do
  begin
    if FArbitraryLogarithmic.Checked then
      RightPosition := Ln(FArbitraryFilters[RightPoint].FrequencyHz)
    else
      RightPosition := FArbitraryFilters[RightPoint].FrequencyHz;
    if Position <= RightPosition then
      Break;
    LeftPoint := RightPoint;
  end;
  if FArbitraryLogarithmic.Checked then
    LeftPosition := Ln(FArbitraryFilters[LeftPoint].FrequencyHz)
  else
    LeftPosition := FArbitraryFilters[LeftPoint].FrequencyHz;
  Ratio := (Position - LeftPosition) / Max(0.0001, RightPosition - LeftPosition);
  if FArbitraryStep.Checked then
    Ratio := 0
  else if FArbitrarySpline.Checked then
  begin
    // Cardinal cubic spline.  This same math lives in the C++ bridge so a
    // marker curve looks and sounds identical instead of merely looking
    // smooth in the VCL window.
    Tension := FArbitraryTension.Position / 100;
    PreviousGain := FArbitraryFilters[Max(0, LeftPoint - 1)].GainDb;
    NextGain := FArbitraryFilters[Min(FArbitraryFilters.Count - 1,
      RightPoint + 1)].GainDb;
    LeftTangent := (FArbitraryFilters[RightPoint].GainDb - PreviousGain) *
      (1 - Tension) * 0.5;
    RightTangent := (NextGain - FArbitraryFilters[LeftPoint].GainDb) *
      (1 - Tension) * 0.5;
    Square := Ratio * Ratio;
    Cube := Square * Ratio;
    Exit((2 * Cube - 3 * Square + 1) * FArbitraryFilters[LeftPoint].GainDb +
      (Cube - 2 * Square + Ratio) * LeftTangent +
      (-2 * Cube + 3 * Square) * FArbitraryFilters[RightPoint].GainDb +
      (Cube - Square) * RightTangent);
  end;
  Result := FArbitraryFilters[LeftPoint].GainDb +
    (FArbitraryFilters[RightPoint].GainDb -
      FArbitraryFilters[LeftPoint].GainDb) * Ratio;
end;

procedure TVSoundRemakeForm.ArbitrarySelectionChanged(Sender: TObject);
begin
  if not FArbitraryUpdating then
    RefreshArbitraryControls;
end;

procedure TVSoundRemakeForm.ArbitraryControlChanged(Sender: TObject);
var
  Filter: TArbitraryFilter;
  Selected: Integer;
  Value: Double;
begin
  if FArbitraryUpdating then
    Exit;
  Selected := FArbitraryList.ItemIndex;
  if (Selected < 0) or (Selected >= FArbitraryFilters.Count) then
    Exit;

  Filter := FArbitraryFilters[Selected];
  if FArbitraryType.ItemIndex >= 0 then
    Filter.Kind := TArbitraryFilterKind(FArbitraryType.ItemIndex);
  if TryReadFloat(FArbitraryFrequency.Text, Value) then
    Filter.FrequencyHz := ClampDouble(Value, 20, 20000);
  if TryReadFloat(FArbitraryGain.Text, Value) then
    Filter.GainDb := ClampDouble(Value, -24, 24);
  if TryReadFloat(FArbitraryQ.Text, Value) then
    Filter.Q := ClampDouble(Value, 0.1, 10);
  Filter.Enabled := FArbitraryEnabled.Checked;
  FArbitraryFilters[Selected] := Filter;
  FArbitraryList.Items[Selected] := ArbitraryFilterCaption(Filter);
  RefreshArbitraryGraph;
end;

procedure TVSoundRemakeForm.RefreshArbitraryGraph;
begin
  UpdateArbitraryAxisLabels;
  if Assigned(FArbitraryChart) then
    FArbitraryChart.Repaint;
end;

procedure TVSoundRemakeForm.UpdateArbitraryAxisLabels;
var
  I: Integer;
  FrequencyHz: Double;
  Settings: TFormatSettings;
begin
  if not Assigned(FArbitraryXLabels[0]) then
    Exit;
  Settings := TFormatSettings.Create('en-US');
  for I := Low(FArbitraryXLabels) to High(FArbitraryXLabels) do
  begin
    if FArbitraryLogarithmic.Checked then
      FrequencyHz := ArbitraryMinimumFrequency * Power(
        ArbitraryMaximumFrequency / ArbitraryMinimumFrequency, I / 9)
    else
      FrequencyHz := ArbitraryMinimumFrequency +
        (ArbitraryMaximumFrequency - ArbitraryMinimumFrequency) * I / 9;
    // Human units are easier to scan and avoid Delphi exponent-format parsing
    // entirely, so a refresh cannot turn a 40 Hz tick into 4.00E+11.
    if FrequencyHz >= 1000 then
      FArbitraryXLabels[I].Caption :=
        FormatFloat('0.#', FrequencyHz / 1000, Settings) + 'k'
    else
      FArbitraryXLabels[I].Caption := FormatFloat('0', FrequencyHz, Settings);
  end;
end;

function TVSoundRemakeForm.ArbitraryPlotRect: TRect;
begin
  Result := Rect(1, 1, FArbitraryChart.ClientWidth - 1,
    FArbitraryChart.ClientHeight - 1);
end;

function TVSoundRemakeForm.ArbitraryXForFrequency(
  const FrequencyHz: Double): Integer;
var
  Plot: TRect;
  Fraction: Double;
begin
  Plot := ArbitraryPlotRect;
  if FArbitraryLogarithmic.Checked then
    Fraction := Ln(ClampDouble(FrequencyHz, ArbitraryMinimumFrequency,
      ArbitraryMaximumFrequency) / ArbitraryMinimumFrequency) /
      Ln(ArbitraryMaximumFrequency / ArbitraryMinimumFrequency)
  else
    Fraction := (ClampDouble(FrequencyHz, ArbitraryMinimumFrequency,
      ArbitraryMaximumFrequency) - ArbitraryMinimumFrequency) /
      (ArbitraryMaximumFrequency - ArbitraryMinimumFrequency);
  Result := Plot.Left + Round(Fraction * Plot.Width);
end;

function TVSoundRemakeForm.ArbitraryFrequencyForX(const X: Integer): Double;
var
  Plot: TRect;
  Fraction: Double;
begin
  Plot := ArbitraryPlotRect;
  Fraction := ClampDouble((X - Plot.Left) / Max(1, Plot.Width), 0, 1);
  if FArbitraryLogarithmic.Checked then
    Result := ArbitraryMinimumFrequency * Power(
      ArbitraryMaximumFrequency / ArbitraryMinimumFrequency, Fraction)
  else
    Result := ArbitraryMinimumFrequency + Fraction *
      (ArbitraryMaximumFrequency - ArbitraryMinimumFrequency);
end;

function TVSoundRemakeForm.ArbitraryYForGain(const GainDb: Double): Integer;
var
  Plot: TRect;
begin
  Plot := ArbitraryPlotRect;
  Result := Plot.Top + Round((40 - ClampDouble(GainDb, -40, 40)) *
    Plot.Height / 80);
end;

function TVSoundRemakeForm.ArbitraryGainForY(const Y: Integer): Double;
var
  Plot: TRect;
begin
  Plot := ArbitraryPlotRect;
  Result := ClampDouble(40 - (Y - Plot.Top) * 80 / Max(1, Plot.Height), -40, 40);
end;

procedure TVSoundRemakeForm.SortArbitraryFilters;
var
  I, J: Integer;
  Temp: TArbitraryFilter;
begin
  for I := 0 to FArbitraryFilters.Count - 2 do
    for J := I + 1 to FArbitraryFilters.Count - 1 do
      if FArbitraryFilters[J].FrequencyHz < FArbitraryFilters[I].FrequencyHz then
      begin
        Temp := FArbitraryFilters[I];
        FArbitraryFilters[I] := FArbitraryFilters[J];
        FArbitraryFilters[J] := Temp;
      end;
end;

procedure TVSoundRemakeForm.UpdateArbitraryCursor;
var
  FrequencyHz, GainDb: Double;
  Settings: TFormatSettings;
begin
  if not Assigned(FArbitraryCursorFrequency) or
    not Assigned(FArbitraryCursorGain) then
    Exit;
  if not FArbitraryCursorInside then
    Exit;
  Settings := TFormatSettings.Create('en-US');
  FrequencyHz := ArbitraryFrequencyForX(FArbitraryCursorX);
  GainDb := ArbitraryResponseAt(FrequencyHz);
  FArbitraryCursorFrequency.Text := FloatToStrF(FrequencyHz, ffFixed, 8, 0, Settings);
  FArbitraryCursorGain.Text := FloatToStrF(GainDb, ffFixed, 8, 1, Settings);
end;

procedure TVSoundRemakeForm.ArbitraryOptionsChanged(Sender: TObject);
begin
  if FProfileLoading then
    Exit;
  RefreshArbitraryGraph;
  QueueEngineSync;
end;

procedure TVSoundRemakeForm.ArbitraryPresetChange(Sender: TObject);
const
  PresetGain: array[0..5] of Double = (10, 20, 30, -10, -20, -30);
begin
  if Assigned(FArbitraryPreset) and
    (FArbitraryPreset.ItemIndex >= Low(PresetGain)) and
    (FArbitraryPreset.ItemIndex <= High(PresetGain)) then
    SetArbitraryFlat(PresetGain[FArbitraryPreset.ItemIndex]);
end;

procedure TVSoundRemakeForm.ArbitraryChartMouseDown(Sender: TObject;
  Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
var
  I, Nearest, Distance, BestDistance: Integer;
  Plot: TRect;
  Filter: TArbitraryFilter;
begin
  Plot := ArbitraryPlotRect;
  if not PtInRect(Plot, Point(X, Y)) then
    Exit;

  Nearest := -1;
  BestDistance := 14;
  for I := 0 to FArbitraryFilters.Count - 1 do
  begin
    Distance := Abs(ArbitraryXForFrequency(FArbitraryFilters[I].FrequencyHz) - X) +
      Abs(ArbitraryYForGain(FArbitraryFilters[I].GainDb) - Y);
    if Distance < BestDistance then
    begin
      BestDistance := Distance;
      Nearest := I;
    end;
  end;

  if Button = mbRight then
  begin
    // The permanent start/end points define the hardware range.  Only user
    // markers are removable, exactly as the original "All zero" behaviour.
    if (Nearest > 0) and (Nearest < FArbitraryFilters.Count - 1) then
    begin
      FArbitraryFilters.Delete(Nearest);
      FArbitraryDragIndex := -1;
      RefreshArbitraryGraph;
      QueueEngineSync;
    end;
    Exit;
  end;
  if Button <> mbLeft then
    Exit;

  if Nearest < 0 then
  begin
    Filter.Kind := afPeak;
    Filter.FrequencyHz := ArbitraryFrequencyForX(X);
    Filter.GainDb := ArbitraryGainForY(Y);
    Filter.Q := 1;
    Filter.Enabled := True;
    FArbitraryFilters.Add(Filter);
    SortArbitraryFilters;
    for I := 0 to FArbitraryFilters.Count - 1 do
      if Abs(FArbitraryFilters[I].FrequencyHz - Filter.FrequencyHz) < 0.01 then
        Nearest := I;
  end;
  if (Nearest > 0) and (Nearest < FArbitraryFilters.Count - 1) then
    FArbitraryDragIndex := Nearest
  else
    FArbitraryDragIndex := -1;
  RefreshArbitraryGraph;
  QueueEngineSync;
end;

procedure TVSoundRemakeForm.ArbitraryChartMouseMove(Sender: TObject;
  Shift: TShiftState; X, Y: Integer);
var
  Filter: TArbitraryFilter;
  I, Distance, Nearest, BestDistance: Integer;
  Plot: TRect;
begin
  Plot := ArbitraryPlotRect;
  FArbitraryCursorInside := PtInRect(Plot, Point(X, Y));
  if FArbitraryCursorInside then
  begin
    FArbitraryCursorX := Max(Plot.Left, Min(Plot.Right, X));
    FArbitraryCursorY := ArbitraryYForGain(
      ArbitraryResponseAt(ArbitraryFrequencyForX(FArbitraryCursorX)));
    UpdateArbitraryCursor;
  end;

  Nearest := -1;
  BestDistance := 14;
  if FArbitraryCursorInside then
    for I := 1 to FArbitraryFilters.Count - 2 do
    begin
      Distance := Abs(ArbitraryXForFrequency(FArbitraryFilters[I].FrequencyHz) - X) +
        Abs(ArbitraryYForGain(FArbitraryFilters[I].GainDb) - Y);
      if Distance < BestDistance then
      begin
        BestDistance := Distance;
        Nearest := I;
      end;
    end;
  if FArbitraryDragIndex >= 0 then
    FArbitraryChart.Cursor := crHandPoint
  else if Nearest >= 0 then
    FArbitraryChart.Cursor := crHandPoint
  else
    FArbitraryChart.Cursor := crCross;

  if (ssLeft in Shift) and (FArbitraryDragIndex >= 0) and
    (FArbitraryDragIndex < FArbitraryFilters.Count) then
  begin
    Filter := FArbitraryFilters[FArbitraryDragIndex];
    Filter.FrequencyHz := ArbitraryFrequencyForX(X);
    Filter.GainDb := ArbitraryGainForY(Y);
    FArbitraryFilters[FArbitraryDragIndex] := Filter;
    SortArbitraryFilters;
    // Sorting makes the response single-valued if a marker moves across a
    // neighbour.  Re-acquire it so the drag remains attached to that marker.
    for I := 1 to FArbitraryFilters.Count - 2 do
      if (Abs(FArbitraryFilters[I].FrequencyHz - Filter.FrequencyHz) < 0.001) and
        (Abs(FArbitraryFilters[I].GainDb - Filter.GainDb) < 0.001) then
      begin
        FArbitraryDragIndex := I;
        Break;
      end;
    FArbitraryCursorY := ArbitraryYForGain(
      ArbitraryResponseAt(ArbitraryFrequencyForX(FArbitraryCursorX)));
    QueueEngineSync;
  end;
  UpdateArbitraryCursor;
  RefreshArbitraryGraph;
end;

procedure TVSoundRemakeForm.ArbitraryChartMouseUp(Sender: TObject;
  Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
begin
  if Button = mbLeft then
  begin
    FArbitraryDragIndex := -1;
    UpdateArbitraryCursor;
    RefreshArbitraryGraph;
    QueueEngineSync;
  end;
end;

procedure TVSoundRemakeForm.CreateFaders(AParent: TWinControl);
var
  I, X: Integer;
  LabelControl: TLabel;
begin
  for I := Low(FFaders) to High(FFaders) do
  begin
    X := 52 + I * 40;
    LabelControl := NewLabel(Self, AParent, Frequencies[I], X, 286, 30, 13, taCenter);

    FFaders[I] := TScrollBar.Create(Self);
    FFaders[I].Parent := AParent;
    FFaders[I].Tag := I;
    FFaders[I].Kind := sbVertical;
    FFaders[I].LargeChange := 4;
    FFaders[I].Min := -20;
    FFaders[I].Max := 20;
    FFaders[I].Position := -InitialBands[I];
    FFaders[I].OnChange := FaderChange;
    FFaders[I].SetBounds(X, 310, 30, 130);

    FFaderDisplays[I] := NewValueReadout(Self, AParent, X, 450, 30, 21);
  end;
end;

procedure TVSoundRemakeForm.ButtonClick(Sender: TObject);
var
  Command: TUiCommand;
  I: Integer;
begin
  if Sender is TMenuItem then
    Command := TUiCommand(TMenuItem(Sender).Tag)
  else if Sender is TButton then
    Command := TUiCommand(TButton(Sender).Tag)
  else
    Exit;
  case Command of
    ucDetect: AddStatus('EQ update sent to the Termite audio engine.');
    ucReset:
      begin
        for I := Low(FFaders) to High(FFaders) do
          FFaders[I].Position := 0;
        FInputGain := 0;
        FSmoothing := 50;
        AddStatus('Graphic EQ returned to a flat 0 dB curve.');
      end;
    ucPause:
      begin
        FEqualizerOn := False;
        AddStatus('EQ bypass enabled.');
      end;
    ucRun:
      begin
        FEqualizerOn := True;
        AddStatus('EQ processing enabled.');
      end;
    ucClearInfo: FStatusLog.Clear;
    ucSleep: FGraphicModeButton.Click;
    ucDefaultStart: FArbitraryModeButton.Click;
    ucRouteApps: ShowRoutingDialog;
    ucExit:
      begin
        Close;
        Exit;
      end;
    ucHelp: AddStatus('The VCL frontend sends EQ changes to the local Termite audio host.');
    ucHelpAbout: AddStatus('Termite is a local audio host. The VCL window controls its EQ engine.');
    ucHelpProfiles: AddStatus('Profiles are .tsf files. Open restores an EQ; Save writes the current EQ.');
    ucHelpRouting: AddStatus('Route apps sends an active app to CABLE Input. Untick it to restore its output.');
    ucMixUp: FWetMix := Min(100, FWetMix + 5);
    ucMixDown: FWetMix := Max(0, FWetMix - 5);
    ucVolumeUp: FInputGain := Min(20, FInputGain + 1);
    ucVolumeDown: FInputGain := Max(-40, FInputGain - 1);
    ucAllZero:
      for I := Low(FFaders) to High(FFaders) do
        FFaders[I].Position := 0;
    ucResetSmoothing: FSmoothing := 50;
    ucDownload: AddStatus('EQ update sent to the Termite audio engine.');
    ucEqOn: FEqualizerOn := True;
    ucEqOff: FEqualizerOn := False;
    ucOpenProfile: OpenProfile;
    ucSaveProfile: SaveProfile;
    ucAddArbitrary:
      begin
        AddArbitraryFilter;
        FArbitraryList.ItemIndex := FArbitraryFilters.Count - 1;
        RefreshArbitraryControls;
        AddStatus('Arbitrary EQ filter added.');
      end;
    ucRemoveArbitrary: RemoveArbitraryFilter;
    ucResetArbitrary: ResetArbitraryFilters;
    ucResetArbitraryTension:
      begin
        FArbitraryTension.Position := 1;
        ArbitraryOptionsChanged(FArbitraryTension);
      end;
  end;
  RefreshControls;
  RefreshChart;
  QueueEngineSync;
  // The reference skin treats focused buttons as permanently lit.  These are
  // instant actions, not keyboard fields, so release focus when the action is
  // complete instead of leaving the last clicked button highlighted.
  if Sender is TButton then
    ActiveControl := nil;
end;

procedure TVSoundRemakeForm.OpenProfile;
var
  Dialog: TOpenDialog;
begin
  Dialog := TOpenDialog.Create(Self);
  try
    Dialog.Title := 'Open Termite sound file';
    Dialog.Filter := 'Termite sound file (*.tsf)|*.tsf|' +
      'Legacy Termite EQ profile (*.termiteeq)|*.termiteeq|All files (*.*)|*.*';
    Dialog.FilterIndex := 1;
    Dialog.Options := Dialog.Options + [ofFileMustExist, ofPathMustExist];
    if Dialog.Execute then
      LoadProfileFromFile(Dialog.FileName);
  finally
    Dialog.Free;
  end;
end;

procedure TVSoundRemakeForm.SaveProfile;
var
  Dialog: TSaveDialog;
  Root, Profile, Effects, VclUi, Band, Marker: TJSONObject;
  Bands, Markers: TJSONArray;
  I: Integer;
  Encoding: TUTF8Encoding;
begin
  Dialog := TSaveDialog.Create(Self);
  try
    try
      Dialog.Title := 'Save Termite sound file';
    Dialog.Filter := 'Termite sound file (*.tsf)|*.tsf|All files (*.*)|*.*';
    Dialog.FilterIndex := 1;
    Dialog.DefaultExt := 'tsf';
    Dialog.FileName := 'Custom profile.tsf';
    if not Dialog.Execute then
      Exit;

    Root := TJSONObject.Create;
    try
      // The host reads the standard Termite EQ profile fields.  VCL-specific
      // state lives alongside them, so the same .tsf works in both frontends.
      Root.AddPair('format', 'termite-eq-profile');
      Root.AddPair('version', TJSONNumber.Create(2));
      Profile := TJSONObject.Create;
      Root.AddPair('profile', Profile);
      AddJsonBoolean(Profile, 'enabled', FEqualizerOn);
      Profile.AddPair('preamp_db', TJSONNumber.Create(FInputGain));
      Profile.AddPair('limiter_ceiling_db', TJSONNumber.Create(-1));
      Effects := TJSONObject.Create;
      Profile.AddPair('effects', Effects);
      AddJsonBoolean(Effects, 'bass_enabled', False);
      Effects.AddPair('bass_db', TJSONNumber.Create(0));
      AddJsonBoolean(Effects, 'loudness_enabled', False);
      Effects.AddPair('loudness_amount', TJSONNumber.Create(0));
      AddJsonBoolean(Effects, 'clarity_enabled', False);
      Effects.AddPair('clarity_db', TJSONNumber.Create(0));
      AddJsonBoolean(Effects, 'stereo_enabled', False);
      Effects.AddPair('stereo_width', TJSONNumber.Create(1));
      AddJsonBoolean(Effects, 'mono', False);
      Effects.AddPair('balance', TJSONNumber.Create(0));
      Bands := TJSONArray.Create;
      Profile.AddPair('bands', Bands);
      for I := Low(FFaders) to High(FFaders) do
      begin
        Band := TJSONObject.Create;
        Bands.AddElement(Band);
        Band.AddPair('shape', TJSONNumber.Create(1));
        Band.AddPair('gain_db', TJSONNumber.Create(-FFaders[I].Position));
        Band.AddPair('q', TJSONNumber.Create(GraphicSmoothingQ));
        AddJsonBoolean(Band, 'enabled', True);
      end;

      VclUi := TJSONObject.Create;
      Root.AddPair('vcl_ui', VclUi);
      if FArbitraryPage.Visible then
        VclUi.AddPair('mode', 'arbitrary')
      else
        VclUi.AddPair('mode', 'graphic');
      VclUi.AddPair('smoothing', TJSONNumber.Create(FSmoothing));
      VclUi.AddPair('arbitrary_tension',
        TJSONNumber.Create(FArbitraryTension.Position));
      VclUi.AddPair('arbitrary_x_axis',
        TJSONNumber.Create(Ord(FArbitraryLinear.Checked)));
      VclUi.AddPair('arbitrary_interpolation', TJSONNumber.Create(
        Ord(FArbitraryStep.Checked) + 2 * Ord(FArbitrarySpline.Checked)));
      AddJsonBoolean(VclUi, 'show_markers', FArbitraryShowMarkers.Checked);
      AddJsonBoolean(VclUi, 'show_grid', FArbitraryShowGrid.Checked);
      AddJsonBoolean(VclUi, 'show_axes', FArbitraryShowAxes.Checked);
      AddJsonBoolean(VclUi, 'show_crosshairs', FArbitraryShowCrosshairs.Checked);
      VclUi.AddPair('marker_shape', TJSONNumber.Create(
        Ord(FArbitrarySquares.Checked)));
      Markers := TJSONArray.Create;
      VclUi.AddPair('arbitrary_markers', Markers);
      for I := 0 to FArbitraryFilters.Count - 1 do
      begin
        Marker := TJSONObject.Create;
        Markers.AddElement(Marker);
        Marker.AddPair('kind', TJSONNumber.Create(Ord(FArbitraryFilters[I].Kind)));
        Marker.AddPair('frequency_hz',
          TJSONNumber.Create(FArbitraryFilters[I].FrequencyHz));
        Marker.AddPair('gain_db', TJSONNumber.Create(FArbitraryFilters[I].GainDb));
        Marker.AddPair('q', TJSONNumber.Create(FArbitraryFilters[I].Q));
        AddJsonBoolean(Marker, 'enabled', FArbitraryFilters[I].Enabled);
      end;

      Encoding := TUTF8Encoding.Create(False);
      try
        TFile.WriteAllText(Dialog.FileName, Root.ToJSON, Encoding);
      finally
        Encoding.Free;
      end;
      AddStatus('Profile saved: ' + ExtractFileName(Dialog.FileName));
    finally
      Root.Free;
    end;
  except
    on E: Exception do
      AddStatus('Could not save profile: ' + E.Message);
  end;
  finally
    Dialog.Free;
  end;
end;

procedure TVSoundRemakeForm.LoadProfileFromFile(const FileName: string);
var
  Source: string;
  RootValue: TJSONValue;
  Root, Profile, VclUi, Band, Marker: TJSONObject;
  Bands, Markers: TJSONArray;
  I, Interpolation: Integer;
  Filter: TArbitraryFilter;
  Mode: string;
  Loaded: Boolean;
begin
  RootValue := nil;
  Loaded := False;
  FProfileLoading := True;
  try
    try
      Source := TFile.ReadAllText(FileName, TEncoding.UTF8);
      RootValue := TJSONObject.ParseJSONValue(Source);
      if not (RootValue is TJSONObject) then
        raise Exception.Create('The file does not contain a Termite profile.');
      Root := TJSONObject(RootValue);
      if not SameText(JsonStringValue(Root, 'format', ''), 'termite-eq-profile') then
        raise Exception.Create('The selected file is not a Termite sound file.');
      Profile := JsonObjectValue(Root, 'profile');
      Bands := JsonArrayValue(Profile, 'bands');
      if (not Assigned(Profile)) or (not Assigned(Bands)) or
        (Bands.Count <> High(FFaders) - Low(FFaders) + 1) then
        raise Exception.Create('The profile does not contain all 20 graphic bands.');

      FEqualizerOn := JsonBooleanValue(Profile, 'enabled', True);
      FInputGain := EnsureRange(Round(JsonNumberValue(Profile, 'preamp_db', 0)),
        -40, 20);
      for I := Low(FFaders) to High(FFaders) do
      begin
        if not (Bands.Items[I] is TJSONObject) then
          raise Exception.Create('A graphic band is malformed.');
        Band := TJSONObject(Bands.Items[I]);
        FFaders[I].Position := -EnsureRange(Round(JsonNumberValue(Band,
          'gain_db', 0)), -20, 20);
      end;

      VclUi := JsonObjectValue(Root, 'vcl_ui');
      if Assigned(VclUi) then
      begin
      FSmoothing := EnsureRange(Round(JsonNumberValue(VclUi, 'smoothing', 50)),
        0, 100);
      FArbitraryTension.Position := EnsureRange(Round(JsonNumberValue(VclUi,
        'arbitrary_tension', 1)), FArbitraryTension.Min, FArbitraryTension.Max);
      FArbitraryLinear.Checked := JsonNumberValue(VclUi, 'arbitrary_x_axis', 0) <> 0;
      FArbitraryLogarithmic.Checked := not FArbitraryLinear.Checked;
      Interpolation := EnsureRange(Round(JsonNumberValue(VclUi,
        'arbitrary_interpolation', 2)), 0, 2);
      FArbitraryStep.Checked := Interpolation = 1;
      FArbitraryInterpolation.Checked := Interpolation = 0;
      FArbitrarySpline.Checked := Interpolation = 2;
      FArbitraryShowMarkers.Checked := JsonBooleanValue(VclUi, 'show_markers', True);
      FArbitraryShowGrid.Checked := JsonBooleanValue(VclUi, 'show_grid', True);
      FArbitraryShowAxes.Checked := JsonBooleanValue(VclUi, 'show_axes', True);
      FArbitraryShowCrosshairs.Checked := JsonBooleanValue(VclUi,
        'show_crosshairs', False);
      FArbitrarySquares.Checked := JsonNumberValue(VclUi, 'marker_shape', 0) <> 0;
      FArbitraryCircles.Checked := not FArbitrarySquares.Checked;

      Markers := JsonArrayValue(VclUi, 'arbitrary_markers');
      if Assigned(Markers) then
      begin
        FArbitraryFilters.Clear;
        for I := 0 to Markers.Count - 1 do
          if Markers.Items[I] is TJSONObject then
          begin
            Marker := TJSONObject(Markers.Items[I]);
            Filter.Kind := TArbitraryFilterKind(EnsureRange(Round(
              JsonNumberValue(Marker, 'kind', Ord(afPeak))), Ord(Low(TArbitraryFilterKind)),
              Ord(High(TArbitraryFilterKind))));
            Filter.FrequencyHz := ClampDouble(JsonNumberValue(Marker,
              'frequency_hz', ArbitraryMinimumFrequency), ArbitraryMinimumFrequency,
              ArbitraryMaximumFrequency);
            Filter.GainDb := ClampDouble(JsonNumberValue(Marker, 'gain_db', 0), -40, 40);
            Filter.Q := ClampDouble(JsonNumberValue(Marker, 'q', 1), 0.1, 20);
            Filter.Enabled := JsonBooleanValue(Marker, 'enabled', True);
            FArbitraryFilters.Add(Filter);
          end;
        if FArbitraryFilters.Count < 2 then
          SeedArbitraryEndpoints;
        SortArbitraryFilters;
      end;
        Mode := JsonStringValue(VclUi, 'mode', 'graphic');
      end
      else
      begin
        Band := TJSONObject(Bands.Items[0]);
        FSmoothing := EnsureRange(Round((8 - JsonNumberValue(Band, 'q', 4.5)) *
          100 / 7), 0, 100);
        Mode := 'graphic';
      end;

      // Do not synchronously repaint either buffered chart while a JSON
      // profile still owns the incoming state.  A VCL repaint can run nested
      // control notifications, and that was the source of the post-load AV.
      FGraphicPage.Visible := not SameText(Mode, 'arbitrary');
      FArbitraryPage.Visible := not FGraphicPage.Visible;
      FGraphicModeButton.Font.Style := [];
      FArbitraryModeButton.Font.Style := [];
      FGraphicModeButton.Default := FGraphicPage.Visible;
      FArbitraryModeButton.Default := FArbitraryPage.Visible;
      if FGraphicPage.Visible then
        FGraphicModeButton.Font.Style := [fsBold]
      else
        FArbitraryModeButton.Font.Style := [fsBold];
      // Keep the load guard active until the visible control readouts are
      // coherent, so their notifications cannot observe an incomplete load.
      RefreshControls;
      Loaded := True;
    except
      on E: Exception do
        AddStatus('Could not open profile: ' + E.Message);
    end;
  finally
    FProfileLoading := False;
    RootValue.Free;
  end;
  if Loaded then
  begin
    QueueEngineSync;
    AddStatus('Profile loaded: ' + ExtractFileName(FileName));
  end;
end;

procedure TVSoundRemakeForm.ShowRoutingDialog;
var
  Dialog: TRouteAppsDialog;
begin
  Dialog := TRouteAppsDialog.Create(Self);
  try
    Dialog.ShowModal;
  finally
    Dialog.Free;
  end;
  AddStatus('App routing is controlled from the checked route list.');
end;

procedure TVSoundRemakeForm.PresetChange(Sender: TObject);
const
  // Each quick curve carries the bandwidth that makes it useful.  Without
  // this, its response depends on the previous manual smoothing setting.
  PresetSmoothing: array[0..6] of Integer = (78, 78, 62, 46, 44, 58, 68);
var
  I: Integer;
  Gain: Integer;
begin
  if not (Sender is TComboBox) then
    Exit;
  if (TComboBox(Sender).ItemIndex >= Low(PresetSmoothing)) and
    (TComboBox(Sender).ItemIndex <= High(PresetSmoothing)) then
    FSmoothing := PresetSmoothing[TComboBox(Sender).ItemIndex];
  for I := Low(FFaders) to High(FFaders) do
  begin
    case TComboBox(Sender).ItemIndex of
      0: Gain := Round(7 * Exp(-I / 4));
      1: Gain := -Round(6 * Exp(-I / 4));
      2: Gain := Round(4 * Exp(-Sqr((I - 5) / 4)));
      3: Gain := Round(6 * Exp(-Sqr((I - 10) / 2.2)));
      4: Gain := Round(5 * Exp(-Sqr((I - 13) / 2.5)));
      5: Gain := Round(6 * (1 - Exp(-Max(0, I - 11) / 3)));
      6: Gain := Round(5 * Exp(-I / 4)) +
          Round(3 * (1 - Exp(-Max(0, I - 13) / 2.5)));
    else
      Gain := 0;
    end;
    FFaders[I].Position := -Gain;
  end;
  FEqualizerOn := True;
  AddStatus('Quick EQ curve applied.');
  RefreshControls;
  RefreshChart;
  QueueEngineSync;
end;

procedure TVSoundRemakeForm.EqualizerClick(Sender: TObject);
begin
  FEqualizerOn := Assigned(FEqualizerOnButton) and FEqualizerOnButton.Checked;
  RefreshControls;
  QueueEngineSync;
end;

procedure TVSoundRemakeForm.FaderChange(Sender: TObject);
begin
  if FProfileLoading then
    Exit;
  RefreshControls;
  RefreshChart;
  QueueEngineSync;
end;

procedure TVSoundRemakeForm.SmoothingChange(Sender: TObject);
begin
  if FProfileLoading then
    Exit;
  FSmoothing := TScrollBar(Sender).Position;
  RefreshControls;
  RefreshChart;
  QueueEngineSync;
end;

procedure TVSoundRemakeForm.GraphicOptionsChanged(Sender: TObject);
begin
  // The graph is buffered, so repaint it now instead of waiting for the next
  // normal paint pass.
  if Assigned(FChart) then
    FChart.Repaint;
end;

procedure TVSoundRemakeForm.QueueEngineSync;
begin
  if FProfileLoading then
    Exit;
  if not Assigned(FBridgeTimer) then
    Exit;
  FBridgeTimer.Enabled := False;
  FBridgeTimer.Enabled := True;
end;

procedure TVSoundRemakeForm.BridgeTimer(Sender: TObject);
begin
  FBridgeTimer.Enabled := False;
  SendEngineSnapshot;
end;

procedure TVSoundRemakeForm.BridgeStatusTimer(Sender: TObject);
var
  BridgeResult: TTermiteBridgeResult;
begin
  if not Assigned(FBridgeClient) or
    not FBridgeClient.TryTakeResult(BridgeResult) then
    Exit;

  case BridgeResult of
    tbrApplied:
      if not FBridgeConnected then
      begin
        FBridgeConnected := True;
        AddStatus('Connected to the Termite audio engine.');
      end;
    tbrUnavailable:
      if FBridgeConnected then
      begin
        FBridgeConnected := False;
        AddStatus('Termite audio engine is no longer available.');
      end;
    tbrRejected:
      AddStatus('Termite rejected the EQ update.');
  end;
end;

procedure TVSoundRemakeForm.SendEngineSnapshot;
var
  Snapshot: TTermiteEqSnapshot;
  I, MarkerCount: Integer;
begin
  FillChar(Snapshot, SizeOf(Snapshot), 0);
  Inc(FBridgeSequence);
  Snapshot.Magic := TermiteBridgeMagic;
  Snapshot.Version := TermiteBridgeVersion;
  Snapshot.Bytes := SizeOf(Snapshot);
  Snapshot.Sequence := FBridgeSequence;
  if FArbitraryPage.Visible then
    Snapshot.Mode := Ord(temArbitrary)
  else
    Snapshot.Mode := Ord(temGraphic);
  Snapshot.EqualizerEnabled := Ord(FEqualizerOn);
  if FArbitraryStep.Checked then
    Snapshot.Interpolation := Ord(tiStep)
  else if FArbitraryInterpolation.Checked then
    Snapshot.Interpolation := Ord(tiLinear)
  else
    Snapshot.Interpolation := Ord(tiSpline);
  if FArbitraryLinear.Checked then
    Snapshot.XAxis := Ord(txLinear)
  else
    Snapshot.XAxis := Ord(txLogarithmic);
  Snapshot.PreampDb := FInputGain;
  Snapshot.Smoothing := FSmoothing;
  Snapshot.Tension := FArbitraryTension.Position / 100;
  for I := Low(FFaders) to High(FFaders) do
    Snapshot.GraphicGains[I] := -FFaders[I].Position;
  MarkerCount := Min(FArbitraryFilters.Count, TermiteBridgeMaxMarkers);
  Snapshot.MarkerCount := MarkerCount;
  for I := 0 to MarkerCount - 1 do
  begin
    Snapshot.Markers[I].FrequencyHz := FArbitraryFilters[I].FrequencyHz;
    Snapshot.Markers[I].GainDb := FArbitraryFilters[I].GainDb;
  end;

  if Assigned(FBridgeClient) then
    FBridgeClient.Submit(Snapshot);
end;

procedure TVSoundRemakeForm.ThemeClick(Sender: TObject);
begin
  if Sender is TMenuItem then
    // VCL queues its style-change notifications.  Applying the style after the
    // popup menu closes keeps that redraw from temporarily using system UI.
    PostMessage(Handle, WM_APP + 71, WPARAM(TMenuItem(Sender).Tag), 0);
end;

procedure TVSoundRemakeForm.WMApplyTheme(var Message: TMessage);
var
  I, ThemeIndex: Integer;
begin
  ThemeIndex := Integer(Message.WParam);
  if (ThemeIndex < Low(FThemeItems)) or (ThemeIndex > High(FThemeItems)) then
    Exit;

  if ActivateReferenceStyle(ThemeIndex) then
  begin
    FActiveThemeIndex := ThemeIndex;
    SaveReferenceThemeIndex(ThemeIndex);
    for I := Low(FThemeItems) to High(FThemeItems) do
      FThemeItems[I].Checked := I = FActiveThemeIndex;
    AddStatus('Theme selected: ' + ReferenceThemeCaptions[ThemeIndex]);
    // SetStyle queues CM_CUSTOMSTYLECHANGED for every form.  Queue our redraw
    // behind that notification so the old style cannot paint one more frame.
    PostMessage(Handle, WM_APP + 72, 0, 0);
  end;
  if ThemeIndex <> FActiveThemeIndex then
    AddStatus('Theme could not be loaded: ' +
      ReferenceThemeCaptions[ThemeIndex]);
end;

procedure TVSoundRemakeForm.WMRefreshAfterTheme(var Message: TMessage);
begin
  ApplyThemeColors;
  Invalidate;
  FModePanel.Invalidate;
  FGraphicPage.Invalidate;
  FArbitraryPage.Invalidate;
  ApplyLayout;
  RefreshChart;
  RefreshArbitraryGraph;
end;

procedure TVSoundRemakeForm.RefreshControls;
var
  I: Integer;
begin
  FVolumeDisplay.Text := IntToStr(FInputGain);
  if Assigned(FWetDisplay) then
    FWetDisplay.Text := IntToStr(FWetMix);
  if Assigned(FDryDisplay) then
    FDryDisplay.Text := IntToStr(100 - FWetMix);
  FSmoothingDisplay.Text := IntToStr(FSmoothing);
  if Assigned(FEqualizerOnButton) then
    FEqualizerOnButton.Checked := FEqualizerOn;
  if Assigned(FEqualizerOffButton) then
    FEqualizerOffButton.Checked := not FEqualizerOn;
  for I := Low(FFaders) to High(FFaders) do
    FFaderDisplays[I].Text := IntToStr(-FFaders[I].Position);
end;

procedure TVSoundRemakeForm.RefreshChart;
begin
  if not Assigned(FChart) then
    Exit;
  FChart.Repaint;
end;

procedure TVSoundRemakeForm.ApplyThemeColors;
var
  Background, Foreground: TColor;
begin
  if not Assigned(FStatusLog) then
    Exit;
  // TRichEdit is a native window and does not automatically follow VCL style
  // colors.  Pull the active skin's system edit colors after every switch.
  if StyleServices.Enabled then
  begin
    Background := StyleServices.GetSystemColor(clWindow);
    Foreground := StyleServices.GetSystemColor(clWindowText);
  end
  else
  begin
    Background := clBlack;
    Foreground := clWhite;
  end;
  FStatusLog.Color := Background;
  FStatusLog.Font.Color := Foreground;
  // Existing RichEdit lines hold their own character attributes, so changing
  // Font.Color alone only affects future log entries.
  FStatusLog.SelectAll;
  FStatusLog.SelAttributes.Color := Foreground;
  FStatusLog.SelStart := Length(FStatusLog.Text);
  FStatusLog.SelLength := 0;
end;

function TVSoundRemakeForm.GraphicSmoothingQ: Double;
begin
  // Smoothing is the inverse of Q: 0 is a narrow, precise band and 100 is a
  // broad band.  It changes bandwidth without reducing the fader's gain.
  Result := 8.0 - 7.0 * ClampDouble(FSmoothing / 100, 0, 1);
end;

function TVSoundRemakeForm.GraphicResponseAt(
  const FrequencyHz: Double): Double;
var
  I: Integer;
  ResponseOmega, BandOmega, Alpha, Cosine, Amplitude, Denominator: Double;
  B0, B1, B2, A1, A2: Double;
  Cosine2, Sine, Sine2, NumeratorReal, NumeratorImaginary: Double;
  DenominatorReal, DenominatorImaginary, Numerator, Magnitude: Double;
begin
  Result := 0;
  // This reproduces the host's peaking-biquad response calculation at
  // 48 kHz.  The preview is consequently a true frequency response, not a
  // cosmetic interpolation between the twenty fader positions.
  ResponseOmega := 2 * Pi * ClampDouble(FrequencyHz, 20, 21600) / 48000;
  for I := Low(FFaders) to High(FFaders) do
  begin
    BandOmega := 2 * Pi * ClampDouble(GraphicFrequencyHz[I], 20, 21600) / 48000;
    Alpha := Sin(BandOmega) / (2 * GraphicSmoothingQ);
    Cosine := Cos(BandOmega);
    Amplitude := Power(10, (-FFaders[I].Position) / 40);
    Denominator := 1 + Alpha / Amplitude;
    B0 := (1 + Alpha * Amplitude) / Denominator;
    B1 := -2 * Cosine / Denominator;
    B2 := (1 - Alpha * Amplitude) / Denominator;
    A1 := -2 * Cosine / Denominator;
    A2 := (1 - Alpha / Amplitude) / Denominator;
    Cosine := Cos(ResponseOmega);
    Sine := Sin(ResponseOmega);
    Cosine2 := Cos(2 * ResponseOmega);
    Sine2 := Sin(2 * ResponseOmega);
    NumeratorReal := B0 + B1 * Cosine + B2 * Cosine2;
    NumeratorImaginary := -B1 * Sine - B2 * Sine2;
    DenominatorReal := 1 + A1 * Cosine + A2 * Cosine2;
    DenominatorImaginary := -A1 * Sine - A2 * Sine2;
    Numerator := Sqr(NumeratorReal) + Sqr(NumeratorImaginary);
    Magnitude := Numerator / Max(Sqr(DenominatorReal) + Sqr(DenominatorImaginary),
      0.000000000001);
    Result := Result + 10 * Ln(Max(Magnitude, 0.000000000001)) / Ln(10);
  end;
end;

procedure TVSoundRemakeForm.DrawGraphicChart(ACanvas: TCanvas;
  const AClientRect: TRect);
const
  SampleCount = 1025;
var
  Canvas: TCanvas;
  R, Plot: TRect;
  I, X, Y: Integer;
  FrequencyHz, ResponseDb: Double;
  Points: array of TPoint;
  TextRect: TRect;
begin
  Canvas := ACanvas;
  R := AClientRect;
  GradientRect(Canvas, R, RGB(11, 12, 13), RGB(38, 40, 42));

  Plot := Rect(ScaleX(46), ScaleY(25), R.Right - ScaleX(10),
    R.Bottom - ScaleY(27));
  Canvas.Brush.Style := bsClear;
  // The chart frame is its edge, not a grid guide.  It stays visible in both
  // view modes so the response has a stable drawing area.
  Canvas.Pen.Color := RGB(153, 171, 176);
  Canvas.Rectangle(Plot);
  if Assigned(FGraphicShowGrid) and FGraphicShowGrid.Checked then
  begin
    Canvas.Pen.Color := RGB(54, 68, 72);
    // Each vertical guide follows the center of its matching fader.  The old
    // 17-division grid could never align with the twenty band controls.
    for I := Low(FFaders) to High(FFaders) do
    begin
      X := FFaders[I].Left + FFaders[I].Width div 2;
      Canvas.MoveTo(X, Plot.Top);
      Canvas.LineTo(X, Plot.Bottom);
    end;
    for I := 0 to 6 do
    begin
      Y := Plot.Top + (Plot.Height * I) div 6;
      Canvas.MoveTo(Plot.Left, Y);
      Canvas.LineTo(Plot.Right, Y);
    end;
    Y := Plot.Top + Plot.Height div 2;
    Canvas.Pen.Color := RGB(87, 118, 119);
    Canvas.MoveTo(Plot.Left, Y);
    Canvas.LineTo(Plot.Right, Y);
  end;

  SetLength(Points, SampleCount);
  for I := 0 to SampleCount - 1 do
  begin
    FrequencyHz := 40 * Power(16000 / 40, I / (SampleCount - 1));
    ResponseDb := ClampDouble(GraphicResponseAt(FrequencyHz), -20, 20);
    X := Plot.Left + (Plot.Width * I) div (SampleCount - 1);
    Y := Plot.Top + Round((20 - ResponseDb) * Plot.Height / 40);
    Points[I] := Point(X, Y);
  end;
  // A dark under-stroke creates a soft glow without the opaque green
  // mountain.  The response remains readable at any window scale.
  Canvas.Pen.Color := RGB(11, 82, 60);
  Canvas.Pen.Width := 5;
  Canvas.Polyline(Points);
  Canvas.Pen.Color := RGB(81, 239, 176);
  Canvas.Pen.Width := 2;
  Canvas.Polyline(Points);
  Canvas.Pen.Width := 1;
  Canvas.Brush.Style := bsSolid;
  Canvas.Brush.Color := RGB(81, 239, 176);
  Canvas.Pen.Color := RGB(8, 29, 23);
  for I := Low(FFaders) to High(FFaders) do
  begin
    X := Plot.Left + (Plot.Width * I) div High(FFaders);
    Y := Plot.Top + Round((20 + FFaders[I].Position) * Plot.Height / 40);
    Canvas.Ellipse(X - 3, Y - 3, X + 4, Y + 4);
  end;

  Canvas.Brush.Style := bsClear;
  Canvas.Font.Name := 'MS Sans Serif';
  Canvas.Font.Height := -Max(9, Round(9 * Min(FLayoutScaleX, FLayoutScaleY)));
  Canvas.Font.Color := clWhite;
  TextRect := Rect(Plot.Left, ScaleY(18), Plot.Right, ScaleY(37));
  Winapi.Windows.DrawText(Canvas.Handle, 'Graphic Equalizer', -1, TextRect,
    DT_CENTER or DT_VCENTER or DT_SINGLELINE);
  Canvas.Font.Height := -Max(9, Round(9 * Min(FLayoutScaleX, FLayoutScaleY)));
  TextRect := Rect(Plot.Left, R.Bottom - ScaleY(22), Plot.Right,
    R.Bottom - ScaleY(3));
  Winapi.Windows.DrawText(Canvas.Handle, 'Frequency, Hz', -1, TextRect,
    DT_CENTER or DT_VCENTER or DT_SINGLELINE);
  Canvas.Font.Orientation := 900;
  Canvas.TextOut(ScaleX(16), Plot.Top + Plot.Height div 2 + ScaleY(25),
    'dB Gain');
  Canvas.Font.Orientation := 0;
  Canvas.Brush.Style := bsSolid;
end;

procedure TVSoundRemakeForm.DrawArbitraryChart(ACanvas: TCanvas;
  const AClientRect: TRect);
const
  // The reference paint box is 800 pixels wide.  Sampling at each horizontal
  // pixel keeps spline, linear, and step rendering continuous while dragging.
  SampleCount = 1601;
var
  Canvas: TCanvas;
  R, Plot: TRect;
  I, X, Y: Integer;
  FrequencyHz, ResponseDb: Double;
  Points: array of TPoint;
begin
  Canvas := ACanvas;
  R := AClientRect;
  GradientRect(Canvas, R, RGB(11, 12, 13), RGB(38, 40, 42));

  SortArbitraryFilters;
  Plot := ArbitraryPlotRect;
  Canvas.Brush.Style := bsClear;
  // This is the permanent edge of the original TPanel.  It is not part of
  // the optional axes overlay, so the plot retains a clear boundary when the
  // user hides axes and grid.
  Canvas.Pen.Color := RGB(80, 97, 102);
  Canvas.Rectangle(Plot);
  if FArbitraryShowGrid.Checked then
  begin
    Canvas.Pen.Color := RGB(54, 68, 72);
    for I := 0 to 9 do
    begin
      X := Plot.Left + (Plot.Width * I) div 9;
      Canvas.MoveTo(X, Plot.Top);
      Canvas.LineTo(X, Plot.Bottom);
    end;
    for I := 0 to 8 do
    begin
      Y := Plot.Top + (Plot.Height * I) div 8;
      Canvas.MoveTo(Plot.Left, Y);
      Canvas.LineTo(Plot.Right, Y);
    end;
  end;
  if FArbitraryShowAxes.Checked then
  begin
    Canvas.Pen.Color := RGB(153, 171, 176);
    Canvas.MoveTo(Plot.Left, Plot.Top);
    Canvas.LineTo(Plot.Left, Plot.Bottom);
    Canvas.LineTo(Plot.Right, Plot.Bottom);
    Y := ArbitraryYForGain(0);
    Canvas.MoveTo(Plot.Left, Y);
    Canvas.LineTo(Plot.Right, Y);
  end;

  SetLength(Points, SampleCount);
  for I := 0 to SampleCount - 1 do
  begin
    FrequencyHz := ArbitraryFrequencyForX(Plot.Left +
      (Plot.Width * I) div (SampleCount - 1));
    ResponseDb := ArbitraryResponseAt(FrequencyHz);
    X := Plot.Left + (Plot.Width * I) div (SampleCount - 1);
    Y := ArbitraryYForGain(ResponseDb);
    Points[I] := Point(X, Y);
  end;
  Canvas.Pen.Color := RGB(11, 82, 60);
  Canvas.Pen.Width := 5;
  Canvas.Polyline(Points);
  Canvas.Pen.Color := RGB(81, 239, 176);
  Canvas.Pen.Width := 2;
  Canvas.Polyline(Points);
  Canvas.Pen.Width := 1;

  if FArbitraryShowMarkers.Checked then
    for I := 0 to FArbitraryFilters.Count - 1 do
    begin
      X := ArbitraryXForFrequency(FArbitraryFilters[I].FrequencyHz);
      Y := ArbitraryYForGain(FArbitraryFilters[I].GainDb);
      if I = FArbitraryDragIndex then
        Canvas.Brush.Color := clRed
      else
        Canvas.Brush.Color := RGB(81, 239, 176);
      Canvas.Pen.Color := RGB(8, 29, 23);
      if FArbitraryCircles.Checked then
        Canvas.Ellipse(X - 5, Y - 5, X + 6, Y + 6)
      else
        Canvas.Rectangle(X - 5, Y - 5, X + 6, Y + 6);
    end;

  if FArbitraryShowCrosshairs.Checked and FArbitraryCursorInside then
  begin
    X := FArbitraryCursorX;
    Y := FArbitraryCursorY;
    Canvas.Pen.Color := RGB(114, 144, 146);
    Canvas.MoveTo(X, Plot.Top);
    Canvas.LineTo(X, Plot.Bottom);
    Canvas.MoveTo(Plot.Left, Y);
    Canvas.LineTo(Plot.Right, Y);
  end;
  Canvas.Brush.Style := bsSolid;
end;

procedure TVSoundRemakeForm.AddStatus(const S: string);
begin
  FStatusLog.Lines.Add(S);
  FStatusLog.SelStart := Length(FStatusLog.Text);
  FStatusLog.SelLength := 0;
end;

end.
