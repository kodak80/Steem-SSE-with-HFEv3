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

struct TFloppyBridgeDrive {
  FloppyBridgeAPI *api;
};


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
  char driver[64],port[64],drive[16];
  char config[1024];
  TFloppyBridgeSettings() {
    strcpy(driver,"greaseweazle");
    strcpy(port,"auto");
    strcpy(drive,"A");
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


void *FloppyBridgeOpen(const char *descriptor,char *error,int error_size) {
  if(!FloppyBridgeAPI::isAvailable())
  {
#ifdef _WIN64
    SetError(error,error_size,"FloppyBridge_x64.dll not found: get it from amiga.robsmithdev.co.uk/winuae and put it next to Steem");
#else
    SetError(error,error_size,"FloppyBridge.dll not found: get it from amiga.robsmithdev.co.uk/winuae and put it next to Steem");
#endif
    return NULL;
  }
  TFloppyBridgeSettings s;
  ParseDescriptor(descriptor,s);
  FloppyBridgeAPI *api=NULL;
  if(s.config[0])
    api=FloppyBridgeAPI::createDriverFromString(s.config);
  else
  {
    int index=FindDriver(s.driver);
    if(index<0)
    {
      SetError(error,error_size,"FloppyBridge has no driver of that name (driver=)");
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
    delete api;
    return NULL;
  }
  api->resetDrive(0);
  api->setMotorStatus(false,true);
  // FloppyBridge detects the disk in the background once the motor runs;
  // give it a moment so the first read (the boot sector) finds it
  for(int ms=0;ms<3000 && !api->isDiskInDrive();ms+=50)
    Sleep(50);
  TFloppyBridgeDrive *drv=new TFloppyBridgeDrive;
  drv->api=api;
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
  }
  delete drv;
}


int FloppyBridgeMaxCylinder(void *handle) {
  TFloppyBridgeDrive *drv=(TFloppyBridgeDrive*)handle;
  return (drv && drv->api) ? (int)drv->api->getMaxCylinder() : 0;
}


bool FloppyBridgeDiskInDrive(void *handle) {
  TFloppyBridgeDrive *drv=(TFloppyBridgeDrive*)handle;
  return drv && drv->api && drv->api->isDiskInDrive();
}


int FloppyBridgeReadTrack(void *handle,int side,int track,
  unsigned char *mfm,unsigned short *speed,int max_bytes) {
  TFloppyBridgeDrive *drv=(TFloppyBridgeDrive*)handle;
  if(!drv || !drv->api || !mfm || !speed || max_bytes<=0)
    return 0;
  FloppyBridgeAPI *api=drv->api;
  bool upper=(side!=0); // ST side 1 = upper head (head 1)
  api->setMotorStatus(upper,true);
  api->gotoCylinder(track,upper);
  // in stalling mode each call waits up to ~450ms for the revolution; the
  // first track also has to wait for the motor and the seek
  int bits=0;
  for(int attempt=0;attempt<10 && !bits;attempt++)
    bits=api->getMFMTrack(upper,(unsigned int)track,false,max_bytes,mfm);
  if(bits<=0)
    return 0;
  if(bits>max_bytes*8)
    bits=max_bytes*8;
  // getMFMSpeed reads the revolution just returned (same track and side):
  // one speed per byte
  int nbytes=(bits+7)/8;
  for(int i=0;i<nbytes;i++)
  {
    int sp=api->getMFMSpeed(i*8);
    if(sp<700 || sp>3000) // turbo (100), not ready or garbage: nominal
      sp=FLOPPYBRIDGE_SPEED_NOMINAL;
    speed[i]=(unsigned short)sp;
  }
  return bits;
}

#endif//#if defined(SSE_FLOPPYBRIDGE)
