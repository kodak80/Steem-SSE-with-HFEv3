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

DOMAIN: GUI
FILE: stemwin.cpp
DESCRIPTION: This file handles the main Steem window and its various buttons.
Keyboard input starts here.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <draw.h>
#include <display.h>
#include <computer.h>
#include <shortcutbox.h>
#include <patchesbox.h>
#include <infobox.h>
#include <translate.h>
#include <loadsave.h>
#include <diskman.h>
#include <stjoy.h>
#include <harddiskman.h>
#include <dataloadsave.h>
#include <osd.h>
#include <key_table.h>
#ifdef DEBUG_BUILD
#include <debugger.h>
#endif
#if defined(SSE_DEBUGGER_FRAME_REPORT)
#include <debug_framereport.h>
#endif
#ifdef WIN32
#include <windowsx.h>
void HandleButtonMessage(UINT Id,HWND hBut);
#endif

#if defined(SSE_EMU_THREAD)

HANDLE hEmuThread=NULL;
DWORD EmuThreadId=0;
bool SuspendRendering=false;

// "hang instead of crashing"
// main thread calls lock, rendering parameters will change
// emu thread calls acknowledge when it's ready to wait
// main thread calls unlock so that emu thread can continue

BOOL TThreadFlag::Lock() {
  if(disabled||blocked)
    return FALSE;
  acknowledged=false;
  blocked=true;
  while(!acknowledged && runstate==RUNSTATE_RUNNING)
  {
    if(disabled)
    {
      acknowledged=true;
      blocked=false;
    }
#if (_WIN32_WINNT>=0x0400)
    SwitchToThread();
#else
    Sleep(0);
#endif
  }
  return TRUE;
}


BOOL TThreadFlag::Acknowledge() {
  if(!blocked||disabled)
    return FALSE;
  while(blocked && runstate==RUNSTATE_RUNNING)
  {
    acknowledged=true;
#if (_WIN32_WINNT>=0x0400)
    SwitchToThread();
#else
    Sleep(0);
#endif
  }
  return TRUE;
}


BOOL TThreadFlag::Unlock() {
  if(!blocked||disabled)
    return FALSE;
  blocked=acknowledged=false;
  return TRUE;
}


TThreadFlag SoundLock,VideoLock,DiskLock;


DWORD WINAPI EmuThreadProc(PVOID pParam) { // called instead of run()
#if defined(SSE_GUI_MENUBAR)
  EnableMenuItem(StemWinMenu,IDC_MENUKILLTHREAD,MF_ENABLED);
#endif
  run();
  SendMessage((HWND)pParam,BM_SETCHECK,0,0); // reset play button
  if(OptionBox.IsVisible())
    EnableWindow(GetDlgItem(OptionBox.Handle,IDC_EMU_THREAD),TRUE);
#if defined(SSE_GUI_MENUBAR)
  EnableMenuItem(StemWinMenu,IDC_MENUKILLTHREAD,MF_DISABLED);
#endif
  hEmuThread=NULL;
  return 0;
}

#endif//#if defined(SSE_EMU_THREAD)


#if defined(SSE_GUI_MENUBAR)

bool AltMenuOn=false;

void check_alt_menu() { // little helper
  if(AltMenuOn)
    SteemSetMenu(false);
  AltMenuOn=false;
}

#define CHECK_ALT_MENU check_alt_menu();

#else

#define CHECK_ALT_MENU

#endif

#define LOGSECTION LOGSECTION_VIDEO_RENDERING

// StemWinResize(int xo,int yo)
// |
// +--> SetStemWinSize(int w,int h,xo,yo)

void StemWinResize(int xo,int yo) {
#if defined(SSE_LIBRETRONUKE)
  TRACEB("no StemWinResize\n");
#else
  int res=(video_mixed_output) ? MEDRES : screen_res;
  int Idx=WinSizeForRes[res];
  int w,h;
#ifndef NO_CRAZY_MONITOR
  if(extended_monitor) 
  {
    int FrameWidth=0,sbh=0;
#ifdef WIN32
    FrameWidth=GuiSM.cx_frame()*2;
    sbh=GuiSM.m_statusbar_height;
#endif
#if defined(SSE_VID_2SCREENS)
    w=MIN((int)em_width,(int)(Disp.rcMonitor.right-Disp.rcMonitor.left-FrameWidth));
    h=MIN((int)em_height,(int)(Disp.rcMonitor.bottom-Disp.rcMonitor.top-5-MENUHEIGHT-sbh-30));
#else
    w=MIN((int)em_width,(int)(GetScreenWidth()-FrameWidth));
    h=MIN((int)em_height,(int)(GetScreenHeight()-5-MENUHEIGHT-sbh-30));
#endif
  }
  else 
#endif
  if(border)
  {
    w=WinSizeBorder[res][Idx].x;
    h=WinSizeBorder[res][Idx].y;
    //optional PAL aspect ratio in windowed mode
    if(OPTION_ST_ASPECT_RATIO && res<2) // also non stretch, even if that's not beautiful
      h=(int)((float)h*ST_ASPECT_RATIO_DISTORTION);
    xo=xo*WinSize[res][Idx].x/640;
    yo=yo*WinSize[res][Idx].y/400;
  }
  else
  {
    LONG s_w=(LONG)GetScreenWidth();
    //ASSERT(Idx);
    while(WinSize[res][Idx].x>s_w
#ifndef SSE_LEAN_AND_MEAN
      &&Idx
#endif
      ) 
      Idx--;
    w=WinSize[res][Idx].x;
    h=WinSize[res][Idx].y;
    if(OPTION_ST_ASPECT_RATIO && res<2)
    {
      h=(int)((Glue.PreviousVideoFreq==NTSC_HZ)
        ? (float)h*ST_ASPECT_RATIO_DISTORTION_60HZ 
        : (float)h*ST_ASPECT_RATIO_DISTORTION);
    }
  }
#ifdef WIN32
  RECT rc;
  GetClientRect(StemWin,&rc);
  int w0=rc.right-rc.left;
#if defined(SSE_GUI_STATUS_BAR)
  int sbh=GuiSM.m_statusbar_height;
#else
  int sbh=0;
#endif
  int h0=rc.bottom-MENUHEIGHT-sbh-rc.top;
  if(w==w0&&h==h0)
  {
    TRACE_LOG("Skip StemWinResize(%d,%d,%d,%d) res %d Idx %d\n",xo,yo,w,h,res,Idx);
    //Draw.MarshalParameters();
    Disp.UpdateSurfaces(w,h);
  }
  else if(w&&h)
#endif//WIN32
  {
    TRACE_LOG("StemWinResize(%d,%d,%d,%d) res %d Idx %d\n",xo,yo,w,h,res,Idx);
    SetStemWinSize(w,h,xo,yo);
  }
#if defined(SSE_VID_D3D)
  if(D3D9_OK && Disp.pD3DDevice)
    Disp.D3DSpriteInit(); //smooth res changes (eg in GEM)
#endif
  DISPLAY_SIZE=WinSizeForRes[res]+1;
#endif
}


void fast_forward_change(bool Down,bool Searchlight) {
  if(Down) 
  {
    if(fast_forward<=0) 
    {
      if(runstate==RUNSTATE_STOPPED) 
      {
        CLICK_PLAY_BUTTON();
        fast_forward=3;
      }
      else if(runstate==RUNSTATE_STOPPING) 
      {
        if(fast_forward==-1) 
          runstate=RUNSTATE_RUNNING;
        fast_forward=3;
      }
      else
        fast_forward=1;
      // keep sound if emu thread, fixing the concurrency seems hard, the effect
      // here is not too bad
      if(!OPTION_EMUTHREAD)
        SoundStop();
    }
    flashlight(Searchlight);
  }
  else if(fast_forward) 
  {
    if(fast_forward==3)
    {
      fast_forward=0;
      if(runstate==RUNSTATE_RUNNING) 
      {
        runstate=RUNSTATE_STOPPING;
        fast_forward=-1;
      }
#ifdef WIN32
      bRunMessagePosted=false;
#endif      
    }
    else
      fast_forward=0;
    fast_forward_stuck_down=false;
    flashlight(false);
    if(!OPTION_EMUTHREAD)
      SoundStart();
  }
  floppy_access_started_ff=false;
#ifdef WIN32
  SendMessage(GetDlgItem(StemWin,IDC_FASTFORWARD),BM_SETCHECK,fast_forward,0);
#endif
#ifdef UNIX
  FastBut.set_check(fast_forward);
#endif
}


void flashlight(bool on) {
  if(on && !flashlight_flag) 
  { //turn flashlight on
#if defined(SSE_BUILD) // another "improvement"
    int rnd=rand(); // make it random so if a scheme isn't ideal it can be changed
    for(int n=0;n<16;n++)
    {
      int i=(!(n&BIT_3))?255:128;
      bool r=!(n&BIT_2);
      bool g=!(n&BIT_1);
      bool b=!(n&BIT_0);
      PCpal[(n+rnd)&15]=colour_convert((r?i:0),(g?i:0),(b?i:0));
    }
    // colour 0 always white
    LONG tmp=PCpal[0];
    PCpal[0]=PCpal[rnd&15];
    PCpal[rnd&15]=tmp;
    HiresPixelMask=1;
#else
    for(int n=0;n<9;n++)
      PCpal[n]=colour_convert(240-n*15,255-n*15,60);
    for(int n=0;n<7;n++)
      PCpal[n+9]=colour_convert(0,30+n*8,50+n*30);
#endif
    flashlight_flag=true;
  }
  else if(!on) 
  {
    flashlight_flag=false;
    HiresPixelMask=0;
    draw_init_resdependent();
    palette_convert_all();
  }
}


void slow_motion_change(bool Down) {
  if(Down) 
  {
    if(slow_motion<=0) 
    {
      if(runstate==RUNSTATE_STOPPED) 
      {
        CLICK_PLAY_BUTTON();
        slow_motion=3;
      }
      else if(runstate==RUNSTATE_STOPPING) 
      {
        if(slow_motion==-1) 
          runstate=RUNSTATE_RUNNING;
        slow_motion=3;
      }
      else
        slow_motion=1;
      // keep sound if emu thread, fixing the concurrency seems hard, the effect
      // here is bad (stutter)
      if(!OPTION_EMUTHREAD)
        SoundStop();
    }
  }
  else if(slow_motion) 
  {
    if(slow_motion==3)
    {
      slow_motion=0;
      if(runstate==RUNSTATE_RUNNING) 
      {
        runstate=RUNSTATE_STOPPING;
        slow_motion=-1;
#ifdef WIN32
        bRunMessagePosted=false;
#endif 
      }
    }
    else
      slow_motion=0;
    if(!OPTION_EMUTHREAD)
      SoundStart();
  }
}


void SetStemWinSize(int w,int h,int xo,int yo) {
  TRACE_LOG("SetStemWinSize %d %d %d %d\n",xo,yo,w,h);
  int cw=w,ch=h;
#if defined(SSE_EMU_THREAD)
  BYTE old_vlock=VideoLock.disabled;
  if(OPTION_EMUTHREAD && runstate!=RUNSTATE_STOPPED)
  {
    if(GetCurrentThreadId()==EmuThreadId)
      VideoLock.disabled=1;
  }
#endif
#ifdef WIN32
  GuiSM.Update();
#if defined(SSE_GUI_STATUS_BAR)
  int sbh=GuiSM.m_statusbar_height;
#else
  int sbh=0;
#endif
  if(FullScreen)
  {
    rcPreFS.top=MAX(int(rcPreFS.top+yo),-GuiSM.cy_caption());
    rcPreFS.right=rcPreFS.left+w+GuiSM.cx_frame()*2;
    rcPreFS.bottom=rcPreFS.top+h+MENUHEIGHT+GuiSM.cy_frame()*2+GuiSM.cy_caption();
  }
  else
  {
    if(!bAppMaximized && !bAppMinimized)
    {
#if 1 // we don't depend on cx_frame and cy_frame for the main window size
      // we start from the existing window
      RECT rc;
      GetWindowRect(StemWin,&rc);
      int x=rc.left+xo;
      int y=MAX((int)(rc.top+yo),-GuiSM.cy_caption());
      rc.top=rc.left=0;
      rc.right=w;
      rc.bottom=h+MENUHEIGHT+sbh;
#if defined(SSE_GUI_MENUBAR)
      rc.bottom+=GuiSM.m_menubar_height;
#endif
      AdjustWindowRectEx(&rc,WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_CLIPSIBLINGS
        |((OPTION_BLOCK_RESIZE)?0:WS_SIZEBOX),FALSE,WS_EX_ACCEPTFILES);
      cw=rc.right-rc.left;
      ch=rc.bottom-rc.top;
#else
      RECT rc;
      GetWindowRect(StemWin,&rc);
      int x=rc.left+xo;
      int y=MAX((int)(rc.top+yo),-GuiSM.cy_caption());
      cw=w+GuiSM.cx_frame()*2;
      ch=h+MENUHEIGHT+GuiSM.cy_frame()*2+GuiSM.cy_caption()+sbh;
#endif
      TRACE_LOG("SetWindowPos4 %d %d %d %d\n",x,y,cw,ch);
      SetWindowPos(StemWin,0,x,y,cw,ch,SWP_NOZORDER|SWP_NOACTIVATE);
#if defined(SSE_GUI_STATUS_BAR)
      if(sbh)
      {
        REFRESH_STATUS_BAR;
      }
      ShowWindow(hStatusBar,(sbh?SW_SHOW:SW_HIDE));
#endif
    }
    else
    {
      WINDOWPLACEMENT wp;
      wp.length=sizeof(WINDOWPLACEMENT);
      GetWindowPlacement(StemWin,&wp);
      RECT *rc=&wp.rcNormalPosition;
      rc->left=MAX(int(rc->left+xo),-GuiSM.cy_caption());
      rc->top=MAX(rc->top+yo,0l);
      rc->right=rc->left+w+GuiSM.cx_frame()*2;
      rc->bottom=rc->top+h+MENUHEIGHT+sbh+GuiSM.cy_frame()*2+GuiSM.cy_caption();
#if defined(SSE_GUI_MENUBAR)
      rc->bottom+=GuiSM.m_menubar_height;
#endif
      SetWindowPlacement(StemWin,&wp);
    }
  }
#endif//WIN32

#ifdef UNIX
  if (XD==NULL) return;

  if (bAppMaximized==0 && bAppMinimized==0){
    XResizeWindow(XD,StemWin,2+w+2-2,MENUHEIGHT+2+h+2-2);
    XClearArea(XD,StemWin,0,0,2+w+2-2,MENUHEIGHT+2+h+2-2,True);
  }else{
    ///// Adjust restore size
  }

  XSizeHints *pHints=XAllocSizeHints();
  if (pHints){
    pHints->flags=PMinSize;
		pHints->min_width=320+4;
		pHints->min_height=200+4+MENUHEIGHT;
    XSetWMSizeHints(XD,StemWin,pHints,XA_WM_NORMAL_HINTS);
    XFree(pHints);
  }
#endif//UNIX

  Disp.UpdateSurfaces(cw,ch);
#if defined(SSE_EMU_THREAD)
  VideoLock.disabled=old_vlock;
#endif
}


void MoveStemWin(int x,int y,int w,int h) {
  if(StemWin==NULL)
    return;

#ifdef WIN32
  if(FullScreen) 
  {
    if(x==MSW_NOCHANGE) x=rcPreFS.left;
    if(y==MSW_NOCHANGE) y=rcPreFS.top;
    if(w==MSW_NOCHANGE) w=rcPreFS.right-rcPreFS.left;
    if(h==MSW_NOCHANGE) h=rcPreFS.top-rcPreFS.bottom;
    rcPreFS.left=x;rcPreFS.top=y;rcPreFS.right=x+w;rcPreFS.bottom=y+h;
  }
  else 
  {
    RECT rc;
    GetWindowRect(StemWin,&rc);
    int new_x=rc.left,new_y=rc.top,new_w=rc.right-rc.left,new_h=rc.bottom-rc.top;
    if(x!=MSW_NOCHANGE) new_x=x;
    if(y!=MSW_NOCHANGE) new_y=y;
    if(w!=MSW_NOCHANGE) new_w=w;
    if(h!=MSW_NOCHANGE) new_h=h;
    MoveWindow(StemWin,new_x,new_y,new_w,new_h,true);
  }
#endif

#ifdef UNIX
  if (XD==NULL) return;

  if (x==MSW_NOCHANGE || y==MSW_NOCHANGE){
    x=MSW_NOCHANGE;
    y=MSW_NOCHANGE;
  }

  XWindowAttributes wa;
  XGetWindowAttributes(XD,StemWin,&wa);
  int new_w=int((w==MSW_NOCHANGE) ? wa.width:w);
  int new_h=int((h==MSW_NOCHANGE) ? wa.height:h);
  if (w==wa.width && h==wa.height){ // Don't resize
    if (x!=MSW_NOCHANGE) XMoveWindow(XD,StemWin,x,y);
  }else{
    if (x==MSW_NOCHANGE){
      XResizeWindow(XD,StemWin,new_w,new_h);
    }else{
      XMoveResizeWindow(XD,StemWin,x,y,new_w,new_h);
    }
  }
#endif
}


#ifdef WIN32

#if 1 // should be faster but we add code+data, what is better?

BYTE extended_keys[11];

void InitRealVKCodeForKeypad() {
  extended_keys[0]=(BYTE)MapVirtualKeyEx(VK_INSERT,0,keyboard_layout);
  extended_keys[1]=(BYTE)MapVirtualKeyEx(VK_DELETE,0,keyboard_layout);
  extended_keys[2]=(BYTE)MapVirtualKeyEx(VK_END,0,keyboard_layout);
  extended_keys[3]=(BYTE)MapVirtualKeyEx(VK_DOWN,0,keyboard_layout);
  extended_keys[4]=(BYTE)MapVirtualKeyEx(VK_NEXT,0,keyboard_layout);
  extended_keys[5]=(BYTE)MapVirtualKeyEx(VK_LEFT,0,keyboard_layout);
  extended_keys[6]=(BYTE)MapVirtualKeyEx(VK_CLEAR,0,keyboard_layout);
  extended_keys[7]=(BYTE)MapVirtualKeyEx(VK_RIGHT,0,keyboard_layout);
  extended_keys[8]=(BYTE)MapVirtualKeyEx(VK_HOME,0,keyboard_layout);
  extended_keys[9]=(BYTE)MapVirtualKeyEx(VK_UP,0,keyboard_layout);
  extended_keys[10]=(BYTE)MapVirtualKeyEx(VK_PRIOR,0,keyboard_layout);
}


void GetRealVKCodeForKeypad(WPARAM &wPar,LPARAM &lPar,bool Extend) {
  static BYTE vk[11][2]={{VK_NUMPAD0,VK_INSERT},{VK_DECIMAL,VK_DELETE},{VK_NUMPAD1,VK_END},
    {VK_NUMPAD2,VK_DOWN},{VK_NUMPAD3,VK_NEXT},{VK_NUMPAD4,VK_LEFT},
    {VK_NUMPAD5,VK_CLEAR},{VK_NUMPAD6,VK_RIGHT},{VK_NUMPAD7,VK_HOME},
    {VK_NUMPAD8,VK_UP},{VK_NUMPAD9,VK_PRIOR}};
  BYTE Scancode=LOBYTE(HIWORD(lPar));
  for(int i=0;i<11;i++)
  {
    if(Scancode==extended_keys[i])
    {
      wPar=vk[i][Extend];
      break;
    }
  }
}

#else

void GetRealVKCodeForKeypad(WPARAM &wPar,LPARAM &lPar) {
  UINT Scancode=BYTE(HIWORD(lPar));
/*
24 Indicates whether the key is an extended key, such as the right-hand ALT 
and CTRL keys that appear on an enhanced 101- or 102-key keyboard. The
 value is 1 if it is an extended key; otherwise, it is zero. 
*/
  bool Extend=(lPar & BIT_24)!=0;
  if(Scancode==MapVirtualKey(VK_INSERT,0)) wPar=Extend?VK_INSERT:VK_NUMPAD0;
  if(Scancode==MapVirtualKey(VK_DELETE,0)) wPar=Extend?VK_DELETE:VK_DECIMAL;
  if(Scancode==MapVirtualKey(VK_END,0)) wPar=Extend?VK_END:VK_NUMPAD1;
  if(Scancode==MapVirtualKey(VK_DOWN,0)) wPar=Extend?VK_DOWN:VK_NUMPAD2;
  if(Scancode==MapVirtualKey(VK_NEXT,0)) wPar=Extend?VK_NEXT:VK_NUMPAD3;
  if(Scancode==MapVirtualKey(VK_LEFT,0)) wPar=Extend?VK_LEFT:VK_NUMPAD4;
  if(Scancode==MapVirtualKey(VK_CLEAR,0)) wPar=Extend?VK_CLEAR:VK_NUMPAD5;
  if(Scancode==MapVirtualKey(VK_RIGHT,0)) wPar=Extend?VK_RIGHT:VK_NUMPAD6;
  if(Scancode==MapVirtualKey(VK_HOME,0)) wPar=Extend?VK_HOME:VK_NUMPAD7;
  if(Scancode==MapVirtualKey(VK_UP,0)) wPar=Extend?VK_UP:VK_NUMPAD8;
  if(Scancode==MapVirtualKey(VK_PRIOR,0)) wPar=Extend?VK_PRIOR:VK_NUMPAD9;
}

#endif


#if !defined(SSE_LIBRETRONUKE)

LRESULT WINAPI WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar) {
  WORD wpar_lo=LOWORD(wPar),wpar_hi=HIWORD(wPar),lpar_lo=LOWORD(lPar),lpar_hi=HIWORD(lPar);

  switch(Mess) {
//  case WM_DEVICECHANGE: //need RegisterDeviceNotification...
  //  break;

#if defined(SSE_GUI_STATUS_BAR)

  case WM_CREATE:
  {
#ifndef SSE_LEAN_AND_MEAN // useless test
    if(hStatusBar==NULL)
#endif
    {
      // create a 4 parts status bar
      hStatusBar=DoCreateStatusBar(Win,(HMENU)IDC_STATUS_BAR,hInstance,4);
      // signal which parts are self-drawn
#if defined(SSE_GUI_STATUS_BAR_DRAW_FREQ)
      SendMessage(hStatusBar,SB_SETTEXT,SB_PART_FREQ|SBT_OWNERDRAW,(LPARAM)SB_PART_FREQ);
#endif
#if defined(SSE_GUI_STATUS_BAR_DRAW_MAIN)
      SendMessage(hStatusBar,SB_SETTEXT,SB_PART_MAIN|SBT_OWNERDRAW,(LPARAM)SB_PART_MAIN);
#endif
#if defined(SSE_GUI_STATUS_BAR_DRAW_ICONS)
      SendMessage(hStatusBar,SB_SETTEXT,SB_PART_ICONS|SBT_OWNERDRAW,(LPARAM)SB_PART_ICONS);
#endif
#if defined(SSE_GUI_STATUS_BAR_DRAW_CAPS)
      SendMessage(hStatusBar,SB_SETTEXT,SB_PART_CAPS|SBT_OWNERDRAW,(LPARAM)SB_PART_CAPS);
#endif
    }
    break;
  }

  case WM_PARENTNOTIFY:
#if defined(SSE_GUI_STATUS_BAR_CLICKABLE) // don't react to clicks anymore
    if(wpar_lo==WM_LBUTTONDOWN||wpar_lo==WM_RBUTTONDOWN) // get clicks in status bar
    {
      SHORT x=GET_X_LPARAM(lPar),y=GET_Y_LPARAM(lPar);
      RECT rc;
      GetClientRect(Win,&rc);
      for(int i=0;i<4;i++) // 4 panes
      {
        RECT &rcsb=status_bar_rc[i];
        int d=rc.bottom-rcsb.bottom;
        int left=x-rcsb.left;
        int right=rcsb.right-x;
        if(left>=0&&right>0&&y-d>=rcsb.top&&y-d<rcsb.bottom)
        {
          if(wpar_lo==WM_LBUTTONDOWN)
          {
/*   bit0 A bit 1 B set = single - we toggle bits
Toggling a bit

The XOR operator (^) can be used to toggle a bit.
number ^= 1 << x;

That will toggle bit x.
*/
            BYTE mask=1<<i;
            SSEConfig.StatusBarMask^=mask;
            switch(i) {
            case SB_PART_ICONS: // toggle flashlight
              flashlight(!!(SSEConfig.StatusBarMask&(1<<i)));
              if(runstate==RUNSTATE_STOPPED)
              {
                draw_end();
                draw(false);
              }
              break;
            case SB_PART_MAIN:
              mask|=1<<SB_PART_ICONS; // for TOS, RAM info
              break;
            }//sw
            GUIRefreshStatusBar(mask);
          }
          else // resize part on right click
          {
            int part_x[4];
            SendMessage(hStatusBar,SB_GETPARTS,(WPARAM)4,(LPARAM)part_x);
            BOOL left_or_right=(left>right);
            if(i==0 && !left_or_right || i==3 && left_or_right)
            {}
            else if(left_or_right) // right
              part_x[i]=x;
            else // left
              part_x[i-1]=x;
            SendMessage(hStatusBar,SB_SETPARTS,(WPARAM)4,(LPARAM)part_x);
          }
        }
      }
      if(wpar_lo==WM_RBUTTONDOWN)
      {
        for(int i=0;i<4;i++) // update rect
          SendMessage(hStatusBar,SB_GETRECT,i,(LPARAM)&status_bar_rc[i]);
      }
    }
#endif//#if defined(SSE_GUI_STATUS_BAR_CLICKABLE)
    break;

  case WM_DRAWITEM:
    // according to compile switches we can draw parts of status bar
    if(wPar==IDC_STATUS_BAR)
    {
      if(FullScreen || !OPTION_STATUS_BAR)
        return TRUE;
#if defined(SSE_EMU_THREAD)
      bool oldSuspendRendering=SuspendRendering;
      SuspendRendering=true;
#endif
      HDC &myHdc=((DRAWITEMSTRUCT*)lPar)->hDC;
      RECT &myRect=((DRAWITEMSTRUCT*)lPar)->rcItem;
      int &sb_part=(int&)((DRAWITEMSTRUCT*)lPar)->itemData;
      int myx=myRect.left+4;
      int myy=myRect.top+(myRect.bottom-myRect.top)/2; // half height
      int txty=myy;
      FillRect(myHdc,&myRect,(HBRUSH)COLOR_WINDOW);
      SetBkMode(myHdc, TRANSPARENT);
      switch(sb_part) {

      case SB_PART_FREQ: // Freq/Mouse
      {
#if defined(SSE_GUI_STATUS_BAR_DRAW_FREQ) //no
        if(SSEConfig.StatusBarMask&(1<<SB_PART_FREQ))
        {
#if defined(SSE_GUI_STATUS_BAR_MOUSE2)
          if(SSEConfig.MouseAd)
          {
            DU32 uxy(SafeLPeek(SSEConfig.MouseAd)); // emu detect or ini override only
            sprintf(status_bar_text[SB_PART_FREQ],"%d,%d",uxy.d16[HI],uxy.d16[LO]);
          }
          else
            sprintf(status_bar_text[SB_PART_FREQ],""); // delete
#elif defined(SSE_GUI_STATUS_BAR_MOUSE)
          DU32 uxy;
          if(SSEConfig.MouseAd)
          { // emu detect or ini override
            SetTextColor(myHdc,RGB(0,200,0));
            uxy.d32=SafeLPeek(SSEConfig.MouseAd);
          }
          else if(OPTION_C1 && (hd6301_peek(0xC9)&0xF8)==0xA8)
          { // mouse abs. mode (Star Trek)
            SetTextColor(myHdc,RGB(0,0,200));
            uxy.d8[3]=(BYTE)hd6301_peek(0xB6);
            uxy.d8[2]=(BYTE)hd6301_peek(0xB7);
            uxy.d8[1]=(BYTE)hd6301_peek(0xB8);
            uxy.d8[0]=(BYTE)hd6301_peek(0xB9);
          }
          else
          { // vq_mouse
            uxy.d16[HI]=Tos.MouseX;
            uxy.d16[LO]=Tos.MouseY;
          }
          sprintf(status_bar_text[SB_PART_FREQ],"%d,%d",uxy.d16[HI],uxy.d16[LO]);
#endif
        }
        else
        {
#if defined(SSE_VID_D3D) // problem: vsync can fail even if freqs are OK (secondary monitor)
          if(OPTION_WIN_VSYNC) // so a green freq can be misleading
            SetTextColor(myHdc,(Disp.Freq%Glue.PreviousVideoFreq==0)
              ?RGB(0,200,0) : RGB(200,0,0));
#endif
          sprintf(status_bar_text[SB_PART_FREQ],"%dHz",Glue.PreviousVideoFreq);
        }
        bDrawText=TRUE;
#endif//#if defined(SSE_GUI_STATUS_BAR_DRAW_FREQ)
        break;
      }//case SB_PART_FREQ

      case SB_PART_MAIN: // config/debug
#if defined(SSE_GUI_STATUS_BAR_DRAW_MAIN) //no
        status_bar_text[SB_PART_MAIN][0]='\0';
        if(Ikbd.Crashed)
          StatusInfo.MessageIndex=TStatusInfo::HD6301_CRASH;
        // build text of status bar, only if there's no special string
        // and debug info not enabled
        if(StatusInfo.MessageIndex==TStatusInfo::MESSAGE_NONE &&
          !(SSEConfig.StatusBarMask&(1<<SB_PART_MAIN)))
        {
          // basic ST/TOS/RAM
          char sb_st_model[20],sb_tos[5],sb_ram[7];
          if(PreciseModel.NotEmpty() && PreciseModel.Length()<20)
            sprintf(sb_st_model,"%s",PreciseModel.Text);
          else
            sprintf(sb_st_model,"%s",st_model_name[ST_MODEL]);
          sprintf(sb_tos,"T%03x",tos_version);
          sprintf(sb_ram,"%dK",mem_len>>10);
          sprintf(status_bar_text[SB_PART_MAIN],"%s %s %s",sb_st_model,sb_tos,sb_ram);
          if(Disp.Method==DISPMETHOD_GDI)
            strcat(status_bar_text[SB_PART_MAIN]," GDI");
        }
        switch(StatusInfo.MessageIndex) {
        case TStatusInfo::MESSAGE_NONE:
          break;
        case TStatusInfo::MC68000_CRASH:
          strcpy(status_bar_text[SB_PART_MAIN],T("HALT"));
          break;
        case TStatusInfo::BLIT_ERROR:
          strcpy(status_bar_text[SB_PART_MAIN],T("BLIT ERROR"));
          break;
        case TStatusInfo::X86_CRASH:
          strcpy(status_bar_text[SB_PART_MAIN],T(STEEM_CRASH_TXT));
          break;
        case TStatusInfo::HD6301_CRASH:
          strcpy(status_bar_text[SB_PART_MAIN],T("6301 CRASH"));
          break;
        default:
          strcpy(status_bar_text[SB_PART_MAIN],StatusInfo.text);
          break;
        }//sw
        bDrawText=TRUE;
#endif
      break;

      case SB_PART_ICONS:
      {
#if defined(SSE_GUI_STATUS_BAR_DRAW_ICONS) //yes
        // we display useful little icons in the third pane of the status bar
        int d=(GuiSM.m_statusbar_height>32)?35:19;
        int isize=(BIG_ICONS&&GuiSM.m_statusbar_height>32)?32:16; // must have room
        myy-=(isize/2);
        if(StatusInfo.MessageIndex==TStatusInfo::X86_CRASH)
        { // display 2 bombs in case Steem catches a system exception (for fun)
          for(int i=0;i<2;i++)
          {
            DrawIconEx(myHdc,myx,myy,hGUIIcon[RC_ICO_BOMB],isize,isize,0,NULL,DI_NORMAL);
            myx+=d;
          }
        }
        else if(flashlight_flag)
        { // high contrast colours (searchlight)
          DrawIconEx(myHdc,myx,myy,hGUIIcon[RC_ICO_OPS_BRIGHTCON],isize,isize,0,NULL,DI_NORMAL);
          myx+=d;
        }
        else
        {
          if(OPTION_TOSFLAG)
          {
            HDC TempDC=CreateCompatibleDC(myHdc);
            HANDLE OldBmp=SelectObject(TempDC,LoadBitmap(hInstance,"TOSFLAGS"));
            int FlagIdx=OptionBox.TOSLangToFlagIdx(SSEConfig.TosLanguage);
            if(FlagIdx>=0)
            {
              int flag_w,flag_h;
              if(isize==32) // bigger TOS flag
              {
                flag_h=GuiSM.m_statusbar_height-4;
                flag_w=(flag_h*RC_FLAG_WIDTH)/RC_FLAG_HEIGHT;
                StretchBlt(myHdc,myx,4,flag_w,flag_h,TempDC,FlagIdx
                  *RC_FLAG_WIDTH,0,RC_FLAG_WIDTH,RC_FLAG_HEIGHT,SRCCOPY);
              }
              else
              {
                flag_w=RC_FLAG_WIDTH;
                BitBlt(myHdc,myx,myy+2,flag_w,RC_FLAG_HEIGHT,TempDC,FlagIdx*RC_FLAG_WIDTH,0,SRCCOPY);
              }
              myx+=flag_w+4;
            }
            DeleteObject(SelectObject(TempDC,OldBmp));
            DeleteDC(TempDC);
          }
#if defined(SSE_GUI_STATUS_BAR_CLICKABLE)
          if(!(SSEConfig.StatusBarMask&(1<<SB_PART_MAIN))) // if it's not in 2nd pane...
#endif
          {
            char sb_ram[32],buff[64];
            if(SSEConfig.Mega)
              sprintf(sb_ram,"%dM",mem_len>>20); // so player can see it's a Mega //Smart! I had forgotten
            else
              sprintf(sb_ram,"%dK",mem_len>>10);
            sprintf(buff,"%01X.%02X %s",tos_version>>8,tos_version&0xFF,sb_ram); // TOS is enough, no need for STF/STE info
            SIZE sz;
            int l=(int)strlen(buff);
            GetTextExtentPoint32(myHdc,buff,l,&sz);
            txty-=(sz.cy/2);
#if !defined(SSE_GUI_STATUS_BAR_CLICKABLE)
            // coloured model clue
            bool bWrongTos=(tos_version<0x206&&(IS_STF&&tos_version>0x104||IS_STE&&tos_version<0x106));
            SetTextColor(myHdc,RGB((bWrongTos)?200:0,((IS_STE)?200:0),((SSEConfig.Mega)?200:0)));
#else
            if(IS_STF&&tos_version>0x104||IS_STE&&tos_version<0x106)
            {
#if defined(SSE_TOS206)
              if(tos_version==0x206 /*&& OPTION_HACKS*/)
                SetTextColor(myHdc,RGB(50,50,50)); // show it's STF
              else
#endif
                SetTextColor(myHdc,RGB(255,0,0)); // show we don't like the ST/TOS combination
            }
#endif
            TextOut(myHdc,myx,txty,buff,l);
            myx+=sz.cx+4;
          }
          // !!! run_speed_ticks_per_second<1000 means accelerated
          if(nSysCyclesPerSecond>CpuNormalHz||run_speed_ticks_per_second<1000)
          {
            DrawIconEx(myHdc,myx,myy,hGUIIcon[RC_ICO_CPUALTSPEED],isize,isize,0,NULL,DI_NORMAL);
            myx+=d;
          }
          if(nSysCyclesPerSecond<CpuNormalHz||run_speed_ticks_per_second>1000)
          { // if you want to see that snail again!
            DrawIconEx(myHdc,myx,myy,hGUIIcon[RC_ICO_ACCURATEFDC],isize,isize,0,NULL,DI_NORMAL);
            myx+=d;
          }
          if(!HardDiskMan.DisableHardDrives||ACSI_EMU_ON)
          {
            DrawIconEx(myHdc,myx,myy,hGUIIcon[RC_ICO_HARDDRIVE16],isize,isize,0,NULL,DI_NORMAL);
            myx+=d;
          }
          if(OPTION_C1) // I
          {
            DrawIconEx(myHdc,myx,myy,hGUIIcon[RC_ICO_OPS_C1],isize,isize,0,NULL,DI_NORMAL);
            myx+=d;
          }
          if(OPTION_VLE==1) // II
          {
            DrawIconEx(myHdc,myx,myy,hGUIIcon[RC_ICO_OPS_C2],isize,isize,0,NULL,DI_NORMAL);
            myx+=d;
          }
          else if(OPTION_VLE==2) // III
          {
            DrawIconEx(myHdc,myx,myy,hGUIIcon[RC_ICO_OPS_C3],isize,isize,0,NULL,DI_NORMAL);
            myx+=d;
          }
          if(OPTION_VLE>=1 && !border && Debug.ShifterTricks)  // oversan should be displayed?
            DrawIconEx(myHdc,myx-d,myy,hGUIIcon[RC_ICO_FULLQUIT],isize,isize,0,NULL,DI_NORMAL);
          Debug.noShifterTricks=!Debug.ShifterTricks;
          if(cart)
          {
            DrawIconEx(myHdc,myx,myy,hGUIIcon[RC_ICO_CHIP],isize,isize,0,NULL,DI_NORMAL);
            myx+=d;
          }
#ifndef SSE_NO_OSD
          if(OsdControl.bPrinting)
          {
            DrawIconEx(myHdc,myx,myy,hGUIIcon[RC_ICO_PRINT],isize,isize,0,NULL,DI_NORMAL);
            myx+=d;
          }
#endif
          if(DONGLE_ID)
          {
            DrawIconEx(myHdc,myx,myy,hGUIIcon[RC_ICO_EXTERNAL],isize,isize,0,NULL,DI_NORMAL);
            myx+=d;
          }
          if(stem_mousemode==STEM_MOUSEMODE_WINDOW)
          { // show when mouse has been captured
            DrawIconEx(myHdc,myx,myy,hGUIIcon[RC_ICO_STCUR],isize,isize,0,NULL,DI_NORMAL);
            if(IsJoyActive(0)) // feedback: mouse not operating, cross over cursor
              DrawIconEx(myHdc,myx,myy,hGUIIcon[RC_ICO_FULLQUIT],isize,isize,0,NULL,DI_NORMAL);
            myx+=d;
          }
          if(GetFocus()==StemWin) // hide joysticks when input not taken
          for(int j=0;j<2;j++)
          { // joysticks 0 & 1, numbers are somewhat hard to read (spares icons)
            if(IsJoyActive(j))
            {
              SetTextColor(myHdc,RGB(0,200,0));
              char txt[2];
              sprintf(txt,"%c",'0'+j);
              TextOut(myHdc,myx,0,txt,1);
              DrawIconEx(myHdc,myx,myy,hGUIIcon[RC_ICO_JOY],isize,isize,0,NULL,DI_NORMAL); 
              if(bSTjoyNoInput[j])  // the USB controller isn't connected, cross icon
                DrawIconEx(myHdc,myx,myy,hGUIIcon[RC_ICO_FULLQUIT],isize,isize,0,NULL,DI_NORMAL);
              myx+=d;
            }
          }
          // sound mute icon is a composition
          if(SSEConfig.SoundMute || MuteWhenInactive&&!bAppActive || !UseSound)
          {
            DrawIconEx(myHdc,myx,myy,hGUIIcon[RC_ICO_OPS_SOUND],isize,isize,0,NULL,DI_NORMAL);
            DrawIconEx(myHdc,myx,myy,hGUIIcon[RC_ICO_FULLQUIT],isize,isize,0,NULL,DI_NORMAL);
          }
          /*if(OPTION_HACKS)
          {
            DrawIconEx(myHdc,myx,myy,
              hGUIIcon[RC_ICO_OPS_HACKS],isize,isize,0,NULL,DI_NORMAL);
            myx+=d;
          }*/
        }//if
        break;
#endif//#if defined(SSE_GUI_STATUS_BAR_DRAW_ICONS)
      }//case SB_PART_ICONS

      case SB_PART_CAPS: // CAPS NUM SCRL or Disks
#if defined(SSE_GUI_STATUS_BAR_DRAW_CAPS) //no
        if(SSEConfig.StatusBarMask&(1<<SB_PART_CAPS))
        {
          if(DiskEmu.StatusBar)
          {
            if(DiskEmu.WritingToDisk())
              SetTextColor(myHdc,RGB(200,0,0));
            //else
              //SetTextColor(myHdc,RGB(0,200,0));//?
          }
          else if(FloppyDrive[DRIVE_A].ImageType.RealExtension>=NUM_EXT)
          {}
          else if(DiskMan.nFloppyDrives==1)
            sprintf(status_bar_text[SB_PART_CAPS],"A:%s",
              extension_list[FloppyDrive[DRIVE_A].ImageType.RealExtension]);
          else if(FloppyDrive[DRIVE_B].ImageType.RealExtension>=NUM_EXT)
          {}
          else if(DiskMan.nFloppyDrives==2)
            sprintf(status_bar_text[SB_PART_CAPS],"A:%s B:%s",
              extension_list[FloppyDrive[DRIVE_A].ImageType.RealExtension],
              extension_list[FloppyDrive[DRIVE_B].ImageType.RealExtension]);
        }
        else
        {
          status_bar_text[SB_PART_CAPS][0]='\0';
          if(GetKeyState(VK_CAPITAL)&1)
            strcat(status_bar_text[SB_PART_CAPS],T("CAPS "));
          if(GetKeyState(VK_NUMLOCK)&1)
            strcat(status_bar_text[SB_PART_CAPS],T("NUM "));
          if(GetKeyState(VK_SCROLL)&1)
            strcat(status_bar_text[SB_PART_CAPS],T("SCRL "));
          if(IsJoyActive(0))
            strcat(status_bar_text[SB_PART_CAPS],T("J0 "));
          if(IsJoyActive(1))
            strcat(status_bar_text[SB_PART_CAPS],T("J1 "));
        }
        bDrawText=TRUE;
#endif//#if defined(SSE_GUI_STATUS_BAR_DRAW_CAPS)
        break;
      }//sw
#ifdef DONT_DELETE
      if(bDrawText)
      {
        SIZE sz;
        int l=(int)strlen(status_bar_text[sb_part]);
        GetTextExtentPoint32(myHdc,status_bar_text[sb_part],l,&sz);
        txty-=(sz.cy/2);
        TextOut(myHdc,myx,txty,status_bar_text[sb_part],l);
      }
#endif
#if defined(SSE_EMU_THREAD)
      SuspendRendering=oldSuspendRendering;
#endif
      return TRUE;
    }//if(wPar==IDC_STATUS_BAR)
    break;

#endif//#if defined(SSE_GUI_STATUS_BAR)

  case WM_PAINT:
  {
    RECT dest;
    GetClientRect(Win,&dest);
    int Height=dest.bottom;
    dest.bottom=MENUHEIGHT;
    // copy the region before BeginPaint(), which will reset it
    HRGN hRgn=NULL;
    int region_type=0;
    if(FullScreen) 
    {
      hRgn=CreateRectRgn(0,0,0,0);
      region_type=GetUpdateRgn(Win,hRgn,TRUE); // do draw NC areas
    }
    PAINTSTRUCT ps;
    BeginPaint(Win,&ps);
/*  When a dialog box is moved in the fullscreen GUI, it trashes the background.
    It's no big problem but it looks bad.
    It is possible to redraw the picture by blitting on dirty rectangles.
    In Direct3D, one call is enough:
    Disp.pD3DDevice->Present(NULL,NULL,NULL,lpRgnData);
    Unfortunately, I've only seen it work in Windows 10, not XP nor Vista.
    In DirectDraw, we need to do a blit for each rectangle. It works on
    most systems, but only in flip and straight blit modes, and not with
    Triple Buffering.
    So for a consistent experience, we erase the rectangles in all cases.
*/
    if(FullScreen && OPTION_FULLSCREEN_GUI)
    {
      if(region_type!=NULLREGION && region_type!=ERROR)
      {
        DWORD dwCount=GetRegionData(hRgn,0,NULL); // 1st call to get #bytes
        if(dwCount)
        {
          RGNDATA *lpRgnData=(RGNDATA*)new BYTE[dwCount];
          dwCount=GetRegionData(hRgn,dwCount,lpRgnData); // 2nd call to get rectangles
          if(dwCount)
          {
            HBRUSH br=CreateSolidBrush(GetSysColor(COLOR_BACKGROUND));
            LPRECT pRect=(LPRECT)lpRgnData->Buffer;
            for(DWORD i=0;i<lpRgnData->rdh.nCount;i++)
              FillRect(ps.hdc,&pRect[i],br); // erase all rectangles
            DeleteObject(br);
          } //if(dwCount)
          delete[] lpRgnData;
        }//if(dwCount)
      }
      DeleteObject(hRgn);
    }
#ifndef ONEGAME
    // background for menu bar, must do that AFTER we redraw the 
    // invalidated rectangles in fullscreen mode or we get those stripes...
    HBRUSH br=CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    FillRect(ps.hdc,&dest,br);
    DeleteObject(br);
    // instead of offsetting the blit, we draw a separation line (v4.1)
    HPEN hLinePen=CreatePen(PS_SOLID,0,RGB(0,0,0));
    SelectObject(ps.hdc, hLinePen);
    MoveToEx(ps.hdc,dest.left,dest.bottom-1,NULL);
    LineTo(ps.hdc,dest.right,dest.bottom-1);
    DeleteObject(hLinePen);
#endif
    if(FullScreen)
    {
      int menu_bottom=0 NOT_ONEGAME(+MENUHEIGHT+2);
#ifndef ONEGAME
      dest.bottom=menu_bottom;
      DrawEdge(ps.hdc,&dest,EDGE_RAISED,BF_BOTTOM);
#endif
      int x_gap=0,y_gap=0;
#if defined(SSE_VID_DD)
      if(draw_fs_blit_mode<DFSM_STRETCHBLIT)
      {
#ifndef NO_CRAZY_MONITOR
        if(extended_monitor)
        {
          x_gap=(Disp.SurfaceWidth-em_width)/2;
          y_gap=(Disp.SurfaceHeight-em_height)/2;
        } else
#endif
        if(border)
        {
          x_gap=(800-(SideBorderSizeWin+320+SideBorderSizeWin)*2)/2;
          y_gap=(600-(TopBorderSize*2+400+BottomBorderSize*2))/2;
        }
        else if(draw_fs_topgap)
          y_gap=draw_fs_topgap;
      }
#endif
      br=(HBRUSH)GetStockObject(BLACK_BRUSH);
      RECT rc;
      if(x_gap)
      {
        rc.top=menu_bottom;rc.left=0;rc.bottom=Height;rc.right=x_gap;
        FillRect(ps.hdc,&rc,br);
        rc.left=dest.right-x_gap;rc.right=dest.right;
        FillRect(ps.hdc,&rc,br);
      }
      if(y_gap)
      {
        rc.top=menu_bottom;rc.left=0;rc.bottom=y_gap;rc.right=dest.right;
        FillRect(ps.hdc,&rc,br);
        rc.top=Height-y_gap;rc.bottom=Height;
        FillRect(ps.hdc,&rc,br);
      }
      draw_grille_black=50;
    }
    else // !FullScreen
    {
      dest.top+=MENUHEIGHT;
      dest.bottom=Height-GuiSM.m_statusbar_height;
      //DrawEdge(ps.hdc,&dest,EDGE_SUNKEN,BF_RECT);
      // order of init is changed for 2 monitors compatibility
      if(runstate==RUNSTATE_STOPPED && SSEConfig.IsInit)
      {
        draw_end();
        if(!draw_blit()) //fail
          FillRect(ps.hdc,&dest,(HBRUSH)GetStockObject(BLACK_BRUSH));
      }
      else 
      {
        dest.bottom=MENUHEIGHT;
        FillRect(ps.hdc,&dest,(HBRUSH)GetStockObject(BLACK_BRUSH));
      }
    }
    EndPaint(Win,&ps);
    return 0;
  }

  case (WM_USER+2):   // Update commands
    if(wPar==2323)  // Running?
      return (runstate==RUNSTATE_RUNNING);
#if !defined(SSE_NO_UPDATE)
    if(wPar==54542) {       // New Steem
      UpdateWin=(HWND)lPar;
      ShowWindow(GetDlgItem(Win,120),int(UpdateWin?SW_SHOW:SW_HIDE));
      if(runstate==RUNSTATE_RUNNING && UpdateWin) {
        osd_start_scroller(EasyStr(T("Steem update! Steem version "))+
          GetCSFStr("Update","LatestVersion","1.3",globalINIFile)+" "+
          T("is ready to be downloaded. Click on the new button in the toolbar (to the right of paste) for more details."));
      }
      return 0;
    }
    else if(wPar==12345) { // New Patches
      if(PatchesBox.IsVisible()) {
        PatchesBox.RefreshPatchList();
      }
      else {
        PatchesBox.SetButtonIcon();
      }
      return 0;
    }
#endif
    break;

  case WM_COMMAND:
    if(wpar_lo>=IDC_DISK_MANAGER&&wpar_lo<IDC_LOADSNAPSHOT)
    {
      int NotifyMess=wpar_hi;
      if(NotifyMess==BN_CLICKED)
        HandleButtonMessage(wpar_lo,(HWND)lPar);
      else if(wpar_lo==IDC_FASTFORWARD) 
      {
        if(NotifyMess==BN_PUSHED||NotifyMess==BN_UNPUSHED||NotifyMess==BN_DBLCLK) 
        {
          if(NotifyMess==BN_DBLCLK) 
            fast_forward_stuck_down=true;
          if(fast_forward_stuck_down) 
          {
            if(NotifyMess==BN_UNPUSHED) 
              break;
            if(NotifyMess==BN_PUSHED) 
              NotifyMess=BN_UNPUSHED;  // Click to turn off
          }
          fast_forward_change((NotifyMess!=BN_UNPUSHED),
            (SendMessage((HWND)lPar,BM_GETCLICKBUTTON,0,0)==BM_CLICKBUTTON_RIGHT));
        }
      }
      else if(wpar_lo==IDC_PLAY) 
      {
#if 0 // it's not exactly the same...
        if(NotifyMess==BN_UNPUSHED)
          slow_motion_change(false);
        else if(NotifyMess==BN_PUSHED)
        {
          if(SendMessage((HWND)lPar,BM_GETCLICKBUTTON,0,0)==BM_CLICKBUTTON_RIGHT)
            slow_motion_change(true);
        }
#else
        if(NotifyMess==BN_PUSHED||NotifyMess==BN_UNPUSHED)
        {
          if(SendMessage((HWND)lPar,BM_GETCLICKBUTTON,0,0)==2||NotifyMess==BN_UNPUSHED)
            slow_motion_change(NotifyMess==BN_PUSHED);

        }
#endif
      }
    }
    else if((wpar_lo>=IDC_UNDORESET&&wpar_lo<(IDC_SNAPSHOT_HISTORY+STATE_HISTORY_LEN))
            ||wpar_lo==IDC_DELAY_LOADSNAPSHOT)
    {
      static BYTE state_on_command=(BYTE)runstate;
      if(runstate==RUNSTATE_STOPPED) 
      {
        bool AddToHistory=true;
        if(wpar_lo>=IDC_SNAPSHOT_HISTORY) 
          LastSnapShot=StateHist[wpar_lo-IDC_SNAPSHOT_HISTORY];
        EasyStr fn=LastSnapShot;
        if(wpar_lo==IDC_UNDORESET) 
        {
          CHECK_ALT_MENU
          fn=TempPath+SLASH+FILE_RESETSNAPSHOT;
          AddToHistory=false;
        }
        if(wpar_lo==IDC_UNDOSNAPSHOTLOAD) 
        {
          fn=TempPath+SLASH+FILE_LOADUNDOSNAPSHOT;
          AddToHistory=false;
        }
        if(wpar_lo==IDC_DEFAULTSNAPHOT) // from shortcut
          fn=DefaultSnapshotFile;
        LoadSnapShot(fn,AddToHistory);
        if(wpar_lo==IDC_UNDORESET||wpar_lo==IDC_UNDOSNAPSHOTLOAD) 
          DeleteFile(fn);
        if(state_on_command==RUNSTATE_RUNNING) // if snapshot is loaded when Steem running, keep on running
        {
          TRACEB("START EMU\n");
          CLICK_PLAY_BUTTON()
        }
        ShortcutBox.bSnapshotLoading=false;
      }
      else 
      {
        Glue.m_Status.stop_emu=(OPTION_NO_OSD_ON_STOP) ? 2 : 1;
        PostMessage(Win,Mess,wPar,lPar); // Keep delaying message until stopped
        return 0;
      }
    }
    else if(wpar_lo>=IDC_PASTE_SPEED&&wpar_lo<IDC_PASTE_SPEED+11)
      PasteSpeed=wpar_lo-IDC_PASTE_SPEED+1;
    else if(wpar_lo>=IDC_SCREENSHOT_FORMATS&&wpar_lo<IDC_SCREENSHOT_FORMATS+50) 
    {
      if(wpar_lo<IDC_FI_FORMATS) 
      { // Change screenshot format
        EasyStringList format_sl;
        Disp.ScreenShotGetFormats(&format_sl);
        OptionBox.ChangeScreenShotFormat((int)format_sl[wpar_lo-IDC_SCREENSHOT_FORMATS].Data[0],
                                         format_sl[wpar_lo-IDC_SCREENSHOT_FORMATS].String);
      }
      else if(wpar_lo<IDC_SCREENSHOT_DIR_CHANGE) 
      { // Change screenshot format options
        EasyStringList format_sl;
#if !defined(SSE_NO_FREEIMAGE)
        Disp.ScreenShotGetFormatOpts(&format_sl);
        OptionBox.ChangeScreenShotFormatOpts((int)format_sl[wpar_lo-IDC_FI_FORMATS].Data[0]);
#endif
      }
      else if(wpar_lo==IDC_SCREENSHOT_DIR_CHANGE) // Change folder
        OptionBox.ChooseScreenShotFolder(Win);
      else if(wpar_lo==IDC_SCREENSHOT_DIR_OPEN) // Open folder
        ShellExecute(NULL,NULL,ScreenShotFol,"","",SW_SHOWNORMAL);
      else if(wpar_lo==IDC_SCREENSHOT_MIN) 
      { // Minimum size shots
        Disp.ScreenShotMinSize=!Disp.ScreenShotMinSize;
        if(OptionBox.Handle)
        {
          if(HWND hii=GetDlgItem(OptionBox.Handle,IDC_MINSCREENSHOT))
            SendMessage(hii,BM_SETCHECK,Disp.ScreenShotMinSize,0);
        }
      }
#if defined(SSE_GUI_CONFIG_WRENCH)
/*  v3.8.0 Player has clicked on the 'Configuration' icon, then
    on 'Load configuration file' or 'Save configuration file'.
    Duplicates the options/configurations feature!
*/
      else if(wpar_lo==IDC_LOADCONFIG||wpar_lo==IDC_SAVECONFIG)
      {
        CHECK_ALT_MENU
        char *fstypes=FSTypes(0,T("Configuration files").Text,"*." CONFIG_FILE_EXT,NULL);
        EasyStr FilNam=FileSelect(Win,wpar_lo==IDC_LOADCONFIG
          ? T("Load configuration file") : T("Save configuration file"),
          OptionBox.ProfileDir,fstypes,1,(wpar_lo==IDC_LOADCONFIG),CONFIG_FILE_EXT,"");
        free(fstypes);
        if(FilNam.NotEmpty()) 
        {
          TConfigStoreFile CSF; //on the stack
          bool ok=CSF.Open(FilNam);
          if(wpar_lo==IDC_LOADCONFIG) 
          {
            if(ok)
            {
              OPTION_WS=CSF.GetByte("Machine","WakeUpState",OPTION_WS);
              LoadAllDialogData(false,"",NULL,&CSF); // radical!
              ROMFile=CSF.GetStr("Machine","ROM_File",ROMFile);
              // add current TOS path if necessary
              if(strchr(ROMFile.Text,SLASHCHAR)==NULL) // no slash = no path
              {
                EasyStr tmp=OptionBox.TOSBrowseDir+SLASH+ROMFile;
                ROMFile=tmp;
              }
              Tos.UpdateTOSPath(&ROMFile);
              OptionBox.NewROMFile=ROMFile;
              reset_st(RESET_COLD|RESET_STOP|RESET_CHANGESETTINGS|RESET_BACKUP);
              SetForegroundWindow(StemWin);
            }
            else
              Alert(T("ini file not recognised"),T("ERROR"),MB_ICONERROR);
          }
          else
          {
            SaveAllDialogData(false,"",&CSF); // radical!
            CSF.SetStr("Machine","WakeUpState",EasyStr(OPTION_WS));
          }
          CSF.Close();
        }
      }
#endif//SSE_GUI_CONFIG
    }
    else 
    {
      if(wpar_hi==0) 
      {
        switch(wpar_lo) {
        case IDC_LOADSNAPSHOT:       //Load SnapShot
        case IDC_SAVESNAPSHOT:       //Save SnapShot
        {
          CHECK_ALT_MENU
          EnableAllWindows(false,Win);
          SoundStop();
          int old_runstate=runstate;
          if(FullScreen && runstate==RUNSTATE_RUNNING) 
          {
            runstate=RUNSTATE_STOPPED;
            Disp.FullscreenRunEnd();
            UpdateWindow(StemWin);
          }
          EasyStr FilNam;
          Str LastStateFol=LastSnapShot;
          RemoveFileNameFromPath(LastStateFol,REMOVE_SLASH);
#ifdef SSE421R1 // opt. strings + calls
          char sbuf[64];
          sprintf(sbuf,"%s %s","Steem","Memory Snapshots");
          char *fstypes=FSTypes(0,T(sbuf).Text,"*.sts",NULL);
          bool bLoading=(wpar_lo==IDC_LOADSNAPSHOT);
          sprintf(sbuf,"%s %s",bLoading?"Load":"Save","Memory Snapshot");
          FilNam=FileSelect(Win,T(sbuf),LastStateFol,fstypes,1,
                            bLoading,"sts",GetFileNameFromPath(LastSnapShot));
#else 
          char *fstypes=FSTypes(0,T("Steem Memory Snapshots").Text,"*.sts",NULL);
          if(wpar_lo==IDC_LOADSNAPSHOT) 
          {
            FilNam=FileSelect(Win,T("Load Memory Snapshot"),LastStateFol,
              fstypes,1,TRUE,"sts",GetFileNameFromPath(LastSnapShot));
          }
          else 
          {
            FilNam=FileSelect(Win,T("Save Memory Snapshot"),LastStateFol,
              fstypes,1,FALSE,"sts",GetFileNameFromPath(LastSnapShot));
          }
#endif
          free(fstypes);
          if(FilNam.NotEmpty()) 
          {
            if(SnapShotGetLastBackupPath().NotEmpty())
              DeleteFile(SnapShotGetLastBackupPath());
            LastSnapShot=FilNam;
            if(wpar_lo==IDC_LOADSNAPSHOT) 
            {
              if(old_runstate==RUNSTATE_STOPPED)
                LoadSnapShot(LastSnapShot);
              else 
              {
                old_runstate=RUNSTATE_STOPPING;
                PostMessage(Win,WM_COMMAND,IDC_DELAY_LOADSNAPSHOT,lPar); // Delay load until stopped
              }
            }
            else
              SaveSnapShot(LastSnapShot,-1);
          }
          SetForegroundWindow(Win);
          runstate=old_runstate;
          if(FullScreen && runstate==RUNSTATE_RUNNING)
            Disp.FullscreenRunStart();
          timer=timeGetTime();
          avg_frame_time_timer=timer;
          avg_frame_time_counter=0;
          auto_frameskip_target_time=timer;
          SoundStart();
          EnableAllWindows(true,Win);
          break;
        }
        case IDC_SNAPSHOTSAVEOVER:
          if(SnapShotGetLastBackupPath().NotEmpty()) 
          {
            DeleteFile(SnapShotGetLastBackupPath());
            MoveFile(LastSnapShot,SnapShotGetLastBackupPath()); // Make backup
          }
          SaveSnapShot(LastSnapShot,-1);
          break;
        case IDC_UNDOSNAPSHOTSAVEOVER:
          // Restore backup, can only get here if backup path is valid
          DeleteFile(LastSnapShot);
          MoveFile(SnapShotGetLastBackupPath(),LastSnapShot);
          break;
#if defined(SSE_GUI_MENUBAR)
        case IDC_MENURUNSTOP:
          CLICK_PLAY_BUTTON();
          CHECK_ALT_MENU
          break;
        case IDC_MENUDISKMAN:
          DiskMan.Show();
          CHECK_ALT_MENU
          break;
        case IDC_MENUPATCHESBOX:
          PatchesBox.Show();
          CHECK_ALT_MENU
          break;
#if defined(SSE_EMU_THREAD)
        case IDC_MENUKILLTHREAD:
          if(OPTION_EMUTHREAD&&runstate!=RUNSTATE_STOPPED&&hEmuThread)
          {// kill thread - radical
            TRACE("kill %s $%X\n","thread",EmuThreadId);
            TerminateThread(hEmuThread,0);
            hEmuThread=NULL;
            runstate=RUNSTATE_STOPPED;
            SendMessage(GetDlgItem(Win,IDC_PLAY),BM_SETCHECK,0,0); // reset play button
            EnableMenuItem(StemWinMenu,wpar_lo,MF_DISABLED);
          }
          CHECK_ALT_MENU
          break;
#endif
        case IDC_MENUTOGGLEFULLSCREEN:
          CHECK_ALT_MENU
          if(FullScreen)
          {
            bool emergency=(StatusInfo.MessageIndex==TStatusInfo::BLIT_ERROR);
            Disp.ChangeToWindowedMode(emergency);
          }
          else
            PostMessage(StemWin,WM_SYSCOMMAND,SC_MAXIMIZE,2);
          break;
        case IDC_MENUEXIT:
          PostMessage(Win,WM_CLOSE,0,0);
          CHECK_ALT_MENU
          break;
        case IDC_MENUREBOOT:
          reset_st(RESET_COLD|RESET_NOSTOP|RESET_CHANGESETTINGS|RESET_BACKUP|RESET_COUNT);
          CHECK_ALT_MENU
          break;
        case IDC_MENURESET:
          reset_st(RESET_WARM|RESET_NOSTOP|RESET_BACKUP|RESET_COUNT);
          CHECK_ALT_MENU
          break;
        case IDC_MENUOPTIONS:
          OptionBox.Show();
          CHECK_ALT_MENU
          break;
        case IDC_MENUINSERTA:
        case IDC_MENUINSERTB:
        {
          char *fstypes=FSTypes(2,NULL);
          EasyStr path=FileSelect(NULL,T("Select Disk Image"),DiskMan.DisksFol,fstypes,1,true,"");
          free(fstypes);
          EasyStr name=GetFileNameFromPath(path);
          DiskMan.InsertDisk(wpar_lo-IDC_MENUINSERTA,name,path,false,false,"",true); // can be ""
          CHECK_ALT_MENU
          break;
        }
        case IDC_MENUGEMDOSHD:
          HardDiskMan.Show();
          CHECK_ALT_MENU
          break;
#if defined(SSE_ACSI_MNGR)
        case IDC_MENUACSIHD:
          AcsiHardDiskMan.Show();
          CHECK_ALT_MENU
          break;
#endif
#if defined(SSE_DISK_SWAPPER)
        case IDC_MENUSWAPPERPREV:
          DiskMan.ChangeDisk(DRIVE_A,-1,TRUE); // get previous disk
          CHECK_ALT_MENU
          break;
        case IDC_MENUSWAPPERNEXT:
          DiskMan.ChangeDisk(DRIVE_A,1,TRUE); // get next disk
          CHECK_ALT_MENU
          break;
#endif
        case IDC_MENUSHORTCUTS:
          ShortcutBox.Show();
          CHECK_ALT_MENU
          break;
        case IDC_MENUJOYSTICKS:
          JoyConfig.Show();
          CHECK_ALT_MENU
          break;
        case IDC_MENUINFO:
          InfoBox.Show();
          CHECK_ALT_MENU
          break;
#if defined(SSE_DEBUGGER_TOGGLE)
        case IDC_MENUDEBUGGER: // but the Debugger itself needs the mouse AFAIK
          ShowWindow(DWin,SW_SHOW);
          CHECK_ALT_MENU
          break;
#endif
#endif
#if defined(SSE_GUI_TOOLBAR)
        case IDC_MENUTOOLBAR:
          OPTION_TOOLBAR=!OPTION_TOOLBAR;
          OptionBox.SSEUpdateIfVisible();
#if defined(SSE_EMU_THREAD)
          if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
            VideoLock.Lock();
#endif
#ifdef SSE_421R0
          GuiSM.Update();
#endif
          StemWinResize();    
#if defined(SSE_EMU_THREAD)
          VideoLock.Unlock();
#endif
          CHECK_ALT_MENU
          break;
#endif
        }//sw
      }//if
    }//if
    break;

  case WM_USER: // magic values
    switch(wPar) {
    case 1234:
#ifndef ONEGAME
      SendMessage(GetDlgItem(Win,IDC_DISK_MANAGER),BM_SETCHECK,DiskMan.IsVisible(),0);
      SendMessage(GetDlgItem(Win,IDC_JOYSTICKS),BM_SETCHECK,JoyConfig.IsVisible(),0);
      SendMessage(GetDlgItem(Win,IDC_INFO),BM_SETCHECK,InfoBox.IsVisible(),0);
      SendMessage(GetDlgItem(Win,IDC_OPTIONS),BM_SETCHECK,OptionBox.IsVisible(),0);
      SendMessage(GetDlgItem(Win,IDC_SHORTCUTS),BM_SETCHECK,ShortcutBox.IsVisible(),0);
      SendMessage(GetDlgItem(Win,IDC_PATCHES),BM_SETCHECK,PatchesBox.IsVisible(),0);
#if defined(SSE_DEBUGGER_TOGGLE)
      SendMessage(GetDlgItem(Win,IDC_DEBUGGER),BM_SETCHECK,DebuggerVisible,0);
#endif
#endif
      break;
    case 12345:
      if(DisableFocusWin) 
        SetForegroundWindow(DisableFocusWin);
      break;
    case 123:
    {
      // Allows external programs to press ST keys
      WORD VKCode=lpar_lo;
      int ChangeModMask;
      switch(VKCode) {
      case VK_SHIFT:
        ChangeModMask=b00000011;
        break;
      case VK_LSHIFT:
        ChangeModMask=b00000001;
        break;
      case VK_RSHIFT:
        ChangeModMask=b00000010;
        break;
      case VK_CONTROL:
      case VK_LCONTROL:
      case VK_RCONTROL:   
        VKCode=VK_CONTROL; 
        ChangeModMask=b00001100;
        break;
      case VK_LMENU:
      case VK_RMENU:
      case VK_MENU:
        VKCode=VK_MENU;
        ChangeModMask=b00110000;
        break;
      default:
        ChangeModMask=0;
      }
      if(lpar_hi)
        ExternalModDown&=~ChangeModMask;
      else
        ExternalModDown|=ChangeModMask;
      if(VKCode==VK_SHIFT) 
      {
        if(ST_Key_Down[key_table[VK_LSHIFT]]!=!lpar_hi) 
          HandleKeyPress(VK_LSHIFT,lpar_hi,IGNORE_EXTEND|NO_SHIFT_SWITCH);
        if(ST_Key_Down[key_table[VK_RSHIFT]]!=!lpar_hi) 
          HandleKeyPress(VK_RSHIFT,lpar_hi,IGNORE_EXTEND|NO_SHIFT_SWITCH);
      }
      else
        HandleKeyPress(VKCode,lpar_hi,IGNORE_EXTEND|NO_SHIFT_SWITCH);
      break;
    }
    case 12:
    {// Return from fullscreen
      SetWindowLong(StemWin,GWL_STYLE,WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX
        |WS_SIZEBOX|WS_MAXIMIZEBOX|WS_VISIBLE);
      if((GetWindowLong(StemWin,GWL_STYLE) & WS_SIZEBOX)==0
        && timeGetTime()<Disp.ChangeToWinTimeOut)
      {
        PostMessage(StemWin,WM_USER,12,0);
        break;
      }
      SetWindowLong(DiskMan.Handle,GWL_STYLE,(GetWindowLong(DiskMan.Handle,
        GWL_STYLE) & ~WS_MAXIMIZE)|WS_MINIMIZEBOX);
      bool MaximizeDiskMan=DiskMan.Maximized && DiskMan.IsVisible();
      for(int n=0;n<nStemDialogs;n++) 
      {
        DEBUG_ONLY(if(DialogList[n]!=&HistList)) 
          DialogList[n]->MakeParent(NULL);
      }
      SetParent(ToolTip,NULL);
      if(!Disp.BorderPossible())
      {
        border=0;
        OptionBox.EnableBorderOptions(FALSE);
      }
      if(MaximizeDiskMan) 
        ShowWindow(DiskMan.Handle,SW_MAXIMIZE);
      SendMessage(StemWin,WM_USER,13,0);
      break;
    }
    case 13:
    { // Return from fullscreen
#if defined(SSE_NO_AOT)
      SetWindowPos(StemWin,(HWND)(HWND_NOTOPMOST),rcPreFS.left,rcPreFS.top,
        rcPreFS.right-rcPreFS.left,rcPreFS.bottom-rcPreFS.top,0);
#else
      SetWindowPos(StemWin,(HWND)((bAlwaysOnTop)?HWND_TOPMOST:HWND_NOTOPMOST),rcPreFS.left,
        rcPreFS.top,rcPreFS.right-rcPreFS.left,rcPreFS.bottom-rcPreFS.top,0);
#endif
      UpdateWindow(StemWin);
      RECT rc;
      GetWindowRect(StemWin,&rc);
      if(!EqualRect(&rc,&rcPreFS)&&timeGetTime()<Disp.ChangeToWinTimeOut) 
      {
        PostMessage(StemWin,WM_USER,13,0);
        break;
      }
      SetWindowLong(StemWin,GWL_STYLE,WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX
        |WS_SIZEBOX|WS_MAXIMIZEBOX|WS_VISIBLE);
      SetForegroundWindow(StemWin);
      CheckResetDisplay();
      InvalidateRect(StemWin,NULL,TRUE);
      palette_convert_all();
      draw(true);
      if(Disp.RunOnChangeToWindow) 
      {
        CLICK_PLAY_BUTTON();
        Disp.RunOnChangeToWindow=false;
      }
      break;
    }
#if defined(SSE_EMU_THREAD) && defined(SSE_GUI_STATUS_BAR)
    case 17:
      GUIRefreshStatusBar2((BYTE)lPar);
      break;
#endif
    }//sw
    break;

  case WM_NCLBUTTONDBLCLK:
    if(wPar==HTCAPTION) 
    {
      PostMessage(Win,WM_SYSCOMMAND,(WPARAM)(IsZoomed(Win) ? SC_RESTORE : SC_MAXIMIZE),0);
      return 0;
    }
    break;

  case WM_SYSCOMMAND:
    if(wPar>=IDSYS_NORMAL&&wPar<IDSYS_BORDEROFF
#if !defined(SSE_NO_AOT)
      &&wPar!=IDSYS_TOP
#endif
      )
    {
      if(IsZoomed(Win)) 
        ShowWindow(Win,SW_SHOWNORMAL);
    }
    switch(wPar) {
#if defined(SSE_GUI_MENUBAR)
    case IDC_MENUBAR: // from ALT menu, player toggles the menu bar option
      if(AltMenuOn)
        SteemSetMenu(false);
      AltMenuOn=false;
      OPTION_MENUBAR=!OPTION_MENUBAR;
      SteemSetMenu(OPTION_MENUBAR);
      return 0;
#endif
    case IDSYS_NORMAL: //1:1
    {
#if defined(SSE_EMU_THREAD)
      bool oldSuspendRendering=SuspendRendering;
      SuspendRendering=true;
#endif
      DISPLAY_SIZE=2;
      WinSizeForRes[screen_res]=1;
      StemWinResize();
#if defined(SSE_EMU_THREAD)
      SuspendRendering=oldSuspendRendering;
#endif
      if(runstate==RUNSTATE_STOPPED)
        draw(false);
      return 0;
    }
    case IDSYS_ASPECT: //Aspect Ratio
    {
#if defined(SSE_EMU_THREAD)
      bool oldSuspendRendering=SuspendRendering;
      SuspendRendering=true;
#endif
      RECT rc;
      GetClientRect(Win,&rc);
      //double ratio;
      int res=(video_mixed_output) ? MEDRES : screen_res;
      int Idx=WinSizeForRes[res];
      double ratio=(border)?(double)(WinSizeBorder[res][Idx].x)/(double)(WinSizeBorder[res][Idx].y)
                           :(double)(WinSize[res][Idx].x)/(double)(WinSize[res][Idx].y);;
      /* if(border)
        ratio=(double)(WinSizeBorder[res][Idx].x)/(double)(WinSizeBorder[res][Idx].y);
      else
        ratio=(double)(WinSize[res][Idx].x)/(double)(WinSize[res][Idx].y); */
      double sz=(rc.right/ratio+((double)rc.bottom-((double)MENUHEIGHT
        +(double)GuiSM.m_statusbar_height)))/2.0;
      SetStemWinSize((int)(sz*ratio+0.5),(int)(sz+0.5));
#if defined(SSE_EMU_THREAD)
      SuspendRendering=oldSuspendRendering;
#endif
      return 0;
    }
#if !defined(SSE_NO_AOT)
    case IDSYS_TOP:
      bAlwaysOnTop=!bAlwaysOnTop;
      CheckMenuItem(StemWin_SysMenu,(UINT)wPar, MF_BYCOMMAND|MF_CHECK(bAlwaysOnTop));
      SetWindowPos(StemWin,(HWND)((bAlwaysOnTop)?HWND_TOPMOST:HWND_NOTOPMOST),0,0,0,0,
        SWP_NOMOVE|SWP_NOSIZE);
      return 0;
#endif
    case IDSYS_BIGGER: //bigger window
    case IDSYS_SMALLER: //smaller window
      if(ResChangeResize) 
      {
#if defined(SSE_EMU_THREAD)
        bool oldSuspendRendering=SuspendRendering;
        SuspendRendering=true;
#endif
        BYTE res=video_mixed_output ? MEDRES : screen_res;
        BYTE size=WinSizeForRes[res];
        if(wPar==IDSYS_BIGGER) 
        {
          if(size<3)
            size++;
        }
        else
        {
          if(size>0)
            size--;
        }
        WinSizeForRes[res]=size;
        StemWinResize();
        OptionBox.UpdateWindowSizeAndBorder();
#if defined(SSE_EMU_THREAD)
        SuspendRendering=oldSuspendRendering;
#endif
      }
      else 
      {
        RECT rc;
        GetClientRect(StemWin,&rc);
        if(wPar==IDSYS_BIGGER) 
        {
          rc.right=rc.right*14142/10000;
          rc.bottom=rc.bottom*14142/10000;
        }
        else 
        {
          rc.right=rc.right*10000/14142;
          rc.bottom=rc.bottom*10000/14142;
        }
        SetStemWinSize(rc.right,rc.bottom);
      }
      if(runstate==RUNSTATE_STOPPED)
        draw(false);
      return 0;
    case IDSYS_BORDEROFF:case IDSYS_BORDERON: //Borders
      OptionBox.SetBorder((int)(wPar-IDSYS_BORDEROFF)); // this includes VideoLock
      OptionBox.UpdateWindowSizeAndBorder();
      CheckMenuRadioItem(StemWin_SysMenu,IDSYS_BORDEROFF,IDSYS_BORDERON,
        IDSYS_BORDEROFF+MIN((int)border,1),MF_BYCOMMAND);
      return 0;
#if defined(SSE_GUI_TOOLBAR)
    case IDSYS_TOOLBAR:
      OPTION_TOOLBAR=!OPTION_TOOLBAR;
      CheckMenuItem(StemWin_SysMenu,IDSYS_TOOLBAR,MF_BYCOMMAND|MF_CHECK(OPTION_TOOLBAR));
      OptionBox.SSEUpdateIfVisible();
#if defined(SSE_EMU_THREAD)
      if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
        VideoLock.Lock();
#endif
#ifdef SSE_421R0
      GuiSM.Update();
#endif
      StemWinResize();    
#if defined(SSE_EMU_THREAD)
      VideoLock.Unlock();
#endif
      return 0;
    case IDSYS_RUNSTOP:
      CLICK_PLAY_BUTTON();
      return 0;
#endif
#ifndef SSE_NO_OSD
    case IDSYS_NOOSD:
      OptionBox.ChangeOSDDisable(!OsdControl.disable);
      return 0;
#endif
    case IDSYS_POSITION:  // panic windows are out of screen! (shouldn't happen...)
    {
      WINDOWPLACEMENT wp;
      wp.length=sizeof(WINDOWPLACEMENT);
      GetWindowPlacement(StemWin,&wp);
      LONG w=wp.rcNormalPosition.right-wp.rcNormalPosition.left;
      LONG h=wp.rcNormalPosition.bottom-wp.rcNormalPosition.top;
      wp.rcNormalPosition.left=100; // assume primary (0,0)
      wp.rcNormalPosition.right=wp.rcNormalPosition.left+w;
      wp.rcNormalPosition.top=100;
      wp.rcNormalPosition.bottom=wp.rcNormalPosition.top+h;
      SetWindowPlacement(StemWin,&wp);
      for(int n=0;n<nStemDialogs;n++) 
      {
        if(DialogList[n]->IsVisible())
        {
          GetWindowPlacement(DialogList[n]->Handle,&wp);
          w=wp.rcNormalPosition.right-wp.rcNormalPosition.left;
          h=wp.rcNormalPosition.bottom-wp.rcNormalPosition.top;
          wp.rcNormalPosition.left=100+n*20; // assume primary (0,0)
          wp.rcNormalPosition.right=wp.rcNormalPosition.left+w;
          wp.rcNormalPosition.top=100+n*20;
          wp.rcNormalPosition.bottom=wp.rcNormalPosition.top+h;
          SetWindowPlacement(DialogList[n]->Handle,&wp);
        }
        else
          DialogList[n]->Left=DialogList[n]->Top=100+n*20;
      }
      return 0;
    }
    }//sw
    switch(wPar&0xFFF0) {
    case SC_MAXIMIZE:
      // when Steem posts the message itself, it sets lParam to 2
      // which means 'fullscreen, not maximize'
      // if the message comes from Windows (click on maximize), it
      // will be 0 and then what we do depends on the option (v4.0.1)
      if((OPTION_MAX_FS || lPar==2) && Disp.CanGoToFullScreen())
      {
        if(runstate==RUNSTATE_RUNNING)
          Disp.RunOnChangeToWindow=true;
        Disp.ChangeToFullScreen(); // fullscreen
        return 0;
      }
      break; // maximize
    case SC_MONITORPOWER:
      if(runstate==RUNSTATE_RUNNING) 
        return 0;
      break;
    case SC_SCREENSAVE:
      // this prevents screensaver from activating but only if Steem has 
      // focus, else we don't get this message
      if(runstate==RUNSTATE_RUNNING||FullScreen) 
        return 0;
      break;
    case SC_TASKLIST:case SC_PREVWINDOW:case SC_NEXTWINDOW:
      if(runstate==RUNSTATE_RUNNING) 
        return 0;
      break;
    }//sw
    break;

  case WM_KEYDOWN:
  case WM_KEYUP:
  case WM_SYSKEYDOWN:case WM_SYSKEYUP:
  {
    SHORT vk_menu_state=GetKeyState(VK_MENU);
    SHORT vk_control_state=GetKeyState(VK_CONTROL);
    SHORT vk_shift_state=GetKeyState(VK_SHIFT);
    if(!bAppActive)
      return 0;
#if defined(SSE_GUI_MENUBAR)
    if(Mess==WM_SYSKEYUP && wPar==VK_MENU && runstate!=RUNSTATE_RUNNING)
    { // maybe player pressed ALT to toggle the menu or maybe to control the permanent menu
      if(!OPTION_MENUBAR)
      {
        AltMenuOn=!AltMenuOn;
        SteemSetMenu(AltMenuOn);
        if(AltMenuOn)
          return DefWindowProc(Win,Mess,wPar,lPar);
      }
    }
    if(runstate==RUNSTATE_RUNNING&&(wPar==VK_SHIFT||wPar==VK_CONTROL||wPar==VK_MENU)) 
      return 0; // when running, keep Steem's original system
#else
    if(wPar==VK_SHIFT||wPar==VK_CONTROL||wPar==VK_MENU) return 0;
#endif
#ifndef ONEGAME
    if(TaskSwitchDisabled)
    {
      int n=0;
      while(TaskSwitchVKList[n])
        if((BYTE)wPar==TaskSwitchVKList[n++])
          return 0;
    }
#endif
//#if defined(SSE_GUI_STATUS_BAR)
    else
    {
      switch(wPar) {
      case VK_NUMLOCK:
      case VK_SCROLL:
        SSEConfig.MaxJoy=MAX_ST_JOYS-1; // stop optimisation until next check
        // no break;
      case VK_CAPITAL:
        UPDATE_STATUS_BAR_PART(SB_PART_CAPS);
        break;
      }//sw
    }
//#endif

#ifdef SSE421R2 // no running condition
    if (Mess==WM_SYSKEYUP && wPar==VK_RETURN &&
      GetForegroundWindow()==Win && GetAsyncKeyState(VK_MENU)<0)
    { // alt-enter
      if(FullScreen)
        Disp.ChangeToWindowedMode();
      else
        Disp.ChangeToFullScreen();
    }
#endif


    if(runstate==RUNSTATE_RUNNING)
    {
#ifndef ONEGAME
      // stop on F12 or Pause, no modifiers
      if((wPar==VK_F12&&SSEOptions.F12Run||wPar==VK_PAUSE&&SSEOptions.PauseRun) &&
        vk_control_state>=0&&vk_menu_state>=0&&vk_shift_state>=0)
      {
        if(!(Mess==WM_KEYUP||Mess==WM_SYSKEYUP))
        {} // return for down -> key not seen by ST
        else
#if defined(SSE_EMU_THREAD)
        if(OPTION_EMUTHREAD&&Glue.m_Status.stop_emu)
        {
          if(Alert(T("The emulation thread isn't responding. Kill it?"),
            T(STEEM_CRASH_TXT),MB_ICONQUESTION|MB_YESNO)==IDYES)
          {
            TRACE("kill %s $%X\n","thread",EmuThreadId);
            TerminateThread(hEmuThread,0);
            hEmuThread=NULL;
            GUIRunEnd();            
            runstate=RUNSTATE_STOPPED;
          }
        }
        else
#endif
        {
          Glue.m_Status.stop_emu=(OPTION_NO_OSD_ON_STOP) ? 2 : 1;
#if defined(SSE_DEBUGGER_FRAME_REPORT)
          FrameEvents.Report();
#endif
        }
        return 0;
      }
      // toggle mouse capture on F11 (no modifiers) or shift Pause
      if((wPar==VK_F11&&vk_shift_state>=0&&SSEOptions.F12Run
        ||wPar==VK_PAUSE&&vk_shift_state<0&&SSEOptions.PauseRun)
        &&vk_control_state>=0&&vk_menu_state>=0)
      {
        if(Mess==WM_KEYUP||Mess==WM_SYSKEYUP)
        {
          if(stem_mousemode==STEM_MOUSEMODE_DISABLED) 
          {
            SetForegroundWindow(StemWin);
            SetStemMouseMode(STEM_MOUSEMODE_WINDOW);
            if(OPTION_CAPTURE_MOUSE&6) // auto
              OPTION_CAPTURE_MOUSE|=BIT_0; // make it sticky
          }
          else
          {
            SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
            if(OPTION_CAPTURE_MOUSE&6) // auto
              OPTION_CAPTURE_MOUSE&=6;
          }
        }
        return 0;
      }
#else //ONEGAME
      if(wPar==VK_PAUSE||wPar==VK_F12||wPar==VK_ESCAPE) {
        if(Mess==WM_KEYUP||Mess==WM_SYSKEYUP) {
          if(runstate==RUNSTATE_RUNNING) {
            OGStopAction=OG_QUIT;
            runstate=RUNSTATE_STOPPING;
          }
        }
      }
#endif
      else if(Mess==WM_KEYUP||Mess==WM_SYSKEYUP||(lPar&0x40000000)==0) 
      {
/*
24 Indicates whether the key is an extended key, such as the right-hand ALT 
and CTRL keys that appear on an enhanced 101- or 102-key keyboard. The
 value is 1 if it is an extended key; otherwise, it is zero. 
*/
        bool Extended=((lPar&BIT_24)!=0);
        GetRealVKCodeForKeypad(wPar,lPar,Extended);
        if(!joy_is_key_used((BYTE)wPar) && !CutDisableKey[(BYTE)wPar]) 
          HandleKeyPress((UINT)wPar,Mess==WM_KEYUP||Mess==WM_SYSKEYUP,Extended);
      }
    }
    else if(runstate==RUNSTATE_STOPPED)
    {
#if !defined(SSE421R2)
      if(Mess==WM_SYSKEYUP && wPar==VK_RETURN &&
        GetForegroundWindow()==Win && GetAsyncKeyState(VK_MENU)<0) 
      { // alt-enter
        if(FullScreen)
          Disp.ChangeToWindowedMode();
        else if(Disp.CanGoToFullScreen()) 
          Disp.ChangeToFullScreen();
      }
      // run on F12, Pause or Ctrl Break
      else 
#endif
        if( (Mess==WM_KEYUP||Mess==WM_SYSKEYUP)
        && (vk_shift_state>=0&&vk_control_state>=0&&vk_menu_state>=0
        && (wPar==VK_F12&&SSEOptions.F12Run || wPar==VK_PAUSE&&SSEOptions.PauseRun)) // F12, Pause
        || wPar==VK_CANCEL) // Ctrl Break
      {
        CLICK_PLAY_BUTTON(); // it's a macro
      }
      // leave on Alt F4 or Ctrl W
      else if((Mess==WM_SYSKEYDOWN && wPar==VK_F4)
#ifdef DEADC0DE // not Ctrl-W
        ||(Mess==WM_KEYDOWN && wPar=='W' && vk_control_state<0)
#endif
        )
      {
        PostMessage(Win,WM_CLOSE,0,0);
      }
#if defined(SSE_GUI_KBD) // F1 for help
      else if(wPar==VK_F1 && (vk_shift_state|vk_control_state|vk_menu_state)>=0)
      {
        /*if(InfoBox.IsVisible())
        {
          SetFocus(InfoBox.Handle);
          return 0;
        }*/
        InfoBox.Hide();
        InfoBox.Page=TGeneralInfo::ABOUT;
        InfoBox.Show();
      }
#endif
#if defined(SSE_GUI_MENUBAR)
      else
        return DefWindowProc(Win,Mess,wPar,lPar);
#endif
    }
    return 0;
  }

  case WM_SYSCHAR:
#if defined(SSE_GUI_KBD) // window menu on alt+space
    if(runstate==RUNSTATE_STOPPED && GetKeyState(VK_MENU)<0)
    {
      if(wPar==VK_SPACE)
        return DefWindowProc(Win,Mess,wPar,lPar);
    }
    //no break
#endif

  case WM_SYSDEADCHAR:
    return 0;

  case WM_LBUTTONDOWN:case WM_LBUTTONUP:
  case WM_RBUTTONDOWN:case WM_RBUTTONUP:
#if !defined(SSE_DBG_NOMONITORSCREEN)
#ifdef DEBUG_BUILD
    if(runstate==RUNSTATE_STOPPED && stem_mousemode==STEM_MOUSEMODE_BREAKPOINT) 
    {
      if(wPar & MK_LBUTTON)
      {
        int x=GET_X_LPARAM(lPar)-2,y=GET_Y_LPARAM(lPar)-2-MENUHEIGHT;
        x&=0xfffffff0; //16 pixels per raster
        x/=16;         //to raster number
        MEM_ADDRESS ad=vbase;
        if(screen_res==HIRES)
          ad+=y*80;
        else ad+=y*160;
        if(screen_res==LORES)
          ad+=x*8;
        else if(screen_res==MEDRES)
          ad+=x*4;
        else ad+=x*2;
        d2_dpoke(ad,0xface);
        debug_set_mon(ad,0,0xffff);
        SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
      }
    }
#endif
#endif
    if(lpar_hi>MENUHEIGHT && stem_mousemode==STEM_MOUSEMODE_DISABLED) 
    {
      if(GetForegroundWindow()==StemWin) 
      {
        if(runstate==RUNSTATE_RUNNING)
        {
          if(OPTION_CAPTURE_MOUSE)
          {
            if(OPTION_CAPTURE_MOUSE&6) // auto
              OPTION_CAPTURE_MOUSE|=BIT_0; // make it sticky
            SetStemMouseMode(STEM_MOUSEMODE_WINDOW);
            disable_input_vbl_count=20; // eat first click
          }
        }
        else if(StartEmuOnClick) 
        {
          CLICK_PLAY_BUTTON();
        }
        return 0; //If an application processes this message, it should return zero.
      }
    }
    break;

  case WM_MOUSEWHEEL:
    MouseWheelMove+=(short)wpar_hi;
    return 0;

  case WM_TIMER:
    if(wPar==SHORTCUTS_TIMER_ID) 
    {
      if(NumJoysticks && bAppActive)
        JoyGetPoses();
      ShortcutsCheck();
    }
    else if(wPar==DISPLAYCHANGE_TIMER_ID) 
    {
      KillTimer(Win,DISPLAYCHANGE_TIMER_ID);
      TConfigStoreFile CSF(globalINIFile);
      LoadAllIcons(&CSF);
      CSF.Close();
    }
#if defined(SSE_GUI_STATUS_BAR)
    else if(wPar==STATUSBAR_TIMER_ID) 
    {
      KillTimer(Win,STATUSBAR_TIMER_ID);
      if(StatusInfo.MessageIndex==TStatusInfo::MESSAGE_MISC)
        StatusInfo.MessageIndex=TStatusInfo::MESSAGE_NONE;
#ifndef SSE_NO_OSD
      OsdControl.bPrinting=false;
#endif
      GUIRefreshStatusBar((1<<SB_PART_MAIN)|(1<<SB_PART_CAPS));
    }
#endif
    break;

  case WM_COPYDATA:
  case WM_DROPFILES:
  {
    EasyStr *Files=NULL;
    char **lpFile;
    int nFiles;
    if(Mess==WM_COPYDATA) 
    {
      COPYDATASTRUCT *cds=(COPYDATASTRUCT*)lPar;
      if(cds->dwData!=MAKECHARCONST('S','C','O','M'))
        break;  // Not Steem comline file
      nFiles=1;
      lpFile=(char**)&(cds->lpData); // lpFile is an array of nFiles pointers to char*s, cds->lpData is a char*
    }
    else 
    {
      nFiles=DragQueryFile((HDROP)wPar,0xffffffff,NULL,0);
      Files=new EasyStr[nFiles];
      lpFile=new char*[nFiles];
      for(int i=0;i<nFiles;i++)
      {
        Files[i].SetLength(MAX_PATH);
        DragQueryFile((HDROP)wPar,i,Files[i],MAX_PATH);
        lpFile[i]=Files[i].Text;
      }
      DragFinish((HDROP)wPar);
    }
    EasyStr OldDiskA=FloppyDrive[DRIVE_A].GetDisk();
    BootStateFile="";
    BootTOSImage=false;
    BootDisk[DRIVE_A]=BootDisk[DRIVE_B]="";
    BootInMode=0;
    ParseCommandLine(nFiles,lpFile);
    if(BootStateFile.NotEmpty()) 
    {
      if(LoadSnapShot(BootStateFile)) 
      {
        SetForegroundWindow(Win);
        CLICK_PLAY_BUTTON();
      }
    }
    else 
    {
      for(int d=DRIVE_A;d<=DRIVE_B;d++) 
      {
        if(BootDisk[d].NotEmpty()) 
        {
          EasyStr Name=GetFileNameFromPath(BootDisk[d]);
          *strrchr(Name,'.')='\0';
          DiskMan.InsertDisk(d,Name,BootDisk[d],false,false);
        }
      }
      bool ChangedDisk=NotSameStr_I(OldDiskA,FloppyDrive[DRIVE_A].GetDisk());
      if(BootTOSImage||ChangedDisk) 
      {
        SetForegroundWindow(Win);
        reset_st(RESET_COLD|(DWORD)(ChangedDisk?RESET_NOSTOP:RESET_STOP)
                 |RESET_CHANGESETTINGS|RESET_BACKUP|RESET_COUNT);
        if(ChangedDisk && runstate!=RUNSTATE_RUNNING)
        {
          CLICK_PLAY_BUTTON();
        }
      }
      else if(BootInMode & BOOT_MODE_RUN) 
      {
        if(runstate==RUNSTATE_STOPPED)
        {
          CLICK_PLAY_BUTTON();
        }
      }
    }
    if(Mess==WM_COPYDATA) 
      return MAKECHARCONST('Y','A','Y','S');
    delete[] Files;
    delete[] lpFile;
    return 0;
  }

  case WM_GETMINMAXINFO:
    ((MINMAXINFO*)lPar)->ptMinTrackSize.x=HOR_PIXELS_LO+GuiSM.cx_frame()*2;
    ((MINMAXINFO*)lPar)->ptMinTrackSize.y=VER_PIXELS_LO+GuiSM.cy_frame()*2
      +GuiSM.cy_caption()+MENUHEIGHT+GuiSM.m_statusbar_height;
    break;

/*  v3.7
    Prevent player from resizing the window by dragging the border.
    Optional because stretching is cool and handy too.
    We pretend the mouse is on the client area, so the resizing cursor
    won't even appear.
    All border values are between HTLEFT and HTBOTTOMRIGHT.
    Returning HTCLIENT all the time would work with Windows 7 but not Vista
    (can't move or close window).
    v4.1.0: this is defeated by the status bar!
*/
  /*case WM_NCHITTEST:
  {
    LRESULT val=DefWindowProc(Win,Mess,wPar,lPar); // real area
    if(OPTION_BLOCK_RESIZE && val>=HTLEFT && val<=HTBOTTOMRIGHT)
      val=HTCLIENT;
    return val;
  }*/
/*  v3.7
    if option above isn't checked, this one enforces a correct aspect ratio
    lPar points to the absolute resizing rectangle, its values may be changed
    GetWindowRect() gives the current rectangle of the window, hopefully the
    same concept.
*/
  case WM_SIZING:
    if(OPTION_LOCK_AR) //Aspect Ratio
    {
#if defined(SSE_EMU_THREAD)
      bool oldSuspendRendering=SuspendRendering;
      SuspendRendering=true;
      if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
        VideoLock.Lock();
#endif
      RECT *rcSizing=(RECT*)lPar;
      int res=(video_mixed_output) ? MEDRES : screen_res;
      BYTE Idx=WinSizeForRes[res];
      int w0=border?WinSizeBorder[res][Idx].x:WinSize[res][Idx].x;
      int h0=border?WinSizeBorder[res][Idx].y:WinSize[res][Idx].y;
      double ratio=(double)w0/(double)h0;
      int w,h;
      if(wPar==WMSZ_LEFT||wPar==WMSZ_RIGHT)
      {
        w=rcSizing->right-rcSizing->left-(GuiSM.cx_frame()*2);
        h=(int)(w/ratio);
        rcSizing->bottom=rcSizing->top+h+(MENUHEIGHT+GuiSM.cy_caption()
                         +GuiSM.cy_frame()*2+GuiSM.m_statusbar_height);
      }
      else
      {
        h=rcSizing->bottom-rcSizing->top-(MENUHEIGHT+GuiSM.cy_caption()
          +GuiSM.cy_frame()*2+GuiSM.m_statusbar_height);
        w=(int)(h*ratio);
        rcSizing->right=rcSizing->left+w+(GuiSM.cx_frame()*2);
      }
#ifndef SSE421R1 // update nothing
      int ws=0,wb=w*10;
      if(Idx>0)
        ws=(border?WinSizeBorder[res][Idx-1].x:WinSize[res][Idx-1].x);
      if(Idx<3)
        wb=(border?WinSizeBorder[res][Idx+1].x:WinSize[res][Idx+1].x);
      // will be closer if player later wants 'normal size'
      if(w<=ws)
      {
        WinSizeForRes[res]--;
        draw_win_mode[res]=0; // "no stretch"
        if(DISPLAY_SIZE>0) // BUG should be 1
          DISPLAY_SIZE--;
        OptionBox.UpdateWindowSizeAndBorder(); // change live!
      }
      else if(w>=wb)
      {
        WinSizeForRes[res]++;
        draw_win_mode[res]=0;
        if(DISPLAY_SIZE<4)
          DISPLAY_SIZE++;
        OptionBox.UpdateWindowSizeAndBorder();
      }
#endif//#ifndef SSE421R1
#if defined(SSE_GUI_STATUS_BAR)
#if !defined(SSE_GUI_STATUS_BAR_DRAW_CAPS)
      if(OPTION_STATUS_BAR) 
      { // give size in the statusbar
#if !defined(SSE_GUI_STATUS_BAR_CLICKABLE) // this +%d is hard to understand, wasn't even sure of what it was...
        sprintf(status_bar_text[SB_PART_CAPS],"%dx%d",w,h);
#else
        int sizew0=(border) ? WinSizeBorder[screen_res][0].x : WinSize[screen_res][0].x;
        int sizeh0=(border) ? WinSizeBorder[screen_res][0].y : WinSize[screen_res][0].y;
        if(w%sizew0==0 && h%sizeh0==0) // match
          sprintf(status_bar_text[SB_PART_CAPS],"%dx%d (=x%d)",w,h,w/sizew0);
        else // show distance
          sprintf(status_bar_text[SB_PART_CAPS],"%dx%d (+%d)",w,h,w%sizew0);
#endif
        PostMessage(hStatusBar,SB_SETTEXT,SB_PART_CAPS,(LPARAM)status_bar_text[SB_PART_CAPS]);
        SetTimer(StemWin,STATUSBAR_TIMER_ID,3000,NULL);
      }
#endif
#endif
#if defined(SSE_EMU_THREAD)
      VideoLock.Unlock();
      SuspendRendering=oldSuspendRendering;
#endif
    }
    break;

  case WM_SIZE:
  {
#if defined(SSE_EMU_THREAD)
    bool oldSuspendRendering=SuspendRendering;
    SuspendRendering=true;
    if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
      VideoLock.Lock();
#endif
    int cw=lpar_lo,ch=lpar_hi;
    RECT rc={0,MENUHEIGHT,cw,ch};
    TRACE_LOG("WM_SIZE %dx%d\n",cw,ch);
    InvalidateRect(Win,&rc,FALSE);
#ifndef ONEGAME
    if(FullScreen) 
    {
      int size=16<<BIG_ICONS;
      SetWindowPos(GetDlgItem(Win,IDC_BACKTOWIN),0,cw-(2*(size+4)+3),0,0,0,
                   SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
      SetWindowPos(GetDlgItem(Win,IDC_QUIT),0,cw-(size+4),0,0,0,
                   SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
      cw-=(size+4)*2+10;
    }
#endif
#if defined(SSE_GUI_STATUS_BAR)
    else if(OPTION_STATUS_BAR)
    {
      int x[4];
#if !defined(SSE_GUI_STATUS_BAR_CLICKABLE)
      x[0]=GUIMUL(60);
      x[1]=(cw*28)/60;
      x[2]=cw-150;
      x[3]=cw;
#else
      x[0]=cw/6;
      x[1]=(cw*28)/60;
      x[2]=(cw*44)/60;
      x[3]=cw;
#endif
      SendMessage(hStatusBar,SB_SETPARTS,(WPARAM)4,(LPARAM)x);
      SendMessage(hStatusBar,WM_SIZE,0,0);
      for(int i=0;i<4;i++) // client rectangles
        SendMessage(hStatusBar,SB_GETRECT,i,(LPARAM)&status_bar_rc[i]);
    }
#endif
#if defined(SSE_GUI_TOOLBAR)
    GUIToolbarArrangeIcons(cw);
#endif
#if !defined(SSE_NO_OSD) && !defined(SSE_OSD_NORESETINFO)
    if(hResetInfoWin)
      SendMessage(hResetInfoWin,WM_USER,1789,0);
#endif
    if(draw_grille_black<4) 
      draw_grille_black=4;
    update_CanUse_400(cw,ch);
    switch(wPar) {
    case SIZE_MAXIMIZED:
      bAppMaximized=true;
      break;
    case SIZE_MINIMIZED:
      bAppMinimized=true;
      break;
    case SIZE_RESTORED:
      if(bAppMinimized)
        bAppMinimized=false;
      else if(bAppMaximized)
        bAppMaximized=false;
      break;
    }
    InvalidateRect(Win,NULL,FALSE);
    if(!FullScreen)
    {
      GetClientRect(Win,&Draw.BltDst);
      //if(!FullScreen)
      {
        Draw.BltDst.top+=MENUHEIGHT;
#if defined(SSE_GUI_STATUS_BAR)
        GuiSM.Update();
        Draw.BltDst.bottom-=GuiSM.m_statusbar_height;
#endif
        TRACE_LOG("Dest %dx%d\n",Draw.BltDst.right-Draw.BltDst.left,Draw.BltDst.bottom-Draw.BltDst.top);
#if defined(SSE_VID_DD)
        POINT pt={0,0};
        ClientToScreen(StemWin,&pt);
        OffsetRect(&Draw.BltDst,pt.x,pt.y);
#endif
      }
#if defined(SSE_VID_TRACE_SIZE)
      TRACE("WM_SIZE -> Dest %d %d %d %d\n",Draw.BltDst.left,Draw.BltDst.top,Draw.BltDst.right,Draw.BltDst.bottom);
#endif
    }
    // a lot of surface creation/deletion, but the system is responsible, if
    // it keeps sending WM_SIZE, it wants us to resize everything while the
    // player is still dragging
    Disp.UpdateSurfaces(cw,ch);
#if defined(SSE_EMU_THREAD)
    SuspendRendering=oldSuspendRendering;
#endif
#ifdef SSE421R1 //DEADC0DE // no reason to do that except crash // lets do it
    if(runstate==RUNSTATE_STOPPED)
      draw(false);
#endif
#if defined(SSE_EMU_THREAD)
    VideoLock.Unlock();
#endif
    break;
  }


  //case WM_MOVING:

  case WM_MOVE:
#if defined(SSE_VID_2SCREENS)
#if defined(SSE_VID_D3D)
    // recreate surfaces if VSync could have changed
    // problem: it syncs on main display anyway
    if(Disp.CheckCurrentMonitorConfig() && Disp.Method==DISPMETHOD_D3D
      && !FullScreen&&OPTION_AUTOVSYNC)
    {
#if defined(SSE_EMU_THREAD)
      bool oldSuspendRendering=SuspendRendering;
      SuspendRendering=true;
      if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
        VideoLock.Lock();
#endif
      Disp.D3DCreateSurfaces();
#if defined(SSE_EMU_THREAD)
      VideoLock.Unlock();
      SuspendRendering=oldSuspendRendering;
#endif
      UPDATE_STATUS_BAR_PART(SB_PART_FREQ);
    }
#else
    Disp.CheckCurrentMonitorConfig();
#endif
#endif
#if defined(SSE_VID_DD)
    if(!FullScreen)
    {
      GetClientRect(Win,&Draw.BltDst);
      Draw.BltDst.top+=MENUHEIGHT;
#if defined(SSE_GUI_STATUS_BAR)
      GuiSM.Update();
      Draw.BltDst.bottom-=GuiSM.m_statusbar_height;
#endif
      OffsetRect(&Draw.BltDst,GET_X_LPARAM(lPar),GET_Y_LPARAM(lPar));
      if(runstate!=RUNSTATE_STOPPED)
        InvalidateRect(Win,NULL,FALSE); // because blitting when moving fast will trash it
    }
#endif
    break;

  case WM_DISPLAYCHANGE:
    if(FullScreen==0) 
    {
#if !defined(SSE_420R3)
      bool old_draw_lock=draw_lock;
#endif
      OptionBox.EnableBorderOptions(Disp.BorderPossible());
      Disp.ScreenChange();
      palette_convert_all();
#if !defined(SSE_420R3)
      if(runstate==RUNSTATE_STOPPED)
      {
        draw(false);
        if(old_draw_lock) 
          draw_begin();
      }
#endif
    }
#if defined(SSE_VID_D3D)
    Disp.m_Adapter=(UINT)-1;
#endif
#if defined(SSE_VID_2SCREENS)
    Disp.CheckCurrentMonitorConfig(); // Update Freq
#endif
    UPDATE_STATUS_BAR_PART(SB_PART_FREQ);
    SetTimer(Win,DISPLAYCHANGE_TIMER_ID,500,NULL);
    break;

  case WM_SETTINGCHANGE:
    GuiSM.Update();
    return 0;

  case WM_CHANGECBCHAIN:
    if((HWND)wPar==NextClipboardViewerWin) 
      NextClipboardViewerWin=(HWND)lPar;
    else if(NextClipboardViewerWin) 
      SendMessage(NextClipboardViewerWin,Mess,wPar,lPar);
    break;

  case WM_DRAWCLIPBOARD:
    UpdatePasteButton();
    if(NextClipboardViewerWin) 
      SendMessage(NextClipboardViewerWin,Mess,wPar,lPar);
    break;

  case WM_ACTIVATEAPP:
    bAppActive=(wPar!=0);
    if(MuteWhenInactive)
    {
      if(SoundBuf && runstate==RUNSTATE_RUNNING)
      {
        DWORD dwStatus ;
        SoundBuf->GetStatus(&dwStatus);
        if(MuteWhenInactive==2 && bAppActive)
        {
          if(!(dwStatus&DSBSTATUS_PLAYING)) 
            SoundBuf->Play(0,0,DSBPLAY_LOOPING);
          MuteWhenInactive--;
        }
        else if(MuteWhenInactive==1 && !bAppActive)
        {
          if((dwStatus&DSBSTATUS_PLAYING))
            SoundBuf->Stop();
          MuteWhenInactive++;
        }
      }
    }
    if(FullScreen) 
    {
      if(wPar) 
      {  //Activating
#if defined(SSE_VID_DD)
        if(using_res_640_400) 
        {
          using_res_640_400=false;
          change_fullscreen_display_mode(true);
        }
#endif
        for(int n=0;n<nStemDialogs;n++) 
        {
          if(DialogList[n]->Handle) 
          {
            SetWindowPos(DialogList[n]->Handle,NULL,DialogList[n]->FSLeft,
              DialogList[n]->FSTop,0,0,SWP_NOZORDER|SWP_NOSIZE|SWP_NOACTIVATE);
          }
        }
        //Disp.ScreenChange(); // double creation at "activate"?
#ifndef ONEGAME
        draw(true);
#else
        CLICK_PLAY_BUTTON();
#endif
      }
      else if(runstate==RUNSTATE_RUNNING) 
      {
#if defined(SSE_VID_2SCREENS) // watch fullscreen demo, do sthg else on 2nd screen
        if(!OPTION_FAKE_FULLSCREEN)
#endif
          Glue.m_Status.stop_emu=1;
      }
    }
    break;

#if defined(SSE_GUI_STATUS_BAR)
  case WM_SETFOCUS:
    if(OPTION_STATUS_BAR &&!FullScreen)
      GUIRefreshStatusBar(); // refresh full status bar: icons, num scrl...
    break;
#endif

  case WM_SETCURSOR:
    switch(lpar_lo) {
    case HTCLIENT:
      if(stem_mousemode==STEM_MOUSEMODE_WINDOW)
      {
        if(no_set_cursor_pos)
          SetCursor(LoadCursor(NULL,IDC_CROSS));
        else
          SetCursor(NULL);
      }
#if !defined(SSE_DBG_NOMONITORSCREEN)
      else if(stem_mousemode==STEM_MOUSEMODE_BREAKPOINT)
        SetCursor(LoadCursor(NULL,IDC_CROSS));
#endif
      else
        SetCursor(PCArrow);
      return TRUE;
    }
    break;

  case WM_ACTIVATE:
    if(wPar==WA_INACTIVE) 
    {
      SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
      UpdateSTKeys();
      EnableTaskSwitch();
    }
    else 
    {
      if(!IsWindowEnabled(Win))
        PostMessage(StemWin,WM_USER,12345,(LPARAM)Win);
      SetFocus(StemWin);
      if(runstate==RUNSTATE_RUNNING && !bAllowTaskSwitch)
        DisableTaskSwitch();
    }
    break;

  case WM_KILLFOCUS:
    SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
    break;

  case WM_CLOSE:
    QuitSteem();
    return false;

  case WM_QUERYENDSESSION:
    QuitSteem();
    return true;

  case WM_DESTROY:
    ChangeClipboardChain(StemWin,NextClipboardViewerWin);
    StemWin=NULL;
    break;
  }
  return DefWindowProc(Win,Mess,wPar,lPar);
}


void HandleButtonMessage(UINT Id,HWND hBut) {
  switch(Id) {
  case IDC_DISK_MANAGER:
    DiskMan.ToggleVisible();
    SendMessage(hBut,BM_SETCHECK,DiskMan.IsVisible(),0);
    break;
  case IDC_JOYSTICKS:
    JoyConfig.ToggleVisible();
    SendMessage(hBut,BM_SETCHECK,JoyConfig.IsVisible(),0);
    break;
  case IDC_INFO:
    InfoBox.ToggleVisible();
    SendMessage(hBut,BM_SETCHECK,InfoBox.IsVisible(),0);
    break;
  case IDC_OPTIONS:
    OptionBox.ToggleVisible();
    SendMessage(hBut,BM_SETCHECK,OptionBox.IsVisible(),0);
    break;
  case IDC_SHORTCUTS:
    ShortcutBox.ToggleVisible();
    SendMessage(hBut,BM_SETCHECK,ShortcutBox.IsVisible(),0);
    break;
  case IDC_PATCHES:
    PatchesBox.ToggleVisible();
    SendMessage(hBut,BM_SETCHECK,PatchesBox.IsVisible(),0);
    break;
  case IDC_PLAY: // TOGGLE EMULATION START/STOP
    if(SendMessage(hBut,BM_GETCLICKBUTTON,0,0)==BM_CLICKBUTTON_RIGHT)
      break;
    bRunMessagePosted=false;
    if(runstate==RUNSTATE_STOPPED)
    {
      if(FullScreen && !bAppActive) 
        return;
#if !defined(SSE_HALT_CPUONLY)
      if(Cpu.ProcessingState==TMC68000::HALTED)
        break; // cancel "run" until reset
#endif
      if(GetForegroundWindow()==StemWin && GetCapture()==NULL 
        && !IsIconic(StemWin) && fast_forward!=3 && slow_motion!=3) 
      {
        if(OPTION_CAPTURE_MOUSE&1)
          SetStemMouseMode(STEM_MOUSEMODE_WINDOW);
      }
      SendMessage(hBut,BM_SETCHECK,1,0);
#if defined(SSE_EMU_THREAD)
      HWND hEmuThreadOption=GetDlgItem(OptionBox.Handle,IDC_EMU_THREAD);
      if(OptionBox.IsVisible())
        EnableWindow(hEmuThreadOption,FALSE);
      if(OPTION_EMUTHREAD)
      {
        // run() encapsulated in an apart thread
        if(hEmuThread==NULL)
          hEmuThread=CreateThread(NULL,0,EmuThreadProc,hBut,0,&EmuThreadId); 
      }
      else
      {
#if defined(SSE_GUI_MENUBAR)
        EnableMenuItem(StemWinMenu,IDC_MENUKILLTHREAD,MF_DISABLED);
#endif
        run(); // and we re-enter when checking messages at VBL
        SendMessage(hBut,BM_SETCHECK,0,0);
        if(OptionBox.IsVisible())
          EnableWindow(hEmuThreadOption,TRUE);
      }
#else
      run(); // unique call
      SendMessage(hBut,BM_SETCHECK,0,0);
#endif
    }
    else 
    {
      if(runstate==RUNSTATE_RUNNING && !Glue.m_Status.stop_emu)
      {
#if defined(SSE_DEBUGGER_FRAME_REPORT)
        FrameEvents.Report();
#endif
        Glue.m_Status.stop_emu=(OPTION_NO_OSD_ON_STOP) ? 2 : 1;
        SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
      }
#if defined(SSE_EMU_THREAD)
      else if(Glue.m_Status.stop_emu && OPTION_EMUTHREAD)
      { // try to stop twice
        if(Alert(T("The emulation thread isn't responding. Kill it?"),
          T(STEEM_CRASH_TXT),MB_ICONQUESTION|MB_YESNO)==IDYES)
        {
          TRACE("kill %s $%X\n","thread",EmuThreadId);
          TerminateThread(hEmuThread,0);
          hEmuThread=NULL;
          GUIRunEnd();
          runstate=RUNSTATE_STOPPED;
        }
      }
#endif
    }
    break;
  case IDC_RESET:
  {
    bool Warm=(SendMessage(hBut,BM_GETCLICKBUTTON,0,0)==BM_CLICKBUTTON_LEFT);
    reset_st(((Warm) ? RESET_WARM : RESET_COLD)| RESET_NOSTOP|
      RESET_CHANGESETTINGS|RESET_BACKUP|RESET_COUNT);
    break;
  }
  case IDC_BACKTOWIN:
    Disp.ChangeToWindowedMode(StatusInfo.MessageIndex==TStatusInfo::BLIT_ERROR);
    break;
#if !defined(SSE_NO_UPDATE)
  case 120:
    if(UpdateWin) {
      SendMessage(hBut,BM_SETCHECK,1,0);
      ShowWindow(UpdateWin,SW_SHOW);
      SetForegroundWindow(UpdateWin);
    }
    break;
#endif
#if defined(SSE_GUI_CONFIG_WRENCH)
/*  Player has clicked on the 'Configuration' icon, this makes a
    popup menu appear, 'Load configuration file' or 'Save configuration file'.
*/
  case IDC_CONFIGS:
  {
    RECT rc;
    GetWindowRect(hBut,&rc);
    HMENU Pop=CreatePopupMenu();
    AppendMenu(Pop,MF_STRING,IDC_LOADCONFIG,T("Load configuration file"));
    AppendMenu(Pop,MF_STRING,IDC_SAVECONFIG,T("Save configuration file"));
    SendMessage(hBut,BM_SETCHECK,1,0);
    TrackPopupMenu(Pop,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,
      rc.left,rc.bottom,0,StemWin,NULL);
    SendMessage(hBut,BM_SETCHECK,0,0);
    DestroyMenu(Pop);
    break;
  }
#endif
  case IDC_SNAPSHOT:
  {
    EasyStringList sl;
    SnapShotGetOptions(&sl);
    HMENU SnapShotMenu=CreatePopupMenu();
    for(int i=0;i<sl.NumStrings;i++) 
    {
      if(IsSameStr(sl[i].String,"-"))
        AppendMenu(SnapShotMenu,MF_SEPARATOR,0,NULL);
      else
        AppendMenu(SnapShotMenu,MF_STRING|(sl[i].Data[1] ? MF_GRAYED : 0),sl[i].Data[0],sl[i].String);
    }
    RECT rc;
    GetWindowRect(hBut,&rc);
    SendMessage(hBut,BM_SETCHECK,1,0);
    TrackPopupMenu(SnapShotMenu,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,
                   rc.left,rc.bottom,0,StemWin,NULL);
    SendMessage(hBut,BM_SETCHECK,0,0);
    DestroyMenu(SnapShotMenu);
    break;
  }
  case IDC_PASTE:
  {
    if(SendMessage(hBut,BM_GETCLICKBUTTON,0,0)==BM_CLICKBUTTON_RIGHT) // right-click
    {
      HMENU Pop=CreatePopupMenu();
      for(int n=0;n<11;n++) 
        AppendMenu(Pop,MF_STRING,IDC_PASTE_SPEED+n,T("Delay")+" - "+n);
      CheckMenuRadioItem(Pop,IDC_PASTE_SPEED,IDC_PASTE_SPEED+10,IDC_PASTE_SPEED-1+PasteSpeed,MF_BYCOMMAND);
      RECT rc;
      GetWindowRect(hBut,&rc);
      SendMessage(hBut,BM_SETCHECK,1,0);
      TrackPopupMenu(Pop,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,rc.left,rc.bottom,0,StemWin,NULL);
      if(PasteText.Empty()) 
        SendMessage(hBut,BM_SETCHECK,0,0);
      DestroyMenu(Pop);
      break;
    }
    else
      PasteIntoSTAction(STPASTE_TOGGLE);
    break;
  }
  case IDC_SCREENSHOT:
  {
    if(SendMessage(hBut,BM_GETCLICKBUTTON,0,0)==BM_CLICKBUTTON_RIGHT)
    {
      HMENU Pop=CreatePopupMenu();
      EasyStringList format_sl;
      Disp.ScreenShotGetFormats(&format_sl);
      AppendMenu(Pop,MF_STRING,IDC_SCREENSHOT_DIR_CHANGE,T("Change Screenshots Folder"));
      AppendMenu(Pop,MF_STRING,IDC_SCREENSHOT_DIR_OPEN,T("Open Screenshots Folder"));
      AppendMenu(Pop,MF_STRING|MF_CHECK(Disp.ScreenShotMinSize),IDC_SCREENSHOT_MIN,
                 T("Minimum Size Screenshots"));
      AppendMenu(Pop,MF_SEPARATOR,0,NULL);
      int sel=0;
      for(int n=0;n<format_sl.NumStrings;n++) 
      {
        AppendMenu(Pop,MF_STRING,IDC_SCREENSHOT_FORMATS+n,format_sl[n].String);
        if(format_sl[n].Data[0]==Disp.ScreenShotFormat)
          sel=IDC_SCREENSHOT_FORMATS+n;
      }
      CheckMenuRadioItem(Pop,IDC_SCREENSHOT_FORMATS,IDC_SCREENSHOT_FORMATS+format_sl.NumStrings,
        sel,MF_BYCOMMAND);
      format_sl.DeleteAll();
#if !defined(SSE_NO_FREEIMAGE)
      Disp.ScreenShotGetFormatOpts(&format_sl);
      if(format_sl.NumStrings) 
      {
        AppendMenu(Pop,MF_SEPARATOR,0,NULL);
        for(int n=0;n<format_sl.NumStrings;n++)
          AppendMenu(Pop,MF_STRING,IDC_FI_FORMATS+n,format_sl[n].String);
        CheckMenuRadioItem(Pop,IDC_FI_FORMATS,IDC_FI_FORMATS+format_sl.NumStrings,
          ((Disp.ScreenShotFormat==FIF_JPEG) ? (IDC_FI_FORMATS+(Disp.ScreenShotFormatOpts
          >>(8+(Disp.ScreenShotFormatOpts==0x800)))-(Disp.ScreenShotFormatOpts
          ==0x400)) : (IDC_FI_FORMATS+Disp.ScreenShotFormatOpts)),MF_BYCOMMAND);
      }
#endif
      RECT rc;
      GetWindowRect(hBut,&rc);
      SendMessage(hBut,BM_SETCHECK,1,0);
      TrackPopupMenu(Pop,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,
        rc.left,rc.bottom,0,StemWin,NULL);
      if(PasteText.Empty()) 
        SendMessage(hBut,BM_SETCHECK,0,0);
      DestroyMenu(Pop);
    }
    else 
    {
#if defined(SSE_VID_NEOPIC)
      if(Disp.ScreenShotFormat==IF_NEO)
      {
        Disp.pNeoFile=new neochrome_file; //32KB
        ZeroMemory(Disp.pNeoFile,sizeof(neochrome_file));
        for(int i=0;i<PAL_SIZE;i++)
        {
          Disp.pNeoFile->palette[i]=STpal[i];
          SWAP_BIG_ENDIAN_WORD(Disp.pNeoFile->palette[i]);
        }
      }
#endif
      if(runstate==RUNSTATE_RUNNING)
        DoSaveScreenShot|=1;
      else
        Disp.SaveScreenShot();
    }
    break;
  }//case
#if defined(SSE_DEBUGGER_TOGGLE)
  case IDC_DEBUGGER:
    ShowWindow(DWin,DebuggerVisible ? SW_HIDE : SW_SHOW);
    break;
#endif
  }//sw
}

#endif//#if !defined(SSE_LIBRETRONUKE)


LRESULT CALLBACK FSQuitWndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar) {
  bool CheckDown=false;
  switch(Mess) {
  case WM_CREATE:
    SetProp(Win,"Down",(HANDLE)0);
    break;
  case WM_DESTROY:
    RemoveProp(Win,"Down");
    break;
  case WM_PAINT:
  {
    RECT rc;
    GetClientRect(Win,&rc);
    PAINTSTRUCT ps;
    BeginPaint(Win,&ps);
    FillRect(ps.hdc,&rc,(HBRUSH)GetSysColorBrush(COLOR_BTNFACE));
    int Down=(GetProp(Win,"Down"))?1:0;
    int Size=16<<BIG_ICONS;
    DrawIconEx(ps.hdc,Down,3+Down,(HICON)hGUIIcon[RC_ICO_FULLQUIT],Size,Size,0,NULL,DI_NORMAL);
    EndPaint(Win,&ps);
    return 0;
  }
  case WM_MOUSEMOVE:case WM_CAPTURECHANGED:
    CheckDown=true;
    break;
  case WM_LBUTTONDOWN:
    SetCapture(Win);
    CheckDown=true;
    break;
  case WM_LBUTTONUP:
    ReleaseCapture();
    CheckDown=true;
    PostMessage(Win,WM_USER,0xface,lPar);
    break;
  case WM_USER:
  {
    if(wPar!=0xface) // see above 
      break;
    RECT dest;
    GetClientRect(Win,&dest);
    if(GET_X_LPARAM(lPar)<dest.right && GET_Y_LPARAM(lPar)<dest.bottom) 
    {
      int Quit=IDYES;
      if(FSQuitAskFirst)
        Quit=Alert(T("Quit Steem"),T("Are you sure?"),MB_ICONQUESTION|MB_YESNO);
      if(Quit==IDYES) 
        QuitSteem();
    }
    return 0;
  }//case
  }//sw
  if(CheckDown) 
  {
    bool OldDown=!!GetProp(Win,"Down");
    bool NewDown=false;
    if(GetCapture()==Win) 
    {
      RECT rc;
      GetClientRect(Win,&rc);
      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(Win,&pt);
      NewDown=(pt.x>=0&&pt.x<rc.right && pt.y>=0&&pt.y<rc.bottom);
    }
    if(OldDown!=NewDown) 
    {
      SetProp(Win,"Down",(HANDLE)NewDown);
      InvalidateRect(Win,NULL,FALSE);
    }
    return 0;
  }
  return DefWindowProc(Win,Mess,wPar,lPar);
}

#endif//win32


HRESULT change_fullscreen_display_mode(bool resizeclippingwindow) {
  HRESULT Ret;

#if defined(SSE_VID_D3D)
  RECT rc=Disp.rcMonitor;
  rc.top+=MENUHEIGHT;
#else // all other builds
  RECT rc={Disp.rcMonitor.left,Disp.rcMonitor.top+MENUHEIGHT,640,480};
#endif

#if defined(SSE_VID_DD)
  int bpp=32;
  int hz_ok=0,hz=0;
  hz=prefer_pc_hz[1+(border!=0)];
  if(draw_fs_blit_mode==DFSM_STRETCHBLIT)
  {
    hz=prefer_pc_hz[3];
    rc.right=Disp.fs_res[Disp.fs_res_choice].x;
    rc.bottom=Disp.fs_res[Disp.fs_res_choice].y;
  }
  else if(draw_fs_blit_mode==DFSM_FAKEFULLSCREEN) 
  {
    rc.right=Disp.rcMonitor.right-Disp.rcMonitor.left;
    rc.bottom=Disp.rcMonitor.bottom-Disp.rcMonitor.top+MENUHEIGHT;
  }
  else
#ifndef NO_CRAZY_MONITOR
  if(extended_monitor)
  {
    rc.right=MAX((int)em_width,640);
    rc.bottom=MAX((int)em_height,480);
    hz=0;
  } else
#endif
  if(border)
  {
    rc.right=800;
    rc.bottom=600;
  }
  if(OPTION_FAKE_FULLSCREEN)
  {}
  else if((Ret=Disp.SetDisplayMode(rc.right,rc.bottom,bpp,hz,&hz_ok))!=DD_OK)
#else // all other builds
  if((Ret=Disp.SetDisplayMode())!=DD_OK)
#endif
    return Ret;

#if defined(SSE_VID_DD)
  if(hz)
  {
    if(draw_fs_blit_mode==DFSM_STRETCHBLIT)
    {
      tested_pc_hz[3]=MAKEWORD(hz,(hz_ok&1));
      real_pc_hz[3]=hz_ok>>16;
    }
    else
    {
      tested_pc_hz[1+(border!=0)]=MAKEWORD(hz,(hz_ok&1));
      real_pc_hz[1+(border!=0)]=hz_ok>>16;
    }
  }
#endif

#ifdef WIN32

#if defined(SSE_VID_2SCREENS)
#if defined(SSE_VID_DD) 
  get_fullscreen_totalrect(&rc);
  // Compute size
  int cw=rc.right-rc.left;
  int ch=rc.bottom-rc.top;
  SetWindowPos(StemWin,0,rc.left,rc.top,cw,ch,0);
#endif
  // D3D: done in D3DCreateSurfaces()
#else
  SetWindowPos(StemWin,HWND_TOPMOST,0,0,rc.right,rc.bottom,0);
#endif

  if(resizeclippingwindow) 
  {
#if defined(SSE_VID_DD) 
#if defined(SSE_VID_2SCREENS)
    get_fullscreen_totalrect(&rc);
    // Compute size
    cw=rc.right-rc.left;
    ch=rc.bottom-rc.top;
    SetWindowPos(StemWin,0,rc.left,rc.top,cw,ch,0);
#elif defined(SSE_VID_DD_MISC)
    SetWindowPos(StemWin,0,0,0,rc.right,rc.bottom,SWP_NOZORDER);
#else
    SetWindowPos(ClipWin,0,0,MENUHEIGHT,rc.right,rc.bottom-MENUHEIGHT,SWP_NOZORDER);
#endif
#endif

  }
  if(DiskMan.IsVisible())
  {
    if(DiskMan.FSMaximized)
    {
      SetWindowPos(DiskMan.Handle,NULL,-GuiSM.cx_frame(),MENUHEIGHT,rc.right
        +GuiSM.cx_frame()*2,rc.bottom+GuiSM.cy_frame()-MENUHEIGHT,SWP_NOZORDER|SWP_NOACTIVATE);
    }
    else 
    {
      SetWindowPos(DiskMan.Handle,NULL,0,0,MIN(DiskMan.FSWidth,(int)rc.right),
        MIN(DiskMan.FSHeight,(int)rc.bottom-MENUHEIGHT),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    }
  }
#if defined(SSE_VID_DD)
  OptionBox.UpdateFullscreen();
#endif
  HDC DC=GetDC(StemWin);
  FillRect(DC,&rc,(HBRUSH)GetStockObject(BLACK_BRUSH));
  ReleaseDC(StemWin,DC);
#endif//WIN32

  draw_grille_black=Glue.PreviousVideoFreq; // Redraw black areas for 1 second
  return DD_OK;
}


#if 0 // this would put the window partly out of screen on border off
void change_window_size_for_border_change(int oldborder,int newborder) {
  if(ResChangeResize==0)
    return;
  if((newborder)&&!(oldborder))
    StemWinResize(-(16*4),-(TopBorderSize*2));
  else if(!(newborder)&&(oldborder))
    StemWinResize((16*4),(TopBorderSize*2));
}
#endif

#ifdef UNIX

int StemWinProc(void*,Window Win,XEvent *Ev)
{
//	printf("%i\n",Ev->type);
#ifndef NO_SHM
  if (Ev->type==Disp.SHMCompletion){
    Disp.asynchronous_blit_in_progress=false;
  }else
#endif

  switch (Ev->type){
    case Expose:
    {
      XWindowAttributes wa;
      XGetWindowAttributes(XD,StemWin,&wa);

      hxc::clip_to_expose_rect(XD,&(Ev->xexpose),DispGC);

      if (Ev->xexpose.y+Ev->xexpose.height>MENUHEIGHT){
        draw_end();
        if (draw_blit()==0){
          XSetForeground(XD,DispGC,BlackCol);
          XFillRectangle(XD,StemWin,DispGC,2,MENUHEIGHT+2,
                          wa.width-4,wa.height-(MENUHEIGHT+4));
        }
        Disp.Surround();
      }

      XSetForeground(XD,DispGC,BkCol);
      XFillRectangle(XD,StemWin,DispGC,0,0,wa.width,MENUHEIGHT);
      XSetClipMask(XD,DispGC,None);

      XSync(XD,False);
      break;
    }
    case ButtonPress:
      if (Ev->xbutton.button==Button4 || Ev->xbutton.button==Button5) break;
      if (runstate==RUNSTATE_RUNNING && stem_mousemode==STEM_MOUSEMODE_DISABLED){
        if (Ev->xbutton.y>MENUHEIGHT){
          SetForegroundWindow(Win,Ev->xbutton.time);
          SetStemMouseMode(STEM_MOUSEMODE_WINDOW);
          disable_input_vbl_count=20; // eat first click
          if(OPTION_CAPTURE_MOUSE&6) // auto
            OPTION_CAPTURE_MOUSE|=BIT_0; // make it sticky
        }
      }else if (runstate==RUNSTATE_STOPPED && StartEmuOnClick){
        PostRunMessage();
      }
    case ButtonRelease:
      break;
    case KeyPress:
    case KeyRelease:
    {
      bool Up=(Ev->type==KeyRelease);
      if (Up==0 && GetKeyState(Ev->xkey.keycode)<0){ //Key repeat
      	return PEEKED_MESSAGE;
      }
      SetKeyState(Ev->xkey.keycode,!Up);

      KeySym ks=XKeycodeToKeysym(XD,Ev->xkey.keycode,0);
      if (ks!=XK_Shift_L && ks!=XK_Shift_R &&
          ks!=XK_Control_L && ks!=XK_Control_R &&
          ks!=XK_Alt_L && ks!=XK_Alt_R){
#if defined(SSE_UNIX)
        if(Ev->xkey.keycode==VK_F11
        || Ev->xkey.keycode==Key_Pause)
#else
        if(Ev->xkey.keycode==Key_Pause)
#endif
        {
          if (Up==0) return PEEKED_MESSAGE;
          if (runstate==RUNSTATE_RUNNING){
            if (GetKeyStateSym(XK_Shift_R)<0 || GetKeyStateSym(XK_Shift_L)<0 || FullScreen){
              runstate=RUNSTATE_STOPPING;
              SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
              break;
            }else{
              if (stem_mousemode==STEM_MOUSEMODE_DISABLED){
                SetStemMouseMode(STEM_MOUSEMODE_WINDOW);
                if(OPTION_CAPTURE_MOUSE&6) // auto
                  OPTION_CAPTURE_MOUSE|=BIT_0; // make it sticky
              }else if (stem_mousemode==STEM_MOUSEMODE_WINDOW){
                SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
              }
            }
          }else if (runstate==RUNSTATE_STOPPED){
            return PEEKED_RUN;
          }
        }
#if defined(SSE_UNIX)/// && defined(SSE_GUI_F12)   
        else if(Ev->xkey.keycode==VK_F12 && !Up)
        {
          if (runstate==RUNSTATE_RUNNING)
          {
            runstate=RUNSTATE_STOPPING;
            SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
          }
          else if (runstate==RUNSTATE_STOPPED)
          {
            PostRunMessage();
          }
        }
#endif        
#if defined(SSE_UNIX) // F1 for help
        else if(Ev->xkey.keycode==VK_F1 && (runstate==RUNSTATE_STOPPED))
        {
          InfoBox.Page=TGeneralInfo::ABOUT;
          InfoBox.Hide(); // just in case
          InfoBox.Show();
        }
#endif        
        else if(Ev->xkey.keycode==VK_SCROLL||Ev->xkey.keycode==VK_NUMLOCK)
          SSEConfig.MaxJoy=MAX_ST_JOYS-1; // stop optimisation until next check
        else if(joy_is_key_used((BYTE)Ev->xkey.keycode)==0
                && CutDisableKey[(BYTE)Ev->xkey.keycode]==0)
          HandleKeyPress(Ev->xkey.keycode,Up,0);
      }
      break;
    }
    case ClientMessage:
      if (Ev->xclient.message_type==hxc::XA_WM_PROTOCOLS){
        if (Atom(Ev->xclient.data.l[0])==hxc::XA_WM_DELETE_WINDOW){
          QuitSteem();
        }
      }else if (Ev->xclient.message_type==RunSteemAtom){
          if (runstate==RUNSTATE_STOPPED){
            return PEEKED_RUN;
          }else if (runstate==RUNSTATE_RUNNING){
            runstate=RUNSTATE_STOPPING;
            SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
          }
      }else if (Ev->xclient.message_type==LoadSnapShotAtom){
    		if (runstate==RUNSTATE_STOPPED){
          bool AddToHistory=true;
          Str fn=LastSnapShot;
          if (Ev->xclient.data.l[0]==IDC_UNDORESET)
          {
            fn=TempPath+SLASH+FILE_RESETSNAPSHOT;
            AddToHistory=false;
          }
          if (Ev->xclient.data.l[0]==IDC_UNDOSNAPSHOTLOAD)
          {
            fn=TempPath+SLASH+FILE_LOADUNDOSNAPSHOT;
            AddToHistory=false;
          }
    	    LoadSnapShot(fn,AddToHistory);
          if (Ev->xclient.data.l[0]==IDC_UNDORESET || Ev->xclient.data.l[0]==IDC_UNDOSNAPSHOTLOAD)
            DeleteFile(fn);
    	  }else{
    	  	runstate=RUNSTATE_STOPPING;

          XEvent SendEv;
          SendEv.type=ClientMessage;
          SendEv.xclient.window=StemWin;
          SendEv.xclient.message_type=LoadSnapShotAtom;
          SendEv.xclient.format=32;
          SendEv.xclient.data.l[0]=Ev->xclient.data.l[0];
          XSendEvent(XD,StemWin,0,0,&SendEv);
    	  }
      }
      break;
    case ConfigureNotify:
    {
      XWindowAttributes wa;
      XGetWindowAttributes(XD,StemWin,&wa);

      if (DiskBut.handle){
        XMoveWindow(XD,InfBut,wa.width-135,0);
        XMoveWindow(XD,PatBut,wa.width-112,0);
        XMoveWindow(XD,CutBut,wa.width-89,0);
        XMoveWindow(XD,OptBut,wa.width-66,0);
        XMoveWindow(XD,JoyBut,wa.width-43,0);
        XMoveWindow(XD,DiskBut,wa.width-20,0);
      }
      bool OldCanUse=CanUse_400;
      if (draw_grille_black<10) draw_grille_black=10;
      update_CanUse_400(wa.width,wa.height);
      if (OldCanUse!=CanUse_400 && FullScreen==0){
      //  draw_end();
      //  draw(false);
      }
      x_draw_surround_count=MAX(x_draw_surround_count,10);
      break;
    }
    case SelectionNotify:
      if (Ev->xselection.property!=None){
        if (Ev->xselection.target==XA_STRING){
          Atom actual_type_return;
          int actual_format_return;
          unsigned long nitems_return;
          unsigned long bytes_after_return;
          char *t;
          XGetWindowProperty(XD,Win,Ev->xselection.property,
                        /*long_offset*/ 0, /*long_length*/ 5000,
                        /*delete*/ True, XA_STRING,
                        &actual_type_return, &actual_format_return,
                        &nitems_return, &bytes_after_return,
                        (BYTE**)(&t));
          if (actual_type_return==XA_STRING){
            PasteText=t;
            PasteVBLCount=PasteSpeed;
            PasteBut.set_check(true);
            XFree(t);
          }
        }
      }
      break;
    case FocusIn:
      bAppActive=true;
      XAutoRepeatOff(XD);
      break;
    case FocusOut:
    {
      Window Foc=0;
      int RevertTo;
      XGetInputFocus(XD,&Foc,&RevertTo);
    	if (Foc!=StemWin){
				XAutoRepeatOn(XD);
        SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
        ZeroMemory(KeyState,sizeof(KeyState));
        UpdateSTKeys();
      	bAppActive=0;
      }
      break;
    }
    case MapNotify:
	  	bAppActive=true;
    	bAppMinimized=0;
    	break;
    case UnmapNotify:
    	bAppMinimized=true;
    	break;
    case DestroyNotify:
      StemWin=0;
      QuitSteem();
      return PEEKED_QUIT;
  }
  return PEEKED_MESSAGE;
}
//---------------------------------------------------------------------------
int snapshot_parse_filename(char*fn,struct stat*s)
{
  if (S_ISDIR(s->st_mode)) return FS_FTYPE_FOLDER;
  if (has_extension(fn,".STS")){
    return FS_FTYPE_FILE_ICON+ICO16_SNAPSHOTS;
  }
  return FS_FTYPE_REJECT;
}
//---------------------------------------------------------------------------
void SnapShotProcess(int i)
{
  bool WaitUntilStopped=0;
  if (i==200 /* Load Snapshot */ || i==201 /* Save Snapshot */){
    fileselect.set_corner_icon(&Ico16,ICO16_SNAPSHOT);
    Str LastSnapShotFol=LastSnapShot;
    RemoveFileNameFromPath(LastSnapShotFol,REMOVE_SLASH);
    EasyStr fn=fileselect.choose(XD,LastSnapShotFol,GetFileNameFromPath(LastSnapShot),
                T("Memory Snapshots"),((i==200) ? FSM_LOAD:FSM_SAVE) | FSM_LOADMUSTEXIST |
                FSM_CONFIRMOVERWRITE,snapshot_parse_filename,".sts");
    if (fileselect.chose_option==FSM_LOAD){
      LastSnapShot=fn;
      WaitUntilStopped=true;
    }else if (fileselect.chose_option==FSM_SAVE){
      LastSnapShot=fn;
      SaveSnapShot(fn,-1);
    }
  }else if (i==205){ // Save over last
    if (SnapShotGetLastBackupPath().NotEmpty()){
      // Make backup, could be on different drive so do it the slow way
      copy_file_byte_by_byte(LastSnapShot,SnapShotGetLastBackupPath());
    }
    SaveSnapShot(LastSnapShot,-1);
  }else if (i==206){ // Undo save over
    // Restore backup, can only get here if backup path is valid
    copy_file_byte_by_byte(SnapShotGetLastBackupPath(),LastSnapShot);
    remove(SnapShotGetLastBackupPath());
  }else if (i>=210 && i<220){ // Load recent
    LastSnapShot=StateHist[i-210];
    WaitUntilStopped=true;
  }else if (i==IDC_UNDORESET || i==IDC_UNDOSNAPSHOTLOAD) // undo reset/last snap
    WaitUntilStopped=true;
  if (WaitUntilStopped){
    if (runstate==RUNSTATE_RUNNING) runstate=RUNSTATE_STOPPING;

    XEvent SendEv;
    SendEv.type=ClientMessage;
    SendEv.xclient.window=StemWin;
    SendEv.xclient.message_type=LoadSnapShotAtom;
    SendEv.xclient.format=32;
    SendEv.xclient.data.l[0]=i;
    XSendEvent(XD,StemWin,0,0,&SendEv);
  }
}
//---------------------------------------------------------------------------
int stemwin_popup_notify(hxc_popup *pop,int mess,INT_PTR idx)
{
	if (mess==POP_CHOOSE){
    int i=pop->menu[idx].Data[1];
    if (i>=100 && i<200){
      PasteSpeed=(i-100)+1;
    }else if (i>=200 && i<300){
      SnapShotProcess(i);
    }
  }
	SnapShotBut.set_check(0);
	PasteBut.set_check(PasteText.NotEmpty());
	return 0;
}

int StemWinButtonNotifyProc(hxc_button *But,int Mess,int *Inf)
{
  switch (But->id){
    case 101:
      if (Inf[0]==Button3){
      	slow_motion_change(Mess==BN_DOWN);
      }else if (Mess==BN_CLICKED){
	      PostRunMessage();
	    }
      break;
    case IDC_FASTFORWARD:
      if (Mess!=BN_DOWN && Mess!=BN_UP) break;
      
      if (Mess==BN_DOWN){
        if (fast_forward_stuck_down){
          fast_forward_stuck_down=0;
          Mess=BN_UP;
        }else{
          if ((DWORD)Inf[1]<ff_doubleclick_time){
            fast_forward_stuck_down=true;
            ff_doubleclick_time=0;
          }else{
            ff_doubleclick_time=(DWORD)Inf[1]+FF_DOUBLECLICK_MS;
          }
        }
      }else{
        if (fast_forward_stuck_down) break;
      }
      fast_forward_change(Mess==BN_DOWN,Inf[0]==Button3);
      break;
    case 102:
    {
#if defined(SSE_BUILD)
      bool Warm=!(Inf[0]==Button3);
      reset_st( (DWORD)(Warm ? RESET_WARM:RESET_COLD) | RESET_NOSTOP |
                  RESET_CHANGESETTINGS | RESET_BACKUP | RESET_COUNT);      
#else
      bool Warm=(Inf[0]==Button3);
      reset_st(DWORD(Warm ? RESET_WARM:RESET_COLD) | DWORD(Warm ? RESET_NOSTOP:RESET_STOP) |
                  RESET_CHANGESETTINGS | RESET_BACKUP | RESET_COUNT);      
#endif
      break;
    }
    case IDC_SNAPSHOT: // Memory Snapshots
    	if (Mess==BN_CLICKED){
        But->set_check(true);

        EasyStringList sl;
        SnapShotGetOptions(&sl);

    		pop.lpig=NULL;
        pop.menu.DeleteAll();
        for (int i=0;i<sl.NumStrings;i++){
          Str Text=sl[i].String;
          while (Text.InStr("&")>=0) Text.Delete(Text.InStr("&"),1);
          pop.menu.Add(Text,-1,sl[i].Data[0]);
        }
        pop.create(XD,But->handle,0,But->h,stemwin_popup_notify,NULL);
      }
      break;
    case 116:
      if (runstate==RUNSTATE_RUNNING){
        DoSaveScreenShot|=1;
      }else{
        Disp.SaveScreenShot();
      }
      break;
    case IDC_PASTE:
      if (Inf[0]==Button3){
      	if (Mess==BN_CLICKED){
      		But->set_check(true);
      		pop.lpig=&Ico16;
	      	pop.menu.DeleteAll();
	        for (int n=0;n<11;n++){
	        	long ico=ICO16_UNRADIOMARKED;
	        	if (PasteSpeed==(1+n)) ico=ICO16_RADIOMARK;
	        	pop.menu.Add(T("Delay")+" - "+n,ico,100+n);
	        }
	        pop.create(XD,But->handle,0,But->h,stemwin_popup_notify,NULL);
	      }
      }else{
		    PasteIntoSTAction(STPASTE_TOGGLE);
		  }
      break;
    case 115:
      Disp.GoToFullscreenOnRun=But->checked; // SS this was not implemented?
      if (runstate==RUNSTATE_RUNNING){
        runstate=RUNSTATE_STOPPING;
        RunWhenStop=true;
      }
      break;

    case 100: //DiskMan
      DiskMan.ToggleVisible();
      break;
    case 103: //Joy
      JoyConfig.ToggleVisible();
      break;
    case 105:
      InfoBox.ToggleVisible();
      break;
    case 107:
      OptionBox.ToggleVisible();
      break;
    case 112:
      ShortcutBox.ToggleVisible();
      break;
    case 113:
      PatchesBox.ToggleVisible();
      break;
    case 120: // AutoUpdate
      break;
  }
  return 0;
}
//---------------------------------------------------------------------------
int timerproc(void*,Window,INT_PTR id)
{
  if (id==SHORTCUTS_TIMER_ID){
    JoyGetPoses();
    ShortcutsCheck();
  }
  return HXC_TIMER_REPEAT;
}

#endif//UNIX

#undef LOGSECTION
