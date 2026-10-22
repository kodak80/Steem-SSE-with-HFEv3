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
FILE: mfp.cpp
DESCRIPTION: The core of Steem's high-level emulation of the infamous
Motorola MC68901 MFP (Multi Function Processor), including the interrupt.
As it is, it is quite compatible (runs many cases) but MFP emulation
in Steem is unsatisfactory, some hacks, no pulse mode.
A big problem is that the timer crystal frequency and the CPU clock have
no common denominator. In Steem, this is handled by using floating point
computing.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <computer.h>
#include <osd.h>
#if defined(SSE_DEBUGGER_FRAME_REPORT)
#include <debug_framereport.h>
#endif
#include <iolist.h>

//#define STOP_INTS_BECAUSE_INTERCEPT_OS ((ioaccess&IOACCESS_INTERCEPT_OS)!=0)

EVENTPROC const event_mfp_timer_timeout[N_MFP_TIMERS]={event_timer_a_timeout,
  event_timer_b_timeout,event_timer_c_timeout,event_timer_d_timeout};
COUNTER_VAR mfp_timer_timeout[N_MFP_TIMERS];
COUNTER_VAR sys_time_of_first_mfp_tick;
const int mfp_timer_prescale[N_MFP_IRQS]={USHRT_MAX,4,10,16,50,64,100,200,
                                          USHRT_MAX,4,10,16,50,64,100,200};
int mfp_timer_counter[N_MFP_TIMERS];
int mfp_timer_period[N_MFP_TIMERS]={10000,10000,10000,10000};
int mfp_timer_period_fraction[N_MFP_TIMERS];
int mfp_timer_period_current_fraction[N_MFP_TIMERS];
double CpuMfpRatio=(double)CpuNormalHz/(double)MFP_XTAL1;
const BYTE mfp_timer_irq[N_MFP_TIMERS]={13,8,5,4}; // A, B, C, D
const BYTE mfp_gpip_irq[N_MFP_GPIP_BITS]={0,1,2,3,6,7,14,15};
const BYTE interrupt_i_bit[N_MFP_IRQS+1]={1,2,4,8,16,32,64,128,1,2,4,8,16,32,64,128,0};
const BYTE interrupt_i_ab[N_MFP_IRQS+1]={1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0}; // A or B reg
bool mfp_timer_enabled[N_MFP_TIMERS]={false,false,false,false};
bool mfp_timer_period_change[N_MFP_TIMERS]={false,false,false,false};
bool mfp_timer_check[N_MFP_TIMERS];
bool mfp_interrupt_enabled[N_MFP_IRQS];
//BYTE mfp_gpip_no_interrupt=0xf7;
BYTE mfp_gpip_no_interrupt=0xFF; // default monochrome

#if defined(SSE_ENABLE_TRACE_LOG)
char* mfp_reg_name[N_MFP_REGS]={"GPIP","AER","DDR","IERA","IERB","IPRA","IPRB","ISRA",
  "ISRB","IMRA","IMRB","VR","TACR","TBCR","TCDCR","TADR","TBDR","TCDR","TDDR",
  "SCR","UCR","RSR","TSR","UDR"};
#endif

#define LOGSECTION LOGSECTION_MFP

void mfp_gpip_set_bit(int const bit,bool const set) {
  BYTE mask=1<<bit;
  BYTE set_mask=(set) ? mask : 0;
  BYTE cur_val=Mfp.reg[MFPR_GPIP] & mask;
  if(cur_val==set_mask)
    return; //no change
  bool old_1_to_0_detector_input=((cur_val^(Mfp.reg[MFPR_AER]&mask))==mask);
  Mfp.reg[MFPR_GPIP]&=~mask;
  Mfp.reg[MFPR_GPIP]|=set_mask;
  // If the DDR bit is low then the bit from the io line is used,
  // if it is high interrupts then it comes from the input buffer.
  // In that case interrupts are handled in the write to the GPIP.
  if(old_1_to_0_detector_input&&(Mfp.reg[MFPR_DDR]&mask)==0) 
  {
    // Transition the right way! Make the interrupt pend (don't cause an intr
    // straight away in case another more important one has just happened).
    Mfp.GetInterrupt(mfp_gpip_irq[bit],ABSOLUTE_SYS_TIME);
  }
}


#ifdef DEBUG_BUILD

bool TMC68901::SetPending(BYTE irq,COUNTER_VAR when_set) {
  irq&=0x0F;
  bool was_already_pending=((reg[MFPR_IPRA+RegAorB(irq)]&IrqBit(irq))!=0);
  reg[MFPR_IPRA+RegAorB(irq)]|=IrqBit(irq); // Set pending

#if defined(SSE_DEBUGGER_FAKE_IO) //timers only when checked in mask
  if(!IrqInfo[irq].IsTimer || (TRACE_MASK2&(TRACE_CONTROL_IRQ_TA>>IrqInfo[irq].Timer)))
    if(was_already_pending)
      TRACE_LOG2(PRICV " MFP irq %d pending again at %d\n",A_S_T,irq,(int)when_set);
    else
      TRACE_LOG2(PRICV " MFP irq %d pending at %d\n",A_S_T,irq,(int)when_set);
#endif

  if(!was_already_pending)
  {
    //PendingTime[irq]=when_set; // record timing
    UpdateNextIrq(when_set);
  }

  return (reg[MFPR_IPRA+RegAorB(irq)] & IrqBit(irq))!=0;
}

#else

void TMC68901::SetPending(BYTE irq,COUNTER_VAR const when_set) {
  ASSERTB((irq&0x0F)==irq);
#ifndef SSE_LEAN_AND_MEAN
  irq&=0x0F;
#endif
  BOOL was_already_pending=(reg[MFPR_IPRA+RegAorB(irq)]&IrqBit(irq));
  reg[MFPR_IPRA+RegAorB(irq)]|=IrqBit(irq); // Set pending
  if(!was_already_pending)
  {
    //PendingTime[irq]=when_set; // record timing
    UpdateNextIrq(when_set);
  }
}

#endif


void TMC68901::SetTimerReg(BYTE const regnum,BYTE const lobyte) {
  // called when the program changes one of the control or data timer registers (iow.cpp)
  BYTE const old_val=reg[regnum];
  BYTE ti;  
  ASSERTB(a_s_t==A_S_T);
  // control registers
  if(regnum>=MFPR_TACR && regnum<=MFPR_TCDCR)
  {
    ti=(regnum-MFPR_TACR)+MFP_TIMER_A;
    BYTE new_control= (ti==MFP_TIMER_C)?((lobyte>>4)&MFP_TIMER_DELAY_MASK):(lobyte&0x0F);
    do { // this do to do D
      if(GetTimerControlRegister(ti)!=new_control)
      {
        new_control&=MFP_TIMER_DELAY_MASK;
        // This ensures that mfp_timer_counter is set to the correct value just
        // in case the data register is read while timer is stopped or the timer
        // is restarted before a write to the data register.
        // prescale_count is the number of MFP_CLKs there has been since the
        // counter last decreased.
        int prescale_count=CalcTimerCounter(ti,a_s_t);
        if(new_control)
        { // Timer running in delay mode (or pulse, but it's very unlikely, not emulated)
          mfp_timer_enabled[ti]=mfp_interrupt_enabled[mfp_timer_irq[ti]];
          mfp_timer_check[ti]=mfp_timer_enabled[ti]||mfp_timer_period_change[ti];
          COUNTER_VAR StartTime=a_s_t;
          int adjust=0;
          if(SSEOptions.MfpStartSync||SSEOptions.MfpStartTclk)
            adjust=SyncXtal(StartTime,SSEOptions.MfpStartTclk);
          if(!SSEOptions.MfpStartTclk)
            adjust=-adjust; //TODO
          adjust+=SSEOptions.MfpStartCpu;
          StartTime+=adjust;
          // compute next timeout, which depends on current counter + prescale
          double precise_cycles0=mfp_timer_prescale[new_control]*mfp_timer_counter[ti]/64;
          precise_cycles0-=prescale_count; // we count it here now
          if(precise_cycles0<0) 
            precise_cycles0=0;
          precise_cycles0*=CpuMfpRatio;
          precise_cycles0*=cpu_cycles_multiplier;
          mfp_timer_timeout[ti]=StartTime+(int)precise_cycles0;
          // compute precise period between two timeouts
          timer_period_tclk[ti]=mfp_timer_prescale[new_control]
            *(int)(BYTE_00_TO_256(reg[MFPR_TADR+ti])); // record
          double precise_cycles=timer_period_tclk[ti]*cpu_cycles_multiplier*CpuMfpRatio;
          mfp_timer_period[ti]=(int)precise_cycles;
          mfp_timer_period_fraction[ti]
            =(int)( (precise_cycles-mfp_timer_period[ti])*MFP_TIMER_PRECISION );
          //TRACE_LOG2("fraction %f %d\n",precise_cycles-mfp_timer_period[ti],mfp_timer_period_fraction[ti]);
          mfp_timer_period_current_fraction[ti]=0;
#if defined(SSE_ENABLE_TRACE_LOG)
          nTimeouts[ti]=0;
#if defined(SSE_DEBUGGER_FAKE_IO) //timers only when checked in mask
          if(TRACE_MASK2&(TRACE_CONTROL_IRQ_TA>>ti))
#endif
          {
            MEM_ADDRESS ad=((reg[MFPR_VR]&0xf0)+(mfp_timer_irq[ti]))<<2;
            TRACE_LOG(PRICV "(%d) %d %d %d PC %06X set timer %c handler %X control\
 %02X data %02X counter %02X prescale %d\n",
              a_s_t,adjust,TIMING_INFO,old_pc,'A'+ti,SafeLPeek(ad),new_control,
              reg[MFPR_TADR+ti], mfp_timer_counter[ti]/64,prescale_count);
            if(lobyte==MFP_TIMER_EVENT_COUNT)
              TRACE_LOG(" (%d)\n",reg[MFPR_TADR+ti]);
            else
            {
              int freq=xtal/(mfp_timer_prescale[new_control]*BYTE_00_TO_256(reg[MFPR_TADR+ti]));
              double cpu_ticks=mfp_timer_prescale[new_control]*CpuMfpRatio;
              TRACE_LOG(" MFP cycles pulse %d %dHz total %d CPU cycles pulse %f\
 total %f next timeout " PRICV "\n",
              mfp_timer_prescale[new_control],freq,
              mfp_timer_prescale[new_control]*(int)(BYTE_00_TO_256(reg[MFPR_TADR+ti])),cpu_ticks,
//                mfp_timer_period[ti],mfp_timer_period_fraction[ti],
              precise_cycles,
              mfp_timer_timeout[ti]);
            }
          }
#endif//#if defined(SSE_ENABLE_TRACE_LOG)
        }
        else //if(new_control)
        {  //timer stopped, or in event count mode
          if(lobyte==0 && old_val!=MFP_TIMER_EVENT_COUNT)
          {
            if(mfp_timer_enabled[ti] && a_s_t-mfp_timer_timeout[ti]>=0) //TODO...
            {
              BYTE i_ab=RegAorB(mfp_timer_irq[ti]);
              BYTE i_bit=IrqBit(mfp_timer_irq[ti]);
              if(!(reg[MFPR_ISRA+i_ab]&i_bit) && (reg[MFPR_IERA+i_ab]&reg[MFPR_IMRA+i_ab]&i_bit))
              {
#if defined(SSE_DEBUGGER) || defined(SSE_LOGSECTIONS_FOR_ALL)
                if(TRACE_LOG_ENABLED(LOGSECTION_IOW))
                {
                  TRACE_LOG(PRICV " timer %c pending on stop\n",a_s_t,'A'+ti);
                }
#endif
                GetInterrupt(mfp_timer_irq[ti],mfp_timer_timeout[ti]);
              }
            }
            // latency when stopping timer - new theory v412 for Froggies...
            int adjust=0;
            if(SSEOptions.MfpStopSync||SSEOptions.MfpStopTclk)
              adjust=SyncXtal(a_s_t,SSEOptions.MfpStopTclk);
            if(!SSEOptions.MfpStartTclk)
              adjust=-adjust; //TODO
            adjust+=SSEOptions.MfpStopCpu;
            CalcTimerCounter(ti,a_s_t+adjust);
          }
#if defined(SSE_ENABLE_TRACE_LOG)
#if defined(SSE_DEBUGGER_FAKE_IO) //only for timers checked in mask
          if(TRACE_MASK2&(TRACE_CONTROL_IRQ_TA>>ti))
#endif
          if(ti<=MFP_TIMER_B &&(lobyte&BIT_3)) // start timer A or B
            TRACE_LOG(PRICV " %d %d %d PC %06X set Timer %c data %d\n",
              A_S_T,TIMING_INFO,old_pc,'A'+ti,reg[MFPR_TADR+ti]);
          else // stop timer
            TRACE_LOG(PRICV " %d %d %d PC %06X stop Timer %c counter %02X (%02X) prescale %d\n",
              A_S_T,TIMING_INFO,old_pc,'A'+ti,Counter[ti],mfp_timer_counter[ti]/64,Prescale[ti]);
#endif
          Prescale[ti]=0; // Lorna, MFPFROG.S
          mfp_timer_enabled[ti]=mfp_timer_period_change[ti]=mfp_timer_check[ti]=false;
        }//if(new_control)
        if(ti==MFP_TIMER_D)
          RS232_CalculateBaud(new_control,false);
#if defined(SSE_DEBUGGER_FRAME_REPORT)
        if(regnum==MFPR_TBCR && (FRAME_REPORT_MASK2 & FRAME_REPORT_MASK_TIMER_B))
          FrameEvents.Add(scan_y,(short)(LINECYCLES/TICKS8),"CB",lobyte);
#endif
      }
      ti++;
      new_control=(lobyte&MFP_TIMER_DELAY_MASK); // Timer D control (if ti is now D)
    } while(ti==MFP_TIMER_D); 
    if(regnum==MFPR_TBCR && lobyte==MFP_TIMER_EVENT_COUNT)
    {
#if defined(SSE_ENABLE_TRACE_LOG)
      nTimeouts[MFP_TIMER_B]=0;
#endif
      if(!OPTION_C3)
        ComputeNextTimerB();
    }
#if defined(SSE_ENABLE_TRACE_LOG)
    if(regnum<=MFPR_TBCR && lobyte>MFP_TIMER_EVENT_COUNT)
      TRACE_LOG("MFP: --------------- PULSE EXTENSION MODE!! -----------------\n");
#endif
#if defined(SSE_INT_MFP_PULSE)
/*  and what should we do?  
    2 timer B events/line is easy
    actual counting is annoying
    since no program uses this feature, we may save the trouble as long as we
    don't have a low-level MFP emu
*/
    if(reg<=MFPR_TBCR && new_val>MFP_TIMER_EVENT_COUNT)
    {
    }
#endif
    prepare_next_event();
  }
  // data registers
  else if(regnum>=MFPR_TADR && regnum <=MFPR_TDDR)
  {
    ti=regnum-MFPR_TADR;
    TRACE_LOG(PRICV " %d %d %d PC %06X Write T%cDR=%02X\n",A_S_T,TIMING_INFO,old_pc,'A'+ti,lobyte);
    BYTE control=GetTimerControlRegister(ti);
/* If the Timer Data Register is written while the timer is running, the new word
is not loaded into the timer until it counts through H01.      */      
    if(control==0) 
    {  // timer stopped: counter updated at once
      mfp_timer_counter[ti]=((int)BYTE_00_TO_256(lobyte))*64;
      Counter[ti]=lobyte;
      Prescale[ti]=0;
      mfp_timer_period[ti]=(int)((double)(mfp_timer_prescale[control]
        *(int)(BYTE_00_TO_256(lobyte)))*CpuMfpRatio);
    }
    else if(control&MFP_TIMER_DELAY_MASK)
    {
      // Need to calculate the period next time the timer times out
      mfp_timer_check[ti]=mfp_timer_period_change[ti]=true;
      if(!mfp_timer_enabled[ti])
      {
        // If it is disabled it could be in the past, causing instant
        // event_timer_?_timeout, so realign it
        //TODO more precise?
        COUNTER_VAR stage=mfp_timer_timeout[ti]-ABSOLUTE_SYS_TIME;
        if(stage<0) 
          stage+=((-stage/mfp_timer_period[ti])+1)*mfp_timer_period[ti];
        stage%=mfp_timer_period[ti];
        mfp_timer_timeout[ti]=ABSOLUTE_SYS_TIME+stage; //realign
      }
    }
    if(regnum==MFPR_TDDR && lobyte!=old_val)
      RS232_CalculateBaud(control,false);
#if defined(SSE_DEBUGGER_FRAME_REPORT)
    if(regnum==MFPR_TBDR && (FRAME_REPORT_MASK2 & FRAME_REPORT_MASK_TIMER_B))
      FrameEvents.Add(scan_y,(short)(LINECYCLES/TICKS8),"DB",lobyte);
#endif

  }
  reg[regnum]=lobyte;
}


void TMC68901::InitTimers() {// For load state and CPU speed change
  UpdateTimerControls();
  CalcInterruptsEnabled();
  CalcTimersEnabled();
  for(int ti=0;ti<N_MFP_TIMERS;ti++)
  {
    BYTE cr=GetTimerControlRegister(ti);
    if(cr&MFP_TIMER_DELAY_MASK)
    { // Not stopped or in event count mode
      // This must allow for counter not being a multiple of 64
      mfp_timer_check[ti]=true;
    }
  }
  RS232_CalculateBaud(GetTimerControlRegister(MFP_TIMER_D),true);
}


BYTE TMC68901::CalcTimerCounter(BYTE const ti,COUNTER_VAR const t) {
  // compute counter and prescale (which it returns) of timer ti at t
  // if the timer isn't delay-running, t doesn't matter
  BYTE cr=GetTimerControlRegister(ti);
  if(cr&MFP_TIMER_DELAY_MASK) 
  { // not stopped, not event count mode
    int fraction=mfp_timer_period_current_fraction[ti];
    COUNTER_VAR stage=mfp_timer_timeout[ti]-t;
    if(stage<0) // has timed out?
    {
      // the while loop can be very slow with 64bit counters, this should accelerate it
      // fixes slow reset, slow save, slow quit on Steem64
      COUNTER_VAR a=-stage/mfp_timer_period[ti];
      if(a>=200) // fraction less important...
        stage+=a*mfp_timer_period[ti];

      while(stage<0 && mfp_timer_period[ti]>0)
      {
        stage+=mfp_timer_period[ti];
        fraction+=mfp_timer_period_fraction[ti]; 
        if(fraction>=MFP_TIMER_PRECISION)
        {
          fraction-=MFP_TIMER_PRECISION;
          stage++;
        }
      }
    }
    //stage%=mfp_timer_period[ti];
    while(stage>/*=*/mfp_timer_period[ti]) // modulo with the precision
    {
      stage-=mfp_timer_period[ti];
      fraction-=mfp_timer_period_fraction[ti];
      if(fraction<0 && stage>0) // temp avoid <0
      {
        fraction+=MFP_TIMER_PRECISION;
        stage--;
      }
    }
    //ASSERT(stage>=0 && stage<mfp_timer_period[ti]);
    int ticks_per_count=mfp_timer_prescale[cr&MFP_TIMER_DELAY_MASK];
    // Convert to number of MFP cycles until timeout
    double dticks=stage/CpuMfpRatio;
    int mfp_ticks=(int)dticks; // truncating is right
    int mfp_count=mfp_ticks/ticks_per_count; // truncating is right
    mfp_timer_counter[ti]=mfp_count*64+64;
    Counter[ti]=(BYTE)(mfp_timer_counter[ti]/64);
    //Prescale[ti]=(BYTE)(ticks_per_count-((mfp_ticks % ticks_per_count)+1));
    Prescale[ti]=(BYTE)((mfp_ticks%ticks_per_count)+1);
  }
  else
    Prescale[ti]=0;
  //TRACE_LOG2("calc %c Counter %X Prescale %X\n",'A'+ti,Counter[ti],Prescale[ti]);
  return Prescale[ti];
}


// only called by m68kProcess() at the end of an instruction
// irq is normally the interrupt 0-15, but it can be 16 too (internal code for spurious)
// this function emulates the IACK process between the CPU and the MFP, taking control
// of both chips at our low level (CPU timings...)
void TMC68901::Iack(char irq) {
  // CPU Timings: n nn ns ni n-  n nS ns nV nv np n np 
  char irq0=irq;

  //if(irq==MFP_INT_SPURIOUS && (!OPTION_SPURIOUS||STOP_INTS_BECAUSE_INTERCEPT_OS))
  if((ioaccess&IOACCESS_INTERCEPT_OS)!=0 && SSEOptions.BlockInterrrupts
    || irq==MFP_INT_SPURIOUS && !OPTION_SPURIOUS)
    return;
  
  M68K_UNSTOP;
  Cpu.ProcessingState=TMC68000::EXCEPTION;
  ASSERTB (!(irq>=0&&irq<N_MFP_IRQS)
      || !(
      (!(mfp_interrupt_enabled[irq]) || PSWI>=6
      || ((reg[MFPR_IMRA+RegAorB(irq)]&IrqBit(irq))==0)
      || ((reg[MFPR_ISRA+RegAorB(irq)]&(-IrqBit(irq)))
      || (RegAorB(irq)&&reg[MFPR_ISRA])))
        ));
#ifndef SSE_LEAN_AND_MEAN
  // we redo some tests, normally this shouldn't happen
  if(irq>=0&&irq<N_MFP_IRQS)
  {
    if(!(mfp_interrupt_enabled[irq]) || PSWI>=6
      || ((reg[MFPR_IMRA+RegAorB(irq)]&IrqBit(irq))==0)
      || ((reg[MFPR_ISRA+RegAorB(irq)]&(-IrqBit(irq)))
      || (RegAorB(irq)&&reg[MFPR_ISRA])))
    {
      TRACE_OSD_DBG("%s %s denied","MFP","irq"); // funny message
      TRACE_LOG("%s %s denied\n","MFP","irq"); // funny message
      return;
    }
  }
#endif
  CPU_BUS_IDLE(6); //n nn
  UPDATE_SR;
  WORD saved_sr=SR; //copy sr
  change_to_supervisor_mode();
  CLEAR_T;
  PSWI=6; // update ipl mask
  iabus=SP-2;
  dbus=pcl; // stack PC low word;
  CPU_BUS_ACCESS_WRITE; // ns 12
  iabus-=4;
  SP=iabus;
  // start IACK bus cycle - the MFP is slow
  for(int i=0;i<12/2;i++) // ni 24
  {
    BUS_WAIT_STATES(2); // just in case, avoid too long CPU timings
  }
  // If no irq is pending at the start of IACK, the MFP won't respond (spurious).
  // Otherwise, another higher irq could trigger during IACK (Anomaly Menu)
  // or the same irq could trigger again (Final Conflict)
  if(irq<MFP_INT_SPURIOUS)
  {
    while(sys_cycles<=0)
    {
#if defined(SSE_DEBUGGER_FAKE_IO)
      if(TRACE_MASK2&TRACE_CONTROL_EVENT)
        TRACE_EVENT("IACK",event_vector);
#endif
      event_vector(); // event_scanline etc. also taken! TODO
      prepare_next_event();
    }
    irq=UpdateNextIrq(A_S_T);
  }
  MEM_ADDRESS vector;
  // The dangerous spurious interrupt test. SPURIOUS.TOS, and triggers in
  // some demos (have a convenient RTE).
  //if(irq==MFP_INT_SPURIOUS && (!OPTION_SPURIOUS||STOP_INTS_BECAUSE_INTERCEPT_OS))
  if(irq==MFP_INT_SPURIOUS && !OPTION_SPURIOUS)
    irq=irq0; // no spurious allowed
  if(OPTION_SPURIOUS&&irq==MFP_INT_SPURIOUS)
  {
#if defined(SSE_STATS)
    Stats.nSpurious++;
#endif
#if defined(SSE_OSD_DEBUGINFO)
    OsdControl.Trace(TOsdControl::OUTPUT_SB,"%s %d","EXC",EXCEPTION_SPURIOUS_EXCEPTION);
#endif
    //TRACE("Spurious\n");
#if defined(SSE_ENABLE_TRACE_LOG)
    TRACE_LOG(PRICV " PC %X Spurious! %X\n",A_S_T,old_pc);
    TRACE_LOG("IRQ %d (%d->%d) IERA %X IPRA %X IMRA %X ISRA %X IERB %X IPRB %X IMRB %X ISRB %X\n",
      Irq,irq0,NextIrq,reg[MFPR_IERA],reg[MFPR_IPRA],reg[MFPR_IMRA],reg[MFPR_ISRA],reg[MFPR_IERB],reg[MFPR_IPRB],reg[MFPR_IMRB],reg[MFPR_ISRB]);
#endif
    int ncycles=(64+4+2-12); //-ni? This hasn't been tested
    for(int i=0;i<ncycles;i+=2)
    {
      BUS_WAIT_STATES(2); // just in case, avoid too long CPU timings
    }
    BUS_WAIT_STATES(4);
    vector=EXCEPTION_SPURIOUS_EXCEPTION;
  }
  else
  {
    vector=(reg[MFPR_VR]&0xf0)+(irq);
    TRACE_LOG3(PRICV " MFP clear IRQ %d\n",A_S_T,irq);
    reg[MFPR_IPRA+RegAorB(irq)]&=~IrqBit(irq);
    if(reg[MFPR_VR] & BIT_3) // software mode: set when vector is passed, cleared by program
      reg[MFPR_ISRA+RegAorB(irq)]|=IrqBit(irq);
    else //automatic
      reg[MFPR_ISRA+RegAorB(irq)]&=~IrqBit(irq);
#if defined(SSE_STATS)
    Stats.nMfpIrq[irq&15]++;
#endif
  }
  vector<<=2;

#if defined(SSE_DEBUGGER)
#if defined(SSE_DEBUGGER_FRAME_REPORT)
  if(FRAME_REPORT_MASK2 & FRAME_REPORT_MASK_INT)
    FrameEvents.Add(scan_y,(short)(LINECYCLES-24)/TICKS8,'I',0x60+irq);
#endif
#if defined(SSE_DEBUGGER_FAKE_IO)
  if( (TRACE_LOG_ENABLED(LOGSECTION_MFP)&&TRACE_LOG2_ENABLED
    || TRACE_LOG_ENABLED(LOGSECTION_INTERRUPTS)) &&
     (!IrqInfo[irq].IsTimer || (TRACE_MASK2&(TRACE_CONTROL_IRQ_TA>>IrqInfo[irq].Timer))))
#endif
  {
    TRACE(PRICV " %d %d %d PC %X ird %X IRQ %d VEC %X ",A_S_T-24,FRAME,
      scan_y,LINECYCLES-24,old_pc,IRD,irq,SafeLPeek(vector));
    switch (irq) {
    case 0:TRACE("Centronics busy\n");          break;
    case 1:TRACE("RS-232 DCD\n");               break;
    case 2:TRACE("RS-232 CTS\n");               break;
    case 3:TRACE("Blitter done\n");             break;
    case 4:TRACE("Timer D #%d\n", ++nTimeouts[3]); break;
    case 5:TRACE("Timer C #%d\n", ++nTimeouts[2]); break;
    case 6:TRACE("ACIA\n");                     break;
    case 7:TRACE("FDC/HDC\n");                  break;
    case 8:TRACE("Timer B #%d\n", ++nTimeouts[1]); break;
    case 9:TRACE("Send Error\n");               break;
    case 10:TRACE("Send buffer empty\n");       break;
    case 11:TRACE("Receive error\n");           break;
    case 12:TRACE("Receive buffer full\n");     break;
    case 13:TRACE("Timer A #%d\n", ++nTimeouts[0]); break;
    case 14:TRACE("RS-232 Ring detect\n");      break;
    case 15:TRACE("Monochrome Detect\n");       break;
    case 16:TRACE("Spurious interrupt\n");       break;
    }//sw
  }
#if defined(SSE_DEBUGGER)
  Debug.FrameInterrupts|=4;
  Debug.FrameMfpIrqs|= 1<<irq;
#endif
#if defined(SSE_DEBUGGER_IRQ) 
  pc_history_y[pc_history_idx]=scan_y;
  pc_history_c[pc_history_idx]=LINECYCLES;
  pc_history[pc_history_idx++]=0x99000001+(6<<16)+(WORD)(irq<<8); 
  if(pc_history_idx>=HISTORY_SIZE) 
    pc_history_idx=0;
#endif
#endif//#if defined(SSE_DEBUGGER)

  CPU_BUS_IDLE(4); //nn
#ifndef SSE_LEAN_AND_MEAN
  LastIrq=irq; //unused
#endif
  UpdateNextIrq(A_S_T); // necessary
  dbus=saved_sr; // SR written between two parts of PC
  CPU_BUS_ACCESS_WRITE; // ns
  iabus+=2;
  dbus=pch; // PC high word 
  CPU_BUS_ACCESS_WRITE; // nS
  iabus=vector;
  CPU_BUS_ACCESS_READ; // nV
  effective_address_h=dbus;
  iabus+=2;
  CPU_BUS_ACCESS_READ; // nv
  effective_address_l=dbus;
  iabus=effective_address;
  Cpu.ProcessingState=TMC68000::NORMAL;
  m68kSetPC(iabus,2);
  interrupt_depth++;
#ifdef DEBUG_BUILD
  debug_check_break_on_irq(irq);
#endif
}


void TMC68901::TimerATick() { // STF: Centronics busy STE: Digital sound
  //ASSERT(GetTimerControlRegister(MFP_TIMER_A)==MFP_TIMER_EVENT_COUNT);
  mfp_timer_counter[MFP_TIMER_A]-=64;
  Counter[MFP_TIMER_A]--;
  if(mfp_timer_counter[MFP_TIMER_A]<64)
  {
    mfp_timer_counter[MFP_TIMER_A]=BYTE_00_TO_256(reg[MFPR_TADR])*64;
    Counter[MFP_TIMER_A]=reg[MFPR_TADR];
    GetInterrupt(MFP_INT_TIMER_A,ABSOLUTE_SYS_TIME);
  }
/*  Besides generating a count pulse, the active transition of the auxiliary
    input signal will also produce an interrupt on the I3 or I4 interrupt
    channel, if the interrupt channel is enabled.
*/
  GetInterrupt(MFP_INT_ACIA,ABSOLUTE_SYS_TIME); // I4 interrupt channel = ACIA
}


TMC68901::TMC68901() {
  ZeroMemory(this,sizeof(TMC68901));
  Restore();
}


void TMC68901::CalcInterruptsEnabled() {
#if 1 // optimised!
  for(int i=0,mask=1;i<N_MFP_GPIP_BITS;i++,mask<<=1)
  {
    mfp_interrupt_enabled[i]=((reg[MFPR_IERB]&mask)!=0);
    mfp_interrupt_enabled[i+8]=((reg[MFPR_IERA]&mask)!=0);
  }
#else
  int mask=1;	
  for(int i=0;i<N_MFP_GPIP_BITS;i++,mask<<=1)
    mfp_interrupt_enabled[i]=((reg[MFPR_IERB]&mask)!=0);
  mask=1;	
  for(int i=0;i<N_MFP_GPIP_BITS;i++,mask<<=1)
    mfp_interrupt_enabled[i+8]=((reg[MFPR_IERA]&mask)!=0);
#endif
}


void TMC68901::CalcTimersEnabled() {
  for(int ti=0;ti<N_MFP_TIMERS;ti++)
  {
    mfp_timer_enabled[ti]=(mfp_interrupt_enabled[mfp_timer_irq[ti]]
      && (GetTimerControlRegister(ti)&MFP_TIMER_DELAY_MASK));
    mfp_timer_check[ti]=(mfp_timer_enabled[ti]||mfp_timer_period_change[ti]);
  }
}


void TMC68901::CalcTimerPeriod(int const ti) {
  BYTE cr=GetTimerControlRegister(ti) & MFP_TIMER_DELAY_MASK;
  int dr=BYTE_00_TO_256(reg[MFPR_TADR+ti]);
  double precise_cycles=((double)mfp_timer_prescale[cr]*dr)*CpuMfpRatio;
  precise_cycles*=cpu_cycles_multiplier; //cpu_cycles_multiplier is a double
  mfp_timer_period[ti]=(int)precise_cycles;
  mfp_timer_period_fraction[ti]=(int)(MFP_TIMER_PRECISION*precise_cycles-(double)mfp_timer_period[ti]);
}


void TMC68901::UpdateTimerControls() { // so it's faster to read them
  timer_control[MFP_TIMER_A]=reg[MFPR_TACR];
  timer_control[MFP_TIMER_B]=reg[MFPR_TBCR];
  timer_control[MFP_TIMER_C]=(reg[MFPR_TCDCR]>>4) & MFP_TIMER_DELAY_MASK;
  timer_control[MFP_TIMER_D]=reg[MFPR_TCDCR] & MFP_TIMER_DELAY_MASK;
}


void TMC68901::Restore() {
  ZeroMemory(IrqInfo,sizeof(IrqInfo));
  // init IrqInfo structure
  IrqInfo[MFP_INT_CENTRONICS_BUSY].IsGpip=IrqInfo[MFP_INT_RS232_DCD].IsGpip
    =IrqInfo[MFP_INT_RS232_CTS].IsGpip=IrqInfo[MFP_INT_BLITTER].IsGpip
    =IrqInfo[MFP_INT_ACIA].IsGpip=IrqInfo[MFP_INT_FDC_AND_DMA].IsGpip
    =IrqInfo[MFP_INT_RS232_RING_INDICATOR].IsGpip
    =IrqInfo[MFP_INT_MONOCHROME_MONITOR_DETECT].IsGpip=true;
  IrqInfo[MFP_INT_TIMER_A].IsTimer=IrqInfo[MFP_INT_TIMER_B].IsTimer
    =IrqInfo[MFP_INT_TIMER_C].IsTimer=IrqInfo[MFP_INT_TIMER_D].IsTimer=true;
  IrqInfo[MFP_INT_TIMER_D].Timer=MFP_TIMER_D;
  IrqInfo[MFP_INT_TIMER_C].Timer=MFP_TIMER_C;
  IrqInfo[MFP_INT_TIMER_B].Timer=MFP_TIMER_B;
  IrqInfo[MFP_INT_TIMER_A].Timer=MFP_TIMER_A;
  ier=(WORD*)&reg[MFPR_IERA];
  ipr=(WORD*)&reg[MFPR_IPRA];
  isr=(WORD*)&reg[MFPR_ISRA];
  imr=(WORD*)&reg[MFPR_IMRA];
  UpdateTimerControls();
  for(int ti=MFP_TIMER_A;ti<=MFP_TIMER_D;ti++)
    timer_period_tclk[ti]=mfp_timer_prescale[timer_control[ti]]*(int)(BYTE_00_TO_256(reg[MFPR_TADR+ti]));
  if(!xtal)
    UpdateXtal();
}


char TMC68901::UpdateNextIrq(COUNTER_VAR const at_time) {
  if(MFP_IRQ) // global test before we check each irq
  {
    for(char irq=N_MFP_IRQS-1;irq>=0;irq--) 
    {
      BYTE i_ab=RegAorB((BYTE)irq);
      BYTE i_bit=IrqBit((BYTE)irq);
      if((i_bit&reg[MFPR_IERA+i_ab]&reg[MFPR_IPRA+i_ab]&reg[MFPR_IMRA+i_ab])
        && !(i_bit&reg[MFPR_ISRA+i_ab]))
      {
        Irq=true; // line is asserted by the MFP
        NextIrq=irq;
#if defined(SSE_DEBUGGER_FAKE_IO) //timers only when checked in mask
        if(!IrqInfo[irq].IsTimer || (TRACE_MASK2&(TRACE_CONTROL_IRQ_TA>>IrqInfo[irq].Timer)))
#endif
        {
          TRACE_LOG3("%d MFP IRQ (%d)\n",at_time,NextIrq);
        }
        break;
      }
      else if((i_bit&reg[MFPR_ISRA+i_ab]))
      {
        Irq=false;
        break; // no IRQ possible then
      }
    }//nxt irq
  }
  else
    Irq=false;
  if(!Irq)
    NextIrq=MFP_INT_SPURIOUS; // pseudo irq 16 means no IRQ, internally in Steem SSE
  update_ipl(at_time);
  return NextIrq;
}


/*  Unique function that sets up the timing of next "timer B event",
    which is when the B tick through TBI is recorded by the MFP (counter increases).
    The GLUE's DE signal is the input.
    It should be correct also when timing of DE is changed by a "Shifter trick".
*/
void TMC68901::ComputeNextTimerB() {
  ASSERTB(!OPTION_C3);
  COUNTER_VAR tontb=0;
#if defined(SSE_HARDWARE_OVERSCAN)
  // mfp receives the normal DE signal, not the tricked overscan DE
  // in this emulation, it's a bit off compared with HW
  if(SSEConfig.OverscanOn && scan_y<0 
    && (scan_y>=VER_PIXELS_HI || COLOUR_MONITOR && scan_y>=VER_PIXELS_LO))
  {
    // skip - notice TOS expects groups of lines with no timer B tick at reset
  }
  else
#endif
  if(Glue.de_v_on && !(Glue.CurrentScanline.Tricks&TRICK_0BYTE_LINE))
  {
    // time of DE transition this scanline
    TimerBLinecycle=(reg[MFPR_AER]&MFP_GPIP_BLITTER_MASK) ?
      Glue.CurrentScanline.StartCycle : Glue.CurrentScanline.EndCycle; // from Hatari
    // absolute
    tontb=TimeOfHSyncOff+TimerBLinecycle;
#if defined(SSE_MFP_421) // better do this before "sync"
    // already ticked?
    if(((reg[MFPR_AER]&MFP_GPIP_BLITTER_MASK) && Glue.m_Status.timerb_start)
       || (!(reg[MFPR_AER]&MFP_GPIP_BLITTER_MASK) && Glue.m_Status.timerb_end))
    {
      tontb+=Glue.CurrentScanline.Cycles; //next line?
    }
#endif
    // add GLUE and MFP processing time
    tontb+=2*TICKS8; // from HDE to DE
    // add wakeup DE shift (Super Neo) (looks suspect)
    tontb+=Mmu.FreqMod[(IS_STE) ? 3 : OPTION_WS]*TICKS8;
    if(SSEOptions.MfpTbSync||SSEOptions.MfpTbTclk)
      tontb+=SyncXtal(tontb,SSEOptions.MfpTbTclk); // sync + count
    tontb+=SSEOptions.MfpTbCpu;
#if !defined(SSE_MFP_421) // PP49: the linecycles test blocks legit ticks
    // already ticked? / no tb?
    if(  ((reg[MFPR_AER]&MFP_GPIP_BLITTER_MASK) && Glue.m_Status.timerb_start)
      || (!(reg[MFPR_AER]&MFP_GPIP_BLITTER_MASK) && Glue.m_Status.timerb_end)
      || (reg[MFPR_TBCR]!=MFP_TIMER_EVENT_COUNT && LINECYCLES>TimerBLinecycle)) // Oh no more froggies
    {
      tontb+=Glue.CurrentScanline.Cycles; //next line?
    }
#endif
  }
  else
    tontb=TimeOfHSyncOff+160000*TICKS8*SSEConfig.CpuBoost;
  time_of_next_timer_b=tontb;
  prepare_next_event();
}


void TMC68901::Reset(bool const Cold) {
  for(BYTE n=0;n<N_MFP_REGS;n++)
  {
    if(n<MFPR_TADR || n>MFPR_TDDR) // Timer data registers not reset
      reg[n]=0;
    else if(Cold) // Timer data registers get random value at power-up
    {
      reg[n]=0xF0|(rand()&0xE);
      Counter[n-MFPR_TADR]=reg[n];
      mfp_timer_counter[n-MFPR_TADR]=reg[n]*64;
    }
  }
  reg[MFPR_GPIP]=mfp_gpip_no_interrupt;
  reg[MFPR_AER]=MFP_GPIP_CTS_MASK;   // CTS goes the other way
  reg[MFPR_TSR]=BIT_7|BIT_4;  //buffer empty | END
  for(int ti=0;ti<N_MFP_TIMERS;ti++)
  {
    Prescale[ti]=timer_control[ti]=0;
    mfp_timer_period_change[ti]=false;
  }
  CalcInterruptsEnabled();
  CalcTimersEnabled();
  Irq=false;
}


// return # system cycles from last timer clock tick (add_tclk_cycles<=0)
// or to next click (add_tclk_cycles>0)
int TMC68901::SyncXtal(COUNTER_VAR const system_time,int const add_tclk_cycles) {
  // so we have a wobble without using rand()
  // cpu ~ CpuNormalHz 8021247
  // timer ~ 2457600
  ASSERTB(nSysCyclesPerSecond && xtal);
  int sys_cycles_in=system_time%nSysCyclesPerSecond;
  if(sys_cycles_in<0)
    sys_cycles_in+=nSysCyclesPerSecond;
  double tclk_cycles_in=((double)sys_cycles_in*(double)xtal)/(double)nSysCyclesPerSecond;
#if 0
  if(add_tclk_cycles<0)
  {
    tclk_cycles_in+=-add_tclk_cycles; // add negative tCLK cycles (not used)
  }
#endif
  int last_tclk_cycle=(int)tclk_cycles_in;
  // if add_tclk_cycles positive, return sys cycles to a future tCLK tick instead
  if(add_tclk_cycles>0)
  {
    int future_tclk_cycle=last_tclk_cycle+add_tclk_cycles;
    double tclk_1000th_to_tick=(future_tclk_cycle-tclk_cycles_in)*1000;
    int sys_1000th_to_tick=(int)((tclk_1000th_to_tick*nSysCyclesPerSecond)/xtal);
    int sys_to_tick=sys_1000th_to_tick/1000;
    return sys_to_tick;
  }
  double tclk_1000th_from_tick=(tclk_cycles_in-last_tclk_cycle)*1000;
  int sys_1000th_from_tick=(int)((tclk_1000th_from_tick*nSysCyclesPerSecond)/xtal);
  int sys_from_tick=sys_1000th_from_tick/1000;
  return sys_from_tick;
}


// the XTAL frequency can be fine-tuned in Steem SSE because we think
// there was some variation
void TMC68901::UpdateXtal() {
 xtal=(IS_STE) ? MFP_XTAL2 : MFP_XTAL1;
}


// MFP is getting an interrupt, internal or external
void TMC68901::GetInterrupt(BYTE const irq,COUNTER_VAR when_set) {
  ASSERTB(irq<MFP_INT_SPURIOUS);
  if(mfp_interrupt_enabled[irq])
  {
    if(IrqInfo[irq].IsTimer)
    {
      if(SSEOptions.MfpIrqTclk||SSEOptions.MfpIrqSync)
        when_set+=SyncXtal(when_set,SSEOptions.MfpIrqTclk);
      when_set+=SSEOptions.MfpIrqCpu;
    }
    SetPending(irq,when_set);
  }
}

#undef LOGSECTION
