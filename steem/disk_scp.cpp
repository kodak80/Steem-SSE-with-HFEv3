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
FILE: disk_scp.cpp
CONDITION: SSE_DISK_SCP must be defined
DESCRIPTION: SCP disk images are produced with Supercard Pro hardware, 
they're at bit level.
TODO: weak bits not always correctly read
TODO: 8bit images not tested
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#if defined(SSE_DISK_SCP)

#include <debug.h>
#include <computer.h>
#include <iolist.h>


#define N_SIDES FloppyDisk[Id].Sides
#define N_TRACKS FloppyDisk[Id].TracksPerSide
#define LOGSECTION LOGSECTION_IMAGE_INFO


TImageSCP::TImageSCP() {
  Init();
}


TImageSCP::~TImageSCP() {
  Close();
}


void TImageSCP::Close() {
  if(fCurrentImage)
  {
    TRACE_LOG("SCP %d close image\n",Id);
    fclose(fCurrentImage);
    if(TimeFromIndexPulse)
      free(TimeFromIndexPulse);
  }
  Init();
}


void TImageSCP::ComputePosition(WORD) {
  // we don't use the parameter, we need it for the declaration as virtual
  // when we start reading/writing, where on the disk?
  //ASSERT(TimeFromIndexPulse);
  if(!TimeFromIndexPulse)
    return; //safety
  int cycles_since_ip=(int)(Fdc.current_time-FloppyDrive[Id].time_of_last_ip);
  DWORD units=cycles_since_ip*5/TICKS8; // in SCP units
  BitPosition=0;
  Position=0;
  for(DWORD i=0;i<nBits;i++) // slow search
  {
    if(TimeFromIndexPulse[i]>=units)
    {
      BitPosition=i; // can be 0
      ASSERTB(((BitPosition/16)&0xFFFF0000)==0);
      Position=(WORD)(BitPosition/16);
      break;
    }
  }
  Fdc.Dpll.Reset(A_S_T); 
  //ASSERT(FloppyDrive[DRIVE].CyclesPerByte());
  // just informative? TODO
  int cpb=FloppyDrive[Id].CyclesPerByte();
  ASSERTB(cpb);
#ifndef SSE_LEAN_AND_MEAN
  if(cpb)
#endif
  FloppyDisk[Id].current_byte=(WORD)((Fdc.current_time-FloppyDrive[Id].time_of_last_ip)/cpb);
  TRACE_LOG2("SCP Position %d\n",BitPosition);
}


int TImageSCP::UnitsToNextFlux(DWORD bit_position) {
  // 1 unit = 25 nanoseconds = 1/40 ms
  ASSERTB(bit_position<nBits);
#ifndef SSE_LEAN_AND_MEAN
  bit_position=bit_position%nBits; // safety
#endif
  DWORD time1=0,time2;
  if(bit_position)
    time1=TimeFromIndexPulse[bit_position-1];
  time2=TimeFromIndexPulse[bit_position];
  //ASSERT(time2>time1||!time1);
  int units_to_next_flux=time2-time1;
  // this takes care of weak bits (?)
#if defined(SSE_GUI_EMUCONTROL)
  if(SSEOptions.FuzzyBits)
#endif
  if(Wobble)
    units_to_next_flux+=(rand()%Wobble)-Wobble/2;
  return units_to_next_flux;
}


WORD TImageSCP::UsToNextFlux(int units_to_next_flux) {
  WORD us_to_next_flux;
  WORD ref_us=((units_to_next_flux/40)+1)&0xFE;  // eg 4
  WORD ref_units=ref_us*40;
  if(units_to_next_flux<ref_units-SCP_DATA_WINDOW_TOLERANCY)
    us_to_next_flux=ref_us-1;
  else if(units_to_next_flux>ref_units+SCP_DATA_WINDOW_TOLERANCY)
    us_to_next_flux=ref_us+1;
  else
    us_to_next_flux=ref_us;
  return us_to_next_flux;
}


WORD TImageSCP::GetImageWord(WORD position) { // for compilation only
  return position;
}


WORD TImageSCP::GetMfmData(WORD position) {
/*  We use the same interface for SCP as for STW so that integration
    with the Disk manager, WD1772 emu etc. is straightforward.
    But precise emulation doesn't send MFM data word by word (16bit).
    Instead it sends bytes and AM signals according to bit sequences,
    as analysed in (3rd party-inspired) Fdc.ShiftBit().
    
    Drive.Read() -> SCP.GetMfmData() -> Fdc.Dpll.GetNextBit() 
    -> SCP.GetNextTransition()
*/
  WORD mfm_data=TImageMfm::GetMfmData(position);
  return mfm_data;
}


int TImageSCP::GetNextTransition(WORD& us_to_next_flux) {
  int t=UnitsToNextFlux(BitPosition);
  us_to_next_flux=UsToNextFlux(t); // in parameter for debug info
  IncBitPosition();
  t/=5; // in cycles
  units_rest+=t%5;
  if(units_rest>5)
  {
    t++;
    units_rest-=5;
  }
  return t; 
}


void TImageSCP::IncBitPosition() {
  //ASSERT( BitPosition>=0 );
  //ASSERT( BitPosition<nBits );
  BitPosition++;
  if(BitPosition>=nBits)
  {
    TRACE_LOG2("\nSCP BitPosition %d triggers reload side %d track %d rev %d/%d\n",BitPosition,FloppyDisk[Id].current_side,FloppyDisk[Id].current_track,rev+1,file_header.IFF_NUMREVS);
    BitPosition=0;
/*  If a sector is spread over IP, we make sure that our event
    system won't start a new byte before returning to current
    byte. 
*/
    if(!Fdc.WaitImage)
      Fdc.WaitIP=true;
    Fdc.WaitImage=false;
    // provided there are >1 revs...    
    if(file_header.IFF_NUMREVS>1)
    {
      // Notice we do no computing, the first bit of the new rev
      // is relative to last bit of previous rev, or we are very
      // lucky.
      LoadTrack(FloppyDisk[Id].current_side,FloppyDisk[Id].current_track,true);
    }      
  }
}


void TImageSCP::Init() {
  fCurrentImage=NULL;
  TimeFromIndexPulse=NULL;
  N_SIDES=2;
  N_TRACKS=83; //max
  nBytes=DISK_BYTES_PER_TRACK; //not really pertinent (TODO?)
  Wobble=5;
}


bool TImageSCP::LoadTrack(BYTE side,BYTE track,bool reload) {
  bool ok=false;
  //ASSERT( side<2 && track<N_TRACKS ); // unique side may be 1
  if(side>=2 || track>=N_TRACKS)
    return ok; //no crash
  BYTE trackn=track;
  if(N_SIDES==2) // general case
    trackn=track*2+side; 
  if(track_header.TDH_TRACKNUM==trackn //already loaded
    && !rev && (!reload||file_header.IFF_NUMREVS==1))
    return true;
  if(!reload)
    units_rest=0;
  if(TimeFromIndexPulse) 
    free(TimeFromIndexPulse);
  TimeFromIndexPulse=NULL;
  int offset=file_header.IFF_THDOFFSET[trackn]; // base = start of file
  if(fCurrentImage) // image exists
  {  
    FSEEK(fCurrentImage,offset,SEEK_SET);
    int size=sizeof(TSCP_track_header);
    FREAD(&track_header,size,1,fCurrentImage);
    // Determine which track rev to load (we go through all available revs)
    if(reload)
      rev++;
    else
      rev=0; 
    rev%=file_header.IFF_NUMREVS;
    DWORD &track_len=track_header.TDH_TABLESTART[rev].TDH_LENGTH; // shorthand
    if(track_len>MEM_1MB)
      return false;
    WORD* flux_to_flux_units_table_16bit=(WORD*)calloc(track_len,sizeof(WORD));
    TimeFromIndexPulse=(DWORD*)calloc(track_len,sizeof(DWORD));
    //ASSERT(flux_to_flux_units_table_16bit && TimeFromIndexPulse);
    if(flux_to_flux_units_table_16bit && TimeFromIndexPulse)
    {
      FSEEK(fCurrentImage,offset+track_header.TDH_TABLESTART[rev].TDH_OFFSET,SEEK_SET);
      // read only a 8bit table if specified
      size_t encoding_bytes = (file_header.IFF_ENCODING == 8) ? 1 : 2;
      FREAD(flux_to_flux_units_table_16bit,encoding_bytes,track_len,fCurrentImage);
      // randomise distance from IP of whole track (War Heli)
      // int units_from_ip=reload?0:(rand()%0xb0); // too much? (Audio Sculpture)
      //int units_from_ip=reload?0:(rand()%0x20);
      int units_from_ip=(reload||!SSEOptions.RandomizeTrack)?0:(rand()%0x16); // TODO
      //ASSERT(!units_from_ip);
      nBits=0;
      // convert to time after IP, one data per bit (SLOW)
      WORD data=0;
      // ASSERT(!(side==0&&track==0&&i==track_header.TDH_TABLESTART[rev].TDH_LENGTH-1));// last data is 0!
      // probably doesn't work but won't break normal images
      if(file_header.IFF_RESOLUTION)
      {
        for(DWORD i=0;i<track_len;i++)
        {
          data=flux_to_flux_units_table_16bit[i]*(file_header.IFF_RESOLUTION+1);
          //ASSERT(units_from_ip+data>=units_from_ip);
          units_from_ip+=(data)?data:0xFFFF;
          //ASSERT(units_from_ip<0x7FFFFFFF); // max +- 200,000,000, OK
          if(data)
            TimeFromIndexPulse[nBits++]=units_from_ip;
        }
      }
      else
      {
        for(DWORD i=0;i<track_len;i++)
        {
          data=flux_to_flux_units_table_16bit[i];
          SWAP_BIG_ENDIAN_WORD(data); // reverse endianess first
          //ASSERT(units_from_ip + flux_to_flux_units_table_16bit[i] >= units_from_ip);
          units_from_ip+=(data) ? data : 0xFFFF;
          //ASSERT(units_from_ip < 0x7FFFFFFF); // max +- 200,000,000, OK
          if(data)
            TimeFromIndexPulse[nBits++] = units_from_ip;
        }
      }
      // check if we end on a 0 data (means we need info from next track!)
      // eg finale-overlander_rev5_smd340
      if(!data) 
      {
        BYTE nextrev=(rev+1)%file_header.IFF_NUMREVS;
        FSEEK(fCurrentImage,offset+track_header.TDH_TABLESTART[nextrev].TDH_OFFSET,SEEK_SET);
        // we don't read more than what we can take, should be enough!
        FREAD(flux_to_flux_units_table_16bit,encoding_bytes,track_len,fCurrentImage);
        {
          for(DWORD i=0;!data&&i<track_len;i++) // exit as soon as we have the transition
          {
            data=flux_to_flux_units_table_16bit[i];
            if(file_header.IFF_RESOLUTION)
              data*=(file_header.IFF_RESOLUTION+1);
            else
            {
              SWAP_BIG_ENDIAN_WORD(data);
            }
            units_from_ip+=(data) ? data : 0xFFFF;
            if(data)
              TimeFromIndexPulse[nBits++] = units_from_ip;
          }
        }
        TRACE_LOG("SCP %d-%d last flux %d\n",side,track,TimeFromIndexPulse[nBits-1]);
      }
      free(flux_to_flux_units_table_16bit);
      ok=true;
    }
    FloppyDisk[Id].current_side=side;
    FloppyDisk[Id].current_track=track;
    // debug info!
    TRACE_LOG("SCP LoadTrack side %d track %d %c%c%c %d rev %d/%d INDEX TIME %d (%f ms)\n",
      side,track,track_header.TDH_ID[0],track_header.TDH_ID[1],track_header.TDH_ID[2],track_header.TDH_TRACKNUM,rev+1,file_header.IFF_NUMREVS,track_header.TDH_TABLESTART[rev].TDH_DURATION,(float)track_header.TDH_TABLESTART[rev].TDH_DURATION*25/1000000);
    TRACE_LOG("TRACK LENGTH %d bits %d last bit unit %d DATA OFFSET %d  checksum %X\n",
      track_header.TDH_TABLESTART[rev].TDH_LENGTH, nBits,TimeFromIndexPulse[nBits-1],track_header.TDH_TABLESTART[rev].TDH_OFFSET,track_header.track_data_checksum);
  }
  return ok;
}


bool TImageSCP::Open(char *path) {
  bool ok=false;
  //if(!SSEConfig.IsInit)    return ok;
  Close(); // make sure previous image is correctly closed
  fCurrentImage=fopen(path,"rb");
  if(fCurrentImage) // image exists
  {
    // we read only the header
    if(FREAD(&file_header,sizeof(TSCP_file_header),1,fCurrentImage))
    {
      if(!strncmp(DISK_EXT_SCP,(char*)&file_header.IFF_ID,3)) // it's SCP
      {
        // compute N_SIDES and N_TRACKS
        if(file_header.IFF_HEADS)
        {
          N_SIDES=1;
          N_TRACKS=file_header.IFF_END-file_header.IFF_START+1;
        }
        else
          N_TRACKS=(file_header.IFF_END-file_header.IFF_START+1)/2;
#if defined(SSE_ENABLE_TRACE_LOG)
          TRACE_LOG("SCP %c sides %d tracks %d IFF_VER %X IFF_DISKTYPE %X IFF_NUMREVS %d IFF_START %d IFF_END %d IFF_FLAGS $%X IFF_ENCODING %d IFF_HEADS %d IFF_RESOLUTION %X IFF_CHECKSUM %X\n",
            'A'+Id,N_SIDES,N_TRACKS,file_header.IFF_VER,file_header.IFF_DISKTYPE,file_header.IFF_NUMREVS,file_header.IFF_START,file_header.IFF_END,file_header.IFF_FLAGS,file_header.IFF_ENCODING,file_header.IFF_HEADS,file_header.IFF_RESOLUTION,file_header.IFF_CHECKSUM);
          ASSERTB(!(file_header.IFF_FLAGS&BIT_4)); // detect read-write image
#endif
        track_header.TDH_TRACKNUM=0xFF;
        ok=true; //TODO some checks?
        LowLevel=true;
      }//cmp
    }//read
  }
  if(!ok)
    Close();
  else 
    FloppyDrive[Id].MfmManager=this;
  return ok;
}


void TImageSCP::SetImageWord(WORD /*position*/,WORD /*mfm_data*/) { // for compilation only
}


void TImageSCP::SetMfmData(WORD /*position*/, WORD /*mfm_data*/) {
/*  The read/write capable images contain padded space to allow the track to
    change size within the image.  Only a single revolution is allowed when
    the TYPE bit is set (read/write capable).  */
  if((file_header.IFF_FLAGS&BIT_4) && file_header.IFF_NUMREVS==1)
  {
    // :) TODO, maybe first create a writable image - notice that SCP can be converted to STW v2
  }
}


#undef LOGSECTION
#undef N_SIDES
#undef N_TRACKS

#endif//#if defined(SSE_DISK_SCP)
