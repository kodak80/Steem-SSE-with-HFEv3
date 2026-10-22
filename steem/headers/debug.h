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

DOMAIN: Debug
FILE: debug.h
DESCRIPTION: Declarations for debug facilities, like ASSERT, TRACE...

ASSERT         all builds
ASSERTB        beta builds (SSE_BETA)
TRACE          all builds
TRACEB         beta builds
TRACE_LOG      beta builds and debug builds
TRACE_SECTION  beta builds and debug builds


The Debug object and some debug facilities are used in all builds (release
too).
struct TDebug
This file is included by 6301.c.
---------------------------------------------------------------------------*/

#pragma once
#ifndef SSEDEBUG_H
#define SSEDEBUG_H


#include "SSE.h"
#include "parameters.h"
#include <stdio.h>
#if defined(__cplusplus)
#ifdef WIN32
#include <windows.h>
#endif
#include "conditions.h"
#include "steemh.h" // LINECYCLES etc.
#include <easystr.h>
#endif//c++

#ifdef UNIX
//#include <assert.h>
#ifdef __cplusplus
#include <gui.h> //because of ASSERT macro
#endif
#endif

#ifdef MINGW_BUILD
#undef NULL
#define NULL 0
#endif

#if defined(SSE_DEBUG) // Debugger build or ide debug build
// general use debug variables;
extern 
#ifdef __cplusplus
"C" 
#endif//c++
int debug0,debug1,debug2,debug3,debug4,debug5,debug6,debug7,debug8,debug9;
#endif

// a structure that may be used by C++ and C objects
#pragma pack(push, 8)

struct TDebug {
#ifdef __cplusplus 
  // ENUM 
  enum EDebug {INIT0,INIT,LOAD,RESET,START,STOP,EXIT};
#endif
  // FUNCTIONS
#ifdef __cplusplus 
  TDebug();
  ~TDebug();
  void Assert(bool test,char *test_as_string);
#if defined(SSE_DEBUG_TRACE)
  void FlushTrace();
  void TraceStartStop(BYTE bStart=TRUE);
  void Trace(char *fmt,...); // one function for both IDE & file
  // if logsection enabled, static for function pointer, used in '6301':
  static void TraceLog(char *fmt,...);
  void DoTrace(char *fmt,va_list args);
  void TraceLog(BYTE section,char *fmt,...); // overload, instead of LOGSECTION
  char* TraceCheckPrivacy(char *string);
  void TraceGeneralInfos(int when);
#define CHECKPATH(x) Debug.TraceCheckPrivacy(x)
#endif//#if defined(SSE_DEBUG_TRACE)
  void Vbl();
  void Reset(bool Cold);
#if defined(SSE_DEBUGGER_TRACE_EVENTS)
  void TraceEvent(char *where,void* pointer);
#endif
#endif//C++
  // DATA
#if defined(SSE_DEBUG_TRACE)
  char trace_buffer[MAX_TRACE_CHARS];
#if defined(SSE_DEBUG_TRACE_LOCK)
  HANDLE trace_file_pointer;
#else
  FILE *trace_file_pointer;
#endif
#endif//#if defined(SSE_DEBUG_TRACE)
  int ShifterTricks; // used by Stats too
#if defined(SSE_OSD_FPS_INFO)
  DWORD frame_checksum;
  DWORD vbase_at_vbi,vcount_at_vsync;
  BYTE frame_no_change;
#endif
#if defined(SSE_ENABLE_TRACE_LOG)
  int LogSection;
  WORD nHbis; // counter for each frame
#endif
#if defined(SSE_DEBUGGER)
  int nTrace;
  HWND dbg_timer_hwnd[4]; //to record WIN handles
  WORD FrameMfpIrqs;
  BYTE FrameInterrupts; //bit0 VBI 1 HBI 2 MFP
  BYTE DialogOnStopEvent;
  WORD MonitorValue;
  BYTE MonitorValueSpecified; // Debugger SSE option
  BYTE MonitorComparison; // as is, none found = 0 means no value to look for
  BYTE MonitorRange; //check from ad1 to ad2
#endif
  BYTE IgnoreErrors; // Ignore in ASSERT and VERIFY
#if defined(SSE_DEBUGGER_FAKE_IO)
/*  Hack. A free zone in IO is mapped to an array of masks to control 
    a lot of debug options using the Debugger's built-in features. */
  WORD ControlMask[FAKE_IO_LENGTH];
#endif
#if defined(SSE_HD6301_LL)
  BYTE HD6301RamBuffer[256+8];
#endif
  BYTE noShifterTricks;
};

extern 
#ifdef __cplusplus
"C" 
#endif
struct TDebug Debug;

#pragma pack(pop)

#ifdef __cplusplus
extern EasyStr sTraceFile;
extern MEM_ADDRESS aes_intout,vdi_intout,vdi_ptsout;
#if defined(DEBUG_BUILD)
// to place Steem Debugger breakpoints in the code
void debug_set_bk(DWORD ad,bool set); //bool set
#define BREAK(ad) debug_set_bk(ad,true)
#else
#define BREAK(ad)
#endif
#endif//#ifdef __cplusplus

#if defined(SSE_ENABLE_TRACE_LOG)
enum logsection_enum_tag {
 LOGSECTION_ALWAYS,
 LOGSECTION_FDC,
 LOGSECTION_DMA,
 LOGSECTION_PASTI,
 LOGSECTION_IMAGE_INFO,
 LOGSECTION_IOW,
 LOGSECTION_IOR,
 LOGSECTION_INTERRUPTS,
 LOGSECTION_TRAP,
 LOGSECTION_MMU,
 LOGSECTION_MFP_TIMERS,
 LOGSECTION_MFP=LOGSECTION_MFP_TIMERS,
 LOGSECTION_CRASH,
 //LOGSECTION_HARDDRIVE,
 LOGSECTION_GEMDOS,
 LOGSECTION_BIOS,LOGSECTION_XBIOS,LOGSECTION_GEM, // v421
 LOGSECTION_ACIA,
 LOGSECTION_IKBD,
 LOGSECTION_PORTS,
 LOGSECTION_GLUE,
 LOGSECTION_BLITTER,
 LOGSECTION_TRACE,
 LOGSECTION_CPU,
 LOGSECTION_CARTRIDGE,
 LOGSECTION_INIT,
 LOGSECTION_AGENDA,
 //LOGSECTION_OPTIONS, // unused
 LOGSECTION_ACSI,
 LOGSECTION_VIDEO_RENDERING,
 LOGSECTION_SOUND,
 NUM_LOGSECTIONS
 };
#endif


//////////////////
// DEBUG MACROS //
//////////////////

extern BYTE FullScreen; // to avoid asserts in fullscreen


#if defined(VC_BUILD) && (0 || defined(_DEBUG)) // break in debugger
#if defined(SSE_X64_DEBUG)
#define ASSERT(x) {if(!(x) && !FullScreen) DebugBreak();}
#else
#define ASSERT(x) {if(!((x)) && !FullScreen) _asm{int 0x03}}
#endif
#elif defined(SSE_UNIX__) // forget it, we just get a segmentation fault!
#define ASSERT(x) assert(x)
#elif defined(SSE_LIBRETRO) //temp
#define ASSERT(x) {if(!(x) && !FullScreen) DebugBreak();}
#else
#define ASSERT(x) Debug.Assert(!!(x),#x)
#endif

#if defined(SSE_BETA)
#define ASSERTB ASSERT
#else
#define ASSERTB(x)
#endif


// BREAKPOINT 
#if defined(SSE_BETA)
#if defined(_DEBUG) && defined(VC_BUILD)
#if defined(SSE_X64_DEBUG)
#define BREAKPOINT(x) {DebugBreak();}
#else
#define BREAKPOINT(x) _asm { int 3 }
#endif
#elif defined(SSE_UNIX_TRACE___) // forget it
#define BREAKPOINT(x) {assert(0);}
#else
#ifdef __cplusplus
#define BREAKPOINT(x) {if(!Debug.IgnoreErrors) { \
  TRACEB("Breakpoint: %s\n",#x); \
  Debug.IgnoreErrors=!(MessageBox(0,#x,"Breakpoint",MB_ICONWARNING|MB_OKCANCEL)==IDOK);}}
#endif//c++
#endif
#else //!SSE_DEBUG
#define BREAKPOINT(x) {}
#endif


// TRACE
#if defined(SSE_DEBUG_TRACE) // TRACE works in all builds
#define TRACE Debug.Trace
#define FLUSH_TRACE Debug.FlushTrace()
#else
#if defined(VC_BUILD) || defined(SSE_UNIX)
#define TRACE(x,...)
#else
#define TRACE
#endif
#define FLUSH_TRACE
#endif

// TRACEB is a beta-only trace
#if defined(SSE_DEBUG_TRACE) && defined(SSE_BETA)
#define TRACEB Debug.Trace
#elif defined(VC_BUILD) || defined(SSE_UNIX)
#define TRACEB(x,...)
#else
#define TRACEB //bcc
#endif


// TRACE_LOG_ENABLED
#if defined(SSE_ENABLE_TRACE_LOG)
#define TRACE_LOG_ENABLED(section) (logsection_enabled[(section)])
#else
#define TRACE_LOG_ENABLED (0)
#endif

// TRACE_LOG: section defined in source
#if defined(SSE_ENABLE_TRACE_LOG)
#ifdef __cplusplus
#define TRACE_LOG Debug.LogSection=LOGSECTION, Debug.TraceLog //!
//#define TRACE_ONLY(x) x
#endif//C++
#else
#if defined(VC_BUILD) || defined(SSE_UNIX)
#define TRACE_LOG(x,...)
//#define TRACE_ONLY(x)
#else
#define TRACE_LOG
//#define TRACE_ONLY(x)
#endif
#endif//#if defined(SSE_ENABLE_TRACE_LOG)

// TRACE_SECTION: section as parameter
#if defined(SSE_ENABLE_TRACE_LOG)
#ifdef __cplusplus
#define TRACE_SECTION Debug.TraceLog
#endif//C++
#else
#if defined(VC_BUILD) || defined(SSE_UNIX)
#define TRACE_SECTION(x,...)
#else
#define TRACE_SECTION
#endif
#endif//#if defined(SSE_ENABLE_TRACE_LOG)

// log detail 1->3
#if defined(SSE_LOGSECTIONS_FOR_ALL)
#define TRACE_LOG2_ENABLED (SSEConfig.TraceFile>=2)
#define TRACE_LOG3_ENABLED (SSEConfig.TraceFile>=3)
#define TRACE_LOG2 if(TRACE_LOG2_ENABLED) Debug.LogSection=LOGSECTION,Debug.TraceLog
#define TRACE_LOG3 if(TRACE_LOG3_ENABLED) Debug.LogSection=LOGSECTION,Debug.TraceLog
#elif defined(DEBUG_BUILD) // detailed traces commanded by Control Mask Browser //never defined now
#ifdef __cplusplus
#define TRACE_LOG2_ENABLED (TRACE_MASK0&TRACE_LEVEL2)
#define TRACE_LOG3_ENABLED (TRACE_MASK0&TRACE_LEVEL3)
#define TRACE_LOG2 if(TRACE_LOG2_ENABLED) Debug.LogSection=LOGSECTION,Debug.TraceLog
#define TRACE_LOG3 if(TRACE_LOG3_ENABLED) Debug.LogSection=LOGSECTION,Debug.TraceLog
#endif//C++
#else
#define TRACE_LOG2_ENABLED false
#define TRACE_LOG3_ENABLED false
#if defined(VC_BUILD) || defined(SSE_UNIX)
#define TRACE_LOG2(x,...)
//#define TRACE_LOG2(x,...) Debug.LogSection=LOGSECTION,Debug.TraceLog
#define TRACE_LOG3(x,...)
#else
#define TRACE_LOG2
#define TRACE_LOG3
#endif
#endif


// dumping disk sectors as they pass through DMA
#if defined(SSE_LOGSECTIONS_FOR_ALL)
#define SHOULD_DUMP_BYTES (TRACE_LOG2_ENABLED)
#elif defined(SSE_DEBUGGER_FAKE_IO)
#define SHOULD_DUMP_BYTES (TRACE_MASK3 & TRACE_CONTROL_FDCBYTES)
#else
#define SHOULD_DUMP_BYTES false
#endif


// TRACE_MFM 3.7.1
#if defined(SSE_DEBUGGER_FAKE_IO) 
#ifdef __cplusplus
#define TRACE_MFM if(TRACE_MASK3&TRACE_CONTROL_FDCMFM) Debug.TraceLog
#endif//C++
#elif defined(VC_BUILD) || defined(SSE_UNIX)
#define TRACE_MFM(x,...)
#else
#define TRACE_MFM
#endif


#if defined(SSE_DEBUGGER_TRACE_EVENTS) //3.8.0
#define TRACE_EVENT(x,y) Debug.TraceEvent(x,y)
#else
#define TRACE_EVENT(x) 
#endif


// OSD
#if defined(SSE_DEBUG) && defined(SSE_OSD_DEBUGINFO)
#define TRACE_OSD OsdControl.Trace // (...)
#define TRACE_OSD_DBG if(OPTION_OSD_DEBUGINFO)OsdControl.Trace
#elif defined(VC_BUILD) || defined(SSE_UNIX)
#define TRACE_OSD(x,...)
#define TRACE_OSD_DBG(x,...)
#else
#define TRACE_OSD //?
#define TRACE_OSD_DBG
#endif

#define TRACE_OSD_FPS if(OPTION_OSD_FPSINFO) OsdControl.Trace


// VERIFY note: generally VERIFY is not used
#if defined(SSE_BETA)
#if defined(_DEBUG) && defined(VC_BUILD)
// Our VERIFY facility has no MFC dependency.
#if defined(SSE_X64_DEBUG)
#define VERIFY(x) {if(!(x) && !FullScreen) DebugBreak();}
#else
#define VERIFY(x) {if(!((x)) && !FullScreen) _asm{int 0x03}}
#endif
#elif defined(SSE_UNIX_TRACE___) // forget it
#define VERIFY(x) if ((x)==0) {TRACE("Verify failed: %s\n",#x); assert(0);} 
#else
#ifdef __cplusplus
#define VERIFY(x) {if((x)==0) {TRACEB("Verify failed: %s\n",#x); \
  if(!Debug.IgnoreErrors&&!FullScreen) { \
  int ok8788=MessageBox(0,#x,"VERIFY",MB_ICONWARNING|MB_ABORTRETRYIGNORE);   \
  if(ok8788==IDABORT) exit(EXIT_FAILURE);\
  Debug.IgnoreErrors=(ok8788==IDIGNORE);}}}
#endif//C++
#endif
#else //!SSE_BETA
#define VERIFY(x) ((void)(x))
#endif


#if defined(__cplusplus)

#if defined(SSE_ENABLE_TRACE_LOG)
extern const char* gemdos_calls[0x58];
extern const char* bios_calls[12];
extern const char* xbios_calls[40];
extern bool logsection_enabled[100];
#endif

#if defined(DEBUG_BUILD) || defined(SSE_LOGSECTIONS_FOR_ALL)

struct struct_logsection {
  char *Name;
  int Index;
};

extern struct_logsection logsections[NUM_LOGSECTIONS+8];
void logfile_wipe();
#endif

extern bool debug_wipe_log_on_reset;

#if defined(DEBUG_BUILD)

#define IOACCESS_DEBUG_MEM_DMA BIT_13
#define IOACCESS_DEBUG_MEM_WRITE_LOG BIT_14
#define IOACCESS_DEBUG_MEM_READ_LOG BIT_15
extern MEM_ADDRESS debug_mem_write_log_address;
extern int debug_mem_write_log_bytes;

#endif//#if defined(DEBUG_BUILD)

#endif//#if defined(__cplusplus)

#endif//#ifndef SSEDEBUG_H
