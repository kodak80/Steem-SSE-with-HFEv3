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
FILE: disk_hfe.h
DESCRIPTION: Declarations for HFE disk image support.
struct  TImageHFE
---------------------------------------------------------------------------*/

#pragma once
#ifndef SSEHFE_H
#define SSEHFE_H

#include <hfe/libhxcfe.h> //3rdparty
#include <hfe/hfe_format.h> //3rdparty
#include "floppy_drive.h"

#pragma pack(push, 8)

struct  TImageHFE:public TImageMfm {
  // interface
  bool Create(char *path);
  bool Open(char *path);
  virtual void Close();
  virtual bool LoadTrack(BYTE side,BYTE track,bool reload=false);
  virtual WORD GetMfmData(WORD position); 
  virtual void SetMfmData(WORD position, WORD mfm_data);
  // other functions
  TImageHFE();
  ~TImageHFE();
  int ComputeIndex(WORD position);
  virtual WORD GetImageWord(WORD position);
  void Init();
  WORD MirrorMFM(WORD mfm_word);
  virtual void SetImageWord(WORD position,WORD mfm_data);
  // STW v2
  virtual int GetNextTransition(WORD& us_to_next_flux);
  virtual void IncBitPosition();
  // HFEv3
  // A v3 track stream may contain opcodes (index, bitrate change, skip bits,
  // random bits), so it can't be addressed as a fixed array of MFM words like
  // v1. Each side/track is decoded once, on first access, into a plain MFM
  // word stream (Steem bit order) plus a mask of weak (random) bits.
  struct THfeV3Track {
    WORD *Data;  // decoded MFM words, first bit = MSB
    WORD *Weak;  // weak bit mask (NULL if none), re-randomised on each read
    WORD nWords;
    bool Dirty;  // written to by the emulated FDC
  };
  bool DecodeV3Track(BYTE side,BYTE track,THfeV3Track &t);
  bool EncodeV3Track(BYTE track);
  void FreeV3Tracks();
  // variables
  WORD *TrackData;
  BYTE *ImageData;
  picfileformatheader *file_header;
  pictrack *track_header;
  int image_size;
  THfeV3Track *V3Tracks; // number_of_track*2 entries, NULL for v1 images
  THfeV3Track *V3Current; // track loaded by LoadTrack()
  bool IsV3;
  // Save copy, like the GTi: writes never change the original image, they
  // are saved as Game.sav.hfe beside it, and that copy is loaded instead of
  // the original from then on. Empty when the open file is itself the save
  // copy (written in place).
  char SavePath[1024];
};

#pragma pack(pop)

#endif//#ifndef SSEHFE_H
