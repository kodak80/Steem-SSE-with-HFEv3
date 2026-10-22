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
FILE: palette.cpp
DESCRIPTION: General palette utility functions. This covers both the code
to create the ST palette and also the code to add to the PC palette.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <palette.h>
#include <options.h>
#include <draw.h>
#include <shifter.h>
#include <display.h>
#include <emulator.h>
#include <debug.h>


LONG palette_table[4096];
LONG logpal[257];
BYTE palette_exec_mem[64+PAL_EXTRA_BYTES];

#ifdef WIN32
HPALETTE winpal=NULL,oldwinpal;
HDC PalDC;
#endif

#ifdef UNIX
const LONG standard_palette[18][2]={
	{0,0},
	{8,0xff0000},{9,0x00ff00},{10,0x0000ff},
	{11,0xffff00},{12,0x00ffff},{13,0xff00ff},
	{13,0xc0c0c0},
	{14,0xe0e0e0},
	{247,0x0000c0},
	{248,0x800000},{249,0x008000},{250,0x000080},
	{251,0x808000},{252,0x008080},{253,0x800080},
	{254,0x808080},
	{255,0xffffff}};
Colormap colormap=0;
XColor new_pal[257];
#endif

char *rgb_txt[3]={"Red","Green","Blue"}; // for option page
int palhalf=0,palnum=0;
short col_brightness=0,col_contrast=0;
short col_gamma[3]={0,0,0}; // RGB
bool palette_changed=false;


void make_palette_table(int brightness,int contrast) {
  contrast+=256;
  LONG c;
  for(int n=0;n<4096;n++) // $000 -> $FFF
  {
    c=0;
    for(int rgb=0;rgb<3;rgb++)
    { //work out levels
      int l=(n>>((2-rgb)*4))&15; // get one nibble from n as bit0-3
      if(SSEOptions.FullSpectrumPal) // new scales to make colours more vivid
      {
        l=(l>>3) | (l<<1);
        l&=0xF;
        l*=(IS_STF)?18:17; // STF 0-252, 8 steps, STE 0-255, 16 steps
      }
      else
      {
        l=((l<<5)+(l<<1)) & (15<<4); // unscramble bits and multiply by 16
                                     // to get number between 0 and 240
      }
      l*=contrast;
      l/=256;
      l+=brightness;
/*  Like brightness and contrast, we compute the gamma ourselves, using the
    common formula. Normally gamma[rgb] can't be set to -128 in the GUI.
    Not sure of the order (after brightness).
    Comparing with a CRT display, I couldn't confirm that Steem needs gamma
    correction, but it's always a handy setting.
*/
      int avg=128;
      ASSERTB(col_gamma[rgb]>-avg);
      if(col_gamma[rgb]
#ifndef SSE_LEAN_AND_MEAN
        &&col_gamma[rgb]>-avg
#endif
        )
        l=(int)(pow(l/255.0,(double)avg/((double)col_gamma[rgb]+128))*255);
      if(l<0)
        l=0;
      else if(l>255)
        l=255;
      { //24/32 mode, 8-bit too (value used in call to add to palette)
        c<<=8;
        c+=l;
      }
    }//nxt rgb
    if(OPTION_GREYSCREEN||OPTION_GREENSCREEN)
    {
      int avg=(c&0xFF)+((c>>8)&0xFF)+((c>>16)&0xFF);
      avg/=3;
      c=OPTION_GREENSCREEN ? (avg<<8) : (avg+(avg<<8)+(avg<<16));
    }
    if(rgb32_bluestart_bit)
      // 0xRRGGBB00
      c<<=8;
    palette_table[n]=c;
  }
}


void palette_prepare(bool) {
  palette_remove();
}


void palette_remove() {
#ifdef WIN32
  if(winpal)
  {
    SetSystemPaletteUse(PalDC,SYSPAL_STATIC);
    SelectPalette(PalDC,oldwinpal,1);
    DeleteDC(PalDC);
    DeleteObject(winpal);
    winpal=NULL;
  }
#endif
}


WORD palette_add_entry(DWORD col) {// Add BGR colour to palette, only used by palette_copy()
#ifdef WIN32
  PALETTEENTRY pbuf;
  int n,m;
  pbuf.peFlags=PC_RESERVED;
  pbuf.peRed=(BYTE)((col&0xff0000)>>16);
  pbuf.peGreen=(BYTE)((col&0x00ff00)>>8);
  pbuf.peBlue=(BYTE)((col&0x0000ff));
  n=10+palhalf+palnum;
  if(palnum<=117)
  {
    if(*((LONG*)(logpal+n))==*((LONG*)&pbuf))
    {
      palnum++;
      n++;
      return (WORD)(n|(n<<8));
    }
    palnum++;
  }
  else
  {
    n=10+palhalf;
    for(m=0;m<palnum;m++)
      if(*(LONG*)(logpal+10+palhalf+m)==*((LONG*)&pbuf))
      {
        n=10+palhalf+m;
        n++;
        return (WORD)(n|(n<<8));
      }
  }
  *((PALETTEENTRY*)(logpal+n))=pbuf;
  palette_changed=true;
  n++;
  return (WORD)(n|(n<<8));
#endif//WIN32

#ifdef UNIX
  if(XD==NULL||colormap==0) return 0;
  int n;
  if(palnum<=127)
    while((DWORD)logpal[palhalf+palnum]==0xffffffff) 
      palnum++;
  if(palnum<=127)
  {
    n=palhalf+palnum;
    new_pal[n].red=(col&0xff0000)>>8;
    new_pal[n].green=(col&0xff00);
    new_pal[n].blue=(col&0xff)<<8;
    logpal[n]=col;
 //   XStoreColor(XD,colormap,&xc);
    palnum++;
    palette_changed=true;
    return (WORD)(n|(n<<8));
  }
  else
  {
    n=palhalf;
    for(int m=0;m<palnum;m++)
    {
      if(*(DWORD*)(logpal+palhalf+m)==col)
      {
        n=palhalf+m;
        return (WORD)(n|(n<<8));
      }
    }
  }
  return 0;
#endif//UNIX

}


void palette_convert(int n) {
  PCpal[n]=palette_table[(STpal[n]&0xfff)]; // TODO shouldn't we use $777 for STF?
}


void palette_copy() {
  int n_cols=PAL_SIZE;
  switch(screen_res) {
  case MEDRES:
    n_cols=4;
    break;
  case HIRES:
    return;  ///// Added by Tarq
#ifndef NO_CRAZY_MONITOR
  case 3: //extended monitor
    if(em_planes==1) 
      return;
    n_cols=(1<<em_planes);
    break;
#endif
  }
  for(int n=0;n<n_cols;n++)
    PCpal[n]=palette_add_entry(palette_table[(STpal[n]&0xfff)]);
}


void palette_convert_all() {
  for(int n=0;n<PAL_SIZE;n++) 
    palette_convert(n);
#if !defined(SSE_NO_FALCONMODE)
  for(int n=0;n<256;n++) 
    emudetect_falcon_palette_convert(n);
#endif
}

#undef LOGSECTION
