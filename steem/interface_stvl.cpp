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

DOMAIN: Emu
FILE: interface_stvl.cpp
CONDITION: SSE_VID_STVL must be defined
DESCRIPTION: This file contains client code for STVL, a plugin for ST 
emulators that replicates the video logic of an STF or an STE at low level.
The video logic of the Mega versions is the same.
The plugin hasn't been released yet.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#if defined(SSE_VID_STVL)

#include <debug.h>
#include <interface_stvl.h>
#include <draw.h>
#include <computer.h>
#include <gui.h>
#include <translate.h>
#if defined(SSE_DEBUGGER_FRAME_REPORT)
#include <debug_framereport.h>
#endif


HMODULE hStvl=NULL;
TStvl Stvl; // We own the full data set

DWORD(STVL_CALLCONV *video_logic_init)(TStvl *pStvl)=NULL;
void(STVL_CALLCONV *video_logic_reset)(TStvl *pStvl,bool Cold)=NULL;
void(STVL_CALLCONV *video_logic_stf_run)(TStvl *pStvl,int cycles)=NULL;
void(STVL_CALLCONV *video_logic_ste_run)(TStvl *pStvl,int cycles)=NULL;
#if defined(SSE_MEGA16)
void(STVL_CALLCONV *video_logic_run)(TStvl *pStvl,int cycles)=NULL; // set to stf or ste logic
#endif
void(STVL_CALLCONV *video_logic_update)(TStvl *pStvl)=NULL;
//void(STVL_CALLCONV *video_logic_update_pal)(TStvl *pStvl, int n, WORD pal)=NULL;

// STVL data includes variables for the ST bus, to spare cycles we
// directly use those in Steem itself
MEM_ADDRESS& abus=Stvl.abus;
DU16 &udbus=Stvl.udbus;
WORD& dbus=Stvl.udbus.d16;
BYTE& dbusl=Stvl.udbus.d8[LO];
BYTE& dbush=Stvl.udbus.d8[HI];
DWORD *draw_mem_line_ptr;
WORD render_vstart,render_vend;
WORD render_hstart;
WORD render_scanline_length;


WORD CALLBACK PeekWord(DWORD addr) {
#ifndef BIG_ENDIAN_PROCESSOR
  WORD video_word= (addr<mem_len) ? *(WORD*)(Mem_End_minus_2-addr) : dbus;
#endif
  return video_word;
}


void StvlInit() {
  hStvl=SteemLoadLibrary(VIDEO_LOGIC_DLL);
  if(hStvl)
  {
    video_logic_init=(DWORD (STVL_CALLCONV*)(TStvl*))GetProcAddress(hStvl,"STVL_init");
    video_logic_reset=(void (STVL_CALLCONV*)(TStvl*,bool))GetProcAddress(hStvl,"STVL_reset");
    video_logic_stf_run=(void (STVL_CALLCONV*)(TStvl*,int))GetProcAddress(hStvl,"STVL_stf_run");
    video_logic_ste_run=(void (STVL_CALLCONV*)(TStvl*,int))GetProcAddress(hStvl,"STVL_ste_run");
#if defined(SSE_MEGA16)
#ifndef SSE_LEAN_AND_MEAN
    video_logic_run=video_logic_ste_run;
#endif
#endif
    video_logic_update=(void (STVL_CALLCONV*)(TStvl*))GetProcAddress(hStvl,"STVL_update");
    //video_logic_update_pal = (void(STVL_CALLCONV*)(TStvl*,int,WORD))GetProcAddress(hStvl, ""STVL_update_pal");
    DWORD version=video_logic_init(&Stvl);
    char tmp[40];
    sprintf(tmp,"%s v%x",VIDEO_LOGIC_DLL,version);
    TRACE_SECTION(LOGSECTION_INIT,"%s loaded\n",tmp);
    //SSEConfig.Stvl=(version&0xFFFF);
#ifndef SSE_X64
#ifdef SSE_STDCALL // 1st version returned nothing, so just alert if suspicious
    if(!(version&0x80000000)) // bit set if dll expects __stdcall
#else
    if(version&0x80000000)
#endif
    {
      Alert(tmp,T("Warning"),MB_OK|MB_ICONWARNING);
      SSEConfig.Stvl=TRUE; // else it could be garbage eg $800
    }
    else
#endif
      SSEConfig.Stvl=version&0xFFFF;
    if(SSEConfig.Stvl>0x0100) // there was no version variable before v101!
      Stvl.version=SSEConfig.Stvl; // the plugin wants it to be the same as its internal version
    if(!SSEConfig.Stvl)
      SteemFreeLibrary(hStvl);
    Stvl.cbHInt=event_scanline; // hbl pending + scanline routines
    Stvl.cbVInt=event_trigger_vbi; // vbl pending + frame routines
    Stvl.cbDe=cb_mfp_de_transition;
#if defined(SSE_DEBUGGER_FRAME_REPORT)
    Stvl.cbTraceVideoEvent=FrameEvents_Add;
#endif
    if(SSEConfig.Stvl>=TStvl::VER_STESND)
      Stvl.cbFetchSound=Mmu.SoundFetch;
    // comment off to test dll rendering (check: doesn't react to colour control)
    Stvl.pPCpal=PCpal;
#if defined(SSE_X64)
/*  Order in struct was different for 32bit and 64bit by accident, so we swap the
    callback functions, which is confusing... */
    if(SSEConfig.Stvl<0x0102)
    {
      Stvl.cbPeekWord=(WORD(*)(DWORD)) &TShifter::SetPal;
      Stvl.cbSetPal=(void(*)(int,WORD)) PeekWord;
    }
    else
#endif
    {
      Stvl.cbPeekWord=PeekWord;
      Stvl.cbSetPal=&TShifter::SetPal; // must be static now, no big trouble
    }
  }
  else // if snapshot later loaded with STVL enabled
    ZeroMemory(&Stvl,sizeof(Stvl));
}


void StvlUpdate() {
  if(hStvl==NULL)
    return;
  Stvl.VideoOut=COLOUR_MONITOR+1;
  Stvl.mono_col=(STpal[0]&1); // extra register in Shifter
  // timer B tick event
  if(Mfp.reg[MFPR_AER]&MFP_GPIP_BLITTER_MASK)
  {
    Stvl.ctrDeOn=1;
    Stvl.ctrDeOff=0;
  }
  else // general case
  {
    Stvl.ctrDeOn=0;
    Stvl.ctrDeOff=1;
  }
 #if defined(SSE_HARDWARE_OVERSCAN)
  if(SSEConfig.OverscanOn)
  {
    Stvl.hwoverscan=OPTION_HWOVERSCAN;
    if(COLOUR_MONITOR)
    {
      switch(OPTION_HWOVERSCAN) {
      case LACESCAN:
        render_hstart=210;
        render_vstart=(border==3)?(25-(BIG_BORDER_TOP-ORIGINAL_BORDER_TOP)):25;
        render_vend=500-3;
        if(Stvl.framefreq==60)
          render_vstart-=(63-34),render_vend-=(63-34);
        render_vend=render_vstart+TopBorderSize+BottomBorderSize+200;
        if(border>=2)
          render_hstart-=(52-32)*4;
        break;
      case AUTOSWITCH:
        render_hstart=210;
        render_vstart=(border==3)?(33-(BIG_BORDER_TOP-ORIGINAL_BORDER_TOP)):33;
        render_vstart-=6+2+1;
        if(Stvl.framefreq==60)
          render_vstart-=(63-34);
        render_vend=render_vstart+TopBorderSize+BottomBorderSize+200;
        if(border>=2)
          render_hstart-=(52-32)*4;
        break;
      }
    }
    else // monochrome
    {
      switch(OPTION_HWOVERSCAN) {
      case LACESCAN:
        render_vstart=(border==3)?(35-(BIG_BORDER_TOP-ORIGINAL_BORDER_TOP)):35;
        // afraid of horrible complication if trying to do it a blit time
        if(Stvl.framefreq==MONO_HZ)
          render_vstart-=(63-36);
        render_vend=500;
        render_hstart=106+68; // hides trash
        break;
      case AUTOSWITCH: // GLU wakeup 2 advised for monochrome but we don't enforce...
        render_vstart=(border==3)?(35-(BIG_BORDER_TOP-ORIGINAL_BORDER_TOP)):35;
        // afraid of horrible complication if trying to do it a blit time
        if(Stvl.framefreq==MONO_HZ)
          render_vstart-=(63-36);
        render_vend=500; //-35?
        render_hstart=110+60;
        break;
      }//sw
    }
  }
  else
#endif//hwo
  if(COLOUR_MONITOR) // standard colour
  {
    render_hstart=236; // border 32
    render_vstart=33;
    if(Stvl.framefreq==60)
    {
      render_hstart-=16;
      render_vstart-=GLU_PAL_TOPSCANLINES-GLU_NTSC_TOPSCANLINES; // (63-34);
      if(SSEConfig.Border60Hz)
        render_vstart+=12;
    }
    render_vend=render_vstart+TopBorderSize+BottomBorderSize+200;
    if(!border)
    {
      short offset=Mmu.DL[OPTION_WS]*4-20; // must see no WS shift
      render_hstart+=32*4+offset;
      render_vstart=(WORD)((Stvl.framefreq==50) ? GLU_PAL_TOPSCANLINES : GLU_NTSC_TOPSCANLINES);
      render_vend=render_vstart+200;
    }
    else if(border>=2)
      render_hstart-=(52-32)*4;
    render_hstart+=(OPTION_SHIFTER_WU-1)* ((Shifter.ShiftMode==1) ? 2 : 4);
  }
  else // standard monochrome
  {
    render_vstart=28;
    // afraid of horrible complication if trying to do it a blit time
    if(Stvl.framefreq==MONO_HZ)
      render_vstart-=(63-36);
    render_vend=500;
    render_hstart=164+16+2;
    if(!border)
    {
      short offset=Mmu.DL[OPTION_WS]*8-20-16-4-12;
      render_hstart+=32*4+offset;
      if(IS_STF) //? todo
        render_hstart+=6;
      render_vstart=33+2; //?
      if(IS_STE) //? todo
        render_vstart+=1;
      render_vend=render_vstart+400;
    }
    if(OPTION_UNSTABLE_SHIFTER) //?
      render_hstart+=2+(OPTION_SHIFTER_WU-1)*2;
  }
  render_scanline_length=(640+(border!=0)*SideBorderSizeWin*2*2); // =precomp
  render_hstart/=2;
  Stvl.st_model=(IS_STE+1); 
  Stvl.wakestate=(IS_STE) ? 1 : Mmu.WS[OPTION_WS];
  Stvl.shifter_wakeup=OPTION_SHIFTER_WU-1;
  Stvl.UnstableShifter=!!(OPTION_UNSTABLE_SHIFTER);
#if defined(SSE_VID_STVL_DIRECT_RAM) //no
/*  STVL can directly access video RAM of Steem
*/
  if(SSEConfig.Stvl>=TStvl::VER_DIRECTRAM)
  {
    ASSERTB((mem_len>>16)&0xFF);
    Stvl.mem_len2=(BYTE)(mem_len>>16);
#ifndef BIG_ENDIAN_PROCESSOR
    Stvl.cbPeekWord=(WORD(CALLBACK*)(DWORD))Mem_End_minus_2; // cast BYTE pointer to function pointer
#endif
  } // no else to restore cbPeekWord?
#endif
  video_logic_update(&Stvl);
}


// called on DE transition, arrange for Timer B tick event if necessary
void CALLBACK cb_mfp_de_transition() {
  time_of_next_timer_b=A_S_T+Stvl.tick8*TICKS8; // now
#if defined(SSE_MFP_421) // PP49: the counter will be activated by then
  if(1)
#else
  if(Mfp.reg[MFPR_TBCR]==MFP_TIMER_EVENT_COUNT) // MFP configured to count TBI ticks
#endif
  {
    // add time of MFP internal processing
    if(SSEOptions.MfpTbSync||SSEOptions.MfpTbTclk)
      time_of_next_timer_b+=Mfp.SyncXtal(time_of_next_timer_b,SSEOptions.MfpTbTclk);
    time_of_next_timer_b+=SSEOptions.MfpTbCpu;
    time_of_next_timer_b+=MFP_TIMER_B_COUNT_CYCLES_STVL;
    prepare_next_event();
  }
  else
    time_of_next_timer_b+=EIGHT_MILLION;
}


/*  We use timings (CPU, Blitter, DMA cycles) to drive the video logic emulation,
    this way we can be cycle accurate without rewriting the whole emulation.
    It's a downside of our method that many timing functions are necessary
    (the rest of this file).
    This is in part because we also provide functions for an accelerated ST, even
    if precise video logic emu makes little sense then. (TODO?)
    The load is extraordinary. An older PC (Pentium D?) can't run it. This is
    where we hit a limitation of software emulation as compared with hardware.
    Bus access statistics are not collected (they would be the same)
*/

#define CHECK_BLIT_REQUEST   if(Blitter.Request) Blitter_CheckRequest()
#define BLIT_CYCLES          Blitter.BlitCycles
#define BLIT_BUS_ACCESS_CTR  Blitter.BusAccessCounter
#define BUS_IDLE_CYCLES      Cpu.BusIdleCycles


// 1 low-level video, no acceleration

void BusStf1Idle(int t) {
  BUS_MASK=0;
  video_logic_stf_run(&Stvl,t*(4/TICKS8)); // call STVL before changing cycles
  sys_cycles-=(t);
}


void BusSte1Idle(int t) {
  BUS_MASK=0;
  while(BLIT_CYCLES>t && t>0)
  {
    BLIT_CYCLES--;
    t--;
  }
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=(t);
  BUS_IDLE_CYCLES+=t;
  CHECK_BLIT_REQUEST;
}


#if defined(SSE_MEGA16)

void Bus1WS(int t) {
  video_logic_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=(t);
}


void Bus2WS(int t) {
  if(t>0)
  {
    acc_cycles+=t;
    if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
    {
      acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
      video_logic_run(&Stvl,12*4/TICKS8);
    }
  }
  sys_cycles-=(t);
}


void BusMega1Idle(int t) {
  BUS_MASK=0;
  while(BLIT_CYCLES>t && t>0)
  {
    BLIT_CYCLES--;
    t--;
  }
  BUS_IDLE_CYCLES+=t;
  if(Cpu16.ScuReg&2)
    t/=2;
  video_logic_run(&Stvl,t*(4/TICKS8)); // Mega ST uses STF video logic, Mega STE uses STE video logic
  sys_cycles-=(t);
  CHECK_BLIT_REQUEST;
}


void BusMega1PrefetchOnly() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  abus=pc&0xfffffe;
  int t;
  if(Cpu16.IsFast(abus))
    t=2*TICKS8;
  else
  {
    t=4*TICKS8;
    if(abus<MEM_4MB)
    {
      t+=(sys_cycles&(4*TICKS8-1));
      Cpu16.Add(abus);
    }
  }
  video_logic_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  IRC=m68k_fetch(pc);
  CHECK_BLIT_REQUEST;
}


void BusMega1PrefetchFinal() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  IR=IRC;
  MEM_ADDRESS au=pc+2;
  abus=au&0xfffffe;
  int t;
  if(Cpu16.IsFast(abus))
    t=2*TICKS8;
  else
  {
    t=4*TICKS8;
    if(abus<MEM_4MB)
    {
      t+=(sys_cycles&(4*TICKS8-1));
      Cpu16.Add(abus);
    }
  }
  video_logic_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  IRC=m68k_fetch(au);
  CHECK_BLIT_REQUEST;
}


void BusMega1PrefetchTotal() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  pc+=2;
  abus=pc&0xfffffe;
  int t;
  if(Cpu16.IsFast(abus))
    t=2*TICKS8;
  else
  {
    t=4*TICKS8;
    if(abus<MEM_4MB)
    {
      t+=(sys_cycles&(4*TICKS8-1));
      Cpu16.Add(abus);
    }
  }
  video_logic_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  IRC=m68k_fetch(pc);
  CHECK_BLIT_REQUEST;
}


void BusMega1ReadB() {
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_LOBYTE)
                     : (BUS_MASK_ACCESS|BUS_MASK_HIBYTE);
  abus=iabus&0x00FFFFFE;
  int t;
  if(Cpu16.IsFast(abus))
    t=2*TICKS8;
  else
  {
    t=4*TICKS8;
    if(abus<MEM_4MB)
    {
      t+=(sys_cycles&(4*TICKS8-1));
      Cpu16.Add(abus);
    }
  }
  video_logic_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  m68k_peek(iabus);
  CHECK_BLIT_REQUEST;
}


void BusMega1Read() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WORD;
  abus=iabus&0x00FFFFFE;
  int t;
  if(Cpu16.IsFast(abus))
    t=2*TICKS8;
  else
  {
    t=4*TICKS8;
    if(abus<MEM_4MB)
    {
      t+=(sys_cycles&(4*TICKS8-1));
      Cpu16.Add(abus);
    }
  }
  video_logic_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  dbus=m68k_dpeek(iabus);
  CHECK_BLIT_REQUEST;
}


void BusMega1WriteB() {
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_LOBYTE)
                     : (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_HIBYTE);
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  dbush=dbusl;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  m68k_poke_abus(dbusl);
  Cpu16.Add(abus);
  CHECK_BLIT_REQUEST;
}


void BusMega1Write() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  m68k_dpoke_abus(dbus);
  Cpu16.Add(abus);
  CHECK_BLIT_REQUEST;
}


void BusMega1BltRead() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WORD;
  int t=4*TICKS8; // Blitter 8MHz
  if(abus<MEM_4MB && abus>=MEM_FIRST_WRITEABLE)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  Cpu16.Add(abus); // address in cache
}


void BusMega1BltWrite() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
  int t=4*TICKS8;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  Cpu16.Add(abus); // address in cache
}

#else//#if defined(SSE_MEGA16)

void BusStf1WS(int t) {
  video_logic_stf_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=(t);
}


void BusSte1WS(int t) {
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=(t);
}


void BusMegaSte1WS(int t) { // same
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=(t);
}


void BusMegaSt1WS(int t) {
  video_logic_stf_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=(t);
}


void BusStf2WS(int t) {
  if(t>0)
  {
    acc_cycles+=t;
    if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
    {
      acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
      video_logic_stf_run(&Stvl,12*4/TICKS8);
    }
  }
  sys_cycles-=(t);
}


void BusSte2WS(int t) {
  if(t>0)
  {
    acc_cycles+=t;
    if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
    {
      acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
      video_logic_ste_run(&Stvl,12*4/TICKS8);
    }
  }
  sys_cycles-=(t);
}


#if defined(SSE_MEGAST)

void BusMegaSt2WS(int t) {
  if(t>0)
  {
    acc_cycles+=t;
    if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
    {
      acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
      video_logic_stf_run(&Stvl,12*4/TICKS8);
    }
  }
  sys_cycles-=(t);
}


void BusMegaSt1Idle(int t) {
  BUS_MASK=0;
  while(BLIT_CYCLES>t && t>0)
  {
    BLIT_CYCLES--;
    t--;
  }
  video_logic_stf_run(&Stvl,t*(4/TICKS8)); // Mega ST uses STF video logic
  sys_cycles-=(t);
  BUS_IDLE_CYCLES+=t;
  CHECK_BLIT_REQUEST;
}


void BusMegaSt1PrefetchOnly() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=pc&0xfffffe;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_stf_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  IRC=m68k_fetch(pc);
  CHECK_BLIT_REQUEST;
}


void BusMegaSt1PrefetchFinal() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  IR=IRC;
  MEM_ADDRESS au=pc+2;
  int t=4*TICKS8;
  abus=au&0xfffffe;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_stf_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  IRC=m68k_fetch(au);
  CHECK_BLIT_REQUEST;
}


void BusMegaSt1PrefetchTotal() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  pc+=2;
  int t=4*TICKS8;
  abus=pc&0xfffffe;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_stf_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  IRC=m68k_fetch(pc);
  CHECK_BLIT_REQUEST;
}


void BusMegaSt1ReadB() {
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_LOBYTE)
    : (BUS_MASK_ACCESS|BUS_MASK_HIBYTE);
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  if(abus<MEM_4MB && abus>=MEM_FIRST_WRITEABLE)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_stf_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  m68k_peek(iabus);
  CHECK_BLIT_REQUEST;
}


void BusMegaSt1Read() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  if(abus<MEM_4MB && abus>=MEM_FIRST_WRITEABLE)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_stf_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  dbus=m68k_dpeek(iabus);
  CHECK_BLIT_REQUEST;
}


void BusMegaSt1BltRead() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WORD;
  int t=4*TICKS8;
  if(abus<MEM_4MB && abus>=MEM_FIRST_WRITEABLE)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_stf_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
}


void BusMegaSt1Write() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_stf_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  m68k_dpoke_abus(dbus);
  CHECK_BLIT_REQUEST;
}


void BusMegaSt1WriteB() {
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_LOBYTE)
    : (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_HIBYTE);
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  dbush=dbusl;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_stf_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  m68k_poke_abus(dbusl);
  CHECK_BLIT_REQUEST;
}


void BusMegaSt1BltWrite() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
  int t=4*TICKS8;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_stf_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
}


void BusMegaSt2Idle(int t) {
  if(t>0)
  {
    BUS_MASK=0;
    acc_cycles+=t;
    if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
    {
      acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
      video_logic_stf_run(&Stvl,12*4/TICKS8);
    }
  }
  while(BLIT_CYCLES>t && t>0)
  {
    BLIT_CYCLES--;
    t--;
  }
  sys_cycles-=(t);
  BUS_IDLE_CYCLES+=t;
  CHECK_BLIT_REQUEST;
}


void BusMegaSt2Read() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
    video_logic_stf_run(&Stvl,12*4/TICKS8);
  }
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  dbus=m68k_dpeek(iabus);
  CHECK_BLIT_REQUEST;
}


void BusMegaSt2PrefetchOnly() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=pc&0xfffffe;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
    video_logic_stf_run(&Stvl,12*4/TICKS8);
  }
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  IRC=m68k_fetch(pc);
  CHECK_BLIT_REQUEST;
}


void BusMegaSt2PrefetchFinal() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  IR=IRC;
  MEM_ADDRESS au=pc+2;
  int t=4*TICKS8;
  abus=au&0xfffffe;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
    video_logic_stf_run(&Stvl,12*4/TICKS8);
  }
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  IRC=m68k_fetch(au);
  CHECK_BLIT_REQUEST;
}


void BusMegaSt2PrefetchTotal() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  pc+=2;
  int t=4*TICKS8;
  abus=pc&0xfffffe;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
    video_logic_stf_run(&Stvl,12*4/TICKS8);
  }
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  IRC=m68k_fetch(pc);
  CHECK_BLIT_REQUEST;
}


void BusMegaSt2ReadB() {
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_LOBYTE)
    : (BUS_MASK_ACCESS|BUS_MASK_HIBYTE);
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
    video_logic_stf_run(&Stvl,12*4/TICKS8);
  }
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  m68k_peek(iabus);
  CHECK_BLIT_REQUEST;
}


void BusMegaSt2BltRead() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WORD;
  int t=4*TICKS8;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
    video_logic_stf_run(&Stvl,12*4/TICKS8);
  }
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
}


void BusMegaSt2Write() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
    video_logic_stf_run(&Stvl,12*4/TICKS8);
  }
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  m68k_dpoke_abus(dbus);
  CHECK_BLIT_REQUEST;
}


void BusMegaSt2WriteB() {
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_LOBYTE)
    : (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_HIBYTE);
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  dbush=dbusl;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
    video_logic_stf_run(&Stvl,12*4/TICKS8);
  }
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  m68k_poke_abus(dbusl);
  CHECK_BLIT_REQUEST;
}


void BusMegaSt2BltWrite() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
  int t=4*TICKS8;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
    video_logic_stf_run(&Stvl,12*4/TICKS8);
  }
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
}

#endif//#if defined(SSE_MEGAST)


#if defined(SSE_MEGASTE)

void BusMegaSte1Idle(int t) {
  BUS_MASK=0;
  while(BLIT_CYCLES>t && t>0)
  {
    BLIT_CYCLES--;
    t--;
  }
  BUS_IDLE_CYCLES+=t;
  if(Cpu16.ScuReg&2)
    t/=2;
  video_logic_ste_run(&Stvl,t*(4/TICKS8)); // Mega STE uses STE video logic
  sys_cycles-=(t);
  CHECK_BLIT_REQUEST;
}


void BusMegaSte1PrefetchOnly() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  abus=pc&0xfffffe;
  int t;
  if(Cpu16.IsFast(abus))
    t=2*TICKS8;
  else
  {
    t=4*TICKS8;
    if(abus<MEM_4MB)
    {
      t+=(sys_cycles&(4*TICKS8-1));
      Cpu16.Add(abus);
    }
  }
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  IRC=m68k_fetch(pc);
  CHECK_BLIT_REQUEST;
}


void BusMegaSte1PrefetchFinal() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  IR=IRC;
  MEM_ADDRESS au=pc+2;
  abus=au&0xfffffe;
  int t;
  if(Cpu16.IsFast(abus))
    t=2*TICKS8;
  else
  {
    t=4*TICKS8;
    if(abus<MEM_4MB)
    {
      t+=(sys_cycles&(4*TICKS8-1));
      Cpu16.Add(abus);
    }
  }
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  IRC=m68k_fetch(au);
  CHECK_BLIT_REQUEST;
}


void BusMegaSte1PrefetchTotal() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  pc+=2;
  abus=pc&0xfffffe;
  int t;
  if(Cpu16.IsFast(abus))
    t=2*TICKS8;
  else
  {
    t=4*TICKS8;
    if(abus<MEM_4MB)
    {
      t+=(sys_cycles&(4*TICKS8-1));
      Cpu16.Add(abus);
    }
  }
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  IRC=m68k_fetch(pc);
  CHECK_BLIT_REQUEST;
}


void BusMegaSte1ReadB() {
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_LOBYTE)
    : (BUS_MASK_ACCESS|BUS_MASK_HIBYTE);
  abus=iabus&0x00FFFFFE;
  int t;
  if(Cpu16.IsFast(abus))
    t=2*TICKS8;
  else
  {
    t=4*TICKS8;
    if(abus<MEM_4MB)
    {
      t+=(sys_cycles&(4*TICKS8-1));
      Cpu16.Add(abus);
    }
  }
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  m68k_peek(iabus);
  CHECK_BLIT_REQUEST;
}


void BusMegaSte1Read() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WORD;
  abus=iabus&0x00FFFFFE;
  int t;
  if(Cpu16.IsFast(abus))
    t=2*TICKS8;
  else
  {
    t=4*TICKS8;
    if(abus<MEM_4MB)
    {
      t+=(sys_cycles&(4*TICKS8-1));
      Cpu16.Add(abus);
    }
  }
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  dbus=m68k_dpeek(iabus);
  CHECK_BLIT_REQUEST;
}


void BusMegaSte1Write() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  m68k_dpoke_abus(dbus);
  Cpu16.Add(abus);
  CHECK_BLIT_REQUEST;
}


void BusMegaSte1WriteB() {
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_LOBYTE)
    : (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_HIBYTE);
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  dbush=dbusl;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  m68k_poke_abus(dbusl);
  Cpu16.Add(abus);
  CHECK_BLIT_REQUEST;
}

#endif//#if defined(SSE_MEGASTE)

#endif//#if defined(SSE_MEGA16)


void BusStf1PrefetchOnly() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=pc&0xfffffe;
  // rounding is computed before the call to STVL and applied after
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_stf_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  IRC=m68k_fetch(pc);
}


void BusStf1PrefetchFinal() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  IR=IRC;
  MEM_ADDRESS au=pc+2;
  int t=4*TICKS8;
  abus=au&0xfffffe;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_stf_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  IRC=m68k_fetch(au);
}


void BusStf1PrefetchTotal() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  pc+=2;
  int t=4*TICKS8;
  abus=pc&0xfffffe;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_stf_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  IRC=m68k_fetch(pc);
}


void BusSte1PrefetchOnly() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=pc&0xfffffe;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  IRC=m68k_fetch(pc);
  CHECK_BLIT_REQUEST;
}


void BusSte1PrefetchFinal() {
  int t=4*TICKS8;
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  IR=IRC;
  MEM_ADDRESS au=pc+2;
  abus=au&0xfffffe;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  IRC=m68k_fetch(au);
  CHECK_BLIT_REQUEST;
}


void BusSte1PrefetchTotal() {
  int t=4*TICKS8;
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  pc+=2;
  abus=pc&0xfffffe;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  IRC=m68k_fetch(pc);
  CHECK_BLIT_REQUEST;
}


void BusStf1ReadB() {
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_LOBYTE)  : (BUS_MASK_ACCESS|BUS_MASK_HIBYTE);
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  if(abus<MEM_4MB && abus>=MEM_FIRST_WRITEABLE)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_stf_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  m68k_peek(iabus);
}


void BusSte1ReadB() {
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_LOBYTE) : (BUS_MASK_ACCESS|BUS_MASK_HIBYTE);
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  if(abus<MEM_4MB && abus>=MEM_FIRST_WRITEABLE)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  m68k_peek(iabus);
  CHECK_BLIT_REQUEST;
}


void BusStf1Read() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  if(abus<MEM_4MB && abus>=MEM_FIRST_WRITEABLE)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_stf_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  dbus=m68k_dpeek(iabus);
}


void BusSte1Read() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  if(abus<MEM_4MB && abus>=MEM_FIRST_WRITEABLE)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  dbus=m68k_dpeek(iabus);
  CHECK_BLIT_REQUEST;
}


void BusSte1BltRead() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WORD;
  int t=4*TICKS8;
  if(abus<MEM_4MB && abus>=MEM_FIRST_WRITEABLE)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
}


void BusStf1Write() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_stf_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  m68k_dpoke_abus(dbus);
}


void BusSte1Write() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  m68k_dpoke_abus(dbus);
  CHECK_BLIT_REQUEST;
}


void BusStf1WriteB() {
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_LOBYTE)
                     : (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_HIBYTE);
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  dbush=dbusl; // STVL expects doubled bytes
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_stf_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  m68k_poke_abus(dbusl);
}


void BusSte1WriteB() {
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_LOBYTE)
                     : (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_HIBYTE);
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  dbush=dbusl;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  m68k_poke_abus(dbusl);
  CHECK_BLIT_REQUEST;
}


void BusSte1BltWrite() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
  int t=4*TICKS8;
  if(abus<MEM_4MB)
    t+=(sys_cycles&(4*TICKS8-1)); 
  video_logic_ste_run(&Stvl,t*(4/TICKS8));
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
}


// 2 low-level video, acceleration
// we call the video logic when enough cycles have been accumulated
// we don't waste time with rounding

COUNTER_VAR acc_cycles=0;

void BusStf2Idle(int t) {
  if(t>0)
  {
    BUS_MASK=0;
    acc_cycles+=t;
    if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
    {
      acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
      video_logic_stf_run(&Stvl,12*4/TICKS8);
    }
  }
  sys_cycles-=(t);
}


void BusSte2Idle(int t) {
  if(t>0)
  {
    BUS_MASK=0;
    acc_cycles+=t;
    if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
    {
      acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
#if defined(SSE_MEGA16)
      video_logic_run(&Stvl,12*4/TICKS8);
#else
      video_logic_ste_run(&Stvl,12*4/TICKS8);
#endif
    }
  }
  while(BLIT_CYCLES>t && t>0)
  {
    BLIT_CYCLES--;
    t--;
  }
  sys_cycles-=(t);
  BUS_IDLE_CYCLES+=t;
  CHECK_BLIT_REQUEST;
}


void BusStf2PrefetchOnly() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=pc&0xfffffe;
  // we don't round up in accelerated mode
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
    video_logic_stf_run(&Stvl,12*4/TICKS8);
  }
  sys_cycles-=t;
  IRC=m68k_fetch(pc);
}


void BusStf2PrefetchFinal() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  IR=IRC;
  MEM_ADDRESS au=pc+2;
  int t=4*TICKS8;
  abus=au&0xfffffe;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
    video_logic_stf_run(&Stvl,12*4/TICKS8);
  }
  sys_cycles-=t;
  IRC=m68k_fetch(au);
}


void BusStf2PrefetchTotal() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  pc+=2;
  int t=4*TICKS8;
  abus=pc&0xfffffe;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
    video_logic_stf_run(&Stvl,12*4/TICKS8);
  }
  sys_cycles-=t;
  IRC=m68k_fetch(pc);
}


void BusSte2PrefetchOnly() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=pc&0xfffffe;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
#if defined(SSE_MEGA16)
    video_logic_run(&Stvl,12*4/TICKS8);
#else
    video_logic_ste_run(&Stvl,12*4/TICKS8);
#endif
  }
  sys_cycles-=t;
  IRC=m68k_fetch(pc);
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  CHECK_BLIT_REQUEST;
}


void BusSte2PrefetchFinal() {
  int t=4*TICKS8;
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  IR=IRC;
  MEM_ADDRESS au=pc+2;
  abus=au&0xfffffe;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
#if defined(SSE_MEGA16)
    video_logic_run(&Stvl,12*4/TICKS8);
#else
    video_logic_ste_run(&Stvl,12*4/TICKS8);
#endif
  }
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  IRC=m68k_fetch(au);
  CHECK_BLIT_REQUEST;
}


void BusSte2PrefetchTotal() {
  int t=4*TICKS8;
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  pc+=2;
  abus=pc&0xfffffe;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
#if defined(SSE_MEGA16)
    video_logic_run(&Stvl,12*4/TICKS8);
#else
    video_logic_ste_run(&Stvl,12*4/TICKS8);
#endif
  }
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  IRC=m68k_fetch(pc);
  CHECK_BLIT_REQUEST;
}


void BusStf2ReadB() {
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_LOBYTE) : (BUS_MASK_ACCESS|BUS_MASK_HIBYTE);
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
    video_logic_stf_run(&Stvl,12*4/TICKS8);
  }
  sys_cycles-=t;
  m68k_peek(iabus);
}


void BusSte2ReadB() {
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_LOBYTE) : (BUS_MASK_ACCESS|BUS_MASK_HIBYTE);
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
#if defined(SSE_MEGA16)
    video_logic_run(&Stvl,12*4/TICKS8);
#else
    video_logic_ste_run(&Stvl,12*4/TICKS8);
#endif
  }
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  m68k_peek(iabus);
  CHECK_BLIT_REQUEST;
}


void BusStf2Read() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
    video_logic_stf_run(&Stvl,12*4/TICKS8);
  }
  sys_cycles-=t;
  dbus=m68k_dpeek(iabus);
}


void BusSte2Read() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
#if defined(SSE_MEGA16)
    video_logic_run(&Stvl,12*4/TICKS8);
#else
    video_logic_ste_run(&Stvl,12*4/TICKS8);
#endif
  }
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  dbus=m68k_dpeek(iabus);
  CHECK_BLIT_REQUEST;
}


void BusSte2BltRead() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WORD;
  int t=4*TICKS8;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
#if defined(SSE_MEGA16)
    video_logic_run(&Stvl,12*4/TICKS8);
#else
    video_logic_ste_run(&Stvl,12*4/TICKS8);
#endif
  }
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
}


void BusStf2Write() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
    video_logic_stf_run(&Stvl,12*4/TICKS8);
  }
  sys_cycles-=t;
  m68k_dpoke_abus(dbus);
}


void BusSte2Write() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
#if defined(SSE_MEGA16)
    video_logic_run(&Stvl,12*4/TICKS8);
#else
    video_logic_ste_run(&Stvl,12*4/TICKS8);
#endif
  }
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  m68k_dpoke_abus(dbus);
  CHECK_BLIT_REQUEST;
}


void BusStf2WriteB() {
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_LOBYTE)
                     : (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_HIBYTE);
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  dbush=dbusl;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
    video_logic_stf_run(&Stvl,12*4/TICKS8);
  }
  sys_cycles-=t;
  m68k_poke_abus(dbusl);
}


void BusSte2WriteB() {
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_LOBYTE)
                     : (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_HIBYTE);
  int t=4*TICKS8;
  abus=iabus&0x00FFFFFE;
  dbush=dbusl;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
#if defined(SSE_MEGA16)
    video_logic_run(&Stvl,12*4/TICKS8);
#else
    video_logic_ste_run(&Stvl,12*4/TICKS8);
#endif
  }
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  m68k_poke_abus(dbusl);
  CHECK_BLIT_REQUEST;
}


void BusSte2BltWrite() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
  int t=4*TICKS8;
  acc_cycles+=t;
  if(acc_cycles>=cpu_cycles_multiplier*CPU_FAST_CYCLES)
  {
    acc_cycles-=(COUNTER_VAR)(cpu_cycles_multiplier*CPU_FAST_CYCLES);
#if defined(SSE_MEGA16)
    video_logic_run(&Stvl,12*4/TICKS8);
#else
    video_logic_ste_run(&Stvl,12*4/TICKS8);
#endif
  }
  sys_cycles-=t;
  BLIT_BUS_ACCESS_CTR++;
}

#undef CHECK_BLIT_REQUEST
#undef BLIT_CYCLES
#undef BLIT_BUS_ACCESS_CTR
#undef BUS_IDLE_CYCLES
#undef LOGSECTION

#endif//#if defined(SSE_VID_STVL)
