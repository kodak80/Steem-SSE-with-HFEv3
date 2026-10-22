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

DOMAIN: File
FILE: configstorefile.h
DESCRIPTION: A class to read and write ini properties file in standard Windows
format. This was necessary due to write-delay problems with
WritePrivateProfileString and other related functions.
SS: sometimes Steem authors were radical! 
struct TCsfFind, TCsfSects, TCsfKeys
class TConfigStoreFile
---------------------------------------------------------------------------*/

#pragma once
#ifndef CONFIGSTOREFILE_H
#define CONFIGSTOREFILE_H

#include <dynamicarray.h>
#include <easystr.h>
#include <easystringlist.h>

#pragma pack(push, 8)

struct TCsfFind {
  int iSect,iKey;
};

struct TCsfSects {
  char *szName,*szNameUpr;
};

struct TCsfKeys {
  char *szName,*szNameUpr,*szValue;
  int iSect;
};

class TConfigStoreFile {
private:
  EasyStr Path;
  EasyStr FileBuf,FileUprBuf;
  DynamicArray<TCsfSects> Sects;
  DynamicArray<TCsfKeys> Keys;
  DynamicArray<char*> szNewMem;
public:
#if defined(SSE_TRACE_DUMP_OPTIONS)
  bool NoPath;
#endif
  TConfigStoreFile(char *NewPath=NULL);
  ~TConfigStoreFile();
  bool Open(char *NewPath=NULL);
  bool Close();
  bool SaveTo(char *File);
  bool FindKey(EasyStr Sect,char *KeyVal,TCsfFind *pSK);
  bool GetBool(char *Sect,char *Key,bool DefVal);
  BYTE GetByte(char *Sect,char *Key,BYTE DefVal);
  int GetInt(char *Sect,char *Key,int DefVal);
  EasyStr GetStr(char *Sect,char *Key,char *DefVal);
  WORD GetWord(char *Sect,char *Key,WORD DefVal);
  void SetInt(char *Sect,char *Key,int val);
  void SetStr(char *Sect,char *Key,char *Val);
  void GetSectionNameList(EasyStringList* pESL);
  void DeleteSection(EasyStr Sect);
  bool GetWholeSect(EasyStringList *pESL,EasyStr Sect,bool Upr=false);
  bool Changed;
};

#pragma pack(pop)

#define WriteCSFInt(s,k,v,f) WriteCSFStr(s,k,EasyStr(v),f)
void WriteCSFStr(char *Sect,char *Key,char *Val,char *File);
EasyStr GetCSFStr(char *Sect,char *Key,char *DefVal,char *File);
int GetCSFInt(char *Sect,char *Key,int DefVal,char *File);

#endif//#ifndef CONFIGSTOREFILE_H

