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
FILE: disk_ghost.cpp
CONDITION: SSE_DISK_GHOST must be defined
DESCRIPTION: Ghost files (extension STG) record changed sectors of a
read-only disk image, like a Pasti image, so that you can save your game
or the high scores.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop


#ifdef SSE_DISK_GHOST

#include <disk_ghost.h>
#include <debug.h>
#include <mymisc.h>

#define HEADER_SIZE (4+2+2)       // "STG"VVNN
#define NRECORDS_POSITION (4+2)
#define RECORD_HEADER_SIZE 5 // OK for future (?) TRK## 
#define SECTOR_HEADER "SEC"  // SEC## where ## is record number

#define LOGSECTION LOGSECTION_IMAGE_INFO

void TGhostDisk::Close() {
  if(fCurrentImage)
  {
    TRACE_LOG("STG close image with %d records\n",nRecords);
    FSEEK(fCurrentImage,NRECORDS_POSITION,SEEK_SET);
    FWRITE(&nRecords,sizeof(WORD),1,fCurrentImage); 
    fclose(fCurrentImage);
    free(SectorData);
    Init();
  }
}


bool TGhostDisk::FindIDField(TWD1772IDField *IDField) {
  bool found=false;
  if(fCurrentImage)
  {
    // set on 1st header, skipping header
    FSEEK(fCurrentImage,HEADER_SIZE,SEEK_SET);
    // this loop is inefficient, we expect few records
    for(int record=0;record<nRecords&&!found;record++)
    {
      // to test header, we recycle IDField (optimisation)
      FREAD(&CurrentIDField,1,RECORD_HEADER_SIZE,fCurrentImage);
      //ASSERT( !strncmp((char*)&CurrentIDField,SECTOR_HEADER,3) );
      // load current ID field
      FREAD(&CurrentIDField,sizeof(TWD1772IDField),1,fCurrentImage);
      // compare with assumed ID field
      if(CurrentIDField.side==IDField->side 
        && CurrentIDField.track==IDField->track 
        && CurrentIDField.num==IDField->num)
      {
        found=true;
      }
      else // skip data, go to next ID field (#bytes depends on len)
      {
        WORD nbytes_to_skip=CurrentIDField.nBytes();
        ASSERTB(nbytes_to_skip==512); // up to now
        FSEEK(fCurrentImage,nbytes_to_skip,SEEK_CUR);
      }
    }
  }
  return found;
}


void TGhostDisk::Init() {
  Version=0x0100; // 1.0
  nRecords=0;
  fCurrentImage=NULL;
  SectorData=NULL;
  SectorBytes=1024; //max
}


bool TGhostDisk::Open(char *path) {
  bool ok=false;
  Close(); // make sure previous image is correctly closed
  const char header_stg[4]="STG";
  fCurrentImage=fopen(path,"rb+"); // try to open existing file
  if(fCurrentImage) // image exists
  {
    char buffer[4];
    FREAD(buffer,1,4,fCurrentImage);
    if(!strncmp(header_stg,buffer,3)) // it's STG
    {
      FREAD(&Version,sizeof(WORD),1,fCurrentImage);
      if(Version>=0x100) // && Version <0x200)
        ok=true;
      FREAD(&nRecords,sizeof(WORD),1,fCurrentImage);
      TRACE_LOG("STG open existing %s, v%04x, %d records\n",CHECKPATH(path),Version,nRecords);
    }
    else
    {
      TRACE_LOG("File %s isn't a STG file\n",path);
      Close();
    }
  }
  else // image doesn't exist
  {
    Init();
    TRACE_LOG("STG create %s\n",CHECKPATH(path));
    fCurrentImage=fopen(path,"wb+"); // create new image
    FWRITE(header_stg,1,4,fCurrentImage); // "STG"
    FWRITE(&Version,sizeof(WORD),1,fCurrentImage);
    FWRITE(&nRecords,sizeof(WORD),1,fCurrentImage);
    ok=true;
  }
  if(ok)
    SectorData=(BYTE*)malloc(SectorBytes); 
  return ok;
}


WORD TGhostDisk::ReadSector(TWD1772IDField *IDField) {
  WORD nbytes=0; 
  if(fCurrentImage && SectorData && FindIDField(IDField))
  {
    nbytes=CurrentIDField.nBytes();
    FREAD(SectorData,1,nbytes,fCurrentImage);
    TRACE_LOG("STG read %d-%d-%d (%d)\n",IDField->side,IDField->track,IDField->num,nbytes);
  } 
  return nbytes;
}


void TGhostDisk::WriteSector(TWD1772IDField *IDField) {
  if(fCurrentImage && SectorData)
  {
    bool IDField_existed=false;
    // the following relies on knowing file pointer after FindIDField()
    if(FindIDField(IDField))
    {
      IDField_existed=true;
      FSEEK(fCurrentImage,-(long)sizeof(TWD1772IDField),SEEK_CUR);
    }
    else
    {
      nRecords++;
      // write record header, record # is in big-endian (easier to read)
      char buf[6];
      sprintf(buf,"%s%c%c",SECTOR_HEADER,HIBYTE(nRecords),LOBYTE(nRecords));
      FWRITE(buf,RECORD_HEADER_SIZE,1,fCurrentImage);    
    }
    // (re)write IDField
    FWRITE(IDField,sizeof(TWD1772IDField),1,fCurrentImage);  
    // write data
    WORD bytes_to_write=IDField->nBytes();
    FWRITE(SectorData,sizeof(BYTE),bytes_to_write,fCurrentImage); 
    TRACE_LOG("STG %s %d-%d-%d (%d)\n", ((IDField_existed)?"update":"write"),
      IDField->side,IDField->track,IDField->num,bytes_to_write);
  }
}


#if defined(SSE_DISK_GHOST)

#include <computer.h>

#if USE_PASTI
/*  Little function to update the value of some register inside
    pasti, hopefully without triggering anything.
*/

void pasti_update_reg(unsigned addr,unsigned data) {
  struct pastiIOINFO pioi;
  pioi.stPC=pc; // would TRUE_PC be better? Pasti probably doesn't use  it
  pioi.cycles=ABSOLUTE_SYS_TIME/TICKS8;    
  pioi.addr=addr;
  pioi.data=data;
  pasti->Io(PASTI_IOWRITE,&pioi);
}

#endif

bool TGhostDisk::CheckCommand(BYTE io_src_b) {
  BYTE &drive=Id;
  TWD1772IDField myIDField;
  myIDField.track=DiskEmu.track;
  myIDField.side=Psg.SelectedSide;
  myIDField.num=Fdc.sr;
  WORD nbytes=SECTOR_SIZE;
/*  Simplest case: the game writes sectors using the "single sector"
    way.
    We don't have "len" info, we guess.
    The sector counter could be set for multiple command calls (eg 9
    for all sectors), so we only envision 512/1024.
*/
  if((io_src_b&0xF0)==0xA0 || (io_src_b&0xF0)==0x80)
  {
    switch(Dma.Counter) {
    case 0:
      nbytes=0;
      break;
    case 2:
      nbytes=SECTOR_SIZE<<1;
      myIDField.len=3;
      break;
    default:
      myIDField.len=2;
    }//sw
  }
  // WRITE 1 SECTOR
  if(nbytes && (io_src_b&0xF0)==0xA0)
  {
    Fdc.cr=io_src_b; //update this...
    if(FloppyDrive[drive].CheckGhostDisk(true))
    {
      // bytes ST memory -> our buffer
      ASSERTB(SectorData);
      for(int i=0;i<nbytes;i++)
        *(SectorData+i)=Dma.GetFifoByte(MNGR_STEEM);
      WriteSector(&myIDField);
      Fdc.str=FDC_STR_MO;
      Fdc.Lines.CommandWasIntercepted=true;
      agenda_fdc_finished(0);
    }
  }
  // READ 1 SECTOR
  if(nbytes && (io_src_b&0xF0)==0x80)
  {   
    // sector is in ghost image?
    if(FloppyDrive[drive].CheckGhostDisk(false) && GhostDisk[drive].ReadSector(&myIDField))
    {
      Fdc.cr=io_src_b; //update this...
      Fdc.str=FDC_STR_MO;
      for(int i=0;i<nbytes;i++)
        Dma.AddToFifo(MNGR_STEEM,*(GhostDisk[drive].SectorData+i));
      Fdc.Lines.CommandWasIntercepted=true;
      agenda_fdc_finished(0); 
    }
  }
/*  For multiples sectors:
    - TOS 1.0 used this instead of R/W 1 sector
    - We don't IRQ, we hope the program will use D0.
    - We assume sectors are 512 bytes, even if it's not the case it could work.
*/
  if((io_src_b&0xF0)==0xB0 || (io_src_b&0xF0)==0x90)
  {
    myIDField.len=2;
    nbytes=myIDField.nBytes(); 
  }
  // WRITE MULTIPLE SECTORS
  if(nbytes && (io_src_b&0xF0)==0xB0)
  {
    if(FloppyDrive[drive].CheckGhostDisk(true))
    {
      Fdc.cr=io_src_b; //update this...
      // for all sectors
      const int k=Dma.Counter;
      for(int j=0;j<k;j++)
      {
        // bytes ST memory -> our buffer
        for(int i=0;i<nbytes;i++)
          *(GhostDisk[drive].SectorData+i)=Dma.GetFifoByte(MNGR_STEEM);
        GhostDisk[drive].WriteSector(&myIDField); // write 1 sector
        myIDField.num=++Fdc.sr;
      }//nxt j
      Fdc.str=FDC_STR_MO; // but no IRQ
      Fdc.Lines.CommandWasIntercepted=true;
    }
  }//multi-write
  // READ MULTIPLE SECTORS
  if(nbytes && (io_src_b&0xF0)==0x90)
  { 
    if(FloppyDrive[drive].CheckGhostDisk(false))
    {
      // for all sectors
      const int k=Dma.Counter;
      for(int j=0;j<k;j++)
      {
        // sector is in ghost image?
        if(FloppyDrive[drive].bGhost && GhostDisk[drive].ReadSector(&myIDField))
        {
          Fdc.cr=io_src_b; //update this...
          for(int i=0;i<nbytes;i++)
            Dma.AddToFifo(MNGR_STEEM,*(GhostDisk[drive].SectorData+i));
          Fdc.str=FDC_STR_MO;
          Fdc.Lines.CommandWasIntercepted=true;
          myIDField.num=++Fdc.sr; // update both sr and ID field's num
        }
      }//nxt j
    }
  }//multi-read
  // FAKE FORMAT
  if((io_src_b&0xF0)==0xF0)
  {
    Fdc.str=FDC_STR_MO;
    Fdc.Lines.CommandWasIntercepted=true;
    //Dma.BaseAddress+=6250;    
    dma_address+=DISK_BYTES_PER_TRACK; 
    Dma.Counter-=DISK_BYTES_PER_TRACK/SECTOR_SIZE;
    agenda_fdc_finished(0);
  }
#if USE_PASTI
/*  Pasti keeps its own variables for all DMA/FDC emulation. This is what
    makes mixing difficult. Updating here is the best way I've found so
    far. TODO
*/
  if(Fdc.Lines.CommandWasIntercepted && hPasti
    && FloppyDrive[drive].ImageType.Manager==MNGR_PASTI)
  {
    //ASSERT(!OPTION_PASTI_JUST_STX||FloppyDrive[drive].ImageType.Extension==EXT_STX);
    pasti_update_reg(0xff8609,Mmu.uDmaCounter.d8[B2]); //    (Dma.BaseAddress&0xff0000)>>16);
    pasti_update_reg(0xff860b,Mmu.uDmaCounter.d8[B1]); //     (Dma.BaseAddress&0xff00)>>8);
    pasti_update_reg(0xff860d,Mmu.uDmaCounter.d8[B0]); //      (Dma.BaseAddress&0xff));
  }
#endif
  return Fdc.Lines.CommandWasIntercepted;
}

#endif

#undef LOGSECTION
#undef HEADER_SIZE
#undef TRACK_HEADER_SIZE
#undef IMAGE_SIZE
#undef NRECORDS_POSITION 
#undef RECORD_HEADER_SIZE
#undef SECTOR_HEADER

#endif//#ifdef SSE_DISK_GHOST
