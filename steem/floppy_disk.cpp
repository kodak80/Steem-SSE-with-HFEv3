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
FILE: floppy_disk.cpp
DESCRIPTION: Definitions for the Floppy Disk objects.
Functions to get ID fields and sector or track data of some image types.
Functions that can give positions relative to the index or the gaps for
some disk image types.
---------------------------------------------------------------------------*/


#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <computer.h>
#include <diskman.h>
#include <iolist.h>


char *extension_list[NUM_EXT]={ "","ST","MSA","DIM","STT","STX","IPF",
"CTR","STG","STW","PRG","TOS","SCP","HFE"};

char* disk_manager[NUM_MNGR]={"NONE","STEEM","PASTI","CAPS","WD1772","PRG","ACSI"};

#if defined(SSE_DISK_M3U)

#define LOGSECTION LOGSECTION_IMAGE_INFO

TDumbDiskSwapper DumbDiskSwapper;


// the implementation honours the name

TDumbDiskSwapper::TDumbDiskSwapper() {
  num_images=image_index=0;
}


WORD TDumbDiskSwapper::Open(EasyStr path) {
  m3upath=path;
  num_images=image_index=0;
  while(GetPath(num_images)!="")
    num_images++;
  ejected=true;
  return num_images;
}


EasyStr TDumbDiskSwapper::GetPath(WORD index) {
  image_path="";
  FILE* fp=fopen(m3upath.Text,"r");
  if(fp)
  {
    char buffer[2048];
    for(int i=0;;i++)
    {
      if(fgets(buffer,sizeof(buffer),fp) != NULL)
      {
        // remove \n TODO there must be a better/safer way
        char *nl;
        nl=strrchr(buffer,'\n');
        if(nl!=NULL)
          *nl='\0';
        nl=strrchr(buffer,'\r');
        if(nl!=NULL)
          *nl='\0';
        if(buffer[0]!='#' && buffer[0]!='\0') // not comment/command, not empty line
        {
          if(i==index) // the one we want
          {
            if(strchr(buffer,':')) // full path in playlist
              image_path=buffer;
            else // make full path
            {
              image_path=m3upath;
              RemoveFileNameFromPath(image_path,KEEP_SLASH);
              image_path+=buffer;
              image_index=index;
            }
            TRACE_LOG2("DumbDiskSwapper %d %s\n",index,image_path.Text);
            break;
          }
        }//if(buffer[0]
        else
          i--; // count only probable file names (anything but # or empty line)
      }//if(fgets
      else
        break;
    }
  }
  fclose(fp);
  return image_path;
}

#endif//#if defined(SSE_DISK_M3U)


char *dot_ext(int i) { // is it ridiculous? do we reduce or add overhead?
  static char buffer[5]=".XXX";
  strcpy(buffer+1,extension_list[i]);
  return buffer;
}


TFloppyDisk::TFloppyDisk() {
#ifdef SSE_DEBUG
  current_byte=0xFFFF;
#endif
  Id=2; // temporary disk in temporary drive for properties
  Init();
  TrackBytes=DISK_BYTES_PER_TRACK_CST;
  fp=Format_fp=NULL;
  PastiDisk=false;
  PastiBuf=NULL;
}


void TFloppyDisk::Init() {
  TrackBytes=DISK_BYTES_PER_TRACK;
  StwPath.SetLength(SSE_MAX_PATH); // see GetTempFileName()
}


int TFloppyDisk::BytePositionOfFirstId() { 
  int first=PostIndexGap();
  switch(nSectors()) {
  case 11: case 22:
    first+=3+3+1;
    break;
  default:
    first+=12+3+1;
  }
  return first;
}


int TFloppyDisk::BytesToID(BYTE &num) {
/*  Compute distance in bytes between current byte and desired ID
    identified by 'num' (sector)
    return 0 if it doesn't exist
    if num=0, assume next ID, num will contain sector index, 1-based
*/
  int bytes_to_id=0;
  
  const WORD my_current_byte=FloppyDrive[Id].BytePosition();

  if(!FloppyDrive[Id].Empty())
  {
    //here we assume normal ST disk image, sectors are 1...10
    int record_length=RecordLength();
    int n_sectors=nSectors();
    int byte_first_id=BytePositionOfFirstId();
    int byte_target_id;
    // If we're looking for whatever next num, we compute it first
    if(!num)
    {
      num=(BYTE)((my_current_byte-byte_first_id)/record_length+1); // current, 1-based
      if(((my_current_byte)%record_length)>=byte_first_id) //v4: ProCopy Analyze
        num++; //next
      if(num>=n_sectors+1) // >=: Wipe-Out ; it's a special case, 6 sectors/track
        num=1; 
    }
    byte_target_id=byte_first_id+(num-1)*record_length;
    bytes_to_id=byte_target_id-my_current_byte;
    if(bytes_to_id<0) // passed it
      bytes_to_id+=TrackBytes; // next rev
  }
  //ASSERT(bytes_to_id>=0);
  return bytes_to_id;
}


int TFloppyDisk::HblsPerSector() {
  int ns=nSectors();
  ASSERTB(ns);
#ifdef SSE_LEAN_AND_MEAN
  return (FloppyDrive[Id].HblsPerRotation()-FloppyDrive[Id].BytesToHbls(TrackGap()))/ns;
#else
  return ns?(FloppyDrive[Id].HblsPerRotation()
    -FloppyDrive[Id].BytesToHbls(TrackGap()))/nSectors() : 1;
#endif
}


void TFloppyDisk::NextID(BYTE &RecordIdx,int &nHbls) {
  RecordIdx=0;
  nHbls=0;
  if(FloppyDrive[Id].Empty())
    return;
  int BytesToRun=BytesToID(RecordIdx); // 0 means first that comes
  if(RecordIdx) // RecordIdx is changed by BytesToID()
    RecordIdx--; // 0-basis
  nHbls=FloppyDrive[Id].BytesToHbls(BytesToRun);
}


int TFloppyDisk::nSectors() { 
  int nSects;
  if(STT_File)
  {
    TWD1772IDField IDList[30]; // much work each time, but STT rare
    nSects=GetIDFields(CURRENT_SIDE,CURRENT_TRACK,IDList);
  }
  else
    nSects=SectorsPerTrack;
  return nSects;
}


int TFloppyDisk::PostIndexGap() {
  switch(nSectors()) {
  case 10:
  case 20:
    return 22; //?
  case 11: case 22:
    return 10;
  default: //9-
    return 60;
  }
}


int TFloppyDisk::PreDataGap() {
  int gap=0;
  switch(nSectors()) {
    // with ID (7) and DAM (1)
  case 11: case 22:
    gap=3+3+7+22+12+3+1;
    break;
  default:
    gap=12+3+7+22+12+3+1; // with ID (7) and DAM (1)
  }
  return gap;
}


int TFloppyDisk::PostDataGap() {
  switch(nSectors()) {
  case 11:case 22:
    return 1;
  default:
    return 40;
  }
}


int TFloppyDisk::PreIndexGap() {
  int gap;
  switch(nSectors()) {
  case 10:
  case 20:
    gap=50+6+(60-22);
    break;
  case 11: case 22:
    gap=20;
    break;
  default:
    gap=664+6; // 6256 vs 6250
  }
  return gap;
}


int TFloppyDisk::RecordLength() {
  switch(nSectors()) {
    case 11: case 22:
      return 566;
    default:
      return 614;
  }
}


int TFloppyDisk::SectorGap() {
  return 2+PostDataGap()+PreDataGap();
}


int TFloppyDisk::TrackGap() {
  return PostIndexGap()+PreIndexGap();
}


int TFloppyDisk::GetIDFields(int Side,int Track,TWD1772IDField *IDList) {
  if(FloppyDrive[Id].Empty()) 
    return 0;
  if(STT_File)
  {
    DWORD TrackStart=STT_TrackStart[Side][Track],Magic=0;
    WORD DataFlags;
    if(TrackStart==0)
      return 0;
    FSEEK(fp,TrackStart,SEEK_SET);
    if(FREAD(&Magic,4,1,fp)==0)
    {
      if(!FloppyDrive[Id].ReinsertDisk())
        return 0;
      if((TrackStart=STT_TrackStart[Side][Track])==0)
        return 0;
      FSEEK(fp,TrackStart,SEEK_SET);
      FREAD(&Magic,4,1,fp);
    }
    if(Magic!=MAKECHARCONST('T','R','C','K'))
      return 0;
    FREAD(&DataFlags,2,1,fp);
    if(DataFlags & BIT_0)
    {       //Sectors
      DWORD Dummy;
      WORD Offset,Flags,NumSectors;
      FREAD(&Offset,2,1,fp);
      FREAD(&Flags,2,1,fp);
      FREAD(&NumSectors,2,1,fp);
      for(WORD n=0;n<NumSectors;n++)
      {
        FREAD(&IDList[n].track,1,1,fp);
        FREAD(&IDList[n].side,1,1,fp);
        FREAD(&IDList[n].num,1,1,fp);
        FREAD(&IDList[n].len,1,1,fp);
        FREAD(&IDList[n].CRC[0],1,1,fp);
        FREAD(&IDList[n].CRC[1],1,1,fp);
        FREAD(&Dummy,4,1,fp); // SectorOffset, SectorLen
      }
      return NumSectors;
    }
    else if(DataFlags & BIT_1)
    { //Raw track
    }
    return 0;
  }
  else
  {
    bool Format=false;
    if(Track<=FLOPPY_MAX_TRACK_NUM)
      Format=TrackIsFormatted[Side][Track];
    if(Side>=int(Format?2:Sides))
      return 0;
    else if(Track>=(int)(Format?FLOPPY_MAX_TRACK_NUM+1:TracksPerSide))
      return 0;
    for(int n=0;n<(int)(Format?FormatMostSectors:SectorsPerTrack);n++)
    {
#if defined(SSE_DISK_STX)
      if(STX_File && ImageSTX[Id].pSectorDesc && n<ImageSTX[Id].pTrackDesc->sectorCount)
        memcpy(&IDList[n],&ImageSTX[Id].pSectorDesc[n].id,sizeof(TWD1772IDField));
      else
#endif
      {
        IDList[n].track=(BYTE)Track;
        IDList[n].side=(BYTE)Side;
        // fake interleave '6' for 11 sectors: 1 7 2 8 3 9 4 10 5 11 6
        if(ADAT&&(SectorsPerTrack%11)==0) // not >= (superdisks)
          IDList[n].num=(BYTE)(1+(n*DISK_11SEC_INTERLEAVE)%SectorsPerTrack);
        else
          IDList[n].num=(BYTE)(1+n);
        IDList[n].len=(BYTE)IDList[n].GetLen(BytesPerSector);
        WORD CRC=0xffff;
        fdc_add_to_crc(CRC,0xa1);
        fdc_add_to_crc(CRC,0xa1);
        fdc_add_to_crc(CRC,0xa1);
        fdc_add_to_crc(CRC,0xfe);
        fdc_add_to_crc(CRC,IDList[n].track);
        fdc_add_to_crc(CRC,IDList[n].side);
        fdc_add_to_crc(CRC,IDList[n].num);
        fdc_add_to_crc(CRC,IDList[n].len);
        IDList[n].CRC[0]=HIBYTE(CRC);
        IDList[n].CRC[1]=LOBYTE(CRC);
      }
    }
    return (Format) ? FormatMostSectors : SectorsPerTrack;
  }
}


bool TFloppyDisk::OpenFormatFile() {
  if(FloppyDrive[Id].Empty()||fp==NULL||WriteProtect||Format_fp
    ||STT_File||FloppyDrive[Id].ImageType.Manager!=MNGR_STEEM)
    return false;
  // The format file is just a max size ST file, any formatted tracks
  // go in here and then are merged with unformatted tracks when
  // the disk is removed from the drive
  FormatTempFile.SetLength(SSE_MAX_PATH);
  GetTempFileName(TempPath,"FMT",0,FormatTempFile);
  // Create it
  Format_fp=fopen(FormatTempFile,"wb");
  if(Format_fp==NULL) 
    return false;
  fclose(Format_fp);
#if !defined(SSE_WRITEDIR)
  SetFileAttributes(FormatTempFile,FILE_ATTRIBUTE_HIDDEN);
#endif
  Format_fp=fopen(FormatTempFile,"r+b");
  if(Format_fp==NULL) 
    return false;
  char zeros[SECTOR_SIZE];
  ZeroMemory(zeros,sizeof(zeros));
  for(int Side=0;Side<2;Side++)
  {
    for(int Track=0;Track<=FLOPPY_MAX_TRACK_NUM;Track++)
    {
      for(int Sector=1;Sector<=FLOPPY_MAX_SECTOR_NUM;Sector++)
        FWRITE(zeros,SECTOR_SIZE,1,Format_fp);
    }
  }
  FFLUSH(Format_fp);
  return true;
}


bool TFloppyDisk::ReopenFormatFile() {
  if(FloppyDrive[Id].Empty()||fp==NULL||ReadOnly||Format_fp==NULL
    ||STT_File||FloppyDrive[Id].ImageType.Manager!=MNGR_STEEM)
    return false;
  fclose(Format_fp);
  Format_fp=fopen(FormatTempFile,"r+b");
  if(Format_fp) 
    return true;
  return false;
}


// Seek in the disk image to the start of the required sector

bool TFloppyDisk::SeekSector(SHORT Side,SHORT Track,SHORT Sector,bool bFormat,
                             bool CheckFreeboot/*=true*/) {
  if(Format_fp==NULL) 
    bFormat=false;

#if defined(SSE_DRIVE_FREEBOOT)
  if(CheckFreeboot) // during emulation, not when manipulating disk images
  {
    if(FloppyDrive[Id].bSingleSided&&Side==1)
      return false; // -> RNF
    if(Id==DRIVE_A && FloppyDrive[Id].bFreeboot)
      Side=1;
  }
#endif

  if(FloppyDrive[Id].Empty())
    return false;
  else if(Side<0||Track<0||Side>1)
  {
    return false;
  }
  else if(Side>=(bFormat ? 2 : Sides))
  {
    return false;
  }
  else if(Track>=(SHORT)(bFormat?FLOPPY_MAX_TRACK_NUM+1:TracksPerSide))
  {
    return false;
  }
#if defined(SSE_MEGASTE)
  else if(Density==2 && !OPTION_HACKS && !IS_MEGASTE)
    return false; // -> RNF
#endif


  if(STT_File)
  {
    DWORD TrackStart=STT_TrackStart[Side][Track],Magic=0;
    WORD DataFlags;
    if(TrackStart==0)
      return true; // Track doesn't exist
    FSEEK(fp,TrackStart,SEEK_SET);
    if(FREAD(&Magic,4,1,fp)==0)
    {
      if(!FloppyDrive[Id].ReinsertDisk()) 
        return false;
      if((TrackStart=STT_TrackStart[Side][Track])==0)
        return false;
      FSEEK(fp,TrackStart,SEEK_SET);
      FREAD(&Magic,4,1,fp);
    }
    if(Magic!=MAKECHARCONST('T','R','C','K'))
      return false;
    FREAD(&DataFlags,2,1,fp);
    bool Failed=true;
    if(DataFlags & BIT_0)
    {       //Sectors
      WORD Offset,Flags,NumSectors;
      FREAD(&Offset,2,1,fp);
      FREAD(&Flags,2,1,fp);
      FREAD(&NumSectors,2,1,fp);
      BYTE TrackNum,SideNum,SectorNum,LenIdx,CRC1,CRC2;
      WORD SectorOffset,SectorLen;
      for(WORD n=0;n<NumSectors;n++)
      {
        FREAD(&TrackNum,1,1,fp);
        FREAD(&SideNum,1,1,fp);
        FREAD(&SectorNum,1,1,fp);
        FREAD(&LenIdx,1,1,fp);
        FREAD(&CRC1,1,1,fp);
        FREAD(&CRC2,1,1,fp);
        FREAD(&SectorOffset,2,1,fp);
        FREAD(&SectorLen,2,1,fp);
        // I'm not sure but it is very possible changing sides during a disk operation
        // would cause it to immediately start reading the other side
        //SS: we don't do that for SCP etc. it would return garbage
        if(TrackNum==Track && SideNum==Side &&SectorNum==Sector && SectorLen!=0)
        {
          FSEEK(fp,TrackStart+SectorOffset,SEEK_SET);
          BytesPerSector=SectorLen;
          Failed=false;
          break;
        }
      }
    }
    else if(DataFlags & BIT_1)
    { // Raw track data (with bad syncs)
    }
    return !Failed;
  }
#if defined(SSE_DISK_STX)
  else if(STX_File)
    return true;
#endif
  else
  {
    if(Sector==0 || Sector>((bFormat) ? FLOPPY_MAX_SECTOR_NUM : SectorsPerTrack))
    {
      return false;
    }
    if(!bFormat)
    {
      int HeaderLen=(DIM_File) ? 32 : 0;
      FSEEK(fp,HeaderLen+(GetLogicalSector(Side,Track,Sector)*BytesPerSector),SEEK_SET);
    }
    else
      FSEEK(Format_fp,GetLogicalSector(Side,Track,Sector,true)*SECTOR_SIZE,SEEK_SET);
    return true;  //no error!
  }
}


LONG TFloppyDisk::GetLogicalSector(SHORT Side,SHORT Track,SHORT Sector,bool FormatFile) {
  if(FloppyDrive[Id].Empty()) 
    return 0;
  if(!FormatFile||Format_fp==NULL)
    return ((Track*Sides*SectorsPerTrack)+(Side*SectorsPerTrack)+(Sector-1));
  return (Track*2*FLOPPY_MAX_SECTOR_NUM)+(Side*FLOPPY_MAX_SECTOR_NUM)+(Sector-1);
}


int TFloppyDisk::GetRawTrackData(SHORT Side,SHORT Track) {
  if(STT_File)
  {
    DWORD TrackStart=STT_TrackStart[Side][Track],Magic;
    WORD DataFlags;
    if(TrackStart==0)
      return 0;
    FSEEK(fp,TrackStart,SEEK_SET);
    if(FREAD(&Magic,4,1,fp)==0)
    {
      if(!FloppyDrive[Id].ReinsertDisk())
        return 0;
      if((TrackStart=STT_TrackStart[Side][Track])==0)
        return 0;
      FSEEK(fp,TrackStart,SEEK_SET);
      FREAD(&Magic,4,1,fp);
    }
    if(Magic!=MAKECHARCONST('T','R','C','K'))
      return 0;
    FREAD(&DataFlags,2,1,fp);
    if(DataFlags & BIT_0)
    { // Skip this section if it exists
      WORD Offset;
      FREAD(&Offset,2,1,fp);
      FSEEK(fp,TrackStart+Offset,SEEK_SET);
    }
    if(DataFlags & BIT_1)
    { //Raw
      WORD Offset,Flags,TrackDataOffset,TrackDataLen;
      FREAD(&Offset,2,1,fp);
      FREAD(&Flags,2,1,fp);
      FREAD(&TrackDataOffset,2,1,fp);
      FREAD(&TrackDataLen,2,1,fp);
      FSEEK(fp,TrackStart+TrackDataOffset,SEEK_SET);
      return TrackDataLen;
    }
  }
#if defined(SSE_DISK_STX)
  else if(STX_File && ImageSTX[Id].pTrackBytes)
    return TrackBytes;
#endif
  return 0;
}


//////////////////////////////
// MFM MANAGERS: base class //
//////////////////////////////

#undef LOGSECTION
#define LOGSECTION LOGSECTION_FDC

void TImageMfm::ComputePosition(WORD position) {
  // when we start reading/writing, where on the disk?
  //ASSERT(FloppyDisk[Id].TrackBytes); 
  ASSERTB(!IMAGE_SCP);
  ASSERTB(FloppyDisk[Id].TrackBytes);
#ifndef SSE_LEAN_AND_MEAN
  if(FloppyDisk[Id].TrackBytes) // good old div /0 crashes the PC like it did the ST
#endif
  position=position%FloppyDisk[Id].TrackBytes; // 0-~6256, safety
  //TRACE_LOG("old position %d new position %d\n",Position,position);
  Position=FloppyDisk[Id].current_byte=position;
  BitPosition=Position*16;
}


WORD TImageMfm::GetMfmData(WORD position) {
  WORD mfm_data=0xFFFF;
  // must compute new starting point?
  if(position!=mfm_data) //dubious optimisation, it's 0xFFFF
  {
    ComputePosition(position);
    BitPosition=Position*16;
    if(SSEOptions.RandomizeTrack)
      BitPosition+=(rand()%3); // War Heli
#if defined(SSE_WD1772_LL)
    Fdc.Dpll.Reset(A_S_T);
#endif
  }
#if defined(SSE_WD1772_LL)
  if(LowLevel)
  {
    // we manage timing here, maybe we should do that in Fdc instead
    COUNTER_VAR a1=Fdc.Dpll.ctime,a2,tm=0;
    // clear dsr signals
    Fdc.Amd.aminfo&=~(CAPSFDC_AI_DSRREADY|CAPSFDC_AI_DSRAM|CAPSFDC_AI_DSRMA1);
    // loop until break
    for(int i=0; ;i++)
    {
       int bit=Fdc.Dpll.GetNextBit(tm,Id); //tm isn't used...
      //ASSERT(bit==0 || bit==1); // 0 or 1, clock and data
      TRACE_MFM("%d",bit); // full flow of bits
      if(Fdc.ShiftBit(bit)) // true if byte ready to transfer
        break;
    }//nxt i
    //Fdc.Mfm.data_last_bit=(mfm_data&1); // no use
    a2=Fdc.Dpll.ctime;
    COUNTER_VAR delay_in_cycles=(a2-a1);
#ifdef SSE_DEBUG  // only report DPLL if there's some adjustment
    if(Fdc.Dpll.increment!=128|| Fdc.Dpll.phase_add||Fdc.Dpll.phase_sub
      ||Fdc.Dpll.freq_add||Fdc.Dpll.freq_sub)
    {
      //ASSERT( !(Fdc.Dpll.freq_add && Fdc.Dpll.freq_sub) ); 
      //ASSERT( !(Fdc.Dpll.phase_add && Fdc.Dpll.phase_sub) );
      TRACE_MFM(" DPLL (%d,%d,%d) ",Fdc.Dpll.increment,Fdc.Dpll.phase_add-
        Fdc.Dpll.phase_sub,Fdc.Dpll.freq_add-Fdc.Dpll.freq_sub);
    }
    //ASSERT(delay_in_cycles>0);
    TRACE_MFM(" %d cycles\n",delay_in_cycles);
#endif
    if(delay_in_cycles>200)
      DiskEmu.BitRate=(WORD)(512-delay_in_cycles)*2;
    Fdc.update_time=Fdc.current_time+delay_in_cycles*TICKS8;
    if(Fdc.update_time-A_S_T<=0) // safety
      Fdc.update_time=A_S_T+16*TICKS8;
    //ASSERT(!mfm_data); // see note at top of function
    mfm_data=Fdc.Mfm.encoded; // correct?
  }//ll
  else
#endif//#if defined(SSE_DISK_STW2)
  {
    mfm_data=Fdc.Mfm.encoded=GetImageWord(Position);
  }
  return mfm_data;
}


int TImageMfm::GetNextTransition(WORD&) { // for compilation only
  ASSERTB(0);
  return 0;
}


void TImageMfm::IncPosition() { // next MFM word
  ASSERTB(FloppyDisk[Id].TrackBytes);
#ifndef SSE_LEAN_AND_MEAN
  if(FloppyDisk[Id].TrackBytes)
#endif
  Position=(Position+1)%FloppyDisk[Id].TrackBytes;
  BitPosition=Position*16;
  if(!Position)
  {
    if(!Fdc.WaitImage)
      Fdc.WaitIP=true;
    Fdc.WaitImage=false;
  }
}


void TImageMfm::SetMfmData(WORD position,WORD mfm_data) {
  // if disk is read-only, we still write but changes will be lost
  if(!FloppyDisk[Id].ReadOnly && !DiskMan.bDiskProtectImage)
    FloppyDisk[Id].WrittenTo=true;
  // must compute new starting point?
  if(position!=0xFFFF)
  {
    ComputePosition(position);
    BitPosition=Position*16;
#if defined(SSE_WD1772_LL)
    Fdc.Dpll.Reset(A_S_T);
#endif
  }
/*  if we start writing at an arbitrary bit, we must get the current mfm word, 
    update the bit and write the updated word back on the image, bit by bit
    with current STW and HFE images, there's no shift anyway */
  if(LowLevel)
  {
    for(int i=15;i>=0;i--)
    {
      WORD current_word=GetImageWord(Position);
      DWORD bitn=15-(BitPosition%16); // highest bit first
      if( (mfm_data>>i)&1 )
        current_word|=1<<bitn; // set
      else
        current_word&=~(1<<bitn); // clear
      SetImageWord(Position,current_word);
      IncBitPosition(); // could change current word (doesn't on current images)
    }
  }
}

#undef LOGSECTION
