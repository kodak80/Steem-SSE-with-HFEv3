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
FILE: gui.cpp
DESCRIPTION: This is a core file that creates the main window in MakeGUI and
has lots and lots of miscellaneous GUI (Graphic User Interface) functions. 
The resource file rc/resource.rc doesn't contain dialog resources!
Instead, the interface is created programmatically. Except for the main
window, GUI objects are created when Show() of the class is called, for
instance TOptionBox::Show()
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <computer.h>
#include <draw.h>
#include <display.h>
#include <osd.h>
#include <gui.h>
#include <screen_saver.h>
#include <mem_browser.h>
#include <diskman.h>
#include <harddiskman.h>
#include <options.h>
#include <stjoy.h>
#include <shortcutbox.h>
#include <patchesbox.h>
#include <infobox.h>
#include <translate.h>
#include <macros.h>
#include <dir_id.h>
#include <loadsave.h>
#include <key_table.h>
#include <stdarg.h>

#define RELEASE_BUILD // was in conditions.h, moved 402R4 because it's only used in gui.cpp

#ifdef DEBUG_BUILD
#include <debugger.h>
#include <debugger_trace.h>
#include <mr_static.h>
#include <dwin_edit.h>
#ifndef RELEASE_BUILD
extern bool HWNDNotValid(HWND,char*,int);
extern LRESULT SendMessage_checkforbugs(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar,char*,int);
extern BOOL PostMessage_checkforbugs(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar,char*,int);

static BOOL DestroyWindow_checkforbugs(HWND Win,char *File,int Line) {
  if(HWNDNotValid(Win,File,Line))
    return FALSE;
  return DestroyWindow(Win);
}

static BOOL InvalidateRect_checkforbugs(HWND Win,CONST RECT *pRC,BOOL bErase,
                                        char *File,int Line) {
  if(HWNDNotValid(Win,File,Line))
    return FALSE;
  return InvalidateRect(Win,pRC,bErase);
}

static BOOL SWP_checkforbugs(HWND Win,HWND WinAfter,int x,int y,int w,int h,UINT Flags,
                             char *File,int Line) {
  if(HWNDNotValid(Win,File,Line))
    return FALSE;
  return SetWindowPos(Win,WinAfter,x,y,w,h,Flags);
}

#undef SendMessage
#define SendMessage(win,m,w,l) SendMessage_checkforbugs(win,m,w,l,__FILE__,__LINE__)
#undef PostMessage
#define PostMessage(win,m,w,l) PostMessage_checkforbugs(win,m,w,l,__FILE__,__LINE__)
#define DestroyWindow(win) DestroyWindow_checkforbugs(win,__FILE__,__LINE__)
#define InvalidateRect(win,rc,b) InvalidateRect_checkforbugs(win,rc,b,__FILE__,__LINE__)
#define SetWindowPos(win,win2,x,y,w,h,fp) SWP_checkforbugs(win,win2,x,y,w,h,fp,__FILE__,__LINE__)
#endif
#endif//DEBUG__BUILD



BYTE KeyDownModifierState[256];
//            DISPLAY_SIZE       1         2          3          4
#ifdef UNIX
POINT WinSize[4][5]={       {{320,200},{640,400},{ 960,600},{1280,800},{-1,-1}},  // res0
                            {{640,200},{640,400},{1280,400},{1280,800},{-1,-1}},  // res1
                            {{640,400},{640,400},{1280,800},{1280,800},{-1,-1}},  // res2
                            {{800,600},{-1,-1}}};

#endif
#ifdef WIN32
POINT WinSize[4][5]={       {{320,200},{640,400},{960, 600},{1280,800},{-1,-1}},  // res0
                            {{640,200},{640,400},{1280,400},{1280,800},{-1,-1}},  // res1 //TODO thought 3 was 960x600
                            //{{320,200},{640,400},{960,600},{1280,800},{-1,-1}},
                            //{{640,400},{1280,800},{-1,-1}},
                            {{320,200},{640,400},{960,600},{1280,800},{-1,-1}},   // res2
                            {{800,600},{-1,-1}}};
#endif

#if defined(SSE_DEFAULTS_421)
BYTE WinSizeForRes[4];
#else
BYTE WinSizeForRes[4]={1,1,0,0}; // first run: double size
#endif

TStatusInfo StatusInfo;
Str ROMFile,CartFile;
Str PasteText;
EasyStr RunDir,WriteDir,globalINIFile,ScreenShotFol,DocDir;
#if defined(SSE_WRITEDIR)
EasyStr TempPath,UsersPath;
#else // if not defined, simple alias to WriteDir
EasyStr &TempPath=WriteDir,&UsersPath=WriteDir;
#endif
EasyStr AcsiDir;
EasyStr LastSnapShot,BootStateFile,StateHist[STATE_HISTORY_LEN];
EasyStr AutoSnapShotName=FILE_AUTOSNAPSHOT;
EasyStr DefaultSnapshotFile;
Str BootDisk[2];

int DoSaveScreenShot=0;
DWORD DisableDiskLightAfter=3000;
int window_mouse_centre_x,window_mouse_centre_y;
int ExternalModDown=0;
int PasteVBLCount=0,PasteSpeed=2;
int BootInMode=BOOT_MODE_DEFAULT;
WORD stem_mousemode=STEM_MOUSEMODE_DISABLED;
LANGID KeyboardLangID=0;
bool TaskSwitchDisabled=false;
bool ResChangeResize=true,CanUse_400=false;
bool bAppActive=true,bAppMinimized=false;
bool FSQuitAskFirst=true,FSDoVsync=false;
bool Quitting=false;
bool comline_allow_LPT_input=false;
bool StartEmuOnClick=false;
bool bAllowTaskSwitch = NOT_ONEGAME(true) ONEGAME_ONLY(false);
bool ShowTips=bAllowTaskSwitch;
bool bPauseWhenInactive=false,BootTOSImage=false;
BYTE MuteWhenInactive=0;
#if !defined(SSE_NO_AOT)
bool bAlwaysOnTop=false;
#endif
bool bAppMaximized=false,AutoLoadSnapShot=false;
bool AllowLPT=true,AllowCOM=true;
bool HighPriority=false;


#ifdef WIN32

RECT rcPreFS;
TSystemMetrics GuiSM;
HICON hGUIIcon[RC_NUM_ICONS],hGUIIconSmall[RC_NUM_ICONS];
#if defined(SSE_GUI_DM_BIGICONS)
HICON hGUIIcon128[RC_NUM_ICONS];
#endif
HWND StemWin=NULL,ParentWin=NULL,ToolTip=NULL,DisableFocusWin=NULL,UpdateWin=NULL;
HMENU StemWin_SysMenu=NULL;
HFONT fnt;
HFONT hSteemGuiFont=NULL;
HCURSOR PCArrow;
COLORREF MidGUIRGB,DkMidGUIRGB;
HHOOK hNTTaskSwitchHook=NULL;
HWND NextClipboardViewerWin=NULL;
bool bRunMessagePosted=false;
bool WinNT=false;

void RegisterSteemControls();
void UnregisterSteemControls();


void TSystemMetrics::Update(BOOL NoMonitorCheck/*=FALSE*/) {
#if defined(SSE_VID_2SCREENS)
  if(!NoMonitorCheck)
    Disp.CheckCurrentMonitorConfig(); // Update monitor rectangle
  m_cx_screen=Disp.rcMonitor.right-Disp.rcMonitor.left; // maybe!
  m_cy_screen=Disp.rcMonitor.bottom-Disp.rcMonitor.top;
#else
  m_cx_screen=GetSystemMetrics(SM_CXSCREEN);
  m_cy_screen=GetSystemMetrics(SM_CYSCREEN);
#endif
  m_cx_frame=(WORD)GetSystemMetrics(SM_CXFRAME);
  m_cy_frame=(WORD)GetSystemMetrics(SM_CYFRAME);
#ifdef SSE_GUI_FIX
  // define for WINVER >=$600 (XP)
  //#define SM_CXPADDEDBORDER       92
  // those variables are only used in the Disk Manager anyway
  WORD padded4=(WORD)GetSystemMetrics(92); // and for y?
  m_cy_frame+=padded4; // 0 in 2000, XP
  m_cx_frame+=padded4; // same for y?
#endif
  m_cy_caption=(WORD)GetSystemMetrics(SM_CYCAPTION);
  m_cx_vscroll=(WORD)GetSystemMetrics(SM_CXVSCROLL);
#if defined(SSE_GUI_STATUS_BAR)
#if defined(SSE_EMU_THREAD)
  if(!VideoLock.blocked)
#endif
    m_statusbar_height=0;
  if(OPTION_STATUS_BAR
#if defined(SSE_EMU_THREAD)
    &&!VideoLock.blocked
#endif
    )
  {
    RECT rcsb;
    if(SendMessage(hStatusBar,SB_GETRECT,0,(LPARAM)&rcsb))
    {
      int x[3];
      SendMessage(hStatusBar,SB_GETBORDERS,0,(LPARAM)x);
      m_statusbar_height=(short)(rcsb.bottom-rcsb.top + x[1]); // client + border
    }
  }
#endif
#if defined(SSE_GUI_TOOLBAR)
  m_toolbar_height=0;
  if(OPTION_TOOLBAR)
    m_toolbar_height+=(BIG_ICONS) ? (ICON_SIZE_BIG+1):(ICON_SIZE_SMALL+1);
#endif
#if defined(SSE_GUI_MENUBAR)
  m_menubar_height=0;
  if(OPTION_MENUBAR)
    m_menubar_height+=(short)GetSystemMetrics(SM_CYMENU);
#endif
}


#if defined(SSE_GUI_MENUBAR)

HMENU StemWinMenu=NULL;
HMENU StemWinMenuFile=NULL;
HMENU StemWinMenuEmu=NULL;
HMENU StemWinMenuTools=NULL;

BOOL SteemSetMenu(bool on) {
  BOOL err=SetMenu(StemWin,on?StemWinMenu:NULL);
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
  StemWinResize();
  CheckMenuItem(StemWin_SysMenu,IDC_MENUBAR,MF_BYCOMMAND|MF_CHECK(on));
  OptionBox.SSEUpdateIfVisible();
#if defined(SSE_EMU_THREAD)
  SuspendRendering=oldSuspendRendering;
  VideoLock.Unlock();
#endif
  return err; // we return SetMenu's return value for good form but we don't use it!
}

#endif//#if defined(SSE_GUI_MENUBAR)


#if defined(SSE_GUI_STATUS_BAR) // from the sample code

HWND hStatusBar=NULL;

// Description: 
//   Creates a status bar and divides it into the specified number of parts.
// Parameters:
//   hwndParent - parent window for the status bar.
//   idStatus - child window identifier of the status bar.
//   hinst - handle to the application instance.
//   cParts - number of parts into which to divide the status bar.
// Returns:
//   The handle to the status bar.
//
HWND DoCreateStatusBar(HWND hwndParent,HMENU idStatus,HINSTANCE hinst,int cParts) {
  HWND hwndStatus;
  RECT rcClient;
  HLOCAL hloc;
  PINT paParts;
  int i,nWidth;

  // Ensure that the common control DLL is loaded.
  //  InitCommonControls();

  // Create the status bar.
  hwndStatus=CreateWindowEx(
    0,                       // no extended styles
    STATUSCLASSNAME,         // name of status bar class
    (LPCSTR)NULL,           // no text when first created
   (OPTION_BLOCK_RESIZE?0:SBARS_SIZEGRIP) | // includes a sizing grip
    WS_CHILD|WS_VISIBLE,   // creates a visible child window
    0,0,0,0,              // ignores size and position
    hwndParent,              // handle to parent window
    (HMENU)idStatus,       // child window identifier
    hinst,                   // handle to application instance
    NULL);                   // no window creation data

  // Get the coordinates of the parent window's client area.
  GetClientRect(hwndParent,&rcClient);

  // Allocate an array for holding the right edge coordinates.
  hloc=LocalAlloc(LHND,sizeof(int) * cParts);
  paParts=(PINT)LocalLock(hloc);

  // Calculate the right edge coordinate for each part, and
  // copy the coordinates to the array.
  nWidth=rcClient.right/cParts;
  int rightEdge=nWidth;
  for(i=0; paParts && i<cParts; i++)
  {
    paParts[i]=rightEdge;
    rightEdge+=nWidth;
  }

  // Tell the status bar to create the window parts.
  SendMessage(hwndStatus,SB_SETPARTS,(WPARAM)cParts,(LPARAM)paParts);

  // Free the array, and return.
  LocalUnlock(hloc);
  LocalFree(hloc);

  return hwndStatus;
}

#endif//#if defined(SSE_GUI_STATUS_BAR)

#endif//WIN32


#ifdef UNIX

XErrorEvent XError;
hxc_popup pop;
hxc_popuphints hints;
Window StemWin=0;
GC DispGC=0;
Cursor EmptyCursor=0;
Atom RunSteemAtom,LoadSnapShotAtom;
XID SteemWindowGroup = 0;
DWORD BlackCol=0,WhiteCol=0,BkCol=0,BorderLightCol,BorderDarkCol;
hxc_alert alert;
short KeyState[256]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
extern "C" LPBYTE Get_icon16_bmp(),Get_icon32_bmp(),Get_icon64_bmp(),Get_tos_flags_bmp();
IconGroup Ico16,Ico32,Ico64,IcoTOSFlags;
Pixmap StemWinIconPixmap=0,StemWinIconMaskPixmap=0;
hxc_button RunBut,FastBut,ResetBut,SnapShotBut,ScreenShotBut,PasteBut,FullScreenBut;
hxc_button InfBut,PatBut,CutBut,OptBut,JoyBut,DiskBut;

#ifdef SSE_UNIX_STATUSBAR
hxc_textdisplay StatusBar;
char status_bar_text[256];
#endif

DWORD ff_doubleclick_time=0;
hxc_fileselect fileselect;
char* Comlines_Default[NUM_COMLINES][8]={
        {"netscape \"[URL]\"","konqueror \"[URL]\"","galeon \"[URL]\"","opera \"[URL]\"","firefox \"[URL]\"","mozilla \"[URL]\"",NULL},
        {"netscape \"[URL]\"","konqueror \"[URL]\"","galeon \"[URL]\"","opera \"[URL]\"","firefox \"[URL]\"","mozilla \"[URL]\"",NULL},
        {"netscape \"mailto:[ADDRESS]\"","mozilla \"mailto:[ADDRESS]\"","kmail \"[ADDRESS]\"","galeon \"mailto:[ADDRESS]\"",NULL},
        {"konqueror \"[PATH]\"","nautilus \"[PATH]\"","xfm \"[PATH]\"",NULL},
        {"kfind \"[PATH]\"","gnome-search-tool \"[PATH]\"",NULL}
        };
Str Comlines[NUM_COMLINES]={Comlines_Default[0][0],Comlines_Default[1][0],Comlines_Default[2][0],Comlines_Default[3][0],Comlines_Default[4][0]};


int romfile_parse_routine(char*fn,struct stat*s)
{
  if (S_ISDIR(s->st_mode)) return FS_FTYPE_FOLDER;
  if (has_extension_list(fn,".IMG",".ROM",NULL)){
    return FS_FTYPE_FILE_ICON+ICO16_STCONFIG;
  }
  return FS_FTYPE_REJECT;
}


int diskfile_parse_routine(char *fn,struct stat *s)
{
  if (S_ISDIR(s->st_mode)) return FS_FTYPE_FOLDER;
  if (FileIsDisk(fn)){
	  return FS_FTYPE_FILE_ICON+ICO16_DISKMAN;	
  }
  return FS_FTYPE_REJECT;
}


int wavfile_parse_routine(char *fn,struct stat *s)
{
  if (S_ISDIR(s->st_mode)) return FS_FTYPE_FOLDER;
  if (has_extension(fn,".WAV")){
	  return FS_FTYPE_FILE_ICON+ICO16_SOUND;	
  }
  return FS_FTYPE_REJECT;
}


int folder_parse_routine(char *,struct stat *s)
{
  if (S_ISDIR(s->st_mode)) return FS_FTYPE_FOLDER;
  return FS_FTYPE_REJECT;
}


int cartfile_parse_routine(char *fn,struct stat*s) {
  if(S_ISDIR(s->st_mode))
    return FS_FTYPE_FOLDER;
  if(has_extension(fn,".STC"))
    if((s->st_size)<=128*1024+4)
      return FS_FTYPE_FILE_ICON+ICO16_CART;
  return FS_FTYPE_REJECT;
}


#if defined(SSE_IKBD_MAPPINGFILE)
int inifile_parse_routine(char *fn,struct stat*s) {
  if (S_ISDIR(s->st_mode)) return FS_FTYPE_FOLDER;
  if (has_extension(fn,".INI")){
    return FS_FTYPE_FILE_ICON+ICO16_STCONFIG;
  }
  return FS_FTYPE_REJECT;
}
#endif

#if defined(SSE_ACSI_MNGR)
int acsi_parse_routine(char *fn,struct stat*s) {
  if (S_ISDIR(s->st_mode)) return FS_FTYPE_FOLDER;
  if (has_extension_list(fn,".IMG",NULL)){
    return FS_FTYPE_FILE_ICON+ICO16_HARDDRIVE;
  }
  return FS_FTYPE_REJECT;
}
#endif


hxc_listview hxc_buttonpicker::lv;
DWORD hxc_buttonpicker::old_joy_axis_down[MAX_PC_JOYS],hxc_buttonpicker::old_joy_button_down[MAX_PC_JOYS];

#endif//UNIX


#ifdef WIN32

bool HandleMessage(MSG *mess) {
  if(DiskMan.Handle) // Handle isn't null if the dialog is open
    if(DiskMan.HasHandledMessage(mess))     
      return false;
  if(OptionBox.Handle)
    if(OptionBox.HasHandledMessage(mess))   
      return false;
  if(JoyConfig.Handle)
    if(JoyConfig.HasHandledMessage(mess))
      return false;
  if(InfoBox.Handle)
    if(InfoBox.HasHandledMessage(mess))
      return false;
  if(ShortcutBox.Handle)
    if(ShortcutBox.HasHandledMessage(mess))
      return false;
  if(HardDiskMan.Handle)
    if(HardDiskMan.HasHandledMessage(mess))
      return false;
#if defined(SSE_ACSI_MNGR)
  if(AcsiHardDiskMan.Handle)
    if(AcsiHardDiskMan.HasHandledMessage(mess))
      return false;
#endif
  if(PatchesBox.Handle)
    if(PatchesBox.HasHandledMessage(mess))
      return false;
  return true;
}


int RCGetSizeOfIcon(INT_PTR n) {
  switch(n) {
  case RC_ICO_DRIVE:case RC_ICO_DRIVELINK:
  case RC_ICO_GREYDISK:case RC_ICO_BLUEDISK:
  case RC_ICO_DRIVEBROKEN:case RC_ICO_DRIVEREADONLY:
  case RC_ICO_DRIVEZIPPED_RO:case RC_ICO_DRIVEZIPPED_RW:
  case RC_ICO_FOLDER:case RC_ICO_FOLDERLINK:
  case RC_ICO_FOLDERBROKEN:case RC_ICO_PARENTDIR:
  case RC_ICO_PRGFILEICO:
  case RC_ICO_APP://v410
    return 33; // 32 + 16
  case RC_ICO_RECORD:case RC_ICO_PLAY_BIG:
    return 32;
  case RC_ICO_HARDDRIVES:case RC_ICO_HARDDRIVES_FR:
  case RC_ICO_HARDDRIVES_ACSI:
  case RC_ICO_DRIVEA:case RC_ICO_DRIVEB:case RC_ICO_DRIVEB_DISABLED:
    return 64;
  case RC_ICO_SNAPSHOTFILEICO:
  case RC_ICO_APP256:
#ifndef DEBUG_BUILD
  case RC_ICO_STCLOSE:
#endif
    return 0;
  case RC_ICO_CFG:
    return 17;
  }
  return 16;
}


#if defined(SSE_GUI_TOOLBAR)
void GUIToolbarArrangeIcons(int cw) {
  GuiShowToolbar(OPTION_TOOLBAR);
  if(!OPTION_TOOLBAR)
    return;
  // toolbar is on top of client area, this function puts icons on proper places
  HWND handle;
  int x=0,y=0,w=ICON_SIZE_SMALL,h=ICON_SIZE_SMALL;
  RECT rc;
  GetClientRect(StemWin,&rc);
  int w0=rc.right-rc.left;
  if(BIG_ICONS)
  {
    h=ICON_SIZE_BIG;
    if(w0>=477)
      w=ICON_SIZE_BIG;
    else // getting out of my way to make it work!
      w=(ICON_SIZE_BIG*w0)/477;
  }
  int d=w+3; //?
  BOOL const repaint=FALSE;
  int offset=w+5;
  //left
  handle=GetDlgItem(StemWin,IDC_RESET);
  MoveWindow(handle,x,y,w,h,repaint);
  x+=d;
  handle=GetDlgItem(StemWin,IDC_PLAY);
  MoveWindow(handle,x,y,w,h,repaint);
  x+=d;
  handle=GetDlgItem(StemWin,IDC_FASTFORWARD);
  MoveWindow(handle,x,y,w,h,repaint);
  x+=d;
  handle=GetDlgItem(StemWin,IDC_SNAPSHOT);
  MoveWindow(handle,x,y,w,h,repaint);
  x+=d;
  handle=GetDlgItem(StemWin,IDC_SCREENSHOT);
  MoveWindow(handle,x,y,w,h,repaint);
  x+=d;
  handle=GetDlgItem(StemWin,IDC_PASTE);
  MoveWindow(handle,x,y,w,h,repaint);
#if defined(SSE_GUI_CONFIG_WRENCH)
  x+=d;
  handle=GetDlgItem(StemWin,IDC_CONFIGS);
  MoveWindow(handle,x,y,w,h,repaint);
#endif
  //right (from right to left)
  handle=GetDlgItem(StemWin,IDC_INFO);
  x=cw-offset;
  MoveWindow(handle,x,y,w,h,repaint);
#if defined(SSE_DEBUGGER_TOGGLE)
  handle=GetDlgItem(StemWin,IDC_DEBUGGER);
  x-=d;
  MoveWindow(handle,x,y,w,h,repaint);
#endif
  handle=GetDlgItem(StemWin,IDC_OPTIONS);
  x-=d;
  MoveWindow(handle,x,y,w,h,repaint);
  handle=GetDlgItem(StemWin,IDC_SHORTCUTS);
  x-=d;
  MoveWindow(handle,x,y,w,h,repaint);
  handle=GetDlgItem(StemWin,IDC_PATCHES);
  x-=d;
  MoveWindow(handle,x,y,w,h,repaint);
  handle=GetDlgItem(StemWin,IDC_JOYSTICKS);
  x-=d;
  MoveWindow(handle,x,y,w,h,repaint);
  x-=d;
  handle=GetDlgItem(StemWin,IDC_DISK_MANAGER);
  MoveWindow(handle,x,y,w,h,repaint);
#if defined(SSE_GUI_CONFIG_WRENCH)
  handle=GetDlgItem(StemWin,IDC_CONFIGS);
  ShowWindow(handle,SW_SHOW);
#endif
}


// The toolbar is optional in Windows builds, it's possible to use a classic menu instead
void GuiShowToolbar(BOOL show) {
  int nCmdShow=show?SW_SHOW:SW_HIDE;
  HWND handle;
  //left
  handle=GetDlgItem(StemWin,IDC_RESET);
  ShowWindow(handle,nCmdShow);
  handle=GetDlgItem(StemWin,IDC_PLAY);
  ShowWindow(handle,nCmdShow);
  handle=GetDlgItem(StemWin,IDC_FASTFORWARD);
  ShowWindow(handle,nCmdShow);
  handle=GetDlgItem(StemWin,IDC_SNAPSHOT);
  ShowWindow(handle,nCmdShow);
  handle=GetDlgItem(StemWin,IDC_SCREENSHOT);
  ShowWindow(handle,nCmdShow);
  handle=GetDlgItem(StemWin,IDC_PASTE);
  ShowWindow(handle,nCmdShow);
#if defined(SSE_GUI_CONFIG_WRENCH)
  handle=GetDlgItem(StemWin,IDC_CONFIGS);
  ShowWindow(handle,nCmdShow);
#endif
  //right (from right to left)
  handle=GetDlgItem(StemWin,IDC_INFO);
  ShowWindow(handle,nCmdShow);
#if defined(SSE_DEBUGGER_TOGGLE)
  handle=GetDlgItem(StemWin,IDC_DEBUGGER);
  ShowWindow(handle,nCmdShow);
#endif
  handle=GetDlgItem(StemWin,IDC_OPTIONS);
  ShowWindow(handle,nCmdShow);
  handle=GetDlgItem(StemWin,IDC_SHORTCUTS);
  ShowWindow(handle,nCmdShow);
  handle=GetDlgItem(StemWin,IDC_PATCHES);
  ShowWindow(handle,nCmdShow);
  handle=GetDlgItem(StemWin,IDC_JOYSTICKS);
  ShowWindow(handle,nCmdShow);
  handle=GetDlgItem(StemWin,IDC_DISK_MANAGER);
  ShowWindow(handle,nCmdShow);
#if defined(SSE_GUI_CONFIG_WRENCH)
  handle=GetDlgItem(StemWin,IDC_CONFIGS);
  ShowWindow(handle,nCmdShow);
#endif
}

#endif

#endif//WIN32

#if defined(SSE_GUI_STATUS_BAR)

char status_bar_text[4][64]; // need global
RECT status_bar_rc[4];

#if defined(SSE_EMU_THREAD)

void GUIRefreshStatusBar(BYTE part) {
  if(!OPTION_STATUS_BAR)
    return;
  DWORD id=GetCurrentThreadId();
  if(OPTION_EMUTHREAD && runstate!=RUNSTATE_STOPPED && id==EmuThreadId)
    PostMessage(StemWin,WM_USER,17,(LPARAM)part);
  else
    GUIRefreshStatusBar2(part);
}


void GUIRefreshStatusBar2(BYTE part) {
#else
void GUIRefreshStatusBar(BYTE part) {
#endif
  // part = mask of parts 0-3 to refresh, default $FF (all)

  ASSERTB(OPTION_STATUS_BAR);

  // parts of status bar can display 2 types of info at choice

  if(part&(1<<SB_PART_FREQ)) // Freq/Mouse
  {
#if defined(SSE_GUI_STATUS_BAR_DRAW_FREQ)
    InvalidateRect(hStatusBar,&status_bar_rc[SB_PART_FREQ],FALSE); // self-drawn
#else
#if !defined(SSE_GUI_STATUS_BAR_CLICKABLE)
#if defined(SSE_GUI_STATUS_BAR_MOUSE2)
    if(SSEConfig.MouseAd)
#else
    if(0)
#endif
#else
    if(SSEConfig.StatusBarMask&(1<<SB_PART_FREQ)) // display mouse coordinates in 1st pane
#endif
    {
#if defined(SSE_GUI_STATUS_BAR_MOUSE2)
      if(SSEConfig.MouseAd)
      {
        DU32 uxy(SafeLPeek(SSEConfig.MouseAd)); // emu detect or ini override only
        sprintf(status_bar_text[SB_PART_FREQ],"%d,%d",uxy.d16[HI],uxy.d16[LO]);
      }
      else
        sprintf(status_bar_text[SB_PART_FREQ],""); // delete // something else useful?
#elif defined(SSE_GUI_STATUS_BAR_MOUSE)
      DU32 uxy;
      if(SSEConfig.MouseAd)
      { // emu detect or ini override
        uxy.d32=SafeLPeek(SSEConfig.MouseAd);
      }
#if defined(SSE_HD6301_LL)
      else if(OPTION_C1 && (hd6301_peek(0xC9)&0xF8)==0xA8)
      { // mouse abs. mode (Star Trek)
        uxy.d8[3]=(BYTE)hd6301_peek(0xB6);
        uxy.d8[2]=(BYTE)hd6301_peek(0xB7);
        uxy.d8[1]=(BYTE)hd6301_peek(0xB8);
        uxy.d8[0]=(BYTE)hd6301_peek(0xB9);
      }
#endif
      else
      { // vq_mouse
        uxy.d16[HI]=Tos.MouseX;
        uxy.d16[LO]=Tos.MouseY;
      }
      sprintf(status_bar_text[SB_PART_FREQ],"%d,%d",uxy.d16[HI],uxy.d16[LO]);
#endif
    }
    else // display ST freq in 1st pane
    {
#if defined(SSE_VID_D3D)
#if defined(SSE_VID_D3D_VSYNC)
      if(Draw.VSync)
#else
      if(OPTION_WIN_VSYNC)
#endif
      { // would be fine green/red, but must draw ourselves, OK but slow for mouse coord.!
        // problem: vsync can fail even if freqs are OK (secondary monitor)
        sprintf(status_bar_text[SB_PART_FREQ],"%d/%dHz",
          Glue.PreviousVideoFreq,Disp.Freq); // at least indicate both freq ST/PC
      }
      else
#endif
        sprintf(status_bar_text[SB_PART_FREQ],"%dHz",Glue.PreviousVideoFreq);
      
    }
    PostMessage(hStatusBar,SB_SETTEXT,SB_PART_FREQ,(LPARAM)status_bar_text[SB_PART_FREQ]);
#endif
  }

  if(part&(1<<SB_PART_MAIN)) // show config in 2nd pane (if not, useful info)
  {
#if defined(SSE_GUI_STATUS_BAR_DRAW_MAIN)
    InvalidateRect(hStatusBar,&status_bar_rc[SB_PART_MAIN],FALSE); // self-drawn
#else
    status_bar_text[SB_PART_MAIN][0]='\0';
    if(Ikbd.Crashed)
      StatusInfo.MessageIndex=TStatusInfo::HD6301_CRASH;
    // build text of status bar, only if there's no special string
    // debug info: never show config
#if defined(SSE_GUI_STATUS_BAR_CLICKABLE)
    if(StatusInfo.MessageIndex==TStatusInfo::MESSAGE_NONE
      && (SSEConfig.StatusBarMask&(1<<SB_PART_MAIN)))
    {
      // basic ST/TOS/RAM
      char sb_st_model[20],sb_tos[5],sb_ram[7];
      if(PreciseModel.NotEmpty() && PreciseModel.Length()<20)
        sprintf(sb_st_model,"%s",PreciseModel.Text); // the only place where it's used...
      else
        sprintf(sb_st_model,"%s",st_model_name[ST_MODEL]);
      sprintf(sb_tos,"T%03x",tos_version);
      sprintf(sb_ram,"%dK",mem_len>>10);
      sprintf(status_bar_text[SB_PART_MAIN],"%s %s %s",sb_st_model,sb_tos,sb_ram);
      if(Disp.Method==DISPMETHOD_GDI)
        strcat(status_bar_text[SB_PART_MAIN]," GDI");
    }
#endif
    switch(StatusInfo.MessageIndex) {
    case TStatusInfo::MESSAGE_NONE:
#if defined(SSE_GUI_STATUS_BAR_CLICKABLE)
      if(!(SSEConfig.StatusBarMask&(1<<SB_PART_MAIN)))
#endif
      {
#if defined(SSE_420R5) // shorter captions for the big GUI
        if(SSEOptions.F12Run)
        {
          if(runstate==RUNSTATE_STOPPED)
            strcpy(status_bar_text[SB_PART_MAIN],T("F12: start"));
          else
            strcpy(status_bar_text[SB_PART_MAIN],(stem_mousemode==STEM_MOUSEMODE_DISABLED)
              ? T("F11: grab mouse F12: stop") : T("F11: free mouse F12: stop"));
        }
        else if(SSEOptions.PauseRun)
        {
          if(runstate==RUNSTATE_STOPPED)
            strcpy(status_bar_text[SB_PART_MAIN],T("Pause: start"));//argh! sounds silly
          else
            strcpy(status_bar_text[SB_PART_MAIN],(stem_mousemode==STEM_MOUSEMODE_DISABLED)
              ? T("Pause: stop, shift+Pause: grab mouse")
              : T("Pause: stop, shift+Pause: free mouse"));
        }
#else
        if(SSEOptions.F12Run)
        {
          if(runstate==RUNSTATE_STOPPED)
            strcpy(status_bar_text[SB_PART_MAIN],T("F12: start emu"));
          else
            strcpy(status_bar_text[SB_PART_MAIN],(stem_mousemode==STEM_MOUSEMODE_DISABLED)
              ? T("F11: capture mouse F12: stop emu") : T("F11: free mouse F12: stop emu"));
        }
        else if(SSEOptions.PauseRun)
        {
          if(runstate==RUNSTATE_STOPPED)
            strcpy(status_bar_text[SB_PART_MAIN],T("Pause: start emu"));
          else
            strcpy(status_bar_text[SB_PART_MAIN],(stem_mousemode==STEM_MOUSEMODE_DISABLED)
              ? T("Pause: stop emu, shift+Pause: mouse on")
              : T("Pause: stop emu, shift+Pause: free mouse"));
        }
#endif
      }
      break;
    case TStatusInfo::MC68000_CRASH:
      strcpy(status_bar_text[SB_PART_MAIN],T("HALT"));
      break;
    case TStatusInfo::BLIT_ERROR:
      strcpy(status_bar_text[SB_PART_MAIN],T("BLIT ERROR")); // funny message
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
    PostMessage(hStatusBar,SB_SETTEXT,SB_PART_MAIN,(LPARAM)status_bar_text[SB_PART_MAIN]);
#endif//#if defined(SSE_GUI_STATUS_BAR_DRAW_MAIN)
  }//if(part&(1<<SB_PART_MAIN))

  if(part&(1<<SB_PART_ICONS)) // icons
  {
#if defined(SSE_GUI_STATUS_BAR_DRAW_ICONS)
    InvalidateRect(hStatusBar,&status_bar_rc[SB_PART_ICONS],FALSE); // self-drawn
#else
    // TODO take back old code in case we don't draw icons?
#endif
  }

  // CAPS NUM SCRL or Disks
  if(part&(1<<SB_PART_CAPS)) // keyboard state/disk
  {
#if defined(SSE_GUI_STATUS_BAR_DRAW_CAPS) //no
    InvalidateRect(hStatusBar,&status_bar_rc[SB_PART_CAPS],FALSE); // self-drawn
#else
#if defined(SSE_GUI_STATUS_BAR_CLICKABLE)
    if(SSEConfig.StatusBarMask&(1<<SB_PART_CAPS))
    {
      char *s=status_bar_text[SB_PART_CAPS];
      for(int i=0;i<DiskMan.nFloppyDrives;i++)
      {
        if(i<DiskMan.nFloppyDrives && FloppyDrive[i].ImageType.RealExtension<NUM_EXT)
        {
          sprintf(s," %c:%s",'A'+i,extension_list[FloppyDrive[i].ImageType.RealExtension]);
          if(FloppyDisk[i].Density==2)
            strcat(s," (HD)");
          s=status_bar_text[SB_PART_CAPS]+strlen(status_bar_text[SB_PART_CAPS]);
        }
      }
    }
    else
#endif//#if defined(SSE_GUI_STATUS_BAR_CLICKABLE)
    {
      status_bar_text[SB_PART_CAPS][0]='\0';
      if(GetKeyState(VK_CAPITAL)&1)
        strcat(status_bar_text[SB_PART_CAPS],T("CAPS "));
      if(GetKeyState(VK_NUMLOCK)&1)
        strcat(status_bar_text[SB_PART_CAPS],T("NUM "));
      if(GetKeyState(VK_SCROLL)&1)
        strcat(status_bar_text[SB_PART_CAPS],T("SCRL "));
    }
    if(!(part&(1<<SB_PART_ICONS))) // not twice
      InvalidateRect(hStatusBar,&status_bar_rc[SB_PART_ICONS],FALSE);
    PostMessage(hStatusBar,SB_SETTEXT,SB_PART_CAPS,(LPARAM)status_bar_text[SB_PART_CAPS]);
#endif//#if defined(SSE_GUI_STATUS_BAR_DRAW_CAPS)
  }
}

#elif defined(SSE_UNIX_STATUSBAR)

void GUIRefreshStatusBar() {
  //return;
  char oldtext[256];
  strcpy(oldtext,status_bar_text);
  switch(StatusInfo.MessageIndex) {
  case TStatusInfo::MESSAGE_NONE:
  {
    if(runstate==RUNSTATE_RUNNING)
      strcpy(status_bar_text,"F11 mouse F12 stop");
    else
    {
      // basic ST/TOS/RAM
      char sb_st_model[32],sb_tos[16],sb_ram[16];
      if(PreciseModel.NotEmpty() && PreciseModel.Length()<20)
        sprintf(sb_st_model,"%s",PreciseModel.Text); // the only place where it's used...
      else
        sprintf(sb_st_model,"%s",st_model_name[ST_MODEL]);
      sprintf(sb_tos,"T%03x",tos_version);
      sprintf(sb_ram,"%dK",mem_len>>10);
      sprintf(status_bar_text,"%s %s %s",sb_st_model,sb_tos,sb_ram);
    }
    break;
  }
  case TStatusInfo::MC68000_CRASH:
    strcpy(status_bar_text,T("HALT"));
    break;
  case TStatusInfo::BLIT_ERROR:
    strcpy(status_bar_text,T("BLIT ERROR")); // funny message
    break;
  case TStatusInfo::X86_CRASH:
    strcpy(status_bar_text,T(STEEM_CRASH_TXT));
    break;
  case TStatusInfo::HD6301_CRASH:
    strcpy(status_bar_text,T("6301 CRASH"));
    break;
  default:
    strcpy(status_bar_text,StatusInfo.text);
    break;
  }//sw
  
  //switch(DISPLAY_SIZE)

  if(strcmp(oldtext,status_bar_text))
  {
    StatusBar.set_text(status_bar_text);
    StatusBar.draw(true);
  }
}

#endif//#if defined(SSE_GUI_STATUS_BAR)

#if !defined(SSE_LIBRETRONUKE)
void GUIRunStart() {

#ifdef WIN32
#if defined(SSE_EMU_THREAD)
  if(!OPTION_EMUTHREAD) // keep timer
#endif
    KillTimer(StemWin,SHORTCUTS_TIMER_ID);
  if(!bAllowTaskSwitch)
    DisableTaskSwitch();
  if(HighPriority) 
    SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);
#endif

#if defined(UNIX) && defined(PRIO_PROCESS) && defined(PRIO_MAX)
  hxc::kill_timer(StemWin,SHORTCUTS_TIMER_ID);
  if(HighPriority)
    setpriority(PRIO_PROCESS,0,PRIO_MAX);
#endif

  CheckResetDisplay(true);

#ifdef WIN32
#ifndef SSE_NO_SCREENSAVER
  if(FullScreen)
    TScreenSaver::killTimer();
#endif
#endif
}


BOOL GUIPauseWhenInactive() { // only & unconditionally called by event_vbl_interrupt()
  if((bPauseWhenInactive && !bAppActive)||timer<CutPauseUntilSysEx_Time) 
  {
    bool MouseWasCaptured=(stem_mousemode==STEM_MOUSEMODE_WINDOW);
    if(FullScreen==0) 
      SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
    SoundStop();
#ifdef WIN32
    SetWindowText(StemWin,Str("Steem - ")+T("Suspended"));
    MSG mess;
    SetTimer(StemWin,MIDISYSEX_TIMER_ID,100,NULL);
#if defined(SSE_EMU_THREAD)
    if(OPTION_EMUTHREAD && hEmuThread && runstate==RUNSTATE_RUNNING)
    {
      while(!bAppActive)
        Sleep(200);
    }
    else
#endif
    {
      while(GetMessage(&mess,NULL,0,0))
      {
        if(HandleMessage(&mess))
        {
          TranslateMessage(&mess);
          DispatchMessage(&mess);
        }
        if((!bPauseWhenInactive||bAppActive) && timeGetTime()>CutPauseUntilSysEx_Time)
          break;
        if(runstate!=RUNSTATE_RUNNING) 
          break;
      }
      if(mess.message==WM_QUIT) 
        QuitSteem();
    }
    KillTimer(StemWin,MIDISYSEX_TIMER_ID);
    SetWindowText(StemWin,stem_window_title);
#endif//WIN32

#ifdef UNIX
    XEvent Ev;
    do {
      if(hxc::wait_for_event(XD,&Ev,200)) ProcessEvent(&Ev);

      if(timeGetTime()>CutPauseUntilSysEx_Time&&(bPauseWhenInactive==0||bAppActive)) break;
    } while(runstate==RUNSTATE_RUNNING && !Quitting);
#endif

    if(FullScreen==0 && MouseWasCaptured && GetForegroundWindow()==StemWin)
      SetStemMouseMode(STEM_MOUSEMODE_WINDOW);
    SoundStart();
    return TRUE;
  }
  return FALSE;
}


void GUIRunEnd() {
  slow_motion=0;
  if(fast_forward) 
  {
    fast_forward=0;
    flashlight(false);
#ifdef WIN32
    SendMessage(GetDlgItem(StemWin,IDC_FASTFORWARD),BM_SETCHECK,0,0);
#endif
#ifdef UNIX
    FastBut.set_check(0);
#endif
  }
#ifdef WIN32
  if(HighPriority)
    SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
#endif

#if defined(UNIX) && defined(PRIO_PROCESS)
  setpriority(PRIO_PROCESS,0,0);
#endif

  SetStemMouseMode(STEM_MOUSEMODE_DISABLED);

#ifdef WIN32
  EnableTaskSwitch();
#if defined(SSE_EMU_THREAD)
  if(!OPTION_EMUTHREAD) // timer wasn't stopped (I hope)
#endif
    SetTimer(StemWin,SHORTCUTS_TIMER_ID,50,NULL);
  OptionBox.MachineUpdateIfVisible();
#ifndef SSE_NO_SCREENSAVER
  if(FullScreen)
    TScreenSaver::prepareTimer();
#endif
  SendMessage(GetDlgItem(StemWin,IDC_PLAY),BM_SETCHECK,0,0); // reset play button
#if defined(SSE_GUI_TOOLBAR)
  if(!FullScreen)
    SendMessage(StemWin,WM_SETICON,ICON_SMALL,(LPARAM)hGUIIcon[RC_ICO_APP]); // restore steem icon
#endif
#if !defined(SSE_420R5) // bug: runstate not stopped
  UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
#endif
#endif//WIN32

#ifdef UNIX
  hxc::set_timer(StemWin,SHORTCUTS_TIMER_ID,50,timerproc,NULL);
#endif

#if defined(SSE_GEM_CONTROL_PANEL)
  OptionBox.MachineUpdateIfVisible();
#endif

}


void GUIColdResetChangeSettings() { // only called by reset_st()
#if defined(SSE_GUI_INSTANTCHANGE)
  if(OptionBox.NewStModel>=0)
  {
    SSEConfig.SwitchSTModel((BYTE)OptionBox.NewStModel);
    OptionBox.NewStModel=-1;
  }
#endif
  if(OptionBox.NewMemConf0==-1) 
  {
    if((SSEConfig.bank_length[0]+SSEConfig.bank_length[1])==(MEM_512KB+MEM_128KB))
    {  // Old 512Kb
      OptionBox.NewMemConf0=MEMCONF_512;
      OptionBox.NewMemConf1=MEMCONF_512K_BANK1_CONF;
    }
#if !defined(SSE_MMU_2560K)
    else if((SSEConfig.bank_length[0]+SSEConfig.bank_length[1])==(MEM_2MB+MEM_512KB))
    {  // Old 2Mb
      OptionBox.NewMemConf0=MEMCONF_2MB;
      OptionBox.NewMemConf1=MEMCONF_2MB_BANK1_CONF;
    }
#endif
  }
  if(OptionBox.NewMemConf0>=0) 
  {
    SSEConfig.make_Mem((BYTE)OptionBox.NewMemConf0,(BYTE)OptionBox.NewMemConf1);
    OptionBox.NewMemConf0=-1;
  }
  if(OptionBox.NewMonitorSel>=0) 
  {
#ifndef NO_CRAZY_MONITOR
    bool old_em=(extended_monitor!=0);
#endif
    switch(OptionBox.NewMonitorSel) {
    case 0:
    case 1:
      extended_monitor=0;
      SSEConfig.UpdateMonitor(!OptionBox.NewMonitorSel);
      break;
    default:
    { //crazy monitor
#ifndef NO_CRAZY_MONITOR
      int m=OptionBox.NewMonitorSel-2;
      SSEConfig.UpdateMonitor((extmon_res[m][2]==1));
      extended_monitor=1;
      em_width=extmon_res[m][0];
      em_height=extmon_res[m][1];
      em_planes=(BYTE)extmon_res[m][2];
      border=0;
#endif
    }//case
    }//sw
#ifndef NO_CRAZY_MONITOR
    //if(extended_monitor!=old_em||extended_monitor)
    if(old_em||extended_monitor)
    {
      if(FullScreen)
        change_fullscreen_display_mode(true);
      else
        Disp.ScreenChange(); // For extended monitor
    }
    else
#endif
      ChangeBorderSize(border);
    OptionBox.NewMonitorSel=-1;
  }
  if(OptionBox.NewROMFile.IsEmpty())
    OptionBox.NewROMFile=ROMFile; // force reload
  if(OptionBox.NewROMFile.NotEmpty()) 
  {
    if(!load_TOS(OptionBox.NewROMFile))
      Alert(T("Can't load ")+" "+OptionBox.NewROMFile,T("ERROR"),MB_ICONEXCLAMATION);
    else
      ROMFile=OptionBox.NewROMFile;
    OptionBox.NewROMFile="";
  }
}


void GUISaveResetBackup() {
  DeleteFile(TempPath+SLASH+FILE_LOADUNDOSNAPSHOT);
#if defined(SSE_VID_LS)
  bool old=SSEOptions.ScreenshotWithSnapshot;
  SSEOptions.ScreenshotWithSnapshot=false;
#endif
  SaveSnapShot(TempPath+SLASH+FILE_RESETSNAPSHOT,-1,false); // Don't add to history
#if defined(SSE_VID_LS)
  SSEOptions.ScreenshotWithSnapshot=old;
#endif
}


void LoadAllIcons(TConfigStoreFile *pCSF,bool FirstCall) {
#ifdef WIN32
#ifdef ONEGAME
  for(int n=1;n<RC_NUM_ICONS;n++) 
    hGUIIcon[n]=NULL;
#else
  HICON hOld[RC_NUM_ICONS],hOldSmall[RC_NUM_ICONS];
#if defined(SSE_GUI_DM_BIGICONS)
  HICON hOld128[RC_NUM_ICONS];
#endif
  for(int n=1;n<RC_NUM_ICONS;n++) 
  {
    hOld[n]=hGUIIcon[n];
    hOldSmall[n]=hGUIIconSmall[n];
#if defined(SSE_GUI_DM_BIGICONS)
    hOld128[n]=hGUIIcon128[n];
#endif
  }
  bool UseDefault=false;
  HDC dc=GetDC(NULL);
  if(GetDeviceCaps(dc,BITSPIXEL)<=8)
    UseDefault=(pCSF->GetInt("Icons","UseDefaultIn256",0)!=0);
  ReleaseDC(NULL,dc);
  Str File;
  for(WORD n=1;n<RC_NUM_ICONS;n++) 
  {
    int size=RCGetSizeOfIcon(n);
#ifdef SSE_421R0
    bool load16too=((size&1) || size==16);
#else
    bool load16too=size & 1;
#endif
    size&=~1;
    hGUIIcon[n]=NULL;
    hGUIIconSmall[n]=NULL;
    if(size)
    {
      if(!UseDefault)
        File=pCSF->GetStr("Icons",Str("Icon")+n,"");
#if defined(SSE_GUI_BIGICONS)
      if(!BIG_ICONS||size>=32)
#endif
      {
      if(File.NotEmpty()) 
        hGUIIcon[n]=(HICON)LoadImage(hInstance,File,IMAGE_ICON,size,size,LR_LOADFROMFILE);
      if(hGUIIcon[n]==NULL) 
        hGUIIcon[n]=(HICON)LoadImage(hInstance,RCNUM(n),IMAGE_ICON,size,size,0);
      }
      if(load16too) 
      {
        if(File.NotEmpty()) 
          hGUIIconSmall[n]=(HICON)LoadImage(hInstance,File,IMAGE_ICON,16,16,LR_LOADFROMFILE);
        if(hGUIIconSmall[n]==NULL) 
          hGUIIconSmall[n]=(HICON)LoadImage(hInstance,RCNUM(n),IMAGE_ICON,16,16,0);  
      }
#if defined(SSE_GUI_BIGICONS)
      // Bit of a hack, we load the same icons as 32x32 instead of 16x16... call to designers...
      if(BIG_ICONS && size<32)
      {
        if(File.NotEmpty())
          hGUIIcon[n]=(HICON)LoadImage(hInstance,File,IMAGE_ICON,32,32,LR_LOADFROMFILE);
        if(hGUIIcon[n]==NULL)
          hGUIIcon[n]=(HICON)LoadImage(hInstance,RCNUM(n),IMAGE_ICON,32,32,0);
      }
#endif
#if defined(SSE_GUI_DM_BIGICONS)
      // Look if there's a big (128) icon too
      size=128;
      if(File.NotEmpty()) 
        hGUIIcon128[n]=(HICON)LoadImage(hInstance,File,IMAGE_ICON,size,size,LR_LOADFROMFILE);
      if(hGUIIcon128[n]==NULL)
        hGUIIcon128[n]=(HICON)LoadImage(hInstance,RCNUM(n),IMAGE_ICON,size,size,0);
      if(hGUIIcon128[n]==NULL)
        hGUIIcon128[n]=hGUIIcon[n]; // if not
#endif
    }//if
  }//nxt
  if(!FirstCall)
  {
    // Update all window classes, buttons and other icon thingies
    SetClassLongPtr(StemWin,GCLP_HICON,(LONG_PTR)hGUIIcon[RC_ICO_APP]);
#ifdef DEBUG_BUILD
    SetClassLongPtr(DWin,GCLP_HICON,(LONG_PTR)hGUIIcon[RC_ICO_BOMB]);
    SetClassLongPtr(trace_window_handle,GCLP_HICON,(LONG_PTR)hGUIIcon[RC_ICO_STCLOSE]);
    for(int n=0;n<MAX_MEMORY_BROWSERS;n++)
      if(m_b[n])
        m_b[n]->update_icon();
#endif
    for(int n=0;n<nStemDialogs;n++)
      DialogList[n]->UpdateMainWindowIcon();
#if !defined(SSE_GUI_CONFIG_WRENCH)
    for(int id=IDC_DISK_MANAGER;id<IDC_CONFIGS;id++)
#else
    for(int id=IDC_DISK_MANAGER;id<=IDC_CONFIGS;id++)
#endif
    {
      if(HWND hwnd=GetDlgItem(StemWin,id))
        PostMessage(hwnd,BM_RELOADICON,0,0);
    }
    DiskMan.LoadIcons();
    OptionBox.LoadIcons();
    InfoBox.LoadIcons();
    ShortcutBox.UpdateDirectoryTreeIcons(&(ShortcutBox.DTree));
    if(ShortcutBox.pChooseMacroTree)
      ShortcutBox.UpdateDirectoryTreeIcons(ShortcutBox.pChooseMacroTree);
    for(int n=1;n<RC_NUM_ICONS;n++) 
    {
      if(hOld[n] /*&& !BIG_ICONS*/) 
        DestroyIcon(hOld[n]);
      if(hOldSmall[n]) 
        DestroyIcon(hOldSmall[n]);
#if defined(SSE_GUI_DM_BIGICONS)
      if(hOld128[n])
        DestroyIcon(hOld128[n]);
#endif

    }
  }
#endif
#endif//WIN32
}

#define LOGSECTION LOGSECTION_INIT


// Steem creates its full GUI programmatically, not through resource files
// MakeGUI() is only called once by Initialise()
BOOL MakeGUI() {
#ifdef WIN32
  GuiSM.Update();
  fnt=SSEConfig.GuiFont();
  MidGUIRGB=GetMidColour(GetSysColor(COLOR_3DFACE),GetSysColor(COLOR_WINDOW));
  DkMidGUIRGB=GetMidColour(GetSysColor(COLOR_3DFACE),MidGUIRGB);
  PCArrow=LoadCursor(NULL,IDC_ARROW);
  ParentWin=GetDesktopWindow();
  WNDCLASS wc={0,WndProc,0,0,hInstance,hGUIIcon[RC_ICO_APP],PCArrow,NULL,NULL,"Steem Window"}; // don't change name
  RegisterClass(&wc);
  wc.lpfnWndProc=FSQuitWndProc;
  wc.lpszClassName="Steem Fullscreen Quit Button";
  RegisterClass(&wc);
#if !defined(SSE_NO_OSD) && !defined(SSE_OSD_NORESETINFO)
  wc.lpfnWndProc=ResetInfoWndProc;
  wc.lpszClassName="Steem Reset Info Window";
  RegisterClass(&wc);
#endif
  RegisterSteemControls();
  RegisterButtonPicker();
  // Main Steem window
  StemWin=CreateWindowEx(WS_EX_ACCEPTFILES,"Steem Window",stem_window_title,WS_CAPTION|WS_SYSMENU
                         |WS_MINIMIZEBOX|(OPTION_BLOCK_RESIZE?0:WS_SIZEBOX)|WS_MAXIMIZEBOX
                         |WS_CLIPSIBLINGS,CW_USEDEFAULT,CW_USEDEFAULT,640,480,ParentWin,NULL,hInstance,NULL);
  if(!IsWindow(StemWin)) 
    StemWin=NULL;
  if(StemWin==NULL) 
    return FALSE;
  StemWin_SysMenu=GetSystemMenu(StemWin,0); // Window menu
  int pos=GetMenuItemCount(StemWin_SysMenu)-2;
  InsertMenu(StemWin_SysMenu,pos,MF_BYPOSITION|MF_STRING,IDSYS_SMALLER,T("Smaller Window"));
  InsertMenu(StemWin_SysMenu,pos,MF_BYPOSITION|MF_STRING,IDSYS_BIGGER,T("Bigger Window"));
  InsertMenu(StemWin_SysMenu,pos,MF_BYPOSITION|MF_SEPARATOR,0,NULL);
  InsertMenu(StemWin_SysMenu,pos,MF_BYPOSITION|MF_STRING,IDSYS_BORDEROFF,T("Border Off"));
  InsertMenu(StemWin_SysMenu,pos,MF_BYPOSITION|MF_STRING,IDSYS_BORDERON,T("Border On"));
  InsertMenu(StemWin_SysMenu,pos,MF_BYPOSITION|MF_SEPARATOR,0,NULL);
  InsertMenu(StemWin_SysMenu,pos,MF_BYPOSITION|MF_STRING,IDSYS_NOOSD,T("Disable On Screen Display"));
  InsertMenu(StemWin_SysMenu,pos,MF_BYPOSITION|MF_SEPARATOR,0,NULL);
#if !defined(SSE_NO_AOT)
  InsertMenu(StemWin_SysMenu,pos,MF_BYPOSITION|MF_STRING,IDSYS_TOP,T("Always On Top"));
#endif
  InsertMenu(StemWin_SysMenu,pos,MF_BYPOSITION|MF_STRING,IDSYS_ASPECT,T("Restore Aspect Ratio"));
  InsertMenu(StemWin_SysMenu,pos,MF_BYPOSITION|MF_STRING,IDSYS_NORMAL,T("Normal Size"));
  InsertMenu(StemWin_SysMenu,pos,MF_BYPOSITION|MF_STRING,IDSYS_POSITION,T("Restore Position")); // just in case
  InsertMenu(StemWin_SysMenu,pos,MF_BYPOSITION|MF_SEPARATOR,0,NULL);
#if defined(SSE_GUI_MENUBAR)
  InsertMenu(StemWin_SysMenu,pos,MF_BYPOSITION|MF_STRING|MF_CHECK(OPTION_MENUBAR),
             IDC_MENUBAR,T("Menu Bar"));
#endif
#if defined(SSE_GUI_TOOLBAR)
  InsertMenu(StemWin_SysMenu,pos,MF_BYPOSITION|MF_STRING|MF_CHECK(OPTION_TOOLBAR),
             IDSYS_TOOLBAR,T("Tool Bar"));
  InsertMenu(StemWin_SysMenu,pos,MF_BYPOSITION|MF_STRING,IDSYS_RUNSTOP,T("Run/Stop (F12)")); // just in case
#endif
  InsertMenu(StemWin_SysMenu,pos,MF_BYPOSITION|MF_SEPARATOR,0,NULL);
#if defined(SSE_GUI_MENUBAR)
  StemWinMenu=CreateMenu();
  StemWinMenuFile=CreatePopupMenu();
  StemWinMenuEmu=CreatePopupMenu();
  StemWinMenuTools=CreatePopupMenu();
  AppendMenu(StemWinMenu,MF_POPUP|MF_STRING,(UINT_PTR)StemWinMenuFile,"&File");
  AppendMenu(StemWinMenu,MF_POPUP|MF_STRING,(UINT_PTR)StemWinMenuEmu,"&Emu");
  AppendMenu(StemWinMenu,MF_POPUP|MF_STRING,(UINT_PTR)StemWinMenuTools,"&Tools");
  // File
  AppendMenu(StemWinMenuFile,MF_STRING,IDC_MENUDISKMAN,"&Disk Manager");
  AppendMenu(StemWinMenuFile,MF_STRING,IDC_MENUINSERTA,T("Insert Disk &A"));
  AppendMenu(StemWinMenuFile,MF_STRING,IDC_MENUINSERTB,T("Insert Disk &B"));
#if defined(SSE_DISK_SWAPPER)
  AppendMenu(StemWinMenuFile,MF_STRING,IDC_MENUSWAPPERPREV,T("Disk swapper &Previous"));
  AppendMenu(StemWinMenuFile,MF_STRING,IDC_MENUSWAPPERNEXT,T("Disk swapper &Next"));
#endif
  AppendMenu(StemWinMenuFile,MF_STRING,IDC_MENUGEMDOSHD,T("&GEMDOS Hard discs"));
#if defined(SSE_ACSI_MNGR)
  AppendMenu(StemWinMenuFile,MF_STRING,IDC_MENUACSIHD,T("A&CSI Hard discs"));
#endif
#if defined(SSE_GUI_CONFIG_WRENCH)
  AppendMenu(StemWinMenuFile,MF_STRING,IDC_LOADCONFIG,T("&Load configuration file"));
  AppendMenu(StemWinMenuFile,MF_STRING,IDC_SAVECONFIG,T("&Save configuration file"));
#endif
  AppendMenu(StemWinMenuFile,MF_STRING,IDC_LOADSNAPSHOT,T("L&oad snapshot file"));
  AppendMenu(StemWinMenuFile,MF_STRING,IDC_SAVESNAPSHOT,T("Sa&ve snapshot file"));
  AppendMenu(StemWinMenuFile,MF_STRING,IDC_MENUEXIT,"E&xit");
  // Emu
  AppendMenu(StemWinMenuEmu,MF_STRING,IDC_MENURUNSTOP,"&Run/Stop (F12)");
  AppendMenu(StemWinMenuEmu,MF_STRING,IDC_MENUREBOOT,"Re&boot");
  AppendMenu(StemWinMenuEmu,MF_STRING,IDC_MENURESET,"Re&set");
  AppendMenu(StemWinMenuEmu,MF_STRING,IDC_UNDORESET,"&Undo last reset");
  AppendMenu(StemWinMenuEmu,MF_STRING,IDC_MENUTOGGLEFULLSCREEN,"Toggle &Fullsceen (Alt-Enter)");
  AppendMenu(StemWinMenuEmu,MF_STRING,IDC_MENUPATCHESBOX,"Patches");
#if defined(SSE_EMU_THREAD)
  AppendMenu(StemWinMenuEmu,MF_STRING|MF_DISABLED,IDC_MENUKILLTHREAD,"&Kill emu thread");
#endif
  // Tools
#if defined(SSE_DEBUGGER_TOGGLE)
  AppendMenu(StemWinMenuTools,MF_STRING,IDC_DEBUGGER,"&Debugger");
#endif
#if defined(SSE_GUI_TOOLBAR)
  AppendMenu(StemWinMenuTools,MF_STRING,IDC_MENUTOOLBAR,"&Tool bar");
#endif
  //AppendMenu(StemWinMenuTools,MF_STRING,IDC_MENUOPTIONS,"&Options");
  AppendMenu(StemWinMenuTools,MF_STRING,IDC_MENUOPTIONS,"&Settings");
  AppendMenu(StemWinMenuTools,MF_STRING,IDC_MENUSHORTCUTS,"&Shortcuts");
  AppendMenu(StemWinMenuTools,MF_STRING,IDC_MENUJOYSTICKS,"&Joysticks");
  AppendMenu(StemWinMenuTools,MF_STRING,IDC_MENUINFO,"&Info");
#endif
  ToolTip=CreateWindowEx(WS_EX_TOPMOST,TOOLTIPS_CLASS,NULL,TTS_ALWAYSTIP|TTS_NOPREFIX,
                         0,0,100,100,NULL,NULL,hInstance,NULL);
  SendMessage(ToolTip,TTM_SETDELAYTIME,TTDT_AUTOPOP,20000); // 20 seconds before disappear
  SendMessage(ToolTip,TTM_SETDELAYTIME,TTDT_INITIAL,400);   // 0.4 second before appear
  SendMessage(ToolTip,TTM_SETDELAYTIME,TTDT_RESHOW,200);     // 0.2 moving from one tool to next
  SendMessage(ToolTip,TTM_SETMAXTIPWIDTH,0,400);

#if defined(SSE_GUI_TOOLBAR)
#ifndef ONEGAME
  // note Str(RC_ICO...) is no cast
  HWND Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_RESET),WS_CHILDWINDOW
                        |WS_VISIBLE|PBS_RIGHTCLICK,0,0,0,0,StemWin,(HMENU)IDC_RESET,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Reset (Left Click = Warm, Right Click = Cold)"));
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_PLAY),WS_CHILDWINDOW|WS_VISIBLE
                   |WS_TABSTOP|PBS_RIGHTCLICK,0,0,0,0,StemWin,(HMENU)IDC_PLAY,hInstance,NULL);
#endif
  ToolAddWindow(ToolTip,Win,T("Run (Left Click = Run/Stop, Right Click = Slow Motion)"));
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_FF),WS_CHILDWINDOW|WS_VISIBLE|PBS_RIGHTCLICK
                   |PBS_DBLCLK,0,0,0,0,StemWin,(HMENU)IDC_FASTFORWARD,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Fast Forward (Right Click = Searchlight, Double Click = Sticky)"));
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_SNAPSHOTBUT),WS_CHILDWINDOW|WS_VISIBLE,
                   0,0,0,0,StemWin,(HMENU)IDC_SNAPSHOT,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Memory Snapshot Menu"));
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_TAKESCREENSHOTBUT),WS_CHILDWINDOW|WS_VISIBLE
                   |PBS_RIGHTCLICK,0,0,0,0,StemWin,(HMENU)IDC_SCREENSHOT,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Take Screenshot")+" ("+T("Right Click = Options")+")");
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_PASTE),WS_CHILD|WS_VISIBLE|PBS_RIGHTCLICK,
                   0,0,0,0,StemWin,(HMENU)IDC_PASTE,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Paste Text Into ST (Right Click = Options)"));
#ifdef RELEASE_BUILD 
  // This causes freeze up if tracing in debugger, so only do it in final build
  NextClipboardViewerWin=SetClipboardViewer(StemWin);
#endif
  UpdatePasteButton();
#if !defined(SSE_NO_UPDATE)
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_UPDATE),WS_CHILD,
                          x,0,20,20,StemWin,(HMENU)120,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Steem Update Available! Click Here For Details!"));
#endif
#if defined(SSE_GUI_CONFIG_WRENCH)
  // 'wrench' icon for config files, popup menu when left click
  // it only doubles the feature in Option box but I didn't know!
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_CFG),WS_CHILDWINDOW|WS_VISIBLE,
                   0,0,0,0,StemWin,(HMENU)IDC_CONFIGS,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Load/save configuration file"));
#endif
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_PATCHES),WS_CHILD|WS_VISIBLE,
                   0,0,0,0,StemWin,(HMENU)IDC_PATCHES,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Patches"));
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_SHORTCUT),WS_CHILD|WS_VISIBLE,
                   0,0,0,0,StemWin,(HMENU)IDC_SHORTCUTS,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Shortcuts"));
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_SETTINGS),WS_CHILD|WS_VISIBLE,
                   0,0,0,0,StemWin,(HMENU)IDC_OPTIONS,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Settings"));
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_JOY),WS_CHILDWINDOW|WS_VISIBLE,
                   0,0,0,0,StemWin,(HMENU)IDC_JOYSTICKS,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Joystick Configuration"));
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_DISKMAN),WS_CHILDWINDOW|WS_VISIBLE,
                   0,0,0,0,StemWin,(HMENU)IDC_DISK_MANAGER,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Disk Manager"));
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_INFO),WS_CHILD|WS_VISIBLE,
                   0,0,0,0,StemWin,(HMENU)IDC_INFO,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("General Info"));
#if defined(SSE_DEBUGGER_TOGGLE)
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_BOMB),WS_CHILD|WS_VISIBLE,
    0,0,0,0,StemWin,(HMENU)IDC_DEBUGGER,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Debugger"));
#endif
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_TOWINDOW),WS_CHILD,
                   0,0,20+BIG_ICONS*16,20+BIG_ICONS*16,StemWin,(HMENU)IDC_BACKTOWIN,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Windowed Mode"));
  Win=CreateWindow("Steem Fullscreen Quit Button","",WS_CHILD,
    0,0,20+BIG_ICONS*16,20+BIG_ICONS*16,StemWin,(HMENU)IDC_QUIT,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Quit Steem"));
#endif//#if defined(SSE_GUI_TOOLBAR)
  SetWindowAndChildrensFont(StemWin,fnt);
#ifndef ONEGAME
#if !defined(SSE_GUI_RESTOREPOS)
  CentreWindow(StemWin,false);
#endif
#else
  MoveWindow(StemWin,GetScreenWidth(),0,100,100,0);
#endif
#ifdef DEBUG_BUILD
  DWin_init();
#endif
#endif//WIN32

#ifdef UNIX
  if (XD==NULL) return 0;

  UNIX_get_fake_VKs();
  RunSteemAtom=XInternAtom(XD,"SteemRun",0);
  LoadSnapShotAtom=XInternAtom(XD,"SteemLoadSnapShot",0);
#ifdef ALLOW_XALLOCID
  SteemWindowGroup=XAllocID(XD);
#endif

  XSetWindowAttributes swa;
  swa.backing_store=NotUseful;
  swa.colormap=colormap;
  StemWin=XCreateWindow(XD,
  				XDefaultRootWindow(XD),
                200,
                200,
                2+320+2,
                MENUHEIGHT+2+200+2,
                0,
                CopyFromParent,
                InputOutput,
                CopyFromParent, 
                CWBackingStore | int(colormap ? CWColormap:0),
                &swa);
  if (StemWin==0) return 0;

  hxc::load_res(XD);

  Atom Prots[1]={hxc::XA_WM_DELETE_WINDOW};
  XSetWMProtocols(XD,StemWin,Prots,1);

  DispGC=XCreateGC(XD,StemWin,0,NULL);

#if defined(SSE_UNIX) //&& defined(SSE_GUI_WINDOW_TITLE)
  XSetStandardProperties(XD,StemWin,WINDOW_TITLE,"Steem",None,_argv,_argc,NULL);
#else
  XSetStandardProperties(XD,StemWin,"Steem Engine","Steem",None,_argv,_argc,NULL);
#endif

  StemWinIconPixmap=Ico16.CreateIconPixmap(ICO16_STEEM,DispGC);
  StemWinIconMaskPixmap=Ico16.CreateMaskBitmap(ICO16_STEEM);
  SetWindowHints(XD,StemWin,True,NormalState,StemWinIconPixmap,StemWinIconMaskPixmap,SteemWindowGroup,0);

  // Hints ignored by OS so we try another way to get the icon in taskbar
  XImage *xi=XGetImage(XD,StemWinIconPixmap,0,0,16,16,AllPlanes,ZPixmap);
  //ASSERT(xi);
  if(xi)
  {
    ChangeIcon(XD,StemWin,xi);
    XDestroyImage(xi);
  }

  XSelectInput(XD,StemWin,KeyPressMask | KeyReleaseMask |
                    ButtonPressMask | ButtonReleaseMask |
                    ExposureMask | StructureNotifyMask |
                    VisibilityChangeMask | FocusChangeMask);

  SetProp(StemWin,cWinProc,(DWORD_PTR)StemWinProc);

  int x=0;
  RunBut.create(XD,StemWin,x,0,20,20,StemWinButtonNotifyProc,NULL,
                  BT_ICON | BT_UPDOWNNOTIFY,"",101,BkCol);
  RunBut.set_icon(&Ico16,ICO16_RUN);
  hints.add(RunBut.handle,T("Run (Right Click = Slow Motion)"),StemWin);
  x+=23;

  FastBut.create(XD,StemWin,x,0,20,20,StemWinButtonNotifyProc,NULL,
                  BT_ICON | BT_UPDOWNNOTIFY,"",IDC_FASTFORWARD,BkCol);
  FastBut.set_icon(&Ico16,ICO16_FF);
  hints.add(FastBut.handle,T("Fast Forward (Right Click = Searchlight, Double Click = Sticky)"),StemWin);
  x+=23;

  ResetBut.create(XD,StemWin,x,0,20,20,StemWinButtonNotifyProc,NULL,
                    BT_ICON,"",102,BkCol);
  ResetBut.set_icon(&Ico16,ICO16_RESET);

#if defined(SSE_BUILD)
  hints.add(ResetBut.handle,T("Reset (Left Click) - Switch off (Right Click)"),StemWin);
#else
  hints.add(ResetBut.handle,T("Reset (Left Click = Cold, Right Click = Warm)"),StemWin);
#endif

  x+=23;

  SnapShotBut.create(XD,StemWin,x,0,20,20,StemWinButtonNotifyProc,NULL,
                      BT_ICON,"",IDC_SNAPSHOT,BkCol);
  SnapShotBut.set_icon(&Ico16,ICO16_SNAPSHOTS);
  hints.add(SnapShotBut.handle,T("Memory Snapshot Menu"),StemWin);
  x+=23;

  ScreenShotBut.create(XD,StemWin,x,0,20,20,StemWinButtonNotifyProc,NULL,
                      BT_ICON,"",116,BkCol);
  ScreenShotBut.set_icon(&Ico16,ICO16_TAKESCREENSHOTBUT);
  hints.add(ScreenShotBut.handle,T("Take Screenshot"),StemWin);
  x+=23;

  PasteBut.create(XD,StemWin,x,0,20,20,StemWinButtonNotifyProc,NULL,
                      BT_ICON,"",IDC_PASTE,BkCol);
  PasteBut.set_icon(&Ico16,ICO16_PASTE);
  hints.add(PasteBut.handle,T("Paste Text Into ST (Right Click = Options)"),StemWin);
  x+=23;
  //if (Disp.CanGoToFullScreen()) // called before Disp.Init()...
  {
    FullScreenBut.create(XD,StemWin,x,0,20,20,StemWinButtonNotifyProc,NULL,
                        BT_ICON | BT_TOGGLE,"",115,BkCol);
    FullScreenBut.set_icon(&Ico16,ICO16_FULLSCREEN);
    hints.add(FullScreenBut.handle,T("Fullscreen"),StemWin);
  }


#ifdef SSE_UNIX_STATUSBAR
  x+=30;
  StatusBar.create(XD,StemWin,x,0,170,20,WhiteCol,false);
  //StatusBar.set_text("This is the status bar");
#endif


  InfBut.create(XD,StemWin,0,0,20,20,StemWinButtonNotifyProc,NULL,
                    BT_ICON,"",105,BkCol);
  InfBut.set_icon(&Ico16,ICO16_GENERALINFO);
  hints.add(InfBut.handle,T("General Info"),StemWin);

  PatBut.create(XD,StemWin,0,0,20,20,StemWinButtonNotifyProc,NULL,
                    BT_ICON,"",113,BkCol);
  PatBut.set_icon(&Ico16,ICO16_PATCHES);
  hints.add(PatBut.handle,T("Patches"),StemWin);

  CutBut.create(XD,StemWin,0,0,20,20,StemWinButtonNotifyProc,NULL,
                    BT_ICON,"",112,BkCol);
  CutBut.set_icon(&Ico16,ICO16_CUT);
  hints.add(CutBut.handle,T("Shortcuts"),StemWin);

  OptBut.create(XD,StemWin,0,0,20,20,StemWinButtonNotifyProc,NULL,
                    BT_ICON,"",107,BkCol);
  OptBut.set_icon(&Ico16,ICO16_OPTIONS);
  hints.add(OptBut.handle,T("Settings"),StemWin);
  JoyBut.create(XD,StemWin,0,0,20,20,StemWinButtonNotifyProc,NULL,
                    BT_ICON,"",103,BkCol);
  JoyBut.set_icon(&Ico16,ICO16_JOY);
  hints.add(JoyBut.handle,T("Joystick Configuration"),StemWin);

  DiskBut.create(XD,StemWin,0,0,20,20,StemWinButtonNotifyProc,NULL,
                    BT_ICON,"",100,BkCol);
  DiskBut.set_icon(&Ico16,ICO16_DISKMAN);
  hints.add(DiskBut.handle,T("Disk Manager"),StemWin);


#ifndef CYGWIN
  // Create empty 1 bit per pixel bitmap
  char *Dat=new char[16*16/8];
  ZeroMemory(Dat,16*16/8);
  Pixmap EmptyPix=XCreatePixmapFromBitmapData(XD,StemWin,
                    Dat,16,16,0,0,1);
  XColor ccols[2];
  ccols[0].pixel=0;
  ccols[1].pixel=0;
  EmptyCursor=XCreatePixmapCursor(XD,EmptyPix,EmptyPix,
                    &ccols[0],&ccols[1],8,8);
  XFreePixmap(XD,EmptyPix);
  delete[] Dat;
#else
  EmptyCursor=XCreateFontCursor(XD,XC_cross);
#endif
#endif//UNIX
  return TRUE;
}

#endif//#if !defined(SSE_LIBRETRONUKE)


void CheckResetIcon() {
#if !defined(SSE_LIBRETRONUKE)

#ifdef WIN32
#ifndef SSE_LEAN_AND_MEAN
  if(StemWin==NULL)
    return;
#endif
  HWND ResetBut=GetDlgItem(StemWin,IDC_RESET);
#ifndef SSE_LEAN_AND_MEAN
  if(ResetBut==NULL) 
    return;
#endif
  int new_icon=(int)(OptionBox.NeedReset() ? RC_ICO_RESETGLOW:RC_ICO_RESET);
#ifndef SSE_LEAN_AND_MEAN
  Str CurNumText;
  CurNumText.SetLength(20);
  GetWindowText(ResetBut,CurNumText,20);
  if(atoi(CurNumText)!=new_icon) 
#endif
    SetWindowText(ResetBut,Str(new_icon));
#endif//WIN32

#ifdef UNIX
  if (ResetBut.handle==0) return;
  int new_icon=int(OptionBox.NeedReset() ? ICO16_RESETGLOW:ICO16_RESET);
  if (ResetBut.icon_index!=new_icon){
    ResetBut.icon_index=new_icon;
    ResetBut.draw();
  }
#endif
#endif//#if !defined(SSE_LIBRETRONUKE)
}


void CheckResetDisplay(bool AlwaysHide) {

#if defined(SSE_LIBRETRONUKE) || defined(SSE_OSD_NORESETINFO)

  (void)AlwaysHide;

#else

  if(!SSEConfig.IsInit) // hdman constructor calls, pc isn't referenced
    return;

#ifndef ONEGAME
#ifndef SSE_NO_OSD

#ifdef WIN32
#if defined(SSE_420R2B)
  if(LITTLE_PC==SafeLPeek(4) && StemWin && runstate==RUNSTATE_STOPPED && !AlwaysHide)
#else
  if(LITTLE_PC==rom_addr && StemWin && runstate==RUNSTATE_STOPPED && !AlwaysHide)
#endif
  {
    if(hResetInfoWin==NULL) 
    {
      if(FullScreen==0) 
        SetWindowLong(StemWin,GWL_STYLE,GetWindowLong(StemWin,GWL_STYLE)|WS_CLIPCHILDREN);
#if defined(SSE_VID_DD) && !defined(SSE_VID_DD_MISC)
      hResetInfoWin=CreateWindow("Steem Reset Info Window","",WS_CHILD,
        0,0,0,0,HWND(FullScreen?ClipWin:StemWin),(HMENU)9876,hInstance,NULL);
#else
      hResetInfoWin=CreateWindow("Steem Reset Info Window","",WS_CHILD,
        0,0,0,0,HWND(StemWin),(HMENU)9876,hInstance,NULL);
#endif
      SendMessage(hResetInfoWin,WM_USER,1789,0);
      ShowWindow(hResetInfoWin,SW_SHOWNA);
    }
    else
    {
      SendMessage(hResetInfoWin,WM_USER,1789,0);
      InvalidateRect(hResetInfoWin,NULL,FALSE);
    }
  }
  else if(hResetInfoWin)
  {
    HWND Win=hResetInfoWin;
    hResetInfoWin=NULL;
    DestroyWindow(Win);
    SetWindowLong(StemWin,GWL_STYLE,GetWindowLong(StemWin,GWL_STYLE) & ~WS_CLIPCHILDREN);
  }
#endif//WIN32

#ifdef UNIX
#if defined(SSE_420R2B)
  if (LITTLE_PC==SafeLPeek(4) && StemWin){
#else
  if (LITTLE_PC==rom_addr && StemWin){
#endif
		XWindowAttributes wa;
	  XGetWindowAttributes(XD,StemWin,&wa);
  	XClearArea(XD,StemWin,0,MENUHEIGHT,wa.width,wa.height-MENUHEIGHT,True); // Make StemWin redraw
  }
#endif//UNIX

#endif//#ifndef SSE_NO_OSD
#endif//#ifndef ONEGAME

#endif//#if defined(SSE_LIBRETRONUKE) || defined(SSE_OSD_NORESETINFO)
}


#undef LOGSECTION

#if !defined(SSE_LIBRETRONUKE)

void CleanupGUI() {
#ifdef WIN32
  WNDCLASS wc;
#ifdef DEBUG_BUILD
  DWin_edit_is_being_temporarily_defocussed=true;
  if(insp_menu) 
    DestroyMenu(insp_menu);
  if(trace_window_handle)
    DestroyWindow(trace_window_handle);
  trace_window_handle=NULL;
  if(DWin) 
  {
    mr_static_delete_children_of(DWin);
    mr_static_delete_children_of(DWin_timings_scroller.GetControlPage());
  }
  if(DWin) 
    DestroyWindow(DWin);
  for(int n=0;n<MAX_MEMORY_BROWSERS;n++)
  {
    if(m_b[n]!=NULL && m_b[n]->owner!=NULL)
    {
      if(IsWindow(m_b[n]->owner))
      {
        DestroyWindow(m_b[n]->owner);
        n--;
      }
    }
  }
  debug_plugin_free();
#ifdef DEADC0DE
  if(HiddenParent) 
    DestroyWindow(HiddenParent);
#endif
  if(GetClassInfo(hInstance,"Steem Debug Window",&wc))
    UnregisterClass("Steem Debug Window",hInstance);
  if(GetClassInfo(hInstance,"Steem Trace Window",&wc)) 
  {
    UnregisterClass("Steem Mem Browser Window",hInstance);
    UnregisterClass("Steem Trace Window",hInstance);
  }
  if(mem_browser::icons_bmp) 
  {
    DeleteDC(mem_browser::icons_dc);
    DeleteObject(mem_browser::icons_bmp);
  }
#endif
#if defined(SSE_GUI_MENUBAR)
  DestroyMenu(StemWinMenu);
  DestroyMenu(StemWinMenuFile);
  DestroyMenu(StemWinMenuEmu);
  DestroyMenu(StemWinMenuTools);
#endif
  if(StemWin) 
  {
    CheckResetDisplay(true);
    DestroyWindow(StemWin);
#ifndef SSE_LEAN_AND_MEAN
    StemWin=NULL;
#endif
  }
  if(ToolTip) 
    DestroyWindow(ToolTip);
  if(GetClassInfo(hInstance,"Steem Window",&wc)) 
  {
    UnregisterSteemControls();
    UnregisterButtonPicker();
    UnregisterClass("Steem Window",hInstance);
#ifdef DEADC0DE
    UnregisterClass("Steem Fullscreen Clip Window",hInstance);
#endif
  }
  CoUninitialize();
  for(int n=1;n<RC_NUM_ICONS;n++)
  {
    if(hGUIIcon[n])
      DestroyIcon(hGUIIcon[n]);
    if(hGUIIconSmall[n])
      DestroyIcon(hGUIIconSmall[n]);
#if defined(SSE_GUI_DM_BIGICONS)
    if(hGUIIcon128[n])
      DestroyIcon(hGUIIcon128[n]);
#endif
  }
#endif//WIN32

#ifdef UNIX
// NOTE: Everything in this function MUST be checked for existance,
//       it can be called *before* MakeGUI!
  if (XD==NULL) return;

  if (StemWin){
		XAutoRepeatOn(XD);
    hxc::destroy_children_of(StemWin);
    XDestroyWindow(XD,StemWin);StemWin=0;
    hxc::free_res(XD);
  }
  if (DispGC){
    XFreeGC(XD,DispGC);DispGC=0;
  }
  if (EmptyCursor){
    XFreeCursor(XD,EmptyCursor);EmptyCursor=0;
  }
  Ico16.FreeIcons();
  Ico32.FreeIcons();
  Ico64.FreeIcons();
  IcoTOSFlags.FreeIcons();
  if (StemWinIconPixmap){
  	XFreePixmap(XD,StemWinIconPixmap);StemWinIconPixmap=0;
  }
  if (StemWinIconMaskPixmap){
	  XFreePixmap(XD,StemWinIconMaskPixmap);StemWinIconMaskPixmap=0;
	}
  if (colormap){
    XFreeColormap(XD,colormap);
    colormap=0;
  }
  hints.stop();
#endif//UNIX
}

#undef LOGSECTION

#endif//#if !defined(SSE_LIBRETRONUKE)

DWORD GetScreenWidth() {
#ifdef WIN32
  return GuiSM.cx_screen();
#endif

#ifdef UNIX
  static int Wid;
  if (XD){
    return (Wid=XDisplayWidth(XD,XDefaultScreen(XD)));
  }else{
    return Wid;
  }
#endif
}


DWORD GetScreenHeight() {
#ifdef WIN32
  return GuiSM.cy_screen();
#endif
#ifdef UNIX
  static int Height;
  if (XD){
    return (Height=XDisplayHeight(XD,XDefaultScreen(XD)));
  }else{
    return Height;
  }
#endif
}


Str SnapShotGetLastBackupPath() {
  if(!has_extension(LastSnapShot,".sts"))
    return ""; // Just in case folder
  Str Backup=TempPath+SLASH+GetFileNameFromPath(LastSnapShot);
  char *ext=strrchr(Backup,'.');
  ASSERTB(ext);
  *ext='\0';
  Backup+=FILE_BACKUPSNAPSHOT;
  return Backup;
}


void SnapShotGetOptions(EasyStringList *p_sl) {
  p_sl->Sort=eslNoSort;
  p_sl->Add(T("&Load Memory Snapshot"),IDC_LOADSNAPSHOT,0);
  EasyStr NoSaveExplain="";
#ifndef DISABLE_STEMDOS
  if(on_rte!=ON_RTE_NONE)
    NoSaveExplain=T("the ST is in the middle of a disk operation");
#endif
#if USE_PASTI
  if(NoSaveExplain.Empty())
  {
    if(hPasti && pasti_active) {
      NoSaveExplain=T("the ST is in the middle of a disk operation");
      pastiSTATEINFO psi;
      psi.bufSize=0;
      psi.buffer=NULL;
      psi.cycles=ABSOLUTE_SYS_TIME/TICKS8;
      pasti->SaveState(&psi);
      if(psi.bufSize>0) // OK
        NoSaveExplain="";
    }
  }
#endif
#if defined(SSE_EMU_THREAD)
  if(NoSaveExplain.IsEmpty())
  {
    if(OPTION_EMUTHREAD && runstate!=RUNSTATE_STOPPED)
      NoSaveExplain=T("emulation thread is running"); // Steem pretty unstable if we proceed...
  }
#endif
  if(NoSaveExplain.IsEmpty())
  {
    p_sl->Add(T("&Save Memory Snapshot"),IDC_SAVESNAPSHOT,0);
    Str Name=GetFileNameFromPath(LastSnapShot);
    char *ext=strrchr(Name,'.');
    if(ext)
    {
      *ext='\0';
      p_sl->Add("-",0,0);
      p_sl->Add(T("Save Over")+" "+Name,IDC_SNAPSHOTSAVEOVER,0);
      if(Exists(SnapShotGetLastBackupPath()))
        p_sl->Add(T("Undo Save Over")+" "+Name,IDC_UNDOSNAPSHOTSAVEOVER,0);
    }
  }
  else
  {
    p_sl->Add(T("Can't save snapshot because"),0,1); // this takes the place of menu options
    p_sl->Add(NoSaveExplain,0,1);
  }
  if(SSEOptions.ResetBackup && Exists(TempPath+SLASH+FILE_RESETSNAPSHOT))
  {
    p_sl->Add("-",0,0);
    p_sl->Add(T("Undo Last Reset"),IDC_UNDORESET,0);
  }
  if(Exists(TempPath+SLASH+FILE_LOADUNDOSNAPSHOT))
  {
    p_sl->Add("-",0,0);
    p_sl->Add(T("Undo Last Memory Snapshot Load"),IDC_UNDOSNAPSHOTLOAD,0);
  }
  // Add history
  bool AddedLine=false;
  for(int n=0;n<STATE_HISTORY_LEN;n++)
  {
    if(StateHist[n].NotEmpty())
    {
      bool FileExists;
#ifdef WIN32
      FileExists=true;
#if defined(SSE_LONG_PATH)
      EasyStr sTmp=StateHist[n];
      PathPrePend(sTmp,false);
      UINT HostDriveType=GetDriveType(sTmp.Lefts(2)+SLASH);
#else
      UINT HostDriveType=GetDriveType(StateHist[n].Lefts(2)+SLASH);
#endif
      if(HostDriveType==DRIVE_NO_ROOT_DIR)
        FileExists=false;
      else if(HostDriveType!=DRIVE_REMOVABLE && HostDriveType!=DRIVE_CDROM)
        FileExists=Exists(StateHist[n]);
#else
      FileExists=Exists(StateHist[n]);
#endif
      if(FileExists)
      {
        EasyStr Name=GetFileNameFromPath(StateHist[n]);
        char *dot=strrchr(Name,'.');
        if(dot)
          *dot='\0';
        if(!AddedLine)
        {
          p_sl->Add("-",0,0);
          AddedLine=true;
        }
        p_sl->Add(Name,IDC_SNAPSHOT_HISTORY+n,0);
      }
    }
  }
}


// client code must free memory!
char *FSTypes(int Type,...) {
  //char ansi_string[1024];
  //char *FileTypes=ansi_string;
  char *FileTypes=(char*)malloc(1024);
  char *tp=FileTypes;
  ZeroMemory(FileTypes,1024);
  switch(Type) {
  case 2:
    strcpy(tp,T("Disk Images"));tp+=strlen(tp)+1;
    strcpy(tp,"*.st;*.stt;*.msa;*.dim;*.zip;*.stz");tp+=strlen(tp);
#ifdef RARLIB_SUPPORT
    strcpy(tp,";*.rar");tp+=strlen(tp);
#endif
#if defined(SSE_DISK_RAR_SUPPORT)
    if(UNRAR_OK)
    {
      strcpy(tp,";*.rar");
      tp+=strlen(tp);
    }
#endif
#if defined(SSE_ARCHIVEACCESS_SUPPORT) || defined(SSE_DISK_7Z_SUPPORT_UNIX)
#if defined(SSE_ARCHIVEACCESS_SUPPORT)
    if(ARCHIVEACCESS_OK) // or test at startup?
#endif
    {
      strcpy(tp,";*.7z;*.bz2;*.gz;*.tar;*.arj");
      tp+=strlen(tp);
    }
#endif
#if USE_PASTI
    if(hPasti) 
    {
      tp[0]=';';tp++;
      pasti->GetFileExtensions(tp,160,TRUE); // will add "*.st;*.stx"
      tp+=strlen(tp);
    }
#endif
    tp++;
    break;
  case 3:
    strcpy(tp,T("TOS Images"));tp+=strlen(tp)+1;
    strcpy(tp,"*.img;*.rom");tp+=strlen(tp)+1;
    break;
  case 4:
    strcpy(tp,T("ACSI Images"));tp+=strlen(tp)+1;
    strcpy(tp,"*.img;*.raw");tp+=strlen(tp)+1;
    break;
  default:
  {
    va_list vl;  
    va_start(vl,Type);
    char* arg;
    do
    {
      arg=va_arg(vl,char*);
      if(arg!=NULL)
      { // each time 2 strings
        strcpy(tp,arg);
        tp+=strlen(arg)+1;
        arg=va_arg(vl,char*);
        ASSERTB(arg!=NULL);
        strcpy(tp,arg);
        tp+=strlen(arg)+1;
      }
    } while(arg!=NULL);
    va_end(vl);
    break;
  }
  }//sw
  if(Type) 
  {
    strcpy(tp,T("All Files"));tp+=strlen(tp)+1;
    strcpy(tp,"*.*");
  }
  //ASSERT(strlen(FileTypes)<256);
  return FileTypes;
}


bool CleanupTempFiles() {
  bool SteemHasCrashed=false;
  DirSearch ds;
  for(int n=0;n<4;n++)
  {
    char *prefix=extension_list[EXT_MSA];
    switch(n) {
    case 1: prefix="ZIP"; break;
    case 2: prefix="FMT"; break; // also for STW temp
    case 3: prefix="CRA"; break;
    }
    if(ds.Find(TempPath+SLASH+prefix+"*.TMP")) 
    {
      EasyStringList FileESL;
      do {
        FileESL.Add(TempPath+SLASH+ds.Name);
      } while(ds.Next());
#ifndef SSE_LEAN_AND_MEAN
      ds.Close();
#endif
      for(int i=0;i<FileESL.NumStrings;i++) 
        DeleteFile(FileESL[i].String);
      if(n==3) 
        SteemHasCrashed=true;
    }
  }
  return SteemHasCrashed;
}


int PeekEvent() {
#ifdef WIN32
  //ASSERT(!OPTION_EMUTHREAD);
  static MSG mess;
  if(PeekMessage(&mess,NULL,0,0,PM_REMOVE)==0) 
    return PEEKED_NOTHING;
  if(mess.message==WM_QUIT) 
  {
    QuitSteem();
    return PEEKED_QUIT;
  }
  if(HandleMessage(&mess)) 
  {
    TranslateMessage(&mess);
    DispatchMessage(&mess);
  }
  return PEEKED_MESSAGE;
#endif//WIN32

#ifdef UNIX
  if (XD==NULL) return PEEKED_NOTHING;

  hxc::check_timers();
  if (XPending(XD)==0) return PEEKED_NOTHING;
  XEvent Ev;
  XNextEvent(XD,&Ev);
  
//XSync(XD,False);//SS  // ?
  
  return ProcessEvent(&Ev);
#endif
}


void SetStemMouseMode(WORD NewMM) {
#ifdef WIN32
  static POINT OldMousePos={-1,0};
  if(stem_mousemode!=STEM_MOUSEMODE_WINDOW && NewMM==STEM_MOUSEMODE_WINDOW) 
    GetCursorPos(&OldMousePos);
  stem_mousemode=NewMM;
  if(NewMM==STEM_MOUSEMODE_WINDOW) 
  {
    if(no_set_cursor_pos||OPTION_VMMOUSE)
    {
      SetCursor((no_set_cursor_pos)?LoadCursor(NULL,IDC_CROSS):NULL);
      POINT pt;
      GetCursorPos(&pt);
      window_mouse_centre_x=pt.x;
      window_mouse_centre_y=pt.y;
    }
    else 
    {
      SetCursor(NULL);
      RECT rc;
      GetWindowRect(StemWin,&rc);
      window_mouse_centre_x=rc.left+160+GuiSM.cx_frame(); //TODO depends on rez?
      window_mouse_centre_y=rc.top+100+MENUHEIGHT+GuiSM.cy_frame()+GuiSM.cy_caption();
      SetCursorPos(window_mouse_centre_x,window_mouse_centre_y);
    }
#ifndef DEBUG_BUILD
    if(!OPTION_VMMOUSE) // else we don't clip, mouse can exit window
    {
      if(FullScreen)
        ClipCursor(NULL);
      else
      {
        RECT rc;
        POINT pt={0,0};
        GetClientRect(StemWin,&rc);
        rc.right-=6;
        rc.bottom-=6+MENUHEIGHT+GuiSM.m_statusbar_height;
        ClientToScreen(StemWin,&pt);
        OffsetRect(&rc,pt.x+3,pt.y+3+MENUHEIGHT);
        ClipCursor(&rc);
      }
    }
#endif
  }
  else 
  {
    SetCursor(PCArrow);
    if(FullScreen && runstate==RUNSTATE_RUNNING) 
    {
#if defined(SSE_VID_2SCREENS) // watch fullscreen demo, do sthg else on 2nd screen
      if(!OPTION_FAKE_FULLSCREEN)
#endif
        runstate=RUNSTATE_STOPPING;
    }

#ifndef DEBUG_BUILD
    ClipCursor(NULL);
#endif
    if(!OPTION_VMMOUSE && OldMousePos.x>=0 && no_set_cursor_pos==0) 
    {
      SetCursorPos(OldMousePos.x,OldMousePos.y);
      OldMousePos.x=-1;
    }
  }
#endif//WIN32

#ifdef UNIX
//  static POINT OldMousePos={-1,0};

//  if (stem_mousemode!=STEM_MOUSEMODE_WINDOW && NewMM==STEM_MOUSEMODE_WINDOW) GetCursorPos(&OldMousePos);
  stem_mousemode=NewMM;

  if (XD==NULL) return;

  if (NewMM==STEM_MOUSEMODE_WINDOW){
#ifdef CYGWIN
    POINT pt;
    GetCursorPos(&pt);
    window_mouse_centre_x=pt.x;
    window_mouse_centre_y=pt.y;
    XGrabPointer(XD,StemWin,0,0,GrabModeAsync,GrabModeAsync,
                  None,EmptyCursor,CurrentTime);
#else
    if(OPTION_VMMOUSE)
    {
      POINT pt;
      GetCursorPos(&pt);
      window_mouse_centre_x=pt.x;
      window_mouse_centre_y=pt.y;
      XGrabPointer(XD,StemWin,0,0,GrabModeAsync,GrabModeAsync,None,EmptyCursor,
        CurrentTime);
    }
    else
    {
      window_mouse_centre_x=164;
      window_mouse_centre_y=MENUHEIGHT+104;
      XGrabPointer(XD,StemWin,0,0,GrabModeAsync,GrabModeAsync,StemWin,EmptyCursor,
        CurrentTime);
      XWarpPointer(XD,None,StemWin,0,0,0,0,window_mouse_centre_x,
        window_mouse_centre_y);
    }
#endif
  }else{
    if (FullScreen && runstate==RUNSTATE_RUNNING) runstate=RUNSTATE_STOPPING;
    XUngrabPointer(XD,CurrentTime);
/*
    if (OldMousePos.x>=0){
      SetCursorPos(OldMousePos.x,OldMousePos.y);
      OldMousePos.x=-1;
    }
*/
  }
#endif
  mouse_vbl_delta_x=mouse_vbl_delta_y=0;
  mouse_vbl_delta=false;
  REFRESH_STATUS_BAR;
}


void ShowAllDialogs(bool Show) {
#ifdef WIN32
  if(FullScreen==0) 
    return;
  static bool DiskManWasMaximized=false;
  int PosChange=(Show)?-3000:3000;
  if(DiskMan.Handle) 
  {
    if(DiskMan.FSMaximized && !Show)
      DiskManWasMaximized=true;
    if(DiskManWasMaximized && Show) 
    {
      SetWindowPos(DiskMan.Handle,NULL,-GuiSM.cx_frame(),MENUHEIGHT,
        Disp.SurfaceWidth+GuiSM.cx_frame()*2,Disp.SurfaceHeight+GuiSM.cy_frame()-MENUHEIGHT,
        SWP_NOZORDER|SWP_NOACTIVATE);
      DiskManWasMaximized=false;
    }
    else 
    {
      DiskMan.FSLeft+=PosChange;
      SetWindowPos(DiskMan.Handle,NULL,DiskMan.FSLeft,DiskMan.FSTop,0,0,
        SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
    }
  }
  for(int n=0;n<nStemDialogs;n++) 
  {
    if(DialogList[n]!=&DiskMan) 
    {
      if(DialogList[n]->Handle) 
      {
        DialogList[n]->FSLeft+=PosChange;
        SetWindowPos(DialogList[n]->Handle,NULL,DialogList[n]->FSLeft,
          DialogList[n]->FSTop,0,0,SWP_NOZORDER|SWP_NOSIZE|SWP_NOACTIVATE);
      }
    }
  }
#endif//WIN32
}


void HandleKeyPress(UINT VKCode,DWORD Up,int Extended) {
#if defined(SSE_NO_OLD_IKBD)
  if(disable_input_vbl_count)
#else
  if(!OPTION_C1 && ikbd_keys_disabled() || disable_input_vbl_count)
#endif
    return; //in duration mode
  BYTE STCode=0;
  BYTE VKCodeLow=LOBYTE(VKCode);
  bool DidShiftSwitching=false;

#ifdef WIN32
  if(macro_play_has_keys) 
    return;
  if((Extended&3)==1) 
  {
    switch(VKCodeLow) {
    case VK_RETURN: 
      STCode=0x72; //STKEY_PAD_ENTER;
      break;
    case VK_DIVIDE:
      STCode=0x65; //STKEY_PAD_DIVIDE;
      break;
    }
  }
#endif

  int ModifierRestoreArray[3];
  if(bEnableShiftSwitching&&shift_key_table[0]&&(Extended & NO_SHIFT_SWITCH)==0)
  {
    if(STCode==0) // can be different in Win32
    {
      HandleShiftSwitching(VKCode,Up,STCode,ModifierRestoreArray);
      if(STCode) 
        DidShiftSwitching=true;
    }
  }
  if(STCode==0)
    STCode=key_table[VKCodeLow];

  if(STCode)
  {
    // If we're sending key combinations, we don't want 6301 to see the key
    if(!DidShiftSwitching||!OPTION_C1)
      ST_Key_Down[STCode]=!Up; // this is used by ikbd.cpp & ireg.c

#ifdef WIN32
#if defined(SSE_ENABLE_TRACE_LOG)
#undef LOGSECTION
#define LOGSECTION LOGSECTION_IKBD
    TRACE_LOG("%d %d Key PC $%X ST $%X ",FRAME,scan_y,VKCode,STCode);
    TRACE_LOG((Up)?"-\n":"+\n");
#undef LOGSECTION
#endif
#endif//WIN32

    if(Up) // The break code for each key is obtained by ORing 0x80 with the make code:
    {
      STCode|=MSB_B; // MSB_B = $80
#if defined(SSE_STATS)
      Stats.nKeyIn++;
#endif
    }
    if(OPTION_C1&&!DidShiftSwitching)
    {
      //We don't write in a buffer, 6301 emu will do it after having scanned
      //ST_Key_Down.
      if(macro_record)
        macro_record_key(STCode);
      //if(DidShiftSwitching)
      //  keyboard_buffer_write(STCode); //must send ourselves to ACIA
    }
    else
      keyboard_buffer_write_n_record(STCode);

#ifndef DISABLE_STEMDOS //control-C
    if(VKCode==
#ifdef WIN32
      'C'
#endif
#ifdef UNIX
      XK_c
#endif
      && ST_Key_Down[key_table[VK_CONTROL]])
      Stemdos.CtrlC();
#endif
  }//if(STCode)
  if(DidShiftSwitching)
    ShiftSwitchRestoreModifiers(ModifierRestoreArray);
}


#ifdef WIN32

void EnableWindow2(HWND Win,bool Enable,HWND NoDisable) {
  if(Win!=NoDisable) 
    SetWindowLong(Win,GWL_STYLE,(GetWindowLong(Win,GWL_STYLE) 
      &( ((Enable) ? ~WS_DISABLED : 0xffffffff) 
      | ((!Enable) ? WS_DISABLED : 0)) ) );
}


void EnableAllWindows(bool Enable,HWND NoDisable) {
  DisableFocusWin=(Enable)?NULL:NoDisable;
#ifdef DEBUG_BUILD
  EnableWindow2(DWin,Enable,NoDisable);
  if(trace_window_handle)
    EnableWindow2(trace_window_handle,Enable,NoDisable);
#endif
  /*DEBUG_ONLY( EnableWindow2(DWin,Enable,NoDisable) );
  DEBUG_ONLY(if(trace_window_handle) EnableWindow2(trace_window_handle,Enable,NoDisable));*/
  EnableWindow2(StemWin,Enable,NoDisable);
  if(DiskMan.Handle) 
  {
    if(HardDiskMan.Handle)
      EnableWindow2(HardDiskMan.Handle,Enable,NoDisable);
    else if(DiskMan.VisibleDiag()==NULL)
      EnableWindow2(DiskMan.Handle,Enable,NoDisable);
    else
      EnableWindow2(DiskMan.VisibleDiag(),Enable,NoDisable);
  }
  for(int n=0;n<nStemDialogs;n++) 
  {
    if(DialogList[n]!=&DiskMan) 
      if(DialogList[n]->Handle) 
        EnableWindow2(DialogList[n]->Handle,Enable,NoDisable);
  }
}


#define WH_KEYBOARD_LL 13
#if (_WIN32_WINNT < 0x0400)
#define LLKHF_ALTDOWN 0x00000020
#endif
#if !defined(MINGW_BUILD) && (_WIN32_WINNT < 0x0400) // well well
typedef struct{
  DWORD vkCode;
  DWORD scanCode;
  DWORD flags;
  DWORD time;
  DWORD dwExtraInfo;
}KBDLLHOOKSTRUCT, *LPKBDLLHOOKSTRUCT;
#endif

LRESULT CALLBACK NTKeyboardProc(INT nCode,WPARAM wParam,LPARAM lParam) {
  KBDLLHOOKSTRUCT *pkbhs=LPKBDLLHOOKSTRUCT(lParam);
  if(nCode==HC_ACTION)
  {
    bool ControlDown=(GetAsyncKeyState(VK_CONTROL)<0),AltDown=(pkbhs->flags&LLKHF_ALTDOWN)!=0;
    bool ShiftDown=(GetAsyncKeyState(VK_SHIFT)<0);
    if(pkbhs->vkCode==VK_TAB && AltDown)
      return 1;
    if(pkbhs->vkCode==VK_ESCAPE&&(AltDown||ShiftDown||ControlDown))
      return 1;
    if(pkbhs->vkCode==VK_DELETE && AltDown && ControlDown)
      return 1;
#ifdef ONEGAME
    if(pkbhs->vkCode==VK_LWIN||pkbhs->vkCode==VK_RWIN) return 1;
#endif
  }
  return CallNextHookEx(hNTTaskSwitchHook,nCode,wParam,lParam);
}


void DisableTaskSwitch() {
  if(TaskSwitchDisabled)
    return;
  if(WinNT) 
  {
    hNTTaskSwitchHook=SetWindowsHookEx(WH_KEYBOARD_LL,HOOKPROC(NTKeyboardProc),
                                       NULL,GetCurrentThreadId());
    if(hNTTaskSwitchHook==NULL) 
    {
      int Base=1400;
      RegisterHotKey(StemWin,Base++,MOD_ALT,VK_TAB);
      RegisterHotKey(StemWin,Base++,MOD_ALT|MOD_SHIFT,VK_TAB);
      RegisterHotKey(StemWin,Base++,MOD_ALT,VK_ESCAPE);
      RegisterHotKey(StemWin,Base++,MOD_ALT|MOD_SHIFT,VK_ESCAPE);
      RegisterHotKey(StemWin,Base++,MOD_CONTROL,VK_ESCAPE);
      RegisterHotKey(StemWin,Base++,MOD_CONTROL|MOD_ALT,VK_ESCAPE);
      RegisterHotKey(StemWin,Base++,MOD_CONTROL|MOD_SHIFT,VK_ESCAPE);
      RegisterHotKey(StemWin,Base++,MOD_CONTROL|MOD_ALT|MOD_SHIFT,VK_ESCAPE);
      RegisterHotKey(StemWin,Base++,MOD_CONTROL|MOD_ALT,VK_DELETE);
      RegisterHotKey(StemWin,Base++,MOD_CONTROL|MOD_ALT|MOD_SHIFT,VK_DELETE);
      RegisterHotKey(StemWin,Base++,MOD_SHIFT,VK_ESCAPE);
#ifdef ONEGAME
      RegisterHotKey(StemWin,Base++,0,VK_LWIN);
      RegisterHotKey(StemWin,Base++,0,VK_RWIN);
#endif
    }
  }
  else 
  {
    UINT PrevSS;
    SystemParametersInfo(SPI_SETSCREENSAVERRUNNING,TRUE,&PrevSS,0);
  }
  TaskSwitchDisabled=true;
}


void EnableTaskSwitch() {
  if(!TaskSwitchDisabled)
    return;
  if(WinNT) 
  {
    if(hNTTaskSwitchHook==NULL)
      for(int n=1400;n<1411 ONEGAME_ONLY(+2);n++) 
        UnregisterHotKey(StemWin,n);
  }
  else 
  {
    UINT PrevSS;
    SystemParametersInfo(SPI_SETSCREENSAVERRUNNING,FALSE,&PrevSS,0);
  }
  if(hNTTaskSwitchHook) 
  {
    UnhookWindowsHookEx(hNTTaskSwitchHook);
    hNTTaskSwitchHook=NULL;
  }
  TaskSwitchDisabled=false;
}


void UpdatePasteButton() {
#ifdef RELEASE_BUILD
  // Only have automatic updating of paste button if final build
  if(PasteText.Empty())
    EnableWindow(GetDlgItem(StemWin,IDC_PASTE),IsClipboardFormatAvailable(CF_TEXT));
#else
  EnableWindow(GetDlgItem(StemWin,IDC_PASTE),true);
#endif
}


#if !defined(RELEASE_BUILD) && defined(DEBUG_BUILD)
bool HWNDNotValid(HWND Win,char *File,int Line) {
  bool Err=0;
  if(Win==NULL)
    Err=true;
  else if(IsWindow(Win)==0)
    Err=true;
  if(Err)
  {
    Alert(Str("WINDOWS: Arrghh, using ")+long(Win)+" as HWND in file "+File+" at line "+Line,"Window Handle Error",MB_ICONEXCLAMATION);
    return true;
  }
  return 0;
}

LRESULT SendMessage_checkforbugs(HWND Win,UINT Mess,WPARAM wPar,
                                LPARAM lPar,char *File,int Line) {
  if(HWNDNotValid(Win,File,Line)) 
    return 0;
  return SendMessageA(Win,Mess,wPar,lPar);
}

BOOL PostMessage_checkforbugs(HWND Win,UINT Mess,WPARAM wPar,
                              LPARAM lPar,char *File,int Line) {
  if(HWNDNotValid(Win,File,Line)) 
    return 0;
  return PostMessageA(Win,Mess,wPar,lPar);
}

#endif

#endif//WIN32


#if defined(SSE_VID_2SCREENS) // otherwise it's in mymisc

void CentreWindow(HWND Win,bool Redraw) {
  RECT rc;
  GetWindowRect(Win,&rc);
  int W=rc.right-rc.left,H=rc.bottom-rc.top;
  Disp.CheckCurrentMonitorConfig(Win); // Update monitor rectangle
  MoveWindow(Win,Disp.rcMonitor.left+(Disp.rcMonitor.right-Disp.rcMonitor.left-W)/2,
    Disp.rcMonitor.top+(Disp.rcMonitor.bottom-Disp.rcMonitor.top-H)/2,W,H,Redraw);
  if(Win!=StemWin)
    Disp.CheckCurrentMonitorConfig(); // Update monitor rectangle
}

#endif

int Alert(char *Mess,char *Title,UINT Flags) {
#ifdef WIN32
  HWND Win=GetActiveWindow();
#endif
#if !defined(SSE_LIBRETRONUKE)
  Disp.FlipToDialogsScreen();
#endif
  int Ret=MessageBox((WINDOWTYPE)(FullScreen?StemWin:0),Mess,Title,
    Flags | MB_TASKMODAL | MB_TOPMOST | MB_SETFOREGROUND);
#ifdef WIN32
  SetActiveWindow(Win);
#endif
  return Ret;
}


// Shift and alt switching, use 4 tables of 256 WORDs:
//  table 0 = no shift, no alt
//  table 1 = shift, no alt
//  table 2 = no shift, alt
//  table 3 = shift, alt
// Look up our key (VKCode) in the table, if it isn't 0 then shift/alt switching
// must be performed. The LOBYTE contains the ST key code, the high byte has
// bit 0 set if shift should be down when the key is sent and bit 1 for alt to
// be down. Before the key is pressed we must change the ST shift and alt states
// to what we need by sending releasing/press IKBD messages for those keys.
// After the key has been sent we then send more messages to restore them to
// their former glory. Surprisingly it works great, except if you try to do any
// key repeat.

void ShiftSwitchChangeModifiers(bool ShiftShouldBePressed,
       bool AltShouldBePressed,int ModifierRestoreArray[3]) {
  // Get current states
  BYTE STLShiftDown=(ST_Key_Down[key_table[VK_LSHIFT]]);
  BYTE STRShiftDown=(ST_Key_Down[key_table[VK_RSHIFT]]);
  int STAltDown=(ST_Key_Down[key_table[VK_MENU]] ? BIT_1 : 0);
  if((STLShiftDown||STRShiftDown) && !ShiftShouldBePressed)
  {
    // Send Shift Up Messages
    if(STLShiftDown) 
    {
      keyboard_buffer_write_n_record(key_table[VK_LSHIFT]|MSB_B);
      ModifierRestoreArray[0]=1; //Lshift down
    }
    if(STRShiftDown)
    {
      keyboard_buffer_write_n_record(key_table[VK_RSHIFT]|MSB_B);
      ModifierRestoreArray[1]=1; //Rshift down
    }
  }
  else if((STLShiftDown||STRShiftDown)==0 && ShiftShouldBePressed) 
  {
    // Send Shift Down Message
    keyboard_buffer_write_n_record(key_table[VK_LSHIFT]);
    ModifierRestoreArray[0]=2; //Lshift up
  }
  if(STAltDown && !AltShouldBePressed) 
  {
    // Send Alt Up Messages
    keyboard_buffer_write_n_record(key_table[VK_MENU]|MSB_B);
    ModifierRestoreArray[2]=1; //Alt down
  }
  else if(STAltDown==0 && AltShouldBePressed) 
  {
    // Send Alt Down Message
    keyboard_buffer_write_n_record(key_table[VK_MENU]);
    ModifierRestoreArray[2]=2; //Alt up
  }
}


void ShiftSwitchRestoreModifiers(int ModifierRestoreArray[3]) {
  if(ModifierRestoreArray[0]==1) 
    keyboard_buffer_write_n_record(key_table[VK_LSHIFT]);
  if(ModifierRestoreArray[0]==2) 
    keyboard_buffer_write_n_record(key_table[VK_LSHIFT]|MSB_B);
  if(ModifierRestoreArray[1]==1) 
    keyboard_buffer_write_n_record(key_table[VK_RSHIFT]);
  if(ModifierRestoreArray[2]==1) 
    keyboard_buffer_write_n_record(key_table[VK_MENU]);
  if(ModifierRestoreArray[2]==2) 
    keyboard_buffer_write_n_record(key_table[VK_MENU]|MSB_B);
}


void HandleShiftSwitching(UINT VKCode,DWORD Up,BYTE &STCode,int ModifierRestoreArray[3]) {
  //ASSERT(VKCode==VK_MENU);
  // These are set to tell the HandleKeyPress routine what to do after it has
  // sent the key.
  ModifierRestoreArray[0]=0;  // LShift
  ModifierRestoreArray[1]=0;  // RShift
  ModifierRestoreArray[2]=0;  // Alt (only one on ST)
  // Don't need to do anything when you release the key
  if(shift_key_table[0]==NULL) 
    return;
  BYTE VKCodeLow=LOBYTE(VKCode);
  // Get ST code and required modifier states
  BYTE Shift,Alt;
  if(Up==0) 
  { // Pressing key
    // Get current state of modifiers
    Shift=((ST_Key_Down[key_table[VK_LSHIFT]]||ST_Key_Down[key_table[VK_RSHIFT]])?BIT_0:0);
    Alt=(ST_Key_Down[key_table[VK_MENU]] ? BIT_1 : 0);
  }
  else 
  { // Releasing key
   // Get state of modifiers when key was pressed
    Shift=(KeyDownModifierState[VKCodeLow]&BIT_0);
    Alt=(KeyDownModifierState[VKCodeLow]&BIT_1);
  }
  WORD KeyEntry=shift_key_table[Shift | Alt][VKCodeLow];
  STCode=(BYTE)KeyEntry;
  KeyDownModifierState[VKCodeLow]=Shift|Alt;
  if(STCode && Up==0) 
  {
    BYTE KeyEntryHi=HIBYTE(KeyEntry);
    bool ShiftShouldBePressed=((KeyEntryHi&BIT_0)!=0);
    bool AltShouldBePressed=((KeyEntryHi&BIT_1)!=0);
    ShiftSwitchChangeModifiers(ShiftShouldBePressed,AltShouldBePressed,ModifierRestoreArray);
  }
}


void PasteIntoSTAction(int Action) {
  if(Action==STPASTE_STOP||Action==STPASTE_TOGGLE) 
  {
    if(PasteText.NotEmpty()) 
    {
      PasteText="";
      PasteVBLCount=0;
#ifdef WIN32
      SendDlgItemMessage(StemWin,IDC_PASTE,BM_SETCHECK,0,0);
#endif
#ifdef UNIX
      PasteBut.set_check(0);
#endif
      return;
    }
    else if(Action==STPASTE_STOP) 
      return;
  }
#ifdef WIN32
  if(IsClipboardFormatAvailable(CF_TEXT)==FALSE) 
    return;
  if(OpenClipboard(StemWin)==FALSE) 
    return;
  HGLOBAL hGbl=GetClipboardData(CF_TEXT);
  if(hGbl) 
  {
    PasteText=(char*)GlobalLock(hGbl);
    PasteVBLCount=PasteSpeed;
    SendDlgItemMessage(StemWin,IDC_PASTE,BM_SETCHECK,1,0);
    GlobalUnlock(hGbl);
  }
  CloseClipboard();
#endif//WIN32
#ifdef UNIX
  Window SelectionOwner=XGetSelectionOwner(XD,XA_PRIMARY);
  if(SelectionOwner!=None) {
    XEvent SendEv;
    SendEv.type=SelectionRequest;
    SendEv.xselectionrequest.requestor=StemWin;
    SendEv.xselectionrequest.owner=SelectionOwner;
    SendEv.xselectionrequest.selection=XA_PRIMARY;
    SendEv.xselectionrequest.target=XA_STRING;
    SendEv.xselectionrequest.property=XA_CUT_BUFFER0;
    SendEv.xselectionrequest.time=CurrentTime;
    XSendEvent(XD,SelectionOwner,0,0,&SendEv);
    // PasteText,PasteVBLCount and PasteBut are set up
    // in SelectionNotify event handler
	}
#endif//UNIX
}


void PasteVBL() {
  if(PasteText.NotEmpty()) 
  {
    if((--PasteVBLCount)<=0) 
    {
      // Convert to ST Ascii
      BYTE c=(BYTE)PasteText[0];
      if(c>127) 
      {
        c=STCharToPCChar[c-128];
        if(c)
          PasteText[0]=(char)c;
      }
      // Go through every character TOS can produce to find it
      switch(c) {
      case '\r': break; // Only need line feeds
      case '\n':
        keyboard_buffer_write_n_record(0x1c);
        keyboard_buffer_write_n_record(0x1c|BIT_7);
        break;
      case '\t':
        keyboard_buffer_write_n_record(0x0f);
        keyboard_buffer_write_n_record(0x0f|BIT_7);
        break;
      case ' ':
        keyboard_buffer_write_n_record(0x39);
        keyboard_buffer_write_n_record(0x39|BIT_7);
        break;
      default:
        DynamicArray<DWORD> Chars;
        GetAvailablePressChars(&Chars);
        for(int n=0;n<Chars.NumItems;n++) 
        {
          if(HIWORD(Chars[n])==c) 
          {
            // Now fix shift/alt and press the key
            int ModifierRestoreArray[3]={0,0,0};
            BYTE STCode=LOBYTE(LOWORD(Chars[n]));
            BYTE Modifiers=HIBYTE(LOWORD(Chars[n]));
            ShiftSwitchChangeModifiers((Modifiers & BIT_0)!=0,
              (Modifiers & BIT_1)!=0,ModifierRestoreArray);
            keyboard_buffer_write_n_record(STCode);
            keyboard_buffer_write_n_record(STCode|BIT_7);
            ShiftSwitchRestoreModifiers(ModifierRestoreArray);
            break;
          }
        }
      }
      PasteText.Delete(0,1);
      if(PasteText.NotEmpty())
        PasteVBLCount=PasteSpeed;
      else 
      {
        PasteText=""; // Release some memory
#ifdef WIN32
        SendDlgItemMessage(StemWin,IDC_PASTE,BM_SETCHECK,0,0);
#endif
#ifdef UNIX
        PasteBut.set_check(0);
#endif
      }
    }
  }
}


#if defined(SSE_GUI_DM_PICS)
/*  We use GDI+ to display JPG files in the GUI (not for screenshots, which rely on D3D9).
    This is adapted from a Microsoft sample.
*/

#pragma comment(lib,"windowscodecs.lib") // needed library
#pragma comment(lib,"GdiPlus.lib") // needed library

#define LOGSECTION LOGSECTION_VIDEO_RENDERING

TGdiPlus MyGdiPlus;

TGdiPlus::TGdiPlus() {
  pGdiPlusBitmap=NULL;
  pbBuffer=NULL;
}


//TGdiPlus::~TGdiPlus() {
  //DeleteBufferAndBmp();
//}


HRESULT TGdiPlus::DisplayPicFile(char* path,HDC hdcDest,RECT* pRect) {

  if(path==NULL)
    path=CurrentPicPath.Text;

  HDC hdcSource = CreateCompatibleDC(hdcDest);
  WCHAR buffer[SSE_MAX_PATH];
  MultiByteToWideChar(CP_ACP,0,path,-1,buffer,SSE_MAX_PATH);
  // Create a decoder
  IWICBitmapDecoder* pDecoder = NULL;
  HRESULT hr = MyGdiPlus.pIWICFactory->CreateDecoderFromFilename(
    buffer,                      // Image to be decoded
    NULL,                            // Do not prefer a particular vendor
    GENERIC_READ,                    // Desired read access to the file
    WICDecodeMetadataCacheOnDemand,  // Cache metadata when needed
    &pDecoder                        // Pointer to the decoder
  );

  // Retrieve the first frame of the image from the decoder
  IWICBitmapFrameDecode* pFrame = NULL;
  if(SUCCEEDED(hr))
  {
    hr = pDecoder->GetFrame(0,&pFrame);
  }

  // Retrieve IWICBitmapSource from the frame
  // pOriginalBitmapSource contains the original bitmap and acts as an intermediate
  if(SUCCEEDED(hr))
  {
    SafeRelease(MyGdiPlus.pOriginalBitmapSource);
    hr = pFrame->QueryInterface(
      IID_IWICBitmapSource,
      reinterpret_cast<void**>(&MyGdiPlus.pOriginalBitmapSource));
  }

  // Step 3: Scale the original IWICBitmapSource to the client rect size
  // and convert the pixel format
  IWICBitmapSource* pToRenderBitmapSource = NULL;
  if(SUCCEEDED(hr))
  {
    //hr = ConvertBitmapSource(hWnd,&pToRenderBitmapSource);
    hr = MyGdiPlus.ConvertBitmapSource(pRect,&pToRenderBitmapSource);
  }

  // Step 4: Create a DIB from the converted IWICBitmapSource
  if(SUCCEEDED(hr))
  {
    hr = MyGdiPlus.CreateDIBSectionFromBitmapSource(pToRenderBitmapSource);
  }

  SafeRelease(pToRenderBitmapSource);
  SafeRelease(pDecoder);
  SafeRelease(pFrame);

  Gdiplus::Graphics graphics(hdcDest);

  if(SUCCEEDED(hr) && MyGdiPlus.pGdiPlusBitmap)
  {
    // Create rendering area
    Gdiplus::SizeF sizef = Gdiplus::SizeF(
      (Gdiplus::REAL)MyGdiPlus.pGdiPlusBitmap->GetWidth(),
      (Gdiplus::REAL)MyGdiPlus.pGdiPlusBitmap->GetHeight());

    Gdiplus::RectF rcClient 
      = Gdiplus::RectF(Gdiplus::PointF((Gdiplus::REAL)pRect->left,(Gdiplus::REAL)pRect->top),sizef);
    // Render the Bitmap
    graphics.DrawImage(MyGdiPlus.pGdiPlusBitmap,rcClient);
  }
  DeleteDC(hdcSource);

  DeleteBufferAndBmp(); // don't let it hang until destructor (crash on exit)

  TRACE_LOG("GDI+ %s at %d,%d ERR %d\n",path,pRect->left,pRect->top,hr);
  return hr;
}


/*******************************************************************************
*  Scales original IWICBitmapSource to the client rect size  and convert       *
*  the pixel format. Store the converted bitmap as *ppToRenderBitmapSource     *
********************************************************************************/
HRESULT TGdiPlus::ConvertBitmapSource(RECT* pRect,IWICBitmapSource** ppToRenderBitmapSource) {
    HRESULT hr = S_OK;

    *ppToRenderBitmapSource = NULL;

    // Get the client Rect
    RECT rcClient=*pRect; // as parameter in our case it's viewlist item
    //hr = GetClientRect(hWnd, &rcClient) ? S_OK: E_FAIL;

    if (SUCCEEDED(hr))
    {
        // Create a BitmapScaler
        IWICBitmapScaler *pScaler = NULL;

        hr = pIWICFactory->CreateBitmapScaler(&pScaler);

        // Initialize the bitmap scaler from the original bitmap map bits
        if (SUCCEEDED(hr))
        {
            //SSE addition: correct aspect ratio
            UINT w,h;
            pOriginalBitmapSource->GetSize(&w,&h);
            double src_ar=h/w;
            LONG dst_w=rcClient.right - rcClient.left;
            LONG dst_h=rcClient.bottom - rcClient.top;
            double dst_ar=dst_h/dst_w;
            ASSERTB(src_ar>=dst_ar);
            if(src_ar>dst_ar) // dst too squat
            {
              LONG new_dst_w=(dst_h*w)/h;
              LONG diff=(dst_w-new_dst_w)/2;
              rcClient.right-=diff;
              rcClient.left+=diff;
            }
            else // just in case
            {
              LONG new_dst_h=(dst_w*h)/w;
              LONG diff=(dst_h-new_dst_h)/2;
              rcClient.bottom-=diff;
              rcClient.top+=diff;
            }

            hr = pScaler->Initialize(
                pOriginalBitmapSource, 
                rcClient.right - rcClient.left, 
                rcClient.bottom - rcClient.top, 
                WICBitmapInterpolationModeFant);
        }

        // Format convert the bitmap into 32bppBGR, a convenient 
        // pixel format for GDI+ rendering 
        if (SUCCEEDED(hr))
        {
            IWICFormatConverter *pConverter = NULL;

            hr = pIWICFactory->CreateFormatConverter(&pConverter);

            // Format convert to 32bppBGR 
            if (SUCCEEDED(hr))
            {
                hr = pConverter->Initialize(
                    pScaler,                         // Input bitmap to convert
                    GUID_WICPixelFormat32bppBGR,     // Destination pixel format
                    WICBitmapDitherTypeNone,         // Specified dither patterm
                    NULL,                            // Specify a particular palette 
                    0.f,                             // Alpha threshold
                    WICBitmapPaletteTypeCustom       // Palette translation type
                    );

                // Store the converted bitmap as ppToRenderBitmapSource 
                if (SUCCEEDED(hr))
                {
                    hr = pConverter->QueryInterface(
                        IID_PPV_ARGS(ppToRenderBitmapSource));
                }
            }

            SafeRelease(pConverter);
        }

        SafeRelease(pScaler);
    }

    return hr;
}


/******************************************************************
*  Creates a DIB Section from the converted IWICBitmapSource      *
******************************************************************/
HRESULT TGdiPlus::CreateDIBSectionFromBitmapSource(IWICBitmapSource* pToRenderBitmapSource) {
    HRESULT hr = S_OK;

    UINT width = 0;
    UINT height = 0;

    // Check BitmapSource format
    WICPixelFormatGUID pixelFormat;
    hr = pToRenderBitmapSource->GetPixelFormat(&pixelFormat);

    if (SUCCEEDED(hr))
    {
        hr = (pixelFormat == GUID_WICPixelFormat32bppBGR) ? S_OK : E_FAIL;
    }

    if (SUCCEEDED(hr))
    {
        hr = pToRenderBitmapSource->GetSize(&width, &height); 
    }

    UINT cbStride = 0;
    if (SUCCEEDED(hr))
    {
        // Size of a scan line represented in bytes: 4 bytes each pixel
        hr = UIntMult(width, sizeof(Gdiplus::ARGB), &cbStride);
    }

    UINT cbBufferSize = 0;
    if (SUCCEEDED(hr))
    {
        // Size of the image, represented in bytes
        hr = UIntMult(cbStride, height, &cbBufferSize);
    }

    if (SUCCEEDED(hr))
    {
        // Make sure to free previously allocated buffer and bitmap
        DeleteBufferAndBmp();

        pbBuffer  = new BYTE[cbBufferSize];

        hr = (pbBuffer) ? S_OK : E_FAIL;

        if (SUCCEEDED(hr))
        {

            WICRect rc = { 0, 0, (INT)width, (INT)height };

            // Extract the image into the GDI+ Bitmap
            hr = pToRenderBitmapSource->CopyPixels(
                &rc,
                cbStride,
                cbBufferSize,
                pbBuffer
                );

            if (SUCCEEDED(hr))
            {
                pGdiPlusBitmap = new Gdiplus::Bitmap(
                    width,
                    height,
                    cbStride,
                    PixelFormat32bppRGB,
                    pbBuffer
                    );
                 
                hr = pGdiPlusBitmap ? S_OK : E_FAIL;

                if (SUCCEEDED(hr))
                { 
                    if (pGdiPlusBitmap->GetLastStatus() != Gdiplus::Ok)
                    {
                        hr = E_FAIL;
                    }
                }
            }

            if (FAILED(hr))
            {
                DeleteBufferAndBmp();
            }
        }
    }

    return hr;
}


void TGdiPlus::DeleteBufferAndBmp() {
  if(pGdiPlusBitmap)
    delete pGdiPlusBitmap;
  pGdiPlusBitmap = NULL;

  if(pbBuffer)
    delete[] pbBuffer;
  pbBuffer = NULL;
}


// look for path +".jpg", "jpeg", ".png" or ".bmp"
bool TGdiPlus::GetPic(EasyStr path) {
  CurrentPicPath=path;
  CurrentPicPath+=".jpg";
  if(Exists(CurrentPicPath))
    return true;

  CurrentPicPath=path;
  CurrentPicPath+=".jpeg";
  if(Exists(CurrentPicPath))
    return true;

  CurrentPicPath=path;
  CurrentPicPath+=".png";
  if(Exists(CurrentPicPath))
    return true;

  CurrentPicPath=path;
  CurrentPicPath+=".bmp";
  if(Exists(CurrentPicPath))
    return true;
  
  RemoveFileNameFromPath(CurrentPicPath,false);
  CurrentPicPath+="diskimage.png"; // one pic for this folder's disk images
  if(Exists(CurrentPicPath))
    return true;
  
  return false;
}

#undef LOGSECTION

#endif//#if defined(SSE_GUI_DM_PICS)


#ifdef UNIX

//---------------------------------------------------------------------------
int HandleXError(Display *XD,XErrorEvent *pXErr)
{
  XError=*pXErr;
  char ErrText[300]={0};
  XGetErrorText(XD,XError.type,ErrText,299);
  if (ErrText[0])
  {
  }
#if defined(SSE_UNIX_TRACE)
  if(ErrText[0])
  { 
    TRACE(ErrText); // newline?
  }
#endif
  return 0;
}
//---------------------------------------------------------------------------
void InitColoursAndIcons()
{
  int Scr=XDefaultScreen(XD);
  if (XDefaultDepth(XD,Scr)==8){ // Oh no! 8-bit!
    XVisualInfo vith;
    //I want to set the member called "class".  But gcc
    //doesn't like members with reserved words for names!
    *((&(vith.depth))+1)=PseudoColor;
    int how_many=0;
    XVisualInfo *vi=XGetVisualInfo(XD,VisualClassMask,&vith,&how_many);
    if (how_many){
      colormap=XCreateColormap(XD,XDefaultRootWindow(XD),vi->visual,AllocAll);
      XFree(vi);

      for (int n=0;n<257;n++){
        logpal[n]=0;
        new_pal[n].pixel=n;
        if (n<256){
        	XQueryColor(XD,XDefaultColormap(XD,XDefaultScreen(XD)),new_pal+n);
  	      XStoreColor(XD,colormap,new_pal+n);
  	    }
        new_pal[n].flags=DoRed | DoGreen | DoBlue;
      }
  		
      for(int n=0;n<8;n++){
      	logpal[n]=0xffffffff;
      }
  		XColor c;
  		for (int n=0;n<18;n++){
  			int nn=standard_palette[n][0];
  			int col=standard_palette[n][1];
  			logpal[nn]=0xffffffff;
    	  c.flags=DoRed | DoGreen | DoBlue;
  			c.pixel=nn;
  			c.red=(col & 0xff0000) >> 8;
  			c.green=(col & 0xff00);
  			c.blue=(col & 0xff) << 8;
  	    XStoreColor(XD,colormap,&c);
  	    new_pal[nn]=c;
  		}
  		IconGroup::ColList=(long(*)[2])standard_palette;
  		IconGroup::ColListLen=18;
			hxc::alloc_colours_vector=steem_hxc_alloc_colours;
			hxc::free_colours_vector=steem_hxc_free_colours;
  	}
  	
    WhiteCol=255;
    BlackCol=0;
    BkCol=13;
    BorderLightCol=14;
    BorderDarkCol=254;
	}else{
    WhiteCol=WhitePixel(XD,Scr);
    BlackCol=BlackPixel(XD,Scr);
    BkCol=GetColourValue(XD,192 << 8,192 << 8,192 << 8,WhiteCol);
    BorderLightCol=GetColourValue(XD,224 << 8,224 << 8,224 << 8,WhiteCol);
    BorderDarkCol=GetColourValue(XD,128 << 8,128 << 8,128 << 8,BlackCol);
  }//SS 8bit

  cWinProc=XUniqueContext();
  cWinThis=XUniqueContext();

  Ico16.LoadIconsFromMemory(XD,Get_icon16_bmp(),16);
  Ico32.LoadIconsFromMemory(XD,Get_icon32_bmp(),32);
  Ico64.LoadIconsFromMemory(XD,Get_icon64_bmp(),64);
  IcoTOSFlags.LoadIconsFromMemory(XD,Get_tos_flags_bmp(),RC_FLAG_WIDTH);
  hxc_button::pcheck_ig=&Ico16;
  hxc_button::check_on_icon=ICO16_TICKED;
  hxc_button::check_off_icon=ICO16_UNTICKED;

  fileselect.set_alert_box_icons(&Ico32,&Ico16);
  fileselect.lpig=&Ico16;

  hints.XD=XD;
}

void steem_hxc_alloc_colours(Display*)
{
  hxc::col_black=0;
  hxc::col_white=255;
  hxc::col_grey=13;
  hxc::col_border_dark=254;
  hxc::col_border_light=14;
  hxc::col_sel_back=247;
  hxc::col_sel_fore=255;
  hxc::col_bk=13;
  hxc::colormap=colormap;
}

void steem_hxc_free_colours(Display*){
}



//---------------------------------------------------------------------------
void steem_hxc_modal_notify(bool going)
{
  // Warning: This could be called at any time! As we only need to do anything
  // when running it is safe.
  if (runstate!=RUNSTATE_RUNNING) return;

  if (going){
    SoundStop();
  }else{
    SoundStart();
  }
}


void XGUIUpdatePortDisplay()
{
  if (OptionBox.IsVisible()){
    // Update open buttons
    for (int p=0;p<3;p++) OptionBox.UpdatePortDisplay(p);
  }
}
//---------------------------------------------------------------------------
void PostRunMessage()
{
  if (XD==NULL || StemWin==0) return;

  XEvent SendEv;
  SendEv.type=ClientMessage;
  SendEv.xclient.window=StemWin;
  SendEv.xclient.message_type=RunSteemAtom;
  SendEv.xclient.format=32;
  XSendEvent(XD,StemWin,0,0,&SendEv);
}


void PrintHelpToStdout()
{
  printf(" \nsteem: run XSteem, the Atari STE emulator for X \n");
  printf("Written by Anthony and Russell Hayward.   \n \n");

  printf("Usage:  steem [options] [disk_image_a [disk_image_b]] [cartridge]\n");
  printf("        steem [options] [state_file]\n \n");

  printf("  disk image: name of disk image (extension ST/MSA/DIM/STT/ZIP/RAR) ");
  printf("for Steem to load.  If 2 disks are specified, the first ");
  printf("will be ST drive A: and the second drive B:.\n \n");

  printf("  cartridge:  name of a cartridge image (.STC) to be loaded.\n \n");

  printf("  state file: previously saved state file (.STS) to load.  If none ");
  printf("is specified, Steem will load \"auto.sts\" provided ");
  printf("the relevant option is checked in the Options dialog.\n \n");

  printf("  tos image:  name of TOS image to use (.IMG or .ROM).\n \n");

  printf("  options:    list of options separated by spaces.  Options are case-");
  printf("independent and can be prefixed by -, --, / or nothing.\n");
  printf("              NOSHM: disable use of Shared Memory.\n");
  printf("              NOSOUND: no sound output.\n");
  printf("              SOF=<n>: set sound output frequency to <n> Hz.\n");
  printf("              PABUFSIZE=<n>: set PortAudio buffer size to <n> samples.\n");
  printf("              FONT=<string>: use a different font.\n");
  printf("              HELP: print this message and quit.\n");
  printf("              INI=<file>: use <file> instead of steem.ini to ");
  printf("initialise options.\n");
  printf("              TRANS=<file>: use <file> instead of searching for ");
  printf("Translate.txt or Translate_*.txt to ");
  printf("translate the GUI text.\n \n");
  
  printf("All of these options (except INI= and TRANS=) can be changed ");
  printf("from the GUI once Steem is running.  It is easiest just to run ");
  printf("Steem and play with the GUI.\n\n");
}


bool GetWindowPositionData(Window Win,TWinPositionData *wpd)
{
  if (Win==0 || XD==NULL) return 1;

  XWindowAttributes wa;
  XGetWindowAttributes(XD,Win,&wa);
  wpd->Left=wa.x;
  wpd->Top=wa.y;
  wpd->Width=wa.width;
  wpd->Height=wa.height;

  wpd->Maximized=0;
  wpd->Minimized=0;

  return 0;
}
//---------------------------------------------------------------------------
void CentreWindow(Window Win,bool)
{
  if (XD==NULL) return;

  XWindowAttributes wa;
  XGetWindowAttributes(XD,Win,&wa);
  XMoveWindow(XD,Win,(GetScreenWidth()-wa.width)/2,
               (GetScreenHeight()-wa.height)/2);
}
//---------------------------------------------------------------------------
bool SetForegroundWindow(Window Win,Time TimeStamp)
{
  if (XD==NULL || Win==0) return 0;

  XRaiseWindow(XD,Win);
  XSync(XD,False);
  XError.display=NULL;
  XSetInputFocus(XD,Win,RevertToNone,TimeStamp);
  XFlush(XD);
  return (XError.display==NULL);
}
//---------------------------------------------------------------------------
Window GetForegroundWindow()
{
  if (XD==NULL) return 0;

	Window Foc;
	int Revert;
	XGetInputFocus(XD,&Foc,&Revert);
  return Foc;
}
//---------------------------------------------------------------------------
/*
NoEventMask            No events wanted
KeyPressMask           Keyboard down events wanted
KeyReleaseMask         Keyboard up events wanted
ButtonPressMask        Pointer button down events wanted
ButtonReleaseMask      Pointer button up events wanted
EnterWindowMask        Pointer window entry events wanted
LeaveWindowMask        Pointer window leave events wanted
PointerMotionMask      Pointer motion events wanted
PointerMotionHint-     Pointer motion hints wanted
Mask
Button1MotionMask      Pointer motion while button 1 down
Button2MotionMask      Pointer motion while button 2 down
Button3MotionMask      Pointer motion while button 3 down
Button4MotionMask      Pointer motion while button 4 down
Button5MotionMask      Pointer motion while button 5 down
ButtonMotionMask       Pointer motion while any button
                       down
KeymapStateMask        Keyboard state wanted at window
                       entry and focus in
ExposureMask           Any exposure wanted
VisibilityChangeMask   Any change in visibility wanted
StructureNotifyMask    Any change in window structure
                       wanted
ResizeRedirectMask     Redirect resize of this window
SubstructureNotify-    Substructure notification wanted
Mask
SubstructureRedi-      Redirect structure requests on
rectMask               children
FocusChangeMask        Any change in input focus wanted
PropertyChangeMask     Any change in property wanted
ColormapChangeMask     Any change in colormap wanted
OwnerGrabButtonMask    Automatic grabs should activate
                       with owner_events set to True
-------------------------------------------------------------
Event Category           Event Type
-------------------------------------------------------------
Keyboard events          KeyPress, KeyRelease
Pointer events           ButtonPress, ButtonRelease, Motion-
                         Notify

Window crossing events   EnterNotify, LeaveNotify
Input focus events       FocusIn, FocusOut
Keymap state notifica-   KeymapNotify
tion event
Exposure events          Expose, GraphicsExpose, NoExpose
Structure control        CirculateRequest, ConfigureRequest,
events                   MapRequest, ResizeRequest
Window state notifica-   CirculateNotify, ConfigureNotify,
tion events              CreateNotify, DestroyNotify,
                         GravityNotify, MapNotify,
                         MappingNotify, ReparentNotify,
                         UnmapNotify, VisibilityNotify
Colormap state notifi-   ColormapNotify
cation event
Client communication     ClientMessage, PropertyNotify,
events                   SelectionClear, SelectionNotify,
                         SelectionRequest
-------------------------------------------------------------
*/
//---------------------------------------------------------------------------
typedef int WNDPROC(void*,Window,XEvent*);
typedef WNDPROC* LPWINDOWPROC;

int ProcessEvent(XEvent *Ev)
{
  if(XD==NULL)
      return PEEKED_NOTHING;

  LPWINDOWPROC WinProc=(LPWINDOWPROC)GetProp(Ev->xany.window,cWinProc);
  if(WinProc)
  {
      void *x=(void*)GetProp(Ev->xany.window,cWinThis);
      return WinProc(x,Ev->xany.window,Ev);
      //return WinProc((void*)GetProp(Ev->xany.window,cWinThis),Ev->xany.window,Ev);
  }
  return PEEKED_MESSAGE;
}



void GUIUpdateInternalSpeakerBut()
{
  OptionBox.internal_speaker_but.set_check(false);
}


//---------------------------------------------------------------------------
short GetKeyState(int Key)
{
  if (Key==VK_LBUTTON || Key==VK_RBUTTON || Key==VK_MBUTTON){
    Window InWin,InChild;
    int RootX,RootY,WinX,WinY;
    UINT Mask;
    XQueryPointer(XD,StemWin,&InWin,&InChild,
                  &RootX,&RootY,&WinX,&WinY,&Mask);
    if (Key==VK_LBUTTON) return short((Mask & Button1Mask) ? -1:0);
    if (Key==VK_MBUTTON) return short((Mask & Button2Mask) ? -1:0);
    if (Key==VK_RBUTTON) return short((Mask & Button3Mask) ? -1:0);
  }else if (Key==VK_SHIFT){
    if (KeyState[VK_LSHIFT]<0 || KeyState[VK_RSHIFT]<0) return -1;
  }else if (Key==VK_CONTROL){
    if (KeyState[VK_LCONTROL]<0 || KeyState[VK_RCONTROL]<0) return -1;
  }else if (Key==VK_MENU){
    if (KeyState[VK_LMENU]<0 || KeyState[VK_RMENU]<0) return -1;
  }
  return KeyState[(BYTE)Key];
}
//---------------------------------------------------------------------------
void SetKeyState(int Key,bool Down,bool Toggled)
{
  KeyState[(BYTE)Key]=short((Toggled ? 1:0) | (Down ? 0x8000:0));
}
//---------------------------------------------------------------------------
short GetKeyStateSym(KeySym Sym)
{
  return KeyState[XKeysymToKeycode(XD,Sym)];
}
//---------------------------------------------------------------------------
TModifierState GetLRModifierStates()
{
  TModifierState mss;
  mss.LShift=(GetKeyState(VK_LSHIFT)<0);
  mss.RShift=(GetKeyState(VK_RSHIFT)<0);
  mss.LCtrl=(GetKeyState(VK_LCONTROL)<0);
  mss.RCtrl=(GetKeyState(VK_RCONTROL)<0);
  mss.LAlt=(GetKeyState(VK_LMENU)<0);
  mss.RAlt=(GetKeyState(VK_RMENU)<0);
  return mss;
}
//---------------------------------------------------------------------------
int MessageBox(WINDOWTYPE,char *Text,char *Caption,UINT Flags)
{
  int icon_index=-1;
  int mb_ico=(Flags&MB_ICONMASK);
  if(mb_ico==MB_ICONEXCLAMATION){
    icon_index=ICO32_EXCLAM;
  }else if(mb_ico==MB_ICONQUESTION){
    icon_index=ICO32_QUESTION;
  }else if(mb_ico==MB_ICONSTOP){
    icon_index=ICO32_STOP;
  }else if(mb_ico==MB_ICONINFORMATION){
    icon_index=ICO32_INFO;
  }
  if(icon_index==-1){
    alert.set_icons(NULL,0);
  }else{
    alert.set_icons(&Ico32,icon_index,&Ico16,icon_index);
  }
  int default_option=-1;
  switch(Flags&MB_DEFMASK){
  case MB_DEFBUTTON1:default_option=1;break;
  case MB_DEFBUTTON2:default_option=2;break;
  case MB_DEFBUTTON3:default_option=3;break;
  case MB_DEFBUTTON4:default_option=4;break;
  }
  int choice;

  switch (Flags & MB_TYPEMASK){
    case MB_OKCANCEL:
      choice=alert.ask(XD,Text,Caption,T("Okay")+"|"+T("Cancel"),default_option,1);
      if(choice==0)return IDOK;
      else return IDCANCEL;
    case MB_ABORTRETRYIGNORE:
      choice=alert.ask(XD,Text,Caption,T("Abort")+"|"+T("Retry")+"|"+T("Ignore"),default_option,0);
      if(choice==0)return IDABORT;
      else if(choice==1)return IDRETRY;
      else return IDRETRY;
    case MB_YESNOCANCEL:
      choice=alert.ask(XD,Text,Caption,T("Yes")+"|"+T("No")+"|"+T("Cancel"),default_option,2);
      if(choice==0)return IDYES;
      else if(choice==1)return IDNO;
      else return IDCANCEL;
    case MB_YESNO:
      choice=alert.ask(XD,Text,Caption,T("Yes")+"|"+T("No"),default_option,1);
      if(choice==0)return IDYES;
      else return IDNO;
    case MB_RETRYCANCEL:
      choice=alert.ask(XD,Text,Caption,T("Retry")+"|"+T("Cancel"),default_option,1);
      if(choice==0)return IDRETRY;
      else return IDCANCEL;
    default:
      alert.ask(XD,Text,Caption,T("Okay"),default_option,0);
      return IDOK;
  }
}
//---------------------------------------------------------------------------
void GetCursorPos(POINT *pPt)
{
  Window InWin,InChild;
  int RootX,RootY;
  UINT Mask;
  if (XQueryPointer(XD,Window(FullScreen ? Disp.XVM_FullWin:StemWin),&InWin,&InChild,
                    &RootX,&RootY,(int*)&(pPt->x),(int*)&(pPt->y),&Mask)==0){
    pPt->x=window_mouse_centre_x;pPt->y=window_mouse_centre_y;
  }
}
//---------------------------------------------------------------------------
void SetCursorPos(int x,int y)
{
  XWarpPointer(XD,None,Window(FullScreen ? Disp.XVM_FullWin:StemWin),0,0,0,0,x,y);
}
//---------------------------------------------------------------------------
int hyperlink_np(hxc_button *b,int mess,int *)
{
  if (mess!=BN_CLICKED) return 0;

  EasyStr Text=b->text;
  char *pipe=strchr(Text,'|');
  if (pipe) Text=pipe+1;
  bool web=IsSameStr_I(Text.Lefts(7),"http://");
  bool ftp=IsSameStr_I(Text.Lefts(6),"ftp://");
  bool email=IsSameStr_I(Text.Lefts(7),"mailto:");
  if (web || ftp || email){
    if (email) Text=Text.Text+7; // strip "mailto:"
    // Shell browser
    Str comline=Comlines[COMLINE_HTTP];
    if (ftp) comline=Comlines[COMLINE_FTP];
    if (email) comline=Comlines[COMLINE_MAILTO];
    shell_execute(comline,Str("[URL]\n")+Text+"\n[ADDRESS]\n"+Text);
  }
  return 0;
}
//---------------------------------------------------------------------------


#endif//UNIX

