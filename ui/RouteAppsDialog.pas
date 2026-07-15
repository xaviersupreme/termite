unit RouteAppsDialog;

interface

uses
  System.Classes,
  Vcl.CheckLst,
  Vcl.Forms,
  Vcl.StdCtrls,
  TermiteBridge;

type
  TRouteAppsDialog = class(TForm)
  private
    FApps: TTermiteRouteResponse;
    FAppList: TCheckListBox;
    FInfo: TLabel;
    FUpdating: Boolean;
    procedure LoadApps(const Prefix: string = '');
    procedure AppCheckChanged(Sender: TObject);
    procedure RefreshClick(Sender: TObject);
  public
    constructor Create(AOwner: TComponent); override;
  end;

implementation

uses
  System.Math,
  System.SysUtils,
  Vcl.Controls;

constructor TRouteAppsDialog.Create(AOwner: TComponent);
var
  RefreshButton, CloseButton: TButton;
begin
  inherited CreateNew(AOwner);
  Caption := 'Route apps through Termite';
  BorderStyle := bsSizeable;
  BorderIcons := [biSystemMenu, biMinimize, biMaximize];
  Position := poOwnerFormCenter;
  ClientWidth := 500;
  ClientHeight := 390;
  Constraints.MinWidth := 420;
  Constraints.MinHeight := 310;
  Font.Name := 'Tahoma';
  Font.Height := -11;

  FInfo := TLabel.Create(Self);
  FInfo.Parent := Self;
  FInfo.AutoSize := False;
  FInfo.WordWrap := True;
  FInfo.SetBounds(14, 12, ClientWidth - 28, 44);
  FInfo.Caption := 'A check means the app has a CABLE preference. The status shows its live stream. Restart playback after routing or returning an app.';

  FAppList := TCheckListBox.Create(Self);
  FAppList.Parent := Self;
  FAppList.SetBounds(14, 63, ClientWidth - 28, 265);
  FAppList.Anchors := [akLeft, akTop, akRight, akBottom];
  FAppList.OnClickCheck := AppCheckChanged;

  RefreshButton := TButton.Create(Self);
  RefreshButton.Parent := Self;
  RefreshButton.Caption := 'Refresh';
  RefreshButton.SetBounds(14, 344, 100, 27);
  RefreshButton.Anchors := [akLeft, akBottom];
  RefreshButton.OnClick := RefreshClick;

  CloseButton := TButton.Create(Self);
  CloseButton.Parent := Self;
  CloseButton.Caption := 'Close';
  CloseButton.ModalResult := mrClose;
  CloseButton.SetBounds(ClientWidth - 114, 344, 100, 27);
  CloseButton.Anchors := [akRight, akBottom];

  LoadApps;
end;

procedure TRouteAppsDialog.LoadApps(const Prefix: string);
var
  I, Count: Integer;
  App: TTermiteRouteApp;
  Name, StateText, MessageText: string;
  LiveOnCable: Boolean;
begin
  FUpdating := True;
  try
    if QueryTermiteRoutes(FApps) <> tbrApplied then
    begin
      FAppList.Items.Clear;
      FInfo.Caption := 'Termite is not running or did not answer the routing request.';
      Exit;
    end;
    FAppList.Items.BeginUpdate;
    try
      FAppList.Items.Clear;
      Count := Min(Integer(FApps.AppCount), TermiteRouteBridgeMaxApps);
      for I := 0 to Count - 1 do
      begin
        App := FApps.Apps[I];
        Name := TermiteRouteAppName(App);
        if Name = '' then
          Name := TermiteRouteAppPath(App);
        LiveOnCable := App.Reserved[0] <> 0;
        if LiveOnCable then
          StateText := 'CABLE Input'
        else if App.ActiveSessionCount <> 0 then
          StateText := 'Windows output'
        else
          StateText := 'no active stream';
        // This source unit is ANSI in the current project. Keep the divider
        // ASCII so a UTF-8 punctuation character cannot turn into mojibake.
        FAppList.Items.Add(Format('%s  -  %s', [Name, StateText]));
        FAppList.Checked[I] := App.RoutedToCable <> 0;
      end;
    finally
      FAppList.Items.EndUpdate;
    end;
    MessageText := TermiteRouteMessage(FApps);
    if Prefix <> '' then
      FInfo.Caption := Prefix + '  ' + MessageText
    else
      FInfo.Caption := MessageText;
  finally
    FUpdating := False;
  end;
end;

procedure TRouteAppsDialog.AppCheckChanged(Sender: TObject);
var
  Index: Integer;
  Response: TTermiteRouteResponse;
  ResultState: TTermiteBridgeResult;
  Path, MessageText: string;
begin
  if FUpdating then
    Exit;
  Index := FAppList.ItemIndex;
  if (Index < 0) or (Index >= Integer(FApps.AppCount)) then
    Exit;
  Path := TermiteRouteAppPath(FApps.Apps[Index]);
  ResultState := SetTermiteRoute(Path, FAppList.Checked[Index], Response);
  MessageText := TermiteRouteMessage(Response);
  if ResultState <> tbrApplied then
  begin
    if MessageText <> '' then
      FInfo.Caption := MessageText
    else
      FInfo.Caption := 'Termite could not change that app route.';
    LoadApps('Route change failed: ' + FInfo.Caption);
    Exit;
  end;
  if MessageText = '' then
    MessageText := 'Route updated.';
  LoadApps(MessageText);
end;

procedure TRouteAppsDialog.RefreshClick(Sender: TObject);
begin
  LoadApps('Route state refreshed.');
end;

end.
