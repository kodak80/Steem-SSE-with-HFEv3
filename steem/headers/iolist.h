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

DOMAIN: Debug
FILE: iolist.h
DESCRIPTION: Declarations for the Debugger's IO list, including the
Control Mask Browser (fake IO used for Debugger control).
struct Tiolist_entry
---------------------------------------------------------------------------*/

#pragma once
#ifndef IOLIST_DECLA_H
#define IOLIST_DECLA_H

#ifdef DEBUG_BUILD

#include <easystr.h>
#include "conditions.h"
#ifdef MINGW_BUILD
#undef NULL
#define NULL 0
#endif


#define IOLIST_PSEUDO_AD 0x53000000
#define IOLIST_PSEUDO_AD_PSG (IOLIST_PSEUDO_AD+0x1000)
#define IOLIST_PSEUDO_AD_FDC (IOLIST_PSEUDO_AD+0x2000)
#define IOLIST_PSEUDO_AD_IKBD (IOLIST_PSEUDO_AD+0x3000)
#define IOLIST_PSEUDO_AD_6301 (IOLIST_PSEUDO_AD+0x4000)
#define IS_IOLIST_PSEUDO_ADDRESS(x) ((x&0xff000000)==IOLIST_PSEUDO_AD)


#if defined(SSE_DEBUGGER_FAKE_IO)

#define OSD_MASK1 (Debug.ControlMask[2])
#define OSD_CONTROL_INTERRUPT               (1<<15)
#define OSD_CONTROL_IKBD                  (1<<14)
#define OSD_CONTROL_FDC              (1<<13)

#define OSD_MASK_CPU (Debug.ControlMask[3])
#define OSD_CONTROL_CPUTRACE           (1<<15)
#define OSD_CONTROL_CPUBOMBS  (1<<14)

#define OSD_MASK2 (Debug.ControlMask[4])
#define OSD_CONTROL_SHIFTERTRICKS           (1<<15)
#define OSD_CONTROL_MODES (1<<14)

#if !defined(SSE_LOGSECTIONS_FOR_ALL)
#define TRACE_MASK0 (Debug.ControlMask[5]) // trace level
#define TRACE_LEVEL2 (1<<15)
#define TRACE_LEVEL3 (1<<14)
#endif

#define TRACE_MASK1 (Debug.ControlMask[6]) //Glue
#define TRACE_CONTROL_GLUREG (1<<15)
#define TRACE_CONTROL_VERTOVSC (1<<14)

#define TRACE_MASK2 (Debug.ControlMask[7])
#define TRACE_CONTROL_IRQ_TA (1<<15) //timer A
#define TRACE_CONTROL_IRQ_TB (1<<14) //timer B
#define TRACE_CONTROL_IRQ_TC (1<<13) //timer C
#define TRACE_CONTROL_IRQ_TD (1<<12) //timer D
#define TRACE_CONTROL_ECLOCK (1<<11)
#define TRACE_CONTROL_IRQ_SYNC (1<<10) //vbi, hbi
#define TRACE_CONTROL_RTE (1<<9)
#define TRACE_CONTROL_EVENT (1<<8)

#define TRACE_MASK3 (Debug.ControlMask[8])
#if defined(SSE_LOGSECTIONS_FOR_ALL)
#define TRACE_CONTROL_FDCSTR (1<<15)
#define TRACE_CONTROL_FDCPSG (1<<14)//drive/side
#define TRACE_CONTROL_FDCREGS (1<<13)// writes to registers CR,TR,SR,DR
//#define TRACE_CONTROL_FDCWD (1<<11) // for Steem's 2nd wd1772 emu, more details
#define TRACE_CONTROL_FDCMFM (1<<12)
#else
#define TRACE_CONTROL_FDCSTR (1<<15)
#define TRACE_CONTROL_FDCBYTES (1<<14)//no logsection needed
#define TRACE_CONTROL_FDCPSG (1<<13)//drive/side
#define TRACE_CONTROL_FDCREGS (1<<12)// writes to registers CR,TR,SR,DR
//#define TRACE_CONTROL_FDCWD (1<<11) // for Steem's 2nd wd1772 emu, more details
#define TRACE_CONTROL_FDCMFM (1<<11)
#endif

#define DEBUGGER_CONTROL_MASK1 (Debug.ControlMask[9])
#define DEBUGGER_CONTROL_LARGE_HISTORY (1<<15)
#define DEBUGGER_CONTROL_HISTORY_TMG (1<<14)

#define SOUND_CONTROL_MASK (Debug.ControlMask[10])
#define SOUND_CONTROL_OSD (1<<9)//first entries other variables

#define DEBUGGER_CONTROL_MASK2 (Debug.ControlMask[11])
#define DEBUGGER_CONTROL_NEXT_PRG_RUN (1<<15)
#define DEBUGGER_CONTROL_TOPOFF (1<<14)
#define DEBUGGER_CONTROL_BOTTOMOFF (1<<13)
#define DEBUGGER_CONTROL_6301 (1<<12) // custom prg run

/*#define TRACE_MASK_IO (Debug.ControlMask[12])
#define TRACE_CONTROL_IO_W (1<<15)
#define TRACE_CONTROL_IO_R (1<<14)*/

#define TRACE_MASK4 (Debug.ControlMask[13]) //cpu
#define TRACE_CONTROL_CPU_REGISTERS (1<<15) 
#define TRACE_CONTROL_CPU_CYCLES (1<<14) 
#define TRACE_CONTROL_CPU_LIMIT (1<<13)
#define TRACE_CONTROL_CPU_VALUES (1<<12)

//#define TRACE_MASK5 (Debug.ControlMask[14]) //trap
//#define TRACE_CONTROL_TRAP1 (1<<15) // GEMDOS
//#define TRACE_CONTROL_TRAP2 (1<<14)  // GEM
//#define TRACE_CONTROL_TRAP13 (1<<13) // BIOS
//#define TRACE_CONTROL_TRAP14 (1<<12) // XBIOS

#endif//#if defined(SSE_DEBUGGER_FAKE_IO)


void iolist_add_entry(MEM_ADDRESS ad,char*name,int bytes,char*bitmask=NULL,
  BYTE*ptr=NULL);

#pragma pack(push, 8)

struct Tiolist_entry {
  MEM_ADDRESS ad;
  EasyStr name;
  int bytes;
  EasyStr bitmask;
  BYTE*ptr;
};

#pragma pack(pop)

void iolist_init();
Tiolist_entry*search_iolist(MEM_ADDRESS);
int iolist_box_draw(HDC,int,int,int,int,Tiolist_entry*,BYTE*);
void iolist_box_click(int,Tiolist_entry*,BYTE*); //bit number clicked, toggle bit
int iolist_box_width(Tiolist_entry*);

#endif//#ifdef DEBUG_BUILD

#endif//IOLIST_DECLA_H
