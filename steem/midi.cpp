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
FILE: midi.cpp
DESCRIPTION: Classes that form the backbone of Steem's MIDI emulation. 
MIDI is short for Musical Instrument Digital Interface. 
The classes convert raw bytes on the ST side (ACIA) to and from MIDI messages
on the PC side.
Steem SSE can do MIDI two ways: using the multimedia libray or using
DirectMusic.
TODO: Linux?
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <shortcutbox.h>
#include <computer.h>
#include <iolist.h>


BYTE MIDI_out_running_status_flag=MIDI_NO_RUNNING_STATUS;
BYTE MIDI_in_running_status_flag=MIDI_NO_RUNNING_STATUS;
int MIDI_in_n_sysex=2,MIDI_out_n_sysex=2,MIDI_in_speed=100;
WORD MIDI_out_volume=0xffff;
DWORD MIDI_in_sysex_max=64*1024,MIDI_out_sysex_max=64*1024;

//  A sysex message starts with $F0 and ends with $F7
#define SYSEX_START 0xF0
#define SYSEX_END 0xF7

#define LOGSECTION LOGSECTION_PORTS


#if defined(SSE_DIRECTMIDI)
// we build DirectMidi this way so it's easier to enable/disable with the SSE_DIRECTMIDI switch
// http://directmidi.sourceforge.net/midiports.htm
#pragma comment(lib,"DSound.lib")
#pragma warning(disable: 4005 4189)
#include <directmidi/DSUTIL/DSUTIL.CPP>
#include <directmidi/DSUTIL/DXUTIL.CPP>
#include <directmidi/C3DBuffer.cpp>
#include <directmidi/C3DListener.cpp>
#include <directmidi/C3DSegment.cpp>
#include <directmidi/CAPathPerformance.cpp>
#include <directmidi/CAudioPath.cpp>
#include <directmidi/CCollection.cpp>
#include <directmidi/CDirectMusic.cpp>
#include <directmidi/CDLSLoader.cpp>
#include <directmidi/CDMusicException.cpp>
#include <directmidi/CInputPort.cpp>
#include <directmidi/CInstrument.cpp>
#include <directmidi/CMasterClock.cpp>
#include <directmidi/CMidiPort.cpp>
#include <directmidi/COutputPort.cpp>
#include <directmidi/CPerformance.cpp>
#include <directmidi/CPortPerformance.cpp>
#include <directmidi/CSampleInstrument.cpp>
#include <directmidi/CSegment.cpp>
#include <directmidi/Dmhelp.cpp>
#pragma warning(default: 4005 4189)

CDirectMusic DirectMusic;
CInputPort DirectMidiIn;
CDMOutputPort DirectMidiOut;
CDMReceiver DMReceiver;
CMasterClock DirectMidiClock;
DWORD DirectMidiClockIndex=0;

// Overriden virtual function for SysEx data capture
void CDMReceiver::RecvMidiMsg(REFERENCE_TIME lprt,DWORD,DWORD dwBytesRead,BYTE *lpBuffer) {
  BYTE *pData=lpBuffer;
  DWORD &DataLen=dwBytesRead;
  MIDIPort.MIDI_In->RunningStatus=MIDI_ALLOW_RUNNING_STATUS;
  CutPauseUntilSysEx_Time=0;
  if(DataLen) 
  {
    while(MIDIPort.MIDI_In->Buf.IsLocked()) // thread-safe!
#if (_WIN32_WINNT>=0x0400)
      if(SSEOptions.MidiUseSleep)
        Sleep(0);
      else
        SwitchToThread();
#else
      Sleep(0);
#endif
    if(MIDIPort.MIDI_In->NotEmptyProc) 
    {
      if(!MIDIPort.MIDI_In->Buf.AreBytesInBuffer())
        MIDIPort.MIDI_In->NotEmptyProc();
    }
    MIDIPort.MIDI_In->TimeSinceLastNote=lprt-MIDIPort.MIDI_In->LastTimestamp;
    MIDIPort.MIDI_In->LastTimestamp=lprt;
    TRACE_LOG2("MIDI SySex in $%X (%d) after %dms\n",pData[0],DataLen,MIDIPort.MIDI_In->TimeSinceLastNote/10000);
    MIDIPort.MIDI_In->Buf.AddBytes(pData,DataLen,MIDIPort.MIDI_In->TimeSinceLastNote);
  }
}


// Overriden virtual function for structured Midi data capture
void CDMReceiver::RecvMidiMsg(REFERENCE_TIME lprt,DWORD /*dwChannel*/,DWORD dwMsg) {
  BYTE *pData=(LPBYTE)&dwMsg;
  BYTE nParams=MidiGetStatusNumParams(pData[0]);
  DWORD DataLen=1+nParams;
  if(MIDI_in_running_status_flag==MIDI_ALLOW_RUNNING_STATUS)
  {
    if(MIDIPort.MIDI_In->RunningStatus==pData[0])
    {
      pData++;
      DataLen--;
    }
    else if(nParams)
      MIDIPort.MIDI_In->RunningStatus=pData[0];
    else
      MIDIPort.MIDI_In->RunningStatus=MIDI_ALLOW_RUNNING_STATUS;
  }

  if(DataLen) 
  {
    while(MIDIPort.MIDI_In->Buf.IsLocked()) // thread-safe!
#if (_WIN32_WINNT>=0x0400)
      if(SSEOptions.MidiUseSleep)
        Sleep(0);
      else
        SwitchToThread();
#else
      Sleep(0);
#endif
    if(MIDIPort.MIDI_In->NotEmptyProc) 
    {
      if(!MIDIPort.MIDI_In->Buf.AreBytesInBuffer())
        MIDIPort.MIDI_In->NotEmptyProc();
    }
    MIDIPort.MIDI_In->TimeSinceLastNote=lprt-MIDIPort.MIDI_In->LastTimestamp;
    MIDIPort.MIDI_In->LastTimestamp=lprt;
    TRACE_LOG2("MIDI in $%X (%d) after %dms\n",pData[0],DataLen,MIDIPort.MIDI_In->TimeSinceLastNote/10000);
    MIDIPort.MIDI_In->Buf.AddBytes(pData,DataLen,MIDIPort.MIDI_In->TimeSinceLastNote);
  }
}


// Sends an unstructured MIDI message

HRESULT CDMOutputPort::SendMidiMsg(REFERENCE_TIME rt,LPBYTE lpMsg,DWORD dwLength) {
  HRESULT hr = DM_FAILED;
#if !defined(SSE_DIRECTMIDI2)
  TCHAR strMembrFunc[] = _T("COutputPort::SendMidiMsg() long version");
#endif
  DWORD dwChannelGroup=0;
  if ((m_pPort) && (m_pBuffer) && (m_pClock))
  {
    if(rt==0) // get current time only if not set by caller
    {
      if(FAILED(hr = m_pClock->GetTime(&rt))) // Gets the exact time to play it
#if defined(SSE_DIRECTMIDI2)
        return hr;
#else
        throw CDMusicException(strMembrFunc,hr,__LINE__);
#endif
    }
    if (FAILED(hr = m_pBuffer->PackUnstructured(rt,dwChannelGroup,dwLength,lpMsg))) 
#if defined(SSE_DIRECTMIDI2)
      return hr;
#else
      throw CDMusicException(strMembrFunc,hr,__LINE__); 
#endif
    if (FAILED(hr = m_pPort->PlayBuffer(m_pBuffer))) // Sends the data
#if defined(SSE_DIRECTMIDI2)
      return hr;
#else
      throw CDMusicException(strMembrFunc,hr,__LINE__);
#endif
    if (FAILED(hr = m_pBuffer->Flush())) // Discards all data in the buffer
#if defined(SSE_DIRECTMIDI2)
      return hr;
#else
      throw CDMusicException(strMembrFunc,hr,__LINE__);
#endif
  }
  else
#if defined(SSE_DIRECTMIDI2)
    return hr;
#else
    throw CDMusicException(strMembrFunc,hr,__LINE__); 
#endif
  TRACE_LOG2("MIDI out %d bytes at %lld\n",dwLength,rt);
  return S_OK;
}	

// Function to send a MIDI normal message to the selected output port

HRESULT CDMOutputPort::SendMidiMsg(REFERENCE_TIME rt,DWORD dwMsg) {
  HRESULT hr = DM_FAILED;
  //TCHAR strMembrFunc[] = _T("COutputPort::SendMidiMsg() short version");
  DWORD dwChannelGroup=0;

  if ((m_pPort) && (m_pBuffer) && (m_pClock))
  {
    if(rt==0) // get current time only if not set by caller
    {
      if(FAILED(hr = m_pClock->GetTime(&rt))) // Gets the exact time to play it
#if defined(SSE_DIRECTMIDI2)
        return hr;
#else
        throw CDMusicException(strMembrFunc,hr,__LINE__);
#endif
    }
    if (FAILED(hr = m_pBuffer->PackStructured(rt,dwChannelGroup,dwMsg))) 
#if defined(SSE_DIRECTMIDI2)
      return hr;
#else
      throw CDMusicException(strMembrFunc,hr,__LINE__); 
#endif
    if (FAILED(hr = m_pPort->PlayBuffer(m_pBuffer))) // Sends the data
#if defined(SSE_DIRECTMIDI2)
      return hr;
#else
      throw CDMusicException(strMembrFunc,hr,__LINE__);
#endif
    if (FAILED(hr = m_pBuffer->Flush())) // Discards all data in the buffer
#if defined(SSE_DIRECTMIDI2)
      return hr;
#else
      throw CDMusicException(strMembrFunc,hr,__LINE__);
#endif
  }
  else
#if defined(SSE_DIRECTMIDI2)
    return hr;
#else
    throw CDMusicException(strMembrFunc,hr,__LINE__);
#endif
  TRACE_LOG2("MIDI out %X at %lld\n",dwMsg,rt);
  return S_OK;
}

#endif//#if defined(SSE_DIRECTMIDI)

#include <translate.h>

// return number of parameters for this status byte
BYTE MidiGetStatusNumParams(BYTE const StatByte) {
  ASSERTB(StatByte&0x80);
#ifndef SSE_LEAN_AND_MEAN
  if(!(StatByte&0x80)) // StatByte's most significant bit should be set
    return 0; // just in case
#endif
  switch(StatByte>>4) {
  case b1100: // Channel Voice Program Change
  case b1101: // Channel Voice Channel Pressure (After-touch).
    return 1;
  case b1111: // System message
    if(StatByte & BIT_3) 
      return 0; // Real time message 11111...
    switch(StatByte & b00000111) {
    case b0000: // SysEx start
    case b0110: // tune request
    case b0111: // SysEx end
      return 0;
    case b0011: // song select
    case b0001: // MIDI Time Code
    case b0101: // Cable Select
      return 1;
    }//sw
  }//sw
  return 2;
}


//
//                                TMIDIOut                                   //
//
TMIDIOut::TMIDIOut(int const Device,int const Volume) {

#ifdef WIN32
  Handle=NULL;
  bool InitFailed=false;
#if !defined(SSE_BADALLOC)
  if(!AllocSysEx())
  {
    ErrorText=T("Unable to allocate enough memory for this MIDI device.");
    InitFailed=true;
  }
  ASSERTB(!InitFailed);
#else
  AllocSysEx();
#endif
  if(!InitFailed)
  {
    Reset();
    Sleep(100);
#if defined(SSE_DIRECTMIDI)
    LastTimestamp=0;
    if(OPTION_DIRECTMUSIC)
    {
      INFOPORT PortInfo;
      DirectMidiOut.GetPortInfo(Device,&PortInfo);
      DirectMidiOut.SetPortParams(0,0,1,0,sound_freq);
      if(DirectMidiOut.ActivatePort(&PortInfo,32)==S_OK)
        Handle=(HMIDIOUT)(INT_PTR)(Device+1);
      else
      {
        ErrorText=T("Failed to open ouput MIDI device, it may already be in use or disconnected.");
        Handle=NULL;
        InitFailed=true;
      }
      TRACE_LOG("MIDI Device %d %s handle %p ok%d\n",Device,PortInfo.szPortDescription,Handle,!InitFailed);
    } else
#endif
    if(midiOutOpen(&Handle,Device,0,0,CALLBACK_NULL)==MMSYSERR_NOERROR) 
    {
      midiOutGetVolume(Handle,&OldVolume);
      SetVolume(Volume);
    }
    else 
    {
      //ErrorText=T("Failed to open the MIDI device, it may already be in use.");
      ErrorText=T("Failed to open ouput MIDI device, it may already be in use or disconnected.");
      Handle=NULL;
      InitFailed=true;
    }
  }
  if(InitFailed) 
  {
    for(int n=0;n<nSysExBufs;n++) 
    {
      if(SysEx[n].pData) 
      {
        delete[] SysEx[n].pData;
        SysEx[n].pData=NULL;   
      }
    }
  }
#endif

}


#ifdef WIN32


#if defined(SSE_DIRECTMIDI)

// compute the MIDI out timing on PC side based on elapsed cycles on ST side
// update LastTimestamp
REFERENCE_TIME TMIDIOut::ComputeReferenceTime() {
  REFERENCE_TIME rt=0,rtnow=0;
  HRESULT hr=DirectMidiOut.m_pClock->GetTime(&rtnow);
  DWORD CyclesSinceLastNote=(DWORD)(A_S_T-CycleOfLastNote);
  if(CyclesSinceLastNote<nSysCyclesPerSecond && LastTimestamp) // time MIDI out event according to ST cycles since last note
    rt=LastTimestamp+(CyclesSinceLastNote*MIDI_UNITS_1SEC)/nSysCyclesPerSecond;
  else if(SUCCEEDED(hr)) // set up timing based on cycles this frame
    rt=rtnow+(FRAMECYCLES*MIDI_UNITS_1SEC)/nSysCyclesPerSecond;
  LastTimestamp=rt;
  return rt;
}

#endif


bool TMIDIOut::FreeHeader(MIDIHDR* const pHdr) {
  if(pHdr==NULL) 
    return true;
  if(pHdr->lpData==NULL) 
    return true; // Don't need to free it
#if defined(SSE_DIRECTMIDI)
  if(!OPTION_DIRECTMUSIC)
#endif
  if(midiOutUnprepareHeader(Handle,pHdr,sizeof(MIDIHDR))!=MMSYSERR_NOERROR) 
    return false;
  pHdr->dwFlags=MHDR_DONE;
  pHdr->lpData=NULL;
  // Remove from list
  for(int n=0;n<nSysExBufs;n++) 
    if(SysEx[n].pHdr==pHdr) 
      SysEx[n].pHdr=NULL;
  return true;
}


// Windows timer to send MIDI out events at a more appropriate timing than just
// Steem's process slice - Multimedia driver
// It's optional because practically it could be worse that way
void CALLBACK SendByteEvent(UINT const uTimerID,UINT,DWORD_PTR const dwUser,DWORD_PTR,DWORD_PTR) {
  BYTE MessBufLen=(BYTE)(dwUser>>24);
  TRACE_LOG("MIDI timer %d out %08X\n",uTimerID,dwUser);
  HMIDIOUT Handle=MIDIPort.MIDI_Out->Handle;
  switch(MessBufLen) {
  case 1:
    midiOutShortMsg(Handle,dwUser&0xFF);
    break;
  case 2:
    midiOutShortMsg(Handle,dwUser&0xFFFF);
    break;
  default:
    midiOutShortMsg(Handle,dwUser&0xFFFFFF);
  }
}

#endif//WIN32


void TMIDIOut::SendByte(BYTE const Val) {

#ifdef WIN32
  if(Handle==NULL) 
    return;
  TRACE_LOG2("MIDI send %X\n",Val);
  //TRACE_OSD("->MIDI %X",Val);
  bool bSendBuffer=false,bAddToBuffer=true;

#if defined(SSE_MIDIRAW)
  if(OPTION_RAWMIDI) // ignore sysex
  {
#if defined(SSE_DIRECTMIDI)
    if(OPTION_DIRECTMUSIC)
    {
      REFERENCE_TIME rt=ComputeReferenceTime();
      DirectMidiOut.SendMidiMsg(rt,Val);
      bAddToBuffer=false;
    }
    else
#endif
      bSendBuffer=true; // so that it can be timed
  } else
#endif

  if((Val & BIT_7)==0) 
  { // Not status byte
    if(pCurSysEx==NULL) // not a sysex byte, so it's a note start/stop or something like that
    {
      if(nStatusParams==-1)
        bAddToBuffer=false; //Ignore all bytes until a status is sent after reset
      else 
      {
        if(ParamCount<=0) // running status
          ParamCount=nStatusParams-1;
        else
          ParamCount--;
        if(ParamCount<=0)
          bSendBuffer=true;
      }
    }
  }
  else
  { // Status byte
    if((Val & b11111000)==b11111000) 
    {  // Real time message (F8 clock FA start FB cont FC stop FE active sensing FF reset)
#if defined(SSE_DIRECTMIDI)
      if(OPTION_DIRECTMUSIC)
      {
        REFERENCE_TIME rt=ComputeReferenceTime();
        DirectMidiOut.SendMidiMsg(rt,Val);
        bAddToBuffer=false;
      }
      else
#endif
        bSendBuffer=true; // so that it's timed
    }
    else 
    {
      if(pCurSysEx) // non real time status byte while sending SYSEX
      {
#if defined(SSE_DIRECTMIDI)
        if(OPTION_DIRECTMUSIC)
        {
          if(Val==SYSEX_END) // should be, we don't add it ourselves
          {
            REFERENCE_TIME rt=ComputeReferenceTime();
            DirectMidiOut.SendMidiMsg(rt,pCurSysEx->pData,pCurSysEx->Len);
            pCurSysEx->pHdr->dwFlags=MHDR_DONE;
            for(int n=0;n<MAX_SYSEX_BUFS;n++) // free resource (?)
            {
              if(SysExHeader[n].dwFlags & MHDR_DONE)
                FreeHeader(&(SysExHeader[n]));
            }
          }
        }
        else
#endif
        {
          if(pCurSysEx->pData[pCurSysEx->Len-1]!=SYSEX_END)
            pCurSysEx->pData[pCurSysEx->Len++]=SYSEX_END; // Put an EOX on the end
          MIDIHDR* pHdr=NULL;
          for(int n=0;n<MAX_SYSEX_BUFS;n++) // get first free SYSEX buffer
          {
            if(SysExHeader[n].dwFlags & MHDR_DONE)
            {
              if(FreeHeader(&(SysExHeader[n])))
              {
                pHdr=&(SysExHeader[n]);
                break;
              }
            }
          }
          if(pHdr)
          {
            ZeroMemory(pHdr,sizeof(MIDIHDR));
            pHdr->lpData=(char*)(pCurSysEx->pData);
            pHdr->dwBufferLength=pCurSysEx->Len;
            pHdr->dwBytesRecorded=pCurSysEx->Len; // Shouldn't need to set this
            midiOutPrepareHeader(Handle,pHdr,sizeof(MIDIHDR));
            midiOutLongMsg(Handle,pHdr,sizeof(MIDIHDR));
            pCurSysEx->pHdr=pHdr;
          }
          else
          {
            TRACE_LOG("MIDI: No sysex headers available, ignoring message!\n");
          }
        }
        if(Val==SYSEX_END) 
          bAddToBuffer=false;
        pCurSysEx=NULL;
      }
      if(Val==SYSEX_START) 
      {
        // Find next free sysex buffer
        for(int n=0;n<nSysExBufs;n++) 
        {
          if(SysEx[n].pHdr)
          {
            // FreeHeader will set SysEx[n].pHdr to NULL if it is freed successfully
            if(SysEx[n].pHdr->dwFlags & MHDR_DONE) 
              FreeHeader(SysEx[n].pHdr);
          }
          if(SysEx[n].pHdr==NULL) 
          {
            pCurSysEx=&(SysEx[n]);
            pCurSysEx->Len=0;
            break;
          }
        }
      }
      else
      {
        if(bAddToBuffer) 
        {
          BYTE nParams=MidiGetStatusNumParams(Val);
          if(nParams>0) 
          {
            // Lose anything that has come before
            MessBufLen=0;
            nStatusParams=ParamCount=nParams;
          }
          else // directly send status byte with no parameters
          {
#if defined(SSE_DIRECTMIDI)
            if(OPTION_DIRECTMUSIC)
            {
              REFERENCE_TIME rt=ComputeReferenceTime();
              DirectMidiOut.SendMidiMsg(rt,Val);
              bAddToBuffer=false;
            }
            else
#endif
              bSendBuffer=true; // so that it's timed 
          }
        }
      }
    }
  }
  if(bAddToBuffer) 
  {
    if(pCurSysEx==NULL) 
    {
      if(MessBufLen<8)
        MessBuf[MessBufLen++]=Val;
      else 
      {
        TRACE_LOG("MIDI: Out message buffer overflow!\n");
      }
    }
    else
    {
      if(pCurSysEx->Len<MaxSysExLen)
        pCurSysEx->pData[pCurSysEx->Len++]=Val;
      else 
      {
        TRACE_LOG("MIDI: Out sysex buffer overflow!\n");
      }
    }
  }
  if(bSendBuffer) 
  {
#if defined(SSE_DIRECTMIDI)
    if(OPTION_DIRECTMUSIC)
    {
      REFERENCE_TIME rt=ComputeReferenceTime();
      TRACE_LOG2("MIDI send %d bytes on %lld\n",MessBufLen,rt);
      switch(MessBufLen) {
      case 1:
        DirectMidiOut.SendMidiMsg(rt,MessBuf[0]);
        break;
      case 2:
        DirectMidiOut.SendMidiMsg(rt,MessBuf[0] | (MessBuf[1] << 8));
        break;
      default: // that would be 3 bytes
        DirectMidiOut.SendMidiMsg(rt,MessBuf[0] | (MessBuf[1] << 8) | (MessBuf[2] << 16));
        break;
      }
    }
    else
#endif
    if(SSEOptions.MidiUseTimer) // set up timer based on cycles this frame
    {
      UINT uDelay=(UINT)(FRAMECYCLES*1000/nSysCyclesPerSecond); // ms
      DWORD_PTR x=MessBuf[0]|(MessBuf[1]<<8)|(MessBuf[2]<<16)|(MessBufLen<<24);
      MMRESULT uTimerID=timeSetEvent(uDelay,1,SendByteEvent,x,TIME_ONESHOT|TIME_CALLBACK_FUNCTION);
      TRACE_LOG("%d %d %d schedule MIDI out %d %06X in %dms timer %d\n",TIMING_INFO,
        MessBufLen,MessBuf[0]|(MessBuf[1]<<8)|(MessBuf[2]<<16),uDelay,uTimerID);
    }
    else // send right now
    {
      switch (MessBufLen){
      case 1:
        midiOutShortMsg(Handle,MessBuf[0]);
        break;
      case 2:
        midiOutShortMsg(Handle,MessBuf[0] | (MessBuf[1] << 8));
        break;
      default:
        midiOutShortMsg(Handle,MessBuf[0] | (MessBuf[1] << 8) | (MessBuf[2] << 16));
        break;
      }
    }
    MessBufLen=MIDI_out_running_status_flag; // skip byte 0 if no status
  }
#endif//WIN32

}


#ifdef WIN32

int TMIDIOut::GetDeviceID() {
#if defined(SSE_DIRECTMIDI)
  if(OPTION_DIRECTMUSIC)
    return (int)(INT_PTR)Handle-1;
  else
#endif
  {
    int DeviceID=-999;
#ifndef SSE_LEAN_AND_MEAN
    if(Handle)
#endif
      midiOutGetID(Handle,(UINT*)&DeviceID);
    return DeviceID;
  }
}

#endif


bool TMIDIOut::SetVolume(int const Volume) { // this works only with "multimedia"

#ifdef WIN32
#if defined(SSE_DIRECTMIDI)
  if(OPTION_DIRECTMUSIC)
    return false;
  else
#endif
  return ((Handle==NULL)?false:(midiOutSetVolume(Handle,MAKELONG(Volume,Volume))==MMSYSERR_NOERROR));
#endif

#ifdef UNIX
  return false;
#endif

}


bool TMIDIOut::Mute() {

#ifdef WIN32
#if defined(SSE_DIRECTMIDI)
  if(OPTION_DIRECTMUSIC)
    return false;
  else
#endif
  return ((Handle==NULL)?false:(midiOutSetVolume(Handle,0)==MMSYSERR_NOERROR));
#endif

#ifdef UNIX
  return false;
#endif

}


void TMIDIOut::Reset() {

#ifdef WIN32
  MessBuf[0]=0;
  MessBufLen=ParamCount=0;
  nStatusParams=-1;
  pCurSysEx=NULL;
#endif

}


#ifdef WIN32

bool TMIDIOut::AllocSysEx() {
  MaxSysExLen=MIDI_out_sysex_max-64;
  nSysExBufs=MIDI_out_n_sysex+1;
#if defined(SSE_BADALLOC)
  for(int n=0;n<MAX_SYSEX_BUFS;n++)
  {
    SysExHeader[n].lpData=NULL;
    SysExHeader[n].dwFlags=MHDR_DONE;
  }
  for(int n=0;n<nSysExBufs;n++)
  {
    SysEx[n].pData=NULL;
    SysEx[n].pHdr=NULL;
  }
  for(int n=0;n<nSysExBufs;n++)
    SysEx[n].pData=new BYTE[MaxSysExLen+1];
  return true;
#else
  try {
    for(int n=0;n<MAX_SYSEX_BUFS;n++) 
    {
      SysExHeader[n].lpData=NULL;
      SysExHeader[n].dwFlags=MHDR_DONE;
    }
    for(int n=0;n<nSysExBufs;n++) 
    {
      SysEx[n].pData=NULL;
      SysEx[n].pHdr=NULL;
    }
    for(int n=0;n<nSysExBufs;n++) 
      SysEx[n].pData=new BYTE[MaxSysExLen+1];
    return true;
  }
  catch(...) {
    return false;
  }
#endif
}


void TMIDIOut::ReInitSysEx() {

  if(Handle==NULL) 
    return;
#if defined(SSE_DIRECTMIDI)
  if(!OPTION_DIRECTMUSIC)
#endif
  {
    midiOutReset(Handle);
#ifndef SSE_LEAN_AND_MEAN
    midiOutShortMsg(Handle,b11110111); // Send an EOX, just in case (shouldn't do any harm)
#endif
  }
  for(int n=0;n<MAX_SYSEX_BUFS;n++) 
    FreeHeader(&(SysExHeader[n]));
  for(int n=0;n<nSysExBufs;n++) 
    if(SysEx[n].pData) 
      delete[] SysEx[n].pData,SysEx[n].pData=NULL;
  if(pCurSysEx) 
    nStatusParams=-1; // If currently sending sysex ignore rest
  pCurSysEx=NULL;
  AllocSysEx();
}

#endif


TMIDIOut::~TMIDIOut() {

#ifdef WIN32
  if(Handle==NULL) 
    return;
#if defined(SSE_DIRECTMIDI)
  if(OPTION_DIRECTMUSIC)
    DirectMidiOut.ReleasePort();
  else
#endif
  {
    midiOutReset(Handle);
#ifndef SSE_LEAN_AND_MEAN
    midiOutShortMsg(Handle,b11110111); // Send an EOX, just in case (shouldn't do any harm)
#endif
  }

  for(int n=0;n<MAX_SYSEX_BUFS;n++) 
    FreeHeader(&(SysExHeader[n]));
#if defined(SSE_DIRECTMIDI)
  if(!OPTION_DIRECTMUSIC)
#endif
  {
    midiOutClose(Handle);
    SetVolume((WORD)(OldVolume));
  }
  for(int n=0;n<nSysExBufs;n++) 
    if(SysEx[n].pData) 
      delete[] SysEx[n].pData,SysEx[n].pData=NULL;
  Handle=NULL;
  Sleep(100);
#endif//WIN32  
}


//
//                                TMIDIIn                                    //
//
TMIDIIn::TMIDIIn(int Device,bool StartNow,LPMIDIINNOTEMPTYPROC NEP) {

#ifdef WIN32
  Handle=NULL;
  NotEmptyProc=NEP;
  Killing=Started=false;
  MaxSysExLen=MIDI_in_sysex_max-64;
  nSysExBufs=MIDI_in_n_sysex;
  ZeroMemory(SysExBuf,sizeof(SysExBuf));
  bool InitFailed=!Buf.Create(MaxSysExLen+10000);
#if defined(SSE_BADALLOC)
  for(int n=0;n<nSysExBufs;n++) 
    SysExBuf[n]=new BYTE[MaxSysExLen+2];
#else
  try {
    for(int n=0;n<nSysExBufs;n++) 
      SysExBuf[n]=new BYTE[MaxSysExLen+2];
  }
  catch(...) {
    InitFailed=true;
  }
  ASSERTB(!InitFailed);
  if(InitFailed)
    ErrorText=T("Unable to allocate enough memory for this MIDI device.");
  else 
#endif
  {
    Reset();
    Sleep(100);
#if defined(SSE_DIRECTMIDI)
    if(OPTION_DIRECTMUSIC)
    {
      // Maximum size for SysEx data in input port
      const int SYSTEM_EXCLUSIVE_MEM = MIDI_in_sysex_max;
      INFOPORT PortInfo;
      DirectMidiIn.GetPortInfo(Device,&PortInfo);
      if(DirectMidiIn.ActivatePort(&PortInfo,SYSTEM_EXCLUSIVE_MEM)==S_OK)
      {
        DirectMidiIn.SetReceiver(DMReceiver);
        Handle=(HMIDIIN)(INT_PTR)(Device+1);
        if(StartNow) 
          Start();
      }
      else
      {
        Handle=NULL;
        InitFailed=true;
      }
    } else
#endif
    if(midiInOpen(&Handle,Device,(DWORD_PTR)InProc,(DWORD_PTR)this,CALLBACK_FUNCTION)
      ==MMSYSERR_NOERROR)
    {
      if(StartNow) 
        Start();
    }
    else 
    {
      //ErrorText=T("Failed to open the MIDI device, it may already be in use.");
      ErrorText=T("Failed to open input MIDI device, it may already be in use or disconnected.");
      Handle=NULL;
      InitFailed=true;
    }
  }
  if(InitFailed) 
  {
    Buf.Destroy();
    for(int n=0;n<nSysExBufs;n++)
    {
      if(SysExBuf[n]) 
      { 
        delete[] SysExBuf[n]; 
        SysExBuf[n]=NULL; 
      }
    }
    Reset();
  }
#endif//WIN32

}


#ifdef WIN32
// this function not called for DirectMidi, only multimedia
void CALLBACK TMIDIIn::InProc(HMIDIIN Handle,UINT Msg,DWORD_PTR dwThis,
                              DWORD_PTR MidiMess,DWORD_PTR timestamp) {
  TMIDIIn *This=(TMIDIIn*)dwThis;
  if(This->Killing)
    return;
  BYTE *pData=NULL;
  DWORD DataLen=0;
  MIDIHDR *pSysExHdr=NULL;
  switch(Msg) {
  case MIM_ERROR:
  case MIM_DATA: 
  {
    pData=(LPBYTE)&MidiMess;
    BYTE nParams=MidiGetStatusNumParams(pData[0]);
    DataLen=1+nParams;
    if(MIDI_in_running_status_flag==MIDI_ALLOW_RUNNING_STATUS) 
    {
      if(This->RunningStatus==pData[0]) 
      {
        pData++;
        DataLen--;
      }
      else if(nParams)
        This->RunningStatus=pData[0];
      else
        This->RunningStatus=MIDI_ALLOW_RUNNING_STATUS;
    }
    break;
  }
  case MIM_LONGERROR:
  case MIM_LONGDATA:
    pSysExHdr=LPMIDIHDR(MidiMess);
    pData=(LPBYTE)pSysExHdr->lpData;
    DataLen=pSysExHdr->dwBytesRecorded;
    This->RunningStatus=MIDI_ALLOW_RUNNING_STATUS; // 0 ; what's the use of the option then?
    CutPauseUntilSysEx_Time=0;
#if defined(SSE_MIDIRAW)
    if(!OPTION_RAWMIDI)
#endif
    {
      if(DataLen==0||pData[DataLen-1]!=b11110111)
        pData[DataLen++]=b11110111; // If no EOX add it
      if(pData[0]!=b11110000)
      {
        pData--;
        pData[0]=b11110000;
        DataLen++;
      }
    }
    break;
  }
  if(DataLen) 
  {
    while(This->Buf.IsLocked()) // thread-safe!
#if (_WIN32_WINNT>=0x0400)
      if(SSEOptions.MidiUseSleep)
        Sleep(0);
      else
        SwitchToThread();
#else
      Sleep(0);
#endif
    if(This->NotEmptyProc) 
    {
      if(!This->Buf.AreBytesInBuffer())
        This->NotEmptyProc();
    }
    // use timestamp (ms)
    This->TimeSinceLastNote=(WORD)(timestamp-This->LastTimestamp);
    This->LastTimestamp=timestamp;
    TRACE_LOG("MM Midi in %X (%d) after %dms\n",pData[0],DataLen,This->TimeSinceLastNote);
#if defined(SSE_DIRECTMIDI)
    This->Buf.AddBytes(pData,DataLen,This->TimeSinceLastNote);
#else
    This->Buf.AddBytes(pData,DataLen);
#endif
#if defined(SSE_MIDIRAW)
    if(!OPTION_RAWMIDI)
#endif
    if(pSysExHdr) 
    {
      midiInUnprepareHeader(Handle,pSysExHdr,sizeof(MIDIHDR));
      ZeroMemory(pSysExHdr,sizeof(MIDIHDR));
      pSysExHdr->lpData=(char*)pData;
      pSysExHdr->dwBufferLength=This->MaxSysExLen;
      pSysExHdr->dwFlags=0;
      midiInPrepareHeader(Handle,pSysExHdr,sizeof(MIDIHDR));
      midiInAddBuffer(Handle,pSysExHdr,sizeof(MIDIHDR));
    }
  }
}


int TMIDIIn::GetDeviceID() {
#if defined(SSE_DIRECTMIDI)
  if(OPTION_DIRECTMUSIC)
    return (int)(INT_PTR)Handle-1;
  else
#endif
  {
    int DeviceID=-999;
    if(Handle!=NULL)
      midiInGetID(Handle,(UINT*)&DeviceID);
    return DeviceID;
  }
}

#endif//WIN32


void TMIDIIn::Reset() {

#ifdef WIN32
  if(Handle==NULL) 
    return;
  bool WasStarted=Started;
  Stop();
  while(Buf.IsLocked()) 
#if (_WIN32_WINNT>=0x0400)
    if(SSEOptions.MidiUseSleep)
      Sleep(0);
    else
      SwitchToThread();
#else
    Sleep(0);
#endif
  Buf.Reset();
  RunningStatus=MIDI_ALLOW_RUNNING_STATUS;
  if(WasStarted) 
    Start();
#endif//WIN32

}


bool TMIDIIn::Start() { // called at each run

  LastTimestamp=0;
  CycleOfLastNote=A_S_T;

#ifdef WIN32

  if(Handle==NULL||Started)
    return Started;
  AddSysExBufs();
  Started=
#if defined(SSE_DIRECTMIDI)
    (OPTION_DIRECTMUSIC) ? (DirectMidiIn.ActivateNotification()==S_OK) :
#endif
    (midiInStart(Handle)==MMSYSERR_NOERROR);
  if(!Started)
    Stop();
  return Started;

#endif//WIN32

#ifdef UNIX
  return false;
#endif

}


void TMIDIIn::Stop() {

#ifdef WIN32
  if(Handle==NULL||!Started)
    return;
  Started=false;
  Killing=true;
#if defined(SSE_DIRECTMIDI)
  if(OPTION_DIRECTMUSIC)
    DirectMidiIn.TerminateNotification();
  else
#endif
  {
    midiInStop(Handle);
    midiInReset(Handle);
    // former void TMIDIIn::RemoveSysExBufs()
    for(int n=0;n<nSysExBufs;n++) 
      if(SysExBuf[n]) 
        midiInUnprepareHeader(Handle,&(SysExHeader[n]),sizeof(MIDIHDR));
  }
  Killing=false;
#endif//WIN32

}


#ifdef WIN32

void TMIDIIn::AddSysExBufs() {
  if(Handle==NULL) 
    return;
  for(int n=0;n<nSysExBufs;n++) 
  {
    if(SysExBuf[n])
    {
      ZeroMemory(&(SysExHeader[n]),sizeof(MIDIHDR));
      SysExHeader[n].lpData=(char*)(SysExBuf[n]+1);
      SysExHeader[n].dwBufferLength=MaxSysExLen;
      SysExHeader[n].dwFlags=0;
#if defined(SSE_DIRECTMIDI)
      if(!OPTION_DIRECTMUSIC)
#endif
      {
        midiInPrepareHeader(Handle,&(SysExHeader[n]),sizeof(MIDIHDR));
        midiInAddBuffer(Handle,&(SysExHeader[n]),sizeof(MIDIHDR));
      }
    }
  }
}


void TMIDIIn::ReInitSysEx() {
  if(Handle==NULL)
    return;
  bool WasStarted=Started;
  Stop();
  for(int n=0;n<nSysExBufs;n++)
  {
    if(SysExBuf[n])
    {
      delete[] SysExBuf[n];
      SysExBuf[n]=NULL;
    }
  }
  MaxSysExLen=MIDI_in_sysex_max-64;
  nSysExBufs=MIDI_in_n_sysex;
#if defined(SSE_BADALLOC)
  for(int n=0;n<nSysExBufs;n++)
    SysExBuf[n]=new BYTE[MaxSysExLen+2];
#else
  try
  {
    for(int n=0;n<nSysExBufs;n++)
      SysExBuf[n]=new BYTE[MaxSysExLen+2];
  }
  catch(...) {}
#endif
  if(WasStarted)
    Start();
}

#endif//WIN32


TMIDIIn::~TMIDIIn() {

#ifdef WIN32

  if(Handle==NULL)
    return;
  Stop();
#if defined(SSE_DIRECTMIDI)
  if(OPTION_DIRECTMUSIC)
    DirectMidiIn.ReleasePort();
  else
#endif
    midiInClose(Handle);

  for(int n=0;n<nSysExBufs;n++) 
    if(SysExBuf[n]) 
      delete[] SysExBuf[n],SysExBuf[n]=NULL;
#endif//WIN32      
}

#undef LOGSECTION
#undef SYSEX_START
#undef SYSEX_END
