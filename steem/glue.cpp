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
FILE: glue.cpp
DESCRIPTION: The Glue (or GLU for General Logic Unit) is an Atari custom
chip with various functions like address decoding, interrupt propagation,
bus arbitration, bus error, video timings.
Address decoding is handled here (high byte) and in the cpu, iow and ior files.
The central routine to update the IPL lines is here, as well as emulations of
the horizontal and vertical sync interrupts.
This file also contains a high-level emulation of the video timings, and
of "Shifter tricks" manipulating those timings. There's quite a few lines
of code for that as such tricks were numerous.
Bus error and bus arbitration are handled in the CPU parts.
On the STE, the Glue and the MMU have been merged together and enhanced,
producing the GSTMCU. In Steem SSE, we do as if they were still separate, 
STE emulation uses the same Glue and Mmu objects (and not an object derived
from both).
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <draw.h>
#include <computer.h>
#include <debug.h>
#include <osd.h>
#if defined(SSE_DEBUGGER_FRAME_REPORT)
#include <debug_framereport.h>
#endif
#if defined(SSE_VID_STVL1)
#include <interface_stvl.h>
#endif
#include <iolist.h>

BYTE const SCANLINE_BYTES_HIGH=80; // bytes of video memory per scanline, normal
BYTE const SCANLINE_BYTES_LOMED=160; // bytes of video memory per scanline, normal


TGlue::TGlue() {
  ZeroMemory(this,sizeof(TGlue));
  Restore();
  PreviousVideoFreq=VideoFreq=NTSC_HZ;
  CurrentScanline.Cycles=CyclesPerScanline8MHz[FREQ_IDX_60];
}


void TGlue::Restore() {
  TRACE_SECTION(LOGSECTION_INIT,"TGlue::Restore()\n");
  DE_cycles[FREQ_IDX_71]=SCANLINE_BYTES_HIGH*2*TICKS8; // 4 cycles per word = 160
  DE_cycles[FREQ_IDX_50]=DE_cycles[FREQ_IDX_60]=DE_cycles[FREQ_IDX_71]<<1; // = 320
  Freq[FREQ_IDX_50]=PAL_HZ;
  Freq[FREQ_IDX_60]=NTSC_HZ;
  Freq[FREQ_IDX_71]=MONO_HZ;
  ShiftMode&=3; // 2bit register
  SyncMode&=3; // 2bit register
  if(VideoFreq!=Freq[FREQ_IDX_50] && VideoFreq!=Freq[FREQ_IDX_60] && VideoFreq!=Freq[FREQ_IDX_71])
  {
    if(screen_res<HIRES)
      VideoFreq=(SyncMode&SYNCPAL) ? Freq[FREQ_IDX_50] : Freq[FREQ_IDX_60];
    else
      VideoFreq=Freq[FREQ_IDX_71];
  }
#if defined(SSE_TIMINGS_US)
  int idx=FREQ_IDX_50;
  if(ShiftMode&HIRES) // or test screen_res
  {
    nLines=GLU_MONO_SCANLINES; //501
    idx=FREQ_IDX_71;
  }
  else 
  {
    if(SyncMode&SYNCPAL) // 50hz
      nLines=GLU_PAL_SCANLINES; //313
    else // 60hz
    {
      nLines=GLU_NTSC_SCANLINES; //263
      idx++;
    }
  }
  nFrameCycles=nLines*CyclesPerScanline[idx];
#endif
#ifdef DEADC0DE // bad idea STEKMAG1: no vbl interrupt, no events, hangs
  if(VCount>=nLines-3)
    VCount=0; // Auto053/Erebus TOS 1.0
#endif

  ////////////
  // DECODE //
  ////////////

/*  Interpret in advance the high byte of 24bit addresses according to the ST model
    Build a look-up table we can use in peek/poke */
  BYTE CartBase=(BYTE)(cartbase>>16),CartEnd=(BYTE)(cartend>>16);
#if defined(SSE_MMU_MONSTER_ALT_RAM)
  BYTE AltRamEnd=(BYTE)(Mmu.MonSTerHimem>>16);
#endif
#if defined(SSE_TOS206)
  // STE TOS is mapped to $E00000-$E39999, STF TOS to $FC0000-$FE9999
  // TOS 2.06 is the ultimate Atari TOS for the STE and STF. If installed in an STF or Mega ST,
  // it is necessary to hack the GLUE ROM lines so that they react to the STE range
  // The hack is part of the kit (or it's an expansion card)
  // It would have been simpler if Atari had started TOS at $E00000 from the beginning
#if defined(SSE_MEGA_421)
  // With the same hack, an STE with 4MB will happily run STF TOS 1.00!
 bool bTosLow=(OPTION_HACKS||tos_version==0x206)?(!tos_high):(!!IS_STE);
#else
  bool bTosLow=!tos_high;// && (IS_STE||OPTION_HACKS);
#endif
#else
  bool bTosLow=(IS_STE);
#endif
  BYTE StRamEnd=(BYTE)(himem>>16);
  for(WORD b=0;b<=0xFF;b++)
  {
    Decode[b]=BERR;
#if defined(SSE_420R5) //opt
    if(b<0x40)
    {
      if(Mmu.Confused)
        Decode[b]=MMU_CONFUSED;
      else if(b==0x00)
        Decode[b]=STRAM_OR_ROM;
      else if(b<StRamEnd)
        Decode[b]=(BYTE)((SSEOptions.TrackVC&&!OPTION_C3)?STRAM_C2:STRAM);
      else
        Decode[b]=(BYTE)(((SSEOptions.TrackVC&&!OPTION_C3)?STRAM_CHECK_C2:STRAM_CHECK)); // can be empty RAM
    }
#else
    if(b==0x00)
      Decode[b]=(BYTE)(Mmu.Confused?MMU_CONFUSED:STRAM_OR_ROM); // 0-7 is ROM
    else if(b<StRamEnd)
#if defined(SSE_GUI_EMUCONTROL)
      Decode[b]=(BYTE)(Mmu.Confused?MMU_CONFUSED
        :(SSEOptions.TrackVC&&!OPTION_C3?STRAM_C2:STRAM)); // faster handling possible!
#else
      Decode[b]=(BYTE)(Mmu.Confused?MMU_CONFUSED:(OPTION_C2?STRAM_C2:STRAM)); // faster handling possible!
#endif
    else if(b<0x40)
      Decode[b]=(BYTE)(Mmu.Confused?MMU_CONFUSED:(OPTION_C2?STRAM_CHECK_C2:STRAM_CHECK)); // can be empty RAM
#endif
#if defined(SSE_MMU_MONSTER_ALT_RAM)
    else if(b<AltRamEnd)
      Decode[b]=ALTRAM;
#endif
    else if(bTosLow && b>=0xE0 && b<0xEC)
      Decode[b]=(BYTE)((b<0xE4)?ROM:ROM1); // 'ROM1' not connected
    else if(!bTosLow && b>=0xFC && b<0xFF)
      Decode[b]=ROM;
    else if(b==0xFF)
      Decode[b]=DEV;
    else if(b>=CartBase && b<CartEnd)
    {
      if(cart)
        Decode[b]=(BYTE)((CartBase==0xFA) ? CART : CART2);
      else
        Decode[b]=CART3; // no cartridge
    }
    else if(IS_STE && b>=0xD0 && b<0xD8)
      Decode[b]=ROM1; // no bus error
    else if(IS_STE && b==0xFE)
      Decode[b]=ROM_CHECK; // must check address
  }
}


void TGlue::Reset(bool Cold) {
  SyncMode=ShiftMode=0;
  VideoFreqIdx=FREQ_IDX_60;
  VideoFreq=Freq[VideoFreqIdx]; //NTSC_HZ;
  CurrentScanline.Cycles=CyclesPerScanline[VideoFreqIdx];
  if(Cold)
  {
    TimeOfHSyncOff=hbl_pending_time=vbl_pending_time=0;
    VCount=0;
    vbl_pending=false;
    cartbase=GLU_CARTBASE;
    cartend=cartbase+0x020000;
  }
  memset(&m_Status,0,sizeof(m_Status));
  gamecart=hscroll=false;
#if defined(SSE_VID_STVL1)
  StvlUpdate();
#endif
}


/*  SetShiftMode() and SetSyncMode() are called when a program writes
    on addresses $FF8260 (shift mode) or $FF820A (sync mode).
*/

#define LOGSECTION LOGSECTION_GLUE

void TGlue::SetShiftMode(BYTE NewRes) {
/*
  The ST possesses three modes  of  video  configuration:
  320  x  200  resolution  with 4 planes, 640 x 200 resolution
  with 2 planes, and 640 x 400 resolution with 1  plane.   The
  modes  are  set through the Shift Mode Register (read/write,
  reset: all zeros).

  ff 8260   R/W             |------xx|   Shift Mode
                                   ||
                                   00       320 x 200, 4 Plane
                                   01       640 x 200, 2 Plane
                                   10       640 x 400, 1 Plane
                                   11       Reserved

  FF8260 is both in the GLU and the Shifter. It is needed in the GLU
  because sync signals are different in mode 2 (71hz).
  It is needed in the Shifter because it needs to know in how many bit planes
  memory has to be decoded, and where it must send the video signal (RGB, 
  Mono).
  For the GLU, '3' is interpreted as '2'. Case: The World is my Oyster screen #2

  Writes on ShiftMode have a 2 cycle resolution as far as the GLU is
  concerned, 4 for the Shifter. The GLU's timing matters for some video effects,
  that's why the write is handled by the Glue object.
    
  In monochrome, frequency is 71hz, a line is transmitted in 28us.
  There are 500 scanlines + vsync = 1 scanline time.
*/

  SHORT CyclesIn=LINECYCLES;
  BYTE OldRes=ShiftMode;
  // Only two lines physically exist in the Glue and the Shifter, not a full byte
  NewRes&=3; 

#if defined(SSE_DEBUGGER_FRAME_REPORT)
  if(!OPTION_C3 &&(FRAME_REPORT_MASK1 & FRAME_REPORT_MASK_SHIFTMODE))
    FrameEvents.Add(scan_y,CyclesIn/TICKS8,'R',NewRes); 
#endif
#if defined(SSE_DEBUGGER_FAKE_IO)
  if(TRACE_MASK1&TRACE_CONTROL_GLUREG)
  {
    TRACE_LOG("%d %d %d GLU RES %d\n",TIMING_INFO,NewRes);
  }
#endif

  ShiftMode=NewRes; // GLUE's copy
  if(screen_res>HIRES || OPTION_C3&&screen_res==HIRES)
    return; // if not, bad display in high resolution
#if !defined(SSE_NO_FALCONMODE)
  if(emudetect_falcon_mode!=EMUD_FALC_MODE_OFF)
    return;
#endif
#ifndef NO_CRAZY_MONITOR
  if(extended_monitor)
  {
    screen_res=NewRes&MEDRES;
    return;
  }
#endif
#if defined(SSE_VID_STVL_UPD)
  Stvl.mde0=ShiftMode&MEDRES; // 0: LORES, 1: MEDRES
  Stvl.mde1=ShiftMode>>1;
#endif
  if(!OPTION_C3)
  {
    if(NewRes==3) 
      NewRes=HIRES;
    if(NewRes!=OldRes)
      AddShiftModeChange(NewRes); // add time & mode
    AddFreqChange((NewRes&HIRES) ? MONO_HZ : ( (SyncMode&SYNCPAL)?PAL_HZ:NTSC_HZ ));
    Shifter.Render(CyclesIn,TShifter::DISPATCHER_SET_SHIFT_MODE);
    if(screen_res==HIRES && !COLOUR_MONITOR)
    {
      freq_change_this_scanline=true;
      return;
    }
  }
  int old_screen_res=screen_res;
  screen_res=NewRes&MEDRES; // only for 0 or 1 - note could weird things happen?
  if(screen_res!=old_screen_res)
  {
#if defined(SSE_SHIFTER_421)
    // if there's HSCROLL and resolution changes before DE, adapt starting pixel
    if(shifter_tick8==(HSCROLL>>old_screen_res))
      shifter_tick8=HSCROLL>>screen_res; // A Letter To Sommarhack
#endif
    shifter_x=(screen_res>LORES) ? HOR_PIXELS_MED : HOR_PIXELS_LO;
    if(draw_lock)
      draw_scanline=(screen_res==LORES) ? draw_scanline_lowres : draw_scanline_medres;
    if(video_mixed_output==3 && (a_s_t-sys_timer_at_res_change<30*TICKS8))
    {
      video_mixed_output=0; //cancel!
    }
    else if(scan_y<-30) // not displaying anything: no output to mix...
    {} // eg Pandemonium/Chaos Dister
    else if(!video_mixed_output)
    {
      video_mixed_output=3;
    }
    else if(video_mixed_output<2)
    {
      //TRACE_OSD_DBG("video_mixed_output");
      video_mixed_output=VIDEO_MIXED_ESTABLISHED;
    }
    sys_timer_at_res_change=a_s_t;
  }
  if(OPTION_C3)
    return;
  freq_change_this_scanline=true; // all switches are interesting
  if(video_last_draw_line==400 && !(ShiftMode&HIRES) && screen_res<HIRES)
  {
    video_last_draw_line>>=1; // simplistic?
    draw_line_off=true; // Steem's original flag for black line
  }
  AdaptScanlineValues(CyclesIn);
}


void TGlue::SetSyncMode(BYTE NewSync) {
/*
    ff 820a   R/W             |------xx|   Sync Mode
                                     ||
                                     | ----   External/_Internal Sync
                                      -----   50 Hz/_60 Hz Field Rate
    Reset: 0
    Only bit 1 is of interest:  1:50 Hz 0:60 Hz.
    Normally, 50hz for Europe, 60hz for the USA.
    At 50hz, the ST displays 313 lines every frame, instead of 312.5 like
    in the PAL standard (one frame with 312 lines, one with 313, etc.) 
    Sync mode is abused to create overscan (3 of the 4 borders).
    If set, bit 0 paralyses the video logic, unless there's real external
    sync.
*/
  SHORT CyclesIn=LINECYCLES;

  // Only two lines physically exist in the Glue and the Shifter, not a full byte
  SyncMode=NewSync&3; // 2bit

#if defined(SSE_DEBUGGER_FRAME_REPORT)
  if(!OPTION_C3 &&(FRAME_REPORT_MASK1 & FRAME_REPORT_MASK_SYNCMODE))
    FrameEvents.Add(scan_y,CyclesIn/TICKS8,'S',SyncMode); 
#endif
#if defined(SSE_DEBUGGER_FAKE_IO)
  if(TRACE_MASK1&TRACE_CONTROL_GLUREG)
  {
    TRACE_LOG("%d %d %d GLU SYNC %d\n",TIMING_INFO,SyncMode);
  }
#endif

#if defined(SSE_VID_STVL_UPD)
  Stvl.exts=SyncMode&1;
  Stvl.pal=SyncMode>>1;
#endif
  if(OPTION_C2 && bFetchingLine)
    CheckSideOverscan(); // force check to adapt timer B to right off...
  if(screen_res>=HIRES)
    VideoFreqIdx=FREQ_IDX_71;
  else if(NewSync&SYNCPAL)
    VideoFreqIdx=FREQ_IDX_50;
  else
    VideoFreqIdx=FREQ_IDX_60;
  //ASSERT(VideoFreqIdx>=0 && VideoFreqIdx<NFREQS);
  BYTE new_freq=Freq[VideoFreqIdx];
  ASSERTB(new_freq==50||new_freq==60||new_freq==MONO_HZ);
  if(VideoFreq!=new_freq)
    freq_change_this_scanline=true;  
  VideoFreq=new_freq;
  ASSERTB(VideoFreq);
  if(OPTION_C3)
    return;
  AddFreqChange(new_freq);
  AdaptScanlineValues(CyclesIn);
}


void TGlue::Vbl() {
  // event_vbl_interrupt() called at cycle 6X with STVL, 0 without
  if(!OPTION_C3) 
  {
    TimeOfHSyncOff=time_of_next_event;
    scan_y=-TopScanlines[VideoFreqIdx]; // needed for Debugger frame by frame C2
  }
  else if(VideoFreqIdx==FREQ_IDX_71)
    scan_y--; // for traces
//  int h=0; scan_y/=h; //crash test
#if defined(SSE_HARDWARE_OVERSCAN)
  if(OPTION_HWOVERSCAN && SSEConfig.OverscanOn)
  {
    SHORT start;
    // hack to get correct display
    if(COLOUR_MONITOR)
    {
      start=(border==3)?-39:-30;
      video_last_draw_line=245;
    }
    else
    {
      start=-30-1;
      video_last_draw_line=471;
    }
    scan_y=start;
    video_first_draw_line=start+1;
  }
#endif
  m_Status.hbi_done=m_Status.vc_reload_done=false;
  m_Status.vbl_done=true;
  // Stopping now if emu thread makes snapshots more compatible
  if(m_Status.stop_emu==1)
  {
    runstate=RUNSTATE_STOPPING;
#ifdef WIN32
#if defined(SSE_STATS)
    Stats.tFrameT=Stats.tBlit1.QuadPart-Stats.tFrameT; // start and stop counting at same place
#endif
#endif
  }
  else
  {
    if(m_Status.stop_emu>=2) // only possible with option No OSD on stop
      m_Status.stop_emu--;
#if defined(SSE_STATS) // reset one frame stats
    Stats.nPal=Stats.nTimerbtick=Stats.nBlit1=Stats.nHbi1=Stats.nReadvc1
      =Stats.nVbaseMid1=Stats.nLinePlus16=0;
#ifdef WIN32
    if(Stats.tFrameT==0)
      Stats.tFrameT=Stats.tBlit1.QuadPart; // start and stop counting at same place
#endif
#endif
  }
#if defined(SSE_DEBUGGER_FAKE_IO)
  if(OSD_MASK2&OSD_CONTROL_MODES)
    TRACE_OSD("R%d S%d",Shifter.ShiftMode,SyncMode);
#endif
  // pushing current freq to avoid spurious overscan detection on timer rollover
  // we don't mess with a_s_t here, it would trigger spurious interrupts!
  AddFreqChange(VideoFreq); 
  AddShiftModeChange(ShiftMode); 
}

#undef LOGSECTION


//////////////////////
// VIDEO INTERRUPTS //
//////////////////////

#define LOGSECTION LOGSECTION_INTERRUPTS


// The IPL (Interrupt Priority Level) lines go from the GLUE to the CPU.
// The interrupt priority logic inside the GLUE is instant.
// This function is called every time IPL could be changed: interrupt pending
// or cleared, and by the MFP updater.
void update_ipl(COUNTER_VAR when) {
  BYTE level;  // can be only 0, 2, 4, 6 (HW: reversed bits, active-low)
               // In the ST, only IPL2 and IPL1 are used, IPL0 is always high
  if(Mfp.Irq)
    level=IPL_MFP; // IPL1 and 2 low
  else if(Glue.vbl_pending)
    level=IPL_VBL; // IPL2 low
  else if(Glue.hbl_pending)
    level=IPL_HBL; // IPL1 low
  else
    level=0;
  if(level!=ipl_timing_ipl[ipl_timing_index]) // only real changes
  {
    ipl_timing_index++; // byte 0-255 should overflow
    ipl_timing_ipl[ipl_timing_index]=level;
    ipl_timing_time[ipl_timing_index]=when;
    TRACE_LOG3(PRICV ": IPL %d\n",when,level); 
  }
}


void HBLInterrupt() {
  // Horizontal interrupt, set pending at the end of HSYNC, IPL 2
  // That interrupt is generally not used by the OS and the programs
#ifdef SSE_DEBUG
  Debug.nHbis++; // knowing how many in the frame is interesting
  //COUNTER_VAR a=A_S_T;
#if defined(SSE_DEBUGGER)
  Debug.FrameInterrupts|=1;
#endif
#if defined(SSE_DEBUGGER_FRAME_REPORT)
  if(FRAME_REPORT_MASK2 & FRAME_REPORT_MASK_INT)
    FrameEvents.Add(scan_y,LINECYCLES/TICKS8,'I',0x20);
#endif
#if defined(SSE_DEBUGGER_FAKE_IO)
  if(TRACE_MASK2&TRACE_CONTROL_IRQ_SYNC)
    TRACE_LOG(PRICV " (%d %d %d) HBI #%d %d Vec %X\n",A_S_T,TIMING_INFO,Debug.nHbis,LPEEK(0x0068));
#endif
#if defined(SSE_DEBUGGER)
  pc_history_y[pc_history_idx]=scan_y;
  pc_history_c[pc_history_idx]=LINECYCLES;
  pc_history[pc_history_idx++]=0x99000001+(2<<16);
  if(pc_history_idx>=HISTORY_SIZE)
    pc_history_idx=0;
#endif
#endif//dbg
#if defined(SSE_STATS)
  Stats.nHbi++;
#endif
  M68K_UNSTOP;
  Cpu.ProcessingState=TMC68000::EXCEPTION;
  CPU_BUS_IDLE(2); //n
  UPDATE_SR;
  WORD saved_sr=SR; //copy sr
  CPU_BUS_IDLE(4); //nn
  change_to_supervisor_mode();
  CLEAR_T;
  PSWI=2; // update ipl mask
  iabus=SP-2;
  dbus=pcl; // stack PC low word;
  CPU_BUS_ACCESS_WRITE; // ns 12
  iabus-=4;
  SP=iabus;
  // start autovector IACK bus cycle
  // e-clock wait-states
  BUS_WAIT_STATES(6); //n ni
  int e_clock_wait_states=Cpu.SyncEClock();
  BUS_WAIT_STATES(e_clock_wait_states); //ni * ?
  // trigger event such as scanline in case hbl is pending again: Monaco GP
  // timing of this: European Demos WS1 C2
#if defined(SSE_420R4) // while is overkill?
  if(sys_cycles<=0)
#else
  while(sys_cycles<=0)
#endif
  {
#if defined(SSE_DEBUGGER_FAKE_IO)
    if(TRACE_MASK2&TRACE_CONTROL_EVENT)
    {
      TRACE_EVENT("HBI",event_vector);
    }
#endif
    event_vector();
    prepare_next_event();
  }
  BUS_WAIT_STATES(4); // ni 
  time_of_last_hbl_interrupt=ABSOLUTE_SYS_TIME; //after wobble or e-clock cycles
  Glue.hbl_pending=false;
  update_ipl(time_of_last_hbl_interrupt);
  CPU_BUS_IDLE(4); //nn
  dbus=saved_sr; // SR written between two parts of PC
  CPU_BUS_ACCESS_WRITE; // ns
  iabus+=2;
  dbus=pch; // PC high word 
  CPU_BUS_ACCESS_WRITE; // nS
  iabus=0x0068;
  CPU_BUS_ACCESS_READ; // nV
  effective_address_h=DPEEK(iabus); // iabus guaranteed to point to existing memory
  iabus+=2;
  CPU_BUS_ACCESS_READ; // nv
  effective_address_l=DPEEK(iabus);
  Cpu.ProcessingState=TMC68000::NORMAL;
  m68kSetPC(effective_address,2);
  Glue.m_Status.hbi_done=true;
  debug_check_break_on_irq(BREAK_IRQ_HBL_IDX);
  interrupt_depth++;
}


void VBLInterrupt() {
  // Vertical interrupt, set pending at the end of VSYNC, IPL 4
  // That interrupt is used by the OS and some programs
#ifdef SSE_DEBUG
#if defined(SSE_DEBUGGER_FRAME_REPORT)
  if(FRAME_REPORT_MASK2 & FRAME_REPORT_MASK_INT)
    FrameEvents.Add(scan_y,LINECYCLES/TICKS8,'I',0x40);
#endif
#if defined(SSE_DEBUGGER)
  Debug.FrameInterrupts|=2;
#endif
#if defined(SSE_DEBUGGER_FAKE_IO)
  if(TRACE_MASK2&TRACE_CONTROL_IRQ_SYNC)
    TRACE_LOG(PRICV " (%d %d %d) ird %X VBI Vec %X sr %X\n",A_S_T,TIMING_INFO,IRD,LPEEK(0x0070),SR);
#endif
#if defined(SSE_DEBUGGER) 
  pc_history_y[pc_history_idx]=scan_y;
  pc_history_c[pc_history_idx]=LINECYCLES;
  pc_history[pc_history_idx++]=0x99000001+(4<<16);
  if(pc_history_idx>=HISTORY_SIZE)
    pc_history_idx=0;
#endif
#endif//dbg
#if defined(SSE_STATS)
  Stats.nVbi++;
#endif
  M68K_UNSTOP;
  Cpu.ProcessingState=TMC68000::EXCEPTION;
  CPU_BUS_IDLE(2); //n
  UPDATE_SR;
  WORD saved_sr=SR; //copy sr
  CPU_BUS_IDLE(4); //nn
  change_to_supervisor_mode();
  CLEAR_T;
  PSWI=4; // update ipl mask
  iabus=SP-2;
  dbus=pcl; // stack PC low word;
  CPU_BUS_ACCESS_WRITE; // ns 12
  iabus-=4;
  SP=iabus;
  // start autovector IACK bus cycle
  // between 10 and 18 cycles for autovector
  // = 4 + 6 + eclock ws (max 8)
  // e-clock wait-states
  BUS_WAIT_STATES(6); //n ni
  int e_clock_wait_states=Cpu.SyncEClock();
  BUS_WAIT_STATES(e_clock_wait_states); // ni * ?
  // trigger event (vbl pending again?)
#if defined(SSE_420R4)
  if(sys_cycles<=0)
#else
  while(sys_cycles<=0)
#endif
  {
#if defined(SSE_DEBUGGER_FAKE_IO)
    if(TRACE_MASK2&TRACE_CONTROL_EVENT)
    {
      TRACE_EVENT("VBI",event_vector);
    }
#endif
    event_vector();
    prepare_next_event();
  }
  BUS_WAIT_STATES(4); // ni
  time_of_last_vbl_interrupt=ABSOLUTE_SYS_TIME;
  Glue.vbl_pending=false;
  update_ipl(time_of_last_vbl_interrupt);
  CPU_BUS_IDLE(4); //nn nn
  dbus=saved_sr; // SR written between two parts of PC
  CPU_BUS_ACCESS_WRITE; // ns
  iabus+=2;
  dbus=pch; // PC high word 
  CPU_BUS_ACCESS_WRITE; // nS
  iabus=0x0070;
  CPU_BUS_ACCESS_READ; // nV
  effective_address_h=DPEEK(iabus); // iabus guaranteed to point to existing memory
  iabus+=2;
  CPU_BUS_ACCESS_READ; // nv
  effective_address_l=DPEEK(iabus);
  Cpu.ProcessingState=TMC68000::NORMAL;
  m68kSetPC(effective_address,2);
  debug_check_break_on_irq(BREAK_IRQ_VBL_IDX);
  interrupt_depth++;
}

#undef LOGSECTION


///////////////////
// VIDEO TIMINGS //
///////////////////

#define LOGSECTION LOGSECTION_GLUE


void TGlue::AdaptScanlineValues(SHORT const CyclesIn) { 
  // called on set sync or shift mode
  // on write HSCROLL
  // on IncScanline (CyclesIn=-1), so on each scanline
  //ASSERT(!OPTION_C3);
  if(bFetchingLine && !(CurrentScanline.Tricks&TRICK_0BYTE_LINE))
  {
    //currently in HIRES
    if(ShiftMode&HIRES) 
    {
      if(CyclesIn<=ScanlineTiming[LINE_STOP_LIMIT][FREQ_IDX_71])
      {
        CurrentScanline.EndCycle=ScanlineTiming[DE_OFF][FREQ_IDX_71];
        if(CyclesIn<=ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_71])
          CurrentScanline.StartCycle=ScanlineTiming[DE_ON+hscroll][FREQ_IDX_71];
      }
    } 
    // not in HIRES
    else if(CyclesIn<=ScanlineTiming[LINE_STOP_LIMIT][FREQ_IDX_60] 
      && !(CurrentScanline.Tricks&(TRICK_0BYTE_LINE|TRICK_LINE_MINUS_106
                                  |TRICK_LINE_PLUS_44|TRICK_LINE_MINUS_2)))
    {
      CurrentScanline.EndCycle=(SyncMode&SYNCPAL)
        ? ScanlineTiming[DE_OFF][FREQ_IDX_50] : ScanlineTiming[DE_OFF][FREQ_IDX_60];
      if(CyclesIn<=ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_60]
        && !(CurrentScanline.Tricks&(TRICK_LINE_PLUS_26|TRICK_LINE_PLUS_20|TRICK_0BYTE_LINE)))
      {
        CurrentScanline.StartCycle=(SyncMode&SYNCPAL)
          ? ScanlineTiming[DE_ON+hscroll][FREQ_IDX_50] : ScanlineTiming[DE_ON+hscroll][FREQ_IDX_60];
      }
    }
/*  With regular HSCROLL, fetching starts earlier and ends at the same time
    as without scrolling.
    The extra words should be counted at the start of the line, not the end, 
    therefore it should "simply" be part of CurrentScanline.Bytes, but it
    complicates emulation... :)
*/
    if(IS_STE && CyclesIn<=CurrentScanline.StartCycle)
    {
      if(Mmu.ExtraBytesForHscroll)
        CurrentScanline.Bytes-=Mmu.ExtraBytesForHscroll;
      Mmu.ExtraBytesForHscroll=0;
      if(hscroll)
      {
        Mmu.ExtraBytesForHscroll=(ShiftMode&HIRES) ? 2 : (8-ShiftMode*4);
        CurrentScanline.Bytes+=Mmu.ExtraBytesForHscroll;
      } 
    }
    // we can't say =80 or =160 because of various "shifter tricks" changing those numbers
    // a bit hacky, saves a variable + rewriting
    if(CyclesIn<ScanlineTiming[LINE_STOP_LIMIT][FREQ_IDX_71])
    {
      if((ShiftMode&HIRES) && !(CurrentScanline.Tricks&TRICK_80BYTE_LINE))
      {
        CurrentScanline.Bytes-=SCANLINE_BYTES_HIGH;
        CurrentScanline.Tricks|=TRICK_80BYTE_LINE; // each line in HIRES
      }
      else if(!(ShiftMode&HIRES) && (CurrentScanline.Tricks&TRICK_80BYTE_LINE))
      {
        CurrentScanline.Bytes+=SCANLINE_BYTES_HIGH;
        CurrentScanline.Tricks&=~TRICK_80BYTE_LINE;
      }
    }
  }
  // this timing is only correct for 508/512 cycle scanlines
  // because the HSYNC counter is reset at a different time from HSYNC off
  // at 50Hz and 60Hz, contrary to 72Hz (ijor)
  if(CyclesIn<=(SHORT)ScanlineCyclesTiming*TICKS8)
  {
    if((ShiftMode&HIRES) && (CyclesIn==-1||PreviousScanline.Cycles!=CyclesPerScanline[FREQ_IDX_71]))
      CurrentScanline.Cycles=CyclesPerScanline[FREQ_IDX_71];
    else if(SyncMode&SYNCPAL)
      CurrentScanline.Cycles=CyclesPerScanline[FREQ_IDX_50];
    else
      CurrentScanline.Cycles=CyclesPerScanline[FREQ_IDX_60];
    prepare_next_event();
  }
  // call it when .Cycles <>0
  Mfp.ComputeNextTimerB();
}


/*  Instead of following a frame event plan as in old Steem versions, we compute
    video timings on the go.
    This function is called by run's prepare_next_event(), so it's called a lot.
    It increases emulation load but it allows us to easily handle sync and mode
    changes.
*/
void TGlue::GetNextVideoEvent() {
  // ASSERT(!OPTION_C3);
  // VBI is set pending some cycles into first scanline of frame, 
  // when VSYNC stops.
  // The video counter will be reloaded again (HW: continuously updated)
  if(!m_Status.vbi_done&&!VCount)
  {
    event_vector=(EVENTPROC)event_trigger_vbi;
    video_event.time=ScanlineTiming[ENABLE_VBI][VideoFreqIdx];
    if(!m_Status.hbi_done)
    {
      hbl_pending=true;
      hbl_pending_time=TimeOfHSyncOff;
      update_ipl(hbl_pending_time);
    }
  }
  // Video counter is reloaded 3 lines before the end of the frame (colour)
  // VBLANK is already on since a couple of scanlines. (? TODO)
  // VSYNC will start, which will trigger reloading of the Video Counter
  // by the Mmu.
  else if(!m_Status.vc_reload_done && (VCount==nLines-3&&nLines!=GLU_MONO_SCANLINES
                                      || nLines==GLU_MONO_SCANLINES&&VCount==nLines-1))
  {
    event_vector=event_start_vbl;
    video_event.time=ScanlineTiming[RELOAD_VC][VideoFreqIdx];
  }
  // event_vbl_interrupt() is Steem's internal frame or vbl routine, called
  // when all the cycles of the frame have elapsed. It normally happens during
  // the ST's VSYNC (between VSYNC start and VSYNC end).
  else if(!m_Status.vbl_done && VCount>=nLines-1)
  {
    event_vector=event_vbl_interrupt;
    video_event.time=CurrentScanline.Cycles;
    m_Status.vbi_done=false;
  }  
  // default event = scanline
  else
  {
    event_vector=(EVENTPROC)event_scanline;
    video_event.time=CurrentScanline.Cycles;
  }
#if defined(SSE_DEBUG)
  video_event.event=event_vector;
#endif
  if(SSEConfig.CpuBoosted  && event_vector!=(EVENTPROC)event_scanline)
    video_event.time=(COUNTER_VAR)((double)video_event.time*cpu_cycles_multiplier);
  time_of_next_event=video_event.time+TimeOfHSyncOff;
}


void TGlue::CheckSideOverscan() {
/*  Various GLU and Shifter tricks can change the border size and the number 
    of bytes fetched from video RAM, and shift pixels. 
    Those tricks can be used with two goals: use a larger display area
    (fullscreen demos are more impressive than borders-on demos), and/or
    scroll the screen by using "sync lines" (eg Enchanted Land, No Buddies Land).
    This function is a big extension of Steem's original draw_check_border_removal()
    with some additions inspired by Hatari (v1.6) in Steem SSE v3.3 and v3.4 and
    other additions later (some my own R&D!)
*/
  int t=0;
  SHORT CyclesIn=LINECYCLES;
  SHORT r0cycle=-1,r1cycle,r2cycle;

  /////////////
  // NO SYNC //
  /////////////

  if(SyncMode&SYNCEXT) // we emulate no genlock -> nothing displayed
    CurrentScanline.Tricks=TRICK_BLACK_LINE; // it's a simplification

  //////////////////////
  // HIGH RES EFFECTS //
  //////////////////////

/*  Monoscreen by Dead Braincells
    012:R0000 020:R0002                       -> 0byte
    024:R0000 032:R0002 164:R0000 192:R0002   -> right off, left off
*/
  if(screen_res==HIRES)
  {
    if(!freq_change_this_scanline)
      return;
    char fetched_bytes_mod=0;
    if(!(CurrentScanline.Tricks&TRICK_0BYTE_LINE)
      && CyclesIn>=ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_71]
      && !(ShiftModeAtCycle(ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_71])&HIRES))
    {
      CurrentScanline.Tricks|=TRICK_0BYTE_LINE;
      fetched_bytes_mod=-SCANLINE_BYTES_HIGH; // hack
      draw_line_off=true;
    }
    else if( !(CurrentScanline.Tricks&TRICK_HIRES_OVERSCAN) 
      && CyclesIn>=ScanlineTiming[LINE_STOP_LIMIT][FREQ_IDX_71]
      && !(ShiftModeAtCycle(ScanlineTiming[DE_OFF][FREQ_IDX_71])&HIRES))
    {
      CurrentScanline.Tricks|=TRICK_HIRES_OVERSCAN;
      fetched_bytes_mod=14;
    }
    SHIFT_SDP(fetched_bytes_mod);
    CurrentScanline.Bytes+=fetched_bytes_mod;
    return;
  }

  /////////////////
  // 0-BYTE LINE //
  /////////////////

/*  Various shift mode or sync mode switches trick the GLUE into passing a 
    scanline in video RAM while the monitor is still displaying a line.
    This is a way to implement "hardware" downward vertical scrolling 
    on a computer where it's not foreseen (No Buddies Land).
    0-byte lines can also be combined with other sync lines (Forest, etc.).

    Normal lines:

    Video RAM
    [][][][][][][][][][][][][][][][] (1)
    [][][][][][][][][][][][][][][][] (2)
    [][][][][][][][][][][][][][][][] (3)
    [][][][][][][][][][][][][][][][] (4)


    Screen
    [][][][][][][][][][][][][][][][] (1)
    [][][][][][][][][][][][][][][][] (2)
    [][][][][][][][][][][][][][][][] (3)
    [][][][][][][][][][][][][][][][] (4)


    0-byte line:

    Video RAM
    [][][][][][][][][][][][][][][][] (1)
    [][][][][][][][][][][][][][][][] (2)
    [][][][][][][][][][][][][][][][] (3)
    [][][][][][][][][][][][][][][][] (4)


    Screen
    [][][][][][][][][][][][][][][][] (1)
    -------------------------------- (0-byte line)
    [][][][][][][][][][][][][][][][] (2)
    [][][][][][][][][][][][][][][][] (3)
    [][][][][][][][][][][][][][][][] (4)
*/

  if(!(CurrentScanline.Tricks&(TRICK_0BYTE_LINE|TRICK_LINE_PLUS_26
    |TRICK_LINE_PLUS_20|TRICK_80BYTE_LINE|TRICK_LINE_PLUS_24|TRICK_LINE_PLUS_44)))
  {

/*  Test previous scanline for 0byte. Must do it here because timings
    can go beyond 512 -> interference with scanline routines. 
    We use the values in LJBK's table, taking care not to break
    emulation of other cases.
    When DE has been negated and the GLU misses HSYNC due to some trick,
    the GLU will fail to trigger next line, until next HSYNC.
    Does the monitor miss one HSYNC too? Anyway, the beam goes down.
    shift mode:
    No line 1 460-472 there's no HSYNC ON, no HBL interrupt
    Beyond/Pax Plax Parallax STF
    No line 2 474-512 there's no HSYNC OFF, no HBL interrupt
    No Buddies Land, Pulsion 172 WS2 (unaligned)
*/
    if((!(PreviousScanline.Tricks&TRICK_LINE_PLUS_44)
      &&ShiftModeAtCycle(ScanlineTiming[HSYNC_ON2][FREQ_IDX_50])&HIRES) ||
      (CyclesIn>=ScanlineTiming[HSYNC_OFF2][FREQ_IDX_50] 
      && (ShiftModeAtCycle(ScanlineTiming[HSYNC_OFF2][FREQ_IDX_50])&HIRES)))
    {
      CurrentScanline.Tricks|=TRICK_0BYTE_LINE;
      VCount--;
      video_last_draw_line++; // TODO scan_y-- is worse
    }
/*  We test for 0byte line at the start of the scanline, affecting
    current scanline.

    Two timings may be targeted.

    HIRES HSYNC ON: premature HSYNC, DE is never asserted
    (ljbk) 0byte line   16-40 on STF
    Nostalgia/Lemmings STF

    LORES DE ON: if mode/frequency isn't right at the timing, the line won't start.
    This can be done by shift or sync mode switches.
    Nostalgia/Lemmings STE
    Forest
*/
    else if(CyclesIn>=ScanlineTiming[LINE_START_LIMIT_PLUS26][FREQ_IDX_71] &&
      (ShiftModeAtCycle(ScanlineTiming[LINE_START_LIMIT_PLUS26][FREQ_IDX_71])&HIRES))
      CurrentScanline.Tricks|=TRICK_0BYTE_LINE;
    else if(CyclesIn>=ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_50] 
      && FreqAtCycle(ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_50])!=PAL_HZ 
      && FreqAtCycle(ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_60])!=NTSC_HZ)
      CurrentScanline.Tricks|=TRICK_0BYTE_LINE;
  }

  if( (CurrentScanline.Tricks&TRICK_0BYTE_LINE) && !(TrickExecuted&TRICK_0BYTE_LINE))
  { 
    draw_line_off=true;
    memset(PCpal,0,sizeof(LONG)*PAL_SIZE);
    CurrentScanline.Bytes=0;
    TrickExecuted|=TRICK_0BYTE_LINE;
  }

  ////////////////////////////////////////
  //  LEFT BORDER OFF (line +26, +20)   //
  ////////////////////////////////////////

/*  To remove the left border, the program sets bit 1 of shift mode so that the 
    GLU thinks that it's a high resolution line starting.
*/

  if(!(TrickExecuted&(TRICK_LINE_PLUS_20|TRICK_LINE_PLUS_26|TRICK_0BYTE_LINE)))
  {
    if(!(CurrentScanline.Tricks&(TRICK_LINE_PLUS_20|TRICK_LINE_PLUS_26|TRICK_LINE_PLUS_24)))
    {
      r2cycle=PreviousChangeToHi(ScanlineTiming[LINE_START_LIMIT_PLUS2][FREQ_IDX_71]);
      if(r2cycle>=ScanlineTiming[LINE_START_LIMIT_MINUS12][FREQ_IDX_71]
        && r2cycle!=-1 && r2cycle<=ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_71])
      {
        r0cycle=NextChangeToLo(r2cycle); // 0 or 1
        if(r0cycle>ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_71] 
          && r0cycle<=ScanlineTiming[LINE_START_LIMIT_PLUS26][FREQ_IDX_71])
        {
/*  Only on the STE, it is possible to create stable long overscan lines (no 
    left, no right border) without a "Shifter reset" switch, aka stabiliser.
    Those shift mode switches for left border removal produce a 20 bytes bonus
    instead of 26, and the total overscan line is 224 bytes instead of 230. 
    224/8=28,no rest => no Shifter confusion.
    The cycle of going high res is 512, which is also the HSYNC cycle, this is
    possible because the HSYNC decision happens one half-cycle before the 
    register change! In this high level emulation that runs at 8MHz, we're pragmatic.
    Test cases: MOLZ, Riverside, EPSS, Hard as Ice, We Were...
    The "DE" decision is made earlier on the STE than on the STF because of possible HSCROLL.
    If there's no HSCROLL, or the STE is in medium resolution, Shifter prefetch is delayed,
    by using a chain of flips flops.
    By 4 cycles for no HSCROLL in high res, by 8 cycles for HSCROLL in med res, by 16 cycles
    for no HSCROLL in low or med res.
    If you switch to low res after the DE decision has been made in high res (emulator cycle 10),
    but before the delay for high res is finished (emulator cycle 14), the GLUE keeps on
    delaying for 16 cycles.
    This is the real explanation for the line +20 trick at emulator cycle 12.
    If there is HSCROLL, we use the same flag but it's a different effect: the line starts
    as HIRES and the Shifter scrolls as for a LORES line.
*/
          if(IS_STE && (r0cycle==ScanlineTiming[LINE_PLUS_20A][FREQ_IDX_50]
            || hscroll && r0cycle==ScanlineTiming[LINE_PLUS_20B][FREQ_IDX_50]))
            CurrentScanline.Tricks|=TRICK_LINE_PLUS_20;
          else
            CurrentScanline.Tricks|=TRICK_LINE_PLUS_26;
        }
      }
    }

    if(CurrentScanline.Tricks&(TRICK_LINE_PLUS_26|TRICK_LINE_PLUS_20))
    {
      if(CurrentScanline.Tricks&TRICK_LINE_PLUS_20)
      {
        CurrentScanline.Bytes+=20;
#if defined(SSE_VID_BORDERS)
        if(SideBorderSize==VERY_LARGE_BORDER_SIDE)
          left_border=12; // there's still some significant left border! (6 byte difference = 12 pixels)
        else
#endif
        {
          left_border=0;
          shifter_tick8+=8;
        }
        TrickExecuted|=TRICK_LINE_PLUS_20;
        if(hscroll)
        {
          CurrentScanline.StartCycle=ScanlineTiming[LINE_PLUS_20C][FREQ_IDX_50];
          CurrentScanline.Bytes-=Mmu.ExtraBytesForHscroll; //-2
          Mmu.ExtraBytesForHscroll=8;
          CurrentScanline.Bytes+=Mmu.ExtraBytesForHscroll; //+8
        }
        else
          CurrentScanline.StartCycle=ScanlineTiming[LINE_PLUS_20D][FREQ_IDX_50];
      }
      else 
      {
/*  A 'left off' grants 26 more bytes, that is 52 pixels (in low res) from cycle
    14 to 66 at 50hz. Confirmed on real STE: Overscan Demos F6
    There's no "shift" hiding the first 4 pixels but the shift is necessary for
    Steem in 384 x 270 display mode: border = 32 pixels instead of 52.
    16 pixels skipped by manipulating video counter, 4 more to skip (low res).
*/
        CurrentScanline.Bytes+=26;
#if defined(SSE_VID_BORDERS)
        if(SideBorderSize!=VERY_LARGE_BORDER_SIDE)
#endif
        {
          shifter_tick8+=4;
          SHIFT_SDP(8);
        }
        TrickExecuted|=TRICK_LINE_PLUS_26;
        left_border=0;
        CurrentScanline.StartCycle=ScanlineTiming[DE_ON+hscroll][FREQ_IDX_71];
        if(HSCROLL && !hscroll)
          SHIFT_SDP(-8); // Hard as Ice

        // additional shifts for left off
        // explained by the late timing of R0
        /////////////////////////////////////////////////////////
        if(hscroll && r0cycle==ScanlineTiming[LINE_PLUS_26A][FREQ_IDX_50])
        { // Big Wobble, D4/Tekila
          SHIFT_SDP(4); // 2 words lost in the Shifter
#if defined(SSE_VID_BORDERS)
          if(SideBorderSize==VERY_LARGE_BORDER_SIDE)
            left_border=16; // quite a border caused by this technique
#endif
        }
        else if(r0cycle==ScanlineTiming[LINE_PLUS_26B][FREQ_IDX_50]) 
        { // Closure STE, DOLB, Kryos, Xmas 2004
          SHIFT_SDP(4); // 2 words lost in the Shifter (DOLB: frame starts with 1 word in the Shifter)
#if defined(SSE_VID_BORDERS)
          if(SideBorderSize==VERY_LARGE_BORDER_SIDE)
            left_border=10; // there's some border caused by this technique
          else
#endif
          {
            SHIFT_SDP(-8);
            shifter_tick8+=8;
          }
          if(hscroll)
            SHIFT_SDP(-6); // display starts earlier
        }
        else if(Mmu.WS[OPTION_WS]==2 && IS_STF 
          && r0cycle==ScanlineTiming[LINE_PLUS_26C][FREQ_IDX_50]) 
        { // Closure STF WS2, Omega WS2
          SHIFT_SDP(6); // 3 words lost in the Shifter
#if defined(SSE_VID_BORDERS)
          if(SideBorderSize==VERY_LARGE_BORDER_SIDE) 
            left_border=12; // real border length depends on wake state
          else
#endif
          {
            SHIFT_SDP(-8);
            shifter_tick8+=5;
          }
        }
      }//+20 or +26
    }
  }

  ////////////////
  // BLACK LINE //
  ////////////////

/*  A sync switch at cycle 34 keeps HBLANK asserted for this line.
    Video memory is fetched, but black pixels are displayed.
    This is handy to hide ugly effects of tricks in "sync lines".
    A mode switch should work too but it's less useful.

    HBLANK OFF 60hz 36 50hz 40

    ljbk table
    switch to 60: 26-28 [WU1,3] 28-30 [WU2,4]
    switch back to 50: 38-...[WU1,3] 40-...[WU2,4]
*/
  if(!draw_line_off)
  {
    if(CyclesIn>=ScanlineTiming[HBLANK_OFF][FREQ_IDX_50] 
      && FreqAtCycle(ScanlineTiming[HBLANK_OFF][FREQ_IDX_60])!=NTSC_HZ
      && FreqAtCycle(ScanlineTiming[HBLANK_OFF][FREQ_IDX_50])!=PAL_HZ)
      CurrentScanline.Tricks|=TRICK_BLACK_LINE;

    if(CurrentScanline.Tricks&TRICK_BLACK_LINE)
    {
      //ASSERT( !(CurrentScanline.Tricks&TRICK_0BYTE_LINE) );
      //TRACE_LOG("%d BLK\n",scan_y);
      draw_line_off=true;
      memset(PCpal,0,sizeof(LONG)*PAL_SIZE); // all colours black
    }
  }

  //////////////////////
  // MED RES OVERSCAN //
  //////////////////////

/*  Overscan (230byte lines) is possible in medium resolution too.
    There can be a plane shift if resolution is changed from 2 to 0 then
    1, according to cycles run in low resolution.
    No Cooper Greetings, 20 cycles, lines 183, 200 36 cycles, shift=2
    Dragonnels/reset, 16 cycles
    PYM/Best Part of Creation, 28 cycles
*/
  if(!left_border && !(TrickExecuted&(TRICK_OVERSCAN_MED_RES|TRICK_0BYTE_LINE)))
  {
    r1cycle=CycleOfLastChangeToShiftMode(1);
    if(r1cycle>ScanlineTiming[MEDRES_OA][FREQ_IDX_50] && r1cycle<=ScanlineTiming[MEDRES_OB][FREQ_IDX_50])
    {
      r0cycle=PreviousShiftModeChange(r1cycle);
      if(r0cycle!=-1 && !ShiftModeChangeAtCycle(r0cycle))
      {
        CurrentScanline.Tricks|=TRICK_OVERSCAN_MED_RES;
        TrickExecuted|=TRICK_OVERSCAN_MED_RES;
        int cycles_in_low_res=(r1cycle-r0cycle)/TICKS8;
        SHIFT_SDP(-(((cycles_in_low_res)/2)%8)/2);
      }
    }
  }

  /////////////////////
  // 4BIT HARDSCROLL //
  /////////////////////

/*  When the left border is removed, a MED/LO switch causes the Shifter to
    shift the line by a number of bytes and pixels dependent on the cycles
    at which the switch occurs. 
    PYM/Let's Do The Twist Again
    D4/NGC
    D4/Nightmare
    By convenience we also do Closure WS1,3,4 here
*/
  if(!left_border && !(CurrentScanline.Tricks&(TRICK_0BYTE_LINE|TRICK_4BIT_SCROLL)))
  {
    r1cycle=CycleOfLastChangeToShiftMode(1);
    if(r1cycle>=ScanlineTiming[MEDRES_OC][FREQ_IDX_50] 
      && r1cycle<=ScanlineTiming[MEDRES_OB][FREQ_IDX_50])
    {
      r0cycle=NextShiftModeChange(r1cycle);
      if(r0cycle>r1cycle && r0cycle<=ScanlineTiming[MEDRES_OB][FREQ_IDX_50] 
        && !ShiftModeChangeAtCycle(r0cycle))
      {
        char cycles_in_med_res=(char)((r0cycle-r1cycle)/TICKS8);
        char cycles_in_low_res=0;
        r0cycle=PreviousShiftModeChange(r1cycle);
        if(r0cycle>=0 && !ShiftModeChangeAtCycle(r0cycle))
          cycles_in_low_res=(char)((r1cycle-r0cycle)/TICKS8);
        char shift_in_bytes=8-cycles_in_med_res/2+cycles_in_low_res/4;
        if(IS_STF && Mmu.WS[OPTION_WS]!=2 && r1cycle==ScanlineTiming[MEDRES_OC][FREQ_IDX_50])
        {
          shift_in_bytes+=2;
#if defined(SSE_VID_BORDERS)
          if(SideBorderSize==VERY_LARGE_BORDER_SIDE)
            left_border=13; // the technique leaves some significant border
          else
#endif
          {
            SHIFT_SDP(-8);
            shifter_tick8+=4;
          }
        }
        if(IS_STF || cycles_in_low_res)
          CurrentScanline.Tricks|=TRICK_4BIT_SCROLL;
        TrickExecuted|=TRICK_4BIT_SCROLL;
        SHIFT_SDP(shift_in_bytes);
        // the numbers came from Hatari, maybe from demo author?
        if(r1cycle==ScanlineTiming[LINE_PLUS_26A][FREQ_IDX_50]
          || r1cycle==ScanlineTiming[LINE_PLUS_26C][FREQ_IDX_50])
          Shifter.HblPixelShift=13+8-cycles_in_med_res-8; // -7,-3,1, 5, done in Render()
      }
    }
  }

  /////////////////
  // LINE +4, +6 //
  /////////////////

  // don't know any case, we leave that bit just in case someone would want to use this
  if(IS_STE && !hscroll 
    && CyclesIn>ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_60] 
    && !(TrickExecuted&(TRICK_0BYTE_LINE | TRICK_LINE_PLUS_26
    | TRICK_LINE_PLUS_20 | TRICK_4BIT_SCROLL | TRICK_OVERSCAN_MED_RES
    | TRICK_LINE_PLUS_4 | TRICK_LINE_PLUS_6)))
  {
    t=FreqAtCycle(ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_60]==PAL_HZ)
      ? (ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_50]+4*TICKS8) 
      : ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_50];
    if(ShiftModeChangeAtCycle(t)==HIRES)
    {
      CurrentScanline.Tricks|=TRICK_LINE_PLUS_6;
      left_border-=2*6; 
      CurrentScanline.Bytes+=6;
      TrickExecuted|=TRICK_LINE_PLUS_6;
    }
    else if(ShiftModeChangeAtCycle(t+4*TICKS8)==HIRES)
    {
      CurrentScanline.Tricks|=TRICK_LINE_PLUS_4;
      left_border-=2*4; 
      CurrentScanline.Bytes+=4;
      TrickExecuted|=TRICK_LINE_PLUS_4;
    }
  }

  /////////////
  // LINE +2 //
  /////////////

/*  A line that starts as a 60hz line and ends as a 50hz line gains 2 bytes 
    because 60hz lines start and stop 4 cycles before 50hz lines.
    This is used in some demos, but most cases are accidents, especially
    on the STE: the GLU checks frequency earlier because of possible horizontal
    scrolling, and this may interfere with the trick that removes top or bottom
    border.

    Forest, Beeshift, LoSTE screens, Closure, Mindbomb/No Shit
    STE: BIG Demo #1, Decade menu, nordlicht stniccc 2015, NPG/World of Music
*/
  if(!(CurrentScanline.Tricks&
    (TRICK_0BYTE_LINE|TRICK_LINE_PLUS_2|TRICK_LINE_PLUS_4|TRICK_LINE_PLUS_6
      |TRICK_LINE_PLUS_20|TRICK_LINE_PLUS_26)))
  {
    if(CyclesIn>=ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_60] 
      && FreqAtCycle(ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_60])==NTSC_HZ 
      && (CyclesIn<ScanlineTiming[LINE_STOP_LIMIT][FREQ_IDX_60] && VideoFreq==PAL_HZ
      || CyclesIn>=ScanlineTiming[LINE_STOP_LIMIT][FREQ_IDX_60] 
      && FreqAtCycle(ScanlineTiming[LINE_STOP_LIMIT][FREQ_IDX_60])==PAL_HZ))
      CurrentScanline.Tricks|=TRICK_LINE_PLUS_2;
  }
  if((CurrentScanline.Tricks&TRICK_LINE_PLUS_2) && !(TrickExecuted&TRICK_LINE_PLUS_2))
  {
    CurrentScanline.Bytes+=2;
#if defined(SSE_SHIFTER_UNSTABLE)
 /* In NPG_WOM, there's an accidental +2 on STE, but the scroll flicker isn't
    ugly.
    It's because those 2 bytes unbalance the Shifter by one word, and the screen
    gets shifted, also next frame, and the graphic above the scroller is ugly.
    It also fixes nordlicht_stniccc2015_partyversion.
    update: on real STE, flicker depends on some unidentified state
 */
    if(OPTION_UNSTABLE_SHIFTER && 
      (DPEEK(0x111a6)==0x07f0   // nordlicht_stniccc2015_partyversion
      ||DPEEK(0xeea6)==0x22d8)) // NPG_WOM
    {
      Shifter.Preload=1;
    }
#endif
    TrickExecuted|=TRICK_LINE_PLUS_2;
  }

  ///////////////
  // LINE -106 //
  ///////////////

/*  A shift mode switch to 2 before cycle 174 (end of HIRES line) causes 
    the line to stop there. 106 bytes of the lores line are not fetched.
    
    ljbk table
    R2 64 [...] -> 172 [WS1] 174 [WS3,4] 176 [WS2]
*/
  if(!(CurrentScanline.Tricks&(TRICK_LINE_MINUS_106|TRICK_0BYTE_LINE))
    && !((CurrentScanline.Tricks&TRICK_80BYTE_LINE)&&(ShiftMode&HIRES))
    && CyclesIn>=ScanlineTiming[DE_OFF][FREQ_IDX_71]
    && (ShiftModeAtCycle(ScanlineTiming[LINE_STOP_LIMIT][FREQ_IDX_71])&HIRES))
     CurrentScanline.Tricks|=TRICK_LINE_MINUS_106;
  if((CurrentScanline.Tricks&TRICK_LINE_MINUS_106) && !(TrickExecuted&TRICK_LINE_MINUS_106))
  {
    TrickExecuted|=TRICK_LINE_MINUS_106;
    if((CurrentScanline.Tricks&TRICK_80BYTE_LINE) && !(ShiftMode&HIRES))
    {
      CurrentScanline.Bytes+=SCANLINE_BYTES_HIGH;
      CurrentScanline.Tricks&=~TRICK_80BYTE_LINE;
    }
    CurrentScanline.Bytes-=106;
/*  The MMU won't fetch anything more, and as long as the ST is in high res,
    the scanline is black, but Steem renders the full scanline in colour.
    It's in fact data of next scanline.
*/
    draw_line_off=true;
    memset(PCpal,0,sizeof(LONG)*PAL_SIZE); // all colours black
  }

  /////////////////////
  // DESTABILISATION //
  /////////////////////

/*  Detect MED/LO and HI/LO switches during DE.
    Note we have 1-4 words in the Shifter, and there's no restabilising at the
    start of next scanline.
    In fact, this is a real Shifter trick and ideally would be in shifter.cpp!
*/
/*
ljbk:
detect unstable: switch MED/LOW - Beeshift
- 3 (screen shifted by 12 pixels because only 1 word will be read before the 4 are available to draw the bitmap);
- 2 (screen shifted by 8 pixels because only 2 words will be read before the 4 are available to draw the bitmap);
- 1 (screen shifted by 4 pixels because only 3 words will be read before the 4 are available to draw the bitmap);
- 0 (screen shifted by 0 pixels because the 4 words will be read to draw the bitmap);
*/
#if defined(SSE_SHIFTER_UNSTABLE)
  if(OPTION_UNSTABLE_SHIFTER && !(CurrentScanline.Tricks &(TRICK_UNSTABLE
    |TRICK_0BYTE_LINE|TRICK_LINE_PLUS_26|TRICK_LINE_MINUS_106|TRICK_LINE_MINUS_2
    |TRICK_LINE_PLUS_2|TRICK_4BIT_SCROLL))&& bFetchingLine) 
  {
    int mode;
    r1cycle=NextShiftModeChange(ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_50]); 
    if(r1cycle>ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_50]
      && r1cycle<ScanlineTiming[LINE_STOP_LIMIT][FREQ_IDX_50]
      && ((r1cycle-ScanlineTiming[DESTAB_A][FREQ_IDX_50])/TICKS8)%8==0 //generic, dangerous! (84+, 204+)
    && (mode=ShiftModeChangeAtCycle(r1cycle))!=0) // we know it's not -1
    {
      r0cycle=NextShiftModeChange(r1cycle,0);
      int cycles_in_med_or_high=(r0cycle-r1cycle)/TICKS8;
      if(r0cycle<=ScanlineTiming[LINE_STOP_LIMIT][FREQ_IDX_50] && cycles_in_med_or_high>0)
      {
        Shifter.Preload=((cycles_in_med_or_high/4)%4);
        if((mode&HIRES)&&Shifter.Preload&1)
          Shifter.Preload+=(4-Shifter.Preload)*2; // there's a low-level explanation
        CurrentScanline.Tricks|=TRICK_UNSTABLE;
      }
    }
  }

  //  Shift due to unstable Shifter, apply effect
  if(OPTION_UNSTABLE_SHIFTER && Shifter.Preload 
    && CyclesIn>ScanlineTiming[DE_ON][FREQ_IDX_50] // arbitrary, the idea is after left off check
    && !(TrickExecuted&TRICK_UNSTABLE) && !(CurrentScanline.Tricks
    &(TRICK_0BYTE_LINE|TRICK_LINE_PLUS_2|TRICK_LINE_PLUS_26))
    // if only 1 word it gets restabilised in WS1!
    && !((Shifter.Preload%4)==1&&IS_STF&&Mmu.WS[OPTION_WS]==1)) 
  {
    // 1. planes
    int shift_sdp=-((Shifter.Preload)%4)*2;
    SHIFT_SDP(shift_sdp);
    // 2. pixels
    // Beeshift, the full frame shifts in the border
    // Dragon: shifts -4, just like the 230 byte lines below
    if(left_border)
    {
      left_border-=(Shifter.Preload%4)*4;
      right_border+=(Shifter.Preload%4)*4;
    }
    //TRACE_LOG("Y%d Preload %d shift SDP %d pixels %d lb %d rb %d\n",scan_y,Preload,shift_sdp,HblPixelShift,left_border,right_border);
    TrickExecuted|=TRICK_UNSTABLE;
  }
#endif

  /////////////
  // LINE -2 //
  /////////////
  
/*  DE ends 4 cycles before normal because freq has been set to 60hz:
    2 bytes fewer are fetched from video memory.
    Thresholds/WU states (from table by ljbk)

      60hz  66 - 380 WU1,3
            68 - 382 WU2,4

      50hz  382 -... WU1,3
            384 -... WU2,4
*/
  if (!(CurrentScanline.Tricks&(TRICK_0BYTE_LINE | TRICK_LINE_MINUS_106 | TRICK_LINE_MINUS_2))
    && CyclesIn > ScanlineTiming[LINE_STOP_LIMIT][FREQ_IDX_60]
    && FreqAtCycle(ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_60])!=Freq[FREQ_IDX_60]
    && FreqAtCycle(ScanlineTiming[LINE_STOP_LIMIT][FREQ_IDX_60])==Freq[FREQ_IDX_60])
     CurrentScanline.Tricks|=TRICK_LINE_MINUS_2;
   
  if((CurrentScanline.Tricks&TRICK_LINE_MINUS_2) && !(TrickExecuted&TRICK_LINE_MINUS_2))
  {
    CurrentScanline.Bytes-=2; 
    TrickExecuted|=TRICK_LINE_MINUS_2;
//    TRACE_LOG("-2 y %d c %d s %d e %d ea %d\n",scan_y,LINECYCLES,scanline_drawn_so_far,overscan_add_extra,ExtraAdded);
  }

  /////////////////////////////////
  // RIGHT BORDER OFF (line +44) // 
  /////////////////////////////////

/*  A sync switch to 0 (60hz) at cycle 384 (end of display for 50hz)
    makes the GLUE fail to stop the line (DE still on).
    DE will stop only at cycle of HSYNC, 472.
    This is 88 cycles later and the reason why the trick grants 44 more
    bytes of video memory for the scanline.

    Because a 60hz line stops at cycle 380, the sync switch must hit just
    after that and right before the test for end of 50hz line occurs.
    That's why cycle 384 is targeted, but according to wake-up state other
    timings may work.
    Obviously, the need to hit the GLU/Shifter registers at precise cycles
    on every useful scanline was impractical.

    WS thresholds (from table by ljbk) 

    Switch to 60hz  382 - 384 WS1,3
                    384 - 386 WS2,4 Nostalgia menu
*/
/*  We used the following to calibrate (Beeshift3).

LJBK:
So going back to the tests, the switchs are done at the places indicated:
-71/50 at 295/305;
-71/50 at 297/305;
-60/50 at 295/305;
and the MMU counter at $FFFF8209.w is read at the end of that line.
It can be either $CC: 204 or $A0: 160.
The combination of the three results is then tested:
WS4: CC A0 CC
WS3: CC A0 A0
WS2: CC CC CC Right Border is always open
WS1: A0 A0 A0 no case where Right Border was open

Emulators_cycle = (cycle_Paulo + 83) mod 512

WS                           1         2         3         4
 
-71/50 at 386/396            N         Y         Y         Y
-71/50 at 388/396            N         Y         N         N
-60/50 at 386/396            N         Y         N         Y

Tests are arranged to be efficient.
*/
  if(!(CurrentScanline.Tricks&(TRICK_0BYTE_LINE|TRICK_LINE_MINUS_2|
    TRICK_LINE_MINUS_106|TRICK_LINE_PLUS_44)))
  {
    bool dont_test=(CyclesIn<ScanlineTiming[LINE_STOP_LIMIT_MINUS2][FREQ_IDX_50])
      || (FreqAtCycle(ScanlineTiming[LINE_STOP_LIMIT][FREQ_IDX_60])==NTSC_HZ);
    if(!dont_test)
    {
      t=ScanlineTiming[LINE_PLUS_44_R][FREQ_IDX_50];
      if((!(SyncMode&SYNCPAL)
        &&(CyclesIn>ScanlineTiming[LINE_STOP_LIMIT][FREQ_IDX_60])
        &&(CyclesIn<=ScanlineTiming[LINE_STOP_LIMIT][FREQ_IDX_50]))
        ||((CyclesIn>=ScanlineTiming[LINE_STOP_LIMIT][FREQ_IDX_50])
        &&FreqAtCycle(ScanlineTiming[LINE_STOP_LIMIT][FREQ_IDX_50])==NTSC_HZ)
        ||CyclesIn>=t&&(ShiftModeAtCycle(t)&HIRES))
      {
        CurrentScanline.Tricks|=TRICK_LINE_PLUS_44;
      }
      if((CurrentScanline.Tricks&TRICK_LINE_PLUS_44) && !(TrickExecuted&TRICK_LINE_PLUS_44))
      {
        right_border=0;
        TrickExecuted|=TRICK_LINE_PLUS_44;
        CurrentScanline.Bytes+=44;
        CurrentScanline.EndCycle=ScanlineTiming[HSYNC_ON][FREQ_IDX_50];
      }
    }
  }

  ////////////////
  // STABILISER //
  ////////////////

/*  A stabiliser is a HI/LO or MED/LO switch that resets the unbalanced Shifter.
    It does that by accelerating the pixel counter, which demo coders couldn't
    know at the time, it's a recent insight by ijor.
    For high-level emulation, the stabiliser isn't vital.
*/
  if(!(CurrentScanline.Tricks&TRICK_STABILISER) && CyclesIn>ScanlineTiming[STAB_A][FREQ_IDX_50])
  { 
    r2cycle=NextShiftModeChange(ScanlineTiming[STAB_A][FREQ_IDX_50]);
    if(r2cycle>ScanlineTiming[STAB_A][FREQ_IDX_50] && r2cycle<ScanlineTiming[STAB_B][FREQ_IDX_50]) 
    {
      if(!ShiftModeChangeAtCycle(r2cycle))
        r2cycle=NextShiftModeChange(r2cycle);
      if(r2cycle>-1 && r2cycle<ScanlineTiming[STAB_B][FREQ_IDX_50]
        && ShiftModeChangeAtCycle(r2cycle))
      {
        r0cycle=NextShiftModeChange(r2cycle,0);
        if(r0cycle>-1 && r0cycle<ScanlineTiming[STAB_C][FREQ_IDX_50]) 
          CurrentScanline.Tricks|=TRICK_STABILISER;
      }
      else if(CyclesIn>ScanlineTiming[STAB_D][FREQ_IDX_50]
        && ShiftModeAtCycle(ScanlineTiming[STAB_D][FREQ_IDX_50])==MEDRES)
      {
        r0cycle=NextShiftModeChange(ScanlineTiming[STAB_D][FREQ_IDX_50],0);
        if(r0cycle>-1 && r0cycle<ScanlineTiming[STAB_C][FREQ_IDX_50]) 
          CurrentScanline.Tricks|=TRICK_STABILISER;
      }
    }
  }
#if defined(SSE_SHIFTER_UNSTABLE)
  if(CurrentScanline.Tricks&TRICK_STABILISER)
    Shifter.Preload=0; // it's a simplification
#endif

  ////////////////////////
  //  NON-STOPPING LINE //
  ////////////////////////

/*  In the Enchanted Land hardware tests, a HI switch at end of display
    when the right border has been removed causes the GLUE to miss HSYNC,
    and DE stays asserted for the rest of the line (24 bytes), then the next 
    scanline, not stopping until HSYNC of that line (232 bytes).
    The result of the test is written to $204-$20D and the line +26 trick 
    timing during the game depends on it. This STF/STE distinction, which 
    was the point of the test (maybe!), is emulated in Steem SSE.

      STF 464/472 => R2 4 (wouldn't work on the STE)
    000204 : 00bd 0003
    000208 : 0059 000d
    00020c : 000d 0001

      STE 460/468 => R2 0
    000204 : 00bd 0002
    000208 : 005a 000c
    00020c : 000d 0001

    The program reads the video counter at the end of the scanline, so we can't
    delay test until next line, unlike 0byte.
*/

  if(!right_border && !(NextScanline.Tricks&TRICK_0BYTE_LINE) 
    && !(CurrentScanline.Tricks&TRICK_LINE_PLUS_24))
  {
    r2cycle=ScanlineTiming[HSYNC_ON2][FREQ_IDX_50]+(short)CurrentScanline.Cycles;
    if(CyclesIn>=r2cycle && (ShiftModeAtCycle(r2cycle)&HIRES))
    {
      TRACE_LOG("Enchanted Land HW test F%d Y%d R2 %d R0 %d\n",FRAME,scan_y,PreviousShiftModeChange(466),NextShiftModeChange(466,0));
      CurrentScanline.Bytes+=24; // "double" right off
      CurrentScanline.Tricks|=TRICK_LINE_PLUS_24; // recycle left off 60hz bit!
      // the following isn't necessary for the game
      NextScanline.Tricks|=TRICK_LINE_PLUS_26|TRICK_LINE_PLUS_44; //+2
      NextScanline.Bytes=232;
      time_of_next_timer_b+=512*TICKS8; // no timer B for this line (too late?)
    }
  }
}


/*  Top and bottom border.
    Using VideoFreqAtStartOfVbl is an approximation.
    Tests are based on existing cases. It's possible to make it more like
    the real video logic but we have an apart plugin for that.
*/
void TGlue::CheckVerticalOverscan() { // only called by event_scanline() at specific scan_y values
  short CyclesIn=LINECYCLES;
  enum{NO_LIMIT=0,LIMIT_TOP,LIMIT_MIDDLE,LIMIT_170,LIMIT_BOTTOM};
  BYTE on_overscan_limit;
  switch(scan_y) {
  case -30:
    on_overscan_limit=LIMIT_TOP;
    break;
  case -1:
    on_overscan_limit=LIMIT_MIDDLE;
    break;
  case 170: // -30+200
    on_overscan_limit=LIMIT_170;
    break;
  default: // not 199 if there are 0byte lines
    on_overscan_limit=LIMIT_BOTTOM;
    break;
  }
  int t=0;
  if(emudetect_overscans_fixed && on_overscan_limit!=LIMIT_MIDDLE) 
  {
    CurrentScanline.Tricks|= (on_overscan_limit==LIMIT_TOP) ? TRICK_TOP_OVERSCAN
                                                            : TRICK_BOTTOM_OVERSCAN;
  }
  // 50hz frame overscan
  else if(on_overscan_limit && VideoFreqAtStartOfVbl==PAL_HZ)
  {
    t=ScanlineTiming[VERT_OVSCN_LIMIT][FREQ_IDX_50];
    if(CyclesIn>=t && FreqAtCycle(t)!=PAL_HZ || CyclesIn<t && VideoFreq!=PAL_HZ)
    {
      switch(on_overscan_limit) {
      case LIMIT_TOP: // should work for 60Hz and 72Hz switches
        CurrentScanline.Tricks|=TRICK_TOP_OVERSCAN;
        break;
      case LIMIT_MIDDLE:
      /*If a program changes the frequency right before the normal frame
      starts, vertical DE won't be asserted (and there will be no timer B)
      during the frame, so it's a 0byte frame! Hard as Ice bug.*/
        if(!de_v_on)
        {
          TRACE_LOG("0byte frame 50hz\n");
          de_start_line=de_end_line+1; // no VDE this frame
        }
        break;
      case LIMIT_170:
      /*FULLAST VINNER frame starts at 50Hz, top overscan but only 200 DE lines*/
        de_end_line=GLU_NTSC_SCANLINES-1;
        video_last_draw_line=scan_y;
        break;
      case LIMIT_BOTTOM:
        CurrentScanline.Tricks|=TRICK_BOTTOM_OVERSCAN;
        break;
      }//sw
    }
#if defined(SSE_DEBUGGER_TOPOFF)
    if(!(CurrentScanline.Tricks&(TRICK_TOP_OVERSCAN|TRICK_BOTTOM_OVERSCAN))
      && freq_change_this_scanline)
    {
      if((DEBUGGER_CONTROL_MASK2&DEBUGGER_CONTROL_TOPOFF)
        &&(on_overscan_limit==LIMIT_TOP))
      {
        runstate=RUNSTATE_STOPPING;
        SET_WHY_STOP("Top off missed");
      }
      else if((DEBUGGER_CONTROL_MASK2&DEBUGGER_CONTROL_BOTTOMOFF)
        &&(on_overscan_limit==LIMIT_BOTTOM))
      {
        //BREAKPOINT(should stop);
        runstate=RUNSTATE_STOPPING;
        SET_WHY_STOP("Bottom off missed");
      }
    }
#endif
  }
  // 60hz frame overscan
  else if(on_overscan_limit!=LIMIT_TOP && VideoFreqAtStartOfVbl==NTSC_HZ)
  {
    t=ScanlineTiming[VERT_OVSCN_LIMIT][FREQ_IDX_60];
    int f=FreqAtCycle(t);
    if(CyclesIn>=t && f!=NTSC_HZ)
    {
#ifdef SSE_BETA // hypothetical...
      if(f==PAL_HZ && on_overscan_limit==LIMIT_MIDDLE && !de_v_on)
      {
        TRACE_LOG("frame 60->50Hz\n"); 
        de_start_line=GLU_PAL_TOPSCANLINES; // becomes a 50hz frame?
        de_end_line=GLU_NTSC_SCANLINES-1;
        nLines=GLU_PAL_SCANLINES;
        scan_y-=GLU_PAL_TOPSCANLINES-GLU_NTSC_TOPSCANLINES; // -=63-34
      }
      else
#endif
        CurrentScanline.Tricks|=TRICK_BOTTOM_OVERSCAN_60HZ;
    }
  }
  else if(screen_res==HIRES && on_overscan_limit==LIMIT_BOTTOM)
  {
    // NOBORDER demo, there's a line -26 and a 0 bytes line, the bottom border is open v420
    t=120*TICKS8; //ad hoc, TODO
    if(CyclesIn>=t && FreqAtCycle(t)!=MONO_HZ || CyclesIn<t && VideoFreq!=MONO_HZ)
    {
      CurrentScanline.Tricks|=TRICK_BOTTOM_OVERSCAN;
      CurrentScanline.Bytes-=26;
      NextScanline.Tricks=TRICK_0BYTE_LINE;
    }
  }
  if(CurrentScanline.Tricks&(TRICK_TOP_OVERSCAN|TRICK_BOTTOM_OVERSCAN|TRICK_BOTTOM_OVERSCAN_60HZ))
  {
    Mfp.ComputeNextTimerB();
    if(on_overscan_limit==LIMIT_TOP)
    {
      video_first_draw_line=-29;
      de_start_line=GLU_NTSC_TOPSCANLINES;
    }
    else // bottom border off
    {
      if(CurrentScanline.Tricks&TRICK_BOTTOM_OVERSCAN_60HZ)
      {
/*  At 60hz, fewer scanlines are displayed in the bottom border than 50hz due
    to vertical blank and sync (It's a girl 2 last screen).*/
        video_last_draw_line=226;
        de_end_line=259;
      }
      else if(screen_res==HIRES)
      {
        video_last_draw_line=470;
        de_end_line=500;
      }
      else
      {
        video_last_draw_line=247;
        de_end_line=309;
      }
    }
  }
#if defined(SSE_DEBUGGER_FRAME_REPORT) && defined(SSE_DEBUGGER_FAKE_IO)
  if(TRACE_LOG_ENABLED(LOGSECTION_GLUE)&&(TRACE_MASK1 & TRACE_CONTROL_VERTOVSC) && CyclesIn>=t) 
  {
    FrameEvents.ReportLine();
    TRACE_LOG("F%d y%d freq at %d %d at %d %d switch %d to %d, %d to %d, %d to %d overscan %X\n",FRAME,scan_y,t,FreqAtCycle(t),t-2*TICKS8,FreqAtCycle(t-2*TICKS8),PreviousFreqChange(PreviousFreqChange(t)),FreqChangeAtCycle(PreviousFreqChange(PreviousFreqChange(t))),PreviousFreqChange(t),FreqChangeAtCycle(PreviousFreqChange(t)),NextFreqChange(t),FreqChangeAtCycle(NextFreqChange(t)),CurrentScanline.Tricks);
  //  ASSERT( scan_y!=199|| (CurrentScanline.Tricks&TRICK_BOTTOM_OVERSCAN) );
    //ASSERT( scan_y!=199|| video_last_draw_line==247 );
  }
#endif
}


void TGlue::EndHBL() {
/*  1. Finish horizontal overscan : correct -2 & +2 effects
    Those tests are much like EndHBL() in Hatari (v1.6)
    2. Detect loaded Shifter (hack)
*/
  if((CurrentScanline.Tricks&(TRICK_LINE_PLUS_2|TRICK_LINE_PLUS_26))
    && !(CurrentScanline.Tricks&(TRICK_LINE_MINUS_2|TRICK_LINE_MINUS_106))
    && CurrentScanline.EndCycle==ScanlineTiming[DE_OFF][FREQ_IDX_60]) 
  {
    CurrentScanline.Tricks&=~TRICK_LINE_PLUS_2;
    SHIFT_SDP(-2);
    CurrentScanline.Bytes-=2;
  } 
  if(CurrentScanline.Tricks&TRICK_LINE_MINUS_2     
    && (CurrentScanline.StartCycle==ScanlineTiming[DE_ON][FREQ_IDX_60]
    || CurrentScanline.EndCycle!=ScanlineTiming[DE_OFF][FREQ_IDX_60]))
  {
    CurrentScanline.Tricks&=~TRICK_LINE_MINUS_2;
    SHIFT_SDP(2);
    CurrentScanline.Bytes+=2;
  }
#if defined(SSE_SHIFTER_UNSTABLE) // just hacks as the full story could be more complicated
  if(OPTION_UNSTABLE_SHIFTER && IS_STF) // not OPTION_HACKS, there's already OPTION_UNSTABLE_SHIFTER
  {
    if((CurrentScanline.Tricks&TRICK_LINE_PLUS_26)
      &&!(CurrentScanline.Tricks&(TRICK_STABILISER|TRICK_LINE_MINUS_2|TRICK_LINE_MINUS_106))
      && (LPEEK(8)==0x118E  // Ventura Board and Sphere + Ultimate Dist
      || LPEEK(8)==0x8B64 // Lame Trop Falcon
      || LPEEK(0x24)==0xF194)) // Overdrive/Dragon
    {
      if(!Shifter.Preload)
      {
        Shifter.Preload=1;
        CurrentScanline.Tricks|=TRICK_UNSTABLE; // so it shows in frame report
      }
    }
  }
#endif
}


bool TGlue::FetchingLine() {
  // does the current scan_y involve fetching by the MMU?
#if !defined(SSE_NO_FALCONMODE)
  if(emudetect_falcon_mode)
    return false;
#endif
  return 
#if defined(SSE_VID_STVL1)
    (OPTION_C3) ? Stvl.vde :
#endif
    // notice < video_last_draw_line, not <=
    (scan_y>=video_first_draw_line && scan_y<video_last_draw_line);
}


void TGlue::IncScanline() { // only called by event_scanline(), calls Shifter.IncScanline()

  //ASSERT(!OPTION_C3);
  Debug.ShifterTricks|=CurrentScanline.Tricks; // for frame
  if(screen_res==HIRES // being in hires is no trick
#ifndef NO_CRAZY_MONITOR
    || extended_monitor && em_planes==1 // monochrome extended monitor
#endif
    )
    Debug.ShifterTricks&=~TRICK_80BYTE_LINE;

#if defined(SSE_STATS)
  Stats.mskOverscan1=Debug.ShifterTricks;
  Stats.mskOverscan|=Debug.ShifterTricks;
#endif
#if defined(SSE_DEBUGGER_FRAME_REPORT)
  if((FRAME_REPORT_MASK2 & FRAME_REPORT_MASK_SHIFTER_TRICKS) 
    && CurrentScanline.Tricks)
    FrameEvents.Add(scan_y,(short)CurrentScanline.Cycles/TICKS8,'T',CurrentScanline.Tricks);
  if((FRAME_REPORT_MASK2 & FRAME_REPORT_MASK_SHIFTER_TRICKS_BYTES))
    FrameEvents.Add(scan_y,(short)CurrentScanline.Cycles/TICKS8,'#',CurrentScanline.Bytes);
#endif

  ASSERTB(VCount<nLines);
  if(VCount<nLines)
    VCount++;
  Shifter.IncScanline();
  PreviousScanline=CurrentScanline; // auto-generated

  de_v_on=(VCount>=de_start_line && VCount<=de_end_line);
  bFetchingLine=FetchingLine();

  CurrentScanline=NextScanline;
  if(CurrentScanline.Tricks)
  {} // don't change #bytes
  else if(de_v_on)
    //Start with 160, it's adapted in AdaptScanlineValues if necessary
    CurrentScanline.Bytes=SCANLINE_BYTES_LOMED; 
  else
    NextScanline.Bytes=0;

  Mmu.ExtraBytesForHscroll=0;
  Mmu.no_LW=false;
#if !defined(SSE_MFP_421)
  m_Status.timerb_start=m_Status.timerb_end=false;
#endif
  AdaptScanlineValues(-1);
  TrickExecuted=0;

#if defined(SSE_HARDWARE_OVERSCAN)
/*  Emulate hardware overscan scanlines of the LaceScan circuit.
    It is a hack that intercepts the 'DE' line between the GLUE and the 
    MMU. Hence only possible on the STF/Mega ST (on the STE, GLUE and
    MMU are one chip).
    Compared with software overscan, 6 more bytes are fetched at 50hz (236).
    We also emulate the AutoSwitch Overscan circuit, which uses the GLUE clock
    to time DE, so that 224 bytes are fetched at 50hz and 60hz. As it is
    divisible by 4, GEM isn't troubled. LaceScan produces 234 bytes at 60hz.
*/
  if(OPTION_HWOVERSCAN && SSEConfig.OverscanOn)
  {
    if(bFetchingLine) // there are also more fetching lines
    {
      left_border=right_border=0;
      if(COLOUR_MONITOR)
      {
        if(OPTION_HWOVERSCAN==LACESCAN)
          CurrentScanline.Bytes=(VideoFreqAtStartOfVbl==NTSC_HZ) ? 234 : 236;
        else
          CurrentScanline.Bytes=224;
#if defined(SSE_VID_BORDERS)
        if(SideBorderSize!=VERY_LARGE_BORDER_SIDE)
#endif
          SHIFT_SDP(8); // as for "left off", skip non displayed border
      }
      else
      {
        CurrentScanline.Bytes=(OPTION_HWOVERSCAN==LACESCAN) ? 100 : 96;
        TrickExecuted=CurrentScanline.Tricks=TRICK_HIRES_OVERSCAN; // needed by Shifter.DrawScanlineToEnd()
      }
    }
  }
#endif
  NextScanline.Tricks=0; // eg for 0byte lines mess
  m_Status.scanline_done=false;
  LINEWID=Mmu.linewid;
}


/*  Argh! those horrible functions still there.
    An attempt at replacing them with a table proved less efficient anyway
    (see R419), so we should try to optimise them instead... 
    TODO: could make another attempt one day...
*/

void TGlue::AddFreqChange(BYTE const f) {
  // Replacing macro ADD_SHIFTER_FREQ_CHANGE(VideoFreq)
  GlueFreqChangeIdx++;
  GlueFreqChangeIdx&=(NMODECHANGES-1);
  GlueFreqChangeTime[GlueFreqChangeIdx]=a_s_t;
  glue_freq_change[GlueFreqChangeIdx]=f;                    
}


void TGlue::AddShiftModeChange(BYTE const mode) {
  // called only by SetShiftMode
  ShifterModeChangeIdx++;
  ShifterModeChangeIdx&=(NMODECHANGES-1);
  ShifterModeChangeTime[ShifterModeChangeIdx]=a_s_t;
  shifter_mode_change[ShifterModeChangeIdx]=mode;                    
}


int TGlue::FreqChangeAtCycle(int const cycle) {
  // if there was a change at this cycle, return it, otherwise -1
  COUNTER_VAR t=cycle+LINECYCLE0; // convert to absolute
  int i,j;
  // loop while it's later than cycle, with safety
  for(i=GlueFreqChangeIdx,j=0;
    j<NMODECHANGES && GlueFreqChangeTime[i]-t>0;
    j++,i--,i&=(NMODECHANGES-1))
  {}
  // here, we're on the right cycle, or before
  int rv=(j<NMODECHANGES && !(GlueFreqChangeTime[i]-t)) ? glue_freq_change[i] : -1;
  return rv;
}


int TGlue::ShiftModeChangeAtCycle(int const cycle) {
  // if there was a change at this cycle, return it, otherwise -1
  COUNTER_VAR t=cycle+LINECYCLE0; // convert to absolute
  int i,j;
  // loop while it's later than cycle, with safety
  for(i=ShifterModeChangeIdx,j=0;
    j<NMODECHANGES && ShifterModeChangeTime[i]-t>0;
    j++,i--,i&=(NMODECHANGES-1))
  {}
  // here, we're on the right cycle, or before
  int rv=(j<NMODECHANGES && !(ShifterModeChangeTime[i]-t)) ? shifter_mode_change[i] : -1;
  return rv;
}


int TGlue::FreqAtCycle(int const cycle) {
  //ASSERT(cycle<=LINECYCLES);
  COUNTER_VAR t=cycle+LINECYCLE0; // convert to absolute
  int i,j;
  for(i=GlueFreqChangeIdx,j=0;
    GlueFreqChangeTime[i]-t>0 && j<NMODECHANGES;
    i--,i&=(NMODECHANGES-1),j++)
  {}
  if(GlueFreqChangeTime[i]-t<=0 && glue_freq_change[i]>0)
    return glue_freq_change[i];
  return VideoFreqAtStartOfVbl;
}


int TGlue::ShiftModeAtCycle(int const cycle) {
  // what was the shift mode at this cycle?
  COUNTER_VAR t=cycle+LINECYCLE0; // convert to absolute
  int i,j;
  for(i=ShifterModeChangeIdx,j=0;
    ShifterModeChangeTime[i]-t>0 && j<NMODECHANGES;
    i--,i&=(NMODECHANGES-1),j++)
  {}
  if(ShifterModeChangeTime[i]-t<=0)
    return shifter_mode_change[i];
  return ShiftMode; // we don't have at_start_of_vbl
}


#ifdef SSE_DEBUG

int TGlue::NextFreqChange(int cycle,int value) {
  // return cycle of next change after this cycle
  COUNTER_VAR t=cycle+LINECYCLE0; // convert to absolute
  int idx,i,j;
  for(idx=-1,i=GlueFreqChangeIdx,j=0;
    GlueFreqChangeTime[i]-t>0 && j<NMODECHANGES;
    i--,i&=(NMODECHANGES-1),j++)
    if(value==-1 || glue_freq_change[i]==value)
      idx=i;
  if(idx!=-1 && GlueFreqChangeTime[idx]-t>0)
    return (int)(GlueFreqChangeTime[idx]-LINECYCLE0);
  return -1;
}

#endif


short TGlue::NextShiftModeChange(int const cycle,int const value) {
  // return cycle of next change after this cycle
  // if value=-1, return any change
  // if none is found, return -1
  //ASSERT(value>=-1 && value <=2);
  COUNTER_VAR t=cycle+LINECYCLE0; // convert to absolute
  short i,j,rv=-1;
  // we start from now, go back in time
  for(i=ShifterModeChangeIdx,j=0; j<NMODECHANGES; i--,i&=(NMODECHANGES-1),j++)
  {
    COUNTER_VAR a=ShifterModeChangeTime[i]-t;
    if(a>0 && a<1024) // as long as it's valid, it's better...
    {
      if(value==-1 || shifter_mode_change[i]==value)
        rv=(short)(ShifterModeChangeTime[i]-LINECYCLE0); // in linecycles
    }
    else
      break; // as soon as it's not valid, we're done
  }
  return rv;
}


short TGlue::NextChangeToHi(int const cycle) {
  // return cycle of next change after this cycle
  // if none is found, return -1

  COUNTER_VAR t=cycle+LINECYCLE0; // convert to absolute
  short i,j,rv=-1;
  // we start from now, go back in time
  for(i=ShifterModeChangeIdx,j=0; j<NMODECHANGES; i--,i&=(NMODECHANGES-1),j++)
  {
    COUNTER_VAR a=ShifterModeChangeTime[i]-t;
    if(a>0 && a<1024) // as long as it's valid, it's better...
    {
      if(shifter_mode_change[i]&HIRES) //HI
        rv=(short)(ShifterModeChangeTime[i]-LINECYCLE0); // in linecycles
    }
    else
      break; // as soon as it's not valid, we're done
  }
  return rv;
}


short TGlue::NextChangeToLo(int const cycle) {
  COUNTER_VAR t=cycle+LINECYCLE0; // convert to absolute
  short i,j,rv=-1;
  // we start from now, go back in time
  for(i=ShifterModeChangeIdx,j=0; j<NMODECHANGES; i--,i&=(NMODECHANGES-1),j++)
  {
    COUNTER_VAR a=ShifterModeChangeTime[i]-t;
    if(a>0 && a<1024) // as long as it's valid, it's better...
    {
      if(!(shifter_mode_change[i]&HIRES)) //not HI (also MED)
        rv=(short)(ShifterModeChangeTime[i]-LINECYCLE0); // in linecycles
    }
    else
      break; // as soon as it's not valid, we're done
  }
  return rv;
}


short TGlue::PreviousChangeToHi(int const cycle) {
  COUNTER_VAR t=cycle+LINECYCLE0; // convert to absolute
  short idx,i,j;
  for(idx=-1,i=ShifterModeChangeIdx,j=0;idx==-1 && j<NMODECHANGES; i--,i&=(NMODECHANGES-1),j++)
    if(ShifterModeChangeTime[i]-t<0&&(shifter_mode_change[i]&HIRES))
      idx=i;
  if(idx!=-1)
    idx=(short)(ShifterModeChangeTime[idx]-LINECYCLE0);
  return idx;
}


short TGlue::PreviousChangeToLo(int const cycle) {
  COUNTER_VAR t=cycle+LINECYCLE0; // convert to absolute
  short idx,i,j;
  for(idx=-1,i=ShifterModeChangeIdx,j=0;idx==-1 && j<NMODECHANGES; i--,i&=(NMODECHANGES-1),j++)
    if(ShifterModeChangeTime[i]-t<0&&!(shifter_mode_change[i]&HIRES))
      idx=i;
  if(idx!=-1)
    idx=(short)(ShifterModeChangeTime[idx]-LINECYCLE0);
  return idx;
}


#if defined(SSE_DEBUG) 

int TGlue::PreviousFreqChange(int const cycle) {
  // return cycle of previous change before this cycle
  COUNTER_VAR t=cycle+LINECYCLE0; // convert to absolute
  int i,j;
  for(i=GlueFreqChangeIdx,j=0;
    GlueFreqChangeTime[i]-t>=0 && j<NMODECHANGES;
    i--,i&=(NMODECHANGES-1),j++)
  {}
  if(GlueFreqChangeTime[i]-t<0)
    return (int)(GlueFreqChangeTime[i]-LINECYCLE0);
  return -1;
}

#endif


short TGlue::PreviousShiftModeChange(int const cycle) {
  // return cycle of previous change before this cycle
  COUNTER_VAR t=cycle+LINECYCLE0; // convert to absolute
  int i,j;
  for(i=ShifterModeChangeIdx,j=0;
    ShifterModeChangeTime[i]-t>=0 && j<NMODECHANGES;
    i--,i&=(NMODECHANGES-1),j++)
  {}
  if(ShifterModeChangeTime[i]-t<0)
    return (short)(ShifterModeChangeTime[i]-LINECYCLE0);
  return -1;
}


short TGlue::CycleOfLastChangeToShiftMode(int const value) {
  int i,j;
  for(i=ShifterModeChangeIdx,j=0;
    shifter_mode_change[i]!=value && j<NMODECHANGES && (ShifterModeChangeTime[i] - LINECYCLE0)>0;
    i--,i&=(NMODECHANGES-1),j++)
  {}
  if(shifter_mode_change[i]==value)
    return (short)(ShifterModeChangeTime[i]-LINECYCLE0);
  return -1;
}


void TGlue::Update() {
/*  Update GLU timings according to ST model and wakeup state (for STF). We do it when
    player changes options, not at each scanline.
    v4: The new CPU interrupt model shifts some timings, including the HBL one
    (triggered at HSYNC OFF), which is annoying as this is our base for
    'emulator cycles' and would better stay 0. 
    So we shifted other timings by 8 instead: old emulator cycle 56 is now 64, 376 is 384 etc.
*/
  char HblShift=GLU_HBL_SHIFT*TICKS8;
  if(Mmu.WS[OPTION_WS]==1 && IS_STF)       
    HblShift+=4*TICKS8; // HBITMG.TOS
  char WuModRes=Mmu.ResMod[IS_STE?3:OPTION_WS]*TICKS8; //-2, 0, 2, STE always 0
  char WuModSync=Mmu.FreqMod[IS_STE?3:OPTION_WS]*TICKS8; // 0 or 2, STE always 0
  // DE (Display Enable)
  ScanlineTiming[DE_ON][FREQ_IDX_71]=GLU_DE_ON_MONO*TICKS8+HblShift;
  ScanlineTiming[DE_ON][FREQ_IDX_60]=GLU_DE_ON_NTSC*TICKS8+HblShift;
  ScanlineTiming[DE_ON][FREQ_IDX_50]=GLU_DE_ON_PAL*TICKS8+HblShift;
  // with WU effects -  the GLUE tests shift mode for freq 71, sync mode for other freqs
  ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_71]=ScanlineTiming[DE_ON][FREQ_IDX_71]+WuModRes;
  ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_60]=ScanlineTiming[DE_ON][FREQ_IDX_60]+WuModSync; 
  ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_50]=ScanlineTiming[DE_ON][FREQ_IDX_50]+WuModSync;
  for(int f=0;f<NFREQS;f++)
  {
    ScanlineTiming[DE_OFF][f]=ScanlineTiming[DE_ON][f]+DE_cycles[f];
    ScanlineTiming[LINE_STOP_LIMIT][f]=ScanlineTiming[LINE_START_LIMIT][f]+DE_cycles[f];
    ScanlineTiming[LINE_STOP_LIMIT_MINUS2][f]=ScanlineTiming[LINE_STOP_LIMIT][f]-2*TICKS8;
  }
  ScanlineTiming[LINE_START_LIMIT_PLUS26][FREQ_IDX_71]
    =ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_71]+26*TICKS8;
  ScanlineTiming[LINE_START_LIMIT_MINUS12][FREQ_IDX_71]
    =ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_71]-12*TICKS8;
  ScanlineTiming[HSYNC_ON2][FREQ_IDX_50]=ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_71]-54*TICKS8;
  ScanlineTiming[HSYNC_OFF2][FREQ_IDX_50]=ScanlineTiming[HSYNC_ON2][FREQ_IDX_50]
    +GLU_HSYNC_DURATION_LO*TICKS8;
  // On the STE, the decision occurs sooner due to hardscroll possibility
  // but prefetch starts sooner only if HSCROLL <> 0.
  if(IS_STE)
  {
    ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_71]=ScanlineTiming[DE_ON_HSCROLL][FREQ_IDX_71]
      =ScanlineTiming[DE_ON][FREQ_IDX_71]-4*TICKS8-1*TICKS8; // 3615GEN4 nitrowave
    ScanlineTiming[LINE_START_LIMIT_MINUS12][FREQ_IDX_71]=0; //512 almost same time as hsync!
    ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_60]=ScanlineTiming[DE_ON_HSCROLL][FREQ_IDX_60]
      =ScanlineTiming[DE_ON][FREQ_IDX_60]-16*TICKS8;
    ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_50]=ScanlineTiming[DE_ON_HSCROLL][FREQ_IDX_50]
      =ScanlineTiming[DE_ON][FREQ_IDX_50]-16*TICKS8;
   ScanlineTiming[HSYNC_OFF2][FREQ_IDX_50]-=1*TICKS8; // approx.
  }
  ScanlineTiming[LINE_START_LIMIT_PLUS2][FREQ_IDX_71]
    =ScanlineTiming[LINE_START_LIMIT][FREQ_IDX_71]+2*TICKS8;
  // HBLANK (decision)
  // There's a -4 difference for 60hz but timings are the same on STE
  // HBLANK in high res is ignored by the STF Shifter
  ScanlineTiming[HBLANK_OFF][FREQ_IDX_50]=GLU_HBLANKOFF_50*TICKS8+HblShift+WuModSync;
  ScanlineTiming[HBLANK_OFF][FREQ_IDX_60]=ScanlineTiming[HBLANK_OFF][FREQ_IDX_50]-4*TICKS8;
  // HSYNC
  // notice 472+40=512 hbl pending = start of scanline
  ScanlineTiming[HSYNC_ON][FREQ_IDX_50]=GLU_HSYNCON_50*TICKS8+HblShift; 
  ScanlineTiming[HSYNC_ON1][FREQ_IDX_50]=ScanlineTiming[HSYNC_ON][FREQ_IDX_50]+WuModSync;
  if(IS_STE)
  {
    ScanlineTiming[HSYNC_ON1][FREQ_IDX_50]-=2*TICKS8;  //?
    ScanlineTiming[HSYNC_ON2][FREQ_IDX_50]-=2*TICKS8;  //?
  }
  ScanlineTiming[HSYNC_ON][FREQ_IDX_60]=ScanlineTiming[HSYNC_ON][FREQ_IDX_50]-4*TICKS8;
  ScanlineTiming[HSYNC_ON1][FREQ_IDX_60]=ScanlineTiming[HSYNC_ON1][FREQ_IDX_50]-4*TICKS8;
  ScanlineTiming[HSYNC_OFF][FREQ_IDX_50]=ScanlineTiming[HSYNC_ON][FREQ_IDX_50]
    +GLU_HSYNC_DURATION_LO*TICKS8;
  ScanlineTiming[HSYNC_OFF][FREQ_IDX_60]=ScanlineTiming[HSYNC_ON][FREQ_IDX_60]
    +GLU_HSYNC_DURATION_LO*TICKS8;
#ifdef SSE_BETA  //not used
  ScanlineTiming[HSYNC_ON2][FREQ_IDX_60]=ScanlineTiming[HSYNC_ON2][FREQ_IDX_50];
  ScanlineTiming[HSYNC_OFF2][FREQ_IDX_60]=ScanlineTiming[HSYNC_OFF2][FREQ_IDX_50];
  ScanlineTiming[HSYNC_ON][FREQ_IDX_71]=204*TICKS8+HblShift+WuModRes;
  ScanlineTiming[HSYNC_OFF][FREQ_IDX_71]=ScanlineTiming[HSYNC_ON][FREQ_IDX_71]
    +GLU_HSYNC_DURATION_HI*TICKS8;
#endif
  // Reload video counter
  ScanlineTiming[RELOAD_VC][FREQ_IDX_50]=GLU_RELOADVC_50*TICKS8+HblShift;
  ScanlineTiming[RELOAD_VC][FREQ_IDX_60]=ScanlineTiming[RELOAD_VC][FREQ_IDX_50]-4*TICKS8; //?
  ScanlineTiming[RELOAD_VC][FREQ_IDX_71]=GLU_RELOADVC_70;
  // Enable VBI - VBITMG.TOS, idealised!
  //ScanlineTiming[ENABLE_VBI][FREQ_IDX_50]=((HblShift&4)||IS_STE) ? 68*TICKS8 : 64*TICKS8;//wrong for 64bit
  ScanlineTiming[ENABLE_VBI][FREQ_IDX_50]=((HblShift==(4*TICKS8))||IS_STE) 
    ? (GLU_ENABLEVBI_50+4)*TICKS8 : GLU_ENABLEVBI_50*TICKS8;
  ScanlineTiming[ENABLE_VBI][FREQ_IDX_60]=ScanlineTiming[ENABLE_VBI][FREQ_IDX_50]-4*TICKS8;
  ScanlineTiming[ENABLE_VBI][FREQ_IDX_71]=0;
  // 508 or 512 cycles? this is settled at around DE timing of the same scanline
  // variable is BYTE (legacy), that's why we don't multiply 62!
  ScanlineCyclesTiming=GLU_SCANLINE_CYCLES_CHECK+HblShift/TICKS8+WuModSync/TICKS8;
  if(IS_STE)
    ScanlineCyclesTiming+=4;
  // Top and bottom borders
  ScanlineTiming[VERT_OVSCN_LIMIT][FREQ_IDX_50]
    =ScanlineTiming[HSYNC_ON1][FREQ_IDX_50]+GLU_HSYNC_DURATION_LO*TICKS8-2*TICKS8;
  ScanlineTiming[VERT_OVSCN_LIMIT][FREQ_IDX_60]=ScanlineTiming[VERT_OVSCN_LIMIT][FREQ_IDX_50]-4*TICKS8;
  // misc
  ScanlineTiming[LINE_PLUS_20A][FREQ_IDX_50]=12*TICKS8+HblShift;
  ScanlineTiming[LINE_PLUS_20B][FREQ_IDX_50]=16*TICKS8+HblShift;
  ScanlineTiming[LINE_PLUS_20C][FREQ_IDX_50]=10*TICKS8+HblShift;
  ScanlineTiming[LINE_PLUS_20D][FREQ_IDX_50]=26*TICKS8+HblShift;
  ScanlineTiming[LINE_PLUS_26A][FREQ_IDX_50]=20*TICKS8+HblShift;
  ScanlineTiming[LINE_PLUS_26B][FREQ_IDX_50]=24*TICKS8+HblShift;
  ScanlineTiming[LINE_PLUS_26C][FREQ_IDX_50]=28*TICKS8+HblShift;
  ScanlineTiming[MEDRES_OA][FREQ_IDX_50]=24*TICKS8+HblShift;
  ScanlineTiming[MEDRES_OB][FREQ_IDX_50]=48*TICKS8+HblShift;
  ScanlineTiming[MEDRES_OC][FREQ_IDX_50]=16*TICKS8+HblShift;
  ScanlineTiming[DESTAB_A][FREQ_IDX_50]=92*TICKS8+HblShift;
//  ScanlineTiming[DESTAB_B][FREQ_IDX_50]=212+HblShift;
  ScanlineTiming[DESTAB0][FREQ_IDX_50]=72*TICKS8+HblShift;
  ScanlineTiming[STAB_A][FREQ_IDX_50]=440*TICKS8+HblShift;
  ScanlineTiming[STAB_B][FREQ_IDX_50]=468*TICKS8+HblShift;
  ScanlineTiming[STAB_C][FREQ_IDX_50]=472*TICKS8+HblShift;
  ScanlineTiming[STAB_D][FREQ_IDX_50]=448*TICKS8+HblShift;
  ScanlineTiming[RENDER_CYCLE][FREQ_IDX_50]=CYCLES_FROM_HBL_TO_LEFT_BORDER_OPEN+8*TICKS8+HblShift;
  ScanlineTiming[LINE_PLUS_44_R][FREQ_IDX_50]
    =ScanlineTiming[LINE_STOP_LIMIT][FREQ_IDX_50]-WuModSync+WuModRes+2*TICKS8;
  if(IS_STE)
    ScanlineTiming[LINE_PLUS_44_R][FREQ_IDX_50]-=2*TICKS8;
#if defined(SSE_VID_STVL1) 
  if(hStvl!=NULL) // now necessary or VC19 hangs on x64 build...
    StvlUpdate();
#endif
}

#undef LOGSECTION
