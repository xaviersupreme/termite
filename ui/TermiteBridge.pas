unit TermiteBridge;

interface

uses
  System.SyncObjs,
  System.SysUtils,
  Winapi.Windows;

const
  TermiteBridgePipe = '\\.\pipe\termite.eq.v1';
  TermiteBridgeMagic = Cardinal($5145544D); // "MTEQ"
  TermiteBridgeVersion = Word(1);
  TermiteBridgeGraphicBands = 20;
  TermiteBridgeMaxMarkers = 32;
  TermiteRouteBridgePipe = '\\.\pipe\termite.route.v1';
  TermiteRouteBridgeMagic = Cardinal($5452544D); // "MTRT"
  TermiteRouteBridgeVersion = Word(1);
  TermiteRouteBridgeMaxApps = 48;
  TermiteRouteBridgePathChars = 520;
  TermiteRouteBridgeDisplayChars = 128;
  TermiteRouteBridgeMessageChars = 256;

type
  TTermiteEqMode = (temGraphic = 0, temArbitrary = 1);
  TTermiteInterpolation = (tiSpline = 0, tiLinear = 1, tiStep = 2);
  TTermiteXAxis = (txLogarithmic = 0, txLinear = 1);

  TTermiteEqMarker = packed record
    FrequencyHz: Single;
    GainDb: Single;
  end;

  // Keep this packed record byte-for-byte identical to
  // host/eq_bridge_protocol.h. The pipe carries values only; it never
  // carries a Delphi or C++ object.
  TTermiteEqSnapshot = packed record
    Magic: Cardinal;
    Version: Word;
    Bytes: Word;
    Sequence: Cardinal;
    Mode: Byte;
    EqualizerEnabled: Byte;
    Interpolation: Byte;
    XAxis: Byte;
    PreampDb: Single;
    Smoothing: Single;
    Tension: Single;
    MarkerCount: Cardinal;
    GraphicGains: array[0..TermiteBridgeGraphicBands - 1] of Single;
    Markers: array[0..TermiteBridgeMaxMarkers - 1] of TTermiteEqMarker;
  end;

  TTermiteBridgeResult = (tbrApplied, tbrUnavailable, tbrRejected);

  TTermiteRouteCommand = (trcListApps = 0, trcSetRoute = 1);

  // Mirrored from host/route_bridge_protocol.h. It is intentionally a
  // value-only record so the VCL app never reaches into the C++ host's ABI.
  TTermiteRouteRequest = packed record
    Magic: Cardinal;
    Version: Word;
    Bytes: Word;
    Sequence: Cardinal;
    Command: Byte;
    RouteToCable: Byte;
    Reserved: Word;
    ExecutablePath: array[0..TermiteRouteBridgePathChars - 1] of WideChar;
  end;

  TTermiteRouteApp = packed record
    ExecutablePath: array[0..TermiteRouteBridgePathChars - 1] of WideChar;
    DisplayName: array[0..TermiteRouteBridgeDisplayChars - 1] of WideChar;
    OpenWindowCount: Cardinal;
    ActiveSessionCount: Cardinal;
    RoutedToCable: Byte;
    Reserved: array[0..2] of Byte;
  end;

  TTermiteRouteResponse = packed record
    Magic: Cardinal;
    Version: Word;
    Bytes: Word;
    Sequence: Cardinal;
    Status: Cardinal;
    AppCount: Cardinal;
    MessageText: array[0..TermiteRouteBridgeMessageChars - 1] of WideChar;
    Apps: array[0..TermiteRouteBridgeMaxApps - 1] of TTermiteRouteApp;
  end;

  // The VCL thread owns drawing and input.  Named-pipe I/O can briefly wait
  // for the host, so it belongs on this small latest-value worker instead.
  // Submit deliberately replaces an unsent snapshot: dragging a control only
  // needs the newest EQ state, never a backlog of stale intermediate states.
  TTermiteBridgeClient = class
  private
    FLock: TObject;
    FWakeEvent: TEvent;
    FThread: THandle;
    FStopping: Boolean;
    FPendingSnapshot: TTermiteEqSnapshot;
    FHasPendingSnapshot: Boolean;
    FLastResult: TTermiteBridgeResult;
    FHasResult: Boolean;
    procedure Execute;
  public
    constructor Create;
    destructor Destroy; override;
    procedure Submit(const Snapshot: TTermiteEqSnapshot);
    function TryTakeResult(out BridgeResult: TTermiteBridgeResult): Boolean;
  end;

function SendTermiteEqSnapshot(const Snapshot: TTermiteEqSnapshot): TTermiteBridgeResult;
function QueryTermiteRoutes(out Response: TTermiteRouteResponse): TTermiteBridgeResult;
function SetTermiteRoute(const ExecutablePath: string; RouteToCable: Boolean;
  out Response: TTermiteRouteResponse): TTermiteBridgeResult;
function TermiteRouteAppPath(const App: TTermiteRouteApp): string;
function TermiteRouteAppName(const App: TTermiteRouteApp): string;
function TermiteRouteMessage(const Response: TTermiteRouteResponse): string;

implementation

type
  TTermiteEqAck = packed record
    Magic: Cardinal;
    Version: Word;
    Bytes: Word;
    Sequence: Cardinal;
    Status: Cardinal;
  end;

function TermiteBridgeThreadProc(Parameter: Pointer): DWORD; stdcall;
begin
  Result := 0;
  try
    TTermiteBridgeClient(Parameter).Execute;
  except
    // This worker has no UI affinity.  A failed bridge request is reported to
    // the form as unavailable instead of leaking an exception across Win32.
  end;
end;

function SendTermiteEqSnapshot(const Snapshot: TTermiteEqSnapshot): TTermiteBridgeResult;
var
  Pipe: THandle;
  Written, Read: Cardinal;
  Ack: TTermiteEqAck;
begin
  Result := tbrUnavailable;
  if not WaitNamedPipe(PChar(TermiteBridgePipe), 25) then
    Exit;

  Pipe := CreateFile(PChar(TermiteBridgePipe), GENERIC_READ or GENERIC_WRITE,
    0, nil, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if Pipe = INVALID_HANDLE_VALUE then
    Exit;
  try
    if not WriteFile(Pipe, Snapshot, SizeOf(Snapshot), Written, nil) or
      (Written <> SizeOf(Snapshot)) then
      Exit;
    if not ReadFile(Pipe, Ack, SizeOf(Ack), Read, nil) or
      (Read <> SizeOf(Ack)) then
      Exit;
    if (Ack.Magic <> TermiteBridgeMagic) or
      (Ack.Version <> TermiteBridgeVersion) or
      (Ack.Bytes <> SizeOf(Ack)) or (Ack.Sequence <> Snapshot.Sequence) then
      Exit;
    if Ack.Status = 0 then
      Result := tbrApplied
    else
      Result := tbrRejected;
  finally
    CloseHandle(Pipe);
  end;
end;

function RouteBridgeBufferText(const Buffer: array of WideChar): string;
begin
  if Length(Buffer) = 0 then
    Exit('');
  Result := PWideChar(@Buffer[0]);
end;

function SendTermiteRouteRequest(const Request: TTermiteRouteRequest;
  out Response: TTermiteRouteResponse): TTermiteBridgeResult;
var
  Pipe: THandle;
  Written, Read: Cardinal;
begin
  FillChar(Response, SizeOf(Response), 0);
  Result := tbrUnavailable;
  if not WaitNamedPipe(PChar(TermiteRouteBridgePipe), 1000) then
    Exit;

  Pipe := CreateFile(PChar(TermiteRouteBridgePipe), GENERIC_READ or GENERIC_WRITE,
    0, nil, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if Pipe = INVALID_HANDLE_VALUE then
    Exit;
  try
    if not WriteFile(Pipe, Request, SizeOf(Request), Written, nil) or
      (Written <> SizeOf(Request)) then
      Exit;
    if not ReadFile(Pipe, Response, SizeOf(Response), Read, nil) or
      (Read <> SizeOf(Response)) then
      Exit;
    if (Response.Magic <> TermiteRouteBridgeMagic) or
      (Response.Version <> TermiteRouteBridgeVersion) or
      (Response.Bytes <> SizeOf(Response)) or
      (Response.Sequence <> Request.Sequence) then
      Exit;
    if Response.Status = 0 then
      Result := tbrApplied
    else
      Result := tbrRejected;
  finally
    CloseHandle(Pipe);
  end;
end;

function NewRouteRequest(const Command: TTermiteRouteCommand;
  const ExecutablePath: string; RouteToCable: Boolean): TTermiteRouteRequest;
begin
  FillChar(Result, SizeOf(Result), 0);
  Result.Magic := TermiteRouteBridgeMagic;
  Result.Version := TermiteRouteBridgeVersion;
  Result.Bytes := SizeOf(Result);
  Result.Sequence := GetTickCount;
  Result.Command := Ord(Command);
  if RouteToCable then
    Result.RouteToCable := 1;
  if ExecutablePath <> '' then
    StrPLCopy(Result.ExecutablePath, ExecutablePath,
      TermiteRouteBridgePathChars - 1);
end;

function QueryTermiteRoutes(out Response: TTermiteRouteResponse): TTermiteBridgeResult;
begin
  Result := SendTermiteRouteRequest(NewRouteRequest(trcListApps, '', False), Response);
end;

function SetTermiteRoute(const ExecutablePath: string; RouteToCable: Boolean;
  out Response: TTermiteRouteResponse): TTermiteBridgeResult;
begin
  Result := SendTermiteRouteRequest(NewRouteRequest(trcSetRoute, ExecutablePath,
    RouteToCable), Response);
end;

function TermiteRouteAppPath(const App: TTermiteRouteApp): string;
begin
  Result := RouteBridgeBufferText(App.ExecutablePath);
end;

function TermiteRouteAppName(const App: TTermiteRouteApp): string;
begin
  Result := RouteBridgeBufferText(App.DisplayName);
end;

function TermiteRouteMessage(const Response: TTermiteRouteResponse): string;
begin
  Result := RouteBridgeBufferText(Response.MessageText);
end;

constructor TTermiteBridgeClient.Create;
var
  ThreadId: Cardinal;
begin
  FLock := TObject.Create;
  FWakeEvent := TEvent.Create(nil, True, False, '');
  FLastResult := tbrUnavailable;
  FThread := CreateThread(nil, 0, @TermiteBridgeThreadProc, Self, 0, ThreadId);
  if FThread = 0 then
  begin
    FWakeEvent.Free;
    FLock.Free;
    RaiseLastOSError;
  end;
end;

destructor TTermiteBridgeClient.Destroy;
begin
  TMonitor.Enter(FLock);
  try
    FStopping := True;
    FHasPendingSnapshot := False;
  finally
    TMonitor.Exit(FLock);
  end;
  FWakeEvent.SetEvent;
  WaitForSingleObject(FThread, INFINITE);
  CloseHandle(FThread);
  FWakeEvent.Free;
  FLock.Free;
end;

procedure TTermiteBridgeClient.Submit(const Snapshot: TTermiteEqSnapshot);
begin
  TMonitor.Enter(FLock);
  try
    FPendingSnapshot := Snapshot;
    FHasPendingSnapshot := True;
  finally
    TMonitor.Exit(FLock);
  end;
  FWakeEvent.SetEvent;
end;

function TTermiteBridgeClient.TryTakeResult(
  out BridgeResult: TTermiteBridgeResult): Boolean;
begin
  TMonitor.Enter(FLock);
  try
    Result := FHasResult;
    if Result then
    begin
      BridgeResult := FLastResult;
      FHasResult := False;
    end;
  finally
    TMonitor.Exit(FLock);
  end;
end;

procedure TTermiteBridgeClient.Execute;
var
  Snapshot: TTermiteEqSnapshot;
  HasSnapshot: Boolean;
  BridgeResult: TTermiteBridgeResult;
begin
  while True do
  begin
    if FWakeEvent.WaitFor(100) <> wrSignaled then
      Continue;
    FWakeEvent.ResetEvent;

    HasSnapshot := False;
    TMonitor.Enter(FLock);
    try
      if FStopping then
        Exit;
      if FHasPendingSnapshot then
      begin
        Snapshot := FPendingSnapshot;
        FHasPendingSnapshot := False;
        HasSnapshot := True;
      end;
    finally
      TMonitor.Exit(FLock);
    end;

    if not HasSnapshot then
      Continue;

    BridgeResult := SendTermiteEqSnapshot(Snapshot);
    TMonitor.Enter(FLock);
    try
      FLastResult := BridgeResult;
      FHasResult := True;
    finally
      TMonitor.Exit(FLock);
    end;
  end;
end;

initialization
  Assert(SizeOf(TTermiteEqMarker) = 8);
  Assert(SizeOf(TTermiteEqSnapshot) = 368);
  Assert(SizeOf(TTermiteEqAck) = 16);
  Assert(SizeOf(TTermiteRouteRequest) = 1056);
  Assert(SizeOf(TTermiteRouteApp) = 1308);
  Assert(SizeOf(TTermiteRouteResponse) = 63316);

end.
