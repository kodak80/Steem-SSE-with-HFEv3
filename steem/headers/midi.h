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

FILE: midi.h
DOMAIN: I/O
DESCRIPTION: Declarations for MIDI input/output.
struct TOutSysEx
class TMIDIOut, TMIDIIn
DirectMidi: class CDMReceiver, CDMOutputPort
---------------------------------------------------------------------------*/

#pragma once
#ifndef MIDI_DECLA_H
#define MIDI_DECLA_H

#include "conditions.h"
#include "parameters.h"
#include <circularbuffer.h>
#include <easystr.h>


#ifdef MINGW_BUILD
#undef NULL
#define NULL 0
#endif

BYTE MidiGetStatusNumParams(BYTE StatByte);

#define MIDI_NO_RUNNING_STATUS 1
#define MIDI_ALLOW_RUNNING_STATUS 0
extern BYTE MIDI_out_running_status_flag;
extern BYTE MIDI_in_running_status_flag;
extern int MIDI_in_n_sysex,MIDI_out_n_sysex,MIDI_in_speed;
extern WORD MIDI_out_volume;
extern DWORD MIDI_in_sysex_max,MIDI_out_sysex_max;

#if defined(SSE_DIRECTMIDI)
#pragma warning(disable: 4459) // some ugly stuff because we use old code
#undef DEFINE_GUID
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        EXTERN_C const GUID DECLSPEC_SELECTANY name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }
#include <dmusicc.h>
#include <dmksctrl.h>
//#include <dmdls.h>
#include <dmusici.h>
#undef DEFINE_GUID
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    EXTERN_C const GUID FAR name
#include <directmidi/CDirectMidi.h>

extern CDirectMusic DirectMusic;
extern CInputPort DirectMidiIn;
class CDMReceiver:public CReceiver
{
public:
	// Overriden virtual functions to receive midi messages, structured and unstructured
	void RecvMidiMsg(REFERENCE_TIME rt,DWORD dwChannel,DWORD dwBytesRead,BYTE *lpBuffer);
	void RecvMidiMsg(REFERENCE_TIME rt,DWORD dwChannel,DWORD dwMsg);
};
extern CDMReceiver DMReceiver;

class CDMOutputPort:public COutputPort
{
public:
  // Overriden functions
  // Sends a message to the port in DWORD format
  HRESULT SendMidiMsg(REFERENCE_TIME rt,DWORD dwMsg);
  // Sends an unstrucuted message to the port
  HRESULT SendMidiMsg(REFERENCE_TIME rt,LPBYTE lpMsg,DWORD dwLength);
};
extern CDMOutputPort DirectMidiOut;

extern CMasterClock DirectMidiClock;
extern DWORD DirectMidiClockIndex;

#endif//#if defined(SSE_DIRECTMIDI)

#ifdef WIN32
struct TOutSysEx {
  BYTE *pData;
  DWORD Len;
  MIDIHDR *pHdr;
};
#endif

class TMIDIOut {
//private:
#ifdef WIN32
public:
  HMIDIOUT Handle;
private:
  BYTE MessBuf[8];
  int MessBufLen,ParamCount,nStatusParams;
  TOutSysEx SysEx[MAX_SYSEX_BUFS+1];
  TOutSysEx *pCurSysEx;
  int nSysExBufs;
  DWORD MaxSysExLen;
  MIDIHDR SysExHeader[MAX_SYSEX_BUFS];
  DWORD OldVolume;
#endif//WIN32
public:
  TMIDIOut(int,int);
  bool AllocSysEx();
  void ReInitSysEx();
  ~TMIDIOut();
  void SendByte(BYTE Val);
#ifdef WIN32
  int GetDeviceID();
  bool IsOpen() {
    return (Handle!=NULL);
  }
  bool FreeHeader(MIDIHDR *pHdr);
#endif//WIN32
  bool SetVolume(int Volume);
  bool Mute();
  void Reset();
  Str ErrorText;
  DWORD TimeLastSent;
#if defined(SSE_DIRECTMIDI)
  REFERENCE_TIME ComputeReferenceTime(); // time MIDI out events according to ST cycles between notes
  REFERENCE_TIME LastTimestamp;
  COUNTER_VAR CycleOfLastNote;
#endif
};

typedef void MIDIINNOTEMPTYPROC();
typedef MIDIINNOTEMPTYPROC* LPMIDIINNOTEMPTYPROC;

class TMIDIIn {
#if defined(SSE_DIRECTMIDI)
public:
#else
private:
DEBUG_ONLY(public:)
#endif
  CircularBuffer Buf;
#ifdef WIN32
  HMIDIIN Handle;
  MIDIHDR SysExHeader[MAX_SYSEX_BUFS];
  BYTE *SysExBuf[MAX_SYSEX_BUFS];  //[MIDI_SYSEX_BUFFER_SIZE+2]; // +2=Space for SOX and EOX
  volatile bool Killing;
  bool Started;
  DWORD MaxSysExLen;
  int RunningStatus,nSysExBufs;
  static void CALLBACK InProc(HMIDIIN Handle,UINT Msg,DWORD_PTR dwThis,
    DWORD_PTR MidiMess,DWORD_PTR timestamp);
#endif//WIN32
public:
  TMIDIIn(int Device,bool StartNow,LPMIDIINNOTEMPTYPROC NEP=NULL);
  void AddSysExBufs();
  void ReInitSysEx();
  ~TMIDIIn();
#ifdef WIN32
  int GetDeviceID();
  bool IsOpen() { 
    return (Handle!=NULL);
  }
#endif
  void Reset();
  bool Start();
  void Stop();
  bool AreBytesToCome() {
    return Buf.AreBytesInBuffer();
  }
  BYTE ReadByte() {
    return Buf.ReadByte();
  }
  void NextByte() {
    Buf.NextByte();
  }
  LPMIDIINNOTEMPTYPROC NotEmptyProc;
  Str ErrorText;
#if defined(SSE_DIRECTMIDI)
  REFERENCE_TIME LastTimestamp,TimeSinceLastNote;
#else
  WORD TimeSinceLastNote;
  DWORD_PTR LastTimestamp;
#endif
  COUNTER_VAR CycleOfLastNote;
};

#endif//MIDI_DECLA_H
