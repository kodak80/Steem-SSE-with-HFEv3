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
FILE: draw.cpp
DESCRIPTION: Routines to handle Steem's video output. draw_routines_init
initialises the system, draw_begin locks output, draw_end unlocks output
and draw_blit blits the frame to the PC display. The orders are given here
and executed in display.cpp (using DirectDraw, D3D...)
Lines are drawn by routines in shifter.cpp and draw_c.cpp
(or assembly routines according to the build: currently _DEBUG builds use
assembly modules because the compîler doesn't optimise routines but NDEBUG
builds use C routines of draw_c.cpp)
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <computer.h>
#include <draw.h>
#include <display.h>
#include <osd.h>
#if defined(SSE_DEBUGGER_FRAME_REPORT)
#include <debug_framereport.h>
#endif
#if defined(SSE_VID_STVL1)
#include <interface_stvl.h>
#endif

// draw_type is defined in conditions.h
#if defined(SSE_420R4)
draw_type *draw_temp_line_buf=NULL,*draw_temp_line_buf_lim=NULL;
#else
draw_type draw_temp_line_buf[DRAW_TEMP_LINE_BUF_LEN];
draw_type *draw_temp_line_buf_lim=(draw_type*)&draw_temp_line_buf+DRAW_TEMP_LINE_BUF_LEN;
#endif
draw_type *draw_dest_ad=NULL,*draw_dest_next_scanline=NULL; // C
draw_type *draw_store_dest_ad=NULL;
#if defined(SSE_DEFAULTS_421)
int draw_win_mode[3];
#else
int draw_win_mode[3]={0,0,0};
#endif
const BYTE TopScanlines[4]={GLU_PAL_TOPSCANLINES,GLU_NTSC_TOPSCANLINES,GLU_MONO_TOPSCANLINES,16};
BYTE bad_drawing=0;
#if defined(SSE_DEFAULTS_421)
BYTE border,border_last_chosen;
#else
//BYTE border=1,border_last_chosen=1;
BYTE border=0,border_last_chosen=0;
#endif
BYTE draw_grille_black=6;
#if defined(SSE_VID_D3D)
const
#endif
BYTE draw_fs_fx=DFSFX_NONE;
BYTE draw_stretch=0,draw_stretch_fs=1;
bool draw_line_off=false;

TDraw Draw;
RECT draw_blit_source_rect;
LONG *PCpal; // C
draw_type *draw_mem;
#if !defined(SSE_420R4)
LPPIXELWISESCANPROC jump_draw_scanline[5][3]; // p res
#endif
LPPIXELWISESCANPROC draw_scanline_lowres,draw_scanline_medres,draw_scanline_hires;
LPPIXELWISESCANPROC draw_scanline;
#if defined(SSE_VID_DD) && !defined(SSE_VID_DD_MISC)
HWND ClipWin;
#endif
#if !defined(SSE_420R4)
void ASMCALL draw_scanline_dont(int,int,int,int) {}
#endif
int draw_line_length; // C
#if defined(SSE_VID_STVL1)
#if defined(SSE_DRAW_C)
int &draw_line_pitch_dw=draw_line_length;
#else
int draw_line_pitch_dw;
#endif
#endif
int draw_dest_increase_y;
int scanline_drawn_so_far;
short draw_first_scanline_for_border,draw_last_scanline_for_border; //calculated from TopBorderSize, BottomBorderSize and res_vertical_scale
short draw_first_scanline_for_border60,draw_last_scanline_for_border60;
short draw_first_possible_line,draw_last_possible_line;
short res_vertical_scale;
short left_border,right_border;
bool draw_lock;
bool draw_buffer_complex_scanlines;

#ifdef UNIX
int x_draw_surround_count=4;
bool prefer_res_640_400=false,using_res_640_400=false;
#endif

#if defined(SSE_VID_DD)
#if defined(SSE_DEFAULTS_421)
BYTE draw_fs_blit_mode;
#else
BYTE draw_fs_blit_mode=1;
#endif
BYTE draw_fs_topgap;
bool prefer_res_640_400=false,using_res_640_400=false;
WORD tested_pc_hz[NPC_HZ_CHOICES]={0,0,0,0};
WORD real_pc_hz[NPC_HZ_CHOICES]={0,0,0,0};
int prefer_pc_hz[NPC_HZ_CHOICES]={0,0,0,0};
#endif


#define LOGSECTION LOGSECTION_VIDEO_RENDERING


/////////////
// Borders //
/////////////

POINT WinSizeBorder[4][5];
BYTE SideBorderSize=ORIGINAL_BORDER_SIDE; // 32
BYTE LeftBorderSize=ORIGINAL_BORDER_SIDE;
BYTE RightBorderSize=ORIGINAL_BORDER_SIDE;
BYTE SideBorderSizeWin=ORIGINAL_BORDER_SIDE; // 32
BYTE BottomBorderSize=ORIGINAL_BORDER_BOTTOM; // 40
BYTE TopBorderSize=ORIGINAL_BORDER_TOP; // 30


void update_CanUse_400(int cw,int ch) {
  int xtra=MENUHEIGHT;
#ifdef WIN32
  xtra+=GuiSM.m_statusbar_height;
#endif
  if(FullScreen)
    CanUse_400=true;
  else if(border)
  {
    CanUse_400=(cw>=640+4*SideBorderSizeWin && ch>=400+2*TopBorderSize+2*BottomBorderSize+xtra);
    TRACE_LOG("CanUse_400 %d cw %d %d ch %d %d\n",CanUse_400,cw,(640+4*SideBorderSizeWin),ch,(xtra+400+2*(TopBorderSize+BottomBorderSize)));
  }
  else 
  {
    CanUse_400=(cw>=640 && ch>=400+xtra);
    TRACE_LOG("CanUse_400 %d cw %d ch %d\n",CanUse_400,cw,ch);
  }
}


void update_winsize() {
  // just AR, TODO
  if(OPTION_LOCK_AR) // idea: same size in all res
  {
    WinSize[MEDRES][0].x=WinSize[LORES][0].x;
    WinSize[MEDRES][2]=WinSize[LORES][2];
  }
  else
  {
    WinSize[MEDRES][0].x=WinSize[MEDRES][1].x;
    WinSize[MEDRES][2].x=WinSize[MEDRES][3].x;
    WinSize[MEDRES][2].y=WinSize[MEDRES][1].y;
  }
}


void ChangeBorderSize(int size) {
  if(size<0)
    size=0;
  else if(size>BIGGEST_BORDER)
    size=BIGGEST_BORDER;
#if defined(SSE_EMU_THREAD)
#ifdef SSE421R1
  bool oldSuspendRendering=SuspendRendering;
  if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
  {    
    VideoLock.Lock();
    SuspendRendering=true;
  }
#else
  bool oldSuspendRendering=SuspendRendering;
  SuspendRendering=true;
  if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
    VideoLock.Lock();
#endif
#endif
  SSEConfig.Border60Hz=0;
  switch(size) {
  case 0: //no border, still need the same figures as normal border
  case 1:
    SideBorderSizeWin=SideBorderSize=ORIGINAL_BORDER_SIDE; // 32
    TopBorderSize=ORIGINAL_BORDER_TOP; // 30
    BottomBorderSize=ORIGINAL_BORDER_BOTTOM; // 40
    // smaller bottom border for hires, only if no overscan possible
    if(MONO_MONITOR && !OPTION_HWOVERSCAN && !SSEOptions.VideoLogicEmu)
      BottomBorderSize-=BORDER71_MINUS_TOP; // 32
    else if(SSEConfig.TosLanguage==0) // US: no black bands
    {
      TopBorderSize-=BORDER60_MINUS_TOP; // 12
      BottomBorderSize-=BORDER60_MINUS_BOTTOM; // 16
      SSEConfig.Border60Hz=1;
    }
    break;
  case 2:
    SideBorderSize=VERY_LARGE_BORDER_SIDE; // 50 (52+48)
    SideBorderSizeWin=LARGE_BORDER_SIDE; // 46 (48+44)
    TopBorderSize=ORIGINAL_BORDER_TOP; 
    BottomBorderSize=LARGE_BORDER_BOTTOM; // 45
    break;
  case BIGGEST_BORDER:
    SideBorderSize=VERY_LARGE_BORDER_SIDE;
    SideBorderSizeWin=VERY_LARGE_BORDER_SIDE;
    TopBorderSize=BIG_BORDER_TOP; // 38
    BottomBorderSize=VERY_LARGE_BORDER_BOTTOM; // 45
    break;
  }//sw
  LeftBorderSize=RightBorderSize=SideBorderSize;
/*  With large borders, we don't shift scanlines by 4 pixels when the left border
    is removed. We must shift other scanlines in compensation, which is
    correct emulation. On the ST, the left border is larger than the right
    border. By directly changing left_border and right_border, we can get
    the right timing for palette effects.*/
  if(size==2)
    LeftBorderSize+=4,RightBorderSize-=4;
  int i,j;
  for(i=0;i<4;i++) 
  {
    for(j=0;j<5;j++) 
    {
      int fx=WinSize[i][j].x/HOR_PIXELS_LO;
      int fy=WinSize[i][j].y/VER_PIXELS_LO;
      WinSizeBorder[i][j]=WinSize[i][j];
      WinSizeBorder[i][j].x+=SideBorderSizeWin*2*fx;
      WinSizeBorder[i][j].y+=(TopBorderSize+BottomBorderSize)*fy;
      //TRACE_LOG("WinSizeBorder[%d][%d] x %d y% d",i,j,WinSizeBorder[i][j].x,WinSizeBorder[i][j].y);
      //TRACE_LOG(" WinSize %dx%d, * %dx%d\n",WinSize[i][j].x,WinSize[i][j].y,fx,fy);
    }//nxt j
  }//nxt i
  draw_first_scanline_for_border=res_vertical_scale*(-TopBorderSize);
  draw_last_scanline_for_border=shifter_y+res_vertical_scale*(BottomBorderSize);
  TRACE_LOG("ChangeBorderSize(%d) side %d side win %d bottom %d\n",
            size,SideBorderSize,SideBorderSizeWin,BottomBorderSize);
#if defined(SSE_VID_STVL1)
  StvlUpdate();
#endif
  StemWinResize();
  Draw.MarshalParameters();
#ifdef WIN32
  RECT rc;
  GetClientRect(StemWin,&rc);
  update_CanUse_400(rc.right-rc.left,rc.bottom-rc.top); // get it right on startup
#endif
#if defined(SSE_EMU_THREAD)
  SuspendRendering=oldSuspendRendering;
  VideoLock.Unlock();
#endif
  if(runstate==RUNSTATE_STOPPED)
  {
    draw_begin(); // Lock() will update draw_line_length
    draw_end();
  }
}


void draw_begin() { //called at each frame, it calls Lock()
#if defined(SSE_VID_TRACE_SIZE)
  TRACE("draw_begin draw_lock%d SuspendRendering%d VideoLock.blocked%d\n",draw_lock,SuspendRendering ,VideoLock.blocked);
#endif
  //TRACE_OSD("frame %d",FRAME);
  if(draw_lock) 
    return;
#if defined(SSE_EMU_THREAD)
  if(SuspendRendering || VideoLock.blocked)
    return;
#elif defined(SSE_EMU_THREAD)
  if(SuspendRendering)
    return;
#endif
#if defined(SSE_VID_DD)
  if(FullScreen&&draw_grille_black>0)
    Disp.DrawFullScreenLetterbox();
#endif
  if(Disp.Lock()!=DD_OK)
    return;
#ifndef SSE_NO_OSD
  osd_draw_begin();
#endif
  draw_lock=true;
#if defined(SSE_VID_DD)
  if(FullScreen&&!OPTION_FAKE_FULLSCREEN&&draw_fs_blit_mode==DFSM_FLIP)
  {
    int offset=Disp.LetterBoxRectangle.top*draw_line_length+Disp.LetterBoxRectangle.left;
    draw_mem+=offset;
#if defined(SSE_DRAW_C)
    offset*=4;
#endif
    Disp.VideoMemorySize-=offset;
  }
#endif
  draw_dest_ad=draw_mem;
  draw_dest_next_scanline=draw_dest_ad+draw_dest_increase_y;
#ifdef SSE421R2 // fix screenshot on stopped emu with OSD
  if(draw_grille_black>0 && !Glue.m_Status.stop_emu)
  {
    // simpler to clear full buffer, no question
    ZeroMemory(draw_dest_ad,Disp.VideoMemorySize);
    draw_grille_black--;
  }
#else
  if(draw_grille_black>0) 
  {
    // simpler to clear full buffer, no question
    ZeroMemory(draw_dest_ad,Disp.VideoMemorySize);
    draw_grille_black--;
  }
#endif
#ifdef UNIX
  if(x_draw_surround_count>0) {
    Disp.Surround();
    x_draw_surround_count--;
  }
#endif
#ifdef DEBUG_BUILD
  if(debug_cycle_colours) 
  {
    debug_cycle_colours--;
    if(debug_cycle_colours==0) 
    {
      for(int i=0;i<PAL_SIZE;i++) 
      {
        PCpal[i]=0x88888888+(rand()%0x77777777);
      }
      debug_cycle_colours=CYCLE_COL_SPEED;
    }
  }
#endif
  if(Draw.Buffered)
  {
    draw_store_dest_ad=draw_dest_ad;
    draw_dest_ad=draw_temp_line_buf;
  }
  else
    Draw.limit2=Disp.VideoMemoryEnd;
  Draw.limit1=draw_dest_ad;
}


#if !defined(SSE_NO_FALCONMODE)
void draw_set_jumps_and_source_fm(bool big_draw) {
  draw_scanline=emudetect_falcon_draw_scanline;
  draw_dest_increase_y=draw_line_length;
  int ox=0,oy=0,ow,oh;
  if(big_draw) 
  {
    ow=640;oh=400;
    if(border) 
    {
      ow=640+SideBorderSizeWin*2+SideBorderSizeWin*2;
      oh=400+TopBorderSize*2+BottomBorderSize*2;
#if defined(SSE_VID_DD)
      if(FullScreen) 
        ox=(800-ow)/2,oy=(600-oh)/2;
#endif
    }
#if defined(SSE_VID_DD)
    else if(FullScreen && using_res_640_400==0)
      oy=(480-oh)/2;
#endif
    if(emudetect_falcon_mode_size==1)
      draw_dest_increase_y*=2; // Have to draw double height
  }
  else
  {
    ow=320*emudetect_falcon_mode_size,oh=200*emudetect_falcon_mode_size;
    if(border) 
    {
      // We double the size of borders too to keep the aspect ratio of the screen the same
      ow+=SideBorderSizeWin*2*emudetect_falcon_mode_size;
      oh+=(TopBorderSize+BottomBorderSize)*emudetect_falcon_mode_size;
    }
  }
  draw_blit_source_rect.left=ox;
  draw_blit_source_rect.right=ox+ow;
  draw_blit_source_rect.top=oy;
  draw_blit_source_rect.bottom=oy+oh;
}
#endif


void draw_end() {
  if(!draw_lock)
    return;
#ifndef ONEGAME
  bool draw_osd=true;
  if(DoSaveScreenShot||slow_motion||Glue.m_Status.stop_emu&&OPTION_NO_OSD_ON_STOP)
    draw_osd=false;
#if 0//def DEBUG_BUILD
  if(runstate!=RUNSTATE_RUNNING)
    draw_osd=false;
#endif
#ifndef SSE_NO_OSD
  OsdControl.bOsdDrawn=false;
  if(draw_osd)
    osd_draw();
#endif
#endif
  Disp.Unlock();
#ifdef SHOW_WAVEFORM
  Disp.DrawWaveform();
#endif
#ifndef SSE_NO_OSD
  osd_draw_end(); // resets osd_draw function pointers
#endif
  //WIN_ONLY( draw_store_dest_ad=NULL; )
  draw_lock=false;
  if(DoSaveScreenShot) 
  {
    Disp.SaveScreenShot();
    DoSaveScreenShot&=~1;
  }
}


bool draw_blit(
#if defined(SSE_VID_BFI) // only in D3D builds
  BOOL erase/*=FALSE*/
#endif
  )
{
  bool ok=false;
#if defined(SSE_EMU_THREAD) 
  if(SuspendRendering || VideoLock.blocked)
    return ok;
#endif
  // we blit the unlocked backsurface
  if((!draw_lock||OPTION_3BUFFER_WIN&&!FullScreen) && !bAppMinimized)
  {
#if defined(SSE_VID_BFI)
    ok=Disp.Blit(erase);
#else
    ok=Disp.Blit();
#endif
    // Check for screen change right after the blit so that we
    //  don't erase the frame (fullscreen) just before it's rendered
    if(runstate==RUNSTATE_RUNNING)
    {
      if(video_mixed_output>0) 
      {
        video_mixed_output--;
        if(video_mixed_output==VIDEO_MIXED_ESTABLISHED)
        {
          TRACE_LOG("F%d mixed output %d\n",FRAME,video_mixed_output);
          init_screen();
          res_change();
        }
        else if(video_mixed_output==0) 
        {
          TRACE_LOG("F%d mixed output %d\n",FRAME,video_mixed_output);
          init_screen();
          if(screen_res==LORES || SCANLINES_INTERPOLATED)
            res_change();
          ScreenResAtStartOfVbl=screen_res;
        }
      }
      else if(screen_res!=ScreenResAtStartOfVbl) 
      {
        TRACE_LOG("(R%d->R%d) ",ScreenResAtStartOfVbl,screen_res);
        init_screen();
        res_change();
        ScreenResAtStartOfVbl=screen_res;
      }
    }//runstate
  }
  return ok;
}


void draw(bool draw_osd) {
  if(FullScreen)
    return;
  TRACE_LOG("draw(%d)\n",draw_osd);
  // This is called by init, load... not for actual emulation except extended monitor
  // It draws the screen in one time, assuming no overscan
#if defined(SSE_EMU_THREAD)
  if(OPTION_EMUTHREAD && runstate!=RUNSTATE_STOPPED && !(extended_monitor&&(bad_drawing&6)))
    return;
#endif
  short save_scan_y=scan_y;
  MEM_ADDRESS save_sdp=shifter_draw_pointer;
  MEM_ADDRESS save_sdp_at_start_of_line=VCountAtHSync;
  int save_drawn_so_far=scanline_drawn_so_far;
  SHORT save_pixel=shifter_tick8;
  shifter_draw_pointer=vbase;
  // we blit the unlocked backsurface
  if(!draw_lock || OPTION_3BUFFER_WIN && !FullScreen)
  { 
    BYTE option_vle=OPTION_VLE;
    OPTION_VLE=0; // no overscan
    draw_begin();
    short yy,yy2;
#ifndef NO_CRAZY_MONITOR
    if(extended_monitor)
    {
      yy=0;
      yy2=(short)MIN(em_height,Disp.SurfaceHeight);
    } else 
#endif
    if(border)
    {
      yy=draw_first_scanline_for_border;
      yy2=draw_last_scanline_for_border;
    }
    else
    {
      yy=0;
      yy2=shifter_y;
    }
    for(;yy<yy2;yy++) 
    {
      scan_y=yy;
      Glue.bFetchingLine=(yy>=0 && yy<shifter_y)||!!extended_monitor;
      scanline_drawn_so_far=0;
      VCountAtHSync=shifter_draw_pointer;
      Shifter.DrawScanlineToEnd();
    }
    OPTION_VLE=option_vle;
#ifndef SSE_NO_OSD
    if(draw_osd) 
      osd_init_draw_static();
#endif
    draw_end();
    draw_blit();
  }
  scan_y=save_scan_y;
  Glue.bFetchingLine=Glue.FetchingLine();
  shifter_draw_pointer=save_sdp;
  VCountAtHSync=save_sdp_at_start_of_line;
  scanline_drawn_so_far=save_drawn_so_far;
  shifter_tick8=save_pixel;
}


void init_screen() {
  draw_end();
  // shifter_x/shifter_y/res_vertical_scale are used to get the source rect.
  switch(screen_res) {
  case LORES:
    shifter_x=HOR_PIXELS_LO;shifter_y=VER_PIXELS_LO;
    res_vertical_scale=1;
    break;
  case MEDRES:
    shifter_x=HOR_PIXELS_MED;
    res_vertical_scale=1;
    shifter_y=VER_PIXELS_LO*res_vertical_scale;
    break;
  case HIRES:
    shifter_x=HOR_PIXELS_HI;shifter_y=VER_PIXELS_HI;
    res_vertical_scale=2;
    break;
  default: BREAKPOINT(switch(screen_res))
  }
  // These are used to determine where to draw
  draw_first_scanline_for_border=res_vertical_scale*(-TopBorderSize);
  draw_last_scanline_for_border=shifter_y+res_vertical_scale*(BottomBorderSize);
  if(res_vertical_scale==2) // shorter top border
  {
    draw_first_scanline_for_border+=BORDER71_SHIFT;
    draw_last_scanline_for_border+=BORDER71_SHIFT;
  }
  // 60Hz
  draw_first_scanline_for_border60=draw_first_scanline_for_border;
  draw_last_scanline_for_border60=draw_last_scanline_for_border;
  if(!SSEConfig.Border60Hz)
  {
    draw_first_scanline_for_border60+=BORDER60_MINUS_TOP;
    draw_last_scanline_for_border60-=BORDER60_MINUS_BOTTOM;
    if(border>1)
      draw_last_scanline_for_border60-=5;
    if(border==BIGGEST_BORDER)
      draw_first_scanline_for_border60+=BIG_BORDER_TOP-ORIGINAL_BORDER_TOP;
  }
  TRACE_LOG("F%d init_screen() %dx%d,%d-%d\n",FRAME,shifter_x,shifter_y,draw_first_scanline_for_border,draw_last_scanline_for_border);
}


void res_change() {
  TRACE_LOG("res_change()\n");
  Draw.MarshalParameters();
  if(ResChangeResize)
#if defined(SSE_VID_D3D_VSYNC)
    if(FullScreen
#if defined(SSE_420R5)
      ||!(OPTION_ADVANCED)
#else // it might be by design but this is buggy behaviour
#if defined(SSE_EMU_THREAD) // still fighting the glitches (esp. in VSync)
      || OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING && EmuThreadId==GetCurrentThreadId()
#endif
#endif
      )
    {
      // D3D9 doesn't seem to allow switching freq on the fly, would be slow anyway
      Disp.D3DSpriteInit(); // at least do that
    }
    else
#endif
      StemWinResize();
}


#if !defined(SSE_420R4)
bool draw_routines_init() { 
  PCpal=Get_PCpal(); // defined in draw_c or asm_draw.asm
  // DEADC0DE
  for(int a=0;a<5;a++)
    for(int c=0;c<3;c++)
      jump_draw_scanline[a][c]=draw_scanline_dont;
  // [0=Smallest size possible, 1=640x400 (all res), 2=640x200 (med/low res)]
  // [screen_res 0-2]
  jump_draw_scanline[0][0]=draw_scanline_32_lowres_pixelwise; //LO
  jump_draw_scanline[0][1]=draw_scanline_32_medres_pixelwise; //ME
  jump_draw_scanline[0][2]=draw_scanline_32_hires; //HI
  jump_draw_scanline[1][0]=draw_scanline_32_lowres_pixelwise_400; //LO
  jump_draw_scanline[1][1]=draw_scanline_32_medres_pixelwise_400; //ME
  jump_draw_scanline[1][2]=draw_scanline_32_hires; //HI
  // draw one line that will be copied in one go = faster than _400 ...
  jump_draw_scanline[2][0]=draw_scanline_32_lowres_pixelwise_dw; //LO
  jump_draw_scanline[2][1]=draw_scanline_32_medres_pixelwise; //ME
  jump_draw_scanline[2][2]=draw_scanline_32_hires; //HI
#if defined(SSE_VID_SIZE4)
  jump_draw_scanline[3][0]=draw_scanline_32_lowres_3x;
  jump_draw_scanline[3][1]=draw_scanline_32_medres_3x;
  jump_draw_scanline[3][2]=draw_scanline_32_hires_2w;
  jump_draw_scanline[4][0]=draw_scanline_32_lowres_4x;
  jump_draw_scanline[4][1]=draw_scanline_32_medres_4x;
  jump_draw_scanline[4][2]=draw_scanline_32_hires_4x;
#endif  
  draw_scanline=draw_scanline_dont; // SS init the general pointer
#ifndef SSE_NO_OSD
  osd_routines_init();
#endif
  return true;
}
#endif//#if defined(SSE_420R4)


#if 1
// to trace which rendering function is used, there are many possibilities
#define ASSIGN(fn1,fn2) \
  { fn1=fn2; \
    TRACE_LOG2(#fn1" = "#fn2"\n"); }
#else
#define ASSIGN(fn1,fn2) fn1=fn2;
#endif

/*  size, scanlines, stretch... marshal all display options and other
    parameters like ST resolution, borders etc. to know how to draw the frame
    called each time one parameter may have changed; but not on every frame */
void TDraw::MarshalParameters() {
  //TRACE("TDraw::MarshalParameters()\n");
  res=(video_mixed_output)?MEDRES:screen_res; // "Letter" 3x 
  Idx=WinSizeForRes[res];
  Scanlines=OPTION_SCANLINES;
  Buffered=Disp.DrawToVidMem;
#if defined(SSE_VID_D3D_VSYNC)
  ASSERTB(Glue.PreviousVideoFreq);
  if(Disp.Freq%Glue.PreviousVideoFreq==0) //eg 100, 120, 144
    Disp.FreqEmu=Glue.PreviousVideoFreq;
  else
    Disp.FreqEmu=Disp.Freq;
  DWORD &FreqEmu=Disp.FreqEmu;
#endif

#if defined(SSE_VID_D3D_VSYNC)
  bool bClose=abs((int)FreqEmu-(int)Glue.PreviousVideoFreq)<=2; // +-2Hz (?)
  if(fast_forward||slow_motion)
    VSync=false;
  else if(FullScreen)
    VSync=(FSDoVsync&&!OPTION_AUTOVSYNC_FS||OPTION_AUTOVSYNC_FS&&bClose);
  else
    VSync=(OPTION_WIN_VSYNC&&!OPTION_AUTOVSYNC||OPTION_AUTOVSYNC&&bClose);
  MasterSync=(VSync)?FreqEmu:Glue.PreviousVideoFreq;
#else
  VSync=((OPTION_WIN_VSYNC&&!FullScreen||FSDoVsync&&FullScreen)
    && (fast_forward==0&&slow_motion==0));
#endif
#ifdef WIN32
  if(FullScreen)
  {
#if defined(SSE_VID_D3D)
    Stretch=draw_stretch_fs;
    Size=(Stretch) ? 1 : DISPLAY_SIZE;
#endif
#if defined(SSE_VID_DD)
    if(draw_fs_blit_mode==DFSM_FLIP||draw_fs_blit_mode==DFSM_STRAIGHTBLIT)
    {
      Size=2;
      if(draw_fs_blit_mode==DFSM_STRAIGHTBLIT)
      {
        BltDst.left=(Disp.SurfaceWidth-BltSrc.right)/2;
        BltDst.top=(Disp.SurfaceHeight-BltSrc.bottom)/2;
      }
    }
    else
      Size=1;
#endif
  }
  else
  {
#if !defined(SSE_GUI_421_REORDER) //?
    if(OPTION_ADVANCED) // there's a setting for each resolution!
    {
      Stretch=(draw_win_mode[res]==DWM_STRETCH);
      Size=WinSizeForRes[res]+1;
    }
    else
#endif
    {
      Stretch=draw_stretch;
      Size=DISPLAY_SIZE;
    }
  }
#endif//WIN32
#ifdef UNIX
  Size=DISPLAY_SIZE;
#endif
  ASSERTB(Size>=1 && Size<=4);

  //HorPix: how many PC pixels for each ST pixel in the scanline?
  if(OPTION_C3) // plugin always draws double lores pixels, single for the rest
    HorPix=(res)?1:2;
  else if(Stretch)
    HorPix=1;
  else switch(res) {
  case LORES:
    HorPix=Size;
    break;
  case MEDRES: // 1x or 2x for the source, which may be stretched if Lock AR
  case HIRES:
    HorPix=(Size<3)?1:2;
    break;
  }
  //VerPix: how many PC scanlines (black or not) for each ST scanline?
#ifndef NO_CRAZY_MONITOR
  if(extended_monitor)
    VerPix=1;
  else
#endif
  if(Stretch)
    VerPix=(Scanlines)?2:1;
  else if(OPTION_C3)
    VerPix=2;
  else switch(res) {
  case LORES:
    VerPix=Size;
    break;
  case MEDRES: // if not Lock AR, 1-2-2-4, else 1-2-3-4
    VerPix=Size;
    if(!OPTION_LOCK_AR && Size==3)
      VerPix--;
    break;
  case HIRES:
    VerPix=(Size<3)?1:2;
    break;
  }
  EffectivePitch=VerPix*Pitch;
  if(Buffered)
  {
#if defined(SSE_420R4)
    // we allocate the scanline buffer when necessary
    if(!draw_temp_line_buf)
    {
      draw_temp_line_buf=(draw_type*)calloc(DRAW_TEMP_LINE_BUF_LEN,sizeof(draw_type));
      draw_temp_line_buf_lim=draw_temp_line_buf+DRAW_TEMP_LINE_BUF_LEN;
    }
#endif
    LineCpyMask=0;
    for(int i=0;i<VerPix;i++)
      LineCpyMask|=1<<i;
    if(Scanlines)
      LineCpyMask&=~(1<<(VerPix-1)); // only last line is black
  }
#if defined(SSE_420R4)
  else
  {
    // we free the scanline buffer as soon as it's not used anymore
    if(draw_temp_line_buf)
      free(draw_temp_line_buf);
    draw_temp_line_buf=draw_temp_line_buf_lim=NULL;
  }
#endif

#ifdef UNIX
  Stretch=0; // X can't stretch
  Size=DISPLAY_SIZE;
#endif

  SrcWidth=HOR_PIXELS_LO; // 320
  if(border)
    SrcWidth+=SideBorderSize*2;
  int res_hshift=(res)?1:0;
  SrcWidth<<=res_hshift;
  SrcWidth*=HorPix;
  
  SrcHeight=VER_PIXELS_LO; // 200
  if(border)
    SrcHeight+=TopBorderSize+BottomBorderSize;
  if(res&HIRES)
    SrcHeight<<=1;
  
  SrcHeight*=VerPix;
  // >0 for Large
  int border_adjust=((SideBorderSize-SideBorderSizeWin)*HorPix)<<res_hshift;
  BltSrc.left=border_adjust;
  BltSrc.right=SrcWidth-border_adjust;
  BltSrc.top=0;
  BltSrc.bottom=SrcHeight;
  // draw rountines (assembly or DrawC according to build options)
#if !defined(SSE421R2) // draw() still needs the routines
  if(!OPTION_C3)
#endif
  {
    BYTE size=(Stretch)?1:Size;
    if(Buffered || Scanlines) // routines draw 1 line
    {
      switch(size) {
      case 1:
        if(res)
          ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_pixelwise_dw)
        else
          ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_pixelwise)
        ASSIGN(draw_scanline_medres,draw_scanline_32_medres_pixelwise)
        ASSIGN(draw_scanline_hires,draw_scanline_32_hires)
        break;
      case 2:
#if defined(SSE_VID_SINGLEPIX)
        if(SSEOptions.SinglePixels)
          ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_pixelwise_dw_sp)
        else
#endif
          ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_pixelwise_dw)
        ASSIGN(draw_scanline_medres,draw_scanline_32_medres_pixelwise)
        ASSIGN(draw_scanline_hires,draw_scanline_32_hires)
        break;
      case 3:
#if defined(SSE_VID_SINGLEPIX)
        if(SSEOptions.SinglePixels)
          if(res)
            ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_4w_sp)
          else
            ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_3w_sp)
        else
#endif
        if(res)
          ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_4w)
        else
          ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_3w)
#if defined(SSE_VID_SINGLEPIX)
        if(SSEOptions.SinglePixels)
          ASSIGN(draw_scanline_medres,draw_scanline_32_medres_2w_sp)
        else
#endif
          ASSIGN(draw_scanline_medres,draw_scanline_32_medres_2w)
#if defined(SSE_VID_SINGLEPIX)
        if(SSEOptions.SinglePixels)
          ASSIGN(draw_scanline_hires,draw_scanline_32_hires_2w_sp)
        else
#endif
          ASSIGN(draw_scanline_hires,draw_scanline_32_hires_2w)
        break;
      case 4:
#if defined(SSE_VID_SINGLEPIX)
        if(SSEOptions.SinglePixels)
          ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_4w_sp)
        else
#endif
          ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_4w)
#if defined(SSE_VID_SINGLEPIX)
        if(SSEOptions.SinglePixels)
          ASSIGN(draw_scanline_medres,draw_scanline_32_medres_2w_sp)
        else
#endif
          ASSIGN(draw_scanline_medres,draw_scanline_32_medres_2w)
#if defined(SSE_VID_SINGLEPIX)
        if(SSEOptions.SinglePixels)
          ASSIGN(draw_scanline_hires,draw_scanline_32_hires_2w_sp)
        else
#endif
          ASSIGN(draw_scanline_hires,draw_scanline_32_hires_2w)
        break;
      }
    }
    else // routines draw several lines
    {
      switch(size) {
      case 1:
        if(res)
          ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_pixelwise_dw)
        else
          ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_pixelwise)
        ASSIGN(draw_scanline_medres,draw_scanline_32_medres_pixelwise)
        ASSIGN(draw_scanline_hires,draw_scanline_32_hires)
        break;
      case 2:
#if defined(SSE_VID_SINGLEPIX)
        if(SSEOptions.SinglePixels)
          ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_pixelwise_400_sp)
        else
#endif
          ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_pixelwise_400)
        ASSIGN(draw_scanline_medres,draw_scanline_32_medres_pixelwise_400)
        ASSIGN(draw_scanline_hires,draw_scanline_32_hires)
        break;
      case 3:
#if defined(SSE_VID_SINGLEPIX)
        if(SSEOptions.SinglePixels)
          if(res)
            ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_4x_sp)
          else
            ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_3x_sp)
        else
#endif
        if(res)
          ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_4x)
        else
          ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_3x)
#if defined(SSE_VID_SINGLEPIX)
        if(SSEOptions.SinglePixels)
          ASSIGN(draw_scanline_medres,draw_scanline_32_medres_3x_sp)
        else
#endif
          ASSIGN(draw_scanline_medres,draw_scanline_32_medres_3x)
#if defined(SSE_VID_SINGLEPIX)
        if(SSEOptions.SinglePixels)
          ASSIGN(draw_scanline_hires,draw_scanline_32_hires_4x_sp)
        else
#endif
          ASSIGN(draw_scanline_hires,draw_scanline_32_hires_4x)
        break;
      case 4:
#if defined(SSE_VID_SINGLEPIX)
        if(SSEOptions.SinglePixels)
          ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_4x_sp)
        else
#endif
          ASSIGN(draw_scanline_lowres,draw_scanline_32_lowres_4x)
#if defined(SSE_VID_SINGLEPIX)
        if(SSEOptions.SinglePixels)
          ASSIGN(draw_scanline_medres,draw_scanline_32_medres_4x_sp)
        else
#endif
          ASSIGN(draw_scanline_medres,draw_scanline_32_medres_4x)
#if defined(SSE_VID_SINGLEPIX)
        if(SSEOptions.SinglePixels)
          ASSIGN(draw_scanline_hires,draw_scanline_32_hires_4x_sp)
        else
#endif
          ASSIGN(draw_scanline_hires,draw_scanline_32_hires_4x)
        break;
      }
    }
    switch(res) {
    case LORES:
      ASSIGN(draw_scanline,draw_scanline_lowres)
      break;
    case MEDRES:
      ASSIGN(draw_scanline,draw_scanline_medres)
      break;
    case HIRES:
      ASSIGN(draw_scanline,draw_scanline_hires)
      break;
    default: BREAKPOINT(switch(res))
    }
  }//!C3

#ifndef NO_CRAZY_MONITOR
  if(extended_monitor) 
  {
    if(em_planes==1) 
      res=2;
#if defined(SSE_420R4)
    switch(res) {
    case LORES:
      ASSIGN(draw_scanline,draw_scanline_32_lowres_pixelwise)
      break;
    case MEDRES:
      ASSIGN(draw_scanline,draw_scanline_32_medres_pixelwise)
      break;
    case HIRES:
      ASSIGN(draw_scanline,draw_scanline_32_hires)
      break;
    }
#else
    draw_scanline=jump_draw_scanline[0][res]; // only use of jump_draw_scanline
#endif
    BltSrc.left=BltSrc.top=0;
    BltSrc.right=MIN(em_width,Disp.SurfaceWidth);
    BltSrc.bottom=MIN(em_height,Disp.SurfaceHeight);
    SrcWidth=BltSrc.right-BltSrc.left;
    SrcHeight=BltSrc.bottom-BltSrc.top;
  }
#endif//#ifndef NO_CRAZY_MONITOR

#if defined(SSE_VID_TRACE_SIZE)
  TRACE_LOG("res%d src %dx%d rect %d,%d,%d,%d +%d\n",res,SrcWidth,
    SrcHeight,BltSrc.left,BltSrc.top,BltSrc.right,BltSrc.bottom,EffectivePitch);
#endif
  draw_blit_source_rect=BltSrc; //tmp
  draw_dest_increase_y=EffectivePitch;

#ifndef NO_CRAZY_MONITOR
  if(extended_monitor)
  {
    draw_first_possible_line=0;
    draw_last_possible_line=(short)em_height;
  } else
#endif
  if(border) 
  {
    draw_first_possible_line=draw_first_scanline_for_border;
    draw_last_possible_line=draw_last_scanline_for_border;
  }
  else
  {
    draw_first_possible_line=0;
    draw_last_possible_line=shifter_y;
#if defined(SSE_VID_DD)
    draw_fs_topgap=40;
#endif
  }
}

#undef ASSIGN
#undef LOGSECTION
