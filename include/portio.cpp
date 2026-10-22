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

DOMAIN: IO
FILE: portio.cpp
DESCRIPTION: Cross-platform direct port input and output class.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#define SSE_411R7 // keep old code too for a while...

#include "portio.h"
#include "circularbuffer.h"

#ifdef UNIX
#include <pthread.h>
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include "x/x_mymisc.h"
#endif
#ifndef TPORTIO_BUF_SIZE
#define TPORTIO_BUF_SIZE 8192
#endif

#pragma warning (disable : 4996)

#ifdef WIN32
bool TPortIO::AlwaysUseNTMethod=true;
#endif

TPortIO::TPortIO(char *Port,bool AllowIn,bool AllowOut
#ifdef UNIX
                  ,int PortType
#endif
																				) {
#ifdef WIN32
  hCom=NULL;
  hInThread=NULL;
  hOutThread=NULL;
  if(AlwaysUseNTMethod)
    WinNT=true;
  else
  {
    OSVERSIONINFO osvi;
    osvi.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
    GetVersionEx(&osvi);
    WinNT=(osvi.dwPlatformId==VER_PLATFORM_WIN32_NT);
  }
  pCancelIOProc=NULL;
  if((hKern32=LoadLibrary("kernel32"))!=NULL)
    pCancelIOProc=(LPCANCELIOPROC)GetProcAddress(hKern32,"CancelIo");
  if(hKern32==NULL||pCancelIOProc==NULL)
    WinNT=false;
  if(WinNT)
  {
    hInEvent=CreateEvent(NULL,true,0,NULL);
    hOutEvent=CreateEvent(NULL,true,0,NULL);
    ZeroMemory(&InOverlapStruct,sizeof(OVERLAPPED));
    InOverlapStruct.hEvent=hInEvent;
    lpInOverlapStruct=&InOverlapStruct;
    ZeroMemory(&OutOverlapStruct,sizeof(OVERLAPPED));
    OutOverlapStruct.hEvent=hOutEvent;
    lpOutOverlapStruct=&OutOverlapStruct;
  }
  else
  {
    hInEvent=hOutEvent=NULL;
    lpInOverlapStruct=NULL;
    lpOutOverlapStruct=NULL;
  }
#endif
#ifdef UNIX
  iCom=-1;
  iInThread=0;
  iOutThread=0;
#endif
  InThreadClosed=OutThreadClosed=true;
  Outputting=Closing=OutPause=InPause=false;
  OutCount=InCount=0;
  lpInFirstByteProc=NULL;
  lpOutFinishedProc=NULL;
  if(Port)
  {
#ifdef WIN32
    Open(Port,AllowIn,AllowOut);
#endif
#ifdef UNIX
    Open(Port,AllowIn,AllowOut,PortType);
#endif
  }
}


TPortIO::~TPortIO() {
  Close();
#ifdef WIN32
  if(hInEvent) 
    CloseHandle(hInEvent);
  hInEvent=NULL;
  if(hOutEvent) 
    CloseHandle(hOutEvent);
  hOutEvent=NULL;
#endif
}


bool TPortIO::OutputByte(BYTE Byte) {
#ifdef WIN32
  if(hCom==NULL)
    return false;
#endif
#ifdef UNIX
  if(iCom==-1)
    return false;
#endif
  bool NoOverflow=OutBuf.AddByte(Byte);
  if(!Outputting)
  {
    // When outputting ends (or at the start) the byte read
    // is the old one, must advance
    OutBuf.NextByte();
    Outputting=true;
#ifdef WIN32
#if defined(SSE_411R7)
    bOutFinishedProcCalled=false;
#else
    ResumeThread(hOutThread);
#endif
#endif
#ifdef UNIX
    pthread_mutex_lock(&OutWaitMutex);
    pthread_cond_signal(&OutWaitCond);
    pthread_mutex_unlock(&OutWaitMutex);
#endif
  }
  return NoOverflow;
}


bool TPortIO::OutputString(char *Str) {
#ifdef WIN32
  if(hCom==NULL)
    return false;
#endif
#ifdef UNIX
  if(iCom==-1)
    return false;
#endif
  int Len=(int)strlen(Str);
  for(int n=0;n<Len;n++)
    if(!OutputByte(Str[n])) 
      return false;
  return true;
}


void TPortIO::Close() {
#ifdef WIN32
  if(hCom==NULL)
    return;
#endif
#ifdef UNIX
  if(iCom==-1)
    return;
#endif
  Closing=true;
#ifdef WIN32
  if(WinNT && pCancelIOProc) 
    pCancelIOProc(hCom); // Cancel all current writes or reads
#if !defined(SSE_411R7)
  // Make sure this dies quickly
  if(hInThread) 
    SetThreadPriority(hInThread,THREAD_PRIORITY_HIGHEST);
#endif
#endif
  /*
  Consider using 'GetTickCount64' instead of 'GetTickCount'. Reason: 
  GetTickCount overflows roughly every 49 days.  Code that does not 
  take that into account can loop indefinitely.  GetTickCount64 operates 
  on 64 bit values and does not have that problem
  */
#if defined(SSE_WINDOWS_XP_MAX) || defined(SSE_UNIX)
  DWORD TimeOut=GetTickCount()+750;
#else
  ULONGLONG TimeOut=GetTickCount64()+750; //402R6
#endif
  while(!InThreadClosed ||!OutThreadClosed)
  {
    if(!OutThreadClosed)
    {
      // Wake up output thread, just in case it decided to sleep
#ifdef WIN32
#if !defined(SSE_411R7)
      if(hOutThread) 
        ResumeThread(hOutThread);
#endif
#endif
#ifdef UNIX
      if(!OutThreadClosed)
      {
        pthread_mutex_lock(&OutWaitMutex);
        pthread_cond_signal(&OutWaitCond);
        pthread_mutex_unlock(&OutWaitMutex);
      }
#endif
    }
    Sleep(2);
#if defined(SSE_WINDOWS_XP_MAX) || defined(SSE_UNIX)
    if(GetTickCount()>TimeOut) 
      break;
#else
    if(GetTickCount64()>TimeOut) 
      break;
#endif
  }
#ifdef WIN32
  if(hInThread)
  {
    if(!InThreadClosed)
      TerminateThread(hInThread,0);
    CloseHandle(hInThread);
  }
  hInThread=NULL;
  if(hOutThread)
  {
    InThreadClosed=true;
    if(!OutThreadClosed)
      TerminateThread(hOutThread,0);
    CloseHandle(hOutThread);
  }
  hOutThread=NULL;
  OutThreadClosed=true;
  if(hCom)
  {
    PurgeComm(hCom,PURGE_TXABORT|PURGE_RXABORT|PURGE_TXCLEAR|PURGE_RXCLEAR);
    CloseHandle(hCom);hCom=NULL;
  }
  if(hKern32) 
    FreeLibrary(hKern32);
  hKern32=NULL;
  pCancelIOProc=NULL;
#endif
#ifdef UNIX
  if(!InThreadClosed)
    pthread_cancel(iInThread);
  iInThread=0;InThreadClosed=true;
  if(iOutThread)
  {
    if(OutThreadClosed==0) pthread_cancel(iOutThread);
    pthread_cond_destroy(&OutWaitCond);
    pthread_mutex_destroy(&OutWaitMutex);
  }
  iOutThread=0;OutThreadClosed=true;
  if(iCom!=-1)
  {
#ifdef TCIFLUSH
    tcflush(iCom,TCIFLUSH);
#endif
#ifdef TCOFLUSH
    tcflush(iCom,TCOFLUSH);
#endif
    close(iCom);
  }
#endif
  Closing=false;
  InpBuf.Destroy();
  InCount=0;
  OutBuf.Destroy();
  Outputting=false;
  OutCount=0;
}


#ifdef WIN32

int TPortIO::Open(char *PortName,bool AllowIn,bool AllowOut) {
  if(hCom) 
    Close();
  DWORD Flags=0;
  if(WinNT) 
    Flags=FILE_FLAG_OVERLAPPED;
  hCom=CreateFile(PortName,GENERIC_READ|GENERIC_WRITE,0,NULL,
    OPEN_EXISTING,Flags,NULL);
  if(hCom==INVALID_HANDLE_VALUE)
  {
    hCom=NULL;
    return 1;
  }
  COMMTIMEOUTS ct={0,1,200,1,200}; //200 milliseconds
  SetCommTimeouts(hCom,&ct);
  SetupCOM(115200,0,RTS_CONTROL_DISABLE,DTR_CONTROL_DISABLE,0,NOPARITY,
    ONESTOPBIT,8); //TODO mimic ST
  if(!InpBuf.Create(TPORTIO_BUF_SIZE))
  {
    Close();
    return 1;
  }
  if(!OutBuf.Create(TPORTIO_BUF_SIZE))
  {
    Close();
    return 1;
  }
  DWORD Id;
  if(AllowIn)
  {
    InThreadClosed=false;
    hInThread=CreateThread(NULL,0,InThreadEntryPoint,this,0,&Id);
    if(hInThread==NULL)
    {
      Close();
      return 1;
    }
#if !defined(SSE_411R7)
    SetThreadPriority(hInThread,THREAD_PRIORITY_NORMAL);
#endif
  }
  if(AllowOut)
  {
    OutThreadClosed=false;
#if defined(SSE_411R7)
    bOutFinishedProcCalled=false;
    hOutThread=CreateThread(NULL,0,OutThreadEntryPoint,this,0,&Id);
    if(hOutThread==NULL)
    {
      Close();
      return 1;
    }
#else
    hOutThread=CreateThread(NULL,0,OutThreadEntryPoint,this,CREATE_SUSPENDED,
      &Id);
    if(hOutThread==NULL)
    {
      Close();
      return 1;
    }
    SetThreadPriority(hOutThread,THREAD_PRIORITY_HIGHEST);
#endif
  }
  return 0;
}


DWORD CALLBACK TPortIO::InThreadEntryPoint(void *t) {
  TPortIO *This=(TPortIO*)t;
  DWORD BytesRead;
  BYTE TempIn;
  while(!This->Closing)
  {
    if(!This->InPause)
    {
      BytesRead=0;
      if(This->WinNT) 
        ResetEvent(This->hInEvent);
#if defined(SSE_411R7)
      BOOL ok=
#endif
      ReadFile(This->hCom,&TempIn,1,&BytesRead,This->lpInOverlapStruct);
#if defined(SSE_411R7) && (_WIN32_WINNT>=0x0400)
      if(!ok)
        SwitchToThread();
      else
#endif
      if(This->WinNT)
      {
        WaitForSingleObject(This->hInEvent,250);
        GetOverlappedResult(This->hCom,This->lpInOverlapStruct,&BytesRead,0);
      }
      if(BytesRead)
      {
        bool FirstByte=!This->InpBuf.AreBytesInBuffer();
        This->InpBuf.AddByte(TempIn);
        if(FirstByte)
          if(This->lpInFirstByteProc) 
            This->lpInFirstByteProc();
        This->InCount++;
      }
      else
      {
        if(This->WinNT && This->pCancelIOProc)
          This->pCancelIOProc(This->hCom);
      }
    }
    else
      Sleep(50);
  }//while(!This->Closing)
  This->InThreadClosed=true;
  return 0;
}


DWORD CALLBACK TPortIO::OutThreadEntryPoint(void *t) {
  TPortIO *This=(TPortIO*)t;
  DWORD BytesWritten;
  BYTE TempOut;
  while(!This->Closing)
  {
    if(This->Outputting)
    {
      if(!This->OutPause)
      {
        TempOut=This->OutBuf.ReadByte();
        BytesWritten=0;
        if(This->WinNT)
          ResetEvent(This->hOutEvent);
        WriteFile(This->hCom,&TempOut,1,&BytesWritten,This->lpOutOverlapStruct);
        if(This->WinNT)
        {
          WaitForSingleObject(This->hOutEvent,250);
          GetOverlappedResult(This->hCom,This->lpOutOverlapStruct,
            &BytesWritten,0);
        }
        if(BytesWritten)
        {
          if(This->OutBuf.AreBytesInBuffer())
            This->OutBuf.NextByte();
          else
            This->Outputting=false;
          This->OutCount++;
        }
        else
          if(This->WinNT && This->pCancelIOProc) 
            This->pCancelIOProc(This->hCom);
      }
      else
        Sleep(50);
    }
    else //if(This->Outputting)
    {
#if defined(SSE_411R7)
      if(This->lpOutFinishedProc && !This->bOutFinishedProcCalled)
      {
        This->bOutFinishedProcCalled=true;
        This->lpOutFinishedProc(); // useful for Centronics, not serial
      }
#if (_WIN32_WINNT>=0x0400)
      //SwitchToThread();
      Sleep(50);
#else
      Sleep(1);
#endif
#else
      if(This->lpOutFinishedProc)  
        This->lpOutFinishedProc();
      if(!This->Outputting)
        SuspendThread(This->hOutThread);
#endif
    }
  }//while(!This->Closing)
  This->OutThreadClosed=true;
  return 0;
}


bool TPortIO::StartBreak() {
  if(hCom==NULL) 
    return false;
  return !!SetCommBreak(hCom);
}


bool TPortIO::EndBreak() {
  if(hCom==NULL) 
    return false;
  return !!ClearCommBreak(hCom);
}


void TPortIO::SetupCOM(int BaudRate,bool bXOn_XOff,int RTS,int DTR,
        bool bParity,BYTE ParityType,BYTE StopBits,BYTE WordLength) {
  if(hCom==NULL) 
    return;
  DCB dcb;
  ZeroMemory(&dcb,sizeof(DCB));
  dcb.DCBlength=sizeof(DCB);
  GetCommState(hCom,&dcb);
  dcb.BaudRate=BaudRate;
  dcb.fBinary=true;
  dcb.fParity=bParity;
  dcb.fOutxCtsFlow=0;
  dcb.fOutxDsrFlow=0;
  dcb.fDtrControl=DTR;
  dcb.fDsrSensitivity=0;
  dcb.fTXContinueOnXoff=true;
  dcb.fOutX=bXOn_XOff;
  dcb.fInX=bXOn_XOff;
  dcb.fErrorChar=0;
  dcb.fNull=0;
  dcb.fRtsControl=RTS;
  dcb.fAbortOnError=0;
  dcb.ByteSize=WordLength;
  dcb.Parity=ParityType;
  dcb.StopBits=StopBits;
  SetCommState(hCom,&dcb);
}


DWORD TPortIO::GetModemFlags() {
  if(hCom==NULL) 
    return 0;
  DWORD Flags=MS_CTS_ON; // Default to this if not available
  GetCommModemStatus(hCom,&Flags);
  return Flags;
}


bool TPortIO::SetDTR(bool Val) {
  if(hCom==NULL)
    return false;
  return !!EscapeCommFunction(hCom,(Val) ? SETDTR : CLRDTR);
}


bool TPortIO::SetRTS(bool Val) {
  if(hCom==NULL) 
    return false;
  return !!EscapeCommFunction(hCom,(Val) ? SETRTS : CLRRTS);
}


HANDLE TPortIO::Handle() {
  return hCom; 
}


bool TPortIO::IsOpen() { 
  return (hCom!=NULL); 
}

#endif//WIN32


BYTE TPortIO::ReadByte() { 
  return InpBuf.ReadByte(); 
}


void TPortIO::NextByte() { 
  InpBuf.NextByte(); 
}


bool TPortIO::AreBytesToRead() {
  return InpBuf.AreBytesInBuffer(); 
}


bool TPortIO::AreBytesToOutput() { 
  return Outputting; 
}


#ifdef UNIX

bool TPortIO::IsOpen() { 
  return iCom!=-1; 
}

#endif
