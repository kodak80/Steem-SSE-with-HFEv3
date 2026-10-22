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

DOMAIN: Emu
FILE: circularbuffer.cpp
DESCRIPTION: Class to implement a circular FIFO buffer.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include "circularbuffer.h"
#ifdef MINGW_BUILD
#undef NULL
#define NULL 0
#endif


CircularBuffer::CircularBuffer(DWORD Size) {
  Buf=NULL;
  Lock=false;
  if(Size>=2)
    Create(Size);
}


CircularBuffer::~CircularBuffer() { 
  Destroy(); 
}


bool CircularBuffer::AreBytesInBuffer() {
  return ( Buf!=NULL && pCurRead!=pCurWrite-1 
    && !(pCurRead==pEnd-1&&pCurWrite==pStart) );
}


BYTE CircularBuffer::ReadByte() {
#if defined(SSE_DIRECTMIDI)
  if(Buf)
    LastTiming=*pRTiming;
#endif
  BYTE b=(Buf==NULL) ? 0 : *pCurRead;
  return b;
}


#ifndef WIN32
// On Windows Sleep(0) will let another thread take over instantly
int CircularBuffer::Sleep(int n) { 
  return n; 
}
#endif


bool CircularBuffer::IsLocked() { 
  return Lock; 
}


bool CircularBuffer::Create(DWORD Size) {
  if(Buf||Size<2) 
    return false;
#if defined(SSE_BADALLOC)
  Buf=new BYTE[Size];
#if defined(SSE_DIRECTMIDI)
  Timing=new REFERENCE_TIME[Size];
  pRTiming=pWTiming=Timing;
#endif
#else
  try
  {
    Buf=new BYTE[Size];
#if defined(SSE_DIRECTMIDI)
    Timing=new REFERENCE_TIME[Size];
    pRTiming=pWTiming=Timing;
#endif
  }
  catch(...)
  {
    return false;
  }
#endif
  BufSize=Size;
  pStart=Buf;
  pEnd=Buf+BufSize;
  Reset();
  return true;
}


#if defined(SSE_DIRECTMIDI) // record timestamp with byte

bool CircularBuffer::AddByte(BYTE Data,REFERENCE_TIME timing) {
  if(Buf==NULL) 
    return false;
  while(Lock)
#if (_WIN32_WINNT>=0x0400)
    SwitchToThread();
#else
    Sleep(0);
#endif
  Lock=true;
  bool Overflow=(pCurRead==pCurWrite);
  *(pCurWrite++)=Data;
  *(pWTiming++)=timing;
  if(pCurWrite>=pEnd)
  {
    pCurWrite=pStart;
    pWTiming=Timing;
  }
  if(Overflow)
  {
    pCurRead=pCurWrite;
    pRTiming=pWTiming;
  }
  Lock=false;
  return !Overflow;
}


bool CircularBuffer::AddBytes(BYTE *pData,DWORD DataLen,REFERENCE_TIME timing) {
  if(Buf==NULL||DataLen>=BufSize) 
    return false;
  bool ok=true;
  for(DWORD i=0;i<DataLen&&ok;i++)
    ok=AddByte(*(pData+i),timing);
  return ok;
}

#else

bool CircularBuffer::AddByte(BYTE Data) {
  if(Buf==NULL) 
    return false;
  while(Lock)
#if (_WIN32_WINNT>=0x0400)
    SwitchToThread();
#else
    Sleep(0);
#endif
  Lock=true;
  bool Overflow=(pCurRead==pCurWrite);
  *(pCurWrite++)=Data;
  if(pCurWrite>=pEnd) 
    pCurWrite=pStart;
  if(Overflow) 
    pCurRead=pCurWrite;
  Lock=false;
  return !Overflow;
}


bool CircularBuffer::AddBytes(BYTE *pData,DWORD DataLen) {
  if(Buf==NULL||DataLen>=BufSize) 
    return false;
  while(Lock)
#if (_WIN32_WINNT>=0x0400)
    SwitchToThread();
#else
    Sleep(0);
#endif
  Lock=true;
  bool Overflow=false;
  BYTE *pOldWrite=pCurWrite;
  if(pCurWrite+DataLen<pEnd)
  {
    pCurWrite+=DataLen;
    if(pCurRead>=pOldWrite && pCurRead<pCurWrite)
    {
      pCurRead=pCurWrite;
      Overflow=true;
    }
    Lock=false;
    memcpy(pOldWrite,pData,DataLen);
  }
  else
  {
    bool Overlap=(pCurRead>=pCurWrite);
    LONG_PTR ToEnd=pEnd-pCurWrite;
    pCurWrite=pStart+(DataLen-ToEnd);
    if(pCurRead<pCurWrite||Overlap)
    {
      pCurRead=pCurWrite;
      Overflow=true;
    }
    Lock=false;
    memcpy(pOldWrite,pData,ToEnd);
    memcpy(pStart,pData+ToEnd,DataLen-ToEnd);
  }
  return !Overflow;
}

#endif//#if defined(SSE_DIRECTMIDI)


void CircularBuffer::NextByte() {
  while(Lock) 
#if (_WIN32_WINNT>=0x0400)
    SwitchToThread();
#else
    Sleep(0);
#endif
  if(AreBytesInBuffer())
  {
#if defined(SSE_DIRECTMIDI)
    pRTiming++;
#endif
    if((++pCurRead)>=pEnd)
    {
      pCurRead=pStart;
#if defined(SSE_DIRECTMIDI)
      pRTiming=Timing;
#endif
    }
  }
}


void CircularBuffer::Reset() {
  if(Buf==NULL) 
    return;
  while(Lock) 
#if (_WIN32_WINNT>=0x0400)
    SwitchToThread();
#else
    Sleep(0);
#endif
  Buf[0]=0;
  pCurRead=pStart;
  pCurWrite=pStart+1;
#if defined(SSE_DIRECTMIDI)
  pRTiming=Timing;
  pWTiming=pRTiming+1;
#endif

}


void CircularBuffer::Destroy() {
  if(Buf==NULL) 
    return;
  while(Lock) 
#if (_WIN32_WINNT>=0x0400)
    SwitchToThread();
#else
    Sleep(0);
#endif
  delete[] Buf;Buf=NULL;
#if defined(SSE_DIRECTMIDI)
  delete[] Timing;
#endif
}
