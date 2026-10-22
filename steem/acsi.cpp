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
FILE: acsi.cpp
CONDITION: SSE_ACSI must be defined
DESCRIPTION: A sector-based emulation of Atari ACSI (Atari Computer System
Interface) hard disks like the SH204 or the Megafile 60.
ACSI was much as SCSI, not as good, cheaper, but effective. On the ST, using
a hard drive wasn't just a theory, it was practical. Even if possible on a plain
ST, most hard drive users had a Mega ST.
Doc: Atari ACSI/DMA Integration guide, June 28, 1991
(no copy/paste of code this time, it's for some fun).
Emulation is straightforward: just fetch sector #n, all are 512 bytes long.
Clusters, partitions, FAT, etc. are OS concepts, not hardware.
It was more difficult to adapt the GUI (hard disk manager...).

v4.1 The legendary laser printer uses the ACSI connection and is emulated here
as well (SSE_ACSI_LASER, also see printer.cpp).
Doc: The Atari Page Printer Reference, 13 april 1988

v4.1.1: support for some SCSI commands (ICD protocol, SSE_ACSI_ICD)
Doc: SCSI Commands Reference Manual, Seagate, February 2006
This allows using larger drives.
---------------------------------------------------------------------------*/


#include <pch.h>
#pragma hdrstop

#if defined(SSE_ACSI)

#include <debug.h>
#include <computer.h>
#include <osd.h>
#include <harddiskman.h>
#include <diskman.h>
#include <iolist.h>

// agenda_acsi() values
enum {
  PHASE_HDRW, // hard drive read/write
  PHASE_LASER_PREFETCH,PHASE_LASER_NEWPAGE,PHASE_LASER_PRINT // laser printer management
};

BYTE acsi_dev=0; // global, active device 0-7

#if defined(SSE_ACSI_LASER) // SLM804 (v4.1)

extern TSLM804Param defaultParam; // forward necessary
pbm_image pbm_descr;

/*  some figures
    8 ppm 7.5s/p, 60 million cycles/page
    data 1 page 2400*3180/8=954000 bytes, 477000 words,
    477000 dma reads *4 = 1,908,000 cycles 0.0318 -> dma load is small
    71Hz 501*224*71*7.5=59759280
    59759280 - 1908000 = 57851280
    224*3.2%=7.168
    -> 8 cycles each scanline, 2 dma reads, 4 dma req
*/

#endif

#define LOGSECTION LOGSECTION_ACSI

void agenda_acsi(int const phase) {

#if defined(SSE_ACSI_LASER)
  if(pbm_descr.data==NULL && phase>=PHASE_LASER_PREFETCH)
    return;
#endif

  switch(phase) {

  case PHASE_HDRW: // hard drive read-write 1 sector
  {
    size_t ok=TRUE;
    for(int j=0;ok&&j<SECTOR_SIZE;j++) // we R/W byte by byte and use DMA request!
    {
#if defined(SSE_ACSI_FMT_AG)
      if(AcsiHdc[acsi_dev].cmd_block[0]==0x04) // format
      {
        if(!DiskMan.bDiskProtectImage)
          ok=FWRITE(&AcsiHdc[acsi_dev].DR,1,1,AcsiHdc[acsi_dev].fpAcsiImg);
      }
#endif
      else
#if defined(SSE_ACSI_ICD)
      if(AcsiHdc[acsi_dev].n_cmd_bytes==10+1)
      {
        if(AcsiHdc[acsi_dev].cmd_block[0+1]==0x2a) // write
        {
          Dma.Drq(MNGR_ACSI);
          if(!DiskMan.bDiskProtectImage)
            ok=FWRITE(&AcsiHdc[acsi_dev].DR,1,1,AcsiHdc[acsi_dev].fpAcsiImg);
        }
        else if((ok=FREAD(&AcsiHdc[acsi_dev].DR,1,1,AcsiHdc[acsi_dev].fpAcsiImg))>0) // fails when driver tests size
          Dma.Drq(MNGR_ACSI);
      }
      else
#endif
      if(AcsiHdc[acsi_dev].cmd_block[0]==0x0A) // write
      {
        Dma.Drq(MNGR_ACSI);
        if(!DiskMan.bDiskProtectImage)
          ok=FWRITE(&AcsiHdc[acsi_dev].DR,1,1,AcsiHdc[acsi_dev].fpAcsiImg);
      }
      else if((ok=FREAD(&AcsiHdc[acsi_dev].DR,1,1,AcsiHdc[acsi_dev].fpAcsiImg))>0) // fails when driver tests size
        Dma.Drq(MNGR_ACSI);
    }//nxt
    AcsiHdc[acsi_dev].block++;
    agenda_delete(agenda_acsi);
    if(ok && AcsiHdc[acsi_dev].block<AcsiHdc[acsi_dev].block_count)
    {
      // if we count DMA cycles, don't add scanlines
      int nhbls=(OPTION_COUNT_DMA_CYCLES)?1:HBL_PER_SECOND/3072;
      agenda_add(agenda_acsi,nhbls,PHASE_HDRW);
    }
    else
    {
      AcsiHdc[acsi_dev].STR=(ok) ? 0 : ((acsi_dev<<5)|BIT_1);
      AcsiHdc[acsi_dev].Irq(true);
    }
    OSD_HD_LED;
    break;
  }

#if defined(SSE_ACSI_LASER)
  case PHASE_LASER_PREFETCH:
    Laser.fill_fifo();
    // no break
  case PHASE_LASER_NEWPAGE:
  {
    //TRACE_LOG("start page DMA $%X %d bytes\n",dma_address,bytes_to_transfer);
    for(int j=0;j<Laser.Param.marginh.d16;j++) // top margin (no need to draw bottom)
      for(int i=0;i<(Laser.Param.pagew.d16+2*Laser.Param.marginw.d16)/8;i++)
        pbm_descr.data[Laser.print_idx++]=0;
    // no break
  }
  case PHASE_LASER_PRINT:
    // The Atari Laser printer was cheap and simple. It used the host computer's
    // memory and just printed the bitmap as it came. Power without the price!
    // This is not hard to emulate. We save bitmap files, we don't print on the PC.
    //TRACE_OSD("PRINT %d %d",bytes_to_transfer,Dma.Counter);
    Laser.busy=true;
    bool end_of_line=false;
    for(int i=0;i<4 && Laser.bytes_to_transfer;i++) // 4 bytes for timing
    {
      if(!Laser.line_bytes)
      {
        for(int j=0;j<Laser.Param.marginw.d16/8;j++) // left margin
          pbm_descr.data[Laser.print_idx++]=0;
      }
      BYTE dma_data=Laser.get_fifo_byte(); // byte per byte
      pbm_descr.data[Laser.print_idx]=dma_data; // from left to right, bit=1 -> black
      Laser.print_idx++;
      Laser.line_bytes++;
      Laser.bytes_to_transfer--;
      if(end_of_line)
        break;
      if(Laser.line_bytes==Laser.Param.pagew.d16/8)
      {
        //TRACE_LOG("line done\n");
        Laser.line_bytes=0;
        for(int j=0;j<Laser.Param.marginw.d16/8;j++) // right margin
          pbm_descr.data[Laser.print_idx++]=0;
        end_of_line=true;
      }
    }//nxt
    if(Laser.bytes_to_transfer)
    {
      agenda_delete(agenda_acsi);
      agenda_add(agenda_acsi,((end_of_line) ? 3 : 1),PHASE_LASER_PRINT);
    }
    else
    {
      Laser.Param.pagecount.d16++;
      TRACE_LOG("print page %d finished Dma.Counter %d page_bytes %d no_dma %d\n",
        Laser.Param.pagecount.d16,Dma.Counter,Laser.page_bytes,Laser.no_dma);
      AcsiHdc[ACSI_ID_LASER].STR=0;
      AcsiHdc[ACSI_ID_LASER].Irq(true);
      Laser.busy=false;
      // we share pbm # with Epson grapihcs
      EasyStr Path=ParallelPort.File;
      RemoveFileNameFromPath(Path,WITH_SLASH);
      Path+="SLM804_";
      Path+=(EasyStr("00000")+SSEConfig.PagePbm++).Rights(5);
      Path+=".pbm";
      FILE *outfile=fopen(Path,"wb");
      if(outfile)
      {
        TRACE_LOG("%s\n",CHECKPATH(Path.Text));
        pbm_save(&pbm_descr,outfile);
        fclose(outfile);
      }
      if(Laser.pages_to_print)
        Laser.pages_to_print--;
      if(Laser.pages_to_print)
      {
        Laser.bytes_to_transfer=Laser.bytes_per_page();
        agenda_delete(agenda_acsi);
        agenda_add(agenda_acsi,400,(AcsiHdc[ACSI_ID_LASER].cmd_block[5]&BIT_6)
          ? PHASE_LASER_NEWPAGE : PHASE_LASER_PREFETCH);
      }
      else
      {
        free(pbm_descr.data);
        pbm_descr.data=NULL;
      }
    }
    break;
#endif//#if defined(SSE_ACSI_LASER)

  }//switch(phase)
}


TAcsiHdc::~TAcsiHdc() {
  CloseImageFile();
#if defined(SSE_ACSI_LASER)
  if(Id==ACSI_ID_LASER && pbm_descr.data!=NULL)
  {
    free(pbm_descr.data);
#ifndef SSE_LEAN_AND_MEAN
    pbm_descr.data=NULL;
#endif
  }
#endif
}


void TAcsiHdc::CloseImageFile() {
  if(fpAcsiImg)
    fclose(fpAcsiImg);
  fpAcsiImg=NULL;
  Active=FALSE;
}


void TAcsiHdc::Format() { 
#if !defined(SSE_ACSI_FMT_AG)
  BYTE sector[SECTOR_SIZE];
  memset(sector,0x6c,SECTOR_SIZE);
#endif
  FSEEK(fpAcsiImg,0,SEEK_SET); //restore
#if defined(SSE_ACSI_FMT_AG)
  block=0;
  block_count=nSectors;
  agenda_delete(agenda_acsi);
  agenda_add(agenda_acsi,HBL_PER_SECOND/3072,PHASE_HDRW);
  AcsiHdc[acsi_dev].DR=0x6C;
  Irq(false);
#else
  if(!DiskMan.bDiskProtectImage)
    for(DWORD i=0+2;i<nSectors;i++)
      FWRITE(sector,SECTOR_SIZE,1,fpAcsiImg); //fill sectors
#endif
}


bool TAcsiHdc::Init(BYTE const num, char* const path) {
  //ASSERT(num<MAX_ACSI_DEVICES);
  CloseImageFile();
  memset(inquiry_string,0,sizeof(inquiry_string));
  fpAcsiImg=fopen(path,"rb+");
  Active=(fpAcsiImg!=NULL); // file is there or not
  if(Active) // note it could be anything, even HD6301V1ST.img or TOS102.img
  {
    DWORDLONG l=GetFileLength(fpAcsiImg); //in bytes - int is not enough
    nSectors=l/SECTOR_SIZE;
    Id=num;
    ASSERTB((Id&7)==Id);
#ifndef SSE_LEAN_AND_MEAN
    Id&=7;
#endif
    char *filename=GetFileNameFromPath(path);
    char *dot=strrchr(filename,'.');
    int nchars=(dot) ? (int)(dot-filename) : 23;
    //ASSERT(nchars>0);
    strncpy(inquiry_string+8,filename,nchars);
#if defined(SSE_ENABLE_TRACE_LOG) // examine MBR
    TRACE_LOG("ACSI %d ID %s phys sectors %d",Id,inquiry_string+8,nSectors);
    FSEEK(fpAcsiImg,0x1c2,SEEK_SET); //restore
    int siz;
    BYTE p_flg;
    char p_id[3+1];
    BYTE p_st[4];
    FREAD(&siz, 4, 1, fpAcsiImg);
    SWAP_BIG_ENDIAN_DWORD(siz);
    TRACE_LOG(" MBR %d sectors (%d MB)\nPartitions",siz,siz/(2*1024));
    for(int i=0;i<4;i++)
    {
      FREAD(&p_flg,1,1,fpAcsiImg);
      FREAD(&p_id,1,3,fpAcsiImg);
      p_id[3]='\0';
      FREAD(&p_st,1,4,fpAcsiImg);
      FREAD(&siz,4,1,fpAcsiImg);
      SWAP_BIG_ENDIAN_DWORD(siz);
      TRACE_LOG(" %d:%X %s %X%X%X%X %d",i,p_flg,p_id,p_st[0],p_st[1],p_st[2],p_st[3],siz);
    }
    TRACE_LOG("\n");
#endif
    acsi_dev=Id;
  }
  //TRACE_INIT("ACSI %d open %s %d sectors %d MB\n",Id,path,nSectors,nSectors/(2*1024));
  return (Active!=0);
}


BYTE TAcsiHdc::IORead() {
  BYTE ior_byte=0;
  if((Dma.mcr&0xFF)==0x8a) // "read status"
  {
    ior_byte=STR;
    TRACE_LOG2("ACSI %d PC %X read STR = %X\n",Id,old_pc,ior_byte);
  }
  if(stem_runmode==STEM_MODE_CPU)
  {
#if defined(SSE_ACSI_LASER)
    if(Laser.bytes_to_transfer && *cmd_block!=0xA) // MODE SENSE + INQUIRY
    {
      Laser.bytes_to_transfer--;
      STR=Laser.controller_buffer[Laser.buffer_idx++]; // next byte
      //if(isalnum(STR)) TRACE_LOG("%c (%X)\n",STR,Laser.bytes_to_transfer);
      Irq(true);
    }
    else
#endif
      Irq(false);
    Active=TRUE;
  }
  return ior_byte;
}


void TAcsiHdc::IOWrite(BYTE const Line,BYTE io_src_b) {
  TRACE_LOG2("PC %X ACSI %d line %d %X\n",old_pc,Id,Line,io_src_b);
  if(!fpAcsiImg && (!OPTION_LASER || Id!=ACSI_ID_LASER))
    return;
  DiskEmu.AcsiBsy=true;
  //TRACE_HD("ACSI PC %X write %X = %X\n",old_pc,Line,io_src_b);
  bool do_irq=false;
  // take new command only if A1 is low, it's our ID and we're ready
  // A1 in ACSI doc is A0 in DMA doc
#if defined(SSE_ACSI_LASER)
  bool ok=(fpAcsiImg || Id==ACSI_ID_LASER&&OPTION_LASER&&!Laser.busy);
  if(!Line && (io_src_b>>5)==Id)
  {
    acsi_dev=Id;
    io_src_b&=0x1f;
    if(Ready && ok)
    {
      cmd_ctr=0;
      n_cmd_bytes=6; // ACSI/SCSI group 0
      Ready=FALSE;
    }
  }
  if(!ok)
  {
    STR=(Id<<5)|BIT_1;
    return;
  }
#else
  if(!Line && (io_src_b>>5)==Id && Ready)
  {
    cmd_ctr=0;
    n_cmd_bytes=6;
    Ready=FALSE;
    io_src_b&=0x1f;
    acsi_dev=Id; // we have the bus
    //ASSERT(acsi_dev<MAX_ACSI_DEVICES);
  }
#endif
  if(cmd_ctr<n_cmd_bytes) // getting command
  {
    cmd_block[cmd_ctr]=io_src_b;
    ASSERTB(Line||!cmd_ctr||AcsiHardDiskMan.nDrives==1);
#if defined(SSE_ACSI_ICD)
    if(!cmd_ctr && io_src_b==0x1F)
      n_cmd_bytes=10+1; //we keep the ICD opcode $1F (or...)
#endif
    cmd_ctr++;
    do_irq=true;
  }
#if defined(SSE_ACSI_LASER) // MODE SELECT
  if(OPTION_LASER && Id==ACSI_ID_LASER && cmd_ctr>n_cmd_bytes)
  { 
    if(!Laser.buffer_idx)
      Laser.bytes_to_transfer=(io_src_b) ? io_src_b : NLASER_PARAMETERS;
    Laser.controller_buffer[Laser.buffer_idx++]=io_src_b;
    if(Laser.buffer_idx>Laser.bytes_to_transfer)
    {
      TSLM804Param::copy_buffer_to_config(&Laser.Param,Laser.controller_buffer);
      Laser.busy=false;
    }
  }
#endif
  if(cmd_ctr==n_cmd_bytes) // command in
  {
#if defined(SSE_ENABLE_TRACE_LOG)
    TRACE_LOG("PC %06X %cCSI %d command",old_pc,(n_cmd_bytes==6)?'A':'S',Id);
    for(int i=0;i<n_cmd_bytes;i++)
    {
      TRACE_LOG(" $%02X",cmd_block[i]);
    }
    TRACE_LOG("\n");
#endif
#if defined(SSE_ACSI_LASER)
    // The SLM804 controller doesn't ouput DMA data to the ST, and main commands
    // are different, so we have an apart switch() for it
    if(OPTION_LASER && Id==ACSI_ID_LASER)
    {
      STR&=0x1F;
      STR|=Id<<5;
      switch(*cmd_block) {
      case 0x00:
        break;
      case 0x03: // Request Sense
        TRACE_LOG("REQUEST SENSE\n");
        break;
      case 0x0a: // Print
        TRACE_LOG("PRINT\n");
        Laser.pages_to_print=cmd_block[4];
        if(!Laser.pages_to_print) // 0=1, FF=infinity
          Laser.pages_to_print++;
        agenda_delete(agenda_acsi);
        agenda_add(agenda_acsi,milliseconds_to_hbl(2000),PHASE_LASER_PREFETCH);
        do_irq=false;
        Irq(false);
        pbm_descr.width=Laser.Param.pagew.d16+Laser.Param.marginw.d16*2;
        pbm_descr.height=Laser.Param.pageh.d16+Laser.Param.marginh.d16*2;
        Laser.bytes_to_transfer=Laser.bytes_per_page();
        pbm_descr.size=pbm_descr.width/8*pbm_descr.height;
        if(pbm_descr.data!=NULL)
          free(pbm_descr.data);
        if((pbm_descr.data=(BYTE*)malloc(pbm_descr.size))!=NULL)
          ZeroMemory(pbm_descr.data,pbm_descr.size);
        Laser.line_bytes=Laser.print_idx=0;
        Laser.busy=true;
        TRACE_LOG("%dx%dx%d margin %dx%d\n",Laser.pages_to_print,Laser.Param.pagew.d16,
          Laser.Param.pageh.d16,Laser.Param.marginw.d16,Laser.Param.marginh.d16);
        break;
      case 0x12: // Inquiry
        TRACE_LOG("INQUIRY\n");
        if(cmd_block[5]&BIT_7)
        { // 0 2 X X X len string
          STR=0;
          strcpy((char*)Laser.controller_buffer,"-----PAGE PRINTER:SLMC804v1.1:ATARI "); // PAGE PRINTER: necessary
          Laser.controller_buffer[0]=2;
          Laser.bytes_to_transfer=(int)strlen((char*)Laser.controller_buffer+5)+5;
          Laser.controller_buffer[1]=Laser.controller_buffer[2]=Laser.controller_buffer[3]=0;
          Laser.controller_buffer[4]=(BYTE)Laser.bytes_to_transfer-5;
          TRACE_LOG("%s %d %d\n",Laser.controller_buffer+5,Laser.controller_buffer[4],Laser.bytes_to_transfer);
          Laser.buffer_idx=0;
        }
        else
          STR|=0x12;
        break;
      case 0x15: // Mode Select
        TRACE_LOG("MODE SELECT\n");
        if(cmd_block[5]&BIT_7) // reset to default
        {
          TSLM804Param::copy_config_to_buffer(&defaultParam,Laser.controller_buffer);
          TSLM804Param::copy_buffer_to_config(&Laser.Param,Laser.controller_buffer);
        }
        else
        {
          TSLM804Param::copy_config_to_buffer(&Laser.Param,Laser.controller_buffer);
          Laser.buffer_idx=0;
          cmd_ctr++; // accept more bytes
        }
        break;
      case 0x1a: // Mode Sense
        TRACE_LOG("MODE SENSE\n");
        Laser.controller_buffer[0]=(cmd_block[4]) ? cmd_block[4] : NLASER_PARAMETERS;
        if(Laser.controller_buffer[0]>NLASER_PARAMETERS)
          Laser.controller_buffer[0]=NLASER_PARAMETERS;
        STR=Laser.buffer_idx=0;
        Laser.bytes_to_transfer=Laser.controller_buffer[0]+1;
        TSLM804Param::copy_config_to_buffer( (cmd_block[5]&BIT_7) 
          ? &defaultParam : &Laser.Param,Laser.controller_buffer);
        break;
      case 0x1b: // Stop Print
        TRACE_LOG("STOP PRINT\n");
        agenda_delete(agenda_acsi);
        break;
      default:
        STR|=0x12;
      }
      cmd_ctr++;
      Ready=TRUE;
      if(do_irq)
        Irq(true);
      return;
    }
#endif//SSE_ACSI_LASER

    STR=Id<<5; // all fine
    switch(*cmd_block) {
    case 0x00: //ready
      break;
    case 0x03: //request sense 
      DR=error_code;
      Dma.Drq(MNGR_ACSI);
      DR=0;
      Dma.Drq(MNGR_ACSI);
      Dma.Drq(MNGR_ACSI);
      Dma.Drq(MNGR_ACSI);
      break;
    case 0x04: //format
      Format();
      break;
    case 0x07: // Initialize Element Status
      do_irq=true;
      break;
    case 0x08: //read
    case 0x0a: //write
      block_count=cmd_block[4]; // 8bit
      TRACE_LOG("%s(6) sectors %d-%d (%d)\n",(*cmd_block==0x0a)?"Write":"Read",
        SectorNum(),SectorNum()+block_count-1,block_count);
      ReadWrite();
      do_irq=false;
      break;
    case 0x0b: //seek
      Seek();
      break;
    case 0x12: //inquiry
      Inquiry();
      break;
    case 0x15: //SCSI mode select
    {
      TRACE_LOG("Mode select (%d) %d %x\n",cmd_block[4],Dma.Counter,dma_address);
      for(int i=0;i<cmd_block[4];i++)
        Dma.Drq(MNGR_ACSI); //do nothing with it?
      break;
    }
    case 0x1a: // Mode Sense 6 
      TRACE_LOG("MODE SENSE\n");
      DR=0;
      for(int i=0;i<cmd_block[4];i++)
        Dma.Drq(MNGR_ACSI);
      do_irq=true;
      break;
#if defined(SSE_ACSI_ICD)
    case 0x1f: // special opcode, the rest is SCSI
      switch(cmd_block[0+1]) {
      case 0x25: //READ CAPACITY (10) assume total, use DMA
        DR=(BYTE)(this->nSectors>>24);
        Dma.Drq(MNGR_ACSI);
        DR=(BYTE)(this->nSectors>>16);
        Dma.Drq(MNGR_ACSI);
        DR=(BYTE)(this->nSectors>>8);
        Dma.Drq(MNGR_ACSI);
        DR=(BYTE)(this->nSectors>>0);
        Dma.Drq(MNGR_ACSI);
        DR=0;
        Dma.Drq(MNGR_ACSI);
        Dma.Drq(MNGR_ACSI);
        DR=2;//(SECTOR_SIZE>>8);
        Dma.Drq(MNGR_ACSI);
        DR=0;
        Dma.Drq(MNGR_ACSI);
        break;
      case 0x28: //READ (10)
      case 0x2a: //WRITE (10)
        block_count=(cmd_block[7+1]<<8)+cmd_block[8+1]; // 16bit
        TRACE_LOG("%s(10) sectors %d-%d (%d)\n",(cmd_block[1]==0x2a)?"Write":"Read",
          SectorNum(),SectorNum()+block_count-1,block_count);
        ReadWrite();
        do_irq=false;
        break;
      case 0x2b: //SEEK (10)
        Seek();
        break;
      default:
        TRACE_LOG("ICD $%X?\n",cmd_block[0+1]);
      }//switch(cmd_block[1])
      break;
#endif
    default: //other commands
      TRACE_LOG("ACSI command $%02X?\n",*cmd_block);
      STR=(Id<<5)|BIT_1;
      error_code=0x20; //invalid opcode
    }//switch(*cmd_block)
#if defined(SSE_ENABLE_TRACE_LOG)
    if(STR&2)
      TRACE_LOG("ACSI error STR %X error code %X\n",STR,error_code);
#endif
    cmd_ctr++;
    Ready=TRUE;
    OSD_HD_LED;
    Active=TRUE;
  }
  if(do_irq)
    Irq(true);
}


void TAcsiHdc::Inquiry() {//drivers display this so we have a cool name
  TRACE_LOG("Inquiry: %s\n",inquiry_string+8); //strange...
  for(int i=0;i<32;i++)
  {
    DR=inquiry_string[i];
    Dma.Drq(MNGR_ACSI); // TODO supposed to fail?!
  }
}


void TAcsiHdc::Irq(bool const state) {
  TRACE_LOG2("ACSI %d Irq %d\n",Id,state);
  hdc_irq=state;
  update_disk_irq();
}



/*
sector in agenda. timing?

Atari  1.25 MB/s (or 1.25 million bytes)
Petari 1914 KB/S

let's say 1.5 MB = 1024*1024*1.5 = 1572864 bytes = 3072 sectors/s

HBL_PER_SECOND/3072 -> 5... in colour, 11... in mono
*/

void TAcsiHdc::ReadWrite() {
#if defined(SSE_STATS)
  Stats.nHdsector+=(COUNTER_VAR)block_count;
#endif
  bool ok=Seek();
  if(!ok)
  {
    STR=(Id<<5)|BIT_1;
    Irq(true);
  }
  else
  {
    block=0;
    agenda_delete(agenda_acsi);
    agenda_add(agenda_acsi,HBL_PER_SECOND/3072,PHASE_HDRW);
    Irq(false);
  }
}


void TAcsiHdc::Reset() {
  Ready=TRUE;
  cmd_ctr=7; // "ready"; we don't restore // keep for older versions?
#if defined(SSE_ACSI_LASER)
  if(Id==ACSI_ID_LASER)
    Laser.Reset();
#endif
}


// get absolute sector index from command
DWORD TAcsiHdc::SectorNum() {
  DWORD block_number;
  // it's a DWORD
  // $FFFFFFFF * $200 = 2,199,023,255,040 bytes
  //           1 tera = 1,099,511,627,776 bytes
  // using | and casts just in case but it shouldn't make a difference
#if defined(SSE_ACSI_ICD)
  if(n_cmd_bytes==10+1)
    block_number=((DWORD)cmd_block[2+1]<<24)|((DWORD)cmd_block[3+1]<<16)
                 |((DWORD)cmd_block[4+1]<<8)|(DWORD)cmd_block[5+1]; //32bit
  else
#endif
    block_number=((DWORD)cmd_block[1]<<16)|((DWORD)cmd_block[2]<<8)|(DWORD)cmd_block[3]; //24bit
  return block_number;
}


bool TAcsiHdc::Seek() {
/*
_DEBUG 32bit code
No cast: seems to limit to 32bit!
00438159 8B 4D FC             mov         ecx,dword ptr [this]  
0043815C E8 3F FF FF FF       call        TAcsiHdc::SectorNum (04380A0h)  
00438161 C1 E0 09             shl         eax,9  
00438164 33 C9                xor         ecx,ecx
00438166 89 45 F0             mov         dword ptr [Offset],eax  
00438169 89 4D F4             mov         dword ptr [ebp-0Ch],ecx
Cast:
00438159 8B 4D FC             mov         ecx,dword ptr [this]  
0043815C E8 3F FF FF FF       call        TAcsiHdc::SectorNum (04380A0h)  
00438161 B9 00 02 00 00       mov         ecx,200h  
00438166 F7 E1                mul         eax,ecx  
00438168 89 45 F0             mov         dword ptr [Offset],eax  
0043816B 89 55 F4             mov         dword ptr [ebp-0Ch],edx  
*/
  INT64 Offset=(INT64)SectorNum()*(INT64)SECTOR_SIZE; // casts are important!
  if(FSEEK(fpAcsiImg,Offset,SEEK_SET))
    STR=(Id<<5)|BIT_1;
  return (!(STR&BIT_1)); // that would mean "OK"
}

#undef LOGSECTION

#endif//#if defined(SSE_ACSI)
