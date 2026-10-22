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

DOMAIN: Emu/Rendering
FILE: shifter.cpp
DESCRIPTION: The ST was a barebone machine made up of cheap components hastily
patched together, including the video shifter. The Shifter is an Atari custom
chip. It is the ST's fastest chip: 32mhz.
A shifter was needed to translate the video RAM into the picture.
Such a system was chosen because RAM space and bandwidth were precious. Chunk
modes are only practical with byte or more pixel sizes. The ST used planar
modes, with only one plane for high resolution, 2 for medium and 4 planes for
low resolution. The planes are not separate like apparently on the Amiga, each
group of four 16bit words contains the data for the planes (1,2 or 4) and
there is only one video pointer. This made plotting a simple point in low res
a daunting task!
Some aspects of the Shifter are emulated at high level here but its core
function, shifting video rasters, is emulated through direct rendering of
ST video memory by assembly modules (only 32bit debug builds now) or C routines
in draw_c.cpp.
Those functions are called by DrawScanlineToEnd(), for a full line,
or Render(), for the colour screen only, whenever we should update the image,
which can be often according to the program (palette or resolution changes on
the fly).
On the STE, the GST Shifter has a sound sample playing feature. This
is also emulated at high level. We call it STE sound and not DMA sound because
on the ST, DMA (direct memory access) implies taking the bus from the CPU.
For convenience, emulation of some (real) shifter tricks is located in glue.cpp
note: SDP=shifter draw pointer
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <draw.h>
#include <display.h>
#include <debug.h>
#include <computer.h>
#if defined(SSE_VID_STVL1)
#include <interface_stvl.h>
#endif
#include <debug_framereport.h>


WORD *SteSoundChannelBuf=NULL;
DWORD SteSoundChannelBufLen=0; // variable
const WORD SteSoundModeToFreq[4]={6258,12517,25033,50066}; // available frequencies :(

WORD STpal[PAL_SIZE]; // C
DWORD SteSoundChannelBufIdx;
MEM_ADDRESS shifter_draw_pointer; // C
int SteSoundOutputCountdown,SteSoundSamplesCountdown;
DU16 SteSoundLastWord;
WORD SteSoundFreq; // one word only
SHORT shifter_tick8;
SHORT shifter_x,shifter_y;
BYTE shifter_hscroll; // C
BYTE shifter_sound_mode;

TShifter::TShifter() {
  ZeroMemory(this,sizeof(TShifter));
#ifdef WIN32
#if !defined(SSE_420R4)
  draw_temp_line_buf_lim=draw_temp_line_buf+DRAW_TEMP_LINE_BUF_LEN;
#endif
#endif
#if !defined(SSE_420R5)
  WakeupShift=SHIFTER_DEFAULT_WAKEUP;
#endif
}


void TShifter::Reset(bool const Cold) {
  if(Cold)
  {
    a_s_t=A_S_T;
    for(int i=0;i<NMODECHANGES;i++)
    {
      glue_freq_change[i]=0;
      shifter_mode_change[i]=0; // Hackabonds after reset
      GlueFreqChangeTime[i]=ShifterModeChangeTime[i]=a_s_t;
    }
    nVbl=0;
  }
  SoundFifoIdx=ShiftMode=Preload=shifter_hscroll=0;
#ifdef WIN32
  if(!OPTION_ADVANCED)
    WakeupShift=0;
  else if(OPTION_RANDOM_WU)
#endif
    WakeupShift=((rand()%8)==7); // 0 (likely) or 1
#if defined(SSE_MEGASTE)
  if(IS_MEGASTE)
    WakeupShift=1-WakeupShift; // maybe
#endif
  video_first_draw_line=0;
  video_last_draw_line=shifter_y;
  shifter_sound_mode=0;
  SteSoundFreq=SteSoundModeToFreq[0];
  SteSoundOutputCountdown=SteSoundSamplesCountdown=0;
#if defined(SSE_SOUND_CARTRIDGE) //hack to reduce pops
  if(SSEConfig.mv16)
    SteSoundLastWord.d16=(DONGLE_ID==TDongle::MUSIC_MASTER)?29408:8352;
  else
#endif
    SteSoundLastWord.d16=0;
}


void TShifter::Restore() {
  ShiftMode&=3; //2bit register
  HiresRaster=0;
  shifter_hscroll&=0xF; //4bit register
  SoundFifoIdx&=3; // 2bit counter
  Microwire.old_top_val_l=Microwire.top_val_l;
  Microwire.old_top_val_r=Microwire.top_val_r;
  SteSoundFreq=SteSoundModeToFreq[shifter_sound_mode&3];
  if(OPTION_SHIFTER_WU<-SHIFTER_MAX_WU_SHIFT || OPTION_SHIFTER_WU>SHIFTER_MAX_WU_SHIFT)
    OPTION_SHIFTER_WU=SHIFTER_DEFAULT_WAKEUP;
}


///////////
// VIDEO //
///////////

// don't render if we're below or beyond video RAM (shifter trick reckoning panic "0byte"...)
#define CHECK_VIDEO_RAM\
  if(draw_dest_ad<Draw.limit1) \
  {\
    return;\
  }


void TShifter::DrawScanlineToEnd() {
  //ASSERT(!OPTION_C3);
  if(!draw_lock)
    return;
  CHECK_VIDEO_RAM;
  MEM_ADDRESS nsdp;
#if !defined(SSE_NO_FALCONMODE)
  if(emudetect_falcon_mode!=EMUD_FALC_MODE_OFF)
  {
    int pic=320*emudetect_falcon_mode_size,bord=0;
    // We double the size of borders too to keep the aspect ratio of the screen the same
    if(border) 
      bord=SideBorderSize*emudetect_falcon_mode_size;
    if(scan_y>=draw_first_possible_line && scan_y<draw_last_possible_line)
    {
      bool in_border=(scan_y>=draw_first_scanline_for_border 
        && scan_y<draw_last_scanline_for_border);
      bool in_pic=(scan_y>=video_first_draw_line && scan_y<video_last_draw_line);
      if(in_pic||in_border)
      {
        // We only have 200 scanlines, but if emudetect_falcon_mode_size==2
        // then the picture must be 400 pixels high. So to make it work we
        // draw two different lines at the end of each scanline.
        // To make it more confusing in some drawing modes if
        // emudetect_falcon_mode_size==1 we have to draw two identical
        // lines. That is handled by the draw_scanline routine. 
        for(int n=0;n<emudetect_falcon_mode_size;n++)
        {
          if(in_pic)
          {
            nsdp=shifter_draw_pointer+pic*emudetect_falcon_mode;
            ///////////////// RENDER VIDEO /////////////////
            draw_scanline(bord,pic,bord,HSCROLL);
            shifter_draw_pointer=nsdp;
          }
          else
            ///////////////// RENDER VIDEO /////////////////
            draw_scanline(bord+pic+bord,0,0,0);
          draw_dest_ad=draw_dest_next_scanline;
          draw_dest_next_scanline+=draw_dest_increase_y;
        }
      }
    }
    shifter_tick8=HSCROLL; //start by drawing this pixel
    scanline_drawn_so_far=SideBorderSize+320+SideBorderSize;
  } else 
#endif
#ifndef NO_CRAZY_MONITOR
  if(extended_monitor&&(scan_y>=draw_first_possible_line && scan_y<draw_last_possible_line))
  {
    int h=MIN(em_height,Disp.SurfaceHeight);
    int w=MIN(em_width,Disp.SurfaceWidth);
    if(extended_monitor==1)
    {	// Borders needed, before hack
      if(em_planes==1)
      { //mono
        int y=h/2-VER_PIXELS_HI/2,x=(w/2-HOR_PIXELS_HI/2)&-16;
        if(scan_y<h)
        {
          if(scan_y<y||scan_y>=y+VER_PIXELS_HI)
          {
#if defined(SSE_DRAW_C)
            if(draw_dest_ad+(w)*Draw.HorPix<Draw.limit2)
#else
            if(draw_dest_ad+(w)*sizeof(DWORD)*Draw.HorPix<Draw.limit2)
#endif
              ///////////////// RENDER VIDEO /////////////////
              draw_scanline(w/16,0,0,0);
          }
          else
          {
#if defined(SSE_DRAW_C)
            if(draw_dest_ad+(w)*Draw.HorPix<Draw.limit2)
#else
            if(draw_dest_ad+(w)*sizeof(DWORD)*Draw.HorPix<Draw.limit2)
#endif
              ///////////////// RENDER VIDEO /////////////////
              draw_scanline(x/16,HOR_PIXELS_HI/16,w/16-x/16-HOR_PIXELS_HI/16,0);
            SHIFT_SDP(80);
          }
        }
      }
      else // colour
      {
        int y=h/2-VER_PIXELS_LO/2,x=(w/2-HOR_PIXELS_LO/2)&-16;
        if(scan_y<h)
        {
          if(scan_y<y||scan_y>=y+VER_PIXELS_LO)
          {
#if defined(SSE_DRAW_C)
            if(draw_dest_ad+(w)*Draw.HorPix<Draw.limit2)
#else
            if(draw_dest_ad+(w)*sizeof(DWORD)*Draw.HorPix<Draw.limit2)
#endif
              ///////////////// RENDER VIDEO /////////////////
              draw_scanline(w,0,0,0);
          }
          else
          {
#if defined(SSE_DRAW_C)
            if(draw_dest_ad+(w)*Draw.HorPix<Draw.limit2)
#else
            if(draw_dest_ad+(w)*sizeof(DWORD)*Draw.HorPix<Draw.limit2)
#endif
              ///////////////// RENDER VIDEO /////////////////
              draw_scanline(x,HOR_PIXELS_LO,w-x-HOR_PIXELS_LO,0);
            SHIFT_SDP(160);
          }
        }
      }
    }
    else
    {
      if(scan_y<h)
      {
        if(em_planes==1) 
          w/=16;
        if(screen_res==MEDRES)
          w/=2; // medium res routine draws two pixels for every one w
#if defined(SSE_DRAW_C)
        if(draw_dest_ad+(w)*Draw.HorPix<Draw.limit2)
#else
        if(draw_dest_ad+(w)*sizeof(DWORD)*Draw.HorPix<Draw.limit2)
#endif
          ///////////////// RENDER VIDEO /////////////////
          draw_scanline(0,w,0,0);
      }
      int real_planes=em_planes;
      if(screen_res==MEDRES)
        real_planes=2;
      SHIFT_SDP(em_width*real_planes/8);
    }
  } else
#endif
  // Colour, regular monitor
  if(screen_res<HIRES)
  {
    Render(Glue.ScanlineTiming[Glue.RENDER_CYCLE][Glue.FREQ_IDX_50]
           +HOR_PIXELS_LO*TICKS8+SideBorderSize*TICKS8,DISPATCHER_DSTE);
    HblStartingHscroll=HSCROLL; // save the real one
#ifdef SSE_LEAN_AND_MEAN
    shifter_tick8=HSCROLL>>screen_res; // Cool STE
#else
    shifter_tick8=HSCROLL; //start by drawing this pixel (note: for next line)
    if(screen_res==MEDRES)
      shifter_tick8=HSCROLL/2;
#endif
  }
  else // Monochrome, regular monitor
  {
    // prepare buffer & 1line ASM routine
    if(scan_y>=video_first_draw_line && scan_y<video_last_draw_line)
    {
      nsdp=shifter_draw_pointer+80;
      if(OPTION_C2&&shifter_draw_pointer>=himem) // no RAM
        shifter_draw_pointer=0+scan_y*80; // just a bad approximation of the effect      
      if(freq_change_this_scanline)
      {
        Glue.CheckSideOverscan();
        if(draw_line_off)
        {
          for(int i=0;i<=20;i++)
          {
            Scanline[i]=SafeLPeek(i*4+shifter_draw_pointer); // save ST memory
            SafeLPoke(i*4+shifter_draw_pointer,0);
          }
        }
      }
#if defined(SSE_SHIFTER_HIRESRASTERS)
/*  shifter_tick8 isn't used, there's no support for hscroll in the routines
    drawing the monochrome scanlines.
    Those routines work with a word precision (16 pixels), so they can't be
    used to implement HSCROLL. TODO?
    We do it here in C for those rare cases (Time Slices demo).
    At least in monochrome we mustn't deal with bit planes, there's only
    one plane.*/
      if(OPTION_HACKS&&(HSCROLL||HiresRaster))
      {
        // save ST memory
        for(int i=0;i<=20;i++)
          Scanline[i]=SafeLPeek(i*4+shifter_draw_pointer);
        if(HSCROLL)
        {
          // hack: shift pixels of full scanline in video RAM
          for(MEM_ADDRESS i=shifter_draw_pointer; i<nsdp;i+=2)
          {
            WORD new_value=SafeDPeek(i)<<HSCROLL;  // shift first bits away
            new_value|=SafeDPeek(i+2)>>(16-HSCROLL); // add first bits of next word at the end
            SafeDPoke(i,new_value);
          }
        }
      }
      if(HiresRaster)
      { // hack: apply raster effect in video memory
        for(int byte=0;byte<HiresRaster;byte++)
        {
          BYTE x=SafePeek(shifter_draw_pointer+byte);
          SafePoke(shifter_draw_pointer+byte,x^Scanline2[byte]); // using XOR
        }
      }
#endif//#if defined(SSE_SHIFTER_HIRESRASTERS)
      if((!(ShiftMode&HIRES))&&!freq_change_this_scanline||(Glue.SyncMode&TGlue::SYNCEXT))
      {} // no output
      else if(scan_y>=draw_first_possible_line && scan_y<draw_last_possible_line)
      {
        if(border)
        {
#if defined(SSE_DRAW_C)
          if(draw_dest_ad+(HOR_PIXELS_HI+SideBorderSize*4)*Draw.HorPix<Draw.limit2)
#else
          if(draw_dest_ad+(HOR_PIXELS_HI+SideBorderSize*4)*sizeof(DWORD)*Draw.HorPix<Draw.limit2)
#endif
          {
            if(Glue.CurrentScanline.Tricks&TRICK_HIRES_OVERSCAN)
              ///////////////// RENDER VIDEO /////////////////
              draw_scanline(0,HOR_PIXELS_HI/16+SideBorderSize/4,0,0);
            else
              ///////////////// RENDER VIDEO /////////////////
              draw_scanline(SideBorderSize/8,HOR_PIXELS_HI/16,SideBorderSize/8,0);
          }
        }
        else
        {
#if defined(SSE_DRAW_C)
          if(draw_dest_ad+HOR_PIXELS_HI*Draw.HorPix<Draw.limit2)
#else
          if(draw_dest_ad+HOR_PIXELS_HI*sizeof(DWORD)*Draw.HorPix<Draw.limit2)
#endif
            ///////////////// RENDER VIDEO /////////////////
            draw_scanline(0,HOR_PIXELS_HI/16,0,0);
        }
      }
      if(draw_line_off||HiresRaster||HSCROLL)
      {
        for(int i=0;i<=20;i++)// restore ST memory
          SafeLPoke(i*4+shifter_draw_pointer,Scanline[i]);
#if defined(SSE_SHIFTER_HIRESRASTERS)
        if(HiresRaster)
        {
          ZeroMemory(Scanline2,112);
          HiresRaster=0; 
        }
#endif
      }
      shifter_draw_pointer=nsdp;
    }
    else if(scan_y>=draw_first_scanline_for_border && scan_y<draw_last_scanline_for_border)
    {
      if(scan_y>=draw_first_possible_line && scan_y<draw_last_possible_line)
      {
        if(border)
        {
#if defined(SSE_DRAW_C)
          if(draw_dest_ad+(HOR_PIXELS_HI+SideBorderSize*4)*Draw.HorPix<Draw.limit2)
#else
          if(draw_dest_ad+(HOR_PIXELS_HI+SideBorderSize*4)*sizeof(DWORD)*Draw.HorPix<Draw.limit2)
#endif
            ///////////////// RENDER VIDEO /////////////////
            draw_scanline((SideBorderSize*2+640+SideBorderSize*2)/16,0,0,0); // rasters!
        }
        else
        {
#if defined(SSE_DRAW_C)
          if(draw_dest_ad+HOR_PIXELS_HI*Draw.HorPix<Draw.limit2)
#else
          if(draw_dest_ad+HOR_PIXELS_HI*sizeof(DWORD)*Draw.HorPix<Draw.limit2)
#endif
            ///////////////// RENDER VIDEO /////////////////
            draw_scanline(HOR_PIXELS_HI/16,0,0,0);
        }
      }
    }
    scanline_drawn_so_far=SideBorderSize+HOR_PIXELS_LO+SideBorderSize; // it's 320...
  }//end Monochrome

  draw_type *d;
  if(scan_y>=draw_first_possible_line && scan_y<draw_last_possible_line)
  {
    // if needed, copy from buffer
    if(Draw.Buffered)
    {  
#ifndef SSE_DRAW_C // assembly hires routines don't update draw_dest_ad
      if(screen_res&HIRES)
        draw_dest_ad+=draw_line_length; 
#endif
      int amount_drawn=(int)(draw_dest_ad-draw_temp_line_buf); 
      draw_dest_ad=draw_store_dest_ad;
      draw_type *src=draw_temp_line_buf;
      ASSERTB(Draw.VerPix);
      for(BYTE i=0;i<Draw.VerPix;i++) // # lines
      {
        d=draw_dest_ad+Draw.Pitch*i; // position
        if(Draw.LineCpyMask&(1<<i) && d+amount_drawn<Disp.VideoMemoryEnd)
        {
          for(int j=0;j<amount_drawn;j++)
            *d++=*(src+j);
        }
      }
    }
    draw_dest_ad=draw_dest_next_scanline;
    if(Draw.Buffered)
    {
      draw_store_dest_ad=draw_dest_ad;
      draw_dest_ad=draw_temp_line_buf;
      Draw.limit2=draw_temp_line_buf_lim;
    }
    else
      Draw.limit2=Disp.VideoMemoryEnd;
    Draw.limit1=draw_dest_ad;
    draw_dest_next_scanline+=draw_dest_increase_y;
  }
}


void TShifter::IncScanline() {
  scan_y++;
  HblPixelShift=0;
  left_border=LeftBorderSize;
  right_border=RightBorderSize;

  //////////////
  // LINE +16 //
  //////////////

/*  On the STE, it is possible to enlarge or shrink the left border by 16 pixels
   (low-res) on a permanent basis.
    When writing on address $FF8264, the HSCROLL register in the Shifter is modified.
    When writing on address $FF8265, the HSCROLL register in the Shifter is modified, and
    the "prefetch" flag in the Glue is changed (set if value<>0, cleared otherwise).
    Timing doesn't matter, there are two registers.
    The net effect of writing <>0 on $FF8265 then 0 on $FF8264 is that only the "prefetch" 
    flag in the Glue stays set, so the line (DE) starts 16 pixels before. 
    The Shifter does its job without any scrolling, STF-like, and we gain 16 pixels. 
    This is a Glue effect.
    When the HSCROLL register in the Shifter is <>0, it will do some preshift on the 
    prefetched words, and start outputting 16 cycles later. So if you have "prefetch" =
    0 in the Glue, and HSCROLL<>0 in the Shifter, you will have a larger left border.
    This is a Shifter effect.
*/
  if(HSCROLL)
    left_border+=16;
  if(Glue.hscroll)
    left_border-=16;
#if defined(SSE_STATS)
  if(Glue.hscroll&&!HSCROLL)
    Stats.nLinePlus16++;
#endif
}


// just taking some unimportant code out of Render for clarity (TODO: used only once)
#define   AUTO_BORDER_ADJUST  \
          if(!(border)) { \
            if(scanline_drawn_so_far<SideBorderSize) { \
              border1-=(SideBorderSize-scanline_drawn_so_far); \
              if(border1<0){ \
                picture+=border1; \
                if(screen_res==LORES) {  \
                  hscroll-=border1;  \
                  SHIFT_SDP((hscroll/16)*8); \
                  hscroll&=15; \
                }else if(screen_res==MEDRES) { \
                  hscroll-=border1*2;  \
                  SHIFT_SDP((hscroll/16)*4); \
                  hscroll&=15; \
                } \
                border1=0; \
                if(picture<0) picture=0; \
              } \
            } \
            int ta=(border1+picture+border2)-320; \
            if(ta>0) { \
              border2-=ta; \
              if(border2<0)  { \
                picture+=border2; \
                border2=0; \
                if (picture<0)  picture=0; \
              } \
            } \
            border1=border2=0; \
          }


void TShifter::Render(SHORT cycles_in,BYTE const dispatcher) {
  // cycles_in: in system cycles (~32MHz in 64bit builds)
  //ASSERT(!OPTION_C3);
  CHECK_VIDEO_RAM;
  //ASSERT(screen_res<HIRES); //asserts
  if(screen_res>=HIRES)
    return;
#ifndef NO_CRAZY_MONITOR
  //ASSERT(extended_monitor==0); //asserts
  if(extended_monitor) 
    return;
#endif
#if !defined(SSE_NO_FALCONMODE)
  if(emudetect_falcon_mode!=EMUD_FALC_MODE_OFF)
    return;
#endif
  cycles_in/=TICKS8; // convert to 8MHz
  if(OPTION_C2 && Glue.bFetchingLine && (freq_change_this_scanline||Preload))
    Glue.CheckSideOverscan();
/*  What happens here is very confusing; we render in real time, but not
    quite.
    We can't render at once because the palette could change. 
    CYCLES_FROM_HBL_TO_LEFT_BORDER_OPEN, ScanlineTiming[RENDER_CYCLE][FREQ_IDX_50]
    (84) takes care of that delay. 
    This causes all sort of trouble because our SDP is late while rendering,
    and sometimes forward!
    This pleads for the more logical (but heavier) low-level emu.
*/    
  switch(dispatcher) { // this may look impressive but it's just a bunch of hacks!
#if !defined(SSE_SHIFTER_421) // only one case, before refactoring
  case DISPATCHER_CPU:
    cycles_in+=MMU_PREFETCH_LATENCY/TICKS8;
    break;
  case DISPATCHER_SET_SHIFT_MODE:
    if(SSEOptions.RoundWriteSM)
      RoundCycles(cycles_in);
    break;
  case DISPATCHER_WRITE_VC:
    if(SSEOptions.RoundWriteVC)
      RoundCycles(cycles_in);
    break;
#endif
  case DISPATCHER_SET_PAL: // dots or not dots, that is the question
    cycles_in-=(WakeupShift-1); // it was ++ in original Steem, = Spectrum512 compatible
    if(Glue.CurrentScanline.StartCycle==Glue.ScanlineTiming[Glue.LINE_START_LIMIT][Glue.FREQ_IDX_60]
      || Glue.CurrentScanline.StartCycle==Glue.ScanlineTiming[Glue.DE_ON][Glue.FREQ_IDX_60])
      cycles_in+=4; // 60hz line with border, HSCROLL or not, still quite hacky!
#if defined(SSE_VID_BORDERS)
    if((SideBorderSize==VERY_LARGE_BORDER_SIDE)
      &&(Glue.CurrentScanline.Tricks&(TRICK_LINE_PLUS_20|TRICK_LINE_PLUS_26)))
      cycles_in-=2; // reason unknow! but I changed hbl timing, it's since v400
#endif
    break;
#if !defined(SSE_SHIFTER_421)
  case DISPATCHER_LINEWIDTH:
  case DISPATCHER_DSTE:
  case DISPATCHER_DEBUGGER:
  //case DISPATCHER_SET_SYNC:
    break;
#endif
  }//sw
#if defined(SSE_SHIFTER_421)
  cycles_in+=RenderingDelay[dispatcher];
#endif
  int pixels_in=cycles_in
    -(Glue.ScanlineTiming[Glue.RENDER_CYCLE][Glue.FREQ_IDX_50]/TICKS8-SideBorderSize);
  if(pixels_in>SideBorderSize+320+SideBorderSize) 
    pixels_in=SideBorderSize+320+SideBorderSize; 
  // this is the most tricky part of our hack for large border/no shift on left off:
  int pixels_in0=pixels_in;
#if defined(SSE_VID_BORDERS)
  if(SideBorderSize==VERY_LARGE_BORDER_SIDE && pixels_in>0)
    if(Glue.CurrentScanline.Tricks&(TRICK_LINE_PLUS_20|TRICK_LINE_PLUS_26)) // only for left off, not too early?
      pixels_in+=4;
#endif
  if(pixels_in>=0) // time to render?
  {
    if(pixels_in>416)
      pixels_in=pixels_in0;    
    if(Glue.bFetchingLine)
    {
      int border1=0,border2=0,picture=0,hscroll=0; // parameters for ASM routine
      int picture_left_edge=left_border; // 0, BS, BS-4, BS-16 (...)
      //last pixel from extreme left to draw of picture
      int picture_right_edge=SideBorderSize+320;
      if(border)
        picture_right_edge+=SideBorderSize-right_border;
      if(pixels_in>picture_left_edge)
      { //might be some picture to draw = fetching RAM
        if(scanline_drawn_so_far>picture_left_edge)
        {
          picture=pixels_in-scanline_drawn_so_far;
          if(picture>picture_right_edge-scanline_drawn_so_far)
            picture=picture_right_edge-scanline_drawn_so_far;
        }
        else
        {
          picture=pixels_in-picture_left_edge;
          if(picture>picture_right_edge-picture_left_edge)
            picture=picture_right_edge-picture_left_edge;
        }
        if(picture<0)
          picture=0;
      }
      if(scanline_drawn_so_far<left_border)
      {
        if(pixels_in>left_border)
          border1=left_border-scanline_drawn_so_far;
        else
          border1=pixels_in-scanline_drawn_so_far; // we're not yet at end of border
        if(border1<0) 
          border1=0;
      }
      border2=pixels_in-scanline_drawn_so_far-border1-picture;
      if(border2<0) 
        border2=0;
      // We Were distorter: must do this check later than at "left off"
      if(!left_border && HSCROLL 
#if defined(SSE_VID_BORDERS)
        && SideBorderSize!=VERY_LARGE_BORDER_SIDE
#endif
        && screen_res==LORES && !scanline_drawn_so_far && shifter_tick8>15)
        SHIFT_SDP(8), shifter_tick8-=16;
      int old_shifter_tick8=shifter_tick8;
      shifter_tick8+=(SHORT)picture;
      MEM_ADDRESS nsdp=shifter_draw_pointer;
      if(OPTION_C2&&shifter_draw_pointer>=himem) // no RAM
        shifter_draw_pointer=0+scan_y*160; // just a bad approximation of the effect
      // On lines -2, don't fetch the last 2 bytes as if it was a 160byte line.
      // Fixes screen #2 of the venerable B.I.G. Demo.
      // The MEDRES stuff is hypothetical
      if(Glue.CurrentScanline.Tricks&TRICK_LINE_MINUS_2)
      {
        int pixels_to_skip=(screen_res>LORES) ? (4*2) : (8*2);
        if(picture>=pixels_to_skip)
          picture-=pixels_to_skip,border2+=pixels_to_skip;
      }
      if(screen_res==LORES)
      {
        hscroll=(old_shifter_tick8&15);
        nsdp-=(old_shifter_tick8/16)*8;
        nsdp+=(shifter_tick8/16)*8; // is sdp forward at end of line?
/*  This is where we do the actual shift for those rare programs using the
    overscan 4bit hardscroll trick.
    Notice it is quite simple, and also very efficient because it just uses 
    the hscroll parameter of the drawing routine.
    hscroll>15 is handled further.
*/
        if(Glue.CurrentScanline.Tricks&TRICK_4BIT_SCROLL)
        {
          hscroll-=HblPixelShift;
          if(hscroll<0)
          {
            if(picture>-hscroll)
            {
              picture+=hscroll,border1-=hscroll;
              hscroll=0;
            }
            else if(!picture) // phew
              hscroll+=HblPixelShift;
          }
        }
      }
      else if(screen_res==MEDRES)
      {
        hscroll=((old_shifter_tick8*2)&15);
        if(HblStartingHscroll&1) // not useful for anything known yet!
        { 
          hscroll++; // restore precision
          HblStartingHscroll=0; // only once/scanline (?)
        }
        nsdp-=(old_shifter_tick8/8)*4;
        nsdp+=(shifter_tick8/8)*4;
      }
      if(draw_lock) // draw_lock is set true in draw_begin(), false in draw_end()
      {
        // real lines
        if(scan_y>=draw_first_possible_line && scan_y<draw_last_possible_line)
        {
          // actually draw it
          if(picture_left_edge<0) 
            picture+=picture_left_edge;
          AUTO_BORDER_ADJUST; // hack borders if necessary
          DEBUG_ONLY( SHIFT_SDP(debug_screen_shift); );
          if(hscroll>=16) // convert excess hscroll in SDP shift
          {
            SHIFT_SDP((SHIFTER_RASTER_PREFETCH_TIMING/(TICKS8*2))*(hscroll/16));
            hscroll-=16*(hscroll/16);
          }
#if defined(SSE_DRAW_C)
          if(draw_dest_ad+(border1+border2)*DISPLAY_SIZE+(picture-hscroll)*Draw.HorPix<Draw.limit2)
#else
          if(draw_dest_ad+(border1+picture+border2-hscroll)*sizeof(DWORD)*Draw.HorPix<Draw.limit2)
#endif
            ///////////////// RENDER VIDEO /////////////////
            draw_scanline(border1,picture,border2,hscroll);
        }
      }
      shifter_draw_pointer=nsdp;
    }
    // overscan lines = a big "left border"
    else if(scan_y>=draw_first_scanline_for_border && scan_y<draw_last_scanline_for_border)
    {
      // top lines of 60hz screens are black in our big window, it's that or resizing the
      // window, which has its drawbacks
      // we don't do the bottom here but in event_vbl_interrupt() which takes care of other cases
      LONG savepal0=PCpal[0];
      if(border && Glue.PreviousVideoFreq==NTSC_HZ
        && (scan_y<draw_first_scanline_for_border60||scan_y>=draw_last_scanline_for_border60))
        PCpal[0]=0;
      int border1; // the only var. sent to draw_scanline
      int left_visible_edge,right_visible_edge;
      // Borders on
      if(border)
      {
        left_visible_edge=0;
        right_visible_edge=SideBorderSize+320+SideBorderSize;
      }
      // No, only the part between the borders
      else
      {
        left_visible_edge=SideBorderSize;
        right_visible_edge=SideBorderSize+320;
      }
      border1=pixels_in-((scanline_drawn_so_far<=left_visible_edge)
        ? left_visible_edge : scanline_drawn_so_far);
      if(border1<0)
        border1=0;
      else if(border1> (right_visible_edge - left_visible_edge))
        border1=(right_visible_edge - left_visible_edge);
      if(scan_y>=draw_first_possible_line && scan_y<draw_last_possible_line)
      {
        ///////////////// RENDER VIDEO /////////////////
#if defined(SSE_DRAW_C)
        if(draw_dest_ad+(border1)*DISPLAY_SIZE<Draw.limit2)
#else
        if(draw_dest_ad+(border1)*sizeof(DWORD)*Draw.HorPix<Draw.limit2)
#endif
          draw_scanline(border1,0,0,0);
      }
      PCpal[0]=savepal0;
    }
    scanline_drawn_so_far=pixels_in;
  }//if(pixels_in>=0)
}


#undef AUTO_BORDER_ADJUST

#if !defined(SSE_SHIFTER_421)
void TShifter::RoundCycles(SHORT& cycles_in) { // hacky but effective? // optional now
#if defined(SSE_420R5) // cycles are 8MHz
  cycles_in-=Glue.ScanlineTiming[Glue.RENDER_CYCLE][Glue.FREQ_IDX_50]/TICKS8;
  short shift=(16>>ShiftMode);
  if(Glue.hscroll && !HSCROLL) 
    cycles_in+=shift;
  cycles_in+=SHIFTER_RASTER_PREFETCH_TIMING/TICKS8;
  cycles_in&=-SHIFTER_RASTER_PREFETCH_TIMING/TICKS8;
  cycles_in+=Glue.ScanlineTiming[Glue.RENDER_CYCLE][Glue.FREQ_IDX_50]/TICKS8;
  if(Glue.hscroll && !HSCROLL) 
    cycles_in-=shift;
#else
  cycles_in-=Glue.ScanlineTiming[Glue.RENDER_CYCLE][Glue.FREQ_IDX_50];
  short shift=(16>>ShiftMode)*TICKS8;
  if(Glue.hscroll && !HSCROLL) 
    cycles_in+=shift;
  cycles_in+=SHIFTER_RASTER_PREFETCH_TIMING;
  cycles_in&=-SHIFTER_RASTER_PREFETCH_TIMING;
  cycles_in+=Glue.ScanlineTiming[Glue.RENDER_CYCLE][Glue.FREQ_IDX_50];
  if(Glue.hscroll && !HSCROLL) 
    cycles_in-=shift;
#endif
}
#endif

void CALLBACK TShifter::SetPal(int const n, WORD NewPal) {
  //ASSERT(n<PAL_SIZE);
#if defined(SSE_GUI_EMUCONTROL)
  if(SSEOptions.BlockPal)
    return;
#endif
  NewPal&=(IS_STF) ? 0x0777 : 0x0FFF; // valid bits
#if defined(SSE_STATS)
  Stats.nPal++;
#endif
  if(flashlight_flag&&MONO_MONITOR&&n==0)
    NewPal^=1;
  if(STpal[n]!=NewPal)
  {
#if defined(SSE_STATS)
    if(NewPal&0x888)
    {
      Stats.nExtendedPal++;
      if(LITTLE_PC>rom_addr)
        Stats.nExtendedPalT++;
    }
#endif
    SHORT CyclesIn=LINECYCLES;
    if(!OPTION_C3 && draw_lock && CyclesIn>MMU_PREFETCH_LATENCY+SHIFTER_RASTER_PREFETCH_TIMING)
      Shifter.Render(CyclesIn,DISPATCHER_SET_PAL);
#if defined(SSE_SHIFTER_HIRESRASTERS)
/*  v3.8.0
    Record when palette is changed during display, so that we apply effect
    at rendering time (there's no real-time rendering for HIRES).
    Limitations:
    -Our trick uses video RAM, so this limits the field to real picture. 
    Since the border is always black, maybe it isn't a problem.
    -Resolution is byte, not bit/cycle.
    Seems to be enough for Time Slices
*/
    if(screen_res==HIRES && OPTION_C2 && n==0 && Glue.bFetchingLine)
    {
      int time_to_first_pixel=28*TICKS8-6*TICKS8
        +Glue.ScanlineTiming[TGlue::LINE_START_LIMIT][TGlue::FREQ_IDX_71];
      int cycle=CyclesIn-time_to_first_pixel;
      if(cycle>=0&&cycle<160*TICKS8) // only during Shifter DE
      {
        BYTE byte=(BYTE)((cycle/TICKS8)/2); // HIRES: 4 pixels/cycle
        for(int i=Shifter.HiresRaster;i<=byte;i++) // from previous pixel chunk to now
          Shifter.Scanline2[i]=(NewPal&1) ? 0xFF : 0x00; // and not the reverse...
        Shifter.HiresRaster=byte; //record where we are
      }
    }
#endif
    PAL_DPEEK(n<<1)=STpal[n]=NewPal;
#if defined(SSE_VID_STVL_UPD)
    Stvl.ST_pal[n]=NewPal;
    Stvl.mono_col=STpal[0]&BIT_0; // extra register in Shifter
#endif
    if(!flashlight_flag&&!draw_line_off DEBUG_ONLY(&&!debug_cycle_colours))
      palette_convert(n);
#if defined(SSE_GEM_CONTROL_PANEL)
    if(OptionBox.Page==PAGE_GEM_CP && OptionBox.IsVisible())
      OptionBox.UpdateColour(n);
#endif
  }
}

#undef LOGSECTION


///////////
// SOUND //
///////////

#define LOGSECTION LOGSECTION_SOUND

void TShifter::SoundSetMode(BYTE const new_mode) {
/*   FF8920 0000 0000 m000 00rr RW Sound Mode Control
     rr:
     00   6258 Hz sample rate (reset state)
     01  12517 Hz sample rate
     10  25033 Hz sample rate
     11  50066 Hz sample rate
     m:
     0	Stereo Mode (reset state)
     1 Mono Mode
*/
  shifter_sound_mode=new_mode&0x83; // valid bits
  SteSoundFreq=SteSoundModeToFreq[shifter_sound_mode&3];
  TRACE_LOG("DMA sound mode %X freq %d\n",new_mode,SteSoundFreq);
  SampleRate=(double)sound_freq; // see 3rdparty/dsp
}


void TShifter::SoundGetLastSample(WORD *pw1,WORD *pw2) {
  if(shifter_sound_mode&SOUNDMONO)
  {  // ST plays HIBYTE, LOBYTE, so last sample is LOBYTE
    if(RENDER_SIGNED_SAMPLES)
      *pw1=(WORD)(SteSoundLastWord.d8[LO]);
    else
      *pw1=(WORD)((SteSoundLastWord.d8[LO])<<6);
    *pw2=*pw1; // play the same in both channels, or ignored in when sound_num_channels==1
  }
  else
  {
    if(sound_num_channels==1) // mono PC output
    {
      //average the channels out
      if(RENDER_SIGNED_SAMPLES)
        *pw1=(BYTE)(((char)(SteSoundLastWord.d8[LO])+(char)(SteSoundLastWord.d8[HI]))/2);
      else
        *pw1=(WORD)(((SteSoundLastWord.d8[LO])+(SteSoundLastWord.d8[HI]))<<5);
      *pw2=0; // skipped
    }
    else
    {
      if(RENDER_SIGNED_SAMPLES)
      {
        *pw1=(WORD)(SteSoundLastWord.d8[HI]);
        *pw2=(WORD)(SteSoundLastWord.d8[LO]);
      }
      else
      {
        *pw1=(WORD)(SteSoundLastWord.d8[HI]<<6);
        *pw2=(WORD)(SteSoundLastWord.d8[LO]<<6);
      }
    }
  }
}


// render digital audio
void TShifter::SoundPlay() { // only called by event_scanline_sub()
  bool bSTeMono=((shifter_sound_mode&SOUNDMONO)!=0);
  //we want to play a/b samples, where a is the DMA sound frequency
  //and b is the number of scanlines a second
  int left_vol_top_val=Microwire.top_val_l,right_vol_top_val=Microwire.top_val_r;
  //this a/b is the same as SteSoundFreq*CyclesPerScanline/8million
  if(bSTeMono)
  {  //play half as many words
    SteSoundSamplesCountdown+=SteSoundFreq*CyclesPerScanlineAtStartOfVbl/2;
    left_vol_top_val=(Microwire.top_val_l >> 1)+(Microwire.top_val_r >> 1);
    right_vol_top_val=left_vol_top_val;
  }
  else //stereo, 1 word per sample
    SteSoundSamplesCountdown+=SteSoundFreq*CyclesPerScanlineAtStartOfVbl;
  bool vol_change_l=(left_vol_top_val<128),vol_change_r=(right_vol_top_val<128);
  while(SteSoundSamplesCountdown>=0)
  {  //play word from buffer
    if(Shifter.SoundFifoIdx>0)
    {
      SteSoundLastWord.d16=Shifter.SoundFifo[0];
      for(int i=0;i<DIGITAL_SOUND_BUFFER_LEN-1;i++)
        Shifter.SoundFifo[i]=Shifter.SoundFifo[i+1];
      Shifter.SoundFifoIdx--;
      if(vol_change_l)
      {
        int b1=(signed char)(SteSoundLastWord.d8[HI]);
        b1*=left_vol_top_val;
        b1/=128;
        SteSoundLastWord.d8[LO]=LOBYTE(b1);
      }
      if(vol_change_r)
      {
        int b2=(signed char)(SteSoundLastWord.d8[LO]);
        b2*=right_vol_top_val;
        b2/=128;
        SteSoundLastWord.d8[HI]=LOBYTE(b2);
      }
      if(!RENDER_SIGNED_SAMPLES)
        SteSoundLastWord.d16^=(WORD)((0x80<<8)|0x80); // unsign
    }
    SteSoundOutputCountdown+=sound_freq;
    WORD w1,w2;
    if(bSTeMono)
    {       //mono, play half as many words
      if(RENDER_SIGNED_SAMPLES)
      {
        w1=(WORD)(SteSoundLastWord.d8[HI]);
        w2=(WORD)(SteSoundLastWord.d8[LO]);
      }
      else
      {
        w1=(WORD)(SteSoundLastWord.d8[HI]<<6);
        w2=(WORD)(SteSoundLastWord.d8[LO]<<6);
      }
      // SteSoundChannelBuf always stereo, so put each mono sample in twice
      while(SteSoundOutputCountdown>=0)
      {
        if(SteSoundChannelBufIdx>=SteSoundChannelBufLen)
          break;
        SteSoundChannelBuf[SteSoundChannelBufIdx++]=w1;
        SteSoundChannelBuf[SteSoundChannelBufIdx++]=w1;
        SteSoundOutputCountdown-=SteSoundFreq;
      }
      SteSoundOutputCountdown+=sound_freq;
      while(SteSoundOutputCountdown>=0)
      {
        if(SteSoundChannelBufIdx>=SteSoundChannelBufLen)
          break;
        SteSoundChannelBuf[SteSoundChannelBufIdx++]=w2;
        SteSoundChannelBuf[SteSoundChannelBufIdx++]=w2;
        SteSoundOutputCountdown-=SteSoundFreq;
      }
    }
    else
    {//stereo , 1 word per sample
/*
 Stereo Samples have to be organized wordwise like 
 Lowbyte -> right channel
 Hibyte  -> left channel
*/
      if(sound_num_channels==1)
      {  //average the channels out
        if(RENDER_SIGNED_SAMPLES) // avoid clipping (Rebirth)
          w1=(BYTE)(((char)SteSoundLastWord.d8[LO]+(char)SteSoundLastWord.d8[HI])/2);
        else
          w1=(WORD)(((SteSoundLastWord.d8[LO])+(SteSoundLastWord.d8[HI]))<<5);
        w2=0; // skipped at mixdown...
      }
      else if(RENDER_SIGNED_SAMPLES)
      {
        w1=(WORD)(SteSoundLastWord.d8[HI]);
        w2=(WORD)(SteSoundLastWord.d8[LO]);
      }
      else
      {
        w1=(WORD)(SteSoundLastWord.d8[HI]<<6);
        w2=(WORD)(SteSoundLastWord.d8[LO]<<6);
      }
      while(SteSoundOutputCountdown>=0)
      {
        if(SteSoundChannelBufIdx>=SteSoundChannelBufLen) 
          break;
        SteSoundChannelBuf[SteSoundChannelBufIdx++]=w1;
        SteSoundChannelBuf[SteSoundChannelBufIdx++]=w2;
        SteSoundOutputCountdown-=SteSoundFreq;
      }
    }
    SteSoundSamplesCountdown-=nSysCyclesPerSecond; 
  }//wend
#if defined(SSE_VID_STVL1)
  if(OPTION_C3 && SSEConfig.Stvl>=TStvl::VER_STESND)
    Stvl.sreq=(SoundFifoIdx<DIGITAL_SOUND_BUFFER_LEN && (Mmu.SoundControl&TMmu::SOUNDPLAY));
#endif
}

#undef LOGSECTION
