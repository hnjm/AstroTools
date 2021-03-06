unit FConnection;

interface

uses
  Winapi.Windows, Winapi.Messages, System.SysUtils, System.Variants, System.Classes;

type TCommandData = array [0..256] of byte;
     TBuffer = array [0..1024] of byte;

type TWord = record
       case byte of
          0: (b1, b2:byte);
          1: (wrd1:word);
       end;
     TInteger = record
       case byte of
          0: (wrd1, wrd2:word);
          1: (int:integer);
          2: (b1, b2, b3, b4:byte);
          3: (f:single);
       end;

//  FBC Protocol
// 168 - ������ ���� �������, #13#10 - ����� ������

 const FBC_DEBUG_MSG = 250;
 const FBC_DEBUG = 251;
 const FBC_RCP = 252;  // Remote Control Panel
 const FBC_GET_FIRMWARE_VERSION = 253;
 const FBC_HANDSHAKE = 254;
 const FBC_PING = 255;

 const FBC_RTC_GET = 90;
 const FBC_RTC_SET = 91;
 const FBC_RTC_TEMP = 92;

//  Windows mrssages
 const WM_DEVICE_PING = WM_APP + FBC_PING;
 const WM_DEVICE_HANDSHAKE = WM_APP + FBC_HANDSHAKE;
 const WM_DEVICE_UNKNOWN_MESSAGE = WM_APP + 256;
 const WM_DEVICE_DEBUG_MSG = WM_APP + FBC_DEBUG_MSG;
 const WM_DEVICE_DEBUG = WM_APP + FBC_DEBUG;
 const WM_DEVICE_RCP = WM_APP + FBC_RCP;
 const WM_FIRMWARE_VERSION = WM_APP + FBC_GET_FIRMWARE_VERSION;
 const WM_RTC_TEMP = WM_APP + FBC_RTC_TEMP;
 const WM_RTC_GET = WM_APP + FBC_RTC_GET;
//  Devices

 const FD_NONE = 100;
 const FD_FOCUSER = 101;
 const FD_MOUNTER = 102;
 const FD_GROWER = 103;


//  Basic communication class
 type TFBasicCom = class(TObject)
   private
   protected
      CommHandle : integer;
      DCB : TDCB;
      Stat : TComStat;
      CommThread : THandle;
      Ovr : TOverlapped;
      ParentHandle:HWND;
      ThreadID, fError:Dword;

      fBuff:TBuffer;
      fPrevSize:DWORD;

      fRTCTemp:double;
      fRTCDateTimeStr:string;

      fIsDebug:boolean;
      fIsRCP_ON:boolean;  //  ���������� ������ ��, false - �������������

      fDeviceVersion, fUnknownMessage:string;
      fDeviceDbgMsg:array of string;
      fDeviceType:integer;

      fIsConnected:boolean;

      procedure SendData(cmd:byte); overload;
      procedure SendData(cmd:byte; data:byte); overload;
      procedure SendData(cmd:byte; data:word); overload;
      procedure SendData(cmd:byte; data:longword); overload;
      procedure SendData(cmd:byte; data:string); overload;
      procedure SendData(cmd:byte; par:byte; data:byte); overload;
      procedure SendData(cmd:byte; par:byte; data:integer); overload;

      class function ThreadProc(Param: Pointer): DWord; stdcall; static;
      function GeTCommandData:integer;
      procedure ParseData(data:TCommandData; size:integer); virtual;

      function fGetLastDeviceDbgMsg:string;
      procedure fAddLastDeviceDbgMsg(str:string);

      procedure fSetIsDebug(debug:boolean);
      procedure fSetRCP(status:boolean);
      procedure fRTCSetDateTimeStr(datetime:string);

      Procedure OnHandshake; virtual;

   public
      Constructor Init; virtual;
      function Connect(port:string; handle:HWND):integer; virtual;
      procedure ReConnect;                                                        // Send handshake command
      procedure Disconnect; virtual;
      property IsConnected:boolean read fIsConnected;

      property UnknownMessage:string read fUnknownMessage;
      property LastDebugMessage:string read fGetLastDeviceDbgMsg;
      property Version:string read fDeviceVersion;
      property LastError:DWORD read fError;
      property IsDebug:boolean read fIsDebug write fSetIsDebug;

      property IsRCP:boolean read fIsRCP_ON write fSetRCP;
      property RTCTemperature:double read fRTCTemp;
      property RTCDateTimeStr:string read fRTCDateTimeStr write fRTCSetDateTimeStr;

      Procedure Ping;

      Procedure GetTemperature;
      Procedure GetDateTime;

      procedure SendDebugCommand(cmd:byte);

 end;

  function  ByteArrayToStr(data:TCommandData; index:integer; size:integer):string;


implementation

function  ByteArrayToStr(data:TCommandData; index:integer; size:integer):string;
var i:integer;
begin
  result := '';
  for i := index to size do
    result := result + char(data[i]);
end;


function  ByteArrayToFloat(data:TCommandData; index:integer; size:integer):single;
var i:TInteger;
begin
   i.b1 := data[index];
   i.b2 := data[index+1];
   i.b3 := data[index+2];
   i.b4 := data[index+3];
   result := i.f;
end;

// -------------    TFBasicCom   -------------------

Constructor TFBasicCom.Init;
begin
  fDeviceVersion := '';
  ParentHandle := 0;
  fUnknownMessage := '';

  fIsDebug := false;
  CommHandle := 0;
  ThreadID := 0;
  fError := 0;
  fRTCTemp := 0;
  fIsConnected := false;

  fDeviceType := FD_NONE;

  fIsRCP_ON := true;
end;

function TFBasicCom.fGetLastDeviceDbgMsg:string;
var i:integer;
begin
   result := fDeviceDbgMsg[0];
   for i := 0 to Length(fDeviceDbgMsg)-2 do
      fDeviceDbgMsg[i]:= fDeviceDbgMsg[i+1];
   Setlength(fDeviceDbgMsg, Length(fDeviceDbgMsg)-1);
end;

procedure TFBasicCom.fAddLastDeviceDbgMsg(str:string);
begin
   Setlength(fDeviceDbgMsg, Length(fDeviceDbgMsg)+1);
   fDeviceDbgMsg[Length(fDeviceDbgMsg)-1] := str;
end;

function TFBasicCom.Connect(port:string; handle:HWND):integer;
var
   str:string;
   i: Integer;
begin

   for i := 0 to sizeof(fBuff)+1 do
      fBuff[i] := 0;
   fPrevSize:=0;


   ParentHandle := handle;
   result := 0;
   str := '//./' + port;
   CommHandle := CreateFile(PChar(str),GENERIC_READ or GENERIC_WRITE,0,nil,
         OPEN_EXISTING, FILE_FLAG_OVERLAPPED,0);

   if (CommHandle=-1) then
    begin
      result := -1;
      fError := GetLastError;
      exit;
    end;

   SetCommMask(CommHandle,EV_RXFLAG);

   GetCommState(CommHandle,DCB);
   DCB.BaudRate:=CBR_115200;
//   DCB.BaudRate:=CBR_9600;
   DCB.Parity:=NOPARITY;
   DCB.ByteSize:=8;
   DCB.StopBits:=OneStopBit;
   DCB.EvtChar:=chr(10);
   SetCommState(CommHandle,DCB);

   CommThread := CreateThread(nil,0,@ThreadProc, Self,0,ThreadID);

//   SendData(FBC_DEBUG, 1);

   SendData(FBC_HANDSHAKE);   // FirmwareVersion request
end;


procedure TFBasicCom.ReConnect;
begin
   SendData(FBC_HANDSHAKE);   // FirmwareVersion request
end;

procedure TFBasicCom.Disconnect;
begin
    TerminateThread(CommThread,0);
    CloseHandle(CommHandle);
end;

class function TFBasicCom.ThreadProc(Param: Pointer): DWord;
begin
  result := TFBasicCom(Param).GetCommandData;
end;


procedure TFBasicCom.SendData(cmd:byte);
var
  Transmit:array [0..255] of byte;
  sendsize:DWORD;
begin
   sendsize:=4;
   Transmit[0]:=168;
   Transmit[1]:=cmd;
   Transmit[2]:=13;
   Transmit[3]:=10;
   WriteFile(CommHandle,Transmit,sendsize,sendsize,@Ovr);
end;

procedure TFBasicCom.SendData(cmd:byte; data:byte);
var
  Transmit:array [0..255] of byte;
  sendsize:DWORD;
begin
   sendsize:=5;
   Transmit[0]:=168;
   Transmit[1]:=cmd;
   Transmit[2]:=data;
   Transmit[3]:=13;
   Transmit[4]:=10;
   WriteFile(CommHandle,Transmit,sendsize,sendsize,@Ovr);
end;

procedure TFBasicCom.SendData(cmd:byte; data:word);
var Transmit:array [0..255] of byte;
    sendsize:DWORD;
    data_b:TWord;
begin
   data_b.wrd1 := data;
   sendsize:=6;
   Transmit[0]:=168;
   Transmit[1]:=cmd;
   Transmit[2]:=data_b.b1;
   Transmit[3]:=data_b.b2;
   Transmit[4]:=13;
   Transmit[5]:=10;
   WriteFile(CommHandle,Transmit,sendsize,sendsize,@Ovr);
end;

procedure TFBasicCom.SendData(cmd:byte; data:longword);
var Transmit:array [0..255] of byte;
    sendsize:DWORD;
    data_b:TInteger;
begin
   data_b.int := data;
   sendsize:=8;
   Transmit[0]:=168;
   Transmit[1]:=cmd;
   Transmit[2]:=data_b.b1;
   Transmit[3]:=data_b.b2;
   Transmit[4]:=data_b.b3;
   Transmit[5]:=data_b.b4;
   Transmit[6]:=13;
   Transmit[7]:=10;
   WriteFile(CommHandle,Transmit,sendsize,sendsize,@Ovr);
end;

procedure TFBasicCom.SendData(cmd:byte; par:byte; data:integer);
var Transmit:array [0..255] of byte;
    sendsize:DWORD;
    data_b:TInteger;
begin
   data_b.int := data;
   sendsize:=9;
   Transmit[0]:=168;
   Transmit[1]:=cmd;
   Transmit[2]:=par;
   Transmit[3]:=data_b.b1;
   Transmit[4]:=data_b.b2;
   Transmit[5]:=data_b.b3;
   Transmit[6]:=data_b.b4;
   Transmit[7]:=13;
   Transmit[8]:=10;
   WriteFile(CommHandle,Transmit,sendsize,sendsize,@Ovr);
end;

procedure TFBasicCom.SendData(cmd:byte; par:byte; data:byte);
var
  Transmit:array [0..255] of byte;
  sendsize:DWORD;
begin
   sendsize:=6;
   Transmit[0]:=168;
   Transmit[1]:=cmd;
   Transmit[2]:=par;
   Transmit[3]:=data;
   Transmit[4]:=13;
   Transmit[5]:=10;
   WriteFile(CommHandle,Transmit,sendsize,sendsize,@Ovr);
end;

procedure TFBasicCom.SendData(cmd:byte; data:string);
var
  Transmit:array [0..255] of byte;
  sendsize:DWORD;
  i:integer;
begin
   for i:=0 to length(Transmit) do
       Transmit[i] := 0;
   Transmit[0]:=168;
   Transmit[1]:=cmd;

   for i:=1 to length(data) do
     Transmit[i+1]:=byte(data[i]);
   Transmit[i+1]:=13;
   Transmit[i+2]:=10;
   sendsize:= length(data) + 4;
   WriteFile(CommHandle,Transmit,sendsize,sendsize,@Ovr);
end;

procedure TFBasicCom.SendDebugCommand(cmd:byte);
begin
  SendData(cmd);
end;

Procedure TFBasicCom.GetTemperature;
begin
  SendData(FBC_RTC_TEMP);
end;



function FindCmdStart(data:TBuffer; size:integer):integer;
   var i:integer;
   begin
     result := -1;
     for i := 0 to size-1 do
       if data[i]=168 then
         begin
           result := i;
           break;
         end;
   end;
   procedure ShiftBuffer(var Buffer:TBuffer; size, shift:integer);
   var i:integer;
   begin
    for i := shift to size-1 do
      Buffer[i-shift] := Buffer[i];

    for i:= size-shift to size-1 do
      Buffer[i] := 0;
   end;

   function FindCmdEnd(data:TBuffer; size:integer):integer;
   var i:integer;
   begin
     result := -1;
     for i := 0 to size-2 do
       if (data[i]=13)and(data[i+1]=10) then
         begin
           result := i;
           break;
         end;
   end;

function TFBasicCom.GeTCommandData:integer;
var
   data:TBuffer;
   i, pos: Integer;
   readsize:DWORD;
   TransMask: DWord;
   Errs : DWord;
   cmd:TCommandData;
begin
   readsize := fPrevSize;  //?????

   while true do
   begin
    TransMask:=EV_RXFLAG;
    WaitCommEvent(CommHandle,TransMask,NIL);
    if ((TransMask and EV_RXFLAG)=EV_RXFLAG) then
     begin
      ClearCommError(CommHandle,Errs,@Stat);
      fPrevSize := readsize;
      readsize := Stat.cbInQue;
      for i := 0 to sizeof(fBuff)+1 do
         data[i]:=0;
      ReadFile(CommHandle,data,readsize,readsize,@Ovr);
      for i := 0 to readsize do
        fBuff[fPrevSize+i] := data[i];
      readsize := readsize+fPrevSize;

      while readsize<>0 do
      begin
        pos := FindCmdStart(fBuff, readsize);
        if pos=-1 then
          begin
            readsize:=0;
            for i := 0 to sizeof(fBuff)+1 do
              fBuff[i]:=0;
            break;
          end;
        if pos>0 then
          begin
            ShiftBuffer(fBuff, readsize, pos);
            readsize:=readsize-pos;
          end;

        pos := FindCmdEnd(fBuff, readsize);
        if pos=-1 then
           break;
        for i := 0 to length(cmd) do
          cmd[i] := 0;
        for i := 0 to pos-1 do
          cmd[i] := fBuff[i];
        ParseData(cmd, pos);
        ShiftBuffer(fBuff, readsize, pos+2);
        readsize := readsize-(pos+2);
      end;

     end;
    end;
end;

Procedure TFBasicCom.OnHandshake;
begin
    fIsConnected := true;
    SendData(FBC_GET_FIRMWARE_VERSION);
    SendData(FBC_RCP, 1);
end;

procedure TFBasicCom.ParseData(data:TCommandData; size:integer);
var s:string;
begin
     try
      case data[1] of
        FBC_HANDSHAKE:
          begin
            if (data[2] = fDeviceType) then
              begin
                PostMessage(ParentHandle, WM_DEVICE_HANDSHAKE, 1, data[2]);
                OnHandshake;
              end
            else
               PostMessage(ParentHandle, WM_DEVICE_HANDSHAKE, 0, data[2]);
          end;
        FBC_DEBUG_MSG:
          begin
            fAddLastDeviceDbgMsg(ByteArrayToStr(data, 2, size-1));
            PostMessage(ParentHandle, WM_DEVICE_DEBUG_MSG, 0, 0);
          end;
       FBC_PING:
          PostMessage(ParentHandle, WM_DEVICE_PING, 0, 0);
       FBC_DEBUG:
          begin
            if (data[2]<>0) then
               fIsDebug := true
              else
               fIsDebug := false;
            PostMessage(ParentHandle, WM_DEVICE_DEBUG, 0, 0);
          end;
       FBC_RCP:
         begin
           if (data[2]<>0) then
               fIsRCP_ON := true
              else
               fIsRCP_ON := false;
           PostMessage(ParentHandle, WM_DEVICE_RCP, 0, 0);
         end;
       FBC_GET_FIRMWARE_VERSION:
         begin
           fDeviceVersion := ByteArrayToStr(data, 2, 4);
           PostMessage(ParentHandle, WM_FIRMWARE_VERSION, 0, 0);
         end;
       FBC_RTC_TEMP:
         begin
//           fRTCTemp := ByteArrayToFloat(data, 2, size-1);
           fRTCTemp := StrToFloat(ByteArrayToStr(data, 2, size-1));
           PostMessage(ParentHandle, WM_RTC_TEMP, 0, 0);
         end;
       FBC_RTC_GET, FBC_RTC_SET:
         begin
           fRTCDateTimeStr := ByteArrayToStr(data, 2, size-1);
           PostMessage(ParentHandle, WM_RTC_GET, 0, 0);
         end
        else
         begin
           fUnknownMessage := ByteArrayToStr(data, 2, size-1);
           PostMessage(ParentHandle, WM_DEVICE_UNKNOWN_MESSAGE, 0, 0);
         end;
      end;
     except
       On EConvertError do
         begin
           fAddLastDeviceDbgMsg('Convert error!  Cmd:' + IntToStr(data[1]) + ' ' + s);
           PostMessage(ParentHandle, WM_DEVICE_DEBUG_MSG, 1, 0);
         end;
     end;
end;


procedure TFBasicCom.fSetIsDebug(debug:boolean);
begin
    if (debug) then
      SendData(FBC_DEBUG, 1)
    else
      SendData(FBC_DEBUG, 0);
end;

Procedure TFBasicCom.Ping;
begin
   SendData(FBC_PING);
end;

procedure TFBasicCom.fSetRCP(status:boolean);
begin
    if (status) then
      SendData(FBC_RCP, 1)
    else
      SendData(FBC_RCP, 0);
end;

Procedure TFBasicCom.GetDateTime;
begin
   SendData(FBC_RTC_GET);
end;


procedure TFBasicCom.fRTCSetDateTimeStr(datetime:string);
begin
  SendData(FBC_RTC_SET, datetime);
end;

end.
