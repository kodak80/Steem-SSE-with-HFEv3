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
FILE: draw.h
DESCRIPTION: Declarations for draw routines.
---------------------------------------------------------------------------*/

#pragma once
#ifndef DRAW_DECLA_H
#define DRAW_DECLA_H

#include "conditions.h"
#include "SSE.h"
#include "parameters.h"

enum EDraw {DWM_STRETCH=0,DWM_NOSTRETCH,DWM_GRILLE};

#if !defined(SSE_420R4)
bool draw_routines_init();
#endif
void init_screen();
void draw_begin();
void draw_end();
bool draw_blit(
#if defined(SSE_VID_BFI) // D3D-only
  BOOL erase=FALSE
#endif
  );
void draw(bool draw_osd);
HRESULT change_fullscreen_display_mode(bool resizeclippingwindow);
void res_change();
void update_CanUse_400(int cw,int ch);
void update_winsize();
#if !defined(SSE_420R4)
void ASMCALL draw_scanline_dont(int,int,int,int);
#endif
extern "C" 
{
  LONG* ASMCALL Get_PCpal(); //ASMCALL is __cdecl
  // int border1,int picture,int border2,int hscroll
  void ASMCALL draw_scanline_32_lowres_pixelwise(int,int,int,int);
  void ASMCALL draw_scanline_32_lowres_pixelwise_dw(int,int,int,int);
  void ASMCALL draw_scanline_32_lowres_pixelwise_400(int,int,int,int);
  void ASMCALL draw_scanline_32_medres_pixelwise(int,int,int,int);
  void ASMCALL draw_scanline_32_medres_pixelwise_400(int,int,int,int);
  void ASMCALL draw_scanline_32_hires(int,int,int,int);
#if defined(SSE_VID_SIZE4)
  // 3 treble size lines
  void ASMCALL draw_scanline_32_lowres_3x(int,int,int,int);
  // 4x wide, 2x height // 3x height
  void ASMCALL draw_scanline_32_medres_3x(int,int,int,int);
  // 4 large lines
  void ASMCALL draw_scanline_32_lowres_4x(int,int,int,int);
  void ASMCALL draw_scanline_32_medres_4x(int,int,int,int);
  // 2 large lines
  void ASMCALL draw_scanline_32_hires_4x(int,int,int,int);
  void ASMCALL draw_scanline_32_hires_4x_scan(int,int,int,int); // 2nd line=0
  // 1 treble size line
  void ASMCALL draw_scanline_32_lowres_3w(int,int,int,int);
  // 1 large line
  void ASMCALL draw_scanline_32_lowres_4w(int,int,int,int);
  void ASMCALL draw_scanline_32_medres_2w(int,int,int,int);
  void ASMCALL draw_scanline_32_hires_2w(int,int,int,int);
#endif

#if defined(SSE_VID_SINGLEPIX)
// Single pixels
  void ASMCALL draw_scanline_32_lowres_pixelwise_dw_sp(int,int,int,int);
  void ASMCALL draw_scanline_32_lowres_pixelwise_400_sp(int,int,int,int);
#if defined(SSE_VID_SIZE4)
  // 3 treble size lines
  void ASMCALL draw_scanline_32_lowres_3x_sp(int,int,int,int);
  // 4x wide, 2x height // 3x height
  void ASMCALL draw_scanline_32_medres_3x_sp(int,int,int,int);
  // 4 large lines
  void ASMCALL draw_scanline_32_lowres_4x_sp(int,int,int,int);
  void ASMCALL draw_scanline_32_medres_4x_sp(int,int,int,int);
  // 2 large lines
  void ASMCALL draw_scanline_32_hires_4x_sp(int,int,int,int);
  void ASMCALL draw_scanline_32_hires_4x_scan_sp(int,int,int,int); // 2nd line=0
  // 1 treble size line
  void ASMCALL draw_scanline_32_lowres_3w_sp(int,int,int,int);
  // 1 large line
  void ASMCALL draw_scanline_32_lowres_4w_sp(int,int,int,int);
  void ASMCALL draw_scanline_32_medres_2w_sp(int,int,int,int);
  void ASMCALL draw_scanline_32_hires_2w_sp(int,int,int,int);
#endif
#endif

}

typedef void ASMCALL PIXELWISESCANPROC(int,int,int,int);
typedef PIXELWISESCANPROC* LPPIXELWISESCANPROC;
#if !defined(SSE_420R4)
extern LPPIXELWISESCANPROC jump_draw_scanline[5][3]; // p res
#endif
extern LPPIXELWISESCANPROC draw_scanline_lowres,draw_scanline_medres,draw_scanline_hires;
extern LPPIXELWISESCANPROC draw_scanline;

#if defined(SSE_VID_STVL1)
#if defined(SSE_DRAW_C)
extern int &draw_line_pitch_dw;
#else
extern int draw_line_pitch_dw; // pitch in DWORD (#BYTES/4)
#endif
#endif

extern "C" // may be used in assembly routines
{
  extern int draw_line_length;
  extern LONG *PCpal;
  // draw_type is BYTE for assembly routines, DWORD for C routines
  extern draw_type *draw_mem,*draw_dest_ad,*draw_dest_next_scanline;
}
extern RECT draw_blit_source_rect;
extern int draw_dest_increase_y; // pitch plus doubling, trebling...
extern int draw_win_mode[3];
extern short res_vertical_scale;
extern short draw_first_scanline_for_border,draw_last_scanline_for_border; 
extern short draw_first_scanline_for_border60,draw_last_scanline_for_border60; 
extern short draw_first_possible_line,draw_last_possible_line;
extern bool draw_lock;
extern BYTE bad_drawing;
extern BYTE draw_grille_black; // # frames that will be zeroed before rendering, see draw_begin()
extern BYTE border; //0: no border 1: normal 2: large 3: max
extern BYTE border_last_chosen;
extern int scanline_drawn_so_far;
extern short left_border,right_border;
extern bool draw_line_off; // black lines


void ChangeBorderSize(int size);
extern BYTE SideBorderSize,SideBorderSizeWin,TopBorderSize,BottomBorderSize;
extern BYTE LeftBorderSize,RightBorderSize;

extern POINT WinSizeBorder[4][5];



// This is for the new scanline buffering (v2.6). If you write a lot direct
// to video memory it can be very slow due to recasching, so if the surface is
// in vid mem we set draw_buffer_complex_scanlines. This means that in
// draw_scanline_to we change draw_dest_ad to draw_temp_line_buf and
// set draw_scanline to draw_scanline_1_line. In draw_scanline_to_end
// we then copy from draw_temp_line_buf to the old draw_dest_ad and
// restore draw_scanline.

#if defined(SSE_420R4)
extern draw_type *draw_temp_line_buf; 
#else
extern draw_type draw_temp_line_buf[DRAW_TEMP_LINE_BUF_LEN]; 
#endif
extern draw_type *draw_temp_line_buf_lim;
extern draw_type *draw_store_dest_ad;

#ifdef WIN32
#if defined(SSE_VID_DD)
void get_fullscreen_rect(RECT *);
extern BYTE draw_fs_blit_mode;
#endif
#if !defined(SSE_VID_DD_MISC)
extern HWND ClipWin; 
#endif
#endif//WIN32

extern bool draw_buffer_complex_scanlines;
extern BYTE draw_stretch;

#ifdef UNIX
extern int x_draw_surround_count;
#endif

#if defined(SSE_VID_D3D)
extern const BYTE draw_fs_fx;
#else // Linux too
extern BYTE draw_fs_fx;
extern bool prefer_res_640_400,using_res_640_400;
#endif

extern BYTE draw_stretch_fs;

struct TDraw {
  void MarshalParameters(); // update
#if defined(SSE_420R4)
  ~TDraw() {
    if(draw_temp_line_buf)
      free(draw_temp_line_buf);
  }
#endif
  draw_type *limit1,*limit2; // should only write between those limits
  RECT BltSrc; // draw_blit_source_rect
  RECT BltDst;
  LONG cw,ch; // client area
  LONG SrcWidth,SrcHeight; // in PC pixels
  int Pitch,EffectivePitch;
  BYTE LineCpyMask; // mask of scanlines to copy (bit=row 0-3)
  BYTE HorPix,VerPix; // PC pixels per ST pixel, PC lines per ST line
  BYTE Size;
  BYTE Buffered; // each scanline first rendered in a buffer, then copied
  BYTE Stretch; // window or fullscreen
  BYTE VSync;
  BYTE Scanlines;
  BYTE res,Idx;
};
extern TDraw Draw;


#endif//DRAW_DECLA_H
