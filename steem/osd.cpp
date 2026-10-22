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
FILE: osd.cpp
CONDITION: SSE_NO_OSD mustn't be defined
DESCRIPTION: Functions to create and draw Steem's on screen display that
appears when the emulator begins to run to give useful information.
Also disk track info, scrollers, debug info, FPS.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#ifndef SSE_NO_OSD

#include <debug.h>
#include <osd.h>
#include <computer.h>
#include <translate.h>
#include <draw.h>
#include <display.h>
#include <stdarg.h>
#include <diskman.h>


#define OSD_ICON_SIZE 24

EasyStr osd_scroller;
EasyStringList osd_scroller_array;
DWORD *osd_plasma_pal=NULL;
BYTE *osd_plasma=NULL;
LONG *osd_font=NULL;
#ifdef WIN32
#if !defined(SSE_OSD_NORESETINFO)
HWND hResetInfoWin=NULL;
#endif
#endif
LONG col_blue,col_red,col_green,col_white;
LONG col_yellow[2],col_fd_red[2];
DWORD FDCCantWriteDisplayTimer=0,HDDisplayTimer=0;
DWORD osd_start_time,osd_scroller_start_time,osd_scroller_finish_time;
bool osd_shown_scroller=false;

#ifdef UNIX
extern "C" LONG* Get_charset_blk();
#endif

void osd_draw_plasma(int,int,int);

typedef void ASMCALL OSDDRAWCHARCLIPPEDPROC (LONG*,draw_type*,LONG,LONG,int,LONG,LONG,RECT*);
typedef void ASMCALL OSDBLUEIZELINEPROC (int,int,int);
typedef void ASMCALL OSDBLACKRECTPROC (void*,int,int,int,int,LONG);
typedef OSDDRAWCHARCLIPPEDPROC* LPOSDDRAWCHARCLIPPEDPROC;
typedef OSDBLUEIZELINEPROC* LPOSDBLUEIZELINEPROC;
typedef OSDBLACKRECTPROC* LPOSDBLACKRECTPROC;

LPOSDDRAWCHARPROC jump_osd_draw_char=NULL;
LPOSDDRAWCHARPROC jump_osd_draw_char_transparent=NULL;
LPOSDDRAWCHARCLIPPEDPROC jump_osd_draw_char_clipped=NULL;
LPOSDDRAWCHARCLIPPEDPROC jump_osd_draw_char_clipped_transparent=NULL;
LPOSDBLACKRECTPROC jump_osd_black_box=NULL;

LPOSDBLUEIZELINEPROC osd_blueize_line;
LPOSDDRAWCHARPROC osd_draw_char,osd_draw_char_transparent;
LPOSDDRAWCHARCLIPPEDPROC osd_draw_char_clipped;
LPOSDDRAWCHARCLIPPEDPROC osd_draw_char_clipped_transparent;
LPOSDBLACKRECTPROC osd_black_box;

void ASMCALL osd_draw_char_dont(LONG*,draw_type*,LONG,LONG,int,LONG,LONG) {}
void ASMCALL osd_draw_char_clipped_dont(LONG*,draw_type*,LONG,LONG,int,LONG,LONG,RECT*) {}
void ASMCALL osd_blueize_line_dont(int,int,int) {}
void ASMCALL osd_black_box_dont(void*,int,int,int,int,LONG) {};


void osd_draw_begin() {

  osd_draw_char=jump_osd_draw_char;
  osd_draw_char_clipped=jump_osd_draw_char_clipped;
  osd_draw_char_transparent=jump_osd_draw_char_transparent;
  osd_draw_char_clipped_transparent=jump_osd_draw_char_clipped_transparent;
  osd_black_box=jump_osd_black_box;
  osd_blueize_line=osd_blueize_line_32;

  col_yellow[0]=colour_convert(255,215,0);
  col_yellow[1]=colour_convert(200,170,0);
  col_red=colour_convert(255,0,0);
  col_blue=colour_convert(0,0,255);
  col_green=colour_convert(0,255,0);
  col_white=colour_convert(255,255,255);
  col_fd_red[0]=colour_convert(255,0,0);
  col_fd_red[1]=colour_convert(200,0,0);
}


void osd_draw_end() {
  osd_draw_char=osd_draw_char_dont;
  osd_draw_char_clipped=osd_draw_char_clipped_dont;
  osd_draw_char_transparent=osd_draw_char_dont;
  osd_draw_char_clipped_transparent=osd_draw_char_clipped_dont;
  osd_black_box=osd_black_box_dont;
  osd_blueize_line=osd_blueize_line_dont;
}


void osd_init_run(bool allow_scroller) {
  osd_start_time=timeGetTime();
  if(allow_scroller)
    osd_shown_scroller=false;
  else
  {
    osd_shown_scroller=true;
    osd_scroller_finish_time=0;
  }
  OsdControl.no_draw=false;
#if defined(SSE_OSD_DEBUGINFO)
  if(runstate==RUNSTATE_RUNNING && run_speed_ticks_per_second!=1000)
  {
    TRACE_OSD_DBG("%s %d%%","Speed",100000/run_speed_ticks_per_second);
  }
#endif
}


void osd_init_draw_static() {
  osd_start_time=timer;
  osd_shown_scroller=true;
  osd_scroller_finish_time=0;
}


EasyStr get_osd_scroller_text(int n) {
  EasyStr ret=osd_scroller_array[n].String;
  char *p=strchr(ret.Text,0xa7),c; // look for Section sign
  int ic;
  while(p)
  {	// found
    ic=(int)(p-ret.Text);
    c=ret[ic+1];
    switch(c) {	// translate special code that follows
    case 'V':case 'v':
      //ret=ret.Lefts(ic)+"Steem Engine v"+(char*)stem_version_text+(ret.Text+ic+2);
      ret=ret.Lefts(ic)+gAppName+" v"+(char*)stem_version_text+" R" 
        + SSE_VERSION_R+(ret.Text+ic+2);
      break;
    case 'B':case 'b':
      ret=ret.Lefts(ic)+(char*)stem_version_date_text+(ret.Text+ic+2);
      break;
    case 'D':case 'd':
      if(FloppyDrive[DRIVE_A].Empty())
        ret=ret.Lefts(ic)+"NO DISK"+(ret.Text+ic+2);
      else
        ret=ret.Lefts(ic)+FloppyDrive[DRIVE_A].GetDisk()+(ret.Text+ic+2);
      break;
    default:
      ret=ret.Lefts(ic)+(ret.Text+ic+2);
      break;
    }
    p=strchr(ret.Text+ic+1,0xa7);
  }
  return ret;
}


void osd_pick_scroller() {
  int rnd=rand();
  if(!OsdControl.show_scrollers&&!OsdControl.show_jokes||!border||screen_res<2&&DISPLAY_SIZE==1)
    return;
  if(osd_scroller_array.NumStrings==0) 
    return;
  if(OsdControl.ScrollerPhase!=TOsdControl::WANT_SCROLLER)
    if((rnd%100)+1>OsdControl.ScrollerFrequency)
      return;
  OsdControl.ScrollerPhase=TOsdControl::SCROLLING;
  int n;
  BOOL NoGood;
  do {
    NoGood=FALSE;
    n=(rand()%osd_scroller_array.NumStrings);
    osd_scroller=get_osd_scroller_text(n);
    if(osd_scroller.Text[0]=='!') // bad joke!
    {
      if(OsdControl.show_jokes)
        osd_scroller.Text[0]=' '; // hide '!'
      else
        NoGood=TRUE;
    }
    else if(!OsdControl.show_scrollers)
      NoGood=TRUE;
  } while(NoGood);
  strupr(osd_scroller.Text);
  osd_shown_scroller=true;
#if defined(SSE_STATS)
  StatsStatic.nScrollers++;
#endif
  osd_scroller_start_time=timer+100;
  osd_scroller_finish_time=osd_scroller_start_time+20*20
    +(int)osd_scroller.Length()*4*20+(1280/4*20);
  OsdControl.ScrollerPosition=0;
}


#ifdef DEADC0DE
void osd_start_scroller(char *t) {
  osd_scroller=t;
  strupr(osd_scroller.Text);
  osd_start_time=0;
  osd_shown_scroller=true;
  osd_scroller_start_time=timer+100;
  osd_scroller_finish_time=osd_scroller_start_time+20*20
    +(int)osd_scroller.Length()*4*20+(1280/4*20);
}
#endif


void osd_draw() {
#ifndef ONEGAME
  if(OsdControl.no_draw||OsdControl.disable) 
    return;
#if defined(SSE_OSD_SHOW_TIME)
  if(OPTION_OSD_TIME && OsdControl.StartingTime)
  {
    DWORD ms=timer-OsdControl.StartingTime; // milliseconds wasted on Steem
    DWORD h,m,s;
    ms_to_hms(ms,h,m,s);
    OsdControl.Trace("%02d:%02d:%02d",h,m,s);
  }
#endif
  if(!draw_mem)
    return;
  const int x1=draw_blit_source_rect.right-draw_blit_source_rect.left;
  const int y1=draw_blit_source_rect.bottom-draw_blit_source_rect.top;
  bool can_have_scroller=true;
  int seconds=MAX(MIN((timer-osd_start_time)/1000,(DWORD)30),(DWORD)0);
  int icon_x=x1-5-OSD_ICON_SIZE;
  int icon_y=y1-5-OSD_ICON_SIZE;
#ifdef WIN32
#if !defined(SSE_LIBRETRONUKE)
#if !defined(SSE_VID_LS)  // see main.cpp
  if(!SSEConfig.IsInit && ShowTips)
  {
    OsdControl.bOsdDrawn=true;
    const BYTE nlines=4+2;
    EasyStr advice[nlines];
    int line=0;
    advice[line++]=T("WELCOME TO STEEM SSE");
    advice[line++]=T("F1: HELP");
    if(SSEOptions.PauseRun)
    {
      advice[line++]=T("PAUSE: START/STOP");
      advice[line++]=T("SHIFT PAUSE: MOUSE ON/OFF");
    }
    if(SSEOptions.F12Run)
    {
      advice[line++]=T("F11: MOUSE ON/OFF");
      advice[line++]=T("F12: START/STOP");
    }
    DWORD col=col_yellow[0];
    RECT cliprect;
    GetClientRect(StemWin,&cliprect);
    int start_y=0;
    for(line=0;line<nlines;line++)
    {
      int x=draw_blit_source_rect.left;
      for(int i=0;i<advice[line].Length();i++)
      {
        int n=(int)(advice[line].Text[i])+(60-33);
        if(n>=60&&n<120)
          osd_draw_char_clipped(osd_font+(n*64),draw_mem,x,start_y+26/2
            -OSD_LOGO_H/2+line*30,draw_line_length,col,32,&cliprect);
        x+=16;
      }
    }
  }
#endif//#if !defined(SSE_VID_LS)
#endif//#if !defined(SSE_LIBRETRONUKE)
#endif//WIN32  

  if(icon_x<0||icon_y<0) // if source rect = 0,0,0,0 
    return;
  if(OsdControl.show_icons)
  {
    if(fast_forward)
    {
      OsdControl.bOsdDrawn=true;
      osd_draw_char(osd_font+(34*64),draw_mem,icon_x,icon_y,
        draw_line_length,col_green,OSD_ICON_SIZE);
    }
    else if(bSoundRecord)
    {
      OsdControl.bOsdDrawn=true;
      osd_draw_char(osd_font+(36*64),draw_mem,icon_x,icon_y,
        draw_line_length,col_red,OSD_ICON_SIZE);
    }
  }
  if(seconds<OsdControl.show_plasma)
  {
    OsdControl.bOsdDrawn=true;
#define PLASMA_MAX (32)
#define PLASMA_W ((5+1)*(PLASMA_MAX))
#define PLASMA_H 26

    int x=x1/2-PLASMA_W/2;
    int start_y=4;
    int frame=14;
    {
      if(osd_plasma_pal==NULL)
      {
        osd_plasma_pal=new DWORD[PLASMA_MAX*2];
        osd_plasma=new BYTE[PLASMA_W*PLASMA_H];
        BYTE *p=osd_plasma;
        for(int py=0;py<PLASMA_H;py++)
        {
          for(int px=0;px<PLASMA_W;px++)
          {
            *(p++)=(BYTE)(PLASMA_MAX/2+double(PLASMA_MAX/2-1)
              *sin(hypot(px+PLASMA_W/8,(PLASMA_H/2-py)*4)/16));
          }//nxt
        }//nxt
      }//if
      DWORD end_time=osd_start_time+OsdControl.show_plasma*1000-500;
      if(OsdControl.show_plasma==OSD_SHOW_ALWAYS)
        end_time=timer+20;
      if(timer>=end_time)
        frame=MIN(15-int(timer-end_time)/20,14);
      else
        frame=MIN(int(timer-(osd_start_time+200))/20,14);
      if(frame>=0)
        osd_draw_plasma(x,start_y,frame);
    }
    if(frame==14)
    {
      x=x1/2-OSD_LOGO_W/2;
#if defined(SSE_BUILD)
      char tmp_buffer[64];
      DWORD totms=OsdControl.show_plasma*1000;
      DWORD nowms=timer-osd_start_time;
      // keep same size, display STEEM SSE then version
      LONG col_logo;
      if(nowms<totms/2)
      {
#ifdef SSE_BETA
        sprintf(tmp_buffer," STEEM SSE");
#else
        sprintf(tmp_buffer," %s",gAppName);
        strupr(tmp_buffer);
#endif
        col_logo=col_white;
      }
      else
      {
#ifdef SSE_BETA
        sprintf(tmp_buffer,"** BETA  **");
#else
        sprintf(tmp_buffer,"%s%s R%d",(SSE_VERSION_R>9?" ":"  "),
          (char*)stem_version_text,SSE_VERSION_R);
#endif
        col_logo=col_yellow[0];
      }
      for(unsigned int i=0;i<strlen(tmp_buffer);i++)
      {
        int n=(int)(tmp_buffer[i])+(60-33);	// need macro?
        if(tmp_buffer[i]=='.')
          x-=4; // closer when dot
        if(n>=60&&n<120)
          osd_draw_char(osd_font+(n*64),draw_mem,x-11-8-8,
            start_y+PLASMA_H/2-OSD_LOGO_H-1,draw_line_length,col_logo,32);
        x+=16;
        if(tmp_buffer[i]=='.')
          x-=4;
      }//nxt i

#else // this is a graphic included in the font indicating v3.2
      for(int c=0;c<OSD_LOGO_W/32+1;c++)
      {
        osd_draw_char(osd_font+((50+c)*64),draw_mem,x,start_y+PLASMA_H/2
          -OSD_LOGO_H/2,draw_line_length,col_white,OSD_LOGO_H);
        x+=32;
      }
#endif//logo
    }
  }
  else if(osd_plasma_pal)
  {
    delete[] osd_plasma_pal; osd_plasma_pal=NULL;
    delete[] osd_plasma;     osd_plasma=NULL;
  }
  if(seconds<OsdControl.show_speed)
  {
    if(avg_frame_time && runstate==RUNSTATE_RUNNING)
    {
      OsdControl.bOsdDrawn=true;
      can_have_scroller=false;
      int real_bar_h=12,bar_h;
      DWORD end_time=osd_start_time+OsdControl.show_speed*1000-500;
      if(OsdControl.show_speed==OSD_SHOW_ALWAYS)
        end_time=timer+20;
      if(timer>=end_time)
        bar_h=real_bar_h-int(timer-end_time)/20;
      else
        bar_h=MIN(int(timer-(osd_start_time+100))/20,real_bar_h);
      if((bar_h+1+1)>0 && MasterSync)
      {
        int bar_w=120,bar_x=draw_blit_source_rect.left+6,bar_y=y1-5-12-bar_h/2;
        int ntarget=(1000*NFRAME_TIME_AVG)/MasterSync;
        // compensating +1 -1 when 60Hz or 72Hz, because of 1ms resolution
        // without overdoing it (cheating)
        if(MasterSync>50)
        {
          static char carry=0;
          ntarget+=carry;
          int difference=ntarget-avg_frame_time;
          if(difference<0)
          {
            ntarget++;
            carry=-1;
          }
          else if(difference>0)
          {
            ntarget--;
            carry=1;
          }
          else
            carry=0;
        }
        double speed=ntarget/(double)avg_frame_time;
        int w=MIN(MAX(int(double(bar_w-1)*speed),2),x1-bar_x);
#if defined(SSE_DRAW_C)
        if(draw_mem+(bar_y+bar_h)*draw_line_length+bar_x+bar_w <Disp.VideoMemoryEnd)
#endif   
        {
          osd_black_box(draw_mem,bar_x-1,bar_y-1,1+bar_w+1,1+bar_h+1,draw_line_length);
          for(int y=bar_y;y<bar_y+bar_h;y++)
            osd_blueize_line(bar_x,y,w);
        }
      }
    }
  }
  if(seconds<OsdControl.show_cpu)
  {
    if(nSysCyclesPerSecond>CpuNormalHz)
    {
      OsdControl.bOsdDrawn=true;
      can_have_scroller=false;
      int bar_w=120,bar_x=draw_blit_source_rect.left+5,cpu_y=y1-5-12+6-15;
      int x=nSysCyclesPerSecond/CpuNormalHz;
      x=bar_x+bar_w+10-x+((timer&15)*x)/(int)(16);
      osd_draw_char(osd_font+((int)41*64),draw_mem,(x),cpu_y,draw_line_length,col_red,20);
    }
  }
  if(seconds<OsdControl.show_icons)
  {
    can_have_scroller=false;
    if(runstate==RUNSTATE_RUNNING)
    {
      if(fast_forward==0 && !bSoundRecord)
      {
        OsdControl.bOsdDrawn=true;
        // Play icon
        int full_h=OSD_ICON_SIZE,h;
        DWORD end_time=osd_start_time+OsdControl.show_icons*1000-500;
        if(OsdControl.show_icons==OSD_SHOW_ALWAYS)
          end_time=timer+20;
        if(timer>=end_time)
          h=MIN(full_h+1-int(timer-end_time)/20,full_h);
        else
          h=MIN(int(timer-(osd_start_time+100))/20,full_h);
        if(h==full_h)
        {
          osd_draw_char(osd_font+(33*64)-2,draw_mem,icon_x,icon_y-1,
            draw_line_length,colour_convert(255,255,0),OSD_ICON_SIZE);
        }
        else if(h>0)
        {
          osd_draw_char(osd_font+(33*64)-2,draw_mem,icon_x,icon_y-1+full_h/2-(h+1)/2,
            draw_line_length,colour_convert(255,255,0),(h+1)/2);
          if(h>1)
          {
            osd_draw_char(osd_font+(33*64)-2+full_h*2-(h & ~1),draw_mem,icon_x,icon_y-1+full_h/2,
              draw_line_length,colour_convert(255,255,0),h/2);
          }
        }
      }
    }
#if 0
    else
    {
      if(!fast_forward)
      { // this draws a blue square
        osd_draw_char(osd_font+(35*64),draw_mem,icon_x,icon_y,draw_line_length,
          col_blue,OSD_ICON_SIZE);
        if(draw_grille_black<4) 
          draw_grille_black=4;
      }
    }
#endif
  }
  if(OsdControl.MessageTimer>timer)// || seconds<OsdControl.show_speed)
  {
    OsdControl.bOsdDrawn=true;
    DWORD col=col_yellow[0];
    // TODO refactor in basic function?
    RECT cliprect={0,0,x1,y1};
    int x=draw_blit_source_rect.left;
    int start_y=0+8;
    for(unsigned int i=0;i<strlen(OsdControl.m_OsdMessage);i++)
    {
      int n=(int)(OsdControl.m_OsdMessage[i])+(60-33);
      if(n>=60&&n<120)
        osd_draw_char_clipped(osd_font+(n*64),draw_mem,x,start_y
          +PLASMA_H/2-OSD_LOGO_H/2,draw_line_length,col,32,&cliprect);
      x+=16;
    }//nxt i
  }
#if defined(SSE_OSD_DRIVELED)
  // Green led for floppy disk read; red for write.
  if(OPTION_DRIVE_INFO||OsdControl.show_disk_light)
  {   
    // There's no timer, we directly check MOTOR ON
    if((Fdc.str&FDC_STR_MO) && (psg_reg[PSGR_PORT_A]&6)!=6
      &&FloppyDrive[DRIVE_A].ImageType.Manager!=MNGR_PRG
      && (FloppyDrive[DRIVE].bDiskInDrive||!OPTION_HACKS))
    {
      BOOL FDCWriting=DiskEmu.WritingToDisk();
      OsdControl.bOsdDrawn=true;
      int idx=32,w=20;
      if(draw_blit_source_rect.bottom>200+TopBorderSize+BottomBorderSize)
        idx=37,w=32;
      DWORD coli=(hbl_count/512)&1;
      DWORD col;
      if(!FDCWriting)
      { 
        // wonder who will perceive that effect
        int g=180;
        if(DiskEmu.BitRate<500)
          g-=40; // BitRate may be more or less correct but here it's all or nothing anyway
        else if(DiskEmu.BitRate>500)
          g+=40;
        if(!coli)
          g+=35;
        int a=(DiskEmu.BitRate==0x68)?g:0; // gray for fuzzy
        col=colour_convert(a,g,a);
      }
      else
        col=col_fd_red[coli];
      if(FDCCantWriteDisplayTimer>timer) // warning write not saved
      {
        osd_draw_char(osd_font+(38*64),draw_mem,(x1-w)-24,1,draw_line_length,col_red,16);
        if(((FDCCantWriteDisplayTimer-timer)%500)<=250)
          osd_draw_char(osd_font+(39*64),draw_mem,(x1-w)-24,1,draw_line_length,col_red,16);
      }
      if(OsdControl.show_disk_light && !OPTION_DRIVE_INFO) // no led if track info
        osd_draw_char(osd_font+(idx*64),draw_mem,(x1-w)-4,4,draw_line_length,col,8);
      if(OPTION_DRIVE_INFO) // Display drive, side, track, sector
      {
        RECT cliprect={0,0,x1,y1};
        DWORD drive_info_length=(int)strlen(DiskEmu.sTrackinfo);
        int x=x1-(drive_info_length+1)*16;
        int start_y=0+8;
        for(unsigned int i=0;i<drive_info_length;i++)
        {
          int n=(int)(DiskEmu.sTrackinfo[i])+(60-33);
          if(n>=60&&n<120)
            osd_draw_char_clipped(osd_font+(n*64),draw_mem,x,start_y+PLASMA_H/2
              -OSD_LOGO_H/2,draw_line_length,col,32,&cliprect);
          x+=16;
        }//nxt i
      }
    }
    // Amber led for hard disk activity
    if(HDDisplayTimer>timer)
    {
      OsdControl.bOsdDrawn=true;
      int idx=32,w=20;
      if(draw_blit_source_rect.bottom>200+TopBorderSize+BottomBorderSize)
        idx=37,w=32;
      DWORD col=col_yellow[(hbl_count/512)&1];
      osd_draw_char(osd_font+(idx*64),draw_mem,(x1-w)-4,4,draw_line_length,col,8);
    }
  }
#endif//#if defined(SSE_OSD_DRIVELED)
  if(can_have_scroller && (!osd_shown_scroller
    || OsdControl.ScrollerPhase==TOsdControl::WANT_SCROLLER))
  {
    osd_shown_scroller=true;
    osd_scroller_finish_time=timer;
    osd_pick_scroller();
  }
  else if(osd_shown_scroller && timer>osd_scroller_finish_time
    + OsdControl.SecondsBetweenScrollers*1000)
  {
    osd_shown_scroller=false;
    osd_scroller_finish_time=0;
  }
  else if(osd_shown_scroller && timer<osd_scroller_finish_time
    && OsdControl.ScrollerPhase!=TOsdControl::NO_SCROLLER
    || OsdControl.ScrollerPhase==TOsdControl::SCROLLING)
  {
    OsdControl.bOsdDrawn=true;
    //int pos=(timer-osd_scroller_start_time)/20;
    OsdControl.ScrollerPosition++; // follows ST frequency, can be smooth if vsynced at that freq
    int pos=OsdControl.ScrollerPosition;
    if(pos>=20)
    {
      int i=(pos-20)/4;
      int xo=x1-4*((pos-20)&3),x;
      x=xo;
#define THE_LEFT ((border==2)?4:0)
#define THE_RIGHT ((x1))
      RECT cliprect={THE_LEFT,0,THE_RIGHT,y1};
      int scroll_len=(int)osd_scroller.Length();
      //while(x>(THE_LEFT-SideBorderSizeWin))
      while(x>(THE_LEFT-LeftBorderSize))
      {
        if(i<scroll_len)
        {
          LONG colour=colour_convert(255,0,0); // scrollers in red are generally readable
          int n=int(osd_scroller[i])+(60-33);
          if(n>=60&&n<120)
          {
            //if(x>=THE_LEFT && x<THE_RIGHT-SideBorderSizeWin)
            if(x>=THE_LEFT && x<THE_RIGHT-RightBorderSize)
              osd_draw_char(osd_font+(n*64),draw_mem,x,y1-24-5,draw_line_length,colour,32);
            else if(x<THE_RIGHT)
            {
              osd_draw_char_clipped(osd_font+(n*64),draw_mem,x,y1-24-5,
                draw_line_length,colour,32,&cliprect);
            }
          }
          if(i==(scroll_len-1))
          {
            if((x+16)<THE_LEFT)
            {
              OsdControl.ScrollerPhase=TOsdControl::NO_SCROLLER;
              //osd_scroller_finish_time=0; // keep for multi
            }
          }
        }
        x-=16;
        if((--i)<0) 
          break;
      }
#undef THE_LEFT
#undef THE_RIGHT
    }
  }
#endif // ONEGAME
  if(OsdControl.bOsdDrawn && draw_grille_black<4) 
    draw_grille_black=4;
}


void osd_draw_plasma(int const x,int const start_y,int const frame) { // called by osd_draw()
  if(!draw_lock)
    return;
  double tr=(double)timer/1024,tb=tr/2,tg=tb/2;
  for(int i=0;i<PLASMA_MAX*2;i++)
    osd_plasma_pal[i]=colour_convert((BYTE)(172+63.0*cos(i*M_PI/32+tr)),
      (BYTE)(172+63.0*sin(i*M_PI/32+tg)),(BYTE)(172-63.0*cos(i*M_PI/32+tb)));
  int idx1_list[PLASMA_H],idx2_list[PLASMA_H];
  int base_y=timer/32;
  for(int y=0;y<PLASMA_H;y++)
  {
    idx1_list[y]=(int)(y*PLASMA_W+PLASMA_W/4-(double)(PLASMA_W/4)
      *sin((double)(base_y+y*2)/PLASMA_H));
    idx2_list[y]=(int)(y*PLASMA_W+PLASMA_W/4+(double)(PLASMA_W/4)
      *sin((double)(base_y+y/2)/PLASMA_H));
  }
  DWORD *p_fuji=(LPDWORD)(osd_font+frame*64);
  BYTE *tl_adr=(BYTE*)draw_mem+x*BytesPerPixel;
  int y_offset=(start_y+1)*draw_line_length*sizeof(draw_type);
  DWORD black_pal=0;
  for(int y=0;y<PLASMA_H;y++)
  {
    int idx1=idx1_list[y],idx2=idx2_list[y];
    BYTE *p=(BYTE*)(tl_adr+y_offset);
    DWORD fuji_mask=*(p_fuji++);
    DWORD fuji_data=*(p_fuji++);
    DWORD bitmask=1;
#if defined(SSE_DRAW_C)
    if((BYTE*)p+(PLASMA_W)*BytesPerPixel<(BYTE*)Disp.VideoMemoryEnd)
#endif    
    for(int px=0;px<PLASMA_W/2;px++)
    {
      for(int pixel=0;pixel<2;pixel++)
      {
        bool draw_data=((fuji_data & bitmask)!=0);
        bool draw_mask=((fuji_mask & bitmask)!=0);
        bitmask<<=1;
        if(frame==14)
        {
          if(px==0&&pixel==0)
            draw_mask=draw_data,draw_data=0;
          else if(px==PLASMA_W/2-1&&pixel==1)
            draw_mask=draw_data,draw_data=0;
        }
        if(pixel==(y&1)) 
          draw_data=0;
        BYTE *p_pal=NULL;
        if(draw_data) 
          p_pal=(LPBYTE)(osd_plasma_pal+osd_plasma[idx1]+osd_plasma[idx2]);
        else if(draw_mask) 
          p_pal=(LPBYTE)&black_pal;
        if(p_pal)
          *(LPDWORD)p=*(LPDWORD)p_pal;
        p+=BytesPerPixel;
      }
      idx1++,idx2++;
      if(bitmask==0) 
        bitmask=1;
    }
    y_offset+=draw_line_length*sizeof(draw_type);
  }
}


void osd_hide() {
  osd_start_time=osd_scroller_finish_time=0;
  osd_shown_scroller=OsdControl.no_draw=true;
}


#ifndef ONEGAME
#if !defined(SSE_OSD_NORESETINFO)
#include <harddiskman.h>

void osd_get_reset_info(EasyStringList *sl) {
  sl->Sort=eslNoSort;
  Str t=Str(T("Model: "))+st_model_name[ST_MODEL]+Str(" TOS: v")+HEXSl(tos_version,3).Insert(".",1);
  sl->Add(t);
  sl->Add(T("Memory size")+": "+(mem_len>>10)+"Kb");
  t=T("Monitor")+": ";
#ifndef NO_CRAZY_MONITOR
  if(extended_monitor)
    t+=T("Extended Monitor At")+" "+em_width+"x"+em_height+"x"+em_planes;
  else
#endif
  {
    if(MONO_MONITOR)
      t+=T("Monochrome")+" ("+T("High Resolution")+")";
    else
      t+=T("Colour")+" ("+T("Low/Med Resolution")+")";
  }
  sl->Add(t);
  sl->Add(T("ST CPU speed")+": "+(n_millions_cycles_per_sec)+" "+T("Megahertz"));
  t=T("Active drives")+": A";
  if(DiskMan.nFloppyDrives==2) 
    t+=", B";
  for(int n=DRIVE_C;n<GEMDOS_MAXDRIVES;n++) 
    if(Stemdos.DriveMounted[n]) 
      t+=Str(", ")+char('A'+n);
#ifdef SSE_ACSI
#if defined(SSE_420R5)
  if(ACSI_EMU_ON&&AcsiHardDiskMan.nDrives)
#else
  if(AcsiHardDiskMan.nDrives)
#endif
    t+=Str(", ")+(AcsiHardDiskMan.nDrives)+T(" ACSI device(s)");
#endif
  sl->Add(t);
  if(pasti_active)
    t=T("Pasti disk emulation enabled");
  else  if(!DiskMan.bTurboDrive)
    t=T("Drive speed")+": "+T("Normal");//+T("Slow");
  else
    t=T("Drive speed")+": "+T("Turbo");//+T("Fast");
  if(t[0]) 
    sl->Add(t);
  t=T("Active ports")+": ";
  if(MIDIPort.IsOpen()) 
    t+="MIDI ";
  if(ParallelPort.IsOpen()) 
    t+=T("Parallel")+" ";
  if(SerialPort.IsOpen()) 
    t+=T("Serial")+" ";;
#if defined(SSE_DONGLE_PORT) 
  if(DONGLE_ID)
    t+=T("Special adapter");
#endif
  if(NotSameStr(t.Rights(2),": ")) 
    sl->Add(t);
  if(cart)
  {
    Str Name=GetFileNameFromPath(CartFile);
    char *dot=strrchr(Name,'.');
    if(dot) 
      *dot='\0';
    t=T("Cartridge")+": "+Name;
    sl->Add(t);
  }
}
#endif//#if !defined(SSE_OSD_NORESETINFO)


#if !defined(SSE_OSD_NORESETINFO)
void osd_draw_reset_info
#ifdef WIN32
  (HDC dc)
#endif
#ifdef UNIX
  (int win_x,int win_y,int win_w,int win_h)
#endif
{
  EasyStringList sl;
  osd_get_reset_info(&sl);
#ifdef WIN32
  int th=GetTextSize(fnt,sl[0].String).Height;
  int max_tw=9999;
#endif
#ifdef UNIX
  int th=hxc::font->ascent+hxc::font->descent;
  int max_tw=MAX(win_w-15-15,200);
#endif
  int info_h=(th+2)*sl.NumStrings;
  int tw=0,hi_tw=0;
  for(int n=sl.NumStrings-1;n>=0;n--)
  {
#ifdef UNIX
    tw=XTextWidth(hxc::font,sl[n].String,strlen(sl[n].String));
#endif
#ifdef WIN32
    tw=GetTextSize(fnt,sl[n].String).Width;
#endif
    if(n==0) 
      tw+=2+RC_FLAG_WIDTH;
    if(tw>=max_tw)
    {
      Str new_str=sl[n].String;
      if(new_str.RightChar()=='.')
        new_str.Delete(new_str.Length()-3,1);
      else
      {
        new_str.Delete(new_str.Length()-1,1);
        new_str+="..";
      }
      sl.SetString(n,new_str);
      n++;
    }
    else if(tw>hi_tw)
      hi_tw=tw;
  }
  int x=5,y=3;  
#ifdef WIN32
  HDC osd_ri_dc=dc;
  HANDLE old_font=SelectObject(osd_ri_dc,fnt);
  RECT fr={x-5,y-3,x+hi_tw+5,y+info_h+1};
  FrameRect(osd_ri_dc,&fr,(HBRUSH)GetStockObject(BLACK_BRUSH));
  fr.left++;fr.top++; fr.right--;fr.bottom--;
  FillRect(osd_ri_dc,&fr,(HBRUSH)GetStockObject(WHITE_BRUSH));
#endif
#ifdef UNIX
  XSetFont(XD,DispGC,hxc::font->fid);
  int px=win_x+win_w/2-(hi_tw+10)/2,py=win_y+(win_h/2)-(info_h/2);
  XSetForeground(XD,DispGC,WhiteCol);
  XFillRectangle(XD,StemWin,DispGC,px-4,py-2,hi_tw+8,info_h+2);
  hxc::draw_border(XD,StemWin,DispGC,px-5,py-3,hi_tw+10,info_h+3,1,
    BlackCol,BlackCol);
#endif
  for(int n=0;n<sl.NumStrings;n++)
  {
#ifdef WIN32
    TextOut(osd_ri_dc,x,y,sl[n].String,(int)strlen(sl[n].String));
#endif
#ifdef UNIX
    XDrawString(XD,StemWin,DispGC,x,y+hxc::font->ascent,
      sl[n].String,strlen(sl[n].String));
#endif
    if(n==0&&tos_version)
    {
      int FlagIdx=OptionBox.TOSLangToFlagIdx(ROM_PEEK(0x1d));
#ifdef WIN32
      if(FlagIdx>=0)
      {
        HDC TempDC=CreateCompatibleDC(dc);
        HANDLE OldBmp=SelectObject(TempDC,LoadBitmap(hInstance,"TOSFLAGS"));
        BitBlt(osd_ri_dc,x+tw-RC_FLAG_WIDTH,y+MAX(th-RC_FLAG_HEIGHT,0)/2,
          RC_FLAG_WIDTH,RC_FLAG_HEIGHT,TempDC,FlagIdx*RC_FLAG_WIDTH,0,SRCCOPY);
        DeleteObject(SelectObject(TempDC,OldBmp));
        DeleteDC(TempDC);
      }
#endif
#ifdef UNIX
      IcoTOSFlags.DrawIcon(FlagIdx,StemWin,DispGC,x+tw-RC_FLAG_WIDTH,y+MAX(th-RC_FLAG_HEIGHT,0)/2);
#endif
    }
    y+=th+2;
  }
#ifdef WIN32
  SelectObject(dc,old_font);
#endif
}
#endif//#if !defined(SSE_OSD_NORESETINFO)

#else//#ifndef ONEGAME

void osd_draw_reset_info(HDC) {}

#endif


#ifdef WIN32
#if !defined(SSE_OSD_NORESETINFO)
LRESULT CALLBACK ResetInfoWndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar) {
#ifndef ONEGAME
  switch(Mess) {
  case WM_PAINT:
  {
    PAINTSTRUCT ps;
    BeginPaint(Win,&ps);
    osd_draw_reset_info(ps.hdc);
    EndPaint(Win,&ps);
    return 0;
  }
  case WM_USER:
  {
    if(wPar!=1789) 
      break;
    // Update size and position
    EasyStringList sl;
    osd_get_reset_info(&sl);
    RECT rc;
#if defined(SSE_VID_DD)
    if(FullScreen)
    {
      get_fullscreen_rect(&rc);
      rc.top-=MENUHEIGHT;
    }
    else
#endif
    {
      GetClientRect(StemWin,&rc);
      rc.top+=MENUHEIGHT;//+2;
      rc.bottom-=2;
#ifdef WIN32
      rc.bottom-=GuiSM.m_statusbar_height;
#endif
      //rc.left+=2;
    }
    int th=GetTextSize(fnt,sl[0].String).Height;
    int info_h=(th+2)*sl.NumStrings;
    int tw,max_tw=0;
    for(int n=sl.NumStrings-1;n>=0;n--)
    {
      tw=GetTextSize(fnt,sl[n].String).Width;
      if(n==0)
        tw+=2+8;
      if(tw>max_tw)
        max_tw=tw;
    }
    int x=rc.left+(rc.right-rc.left)/2-(max_tw+10)/2;
    SetWindowPos(Win,NULL,x,rc.bottom-info_h-4,max_tw+10,info_h+4,SWP_NOZORDER);
    return 0;
  }//case
  }//sw
#endif
  return DefWindowProc(Win,Mess,wPar,lPar);
}
#endif//#if !defined(SSE_OSD_NORESETINFO)
#endif


void osd_routines_init() {
  jump_osd_draw_char=osd_draw_char_32;
  jump_osd_draw_char_clipped=osd_draw_char_clipped_32;
  jump_osd_black_box=osd_black_box_32;
  jump_osd_draw_char_transparent=osd_draw_char_transparent_32;
  jump_osd_draw_char_clipped_transparent=osd_draw_char_clipped_transparent_32;
  osd_draw_char=osd_draw_char_dont;
  osd_draw_char_clipped=osd_draw_char_clipped_dont;
  osd_draw_char_transparent=osd_draw_char_dont;
  osd_draw_char_clipped_transparent=osd_draw_char_clipped_dont;
  osd_black_box=osd_black_box_dont;
  osd_blueize_line=osd_blueize_line_dont;
#ifdef WIN32
  {
    HRSRC res;
    HGLOBAL hglob;
    res=FindResource(hInstance,"OSD_FONT_BLOCK",RCNUM(300) /*RT_RCDATA*/);
    if(res)
    {
      hglob=LoadResource(hInstance,res);
      if(hglob)
        osd_font=(LONG*)LockResource(hglob);
#if defined(SSE_OSD_EXTRACT_GRAPHICS)
      // finally could produce graphics of charset.blk!
      // to edit and save back we used other tools: irfanview, paint, winhex
      pbm_image descr;
      descr.width=32*2; // off + on
      descr.height=30720/(descr.width/8);
      descr.size=(descr.width/8)*descr.height;
      int ndwords=descr.size/4;
      DWORD *dwp=(DWORD*)osd_font;
      DWORD *dw=new DWORD[ndwords];
      for(int i=0;i<ndwords;i++)
      {
        dw[i]=dwp[i];
        SWAP_BIG_ENDIAN_DWORD(dw[i]); // pbm is big-endian
      }
      descr.data=((BYTE*)dw);
      FILE *outfile=fopen("charset.pbm","wb");
      pbm_save(&descr, outfile);
      delete [] dw;
#endif
    }
  }
#endif
#ifdef UNIX
  osd_font=Get_charset_blk();
#endif
  if(osd_font==NULL)
  {
    jump_osd_draw_char=osd_draw_char_dont;
    jump_osd_draw_char_clipped=osd_draw_char_clipped_dont;
    jump_osd_draw_char_transparent=osd_draw_char_dont;
    jump_osd_draw_char_clipped_transparent=osd_draw_char_clipped_dont;
  }
  osd_start_time=0;
}


TOsdControl OsdControl;

TOsdControl::TOsdControl() {
  ZeroMemory(this,sizeof(TOsdControl));
  show_plasma=show_speed=show_icons=show_cpu=4; // seconds
  show_disk_light=true;
  show_jokes=true;
  ScrollerFrequency=10;
  SecondsBetweenScrollers=60*5;
}


void TOsdControl::HdLed(int time) {
  if(show_disk_light)
    HDDisplayTimer=timer+time;
}


/*  Display some information in yellow on the top left corner of the screen,
    a bit like drive info, but it can be any (short) message.
    To spare code we handle sending those messages to the statusbar here.
*/  
void TOsdControl::Trace(char* fmt,...) {
  va_list body;	
  va_start(body, fmt);	
#if defined(SSE_UNIX)
  vsnprintf(m_OsdMessage,OSD_MESSAGE_LENGTH,fmt,body); // check for overrun 
#else
  _vsnprintf(m_OsdMessage,OSD_MESSAGE_LENGTH,fmt,body); // check for overrun 
#endif
  va_end(body);
  strupr(m_OsdMessage); // OSD font is upper-only
  MessageTimer=((runstate==RUNSTATE_STOPPED)?timeGetTime():timer)+OSD_MESSAGE_TIME*1000;
}


// output 1: on status bar only if possible, 3 on both OSD and status bar
BYTE TOsdControl::Trace(int output,char *fmt,...) {
  va_list body;	
  va_start(body, fmt);	
#if defined(SSE_UNIX)
  vsnprintf(m_OsdMessage,OSD_MESSAGE_LENGTH,fmt,body); // check for overrun 
#else
  _vsnprintf(m_OsdMessage,OSD_MESSAGE_LENGTH,fmt,body); // check for overrun 
#endif
  va_end(body);
  BYTE done=0;
#if defined(SSE_GUI_STATUS_BAR)
#if defined(SSE_GUI_STATUS_BAR_CLICKABLE)
  if(!FullScreen&&(output&OUTPUT_SB)&&!(SSEConfig.StatusBarMask&(1<<SB_PART_MAIN)))
#else
  if(!FullScreen&&(output&OUTPUT_SB))
#endif
  {
    if(strcmp(m_OsdMessage,status_bar_text[SB_PART_MAIN]))
    {
      strcpy(status_bar_text[SB_PART_MAIN],m_OsdMessage);
#if defined(SSE_GUI_STATUS_BAR_DRAW_MAIN)
      UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
#else
      PostMessage(hStatusBar,SB_SETTEXT,SB_PART_MAIN,(LPARAM)OsdControl.m_OsdMessage);
#endif
    }
    SetTimer(StemWin,STATUSBAR_TIMER_ID,3000,NULL); //3s to read
    done|=OUTPUT_SB;
  }
#endif//#if defined(SSE_GUI_STATUS_BAR)
  if((output&OUTPUT_OSD)||!done &&OPTION_OSD_DEBUGINFO)
  {
    strupr(m_OsdMessage); // OSD font is upper-only
    MessageTimer=((runstate==RUNSTATE_STOPPED)?timeGetTime():timer)+OSD_MESSAGE_TIME*1000;
    done|=OUTPUT_OSD;
  }
  return done;
}


#undef OSD_ICON_SIZE

#endif//#ifndef SSE_NO_OSD
