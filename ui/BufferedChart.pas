unit BufferedChart;

interface

uses
  System.Classes,
  System.Types,
  Winapi.Messages,
  Vcl.Controls,
  Vcl.Graphics;

type
  TChartRenderEvent = procedure(ACanvas: TCanvas;
    const AClientRect: TRect) of object;

  
  
  
  
  TBufferedChart = class(TCustomControl)
  private
    FBuffer: Vcl.Graphics.TBitmap;
    FOnRender: TChartRenderEvent;
    procedure WMEraseBkgnd(var Message: TWMEraseBkgnd); message WM_ERASEBKGND;
  protected
    procedure Paint; override;
  public
    constructor Create(AOwner: TComponent); override;
    destructor Destroy; override;
    property Color;
    property ParentColor;
    property Cursor;
    property OnMouseDown;
    property OnMouseMove;
    property OnMouseUp;
    property OnRender: TChartRenderEvent read FOnRender write FOnRender;
  end;

implementation

uses
  Winapi.Windows;

constructor TBufferedChart.Create(AOwner: TComponent);
begin
  inherited Create(AOwner);
  ControlStyle := ControlStyle + [csOpaque];
  
  
  StyleElements := [];
  DoubleBuffered := True;
  ParentDoubleBuffered := False;
  Color := clBlack;
  FBuffer := Vcl.Graphics.TBitmap.Create;
  FBuffer.PixelFormat := pf32bit;
end;

destructor TBufferedChart.Destroy;
begin
  FBuffer.Free;
  inherited;
end;

procedure TBufferedChart.WMEraseBkgnd(var Message: TWMEraseBkgnd);
begin
  
  
  Message.Result := 1;
end;

procedure TBufferedChart.Paint;
var
  Client: TRect;
begin
  if (ClientWidth <= 0) or (ClientHeight <= 0) then
    Exit;

  Client := ClientRect;
  if (FBuffer.Width <> ClientWidth) or (FBuffer.Height <> ClientHeight) then
  begin
    FBuffer.SetSize(ClientWidth, ClientHeight);
    FBuffer.PixelFormat := pf32bit;
  end;
  FBuffer.Canvas.Brush.Style := bsSolid;
  FBuffer.Canvas.Brush.Color := Color;
  FBuffer.Canvas.FillRect(Client);
  if Assigned(FOnRender) then
    FOnRender(FBuffer.Canvas, Client);
  BitBlt(Canvas.Handle, 0, 0, ClientWidth, ClientHeight,
    FBuffer.Canvas.Handle, 0, 0, SRCCOPY);
end;

end.
