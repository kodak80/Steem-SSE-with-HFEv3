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
  virtual int VarByteCycles(WORD byteindex,int nominal_cpb);
  virtual void IncBitPosition();
  // HFEv3
  // A v3 track stream may contain opcodes (index, bitrate change, skip bits,
  // random bits), so it can't be addressed as a fixed array of MFM words like
  // v1. Each side/track is decoded once, on first access, into a plain MFM
  // word stream (Steem bit order) plus a mask of weak (random) bits.
  struct THfeV3Track {
    WORD *Data;  // decoded MFM words, first bit = MSB
    WORD *Weak;  // weak bit mask (NULL if none), re-randomised on each read
    WORD *Cyc;   // per-cell flux time, 8MHz cycles in 4.4 fixed point (256 =
                 // nominal 16cy/2us); NULL when the track is pure nominal
                 // density, so normal disks keep the exact nominal timing path
    WORD nWords;
    bool Dirty;  // written to by the emulated FDC
    bool Unread; // bridge mode: no data from the drive, random bits for now
    bool Formatted; // bridge mode: rewritten whole by the ST (WRITE TRACK)
  };
  bool DecodeV3Track(BYTE side,BYTE track,THfeV3Track &t);
  bool EncodeV3Track(BYTE track);
  void FreeV3Tracks();
  void FreeV3TrackData(THfeV3Track &t);
  static bool MakeUnformattedTrack(THfeV3Track &t);
  static bool BitsToV3Track(const BYTE *mfm,const WORD *speed,
    const BYTE *weak,int nbits,THfeV3Track &t);
#if defined(SSE_FLOPPYBRIDGE)
  // Bridge mode: a descriptor file (text starting with "FLOPPYBRIDGE") opens
  // a real drive on a Greaseweazle etc. through FloppyBridge instead of an
  // image. Each track read from the drive is stored as a decoded v3 track,
  // with the measured density in Cyc, and kept until the disk is removed.
  // The drive is read-only for now.
  bool OpenBridge(const char *descriptor,const char *path);
  bool LoadBridgeTrack(BYTE side,BYTE track,THfeV3Track &t);
  void BridgeMotorOff(); // the emulated motor stopped
  void BridgePoll(); // every VBL: has the real disk been changed?
  void BridgeDiskChanged(bool disk_in);
  void BridgeVerifyDisk(BYTE side,BYTE track,THfeV3Track &t);
  int BridgePollCount;
  bool BridgeVerify; // the drive was idle: check it's still the same disk
  bool BridgeLastWP; // write-protect sensor at the last poll
  int BridgeLastSide,BridgeLastTrack; // last track loaded
  int BridgeTraceLoads; // after a disk change: log what the ST reads
  // write the tracks the ST changed to the real disk (except keep)
  void BridgeFlush(const THfeV3Track *keep);
  bool BridgeCanWrite(); // write=on and the disk isn't write-protected
  void *Bridge; // FloppyBridgeOpen() handle, NULL for an image file
  picfileformatheader BridgeHeader; // stands in for the file header
  bool BridgeDiskIn;
  char BridgeError[256];
#endif
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
