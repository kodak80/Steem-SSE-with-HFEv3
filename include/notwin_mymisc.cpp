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

DOMAIN: Various
FILE: notwin_mymisc.cpp
CONDITION: UNIX
DESCRIPTION: implementation of some Windows functions in Linux
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#ifdef UNIX

#include <conditions.h>
#include <stdio.h>
#include "easystr.h"

void ZeroMemory(void *Mem,DWORD Len) {
  memset(Mem,0,Len);
}


bool DeleteFile(char *File) {
  return unlink(File)==0;
}


UINT GetTempFileName(char *PathName,char *Prefix,UINT Unique,
                      char *TempFileName) {
  EasyStr Ret;
  WORD Num=(WORD)((Unique)?(WORD)Unique:(WORD)(rand()&0xffff));
  for(;;)
  {
    Ret=PathName;
    Ret+=SLASH;
    Ret+=Prefix;
    Ret.SetLength(MAX_PATH+4);
    char *StartOfNum=Ret.Right()+1;
#ifdef MINGW_BUILD
    _ultoa(Num,StartOfNum,16);
#else
    ultoa(Num,StartOfNum,16);
#endif
    strupr(StartOfNum);
    Ret+=".TMP";
    if(Ret.Length()<MAX_PATH)
    {
      if(Unique==0)
      {
        if(access(Ret,0)==0)
          //File exists
          Num++;
        else
        {
          strcpy(TempFileName,Ret);
          fclose(fopen(TempFileName,"wb"));
          return Num;
        }
      }
      else
      {
        strcpy(TempFileName,Ret);
        return Num;
      }
    }
    else
      return 0;
  }
}


BOOL CopyFile(LPCSTR lpExistingFileName,LPCSTR lpNewFileName,BOOL bFailIfExists) {
  if(!Exists(lpExistingFileName)||Exists(lpNewFileName))
    return FALSE;
  FILE* source = fopen(lpExistingFileName, "rb");
  FILE* dest = fopen(lpNewFileName, "wb");

  // clean and more secure
  // feof(FILE* stream) returns non-zero if the end of file indicator for stream is set
  size_t size;
  BYTE buf;
  while (size = fread(&buf, 1, 1, source)) {
      fwrite(&buf, 1, size, dest);
  }

  fclose(source);
  fclose(dest);
  return TRUE;
}

#endif//UNIX
