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
FILE: run.h
DESCRIPTION: Declarations for Steem's central run() function.
struct TMicroTime
---------------------------------------------------------------------------*/

#pragma once
#ifndef RUN_DECLA_H
#define RUN_DECLA_H

#include "conditions.h"

#define RUNSTATE_RUNNING  0
#define RUNSTATE_STOPPING 1
#define RUNSTATE_STOPPED  2

// was ABSOLUTE_CPU_TIME, this is expressed in cycles, in 64bit builds 32MHz, in 32bit builds 8MHz
#define ABSOLUTE_SYS_TIME (sys_timer-sys_cycles)
#define A_S_T ABSOLUTE_SYS_TIME // was ACT, just shorthand

#define AUTO_FRAMESKIP 8 // arbitrary

#define CALC_VIDEO_FREQ_IDX           \
            switch(Glue.VideoFreq){        \
              case 50:      VideoFreqIdx=0; break;  \
              case 60:      VideoFreqIdx=1; break;   \
              default:      VideoFreqIdx=2;   \
            }

extern BYTE stem_runmode; // can be STEM_MODE_CPU (0) or STEM_MODE_INSPECT (2)

#ifdef DEBUG_BUILD

void event_debug_stop();
#define CHECK_BREAKPOINT                     \
        if(debug_num_bk){ \
          if(!debug_first_instruction) breakpoint_check();     \
        }   \
        if(LITTLE_PC==trace_over_breakpoint){ \
          if(runstate==RUNSTATE_RUNNING) runstate=RUNSTATE_STOPPING;                 \
        } 

#define SET_WHY_STOP(s) runstate_why_stop=s;

#else

#define CHECK_BREAKPOINT
#define SET_WHY_STOP(s)

#endif

void run();
void AdaptCpuBoost();
void prepare_next_event();

// event functions

// MFP delay timers A-D
void event_timer_a_timeout(),event_timer_b_timeout(),
event_timer_c_timeout(),event_timer_d_timeout();

void CALLBACK event_scanline(); // CALLBACK because it can be used by STVL
void event_timer_b();
void event_start_vbl();
void event_vbl_interrupt();
#if USE_PASTI
void event_pasti_update();
#endif

// event functions added in SSE builds

void CALLBACK event_trigger_vbi(); // CALLBACK because it can be used by STVL
void event_wd1772(); //1 event for FDC: various parts of its program
void event_driveA_ip(); // 1 event for each drive: index pulse
void event_driveB_ip();
void event_acia();
void event_dummy();

extern int runstate; // can be RUNSTATE_RUNNING (0), RUNSTATE_STOPPING (1) or RUNSTATE_STOPPED (2)


#ifdef WIN32

// structure used to manage more precise timing than milliseconds
// using performance counters
struct TMicroTime {
  LARGE_INTEGER Frequency,StartBlit;
  LONGLONG BfiStartBlit;
  int Ms(LONGLONG ticks) { // convert ticks to milliseconds
    ticks*=1000;
    int ms=(int)(ticks/Frequency.QuadPart);
    return ms;
  }
  LONGLONG Us(LONGLONG ticks) { // convert ticks to microseconds
    ticks*=1000000;
    LONGLONG us=ticks/Frequency.QuadPart;
    return us;
  }
  LONGLONG Ticks(int ms) { // convert milliseconds to ticks
    LONGLONG ticks=ms*Frequency.QuadPart;
    ticks/=1000;
    return ticks;
  }
};

extern TMicroTime MicroTime;

#endif//WIN32


// fast_forward_max_speed=(1000 / (max %/100)); 0 for unlimited
extern int fast_forward,fast_forward_max_speed;
extern bool fast_forward_stuck_down;
extern int slow_motion,slow_motion_speed;
extern int run_speed_ticks_per_second;
extern bool disable_speed_limiting;
extern int run_start_time;
extern DWORD avg_frame_time,avg_frame_time_timer,frame_delay_timeout,timer;
extern int avg_frame_time_counter;
extern DWORD auto_frameskip_target_time;
extern int frameskip, frameskip_count;
extern bool flashlight_flag;
extern COUNTER_VAR time_of_event_acia;
extern COUNTER_VAR time_of_next_event;
extern COUNTER_VAR time_of_next_timer_b; // timer B tick
extern COUNTER_VAR time_of_last_hbl_interrupt,time_of_last_vbl_interrupt;
extern COUNTER_VAR sys_timer_at_res_change;
extern EVENTPROC event_vector;
extern EVENTPROC const event_mfp_timer_timeout[4];
extern int CyclesPerScanlineAtStartOfVbl;

#ifdef UNIX
extern bool RunWhenStop;
#endif

#endif//RUN_DECLA_H
