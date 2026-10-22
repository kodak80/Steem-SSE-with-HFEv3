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

Domain: Emu
FILE: mmu.h
DESCRIPTION: Declarations for the high-level MMU (Memory Management Unit)
emulation.
struct TMmu
---------------------------------------------------------------------------*/

#pragma once
#ifndef SSEMMU_H
#define SSEMMU_H

#include <data_union.h>
#include "conditions.h"
#include "SSE.h"


#pragma pack(push, 8)

struct TMmu {
  // FUNCTIONS
  void Restore();
  void Reset(bool Cold);
  MEM_ADDRESS ReadVideoCounter(short CyclesIn);
  void WriteVideoCounter(BYTE reg, BYTE io_src_b);
  void UpdateVideoCounter(short CyclesIn);
  void SetSoundControl(BYTE io_src_b); //v402
  bool SoundStop(); // returns true if stop (no loop)
  static void CALLBACK SoundFetch();
  MEM_ADDRESS BankLength(DWORD conf);
  //DATA
  MEM_ADDRESS VideoCounter; // to separate from rendering
/* //ok vs2015, not BCC
  DU32 uDmaCounter;
  MEM_ADDRESS &DmaCounter=uDmaCounter.d32; // disk operations
*/
  DU32 uDmaCounter;
  MEM_ADDRESS MonSTerHimem;

/*
Wake-up states (WU, WS)
This is important for timings relative to GLU "decisions", and is caused by
two 2bit counters, one in GLU, one in MMU, that get non deterministic
values at power up on the STF.
The relation between those counters causes a different latency between DE signal
and LOAD signal (=DL latency), one of four, in CPU cycles.

+------------------------------------------------------------+---------------+
| Steem  option    |              Wake-up concepts           |    Cycle      |
|    variable      |                                         |  adjustment   |
+------------------+---------------+------------+------------+-------+-------+
|  OPTION_WS       |   DL Latency  |     WU     |      WS    | SHIFT |  SYNC |
|                  |     (Dio)     |    (ijor)  |    (LJBK)  | (Res) |(Freq) |
+------------------+---------------+------------+------------+-------+-------+
|        1         |      3        |   2 (warm) |      2     |   +2  |   +2  |
|        2         |      4        |     2      |      4     |    -  |   +2  |
|        3         |      5        |   1 (cold) |      3     |    -  |    -  |
|        4         |      6        |     1      |      1     |   -2  |    -  |
+------------------+---------------+------------+------------+-------+-------+

'Cycle adjustment' is applied to apparent GLU timings.

On the STE, DL=6, WS=1.

*/
  BYTE WU[6]; // 0, 1, 2
  BYTE WS[6]; // 0 + 4 + panic
  char ResMod[6],FreqMod[6];
  BYTE DL[6];
  BYTE linewid,linewid0; // two variables for rendering purposes
  BYTE ExtraBytesForHscroll;
  BYTE Config; // 4bit register
  bool no_LW; // inhibition of 'add LINEWID'
  bool Confused; //ref. v402
  MEM_ADDRESS bank_length[2]; // length according to CONFIG register
  DU32 uVBase;
  DU32 uSoundFrameStart,uNextSoundFrameStart,uSoundFrameEnd,uNextSoundFrameEnd,uSoundFetchAd;
  BYTE SoundControl; // 2bit:
  enum { SOUNDPLAY=BIT_0,SOUNDLOOP=BIT_1 };
};

#pragma pack(pop)


//////////
// DISK //
//////////

extern MEM_ADDRESS &dma_address;


////////////
// MEMORY //
////////////

enum EMmu {MEMCONF_128,MEMCONF_512,MEMCONF_2MB,
// The MMU config never gets set to this, but we use it to distinguish between
// 640Kb (retained for legacy reasons) and 512Kb.
MEMCONF_0,
MEMCONF_7MB, // 14MB hack
#if defined(SSE_MMU_MONSTER_ALT_RAM)
MEMCONF_6MB, // MonSTer
#endif
N_MEMCONF,
MEMCONF_512K_BANK1_CONF=MEMCONF_0,MEMCONF_2MB_BANK1_CONF=MEMCONF_0
};


#define MEM_128KB         (128*1024)
#define MEM_512KB         (512*1024)
#define MEM_1MB           (1024*1024)
#define MEM_2MB           (2*1024*1024)
#define MEM_4MB           0x400000
#define MEM_12MB          0xC00000
#define MEM_14MB          0xE00000

extern BYTE *STMem; // ST RAM is allocated as needed according to the config
extern MEM_ADDRESS himem; // limit of ST RAM, eg 1024K

MEM_ADDRESS mmu_confused_address(MEM_ADDRESS ad);
#if !defined(SSE_DEBUGGER)
BYTE mmu_confused_peek(MEM_ADDRESS ad);
WORD mmu_confused_dpeek(MEM_ADDRESS ad);
#else
BYTE mmu_confused_peek(MEM_ADDRESS ad,bool bBErrOn);
WORD mmu_confused_dpeek(MEM_ADDRESS ad,bool bBErrOn);
LONG mmu_confused_lpeek(MEM_ADDRESS ad,bool bBErrOn);
#endif
void mmu_confused_poke_abus(BYTE x);
void mmu_confused_dpoke_abus(WORD x);
void GetCurrentMemConf(BYTE MemConf[2]);

EasyStr ReadStringFromMemory(MEM_ADDRESS ad,int max_len);
MEM_ADDRESS WriteStringToMemory(MEM_ADDRESS ad,char*c);

// used by asm_draw
#ifndef BIG_ENDIAN_PROCESSOR
extern "C" BYTE *Mem_End,*Mem_End_minus_1,*Mem_End_minus_2,*Mem_End_minus_4; // backwards in PC memory!
#endif
extern "C" DWORD mem_len;


/////////////////
// Sound (STE) //
/////////////////

// it's generally called DMA sound but it is confusing as DMA disk
// operations use bus arbitration, while sound is more like
// display through the Shifter, bus access is interleaved
extern MEM_ADDRESS &SteSndFrameStart,&NextSteSndFrameStart,&SteSndFrameEnd,&NextSteSndFrameEnd;
extern MEM_ADDRESS &SteSndFetchAd;

#if defined(SSE_SOUND_CARTRIDGE)
// hacking STE sound for emulation of the MV16 cartridge
void mv16_fetch(WORD data);
#endif


///////////
// VIDEO //
///////////

#define LINEWID Mmu.linewid0 //lock it before rendering
extern MEM_ADDRESS &vbase;

#endif//#ifndef SSEMMU_H
