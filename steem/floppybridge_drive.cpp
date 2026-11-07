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
  cache=off             ; off (default) or on: FloppyBridge reads ahead in the
                        ; background, also while Steem is stopped
  write=off             ; off (default) or on: write what the ST writes to the
                        ; disk, a whole track at a time (FloppyBridgeWriteTrack)
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
#include <time.h>
#include <algorithm>

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
  bool write;   // write=on
  int rev_bits; // length of a revolution, from the last track read
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
  char driver[64],port[64],drive[16],log[16],density[16],cache[16],write[16];
  char config[1024];
  TFloppyBridgeSettings() {
    strcpy(driver,"greaseweazle");
    strcpy(port,"auto");
    strcpy(drive,"A");
    strcpy(log,"on");
    strcpy(density,"auto");
    strcpy(cache,"off");
    strcpy(write,"off");
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
      else if(SameText(key,"cache")) { dst=s.cache; dst_size=sizeof(s.cache); }
      else if(SameText(key,"write")) { dst=s.write; dst_size=sizeof(s.write); }
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
  int id_size=-1,id_end=-100000,id_sector=0,id_pos=0;
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
        id_pos=i-47; // first of the 3 syncs
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
        int data_start=pos;
        bool ok=true;
        int stored=0;
        for(int k=0;k<len+2;k++,pos+=16)
        {
          int b=MfmByte(mfm,nbits,pos);
          if(b<0)
          {
            ok=false;
            break;
          }
          if(k<len)
            crc=Crc16(crc,(BYTE)b);
          else
            stored=(stored<<8)|b;
        }
        if(ok)
          ok=(stored==crc);
        if(c.secs<FLOPPYBRIDGE_CHECK_MAX)
        {
          c.sec_id[c.secs]=(BYTE)id_sector;
          c.sec_start[c.secs]=data_start;
          c.sec_end[c.secs]=data_start+len*16;
          c.sec_id_pos[c.secs]=id_pos;
          c.sec_crc[c.secs]=crc;
          c.sec_good[c.secs]=ok;
          c.secs++;
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


void FloppyBridgeSectorSpeeds(const TFloppyBridgeCheck &c,const WORD *speed,
  int nbytes,char *out,int size) {
  if(!out || size<=0)
    return;
  out[0]=0;
  if(!speed || nbytes<=0)
    return;
  long long total=0;
  for(int i=0;i<nbytes;i++)
    total+=speed[i];
  long long mean=total/nbytes;
  if(mean<=0)
    return;
  int n=0;
  for(int k=0;k<c.secs && n<size;k++)
  {
    int b0=c.sec_start[k]/8,b1=c.sec_end[k]/8; // MFM bytes
    if(b0<0) b0=0;
    if(b1>nbytes) b1=nbytes;
    if(b1<=b0)
      continue;
    long long sum=0;
    for(int i=b0;i<b1;i++)
      sum+=speed[i];
    int rel=(int)((sum*1000/(b1-b0)+mean/2)/mean);
    n+=snprintf(out+n,size-n,"%s%d:%d",n?" ":"",c.sec_id[k],rel);
  }
}


static int SectorAverage(const TFloppyBridgeCheck &c,int k,const WORD *speed,
  int nbytes) {
  int b0=c.sec_start[k]/8,b1=c.sec_end[k]/8; // MFM bytes
  if(b0<0) b0=0;
  if(b1>nbytes) b1=nbytes;
  if(b1<=b0)
    return -1;
  long long sum=0;
  for(int i=b0;i<b1;i++)
    sum+=speed[i];
  return (int)(sum/(b1-b0));
}


int FloppyBridgeSectorDeviation(const TFloppyBridgeCheck &c,const WORD *speed,
  int nbytes) {
  if(!speed || c.secs<3)
    return -1;
  std::vector<int> avg;
  for(int k=0;k<c.secs;k++)
  {
    int a=SectorAverage(c,k,speed,nbytes);
    if(a>0)
      avg.push_back(a);
  }
  if(avg.size()<3)
    return -1;
  std::vector<int> sorted=avg;
  std::sort(sorted.begin(),sorted.end());
  int median=sorted[sorted.size()/2];
  int dev=0;
  for(size_t k=0;k<avg.size();k++)
  {
    int d=abs(avg[k]-median)*1000/median;
    if(d>dev)
      dev=d;
  }
  return dev;
}


/*  The speed FloppyBridge gives for each byte comes from the raw flux timing
    and jitters by several % from byte to byte, also on a normal disk, and
    the drive's speed wobbles a few % during a revolution.
    In auto mode the speeds are averaged over 32 bytes and taken relative to
    the track's average (so a drive turning a bit fast or slow doesn't
    count). Whether the track is kept as variable density is decided per
    sector: if one sector's data reads 4% or more slower or faster than the
    track's median sector, it's a density protection, else the track is
    nominal. Measured on real disks (Greaseweazle): ordinary sectors stay
    within 3.3% of the median, the Copylock sector of Total Recall was 5.5%
    slower. Tracks without sectors: nominal unless some part is 8% off.
    (The first version used "any 32-byte stretch 3% off" and flagged most
    ordinary tracks: drive wobble and gaps reach 6-7% over 32 bytes.)
*/
#define FLOPPYBRIDGE_SECTOR_THRESHOLD 40 // 4%
void FloppyBridgeSmoothSpeeds(WORD *speed,int nbytes,int mode,
  int *min_out,int *max_out,const TFloppyBridgeCheck *c) {
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
  int dev=c ? FloppyBridgeSectorDeviation(*c,speed,nbytes) : -1;
  bool nominal=(dev>=0) ? (dev<FLOPPYBRIDGE_SECTOR_THRESHOLD)
    : (mn>=920 && mx<=1080);
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
  drv->write=SameText(s.write,"on")||SameText(s.write,"1");
  drv->rev_bits=0;
  drv->density=SameText(s.density,"off") ? FLOPPYBRIDGE_DENSITY_OFF
    : SameText(s.density,"raw") ? FLOPPYBRIDGE_DENSITY_RAW
    : FLOPPYBRIDGE_DENSITY_AUTO;
  if(log_path && log_path[0] && !SameText(s.log,"off") && !SameText(s.log,"0"))
  {
    // appended, so all the disks of a session (disk swaps re-open the
    // descriptor) are in the log
    drv->log=fopen(log_path,"a");
    if(drv->log)
    {
      time_t now=time(NULL);
      char when[64]="";
      strftime(when,sizeof(when),"%Y-%m-%d %H:%M:%S",localtime(&now));
      fprintf(drv->log,"\n---- %s inserted ----\n",when);
    }
  }
  Log(drv,"Steem FloppyBridge log - driver=%s port=%s drive=%s density=%s cache=%s write=%s%s%s",
    s.driver,s.port,s.drive,s.density,s.cache,drv->write?"on":"off",
    s.config[0]?" config=":"",s.config);
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
/*  FloppyBridge's auto-cache reads the rest of the disk in the background
    whenever the emulated motor is off - turning the real motor on itself,
    also while Steem is stopped, before Play. It can only be set before the
    driver is opened. Off by default: the drive only spins when the ST's
    does, like a real ST (each new track then costs a revolution or two of
    real time; the ST doesn't see it). cache=on for faster loading.
*/
  api->setAutoCacheMode(SameText(s.cache,"on")||SameText(s.cache,"1"));
  if(!api->initialise())
  {
    SetError(error,error_size,api->getLastErrorMessage());
    Log(drv,"initialise failed: %s",error);
    delete api;
    FloppyBridgeClose(drv);
    return NULL;
  }
  drv->api=api;
  // The motor stays off until the ST reads the disk (it used to be turned
  // on here, and FloppyBridge then read the disk while Steem was stopped).
  api->setMotorStatus(false,false);
  drv->disk_in=api->isDiskInDrive();
  Log(drv,"connected, %d cylinders, motor off until the ST reads",
    (int)api->getMaxCylinder());
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


void FloppyBridgeMotorOff(void *handle) {
  TFloppyBridgeDrive *drv=(TFloppyBridgeDrive*)handle;
  if(!drv || !drv->api || !drv->api->isMotorRunning())
    return;
  drv->api->setMotorStatus(false,false);
  Log(drv,"motor off");
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
  bool was_running=api->isMotorRunning();
  if(!was_running)
    Log(drv,"motor on");
  api->setMotorStatus(upper,true);
  api->gotoCylinder(track,upper);
/*  In stalling mode getMFMTrack waits up to 450ms for the revolution, but
    returns 0 at once while the motor spins up or the disk isn't detected.
    So we wait on the clock, up to 5s (motor, seek, a few revolutions).
    If the revolution has bad sectors, up to 2 more are read and the best
    one is kept: a real FDC would retry too.
*/
  std::vector<BYTE> tmp(max_bytes);
  std::vector<WORD> tmp_speed(max_bytes);
  int best_bits=0,best_score=-1,revs=0;
  std::vector<WORD> best_raw; // speeds before smoothing, for the log
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
      // no disk: give up at once if the motor was already running (the
      // disk would have been detected), else after the spin-up
      if(!api->isDiskInDrive() && (was_running || ElapsedMs(start)>2500))
        break;
      FbSleep(10);
      continue;
    }
    if(bits>max_bytes*8)
      bits=max_bytes*8;
    revs++;
    TFloppyBridgeCheck c;
    FloppyBridgeCheckTrack(&tmp[0],bits,c);
/*  getMFMSpeed returns "garbage" until FloppyBridge is ready, and on the
    first track after the motor starts it gets ready a moment after
    getMFMTrack already returned the data (m_firstTrackMode is cleared when
    its background read finishes). Taking the speeds then gave a flat 1000
    and Total Recall's Copylock sector wasn't slower any more -> the game
    halted. So: wait until ready, and if the speeds are still missing, read
    another revolution.
*/
    for(int w=0;w<1000 && !api->isReady();w+=5)
      FbSleep(5);
    // getMFMSpeed reads the revolution just returned: one speed per byte
    int nbytes=(bits+7)/8,garbage=0;
    for(int i=0;i<nbytes;i++)
    {
      int sp=api->getMFMSpeed(i*8);
      if(sp<700 || sp>3000) // turbo (100), not ready or garbage: nominal
      {
        sp=FLOPPYBRIDGE_SPEED_NOMINAL;
        garbage++;
      }
      tmp_speed[i]=(unsigned short)sp;
    }
    bool speed_ok=(garbage<nbytes/2);
    if(!speed_ok)
      Log(drv,"track %2d side %d: no speed data on rev %d (ready %s)",track,side,
        revs,api->isReady()?"yes":"no");
    // revolutions with speed data first, then the one with the best sectors
    int score=(speed_ok?100000:0)+c.good*4+c.ids-c.bad*2;
    if(score>best_score)
    {
      best_score=score;
      best_bits=bits;
      best_check=c;
      memcpy(mfm,&tmp[0],nbytes);
      memcpy(speed,&tmp_speed[0],nbytes*sizeof(WORD));
    }
    // a good track (or a blank or unusual one: retrying won't help)
    if(c.bad==0 && c.bad_ids==0 && speed_ok)
      break;
  }
  if(!best_bits)
  {
    Log(drv,"track %2d side %d: NO DATA after %d ms (motor %s, ready %s, disk %s)",
      track,side,ElapsedMs(start),api->isMotorRunning()?"on":"off",
      api->isReady()?"yes":"no",api->isDiskInDrive()?"in":"not detected");
    return 0;
  }
  drv->rev_bits=best_bits;
  int mn,mx;
  best_raw.assign(speed,speed+(best_bits+7)/8);
  FloppyBridgeSmoothSpeeds(speed,(best_bits+7)/8,drv->density,&mn,&mx,
    &best_check);
  char info[300];
  FormatCheck(best_check,info,sizeof(info));
  bool density=false;
  for(int i=0;i<(best_bits+7)/8 && !density;i++)
    density=(speed[i]!=1000);
  Log(drv,"track %2d side %d: %d bits, %d ms, %d rev, speed %d-%d%s | %s",
    track,side,best_bits,ElapsedMs(start),revs,mn,mx,
    density?" VARIABLE":"",info);
  // where a protection would be: speed of each sector
  if(density || track==0)
  {
    char sp[400];
    FloppyBridgeSectorSpeeds(best_check,&best_raw[0],(int)best_raw.size(),
      sp,sizeof(sp));
    Log(drv,"          sector speed (track average 1000): %s",sp);
  }
  return best_bits;
}

///////////
// writing
///////////

static inline void SetMfmBit(BYTE *mfm,int i,int b) {
  if(b)
    mfm[i>>3]|=(BYTE)(0x80>>(i&7));
  else
    mfm[i>>3]&=(BYTE)~(0x80>>(i&7));
}


int FloppyBridgePrepareWrite(const BYTE *mfm,int nbits,int rev_bits,
  BYTE *out,int out_size,int *gap_bits,int *rotate_bits) {
  if(gap_bits) *gap_bits=0;
  if(rotate_bits) *rotate_bits=0;
  if(!mfm || nbits<16 || !out)
    return 0;
  if(rev_bits<=0)
    rev_bits=nbits;
  // the largest gap: between the end of a data field (+CRC) and the
  // preamble (12 x $00) of the next sector's ID, going round the track
  TFloppyBridgeCheck c;
  FloppyBridgeCheckTrack(mfm,nbits,c);
  int cut=0,gap=nbits; // no sectors: no constraint
  if(c.secs>=1)
  {
    gap=-1;
    for(int k=0;k<c.secs;k++)
    {
      int end=c.sec_end[k]+32; // after the CRC
      int next=(k+1<c.secs) ? c.sec_id_pos[k+1] : c.sec_id_pos[0]+nbits;
      int g=next-12*16-end;
      if(g>gap)
      {
        gap=g;
        cut=end+16*((g/2)/16); // whole bytes after the data: same MFM phase
      }
    }
    if(gap<0)
      gap=0;
    cut%=nbits;
  }
  // ~1% more than a revolution, by padding with gap or cutting into it;
  // each side of the cut has gap/2 bits to give
  int target=rev_bits+rev_bits/100;
  int room=(gap/2-32)/16*16;
  if(room<0)
    room=0;
  int delta=target-nbits;
  if(delta>room) delta=room;
  if(delta<-room) delta=-room;
  delta=delta/16*16;
  int total=nbits+delta;
  if(total<=0 || (total+7)/8>out_size)
    return 0;
  memset(out,0,(total+7)/8);
  for(int i=0;i<total;i++)
  {
    // past the end of the track (padding): repeat the gap the write
    // started with, so where it overwrites the start it writes the same
    int src=(i<nbits) ? cut+i : cut+(i-nbits)%(gap/2>16?gap/2:16);
    SetMfmBit(out,i,MfmBit(mfm,src%nbits));
  }
  if(gap_bits) *gap_bits=gap;
  if(rotate_bits) *rotate_bits=cut;
  return total;
}


bool FloppyBridgeCanWrite(void *handle) {
  TFloppyBridgeDrive *drv=(TFloppyBridgeDrive*)handle;
  return drv && drv->api && drv->write && !drv->api->isWriteProtected();
}


// every good sector of what we wrote must read back, same ID, same data
static bool SameSectors(const TFloppyBridgeCheck &wrote,
  const TFloppyBridgeCheck &read,int *missing) {
  int miss=0;
  for(int k=0;k<wrote.secs;k++)
  {
    if(!wrote.sec_good[k])
      continue;
    bool found=false;
    for(int j=0;j<read.secs && !found;j++)
      found=(read.sec_good[j] && read.sec_id[j]==wrote.sec_id[k]
        && read.sec_crc[j]==wrote.sec_crc[k]);
    if(!found)
      miss++;
  }
  if(missing)
    *missing=miss;
  return !miss && read.ids>=wrote.ids;
}


int FloppyBridgeWriteTrack(void *handle,int side,int track,const BYTE *mfm,
  int nbits,int flags) {
  TFloppyBridgeDrive *drv=(TFloppyBridgeDrive*)handle;
  if(!drv || !drv->api || !mfm || nbits<16)
    return -1;
  FloppyBridgeAPI *api=drv->api;
  if(!drv->write)
  {
    Log(drv,"track %2d side %d: written by the ST, NOT saved to the disk (write=off)",
      track,side);
    return -1;
  }
  // tracks we can't rewrite faithfully stay as they are on the disk (the
  // ST still sees its changes until the disk is removed)
  if(!(flags&FLOPPYBRIDGE_WRITE_FORMATTED))
  {
    if(flags&FLOPPYBRIDGE_WRITE_VARIABLE)
    {
      Log(drv,"track %2d side %d: NOT written - variable density (copy protection), rewriting it would destroy it",
        track,side);
      return -1;
    }
    if(flags&FLOPPYBRIDGE_WRITE_UNREAD)
    {
      Log(drv,"track %2d side %d: NOT written - the track couldn't be read, so the rest of it isn't known",
        track,side);
      return -1;
    }
  }
  bool upper=(side!=0);
  api->setMotorStatus(upper,true);
  if(api->isWriteProtected())
  {
    Log(drv,"track %2d side %d: NOT written - disk is write-protected",track,side);
    return -1;
  }
  int rev=drv->rev_bits ? drv->rev_bits : nbits;
  std::vector<BYTE> buf(nbits/8+rev/8+64);
  int gap=0,rotate=0;
  int total=FloppyBridgePrepareWrite(mfm,nbits,rev,&buf[0],(int)buf.size(),
    &gap,&rotate);
  if(!total)
    return 0;
  TFloppyBridgeCheck wrote;
  FloppyBridgeCheckTrack(mfm,nbits,wrote);
  int result=0;
  for(int attempt=0;attempt<2 && result!=1;attempt++)
  {
    TFbClock::time_point start=TFbClock::now();
    // whole bytes: pad the last one with the bits that follow (gap)
    int nbytes=(total+7)/8;
    if(!api->writeMFMTrackToBuffer(upper,(unsigned int)track,true,nbytes,&buf[0]))
    {
      Log(drv,"track %2d side %d: write request refused by FloppyBridge",track,side);
      continue;
    }
    // queued: wait for the write, then FloppyBridge reads the track back
    int ms=0;
    for(;ms<5000 && api->isWritePending();ms+=10)
      FbSleep(10);
    if(api->isWriteProtected())
    {
      Log(drv,"track %2d side %d: NOT written - disk is write-protected",track,side);
      return -1;
    }
    // verify: read it back like the ST would
    std::vector<BYTE> back(buf.size()+8192);
    int bits=0;
    for(int w=0;w<5000 && bits<=0;w+=10)
    {
      bits=api->getMFMTrack(upper,(unsigned int)track,false,(int)back.size(),
        &back[0]);
      if(bits<=0)
        FbSleep(10);
    }
    TFloppyBridgeCheck read;
    memset(&read,0,sizeof(read));
    if(bits>0)
      FloppyBridgeCheckTrack(&back[0],bits,read);
    int missing=0;
    bool ok=(bits>0 && SameSectors(wrote,read,&missing));
    char info[300];
    FormatCheck(read,info,sizeof(info));
    Log(drv,"track %2d side %d: WRITTEN %d bits (gap %d, rotated %d), %d ms, verify %s | %s",
      track,side,total,gap/16,rotate/16,ElapsedMs(start),
      ok ? "OK" : (bits>0 ? "FAILED" : "NO DATA"),info);
    if(ok)
      result=1;
    else if(bits>0)
      Log(drv,"          %d sector(s) didn't read back the same%s",missing,
        attempt ? "" : ", writing again");
  }
  if(result!=1)
    Log(drv,"track %2d side %d: WRITE FAILED - the track on the disk may be damaged",
      track,side);
  return result;
}

#endif//#if defined(SSE_FLOPPYBRIDGE)
