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
FILE: interface_caps.cpp
CONDITION: SSE_DISK_CAPS must be defined
DESCRIPTION: Implementation of CAPS (SPS) library support in Steem SSE.
Steem supports IPF and CTR (not RAW).
We use an improved version of CapsPlug (3rd party) to access the DLL or
the Linux library.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#if defined(SSE_DISK_CAPS)

#include <debug.h>
#include <computer.h>
#include <caps/CapsPlug.h> // 3rdparty
#include <diskman.h>
#include <iolist.h>

/*
  for static lib, this directory should be added to linker search paths:
 ../3rdparty/caps_linux/capsimg_source_linux_macosx/CAPSImg 
*/

TCaps::TCaps() {
#ifndef SSE_LEAN_AND_MEAN
  CAPSIMG_OK=0; // we init in main to keep control of timing/be able to trace
#endif
#if defined(SSE_DISK_CAPS_MEMORY)
  pImage[0]=pImage[1]=NULL;
#endif
}


TCaps::~TCaps() {
  try {
#if defined(SSE_DISK_CAPS_MEMORY)
    if(pImage[0])
      delete [] pImage[0];
    if(pImage[1])
      delete [] pImage[1];
#ifndef SSE_LEAN_AND_MEAN
    pImage[0]=pImage[1]=NULL;
#endif
#endif
    if(CAPSIMG_OK)
    {
#if !defined(SSE_DISK_CAPS_STATIC)
      CapsRemImage(ContainerID[DRIVE_A]);
      CapsRemImage(ContainerID[DRIVE_B]);
      CapsExit();
#endif
    }
  }
  catch(...) {
    // note no trace in global destructors
    // note 2: VC-specific
  }
#ifndef SSE_LEAN_AND_MEAN
  CAPSIMG_OK=FALSE;
#endif
}


#define LOGSECTION LOGSECTION_INIT

int TCaps::Init() {
  Active=FALSE;
  Version=0;
  ContainerID[DRIVE_A]=ContainerID[DRIVE_B]=-1;
  LockedSide[DRIVE_A]=LockedSide[DRIVE_B]=-1;
  LockedTrack[DRIVE_A]=LockedTrack[DRIVE_B]=-1; 
  CapsVersionInfo versioninfo;
  if(CapsInit(WIN_ONLY(SSE_DISK_CAPS_PLUGIN_FILE))) // CAPSImg.dll
  {
#ifndef SSE_LEAN_AND_MEAN
    CAPSIMG_OK=FALSE;
#endif
    return 0;
  }
  CapsGetVersionInfo((void*)&versioninfo,0);
  TRACE_LOG("%s loaded, v%d.%d\n",SSE_DISK_CAPS_PLUGIN_FILE,versioninfo.release,versioninfo.revision);
  Version=versioninfo.release*10+versioninfo.revision; 
  // controller init
  fdc.type=sizeof(CapsFdc);  // must be >=sizeof(CapsFdc)
  fdc.model=cfdcmWD1772;
  fdc.clockfrq=CpuNormalHz/TICKS8; // should have different clock?
  fdc.drive=Drive;
  // drives
  Drive[DRIVE_A].type=Drive[DRIVE_B].type=sizeof(CapsDrive); // must be >=sizeof(CapsDrive)
  Drive[DRIVE_A].rpm=Drive[DRIVE_B].rpm=CAPSDRIVE_35DD_RPM;
  Drive[DRIVE_A].maxtrack=Drive[DRIVE_B].maxtrack=CAPSDRIVE_35DD_HST;
  fdc.drivemax=fdc.drivecnt=2; // will be changed by SetNumFloppies
  int ec=CapsFdcInit(&fdc);
  if(ec!=imgeOk)
  {
    TRACE_LOG("CAPSFdcInit failure %d\n",ec);
    return (Version=0);
    //return 0;
  }
  // The callbacks must be set up after CapsFdcInit()
  fdc.cbdrq=CallbackDRQ;
  fdc.cbirq=CallbackIRQ;
  fdc.cbtrk=CallbackTRK;
  // we already create our 2 Caps drives, instead of waiting for an image:
  ContainerID[DRIVE_A]=CapsAddImage();
  ContainerID[DRIVE_B]=CapsAddImage();
  //ASSERT(ContainerID[0]!=-1&&ContainerID[1]!=-1);
///BUG too late  fdc.drivemax=fdc.drivecnt=2; // will be changed by SetNumFloppies
  CAPSIMG_OK=(Version>0);
  return Version;
}


void TCaps::Reset() {
  CapsFdcReset(&fdc);
}


#undef LOGSECTION
#define LOGSECTION LOGSECTION_IMAGE_INFO

// only called by SetDisk()
int TCaps::InsertDisk(int drive,char* File,CapsImageInfo *img_info) {
  if(!CAPSIMG_OK)
    return -1;
  //ASSERT( !drive || drive==1 );
  //ASSERT( img_info );
  //ASSERT( ContainerID[drive]!=-1 );
  bool FileIsReadOnly=FloppyDisk[drive].ReadOnly;
#if defined(SSE_DISK_CAPS_MEMORY)
/*  File in memory, it's more code and data for Steem without a clear benefit:
    not defined for the moment */
  FILE *fn=fopen(File,"rb");
  ASSERTB(fn);
  long filelen=GetFileLength(fn);
  ASSERTB(!pImage[drive]);
  pImage[drive]=new BYTE[filelen];
  fread(pImage[drive],1,filelen,fn);
  fclose(fn);
  CapsLockImageMemory(ContainerID[drive],pImage[drive],filelen,0); // what flag?
#else
  CapsLockImage(ContainerID[drive],File); // open the CAPS file
#endif
  CapsGetImageInfo(img_info,ContainerID[drive]);
  //ASSERT( img_info->type==ciitFDD );
  TRACE_LOG("Disk in %c is CAPS release %d rev %d of %d/%d/%d for ",
    drive+'A',img_info->release,img_info->revision,img_info->crdt.day,
    img_info->crdt.month,img_info->crdt.year);
  for(int i=0;i<CAPS_MAXPLATFORM;i++)
  {
#if defined(SSE_ENABLE_TRACE_LOG)
    if((img_info->platform[i])!=ciipNA)
      TRACE_LOG("%s ",CapsGetPlatformName(img_info->platform[i]));
#endif
  }
  TRACE_LOG("Sides:%d Tracks:%d-%d\n",img_info->maxhead+1,img_info->mincylinder,img_info->maxcylinder);
  Active=TRUE;
  Drive[drive].diskattr|=CAPSDRIVE_DA_IN;
  if(!FileIsReadOnly)
    Drive[drive].diskattr&=~CAPSDRIVE_DA_WP;
  else
    Drive[drive].diskattr|=CAPSDRIVE_DA_WP;
#if defined(SSE_DRIVE_SINGLESIDE)
  if(FloppyDrive[drive].bSingleSided && Caps.Version>50)
    Drive[drive].diskattr|=CAPSDRIVE_DA_SS; //tested OK on Dragonflight
#endif
  CapsFdcInvalidateTrack(&fdc,drive); // Galaxy Force II
  LockedTrack[drive]=LockedSide[drive]=-1;
  return ERR_OK;
}


void TCaps::RemoveDisk(int drive) {
  if(!CAPSIMG_OK)
    return;
  TRACE_LOG("Drive %c removing image\n",drive+'A');
  CapsUnlockImage(Caps.ContainerID[drive]); // eject disk
  Drive[drive].diskattr&=~CAPSDRIVE_DA_IN;
  if(FloppyDrive[!drive].ImageType.Manager==MNGR_CAPS)
    Active=FALSE;
#if defined(SSE_DISK_CAPS_MEMORY)
  delete [] pImage[drive];
  pImage[drive]=NULL;
#endif
}


void TCaps::WritePsgA(int data) {
  // drive selection
  fdc.drivenew=-2;
  switch(data&(BIT_2|BIT_1)) {
  case BIT_2:
    fdc.drivenew=DRIVE_A;
    break;
  case BIT_1:
    fdc.drivenew=DRIVE_B;
    break;
  case 0:
    if(fdc.drivecnt==1) // see TYM2149::CurrentDrive()
      fdc.drivenew=DRIVE_A;
    break;
  }
  if(!fdc.drivenew || fdc.drivenew==1)
    Drive[fdc.drivenew].newside=((data&BIT_0)==0);
}


UDWORD TCaps::ReadWD1772(BYTE Line) {
  UDWORD data=CapsFdcRead(&fdc,Line); 
  if(!Line) // read status register
  {
    if(floppy_mediach[DRIVE])
    {
      if(floppy_mediach[DRIVE]/10!=1)
        data|=FDC_STR_WP;
      else
        data&=~FDC_STR_WP;
      TRACE_SECTION(LOGSECTION_FDC,"FDC SR mediach %d WP %x\n",floppy_mediach[DRIVE],data&FDC_STR_WP);
    }
  }
  return data;
}


void TCaps::WriteWD1772(BYTE Line,BYTE data) {
  if(!Line) // command
  {
    if(!(::Fdc.str&FDC_STR_MO)) // assume no cycle run!
      FloppyDrive[DRIVE].bMotor=true;
  }
  CapsFdcWrite(&fdc,Line,data); // send to DLL
}


void TCaps::Hbl() {
  // we run cycles at each HBL if there's an IPF file in. Performance OK
  CapsFdcEmulate(&fdc,Glue.CurrentScanline.Cycles/TICKS8);
}


/*  Callback functions. Since they're static, they access object data like
    any external function, using 'Caps.'
*/

void CAPS_CALLCONV TCaps::CallbackDRQ(PCAPSFDC, UDWORD) {
  // transfer one byte
  if(!(Dma.mcr&BIT_8)) // disk->RAM
    Dma.AddToFifo(MNGR_CAPS,(BYTE)CapsFdcGetInfo(cfdciR_Data,&Caps.fdc,0) );
  else // RAM -> disk
    Caps.fdc.r_data=Dma.GetFifoByte(MNGR_CAPS);  
  Caps.fdc.r_st1&=~CAPSFDC_SR_IP_DRQ; // The Pawn
  Caps.fdc.lineout&=~CAPSFDC_LO_DRQ; //?
}


void CAPS_CALLCONV TCaps::CallbackIRQ(PCAPSFDC, UDWORD lineout) {
  // function called to clear IRQ, can mess with sound (Jupiter's Masterdrive)
  if(lineout) 
  {
    DiskEmu.Update(true);
#if defined(DEBUG_BUILD)
    ASSERTB(DiskEmu.LastManager==MNGR_CAPS);
    if(TRACE_LOG_ENABLED(LOGSECTION_FDC)) 
      DiskEmu.TraceRegs();
#endif
#if defined(SSE_DRIVE_SOUND)
    if(OPTION_DRIVE_SOUND)
      FloppyDrive[DRIVE].SoundCheckIrq();
#endif
  }
  fdc_irq=((lineout&CAPSFDC_LO_INTRQ)==CAPSFDC_LO_INTRQ);
  update_disk_irq();
  if(FLOPPY_ACCESS_FF(DRIVE)) 
    floppy_access_ff_counter=FLOPPY_FF_VBL_COUNT;
}


void CAPS_CALLCONV TCaps::CallbackTRK(PCAPSFDC, UDWORD drive) {
  int side=Caps.Drive[drive].side;
  int track=Caps.Drive[drive].track;
  CapsTrackInfoT2 track_info; // apparently we must use type 2...
  track_info.type=1;
  UDWORD flags=DI_LOCK_DENALT|DI_LOCK_DENVAR|DI_LOCK_UPDATEFD|DI_LOCK_TYPE;
#if defined(SSE_DISK_CAPS_MEMORY)
  flags|=DI_LOCK_MEMREF;
#endif
  CapsRevolutionInfo CRI;
  if(Caps.LockedSide[drive]!=side || Caps.LockedTrack[drive]!=track)
    CapsSetRevolution(Caps.ContainerID[drive],0); // new track, reset #revs just in case
  CapsLockTrack((PCAPSTRACKINFO)&track_info,Caps.ContainerID[drive],track,side,flags);
  CapsGetInfo(&CRI,Caps.ContainerID[drive],track,side,cgiitRevolution,0);
  TRACE_LOG("max rev %d real %d next %d\n",CRI.max,CRI.real,CRI.next);
  //ASSERT( track==(int)track_info.cylinder );
  //ASSERT( !track_info.sectorsize ); //normally 0
  TRACE_LOG("CAPS Lock %c:S%dT%d flags %X sectors %d tracklen %d overlap %d startbit %d timebuf %x\n",
    drive+'A',side,track,flags,track_info.sectorcnt,track_info.tracklen,track_info.overlap,track_info.startbit,track_info.timebuf);
  Caps.Drive[drive].trackbuf=track_info.trackbuf;
  Caps.Drive[drive].timebuf=track_info.timebuf;
  Caps.Drive[drive].tracklen=track_info.tracklen;
  Caps.Drive[drive].overlap=track_info.overlap;
  Caps.Drive[drive].ttype=track_info.type; //?
  Caps.LockedSide[drive]=side;
  Caps.LockedTrack[drive]=track;
#if defined(SSE_ENABLE_TRACE_LOG)
  if(FloppyDrive[drive].ImageType.Extension==EXT_IPF) // not CTR
  {
    CapsSectorInfo CSI;
    TRACE_LOG("sector info (encoder,cell type,data,gap info)\n");
    for(DWORD sec_num=1;sec_num<=track_info.sectorcnt;sec_num++)
    {
      CapsGetInfo(&CSI,Caps.ContainerID[drive],track,side,cgiitSector,sec_num-1);
      TRACE_LOG2("#%d|%d|%d|%d %d %d|%d %d %d %d %d %d %d\n",
        sec_num,
        CSI.enctype,      // encoder type
        CSI.celltype,     // bitcell type
        CSI.descdatasize, // data size in bits from IPF descriptor
        CSI.datasize,     // data size in bits from decoder
        CSI.datastart,    // data start position in bits from decoder
        CSI.descgapsize,  // gap size in bits from IPF descriptor
        CSI.gapsize,      // gap size in bits from decoder
        CSI.gapstart,     // gap start position in bits from decoder
        CSI.gapsizews0,   // gap size before write splice
        CSI.gapsizews1,   // gap size after write splice
        CSI.gapws0mode,   // gap size mode before write splice
        CSI.gapws1mode);   // gap size mode after write splice
    }//nxt
  }//if
#endif
}

#endif//#if defined(SSE_DISK_CAPS)

#undef LOGSECTION
