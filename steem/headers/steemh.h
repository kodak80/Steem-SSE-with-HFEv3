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

DOMAIN: All
FILE: steemh.h
DESCRIPTION: miscellanous declarations
class SE_Exception
---------------------------------------------------------------------------*/

#pragma once
#ifndef STEEMH_DECLA_H
#define STEEMH_DECLA_H

#include "conditions.h"


////////////////
// EXCEPTIONS //
////////////////

#if defined(SSE_MAIN_LOOP2)
#ifdef WIN32
#ifdef __cplusplus
#include <eh.h>
/*  The idea is to report a SEH exception in a normal try/catch block.
    https://msdn.microsoft.com/en-us/library/5z4bw5h5(VS.80).aspx
*/
class SE_Exception //as adapted
{
public:
//private:
    unsigned int nSE;
public:
    EXCEPTION_POINTERS* m_pExp; 
    //SE_Exception() {}
    //SE_Exception( unsigned int n ) : nSE( n ) {}
    SE_Exception(unsigned int u, EXCEPTION_POINTERS* pExp) {
      nSE=u;
      m_pExp=pExp;
    }
    ~SE_Exception() {}
    //unsigned int getSeNumber() { return nSE; }
    void handle_exception();
};

void __cdecl trans_func(unsigned int u,EXCEPTION_POINTERS* pExp);

#endif
#endif
#endif


/////////////
// VARIOUS //
/////////////

extern const char *stem_version_date_text;
#define TIMING_INFO FRAME,scan_y,LINECYCLES // for TRACE


///////////
// VIDEO //
///////////

#define FRAME (Shifter.nVbl) 
#define LINECYCLE0 TimeOfHSyncOff
#define LINECYCLES ((SHORT)(ABSOLUTE_SYS_TIME-LINECYCLE0))
#define FRAMECYCLES (ABSOLUTE_SYS_TIME-sys_time_of_last_vbl)

#if defined(SSE_VID_SIZE4)
#define SCANLINES_OK (SSEOptions.Scanlines && WinSizeForRes[screen_res])
#else
#define SCANLINES_OK (SSEOptions.Scanlines && screen_res<HIRES && WinSizeForRes[screen_res])
#endif

#define SCANLINES_INTERPOLATED (SCANLINES_OK && draw_win_mode[screen_res]==DWM_STRETCH && !FullScreen)

#endif//STEEMH_DECLA_H
