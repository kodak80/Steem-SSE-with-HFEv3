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

FILE: draw_c.cpp
DESCRIPTION: Alternative C++ drawing routines for systems that don't support
Steem's x86 assembler versions.
Used in the x64 build.
v4.1.0: used in the Windows x86 build as well, performance is comparable with
assembly (maybe better), code is smaller.
Unlike with assembly modules, the C compiler is able to optimize those
routines.
Where we use intrinsics, the compiler is supposed to optimize too.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <draw.h>
#include <emulator.h>
#include <osd.h>
#include <shifter.h>
#include <mmu.h>

#if defined(SSE_VC_INTRINSICS)
#include <intrin.h>
#endif

LONG PCpal_array[16];

LONG* ASMCALL Get_PCpal() {
  return PCpal_array;
}

#ifndef SSE_NO_OSD

#define OSD_PIXEL(mask)  \
  if(dw1&(mask)){    \
    OSD_DRAWPIXEL(colour);  \
  }else if(dw0&mask){    \
    OSD_DRAWPIXEL(0);     \
  }else{              \
    dadd++; \
  }


#define bpp 4

#define OSD_DRAWPIXEL(c) *dadd++=c;


extern "C" void ASMCALL osd_draw_char_32(LONG*source_ad,draw_type*dst,LONG x,
                                         LONG y,int l,LONG colour,LONG h) {
#include "draw_c_osd_draw_char.cpp"
}

extern "C" void ASMCALL osd_blueize_line_32(int x,int y,int w){
#include "draw_c_osd_blueize_line.cpp"
}

#undef bpp


extern "C" void ASMCALL osd_draw_char_clipped_32(LONG* src,DWORD* dst,LONG x,
                                     LONG y,int l,LONG c,LONG s,RECT* cr) {

// this is osd_draw_char_32 adapted
#define bpp 4
#define h s
#define source_ad src
#define colour c

  DWORD x_left=BIT_31;
  DWORD x_right=0;
  LONG d=cr->left-x;
  if(d>31)
    return;
  else if(d>0)
  {
    x_left>>=d;
    x+=d;
  }
  d=x+s+1-cr->right;
  if(d>=0)
    x_right=1<<d;
  DWORD* dadd=dst+x+y*l,*daddl=dadd;
  DWORD dw0,dw1;
  for(int yy=h;yy>0;yy--)
  {
    dw0=*source_ad;
    dw1=*(source_ad+1);
    source_ad+=2;
    for(DWORD bit=x_left;bit>x_right;bit>>=1)
    {
      OSD_PIXEL(bit);
    }
    daddl+=l;
    dadd=daddl;
  }

#undef source_ad
#undef h
#undef colour
}


void ASMCALL osd_draw_char_transparent_32(LONG*source_ad,draw_type*dst,LONG x,
                                          LONG y,int l,LONG colour,LONG h) {
  osd_draw_char_32(source_ad,dst,x,y,l,colour,h);
}


extern "C" void ASMCALL osd_draw_char_clipped_transparent_32(LONG* src,
                 DWORD* dst,LONG x,LONG y,int l,LONG c,LONG s,RECT* cr) {
  if(x>=cr->left && x<=(cr->right-s)) 
    osd_draw_char_32(src,dst,x,y,l,c,s);
}


void ASMCALL osd_black_box_32(void* dst,int x,int y,
                              int w,int h,LONG l) {
  for(int h1=0;h1<h;h1++)
  {
    DWORD*dadd=(DWORD*)dst+(y+h1)*l+x;
    for(int n=w;n>0;n-=2)
    { // black frame, white filling, so it's visible with more palette choices
      LONG c=(n==w||n==2||!h1||h1==h-1) ? 0 : col_white;
      OSD_DRAWPIXEL(c);
      OSD_DRAWPIXEL(c);
    }
  }
}

#endif//#ifndef SSE_NO_OSD


// increase = 80 or 160, doubleflag =0, not used
#define GET_START(doubleflag,increase)  \
  source=shifter_draw_pointer&0xfffffe; \
  if(source+increase>mem_len) source&=0xfffe;

#if 0
#define DRAW_BORDER(n)    \
  for(n*=8;n>0;n--){                       \
    DRAW_2_BORDER_PIXELS               \
  }
#endif


// generic
#define DRAW_BORDER_PIXELS_A(npixels) \
  for(int i=npixels;i;i--) \
  { \
    DRAWPIXEL(PCpal);\
  }

#define DRAW_BORDER_PIXELS DRAW_BORDER_PIXELS_A

#ifndef BIG_ENDIAN_PROCESSOR
#define GET_SCREEN_DATA_INTO_REGS_AND_INC_SA {\
  *(DWORD*)&w[0]=*(DWORD*)(Mem_End_minus_2-source-6);\
  *(DWORD*)&w[2]=*(DWORD*)(Mem_End_minus_2-source-2);\
  source+=8;\
}
#endif

// LPEEK and DPEEK are dangerous if source is wrong

#define GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_MEDRES \
  *(DWORD*)&w[0]=LPEEK(source);\
  source+=4;

#define GET_SCREEN_DATA_INTO_REGS_AND_INC_SA_HIRES \
  w0=DPEEK(source);      \
  source+=2;

// planar to chunky
// how to optimise that? don't see a good fit among SSE2?
// we can optimise the whole loop but unrolling maybe not a great idea
#define CALC_COL_LOWRES_AND_DRAWPIXEL(mask) { \
  int nibble=    ((w[3]&(mask))!=0) \
              + (((w[2]&(mask))!=0)<<1)  \
              + (((w[1]&(mask))!=0)<<2)  \
              + (((w[0]&(mask))!=0)<<3); \
  DRAWPIXEL(PCpal+nibble)\
}

#define CALC_COL_MEDRES_AND_DRAWPIXEL(mask)   { \
  int nibble= ((w[1]&(mask))!=0) + (((w[0]&(mask))!=0)<<1) ; \
  DRAWPIXEL_MEDRES(PCpal+nibble)\
}

#define CALC_COL_HIRES_AND_DRAWPIXEL(mask) \
  DRAWPIXEL( ( (w0&(mask)) ? (fore) : (back) )  )

#define DRAWPIXEL(s_add)  *draw_dest_ad++=*(LPDWORD)(s_add);

#define DRAWPIXEL_MEDRES(s) DRAWPIXEL(s)


// small display size or stretched (double, triple, D3D fullscreen...)

#if defined(SSE_VC_INTRINSICS)

#undef DRAW_BORDER_PIXELS 
#define DRAW_BORDER_PIXELS(npixels) {\
  __stosd(draw_dest_ad,*PCpal,npixels);\
  draw_dest_ad+=npixels;\
}

#endif

extern "C" void ASMCALL draw_scanline_32_lowres_pixelwise(int border1,
                                  int picture,int border2,int hscroll) {
#include "draw_c_lowres_scanline.cpp"
}


extern "C" void ASMCALL draw_scanline_32_medres_pixelwise(int border1,
                                   int picture,int border2,int hscroll) {
  border1*=2;border2*=2;
#include "draw_c_medres_scanline.cpp"
}


#undef DRAW_2_BORDER_PIXELS
#undef DRAWPIXEL
#undef DRAWPIXEL_MEDRES
#undef DRAW_BORDER_PIXELS 

#define DRAW_BORDER_PIXELS DRAW_BORDER_PIXELS_A


///////////////////////////////////////////////////////////
////////////////////////// _dw  ///////////////////////////
///////////////////////////////////////////////////////////


// double no stretch + scanlines

#if defined(SSE_VC_INTRINSICS)

#undef DRAW_BORDER_PIXELS
#define DRAW_BORDER_PIXELS(npixels) \
  __stosd(draw_dest_ad,*(PCpal),(npixels)*2);\
  draw_dest_ad+=(npixels)*2;

#endif

#define DRAWPIXEL(s_add)  *draw_dest_ad++=*(DWORD*)(s_add);\
                          *draw_dest_ad++=*(DWORD*)(s_add);


extern "C" void ASMCALL draw_scanline_32_lowres_pixelwise_dw(int border1,
                                    int picture,int border2,int hscroll) {
#include "draw_c_lowres_scanline.cpp"
}


#undef DRAW_2_BORDER_PIXELS
#undef DRAWPIXEL

#undef DRAW_BORDER_PIXELS 
#define DRAW_BORDER_PIXELS DRAW_BORDER_PIXELS_A


///////////////////////////////////////////////////////////
////////////////////////// _400 ///////////////////////////
///////////////////////////////////////////////////////////

// 400 = draw double height scanlines



#if defined(SSE_VC_INTRINSICS)

#undef DRAW_BORDER_PIXELS
#define DRAW_BORDER_PIXELS(npixels) \
  __stosd((draw_dest_ad),*(PCpal),(npixels)*2);\
  __stosd((draw_dest_ad+draw_line_length),*(PCpal),(npixels)*2);\
  draw_dest_ad+=(npixels)*2;

#endif

// __stosd is actually too slow for this
#define DRAWPIXEL(s_add)  \
  *(draw_dest_ad+draw_line_length)=*(DWORD*)(s_add); \
  *(draw_dest_ad+draw_line_length+1)=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add);

extern "C" void ASMCALL draw_scanline_32_lowres_pixelwise_400(int border1,
                                  int picture,int border2,int hscroll) {
#include "draw_c_lowres_scanline.cpp"
}

#define DRAWPIXEL_MEDRES(s_add)  \
  *(draw_dest_ad+draw_line_length)=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add);

extern "C" void ASMCALL draw_scanline_32_medres_pixelwise_400(int border1,
                                    int picture,int border2,int hscroll) {
#include "draw_c_medres_scanline.cpp"
}


#undef DRAW_2_BORDER_PIXELS
#undef DRAWPIXEL
#undef DRAWPIXEL_MEDRES


/* Engineering Hardware Specification Atari Corp:
"In  monochrome mode the border color is always black."
*/

#undef DRAW_BORDER_PIXELS

#if defined(SSE_VC_INTRINSICS)

#define DRAW_BORDER_PIXELS(npixels) {\
  __stosd(draw_dest_ad,0,npixels);\
  draw_dest_ad+=npixels;\
}

#else

#define DRAW_BORDER_PIXELS(npixels) \
  for(int i=npixels;i;i--) \
  { \
    DRAWPIXEL(0);\
  }

#endif

#undef DRAWPIXEL

#define DRAWPIXEL(col) *draw_dest_ad++=(DWORD)(col);

extern "C" void ASMCALL draw_scanline_32_hires(int border1,int picture,
                                               int border2,int) {
#include "draw_c_hires_scanline.cpp"
}

#undef DRAWPIXEL
#undef DRAW_BORDER_PIXELS

////////////
// 3x, 4x //
////////////


#if defined(SSE_VID_SIZE4)

#if defined(SSE_VC_INTRINSICS)

#define DRAW_BORDER_PIXELS(npixels) \
  __stosd((draw_dest_ad),*(PCpal),(npixels)*4);\
  __stosd((draw_dest_ad+draw_line_length),*(PCpal),(npixels)*4);\
  __stosd((draw_dest_ad+draw_line_length+draw_line_length),*(PCpal),(npixels)*4);\
  __stosd((draw_dest_ad+draw_line_length+draw_line_length+draw_line_length), \
    *(PCpal),(npixels)*4);\
  draw_dest_ad+=(npixels)*4;

#else

#define DRAW_BORDER_PIXELS DRAW_BORDER_PIXELS_A

#endif

#define DRAWPIXEL(s_add)  \
  int l=draw_line_length;\
  *(draw_dest_ad+l+0)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+1)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+2)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+3)=*(DWORD*)(s_add); \
  l+=l;\
  *(draw_dest_ad+l+0)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+1)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+2)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+3)=*(DWORD*)(s_add); \
  l+=draw_line_length;\
  *(draw_dest_ad+l+0)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+1)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+2)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+3)=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add); 

extern "C" void ASMCALL draw_scanline_32_lowres_4x(int border1,int picture,
                                                   int border2,int hscroll) {
#include "draw_c_lowres_scanline.cpp"
}

#undef DRAWPIXEL

#if defined(SSE_VC_INTRINSICS)

#undef DRAW_BORDER_PIXELS
#define DRAW_BORDER_PIXELS(npixels) \
  __stosd(draw_dest_ad,*(PCpal),(npixels)*3);\
  __stosd((draw_dest_ad+draw_line_length),*(PCpal),(npixels)*3);\
  __stosd((draw_dest_ad+draw_line_length+draw_line_length),*(PCpal),(npixels)*3);\
  draw_dest_ad+=(npixels)*3;

#endif

#define DRAWPIXEL(s_add)  \
  int l=draw_line_length;\
  *(((draw_dest_ad+l+0)))=*(DWORD*)(s_add); \
  *(((draw_dest_ad+l+1)))=*(DWORD*)(s_add); \
  *(((draw_dest_ad+l+2)))=*(DWORD*)(s_add); \
  l+=l;\
  *(((draw_dest_ad+l+0)))=*(DWORD*)(s_add); \
  *(((draw_dest_ad+l+1)))=*(DWORD*)(s_add); \
  *(((draw_dest_ad+l+2)))=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add);

extern "C" void ASMCALL draw_scanline_32_lowres_3x(int border1,int picture,
                                                   int border2,int hscroll) {
#include "draw_c_lowres_scanline.cpp"
}


#if defined(SSE_VC_INTRINSICS)

#undef DRAW_BORDER_PIXELS
#define DRAW_BORDER_PIXELS(npixels) \
  __stosd((draw_dest_ad),*(PCpal),(npixels)*4);\
  __stosd((draw_dest_ad+draw_line_length),*(PCpal),(npixels)*4);\
  __stosd((draw_dest_ad+draw_line_length+draw_line_length),*(PCpal),(npixels)*4);\
  __stosd((draw_dest_ad+draw_line_length+draw_line_length+draw_line_length),\
    *(PCpal),(npixels)*4);\
  draw_dest_ad+=(npixels)*4;

#else

#undef DRAW_BORDER_PIXELS
#define DRAW_BORDER_PIXELS(npixels) \
  int npix=npixels<<1;  \
  for(int i=npix;i;i--) \
  { \
    DRAWPIXEL_MEDRES(PCpal);\
  }


#endif


#define DRAWPIXEL_MEDRES(s_add)  \
  int l=draw_line_length;\
  *(draw_dest_ad+l+0)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+1)=*(DWORD*)(s_add); \
  l+=l;\
  *(draw_dest_ad+l+0)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+1)=*(DWORD*)(s_add); \
  l+=draw_line_length;\
  *(draw_dest_ad+l+0)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+1)=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add);

extern "C" void ASMCALL draw_scanline_32_medres_4x(int border1,int picture,
                                                   int border2,int hscroll) {
#include "draw_c_medres_scanline.cpp"
}


#undef  DRAWPIXEL_MEDRES

// x3!!!! (if 4:2, 3rd line overwritten + 1 beyond!)
#define DRAWPIXEL_MEDRES(s_add)  \
  *(draw_dest_ad+draw_line_length+0)=*(DWORD*)(s_add); \
  *(draw_dest_ad+draw_line_length+1)=*(DWORD*)(s_add); \
  *(draw_dest_ad+draw_line_length+draw_line_length+0)=*(DWORD*)(s_add); \
  *(draw_dest_ad+draw_line_length+draw_line_length+1)=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add);



#if defined(SSE_VC_INTRINSICS)  // 3 lines, pixels *4! (fixed in assembly too)

#undef DRAW_BORDER_PIXELS
#define DRAW_BORDER_PIXELS(npixels) \
  __stosd(draw_dest_ad,*(PCpal),(npixels)*4);\
  __stosd((draw_dest_ad+draw_line_length),*(PCpal),(npixels)*4);\
  __stosd((draw_dest_ad+draw_line_length+draw_line_length),*(PCpal),(npixels)*4);\
  draw_dest_ad+=(npixels)*4;

#else

#undef DRAW_BORDER_PIXELS
#define DRAW_BORDER_PIXELS(npixels) \
  int npix=(npixels*3)/2;  \
  for(int i=npix;i;i--) \
  { \
    DRAWPIXEL(PCpal);\
  }
  

#endif

extern "C" void ASMCALL draw_scanline_32_medres_3x(int border1,int picture,
                                                   int border2,int hscroll) {
#include "draw_c_medres_scanline.cpp"
}

#undef DRAW_BORDER_PIXELS

#if defined(SSE_VC_INTRINSICS)

#define DRAW_BORDER_PIXELS(npixels) {\
  __stosd(draw_dest_ad,0,(npixels)*2);\
  __stosd(draw_dest_ad+draw_line_length,0,(npixels)*2);\
  draw_dest_ad+=2*(npixels);\
}

#else

#define DRAW_BORDER_PIXELS(npixels) \
  for(int i=npixels;i;i--) \
  { \
    DRAWPIXEL(0);\
  }

#endif

#undef DRAWPIXEL

#define DRAWPIXEL(col) \
  *(draw_dest_ad+draw_line_length)=(DWORD)(col);\
  *(draw_dest_ad+draw_line_length+1)=(DWORD)(col);\
  *draw_dest_ad++=(DWORD)(col);\
  *draw_dest_ad++=(DWORD)(col);

extern "C" void ASMCALL draw_scanline_32_hires_4x(int border1,int picture,
                                                  int border2,int) {
#include "draw_c_hires_scanline.cpp"
}

#undef DRAWPIXEL


#define DRAWPIXEL(col) \
  *(draw_dest_ad+draw_line_length)=0;\
  *(draw_dest_ad+draw_line_length+1)=0;\
  *draw_dest_ad++=(DWORD)(col);\
  *draw_dest_ad++=(DWORD)(col);

extern "C" void ASMCALL draw_scanline_32_hires_4x_scan(int border1,int picture,
                                                       int border2,int) {
#include "draw_c_hires_scanline.cpp"
}


// one line only: simpler, faster

#undef DRAWPIXEL
#undef DRAW_BORDER_PIXELS

#if defined(SSE_VC_INTRINSICS)

#define DRAW_BORDER_PIXELS(npixels) \
  __stosd((draw_dest_ad),*(PCpal),(npixels)*4);\
  draw_dest_ad+=(npixels)*4;

#else

#define DRAW_BORDER_PIXELS DRAW_BORDER_PIXELS_A

#endif

#define DRAWPIXEL(s_add)  \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add);

extern "C" void ASMCALL draw_scanline_32_lowres_4w(int border1,int picture,
                                                   int border2,int hscroll) {
#include "draw_c_lowres_scanline.cpp"
}


#undef DRAWPIXEL

#if defined(SSE_VC_INTRINSICS)

#undef DRAW_BORDER_PIXELS
#define DRAW_BORDER_PIXELS(npixels) \
  __stosd((draw_dest_ad),*(PCpal),(npixels)*3);\
  draw_dest_ad+=(npixels)*3;

#endif

#define DRAWPIXEL(s_add)  \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add);

extern "C" void ASMCALL draw_scanline_32_lowres_3w(int border1,int picture,
                                                   int border2,int hscroll) {
#include "draw_c_lowres_scanline.cpp"
}

#undef DRAWPIXEL_MEDRES

#define DRAWPIXEL_MEDRES(s_add)  \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add);

#undef DRAW_BORDER_PIXELS

#if defined(SSE_VC_INTRINSICS)

#define DRAW_BORDER_PIXELS(npixels) \
  __stosd((draw_dest_ad),*(PCpal),(npixels)*4);\
  draw_dest_ad+=(npixels)*4;

#else

#define DRAW_BORDER_PIXELS(npixels) \
  int ndots=npixels<<1; \
  for(int i=ndots;i;i--) \
  { \
    DRAWPIXEL_MEDRES(PCpal);\
  }

#endif


extern "C" void ASMCALL draw_scanline_32_medres_2w(int border1,int picture,
                                                   int border2,int hscroll) {
#include "draw_c_medres_scanline.cpp"
}


#undef DRAWPIXEL

#define DRAWPIXEL(col) \
  *draw_dest_ad++=(DWORD)(col);\
  *draw_dest_ad++=(DWORD)(col);


#undef DRAW_BORDER_PIXELS
#if defined(SSE_VC_INTRINSICS) && defined(SSE_X64)
#define DRAW_BORDER_PIXELS(npixels) {\
  __stosq((PDWORD64)draw_dest_ad,0,(npixels));\
  draw_dest_ad+=(npixels)*2;}
#else
//#define DRAW_BORDER_PIXELS DRAW_BORDER_PIXELS_A
#define DRAW_BORDER_PIXELS(npixels) \
  for(int i=npixels;i;i--) \
  { \
    DRAWPIXEL(0);\
  }
#endif

void ASMCALL draw_scanline_32_hires_2w(int border1,int picture,int border2,int) {
#include "draw_c_hires_scanline.cpp"
}

#undef DRAWPIXEL
#undef DRAW_BORDER_PIXELS

#endif//#if defined(SSE_VID_SIZE4)


///////////////////
// Single pixels //
///////////////////

#if defined(SSE_VID_SINGLEPIX)

// The new pack of bugs...

#if defined(SSE_VC_INTRINSICS) && defined(SSE_X64)

#if defined(SSE_420R5) // Fix display video buffer + single pixels border alignment

#define DRAW_BORDER_PIXELS(npixels) \
  DWORDLONG x=*(PCpal);\
  /*x<<=32;*/\
  __stosq((PDWORD64)draw_dest_ad,x,(npixels));\
  draw_dest_ad+=(npixels)*2;

#else

#define DRAW_BORDER_PIXELS(npixels) \
  DWORDLONG x=*(PCpal);\
  x<<=32;\
  __stosq((PDWORD64)draw_dest_ad,x,(npixels));\
  draw_dest_ad+=(npixels)*2;

#endif

#else
#define DRAW_BORDER_PIXELS DRAW_BORDER_PIXELS_A // = npixels * DRAWPIXELS
#endif

#undef DRAWPIXEL
#define DRAWPIXEL(s_add)  *draw_dest_ad++=*(DWORD*)(s_add);\
                          *draw_dest_ad++=0;


void ASMCALL draw_scanline_32_lowres_pixelwise_dw_sp(int border1,
                                    int picture,int border2,int hscroll) {
#include "draw_c_lowres_scanline.cpp"
}


#undef DRAW_BORDER_PIXELS
#if defined(SSE_VC_INTRINSICS) && defined(SSE_X64)

#if defined(SSE_420R5) // Fix display no video buffer + single pixels border alignment

#define DRAW_BORDER_PIXELS(npixels) \
  {DWORDLONG x=*(PCpal);\
  /*x<<=32;*/\
  __stosq((PDWORD64)draw_dest_ad,x,(npixels));\
  __stosq((PDWORD64)(draw_dest_ad+draw_line_length),x,(npixels));\
  draw_dest_ad+=(npixels)*2;}

#else

#define DRAW_BORDER_PIXELS(npixels) \
  {DWORDLONG x=*(PCpal);\
  x<<=32;\
  __stosq((PDWORD64)draw_dest_ad,x,(npixels));\
  __stosq((PDWORD64)(draw_dest_ad+draw_line_length),x,(npixels));\
  draw_dest_ad+=(npixels)*2;}

#endif

#else
#define DRAW_BORDER_PIXELS DRAW_BORDER_PIXELS_A
#endif

#undef DRAWPIXEL

#if defined(SSE_420R5) // Fix display no video buffer + single pixels

#define DRAWPIXEL(s_add)  \
  { draw_type* draw_dest_ad2=draw_dest_ad+draw_line_length;\
    *draw_dest_ad++=*(DWORD*)(s_add); \
    *draw_dest_ad++=0; \
    *draw_dest_ad2++=*(DWORD*)(s_add); \
    *draw_dest_ad2=0;} \


#else

#define DRAWPIXEL(s_add)  \
  { draw_type* draw_dest_ad2=draw_dest_ad+draw_line_length;\
    *draw_dest_ad=*(DWORD*)(s_add); \
    *draw_dest_ad++=0; \
    *draw_dest_ad2=*(DWORD*)(s_add); \
    *draw_dest_ad2++=0;} \

#endif

void ASMCALL draw_scanline_32_lowres_pixelwise_400_sp(int border1,
                                    int picture,int border2,int hscroll) {
#include "draw_c_lowres_scanline.cpp"
}

#if defined(SSE_VID_SIZE4)

// 3 treble size lines
#undef DRAW_BORDER_PIXELS
#define DRAW_BORDER_PIXELS DRAW_BORDER_PIXELS_A

#undef DRAWPIXEL
#define DRAWPIXEL(s_add)  \
  int l=draw_line_length;\
  *(((draw_dest_ad+l+0)))=*(DWORD*)(s_add); \
  *(((draw_dest_ad+l+1)))=0; \
  *(((draw_dest_ad+l+2)))=0; \
  l+=l;\
  *(((draw_dest_ad+l+0)))=*(DWORD*)(s_add); \
  *(((draw_dest_ad+l+1)))=0; \
  *(((draw_dest_ad+l+2)))=0; \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=0; \
  *draw_dest_ad++=0;

void ASMCALL draw_scanline_32_lowres_3x_sp(int border1,
                                    int picture,int border2,int hscroll) {
#include "draw_c_lowres_scanline.cpp"
}


// 4x wide, 2x height // 3x height
#undef  DRAWPIXEL_MEDRES
// x3!!!! (if 4:2, 3rd line overwritten + 1 beyond!)
#define DRAWPIXEL_MEDRES(s_add)  \
  { draw_type* draw_dest_ad2=draw_dest_ad+draw_line_length;\
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=0; \
  *draw_dest_ad2++=*(DWORD*)(s_add); \
  *draw_dest_ad2=0;}

#undef DRAW_BORDER_PIXELS
#if defined(SSE_VC_INTRINSICS)  && defined(SSE_X64)

#if defined(SSE_420R5) // fix border align

#define DRAW_BORDER_PIXELS(npixels) \
  {DWORDLONG x=*(PCpal);\
 /* x<<=32; */\
  __stosq((PDWORD64)draw_dest_ad,x,(npixels)*2);\
  __stosq((PDWORD64)(draw_dest_ad+draw_line_length),x,(npixels)*2);\
  __stosq((PDWORD64)(draw_dest_ad+draw_line_length+draw_line_length),x,(npixels)*2);\
  draw_dest_ad+=(npixels)*4;}

#else

#define DRAW_BORDER_PIXELS(npixels) \
  {DWORDLONG x=*(PCpal);\
  x<<=32;\
  __stosq((PDWORD64)draw_dest_ad,x,(npixels)*2);\
  __stosq((PDWORD64)(draw_dest_ad+draw_line_length),x,(npixels)*2);\
  __stosq((PDWORD64)(draw_dest_ad+draw_line_length+draw_line_length),x,(npixels)*2);\
  draw_dest_ad+=(npixels)*4;}

#endif

#else
#define DRAW_BORDER_PIXELS DRAW_BORDER_PIXELS_A
#endif

void ASMCALL draw_scanline_32_medres_3x_sp(int border1,
                                    int picture,int border2,int hscroll) {
#include "draw_c_medres_scanline.cpp"
}


// 4 large lines: double pixels actually
#undef DRAW_BORDER_PIXELS
#define DRAW_BORDER_PIXELS DRAW_BORDER_PIXELS_A

#undef DRAWPIXEL
#define DRAWPIXEL(s_add)  \
  int l=draw_line_length;\
  *(draw_dest_ad+l+0)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+1)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+2)=0; \
  *(draw_dest_ad+l+3)=0; \
  l+=l;\
  *(draw_dest_ad+l+0)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+1)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+2)=0; \
  *(draw_dest_ad+l+3)=0; \
  l+=draw_line_length;\
  *(draw_dest_ad+l+0)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+1)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+2)=0; \
  *(draw_dest_ad+l+3)=0; \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=0; \
  *draw_dest_ad++=0;

void ASMCALL draw_scanline_32_lowres_4x_sp(int border1,
                                    int picture,int border2,int hscroll) {
#include "draw_c_lowres_scanline.cpp"
}

#undef DRAW_BORDER_PIXELS
#if defined(SSE_VC_INTRINSICS) && defined(SSE_X64)

#if defined(SSE_420R5) // fix border align

#define DRAW_BORDER_PIXELS(npixels) \
  {DWORDLONG x=*(PCpal);\
  /*  x<<=32; */\
  __stosq((PDWORD64)draw_dest_ad,x,(npixels)*2);\
  __stosq((PDWORD64)(draw_dest_ad+draw_line_length),x,(npixels)*2);\
  __stosq((PDWORD64)(draw_dest_ad+draw_line_length+draw_line_length),x,(npixels)*2);\
  __stosq((PDWORD64)(draw_dest_ad+draw_line_length+draw_line_length+draw_line_length),x,(npixels)*2);\
  draw_dest_ad+=(npixels)*4;}

#else

#define DRAW_BORDER_PIXELS(npixels) \
  {DWORDLONG x=*(PCpal);\
  x<<=32;\
  __stosq((PDWORD64)draw_dest_ad,x,(npixels)*2);\
  __stosq((PDWORD64)(draw_dest_ad+draw_line_length),x,(npixels)*2);\
  __stosq((PDWORD64)(draw_dest_ad+draw_line_length+draw_line_length),x,(npixels)*2);\
  __stosq((PDWORD64)(draw_dest_ad+draw_line_length+draw_line_length+draw_line_length),x,(npixels)*2);\
  draw_dest_ad+=(npixels)*4;}

#endif

#else
#define DRAW_BORDER_PIXELS DRAW_BORDER_PIXELS_A
#endif

#undef DRAWPIXEL_MEDRES
#define DRAWPIXEL_MEDRES(s_add)  \
  int l=draw_line_length;\
  *(draw_dest_ad+l+0)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+1)=0; \
  l+=l;\
  *(draw_dest_ad+l+0)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+1)=0; \
  l+=draw_line_length;\
  *(draw_dest_ad+l+0)=*(DWORD*)(s_add); \
  *(draw_dest_ad+l+1)=0; \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=0;

void ASMCALL draw_scanline_32_medres_4x_sp(int border1,
                                    int picture,int border2,int hscroll) {
#include "draw_c_medres_scanline.cpp"
}


// 2 large lines
#undef DRAW_BORDER_PIXELS
#if defined(SSE_VC_INTRINSICS) && defined(SSE_X64)
#define DRAW_BORDER_PIXELS(npixels) {\
  __stosq((PDWORD64)draw_dest_ad,0,(npixels));\
  __stosq((PDWORD64)(draw_dest_ad+draw_line_length),0,(npixels));\
  draw_dest_ad+=(npixels)*2;}
#else
//#define DRAW_BORDER_PIXELS DRAW_BORDER_PIXELS_A

#define DRAW_BORDER_PIXELS(npixels) \
  for(int i=npixels;i;i--) \
  { \
    DRAWPIXEL(0);\
  }

#endif


#undef DRAWPIXEL

#define DRAWPIXEL(col) \
  *(draw_dest_ad+draw_line_length)=(DWORD)(col);\
  *(draw_dest_ad+draw_line_length+1)=0;\
  *draw_dest_ad++=(DWORD)(col);\
  *draw_dest_ad++=0;

void ASMCALL draw_scanline_32_hires_4x_sp(int border1,
                                    int picture,int border2,int) {
#include "draw_c_hires_scanline.cpp"
}


#undef DRAWPIXEL
#define DRAWPIXEL(col) \
  *draw_dest_ad++=(DWORD)(col);\
  *draw_dest_ad++=0;

void ASMCALL draw_scanline_32_hires_4x_scan_sp(int border1,
                                    int picture,int border2,int) {// 2nd line=0
#include "draw_c_hires_scanline.cpp"
}; 


// 1 treble size line

#undef DRAW_BORDER_PIXELS
#define DRAW_BORDER_PIXELS DRAW_BORDER_PIXELS_A

#undef DRAWPIXEL
#define DRAWPIXEL(s_add)  \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=0;

void ASMCALL draw_scanline_32_lowres_3w_sp(int border1,
                                    int picture,int border2,int hscroll) {
#include "draw_c_lowres_scanline.cpp"
}


// 1 large line
#undef DRAWPIXEL
#undef DRAW_BORDER_PIXELS
#if defined(SSE_VC_INTRINSICS) && defined(SSE_X64)

#if defined(SSE_420R5) // fix border align

#define DRAW_BORDER_PIXELS(npixels) \
  {DWORDLONG x=*(PCpal);\
  /* x<<=32; */ \
  __stosq((PDWORD64)draw_dest_ad,x,(npixels)*2);\
  draw_dest_ad+=(npixels)*4;}

#else

#define DRAW_BORDER_PIXELS(npixels) \
  {DWORDLONG x=*(PCpal);\
  x<<=32;\
  __stosq((PDWORD64)draw_dest_ad,x,(npixels)*2);\
  draw_dest_ad+=(npixels)*4;}

#endif

#else
#define DRAW_BORDER_PIXELS DRAW_BORDER_PIXELS_A
#endif

#define DRAWPIXEL(s_add)  \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=0; \
  *draw_dest_ad++=0;

void ASMCALL draw_scanline_32_lowres_4w_sp(int border1,
                                    int picture,int border2,int hscroll) {
#include "draw_c_lowres_scanline.cpp"
}


#undef DRAWPIXEL_MEDRES
#define DRAWPIXEL_MEDRES(s_add)  \
  *draw_dest_ad++=*(DWORD*)(s_add); \
  *draw_dest_ad++=0;

#undef DRAW_BORDER_PIXELS
#define DRAW_BORDER_PIXELS DRAW_BORDER_PIXELS_A

void ASMCALL draw_scanline_32_medres_2w_sp(int border1,
                                    int picture,int border2,int hscroll) {
#include "draw_c_medres_scanline.cpp"
}

#undef DRAW_BORDER_PIXELS
#if defined(SSE_VC_INTRINSICS) && defined(SSE_X64)
#define DRAW_BORDER_PIXELS(npixels) {\
  __stosq((PDWORD64)draw_dest_ad,0,(npixels));\
  draw_dest_ad+=(npixels)*2;}
#else
//#define DRAW_BORDER_PIXELS DRAW_BORDER_PIXELS_A
#define DRAW_BORDER_PIXELS(npixels) \
  for(int i=npixels;i;i--) \
  { \
    DRAWPIXEL(0);\
  }
#endif

#undef DRAWPIXEL
#define DRAWPIXEL(col) \
  *draw_dest_ad++=(DWORD)(col);\
  *draw_dest_ad++=0;

void ASMCALL draw_scanline_32_hires_2w_sp(int border1,int picture,int border2,int) {
#include "draw_c_hires_scanline.cpp"
}

#undef DRAWPIXEL
#undef DRAW_BORDER_PIXELS


#endif//#if defined(SSE_VID_SIZE4)
#endif//#if defined(SSE_VID_SINGLEPIX)
