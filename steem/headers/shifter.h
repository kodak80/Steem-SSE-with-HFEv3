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
FILE: shifter.h
DESCRIPTION: Declarations for the high-level Shifter emulation.
struct TShifter
---------------------------------------------------------------------------*/

#pragma once
#ifndef SSESHIFTER_H
#define SSESHIFTER_H


#include "conditions.h"
#include "SSE.h"
#include <data_union.h>

#define SHIFTER_RASTER_PREFETCH_TIMING (16*TICKS8) // in all shift modes, all 4 Shifter registers are loaded before picture starts
#define SHIFT_SDP(nbytes) shifter_draw_pointer+=(MEM_ADDRESS)(nbytes)
#define HSCROLL shifter_hscroll
#define OPTION_SHIFTER_WU (Shifter.WakeupShift)


extern "C" // can be used by assembly routines => no struct, don't change names
{
  extern WORD STpal[PAL_SIZE]; // the full ST palette is here
  extern MEM_ADDRESS shifter_draw_pointer; // ST pointer to video RAM to be rendered
  extern BYTE shifter_hscroll; // STE-only horizontal scroll
}

extern SHORT shifter_tick8; // was shifter_pixel, but it's 2 pixels in MEDRES, 4 in HIRES
extern SHORT shifter_x,shifter_y;
extern BYTE shifter_sound_mode; // should be in struct but old memory snapshot issues

#pragma pack(push, 8)

struct TShifter {
  //ENUM
  enum { SOUNDMONO=BIT_7,
    DIGITAL_SOUND_BUFFER_LEN=4, // 4 words
    // ID which part of emulation required video rendering
    DISPATCHER_CPU,DISPATCHER_SET_SHIFT_MODE,
    DISPATCHER_WRITE_VC,DISPATCHER_SET_PAL,DISPATCHER_DSTE,
    DISPATCHER_LINEWIDTH,DISPATCHER_HSCROLL,
    DISPATCHER_SET_SYNC,DISPATCHER_DEBUGGER
  };
  //FUNCTIONS
  TShifter();
  void DrawScanlineToEnd();
  void IncScanline();
  void Render(SHORT cycles_in,BYTE dispatcher); // render video scanline part
  void Reset(bool Cold);
  void Restore();
  void RoundCycles(SHORT &cycles_in);
  static void CALLBACK SetPal(int n, WORD NewPal); // static for STVL
  void SoundGetLastSample(WORD *pw1,WORD *pw2); // v402
  void SoundSetMode(BYTE new_mode);
  void SoundPlay(); // render digital audio
  //DATA
  BYTE *ScanlineBuffer; // unused
  DWORD Scanline[230/4+2];
  int nVbl; // = FRAME for debugger
  BYTE Scanline2[112];
  BYTE HiresRaster; // for a hack
  BYTE HblStartingHscroll; // saving true hscroll in MED RES (no use)
  BYTE ShiftMode;
  BYTE Preload; // #words into Shifter's IR (gross approximation)
  char WakeupShift;
  char HblPixelShift; // for 4bit scrolling, other shifts
  WORD SoundFifo[DIGITAL_SOUND_BUFFER_LEN]; // ref. 4.0.2
  BYTE SoundFifoIdx;
#if defined(SSE_SHIFTER_421) // refactoring that will help with the interface
  //DWORD: 1; // padding
  char RenderingDelay[16]; // part of object Shifter means saved with snapshot
#endif
};


#pragma pack(pop)


////////////////////////////////////////////
// Digital Sound (GST Shifter of the STE) //
////////////////////////////////////////////

extern DU16 SteSoundLastWord;
extern WORD SteSoundFreq;
// Max frequency/lowest refresh *2 for stereo
extern WORD *SteSoundChannelBuf;
extern DWORD SteSoundChannelBufLen, SteSoundChannelBufIdx;
extern int SteSoundOutputCountdown,SteSoundSamplesCountdown; // int: can be negative
extern const WORD SteSoundModeToFreq[4];

#endif//define SSESHIFTER_H
