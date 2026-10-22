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

DOMAIN: hard drive
FILE: hd_gemdos.cpp
CONDITION: DISABLE_STEMDOS musn't be defined
DESCRIPTION: Steem's virtual hard drive emulation. This is achieved through
intercepting ST OS calls and translating them to PC OS calls. In Steem SSE,
we call this emulation GEMDOS HD emulation, but internally it is STEMDOS,
while GEMDOS is the non-intercepted system.

The starting point is StemdosCheckTrap1(), where it is decided if a
call will be intercepted (STEMDOS) or not (GEMDOS).

cpu_op: intercept_os()
        |-> intercept_gemdos()
            |-> StemdosCheckTrap1()

When a call is intercepted, it can result in other calls. STEMDOS triggers
its own interrupts and checks at RTE.
This high-level emulation has been refactored and improved in v410, resulting
in more reliable operation in TOS 1.0 and 1.02.

TODO Disk operation can cause slow-downs that hamper emulation, eg Drone
We could create an apart thread?
This file was called stemdos, maybe it was better, especially since hd_acsi
has been renamed acsi.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <iolist.h>
#include <computer.h>
#include <shortcutbox.h>
#include <osd.h>
#include <harddiskman.h>
#include <translate.h>



enum {

  // STEMDOS_RTE_GETDRIVE            0x02
  // STEMDOS_RTE_GETDIR              0x01
  STEMDOS_RTE_DUP                =0x03,
  // STEMDOS_RTE_FCREATE             0x10
  // STEMDOS_RTE_FOPEN               0x20
  STEMDOS_RTE_GET_DTA_FOR_FSFIRST=0x30,
  STEMDOS_RTE_FCLOSE             =0x40,
  // STEMDOS_RTE_DFREE               0x50
  // STEMDOS_RTE_MKDIR               0x60
  // STEMDOS_RTE_RMDIR               0x70
  // STEMDOS_RTE_FDELETE             0x80
  // STEMDOS_RTE_FATTRIB             0x90
  // STEMDOS_RTE_RENAME              0xa0
  STEMDOS_RTE_PEXEC              =0xb0,
  STEMDOS_RTE_MFREE              =0xc0,
  STEMDOS_RTE_MFREE2             =0xd0,
  // STEMDOS_RTE_SUBACTION           0xf
  // STEMDOS_RTE_MAINACTION          0xf0
  STEMDOS_FILE_IS_STEMDOS          =0,
  STEMDOS_FILE_IS_GEMDOS           =1,
  // STEMDOS_FILE_ASKING               2
  // GEMDOS_VECTOR LPEEK           0x84

  // cooler with proper names

  //GEMDOS System Call Summary
  P_TERM0        =0x00,
  C_CONIN        =0x01,
  C_CONOUT       =0x02,
  C_AUXIN        =0x03,
  C_AUXOUT       =0x04,
  C_PRNOUT       =0x05,
  C_RAWIO        =0x06,
  C_RAWCIN       =0x07,
  C_NECIN        =0x08,
  C_CONWS        =0x09,
  C_CONRS        =0x0a,
  C_CONIS        =0x0b,
  D_SETDRV       =0x0e,
  C_CONOS        =0x10,
  C_PRNOS        =0x11,
  C_AUXIS        =0x12,
  C_AUXOS        =0x13,
  D_GETDRV       =0x19,
  F_SETDTA       =0x1A,
  // S_SETVEC        0x25
  T_GETDATE      =0x2a,
  T_SETDATE      =0x2b,
  T_GETTIME      =0x2c,
  T_SETTIME      =0x2d,
  F_GETDTA       =0x2f,
  /*
   S_VERSION       0x30
  All you need to know:
  0x1300 (0.13) TOS 1.0, TOS 1.02
  0x1500 (0.15) TOS 1.04, TOS 1.06
  0x1700 (0.17) TOS 1.62
  0x1900 (0.19) TOS 2.01, TOS 2.05, TOS 2.06
  */
  P_TERMRES       =0x31,
  // S_GETVEC        =0x35
  D_FREE          =0x36,
  D_CREATE        =0x39,
  D_DELETE        =0x3a,
  D_SETPATH       =0x3b,
  F_CREATE        =0x3c,
  F_OPEN          =0x3d,
  F_CLOSE         =0x3e,
  F_READ          =0x3f,
  F_WRITE         =0x40,
  F_DELETE        =0x41,
  F_SEEK          =0x42,
  F_ATTRIB        =0x43,
  // F_IOCTL         =0x44
  F_DUP           =0x45,
  F_FORCE         =0x46,
  D_GETPATH       =0x47,
  M_ALLOC         =0x48,
  M_FREE          =0x49,
  M_SHRINK        =0x4a,
  P_EXEC          =0x4b,
  P_TERM          =0x4c,
  F_SFIRST        =0x4e,
  F_SNEXT         =0x4f,
  F_RENAME        =0x56,
  F_DATIME        =0x57,

  // File attributes, the same as in Windows (but not Unix)!
  FA_READONLY=BIT_0,
#undef FA_HIDDEN // BCC stupid warning
  FA_HIDDEN  =BIT_1,
#undef FA_SYSTEM
  FA_SYSTEM  =BIT_2,
  FA_VOLUME  =BIT_3,
  FA_DIR     =BIT_4,
  // FA_ARCHIVE  BIT_5
  // FA_RESERVED BIT_6

  // P_EXEC modes
  PE_LOADGO    =0,
  PE_LOAD      =3,
  PE_GO        =4,
  PE_BASEPAGE  =5,
  PE_GOTHENFREE=6,// available in v0.15 (TOS 1.04+)

 // 6 Standard handles (4, 5: reserved)
 GSH_CONIN  =0,// con: input
 GSH_CONOUT =1,// con: Standard output
 GSH_AUX    =2,// aux: Currently mapped serial device 
 GSH_PRN    =3, // prn: Printer port
 DTA_SEARCH_NUMBER=4, // Steem internal
 DTA_ATTRIB=21,
 DTA_TIME=22,
 DTA_DATE=24,
 DTA_LEN=26,
 DTA_NAME=30
};

WORD GemdosCommand;
bool StemdosComlineReadRb=false; //Command line option

#ifndef DISABLE_STEMDOS
#define D0 REGL(0) // 32bit CPU register D0
// functions and data private to hd_gemdos.cpp, this could all go into
// the struct
int StemdosOpenFile(WORD attr);
void StemdosCloseFile(TStemdosFile* sfs);
void StemdosSeek(WORD h,MEM_ADDRESS mySP);
void StemdosFdatime(WORD h,MEM_ADDRESS mySP);
void StemdosDfree(int dr,MEM_ADDRESS buffer);
void StemdosRead(WORD h,MEM_ADDRESS mySP);
void StemdosWrite(WORD h,MEM_ADDRESS mySP);
DWORD StemdosSearchWildcardPCPath();
void StemdosTrap1();
bool StemdosPterm();
void StemdosMkdir();
void StemdosRmdir();
void StemdosFdelete();
void StemdosRename(EasyStr NewFilename);
void StemdosFattrib(WORD SetAttr,WORD STAttr);
int StemdosPexec();
void StemdosFsfirst(MEM_ADDRESS mySP);
void StemdosFsnext();
int StemdosGetFilePath();
BYTE StemdosGetBootDrive();
void StemdosGetPCPath();
void StemdosAddPexec(MEM_ADDRESS ad,MEM_ADDRESS env);
void StemdosParsePath(); //remove \..\ etc.
void StemdosCallFdup();
void StemdosCallMfree(MEM_ADDRESS ad);
void StemdosCallFgetdta();
void StemdosCallFclose(WORD h);
void StemdosCallPexec(MEM_ADDRESS command,MEM_ADDRESS env);
void StemdosFinish();
void StemdosSkipGemdos(); //clear stack from original GEMDOS call
char* StrUpperNoSpecial(char*Str);
int StemdosRteAction;
TStemdosFile StemdosFile[MAX_STEMDOS_FILES];
TStemdosFile StemdosNewFile;
BYTE StemdosForcedHandle[GEMDOS_STD_HANDLES]={0,0,0,0,0,0}; 
EasyStr StemdosFilename,PCFilename;
FILE *fStemdosPexec=NULL;
WORD StemdosPexecMode=0;
bool StemdosIgnoreNextPexec=false;
bool STfileReadError=false;
WORD StemdosSaveSR;
MEM_ADDRESS StemdosSaveAddress;


// Find "Allow wildcards?"

// The ST used create time but using standard functions we can't get or
// set create time on DOS (not sure about setting on UNIX). We would have to
// change file access to Windows commands (shudder).


#define LOGSECTION LOGSECTION_GEMDOS

TStemdosFile::TStemdosFile() {
  fp=NULL;
  attrib=Pexec=0;
  date=time=0;
  h=0;
  open=false;
}


TStemdosFsnext::TStemdosFsnext() {
  dta=0;
  start_hbl=attr=0;
}


void TStemdos::Init() {
  for(int n=DRIVE_A;n<GEMDOS_MAXDRIVES;n++) // A: on purpose
  {
    DriveMounted[n]=false;
    MountPath[n]="";
    CurrentPath[n]="";
  }
  StemdosNewFile.open=false;
  StemdosNewFile.fp=NULL;
  StemdosNewFile.attrib=0;
  StemdosNewFile.Pexec=0;
  StemdosNewFile.filename="";
  StemdosNewFile.date=StemdosNewFile.time=0;
  for(int n=0;n<MAX_STEMDOS_FILES;n++)
  {
    StemdosFile[n].open=false;
    StemdosFile[n].fp=NULL;
    StemdosFile[n].attrib=0;
    StemdosFile[n].Pexec=0;
    StemdosFile[n].filename="";
    StemdosFile[n].date=StemdosFile[n].time=0;
    StemdosFile[n].h=0;
  }
  fStemdosPexec=NULL;
}


void TStemdos::Reset() {
  SetDriveReset();
  CloseAllFiles();
  for(int n=0;n<MAX_STEMDOS_FSNEXT_STRUCTS;n++)
  {
    FsnextData[n].dta=0;
    FsnextData[n].path="";
  }
  PexecListPtr=0;
  ZeroMemory(PexecList,sizeof(PexecList));
  StemdosIgnoreNextPexec=false;
  for(int n=DRIVE_A;n<GEMDOS_MAXDRIVES;n++) 
    CurrentPath[n]="";
  bInterceptDateTime=true;
}


void TStemdos::SetDriveReset() {
  CurrentDrive=StemdosGetBootDrive();
}


bool StemdosCheckTrap1() {
  bool bIntercepted=false; // to intercept or not to intercept, that is the question
  bool Invalid;
  MEM_ADDRESS mySP=GetSPBeforeTrap(&Invalid);
  if(Invalid) 
  {
    TRACE_LOG("STEMDOS Invalid SP %x\n",mySP);
    return bIntercepted;
  }
  GemdosCommand=SafeDPeek(mySP);
  WORD para_word=SafeDPeek(mySP+2); // usesful for some functions
  MEM_ADDRESS para_long=SafeLPeek(mySP+2); // usesful for some functions 
  StemdosSaveAddress=SafeLPeek(SP+2); // (A7) is pushed sr

  switch(GemdosCommand) {

  case P_TERM0:
  case P_TERM:
  {
    bool isStemdos=StemdosPterm();
    if(isStemdos && StemdosPexecMode==PE_GO) // must free memory - StemdosPexecMode assigned in P_EXEC
    {
#if defined(SSE_ENABLE_TRACE_LOG)
      short ec=(GemdosCommand==P_TERM0) ? 0 : para_word;
      TRACE_LOG("%06X Process %d PTERM %d $%X env $%X\n",old_pc,Stemdos.PexecListPtr+1,ec,
                Stemdos.PexecList[Stemdos.PexecListPtr],Stemdos.env[Stemdos.PexecListPtr]);
#endif     
      on_rte=ON_RTE_STEMDOS;
      UPDATE_SR;
      StemdosSaveSR=SR;
      PSWI=7;
      on_rte_interrupt_depth=interrupt_depth+1;
      // first free the environment string, it's an apart allocation!
      StemdosCallMfree(Stemdos.env[Stemdos.PexecListPtr]); 
      StemdosRteAction=STEMDOS_RTE_MFREE;
      bIntercepted=true; 
    }
    break; // do mfree, then term
  }
  // stdin con:
  case C_CONIN:
  case C_RAWIO:
  case C_RAWCIN:
  case C_NECIN:
  case C_CONRS:
  case C_CONIS:
  {
    BYTE h=StemdosForcedHandle[GSH_CONIN]; //see F_FORCE
    if(h)
    {
      if(StemdosFile[h].open)
      {
        int c;
        switch(GemdosCommand) {
        case C_RAWIO:
          c=para_word;
          if((BYTE)c!=0xff)
            break;
        case C_CONIN:
        case C_RAWCIN:
        case C_NECIN:
          c=FGETC(StemdosFile[h].fp);
          if(c==EOF)
            c=0;
          D0=c;
          StemdosSkipGemdos();
          bIntercepted=true;
          break;
        case C_CONIS:
          D0=-1;
          StemdosSkipGemdos();
          bIntercepted=true;
          break;
        case C_CONRS:
          D0=0;
          StemdosSkipGemdos();
          bIntercepted=true;
          break;
        }
      }
      else
        StemdosForcedHandle[GSH_CONIN]=0;
    }
    break;
  }
  // stdout con:
  case C_CONOUT:
  case C_CONWS:
  case C_CONOS:
  {
    BYTE h=StemdosForcedHandle[GSH_CONOUT];
    if(h)
    {
      if(StemdosFile[h].open)
      {
        switch(GemdosCommand) {
        case C_CONOUT:
          FPUTC(para_word,StemdosFile[h].fp);
          D0=0;
          break;
        case C_CONWS:
        {
          Str line=ReadStringFromMemory(para_long,32000);
          D0=(int)FWRITE(line.Text,1,line.Length(),StemdosFile[h].fp);
          break;
        }
        default:
          D0=-1;
        }
        StemdosSkipGemdos();
        bIntercepted=true;
      }
      else
        StemdosForcedHandle[GSH_CONOUT]=0;
    }
    break;
  }
  // aux:
  case C_AUXIN:
  case C_AUXOUT:
  case C_AUXIS:
  case C_AUXOS:
  {
    BYTE h=StemdosForcedHandle[GSH_AUX];
    if(h)
    {
      if(StemdosFile[h].open)
      {
        switch(GemdosCommand) {
        case C_AUXOUT:
          FPUTC(para_word,StemdosFile[h].fp);
          D0=0;
          break;
        case C_AUXIN:
          D0=(BYTE)FGETC(StemdosFile[h].fp);
          break;
        case C_AUXIS:
          D0=(feof(StemdosFile[h].fp)) ? 0 : -1;
          break;
        default: //C_AUXOS!
          D0=-1;
        }
        StemdosSkipGemdos();
        bIntercepted=true;
      }
      else
        StemdosForcedHandle[GSH_AUX]=0;
    }
    break;
  }
  // prn:
  case C_PRNOUT:
  case C_PRNOS:
  {
    BYTE h=StemdosForcedHandle[GSH_PRN];
    if(h)
    {
      if(StemdosFile[h].open)
      {
        if(GemdosCommand==C_PRNOUT)
        {
          FPUTC(para_word,StemdosFile[h].fp);
          D0=0;
        }
        else
          D0=-1;
        StemdosSkipGemdos();
        bIntercepted=true;
      }
      else
        StemdosForcedHandle[GSH_PRN]=0;
    }
    break;
  }
  case D_SETDRV:
    Stemdos.CurrentDrive=(BYTE)para_word;
    break;   //let Gemdos set its drive
  case D_GETDRV:
    if(!HardDiskMan.DisableHardDrives)
    {
      D0=Stemdos.CurrentDrive;
      StemdosSkipGemdos();
      bIntercepted=true;
    }
    break;
  case F_SETDTA:
/*When a process is started, its DTA is located at a point where it could overlay potentially
important system structures. To avoid overwriting memory a process wishing to use Fsfirst()
and Fsnext() should allocate space for a new DTA and use Fsetdta() to instruct the OS to use it.
The original location of the DTA should be saved first, however. Its location can be found with
the call Fgetdta(). At the completion of the operation the old address should be replaced with
Fsetdta().
Nice OS Atari!
*/
    Stemdos.Dta=para_long;
    break; //let GEMDOS set the DTA
  //case 0x20:  // Super
    //return;
  case T_GETDATE:
  case T_SETDATE:
  case T_GETTIME:
  case T_SETTIME:
    if(Stemdos.bInterceptDateTime && OPTION_RTC_HACK)
    {
      time_t tmr=time(NULL); // used by GFA3 each time the editor appears
      struct tm *lpTime=localtime(&tmr);
      DWORD DOSTime=TMToDOSDateTime(lpTime);
      switch(GemdosCommand) {
      case T_GETDATE:
        D0=HIWORD(DOSTime);
        break;
      case T_GETTIME:
        D0=LOWORD(DOSTime);
        break;
      case T_SETDATE:
        if(para_word!=HIWORD(DOSTime))
          Stemdos.bInterceptDateTime=false;
        return bIntercepted; // don't RTE
      case T_SETTIME:
        if(para_word!=LOWORD(DOSTime))
          Stemdos.bInterceptDateTime=false;
        return bIntercepted; // don't RTE
      }
      StemdosSkipGemdos();
      bIntercepted=true;
    }
    break;
  case P_TERMRES:
    StemdosPterm();
    // Gemdos P_TERMRES handles all memory fiddling
    break; // return to Gemdos
  case D_FREE:
  {
    MEM_ADDRESS buffer=para_long; // where info is to be copied
    WORD d=SafeDPeek(mySP+6);
    if(d==0) //need to get current drive
      d=Stemdos.CurrentDrive+1; //done!
    if(Stemdos.IsMounted((BYTE)(d-1)))
    {
      StemdosDfree(d-1,buffer);
      StemdosSkipGemdos();
      bIntercepted=true;
    }
    break;
  }
  case D_CREATE:
  case D_DELETE:
  case F_DELETE:
    StemdosFilename=ReadStringFromMemory(para_long,STEMDOS_MAX_PATH);
    if(StemdosGetFilePath()==STEMDOS_FILE_IS_STEMDOS)
    {
      switch(GemdosCommand) {
      case D_CREATE:
        StemdosMkdir();
        break;
      case D_DELETE:
        StemdosRmdir();
        break;
      case F_DELETE:
        StemdosFdelete();
        break;
      }
      StemdosSkipGemdos();
      bIntercepted=true;
      break;
    }
    break;  //GEMDOS Dcreate
  case D_SETPATH:
    StemdosFilename=ReadStringFromMemory(para_long,STEMDOS_MAX_PATH);
    if(StemdosGetFilePath()==STEMDOS_FILE_IS_STEMDOS)
    {
      if(StemdosFilename[2]=='\0' || IsSameStr(StemdosFilename.Text+2,"\\"))
      {
        Stemdos.CurrentPath[Stemdos.CurrentDrive]="";
        D0=Tos.NoError;
      }
      else
      { //nonempty path
        EasyStr NewFol=StemdosFilename.Text+2;
// ST slash is like DOS/Windows: \   //
#define NO_ST_SLASH(cs) {size_t i=strlen(cs);if(i)if(cs[i-1]=='\\')cs[i-1]=0;}
        NO_ST_SLASH(NewFol);
#undef NO_ST_SLASH
        DWORD Attrib=FILE_ATTRIBUTE_DIRECTORY;
        // Don't check existence if not changing it
        if(NotSameStr_I(NewFol,Stemdos.CurrentPath[Stemdos.CurrentDrive]))
        {
          StemdosGetPCPath();
          Attrib=GetFileAttributes(PCFilename);
        }
        if((Attrib&FILE_ATTRIBUTE_DIRECTORY)==0 || Attrib==INVALID_FILE_ATTRIBUTES)
          D0=Tos.PathNotFound;
        else
        {
          Stemdos.CurrentPath[Stemdos.CurrentDrive]=NewFol;
          D0=Tos.NoError;
        }
      }
      TRACE_LOG("%06X Process %d D_SETPATH %s: %d\n",old_pc,Stemdos.PexecListPtr,
                CHECKPATH(Stemdos.CurrentPath[Stemdos.CurrentDrive].Text),D0);
      StemdosSkipGemdos();
      bIntercepted=true;
    }
    break;
  case F_CREATE:
  case F_OPEN:
    UPDATE_SR;
    StemdosSaveSR=SR;
    PSWI=7;
    StemdosRteAction=0;
    StemdosFilename=ReadStringFromMemory(para_long,STEMDOS_MAX_PATH);
    if(StemdosGetFilePath()==STEMDOS_FILE_IS_STEMDOS)
    {
      WORD attr=SafeDPeek(mySP+6);
      if(GemdosCommand==F_CREATE && (attr&FA_DIR))
      { //create volume label
        StemdosFinish();
        D0=Tos.NoError; //succeeded, honest!
        StemdosSkipGemdos();  //don't do GEMDOS call, freaking volume label!
        bIntercepted=true;
        break;
      }
      int ec=StemdosOpenFile(attr);
      if(ec<0)
        StemdosSkipGemdos(); //don't do GEMDOS call, couldn't open file
      TRACE_LOG("%06X Process %d %s %s: %d%c",old_pc,StemdosNewFile.Pexec,
                (GemdosCommand==F_CREATE) ? "F_CREATE" : "F_OPEN",CHECKPATH(PCFilename.Text),
                ec,(ec<0) ? '\n' : ' '); // leaving rooom for F_DUP
      bIntercepted=true;
      break; //call GEMDOS to get file handle - interrupt already set up
    }
    StemdosFinish();
    break;
  case F_CLOSE:
  {
    UPDATE_SR;
    StemdosSaveSR=SR;
    PSWI=7;
    WORD h=para_word;
    if(h<GEMDOS_STD_HANDLES)
    {
      h=StemdosForcedHandle[h];
      StemdosForcedHandle[h]=0;
    }
    if(h<GEMDOS_STD_HANDLES || h>=MAX_STEMDOS_FILES)
    {
      TRACE_LOG("GEMDOS Fclose %d\n",h);
      StemdosFinish();  //GEMDOS can handle this one!
    }
    else if(StemdosFile[h].open)
    { //one of ours
      StemdosCloseFile(&(StemdosFile[h]));
      TRACE_LOG("%06X Process %d F_CLOSE h %d %s\n",old_pc,Stemdos.PexecListPtr,h,
                CHECKPATH(StemdosFile[h].filename.Text));
      on_rte=ON_RTE_STEMDOS;
      on_rte_interrupt_depth=interrupt_depth+1;
      StemdosRteAction=STEMDOS_RTE_FCLOSE;
      StemdosCallFclose(h); // to deallocate phoney file handle
      bIntercepted=true;
    }
    else
      StemdosFinish();  //GEMDOS file
    break;
  }
  case F_READ:
  {
    WORD h=para_word;
    if(h<GEMDOS_STD_HANDLES) 
      h=StemdosForcedHandle[h];
    if(h>=GEMDOS_STD_HANDLES && h<MAX_STEMDOS_FILES)
    {
      if(StemdosFile[h].open)
      { //one of ours
        StemdosRead(h,mySP);
        StemdosSkipGemdos();
        bIntercepted=true;
      }
    }
    break;
  }
  case F_WRITE:
  {
    WORD h=para_word;
    if(h<GEMDOS_STD_HANDLES) 
      h=StemdosForcedHandle[h];
    if(h>=GEMDOS_STD_HANDLES && h<MAX_STEMDOS_FILES)
    {
      if(StemdosFile[h].open)
      { //one of ours
        StemdosWrite(h,mySP);
        StemdosSkipGemdos();
        bIntercepted=true;
      }
    }
    break;
  }
  case F_SEEK:
  {
    WORD h=SafeDPeek(mySP+6);
    if(h<GEMDOS_STD_HANDLES) 
      h=StemdosForcedHandle[h];
    if(h>=GEMDOS_STD_HANDLES && h<MAX_STEMDOS_FILES)
    {
      if(StemdosFile[h].open)
      { //one of ours
        StemdosSeek(h,mySP);
        StemdosSkipGemdos();
        bIntercepted=true;
      }
    }
    break;
  }
  case F_ATTRIB:
  {
    StemdosFilename=ReadStringFromMemory(para_long,STEMDOS_MAX_PATH);
    WORD STAttr=SafeDPeek(mySP+8); // used when setting attributes
    WORD SetAttr=SafeDPeek(mySP+6); // 0=read 1=write
    if(StemdosGetFilePath()==STEMDOS_FILE_IS_STEMDOS)
    {
      StemdosFattrib(SetAttr,STAttr);
      StemdosSkipGemdos();
      bIntercepted=true;
      break;
    }
    break;  //GEMDOS Fattrib
  }
  case F_FORCE:
  { 
    WORD hStd=para_word,h=SafeDPeek(mySP+4);
    if(hStd<GEMDOS_STD_HANDLES)
    {
      if(h>=GEMDOS_STD_HANDLES && h<MAX_STEMDOS_FILES)
      {
        if(StemdosFile[h].open)
        { //one of ours
          StemdosForcedHandle[hStd]=(BYTE)h;
          D0=Tos.NoError;
          TRACE_LOG("%06X Process %d F_FORCE hStd %d h %d\n",old_pc,Stemdos.PexecListPtr,hStd,h);
          StemdosSkipGemdos();
          bIntercepted=true;
        }
        else
          StemdosForcedHandle[hStd]=0;
      }
    }
    break;
  }
  case D_GETPATH:
  {
/*LONG Dgetpath( buf, drive )
drive should be DEFAULT_DRIVE (0) for the current GEMDOS
drive, 1 for drive ‘A:’, 2 for drive ‘B:’, and so on
*/
    WORD drive=SafeDPeek(mySP+6); //what's it set the drive to?
    if(drive==0)
      drive=Stemdos.CurrentDrive;
    else
      drive--;
    if(Stemdos.IsMounted((BYTE)drive))
    {
      WriteStringToMemory(para_long,Stemdos.CurrentPath[drive]);
      D0=Tos.NoError;
      TRACE_LOG("%06X Process %d D_GETPATH %s\n",old_pc,Stemdos.PexecListPtr,
                CHECKPATH(Stemdos.CurrentPath[drive].Text));
      StemdosSkipGemdos();
      bIntercepted=true;
    }
    break;
  }
  case M_ALLOC:
    TRACE_LOG2("%06X Process %d M_ALLOC %d\n",old_pc,Stemdos.PexecListPtr,(LONG)para_long);
    break;
  case M_FREE:
    TRACE_LOG2("%06X Process %d M_FREE $%x\n",old_pc,Stemdos.PexecListPtr,para_long);
    break;
  case M_SHRINK:
    TRACE_LOG("%06X Process %d M_SHRINK %x to %d\n",old_pc,Stemdos.PexecListPtr,
              SafeLPeek(mySP+4),SafeLPeek(mySP+8));
    break;
  case P_EXEC: // Pexec (mode,fil,com,env)
  {
#if defined(SSE_TOS_KEYBOARD_CLICK)
    if(!OPTION_KEYBOARD_CLICK) //see Pump ab das Bier by The Confederacy, Game Over II
      Tos.CheckKeyboardClick();
#endif
      //modes - 0=Load n' go
      //        3=Load n' dont go (return basepage address in D0)
      //        4=Run from memory (fil=ignored,com=Address,env=ignored)
      //        5=Make basepage (fil=ignored)
      //        6=Go then free (TOS 1.04+)
    WORD ExecMode=para_word;
    TRACE_LOG("%s %d\n","P_EXEC",ExecMode);
#if defined(SSE_STATS)
    if(ExecMode==PE_LOADGO || ExecMode==PE_GO)
      Stats.nPrg++;
#endif
    switch(ExecMode) {
    case PE_LOADGO:
    case PE_LOAD:
    {
      UPDATE_SR;
      StemdosSaveSR=SR;
      PSWI=7;
#ifdef DEBUG_BUILD
      // this will work only when going super->user
      if(stop_on_next_program_run)
      {
        stop_on_user_change=1;
        stop_on_next_program_run=2;
      }
#endif
      if(OPTION_CAPTURE_MOUSE&4) // release mouse at program run
      {
        OPTION_CAPTURE_MOUSE=4;
        SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
      }
      StemdosFilename=ReadStringFromMemory(SafeLPeek(mySP+4),STEMDOS_MAX_PATH);
      int IsGemdos=StemdosGetFilePath(); // this function may change StemdosFilename
      ASSERTB(IsGemdos==STEMDOS_FILE_IS_STEMDOS||IsGemdos==STEMDOS_FILE_IS_GEMDOS);
#if defined(SSE_STATS)
      strncpy(Tos.PrgName,GetFileNameFromPath(StemdosFilename.Text),16);
      Tos.PrgName[15]='\0';
      TRACE_LOG("%s\n",Tos.PrgName);
#endif
      switch(IsGemdos) {
      case STEMDOS_FILE_IS_STEMDOS:
      {
        MEM_ADDRESS command=SafeLPeek(mySP+8);
        MEM_ADDRESS env=SafeLPeek(mySP+12);
        StemdosPexecMode=ExecMode;
        StemdosGetPCPath();
        StemdosSearchWildcardPCPath();
        fStemdosPexec=fopen(PCFilename,"rb");
        if(fStemdosPexec)
        {
          TRACE_LOG("%s %X %X\n",CHECKPATH(StemdosFilename.Text),command,env);
          on_rte=ON_RTE_STEMDOS;
          on_rte_interrupt_depth=interrupt_depth+1;
          StemdosRteAction=STEMDOS_RTE_PEXEC;
          StemdosCallPexec(command,env);
        }
        else
        { //no such file
          D0=Tos.FileNotFound;
          StemdosFinish();
          StemdosSkipGemdos();
        }
        bIntercepted=true;
        break;
      }
      case STEMDOS_FILE_IS_GEMDOS:
        TRACE_LOG("%s left to GEMDOS\n",CHECKPATH(StemdosFilename.Text));
        StemdosAddPexec(0,0); //log latest program as Gemdos
        StemdosFinish();
#if defined(SSE_DEBUG_SYMBOLS)
        Tos.TrackSymbols=1;
#endif
        break;
      }//sw
      break;
    }
    case PE_GO:
    case PE_GOTHENFREE:
      if(StemdosIgnoreNextPexec)
        // This is a hard drive program, we change the ExecMode 0 call to 
        // ExecMode 4 (or 6)
        StemdosIgnoreNextPexec=false;
      else
        StemdosAddPexec(0,0); //log latest program as Gemdos
      break;
    }//sw
    break;
  }
  case F_SFIRST:
    StemdosFilename=ReadStringFromMemory(para_long,STEMDOS_MAX_PATH);
#ifndef NO_CRAZY_MONITOR
    if(extended_monitor==1 && StemdosFilename=="\\AUTO\\*.PRG")
    {
      call_a000();
      extended_monitor++;
      break; // When $A000 RTEs we will return to the gemdos vector address,
             // and then fsfirst will do its magic
    }
#endif
    if(StemdosGetFilePath()==STEMDOS_FILE_IS_STEMDOS)
    {
      UPDATE_SR;
      StemdosSaveSR=SR;
      PSWI=7;
      on_rte=ON_RTE_STEMDOS;
      on_rte_interrupt_depth=interrupt_depth+1;
      StemdosRteAction=STEMDOS_RTE_GET_DTA_FOR_FSFIRST;
      StemdosCallFgetdta();
      bIntercepted=true;
    }
    break;  //GEMDOS allowed to continue
  case F_SNEXT:
    if(SafeLPeek(Stemdos.Dta)==0x0baddeed) // magic number for STEMDOS search $baddeed
    {
      StemdosFsnext();
      StemdosSkipGemdos();
      bIntercepted=true;
    }
    break;
  case F_RENAME:
  {
    StemdosFilename=ReadStringFromMemory(SafeLPeek(mySP+4),STEMDOS_MAX_PATH);
    EasyStr NewFilename=ReadStringFromMemory(SafeLPeek(mySP+8),STEMDOS_MAX_PATH);
    if(StemdosGetFilePath()==STEMDOS_FILE_IS_STEMDOS)
    {
      StemdosRename(NewFilename);
      StemdosSkipGemdos();
      bIntercepted=true;
      break;
    }
    break;  //GEMDOS Fattrib
  }
  case F_DATIME:
  {
    WORD h=SafeDPeek(mySP+6);
    if(h<GEMDOS_STD_HANDLES) 
      h=StemdosForcedHandle[h];
    if(h>=GEMDOS_STD_HANDLES && h<MAX_STEMDOS_FILES)
    {
      if(StemdosFile[h].open)
      { //one of ours
        StemdosFdatime(h,mySP);
        StemdosSkipGemdos();
        bIntercepted=true;
      }
    }
    break;
  }
  default: 
    break;
  }//sw
  if(bIntercepted)
  {
    TRACE_LOG2("Intercepted\n");
  }
  return bIntercepted; // just in case
}


BYTE StemdosGetBootDrive() {
  if(Stemdos.BootDrive<DRIVE_C) 
    return DRIVE_A;
  // If there is a disk in drive A then boot from it except if
  // the control key is being held down
  bool NoControl=true;
  if(!CutDisableKey[VK_CONTROL]) 
    NoControl=((GetKeyState(VK_CONTROL)<0)==0);
#if defined(SSE_TOS_PRG_AUTORUN)
  if(FloppyDrive[DRIVE_A].ImageType.Manager!=MNGR_PRG
     && Stemdos.BootDrive==AUTORUN_HD && FloppyDrive[DRIVE_A].DiskInDrive())
    return DRIVE_A;
  else if(FloppyDrive[DRIVE_A].ImageType.Manager==MNGR_PRG && OPTION_PRG_SUPPORT)
  {
    TRACE_LOG("PRG reads boot drive as %c\n",AUTORUN_HD+'A');
    return AUTORUN_HD;
  }
#endif
  if(FloppyDrive[DRIVE_A].DiskInDrive() && NoControl) 
    return DRIVE_A;
  return (Stemdos.IsMounted(Stemdos.BootDrive)) ? Stemdos.BootDrive : DRIVE_A;
}


int TStemdos::AnyFilesOpen() {
  int ctr=0;
  for(int n=GEMDOS_STD_HANDLES;n<MAX_STEMDOS_FILES;n++)
    if(StemdosFile[n].open) 
      ctr++;
  return ctr;
}


void StemdosSkipGemdos() {
  //clear stack from original GEMDOS call so that it won't be called
  m68kPerformRte();
#if defined(SSE_STATS)
  Stats.nGemdosi++; // other places?
#endif
  interrupt_depth--;
  intercept_os();
}


void StemdosRte() { // called by m68k_rte()
  switch(StemdosRteAction) {
  case STEMDOS_RTE_DUP: //get file handle
    SP+=4; //correct stack
    if(D0>=GEMDOS_STD_HANDLES && D0<MAX_STEMDOS_FILES) //valid file handle
    { 
      if(StemdosFile[D0].open)
      { // F_DUP has somehow returned file handle that we already have open!
        TRACE_LOG("F_DUP handle %d already open\n",D0);
        StemdosRteAction=STEMDOS_RTE_DUP; //get new file handle
        StemdosCallFdup();
      }
      else
      {
        StemdosFile[D0]=StemdosNewFile;
        StemdosNewFile.open=false;
        StemdosFile[D0].h=REGB(0);
        TRACE_LOG("F_DUP %d\n",D0);
        StemdosFinish(); //handle in d0
        StemdosSkipGemdos();
      }
    }
    else
    {
      TRACE_LOG("F_DUP error %d\n",D0);
      StemdosCloseFile(&StemdosNewFile);
      if(D0>=0) 
        D0=Tos.InternalError; // internal GEMDOS error if we got crazy error
      StemdosFinish(); //error code in d0
      StemdosSkipGemdos();
    }
    break;
  case STEMDOS_RTE_GET_DTA_FOR_FSFIRST:
  {
    SP+=2; //correct stack
    Stemdos.Dta=D0&0xffffff; //get DTA
    bool Invalid;
    MEM_ADDRESS mySP=GetSPBeforeTrap(&Invalid);
    // Pass stack pointer with original call info on it
    if(!Invalid) 
      StemdosFsfirst(mySP);
    StemdosFinish();
    StemdosSkipGemdos();
    break;
  }
  case STEMDOS_RTE_FCLOSE:
    SP+=4; //correct stack
    StemdosFinish();
    StemdosSkipGemdos();
    break;
  case STEMDOS_RTE_PEXEC:
  {
    SP+=16; //correct stack, 3 longs and 2 words
    StemdosPexec();
    break;
  }
  case STEMDOS_RTE_MFREE: // on ctrl-C + PTerm
    StemdosCallMfree(Stemdos.PexecList[Stemdos.PexecListPtr]);
    StemdosRteAction=STEMDOS_RTE_MFREE2;
    break;
  case STEMDOS_RTE_MFREE2:
    SP+=6; //correct stack
    SR=StemdosSaveSR; //retain status
    UPDATE_FLAGS;
    StemdosFinish();
    // don't need SkipGemdos because GEMDOS has already returned
    break;
  }//sw
}


void TStemdos::UpdateDrvbits() {
  for(BYTE n=DRIVE_C;n<GEMDOS_MAXDRIVES;n++)
  {
    if(IsMounted(n))
    {
      LPEEK(SV_DRVBITS)|=(1<<n);
    }
  }
}


void StemdosGetPCPath() {
  StrUpperNoSpecial(StemdosFilename);
  char first_letter=(char)toupper(StemdosFilename[0]);
  BYTE harddrivenum=first_letter-'A';
  if(harddrivenum<GEMDOS_MAXDRIVES)
    PCFilename=Stemdos.MountPath[harddrivenum];
#ifdef UNIX
  EasyStr sf=StemdosFilename;
  for(int i=0;sf[i];i++) if(sf[i]=='\\') sf[i]='/';
  PCFilename=find_file_i(PCFilename,sf.Text+3);
#endif
#ifdef WIN32
  PCFilename+=(StemdosFilename.Text+2);
#endif
}


DWORD StemdosSearchWildcardPCPath() {
  DirSearch ds;
  ds.st_only=true;
  if(!ds.Find(PCFilename))
    return 0xffffffff;
  RemoveFileNameFromPath(PCFilename,KEEP_SLASH);
  PCFilename+=ds.Name;
  DWORD Attrib=ds.Attrib;
  ds.Close();
  return Attrib;
}


int StemdosOpenFile(WORD const attr) {
  FILE *fp=NULL;
  StemdosGetPCPath();
  StemdosNewFile.attrib=0;
  D0=Tos.NoError;
  if(PCFilename.RightChar()==SLASHCHAR)
    D0=Tos.PathNotFound; // Don't allow empty names
  else if(GemdosCommand==F_OPEN)
  { //open
    DWORD Attrib=StemdosSearchWildcardPCPath();
    if(Attrib!=0xffffffff)
    {
      if(Attrib&FILE_ATTRIBUTE_DIRECTORY)
        D0=Tos.PathNotFound;
      else if((Attrib&FILE_ATTRIBUTE_READONLY) && attr!=0)
        D0=Tos.AccessDenied;
      else
      {
        StemdosNewFile.attrib=Attrib&
          (FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_READONLY);
        Attrib&=~(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_READONLY);
        SetFileAttributes(PCFilename,Attrib);
        char *pc_mode="r+b"; // Don't wipe the file but allow writing
        // There is a problem here, the ST can open files for read, and then write to them.
        // DOS Windows seems fine with it too, but XP and UNIX are sensible.
        // Always opening for read and write causes problems with permissions and
        // clashes with other programs. Command line option only solution.
        if(StemdosComlineReadRb && attr==0) 
          pc_mode="rb";
        fp=fopen(PCFilename,pc_mode);
        if(fp==NULL) // maybe the file is read-only
        {
          pc_mode="rb";
          fp=fopen(PCFilename,pc_mode);
        }
        if(fp==NULL)
          D0=Tos.PathNotFound;
        else
        {
          D0=Tos.NoError;
          FSEEK(fp,0,SEEK_SET); // Always start at offset 0, whatever mode opened
        }
      }
    }
    else
    {
      D0=Tos.FileNotFound;
    }
  }
  if(D0>=0 && GemdosCommand==F_CREATE)
  { // Fcreate

#ifdef WIN32
    // We need to set the creation date of the file, but on Windows deleting the
    // file and re-creating it immediately doesn't work!
    HANDLE h=CreateFile(PCFilename,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_FLAG_WRITE_THROUGH,NULL);
    if(h!=INVALID_HANDLE_VALUE)
    {
      FILETIME ft;
      GetSystemTimeAsFileTime(&ft);
      SetFileTime(h,&ft,&ft,&ft);
      CloseHandle(h);
      StemdosNewFile.attrib=0;
#if defined(SSE_LEAN_AND_MEAN)
      // knowing ST has same attributes as WIN32 this can be simplified
      StemdosNewFile.attrib|=attr&(FA_READONLY|FA_HIDDEN|FA_SYSTEM);
#else
      if(attr&FA_HIDDEN) 
        StemdosNewFile.attrib|=FILE_ATTRIBUTE_HIDDEN;
      if(attr&FA_SYSTEM) 
        StemdosNewFile.attrib|=FILE_ATTRIBUTE_SYSTEM;
      if(attr&FA_READONLY) 
        StemdosNewFile.attrib|=FILE_ATTRIBUTE_READONLY;
#endif
      // QUESTION: Is this closing/reopening necessary any more?
      SetFileAttributes(PCFilename,0);
      fp=fopen(PCFilename,"w+b");
      if(fp!=NULL)
        FSEEK(fp,0,SEEK_SET); // Always start at offset 0
    }
#endif//WIN32

#ifdef UNIX // (some code duplication for IDE)
    DeleteFile(PCFilename);
    fp=fopen(PCFilename,"wb");
    if(fp)
    {
      fclose(fp);
      StemdosNewFile.attrib=0;
      // UNIX attributes are different from ST
      if(attr&2) StemdosNewFile.attrib|=FILE_ATTRIBUTE_HIDDEN;
      if(attr&4) StemdosNewFile.attrib|=FILE_ATTRIBUTE_SYSTEM;
      if(attr&1) StemdosNewFile.attrib|=FILE_ATTRIBUTE_READONLY;
      // QUESTION: Is this closing/reopening necessary any more?
      SetFileAttributes(PCFilename,0);
      fp=fopen(PCFilename,"w+b");
      if(fp!=NULL) 
        FSEEK(fp,0,SEEK_SET); // Always start at offset 0
    }
#endif//UNIX

    if(fp==NULL) 
      D0=Tos.PathNotFound;
  }
  if(D0>=0)
  { //succeeded
    StemdosNewFile.open=true;
    StemdosNewFile.fp=fp;
    StemdosNewFile.Pexec=Stemdos.PexecListPtr;
    StemdosNewFile.filename=PCFilename;
    StemdosNewFile.date=StemdosNewFile.time=0;
    StemdosRteAction=STEMDOS_RTE_DUP; //get file handle
    on_rte=ON_RTE_STEMDOS;
    on_rte_interrupt_depth=interrupt_depth+1;
    StemdosCallFdup();
  }
  else  // Failed to open file
    StemdosFinish(); // Error code is in D0
  return D0;
}


void TStemdos::CloseAllFiles() { // on reset, steem exit
  if(StemdosNewFile.open) 
    StemdosCloseFile(&StemdosNewFile);
  if(fStemdosPexec)
  {
    fclose(fStemdosPexec);
    fStemdosPexec=NULL;
  }
  for(int n=GEMDOS_STD_HANDLES;n<MAX_STEMDOS_FILES;n++)
  {
    if(StemdosFile[n].open) 
      StemdosCloseFile(&(StemdosFile[n]));
  }
}


void StemdosCloseFile(TStemdosFile* const sfs) {
  // close PC file
  if(sfs->open)
  {
    fclose(sfs->fp);
    sfs->fp=NULL;
    if(sfs->time || sfs->date)
    {
#ifdef WIN32
      DWORD attrib=GetFileAttributes(sfs->filename);
      HANDLE f=CreateFile(sfs->filename,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,attrib,NULL);
      if(f!=INVALID_HANDLE_VALUE)
      {
        FILETIME ft,gmt_ft;
        DosDateTimeToFileTime(sfs->date,sfs->time,&ft);
        // Convert to GMT (all ST times are local time, all PC times are GMT)
        LocalFileTimeToFileTime(&ft,&gmt_ft);
        SetFileTime(f,&gmt_ft,NULL,NULL);
        CloseHandle(f);
      }
#endif
#ifdef UNIX
      ///// How?
#endif
    }
    if(sfs->attrib)
    {
      DWORD win_attr=GetFileAttributes(sfs->filename);
      win_attr&=~(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_READONLY);
      win_attr|=sfs->attrib&(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_READONLY);
      SetFileAttributes(sfs->filename,win_attr);
      sfs->attrib=0;
    }
    sfs->open=false;
    sfs->h=0;
  }
}


void StemdosRead(WORD const h,MEM_ADDRESS const mySP) {
  DWORD count=SafeLPeek(mySP+4);
#if defined(SSE_STATS)
  Stats.nHdsector+=count/SECTOR_SIZE;
#endif
  MEM_ADDRESS buf=SafeLPeek(mySP+8);
  DWORD c=0;
  while(c<count)
  {
    int i=FGETC(StemdosFile[h].fp);
    if(i==EOF) 
      break;
    c++;
    SafePoke(buf,(BYTE)i);
    buf++;
  }
  D0=c; //number of characters read
  OSD_HD_LED;
  TRACE_LOG2("%06X Process %d F_READ %d %s %d %d\n",old_pc,Stemdos.PexecListPtr,
             h,CHECKPATH(StemdosFile[h].filename.Text),count,D0);
}


void StemdosWrite(WORD const h,MEM_ADDRESS const mySP) {
  DWORD count=SafeLPeek(mySP+4);
#if defined(SSE_STATS)
  Stats.nHdsector+=count/SECTOR_SIZE;
#endif
  MEM_ADDRESS buf=SafeLPeek(mySP+8);
  DWORD c=0,i;
  while(c<count)
  {
    i=SafePeek(buf);
    buf++;
    c++;
    if(FPUTC(i,StemdosFile[h].fp)==EOF)
    { //error
      D0=Tos.AccessDenied;
      return;
    }
  }
  D0=c; //number of characters written
  OSD_HD_LED;
  TRACE_LOG2("%06X Process %d F_WRITE %d %s %d %d\n",old_pc,Stemdos.PexecListPtr,
             h,CHECKPATH(StemdosFile[h].filename.Text),count,D0);
}


void StemdosSeek(WORD const h,MEM_ADDRESS const mySP) {
  LONG offset=SafeLPeek(mySP+2); // must be signed for sign extension: Pure C debugger
  WORD seekmode=SafeDPeek(mySP+8);
  INT64 new_pos=-1,old_pos=FTELL(StemdosFile[h].fp);
  INT64 file_len=GetFileLength(StemdosFile[h].fp);
  switch(seekmode) {
  case 0:
    new_pos=offset;
    break;
  case 1:
    new_pos=old_pos+offset;
    break;
  case 2:
    new_pos=file_len+offset; // Offset must be negative!
  }
  int error=TRUE;
  if(new_pos>=0 && new_pos<=file_len)
  {
    error=FSEEK(StemdosFile[h].fp,new_pos,SEEK_SET);
    // error might be 0 even if function fails - DOS doesn't verify!
    if(error)
    {
      TRACE_LOG("SEEK ERROR %d\n",error);
      FSEEK(StemdosFile[h].fp,old_pos,SEEK_SET);
    }
  }
  D0=(error) ? (DWORD)Tos.RangeError : (DWORD)FTELL(StemdosFile[h].fp);
  TRACE_LOG2("%06X Process %d F_SEEK %d %s %d %d %d\n",old_pc,Stemdos.PexecListPtr,
             h,CHECKPATH(StemdosFile[h].filename.Text),seekmode,offset,D0);
}


void StemdosFdatime(WORD const h,MEM_ADDRESS const mySP) {
/*GEMDOS versions below 0.15 yielded very unpredictable results with this call
and should therefore be avoided.*/
  MEM_ADDRESS timeptr=SafeLPeek(mySP+2);
  WORD flag=SafeDPeek(mySP+8);
  if(flag==0)
  { //read
    WORD date,time;
    if(StemdosFile[h].time||StemdosFile[h].date)
    {
      date=StemdosFile[h].date;
      time=StemdosFile[h].time;
    }
    else
    {
#ifdef WIN32
      FILETIME local_ft;
      DirSearch ds(StemdosFile[h].filename);
#if 1 //from Petari
      FileTimeToLocalFileTime(&ds.LastWriteTime,&local_ft); // Convert from GMT
#else
      FileTimeToLocalFileTime(&ds.CreationTime,&local_ft); // Convert from GMT
#endif
      FileTimeToDosDateTime(&local_ft,&date,&time);
#endif
#ifdef UNIX
      struct stat s;
      fstat(fileno(StemdosFile[h].fp),&s);
      DWORD ddt=TMToDOSDateTime(localtime(&(s.st_ctime)));
      date=HIWORD(ddt);
      time=LOWORD(ddt);
#endif
    }
    SafePoke(timeptr+0,HIBYTE(time));
    SafePoke(timeptr+1,LOBYTE(time));
    SafePoke(timeptr+2,HIBYTE(date));
    SafePoke(timeptr+3,LOBYTE(date));
  }
  else
  {
    StemdosFile[h].time=MAKEWORD(SafePeek(timeptr+1),SafePeek(timeptr+0));
    StemdosFile[h].date=MAKEWORD(SafePeek(timeptr+3),SafePeek(timeptr+2));
  }
#if defined(SSE_ENABLE_TRACE_LOG)
  int year,month,day,hour,minute,second;
  Stemdos.ConvertDate(StemdosFile[h].date,year,month,day);
  Stemdos.ConvertTime(StemdosFile[h].time,hour,minute,second);
  TRACE_LOG("%06X Process %d F_DATIME %c %X (%02d-%02d-%04d) %X (%02d:%02d:%02d)\n",
            old_pc,Stemdos.PexecListPtr,(flag) ? 'W' : 'R',day,month,year,hour,minute,second);
#endif
  D0=Tos.NoError;
}


void StemdosFsfirst(MEM_ADDRESS const mySP) {
  char fsn=-1;
  StemdosGetPCPath();
  // Search for search with this DTA
  for(char n=0;n<MAX_STEMDOS_FSNEXT_STRUCTS;n++)
  {
    if(Stemdos.FsnextData[n].dta==Stemdos.Dta)
    {
      fsn=n;
      break;
    }
  }
  if(fsn==-1)
  {
    // New search
    for(char n=0;n<MAX_STEMDOS_FSNEXT_STRUCTS;n++)
    {
      if(Stemdos.FsnextData[n].dta==0)
      {
        fsn=n;
        break;
      }
    }
  }
  if(fsn==-1)
  {
    // There are 100 active searches!!
    DWORD oldest=0xffffffff;
    char oldest_n=0;
    for(char n=0;n<MAX_STEMDOS_FSNEXT_STRUCTS;n++)
    {
      if(Stemdos.FsnextData[n].start_hbl-oldest<0)
      {
        oldest=Stemdos.FsnextData[n].start_hbl;
        oldest_n=n;
      }
    }
    fsn=oldest_n;
    TRACE_LOG2("Replace search index %d\n",fsn);
  }
  Stemdos.FsnextData[fsn].dta=Stemdos.Dta;
  Stemdos.FsnextData[fsn].NextFile="";         // Get first file 
  Stemdos.FsnextData[fsn].path=PCFilename;
  Stemdos.FsnextData[fsn].attr=SafeDPeek(mySP+6); //attributes
  Stemdos.FsnextData[fsn].start_hbl=hbl_count;
  SafeLPoke(Stemdos.Dta,0x0baddeed); //magic number for STEMDOS search $baddeed
  SafePoke(Stemdos.Dta+DTA_SEARCH_NUMBER,fsn);  //store number of search
  StemdosFsnext();
}


DWORD PCAttrToSTAttr(DWORD const PCAttr) { // not read-only
#if defined(WIN32) && defined(SSE_LEAN_AND_MEAN)
  DWORD STAttr=PCAttr&(FA_HIDDEN|FA_SYSTEM|FA_DIR);
#else
  int STAttr=0;
  if(PCAttr&FILE_ATTRIBUTE_HIDDEN)    
    STAttr|=FA_HIDDEN;
  if(PCAttr&FILE_ATTRIBUTE_SYSTEM)    
    STAttr|=FA_SYSTEM;
  if(PCAttr&FILE_ATTRIBUTE_DIRECTORY) 
    STAttr|=FA_DIR;
#endif
  return STAttr;
}


// used for FIRST & NEXT!
#ifdef SSE_421R0 // refactoring

void StemdosFsnext() {
  char fsn=SafePeek(Stemdos.Dta+DTA_SEARCH_NUMBER); // Search number
  if(fsn<0 || fsn>=MAX_STEMDOS_FSNEXT_STRUCTS) // not in range
  {
    if(fsn!=-1) // -1 = Flag to indicate finished search
    {
      fsn=-1;
      for(char n=0;(fsn<0 && n<MAX_STEMDOS_FSNEXT_STRUCTS);n++) //look for DTA match
        if(Stemdos.FsnextData[n].dta==Stemdos.Dta)
          fsn=n; // match found
    }
    if(fsn<0)
    {
      D0=Tos.NoMoreFiles;
      TRACE_LOG("%s %d\n","ERROR",D0);
      return;
    }
    SafePoke(Stemdos.Dta+DTA_SEARCH_NUMBER,fsn);
  }
  WORD CreateDate=0,CreateTime=0;
  bool bFirstFile=false,bLastFile=false;
  TStemdosFsnext *find_struct=&(Stemdos.FsnextData[fsn]);
  if(find_struct->NextFile=="") // as set by StemdosFsfirst()
  {
    bFirstFile=true;
    D0=Tos.FileNotFound; // potential error
  }
  else
    D0=Tos.NoMoreFiles; // eventual error
  if(find_struct->attr==FA_VOLUME) //search for volume label
  {
    SafePoke(Stemdos.Dta+DTA_ATTRIB,1+8); //file attributes, volume label
    for(int n=DTA_TIME;n<DTA_NAME;n++) // no lpoke, is the address even?
      SafePoke(Stemdos.Dta+n,0);
    char *VolumeName=VOLUME_NAME;
    for(int n=0;VolumeName[n]!='\0';n++) 
      SafePoke(Stemdos.Dta+DTA_NAME+n,VolumeName[n]);
    bLastFile=true; // Only 1 volume label (thank goodness)
    D0=Tos.NoError;
  }
  else // look for a file or folder
  {
    DirSearch ds; // Cross-platform class to retrieve directory contents on the PC
    ds.st_only=true;
    if(ds.Find(find_struct->path))
    {
      do
      {
        char *fname=StrUpperNoSpecial(ds.ShortName);
        // NextFile contains the name of the next matching file
        if(bFirstFile || IsSameStr_I(find_struct->NextFile,fname))
        {
          // Check if found file's attributes match what you asked for
          DWORD that_files_attr=PCAttrToSTAttr(ds.Attrib);
          if((find_struct->attr&that_files_attr)==that_files_attr)
          {
            if(ds.Attrib&FILE_ATTRIBUTE_READONLY) // because PCAttrToSTAttr doesn't do it
              that_files_attr|=FA_READONLY; // if FILE_ATTRIBUTE_READONLY=FA_READONLY, compiler may optimise
            SafePoke(Stemdos.Dta+DTA_ATTRIB,(BYTE)that_files_attr); //file attributes
            // Petari: must use last write time as create time
#ifdef WIN32
            FILETIME lft;
            FileTimeToLocalFileTime(&ds.LastWriteTime,&lft); // File time is always GMT
            FileTimeToDosDateTime(&lft,&CreateDate,&CreateTime);
#endif
#ifdef UNIX
            CreateDate=(WORD)(ds.LastWriteTime>>16);
            CreateTime=(WORD)(ds.LastWriteTime);
#endif
            SafePoke(Stemdos.Dta+DTA_TIME,HIBYTE(CreateTime)); //file clock time
            SafePoke(Stemdos.Dta+DTA_TIME+1,LOBYTE(CreateTime)); //file clock time
            SafePoke(Stemdos.Dta+DTA_DATE,HIBYTE(CreateDate)); //file date
            SafePoke(Stemdos.Dta+DTA_DATE+1,LOBYTE(CreateDate)); //file date
            DU32 duSize(ds.SizeLow);
            SafePoke(Stemdos.Dta+DTA_LEN,duSize.d8[B3]); //file size, high byte
            SafePoke(Stemdos.Dta+DTA_LEN+1,duSize.d8[B2]); //file size, mid-high byte
            SafePoke(Stemdos.Dta+DTA_LEN+2,duSize.d8[B1]); //file size, mid-low byte
            SafePoke(Stemdos.Dta+DTA_LEN+3,duSize.d8[B0]); //file size, low byte
            for(int n=0;n<TOS_FILENAME_LEN;n++) 
              SafePoke(Stemdos.Dta+DTA_NAME+n,fname[n]);
            D0=Tos.NoError;
          }
        }
        if(D0!=Tos.NoError) // if found no matching file, check next file
          if(!ds.Next())
            bLastFile=true;
      } while(D0!=Tos.NoError && !bLastFile);

       // Find next matching file (for next call to fsnext)
      for(bool bFoundNext=false;!bFoundNext&&!bLastFile;)
      {
        if(!ds.Next())
          bLastFile=true;
        else
        {
          DWORD STAttr=PCAttrToSTAttr(ds.Attrib);
          bFoundNext=((find_struct->attr&STAttr)==STAttr);
          if(bFoundNext)
            find_struct->NextFile=StrUpperNoSpecial(ds.ShortName);
        }
      }//nxt        
    }
    else
      // ds.Find failed
      bLastFile=true; // No files
  }
  if(D0<0 || bLastFile)
  { // Error or finished
    TRACE_LOG("%s %d\n","ERROR",D0);
    find_struct->dta=0;
    find_struct->path="";
    SafePoke(Stemdos.Dta+DTA_SEARCH_NUMBER,255); // return no more files next time you fsnext
  }
#if defined(SSE_ENABLE_TRACE_LOG)
  if(TRACE_LOG3_ENABLED)
  {
    char fname[TOS_FILENAME_LEN+1];
    for(int n=0;n<TOS_FILENAME_LEN;n++)
      fname[n]=SafePeek(Stemdos.Dta+30+n);
    fname[TOS_FILENAME_LEN]='\0';
    int year,month,day,hour,minute,second;
    Stemdos.ConvertDate(CreateDate,year,month,day);
    Stemdos.ConvertTime(CreateTime,hour,minute,second);
    TRACE_LOG("%06X Process %d %s %s (%02d-%02d-%04d %02d:%02d:%02d): %d\n",old_pc,
               Stemdos.PexecListPtr,(GemdosCommand==F_SFIRST) ? "F_SFIRST" : "F_SNEXT",fname,
               day,month,year,hour,minute,second,D0);
  }
#endif
}

#else//#ifdef SSE_421R0

void StemdosFsnext() {
  WORD CreateDate=0,CreateTime=0;
  int fsn=SafePeek(Stemdos.Dta+4); // Search number
  if(fsn>=0 && fsn<MAX_STEMDOS_FSNEXT_STRUCTS) // In range
  { 
#ifdef DEAD_C0DE // fix old bug F_SNEXT (Geneva/NeoDesk)
    if(Stemdos.FsnextData[fsn].dta!=Stemdos.Dta)
    {
      TRACE("Not the current search\n");
      fsn=-1; // Not the current search
    }
#endif
  }
  else
  {
    if(fsn==255) // Flag to indicate finished search
    { 
      D0=Tos.NoMoreFiles;
      TRACE_LOG("%s %d\n","ERROR",D0);
      return;
    }
    fsn=-1;
  }
  if(fsn<0)
  {
    int n;
    for(n=0;n<MAX_STEMDOS_FSNEXT_STRUCTS;n++) //look for DTA match
    {
      if(Stemdos.FsnextData[n].dta==Stemdos.Dta)
      { 
        fsn=n;
        SafePoke(Stemdos.Dta+4,(BYTE)fsn);
        break;
      }
    }
    if(n>=MAX_STEMDOS_FSNEXT_STRUCTS)
    { //no match found
      D0=Tos.NoMoreFiles;
      TRACE_LOG("%s %d\n","ERROR",D0);
      return;
    }
  }
  TStemdosFsnext *find_struct=&(Stemdos.FsnextData[fsn]);
  bool bFirstFile=(find_struct->NextFile=="");
  D0=(bFirstFile) ? Tos.FileNotFound : Tos.NoMoreFiles;
  bool bLastFile=false;
  if(find_struct->attr==FA_VOLUME)
  { //search for volume label
    SafePoke(Stemdos.Dta+21,1+8); //file attributes, volume label
    for(int n=22;n<30;n++) // no lpoke, is the address even?
      SafePoke(Stemdos.Dta+n,0);
    for(int n=0;n<14;n++) 
      SafePoke(Stemdos.Dta+30+n,EasyStr("STEMDISK.MNT")[n]);
    bLastFile=true; // Only 1 volume label (thank goodness)
    D0=Tos.NoError;
  }
  else
  {
    DirSearch ds;
    ds.st_only=true;
    if(ds.Find(find_struct->path))
    {
      for(;;) 
      {
        char *fname=StrUpperNoSpecial(ds.ShortName);
        // NextFile contains the name of the next matching file
        if(bFirstFile || IsSameStr_I(find_struct->NextFile,fname))
        {
          // Check if found file's attributes match what you asked for
          DWORD that_files_attr=PCAttrToSTAttr(ds.Attrib);
          if((find_struct->attr&that_files_attr)==that_files_attr)
          {
#if defined(WIN32) && defined(SSE_LEAN_AND_MEAN)
            that_files_attr|=(ds.Attrib&FA_READONLY);
#else
            if(ds.Attrib&FILE_ATTRIBUTE_READONLY) 
              that_files_attr|=FA_READONLY;
#endif
            SafePoke(Stemdos.Dta+21,(BYTE)that_files_attr); //file attributes
#ifdef WIN32
            FILETIME lft;
#if 1 //from Petari
            FileTimeToLocalFileTime(&ds.LastWriteTime,&lft); // File time is always GMT
#else
            FileTimeToLocalFileTime(&ds.CreationTime,&lft); // File time is always GMT
#endif
            FileTimeToDosDateTime(&lft,&CreateDate,&CreateTime);
#endif
#ifdef UNIX
#if 1 //from Petari
            CreateDate=(WORD)(ds.LastWriteTime>>16);
            CreateTime=(WORD)(ds.LastWriteTime);
#else
            CreateDate=(WORD)(ds.CreationTime>>16);
            CreateTime=(WORD)(ds.CreationTime);
#endif
#endif
            SafePoke(Stemdos.Dta+22,HIBYTE(CreateTime)); //file clock time
            SafePoke(Stemdos.Dta+23,LOBYTE(CreateTime)); //file clock time
            SafePoke(Stemdos.Dta+24,HIBYTE(CreateDate)); //file date
            SafePoke(Stemdos.Dta+25,LOBYTE(CreateDate)); //file date
            SafePoke(Stemdos.Dta+26,(BYTE)((ds.SizeLow&0xff000000)>>24)); //file size, high byte
            SafePoke(Stemdos.Dta+27,(BYTE)((ds.SizeLow&0xff0000)>>16)); //file size, mid-high byte
            SafePoke(Stemdos.Dta+28,(BYTE)((ds.SizeLow&0xff00)>>8)); //file size, mid-low byte
            SafePoke(Stemdos.Dta+29,(BYTE)(ds.SizeLow&0xff)); //file size, low byte
            for(int n=0;n<14;n++) 
              SafePoke(Stemdos.Dta+30+n,fname[n]);
            D0=Tos.NoError;
            break;
          }
        }
        if(!ds.Next())
        {
          bLastFile=true;
          break;
        }
      }//nxt
      if(!bLastFile)
      {
        // Find next matching file (for next call to fsnext)
        for(;;) 
        {
          if(!ds.Next())
          {
            bLastFile=true;
            break;
          }
          else
          {
            DWORD STAttr=PCAttrToSTAttr(ds.Attrib);
            if((find_struct->attr&STAttr)==STAttr)
            {
              find_struct->NextFile=StrUpperNoSpecial(ds.ShortName);
              break;
            }
          }
        }//nxt
      }
    }
    else
      // ds.Find failed
      bLastFile=true; // No files
  }
  if(D0<0 || bLastFile)
  { // Error or finished
    TRACE_LOG("%s %d\n","ERROR",D0);
    find_struct->dta=0;
    find_struct->path="";
    SafePoke(Stemdos.Dta+4,255); // return no more files next time you fsnext
  }
#if defined(SSE_ENABLE_TRACE_LOG)
  char fname[15];
  for(int n=0;n<14;n++) 
    fname[n]=SafePeek(Stemdos.Dta+30+n);
  fname[14]='\0';
  int year,month,day,hour,minute,second;
  Stemdos.ConvertDate(CreateDate,year,month,day);
  Stemdos.ConvertTime(CreateTime,hour,minute,second);
  TRACE_LOG2("%06X Process %d %s %s (%02d-%02d-%04d %02d:%02d:%02d): %d\n",old_pc,
             Stemdos.PexecListPtr,(GemdosCommand==F_SFIRST) ? "F_SFIRST" : "F_SNEXT",fname,
             day,month,year,hour,minute,second,D0);
#endif
}

#endif////#ifdef SSE_421R0


void StemdosDfree(int const dr,MEM_ADDRESS const buffer) {
#ifdef WIN32
  char root_path[16];
  strncpy(root_path,Stemdos.MountPath[dr].Text,2);
  root_path[2]='\0';
  strcat(root_path,"\\");
  DWORD dw[4];
  GetDiskFreeSpace(root_path,&(dw[3]),&(dw[2]),&(dw[0]),&(dw[1]));
  // max clusers under 0x3e88888 (about 64Mb)
  // Clusters free * sectors per cluster * bytes per sector = num bytes free
  if(DWORDLONG(dw[0])*dw[3]*dw[2]>=0x3e88888)
    dw[0]=MAX(0x3e88888/(dw[3]*dw[2]),(DWORD)1);
  for(int n=0;n<4;n++) 
    SafeLPoke(buffer+n*4,dw[n]);
#endif
#ifdef UNIX
  DWORD free_units=65536,total_units=100000;
  DWORD bytes_per_sector=SECTOR_SIZE,sectors_per_unit=2;    // 64Mb
#ifdef LINUX
  struct statfs sfs;
  if(statfs(Stemdos.MountPath[dr],&sfs)==0)
  {
// f_bsize;    /* optimal transfer block size */
// f_blocks;   /* total data blocks in file system */
// f_bfree;    /* free blocks in fs */
// f_bavail;   /* free blocks avail to non-superuser */
    sectors_per_unit=sfs.f_bsize/bytes_per_sector;
    total_units=sfs.f_blocks;
    free_units=sfs.f_bavail;
  }
  if(((long double)free_units) * bytes_per_sector*sectors_per_unit>=0x3e88888)
  { // 64Mb
    free_units=MAX(0x3e88888/(bytes_per_sector*sectors_per_unit),1);
  }
#endif
  SafeLPoke(buffer+0,free_units);
  SafeLPoke(buffer+4,total_units);
  SafeLPoke(buffer+8,bytes_per_sector);
  SafeLPoke(buffer+12,sectors_per_unit);
#endif//UNIX
  D0=Tos.NoError;
  TRACE_LOG("%06X Process %d D_FREE %d\n",old_pc,Stemdos.PexecListPtr,
            (SafeLPeek(buffer)/MEM_1MB)*SafeLPeek(buffer+8)*SafeLPeek(buffer+12));
}


void StemdosMkdir() {
  StemdosGetPCPath();
  if(!CreateDirectory(PCFilename,NULL)) // error
  {
#ifdef WIN32
    if(GetLastError()!=ERROR_PATH_NOT_FOUND)
      D0=Tos.AccessDenied;
    else
#endif
      D0=Tos.PathNotFound;
  }
  else
    D0=Tos.NoError;
  TRACE_LOG("%06X Process %d D_CREATE %s: %d\n",old_pc,Stemdos.PexecListPtr,
            CHECKPATH(StemdosFilename.Text),D0);
  OSD_HD_LED;
}


void StemdosRmdir() {
  StemdosGetPCPath();
  if(!RemoveDirectory(PCFilename)) // error
  {
#ifdef WIN32
    if(GetLastError()!=ERROR_PATH_NOT_FOUND)
      D0=Tos.AccessDenied;
    else
#endif
      D0=Tos.PathNotFound;
  }
  else
    D0=Tos.NoError;
  TRACE_LOG("%06X Process %d D_DELETE %s: %d\n",old_pc,Stemdos.PexecListPtr,
            CHECKPATH(StemdosFilename.Text),D0);
  OSD_HD_LED;
}


void StemdosFdelete() {
  StemdosGetPCPath();
  StemdosSearchWildcardPCPath();
  if(!DeleteFile(PCFilename)) // error
  {
#ifdef WIN32
    DWORD er=GetLastError();
    switch(er) {
    case ERROR_SHARING_VIOLATION: // file is open
      D0=Tos.AccessDenied;
      break;
    default:
      D0=Tos.FileNotFound;
    }
#endif
#ifdef UNIX
     D0=Tos.FileNotFound;
#endif
  }
  else
    D0=Tos.NoError;
  TRACE_LOG("%06X Process %d F_DELETE %s: %d\n",old_pc,Stemdos.PexecListPtr,
            CHECKPATH(StemdosFilename.Text),D0);
  OSD_HD_LED;
}


void StemdosFattrib(WORD const SetAttr,WORD const STAttr) {
  OSD_HD_LED;
  StemdosGetPCPath();
  StemdosSearchWildcardPCPath();
  if(SetAttr)
  {
    if(STAttr&FA_VOLUME) //trying to change a file into a volume label, the fools!
      D0=Tos.AccessDenied;
    else
    {
      DWORD PCAttr=GetFileAttributes(PCFilename);
#if defined(WIN32) && defined(SSE_LEAN_AND_MEAN)
      if(((PCAttr&FA_DIR))^((STAttr&FA_DIR)))
#else
      if((!(PCAttr&FILE_ATTRIBUTE_DIRECTORY))^(!(STAttr&FA_DIR)))
#endif
      { //bad!
        D0=Tos.AccessDenied; //not a chance!
      }
      else
      {
        PCAttr&=~(FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM|FILE_ATTRIBUTE_READONLY);
#if defined(WIN32) && defined(SSE_LEAN_AND_MEAN)
        PCAttr|=STAttr&(FA_SYSTEM|FA_HIDDEN|FA_READONLY);
#else
        if(STAttr&FA_HIDDEN)    
          PCAttr|=FILE_ATTRIBUTE_HIDDEN;
        if(STAttr&FA_SYSTEM)    
          PCAttr|=FILE_ATTRIBUTE_SYSTEM;
        if(STAttr&FA_READONLY)    
          PCAttr|=FILE_ATTRIBUTE_READONLY;
#endif
        if(SetFileAttributes(PCFilename,PCAttr)) // TODO: only on ST, normal for PC
          D0=STAttr; //succeed
        else
          D0=Tos.AccessDenied;
      }
    }
  }
  else
  { //read attributes
    DWORD PCAttr=GetFileAttributes(PCFilename);
    if(PCAttr==INVALID_FILE_ATTRIBUTES)
      D0=Tos.FileNotFound;
    else
    {
#if defined(WIN32) && defined(SSE_LEAN_AND_MEAN)
      D0=(PCAttr&(FA_READONLY|FA_HIDDEN|FA_SYSTEM|FA_DIR));
#else
      D0=0;
      if(PCAttr&FILE_ATTRIBUTE_READONLY)
        D0|=FA_READONLY;
      if(PCAttr&FILE_ATTRIBUTE_SYSTEM)   
        D0|=FA_SYSTEM;
      if(PCAttr&FILE_ATTRIBUTE_HIDDEN)    
        D0|=FA_HIDDEN;
      if(PCAttr&FILE_ATTRIBUTE_DIRECTORY) 
        D0|=FA_DIR;
#endif
    }
  }
  TRACE_LOG("%06X Process %d F_ATTRIB %s %c: %d\n",old_pc,Stemdos.PexecListPtr,
            CHECKPATH(StemdosFilename.Text),(SetAttr) ? 'W' : 'R',D0);
}


void StemdosRename(EasyStr NewFilename) {
  OSD_HD_LED;
  //try to rename StemdosFilename to StemdosNewFilename
  StrUpperNoSpecial(NewFilename.Text);
  if(NewFilename[1]==':')
  {
    if(toupper(NewFilename[0])!=toupper(StemdosFilename[0]))
    { //trying to rename accross drives
      D0=Tos.InvalidDrive;
      return;
    }
  }
  StemdosGetPCPath();
  if(StemdosSearchWildcardPCPath()==0xffffffff)
  {
    D0=Tos.FileNotFound;
    return;
  }
  EasyStr f1=PCFilename;
  StemdosFilename=NewFilename;
  if(StemdosGetFilePath()==STEMDOS_FILE_IS_GEMDOS)
  { // Extend to full path
    D0=Tos.InvalidDrive;
    return;
  }
  StemdosGetPCPath();
  if(Exists(PCFilename))
    D0=Tos.AccessDenied;
  else
  {
    if(MoveFile(f1,PCFilename))
      D0=Tos.NoError;
    else
      D0=Tos.AccessDenied;
  }
  TRACE_LOG("%06X Process %d F_RENAME %s: %d\n",old_pc,Stemdos.PexecListPtr,
            CHECKPATH(f1.Text),CHECKPATH(NewFilename.Text),D0);
}


void StemdosFinish() {
  on_rte=ON_RTE_NONE;
  SR=StemdosSaveSR;  //restore status
  UPDATE_FLAGS;
  Cpu.StemdosCheckFlags();
}

#ifdef VC_BUILD
#define GOTO_FINISHED goto Finished;
#else
#define GOTO_FINISHED {TRACE_LOG("PEXEC: %d\n",D0);return D0;}
#endif

int StemdosPexec() {
  //called from StemdosRte, nothing done after this fn called.
  // Stack is as for original GEMDOS call, PC is at os_gemdos_vector
  if(D0<0)
  {
    TRACE_LOG("TOS %s %d\n","ERROR",D0);
    fclose(fStemdosPexec);
    fStemdosPexec=NULL;
    StemdosFinish();
    StemdosSkipGemdos();
    GOTO_FINISHED
  }
  // when we come here, basepage has just been created, address
  // was returned in D0
  MEM_ADDRESS const basepage=D0;
  WORD PRG_magic=STfileReadWord(fStemdosPexec);
  if(PRG_magic!=0x601a)
  { //not executable
    TRACE_LOG("PRG_magic %s %X\n","ERROR",PRG_magic);
    D0=Tos.InvalidProgramLoadFormat;
    fclose(fStemdosPexec);
    fStemdosPexec=NULL;
    SR=StemdosSaveSR;  //restore status - we don't restore the old status after term
    UPDATE_FLAGS;
    StemdosCallMfree(Stemdos.PexecList[Stemdos.PexecListPtr]);
    StemdosRteAction=STEMDOS_RTE_MFREE2; //correct stack after finish
    GOTO_FINISHED
  }
  Tos.PRG_tsize=STfileReadLong(fStemdosPexec);
  Tos.PRG_dsize=STfileReadLong(fStemdosPexec);
  Tos.PRG_bsize=STfileReadLong(fStemdosPexec);
  Tos.PRG_ssize=STfileReadLong(fStemdosPexec);
  TRACE_LOG("Size TEXT %d DATA %d BSS %d SYMBOL %d\n",
            Tos.PRG_tsize,Tos.PRG_dsize,Tos.PRG_bsize,Tos.PRG_ssize);
  FSEEK(fStemdosPexec,0x1C,SEEK_SET); //seek to end of header (Text Segment)
  MEM_ADDRESS textbase=basepage+0x100;
  MEM_ADDRESS lo_tpa=textbase,ad=textbase;
  MEM_ADDRESS hi_tpa=SafeLPeek(basepage+0x4); //basepage+4 contains hi-tpa
  if((textbase+Tos.PRG_tsize+Tos.PRG_dsize+Tos.PRG_bsize)>hi_tpa)
  {
    TRACE_LOG("TPA overflow %s\n","ERROR");
    D0=Tos.InsufficientMemory;
    fclose(fStemdosPexec);
    fStemdosPexec=NULL;
    StemdosPterm();
    StemdosCallMfree(Stemdos.PexecList[Stemdos.PexecListPtr]);
    SR=StemdosSaveSR;  //restore status - we don't restore the old status after term
    UPDATE_FLAGS;
    StemdosRteAction=STEMDOS_RTE_MFREE2; //correct stack after finish
    GOTO_FINISHED
  }
  TRACE_LOG("Starting Process %d basepage $%X",Stemdos.PexecListPtr+1,basepage);
  // Clear entire heap, correct? Yes
  if(lo_tpa<mem_len && hi_tpa<mem_len)
  {
    int bytes=hi_tpa-lo_tpa;
    if(bytes>0)
    {
      BYTE *pFirst=lpPEEK(lo_tpa),*pLast=lpPEEK(hi_tpa-1);
      if(pLast<pFirst)
        pFirst=pLast;
      ZeroMemory(pLast,bytes);
    }
  }
  // tracing the total RAM allocated allows us to see if there's a leak or not
  TRACE_LOG(" RAM %dK TEXT $%X",(hi_tpa-textbase)/1024,textbase);
  //set up basepage
  SafeLPoke(basepage+0x8,ad); //start of text
  SafeLPoke(basepage+0xc,Tos.PRG_tsize); //length of text
#if defined(SSE_STATS)
  Stats.nHdsector+=(Tos.PRG_tsize+Tos.PRG_dsize)/SECTOR_SIZE;
#endif
  STfileReadToSTMemory(fStemdosPexec,ad,Tos.PRG_tsize); //load in text segment
  ad+=Tos.PRG_tsize;
  TRACE_LOG(" DATA $%X",ad);
  SafeLPoke(basepage+0x10,ad); //start of data
  SafeLPoke(basepage+0x14,Tos.PRG_dsize); //length of data
  STfileReadToSTMemory(fStemdosPexec,ad,Tos.PRG_dsize); //load in data segment
  ad+=Tos.PRG_dsize;
  SafeLPoke(basepage+0x18,ad); //start of bss
  SafeLPoke(basepage+0x1c,Tos.PRG_bsize); //length of bss
  // BSS already zeroed (must be in TPA!)
  FSEEK(fStemdosPexec,Tos.PRG_ssize,SEEK_CUR); //seek to end of symbol table
  LONG fixup_ad=STfileReadLong(fStemdosPexec);
  TRACE_LOG(" BSS $%X FIXUP %X\n",ad,fixup_ad);
  /*This LONG indicates the first location in the executable (as an offset 
    from the  beginning) containing a longword needing a fixup. 
    A 0 means there are no fixups.*/
  if(fixup_ad && !STfileReadError)
  { // longs to relocate
    ad=SafeLPeek(fixup_ad+textbase);
    SafeLPoke(fixup_ad+textbase,ad+textbase); // fix 1st
    TRACE_LOG2("fixup $%X to $%X as $%X\n",ad,fixup_ad+textbase,ad+textbase);
    BYTE b;
    /*This area contains a stream of BYTEs containing fixup information.
      Each byte has a significance as follows:
      Value         Meaning
      0             End of list.
      1             Advance 254 bytes.
      2-254 (even)  Advance this many bytes and fixup the longword there.  */
    for(; fStemdosPexec;)
    {
      if(!FREAD(&b,1,1,fStemdosPexec))
        break;
      else if(b==0)
        break;
      else if(b==1)
        fixup_ad+=254;
      else
      {
        if(b&1) // odd
        {
          TRACE_LOG("%s %d\n","ERROR",b);
          fclose(fStemdosPexec);
          fStemdosPexec=NULL;
          StemdosFinish();
        }
        else
        {
          fixup_ad+=b;
          ad=SafeLPeek(fixup_ad+textbase); //relocate this address
          SafeLPoke(fixup_ad+textbase,ad+textbase);
          TRACE_LOG2("fixup $%X to $%X as $%X\n",ad,fixup_ad+textbase,ad+textbase);
        }
      }
    }
  }
#if defined(SSE_DEBUG_SYMBOLS)
  if(fStemdosPexec)
    Tos.GetSymbols(fStemdosPexec,basepage);
#endif
  if(fStemdosPexec)
    fclose(fStemdosPexec);
  fStemdosPexec=NULL;
  if(StemdosPexecMode==PE_LOADGO)
  {
    MEM_ADDRESS env=SafeLPeek(basepage+0x2C);
    StemdosAddPexec(basepage,env);
    // This makes sure Steem doesn't take the Pexec mode 4 or 6 call below
    // as a different program being run.
    StemdosIgnoreNextPexec=true;
    // Pexec call to just go, must write everything as the sp
    // might have been on the now cleared heap (Team STF demo)
    MEM_ADDRESS mySP=GetSPBeforeTrap();
    SafeDPoke(mySP,P_EXEC);
    // mode PE_GOTHENFREE, available since TOS 1.04, frees the memory 
    // afterwards so we don't need to do it ourselves with hacks
    StemdosPexecMode=(WORD)((tos_version>=0x104) ? PE_GOTHENFREE : PE_GO);
    SafeDPoke(mySP+2,StemdosPexecMode);
    SafeLPoke(mySP+8,basepage);
    StemdosFinish();
  }
  else
  {
    StemdosFinish();
    StemdosSkipGemdos();
  }
#ifdef VC_BUILD
Finished:
#endif
  TRACE_LOG("PEXEC: %d\n",D0);
  return D0;
}

#undef GOTO_FINISHED

bool StemdosPterm() {
  // return true if process is STEMDOS
  if(Stemdos.PexecListPtr) // there are processes
  {
    // close files: important because some programs forget to do it (Lattice C) 
    for(int n=GEMDOS_STD_HANDLES;n<MAX_STEMDOS_FILES;n++)
    {
      if(StemdosFile[n].open)
      {
        if(StemdosFile[n].Pexec==Stemdos.PexecListPtr)
        {
          TRACE_LOG("Pterm Close file %d %s\n",n,CHECKPATH(StemdosFile[n].filename.Text));
          StemdosCloseFile(&StemdosFile[n]);
        }
      }
    }
    Stemdos.PexecListPtr--;
    //TRACE_LOG("PExec ptr %d -> %d\n",Stemdos.PexecListPtr+1,Stemdos.PexecListPtr);
    if(Stemdos.PexecList[Stemdos.PexecListPtr])
    { //one of ours
      return true; // Only Mfree if load n go
    }
  }
  return false;
}


void StemdosAddPexec(MEM_ADDRESS const ad,MEM_ADDRESS const env) { 
  // if ad = 0, it's a GEMDOS process
  ASSERTB(Stemdos.PexecListPtr<MAX_STEMDOS_PEXEC_LIST);
  if(Stemdos.PexecListPtr>=MAX_STEMDOS_PEXEC_LIST)
  {
    for(int n=0;n<MAX_STEMDOS_PEXEC_LIST-1;n++)
    {
      Stemdos.PexecList[n]=Stemdos.PexecList[n+1];
      Stemdos.env[n]=Stemdos.env[n+1];
    }
    Stemdos.PexecListPtr--;
  }
  Stemdos.env[Stemdos.PexecListPtr]=env; // record environment address too
  Stemdos.PexecList[Stemdos.PexecListPtr++]=ad;
  //TRACE_LOG("PExec ptr %d -> %d\n",Stemdos.PexecListPtr-1,Stemdos.PexecListPtr);
}


void TStemdos::CtrlC() {
  //control-c pressed
  if(GemdosCommand==C_CONRS)
  { //readline
    if(StemdosPterm())
    {
      on_rte=ON_RTE_STEMDOS;
      on_rte_interrupt_depth=interrupt_depth; //get RTE from current interrupt
      StemdosRteAction=STEMDOS_RTE_MFREE;
    }
  }
}


bool TStemdos::IsMounted(BYTE const a) { //a can be 0,1 too
  if(a<GEMDOS_MAXDRIVES) 
    return Stemdos.DriveMounted[a];
  return false;
}


void StemdosParsePath() { 
  //remove \..\ etc.
#if defined(SSE_420R5)
  int c=0;
#else
  INT_PTR c=0;
#endif
  while(c<StemdosFilename.Length())
  {
    if(StemdosFilename.Mids(c,3)=="\\..")
    { //back
#if defined(SSE_420R5)
      int cc=c-1;
#else
      INT_PTR cc=c-1;
#endif
      while(cc>=0)
      {
        if(StemdosFilename[cc]=='\\')
        { //found previous folder
          StemdosFilename.Delete(cc,c+3-cc); //remove folder and \..
          c=cc;
          cc=-99; //stop looking back
        }
        cc--;
      }
      if(cc>-99)  //didn't find a previous folder
        StemdosFilename.Delete(c,3);
    }
    else if(StemdosFilename.Mids(c,2)=="\\.")
      //refresh!
      StemdosFilename.Delete(c,2); //remove \.
    else
      c++;
  }
  if(StemdosFilename[2]!='\\') 
    StemdosFilename.Insert("\\",2);
  char *i=strchr(StemdosFilename,' ');
  if(i) 
    *i='\0';  //truncate
  char *slash1,*slash2;
  slash2=StemdosFilename.Right()+1; //point to null-termination
  slash1=slash2;
  while(slash1>StemdosFilename.Text)
  {
    slash1--;
    if(*slash1=='\\')
    {  //slash!
      i=slash1+1;
      int letters=8; //8 letters for filename
      bool period=false;  //only one extension!
      while(i<slash2)
      {
        if(*i=='.')
        {
          if(period)
          { //second .
            while(slash2>i)
            { //excise to slash or null
              memmove(i,i+1,strlen(i));
              slash2--;
            }
            break;   //finished between these slashes
          }
          else
          { //first .
            period=true;
            letters=3; //3 letters for extension
            i++; //look at next letter
          }
        }
        if(letters==0)
        { //already gone past 8 letters
          while(*i!='.' && *i!='\\' && *i)
          { //remove extra characters
            memmove(i,i+1,strlen(i));
            slash2--;
          }
          letters=3;  //max 3 left for filename
          //i now points to next letter, probably .
        }
        else
        {
          letters--;
          i++; //count letter and move to next one
        }
      }
      slash2=slash1;  //look left of this slash now
    }
  }
  RemoveIllegalFromPath(StemdosFilename,true,false,'-',false);
}


int StemdosGetFilePath() {
  StrUpperNoSpecial(StemdosFilename);
#if 1 // 4 chars = dword
  DWORD asdword=*(DWORD*)(StemdosFilename.Text);
  DWORD asdwordcst[8]={*(DWORD*)"CON:",*(DWORD*)"AUX:",*(DWORD*)"VID:",*(DWORD*)"MID:",
                       *(DWORD*)"PRN:",*(DWORD*)"LST:",*(DWORD*)"IKB:",*(DWORD*)"STD:"};
  bool found=false;
  for(int i=0;i<8;i++)//gets developed
  {
    if(asdword==asdwordcst[i])
      found=true;
  }
  if(found)
    return STEMDOS_FILE_IS_GEMDOS;
#else
  if(StemdosFilename=="CON:" || StemdosFilename=="AUX:"
    || StemdosFilename=="VID:" || StemdosFilename=="MID:"
    || StemdosFilename=="PRN:" || StemdosFilename=="LST:"
    || StemdosFilename=="IKB:" || StemdosFilename=="STD:")
  {
    return STEMDOS_FILE_IS_GEMDOS;
  }
#endif
  else if(StemdosFilename.NotEmpty() && StemdosFilename[1]==':')
  {
    if(Stemdos.IsMounted(StemdosFilename[0]-'A'))
    {
      StemdosParsePath();
      return STEMDOS_FILE_IS_STEMDOS;
    }
    else
      return STEMDOS_FILE_IS_GEMDOS;
  }
  else
  { //not full path
    if(Stemdos.IsMounted(Stemdos.CurrentDrive))
    {
      if(StemdosFilename[0]!='\\')
      {
        StemdosFilename.Insert("\\",0); //make sure it begins with a slash
        StemdosFilename.Insert(Stemdos.CurrentPath[Stemdos.CurrentDrive],0); //put on current path
      }
      StemdosFilename.Insert(EasyStr(char('A'+Stemdos.CurrentDrive))+":",0); //and C:
      StemdosParsePath();  //remove slash..slash
      return STEMDOS_FILE_IS_STEMDOS;
    }
    else
      return STEMDOS_FILE_IS_GEMDOS;
  }
}


void TStemdos::CheckPaths() {
  if(CurrentDrive>DRIVE_B && !DriveMounted[CurrentDrive])
    CurrentDrive=DRIVE_A;
  for(int d=DRIVE_A;d<GEMDOS_MAXDRIVES;d++)
  {
    if(DriveMounted[d])
    {
      if(CurrentPath[d].NotEmpty())
      {
        DWORD Attrib=GetFileAttributes(MountPath[d]+CurrentPath[d]);
        if((Attrib&FILE_ATTRIBUTE_DIRECTORY)==0 || Attrib==INVALID_FILE_ATTRIBUTES)
          CurrentPath[d]="";
      }
    }
  }
}


void StemdosTrap1() {
  Cpu.ProcessingState=TMC68000::EXCEPTION;
  SET_PC(StemdosSaveAddress);
  m68k_interrupt(os_gemdos_vector);
  Cpu.ProcessingState=TMC68000::NORMAL;
}


void StemdosCallFdup() {
  // this is the trick used by Stemdos to get a handle for its files
  m68k_PUSH_W(GSH_PRN); // printer!
  m68k_PUSH_W(F_DUP);
  StemdosTrap1();
}


void StemdosCallFgetdta() { // for F_SFIRST
  m68k_PUSH_W(F_GETDTA);
  StemdosTrap1();
}


void StemdosCallFclose(WORD const h) {
  m68k_PUSH_W(h);
  m68k_PUSH_W(F_CLOSE);
  StemdosTrap1();
}


void StemdosCallPexec(MEM_ADDRESS const command,MEM_ADDRESS const env) {
  Cpu.ProcessingState=TMC68000::EXCEPTION;
  m68k_PUSH_L(env);
  m68k_PUSH_L(command);
  m68k_PUSH_L(0);
  m68k_PUSH_W(PE_BASEPAGE);
  m68k_PUSH_W(P_EXEC);
  m68k_interrupt(os_gemdos_vector); // want to return from this interrupt into GEMDOS
  Cpu.ProcessingState=TMC68000::NORMAL;
}


void StemdosCallMfree(MEM_ADDRESS ad) {
  TRACE_LOG("%06X Process %d Call M_FREE $%X\n",old_pc,Stemdos.PexecListPtr,ad);
  m68k_PUSH_L(ad);
  m68k_PUSH_W(M_FREE);
  Cpu.ProcessingState=TMC68000::EXCEPTION;
  m68k_interrupt(os_gemdos_vector);
  Cpu.ProcessingState=TMC68000::NORMAL;
}


// Misc 

char* StrUpperNoSpecial(char* const Str) {
  size_t Len=strlen(Str);
  for(size_t n=0;n<Len;n++)
  {
    if(Str[n]>32) 
      Str[n]=(char)toupper(Str[n]);
  }
  return Str;
}


WORD STfileReadWord(FILE* const fp) {
  WORD wrd;
  FREAD(&wrd,1,2,fp);
  SWAP_BIG_ENDIAN_WORD(wrd);
#ifdef BIG_ENDIAN_PROCESSOR
//  FREAD(&wrd,1,2,fp);
#else
//  FREAD((BYTE*)(&wrd)+1,1,1,fp); //high byte in file ->high byte in word
//  FREAD((BYTE*)(&wrd),1,1,fp); //low byte in file ->low byte in word
#endif
  return wrd;
}


LONG STfileReadLong(FILE* const fp) {
  LONG lng;
  if(!FREAD(&lng,1,4,fp))
    STfileReadError=true;
  SWAP_BIG_ENDIAN_DWORD(lng);
  return lng;
}

#undef D0
#endif//DISABLE_STEMDOS

void STfileReadToSTMemory(FILE* const fp,MEM_ADDRESS ad,int const nBytes) {
  // this is used by the Debugger and by hd_gemdos
  //ASSERT(ad+nBytes<mem_len);
  BYTE buf;
  for(int n=0;n<nBytes;n++)
  {
    FREAD(&buf,1,1,fp);
    SafePoke(ad++,buf);
  }
}


TStemdos::TStemdos() {
  BootDrive=CurrentDrive=DRIVE_C; //?
  ZeroMemory(PexecList,MAX_STEMDOS_PEXEC_LIST*sizeof(MEM_ADDRESS));
  ZeroMemory(env,MAX_STEMDOS_PEXEC_LIST*sizeof(MEM_ADDRESS));
  ZeroMemory(DriveMounted,GEMDOS_MAXDRIVES*sizeof(bool));
  bInterceptDateTime=false;
  Dta=0;
  PexecListPtr=0;
}


#if defined(SSE_ENABLE_TRACE_LOG) // only for TRACE
/*
typedef struct
{
unsigned hour:5;
unsigned minute:6;
unsigned second:5;
unsigned year:7;
unsigned month:4;
unsigned day:5;
} DATETIME;
second should be multiplied times two to obtain the actual value.
year is expressed as an offset from 1980.
Same as DOS
*/

bool TStemdos::ConvertDate(WORD date,int& y,int& m,int& d) {
  y=(date>>9)+1980; // bug in 2108
  m=(date>>5)&0xF;
  d=date&0x1F;
  return (d<367 && m<13);
}


bool TStemdos::ConvertTime(WORD time,int& h,int& m,int& s) {
  h=time>>11;
  m=(time>>5)&0x3F;
  s=(time&0x1F)*2;
  return (h<25 && m<60 && s<60);
}

#endif

#undef LOGSECTION

