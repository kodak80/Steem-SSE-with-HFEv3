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

FILE: debug.cpp
DESCRIPTION: General debugging facilities. This is meant to debug Steem
itself as well as ST programs. This is not the Debugger, see debugger.cpp etc.
for that.
TRACE function (file or IDE), OSD message.
Some debug facilities are used in the Regular Steem as well as in the
Debugger.
The beta builds can use TRACE_LOG, the sections to trace are defined here.
With the debug builds, sections to trace are commanded in the Debugger.
---------------------------------------------------------------------------*/

#include <pch.h> 
#pragma hdrstop

#include <debug.h>
#include <stdarg.h>
#include <gui.h>
#include <harddiskman.h>
#include <draw.h>
#include <display.h>
#include <computer.h>
#include <osd.h>
#include <infobox.h>
#include <diskman.h>
#include <key_table.h>
#include <stjoy.h>
#ifdef WIN32
#include <time.h>
#endif
#if defined(SSE_DEBUGGER_FRAME_REPORT)
#include <debug_framereport.h>
#endif
#ifdef DEBUG_BUILD
#include <iolist.h>
#include <debugger.h>
#endif

#if defined(SSE_DEBUG_TRACE_LOCK)
#define FILEHANDLE_BAD_VALUE INVALID_HANDLE_VALUE
#else
#define FILEHANDLE_BAD_VALUE NULL
#endif

#if defined(SSE_DEBUG)
int debug0,debug1,debug2,debug3,debug4,debug5,debug6,debug7,debug8,debug9;
#if defined(SSE_HD6301_LL)
extern "C" void (*hd6301_trace)(char *fmt,...);
#endif
#endif

TDebug Debug; // singleton, present in all builds
EasyStr sTraceFile;
bool debug_wipe_log_on_reset=false;

TDebug::TDebug() {
  ZeroMemory(this,sizeof(TDebug));
#if defined(SSE_ENABLE_TRACE_LOG)
  ZeroMemory(logsection_enabled,100*sizeof(bool));
  logsection_enabled[LOGSECTION_ALWAYS]=1;
  logsection_enabled[LOGSECTION_FDC]=0;
  logsection_enabled[LOGSECTION_IOW]=0;
  logsection_enabled[LOGSECTION_MFP_TIMERS]=0;
  logsection_enabled[LOGSECTION_INIT]=0;
  logsection_enabled[LOGSECTION_CRASH]=0;
  logsection_enabled[LOGSECTION_GEMDOS]=0;
  logsection_enabled[LOGSECTION_IKBD]=0;
  logsection_enabled[LOGSECTION_AGENDA]=0;
  logsection_enabled[LOGSECTION_INTERRUPTS]=0;
  logsection_enabled[LOGSECTION_TRAP]=0;
  logsection_enabled[LOGSECTION_SOUND]=0;
  logsection_enabled[LOGSECTION_GLUE]=0;
  logsection_enabled[LOGSECTION_BLITTER]=0;
  logsection_enabled[LOGSECTION_PORTS]=0;
  logsection_enabled[LOGSECTION_TRACE]=0;
  logsection_enabled[LOGSECTION_CPU]=0;
  logsection_enabled[LOGSECTION_VIDEO_RENDERING]=0;
  //logsection_enabled[LOGSECTION_OPTIONS]=0;
  logsection_enabled[LOGSECTION_IOR]=0;
  logsection_enabled[LOGSECTION_IMAGE_INFO]=0;
#if defined(SSE_BETA) && !defined(SSE_DEBUGGER)
  //logsection_enabled[LOGSECTION_FDC]=1;
  //logsection_enabled[LOGSECTION_IMAGE_INFO]=1;
  //logsection_enabled[LOGSECTION_VIDEO_RENDERING]=1;
#endif
#endif//#if defined(SSE_ENABLE_TRACE_LOG)
#if defined(SSE_DEBUG) && defined(SSE_HD6301_LL)
  hd6301_trace=&TDebug::TraceLog;
#endif
#if defined(SSE_DEBUG_TRACE) && defined(SSE_421R0)
  trace_file_pointer=FILEHANDLE_BAD_VALUE;
#endif
}


TDebug::~TDebug() {
#if defined(SSE_DEBUG_TRACE)
  Debug.TraceStartStop(false);
#endif
}


// v4.2.1 reduce macro overhead
// but each assert still produces a string 
void TDebug::Assert(bool test,char* test_as_string) {
  if(!test)
  {
    TRACE("%s: %s\n","ASSERT",test_as_string);
    if(!IgnoreErrors && !FullScreen)
    {
      int id=MessageBox(0,test_as_string,"ASSERT",MB_ICONWARNING|MB_ABORTRETRYIGNORE);
      if(id==IDABORT)
        exit(EXIT_FAILURE);
      IgnoreErrors=(id==IDIGNORE);
    }
  }
}


#define LOGSECTION LOGSECTION_GLUE

void TDebug::Vbl() { 
#if defined(SSE_DEBUGGER_FRAME_REPORT)
  FrameEvents.Vbl(); 
#endif
#if defined(SSE_DEBUG)
  if(ShifterTricks)
  {
#if defined(SSE_DEBUGGER_FAKE_IO)
    if(OSD_MASK2 & OSD_CONTROL_SHIFTERTRICKS)
      TRACE_OSD("T%X",ShifterTricks);
#endif
#if defined(SSE_OSD_FPS_INFO) // can also give VRAM size
    if(vcount_at_vsync-vbase_at_vbi>32000)
      TRACE_LOG("F%d tricks %x %d bytes\n",FRAME,ShifterTricks,vcount_at_vsync-vbase_at_vbi);
    else
#endif
    TRACE_LOG("F%d tricks %x\n",FRAME,ShifterTricks);
  }
#endif
  if(!border && (OPTION_VLE>=1) && (!ShifterTricks^noShifterTricks))
  {
    UPDATE_STATUS_BAR_PART(SB_PART_ICONS);
  }
  ShifterTricks=0;
#if defined(SSE_DEBUGGER_FAKE_IO)
/*  This system so that we only report these once per frame, giving
    convenient info about VBI, HBI, and MFP IRQ.
*/
  if((OSD_MASK1 & OSD_CONTROL_INTERRUPT)&&FrameInterrupts)
  {
    char buf1[40]="",buf2[4];
    if(FrameInterrupts&2)
      strcat(buf1,"V");
    if(FrameInterrupts&1)
      strcat(buf1,"H");
    if(FrameMfpIrqs)
    {
      strcat(buf1," MFP ");
      for(int i=15;i>=0;i--)
      {
        if(FrameMfpIrqs&(1<<i))
        {
          sprintf(buf2,"%d ",i);
          strcat(buf1,buf2);
        }
      }
    }
    TRACE_OSD(buf1);
  }
  FrameInterrupts=0;
  FrameMfpIrqs=0;
#endif  
#if defined(SSE_OSD_FPS_INFO)
/*  cases where it doesn't work
    Pacmania STE: VBASE ignored by game
    Pro Tennis Simulator intro: screen cleaned up after DE
*/
  if(OPTION_OSD_FPSINFO) // will the load kill our fps?
  {
    DWORD checksum=0;
    DWORD max=MIN(vcount_at_vsync,mem_len);
    for(DWORD x=vbase_at_vbi;x<max;x+=4)
      checksum+=LPEEK(x);
    if(checksum==frame_checksum)
      frame_no_change++;
    frame_checksum=checksum;
  }
#endif
  ASSERTB(!(sys_timer&1));
#ifdef SSE_DEBUG
  nHbis=0;
#endif
  Shifter.nVbl++;
#if defined(SSE_STATS)
  Stats.nFrame++;
#ifdef SSE_BETA
  //TRACE_OSD("%d",A_S_T); // check timer wrap
//for tests only! - or future teaser free version until you pay big $ to register?? ;)
#if 0
  if(Stats.nFrame==30*60 && Cpu.ProcessingState!=TMC68000::HALTED)
  {
    reset_st(RESET_COLD|RESET_NOSTOP|RESET_CHANGESETTINGS|RESET_BACKUP);
  }
#endif
#endif
#endif//#if defined(SSE_STATS)
#if defined(SSE_DEBUGGER_FRAME_REPORT)
  if(FRAME_REPORT_MASK1 & FRAME_REPORT_MASK_VC_LINES)
    FrameEvents.Add(scan_y,0,"VB",vbase);
  if(FRAME_REPORT_MASK1 & FRAME_REPORT_MASK_SHIFTMODE)
    FrameEvents.Add(scan_y,0,"R=",Shifter.ShiftMode);
  if(FRAME_REPORT_MASK1 & FRAME_REPORT_MASK_SYNCMODE)
    FrameEvents.Add(scan_y,0,"S=",Glue.SyncMode);
#endif
}


#if defined(SSE_DEBUG_TRACE)

void TDebug::TraceStartStop(BYTE bStart/*=true*/) {
  // close trace file if open
  if(trace_file_pointer!=FILEHANDLE_BAD_VALUE)
  {
    Trace("End\n");
#if defined(SSE_DEBUG_TRACE_LOCK)
    CloseHandle(trace_file_pointer);
#else
    fclose(trace_file_pointer);
#endif
    trace_file_pointer=FILEHANDLE_BAD_VALUE;
  }
  // open trace file if required
  if(bStart)
  {
    EasyStr trace_file=SSE_TRACE_FILE_NAME;
    // file could be already open, in that case we create TRACE(1).txt or ... instead
    for(int i=1;trace_file_pointer==FILEHANDLE_BAD_VALUE && i<100;i++)
    {
      EasyStr trace_path=UsersPath+SLASH+trace_file;
#if defined(SSE_DEBUG_TRACE_LOCK)
      // fopen doesn't check or lock the file, must use OS
      trace_file_pointer=CreateFile(trace_path.Text,GENERIC_WRITE,FILE_SHARE_READ,
                                    NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
#else
      trace_file_pointer=fopen(trace_path,"w");
#endif
      if(trace_file_pointer==FILEHANDLE_BAD_VALUE)
      {
        GetNewFileName(trace_file); // new function!
#if defined(SSE_STATS)
        GetNewFileName(sStatsFile); // shrewd!
#endif
      }
      else
        sTraceFile=trace_file;
    }//nxt
    TraceGeneralInfos(INIT0);
  }
}

#endif//#if defined(SSE_DEBUG_TRACE)

void TDebug::Reset(bool Cold) {
  TRACE_SECTION(LOGSECTION_INIT,PRICV " %s %s\n",A_S_T,(Cold?"Cold":"Warm"),"Reset");
  if(Cold)
  {
    IgnoreErrors=0;
#if defined(SSE_OSD_SHOW_TIME)
    OsdControl.StartingTime=timeGetTime();
    OsdControl.StoppingTime=0;
#endif
  }
  else if(runstate==RUNSTATE_RUNNING)
  {
#if defined(SSE_OSD_DEBUGINFO)
    OsdControl.Trace(TOsdControl::OUTPUT_SB,"Reset");
#endif
  }
  ShifterTricks=0;
}


#if defined(SSE_DEBUG_TRACE)

void TDebug::DoTrace(char *fmt,va_list args) {
#if defined(SSE_UNIX)
  int nchars=vsnprintf(trace_buffer,MAX_TRACE_CHARS,fmt,args); // check for overrun 
#endif
#ifdef WIN32
#if !defined(SSE_DEBUG)
  _vsnprintf(trace_buffer,MAX_TRACE_CHARS,fmt,args);
#else
  int nchars=_vsnprintf(trace_buffer,MAX_TRACE_CHARS,fmt,args); // check for overrun 
#endif
#endif//WIN32
  va_end(args);	
#ifdef SSE_DEBUG
  if(nchars==-1)
    strcpy(trace_buffer,"TRACE buffer overrun\n");
#endif
#if defined(SSE_DEBUG_TRACE_IDE)
  OutputDebugString(trace_buffer); // there's no way to do that with Linux/CodeLite?
#endif
#if defined(SSE_UNIX_TRACE)
  if(!SSEConfig.TraceFile)  
    fprintf(stderr,trace_buffer); // goes to Terminal windo (if open)
#endif 
  if(trace_file_pointer && trace_buffer)
  {
#if defined(SSE_DEBUG_TRACE_LOCK)
    // our traces with only \n should be translated to Windows format
    // not very good for performance
    // we can't put \r\n everywhere because of the Linux build
    char neo_buffer[1024];
    DWORD n=0;
#if defined(SSE_420R5) // opt
      char *src=trace_buffer;
      while(*src)
      {
        if(*src=='\n')
          neo_buffer[n++]='\r';
        neo_buffer[n++]=*src++;
      }
#else
    for(int i=0;trace_buffer[i]!='\0';i++)
    {
      if(trace_buffer[i]=='\n')
        neo_buffer[n++]='\r';
      neo_buffer[n++]=trace_buffer[i];
    }
#endif
    WriteFile(trace_file_pointer,neo_buffer,n,&n,NULL);
#else
    fprintf(trace_file_pointer,"%s",trace_buffer);
#endif//#if defined(SSE_DEBUG_TRACE_LOCK)
#if defined(SSE_DEBUGGER)
    nTrace++;
    if(TRACE_FILE_REWIND && nTrace>=TRACE_MAX_WRITES)
    {
      nTrace=0;
#if defined(SSE_DEBUG_TRACE_LOCK)
      SetFilePointer(trace_file_pointer,0,NULL,FILE_BEGIN);
#else
      rewind(trace_file_pointer); // it doesn't erase
#endif
      TRACE("\n============\nREWIND TRACE\n============\n");
    }
#endif//#if defined(SSE_DEBUGGER)
  }
#if defined(SSE_LIBRETRO)//temp
  //FlushTrace();
#endif

}


void TDebug::Trace(char *fmt,...) {
  // Our TRACE facility has no MFC dependency.
  va_list body;	
  va_start(body, fmt);

#ifdef SSE_421R0
  DoTrace(fmt,body);
#else//#ifdef SSE_421R0
#if defined(SSE_UNIX)
  int nchars=vsnprintf(trace_buffer,MAX_TRACE_CHARS,fmt,body); // check for overrun 
#endif
#ifdef WIN32
#if !defined(SSE_DEBUG)
  _vsnprintf(trace_buffer,MAX_TRACE_CHARS,fmt,body);
#else
  int nchars=_vsnprintf(trace_buffer,MAX_TRACE_CHARS,fmt,body); // check for overrun 
#endif
#endif//WIN32
  va_end(body);	
#ifdef SSE_DEBUG
  if(nchars==-1)
    strcpy(trace_buffer,"TRACE buffer overrun\n");
#endif
#if defined(SSE_DEBUG_TRACE_IDE)
  OutputDebugString(trace_buffer); // there's no way to do that with Linux/CodeLite?
#endif
#if defined(SSE_UNIX_TRACE)
  if(!SSEConfig.TraceFile)  
    fprintf(stderr,trace_buffer); // goes to Terminal windo (if open)
#endif 
  if(trace_file_pointer && trace_buffer)
  {
#if defined(SSE_DEBUG_TRACE_LOCK)
    // our traces with only \n should be translated to Windows format
    // not very good for performance
    // we can't put \r\n everywhere because of the Linux build
    char neo_buffer[1024];
    DWORD n=0;
#if defined(SSE_420R5) // opt
      char *src=trace_buffer;
      while(*src)
      {
        if(*src=='\n')
          neo_buffer[n++]='\r';
        neo_buffer[n++]=*src++;
      }
#else
    for(int i=0;trace_buffer[i]!='\0';i++)
    {
      if(trace_buffer[i]=='\n')
        neo_buffer[n++]='\r';
      neo_buffer[n++]=trace_buffer[i];
    }
#endif
    WriteFile(trace_file_pointer,neo_buffer,n,&n,NULL);
#else
    fprintf(trace_file_pointer,"%s",trace_buffer);
#endif//#if defined(SSE_DEBUG_TRACE_LOCK)
#if defined(SSE_DEBUGGER)
    nTrace++;
    if(TRACE_FILE_REWIND && nTrace>=TRACE_MAX_WRITES)
    {
      nTrace=0;
#if defined(SSE_DEBUG_TRACE_LOCK)
      SetFilePointer(trace_file_pointer,0,NULL,FILE_BEGIN);
#else
      rewind(trace_file_pointer); // it doesn't erase
#endif
      TRACE("\n============\nREWIND TRACE\n============\n");
    }
#endif//#if defined(SSE_DEBUGGER)
  }
#if defined(SSE_LIBRETRO)//temp
  FlushTrace();
#endif
#endif//#ifdef SSE_421R0
}


#ifdef WIN32

//https://stackoverflow.com/questions/36543301/detecting-windows-10-version/36545162#36545162

//#ifdef BCC_BUILD
#ifndef VC_BUILD
typedef LONG NTSTATUS;
typedef LONG* PNTSTATUS;
typedef OSVERSIONINFOW RTL_OSVERSIONINFOW;
typedef OSVERSIONINFOW *PRTL_OSVERSIONINFOW;
#endif
#define STATUS_SUCCESS (0x00000000)

typedef NTSTATUS (WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

RTL_OSVERSIONINFOW GetRealOSVersion() {
    HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)::GetProcAddress(hMod, "RtlGetVersion");
        if (fxPtr != NULL) {
            RTL_OSVERSIONINFOW rovi = { 0 };
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if ( STATUS_SUCCESS == fxPtr(&rovi) ) {
                return rovi;
            }
        }
    }
    RTL_OSVERSIONINFOW rovi = { 0 };
    return rovi;
}


//https://docs.microsoft.com/en-us/windows/win32/api/wow64apiset/nf-wow64apiset-iswow64process?redirectedfrom=MSDN
typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

LPFN_ISWOW64PROCESS fnIsWow64Process=NULL;

BOOL IsWow64()
{
    BOOL bIsWow64 = FALSE;

    //IsWow64Process is not available on all supported versions of Windows.
    //Use GetModuleHandle to get a handle to the DLL that contains the function
    //and GetProcAddress to get a pointer to the function if available.
    HMODULE hm=GetModuleHandle(TEXT("kernel32"));
    if(hm)
      fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress(hm,"IsWow64Process");

    if(NULL != fnIsWow64Process)
    {
        if (!fnIsWow64Process(GetCurrentProcess(),&bIsWow64))
        {
            //handle error
        }
    }
    return bIsWow64;
}

#endif//WIN32

// A series of TRACE giving precious info at various times

void TDebug::TraceGeneralInfos(int when) {

  // get run date and time in strings
  char sdate[9],stime[9];
#ifdef WIN32
  _strdate( sdate );
  _strtime( stime );
#endif
#ifdef UNIX
  time_t rawtime;
  time (&rawtime);
  tm *timeinfo=localtime(&rawtime);
  strftime(sdate,9,"%D",timeinfo);
  strftime(stime,9,"%T",timeinfo);
#endif

  switch(when) {

  case INIT0: // opening of trace
    TRACE("%s %s %s\n%s v%s (built %s) \n",SSE_TRACE_FILE_NAME,sdate,stime,gAppName,
           gAppBuildInfo,stem_version_date_text);
#ifdef BCC_BUILD
    TRACE("BCC");
#endif
#ifdef VC_BUILD
    TRACE("VC%d",_MSC_VER);
#endif
#ifdef MINGW_BUILD
    TRACE("MinGW");
#endif
#ifdef UNIX
    TRACE("GCC");
#endif
#ifdef SSE_VID_DD
    TRACE(" DD%x",DIRECTDRAW_VERSION>>8);
#endif
#ifdef SSE_VID_D3D
    TRACE(" %s%x","D3D",DIRECT3D_VERSION>>8);
#endif
#ifdef SSE_DRAW_C
    //TRACE(" DrawC"); // should be all now
#endif
#ifdef SSE_LIBRETRO
    TRACE(" DLL");
#endif
    TRACE("\n");
#ifdef WIN32
  {
    RTL_OSVERSIONINFOW rovi=GetRealOSVersion();
    SSEConfig.WindowsVersion=rovi.dwMajorVersion|(rovi.dwMinorVersion<<8)|(rovi.dwBuildNumber<<24);
#ifdef SSE_X64
    int bits=64; // IsWow64() is FALSE!
#else
    int bits=(32<<(int)!!IsWow64());
#endif
    TRACE("Windows v%d.%d.%d %dbit %s\n",rovi.dwMajorVersion,rovi.
           dwMinorVersion,rovi.dwBuildNumber,bits,rovi.szCSDVersion);
    //TRACE("%s $%X\n","thread",GetCurrentThreadId());
  }
#endif//WIN32
    break;

  case INIT: // when init done (but options not retrieved)
    break;

  case RESET:
    TRACE("%s ~%dHz %dK T%X(%d) %s C%d C%d",st_model_name[ST_MODEL],nSysCyclesPerSecond/TICKS8,
           mem_len>>10,tos_version,SSEConfig.TosLanguage,screen_type[SSEConfig.ColourMonitor],
           OPTION_C1,(OPTION_VLE?OPTION_VLE+1:0));
    if(IS_STF)
    {
      if(OPTION_HWOVERSCAN)
        TRACE(" %s\n",overscan_dev[OPTION_HWOVERSCAN]);
      else
        TRACE(" WU%d\n",Mmu.WS[OPTION_WS]);
    }
    else
      TRACE("\n");
    break;

  case START:
#if defined(SSE_OSD_SHOW_TIME)
    if(OsdControl.StoppingTime)
      OsdControl.StartingTime+=timeGetTime()-OsdControl.StoppingTime;
#endif
#if defined(SSE_EMU_THREAD)
    if(OPTION_EMUTHREAD)
      TRACE("\n%s Run %s $%X\n",stime,"thread",EmuThreadId);
    else
#endif
      TRACE("\n%s Run\n",stime);
    break;

  case STOP:
#if defined(SSE_OSD_SHOW_TIME)
    OsdControl.StoppingTime=timeGetTime();
#endif
    TRACE("%s Stop\n",stime);
    break;

  case EXIT:
    TRACE("%s Leaving Steem\n",stime);
    break;

  }//sw
   FlushTrace();
}


#ifdef VC_BUILD
__declspec(noinline) 
#endif
void TDebug::FlushTrace() {
  if(trace_file_pointer)
  {
    FFLUSH(trace_file_pointer);
    if(!(OPTION_EMUTHREAD&&runstate!=RUNSTATE_STOPPED) && InfoBox.IsVisible())
    {
      if(InfoBox.Page==TGeneralInfo::TRACEFILE)
      {
        //InfoBox.CreatePage(InfoBox.Page); // doesn't work, don't know why
        InfoBox.Hide();
        InfoBox.Show(); // radical
      }
    }
  }
}


// input: a path. output: same path or just the file according to option
// it's because some personal data, such as a name, can be in the path
// and you may want not to have that in a trace file you share
char* TDebug::TraceCheckPrivacy(char *string) {
  return (SSEConfig.TraceShowPath) ? string : GetFileNameFromPath(string);
}


#if defined(SSE_ENABLE_TRACE_LOG)

void TDebug::TraceLog(char *fmt,...) { // static for 6301
  if(logsection_enabled[Debug.LogSection])
  {
    va_list body;	
    va_start(body, fmt);

#ifdef SSE_421R0

    Debug.DoTrace(fmt,body);

#else//#ifdef SSE_421R0
#if defined(SSE_UNIX)
    int nchars=vsnprintf(Debug.trace_buffer,MAX_TRACE_CHARS,fmt,body); // check for overrun 
#else
    int nchars=_vsnprintf(Debug.trace_buffer,MAX_TRACE_CHARS,fmt,body); // check for overrun 
#endif
    va_end(body);	
    if(nchars==-1)
      strcpy(Debug.trace_buffer,"TRACE buffer overrun\n");
#ifdef WIN32
#if defined(SSE_DEBUG_TRACE_IDE)
    if(Debug.LogSection!=LOGSECTION_CPU) //there are limits!
      OutputDebugString(Debug.trace_buffer);
#endif
#endif//WIN32
#if defined(SSE_UNIX_TRACE)
    if(!SSEConfig.TraceFile)
      fprintf(stderr,Debug.trace_buffer);
#endif 
    if(Debug.trace_file_pointer && Debug.trace_buffer)
    {
#if defined(SSE_DEBUG_TRACE_LOCK)
      // our traces with only \n should be translated to Windows format
      // not very good for performance
      // we can't put \r\n everywhere because of the Linux build
      char neo_buffer[1024];
      DWORD n=0;
#if defined(SSE_420R5) // opt
      char *src=Debug.trace_buffer;
      while(*src)
      {
        if(*src=='\n')
          neo_buffer[n++]='\r';
        neo_buffer[n++]=*src++;
      }
#else
      int l=(int)strlen(Debug.trace_buffer);
      for(int i=0;i<l;i++)
      {
        if(Debug.trace_buffer[i]=='\n')
          neo_buffer[n++]='\r';
        neo_buffer[n++]=Debug.trace_buffer[i];
      }
#endif
      WriteFile(Debug.trace_file_pointer,neo_buffer,n,&n,NULL);
#ifdef TEST02
      //FLUSH_TRACE; // when it's crashy
#endif
#else
      fprintf(Debug.trace_file_pointer,"%s",Debug.trace_buffer);
#endif//#if defined(SSE_DEBUG_TRACE_LOCK)
    }
#if defined(SSE_DEBUGGER)
    Debug.nTrace++;
    if(TRACE_FILE_REWIND && Debug.nTrace>=TRACE_MAX_WRITES && Debug.trace_file_pointer)
    {
      Debug.nTrace=0;
#if defined(SSE_DEBUG_TRACE_LOCK)
      SetFilePointer(Debug.trace_file_pointer, 0, NULL, FILE_BEGIN);
#else
      rewind(Debug.trace_file_pointer); // it doesn't erase
#endif
      TRACE("\n============\nREWIND TRACE\n============\n");
      Debug.TraceGeneralInfos(INIT);
    }
#endif//#if defined(SSE_DEBUGGER)
#endif//#ifdef SSE_421R0
  }
}


void TDebug::TraceLog(BYTE section,char* fmt,...) { // overload, instead of LOGSECTION
  if(logsection_enabled[section])
  {
    va_list body;
    va_start(body,fmt);
    Debug.DoTrace(fmt,body);
  }
}

#endif//#if defined(SSE_ENABLE_TRACE_LOG)

#endif//#if defined(SSE_DEBUG_TRACE)


#if defined(SSE_DEBUGGER_TRACE_EVENTS)
  // not very smart...
void TDebug::TraceEvent(char *where,void* pointer) {
  TRACE("%s " PRICV " ",where,A_S_T);
  if(pointer==event_scanline)
    TRACE("event_scanline");
  else if(pointer==event_timer_a_timeout)
    TRACE("event_timer_a_timeout");
  else if(pointer==event_timer_b_timeout)
    TRACE("event_timer_b_timeout");
  else if(pointer==event_timer_c_timeout)
    TRACE("event_timer_c_timeout");
  else if(pointer==event_timer_d_timeout)
    TRACE("event_timer_d_timeout");
  else if(pointer==event_timer_b)
    TRACE("event_timer_b");
  else if(pointer==event_start_vbl)
    TRACE("event_start_vbl");
  else if(pointer==event_vbl_interrupt)
    TRACE("event_vbl_interrupt");
  else if(pointer==event_trigger_vbi)
    TRACE("event_trigger_vbi");
  else if(pointer==event_wd1772)
    TRACE("event_wd1772");
  else if(pointer==event_driveA_ip)
    TRACE("event_driveA_ip");
  else if(pointer==event_driveB_ip)
    TRACE("event_driveB_ip");
  else if(pointer==event_acia)
    TRACE("event_acia");
#if USE_PASTI
  else if(pointer==event_pasti_update)
    TRACE("event_pasti_update");
#endif
    TRACE(" (" PRICV ")\n",A_S_T-time_of_next_event);
}

#endif


#if defined(SSE_ENABLE_TRACE_LOG)
//////////////////////////////// names of OS calls //////////////////////////////////
const char* gemdos_calls[0x58]={"Pterm0","Conin","Conout(c=&)","Cauxin","Cauxout (c=&)",
  "Cprnout(c=&)","Raw con io Crawio(c=&)","Crawcin","Cnecic","Print line(text=%)",
  "ReadLine(buf=%)","Constat","","","SetDrv(drv=&)","","Conout stat","Printer status",
  "inp?(serial)","out?(serial)","","","","","","GetDrv","SetDTA(buf=%)","","","","","",
  "super(%)","","","","","","","","","","GetDate","SetDate(date=&)","Gettime",
  "Settime(time=&)","","GetDTA","Get version number","PtermRes(keepcnt=%,retcode=&)",
  "","","","","Dfree(buf=%,drive=&)","","","Mkdir(path=%=$)","Rmdir(path=%=$)",
  "Chdir(path=%)","Fcreate(fname=%=$,attr=&)","Fopen(fname=%=$,mode=&)","Fclose(handle=&)",
  "Fread(handle=&,count=%,buf=%)","Fwrite(handle=&,count=%,buf=%)","Fdelete(fname=%=$)",
  "Fseek(offset=%,handle=&,seekmode=&)","Fattrib(fname=%=$,flag=&,attrib=&)",
#if SSE_VERSION>=420
  "Mxalloc(amount=%, mode=&)",
#else
  "",
#endif
  "Fdup(handle=&)","Fforce(stdh=&,nonstdh=&)","DgetPath(buf=%,drive=&)",
  "Malloc(%)","Mfree(addr=%)","Mshrink(dummy=&,block=%,newsize=%)","Pexec(mode=&,%=$,%,%)",
  "Pterm(retcode=&)","","Fsfirst(fnam=%=$,attr=&)","Fsnext","","","","","","",
  "Frename(dummy=&,oldname=%=$,newname=%=$)","Fdatime(timeptr=%,handle=&,flag=&)"};

const char* bios_calls[12]={"GetMBP(pointer=%)","Bconstat(dev=&)","Bconin(dev=&)",
  //"Bconout(dev=&,c=#)","Rwabs(rwflag=&,buffer=%,number=&,recno=&,dev=&), read/write disk sector",
  "Bconout(dev=&,c=#)","Rwabs(mode=&,buffer=%,count=&,recno=&,dev=&), read/write disk sector",
  "Setexec(number=&,vector=%), set exception vector","Tickcal","Getbpb(dev=&)","Bcostat(dev=&)",
  "Mediach(dev=&)","Drvmap","Kbshift(mode=&)"};

const char* xbios_calls[40]={"InitMouse(type=&,parameter=%,vector=%)","Ssbrk(number=%)",
  "Physbase","Logbase","Getrez","void setscreen(log=%,phys=%,res=&)","Setpalette(ptr=%)",
  "Setcolor(colornum=&,color=&)","Floprd(buffer=%,filler=%,dev=&,sector=&,track=&,side=&,count=&)",
  "Flopwr(buffer=%,filler=%,dev=&,sector=&,track=&,side=&,count=&)",
  "Flopfmt(buffer=%,filler=%,dev=&,spt=&,track=&,side=&,interleave=&,magic=&,virgin=&)","",
  "Midiws(count=&,ptr=%)","Mfpint(number=&,vector=%)","Iorec(dev=&)",
  "Rsconf(baud=&,ctrl=&,ucr=&,rsr=&,tsr=&,scr=&)","Keytbl(unshift=%,shift=%,caps=%)",
  "random","protobt(buffer=%,serialno=%,disktype=&,execflag=&)",
  "Flopver(buffer=%,filler=%,dev=&,sector=&,track=&,side=&,count=&)","Scrdmp",
  "Cursconf(function=&,rate=&)","Settime(time=%)","Gettime","Bioskeys","Ikbdws(number=&,ptr=%)",
  "Jdisint(number=&)","Jenabint(number=&)","Giaccess(data=&,register=&)","Offgibit(bitnumber=&)",
  "Ongibit(bitnumber=&)","Xbtimer(timer=&,control=&,data=&,vector=%)","Dosound(pointer=%)",
  "Setprt(config=&)","Kbdvbase","Kbrate(delay=&,repeat=&)","Prtblk(parameter=%)",
  "Vsync","Supexec(%)","Puntaes"};

#endif


#ifdef DEBUG_BUILD

void stop_cpu_log() {
  logsection_enabled[LOGSECTION_CPU]=false;
  CheckMenuItem(logsection_menu,300+LOGSECTION_CPU, MF_BYCOMMAND|MF_UNCHECKED);
  TRACE("CPU LOG OFF\n");
}

#endif


#if defined(SSE_ENABLE_TRACE_LOG)

bool logsection_enabled[100];

#endif//#if defined(SSE_ENABLE_TRACE_LOG)


#if defined(DEBUG_BUILD)

char d2_t_buf[200]; //yep, 200 bytes for int to (hexa)decimal string conversion

#endif

#if defined(DEBUG_BUILD) || defined(SSE_LOGSECTIONS_FOR_ALL)

struct_logsection logsections[NUM_LOGSECTIONS+8]={
  {"Always",LOGSECTION_ALWAYS},
//  {"-",-1},
  {"Device IOW",LOGSECTION_IOW},
  {"Device IOR",LOGSECTION_IOR},
  {"-",-1},
  {"CPU",LOGSECTION_CPU},
  {"Trace",LOGSECTION_TRACE},
  {"Crash",LOGSECTION_CRASH},
  {"Interrupts",LOGSECTION_INTERRUPTS},
  {"Trap",LOGSECTION_TRAP},
  {"GLU",LOGSECTION_GLUE}, // formerly VIDEO
  {"MMU",LOGSECTION_MMU},
  {"Blitter",LOGSECTION_BLITTER},
  {"MFP",LOGSECTION_MFP_TIMERS},
  {"ACIA",LOGSECTION_ACIA},
  {"IKBD",LOGSECTION_IKBD},
  {"Cartridge",LOGSECTION_CARTRIDGE},
  {"FDC",LOGSECTION_FDC},
  {"ACSI",LOGSECTION_ACSI},
  {"DMA",LOGSECTION_DMA},
  {"Ports",LOGSECTION_PORTS},
  {"-",-1},
  //{"Image info",LOGSECTION_IMAGE_INFO},
  {"Disk image",LOGSECTION_IMAGE_INFO},
  {"Pasti",LOGSECTION_PASTI},
  //{"Hard drive",LOGSECTION_HARDDRIVE}, // ACSI + GEMDOS
  {"Gemdos",LOGSECTION_GEMDOS},
  {"Bios",LOGSECTION_BIOS},  {"Xbios",LOGSECTION_XBIOS},{"GEM",LOGSECTION_GEM},
  {"Init",LOGSECTION_INIT},//first of log2
  //{"Options",LOGSECTION_OPTIONS},
  {"Tasks",LOGSECTION_AGENDA},
  {"Video rendering",LOGSECTION_VIDEO_RENDERING},
  {"Sound",LOGSECTION_SOUND},
  {"*",-1}};

void logfile_wipe() {
  if(Debug.trace_file_pointer)
  {
#if defined(SSE_DEBUG_TRACE_LOCK)
    SetFilePointer(Debug.trace_file_pointer,0,0,FILE_BEGIN);
    SetEndOfFile(Debug.trace_file_pointer);
#else
    fclose(Debug.trace_file_pointer);
    EasyStr trace_file=UsersPath+SLASH+SSE_TRACE_FILE_NAME; 
    Debug.trace_file_pointer=freopen(trace_file, "w", stdout );
#endif
    Debug.TraceGeneralInfos(TDebug::INIT);
  }
}

#endif//#if defined(DEBUG_BUILD) || defined(SSE_LOGSECTIONS_FOR_ALL)

#if defined(DEBUG_BUILD)

MEM_ADDRESS debug_mem_write_log_address;
int debug_mem_write_log_bytes;

#endif//#if defined(DEBUG_BUILD)

MEM_ADDRESS aes_intout,vdi_intout,vdi_ptsout=0;

#undef FILEHANDLE_BAD_VALUE
