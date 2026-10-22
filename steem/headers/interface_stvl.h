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
FILE: interface_stvl.h
DESCRIPTION: Declarations for Steem's STVL interface.
STVL is a plugin for ST emulators able to emulate the video logic of an STF
or an STE at low level.
---------------------------------------------------------------------------*/

#pragma once
#ifndef INTERFACE_STVL_H
#define INTERFACE_STVL_H

#include "SSE.h"

#if defined(SSE_VID_STVL)

#include <stvl/stvl.h>
#include "conditions.h"


extern HMODULE hStvl; //dll
extern TStvl Stvl;
extern WORD render_vstart, render_vend;
extern WORD render_hstart;
extern WORD render_scanline_length;
extern DWORD *draw_mem_line_ptr;

// pointers 
extern DWORD (STVL_CALLCONV *video_logic_init)(TStvl *pStvl);
extern void (STVL_CALLCONV *video_logic_reset)(TStvl *pStvl,bool Cold);
extern void (STVL_CALLCONV *video_logic_stf_run)(TStvl *pStvl,int cycles);
extern void (STVL_CALLCONV *video_logic_ste_run)(TStvl *pStvl,int cycles);
extern void (STVL_CALLCONV *video_logic_update)(TStvl *pStvl);
//extern void (STVL_CALLCONV*video_logic_update_pal)(TStvl *pStvl,int n,WORD pal);
void StvlUpdate();
void StvlInit();
void CALLBACK cb_mfp_de_transition();

extern COUNTER_VAR acc_cycles;
// 1 low-level video, no acceleration
// 2 low-level video, acceleration
// It's quite a long list and each one needs a definition!
void BusStf1Idle(int t);
#if defined(SSE_MEGA16)
extern void (STVL_CALLCONV *video_logic_run)(TStvl *pStvl,int cycles); // set to stf or ste logic
void Bus1WS(int t);
void Bus2WS(int t);
void BusMega1Idle(int t);
void BusMega1PrefetchOnly();
void BusMega1PrefetchFinal();
void BusMega1PrefetchTotal();
void BusMega1ReadB();
void BusMega1Read();
void BusMega1Write();
void BusMega1WriteB();
void BusMega1BltRead();
void BusMega1BltWrite();
#else
void BusStf1WS(int t);
void BusSte1WS(int t);
void BusMegaSt1WS(int t);
void BusMegaSte1WS(int t);
void BusStf2WS(int t);
void BusSte2WS(int t);
void BusMegaSt1Idle(int t);
void BusMegaSte1Idle(int t); 
void BusMegaSt1PrefetchOnly();
void BusMegaSt1PrefetchFinal();
void BusMegaSt1PrefetchTotal();
void BusMegaSte1PrefetchOnly();
void BusMegaSte1PrefetchFinal();
void BusMegaSte1PrefetchTotal();
void BusMegaSt1ReadB();
void BusMegaSt1Read();
void BusMegaSt1Write();
void BusMegaSt1WriteB();
void BusMegaSte1ReadB();
void BusMegaSte1Read();
void BusMegaSte1Write();
void BusMegaSte1WriteB();
void BusMegaSt2Idle(int t);
void BusMegaSt2PrefetchOnly();
void BusMegaSt2PrefetchFinal();
void BusMegaSt2PrefetchTotal();
void BusMegaSt2WS(int t);
void BusMegaSt2ReadB();
void BusMegaSt2Read();
void BusMegaSt2Write();
void BusMegaSt2WriteB();
void BusMegaSt2BltRead();
void BusMegaSt2BltWrite();
#endif
void BusStf1PrefetchOnly();
void BusStf1PrefetchFinal();
void BusStf1PrefetchTotal();
void BusStf1ReadB();
void BusStf1Read();
void BusStf1Write();
void BusStf1WriteB();
void BusStf2Idle(int t);
void BusStf2PrefetchOnly();
void BusStf2PrefetchFinal();
void BusStf2PrefetchTotal();
void BusStf2ReadB();
void BusStf2Read();
void BusStf2Write();
void BusStf2WriteB();
void BusMegaSt1BltRead();
void BusMegaSt1BltWrite();
void BusSte1Idle(int t);
void BusSte1PrefetchOnly();
void BusSte1PrefetchFinal();
void BusSte1PrefetchTotal();
void BusSte1ReadB();
void BusSte1Read();
void BusSte1Write();
void BusSte1WriteB();
void BusSte1BltRead();
void BusSte1BltWrite();
// we don't do accelerated for the Mega, we use STE functions
void BusSte2Idle(int t);
void BusSte2PrefetchOnly();
void BusSte2PrefetchFinal();
void BusSte2PrefetchTotal();
void BusSte2ReadB();
void BusSte2Read();
void BusSte2Write();
void BusSte2WriteB();
void BusSte2BltRead();
void BusSte2BltWrite();

#endif//#if defined(SSE_VID_STVL)

#endif//#ifndef INTERFACE_STVL_H

