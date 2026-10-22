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

DOMAIN: Disk image
FILE: disk_stw.h
DESCRIPTION: Declarations for STW disk image support.
struct TImageSTW, TImageSTW2, TStw2TrackInfo, TStw2Data
---------------------------------------------------------------------------*/

#pragma once
#ifndef SSE_STW_H
#define SSE_STW_H

#include "floppy_drive.h"

#pragma pack(push, 8)

struct TImageSTW:public TImageMfm {
  // interface
#if defined(SSE_DISK_STW2)
  bool Create(char *path,BYTE density); // density DD=2 HD=4
#else
  bool Create(char *path,BYTE density=1); //DD=1 HD=2
#endif
  bool Open(char *path);
  virtual void Close();
  virtual bool LoadTrack(BYTE side,BYTE track,bool reload=false);
  virtual WORD GetMfmData(WORD position); 
  virtual void SetMfmData(WORD position,WORD mfm_data);
  // other functions
  TImageSTW();
  ~TImageSTW();
  void Init();
  virtual WORD GetImageWord(WORD position);
  bool GetSectorData(BYTE side,BYTE track,BYTE sector,BYTE *pdata); // for utilities
  virtual void SetImageWord(WORD position,WORD mfm_data);
  // variables
  WORD *TrackData;
  BYTE *ImageData;
  WORD Version;
  BYTE nSides,nTracks; // those are constants in STW v100
  virtual int GetNextTransition(WORD& us_to_next_flux);
  virtual void IncBitPosition();
};


#if defined(SSE_DISK_AUTOSTW)
// conversion functions
bool STtoSTW(BYTE id,char *dst_path); // ST/MSA/DIM -> STW
bool STWtoST(BYTE id); // STW -> ST/MSA/DIM
#endif


#if defined(SSE_DISK_STW2)

// we define no struct for headers

// this isn't a header but a unit for a table built in memory by the program, not saved
struct TStw2TrackInfo {
  // position of the track header (16 bytes) followed by data
  // relative to header of first track (that is, starting at 16)
  int position;
  WORD records; // # TStw2Data record units for the track
};


// a STW2 record unit
// timing and fuzzy are 8bit; 16bit would be easier but we mind the file size!
// STWv2 is twice larger as STWv1
struct TStw2Data {
  WORD mfm;
  BYTE fuzzy;
  BYTE timing;
};


struct TImageSTW2:public TImageSTW {
  enum EImageSTW2 { DEFAULT_TIMING=0x80,DEFAULT_FUZZY=0xFF };
  // interface
  bool Create(char *path,BYTE density); //DD=2
  bool Open(char *path);
  virtual void Close();
  virtual bool LoadTrack(BYTE side,BYTE track,bool reload=false);
#if defined(SSE_ENABLE_TRACE_LOG)
  void LogTrack(BYTE side,BYTE track);
#endif
  bool SaveTrack(BYTE side,BYTE track);
  // other functions
  TImageSTW2();
  ~TImageSTW2();
  virtual WORD GetImageWord(WORD position);
  virtual void IncPosition();
  void Init();
  virtual void SetImageWord(WORD position,WORD mfm_data);
  // variables
  TStw2TrackInfo TrackInfo[2][DRIVE_MAX_CYL+1];
  TStw2Data *TrackDataEx,*pTrackDataEx;
  DWORD ImageSize;
  WORD nMaxTrackWords;
  BYTE CurrentFuzzy,CurrentTiming,SourceImage;
  BYTE Density,Encoding;  // 2=DD,2=MFM
  bool Dirty; // track written to
};

// conversion functions
bool STXtoSTW(char* src_path,char* dst_path);
bool SCPtoSTW(char* src_path,char* dst_path);

#endif//#if defined(SSE_DISK_STW2)

#pragma pack(pop)

#endif//#ifndef SSE_STW_H
