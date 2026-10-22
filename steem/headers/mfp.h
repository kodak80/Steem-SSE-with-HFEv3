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
FILE: mfp.h
DESCRIPTION: Declarations for Steem's high-level emulation of the infamous
Motorola MC68901 MFP (Multi Function Processor).
TODO: move some variables to the Mfp object
struct TMC68901IrqInfo, TMC68901
---------------------------------------------------------------------------*/

#pragma once
#ifndef MFP_DECLA_H
#define MFP_DECLA_H


#include "SSE.h"
#include "conditions.h"
//#include "binary.h"
//#include "debug.h"


enum EMfp {
  
  // Registers
  
  N_MFP_REGS=24,
  MFPR_GPIP=0, // ff fa01 MFP General Purpose I/O
  MFPR_AER, // ff fa03 MFP Active Edge
  MFPR_DDR, // ff fa05 MFP Data Direction
  MFPR_IERA, // ff fa07 MFP Interrupt Enable A
  MFPR_IERB, // ff fa09 MFP Interrupt Enable B
  MFPR_IPRA, // ff fa0b MFP Interrupt Pending A
  MFPR_IPRB, // ff fa0d MFP Interrupt Pending B
  MFPR_ISRA, // ff fa0f MFP Interrupt In-Service A
  MFPR_ISRB, // ff fa11 MFP Interrupt In-Service B
  MFPR_IMRA, // ff fa13 MFP Interrupt Mask A
  MFPR_IMRB, // ff fa15 MFP Interrupt Mask B
  MFPR_VR, // ff fa17 MFP Vector
  MFPR_TACR, // ff fa19 MFP Timer A Control
  MFPR_TBCR, // ff fa1b MFP Timer B Control
  MFPR_TCDCR, // ff fa1d MFP Timers C and D Control
  MFPR_TADR, // ff fa1f  MFP Timer A Data
  MFPR_TBDR, // ff fa21  MFP Timer B Data
  MFPR_TCDR, // ff fa23  MFP Timer C Data
  MFPR_TDDR, // ff fa25  MFP Timer D Data

  // RS232
  
  MFPR_SCR, // ff fa27 MFP Sync Character
  MFPR_UCR, // ff fa29 MFP USART Control
  MFPR_RSR, // ff fa2b MFP Receiver Status
  MFPR_TSR, // ff fa2d MFP Transmitter Status
  MFPR_UDR, // ff fa2f MFP USART Data

  // Interrupts
  
  N_MFP_IRQS=16,N_MFP_GPIP_BITS=8,
  MFP_INT_SPURIOUS=16, // pseudo interrupt
  MFP_INT_MONOCHROME_MONITOR_DETECT=15, // One interrupt line for monitor & STE sound
                                        // It is so high because monitors could be damaged
  MFP_INT_RS232_RING_INDICATOR=14,
  MFP_INT_TIMER_A=13, // notice that timer A has higher priority than B, B than C, C than D
  MFP_INT_RS232_RECEIVE_BUFFER_FULL=12,
  MFP_INT_RS232_RECEIVE_ERROR=11,
  MFP_INT_RS232_TRANSMIT_BUFFER_EMPTY=10,
  MFP_INT_RS232_TRANSMIT_ERROR=9,
  MFP_INT_TIMER_B=8, // can be used for raster effects or to open the bottom border
                     // TOS uses it at init as a test
  MFP_INT_FDC_AND_DMA=7, // One interrupt line for floppy and hard disk
  MFP_INT_ACIA=6, // One interrupt line for 2 ACIA chips: IKBD & MIDI
  MFP_INT_TIMER_C=5, // used by TOS
  MFP_INT_TIMER_D=4,
  MFP_INT_BLITTER=3, // Nobody uses that
  MFP_INT_RS232_CTS=2,
  MFP_INT_RS232_DCD=1,
  MFP_INT_CENTRONICS_BUSY=0,

  // GPIP bits

  MFP_GPIP_CENTRONICS_BIT=0,
  MFP_GPIP_DCD_BIT=1,
  MFP_GPIP_CTS_BIT=2,
  MFP_GPIP_BLITTER_BIT=3,
  MFP_GPIP_ACIA_BIT=4,
  MFP_GPIP_FDC_BIT=5,
  MFP_GPIP_RING_BIT=6,
  MFP_GPIP_MONO_BIT=7,

  MFP_GPIP_CENTRONICS_MASK=BIT_0,
  MFP_GPIP_DCD_MASK=BIT_1,
  MFP_GPIP_CTS_MASK=BIT_2,
  MFP_GPIP_BLITTER_MASK=BIT_3,
  MFP_GPIP_ACIA_MASK=BIT_4,
  MFP_GPIP_FDC_MASK=BIT_5,
  MFP_GPIP_RING_MASK=BIT_6,
  MFP_GPIP_MONO_MASK=BIT_7,

  // TIMERS

  N_MFP_TIMERS=4,N_MFP_PRESCALES=16,
  MFP_TIMER_A=0,MFP_TIMER_B,MFP_TIMER_C,MFP_TIMER_D,
  MFP_TIMER_DELAY_MASK=7,MFP_TIMER_EVENT_COUNT=8
};

#if defined(SSE_DEBUG) || defined(BCC_BUILD) || defined(SSE_ENABLE_TRACE_LOG)
extern char* mfp_reg_name[N_MFP_REGS]; //3.8.2
#endif

extern BYTE mfp_gpip_no_interrupt;

#define MFP_GPIP_COLOUR     0x80
#define MFP_GPIP_NOT_COLOUR 0x7F
#define MFP_CLK 2451
#define MFP_CLK_EXACT Mfp.xtal // it is used only in rs232

extern double CpuMfpRatio;

extern const BYTE interrupt_i_bit[N_MFP_IRQS+1];
extern const BYTE interrupt_i_ab[N_MFP_IRQS+1];


#pragma pack(push, 8)


struct TMC68901IrqInfo {
  unsigned int IsGpip:1;  // bad decision as sizeof is 4, but this struct is only really
  unsigned int IsTimer:1; // used by Stats report anyway, so performance doesn't matter
  unsigned int Timer:N_MFP_TIMERS;
};


struct TMC68901 {
  // FUNCTIONS
  TMC68901();
  void Restore();
  void Reset(bool Cold);
  char UpdateNextIrq(COUNTER_VAR at_time);
  void ComputeNextTimerB();
  // DATA
  COUNTER_VAR IrqSetTime;//unused - kept for snapshot compatibility
  COUNTER_VAR IackTiming;//unused
  COUNTER_VAR WriteTiming;//unused
  double Period[N_MFP_TIMERS]; // to record the period as double (debug use only for now)
  WORD IPR;//unused
  char Wobble[N_MFP_TIMERS]; // can be negative
  BYTE Counter[N_MFP_TIMERS],Prescale[N_MFP_TIMERS]; // to hold real values
  BYTE LastRegisterWritten;
  BYTE LastRegisterFormerValue;
  BYTE LastRegisterWrittenValue;
  BYTE Vector;//unused
  char NextIrq;
  char LastIrq;
  char IrqInService;//unused
  bool Irq;  // asserted or not, problem, we're not in real-time
  bool WritePending;//unused
  TMC68901IrqInfo IrqInfo[N_MFP_IRQS];
  //400
  BYTE reg[N_MFP_REGS]; // 24 directly addressable internal registers, each 8bit
  WORD *ier,*ipr,*isr,*imr; // for word access //we're little endian!!!
  COUNTER_VAR time_of_last_tb_tick;
  int nTimeouts[N_MFP_TIMERS];//debug
  BYTE tbctr_old;//another hack
  void CalcInterruptsEnabled();
  void CalcTimersEnabled();
  void CalcTimerPeriod(int t);
  void GetInterrupt(BYTE irq,COUNTER_VAR when_set); // MFP part or peripheral signals interrupt
  void UpdateTimerControls();
  BYTE GetTimerControlRegister(int n) {
    return timer_control[n];
  }
  BYTE timer_control[N_MFP_TIMERS]; // meta
  BYTE gpip_buffer; // looks like this thing should be persistent
  BYTE RegAorB(char irq)  {
    return interrupt_i_ab[(BYTE)irq];
  }
  BYTE IrqBit(char irq) {
    return interrupt_i_bit[(BYTE)irq];
  }
  void SetTimerReg(BYTE regnum,BYTE lobyte);
  BYTE CalcTimerCounter(BYTE ti,COUNTER_VAR t);
  void InitTimers();
  void Iack(char irq); // execute the interrupt acknowledge process
#ifdef DEBUG_BUILD
  bool SetPending(BYTE irq,COUNTER_VAR when_set);
#else
  void SetPending(BYTE irq,COUNTER_VAR when_set);
#endif
  void TimerATick();
  int SyncXtal(COUNTER_VAR system_time,int add_tclk_cycles);
  int xtal;
  void UpdateXtal();
  int timer_period_tclk[N_MFP_TIMERS]; // to record the timeout interval in xtal cycles
};

#pragma pack(pop)


extern const BYTE mfp_timer_irq[N_MFP_TIMERS];
extern const BYTE mfp_gpip_irq[N_MFP_GPIP_BITS];
extern const int mfp_timer_prescale[N_MFP_PRESCALES];
extern int mfp_timer_counter[N_MFP_TIMERS];
extern COUNTER_VAR mfp_timer_timeout[N_MFP_TIMERS];
extern COUNTER_VAR sys_time_of_first_mfp_tick;
extern bool mfp_timer_enabled[N_MFP_TIMERS];
extern bool mfp_timer_check[N_MFP_TIMERS];
extern int mfp_timer_period[N_MFP_TIMERS];
extern int mfp_timer_period_fraction[N_MFP_TIMERS];
extern int mfp_timer_period_current_fraction[N_MFP_TIMERS];
extern bool mfp_timer_period_change[N_MFP_TIMERS];
extern bool mfp_interrupt_enabled[N_MFP_IRQS];
void mfp_gpip_set_bit(int bit,bool set);

#define BYTE_00_TO_256(x) ( (int) ((unsigned char) (( (unsigned char)x )-1))  +1 )
#define MFP_IRQ ( *Mfp.ier & *Mfp.ipr & *Mfp.imr & (~*Mfp.isr) )

#endif//MFP_DECLA_H
