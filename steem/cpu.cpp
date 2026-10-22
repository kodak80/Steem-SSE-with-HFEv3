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
FILE: cpu.cpp
DESCRIPTION: Core definitions for the Motorola MC68000 emulation: process,
timings, exceptions, e-clock.
m68kProcess() is the central function, executing one instruction at a time
Opcodes are emulated in cpu_op.cpp, effective address is dealt with
in cpu_ea.cpp.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <gui.h>
#include <hd_gemdos.h>
#include <computer.h>
#include <osd.h>
#include <cpu_op.h>
#include <iolist.h>
#if defined(SSE_DEBUGGER)
#include <mem_browser.h>
#endif
#if defined(SSE_DEBUGGER_FRAME_REPORT)
#include <debug_framereport.h>
#endif
#if defined(SSE_VID_STVL1)
#include <interface_stvl.h>
#endif
#ifdef SSE_BETA
#include <68K_Disassembler/Disassembler.h>
#endif

/// pointers to functions
void (*m68k_call_table[0xffff+1])(); // opcode entry, big table
void (*pBusIdle)(int t);
void (*pBusWS)(int t); // wait states
void (*pBusPrefetchOnly)();
void (*pBusPrefetchFinal)();
void (*pBusPrefetchTotal)();
void (*pBusReadB)(); // read Byte
void (*pBusRead)();
void (*pBusWrite)();
void (*pBusWriteB)();
void (*pBusBltRead)(); // blitter
void (*pBusBltWrite)();

#if defined(SSE_DEFAULTS_421)
DWORD CpuNormalHz,CpuCustomHz;
#else
DWORD CpuNormalHz=CPU_CLOCK_STF_PAL;
DWORD CpuCustomHz=CPU_CLOCK_STF_PAL;
#endif
WORD &SR=Cpu.sr; // Status Register
#ifdef SSE_DEBUGGER
WORD debug_old_sr=0;
#endif
WORD &IRC=Cpu.irc; // Instruction Register Capture
WORD &IR=Cpu.ir;  // Instruction Register
WORD &IRD=Cpu.ird;  // Instruction Register Decode
DUS32 *cpureg=(DUS32*)&Cpu.r; // all registers
DU32 *cpuareg=(DU32*)&Cpu.r[8]; // address registers

// hardware does it differently, it's easier so in emulation (we swap)
MEM_ADDRESS &SP=(MEM_ADDRESS&)Cpu.r[15]; // A7
MEM_ADDRESS &other_sp=(MEM_ADDRESS&)Cpu.r[16];

MEM_ADDRESS &pc=Cpu.upc.d32; // address of next operand
WORD &pch=Cpu.upc.d16[HI],&pcl=Cpu.upc.d16[LO];
DU32 uiabus; // AOB
MEM_ADDRESS &iabus=uiabus.d32;
WORD &iabush=uiabus.d16[HI];
WORD &iabusl=uiabus.d16[LO];
DU32 ueffective_address;
DWORD &effective_address=ueffective_address.d32;
WORD &effective_address_h=ueffective_address.d16[HI];
WORD &effective_address_l=ueffective_address.d16[LO];
DU32 um68k_old_dest;
DWORD &m68k_old_dest=um68k_old_dest.d32;
WORD &m68k_old_dest_h=um68k_old_dest.d16[HI];
WORD &m68k_old_dest_l=um68k_old_dest.d16[LO];
WORD m68k_ap,m68k_iriwo;
SHORT m68k_src_w;
DUS32 sm68k_src_l;
LONG &m68k_src_l=sm68k_src_l.d32;
SHORT &m68k_src_lh=sm68k_src_l.d16[HI];
SHORT &m68k_src_ll=sm68k_src_l.d16[LO];
CHAR m68k_dst_b;
SHORT m68k_dst_w;
DUS32 sm68k_dst_l;
LONG &m68k_dst_l=sm68k_dst_l.d32;
SHORT &m68k_dst_lh=sm68k_dst_l.d16[HI];
SHORT &m68k_dst_ll=sm68k_dst_l.d16[LO];
CHAR m68k_src_b;

WORD ry,rx;

COUNTER_VAR a_s_t; // to record Absolute System Time
DUS32 uresult;
LONG &resultl=uresult.d32; //32bit internal register 
SHORT &resulth=uresult.d16[HI];
SHORT &resultw=uresult.d16[LO];
signed char &resultb=uresult.d8[LO];
DU64 uflags;

#ifdef BIG_ENDIAN_PROCESSOR
BYTE &pswT=uflags.d8[0];
BYTE &pswS=uflags.d8[1];
BYTE &pswI=uflags.d8[2];
BYTE &pswX=uflags.d8[3];
BYTE &pswN=uflags.d8[4];
BYTE &pswZ=uflags.d8[5];
BYTE &pswV=uflags.d8[6];
BYTE &pswC=uflags.d8[7];
#else
BYTE &pswT=uflags.d8[7];
BYTE &pswS=uflags.d8[6];
BYTE &pswI=uflags.d8[5];
BYTE &pswX=uflags.d8[4];
BYTE &pswN=uflags.d8[3];
BYTE &pswZ=uflags.d8[2];
BYTE &pswV=uflags.d8[1];
BYTE &pswC=uflags.d8[0];
#endif

extern const char* exception_action_name[4];
TMC68kException ExceptionObject;
jmp_buf *pJmpBuf=NULL;

COUNTER_VAR ipl_timing_time[256];
BYTE ipl_timing_ipl[256];
BYTE ipl_timing_index=0; // byte 0-255 should overflow

#define LOGSECTION LOGSECTION_CPU

COUNTER_VAR check_ipl(); // return ipl level IF above mask in SR, 0 otherwise


void m68k_trap1() { 
  // on the MC68000, illegal instructions including illegal addressing mode
  // decode to the trap1 exception microcode
  // older CPUs tried to execute them, with funny undocumented effects
  exception(EXCEPTION_ILLEGAL,EA_INST,0);
}


/*  For performance (?) and readability (?), we manipulate flags individually,
    as booleans (actual type is BYTE; the IPL bits are one variable), so
    everytime we need to read or write SR, we must update it first (read) or
    update flags after (write).
    Maybe we could do the same for just CCR? */

void update_sr_from_flags() {
  SR=(pswT<<15)|(pswS<<13)|(pswI<<8)|(pswX<<4)|(pswN<<3)|(pswZ<<2)|(pswV<<1)|pswC;
}


void update_flags_from_sr() {
  PSWT=((SR&SR_T)!=0);
  PSWS=((SR&SR_S)!=0);
  PSWI=((SR&SR_IPL)>>8);
  PSWX=((SR&SR_X)!=0);
  PSWN=((SR&SR_N)!=0);
  PSWZ=((SR&SR_Z)!=0);
  PSWV=((SR&SR_V)!=0);
  PSWC=((SR&SR_C)!=0);
}


#if defined(SSE_OPTION_FASTBLITTER)

void fastblit() {
#if defined(SSE_STATS)
  Stats.nFastBlit++;
#endif
}

#endif


/*  Adapt function pointers to the current configuration.
    There are functions for timing (cycle counting), for reading the bus, 
    including fetching. In some previous versions, the reading functions were
    different for STF and STE.
    Timing functions also call R/W functions.
*/
void SetTimingFunctions() {

#if defined(SSE_MEGA16)
#if defined(SSE_VID_STVL1)
  if(OPTION_C3 && hStvl)
  {
#if defined(SSE_HARDWARE_OVERSCAN)
/*  Since STVL v110, as an optimisation, the video logic for hardware overscan
    runs in an apart function We don't test the version, we test the function */
    video_logic_run=NULL;
    if(IS_STF)
    {
      if(SSEConfig.OverscanOn)
        video_logic_run=(void (STVL_CALLCONV*)(TStvl*,int))GetProcAddress(hStvl,"STVL_stf_hwo_run");
    }
    else
      video_logic_run=video_logic_ste_run;
    if(video_logic_run==NULL)
      video_logic_run=video_logic_stf_run;
#else
    video_logic_run=(IS_STF)?video_logic_stf_run:video_logic_ste_run;
#endif
    if(SSEConfig.CpuBoosted) // 2 low-level video, acceleration
    {
      if(IS_STE||SSEConfig.Mega) // high speed, no cache subtelty
      {
        pBusIdle=BusSte2Idle;
        pBusWS=Bus2WS;
        pBusPrefetchOnly=BusSte2PrefetchOnly;
        pBusPrefetchFinal=BusSte2PrefetchFinal;
        pBusPrefetchTotal=BusSte2PrefetchTotal;
        pBusReadB=BusSte2ReadB;
        pBusRead=BusSte2Read;
        pBusWrite=BusSte2Write;
        pBusWriteB=BusSte2WriteB;
        pBusBltRead=BusSte2BltRead;
        pBusBltWrite=BusSte2BltWrite;
      }
      else
      {
        pBusIdle=BusStf2Idle;
        pBusWS=Bus2WS;
        pBusPrefetchOnly=BusStf2PrefetchOnly;
        pBusPrefetchFinal=BusStf2PrefetchFinal;
        pBusPrefetchTotal=BusStf2PrefetchTotal;
        pBusReadB=BusStf2ReadB;
        pBusRead=BusStf2Read;
        pBusWrite=BusStf2Write;
        pBusWriteB=BusStf2WriteB;
      }
    }
    else // 1 low-level video, no acceleration
    {
      pBusWS=Bus1WS;
      if(SSEConfig.Mega) // Mega ST and Mega STE
      {        
        pBusIdle=BusMega1Idle;
        pBusPrefetchOnly=BusMega1PrefetchOnly;
        pBusPrefetchFinal=BusMega1PrefetchFinal;
        pBusPrefetchTotal=BusMega1PrefetchTotal;
        pBusReadB=BusMega1ReadB;
        pBusRead=BusMega1Read;
        pBusWrite=BusMega1Write;
        pBusWriteB=BusMega1WriteB;
        pBusBltRead=BusMega1BltRead;
        pBusBltWrite=BusMega1BltWrite;
      }
      else if(IS_STE)
      {
        pBusIdle=BusSte1Idle;
        pBusPrefetchOnly=BusSte1PrefetchOnly;
        pBusPrefetchFinal=BusSte1PrefetchFinal;
        pBusPrefetchTotal=BusSte1PrefetchTotal;
        pBusReadB=BusSte1ReadB;
        pBusRead=BusSte1Read;
        pBusWrite=BusSte1Write;
        pBusWriteB=BusSte1WriteB;
        pBusBltRead=BusSte1BltRead;
        pBusBltWrite=BusSte1BltWrite;
      }
      else // STF not Mega
      {
        pBusIdle=BusStf1Idle;
        pBusPrefetchOnly=BusStf1PrefetchOnly;
        pBusPrefetchFinal=BusStf1PrefetchFinal;
        pBusPrefetchTotal=BusStf1PrefetchTotal;
        pBusReadB=BusStf1ReadB;
        pBusRead=BusStf1Read;
        pBusWrite=BusStf1Write;
        pBusWriteB=BusStf1WriteB;
      }
    }//if(SSEConfig.CpuBoosted)
  }//if(OPTION_C3 && hStvl)
  else
#endif
  {
    pBusWS=BusWS;
    pBusBltRead=BusSteBltRead; // not used if no blitter
    pBusBltWrite=BusSteBltWrite; // not used if no blitter
    if(SSEConfig.Mega) // both Mega ST and Mega STE
    {
      pBusIdle=BusMegaIdle;
      pBusPrefetchOnly=BusMegaPrefetchOnly;
      pBusPrefetchFinal=BusMegaPrefetchFinal;
      pBusPrefetchTotal=BusMegaPrefetchTotal;
      pBusReadB=BusMegaReadB;
      pBusRead=BusMegaRead;
      pBusWrite=BusMegaWriteW;
      pBusWriteB=BusMegaWriteB;
    }
    else if(IS_STE)
    {
      pBusIdle=BusSteIdle;
      pBusPrefetchOnly=BusStePrefetchOnly;
      pBusPrefetchFinal=BusStePrefetchFinal;
      pBusPrefetchTotal=BusStePrefetchTotal;
      pBusReadB=BusSteReadB;
      pBusRead=BusSteRead;
      pBusWrite=BusSteWriteW;
      pBusWriteB=BusSteWriteB;
    }
    else // STF not Mega
    {
      pBusIdle=BusStfIdle;
      pBusPrefetchOnly=BusStfPrefetchOnly;
      pBusPrefetchFinal=BusStfPrefetchFinal;
      pBusPrefetchTotal=BusStfPrefetchTotal;
      pBusReadB=BusStfReadB;
      pBusRead=BusStfRead;
      pBusWrite=BusStfWriteW;
      pBusWriteB=BusStfWriteB;
    }
  }

#else//#if defined(SSE_MEGA16)

  if(IS_STE)
  {
#if defined(SSE_VID_STVL1) // those are defined in interface_stvl.cpp
    if(OPTION_C3 && hStvl)
    {
      if(SSEConfig.CpuBoosted)
      {
        // 2 low-level video, acceleration
        pBusIdle=BusSte2Idle;
        pBusWS=BusSte2WS;
        pBusPrefetchOnly=BusSte2PrefetchOnly;
        pBusPrefetchFinal=BusSte2PrefetchFinal;
        pBusPrefetchTotal=BusSte2PrefetchTotal;
        pBusReadB=BusSte2ReadB;
        pBusRead=BusSte2Read;
        pBusWrite=BusSte2Write;
        pBusWriteB=BusSte2WriteB;
        pBusBltRead=BusSte2BltRead;
        pBusBltWrite=BusSte2BltWrite;
      }
      else
      {
        // 1 low-level video, no acceleration
        pBusBltRead=BusSte1BltRead; // Blitter runs at 8MHz
        pBusBltWrite=BusSte1BltWrite;
#if defined(SSE_MEGASTE)
        if(IS_MEGASTE)
        {
          pBusIdle=BusMegaSte1Idle;
          pBusWS=BusMegaSte1WS;
          pBusPrefetchOnly=BusMegaSte1PrefetchOnly;
          pBusPrefetchFinal=BusMegaSte1PrefetchFinal;
          pBusPrefetchTotal=BusMegaSte1PrefetchTotal;
          pBusReadB=BusMegaSte1ReadB;
          pBusRead=BusMegaSte1Read;
          pBusWrite=BusMegaSte1Write;
          pBusWriteB=BusMegaSte1WriteB;
        }
        else
#endif
        {
          pBusIdle=BusSte1Idle;
          pBusWS=BusSte1WS;
          pBusPrefetchOnly=BusSte1PrefetchOnly;
          pBusPrefetchFinal=BusSte1PrefetchFinal;
          pBusPrefetchTotal=BusSte1PrefetchTotal;
          pBusReadB=BusSte1ReadB;
          pBusRead=BusSte1Read;
          pBusWrite=BusSte1Write;
          pBusWriteB=BusSte1WriteB;
        }
      }
    }
    else
#endif
    {
      // 0 no low-level video
      pBusBltRead=BusSteBltRead;
      pBusBltWrite=BusSteBltWrite;
#if defined(SSE_MEGASTE)
      if(IS_MEGASTE)
      {
        pBusIdle=BusMegaSteIdle;
        pBusWS=BusMegaSteWS;
        pBusPrefetchOnly=BusMegaStePrefetchOnly;
        pBusPrefetchFinal=BusMegaStePrefetchFinal;
        pBusPrefetchTotal=BusMegaStePrefetchTotal;
        pBusReadB=BusMegaSteReadB;
        pBusRead=BusMegaSteRead;
        pBusWrite=BusMegaSteWriteW;
        pBusWriteB=BusMegaSteWriteB;
      }
      else
#endif
      {
        pBusIdle=BusSteIdle;
        pBusWS=BusSteWS;
        pBusPrefetchOnly=BusStePrefetchOnly;
        pBusPrefetchFinal=BusStePrefetchFinal;
        pBusPrefetchTotal=BusStePrefetchTotal;
        pBusReadB=BusSteReadB;
        pBusRead=BusSteRead;
        pBusWrite=BusSteWriteW;
        pBusWriteB=BusSteWriteB;
      }
    }
  }
  else if(ST_MODEL==STF||ST_MODEL==STFM)
  {
#if defined(SSE_VID_STVL1)
    if(OPTION_C3 && hStvl)
    {
      if(SSEConfig.CpuBoosted)
      {
        // 2 low-level video, acceleration
        pBusIdle=BusStf2Idle;
        pBusWS=BusStf2WS;
        pBusPrefetchOnly=BusStf2PrefetchOnly;
        pBusPrefetchFinal=BusStf2PrefetchFinal;
        pBusPrefetchTotal=BusStf2PrefetchTotal;
        pBusReadB=BusStf2ReadB;
        pBusRead=BusStf2Read;
        pBusWrite=BusStf2Write;
        pBusWriteB=BusStf2WriteB;
      }
      else
      {
        // 1 low-level video, no acceleration
        pBusIdle=BusStf1Idle;
        pBusWS=BusStf1WS;
        pBusPrefetchOnly=BusStf1PrefetchOnly;
        pBusPrefetchFinal=BusStf1PrefetchFinal;
        pBusPrefetchTotal=BusStf1PrefetchTotal;
        pBusReadB=BusStf1ReadB;
        pBusRead=BusStf1Read;
        pBusWrite=BusStf1Write;
        pBusWriteB=BusStf1WriteB;
      }
    }
    else 
#endif
    {
      // 0 no low-level video
      pBusIdle=BusStfIdle;
      pBusWS=BusStfWS;
      pBusPrefetchOnly=BusStfPrefetchOnly;
      pBusPrefetchFinal=BusStfPrefetchFinal;
      pBusPrefetchTotal=BusStfPrefetchTotal;
      pBusReadB=BusStfReadB;
      pBusRead=BusStfRead;
      pBusWrite=BusStfWriteW;
      pBusWriteB=BusStfWriteB;
    }
  }
#if defined(SSE_MEGAST)
  else if(ST_MODEL==MEGA_ST)
  {
#if defined(SSE_VID_STVL1)
    if(OPTION_C3 && hStvl)
    {
      if(SSEConfig.CpuBoosted)
      {
        // 2 low-level video, acceleration
        pBusIdle=BusMegaSt2Idle;
        pBusWS=BusMegaSt2WS;
        pBusPrefetchOnly=BusMegaSt2PrefetchOnly;
        pBusPrefetchFinal=BusMegaSt2PrefetchFinal;
        pBusPrefetchTotal=BusMegaSt2PrefetchTotal;
        pBusReadB=BusMegaSt2ReadB;
        pBusRead=BusMegaSt2Read;
        pBusWrite=BusMegaSt2Write;
        pBusWriteB=BusMegaSt2WriteB;
        pBusBltRead=BusMegaSt2BltRead;
        pBusBltWrite=BusMegaSt2BltWrite;
      }
      else
      {
        // 1 low-level video, no acceleration
        pBusIdle=BusMegaSt1Idle;
        pBusWS=BusMegaSt1WS;
        pBusPrefetchOnly=BusMegaSt1PrefetchOnly;
        pBusPrefetchFinal=BusMegaSt1PrefetchFinal;
        pBusPrefetchTotal=BusMegaSt1PrefetchTotal;
        pBusReadB=BusMegaSt1ReadB;
        pBusRead=BusMegaSt1Read;
        pBusWrite=BusMegaSt1Write;
        pBusWriteB=BusMegaSt1WriteB;
        pBusBltRead=BusMegaSt1BltRead;
        pBusBltWrite=BusMegaSt1BltWrite;
      }
    }
    else
#endif
    {
      // 0 no low-level video
      pBusIdle=BusSteIdle;
      pBusWS=BusSteWS;
      pBusPrefetchOnly=BusStePrefetchOnly;
      pBusPrefetchFinal=BusStePrefetchFinal;
      pBusPrefetchTotal=BusStePrefetchTotal;
      pBusReadB=BusSteReadB;
      pBusRead=BusSteRead;
      pBusWrite=BusSteWriteW;
      pBusWriteB=BusSteWriteB;
      pBusBltRead=BusSteBltRead;
      pBusBltWrite=BusSteBltWrite;
    }
  }
#endif//#if defined(SSE_MEGAST)
#endif//#if defined(SSE_MEGA16)

#if defined(SSE_OPTION_FASTBLITTER)
  if(OPTION_FASTBLITTER)
    pBusBltRead=pBusBltWrite=fastblit;
#if defined(SSE_OPTION_FASTLINEA)
  if(lineA)
  {
    m68k_peek=m68k_peek_st_lineA;
    m68k_dpeek=m68k_dpeek_st_lineA;
    m68k_fetch=m68k_fetch_st_lineA;
    m68k_poke_abus=m68k_poke_abus_lineA;
    m68k_dpoke_abus=m68k_dpoke_abus_lineA;
  }
  else
#endif
#endif
  {
    m68k_peek=m68k_peek_st;
    m68k_dpeek=m68k_dpeek_st;
    m68k_fetch=m68k_fetch_st;
    m68k_poke_abus=m68k_poke_abus0;
    m68k_dpoke_abus=m68k_dpoke_abus0;
  }
}


/*  Specialised timing functions
    Simpler for STF, but to be strict there should be bus arbitration too
    however disk dma timings are less important than for the blitter so
    we can get away with it... TODO maybe, but there's a trade-off with load
*/


#define CHECK_BLIT_REQUEST   if(Blitter.Request) Blitter_CheckRequest()
#define BLIT_CYCLES          Blitter.BlitCycles
#define BLIT_BUS_ACCESS_CTR  Blitter.BusAccessCounter
#define BUS_IDLE_CYCLES      Cpu.BusIdleCycles

void BusStfIdle(int t) {
  BUS_MASK=0; // no bus access, so no wait states possible
  sys_cycles-=(t); // macros multiply t by TICKS8 if necessary
}


void BusWS(int t) {
  // wait states: don't change BUS_MASK
  sys_cycles-=(t); // macros multiply t by TICKS8 if necessary
}


#if !defined(SSE_MEGA16)
#if defined(SSE_MEGASTE)
void BusMegaSteIdle(int t) {
  BUS_MASK=0;
  while(BLIT_CYCLES>t && t>0)
  {
    BLIT_CYCLES--;
    t--;
  }
  BUS_IDLE_CYCLES+=t;
  if(Cpu16.ScuReg&2) // bit 1 = CPU clock
    t/=2; // to emulate 16MHz we just count half the cycles
          // that's why we call them system cycles now
  sys_cycles-=(t);
  CHECK_BLIT_REQUEST;
}
#endif

void BusStfWS(int t) {
  sys_cycles-=(t);
}


void BusSteWS(int t) {
  sys_cycles-=(t);
}


#if defined(SSE_MEGASTE)
void BusMegaSteWS(int t) { // same
  sys_cycles-=(t);
}


void BusMegaStePrefetchOnly() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  abus=pc&0xfffffe;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  BLIT_BUS_ACCESS_CTR++;
  if(Cpu16.IsFast(abus))
    sys_cycles-=2*TICKS8;
  else
  {
    sys_cycles-=4*TICKS8;
    if(abus<MEM_4MB)
    {
      RAM_ACCESS_WS
      Cpu16.Add(abus);
    }
  }
  IRC=m68k_fetch(pc);
  CHECK_BLIT_REQUEST;
}


void BusMegaStePrefetchFinal() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  IR=IRC;
  MEM_ADDRESS au=pc+2;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  abus=au&0xfffffe;
  BLIT_BUS_ACCESS_CTR++;
  if(Cpu16.IsFast(abus))
    sys_cycles-=2*TICKS8;
  else
  {
    sys_cycles-=4*TICKS8;
    if(abus<MEM_4MB)
    {
      RAM_ACCESS_WS
      Cpu16.Add(abus);
    }
  }
  IRC=m68k_fetch(au);
  CHECK_BLIT_REQUEST;
}


void BusMegaStePrefetchTotal() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  pc+=2;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  abus=pc&0xfffffe;
  BLIT_BUS_ACCESS_CTR++;
  if(Cpu16.IsFast(abus))
    sys_cycles-=2*TICKS8;
  else
  {
    sys_cycles-=4*TICKS8;
    if(abus<MEM_4MB)
    {
      RAM_ACCESS_WS
      Cpu16.Add(abus);
    }
  }
  IRC=m68k_fetch(pc);
  CHECK_BLIT_REQUEST;
}


void BusMegaSteReadB() {
  abus=iabus&0x00FFFFFE;
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_LOBYTE)
                     : (BUS_MASK_ACCESS|BUS_MASK_HIBYTE);
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  BLIT_BUS_ACCESS_CTR++;
  if(Cpu16.IsFast(abus))
    sys_cycles-=2*TICKS8;
  else
  {
    sys_cycles-=4*TICKS8;
    if(abus<MEM_4MB)
    {
      RAM_ACCESS_WS
      Cpu16.Add(abus);
    }
  }
  m68k_peek(iabus);
  CHECK_BLIT_REQUEST;
}


void BusMegaSteRead() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WORD;
  abus=iabus&0x00FFFFFE;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  BLIT_BUS_ACCESS_CTR++;
  if(Cpu16.IsFast(abus))
    sys_cycles-=2*TICKS8;
  else
  {
    sys_cycles-=4*TICKS8;
    if(abus<MEM_4MB)
    {
      RAM_ACCESS_WS
      Cpu16.Add(abus);
    }
  }
  dbus=m68k_dpeek(iabus);  
  CHECK_BLIT_REQUEST;
}


void BusMegaSteWriteB() {
  abus=iabus&0x00FFFFFE;
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_LOBYTE)
                     : (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_HIBYTE);
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  BLIT_BUS_ACCESS_CTR++;
  dbush=dbusl;
  sys_cycles-=4*TICKS8; // writes are not cached
  if(abus<MEM_4MB)
    RAM_ACCESS_WS
  m68k_poke_abus(dbusl);
  Cpu16.Add(abus);
  CHECK_BLIT_REQUEST;
}


void BusMegaSteWriteW() {
  abus=iabus&0x00FFFFFE;
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  BLIT_BUS_ACCESS_CTR++;
  sys_cycles-=4*TICKS8;
  if(abus<MEM_4MB)
    RAM_ACCESS_WS
  m68k_dpoke_abus(dbus);
  Cpu16.Add(abus);
  CHECK_BLIT_REQUEST;
}
#endif//#if defined(SSE_MEGASTE)
#endif//#if !defined(SSE_MEGA16)

void BusStfPrefetchOnly() { // PREFETCH_ONLY
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  sys_cycles-=4*TICKS8; // TODO make 4 a macro too?
  abus=pc&0xfffffe; // enforce 23bit address bus
  // rounding up to 4 for RAM access - Shifter is handled in IO
  // wait states possible on each ST RAM access (whether RAM is installed or not)
  // COMPROMISE: wrong for first 8 bytes which are ROM, but who's gonna fetch there?
  if(abus<MEM_4MB)
    RAM_ACCESS_WS
  // the CPU timing functions also perform the R/W
  IRC=m68k_fetch(pc);
}


void BusStfPrefetchFinal() { // PREFETCH_FINAL
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  IR=IRC;
  MEM_ADDRESS au=pc+2;
  abus=au&0xfffffe;
  sys_cycles-=4*TICKS8;
  if(abus<MEM_4MB)
    RAM_ACCESS_WS
  IRC=m68k_fetch(au);
}


void BusStfPrefetchTotal() { // PREFETCH
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  pc+=2;
  sys_cycles-=4*TICKS8;
  abus=pc&0xfffffe;
  if(abus<MEM_4MB)
    RAM_ACCESS_WS
  IRC=m68k_fetch(pc);
}


void BusStfReadB() {
  abus=iabus&0x00FFFFFE; // abus is 23bit, iabus is 32bit (CPU internal address bus)
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_LOBYTE) : (BUS_MASK_ACCESS|BUS_MASK_HIBYTE);
  sys_cycles-=4*TICKS8;
  if(abus<MEM_4MB && abus>=MEM_FIRST_WRITEABLE)
    RAM_ACCESS_WS
  m68k_peek(iabus);
}


void BusStfRead() {
  abus=iabus&0x00FFFFFE;
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WORD;
  sys_cycles-=4*TICKS8;
  if(abus<MEM_4MB && abus>=MEM_FIRST_WRITEABLE)
    RAM_ACCESS_WS
  dbus=m68k_dpeek(iabus);
}


void BusStfWriteB() {
  abus=iabus&0x00FFFFFE;
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_LOBYTE)
                     : (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_HIBYTE);
  sys_cycles-=4*TICKS8;
  if(abus<MEM_4MB) // for writes no need to test for reset vector, it crashes
    RAM_ACCESS_WS
  dbush=dbusl; // motorola quirk, assume correct byte is low order (our way)
  m68k_poke_abus(dbusl);
}


void BusStfWriteW() {
  abus=iabus&0x00FFFFFE;
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
  sys_cycles-=4*TICKS8;
  if(abus<MEM_4MB)
    RAM_ACCESS_WS
  m68k_dpoke_abus(dbus);
}


void BusSteIdle(int t) {
  BUS_MASK=0;
  while(Blitter.BlitCycles>t && t>0)
  {
    Blitter.BlitCycles--;
    t--; // CPU running during a blit (rare)
  }
  sys_cycles-=(t);
  Cpu.BusIdleCycles+=t;
  CHECK_BLIT_REQUEST;
}


void BusStePrefetchOnly() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  sys_cycles-=4*TICKS8;
  Blitter.BlitCycles=0;
  Cpu.BusIdleCycles=0;
  abus=pc&0xfffffe;
  if(abus<MEM_4MB)
    RAM_ACCESS_WS
  BLIT_BUS_ACCESS_CTR++; // 7bit + only if busy on HW but we must keep code lean
  IRC=m68k_fetch(pc);
  CHECK_BLIT_REQUEST;
}


void BusStePrefetchFinal() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  IR=IRC;
  MEM_ADDRESS au=pc+2;
  sys_cycles-=4*TICKS8;
  Blitter.BlitCycles=0;
  Cpu.BusIdleCycles=0;
  abus=au&0xfffffe;
  if(abus<MEM_4MB)
    RAM_ACCESS_WS
  BLIT_BUS_ACCESS_CTR++;
  IRC=m68k_fetch(au);
  CHECK_BLIT_REQUEST;
}


void BusStePrefetchTotal() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  pc+=2;
  sys_cycles-=4*TICKS8;
  Blitter.BlitCycles=0;
  Cpu.BusIdleCycles=0;
  abus=pc&0xfffffe;
  if(abus<MEM_4MB)
    RAM_ACCESS_WS
  BLIT_BUS_ACCESS_CTR++;
  IRC=m68k_fetch(pc);
  CHECK_BLIT_REQUEST;
}


void BusSteReadB() {
  abus=iabus&0x00FFFFFE;
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_LOBYTE) : (BUS_MASK_ACCESS|BUS_MASK_HIBYTE);
  sys_cycles-=4*TICKS8;
  Blitter.BlitCycles=0;
  Cpu.BusIdleCycles=0;
  if(abus<MEM_4MB && abus>=MEM_FIRST_WRITEABLE)
    RAM_ACCESS_WS
  BLIT_BUS_ACCESS_CTR++;
  m68k_peek(iabus);
  CHECK_BLIT_REQUEST;
}


void BusSteRead() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WORD;
  abus=iabus&0x00FFFFFE;
  sys_cycles-=4*TICKS8;
  Blitter.BlitCycles=0;
  Cpu.BusIdleCycles=0;
  if(abus<MEM_4MB && abus>=MEM_FIRST_WRITEABLE)
    RAM_ACCESS_WS
  BLIT_BUS_ACCESS_CTR++;
  dbus=m68k_dpeek(iabus);
  CHECK_BLIT_REQUEST;
}


void BusSteWriteB() {
  abus=iabus&0x00FFFFFE;
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_LOBYTE)
                     : (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_HIBYTE);
  sys_cycles-=4*TICKS8;
  Blitter.BlitCycles=0;
  Cpu.BusIdleCycles=0;
  if(abus<MEM_4MB)
    RAM_ACCESS_WS
  BLIT_BUS_ACCESS_CTR++;
  dbush=dbusl;
  m68k_poke_abus(dbusl);
  CHECK_BLIT_REQUEST;
}


void BusSteWriteW() {
  abus=iabus&0x00FFFFFE;
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
  sys_cycles-=4*TICKS8;
  Blitter.BlitCycles=0;
  Cpu.BusIdleCycles=0;
  if(abus<MEM_4MB)
    RAM_ACCESS_WS
  BLIT_BUS_ACCESS_CTR++;
  m68k_dpoke_abus(dbus);
  CHECK_BLIT_REQUEST;
}


void BusSteBltRead() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WORD;
  sys_cycles-=4*TICKS8;
  if(abus<MEM_4MB && abus>=MEM_FIRST_WRITEABLE)
    RAM_ACCESS_WS
  // peek is done by blitter
  BLIT_BUS_ACCESS_CTR++;
}


void BusSteBltWrite() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
  sys_cycles-=4*TICKS8;
  if(abus<MEM_4MB)
    RAM_ACCESS_WS
  // poke is done by blitter
  BLIT_BUS_ACCESS_CTR++;
}

#if defined(SSE_MEGA)

void BusMegaIdle(int t) {
  BUS_MASK=0;
  while(BLIT_CYCLES>t && t>0)
  {
    BLIT_CYCLES--;
    t--;
  }
  BUS_IDLE_CYCLES+=t;
  if(Cpu16.ScuReg&2) // bit 1 = CPU clock
    t/=2; // to emulate 16MHz we just count half the cycles
          // that's why we call them system cycles now
  sys_cycles-=(t);
  CHECK_BLIT_REQUEST;
}


void BusMegaPrefetchOnly() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  abus=pc&0xfffffe;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  BLIT_BUS_ACCESS_CTR++;
  if(Cpu16.IsFast(abus))
    sys_cycles-=2*TICKS8;
  else
  {
    sys_cycles-=4*TICKS8;
    if(abus<MEM_4MB)
    {
      RAM_ACCESS_WS
      Cpu16.Add(abus);
    }
  }
  IRC=m68k_fetch(pc);
  CHECK_BLIT_REQUEST;
}


void BusMegaPrefetchFinal() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  IR=IRC;
  MEM_ADDRESS au=pc+2;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  abus=au&0xfffffe;
  BLIT_BUS_ACCESS_CTR++;
  if(Cpu16.IsFast(abus))
    sys_cycles-=2*TICKS8;
  else
  {
    sys_cycles-=4*TICKS8;
    if(abus<MEM_4MB)
    {
      RAM_ACCESS_WS
      Cpu16.Add(abus);
    }
  }
  IRC=m68k_fetch(au);
  CHECK_BLIT_REQUEST;
}


void BusMegaPrefetchTotal() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
  pc+=2;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  abus=pc&0xfffffe;
  BLIT_BUS_ACCESS_CTR++;
  if(Cpu16.IsFast(abus))
    sys_cycles-=2*TICKS8;
  else
  {
    sys_cycles-=4*TICKS8;
    if(abus<MEM_4MB)
    {
      RAM_ACCESS_WS
      Cpu16.Add(abus);
    }
  }
  IRC=m68k_fetch(pc);
  CHECK_BLIT_REQUEST;
}


void BusMegaReadB() {
  abus=iabus&0x00FFFFFE;
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_LOBYTE) : (BUS_MASK_ACCESS|BUS_MASK_HIBYTE);
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  BLIT_BUS_ACCESS_CTR++;
  if(Cpu16.IsFast(abus))
    sys_cycles-=2*TICKS8;
  else
  {
    sys_cycles-=4*TICKS8;
    if(abus<MEM_4MB)
    {
      RAM_ACCESS_WS
      Cpu16.Add(abus);
    }
  }
  m68k_peek(iabus);
  CHECK_BLIT_REQUEST;
}


void BusMegaRead() {
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WORD;
  abus=iabus&0x00FFFFFE;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  BLIT_BUS_ACCESS_CTR++;
  if(Cpu16.IsFast(abus))
    sys_cycles-=2*TICKS8;
  else
  {
    sys_cycles-=4*TICKS8;
    if(abus<MEM_4MB)
    {
      RAM_ACCESS_WS
      Cpu16.Add(abus);
    }
  }
  dbus=m68k_dpeek(iabus);  
  CHECK_BLIT_REQUEST;
}


void BusMegaWriteB() {
  abus=iabus&0x00FFFFFE;
  BUS_MASK=(iabus&1) ? (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_LOBYTE)
                     : (BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_HIBYTE);
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  BLIT_BUS_ACCESS_CTR++;
  dbush=dbusl;
  sys_cycles-=4*TICKS8; // writes are not cached
  if(abus<MEM_4MB)
    RAM_ACCESS_WS
  m68k_poke_abus(dbusl);
  Cpu16.Add(abus);
  CHECK_BLIT_REQUEST;
}


void BusMegaWriteW() {
  abus=iabus&0x00FFFFFE;
  BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
  BLIT_CYCLES=0;
  BUS_IDLE_CYCLES=0;
  BLIT_BUS_ACCESS_CTR++;
  sys_cycles-=4*TICKS8;
  if(abus<MEM_4MB)
    RAM_ACCESS_WS
  m68k_dpoke_abus(dbus);
  Cpu16.Add(abus);
  CHECK_BLIT_REQUEST;
}

#endif//#if defined(SSE_MEGA)


void m68k_push_l_without_timing(DWORD const x) {
  SP-=4;
  iabus=SP;
#if !defined(BIG_ENDIAN_PROCESSOR)
  m68k_dpoke_abus(*((WORD*)&x+1));
  iabus+=2;
  m68k_dpoke_abus(*(WORD*)&x);
#else
  m68k_dpoke_abus(x>>16);
  iabus+=2;
  m68k_dpoke_abus(x&0xFFFF);
#endif
}


//This is to avoid code duplication
void m68k_finish_exception(MEM_ADDRESS const ad) {
/*
  Trace               | 36(4/3)  |              nn    ns nS ns nV nv np n np
  CHK Instruction     | 42(4/3)+ |   np (n-)    nn    ns nS ns nV nv np n np      
  Divide by Zero      | 40(4/3)+ |           nn nn    ns nS ns nV nv np n np      
  TRAP instruction    | 36(4/3)  |              nn    ns nS ns nV nv np n np      
  TRAPV instruction   | 36(5/3)  |   np               ns nS ns nV nv np n np   
  LINEA                                        [nn    ns nS ns nV nv np n np]
  LINEF                                        [nn    ns nS ns nV nv np n np]

Here we do:                                           ns nS ns nV nv np n np   
*/
  UPDATE_SR;
  WORD saved_sr=SR;
  change_to_supervisor_mode();
  CLEAR_T;
  iabus=SP-2;
  dbus=pcl; // stack PC low word
  CPU_BUS_ACCESS_WRITE; // ns
  iabus-=4;
  dbus=saved_sr; // stack SR
  CPU_BUS_ACCESS_WRITE; // ns
  SP=iabus;
  iabus+=2;
  dbus=pch; // PC high word 
  CPU_BUS_ACCESS_WRITE; // nS
  iabus=ad;
  CPU_BUS_ACCESS_READ; // nV
  effective_address_h=dbus;
  iabus+=2;
  CPU_BUS_ACCESS_READ; // nv
  effective_address_l=dbus;
  ASSERTB(Cpu.ProcessingState!=TMC68000::HALTED);
  //if(Cpu.ProcessingState!=TMC68000::HALTED) // normally not, just in case
  Cpu.ProcessingState=TMC68000::NORMAL; // before fetching PC
  m68kSetPC(effective_address,2); // where IPL will be scanned
  interrupt_depth++;
}


void m68k_interrupt(MEM_ADDRESS const ad) { 
/*  Only called by GEMDOS hack (os_gemdos_vector), no need for precise 
    CPU behaviour here
*/
  M68K_UNSTOP;
  change_to_supervisor_mode();
  CLEAR_T;
  m68k_PUSH_L(pc);
  UPDATE_SR;
  m68k_PUSH_W(SR);
  SET_PC(ad);
  interrupt_depth++;
}


#undef LOGSECTION
#define LOGSECTION LOGSECTION_CRASH

void exception(int const en,int const ea,MEM_ADDRESS const ad) {
  if(stem_runmode!=STEM_MODE_CPU)
    return;
  ioaccess=0;
  ExceptionObject.init(en,ea,ad);
#if defined(SSE_M68K_EXCEPTION_TRY_CATCH) // no because it's slow
  throw &ExceptionObject;
#else
  if(pJmpBuf==NULL)
  {
    if(en)
    {
      //BREAKPOINT(Unhandled exception!); //emulator crash on bad snapshot etc.
    }
    return;
  }
  longjmp(*pJmpBuf,1);
#endif
}


void TMC68kException::init(int en,int ea,MEM_ADDRESS ad) {
  bombs=en;
  u_pc.d32=pc;
  ucrash_address.d32=old_pc; //this is for group 1+2
  // this is where the EA_FETCH/EA_READ difference is important
  if((bombs==EXCEPTION_BUS_ERROR||bombs==EXCEPTION_ADDRESS_ERROR) && ea!=EA_FETCH)
    uaddress.d32=iabus;
  else
    uaddress.d32=ad;
  UPDATE_SR;
  crash_sr=SR;
  crash_ird=IRD;
  // we use bus mask instead of ea to assign action
  if((BUS_MASK&BUS_MASK_FETCH)==BUS_MASK_FETCH)
    action=EA_FETCH;
  else if((BUS_MASK&BUS_MASK_WRITE)==BUS_MASK_WRITE)
    action=EA_WRITE;
  else
    action=EA_READ;
}


void m68k_halt() { // only called by TMC68kException::crash()
  Cpu.ProcessingState=TMC68000::HALTED;
  // contrary to real ST we indicate the CPU's halted
  StatusInfo.MessageIndex=TStatusInfo::MC68000_CRASH;
  UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
#if !defined(SSE_HALT_CPUONLY)
  // the real ST's other chips continue running even if the CPU halted
  // in Steem SSE we stop emulation, it's a high-level simplification
  runstate=RUNSTATE_STOPPING;
#endif
}


void m68k_get_immediate_l_with_timing() {
  m68k_src_lh=IRC;
  PREFETCH; //np
  m68k_src_ll=IRC;
  PREFETCH; //np
}


void m68k_push_w(WORD x) {
  SP-=2;
  iabus=SP;
  m68k_dpoke_abus(x);
}


void m68k_write_long_result_backwards() {
  dbus=resultw;
  CPU_BUS_ACCESS_WRITE; //nw
  iabus-=2;
  dbus=resulth;
  CPU_BUS_ACCESS_WRITE; //nW
}


void m68k_push_l_with_timing(DU32 &x) {
  SP-=4;
  iabus=SP;
  dbus=x.d16[HI];
  CPU_BUS_ACCESS_WRITE; // nS
  iabus+=2;
  dbus=x.d16[LO];
  CPU_BUS_ACCESS_WRITE; // ns
}


void TMC68000::m68kUnstop() {
  if(ProcessingState==STOPPED)
  {
    ProcessingState=NORMAL; // will be exception soon
#ifdef DEBUG_BUILD
    m68kSetPC(pc+4,false); 
#endif
  }
}


void detect_change_to_user_mode() {
  if(!SUPERFLAG)
  {
    MEM_ADDRESS temp=SP;SP=other_sp;other_sp=temp;
#ifndef SSE_LEAN_AND_MEAN
    CLEAR_S; // already done anyway
#endif
  }
}


void change_to_supervisor_mode() {
  if(!SUPERFLAG)
  {
    MEM_ADDRESS temp=SP;SP=other_sp;other_sp=temp;
    SET_S;
  }
}



/*  In our emulation, the crash exception process is handled in one big function, just
*   like normal instruction execution is handled in another function (and subfunctions),
*   and interrupts are handled in yet other functions.
*   The real CPU handles exceptions as a state machine using nanocodes, just like instructions.
*/
void TMC68kException::crash() {
  ASSERTB(bombs<12);
#if defined(SSE_STATS)
  Stats.nException[bombs]++;
#endif
  if(bombs==EXCEPTION_RESET)
  {
    reset_st(uaddress.d32|RESET_STAGE2);  // address = flags of reset_st (internal trick)
    return;
  }
#if defined(SSE_MEGA_421)
//  else if(bombs==EXCEPTION_BUS_ERROR)
//    Cpu16.Reset(); // too slow
#endif
  M68K_UNSTOP;
#if defined(SSE_DEBUGGER_FAKE_IO)
  if(OSD_MASK_CPU & OSD_CONTROL_CPUBOMBS)
#endif
  {
    TRACE_OSD_DBG("%s %d","EXC",bombs);
  }
#if defined(SSE_ENABLE_TRACE_LOG)
  if(Cpu.nExceptions!=-1)
  {
    Cpu.nExceptions++;  
    TRACE_LOG("\nException #%d, %d bombs (",Cpu.nExceptions,bombs);
    switch(bombs) {  
#ifndef SSE_LEAN_AND_MEAN // don't come here
    case EXCEPTION_RESET:
      TRACE_LOG("EXCEPTION_RESET"); 
      break;
#endif
    case EXCEPTION_BUS_ERROR:
      TRACE_LOG("EXCEPTION_BUS_ERROR"); 
      break;
    case EXCEPTION_ADDRESS_ERROR:
      TRACE_LOG("EXCEPTION_ADDRESS_ERROR"); 
      break;
    case EXCEPTION_ILLEGAL:
      TRACE_LOG("EXCEPTION_ILLEGAL"); 
      break;
#ifndef SSE_LEAN_AND_MEAN // don't come here
    case EXCEPTION_DIVISION_BY_ZERO:
      TRACE_LOG("EXCEPTION_DIVISION_BY_ZERO"); 
      break;
    case EXCEPTION_CHK:
      TRACE_LOG("EXCEPTION_CHK"); 
      break;
    case EXCEPTION_TRAPV:
      TRACE_LOG("EXCEPTION_TRAPV"); 
      break;
#endif
    case EXCEPTION_PRIVILEGE_VIOLATION:
      TRACE_LOG("EXCEPTION_PRIVILEGE_VIOLATION"); 
      break;
#ifndef SSE_LEAN_AND_MEAN // don't come here
    case EXCEPTION_TRACE_EXCEPTION:
      TRACE_LOG("EXCEPTION_TRACE_EXCEPTION"); 
      break;
    case EXCEPTION_LINE_A:
      TRACE_LOG("EXCEPTION_LINE_A"); 
      break;
    case EXCEPTION_LINE_F:
      TRACE_LOG("EXCEPTION_LINE_F"); 
      break;
#endif
    }//sw
#ifdef DEBUG_BUILD
    TRACE_LOG(") during \"%s\"\n",exception_action_name[action]);
    // take advantage of the disassembler
    // disassembly can be wrong if word at old_pc has changed!
    EasyStr instr=disa_d2(old_pc);
    TRACE_LOG("PC=%X-IRD=%04X-Ins: %s -SR=%04X-Bus=%06X",old_pc,crash_ird,instr.Text,crash_sr,abus);
#else
    TRACE_LOG(") action %d\n",action);
    TRACE_LOG("PC=%X-IRD=%04X-SR=%04X-Bus=%06X",old_pc,crash_ird,crash_sr,iabus);
#endif

    TRACE_LOG("-Vector $%X=%08X\n",bombs*4,SafeLPeek(bombs*4));
    // dump registers
    TRACE_LOG("D0=%X D1=%X D2=%X D3=%X D4=%X D5=%X D6=%X D7=%X\n",
              Cpu.r[0],Cpu.r[1],Cpu.r[2],Cpu.r[3],Cpu.r[4],Cpu.r[5],Cpu.r[6],Cpu.r[7]);
    TRACE_LOG("A0=%X A1=%X A2=%X A3=%X A4=%X A5=%X A6=%X A7=%X\n",Cpu.r[8],
              Cpu.r[9],Cpu.r[10],Cpu.r[11],Cpu.r[12],Cpu.r[13],Cpu.r[14],Cpu.r[15]);
  }
#endif
  bool inExcept01=(Cpu.ProcessingState==TMC68000::EXCEPTION); // but which exception?
  Cpu.ProcessingState=TMC68000::EXCEPTION;
  Cpu.tpend=false; // crash cancels trace (fortunately because we're out of the process function anyway)
  if(bombs==EXCEPTION_ILLEGAL||bombs==EXCEPTION_PRIVILEGE_VIOLATION)
  {
    // Illegal Instruction | 34(4/3)  |              nn    ns nS ns nV nv np n np
    CPU_BUS_IDLE(4); //nn
    change_to_supervisor_mode();
    CLEAR_T;
    UPDATE_SR;
    TRACE_LOG("Push PC %X on %X, SR %04X on %X\n",ucrash_address.d32,SP-4,crash_sr,SP-6);
    iabus=SP-2;
    dbus=ucrash_address.d16[LO]; // stack PC low word
    CPU_BUS_ACCESS_WRITE; // ns 
    iabus-=4;
    dbus=crash_sr; // SR written between two parts of PC
    SP=iabus;
    CPU_BUS_ACCESS_WRITE; // ns
    iabus+=2;
    dbus=ucrash_address.d16[HI]; // PC high word 
    CPU_BUS_ACCESS_WRITE; // nS      
    iabus=bombs*4;
    CPU_BUS_ACCESS_READ; // nV
    effective_address_h=dbus;
    iabus+=2;
    CPU_BUS_ACCESS_READ; // nv
    effective_address_l=dbus;
    MEM_ADDRESS ad=effective_address;
    if(ad&1) // bad vector!
    {
      // Very rare, generally indicates emulation/snapshot bug, but there are real cases
      bombs=EXCEPTION_ADDRESS_ERROR;
#if defined(SSE_STATS)
      Stats.nException[bombs]++;
#endif
#if defined(SSE_ENABLE_TRACE_LOG)
      Cpu.nExceptions++;
      TRACE_LOG("->%d bombs\n",bombs);
#endif
      uaddress.d32=ad;
      action=EA_FETCH;
    }
    else
    {
      TRACE_LOG("PC = %X\n\n",ad);
      Cpu.ProcessingState=TMC68000::NORMAL;
      m68kSetPC(ad,2);
      interrupt_depth++;
    }
  }
  if(bombs==EXCEPTION_BUS_ERROR||bombs==EXCEPTION_ADDRESS_ERROR)
  {
/*
Address error       | 50(4/7)  |     nn ns nS ns ns ns nS ns nV nv np n np
Bus error           | 50(4/7)  |     nn ns nS ns ns ns nS ns nV nv np n np
*/
    // MC68000 bug documented by ijor, for the explanation see:
    // http://www.atari-forum.com/viewtopic.php?f=68&t=37890#p387686
    // it's not fancy to support it because the opcode and the ssw are changed
    if(crash_ird!=IR) // simplification, no overhead cost
    {
      crash_ird=IR;
      TRACE_LOG("TVN latched IR %04X I/N %d\n",crash_ird,inExcept01);
      // test is heavy but it's rarely necessary
      if(Cpu.tpend || check_ipl() || m68k_call_table[crash_ird]==m68k_trap1
        || (crash_sr&0x2000)==0 && Cpu.IsPriv(crash_ird)
        || m68k_call_table[crash_ird]==m68k_lineA
        || m68k_call_table[crash_ird]==m68k_lineF)
        inExcept01=true; // I/N bit affected
    }
    // The GLUE contains a 6bit counter that asserts BERR if AS stays asserted 
    // for more than 64 cycles. 64 cycles + 4 see error + 2 internal CPU propagation?
    // We don't emulate this at low level so we count cycles here.
    int ncycles=(bombs==EXCEPTION_BUS_ERROR) ? (64+4+2) : (4); // timing on STE (BUSERRT1.TOS)
    for(int i=0;i<ncycles;i+=2)
    {
      BUS_WAIT_STATES(2); // just in case, avoid too long CPU timings
    }
    BUS_WAIT_STATES(4); //nn
    change_to_supervisor_mode();
    CLEAR_T;
    UPDATE_SR;
    TRY_M68K_EXCEPTION
      if(u_pc.d32!=Cpu.Pc)
      {
        TRACE_LOG("pc %X true PC %X\n",u_pc.d32,Cpu.Pc);
        u_pc.d32=Cpu.Pc; // guaranteed exact...
      }
      TRACE_LOG("Push PC %X on %X, SR %04X on %X\n",u_pc.d32,SP-4,crash_sr,SP-6);
      iabus=SP-2;
      dbus=u_pc.d16[LO]; // stack PC low word
      CPU_BUS_ACCESS_WRITE; // ns 
      iabus-=4;
      dbus=crash_sr; // SR written between two parts of PC
      CPU_BUS_ACCESS_WRITE; // ns
      SP=iabus;
      iabus+=2;
      dbus=u_pc.d16[HI]; // PC high word 
      CPU_BUS_ACCESS_WRITE; // nS      
      TRACE_LOG("Push IR %X on %X\n",crash_ird,SP-2);
      iabus=SP-2;
      dbus=crash_ird;
      SP=iabus;
      CPU_BUS_ACCESS_WRITE; // ns
      // special status word SSW: 5 bits only, the rest is previous ird
      // (because ftu wasn't reset between the two writes - ijor)
      WORD ssw=crash_ird & 0xffe0;
      // ssw <= { ~bciWrite, inExcept01, rFC};
      if(action!=EA_WRITE)
        ssw|=B6_010000;
      if(inExcept01)
        ssw|=B6_001000;
      if(crash_sr & SR_S)
        ssw|=B6_000100;
      if(action==EA_FETCH)
        ssw|=B6_000010;
      else
        ssw|=B6_000001;
      TRACE_LOG("Push crash address %X on %X, ssw %04X on %X\n",uaddress.d32,SP-4,ssw,SP-6);
      iabus=SP-2;
      dbus=uaddress.d16[LO]; // stack crash address low word
      CPU_BUS_ACCESS_WRITE; // ns 
      iabus-=4;
      dbus=ssw;
      CPU_BUS_ACCESS_WRITE; // ns
      SP=iabus;
      iabus+=2;
      dbus=uaddress.d16[HI]; // crash address high word 
      CPU_BUS_ACCESS_WRITE; // nS      
      iabus=bombs*4;
      CPU_BUS_ACCESS_READ; // nV
      effective_address_h=dbus;
      iabus+=2;
      CPU_BUS_ACCESS_READ; // nv
      effective_address_l=dbus;
      iabus=effective_address;
      TRACE_LOG("PC = %X\n",iabus);
      if(Cpu.ProcessingState!=TMC68000::HALTED)
        Cpu.ProcessingState=TMC68000::NORMAL;
      m68kSetPC(iabus,2);
    CATCH_M68K_EXCEPTION
      // bus/address exception on bus/address error exception handling halts the CPU
      // there are hardware ways to restarts it (ijor)
      TRACE("HALT PC %X SR %X address %X Exception %d dbus %X abus %X\n",u_pc.d32,crash_sr,uaddress.d32,bombs,dbus,iabus); 
      m68k_halt(); // bus/address error in group 0 exception
      return;
    END_M68K_EXCEPTION
    interrupt_depth++;
  }
  if(!OPTION_EMUTHREAD)
    PeekEvent(); // Stop exception freeze
}

#undef LOGSECTION


#if defined(DEBUG_BUILD)

void DebugCheckIOAccess() {
  if(ioaccess & IOACCESS_DEBUG_MEM_WRITE_LOG)
  {
    /*int val=int((debug_mem_write_log_bytes==1) 
      ? int(d2_peek(debug_mem_write_log_address)) 
      : int(d2_dpeek(debug_mem_write_log_address))); */
    int val=d2_peek(debug_mem_write_log_address);
    int val2=(debug_mem_write_log_address&1) ? 0 : d2_dpeek(debug_mem_write_log_address);
    if(ioaccess&IOACCESS_DEBUG_MEM_DMA)
      TRACE("DMA from disk $%X $%X\n",debug_mem_write_log_address,val2);
    else
      TRACE("PC %6X %s write %X|%X to %X\n",old_pc,disa_d2(old_pc).Text,
        val,val2,debug_mem_write_log_address);
  }
  if(ioaccess & IOACCESS_DEBUG_MEM_READ_LOG)
  {
    int val=d2_peek(debug_mem_write_log_address);
    int val2=(debug_mem_write_log_address&1) ? 0 : d2_dpeek(debug_mem_write_log_address);
    int val3=(debug_mem_write_log_address&1) ? 0 : d2_lpeek(debug_mem_write_log_address);
    if(ioaccess&IOACCESS_DEBUG_MEM_DMA)
      TRACE("DMA to disk $%X $%X\n",debug_mem_write_log_address,val2);
    else
      TRACE("PC %X %s read %X|%X|%X from %X\n",old_pc,disa_d2(old_pc).Text,
            val,val2,val3,debug_mem_write_log_address);
  }
  ioaccess&=~(IOACCESS_DEBUG_MEM_WRITE_LOG|IOACCESS_DEBUG_MEM_READ_LOG|IOACCESS_DEBUG_MEM_DMA);
}

#endif


/*  this function is only called to check some MC68000 crashes
    it's less heavy than a flag in a big opcode table
    or a flag that would be cleared by the process loop, set by privileged
    instructions  
*/
bool TMC68000::IsPriv(WORD op) { 
  bool is_priv=(op==0x4E70 || op==0x4E72 || op==0x4E73 // reset||stop||rte
   || m68k_call_table[op]==m68k_ori_w_to_sr
   || m68k_call_table[op]==m68k_andi_w_to_sr
   || m68k_call_table[op]==m68k_eori_w_to_sr
   || m68k_call_table[op]==m68k_move_to_sr
   || m68k_call_table[op]==m68k_move_to_usp
   || m68k_call_table[op]==m68k_move_from_usp);
  return is_priv;
}


#define LOGSECTION LOGSECTION_INTERRUPTS


COUNTER_VAR tvn_latch_time=0; // updated by CPU opcode emulation

/*  check if tvn (trap vector number) was set for interrupts this instruction
    we look back because event may trigger right after the instruction
    called by m68kProcess() and TMC68kException::crash()
*/


COUNTER_VAR check_ipl() {
#if 1//TEST
//#ifdef DEBUG_BUILD
  if(stem_runmode==STEM_MODE_CPU)
//#endif
  // if() should be enough, while() better but risk of hanging at 0
  if(sys_cycles<=0) 
  {
#if defined(SSE_DEBUGGER_FAKE_IO)
    if(TRACE_MASK2&TRACE_CONTROL_EVENT)
    {
      TRACE_EVENT("Check IPL",event_vector);
    }
#endif
    event_vector();
    prepare_next_event();
  }
#else // more complex and safe
  if(stem_runmode==STEM_MODE_CPU)
  {
    for(int i=0;sys_cycles<=0&&i<10;i++)
    {
#if defined(SSE_DEBUGGER_FAKE_IO)
      if(TRACE_MASK2&TRACE_CONTROL_EVENT)
        TRACE_EVENT("Check IPL",event_vector);
#endif
      event_vector();
      prepare_next_event();
    }
  }
#endif
  // get correct ipl
  COUNTER_VAR ipl;
  BYTE look_back_index=ipl_timing_index;
  // tvn_latch_time = 1 cycle before rInterrupt latch
  //                  0.5 cycle after ipl should be lastly updated to be stable 
  while(tvn_latch_time-ipl_timing_time[look_back_index]<DBI_DELAY)
  {
    look_back_index--;
    // necessary test, happens on reset
    if(look_back_index==ipl_timing_index)
    {
      TRACE_LOG("ipl overflow latch " PRICV " time " PRICV " ipl %d\n",tvn_latch_time,
                ipl_timing_time[look_back_index],ipl_timing_ipl[look_back_index]);
      TRACE_LOG("%s %d (%x) %s %d %s %d\n","MFP",Mfp.Irq,MFP_IRQ,"VSYNC",Glue.vbl_pending,
                "HSYNC",Glue.hbl_pending);
      update_ipl(A_S_T); // get current ipl regardless of timing
      look_back_index=ipl_timing_index;
      break;
    }
  }
  ipl=ipl_timing_ipl[look_back_index];
  // return ipl level IF above mask, 0 otherwise
  return ( (ipl>pswI) ? ipl : 0);
}


#if defined(SSE_LOGSECTIONS_FOR_ALL) && !defined(SSE_DEBUGGER)

EasyStr disa(MEM_ADDRESS ad) {
  // reverse memory and use 3rd party disassembler
  DWORD decoblock[2];
  decoblock[0]=SafeLPeek(ad);
  decoblock[1]=SafeLPeek(ad+4);
  SWAP_BIG_ENDIAN_DWORD(decoblock[0]);
  SWAP_BIG_ENDIAN_DWORD(decoblock[1]);
  char *decopt=(char*)&decoblock;
  int offset=0;
  EasyStr sdisa=Disassembler::parseInstruction(decopt,offset); // 3rd party
  return sdisa;
}

#endif

void m68kProcess() { 

#if defined(SSE_DEBUGGER)
  WORD tir=0;

  // if instruction before pc is TRAP, try to show return value
  if(TRACE_LOG_ENABLED(LOGSECTION_TRAP)&&TRACE_LOG2_ENABLED)
  {
    tir=d2_dpeek(pc-2);
    if((tir&0xFFF0)==0x4E40)
    {
      bool DoTrace=false;
      switch(tir) {
      case 0x4E41:
        if(TRACE_LOG_ENABLED(LOGSECTION_GEMDOS)) // double condition
          DoTrace=true;
        break;
      case 0x4E42:
        if(TRACE_LOG_ENABLED(LOGSECTION_GEM)) // VDI+AES
        {
          WORD aes=d2_dpeek(aes_intout);
          WORD vdi=d2_dpeek(vdi_intout);
          TRACE("intout AES (%X) %08X (%d) VDI (%X)  %08X (%d)\n",aes_intout,aes,aes,vdi_intout,vdi,vdi);
          DoTrace=true;
        }
        break;
      case 0x4E4D:
        if(TRACE_LOG_ENABLED(LOGSECTION_BIOS))
          DoTrace=true;
        break;
      case 0x4E4E:
        if(TRACE_LOG_ENABLED(LOGSECTION_XBIOS))
          DoTrace=true;
        break;
      default:
        DoTrace=true;
      }//sw
      if(DoTrace)
        TRACE("(%06X) TRAP#%d D0 %08X (%d)\n",pc-2,(tir&0x0F),REGL(0),REGL(0));
    }
  }

#if defined(SSE_DEBUG_SYMBOLS)
  if(Tos.TrackSymbols&&Tos.LastFunc==TTos::RWABS)
  {
    if(!tir)
      tir=SafeDPeek(pc-2);
    if(tir==0x4E4D) // trap BIOS
    {
      Tos.LastFunc=0;
      // buffer is at sp+4, count at sp+8
      MEM_ADDRESS buffer=SafeLPeek(AREG(7)+4);
      WORD count=SafeDPeek(AREG(7)+8);
      WORD recno=SafeDPeek(AREG(7)+8+2);
      if(Tos.fpPrgCopy==NULL)
      {
        WORD PRG_magic=SafeDPeek(buffer);
        if(PRG_magic==0x601a)
        {
          Tos.fpPrgCopy=FOPEN("stprg.bin","wb+");
          // info from file header
          Tos.PRG_tsize=SafeLPeek(buffer+0x02);
          Tos.PRG_dsize=SafeLPeek(buffer+0x06);
          Tos.PRG_bsize=SafeLPeek(buffer+0x0A);
          Tos.PRG_ssize=SafeLPeek(buffer+0x0E);
          Tos.first_recno=recno;
        }
      }
      // copy sector to temp file
      // it's not just the ST file, could be FAT reads, we check recno (poor test)
      //TRACE("recno %d Tos.first_recno %d\n",recno,Tos.first_recno);
      if(Tos.fpPrgCopy!=NULL && recno>=Tos.first_recno)
      {
        int cluster_size=(Stemdos.CurrentDrive>=2)?SECTOR_SIZE*2:SECTOR_SIZE; // 1024byte on hard drive
        STfile_write_from_ST_memory(Tos.fpPrgCopy,buffer,count*cluster_size);
      }
    }//if(tir==0x4E4D)
  }//if(Tos.TrackSymbols&&Tos.LastFunc==TTos::RWABS)
#endif

/*  Very powerful but demanding traces.
    You may have, beside the disassembly, cycles (absolute, frame, line) and
    registers.
*/
  if(TRACE_LOG_ENABLED(LOGSECTION_CPU))
  {
#if defined(SSE_DEBUGGER_FAKE_IO)
    if(TRACE_MASK4 & TRACE_CONTROL_CPU_CYCLES)
    {
      TRACE("Cycles " PRICV " %d %d (%d)\n",A_S_T,FRAMECYCLES,LINECYCLES,scan_y);
    }
    if(TRACE_MASK4 & TRACE_CONTROL_CPU_REGISTERS)
    {
      UPDATE_SR;
      TRACE("SR=%04X D0=%X D1=%X D2=%X D3=%X D4=%X D5=%X D6=%X D7=%X\n",
                SR,Cpu.r[0],Cpu.r[1],Cpu.r[2],Cpu.r[3],Cpu.r[4],Cpu.r[5],Cpu.r[6],Cpu.r[7]);
      TRACE("PC=%X A0=%X A1=%X A2=%X A3=%X A4=%X A5=%X A6=%X A7=%X\n",
                pc,Cpu.r[8],Cpu.r[9],Cpu.r[10],Cpu.r[11],Cpu.r[12],Cpu.r[13],Cpu.r[14],SP);
    }
#endif
    // makes a difference when there's a prefetch trick (only IR is correct) TODO
    if(PSWT)
      TRACE("(T) %X: %04X %04X %s\n",pc,IR,IRC,disa_d2(pc,IR).Text); // IRD not valid yet
    else
      TRACE("%X: %04X %04X %s\n",pc,IR,IRC,disa_d2(pc,IR).Text);
  }

#endif//SSE_DEBUGGER

#if defined(SSE_LOGSECTIONS_FOR_ALL) && !defined(SSE_DEBUGGER)
  if(TRACE_LOG_ENABLED(LOGSECTION_CPU))
  {
    if(TRACE_LOG3_ENABLED)
    {
      UPDATE_SR;
      TRACE("SR=%04X D0=%X D1=%X D2=%X D3=%X D4=%X D5=%X D6=%X D7=%X\n",
                SR,Cpu.r[0],Cpu.r[1],Cpu.r[2],Cpu.r[3],Cpu.r[4],Cpu.r[5],Cpu.r[6],Cpu.r[7]);
      TRACE("PC=%X A0=%X A1=%X A2=%X A3=%X A4=%X A5=%X A6=%X A7=%X\n",
                pc,Cpu.r[8],Cpu.r[9],Cpu.r[10],Cpu.r[11],Cpu.r[12],Cpu.r[13],Cpu.r[14],SP);
    }
    if(TRACE_LOG2_ENABLED)
    {
      if(PSWT)
        TRACE("(T) %X: %04X %04X %s\n",pc,IR,IRC,disa(pc).Text); // IRD not valid yet
      else
        TRACE("%X: %04X %04X %s\n",pc,IR,IRC,disa(pc).Text);
    }
  }
#endif//#if defined(SSE_LOGSECTIONS_FOR_ALL) && !defined(SSE_DEBUGGER)

#if defined(SSE_HALT_CPUONLY)
  if(Cpu.ProcessingState==TMC68000::HALTED)
  {
    CPU_BUS_IDLE(4);
    return;
  }
#endif
  if(PSWT) // trace bit is set
  {
    Cpu.tpend=true; // hardware latch (=flag)

#if defined(SSE_ENABLE_TRACE_LOG)    
    if(PSWT && !TRACE_LOG_ENABLED(LOGSECTION_CPU))
    {
#if defined(SSE_DEBUGGER)
      TRACE_SECTION(LOGSECTION_TRACE,"(T) PC %X SR %04X VEC %X IRD %04X: %s\n",
                    pc,SR,LPEEK(0x24),IR,disa_d2(pc,IR).Text);
#else
      TRACE_SECTION(LOGSECTION_TRACE,"(T) PC %X SR %04X VEC %X IRD %04X: %s\n",
                    pc,SR,LPEEK(0x24),IR,disa(pc).Text);
#endif
    }
#endif

#if defined(SSE_DEBUGGER_FAKE_IO)
    if(OSD_MASK_CPU & OSD_CONTROL_CPUTRACE)
      TRACE_OSD("TRACE %X",pc);
#elif defined(SSE_OSD_DEBUGINFO)
    TRACE_OSD_DBG("%s %d","EXC",EXCEPTION_TRACE_EXCEPTION);
#endif
  }
  old_pc=pc;
  //ASSERT(old_pc!=0xe0376a);
  IRD=IR; // like for pc, timing isn't 100% correct
  // generally but not always useful, could use macros to compute only when needed
  // at the start of the instruction TODO maybe
  ry=IRD&7;               // (PARAM_M) (EA)
  rx=(IRD&BITS_ba9)>>9;   // (PARAM_N)
  pc+=2; // we do it here by convenience + the Debugger
  TRUE_PC=pc; // anyway
  //ASSERT(IRD!=0x000A);
  /////////// CALL CPU EMU FUNCTION ///////////////
  m68k_call_table[IRD](); // big opcode function table
  // check trace, before interrupt, as a debugger would want, cancelled by crash
  // IPL is scanned again by the end of the trace exception
  if(Cpu.tpend) 
  {
#if defined(SSE_STATS)
    Stats.nException[EXCEPTION_TRACE_EXCEPTION]++;
#endif
    M68K_UNSTOP; // STOP cancelled at once by trace
    Cpu.ProcessingState=TMC68000::EXCEPTION;
    CPU_BUS_IDLE(4); //  nn
    m68k_finish_exception(EXCEPTION_TRACE_EXCEPTION*4); //ns nS ns nV nv np n np 
    Cpu.tpend=false; // timing?
  }
  // check interrupt, looking back
  switch(check_ipl()) {
  case IPL_NONE: // no interrupt
    break;
  case IPL_MFP: // MFP IRQ
    Mfp.Iack(Mfp.NextIrq);
    break;
  case IPL_VBL: // Vertical sync end
    VBLInterrupt();
    if(check_ipl()==IPL_MFP) // see below
      Mfp.Iack(Mfp.NextIrq);
    break;
  case IPL_HBL: // Horizontal sync end
    HBLInterrupt();
    // do it twice, a higher interrupt can trigger before the first instruction 
    // of a lower interrupt's handler: Suretrip fullscreen
    switch(check_ipl()) {
    case IPL_NONE:
      break;
    case IPL_MFP:
      Mfp.Iack(Mfp.NextIrq);
      break;
    case IPL_VBL:
      VBLInterrupt();
      if(check_ipl()==IPL_MFP) // see above
        Mfp.Iack(Mfp.NextIrq);
      break;
    }
  }//sw

#ifdef DEBUG_BUILD
  if(ioaccess&(IOACCESS_DEBUG_MEM_WRITE_LOG|IOACCESS_DEBUG_MEM_READ_LOG))
  {
    DEBUG_CHECK_IOACCESS
  }
  // formerly CHECK_STOP_USER_MODE_NO_INTR:
  if(stop_on_user_change==2)
  {
    if(!PSWS)
    {
      if(runstate==RUNSTATE_RUNNING)
      {
        runstate=RUNSTATE_STOPPING;
        SET_WHY_STOP(HEXSl(old_pc,6)+": Switch to user mode")
      }
      if(stop_on_next_program_run==2)
        stop_new_program_exec();
    }
    else
      stop_on_user_change=1;
  }
  else if(stop_on_next_reset==2)
  {
    if(runstate==RUNSTATE_RUNNING)
    {
      runstate=RUNSTATE_STOPPING;
      SET_WHY_STOP(HEXSl(old_pc,6)+": Reset")
        stop_on_next_reset=1; /*sticky for the moment*/
    }
  }
  debug_first_instruction=false;
#endif//#ifdef DEBUG_BUILD

}


void m68kSetPC(MEM_ADDRESS const ad,int const count_timing) {
  pc=ad;             
  if(count_timing)
  {
    PREFETCH_ONLY;
    if(count_timing==2) // exception
    {
      CPU_BUS_IDLE(2); // two nonbus clock periods (dead cycles)
    }
    CHECK_IPL;
    PREFETCH_FINAL;
  }
  else
  {
    BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_FETCH|BUS_MASK_WORD;
    abus=pc&0xfffffe;
    IR=m68k_fetch(pc);
    MEM_ADDRESS au=pc+2;
    abus=au&0xfffffe;
    IRC=m68k_fetch(au);
  }
}


// only used for OS interception
void m68kPerformRte() {
  MEM_ADDRESS pushed_return_address=m68k_lpeek(SP+2);
  // An Illegal routine could manipulate this value.
  SET_PC(pushed_return_address);
  SR=m68k_dpeek(SP);
  SP+=6;
#ifndef SSE_LEAN_AND_MEAN
  SR&=SR_VALID_BITMASK;
#endif
  UPDATE_FLAGS;
  DETECT_CHANGE_TO_USER_MODE;         
}


#undef LOGSECTION


TMC68000::TMC68000() {
  Reset(true);
}


void TMC68000::Reset(bool Cold) {
  // notice that the registers are never reset
  tpend=false;
  if(Cold)
  {
#if defined(SSE_ENABLE_TRACE_LOG)
    nExceptions=0;
#endif
    cycles_for_eclock=cycles0=0;
    // "At power-on, it is impossible to guarantee phase relationship of E to CLK"
    eclock_sync_cycle=(rand()%9); // can be odd but we even out on syncs...
  }
  ZeroMemory(&ipl_timing_time,sizeof(COUNTER_VAR)*256);
  ZeroMemory(&ipl_timing_ipl,sizeof(BYTE)*256);
  a_s_t=A_S_T; // not zero on a reset without power-on
  tvn_latch_time=a_s_t;
  for(int i=0;i<256;i++)
    ipl_timing_time[i]=a_s_t;
  ProcessingState=EXCEPTION;
#if defined(SSE_OPTION_FASTLINEA)
  lineA=false;
  SetTimingFunctions();
#endif
  // TODO: set stack and PC here?
}


/////////////
// E-CLOCK //
/////////////

/*  
"Enable (E)
This signal is the standard enable signal common to all M6800 Family peripheral
devices. A single period of clock E consists of 10 Cpu clock periods (six clocks
low, four clocks high). This signal is generated by an internal ring counter that may
come up in any state. (At power-on, it is impossible to guarantee phase relationship of E
to CLK.) The E signal is a free-running clock that runs regardless of the state of the
MPU bus."

    In the ST, the E clock is used during autovector interrupts (horizontal and
    vertical sync) and during ACIA R/W.
    Wait states are generated for those operations, depending on the relationship
    between the E clock and the bus access timings.
    The delay can be 0-8 cycles, but never 9 apparently.
    Notice that for hbl, vbl interrupts, there's a RAM bus access right before we
    come here. 
    
    programs:
    NOJITTER.PRG by Nyh, Spectrum 512, Mental Hangover, 3615GEN4 HMD, Closure...

    doc: 
    M68000 User manual Appendix B M6800 peripheral interface
    Motorola AN-808: Interfacing M6800 peripheral devices to the MC68000 asynchronously
*/

int TMC68000::SyncEClock() {
  //ASSERT(eclock_sync_cycle<10);
  UpdateCyclesForEClock();
  int cycles=(cycles_for_eclock+eclock_sync_cycle)%10;
  cycles&=~BIT_0; // even only, at least in Steem SSE (TODO)
  int wait_states=8-cycles; //0 2 4 6 8
#if defined(SSE_DEBUGGER_FRAME_REPORT)
  if((TRACE_MASK2&TRACE_CONTROL_ECLOCK))
    FrameEvents.Add(scan_y,LINECYCLES/TICKS8,'E',wait_states);
#endif
  //ASSERT(wait_states==0||wait_states==2||wait_states==4||wait_states==6||wait_states==8);
  return wait_states;
}


/*  We come here at each VBL and each time the e-clock is read,
    so cycles_for_eclock should never overflow or go negative.
*/

void TMC68000::UpdateCyclesForEClock() {
  COUNTER_VAR cycles1=A_S_T; // current CPU cycles (can be negative)
  COUNTER_VAR ncycles=cycles1-cycles0; // elapsed CPU cycles since last refresh
//  ASSERT(!(ncycles%TICKS8));
  cycles_for_eclock+=ncycles/TICKS8; // update counter for E-clock
  cycles_for_eclock%=10*16; // remove high bits
  cycles0=cycles1; // record current CPU cycles
}

#undef CHECK_BLIT_REQUEST
#undef BLIT_CYCLES
#undef BLIT_BUS_ACCESS_CTR
#undef BUS_IDLE_CYCLES
#undef LOGSECTION
