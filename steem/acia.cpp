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
FILE: acia.cpp
DESCRIPTION: High level emulation of the MC6850.
Two Motorola MC6850 ACIA (Asynchronous Communications Interface
Adapter) chips are used in the ST to handle communication between the CPU and 
1) the keyboard chip, 2) the MIDI ports.
Serial communication happens bit by bit on a single line per direction.
There are two emulations, one based on agendas (not OPTION_C1, scanline
precision) and one based on events (OPTION_C1, cycle precision). Both
emulations directly handle bytes, not bits.
If SSE_NO_OLD_IKBD is defined, the aganda-based emulation is not compiled.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <computer.h>
#include <debug.h>
#if defined(SSE_VID_STVL1)
#include <interface_stvl.h>
#endif


#define LOGSECTION LOGSECTION_ACIA

int ACIAClockToHBLS(int ClockDivide,bool MIDI_In/*=false*/) {
  // We assume the default setting of 9 bits per byte sent, I'm not sure
  // if the STs ACIAs worked in any other mode. The ACIA master clock is 500kHz.
  //ASSERT(ClockDivide==1||ClockDivide==2);
  double dHBLs=(Glue.VideoFreq==MONO_HZ) ? HBLS_PER_SECOND_MONO : HBLS_PER_SECOND_AVE;
  dHBLs/=(ClockDivide==2) ? (500000.0/64.0/9.0) : (500000.0/16.0/9.0);
  if(MIDI_In && MIDI_in_speed!=100) 
  {
    dHBLs*=100;
    dHBLs/=MIDI_in_speed;
  }
  return (int)dHBLs+1; // +1?
}


void ACIA_Reset(BYTE nACIA,bool Cold) {
  TRACE_LOG("ACIA %d Reset (cold %d)\n",nACIA,Cold);
  acia[nACIA].tx_flag=FALSE;
  agenda_delete((nACIA==ACIA_IKBD)?agenda_acia_tx_delay_IKBD:agenda_acia_tx_delay_MIDI);
  acia[nACIA].rx_not_read=FALSE;
  acia[nACIA].overrun=0;
  acia[nACIA].clock_divide=(nACIA==ACIA_MIDI) ? 1 : 2;
  acia[nACIA].tx_irq_enabled=FALSE;
  acia[nACIA].rx_irq_enabled=TRUE;
  acia[nACIA].data=0; // ?
  acia[nACIA].last_tx_write_time=0;
  acia[nACIA].irq=FALSE;
  acia[nACIA].Id=nACIA;
  if(OPTION_C1)
  {
/*  "Master Reset clears the status register (except for external conditions
    on CTS, DCD) and initializes both the receiver and the transmitter."
    Master reset does not affect other control register bits.
*/
    acia[nACIA].sr=ACIA_TDRE;
    if(Cold)
      acia[nACIA].cr=ACIA_RIE; //?
    acia[nACIA].rdrs=acia[nACIA].tdrs=0;
    acia[nACIA].LineRxBusy=acia[nACIA].LineTxBusy=0;
  }
  if(!Cold)
    mfp_gpip_set_bit(MFP_GPIP_ACIA_BIT,!(acia[ACIA_IKBD].irq||acia[ACIA_MIDI].irq));
}


void ACIA_SetControl(BYTE nACIA,BYTE Val) { // only called by io_write()
  acia[nACIA].clock_divide=(Val&(ACIA_CD1|ACIA_CD2));
  acia[nACIA].tx_irq_enabled=((Val&(ACIA_TC1|ACIA_TC2))==ACIA_TC1);
  acia[nACIA].rx_irq_enabled=((Val&ACIA_RIE)!=0);
#if defined(SSE_HARDWARE_OVERSCAN)
  // The overscan circuit is activated by using the free ACIA RTS pin (output)
  if(OPTION_HWOVERSCAN && nACIA==ACIA_IKBD)
  {
    // this is saved with the snapshot
    SSEConfig.OverscanOn=((Val&ACIA_TC2)!=0) && ((Val&ACIA_TC1)==0);
#if defined(SSE_VID_STVL1)
    StvlUpdate();
#endif
  }
#endif
  if(OPTION_C1)
  {
    ACIA_CheckIrq(nACIA);
    return;
  }
#if !defined(SSE_NO_OLD_IKBD)
  if(acia[nACIA].tx_irq_enabled)
    acia[nACIA].irq=TRUE;
  else 
  {
    acia[nACIA].irq=FALSE;
  }
  mfp_gpip_set_bit(MFP_GPIP_ACIA_BIT,!(acia[ACIA_IKBD].irq||acia[ACIA_MIDI].irq));
#endif
}


void ACIA_CheckIrq(BYTE acia_num) {
  acia[acia_num].CheckIrq();
  // if an ACIA IRQ goes high, the active low MFP's GPIP ACIA bit goes low
  mfp_gpip_set_bit(MFP_GPIP_ACIA_BIT,
                   !((acia[ACIA_IKBD].sr&ACIA_IRQ)|(acia[ACIA_MIDI].sr&ACIA_IRQ)));
}


int TMC6850::TransmissionTime() {
/*  
Compute cycles according to bits 0-1 of cr
SS1 SS0                  Speed (bit/s)
 0   0    Normal            500000
 0   1    Div by 16          31250 (ST: MIDI)   cr&1=1
 1   0    Div by 64         7812.5 (ST: IKBD)   cr&1=0
MIDI transmission is 4 times faster than IKBD
*/
  int cycles=(cr&1) ? (10*16*16) : (10*16*64); //2560 for MIDI, 10240 for 6301
  cycles*=TICKS8;
  if(SSEConfig.CpuBoosted)
  {
    double m=cycles*cpu_cycles_multiplier;
    cycles=(int)m;
  }
  return cycles;
}


void TMC6850::SyncEClock() {
  // e-clock synchronisation on read/write, see TMC68000::<SyncEClock()
  // we already counted 4 cycles for the R/W so we do -4 here, important for
  // good sync with video interrupts: Mental Hangover
  int wait_states=6-4; 
  if(OPTION_C1)
  {
    BUS_WAIT_STATES(wait_states);
    wait_states=Cpu.SyncEClock();
    BUS_WAIT_STATES(wait_states+4); // +... +4
  }
#if !defined(SSE_NO_OLD_IKBD)
  else
  {
    wait_states+=4+(CpuNormalHz/TICKS8-(A_S_T/TICKS8-shifter_cycle_base/TICKS8))%10;
    BUS_WAIT_STATES(wait_states);
  }
#endif
}


bool TMC6850::CheckIrq() {
  bool IrqHigh=(IrqForTx() && (sr&ACIA_TDRE) // TX
    || (cr&ACIA_RIE) && (sr&(ACIA_RDRF|ACIA_OVRN))); //RX/OVR
  if(IrqHigh)
  {
    sr|=ACIA_IRQ;
    TRACE_LOG("ACIA %d IRQ, sr=%X\n",Id,sr);
  }
  else
    sr&=~ACIA_IRQ;
  return IrqHigh;
}


void TMC6850::TransmitTDR() {
  // The byte in TDR is moved into TDRS and transmission begins.
  // TDRE is set, and IRQ is asserted if appropriate.
  tdrs=tdr; // there could be timing difference for TDRS and TDRE... 
  sr|=ACIA_TDRE;
  if(CheckIrq())
    mfp_gpip_set_bit(MFP_GPIP_ACIA_BIT,false); //trigger
  LineTxBusy=1;
  TimeTx=time_of_next_event+TransmissionTime();
  if(TimeTx-time_of_event_acia<=0)
    time_of_event_acia=TimeTx;
}

bool TMC6850::IrqForTx() {
  return ((cr&ACIA_TC1) && !(cr&ACIA_TC2));
}

#undef LOGSECTION
