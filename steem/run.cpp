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
FILE: run.cpp
CONDITION: SSE_LIBRETRO mustn't be defined
DESCRIPTION: This file contains Steem's run() function, the routine that
actually makes Steem go (optionally in a distinct thread).
Also included here is the code for the event system that allows time-critical
Steem functions (such as VBLs, HBLs and MFP timers) to be scheduled to the
nearest cycle. Speed limiting and drawing is also handled here, in 
event_scanline and event_vbl_interrupt.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#if !defined(SSE_LIBRETRO)

#include <debug.h>
#include <computer.h>
#include <draw.h>
#include <display.h>
#include <osd.h>
#include <infobox.h>
#include <stjoy.h>
#include <shortcutbox.h>
#include <steemh.h>
#ifdef DEBUG_BUILD
#include <debugger.h>
#include <iolist.h>
#if defined(SSE_DEBUGGER_FRAME_REPORT)
#include <debug_framereport.h>
#endif
#endif
#if defined(SSE_VID_STVL1)
#include <interface_stvl.h>
#endif
#include <translate.h>

COUNTER_VAR time_of_next_timer_b=0;
// fast_forward_max_speed=(1000 / (max %/100)); 0 for unlimited
int fast_forward=0,fast_forward_max_speed=0;
int slow_motion=0,slow_motion_speed=100;
#if defined(SSE_DEFAULTS_421)
int run_speed_ticks_per_second;
#else
int run_speed_ticks_per_second=1000;
#endif
int avg_frame_time_counter=0;
#if defined(SSE_DEFAULTS_421)
int frameskip,frameskip_count;
#else
int frameskip=1,frameskip_count=1;
#endif
DWORD avg_frame_time=0;
bool disable_speed_limiting=false; // command-line option
bool fast_forward_stuck_down=false;
bool flashlight_flag=false;

#ifdef UNIX
bool RunWhenStop=false; // for fullscreen
#endif

EVENTPROC event_vector;
COUNTER_VAR time_of_next_event;
COUNTER_VAR time_of_last_hbl_interrupt, time_of_last_vbl_interrupt;
COUNTER_VAR sys_timer_at_res_change;
int runstate;
int run_start_time;
BYTE stem_runmode;
DWORD avg_frame_time_timer,frame_delay_timeout,timer;
DWORD speed_limit_wait_till;
DWORD auto_frameskip_target_time;
DWORD start_of_this_second;
DWORD nframes_this_second;

#ifdef WIN32
TMicroTime MicroTime;
#endif

/*
* run() is called every time emulation is started, generally because the player
* pressed F12 or clicked the play icon
* Windows builds: run() can be run in an apart thread
*/
void run() {
#ifdef WIN32
#if defined(SSE_GUI_TOOLBAR)
  if(!OPTION_TOOLBAR && !FullScreen)
    SendMessage(StemWin,WM_SETICON,ICON_SMALL,(LPARAM)hGUIIcon[RC_ICO_PLAY]);
#endif
#endif//WIN32
#ifdef SSE_HD6301_LL
  Ikbd.Crashed=0;
  mousek=0;
#endif
  StatusInfo.MessageIndex=TStatusInfo::MESSAGE_NONE;
#if defined(SSE_EMU_THREAD)
  SoundLock.Unlock();
  VideoLock.Unlock();
#endif
  // before each run, to avoid some crashes (mostly snapshot-related),
  // restore the stable parts of emulation objects
  ComputerRestore();
#if defined(SSE_STATS_CPU)
  Stats.Restore(); // at each run (Start emulation)
  Stats.myCpuUsage.GetUsage(); // mark start of emulation
#endif
  bool ExcepHappened;
  if(FullScreen)
    Disp.FullscreenRunStart();
  GUIRunStart();
  DEBUG_ONLY( debug_run_start(); )
#if defined(SSE_420R2B)
  bool bOnReset=(LITTLE_PC==SafeLPeek(4));
#else
  bool bOnReset=(LITTLE_PC==rom_addr);
#endif
#ifndef DISABLE_STEMDOS
  if(bOnReset)
    Stemdos.SetDriveReset();
#endif
  ikbd_run_start(bOnReset);
  runstate=RUNSTATE_RUNNING;
  REFRESH_STATUS_BAR;
  REFRESH_STATUS_BAR_GX;
  Glue.m_Status.stop_emu=0;
#ifdef WIN32
  UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
  // Make timer accurate to 1ms
  TIMECAPS tc;
  tc.wPeriodMin=1;
  timeGetDevCaps(&tc,sizeof(TIMECAPS));
  timeBeginPeriod(tc.wPeriodMin);
#endif
  timer=timeGetTime();
  srand(timer); // each thread must seed
#if defined(SSE_STATS)
#ifndef SSE_TIMINGS_US
#ifdef WIN32
  Stats.tEmu0.LowPart=timer;
#endif
#endif
#if defined(SSE_STATS_EXT)
  Stats.Ast0=A_S_T;
#endif
#endif
  SoundStart();
  Glue.AddFreqChange(Glue.VideoFreq);
#ifdef UNIX
  if(Disp.GoToFullscreenOnRun)
    Disp.ChangeToFullScreen();
#endif
#if defined(SSE_DEBUGGER_NODRAW)
  if(!draw_lock)
#endif
  {
    init_screen();
    Draw.MarshalParameters();
    if(bad_drawing==0)
    {
      draw_begin();
#if defined(DEBUG_BUILD) && !defined(SSE_DEBUGGER_NODRAW)
      debug_update_drawing_position();
#endif
    }
  }
  PortsRunStart();
  stem_runmode=STEM_MODE_CPU;
#if defined(SSE_DEBUG_TRACE)
  Debug.TraceGeneralInfos(TDebug::START);
#endif
#ifdef SSE_DEBUG_BUILD
  debug_first_instruction=true; // Don't break if running from breakpoint
#endif
  timer=timeGetTime();
  run_start_time=timer; // For log speed limiting
#ifndef SSE_NO_OSD
  osd_init_run(true);
#endif
  frameskip_count=1;
#if defined(SSE_VID_OLDSYNC)
  ASSERTB(Glue.VideoFreq && MasterSync);
  speed_limit_wait_till=timer+(run_speed_ticks_per_second/
    ((SSEOptions.OldSync)?Glue.VideoFreq:MasterSync));
#else
  speed_limit_wait_till=timer+(run_speed_ticks_per_second/MasterSync);
#endif
  nframes_this_second=0+1;
#if defined(SSE_OSD_FPS_INFO)
  Debug.frame_no_change=0;
#endif
#if defined(SSE_TIMINGS_US)
  QueryPerformanceCounter(&MicroTime.StartBlit); // first frame won't have precise timing
#if defined(SSE_STATS)
  Stats.tEmu0=MicroTime.StartBlit;
#endif
#endif
  avg_frame_time_timer=start_of_this_second=timer;
  avg_frame_time_counter=0;
  // I don't think this can do any damage now, it just checks its
  // list and updates sys_timer and sys_cycles
  DEBUG_ONLY(prepare_next_event(); )
  ioaccess=0;
#if defined(SSE_MAIN_LOOP)
#if defined(SSE_EMU_THREAD) && !defined(_DEBUG) // want to see exception
  try  // apart thread or not
#endif
#endif
  {
#if defined(SSE_EMU_THREAD) && defined(SSE_MAIN_LOOP2) 
    //_se_translator_function old_se_f=
    _set_se_translator(trans_func);
#endif
    //for(;;); // TEST stuck in infinite loop
    //int a=0; int b=5/a;  printf("yoho %d",b);// TEST SEH exception
    do {
      ExcepHappened=false;
      TRY_M68K_EXCEPTION
        while(runstate==RUNSTATE_RUNNING) 
        {
          // sys_cycles is the amount of cycles before next event.
          // So it is *decremented* by instruction timings, not incremented.
          while(sys_cycles>0&&runstate==RUNSTATE_RUNNING)
          {
#ifdef DEBUG_BUILD
            pc_history_y[pc_history_idx]=scan_y;
            pc_history_c[pc_history_idx]=LINECYCLES;
            pc_history[pc_history_idx++]=(pc&0x00FFFFFF);
            if(pc_history_idx>=HISTORY_SIZE) 
              pc_history_idx=0;
#endif
            m68kProcess();
#ifdef DEBUG_BUILD
            debug_first_instruction=false;
            CHECK_BREAKPOINT
#endif
          }//wend
#ifdef DEBUG_BUILD
          if(runstate!=RUNSTATE_RUNNING) 
            break;
          //stem_runmode=STEM_MODE_INSPECT; // why? we're still running
#endif
          for(int i=0;sys_cycles<=0 && (runstate==RUNSTATE_RUNNING);i++) // get out of buggy loop
          {
#if defined(SSE_DEBUGGER_FAKE_IO)
            if(TRACE_MASK2&TRACE_CONTROL_EVENT)
              TRACE_EVENT("Run",event_vector);
#endif
            event_vector();
            prepare_next_event();
            if(//!OPTION_EMUTHREAD && // also for emuthread: avoids killing the thread
              i==10) // get out of buggy loop
            {
              PeekEvent();
              i=0; // i business to avoid load
            }
          }
          CHECK_BREAKPOINT
          DEBUG_ONLY(stem_runmode=STEM_MODE_CPU; )
        }//while (runstate==RUNSTATE_RUNNING)
      CATCH_M68K_EXCEPTION
        TMC68kException e=ExceptionObject;
        ExcepHappened=true;
#ifndef DEBUG_BUILD
        e.crash();
#else
        stem_runmode=STEM_MODE_INSPECT;
        bool alertflag=false;
        if(crash_notification!=CRASH_NOTIFICATION_NEVER)
        {
          alertflag=true;
          //TRY_M68K_EXCEPTION
            if(e.bombs>8)
              alertflag=false;
            else if(crash_notification==CRASH_NOTIFICATION_NOT_TOS
              && e.u_pc.d32>=rom_addr && e.u_pc.d32<rom_addr_end)
              alertflag=false;
            else if(crash_notification==CRASH_NOTIFICATION_BOMBS_DISPLAYED
              && SafeLPeek(e.bombs*4)<rom_addr) //not bombs routine
              alertflag=false;
          //CATCH_M68K_EXCEPTION
            //alertflag=true;
          //END_M68K_EXCEPTION
        }
        DEBUG_ONLY(stem_runmode=STEM_MODE_CPU;) // avoid wrong timing in bus_jam
        if(!alertflag)
          e.crash();
        else
        {
          bool was_locked=draw_lock;
          draw_end();
          if(runstate==RUNSTATE_STOPPED)
            draw(false);
#if defined(SSE_DEBUGGER_STATUS_BAR)
          char crash_msg[60];
          sprintf(crash_msg,"Exception %d bombs",e.bombs); // can become HALT
          DbgStatusBarMsg(crash_msg);
          runstate=RUNSTATE_STOPPING;
          e.crash(); //crash
          debug_trace_crash(e);
          ExcepHappened=false;
          if(Debug.DialogOnStopEvent)
#endif
          {
            if(IDOK==Alert(
              "Exception - do you want to crash (OK)\nor trace? (CANCEL)",
              EasyStr("Exception ")+e.bombs,MB_OKCANCEL|MB_ICONEXCLAMATION)) 
            {
                e.crash();
                if(was_locked)
                  draw_begin();
            }
            else
            {
              runstate=RUNSTATE_STOPPING;
              e.crash(); //crash
              debug_trace_crash(e);
              ExcepHappened=false;
            }
          }
        }
        if(debug_num_bk)
          breakpoint_check();
        if(runstate!=RUNSTATE_RUNNING)
          ExcepHappened=false;
#endif
      END_M68K_EXCEPTION
    } while(ExcepHappened);
    //_set_se_translator(old_se_f);
#if defined(SSE_STATS)
    Stats.run_time=timeGetTime()-run_start_time; // milliseconds run
    StatsStatic.TotalRuntime+=Stats.run_time;
#if defined(SSE_STATS_CPU)
    Stats.tCpuUsage=Stats.myCpuUsage.GetUsage(); // collect CPU%
#endif
    if(InfoBox.Page==TGeneralInfo::STATS && InfoBox.IsVisible()) // show?
      InfoBox.CreatePage(InfoBox.Page); // note exceptions caught by run() handler
#endif
  }
#if defined(SSE_MAIN_LOOP)
#if defined(SSE_EMU_THREAD) // apart thread or not
#if defined(SSE_MAIN_LOOP2)
  catch(SE_Exception e) {
    e.handle_exception();
  }
#elif !defined(_DEBUG)
  catch(...) {
    Alert(T("Unknown exception"),T(STEEM_CRASH_TXT),MB_ICONEXCLAMATION|MB_OK);
    TRACE("Unknown exception\n");
  }
#endif
#endif
#endif
  PortsRunEnd();
  SoundStop();
  if(FullScreen && !ShortcutBox.bSnapshotLoading)
    Disp.FullscreenRunEnd();
  Glue.m_Status.stop_emu=0;
  GUIRunEnd();
#if defined(SSE_DEBUGGER) && !defined(SSE_DEBUGGER_NODRAW)
  draw_end();
#else
  if(draw_lock)
    draw_end();
#endif
  CheckResetDisplay();
#ifdef DEBUG_BUILD
#if !defined(SSE_DEBUGGER_NODRAW)
  if(redraw_on_stop) // pal can be zeroed, no bug... (Forest, Lethal XCess..)
    draw(false);
  else if(runstate_why_stop=="Run until")
    update_display_after_trace();
#endif
  debug_run_until=DRU_OFF;
  debug_run_end();
#endif
#if defined(SSE_DEBUG_TRACE)
  Debug.TraceGeneralInfos(TDebug::STOP);
#endif
#ifdef WIN32
  timeEndPeriod(tc.wPeriodMin); // Finished with accurate timing
#endif
  ONEGAME_ONLY(OGHandleQuit(); )
#ifdef UNIX
  if(RunWhenStop)
  {
    CLICK_PLAY_BUTTON();
    RunWhenStop=false;
  }
#endif
#ifdef WIN32
  bRunMessagePosted=false;
#endif
  runstate=RUNSTATE_STOPPED;
  REFRESH_STATUS_BAR;
#if defined(SSE_420R5)
  UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
#endif

}


void prepare_event_check_for_timer_timeout(int const tn) {
  if(mfp_timer_check[tn])
  {
    if((time_of_next_event-mfp_timer_timeout[tn])>=0)
    {
      time_of_next_event=mfp_timer_timeout[tn];
      event_vector=event_mfp_timer_timeout[tn];
    }
  }
}


// compute sys_cycles to run until next event
void prepare_next_event() {
  if(OPTION_C3)
  {
    event_vector=event_dummy;
    time_of_next_event=A_S_T+EIGHT_MILLION;
  }
  else
  {
    Glue.GetNextVideoEvent();
  }
#if !defined(SSE421R2) // must trigger so the condition is tested in event_timer_b and if not met, tick is canceled
  if(Mfp.reg[MFPR_TBCR]==MFP_TIMER_EVENT_COUNT) // if TBCR changes we come back here (iow calls us)
#endif
  {
    if((time_of_next_event-time_of_next_timer_b)>=0)
    {
      time_of_next_event=time_of_next_timer_b;
      event_vector=event_timer_b;
    }
  }
  // check timers for timeouts
  prepare_event_check_for_timer_timeout(MFP_TIMER_A);
  prepare_event_check_for_timer_timeout(MFP_TIMER_B);
  prepare_event_check_for_timer_timeout(MFP_TIMER_C);
  prepare_event_check_for_timer_timeout(MFP_TIMER_D);
#ifdef DEBUG_BUILD
  if(debug_run_until==DRU_CYCLE)
  {
    if((time_of_next_event-debug_run_until_val)>=0)
    {
      time_of_next_event=debug_run_until_val;
      event_vector=event_debug_stop;
    }
  }
#endif
#if USE_PASTI
  if((time_of_next_event-pasti_update_time)>=0)
  {
    time_of_next_event=pasti_update_time;
    event_vector=event_pasti_update;
  }
#endif
  // overhead added by Steem SSE
  if((time_of_next_event-Fdc.update_time)>=0)
  {
    time_of_next_event=Fdc.update_time;
    event_vector=event_wd1772;
  }
  if((time_of_next_event-FloppyDrive[DRIVE_A].time_of_next_ip)>=0
    && FloppyDrive[DRIVE_A].ImageType.Manager==MNGR_WD1772) //402R6
  {
    time_of_next_event=FloppyDrive[DRIVE_A].time_of_next_ip;
    event_vector=event_driveA_ip;
  }
  if((time_of_next_event-FloppyDrive[DRIVE_B].time_of_next_ip)>=0
    && FloppyDrive[DRIVE_B].ImageType.Manager==MNGR_WD1772) //402R6
  {
    time_of_next_event=FloppyDrive[DRIVE_B].time_of_next_ip;
    event_vector=event_driveB_ip;
  }
  if(time_of_next_event-time_of_event_acia>=0)
  {
    time_of_next_event=time_of_event_acia;
    event_vector=event_acia;
  }
  // It is safe for events to be in past, whatever happens events
  // cannot get into a constant loop.
  // If a timer is set to shorter than the time for an MFP interrupt then it will
  // happen a few times, but eventually will go into the future (as the interrupt can
  // only fire once, when it raises the IPL).
  int oo=(int)(time_of_next_event-sys_timer);
  // sys_timer must always be set to the next 4 cycle boundary after time_of_next_event
  //SS: this is still true after rounding refactoring
  //guess (!) it's because it enforces CPU R/W cycle, which is still 4 cycles
  //(clocks) in our reckoning, but still don't see how exactly
  oo=(oo+(4*TICKS8-1)) & -(4*TICKS8);
  sys_cycles+=oo;sys_timer+=oo;
}


#define LOGSECTION LOGSECTION_MFP_TIMERS

void handle_timeout(BYTE const tn) { // tn 0-3
  //TRACE_LOG2("handle_timeout(%d)\n",tn);
  a_s_t=A_S_T;
  if(mfp_timer_period_change[tn])
  {
    Mfp.CalcTimerPeriod(tn);
    mfp_timer_period_change[tn]=false;
    mfp_timer_check[tn]=mfp_timer_enabled[tn];
  }
  COUNTER_VAR new_timeout=mfp_timer_timeout[tn];
  COUNTER_VAR cmp;
  ASSERTB(mfp_timer_period[tn]>0);
#ifndef SSE_LEAN_AND_MEAN
  if(mfp_timer_period[tn]>0)
#endif
    do 
    {
      new_timeout+=mfp_timer_period[tn];
      cmp=new_timeout-a_s_t;
    } while(cmp<0 /*|| cmp==0&&cpu_cycles_multiplier<32.0*/);

  mfp_timer_period_current_fraction[tn]+=mfp_timer_period_fraction[tn]; 
  // this guarantees that we're always at the right cycle, despite
  // the inconvenience of a ratio
  if(mfp_timer_period_current_fraction[tn]>=MFP_TIMER_PRECISION)
  {
    mfp_timer_period_current_fraction[tn]-=MFP_TIMER_PRECISION;
    new_timeout++;
#if defined(SSE_DEBUGGER_FAKE_IO)
    if(TRACE_MASK2&(TRACE_CONTROL_IRQ_TA>>tn))
    {
      TRACE_LOG2("+1 ");
    }
#endif
  }
  Mfp.Counter[tn]=Mfp.reg[MFPR_TADR+tn]; // load counter
  BYTE prescale_index=(Mfp.GetTimerControlRegister(tn)&MFP_TIMER_DELAY_MASK);
  ASSERTB(prescale_index); // this assert found an issue
  Mfp.Prescale[tn]=(BYTE)mfp_timer_prescale[prescale_index]; // load prescale (bad if 0)
  Mfp.GetInterrupt(mfp_timer_irq[tn],mfp_timer_timeout[tn]);
  //ASSERT(new_timeout-A_S_T>0);
#if defined(SSE_DEBUGGER_FAKE_IO)
  if(TRACE_MASK2&(TRACE_CONTROL_IRQ_TA>>tn))
  {
    TRACE_LOG2(PRICV " Timer %c timeout, next " PRICV "\n",mfp_timer_timeout[tn],
      'A'+tn,new_timeout);
  }
#endif
  mfp_timer_timeout[tn]=new_timeout;
#if defined(SSE_STATS)
  Stats.nMfpTimeout[tn]++;
  DWORD divisor=Mfp.Prescale[tn]*BYTE_00_TO_256(Mfp.Counter[tn]);
  ASSERTB(divisor);
#ifndef SSE_LEAN_AND_MEAN
  if(divisor) // should be
#endif
  Stats.fTimer[tn]=Mfp.xtal/divisor; //Hz
#endif
}


void event_timer_a_timeout() {
  handle_timeout(MFP_TIMER_A);
}


void event_timer_b_timeout() {
  handle_timeout(MFP_TIMER_B);
}


void event_timer_c_timeout() {
  handle_timeout(MFP_TIMER_C);
}


void event_timer_d_timeout() {
  handle_timeout(MFP_TIMER_D);
}


#undef LOGSECTION
#define LOGSECTION LOGSECTION_INTERRUPTS

void event_timer_b() {
  Mfp.time_of_last_tb_tick=time_of_next_timer_b;
  if(OPTION_C3||scan_y<video_last_draw_line) 
  {
    if(Mfp.reg[MFPR_TBCR]==MFP_TIMER_EVENT_COUNT) 
    { // timer B tick
#if defined(SSE_STATS)
      Stats.nTimerbtick++;
#endif
#if defined(SSE_DEBUGGER_FRAME_REPORT)
      if(FRAME_REPORT_MASK2 & FRAME_REPORT_MASK_TIMER_B)
        FrameEvents.Add(scan_y,(short)(time_of_next_timer_b-LINECYCLE0)/TICKS8, "TB",
                        mfp_timer_counter[MFP_TIMER_B]/64);
#endif
      mfp_timer_counter[MFP_TIMER_B]-=64;
      Mfp.tbctr_old=Mfp.Counter[MFP_TIMER_B];
      Mfp.Counter[MFP_TIMER_B]--;
      if(mfp_timer_counter[MFP_TIMER_B]<64) 
      {
#if defined(SSE_STATS)
        Stats.nTimerb++;
#endif
#if defined(SSE_ENABLE_TRACE_LOG)
#if defined(SSE_DEBUGGER_FAKE_IO) //timers only when checked in mask
        if(TRACE_MASK2&TRACE_CONTROL_IRQ_TB)
#endif
          if(mfp_interrupt_enabled[8]) 
            TRACE_LOG("F%d y%d c%d Timer B pending\n",TIMING_INFO); //?
#endif
        mfp_timer_counter[MFP_TIMER_B]=BYTE_00_TO_256(Mfp.reg[MFPR_TBDR])*64;
        Mfp.Counter[MFP_TIMER_B]=Mfp.reg[MFPR_TBDR];
        Mfp.GetInterrupt(MFP_INT_TIMER_B,time_of_next_timer_b);
      }
/*  Besides generating a count pulse, the active transition of the auxiliary
    input signal will also produce an interrupt on the I3 or I4 interrupt
    channel, if the interrupt channel is enabled.
*/
      Mfp.GetInterrupt(MFP_INT_BLITTER,time_of_next_timer_b);
    }
  }
  time_of_next_timer_b+=EIGHT_MILLION; // put into future
  if(Mfp.reg[MFPR_AER]&MFP_GPIP_BLITTER_MASK)
    Glue.m_Status.timerb_start=1;
  else
    Glue.m_Status.timerb_end=1;
}

#undef LOGSECTION


void event_scanline_sub() {
/*  We take some tasks out of event_scanline(), so we can execute them from
    event_vbl_interrupt() too.
*/
#define LOGSECTION LOGSECTION_AGENDA
  // former CHECK_AGENDA macro
  if(++hbl_count==agenda_next_time)
  {
    if(agenda_length)
    {
      while((signed int)(hbl_count-agenda[agenda_length-1].time)>=0)
      {
        agenda_length--;
        TRACE_LOG("agenda execute #%d %p(%d)\n",agenda_length,
          agenda[agenda_length].perform,agenda[agenda_length].param);
        if(agenda[agenda_length].perform!=NULL) 
          agenda[agenda_length].perform(agenda[agenda_length].param);
        if(agenda_length)
          agenda_next_time=agenda[agenda_length-1].time;
        else
        {
          agenda_next_time=hbl_count-1; // wait 42 hours
          break;
        }
      }//wend
    }
  }
#undef LOGSECTION
#if defined(SSE_HD6301_LL)
  if(OPTION_C1)
  {
    //ASSERT(HD6301_OK);
    ASSERTB(stem_runmode==STEM_MODE_CPU); // this assert found an issue
    hd6301_run_cycles(A_S_T);
    if(Ikbd.Crashed)
    {
      TRACE("6301 CRASH\n");
      UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
      runstate=RUNSTATE_STOPPING;
    }
  }
#endif
#if defined(SSE_DISK_CAPS)
  if(Caps.Active==TRUE)
    Caps.Hbl();
#endif
  if(IS_STE && ste_sound_on_this_screen)
  {
#if defined(SSE_VID_STVL1)
    if(!OPTION_C3 || SSEConfig.Stvl<TStvl::VER_STESND)
#endif
      Mmu.SoundFetch();
    if(!(Glue.CurrentScanline.Tricks&TRICK_0BYTE_LINE)) // Fckcancr (on flicker!)
      Shifter.SoundPlay();
#if defined(SSE_VID_STVL1)
    if(!OPTION_C3 || SSEConfig.Stvl<TStvl::VER_STESND)
#endif
    if(!Glue.bFetchingLine) // no DE this line
      Mmu.SoundFetch(); // make sure FIFO is filled
  }
}


void CALLBACK event_scanline() {
  event_scanline_sub();
#if defined(SSE_VID_STVL1)
  if(OPTION_C3)
  { // copy scanline from STVL buffer to video memory, twice if line doubled
    time_of_next_event=A_S_T+Stvl.tick8*TICKS8;
    // timer B before scanline
    if(sys_cycles<=0 && event_vector==event_timer_b)
    {
#if defined(SSE_DEBUGGER_FAKE_IO)
      if(TRACE_MASK2&TRACE_CONTROL_EVENT)
        TRACE_EVENT("scanline",event_vector);
#endif
      event_vector();
    }
    for(int n=0;n<Stvl.hsync;n++) // in case of 'No Buddies Land' type 0byte lines
    if(draw_lock && Stvl.render_y>render_vstart && Stvl.render_y<=render_vend)
    {
      DWORD *source_start=Stvl.draw_mem_ptr_min+render_hstart;
      DWORD *source_end=source_start+render_scanline_length;
      // the checks are necessary, anything may happen
      if(draw_mem_line_ptr>=(DWORD*)draw_mem&&source_end<Stvl.draw_mem_ptr_max)
      {
        if(draw_mem_line_ptr+render_scanline_length<(DWORD*)Disp.VideoMemoryEnd)
          for(int i=0;i<render_scanline_length;i++)
            draw_mem_line_ptr[i]=source_start[i];
        draw_mem_line_ptr+=draw_line_pitch_dw;
        if(Draw.VerPix>1)
        {
          if(!SCANLINES_OK && draw_mem_line_ptr+render_scanline_length<(DWORD*)Disp.VideoMemoryEnd)
          //if(!SCANLINES_OK && draw_mem_line_ptr+render_scanline_length<(DWORD*)Draw.limit2)
          {
            for(int i=0;i<render_scanline_length;i++)
              draw_mem_line_ptr[i]=source_start[i];
          }
          draw_mem_line_ptr+=draw_line_pitch_dw;
        }
#if defined(SSE_VID_STVL_DBG)
        Stvl.dbg_npixels=0;
#endif
      }
    }
#if defined(SSE_DEBUGGER_TOPOFF) // many false alerts just like with C2
    if((DEBUGGER_CONTROL_MASK2&DEBUGGER_CONTROL_TOPOFF)
      && scan_y==-29 && !Stvl.vde && freq_change_this_scanline)
    {
      runstate=RUNSTATE_STOPPING;
      SET_WHY_STOP("Top off missed");
    }
    if((DEBUGGER_CONTROL_MASK2&DEBUGGER_CONTROL_BOTTOMOFF)
      && scan_y==200 && !Stvl.vde && freq_change_this_scanline)
    {
      runstate=RUNSTATE_STOPPING;
      SET_WHY_STOP("Bottom off missed");
    }
#endif
  }
  else
#endif
  if(scan_y>=draw_first_scanline_for_border && scan_y<draw_last_scanline_for_border)
  {
    if(bad_drawing==0) // !0 rarer than out of limits
      Shifter.DrawScanlineToEnd();
  }
  if(OPTION_C2)
  {
    if(Glue.bFetchingLine)
      Glue.EndHBL(); // check for +2 -2 errors + unstable Shifter
    // we check vertical overscan only on interesting scanlines
    if(Glue.CurrentScanline.Cycles>GLU_SCANLINE_CYCLES_72HZ
      && (scan_y==-30||scan_y==-1||scan_y==170
        ||scan_y==video_last_draw_line-1&& (scan_y<245||screen_res==HIRES)))
      Glue.CheckVerticalOverscan(); // check top & bottom borders
  }
  if(freq_change_this_scanline) 
  {
    if(OPTION_C3 || GlueFreqChangeTime[GlueFreqChangeIdx]<time_of_next_event-16*TICKS8
      && ShifterModeChangeTime[ShifterModeChangeIdx]<time_of_next_event-16*TICKS8)
      freq_change_this_scanline=false;
    if(draw_line_off)
    {
      palette_convert_all();
      draw_line_off=false;
    }
  }
  scanline_drawn_so_far=0;
#if 0 && defined(DEBUG_BUILD)
/*  Enforce register limitations, so that "report SDP" isn't messed up
    in the debug build.
*/
#if defined(SSE_MMU_MONSTER_ALT_RAM)
  if(mem_len<14*0x100000) 
#else
  if(mem_len<=MEM_4MB) 
#endif
    shifter_draw_pointer&=0x3FFFFE;
#endif
#if defined(SSE_VID_STVL1) 
  if(OPTION_C3)
    shifter_draw_pointer=VCountAtHSync=Stvl.vcount.d32;
  else
#endif
  if(Glue.bFetchingLine)
  {
#if 0
    //looks nice but takes more CPU power (another CheckSideOverscan round)
    Mmu.UpdateVideoCounter(LINECYCLES);
    shifter_draw_pointer=VCountAtHSync=Mmu.VideoCounter;
#else
    short added_bytes=Glue.CurrentScanline.Bytes;
    // extra words for HSCROLL are included in Bytes
    if(IS_STE && added_bytes && !Mmu.no_LW)
      added_bytes+=((WORD)LINEWID)<<1; 
    VCountAtHSync+=added_bytes;
#if defined(SSE_MMU_MONSTER_ALT_RAM)
    if(mem_len<MEM_14MB) 
#else
    if(mem_len<=MEM_4MB) 
#endif
      VCountAtHSync&=0x3FFFFE; // Leavin' Teramis
    shifter_draw_pointer=VCountAtHSync;
#endif
  }
  else 
    VCountAtHSync=shifter_draw_pointer;
  Mmu.VideoCounter=VCountAtHSync;
  TimeOfHSyncOff=time_of_next_event; 
#if defined(SSE_MFP_421)
    Glue.m_Status.timerb_start=Glue.m_Status.timerb_end=false;
#endif
#if defined(SSE_VID_STVL1)
  if(OPTION_C3)
  {
#if defined(SSE_DEBUGGER_FRAME_REPORT)
    if((FRAME_REPORT_MASK2 & FRAME_REPORT_MASK_SHIFTER_TRICKS_BYTES))
      FrameEvents.Add(scan_y,Stvl.video_linecycles,'#',Stvl.dbg_fetched_words*2);
#endif
    Glue.CurrentScanline.Cycles=Stvl.video_linecycles*TICKS8;
    scan_y++;
    Glue.bFetchingLine=Glue.FetchingLine();
  }
  else
#endif
    Glue.IncScanline(); // will call Shifter.IncScanline()
#if defined(SSE_DEBUGGER_FRAME_REPORT)
  if(Glue.bFetchingLine && (FRAME_REPORT_MASK1&FRAME_REPORT_MASK_VC_LINES)) 
    FrameEvents.Add(scan_y,0,"VC",shifter_draw_pointer); 
#endif
#ifdef DEBUG_BUILD
  if(debug_run_until==DRU_SCANLINE)
  {
    if(debug_run_until_val==scan_y)
    {
      if(runstate==RUNSTATE_RUNNING) 
      {
        runstate=RUNSTATE_STOPPING;
        runstate_why_stop="Run until";
      }
#if defined(SSE_DEBUGGER_FRAME_REPORT)
      FrameEvents.Report();
#endif
    }
  }
#endif
  Glue.hbl_pending=true;
  Glue.hbl_pending_time=TimeOfHSyncOff;
  update_ipl(Glue.hbl_pending_time);
#if defined(SSE_VID_DD_3BUFFER_WIN)
  // DirectDraw Check for Window VSync at each ST scanline!
  if(OPTION_3BUFFER_WIN&&!FullScreen)
    Disp.BlitIfVBlank();
#endif
  Glue.m_Status.hbi_done=false;
  Glue.m_Status.scanline_done=true;
  Glue.m_Status.vc_reload_done=false;
#if defined(SSE_VID_STVL1)
  if(OPTION_C3)
    prepare_next_event();
#endif
  //OsdControl.Trace ("VBI %d",FRAME);
}

#undef LOGSECTION
#define LOGSECTION LOGSECTION_VIDEO_RENDERING

// This happens about 60 cycles into scanline 247 (50Hz)
void event_start_vbl() {
  Glue.m_Status.vc_reload_done=true; // checked this line
#if defined(SSE_DEBUGGER_FRAME_REPORT)
  if(FRAME_REPORT_MASK1 & FRAME_REPORT_MASK_VIDEOBASE)
    FrameEvents.Add(scan_y,LINECYCLES/TICKS8,'r',VideoFreqIdx); // "reload"
#endif
  Glue.vsync=true;
#if defined(SSE_OSD_FPS_INFO)
  if(OPTION_OSD_FPSINFO)
    Debug.vcount_at_vsync=shifter_draw_pointer; // can be >vbase+32KB if overscan
#endif
  // As soon as VSYNC is asserted, the MMU keeps on copying VBASE to VCOUNT
  // We don't emulate the continuous copy but we copy at start and stop
  // (event_start_vbl(),event_trigger_vbi())
  Mmu.VideoCounter=shifter_draw_pointer=vbase;
  VCountAtHSync=shifter_draw_pointer;
  left_border=LeftBorderSize,right_border=RightBorderSize; // not always the same
  Glue.m_Status.vbl_done=false;
}


// called  after the last scanline of the frame, before the vertical interrupt
// STVL: called at vertical interrupt
void event_vbl_interrupt() {
#if defined(SSE_DONGLE)
/*  When pressing some button of his cartridge, the player triggered an MFP
    interrupt.
    By releasing the button, the concerned bit should change state. We use
    no counter but do it at first VBL for simplicity.
*/
  if(cart)
  {
    switch(DONGLE_ID) {
#if defined(SSE_DONGLE_URC) 
    case TDongle::URC:
      if(!(Mfp.reg[MFPR_GPIP]&MFP_GPIP_RING_MASK))
        mfp_gpip_set_bit(MFP_GPIP_RING_BIT,true); // Ultimate Ripper
      break;
#endif
#if defined(SSE_DONGLE_MULTIFACE) // cart + monochrome
    case TDongle::MULTIFACE:
      if(!(Mfp.reg[MFPR_GPIP]&MFP_GPIP_MONO_MASK))
        mfp_gpip_set_bit(MFP_GPIP_MONO_BIT,true);
      break;
#endif
    }
  }//if
#endif
#if defined(SSE_VID_STVL1)
  if(OPTION_C3)
    time_of_next_event=A_S_T+Stvl.tick8*TICKS8;
  else
#endif
/*  With GLU/video event refactoring, we call event_scanline() one time fewer,
    if we did now it would mess up some timings, so we call the sub
    with some HBL-dependent tasks: DMA sound, HD6301 & CAPS emu.
    Important for Relapse STE sound
*/
  {
    event_scanline_sub();
    Glue.VCount=0;
  }
#if defined(SSE_VID_DD_3BUFFER_WIN)
  BOOL bDisplayed=FALSE; // wasn't this missing before? (obsolete feature anyway)
#endif
#if defined(SSE_STATS)
#if defined(SSE_STATS_QP)
  LARGE_INTEGER tEmu1;
  QueryPerformanceCounter(&tEmu1);
  Stats.tEmu=(int)(tEmu1.QuadPart-Stats.tEmu0.QuadPart);
#else
  DWORD tEmu1=timeGetTime();
  Stats.tEmu=tEmu1-Stats.tEmu0; // +- emu time this frame 
#endif
  Stats.tEmuT+=Stats.tEmu; // add to total time
  Stats.tEmuN++; // add to #frames -> average=tEmuT/tEmuN
  Stats.tLockT+=Stats.tLock;
  Stats.tUnlockT+=Stats.tUnlock;
  Stats.tBlitT+=Stats.tBlit;
  Stats.tLock=Stats.tUnlock=Stats.tBlit=0;
#endif
#if defined(SSE_VID_D3D_VSYNC)
  MasterSync=(Draw.VSync)?Disp.FreqEmu:Glue.PreviousVideoFreq;
  bool VSyncing=!!Draw.VSync;
#else
  bool VSyncing=((OPTION_WIN_VSYNC&&!FullScreen||FSDoVsync&&FullScreen)
    && (fast_forward==0&&slow_motion==0));
#endif
  bool BlitFrame=false;

  DWORD myMasterSync=
#if defined(SSE_VID_OLDSYNC)
    (SSEOptions.OldSync)?Glue.VideoFreq: // "shifter_freq"
#endif
    MasterSync;

  if(!OPTION_C3&&!extended_monitor)
  {
    if(Draw.Buffered)
      draw_dest_ad=draw_store_dest_ad;
    scanline_drawn_so_far=0;
    VCountAtHSync=shifter_draw_pointer;
  }
  DiskEmu.Update(); // for sound, OSD
  //-------- display to screen -------



#if defined(SSE_TIMINGS_US)
  BOOL bDoPreciseTiming=(OPTION_MICROSECONDS && !VSyncing && frameskip==1
                        && !slow_motion && !fast_forward
#if defined(SSE_VID_OLDSYNC)
                        && !SSEOptions.OldSync
#endif    
    );
#endif

  if(draw_lock) 
  {
    draw_end();
#if defined(SSE_TIMINGS_US) // option us
    if(!bDoPreciseTiming)
#endif
    if(!VSyncing && !OPTION_3BUFFER_WIN)
      draw_blit();
    BlitFrame=true;
  }
  else if(bad_drawing&2) 
  {
    TRACE_LOG("F%d bad_drawing %d\n",FRAME,bad_drawing);
    // bad_drawing bits: & 1 - bad drawing option selected  & 2 - bad-draw next screen
    //                   & 4 - temporary bad drawing because of extended monitor.
    draw(false);
    bad_drawing&=(~2);
  }
  if(floppy_mediach[DRIVE_A]) 
    floppy_mediach[DRIVE_A]--;  //counter for media change
  if(floppy_mediach[DRIVE_B]) 
    floppy_mediach[DRIVE_B]--;  //counter for media change
#ifdef SHOW_DRAW_SPEED
  {
    HDC dc=GetDC(StemWin);
    if(dc!=NULL) {
      char buf[16];
      ultoa(avg_frame_time*10/NFRAME_TIME_AVG,buf,10);
      TextOut(dc,2,MENUHEIGHT+2,buf,strlen(buf));
      ReleaseDC(StemWin,dc);
    }
  }
#endif

  //------------ Shortcuts -------------
#if defined(SSE_EMU_THREAD)
  if(!OPTION_EMUTHREAD) // there's a timer 50ms
#endif
  if((--shortcut_vbl_count)<0) 
  {
    ShortcutsCheck();
    shortcut_vbl_count=SHORTCUT_VBLS_BETWEEN_CHECKS;
  }

  //------------- Auto Frameskip Calculation -----------
  if(frameskip==AUTO_FRAMESKIP) 
  { //decide if we are ahead of schedule
#if defined(SSE_VID_OLDSYNC)
    if(SSEOptions.OldSync)
    { // copied from Steem 3.2+
      if (fast_forward==0 && slow_motion==0 && VSyncing==0){
        timer=timeGetTime();
        if (timer<auto_frameskip_target_time){
          frameskip_count=1;   //we are ahead of target so draw the next frame
          speed_limit_wait_till=auto_frameskip_target_time;
        }else{
          auto_frameskip_target_time+=(run_speed_ticks_per_second+(Glue.VideoFreq/2))/Glue.VideoFreq;
        }
      }else if (VSyncing){
        frameskip_count=1;   //disable auto frameskip
        auto_frameskip_target_time=timer;
      }
    }
    else
#endif    
    if(!VSyncing) 
    {
      timer=timeGetTime();
      if(timer<auto_frameskip_target_time) 
      {
        frameskip_count=1;   //we are ahead of target so draw the next frame
        speed_limit_wait_till=auto_frameskip_target_time;
      }
      else
        auto_frameskip_target_time+=(run_speed_ticks_per_second
         +(MasterSync/2))/MasterSync;
    }
    else if(VSyncing) 
    {
      frameskip_count=1;   //disable auto frameskip
      auto_frameskip_target_time=timer;
    }
  }//if(frameskip==AUTO_FRAMESKIP) 
#if defined(SSE_TIMINGS_US)
  int time_for_exact_limit=
#if defined(SSE_VID_OLDSYNC)
    (SSEOptions.OldSync)?1:
#endif
    OPTION_TIMINGLOOP;
#else
  int time_for_exact_limit=1;
#endif
  // Work out how long to wait until we start next screen
  if(slow_motion
#ifdef WIN32
#ifdef SSE_421R0 // one click for one frame
     && (SendMessage(GetDlgItem(StemWin,IDC_PLAY),BM_GETCLICKBUTTON,0,0)==BM_CLICKBUTTON_RIGHT)
#endif
#endif
     )
  {
    int i=(int)((cut_slow_motion_speed)?cut_slow_motion_speed:slow_motion_speed);
    if(i)
      frame_delay_timeout=timer+(1000000/i)/MasterSync;
    auto_frameskip_target_time=timer;
    frameskip_count=1;
  }
  else if((frameskip_count<=1||fast_forward)&&!disable_speed_limiting)
  {
    frame_delay_timeout=speed_limit_wait_till;
    if(VSyncing)
    {
      // Allow up to a 25% increase in run speed
      time_for_exact_limit=((run_speed_ticks_per_second+(myMasterSync/2))/myMasterSync)/4;
    }
  }
  else
    frame_delay_timeout=timer;
  //--------- Look for Windows messages ------------
  // not necessary if emu thread active
  int old_slow_motion=slow_motion;
  int m=0;

  for(;;)
  {
    timer=timeGetTime();
#if defined(SSE_VID_DD_3BUFFER_WIN)
    if(OPTION_3BUFFER_WIN&&!FullScreen&&!bDisplayed)
      bDisplayed=Disp.BlitIfVBlank();
#endif
    // Break if used up enough time and processed at least 3 messages
    if((int)(frame_delay_timeout-timer)<=time_for_exact_limit && m>=3)
      break;

    // Get next message from the queue, if none left then go to the Sleep
    // routine in Windows to give other processes more time. Also do that
    // if more than 15 messages have been retrieved.
    // Don't go to Sleep if slow motion is on, that way the message to turn
    // it off can be dealt with instantly.
    // emu thread: take 1 max in case main loop is stuck (test)
    if(OPTION_EMUTHREAD||PeekEvent()==PEEKED_NOTHING||m>15)
    {
      // Get more than 15 messages if slow motion is on, otherwise GUI will lock up
      // Should really do something to stop high CPU load when slow_motion is on
      if(slow_motion==0) 
      {
        if(old_slow_motion)
        {
          // Don't sleep if you just turned slow motion off (stops annoying GUI delay)
          frame_delay_timeout=timer;
        }
        break;
      }
    }
    m++;
  }
  if(new_n_cpu_cycles_per_second) 
  {
    if(new_n_cpu_cycles_per_second!=nSysCyclesPerSecond) 
    {
      nSysCyclesPerSecond=new_n_cpu_cycles_per_second;
      AdaptCpuBoost();
    }
    new_n_cpu_cycles_per_second=0;
  }
  // cycles for one full frame
  Glue.nFrameCycles=Glue.nLines*CyclesPerScanline[VideoFreqIdx];
  // Eat up remaining time-time_for_exact_limit with Sleep
  int time_to_sleep=0; // in ms
#if defined(SSE_TIMINGS_US)
  LARGE_INTEGER Now;
  if(bDoPreciseTiming)
  {
    // convert to PC ticks: *timer clock/ST clock
    LONGLONG PCticks=Glue.nFrameCycles*MicroTime.Frequency.QuadPart;
    PCticks/=nSysCyclesPerSecond;
#if defined(SSE_VID_BFI) // BFI also for FreeSync
    if(OPTION_BFI)
    {
      MicroTime.BfiStartBlit=MicroTime.StartBlit.QuadPart+PCticks;
      PCticks/=2; // must be half or we don't have a real refresh rate, HW could be confused
    }
#endif
    // compute delay until start blit: last blit + converted ST cycles - now
    MicroTime.StartBlit.QuadPart+=PCticks;
    QueryPerformanceCounter(&Now);
    LONGLONG SleepTime=MicroTime.StartBlit.QuadPart-Now.QuadPart;
    LONGLONG SleepTimeMs=MicroTime.Ms(SleepTime);
    time_to_sleep=(int)SleepTimeMs-OPTION_TIMINGLOOP;
    time_to_sleep=MIN(time_to_sleep,20);
  }
  else if(!Draw.VSync
#if defined(SSE_VID_OLDSYNC)
    ||SSEOptions.OldSync
#endif
    )
#endif
  {
    time_to_sleep=(int)frame_delay_timeout-(int)timeGetTime()-time_for_exact_limit;
    //TRACE_OSD_DBG("tts%d",time_to_sleep);
  }
  if(time_to_sleep>0)
  {
#if defined(SSE_VID_DD_3BUFFER_WIN)
/*  This is the part responsible for high CPU use.
    Maybe check probability of VBLANK but it depends on Hz
*/
    if(OPTION_3BUFFER_WIN&&!FullScreen
#if defined(SSE_VID_OLDSYNC)
      &&!SSEOptions.OldSync
#endif      
      )
    {
      int limit=(int)(frame_delay_timeout)-(int)(time_for_exact_limit);
      do {
        if(!bDisplayed)
          bDisplayed=Disp.BlitIfVBlank();
      } while((int)(timeGetTime())<limit);
    }
    else
#endif
      Sleep((DWORD)time_to_sleep); // it is very approximate
  }
#if defined(SSE_STATS)
  else if(!fast_forward && !Draw.VSync)
  {
    Stats.nSlowdown++;
#if 0//defined(SSE_BETA) && defined(SSE_OSD_DEBUGINFO) // can "pollute" other messages
    TRACE_LOG("F%d slow-down %d\n",FRAME,time_to_sleep);
    if(OsdControl.TraceTo)
    {
      TRACE_OSD_DBG("LAG %d %d",1,time_to_sleep);
    }
#endif
  }
#endif
  DWORD now=timeGetTime(); // take time before vsync
  if(VSyncing && BlitFrame) 
  {
#if defined(SSE_VID_DD)
    Disp.VSync();
#endif
#ifndef WIN32
    timer=timeGetTime(); // useless?
#endif
#if defined(SSE_VID_OLDSYNC)
    if(SSEOptions.OldSync)
      timer=timeGetTime(); // we don't do VSync like in old DD builds anyway but...
#endif
#if defined(SSE_VID_D3D_VSYNC)
/*  If PC display is at 100Hz and the ST at 50Hz, we blit twice.
    At least it works on my system, contrary to D3DPRESENT_INTERVAL_TWO.
    If option BFI is on, first blit is a black frame due to sprite mask.
    We don't split emu time (more involved, more CPU...)
*/
    if(Disp.Freq>Disp.FreqEmu)
    {
      int nblack=Disp.Freq/Disp.FreqEmu-1;
      BYTE save=video_mixed_output;
      video_mixed_output=0; // no interference with the countdown
      for(int i=0;i<nblack;i++)
        draw_blit(OPTION_BFI&&!i); // !i -> only one black frame max ?
      video_mixed_output=save;
    }
#endif
    draw_blit();
#if defined(SSE_STATS)
#ifdef WIN32
    Stats.tEmu0=Stats.tBlit1; // spare one QPC, we're very close
#else
    Stats.tEmu0=timeGetTime(); // update after vsync
#endif
#endif
  }
  else
  {
#if defined(SSE_STATS)
#if defined(SSE_STATS_QP)
    QueryPerformanceCounter(&Stats.tEmu0);
#else
    Stats.tEmu0=now;
#endif
#endif
    // Wait until desired time (to nearest 1000th of a second).
#if defined(SSE_TIMINGS_US)
/*  Even if we use a long loop, the thread may be preempted anytime,
    and if the system is busy we'll miss the timing regardless, we can't
    make a miracle here, the General option Make Steem high priority 
    has a greater impact in such a case, this is by design of a multitask OS.
*/
    if(bDoPreciseTiming) // us
    {  // hard precise loop
#if defined(SSE_VID_BFI)
      if(OPTION_BFI)
      {
        do
          QueryPerformanceCounter(&Now);
        while(MicroTime.StartBlit.QuadPart-Now.QuadPart>0);
        draw_blit(TRUE);
        MicroTime.StartBlit.QuadPart=MicroTime.BfiStartBlit;
        QueryPerformanceCounter(&Now);
        LONGLONG SleepTime=MicroTime.StartBlit.QuadPart-Now.QuadPart;
        LONGLONG SleepTimeMs=MicroTime.Ms(SleepTime);
        time_to_sleep=(int)SleepTimeMs-OPTION_TIMINGLOOP;
        if(time_to_sleep>0)
          Sleep(time_to_sleep);
      }
#endif
      QueryPerformanceCounter(&Now);
#if defined(SSE_BETA) // spot lag
      LONGLONG lag=Now.QuadPart-MicroTime.StartBlit.QuadPart;
      if(lag>0)
      {
        lag=MicroTime.Us(lag);
        int ilag=(int)lag;
        TRACE_LOG("F%d TimingLoop LAG %d\n",FRAME,ilag);
        if(OPTION_OSD_DEBUGINFO)
        {
          //TRACE_OSD_DBG("LAG %d %d %d",2,time_to_sleep,ilag);
        }
      }
#endif
      while(MicroTime.StartBlit.QuadPart-Now.QuadPart>0) {
        QueryPerformanceCounter(&Now);
#if defined(SSE_VID_DD_3BUFFER_WIN)
        if(OPTION_3BUFFER_WIN&&!FullScreen&&!bDisplayed)
          bDisplayed=Disp.BlitIfVBlank();
#endif
      }//wend
      MicroTime.StartBlit=Now; // update (could be much later)
    }
    else
#endif
    {

#if defined(SSE_BETA_) // spot lag
      //timer=timeGetTime();
#if defined(SSE_OSD_DEBUGINFO)
//      if(OsdControl.TraceTo && ((int)frame_delay_timeout-(int)timer)<0)
//        TRACE_OSD_DBG("LAG %d %d",2,((int)frame_delay_timeout-(int)timer));
#endif
      while(((int)frame_delay_timeout-(int)timer)>0)
      {
        timer=timeGetTime();
#if defined(SSE_VID_DD_3BUFFER_WIN)
        if(OPTION_3BUFFER_WIN&&!FullScreen&&!bDisplayed)
          bDisplayed=Disp.BlitIfVBlank();
#endif
      }
#else
      do {
        timer=timeGetTime();
#if defined(SSE_VID_DD_3BUFFER_WIN)
        if(OPTION_3BUFFER_WIN&&!FullScreen&&!bDisplayed)
          bDisplayed=Disp.BlitIfVBlank();
#endif
      } while(((int)frame_delay_timeout-(int)timer)>0);
      //} while((int)(frame_delay_timeout-timer)>0);
#endif

    }
    // For some reason when we get here timer can be > frame_delay_timeout, even if
    // we are running very fast. This line makes it so we don't lose a millisecond
    // here and there.
    if(time_to_sleep>0) 
      timer=frame_delay_timeout;

  }
#if defined(SSE_EMU_THREAD)
  if(OPTION_EMUTHREAD && runstate!=RUNSTATE_STOPPED)
  {
    if(VideoLock.blocked)
      VideoLock.Acknowledge();
    if(DiskLock.blocked)
      DiskLock.Acknowledge();
  }
#endif
#if defined(SSE_TIMINGS_US) // option us
  if(bDoPreciseTiming && BlitFrame && !VSyncing && !OPTION_3BUFFER_WIN)
    draw_blit();
#endif
  //-------------- Pause Steem -------------
  if(GUIPauseWhenInactive()) 
  {
    timer=timeGetTime();
    avg_frame_time_timer=timer;
    avg_frame_time_counter=0;
    auto_frameskip_target_time=timer;
  }
  if(floppy_access_ff_counter>0) 
  {
    floppy_access_ff_counter--;
    if(fast_forward==0 && floppy_access_ff_counter>0)
    {
      fast_forward_change(true,false);
      floppy_access_started_ff=true;
    }
    else if(fast_forward && floppy_access_ff_counter==0 && floppy_access_started_ff)
      fast_forward_change(false,false);
  }
  //--------- Work out avg_frame_time (for OSD) ----------
  if(avg_frame_time==0)
    avg_frame_time=(timer-avg_frame_time)*NFRAME_TIME_AVG;
  else if(++avg_frame_time_counter>=NFRAME_TIME_AVG)
  {
    avg_frame_time=timer-avg_frame_time_timer; //take average of frame time over 12 frames, ignoring the time we've skipped
    avg_frame_time_timer=timer;
    avg_frame_time_counter=0;
  }
#if !defined(SSE_NO_OLD_IKBD)
  if(!OPTION_C1 && NumJoysticks)
    JoyGetPoses(); // Get the positions of all the PC joysticks
#endif
  if(slow_motion) 
  {
    // Extra screenshot check (so you actually take a picture of what you see)
    frameskip_count=0;
    ShortcutsCheck();
    if(DoSaveScreenShot&1) 
    {
      Disp.SaveScreenShot();
      DoSaveScreenShot&=~1;
    }
    // without this, Steem will make up the slow down as soon as slow_motion is cleared!
    start_of_this_second=timer;
    nframes_this_second=0;
  }
  IKBD_VBL();    // Handle ST joysticks and mouse
#if defined(SSE_HD6301_LL)
  if(OPTION_C1)
    Ikbd.Vbl();
#endif
  RS232_VBL();   // Update all flags, check for the phone ringing
  SoundVBL();   // Write a VBLs worth + a bit of samples to the sound card
#if defined(SSE_DRIVE_SOUND)
/*  We don't check the option here because we may have to suddenly stop
    motor sound loop.
*/
  FloppyDrive[DRIVE_A].SoundVBL();
  FloppyDrive[DRIVE_B].SoundVBL();
#endif
#if defined(SSE_EMU_THREAD)
  if(OPTION_EMUTHREAD && runstate!=RUNSTATE_STOPPED && SoundLock.blocked)
    SoundLock.Acknowledge();
#endif
  SteSoundChannelBufIdx=0;  //need to maintain this even if sound off
  ste_sound_on_this_screen=(Mmu.SoundControl&TMmu::SOUNDPLAY)||Shifter.SoundFifoIdx;
  //---------- Frameskip -----------
  if(--frameskip_count<=0) 
  {
    if(runstate==RUNSTATE_RUNNING && !bAppMinimized)
    {
#ifndef NO_CRAZY_MONITOR
      if(extended_monitor) 
      {
        //bad_drawing bits: &1 - bad drawing option selected  &2 - bad-draw next screen
        //                  &4 - temporary bad drawing because of extended monitor.
        bad_drawing|=6;
        //        if(!FullScreen && em_needs_fullscreen)bad_drawing=4;
      }
      else
#endif
      {
        bad_drawing&=3;
        if(bad_drawing&1)
          bad_drawing|=3;
        else if(Glue.m_Status.stop_emu!=1||!OPTION_NO_OSD_ON_STOP)
          draw_begin();
      }
    }
    if(fast_forward)
      frameskip_count=20;
    else 
    {
      int fs=frameskip;
      if(fs==AUTO_FRAMESKIP && VSyncing) 
        fs=1;
      frameskip_count=fs;
#if defined(SSE_VID_OLDSYNC)
      if(SSEOptions.OldSync)
      { // copied from Steem 3.2+
        if (fs==AUTO_FRAMESKIP){
          auto_frameskip_target_time=timer+((run_speed_ticks_per_second+(Glue.VideoFreq/2))/Glue.VideoFreq);
          speed_limit_wait_till=auto_frameskip_target_time;
        }else{
          speed_limit_wait_till=timer+((fs*(run_speed_ticks_per_second+(Glue.VideoFreq/2)))/Glue.VideoFreq);
        }
      }
      else
#endif
      if(fs==AUTO_FRAMESKIP) 
      {
        if(now-start_of_this_second>=1000)
        {
#if defined(SSE_OSD_FPS_INFO)
          if(OPTION_OSD_FPSINFO)
          {
            WORD fps=(WORD)(nframes_this_second-Debug.frame_no_change);
            if(fps)
            {
              TRACE_OSD_FPS("%d%s",fps,"FPS");
            }
#if defined(SSE_STATS)
            Stats.nFps=(Stats.nFps) ? (Stats.nFps+fps)/2 : fps;
#endif
          }
          Debug.frame_no_change=0;
#endif
          nframes_this_second=0;
          start_of_this_second=now;
        }
        nframes_this_second++;
        auto_frameskip_target_time=timer+((run_speed_ticks_per_second+(myMasterSync/2))/myMasterSync);
        speed_limit_wait_till=auto_frameskip_target_time;
      }
      else 
      {
        if(frameskip==1)
        {
          int lag=0;
          if(nframes_this_second>=myMasterSync)
          {
            lag=now-start_of_this_second-run_speed_ticks_per_second;
            if(VSyncing)
            {
#if defined(SSE_OSD_FPS_INFO)
              if(OPTION_OSD_FPSINFO)
              {
                WORD fps=(WORD)(nframes_this_second-Debug.frame_no_change);
                if(fps)
                {
                  TRACE_OSD_FPS("%d%s",fps,"FPS");
                }
#if defined(SSE_STATS)
                Stats.nFps=(Stats.nFps) ? (Stats.nFps+fps)/2 : fps;
#endif
              }
#endif
              //start_of_this_second=timer;
              start_of_this_second=now; //leeway?
            }
            else //!(VSyncing)
            {
#if defined(SSE_OSD_FPS_INFO)
              if(OPTION_OSD_FPSINFO)
              {
                WORD fps=(WORD)(nframes_this_second-Debug.frame_no_change);
                if(fps)
                {
                  TRACE_OSD_FPS("%d%s",fps,"FPS");
                }
#if defined(SSE_STATS)
                Stats.nFps=(Stats.nFps) ? (Stats.nFps+fps)/2 : fps;
#endif
              }
#endif
              // very precise, lose no ms
              start_of_this_second+=run_speed_ticks_per_second;
              // but give up if >1s (emu probably stopped by external cause)
              if(lag>run_speed_ticks_per_second) 
              {
                start_of_this_second=now;
                lag=0; 
              }
            }
            nframes_this_second=0;
#if defined(SSE_OSD_FPS_INFO)            
            Debug.frame_no_change=0;
#endif          
          }
#if defined(SSE_ENABLE_TRACE_LOG)
          else if(now-start_of_this_second>=(DWORD)run_speed_ticks_per_second)
          {
            TRACE_LOG("frame lag %d\n",now-start_of_this_second-(DWORD)run_speed_ticks_per_second);
          }
#endif
          nframes_this_second++;
          if(VSyncing)
          {
            speed_limit_wait_till=timer+(run_speed_ticks_per_second/myMasterSync);
          }
          else
          {
            DWORD from_start=((fs*run_speed_ticks_per_second
              *(nframes_this_second))/myMasterSync)-lag;
            speed_limit_wait_till=start_of_this_second+from_start;
          }
#if 1 //?
          if(Glue.VideoFreq!=MONO_HZ)
            speed_limit_wait_till--;
#endif
        }
        else
        {
          nframes_this_second++;
          if(now>=start_of_this_second+run_speed_ticks_per_second)
          {
#if defined(SSE_OSD_FPS_INFO)
            if(OPTION_OSD_FPSINFO)
            {
              WORD fps=(WORD)(nframes_this_second-Debug.frame_no_change);
              if(fps)
                TRACE_OSD_FPS("%d%s",fps,"FPS");
#if defined(SSE_STATS)
              Stats.nFps=(Stats.nFps) ? (Stats.nFps+fps)/2 : fps;
#endif
              Debug.frame_no_change=0;
            }
#endif
            nframes_this_second=0;
            start_of_this_second=now;
          }
          speed_limit_wait_till=timer+((fs*run_speed_ticks_per_second)/myMasterSync);
        }
      }
    }
  }
  if(fast_forward)
    speed_limit_wait_till=timer+(fast_forward_max_speed/myMasterSync);
/*   // liable to hang and not used one single time!
  // The MFP clock aligns with the CPU clock every 8000 CPU cycles
  while(abs_quick(ABSOLUTE_SYS_TIME-sys_time_of_first_mfp_tick)>160000*TICKS8)
    sys_time_of_first_mfp_tick+=160000*TICKS8;
    */
  //while(abs_quick(ABSOLUTE_SYS_TIME-shifter_cycle_base)>160000*TICKS8)
    //shifter_cycle_base+=160000; //SS 60000?
#if !defined(SSE_NO_OLD_IKBD)
  if(!OPTION_C1)
  {
    if(abs_quick(ABSOLUTE_SYS_TIME-shifter_cycle_base)>160000*TICKS8)
      shifter_cycle_base=ABSOLUTE_SYS_TIME; // not dangerous... TODO
  }
#endif
  shifter_tick8=(HSCROLL>>Shifter.ShiftMode);
  left_border=LeftBorderSize;
  right_border=RightBorderSize;
  scanline_drawn_so_far=video_first_draw_line=0;
  video_last_draw_line=shifter_y;
#if !defined(SSE_NO_FALCONMODE)
  if(emudetect_falcon_mode && emudetect_falcon_extra_height) 
  {
    video_first_draw_line=-20;
    video_last_draw_line=320;
  }
#endif
  if((Shifter.ShiftMode&2) && screen_res<HIRES) // LEGACY My socks are weapons
  {
    video_last_draw_line*=2; //400 fetching lines
    memset(PCpal,0,sizeof(LONG)*PAL_SIZE); // all colours black
  }
  Glue.Vbl();
  VideoFreqAtStartOfVbl=Glue.VideoFreq;
  //ASSERT(VideoFreqAtStartOfVbl!=GLUE_HZ71);
  CyclesPerScanlineAtStartOfVbl=CyclesPerScanline[VideoFreqIdx];
  sys_time_of_last_vbl=time_of_next_event;
  PasteVBL();
  ONEGAME_ONLY(OGVBL(); )
#ifdef DEBUG_BUILD
  if(debug_run_until==DRU_VBL||(debug_run_until==DRU_SCANLINE && debug_run_until_val==scan_y)) {
    if(runstate==RUNSTATE_RUNNING) 
    {
      runstate=RUNSTATE_STOPPING;
      runstate_why_stop="Run until";
    }
  }
#if !defined(SSE_DBG_NOSENDKEYS)
  debug_vbl();
#endif
#endif
  Debug.Vbl(); // Debug.Vbl() is different from debug_vbl()!
  Cpu.UpdateCyclesForEClock(); // refresh
#if defined(SSE_VID_STVL1)
  if(OPTION_C3)
    prepare_next_event();
#endif
}


void AdaptCpuBoost() {
  n_millions_cycles_per_sec=(nSysCyclesPerSecond/TICKS8)/1000000;
  int factor=n_millions_cycles_per_sec;
  cpu_cycles_multiplier=(double)factor/(8);
  SSEConfig.CpuBoost=(int)cpu_cycles_multiplier;
  SSEConfig.CpuBoosted=(SSEConfig.CpuBoost > 1);
  for(int idx=0;idx<3;idx++) //3 frequencies
    CyclesPerScanline[idx]=(int)(CyclesPerScanline8MHz[idx]*cpu_cycles_multiplier);
  Mfp.InitTimers();
  SetTimingFunctions();
  if(runstate==RUNSTATE_RUNNING) 
    prepare_next_event();
  CheckResetDisplay();
}


#if USE_PASTI

void event_pasti_update() {
  if(!(hPasti && (pasti_active || FloppyDrive[DRIVE].ImageType.Extension==EXT_STX)))
  {
    pasti_update_time=time_of_next_event+EIGHT_MILLION;
    return;
  }
  struct pastiIOINFO pioi;
  pioi.stPC=pc;
  pioi.cycles=time_of_next_event/TICKS8;
  pasti->Io(PASTI_IOUPD,&pioi);
  pasti_handle_return(&pioi);
}

#endif


#ifdef DEBUG_BUILD

void event_debug_stop() {
  if(runstate==RUNSTATE_RUNNING)
  {
    runstate=RUNSTATE_STOPPING;
    runstate_why_stop="Run until";
  }
  debug_run_until=DRU_OFF; // Must be here to prevent freeze up as this event never goes into the future!
}

#endif


// SSE added events

void CALLBACK event_trigger_vbi() { //6X cycles into frame (colour)
  ASSERTB(Glue.VCount==0||OPTION_C3);
#if defined(SSE_OSD_FPS_INFO)
  if(OPTION_OSD_FPSINFO)
    Debug.vbase_at_vbi=vbase; // program can change vbase at end of frame
#endif
  // VideoFreqIdx and video_freq are incorrect if mode=2 on colour screen
  // at least start the frame with correct video freq
  BYTE const idx=(BYTE)((Glue.ShiftMode&HIRES) ? Glue.FREQ_IDX_71 : 
    ( (Glue.SyncMode&Glue.SYNCPAL) ? Glue.FREQ_IDX_50 : Glue.FREQ_IDX_60 ));
  Glue.VideoFreq=Glue.Freq[idx];
  ASSERTB(Glue.VideoFreq);
  //scan_y=-TopScanlines[idx];
  VideoFreqIdx=idx;
  // note: now we know that it's not a down counter on real HW but anyways, it works
  // Glue.nLines is used for microseconds timings, option C3 or not
  if(Glue.ShiftMode&HIRES) // 71hz (monochrome)
  {
    Glue.nLines=GLU_MONO_SCANLINES; //501
    Glue.de_start_line=TopScanlines[Glue.FREQ_IDX_71];
    Glue.de_end_line=Glue.de_start_line+400-1;
  }
  else 
  {
    if(Glue.SyncMode&Glue.SYNCPAL) // 50hz
      Glue.nLines=GLU_PAL_SCANLINES; //313
    else // 60hz
      Glue.nLines=GLU_NTSC_SCANLINES; //263
    Glue.de_start_line=TopScanlines[VideoFreqIdx];
    Glue.de_end_line=Glue.de_start_line+200-1;
  }
#if defined(SSE_VID_STVL1) 
  if(OPTION_C3)
  {
    // blit the frame, set vbi pending
    // delete rest of 60hz or 71hz screen, because our rendering
    // surface has just too many lines for those frequencies
    if(Stvl.framefreq!=50 && border
      && draw_mem_line_ptr>(DWORD*)draw_mem
      //&& draw_mem_line_ptr<(DWORD*)Draw.limit2)
      && draw_mem_line_ptr<(DWORD*)Disp.VideoMemoryEnd)
    {      
      for(DWORD* i=draw_mem_line_ptr;i<(DWORD*)Disp.VideoMemoryEnd;i++)
      //for(DWORD* i=draw_mem_line_ptr;i<(DWORD*)Draw.limit2;i++)
        *i=0;
    }
    if(Stvl.framefreq && Glue.PreviousVideoFreq!=Stvl.framefreq)
    {
      StvlUpdate();
      Glue.PreviousVideoFreq=Stvl.framefreq;
#if defined(SSE_VID_D3D_VSYNC)
      if(OPTION_AUTOVSYNC&&!FullScreen||FullScreen&&OPTION_AUTOVSYNC_FS&&OPTION_FAKE_FULLSCREEN)
        Disp.D3DCreateSurfaces(); // probably switch on/off autovsync, too bad if there's a glitch
#endif
      UPDATE_STATUS_BAR_PART(SB_PART_FREQ);
#if !defined(SSE_GUI_421_REORDER)
      OptionBox.UpdateSTVideoPage();
#endif
      draw_grille_black=4;
    }
    Stvl.render_y=0;
    draw_mem_line_ptr=(DWORD*)draw_mem; // Disp.DrawToVidMem is ignored
    event_vbl_interrupt();
    Glue.vbl_pending=true;
    Glue.vbl_pending_time=A_S_T+Stvl.tick8*TICKS8;
    update_ipl(Glue.vbl_pending_time);
    scan_y=-TopScanlines[idx];
    return;
  }
  else
#endif
  {
    if(Glue.PreviousVideoFreq!=Glue.Freq[idx])
    {
      //TRACE_LOG("(%d->%d) ",Glue.PreviousVideoFreq,Glue.Freq[idx]);
      Glue.PreviousVideoFreq=Glue.Freq[idx];
#if defined(SSE_VID_D3D_VSYNC)
      if(OPTION_AUTOVSYNC&&!FullScreen||FullScreen&&OPTION_AUTOVSYNC_FS&&OPTION_FAKE_FULLSCREEN)
        Disp.D3DCreateSurfaces(); // probably switch on/off autovsync, too bad if there's a glitch
#endif
      init_screen();
      UPDATE_STATUS_BAR_PART(SB_PART_FREQ); // new frequency in status bar
#if !defined(SSE_GUI_421_REORDER)
      OptionBox.UpdateSTVideoPage(); // new frequency in option box
#endif
      draw_grille_black=4;
    }
  }
  //ASSERT(!Glue.m_Status.vbi_done);
  // As soon as VSYNC is asserted, the MMU keeps on copying VBASE to VCOUNT
  // We don't emulate the continuous copy but we copy at start and stop
  // (event_start_vbl(),event_trigger_vbi())
  Glue.vsync=false;
  Mmu.VideoCounter=VCountAtHSync=shifter_draw_pointer=vbase;
#if defined(SSE_HARDWARE_OVERSCAN)
  // hack to get correct display
  if(OPTION_HWOVERSCAN && SSEConfig.OverscanOn)
  {
    int off=0;
    if(COLOUR_MONITOR)
    {
      if(VideoFreqAtStartOfVbl==PAL_HZ)
        off=(OPTION_HWOVERSCAN==LACESCAN) ? (27*236-8*3) : (23*224+2*8); 
      // and 236*24-80+22+8+8+8+8+8 for other "generic" overscan circuit
      else //TODO
        off=(OPTION_HWOVERSCAN==LACESCAN) ? (20*234-8*3) : (16*224+2*8);
      if(border>=2)
        off+=8;
    }
    SHIFT_SDP(off);
    Mmu.VideoCounter=shifter_draw_pointer;
  }
#endif
  Glue.vbl_pending=true;
  Glue.vbl_pending_time=time_of_next_event;
  update_ipl(Glue.vbl_pending_time);
  Glue.m_Status.vbi_done=true;
  scan_y=-Glue.de_start_line;
}


/*  There's an event for floppy (STW etc.) because we want to handle DRQ for each
    byte, and the resolution of HBL is too gross for that:

    6256 bytes/ track , 5 revs /s= 31280 bytes
    1 second= 8021248 CPU cycles in our emu
    8021248/31280 = 256,433 cycles / byte
    8000000/31280 = 255,754 cycles / byte
    One HBL= 512 cycles at 50hz.

    Caps works with HBL because it hold its own cycle count.
    
    Here we should transfer control, or dispatch to handlers
*/

void event_wd1772() {
  Fdc.current_time=time_of_next_event;
  Fdc.OnUpdate();
}


void event_driveA_ip() {
  FloppyDrive[DRIVE_A].IndexPulse();
}


void event_driveB_ip() {
  FloppyDrive[DRIVE_B].IndexPulse();
}


//  ACIA event to handle IO with both 6301 and MIDI
// TODO risk of simultanate events?

COUNTER_VAR time_of_event_acia;

void event_acia() {
  if(OPTION_C1)
  {
    // find ACIA event to run
    // start transmission
//    ASSERT(time_of_event_acia==time_of_next_event);
    if(acia[ACIA_IKBD].LineTxBusy==2 && time_of_event_acia==acia[ACIA_IKBD].TimeTx)
      acia[ACIA_IKBD].TransmitTDR();
    else if(acia[ACIA_MIDI].LineTxBusy==2 && time_of_event_acia==acia[ACIA_MIDI].TimeTx)
      acia[ACIA_MIDI].TransmitTDR();
    // IKBD
    else if(acia[ACIA_IKBD].LineRxBusy==1 && time_of_event_acia==acia[ACIA_IKBD].TimeRx)
      agenda_keyboard_replace(0); // from IKBD
    else if(acia[ACIA_IKBD].LineTxBusy && time_of_event_acia==acia[ACIA_IKBD].TimeTx)
      agenda_ikbd_process(acia[ACIA_IKBD].tdrs); // to IKBD
    // MIDI
    else if(acia[ACIA_MIDI].LineRxBusy && time_of_event_acia==acia[ACIA_MIDI].TimeRx)
      agenda_midi_replace(0); // from MIDI
    else if(acia[ACIA_MIDI].LineTxBusy && time_of_event_acia==acia[ACIA_MIDI].TimeTx)
    { // to MIDI, do the job here
      acia[ACIA_MIDI].LineTxBusy=0; 
      MIDIPort.OutputByte(acia[ACIA_MIDI].tdrs);
      // send next MIDI note if any
      if(!(acia[ACIA_MIDI].sr&BIT_1))
        acia[ACIA_MIDI].TransmitTDR();
    }
    time_of_event_acia=time_of_next_event+nSysCyclesPerSecond; // put into future
    // schedule next ACIA event if any (if not, it's still in the future)
    // it's not very smart but I see no better way for now
    if(acia[ACIA_IKBD].LineRxBusy==1 && acia[ACIA_IKBD].TimeRx-time_of_event_acia<0)
      time_of_event_acia=acia[ACIA_IKBD].TimeRx;
    if(acia[ACIA_IKBD].LineTxBusy && acia[ACIA_IKBD].TimeTx-time_of_event_acia<0)
      time_of_event_acia=acia[ACIA_IKBD].TimeTx;
    if(acia[ACIA_MIDI].LineRxBusy && acia[ACIA_MIDI].TimeRx-time_of_event_acia<0)
      time_of_event_acia=acia[ACIA_MIDI].TimeRx;
    if(acia[ACIA_MIDI].LineTxBusy && acia[ACIA_MIDI].TimeTx-time_of_event_acia<0)
      time_of_event_acia=acia[ACIA_MIDI].TimeTx;
  }
#if !defined(SSE_NO_OLD_IKBD)
  else
    time_of_event_acia=time_of_next_event+nSysCyclesPerSecond; // put into future
#endif
}


void event_dummy() { 
  // used when STVL is active
  // if it's actually reached, it's probably a bug
  TimeOfHSyncOff=time_of_next_event;
}

#undef LOGSECTION

#endif//#if !defined(SSE_LIBRETRO)
