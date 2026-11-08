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
FILE: greaseweazle_flux.cpp
CONDITION: SSE_GREASEWEAZLE_FLUX must be defined (device part: WIN32 only)
DESCRIPTION: Raw flux from a real drive on a Greaseweazle, see
greaseweazle_flux.h. The Greaseweazle protocol is Keir Fraser's (USB CDC
serial): a command is [cmd, length, params...], answered by [cmd, ack]
and maybe more bytes. READ_FLUX streams the flux as bytes:
  1-249        a transition, that many sample ticks after the previous one
  250-254, b   a transition, 250 + (n-250)*255 + b - 1 ticks after
  255, op, 28-bit value: op 1 = index pulse (value: ticks after the last
               transition), op 2 = space (value: ticks to add, no
               transition), op 3 = astable (ignored)
  0            end of the stream
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#if defined(SSE_GREASEWEAZLE_FLUX)

#include <greaseweazle_flux.h>
#include <floppybridge_drive.h> // FloppyBridgeCheckTrack, for the log
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <chrono>
#include <thread>

#ifdef WIN32
#include <windows.h>
#include <setupapi.h>
#ifdef _MSC_VER
#pragma comment(lib,"setupapi.lib")
#endif
#endif


/////////////////////
// pure functions //
/////////////////////

static unsigned int Read28(const unsigned char *p) {
  return (p[0]>>1) | ((unsigned)(p[1]&0xFE)<<6) | ((unsigned)(p[2]&0xFE)<<13)
    | ((unsigned)(p[3]&0xFE)<<20);
}


bool GwDecodeFluxStream(const unsigned char *s,int n,
  std::vector<unsigned long long> &flux,std::vector<unsigned long long> &index) {
  flux.clear();
  index.clear();
  unsigned long long last=0; // time of the last transition
  unsigned long long pending=0; // space since then
  int i=0;
  while(i<n)
  {
    unsigned char b=s[i];
    if(b==0)
      break; // end
    if(b==255)
    {
      if(i+6>n)
        return false;
      unsigned char op=s[i+1];
      unsigned int v=Read28(s+i+2);
      if(op==1)
        index.push_back(last+pending+v);
      else if(op==2)
        pending+=v;
      else if(op!=3)
        return false;
      i+=6;
      continue;
    }
    unsigned int v;
    if(b<250)
    {
      v=b;
      i++;
    }
    else
    {
      if(i+2>n)
        return false;
      v=250+(b-250)*255+s[i+1]-1;
      i+=2;
    }
    last+=pending+v;
    pending=0;
    flux.push_back(last);
  }
  return true;
}


int GwSplitRevolutions(const std::vector<unsigned long long> &flux,
  const std::vector<unsigned long long> &index,unsigned int sample_freq,
  TGwFluxRev *revs,int max_revs) {
  if(!sample_freq)
    return 0;
  int n=0;
  size_t f=0;
  for(size_t r=0;r+1<index.size() && n<max_revs;r++)
  {
    unsigned long long start=index[r],end=index[r+1];
    TGwFluxRev &rev=revs[n];
    rev.clear();
    while(f<flux.size() && flux[f]<=start)
      f++;
    size_t g=f;
    while(g<flux.size() && flux[g]<=end)
    {
      rev.push_back((unsigned int)((flux[g]-start)*(GWFLUX_UNITS_PER_US*1000000ull)
        /sample_freq));
      g++;
    }
    if(g<flux.size()) // continuity: first transition of the next revolution
      rev.push_back((unsigned int)((flux[g]-start)*(GWFLUX_UNITS_PER_US*1000000ull)
        /sample_freq));
    if(rev.size()<100)
      continue; // nothing there (no disk, unformatted)
    n++;
  }
  return n;
}


int GwFluxToBits(const TGwFluxRev &rev,unsigned int rev_units,
  unsigned char *mfm,int max_bytes) {
/*  The WD1772's digital PLL, as Steem emulates it (TWD1772Dpll::GetNextBit,
    from MAME), on the transitions converted to 8MHz cycles like the SCP
    reader does (units/5, the rest carried over), without the random wobble:
    what the ST would read, for the log.
*/
  memset(mfm,0,max_bytes);
  int nbits=0,maxbits=max_bytes*8;
  static const unsigned char pha[8]={0xf,0x7,0x3,0x1,0,0,0,0};
  static const unsigned char phs[8]={0,0,0,0,0x1,0x3,0x7,0xf};
  static const unsigned char freqa[4][8]={{0xf,0x7,0x3,0x1,0,0,0,0},
    {0x7,0x3,0x1,0,0,0,0,0},{0x7,0x3,0x1,0,0,0,0,0},{0,0,0,0,0,0,0,0}};
  static const unsigned char freqs[4][8]={{0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0x1,0x3,0x7},{0,0,0,0,0,0x1,0x3,0x7},{0,0,0,0,0x1,0x3,0x7,0xf}};
  unsigned int counter=0,increment=128,transition_time=0xffff;
  unsigned char history=0x80,slot=0,phase_add=0,phase_sub=0,freq_add=0,
    freq_sub=0;
  long long ctime=0,latest=0,tm=0;
  size_t ti=0;
  unsigned int prev=0,rest=0;
  long long t=0;
  for(;;)
  {
    while(ctime-latest>=0)
    {
      if(ti>=rev.size() || rev[ti]>rev_units)
        return nbits;
      unsigned int u=rev[ti]-prev;
      prev=rev[ti];
      ti++;
      unsigned int c=u/5;
      rest+=u%5;
      if(rest>5)
      {
        c++;
        rest-=5;
      }
      t+=c;
      latest=t;
    }
    long long when=latest;
    for(;;)
    {
      long long etime=ctime+slot+1;
      if(transition_time==0xffff && etime-when>=0)
        transition_time=counter;
      if(slot<8)
      {
        unsigned char mask=(unsigned char)(1<<slot);
        if(phase_add&mask)
          counter+=226;
        else if(phase_sub&mask)
          counter+=30;
        else
          counter+=increment;
        if((freq_add&mask) && increment<140)
          increment++;
        else if((freq_sub&mask) && increment>117)
          increment--;
      }
      else
        counter+=increment;
      slot++;
      tm=etime;
      if(counter&0x800)
        break;
    }
    int bit=(transition_time!=0xffff);
    if(bit)
    {
      int cslot=(transition_time>>8)&7;
      phase_add=pha[cslot];
      phase_sub=phs[cslot];
      int way=(transition_time&0x400) ? 1 : 0;
      if(history&0x80)
        history=way ? 0x80 : 0x83;
      else if(history&0x40)
        history=way ? (history&2) : ((history&2)|1);
      freq_add=freqa[history&3][cslot];
      freq_sub=freqs[history&3][cslot];
      history=way ? (unsigned char)((history>>1)|2) : (unsigned char)(history>>1);
    }
    else
      phase_add=phase_sub=freq_add=freq_sub=0;
    counter&=0x7ff;
    ctime=tm;
    transition_time=0xffff;
    slot=0;
    if(nbits>=maxbits)
      return nbits;
    if(bit)
      mfm[nbits/8]|=(unsigned char)(0x80>>(nbits%8));
    nbits++;
  }
}


#ifdef WIN32

//////////////////
// the device  //
//////////////////

enum {GW_GETINFO=0,GW_SEEK=2,GW_HEAD=3,GW_SETPARAMS=4,GW_GETPARAMS=5,
  GW_MOTOR=6,GW_READFLUX=7,GW_GETFLUXSTATUS=9,GW_SELECT=12,GW_DESELECT=13,
  GW_SETBUSTYPE=14,GW_SETPIN=15,GW_RESET=16,GW_GETPIN=20,GW_NOCLICKSTEP=22};
enum {GW_ACK_OKAY=0,GW_ACK_BADCOMMAND=1,GW_ACK_NOINDEX=2,GW_ACK_NOTRK0=3,
  GW_ACK_FLUXOVERFLOW=4};

struct TGwFlux {
  HANDLE port;
  FILE *log;
  std::chrono::steady_clock::time_point t0;
  unsigned int sample_freq;
  int fw_major,fw_minor;
  int unit; // drive select line
  int bus;  // 1 = IBM PC, 2 = Shugart
  int densel; // pin 2 (density select): -1 = left alone, 0 = low, 1 = high
  int revs,cylinders;
  int cyl; // where the head is (-1: unknown)
  bool motor,selected;
  bool dskchg_ok; // the disk-change line can be read
  bool noclick_ok; // firmware can reset it without a click
  bool disk_was_out;
  bool rereading;
};


static double ElapsedS(TGwFlux *gw) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now()-gw->t0)
    .count();
}


static void Log(TGwFlux *gw,const char *fmt,...) {
  if(!gw || !gw->log)
    return;
  fprintf(gw->log,"%7.3f  ",ElapsedS(gw));
  va_list args;
  va_start(args,fmt);
  vfprintf(gw->log,fmt,args);
  va_end(args);
  fputs("\n",gw->log);
  fflush(gw->log);
}


static void SetError(char *error,int size,const char *msg) {
  if(error && size>0)
  {
    strncpy(error,msg,size-1);
    error[size-1]=0;
  }
}


static bool PortWrite(TGwFlux *gw,const void *p,int n) {
  DWORD done=0;
  return WriteFile(gw->port,p,n,&done,NULL) && (int)done==n;
}


// read exactly n bytes (2s timeout)
static bool PortRead(TGwFlux *gw,void *p,int n) {
  unsigned char *b=(unsigned char*)p;
  int got=0,tries=0;
  while(got<n && tries<3)
  {
    DWORD done=0;
    if(!ReadFile(gw->port,b+got,n-got,&done,NULL))
      return false;
    if(!done)
      tries++;
    got+=done;
  }
  return got==n;
}


// send a command, get its ack (-1: no answer)
static int Command(TGwFlux *gw,int cmd,const unsigned char *params,int n) {
  unsigned char buf[64];
  buf[0]=(unsigned char)cmd;
  buf[1]=(unsigned char)(2+n);
  if(n)
    memcpy(buf+2,params,n);
  if(!PortWrite(gw,buf,2+n))
    return -1;
  unsigned char ack[2];
  if(!PortRead(gw,ack,2) || ack[0]!=cmd)
    return -1;
  return ack[1];
}


static int Command1(TGwFlux *gw,int cmd,int param) {
  unsigned char p=(unsigned char)param;
  return Command(gw,cmd,&p,1);
}


static const char *AckText(int ack) {
  static const char *text[]={"OK","bad command","no index","no track 0",
    "flux overflow","flux underflow","write-protected","no drive selected",
    "no bus type","bad drive","bad pin","bad cylinder"};
  if(ack<0)
    return "no answer";
  if(ack<(int)(sizeof(text)/sizeof(text[0])))
    return text[ack];
  return "error";
}


// back to a known state: bus reset (motor off, drive deselected)
static void SetDensel(TGwFlux *gw) {
  if(gw->densel<0)
    return;
  unsigned char p[2]={2,(unsigned char)gw->densel};
  int ack=Command(gw,GW_SETPIN,p,2);
  if(ack!=GW_ACK_OKAY)
    Log(gw,"density select (pin 2) %s refused: %s",gw->densel?"high":"low",
      AckText(ack));
}


static void Recover(TGwFlux *gw) {
  PurgeComm(gw->port,PURGE_RXCLEAR|PURGE_TXCLEAR);
  Command(gw,GW_RESET,NULL,0);
  Command1(gw,GW_SETBUSTYPE,gw->bus);
  SetDensel(gw);
  gw->selected=false;
  gw->motor=false;
  gw->cyl=-1;
  Log(gw,"Greaseweazle reset");
}


static void Select(TGwFlux *gw,bool on) {
  if(on==gw->selected)
    return;
  if(on)
    gw->selected=(Command1(gw,GW_SELECT,gw->unit)==GW_ACK_OKAY);
  else
  {
    Command(gw,GW_DESELECT,NULL,0);
    gw->selected=false;
  }
}


static int GetPin(TGwFlux *gw,int pin) {
  if(Command1(gw,GW_GETPIN,pin)!=GW_ACK_OKAY)
    return -1;
  unsigned char v=0;
  if(!PortRead(gw,&v,1))
    return -1;
  return v;
}


// a Greaseweazle among the serial ports (USB 1209:4D69)
static bool FindPort(char *port,int size) {
  static const GUID ports_class={0x4D36E978,0xE325,0x11CE,
    {0xBF,0xC1,0x08,0x00,0x2B,0xE1,0x03,0x18}};
  HDEVINFO list=SetupDiGetClassDevsA(&ports_class,NULL,NULL,DIGCF_PRESENT);
  if(list==INVALID_HANDLE_VALUE)
    return false;
  int best=0;
  SP_DEVINFO_DATA dev;
  dev.cbSize=sizeof(dev);
  for(DWORD i=0;SetupDiEnumDeviceInfo(list,i,&dev);i++)
  {
    char id[512]="";
    if(!SetupDiGetDeviceInstanceIdA(list,&dev,id,sizeof(id),NULL))
      continue;
    for(char *c=id;*c;c++)
      *c=(char)toupper((unsigned char)*c);
    int score=0;
    if(strstr(id,"VID_1209&PID_4D69"))
      score=2;
    else if(strstr(id,"VID_1209&PID_0001"))
      score=1; // old shared test PID
    if(score<=best)
      continue;
    HKEY key=SetupDiOpenDevRegKey(list,&dev,DICS_FLAG_GLOBAL,0,DIREG_DEV,
      KEY_READ);
    if(key==INVALID_HANDLE_VALUE)
      continue;
    char name[64]="";
    DWORD len=sizeof(name),type=0;
    if(RegQueryValueExA(key,"PortName",NULL,&type,(BYTE*)name,&len)
      ==ERROR_SUCCESS && !strncmp(name,"COM",3))
    {
      strncpy(port,name,size-1);
      port[size-1]=0;
      best=score;
    }
    RegCloseKey(key);
  }
  SetupDiDestroyDeviceInfoList(list);
  return best>0;
}


static void GetValue(const char *descriptor,const char *key,char *out,
  int size,const char *def) {
  strncpy(out,def,size-1);
  out[size-1]=0;
  size_t klen=strlen(key);
  const char *p=descriptor;
  while(*p)
  {
    while(*p==' ' || *p=='\t' || *p=='\r' || *p=='\n')
      p++;
    const char *line=p;
    while(*p && *p!='\n')
      p++;
    if(*line=='#' || *line==';')
      continue;
    bool match=true;
    for(size_t i=0;i<klen;i++)
      if(tolower((unsigned char)line[i])!=key[i])
        match=false;
    if(!match)
      continue;
    const char *v=line+klen;
    while(*v==' ' || *v=='\t')
      v++;
    if(*v!='=')
      continue;
    v++;
    while(*v==' ' || *v=='\t')
      v++;
    int n=0;
    while(v+n<p && v[n]!='\r' && v[n]!='#' && v[n]!=';' && n<size-1)
      n++;
    while(n && (v[n-1]==' ' || v[n-1]=='\t'))
      n--;
    memcpy(out,v,n);
    out[n]=0;
  }
}


void *GwFluxOpen(const char *descriptor,const char *log_path,char *error,
  int error_size) {
  char port[64],drive[16],log[16],revs[16],tracks[16],densel[16];
  GetValue(descriptor,"port",port,sizeof(port),"auto");
  GetValue(descriptor,"drive",drive,sizeof(drive),"A");
  GetValue(descriptor,"log",log,sizeof(log),"on");
  GetValue(descriptor,"revs",revs,sizeof(revs),"2");
  GetValue(descriptor,"tracks",tracks,sizeof(tracks),"82");
  GetValue(descriptor,"densel",densel,sizeof(densel),"auto");
  TGwFlux *gw=new TGwFlux();
  gw->port=INVALID_HANDLE_VALUE;
  gw->t0=std::chrono::steady_clock::now();
  gw->cyl=-1;
  gw->revs=atoi(revs);
  if(gw->revs<1) gw->revs=1;
  if(gw->revs>GWFLUX_MAX_REVS) gw->revs=GWFLUX_MAX_REVS;
  gw->densel=-1;
  if(!strcmp(densel,"low") || !strcmp(densel,"l") || !strcmp(densel,"0"))
    gw->densel=0;
  else if(!strcmp(densel,"high") || !strcmp(densel,"h") || !strcmp(densel,"1"))
    gw->densel=1;
  gw->cylinders=atoi(tracks);
  if(gw->cylinders<40) gw->cylinders=40;
  if(gw->cylinders>84) gw->cylinders=84;
  // drive: A/B (PC cable, after the twist), 0-3 (Shugart)
  char d=(char)toupper((unsigned char)drive[0]);
  gw->bus=1;
  gw->unit=0;
  if(d=='B')
    gw->unit=1;
  else if(d>='0' && d<='3')
  {
    gw->bus=2;
    gw->unit=d-'0';
  }
  if(log_path && strcmp(log,"off"))
  {
    gw->log=fopen(log_path,"a");
    if(gw->log)
    {
      time_t now=time(NULL);
      char when[64];
      strftime(when,sizeof(when),"%Y-%m-%d %H:%M:%S",localtime(&now));
      fprintf(gw->log,"\n---- %s inserted ----\n",when);
    }
  }
  Log(gw,"Steem Greaseweazle flux log - port=%s drive=%s revs=%d tracks=%d densel=%s",
    port,drive,gw->revs,gw->cylinders,densel);
  char name[80];
  if(!strcmp(port,"auto") || !port[0])
  {
    if(!FindPort(port,sizeof(port)))
    {
      SetError(error,error_size,"No Greaseweazle found (USB). Is it plugged in? You can also give its port in the descriptor, eg port=COM5.");
      Log(gw,"no Greaseweazle found");
      GwFluxClose(gw);
      return NULL;
    }
  }
  snprintf(name,sizeof(name),"\\\\.\\%s",port);
  gw->port=CreateFileA(name,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,
    0,NULL);
  if(gw->port==INVALID_HANDLE_VALUE)
  {
    char msg[200];
    snprintf(msg,sizeof(msg),"Can't open %s (in use by another program, eg FloppyBridge or DiskFlashback?).",port);
    SetError(error,error_size,msg);
    Log(gw,"%s",msg);
    GwFluxClose(gw);
    return NULL;
  }
  DCB dcb;
  memset(&dcb,0,sizeof(dcb));
  dcb.DCBlength=sizeof(dcb);
  GetCommState(gw->port,&dcb);
  dcb.BaudRate=9600; // ignored by USB CDC, but must not be 10000 (reset)
  dcb.ByteSize=8;
  dcb.Parity=NOPARITY;
  dcb.StopBits=ONESTOPBIT;
  dcb.fBinary=TRUE;
  dcb.fOutxCtsFlow=FALSE;
  dcb.fRtsControl=RTS_CONTROL_ENABLE;
  dcb.fDtrControl=DTR_CONTROL_ENABLE;
  SetCommState(gw->port,&dcb);
  COMMTIMEOUTS to;
  memset(&to,0,sizeof(to));
  to.ReadIntervalTimeout=MAXDWORD; // return what's there, wait for 1 byte
  to.ReadTotalTimeoutMultiplier=MAXDWORD;
  to.ReadTotalTimeoutConstant=2000;
  to.WriteTotalTimeoutConstant=2000;
  SetCommTimeouts(gw->port,&to);
  PurgeComm(gw->port,PURGE_RXCLEAR|PURGE_TXCLEAR);
  // firmware info: <4BI3B21x
  unsigned char info[32];
  bool ok=false;
  for(int tries=0;tries<2 && !ok;tries++)
  {
    ok=(Command1(gw,GW_GETINFO,0)==GW_ACK_OKAY && PortRead(gw,info,32));
    if(!ok)
      PurgeComm(gw->port,PURGE_RXCLEAR|PURGE_TXCLEAR);
  }
  if(!ok || !info[2])
  {
    SetError(error,error_size,ok ? "The Greaseweazle is in update mode."
      : "The device doesn't answer like a Greaseweazle.");
    Log(gw,"%s on %s",ok?"update mode":"no answer",port);
    GwFluxClose(gw);
    return NULL;
  }
  gw->fw_major=info[0];
  gw->fw_minor=info[1];
  gw->sample_freq=info[4]|(info[5]<<8)|(info[6]<<16)|((unsigned)info[7]<<24);
  Command(gw,GW_RESET,NULL,0);
  if(Command1(gw,GW_SETBUSTYPE,gw->bus)!=GW_ACK_OKAY)
  {
    SetError(error,error_size,"The Greaseweazle refused the drive bus type.");
    GwFluxClose(gw);
    return NULL;
  }
  SetDensel(gw);
  Select(gw,true);
  int chg=(gw->bus==1) ? GetPin(gw,34) : -1;
  int wp=GetPin(gw,28);
  gw->dskchg_ok=(chg>=0);
  Select(gw,false);
  Log(gw,"connected on %s, firmware %d.%d, sample clock %.3f MHz, disk-change line %s, write-protect %s",
    port,gw->fw_major,gw->fw_minor,gw->sample_freq/1e6,
    gw->dskchg_ok ? (chg ? "says disk in" : "says no disk/changed") : "not available",
    wp<0 ? "?" : (wp ? "off" : "on"));
  Log(gw,"motor off until the ST reads; read-only");
  return gw;
}


void GwFluxClose(void *handle) {
  TGwFlux *gw=(TGwFlux*)handle;
  if(!gw)
    return;
  if(gw->port!=INVALID_HANDLE_VALUE)
  {
    GwFluxMotorOff(gw);
    CloseHandle(gw->port);
    Log(gw,"drive closed");
  }
  if(gw->log)
    fclose(gw->log);
  delete gw;
}


int GwFluxRevs(void *handle) {
  return handle ? ((TGwFlux*)handle)->revs : 0;
}


int GwFluxCylinders(void *handle) {
  return handle ? ((TGwFlux*)handle)->cylinders : 0;
}


void GwFluxLogMessage(void *handle,const char *msg) {
  Log((TGwFlux*)handle,"%s",msg);
}


void GwFluxMotorOff(void *handle) {
  TGwFlux *gw=(TGwFlux*)handle;
  if(!gw || !gw->motor)
    return;
  unsigned char p[2]={(unsigned char)gw->unit,0};
  Command(gw,GW_MOTOR,p,2);
  Select(gw,false);
  gw->motor=false;
  Log(gw,"motor off");
}


int GwFluxReadTrack(void *handle,int side,int track,TGwFluxRev *revs,
  int max_revs) {
  TGwFlux *gw=(TGwFlux*)handle;
  if(!gw || track<0 || track>=gw->cylinders)
    return 0;
  auto start=std::chrono::steady_clock::now();
  // motor, select, seek; if the Greaseweazle refuses (eg it switched the
  // drive off itself after a long idle time), reset it and try once more
  for(int attempt=0;;attempt++)
  {
    int ack=GW_ACK_OKAY;
    const char *what="";
    if(!gw->motor)
    {
      unsigned char p[2]={(unsigned char)gw->unit,1};
      ack=Command(gw,GW_MOTOR,p,2); // waits for spin-up
      what="motor on";
      if(ack==GW_ACK_OKAY)
      {
        gw->motor=true;
        Log(gw,"motor on");
      }
    }
    if(ack==GW_ACK_OKAY)
    {
      Select(gw,true);
      if(gw->cyl!=track)
      {
        // where the head is isn't known (after a disk change, a reset...):
        // back to track 0 first, the drive's sensor tells where it is
        if(gw->cyl<0 && track)
          ack=Command1(gw,GW_SEEK,0);
        if(ack==GW_ACK_OKAY)
          ack=Command1(gw,GW_SEEK,track);
        what="seek";
        if(ack==GW_ACK_OKAY)
          gw->cyl=track;
      }
    }
    if(ack==GW_ACK_OKAY)
      break;
    Log(gw,"%s (track %d) failed: %s",what,track,AckText(ack));
    if(attempt)
      return 0;
    Recover(gw);
  }
  Command1(gw,GW_HEAD,side);
  int nrevs=gw->revs<max_revs ? gw->revs : max_revs;
  // READ_FLUX <IHI: ticks (0 = no limit), index pulses, linger after the
  // last one (1ms: the transition after the index, for continuity)
  unsigned char p[10];
  unsigned int linger=gw->sample_freq/1000;
  memset(p,0,sizeof(p));
  p[4]=(unsigned char)(nrevs+1);
  p[6]=(unsigned char)linger;
  p[7]=(unsigned char)(linger>>8);
  p[8]=(unsigned char)(linger>>16);
  p[9]=(unsigned char)(linger>>24);
  std::vector<unsigned char> stream;
  stream.reserve(200000*(nrevs+1));
  int ack=Command(gw,GW_READFLUX,p,10);
  bool ended=false;
  if(ack==GW_ACK_OKAY)
  {
    unsigned char buf[4096];
    int idle=0;
    while(!ended && idle<3)
    {
      DWORD done=0;
      if(!ReadFile(gw->port,buf,sizeof(buf),&done,NULL))
        break;
      if(!done)
      {
        idle++; // 2s each
        continue;
      }
      idle=0;
      for(DWORD i=0;i<done;i++)
        if(buf[i]==0)
        {
          // a 0 can't be part of a value: 250-254 is followed by 1-255, the
          // 28-bit values have bit 0 set
          stream.insert(stream.end(),buf,buf+i);
          ended=true;
          break;
        }
      if(!ended)
        stream.insert(stream.end(),buf,buf+done);
    }
  }
  int status=Command(gw,GW_GETFLUXSTATUS,NULL,0);
  double ms=std::chrono::duration<double,std::milli>(
    std::chrono::steady_clock::now()-start).count();
  if(ack!=GW_ACK_OKAY || !ended || status!=GW_ACK_OKAY)
  {
    Log(gw,"track %2d side %d: read failed (%s)",track,side,
      status==GW_ACK_NOINDEX ? "no index pulse: no disk?"
      : status==GW_ACK_FLUXOVERFLOW ? "USB too slow: flux overflow"
      : AckText(ack!=GW_ACK_OKAY ? ack : status));
    if(!ended || status<0)
      Recover(gw); // resync the protocol
    return 0;
  }
  std::vector<unsigned long long> flux,index;
  if(!GwDecodeFluxStream(stream.data(),(int)stream.size(),flux,index))
  {
    Log(gw,"track %2d side %d: malformed flux stream",track,side);
    return 0;
  }
  int n=GwSplitRevolutions(flux,index,gw->sample_freq,revs,nrevs);
  // what the ST would find on the first revolution
  TFloppyBridgeCheck c;
  memset(&c,0,sizeof(c));
  double rev_ms=0;
  int rev_ids[GWFLUX_MAX_REVS]={0},rev_good[GWFLUX_MAX_REVS]={0},
    rev_bad[GWFLUX_MAX_REVS]={0};
  if(n && index.size()>=2)
  {
    rev_ms=(index[1]-index[0])*1000.0/gw->sample_freq;
    unsigned int rev_units=(unsigned int)((index[1]-index[0])
      *(GWFLUX_UNITS_PER_US*1000000ull)/gw->sample_freq);
    static unsigned char mfm[32768];
    int nbits=GwFluxToBits(revs[0],rev_units,mfm,sizeof(mfm));
    FloppyBridgeCheckTrack(mfm,nbits,c);
    // the other revolutions, for the log if they differ
    for(int r=1;r<n;r++)
    {
      TFloppyBridgeCheck cr;
      nbits=GwFluxToBits(revs[r],rev_units,mfm,sizeof(mfm));
      FloppyBridgeCheckTrack(mfm,nbits,cr);
      rev_ids[r]=cr.ids;
      rev_good[r]=cr.good;
      rev_bad[r]=cr.bad+cr.bad_ids;
    }
    rev_ids[0]=c.ids;
    rev_good[0]=c.good;
    rev_bad[0]=c.bad+c.bad_ids;
  }
  // sectors of another track: the head isn't where we thought (some
  // protections number their sectors oddly: only one try)
  if(c.ids>=3 && c.id_track!=track && !gw->rereading)
  {
    Log(gw,"track %2d side %d: the sectors say track %d - back to track 0 and again",
      track,side,c.id_track);
    gw->cyl=-1;
    gw->rereading=true;
    int r=GwFluxReadTrack(gw,side,track,revs,max_revs);
    gw->rereading=false;
    return r;
  }
  // the log
  if(gw->log)
  {
    char line[400];
    int len=snprintf(line,sizeof(line),"track %2d side %d: %d rev, %d ms,",
      track,side,n,(int)ms);
    if(n && index.size()>=2)
    {
      len+=snprintf(line+len,sizeof(line)-len,
        " %.2f ms/rev, %d transitions | %d IDs",rev_ms,(int)revs[0].size(),
        c.ids);
      if(c.ids)
      {
        len+=snprintf(line+len,sizeof(line)-len," (trk %d side %d, sectors",
          c.id_track,c.id_side);
        for(int k=0;k<c.ids && k<FLOPPYBRIDGE_CHECK_MAX && len<300;k++)
          len+=snprintf(line+len,sizeof(line)-len," %d",c.id_list[k]);
        len+=snprintf(line+len,sizeof(line)-len,")");
      }
      len+=snprintf(line+len,sizeof(line)-len,", data OK %d, bad %d",c.good,
        c.bad);
      if(c.bad)
      {
        len+=snprintf(line+len,sizeof(line)-len," (");
        for(int k=0;k<c.bad && k<FLOPPYBRIDGE_CHECK_MAX && len<380;k++)
          len+=snprintf(line+len,sizeof(line)-len,"%s%d",k?",":"",
            c.bad_list[k]);
        len+=snprintf(line+len,sizeof(line)-len,")");
      }
      if(c.bad_ids)
        len+=snprintf(line+len,sizeof(line)-len,", %d bad ID CRC",c.bad_ids);
    }
    else
      len+=snprintf(line+len,sizeof(line)-len," nothing (unformatted?)");
    Log(gw,"%s",line);
    // revolutions that don't read the same (decoded like the ST's WD1772)
    bool differ=false;
    for(int r=1;r<n;r++)
      if(rev_ids[r]!=rev_ids[0] || rev_good[r]!=rev_good[0]
        || rev_bad[r]!=rev_bad[0])
        differ=true;
    if(differ || rev_bad[0])
    {
      len=snprintf(line,sizeof(line),"          revolutions (IDs/data OK/bad):");
      for(int r=0;r<n;r++)
        len+=snprintf(line+len,sizeof(line)-len," %d/%d/%d",rev_ids[r],
          rev_good[r],rev_bad[r]);
      Log(gw,"%s",line);
    }
  }
  return n;
}


int GwFluxPoll(void *handle,bool *write_protected) {
/*  The disk-change line (pin 34) of a PC drive goes low when the disk is
    taken out and stays low until the head steps with a disk in: the
    "no-click step" command does that without moving the head.
*/
  TGwFlux *gw=(TGwFlux*)handle;
  if(!gw)
    return GWFLUX_POLL_UNKNOWN;
  bool was_selected=gw->selected;
  Select(gw,true);
  int wp=GetPin(gw,28);
  if(write_protected && wp>=0)
    *write_protected=(wp==0);
  int result=GWFLUX_POLL_UNKNOWN;
  if(gw->dskchg_ok)
  {
    int in=GetPin(gw,34);
    if(in==0)
    {
      if(Command(gw,GW_NOCLICKSTEP,NULL,0)==GW_ACK_OKAY)
        in=GetPin(gw,34);
      gw->cyl=-1; // the step may have moved the head (it did, to track 0)
      if(in==0)
        gw->disk_was_out=true;
    }
    if(in==0)
      result=GWFLUX_POLL_NODISK;
    else if(in==1)
    {
      result=gw->disk_was_out ? GWFLUX_POLL_NEWDISK : GWFLUX_POLL_DISK;
      gw->disk_was_out=false;
    }
  }
  if(!was_selected && !gw->motor)
    Select(gw,false);
  return result;
}

#else // not WIN32: no device

void *GwFluxOpen(const char*,const char*,char *error,int error_size) {
  if(error && error_size>0)
  {
    strncpy(error,"Greaseweazle flux mode needs Windows",error_size-1);
    error[error_size-1]=0;
  }
  return NULL;
}
void GwFluxClose(void*) {}
int GwFluxRevs(void*) { return 0; }
int GwFluxCylinders(void*) { return 0; }
int GwFluxReadTrack(void*,int,int,TGwFluxRev*,int) { return 0; }
void GwFluxMotorOff(void*) {}
int GwFluxPoll(void*,bool*) { return GWFLUX_POLL_UNKNOWN; }
void GwFluxLogMessage(void*,const char*) {}

#endif//WIN32

#endif//#if defined(SSE_GREASEWEAZLE_FLUX)
