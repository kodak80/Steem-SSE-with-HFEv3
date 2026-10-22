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
FILE: di_get_contents.h
DESCRIPTION: Declarations for the disk image recognition system that uses
the TOSEC database.
---------------------------------------------------------------------------*/

#pragma once
#ifndef DI_GET_CONTENTS_H
#define DI_GET_CONTENTS_H

#include <conditions.h>

int GetContentsFromDiskImage(char *Fil,char *szRetBuf,int iRetBufLen,
                               //DWORD &crc32);
                               DWORD *dwCRCs);
                               //int /*OnAmbiguity*/);
void GetContents_SearchDatabase(char *szFind,char *szRetBuf,int iRetBufLen);

//extern char GetContents_ListFile[512];
//extern char GetContents_ListFile[MAX_PATH];
extern char *GetContents_ListFile;
typedef void GETZIPCRCSPROC(char*,DWORD*,int);
typedef BYTE* CONVERTTOSTPROC(char*,int,int*);
extern GETZIPCRCSPROC *GetContents_GetZipCRCsProc;
#ifdef DEADC0DE
extern CONVERTTOSTPROC *GetContents_ConvertToSTProc;
#endif

#endif//#ifndef DI_GET_CONTENTS_H
