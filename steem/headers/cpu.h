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
FILE: cpu.h
DESCRIPTION: Declarations for CPU emulation.
struct TMC68000, TMC68kException
---------------------------------------------------------------------------*/

#pragma once
#ifndef CPU_DECLA_H
#define CPU_DECLA_H

#include "binary.h"
#include "setjmp.h"
#include "conditions.h"
#include "SSE.h"
#include "acc.h"
#include "emulator.h"
#include <data_union.h>
#ifdef DEBUG_BUILD
#include "d2.h"
#include "debug_emu.h"
#endif


#pragma pack(push, 8)

struct TMC68000 {
  // ENUM
/* MC68000UM p99 (6-1)
"The processor is always in one of three processing states: normal, exception, or halted.
The normal processing state is associated with instruction execution; the memory
references are to fetch instructions and operands and to store results. A special case of
the normal state is the stopped state, resulting from execution of a STOP instruction. In
this state, no further memory references are made."
*/
  enum {NORMAL,STOPPED,EXCEPTION,HALTED,NREGS=16}; 
  // FUNCTIONS
  TMC68000();
  void Reset(bool Cold);
  int SyncEClock();
  void UpdateCyclesForEClock();
  void m68kUnstop();
  void StemdosCheckFlags();
  bool IsPriv(WORD op);
  // DATA
  COUNTER_VAR dbi_timing;
  COUNTER_VAR cycles_for_eclock; // because of integer overflow problems
  COUNTER_VAR cycles0; // to record A_S_T
  MEM_ADDRESS Pc; // Pc is the official register (see TRUE_PC)
  DU32 upc; // upc can be temp register (reference pc)
  int BusIdleCycles;
  signed int r[NREGS+1]; // data and address registers (A7 is doubled)
  WORD sr; // status register
  WORD ird; // current opcode
  WORD irc,ir; //2 words prefetch queue
  BYTE eclock_sync_cycle; //0-8
  BYTE ProcessingState;
  bool tpend; // actual internal latch set when CPU should trace current instruction
#if defined(SSE_ENABLE_TRACE_LOG)
  COUNTER_VAR nExceptions;
#endif
};

// process one 68000 instruction until completion or exception
// handling trace and interrupts as well
void m68kProcess();


///////////////
// Registers //
///////////////

#define areg (Cpu.r+8) //TODO: will eventually disappear? (still used by Debugger)
#define LITTLE_PC (pc&0xFFFFFE) // 23bit
#define TRUE_PC Cpu.Pc
extern BYTE d8; // for single byte R/W
extern DUS32 uresult;
extern LONG &resultl; // long result of arthmetic operations
extern short &resulth,&resultw; // high word (h), low word (w, not l!)
extern signed char &resultb; // byte result
extern DUS32 *cpureg;
extern DU32 *cpuareg;
extern DU16 uird;
extern WORD &IRC,&IR,&IRD; // there's no real reason, we use those in code
#define REGB(n) cpureg[n].d8[B0] // low order
#define REGW(n) cpureg[n].d16[LO] // low order
#define REGL(n) cpureg[n].d32
#define AREG(n) cpureg[8+n].d32
extern DU32 uiabus;
extern MEM_ADDRESS &iabus; // MC68000 internal address bus, 32 bit
extern WORD &iabush;
extern WORD &iabusl;
extern DU32 *ureg; // registers
extern DU32 *cpuareg; // address registers
extern MEM_ADDRESS &SP,&other_sp; // A7
extern MEM_ADDRESS &pc; // Steem variable which generqlly corresponds to PC or AU of the MC68000
extern WORD &pch,&pcl;
extern WORD &SR; // status register (Cpu.sr)
#ifdef SSE_DEBUGGER
extern WORD debug_old_sr;
#endif


///////////////////
// ST Exceptions //
///////////////////

#if defined(SSE_M68K_EXCEPTION_TRY_CATCH)
/*  Using C++ try/catch works but it's dramatically slower in the debug build.
    Check for example with the BIG Demo loader.
    There's certainly a performance hit in the release builds too, as try/catch
    does more than setjmp, but we don't need this work, because setjmp is only
    used to go through the calling stack, in other words it's appropriate. 

    No reason to define, but heed warning C4611.
*/
#define TRY_M68K_EXCEPTION try {
#define CATCH_M68K_EXCEPTION } catch (TMC68kException* exception_object) { 
#define END_M68K_EXCEPTION }
#else
// This is exactly the same as the C++ exceptions except you mustn't return or break
// out of the exception block without executing the END_M68K_EXCEPTION. If you have
// to you can just add pJmpBuf=oldpJmpBuf before the return/break and it will work.
// This could be fixed by making a wrapper class for jmp_buf so it will call the
// destructor when it goes out of scope, but GCC seems flakey on that sort of thing.
#define TRY_M68K_EXCEPTION jmp_buf temp_excep_jump;jmp_buf *oldpJmpBuf=pJmpBuf;pJmpBuf=&temp_excep_jump;if (setjmp(temp_excep_jump)==0){
#define CATCH_M68K_EXCEPTION }else{
#define END_M68K_EXCEPTION }pJmpBuf=oldpJmpBuf;
#endif

// The number of the exception is matched by the amount of bombs displayed on the screen
// when TOS catches the exception
enum E68000Exception {
  EXCEPTION_RESET=0,
  // there's no exception #1 because exception #0's vector is 8 bytes long (stack address, PC address)
  EXCEPTION_BUS_ERROR=2,
  EXCEPTION_ADDRESS_ERROR=3,
  EXCEPTION_ILLEGAL=4,
  EXCEPTION_DIVISION_BY_ZERO=5,
  EXCEPTION_CHK=6,
  EXCEPTION_TRAPV=7,
  EXCEPTION_PRIVILEGE_VIOLATION=8,
  EXCEPTION_TRACE_EXCEPTION=9, 
  EXCEPTION_LINE_A=10,
  EXCEPTION_LINE_F=11, // 11, not $F!
  EXCEPTION_SPURIOUS_EXCEPTION=24,
  EA_READ=0,EA_WRITE,EA_FETCH,EA_INST
};


struct TMC68kException {
  TMC68kException() {
    memset(this,0,sizeof(TMC68kException));
  }
  ~TMC68kException() {}
  void init(int en,int ea,MEM_ADDRESS ad);
  void crash();
  DU32 u_pc;
  DU32 ucrash_address;
  DU32 uaddress;
  int bombs;
  int action;
  WORD crash_sr,crash_ird;
};

// IPL Interrupt Priority Level
#define IPL_RESET 7
#define IPL_MFP 6
#define IPL_VBL 4
#define IPL_HBL 2
#define IPL_NONE 0

extern COUNTER_VAR tvn_latch_time;
extern COUNTER_VAR ipl_timing_time[256];
extern BYTE ipl_timing_ipl[256];
extern BYTE ipl_timing_index; // must be byte, overflows!

#define CHECK_IPL tvn_latch_time=A_S_T // macro record timing of last IPL scan


#define SR_IPL   (BIT_a+BIT_9+BIT_8)

void exception(int en,int ea,MEM_ADDRESS ad);
void m68k_trap1(); // illegal opcode, called so in the patent
void m68k_finish_exception(MEM_ADDRESS ad);
void m68k_interrupt(MEM_ADDRESS ad); // only called by utilities
extern TMC68kException ExceptionObject;
extern jmp_buf *pJmpBuf;


///////////////////////
// Effective Address //
///////////////////////

extern DU32 ueffective_address;
extern DWORD &effective_address;
extern WORD &effective_address_h;
extern WORD &effective_address_l;
extern DU32 um68k_old_dest;
extern DWORD &m68k_old_dest;
extern WORD &m68k_old_dest_h;
extern WORD &m68k_old_dest_l;
extern DUS32 sm68k_src_l;
extern LONG &m68k_src_l;
extern SHORT &m68k_src_lh;
extern SHORT &m68k_src_ll;
extern CHAR m68k_dst_b;
extern SHORT m68k_dst_w;
extern DUS32 sm68k_dst_l;
extern LONG &m68k_dst_l;
extern SHORT &m68k_dst_lh;
extern SHORT &m68k_dst_ll;
extern WORD m68k_ap,m68k_iriwo;
extern SHORT m68k_src_w;
extern CHAR m68k_src_b;
extern WORD ry,rx;
// pointer to function arrays
extern void (*m68k_jsr_get_source_b[8])();
extern void (*m68k_jsr_get_source_w[8])();
extern void (*m68k_jsr_get_source_l[8])();
extern void (*m68k_jsr_get_source_b_not_a[8])();
extern void (*m68k_jsr_get_source_w_not_a[8])();
extern void (*m68k_jsr_get_source_l_not_a[8])();
extern void (*m68k_jsr_get_dest_b[8])();
extern void (*m68k_jsr_get_dest_w[8])();
extern void (*m68k_jsr_get_dest_l[8])();
extern void (*m68k_jsr_get_dest_b_not_a[8])();
extern void (*m68k_jsr_get_dest_w_not_a[8])();
extern void (*m68k_jsr_get_dest_l_not_a[8])();
extern void (*m68k_jsr_get_dest_b_not_a_or_d[8])();
extern void (*m68k_jsr_get_dest_w_not_a_or_d[8])();
extern void (*m68k_jsr_get_dest_l_not_a_or_d[8])();
// functions
void m68k_get_source_000_b();
void m68k_get_source_000_w();
void m68k_get_source_000_l();
void m68k_get_source_001_b();
void m68k_get_source_001_w();
void m68k_get_source_001_l();
void m68k_get_source_010_b();
void m68k_get_source_010_w();
void m68k_get_source_010_l();
void m68k_get_source_011_b();
void m68k_get_source_011_w();
void m68k_get_source_011_l();
void m68k_get_source_100_b();
void m68k_get_source_100_w();
void m68k_get_source_100_l();
void m68k_get_source_101_b();
void m68k_get_source_101_w();
void m68k_get_source_101_l();
void m68k_get_source_110_b();
void m68k_get_source_110_w();
void m68k_get_source_110_l();
void m68k_get_source_111_b();
void m68k_get_source_111_w();
void m68k_get_source_111_l();
void m68k_get_dest_000_b();
void m68k_get_dest_000_w();
void m68k_get_dest_000_l();
void m68k_get_dest_001_b();
void m68k_get_dest_001_w();
void m68k_get_dest_001_l();
void m68k_get_dest_010_b();
void m68k_get_dest_010_w();
void m68k_get_dest_010_l();
void m68k_get_dest_011_b();
void m68k_get_dest_011_w();
void m68k_get_dest_011_l();
void m68k_get_dest_100_b();
void m68k_get_dest_100_w();
void m68k_get_dest_100_l();
void m68k_get_dest_101_b();
void m68k_get_dest_101_w();
void m68k_get_dest_101_l();
void m68k_get_dest_110_b();
void m68k_get_dest_110_w();
void m68k_get_dest_110_l();
void m68k_get_dest_111_b();
void m68k_get_dest_111_w();
void m68k_get_dest_111_l();




BYTE m68k_read_dest_b();
WORD m68k_read_dest_w();
LONG m68k_read_dest_l();

// could have tables for these, but they can be worse for performance
#define DEST_IS_REGISTER ((IRD&BITS_543)<=BITS_543_001)
#define DEST_IS_MEMORY ((IRD&BITS_543)>BITS_543_001)
#define SOURCE_IS_REGISTER_OR_IMMEDIATE ((IRD&BITS_543)<=BITS_543_001 || ((IRD&b00111111)==b00111100) )
#define DEST_IS_DATA_REGISTER ((IRD&BITS_543)==BITS_543_000)
#define DEST_IS_ADDRESS_REGISTER ((IRD&BITS_543)==BITS_543_001)


void m68kGetDestByte();
void m68kGetDestWord();
void m68kGetDestLong();
void m68kGetDestByteNotA();
void m68kGetDestWordNotA();
void m68kGetDestLongNotA();
void m68kGetDestByteNotAOrD();
void m68kGetDestWordNotAOrD();
void m68kGetDestLongNotAOrD();
void m68kGetSourceByte();
void m68kGetSourceWord();
void m68kGetSourceLong();
void m68kGetSourceByteNotA();
void m68kGetSourceWordNotA();
void m68kGetSourceLongNotA();
#define m68k_GET_DEST_B m68kGetDestByte();
#define m68k_GET_DEST_W m68kGetDestWord();
#define m68k_GET_DEST_L m68kGetDestLong();
#define m68k_GET_DEST_B_NOT_A m68kGetDestByteNotA();
#define m68k_GET_DEST_W_NOT_A m68kGetDestWordNotA();
#define m68k_GET_DEST_L_NOT_A m68kGetDestLongNotA();
#define m68k_GET_DEST_B_NOT_A_OR_D m68kGetDestByteNotAOrD();
#define m68k_GET_DEST_W_NOT_A_OR_D m68kGetDestWordNotAOrD();
#define m68k_GET_DEST_L_NOT_A_OR_D m68kGetDestLongNotAOrD();
#define m68k_GET_SOURCE_B m68kGetSourceByte();
#define m68k_GET_SOURCE_W m68kGetSourceWord();
#define m68k_GET_SOURCE_L m68kGetSourceLong();
#define m68k_GET_SOURCE_B_NOT_A m68kGetSourceByteNotA();
#define m68k_GET_SOURCE_W_NOT_A m68kGetSourceWordNotA();
#define m68k_GET_SOURCE_L_NOT_A m68kGetSourceLongNotA();


///////////
// Flags //
///////////

// User flags
#define SR_C BIT_0 // carry
#define SR_V BIT_1 // overflow
#define SR_Z BIT_2 // zero
#define SR_N BIT_3 // negative
#define SR_X BIT_4 // extend
// System flags
#define SR_S BIT_d // supervisor
#define SR_T BIT_f // trace


// we define flags as booleans (BYTE) for quicker use
// flags or SR are updated as needed, on R/W SR
// the union allows changing more than one flag in one go

extern BYTE &pswT,&pswS,&pswI,&pswX,&pswN,&pswZ,&pswV,&pswC;

void update_sr_from_flags();
#define UPDATE_SR update_sr_from_flags()
void update_flags_from_sr();
#define UPDATE_FLAGS update_flags_from_sr()

#define SUPERFLAG (pswS)
#define SSP ((MEM_ADDRESS)((SUPERFLAG) ? SP:other_sp))
#define USP ((MEM_ADDRESS)((SUPERFLAG) ? other_sp:SP))
#define lpSSP ((MEM_ADDRESS*)((SUPERFLAG) ? &SP:&other_sp))
#define lpUSP ((MEM_ADDRESS*)((SUPERFLAG) ? &other_sp:&SP))
#define IOACCESS_FLAGS_MASK  0xFFFFFFC0
#define IOACCESS_FLAG_FOR_CHECK_INTRS BIT_6
#define IOACCESS_FLAG_FOR_CHECK_INTRS_MFP_CHANGE BIT_10
#define IOACCESS_FLAG_DELAY_MFP BIT_11
#define IOACCESS_INTERCEPT_OS BIT_12

#define CCR WORD_B_0(&SR)
#ifndef SSE_LEAN_AND_MEAN
#define SR_VALID_BITMASK 0xa71f
#endif
extern DU64 uflags;
#define PSWC pswC
#define PSWV pswV
#define PSWZ pswZ
#define PSWN pswN
#define PSWX pswX
#define PSWI pswI
#define PSWS pswS
#define PSWT pswT

#define SET_C pswC=1
#define CLEAR_C pswC=0
#define SET_N pswN=1
#define CLEAR_N pswN=0
#define SET_V pswV=1
#define CLEAR_V pswV=0
#define SET_X pswX=1
#define CLEAR_X pswX=0
#define SET_Z pswZ=1
#define CLEAR_Z pswZ=0
#define SET_S pswS=1
#define CLEAR_S pswS=0
#define SET_T pswT=1
#define CLEAR_T pswT=0

#if defined(BIG_ENDIAN_PROCESSOR)
#define CLEAR_VC CLEAR_V,CLEAR_C
#define CLEAR_NVC_SET_Z CLEAR_N,CLEAR_V,CLEAR_C,SET_Z
//#define CLEAR_C_SET_NV CLEAR_C,SET_N,SET_V
#define CLEAR_XNC CLEAR_X,CLEAR_N,CLEAR_C
#define SET_XNC SET_X,SET_N,SET_C
#define SET_XC SET_X,SET_C
#define CLEAR_XC CLEAR_X,CLEAR_C
#define SET_NV SET_N,SET_V
//#define CLEAR_XNZVC CLEAR_X,CLEAR_N,CLEAR_Z,CLEAR_V,CLEAR_C
//#define CLEAR_NZVC CLEAR_N,CLEAR_Z,CLEAR_V,CLEAR_C
//#define CLEAR_NZV CLEAR_N,CLEAR_Z,CLEAR_V
#else
/*
tough to get right, but when it's done, it's an optimisation
XNZVC
BYTE &pswT=uflags.d8[7];  d16[3]L   d32[1]LL
BYTE &pswS=uflags.d8[6];  d16[3]H   d32[1]LH
BYTE &pswI=uflags.d8[5];  d16[2]L   d32[1]HL
BYTE &pswX=uflags.d8[4];  d16[2]H   d32[1]HH
BYTE &pswN=uflags.d8[3];  d16[1]L   d32[0]LL
BYTE &pswZ=uflags.d8[2];  d16[1]H   d32[0]LH
BYTE &pswV=uflags.d8[1];  d16[0]L   d32[0]HL
BYTE &pswC=uflags.d8[0];  d16[0]H   d32[0]HH
*/
#define CLEAR_VC uflags.d16[LO]=0
#define CLEAR_NVC_SET_Z uflags.d32[0]=0x00010000
//#define CLEAR_C_SET_NV  uflags.d32[0]&=0x01010100,uflags.d32[0]|=0x01000100
#define CLEAR_XNC       uflags.d64&=0x0101070000010100
#define SET_XNC         uflags.d64|=0x0000000101000001
#define SET_XC          uflags.d64|=0x0000000100000001
#define CLEAR_XC        uflags.d64&=0x0101070001010100
#define SET_NV          uflags.d32[0]|=0x01000100
//#define CLEAR_XNZVC     uflags.d64&=0x0101070000000000
//#define CLEAR_NZVC      uflags.d32[0]&=0
//#define CLEAR_NZV       uflags.d32[0]&=0x00000001
#endif

void detect_change_to_user_mode();
#define DETECT_CHANGE_TO_USER_MODE detect_change_to_user_mode();


///////////////
// Peek/Poke //
///////////////

// pointers to functions

extern BYTE (*m68k_peek)(MEM_ADDRESS ad);
extern WORD (*m68k_dpeek)(MEM_ADDRESS ad);
extern WORD (*m68k_fetch)(MEM_ADDRESS ad);
extern void (*m68k_poke_abus)(BYTE x);
extern void (*m68k_dpoke_abus)(WORD x);

// functions

BYTE m68k_peek_st(MEM_ADDRESS ad);
WORD m68k_dpeek_st(MEM_ADDRESS ad);
WORD m68k_fetch_st(MEM_ADDRESS ad);
// only used by utilities
DWORD m68k_lpeek(MEM_ADDRESS ad);

void m68k_poke_abus0(BYTE x);
void m68k_dpoke_abus0(WORD x);

#if defined(SSE_OPTION_FASTLINEA)
BYTE m68k_peek_st_lineA(MEM_ADDRESS ad);
WORD m68k_dpeek_st_lineA(MEM_ADDRESS ad);
WORD m68k_fetch_st_lineA(MEM_ADDRESS ad);
void m68k_poke_abus_lineA(BYTE x);
void m68k_dpoke_abus_lineA(WORD x);
#endif


void m68k_push_w(WORD x);
#define m68k_PUSH_W(x) m68k_push_w(x)
void m68k_push_l_without_timing(DWORD x);
#define m68k_PUSH_L(x) m68k_push_l_without_timing(x)

void change_to_supervisor_mode();


/////////////////////
// Counting cycles //
/////////////////////

/*
Note on rounding:
You need "round cycles up to 4" only for RAM and Shifter accesses.
This was already in the Engineering Hardware Specification of 1986,
but it took a long time to be implemented in emulators!

     ---------------
    | MC68000 MPU   |<--
    |               |   |
     ---------------    |
                        |                           ----------
                        |<------------------------>|192 Kbyte |<--->EXPAN
               ---------|------------------------->| ROM      |
              |         |                           ----------
              |         |                           ----------
              |         |                          |512K or 1M|  
              |         |                       -->| byte RAM |<--
      ----------        |        ----------    |    ----------    |
     | Control  |<----->|<----->| Memory   |<--                   |
     | Logic    |-------|------>|Controller|<--                   |
      ----------        |        ----------    |    ----------    |
       |||||            |        ----------     -->| Video    |<--  RF MOD
       |||||            |<----->| Buffers  |<----->| Shifter  |---->RGB
       |||||            |       |          |        ----------      MONO
       |||||            |        ----------
       |||||            |        ----------         ----------
       |||||            |<----->| MC6850   |<----->| Keyboard |<--->IKBD
       |||| ------------|------>| ACIA     |       | Port     |
       ||||             |        ----------         ----------
       ||||             |        ----------         ----------
       ||||             |<----->| MC6850   |<----->| MIDI     |---->OUT/THRU
       ||| -------------|------>| ACIA     |       | Ports    |<----IN
       |||              |        ----------         ----------
       |||              |        ----------         ----------
       |||              |<----->| MK68901  |<----->| RS232    |<--->MODEM
       || --------------|------>| MFP      |<--    | Port     |
       ||               |        ----------    |    ----------
       ||               |                      |    ----------
       ||               |                       ---| Parallel |<--->PRINTER
       ||               |                       -->| Port     |
       ||               |        ----------    |    ----------
       ||               |<----->| YM-2149  |<--     ----------
       | ---------------|------>| PSG      |------>| Sound    |---->AUDIO
       |                |       |          |---    | Channels |
       |                |        ----------    |    ----------
       |                |                      |    ----------
       |                |        ----------     -->| Floppy   |<--->FLOPPY
       |                |<------| WD1772   |<----->|Disk Port |     DRIVE
       |                |    -->| FDC      |        ----------
       |                |   |    ----------
       |                |   |    ----------         ----------
       |                |    -->| DMA      |<----->|Hard Disk |<--->HARD
       |                |<----->|Controller|       | Port     |     DRIVE
        ----------------------->|          |        ----------
                                 ----------
*/

extern DWORD CpuNormalHz,CpuCustomHz;
extern COUNTER_VAR a_s_t; // to be updated with ABSOLUTE_SYS_TIME and used as appropriate // was act
void SetTimingFunctions(); // to be called on config change

// pointers to functions
// those functins count cycles and some do an operation such as a R/W
extern void (*pBusIdle)(int t);   // n*t
extern void (*pBusWS)(int t);     // for waitstates (bus busy)
extern void (*pBusPrefetchOnly)();   // PREFETCH_ONLY (fetch IRC at pc)
extern void (*pBusPrefetchFinal)();  // PREFETCH_FINAL (IRC->IR + fetch IRC at au=pc+2)
extern void (*pBusPrefetchTotal)();  // PREFETCH (do pc+2, fetch IRC at pc)
extern void (*pBusReadB)();       // CPU read byte
extern void (*pBusRead)();        // CPU read word
extern void (*pBusWrite)();       // CPU write word
extern void (*pBusWriteB)();      // CPU write byte
extern void (*pBusBltRead)();     // Blitter read word
extern void (*pBusBltWrite)();    // Blitter write word

// macros actually used
#define CPU_BUS_IDLE(t)               pBusIdle((t)*TICKS8) // max is 8
#define PREFETCH                      pBusPrefetchTotal();
#define PREFETCH_FINAL                pBusPrefetchFinal();
#define PREFETCH_ONLY                 pBusPrefetchOnly();
#define CPU_BUS_ACCESS_READ           pBusRead()
#define CPU_BUS_ACCESS_READ_B         pBusReadB()
#define CPU_BUS_ACCESS_WRITE          pBusWrite()
#define CPU_BUS_ACCESS_WRITE_B        pBusWriteB()
#define BLT_BUS_ACCESS_READ           pBusBltRead()
#define BLT_BUS_ACCESS_WRITE          pBusBltWrite()
#define DMA_BUS_ACCESS_READ           pBusRead()
#define DMA_BUS_ACCESS_WRITE          pBusWrite()

#if defined(SSE_STATS_EXT) // debugger-only
#if defined(SSE_ENABLE_TRACE_LOG) // beta too
#define BUS_WAIT_STATES(t) if(stem_runmode==STEM_MODE_CPU){pBusWS((t)*TICKS8);a_s_t=A_S_T; \
Stats.ChipsAccessWs+=(t)*TICKS8;}
#else
#define BUS_WAIT_STATES(t) {pBusWS((t)*TICKS8);a_s_t=A_S_T;Stats.ChipsAccessWs+=(t)*TICKS8;}
#endif
#define RAM_ACCESS_WS { \
  int odd=sys_cycles&(4*TICKS8-1); /* 0-3 or 0-15 */ \
  if(odd) { \
    Stats.RamAccessWs+=odd; \
    sys_cycles&=-4*TICKS8; \
  } \
}
#else
#if defined(SSE_ENABLE_TRACE_LOG) // beta too
#define BUS_WAIT_STATES(t) if(stem_runmode==STEM_MODE_CPU){pBusWS((t)*TICKS8);a_s_t=A_S_T;}
#else
#define BUS_WAIT_STATES(t) {pBusWS((t)*TICKS8);a_s_t=A_S_T;}
#endif
#define RAM_ACCESS_WS {sys_cycles&=-4*TICKS8;}
#endif


// specialised timing functions (also see interface_stvl)

void BusWS(int t);

#if !defined(SSE_MEGA16)

void BusStfWS(int t);
void BusSteWS(int t);
//MEGASTE
void BusMegaSteWS(int t);
void BusMegaSteIdle(int t);
void BusMegaSteWS(int t);
void BusMegaStePrefetchOnly();
void BusMegaStePrefetchFinal();
void BusMegaStePrefetchTotal();
void BusMegaSteReadB();
void BusMegaSteRead();
void BusMegaSteWriteB();
void BusMegaSteWriteW();
#endif

void BusStfIdle(int t);
void BusStfPrefetchOnly();
void BusStfPrefetchFinal();
void BusStfPrefetchTotal();
void BusStfReadB();
void BusStfRead();
void BusStfWriteB();
void BusStfWriteW();

void BusSteIdle(int t);
void BusStePrefetchOnly();
void BusStePrefetchFinal();
void BusStePrefetchTotal();
void BusSteReadB();
void BusSteRead();
void BusSteWriteB();
void BusSteWriteW();
void BusSteBltRead();
void BusSteBltWrite();

void BusMegaIdle(int t);
void BusMegaPrefetchOnly();
void BusMegaPrefetchFinal();
void BusMegaPrefetchTotal();
void BusMegaReadB();
void BusMegaRead();
void BusMegaWriteB();
void BusMegaWriteW();


/////////////
// Opcodes //
/////////////

extern void (*m68k_call_table[0xffff+1])(); // 65536 function pointers
void cpu_routines_init(); // in cpuinit.cpp


/////////////
// Helpers //
/////////////

// count_timing 0: no (meta-emulation), 1: yes, 2: yes, for an exception
void m68kSetPC(MEM_ADDRESS ad,int count_timing=0);
#define SET_PC(ad) m68kSetPC(ad,0); // use out of running emu

#define m68k_GET_IMMEDIATE_L_WITH_TIMING m68k_get_immediate_l_with_timing()
#define WRITE_LONG_BACKWARDS m68k_write_long_result_backwards()
void m68k_push_l_with_timing(DU32 &x);
void m68k_get_immediate_l_with_timing();
#define m68k_GET_IMMEDIATE_L_WITH_TIMING m68k_get_immediate_l_with_timing()
void m68k_write_long_result_backwards();
#define WRITE_LONG_BACKWARDS m68k_write_long_result_backwards()
#define m68k_PUSH_L_WITH_TIMING(x) m68k_push_l_with_timing(x)


//see 3rdparty/pasti, we made no h
extern "C" unsigned getDivu68kCycles( DWORD dividend, WORD divisor);
extern "C" unsigned getDivs68kCycles( LONG dividend, SHORT divisor);

void m68kPerformRte(); // only used for OS interception


#define M68K_UNSTOP Cpu.m68kUnstop()

#if defined(SSE_LOGSECTIONS_FOR_ALL)
EasyStr disa(MEM_ADDRESS ad); //3rd party
#endif

///////////
// Debug //
///////////


#ifdef DEBUG_BUILD
#include "debug_emu.h"

/*
This is for the "stop on change to user mode" and "stop on next program run" options.
Whenever an instruction that can change SR is executed we check for SUPER change. If
a change to user mode has happened then stop_on_user_change is set to 2. Then when we
check for interrupts (after the current instruction) we see if an interrupt has happened
and if it hasn't we stop. If it has happened we return stop_on_user_change to 1 and wait
for the next change to user mode (when the interrupt has finished).
*/

#define CHECK_STOP_ON_USER_CHANGE check_stop_on_user_change()

#define PARAM_N ((IRD&BITS_ba9)>>9)
#define PARAM_M (IRD&0x7)

void DebugCheckIOAccess();
#define DEBUG_CHECK_IOACCESS DebugCheckIOAccess();


#else

#define CHECK_STOP_ON_USER_CHANGE
#define debug_check_break_on_irq(irq)
#define PARAM_N rx // must also keep PARAM_N above for debugger!
#define PARAM_M ry // must also keep PARAM_M above for debugger!

#define DEBUG_CHECK_IOACCESS

#endif//DEBUG_BUILD?

#pragma pack(pop)

#endif//#ifndef CPU_DECLA_H
