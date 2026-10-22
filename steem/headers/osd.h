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

DOMAIN: Rendering
FILE: osd.h
DESCRIPTION: Declarations for OSD (On Screen Display).
struct TOsdControl
---------------------------------------------------------------------------*/

#pragma once
#ifndef OSD_DECLA_H
#define OSD_DECLA_H

#include <easystringlist.h>
#include <conditions.h>
#include "SSE.h"

#ifndef SSE_NO_OSD

#define OSD_LOGO_W 124
#define OSD_LOGO_H 11
#define OSD_SHOW_ALWAYS 0xff

extern DWORD *osd_plasma_pal;
extern BYTE *osd_plasma;
extern LONG *osd_font;
extern EasyStr osd_scroller;
extern EasyStringList osd_scroller_array;
extern DWORD osd_scroller_finish_time,FDCCantWriteDisplayTimer;
extern LONG col_blue,col_white;
void osd_draw_begin();
void osd_init_run(bool allow_scroller);
void osd_draw();
void osd_hide();
void osd_draw_end();
void osd_routines_init();
void osd_init_draw_static();
EasyStr get_osd_scroller_text(int n);

#ifdef WIN32
#if !defined(SSE_OSD_NORESETINFO)
void osd_draw_reset_info(HDC dc);
LRESULT CALLBACK ResetInfoWndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar);
extern HWND hResetInfoWin;
#endif
#endif

#ifdef UNIX
void osd_draw_reset_info(int,int,int,int);
#endif

void ASMCALL osd_draw_char_dont(LONG*,draw_type*,LONG,LONG,int,LONG,LONG);
typedef void ASMCALL OSDDRAWCHARPROC(LONG*,draw_type*,LONG,LONG,int,LONG,LONG);

typedef OSDDRAWCHARPROC* LPOSDDRAWCHARPROC;
extern LPOSDDRAWCHARPROC jump_osd_draw_char;


extern "C" {
void ASMCALL osd_draw_char_clipped_32(LONG* src,draw_type* dst,LONG x,LONG y,
                                      int l,LONG c,LONG s,RECT* cr);

void ASMCALL osd_draw_char_32(LONG*source_ad,draw_type*dst,LONG x,LONG y,
                              int l,LONG colour,LONG h);

void ASMCALL osd_draw_char_transparent_32(LONG *source_ad,draw_type *draw_mem,
                       LONG x,LONG y,int draw_line_length,LONG colour,LONG h);

void ASMCALL osd_draw_char_clipped_transparent_32(LONG* src,draw_type* dst,
                                  LONG x,LONG y,int l,LONG c,LONG s,RECT* cr);

void ASMCALL osd_blueize_line_32(int x,int y,int w);
void palette_convert_line_32(int);
void ASMCALL osd_black_box_32(void* dst,int x,int y,int w,int h,LONG l);
} // extern "C"



#pragma pack(push, 8)

struct TOsdControl {
  enum EOsdControl {NO_SCROLLER,WANT_SCROLLER,SCROLLING,OUTPUT_OSD=BIT_0,OUTPUT_SB=BIT_1};
  TOsdControl();
  void HdLed(int time=HD_TIMER);
  void Trace(char *fmt,...); // yellow message in top left
  BYTE Trace(int output,char *fmt,...); //bit0 OSD bit1 StatusBar
#if defined(SSE_OSD_SHOW_TIME)
  DWORD StartingTime; // record time on cold reset
  DWORD StoppingTime; // to adjust when stopping/restarting
#endif
  DWORD MessageTimer;
  LONG ScrollerPosition;
  BYTE ScrollerPhase;
  char m_OsdMessage[OSD_MESSAGE_LENGTH+1]; // +null as usual
  BYTE show_plasma,show_speed,show_icons,show_cpu;
  bool show_disk_light,show_scrollers;
  bool show_jokes;
  bool no_draw,disable,bOsdDrawn;
  bool bPrinting;
  BYTE ScrollerFrequency;
  DWORD SecondsBetweenScrollers;
};

extern TOsdControl OsdControl;

#pragma pack(pop)

#endif//#ifdef SSE_NO_OSD

#ifdef SSE_OSD_DRIVELED
#define OSD_HD_LED OsdControl.HdLed()
#else
#define OSD_HD_LED
#endif

#endif//OSD_DECLA_H

