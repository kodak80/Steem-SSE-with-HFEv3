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

DOMAIN: disk image
FILE: floppy_drive.h
DESCRIPTION: Declarations for floppy drive emulation.
struct TSF314
---------------------------------------------------------------------------*/

#pragma once
#ifndef SSEDRIVE_H
#define SSEDRIVE_H

#include <easystr.h>
#include "floppy_disk.h"
#include "SSE.h"

#define DRIVE_A 0
#define DRIVE_B 1
#define DRIVE_C 2
#define DRIVE_Z (DRIVE_C+'Z'-'C')


#pragma pack(push, 8)

struct TSF314 {
  // FUNCTIONS
  TSF314();
  WORD BytePosition(); //this has to do with IP and rotation speed
  WORD BytesToHbls(int bytes);
#if defined(SSE_DISK_GHOST)
  bool CheckGhostDisk(bool bWrite);
#endif
  DWORD HblsAtIndex();
  WORD HblsNextIndex();
  WORD HblsPerRotation();
  WORD HblsToBytes(WORD hbls);
  void Init(),Restore(BYTE myid);
  void UpdateAdat(bool RefreshGUI=true);
  int CyclesPerByte();
  void IndexPulse();
  void Motor(bool state);
  void Read();
  void Step(int direction);
  void Write();

  // returns 0 if OK, negative error code if not (defined in EDiskImage)
  // implementation is in diskman.cpp
  int SetDisk(EasyStr FilePath,EasyStr CompressedDiskName="",
              TBpbInfo *pDetectBPB=NULL,TBpbInfo *pFileBPB=NULL);
  bool DiskInDrive() {
    return bDiskInDrive;
  }
  bool NotEmpty() {
    return DiskInDrive();
  }
  bool Empty() {
    return !DiskInDrive();
  }
  void RemoveDisk(bool LoseChanges=false);
  bool ReinsertDisk();
  EasyStr GetDisk();
#if defined(SSE_DRIVE_SOUND)
#ifdef WIN32
  void SoundLoadSamples(IDirectSound *DSObj,DSBUFFERDESC *dsbd,WAVEFORMATEX *wfx);
#endif
#ifdef UNIX
  void SoundLoadSamples();
#endif
  void SoundReleaseBuffers();
  void SoundStopBuffers();
  void SoundCheckCommand(BYTE cr);
  void SoundCheckIrq();
  void SoundVBL();
  void SoundChangeVolume();
  void SoundStep(BOOL end_of_seek=FALSE);
  //410
  void SoundPlay(int snd);
  void SoundStop(int snd);
#endif//sound
  // DATA
  TImageMfm *MfmManager; //polymorphic
#if defined(SSE_DRIVE_SOUND)
  int SoundVolume;
#endif
  int cycles_per_byte;
  COUNTER_VAR time_of_next_ip, time_of_last_ip;
  TImageType ImageType;
  WORD SectorChecksum; // meta
  BYTE Id; // A: (0) or B: (1)
  BYTE track;
#if defined(SSE_DRIVE_SOUND)
  BYTE old_track;
#endif
  bool bMotor;
  bool bSingleSided;
  bool bFreeboot;
  bool bGhost;
  bool reading,writing;
  bool bAdat;
  bool bDiskInDrive;
};

#pragma pack(pop)

#if defined(SSE_DRIVE_SOUND)
extern EasyStr DriveSoundDir[2]; // not part of struct, otherwise ctor must be changed
#endif

#endif//#ifndef SSEDRIVE_H
