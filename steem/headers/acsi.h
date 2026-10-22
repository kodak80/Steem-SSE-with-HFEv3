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

DOMAIN: Hard drive, Laser printer
FILE: acsi.h
DESCRIPTION: Declarations for ACSI hard drive image emulation, and
ACSI laser printer emulation.
struct TAcsiHdc
---------------------------------------------------------------------------*/

#pragma once
#ifndef SSEACSI_H
#define SSEACSI_H

#include "conditions.h"
#include "SSE.h"


#pragma pack(push, 8)

#if defined(SSE_ACSI)

struct TAcsiHdc {
  TAcsiHdc() { memset(this,0,sizeof(TAcsiHdc)); }
  ~TAcsiHdc();
  // interface
  bool Init(BYTE num,char *path);
  BYTE IORead();
  void IOWrite(BYTE Line,BYTE io_src_b);
  void Irq(bool state);
  void Reset();
  // other functions
  void CloseImageFile();
  void ReadWrite();
  DWORD SectorNum(); // get absolute sector index from command
  bool Seek();
  void Format();
  void Inquiry();
  // member variables
  DWORDLONG nSectors;
  DWORD block;
#if defined(SSE_ACSI_FMT_AG)
  DWORDLONG block_count;
#else
  DWORD block_count;
#endif
  char inquiry_string[32];
  FILE *fpAcsiImg;
  BYTE Id; //0-7
  BYTE cmd_block[16];
  BYTE n_cmd_bytes; //6=ACSI 10+1=SCSI
  BYTE Ready;
  BYTE cmd_ctr;
  BYTE STR,DR;
  BYTE error_code;
  BYTE Active; // can't be bool, can be 2
};


extern BYTE acsi_dev;

void agenda_acsi(int phase);

#endif//SSE_ACSI

#pragma pack(pop)

#endif//#ifndef SSEACSI_H
