/*---------------------------------------------------------------------------
PROJECT: Steem SSE
Atari ST emulator
Copyright (C) 2026 by Anthony Hayward and Russel Hayward + SSE

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see https://www.gnu.org/licenses/.

DOMAIN: I/O
FILE: stports.cpp
DESCRIPTION: Code to handle Steem's flexible port redirection system.
Internet connection (if SSE_NETWORK defined, WIN32).
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <computer.h>
#include <translate.h>
#include <stjoy.h>

#if defined(SSE_NETWORK)

#pragma warning (disable: 4459) // warning C4459: declaration of 'pc' hides global declaration

#include <notifyinit.h>
#include <ws2tcpip.h>
// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")
#define DEFAULT_BUFLEN 512
DWORD WINAPI ConnectThreadProc(PVOID pParam);
DWORD WINAPI ReceiveThreadProc(PVOID pParam);

#pragma warning (default: 4459)

#endif

TSTPort STPort[NSTPORTS];

#if defined(SSE_DONGLE)
TDongle Dongle;
#endif

// MIDI Port

#define LOGSECTION LOGSECTION_PORTS 
// TRACE_LOG: init, errors... TRACE_LOG2: all traffic
#define PORT_LOG TRACE_LOG
#define PORT_LOG2 TRACE_LOG/*2*/ //wip


void agenda_midi_replace(int) { // MIDI -> ACIA
  if(OPTION_C1) // called from event_acia()
  {
    acia[ACIA_MIDI].LineRxBusy=0;
    if(MIDIPort.AreBytesToCome())
    {
      MIDIPort.NextByte();
      BYTE midi_in=MIDIPort.ReadByte();
      PORT_LOG2("F%d MIDI ACIA RDR %X SR %x\n",FRAME,midi_in,acia[ACIA_MIDI].sr);
      if(acia[ACIA_MIDI].sr&ACIA_RDRF)
      {
        // discard data and set overrun
        PORT_LOG("OVR\n");
        if(acia[ACIA_MIDI].overrun!=ACIA_OVERRUN_YES)
          acia[ACIA_MIDI].overrun=ACIA_OVERRUN_COMING;
      }
      else
      {
        acia[ACIA_MIDI].rdr=midi_in;
        acia[ACIA_MIDI].sr|=ACIA_RDRF; // RDR full
        acia[ACIA_MIDI].sr&=~ACIA_OVRN; // no overrun //here?
      }
      if(acia[ACIA_MIDI].cr&ACIA_IRQ)
        acia[ACIA_MIDI].sr|=ACIA_IRQ; // IRQ anyway
      mfp_gpip_set_bit(MFP_GPIP_ACIA_BIT,
        !((acia[ACIA_IKBD].sr&ACIA_IRQ)||(acia[ACIA_MIDI].sr&ACIA_IRQ)));
      acia[ACIA_MIDI].LineRxBusy=0;
      if(MIDIPort.AreBytesToCome())
      {
        acia[ACIA_MIDI].LineRxBusy=1;
        COUNTER_VAR trans_time=acia[ACIA_MIDI].TransmissionTime();
        // use timestamp to shift receive timing
        COUNTER_VAR rectime=time_of_event_acia;
#if defined(SSE_DIRECTMIDI)
        if(MIDIPort.MIDI_In->Buf.LastTiming<MIDI_UNITS_1SEC)
#else
        if(MIDIPort.MIDI_In->TimeSinceLastNote<MIDI_UNITS_1SEC)
#endif
        {
          //TRACE_OSD("M2 %d",MIDIPort.MIDI_In->TimeSinceLastNote);  

// why /TICKS8? if it's a bug recording would be too fast but report says it's too slow
//          COUNTER_VAR CyclesSinceLastNote=(COUNTER_VAR)((MIDIPort.MIDI_In->TimeSinceLastNote*
//            nSysCyclesPerSecond/TICKS8)/MIDI_UNITS_1SEC);

          COUNTER_VAR CyclesSinceLastNote=(COUNTER_VAR)((MIDIPort.MIDI_In->TimeSinceLastNote*
            nSysCyclesPerSecond)/MIDI_UNITS_1SEC);

          //TRACE_OSD("M2 %d",CyclesSinceLastNote);
          COUNTER_VAR t=MIDIPort.MIDI_In->CycleOfLastNote+CyclesSinceLastNote;
          //TRACE_OSD("M2 %d",t-time_of_event_acia);
          COUNTER_VAR diff=t-time_of_event_acia;
          if(diff>-32000)
          {
            rectime=t;
            if(diff>0)
            {
              //TRACE_LOG("shift %d cycles\n",diff);
              time_of_event_acia=(COUNTER_VAR)t;
            }
          }
        }
        MIDIPort.MIDI_In->CycleOfLastNote=(COUNTER_VAR)rectime; 
        if(MIDI_in_speed<100)
        {
          trans_time*=100;
          trans_time/=MIDI_in_speed;
        }
        acia[ACIA_MIDI].TimeRx=time_of_event_acia+trans_time;
        if(acia[ACIA_MIDI].TimeRx-time_of_event_acia<=0)
          time_of_event_acia=acia[ACIA_MIDI].TimeRx;
      }
    }
  }
#if !defined(SSE_NO_OLD_IKBD)
  else if(MIDIPort.AreBytesToCome())
  {
    MIDIPort.NextByte();
    if(acia[ACIA_MIDI].rx_not_read)
    {
      // discard data and set overrun
      PORT_LOG("MIDI in OVR\n");
      if(acia[ACIA_MIDI].overrun!=ACIA_OVERRUN_YES)
        acia[ACIA_MIDI].overrun=ACIA_OVERRUN_COMING;
    }
    else
    {
      acia[ACIA_MIDI].data=MIDIPort.ReadByte();
      PORT_LOG2("MIDI in %X\n",acia[ACIA_MIDI].data);
      acia[ACIA_MIDI].rx_not_read=TRUE;
    }
    if(acia[ACIA_MIDI].rx_irq_enabled)
      acia[ACIA_MIDI].irq=TRUE;
    mfp_gpip_set_bit(MFP_GPIP_ACIA_BIT,!(acia[ACIA_IKBD].irq||acia[ACIA_MIDI].irq));
    if(MIDIPort.AreBytesToCome())
      agenda_add(agenda_midi_replace,ACIAClockToHBLS(acia[ACIA_MIDI].clock_divide,true),false);
  }
#endif//#if !defined(SSE_NO_OLD_IKBD)
}


void MidiInBufNotEmpty() {
  if(OPTION_C1)
  {
    if(acia[ACIA_MIDI].LineRxBusy)
      return;
    acia[ACIA_MIDI].LineRxBusy=1;
    COUNTER_VAR now=A_S_T;
    COUNTER_VAR trans_time=acia[ACIA_MIDI].TransmissionTime();
    // use timestamp to shift timing
    //TRACE_OSD("M1 %d",MIDIPort.MIDI_In->TimeSinceLastNote);
#if defined(SSE_DIRECTMIDI)
    if(MIDIPort.MIDI_In->Buf.LastTiming<MIDI_UNITS_1SEC)
#else
    if(MIDIPort.MIDI_In->TimeSinceLastNote<MIDI_UNITS_1SEC)
#endif
    {
      COUNTER_VAR CyclesSinceLastNote=(COUNTER_VAR)((MIDIPort.MIDI_In->TimeSinceLastNote*
                                        nSysCyclesPerSecond)/MIDI_UNITS_1SEC);
      COUNTER_VAR t=MIDIPort.MIDI_In->CycleOfLastNote+CyclesSinceLastNote;
      //TRACE_OSD("M1 %d",t-now);
      if(t-now>-32000)//0)
      {
        //TRACE_LOG("shift %d cycles\n",t-now);
        now=t;
      }
    }
    MIDIPort.MIDI_In->CycleOfLastNote=now;
    if(MIDI_in_speed<100)
    {
      trans_time*=100;
      trans_time/=MIDI_in_speed;
    }
    acia[ACIA_MIDI].TimeRx=now+trans_time;
    if(acia[ACIA_MIDI].TimeRx-time_of_event_acia<=0)
      time_of_event_acia=acia[ACIA_MIDI].TimeRx;
  }
#if !defined(SSE_NO_OLD_IKBD)
  else
    agenda_add(agenda_midi_replace,ACIAClockToHBLS(acia[ACIA_MIDI].clock_divide,true)+1,false); //+1 for middle of scanline
#endif
}


// Parallel Port

void ParallelInputNotify() { // function pointer is used
  agenda_add(agenda_check_centronics_interrupt,1,0);
}


void ParallelOutputNotify() { // function pointer is used
  agenda_add(agenda_check_centronics_interrupt,1,0);
}


void UpdateCentronicsBusyBit() {
  if((stick[N_JOY_PARALLEL_1]&BIT_4)==0)
  {
    bool was_already_pending=((Mfp.reg[MFPR_IPRB]&BIT_0)!=0); // centronics busy
    if(ParallelPort.IsOpen())
    {
      if(psg_reg[PSGR_MIXER]&BIT_7)
      {  // Output
        // If there are bytes being sent then input is high
        bool AreBytesToOutput=ParallelPort.AreBytesToOutput();
        mfp_gpip_set_bit(MFP_GPIP_CENTRONICS_BIT,AreBytesToOutput);
      }
      else
      {   // Input
          // If there are bytes being received then input is high
        bool AreBytesToRead=ParallelPort.AreBytesToRead();
        mfp_gpip_set_bit(MFP_GPIP_CENTRONICS_BIT,AreBytesToRead);
      }
    }
    else
    {
      mfp_gpip_set_bit(MFP_GPIP_CENTRONICS_BIT,true); // Always busy if port closed
    }
    bool ispending=((Mfp.reg[MFPR_IPRB]&BIT_0)!=0); // centronics busy
/*  On the STF, Centronics busy is also connected to Timer A input, for some reason
    (On the STE, it is DMA sound)*/
    if(IS_STF && ispending && !was_already_pending // something like that
      &&Mfp.GetTimerControlRegister(MFP_TIMER_A)==MFP_TIMER_EVENT_COUNT)
    {
      Mfp.TimerATick();
    }
  }
}


void agenda_check_centronics_interrupt(int) {
  UpdateCentronicsBusyBit();
  if(ParallelPort.AreBytesToRead()||ParallelPort.AreBytesToOutput())
    agenda_add(agenda_check_centronics_interrupt,2,0);
}


// Serial Port

void agenda_serial_replace(int) {
  if(UpdateBaud) 
    RS232_CalculateBaud(Mfp.GetTimerControlRegister(MFP_TIMER_D),true);
  if(SerialPort.AreBytesToCome())
  {
    SerialPort.NextByte();
    if((Mfp.reg[MFPR_RSR]&BIT_0) /*Recv enable*/&&
      (Mfp.reg[MFPR_TSR]&b00000110)!=b00000110 /*Loopback*/&&
      (Mfp.reg[MFPR_RSR]&BIT_6)==0 /*No overrun*/)
    {
      if((Mfp.reg[MFPR_RSR]&BIT_7 /*Buffer Full*/)==0)
      {
        rs232_recv_byte=SerialPort.ReadByte();
        rs232_recv_overrun=false;
      }
      else
        rs232_recv_overrun=true;
      Mfp.reg[MFPR_RSR]&=(~(BIT_2 /*Char in progress*/|BIT_3 /*Break*/|
        BIT_4 /*Frame Error*/|BIT_5 /*Parity Error*/));
      Mfp.reg[MFPR_RSR]|=BIT_7 /*Buffer Full*/;
      Mfp.GetInterrupt(MFP_INT_RS232_RECEIVE_BUFFER_FULL,ABSOLUTE_SYS_TIME);
    }
    if(SerialPort.AreBytesToCome())
    {
      Mfp.reg[MFPR_RSR]|=BIT_2; // Character in progress
      agenda_add(agenda_serial_replace,rs232_hbls_per_word,0);
    }
    else
      Mfp.reg[MFPR_RSR]&=~BIT_2; // Character in progress
  }
}


void SerialInBufNotEmpty() {
  Mfp.reg[MFPR_RSR]|=BIT_2; // Character in progress
  agenda_add(agenda_serial_replace,rs232_hbls_per_word+1,0); //+1 for middle of scanline
}


TSTPort::TSTPort() {
  MIDI_Out=NULL;
  MIDI_In=NULL;
  PCPort=PCPortIn=NULL;
  fp=NULL;
  LoopBuf=NULL;
  Type=PORTTYPE_NONE;

#if defined(SSE_NETWORK)
  IPPort=DEFAULT_IP_PORT;
  ListenSocket=INVALID_SOCKET;
  for(int i=0;i<MAXCLIENT_SOCKETS;i++)
    Socket[i]=INVALID_SOCKET;
  Connected=NOTCONNECTED;
  nSockets=0;
  MaxClients=1;
  ByteComing=false;
#endif

#ifdef WIN32
#if defined(SSE_DIRECTMIDI)
  MIDIOutDevice=(OPTION_DIRECTMUSIC)?(-1):(-2);
#else
  MIDIOutDevice=-2;
#endif
  MIDIInDevice=-1;
  COMNum=0;LPTNum=0;
#endif
#ifdef UNIX
  PortDev[TPORTIO_TYPE_SERIAL]="/dev/ttyS0";
  PortDev[TPORTIO_TYPE_PARALLEL]="/dev/lp0";
  PortDev[TPORTIO_TYPE_MIDI]="/dev/midi";
  PortDev[TPORTIO_TYPE_UNKNOWN]="/dev/null";
  for(int n=0;n<TPORTIO_NUM_TYPES;n++)
  {
    AllowIO[n][0]=true;
    AllowIO[n][1]=0;
    if(n==TPORTIO_TYPE_SERIAL||n==TPORTIO_TYPE_MIDI)
      // Input only works on these
      AllowIO[n][1]=true;
  }
#endif
  Id=0;
}


// returns true if OK
bool TSTPort::Create(BYTE id,Str &ErrorText,Str &ErrorTitle,bool DoAlert) {
  Close();
  Id=id;
  bool Running=(runstate==RUNSTATE_RUNNING);
  LPPORTIOINFIRSTBYTEPROC FirstByteInProc=NULL;
  LPPORTIOOUTFINISHEDPROC LastByteOutProc=NULL;
  if(&MIDIPort==this)
    FirstByteInProc=MidiInBufNotEmpty;
  else if(&ParallelPort==this)
  {
    FirstByteInProc=ParallelInputNotify;
    LastByteOutProc=ParallelOutputNotify;
  }
  else if(&SerialPort==this)
    FirstByteInProc=SerialInBufNotEmpty;
  bool Error=false;
  switch(Type) {
  case PORTTYPE_FILE:
    fp=fopen(File,"ab"); // can be NULL
    return true; //??
  case PORTTYPE_LOOP:
    LoopBuf=new CircularBuffer(PORT_LOOPBUFSIZE);
    return true;
  }
#ifdef WIN32
  EasyStr PortName=EasyStr("COM")+(COMNum+1);
  bool AllowIn=true,AllowOut=true;
  switch(Type) {
  case PORTTYPE_MIDI:
  {
    bool MIDIOutErr=false,MIDIInErr=false;
#if defined(SSE_DIRECTMIDI)
    if(OPTION_DIRECTMUSIC&&MIDIOutDevice>-1||!OPTION_DIRECTMUSIC&&MIDIOutDevice>-2)
#else
    if(MIDIOutDevice>-2)
#endif
    {
      MIDI_Out=new TMIDIOut(MIDIOutDevice,(Running ? MIDI_out_volume : 0));
      if(!MIDI_Out->IsOpen())
        MIDIOutErr=true;
    }
    if(MIDIInDevice>-1)
    {
      MIDI_In=new TMIDIIn(MIDIInDevice,Running,FirstByteInProc);
      if(!MIDI_In->IsOpen())
        MIDIInErr=true;
    }
    if(MIDIInErr && MIDIOutErr)
    {
      ErrorTitle=T("MIDI Errors");
      ErrorText=T("MIDI Output Error")+"\n"+MIDI_Out->ErrorText+"\n\n";
      ErrorText+=T("MIDI Input Error")+"\n"+MIDI_In->ErrorText;
    }
    else if(MIDIInErr)
    {
      ErrorTitle=T("MIDI Input Error");
      ErrorText=MIDI_In->ErrorText;
    }
    else if(MIDIOutErr)
    {
      ErrorTitle=T("MIDI Output Error");
      ErrorText=MIDI_Out->ErrorText;
    }
    if(MIDIOutErr) 
    {
      delete MIDI_Out; 
      MIDI_Out=NULL;
    }
    if(MIDIInErr)
    { 
      delete MIDI_In; 
      MIDI_In=NULL;
    }
    Error=(MIDIOutErr||MIDIInErr);
    break;
  }
  case PORTTYPE_PARALLEL:
    PortName=EasyStr("LPT")+(LPTNum+1);
    AllowIn=comline_allow_LPT_input;
  case PORTTYPE_COM:
    PCPort=new TPortIO(PortName,AllowIn,AllowOut);
    break;
  }
#endif
#ifdef UNIX
  int PortIOType=GetPortIOType(Type);
  if(PortIOType>=0)
  {
    if(Type==PORTTYPE_LAN)
    {
      PCPort=new TPortIO(PortDev[PortIOType],true,0,PortIOType);
      PCPortIn=new TPortIO(LANPipeIn,0,true,PortIOType);
    }
    else
      PCPort=new TPortIO(PortDev[PortIOType],AllowIO[PortIOType][0],AllowIO[PortIOType][1],PortIOType);
  }
#endif
  if(PCPort)
  {
    if(!PCPort->IsOpen())
      Error=true;
    if(PCPortIn) 
      if(!PCPortIn->IsOpen())
        Error=true;
    if(Error)
    {
      ErrorTitle=T("Port Error");
#ifdef WIN32
      ErrorText=T("Could not open port ")+PortName+". "+
        T("It may not exist or it could be in use by another program.");
#else
      Str BadDev;
      ErrorText="";
      if(PCPortIn)
      {
        if(PCPort->IsOpen()==0)
        {
          BadDev=PortDev[PortIOType];
        }
        else
        {
          BadDev=LANPipeIn;
        }
      }
      else if(AllowIO[PortIOType][0]||AllowIO[PortIOType][1])
      {
        BadDev=PortDev[PortIOType];
      }
      if(BadDev.NotEmpty())
      {
        ErrorText=T("Could not open device")+" "+BadDev+"\n\n"+
          T("It may not exist, it could be in use by another program or you may not have permission to access it.");
      }
#endif
      delete PCPort;
      PCPort=NULL;
      if(PCPortIn) 
        delete PCPortIn;
      PCPortIn=NULL;
    }
    else
    {
      PCPort->lpInFirstByteProc=FirstByteInProc;
      PCPort->lpOutFinishedProc=LastByteOutProc;
      PCPort->OutPause=!Running;
      PCPort->InPause=!Running;
      if(PCPortIn)
      {
        PCPortIn->lpInFirstByteProc=FirstByteInProc;
        PCPortIn->InPause=!Running;
      }
    }
  }

#if defined(SSE_NETWORK)
  if(Type==PORTTYPE_TCPIP)
  {
    if(sIPAddr.NotEmpty())
    {
      // set up connection thread (so Steem won't freeze if there's no connection)
      DWORD ThreadId;
      HANDLE hThread=CreateThread(NULL,0,ConnectThreadProc,(PVOID)id,0,&ThreadId);
      PORT_LOG("Port %d start connection thread %p ID %d\n",id,hThread,ThreadId);
      // set up receive thread
      hThread=CreateThread(NULL,0,ReceiveThreadProc,(PVOID)id,0,&ThreadId);
      PORT_LOG("Port %d start receive thread %p ID %d\n",id,hThread,ThreadId);
    }
  }
#endif

  if(Running)
  {
    if(this==&ParallelPort)
      UpdateCentronicsBusyBit();
    else if(this==&SerialPort)
    {
      SetDTR((psg_reg[PSGR_PORT_A]&BIT_4)!=0);
      SetRTS((psg_reg[PSGR_PORT_A]&BIT_3)!=0);
    }
  }
  if(DoAlert && Error && ErrorText.NotEmpty())
    Alert(ErrorText,ErrorTitle,MB_ICONEXCLAMATION|MB_OK);
  CheckResetDisplay();
  return !Error;
}


bool TSTPort::OutputByte(BYTE Byte) {
#if defined(SSE_STATS)
  //ASSERT(Id<3);
  Stats.nPorto[Id]++;
#endif
  if(MIDI_Out)
    MIDI_Out->SendByte(Byte);
  if(fp)
    FPUTC(Byte,fp);
#if defined(SSE_PRINTER)
  if(Id==PARALLEL&&OPTION_PRINTER)
    Printer.HandleByte(Byte);
#endif
  if(PCPort)
    return PCPort->OutputByte(Byte);
  if(LoopBuf)
  {
    LPPORTIOINFIRSTBYTEPROC FirstByteProc=NULL;
    if(&MIDIPort==this)
      FirstByteProc=MidiInBufNotEmpty;
    else if(&SerialPort==this)
      FirstByteProc=SerialInBufNotEmpty;
    bool FirstByte=!LoopBuf->AreBytesInBuffer();
    bool RetVal=LoopBuf->AddByte(Byte);
    if(FirstByte && FirstByteProc) 
      FirstByteProc();
    return RetVal;
  }

#if defined(SSE_NETWORK)
  if(Type==PORTTYPE_TCPIP)
  {
    for(int i=0;i<nSockets;i++) // send to all sockets (1 if we're Client)
    {
      if(Socket[i]!=INVALID_SOCKET)
      {
        char sendbuf[DEFAULT_BUFLEN];
        sendbuf[0]=Byte;
        int iResult=send(Socket[i],sendbuf,1,0);
        bool ok=(iResult!=SOCKET_ERROR);
#if defined(SSE_ENABLE_TRACE_LOG)
        if(ok)
          PORT_LOG2("Port %d socket%d output %X\n",Id,i,Byte);
        else
          PORT_LOG("Port %d socket%d error %d\n",Id,i,iResult);
#endif
        return ok;
      }
    }
  }
#endif

  return true;
}


bool TSTPort::AreBytesToOutput() {
  if(PCPort) 
    return PCPort->AreBytesToOutput();
  return false; // Instant output
}


void TSTPort::StartOutput() { // only called by PortsRunStart(), which is called by run()
  if(MIDI_Out)
  {
#if defined(SSE_DIRECTMIDI)
    if(!OPTION_DIRECTMUSIC)
#endif
    MIDI_Out->SetVolume(MIDI_out_volume);
    MIDI_Out->TimeLastSent=timer; // as just updated in run()
  }
  if(PCPort)
    PCPort->OutPause=false;
}


void TSTPort::StopOutput() { // only called by PortsRunEnd(), which is called by run()
#if defined(SSE_DIRECTMIDI)
  if(!OPTION_DIRECTMUSIC)
#endif
  if(MIDI_Out) 
    MIDI_Out->SetVolume(0);
  if(PCPort)
    PCPort->OutPause=true;
}


void TSTPort::Reset() { // only called by reset_peripherals(bool Cold)
  if(MIDI_Out) 
    MIDI_Out->Reset();
  if(MIDI_In)
    MIDI_In->Reset();
  if(LoopBuf) 
    LoopBuf->Reset();
}


void TSTPort::StartInput() { // only called by PortsRunStart(), which is called by run()
  if(MIDI_In) 
    MIDI_In->Start();
  if(PCPortIn)
    PCPortIn->InPause=false;
  else if(PCPort)
    PCPort->InPause=false;
}


void TSTPort::StopInput() { // only called by PortsRunEnd(), which is called by run()
  if(MIDI_In) 
    MIDI_In->Stop();
  if(PCPortIn)
    PCPortIn->InPause=true;
  else if(PCPort)
    PCPort->InPause=true;
}


bool TSTPort::AreBytesToRead() {
  if(MIDI_In) 
    return MIDI_In->AreBytesToCome();
  if(PCPortIn) 
    return PCPortIn->AreBytesToRead();
  if(PCPort) 
    return PCPort->AreBytesToRead();
  if(LoopBuf) 
    return LoopBuf->AreBytesInBuffer();
#if defined(SSE_NETWORK)
  if(ByteComing) // detected by polling thread
    return true;
#endif
  return false;
}


void TSTPort::NextByte() { // input
  if(MIDI_In) 
    MIDI_In->NextByte();
  if(PCPortIn) 
    PCPortIn->NextByte();
  if(PCPort) 
    PCPort->NextByte();
  if(LoopBuf) 
    LoopBuf->NextByte();
}


BYTE TSTPort::ReadByte() {
#if defined(SSE_STATS)
  //ASSERT(Id<3);
  Stats.nPorti[Id]++;
#endif
  if(MIDI_In) 
    return MIDI_In->ReadByte();
  if(PCPortIn) 
    return PCPortIn->ReadByte();
  if(PCPort) 
    return PCPort->ReadByte();
  if(LoopBuf) 
    return LoopBuf->ReadByte();

#if defined(SSE_NETWORK)
  if(Type==PORTTYPE_TCPIP)
  {
    for(int i=0;i<nSockets;i++)
    {
      if(Socket[i]!=INVALID_SOCKET)
      {
        char recvbuf[DEFAULT_BUFLEN];

        // or should we use an intermediate buffer?
        int iResult = recv(Socket[i],recvbuf,1,0);
        ByteComing=false;

        if(iResult==1)
        {
          // server sends received byte to other clients (simulate MIDI?)
          for(int j=0;j<nSockets;j++)
          {
            if(i!=j)
            {
              if(Socket[j]!=INVALID_SOCKET)
              {
                iResult=send(Socket[j],recvbuf,1,0);
                bool ok=(iResult!=SOCKET_ERROR);
#if defined(SSE_ENABLE_TRACE_LOG)
                if(!ok)
                  PORT_LOG("Port %d server output socket%d error %d\n",Id,j,iResult);
#endif
              }
            }
          }
          PORT_LOG2("Port %d socket%d input %X\n",Id,i,recvbuf[0]);
          return (BYTE)recvbuf[0]; // first client with byte is served
        }
#if defined(SSE_ENABLE_TRACE_LOG)
        else
          PORT_LOG("Port %d socket%d error %d\n",Id,i,iResult);
#endif
      }
    }//nxt i
  }
#endif//#if defined(SSE_NETWORK)

  return 0;
}


void TSTPort::SetupCOM(int BaudRate,bool bXOn_XOff,int RTS,int DTR,bool bParity,
                       BYTE ParityType,BYTE StopBits,BYTE WordLength) {
  if(Type==PORTTYPE_COM && PCPort)
    PCPort->SetupCOM(BaudRate,bXOn_XOff,RTS,DTR,bParity,ParityType,StopBits,WordLength);
}


DWORD TSTPort::GetModemFlags() {
  if(PCPort) 
    return PCPort->GetModemFlags();
  if(IsOpen()) 
    return MS_CTS_ON; // Clear to send
  return 0;
}


bool TSTPort::SetDTR(bool Val) {
#if defined(SSE_DONGLE_MUSIC_MASTER)
  if(DONGLE_ID==TDongle::MUSIC_MASTER)
  { //record old value, new value and timing
    Dongle.Value=((Dongle.Value<<1)|(int)Val)&3; //old - new
    Dongle.Timing=A_S_T;
  }
#endif
  if(PCPort) 
    return PCPort->SetDTR(Val);
  return false;
}


bool TSTPort::SetRTS(bool Val) {
  if(PCPort) 
    return PCPort->SetRTS(Val);
  return false;
}


bool TSTPort::StartBreak() {
  if(PCPort) 
    return PCPort->StartBreak();
  return false;
}


bool TSTPort::EndBreak() {
  if(PCPort) 
    return PCPort->EndBreak();
  return false;
}


#ifdef WIN32

int TSTPort::GetMIDIOutDeviceID() {
  if(MIDI_Out) 
    return MIDI_Out->GetDeviceID();
  return -999; //TODO refactor
}


int TSTPort::GetMIDIInDeviceID() {
  if(MIDI_In) 
    return MIDI_In->GetDeviceID();
  return -999;
}

#endif


void TSTPort::Close() {
  if(MIDI_Out)
    delete MIDI_Out; 
  MIDI_Out=NULL;
  if(MIDI_In)
    delete MIDI_In; 
  MIDI_In=NULL;
  if(PCPort)
    delete PCPort; 
  PCPort=NULL;
  if(PCPortIn)
    delete PCPortIn; 
  PCPortIn=NULL;
  if(fp)
  {
#if defined(SSE_PRINTER)
    if(Id==PARALLEL)
      Printer.Close();
#endif
    fclose(fp); 
  }
  fp=NULL;
  if(LoopBuf)
    delete LoopBuf; 
  LoopBuf=NULL;

#if defined(SSE_NETWORK)
  if(ListenSocket!=INVALID_SOCKET)
    closesocket(ListenSocket);
  for(int i=0;i<nSockets;i++)
  {
    if(Socket[i]!=INVALID_SOCKET)
    {
      // shutdown the connection since no more data will be sent
      int iResult = shutdown(Socket[i],SD_SEND);
      if(iResult == SOCKET_ERROR) {
        PORT_LOG("Port %d shutdown socket%d failed with error: %d\n",Id,i,WSAGetLastError());
      }
      closesocket(Socket[i]);
    }
    Socket[i]=INVALID_SOCKET;
  }
  nSockets=0;
  ListenSocket=INVALID_SOCKET;
  Connected=NOTCONNECTED;
  ByteComing=false;
#endif

  if(runstate==RUNSTATE_RUNNING &&this==&ParallelPort) 
    UpdateCentronicsBusyBit();
  CheckResetDisplay();
}


void PortsRunStart() { // only called at start of run()
  MIDIPort.StartInput();     
  MIDIPort.StartOutput();
  ParallelPort.StartInput(); 
  ParallelPort.StartOutput();
  SerialPort.StartInput();   
  SerialPort.StartOutput();
  RS232_CalculateBaud(Mfp.GetTimerControlRegister(MFP_TIMER_D),true);
  // Update external devices (Could have changed while stopped or reset may have happened)
  SerialPort.SetDTR((psg_reg[PSGR_PORT_A]&BIT_4)!=0);
  SerialPort.SetRTS((psg_reg[PSGR_PORT_A]&BIT_3)!=0);
  RS232_VBL(); //?
  UpdateCentronicsBusyBit();
}


void PortsRunEnd() { // only called at end of run()
  MIDIPort.StopInput();     
  MIDIPort.StopOutput();
  ParallelPort.StopInput(); 
  ParallelPort.StopOutput();
  SerialPort.StopInput();   
  SerialPort.StopOutput();
}


void PortsOpenAll() {  // only called by LoadAllDialogData()
  //TRACE_INIT("PortsOpenAll\n");
#ifndef ONEGAME
  Str ErrorText,ErrorTitle;
  MIDIPort.Create(TSTPort::MIDI,ErrorText,ErrorTitle,true);
  ParallelPort.Create(TSTPort::PARALLEL,ErrorText,ErrorTitle,true);
  SerialPort.Create(TSTPort::SERIAL,ErrorText,ErrorTitle,true);
#ifdef UNIX
  XGUIUpdatePortDisplay();
#endif
#endif
}


#if defined(SSE_NETWORK)

// thread to start internet connection, necessary because there's no timeout 
// and we don't always know how many clients the server will handle
// pParam is the port number
DWORD WINAPI ConnectThreadProc(PVOID pParam) {

  INT_PTR nPort=(INT_PTR)pParam;
  TSTPort& Port=STPort[nPort]; // shorthand reference

  ASSERTB(Port.Connected==TSTPort::NOTCONNECTED);
  Port.Connected=TSTPort::CONNECTING;
  // this is based on a MSDN example
  struct addrinfo* result = NULL;
  struct addrinfo* ptr = NULL;
  struct addrinfo hints;
  ZeroMemory(&hints,sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  int iResult;
  char sPort[16];
  sprintf(sPort,"%d",Port.IPPort);
  if(!strcmpi(Port.sIPAddr.Text,"SERVER"))
  {
    PORT_LOG("Port %d %s\n",nPort,"SERVER");

    hints.ai_family = AF_INET;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL,sPort,&hints,&result);
    if(iResult != 0) {
      PORT_LOG("getaddrinfo failed with error: %d\n",iResult);
      return (DWORD)-1;
    }

    OptionBox.PortsMakeTypeVisible((int)nPort);

    // Create a SOCKET for connecting to server
    Port.ListenSocket = socket(result->ai_family,result->ai_socktype,result->ai_protocol);
    if(Port.ListenSocket == INVALID_SOCKET) {
      PORT_LOG("socket failed with error: %ld\n",WSAGetLastError());
      freeaddrinfo(result);
      return (DWORD)-1;
    }

    // Setup the TCP listening socket
    iResult = bind(Port.ListenSocket,result->ai_addr,(int)result->ai_addrlen);
    if(iResult == SOCKET_ERROR) {
      PORT_LOG("bind failed with error: %d\n",WSAGetLastError());
      freeaddrinfo(result);
      closesocket(Port.ListenSocket);
      Port.ListenSocket=INVALID_SOCKET;
      return (DWORD)-1;
    }

    freeaddrinfo(result);

    // take up to 16 clients
    for(Port.nSockets=0;Port.nSockets<Port.MaxClients;Port.nSockets++)
    {
      // must be authorized by firewall!
      iResult = listen(Port.ListenSocket,1/*SOMAXCONN*/);
      if(iResult == SOCKET_ERROR) {
        PORT_LOG("listen failed with error: %d\n",WSAGetLastError());
        if(Port.ListenSocket!=INVALID_SOCKET)
          closesocket(Port.ListenSocket);
        Port.ListenSocket=INVALID_SOCKET;
        return (DWORD)-1;
      }

      // Accept a client socket
      Port.Socket[Port.nSockets] = accept(Port.ListenSocket,NULL,NULL);
      if(Port.Socket[Port.nSockets] == INVALID_SOCKET) {
        PORT_LOG("accept failed with error: %d\n",WSAGetLastError());
        closesocket(Port.ListenSocket);
        Port.ListenSocket=INVALID_SOCKET;
        return (DWORD)-1;
      }
      Port.Connected=TSTPort::CONNECTED;
      PORT_LOG("Connected to %s #%d\n","CLIENT",Port.nSockets);
    }//nxt

    // No longer need server socket
    if(Port.ListenSocket!=INVALID_SOCKET)
      closesocket(Port.ListenSocket);
    Port.ListenSocket=INVALID_SOCKET;

  }
  else if(Port.sIPAddr.IsNotEmpty())
  {
    PORT_LOG("Port %d %s\n",nPort,"CLIENT");

    OptionBox.PortsMakeTypeVisible((int)nPort);

    hints.ai_family = AF_UNSPEC;

    // Resolve the server address and port
    iResult = getaddrinfo(Port.sIPAddr.Text,sPort,&hints,&result);
    if(iResult != 0) {
      PORT_LOG("getaddrinfo failed with error: %d\n",iResult);
      Port.Connected=TSTPort::NOTCONNECTED;
      OptionBox.PortsMakeTypeVisible((int)nPort);
      return (DWORD)-1;
    }

    // Attempt to connect to an address until one succeeds
    for(ptr=result; ptr != NULL ;ptr=ptr->ai_next) {

      // Create a SOCKET for connecting to server
      Port.Socket[0] = socket(ptr->ai_family,ptr->ai_socktype,
        ptr->ai_protocol);
      if(Port.Socket[0] == INVALID_SOCKET) {
        PORT_LOG("socket failed with error: %ld\n",WSAGetLastError());
        return (DWORD)-1;
      }

      // Connect to server.
      iResult = connect(Port.Socket[0],ptr->ai_addr,(int)ptr->ai_addrlen);
      if(iResult == SOCKET_ERROR) {
        if(Port.Socket[0]!=INVALID_SOCKET)
          closesocket(Port.Socket[0]);
        Port.Socket[0] = INVALID_SOCKET;
        continue;
      }
      Port.nSockets=1;
      Port.Connected=TSTPort::CONNECTED;
      PORT_LOG("Connected to %s\n","SERVER");
      break;
    }
  }
  if(Port.Connected!=TSTPort::CONNECTED)
    Port.Connected=TSTPort::NOTCONNECTED;
  OptionBox.PortsMakeTypeVisible((int)nPort);
  PORT_LOG("exiting ConnectThreadProc for port %d\n",nPort);
  return 0;
}


// TCP input polling thread - is there a better way?
// when a new byte has been received, set flag and update emulation state
// pParam is the port number
DWORD WINAPI ReceiveThreadProc(PVOID pParam) {

  INT_PTR nPort=(INT_PTR)pParam;
  TSTPort& Port=STPort[nPort]; // shorthand reference
  while(Port.Connected==TSTPort::CONNECTING)
    SwitchToThread();
  while(Port.Connected==TSTPort::CONNECTED)
  {
    for(int i=0;i<Port.nSockets;i++)
    {
      if(Port.Socket[i]!=INVALID_SOCKET)
      {
      //  valid=TRUE;
        if(!Port.ByteComing) // only one thread at a time should try to receive, even if peek
        {
          char recvbuf[DEFAULT_BUFLEN];
          // MSG_PEEK: The data is copied into the buffer, but is not removed from the input queue.
          int iResult = recv(Port.Socket[i],recvbuf,1,MSG_PEEK);
          if(iResult>=1) // >1 is trouble TODO
          {
            PORT_LOG2("Port %d internet socket%d %d bytes coming\n",nPort,i,iResult);
            switch(nPort) {
            case TSTPort::MIDI:
              MidiInBufNotEmpty(); // this will set up an agenda or an event
              break;
            case TSTPort::PARALLEL:
              ParallelInputNotify(); // this will set up an agenda
              break;
            case TSTPort::SERIAL:
              SerialInBufNotEmpty(); // this will set up an agenda
              break;
            }//sw
            Port.ByteComing=true;
          }
        }
      }
    }
    // note Sleep(0) won't prevent CPU hog, Sleep(1) is better but SwitchToThread is best
    SwitchToThread();
  }//wend
  PORT_LOG("exiting ReceiveThreadProc for port %d\n",nPort); 
  return 0;
}

#undef DEFAULT_BUFLEN

#endif//#if defined(SSE_NETWORK)

#undef LOGSECTION
#undef PORT_LOG
#undef PORT_LOG2
