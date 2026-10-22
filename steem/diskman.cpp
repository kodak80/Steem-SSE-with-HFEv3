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

DOMAIN: Disk image, GUI
FILE: diskman.cpp
DESCRIPTION: This file contains the code for Steem's disk manager. Some of
these functions are used in the emulation of Steem for vital processes such
as changing disk images and determining what files are disks.
Some TSF314 functions are defined here in diskman.cpp so that they're part
of the GUI object in the quick builds.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <diskman.h>
#include <debug.h>
#include <computer.h>
#include <display.h>
#include <osd.h>
#include <translate.h>
#include <harddiskman.h>
#include <archive.h>
#include <di_get_contents.h>
#include <choosefolder.h>
#include <notifyinit.h>
#include <dataloadsave.h>
#include <mymisc.h>
#include <draw.h>
#include <iolist.h>


//#include <CommCtrl.h>
#ifdef MINGW_BUILD
#define LVS_EX_INFOTIP          0x00000400 // listview does InfoTips for you
#define LVS_EX_LABELTIP         0x00004000 // listview unfolds partly hidden labels if it does not have infotip text
#define LVN_GETINFOTIPA          (LVN_FIRST-57)
typedef struct tagNMLVGETINFOTIPA
{
    NMHDR hdr;
    DWORD dwFlags;
    LPSTR pszText;
    int cchTextMax;
    int iItem;
    int iSubItem;
    LPARAM lParam;
} NMLVGETINFOTIPA, *LPNMLVGETINFOTIPA;
#endif

#ifdef UNIX
#include <x/hxc_prompt.h>
#endif
#ifdef WIN32
#include <windowsx.h>
#endif




#define LOGSECTION LOGSECTION_IMAGE_INFO

#ifdef WIN32

//#define LVS_SMALLVIEW LVS_SMALLICON
#define LVS_SMALLVIEW LVS_LIST

void TDiskManager::RefreshDiskView(EasyStr SelPath,bool EditLabel,
                                   EasyStr SelLinkPath,int iItem) {
#if !defined(SSE_LIBRETRONUKE)
  SetDir(DisksFol,false,SelPath,EditLabel,SelLinkPath,iItem);
#endif
}

#endif


bool ExtensionIsPastiDisk(char *Ext) {
#if USE_PASTI
  if(Ext==NULL||hPasti==NULL)  // for the plugin only
    return false;
  if(*Ext=='.') 
    Ext++;
/*  Pasti knows which extensions it can handle (STX,ST,MSA).
    If the option isn't checked (pasti_active is false), Pasti will run
    STX images only, without us changing pasti_active.
    If the option is checked, Pasti will get all DMA/FDC writes.
*/
  if(!pasti_active)
    return IsSameStr_I(Ext,DISK_EXT_STX);
  char *t=pasti_file_exts;
  while(*t) 
  {
    if(IsSameStr_I(Ext,t)) 
      return true;
    t+=strlen(t)+1;
  }
#endif
  return false;
}


#ifdef DEADC0DE
bool ExtensionIsArchive(char *TestedExt) {
  return (MatchesAnyString_I(TestedExt,"STZ","ZIP",
#if defined(SSE_DISK_RAR_SUPPORT) || defined(RARLIB_SUPPORT)
    "RAR",
#endif
#if defined(SSE_ARCHIVEACCESS_SUPPORT) || defined(SSE_DISK_7Z_SUPPORT_UNIX)
    "7Z","BZ2","GZ","TAR","ARJ",
#endif
    NULL));
}
#endif


int ExtensionIsDisk(char *TestedExt) {
  int ret=0;
  if(TestedExt==NULL) 
    return ret;
  if(*TestedExt=='.') 
    TestedExt++;

#ifdef WIN32 // check if plugins present
  if(MatchesAnyString_I(TestedExt,"STZ","ZIP",NULL))
    ret=(enable_zip) ? DISK_COMPRESSED : DISK_COMPRESSED_NODLL;
#if defined(SSE_DISK_RAR_SUPPORT) || defined(RARLIB_SUPPORT)
  else if(MatchesAnyString_I(TestedExt,"RAR",NULL))
    ret=(UNRAR_OK||ARCHIVEACCESS_OK) ? DISK_COMPRESSED : DISK_COMPRESSED_NODLL;
#endif
#if defined(SSE_ARCHIVEACCESS_SUPPORT)
  else if(MatchesAnyString_I(TestedExt,"7Z","BZ2","GZ","TAR","ARJ",NULL))
    ret=(ARCHIVEACCESS_OK) ? DISK_COMPRESSED : DISK_COMPRESSED_NODLL;
#endif
#endif//WIN32

#ifdef UNIX // we don't check
  if(MatchesAnyString_I(TestedExt,"STZ","ZIP",
#if defined(SSE_DISK_RAR_SUPPORT) || defined(RARLIB_SUPPORT)
    "RAR",
#endif
#if defined(SSE_ARCHIVEACCESS_SUPPORT) || defined(SSE_DISK_7Z_SUPPORT_UNIX)
    "7Z","BZ2","GZ","TAR","ARJ",
#endif
    NULL))
    ret=DISK_COMPRESSED;
#endif//UNIX

#if USE_PASTI
  else if(hPasti && !ret && ExtensionIsPastiDisk(TestedExt))
    ret=DISK_PASTI; // plugin: STX + other formats
#endif
#if defined(SSE_DISK_STX)
  else if(!ret && MatchesAnyString_I(TestedExt,DISK_EXT_STX,NULL))
    ret=DISK_PASTI; // native: STX only
#else
  else if(MatchesAnyString_I(TestedExt,DISK_EXT_STX,NULL))
    ret=DISK_NODLL; // "Missing plugin"
#endif
#if defined(SSE_DISK_CAPS)
  else if(MatchesAnyString_I(TestedExt,DISK_EXT_IPF,DISK_EXT_CTR,NULL))
    ret=(SSEConfig.CapsImgLib)?DISK_UNCOMPRESSED:DISK_NODLL;
#endif
  else if(MatchesAnyString_I(TestedExt,DISK_EXT_ST,DISK_EXT_STT,DISK_EXT_DIM,
    DISK_EXT_MSA,
#if defined(SSE_DISK_SCP)
    DISK_EXT_SCP,
#endif    
#if defined(SSE_DISK_STW)
    DISK_EXT_STW,
#endif  
#if defined(SSE_DISK_HFE)
    DISK_EXT_HFE,
#endif
    NULL))
    ret=DISK_UNCOMPRESSED;
#if defined(SSE_TOS_PRG_AUTORUN)
  else if(OPTION_PRG_SUPPORT
    && MatchesAnyString_I(TestedExt,DISK_EXT_PRG,DISK_EXT_TOS,NULL))
    ret=DISK_UNCOMPRESSED;
#endif
  else if(!ret && MatchesAnyString_I(TestedExt,CONFIG_FILE_EXT,NULL))
    ret=DISK_IS_CONFIG;
#if defined(SSE_DISK_M3U)
  else if(!ret && MatchesAnyString_I(TestedExt,EXT_M3U,NULL))
    ret=DISK_IS_PLAYLIST; // we'll open first disk of the list
#endif
  return ret;
}



EasyStr TDiskManager::CreateDiskName(char *Name,char *DiskInZip) {
  EasyStr Ret=Name;
#if 1 // we want the file inside the archive, never the archive
  if(DiskInZip!=NULL && DiskInZip[0]!='\0') 
    Ret=DiskInZip;
#else
  if(DiskInZip[0]) 
    Ret=Ret+" ("+DiskInZip+")";
#endif
  return Ret;
}


void TDiskManager::PerformInsertAction(int Action,EasyStr Name,EasyStr Path,EasyStr DiskInZip) {
  bool bInsertSucceeded=true;
  bool bAllowInsert2=(Action==ACTION_INSERT_A||Action==ACTION_INSERT_RUN);
#if defined(SSE_DISK_SWAPPER)
  Action&=3;
#endif
  if(Path.NotEmpty())
    bInsertSucceeded=InsertDisk((Action==ACTION_INSERT_B) ? DRIVE_B : DRIVE_A,
      Name,Path,false,false,DiskInZip,false,bAllowInsert2);
  else
    EjectDisk((Action==ACTION_INSERT_B) ? DRIVE_B : DRIVE_A);
  if(bInsertSucceeded && Action==ACTION_INSERT_RUN) 
  {
#ifdef WIN32
    if(CloseAfterIRR && Handle) 
      PostMessage(Handle,WM_CLOSE,0,0);
    if(IsIconic(StemWin)) 
      OpenIcon(StemWin);
    SetForegroundWindow(StemWin);
#else
    if(SetForegroundWindow(StemWin)==0) return;
    if(CloseAfterIRR && Handle) {
      XEvent SendEv;
      SendEv.type=ClientMessage;
      SendEv.xclient.window=Handle;
      SendEv.xclient.message_type=hxc::XA_WM_PROTOCOLS;
      SendEv.xclient.format=32;
      SendEv.xclient.data.l[0]=hxc::XA_WM_DELETE_WINDOW;
      XSendEvent(XD,Handle,0,0,&SendEv);
    }
#endif
    reset_st(RESET_COLD|RESET_NOSTOP|RESET_CHANGESETTINGS|RESET_BACKUP|RESET_COUNT);
    if(runstate!=RUNSTATE_RUNNING) 
    {
      CLICK_PLAY_BUTTON();
    }
    else 
    {
      WORD mode=(OPTION_CAPTURE_MOUSE&BIT_0) ? STEM_MOUSEMODE_WINDOW : STEM_MOUSEMODE_DISABLED;
      SetStemMouseMode(mode);
#ifndef SSE_NO_OSD
      osd_init_run(true);
#endif
    }
  }
}


void TDiskManager::SetNumFloppies(BYTE NewNum) {
  // The ST can handle max 2 floppy drives
  // if NewNum is 0, the GEM will appear without drive icons on reset
  // so internally Steem emulates 0 drives (ST with no connected floppy)
  // Steem's GUI only allows 1 or 2 though and there's no plan to change that
  nFloppyDrives=MIN(NewNum,(BYTE)2); // in case of wrong ini
#if defined(SSE_DISK_CAPS)
  Caps.fdc.drivemax=Caps.fdc.drivecnt=NewNum;
#endif
#if USE_PASTI
  if(hPasti) 
  {
    struct pastiCONFIGINFO pci;
    pci.flags=PASTI_CFDRIVES;
    pci.ndrives=NewNum;
    pci.drvFlags=0;
    pasti->Config(&pci);
  }
#endif
#if !defined(SSE_LIBRETRONUKE)
  if(Handle) 
  {
#ifdef WIN32
    if(HWND win=GetDlgItem(Handle,IDC_DRIVEB)) 
      InvalidateRect(win,NULL,FALSE);
#endif
#ifdef UNIX
    if(nFloppyDrives==2)
      drive_icon[1].set_icon(&Ico32,ICO32_DRIVE_B);
    else
      drive_icon[1].set_icon(&Ico32,ICO32_DRIVE_B_OFF);
#endif
  }
  CheckResetDisplay();
#endif//#if !defined(SSE_LIBRETRONUKE)
  UPDATE_STATUS_BAR_PART(SB_PART_CAPS);
}


TDiskManager::TDiskManager() {
  Section="Disks";
  EjectDisksWhenQuit=ShowHiddenFiles=HideBroken=bArchiveRW=bTurboDrive=false;
#if !defined(SSE_DEFAULTS_421)
  floppy_access_ff=false;
#endif
  HideExtension=true;
#if defined(SSE_DISK_STX)
  bDiskProtectImageStx=true; // so it is RW only if player wants it
#endif
  DoubleClickAction=ACTION_INSERT_RUN;
  nFloppyDrives=1; // more compatible
  DiskImageType=1;

#ifdef WIN32
#ifndef SSE_NO_WINSTON_IMPORT
  ImportOnlyIfExist=true;
  ImportConflictAction=0;
  ContentConflictAction=2;
#endif
#if defined(SSE_ACSI_MNGR) // more space for the ACSI icon in initial disk manager
  Width=403+70+GuiSM.cx_frame()*2+GuiSM.cx_vscroll();
#else 
  Width=403+GuiSM.cx_frame()*2+GuiSM.cx_vscroll();
#endif
  Height=331+100+GuiSM.cy_caption();
#if !defined(SSE_420R4)
  Left=(GuiSM.cx_screen()-Width)/2;
  Top=(GuiSM.cy_screen()-Height)/2;
#endif
  FSWidth=Width;
  FSHeight=Height;
#if !defined(SSE_420R4)
  FSLeft=320-FSWidth/2;
  FSTop=240-FSHeight/2;
#endif
  il[DRIVE_A]=il[DRIVE_B]=NULL;
  Dragging=DropTarget=-1;
  DiskDiag=NULL;
#if !defined(SSE_NO_WINSTON_IMPORT)
  LinksDiag=NULL;
  ImportDiag=NULL;
#endif
  PropDiag=NULL;ContentDiag=NULL;DatabaseDiag=NULL;
  SecsPerTrackIdx=9;TracksIdx=80;SidesIdx=2;
  SaveScroll=0;
#if defined(SSE_GUI_DM_BIGICONS)
  IconSize=2; // 64x64
#else
  SmallIcons=false; // 32x32
#endif
  IconSpacing=1;

  DoExtraShortcutCheck=false;

  MSAConvProcess=NULL;
#ifndef SSE_NO_WINSTON_IMPORT
  HKEY Key;
  WinSTonPath="C:\\Program Files\\WinSTon";
  if(RegOpenKey(HKEY_CURRENT_USER,"Software\\WinSTon",&Key)==ERROR_SUCCESS) {
    DWORD Size=500;
    EasyStr Path;
    Path.SetLength(Size);
    if(RegQueryValueEx(Key,"InstalledDirectory",NULL,NULL,(BYTE*)Path.Text,&Size)==ERROR_SUCCESS) {
      WinSTonPath=Path;
    }
    RegCloseKey(Key);
  }
  NO_SLASH(WinSTonPath);
  WinSTonDiskPath=WinSTonPath+"\\Discs";
#endif
  MSAConvPath="";
  DatabaseFind="";
  DatabaseDiag=NULL;
#endif//WIN32

#ifdef UNIX
  DisksFol="///";
  HomeFol="///";
  Width=500;Height=400;
  HistBackLength=0;
  HistForwardLength=0;
#endif
}


#if !defined(SSE_LIBRETRONUKE)

void TDiskManager::Show() {
  if(Handle!=NULL) 
  {
#ifdef WIN32
    if(IsIconic(Handle)) 
      ShowWindow(Handle,SW_SHOWNORMAL);
    SetForegroundWindow(Handle);
#endif
    return;
  }

#ifdef WIN32
  bool MaximizeIt=(FullScreen)?FSMaximized:Maximized;
  ManageWindowClasses(SD_REGISTER);
  Handle=CreateWindowEx(WS_EX_CONTROLPARENT|WS_EX_APPWINDOW,"Steem Disk Manager",T("Disk Manager"),
                        WS_CAPTION|WS_SYSMENU|WS_SIZEBOX|WS_MAXIMIZEBOX|WS_MINIMIZEBOX,
                        Left,Top,Width,Height,ParentWin,NULL,hInstance,NULL);
#if defined(SSE_420R4)
  UpdateLeftTop();
#endif
  if(HandleIsInvalid()) 
  {
    ManageWindowClasses(SD_UNREGISTER);
    return;
  }
  SetWindowLongPtr(Handle,GWLP_USERDATA,(LONG_PTR)this);
  MakeParent((FullScreen) ? StemWin : NULL);
  HWND Win;

#if defined(SSE_GUI_DM_BIGICONS) // we copy the whole block for cleaner modding

  WORD FlopContIconSize=64; // size of the icon in the floppy content view, originally 32 now 64
  const int extra_diskin_h=(BIG_ICONS?26:16);

  // DM diskview
  for(int Countdown=10;Countdown>0;Countdown--) //?
  {
    DiskView=CreateWindowEx(WS_EX_ACCEPTFILES|WS_EX_CLIENTEDGE,WC_LISTVIEW,"", WS_CHILD|WS_VISIBLE
                            |WS_TABSTOP|LVS_ICON|LVS_SHAREIMAGELISTS|LVS_SINGLESEL|LVS_EDITLABELS,
                            10,FlopContIconSize+50+GuiSM.mCharHeight+BIG_ICONS*8+extra_diskin_h,
                            GUIMUL(480),GUIMUL(300),Handle,(HMENU)IDC_DISKVIEW,hInstance,NULL);
    ASSERTB(DiskView);
    if(DiskView) 
      break;
    Sleep(50);
  }
#ifndef SSE_LEAN_AND_MEAN
  if(DiskView==NULL)
  {
    DestroyWindow(Handle);Handle=NULL;
    ManageWindowClasses(SD_UNREGISTER);
    return;
  }
#endif
  LoadIcons();
  ListView_SetImageList(DiskView,il[0],LVSIL_SMALL);
  ListView_SetImageList(DiskView,il[MAX(IconSize,(char)1)],LVSIL_NORMAL); // 1-max
  ListView_SetExtendedListViewStyle(DiskView, LVS_EX_INFOTIP  | LVS_EX_LABELTIP);
  AdaptBackground(); // colors including GEM green!
  
  // DM toolbar
  int x=10,y=FlopContIconSize+64;
  int d=23;
  int wh=21;
  if(BIG_ICONS)
    d+=16,wh+=16,y+=10;
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_BACK),WS_CHILD|WS_VISIBLE|WS_DISABLED
                   |WS_TABSTOP|PBS_RIGHTCLICK,x,y,wh,wh,Handle,(HMENU)IDC_BACK,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Back"));
  x+=d;
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_FORWARD),WS_CHILD|WS_VISIBLE|WS_DISABLED
                   |WS_TABSTOP|PBS_RIGHTCLICK,x,y,wh,wh,Handle,(HMENU)IDC_FORWARD,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Forward"));
  x+=d;
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_HOME),WS_CHILD|WS_VISIBLE|WS_TABSTOP
                   |PBS_RIGHTCLICK,x,y,wh,wh,Handle,(HMENU)IDC_HOME,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("To home folder"));
  x+=d;
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_SETHOME),WS_CHILD|WS_VISIBLE|WS_TABSTOP
                   |PBS_RIGHTCLICK,x,y,wh,wh,Handle,(HMENU)IDC_SET_HOME,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Make this folder your home folder"));
  x+=d;
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_SETTINGS),WS_CHILD|WS_VISIBLE|WS_TABSTOP
                   |PBS_RIGHTCLICK,x,y,wh,wh,Handle,(HMENU)IDC_OPTIONS,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Disk Manager settings"));
  x+=d;
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_DISKMANTOOLS),WS_CHILD|WS_VISIBLE|WS_TABSTOP
                   |PBS_RIGHTCLICK,x,y,wh,wh,Handle,(HMENU)IDC_DISKMANTOOLS,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Disk image management tools"));
  x+=d;
  int w=GuiSM.mCbUnits*2;
  Win=CreateWindow("Combobox","",WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|CBS_HASSTRINGS
                   |CBS_DROPDOWNLIST,x,y,w,200,Handle,(HMENU)IDC_PCDRIVE,hInstance,NULL);
  char Root[4]={0,':','\\',0};
  for(int dr=0;dr<27;dr++) 
  {
    Root[0]=char('A'+dr);
    if(GetDriveType(Root)>1)
      SendMessage(Win,CB_ADDSTRING,0,(LPARAM)Root);
  }
  x+=w+GuiSM.mHorizontalSeparation;
  CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Path Display","",WS_CHILD|WS_VISIBLE,
                 x,y,0,0,Handle,(HMENU)IDP_DISK,hInstance,NULL);

  // floppy drive icon + content (view) A+B
  HWND hDriveA=CreateWindowEx(WS_EX_CONTROLPARENT,"Steem Disk Manager Drive Icon","A",WS_CHILD
                              |WS_VISIBLE|WS_TABSTOP,10,10+extra_diskin_h,64,64,Handle,
                              (HMENU)IDC_DRIVEA,hInstance,NULL);
  ToolAddWindow(ToolTip,hDriveA,T("Right click for some options"));
  int Disabled=(AreNewDisksInHistory(DRIVE_A)?0:WS_DISABLED);
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_SMALLDOWNARROW),WS_CHILDWINDOW|WS_VISIBLE
                   |WS_TABSTOP|Disabled,52,52,12,12,hDriveA,(HMENU)IDC_HISTBUT,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Drive A disk history"));
  HWND hContentA=CreateWindowEx(WS_EX_ACCEPTFILES|WS_EX_CLIENTEDGE,WC_LISTVIEW,"",WS_CHILD|WS_VISIBLE
                                |WS_TABSTOP|LVS_ICON|LVS_SHAREIMAGELISTS|LVS_SINGLESEL|LVS_NOSCROLL
                                |LVS_AUTOARRANGE,75,10,FlopContIconSize+58,FlopContIconSize+32
                                +extra_diskin_h,Handle,(HMENU)IDC_CONTENTA,hInstance,NULL);
  ListView_SetImageList(hContentA,il[2],LVSIL_NORMAL); // 64x64
  SetDriveViewEnable(DRIVE_A,false);
  HWND hDriveB=CreateWindowEx(WS_EX_CONTROLPARENT,"Steem Disk Manager Drive Icon","B",WS_CHILD
                              |WS_VISIBLE|WS_TABSTOP,143+FlopContIconSize,10+extra_diskin_h,64,64,
                              Handle,(HMENU)IDC_DRIVEB,hInstance,NULL);
  EasyStr tip=T("Left click to switch on/off\n")+T("Right click for some options");
  ToolAddWindow(ToolTip,Win,tip);
  Disabled=(AreNewDisksInHistory(DRIVE_B)?0:WS_DISABLED);
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_SMALLDOWNARROW),WS_CHILDWINDOW|WS_VISIBLE
                   |WS_TABSTOP|Disabled,52,52,12,12,hDriveB,(HMENU)IDC_HISTBUT,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Drive B disk history"));
  HWND hContentB=CreateWindowEx(WS_EX_ACCEPTFILES|WS_EX_CLIENTEDGE,WC_LISTVIEW,"",WS_CHILD
                                |WS_VISIBLE|WS_TABSTOP|LVS_ICON|LVS_SHAREIMAGELISTS|LVS_SINGLESEL
                                |LVS_NOSCROLL|LVS_AUTOARRANGE,208+FlopContIconSize,10,
                                FlopContIconSize+58,FlopContIconSize+32+extra_diskin_h,Handle,
                                (HMENU)IDC_CONTENTB,hInstance,NULL);
  ListView_SetImageList(hContentB,il[2],LVSIL_NORMAL);
  SetDriveViewEnable(DRIVE_B,false);

  // ACSI + GEMDOS HDD icons
  {
    int ico=RC_ICO_HARDDRIVES;
#ifdef DEADC0DE // Sorry Frenchies
    if(IsSameStr_I(T("File"),"Fichier")) ico=RC_ICO_HARDDRIVES_FR;
#endif
    Win=CreateWindow("Steem Flat PicButton",Str(ico),WS_CHILD|WS_VISIBLE|WS_TABSTOP|PBS_RIGHTCLICK,
                     400,10,64,64,Handle,(HMENU)IDC_HDGEMDOS,hInstance,NULL);
    SendMessage(Win,BM_SETCHECK,!HardDiskMan.DisableHardDrives,0);
#if defined(SSE_ACSI_MNGR)
    ToolAddWindow(ToolTip,Win,tip);
    ico=RC_ICO_HARDDRIVES_ACSI;
    DWORD style=WS_CHILD|WS_VISIBLE|WS_TABSTOP|PBS_RIGHTCLICK;
    Win=CreateWindow("Steem Flat PicButton",Str(ico),style|ENABLE_CHECK(!pasti_active),
                     400,10,64,64,Handle,(HMENU)IDC_HDACSI,hInstance,NULL);
    SendMessage(Win,BM_SETCHECK,SSEOptions.Acsi,0);
    ToolAddWindow(ToolTip,Win,tip);
#else
    ToolAddWindow(ToolTip,Win,T("Hard Drive Manager"));
#endif
  }

  Font=SSEConfig.GuiFont();
  SetWindowAndChildrensFont(Handle,Font);
  SetWindowLongPtr(hDriveA,GWLP_USERDATA,(LONG_PTR) this);
  SetWindowLongPtr(hDriveB,GWLP_USERDATA,(LONG_PTR) this);
  Old_ListView_WndProc=(WNDPROC)GetClassLongPtr(hContentA,GCLP_WNDPROC);
  SetWindowLongPtr(hContentA,GWLP_USERDATA,(LONG_PTR) this);
  SetWindowLongPtr(hContentA,GWLP_WNDPROC, (LONG_PTR)DriveView_WndProc);
  SetWindowLongPtr(hContentB,GWLP_USERDATA,(LONG_PTR) this);
  SetWindowLongPtr(hContentB,GWLP_WNDPROC,(LONG_PTR)DriveView_WndProc);
  /*SetWindowLongPtr(GetDlgItem(Handle,IDC_DISKVIEW),GWLP_USERDATA,(LONG_PTR) this);
  SetWindowLongPtr(GetDlgItem(Handle,IDC_DISKVIEW),GWLP_WNDPROC,
    (LONG_PTR)DiskView_WndProc);*/
  SetWindowLongPtr(DiskView,GWLP_USERDATA,(LONG_PTR) this);
  SetWindowLongPtr(DiskView,GWLP_WNDPROC,(LONG_PTR)DiskView_WndProc);
  for(BYTE i=DRIVE_A;i<=DRIVE_B;i++) 
  {
    if(FloppyDrive[i].DiskInDrive())
      InsertDisk(i,FloppyDisk[i].DiskName,FloppyDrive[i].GetDisk(),true,false,
                 FloppyDisk[i].DiskInZip);
  }
  ShowWindow(Handle,(MaximizeIt) ? SW_MAXIMIZE : SW_SHOW);
  UpdateWindow(Handle);
  SetDiskViewMode((IconSize==0) ? LVS_SMALLVIEW : LVS_ICON);

#else//#if defined(SSE_GUI_DM_BIGICONS)

  const int extra_diskin_h=(BIG_ICONS?26:16);
  for(int Countdown=10;Countdown>0;Countdown--) //?
  {
    DiskView=CreateWindowEx(WS_EX_ACCEPTFILES|WS_EX_CLIENTEDGE,WC_LISTVIEW,"",
      WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_ICON|LVS_SHAREIMAGELISTS|LVS_SINGLESEL
      |LVS_EDITLABELS,10,82+GuiSM.mCharHeight+BIG_ICONS*8+extra_diskin_h,GUIMUL(480),
      GUIMUL(200),Handle,(HMENU)IDC_DISKVIEW,hInstance,NULL);
    if(DiskView) 
      break;
    Sleep(50);
  }
  if(DiskView==NULL)
  {
    DestroyWindow(Handle);Handle=NULL;
    ManageWindowClasses(SD_UNREGISTER);
    return;
  }
  LoadIcons();
  ListView_SetImageList(DiskView,il[0],LVSIL_NORMAL);
  ListView_SetImageList(DiskView,il[1],LVSIL_SMALL);
  ListView_SetExtendedListViewStyle(DiskView, LVS_EX_INFOTIP  | LVS_EX_LABELTIP);
  AdaptBackground();
  int x=10,y=80+16;
  int d=23;
  int wh=21;
  if(BIG_ICONS)
    d+=16,wh+=16,y+=10;
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_BACK),WS_CHILD|WS_VISIBLE
    |WS_DISABLED|WS_TABSTOP|PBS_RIGHTCLICK,x,y,wh,wh,Handle,(HMENU)IDC_BACK,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Back"));
  x+=d;
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_FORWARD),WS_CHILD|WS_VISIBLE|WS_DISABLED
    |WS_TABSTOP|PBS_RIGHTCLICK,x,y,wh,wh,Handle,(HMENU)IDC_FORWARD,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Forward"));
  x+=d;
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_HOME),WS_CHILD|WS_VISIBLE
    |WS_TABSTOP|PBS_RIGHTCLICK,x,y,wh,wh,Handle,(HMENU)IDC_HOME,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("To home folder"));
  x+=d;
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_SETHOME),WS_CHILD
    |WS_VISIBLE|WS_TABSTOP|PBS_RIGHTCLICK,x,y,wh,wh,Handle,(HMENU)IDC_SET_HOME,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Make this folder your home folder"));
  x+=d;
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_SETTINGS),WS_CHILD
    |WS_VISIBLE|WS_TABSTOP|PBS_RIGHTCLICK,x,y,wh,wh,Handle,(HMENU)IDC_OPTIONS,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Disk Manager settings"));
  x+=d;
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_DISKMANTOOLS),WS_CHILD|WS_VISIBLE|WS_TABSTOP
    |PBS_RIGHTCLICK,x,y,wh,wh,Handle,(HMENU)IDC_DISKMANTOOLS,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Disk image management tools"));
  x+=d;
  int w=GuiSM.mCbUnits*2;
  Win=CreateWindow("Combobox","",WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL
    |CBS_HASSTRINGS|CBS_DROPDOWNLIST,x,y,w,200,Handle,(HMENU)IDC_PCDRIVE,hInstance,NULL);
  char Root[4]={0,':','\\',0};
  for(int dr=0;dr<27;dr++) 
  {
    Root[0]=char('A'+dr);
    if(GetDriveType(Root)>1)
      SendMessage(Win,CB_ADDSTRING,0,(LPARAM)Root);
  }
  x+=w+GuiSM.mHorizontalSeparation;
  CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Path Display","",WS_CHILD|WS_VISIBLE,
    x,y,0,0,Handle,(HMENU)IDP_DISK,hInstance,NULL);
  HWND hDriveA=CreateWindowEx(WS_EX_CONTROLPARENT,"Steem Disk Manager Drive Icon","A",WS_CHILD
    |WS_VISIBLE|WS_TABSTOP,10,10+extra_diskin_h,64,64,Handle,(HMENU)IDC_DRIVEA,hInstance,NULL);
  ToolAddWindow(ToolTip,hDriveA,T("Right click for some options"));
  int Disabled=(AreNewDisksInHistory(DRIVE_A)?0:WS_DISABLED);
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_SMALLDOWNARROW),WS_CHILDWINDOW|WS_VISIBLE
    |WS_TABSTOP|Disabled,52,52,12,12,hDriveA,(HMENU)IDC_HISTBUT,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Drive A disk history"));
  HWND hContentA=CreateWindowEx(WS_EX_ACCEPTFILES|WS_EX_CLIENTEDGE,WC_LISTVIEW,"",
    WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|LVS_ICON|LVS_SHAREIMAGELISTS|LVS_SINGLESEL|LVS_NOSCROLL,
    75,10,90,64+extra_diskin_h,Handle,(HMENU)IDC_CONTENTA,hInstance,NULL);
  ListView_SetIconSpacing(hContentA,88,200);
  ListView_SetImageList(hContentA,il[0],LVSIL_NORMAL);
  SetDriveViewEnable(DRIVE_A,false);

  HWND hDriveB=CreateWindowEx(WS_EX_CONTROLPARENT,"Steem Disk Manager Drive Icon","B",
    WS_CHILD|WS_VISIBLE|WS_TABSTOP,175,10+extra_diskin_h,64,64,Handle,
     (HMENU)IDC_DRIVEB,hInstance,NULL);
  EasyStr tip=T("Left click to switch on/off\n")+T("Right click for some options");
  ToolAddWindow(ToolTip,Win,tip);
  Disabled=(AreNewDisksInHistory(DRIVE_B)?0:WS_DISABLED);
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_SMALLDOWNARROW),WS_CHILDWINDOW|WS_VISIBLE
    |WS_TABSTOP|Disabled,52,52,12,12,hDriveB,(HMENU)IDC_HISTBUT,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Drive B disk history"));
  HWND hContentB=CreateWindowEx(WS_EX_ACCEPTFILES|WS_EX_CLIENTEDGE,WC_LISTVIEW,"",
    WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|LVS_ICON|LVS_SHAREIMAGELISTS|LVS_SINGLESEL|LVS_NOSCROLL,
    240,10,90,64+extra_diskin_h,Handle,(HMENU)IDC_CONTENTB,hInstance,NULL);
  ListView_SetIconSpacing(hContentB,88,200);
  ListView_SetImageList(hContentB,il[0],LVSIL_NORMAL);
  SetDriveViewEnable(DRIVE_B,false);
  {
    int ico=RC_ICO_HARDDRIVES;
#ifdef DEADC0DE // Sorry Frenchies
    if(IsSameStr_I(T("File"),"Fichier")) ico=RC_ICO_HARDDRIVES_FR;
#endif
    Win=CreateWindow("Steem Flat PicButton",Str(ico),WS_CHILD|WS_VISIBLE|WS_TABSTOP|PBS_RIGHTCLICK,
      400,10,64,64,Handle,(HMENU)IDC_HDGEMDOS,hInstance,NULL);
    SendMessage(Win,BM_SETCHECK,!HardDiskMan.DisableHardDrives,0);
#if defined(SSE_ACSI_MNGR)
    ToolAddWindow(ToolTip,Win,tip);
      //T("GEMDOS Hard Drive Manager - right click to toggle on/off"));
    ico=RC_ICO_HARDDRIVES_ACSI;
    DWORD style=WS_CHILD|WS_VISIBLE|WS_TABSTOP|PBS_RIGHTCLICK;
    Win=CreateWindow("Steem Flat PicButton",Str(ico),style|(pasti_active?
      WS_DISABLED:0),400,10,64,64,Handle,(HMENU)IDC_HDACSI,hInstance,NULL);
    SendMessage(Win,BM_SETCHECK,SSEOptions.Acsi,0);
    ToolAddWindow(ToolTip,Win,tip);
      //T("ACSI Hard Drive Manager - right click to toggle on/off"));
#else
    ToolAddWindow(ToolTip,Win,T("Hard Drive Manager"));
#endif
  }
  Font=SSEConfig.GuiFont();
  SetWindowAndChildrensFont(Handle,Font);
  SetWindowLongPtr(hDriveA,GWLP_USERDATA,(LONG_PTR) this);
  SetWindowLongPtr(hDriveB,GWLP_USERDATA,(LONG_PTR) this);
  Old_ListView_WndProc=(WNDPROC)GetClassLongPtr(hContentA,GCLP_WNDPROC);
  SetWindowLongPtr(hContentA,GWLP_USERDATA,(LONG_PTR) this);
  SetWindowLongPtr(hContentA,GWLP_WNDPROC, (LONG_PTR)DriveView_WndProc);
  SetWindowLongPtr(hContentB,GWLP_USERDATA,(LONG_PTR) this);
  SetWindowLongPtr(hContentB,GWLP_WNDPROC,(LONG_PTR)DriveView_WndProc);
  /*SetWindowLongPtr(GetDlgItem(Handle,IDC_DISKVIEW),GWLP_USERDATA,(LONG_PTR) this);
  SetWindowLongPtr(GetDlgItem(Handle,IDC_DISKVIEW),GWLP_WNDPROC,
    (LONG_PTR)DiskView_WndProc);*/
  SetWindowLongPtr(DiskView,GWLP_USERDATA,(LONG_PTR) this);
  SetWindowLongPtr(DiskView,GWLP_WNDPROC,(LONG_PTR)DiskView_WndProc);
  for(WORD i=DRIVE_A;i<=DRIVE_B;i++) 
  {
    if(FloppyDrive[i].DiskInDrive())
      InsertDisk(i,FloppyDisk[i].DiskName,FloppyDrive[i].GetDisk(),
        true,false,FloppyDisk[i].DiskInZip);
  }
  ShowWindow(Handle,(MaximizeIt) ? SW_MAXIMIZE : SW_SHOW);
  UpdateWindow(Handle);
  SetDiskViewMode((SmallIcons) ? LVS_SMALLVIEW : LVS_ICON);

#endif//#if defined(SSE_GUI_DM_BIGICONS)

  RefreshDiskView();
  // point to disk in A: if possible
  if(FloppyDrive[DRIVE_A].NotEmpty())
  {
    TDiskManFileInfo *Inf=GetItemInf(0,hContentA);
    EasyStr Fol=Inf->Path;
    char *slash=strrchr(Fol,SLASHCHAR);
    if(slash) 
      *slash='\0';
    if(IsSameStr_I(Fol,DisksFol))
      GoToDisk(Inf->Path,false,false);
  }
  SetFocus(DiskView);
  if(StemWin) 
    PostMessage(StemWin,WM_USER,1234,0);
#endif//WIN32

#ifdef UNIX
  if (StandardShow(Width,Height,T("Disk Manager"),
        ICO16_DISKMAN,ExposureMask | StructureNotifyMask,
        (LPWINDOWPROC)WinProc,true)) return;

  SetWindowNormalSize(XD,Handle,10+32+10+10+10+60+60+10
#if defined(SSE_ACSI_MNGR)
    +70 //?
#endif
    ,110+50+10);

  int y=10;
  for(int d=0;d<2;d++){
    drive_icon[d].create(XD,Handle,10,y-2,32,32,button_notify_handler,this,
                          BT_ICON | BT_STATIC | BT_BORDER_NONE | BT_TEXT_CENTRE,
                          EasyStr(char('A'+d)),100+d,BkCol);
    drive_icon[d].set_icon(&Ico32,ICO32_DRIVE_A+d);

    disk_name[d].create(XD,Handle,43,y,320,25,button_notify_handler,this,
                          BT_TEXT | BT_STATIC | BT_TEXT_CENTRE | BT_BORDER_INDENT,
                          "",200+d,WhiteCol);

    eject_but[d].create(XD,Handle,Width-103
#if defined(SSE_ACSI_MNGR)
    -70
#endif    
    ,y+1,25,25,
              button_notify_handler,this,BT_ICON,"Eject",302+d,BkCol);
	  eject_but[d].set_icon(&Ico16,ICO16_EJECTDISK);
	
    SetWindowGravity(XD,eject_but[d].handle,NorthEastGravity);
    y+=34;
  }

  HardBut.create(XD,Handle,Width-70,10,60,60,
              button_notify_handler,this,BT_ICON,"",10,BkCol);
  if(IsSameStr_I(T("File"),"Fichier")) // French localisation!
    HardBut.set_icon(&Ico64,ICO64_HARDDRIVES_FR);
  else
    HardBut.set_icon(&Ico64,ICO64_HARDDRIVES);
  SetWindowGravity(XD,HardBut.handle,NorthEastGravity);

#if defined(SSE_ACSI_MNGR)
  hints.add(HardBut.handle,T("GEMDOS Hard Drive Manager"),Handle);
  HardButAcsi.create(XD,Handle,Width-70-70,10,60,60,
              button_notify_handler,this,BT_ICON,"",11,BkCol);
  HardButAcsi.set_icon(&Ico64,ICO64_HARDDRIVES_ACSI);
  SetWindowGravity(XD,HardButAcsi.handle,NorthEastGravity);
  hints.add(HardButAcsi.handle,T("ACSI Hard Drive Manager"),Handle);
#else
  hints.add(HardBut.handle,T("Hard Drive Manager"),Handle);
#endif


  BackBut.create(XD,Handle,10,82,21,21,
              button_notify_handler,this,BT_ICON,"",2,BkCol);
  BackBut.set_icon(&Ico16,ICO16_BACK);
  hints.add(BackBut.handle,T("Back"),Handle);

  ForwardBut.create(XD,Handle,35,82,21,21,
              button_notify_handler,this,BT_ICON,"",3,BkCol);
  ForwardBut.set_icon(&Ico16,ICO16_FORWARD);
  hints.add(ForwardBut.handle,T("Forward"),Handle);

  HomeBut.create(XD,Handle,60,82,21,21,
              button_notify_handler,this,BT_ICON,"",4,BkCol);
  HomeBut.set_icon(&Ico16,ICO16_HOMEFOLDER);
  hints.add(HomeBut.handle,T("To home folder"),Handle);

  SetHomeBut.create(XD,Handle,85,82,21,21,
              button_notify_handler,this,BT_ICON,"",5,BkCol);
  SetHomeBut.set_icon(&Ico16,ICO16_SETHOMEFOLDER);
  hints.add(SetHomeBut.handle,T("Make this folder your home folder"),Handle);

  MenuBut.create(XD,Handle,110,82,21,21,
              button_notify_handler,this,BT_ICON,"",6,BkCol);
  MenuBut.set_icon(&Ico16,ICO16_DISKMANMENU);
  hints.add(MenuBut.handle,T("Disk Manager settings"),Handle);
  DirOutput.create(XD,Handle,135,80,445,25,NULL,this,
                    BT_TEXT | BT_STATIC | BT_TEXT_PATH | BT_BORDER_INDENT,
                    DisksFol,0,WhiteCol);

  if (StemWin) DiskBut.set_check(true);

  dir_lv.ext_sl.DeleteAll();
  dir_lv.ext_sl.Add(3,T("Parent Directory"),1,1,1);
  dir_lv.ext_sl.Add(3,"",ICO16_FOLDER,ICO16_FOLDERLINK,ICO16_FOLDERLINKBROKEN);
  dir_lv.ext_sl.Add(4,"st",ICO16_DISK,ICO16_DISKLINK,ICO16_DISKLINKBROKEN,ICO16_DISK_RO);
  dir_lv.ext_sl.Add(4,"stt",ICO16_DISK,ICO16_DISKLINK,ICO16_DISKLINKBROKEN,ICO16_DISK_RO);
  dir_lv.ext_sl.Add(4,"dim",ICO16_DISK,ICO16_DISKLINK,ICO16_DISKLINKBROKEN,ICO16_DISK_RO);
  dir_lv.ext_sl.Add(4,"msa",ICO16_DISK,ICO16_DISKLINK,ICO16_DISKLINKBROKEN,ICO16_DISK_RO);
#if defined(SSE_DISK_STW)
  dir_lv.ext_sl.Add(4,"stw",ICO16_DISK,ICO16_DISKLINK,ICO16_DISKLINKBROKEN,ICO16_DISK_RO);
#endif
#if defined(SSE_DISK_SCP)
  dir_lv.ext_sl.Add(4,"scp",ICO16_DISK,ICO16_DISKLINK,ICO16_DISKLINKBROKEN,ICO16_DISK_RO);
#endif
#if defined(SSE_DISK_HFE)
  dir_lv.ext_sl.Add(4,"hfe",ICO16_DISK,ICO16_DISKLINK,ICO16_DISKLINKBROKEN,ICO16_DISK_RO);
#endif
#if defined(SSE_DISK_STX)
  dir_lv.ext_sl.Add(4,"stx",ICO16_DISK,ICO16_DISKLINK,ICO16_DISKLINKBROKEN,ICO16_DISK_RO);
#endif
#if defined(SSE_DISK_CAPS)
  dir_lv.ext_sl.Add(4,"ipf",ICO16_DISK,ICO16_DISKLINK,ICO16_DISKLINKBROKEN,ICO16_DISK_RO);
  dir_lv.ext_sl.Add(4,"ctr",ICO16_DISK,ICO16_DISKLINK,ICO16_DISKLINKBROKEN,ICO16_DISK_RO);
#endif
  ArchiveTypeIdx=dir_lv.ext_sl.NumStrings;
  int zipicon=/*ICO16_ZIP_RO;
  if (bArchiveRW) zipicon=*/ICO16_ZIP_RW;
  dir_lv.ext_sl.Add(3,"zip",zipicon,ICO16_DISKLINK,ICO16_DISKLINKBROKEN);
  dir_lv.ext_sl.Add(3,"stz",zipicon,ICO16_DISKLINK,ICO16_DISKLINKBROKEN);
#if defined(RAR_SUPPORT) || defined(SSE_DISK_RAR_SUPPORT_UNIX)
  dir_lv.ext_sl.Add(3,"rar",zipicon,ICO16_DISKLINK,ICO16_DISKLINKBROKEN);
#endif
#if defined(SSE_DISK_7Z_SUPPORT_UNIX) // "7Z","BZ2","GZ","TAR","ARJ"
  dir_lv.ext_sl.Add(3,"7z",zipicon,ICO16_DISKLINK,ICO16_DISKLINKBROKEN);
  dir_lv.ext_sl.Add(3,"bz2",zipicon,ICO16_DISKLINK,ICO16_DISKLINKBROKEN);
  dir_lv.ext_sl.Add(3,"gz",zipicon,ICO16_DISKLINK,ICO16_DISKLINKBROKEN);
  dir_lv.ext_sl.Add(3,"tar",zipicon,ICO16_DISKLINK,ICO16_DISKLINKBROKEN);
  dir_lv.ext_sl.Add(3,"arj",zipicon,ICO16_DISKLINK,ICO16_DISKLINKBROKEN);
#endif
  dir_lv.lpig=&Ico16;
  dir_lv.base_fol="";
  dir_lv.fol=DisksFol;
  dir_lv.allow_type_change=0;
  dir_lv.show_broken_links=0; //(HideBroken==0);
  dir_lv.HideExtension=HideExtension;
  dir_lv.create(XD,Handle,10,110,285,120,dir_lv_notify_handler,this);

  SetNumFloppies(nFloppyDrives);

  UpdateDiskNames(0);
  UpdateDiskNames(1);

  XMapWindow(XD,Handle);
#endif//UNIX
}

#endif

void TDiskManager::Hide() {
#if !defined(SSE_LIBRETRONUKE)
  if(Handle==NULL) 
    return;

#ifdef WIN32
  HardDiskMan.Hide();
#ifndef SSE_LEAN_AND_MEAN
  if(HardDiskMan.Handle) 
    return;
#endif
  if(DatabaseDiag)
    SendMessage(DatabaseDiag,WM_CLOSE,0,0);
#if defined(SSE_ACSI_MNGR)
  AcsiHardDiskMan.Hide();
#ifndef SSE_LEAN_AND_MEAN
  if(AcsiHardDiskMan.Handle) 
    return;
#endif
#endif
  ShowWindow(Handle,SW_HIDE);
  if(FullScreen) 
    SetFocus(StemWin);
  ToolsDeleteAllChildren(ToolTip,Handle);
  ToolsDeleteAllChildren(ToolTip,GetDlgItem(Handle,IDC_DRIVEA));
  ToolsDeleteAllChildren(ToolTip,GetDlgItem(Handle,IDC_DRIVEB));
  int c=(int)SendMessage(DiskView,LVM_GETITEMCOUNT,0,0);
  for(int i=0;i<c;i++)
    SendMessage(DiskView,LVM_DELETEITEM,0,0);
  DestroyWindow(Handle);
  Handle=DiskView=NULL;
  for(int n=0;n<NICONSIZES;n++) 
  {
    ImageList_Destroy(il[n]);
    il[n]=NULL;
  }
  if(StemWin) 
    PostMessage(StemWin,WM_USER,1234,0);
  ManageWindowClasses(SD_UNREGISTER);
#endif//WIN32

#ifdef UNIX
  if(XD==NULL)
    return;
  if(HardDiskMan.IsVisible())
    return;
  hints.remove_all_children(Handle);
  StandardHide();
  if(StemWin)
    DiskBut.set_check(0);
#endif
#endif//#if !defined(SSE_LIBRETRONUKE)
}


TDiskManager::~TDiskManager() { 
  Hide(); 
}


#ifdef WIN32

#if !defined(SSE_LIBRETRONUKE)
void TDiskManager::ManageWindowClasses(bool bUnreg) {
  WNDCLASS wc;
  char *ClassName[3]={
    "Steem Disk Manager",
    "Steem Disk Manager Dialog",
    "Steem Disk Manager Drive Icon"
  };
  if(bUnreg)
  {
    for(int n=0;n<3;n++)
      UnregisterClass(ClassName[n],hInstance);
  }
  else 
  {
    RegisterMainClass(WndProc,ClassName[0],RC_ICO_DISKMAN);
    wc.style=CS_DBLCLKS;
    wc.lpfnWndProc=Dialog_WndProc;
    wc.cbClsExtra=wc.cbWndExtra=0;
    wc.hInstance=hInstance;
    wc.hIcon=hGUIIconSmall[RC_ICO_DRIVE];
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszMenuName=NULL;
    wc.lpszClassName=ClassName[1];
    RegisterClass(&wc);
    wc.style=0;
    wc.lpfnWndProc=Drive_Icon_WndProc;
    wc.cbClsExtra=wc.cbWndExtra=0;
    wc.hInstance=hInstance;
    wc.hIcon=NULL;
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszMenuName=NULL;
    wc.lpszClassName=ClassName[2];
    RegisterClass(&wc);
  }
}


void TDiskManager::LoadIcons() {
  if(Handle==NULL) 
    return;
  HIMAGELIST old_il[NICONSIZES];
  for(int i=0;i<NICONSIZES;i++)
    old_il[i]=il[i];
  
#if defined(SSE_GUI_DM_BIGICONS)
  HICON *pIcons=hGUIIconSmall;
  for(int n=0,s=16;n<NICONSIZES;n++,s<<=1) // s 16 32 64 128
#else
  HICON *pIcons=hGUIIcon;
  for(int n=0;n<2;n++) 
#endif
  {
#if defined(SSE_DISK_M3U)
#if defined(SSE_GUI_DM_BIGICONS)
    il[n]=ImageList_Create(s,s,BPPToILC|ILC_MASK,9+1+1+2+1,9+1+1+2+1);
#else
    //il[n]=ImageList_Create(32-n*16,32-n*16,BPPToILC|ILC_MASK,9+1+1+2+1,9+1+1+2+1);
    il[n]=ImageList_Create(32-n*16,32-n*16,BPPToILC|ILC_MASK,15,1);
#endif
#else
    il[n]=ImageList_Create(32-n*16,32-n*16,BPPToILC|ILC_MASK,9+1+1+2,9+1+1+2);
#endif
    if(il[n]) 
    {
      ImageList_AddIcon(il[n],pIcons[RC_ICO_FOLDER]);
      ImageList_AddIcon(il[n],pIcons[RC_ICO_DRIVE]);
      ImageList_AddIcon(il[n],pIcons[RC_ICO_PARENTDIR]); 
      ImageList_AddIcon(il[n],pIcons[RC_ICO_FOLDERLINK]);
      ImageList_AddIcon(il[n],pIcons[RC_ICO_DRIVELINK]);
      ImageList_AddIcon(il[n],pIcons[RC_ICO_DRIVEREADONLY]);
      ImageList_AddIcon(il[n],pIcons[RC_ICO_FOLDERBROKEN]);
      ImageList_AddIcon(il[n],pIcons[RC_ICO_DRIVEBROKEN]);
    //  if(bArchiveRW) //it's not saved anyway, not worth complicating the GUI
        ImageList_AddIcon(il[n],pIcons[RC_ICO_DRIVEZIPPED_RW]); // = ST disk
    //  else
#ifdef SSE_421R0
      ImageList_AddIcon(il[n],pIcons[RC_ICO_DRIVEZIPPED_RO]); // = zipped folder
#endif
      ImageList_AddIcon(il[n],pIcons[RC_ICO_PRGFILEICO]);
      ImageList_AddIcon(il[n],pIcons[RC_ICO_CFG]);
#if! defined(SSE_GUI_DM_BIGICONS_LESS)
      ImageList_AddIcon(il[n],pIcons[RC_ICO_GREYDISK]);
      ImageList_AddIcon(il[n],pIcons[RC_ICO_BLUEDISK]);
#endif
#if defined(SSE_DISK_M3U)
      ImageList_AddIcon(il[n],pIcons[RC_ICO_TEXT]);
#endif
    }
#if defined(SSE_GUI_DM_BIGICONS)
    if(s==16)
      pIcons=hGUIIcon; // 32x32
    else
      pIcons=hGUIIcon128; // 64x64 and 128x128
#else
    pIcons=hGUIIconSmall; 
#endif
  }
  if(VisibleDiag()) 
    SetClassLongPtr(VisibleDiag(),GCLP_HICON,(LONG_PTR)(hGUIIconSmall[RC_ICO_DRIVE]));
  if(HWND hGemdos=GetDlgItem(Handle,IDC_HDGEMDOS)) 
  {
    // Update controls
    for(int id=IDC_HOME;id<IDC_PCDRIVE;id++) //80-89
    {
      if(HWND hwnd=GetDlgItem(Handle,id))
        PostMessage(hwnd,BM_RELOADICON,0,0);
    }
    PostMessage(hGemdos,BM_RELOADICON,0,0);
#if defined(SSE_ACSI_MNGR)
    PostMessage(GetDlgItem(Handle,IDC_HDACSI),BM_RELOADICON,0,0);
#endif
    HWND hwnd=GetDlgItem(Handle,IDC_DRIVEA);
    PostMessage(GetDlgItem(hwnd,IDC_HISTBUT),BM_RELOADICON,0,0);
    InvalidateRect(hwnd,NULL,TRUE);
    hwnd=GetDlgItem(Handle,IDC_DRIVEB);
    PostMessage(GetDlgItem(hwnd,IDC_HISTBUT),BM_RELOADICON,0,0);
    InvalidateRect(hwnd,NULL,TRUE);
#if defined(SSE_GUI_DM_BIGICONS)
    ListView_SetImageList(DiskView,il[0],LVSIL_SMALL);
    ListView_SetImageList(DiskView,il[MAX(IconSize,(char)1)],LVSIL_NORMAL);
#else
    ListView_SetImageList(DiskView,il[0],LVSIL_NORMAL);
    ListView_SetImageList(DiskView,il[1],LVSIL_SMALL);
#endif
    LPARAM count=SendMessage(DiskView,LVM_GETITEMCOUNT,0,0);
    SendMessage(DiskView,LVM_REDRAWITEMS,0,count);
  }
  for(int n=0;n<NICONSIZES;n++) 
    if(old_il[n]) 
      ImageList_Destroy(old_il[n]);
}


void TDiskManager::SetDiskViewMode(int Mode) {
  SetWindowLong(DiskView,GWL_STYLE,
    (GetWindowLong(DiskView,GWL_STYLE) & ~LVS_SMALLVIEW & ~LVS_ICON)|Mode);
#if defined(SSE_GUI_DM_BIGICONS)
  if(IconSize==0)
#else
  if(SmallIcons) 
#endif
  {
    TWidthHeight widh=GetTextSize(Font,"Width of y Line in small icon view");
    widh.Width/=2;
    if(IconSpacing==1)
      widh.Width*=2;
    if(IconSpacing==2)
      widh.Width*=4;
    ListView_SetColumnWidth(DiskView,(DWORD)-1,18+widh.Width);
  }
  else 
  {
    TWidthHeight widh=GetTextSize(Font,"8");
#if defined(SSE_GUI_DM_BIGICONS)
    ListView_SetImageList(DiskView,il[IconSize],LVSIL_NORMAL);
    int sizeinpix=16<<IconSize;
    ListView_SetIconSpacing(DiskView,sizeinpix+24+IconSpacing*GUIMUL(12),
                            sizeinpix+6+(widh.Height+2)*2);
#else
    ListView_SetIconSpacing(DiskView,32+24+IconSpacing*GUIMUL(12),38+(widh.Height+2)*2);
#endif
  }
  SendMessage(DiskView,LVM_SORTITEMS,(WPARAM)this,(LPARAM)CompareFunc);
}


// this will display all the image disk icons
void TDiskManager::SetDir(EasyStr NewFol,bool AddToHistory,EasyStr SelPath,
                          bool EditLabel,EasyStr SelLinkPath,int iSelItem) {
  EasyStr Fol=NewFol;
  if(Fol.RightChar()!='\\' && Fol.RightChar()!='/') 
    Fol+=SLASH;
  WIN32_FIND_DATA wfd;
  HANDLE Find=FindFirstFile(Fol+"*.*",&wfd);
  if(Find!=INVALID_HANDLE_VALUE) 
  {
    SetCursor(LoadCursor(NULL,IDC_WAIT));
    {
      SetWindowText(GetDlgItem(Handle,IDP_DISK),Fol.Lefts(Fol.Length()-1).Text
        +MIN(3,(int)Fol.Length()-1));
      HWND hPcDrive=GetDlgItem(Handle,IDC_PCDRIVE);
      int idx=(int)SendMessage(hPcDrive,CB_FINDSTRING,0xffffffff,(LPARAM)((Fol.Lefts(2)+SLASH).Text));
      if(idx>-1) 
        SendMessage(hPcDrive,CB_SETCURSEL,idx,0);
    }
    SendMessage(DiskView,WM_SETREDRAW,0,0);
    int c=(int)SendMessage(DiskView,LVM_GETITEMCOUNT,0,0);
    for(int i=0;i<c;i++)
      SendMessage(DiskView,LVM_DELETEITEM,0,0);
#if defined(SSE_GUI_DM_BIGICONS)
    if(IconSize==0)
#else
    if(SmallIcons) 
#endif
    {
      SetDiskViewMode(LVS_ICON);
      SetDiskViewMode(LVS_SMALLVIEW);
    }
    IShellLink *LinkObj=NULL;
    IPersistFile *FileObj=NULL;
    HRESULT hres=CoCreateInstance(CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,
                                  IID_IShellLink,(void**)&LinkObj);
    if(!SUCCEEDED(hres))
      LinkObj=NULL;
    if(LinkObj) 
    {
      hres=LinkObj->QueryInterface(IID_IPersistFile,(void**)&FileObj);
      if(!SUCCEEDED(hres))
        FileObj=NULL;
    }
    DynamicArray<TDiskManFileInfo*> Files;
    Files.Resize(512); // Assume for 512 items
    Files.SizeInc=128; // Increase by 128 items at a time
    EasyStr Name,Path,Extension,LinkPath;
    char *exts;
    TDiskManFileInfo *Inf;
    bool Link,Broken;
#if defined(SSE_DISK_SWAPPER) // checking archives could be slow
    TNotify myNotify( ((HideBroken) ? (T("Disk operation").Text) : NULL));
#endif
    do {
      bool Add=true;
      if((wfd.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN)&&!ShowHiddenFiles)
        Add=false;
      else if(wfd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) 
      {
        if(wfd.cFileName[0]=='.' && wfd.cFileName[1]=='\0')
          Add=false;
       // else if(wfd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)
       // keep those, eg Frontpage's website folder // Add=0; 
      }
      if(Add) 
      {
        Link=false;
        Name=wfd.cFileName;
        Path=Fol+Name;
        LinkPath="";
        Extension="";
        Broken=false;
        if((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)==0) 
        {
          exts=strrchr(Name,'.');
          if(exts!=NULL)
          {
            if(HideExtension)
              *exts='\0'; //Strip extension from Name
            Extension=exts+1;
            strupr(Extension);
            if(IsSameStr_I(Extension,"LNK"))
            {
              Link=true;
              LinkPath=Path;
              Path=GetLinkDest(Fol+wfd.cFileName,&wfd,NULL,LinkObj,FileObj);
              NO_SLASH(Path);
              if(Path.NotEmpty()&&DoExtraShortcutCheck) 
              {
                HANDLE hFind=FindFirstFile(Path,&wfd);
                if(hFind!=INVALID_HANDLE_VALUE) 
                {
                  FindClose(hFind);
                  EasyStr DestFilePath=Path;
                  RemoveFileNameFromPath(DestFilePath,WITH_SLASH);
                  Path=DestFilePath+wfd.cFileName;
                }
              }
              if(Path.NotEmpty()) 
              {
#if defined(SSE_LONG_PATH)
                EasyStr sTmp=Path;
                PathPrePend(sTmp,false);
                UINT HostDriveType=GetDriveType(sTmp.Lefts(2)+SLASH);
#else
                UINT HostDriveType=GetDriveType(Path.Lefts(2)+SLASH);
#endif
                if(HostDriveType==DRIVE_NO_ROOT_DIR)
                  Broken=true;
                else if(HostDriveType!=DRIVE_REMOVABLE && HostDriveType!=DRIVE_CDROM)
                {
#if defined(SSE_LONG_PATH)
                  if(sTmp.Length()!=2)
#else
                  if(Path.Length()!=2) 
#endif
                    Broken=(GetFileAttributes(Path)==INVALID_FILE_ATTRIBUTES);
                }
                exts=strrchr(wfd.cFileName,'.');
                if(exts!=NULL) 
                {
                  Extension=exts+1;
                  strupr(Extension);
                }
              }
            }//if(IsSameStr_I(Extension,"LNK"))
          }
        }
        if(Path.NotEmpty()&&(!Broken||!HideBroken)) 
        {
          if((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)&&!(Broken && wfd.nFileSizeLow==1))
          { // folder
            Inf=new TDiskManFileInfo;
            Inf->Folder=true;
            Inf->ReadOnly=false;
            Inf->BrokenLink=Broken;
            Inf->Zip=false;
            if(Name=="..") 
            {
              Inf->Image=IMG_PARENTDIR;
              Inf->Name=T("Parent Directory");
              Str HigherPath=Fol;
              NO_SLASH(HigherPath);
              RemoveFileNameFromPath(HigherPath,REMOVE_SLASH);
              Inf->Path=HigherPath;
              Inf->LinkPath="";
              Inf->UpFolder=true;
            }
            else 
            {
              Inf->Image=(Link?3:0)+(Broken?3:0);
              Inf->Name=Name;
              Inf->Path=Path;
              Inf->LinkPath=LinkPath;
              Inf->UpFolder=false;
            }
            Files.Add(Inf);
          }
          else 
          {
            int Type=ExtensionIsDisk(Extension);
            if(Type==DISK_UNCOMPRESSED||Type==DISK_PASTI||Type==DISK_IS_CONFIG
#if defined(SSE_DISK_M3U)
               || Type==DISK_IS_PLAYLIST
#endif
               )
            {
              Inf=new TDiskManFileInfo;
              Inf->Name=Name;
              Inf->Date=wfd.ftLastWriteTime;//
              Inf->Path=Path;
              Inf->LinkPath=LinkPath;
              Inf->Folder=false;
              Inf->UpFolder=false;
              Inf->ReadOnly=(Broken) ? false : (wfd.dwFileAttributes&FILE_ATTRIBUTE_READONLY);
              Inf->BrokenLink=Broken;
              Inf->Zip=false;
              Inf->Image=1+(Link ? 3 : 0)+(Broken ? 3 : 0);
              if(Inf->ReadOnly && !Link)
                Inf->Image=IMG_DISKREADONLY;
#if defined(SSE_TOS_PRG_AUTORUN)
              if(OPTION_PRG_SUPPORT && MatchesAnyString_I(Extension,DISK_EXT_PRG,DISK_EXT_TOS,NULL))
                Inf->Image=IMG_PRGFILEICO;
#endif
              if(Type==DISK_IS_CONFIG)
                Inf->Image=IMG_CFG;
#if defined(SSE_DISK_M3U)
              else if(Type==DISK_IS_PLAYLIST)
                Inf->Image=IMG_PLAYLIST;
#endif
#if !defined(SSE_GUI_DM_BIGICONS_LESS)
              else if(Inf->Image==IMG_DISK)
              { // de facto IMG_DISK is the default for stx, scp, ipf

                if(Type!=DISK_PASTI)
                {
                   if(MatchesAnyString_I(Extension,DISK_EXT_ST,DISK_EXT_MSA,DISK_EXT_DIM,NULL))
                     Inf->Image=IMG_BLUEDISK;
                   else if(MatchesAnyString_I(Extension,DISK_EXT_STW,DISK_EXT_HFE,NULL))
                     Inf->Image=IMG_GREYDISK;
                }
              }
#endif//#if !defined(SSE_GUI_DM_BIGICONS_LESS)
#if defined(SSE_GUI_DM_PICS)
              if(IconSize>0) // not for 16x16 list, 32x32 OK, 64x64 better
              {
                if(MyGdiPlus.GetPic(Path))
                    Inf->Image=IMG_CUSTOM; // Steem will display the pic of the disk instead of an icon!
              }
#endif
              Files.Add(Inf);
            }
            else if(Type==DISK_COMPRESSED && enable_zip) 
            {
#if defined(SSE_DISK_SWAPPER) // it works but it's slow -> optional
              if(!HideBroken || Link || IsDiskImage(Path.Text))
#endif
              {
                Inf=new TDiskManFileInfo;
                Inf->Name=Name;
                Inf->Date=wfd.ftLastWriteTime;//
                Inf->Path=Path;
                Inf->LinkPath=LinkPath;
                Inf->Folder=false;
                Inf->UpFolder=false;
                Inf->ReadOnly=true;
                Inf->BrokenLink=Broken;
                Inf->Zip=true;
#if defined(SSE_421R0)
                if(Link)
                  Inf->Image=(Broken)?IMG_DISKBROKEN:IMG_DISKLINK;
                else if(HideBroken) // we know that it's an image disk
                  Inf->Image=IMG_DISKZIPPED_RW;
                else
                  Inf->Image=IMG_ZIPPED_FILE;
#else
                Inf->Image=(Link) ? (IMG_DISKLINK+(Broken?3:0)) : IMG_DISKZIPPED_RW;
#endif
#if defined(SSE_GUI_DM_PICS)
                if(IconSize>0) // not for 16x16 list, 32x32 OK, 64x64 better
                {
                  if(MyGdiPlus.GetPic(Path))
                    Inf->Image=IMG_CUSTOM; // Steem will display the pic of the disk instead of an icon!
                }
#endif
                Files.Add(Inf);
              }
            }
          }
        }
      }
    } while(FindNextFile(Find,&wfd));
    FindClose(Find);
    if(LinkObj) 
      LinkObj->Release();
    if(FileObj) 
      FileObj->Release();
    SendMessage(DiskView,LVM_SETITEMCOUNT,Files.NumItems+16,0);
    LV_ITEM lvi;
    lvi.mask=LVIF_TEXT|LVIF_IMAGE|LVIF_PARAM;
    lvi.iItem=lvi.iSubItem=0;
    lvi.pszText=LPSTR_TEXTCALLBACK;
    for(int n=0;n<Files.NumItems;n++) 
    {
      lvi.lParam=(LPARAM)(Files[n]);
      lvi.iImage=Files[n]->Image;
      SendMessage(DiskView,LVM_INSERTITEM,0,LPARAM(&lvi));
    }
    Files.DeleteAll();
    SendMessage(DiskView,LVM_SORTITEMS,(WPARAM)this,(LPARAM)CompareFunc);
    if(NotSameStr_I(NewFol,DisksFol)) 
    {
      if(AddToHistory) 
      {
        HistForward[0]="";
        for(int n=DM_HISTORY_LEN-1;n>0;n--)
          HistBack[n]=HistBack[n-1];
        HistBack[0]=DisksFol;
        EnableWindow(GetDlgItem(Handle,IDC_BACK),TRUE);
        if(GetFocus()==GetDlgItem(Handle,IDC_FORWARD))
          SetFocus(GetDlgItem(Handle,IDC_BACK));
        EnableWindow(GetDlgItem(Handle,IDC_FORWARD),FALSE);
      }
      DisksFol=NewFol;
      NO_SLASH(DisksFol);
    }
    if(IsSameStr_I(DisksFol,HomeFol)) 
    {
      HWND focus=GetFocus();
      if(focus==GetDlgItem(Handle,IDC_HOME)||focus==GetDlgItem(Handle,IDC_SET_HOME))
        SetFocus(GetDlgItem(Handle,IDC_PCDRIVE));
      AtHome=true;
    }
    else
      AtHome=false;
    if(SelPath.NotEmpty()||SelLinkPath.NotEmpty()) 
    {
      if(!SelectItemWithPath(SelPath,EditLabel,SelLinkPath))
        SelPath="",SelLinkPath="";
    }
    if(SelPath.IsEmpty()&&SelLinkPath.IsEmpty()) 
    {
      lvi.stateMask=LVIS_SELECTED|LVIS_FOCUSED;
      lvi.state=LVIS_SELECTED|LVIS_FOCUSED;
      iSelItem=BOUND(iSelItem,0,MAX((LONG)SendMessage(DiskView,LVM_GETITEMCOUNT,0,0)-1,0L));
      SendMessage(DiskView,LVM_SETITEMSTATE,iSelItem,(LPARAM)&lvi);
      SendMessage(DiskView,LVM_ENSUREVISIBLE,iSelItem,1);
    }
    SendMessage(DiskView,WM_SETREDRAW,1,0);
    InvalidateRect(DiskView,NULL,TRUE);
    UpdateWindow(DiskView);
    SetCursor(PCArrow);
  }
}
#endif//#if !defined(SSE_LIBRETRONUKE)


int CALLBACK TDiskManager::CompareFunc(LPARAM lPar1,LPARAM lPar2,LPARAM lPar3) {
  TDiskManFileInfo *Inf1=(TDiskManFileInfo*)lPar1;
  TDiskManFileInfo *Inf2=(TDiskManFileInfo*)lPar2;
  if(Inf1->UpFolder)
    return -1;
  else if(Inf2->UpFolder)
    return 1;
  else if(Inf1->Folder && !Inf2->Folder)
    return -1;
  else if(!Inf1->Folder&&Inf2->Folder)
    return 1;
  else if(((TDiskManager*)lPar3)->SortBy) // by date
    return CompareFileTime(&Inf2->Date,&Inf1->Date); // new first
  else
    return strcmpi(Inf1->Name.Text,Inf2->Name.Text);    
}


bool TDiskManager::SelectItemWithPath(char *Path,bool EditLabel,char *LinkPath) {
  int c=(int)SendMessage(DiskView,LVM_GETITEMCOUNT,0,0);
  bool Match;
  LV_ITEM lvi;
  lvi.mask=LVIF_PARAM;
  lvi.iSubItem=0;
  for(lvi.iItem=0;lvi.iItem<c;lvi.iItem++) 
  {
    SendMessage(DiskView,LVM_GETITEM,0,(LPARAM)&lvi);
    Match=true;
    if(Path)     
      Match&=(bool)(Path[0] ? IsSameStr_I(((TDiskManFileInfo*)lvi.lParam)->Path,Path):true);
    if(LinkPath) 
      Match&=(bool)(LinkPath[0] ?
        IsSameStr_I(((TDiskManFileInfo*)lvi.lParam)->LinkPath,LinkPath):true);
    if(Match) 
    {
      lvi.stateMask=LVIS_SELECTED|LVIS_FOCUSED;
      lvi.state=LVIS_SELECTED|LVIS_FOCUSED;
      SendMessage(DiskView,LVM_SETITEMSTATE,lvi.iItem,(LPARAM)&lvi);
      SendMessage(DiskView,LVM_ENSUREVISIBLE,lvi.iItem,1);
      if(EditLabel) 
        SendMessage(DiskView,LVM_EDITLABEL,lvi.iItem,0);
      return true;
    }
  }
  return false;
}


int TDiskManager::GetSelectedItem() {
  int c=(int)SendMessage(DiskView,LVM_GETITEMCOUNT,0,0);
  LV_ITEM lvi;
  lvi.iSubItem=0;
  for(lvi.iItem=0;lvi.iItem<c;lvi.iItem++) 
  {
    if(SendMessage(DiskView,LVM_GETITEMSTATE,lvi.iItem,LVIS_SELECTED))
      return lvi.iItem;
  }
  return -1;
}


bool TDiskManager::HasHandledMessage(MSG *mess) {
  if(Dragging==-1) 
  {
    if(VisibleDiag())
      return !!IsDialogMessage(VisibleDiag(),mess);
    else
      return !!IsDialogMessage(Handle,mess);
  }
  else
    return false;
}


void TDiskManager::AddFoldersToMenu(HMENU Pop,int StartID,EasyStr NoAddFol,bool Setting) {
  int MaxWidth=GuiSM.cx_screen()/2;
  if(NotSameStr_I(HomeFol,NoAddFol)) 
  {
    InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,StartID,EasyStr(Setting?
      "(":"")+ShortenPath(HomeFol,Font,MaxWidth)+(Setting?")":""));
    InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,1999,NULL);
  }
  if(Setting)
    StartID+=5;
  else
    StartID++;
  for(int n=0;n<DM_QUICKFOL_LEN;n++) 
  {
    if(Setting) 
    {
      HMENU OptionsPop=CreatePopupMenu();
      InsertMenu(OptionsPop,0xffffffff,MF_BYPOSITION|MF_STRING,StartID+0,
        T("Change to Current Folder"));
      InsertMenu(OptionsPop,0xffffffff,MF_BYPOSITION|MF_STRING,StartID+1,T("Change to..."));
      InsertMenu(OptionsPop,0xffffffff,MF_BYPOSITION|MF_STRING,StartID+2,T("Erase"));
      InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_POPUP,
        (UINT_PTR)OptionsPop,EasyStr(1+n)+": ("+ShortenPath(QuickFol[n],Font,MaxWidth)+")");
      StartID+=5;
    }
    else if(QuickFol[n].NotEmpty()) 
    {
      InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|
        (IsSameStr_I(QuickFol[n],NoAddFol)?(MF_DISABLED|MF_GRAYED):0),
        StartID++,EasyStr(1+n)+": "+ShortenPath(QuickFol[n],Font,MaxWidth));
    }
    else
      InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_DISABLED|MF_GRAYED,
        StartID++,EasyStr(1+n)+":");
  }
}


Str TDiskManager::GetMSAConverterPath() { // called by context menu handlers
  if(MSAConvPath.NotEmpty())
  {
    if(Exists(MSAConvPath))
      return MSAConvPath;
  }
#if defined(_DEBUG)
  BREAKPOINT(Auto MSA path wont work in _DEBUG);
#else
  // First look for msa.exe, maybe there's no need to ask player
  EasyStr str(RunDir);
  str+=SLASH SSE_PLUGIN_DIR1 SLASH MSA_CONVERTER;
  if(Exists(str))
  {
    MSAConvPath=str;
    return MSAConvPath;
  }
  str=RunDir+SLASH SSE_PLUGIN_DIR2 SLASH MSA_CONVERTER;
  if(Exists(str))
  {
    MSAConvPath=str;
    return MSAConvPath;
  }
  str=RunDir+SLASH+MSA_CONVERTER;//   "\\msa.exe";
  if(Exists(str))
  {
    MSAConvPath=str;
    return MSAConvPath;
  }
#endif	// NDBG
  int i=Alert(T("Have you installed MSA Converter elsewhere on this computer?"),
    T("Run MSA Converter"),MB_ICONQUESTION|MB_YESNO);
  if(i==IDYES) 
  {
    Str Fol=MSAConvPath;
    if(Fol.NotEmpty())
      RemoveFileNameFromPath(Fol,REMOVE_SLASH);
    else 
    {
      char path[MAX_PATH];
      GetEnvironmentVariable("ProgramFiles",path,MAX_PATH); // probably wrong anyway
      Fol=path;
      ITEMIDLIST *idl;
      if(SHGetSpecialFolderLocation(NULL,CSIDL_PROGRAM_FILES,&idl)==NOERROR) 
      {
#if !defined(SSE_WIN32_A)
        IMalloc *Mal;SHGetMalloc(&Mal);
#endif
        Fol.SetLength(MAX_PATH);
        SHGetPathFromIDList(idl,Fol);
#if defined(SSE_WIN32_A)
        CoTaskMemFree(idl);
#else
        Mal->Free(idl);
#endif
      }
      NO_SLASH(Fol);
    }
    EnableAllWindows(false,Handle);
    char *fstypes=FSTypes(1,T("Executables").Text,"*.exe",NULL);
    EasyStr NewMSA=FileSelect((FullScreen) ? StemWin : Handle,T("Run MSA Converter"),
      Fol,fstypes,1,true,"exe");
    free(fstypes);
    if(NewMSA.NotEmpty()) 
      MSAConvPath=NewMSA;
    SetForegroundWindow(Handle);
    EnableAllWindows(true,Handle);
    return MSAConvPath;
  }
  else 
  {
    i=Alert(T("MSA Converter is a free Windows program to edit disk images and convert them between different formats.")+" "+
      T("It has great features like converting archives containing files into disk images.")+"\r\n\r\n"+
      T("Would you like to open the MSA Converter website now so you can find out more and download it?"),
      T("Run MSA Converter"),MB_ICONQUESTION|MB_YESNO);
    if(i==IDYES)
      ShellExecute(NULL,NULL,MSACONV_WEB,"","",SW_SHOWNORMAL);
  }
  return "";
}


void TDiskManager::AddFileOrFolderContextMenu(HMENU Pop,TDiskManFileInfo *Inf) {
  bool AddProperties=false;
  if(!Inf->UpFolder)
  {
    if(!Inf->BrokenLink)
    {
      if(!Inf->Folder)
      {
        int MultiDisk=0;
        HMENU IAPop=NULL,IBPop=NULL,IRRPop=NULL,StwPop=NULL;
        MenuESL.DeleteAll();
        MenuESL.Sort=eslSortByNameI;
        if(Inf->Zip) 
        {
          zippy.list_contents(Inf->Path,&MenuESL,true);
          if(MenuESL.NumStrings>1) 
          {
            MultiDisk=MF_POPUP;
            IAPop=CreatePopupMenu(),IBPop=CreatePopupMenu(),IRRPop=CreatePopupMenu();
            StwPop=CreatePopupMenu();
            for(int i=0;i<MIN(MenuESL.NumStrings,999);i++)
            {
              InsertMenu(IAPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_INSERTA_MULTI+i,
                MenuESL[i].String);
              InsertMenu(IBPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_INSERTB_MULTI+i,
                MenuESL[i].String);
              InsertMenu(IRRPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_INSERTRUN_MULTI+i,
                MenuESL[i].String);
              InsertMenu(StwPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_TOSTW_MULTI+i,
                MenuESL[i].String);
            }
          }
        }
        AddProperties=true;
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MultiDisk,
          ((MultiDisk==0) ? IDC_INSERTA : (UINT_PTR)IAPop),T("Insert Into Drive &A"));
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MultiDisk,
          ((MultiDisk==0) ? IDC_INSERTB : (UINT_PTR)IBPop),T("Insert Into Drive &B"));
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MultiDisk,
          ((MultiDisk==0) ? IDC_INSERTRUN : (UINT_PTR)IRRPop),T("Insert, Reset and &Run"));
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,999,NULL);
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_CONTENT,
          T("Get &CRC32 and Contents"));
        HMENU ContentsPop=CreatePopupMenu();
        AddFoldersToMenu(ContentsPop,IDC_CONTENT_BASE,"",false);
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_POPUP,
          (UINT_PTR)ContentsPop,T("Get Contents and Create Shortcuts In"));
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,0,NULL);
        if(Inf->LinkPath.NotEmpty()) 
        {
          InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_LINKGOTODISK,T("&Go To Disk"));
          InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_OPENFOLDER,
            T("Open Disk's Folder in Explorer"));
          InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,999,NULL);
        }
        if(!Inf->Zip)
        {
          Inf->ReadOnly=(access(Inf->Path,2)!=0);
          InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(Inf->ReadOnly),
                     IDC_READONLY,T("Read &Only"));
          InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,999,NULL);
        }
        else
        {
          if(MenuESL.NumStrings)
          {    // There are disks in archive
            if(MultiDisk)
              InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_EXTRACT,
                T("E&xtract Disks Here"));
            else
              InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_EXTRACT,
                T("E&xtract Disk Here"));
            InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,999,NULL);
          }
        }
        HMENU MSAPop=CreatePopupMenu();
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_POPUP,(UINT_PTR)MSAPop,"MSA Converter");
        bool FileZip=false;
        if(Inf->Zip) 
        {
          // MSA Converter doesn't work with RAR or 7z files
          if(MenuESL.NumStrings==0&&has_extension(Inf->Path,"zip")) 
            FileZip=true;
        }
        if(FileZip)
          InsertMenu(MSAPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_MSACONVZIPTODI,
            T("Convert to Disk Image"));
        else 
        {
          InsertMenu(MSAPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_MSACONVOPENDI,
            T("Open Disk Image"));
          bool AddedSep=false;
          for(int d=DRIVE_C;d<GEMDOS_MAXDRIVES;d++) 
          {
            if(Stemdos.DriveMounted[d]) 
            {
              if(!AddedSep)
              {
                InsertMenu(MSAPop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,999,NULL);
                AddedSep=true;
              }
              InsertMenu(MSAPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_EXTRACT_HD+d,
                T("Extract Contents to ST Hard Drive")+" "+char('A'+d)+":");
            }
          }
        }
#if defined(SSE_DISK_STW)
/*  To help our MFM disk image format, we add a right click option to convert
    regular images to STW.
    Works with archives too.
*/
        if(!Inf->Folder&&!Inf->UpFolder &&!pasti_active) // pasti_active would interfere in SetDisk
        {
          char *ext=NULL;
          char *dot=strrchr(Inf->Path,'.');
          if(dot) 
            ext=dot+1;
          if(ext&&(IsSameStr_I(ext,DISK_EXT_ST)||IsSameStr_I(ext,DISK_EXT_MSA)
            ||IsSameStr_I(ext,DISK_EXT_DIM)
#if defined(SSE_DISK_STX2STW)
            ||IsSameStr_I(ext,DISK_EXT_STX)
#endif
#if defined(SSE_DISK_SCP2STW)
            ||IsSameStr_I(ext,DISK_EXT_SCP)
#endif
            ||Inf->Zip))
            InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_POPUP,
              ((MultiDisk==0) ? IDC_CONVERTSTW : (UINT_PTR)StwPop),T("Convert to ST&W"));
        }
#endif
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,999,NULL);
      }
      else 
      {
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_EXPLORER,T("Open in &Explorer"));
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_FIND,EasyStr(T("&Find..."))+" \10F3");
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,999,NULL);
      }
    }
    else 
    {
      InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_FIX_SHORTCUT,T("&Fix Shortcut"));
      InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,999,NULL);
    }
    if(Inf->LinkPath.NotEmpty()) 
    {
      HMENU MoveLinkPop=CreatePopupMenu();
      AddFoldersToMenu(MoveLinkPop,IDC_QUICKFOL_BASE+IDC_QUICKFOL_MOVE_LINKPATH*IDC_QUICKFOL_DIV,
        DisksFol,false);
      InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_POPUP,
        (UINT_PTR)MoveLinkPop,T("&Move Shortcut To"));
      HMENU CopyLinkPop=CreatePopupMenu();
      AddFoldersToMenu(CopyLinkPop,IDC_QUICKFOL_BASE+IDC_QUICKFOL_COPY_LINKPATH*IDC_QUICKFOL_DIV,
        DisksFol,false);
      InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_POPUP,
        (UINT_PTR)CopyLinkPop,T("&Copy Shortcut To"));
    }
    EasyStr MoveToText=T("&Move Disk To"),CopyToText=T("&Copy Disk To"),
      ShortcutToText=T("Create &Shortcut To Disk In");
    if(Inf->Folder) 
    {
      MoveToText=T("&Move Folder To"),CopyToText=T("&Copy Folder To"),
        ShortcutToText=T("Create &Shortcut To Folder In");
    }
    Str FolderContainingDisk=Inf->Path;
    RemoveFileNameFromPath(FolderContainingDisk,REMOVE_SLASH);
    HMENU MovePop=CreatePopupMenu();
    AddFoldersToMenu(MovePop,IDC_QUICKFOL_BASE+IDC_QUICKFOL_MOVEPATH*IDC_QUICKFOL_DIV,
      FolderContainingDisk,false);
    InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_POPUP,(UINT_PTR)MovePop,MoveToText);
    HMENU CopyPop=CreatePopupMenu();
    AddFoldersToMenu(CopyPop,IDC_QUICKFOL_BASE+IDC_QUICKFOL_COPYPATH*IDC_QUICKFOL_DIV,
      FolderContainingDisk,false);
    InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_POPUP,(UINT_PTR)CopyPop,CopyToText);
    if(Inf->LinkPath.Empty()) 
    {
      HMENU LinkPop=CreatePopupMenu();
      AddFoldersToMenu(LinkPop,IDC_QUICKFOL_BASE+2*IDC_QUICKFOL_DIV,"",false);
      InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_POPUP,(UINT_PTR)LinkPop,ShortcutToText);
    }
    InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,999,NULL);
    InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_RENAME,EasyStr(T("&Rename"))+" \10F2");
    InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_DELETE,EasyStr(T("Delete"))+" \10DEL");
    InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,999,NULL);
    if(AddProperties)
      InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_PROPERTIES,T("Properties"));
    InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,999,NULL);
  }
}


void TDiskManager::GoToDisk(Str Path,bool bRefresh,bool bFocusView) {
#if !defined(SSE_LIBRETRONUKE)
  EasyStr Fol=Path;
  char *slash=strrchr(Fol,SLASHCHAR);
  if(slash) 
    *slash='\0';
  if(IsSameStr_I(Fol,DisksFol)==0)
    SetDir(Fol,true,Path);
  else if(bRefresh)
    RefreshDiskView(Path);
  else
    SelectItemWithPath(Path);
  if(bFocusView)
    SetFocus(DiskView);
#endif
}


//#pragma warning (disable: 4701) //lvi in case WM_USER//390

#if !defined(SSE_LIBRETRONUKE)

LRESULT CALLBACK TDiskManager::WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar) {
  LRESULT Ret=DefStemDialogProc(Win,Mess,wPar,lPar);
  if(StemDialog_RetDefVal) 
    return Ret;
  TDiskManager *This=(TDiskManager*)GetWindowLongPtr(Win,GWLP_USERDATA);
  WORD wpar_lo=LOWORD(wPar);
  WORD wpar_hi=HIWORD(wPar);
  switch(Mess) {
  case WM_COMMAND:
  {
    switch(wpar_lo) {
    case IDCANCEL: //Esc + internally sent
    {
      if(This->Dragging>-1) 
        break;
      int SelItem=This->GetSelectedItem();
      if(SelItem>-1) 
      {
        TDiskManFileInfo *Inf=This->GetItemInf(SelItem);
        if(Inf->LinkPath.NotEmpty())
          This->RefreshDiskView("",false,Inf->LinkPath);
        else
          This->RefreshDiskView(Inf->Path);
      }
      else
        This->RefreshDiskView();
      break;
    }
    case IDC_HDGEMDOS:  // GEMDOS Hard Drives
    {
      HWND icon_handle=GetDlgItem(Win,wpar_lo);
      if(wpar_hi==BN_CLICKED) // click, left or right
      {
        if(SendMessage(icon_handle,BM_GETCLICKBUTTON,0,0)==BM_CLICKBUTTON_RIGHT)
        { // Right click: options (v4.2)
          SendMessage(icon_handle,BM_SETCHECK,1,0);
          HardDiskMan.Show();
        }
        else
        { //Left click on HD manager icon toggles HD
          HardDiskMan.DisableHardDrives=!HardDiskMan.DisableHardDrives;
          HardDiskMan.update_mount();
          REFRESH_STATUS_BAR_GX;
          SendMessage(icon_handle,BM_SETCHECK,!HardDiskMan.DisableHardDrives,0);
        }
      }
      break;
    }
#if defined(SSE_ACSI_MNGR)
    case IDC_HDACSI: // ACSI Hard Drives
    {
      HWND icon_handle=GetDlgItem(Win,wpar_lo);
      if(wpar_hi==BN_CLICKED) // click, left or right 
      {
        if(SendMessage(icon_handle,BM_GETCLICKBUTTON,0,0)==BM_CLICKBUTTON_RIGHT)
        { //right click opens HD option dialog
          SendMessage(icon_handle,BM_SETCHECK,1,0);
          AcsiHardDiskMan.Show();
        }  
        else
        {
          //Left click on HD manager icon toggles HD
          SSEOptions.Acsi=!SSEOptions.Acsi;
          if(SSEOptions.Acsi)
            load_TOS(ROMFile); // for STE speedboot hack
          REFRESH_STATUS_BAR_GX;
          SendMessage(icon_handle,BM_SETCHECK,SSEOptions.Acsi,0);
        }
      }
      break;
    }
#endif
    case IDC_HOME:  // Go Home
      if(This->Dragging>-1) 
        break;
      if(wpar_hi==BN_CLICKED) 
      {
        bool InHome=IsSameStr_I(This->HomeFol,This->DisksFol);
        if(SendMessage((HWND)lPar,BM_GETCLICKBUTTON,0,0)==BM_CLICKBUTTON_RIGHT||InHome)
        {
          SendMessage((HWND)lPar,BM_SETCHECK,1,0);
          HMENU Pop=CreatePopupMenu();
          This->AddFoldersToMenu(Pop,5000,This->DisksFol,false);
          RECT rc;
          GetWindowRect((HWND)lPar,&rc);
          TrackPopupMenu(Pop,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,
            rc.left,rc.bottom,0,Win,NULL);
          DestroyMenu(Pop);
          SendMessage((HWND)lPar,BM_SETCHECK,0,0);
        }
        else 
          This->SetDir(This->HomeFol,true);
      }
      break;
    case IDC_SET_HOME:  // Set Home
      if(wpar_hi==BN_CLICKED) 
      {
        SendMessage((HWND)lPar,BM_SETCHECK,1,0);
        if(SendMessage((HWND)lPar,BM_GETCLICKBUTTON,0,0)==BM_CLICKBUTTON_RIGHT
          ||IsSameStr_I(This->HomeFol,This->DisksFol)) 
        {
          HMENU Pop=CreatePopupMenu();
          This->AddFoldersToMenu(Pop,8000,This->DisksFol,true);
          RECT rc;
          GetWindowRect((HWND)lPar,&rc);
          TrackPopupMenu(Pop,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,
            rc.left,rc.bottom,0,Win,NULL);
          DestroyMenu(Pop);
        }
        else 
        {
          if(Alert(This->DisksFol+"\n\n"
            +T("Do you want to make this folder your new home folder?"),
            T("Are you sure?"),MB_YESNO|MB_ICONQUESTION)==IDYES)
            This->HomeFol=This->DisksFol;
        }
        SendMessage((HWND)lPar,BM_SETCHECK,0,0);
      }
      break;
    case IDC_BACK:
      if(This->Dragging>-1) 
        break;
      if(wpar_hi==BN_CLICKED) 
      {
        if(SendMessage((HWND)lPar,BM_GETCLICKBUTTON,0,0)==BM_CLICKBUTTON_RIGHT)
        {
          SendMessage((HWND)lPar,BM_SETCHECK,1,0);
          HMENU Pop=CreatePopupMenu();
          for(int n=0;n<DM_HISTORY_LEN;n++) 
          {
            if(This->HistBack[n].NotEmpty()) 
            {
              EasyStr Name=GetFileNameFromPath(This->HistBack[n]);
              if(Name.Empty()) 
                Name=This->HistBack[n];
              InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,5040+n,Name);
            }
          }
          RECT rc;
          GetWindowRect((HWND)lPar,&rc);
          TrackPopupMenu(Pop,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,
            rc.left,rc.bottom,0,Win,NULL);
          DestroyMenu(Pop);
          SendMessage((HWND)lPar,BM_SETCHECK,0,0);
        }
        else 
        {
          for(int n=DM_HISTORY_LEN-1;n>0;n--)
            This->HistForward[n]=This->HistForward[n-1];
          This->HistForward[0]=This->DisksFol;
          This->SetDir(This->HistBack[0],false);
          for(int n=0;n<DM_HISTORY_LEN-1;n++) 
            This->HistBack[n]=This->HistBack[n+1];
          This->HistBack[DM_HISTORY_LEN-1]="";
          EnableWindow(GetDlgItem(Win,IDC_FORWARD),TRUE);
          if(This->HistBack[0].IsEmpty()) 
          {
            if(GetFocus()==(HWND)lPar) 
              SetFocus(GetDlgItem(Win,IDC_FORWARD));
            EnableWindow((HWND)lPar,FALSE);
          }
        }
      }
      break;
    case IDC_FORWARD:
      if(This->Dragging>-1) 
        break;
      if(wpar_hi==BN_CLICKED)
      {
        if(SendMessage((HWND)lPar,BM_GETCLICKBUTTON,0,0)==BM_CLICKBUTTON_RIGHT)
        {
          SendMessage((HWND)lPar,BM_SETCHECK,1,0);
          HMENU Pop=CreatePopupMenu();
          for(int n=0;n<DM_HISTORY_LEN;n++) 
          {
            if(This->HistForward[n].NotEmpty()) 
            {
              EasyStr Name=GetFileNameFromPath(This->HistForward[n]);
              if(Name.Empty()) 
                Name=This->HistForward[n];
              InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,5060+n,Name);
            }
          }
          RECT rc;
          GetWindowRect((HWND)lPar,&rc);
          TrackPopupMenu(Pop,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,
            rc.left,rc.bottom,0,Win,NULL);
          DestroyMenu(Pop);
          SendMessage((HWND)lPar,BM_SETCHECK,0,0);
        }
        else 
        {
          for(int n=DM_HISTORY_LEN-1;n>0;n--)
            This->HistBack[n]=This->HistBack[n-1];
          This->HistBack[0]=This->DisksFol;
          This->SetDir(This->HistForward[0],false);
          for(int n=0;n<DM_HISTORY_LEN-1;n++)
            This->HistForward[n]=This->HistForward[n+1];
          This->HistForward[DM_HISTORY_LEN-1]="";
          EnableWindow(GetDlgItem(Win,IDC_BACK),TRUE);
          if(This->HistForward[0].IsEmpty()) 
          {
            if(GetFocus()==(HWND)lPar)
              SetFocus(GetDlgItem(Win,IDC_BACK));
            EnableWindow((HWND)lPar,FALSE);
          }
        }
      }
      break;
    case IDC_OPTIONS:
      if(This->Dragging>-1) 
        break;
      if(wpar_hi==BN_CLICKED) 
      {
        SendMessage((HWND)lPar,BM_SETCHECK,1,0);
        HMENU Pop=CreatePopupMenu();
        if(OPTION_HACKS)
          InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(This->bTurboDrive),
                     IDC_TURBODRIVE,T("&Turbo Drive"));
        //ALL_SETTINGS_BEGIN
          InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(OPTION_COUNT_DMA_CYCLES),
                     IDC_DMACYCLES,T("Count &DMA transfer cycles"));
        //ALL_SETTINGS_END
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(This->floppy_access_ff),
                   IDC_AUTOFFWD,T("Automatic &fast forward on disk access"));
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(This->bArchiveRW),
                   IDC_ARCHIRW,T("Read/Write &Archives (Changes lost on eject)"));
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(This->bDiskProtectImage),
                   IDC_PROTECTIMAGES,T("&Protect image files"));
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(This->nFloppyDrives>1),
                   IDC_CONNECT_DRIVEB,T("Connect drive &B:"));
#if defined(SSE_DISK_GHOST) && !defined(SSE_GUI_421_REORDER)
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(OPTION_GHOST_DISK),
                   IDC_GHOSTDISK,T("Enable &ghost disks for protected disks"));
        //InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,1999,NULL);
#endif
#if defined(SSE_TOS_PRG_AUTORUN)
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(OPTION_PRG_SUPPORT),
                   IDC_RUNPRG,T("&Run PRG and TOS files"));
        //InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,1999,NULL);
#endif
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(This->AutoInsert2),
                   IDC_AUTOINSERTB,T("Automatically Insert &Second Disk"));
#if defined(SSE_DISK_SWAPPER)
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(This->bSwapperPattern),
                   IDC_PATTERNS,T("Disk S&wapper Patterns"));
#endif
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(This->EjectDisksWhenQuit),
                   IDC_AUTOEJECT,T("E&ject Disks When Quit"));
        //InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,1999,NULL);
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(This->CloseAfterIRR),
                   IDC_AUTOCLOSE,T("&Close Disk Manager After Insert, Reset and Run"));
        //InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,1999,NULL);
#if USE_PASTI
#if defined(SSE_DISK_STX) // if no pasti plugin, offer image protection option
        HMENU PastiPop=CreatePopupMenu();
        if(hPasti) 
        {
          InsertMenu(PastiPop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(pasti_active),
                     IDC_PASTI,T("Use &Pasti for all floppies and ACSI"));
          InsertMenu(PastiPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_PASTICONFIG,
                     T("Pasti &Configuration"));
        }
        else InsertMenu(PastiPop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(This->bDiskProtectImageStx),
                        IDC_PASTI,T("&Protect image files"));
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_POPUP,(UINT_PTR)PastiPop,T("&Pasti"));
#else
        if(hPasti) 
        {
          HMENU PastiPop=CreatePopupMenu();
          InsertMenu(PastiPop,0xffffffff,MF_BYPOSITION|MF_STRING|((pasti_active) ? MF_CHECKED : 0),
            IDC_PASTI,T("Use &Pasti for all floppies and ACSI"));
          InsertMenu(PastiPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_PASTICONFIG,
            T("Pasti &Configuration"));
          InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_POPUP,
            (UINT_PTR)PastiPop,T("&Pasti"));
        }
#endif
#endif
        HMENU MfmPop=CreatePopupMenu();
        InsertMenu(MfmPop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(OPTION_AUTOSTW),
                   IDC_MFMEMU,T("&Auto convert"));
#if defined(SSE_DISK_STW2)
        InsertMenu(MfmPop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(SSEOptions.MfmLowLevel),
                   IDC_MFMLOWLEVEL,T("&Low-level"));
#endif
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_POPUP,(UINT_PTR)MfmPop,T("&MFM emulation"));
#if defined(SSE_DISK_GHOST) && defined(SSE_GUI_421_REORDER)
        HMENU GhostPop=CreatePopupMenu();
        InsertMenu(GhostPop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(OPTION_GHOST_DISK),
                   IDC_GHOSTDISK,T("&Enable ghost disks"));
        InsertMenu(GhostPop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(SSEOptions.GhostDiskRO),
                   IDC_GHOSTDISKRO,T("Also &read-only and zip disks"));
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_POPUP,(UINT_PTR)GhostPop,T("&Ghost disk"));
#endif
        HMENU DCActionPop=CreatePopupMenu();
        InsertMenu(DCActionPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_DBCLICK_NONE,T("&None"));
        InsertMenu(DCActionPop,0xffffffff,MF_BYPOSITION|MF_STRING,
                   IDC_DBCLICK_INSERTA,T("Insert In Drive &A"));
        InsertMenu(DCActionPop,0xffffffff,MF_BYPOSITION|MF_STRING,
                   IDC_DBCLICK_RUN,T("Insert, &Reset and &Run"));
        CheckMenuRadioItem(DCActionPop,IDC_DBCLICK_NONE,IDC_DBCLICK_RUN,
                           IDC_DBCLICK_NONE+This->DoubleClickAction,MF_BYCOMMAND);
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_POPUP,
                   (UINT_PTR)DCActionPop,T("Double Clic&k Disk Action"));
        //InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,1999,NULL);
        HMENU HidePop=CreatePopupMenu();
        InsertMenu(HidePop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(!This->ShowHiddenFiles),
                   IDC_HIDEHIDDEN,T("&Hidden files"));
#if defined(SSE_DISK_SWAPPER)
        InsertMenu(HidePop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(This->HideBroken),
                   IDC_HIDE_ARCHIVE,T("&Non-Disk Archives and Broken Shortcuts"));
#else
        InsertMenu(HidePop,0xffffffff,MF_BYPOSITION|MF_STRING
          |(This->HideBroken ? MF_CHECKED : 0),IDC_HIDE_ARCHIVE,T("Hide &Broken Shortcuts"));
#endif
        InsertMenu(HidePop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(This->HideExtension),
                   IDC_HIDE_EXT,T("&Extension"));
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_POPUP,(UINT_PTR)HidePop,T("&Hide"));
        HMENU IconSizePop=CreatePopupMenu();
#if defined(SSE_GUI_DM_BIGICONS)
        InsertMenu(IconSizePop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_SIZE_SMALL,T("16x16"));
        InsertMenu(IconSizePop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_SIZE_32,T("32x32"));
        InsertMenu(IconSizePop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_SIZE_64,T("64x64"));
        InsertMenu(IconSizePop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_SIZE_128,T("128x128"));
        CheckMenuRadioItem(IconSizePop,IDC_SIZE_SMALL,IDC_SIZE_SMALL+NICONSIZES,
                           IDC_SIZE_SMALL+This->IconSize,MF_BYCOMMAND);
#else
        InsertMenu(IconSizePop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_SIZE_LARGE,T("&Large"));
        InsertMenu(IconSizePop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_SIZE_SMALL,T("&Small"));
        CheckMenuRadioItem(IconSizePop,IDC_SIZE_LARGE,IDC_SIZE_SMALL,
          IDC_SIZE_LARGE+This->SmallIcons,MF_BYCOMMAND);
#endif
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_POPUP,
          (UINT_PTR)IconSizePop,T("Icon Si&ze"));

        HMENU SpacingPop=CreatePopupMenu();
        InsertMenu(SpacingPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_SIZE_THIN,T("&Thin"));
        InsertMenu(SpacingPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_SIZE_NORMAL,T("&Normal"));
        InsertMenu(SpacingPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_SIZE_WIDE,T("&Wide"));        
        CheckMenuRadioItem(SpacingPop,IDC_SIZE_THIN,IDC_SIZE_WIDE,
          IDC_SIZE_THIN+This->IconSpacing,MF_BYCOMMAND);
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_POPUP,
          (UINT_PTR)SpacingPop,T("Icon Spacin&g"));

        HMENU MenuSortBy=CreatePopupMenu();
        InsertMenu(MenuSortBy,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_SORT_NAME,T("&Name"));
        InsertMenu(MenuSortBy,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_SORT_DATE,T("&Date"));
        CheckMenuRadioItem(MenuSortBy,IDC_SORT_NAME,IDC_SORT_DATE,IDC_SORT_NAME+This->SortBy,
          MF_BYCOMMAND);
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_POPUP,
          (UINT_PTR)MenuSortBy,T("Sort b&y"));

        RECT rc;
        GetWindowRect((HWND)lPar,&rc);
        TrackPopupMenu(Pop,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,
          rc.left,rc.bottom,0,Win,NULL);
        DestroyMenu(Pop);
        SendMessage((HWND)lPar,BM_SETCHECK,0,0);
      }
      break;
    case IDC_DISKMANTOOLS: // Disk image management tools
      if(This->Dragging>-1) 
        break;
      if(wpar_hi==BN_CLICKED) 
      {
        SendMessage((HWND)lPar,BM_SETCHECK,1,0);
        HMENU Pop=CreatePopupMenu();
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_DATABASE,
          T("Search Disk Image &Database")+"\10F9");
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,0,NULL);
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_FOLEXPLORER,
          T("Open Current Folder In &Explorer")+"\10F4");
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(This->ExplorerFolders),
                   IDC_PANE,T("&Folders Pane in Explorer"));
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_FINDCUR,T("Find In &Current Folder"));
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_MSACONV,T("Run &MSA Converter")+"\10F6");
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,0,NULL);
#ifndef SSE_NO_WINSTON_IMPORT
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,2001,T("Import &WinSTon Favourites"));
#endif
        RECT rc;
        GetWindowRect((HWND)lPar,&rc);
        TrackPopupMenu(Pop,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,rc.left,rc.bottom,0,Win,NULL);
        DestroyMenu(Pop);
        SendMessage((HWND)lPar,BM_SETCHECK,0,0);
      }
      break;
    case IDOK:  //Return
      if(This->Dragging>-1) 
        break;
      if(GetFocus()==This->DiskView)
        PostMessage(Win,WM_USER,1234,0);
      break;
    case IDC_PCDRIVE:  //Drive Combo
      if(This->Dragging>-1) 
        break;
      if(wpar_hi==CBN_SELENDOK) 
      {
        char Text[8];
        SendMessage((HWND)lPar,CB_GETLBTEXT,SendMessage((HWND)lPar,CB_GETCURSEL,0,0),(LPARAM)Text);
        if(Text[0]!=This->DisksFol[0]) 
        {
          This->SetDir(Text,true);
          if(Text[0]!=This->DisksFol[0]) 
          {
            int idx=(int)SendMessage((HWND)lPar,CB_FINDSTRING,0xffffffff,
              (LPARAM)((This->DisksFol.Lefts(2)+SLASH).Text));
            if(idx>-1) 
              SendMessage((HWND)lPar,CB_SETCURSEL,idx,0);
          }
        }
      }
      break;
    case IDC_NEWFOLDER:
    {
      EasyStr FolName=This->DisksFol+SLASH+T("New Folder");
      int n=2;
      while(GetFileAttributes(FolName)!=INVALID_FILE_ATTRIBUTES)
        FolName=This->DisksFol+SLASH+T("New Folder")+" ("+(n++)+")";
      CreateDirectory(FolName,NULL);
      This->RefreshDiskView(FolName,true);
      break;
    }
    case IDC_NEWST:
    case IDC_NEWHDST: // HD ST disk image, 18 sectors instead of 9
    {
      EasyStr STName=This->DisksFol+SLASH+T("Blank Disk")+".st";
      int n=2;
      while(Exists(STName))
        STName=This->DisksFol+SLASH+T("Blank Disk")+" ("+(n++)+").st";
      WORD sectors=(wpar_lo==IDC_NEWHDST) ? 18 : 9;
      if(This->CreateDiskImage(STName,sectors*2*80,sectors,2))
        This->RefreshDiskView(STName,true);
      else
        Alert(EasyStr(T("Could not create the disk image "))+STName,
          T("ERROR"),MB_ICONEXCLAMATION);
      return 0;
    }
    case IDC_NEWCUSTOM:  // Custom disk image
      This->ShowDiskDiag();
      break;
#if defined(SSE_DISK_STW) || defined(SSE_DISK_HFE)
    case IDC_NEWSTW: // New STW Disk Image
    case IDC_NEWHFE: // New HFE Disk Image
    case IDC_NEWHDSTW: // New HD STW Disk Image
    {
      char *extension=(wPar==IDC_NEWHFE)?DISK_EXT_HFE:DISK_EXT_STW;
      EasyStr STName=This->DisksFol+SLASH+extension+" Disk."+extension;
      int n=2;
      while(Exists(STName))
        STName=This->DisksFol+SLASH+extension+" Disk ("+(n++)+")."+extension;
      // still ugly C++

      if(0
#if defined(SSE_DISK_STW)        
        ||(wPar==IDC_NEWSTW) && ImageSTW[2].Create(STName,2) // DD
        ||(wPar==IDC_NEWHDSTW) && ImageSTW[2].Create(STName,4) // HD
#endif
#if defined(SSE_DISK_HFE)
        ||(wPar==IDC_NEWHFE) && ImageHFE[2].Create(STName)
#endif
        )
      {
        This->RefreshDiskView(STName,true);
      }
      else
        Alert(EasyStr(T("Could not create the disk image "))+STName,
          T("ERROR"),MB_ICONEXCLAMATION);
      return 0;
    }
#endif
    case IDC_REFRESH:
      PostMessage(Win,WM_COMMAND,IDCANCEL,0);
      break;
    case IDC_BACKGROUND:
      This->mBackground++;
      This->AdaptBackground();
      This->RefreshDiskView(""); // can be slow
      break;
    case IDC_INSERTA:case IDC_INSERTB:case IDC_INSERTRUN:
    {
      TDiskManFileInfo *Inf=This->GetItemInf(This->MenuTarget);
      if(Inf)
        This->PerformInsertAction(wpar_lo-IDC_INSERTA,Inf->Name,Inf->Path,"");
      break;
    }
    case IDC_RENAME:
      SendMessage(This->DiskView,LVM_EDITLABEL,This->MenuTarget,0);
      break;
    case IDC_DELETE:
      PostMessage(Win,WM_USER,1234,2);
      break;
    // drive icon context menu
    case IDC_FILESELEC:
    case (IDC_FILESELEC+1):
    {
      char *fstypes=FSTypes(2,NULL);
      EasyStr path=FileSelect(NULL,T("Select Disk Image"),DiskMan.DisksFol,fstypes,1,true,"");
      free(fstypes);
      EasyStr name=GetFileNameFromPath(path);
      DiskMan.InsertDisk((wpar_lo-IDC_FILESELEC),name,path,false,false,"",true); // can be ""
      break;
    }
#if defined(SSE_DRIVE_SOUND)
    case IDC_SOUNDDIR:
    case (IDC_SOUNDDIR+1):
    {
      int drive=(wpar_lo-IDC_SOUNDDIR);
      EasyStr NewFol=ChooseFolder((FullScreen) ? StemWin : Win,
        T("Pick a Folder"),DriveSoundDir[drive]);
      if(NewFol.NotEmpty()&&NotSameStr_I(NewFol,DriveSoundDir[drive]))
      {
        NO_SLASH(NewFol);
        DriveSoundDir[drive]=NewFol;
      }
      break;
    }
#endif
    case IDC_STOPMOTOR:
    case (IDC_STOPMOTOR+1): // hack, some programs leave the motor on
      FloppyDrive[(wpar_lo-IDC_STOPMOTOR)].bMotor=false;
      Fdc.str&=~FDC_STR_MO;
      agenda_delete(agenda_fdc_motor_flag_off);
      break;
#if defined(SSE_DRIVE_SINGLESIDE)
    case IDC_SINGLESIDE:
    case (IDC_SINGLESIDE+1):
      FloppyDrive[wpar_lo-IDC_SINGLESIDE].bSingleSided
        =!FloppyDrive[wpar_lo-IDC_SINGLESIDE].bSingleSided;
      InvalidateRect(GetDlgItem(This->Handle,(wpar_lo-IDC_DRIVEA)),NULL,FALSE);
      break;
#endif
#if defined(SSE_DRIVE_FREEBOOT)
    case IDC_FREEBOOT:
    case (IDC_FREEBOOT+1):
      FloppyDrive[wpar_lo-IDC_FREEBOOT].bFreeboot=!FloppyDrive[wpar_lo-IDC_FREEBOOT].bFreeboot;
      InvalidateRect(GetDlgItem(This->Handle,(wpar_lo-IDC_DRIVEA)),NULL,FALSE);
      Psg.CheckFreeboot();
      break;
#endif
    case IDC_READONLYA:  // Disk in Drive 1
    case IDC_READONLYB:  // Disk in Drive 2
      This->DragLV=GetDlgItem(Win,(wpar_lo==IDC_READONLYA?IDC_CONTENTA:IDC_CONTENTB));
      // no break
    case IDC_READONLY:  // Toggle Read-Only
    {
      bool FromDV=false;
      if(wpar_lo==IDC_READONLY) 
      {
        This->DragLV=This->DiskView;
        FromDV=true;
      }
      LV_ITEM lvi;
      lvi.iItem=int(FromDV?This->MenuTarget:0);
      lvi.iSubItem=0;
      lvi.mask=LVIF_PARAM;
      SendMessage(This->DragLV,LVM_GETITEM,0,(LPARAM)&lvi);
      TDiskManFileInfo *Inf=(TDiskManFileInfo*)lvi.lParam;
      EasyStr DiskPath=Inf->Path;
      bool InDrive[2]={false,false};
      EasyStr OldName[2],DiskInZip[2];
      for(int d=DRIVE_A;d<=DRIVE_B;d++) 
      {
        if(IsSameStr_I(FloppyDrive[d].GetDisk(),DiskPath)) 
        {
          InDrive[d]=true;
          OldName[d]=FloppyDisk[d].DiskName;
          DiskInZip[d]=FloppyDisk[d].DiskInZip;
          FloppyDrive[d].RemoveDisk();
        }
      }
      DWORD Attrib=GetFileAttributes(DiskPath);
      if(Inf->ReadOnly)
        SetFileAttributes(DiskPath,Attrib & ~FILE_ATTRIBUTE_READONLY);
      else
        SetFileAttributes(DiskPath,Attrib|FILE_ATTRIBUTE_READONLY);
      Inf->ReadOnly=((GetFileAttributes(DiskPath)&FILE_ATTRIBUTE_READONLY)!=0); // Just in case of failure
      int c=(int)SendMessage(This->DiskView,LVM_GETITEMCOUNT,0,0);
      if(!FromDV || Inf->LinkPath.NotEmpty())
      {
        for(lvi.iItem=1;lvi.iItem<c;lvi.iItem++)
        {
          lvi.mask=LVIF_PARAM;
          SendMessage(This->DiskView,LVM_GETITEM,0,(LPARAM)&lvi);
          if(((TDiskManFileInfo*)lvi.lParam)->LinkPath.IsEmpty())
          {
            if(IsSameStr_I(((TDiskManFileInfo*)lvi.lParam)->Path,DiskPath)) 
            {
              ((TDiskManFileInfo*)lvi.lParam)->ReadOnly=Inf->ReadOnly;
              break;
            }
          }
        }
      }
      if(lvi.iItem<c) 
      {
        lvi.mask=LVIF_IMAGE;
        lvi.iImage=1+Inf->ReadOnly*4;
        SendMessage(This->DiskView,LVM_SETITEM,0,(LPARAM)&lvi);
      }
      if(InDrive[DRIVE_A])
        This->InsertDisk(DRIVE_A,OldName[DRIVE_A],DiskPath,false,false,DiskInZip[DRIVE_A]);
      if(InDrive[DRIVE_B])
        This->InsertDisk(DRIVE_B,OldName[DRIVE_B],DiskPath,false,false,DiskInZip[DRIVE_B]);
      SetFocus(This->DragLV);
      break;
    }
    case IDC_WRITEPROTECT:
    case (IDC_WRITEPROTECT+1):
    {
      WORD drive=wpar_lo-IDC_WRITEPROTECT;
      FloppyDisk[drive].WriteProtect=!FloppyDisk[drive].WriteProtect;
      FloppyDrive[drive].ReinsertDisk();
      break;
    }
    case IDC_PROTECTIMAGES:
      This->bDiskProtectImage=!This->bDiskProtectImage;
      if(This->bDiskProtectImage)
        FloppyDisk[DRIVE_A].WrittenTo=FloppyDisk[DRIVE_B].WrittenTo=false;
      break;
    case IDC_EXPLORER:
    case IDC_FIND:                                                                           
    {
      TDiskManFileInfo *Inf=This->GetItemInf(This->MenuTarget);
      if(Inf) ShellExecute(NULL,LPSTR((wpar_lo==IDC_FIND) ? "Find" : 
        (LPSTR)(This->ExplorerFolders ? "explore" : NULL)),Inf->Path,"","",SW_SHOWNORMAL);
      break;
    }
    case IDC_FIX_SHORTCUT:
    {
      TDiskManFileInfo *Inf=This->GetItemInf(This->MenuTarget);
      WIN32_FIND_DATA wfd;
      EasyStr NewPath=GetLinkDest(Inf->LinkPath,&wfd,(FullScreen) ? StemWin : This->Handle);
      if(NewPath.NotEmpty()) 
      {
        if(GetFileAttributes(NewPath)!=INVALID_FILE_ATTRIBUTES)
        {
          Inf->Path=NewPath;
          Inf->BrokenLink=false;
          LV_ITEM lvi;
          lvi.iItem=This->MenuTarget;
          lvi.iSubItem=0;
          lvi.mask=LVIF_IMAGE;
          lvi.iImage=(Inf->Folder) ? 3 : 4;
          SendMessage(This->DiskView,LVM_SETITEM,0,(LPARAM)&lvi);
        }
      }
      break;
    }
    case IDC_EXTRACT:  //Extract
    {
      TDiskManFileInfo *Inf=This->GetItemInf(This->MenuTarget);
      This->ExtractDisks(Inf->Path);
      break;
    }
    case IDC_COPYCLP:
    {//Put the name of the game in the clipboard when user clicks on it.
      TDiskManFileInfo *Inf=This->GetItemInf(0,(HWND)GetDlgItem(Win,
        IDC_CONTENTA+This->MenuTarget));
      SetClipboardText(Inf->Name.Text); // in acc.cpp
      break;
    }
    case IDC_LINKGOTODISK:  // Go to disk
    case IDC_GOTODISK:  // Go to disk in drive
    {
#if defined(SSE_420R5) //readability
      TDiskManFileInfo *Inf;
      if(wpar_lo==IDC_LINKGOTODISK)
        Inf=This->GetItemInf(This->MenuTarget,This->DiskView);
      else
        Inf=This->GetItemInf(0,GetDlgItem(Win,IDC_CONTENTA+This->MenuTarget));
#else
      TDiskManFileInfo *Inf=This->GetItemInf((wpar_lo==IDC_LINKGOTODISK) ?
        This->MenuTarget : 0,(wpar_lo==IDC_LINKGOTODISK) ?
        This->DiskView : GetDlgItem(Win,IDC_CONTENTA+This->MenuTarget));
#endif
      This->GoToDisk(Inf->Path,false);
      break;
    }
    case IDC_OPENFOLDER: // Open Disk's Folder in Explorer
    {
      TDiskManFileInfo *Inf=This->GetItemInf(This->MenuTarget);
      if(Inf) 
      {
        Str Fol=Inf->Path;
        RemoveFileNameFromPath(Fol,REMOVE_SLASH);
        ShellExecute(NULL,LPSTR(This->ExplorerFolders ? "explore" : NULL),
          Fol,"","",SW_SHOWNORMAL);
      }
      break;
    }
    // can make case 1093 for copy-to-clipboard?
    case IDC_PROPERTIES:
    {
      TDiskManFileInfo *Inf=This->GetItemInf(This->MenuTarget);
      if(Inf)
      {
        This->PropInf=*Inf;
        This->ShowPropDiag();
      }
      break;
    }
    case IDC_SWAP: // Swap disks
      This->SwapDisks(This->MenuTarget);
      break;
    case IDC_EJECT:
      if(SendMessage(GetDlgItem(Win,IDC_CONTENTA+This->MenuTarget),LVM_GETITEMCOUNT,0,0))
        This->EjectDisk(!!This->MenuTarget);
      break;
    case IDC_EJECTNOSAVE:
      if(SendMessage(GetDlgItem(Win,IDC_CONTENTA+This->MenuTarget),LVM_GETITEMCOUNT,0,0))
        This->EjectDisk(!!This->MenuTarget,true); // lose changes
      break;
#if defined(SSE_DISK_SWAPPER)
    case IDC_PREVIOUS: // Previous disk
      This->ChangeDisk(This->MenuTarget,-1,FALSE);
      break;
    case IDC_NEXT: // Next disk
      This->ChangeDisk(This->MenuTarget,1,FALSE);
      break;
#endif
    case IDC_HIDE_ARCHIVE:
      This->HideBroken=!This->HideBroken;
      PostMessage(Win,WM_COMMAND,IDCANCEL,0);
      break;
    case IDC_HIDEHIDDEN:
      This->ShowHiddenFiles=!This->ShowHiddenFiles;
      PostMessage(Win,WM_COMMAND,IDCANCEL,0);
      break;
    case IDC_HIDE_EXT:
      This->HideExtension=!This->HideExtension;
      PostMessage(Win,WM_COMMAND,IDCANCEL,0);
      break;
    case IDC_FOLEXPLORER:
      ShellExecute(NULL,LPSTR(This->ExplorerFolders ? "explore" : NULL),
        This->DisksFol,"","",SW_SHOWNORMAL);
      break;
    case IDC_PANE:
      This->ExplorerFolders=!This->ExplorerFolders;
      break;
#if defined(SSE_GUI_DM_BIGICONS)
    case IDC_SIZE_SMALL:
    case IDC_SIZE_32:
    case IDC_SIZE_64:
    case IDC_SIZE_128:
      This->IconSize=(char)(wpar_lo-IDC_SIZE_SMALL);
      if(This->IconSize>0)
      {
        SendMessage(This->DiskView,WM_SETREDRAW,0,0);
        This->SetDiskViewMode(LVS_ICON);
      }
      This->RefreshDiskView();
      break;
#else
    case IDC_SIZE_LARGE:
      if(This->SmallIcons) 
      {
        This->SmallIcons=false;
        SendMessage(This->DiskView,WM_SETREDRAW,0,0);
        This->SetDiskViewMode(LVS_ICON);
        This->RefreshDiskView();
      }
      break;
    case IDC_SIZE_SMALL:
      if(!This->SmallIcons)
      {
        This->SmallIcons=true;
        This->RefreshDiskView();
      }
      break;
#endif
    case IDC_DBCLICK_NONE:case IDC_DBCLICK_INSERTA:case IDC_DBCLICK_RUN:
      This->DoubleClickAction=wpar_lo-IDC_DBCLICK_NONE;
      break;
#ifndef SSE_NO_WINSTON_IMPORT
    case 2001:  // Import
      This->ShowImportDiag();
      break;
#endif
    case IDC_FINDCUR:
      ShellExecute(NULL,"Find",This->DisksFol,"","",SW_SHOWNORMAL);
      break;
    case IDC_AUTOEJECT:
      This->EjectDisksWhenQuit=!This->EjectDisksWhenQuit;
      break;
    case IDC_CONNECT_DRIVEB:
      SendDlgItemMessage(Win,IDC_DRIVEB,WM_LBUTTONDOWN,0,0);
      break;
    case IDC_TURBODRIVE:
      This->bTurboDrive=!This->bTurboDrive;
      CheckResetDisplay();
      FloppyDrive[DRIVE_A].UpdateAdat(false);
      FloppyDrive[DRIVE_B].UpdateAdat(true);
      break;
    case IDC_ARCHIRW:
      This->bArchiveRW=!This->bArchiveRW;
      This->LoadIcons();
      for(int drv=DRIVE_A;drv<=DRIVE_B;drv++)
      {
        if(FloppyDrive[drv].DiskInDrive())
        {
          if(FloppyDisk[drv].IsZip())
            FloppyDisk[drv].ReadOnly=!This->bArchiveRW;
        }
      }
      break;
    case IDC_AUTOCLOSE:
      This->CloseAfterIRR=!This->CloseAfterIRR;
      break;
    case IDC_AUTOINSERTB:
      This->AutoInsert2=!This->AutoInsert2;
      break;
#if defined(SSE_DISK_SWAPPER)
    case IDC_PATTERNS:
      This->bSwapperPattern=!This->bSwapperPattern;
      break;
#endif
    case IDC_AUTOFFWD:
      This->floppy_access_ff=!This->floppy_access_ff;
      break;
    case IDC_DMACYCLES:
      OPTION_COUNT_DMA_CYCLES=!OPTION_COUNT_DMA_CYCLES;
      break;
    case IDC_SIZE_THIN:case IDC_SIZE_NORMAL:case IDC_SIZE_WIDE:
      if(This->IconSpacing!=(wpar_lo-IDC_SIZE_THIN)) 
      {
        This->IconSpacing=wpar_lo-IDC_SIZE_THIN;
#if defined(SSE_GUI_DM_BIGICONS)
        This->SetDiskViewMode((This->IconSize==0) ? LVS_SMALLVIEW : LVS_ICON);
#else
        This->SetDiskViewMode((This->SmallIcons) ? LVS_SMALLVIEW : LVS_ICON);
#endif
        This->RefreshDiskView();
      }
      break;
    case IDC_SORT_NAME:case IDC_SORT_DATE:
      if(This->SortBy!=(wpar_lo-IDC_SORT_NAME)) 
      {
        This->SortBy=wpar_lo-IDC_SORT_NAME;
        This->RefreshDiskView();
      }
      break;

#if USE_PASTI
    case IDC_PASTI:case IDC_PASTICONFIG:
      if(hPasti==NULL)
      {
#if defined(SSE_DISK_STX)
        This->bDiskProtectImageStx=!This->bDiskProtectImageStx;
#endif
        break;
      }
      if(wpar_lo==IDC_PASTICONFIG) // Pasti configuration
        pasti->DlgConfig((FullScreen) ? StemWin : This->Handle,0,NULL);
      if(wpar_lo==IDC_PASTI) 
      { // option use pasti
        // no reset, just reinsert
        pasti_active=!pasti_active;
        //TRACE_LOG("pasti_active %d\n",pasti_active);
        EnableWindow(GetDlgItem(This->Handle,IDC_HDACSI),!pasti_active);
        for(WORD i=DRIVE_A;i<=DRIVE_B;i++)
        {
          InvalidateRect(GetDlgItem(This->Handle,IDC_DRIVEA+i),NULL,FALSE);
          if(FloppyDrive[i].NotEmpty())
          {
            EasyStr name=FloppyDisk[i].DiskName;
            EasyStr path=FloppyDisk[i].GetImageFile();
            This->EjectDisk(!!i);
            This->InsertDisk(i,name,path,false,false,"",true);
          }
          else FloppyDrive[i].ImageType.Manager=(BYTE)((pasti_active)
            ? MNGR_PASTI : (OPTION_AUTOSTW ? MNGR_WD1772 : MNGR_STEEM));
        }
        This->RefreshDiskView();
      }//if(wpar_lo==IDC_PASTI
      break;
#endif//pasti
#if defined(SSE_DISK_AUTOSTW)
    case IDC_MFMEMU:
      for(int i=DRIVE_A;i<=DRIVE_B;i++) // reinsert disks, like for Pasti
      {
        if(FloppyDrive[i].NotEmpty())
        {
          EasyStr name=FloppyDisk[i].DiskName;
          EasyStr path=FloppyDisk[i].GetImageFile();
          This->EjectDisk(!!i);
          OPTION_AUTOSTW=!OPTION_AUTOSTW;
          This->InsertDisk(i,name,path,false,false,"",true);
          OPTION_AUTOSTW=!OPTION_AUTOSTW;
        }
        else FloppyDrive[i].ImageType.Manager=(BYTE)((pasti_active)
          ? MNGR_PASTI : (OPTION_AUTOSTW ? MNGR_WD1772 : MNGR_STEEM));
      }
      OPTION_AUTOSTW=!OPTION_AUTOSTW;
      break;
#endif
#if defined(SSE_DISK_STW2)
    case IDC_MFMLOWLEVEL:
      SSEOptions.MfmLowLevel=!SSEOptions.MfmLowLevel;
      for(int i=DRIVE_A;i<=DRIVE_B;i++) // change for current disks
      {
        if(FloppyDrive[i].NotEmpty())
        {
          if(FloppyDrive[i].ImageType.Manager==MNGR_WD1772&&FloppyDrive[i].MfmManager)
            FloppyDrive[i].MfmManager->LowLevel=SSEOptions.MfmLowLevel;
        }
      }
      break;
#endif
    case IDC_DATABASE:
      This->ShowDatabaseDiag();
      break;
#if defined(SSE_DISK_GHOST)
    case IDC_GHOSTDISK:
      OPTION_GHOST_DISK=!OPTION_GHOST_DISK;
      break;
#if defined(SSE_GUI_421_REORDER)
    case IDC_GHOSTDISKRO:
      SSEOptions.GhostDiskRO=!SSEOptions.GhostDiskRO;
      break;
#endif
#endif
#if defined(SSE_TOS_PRG_AUTORUN)
    case IDC_RUNPRG:
      OPTION_PRG_SUPPORT=!OPTION_PRG_SUPPORT;
      This->RefreshDiskView();
      break;
#endif
    }
    if(wpar_lo>=4000&&wpar_lo<5000)
      This->MenuTarget=wpar_lo;
    else if(wpar_lo>=5000&&wpar_lo<5080) 
    {
      if(wpar_lo>=5060) 
      {
        int nGoForward=(wpar_lo-5060)+1;
        for(int i=0;i<nGoForward;i++)
          for(int n=DM_HISTORY_LEN-1;n>0;n--)
            This->HistBack[n]=This->HistBack[n-1];
        int BackIdx=0,ForIdx=nGoForward-2;
        for(int n=0;n<nGoForward-1;n++) 
          This->HistBack[BackIdx++]=This->HistForward[ForIdx--];
        This->HistBack[BackIdx]=This->DisksFol;
        This->SetDir(This->HistForward[nGoForward-1],false);
        for(int i=0;i<nGoForward;i++) 
        {
          for(int n=0;n<DM_HISTORY_LEN-1;n++)
            This->HistForward[n]=This->HistForward[n+1];
          This->HistForward[DM_HISTORY_LEN-1]="";
        }
        EnableWindow(GetDlgItem(Win,IDC_BACK),TRUE);
        if(This->HistForward[0].IsEmpty()) 
        {
          if(GetFocus()==GetDlgItem(Win,IDC_FORWARD)) 
            SetFocus(GetDlgItem(Win,IDC_BACK));
          EnableWindow(GetDlgItem(Win,IDC_FORWARD),FALSE);
        }
      }
      else if(wpar_lo>=5040) 
      {
        int nGoBack=(wpar_lo-5040)+1;
        for(int i=0;i<nGoBack;i++)
          for(int n=DM_HISTORY_LEN-1;n>0;n--)
            This->HistForward[n]=This->HistForward[n-1];
        int ForIdx=0,BackIdx=nGoBack-2;
        for(int n=0;n<nGoBack-1;n++) 
          This->HistForward[ForIdx++]=This->HistBack[BackIdx--];
        This->HistForward[ForIdx]=This->DisksFol;
        This->SetDir(This->HistBack[nGoBack-1],false);
        for(int i=0;i<nGoBack;i++) 
        {
          for(int n=0;n<DM_HISTORY_LEN-1;n++)
            This->HistBack[n]=This->HistBack[n+1];
          This->HistBack[DM_HISTORY_LEN-1]="";
        }
        HWND hFwd=GetDlgItem(Win,IDC_FORWARD);
        EnableWindow(hFwd,TRUE);
        if(This->HistBack[0].IsEmpty())
        {
          HWND hBck=GetDlgItem(Win,IDC_BACK);
          if(GetFocus()==hBck) 
            SetFocus(hFwd);
          EnableWindow(hBck,FALSE);
        }
      }
      else 
      { // Go to quick folder
        if(wpar_lo==5000)
          This->SetDir(This->HomeFol,true);
        else 
        {
          if(This->QuickFol[wpar_lo-5001].NotEmpty())
            This->SetDir(This->QuickFol[wpar_lo-5001],true);
        }
      }
    }
    if(wpar_lo>=8000&&wpar_lo<8100) 
    {  // Change/erase quick folder
      if(wpar_lo==8000) 
      { // accidental click likely
        if(Alert(This->DisksFol+"\n\n"
          +T("Do you want to make this folder your new home folder?"),
          T("Are you sure?"),MB_YESNO|MB_ICONQUESTION)==IDYES)
          This->HomeFol=This->DisksFol;
      }
      else 
      {
        int n=(wpar_lo-8005)/5;
        int Action=(wpar_lo-8005)%5;
        switch(Action) {
        case 0:
          This->QuickFol[n]=This->DisksFol;
          break;
        case 1:
        {
          EnableAllWindows(false,Win);
          // SS: this allows player to select a network folder as well
          // (home network when it was supported by Windows)
          EasyStr NewFol=ChooseFolder((FullScreen) ? StemWin : Win,
            T("Pick a Folder"),This->DisksFol);
          if(NewFol.NotEmpty())
            This->QuickFol[n]=NewFol;
          SetForegroundWindow(Win);
          EnableAllWindows(true,Win);
          break;
        }
        case 2:
          This->QuickFol[n]="";
          break;
        }
      }
    }
    else if(wpar_lo>=IDC_QUICKFOL_BASE&&wpar_lo<IDC_QUICKFOL_BASE+1000) 
    {
      int SelItem=This->GetSelectedItem();
      if(SelItem>-1) 
      {
        TDiskManFileInfo *Inf=This->GetItemInf(SelItem);
        int Action=(wpar_lo-IDC_QUICKFOL_BASE)/IDC_QUICKFOL_DIV;
        EasyStr SrcFol,DestFol,To;
#if defined(SSE_LONG_PATH)
        EasyStr sFrom;
        sFrom.SetLength(SSE_MAX_PATH);
        char *From=sFrom.Text;
        ZeroMemory(From,SSE_MAX_PATH);
#else
        char From[MAX_PATH+2];
        ZeroMemory(From,sizeof(From));
#endif
        if((wpar_lo%IDC_QUICKFOL_DIV)==0)
          DestFol=This->HomeFol;
        else
          DestFol=This->QuickFol[(wpar_lo%IDC_QUICKFOL_DIV)-1];
        To=DestFol+SLASH+GetFileNameFromPath((Action<=IDC_QUICKFOL_COPYPATH) 
          ? Inf->Path : Inf->LinkPath);
        bool bMoving=false;
        switch(Action) {
        case IDC_QUICKFOL_MOVEPATH: //0
        case IDC_QUICKFOL_MOVE_LINKPATH: //3
          bMoving=true;
        case IDC_QUICKFOL_COPYPATH: //1
        case IDC_QUICKFOL_COPY_LINKPATH: //4
          strcpy(From,(Action<=IDC_QUICKFOL_COPYPATH)?Inf->Path:Inf->LinkPath);
          SrcFol=From;
          RemoveFileNameFromPath(SrcFol,REMOVE_SLASH);
          if(NotSameStr_I(SrcFol,DestFol)||bMoving)
          {
            This->MoveOrCopyFile(bMoving,From,To,(Action==IDC_QUICKFOL_MOVEPATH) 
              ? Inf->Path : Str(),IsSameStr_I(SrcFol,DestFol));
            bool Refresh=false;
            if(Action==IDC_QUICKFOL_MOVEPATH && Inf->LinkPath.NotEmpty()) 
            {
              CreateLink(Inf->LinkPath,To);
              Inf->Path=To;
            }
            if(bMoving && IsSameStr_I(SrcFol,This->DisksFol))
              Refresh=true;
            if(IsSameStr_I(DestFol,This->DisksFol))
              Refresh=true;
            if(Refresh)
              This->RefreshDiskView("",false,"",SelItem);
          }
          break;
        case IDC_QUICKFOL_LINK_TO_PATH:  //2
        {
          EasyStr LinkName=DestFol+SLASH+Inf->Name+".lnk";
          int n=2;
          while(Exists(LinkName))
            LinkName=DestFol+SLASH+Inf->Name+" ("+(n++)+").lnk";
          CreateLink(LinkName,Inf->Path);
          if(IsSameStr_I(DestFol,This->DisksFol)) 
            This->RefreshDiskView("",true,LinkName);
          break;
        }
        }
      }
    }
    else if((wpar_lo>=IDC_CONTENT_BASE&&wpar_lo<IDC_CONTENT_BASE+20)||wpar_lo==IDC_CONTENT) 
    {
      // Get contents [and create shortcuts in DestFol]
      Str DestFol;
      if(wpar_lo!=IDC_CONTENT) 
      {
        if((wpar_lo%IDC_QUICKFOL_DIV)==0)
          DestFol=This->HomeFol;
        else
          DestFol=This->QuickFol[(wpar_lo%IDC_QUICKFOL_DIV)-1];
      }
      TDiskManFileInfo *Inf=This->GetItemInf(This->MenuTarget);
      This->GetContentsSL(Inf->Path);
      if(This->contents_sl.NumStrings) 
      {
        if(DestFol.NotEmpty()) 
        {
          // 0 = path, 1 = disk name, 2+ = contents
          Str ShortName=This->contents_sl[1].String;
          Str SelLink;
          int start_i=2;
          if(This->contents_sl.NumStrings==1) 
            start_i=1;
          for(int i=start_i;i<This->contents_sl.NumStrings;i++) 
          {
            SelLink=GetUniquePath(DestFol,Str(This->contents_sl[i].String)
              +" ("+ShortName+").lnk");
            CreateLink(SelLink,Inf->Path);
          }
          if(SelLink.NotEmpty()&&IsSameStr_I(DestFol,This->DisksFol))
            This->RefreshDiskView("",false,SelLink);
        }
        else 
        {
          This->ContentsLinksPath=This->DisksFol;
          This->ShowContentDiag();
        }
      }
    }
    else if(wpar_lo>=IDC_TOSTW_MULTI&&wpar_lo<(IDC_TOSTW_MULTI+200)||wpar_lo==IDC_CONVERTSTW) // 9600-9799
    {
      TDiskManFileInfo *Inf=This->GetItemInf(This->MenuTarget);
      if(Inf)
      {
#if defined(SSE_LONG_PATH)
        EasyStr sDest;
        sDest.SetLength(SSE_MAX_PATH);
        char *dest=sDest.Text;
#else
        char dest[MAX_PATH];
#endif
        strcpy(dest,Inf->Path.Text);
        char *CompressedDiskName="";
        if(wpar_lo!=IDC_CONVERTSTW)
        {
          CompressedDiskName=This->MenuESL[(wpar_lo-IDC_TOSTW_MULTI)].String;
          RemoveFileNameFromPath(dest,KEEP_SLASH);
          strcat(dest,CompressedDiskName);
        }
        char *dot=strrchr(dest,'.');
        if(dot)
          strncpy(dot+1,"STW",3);
        bool save1=OPTION_AUTOSTW;
        bool save2=FloppyDisk[2].ReadOnly;
        FloppyDisk[2].ReadOnly=false;
        OPTION_AUTOSTW=false; // don't want to autoconvert it!
        int Err=FloppyDrive[2].SetDisk(Inf->Path,CompressedDiskName);
        if(Err==ERR_OK)
        {
          if(FloppyDrive[2].ImageType.Manager==MNGR_STEEM) //DIM, MSA or ST
          {
            if(FloppyDrive[2].ImageType.Extension==EXT_STX)
            {
#if defined(SSE_DISK_STX2STW)
              char *src=FloppyDisk[2].ZipTempFile.Text;
              STXtoSTW(src,dest);
#endif
            }
            else
            {
#if defined(SSE_DISK_STW)
              STtoSTW(2,dest);
#endif
            }
            FloppyDisk[2].WrittenTo=true;
          }
#if defined(SSE_DISK_STX2STW) && USE_PASTI
          else if(FloppyDrive[2].ImageType.Extension==EXT_STX)
          { // this will work only for WIN32
            char* src= (FloppyDisk[2].IsZip())?FloppyDisk[2].ZipTempFile.Text:Inf->Path.Text;
            STXtoSTW(src,dest);
            FloppyDisk[2].WrittenTo=true;
          }
#endif
#if defined(SSE_DISK_SCP2STW)
          else if(FloppyDrive[2].ImageType.Extension==EXT_SCP)
          {
            char* src= (FloppyDisk[2].IsZip())?FloppyDisk[2].ZipTempFile.Text:Inf->Path.Text;
            SCPtoSTW(src,dest);
            FloppyDisk[2].WrittenTo=true;
          }
#endif
#if defined(SSE_DISK_STW)
          if(FloppyDisk[2].WrittenTo)
            ImageSTW[2].Close();
#endif
          FloppyDrive[2].RemoveDisk(true);
          This->RefreshDiskView(dest,true); // make new image appear
        }//!err
        OPTION_AUTOSTW=save1;
        FloppyDisk[DRIVE_A].ReadOnly=save2;
      }//inf
    }
    else if(wpar_lo>=IDC_INSERTA_MULTI&&wpar_lo<(IDC_INSERTA_MULTI+1000)) // 9000-9999
    {
      TDiskManFileInfo *Inf=This->GetItemInf(This->MenuTarget);
      if(Inf) 
      {
        int Action=(wpar_lo-IDC_INSERTA_MULTI)/200;
        EasyStr DiskInZip=This->MenuESL[(wpar_lo-IDC_INSERTA_MULTI)%200].String;
        This->PerformInsertAction(Action,Inf->Name,Inf->Path,DiskInZip);
      }
    }
    else if(wpar_lo>=IDC_MSACONV&&wpar_lo<IDC_PATTERNS) //2030-2099
    { // MSA Converter
      Str MSA=This->GetMSAConverterPath();
      if(This->MSAConvPath.NotEmpty()) 
      {
        Str Comline;
        TDiskManFileInfo *Inf=This->GetItemInf(This->MenuTarget);
        Str NewSel;
        switch(wpar_lo) {
        case IDC_MSACONVOPENDI:  Comline=Str("\"")+Inf->Path+"\"";  break;
        case IDC_MSACONVZIPTODI: // Zip containing files to disk image
          NewSel=Inf->Path;
          *strchr(NewSel,'.')='\0';
          NewSel+=".st";
          Comline=Str("\"convert\" \"")+Inf->Path+"\" \"st\" \"exit\"";
          break;
        default:
          if(wpar_lo>IDC_EXTRACT_HD&&wpar_lo<=IDC_EXTRACT_HD+26) 
          {
            NewSel=GetUniquePath(Stemdos.MountPath[wpar_lo-IDC_EXTRACT_HD],Inf->Name);
            CreateDirectory(NewSel,NULL);
            Comline=Str("\"diskimg_to_hdisk\" \"")+Inf->Path+"\" \""
              +NewSel+"\" \"exit\"";
            NewSel=""; // Nothing to select
          }
          else
            Comline=Str("\"user_path\" \"")+This->DisksFol+"\"";
        }
        SHELLEXECUTEINFO sei;
        sei.cbSize=sizeof(SHELLEXECUTEINFO);
        sei.fMask=SEE_MASK_FLAG_NO_UI|SEE_MASK_NOCLOSEPROCESS;
        sei.hwnd=NULL;
        sei.lpVerb=NULL;
        sei.lpFile=MSA;
        sei.lpParameters=Comline;
        sei.lpDirectory=NULL;
        sei.nShow=SW_SHOWNORMAL;
        if(ShellExecuteEx(&sei)) 
        {
          if(NewSel.NotEmpty()) 
          {
            This->MSAConvProcess=sei.hProcess;
            This->MSAConvSel=NewSel;
            SetTimer(Win,MSACONV_TIMER_ID,1000,NULL);
          }
        }
      }
    }
    break;
  }
  case WM_CONTEXTMENU:
    if((HWND)wPar==This->DiskView) 
    {
      HMENU Pop=CreatePopupMenu();
      int c=(int)SendMessage(This->DiskView,LVM_GETITEMCOUNT,0,0);
      LV_ITEM lvi;
      lvi.mask=LVIF_PARAM|LVIF_STATE;
      lvi.iSubItem=0;
      lvi.stateMask=LVIS_SELECTED;
      for(lvi.iItem=0;lvi.iItem<c;lvi.iItem++) 
      {
        SendMessage(This->DiskView,LVM_GETITEM,0,(LPARAM)&lvi);
        if(lvi.state==LVIS_SELECTED) 
        {
          This->AddFileOrFolderContextMenu(Pop,(TDiskManFileInfo*)lvi.lParam);
          This->MenuTarget=lvi.iItem;
          break;
        }
      }
      HMENU FolOptionsPop=Pop;
      if(GetScreenHeight()<600) 
      {
        FolOptionsPop=CreatePopupMenu();
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_POPUP|MF_STRING,
          (UINT_PTR)FolOptionsPop,T("More Options"));
      }
      InsertMenu(FolOptionsPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_REFRESH,
        Str(T("Refresh"))+" \10ESC");
      InsertMenu(FolOptionsPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_BACKGROUND,
        T("Change background"));
      InsertMenu(FolOptionsPop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,999,NULL);
      InsertMenu(FolOptionsPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_FOLEXPLORER,
        T("Open Current Folder In &Explorer")+"\10F4");
      InsertMenu(FolOptionsPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_FINDCUR,
        T("Find In Current Folder"));
      InsertMenu(FolOptionsPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_MSACONV,
        T("Run MSA Converter")+"\10F6");
      //InsertMenu(FolOptionsPop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,999,NULL);
      HMENU NewPop=CreatePopupMenu();
      InsertMenu(NewPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_NEWFOLDER,T("&Folder"));
      InsertMenu(NewPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_NEWST,T("&ST Disk Image"));
      InsertMenu(NewPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_NEWHDST,T("HD ST Disk Image"));
      InsertMenu(NewPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_NEWCUSTOM,T("&Custom Disk Image"));
#if defined(SSE_DISK_STW)
      InsertMenu(NewPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_NEWSTW,T("ST&W Disk Image"));
#endif
      InsertMenu(NewPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_NEWHDSTW,T("HD STW Disk Image"));
#if defined(SSE_DISK_HFE)
      InsertMenu(NewPop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_NEWHFE,T("&HFE Disk Image"));
#endif
      InsertMenu(FolOptionsPop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_POPUP,
        (UINT_PTR)NewPop,T("&New..."));
      POINT pt;
      GetCursorPos(&pt);
      TrackPopupMenu(Pop,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,pt.x,pt.y,0,Win,NULL);
      DestroyMenu(Pop);
    }
    else 
    {
      HWND hca=GetDlgItem(Win,IDC_CONTENTA);
      HWND hcb=GetDlgItem(Win,IDC_CONTENTB);
      if( ((HWND)wPar==hca||(HWND)wPar==hcb) && 
      // right click in box on the right of disk icon A or B
        SendMessage((HWND)wPar,LVM_GETITEMCOUNT,0,0)>0) // disk image in
      {
        HMENU Pop=CreatePopupMenu();
        LV_ITEM lvi;
        lvi.iItem=0;
        lvi.iSubItem=0;
        lvi.mask=LVIF_PARAM;
        SendMessage((HWND)wPar,LVM_GETITEM,0,(LPARAM)&lvi);
        TDiskManFileInfo *Inf=(TDiskManFileInfo*)lvi.lParam;
        This->MenuTarget=((HWND)wPar==hca) ? DRIVE_A : DRIVE_B;
        if(!Inf->Zip)
          InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(Inf->ReadOnly),
                     IDC_READONLYA+This->MenuTarget,T("Read &Only"));
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING
                   |MF_CHECK(FloppyDisk[This->MenuTarget].WriteProtect),
                   IDC_WRITEPROTECT+This->MenuTarget,T("&Write protect"));
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_SWAP,T("&Swap A: and B:"));
#if defined(SSE_DISK_SWAPPER)
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_PREVIOUS,T("Swapper &Previous disk"));
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_NEXT,T("Swapper &Next disk"));
#endif
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR|MF_STRING,999,"-");
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_EJECT,T("Eject Disk \10DEL"));
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_EJECTNOSAVE,
          T("&Eject Disk (don't save changes)"));
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_SEPARATOR,999,NULL);
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_GOTODISK,T("&Go To Disk"));
/*  This is so the player can read the full name of the disk without
    checking at the place of storage.
    If he clicks on it, it is copied in the clipboard, whatever use this
    then may have.
*/
        InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_COPYCLP,Inf->Name.Text);
        POINT pt;
        GetCursorPos(&pt);
        TrackPopupMenu(Pop,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,pt.x,pt.y,0,Win,NULL);
        DestroyMenu(Pop);
      }
    }
    break;
  case WM_NOTIFY:
    if(wPar==IDC_DISKVIEW) 
    {  //DiskView Only
      switch(((NMHDR*)lPar)->code) {
      case LVN_GETINFOTIP:
      {
        // on hovering, display the first image extension if any
        // handy if extensions are hidden or the image is zipped
        NMLVGETINFOTIP *pGetInfoTip = (LPNMLVGETINFOTIP)lPar;
        if(pGetInfoTip)
        {
          int ix=pGetInfoTip->iItem;
          TDiskManFileInfo *Inf=This->GetItemInf(ix);
          int ext=This->IsDiskImage(Inf->Path.Text);
          if(ext)
          {
            char tip[256*2];
            if((int)strlen(Inf->Name.Text)<256*2-5-3)
              sprintf(tip,"(%s) %s",extension_list[ext],Inf->Name.Text);
            if(This->MenuESL.NumStrings>1)
              strcat(tip,"...");
            if((int)strlen(tip)<pGetInfoTip->cchTextMax)
              strcpy(pGetInfoTip->pszText,tip);
          }
        }
        break;
      }
      case LVN_GETDISPINFO:
      {
        LV_DISPINFO *DispInf=(LV_DISPINFO*)lPar;
        DispInf->item.mask=LVIF_TEXT;
        DispInf->item.pszText=((TDiskManFileInfo*)(DispInf->item.lParam))->
          Name.Text;
        break;
      }
      case LVN_BEGINLABELEDIT:
      {
        LV_DISPINFO *DispInf=(LV_DISPINFO*)lPar;
        if(DispInf->item.iItem==0) 
          return true;
        return 0;
      }
      case LVN_ENDLABELEDIT:
      {
        LV_DISPINFO *DispInf=(LV_DISPINFO*)lPar;
        if(DispInf->item.pszText==NULL) 
          return 0;
        TDiskManFileInfo *Inf=(TDiskManFileInfo*)DispInf->item.lParam;
        bool Link=Inf->LinkPath.NotEmpty();
        bool InDrive[2]={false,false};
        EasyStr NewDiskName[2],OldDiskName[2],OldDiskPath[2],DiskInZip[2];
        EasyStr NewName=DispInf->item.pszText,OldName=Inf->Name,Extension;
        EasyStr OldPath=This->DisksFol+SLASH+OldName;
        EasyStr NewPath=This->DisksFol+SLASH+NewName;
        if(Link)       
          Extension=".lnk";
        else if(!Inf->Folder)
          Extension=strrchr(Inf->Path,'.');
        if(This->HideExtension && Extension.NotEmpty())
        {
          OldPath+=Extension;
          NewPath+=Extension;
        }
        if(!Link && !Inf->Folder)
        { // Check for inserted disks
          for(int d=DRIVE_A;d<=DRIVE_B;d++) 
          {
            OldDiskName[d]=FloppyDisk[d].DiskName;
            NewDiskName[d]=(OldDiskName[d]==OldName) ? NewName : OldDiskName[d];
            OldDiskPath[d]=FloppyDrive[d].GetDisk();
            DiskInZip[d]=FloppyDisk[d].DiskInZip;
            if(IsSameStr_I(OldDiskPath[d],OldPath)) 
            {
              InDrive[d]=true;
              FloppyDrive[d].RemoveDisk();
            }
          }
        }
        else 
        {
          // Check for inserted disks being in renamed folder TODO
        }
        BOOL success=MoveFile(OldPath,NewPath);
        if(success)
        {
          if(Link) // with or without original extension
            Inf->LinkPath=This->DisksFol+SLASH+NewName+Extension;
          else 
          {
            if(!Inf->Folder)
              This->UpdateBPBFiles(Inf->Path,NewPath,true);
            Inf->Path=NewPath;
          }
          Inf->Name=NewName;
          for(int d=DRIVE_A;d<=DRIVE_B;d++) 
          {
            if(InDrive[d]) // as checked before
            {
              This->InsertHistoryDelete(d,OldDiskName[d],OldPath,DiskInZip[d]);
              This->InsertHistoryAdd(d,NewDiskName[d],NewPath,DiskInZip[d]);
              FloppyDrive[d].SetDisk(NewPath,DiskInZip[d]);
              FloppyDisk[d].DiskName=NewDiskName[d];
              HWND LV=GetDlgItem(Win,IDC_CONTENTA+d);
              Inf=This->GetItemInf(0,LV);
              Inf->Path=FloppyDrive[d].GetDisk();
              if(Inf->Name==OldDiskName[d]) 
              {
                Inf->Name=NewDiskName[d];
                LV_ITEM lvi;
                lvi.mask=LVIF_TEXT;
                lvi.iItem=0;
                lvi.iSubItem=0;
                lvi.pszText=NewDiskName[d].Text;
                SendMessage(LV,LVM_SETITEM,0,(LPARAM)&lvi);
                CentreLVItem(LV,0);
              }
            }
          }
          return true;
        }
        else 
        {
          Alert(T("Unable to rename file (read-only? inserted?)"),T("ERROR"),
            MB_ICONEXCLAMATION);
          for(int d=DRIVE_A;d<=DRIVE_B;d++) 
          {
            if(InDrive[d])
            {
              FloppyDrive[d].SetDisk(OldPath,DiskInZip[d]);
              FloppyDisk[d].DiskName=OldDiskName[d];
            }
          }
        }
        return 0;
      }
      case LVN_KEYDOWN:
      {
        if(This->Dragging>-1) 
          break;
        LV_KEYDOWN *KeyInf=(LV_KEYDOWN*)lPar;
        switch(KeyInf->wVKey) {
        case VK_RETURN:case VK_SPACE:
          PostMessage(Win,WM_USER,1234,0);
          break;
        case VK_BACK:
          PostMessage(Win,WM_USER,1234,1);
          break;
        case VK_DELETE:
          if(GetKeyState(VK_SHIFT)<0
            ||Alert(T("Delete?"),T("Are you sure?"),MB_YESNO|MB_ICONQUESTION)==IDYES)
          PostMessage(Win,WM_USER,1234,2);
          break;
        case VK_F2:
        {
          int SelItem=This->GetSelectedItem();
          if(SelItem>-1) 
            SendMessage(This->DiskView,LVM_EDITLABEL,SelItem,0);
          break;
        }
        case VK_F3:
        {
          int SelItem=This->GetSelectedItem();
          if(SelItem>-1) 
          {
            TDiskManFileInfo *Inf=This->GetItemInf(SelItem);
            if(Inf->Folder)
              ShellExecute(NULL,"Find",Inf->Path,"","",SW_SHOWNORMAL);
            else
              SelItem=-1;
          }
          if(SelItem==-1)
            ShellExecute(NULL,"Find",This->DisksFol,"","",SW_SHOWNORMAL);
          break;
        }
        case VK_F4:
          PostMessage(Win,WM_COMMAND,IDC_FOLEXPLORER,0);
          break;
        case VK_F5:
          PostMessage(Win,WM_COMMAND,IDCANCEL,0);
          break;
        case VK_F6:
          PostMessage(Win,WM_COMMAND,IDC_MSACONV,0);
          break;
        case VK_F9:
          PostMessage(Win,WM_COMMAND,IDC_DATABASE,0);
          break;
        case VK_F12: // start/stop emulation on F12 in disk manager
          if(SSEOptions.F12Run)
          {
            CLICK_PLAY_BUTTON(); // it's a macro
          }
          return TRUE;
        case VK_PAUSE: // start/stop emulation on Pause in disk manager
          if(SSEOptions.PauseRun)
          {
            CLICK_PLAY_BUTTON();
          }
          return TRUE;
        // intercept a & b... (bad idea?)
        case 'A': // insert in A:
          PostMessage(Win,WM_USER,1234,3);
          return TRUE; // this breaks usual Windows behaviour
        case 'B': // insert in B:
          PostMessage(Win,WM_USER,1234,4);
          return TRUE;
        }
        break;
      }
      case NM_DBLCLK:
        if(This->Dragging>-1) 
          break;
        PostMessage(Win,WM_USER,1234,0);
        break;
      }//sw
    }
    if(wPar>=IDC_CONTENTA&&wPar<=IDC_DISKVIEW) 
    { //ListView
      switch(((NMHDR*)lPar)->code) {
      case LVN_DELETEITEM:
      {
        LV_ITEM lvi;
        lvi.mask=LVIF_PARAM;
        lvi.iItem=((NM_LISTVIEW*)lPar)->iItem;
        lvi.iSubItem=0;
        SendMessage(GetDlgItem(Win,(int)wPar),LVM_GETITEM,0,(LPARAM)&lvi);
        delete ((TDiskManFileInfo*)lvi.lParam);
        if(wPar<IDC_DISKVIEW) 
          This->SetDriveViewEnable((int)wPar-IDC_CONTENTA,false);
        break;
      }
      case LVN_BEGINDRAG:case LVN_BEGINRDRAG:
        This->BeginDrag(((NM_LISTVIEW*)lPar)->iItem,GetDlgItem(Win,(int)wPar));
        break;

#if defined(SSE_GUI_DM_PICS)
      case NM_CUSTOMDRAW:
      {
        LPNMLVCUSTOMDRAW  lplvcd = (LPNMLVCUSTOMDRAW)lPar;
        switch(lplvcd->nmcd.dwDrawStage) {
        case CDDS_PREPAINT:
          return CDRF_NOTIFYITEMDRAW; // to get CDDS_ITEMPREPAINT
        case CDDS_ITEMPREPAINT: // we don't check here if we should draw to spare code
           return CDRF_NOTIFYPOSTPAINT ; // we do that so we get the text (filename) drawn
        case CDDS_ITEMPOSTPAINT:
        {
          TDiskManFileInfo* pInf=(TDiskManFileInfo*)lplvcd->nmcd.lItemlParam;
          if(pInf->Image==IMG_CUSTOM)
          {
            if(MyGdiPlus.GetPic(pInf->Path))
            {
              HDC hdcDest=lplvcd->nmcd.hdc;
              // lplvcd->nmcd.rc is wrong! need to get the right RECT
              RECT rc2;
              DWORD_PTR index=lplvcd->nmcd.dwItemSpec; // get index of item
              ListView_GetItemRect(GetDlgItem(Win,(int)wPar),index,&rc2,LVIR_ICON); // get its RECT
              MyGdiPlus.DisplayPicFile(NULL,hdcDest,&rc2); // display pic using GDI+
            }
          }
        }
        }//sw
      }//case NM_CUSTOMDRAW:
#endif//#if defined(SSE_GUI_DM_PICS)

      }//sw
      if(wPar<IDC_DISKVIEW) 
      {
        if(((NMHDR*)lPar)->code==LVN_KEYDOWN)
        {
          LV_KEYDOWN *KeyInf=(LV_KEYDOWN*)lPar;
          if(KeyInf->wVKey==VK_DELETE) 
            This->EjectDisk(!!((int)wPar-IDC_CONTENTA));
        }
      }
    }
    break;
  case WM_MOUSEMOVE:
    if(This->Dragging>-1) 
      This->MoveDrag();
    break;
  case WM_TIMER:
    if(wPar==DISKVIEWSCROLL_TIMER_ID) 
    {
      if(This->DragLV==This->DiskView&&(This->LastOverID!=80||This->AtHome)) 
      {
        POINT spt;
        GetCursorPos(&spt);
        RECT rc;
        GetWindowRect(This->DiskView,&rc);
        if(spt.x>=rc.left && spt.y<=rc.right) 
        {
          int y=0;
          if(spt.y<=rc.top+2&&spt.y>=rc.top-20)
            y=-5;
          else if(spt.y>=rc.bottom-2&&spt.y<=rc.bottom+10)
            y=5;
          if(y) 
          {
            if(This->DragEntered) 
            {
              ImageList_DragLeave(Win);
              This->DragEntered=false;
            }
            SendMessage(This->DiskView,LVM_SCROLL,0,y);
            UpdateWindow(This->DiskView);
          }
        }
      }
    }
    else if(wPar==MSACONV_TIMER_ID) 
    {
      bool Kill=true;
      DWORD Code;
      if(GetExitCodeProcess(This->MSAConvProcess,&Code)) 
      {
        if(Code==STILL_ACTIVE)
          Kill=false;
        else 
        {
          This->GoToDisk(This->MSAConvSel,true);
          This->MSAConvSel="";
          This->MSAConvProcess=NULL;
        }
      }
      if(Kill) 
        KillTimer(Win,MSACONV_TIMER_ID);
    }
    break;
  case WM_LBUTTONUP:case WM_RBUTTONUP:
    if(This->Dragging>-1)
      This->EndDrag(GET_X_LPARAM(lPar),GET_Y_LPARAM(lPar),(Mess==WM_RBUTTONUP));
    break;
  case WM_CAPTURECHANGED:
    if(!This->EndingDrag)
    {
      if(This->Dragging>-1) 
      {
        if(This->DragEntered) 
        {
          ImageList_DragLeave(Win);
          This->DragEntered=false;
        }
        ImageList_EndDrag();
        SendMessage(GetDlgItem(Win,IDC_HOME),BM_SETCHECK,0,0);
        This->Dragging=-1;
        InvalidateRect(This->DiskView,NULL,TRUE);
      }
    }
    break;
  case WM_USER:
    if(wPar==1234) 
    {
      LV_ITEM lvi;
      ZeroMemory(&lvi,sizeof(lvi)); //W4
      int c=(int)SendMessage(This->DiskView,LVM_GETITEMCOUNT,0,0);
      lvi.mask=LVIF_PARAM|LVIF_STATE;
      lvi.iSubItem=0;
      lvi.stateMask=LVIS_SELECTED;
      for(lvi.iItem=0;lvi.iItem<c;lvi.iItem++) 
      {
        SendMessage(This->DiskView,LVM_GETITEM,0,(LPARAM)&lvi);
        if(lvi.state==LVIS_SELECTED)
          break;
      }
      TDiskManFileInfo *Inf=(TDiskManFileInfo*)lvi.lParam;
      int floppy_no=DRIVE_A;
      if(lPar>=3)
      {
        floppy_no=(int)lPar-2;
        lPar=0;
      }
      if(lPar==0&&lvi.iItem<c) 
      {
        if(Inf->Folder) 
        {
          if(Inf->UpFolder)
            lPar=1;
          else
            This->SetDir(Inf->Path,true);
        }
        else 
        {
          TDiskManFileInfo *Inf2=This->GetItemInf(This->GetSelectedItem());
          if(Inf2) 
          {
            if(floppy_no)
              This->PerformInsertAction(ACTION_INSERT_A+floppy_no-1,Inf2->Name,Inf->Path,"");
            else switch(This->DoubleClickAction) {
            case ACTION_DOUBLE_CLICK_INSERT_A:
              This->PerformInsertAction(ACTION_INSERT_A,Inf2->Name,Inf->Path,"");
              break;
            case ACTION_INSERT_RUN:
              This->PerformInsertAction(ACTION_INSERT_RUN,Inf2->Name,Inf->Path,"");
              break;
            }
          }
        }
      }
      else if(!lPar)
        lPar++; // double click on no icon: go up in directory tree
      if(lPar==1) 
      {  // Go Up
        EasyStr Fol=This->DisksFol;
        char *LastSlash=strrchr(Fol,'\\');
        if(LastSlash==NULL) 
        {
          LastSlash=strrchr(Fol,'/');
          if(LastSlash==NULL)
            LastSlash=strrchr(Fol,':');
        }
        if(LastSlash!=NULL) 
        {
          *LastSlash='\0';
          if(NotSameStr_I(Fol,This->DisksFol)) 
            This->SetDir(Fol,true,This->DisksFol);
        }
      }
      else if(lPar==2 && !Inf->UpFolder)
      {  //Delete
#if defined(SSE_LONG_PATH)
        EasyStr sFol;
        sFol.SetLength(SSE_MAX_PATH);
        char *Fol=sFol.Text;
        ZeroMemory(Fol,SSE_MAX_PATH);
#else
        char Fol[MAX_PATH+2];
        ZeroMemory(Fol,MAX_PATH+2);
#endif
        if(Inf->LinkPath.IsEmpty())
          strcpy(Fol,Inf->Path);
        else
          strcpy(Fol,Inf->LinkPath);
        EasyStr OldDisk[2],OldName[2],DiskInZip[2];
        for(int d=DRIVE_A;d<=DRIVE_B;d++) 
        {
          if(IsSameStr_I(FloppyDrive[d].GetDisk(),Fol)) 
          {
            OldDisk[d]=Fol;
            OldName[d]=FloppyDisk[d].DiskName;
            DiskInZip[d]=FloppyDisk[d].DiskInZip;
            FloppyDrive[d].RemoveDisk();
          }
        }
        SHFILEOPSTRUCT fos;
        fos.hwnd=(FullScreen) ? StemWin : This->Handle;
        fos.wFunc=FO_DELETE;
        fos.pFrom=Fol;
        fos.pTo="\0\0";
        fos.fFlags=(FILEOP_FLAGS)(((GetKeyState(VK_SHIFT)<0) ? 0 : FOF_ALLOWUNDO)
#if defined(SSE_WINDOWS_2000_MIN) && !defined(MINGW_BUILD)
          | (FullScreen ? FOF_SILENT : FOF_WANTNUKEWARNING));
#else
          | (FullScreen ? FOF_SILENT : 0));
#endif
        fos.hNameMappings=NULL;
        fos.lpszProgressTitle=StaticT("Deleting...");
        EnableWindow(This->Handle,FALSE);
        SHFileOperation(&fos);
        EnableWindow(This->Handle,TRUE);
        for(int disk=DRIVE_A;disk<=DRIVE_B;disk++) 
        {
          if(OldDisk[disk].NotEmpty()) 
          {
            if(fos.fAnyOperationsAborted) 
            {
              FloppyDrive[disk].SetDisk(OldDisk[disk],DiskInZip[disk]);
              FloppyDisk[disk].DiskName=OldName[disk];
            }
            else
              SendMessage(GetDlgItem(Win,IDC_CONTENTA+disk),LVM_DELETEITEM,0,0);
          }
        }
        if(!fos.fAnyOperationsAborted)
        {
          if(!Inf->Folder && Inf->LinkPath.IsEmpty())  // Deleting disk
            This->UpdateBPBFiles(Inf->Path,"",false);
          This->RefreshDiskView("",false,"",lvi.iItem);
        }
      }
    }
    SendMessage(GetDlgItem(Win,IDC_HDGEMDOS),BM_SETCHECK,
      !HardDiskMan.DisableHardDrives||HardDiskMan.IsVisible(),0);
#if defined(SSE_ACSI_MNGR)
    SendMessage(GetDlgItem(Win,IDC_HDACSI),BM_SETCHECK,
      SSEOptions.Acsi||AcsiHardDiskMan.IsVisible(),0);
#endif
    REFRESH_STATUS_BAR_GX;
    break;

  case WM_SIZE:
    if(HWND hwnd=GetDlgItem(Win,IDC_HDGEMDOS)) 
    {
      WORD lpar_lo=LOWORD(lPar);
      SetWindowPos(hwnd,0,lpar_lo-80,10,0,0,SWP_NOSIZE|SWP_NOZORDER);
#if defined(SSE_ACSI_MNGR)
      SetWindowPos(GetDlgItem(Win,IDC_HDACSI),0,lpar_lo-80*2,10,0,0,
        SWP_NOSIZE|SWP_NOZORDER);
#endif
      int w=GuiSM.mCbUnits+GuiSM.mHorizontalSeparation+187+BIG_ICONS*101
        -GuiSM.m_cx_frame; // adjust path right border
      SetWindowPos(GetDlgItem(Win,IDP_DISK),0,0,0,lpar_lo-w,
        GuiSM.mCharHeight,SWP_NOMOVE|SWP_NOZORDER);
      int h=108+BIG_ICONS*12+GuiSM.m_cy_frame; // adjust low border
#if defined(SSE_GUI_DM_BIGICONS)
      h+=(BIG_ICONS)?(26+32):(16+32);
#else
      h+=(BIG_ICONS)?26:16;
#endif
      SetWindowPos(This->DiskView,NULL,0,0,lpar_lo-20,HIWORD(lPar)-h,SWP_NOMOVE|SWP_NOZORDER);
#if defined(SSE_GUI_DM_BIGICONS)
      if(This->IconSize>0)
#else
      if(!This->SmallIcons)
#endif
        SendMessage(This->DiskView,LVM_ARRANGE,LVA_DEFAULT,0);
    }
    if(FullScreen) 
    {
      if(!IsZoomed(Win))
      {
        This->FSMaximized=false;
        RECT rc;
        GetWindowRect(Win,&rc);
        This->FSWidth=rc.right-rc.left;
        This->FSHeight=rc.bottom-rc.top;
      }
      else 
        This->FSMaximized=true;
    }
    else 
    {
      if(!IsIconic(Win))
      {
        if(!IsZoomed(Win))
        {
          This->Maximized=false;
          RECT rc;
          GetWindowRect(Win,&rc);
          This->Width=rc.right-rc.left;
          This->Height=rc.bottom-rc.top;
        }
        else
          This->Maximized=true;
      }
    }
    break;
  case (WM_USER+1011):
  {
    if(This->VisibleDiag())
      SendMessage(This->VisibleDiag(),WM_COMMAND,IDCANCEL,0);
    HWND NewParent=(HWND)lPar;
    if(NewParent) 
    {
      //        SetWindowLong(Win,GWL_STYLE,GetWindowLong(Win,GWL_STYLE) & ~WS_MINIMIZEBOX);
      This->CheckFSPosition(NewParent);
      SetWindowPos(Win,NULL,This->FSLeft,This->FSTop,This->FSWidth,This->FSHeight,SWP_NOZORDER);
    }
    else
      SetWindowPos(Win,NULL,This->Left,This->Top,This->Width,This->Height,SWP_NOZORDER);
    This->ChangeParent(NewParent);
    break;
  }
  case WM_GETMINMAXINFO:
  {
    MINMAXINFO *mmi=(MINMAXINFO*)lPar;
#if defined(SSE_ACSI_MNGR)
    mmi->ptMinTrackSize.x=403+70+GuiSM.cx_frame()*2+GuiSM.cx_vscroll();
#else
    mmi->ptMinTrackSize.x=403+GuiSM.cx_frame()*2+GuiSM.cx_vscroll();
#endif
    mmi->ptMinTrackSize.y=186+GuiSM.cy_caption()+GuiSM.cy_frame()*2;
    if(FullScreen) 
    {
      mmi->ptMaxSize.x=GuiSM.cx_screen()+GuiSM.cx_frame()*2;
      mmi->ptMaxSize.y=GuiSM.cy_screen()+GuiSM.cy_frame()-MENUHEIGHT;
      mmi->ptMaxPosition.x=-GuiSM.cx_frame();
      mmi->ptMaxPosition.y=MENUHEIGHT;
    }
    else 
    {
      mmi->ptMaxPosition.x=-GuiSM.cx_frame();
      mmi->ptMaxPosition.y=-GuiSM.cy_frame();
    }
    break;
  }
  case WM_CLOSE:
    This->Hide();
    return 0;
  }
  return DefWindowProc(Win,Mess,wPar,lPar);
}


//#pragma warning (default: 4701)


LRESULT CALLBACK TDiskManager::Drive_Icon_WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar) {
  TDiskManager *This=(TDiskManager*)GetWindowLongPtr(Win,GWLP_USERDATA);
  BYTE d=(BYTE)(GetDlgCtrlID(Win)-IDC_DRIVEA);
  ASSERTB(d<2);
  TSF314 &drive=FloppyDrive[d]; // shortcut
  TFloppyDisk &disk=FloppyDisk[d]; // shortcut
  WORD wpar_lo=LOWORD(wPar);
  switch(Mess) {
  case WM_PAINT:
  {
    PAINTSTRUCT ps;
    RECT box;
    HBRUSH br;
    BeginPaint(Win,&ps);
    GetClientRect(Win,&box);
    br=CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    FillRect(ps.hdc,&box,br);
    if(d==DRIVE_B&&This->nFloppyDrives==1)
      DrawIconEx(ps.hdc,0,0,hGUIIcon[RC_ICO_DRIVEB_DISABLED],64,64,0,NULL,DI_NORMAL);
    else
      DrawIconEx(ps.hdc,0,0,hGUIIcon[RC_ICO_DRIVEA+d],64,64,0,br,DI_NORMAL);
    HFONT hFont;
    SetBkMode(ps.hdc, TRANSPARENT);
    hFont = (HFONT)GetStockObject(ANSI_VAR_FONT); // -> smaller
    (HFONT)SelectObject(ps.hdc, hFont);
    if(pasti_active || drive.ImageType.Manager==MNGR_PASTI)
      TextOut(ps.hdc,22,48,T("Pasti"),(int)T("Pasti").Length());
    else if(This->bTurboDrive) // draw fast chip
      DrawIconEx(ps.hdc,24,48,hGUIIcon[RC_ICO_CPUALTSPEED],16,16,0,NULL,DI_NORMAL);
    DeleteObject(br);
#if defined(SSE_DRIVE_SINGLESIDE)
    if(drive.bSingleSided)
      TextOut(ps.hdc,0,0,T("SF354"),(int)T("SF354").Length());
#endif
#if defined(SSE_DRIVE_FREEBOOT)
    if(drive.bFreeboot)
      TextOut(ps.hdc,0,0,T("Freeboot"),(int)T("Freeboot").Length());
#endif
    EndPaint(Win,&ps);
    return 0;
  }
  case WM_LBUTTONDOWN:
  case WM_LBUTTONDBLCLK:
    if(d==DRIVE_B) 
      This->SetNumFloppies(3-This->nFloppyDrives); // assume 1 or 2 (if 0->3->2)
    return 0;
  case WM_RBUTTONDOWN: // right click on drive, make context menu
  case WM_CONTEXTMENU:
  {
    This->MenuTarget=d;
    HMENU Pop=CreatePopupMenu();
    InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_FILESELEC+d,
      T("Choose with Windows file selector"));
    if(OPTION_HACKS)
      InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(This->bTurboDrive),
                 IDC_TURBODRIVE,T("&Turbo Drive"));
#if defined(SSE_DRIVE_SINGLESIDE)
/*  The first 520 ST were equipped with a single-sided drive unfortunately.
    The external model was called SF354 (double-sided was SF314).
    The first STF also had an internal single-sided drive.
    Because of that, almost all games were single-sided for a long time.
*/
    if(drive.ImageType.Manager!=MNGR_PASTI)
      InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(drive.bSingleSided),
                 IDC_SINGLESIDE+d,T("Single-sided drive"));
#endif
#if defined(SSE_DRIVE_FREEBOOT)
/*  The Freeboot was a hardware mod that could force the drive side to 1 (B)
    with a switch. Using it fooled the ST into thinking it was reading
    side A (based on the YM2149 register) when in fact it was B, so you could
    for example have two single-sided games on one double sided disk, or disk B
    of a game on side B.
    It worked because the WD1772 doesn't check side (it couldn't, it doesn't
    know the side it's operating on).
    It was also possible to effectively swap drive A: and B:, which was useful
    if your internal drive was single-sided and your boot disk double-sided.
*/
    InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING|MF_CHECK(drive.bFreeboot),
               IDC_FREEBOOT+d,T("Freeboot"));
#endif
    if(OPTION_HACKS && drive.bMotor)
      InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_STOPMOTOR+d,T("Stop motor"));
#if defined(SSE_DRIVE_SOUND)
    if(OPTION_DRIVE_SOUND)
    {
      InsertMenu(Pop,0xffffffff,MF_BYPOSITION|MF_STRING,IDC_SOUNDDIR+d,
        T("Choose drive sound directory"));
    }
#endif
    POINT pt;
    GetCursorPos(&pt); // menu will appear at the mouse pointer
    TrackPopupMenu(Pop,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,pt.x,pt.y,0,This->Handle,NULL);
    DestroyMenu(Pop);
    return 0; // mimic
  }
  case WM_COMMAND:
    if(wpar_lo==IDC_HISTBUT) 
    {
      SendMessage((HWND)lPar,BM_SETCHECK,1,0);
      HMENU Pop=CreatePopupMenu();
      EasyStr CurrentDiskName=This->CreateDiskName(disk.DiskName,disk.DiskInZip);
      for(int n=0;n<DM_HISTORY_LEN;n++) 
      {
        if(This->InsertHist[d][n].Path.NotEmpty()) 
        {
          EasyStr MenuItemText=This->CreateDiskName(This->InsertHist[d][n].Name,
                                                    This->InsertHist[d][n].DiskInZip);
          if(NotSameStr_I(CurrentDiskName,MenuItemText))
            AppendMenu(Pop,MF_STRING,IDC_HIST_BASE+n,MenuItemText);
        }
      }
      RECT rc;
      GetWindowRect((HWND)lPar,&rc);
      TrackPopupMenu(Pop,TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,rc.left,rc.bottom,0,Win,NULL);
      DestroyMenu(Pop);
      SendMessage((HWND)lPar,BM_SETCHECK,0,0);
    }
    else
    {
      WORD a=wpar_lo-IDC_HIST_BASE;
      if(a<DM_HISTORY_LEN)
      {
        This->InsertDisk(d,This->InsertHist[d][a].Name,This->InsertHist[d][a].Path,false,true,
          This->InsertHist[d][a].DiskInZip,false,true);
      }
    }
    break;
  }
  return DefWindowProc(Win,Mess,wPar,lPar);
}


LRESULT CALLBACK TDiskManager::DriveView_WndProc(HWND Win,UINT Mess,
                                            WPARAM wPar,LPARAM lPar) {
  TDiskManager *This=(TDiskManager*)GetWindowLongPtr(Win,GWLP_USERDATA);
  if(Mess==WM_DROPFILES) 
  {
    int nFiles=DragQueryFile((HDROP)wPar,0xffffffff,NULL,0);
    for(int i=0;i<nFiles;i++) 
    {
      EasyStr File;
      File.SetLength(SSE_MAX_PATH);
      DragQueryFile((HDROP)wPar,i,File,SSE_MAX_PATH);
      char *dot=strrchr(GetFileNameFromPath(File),'.');
      if(dot!=NULL && IsSameStr_I(dot,".LNK"))
      {
        WIN32_FIND_DATA wfd;
        File=GetLinkDest(File,&wfd);
        dot=strrchr(GetFileNameFromPath(File),'.');
      }
      if(dot!=NULL) 
      {
        if(ExtensionIsDisk(dot)) 
        {
          EasyStr Name=GetFileNameFromPath(File);
          *strrchr(Name,'.')='\0';
          if(DiskMan.InsertDisk(GetDlgCtrlID(Win)-IDC_CONTENTA,Name,File,false,false,"",false,true)) 
            break;
        }
      }
    }
    DragFinish((HDROP)wPar);
    SetForegroundWindow(This->Handle);
    return 0;
  }
  else if(Mess==WM_KEYDOWN && This->Dragging>-1)
    return 0;
  else if(Mess==WM_LBUTTONDOWN||Mess==WM_MBUTTONDOWN||Mess==WM_RBUTTONDOWN||
          Mess==WM_LBUTTONDBLCLK||Mess==WM_MBUTTONDBLCLK||Mess==WM_RBUTTONDBLCLK)
  {
    if(SendMessage(Win,LVM_GETITEMCOUNT,0,0)==0)
      return 0;
  }
  return CallWindowProc(This->Old_ListView_WndProc,Win,Mess,wPar,lPar);
}


LRESULT CALLBACK TDiskManager::DiskView_WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar) {
  TDiskManager *This=(TDiskManager*)GetWindowLongPtr(Win,GWLP_USERDATA);
  if(Mess==WM_DROPFILES) 
  {
    POINT pt;
    GetCursorPos(&pt);
    This->MenuTarget=0;
    HMENU OpMenu=CreatePopupMenu();
    AppendMenu(OpMenu,MF_STRING,IDC_MOVEHERE,T("&Move Here"));
    AppendMenu(OpMenu,MF_STRING,IDC_COPYHERE,T("&Copy Here"));
    AppendMenu(OpMenu,MF_STRING,IDC_CREATESHORTCUT,T("Create &Shortcut(s) Here"));
    AppendMenu(OpMenu,MF_SEPARATOR,4099,NULL);
    AppendMenu(OpMenu,MF_STRING,4098,T("Cancel"));
    TrackPopupMenu(OpMenu,TPM_LEFTALIGN|TPM_LEFTBUTTON,pt.x,pt.y,0,This->Handle,NULL);
    DestroyMenu(OpMenu);
    MSG mess;
    while(PeekMessage(&mess,This->Handle,WM_COMMAND,WM_COMMAND,PM_REMOVE)) 
      DispatchMessage(&mess);
    EasyStr SelPath,SelLink;
    if(This->MenuTarget>=IDC_MOVEHERE&&This->MenuTarget<=IDC_CREATESHORTCUT) 
    {
      int nFiles=DragQueryFile((HDROP)wPar,0xffffffff,NULL,0);
      if(This->MenuTarget==IDC_CREATESHORTCUT) 
      {
        EasyStr File,Name,LinkFile;
        for(int i=0;i<nFiles;i++) 
        {
          File.SetLength(SSE_MAX_PATH);
          DragQueryFile((HDROP)wPar,i,File,SSE_MAX_PATH);
          Name.SetLength(SSE_MAX_PATH);
          GetLongPathName(File,Name,SSE_MAX_PATH);
          Name=EasyStr(GetFileNameFromPath(Name));
          char *dot=strrchr(Name,'.');
          if(dot!=NULL)
            if(ExtensionIsDisk(dot)||ExtensionIsPastiDisk(dot)) 
              *dot='\0';
          LinkFile=This->DisksFol+SLASH+Name+".lnk";
          int n=2;
          while(Exists(LinkFile))
            LinkFile=This->DisksFol+SLASH+Name+" ("+(n++)+").lnk";
          CreateLink(LinkFile,File);
          SelLink=LinkFile;
        }
      }
      else 
      {
#if defined(SSE_LONG_PATH)
        EasyStr sFrom;
        sFrom.SetLength(SSE_MAX_PATH*nFiles);
        char *From=sFrom.Text;
#else
        char *From=new char[MAX_PATH*nFiles+2];
#endif
        ZeroMemory(From,SSE_MAX_PATH*nFiles);
        char *FromPtr=From;
        for(int i=0;i<nFiles;i++) 
        {
          DragQueryFile((HDROP)wPar,i,FromPtr,SSE_MAX_PATH);
          // support links
          if(FileIsDisk(FromPtr)) 
            SelPath=This->DisksFol+SLASH+GetFileNameFromPath(FromPtr);
          FromPtr+=strlen(FromPtr)+1;
        }
        SHFILEOPSTRUCT fos;
        fos.hwnd=This->Handle;
        fos.wFunc=int((This->MenuTarget==IDC_MOVEHERE)?FO_MOVE:FO_COPY);
        fos.pFrom=From;
        fos.pTo=This->DisksFol;
        fos.fFlags=FILEOP_FLAGS(FOF_ALLOWUNDO)|FOF_RENAMEONCOLLISION;
        fos.hNameMappings=NULL;
        fos.lpszProgressTitle=LPSTR((This->MenuTarget==IDC_MOVEHERE) 
          ? StaticT("Moving...") : StaticT("Copying..."));
        EnableWindow(This->Handle,FALSE);
        SHFileOperation(&fos);
        EnableWindow(This->Handle,TRUE);
      }
      This->RefreshDiskView(SelPath,false,SelLink);
      SetForegroundWindow(This->Handle);
    }
    DragFinish((HDROP)wPar);
    return 0;
  }
  else if(Mess==WM_KEYDOWN && This->Dragging>-1)
    return 0;
  else if(Mess==WM_VSCROLL && This->Dragging>-1)
  {
    ImageList_DragLeave(This->Handle);
    LRESULT Ret=CallWindowProc(This->Old_ListView_WndProc,Win,Mess,wPar,lPar);
    UpdateWindow(Win);
    POINT mpt;
    GetCursorPos(&mpt);
    ScreenToClient(This->Handle,&mpt);
    ImageList_DragEnter(This->Handle,mpt.x-This->DragWidth,mpt.y-This->DragHeight);
    This->MoveDrag();
    return Ret;
  }
  return CallWindowProc(This->Old_ListView_WndProc,Win,Mess,wPar,lPar);
}
#endif//#if !defined(SSE_LIBRETRONUKE)

void TDiskManager::SetDriveViewEnable(int drive,bool EnableIt) {
  HWND LV=GetDlgItem(Handle,IDC_CONTENTA+drive);
  if(GetFocus()==LV) 
    SetFocus(DiskView);
  LONG colidx,Style=GetWindowLong(LV,GWL_STYLE);
  if(EnableIt)
  {
    Style|=WS_TABSTOP; // keyboard control
    colidx=COLOR_WINDOW;
  }
  else
  {
    Style&=~WS_TABSTOP;
    colidx=COLOR_BTNFACE;
  }
  SetWindowLong(LV,GWL_STYLE,Style);
  SendMessage(LV,LVM_SETBKCOLOR,0,(LPARAM)GetSysColor(colidx));
  InvalidateRect(LV,NULL,TRUE);
}


TDiskManFileInfo* TDiskManager::GetItemInf(int iItem,HWND LV/*=NULL*/) {
  LV_ITEM lvi;
  lvi.iItem=iItem;
  lvi.iSubItem=0;
  lvi.mask=LVIF_PARAM;
  lvi.lParam=0; // v4 anti-crash
  SendMessage((HWND)((LV!=NULL) ? LV : DiskView),LVM_GETITEM,0,(LPARAM)&lvi);
  return (TDiskManFileInfo*)lvi.lParam;
}


void TDiskManager::AdaptBackground() { // player can choose colour of DM, like GEM green, some like it
  int green=(SSEOptions.FullSpectrumPal) ? (IS_STE?255:252) : (IS_STE?240:224);
  switch(mBackground&3) { // mBackground is a BYTE meant to overflow!
  case 0: // white/white
    ListView_SetBkColor(DiskView,RGB(255,255,255));
    ListView_SetTextBkColor(DiskView,RGB(255,255,255));
    break;
  case 1: // white/green
    ListView_SetBkColor(DiskView,RGB(0,green,0));
    ListView_SetTextBkColor(DiskView,RGB(255,255,255));
    break;
  case 2: // transp./green
    ListView_SetBkColor(DiskView,RGB(0,green,0));
    ListView_SetTextBkColor(DiskView,CLR_NONE);
    break;
  case 3: // transp./grey
    ListView_SetBkColor(DiskView,CLR_NONE);
    ListView_SetTextBkColor(DiskView,CLR_NONE);
    break;
  }//sw
}

#endif//WIN32


#ifdef UNIX

int TDiskManager::dir_lv_notify_handler(hxc_dir_lv *dlv,int mess,INT_PTR i)
{
  TDiskManager *This=(TDiskManager*)dlv->owner;
  if (mess==DLVN_FOLDERCHANGE){
    This->set_path((char*)i,true,0);
    return 0;
  }else if (mess==DLVN_DOUBLECLICK || mess==DLVN_RETURN){
    if (This->DoubleClickAction==0 || i<0) return 0;

    EasyStr file=dlv->get_item_path(i);
    if ((GetFileAttributes(file) & FILE_ATTRIBUTE_DIRECTORY)==0){
      int action=2;
      if (This->DoubleClickAction==1) action=0;
      This->PerformInsertAction(action,dlv->get_item_name(i),file,"");
      return 1; // Don't focus the listview
    }
  }else if (mess==DLVN_DROP){
  	hxc_listview_drop_struct *ds=(hxc_listview_drop_struct*)i;
    EasyStr file=dlv->get_item_path(ds->dragged);
    int type=dlv->sl[ds->dragged].Data[DLVD_TYPE];

    if (dlv->lv.is_dropped_in(ds,&(This->HomeBut))){
    }else if (dlv->lv.is_dropped_in(ds,&(This->disk_name[0])) ||
              dlv->lv.is_dropped_in(ds,&(This->drive_icon[0]))){
      if (type>=2){
        This->PerformInsertAction(0,dlv->get_item_name(ds->dragged),file,"");
      }
    }else if (dlv->lv.is_dropped_in(ds,&(This->disk_name[1])) ||
              dlv->lv.is_dropped_in(ds,&(This->drive_icon[1]))){
      if (type>=2){
        This->PerformInsertAction(1,dlv->get_item_name(ds->dragged),file,"");
      }
    }
  }else if (mess==DLVN_CONTEXTMENU){
    dlv->pop.lpig=&Ico16;
    if (i>=0){
      EasyStr file=dlv->get_item_path(i);
      int is_link=dlv->sl[i].Data[DLVD_FLAGS] & DLVF_LINKMASK;
      int type=dlv->sl[i].Data[DLVD_TYPE];
      bool is_zip=type >= This->ArchiveTypeIdx;
      bool read_only=(dlv->sl[i].Data[DLVD_FLAGS] & DLVF_READONLY)!=0;
      int pos=0;
      if (is_link<2){ // 2 = dead link
        if (type>=2){ // File
          dlv->pop.menu.InsertAt(pos++,2,StripAndT("Insert Into Drive &A"),
            ICO16_INSERTDISK,1010);
          dlv->pop.menu.InsertAt(pos++,2,StripAndT("Insert Into Drive &B"),
            ICO16_INSERTDISK,1011);
          dlv->pop.menu.InsertAt(pos++,2,StripAndT("Insert, Reset and &Run"),
            ICO16_INSERTDISK,1012);
          dlv->pop.menu.InsertAt(pos++,2,"-",-1,0);
          dlv->pop.menu.InsertAt(pos++,2,StripAndT("Get &Contents"),-1,IDC_CONTENT);
#if defined(SSE_DISK_STW)
          dlv->pop.menu.InsertAt(pos++,2,StripAndT("Convert to ST&W"),-1,
            IDC_CONVERTSTW);
#endif          
          dlv->pop.menu.InsertAt(pos++,2,"-",-1,0);
          if (is_link){
            dlv->pop.menu.InsertAt(pos++,2,StripAndT("&Go To Disk"),
              ICO16_FORWARD,IDC_LINKGOTODISK);
            dlv->pop.menu.InsertAt(pos++,2,StripAndT("Open Disk's Folder in File Manager"),-1,IDC_OPENFOLDER);
            dlv->pop.menu.InsertAt(pos++,2,"-",-1,0);
          }
          
          if (is_zip==0){
            dlv->pop.menu.InsertAt(pos++,2,StripAndT("Read &Only"),
                                    (read_only ? ICO16_TICKED:ICO16_UNTICKED),IDC_READONLY);
            dlv->pop.menu.InsertAt(pos++,2,"-",-1,0);
          }else{
            dlv->pop.menu.InsertAt(pos++,2,StripAndT("E&xtract Disk Here"),ICO16_ZIP_RW,IDC_EXTRACT);
            dlv->pop.menu.InsertAt(pos++,2,"-",-1,0);
          }
        }else{ // Folder
          int pos=0;
          dlv->pop.menu.InsertAt(pos++,2,StripAndT("Open in File Manager"),-1,IDC_EXPLORER);
          dlv->pop.menu.InsertAt(pos++,2,StripAndT("&Find..."),-1,IDC_FIND);
          dlv->pop.menu.InsertAt(pos++,2,"-",-1,0);
        }
      }
    }
    int pos=dlv->pop.menu.NumStrings-1; // New folder
    dlv->pop.menu[pos].Data[0]=ICO16_FOLDER;
    dlv->pop.menu.InsertAt(pos++,2,StripAndT("Open Current Folder In File Manager"),-1,IDC_FOLEXPLORER);
    dlv->pop.menu.InsertAt(pos++,2,StripAndT("Find In Current Folder"),-1,IDC_FINDCUR);
    dlv->pop.menu.InsertAt(pos++,2,"-",-1,0);  
    dlv->pop.menu.Add(StripAndT("New Standard &Disk Image"),ICO16_DISK,IDC_NEWST);
    dlv->pop.menu.Add(StripAndT("New Custom Disk &Image"),ICO16_DISK,IDC_NEWCUSTOM);
#if defined(SSE_DISK_STW) //new context option
    dlv->pop.menu.Add(StripAndT("New ST&W Disk Image"),ICO16_DISK,IDC_NEWSTW);
#endif
#if defined(SSE_DISK_HFE) //new context option
    dlv->pop.menu.Add(StripAndT("New &HFE Disk Image"),ICO16_DISK,IDC_NEWHFE);
#endif

  }
  else if(mess==DLVN_POPCHOOSE)
  {
    if(dlv->pop.menu[i].NumData<2)
      return 0;
    int action=dlv->pop.menu[i].Data[1];
    switch (action){
      case IDC_INSERTA:
      case IDC_INSERTB:
      case IDC_INSERTRUN:
        
        This->PerformInsertAction(action-IDC_INSERTA,
                    dlv->get_item_name(dlv->lv.sel),
                    dlv->get_item_path(dlv->lv.sel),"");
        return 0;
      case IDC_CONTENT:
        This->GetContentsSL(dlv->get_item_path(dlv->lv.sel));
        if (This->contents_sl.NumStrings){
          This->ContentsLinksPath=This->DisksFol;
          This->ShowContentDiag();
        }
        return 0;
#if defined(SSE_DISK_STW)
/*  Create a new STW disk image, and copy the data from a DIM, MSA or ST
    image into it
*/
      case IDC_CONVERTSTW:
      {
        char src[MAX_PATH],dest[MAX_PATH];
        strcpy(src,dlv->get_item_path(dlv->lv.sel).Text); // once (each time it creates an EasyStr)
        strcpy(dest,src);
        char *dot=strrchr(dest,'.');
        if(dot)
          strcpy(dot+1,"STW");
        bool save1=OPTION_AUTOSTW;
        bool save2=FloppyDisk[2].ReadOnly;
        FloppyDisk[2].ReadOnly=false;
        OPTION_AUTOSTW=false; // don't want to autoconvert it!
        int Error=FloppyDrive[2].SetDisk(src,"");
        if(Error==ERR_OK)
        {
          if(FloppyDrive[2].ImageType.Manager==MNGR_STEEM) //DIM, MSA or ST
          {
            //TRACE_LOG("Creating %s\n",GetFileNameFromPath(dest));
            STtoSTW(2,dest);
            FloppyDisk[2].WrittenTo=true;
            ImageSTW[2].Close();
          }
#if defined(SSE_DISK_SCP2STW)
          else if(FloppyDrive[2].ImageType.Extension==EXT_SCP)
          {
            if(FloppyDisk[2].IsZip())
              strcpy(src,FloppyDisk[2].ZipTempFile.Text);
            SCPtoSTW(src,dest);
            FloppyDisk[2].WrittenTo=true;
          }
#endif          
          FloppyDrive[2].RemoveDisk(true);
          This->RefreshDiskView(dest);
        }//!err
        OPTION_AUTOSTW=save1;
        FloppyDisk[2].ReadOnly=save2;
        return 0;
      }
#endif        
      case IDC_READONLY:
        This->ToggleReadOnly(dlv->lv.sel);
        return 0;
      case IDC_EXPLORER:case IDC_OPENFOLDER:
      {
        Str fol=dlv->get_item_path(dlv->lv.sel);
        if (action==IDC_OPENFOLDER) RemoveFileNameFromPath(fol,KEEP_SLASH);
        shell_execute(Comlines[COMLINE_FM],Str("[PATH]\n")+fol);
        return 0;
      }
      case IDC_FIND:
        shell_execute(Comlines[COMLINE_FIND],Str("[PATH]\n")+dlv->get_item_path(dlv->lv.sel));
        return 0;
      case IDC_EXTRACT:
        This->ExtractDisks(dlv->get_item_path(dlv->lv.sel));
        return 0;
      case IDC_LINKGOTODISK:
      {
        Str File=dlv->get_item_path(dlv->lv.sel,true);
        Str DiskFol=File;
        RemoveFileNameFromPath(DiskFol,REMOVE_SLASH);
        This->set_path(DiskFol);
        dlv->select_item_by_name(GetFileNameFromPath(File));
        return 0;
      }
      case IDC_NEWST:
      case IDC_NEWCUSTOM:
      {
        EasyStr STName;
        int sectors=1440,secs_per_track=9,sides=2;
        if (action==IDC_NEWCUSTOM){
          STName=This->GetCustomDiskImage(&sectors,&secs_per_track,&sides);
        }else{
          hxc_prompt prompt;
          STName=prompt.ask(XD,T("Blank Disk"),T("Enter Name"));
        }
        if (STName.NotEmpty()){
          STName=GetUniquePath(This->DisksFol,STName+".st");
          if (This->CreateDiskImage(STName,sectors,secs_per_track,sides)){
            This->RefreshDiskView(STName);
          }else{
            Alert(Str(T("Could not create the disk image "))+STName,
                      T("ERROR"),MB_ICONEXCLAMATION);
          }
        }
        return 0;
      }
#if defined(SSE_DISK_STW)
      case IDC_NEWSTW:  // STW
      {
        hxc_prompt prompt;
        EasyStr STName=prompt.ask(XD,T("STW Disk"),T("Enter Name"));
        if (STName.NotEmpty()){
          STName=GetUniquePath(This->DisksFol,STName+".stw");
          if(ImageSTW[0].Create(STName,2)) 
          {
            This->RefreshDiskView(STName);
          }else{
            Alert(Str(T("Could not create the disk image "))+STName,
                      T("ERROR"),MB_ICONEXCLAMATION);
          }
        }
        return 0;
      }
#endif
#if defined(SSE_DISK_HFE)
      case IDC_NEWHFE:  // HFE
      {
        hxc_prompt prompt;
        EasyStr STName=prompt.ask(XD,T("HFE Disk"),T("Enter Name"));
        if (STName.NotEmpty()){
          STName=GetUniquePath(This->DisksFol,STName+".hfe");
          if(ImageHFE[0].Create(STName)) {
            This->RefreshDiskView(STName);
          }else{
            Alert(Str(T("Could not create the disk image "))+STName,
                      T("ERROR"),MB_ICONEXCLAMATION);
          }
        }
        return 0;
      }
#endif
      case IDC_FOLEXPLORER:
        shell_execute(Comlines[COMLINE_FM],Str("[PATH]\n")+This->DisksFol);
        return 0;
      case IDC_FINDCUR:
        shell_execute(Comlines[COMLINE_FIND],Str("[PATH]\n")+This->DisksFol);
        return 0;
    }
  }else if (mess==DLVN_CONTENTSCHANGE){
    dlv_ccn_struct *p_ccn=(dlv_ccn_struct*)i;
    if (p_ccn->time==DLVCCN_BEFORE){
      This->TempEject_InDrive[0]=0;
      This->TempEject_InDrive[1]=0;
      for (int d=0;d<2;d++){
        // Should check whether in folder being deleted too.
        if (IsSameStr_I(FloppyDrive[d].GetDisk(),p_ccn->path)){
          This->TempEject_InDrive[d]=true;
          This->TempEject_Name=FloppyDisk[d].DiskName;
          This->TempEject_DiskInZip[d]=FloppyDisk[d].DiskInZip;
          FloppyDrive[d].RemoveDisk();
        }
      }
    }else if (p_ccn->time==DLVCCN_AFTER){
      Str new_path=p_ccn->path;
      Str new_name=This->TempEject_Name;
      if (p_ccn->success){
        if (p_ccn->action==DLVCCN_DELETE){
          This->UpdateDiskNames(0);
          This->UpdateDiskNames(1);
          return 0;
        }else if (p_ccn->action==DLVCCN_MOVE || p_ccn->action==DLVCCN_RENAME){
          new_path=p_ccn->new_path;
          if (p_ccn->action==DLVCCN_RENAME){
            new_name=GetFileNameFromPath(new_path);
            if (p_ccn->flags & DLVF_EXTREMOVED){
              char *dot=strrchr(new_name,'.');
              if (dot) *dot='\0';
            }
          }
        }
      }
      for (int d=0;d<2;d++){
        if (This->TempEject_InDrive[d]){
          This->InsertDisk(d,new_name,new_path,false,false,
            This->TempEject_DiskInZip[d]);
        }
      }
    }
    return 0;
  // intercept a & b... (bad idea?)
  }else if (mess==DLVN_KEYPRESS){
    if(i=='a'||i=='b')
    {
      EasyStr file=dlv->get_item_path(dlv->lv.sel);
      if ((GetFileAttributes(file) & FILE_ATTRIBUTE_DIRECTORY)==0){
      This->PerformInsertAction(i-'a',dlv->get_item_name(dlv->lv.sel),file,"");
      return 1; // "don't handle"
    }
  }

  }
  return 0;
}
//---------------------------------------------------------------------------
void TDiskManager::UpdateDiskNames(int d)
{
  if (XD==NULL || Handle==0) return;

  if (FloppyDrive[d].DiskInDrive()){
    Str RO;
    if (FloppyDisk[d].ReadOnly) RO=" <RO>";
    disk_name[d].set_text(FloppyDisk[d].DiskName+RO);
  }else{
    disk_name[d].set_text("");
  }
}
//---------------------------------------------------------------------------
void TDiskManager::set_home(Str fol)
{
  if (Alert(fol+"/"+"\n\n"+
        T("Are you sure you want to make this folder your new home folder?"),
        T("Change Home Folder?"),MB_YESNO | MB_ICONQUESTION)==IDYES){
    HomeFol=fol;
  }
}
//---------------------------------------------------------------------------
int TDiskManager::button_notify_handler(hxc_button *But,int mess,int *Inf)
{
  TDiskManager *This=(TDiskManager*)GetProp(But->XD,But->parent,cWinThis);
  if (mess==BN_CLICKED){
    switch (But->id){
      case 2: //back
      	if (This->HistBackLength > 0){
          for (int n=DM_HISTORY_LEN-1;n>0;n--){
            This->HistForward[n]=This->HistForward[n-1];
          }
          This->HistForward[0]=This->DisksFol;
          if((This->HistForwardLength)<DM_HISTORY_LEN)
            This->HistForwardLength++;

          This->set_path(This->HistBack[0],0);
          for (int n=0;n<DM_HISTORY_LEN-1;n++){
            This->HistBack[n]=This->HistBack[n+1];
          }
          This->HistBack[DM_HISTORY_LEN-1]="";
          This->HistBackLength--;
				}
      	break;
      case 3: //forward
      	if (This->HistForwardLength > 0){
          for (int n=DM_HISTORY_LEN-1;n>0;n--){
            This->HistBack[n]=This->HistBack[n-1];
          }
          if((This->HistBackLength)<DM_HISTORY_LEN)
            This->HistBackLength++;
          This->HistBack[0]=This->DisksFol;
          This->set_path(This->HistForward[0],0);
          for (int n=0;n<DM_HISTORY_LEN-1;n++){
            This->HistForward[n]=This->HistForward[n+1];
          }
          This->HistForward[DM_HISTORY_LEN-1]="";
          This->HistForwardLength--;
				}
      	break;
      case 4: //go home
      {
        bool at_home=IsSameStr(This->HomeFol.Text,This->DisksFol.Text);
        if (Inf[0]!=Button1 || at_home){
          But->set_check(true);
          pop.lpig=&Ico16;
          pop.menu.DeleteAll();
          if (at_home==0){
            pop.menu.Add(This->HomeFol,ICO16_HOMEFOLDER,4000);
            pop.menu.Add("-",-1);
          }
          for (int i=0;i<DM_HISTORY_LEN;i++){
            pop.menu.Add(Str(i+1)+": "+This->QuickFol[i],ICO16_FOLDER,4010+i);
          }
          pop.create(XD,But->handle,0,But->h,This->menu_popup_notifyproc,This);
        }else{
          This->set_path(This->HomeFol,true);
        }
        break;
      }
      case 5: //set home
      {
        bool at_home=IsSameStr(This->HomeFol.Text,This->DisksFol.Text);
        if (Inf[0]!=Button1 || at_home){
          But->set_check(true);
          pop.lpig=&Ico16;
          pop.menu.DeleteAll();
          if (at_home==0){
            pop.menu.Add(Str("(")+This->HomeFol+")",ICO16_HOMEFOLDER,4100);
            pop.menu.Add("-",-1);
          }
          for (int i=0;i<DM_HISTORY_LEN;i++){
            pop.menu.Add(Str(i+1)+": ("+This->QuickFol[i]+")",ICO16_FOLDER,4110+i);
          }
          pop.create(XD,But->handle,0,But->h,This->menu_popup_notifyproc,This);
        }else{
          This->set_home(This->DisksFol);
        }
      	break;
			}
			case 6:
      {
    		But->set_check(true);
    		pop.lpig=&Ico16;
      	pop.menu.DeleteAll();
        pop.menu.Add(StripAndT("Connect Drive B"),
        	int((This->nFloppyDrives>1) ? ICO16_TICKED:ICO16_UNTICKED),IDC_CONNECT_DRIVEB);
        pop.menu.Add(StripAndT("Turbo Drive (hack)"),
          int((This->bTurboDrive!=0) ? ICO16_TICKED:ICO16_UNTICKED),IDC_TURBODRIVE);
#if defined(SSE_DISK_AUTOSTW)
        pop.menu.Add(StripAndT("MFM emulation"),
        	int(OPTION_AUTOSTW ? ICO16_TICKED:ICO16_UNTICKED),IDC_MFMEMU);
#endif 
        pop.menu.Add(StripAndT("Count DMA transfer cycles"), //412
        	int(OPTION_COUNT_DMA_CYCLES ? ICO16_TICKED:ICO16_UNTICKED),IDC_DMACYCLES);
        pop.menu.Add(StripAndT("Read/Write Archives (Changes Lost On Eject)"),
        	int(This->bArchiveRW ? ICO16_TICKED:ICO16_UNTICKED),IDC_ARCHIRW);
        pop.menu.Add(StripAndT("Protect image files"),
        	int(This->bDiskProtectImage ? ICO16_TICKED:ICO16_UNTICKED),IDC_PROTECTIMAGES);
#if defined(SSE_DISK_GHOST)
        pop.menu.Add(StripAndT("Enable ghost disks for protected disks"),
        	int(OPTION_GHOST_DISK ? ICO16_TICKED:ICO16_UNTICKED),IDC_GHOSTDISK);
        pop.menu.Add(StripAndT("Also read-only and zip disks"),
        	int(SSEOptions.GhostDiskRO ? ICO16_TICKED:ICO16_UNTICKED),IDC_GHOSTDISKRO);
#endif
#if defined(SSE_TOS_PRG_AUTORUN)
        pop.menu.Add(StripAndT("Run PRG and TOS files"),
        	int(OPTION_PRG_SUPPORT ? ICO16_TICKED:ICO16_UNTICKED),IDC_RUNPRG);
#endif
        pop.menu.Add(StripAndT("Hide Extensions"),
        	int((This->HideExtension) ? ICO16_TICKED:ICO16_UNTICKED),IDC_HIDE_EXT);
        pop.menu.Add("-",-1,0);
        pop.menu.Add(StripAndT("Search Disk Image Database"),-1,IDC_DATABASE);
        pop.menu.Add(StripAndT("Open Current Folder In File Manager"),-1,IDC_FOLEXPLORER);
        pop.menu.Add(StripAndT("Find In Current Folder"),-1,IDC_FINDCUR);
        pop.menu.Add("-",-1,0);
        pop.menu.Add(StripAndT("Automatically Insert &Second Disk"),int(This->AutoInsert2 ? ICO16_TICKED:ICO16_UNTICKED),IDC_AUTOINSERTB);
//        pop.menu.Add(StripAndT("Hide Dangling Links"),int(This->HideBroken),IDC_HIDE_ARCHIVE);
        pop.menu.Add(StripAndT("E&ject Disks When Quit"),
        	int(This->EjectDisksWhenQuit ? ICO16_TICKED:ICO16_UNTICKED),IDC_AUTOEJECT);
#if defined(SSE_DISK_SWAPPER)
        pop.menu.Add(StripAndT("Disk S&wapper Based on Name"),
        	int(This->bSwapperPattern ? ICO16_TICKED:ICO16_UNTICKED),IDC_PATTERNS);
#endif          
        pop.menu.Add("-",-1,0);
        int idx=pop.menu.NumStrings;
        pop.menu.Add(StripAndT("Double Click On Disk Does &Nothing"),ICO16_UNRADIOMARKED,IDC_DBCLICK_NONE);
        pop.menu.Add(StripAndT("Double Click On Disk Inserts In &Drive A"),ICO16_UNRADIOMARKED,IDC_DBCLICK_INSERTA);
        pop.menu.Add(StripAndT("Double Click On Disk Inserts, &Resets and Runs"),ICO16_UNRADIOMARKED,IDC_DBCLICK_RUN);
        pop.menu[idx+This->DoubleClickAction].Data[0]=ICO16_RADIOMARK;
        pop.menu.Add("-",-1,0);
        pop.menu.Add(StripAndT("&Close Disk Manager After Insert, Reset and Run"),
                  int(This->CloseAfterIRR ? ICO16_TICKED:ICO16_UNTICKED),IDC_AUTOCLOSE);
        pop.create(XD,But->handle,0,But->h,This->menu_popup_notifyproc,This);
				break;
      }
			case IDC_HDGEMDOS:
				HardDiskMan.Show();
				break;
#if defined(SSE_ACSI_MNGR)
      case IDC_HDACSI:
        AcsiHardDiskMan.Show();
        break;
#endif        
      case IDC_CONTENTA:
      case IDC_CONTENTB:
        if(Inf[0]==3) //1=left, 2=middle, 3=right
        {
          // popup menu for freeboot etc.
          int disk=But->id-IDC_CONTENTA;
          But->set_check(true); //?
          pop.lpig=&Ico16;
          pop.menu.DeleteAll();
#if defined(SSE_DRIVE_SINGLESIDE)
          pop.menu.Add(StripAndT("Single-sided drive"),
            (int)((FloppyDrive[disk].bSingleSided) ? ICO16_TICKED:ICO16_UNTICKED),
            IDC_SINGLESIDE+disk);
#endif
#if defined(SSE_DRIVE_FREEBOOT)
          pop.menu.Add(StripAndT("Freeboot"),
            (int)((FloppyDrive[disk].bFreeboot) ? ICO16_TICKED:ICO16_UNTICKED),IDC_FREEBOOT+disk);
#endif
          if(OPTION_HACKS && FloppyDrive[disk].bMotor)
            pop.menu.Add(StripAndT("Stop motor"),-1,IDC_STOPMOTOR+disk);
#if defined(SSE_DRIVE_SOUND)
          pop.menu.Add(StripAndT("Choose drive sound directory"),-1,IDC_SOUNDDIR+disk);
#endif
          pop.create(XD,But->handle,0,But->h,This->menu_popup_notifyproc,This);
        }
        else if(Inf[0]==1 && But->id==101)
          This->SetNumFloppies(3-This->nFloppyDrives);
        break;
      case 200:case 201:
      {
        if (Inf[0]!=Button3 && Inf[0]!=Button2) break;

        pop.lpig=&Ico16;
        pop.menu.DeleteAll();

        int d=But->id-200;
        bool added_line=true;
        if (FloppyDrive[d].NotEmpty()){
          if (FloppyDisk[d].IsZip()==0){
            int ico=ICO16_UNTICKED;
            if (FloppyDisk[d].ReadOnly) ico=ICO16_TICKED;
            pop.menu.Add(StripAndT("Read &Only"),ico,IDC_READONLY,(-d)-1);
            ico=(FloppyDisk[d].WriteProtect)?ICO16_TICKED:ICO16_UNTICKED;
            pop.menu.Add(StripAndT("&Write protect"),ico,
              IDC_WRITEPROTECT+d,d);
          }
        }
        if (FloppyDrive[DRIVE_A].NotEmpty() || FloppyDrive[DRIVE_B].NotEmpty()){
          pop.menu.Add(StripAndT("&Swap A: and B:"),-1,IDC_SWAP);
          added_line=0;
        }
        if (FloppyDrive[d].NotEmpty()){
          pop.menu.Add(StripAndT("&Remove Disk From Drive"),ICO16_EJECTDISK,IDC_EJECT,d);
          pop.menu.Add(StripAndT("&Go To Disk"),-1,IDC_GOTODISK,d);
#if defined(SSE_DISK_SWAPPER)
          pop.menu.Add(StripAndT("&Previous disk"),-1,IDC_PREVIOUS,d);
          pop.menu.Add(StripAndT("&Next disk"),-1,IDC_NEXT,d);
#endif
        }
        EasyStr CurrentDiskName=This->CreateDiskName(FloppyDisk[d].DiskName,
          FloppyDisk[d].DiskInZip);
        for (int n=0;n<DM_HISTORY_LEN;n++){
          if (This->InsertHist[d][n].Path.NotEmpty()){
            EasyStr MenuItemText=This->CreateDiskName(This->InsertHist[d][n].Name,This->InsertHist[d][n].DiskInZip);
            if (NotSameStr_I(CurrentDiskName,MenuItemText)){
              if (added_line==0){
                pop.menu.Add("-",-1,0,0);
                added_line=true;
              }
              pop.menu.Add(MenuItemText,ICO16_INSERTDISK,3000+n,d);
            }
          }
        }
        if (pop.menu.NumStrings) pop.create(XD,0,POP_CURSORPOS,0,This->menu_popup_notifyproc,This);
        break;
      }
      case 302:case 303:
        This->EjectDisk(But->id & 1);
        break;
    }
  }
  return 0;
}
//---------------------------------------------------------------------------
int TDiskManager::menu_popup_notifyproc(hxc_popup *pPop,int mess,INT_PTR i)
{
  TDiskManager *This=(TDiskManager*)(pPop->owner);
	int id=pop.menu[i].Data[1];
	if (mess==POP_CHOOSE){
		switch (id){
      case IDC_HIDE_ARCHIVE:
        This->HideBroken=!This->HideBroken;
        This->dir_lv.show_broken_links=(This->HideBroken==0);
        This->RefreshDiskView();
        break;
#if defined(SSE_DISK_SWAPPER)
      case IDC_PATTERNS:
        This->bSwapperPattern=!This->bSwapperPattern;
        break;
#endif        
      case IDC_DBCLICK_NONE:case IDC_DBCLICK_INSERTA:case IDC_DBCLICK_RUN:
        This->DoubleClickAction=id-IDC_DBCLICK_NONE;
        break;
      case IDC_AUTOEJECT:
        This->EjectDisksWhenQuit=!This->EjectDisksWhenQuit;
        break;
      case IDC_CONNECT_DRIVEB:
        This->SetNumFloppies(3-This->nFloppyDrives);
        break;
      case IDC_TURBODRIVE:
        This->bTurboDrive=!This->bTurboDrive;
        CheckResetDisplay();
        FloppyDrive[DRIVE_A].UpdateAdat();
        FloppyDrive[DRIVE_B].UpdateAdat();        
        break;
      case IDC_ARCHIRW:
      {
        This->bArchiveRW=!This->bArchiveRW;
        int zipicon=ICO16_ZIP_RO;
        if(This->bArchiveRW)
          zipicon=ICO16_ZIP_RW;
        for (int i=This->ArchiveTypeIdx;i<This->dir_lv.ext_sl.NumStrings;i++){
          This->dir_lv.ext_sl[i].Data[0]=zipicon;
        }
        This->UpdateDiskNames(0);
        This->UpdateDiskNames(1);
        This->RefreshDiskView();
        break;
      }
      case IDC_PROTECTIMAGES:
        This->bDiskProtectImage=!This->bDiskProtectImage;
        if(This->bDiskProtectImage)
          FloppyDisk[DRIVE_A].WrittenTo=FloppyDisk[DRIVE_B].WrittenTo=false;
        break;
#if defined(SSE_DISK_AUTOSTW)
      case IDC_MFMEMU:
        for(int i=DRIVE_A;i<=DRIVE_B;i++) // reinsert disks, like for Pasti
        {
          if(FloppyDrive[i].NotEmpty())
          {
            EasyStr name=FloppyDisk[i].DiskName;
            EasyStr path=FloppyDisk[i].GetImageFile();
            This->EjectDisk(i);
            OPTION_AUTOSTW=!OPTION_AUTOSTW;
            This->InsertDisk(i,name,path,false,false,"",true);
            OPTION_AUTOSTW=!OPTION_AUTOSTW;
          }
          else FloppyDrive[i].ImageType.Manager=(BYTE)((pasti_active)
            ? MNGR_PASTI : (OPTION_AUTOSTW ? MNGR_WD1772 : MNGR_STEEM));
        }      
        OPTION_AUTOSTW=!OPTION_AUTOSTW;
        break;
#endif
      case IDC_DMACYCLES:
        OPTION_COUNT_DMA_CYCLES=!OPTION_COUNT_DMA_CYCLES;
        break;
#if defined(SSE_DISK_GHOST)
      case IDC_GHOSTDISK:
        OPTION_GHOST_DISK=!OPTION_GHOST_DISK;
        //TRACE_LOG("Option Ghost disk %d\n",OPTION_GHOST_DISK);
        break;
#endif
      case IDC_GHOSTDISKRO:
        SSEOptions.GhostDiskRO=!SSEOptions.GhostDiskRO;
        break;
#if defined(SSE_TOS_PRG_AUTORUN)
      case IDC_RUNPRG:
        OPTION_PRG_SUPPORT=!OPTION_PRG_SUPPORT;
        TRACE_LOG("Option PRG support %d\n",OPTION_GHOST_DISK);
        This->RefreshDiskView();
        break;
#endif        
      case IDC_AUTOCLOSE:
        This->CloseAfterIRR=!This->CloseAfterIRR;
        break;
      case IDC_AUTOINSERTB:
        This->AutoInsert2=!This->AutoInsert2;
        break;
      case IDC_DATABASE:
        This->ShowDatabaseDiag();
        break;
      case IDC_HIDE_EXT:
        This->HideExtension=!This->HideExtension;
        This->dir_lv.HideExtension=This->HideExtension;
        This->RefreshDiskView("");
        break;
      case IDC_READONLY:  // Toggle Read-Only
        This->ToggleReadOnly(pop.menu[i].Data[2]);
        break;
      case IDC_WRITEPROTECT:
      case (IDC_WRITEPROTECT+1):
      {
        int floppy_no=pop.menu[i].Data[2];
        FloppyDisk[floppy_no].WriteProtect=!FloppyDisk[floppy_no].WriteProtect;
        FloppyDrive[floppy_no].ReinsertDisk();
        break;
      }
      case IDC_SWAP:
        This->SwapDisks(0);
        break;
      case IDC_EJECT:
        This->EjectDisk(pop.menu[i].Data[2]);
        break;
      case IDC_GOTODISK:
      {
        int d=pop.menu[i].Data[2]; // Get index
        EasyStr DiskFol=FloppyDrive[d].GetDisk();
        RemoveFileNameFromPath(DiskFol,REMOVE_SLASH);
        This->set_path(DiskFol);
        This->dir_lv.select_item_by_name(GetFileNameFromPath(FloppyDrive[d].GetDisk()));
        break;
      }
#if defined(SSE_DISK_SWAPPER)
      case IDC_PREVIOUS: // Previous disk
        This->ChangeDisk(pop.menu[i].Data[2],-1,FALSE);
        break;
      case IDC_NEXT: // Next disk
        This->ChangeDisk(pop.menu[i].Data[2],1,FALSE);
        break;
#endif      
      case IDC_FOLEXPLORER:
        shell_execute(Comlines[COMLINE_FM],Str("[PATH]\n")+This->DisksFol);
        break;
      case IDC_FINDCUR:
        shell_execute(Comlines[COMLINE_FIND],Str("[PATH]\n")+This->DisksFol);
        break;
      // drive context menu
      case IDC_STOPMOTOR:
      case (IDC_STOPMOTOR+1): // hack, some programs leave the motor on
      {
        int floppy_no=id-IDC_STOPMOTOR;
        FloppyDrive[floppy_no].bMotor=false;
        Fdc.str&=~FDC_STR_MO;
        agenda_delete(agenda_fdc_motor_flag_off);
        break;
      }
#if defined(SSE_DRIVE_SINGLESIDE)
      case IDC_SINGLESIDE:
      case (IDC_SINGLESIDE+1):
      {
        int floppy_no=id-IDC_SINGLESIDE;
        FloppyDrive[floppy_no].bSingleSided=!FloppyDrive[floppy_no].bSingleSided;
        break;
      }
#endif
#if defined(SSE_DRIVE_FREEBOOT)
      case IDC_FREEBOOT:
      case (IDC_FREEBOOT+1):
      {
        int floppy_no=id-IDC_FREEBOOT;
        FloppyDrive[floppy_no].bFreeboot=!FloppyDrive[floppy_no].bFreeboot;
        Psg.CheckFreeboot();
        break;
      }
#endif
#if defined(SSE_DRIVE_SOUND)
      case IDC_SOUNDDIR:
      case (IDC_SOUNDDIR+1):
      {
        int floppy_no=id-IDC_SOUNDDIR;
        EasyStr NewFol=fileselect.choose(XD,DriveSoundDir[floppy_no],"",T("Pick a Folder"),
		   	  FSM_CHOOSE_FOLDER | FSM_CONFIRMCREATE,folder_parse_routine,"");
        if(NewFol.NotEmpty()&&NotSameStr_I(NewFol,DriveSoundDir[floppy_no]))
        {
          NO_SLASH(NewFol);
          DriveSoundDir[floppy_no]=NewFol;
        }
        break;
      }
#endif
		}
    if (id>=3000 && id<3030){
      int disk=pop.menu[i].Data[2];
      int n=id-3000;
      This->InsertDisk(disk,This->InsertHist[disk][n].Name,This->InsertHist[disk][n].Path,
                        false,true,This->InsertHist[disk][n].DiskInZip,false,true);
    }else if (id>=4000 && id<4100){
      if (id==4000){
        This->set_path(This->HomeFol,true);
      }else{
        id-=4010;
        if (This->QuickFol[id].NotEmpty()) This->set_path(This->QuickFol[id],true);
      }
    }else if (id>=4100 && id<4200){
      if (id==4100){
        This->set_home(This->DisksFol);
      }else{
        id-=4110;
        This->QuickFol[id]=This->DisksFol;
      }
    }
	}
  This->MenuBut.set_check(0);
  This->HomeBut.set_check(0);
  This->SetHomeBut.set_check(0);
	return 0;
}
//---------------------------------------------------------------------------
void TDiskManager::ToggleReadOnly(int i)
{
  EasyStr DiskPath;
  if (i<0){
    DiskPath=FloppyDrive[-(i+1)].GetDisk();
  }else{
    DiskPath=dir_lv.get_item_path(i,true);
  }

  bool InDrive[2]={0,0};
  EasyStr OldName[2],DiskInZip[2];
  for (int d=0;d<2;d++){
    if (IsSameStr_I(FloppyDrive[d].GetDisk(),DiskPath)){
      InDrive[d]=true;
      OldName[d]=FloppyDisk[d].DiskName;
      DiskInZip[d]=FloppyDisk[d].DiskInZip;
      FloppyDrive[d].RemoveDisk();
    }
  }
  DWORD Attrib=GetFileAttributes(DiskPath);
  if (Attrib & FILE_ATTRIBUTE_READONLY){
    SetFileAttributes(DiskPath,Attrib & ~FILE_ATTRIBUTE_READONLY);
  }else{
    SetFileAttributes(DiskPath,Attrib | FILE_ATTRIBUTE_READONLY);
  }

  if (InDrive[0]) InsertDisk(0,OldName[0],DiskPath,false,false,DiskInZip[0]);
  if (InDrive[1]) InsertDisk(1,OldName[1],DiskPath,false,false,DiskInZip[1]);

  RemoveFileNameFromPath(DiskPath,REMOVE_SLASH);
  if (IsSameStr(DiskPath,DisksFol)) RefreshDiskView();
}
//---------------------------------------------------------------------------
void TDiskManager::RefreshDiskView(Str sel)
{
	set_path(DisksFol,0);
  if (sel.NotEmpty()){
    dir_lv.select_item_by_name(GetFileNameFromPath(sel));
  }
}
//---------------------------------------------------------------------------
void TDiskManager::set_path(EasyStr new_path,bool add_to_history,bool change_dir_lv)
{
  if (add_to_history){
    HistForward[0]="";
    HistForwardLength=0;
    for (int n=DM_HISTORY_LEN-1;n>0;n--) HistBack[n]=HistBack[n-1];
    HistBack[0]=DisksFol;
    if (HistBackLength<DM_HISTORY_LEN)
      HistBackLength++;
  }

	NO_SLASH(new_path);
  if (change_dir_lv){
    dir_lv.fol=new_path;
    dir_lv.refresh_fol();
  }
  DisksFol=new_path;
  DirOutput.set_text(DisksFol);
}

//---------------------------------------------------------------------------
int TDiskManager::WinProc(TDiskManager *This,Window Win,XEvent *Ev)
{
  switch (Ev->type){
    case ClientMessage:
      if (Ev->xclient.message_type==hxc::XA_WM_PROTOCOLS){
        if (Atom(Ev->xclient.data.l[0])==hxc::XA_WM_DELETE_WINDOW){
          This->Hide();
        }
      }
      break;
    case ConfigureNotify:
    {
      XWindowAttributes wa;
      XGetWindowAttributes(XD,Win,&wa);

      This->Width=wa.width;This->Height=wa.height;
      int w=This->Width;int h=This->Height;

      for(int d=0;d<2;d++)
      {
#if defined(SSE_ACSI_MNGR)
        XResizeWindow(XD,This->disk_name[d].handle,MAX(w-(10+32 + 25+10+70*2+10),
          10),25);
#else        
  	    XResizeWindow(XD,This->disk_name[d].handle,MAX(w-(10+32 + 25+10+60+10),10),25);
#endif
     	}
      XResizeWindow(XD,This->DirOutput.handle,MAX(w-145,30),25);
      XResizeWindow(XD,This->dir_lv.lv.handle,MAX(w-20,10),MAX(h-120,10));

      XSync(XD,0);
      break;
    }
  }
  return PEEKED_MESSAGE;
}
//---------------------------------------------------------------------------
Str TDiskManager::GetCustomDiskImage(int *pSectors,int *pSecsPerTrack,int *pSides)
{
  int w=300,h=10+35+35+35+25+10;
  Window handle=hxc::create_modal_dialog(XD,w,h,T("Create Custom Disk Image"),true);
  if (handle==0) return "";

  hxc_edit *p_ed;
  hxc_dropdown *p_sides_dd,*p_tracks_dd,*p_secs_dd;

  int y=10,x=10,hw=(w-20)/2,tw;
  hxc_button *p_but=new hxc_button(XD,handle,x,y,0,25,NULL,this,BT_LABEL,T("Name"),0,hxc::col_bk);
  x+=p_but->w+5;

  p_ed=new hxc_edit(XD,handle,x,y,w-10-x,25,NULL,this);
  p_ed->set_text(T("Blank Disk"),true);
  y+=35;

  x=10;
  new hxc_button(XD,handle,x,y,0,25,NULL,this,BT_LABEL,T("Sides"),0,hxc::col_bk);
  x+=hw;

  p_sides_dd=new hxc_dropdown(XD,handle,x,y,hw,200,NULL,this);
  p_sides_dd->additem("1",1);
  p_sides_dd->additem("2",2);
  p_sides_dd->sel=1;
  p_sides_dd->changesel(SidesIdx);
  y+=35;

  x=10;
  new hxc_button(XD,handle,x,y,0,25,NULL,this,BT_LABEL,T("Tracks"),0,hxc::col_bk);
  x+=hw;

  tw=hxc::get_text_width(XD,T("0 to "));
  new hxc_button(XD,handle,x-tw,y,0,25,NULL,this,BT_LABEL,T("0 to "),0,hxc::col_bk);

  p_tracks_dd=new hxc_dropdown(XD,handle,x,y,hw,300,NULL,this);
  for (int n=75;n<=FLOPPY_MAX_TRACK_NUM;n++) p_tracks_dd->additem(Str(n),n);
  p_tracks_dd->sel=80-75;
  p_tracks_dd->changesel(TracksIdx);
  y+=35;

  x=10;
  new hxc_button(XD,handle,x,y,0,25,NULL,this,BT_LABEL,T("Sectors"),0,hxc::col_bk);
  x+=hw;

  tw=hxc::get_text_width(XD,T("1 to "));
  new hxc_button(XD,handle,x-tw,y,0,25,NULL,this,BT_LABEL,T("1 to "),0,hxc::col_bk);

  p_secs_dd=new hxc_dropdown(XD,handle,x,y,hw,300,NULL,this);
  for (int n=8;n<=FLOPPY_MAX_SECTOR_NUM;n++) p_secs_dd->additem(Str(n),n);
  p_secs_dd->sel=9-8;
  p_secs_dd->changesel(SecsPerTrackIdx);

  Str ret;
  if (hxc::show_modal_dialog(XD,handle,true,p_ed->handle)==1){
    ret=p_ed->text;

    // 0 to tracks_per_side inclusive! Choosing 80 gives you 81 tracks.
    int tracks_per_side=p_tracks_dd->sl[p_tracks_dd->sel].Data[0]+1;
    *pSecsPerTrack=p_secs_dd->sl[p_secs_dd->sel].Data[0];
    *pSides=p_sides_dd->sl[p_sides_dd->sel].Data[0];

    *pSectors=*pSecsPerTrack * tracks_per_side * *pSides;

    SidesIdx=p_sides_dd->sel;
    TracksIdx=p_tracks_dd->sel;
    SecsPerTrackIdx=p_secs_dd->sel;
  }

  hxc::destroy_modal_dialog(XD,handle);

  return ret;
}


int TDiskManager::diag_lv_np(hxc_listview *lv,int mess,INT_PTR i)
{
  if (mess==LVN_ICONCLICK){
    int icon=lv->sl[i].Data[0]-101;
    if (icon==ICO16_TICKED){
      icon=ICO16_UNTICKED;
    }else{
      icon=ICO16_TICKED;
    }
    lv->sl[i].Data[0]=101+icon;
    lv->draw(0);
  }
  return 0;
}

#endif//UNIX


bool TDiskManager::CreateDiskImage(char *STName,WORD Sectors,WORD SecsPerTrack,WORD Sides) {
  ASSERTB(SecsPerTrack&&Sides) ;
#ifndef SSE_LEAN_AND_MEAN
  if(!SecsPerTrack||!Sides) 
    return false;
#endif
  WORD nTracks=(Sectors/SecsPerTrack)/Sides;
  FILE *fp=fopen(STName,"wb");
  if(fp==NULL)
    return false;
  // write header
  int header_size=0;
  WORD header_word;
  switch(DiskImageType) {
  case EXT_MSA:
/*
Header:
  Word	ID marker, should be $0E0F
  Word	Sectors per track
  Word	Sides (0 or 1; add 1 to this to get correct number of sides)
  Word	Starting track (0-based)
  Word	Ending track (0-based)
*/
    header_size=10+2;
    header_word=0x0E0F;
    SWAP_BIG_ENDIAN_WORD(header_word);
    FWRITE(&header_word,sizeof(WORD),1,fp);
    header_word=SecsPerTrack;
    SWAP_BIG_ENDIAN_WORD(header_word);
    FWRITE(&header_word,sizeof(WORD),1,fp);
    header_word=Sides-1;
    SWAP_BIG_ENDIAN_WORD(header_word);
    FWRITE(&header_word,sizeof(WORD),1,fp);
    header_word=0;
    FWRITE(&header_word,sizeof(WORD),1,fp);
    header_word=nTracks-1;
    SWAP_BIG_ENDIAN_WORD(header_word);
    FWRITE(&header_word,sizeof(WORD),1,fp);
    break;
  case EXT_DIM:
/*
$00     $4242 ("BB") identifier
$02     low byte=1=get used sectors, hi byte=1=read disk conf
$04     seems to be always(?) zero

$06     hi byte=sides
$08     hi byte=sectors
$0a     hi byte=start track
$0c     hi byte=end track
0x000D	Byte		Double-Density(0) or High-Density (1)
The following information block can easily be identified as a Bios
Parameter Block (BPB). The contained data is encoded in Motorola manner.
$0e     RECSIZ sector size (bytes) 
$10     CLSIZ  sectors per cluster
$12     CLSIZB cluster size (bytes) 
$14     RDLEN  root dir size (sectors) 
$16     FSIZ   FAT size (sectors) 
$18     FATREC first sector if 2nd FAT (the one that is used by TOS)
$1a     DATREC Number of 1st data sector 
$1c     NUMCL  Total number of clusters minus DATREC
$1e     BFLAGS (Bit 0 is 0 for 12 bit FAT, 1 for 16 bit FAT)
*/
    header_size=32;
    header_word=0x4242;
    FWRITE(&header_word,sizeof(WORD),1,fp);
    header_word=0;
    FWRITE(&header_word,sizeof(WORD),1,fp);
    header_word=0;
    FWRITE(&header_word,sizeof(WORD),1,fp);
    header_word=Sides-1;
    FWRITE(&header_word,sizeof(WORD),1,fp);
    header_word=SecsPerTrack;
    FWRITE(&header_word,sizeof(WORD),1,fp);
    header_word=0;
    FWRITE(&header_word,sizeof(WORD),1,fp);
    header_word=nTracks-1;
    FWRITE(&header_word,sizeof(WORD),1,fp);
    //bpb - don't think Steem uses it anyway
    header_word=0x200; // RECSIZ sector size (bytes) 
    SWAP_BIG_ENDIAN_WORD(header_word);
    FWRITE(&header_word,sizeof(WORD),1,fp);
    header_word=0x2; // CLSIZ  sectors per cluster
    SWAP_BIG_ENDIAN_WORD(header_word);
    FWRITE(&header_word,sizeof(WORD),1,fp);
    header_word=0x400; // CLSIZB cluster size (bytes) 
    SWAP_BIG_ENDIAN_WORD(header_word);
    FWRITE(&header_word,sizeof(WORD),1,fp);
    header_word=0x7; // RDLEN  root dir size (sectors) 
    SWAP_BIG_ENDIAN_WORD(header_word);
    FWRITE(&header_word,sizeof(WORD),1,fp);
    header_word=0x3; // FSIZ   FAT size (sectors) 
    SWAP_BIG_ENDIAN_WORD(header_word);
    FWRITE(&header_word,sizeof(WORD),1,fp);
    header_word=0x4; // FATREC first sector of 2nd FAT (the one that is used by TOS)
    SWAP_BIG_ENDIAN_WORD(header_word);
    FWRITE(&header_word,sizeof(WORD),1,fp);
    header_word=0xE; // DATREC Number of 1st data sector FATREC + FSIZ + RDLEN
    SWAP_BIG_ENDIAN_WORD(header_word);
    FWRITE(&header_word,sizeof(WORD),1,fp);
    header_word=Sectors/2-0xE/2; // NUMCL  Total number of clusters minus DATREC (SEC - DATREC) / CLSIZ
    SWAP_BIG_ENDIAN_WORD(header_word);
    FWRITE(&header_word,sizeof(WORD),1,fp);
    header_word=0; // BFLAGS (Bit 0 is 0 for 12 bit FAT, 1 for 16 bit FAT)
    SWAP_BIG_ENDIAN_WORD(header_word);
    FWRITE(&header_word,sizeof(WORD),1,fp);
    break;
  }//sw
  {
    char zeros[512];
    ZeroMemory(zeros,sizeof(zeros));
    for(int track=0;track<nTracks;track++)
      for(int side=0;side<Sides;side++)
      {
        if(DiskImageType==EXT_MSA)
        {
          WORD size=SECTOR_SIZE*SecsPerTrack;
          SWAP_BIG_ENDIAN_WORD(size);
          FWRITE(&size,sizeof(WORD),1,fp);
        }
        for(int n=0;n<SecsPerTrack;n++) // zero disk data
          FWRITE(zeros,1,SECTOR_SIZE,fp);
      }
  }
  WORD buf;
  FSEEK(fp,header_size,SEEK_SET);
  FPUTC(0xeb,fp);FPUTC(0x30,fp);
  FSEEK(fp,header_size+8,SEEK_SET); // Skip loader
  FPUTC((BYTE)rand(),fp);FPUTC(BYTE(rand()),fp);FPUTC(BYTE(rand()),fp); //Serial number
  buf=SECTOR_SIZE;
  SWAP_LITTLE_ENDIAN_WORD(buf);
  FWRITE(&buf,2,1,fp); //Bytes Per Sector
  buf=2;
  FWRITE(&buf,1,1,fp); //Sectors Per Cluster
  buf=1;
  SWAP_LITTLE_ENDIAN_WORD(buf);
  FWRITE(&buf,2,1,fp); //RES
  buf=2;
  FWRITE(&buf,1,1,fp); //FATs
  buf=112;
  SWAP_LITTLE_ENDIAN_WORD(buf);
  FWRITE(&buf,2,1,fp); //Dir Entries
  buf=Sectors;
  SWAP_LITTLE_ENDIAN_WORD(buf);
  FWRITE(&buf,2,1,fp);
  buf=249;
  FWRITE(&buf,1,1,fp); //Unused - MSDOS signo in fact
  // from Petari
  // 3 sectors per FAT is enough up to 1MB disk capacity
  // 5 sectors per FAT up to 1700 KB - so HD, 20 sectors per track too
  buf=(Sectors<2000)?3:5;
  SWAP_LITTLE_ENDIAN_WORD(buf);
  FWRITE(&buf,2,1,fp);    //Sectors Per FAT
  buf=SecsPerTrack;
  SWAP_LITTLE_ENDIAN_WORD(buf);
  FWRITE(&buf,2,1,fp);
  buf=Sides;
  SWAP_LITTLE_ENDIAN_WORD(buf);
  FWRITE(&buf,2,1,fp);
  buf=0;
  SWAP_LITTLE_ENDIAN_WORD(buf);
  FWRITE(&buf,2,1,fp); //Hidden Sectors
  FSEEK(fp,header_size+510,SEEK_SET);
  FPUTC(0x97,fp);FPUTC(0xc7,fp); 
  FPUTC(0xf0,fp);FPUTC(0xff,fp);FPUTC(0xff,fp);  // First FAT begin
  FSEEK(fp,(Sectors<2000)?(header_size+2048):(header_size+3072),SEEK_SET); // from Petari
  FPUTC(0xf0,fp);FPUTC(0xff,fp);FPUTC(0xff,fp);  // Second FAT begin
  fclose(fp);
  DeleteFile(Str(STName)+".steembpb");
  return true;
}


void TDiskManager::ToggleVisible() {
#if !defined(SSE_LIBRETRONUKE)
  if(HardDiskMan.IsVisible()) //SS don't quite understand, don't do it for ACSI
    HardDiskMan.Show();
  else
    IsVisible() ? Hide() : Show();
#endif
}


void TDiskManager::SwapDisks(int FocusDrive) { // swap disks in A and B
  TFloppyDisk &discA=FloppyDisk[DRIVE_A],&discB=FloppyDisk[DRIVE_B]; // shorthand
  TSF314 &driveA=FloppyDrive[DRIVE_A],&driveB=FloppyDrive[DRIVE_B];
#ifdef WIN32
  HWND FocusTo=NULL;
  HWND hDiskA=GetDlgItem(Handle,IDC_CONTENTA);
  HWND hDiskB=GetDlgItem(Handle,IDC_CONTENTB);
  if(Handle && GetForegroundWindow()==Handle)
  {
    HWND focus=GetFocus();
    if(FocusDrive>-1)
      FocusTo=FocusDrive==DRIVE_A ? hDiskA : hDiskB;
    else
      FocusTo=focus;
    if(focus==hDiskA||focus==hDiskB) 
      SetFocus(NULL);
  }
#endif
  EasyStr DiskPath[2];
  DiskPath[DRIVE_A]=driveA.GetDisk();
  DiskPath[DRIVE_B]=driveB.GetDisk();
  EasyStr Name[2]={discA.DiskName,discB.DiskName};
  EasyStr DiskInZip[2]={discA.DiskInZip,discB.DiskInZip};
  bool HadDisk[2]={driveA.NotEmpty(),driveB.NotEmpty()};
  driveA.RemoveDisk();
  driveB.RemoveDisk();
#ifdef WIN32
  if(Handle) 
  {
    if(HadDisk[DRIVE_A]) 
      SendMessage(hDiskA,LVM_DELETEITEM,0,0);
    if(HadDisk[DRIVE_B]) 
      SendMessage(hDiskB,LVM_DELETEITEM,0,0);
  }
#endif
  if(HadDisk[DRIVE_B]) 
    InsertDisk(DRIVE_A,Name[DRIVE_B],DiskPath[DRIVE_B],false,false,DiskInZip[DRIVE_B]);
  if(HadDisk[DRIVE_A]) 
    InsertDisk(DRIVE_B,Name[DRIVE_A],DiskPath[DRIVE_A],false,false,DiskInZip[DRIVE_A]);
  UPDATE_STATUS_BAR_PART(SB_PART_CAPS);
#ifdef WIN32
  if(FocusTo)
    SetFocus(FocusTo);
#endif
#ifdef UNIX
  UpdateDiskNames(DRIVE_A);UpdateDiskNames(DRIVE_B);
#endif
}


#if defined(SSE_DISK_SWAPPER)
/*  Smart Disk Swapper: no GUI, no INI, the swapper tries to find the
    next or previous disk based on names only, or based on position.
    Works with archives too.
    v421: a "dumb" disk swapper has been added, it works with m3u lists
*/
// 2 functions used as pointers: find the name in an archive or a folder

BOOL CheckerArchive(char *NameToTest) {
  BOOL bFound=FALSE;
  for(int i=0;i<DiskMan.MenuESL.NumStrings&&!bFound;i++)
  {
    if(!strcmp(DiskMan.MenuESL[i].String,NameToTest))
      bFound=TRUE;
  }
  return bFound;
}


BOOL CheckerFolder(char *PathToTest) {
  BOOL bFound=FALSE;
  if(Exists(PathToTest)) // this is a macro
    bFound=TRUE;
  return bFound;
}


BOOL TestFilenamePatterns(char* oldname,int direction,BOOL(*pChecker)(char *),EasyStr &newname) {
  // some low-level string manipulation
  // oldname may be a file name, with extension, or a path
  // dir 1=next, -1=previous
  // pChecker function pointer to check for existence
  BOOL bFound=FALSE;
#if defined(SSE_LONG_PATH)
  EasyStr sNewFile;
  sNewFile.SetLength(SSE_MAX_PATH);
  char *NewFile=sNewFile.Text;
#else
  char NewFile[MAX_PATH];
#endif
  const size_t OriginalLen=strlen(oldname);
  size_t i=OriginalLen;
  TRACE_LOG2("%s %c%d\n",CHECKPATH(oldname),(direction<0) ? '-' : '+',direction);
  // skip extension
  do
    i--;
  while(i>0 && oldname[i]!='.');
  i--;
  // we start from the end, for likely patterns Disk 1, Disk A, [1]...
  // if it is disc x of y, y will be tested first (FALSE) then x
  // this way we will get them almost all
  for(; i>0 && !bFound ; i--)
  {
    if(isdigit((BYTE)oldname[i]))
    {
      // can be more than one: check previous chars
      char convert_buf[16];
      size_t j=i;
      while(j>1 && isdigit((BYTE)oldname[j-1]) && i-j<10)
        j--;
      int old_ndigits=(int)(i-j+1);
      strncpy(convert_buf,&oldname[j],old_ndigits);
      convert_buf[old_ndigits]='\0';
      int value=atoi(convert_buf);
      int new_value=value+direction;
      sprintf(convert_buf,"%d",new_value);
      int new_ndigits=(int)strlen(convert_buf);
      int less_digits=MAX(0,(old_ndigits-new_ndigits));
      // previous from 10 may be 9 or 09, check both
      for(int k=0;j<SSE_MAX_PATH&&k<=less_digits&&!bFound;k++)
      {
        strncpy(NewFile,oldname,j);
        NewFile[j]='\0';
        if(k)
        {
          char format_string[16];
          sprintf(format_string,"%%0%dd",old_ndigits); // edgy
          sprintf(convert_buf,format_string,new_value);  
        }
        strcat(NewFile,convert_buf);
        if(i<OriginalLen-1)
          strcat(NewFile,&oldname[i+1]);
        TRACE_LOG2("-> %s\n",CHECKPATH(NewFile));
        if(pChecker(NewFile))
          bFound=TRUE;
      }
      i=j;
    }//if(isdigit(oldname[i]))
    else if(isalpha((BYTE)oldname[i]) && OriginalLen<SSE_MAX_PATH && i<SSE_MAX_PATH)
    {
      // for letters, we only look at the last letter of a group
      if(!isalpha((BYTE)oldname[i+1])) // letter must be separate
      {
        strcpy(NewFile,oldname);
        NewFile[i]+=(char)direction;
        // if not alpha...
        TRACE_LOG2("-> %s\n",CHECKPATH(NewFile));
        if(pChecker(NewFile))
          bFound=TRUE;
      }
    }
  }//nxt i
  if(bFound)
  {
    newname=NewFile;
    TRACE_LOG("%s %s\n","Disk swapper found",CHECKPATH(newname.Text));
  }
  else
  {
    TRACE_LOG("%s nothing for %s\n","Disk swapper found",CHECKPATH(oldname));
  }
  return bFound;
}


int TDiskManager::IsDiskImage(char *name) { // helper using helper TODO
  int ext=EXT_NONE;
  char sExt[16];
  sExt[0]='\0';
  char* dot=NULL;
  int Type=FileIsDisk(name);
  if(Type==DISK_COMPRESSED)
  {
    MenuESL.DeleteAll();
    MenuESL.Sort=eslSortByNameI;
    zippy.list_contents(name,&MenuESL,true);
    for(int i=0;i<MenuESL.NumStrings;i++)
    {
      Type=FileIsDisk(MenuESL[i].String);
      if(Type==DISK_UNCOMPRESSED||Type==DISK_PASTI)
      {
        dot=strrchr(MenuESL[i].String,'.');
        break;
      }
    }
  }
  else
    dot=strrchr(name,'.');
  if(dot)
  {
    strncpy(sExt,dot+1,16);
    for(int i=EXT_NONE+1;i<NUM_EXT&&ext==EXT_NONE;i++)
    {
      if(i!=EXT_STG && !strcmpi(sExt,extension_list[i]))
        ext=i;
    }
  }
  return ext;
}


BOOL TDiskManager::ChangeDisk(int floppyno,int direction,BOOL bStatusbar) { 
  // change disk in drive, looking for previous or next one
  TRACE_LOG2("Swapper %c\n",(direction<0) ?'-':'+');
  BOOL bFound=FALSE,bSkip=FALSE;
  EasyStr newname;
  BOOL insert_second_disk_in_b=(floppyno==2);
  floppyno&=1;
  TFloppyDisk &disc=FloppyDisk[floppyno];
  TSF314 &drive=FloppyDrive[floppyno];
  // if empty, nothing to do
  if(!drive.Empty())
  {
#if defined(SSE_DISK_M3U)
    // first check if current image is current image of M3U swapper (all disks must go in same drive)
    // in that case, we use the dumb swapper
    if(disc.ImageFile==DumbDiskSwapper.image_path)
    {
      int idx=DumbDiskSwapper.image_index;
      // inc or dec with bounding
      idx+=direction;
      if(idx<0)
        idx=DumbDiskSwapper.num_images-1;
      else if(idx>=DumbDiskSwapper.num_images)
        idx=0;
      newname=DumbDiskSwapper.GetPath((WORD)idx); // this updates image path
      EasyStr Name=GetFileNameFromPath(newname);
      PerformInsertAction(floppyno|4,Name,newname,"");
      bFound=true;
    }
#endif
    // if archive, first look into archive
    if(!bFound && disc.IsZip())
    {
/* even when not matching patterns, sort files because it's hard
   to control the real order, both WinRAR and 7-Zip show sorted files*/
      MenuESL.DeleteAll();
      MenuESL.Sort=eslSortByNameI;
      zippy.list_contents(disc.GetImageFile(),&MenuESL,true);
      if(MenuESL.NumStrings>1) 
      {
        if(bSwapperPattern||insert_second_disk_in_b)
        { // create names and look for them in archive
          bFound=TestFilenamePatterns(disc.DiskInZip.Text,direction,
            CheckerArchive,newname);
          if(bFound)
          { // or w/ 4 to disable autoinsert second disk, and w/ 3 in function
            PerformInsertAction(floppyno|4|insert_second_disk_in_b,
              disc.DiskName,disc.ImageFile,newname);
          }
        }
        else if(disc.DiskInZip.Text!=NULL)
        { // take previous or next image in archive, if it exists
          // first find current file in list
          int oldnum=-1;
          for(int i=0;i<MenuESL.NumStrings;i++)
            if(!strcmp(disc.DiskInZip.Text,MenuESL[i].String))
              oldnum=i;;
          if(oldnum!=-1)
          {
            TRACE_LOG2("Current archive #%d\n",oldnum);
            int newnum=oldnum;
            do {
              newnum+=direction;
              if(newnum>=0 && newnum<MenuESL.NumStrings)
              {
                newname=MenuESL[newnum].String;
                TRACE_LOG2("New archive #%d = %s\n",newnum,CHECKPATH(newname.Text));
                if(IsDiskImage(newname))
                  bFound=TRUE;
              }
            } while (!bFound && newnum>=0 && newnum<MenuESL.NumStrings);
            if(bFound)
            {
              ArchiveNoMore=false;
              PerformInsertAction(floppyno|4,disc.DiskName,disc.ImageFile,newname);
            }
            else if(!ArchiveNoMore)
            {
              ArchiveNoMore=true;
              bSkip=TRUE;
            }
#ifdef SSE_DEBUGGER
            else TRACE_LOG2("archive #%d doesn't exist\n",newnum);
#endif
          }
        }
      }//if(MenuESL.NumStrings>1) 
    }//if(disc.IsZip())
    // look into folder
    if(!bFound&&!bSkip)
    {
      if(bSwapperPattern||insert_second_disk_in_b)
        // create names and look for them in directory
        bFound=TestFilenamePatterns(disc.ImageFile.Text,direction,CheckerFolder,newname);
      else
      { // take previous or next file in directory, regardless of name
        // we must skip files that are not disk images
#ifdef WIN32
        // checking all archives for 'previous' is slow
        TNotify myNotify(T("Disk operation"));
#endif
        DirSearch ds;
        EasyStr current_path,previous;
        EasyStr path=disc.ImageFile;
        EasyStr currentfilename=GetFileNameFromPath(path);
        RemoveFileNameFromPath(path,WITH_SLASH);
        newname=path; // for now only the path (recycling)
        path+="*.*"; // could be slow if we crawl through a big folder
        bool ok=ds.Find(path);
        while(ok && strcmp(ds.Name,currentfilename.Text)) // while different
        {
          if(direction<0) // if --, memorise last disk image before current
          {
            current_path=newname+ds.Name;
            if(IsDiskImage(current_path.Text)) // could skip for --
              previous=current_path;
          }
          ok=ds.Next();
        }
        if(ok) // we reached current file
        {
          if(direction>0) // if ++, get next disk image
          {
            while(!bFound &&  ds.Next()) // could skip for ++
            {
              current_path=newname+ds.Name;
              if(IsDiskImage(current_path.Text))
              {
                newname=current_path;
                bFound=TRUE;
              }
            }
          }
          else if(previous.NotEmpty()) // if --, get memorised path
          {
            newname=previous;
            bFound=TRUE;
          }
        }//if(ok)
        ds.Close();
      }//if(bSwapperPattern)
      if(bFound)
      {
        EasyStr Name=GetFileNameFromPath(newname);
        PerformInsertAction(floppyno|4|insert_second_disk_in_b,Name,newname,"");
#ifdef WIN32
        if(!insert_second_disk_in_b)
          GoToDisk(newname,false,false); // highlight new disk in DM but no don't change focus
#endif        
      }
    }//if(!bFound)
  }//if(!drive.Empty())
#if defined(SSE_GUI_STATUS_BAR)
  if(bStatusbar)
  {
    char *string;
    if(bFound)
      if(disc.IsZip())
        string=disc.DiskInZip.Text;
      else
        string=disc.DiskName.Text;
    else
      string="ERROR";
    if(string)
    {
      const int maxlen=40;
      int len=(int)strlen(string);
      if(len>maxlen) // if needed, shorten the name as start...end
      {
        char string2[64];
        strncpy(string2,string,maxlen/2-2);
        strcat(string2,"...");
        strcat(string2,string+len-maxlen/2+2);
        string=string2;
      }
#if defined(SSE_OSD_DEBUGINFO)
      if(OsdControl.Trace(TOsdControl::OUTPUT_SB,string)&TOsdControl::OUTPUT_OSD) // drawn on OSD
        if(runstate==RUNSTATE_STOPPED && !FullScreen)
          draw(true);
#endif
    }
  }
#endif
  return bFound;
}

#endif


bool TDiskManager::AreNewDisksInHistory(int d) {
  TFloppyDisk &disk=FloppyDisk[d]; // shortcut
  EasyStr CurrentDiskName=CreateDiskName(disk.DiskName,disk.DiskInZip);
  for(int n=0;n<DM_HISTORY_LEN;n++) 
  {
    if(InsertHist[d][n].Path.NotEmpty()) 
    {
      EasyStr MenuItemText=CreateDiskName(InsertHist[d][n].Name,InsertHist[d][n].DiskInZip);
      if(NotSameStr_I(CurrentDiskName,MenuItemText))
        return true;
    }
  }
  return false;
}


void TDiskManager::InsertHistoryAdd(int d,char *Name,char *Path,char *DiskInZip) {
  InsertHistoryDelete(d,Name,Path,DiskInZip); // no doubles
  for(int n=DM_HISTORY_LEN-1;n>0;n--) // shift previous history
    InsertHist[d][n]=InsertHist[d][n-1];
  InsertHist[d][0].Name=Name;
  InsertHist[d][0].Path=Path;
  InsertHist[d][0].DiskInZip=DiskInZip;
#ifdef WIN32
#ifndef SSE_LEAN_AND_MEAN
  if(Handle)
#endif
#if defined(SSE_420R5) // same number
    EnableWindow(GetDlgItem(GetDlgItem(Handle,IDC_DRIVEA+d),IDC_HISTBUT),AreNewDisksInHistory(d));
#else
    EnableWindow(GetDlgItem(GetDlgItem(Handle,IDC_DRIVEA+d),IDC_CONTENTA),AreNewDisksInHistory(d));
#endif
#endif
}


void TDiskManager::InsertHistoryDelete(int d,char *Name,char *Path,char *DiskInZip) {
  for(int n=0;n<DM_HISTORY_LEN;n++) 
  {
    if(IsSameStr_I(Name,InsertHist[d][n].Name)&&IsSameStr_I(Path,InsertHist[d][n].Path)
      && IsSameStr_I(DiskInZip,InsertHist[d][n].DiskInZip))
      InsertHist[d][n].Path="";
  }
  for(int n=0;n<DM_HISTORY_LEN;n++) 
  {
    bool More=false;
    for(int i=n;i<DM_HISTORY_LEN;i++)
      if(InsertHist[d][i].Path.NotEmpty()) 
      {
        More=true;
        break;
      }
    if(!More)
      break;
    if(InsertHist[d][n].Path.Empty()) 
    {
      for(int i=n;i<DM_HISTORY_LEN-1;i++)
        InsertHist[d][i]=InsertHist[d][i+1];
      n--;
    }
  }
}


// return true if OK
bool TDiskManager::InsertDisk(int floppyno,EasyStr Name,EasyStr Path,
                        bool const bDontChangeDisk,bool const bMakeFocus,EasyStr DiskInZip,
                        bool const bSuppressError,bool const bAllowInsert2) {
  ASSERTB((floppyno&1)==floppyno); // floppyno int to avoid casts or warnings
#ifndef SSE_LEAN_AND_MEAN
  floppyno&=1;
#endif
  TSF314 &drive=FloppyDrive[floppyno]; // shorthand
  TFloppyDisk &disk=FloppyDisk[floppyno]; // shorthand
  if(!bDontChangeDisk)
  {
    if(Path.Empty())
      return false;
    TRACE_LOG("%c: Inserting disk %s [%s]\n",floppyno+'A',Name.Text,CHECKPATH(Path.Text));
    int Error=drive.SetDisk(Path,DiskInZip);
    if(Error) 
    {
      TRACE_LOG("Set Disk Error %d\n",Error);
      if(drive.Empty()) 
        EjectDisk(!!floppyno); // Update display
      if(!bSuppressError) 
      {
        switch(Error) {
        case FIMAGE_WRONGFORMAT:
          Alert(Path+": "+T("image not recognised!"),T("ERROR"),MB_ICONEXCLAMATION);
          break;
        case FIMAGE_CANTOPEN:
          Alert(Path+" "+T("cannot be opened."),T("ERROR"),MB_ICONEXCLAMATION);
          break;
        case FIMAGE_FILEDOESNTEXIST:
          Alert(Path+" "+T("doesn't exist!"),T("ERROR"),MB_ICONEXCLAMATION);
          break;
        case FIMAGE_CORRUPTZIP:
          Alert(Path+" "+T("does not contain any files, it may be corrupt!"),
            T("ERROR"),MB_ICONEXCLAMATION);
          break;
        case FIMAGE_NODISKSINZIP:
          ExtractArchiveToSTHardDrive(Path);
          break;
        case FIMAGE_DIMNOMAGIC:
          Alert(Path+" "+T("is not in the correct format, it may be corrupt!")+
            "\r\n\r\n"+
T("This image has the extension DIM, unfortunately many different disk imaging programs use that extension for different disk image formats.")+" "+
T("Sometimes DIM images are actually ST images with the incorrect extension.")+" "+
T("You may find you can use this image by changing the extension to .st.")+"\r\n"+
T("WARNING: Backup the disk image before you change the extension, inserting an image with the wrong extension could corrupt it."),
            T("ERROR"),MB_ICONEXCLAMATION);
          break;
        case FIMAGE_DIMTYPENOTSUPPORTED:
            Alert(Path+" "+
T("is in a version of the DIM format that Steem currently doesn't support.")+" "+
T("If you have details for how to read this disk image please let us know and we'll support it in the next version."),
          T("ERROR"),MB_ICONEXCLAMATION);
          break;
        case FIMAGE_MISSING_DLL:
          Alert(T("Missing plugin"),T("ERROR"),MB_ICONEXCLAMATION);
          break;
        case FIMAGE_IS_CONFIG:
          break;
        }
      }
      return false;
    }
    disk.DiskName=Name; // name = path removed

    if(bAllowInsert2 && floppyno==DRIVE_A)
      AutoInsert2&=~2;
    if(bAllowInsert2 && floppyno==DRIVE_A && AutoInsert2 && nFloppyDrives==2) 
    {
#if defined(SSE_DISK_SWAPPER)
      if(ChangeDisk(2,1,FALSE)) // use our powerful disk swapper instead!
        AutoInsert2|=2;
#else
      Error=1;
      Str NewPath=Path;
      Str NewDiskInZip=DiskInZip;
      char *dot=strrchr(NewPath,'.');
      if(dot)
      {
        // The last symbol before the . must be A and B, it will not 
        //work with (A) and (B)
        dot--;
        if(*dot=='1')
          *dot='2',Error=0;
        if(*dot=='a')
          *dot='b',Error=0;
        if(*dot=='A')
          *dot='B',Error=0;
        Str NewName=GetFileNameFromPath(NewPath);
        if(HideExtension)
        {
          dot=strrchr(NewName,'.');
          NewName=NewName.Lefts(dot-NewName.Text);
        }
        if(Error==0)
        {
          InsertDisk(DRIVE_B,NewName,NewPath,false,false,NewDiskInZip,true);
          AutoInsert2|=2; //TODO def
        }
      }
#endif
    }// if(bAllowInsert2 && floppyno==DRIVE_A && AutoInsert2)
    DiskInZip=disk.DiskInZip; // for GUI update
    InsertHistoryAdd(floppyno,Name,Path,DiskInZip);
  }
#ifdef WIN32
  if(Handle==NULL) 
    return true;
  HWND LV=GetDlgItem(Handle,IDC_CONTENTA+floppyno);
  if(SendMessage(LV,LVM_GETITEMCOUNT,0,0)) 
    SendMessage(LV,LVM_DELETEITEM,0,0);
  SetDriveViewEnable(floppyno,true);
  if(GetForegroundWindow()==Handle && bMakeFocus)
    SetFocus(LV);
  Name=CreateDiskName(Name,DiskInZip);
  TDiskManFileInfo *Inf=new TDiskManFileInfo;
  Inf->Name=Name;
  { //v410: date
    WIN32_FIND_DATA wfd;
    FindFirstFile(Path,&wfd);
    Inf->Date=wfd.ftLastWriteTime;
  }
  Inf->Path=Path;
  Inf->Folder=Inf->UpFolder=Inf->BrokenLink=false;
  Inf->ReadOnly=disk.ReadOnly;
  Inf->Zip=disk.IsZip();
  LV_ITEM lvi;
  lvi.mask=LVIF_TEXT|LVIF_PARAM|LVIF_IMAGE|LVIF_STATE;
  lvi.iItem=lvi.iSubItem=0;
#if 1
  BYTE &extension=drive.ImageType.RealExtension;
  if(Inf->Zip)
    Inf->Image=IMG_DISKZIPPED_RW;
  else if(disk.ReadOnly||disk.WriteProtect)
    Inf->Image=IMG_DISKREADONLY;
  else switch(extension) {
#if !defined(SSE_GUI_DM_BIGICONS_LESS)
  case EXT_ST: case EXT_MSA: case EXT_DIM:
    Inf->Image=IMG_BLUEDISK;
    break;
  case EXT_STW: case EXT_HFE:
    Inf->Image=IMG_GREYDISK;
    break;
#endif
  case EXT_PRG: case EXT_TOS:
    Inf->Image=IMG_PRGFILEICO;
    break;
  default:
    Inf->Image=IMG_DISK; // like in listview
  }//sw if
#if defined(SSE_GUI_DM_PICS)
  if(MyGdiPlus.GetPic(Path))
    Inf->Image=IMG_CUSTOM; // Steem will display the picture of the disk in the disk A or B container!
#endif
  lvi.iImage=Inf->Image;
#else
  lvi.iImage=int(Inf->Zip ? 8 : (1+disk.ReadOnly*4));
#endif
  lvi.stateMask=LVIS_SELECTED|LVIS_FOCUSED;
  lvi.state=LVIS_SELECTED|LVIS_FOCUSED;
  lvi.lParam=(LPARAM)Inf;
  lvi.pszText=Inf->Name;
  SendMessage(LV,LVM_INSERTITEM,0,(LPARAM)&lvi);
  CentreLVItem(LV,0,LVIS_SELECTED|LVIS_FOCUSED);
#endif//WIN32

#ifdef UNIX
  UpdateDiskNames(floppyno);
#endif
  UPDATE_STATUS_BAR_PART(SB_PART_CAPS);
  return true;
}


void TDiskManager::ExtractArchiveToSTHardDrive(Str Path) {
  if(Alert(Path+": "+T("Steem doesn't recognise any disk images.")+"\n\n"+
    T("Would you like to extract the contents of this archive to an ST hard drive?"),
    T("Extract Contents?"),MB_ICONQUESTION|MB_YESNO)==IDNO) 
    return;
  Str Name=GetFileNameFromPath(Path);
  char *dot=strrchr(Name,'.');
  if(dot) 
    *dot='\0';
  Str ExtractPath;
  int d=DRIVE_C;
  for(;d<GEMDOS_MAXDRIVES;d++) 
  {
    if(Stemdos.DriveMounted[d]) 
    {
      ExtractPath=Stemdos.MountPath[d];
      break;
    }
  }
  if(ExtractPath.Empty()) 
  {
    ExtractPath=UsersPath+SLASH+"st_c";
    CreateDirectory(ExtractPath,NULL);
    if(HardDiskMan.NewDrive(ExtractPath)) 
    {
      d=DRIVE_C;
      HardDiskMan.update_mount();
    }
    else 
    {
      Alert(T("Could not create a new hard drive."),T("ERROR"),MB_ICONEXCLAMATION);
      return;
    }
  }
  Str Fol;
#ifdef WIN32
  Fol=GetUniquePath(ExtractPath,Name);
#else
  {
    if(Name.Length()>8) Name[8]=0;
    strupr(Name);
    struct stat s;
    bool first=true;
    for(;;) {
      Fol=ExtractPath+"/"+Name;
      if(stat(Fol,&s)==-1) break;
      if(first) {
        if(Name.Length()<7) Name+="_";
        if(Name.Length()<8) Name+="2";
        *Name.Right()='2';
        first=0;
      }
      else {
        (*Name.Right())++;
        if(Name.RightChar()==char('9'+1)) *Name.Right()='A';
      }
    }
  }
#endif
  CreateDirectory(Fol,NULL);
  EasyStringList sl;
  sl.Sort=eslNoSort;
  zippy.list_contents(Path,&sl,false);
  // If every file is in the same folder then strip it (stops annoying double folder)
  Str Temp=sl[0].String,FirstFol;
  for(int i=0;i<Temp.Length();i++) 
  {
    if(Temp[i]=='\\'||Temp[i]=='/') 
    {
      Temp[i+1]='\0';
      FirstFol=Temp;
      break;
    }
  }
  if(FirstFol.NotEmpty()) 
  {
    for(int s=1;s<sl.NumStrings;s++) 
    {
      if(strstr(sl[s].String,FirstFol)!=sl[s].String) 
      {
        FirstFol="";
        break;
      }
    }
  }
  // Extract all files, make sure we create all necessary directories
  for(int s=0;s<sl.NumStrings;s++) 
  {
    Str Dest=Fol+SLASH+(sl[s].String+FirstFol.Length());
#ifdef UNIX
    while(strchr(Dest,'\\'))
      (*strchr(Dest,'\\'))='/';
#endif
    Str ContainingPath=sl[s].String+FirstFol.Length();
    INT_PTR cpl=ContainingPath.Length();
    for(int i=0;i<cpl;i++) 
    {
      if(ContainingPath[i]=='\\'||ContainingPath[i]=='/') 
      {
        char old=ContainingPath[i];
        ContainingPath[i]='\0';
        if(GetFileAttributes(Fol+SLASH+ContainingPath)==INVALID_FILE_ATTRIBUTES)
          CreateDirectory(Fol+SLASH+ContainingPath,NULL);
        ContainingPath[i]=old;
      }
    }
    if(Dest.RightChar()!='/' && Dest.RightChar()!='\\') 
    {
      if(zippy.extract_file(Path,(DWORD)sl[s].Data[0],Dest,(DWORD)sl[s].Data[1])==ZIPPY_FAIL)
      {
        Alert(T("Could not extract files, this archive may be corrupt!"),
          T("ERROR"),MB_ICONEXCLAMATION);
        return;
      }
    }
  }
  Str STFol=Str((char)('A'+d))+":\\";
  DirSearch ds(Fol);
  STFol+=ds.ShortName;
  ds.Close();
  if(Alert(T("Files successfully extracted to:")+"\n\n"+
    T("PC folder")+": "+Fol+"\n"+
    T("ST folder")+": "+STFol+"\n"+
    T("Would you like to run Steem and go to the GEM desktop now?"),
    T("Files Extracted"),MB_ICONQUESTION|MB_YESNO)==IDYES)
    PerformInsertAction(ACTION_INSERT_RUN,"","","");
}


void TDiskManager::EjectDisk(bool const floppy_no,bool const losechanges/*=false*/) {
  FloppyDrive[floppy_no].RemoveDisk(losechanges);
  FloppyDrive[floppy_no].UpdateAdat();
#ifdef WIN32
  if(Handle)
  {
    SendMessage(GetDlgItem(Handle,IDC_CONTENTA+floppy_no),LVM_DELETEITEM,0,0);
    HWND hHistBut=GetDlgItem(GetDlgItem(Handle,IDC_DRIVEA+floppy_no),IDC_HISTBUT);
    EnableWindow(hHistBut,AreNewDisksInHistory(floppy_no));
  }
#endif//WIN32
#ifdef UNIX
  UpdateDiskNames(floppy_no);
#endif
}


void TDiskManager::ExtractDisks(Str Path) {
  if(!enable_zip) 
    return;
  EasyStringList sl;
  sl.Sort=eslNoSort;
  zippy.list_contents(Path,&sl,true);
  if(sl.NumStrings==0)
    Alert(EasyStr(T("Cannot find a disk image in the archive"))+" "+Path,
      T("ERROR"),MB_ICONEXCLAMATION);
  else 
  {
    EasyStr SelPath="";
    for(int s=0;s<sl.NumStrings;s++) 
    {
      int Choice=IDYES;
      EasyStr DestPath=DisksFol+SLASH+GetFileNameFromPath(sl[s].String);
      if(Exists(DestPath))
        Choice=Alert(Str(sl[s].String)+" "+T("already exists, do you want to overwrite it?"),
          T("Are you sure?"),MB_ICONQUESTION|MB_YESNO);
      if(Choice==IDYES) 
      {
        if(zippy.extract_file(Path,(DWORD)sl[s].Data[0],DestPath,(DWORD)sl[s].Data[1])==ZIPPY_FAIL)
          Alert(EasyStr(T("There was an error extracting"))+" "+sl[s].String
            +" "+T("from")+" "+Path,T("ERROR"),MB_ICONEXCLAMATION);
        else 
          SelPath=DestPath;
      }
    }
    if(SelPath.NotEmpty()) 
      RefreshDiskView(SelPath);
  }
}


void TDiskManager::GCGetCRC(char *Path,DWORD *lpCRC,int nCRCs) {
  EasyStringList sl;
  zippy.list_contents(Path,&sl,true);
  int n=MIN(sl.NumStrings,nCRCs);
  for(int i=0;i<n;i++)
    *(lpCRC++)=(DWORD)sl[i].Data[2];
}


#ifdef DEADC0DE
BYTE* TDiskManager::GCConvertToST(char *Path,int Num,int *pLen) {
  char file[MAX_PATH];
  char real_name[MAX_PATH];
  bool del=false;
  *pLen=0;
  if(FileIsDisk(Path)==DISK_COMPRESSED) 
  {
    EasyStringList sl;
    zippy.list_contents(Path,&sl,true);
    if(Num>=sl.NumStrings) 
    {
      *pLen=-1;
      return NULL;
    }
    GetTempFileName(WriteDir,"ZIP",0,file);
    zippy.extract_file(Path,(DWORD)sl[Num].Data[0],file,true,0);
    strcpy(real_name,sl[Num].String);
    del=true;
  }
  else 
  {
    if(Num>0) 
    {
      *pLen=-1;
      return NULL;
    }
    strcpy(file,Path);
    strcpy(real_name,Path);
  }
  char *ext=strrchr(real_name,'.');
  if(ext==NULL) 
  {
    if(del) 
      DeleteFile(file);
    return NULL;
  }
  BYTE *mem=NULL;
  int len;
  if(IsSameStr_I(ext,dot_ext(EXT_MSA))) 
  {
    FILE *nf=fopen(file,"rb");
    if(nf==NULL) 
    {
      if(del) 
        DeleteFile(file);
      return NULL;
    }
    WORD ID,MSA_SecsPerTrack,MSA_Sides,StartTrack,MSA_EndTrack;
    // Read header
    FREAD(&ID,2,1,nf);               SWAP_BIG_ENDIAN_WORD(ID);
    FREAD(&MSA_SecsPerTrack,2,1,nf); SWAP_BIG_ENDIAN_WORD(MSA_SecsPerTrack);
    FREAD(&MSA_Sides,2,1,nf);        SWAP_BIG_ENDIAN_WORD(MSA_Sides);
    FREAD(&StartTrack,2,1,nf);       SWAP_BIG_ENDIAN_WORD(StartTrack);
    FREAD(&MSA_EndTrack,2,1,nf);     SWAP_BIG_ENDIAN_WORD(MSA_EndTrack);
    bool Error=(MSA_SecsPerTrack<1||MSA_SecsPerTrack>FLOPPY_MAX_SECTOR_NUM||
      MSA_Sides>1||StartTrack!=0||MSA_EndTrack<1
      ||MSA_EndTrack>FLOPPY_MAX_TRACK_NUM);
    if(!Error) 
    {
      *pLen=(MSA_SecsPerTrack*SECTOR_SIZE)*(MSA_EndTrack+1)*(MSA_Sides+1);
      mem=(BYTE*)malloc(*pLen+16);
      // Read data
      WORD Len,NumRepeats;
      BYTE *TrackData=new BYTE[(MSA_SecsPerTrack*SECTOR_SIZE)+16];
      BYTE *pDat,*pEndDat,dat;
      BYTE *pSTBuf=mem;
      for(int n=0;n<=MSA_EndTrack;n++) 
      {
        for(int s=0;s<=MSA_Sides;s++) 
        {
          Len=0;
          FREAD(&Len,1,2,nf); SWAP_BIG_ENDIAN_WORD(Len);
          if(Len>MSA_SecsPerTrack*SECTOR_SIZE||Len==0)
          {
            Error=true;
            break;
          }
          if(WORD(FREAD(TrackData,1,Len,nf))<Len)
          {
            Error=true;
            break;
          }
          if(Len==(MSA_SecsPerTrack*SECTOR_SIZE)) 
          {
            memcpy(pSTBuf,TrackData,Len);
            pSTBuf+=Len;
          }
          else 
          {
            // Convert compressed MSA format track in TrackData to ST format in STBuf
            BYTE *pSTBufEnd=pSTBuf+(MSA_SecsPerTrack*SECTOR_SIZE);
            pDat=TrackData;
            pEndDat=TrackData+Len;
            while(pDat<pEndDat && pSTBuf<pSTBufEnd) 
            {
              dat=*(pDat++);
              if(dat==0xE5) 
              {
                dat=*(pDat++);
                NumRepeats=*LPWORD(pDat);pDat+=2;
                SWAP_BIG_ENDIAN_WORD(NumRepeats);
                for(int s2=0;s2<NumRepeats && pSTBuf<pSTBufEnd;s2++) 
                  *(pSTBuf++)=dat;
              }
              else
                *(pSTBuf++)=dat;
            }
          }
        }
        if(Error) 
          break;
      }
      delete[] TrackData;
    }
    fclose(nf);
    if(Error) 
    {
      free(mem);
      mem=NULL;
      *pLen=0;
    }
  }
  else if(IsSameStr_I(ext,dot_ext(EXT_DIM))) 
  {
    FILE *fp=fopen(file,"rb");
    if(fp) 
    {
      len=GetFileLength(fp)-32;
      mem=(BYTE*)malloc(len);
      FSEEK(fp,32,SEEK_SET);
      FREAD(mem,1,len,fp);
      fclose(fp);
      *pLen=len;
    }
  }
  if(del) 
    DeleteFile(file);
  return mem;
}
#endif


#if !defined(SSE_LIBRETRONUKE)

void TDiskManager::InitGetContents() { // called by Initialise()
  GetContents_GetZipCRCsProc=GCGetCRC;
#ifdef DEADC0DE
  GetContents_ConvertToSTProc=GCConvertToST;
#endif
  GetContents_ListFile=new char[SSE_MAX_PATH]; // global
  strcpy(GetContents_ListFile,RunDir+SLASH+SSE_PLUGIN_DIR1+SLASH+DISK_IMAGE_DB);
  if(!Exists(GetContents_ListFile)) 
    strcpy(GetContents_ListFile,RunDir+SLASH+SSE_PLUGIN_DIR2+SLASH+DISK_IMAGE_DB);
  if(!Exists(GetContents_ListFile)) 
    strcpy(GetContents_ListFile,RunDir+SLASH+DISK_IMAGE_DB);
  if(!Exists(GetContents_ListFile)) 
    strcpy(GetContents_ListFile,DocDir+DISK_IMAGE_DB);
  TRACE_SECTION(LOGSECTION_INIT,"%s %d\n",CHECKPATH(GetContents_ListFile),Exists(GetContents_ListFile));
}


bool TDiskManager::GetContentsCheckExist() {
  if(Exists(GetContents_ListFile)) 
    return true;
  int i=Alert(
    T("Steem cannot find the ST disk image database, would you like to open\
 the disk image database website now?"),
    T("Cannot Find Database"),MB_ICONQUESTION|MB_YESNO);
  if(i==IDYES) 
  {
#ifdef WIN32
    ShellExecute(NULL,NULL,DIDATABASE_WEB,"","",SW_SHOWNORMAL);
#endif
#ifdef UNIX
    shell_execute(Comlines[COMLINE_HTTP],Str("[URL]\n")+DIDATABASE_WEB);
#endif
  }
  return false;
}


void TDiskManager::GetContentsSL(Str Path) {
  contents_sl.DeleteAll();
  if(!GetContentsCheckExist())
    return;
  char buf[1024];
  DWORD dwCRCs[30];
  memset(dwCRCs,0,30*sizeof(DWORD));
//  TNotify myNotify(T("Disk operation"));
  int nLinks=GetContentsFromDiskImage(Path,buf,1024,dwCRCs);//crc32);//GC_ONAMBIGUITY_GUESS);
  if(nLinks>0)
  {
    contents_sl.Sort=eslNoSort;
    contents_sl.Add(Path,dwCRCs[0]); // of first disk image found
    char *p=buf;
    for(int i=0;i<nLinks;i++) 
    {
      if(p[0]=='\0')
        break;
      contents_sl.Add(p);
      p+=strlen(p)+1;
    }
  }
  else // if not recognised, give all the names and CRC32
  {
    MenuESL.DeleteAll();
    MenuESL.Sort=eslSortByNameI;
    zippy.list_contents(Path,&MenuESL,true);
    const int namesize=48;
    const int size=30*(8+4+namesize);
    char sCrc[size]=""; // too big
    char tCrc[16];
    char tName[namesize+1]; // including last!
    for(int i=0;dwCRCs[i]>0&&i<30;i++)
    {
      memset(tName,0,namesize+1);
      if(MenuESL[i].String)
        strncpy(tName,MenuESL[i].String,namesize);
      strcat(sCrc,tName);
      sprintf(tCrc," %08X\n",dwCRCs[i]);
      strcat(sCrc,tCrc);
    }
    TRACE("%s",sCrc);
    Alert(sCrc,T("CRC32 not found in database"),MB_OK);
  }
}

#endif//#if !defined(SSE_LIBRETRONUKE)

#ifdef DEADC0DE // if we want short names we edit the text file

Str TDiskManager::GetContentsGetAppendName(Str TOSECName) {
  Str FirstName=TOSECName;
  char *spc=strchr(FirstName,' ');
  if(spc) 
    *spc='\0';
  Str TOSEC=TOSECName;
  Str Letter;
  spc=strstr(TOSEC," of ");
  if(spc) 
  {
    while(strstr(spc+1," of ")) 
      spc=strstr(spc+1," of "); // find last " of "
    *(spc--)='\0';
    for(;spc>TOSEC.Text;spc--) 
      if(spc[0]<'0'||spc[0]>'9') 
      {
        *(spc++)='\0';
        break;
      }
    Letter=char('a'+atoi(spc)-1);
  }
  else 
  {
    for(int i=0;i<10;i++)
      if(strstr(TOSEC,Str("Part ")+char('A'+i))==TOSEC.Right()-5) 
        Letter=char('a'+i);
  }
  Str Number;
  for(spc=TOSEC.Text+TOSEC.Length()-1;spc>TOSEC.Text;spc--) 
  {
    if(spc[0]>='0' && spc[0]<='9') 
    {
      *(spc+1)='\0';
      for(;spc>TOSEC.Text;spc--) 
        if(spc[0]<'0'||spc[0]>'9') 
        {
          Number=atoi(spc);
          break;
        }
      break;
    }
  }
  if(FirstName=="Automation") 
    FirstName="Auto";
  if(strstr(TOSEC,"Pompey Pirates")==TOSEC.Text)
    FirstName="PP";
  if(strstr(TOSEC,"Sewer Doc")==TOSEC.Text) 
    FirstName="Sewer Doc";
  if(strstr(TOSEC,"Flame of Finland")==TOSEC.Text) 
    FirstName="FOF";
  if(strstr(TOSEC,"Persistance of Vision")==TOSEC.Text) 
    FirstName="POV";
  if(strstr(TOSEC,"ST Format")==TOSEC.Text) 
    FirstName="STF";
  if(strstr(TOSEC,"Bad Brew Crew")==TOSEC.Text) 
    FirstName="BBC";
  Str ShortName=FirstName;
  if(Number.NotEmpty()) 
    ShortName+=Str(" ")+Number+Letter;
  return ShortName;
}

#endif


////////////
// TSF314 //
////////////

// TSF314 functions defined here because they're more part of GUI than EMU
// (important when we compile big modules - not the official releases)


//#pragma warning (disable: 4701) //MSA vars


int TSF314::SetDisk(EasyStr FilePath,EasyStr CompressedDiskName,
                    TBpbInfo *pDetectBPB,TBpbInfo *pFileBPB) {
  TRACE_LOG("%c: SetDisk %s\n",'A'+Id,CHECKPATH(FilePath.c_str()));
/*  Note that the drive may still be spinning when a floppy is removed
    or inserted -> that state mustn't be changed (Braindamage)
    This function is rather unoptimised but performance doesn't matter here,
    if efforts should be made, it could be toward smaller code footprint.
*/
  if(!Exists(FilePath))
    return FIMAGE_FILEDOESNTEXIST;
  TFloppyDisk &disk=FloppyDisk[Id]; // shorthand
#if defined(SSE_ENABLE_TRACE_LOG)
  DWORD time0=timeGetTime();
#endif
#ifdef WIN32
  TNotify myNotify(T("Disk operation"));
#endif
  DiskEmu.Update(2); // get TR etc.
  RemoveDisk(); // remove current disk first thing
  EasyStr OriginalFile=FilePath,NewZipTemp;
  disk.ReadOnly=((GetFileAttributes(FilePath)&FILE_ATTRIBUTE_READONLY)!=0);
  Str Ext;
  //TODO since it's || exclusive it could be refactored
  bool ST=false,MSA=false,STT=false,DIM=false,f_PastiDisk=false;
  bool IPF=false,CTR=false,SCP=false,STW=false,PRG=false,TOS=false,HFE=false;
  disk.crc32=0;
  char *dot=strrchr(FilePath,'.');
  if(dot) 
    Ext=dot+1;
  int Type=ExtensionIsDisk(dot);

#if defined(SSE_DISK_M3U)
  if(Type==DISK_IS_PLAYLIST) // we check here and not below so that zip files can be in the list
  {
    if(DumbDiskSwapper.Open(FilePath)>0)
    {
      OriginalFile=FilePath=DumbDiskSwapper.GetPath(0); // we open first disk of the list
      dot=strrchr(OriginalFile,'.');
      if(dot)
        Ext=dot+1;
      Type=ExtensionIsDisk(dot);
    }
  }
#endif

  // NewDiskInZip will be blank for default disk, RealDiskInZip will be the
  // actual name of the file in the zip that is a disk image
  EasyStr NewDiskInZip,RealDiskInZip;
  switch(Type) {
  case DISK_COMPRESSED:
  {
    int HOffset=-1;
    bool CorruptZip=true;
    if(zippy.first(FilePath)==ZIPPY_SUCCEED)
    {
      CorruptZip=false;
      do
      {
        EasyStr filename=zippy.filename_in_zip();
        Type=FileIsDisk(filename);
        TRACE_LOG("%s %s Type %d\n",FilePath.Text,filename.Text,Type);
        if(Type==DISK_UNCOMPRESSED||Type==DISK_PASTI)
        {
          // look for CompressedDiskName, if any, or take first
          if(CompressedDiskName.Empty()||IsSameStr_I(CompressedDiskName,filename.Text))
          {
            MSA=has_extension(filename,DISK_EXT_MSA);
            ST=has_extension(filename,DISK_EXT_ST);
            if(Type==DISK_PASTI)
              f_PastiDisk=true;
            else
            {
              STT=has_extension(filename,DISK_EXT_STT);
              DIM=has_extension(filename,DISK_EXT_DIM);
#if defined(SSE_DISK_CAPS)
              IPF=has_extension(filename,DISK_EXT_IPF);
              CTR=has_extension(filename,DISK_EXT_CTR);
#endif
#if defined(SSE_DISK_SCP)
              SCP=has_extension(filename,DISK_EXT_SCP);
#endif
#if defined(SSE_DISK_STW)
              STW=has_extension(filename,DISK_EXT_STW);
#endif
#if defined(SSE_DISK_HFE)
              HFE=has_extension(filename,DISK_EXT_HFE);
#endif
#if defined(SSE_TOS_PRG_AUTORUN)
              PRG=OPTION_PRG_SUPPORT && has_extension(filename,DISK_EXT_PRG);
              TOS=OPTION_PRG_SUPPORT && has_extension(filename,DISK_EXT_TOS);
#endif
            }
            HOffset=zippy.current_file_offset;
            NewDiskInZip=CompressedDiskName;
            RealDiskInZip=filename.Text;
            break;
          }
        }
      } while(zippy.next()==ZIPPY_SUCCEED);
    }
    zippy.close();
    if(HOffset!=-1)
    {
      NewZipTemp.SetLength(SSE_MAX_PATH);
      GetTempFileName(TempPath,"ZIP",0,NewZipTemp);
      if(zippy.extract_file(FilePath,HOffset,NewZipTemp)==ZIPPY_FAIL)
      {
        //TRACE_LOG("Failed to extract%s\n",NewZipTemp.c_str());
        DeleteFile(NewZipTemp);
        return FIMAGE_WRONGFORMAT;
      }
      else
      {
        disk.crc32=zippy.crc;
        TRACE_LOG("file #%d %s %08X\n",HOffset,"CRC32",zippy.crc);
      }
      FilePath=NewZipTemp;
      disk.ReadOnly=!DiskMan.bArchiveRW;
    }
    else
    {
      if(CorruptZip) 
        return FIMAGE_CORRUPTZIP;
      if(Type==DISK_NODLL)
        return FIMAGE_MISSING_DLL;
      return FIMAGE_NODISKSINZIP;
    }
    break;
  }
  case DISK_NODLL:
  case DISK_COMPRESSED_NODLL: // won't appear in list anyway
    return FIMAGE_MISSING_DLL;
  case DISK_PASTI:
    f_PastiDisk=true;
    break;
  case DISK_IS_CONFIG: // wrench
  {
    TConfigStoreFile CSF; //on the stack
    bool ok=CSF.Open(FilePath);
    if(ok)
    {
      LoadAllDialogData(false,"",NULL,&CSF);
      ROMFile=CSF.GetStr("Machine","ROM_File",ROMFile);
      // add current TOS path if necessary
      if(strchr(ROMFile.Text,SLASHCHAR)==NULL) // no slash = no path
      {
        EasyStr tmp=OptionBox.TOSBrowseDir+SLASH+ROMFile;
        ROMFile=tmp;
        Tos.UpdateTOSPath(&ROMFile);
      }
      OptionBox.NewROMFile=ROMFile;
      CSF.Close();
      reset_st(RESET_COLD|RESET_STOP|RESET_CHANGESETTINGS|RESET_BACKUP|RESET_COUNT);
      SetForegroundWindow(StemWin);
    }
    CSF.Close();
    return (ok) ? FIMAGE_IS_CONFIG : FIMAGE_WRONGFORMAT;
  }
  case 0:
    TRACE_LOG("Disk type 0\n");
    return FIMAGE_WRONGFORMAT;
  default:
    ST=IsSameStr_I(Ext,DISK_EXT_ST);
    MSA=IsSameStr_I(Ext,DISK_EXT_MSA);
    STT=IsSameStr_I(Ext,DISK_EXT_STT);
    DIM=IsSameStr_I(Ext,DISK_EXT_DIM);
#if defined(SSE_DISK_CAPS)
    IPF=IsSameStr_I(Ext,DISK_EXT_IPF);
    CTR=IsSameStr_I(Ext,DISK_EXT_CTR);
#endif
#if defined(SSE_DISK_SCP)
    SCP=IsSameStr_I(Ext,DISK_EXT_SCP);
#endif
#if defined(SSE_DISK_STW)
    STW=IsSameStr_I(Ext,DISK_EXT_STW);
#endif
#if defined(SSE_TOS_PRG_AUTORUN)
    PRG=OPTION_PRG_SUPPORT && IsSameStr_I(Ext,DISK_EXT_PRG);
    TOS=OPTION_PRG_SUPPORT && IsSameStr_I(Ext,DISK_EXT_TOS);
#endif
#if defined(SSE_DISK_HFE)
    HFE=IsSameStr_I(Ext,DISK_EXT_HFE);
#endif
    break;
  }//switch(Type)
  if(disk.crc32==0)
    disk.crc32=GetCRCFromFile(FilePath);
#if defined(SSE_DISK_STX)
  disk.STX_File=false;
  if(f_PastiDisk && !SSEConfig.PastiDll) // no plugin?
  {
    f_PastiDisk=false;
    if(!ImageSTX[Id].Open(FilePath))
      return FIMAGE_CANTOPEN;
    ImageType.Manager=MNGR_STEEM; // Steem will handle the image itself
    ImageType.Extension=EXT_STX;
    disk.STX_File=true;
  } else
#else
  if(Id>DRIVE_B)
    f_PastiDisk=false; // Never use pasti for extra drives
#endif
  if(f_PastiDisk)
  {
#if USE_PASTI
    int Ret=0;
    if(Id<=DRIVE_B&&hPasti)
    {
      FILE *nf=fopen(FilePath,"rb");
      if(nf)
      {
        disk.PastiBufLen=(int)GetFileLength(nf);
        disk.PastiBuf=new BYTE[disk.PastiBufLen+16];
        FREAD(disk.PastiBuf,1,disk.PastiBufLen,nf);
        fclose(nf);
        Str RealFile=FilePath;
        if(NewZipTemp.NotEmpty()) 
          RealFile=RealDiskInZip;
        struct pastiDISKIMGINFO pdi;
        pdi.imgType=PASTI_ITPROT;
        ImageType.Manager=MNGR_PASTI;
        if(has_extension(RealFile,extension_list[EXT_ST])) 
        {
          pdi.imgType=PASTI_ITST;
          ImageType.Extension=EXT_ST;
        }
        else if(has_extension(RealFile,extension_list[EXT_MSA])) 
        {
          pdi.imgType=PASTI_ITMSA;
          ImageType.Extension=EXT_MSA;
        }
        else
          ImageType.Extension=EXT_STX;
        pdi.mode=(pdi.imgType==PASTI_ITPROT && NewZipTemp.Empty()) 
          ? PASTI_LDFNAME : PASTI_LDMEM;
        pdi.fileName=RealFile;
        pdi.fileBuf=disk.PastiBuf;
        pdi.fileLength=disk.PastiBufLen;
        pdi.bufSize=disk.PastiBufLen+16;
        pdi.bDirty=FALSE;
        BOOL bMediachDelay=TRUE;
#if defined(SSE_420R2B)
        if(LITTLE_PC==SafeLPeek(4))
#else
        if(LITTLE_PC==rom_addr)
#endif
          bMediachDelay=FALSE;
        if(!pasti->ImgLoad(Id,disk.WriteProtect,bMediachDelay,A_S_T,&pdi))
          Ret=FIMAGE_CANTOPEN;
      }
      else
        Ret=FIMAGE_CANTOPEN;
    }
    else
      Ret=FIMAGE_WRONGFORMAT;
    if(Ret)
    {
      if(disk.PastiBuf)
        delete[] disk.PastiBuf;
      disk.PastiBuf=NULL;
      if(NewZipTemp.NotEmpty()) 
        DeleteFile(NewZipTemp);
      return Ret;
    }
#endif//USE_PASTI
  }
#if defined(SSE_DISK_CAPS)
  else if(CAPSIMG_OK&&(CTR||IPF))
  {
    if(Id>DRIVE_B) // no extra drive
      return FIMAGE_WRONGFORMAT;
    CapsImageInfo img_info;
    int ext=(IPF)?EXT_IPF:EXT_CTR;
    if(Caps.InsertDisk(Id,FilePath,&img_info))
      return FIMAGE_WRONGFORMAT;
    ImageType.Manager=MNGR_CAPS;
    ImageType.Extension=(BYTE)ext;
  }
#endif
#if defined(SSE_DISK_SCP)
  else if(SCP)
  {
    if(!ImageSCP[Id].Open(FilePath))
      return FIMAGE_WRONGFORMAT;
    FloppyDisk[Id].TrackBytes=ImageSCP[Id].nBytes;
    ImageType.Manager=MNGR_WD1772;
    ImageType.Extension=EXT_SCP;
    reading=writing=false; //?
  }
#endif
#if defined(SSE_DISK_STW)
  else if(STW)
  {
    if(!ImageSTW[Id].Open(FilePath)) // extra drive OK
      return FIMAGE_WRONGFORMAT;
    ImageType.Manager=MNGR_WD1772;
    ImageType.Extension=EXT_STW;
    reading=writing=0;
    BYTE pdata[SECTOR_SIZE]; // sector buffer
    if(ImageSTW[Id].GetSectorData(0,0,1,pdata)) // boot sector for disk properties
    {
      disk.BytesPerSector=*(WORD*)(pdata+0xB);
      WORD total_sectors=*(WORD*)(pdata+0x13);
      disk.SectorsPerTrack=*(WORD*)(pdata+0x18);
      disk.Sides=*(WORD*)(pdata+0x1A);
      if(disk.SectorsPerTrack && disk.Sides) // image could be corrupt
        disk.TracksPerSide=(total_sectors/disk.SectorsPerTrack)/disk.Sides;
      disk.DiskFileLen=total_sectors*disk.BytesPerSector;
      disk.ValidBPB=( (disk.Sides==1||disk.Sides==2) && disk.SectorsPerTrack>5
        && disk.SectorsPerTrack<12  && total_sectors>359); // guess
    }
#if 0
    // using Steem to convert, because HxC software counts too many bytes
    // see just above
    // this code is only compiled on demand, it's no Steem feature
    if(floppy_no==1&&FloppyDrive[DRIVE_A].ImageType.Extension==EXT_HFE)
    {
      TRACE("Converting HFE to STW...\n");
      for(int si=0;si<2;si++)
        for(int tr=0;tr<ImageHFE[0].file_header.number_of_track;tr++)
        {
          ImageHFE[0].LoadTrack(si,tr);
          ImageSTW[1].LoadTrack(si,tr);
          for(int by=0;by<FloppyDisk[DRIVE_B].TrackBytes;by++)
          {
            WORD mfm=ImageHFE[0].GetMfmData(by);
            ImageSTW[1].SetMfmData(by,mfm);
          }
        }
      WrittenTo=true;
      ImageSTW[1].Close();
    }
#endif
  }
#endif//stw
#if defined(SSE_DISK_HFE)
  else if(HFE)
  {
    if(Id>DRIVE_B||!ImageHFE[Id].Open(FilePath)) //?
      return FIMAGE_WRONGFORMAT;
    FloppyDisk[Id].TrackBytes=DISK_BYTES_PER_TRACK; //default
    // check if HD disk
    if(ImageHFE[Id].file_header->floppyinterfacemode==ATARIST_HD_FLOPPYMODE)
      disk.TrackBytes*=2;
    ImageType.Manager=MNGR_WD1772;
    ImageType.Extension=EXT_HFE;
    reading=writing=false; //?
  }
#endif//hfe
#if defined(SSE_TOS_PRG_AUTORUN)
/*  v3.7
    Support for PRG and TOS files
    Not disk images but single PRG or TOS files may be selected.
    In that case we copy to harddisk Z: and boot from it.
    This approach minimises RAM/code use.
*/
  else if(PRG||TOS)
  {
    SSEConfig.old_DisableHardDrives=HardDiskMan.DisableHardDrives;
    HardDiskMan.DisableHardDrives=false; // or mount path is wrong
    HardDiskMan.update_mount();
    Str &PrgPath=Stemdos.MountPath[AUTORUN_HD]; // Z: (2+'Z'-'C')
    TRACE_LOG("PRG/TOS %c:=%s\n",AUTORUN_HD+'A',CHECKPATH(PrgPath.Text));
    BOOL ok=!strncmp(PrgPath.Rights(3),"PRG",3);  //our dedicated folder
    if(ok)
    {
      ImageType.Manager=MNGR_PRG; //and drive not empty
      ImageType.Extension=(BYTE)((TOS)?EXT_TOS:EXT_PRG);
      Str NewPath,AutoPath,RootPath;
      RootPath=PrgPath+SLASH+"AUTORUN.PRG";
      if(Exists(RootPath.Text))
        ok=DeleteFile(RootPath.Text); // delete this first or it's run on STE instead of .TOS program
      AutoPath=PrgPath+SLASH+"AUTO"+SLASH+"AUTORUN.PRG";
      if(ok&&Exists(AutoPath.Text))
        ok=DeleteFile(AutoPath.Text); // anyway
      if(TOS||tos_version<=0x102||tos_version>=0x200) // TODO 205+6?
        NewPath=AutoPath; // we'll use AUTO, provided in PRG folder
      else
        NewPath=RootPath; // we'll use DESKTOP.INF, provided in PRG folder
      if(ok)
        ok=CopyFile(FilePath.Text,NewPath.Text,FALSE); // there's no CopyFile in Unix
      TRACE_LOG("copy %s to %s\n",FilePath.Text,NewPath.Text);
    }
    if(!ok)
    {
      HardDiskMan.DisableHardDrives=!!SSEConfig.old_DisableHardDrives;
      return FIMAGE_CANTOPEN;
    }
  }
#endif//prg  
  else // regular disk images + STT
  {
    FloppyDisk[Id].TrackBytes=DISK_BYTES_PER_TRACK; // should be 6256
    ImageType.Manager=MNGR_STEEM;
    if(MSA)
      ImageType.Extension=EXT_MSA;
    else if(DIM)
      ImageType.Extension=EXT_DIM;
    else if(STT)
      ImageType.Extension=EXT_STT;
    else if(ST) // default
      ImageType.Extension=EXT_ST;
    // Open for read for an MSA (going to convert to ST and write to that)
    // and if the file is read-only, otherwise open for update
    FILE *nf=fopen(FilePath,(MSA||disk.ReadOnly) ? "rb" : "r+b");
    if(nf==NULL)
    {
      if(NewZipTemp.NotEmpty()) 
        DeleteFile(NewZipTemp);
      return FIMAGE_CANTOPEN;
    }
    if(GetFileLength(nf)<512)
    {
      TRACE_LOG("File length of %s = %d\n",CHECKPATH(FilePath.c_str()),GetFileLength(nf));
      fclose(nf);
      if(NewZipTemp.NotEmpty()) 
        DeleteFile(NewZipTemp);
      return FIMAGE_WRONGFORMAT;
    }
    FSEEK(nf,0,SEEK_SET);
    EasyStr NewMSATemp="";
    short MSA_SecsPerTrack=0,MSA_EndTrack=0,MSA_Sides=0;
    if(MSA)
    {
      NewMSATemp.SetLength(SSE_MAX_PATH);
      GetTempFileName(TempPath,extension_list[EXT_MSA],0,NewMSATemp);
      FILE *tf=fopen(NewMSATemp,"wb");
      if(tf)
      {
        bool Err=false;
        short ID,StartTrack;
        FSEEK(nf,0,SEEK_SET);
        // Read header
        FREAD(&ID,2,1,nf);               SWAP_BIG_ENDIAN_WORD(ID);
        FREAD(&MSA_SecsPerTrack,2,1,nf); SWAP_BIG_ENDIAN_WORD(MSA_SecsPerTrack);
        FREAD(&MSA_Sides,2,1,nf);        SWAP_BIG_ENDIAN_WORD(MSA_Sides);
        FREAD(&StartTrack,2,1,nf);       SWAP_BIG_ENDIAN_WORD(StartTrack);
        FREAD(&MSA_EndTrack,2,1,nf);     SWAP_BIG_ENDIAN_WORD(MSA_EndTrack);
/*
Header:
  Word	ID marker, should be $0E0F
  Word	Sectors per track
  Word	Sides (0 or 1; add 1 to this to get correct number of sides)
  Word	Starting track (0-based)
  Word	Ending track (0-based)
*/
        TRACE_LOG("MSA ID %X sides %d tracks %d (%d-%d) sectors %d\n",ID,
          MSA_Sides+1,MSA_EndTrack-StartTrack+1,StartTrack,MSA_EndTrack,MSA_SecsPerTrack);
        if(MSA_SecsPerTrack<1||MSA_SecsPerTrack>FLOPPY_MAX_SECTOR_NUM||MSA_Sides<0||MSA_Sides>1
          ||StartTrack<0||StartTrack>FLOPPY_MAX_TRACK_NUM||StartTrack>=MSA_EndTrack
          ||MSA_EndTrack<1||MSA_EndTrack>FLOPPY_MAX_TRACK_NUM)
        {
          Err=true;
        }
        if(!Err)
        {
          // Read data
          WORD Len,NumRepeats;
          BYTE *TrackData=new BYTE[(MSA_SecsPerTrack*SECTOR_SIZE)+16];
          BYTE *pDat,*pEndDat,dat;
          BYTE *STBuf=new BYTE[(MSA_SecsPerTrack*SECTOR_SIZE)+16];
          BYTE *pSTBuf,*pSTBufEnd=STBuf+(MSA_SecsPerTrack*SECTOR_SIZE)+8;
          for(int n=0;n<=MSA_EndTrack;n++)
          {
            for(int s=0;s<=MSA_Sides;s++)
            {
              if(n>=StartTrack)
              {
                Len=0;
                FREAD(&Len,1,2,nf); SWAP_BIG_ENDIAN_WORD(Len);
                if(Len>MSA_SecsPerTrack*SECTOR_SIZE||Len==0)
                {
                  Err=true;
                  break;
                }
                if((WORD)FREAD(TrackData,1,Len,nf)<Len)
                {
                  Err=true;
                  break;
                }
                if(Len==(MSA_SecsPerTrack*SECTOR_SIZE))
                  FWRITE(TrackData,Len,1,tf);
                else
                {
                  // Convert compressed MSA format track in TrackData to ST format in STBuf
                  pSTBuf=STBuf;
                  pDat=TrackData;
                  pEndDat=TrackData+Len;
                  while(pDat<pEndDat && pSTBuf<pSTBufEnd)
                  {
                    dat=*(pDat++);
                    if(dat==0xE5)
                    {
                      dat=*(pDat++);
                      NumRepeats=*(LPWORD)pDat;
                      pDat+=2;
                      SWAP_BIG_ENDIAN_WORD(NumRepeats);
                      for(int s2=0;s2<NumRepeats && pSTBuf<pSTBufEnd;s2++)
                        *(pSTBuf++)=dat;
                    }
                    else
                      *(pSTBuf++)=dat;
                  }
                  if(pSTBuf>=pSTBufEnd)
                  {
                    Err=true;
                    break;
                  }
                  FWRITE(STBuf,MSA_SecsPerTrack*SECTOR_SIZE,1,tf);
                }
              }
              else
              {
                ZeroMemory(TrackData,MSA_SecsPerTrack*SECTOR_SIZE);
                if(n==0&&s==0)
                {   // Write BPB
                  *(LPWORD)(TrackData+11)=SECTOR_SIZE;
                  TrackData[13]=2;           // SectorsPerCluster
                  *(LPWORD)(TrackData+17)=112; // nDirEntries
                  *(LPWORD)(TrackData+19)=(WORD)(MSA_EndTrack * MSA_SecsPerTrack);
                  *(LPWORD)(TrackData+22)=3;   // SectorsPerFAT
                  *(LPWORD)(TrackData+24)=MSA_SecsPerTrack;
                  *(LPWORD)(TrackData+26)=MSA_Sides;
                  *(LPWORD)(TrackData+28)=0;
                }
                FWRITE(TrackData,MSA_SecsPerTrack*SECTOR_SIZE,1,tf);
              }
            }//nxt s
            if(Err) 
              break;
          }//nxt n
          delete[] TrackData;
          delete[] STBuf;
        }
        fclose(tf);
        fclose(nf);
        if(!Err)
        {
#if !defined(SSE_WRITEDIR)
          SetFileAttributes(NewMSATemp,FILE_ATTRIBUTE_HIDDEN);
#endif
          nf=fopen(NewMSATemp,"r+b");
          Err=(nf==NULL);
        }
        if(Err)
        {
          TRACE_LOG("Error opening %s\n",CHECKPATH(NewMSATemp.c_str()));
          DeleteFile(NewMSATemp);
          if(NewZipTemp.NotEmpty()) 
            DeleteFile(NewZipTemp);
          return FIMAGE_WRONGFORMAT;
        }
      }
      else
      {
          // Couldn't open NewMSATemp
        fclose(nf);
        if(NewZipTemp.NotEmpty()) 
          DeleteFile(NewZipTemp);
        return FIMAGE_CANTOPEN;
      }
    }//MSA
    bool f_ValidBPB=true;
    long f_DiskFileLen=(long)GetFileLength(nf);
    if(STT)
    {
      DWORD Magic;
      WORD Version,Flags,AllTrackFlags,NumTracks,NumSides;
      FREAD(&Magic,4,1,nf); //TODO big-endian?
      FREAD(&Version,2,1,nf);
      FREAD(&Flags,2,1,nf);
      FREAD(&AllTrackFlags,2,1,nf);
      TRACE_LOG("STT flags %X\n",AllTrackFlags);
      FREAD(&NumTracks,2,1,nf);
      FREAD(&NumSides,2,1,nf);
      bool Err=(Magic!=MAKECHARCONST('S','T','E','M')||Version!=1||(AllTrackFlags&BIT_0)==0);
      if(!Err)
      {
        ZeroMemory(disk.STT_TrackStart,sizeof(disk.STT_TrackStart));
        ZeroMemory(disk.STT_TrackLen,sizeof(disk.STT_TrackLen));
        for(int s=0;s<NumSides;s++)
        {
          for(int t=0;t<NumTracks;t++)
          {
            FREAD(&disk.STT_TrackStart[s][t],4,1,nf);
            FREAD(&disk.STT_TrackLen[s][t],2,1,nf);
          }
        }
      }
      else
      {
        fclose(nf);
        if(NewMSATemp.NotEmpty()) 
          DeleteFile(NewMSATemp);
        if(NewZipTemp.NotEmpty()) 
          DeleteFile(NewZipTemp);
        return FIMAGE_WRONGFORMAT;
      }
      FSEEK(nf,0,SEEK_SET);
      disk.fp=nf;
      disk.STT_File=true;
      disk.TracksPerSide=NumTracks;
      disk.Sides=NumSides;
      disk.BytesPerSector=SECTOR_SIZE;
      disk.SectorsPerTrack=0xff; // Variable
    }
    else //!STT
    {
      TBpbInfo bpbi={0,0,0,0};
      int HeaderLen=(DIM) ? 32 : 0; // 0 for ST/MSA
      f_DiskFileLen-=HeaderLen;
      if(DIM)
      {
        int Err=0;
        FSEEK(nf,0,SEEK_SET);
        WORD Magic;
        FREAD(&Magic,1,2,nf);
        if(Magic!=0x4242)
          Err=FIMAGE_DIMNOMAGIC;
        else
        {
          BYTE UsedSectors;
          FSEEK(nf,3,SEEK_SET);
          FREAD(&UsedSectors,1,1,nf);
          if(UsedSectors!=0)
            Err=FIMAGE_DIMTYPENOTSUPPORTED;
        }
        if(Err)
        {
          fclose(nf);
          if(NewMSATemp.NotEmpty()) 
            DeleteFile(NewMSATemp);
          if(NewZipTemp.NotEmpty()) 
            DeleteFile(NewZipTemp);
          return Err;
        }
      }
      // Always append the name of the real disk file in the zip to the name
      // of the steembpb file, even if we are using default disk
      // This is so we don't need 2 .steembpb files for the default disk
      EasyStr BPBFile=OriginalFile+RealDiskInZip+".steembpb";
      bool HasBPBFile=(GetCSFInt("BPB","Sides",0,BPBFile)!=0);
      if(MSA)
        bpbi.BytesPerSector=SECTOR_SIZE; // MSA no choice
      else
      {
        FSEEK(nf,HeaderLen+11,SEEK_SET);
        FREAD(&bpbi.BytesPerSector,2,1,nf); // .ST, .DIM, we have choice?
      }
      FSEEK(nf,HeaderLen+19,SEEK_SET);
      FREAD(&bpbi.Sectors,2,1,nf);
      FSEEK(nf,HeaderLen+24,SEEK_SET);
      FREAD(&bpbi.SectorsPerTrack,2,1,nf);
      FREAD(&bpbi.Sides,2,1,nf);
      TRACE_LOG("disk BPB %d %d %d %d\n",bpbi.Sides,bpbi.BytesPerSector,bpbi.SectorsPerTrack,bpbi.Sectors);
      if(pFileBPB) 
        *pFileBPB=bpbi; // Store BPB exactly as it is in the file (for DiskMan)
      // A BPB is corrupt when one of its fields is totally wrong
      bool BPBCorrupt=false;
      if(bpbi.BytesPerSector!=128&&bpbi.BytesPerSector!=256&&
        bpbi.BytesPerSector!=512&&bpbi.BytesPerSector!=1024) 
        BPBCorrupt=true; // 1024 possible
      if(bpbi.SectorsPerTrack<1||bpbi.SectorsPerTrack>FLOPPY_MAX_SECTOR_NUM) 
        BPBCorrupt=true;
      if(bpbi.Sides<1||bpbi.Sides>2) 
        BPBCorrupt=true;
      // Has to be exact length for Steem to accept it
      if(BPBCorrupt || (bpbi.Sectors*bpbi.BytesPerSector)!=f_DiskFileLen)
      {
        f_ValidBPB=false;
        // If the BPB is only a few sectors out then we don't want to destroy
        // the value in BytesPerSector.
        if(BPBCorrupt)
          bpbi.BytesPerSector=SECTOR_SIZE; // 99.9% of ST disks used sectors this size
      }
      if(!f_ValidBPB)
      {
        if(MSA)
        {
          // Probably got a better chance of being right than guessing
          bpbi.SectorsPerTrack=MSA_SecsPerTrack;
          bpbi.Sides=MSA_Sides+1;
          bpbi.Sectors=(MSA_EndTrack+1)*bpbi.SectorsPerTrack*bpbi.Sides;
        }
        else
        {
          // BPB's wrong, time to guess the format
          bpbi.SectorsPerTrack=0;
          bpbi.Sectors=f_DiskFileLen/bpbi.BytesPerSector;
          bpbi.Sides=(bpbi.Sectors<1100) ? 1 : 2; // Total guess
          // Work out bpbi.SectorsPerTrack from bpbi.Sides and bpbi.Sectors
          bool Found=false;
          for(;;)
          {
            for(int t=75;t<=FLOPPY_MAX_TRACK_NUM;t++)
            {
              for(int s=8;s<=13;s++)
              {
                if(bpbi.Sectors==(t+1)*s*bpbi.Sides)
                {
                  bpbi.SectorsPerTrack=(WORD)s;
                  Found=true;
                  break;
                }
              }
              if(Found) 
                break;
            }
            if(Found) 
              break;
            if(bpbi.Sectors<10) 
              break;
            bpbi.Sectors--;
          }
          if(bpbi.SectorsPerTrack==0 && !HasBPBFile)
          {
            fclose(nf);
            if(NewMSATemp.NotEmpty()) 
              DeleteFile(NewMSATemp);
            if(NewZipTemp.NotEmpty()) 
              DeleteFile(NewZipTemp);
            return FIMAGE_WRONGFORMAT;
          }
        }
      }
      if(pDetectBPB) 
        *pDetectBPB=bpbi; // Steem's best guess (or the BPB if it is valid)
      if(HasBPBFile)
      {
        // User specified disk parameters
        TConfigStoreFile CSF(BPBFile);
        bpbi.Sides=CSF.GetInt("BPB","Sides",2);
        bpbi.SectorsPerTrack=CSF.GetInt("BPB","SectorsPerTrack",9);
        bpbi.BytesPerSector=CSF.GetInt("BPB","BytesPerSector",SECTOR_SIZE);
        bpbi.Sectors=CSF.GetInt("BPB","Sectors",1440);
        CSF.Close();
      }
      FSEEK(nf,HeaderLen,SEEK_SET);
      disk.fp=nf;
      disk.BytesPerSector=(short)bpbi.BytesPerSector;
      disk.SectorsPerTrack=(short)bpbi.SectorsPerTrack;
      disk.Sides=(short)bpbi.Sides;
      if(disk.SectorsPerTrack && disk.Sides)
        disk.TracksPerSide=(short)((short)(bpbi.Sectors/disk.SectorsPerTrack)/disk.Sides);
#if defined(SSE_SSE_DSKOS9) // ad hoc!!!!
      if(ST) {
        disk.BytesPerSector/=2; //256
        disk.SectorsPerTrack*=2; //16 (0-15)
        disk.TracksPerSide*=2;
      }
#endif
      disk.DIM_File=DIM;
    }
    disk.MSATempFile=NewMSATemp;
    disk.ValidBPB=f_ValidBPB;
    disk.DiskFileLen=f_DiskFileLen;
  }
  disk.ZipTempFile=NewZipTemp;
  disk.DiskInZip=RealDiskInZip;
  disk.ImageFile=OriginalFile;
  disk.PastiDisk=f_PastiDisk;
  if(ImageType.Extension)
    bDiskInDrive=true;
  disk.WrittenTo=false;
  //SS note that options haven't been retrieved yet when starting
  //steem and auto.sts is loaded with its disks -> we can't open a
  //ghost image here, we don't know if option is set 
//TODO, now we could...
  // Media change, write protect for 10 VBLs, unprotect for 10 VBLs, wp for 10
  if(this==&FloppyDrive[DRIVE_A]) 
    floppy_mediach[DRIVE_A]=FLOPPY_MEDIACH_VBL;
  if(this==&FloppyDrive[DRIVE_B]) 
    floppy_mediach[DRIVE_B]=FLOPPY_MEDIACH_VBL;
  // disable input for pasti
  if(!OPTION_AUTOSTW && Id<2 && ImageType.Manager==MNGR_STEEM // eg Union Demo STW!
    && ImageType.Extension!=EXT_STT
#if defined(SSE_DISK_STX)
    && ImageType.Extension!=EXT_STX
#endif
    && disk.SectorsPerTrack>=12 && disk.TrackBytes==DISK_BYTES_PER_TRACK)
  {
    disk.TrackBytes*=2; // we're still slow but maybe it's accurate! (ADAT)
  }
  ImageType.RealExtension=ImageType.Extension;
#if defined(SSE_DISK_M3U)
  // if current image is not current image of M3U swapper, deactivate dumb swapper
  if(disk.ImageFile!=DumbDiskSwapper.image_path)
    DumbDiskSwapper.image_path="";
#endif

#if defined(SSE_DISK_AUTOSTW)
  if(OPTION_AUTOSTW && Id<2 && ImageType.Manager==MNGR_STEEM && DiskInDrive()
#if defined(SSE_DISK_STX)
    && ImageType.Extension!=EXT_STX
#endif
    && ImageType.RealExtension!=EXT_STT)
  {
    // create temp STW, Steem will use it instead of the ST/MSA/DIM
    GetTempFileName(TempPath,"FMT",0,disk.StwPath); // see CleanupTempFiles()
    bool ok=STtoSTW(Id,disk.StwPath.Text);
    if(ok)
    {
      TRACE_LOG("Converted %s (%d-%d-%d) to STW\n",extension_list[ImageType.Extension],disk.Sides,disk.TracksPerSide,disk.SectorsPerTrack);
      ImageType.Manager=MNGR_WD1772;
      ImageType.Extension=EXT_STW;
      reading=writing=false;
      disk.WrittenTo=false; // set during conversion
    }
  }
#endif//#if defined(SSE_DISK_AUTOSTW)

  // determine DD or HD
#ifdef SSE_421R0 // avoid DD originals recognised  as HD
  disk.Density=(disk.SectorsPerTrack>=18 || disk.TrackBytes>=DISK_BYTES_PER_TRACK*2);
#else
  disk.Density=(disk.TrackBytes>DISK_BYTES_PER_TRACK //=*2
    || disk.SectorsPerTrack>11
    || disk.BytesPerSector>=1024 && disk.SectorsPerTrack>6) ? 2 : 1;
#endif

#if 0 // defined(SSE_GUI_421_REORDER)
  if(SSEOptions.ForceDiskDD) // one more safety
    disk.Density=1;
#endif

  TRACE("%c:%s (%s) MNGR_%s (%d)-%s %d-%d-%d R%c %s %X\n",'A'+Id,
         CHECKPATH(OriginalFile.Text),RealDiskInZip.Text,disk_manager[ImageType.Manager],ImageType.Manager,
         extension_list[ImageType.RealExtension],disk.Sides,disk.TracksPerSide,disk.SectorsPerTrack,
         (disk.ReadOnly?'O':'W'),"CRC32",disk.crc32);

  DiskEmu.LastManager=ImageType.Manager;
  UpdateAdat();
#if defined(SSE_DISK_SWAPPER)
  DiskMan.ArchiveNoMore=false;
#endif
#if defined(SSE_ENABLE_TRACE_LOG)
/*  TRACE_LOG("Manager %d (%s) Ext %s Sides %d Tracks %d Sectors %d adat %d\n",
    ImageType.Manager,disk_manager[ImageType.Manager],extension_list[ImageType.Extension],
    disk.Sides,disk.TracksPerSide,disk.SectorsPerTrack,bAdat);*/
  TRACE_LOG("Loading time: %dms\n",timeGetTime()-time0);
#endif
#if USE_PASTI
  // catch player's mistake
  if(ImageType.Manager!=MNGR_PASTI && pasti_active)
  {
    Alert(T("Disabling pasti.dll to run current disk image"),T("Warning"),0);
    pasti_active=false;
  }
#endif  
  if(ImageType.Manager!=MNGR_STEEM)
  {
    agenda_delete(agenda_fdc_motor_flag_off); // just in case
    if((Fdc.str&FDC_STR_MO)&&ImageType.Manager==MNGR_WD1772)
      time_of_next_ip=time_of_next_event+nSysCyclesPerSecond/5; // make sure we have IP
  }
  else if(!ADAT && (Fdc.str&FDC_STR_MO)) // assume drive was empty
    agenda_add(agenda_fdc_motor_flag_off,milliseconds_to_hbl(1800),0);
  // each time a disk is set, the WP status is first inherited from the RO status
  // but player can change it
  disk.WriteProtect=disk.ReadOnly;
#if defined(SSE_GUI_QUICKSETTINGS)
  if(Id==0 && OptionBox.IsVisible()) // if wrong page, does nothing wrong
    SetWindowText(GetDlgItem(OptionBox.Handle,IDS_DISCNAME),GetFileNameFromPath(disk.ImageFile));
#endif
  return ERR_OK;
}


void TSF314::RemoveDisk(bool LoseChanges) {
  TFloppyDisk &disk=FloppyDisk[Id]; // shorthand
#if !defined(SSE_LIBRETRONUKE)
  if(Id==DRIVE_A && (DiskMan.AutoInsert2&2))
    DiskMan.EjectDisk(DRIVE_B);
#endif
#if defined(SSE_EMU_THREAD)
  if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING && EmuThreadId!=GetCurrentThreadId())
    DiskLock.Lock();
#endif
  DiskMan.AutoInsert2&=~2;
  TRACE_LOG("Remove disk [%s] from drive %c\n",CHECKPATH(disk.DiskName.Text),'A'+Id);
#if defined(SSE_DISK_AUTOSTW)  
  if(OPTION_AUTOSTW && ImageType.Manager==MNGR_WD1772 && Id<=DRIVE_B
     && ImageType.Extension==EXT_STW && (ImageType.RealExtension==EXT_ST
     ||ImageType.RealExtension==EXT_MSA||ImageType.RealExtension==EXT_DIM))
  {
    if(!LoseChanges && disk.WrittenTo)
    {
#if defined(SSE_ENABLE_TRACE_LOG)
      if(STWtoST(Id)) // if not, write fails, image is closed
      {
        TRACE_LOG("Converted back (%d-%d-%d) from STW\n",disk.Sides,disk.TracksPerSide,disk.SectorsPerTrack);
      }
#endif
    }
    else
      ImageSTW[Id].Close(); // or it won't be deleted!
    // delete temp STW
    DeleteFile(disk.StwPath.Text); // no need to delete string
    ImageType.Manager=MNGR_STEEM; // temp
    ImageType.Extension=ImageType.RealExtension;
  }
#endif
#if defined(SSE_DISK_STX)
  if(disk.STX_File)
    ImageSTX[Id].Close();
  else
#endif
  if(disk.fp!=NULL && !disk.ReadOnly && !LoseChanges && disk.WrittenTo && disk.ZipTempFile.Empty())
  {
    short MSASecsPerTrack=0,MSAStartTrack=0,MSAEndTrack=0,MSASides=0;
    bool MSAResize=false;
    if(disk.Format_fp!=NULL)
    {
      if(disk.FormatLargestSector>0)
      { // Formatted any track properly?
        // Try to merge the formatted data on Format_fp with the old data on fp
        int MaxTrack=0;
        for(int Side=0;Side<2;Side++)
        {
          for(int Track=FLOPPY_MAX_TRACK_NUM;Track>0;Track--)
          {
            if(disk.TrackIsFormatted[Side][Track])
            {
              if(Track>MaxTrack)
                MaxTrack=Track;
              break;
            }
          }
        }
        if(MaxTrack>0)
        {
          bool CanShrink=true,WipeOld=true;
          // Only shrink if all tracks were written
          for(int Track=MaxTrack;Track>=1;Track--)
          {
            if(!disk.TrackIsFormatted[0][Track])
            {
              CanShrink=false;
              WipeOld=false;
              break;
            }
          }
          if(MaxTrack<70)
          { // Might want some old data left on the end
            CanShrink=false;
            WipeOld=false;
          }
          // Should we make it single-sided?
          int NewSides=1;
          for(int Track=MaxTrack;Track>=0;Track--)
          {
            if(disk.TrackIsFormatted[1][Track])
              NewSides=2;
            else
              // Don't wipe if haven't formatted over all sectors
              WipeOld=false;
          }
          if(!CanShrink) 
            NewSides=MAX(NewSides,(int)disk.Sides);
          int NewTracksPerSide=(CanShrink)?(MaxTrack+1):MAX((int)disk.TracksPerSide,MaxTrack+1);
          int NewSectorsPerTrack=(CanShrink) ? disk.FormatMostSectors
            : MAX((int)disk.SectorsPerTrack,disk.FormatMostSectors);
          size_t NewBytesPerSector=(WipeOld) ? disk.FormatLargestSector
            : MAX((int)disk.BytesPerSector,disk.FormatLargestSector);
          int NewBytesPerTrack=(int)NewBytesPerSector*NewSectorsPerTrack;
          int HeaderLen=(disk.DIM_File) ? 32 : 0;
          const size_t diskbuflen=HeaderLen+NewBytesPerSector*NewSectorsPerTrack
            *NewTracksPerSide*NewSides;
          BYTE *NewDiskBuf=new BYTE[diskbuflen];
          BYTE *lpNewDisk=NewDiskBuf;
          ZeroMemory(NewDiskBuf,diskbuflen);
          if(HeaderLen)
          {
            // Keep the header if there is one
            FSEEK(disk.fp,0,SEEK_SET);
            FREAD(lpNewDisk,HeaderLen,1,disk.fp);
            lpNewDisk+=HeaderLen;
          }
          for(SHORT t=0;t<NewTracksPerSide;t++)
          {
            for(SHORT Side=0;Side<NewSides;Side++)
            {
              int Countdown=3;
              if(disk.TrackIsFormatted[Side][t])
              {
                // Read a track from the format file
                for(SHORT s=1;s<=NewSectorsPerTrack;s++)
                {
                  bool NextSector=true;
                  disk.SeekSector(Side,t,s,true,false);
                  if(FREAD(lpNewDisk,1,NewBytesPerSector,disk.Format_fp)<NewBytesPerSector)
                  {
                    if((Countdown--)>0)
                    {
                      if(disk.ReopenFormatFile())
                      {
                        s--; // Try to redo a sector 3 times
                        NextSector=false;
                      }
                    }
                  }
                  if(NextSector) 
                    lpNewDisk+=NewBytesPerSector;
                }
              }
              else if(t<disk.TracksPerSide && Side<disk.Sides)
              {
                // Copy information from the old disk onto the new disk
                int nsec=MIN((int)disk.SectorsPerTrack,NewSectorsPerTrack);
                for(SHORT s=1;s<=nsec;s++)
                {
                  bool NextSector=true;
                  disk.SeekSector(Side,t,s,false,false);
                  size_t count=MIN((int)disk.BytesPerSector,(int)NewBytesPerSector);
                  if(FREAD(lpNewDisk,1,count,disk.fp)<count)
                  {
                    if((Countdown--)>0)
                    {
                      if(ReinsertDisk())
                      {
                        s--; // Try to redo a sector 3 times
                        NextSector=false;
                      }
                    }
                  }
                  if(NextSector) 
                    lpNewDisk+=NewBytesPerSector;
                }
                // If getting bigger then skip
                for(int s=disk.SectorsPerTrack;s<NewSectorsPerTrack;s++) 
                  lpNewDisk+=NewBytesPerSector;
              }
              else
                lpNewDisk+=NewBytesPerTrack;
            }
          }
          // Write it back to the original file (finally)
          int Countdown=3;
          for(;;)
          {
            fclose(FloppyDisk[Id].fp);
            disk.SectorsPerTrack=(short)NewSectorsPerTrack;
            disk.Sides=(short)NewSides;
            disk.TracksPerSide=(short)NewTracksPerSide;
            disk.BytesPerSector=(short)NewBytesPerSector;
            if(disk.MSATempFile.NotEmpty())
            {
              MSASecsPerTrack=(short)NewSectorsPerTrack;
              MSAEndTrack=(short)NewTracksPerSide+1;
              MSASides=(short)NewSides-1;
              MSAResize=true;
              disk.fp=fopen(disk.MSATempFile,"wb");
            }
            else
              disk.fp=fopen(disk.ImageFile,"wb");
            if(disk.fp)
            {
              FSEEK(disk.fp,0,SEEK_SET);
              size_t NewDiskSize=HeaderLen+NewBytesPerSector*NewSectorsPerTrack
                *NewTracksPerSide*NewSides;
              if(FWRITE(NewDiskBuf,1,NewDiskSize,disk.fp)==NewDiskSize)
              {
                TConfigStoreFile CSF(disk.ImageFile+".steembpb");
                CSF.SetStr("BPB","Sides",Str(disk.Sides));
                CSF.SetStr("BPB","SectorsPerTrack",Str(disk.SectorsPerTrack));
                CSF.SetStr("BPB","BytesPerSector",Str(disk.BytesPerSector));
                CSF.SetStr("BPB","Sectors",Str(disk.SectorsPerTrack*disk.TracksPerSide*disk.Sides));
                CSF.Close();
                break;
              }
              else
              {
                if((--Countdown)<0)
                {
                  break;
                }
              }
            }
            else
            {
              break;
            }
          }
          delete[] NewDiskBuf;
        }
      }
    }
    if(disk.fp && disk.MSATempFile.NotEmpty())
    {
      // Write ST format MSATempFile to MSA format ImageFile
#ifdef WIN32
      if(stem_mousemode!=STEM_MOUSEMODE_WINDOW)
        SetCursor(LoadCursor(NULL,IDC_WAIT));
#endif
      FILE *MSA=fopen(disk.ImageFile,"r+b");
      if(MSA)
      {
        BYTE Temp;
        FSEEK(MSA,2,SEEK_SET); //Seek past ID
        if(!MSAResize)
        {
          FREAD(&MSASecsPerTrack,2,1,MSA);
          SWAP_BIG_ENDIAN_WORD(MSASecsPerTrack);
          FREAD(&MSASides,2,1,MSA);
          SWAP_BIG_ENDIAN_WORD(MSASides);
          FSEEK(MSA,2,SEEK_CUR);
          // Skip StartTrack
          FREAD(&MSAEndTrack,2,1,MSA);
          SWAP_BIG_ENDIAN_WORD(MSAEndTrack);
          FSEEK(MSA,6,SEEK_SET);
          Temp=HIBYTE(MSAStartTrack);   FWRITE(&Temp,1,1,MSA);
          Temp=LOBYTE(MSAStartTrack);   FWRITE(&Temp,1,1,MSA);
        }
        else
        {
              // Write out MSA file info (in big endian)
          Temp=HIBYTE(MSASecsPerTrack); FWRITE(&Temp,1,1,MSA);
          Temp=LOBYTE(MSASecsPerTrack); FWRITE(&Temp,1,1,MSA);
          Temp=HIBYTE(MSASides);        FWRITE(&Temp,1,1,MSA);
          Temp=LOBYTE(MSASides);        FWRITE(&Temp,1,1,MSA);
          Temp=HIBYTE(MSAStartTrack);   FWRITE(&Temp,1,1,MSA);
          Temp=LOBYTE(MSAStartTrack);   FWRITE(&Temp,1,1,MSA);
          Temp=HIBYTE(MSAEndTrack);     FWRITE(&Temp,1,1,MSA);
          Temp=LOBYTE(MSAEndTrack);     FWRITE(&Temp,1,1,MSA);
        }
        FSEEK(MSA,10,SEEK_SET); // Past header
        FSEEK(disk.fp,0,SEEK_SET);
        int Len=(WORD)(MSASecsPerTrack*SECTOR_SIZE);
        // Convert ST format fp to MSA format MSA (uncompressed)
        int ReinsertAttempts=0;
        BYTE *MSADataBuf=new BYTE[(MSAEndTrack+1)*(MSASides+1)*MSASecsPerTrack*(SECTOR_SIZE+2)];
        BYTE *pD=MSADataBuf;
        for(SHORT t=0;t<=MSAEndTrack;t++)
        {
          for(SHORT s=0;s<=MSASides;s++)
          {
            *(pD++)=HIBYTE(Len);
            *(pD++)=LOBYTE(Len);
            for(SHORT sec=1;sec<=MSASecsPerTrack;sec++)
            {
              disk.SeekSector(s,t,sec,false,false);
              if(FREAD(pD,1,SECTOR_SIZE,disk.fp)==SECTOR_SIZE)
                // Read sector from ST file
                pD+=SECTOR_SIZE;
              else if(ReinsertAttempts<5)
              {
                ReinsertDisk();
                sec--;
                ReinsertAttempts++;
              }
              else
              { // All else has failed, write an empty sector
                ZeroMemory(pD,SECTOR_SIZE);
                pD+=SECTOR_SIZE;
              }
            }
          }
        }
        FWRITE(MSADataBuf,1,(LONG_PTR)(pD)-(LONG_PTR)(MSADataBuf),MSA);
        fclose(MSA);
        delete[] MSADataBuf;
      }
#ifdef WIN32
      if(stem_mousemode!=STEM_MOUSEMODE_WINDOW)
        SetCursor(PCArrow);
#endif
    }
  }
#if USE_PASTI
  if(disk.PastiDisk && Id<2) // not sure pasti expects 3 drives!
  {
    if(!disk.WriteProtect && hPasti && !LoseChanges && disk.ZipTempFile.Empty())
    {
      struct pastiDISKIMGINFO pdi;
      pdi.mode=PASTI_LDFNAME;
      pdi.fileName=disk.ImageFile;
      pdi.fileBuf=disk.PastiBuf;
      pdi.bufSize=disk.PastiBufLen;
    }
    pasti->Eject(Id,ABSOLUTE_SYS_TIME/TICKS8);
  }
#endif
  if(disk.PastiBuf) 
    delete[] disk.PastiBuf;
  disk.PastiBuf=NULL;
  disk.PastiBufLen=0;
  disk.PastiDisk=false;
#if defined(SSE_DISK_CAPS)
  if(CAPSIMG_OK && Id<2 && ImageType.Manager==MNGR_CAPS) // not sure caps expects 3 drives!
    Caps.RemoveDisk(Id);
#endif
  if(LoseChanges)
    disk.WrittenTo=false; // this is checked by MFM managers
  // use polymorphism for closing (for opening it wouldn't make it simpler)
  if(ImageType.Manager==MNGR_WD1772 && MfmManager && bDiskInDrive)
    MfmManager->Close();
  reading=writing=false; //? TODO
  if(disk.fp!=NULL) 
    fclose(disk.fp);
  if(disk.Format_fp!=NULL) 
    fclose(disk.Format_fp);
  disk.Format_fp=disk.fp=NULL;
#if defined(SSE_TOS_PRG_AUTORUN)
  if(ImageType.Manager==MNGR_PRG)
  {
    HardDiskMan.DisableHardDrives=!!SSEConfig.old_DisableHardDrives;
    HardDiskMan.update_mount();
#ifdef WIN32    
    HWND icon_handle=GetDlgItem(DiskMan.Handle,IDC_HDGEMDOS); // update GEMDOS HD icon
    SendMessage(icon_handle,BM_SETCHECK,!HardDiskMan.DisableHardDrives,0);
#endif
  }
#endif
  ImageType.Manager=(BYTE)((pasti_active)?MNGR_PASTI:((OPTION_AUTOSTW)?MNGR_WD1772:MNGR_STEEM));
  ImageType.Extension=EXT_NONE;
#if defined(SSE_DISK_AUTOSTW)  
  ImageType.RealExtension=ImageType.Extension;
#endif
  UpdateAdat(false);
#if defined(SSE_DISK_GHOST)
  // This makes sure to update the image before leaving
  if(OPTION_GHOST_DISK && bGhost)
  {
    //ASSERT(Id<2); // disk 2 will never be ghosted
    if(Id<2)
      GhostDisk[Id].Close();
    bGhost=false;
  }
#endif
  bDiskInDrive=false;
  if(disk.ZipTempFile.NotEmpty())    
    DeleteFile(disk.ZipTempFile);
  if(disk.MSATempFile.NotEmpty())    
    DeleteFile(disk.MSATempFile);
  if(disk.FormatTempFile.NotEmpty()) 
    DeleteFile(disk.FormatTempFile);
  disk.ImageFile=disk.MSATempFile=disk.ZipTempFile=disk.FormatTempFile=disk.DiskName="";
  disk.BytesPerSector=disk.Sides=disk.SectorsPerTrack=disk.TracksPerSide=0;
  ZeroMemory(disk.TrackIsFormatted,sizeof(disk.TrackIsFormatted));
  disk.FormatMostSectors=disk.FormatLargestSector=0;
  disk.STT_File=disk.DIM_File=false;
#if defined(SSE_STATS)
  if(Id<2)
    StatsStatic.maxSector[Id]=StatsStatic.maxSide[Id]=StatsStatic.maxTrack[Id]=0;
#endif
#if defined(SSE_EMU_THREAD)
  DiskLock.Unlock();
#endif
  FloppyDisk[Id].DiskName=FloppyDisk[Id].DiskInZip="";
  FloppyDisk[Id].Density=1;
}


EasyStr TSF314::GetDisk() { 
  return FloppyDisk[Id].GetImageFile(); 
}


bool TSF314::ReinsertDisk() {
  bool old_wp=FloppyDisk[Id].WriteProtect;
  bool old_ro=FloppyDisk[Id].ReadOnly;
  EasyStr Name=FloppyDisk[Id].DiskName;
  EasyStr DiskInZip=FloppyDisk[Id].DiskInZip;
  EasyStr DiskPath=FloppyDisk[Id].ImageFile;
  bool ok=DiskMan.InsertDisk(Id,Name,DiskPath,false,false,DiskInZip);
  if(old_ro==FloppyDisk[Id].ReadOnly&&old_wp!=FloppyDisk[Id].WriteProtect)
  {
    FloppyDisk[Id].WriteProtect=old_wp;
    ok=DiskMan.InsertDisk(Id,Name,DiskPath,true,false,DiskInZip);
  }
  return ok;
}

#undef LOGSECTION
