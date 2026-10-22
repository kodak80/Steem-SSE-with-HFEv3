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

DOMAIN: Emu, Disk image
FILE: floppy_drive.cpp
DESCRIPTION: Floppy drive object. 
The object handles loading and removing/saving a disk image.
When a disk is spinning, we get an index pulse (IP) on each rotation, which
is sent to the fdc.
The fdc and the psg command the drive, to start or stop the motor, step, 
read, or write. 
The image type manager and type are stored in this object. When there's no
disk, the manager is MNGR_STEEM or MNGR_WD1772 according to option MFM.
Steem's drive sound is handled here.
Some TSF314 functions are defined in diskman.cpp.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <computer.h>
#include <diskman.h>
#include <osd.h>
#include <iolist.h>


#if defined(SSE_DRIVE_SOUND)
// todo move particulars of sound libraries


#include "various/sound.h" //struct TWavFileFormat

enum EDriveSound {START,MOTOR,STEP,SEEK,NSOUNDS};

DWORD SeekFreq[2][NSOUNDS];

#ifdef WIN32
IDirectSoundBuffer *SoundBuffer[2][NSOUNDS];
#endif

#ifdef UNIX
BYTE *SoundBuffer[2][NSOUNDS];
#ifndef NO_PORTAUDIO
PaStream *paDiskSoundStream[2][NSOUNDS];
#endif
#if defined(SSE_UNIX_PULSEAUDIO)
pa_stream *pulseDiskStream[2][NSOUNDS];
#endif
int SoundIndex[2][NSOUNDS];
int SampleLength[2][NSOUNDS];

bool SoundOn[2][NSOUNDS];
bool bLoop[2][NSOUNDS];
DWORD LoopStart[2][NSOUNDS];
DWORD LoopEnd[2][NSOUNDS];
#endif//unix

#endif//sound

#define LOGSECTION LOGSECTION_FDC


TSF314::TSF314() {
  ZeroMemory(this,sizeof(TSF314));
  Id=2;
  Init();
}


#if defined(SSE_DISK_GHOST)

bool TSF314::CheckGhostDisk(bool const bWrite) {
  ASSERTB(Id<2);
  ASSERTB(OPTION_GHOST_DISK);
  //ASSERT(!bGhost);
  if(!bGhost) // need to open ghost image?
  {
    EasyStr STGPath=FloppyDisk[Id].GetImageFile();
    STGPath+=dot_ext(EXT_STG); 
    if(bWrite || Exists(STGPath))
    {
      if(GhostDisk[Id].Open(STGPath.Text))
        bGhost=true; 
    }
  }
  return bGhost;
}

#endif


void TSF314::Init() {
#if defined(SSE_DRIVE_SOUND)
  for(int i=0;i<NSOUNDS&&Id<2;i++)
    SoundBuffer[Id][i]=NULL;
  SoundVolume=-1300; // 0db = max!
#endif
  SectorChecksum=0;
  bDiskInDrive=false;
  if(pasti_active)
    ImageType.Manager=MNGR_PASTI;
  else
    ImageType.Manager=(BYTE)((OPTION_AUTOSTW)?MNGR_WD1772:MNGR_STEEM);
  MfmManager=NULL;
}


void TSF314::Restore(BYTE myid) {
  //ASSERT(!(ImageType.RealExtension>=NUM_EXT));
  Id=myid;
  if(Empty())
  {
    //TRACE_LOG("drive %c empty\n",'A'+Id);
    ImageType.Extension=ImageType.RealExtension=EXT_NONE;
    if(pasti_active)
      ImageType.Manager=MNGR_PASTI;
    else
      ImageType.Manager=(BYTE)((OPTION_AUTOSTW)?MNGR_WD1772:MNGR_STEEM);
  }
  if(ImageType.Extension>=NUM_EXT)
    ImageType.Extension=EXT_NONE;
  if(ImageType.Manager==MNGR_WD1772)
  {
    switch(ImageType.Extension) {
    default: // no disk
#if defined(SSE_DISK_STW)
    case EXT_STW:
      MfmManager=&ImageSTW[Id];
      break;
#endif
#if defined(SSE_DISK_SCP)
    case EXT_SCP:
      MfmManager=&ImageSCP[Id];
      break;
#endif
#if defined(SSE_DISK_HFE)
    case EXT_HFE:
      MfmManager=&ImageHFE[Id];
      break;
#endif
    }
  }
  if(ImageType.Extension)
    bDiskInDrive=true;
  if(track>FLOPPY_MAX_TRACK_NUM)
    track=0;
  UpdateAdat(false);
}


void TSF314::UpdateAdat(bool const bRefreshGUI/*=true*/) {
/*  ADAT=accurate disk access times
    This is defined so: Steem slow (original ADAT) or STX or Caps (IPF,CTR)
    or SCP.
*/
  bAdat= (!DiskMan.bTurboDrive && ImageType.Manager==MNGR_STEEM
    || ImageType.Extension==EXT_STX || ImageType.Manager==MNGR_CAPS
    || ImageType.Manager==MNGR_WD1772 &&(!DiskMan.bTurboDrive || ImageType.Extension==EXT_SCP));
  if(bRefreshGUI)
  {
#ifdef WIN32
    if(DiskMan.IsVisible()) // drive icon with or without flash
    {
      InvalidateRect(GetDlgItem(DiskMan.Handle,IDC_DRIVEA),NULL,FALSE);
      InvalidateRect(GetDlgItem(DiskMan.Handle,IDC_DRIVEB),NULL,FALSE);
    }
#endif
  }
}


WORD TSF314::BytePosition() { //TODO
  WORD position=0;
#ifdef SSE_DISK_STW
/*  This assumes constant bytes/track (some protected disks have more)
    This should be 0-6255
    This is independent of #sectors
    This is independent of disk type
    This is based on Index Pulse, that is sent by the drive 
*/
  if(ImageType.Manager==MNGR_WD1772 && CyclesPerByte())
  {
#if 1//(SSE_VERSION>=420) //testing!
    position=(WORD)((Fdc.current_time-time_of_last_ip)/cycles_per_byte)%FloppyDisk[Id].TrackBytes;
    //TRACE_LOG("--- position %d\n",position);
#else
    COUNTER_VAR time_now=a_s_t=A_S_T; // should use other var?
    position=(WORD)((time_now-time_of_last_ip)/cycles_per_byte);
    //TRACE_LOG("--- position %d\n",position);
    if(position>=FloppyDisk[Id].TrackBytes)
    {
      // argh! IP didn't occur yet
      position=FloppyDisk[Id].TrackBytes-(WORD)((time_of_next_ip-time_now)/cycles_per_byte);
      time_of_last_ip=time_now;
      if(position>=FloppyDisk[Id].TrackBytes)
        position=0; //some safety
    }
#endif
  }
  else
#endif
    position=HblsToBytes( (hbl_count-hbl_at_ip)% HblsPerRotation() );
  //TRACE_LOG("new position %d\n",position);
  return position;

}


WORD TSF314::BytesToHbls(int bytes) {
  // typically 7.98 for 16 bytes... better be 8 than 7
  WORD perrev=HblsPerRotation();
  int d=(perrev*bytes)/FloppyDisk[Id].TrackBytes;
  int r=(perrev*bytes)%FloppyDisk[Id].TrackBytes;
  if(FloppyDisk[Id].TrackBytes-r>(FloppyDisk[Id].TrackBytes*3)/2)
    d++;
  return (WORD)d;
}


DWORD TSF314::HblsAtIndex() { // absolute
  return (hbl_count/HblsPerRotation())*HblsPerRotation();
}


WORD TSF314::HblsNextIndex() { // relative
  return HblsPerRotation()-hbl_count%HblsPerRotation();
}


WORD TSF314::HblsPerRotation() {
  WORD hbls=HBL_PER_SECOND/(DRIVE_RPM/60);
  ASSERTB(hbls);
#ifdef SSE_LEAN_AND_MEAN
  return hbls;
#else
  return (hbls) ? hbls : 1;  // not 0, it's a divisor
#endif
}


WORD TSF314::HblsToBytes(WORD hbls) {
  return FloppyDisk[Id].TrackBytes*hbls/HblsPerRotation();
}


/*  The Fdc emu written for HFE, SCP, STW images uses events following
    a spinning drive.
    Drive events are index pulse (IP), reading or writing a byte.
*/

int TSF314::CyclesPerByte() { // normally 256
  int cycles;
#if defined(SSE_MEGASTE)
  if(MegaSte.FdHd&BIT_0)
    cycles=(STE_CLOCK8*TICKS8*2); // double
  else
#endif
  if(IS_STE) // STE has a separate 8MHz quartz
    cycles=STE_CLOCK8*TICKS8;
  else
    cycles=CpuNormalHz;
  cycles/=DRIVE_RPM/60; // per rotation (300/60 = 5)
  cycles/=FloppyDisk[Id].TrackBytes; // per byte
  if(!ADAT)
    cycles=DRIVE_FAST_CYCLES_PER_BYTE; //hack!
  cycles_per_byte=cycles; // save
  //ASSERT(cycles);
  return cycles;
}


/*  Function called by event manager in run.cpp based on preset timing
    Motor must be on, a floppy disk must be inside.
    The drive must be selected for the pulse to go to the WD1772.
    If conditions are not met, we put timing of next check 1 second away,
    because it seems to be Steem's way. TODO: better way with less checking,
    though 1/sec is OK!
*/
void TSF314::IndexPulse() {

  //ASSERT(Id==0||Id==1);

  TFloppyDisk &disk=FloppyDisk[Id];

  time_of_next_ip=time_of_next_event+nSysCyclesPerSecond; 

  if(ImageType.Manager!=MNGR_WD1772 || !bMotor || Empty())
    return; 

  time_of_last_ip=time_of_next_event; // record timing
  // Make sure that we always end track at the same byte when R/W
  if(!reading && !writing || disk.current_byte>=disk.TrackBytes-1)
  {
    TRACE_LOG2("reset disk.current_byte from %d\n",disk.current_byte);
    disk.current_byte=0;
  }
  else
  {
    TRACE_LOG2("no reset disk.current_byte at %d\n",disk.current_byte);
  }
  // Program next event, at next IP or in 1 sec (more?)
  COUNTER_VAR cycles=(nSysCyclesPerSecond*60/DRIVE_RPM); // based on drive rpm
  //int cycles=CyclesPerByte()*disk.TrackBytes;
  if(!ADAT)
    // make it longer or it may come before the Fdc event
    cycles*=DRIVE_FAST_IP_MULTIPLIER;
  time_of_next_ip=time_of_last_ip+cycles;
  TRACE_LOG("IP %c: now %d +%d -> %d\n",'A'+Id,time_of_last_ip,cycles,time_of_next_ip);
  // send pulse to Fdc
  if(Psg.CurrentDrive()==Id)
    Fdc.OnIndexPulse(Id);
}


void TSF314::Motor(bool state) {
/*  If we're starting the motor, we must program time of next IP.
    We start from last position or from a new random one.
*/

  if(Id>=DiskMan.nFloppyDrives)
    state=false; // no drive

  TFloppyDisk &disk=FloppyDisk[Id];
#if defined(SSE_DEBUGGER_FAKE_IO)
  if(state!=bMotor && (TRACE_MASK3 & TRACE_CONTROL_FDCPSG))
  {
    TRACE_LOG("FDC %s Drive %c: motor %s\n",disk_manager[DiskEmu.LastManager],'A'+Id,state ? "on" : "off");
  }
#endif
  
  if(ImageType.Manager!=MNGR_WD1772)
  {
  }
  else if(bMotor && !state) //stopping - record position
  { //TODO actually it should stop a bit after IP
    disk.current_byte=(BytePosition())%disk.TrackBytes;
  }
  else if(!bMotor && state) // starting
  {
    WORD bytes_to_next_ip=(disk.current_byte<disk.TrackBytes)
      ? (disk.TrackBytes-disk.current_byte)
      : (rand()%disk.TrackBytes);
    time_of_next_ip=A_S_T + bytes_to_next_ip * CyclesPerByte();
  }
  bMotor=state;
}


void TSF314::Read() { // only called by TWD1772::Read()
/*  For Read() and Write():
    We "sync" on data in the beginning of a sequence.
    After that, we do each successive byte until the command is
    finished. That way, we won't lose any byte, and we add
    2 events/scanline only when reading.
*/
  //ASSERT(!writing);
  //ASSERT(OPTION_AUTOSTW|IMAGE_STW||IMAGE_SCP||IMAGE_HFE);
  //ASSERT(Id==DRIVE);

  //it works but side could change again in the interval
  TFloppyDisk &disk=FloppyDisk[Id];
  if((disk.current_side!=CURRENT_SIDE||disk.current_track!=track||!disk.TrackBytes)
    && ImageType.Manager==MNGR_WD1772 && MfmManager!=NULL)
    if(!MfmManager->LoadTrack(CURRENT_SIDE,CURRENT_TRACK))
      return; // can happen if main thread is changing the disk

  bool new_position=!reading && !writing;

  if(!reading || disk.current_byte>=disk.TrackBytes-1)
  {
#if defined(SSE_DISK_SCP)
    if(!reading || !(IMAGE_SCP))
      disk.current_byte=BytePosition();
    else
      disk.current_byte++;
    reading=true; 
#else
    reading=true; 
    FloppyDisk[Id].current_byte=BytePosition();
#endif
//    TRACE_LOG("Start reading at byte %d\n",FloppyDisk[Id].current_byte);
  }
  else // get next byte regardless of timing
    disk.current_byte++;

  if(ImageType.Manager==MNGR_WD1772)
    Fdc.Mfm.encoded=MfmManager->GetMfmData(new_position ? disk.current_byte : 0xffff);
#if defined(SSE_DRIVE_SINGLESIDE)
  if(bSingleSided && CURRENT_SIDE==1)
    Fdc.Mfm.encoded=(WORD)rand();
#endif
#if defined(SSE_MEGASTE)
  if(disk.Density==2 && ! OPTION_HACKS && !IS_MEGASTE)
    Fdc.Mfm.encoded=(WORD)rand();
#endif
  // set up next byte event
  if(ImageType.Extension!=EXT_SCP && disk.current_byte<=disk.TrackBytes)
  {
#if defined(SSE_DISK_STW2)
    // this is where variable bit rate protections are handled (and not in the DPLL!)
    if(ImageType.Extension==EXT_STW && ImageSTW[Id].Version>=0x0200)
    {
      DiskEmu.BitRate=(512-(WORD)ImageSTW[Id].CurrentTiming*2)*2;
      int t=ImageSTW[Id].CurrentTiming*2*TICKS8;
      t*=DRIVE_RPM_CST;
      t/=DRIVE_RPM;
      Fdc.update_time=Fdc.current_time+t;
    }
    else
#endif
    Fdc.update_time=time_of_last_ip+cycles_per_byte*(disk.current_byte+1);
    if(Fdc.update_time-A_S_T<0) // safety
      Fdc.update_time=A_S_T+cycles_per_byte;
  }
}


void TSF314::Step(int direction) {
#if defined(SSE_DRIVE_SOUND)
  if(OPTION_DRIVE_SOUND && DiskEmu.InterTrack==1) // Step() is called also for SEEK
    SoundStep();
#endif
#if defined(SSE_GUI_EMUCONTROL)
  if(direction && track<SSEOptions.DiscMaxTrack)
#else
  if(direction && track<DRIVE_MAX_CYL)
#endif
    track++;
  else if(track)
    track--;
  //TRACE_LOG("STEP %d\n",track);
  Fdc.Lines.track0=(track==0 && Id<DiskMan.nFloppyDrives);
  if(Fdc.Lines.track0)
    Fdc.str|=FDC_STR_T0; // doing it here?
  CyclesPerByte();  // compute - should be the same every track but...
  //TRACE_LOG("Drive %d Step d%d new track: %d\n",Id,direction,track[Id]);
#if defined(SSE_GUI_STATUS_BAR)
#if !defined(SSE_GUI_STATUS_BAR_CLICKABLE)
  if((!OPTION_DRIVE_INFO) && OPTION_STATUS_BAR && (DRIVE<DiskMan.nFloppyDrives))
#else
  if(OPTION_DRIVE_INFO && OPTION_STATUS_BAR
    && (DRIVE<DiskMan.nFloppyDrives)
    && (SSEConfig.StatusBarMask&(1<<SB_PART_CAPS)))
#endif
  {
#ifdef SSE_DEBUG // add current command (CR)
    sprintf(DiskEmu.sTrackinfo,"%2X-%C:%u-%02u-%02u",Fdc.cr,DRIVE,CURRENT_SIDE,track,Fdc.sr);
#else
    sprintf(DiskEmu.sTrackinfo,"%C:%u-%02u-%02u",DRIVE,CURRENT_SIDE,track,Fdc.sr);
#endif
#if defined(SSE_GUI_STATUS_BAR_DRAW_CAPS)
    strcpy(status_bar_text[SB_PART_CAPS],DiskEmu.sTrackinfo);
    DiskEmu.StatusBar=true;
    UPDATE_STATUS_BAR_PART(SB_PART_CAPS);
#else
    PostMessage(hStatusBar,SB_SETTEXT,SB_PART_CAPS,(LPARAM)DiskEmu.sTrackinfo);
#endif
  }
#endif//#if defined(SSE_GUI_STATUS_BAR)
}


void TSF314::Write() {
  //ASSERT(OPTION_AUTOSTW|IMAGE_STW||IMAGE_SCP||IMAGE_HFE);
  //ASSERT(Id==DRIVE);
  //ASSERT(FloppyDisk[Id].current_side==CURRENT_SIDE);
  //ASSERT(FloppyDisk[Id].current_track==CURRENT_TRACK);

#if defined(SSE_DISK_SCP) || defined(SSE_DISK_HFE)
  bool new_position=!writing;
#endif

  TFloppyDisk &disk=FloppyDisk[Id];

  if(!writing || disk.current_byte>=disk.TrackBytes-1)
  {
    if(reading && disk.current_byte<disk.TrackBytes-1)
      disk.current_byte++;
    else
      disk.current_byte=BytePosition();
    writing=true;
    reading=false;
//    TRACE_LOG("Start writing at byte %d\n",disk.current_byte);
  }
  else
    disk.current_byte++;
#if defined(SSE_DRIVE_SINGLESIDE)
  if(bSingleSided && CURRENT_SIDE==1)
  {}
  else
#endif
#if defined(SSE_MEGASTE)
  if(disk.Density==2 && ! OPTION_HACKS && !IS_MEGASTE)
  {}
  else
#endif
  if(ImageType.Manager==MNGR_WD1772)
  {
#ifdef SSE_OSD_DRIVELED
    if(disk.IsZip())
      FDCCantWriteDisplayTimer=timer+5000; // Writing will be lost!
#endif
    MfmManager->SetMfmData((new_position) ? disk.current_byte : 0xffff,Fdc.Mfm.encoded);
  }

  // set up next byte event
  if(disk.current_byte<=disk.TrackBytes)
    Fdc.update_time=time_of_last_ip+cycles_per_byte*(disk.current_byte+1);
  if(Fdc.update_time-A_S_T<0)
    Fdc.update_time=A_S_T+cycles_per_byte;
}


/////////////////
// DRIVE SOUND //
/////////////////

#if defined(SSE_DRIVE_SOUND)

/*  This is where we emulate the floppy drive sounds.
    Each drive can have its own soundset.
    DirectSound makes it rather easy, you just load your samples
    in secondary buffers and play as needed, one shot or loop,
    the mixing is done by the system.
    In Linux I tried to do it with portaudio but it doesn't work, at least on
    VMWare. RtAudio doesn't even pretend to offer multiple streams. 
    PulseAudio +- OK in VMWare.
*/

char* drive_sound_wav_files[NSOUNDS]={"drive_startup.wav","drive_spin.wav",
                                      "drive_click.wav","drive_seek.wav"};

#ifdef UNIX
char *dbg_snd_name[]={"START","MOTOR","STEP","SEEK_BI","SEEK_FWD","SEEK_BACK","SEEK"};
#endif

EasyStr DriveSoundDir[2];

#undef LOGSECTION
#define LOGSECTION LOGSECTION_SOUND


//410
#ifdef UNIX
void TSF314::SoundPlay(int snd) {
  if(!SoundOn[Id][snd])
  {
    TRACE_LOG("Play %s %c buffer %p stream %x\n",dbg_snd_name[snd],'A'+Id,SoundBuffer[Id][snd],pulseDiskStream[Id][snd]);  
  }
  switch(UseSound) {
#ifdef UNIX
#if defined(SSE_UNIX_PULSEAUDIO)
  case XS_PULSE:
    
    if(SoundBuffer[Id][snd]&&!SoundOn[Id][snd]&&pulseDiskStream[Id][snd])
    {
      SoundIndex[Id][snd]=0;
      pa_stream_cork(pulseDiskStream[Id][snd],FALSE,NULL,NULL);
      SoundOn[Id][snd]=true;
    }
    break;
#endif
#ifndef NO_PORTAUDIO
  case XS_PA:
    Pa_StartStream(SoundBuffer[Id][snd]);
    break;
#endif
#endif//UNIX
  }//sw
}
  
  
void TSF314::SoundStop(int snd) {
  if(SoundOn[Id][snd])
  {
    TRACE_LOG("Stop %s %c buffer %p stream %x\n",dbg_snd_name[snd],'A'+Id,SoundBuffer[Id][snd],pulseDiskStream[Id][snd]);
  }
  switch(UseSound) {  
#ifdef UNIX
#if defined(SSE_UNIX_PULSEAUDIO)
  case XS_PULSE:
    if(SoundBuffer[Id][snd]&&SoundOn[Id][snd]&&pulseDiskStream[Id][snd])
    {
      pa_stream_cork(pulseDiskStream[Id][snd],TRUE,NULL,NULL);
    }
    SoundOn[Id][snd]=false;
    break;
#endif
#ifndef NO_PORTAUDIO
  case XS_PA:
    Pa_StopStream(SoundBuffer[Id][snd]);
    break;
#endif
#endif//UNIX
  }//sw
}
#endif


void TSF314::SoundChangeVolume() {
/*  Same volume for each buffer
*/
  //ASSERT(Id<2);
  SoundVolume=MIN(SoundVolume,0);
  for(int i=0;i<NSOUNDS;i++)
  {
#ifdef WIN32    
    if(SoundBuffer[Id][i])
      SoundBuffer[Id][i]->SetVolume(SoundVolume);
#endif
#ifdef UNIX

    if(UseSound==XS_PULSE)
    {
      if(pulseDiskStream[Id][i])
      {
        pa_cvolume vol;
        pa_volume_t v=SoundVolume ? (SoundVolume+10000)*6 :  PA_VOLUME_NORM;
        pa_cvolume_set(&vol,1,v); // don't know if 2
        DWORD idx=pa_stream_get_index(pulseDiskStream[Id][i]);
        pa_context_set_sink_input_volume(context,idx,&vol,NULL,NULL);        
      }
    }
#endif
  }
}


void TSF314::SoundCheckCommand(BYTE cr) {
/*  Called at each WD1772 command, beginning and when spun up (all managers).
    If motor wasn't on we play the startup sound.
*/
  //ASSERT(Id<2);
  if(MuteWhenInactive && !bAppActive || Id>=DiskMan.nFloppyDrives || ImageType.Manager==MNGR_PRG)
    return;
  //TRACE_LOG("%c: SoundCheckCommand(%x) motor %d\n",'A'+Id,cr,motor);
#ifdef WIN32  
  DWORD dwStatus=0; //W4
#endif
  // start MOTOR
  if(!(Fdc.str&FDC_STR_MO) && SoundBuffer[Id][START] && bDiskInDrive)
  {
#ifdef WIN32
    SoundBuffer[Id][START]->GetStatus(&dwStatus);
    if(!(dwStatus&DSBSTATUS_PLAYING))
      SoundBuffer[Id][START]->Play(0,0,0);
#endif
#ifdef UNIX
    SoundPlay(START);
#endif
  }

  // start SEEK
  BOOL OneSeekSample=TRUE;
#ifdef WIN32
  if(SoundBuffer[Id][SEEK])
    SoundBuffer[Id][SEEK]->GetStatus(&dwStatus);
#endif

  // should play seek sound?
  if(cr&BIT_7)
  { // not type I
  }
  else if(SoundBuffer[Id][SEEK] &&
#ifdef WIN32
    !(dwStatus&DSBSTATUS_PLAYING) &&
#endif
    DiskEmu.InterTrack>1)
  {
    
#ifdef WIN32    
    DWORD sf=SeekFreq[Id][SEEK];
    // alter frequency according to step rate, sample must be long enough
    switch(cr&3) {
    case 0: // 6ms
      sf*=5,sf/=6;
      //sf*=4,sf/=5;
      break;
    case 1: // 12ms
      sf*=4,sf/=5;
      //sf*=3,sf/=4;
      break;
    case 2: // 2ms
      sf*=4, sf/=3;
      break;
    // default = 3: 3 ms
    }
    ////if(sf!=SeekFreq[Id][SEEK]) TRACE_OSD("sf %d %d",sf,SeekFreq[Id][SEEK]);
    // alter the sound to differentiate forward/backward, although I don't
    // know the theory on this! only if no back/fwd samples available
    if(DiskEmu.direction && OneSeekSample) // < or >?
      sf+=SeekFreq[Id][SEEK]/60;
    //  sf+=SeekFreq[Id][SEEK]/50;
    SoundBuffer[Id][SEEK]->SetFrequency(sf);
    SoundBuffer[Id][SEEK]->SetCurrentPosition(0);
    SoundBuffer[Id][SEEK]->Play(0,0,DSBPLAY_LOOPING); // start SEEK loop
    TRACE_LOG("F%d Play SEEK speed %d %d->%d\n",FRAME,fdc_step_time_to_hbls[(cr&3)]/15,
      (cr&BIT_4) ? Fdc.tr : DiskEmu.track,(cr&BIT_4) ? Fdc.dr : 0);
#endif//WIN32
#ifdef UNIX
    SoundPlay(SEEK);
#endif
    
  }
  else if( (ImageType.Manager==MNGR_PASTI||ImageType.Manager==MNGR_CAPS)
    && ( DiskEmu.InterTrack==1 ))
  {
    SoundStep();
  }
}


void TSF314::SoundCheckIrq() {
/*  Called at the end of each FDC command, all WD1772 emulations.
    Stop SEEK loop.
    Emit a "STEP" click noise if we were effectively seeking
    (generally already stopped at VBL check).
*/
  //ASSERT(Id<2);
  if(SoundBuffer[Id][SEEK])
  {
#ifdef WIN32
    DWORD dwStatus ;
    SoundBuffer[Id][SEEK]->GetStatus(&dwStatus);
    if((dwStatus&DSBSTATUS_PLAYING))
#endif
    {
      TRACE_LOG("IRQ stop SEEK\n");
#ifdef WIN32      
      SoundBuffer[Id][SEEK]->Stop();
#endif
#ifdef UNIX
      SoundStop(SEEK);
#endif
      if(DiskEmu.InterTrack>1)
        SoundStep(TRUE); // approximation of end of seek = click, lower
    }
  }
}


void TSF314::SoundVBL() { // start or stop playing sound loops if needed

  ASSERTB(Id<2);
#ifndef SSE_LEAN_AND_MEAN
  Id&=1;
#endif
  //TRACE_LOG("Frame %d drive sound VBL check STR %x\n",FRAME,DiskEmu.str);
  // check MOTOR
  bool motor_on= ((Fdc.str&FDC_STR_MO)
    && !Empty() // but clicks still on
    && ImageType.Manager!=MNGR_PRG
    && Id==Psg.CurrentDrive() //must be selected
    && !(MuteWhenInactive && !bAppActive)
    && Id<DiskMan.nFloppyDrives);
 
#ifdef WIN32
  DWORD dwStatus;
  if(SoundBuffer[Id][MOTOR])
  {
    SoundBuffer[Id][MOTOR]->GetStatus(&dwStatus);
    if(OPTION_DRIVE_SOUND && motor_on && !(dwStatus&DSBSTATUS_PLAYING))
      SoundBuffer[Id][MOTOR]->Play(0,0,DSBPLAY_LOOPING); // start motor loop
    else if((!OPTION_DRIVE_SOUND||!motor_on) && (dwStatus&DSBSTATUS_PLAYING))
      SoundBuffer[Id][MOTOR]->Stop();
  }
#endif

#ifdef UNIX
  if(OPTION_DRIVE_SOUND && motor_on && !SoundOn[Id][MOTOR])
    SoundPlay(MOTOR);
  else if((!OPTION_DRIVE_SOUND||!motor_on)&&SoundOn[Id][MOTOR])
    SoundStop(MOTOR);
#endif

  // check command
  if(DiskEmu.VBLSoundcheck && motor_on) // pasti, caps
  {
    if(Fdc.str&FDC_STR_SU) // spunup -> can start sound
    {
      //TRACE_LOG("(%d) SU\n",ImageType.Manager);
      SoundCheckCommand(Fdc.cr);
      DiskEmu.VBLSoundcheck=false;
    }
  }
  // check if we must stop SEEK sample
  if(SoundBuffer[Id][SEEK])
  {
#ifdef WIN32
    SoundBuffer[Id][SEEK]->GetStatus(&dwStatus);
    //TRACE_OSD("%d-%d",DiskEmu.track,DiskEmu.maxtrack);
    if((dwStatus&DSBSTATUS_PLAYING) && (Id!=Psg.CurrentDrive()
      || (Fdc.cr&BIT_7)
      || (Fdc.str&(FDC_STR_BSY|FDC_STR_MO))!=(FDC_STR_BSY|FDC_STR_MO)
      || abs_quicki(old_track-DiskEmu.track)==DiskEmu.InterTrack
      || !DiskEmu.direction && DiskEmu.track==0
      || DiskEmu.direction && DiskEmu.track>=DiskEmu.maxtrack))
    {
      TRACE_LOG("F%d stop SEEK\n",FRAME);
      SoundBuffer[Id][SEEK]->Stop();
      if(DiskEmu.InterTrack>1)
        SoundStep(TRUE); // approximation of end of seek = click, lower
    }
#endif//WIN32
#ifdef UNIX
    if(Id!=Psg.CurrentDrive()
      || Fdc.CommandType()!=1 
      || (Fdc.str&(FDC_STR_BSY|FDC_STR_MO))!=(FDC_STR_BSY|FDC_STR_MO)
      || old_track==DiskEmu.track)
      SoundStop(SEEK);
#endif
  }
}


#ifdef UNIX

#ifndef NO_PORTAUDIO

int PADiskCallback(const void*,void *pOutBuf,unsigned long frameCount, const 
                PaStreamCallbackTimeInfo* OutTime,PaStreamCallbackFlags,void* i) {
  int sndi=(INT_PTR)i&0xFF;
  int Id=((INT_PTR)i>>8)&0xFF;
  int bytes_per_sample=((INT_PTR)i>>16)&0xFF;
  bool loop=(sndi==MOTOR || sndi==SEEK);
  char *mybuffer=(char*)pOutBuf;
  int rv=paContinue;
  for(DWORD i=0;i<frameCount;i++)
  {
    for(int a=0;a<bytes_per_sample;a++)
    {
      *mybuffer=*(SoundBuffer[Id][sndi]+SoundIndex[Id][sndi]);
      if(SoundIndex[Id][sndi]<SampleLength[Id][sndi])
      {
        SoundIndex[Id][sndi]++;
      }
      else if(loop)
      {
        SoundIndex[Id][sndi]=0;
      }
      else
        rv=paComplete;
      if(bytes_per_sample==1)
        *mybuffer^=128;
      mybuffer++;
    }
  }
  return rv;
}

#endif

#if defined(SSE_UNIX_PULSEAUDIO)

void stream_started_callback(pa_stream *s, void *userdata);

void floppy_stream_state_cb(pa_stream *s, void *userdata) {
  return;
#if 0  //dbg
  int id=(COUNTER_VAR)userdata>>16;
  int i=(COUNTER_VAR)userdata&0xFFFF;
  TRACE("stream_state_callback %c-%d %x %d\n",'A'+id,i,s,pa_stream_get_state(s));
  
  char cmt[PA_CHANNEL_MAP_SNPRINT_MAX], sst[PA_SAMPLE_SPEC_SNPRINT_MAX];  
  TRACE("Using sample spec '%s', channel map '%s'.\n",
          pa_sample_spec_snprint(sst, sizeof(sst), pa_stream_get_sample_spec(s)),
          pa_channel_map_snprint(cmt, sizeof(cmt), pa_stream_get_channel_map(s)));

  TRACE("Connected to device %s (%u, %ssuspended).\n",
          pa_stream_get_device_name(s),
          pa_stream_get_device_index(s),
          pa_stream_is_suspended(s) ? "" : "not ");
#endif
  
}

void floppy_stream_cb(pa_stream *s, size_t length, void *userdata) {
  
  int sndi=((COUNTER_VAR)userdata)&0xFF;
  //ASSERT(sndi!=SEEK);
  int Id=(((COUNTER_VAR)userdata)>>8)&0xFF;
  int bytes_per_sample=(((COUNTER_VAR)userdata)>>16)&0xFF;
  //ASSERT(bytes_per_sample);
  if(!SoundBuffer[Id][sndi])// ||sndi!=MOTOR || Id)//temp
    return;
  bool loop=(sndi==MOTOR || sndi==SEEK) ;
  BYTE *mybuffer=(BYTE*)malloc(length);
  BYTE *b=mybuffer;
  int dbg_i=0;
  int dbg_start=SoundIndex[Id][sndi];
#if 1
//SoundIndex[Id][sndi]=0;
  for(DWORD i=0;i<length;i++)
  {

  //    if(sndi!=MOTOR || Id)//tmp
    //    *b=0;
   //  else
       
      *b=*(SoundBuffer[Id][sndi]+SoundIndex[Id][sndi]);
      
      if(SoundIndex[Id][sndi]<SampleLength[Id][sndi])
      {
        SoundIndex[Id][sndi]++;
        // that's better than Win build!
        if(bLoop[Id][sndi] && SoundIndex[Id][sndi]==LoopEnd[Id][sndi])
        {
          SoundIndex[Id][sndi]=LoopStart[Id][sndi];
          if(!(i&1))
            SoundIndex[Id][sndi]++;
        }
      }      
      else if(loop)
      {
        //SoundIndex[Id][sndi]=0;
        SoundIndex[Id][sndi]= (i&1) ? 0 : 1;
      }
      else
      {
        if(SoundOn[Id][sndi])
          FloppyDrive[Id].SoundStop(sndi);
        /////SoundOn[Id][sndi]=false;
      }
      
      if(bytes_per_sample==1)
        *b^=128;
      
      b++;

      dbg_i++;

  }

#else  
  for(DWORD i=0;i<length/bytes_per_sample;i++)
  {
    for(int a=0;a<bytes_per_sample;a++)
    {
      if(sndi!=MOTOR || Id)//tmp
        *b=0;
     else
       
      *b=*(SoundBuffer[Id][sndi]+SoundIndex[Id][sndi]);
      
      if(SoundIndex[Id][sndi]<SampleLength[Id][sndi])
      {
        SoundIndex[Id][sndi]++;
      }
      else if(loop)
      {
        SoundIndex[Id][sndi]=0;
      }
      else
      {}
      
      if(bytes_per_sample==1)
        *b^=128;
      b++;
      dbg_i++;
    }
  }
#endif
  int dbg_stop=SoundIndex[Id][sndi];
  if (pa_stream_write(s, (uint8_t*) mybuffer , length, NULL, 0,
    PA_SEEK_RELATIVE) < 0) {
      TRACE("pa_stream_write() failed: %s\n", pa_strerror(pa_context_errno(context)));
      //quit(1);
      return;
  }  
  
  free(mybuffer);
}


#endif

#endif//UNIX


void TSF314::SoundLoadSamples(
#ifdef WIN32 // called by DSCreateSoundBuf()
              IDirectSound *DSObj,DSBUFFERDESC *dsbd,WAVEFORMATEX *wfx
#endif
                               ) {
/*  Called from sound.cpp's DSCreateSoundBuf(), on each run().
    We load each sample in its own secondary buffer, each time, which doesn't 
    seem optimal, but saves memory.
*/
  //ASSERT(Id<2);
  HRESULT Ret;
  TWavFileFormat WavFileFormat;
  FILE *fp;
  EasyStr path=DriveSoundDir[Id]+SLASH;
  EasyStr pathplusfile;
  for(int i=0;i<NSOUNDS;i++)
  {
    pathplusfile=path;
    pathplusfile+=drive_sound_wav_files[i];
    fp=fopen(pathplusfile.Text,"rb");
    if(fp)
    {
      FREAD(&WavFileFormat,sizeof(TWavFileFormat),1,fp);
#ifdef WIN32      
      wfx->nChannels=WavFileFormat.nChannels;
      SeekFreq[Id][i]=wfx->nSamplesPerSec=WavFileFormat.nSamplesPerSec;
      //TRACE_LOG("%s SeekFreq[%d][%d]=%d\n",pathplusfile.Text,Id,i,SeekFreq[Id][i]);
      wfx->wBitsPerSample=WavFileFormat.wBitsPerSample;
      wfx->nBlockAlign=wfx->nChannels*wfx->wBitsPerSample/8;
      wfx->nAvgBytesPerSec=WavFileFormat.nAvgBytesPerSec;
      dsbd->dwFlags|=DSBCAPS_STATIC|DSBCAPS_CTRLFREQUENCY;
      dsbd->dwBufferBytes=WavFileFormat.length;
      Ret=DSObj->CreateSoundBuffer(dsbd,&SoundBuffer[Id][i],NULL);
      if(Ret==DS_OK)
      {
        LPVOID lpvAudioPtr1;
        DWORD dwAudioBytes1;
        Ret=SoundBuffer[Id][i]->Lock(0,0,&lpvAudioPtr1,&dwAudioBytes1,NULL,0,DSBLOCK_ENTIREBUFFER);
        if(Ret==DS_OK)
          FREAD(lpvAudioPtr1,1,dwAudioBytes1,fp);
        Ret=SoundBuffer[Id][i]->Unlock(lpvAudioPtr1,dwAudioBytes1,NULL,0);
      }
#endif//WIN32

#ifdef UNIX
      switch(UseSound) {
#ifndef NO_PORTAUDIO
      // doesn't work on my system (Device unavailable)
      case XS_PA:
        if(pa_init)
        {
          PaStreamParameters outStreamParams;
          ZeroMemory(&outStreamParams,sizeof(PaStreamParameters));
          outStreamParams.device=pa_out_dev;
          outStreamParams.channelCount=WavFileFormat.nChannels;
          outStreamParams.sampleFormat=(WavFileFormat.wBitsPerSample==16)
            ? paInt16 : paUInt8;
          // this is not like DirectSound, opening mulitple streams in portaudio
          // seems liable to fail            
          PaError err=Pa_OpenStream(&paDiskSoundStream[Id][i],NULL,
            &outStreamParams,WavFileFormat.nSamplesPerSec,pa_output_buffer_size,
            paDitherOff | paClipOff,PADiskCallback,
            (void*)((WavFileFormat.wBitsPerSample/8<<16)+(Id<<8)+i));
          if(err==paNoError)
          {
            SoundBuffer[Id][i]=new BYTE[WavFileFormat.length];
            SampleLength[Id][i]=WavFileFormat.length;
            SoundIndex[Id][i]=0;
            FREAD(SoundBuffer[Id][i],1,WavFileFormat.length,fp);
            TRACE_LOG("%s loaded\n",CHECKPATH(pathplusfile.Text));
          }
          else
          {
            TRACE_LOG("Pa_OpenStream %s %dHz %dbit: %p %s\n",
                      CHECKPATH(pathplusfile.Text),WavFileFormat.nSamplesPerSec,
                      WavFileFormat.wBitsPerSample,paDiskSoundStream[Id][i],Pa_GetErrorText(err));
          }
        }
        break;
#endif
#ifndef NO_RTAUDIO
      case XS_RT:
        SoundBuffer[Id][i]=NULL; // rtaudio has one single stream so forget it
        break;
#endif
#if defined(SSE_UNIX_PULSEAUDIO)
      case XS_PULSE:
        if(pulse_init)
        {
          pa_sample_spec floppy_sample_spec = {
            .format = (WavFileFormat.wBitsPerSample==16 ? PA_SAMPLE_S16LE
                : PA_SAMPLE_U8),
            //.rate = WavFileFormat.nSamplesPerSec,
            .rate = (uint32_t)WavFileFormat.nSamplesPerSec,
            //.channels = WavFileFormat.nChannels
            .channels = (uint8_t)WavFileFormat.nChannels
          };
          // in control panel, only app "steem" appears though
          char myStreamName[16];
          sprintf(myStreamName,"Floppy%d-%02d",Id,i);
          TRACE_LOG("%d %d %d %s\n",floppy_sample_spec.channels,floppy_sample_spec.format,floppy_sample_spec.rate,myStreamName);
          if (!(pulseDiskStream[Id][i] = pa_stream_new(context, myStreamName, &floppy_sample_spec, channel_map_set 
              ? &channel_map : NULL))) {
                TRACE_LOG("floppy pa_stream_new() failed: %s\n", pa_strerror(pa_context_errno(context)));
                break;
          }
          int r;
          
          pa_cvolume vol;
          vol.channels=WavFileFormat.nChannels;
          vol.values[0]=vol.values[1]=(SoundVolume+10000)*6;
          
          if ((r = pa_stream_connect_playback(pulseDiskStream[Id][i], pulse_device, 
            NULL, PA_STREAM_START_CORKED, (SoundVolume?&vol:NULL), NULL /*pulseDiskStream[Id][0]*/ /*stream*/)) < 0) {
              TRACE("floppy pa_stream_connect_playback() failed: %s\n", pa_strerror(pa_context_errno(context)));
              break;
          }
          SoundBuffer[Id][i]=new BYTE[WavFileFormat.length];
          SampleLength[Id][i]=WavFileFormat.length;
          SoundIndex[Id][i]=0;
          SoundOn[Id][i]=false;
          int br=FREAD(SoundBuffer[Id][i],1,WavFileFormat.length,fp);
          //pa_stream_set_state_callback(pulseDiskStream[Id][i], floppy_stream_state_cb,(void*)((Id<<16)+i));
          //pa_stream_set_started_callback(pulseDiskStream[Id][i], stream_started_callback, NULL);
          pa_stream_set_write_callback(pulseDiskStream[Id][i],floppy_stream_cb,
                                       (void*)(((WavFileFormat.wBitsPerSample/8)<<16)+(Id<<8)+i));
                      
          TRACE_LOG("%s loaded, len %d %d\n",CHECKPATH(pathplusfile.Text),SampleLength[Id][i],br);
          BYTE ch;
          do {
            br=FREAD(&ch,1,1,fp);
            if(br)
              TRACE("%c %x ",((ch>='A')?ch:'?'),ch);
          } while(br==1);
          TRACE("\n");
          // detect 'smpl' cue points (Wavosaur)
          const char sSmpl[5]="smpl";
          char rSmpl[48]="????";
          br=FREAD(rSmpl,1,4,fp);
          if(!strcmp(sSmpl,rSmpl))
          {

            br=FREAD(rSmpl,1,48,fp); // skip
            DWORD a=0,b=0;
            br=FREAD(&a,sizeof(DWORD),1,fp);
            br=FREAD(&b,sizeof(DWORD),1,fp);
            TRACE_LOG("smpl found loop %d->%d\n",a,b);
            if(b>a)
            {
              bLoop[Id][i]=true;
              LoopStart[Id][i]=a;
              LoopEnd[Id][i]=b;//a+300;//b;
            }
          }
        }
        break;
#endif
      }//sw
#endif//UNIX
      fclose(fp);
    }
#if defined(SSE_ENABLE_TRACE_LOG)
    else TRACE_LOG("DriveSound. Can't load sample file %s\n",CHECKPATH(pathplusfile.Text));
#endif
  }//nxt
  SoundChangeVolume();
}


void TSF314::SoundReleaseBuffers() {
  // Called from sound.cpp's DSReleaseAllBuffers()
  //ASSERT(Id<2);
  TRACE_LOG("SoundReleaseBuffers %c\n",'A'+Id);
  for(int i=0;i<NSOUNDS;i++)
  {
    if(SoundBuffer[Id][i])
    {
#ifdef WIN32
      SoundBuffer[Id][i]->Stop();
      SoundBuffer[Id][i]->Release();
#endif
#ifdef UNIX
      switch(UseSound) {
#ifndef NO_PORTAUDIO
      case XS_PA:
        Pa_StopStream(paDiskSoundStream[Id][i]);
        Pa_CloseStream(paDiskSoundStream[Id][i]);
        break;
#endif
#if defined(SSE_UNIX_PULSEAUDIO)
      case XS_PULSE:
        if(pulse_init)
        {
          if(pulseDiskStream[Id][i])
            pa_stream_unref(pulseDiskStream[Id][i]);
          pulseDiskStream[Id][i]=NULL;
        }
        break;
#endif
      }//sw
      delete[] SoundBuffer[Id][i];
#endif
    }
    SoundBuffer[Id][i]=NULL;
  }
}


void TSF314::SoundStep(BOOL end_of_seek) {
  // end_of_seek: called at the end of a SEEK, different sound
  //ASSERT(Id<2);
  if(!SoundBuffer[Id][STEP] || ImageType.Manager==MNGR_PRG
    || (MuteWhenInactive && !bAppActive ) || Id>=DiskMan.nFloppyDrives)
    return;
#ifdef WIN32
  TRACE_LOG("%d Play STEP\n",DiskEmu.track);
  // quieter for further tracks, end of seek
  int db=SoundVolume-DiskEmu.track;
  if(end_of_seek)
    db-=100;
  // TRACE_OSD("%d %d",SoundVolume,db);
  SoundBuffer[Id][STEP]->SetVolume(db);
  SoundBuffer[Id][STEP]->SetCurrentPosition(0);
  SoundBuffer[Id][STEP]->Play(0,0,0);
  // play seek sound if we're stepping through
  if((Fdc.old_cr&(BIT_6|BIT_5|BIT_4)) && !(Fdc.old_cr&BIT_7)
     && old_track!=track && !end_of_seek && SoundBuffer[Id][SEEK])
  {
    TRACE_LOG2("F%d Play SEEK for STEP\n",FRAME);
    //SoundBuffer[Id][SEEK]->SetCurrentPosition(0);
    SoundBuffer[Id][SEEK]->SetVolume(SoundVolume-20);
    SoundBuffer[Id][SEEK]->Play(0,0,DSBPLAY_LOOPING);
  }
#endif//WIN32
#ifdef UNIX
  SoundPlay(STEP);
#endif
}


void TSF314::SoundStopBuffers() {
  for(int i=0;i<NSOUNDS;i++)
    if(SoundBuffer[Id][i])
    {
#ifdef WIN32
      SoundBuffer[Id][i]->Stop();
#endif
#ifdef UNIX
      SoundStop(i);
#endif
    }
}

#endif//sound

#undef LOGSECTION
