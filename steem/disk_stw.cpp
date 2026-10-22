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
FILE: disk_stw.cpp
CONDITION: SSE_DISK_STW must be defined
DESCRIPTION: STW is yet another Atari ST disk image format, devised for Steem
SSE. The W in STW stands for 'write', ST is a reference to the well known 
ST format and of course to the Atari ST itself.
The purpose of this format is to allow emulation of all WD1772 (floppy
disk controller) commands in Steem SSE, and keep the results of command
Write Track (Format).
We really go for it here as this simplistic interface knows nothing but
the side, track and words on the track. MFM encoding/decoding, timing and
all the rest is for the distinct floppy drive and fdc emulators.

v4.0
We handle ST/MSA/DIM <-> STW conversion here, those utility routines do
know about disk structure.

v4.1.2
If SSE_DISK_STW2 is defined, support for STW v2 is compiled
STW v2 contains fuzzy byte and timing information
We handle SCP -> STW conversion here

v4.2.0
We handle STX -> STW conversion here
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#if defined(SSE_DISK_STW)
 
#include <debug.h>
#include <computer.h>
#include <osd.h>
#include <iolist.h>


#define HEADER_SIZE (4+2+1+1+2) //STW Version nSides nTracks nTrackWords
#define TRACK_HEADER_SIZE (3+2) //"TRK" side track
#define IMAGE_SIZE (HEADER_SIZE+nSides*nTracks*(TRACK_HEADER_SIZE+nTrackWords*sizeof(WORD)))

#define LOGSECTION LOGSECTION_IMAGE_INFO


TImageSTW::TImageSTW() {
  Init();
}


TImageSTW::~TImageSTW() {
  Close();
}


// save if necessary and close file
void TImageSTW::Close() {
  if(fCurrentImage)
  {
    TRACE_LOG("STW %d %s image\n",Id,FloppyDisk[Id].WrittenTo?"save and close":"close");
    FSEEK(fCurrentImage,0,SEEK_SET); // rewind
    if(ImageData && FloppyDisk[Id].WrittenTo)
      FWRITE(ImageData,1,IMAGE_SIZE,fCurrentImage);
    fclose(fCurrentImage);
    if(ImageData)
      free(ImageData);
  }
  Init(); // zeroes variables
}


bool TImageSTW::Create(char *path,BYTE density) {
  // utility called by Disk manager and for auto MFM conversion
  //ASSERT(Id==0);
  bool ok=false;
  Close();
  fCurrentImage=fopen(path,"wb+"); // create new image
  if(fCurrentImage)
  {
    // write header
    FWRITE(DISK_EXT_STW,1,4,fCurrentImage); // "STW"
    SWAP_BIG_ENDIAN_WORD(Version);
    FWRITE(&Version,sizeof(WORD),1,fCurrentImage);
    SWAP_BIG_ENDIAN_WORD(Version);
    FWRITE(&nSides,sizeof(BYTE),1,fCurrentImage);
    FWRITE(&nTracks,sizeof(BYTE),1,fCurrentImage);
#if defined(SSE_DISK_STW2)
    nTrackWords=(DISK_BYTES_PER_TRACK*density)/2;
#else
    nTrackWords=DISK_BYTES_PER_TRACK*density;
#endif
    SWAP_BIG_ENDIAN_WORD(nTrackWords);
    FWRITE(&nTrackWords,sizeof(WORD),1,fCurrentImage);
    SWAP_BIG_ENDIAN_WORD(nTrackWords);
    // init all tracks with random bytes (unformatted disk) 
    for(BYTE track=0;track<nTracks;track++)
    {
      for(BYTE side=0;side<nSides;side++)
      {
        // this can be seen as "metaformat"
        FWRITE("TRK",1,3,fCurrentImage);
        FWRITE(&side,1,1,fCurrentImage);
        FWRITE(&track,1,1,fCurrentImage);

        WORD data; // MFM encoding = clock byte and data byte mixed
        for(int byte=0;byte<nTrackWords;byte++)
        {
          data=(WORD)rand();
          FWRITE(&data,sizeof(data),1,fCurrentImage); 
        }
      }
    }
    ok=true;
    Close(); 
  }
  TRACE_LOG("STW create %s %s\n",CHECKPATH(path),ok?"OK":"failed");  
  return ok;
}


WORD TImageSTW::GetImageWord(WORD position) {
  WORD mfm_data=0;
  if(TrackData&&position<nTrackWords)
  {
    mfm_data=TrackData[position];
    SWAP_BIG_ENDIAN_WORD(mfm_data);
  }
  return mfm_data;
}


WORD TImageSTW::GetMfmData(WORD position) {
  WORD mfm_data=TImageMfm::GetMfmData(position);
  if(!LowLevel)
    IncPosition();
  return mfm_data;
}


bool TImageSTW::GetSectorData(BYTE side,BYTE track,BYTE sector,BYTE *pdata) {
  //  used for conversion back to ST and for disk properties
#if defined(SSE_DISK_STW2)
  if(Version>=0x200)
    return false;
#endif
  TWD1772MFM wd1772mfm;
  // for each sector we start from the index, that's because 11-sector
  // tracks are interleaved and we don't care for performance
  bool old_ll=LowLevel;
  LowLevel=false;
  Position=0;
  bool ok=LoadTrack(side,track); // also inefficient, load every time (it's just a pointer for STW)
  bool found_header=false,found_data=false;
  int n4489=0,nbytes=0;
  for(int i=0;ok && i<nTrackWords;i++)
  {
    wd1772mfm.encoded=GetMfmData(0xFFFF); // it ++
    if(wd1772mfm.encoded==0x4489)
      n4489++;
    wd1772mfm.Decode();
    if(found_data)
    {
      pdata[nbytes]=wd1772mfm.data;
      nbytes++;
      if(nbytes==SECTOR_SIZE)
      {
        //TRACE_LOG("written sector %d %d %d\n",side,track,sector2);
        ok=true;
        break; // sector done
      }
    }
    else switch(n4489) {
    case 0:case 1: case 2: 
      if(wd1772mfm.encoded!=0x4489)
        n4489=0; // 3 or nothing
      break;
    case 3: // 3A1 = Address Mark
      if(wd1772mfm.encoded==0x4489)
      {} // 3rd a1
      else if((wd1772mfm.data&0xFE)==0xFE)
        n4489++; // followed with track side sector
      else 
      {
        if((wd1772mfm.data&0xFE)==0xFA && found_header) // $FB
          found_data=true; // followed with 512 words of MFM data to convert
        n4489=0;
      }
      break;
    case 4: // 4-5-6 find header, we don't care about CRC
      if(wd1772mfm.data==track)
        n4489++;
      else
        n4489=0;
      break;
    case 5:
      if(wd1772mfm.data==side)
        n4489++;
      else
        n4489=0;
      break;
    case 6:
      if(wd1772mfm.data==sector)
      {
        found_header=true;
        //TRACE_LOG("found header %d %d %d\n",side,track,sector2);
      }
      n4489=0;
      break;
    }//sw
  }
  LowLevel=old_ll;
  return ok;
}


void TImageSTW::Init() {
  Version=0x0100; // 1.0
  fCurrentImage=NULL;
  ImageData=NULL;
  TrackData=NULL;
  nSides=2;
  nTracks=84;
  // STW v2
  nTrackWords=DISK_BYTES_PER_TRACK;
  nTrackBits=nTrackWords*16;
  CurrentSide=CurrentTrack=0xFF;
}


bool  TImageSTW::LoadTrack(BYTE side,BYTE track,bool) {
  //ASSERT(Id==0||Id==1);
  bool ok=false;
  if(side<nSides && track<nTracks && ImageData)  
  {
    int position=HEADER_SIZE
      +track*nSides*(TRACK_HEADER_SIZE+nTrackWords*sizeof(WORD))
      +side*(TRACK_HEADER_SIZE+nTrackWords*sizeof(WORD));
    //runtime format check
    if( !strncmp("TRK",(char*)ImageData+position,3) 
      && *(ImageData+position+3)==side && *(ImageData+position+4)==track)
    {
#if defined(SSE_ENABLE_TRACE_LOG)
      if(TrackData!=(WORD*)(ImageData+position+TRACK_HEADER_SIZE)) //only once
        TRACE_LOG("STW LoadTrack %c: side %d track %d\n",'A'+DRIVE,side,track);  
#endif
      FloppyDisk[Id].current_side=side;
      FloppyDisk[Id].current_track=track;
      TrackData=(WORD*)(ImageData+position+TRACK_HEADER_SIZE);
      nTrackBits=nTrackWords*16;
      ok=true;
    }
  }
#if defined(SSE_ENABLE_TRACE_LOG)
  else
    TRACE_LOG("STW can't load side %d track %d from %p\n",side,track,ImageData);
#endif
  return ok;
}


bool TImageSTW::Open(char *path) {
  bool ok=false;
  Close(); // make sure previous image is correctly closed
  fCurrentImage=fopen(path,"rb+"); // try to open existing file
  if(!fCurrentImage) // maybe it's read-only
    fCurrentImage=fopen(path,"rb");
  if(fCurrentImage) // image exists
  {
    // read header first to check size (dd or hd)
    BYTE header[10];
    FREAD(header,1,10,fCurrentImage);
    header[3]='\0';
    FSEEK(fCurrentImage,0,SEEK_SET);
    if(!strncmp(DISK_EXT_STW,(char*)header,3)) // it's STW
    {
      Version=*(WORD*)(header+4);
      SWAP_BIG_ENDIAN_WORD(Version);
      if(Version>=0x100 && Version <0x200)
        ok=true;
      nSides=*(BYTE*)(header+6);
      nTracks=*(BYTE*)(header+7);
      nTrackWords=*(WORD*)(header+8);
      SWAP_BIG_ENDIAN_WORD(nTrackWords);
      if(ok)
        ImageData=(BYTE*)malloc(IMAGE_SIZE); //actual size
    }
    if(ImageData)
    {
      FREAD(ImageData,1,IMAGE_SIZE,fCurrentImage); 
    //  if(!strncmp(DISK_EXT_STW,(char*)ImageData,3)) // it's STW
      {
//#if defined(SSE_ENABLE_TRACE_LOG)
        // check meta-format
        for(BYTE track=0;track<nTracks;track++)
        {
          for(BYTE side=0;side<nSides;side++)
          {
            int position=HEADER_SIZE
              +track*nSides*(TRACK_HEADER_SIZE+nTrackWords*sizeof(WORD))
              +side*(TRACK_HEADER_SIZE+nTrackWords*sizeof(WORD));
            if(strncmp("TRK",(char*)ImageData+position,3)
              ||  *(ImageData+position+3)!=side 
              ||  *(ImageData+position+4)!=track )
              ok=false;
          }//nxt side
        }//nxt track
        //ASSERT(ok);
        TRACE_LOG("Open STW %s, V%X S%d T%d B%d OK%d\n",CHECKPATH(path),
                  Version,nSides,nTracks,nTrackWords,ok);
//#endif
      }
    }//if(ImageData)
    if(!ok)
    {
      fclose(fCurrentImage);
      fCurrentImage=NULL;
    }
  }//if(fCurrentImage)
  if(!ok)
    Close();
  else 
  {
    FloppyDisk[Id].TrackBytes=nTrackWords;
    FloppyDrive[Id].MfmManager=this;
    LowLevel=SSEOptions.MfmLowLevel;
  }
  return ok;
}


void TImageSTW::SetImageWord(WORD position,WORD mfm_data) {
  if(TrackData && position<nTrackWords)
  {
    TrackData[position]=mfm_data;
    SWAP_BIG_ENDIAN_WORD(TrackData[position]);
  }
}


void TImageSTW::SetMfmData(WORD position,WORD mfm_data) {
  TImageMfm::SetMfmData(position,mfm_data);
  if(!LowLevel)
  {
    SetImageWord(Position,mfm_data);
    IncPosition();
  }
}


#if defined(SSE_DISK_AUTOSTW)
/*  Conversion functions STW <-> ST, MSA, DIM.
    Images can be converted on the fly on insertion and ejection.
    This is used for option 'MFM emulation'
    We use data and functions of FloppyDisk and FloppyDrive to access the
    ST/MSA/DIM image. Assume: such image is inserted in drive id.
    That way, we don't need to care for the differences between ST, MSA and
    DIM.
*/

void wd1772_write_stw(BYTE id,BYTE data,TWD1772MFM* wd1772mfm,
                      TWD1772Crc* wd1772crc,int& p,int write_mode) {
  if(!write_mode)
    wd1772crc->Add(data);
  wd1772mfm->data=data;
  //wd1772mfm->Encode((write_mode==1)?(TWD1772MFM::FORMAT_CLOCK)
  //  :(TWD1772MFM::NORMAL_CLOCK));
  wd1772mfm->Encode(write_mode);
  if(write_mode==1)
    wd1772crc->Reset();
  ImageSTW[id].SetMfmData(0xFFFF,wd1772mfm->encoded);
  p++;
}

#define WD1772_WRITE(d) wd1772_write_stw(id,d,&wd1772mfm,&wd1772crc,p,0);
#define WD1772_WRITE_A1 wd1772_write_stw(id,0xA1,&wd1772mfm,&wd1772crc,p,1);
#define WD1772_WRITE_CRC(d) wd1772_write_stw(id,d,&wd1772mfm,&wd1772crc,p,2);


// convert ST/MSA/DIM to STW
bool STtoSTW(BYTE id,char *dst_path) {
  // id = drive which contains the image to convert (with FILE pointer)
  // dst_path = path to STW disk image to  create

  bool old_ll=ImageSTW[id].LowLevel;
  ImageSTW[id].LowLevel=false;
  ImageSTW[id].Version=0x0100;
  TWD1772MFM wd1772mfm;
  TWD1772Crc wd1772crc;
  TWD1772IDField wd1772id;
  TFloppyDisk &disc=FloppyDisk[id]; // shorthand
#if defined(SSE_DISK_STW2)
  BYTE density=(disc.SectorsPerTrack<=11 || disc.BytesPerSector<512) ? 2 : 4;
#else
  BYTE density=(disc.SectorsPerTrack<=11 || disc.BytesPerSector<512) ? 1 : 2;
#endif
  bool ok=ImageSTW[id].Create(dst_path,density);
  if(ok)
    ok=ImageSTW[id].Open(dst_path);
  int Sector_length_code=wd1772id.GetLen(disc.BytesPerSector);
  for(BYTE track=0;ok && track<disc.TracksPerSide;track++)
  {
    for(BYTE side=0;ok && side<disc.Sides;side++)
    {
      if(!ImageSTW[id].LoadTrack(side,track))
      {
        TRACE_LOG("can't load track %d %d\n",side,track);
        ok=false;
      }
      ImageSTW[id].Position=0;
      int p=0;
      for(int i=0;i<disc.PostIndexGap();i++)
        WD1772_WRITE(0x4E)
      BYTE sector;
      // write all sectors of this track/side
      for(BYTE sector2=1;ok && sector2<=disc.SectorsPerTrack;sector2++)
      {
        //  We must use interleave 6 for 11 sectors, eg Pang -EMP
        sector=disc.SectorsPerTrack==11 ? ((((sector2-1)*DISK_11SEC_INTERLEAVE)%11)+1) : sector2;
        if(!disc.SeekSector(side,track,sector,false,false))
        {
          TRACE_LOG("can't find sector %d %d %d\n",side,track,sector);
          break; // not in source, write nothing more
        }
        for(int i=0;i<(disc.SectorsPerTrack==11?3:12);i++)
          WD1772_WRITE(0)
        for(int i=0;i<3;i++)
          WD1772_WRITE_A1
        // write sector ID
        WD1772_WRITE(0xFE)
        WD1772_WRITE(track)
        WD1772_WRITE(side)
#if defined(SSE_DSKOS9)
        if(disc.BytesPerSector==256)
          sector--; // OS9 has sectors starting at 0, not 1
#endif
        WD1772_WRITE(sector)
        WD1772_WRITE((BYTE)Sector_length_code)
        WD1772_WRITE_CRC(wd1772crc.crc>>8)
        WD1772_WRITE_CRC(wd1772crc.crc&0xFF)
        for(int i=0;i<22;i++)
          WD1772_WRITE(0x4E)
        for(int i=0;i<12;i++) // 11 sectors too?
          WD1772_WRITE(0)
        for(int i=0;i<3;i++)
          WD1772_WRITE_A1
        // write sector data
        WD1772_WRITE(0xFB)
        BYTE b=0;
        for(int i=0;ok && i<disc.BytesPerSector;i++)
        {
          if(FREAD(&b,1,1,disc.fp)!=1)
          {
            TRACE_LOG("fail read byte %d %d %d %d\n",side,track,sector,i);
            ok=false;
          }
          WD1772_WRITE(b)
        }
        WD1772_WRITE_CRC(wd1772crc.crc>>8)
        WD1772_WRITE_CRC(wd1772crc.crc&0xFF)
        WD1772_WRITE(0xFF) // so 1 byte gap fewer ?
        for(int i=0;i<(disc.SectorsPerTrack==11?1-1:40-1);i++)
          WD1772_WRITE(0x4E)
      }//sector
      // pre-index gap: the rest
      while(p<disc.TrackBytes)
        WD1772_WRITE(0x4E)
      //TRACE_LOG("STW track %d/%d written %d sectors %d bytes\n",side,track,sector,p);
    }//side
  }//track
  ImageSTW[id].LowLevel=old_ll;
  TRACE_LOG("STtoSTW(%s) ok%d\n",CHECKPATH(dst_path),ok);
  return ok;
}

#undef WD1772_WRITE
#undef WD1772_WRITE_A1
#undef WD1772_WRITE_CRC


bool STWtoST(BYTE id) { // STW -> ST/MSA/DIM
  // ImageSTW[id] contains a valid STW image that we convert back to ST
  // FloppyDisk[id] has the correct FILE pointer
  bool old_ll=ImageSTW[id].LowLevel;
  ImageSTW[id].LowLevel=false;
  bool ok=(ImageSTW[id].ImageData!=NULL && FloppyDisk[id].fp!=NULL);
  for(BYTE track=0;ok && track<FloppyDisk[id].TracksPerSide;track++)
  {
    for(BYTE side=0;ok && side<FloppyDisk[id].Sides;side++)
    {
      BYTE pdata[SECTOR_SIZE]; // sector buffer
      for(BYTE sector=1;ok && sector<=FloppyDisk[id].SectorsPerTrack;sector++)
      {
        if(!FloppyDisk[id].SeekSector(side,track,sector,false,false))
        {
          TRACE_LOG("sector %d %d %d not found\n",side,track,sector);
          ok=false; // not on dest image
        }
        if(!ImageSTW[id].GetSectorData(side,track,sector,pdata))
        {
          TRACE_LOG("STW %d can't retrieve sector %d %d %d\n",id,side,track,sector);
          ok=false;
        }
        else if(FWRITE(&pdata,sizeof(BYTE),SECTOR_SIZE,FloppyDisk[id].fp)!=SECTOR_SIZE)
        {
          TRACE_LOG("fail write sector %d %d %d\n",side,track,sector);
          ok=false; // read-only -> close STW image and leave
        }
      }//sector
    }//side
  }//track
  ImageSTW[id].Close();
  ImageSTW[id].LowLevel=old_ll;
  return ok;
}

#endif//#if defined(SSE_DISK_AUTOSTW)

#undef HEADER_SIZE
#undef TRACK_HEADER_SIZE
#undef IMAGE_SIZE

#endif//#if defined(SSE_DISK_STW)


////////////
// STW v2 //
////////////

#if defined(SSE_DISK_STW2)
/*  STWv2 adds fuzzy bit, timing information and variable track length to the
*   STW format. It has become the ultimate WD1772 format! We dont't call it
*   that way because its practical use is limited. Extension is still STW.
*/

#define STW2VERSION 0x200
#define SOURCE_STEEMSSE 1
#define HEADER_SIZE 16
#define TRACK_HEADER_SIZE 16


TImageSTW2::TImageSTW2() {
  Init();
}


TImageSTW2::~TImageSTW2() {
  Close();
}


void TImageSTW2::Close() {
  if(Version<0x200) // STWv2 managers replace STWv1 ones, we must test version
  {                 // it isn't very elegant TODO
    TImageSTW::Close();
    return;
  }
  if(fCurrentImage)
  {
    TRACE_LOG("STW %d %s image\n",Id,FloppyDisk[Id].WrittenTo?"save and close":"close");
    if(ImageData && ImageSize && FloppyDisk[Id].WrittenTo)
    {
      FSEEK(fCurrentImage,0,SEEK_SET); // rewind
      FWRITE(ImageData,1,ImageSize,fCurrentImage);
    }
    fclose(fCurrentImage);
    free(ImageData);
  }
  if(TrackDataEx)
    free(TrackDataEx);
  FloppyDrive[Id].MfmManager=NULL;
  Init(); // zeroes variables
}


// density 2 for DD
bool TImageSTW2::Create(char* path,BYTE density) {
  if(Version<0x200)
    return TImageSTW::Create(path,density);
  bool ok=false;
  DWORD dwReserved=0;
  Density=density;
  FILE* fDest=fopen(path,"wb+");
  if(fDest)
  {
    // write file header
    FWRITE(DISK_EXT_STW,1,4,fDest);               //  4
    SWAP_BIG_ENDIAN_WORD(Version);                // only Version is big endian
    FWRITE(&Version,sizeof(WORD),1,fDest);        //  6
    SWAP_BIG_ENDIAN_WORD(Version);
    FWRITE(&nSides,sizeof(BYTE),1,fDest);         //  7
    FWRITE(&nTracks,sizeof(BYTE),1,fDest);        //  8
    FWRITE(&Density,sizeof(BYTE),1,fDest);        //  9
    FWRITE(&Encoding,sizeof(BYTE),1,fDest);       // 10
    BYTE source=SOURCE_STEEMSSE;
    FWRITE(&source,sizeof(WORD),1,fDest);         // 12
    FWRITE(&dwReserved,sizeof(DWORD),1,fDest);    // 16
    // create tracks
    TrackDataEx=(TStw2Data*)calloc(nTrackWords,sizeof(TStw2Data)); // create the track in memory
    for(BYTE track=0;track<nTracks;track++)
    {
      for(BYTE side=0;side<nSides;side++)
      {
        for(int byte=0;byte<nTrackWords;byte++)
        {
          TrackDataEx[byte].mfm=(WORD)rand();
          TrackDataEx[byte].fuzzy=DEFAULT_FUZZY; // simplification
          TrackDataEx[byte].timing=DEFAULT_TIMING;  // simplification
        }
        // write track to file
        FWRITE("TRK",1,3,fDest);                  //  3
        FWRITE(&side,1,1,fDest);                  //  4
        FWRITE(&track,1,1,fDest);                 //  5
        FWRITE(&nTrackWords,sizeof(WORD),1,fDest);//  7
        FWRITE(&nTrackBits,sizeof(DWORD),1,fDest);// 11
        FWRITE(&SourceImage,sizeof(BYTE),1,fDest);// 12
        FWRITE(&dwReserved,sizeof(DWORD),1,fDest);// 16
        FWRITE(TrackDataEx,sizeof(TStw2Data),nTrackWords,fDest);
        if(nTrackWords%(16/sizeof(TStw2Data)))    // padding
        {
          DWORD l=((16/sizeof(TStw2Data))-nTrackWords%(16/sizeof(TStw2Data)))*(16/sizeof(TStw2Data));
          for(DWORD i=0;i<l;i++)
            FWRITE(&dwReserved,sizeof(BYTE),1,fDest);
        }
      }//nxt side
    }//nxt track
    free(TrackDataEx);
    //TrackDataEx=NULL;
    ok=true;
    fclose(fDest);
    Init();
  }//if(fDest)
  return ok;
}


WORD TImageSTW2::GetImageWord(WORD position) {
  if(Version<0x200)
    return TImageSTW::GetImageWord(position);
  WORD mfm_data=0;
  if(TrackDataEx&&position<nTrackWords)
  {
    mfm_data=TrackDataEx[position].mfm;
    CurrentFuzzy=TrackDataEx[position].fuzzy;
#if defined(SSE_GUI_EMUCONTROL)
    if(SSEOptions.FuzzyBits)
#endif
    if(CurrentFuzzy!=DEFAULT_FUZZY)
    {
      //TRACE_LOG("%04X",mfm_data);
      BYTE odd=(LowLevel) ? (~BitPosition&1) : (BitPosition&1); // clock/data, just the way it is...
      for(int i=0;i<8;i++) // more work because the mask is 8bit
      {
        if((CurrentFuzzy&(1<<i))==0) // that bit is fuzzy
        {
          if(rand()%2)
            mfm_data&=~(1<<(i*2 +odd));
          else 
            mfm_data|=(1<<(i*2 +odd));
        }
      }
      //TRACE_LOG("->%04X ",mfm_data);
    }
    CurrentTiming=TrackDataEx[position].timing;
  }
  TRACE_MFM("[%d-%04X]",position,mfm_data);
  return mfm_data;
}


int TImageSTW::GetNextTransition(WORD& us_to_next_flux) { // notice not TImageSTW2
  DWORD bit=0;
  int cycles=0;
  us_to_next_flux=0;
  do {
    DWORD position=BitPosition/16;
    cycles+=(256/16);
    us_to_next_flux+=2;
    DWORD index=15-(BitPosition%16); // most significant bit read first
    WORD mfm_data=GetImageWord((WORD)position);
    bit=(mfm_data>>index)&1;
    IncBitPosition();
  } while(bit==0);
  return cycles;
}


void TImageSTW::IncBitPosition() { // notice not TImageSTW2
  BitPosition++;
  if(BitPosition/16>Position)
    Position++;
  ASSERTB(BitPosition/16==Position);
  if(BitPosition>=nTrackBits)
  {
    Position=0;
    BitPosition=0;
    FloppyDisk[Id].current_byte=0;
    if(!Fdc.WaitImage)
      Fdc.WaitIP=true;
    Fdc.WaitImage=false;
  }
}


void TImageSTW2::IncPosition() { // next MFM word
  if(Version<0x200)
  {
    TImageSTW::IncPosition();
    return;
  }
  ASSERTB(FloppyDisk[Id].TrackBytes);
  ASSERTB(nTrackWords);
#ifndef SSE_LEAN_AND_MEAN
  if(nTrackWords)
#endif
  Position=(Position+1)%nTrackWords; 
  BitPosition=Position*16;
  if(!Position)
  {
    TRACE_LOG2("STW2 reset disk.current_byte from %d\n",FloppyDisk[Id].current_byte);
    FloppyDisk[Id].current_byte=0;
    if(!Fdc.WaitImage)
      Fdc.WaitIP=true;
    Fdc.WaitImage=false;
  }
}


void TImageSTW2::Init() {
  TImageSTW::Init();
  TrackDataEx=pTrackDataEx=NULL;
  ImageSize=0;
  CurrentFuzzy=DEFAULT_FUZZY;
  CurrentTiming=DEFAULT_TIMING;
  Encoding=2;
  Density=2;
  SourceImage=EXT_STW;
  Version=STW2VERSION;
  Dirty=false;
  memset(TrackInfo,0,sizeof(TrackInfo));
}


bool TImageSTW2::LoadTrack(BYTE side,BYTE track,bool reload) {
  if(Version<0x200)
    return TImageSTW::LoadTrack(side,track,reload);
  bool ok=false;
  if(Dirty)
    SaveTrack(CurrentSide,CurrentTrack);
  if(side<nSides && track<nTracks && ImageData)
  {
    int position=TrackInfo[side][track].position;
    //runtime format check
    if( !strncmp("TRK",(char*)ImageData+position,3) 
      && *(ImageData+position+3)==side && *(ImageData+position+4)==track
      && *(WORD*)(ImageData+position+5)==TrackInfo[side][track].records)
    {
      FloppyDisk[Id].current_side=side;
      FloppyDisk[Id].current_track=track;
      pTrackDataEx=(TStw2Data*)(ImageData+position+TRACK_HEADER_SIZE); // pointer inside image
      nTrackWords=TrackInfo[side][track].records;
      // copy track to buffer, which we'll use for R/W
      memcpy(TrackDataEx,pTrackDataEx,nTrackWords*sizeof(TStw2Data));
      nTrackBits=*(DWORD*)(ImageData+position+7);
      SourceImage=*(ImageData+position+7+4); // it can be different for each track!
      switch(SourceImage) {
      case EXT_STX:
        LowLevel=false;
        break;
      case EXT_SCP:
        LowLevel=true;
        break;
      }
      if(nTrackWords&&nTrackBits>16)
        ok=true;
      if(ok)
      {
#if defined(SSE_ENABLE_TRACE_LOG)
        if(CurrentSide!=side||CurrentTrack!=track)
        {
          TRACE_LOG("STW2 LoadTrack %c:%d.%d bytes %d bits %d source %s\n",
            'A'+Id,side,track,nTrackWords,nTrackBits,extension_list[SourceImage]);
        }
#endif
        CurrentSide=side,CurrentTrack=track;
        FloppyDisk[Id].TrackBytes=nTrackWords;
        Dirty=false;
      }
    }
  }
#if defined(SSE_ENABLE_TRACE_LOG)
  if(!ok)
    TRACE_LOG("STW2 can't load %d.%d\n",side,track);
#endif
  return ok;
}


#if defined(SSE_ENABLE_TRACE_LOG)
// debug: dump raw track with sync, fuzzy, timing info in trace.txt, in that order of priority
// can be used by converters
// control mask browser: check data/mfm in the TRACE disk line
void TImageSTW2::LogTrack(BYTE side,BYTE track) {
  if(TrackDataEx&&LoadTrack(side,track))
  {
    TWD1772MFM wd1772mfm; // use our own
    WORD a1detect=0,nfuzzy=0,dsrcnt=16;
    if(SHOULD_DUMP_BYTES) // DMA log
    {
      for(int i=0;i<nTrackWords;i+=16)
      {
#if defined(SSE_DEBUGGER_FAKE_IO)
        if(TRACE_MASK3&TRACE_CONTROL_FDCMFM)
        {
          TRACE_LOG("\n(%04d)",i);
          for(int j=0;j<16&&(i+j<nTrackWords);j++)
          {
            TRACE_LOG("%04X",TrackDataEx[i+j].mfm); // this is the raw MFM
          }
        }
#endif
        TRACE_LOG("\n(%04d)",i);
        for(int j=0;j<16&&(i+j<nTrackWords);j++)
        {
          int k=i+j;
          char extra_info=' ';
          if(TrackDataEx[k].fuzzy!=DEFAULT_FUZZY)
          {
            nfuzzy++;
            extra_info='@';
          }
          else if(pTrackDataEx[k].timing-DEFAULT_TIMING>3)
            extra_info='-';
          else if(pTrackDataEx[k].timing-DEFAULT_TIMING<-3)
            extra_info='+';
          // simple raw MFM to data conversion - it's no emulation of WD1772 Read Track
          BYTE data=0;
          for(int l=0;l<16;l++)
          {
            int bit=(TrackDataEx[k].mfm>>(15-l))&1;
            a1detect<<=1;
            a1detect|=bit; // 0 or 1
            if(!--dsrcnt) // data byte complete
            {
              wd1772mfm.encoded=a1detect;
              wd1772mfm.Decode();
              data=wd1772mfm.data;
              dsrcnt=16;
            }
            if(a1detect==0x4489) // $A1 but not $C2 ($5224)
            {
              extra_info='*';
              dsrcnt=16;
              a1detect=0;
            }
          }
          TRACE_LOG(" %c%02X",extra_info,data);
        }
      }//nxt i
    }//if(TRACE_MASK3 & TRACE_CONTROL_FDCBYTES)
    TRACE_LOG("\n%d.%d %d MFM words, %d bits Fuzzy bytes: %d\n",side,track,nTrackWords,nTrackBits,nfuzzy);
  }//if(TrackDataEx&&LoadTrack(side,track))
}
#endif//#if defined(SSE_ENABLE_TRACE_LOG)


bool TImageSTW2::Open(char* path) {
  bool ok=TImageSTW::Open(path); // true if image is v1, if not it will fail
  if(ok)  
    return ok;
  //Init();
  fCurrentImage=fopen(path,"rb+"); // try to open existing file
  if(!fCurrentImage) // maybe it's read-only
    fCurrentImage=fopen(path,"rb");
  if(fCurrentImage) // image exists
  {
    // read header first
    BYTE header[HEADER_SIZE];
    FREAD(header,HEADER_SIZE,1,fCurrentImage);
    header[3]='\0';
    if(!strncmp(DISK_EXT_STW,(char*)header,3))
    {
      Version=*(WORD*)(header+4);
      SWAP_BIG_ENDIAN_WORD(Version);
      if(Version>=0x200 && Version <0x300) // only v2
        ok=true;
      nSides=header[6];
      nTracks=header[7];
      Density=header[8];
      Encoding=header[9];
      ImageSize=(int)GetFileLength(fCurrentImage);
      if(ok)
        ImageData=(BYTE*)malloc(ImageSize);
    }
    if(ok&&ImageData)
    {
      FSEEK(fCurrentImage,0,SEEK_SET); // rewind
      FREAD(ImageData,1,ImageSize,fCurrentImage); // copy whole file to memory
      // browse tracks and check meta-format, fill TrackInfo up
      int position=HEADER_SIZE;
      for(BYTE track=0;ok&&track<nTracks;track++)
      {
        for(BYTE side=0;ok&&side<nSides;side++)
        {
          if(strncmp("TRK",(char*)ImageData+position,3)
            ||  *(ImageData+position+3)!=side 
            ||  *(ImageData+position+4)!=track )
            ok=false;
          else
          {
            TrackInfo[side][track].position=position; // position of TRK
            TrackInfo[side][track].records=*(WORD*)(ImageData+position+5);
            position+=TRACK_HEADER_SIZE+TrackInfo[side][track].records*sizeof(TStw2Data);
            if(TrackInfo[side][track].records%(16/sizeof(TStw2Data))) // padding
              position+=(16/sizeof(TStw2Data)-TrackInfo[side][track].records%sizeof(TStw2Data))
                *(16/sizeof(TStw2Data));
          }
        }//nxt side
      }//nxt track
      const WORD NormalBytes=(DISK_BYTES_PER_TRACK/2)*Density;
      nMaxTrackWords=NormalBytes;
      WORD uttermax=NormalBytes*2;
      // reserve a big enough buffer once for all
      TrackDataEx=(TStw2Data*)calloc(uttermax,sizeof(TStw2Data));
      for(int byte=0;byte<uttermax;byte++)
      {
        TrackDataEx[byte].mfm=(WORD)rand();
        TrackDataEx[byte].fuzzy=DEFAULT_FUZZY; // simplification
        TrackDataEx[byte].timing=DEFAULT_TIMING;  // simplification
      }
      TRACE_LOG("Open STW %s, V%X S%d T%d OK%d\n",CHECKPATH(path),Version,nSides,nTracks,ok); 
    }
  }//if(fCurrentImage)
  if(!ok)
    Close();
  else 
    FloppyDrive[Id].MfmManager=this;
  return ok;
}


// save track buffer to that track, changing the image size if necessary
bool TImageSTW2::SaveTrack(BYTE side,BYTE track) {
  bool ok=false;
  if(side<nSides && track<nTracks && ImageData)
  {
    if(TrackInfo[side][track].records!=nTrackWords) // image changes size
    { 
      DWORD NewImageSize=ImageSize+(nTrackWords-TrackInfo[side][track].records)*sizeof(TStw2Data);
      if(NewImageSize%16)
        NewImageSize=(NewImageSize/16+1)*16; // padding
      TRACE_LOG("%d.%d records %d->%d resize %d->%d\n",side,track,TrackInfo[side][track].records,nTrackWords,ImageSize,NewImageSize);
      TrackInfo[side][track].records=nTrackWords;
      BYTE *NewImageData=(BYTE*)malloc(NewImageSize); // must create new image, can't realloc
      memcpy(NewImageData,ImageData,HEADER_SIZE); // could copy in 3 steps? TODO
      DWORD position=HEADER_SIZE;
      for(BYTE t=0;t<nTracks;t++)
      {
        for(BYTE s=0;s<nSides;s++)
        {
          // copy old track header&data
          memcpy(NewImageData+position,ImageData+TrackInfo[s][t].position,
            TRACK_HEADER_SIZE+TrackInfo[s][t].records*sizeof(TStw2Data));
          TrackInfo[s][t].position=position; // new position of TRK
          if(s==side&&t==track)
          {
            *(WORD*)(NewImageData+position+5)=nTrackWords;
            *(DWORD*)(NewImageData+position+7)=nTrackBits;
            *(NewImageData+position+7+4)=SourceImage;
          }
          position+=TRACK_HEADER_SIZE+TrackInfo[s][t].records*sizeof(TStw2Data);
          if(TrackInfo[s][t].records%(16/sizeof(TStw2Data))) // padding
          {
            int nzeroes=(16/sizeof(TStw2Data)-TrackInfo[s][t].records%sizeof(TStw2Data))
              *(16/sizeof(TStw2Data));
            memset(NewImageData+position,0,nzeroes);
            position+=nzeroes; // should write 0
          }
        }//nxt s
      }//nxt t
      ASSERTB(position==NewImageSize);
      free(ImageData);
      ImageSize=NewImageSize;
      ImageData=NewImageData;
    }//if
    BYTE *p=(ImageData+TrackInfo[side][track].position+TRACK_HEADER_SIZE);
    memcpy(p,TrackDataEx,nTrackWords*sizeof(TStw2Data));
    ok=true;
    Dirty=false;
  }//if
  return ok;
}


void TImageSTW2::SetImageWord(WORD position,WORD mfm_data) {
  if(Version<0x200)
    TImageSTW::SetImageWord(position,mfm_data);
  else if(TrackDataEx && position<nMaxTrackWords)
  {
    if(nTrackWords<=position)
      nTrackWords=FloppyDisk[Id].TrackBytes=position+1;
    TrackDataEx[position].mfm=mfm_data;
    TrackDataEx[position].fuzzy=CurrentFuzzy;
    TrackDataEx[position].timing=CurrentTiming;
    if(!FloppyDisk[Id].ReadOnly)
      Dirty=true;
  }
}

#endif//#if defined(SSE_DISK_STW2)


/////////////////////////////////////////////
// Conversion of STX and SCP images to STW //
/////////////////////////////////////////////

#include <translate.h>
#include <notifyinit.h>


/////////
// STX //
/////////

#if defined(SSE_DISK_STX2STW)

#ifndef SSE_LEAN_AND_MEAN
#define HANDLE_PASTI_REV2 // not necessary to handle existing images
#endif

#include <disk_stx.h>


void inc_roll(WORD &counter,WORD limit) {
  counter++;
  if(counter>=limit)
    counter=0;
}


void dec_roll(WORD &counter,WORD limit) {
  if(counter<=0)
    counter=limit-1;
  counter--;
}


void wd1772_write_mft(TImageSTW2 *myImageSTW2,BYTE data,TWD1772MFM* wd1772mfm,
                      TWD1772Crc* wd1772crc,int write_mode,BYTE *databyte) {
  if(myImageSTW2->Position>=myImageSTW2->nTrackWords)
    return;
  if(!write_mode)
    wd1772crc->Add(data);
  wd1772mfm->data=data;
  if(databyte)
    databyte[myImageSTW2->Position]=data;
  wd1772mfm->Encode(write_mode);
  if(write_mode==1)
    wd1772crc->Reset();
  myImageSTW2->SetMfmData(0xFFFF,wd1772mfm->encoded);
}


#define WD1772_WRITE(d) wd1772_write_mft(&StwMngr,d,&wd1772mfm,&wd1772crc,0,databyte)
#define WD1772_WRITE_A1 wd1772_write_mft(&StwMngr,0xA1,&wd1772mfm,&wd1772crc,1,databyte)
#define WD1772_WRITE_CRC(d) wd1772_write_mft(&StwMngr,d,&wd1772mfm,&wd1772crc,2,databyte)



// offset points to a FE, we check if following bytes match the id

bool check_id(WORD o,WORD nTrackWords,BYTE *pTrackBytes,TWD1772IDField& id) {
  bool ok=false;
  inc_roll(o,nTrackWords);
  if(pTrackBytes[o]==id.track)
  {
    inc_roll(o,nTrackWords);
    if(pTrackBytes[o]==id.side)
    {
      inc_roll(o,nTrackWords);
      if(pTrackBytes[o]==id.num)
        ok=true;
    }
  }
  return ok;
}


/*  Convert an STX image to STW v2 (MFM)
*   It's not trivial but it's fun and hacky
*   This is based on the unofficial Pasti file format documentation by DrCoolZic
*   and especially sarnau
*   Can't convert:
*   Audio Sculpture, Skull & Crossbones, etc.
*   Don't think it's possible to create a generic converter that handles all images
*   This was the first attempt at natively supporting Pasti images (see SSE_DISK_STX)
*/
bool STXtoSTW(char* src_path,char* dst_path) {
  bool ok=true;
  //logfile_wipe();
#ifdef WIN32
  TNotify myNotify(T("Disk operation")); // takes some time if logging
#endif
  TWD1772MFM wd1772mfm; // etc... the power of object-oriented programming
  TWD1772Crc wd1772crc;
  TImageSTW2 StwMngr;
  const BYTE Id=2; // no interference with A: or B:
  TImageSTX &StxMngr=ImageSTX[Id]; // if the image is zipped, it's already set as ID2
  StxMngr.Id=StwMngr.Id=Id;
  TFloppyDisk &disk=FloppyDisk[Id]; // shorthand  
  StwMngr.LowLevel=false; // better be false for running the image as well!
  // open STX image if necessary, create STW file
  if(!StxMngr.pStxImage) // already open...
    ok=StxMngr.Open(src_path); // load pasti image
  if(ok)
    ok=StwMngr.Create(dst_path,2);
  if(ok&&disk.TracksPerSide>StwMngr.nTracks)
    ok=false;
  else
    ok=StwMngr.Open(dst_path);
  const WORD NormalBytes=(DISK_BYTES_PER_TRACK/2)*StwMngr.Density;
  StwMngr.SourceImage=EXT_STX; // for stx tracks
  for(BYTE track=0;ok&&track<StwMngr.nTracks;track++)
  {
    for(BYTE side=0;ok&&side<StwMngr.nSides;side++)
    {
      StwMngr.Dirty=false;
      ok=StwMngr.LoadTrack(side,track);
      bool bTrackRecord=false,bTrackImage=false;
      bTrackRecord=StxMngr.LoadTrack(side,track);
      if(bTrackRecord)
        bTrackImage=StxMngr.pTrackBytes!=NULL;
      WORD &nTrackWords=StwMngr.nTrackWords=StwMngr.nMaxTrackWords=disk.TrackBytes; // shorthand
      StwMngr.nTrackBits=StwMngr.nTrackWords*16;
      TRACE_LOG("%d.%d % bytes %d bits %d Image %d sector bytes %d\n",
        side,track,nTrackWords,StwMngr.nTrackBits,bTrackImage,DiskEmu.bytes);
      BYTE *databyte=(BYTE*)malloc(nTrackWords);
      BYTE *sync=(BYTE*)malloc(nTrackWords);
      memset(sync,0,nTrackWords);
      StwMngr.CurrentFuzzy=StwMngr.DEFAULT_FUZZY;
      StwMngr.CurrentTiming=StwMngr.DEFAULT_TIMING;
      if(bTrackRecord)
      {
        if(!StxMngr.FirstSync)
          StxMngr.FirstSync=0xFFFF;
        int niam=0,ndam=0; // debug info
        StwMngr.Position=0;
        BYTE *p=StxMngr.pTrackBytes;
        if(bTrackImage)
        {
          // check track timing (Arkanoid 0.79)
          int ratio=(StwMngr.nTrackWords*1000)/NormalBytes;
          if(ratio>=1002 || ratio<=998)
          {
            int current_timing=(StwMngr.DEFAULT_TIMING*1000)/ratio;
            ASSERTB((current_timing&0xFFFFFF00)==0);
            StwMngr.CurrentTiming=current_timing&0xFF;
            TRACE_LOG("timing $%X ",StwMngr.CurrentTiming);
          }
          // encode track data ("first pass") and look for address marks
          for(WORD i=0;i<nTrackWords;i++)
          {
            WORD j=i;
            if((*p&0xFF)>=0xFC) // IAM
            {
              dec_roll(j,nTrackWords);
              if(StxMngr.pTrackBytes[j]==0xA1)
              {
                dec_roll(j,nTrackWords);
                if(StxMngr.pTrackBytes[j]==0xA1)
                {
                  sync[i]='i';
                  niam++;
                  dec_roll(j,nTrackWords);
                  for(int k=0;k<3;k++)
                  {
                    sync[j]=0xA1;
                    inc_roll(j,nTrackWords);
                  }
                }
              }
            }
            else if((*p&0xFE)==0xF8||(*p&0xFE)==0xFA) // DAM
            { 
              dec_roll(j,nTrackWords);
              if(StxMngr.pTrackBytes[j]==0xA1)
              {
                dec_roll(j,nTrackWords);
                if(StxMngr.pTrackBytes[j]==0xA1)
                {
                  sync[i]='d';
                  ndam++;
                  dec_roll(j,nTrackWords); // 1st A1
                  for(int k=0;k<3;k++)
                  {
                    sync[j]=0xA1;
                    inc_roll(j,nTrackWords);
                  }
                }
              }
            }
            else if(disk.SectorsPerTrack==0&&(*p==0x14||*p==0xC2)) // Golden Axe 0.2
            { 
              inc_roll(j,nTrackWords);
              WORD a1_2=j;
              if(StxMngr.pTrackBytes[j]==0xA1 && !sync[j])
              {
                inc_roll(j,nTrackWords);
                if(StxMngr.pTrackBytes[j]==0xA1 && !sync[j])
                  sync[i]=sync[a1_2]=sync[j]=0xA1;
              }
            }
            wd1772mfm.data=databyte[i]=*p++;
            if(i==StxMngr.FirstSync||sync[i]==0xA1)
            {
              wd1772mfm.data=databyte[i]=sync[i];
              wd1772mfm.Encode(TWD1772MFM::FORMAT_CLOCK);
              sync[i]='*';
            }
            else
              wd1772mfm.Encode(TWD1772MFM::NORMAL_CLOCK);
            StwMngr.SetMfmData(0xFFFF,wd1772mfm.encoded);
          }//nxt i
          ASSERTB(StxMngr.pTrackBytes+nTrackWords==p);
          TRACE_LOG("track data %d bytes, %d IAM, %d DAM\n",nTrackWords,niam,ndam);
        }//if(bTrackImage)
        BYTE TrackTiming=StwMngr.CurrentTiming;
        BYTE TrackFuzzy=StwMngr.DEFAULT_FUZZY;
        if(disk.SectorsPerTrack&&!bTrackImage&&!StxMngr.pSectorDesc&&DiskEmu.bytes<=SECTOR_SIZE*10)
        {
          // no info but data, write start of track
          for(int i=0;i<FloppyDisk[Id].PostIndexGap();i++)
            WD1772_WRITE(0x4E);
        }
#ifdef HANDLE_PASTI_REV2 // not necessary
        TStxFileDesc *pStxFileDesc=(TStxFileDesc*)StxMngr.pStxImage;
        // check sector timing
        WORD *pTimingTable=NULL;
        int nTimingBytes=0;
        if(StxMngr.pSectorDesc)
        {
          for(WORD sr=0;sr<disk.SectorsPerTrack;sr++)
          {
            // compute expected length of timing table
            if((StxMngr.pSectorDesc[sr].fdcFlags&NStx::SECT_FLAG_MACRODOS)&&(pStxFileDesc->revision>=2))
              nTimingBytes+=4+(16<<(StxMngr.pSectorDesc->id.len&3)); // header + record (64 for 512)
          }
          if(nTimingBytes) // we start from the end of the track record
          {
            pTimingTable=(WORD*)((BYTE*)StxMngr.pTrackDesc+StxMngr.pTrackDesc->recordSize-nTimingBytes);
            p=(BYTE*)pTimingTable;
            ASSERTB(*(WORD*)p==5); // if not, tough, generic macrodos should work anyway
          }
        }
#endif
        WORD LastDataCrc=0;
        // encode sectors
        for(BYTE sr=0;sr<disk.SectorsPerTrack;sr++)
        {
          StwMngr.CurrentTiming=TrackTiming;
          StxMngr.GetSector(side,track,sr);
          TWD1772IDField &id=StxMngr.id;
#if defined(SSE_ENABLE_TRACE_LOG)          
          id.Trace(LOGSECTION_IMAGE_INFO);
#endif
          int SectorBytes=id.nBytes();
          BYTE *SectorTiming=(BYTE*)malloc(SectorBytes);
          memset(SectorTiming,StwMngr.CurrentTiming,SectorBytes);
          BYTE *SectorFuzzy=(BYTE*)malloc(SectorBytes);
          memset(SectorFuzzy,StwMngr.DEFAULT_FUZZY,SectorBytes);
          // check sector timing
          if(StxMngr.pSectorDesc)
          {
            if(StxMngr.bMacrodos)
            {
#ifdef HANDLE_PASTI_REV2
              bool bDefaultMacrodos=true;
              if(pStxFileDesc->revision>=2 && pTimingTable)
              { // golden axe 0.01.0
                // use timing table... it's messy, so anything wrong and we default to Macrodos
                if(*pTimingTable==0x005)
                {
                  bDefaultMacrodos=false;
                  WORD size=*(++pTimingTable);
                  WORD words=8<<(StxMngr.pSectorDesc->id.len&3);
                  if(size==4+words*2)
                  {
                    for(int i=0;!bDefaultMacrodos&&i<words;i++)
                    {
                      WORD timing=*(++pTimingTable);
                      SWAP_BIG_ENDIAN_WORD(timing);
                      if((timing&0xFF00)==0)
                        for(int j=0;j<16;j++)
                          SectorTiming[i*16+j]=timing&0xFF;
                      else
                        bDefaultMacrodos=true;
                    }
                  }
                  TRACE_LOG(" %d timings",words);
                }
              }
              if(bDefaultMacrodos)
#endif
              { // Colorado 0.1.1
                // build speed table
                for(int i=0;i<SectorBytes;i++)
                {
                  if(i>SectorBytes/4&&i<SectorBytes/2)
                    SectorTiming[i]=133;
                  else if(i>SectorBytes/2&&i<(3*SectorBytes)/4)
                    SectorTiming[i]=121;
                  else
                    SectorTiming[i]=StwMngr.DEFAULT_TIMING;
                }
                TRACE_LOG(" Macrodos");
              }
            }
            else if(StxMngr.pSectorDesc[sr].readTime && SectorBytes)
            { // Populous 0.0.6
              // there can be a value even if not much different from reference 16384us=512*32us
              int ratio=(StxMngr.pSectorDesc[sr].readTime*1000)/(32*SectorBytes);
              if(ratio>=1010&&ratio<1200 || ratio<=990&&ratio>750) //?
              {
                int current_timing=(StwMngr.DEFAULT_TIMING*ratio)/1000;
                ASSERTB((current_timing&0xFFFFFF00)==0);
                TRACE_LOG(" %c$%02X\n",((current_timing>StwMngr.DEFAULT_TIMING)?'-':'+'),current_timing);
                // build speed table
                for(int i=0;i<SectorBytes;i++)
                  SectorTiming[i]=(BYTE)current_timing;
              }
            }
            // sector fuzzy bits
            if(StxMngr.bFuzzySector && StxMngr.pFuzzyTable)
            {
              memcpy(SectorFuzzy,StxMngr.pFuzzyTable,SectorBytes);
              StxMngr.pFuzzyTable+=SectorBytes;
              TRACE_LOG(" @");
            }
          }//if(pSectorDesc)
          WORD offset=0;
          // locate IAM
          BYTE iam=0xFE;
          bool bFoundInImage=false;
          if(bTrackImage)
          {
            // look for ID up and down in track data
            WORD BytePosition=StxMngr.pSectorDesc[sr].bitPosition/8; // bitPosition is actually approximate too
            WORD before=BytePosition,after=BytePosition;
            offset=0;
            for(WORD j=0;j<nTrackWords&&!bFoundInImage;j++)
            {
              if(sync[before]=='i')
              {
                bFoundInImage=true;
                offset=before;
              }
              else if(sync[after]=='i')
              {
                bFoundInImage=true;
                offset=after;
              }
              dec_roll(before,nTrackWords);
              inc_roll(after,nTrackWords);
            }
            // if id doesn't match, look for another one that would just in case
            if(bFoundInImage && !check_id(offset,nTrackWords,StxMngr.pTrackBytes,id))
            {
              before=after=offset;
              for(int i=0;i<32;i++)
              {
                dec_roll(before,nTrackWords);
                inc_roll(after,nTrackWords);
                if(sync[before]=='i' && check_id(before,nTrackWords,StxMngr.pTrackBytes,id))
                {
                  offset=before;
                  break;
                }
                else if(sync[after]=='i'&& check_id(after,nTrackWords,StxMngr.pTrackBytes,id))
                {
                  offset=after;
                  break;
                }
              }//nxt i
            }//if
            iam=databyte[offset];
            //TRACE_LOG(" img");// VS2008 WTF: fatal error C1001
            if(check_id(offset,nTrackWords,StxMngr.pTrackBytes,id))
            {
              //TRACE_LOG(" ok");
              TRACE_LOG(" img ok");
            }
            for(int j=0;j<3;j++)
              dec_roll(offset,nTrackWords); // first A1
            StwMngr.Position=offset;
          }//if(bTrackImage)
          if(!bFoundInImage)
          {
            if(StxMngr.pSectorDesc)
            {
              offset=StxMngr.pSectorDesc[sr].bitPosition/8;
              while(offset<LastDataCrc) // don't erase previous data
                inc_roll(offset,nTrackWords);
              if(sr) // Turrican 0.7.0 contains 0.7.16 & 0.7.1
              {
                if((StxMngr.pSectorDesc[sr].fdcFlags&FDC_STR_CRC)
                  && (StxMngr.pSectorDesc[sr-1].fdcFlags&FDC_STR_CRC)
                  && (StxMngr.pSectorDesc[sr].bitPosition-StxMngr.pSectorDesc[sr-1].bitPosition<96*8))
                {
                  int hack_add=112; // too hacky for release (but the image loads), that's where we gave up!
                  TRACE_LOG(" +%d",hack_add);
                  for(int i=0;i<hack_add;i++)
                    inc_roll(offset,nTrackWords);
                }
              }
              for(int i=0;i<(3+12)&&offset>LastDataCrc;i++) // go to start of 0 sequence
                dec_roll(offset,nTrackWords);
            }//if(pSectorDesc)
            else // we know there's a sector because nSecs>0
            {
              offset=StwMngr.Position; // we should be post gap
              if(DiskEmu.bytes<=SECTOR_SIZE*10)
                for(int i=0;i<15;i++) //?
                  inc_roll(offset,nTrackWords);
            }
            ASSERTB((offset&0xFFFF0000)==0);
            StwMngr.Position=offset;
            for(int i=0;i<12;i++)
              WD1772_WRITE(0x00);
          }
          // write address mark
          offset=StwMngr.Position;
          for(int i=0;i<3;i++)
          {
            WD1772_WRITE_A1;
            sync[offset]='*';
            inc_roll(offset,nTrackWords);
          }
          // write sector header
          WORD IamPos=offset;
          WD1772_WRITE(iam);
          sync[offset]='I'; // not 'i', so it won't be attributed again (Microprose Golf 1.36)
          TRACE_LOG(" IAM %04d",offset);
          WD1772_WRITE(id.track);
          WD1772_WRITE(id.side);
          WD1772_WRITE(id.num);
          WD1772_WRITE(id.len);
          if((StxMngr.SectorFlags&(FDC_STR_CRC|FDC_STR_RNF))!=(FDC_STR_CRC|FDC_STR_RNF))
          {
            WD1772_WRITE_CRC(wd1772crc.crc>>8);
            WD1772_WRITE_CRC(wd1772crc.crc&0xFF);
          }
          else
          {
            WD1772_WRITE(id.CRC[0]);
            WD1772_WRITE(id.CRC[1]);
          }
          // locate DAM
          BYTE dam=0xFB,altdam=0xFA;
          if(StxMngr.SectorFlags&FDC_STR_RNF)
            continue;
          if(StxMngr.SectorFlags&FDC_STR_RT)
            dam=0xF8,altdam=0xF9; // deleted
          bFoundInImage=false;
          if(bTrackImage)
          {
            WORD t=IamPos;
            for(int i=0;i<(7+24);i++)  // ID 7 + 24 + 12
              inc_roll(t,nTrackWords);
            offset=0;
            for(WORD j=0;j<nTrackWords&&!bFoundInImage;j++)
            {
              if(sync[t]=='d')
              {
                bFoundInImage=true;
                offset=t;
                dam=databyte[offset];
              }
              inc_roll(t,nTrackWords);
            }
            if(bFoundInImage)
            {
              TRACE_LOG(" img");
              for(int j=0;j<3;j++)
                dec_roll(offset,nTrackWords); // first A1
              StwMngr.Position=offset;
            }
          }//if(bTrackImage)
          if(!bFoundInImage)
          {
            // write format bytes before data
            for(int i=0;i<22;i++)
              WD1772_WRITE(0x4e);
            for(int i=0;i<12;i++)
              WD1772_WRITE(0x00);
          }
          // write address mark
          offset=StwMngr.Position;
          for(int i=0;i<3;i++)
          {
            WD1772_WRITE_A1;
            sync[offset]='*';
            inc_roll(offset,nTrackWords);
          }
          TRACE_LOG(" DAM %04d",offset);
          // write DAM
          WD1772_WRITE(dam);
          sync[offset]='D';
          // write sector data
          BYTE* pSectorData=StxMngr.pStxTrackData;
          if(StxMngr.pSectorDesc)
            pSectorData+=StxMngr.pSectorDesc[sr].dataOffset;
          else
            pSectorData+=sr*SectorBytes; // we know there's a sector because sectorCount>0
          // write data 
          bool bHitSync=false;
          for(int i=0;i<SectorBytes&&!bHitSync;i++)
          {
            BYTE d;
            d=pSectorData[i];
            StwMngr.CurrentTiming=SectorTiming[i]; // those values
            StwMngr.CurrentFuzzy=SectorFuzzy[i]; // will be written on the image
            WD1772_WRITE(d);
            switch(sync[StwMngr.Position]) {
            case '*': case 'I': case 'D':
            case 'i': // Ange de Cristal 0.78... but probably bad for other cases
              bHitSync=true;
              break;
            }
          }
          if(bHitSync)
          {
            TRACE_LOG(" sync %c at %d",sync[StwMngr.Position],StwMngr.Position);
            continue;
          }
          StwMngr.CurrentTiming=TrackTiming;
          StwMngr.CurrentFuzzy=TrackFuzzy;
          // CRC
          TRACE_LOG(" CRC at %d\n",offset=StwMngr.Position);
          if(StxMngr.SectorFlags&FDC_STR_CRC)
          {
            if(!bTrackImage)
            {
              WD1772_WRITE(0xDE); // bogus CCR
              WD1772_WRITE(0xAD);
            }
          }
          else
          {
            WD1772_WRITE_CRC(wd1772crc.crc>>8);
            WD1772_WRITE_CRC(wd1772crc.crc&0xFF);
            LastDataCrc=StwMngr.Position;
          }
          if(!bTrackImage&&!StxMngr.pSectorDesc&&DiskEmu.bytes<=SECTOR_SIZE*10) // only in this case
          {
            // write format bytes after sector
            for(int i=0;i<((disk.SectorsPerTrack%11==0)?1-1:40-1);i++)
              WD1772_WRITE(0x4E);
          }
          free(SectorTiming);
          free(SectorFuzzy);
        }//nxt sr
        StwMngr.SaveTrack(side,track);
#if defined(SSE_ENABLE_TRACE_LOG)
        if(TRACE_LOG_ENABLED(LOGSECTION_IMAGE_INFO))
          StwMngr.LogTrack(side,track);
#endif
      }//if(bTrackRecord)
      free(sync);
      free(databyte);
    }//nxt side
  }//nxt track
  if(disk.fp)
  {
    fclose(disk.fp);
    disk.fp=NULL;
  }
  disk.WrittenTo=true; // necessary
  TRACE_LOG("STXtoSTW(%s,%s) ok%d\n",CHECKPATH(src_path),CHECKPATH(dst_path),ok);
  FLUSH_TRACE;
  return ok;
}

#undef HANDLE_PASTI_REV2
#undef WD1772_WRITE
#undef WD1772_WRITE_A1
#undef WD1772_WRITE_CRC

#endif//#if defined(SSE_DISK_STX2STW)


/////////
// SCP //
/////////

#if defined(SSE_DISK_SCP2STW)

/*  Convert an SCP image to STW v2
*   It's not foolproof (fuzzy bits?) but there are no known failures for the
*   moment (haven't tried a lot).
*/
bool SCPtoSTW(char* src_path,char* dst_path) {
  bool ok=false;
#ifdef WIN32
  TNotify myNotify(T("Disk operation")); // takes some time
#endif
  TWD1772Dpll wd1772dpll; // we only use the DPLL, not the data separator (no need to examine syncs)
  TImageSTW2 StwMngr; // note we count on destructors to closes the images
  TImageSCP ScpMngr;
  BYTE Id=2; // no interference with A: or B:
  StwMngr.Id=ScpMngr.Id=Id;  
  TFloppyDisk &disk=FloppyDisk[Id]; // shorthand  
  StwMngr.LowLevel=false; // we write word by word
  const WORD NormalBytes=(DISK_BYTES_PER_TRACK/2)*StwMngr.Density;
  const int utter_max=NormalBytes*2;
  WORD &nTrackWords=StwMngr.nTrackWords; // shorthand
  // open SCP image, create STW file
  ok=ScpMngr.Open(src_path);
  if(ok)
    ok=StwMngr.Create(dst_path,2);
  if(ok)
    ok=StwMngr.Open(dst_path);
  if(ok)
    ok=(disk.TracksPerSide<=StwMngr.nTracks); // as set by Open()
  if(ok)
  {
    ScpMngr.Wobble=0; // after opening
    FloppyDrive[Id].MfmManager=&ScpMngr;
    TStw2Data* track_data[2]={(TStw2Data*)calloc(utter_max,sizeof(TStw2Data)),
                              (TStw2Data*)calloc(utter_max,sizeof(TStw2Data))};
    WORD* fuzzy[2]={(WORD*)calloc(utter_max,sizeof(WORD)),(WORD*)calloc(utter_max,sizeof(WORD))};
    for(BYTE track=0;ok&&track<StwMngr.nTracks;track++)
    {
      FloppyDisk[Id].current_track=track;
      for(BYTE side=0;ok&&side<StwMngr.nSides;side++)
      {
        StwMngr.Dirty=false;
        ok=StwMngr.LoadTrack(side,track);
        FloppyDisk[Id].current_side=side;
        ScpMngr.rev=ScpMngr.file_header.IFF_NUMREVS-1; // trick to avoid general bit shifting
        if(ScpMngr.LoadTrack(side,track,true))
        {
          StwMngr.SourceImage=EXT_SCP;
          memset(fuzzy[0],0xFF,utter_max*sizeof(WORD));
          memset(fuzzy[1],0xFF,utter_max*sizeof(WORD));
          nTrackWords=StwMngr.Position=ScpMngr.Position=0;
          StwMngr.BitPosition=ScpMngr.BitPosition=0;
          StwMngr.CurrentFuzzy=StwMngr.DEFAULT_FUZZY;
          int mfm_bit_index=15,fdc_cycles=0,timing_ctr=0;
          DWORD TrackBits[2];
          WORD TrackWords[2];
          WORD mfm_word=0;
          bool bStop=false,bRev2=false;
          wd1772dpll.Reset(0);
          do {
            BYTE last_rev=ScpMngr.rev;
            // we must use the WD1772 DPLL to convert the image: this is not a universal preservation format
            int a1=(int)wd1772dpll.ctime;
            COUNTER_VAR tm=0;
            int bit=wd1772dpll.GetNextBit(tm,Id); // will trigger load track at end of rev
            int a2=(int)wd1772dpll.ctime;
            fdc_cycles+=a2-a1;
            if(StwMngr.Position>=utter_max)
              bStop=true; // no crash
            else if(ScpMngr.BitPosition==0 && (last_rev!=ScpMngr.rev||ScpMngr.file_header.IFF_NUMREVS==1))
            {
              TrackBits[last_rev]=StwMngr.BitPosition;
              TrackWords[last_rev]=StwMngr.Position+1;
              track_data[last_rev][StwMngr.Position].mfm=mfm_word; // last word will rarely be full
              track_data[last_rev][StwMngr.Position].fuzzy=StwMngr.CurrentFuzzy;
              track_data[last_rev][StwMngr.Position].timing=StwMngr.DEFAULT_TIMING;
              if(ScpMngr.file_header.IFF_NUMREVS>1 && ScpMngr.rev==1)
              {
                timing_ctr=StwMngr.BitPosition=0;
                StwMngr.Position=mfm_word=0;
                bRev2=true;
                mfm_bit_index=15;
              }
              else
                bStop=true;
            }
            if(!bStop)
            {
              if(++timing_ctr==16)
              { // record timing every 16 bits of raw mfm
                track_data[ScpMngr.rev][StwMngr.Position].timing=(BYTE)(fdc_cycles/2);
                timing_ctr=fdc_cycles=0;
              }
              // mark fuzzy bit, it's simply based on the DPLL internal state - could be trouble
              if( wd1772dpll.phase_add==0x0F && wd1772dpll.freq_add>3
                ||wd1772dpll.phase_sub==0x0F && wd1772dpll.freq_sub>3)
              {
                fuzzy[ScpMngr.rev][StwMngr.Position]&=~(1<<mfm_bit_index);
                StwMngr.CurrentFuzzy&=~(1<<((mfm_bit_index+1)/2));
              }
              StwMngr.BitPosition++;
              if(bit)
                mfm_word|=(1<<mfm_bit_index); // set bit, highest first
              if(--mfm_bit_index<0)
              {
                track_data[ScpMngr.rev][StwMngr.Position].mfm=mfm_word;
                track_data[ScpMngr.rev][StwMngr.Position].fuzzy=StwMngr.CurrentFuzzy;
                StwMngr.CurrentFuzzy=StwMngr.DEFAULT_FUZZY;
                StwMngr.Position++;
                mfm_word=0;
                mfm_bit_index=15;
              }//if
            }//if(!bStop)
          } while(!bStop);
          // try to make one perfectly looping track if we have two revs
          if(bRev2&&TrackWords[0]>1024&&TrackWords[1]>1024)
          {
            // look for equal bits at end of revs
            const WORD BufferSize=64;
            WORD mfm1[BufferSize];
            WORD pos1=TrackWords[1]-BufferSize*2;
            DWORD bpos1=pos1*16;
            for(int i=0;i<BufferSize;i++)
            {
              int ix=pos1+i;
              mfm1[i]=track_data[1][ix].mfm&fuzzy[1][ix];
            }
            DWORD bpos0=TrackBits[0]-BufferSize*2+32;
            bool bDiff;
            do { // compare buffer with rev0 data (Slow)
              bDiff=false;
              int div=(--bpos0)/16,rest=bpos0%16,complement=16-rest;
              for(int i=0;i<BufferSize&&!bDiff;i++)
              {
                WORD a=track_data[0][div+i].mfm&fuzzy[0][div+i];
                WORD b=track_data[0][div+i+1].mfm&fuzzy[0][div+i+1];
                WORD c=(a<<rest)|(b>>complement);
                if(c!=mfm1[i])
                  bDiff=true;
              }
            } while(bpos0>0 && bDiff);
            if(bpos0>0) // found match
            {
              StwMngr.nTrackBits=bpos1+TrackBits[0]-bpos0;
              // beginning of track is beginning of rev1
              for(int i=0;i<pos1;i++)
                StwMngr.TrackDataEx[i]=track_data[1][i];
              // end of track is end of rev0, we must shift all data and fuzzy info
              for(DWORD i=bpos0,j=0;i<TrackBits[0];i+=16,j+=16)
              {
                int div=i/16,div1=div+1,rest=i%16,complement=16-rest;
                WORD a=track_data[0][div].mfm;
                WORD b=track_data[0][div1].mfm;
                StwMngr.TrackDataEx[pos1].mfm=(a<<rest)|(b>>complement);
                WORD a_f=fuzzy[0][div];
                WORD b_f=fuzzy[0][div1];
                WORD w_fuzzy=(a_f<<=rest)|(b_f>>complement);
                BYTE b_fuzzy=StwMngr.DEFAULT_FUZZY;
                if(w_fuzzy!=(StwMngr.DEFAULT_FUZZY|(StwMngr.DEFAULT_FUZZY<<8))) // this is $FFFF
                  for(int k=0;k<16;k++)
                    if((w_fuzzy&(1<<k))==0)
                      b_fuzzy&=~(1<<k/2);
                StwMngr.TrackDataEx[pos1].fuzzy=b_fuzzy;
                int timing=(track_data[0][div].timing*rest
                  +track_data[0][div1].timing*complement)/16;
                StwMngr.TrackDataEx[pos1].timing=(BYTE)timing; // averaged!
                pos1++;
              }//nxt i
            }
            else // no match
              bRev2=false;
          }
          if(!bRev2)
            StwMngr.nTrackBits=TrackBits[0];
          ASSERTB(StwMngr.nTrackBits);
          StwMngr.nTrackWords=(WORD)(StwMngr.nTrackBits/16);
          if(StwMngr.nTrackBits%16)
            StwMngr.nTrackWords++; // as # records or words
          if(!bRev2) //copy rev0
            memcpy(StwMngr.TrackDataEx,track_data[0],sizeof(TStw2Data)*StwMngr.nTrackWords);
          StwMngr.SaveTrack(side,track);
#if defined(SSE_ENABLE_TRACE_LOG)
          if(TRACE_LOG_ENABLED(LOGSECTION_IMAGE_INFO))
            StwMngr.LogTrack(side,track);
#endif
        }
        TRACE_LOG("%d.%d %d words, %d bits (%d in)\n",side,track,nTrackWords,StwMngr.nTrackBits,StwMngr.nTrackBits%16);
      }//nxt side
    }//nxt track
    free(track_data[0]);
    free(track_data[1]);
    free(fuzzy[0]);
    free(fuzzy[1]);
  }
  disk.WrittenTo=true;
  TRACE_LOG("SCPtoSTW(%s,%s) ok%d\n",CHECKPATH(src_path),CHECKPATH(dst_path),ok);
  return ok;
}

#endif//#if defined(SSE_DISK_SCP2STW)

#undef LOGSECTION
#undef HEADER_SIZE
#undef TRACK_HEADER_SIZE
#undef SOURCE_STEEMSSE
#undef STWVERSION
