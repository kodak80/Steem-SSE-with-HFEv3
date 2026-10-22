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
FILE: acia.h
DESCRIPTION: Declarations for the ACIA MC6850 emulation.
It is high-level (byte per byte), which is sufficient to interface with the
6301 low-level emulation (also byte per byte).
struct TMC6850
acia.h must be compatible with C and with C++, it is included by 6310.c.
---------------------------------------------------------------------------*/

#pragma once
#ifndef ACIA_H
#define ACIA_H

#include <binary.h>

#ifdef __cplusplus
#include "options.h"
#include "emulator.h"
#include "conditions.h"
#endif

enum EAcia {  // global enum OK for C
  ACIA_OVERRUN_NO=0,ACIA_OVERRUN_COMING,ACIA_OVERRUN_YES,
  ACIA_IKBD=0,ACIA_MIDI,
  ACIA_CD1=BIT_0,ACIA_CD2=BIT_1,ACIA_WS1=BIT_2,ACIA_WS2=BIT_3,
  ACIA_WS3=BIT_4,ACIA_TC1=BIT_5,ACIA_TC2=BIT_6,ACIA_RIE=BIT_7,
  ACIA_RDRF=BIT_0,ACIA_TDRE=BIT_1,ACIA_DCD=BIT_2,ACIA_RTS=BIT_3,
  ACIA_FE=BIT_4,ACIA_OVRN=BIT_5,ACIA_PE=BIT_6,ACIA_IRQ=BIT_7          
};

// we do this for all our struct because it seemed to solve some complicated problem
// but maybe it's open for revision
#pragma pack(push, 8)

#ifdef __cplusplus
extern "C" {
#endif

struct TMC6850 {
  // DATA
  // we keep the start of the old structure for snapshot compatibility
  // (didn't think about that in intermediary versions so it may be compatible with v3.2 but not
  // later)
  int clock_divide;
  int rx_delay__unused;
  BYTE rx_irq_enabled;
  BYTE rx_not_read;
  int overrun;
  BOOL tx_flag;
  BYTE tx_irq_enabled;
  BYTE data;
  BYTE irq;
  COUNTER_VAR last_tx_write_time;
  COUNTER_VAR last_rx_read_time;
  BYTE LineRxBusy;// receiveing from 6301 or MIDI
  BYTE ByteWaitingRx; // unused
  BYTE ByteWaitingTx; // unused
  BYTE LineTxBusy;// transmitting to 6301 or MIDI
  BYTE cr;    // control 
  BYTE sr;    // status
  BYTE rdr;   // receive data 
  BYTE tdr;   // transmit data
  BYTE rdrs;  // receive data shift
  BYTE tdrs;  // transmit data shift
  BYTE Id;    // there are 2 ACIAs in the ST (0 keyboard, 1 MIDI)
  COUNTER_VAR TimeRx,TimeTx;
#ifdef __cplusplus
  // FUNCTIONS
  bool IrqForTx();
  static void SyncEClock();
  bool CheckIrq();
  void TransmitTDR();
  int TransmissionTime();
#endif
};

extern struct TMC6850 acia[2];

#pragma pack(pop)

#ifdef __cplusplus
}//extern "C"

void ACIA_CheckIrq(BYTE acia_num);
int ACIAClockToHBLS(int ClockDivide,bool MIDI_In=false);
void ACIA_Reset(BYTE nACIA,bool Cold);
void ACIA_SetControl(BYTE nACIA,BYTE Val);
void agenda_acia_tx_delay_IKBD(int),agenda_acia_tx_delay_MIDI(int);

#endif

#endif//#ifndef ACIA_H
