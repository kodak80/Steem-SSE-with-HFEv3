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

DOMAIN: hard drive
FILE: hd_gemdos.h
DESCRIPTION: Declarations for Steem's virtual hard drive emulation based on
hacking TOS.
struct TStemdosFile, TStemdosFsnext, TStemdos
---------------------------------------------------------------------------*/

#pragma once
#ifndef STEMDOS_DECLA_H
#define STEMDOS_DECLA_H


#include "conditions.h"

#pragma pack(push, 8)

#define GEMDOS_MAXDRIVES 26 // A-Z
#define GEMDOS_STD_HANDLES 6

extern bool StemdosComlineReadRb;

void STfileReadToSTMemory(FILE*fp,MEM_ADDRESS ad,int nBytes);
bool StemdosCheckTrap1(); // returns true if intercepted (just in case)
void StemdosRte();

WORD STfileReadWord(FILE*f);
LONG STfileReadLong(FILE*f);
extern bool STfileReadError;


struct TStemdosFile {
  TStemdosFile();
  Str filename;
  FILE *fp;
  DWORD attrib;
  int Pexec; // ownwe as a number, not address
  WORD date,time;
  BYTE h;
  bool open;
};

extern TStemdosFile StemdosFile[MAX_STEMDOS_FILES];

struct TStemdosFsnext {
  TStemdosFsnext();
  EasyStr path;
  EasyStr NextFile;
  MEM_ADDRESS dta;
  DWORD start_hbl, attr;
};


struct TStemdos { // object exists even if STEMDOS disabled
  TStemdos();
  void Init();
  void Reset();
  void SetDriveReset();
  void UpdateDrvbits();
  bool IsMounted(BYTE a);
  int AnyFilesOpen();
  void CheckPaths();
  void CtrlC(); //control-c pressed
  void CloseAllFiles();
  bool ConvertDate(WORD date,int& y,int& m,int& d);
  bool ConvertTime(WORD time,int& h,int& m,int& s);
  // DATA
  TStemdosFsnext FsnextData[MAX_STEMDOS_FSNEXT_STRUCTS];
  EasyStr MountPath[GEMDOS_MAXDRIVES];   // PC path
  EasyStr CurrentPath[GEMDOS_MAXDRIVES]; // internal GEMDOS path
  MEM_ADDRESS PexecList[MAX_STEMDOS_PEXEC_LIST];
  MEM_ADDRESS env[MAX_STEMDOS_PEXEC_LIST];
  MEM_ADDRESS Dta;
  int PexecListPtr;
  BYTE BootDrive,CurrentDrive;
  bool DriveMounted[GEMDOS_MAXDRIVES];
  bool bInterceptDateTime;
};

extern TStemdos Stemdos;

#pragma pack(pop)

#endif//#ifndef STEMDOS_DECLA_H
