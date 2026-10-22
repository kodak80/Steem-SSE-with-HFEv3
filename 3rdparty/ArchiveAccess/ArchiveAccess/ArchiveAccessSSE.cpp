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
FILE: ArchiveAccessSSE.cpp
CONDITION: SSE_ARCHIVEACCESS_SUPPORT must be defined; Windows-only
DESCRIPTION: Code interfacing Steem SSE with the archiveaccess plugin.
---------------------------------------------------------------------------*/

///////////////////////////////////////////////////////////////////////////////
// ArchiveAccessSSE.cpp for Steem SSE
// based on:
//
// ArchiveAccessTest.cpp 
// Copyright 2004 X-Ways Technology AG
// Author: Björn Ganster
// Licensed under the LGPL
//
///////////////////////////////////////////////////////////////////////////////

#include <pch.h>
#pragma hdrstop


#if defined(SSE_ARCHIVEACCESS_SUPPORT)

//#include "ArchiveAccessSSE.h" //no!
#include <options.h>
#include <debug.h>
#include <iolist.h>
#include <windows.h>
#include "../ArchiveAccess/bgstr.h"
#include "../ArchiveAccess/ArchiveAccessBase.h"

#if defined(BCC_BUILD)
#define max(a,b) ( (a)>(b)?(a):(b) ) //when 1st compiling as is
#endif

#include "../ArchiveAccess/ArchiveAccessDynamic.h"

#include <archive.h>
#include <acc.h>
#include <mymisc.h>


///////////////////////////////////////////////////////////////////////////////
#if defined(SSE_X64)
/*64-bit versions of Windows use 32-bit handles for interoperability. 
When sharing a handle between 32-bit and 64-bit applications, only the lower 
32 bits are significant, so it is safe to truncate the handle (when passing
it from 64-bit to 32-bit) or sign-extend the handle (when passing it from 
32-bit to 64-bit). Handles that can be shared include handles to user objects
such as windows (HWND), handles to GDI objects such as pens and brushes 
(HBRUSH and HPEN), and handles to named objects such as mutexes, semaphores,
and file handles.
https://docs.microsoft.com/en-us/windows/desktop/WinProg64/interprocess-communication
*/
#pragma warning( disable : 4302 4311 4312)
#endif
///////////////////////////////////////////////////////////////////////////////


// Dynamically loaded library

HINSTANCE hinstLib;

#define LOGSECTION LOGSECTION_INIT

bool LoadArchiveAccessDll (const TCHAR* LibName)
{
    hinstLib = SteemLoadLibrary(LibName); 
    if (hinstLib != NULL) {
        aaDetermineArchiveType = (paaDetermineArchiveType) GetProcAddress(hinstLib, "aaDetermineArchiveType");
//		aaInit = (paaInit) GetProcAddress(hinstLib, "aaInit");
        aaOpenArchiveFile = (paaOpenArchiveFile) GetProcAddress(hinstLib, "aaOpenArchiveFile");
        aaOpenArchive = (paaOpenArchive) GetProcAddress(hinstLib, "aaOpenArchive");
        aaGetFileCount = (paaGetFileCount) GetProcAddress(hinstLib, "aaGetFileCount");
        aaGetFileInfo = (paaGetFileInfo) GetProcAddress(hinstLib, "aaGetFileInfo");
        aaExtract = (paaExtract) GetProcAddress(hinstLib, "aaExtract");
        aaCloseArchive = (paaCloseArchive) GetProcAddress(hinstLib, "aaCloseArchive");
        TRACE_LOG("%s loaded\n",LibName);
        return true;
    } 
    else 
    {
        TRACE_LOG("%s not available\n",LibName);
        return false;
    }
}

#undef LOGSECTION

void UnloadArchiveAccessDll ()
{
    SteemFreeLibrary(hinstLib); 
}

#pragma warning(disable: 4293)//'<<' : shift count negative or too big, undefined behavior//382

///////////////////////////////////////////////////////////////////////////////
// Callback function for reading data

HRESULT __stdcall readCallback (int StreamID, INT64 offset, UINT32 count, 
                                void* buf, UINT32* processedSize)
{
   unsigned long ReadBytes;
   HANDLE handle = reinterpret_cast <HANDLE> (StreamID);
   long offsHi = (long) (offset >> 32);
   //long  offsLo = (long) (offset & ((1 << 32) -1));
   long offsLo=(long)offset; // bcc problem #1
   SetFilePointer (handle, offsLo, &offsHi, FILE_BEGIN);
   int result = ReadFile(handle, buf, count, &ReadBytes, NULL);
   if (processedSize != NULL)
       (*processedSize) = ReadBytes;
   if (result != 0)
     return S_OK;
   else
     return S_FALSE;
}


///////////////////////////////////////////////////////////////////////////////
// Callback function for writing data

HRESULT __stdcall writeCallback (int StreamID, INT64 offset, UINT32 count, 
                                 const void* buf, UINT32* processedSize)
{
  int result = 1;
  if (count != 0) {
    unsigned long procSize;
    HANDLE outFileHandle = reinterpret_cast <HANDLE> (StreamID);
    long offsHi = (long) (offset >> 32);
    //long offsLo = (long) (offset & ((1 << 32) -1));
    long offsLo=(long)offset; // bcc
    SetFilePointer (outFileHandle, offsLo, &offsHi, FILE_BEGIN);
    result = WriteFile (outFileHandle, buf, count, &procSize, NULL);
    if (processedSize != NULL)
      (*processedSize) = procSize;
  }
  if (result != 0)
  {
    if(processedSize!=NULL)
      add_to_crc32(zippy.crc,(const unsigned char *)buf,*processedSize); //10% slower
    return S_OK;
  }
  else
    return S_FALSE;
}


FileInArchiveInfo FileInfo;
WORD current_archived_file;
aaHandle ArchiveHandle=NULL;

void ArchiveAccess_Close() {
  if(ArchiveHandle) // or big crash...
  {
    aaCloseArchive (ArchiveHandle);
    CloseHandle(zippy.hArcData); // fixes handle leak + impossible to move file
  }
  ArchiveHandle=0;
}


bool ArchiveAccess_Extract(char *dest_dir) {
  HANDLE outFileHandle = 
    CreateFile (dest_dir, FILE_WRITE_DATA, FILE_SHARE_READ, 
    NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  UINT64 ExtractedFileSize;
  int oStreamID = reinterpret_cast <int> (outFileHandle);
  zippy.crc=~0u;
  aaExtract (ArchiveHandle, current_archived_file, oStreamID, writeCallback, 
    &ExtractedFileSize);
  zippy.crc=~zippy.crc;
  CloseHandle (outFileHandle);
  return true; //of course
}


#define LOGSECTION LOGSECTION_IMAGE_INFO

bool ArchiveAccess_Open(TCHAR* ArchiveFileName) { 
  // ArchiveFileName is the full path
  bool ok=false;
  wchar_t password[] = L"xxx";
  int OpenArchiveError;
  HANDLE FileHandle = CreateFile (ArchiveFileName, FILE_READ_DATA, 
                                       FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL, NULL);
  if(FileHandle!=(void*)-1) 
  {
    // Open archive
    int iStreamID = reinterpret_cast <int> (FileHandle);
    unsigned long FileSizeHi;
    UINT32 FileSizeLo = GetFileSize (FileHandle, &FileSizeHi);
//    INT64 FileSize = (FileSizeHi << 32) + FileSizeLo;
    INT64 FileSize = FileSizeHi;
    FileSize <<= 32;
    FileSize += FileSizeLo;
    int flags;
    int ArchiveType = 
      aaDetermineArchiveType (readCallback, iStreamID, FileSize, &flags);
    ArchiveHandle = aaOpenArchive(readCallback, iStreamID, FileSize,
      ArchiveType, &OpenArchiveError, password);
    zippy.hArcData=FileHandle; // memorise this
#if defined(SSE_ENABLE_TRACE_LOG)
    TRACE_LOG2("ArchiveAccess open %s Type %c%c%c%c flags %X: ERR %d handle:%d\n",
     CHECKPATH(ArchiveFileName),(ArchiveType>>(3*8))&0xFF,(ArchiveType>>(2*8))&0xFF,
     (ArchiveType>>8)&0xFF,ArchiveType&0xFF,flags,OpenArchiveError,iStreamID);
#endif
    if (OpenArchiveError == 0) { //OK
      current_archived_file=0; // first file
//  from ArchiveAccessClass:
// Unfortunately, not all archive formats store full information on the files 
// contained in them. If any data is missing, the function result will be set
// to S_FALSE to indicate this. In particular,
// - the file name may be missing. In this case, a valid file name may often
//   be built by truncating one extension (.bz2, .gz) from the archive file 
//   name or by appending .unpacked
// - the file size may be reported as 0 (bzip). In this case, the file size
//   can be obtained by unpacking the file to disk and reading the file size
//   from the unpacked file

      //BOOL err=
      aaGetFileInfo (ArchiveHandle,current_archived_file, &FileInfo); // was fine as is!
      //TRACE_LOG("sizeof(FileInfo)=%d\n",sizeof(FileInfo)); // 2104 vc + bcc
#if 0
      if(err==S_FALSE)
      {
        // oops it returns S_FALSE on everything? forget it then, nobody launches a bz2 file
        // at once, at least so the player could first load a disk with the correct
        // image extension, anyway, I've never seen a bz2 ST image
        //TRACE_LOG("aaGetFileInfo failed->dummy\n");
        //if(FileInfo.path==L"")
          wcscpy((wchar_t*)FileInfo.path,L"dummy.st");
        if(FileInfo.UncompressedFileSize==0)
          FileInfo.UncompressedFileSize=720*1024;
      }
#endif
      ok=true;
    }
  }
  return ok;
}


bool ArchiveAccess_Select(int n) {
  if(ArchiveHandle)
  {
    int FileCount = aaGetFileCount (ArchiveHandle);
    if(n<FileCount)
    {
      aaGetFileInfo (ArchiveHandle,n,&FileInfo);
      TRACE_LOG3("select %S\n",FileInfo.path);
      return true;
    }
  }
  return false;
}

#endif//SSE_ARCHIVEACCESS_SUPPORT
