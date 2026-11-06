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
FILE: floppybridge_drive.cpp
CONDITION: SSE_FLOPPYBRIDGE must be defined
DESCRIPTION: Real floppy drive through FloppyBridge (Greaseweazle, DrawBridge,
SuperCard Pro). The drive code lives in Rob Smith's FloppyBridge DLL, loaded
at run time by the public-domain client in 3rdparty/floppybridge; get
FloppyBridge.dll / FloppyBridge_x64.dll from
https://amiga.robsmithdev.co.uk/winuae and put it next to Steem.

The drive is described by a small text file with the .hfe extension, so it is
inserted from the disk manager like any image:

  FLOPPYBRIDGE
  driver=greaseweazle   ; greaseweazle (default), drawbridge, supercardpro
  port=auto             ; auto (default) or a serial port, eg COM5
  drive=A               ; A (default), B for a PC cable; 0-3 for Shugart
  log=on                ; on (default): Greaseweazle.log next to the file
  density=auto          ; auto (default), off or raw: see FloppyBridgeSmoothSpeeds
  ;config=...           ; or a complete FloppyBridge config string

The bridge runs in "stalling" mode: when the emulated FDC needs a track that
hasn't been read yet, Steem waits for the drive instead of getting garbage.
Steem doesn't run while it waits, so the ST doesn't see the delay. Smart
speed/turbo is disabled so FloppyBridge reports the measured speed of the
data (variable-density protections such as Rob Northen Copylock need it),
and the drive is used as DD.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#if defined(SSE_FLOPPYBRIDGE)

#include <floppybridge_drive.h>
// the client is a single source file, built here so the project needs no
// special settings for it
#include <floppybridge/floppybridge_lib.cpp>

#include <ctype.h>

#include <stdarg.h>
#include <chrono>
#include <thread>

typedef std::chrono::steady_clock TFbClock;

struct TFloppyBridgeDrive {
  FloppyBridgeAPI *api;
  FILE *log;
  TFbClock::time_point t0;
  int density; // 0 auto, 1 off, 2 raw
  bool disk_in;
};


static int ElapsedMs(TFbClock::time_point since) {
  return (int)std::chrono::duration_cast<std::chrono::milliseconds>(
    TFbClock::now()-since).count();
}


static void FbSleep(int ms) {
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}


static void Log(TFloppyBridgeDrive *drv,const char *fmt,...) {
  if(!drv || !drv->log)
    return;
  fprintf(drv->log,"%7.3f  ",ElapsedMs(drv->t0)/1000.0);
  va_list args;
  va_start(args,fmt);
  vfprintf(drv->log,fmt,args);
  va_end(args);
  fputs("\n",drv->log);
  fflush(drv->log); // so the log is complete even if Steem is killed
}


static void SetError(char *error,int error_size,const char *msg) {
  if(error && error_size>0)
  {
    strncpy(error,msg ? msg : "FloppyBridge error",error_size-1);
    error[error_size-1]=0;
  }
}


static bool SameText(const char *a,const char *b) {
  for(;*a && *b;a++,b++)
    if(tolower((unsigned char)*a)!=tolower((unsigned char)*b))
      return false;
  return !*a && !*b;
}


static bool ContainsText(const char *text,const char *word) {
  size_t n=strlen(word);
  for(;*text;text++)
  {
    size_t i=0;
    while(i<n && text[i] && tolower((unsigned char)text[i])==tolower((unsigned char)word[i]))
      i++;
    if(i==n)
      return true;
  }
  return false;
}


// descriptor settings
struct TFloppyBridgeSettings {
  char driver[64],port[64],drive[16],log[16],density[16];
  char config[1024];
  TFloppyBridgeSettings() {
    strcpy(driver,"greaseweazle");
    strcpy(port,"auto");
    strcpy(drive,"A");
    strcpy(log,"on");
    strcpy(density,"auto");
    config[0]=0;
  }
};


static void ParseDescriptor(const char *text,TFloppyBridgeSettings &s) {
  // key=value lines, ';' or '#' start a comment (except in config=); the
  // first line is the signature
  const char *p=text;
  while(*p)
  {
    const char *eol=p;
    while(*eol && *eol!='\n' && *eol!='\r')
      eol++;
    char line[1100];
    size_t len=(size_t)(eol-p);
    if(len>=sizeof(line))
      len=sizeof(line)-1;
    memcpy(line,p,len);
    line[len]=0;
    // a config string is kept whole, it may contain ';' or '#'
    char *k=line;
    while(*k==' '||*k=='\t') k++;
    if(strncmp(k,"config",6) && strncmp(k,"CONFIG",6))
    {
      char *c=strpbrk(line,";#");
      if(c)
        *c=0;
    }
    char *eq=strchr(line,'=');
    if(eq)
    {
      *eq=0;
      char *key=line,*val=eq+1;
      while(*key==' '||*key=='\t') key++;
      while(*val==' '||*val=='\t') val++;
      for(char *e=key+strlen(key);e>key && (e[-1]==' '||e[-1]=='\t');) *--e=0;
      for(char *e=val+strlen(val);e>val && (e[-1]==' '||e[-1]=='\t');) *--e=0;
      char *dst=NULL;
      size_t dst_size=0;
      if(SameText(key,"driver")) { dst=s.driver; dst_size=sizeof(s.driver); }
      else if(SameText(key,"port")) { dst=s.port; dst_size=sizeof(s.port); }
      else if(SameText(key,"drive")) { dst=s.drive; dst_size=sizeof(s.drive); }
      else if(SameText(key,"config")) { dst=s.config; dst_size=sizeof(s.config); }
      else if(SameText(key,"log")) { dst=s.log; dst_size=sizeof(s.log); }
      else if(SameText(key,"density")) { dst=s.density; dst_size=sizeof(s.density); }
      if(dst)
      {
        strncpy(dst,val,dst_size-1);
        dst[dst_size-1]=0;
      }
    }
    p=eol;
    while(*p=='\n'||*p=='\r')
      p++;
  }
}


static int FindDriver(const char *name) {
  // by index, or by (part of) the name FloppyBridge gives the driver
  if(isdigit((unsigned char)name[0]))
    return atoi(name);
  const char *word=name;
  if(ContainsText(name,"grease"))
    word="greaseweazle";
  else if(ContainsText(name,"drawbridge")||ContainsText(name,"arduino"))
    word="drawbridge";
  else if(ContainsText(name,"supercard")||ContainsText(name,"scp"))
    word="supercard";
  std::vector<FloppyBridgeAPI::DriverInformation> list;
  FloppyBridgeAPI::getDriverList(list);
  for(size_t i=0;i<list.size();i++)
  {
    char drv_name[BRIDGE_STRING_MAX_LENGTH+1];
    // the project is built MultiByte, so TCHAR is char
    strncpy(drv_name,(const char*)list[i].name,BRIDGE_STRING_MAX_LENGTH);
    drv_name[BRIDGE_STRING_MAX_LENGTH]=0;
    if(ContainsText(drv_name,word))
      return (int)list[i].driverIndex;
  }
  return -1;
}




/////////////////////////
// checking a revolution
/////////////////////////

// MFM bit i of the revolution (first bit = MSB of mfm[0])
static inline int MfmBit(const BYTE *mfm,int i) {
  return (mfm[i>>3]>>(7-(i&7)))&1;
}


// data byte of the 16 cells starting at bit pos (clock, data, clock...)
static int MfmByte(const BYTE *mfm,int nbits,int pos) {
  if(pos<0 || pos+16>nbits)
    return -1;
  int b=0;
  for(int k=1;k<16;k+=2)
    b=(b<<1)|MfmBit(mfm,pos+k);
  return b;
}


static WORD Crc16(WORD crc,BYTE b) { // CCITT, as the WD1772
  crc^=(WORD)(b<<8);
  for(int k=0;k<8;k++)
    crc=(crc&0x8000) ? (WORD)((crc<<1)^0x1021) : (WORD)(crc<<1);
  return crc;
}


// what the ST would find on the revolution: sector IDs and their data
void FloppyBridgeCheckTrack(const BYTE *mfm,int nbits,TFloppyBridgeCheck &c) {
  memset(&c,0,sizeof(c));
  c.id_track=c.id_side=-1;
  if(!mfm || nbits<64)
    return;
  WORD reg=0;
  int syncs=0,last_sync=-100;
  int id_size=-1,id_end=-100000,id_sector=0;
  for(int i=0;i<nbits;i++)
  {
    reg=(WORD)((reg<<1)|MfmBit(mfm,i));
    if(reg!=0x4489)
      continue;
    syncs=(i-last_sync==16) ? syncs+1 : 1;
    last_sync=i;
    if(syncs<3)
      continue;
    int pos=i+1; // first cell of the address mark
    int mark=MfmByte(mfm,nbits,pos);
    if(mark<0)
      break;
    WORD crc=0xFFFF;
    crc=Crc16(crc,0xA1); crc=Crc16(crc,0xA1); crc=Crc16(crc,0xA1);
    crc=Crc16(crc,(BYTE)mark);
    pos+=16;
    if(mark==0xFE) // ID field
    {
      int f[6];
      for(int k=0;k<6;k++,pos+=16)
        f[k]=MfmByte(mfm,nbits,pos);
      if(f[5]<0)
        break;
      for(int k=0;k<4;k++)
        crc=Crc16(crc,(BYTE)f[k]);
      if(crc==(WORD)((f[4]<<8)|f[5]))
      {
        if(c.ids<FLOPPYBRIDGE_CHECK_MAX)
          c.id_list[c.ids]=(BYTE)f[2];
        c.ids++;
        c.id_track=f[0];
        c.id_side=f[1];
        id_size=f[3]&3;
        id_sector=f[2];
        id_end=pos;
      }
      else
        c.bad_ids++;
      i=pos-1;
      syncs=0;
    }
    else if(mark==0xFB || mark==0xF8) // data field
    {
      // only counted if it follows a good ID, as the FDC would read it
      if(id_size>=0 && pos-id_end<16*60)
      {
        int len=128<<id_size;
        bool ok=true;
        for(int k=0;k<len+2 && ok;k++,pos+=16)
        {
          int b=MfmByte(mfm,nbits,pos);
          if(b<0)
            ok=false;
          else if(k<len)
            crc=Crc16(crc,(BYTE)b);
          else if(k==len)
            ok=(b==(crc>>8));
          else
            ok=(b==(crc&0xFF));
        }
        if(ok)
          c.good++;
        else
        {
          if(c.bad<FLOPPYBRIDGE_CHECK_MAX)
            c.bad_list[c.bad]=(BYTE)id_sector;
          c.bad++;
        }
        i=pos-1;
      }
      id_size=-1;
      syncs=0;
    }
    else
      syncs=0;
  }
}


/*  The speed FloppyBridge gives for each byte comes from the raw flux timing
    and jitters by several % from byte to byte, also on a normal disk. Taken
    as is it would make every track "variable density". In auto mode it is
    averaged over 32 bytes and taken relative to the track's average (so a
    drive turning a bit fast or slow doesn't count); if no part of the track
    is more than 3% off, the track is nominal. A Copylock-style slow sector
    stands out clearly from that.
*/
void FloppyBridgeSmoothSpeeds(WORD *speed,int nbytes,int mode,
  int *min_out,int *max_out) {
  if(min_out) *min_out=1000;
  if(max_out) *max_out=1000;
  if(!speed || nbytes<=0)
    return;
  if(mode==FLOPPYBRIDGE_DENSITY_RAW)
  {
    int mn=3000,mx=0;
    for(int i=0;i<nbytes;i++)
    {
      if(speed[i]<mn) mn=speed[i];
      if(speed[i]>mx) mx=speed[i];
    }
    if(min_out) *min_out=mn;
    if(max_out) *max_out=mx;
    return;
  }
  if(mode==FLOPPYBRIDGE_DENSITY_OFF)
  {
    for(int i=0;i<nbytes;i++)
      speed[i]=1000;
    return;
  }
  const int W=32; // window, bytes
  std::vector<int> sm(nbytes);
  long long total=0;
  for(int i=0;i<nbytes;i++)
    total+=speed[i];
  long long sum=0;
  int count=0,lo=0,hi=0; // window [lo,hi)
  for(int i=0;i<nbytes;i++)
  {
    int want_lo=i-W/2,want_hi=i+W/2;
    if(want_lo<0) want_lo=0;
    if(want_hi>nbytes) want_hi=nbytes;
    while(hi<want_hi) { sum+=speed[hi++]; count++; }
    while(lo<want_lo) { sum-=speed[lo++]; count--; }
    sm[i]=(int)(sum/count);
  }
  int mean=(int)(total/nbytes);
  if(mean<=0)
    mean=1000;
  int mn=100000,mx=0;
  for(int i=0;i<nbytes;i++)
  {
    sm[i]=(int)(((long long)sm[i]*1000+mean/2)/mean);
    if(sm[i]<mn) mn=sm[i];
    if(sm[i]>mx) mx=sm[i];
  }
  if(min_out) *min_out=mn;
  if(max_out) *max_out=mx;
  bool nominal=(mn>=970 && mx<=1030);
  for(int i=0;i<nbytes;i++)
    speed[i]=(WORD)(nominal ? 1000 : sm[i]);
}


/////////////
// the drive
/////////////

void *FloppyBridgeOpen(const char *descriptor,const char *log_path,
  char *error,int error_size) {
  TFloppyBridgeSettings s;
  ParseDescriptor(descriptor,s);
  TFloppyBridgeDrive *drv=new TFloppyBridgeDrive;
  drv->api=NULL;
  drv->log=NULL;
  drv->t0=TFbClock::now();
  drv->disk_in=false;
  drv->density=SameText(s.density,"off") ? FLOPPYBRIDGE_DENSITY_OFF
    : SameText(s.density,"raw") ? FLOPPYBRIDGE_DENSITY_RAW
    : FLOPPYBRIDGE_DENSITY_AUTO;
  if(log_path && log_path[0] && !SameText(s.log,"off") && !SameText(s.log,"0"))
    drv->log=fopen(log_path,"w");
  Log(drv,"Steem FloppyBridge log - driver=%s port=%s drive=%s density=%s%s%s",
    s.driver,s.port,s.drive,s.density,s.config[0]?" config=":"",s.config);
  if(!FloppyBridgeAPI::isAvailable())
  {
#ifdef _WIN64
    SetError(error,error_size,"FloppyBridge_x64.dll not found: get it from amiga.robsmithdev.co.uk/winuae and put it next to Steem");
#else
    SetError(error,error_size,"FloppyBridge.dll not found: get it from amiga.robsmithdev.co.uk/winuae and put it next to Steem");
#endif
    Log(drv,"%s",error);
    FloppyBridgeClose(drv);
    return NULL;
  }
  FloppyBridgeAPI *api=NULL;
  if(s.config[0])
    api=FloppyBridgeAPI::createDriverFromString(s.config);
  else
  {
    int index=FindDriver(s.driver);
    if(index<0)
    {
      SetError(error,error_size,"FloppyBridge has no driver of that name (driver=)");
      Log(drv,"%s",error);
      FloppyBridgeClose(drv);
      return NULL;
    }
    api=FloppyBridgeAPI::createDriver((unsigned int)index);
    if(api)
    {
      if(SameText(s.port,"auto") || !s.port[0])
        api->setComPortAutoDetect(true);
      else
      {
        api->setComPortAutoDetect(false);
        api->setComPort(s.port);
      }
      FloppyBridge::DriveSelection sel=FloppyBridge::DriveSelection::dsDriveA;
      if(SameText(s.drive,"B")) sel=FloppyBridge::DriveSelection::dsDriveB;
      else if(SameText(s.drive,"0")) sel=FloppyBridge::DriveSelection::dsDrive0;
      else if(SameText(s.drive,"1")) sel=FloppyBridge::DriveSelection::dsDrive1;
      else if(SameText(s.drive,"2")) sel=FloppyBridge::DriveSelection::dsDrive2;
      else if(SameText(s.drive,"3")) sel=FloppyBridge::DriveSelection::dsDrive3;
      api->setDriveCableSelection(sel);
    }
  }
  if(!api)
  {
    SetError(error,error_size,"FloppyBridge couldn't create the driver");
    Log(drv,"%s",error);
    FloppyBridgeClose(drv);
    return NULL;
  }
  // whatever the config says, these are needed for an accurate ST drive
  api->setBridgeMode(FloppyBridge::BridgeMode::bmStalling);
  api->setBridgeDensityMode(FloppyBridge::BridgeDensityMode::bdmDDOnly);
  api->setSmartSpeedEnabled(false);
  api->setAutoCacheMode(true);
  if(!api->initialise())
  {
    SetError(error,error_size,api->getLastErrorMessage());
    Log(drv,"initialise failed: %s",error);
    delete api;
    FloppyBridgeClose(drv);
    return NULL;
  }
  drv->api=api;
  Log(drv,"connected, %d cylinders",(int)api->getMaxCylinder());
  api->resetDrive(0);
  api->setMotorStatus(false,true);
  // the motor spins up and FloppyBridge detects the disk in the background;
  // wait for that so the first read (the boot sector) finds the drive ready
  int ms=0;
  for(;ms<4000 && !(api->isMotorRunning() && api->isDiskInDrive());ms+=20)
    FbSleep(20);
  drv->disk_in=api->isDiskInDrive();
  Log(drv,"motor %s, disk %s after %d ms",api->isMotorRunning()?"on":"off",
    drv->disk_in?"in":"NOT detected",ms);
  return drv;
}


void FloppyBridgeClose(void *handle) {
  TFloppyBridgeDrive *drv=(TFloppyBridgeDrive*)handle;
  if(!drv)
    return;
  if(drv->api)
  {
    drv->api->setMotorStatus(false,false);
    drv->api->shutdown();
    delete drv->api;
    Log(drv,"drive closed");
  }
  if(drv->log)
    fclose(drv->log);
  delete drv;
}


int FloppyBridgeMaxCylinder(void *handle) {
  TFloppyBridgeDrive *drv=(TFloppyBridgeDrive*)handle;
  return (drv && drv->api) ? (int)drv->api->getMaxCylinder() : 0;
}


bool FloppyBridgeDiskInDrive(void *handle) {
  TFloppyBridgeDrive *drv=(TFloppyBridgeDrive*)handle;
  if(!drv || !drv->api)
    return false;
  bool in=drv->api->isDiskInDrive();
  if(in!=drv->disk_in)
  {
    Log(drv,"disk %s",in?"inserted":"removed (or not detected)");
    drv->disk_in=in;
  }
  return in;
}


static void FormatCheck(const TFloppyBridgeCheck &c,char *out,size_t size) {
  int n=snprintf(out,size,"%d IDs",c.ids);
  if(c.ids && (size_t)n<size)
  {
    n+=snprintf(out+n,size-n," (trk %d side %d, sectors",c.id_track,c.id_side);
    for(int k=0;k<c.ids && k<FLOPPYBRIDGE_CHECK_MAX && (size_t)n<size;k++)
      n+=snprintf(out+n,size-n," %d",c.id_list[k]);
    if((size_t)n<size)
      n+=snprintf(out+n,size-n,")");
  }
  if((size_t)n<size)
    n+=snprintf(out+n,size-n,", data OK %d, bad %d",c.good,c.bad);
  for(int k=0;k<c.bad && k<FLOPPYBRIDGE_CHECK_MAX && (size_t)n<size;k++)
    n+=snprintf(out+n,size-n,"%s%d",k?",":" (",c.bad_list[k]);
  if(c.bad && (size_t)n<size)
    n+=snprintf(out+n,size-n,")");
  if(c.bad_ids && (size_t)n<size)
    snprintf(out+n,size-n,", %d bad ID CRC",c.bad_ids);
}


int FloppyBridgeReadTrack(void *handle,int side,int track,
  unsigned char *mfm,unsigned short *speed,int max_bytes) {
  TFloppyBridgeDrive *drv=(TFloppyBridgeDrive*)handle;
  if(!drv || !drv->api || !mfm || !speed || max_bytes<=0)
    return 0;
  FloppyBridgeAPI *api=drv->api;
  bool upper=(side!=0); // ST side 1 = upper head (head 1)
  TFbClock::time_point start=TFbClock::now();
  api->setMotorStatus(upper,true);
  api->gotoCylinder(track,upper);
/*  In stalling mode getMFMTrack waits up to 450ms for the revolution, but
    returns 0 at once while the motor spins up or the disk isn't detected.
    So we wait on the clock, up to 5s (motor, seek, a few revolutions).
    If the revolution has bad sectors, up to 2 more are read and the best
    one is kept: a real FDC would retry too.
*/
  std::vector<BYTE> tmp(max_bytes);
  int best_bits=0,best_score=-1,revs=0;
  TFloppyBridgeCheck best_check;
  memset(&best_check,0,sizeof(best_check));
  while(revs<3 && ElapsedMs(start)<5000)
  {
    if(revs)
    {
      // next revolution: FloppyBridge keeps reading the track in the
      // background; after one revolution (200ms) the next buffer is ready
      // (isMFMDataAvailable() is always true in stalling mode, no use here)
      FbSleep(220);
      api->mfmSwitchBuffer(upper);
    }
    int bits=api->getMFMTrack(upper,(unsigned int)track,false,max_bytes,
      &tmp[0]);
    if(bits<=0)
    {
      FbSleep(10);
      continue;
    }
    if(bits>max_bytes*8)
      bits=max_bytes*8;
    revs++;
    TFloppyBridgeCheck c;
    FloppyBridgeCheckTrack(&tmp[0],bits,c);
    int score=c.good*4+c.ids-c.bad*2;
    if(score>best_score)
    {
      best_score=score;
      best_bits=bits;
      best_check=c;
      memcpy(mfm,&tmp[0],(bits+7)/8);
      // getMFMSpeed reads the revolution just returned: one speed per byte
      int nbytes=(bits+7)/8;
      for(int i=0;i<nbytes;i++)
      {
        int sp=api->getMFMSpeed(i*8);
        if(sp<700 || sp>3000) // turbo (100), not ready or garbage: nominal
          sp=FLOPPYBRIDGE_SPEED_NOMINAL;
        speed[i]=(unsigned short)sp;
      }
    }
    // a good track (or a blank or unusual one: retrying won't help)
    if(c.bad==0 && c.bad_ids==0)
      break;
  }
  if(!best_bits)
  {
    Log(drv,"track %2d side %d: NO DATA after %d ms (motor %s, ready %s, disk %s)",
      track,side,ElapsedMs(start),api->isMotorRunning()?"on":"off",
      api->isReady()?"yes":"no",api->isDiskInDrive()?"in":"not detected");
    return 0;
  }
  int mn,mx;
  FloppyBridgeSmoothSpeeds(speed,(best_bits+7)/8,drv->density,&mn,&mx);
  char info[300];
  FormatCheck(best_check,info,sizeof(info));
  bool density=false;
  for(int i=0;i<(best_bits+7)/8 && !density;i++)
    density=(speed[i]!=1000);
  Log(drv,"track %2d side %d: %d bits, %d ms, %d rev, speed %d-%d%s | %s",
    track,side,best_bits,ElapsedMs(start),revs,mn,mx,
    density?" VARIABLE":"",info);
  return best_bits;
}

#endif//#if defined(SSE_FLOPPYBRIDGE)
