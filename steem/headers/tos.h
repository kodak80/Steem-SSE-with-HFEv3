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

DOMAIN: OS
FILE: tos.h
DESCRIPTION: Declarations for TOS (The Operating System) utilities, and
for some cartridge utilities (could be cart.h and cart.cpp in the future)
struct TTos
---------------------------------------------------------------------------*/

#pragma once
#ifndef TOS_DECLA_H
#define TOS_DECLA_H

#include <dirsearch.h>
#include <easystr.h>
#include "conditions.h"
#include "SSE.h"

extern BYTE *STRom;
extern BYTE *cart,*cart_save;
#ifndef BIG_ENDIAN_PROCESSOR
extern BYTE *Rom_End,*Rom_End_minus_1,*Rom_End_minus_2,*Rom_End_minus_4;
extern BYTE *Cart_End_minus_1,*Cart_End_minus_2,*Cart_End_minus_4;
#endif
extern DWORD tos_len;
extern MEM_ADDRESS rom_addr,rom_addr_end;
extern MEM_ADDRESS os_gemdos_vector,os_bios_vector,os_xbios_vector;
extern WORD tos_version;
extern bool tos_high; // TOS is at $FC0000 (STF)
extern int aes_calls_since_reset; // Legacy: unused but saved in snapshot


#define ON_RTE_NONE 0
#define ON_RTE_STEMDOS 1
#define ON_RTE_LINEA_HACK 2
#define ON_RTE_EMHACK 3
#define ON_RTE_DONE_MALLOC_FOR_EM 4
#define ON_RTE_MOUSE 5
#define ON_RTE_STOP 400
#define ON_RTS_STOP 401

#define SV_PHYSTOP  0x42e
#define SV_DRVBITS  0x4c2

#define TRAP_GEMDOS  1
#define TRAP_GEM     2
#define TRAP_BIOS   13
#define TRAP_XBIOS  14


#pragma pack(push, 8)

#if defined(SSE_DEBUG_SYMBOLS)

#define MAX_SYMBOLS 1024 // the sky is the limit

struct TTosSymbol {
  //char name[8]; // DRI null-padded but no terminating 0
  char name[24]; // GST: 14 more chars possible (total 22 but we add 0 and make it even)
  WORD type;  //DEFINED=0x8000,EQUATED=0x4000,GLOBAL=0x2000,EQU_REG=0x1000,
              //EXTERNAL=0x0800,DAT_REL=0x0400,TEX_REL=0x0200,BSS_REL=0x0100
              //GST=0x48
  WORD value1,value2;
  MEM_ADDRESS ad; // fixed up address
};

extern TTosSymbol TosSymbol[MAX_SYMBOLS];
extern SHORT NextTosSymbol;
void HandlePrgCopy();

#endif//#if defined(SSE_DEBUG_SYMBOLS)

struct TTos {
  enum  {                      //Table 4-3.  BDOS Error Codes
    NoError=0,                       // E_OK
    //InvalidFunction=-32,           // EINVFN
    FileNotFound=-33,              // EFILNF
    PathNotFound=-34,              // EPTHNF
    //NoHandlesLeft=-35,             // ENHNDL
    AccessDenied=-36,              // EACCDN
    //InvalidHhandle=-37,            // EIHNDL
    InsufficientMemory=-39,        // ENSMEM
    //InvalidMemoryBlockAddress=-40, // EIMBA
    InvalidDrive=-46,              // EDRIVE
    NoMoreFiles=-49,               // ENMFIL
    RangeError=-64,                // ERANGE (can't use this, defined in WinError.h !)
    InternalError=-65,             // EINTRN
    InvalidProgramLoadFormat=-66,  // EPLFMT
    //SetblockFailure=-67            // EGSBF (due to growth restrictions)  
    RWABS=0x04 //Read/write sectors to a device
  };
  void CheckKeyboardClick();
  EasyStr GetNextTos(DirSearch &ds); // to enumerate TOS files
  void GetTosProperties(EasyStr Path,WORD &Ver,BYTE &Country,WORD &Date,BYTE &Recognised);
  void HackMemoryForExtendedMonitor();
#if !defined(SSE_GUI_STATUS_BAR_MOUSE2)
  WORD MouseX,MouseY; // for hack based on vq_mouse interception
  //MEM_ADDRESS MouseAd; // for hack based on stock address
#endif
  void UpdateTOSPath(EasyStr *pPath);
  LONG PRG_tsize,PRG_dsize,PRG_bsize,PRG_ssize;
#if defined(SSE_STATS)
  char PrgName[16];
#endif
#if defined(SSE_DEBUG_SYMBOLS)
  TTos();
  ~TTos();
  void Reset();
  bool GetSymbols(FILE *fpPrg,MEM_ADDRESS basepage);
  void TraceBasepage(MEM_ADDRESS basepage); // dump basepage info
  FILE *fpPrgCopy;
  MEM_ADDRESS Basepage;
  WORD LastFunc;
  WORD first_recno;
  BYTE TrackSymbols;
#endif
};

#pragma pack(pop)

MEM_ADDRESS GetSPBeforeTrap(bool* pInvalid=NULL);
void intercept_os();
bool load_TOS(char *File);  // true: succeeded
MEM_ADDRESS get_TOS_address(char *File);
void GetTOSKeyTableAddresses(MEM_ADDRESS *lpUnshiftTable,MEM_ADDRESS *lpShiftTable);


///////////////
// Cartridge //
///////////////

bool load_cart(char *filename); // true: succeeded

#if defined(SSE_CARTRIDGE_ACTIVE)
/*  buffer of bytes that can be used to store the value of rpin[], a parameter
*   used in CartridgeCheck()
*/
extern BYTE CartridgeData[CARTRIDGE_DATA_SIZE];

/*  pointer to function
*   ad=address bus value, 23 significant bits A1-A23
*   rpin=table of booleans representing pins of the chip rpin[0]=bit 0
*   for Cubase 2, rpin[8] to rpin[15] represent D8-D15
*   for Cubase 3, rpin[8] represents D8
*   return value=data bus value
*/
extern "C" WORD (*CartridgeCheck)(DWORD ad,BYTE *rpin);
#endif

/*  These functions contain the algorithms used by Cubase 2 and Cubase 3.
*   They may be built once then the code may be copied into a cartridge file,
*   or they may be included in the release build.
*   They may be called directly or through (*CartridgeCheck)().
*
*   The Cubase 2 cartridge reacts to each data strobe (precisely UDS)! This makes
*   emulating it quite heavy, a check on each R/W. Since Cubase 3 is available
*   too, and Cubase 2 was properly cracked, there's little incentive to support Cubase 2.
*   However it is useful in some dev build just to test Steem's CPU emulation.
*   SSE_DONGLE_CUBASE2 isn't defined in release builds.
* 
*   The Cubase 3 cartridge reacts only to the ROM3 signal, emulating it is not
*   too intrusive. In Steem SSE support is offered through a cartridge file that
*   contains the protection code. The cartridge file will be released only with
*   the permission of Steinberg, and I won't ask them so as not to attract
*   attention to the recent reverse engineering efforts by other people.
*   Anyway, for tight MIDI you need ST hardware.
*   SSE_DONGLE_CUBASE3 is defined in release builds.
* 
*   The C code isn't released either, but is just based on verilog code released
*   by a third party - check atari forums.
*/ 

#if defined(SSE_DONGLE_CUBASE2) \
  && (defined(SSE_DONGLE_CUBASE2_BUILD) || !defined(SSE_CARTRIDGE_ACTIVE2))
extern "C" WORD CartridgeCheckCubase2(DWORD ad,BYTE *rpin);
#endif
#if defined(SSE_DONGLE_CUBASE3) \
  && (defined(SSE_DONGLE_CUBASE3_BUILD) || !defined(SSE_CARTRIDGE_ACTIVE2))
extern "C" WORD CartridgeCheckCubase3(DWORD ad,BYTE *rpin);
#endif

#endif//#ifndef TOS_DECLA_H
