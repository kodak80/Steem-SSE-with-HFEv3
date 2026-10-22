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
FILE: disk_stx.cpp
CONDITION: SSE_DISK_STX must be defined
DESCRIPTION: Native STX disk image support: image manager
It is used both for direct support (mainly in fdc.cpp) and for
conversion to STW (in disk_stw.cpp)
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop


#if defined(SSE_DISK_STX)

#include <disk_stx.h>
#include <mymisc.h>
#include <debug.h>
#include <computer.h>
#include <iolist.h>


#define LOGSECTION LOGSECTION_IMAGE_INFO

using namespace NStx;


void TImageSTX::Close() {
  if(pStxImage!=NULL)
    free(pStxImage);
  Init();
}


BYTE TImageSTX::FindID(WORD& bytes,SHORT side,SHORT track,SHORT num) {
  // side, track and num are SHORT and not BYTE so that -1 ("don't care") is unambiguous
  TFloppyDisk &disc=FloppyDisk[Id]; // shorthand
  BYTE order=0xFF;
  WORD distance=INVALID_TRACKBYTES; // must be unsigned
  for(BYTE i=0;i<pTrackDesc->sectorCount;i++)
  {
    SHORT d=-bytes;
    if(pSectorDesc)
      d+=pSectorDesc[i].bitPosition/8; // Dungeon Master 0.0
    else
      d+=(SHORT)disc.PostIndexGap()+(SHORT)disc.RecordLength()*i; // Dungeon Master 0.1-0.79
    if(d<0)
      d+=disc.TrackBytes;
    if(d<distance) // We want the closest sector in case there are duplicates (Chimera 0.79.8)
    {
      //TRACE_LOG("i %d track %d\n",i,pSectorDesc[i].id.track);
      bool ok=(pSectorDesc==NULL||track==-1);
      if(!ok)
        ok=pSectorDesc[i].id.track==(BYTE)track && (side==-1||pSectorDesc[i].id.side==(BYTE)side)
          && (pSectorDesc[i].fdcFlags&(FDC_STR_RNF|FDC_STR_CRC))!=(FDC_STR_RNF|FDC_STR_CRC) // invalid ID
          && (num==-1||pSectorDesc[i].id.num==(BYTE)num);
      if(ok)
      {
        distance=d;
        if(pSectorDesc)
          id=pSectorDesc[order=i].id;
      } 
    }
  }
  bytes=distance; // if not found, it is INVALID_TRACKBYTES
  if(!pSectorDesc)
    order=(BYTE)(num-1);
  id.num=(BYTE)num;
  return order;
}


bool TImageSTX::GetSector(BYTE side,BYTE track,BYTE order) {
  WORD bytes=INVALID_TRACKBYTES; // don't care about timing
  bool ok=GetSector(side,track,order,bytes); // order is zero-based
  return ok;
}


bool TImageSTX::GetSector(BYTE side,BYTE track,BYTE sr,WORD &bytes) {
  TFloppyDisk &disc=FloppyDisk[Id]; // shorthand
  bool direct=(bytes==INVALID_TRACKBYTES); // true when called by overload
  bool ok=true;
  if(!direct)
    ok=LoadTrack(side,track);
  bFuzzySector=bMacrodos=false;
  SectorTiming=NORMAL_TIMING;
  BYTE found_pos=0xFF;
  if(ok)
  {
    found_pos=(direct) ? sr : FindID(bytes,-1,Fdc.tr,sr);
    if(found_pos==0xFF && bytes==INVALID_TRACKBYTES)
      ok=false;
    else if(pSectorDesc)
    {
      if(!direct&&(pSectorDesc[found_pos].fdcFlags&FDC_STR_RNF)) // closest ID has no data
      {
        bytes+=sizeof(TWD1772IDField); // try to find same ID with record
        found_pos=FindID(bytes,-1,Fdc.tr,sr);
      }
      for(BYTE i=0;i<found_pos;i++) // correct position in fuzzy table
      {
        if((pSectorDesc[i].fdcFlags&SECT_FLAG_FUZZY) && pFuzzyTable)
          pFuzzyTable+=pSectorDesc[i].id.nBytes();
      }
      disc.BytesPerSector=pSectorDesc[found_pos].id.nBytes();
      if(pSectorDesc[found_pos].readTime) // Populous 0.0.6
        SectorTiming=(pSectorDesc[found_pos].readTime*NORMAL_TIMING)/(32*disc.BytesPerSector);
      if(pSectorDesc[found_pos].fdcFlags&SECT_FLAG_FUZZY)
      {
        bFuzzySector=true;
        DiskEmu.BitRate=0x68; // arbitrary
      }
      if(pSectorDesc[found_pos].fdcFlags&SECT_FLAG_MACRODOS) // can be both fuzzy and macrodos
        bMacrodos=true; // as long as there are no different cases, we ignore the timing table
      SectorFlags=pSectorDesc[found_pos].fdcFlags&~(SECT_FLAG_FUZZY|SECT_FLAG_MACRODOS);
      id=pSectorDesc[found_pos].id;
    }
    else
    {
      disc.BytesPerSector=SECTOR_SIZE;
      SectorFlags=(side<disc.Sides&& track<disc.TracksPerSide&& sr<=disc.SectorsPerTrack)
        ? 0 : FDC_STR_RNF;
      id.num=sr;
      id.len=2; // 512 bytes
    }
    if(SectorFlags&FDC_STR_RNF)
      ok=false;
    if(bytes!=INVALID_TRACKBYTES)
      bytes+=38; // assume standard 22 E5 + 12 00 + 3 A1 + 1 FB
  }
  if(ok)
  {
    // locate sector data
    BYTE* pSectorData=pStxTrackData;
    if(pSectorDesc)
      pSectorData+=pSectorDesc[found_pos].dataOffset;
    else
      pSectorData+=found_pos*disc.BytesPerSector; // we know there's a sector because sectorCount>0
    long offset=(long)(pSectorData-pStxImage);
    FSEEK(disc.fp,offset,SEEK_SET); // directly pointing into file for Steem's FDC emulation
  }
  return ok;
}


void TImageSTX::Init() {
  BYTE s_id=Id; // keep Id
  memset(this,0,sizeof(TImageSTX));
  Id=s_id;
  CurrentSide=CurrentTrack=0xFF; // $FF is physically impossible for both side and track
}


bool TImageSTX::LoadTrack(BYTE side,BYTE track) {
  ASSERTB(side<=1);
  TFloppyDisk &disc=FloppyDisk[Id]; // shorthand
  bool ok=false;
  int tr=(side<<7)|track; // not the record order...
  pTrackDesc=pTrackDescTable[tr]; // can be NULL
  pSectorDesc=NULL;
  pTrackBytes=pFuzzyTable=NULL;
  FirstSync=0; // not INVALID_TRACKBYTES because Steem could randomize data up to FirstSync
  //TRACE_LOG("LoadTrack %c $%X:%d-%d %p\n",Id+'A',tr,side,track,pTrackDescTable[tr]);
  if(pTrackDesc!=NULL)
  {
    BYTE *p=(BYTE*)pTrackDesc+sizeof(TStxTrackDesc);
    if(pTrackDesc->trackFlags&TRK_SECT) // there's a track image
    {
      pSectorDesc=(TStxSectorDesc*)((BYTE*)pTrackDesc+sizeof(TStxTrackDesc));
      p+=sizeof(TStxSectorDesc)*pTrackDesc->sectorCount;
    }
    if(pTrackDesc->fuzzyCount)
      pFuzzyTable=p;
    p+=pTrackDesc->fuzzyCount;
    pStxTrackData=p;
    if(pTrackDesc->trackFlags&TRK_IMAGE) // #bytes could be different from Desc
    {
      if(pTrackDesc->trackFlags&TRK_SYNC) // 1st $A1 is indicated...
      {
        FirstSync=*(WORD*)p;
        p+=2;
      }
      disc.TrackBytes=*(WORD*)p;
      pTrackBytes=p+2; // pointing to data in memory 
      long offset=(long)(pTrackBytes-pStxImage);
      FSEEK(disc.fp,offset,SEEK_SET); // directly pointing into file
    }
    else if(pTrackDesc->trackLength!=INVALID_TRACKBYTES) // Discovery dumps
      disc.TrackBytes=pTrackDesc->trackLength;
    else // default
      disc.TrackBytes=DISK_BYTES_PER_TRACK;
    TrackTiming=(disc.TrackBytes*NORMAL_TIMING)/DISK_BYTES_PER_TRACK; // < = slower
    disc.SectorsPerTrack=pTrackDesc->sectorCount;
#if defined(SSE_ENABLE_TRACE_LOG) // track info
    if(CurrentSide!=side||CurrentTrack!=track)
    {
      TRACE_LOG("$%X %d.%d flags $%X %d bytes (%d, %d fuzzy) %d sectors 1st sync %d\n",
        pTrackDesc->trackNumber,side,track,pTrackDesc->trackFlags,pTrackDesc->trackLength,
        disc.TrackBytes,pTrackDesc->fuzzyCount,pTrackDesc->sectorCount,FirstSync);
    }
#endif
    if(pTrackDesc->trackFlags&TRK_SECT)
    {
      pSectorDesc=(TStxSectorDesc*)((BYTE*)pTrackDesc+sizeof(TStxTrackDesc)); // start of descriptors
#if defined(SSE_ENABLE_TRACE_LOG) && defined(SSE_DEBUGGER_FAKE_IO) // sector info
      if( (CurrentSide!=side||CurrentTrack!=track)
        &&(TRACE_LOG_ENABLED(LOGSECTION_IMAGE_INFO))&&TRACE_LOG2_ENABLED)
      {
        for(BYTE sr=0;sr<pTrackDesc->sectorCount;sr++)
        {
          TRACE_LOG("%02d: flags $%02X at %d time %d ",sr,pSectorDesc[sr].fdcFlags,pSectorDesc[sr].bitPosition/8,pSectorDesc[sr].readTime);
          pSectorDesc[sr].id.Trace(LOGSECTION_IMAGE_INFO);
        }
      }
#endif
    }
    ok=true;
  } 
  else // no track record for this side/cylinder
    disc.TrackBytes=DISK_BYTES_PER_TRACK;
  id.side=CurrentSide=side,id.track=CurrentTrack=track;
  return ok;
}


bool TImageSTX::Open(EasyStr FilePath) {
  Close();
  TFloppyDisk &disc=FloppyDisk[Id]; // shorthand
  bool ok=false;
  TStxFileDesc *pStxFileDesc=NULL; // pointer to the image header (no need for a member variable)
  if(disc.fp==NULL) // can be already open by SetDisk()
  {
    disc.fp=fopen(FilePath,(disc.ReadOnly) ? "rb" : "r+b");
    if(disc.fp==NULL)
      disc.fp=fopen(FilePath,"rb");
  }
  if(disc.fp!=NULL)
  {
    LONG FileSize=(LONG)GetFileLength(disc.fp);
    if(FileSize)
    {
      pStxImage=(BYTE*)malloc(FileSize);
      if((LONG)FREAD(pStxImage,1,FileSize,disc.fp)==FileSize)
        ok=true;
    }
  }
  if(ok)
  {
    pStxFileDesc=(TStxFileDesc*)pStxImage;
    if(strncmp("RSY",(char*)pStxFileDesc->pastiFileId,3)) // what's RSY stand for?
      ok=false; // it's not a pasti file
    if(ok && pStxFileDesc->version<3)
      ok=false; // only v3 described, assume higher versions compatible
    TRACE_LOG("%c %c%c%c tool $%X v%dR%d %d track records ok%d ?%d ?%d\n",Id+'A',
      pStxFileDesc->pastiFileId[0],pStxFileDesc->pastiFileId[1],pStxFileDesc->pastiFileId[2], // RSY
      pStxFileDesc->tool,pStxFileDesc->version,pStxFileDesc->revision,pStxFileDesc->trackCount,ok,
      pStxFileDesc->reserved_1,pStxFileDesc->reserved_2);
  }
  if(ok)
  {
    DWORD StxPosition=sizeof(TStxFileDesc);
    BYTE MaxTrack=0,Sides=1;
    for(BYTE tr=0;tr<pStxFileDesc->trackCount;tr++)
    {
      //pTrackDescTable[tr]=pTrackDesc=(TStxTrackDesc*)(pStxImage+StxPosition); // bug ! depends on correct track order
      pTrackDesc=(TStxTrackDesc*)(pStxImage+StxPosition);
      ASSERTB(pTrackDesc);
      pTrackDescTable[pTrackDesc->trackNumber]=pTrackDesc; // tracks can be in any order (Spikey in Transylvania)
      if(pTrackDesc->trackNumber&0x80)
        Sides=2; // as soon as a side 1 record is detected, we have two sides
      BYTE track=pTrackDesc->trackNumber&0x7F; // floppies have 80+ tracks, will never reach 128
      if(track>MaxTrack)
        MaxTrack=track;
      StxPosition+=pTrackDesc->recordSize; // the image is like a linked chain
      //BYTE side=(pTrackDesc->trackNumber&0x80)>>7;
      //TRACE_LOG("#%d: track number $%X:%d-%d - %d sectors %d bytes at %p\n",
      //  tr,pTrackDesc->trackNumber,side,track,pTrackDesc->sectorCount,pTrackDesc->recordSize,pTrackDesc);
      //LoadTrack(side,track);
    }
    disc.TracksPerSide=MaxTrack+1;
    disc.Sides=Sides;
  }
  return ok;
}

#undef LOGSECTION

#endif//#if defined(SSE_DISK_STX)
