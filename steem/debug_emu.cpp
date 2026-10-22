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
FILE: debug_emu.cpp
CONDITION: DEBUG_BUILD must be defined
DESCRIPTION: General low-level debugging functions.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop


#ifdef DEBUG_BUILD

#include <computer.h>
#include <debug.h>
#include <debug_emu.h>
#include <debugger.h>
#include <gui.h>
#include <draw.h>
#include <osd.h>
#if defined(SSE_VID_STVL1)
#include <interface_stvl.h>
#endif
#include <iolist.h>

bool debug_in_trace=false;
#if !defined(SSE_DEBUGGER_NODRAW)
bool redraw_on_stop=false,redraw_after_trace=false;
COLORREF debug_gun_pos_col=RGB(255,0,0);
#endif
int crash_notification=CRASH_NOTIFICATION_NEVER;
bool stop_on_blitter_flag=false;
BYTE stop_on_user_change=0;
BYTE stop_on_next_program_run=0;
BYTE stop_on_next_reset=0;
bool debug_first_instruction=false;
Str runstate_why_stop;
DWORD debug_cycles_since_VBL,debug_cycles_since_HBL;
COUNTER_VAR debug_ACT;
DWORD debug_USP,debug_SSP;
MEM_ADDRESS debug_VAP;
COUNTER_VAR debug_time_to_timer_timeout[4];
#if defined(SSE_DEBUGGER)
int debug_timer_prescale[4];
int debug_timer_data[4];
int debug_timer_count[4];
int debug_timer_ticks[4];
#endif
DWORD debug_frame_interrupts;
int debug_cycle_colours=(0);
int debug_screen_shift=(0);
int debug_num_bk=(0),debug_num_mon_reads=(0),debug_num_mon_writes=(0);
int debug_num_mon_reads_io=(0),debug_num_mon_writes_io=(0);

MEM_ADDRESS debug_bk_ad[MAX_BREAKPOINTS],debug_mon_read_ad[MAX_BREAKPOINTS],debug_mon_write_ad[MAX_BREAKPOINTS];
MEM_ADDRESS debug_mon_read_ad_io[MAX_BREAKPOINTS],debug_mon_write_ad_io[MAX_BREAKPOINTS];
WORD debug_mon_read_mask[MAX_BREAKPOINTS],debug_mon_write_mask[MAX_BREAKPOINTS];
WORD debug_mon_read_mask_io[MAX_BREAKPOINTS],debug_mon_write_mask_io[MAX_BREAKPOINTS];

MEM_ADDRESS trace_over_breakpoint=0xffffffff;
COUNTER_VAR debug_run_until=(DRU_OFF),debug_run_until_val;
BYTE monitor_mode=MONITOR_MODE_STOP,breakpoint_mode=BREAKPOINT_MODE_STOP;
bool break_on_irq[NUM_BREAK_IRQS]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

MEM_ADDRESS pc_history[HISTORY_SIZE];
SHORT pc_history_y[HISTORY_SIZE];
SHORT pc_history_c[HISTORY_SIZE];

int pc_history_idx=0;
#if !defined(SSE_DBG_NOSENDKEYS)
BYTE debug_send_alt_keys=0,debug_send_alt_keys_vbl_countdown=0;
#endif
// this was probably something only Steem authors had:
void *debug_plugin_routines[]={(void*)2,(void*)debug_plugin_read_mem,
  (void*)debug_plugin_write_mem};

DynamicArray<TDebugPluginfo> debug_plugins;

LONG how_big_is_0000;
char reg_name_buf[8];

int debug_get_ad_mode(MEM_ADDRESS ad) {
  TDebugAddress *pda=debug_find_address(ad);
  if(pda==NULL) 
    return 0;
  int ad_mode=pda->mode;
  if(ad_mode==1) 
    ad_mode=int((pda->bwr&1)?breakpoint_mode:monitor_mode);
  return ad_mode;
}


WORD debug_get_ad_mask(MEM_ADDRESS ad,bool read) {
  TDebugAddress *pda=debug_find_address(ad);
  if(pda==NULL) 
    return 0;
  return pda->mask[int(read?1:0)];
}


#if !defined(SSE_DEBUGGER_NODRAW) // forget it

// This is for if the emu is half way though the screen, it should be called
// immediately after draw_begin to fix draw_dest_ad
void debug_update_drawing_position(int *pHorz) {
  int y=-TopScanlines[0];
  if(VideoFreqAtStartOfVbl==NTSC_HZ)
    y=-TopScanlines[1];
  for(;y<scan_y;y++)
  {
    if(y>=draw_first_scanline_for_border||y>=video_first_draw_line)
    {
      if(y<video_last_draw_line||y<draw_last_scanline_for_border)
      {
        if(y>=draw_first_possible_line && y<draw_last_possible_line)
        {
          draw_dest_ad=draw_dest_next_scanline;
          draw_dest_next_scanline+=draw_dest_increase_y;
        }
      }
    }
  }
  if(ScreenResAtStartOfVbl<2)
  {
    if((scan_y>=draw_first_scanline_for_border||scan_y>=video_first_draw_line)
      && (scan_y<video_last_draw_line||scan_y<draw_last_scanline_for_border))
    {
      int horz_scale=1;
      if(ScreenResAtStartOfVbl==1||video_mixed_output||draw_med_low_double_height)
        horz_scale=2;
      int x=scanline_drawn_so_far;
#if defined(SSE_VID_STVL1)
      if(OPTION_C3)
        x=Stvl.dbg_npixels;
#endif
      if(border==0)
        x=MAX(x-SideBorderSizeWin,0);
      x*=horz_scale;
#if defined(SSE_DRAW_C)
      draw_dest_ad+=x;
#else
      draw_dest_ad+=x*BytesPerPixel;
#endif
      if(pHorz)
        *pHorz=horz_scale;
    }
  }
}


void update_display_after_trace() {
  if(extended_monitor) 
    return;
  OsdControl.no_draw=true;
  if(ScreenResAtStartOfVbl<2 && !redraw_after_trace)
  {
    RECT old_src=draw_blit_source_rect;
    draw_begin();
    if(draw_blit_source_rect.left!=old_src.left || draw_blit_source_rect.top
      !=old_src.top || draw_blit_source_rect.right!=old_src.right
      || draw_blit_source_rect.bottom!=old_src.bottom)
    {
      draw_end();
      draw(false);
      draw_begin();
    }
    draw_buffer_complex_scanlines=false;
    int horz_scale=0;
    debug_update_drawing_position(&horz_scale);
#if defined(SSE_VID_STVL1)
    if(OPTION_C3)
    {
      if(draw_lock && Stvl.render_y>render_vstart && Stvl.render_y<=render_vend)
      {
        if(draw_mem_line_ptr+render_scanline_length<(DWORD*)Disp.VideoMemoryEnd
          && draw_mem_line_ptr>=(DWORD*)draw_mem)
        {
          DWORD *source_start=Stvl.draw_mem_ptr_min+render_hstart; //bugfix too
          for(int i=0;i<render_scanline_length;i++)
            draw_mem_line_ptr[i]=source_start[i];
        }
      }
    }
    else
#endif
      Shifter.Render(LINECYCLES,DISPATCHER_DEBUGGER);
    if((scanline_drawn_so_far<SideBorderSize+320+SideBorderSize)&&horz_scale)
    {
      int line_add=0;
      if(draw_med_low_double_height) line_add=draw_line_length;
      for(int i=0;i<horz_scale;i++)
      {
        DWORD col=colour_convert(GetRValue(debug_gun_pos_col),
          GetGValue(debug_gun_pos_col),GetBValue(debug_gun_pos_col));
        switch(BytesPerPixel) {
        case 1:
          *draw_dest_ad=BYTE(col);
          *(draw_dest_ad+line_add)=BYTE(col);
          break;
        case 2:
          *LPWORD(draw_dest_ad)=WORD(col);
          *LPWORD(draw_dest_ad+line_add)=WORD(col);
          break;
        case 3:
        case 4:
          *LPDWORD(draw_dest_ad)=DWORD(col);
          *LPDWORD(draw_dest_ad+line_add)=DWORD(col);
          break;
        }
#if defined(SSE_DRAW_C)
        draw_dest_ad++;
#else
        draw_dest_ad+=BytesPerPixel;
#endif
      }
    }
#if defined(SSE_DEBUGGER)
    INT_PTR remaining=Disp.VideoMemoryEnd-draw_dest_ad;
    if(remaining>0&&remaining<Disp.VideoMemorySize)
      if(draw_lock)
#if defined(SSE_DRAW_C)
        ZeroMemory((BYTE*)draw_dest_ad,remaining*4);
#else
        ZeroMemory((BYTE*)draw_dest_ad,remaining);
#endif
#endif
    {
      draw_end();
      draw_blit();
    }
  }
  else
    draw(false);
  OsdControl.no_draw=false;
}

#endif//#if !defined(SSE_DEBUGGER_NODRAW)

void breakpoint_log() {
  //if(logging_suspended) 
    //return;
  Str logline=Str("\r\n!!!! PASSED BREAKPOINT at address $")+HEXSl(pc,6)+", sr="+HEXSl(SR,4);
  logline+="\r\n";
  for(int n=0;n<8;n++) logline+=Str("d")+n+"="+HEXSl(Cpu.r[n],6)+"  ";
  logline+="\r\n";
  for(int n=0;n<8;n++) logline+=Str("a")+n+"="+HEXSl(areg[n],6)+"  ";
  logline+="\r\n";
  logline+=Str("time=")+ABSOLUTE_SYS_TIME+" scanline="+scan_y+" cycle="+(ABSOLUTE_SYS_TIME-TimeOfHSyncOff);
  logline+="\r\n";
  TRACE("%s",logline.c_str());
}


void breakpoint_check() {
  if(runstate!=RUNSTATE_RUNNING) 
    return;
  MEM_ADDRESS dbgpc=(pc&0x00FFFFFF); // pc is 32bit now
  for(int n=0;n<debug_num_bk;n++)
  {
    if(debug_bk_ad[n]==dbgpc)
    {
      if(debug_get_ad_mode(dbgpc)==3)
        breakpoint_log();
      else
      {
        runstate=RUNSTATE_STOPPING;
        SET_WHY_STOP(Str("Hit breakpoint at address $")+HEXSl(dbgpc,6));
      }
      return;
    }
  }
}


void check_stop_on_user_change() {
  if(stop_on_user_change && !PSWS && (debug_old_sr&SR_S))
    stop_on_user_change=2;
#if defined(SSE_DEBUG_SYMBOLS)
  if(Tos.fpPrgCopy && !PSWS && (debug_old_sr&SR_S))
    HandlePrgCopy();
#endif
}


void debug_update_cycle_counts() {
  debug_ACT=ABSOLUTE_SYS_TIME;
  debug_cycles_since_VBL=(DWORD)(debug_ACT-sys_time_of_last_vbl);
  debug_cycles_since_HBL=(DWORD)(debug_ACT-TimeOfHSyncOff);
  debug_VAP=
#if defined(SSE_VID_STVL1) 
    (OPTION_C3)?Stvl.vcount.d32:
#endif
    Mmu.ReadVideoCounter((short)debug_cycles_since_HBL);
  
#if defined(SSE_HD6301_LL)
  hd6301_copy_ram(Debug.HD6301RamBuffer); // in 6301.c
#endif
  for(BYTE t=0;t<4;t++)
  {
    debug_timer_data[t]=Mfp.reg[MFPR_TADR+t]; //could directly point to it?
    Mfp.CalcTimerCounter(t,debug_ACT); //! do we change emulation?
    debug_timer_count[t]=mfp_timer_counter[t]/64;
    if(Mfp.timer_control[t])
    {
      if(Mfp.timer_control[t]==8)
      {
        if(t==MFP_TIMER_B)
          debug_time_to_timer_timeout[t]=(time_of_next_timer_b-debug_ACT);
      }
      else
      {
        debug_time_to_timer_timeout[t]=(mfp_timer_timeout[t]-debug_ACT);
#if defined(SSE_DEBUGGER)
        debug_timer_prescale[t]=mfp_timer_prescale[Mfp.GetTimerControlRegister(t)];
        debug_timer_data[t]=Mfp.reg[MFPR_TADR+t]; //could directly point to it?
        debug_timer_ticks[t]=Mfp.Prescale[t];
#endif
      }
    }
    else
    {
        debug_time_to_timer_timeout[t]=0;
#if defined(SSE_DEBUGGER)
      debug_timer_prescale[t]=0;
      debug_timer_ticks[t]=0;
      //debug_frame_interrupts=(Debug.FrameInterrupts<<16)+Debug.FrameMfpIrqs;
      debug_frame_interrupts=
        //((Debug.FrameInterrupts&4)<<18) // mfp irq
        (Debug.FrameMfpIrqs<<16) // mfp irq mask
        |((Debug.FrameInterrupts&2)<<7) // vbl
        |(Debug.FrameInterrupts&1); // hbl
#endif
    }
  }
  debug_USP=USP;
  debug_SSP=SSP;
}


void debug_hit_mon(MEM_ADDRESS ad,int read) {
  if(stem_runmode!=STEM_MODE_CPU)
    return;
  int bytes=2;
#if defined(SSE_DEBUGGER_MONITOR_VALUE)
/*  When the option is checked, we will stop Steem only if the condition
    is met when R/W on the address.
    Problem: Steem didn't foresee it and more changes are needed for
    write (what value?)
*/
  int val=0;
  if(ad&1)
    ad--;
  if(Debug.MonitorValueSpecified && Debug.MonitorComparison)
  {
    val=(int)d2_dpeek(ad);
    //val=dbus; // problem write byte $XX -> dbus = $XXXX
    if( (Debug.MonitorComparison=='=' && val!=Debug.MonitorValue)
      ||(Debug.MonitorComparison=='!' && val==Debug.MonitorValue)
      ||(Debug.MonitorComparison=='<' && val>=Debug.MonitorValue)
      ||(Debug.MonitorComparison=='>' && val<=Debug.MonitorValue))
    {
     // TRACE("?addr %X value %X %c %X\n",ad,val,Debug.MonitorComparison,Debug.MonitorValue);
      return;
    }
    else
      TRACE("addr %X value %X %c %X\n",ad,val,Debug.MonitorComparison,Debug.MonitorValue);
  }
  else
  {
    WORD mask=debug_get_ad_mask(ad,(read!=0));
    if(mask==0xff00) 
      bytes=1;
    if(mask==0x00ff) 
      bytes=1,ad++;
    val=(bytes==1) ? (int)d2_peek(ad) : (int)d2_dpeek(ad);
  }
#else 
  WORD mask=debug_get_ad_mask(ad,read);
  if(mask==0xff00) bytes=1;
  if(mask==0x00ff) bytes=1,ad++;
  int val=int((bytes==1)?int(d2_peek(ad)):int(d2_dpeek(ad)));
#endif//390
  Str mess;
  if(Dma.Request)
    mess=HEXSl(ad,6)+": DMA access $" + HEXSl(val,4);
  else if(read)
    mess=HEXSl(old_pc,6)+": Read "+val
      +" ($"+HEXSl(val,bytes*2)+") from address $"+HEXSl(ad,6);
  else
    mess=HEXSl(old_pc,6)+": Write "+val
      +" ($"+HEXSl(val,bytes*2)+") to address $"+HEXSl(ad,6);
  int ad_mode=debug_get_ad_mode(ad & ~1);
#if defined(SSE_DEBUGGER_MONITOR_RANGE) // mode is likely 0 (ad not found)
  if(Debug.MonitorRange)
    ad_mode=monitor_mode;
#endif
  if(ad_mode==MONITOR_MODE_STOP)
  {
    if(runstate==RUNSTATE_RUNNING)
    {
      runstate=RUNSTATE_STOPPING;
      SET_WHY_STOP(mess);
    }
    else if(runstate==RUNSTATE_STOPPED)
    {
#if defined(SSE_DEBUGGER_STATUS_BAR)
      DbgStatusBarMsg(mess.Text);
      if(Debug.DialogOnStopEvent)
#endif
        Alert(mess,"Monitor Activated",0);
    }
  }
  else
  {
    debug_mem_write_log_address=ad;
    debug_mem_write_log_bytes=bytes;
    if(read)
      ioaccess|=IOACCESS_DEBUG_MEM_READ_LOG;
    else
      ioaccess|=IOACCESS_DEBUG_MEM_WRITE_LOG;
    if(Dma.Request)
      ioaccess|=IOACCESS_DEBUG_MEM_DMA;
  }
}


void debug_hit_io_mon_write(MEM_ADDRESS ad,int val) {
  if(stem_runmode!=STEM_MODE_CPU) 
    return;
  WORD mask=debug_get_ad_mask(ad,FALSE); // "return pda->mask[int(read ? 1:0)];"

#if defined(SSE_DEBUGGER_MONITOR_VALUE)
/*  When the option is checked, we will stop Steem only if the condition
    is met when R/W on the address.
*/
  if(Debug.MonitorValueSpecified && Debug.MonitorComparison)
  {
    if(
      (Debug.MonitorComparison=='=' && val!=Debug.MonitorValue)
      ||(Debug.MonitorComparison=='!' && val==Debug.MonitorValue)
      ||(Debug.MonitorComparison=='<' && val>=Debug.MonitorValue)
      ||(Debug.MonitorComparison=='>' && val<=Debug.MonitorValue))
    {
      return;
    }
    else
      TRACE("addr %X value %X %c %X\n",ad,val,Debug.MonitorComparison,Debug.MonitorValue);
  }
#endif

  int bytes=2;
  if(mask==0xff00) 
    bytes=1;
  if(mask==0x00ff) 
    bytes=1,ad++;
  Str mess=HEXSl(old_pc,6)+": Wrote to address $"+HEXSl(ad,6)+", new value is "
    +val+" ($"+HEXSl(val,bytes*2)+")";
  int ad_mode=debug_get_ad_mode(ad & ~1);
  if(ad_mode==MONITOR_MODE_STOP)
  {
    if(runstate==RUNSTATE_RUNNING)
    {
      runstate=RUNSTATE_STOPPING;
      SET_WHY_STOP(mess);
    }
    else if(runstate==RUNSTATE_STOPPED)
    {
      Alert(mess,"Monitor Activated",0);
    }
  }
  else
  {
    debug_mem_write_log_address=ad;
    debug_mem_write_log_bytes=bytes;
    ioaccess|=IOACCESS_DEBUG_MEM_WRITE_LOG; // ->Trace.txt
  }
}


void debug_check_for_events() {
  while(sys_cycles<=0)
  {
    TRACE_EVENT("DBG",event_vector);
    event_vector();
    prepare_next_event();
  }
}


void iolist_debug_add_pseudo_addresses() {
  iolist_add_entry(IOLIST_PSEUDO_AD_PSG+0x000,"PSG0 Ch.A Freq L",1,NULL,psg_reg);
  iolist_add_entry(IOLIST_PSEUDO_AD_PSG+0x002,"PSG1 Ch.A Freq H",1,NULL,psg_reg+1);
  iolist_add_entry(IOLIST_PSEUDO_AD_PSG+0x004,"PSG2 Ch.B Freq L",1,NULL,psg_reg+2);
  iolist_add_entry(IOLIST_PSEUDO_AD_PSG+0x006,"PSG3 Ch.B Freq H",1,NULL,psg_reg+3);
  iolist_add_entry(IOLIST_PSEUDO_AD_PSG+0x008,"PSG4 Ch.C Freq L",1,NULL,psg_reg+4);
  iolist_add_entry(IOLIST_PSEUDO_AD_PSG+0x00a,"PSG5 Ch.C Freq H",1,NULL,psg_reg+5);
  iolist_add_entry(IOLIST_PSEUDO_AD_PSG+0x00c,"PSG6 Noise Freq",1,NULL,psg_reg+6);
  iolist_add_entry(IOLIST_PSEUDO_AD_PSG+0x00e,"PSG7 Mixer",1,"PortB Out|PortA Out|Ch.C Noise off|B Noise Off|A Noise Off|C Tone Off|B Tone Off|A Tone Off",psg_reg+7);
  iolist_add_entry(IOLIST_PSEUDO_AD_PSG+0x010,"PSG8 Ch.A Ampl",1,".|.|.|env|A3|A2|A1|A0",psg_reg+8);
  iolist_add_entry(IOLIST_PSEUDO_AD_PSG+0x012,"PSG9 Ch.B Ampl",1,".|.|.|env|A3|A2|A1|A0",psg_reg+9);
  iolist_add_entry(IOLIST_PSEUDO_AD_PSG+0x014,"PSG10 Ch.C Ampl",1,".|.|.|env|A3|A2|A1|A0",psg_reg+10);
  iolist_add_entry(IOLIST_PSEUDO_AD_PSG+0x016,"PSG11 Env Period H",1,NULL,psg_reg+11);
  iolist_add_entry(IOLIST_PSEUDO_AD_PSG+0x018,"PSG12 Env Period L",1,NULL,psg_reg+12);
  iolist_add_entry(IOLIST_PSEUDO_AD_PSG+0x01a,"PSG13 Env shape",1,".|.|.|.|Continue|Attack|Alternate|Hold",psg_reg+13);
  iolist_add_entry(IOLIST_PSEUDO_AD_PSG+0x01c,"PSG14 Port A",1,"IDE Drv on|SCC A|Mon Jack GPO|Int. Spkr|Cent strobe|RS232 DTR|RS232 RTS|Drv 1|Drv 0|Drv side",psg_reg+14);
  iolist_add_entry(IOLIST_PSEUDO_AD_PSG+0x01e,"PSG15 Port B (Parallel port)",1,NULL,psg_reg+15);

  iolist_add_entry(IOLIST_PSEUDO_AD_FDC+0x000,"FDC Command Register",1,NULL,&Fdc.cr);
  iolist_add_entry(IOLIST_PSEUDO_AD_FDC+0x002,"FDC Status Register",1,"Motor|Write Protect|Spin/Rec|Seek Fail|CRC Err|Track 0|Index|Busy",&Fdc.str);
  iolist_add_entry(IOLIST_PSEUDO_AD_FDC+0x004,"FDC Sector Register",1,NULL,&Fdc.sr);
  iolist_add_entry(IOLIST_PSEUDO_AD_FDC+0x006,"FDC Track Register",1,NULL,&Fdc.tr);
  iolist_add_entry(IOLIST_PSEUDO_AD_FDC+0x008,"FDC Data Register",1,NULL,&Fdc.dr);
  iolist_add_entry(IOLIST_PSEUDO_AD_FDC+0x00a,"FDC Drive/Side (PSG 14)",1,"B|A|Side 0",&(psg_reg[14]));
  iolist_add_entry(IOLIST_PSEUDO_AD_FDC+0x00c,"FDC Current Track Drive A",1,NULL,&(FloppyDrive[DRIVE_A].track));
  iolist_add_entry(IOLIST_PSEUDO_AD_FDC+0x00e,"FDC Current Track Drive B",1,NULL,&(FloppyDrive[DRIVE_B].track));
  iolist_add_entry(IOLIST_PSEUDO_AD_FDC+0x010,"FDC Spinning Up",1,NULL,lpDWORD_B_0(&fdc_spinning_up));
  iolist_add_entry(IOLIST_PSEUDO_AD_FDC+0x012,"FDC Type 1 Command Active",1,NULL,lpDWORD_B_0(&Fdc.StatusType));
  iolist_add_entry(IOLIST_PSEUDO_AD_FDC+0x018,"DMA Address High",1,NULL,lpDWORD_B_2(&dma_address));
  iolist_add_entry(IOLIST_PSEUDO_AD_FDC+0x01a,"DMA Address Mid",1,NULL,lpDWORD_B_1(&dma_address));
  iolist_add_entry(IOLIST_PSEUDO_AD_FDC+0x01c,"DMA Address Low",1,NULL,lpDWORD_B_0(&dma_address));
  iolist_add_entry(IOLIST_PSEUDO_AD_FDC+0x01e,"DMA Mode",1,"FDC Transfer|Disable DMA|.|Sec Count Select|HDC|A1|A0|.",lpWORD_B_0(&Dma.mcr));
  iolist_add_entry(IOLIST_PSEUDO_AD_FDC+0x020,"DMA Write From RAM (Bit 8 of Mode)",1,NULL,lpWORD_B_1(&Dma.mcr));
  iolist_add_entry(IOLIST_PSEUDO_AD_FDC+0x022,"DMA Status",1,"DRQ|Sec Count Not 0|No Error",&Dma.sr);
  iolist_add_entry(IOLIST_PSEUDO_AD_FDC+0x024,"DMA Sector Count",1,NULL,lpDWORD_B_0(&Dma.Counter));

  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x000,"IKBD Mouse Mode",1,NULL,lpDWORD_B_0(&Ikbd.mouse_mode));
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x002,"IKBD Joy Mode",1,NULL,lpDWORD_B_0(&Ikbd.joy_mode));
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x004,"IKBD Send Nothing Flag",1,NULL,(BYTE*)&Ikbd.send_nothing);
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x006,"IKBD Resetting Flag",1,NULL,(BYTE*)&Ikbd.resetting);
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x008,"IKBD Mouse Button Action",1,"Keys|Release ABS|Press ABS",&Ikbd.mouse_button_press_what_message);
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x00a,"IKBD Port 0 Joystick Flag",1,NULL,(BYTE*)&Ikbd.port_0_joy);
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x00c,"IKBD Mouse Y Reverse Flag",1,NULL,(BYTE*)&Ikbd.mouse_upside_down);
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x00e,"IKBD Relative Mouse Threshold X",1,NULL,lpDWORD_B_0(&Ikbd.relative_mouse_threshold_x));
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x010,"IKBD Relative Mouse Threshold Y",1,NULL,lpDWORD_B_0(&Ikbd.relative_mouse_threshold_y));
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x012,"IKBD Abs Mouse Pos X High",1,NULL,lpDWORD_B_1(&Ikbd.abs_mouse_x));
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x014,"IKBD Abs Mouse Pos X Low",1,NULL,lpDWORD_B_0(&Ikbd.abs_mouse_x));
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x016,"IKBD Abs Mouse Pos Y High",1,NULL,lpDWORD_B_1(&Ikbd.abs_mouse_y));
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x018,"IKBD Abs Mouse Pos Y Low",1,NULL,lpDWORD_B_0(&Ikbd.abs_mouse_y));
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x01a,"IKBD Abs Mouse Max X High",1,NULL,lpDWORD_B_1(&Ikbd.abs_mouse_max_x));
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x01c,"IKBD Abs Mouse Max X Low",1,NULL,lpDWORD_B_0(&Ikbd.abs_mouse_max_x));
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x01e,"IKBD Abs Mouse Max Y High",1,NULL,lpDWORD_B_1(&Ikbd.abs_mouse_max_y));
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x020,"IKBD Abs Mouse Max Y Low",1,NULL,lpDWORD_B_0(&Ikbd.abs_mouse_max_y));
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x022,"IKBD Abs Mouse Scale X",1,NULL,lpDWORD_B_0(&Ikbd.abs_mouse_scale_x));
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x024,"IKBD Abs Mouse Scale Y",1,NULL,lpDWORD_B_0(&Ikbd.abs_mouse_scale_y));
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x026,"IKBD Absolute Mouse Buttons",1,
    "RMB Down|RMB Was Down|LMB Down|LMB Was Down",lpDWORD_B_0(&Ikbd.abs_mousek_flags));
  iolist_add_entry(IOLIST_PSEUDO_AD_IKBD+0x028,"IKBD Joy Button Duration",1,NULL,lpDWORD_B_0(&Ikbd.duration));

#if defined(SSE_HD6301_LL)
  char buffer[80],mask[80]; //overkill for a time
  // internal registers $0-$15
  for(int i=0;i<256;i++)
  {
    mask[0]=NULL;
    sprintf(buffer,(i<0x16)?"IREG %02X":"RAM %02X",i);
    if(i==0x00)
      strcat(buffer," DDR1");
    else if(i==0x01)
      strcat(buffer," DDR2");
    else if(i==0x02)
      strcat(buffer," DR1");
    else if(i==0x03)
      strcat(buffer," DR2");
    else if(i==0x04)
      strcat(buffer," DDR3");
    else if(i==0x05)
      strcat(buffer," DDR4");
    else if(i==0x06)
      strcat(buffer," DR3");
    else if(i==0x07)
      strcat(buffer," DR4");
    else if(i==0x08)
      strcat(buffer," TCSR");
    else if(i==0x09||i==0x0A)
      strcat(buffer," FRC");
    else if(i==0x0B||i==0x0C)
      strcat(buffer," OCR");
    else if(i==0x0D||i==0x0E)
      strcat(buffer," ICR");
    else if(i==0x0F)
      strcat(buffer," CSR");
    else if(i==0x10)
      strcat(buffer," RMCR");
    else if(i==0x11)
    {
      strcat(buffer," TRCSR");
      strcpy(mask,"RDRF|OVR|TDRE|RIE|RE|TIE|TE|WU"); //yeah!
    }
    else if(i==0x12)
      strcat(buffer," RDR");
    else if(i==0x13)
      strcat(buffer," TDR");
    else if(i==0x14)
      strcat(buffer," RCR");
    else if(i<0x80)
      strcat(buffer," (NULL)");
    else if(i==0x80)
      strcat(buffer," [speculation ahead]");
    else if(i>=0x82&&i<=0x87)
      strcat(buffer," Date+time");
    else if(i==0x88)
      strcat(buffer," Init"); // $AA
    else if(i==0x9B)
      strcat(buffer," Buttons");
    else if(i==0xA4)
      strcat(buffer," Joystick 0");
    else if(i==0xA5)
      strcat(buffer," Joystick 1");
    else if(i>=0xAA&&i<=0xAD)
      strcat(buffer," Abs. mouse limit");
    else if(i==0xAE&&i<=0xAF)
      strcat(buffer," Mouse keycode");
    else if(i==0xB0||i==0xB1)
      strcat(buffer," Mouse threshold ");
    else if(i==0xB2||i==0xB3)
      strcat(buffer," Mouse scale ");
    else if(i==0xB4)
      strcat(buffer," Mouse button action");
    else if(i>=0xB5&&i<=0xB9)
      strcat(buffer," Abs. Mouse report");
    else if(i==0xBC)
      strcat(buffer," Mouse X");
    else if(i==0xBD)
      strcat(buffer," Mouse Y");
    else if(i==0xBE)
      strcat(buffer," Mouse move X");
    else if(i==0xBF)
      strcat(buffer," Mouse move Y");
    else if(i>=0xC0&&i<=0xC2)
      strcat(buffer," Mouse buttons");
    else if(i==0xC9)
    {
      strcat(buffer," Mouse mode");
      strcpy(mask,"on|key|abs|evt|.|.|mon|rev");
    }
    else if(i==0xCA)
    {
      strcat(buffer," Joystick mode");
      strcpy(mask,".|.|on|int|evt|key|mon|but");
    }
    else if(i==0xCB)
    {
      strcat(buffer," Command status");
      strcpy(mask,"new|.|input full|complete|complete|par|par|par");
    }
    else if(i>=0xCD&&i<=0xD4)
      strcat(buffer," Input buffer");
    else if(i==0xD6)
      strcat(buffer," Output buffer index");
    else if(i==0xD7)
      strcat(buffer," Output buffer counter");
    else if(i>=0xD9&&i<=0xED)
      strcat(buffer," Output buffer");
    else if(i>=0xFF-6) // size?
      strcat(buffer," Stack");
    iolist_add_entry(IOLIST_PSEUDO_AD_6301+i*2,buffer,1,
      mask[0]?mask:NULL,&Debug.HD6301RamBuffer[i]);
  }

#endif
}


int PASCAL debug_plugin_read_mem(DWORD ad,BYTE *buf,int len) {
  if(ad>=himem) 
    return 0;
  if(ad+len>=himem) 
    len=himem-ad;
  int n_bytes=len;
  BYTE *p=lpPEEK(ad);
  while(len--)
  {
    *(buf++)=*p;
    p+=MEM_DIR;
  }
  return n_bytes;
}


int PASCAL debug_plugin_write_mem(DWORD ad,BYTE *buf,int len) {
  if(ad>=himem) 
    return 0;
  if(ad+len>=himem) 
    len=himem-ad;
  int n_bytes=len;
  BYTE *p=lpPEEK(ad);
  while(len--)
  {
    *p=*(buf++);
    p+=MEM_DIR;
  }
  return n_bytes;
}


char *reg_name(int n) {
  if(debug_uppercase_disa)
    reg_name_buf[0]="DA"[int((n&8)?1:0)];
  else
    reg_name_buf[0]="da"[int((n&8)?1:0)];
  reg_name_buf[1]=(char)('0'+(n&7));
  reg_name_buf[2]='\0';
  return reg_name_buf;
}


MEM_ADDRESS oi(MEM_ADDRESS ad,int of) { //offset by instruction
  if(of==0)
    return ad;
  DWORD_PTR save_dpc=dpc;
  if(of>0)
  {
    dpc=ad;
    for(int n=0;n<of;n++)
      disa_d2((MEM_ADDRESS)dpc);
    MEM_ADDRESS next_inst=dpc&0xffffff;
    dpc=save_dpc;
    if(next_inst<ad)
      return 0;
    return next_inst;
  }
  else if(of<0)
  {
    for(int n=0;n>of;n--)
    {
      // longest instruction is 10 bytes, start from there
      MEM_ADDRESS sad=(ad-10)&0xffffff;
      if(sad>ad)
        ad+=0x1000000;
      while(sad<ad)
      {
        disa_d2(sad&0xffffff);
        if(dpc==(ad&0xffffff))
          break;
        sad+=2;
      }
      if(sad==ad)  // If nothing turns into a decent instruction just guess
        ad-=2;
      else
        ad=sad;
    }
    dpc=save_dpc;
    return ad&0xffffff;
  }
  return 0;
}


#if defined(SSE_DEBUGGER_MONITOR_RANGE) 
/*  Adding range check: is ad between ad1 and ad2
    We use the first 2 watches
*/
  //bool debug_check_wr_check_range(MEM_ADDRESS ad,int num,MEM_ADDRESS *adarr,bool wr) {
bool debug_check_wr_check_range(MEM_ADDRESS ad,int num,MEM_ADDRESS *adarr) {//390
  MEM_ADDRESS ad1=0,ad2=0;
  for(int i=0;i<num;i++)
  {
    if(!ad1)
      ad1=adarr[i];
    else if(!ad2)
    {
      ad2=adarr[i];
      break;
    }
  }
  if(ad1&&ad2&&(ad1<ad2 && ad1<=ad && ad<=ad2 || ad1>ad2 && ad2<=ad && ad<=ad1))
  {
    return true;
  }
  return false;
}
#endif

HWND DbgCreatePushButton(char* caption,int x,int y,int &wid,HWND owner,int id) {
  wid=GetTextSize(hSteemGuiFont,caption).Width+GuiSM.mHorizontalSeparation;
  int h=GuiSM.mCharHeight;
  HWND win=CreateWindow("Button",caption,WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
    x,y,wid,h,owner,(HMENU)(INT_PTR)id,hInstance,NULL);
  return win;
}


#if defined(SSE_DEBUG_SYMBOLS) // little utility created to help development of the feature
void debug_dump_ram(MEM_ADDRESS const start,int const nbytes) {
  MEM_ADDRESS ad=start;
  int ctr=0;
  BYTE row[16];
  while(ctr<nbytes)
  {
    for(int i=0;i<16;i++) // read RAM
    {
      row[i]=(ctr<nbytes)?SafePeek(ad):'.';
      ad++,ctr++;
    }
    TRACE("%06X: ",ad); // start line
    for(int i=0;i<16;i++) // output hex
    {
      TRACE("%02X ",row[i]);
    }
    for(int i=0;i<16;i++) // output text
    {
      TRACE("%c",(row[i]>=0x30&&row[i]<='z')?row[i]:'.');
    }
    TRACE("\n");
  }//wend
}
#endif



#endif//#ifdef DEBUG_BUILD
