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

DOMAIN: File
FILE: dataloadsave.cpp
DESCRIPTION: The code to load and save all Steem's options (and there are
a lot of them) to and from the ini file.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <dataloadsave.h>
#include <debug.h>
#include <computer.h>
#include <diskman.h>
#include <translate.h>
#include <stjoy.h>
#include <shortcutbox.h>
#include <infobox.h>
#include <patchesbox.h>
#include <options.h>
#include <draw.h>
#include <display.h>
#include <harddiskman.h>
#include <key_table.h>
#include <osd.h>
#ifdef DEBUG_BUILD
#include <debugger.h>
#include <iolist.h>
#endif

bool bLoadPosition=false;

#if !defined(ONEGAME) && !defined(SSE_LIBRETRONUKE)

#if defined(SSE_GUI_RESTOREPOS)

#define UPDATE \
    if (Handle) Hide();  \
    if(bLoadPosition)LoadPosition(pCSF); \
    if (pCSF->GetInt(Section,"Visible",0)) Show();
#elif defined(SSE_420R4)
// v420R4: stop recording and loading dialog positions
#define UPDATE \
    if (pCSF->GetInt(Section,"Visible",0)) Show();

#else

#define UPDATE \
    if (Handle) Hide();  \
    LoadPosition(pCSF); \
    if (pCSF->GetInt(Section,"Visible",0)) Show();

#endif

#else

#define UPDATE

#endif

enum {
  PSEC_SNAP,
  PSEC_PASTE,
  PSEC_CUT,
  PSEC_PATCH,
  PSEC_MACHINETOS,
  PSEC_MACRO,
  PSEC_PORTS,
  PSEC_GENERAL,
  PSEC_SOUND,
  PSEC_DISPFULL,
  PSEC_STARTUP,
#if !defined(SSE_420R5)
  PSEC_AUTOUP,
#endif
  PSEC_JOY,
  PSEC_HARDDRIVES,
  PSEC_DISKEMU,
  PSEC_POSSIZE,//15
  PSEC_DISKGUI,
  PSEC_PCJOY,
  PSEC_OSD,
#if !defined(SSE_GUI_421_REORDER)
  PSEC_STVIDEO
#endif
};


TProfileSectionData ProfileSection[/*PSEC_NSECT+1*/]=
  {{"Machine and TOS",PSEC_MACHINETOS},
#if !defined(SSE_GUI_421_REORDER)
  {"ST Video",PSEC_STVIDEO},
#endif
  {"General",PSEC_GENERAL},
  {"Display",PSEC_DISPFULL},
  {"On Screen Display",PSEC_OSD},
  {"Steem Window Position and Size", PSEC_POSSIZE},
  {"Disk Emulation",PSEC_DISKEMU},
  {"Disk Manager",PSEC_DISKGUI},
  {"Hard Drives",PSEC_HARDDRIVES},
  {"Joysticks",PSEC_JOY},
  {"Ports and MIDI",PSEC_PORTS},
#ifdef UNIX
  {"PC Joysticks",PSEC_PCJOY},
#endif
  {"Sound",PSEC_SOUND},
  {"Shortcuts",PSEC_CUT},
  {"Macros",PSEC_MACRO},
  {"Patches",PSEC_PATCH},
  {"Startup",PSEC_STARTUP},
#if !defined(SSE_420R5)
#ifdef SSE_NO_UPDATE // TODO
  {"Update and version",PSEC_AUTOUP},
#else
  {"Auto Update and File Associations",PSEC_AUTOUP},
#endif
#endif
  {"Memory Snapshots",PSEC_SNAP},
  {"Paste Delay",PSEC_PASTE},
  {NULL,0xFF}};

#ifdef DEADC0DE
Str ProfileSectionGetStrFromID(int ID) {
  for(int i=0;;i++) 
  {
    if(ProfileSection[i].Name==NULL) 
      break;
    if(ProfileSection[i].ID==ID) 
      return ProfileSection[i].Name;
  }
  return "";
}
#endif


#define LOGSECTION LOGSECTION_INIT

/* Read and use settings of an ini file
   called by LoadState (startup), TOptionBox::LoadProfile, TSF314::SetDisk
   At startup, all sections are read
*/
void LoadAllDialogData(bool FirstLoad,Str INIFile,bool *SecDisabled,TConfigStoreFile *pCSF) {
  TRACE("%s %s\n","Load",CHECKPATH(INIFile.Text));
#if defined(SSE_GUI_STATUS_BAR_MOUSE)
  SSEConfig.MouseAd=0;// reset anyway!
#endif
  bool Temp[256];
  if(SecDisabled==NULL) 
  {
    ZeroMemory(Temp,sizeof(Temp));
    SecDisabled=Temp;
  }
  bool DeleteCSF=false;
  if(pCSF==NULL) 
  {
    pCSF=new TConfigStoreFile(INIFile);
    DeleteCSF=true;
  }

#if defined(SSE_GUI_RESTOREPOS)
  bLoadPosition=(pCSF->GetInt("Options","RestoreWinPos",bLoadPosition)!=0);
#endif

  //SEC(PSEC_SNAP) 
  if(!SecDisabled[PSEC_SNAP])
  {
    LastSnapShot=pCSF->GetStr("Main","LastSnapShot",UsersPath+SLASH+T(MEMORY_SNAPSHOTS)+SLASH);
    Str Dir=LastSnapShot;
    RemoveFileNameFromPath(Dir,REMOVE_SLASH);
    if(GetFileAttributes(Dir)==INVALID_FILE_ATTRIBUTES)
    {
#ifndef ONEGAME
      LastSnapShot=UsersPath+SLASH+T(MEMORY_SNAPSHOTS);
      CreateDirectory(LastSnapShot,NULL);
      LastSnapShot+=SLASH;
#else
      LastSnapShot=WriteDir+SLASH;
#endif
    }
    for(int n=0;n<STATE_HISTORY_LEN;n++)
      StateHist[n]=pCSF->GetStr("Main",Str("SnapShotHistory")+n,"");
#if defined(SSE_VID_LS)
    SSEOptions.ScreenshotWithSnapshot=pCSF->GetBool("Main","ScreenshotWithSnapshot",1);
#endif
  }
  //SEC(PSEC_PASTE) 
  if(!SecDisabled[PSEC_PASTE])
    PasteSpeed=pCSF->GetInt("Main","PasteSpeed",PasteSpeed);
#if defined(SSE_VID_2SCREENS)
  // we need that to get dialogs on correct screen
  if(!FirstLoad)//402R16
#endif
  {
    DiskMan.LoadData(FirstLoad,pCSF,SecDisabled);
    JoyConfig.LoadData(FirstLoad,pCSF,SecDisabled);
    OptionBox.LoadData(FirstLoad,pCSF,SecDisabled);
    InfoBox.LoadData(FirstLoad,pCSF,SecDisabled);
    ShortcutBox.LoadData(FirstLoad,pCSF,SecDisabled);
    PatchesBox.LoadData(FirstLoad,pCSF,SecDisabled);
  }
#ifndef ONEGAME
#ifdef SSE421R1
  int Left=MSW_NOCHANGE,Top=MSW_NOCHANGE,Width=MSW_NOCHANGE,Height=MSW_NOCHANGE;
#endif
  //SEC(PSEC_POSSIZE)
  if(!SecDisabled[PSEC_POSSIZE])
  {
#if !defined(SSE_NO_AOT)
    bAlwaysOnTop=pCSF->GetBool("Main","AOT",0);
#endif

#ifdef WIN32
#if !defined(SSE_NO_AOT)
    CheckMenuItem(StemWin_SysMenu,IDSYS_TOP,MF_BYCOMMAND|MF_CHECK(bAlwaysOnTop));
#endif
    if(!FirstLoad)
#if defined(SSE_NO_AOT)
      SetWindowPos(StemWin,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
#else
      SetWindowPos(StemWin,(bAlwaysOnTop) ? HWND_TOPMOST : HWND_NOTOPMOST,0,0,0,0,
        SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
#endif
#if defined(SSE_GUI_RESTOREPOS)
#ifndef SSE421R1
    int Left=MSW_NOCHANGE,Top=MSW_NOCHANGE,Width=MSW_NOCHANGE,Height=MSW_NOCHANGE;
#endif
    if(bLoadPosition)
    {
      Left=pCSF->GetInt("Main","Left",Left);
      Width=pCSF->GetInt("Main","Width",Width);
      Top=pCSF->GetInt("Main","Top",Top);
      Height=pCSF->GetInt("Main","Height",Height);
    }
#endif
#if defined(SSE_VID_2SCREENS) // Steem can start on any screen
#if !defined(SSE_GUI_RESTOREPOS)
    int Left=pCSF->GetInt("Main","Left",MSW_NOCHANGE);
    int Width=pCSF->GetInt("Main","Width",MSW_NOCHANGE);
    int Top=pCSF->GetInt("Main","Top",MSW_NOCHANGE);
    int Height=pCSF->GetInt("Main","Height",MSW_NOCHANGE);
#endif
#if defined(SSE_GUI_STATUS_BAR)
    OPTION_STATUS_BAR=pCSF->GetBool("Main","StatusBar",OPTION_STATUS_BAR);
#if defined(SSE_GUI_STATUS_BAR_CLICKABLE)
    SSEConfig.StatusBarMask=pCSF->GetByte("Main","StatusBarMask",SSEConfig.StatusBarMask);
#endif
    OPTION_TOSFLAG=pCSF->GetBool("Main","TosFlag",OPTION_TOSFLAG);
#endif
#if defined(SSE_GUI_TOOLBAR)
    OPTION_TOOLBAR=pCSF->GetBool("Main","ToolBar",OPTION_TOOLBAR);
    CheckMenuItem(StemWin_SysMenu,IDSYS_TOOLBAR,MF_BYCOMMAND|MF_CHECK(OPTION_TOOLBAR));
#endif
#if defined(SSE_GUI_MENUBAR)
    OPTION_MENUBAR=pCSF->GetBool("Main","MenuBar",OPTION_MENUBAR);
    SteemSetMenu(OPTION_MENUBAR);
#endif
#ifndef SSE421R1
    if(Left!=MSW_NOCHANGE
#ifndef SSE_LEAN_AND_MEAN
      && Width!=MSW_NOCHANGE && Top!=MSW_NOCHANGE && Height!=MSW_NOCHANGE
#endif
      )
    {
      WINDOWPLACEMENT wp;
      wp.length=sizeof(WINDOWPLACEMENT);
      GetWindowPlacement(StemWin,&wp);
      wp.rcNormalPosition.left=Left;
      wp.rcNormalPosition.top=Top;
      wp.rcNormalPosition.right=Left+Width;
      wp.rcNormalPosition.bottom=Top+Height;
      /*If the information specified in WINDOWPLACEMENT would result in a 
      window that is completely off the screen, the system will automatically 
      adjust the coordinates so that the window is visible, taking into account
      changes in screen resolution and multiple monitor configuration.*/
      SetWindowPlacement(StemWin,&wp);
    }
#endif//#ifndef SSE421R1

#else//#if defined(SSE_VID_2SCREENS)
#if defined(SSE_GUI_STATUS_BAR)
    OPTION_STATUS_BAR=pCSF->GetBool("Main","StatusBar",OPTION_STATUS_BAR);
#if defined(SSE_GUI_STATUS_BAR_CLICKABLE)
    SSEConfig.StatusBarMask=pCSF->GetByte("Main","StatusBarMask",SSEConfig.StatusBarMask);
#endif
#endif
#if defined(SSE_GUI_TOOLBAR)
    OPTION_TOOLBAR=pCSF->GetBool("Main","ToolBar",OPTION_TOOLBAR);
#endif
#if defined(SSE_GUI_MENUBAR)
    OPTION_MENUBAR=pCSF->GetBool("Main","MenuBar",OPTION_MENUBAR);
    SteemSetMenu(OPTION_MENUBAR);
#endif
#if defined(SSE_GUI_RESTOREPOS)
    if(bLoadPosition)
      MoveStemWin(Left,Top,Width,Height);
#else
    int Left=pCSF->GetInt("Main","Left",MSW_NOCHANGE);
    if(Left!=MSW_NOCHANGE) Left=MAX(MIN(Left,(int)GetScreenWidth()-100),-100);
    int Top=pCSF->GetInt("Main","Top",MSW_NOCHANGE);
    if(Top!=MSW_NOCHANGE) Top=MAX(MIN(Top,(int)GetScreenHeight()-100),-100);
    MoveStemWin(Left,Top,pCSF->GetInt("Main","Width",MSW_NOCHANGE),
      pCSF->GetInt("Main","Height",MSW_NOCHANGE));
#endif
#endif//#if defined(SSE_VID_2SCREENS)
#endif//WIN32

#ifdef UNIX
    Disp.GoToFullscreenOnRun=pCSF->GetInt("Main","GoToFullscreenOnRun",Disp.GoToFullscreenOnRun);
    FullScreenBut.set_check(Disp.GoToFullscreenOnRun);
#endif

  }
#if defined(SSE_VID_2SCREENS)
  if(FirstLoad)//402R16
  {
    DiskMan.LoadData(FirstLoad,pCSF,SecDisabled);
    JoyConfig.LoadData(FirstLoad,pCSF,SecDisabled);
    OptionBox.LoadData(FirstLoad,pCSF,SecDisabled);
    InfoBox.LoadData(FirstLoad,pCSF,SecDisabled);
    ShortcutBox.LoadData(FirstLoad,pCSF,SecDisabled);
    PatchesBox.LoadData(FirstLoad,pCSF,SecDisabled);
#ifdef WIN32
#ifndef SSE421R1
    SetFocus(StemWin); // is it what player wants?
#endif
#endif
  }

#ifdef SSE421R1 // after we read settings
    if(Left!=MSW_NOCHANGE
#ifndef SSE_LEAN_AND_MEAN
      && Width!=MSW_NOCHANGE && Top!=MSW_NOCHANGE && Height!=MSW_NOCHANGE
#endif
      )
    {
      WINDOWPLACEMENT wp;
      wp.length=sizeof(WINDOWPLACEMENT);
      GetWindowPlacement(StemWin,&wp);
      wp.rcNormalPosition.left=Left;
      wp.rcNormalPosition.top=Top;
      wp.rcNormalPosition.right=Left+Width;
      wp.rcNormalPosition.bottom=Top+Height;
      /*If the information specified in WINDOWPLACEMENT would result in a 
      window that is completely off the screen, the system will automatically 
      adjust the coordinates so that the window is visible, taking into account
      changes in screen resolution and multiple monitor configuration.*/
      SetWindowPlacement(StemWin,&wp);
    }
#endif//#ifdef SSE421R1

#endif
#if defined(SSE_420R4) // now we want full names by default
  Disp.ScreenShotUseFullName=pCSF->GetBool("Display","ScreenShotUseFullName",Disp.ScreenShotUseFullName);
  Disp.ScreenShotAlwaysAddNum=pCSF->GetBool("Display","ScreenShotAlwaysAddNum",Disp.ScreenShotAlwaysAddNum);
#else
  int i=pCSF->GetInt("Display","ScreenShotUseFullName",99);
  if(i==(i&1)) 
    Disp.ScreenShotUseFullName=(i&1);
  i=pCSF->GetInt("Display","ScreenShotAlwaysAddNum",99);
  if(i==(i&1)) 
    Disp.ScreenShotAlwaysAddNum=(i&1);
#endif
#else
  OGLoadData(pCSF);
#endif
  if(!FirstLoad && !!pCSF->GetInt("Options","RunOnStart",0))
  {
    CLICK_PLAY_BUTTON();
  }
  if(DeleteCSF) 
  {
    pCSF->Close();
    if(DeleteCSF) 
      delete pCSF;
  }
  PortsOpenAll();
#if defined(SSE_DEBUG_TRACE)
  Debug.TraceGeneralInfos(TDebug::LOAD);
#endif
  OptionBox.MachineUpdateIfVisible();
  OptionBox.SSEUpdateIfVisible();
}


#ifndef ONEGAME

void SaveAllDialogData(bool FinalSave,Str INIFile,TConfigStoreFile *pCSF) {
  // TODO SecDisabled for saving as well, it's a bit of work, but easy
  // however we guess Steem Authors had reason for doing it that way (on loading)?
  TRACE("%s %s\n","Save",CHECKPATH(INIFile.Text));
  bool DeleteCSF=false;
  if(pCSF==NULL)
  {
    pCSF=new TConfigStoreFile(INIFile);
    DeleteCSF=true;
  }
  if(AutoSnapShotName.Empty()) 
    AutoSnapShotName="autosave";
#if defined(SSE_VID_LS) && defined(SSE_DEFAULTS_421)
  pCSF->SetInt("Main","ScreenshotWithSnapshot",SSEOptions.ScreenshotWithSnapshot);
#endif
  TWinPositionData wpd={0,0,0,0,0,0}; // this is a Steem struct
  GetWindowPositionData(StemWin,&wpd); // this is a Steem function
  pCSF->SetStr("Main","Left",EasyStr(wpd.Left));
  pCSF->SetStr("Main","Top",EasyStr(wpd.Top));
  pCSF->SetStr("Main","Width",EasyStr(wpd.Width));
  pCSF->SetStr("Main","Height",EasyStr(wpd.Height));
  pCSF->SetStr("Main","Maximized",(LPSTR)((wpd.Maximized)? "1" : "0"));
#if !defined(SSE_NO_AOT)
  pCSF->SetStr("Main","AOT",(LPSTR)((bAlwaysOnTop)? "1" : "0"));
#endif
  pCSF->SetStr("Main","FontSize",EasyStr(FONT_SIZE));
  pCSF->SetStr("Main","BigIcons",EasyStr(BIG_ICONS));
#ifdef UNIX
  pCSF->SetInt("Main","GoToFullscreenOnRun",Disp.GoToFullscreenOnRun);
#endif
  pCSF->SetStr("Main","LastSnapShot",LastSnapShot);
  for(int n=0;n<STATE_HISTORY_LEN;n++)
    pCSF->SetStr("Main",Str("SnapShotHistory")+n,StateHist[n]);
  pCSF->SetInt("Main","PasteSpeed",PasteSpeed);
#if defined(SSE_420R4)
  pCSF->SetInt("Display","ScreenShotUseFullName",Disp.ScreenShotUseFullName);
  pCSF->SetInt("Display","ScreenShotAlwaysAddNum",Disp.ScreenShotAlwaysAddNum);
#else
  int i=pCSF->GetInt("Display","ScreenShotUseFullName",999);
  if(i==999) 
    pCSF->SetInt("Display","ScreenShotUseFullName",99);
  i=pCSF->GetInt("Display","ScreenShotAlwaysAddNum",999);
  if(i==999) 
    pCSF->SetInt("Display","ScreenShotAlwaysAddNum",99);
#endif

#if defined(SSE_LOGSECTIONS_FOR_ALL)
  pCSF->SetInt("Options","TraceFile",SSEConfig.TraceFile);
#endif

  DiskMan.SaveData(FinalSave,pCSF);
  JoyConfig.SaveData(FinalSave,pCSF);
  OptionBox.SaveData(FinalSave,pCSF);
  InfoBox.SaveData(FinalSave,pCSF);
  ShortcutBox.SaveData(FinalSave,pCSF);
  PatchesBox.SaveData(FinalSave,pCSF);
  if(DeleteCSF) 
  {
    pCSF->Close();
    delete pCSF;
  }
}

#else

void SaveAllDialogData(bool,Str,TConfigStoreFile*)
{
}

#endif

void TDiskManager::LoadData(bool FirstLoad,TConfigStoreFile *pCSF,bool *SecDisabled) {
  HardDiskMan.LoadData(FirstLoad,pCSF,SecDisabled); // load HD data first for PRG support
  //SEC(PSEC_DISKEMU)
  if(!SecDisabled[PSEC_DISKEMU])
  {
#if USE_PASTI
    pasti_active=(pCSF->GetBool("Disks","PastiActive",pasti_active))&&(hPasti!=NULL);
    EnableWindow(GetDlgItem(Handle,IDC_HDACSI),!pasti_active);
    if(hPasti)
    {
      EasyStringList sl(eslNoSort);
      pCSF->GetWholeSect(&sl,"Pasti",false);
      char Buf[8192],*p=Buf;
      ZeroMemory(Buf,sizeof(Buf));
      for(int i=0;i<sl.NumStrings;i++) 
      {
        strcpy(p,sl[i].String); // Key Name
        p+=strlen(p)+1;
        strcpy(p,pCSF->GetStr("Pasti",sl[i].String,"LOAD ERROR")); // Value 
        p+=strlen(p)+1;
      }
      pastiLOADINI pli;
      pli.mode=PASTI_LCSTRINGS;
      pli.name=NULL;
      pli.buffer=Buf;
      pli.bufSize=8192;
      pasti->LoadConfig(&pli,NULL);
    }
#endif
#if defined(SSE_DISK_GHOST)
    OPTION_GHOST_DISK=pCSF->GetBool("Disks","GhostDisk",OPTION_GHOST_DISK);
#endif
#if defined(SSE_DISK_AUTOSTW)
    OPTION_AUTOSTW=pCSF->GetBool("Disks","AutoSTW",OPTION_AUTOSTW);
#endif
    OPTION_COUNT_DMA_CYCLES=pCSF->GetBool("Disks","CountDmaCycles",OPTION_COUNT_DMA_CYCLES);
#if defined(SSE_DRIVE_SOUND)
    OPTION_DRIVE_SOUND=pCSF->GetBool("Disks","DriveSound",OPTION_DRIVE_SOUND);
    FloppyDrive[DRIVE_A].SoundVolume=pCSF->GetInt("Disks","DriveSoundVolume",FloppyDrive[0].SoundVolume);
    FloppyDrive[DRIVE_B].SoundVolume=FloppyDrive[DRIVE_A].SoundVolume;
#ifndef UNIX
    FloppyDrive[DRIVE_A].SoundChangeVolume();
    FloppyDrive[DRIVE_B].SoundChangeVolume();
#endif
    DriveSoundDir[DRIVE_A]=pCSF->GetStr(Section,"DriveSoundDirA",(FirstLoad)?
                                        (UsersPath+SLASH+"DriveSound"):DriveSoundDir[DRIVE_A]);
    DriveSoundDir[DRIVE_B]=pCSF->GetStr(Section,"DriveSoundDirB",(FirstLoad)?
                                        (UsersPath+SLASH+"DriveSound"):DriveSoundDir[DRIVE_B]);
#endif
    OPTION_PRG_SUPPORT=pCSF->GetBool("Disks","PRG_support",OPTION_PRG_SUPPORT);
#if defined(SSE_DRIVE_SINGLESIDE)
    // single-sided is part of the config (not freeboot)
    FloppyDrive[DRIVE_A].bSingleSided=pCSF->GetBool("Disks","SingleSidedA",0);
    FloppyDrive[DRIVE_B].bSingleSided=pCSF->GetBool("Disks","SingleSidedB",0);
#endif
    SetNumFloppies(pCSF->GetByte("Disks","NumFloppyDrives",nFloppyDrives));
    bTurboDrive=pCSF->GetBool("Disks","QuickDiskAccess",bTurboDrive);
    bArchiveRW=pCSF->GetBool("Disks","FloppyArchiveIsReadWrite",bArchiveRW);
    mBackground=pCSF->GetByte("Disks","Background",mBackground);
    floppy_access_ff=pCSF->GetBool("Disks","DiskAccessFF",floppy_access_ff);
    bDiskProtectImage=pCSF->GetBool("Disks","DiskProtectImage",bDiskProtectImage);
#if defined(SSE_DISK_STX)
    bDiskProtectImageStx=pCSF->GetBool("Disks","DiskProtectImageStx",bDiskProtectImageStx);
#endif
  }
  //SEC(PSEC_DISKGUI)
  if(!SecDisabled[PSEC_DISKGUI])
  {
    Width=pCSF->GetInt("Disks","Width",Width);
    Height=pCSF->GetInt("Disks","Height",Height);
    Maximized=pCSF->GetBool("Disks","Maximized",0);
    FSWidth=pCSF->GetInt("Disks","FSWidth",FSWidth);
    FSHeight=pCSF->GetInt("Disks","FSHeight",FSHeight);
    FSMaximized=pCSF->GetBool("Disks","FSMaximized",0);
#if defined(SSE_GUI_CONFIG)
    if(FirstLoad) // don't mess all settings when loading a config
#endif
    {
      HomeFol=pCSF->GetStr("Disks","HomeFolder",HomeFol);
      NO_SLASH(HomeFol);
      DisksFol=pCSF->GetStr("Disks","CurrentFolder",DisksFol);
      NO_SLASH(DisksFol);
      if(
#ifdef WIN32
        HomeFol.Empty()||
#endif
        HomeFol.Lefts(2)=="//")
        HomeFol=RunDir;
      else if(HomeFol.NotEmpty()) 
      {
        DWORD Attrib=GetFileAttributes(HomeFol);
        if((Attrib & FILE_ATTRIBUTE_DIRECTORY)==0||Attrib==INVALID_FILE_ATTRIBUTES)
          HomeFol=RunDir;
      }
      if(
#ifdef WIN32        
        DisksFol.Empty()||
#endif
        DisksFol.Lefts(2)=="//")
        DisksFol=RunDir;
      else if(DisksFol.NotEmpty()) 
      {
        DWORD Attrib=GetFileAttributes(DisksFol);
        if((Attrib & FILE_ATTRIBUTE_DIRECTORY)==0||Attrib==INVALID_FILE_ATTRIBUTES)
          DisksFol=HomeFol;
      }
      for(int n=0;n<10;n++)
        QuickFol[n]=pCSF->GetStr("Disks",EasyStr("QuickFol")+n,QuickFol[n]);
      for(int d=DRIVE_A;d<=DRIVE_B;d++)
      {
        FloppyDrive[d].ImageType.Manager=(BYTE)((pasti_active) ? MNGR_PASTI
                                             : (OPTION_AUTOSTW ? MNGR_WD1772 : MNGR_STEEM));
        for(int n=0;n<DM_HISTORY_LEN;n++) 
        {
          InsertHist[d][n].Name=pCSF->GetStr("Disks",EasyStr("InsertHistoryName")+d+n,
                                             InsertHist[d][n].Name);
          InsertHist[d][n].Path=pCSF->GetStr("Disks",EasyStr("InsertHistoryPath")+d+n,
                                             InsertHist[d][n].Path);
          InsertHist[d][n].DiskInZip=pCSF->GetStr("Disks",EasyStr("InsertHistoryDiskInZip")+d+n,
                                                  InsertHist[d][n].DiskInZip);
        }
        if(BootDisk[d].NotEmpty()&&FirstLoad)
          InsertHistoryAdd(d,FloppyDisk[d].DiskName,FloppyDrive[d].GetDisk(),FloppyDisk[d].DiskInZip);
      }
    }
    HideBroken=pCSF->GetBool("Disks","HideBroken",HideBroken);
    HideExtension=pCSF->GetBool("Disks","HideExtension",HideExtension);
    ShowHiddenFiles=pCSF->GetBool("Disks","ShowHiddenFiles",ShowHiddenFiles);
#ifdef WIN32
    ExplorerFolders=pCSF->GetBool("Disks","ExplorerFolders",ExplorerFolders);
#ifndef SSE_NO_WINSTON_IMPORT
    WinSTonPath=pCSF->GetStr("Disks","WinSTonPath",WinSTonPath);
    WinSTonDiskPath=pCSF->GetStr("Disks","WinSTonDiskPath",WinSTonDiskPath);
    ImportPath=pCSF->GetStr("Disks","ImportPath",ImportPath);
    ImportOnlyIfExist=(bool)pCSF->GetInt("Disks","ImportOnlyIfExist",ImportOnlyIfExist);
    ImportConflictAction=pCSF->GetInt("Disks","ImportConflictAction",ImportConflictAction);
#endif
    MSAConvPath=pCSF->GetStr("Disks","MSAConvPath",MSAConvPath);
#if defined(SSE_GUI_DM_BIGICONS)
    IconSize=(char)pCSF->GetInt("Disks","IconSize",-1);
    if(IconSize==-1) // not found
      IconSize=!pCSF->GetBool("Disks","SmallIcons",false); // base on old setting
#else
    SmallIcons=pCSF->GetBool("Disks","SmallIcons",SmallIcons);
#endif
    IconSpacing=pCSF->GetInt("Disks","IconSpacing",IconSpacing);
#ifdef SSE_421R0
    SortBy=pCSF->GetInt("Disks","SortBy",SortBy);
#endif
#endif//WIN32
    EjectDisksWhenQuit=pCSF->GetBool("Disks","EjectDisksWhenQuit",EjectDisksWhenQuit);
    DoubleClickAction=pCSF->GetInt("Disks","DoubleClickAction",DoubleClickAction);
    CloseAfterIRR=pCSF->GetBool("Disks","CloseAfterIRR",CloseAfterIRR);
    ContentListsFol=pCSF->GetStr("Disks","ContentListsFol",RunDir+SLASH+"contents");
#if defined(SSE_DISK_SWAPPER)
    bSwapperPattern=pCSF->GetBool("Disks","SwapperPattern",bSwapperPattern);
#endif
#if defined(SSE_DISK_M3U)
    DumbDiskSwapper.m3upath=pCSF->GetStr("Disks","SwapperM3uPath",DumbDiskSwapper.m3upath);
    DumbDiskSwapper.image_path=pCSF->GetStr("Disks","SwapperImagePath",DumbDiskSwapper.image_path);
    DumbDiskSwapper.image_index=pCSF->GetWord("Disks","SwapperImageIndex",DumbDiskSwapper.image_index);
    DumbDiskSwapper.num_images=pCSF->GetWord("Disks","SwapperNumImages",DumbDiskSwapper.num_images);
#endif
    if(BootDisk[DRIVE_A].Empty()||!FirstLoad)
    {
      if((pCSF->GetStr("Disks","Disk_A_Name","")).NotEmpty())
      {
#if defined(SSE_GUI_CONFIG)
        // if relative path, add to home folder (so if no home folder
        // in config, relative to ours)
        EasyStr path=pCSF->GetStr("Disks","Disk_A_Path","");
        if(path.Text[1]!=':') // Windows
          path=HomeFol+SLASH+path;
        InsertDisk(DRIVE_A,pCSF->GetStr("Disks","Disk_A_Name",""),path,false,
          false,pCSF->GetStr("Disks","Disk_A_DiskInZip",""),true,false);
#else
        InsertDisk(DRIVE_A,pCSF->GetStr("Disks","Disk_A_Name",""),
          pCSF->GetStr("Disks","Disk_A_Path",""),false,false,
          pCSF->GetStr("Disks","Disk_A_DiskInZip",""),true,false);
#endif
        FloppyDisk[DRIVE_A].WriteProtect=pCSF->GetBool("Disks","Disk_A_WP",
          FloppyDisk[DRIVE_A].WriteProtect);
      }
    }
    if(BootDisk[DRIVE_B].Empty()||!FirstLoad)
    {
      if((pCSF->GetStr("Disks","Disk_B_Name","")).NotEmpty())
      {
#if defined(SSE_GUI_CONFIG)
        EasyStr path=pCSF->GetStr("Disks","Disk_B_Path","");
        if(path.Text[1]!=':')
          path=HomeFol+SLASH+path;
        InsertDisk(DRIVE_B,pCSF->GetStr("Disks","Disk_B_Name",""),path,false,
          false,pCSF->GetStr("Disks","Disk_B_DiskInZip",""),true,false);
#else
        InsertDisk(DRIVE_B,pCSF->GetStr("Disks","Disk_B_Name",""),
          pCSF->GetStr("Disks","Disk_B_Path",""),false,false,
          pCSF->GetStr("Disks","Disk_B_DiskInZip",""),true);
#endif
        FloppyDisk[DRIVE_B].WriteProtect=pCSF->GetBool("Disks","Disk_B_WP",
          FloppyDisk[DRIVE_B].WriteProtect);
      }
    }
    AutoInsert2=pCSF->GetByte("Disks","AutoInsert2",AutoInsert2);
    UPDATE;
  }
#if defined(SSE_ACSI)
  AcsiHardDiskMan.LoadData(FirstLoad,pCSF,SecDisabled);
#endif
}

#if defined(SSE_GUI_CONFIG)

// little helper for TDiskManager::SaveData()
// low-level, could be more generic
void write_rel_disk_path(TDiskManager *diskman,int d,TConfigStoreFile *pCSF,EasyStr &Path) {
  EasyStr path=Path;
#if defined(SSE_420R5)
  int l1=path.Length();
  int l2=diskman->HomeFol.Length();
  int i=0;
#else
  INT_PTR l1=path.Length();
  INT_PTR l2=diskman->HomeFol.Length();
  INT_PTR i=0;
#endif
  for(i=0;i<l1 && i<l2 && path.Text[i]==diskman->HomeFol.Text[i];i++)
  {}
  if(i==l2)
    path=path.Mids(i+1,l1-i); // +1 = after slash
  pCSF->SetStr("Disks",(char*)((d==DRIVE_A)?"Disk_A_Path":"Disk_B_Path"),path);
}

#endif

void TDiskManager::SaveData(bool FinalSave,TConfigStoreFile *pCSF) {
#if USE_PASTI
  if(hPasti) 
  {
    pCSF->DeleteSection("Pasti");
    char Buf[8192],*p=Buf;
    ZeroMemory(Buf,sizeof(Buf));
    pastiLOADINI pli;
    pli.mode=PASTI_LCSTRINGS;
    pli.name=NULL;
    pli.buffer=Buf;
    pli.bufSize=8192;
    pasti->SaveConfig(&pli,NULL);
    while(p[0]) {
      char *val=p+strlen(p)+1;
      pCSF->SetStr("Pasti",p,val);
      p=val+strlen(val)+1;
    }
  }
#endif
  SavePosition(FinalSave,pCSF);
  pCSF->SetStr("Disks","Width",EasyStr(Width));
  pCSF->SetStr("Disks","Height",EasyStr(Height));
  pCSF->SetStr("Disks","Maximized",LPSTR(Maximized? "1" : "0"));
  pCSF->SetStr("Disks","FSWidth",EasyStr(FSWidth));
  pCSF->SetStr("Disks","FSHeight",EasyStr(FSHeight));
  pCSF->SetStr("Disks","FSMaximized",LPSTR(FSMaximized? "1" : "0"));
  pCSF->SetStr("Disks","CurrentFolder",DisksFol);
#if defined(SSE_GUI_CONFIG)
  if(FinalSave) // not for configs
#endif
    pCSF->SetStr("Disks","HomeFolder",HomeFol);
  EasyStr Path[2];
  Path[DRIVE_A]=FloppyDrive[DRIVE_A].GetDisk();
  Path[DRIVE_B]=FloppyDrive[DRIVE_B].GetDisk();
  if(EjectDisksWhenQuit && FinalSave) 
    Path[DRIVE_A]=Path[DRIVE_B]="";
#if defined(SSE_GUI_CONFIG)
  if(!FinalSave) // if FinalSave, stay compatible with older versions
    write_rel_disk_path(this,DRIVE_A,pCSF,Path[DRIVE_A]);
  else
#endif
    pCSF->SetStr("Disks","Disk_A_Path",Path[DRIVE_A]);
  if(Path[DRIVE_A]!="")
    pCSF->SetStr("Disks","Disk_A_WP",EasyStr(FloppyDisk[DRIVE_A].WriteProtect));
  if(Path[DRIVE_B]!="")
    pCSF->SetStr("Disks","Disk_B_WP",EasyStr(FloppyDisk[DRIVE_B].WriteProtect));
  pCSF->SetStr("Disks","Disk_A_Name",FloppyDisk[DRIVE_A].DiskName);
  pCSF->SetStr("Disks","Disk_A_DiskInZip",FloppyDisk[DRIVE_A].DiskInZip);
#if defined(SSE_GUI_CONFIG)
  if(!FinalSave) // if FinalSave, stay compatible with older versions
    write_rel_disk_path(this,DRIVE_B,pCSF,Path[DRIVE_B]);
  else
#endif
  pCSF->SetStr("Disks","Disk_B_Path",Path[DRIVE_B]);
  pCSF->SetStr("Disks","Disk_B_Name",FloppyDisk[DRIVE_B].DiskName);
  pCSF->SetStr("Disks","Disk_B_DiskInZip",FloppyDisk[DRIVE_B].DiskInZip);
  pCSF->SetInt("Disks","AutoInsert2",AutoInsert2);
  if(FinalSave) 
  {
    // keep manager info for final snapshot save or pasti state not saved
    // see no better way!
    TImageType save_type[2];
    for(int d=DRIVE_A;d<=DRIVE_B;d++)
    {
      save_type[d]=FloppyDrive[d].ImageType;
      FloppyDrive[d].RemoveDisk();
      FloppyDrive[d].ImageType=save_type[d];
    }
  }
  for(int n=0;n<DM_QUICKFOL_LEN;n++)
    pCSF->SetStr("Disks",EasyStr("QuickFol")+n,QuickFol[n]);
  for(int d=DRIVE_A;d<=DRIVE_B;d++)
  {
    for(int n=0;n<DM_HISTORY_LEN;n++) 
    {
      pCSF->SetStr("Disks",EasyStr("InsertHistoryName")+d+n,InsertHist[d][n].Name);
      pCSF->SetStr("Disks",EasyStr("InsertHistoryPath")+d+n,InsertHist[d][n].Path);
      pCSF->SetStr("Disks",EasyStr("InsertHistoryDiskInZip")+d+n,InsertHist[d][n].DiskInZip);
    }
  }
#ifdef WIN32
  pCSF->SetStr("Disks","ExplorerFolders",(ExplorerFolders) ? "1" : "0");
#ifndef SSE_NO_WINSTON_IMPORT
  pCSF->SetStr("Disks","WinSTonPath",EasyStr(WinSTonPath));
  pCSF->SetStr("Disks","WinSTonDiskPath",EasyStr(WinSTonDiskPath));
  pCSF->SetStr("Disks","ImportPath",EasyStr(ImportPath));
  pCSF->SetStr("Disks","ImportOnlyIfExist",LPSTR(ImportOnlyIfExist? "1" : "0"));
  pCSF->SetStr("Disks","ImportConflictAction",EasyStr(ImportConflictAction));
#endif
  pCSF->SetStr("Disks","MSAConvPath",MSAConvPath);
#if defined(SSE_GUI_DM_BIGICONS)
  pCSF->SetInt("Disks","IconSize",IconSize);
#else
  pCSF->SetStr("Disks","SmallIcons",(SmallIcons) ? "1" : "0");
#endif
  pCSF->SetInt("Disks","IconSpacing",IconSpacing);
#ifdef SSE_421R0
  pCSF->SetInt("Disks","SortBy",SortBy);
#endif
#endif//WIN32
#if USE_PASTI
  pCSF->SetInt("Disks","PastiActive",pasti_active);
#endif
  pCSF->SetStr("Disks","HideBroken",(LPSTR)((HideBroken) ? "1" : "0"));
  pCSF->SetStr("Disks","ShowHiddenFiles",(LPSTR)((ShowHiddenFiles) ? "1" : "0"));
  pCSF->SetStr("Disks","HideExtension",(LPSTR)((HideExtension) ? "1" : "0"));
  pCSF->SetStr("Disks","EjectDisksWhenQuit",(LPSTR)((EjectDisksWhenQuit) ? "1" : "0"));
  pCSF->SetStr("Disks","DoubleClickAction",Str(DoubleClickAction));
  pCSF->SetInt("Disks","CloseAfterIRR",CloseAfterIRR);
  pCSF->SetInt("Disks","NumFloppyDrives",nFloppyDrives);
#if defined(SSE_DRIVE_SINGLESIDE)
  pCSF->SetInt("Disks","SingleSidedA",FloppyDrive[DRIVE_A].bSingleSided);
  pCSF->SetInt("Disks","SingleSidedB",FloppyDrive[DRIVE_B].bSingleSided);
#endif
  pCSF->SetInt("Disks","QuickDiskAccess",bTurboDrive);
  pCSF->SetInt("Disks","FloppyArchiveIsReadWrite",bArchiveRW);
  pCSF->SetInt("Disks","Background",mBackground);
#if defined(SSE_DISK_SWAPPER)
  pCSF->SetInt("Disks","SwapperPattern",bSwapperPattern);
#endif
  pCSF->SetInt("Disks","DiskAccessFF",floppy_access_ff);
#if defined(SSE_DISK_GHOST)
  pCSF->SetStr("Disks","GhostDisk",EasyStr(OPTION_GHOST_DISK));
#endif
#if defined(SSE_DISK_AUTOSTW)
  pCSF->SetStr("Disks","AutoSTW",EasyStr(OPTION_AUTOSTW));
#endif
  pCSF->SetStr("Disks","CountDmaCycles",EasyStr(OPTION_COUNT_DMA_CYCLES));
#if defined(SSE_DRIVE_SOUND)
  pCSF->SetStr("Disks","DriveSound",EasyStr(OPTION_DRIVE_SOUND));
  pCSF->SetStr("Disks","DriveSoundVolume",EasyStr(FloppyDrive[DRIVE_A].SoundVolume));
  pCSF->SetStr("Disks","DriveSoundDirA",DriveSoundDir[DRIVE_A]);
  pCSF->SetStr("Disks","DriveSoundDirB",DriveSoundDir[DRIVE_B]);
#endif
#if defined(SSE_TOS_PRG_AUTORUN)
  pCSF->SetInt("Disks","PRG_support",OPTION_PRG_SUPPORT);
#endif
  pCSF->SetInt("Disks","DiskProtectImage",bDiskProtectImage);
#if defined(SSE_DISK_STX)
  pCSF->SetInt("Disks","DiskProtectImageStx",bDiskProtectImageStx);
#endif
  HardDiskMan.SaveData(FinalSave,pCSF);
#if defined(SSE_ACSI)
  AcsiHardDiskMan.SaveData(FinalSave,pCSF);
#endif
#if defined(SSE_DISK_M3U)
  pCSF->SetStr("Disks","SwapperM3uPath",DumbDiskSwapper.m3upath);
  pCSF->SetStr("Disks","SwapperImagePath",DumbDiskSwapper.image_path);
  pCSF->SetInt("Disks","SwapperImageIndex",DumbDiskSwapper.image_index);
  pCSF->SetInt("Disks","SwapperNumImages",DumbDiskSwapper.num_images);
#endif
}


void THardDiskManager::LoadData(bool FirstLoad,TConfigStoreFile *pCSF,bool *SecDisabled) {
  //SEC(PSEC_HARDDRIVES) 
  if(!SecDisabled[PSEC_HARDDRIVES])
  {
    if(nDrives==0||!FirstLoad)
    {  // In case of SteemIntro
      EasyStr Str;
      for(nDrives=0;nDrives<MAX_GEMDOS_HARDDRIVES;nDrives++) 
      {
        Str=pCSF->GetStr("HardDrives",EasyStr("Drive_")+nDrives+"_Path","NOT ASSIGNED");
        if(Str=="NOT ASSIGNED") 
          break;
        NO_SLASH(Str);
        HDrive[nDrives].Path=Str;
        Str=pCSF->GetStr("HardDrives",EasyStr("Drive_")+nDrives+"_Letter",EasyStr(char('C'+nDrives)));
        HDrive[nDrives].Letter=Str[0];
      }
    }
#ifndef DISABLE_STEMDOS 
    Stemdos.BootDrive=pCSF->GetByte("HardDrives","BootDrive",Stemdos.BootDrive);
#endif
    DisableHardDrives=pCSF->GetBool("HardDrives","DisableHardDrives",DisableHardDrives);
#ifdef WIN32
    SendMessage(GetDlgItem(DiskMan.Handle,IDC_HDGEMDOS),BM_SETCHECK,!HardDiskMan.DisableHardDrives,0);
#endif
    update_mount();
    UPDATE;
  }
}


void THardDiskManager::SaveData(bool FinalSave,TConfigStoreFile *pCSF) {
  SavePosition(FinalSave,pCSF);
  for(BYTE n=0;n<MAX_GEMDOS_HARDDRIVES;n++) 
  {
    if(n<nDrives) 
    {
      pCSF->SetStr("HardDrives",EasyStr("Drive_")+n+"_Letter",EasyStr(HDrive[n].Letter));
      pCSF->SetStr("HardDrives",EasyStr("Drive_")+n+"_Path",HDrive[n].Path);
    }
    else 
    {
      pCSF->SetStr("HardDrives",EasyStr("Drive_")+n+"_Letter","NOT ASSIGNED");
      pCSF->SetStr("HardDrives",EasyStr("Drive_")+n+"_Path","NOT ASSIGNED");
    }
  }
#ifndef DISABLE_STEMDOS
  pCSF->SetStr("HardDrives","BootDrive",EasyStr(Stemdos.BootDrive));
#endif
  pCSF->SetInt("HardDrives","DisableHardDrives",DisableHardDrives);
}


#if defined(SSE_ACSI)

void TAcsiHardDiskManager::LoadData(bool FirstLoad,TConfigStoreFile *pCSF,bool *SecDisabled) {
  //SEC(PSEC_HARDDRIVES)
  if(!SecDisabled[PSEC_HARDDRIVES])
  {
    if(nDrives==0||!FirstLoad)
    {  // In case of SteemIntro
      EasyStr Str;
      for(nDrives=0;nDrives<MAX_ACSI_DEVICES;nDrives++) 
      {
        Str=pCSF->GetStr("HardDrives",EasyStr("AcsiDrive_")+nDrives+"_Path","NOT ASSIGNED");
        if(Str=="NOT ASSIGNED")
          break;
        NO_SLASH(Str);
        HDrive[nDrives].Path=Str;
        Str=pCSF->GetStr("HardDrives",EasyStr("AcsiDrive_")+nDrives+"_Letter",EasyStr(char('C'+nDrives)));
        HDrive[nDrives].Letter=Str[0];
        if(AcsiHdc[nDrives].Init(nDrives,HDrive[nDrives].Path))
          SSEConfig.AcsiImg=true;
      }
    }
    SSEOptions.Acsi=pCSF->GetBool("HardDrives","Acsi",SSEOptions.Acsi);
#ifdef WIN32
    if(HWND h=GetDlgItem(DiskMan.Handle,IDC_HDACSI))
      SendMessage(h,BM_SETCHECK,SSEOptions.Acsi,0);
#endif    
    AcsiDir=pCSF->GetStr("HardDrives","AcsiDir",UsersPath+SLASH+"ACSI");
    UPDATE; // note since we use [HardDrives] for both, confusion ACSI/GEMDOS dialog box
  }
}


void TAcsiHardDiskManager::SaveData(bool FinalSave,TConfigStoreFile *pCSF) {
  SavePosition(FinalSave,pCSF);
  for(BYTE n=0;n<MAX_ACSI_DEVICES;n++)
  {
    if(n<nDrives)
    {
      pCSF->SetStr("HardDrives",EasyStr("AcsiDrive_")+n+"_Letter",EasyStr(HDrive[n].Letter));
      pCSF->SetStr("HardDrives",EasyStr("AcsiDrive_")+n+"_Path",HDrive[n].Path);
    }
    else
    {
      pCSF->SetStr("HardDrives",EasyStr("AcsiDrive_")+n+"_Letter","NOT ASSIGNED");
      pCSF->SetStr("HardDrives",EasyStr("AcsiDrive_")+n+"_Path","NOT ASSIGNED");
    }
  }
  pCSF->SetStr("HardDrives","AcsiDir",AcsiDir);
}

#endif


void TJoystickConfig::LoadData(bool FirstLoad,TConfigStoreFile *pCSF,bool *SecDisabled) {
#ifdef UNIX
  //SEC(PSEC_PCJOY) 
  if(!SecDisabled[PSEC_PCJOY])
  {
    // Have to load calibration of joysticks
    for(int j=0;j<MAX_PC_JOYS;j++) {
      EasyStr Sect=EasyStr("PCJoystick ")+(j+1);
      JoyInfo[j].On=bool(pCSF->GetInt(Sect,"On",JoyInfo[j].On));
      JoyInfo[j].DeviceFile=pCSF->GetStr(Sect,"DeviceFile",JoyInfo[j].DeviceFile);
      JoyInfo[j].NumButtons=pCSF->GetInt(Sect,"NumButtons",JoyInfo[j].NumButtons);
      for(int a=0;a<6;a++) {
        JoyInfo[j].AxisMin[a]=pCSF->GetInt(Sect,Str("Axis_")+a+"_Min",JoyInfo[j].AxisMin[a]);
        JoyInfo[j].AxisMax[a]=pCSF->GetInt(Sect,Str("Axis_")+a+"_Max",JoyInfo[j].AxisMax[a]);
        JoyInfo[j].AxisLen[a]=(JoyInfo[j].AxisMax[a]-JoyInfo[j].AxisMin[a]);
        JoyInfo[j].AxisMid[a]=JoyInfo[j].AxisMin[a]+JoyInfo[j].AxisLen[a]/2;
        JoyInfo[j].AxisExists[a]=pCSF->GetInt(Sect,Str("Axis_")+a+"_Exists",JoyInfo[j].AxisExists[a]);
        JoyInfo[j].AxisDZ[a]=pCSF->GetInt(Sect,Str("Axis_")+a+"_DZ",JoyInfo[j].AxisDZ[a]);
        JoyInfo[j].Range=pCSF->GetInt(Sect,Str("Axis_")+a+"_Range",JoyInfo[j].Range);
      }
      JoyPosReset(j); // Sets all to mid
    }
    PCJoyEdit=pCSF->GetInt("Joysticks","PCJoyEdit",PCJoyEdit);
  }
#endif
  //SEC(PSEC_JOY) 
  if(!SecDisabled[PSEC_JOY])
  {
#ifdef WIN32
    BYTE Method=pCSF->GetByte("Joysticks","JoyReadMethod",JoyReadMethod);
#if defined(SSE_NO_JOYSTICK_MM)
    if(Method==PCJOY_READ_WINMM)
      Method=PCJOY_READ_DI;
#endif
    if(FirstLoad && DisablePCJoysticks) 
      Method=PCJOY_READ_DONT;
    if(FirstLoad||Method!=JoyReadMethod) 
      InitJoysticks(Method);
#endif//WIN32
#ifdef UNIX
    if(FirstLoad)
      InitJoysticks((int)(DisablePCJoysticks?PCJOY_READ_DONT:PCJOY_READ_KERNELDRIVER));
#endif
    // Default ST joysticks
    int DefJoy[MAX_PC_JOYS]={2,2,2,2,2,2,2,2}; // 2=use cursor keys
#ifndef UNIX
    if(NumJoysticks) 
    {
      DefJoy[1]=0; // Use joystick 1 for stick 1
      JoySetup[0][1].ToggleKey=ACTIVE_ALWAYS;
      JoySetup[0][0].ToggleKey=ACTIVE_NEVER;
#ifndef ONEGAME
      if(NumJoysticks>1) 
      {
        JoySetup[0][0].ToggleKey=ACTIVE_ALWAYS;
        DefJoy[0]=1; // Use joystick 2
      }
      else 
        JoySetup[0][0].ToggleKey=VK_SCROLL;
#endif
    }
    else
#endif
    {
      // this can confuse player, only makes sense on QWERTY anyway, player
      // can define their keys if they want to play
      //DefJoy[0]=3; // 3=use A, W, S, Z and shift
#ifdef ONEGAME
      JoySetup[0][1].ToggleKey=1;
      JoySetup[0][0].ToggleKey=0;
#else
#ifdef WIN32
      JoySetup[0][1].ToggleKey=VK_SCROLL; //?
      JoySetup[0][0].ToggleKey=VK_SCROLL;
#else
      JoySetup[0][1].ToggleKey=ACTIVE_NEVER;
      JoySetup[0][0].ToggleKey=ACTIVE_NEVER;
#endif
#endif
    }
    for(int n=0;n<MAX_PC_JOYS;n++) 
      SetJoyToDefaults(n,DefJoy[n]);
#ifdef UNIX
    ConfigST=(bool)pCSF->GetInt("Joysticks","ConfigST",ConfigST);
#endif
    for(int Setup=0;Setup<JOYSTICK_SETUPS;Setup++) 
    {
      for(int n=0;n<MAX_PC_JOYS;n++) 
      {
        EasyStr Sect=EasyStr("Joystick ")+(n+1);
        EasyStr Prefix;
        if(Setup) 
          Prefix=Str(Setup)+"_";
        JoySetup[Setup][n].Type=pCSF->GetInt(Sect,Prefix+"Type",JoySetup[Setup][n].Type);
        JoySetup[Setup][n].ToggleKey=pCSF->GetInt(Sect,Prefix+"ToggleKey",JoySetup[Setup][n].ToggleKey);
        JoySetup[Setup][n].AnyFireOnJoy=pCSF->GetInt(Sect,Prefix+"AnyFireOnJoy",
                                                     JoySetup[Setup][n].AnyFireOnJoy);
        JoySetup[Setup][n].DeadZone=pCSF->GetInt(Sect,Prefix+"DeadZone",JoySetup[Setup][n].DeadZone);
        JoySetup[Setup][n].AutoFireSpeed=pCSF->GetInt(Sect,Prefix+"AutoFireSpeed",
                                                      JoySetup[Setup][n].AutoFireSpeed);
        for(int i=0;i<JOY_N_DIR_ID;i++)
          JoySetup[Setup][n].DirID[i]=pCSF->GetInt(Sect,Prefix+"DirID"+i,JoySetup[Setup][n].DirID[i]);
        if(n==2||n==4) 
        {
          for(int i=0;i<JAGPAD_N_DIR;i++)
            JoySetup[Setup][n].JagDirID[i]=pCSF->GetInt(Sect,Prefix+"JagDirID"+i,
                                                        JoySetup[Setup][n].JagDirID[i]);
        }
      }
    }
    nJoySetup=pCSF->GetByte("Joysticks","Setup",nJoySetup);
    JoyConfig.JoySetupUpdate(false);
    CreateJoyAnyButtonMasks();
    BasePort=pCSF->GetByte("Joysticks","BasePort",BasePort);
    mouse_speed=pCSF->GetByte("Joysticks","MouseSpeed",mouse_speed);
    UPDATE;
  }
}


void TJoystickConfig::SaveData(bool FinalSave,TConfigStoreFile *pCSF) {
  SavePosition(FinalSave,pCSF);
  JoyConfig.JoySetupUpdate(true);
#ifdef WIN32    
  pCSF->SetInt("Joysticks","JoyReadMethod",JoyReadMethod);
#endif
#ifdef UNIX
  for(int j=0;j<MAX_PC_JOYS;j++)
  {
    EasyStr Sect=EasyStr("PCJoystick ")+(j+1);
    pCSF->SetInt(Sect,"On",JoyInfo[j].On);
    pCSF->SetStr(Sect,"DeviceFile",JoyInfo[j].DeviceFile);
    pCSF->SetInt(Sect,"NumButtons",JoyInfo[j].NumButtons);
    for(int a=0;a<6;a++) {
      pCSF->SetInt(Sect,Str("Axis_")+a+"_Min",JoyInfo[j].AxisMin[a]);
      pCSF->SetInt(Sect,Str("Axis_")+a+"_Max",JoyInfo[j].AxisMax[a]);
      pCSF->SetInt(Sect,Str("Axis_")+a+"_Exists",JoyInfo[j].AxisExists[a]);
      pCSF->SetInt(Sect,Str("Axis_")+a+"_DZ",JoyInfo[j].AxisDZ[a]);
      pCSF->SetInt(Sect,Str("Axis_")+a+"_Range",JoyInfo[j].Range);
    }
  }
  pCSF->SetInt("Joysticks","ConfigST",ConfigST);
  pCSF->SetInt("Joysticks","PCJoyEdit",PCJoyEdit);
#endif
  for(int Setup=0;Setup<JOYSTICK_SETUPS;Setup++) 
  {
    for(int n=0;n<MAX_PC_JOYS;n++)
    {
      EasyStr Sect="Joystick ";
      Sect+=(n+1);
      EasyStr Prefix;
      if(Setup) 
        Prefix=Str(Setup)+"_";
      pCSF->SetStr(Sect,Prefix+"ToggleKey",EasyStr(JoySetup[Setup][n].ToggleKey));
      pCSF->SetStr(Sect,Prefix+"AnyFireOnJoy",EasyStr(JoySetup[Setup][n].AnyFireOnJoy));
      pCSF->SetStr(Sect,Prefix+"DeadZone",EasyStr(JoySetup[Setup][n].DeadZone));
      pCSF->SetStr(Sect,Prefix+"AutoFireSpeed",EasyStr(JoySetup[Setup][n].AutoFireSpeed));
      pCSF->SetStr(Sect,Prefix+"Type",EasyStr(JoySetup[Setup][n].Type));
      for(int i=0;i<JOY_N_DIR_ID;i++)
        pCSF->SetStr(Sect,Prefix+"DirID"+i,EasyStr(JoySetup[Setup][n].DirID[i]));
      if(n==2||n==4) 
      {
        for(int i=0;i<JAGPAD_N_DIR;i++) 
          pCSF->SetStr(Sect,Prefix+"JagDirID"+i,EasyStr(JoySetup[Setup][n].JagDirID[i]));
      }
    }
  }
  pCSF->SetInt("Joysticks","Setup",nJoySetup);
  pCSF->SetInt("Joysticks","BasePort",BasePort);
  pCSF->SetStr(Section,"MouseSpeed",EasyStr(mouse_speed));
}


// helper function
int ConvertBitness(TConfigStoreFile *pCSF,char *Key,int DefVal) { // little helper
  int Bitness=pCSF->GetInt("ControlPanel","Bitness",0); // if absent, do nothing
  int original=pCSF->GetInt("ControlPanel",Key,DefVal);
  int converted=original;
  if(Bitness==32 && SSE_BITNESS==64)
    converted<<=2;
  else if(Bitness==64 && SSE_BITNESS==32)
    converted>>=2;
  return converted;
}


void TOptionBox::LoadData(bool FirstLoad,TConfigStoreFile *pCSF,bool *SecDisabled) {
  TRACE_LOG("Retrieving options\n");
  //SEC(PSEC_MACHINETOS) 
  if(!SecDisabled[PSEC_MACHINETOS])
  {
    nSysCyclesPerSecond/=TICKS8;
    nSysCyclesPerSecond=pCSF->GetInt("Options","CPUBoost",nSysCyclesPerSecond);
    nSysCyclesPerSecond=MIN(nSysCyclesPerSecond,(DWORD)CPU_MAX_HERTZ);
    nSysCyclesPerSecond*=TICKS8;
    nSysCyclesPerSecond=MAX(nSysCyclesPerSecond,CpuNormalHz);
    AdaptCpuBoost();
    ShowTips=pCSF->GetBool("Options","ShowToolTips",ShowTips);
    TOSBrowseDir=pCSF->GetStr("Machine","ROM_Add_Dir",(FirstLoad) ? RunDir : TOSBrowseDir);
    Str LastCartFol=pCSF->GetStr("Machine","Cart_Dir","");
    if(LastCartFol.NotEmpty()) 
    {
      LastCartFile=LastCartFol+SLASH;
      pCSF->SetStr("Machine","Cart_Dir","");
    }
    LastCartFile=pCSF->GetStr("Machine","LastCartFile",LastCartFile);
    if(LastCartFile.Empty()) 
      LastCartFile=RunDir+SLASH;
    OPTION_CARTRIDGE_OFF=pCSF->GetBool("Machine","CartidgeOff",OPTION_CARTRIDGE_OFF);
    if(OPTION_CARTRIDGE_OFF)
    {
      cart_save=cart; //update
      cart=NULL;
    }
    NewMemConf0=pCSF->GetInt("Options","NewMemConf0",NewMemConf0);
    NewMemConf1=pCSF->GetInt("Options","NewMemConf1",NewMemConf1);
    NewMonitorSel=pCSF->GetInt("Options","NewMonitorSel",NewMonitorSel);
    NewROMFile=pCSF->GetStr("Options","NewROMFile",NewROMFile);
#if defined(SSE_GUI_INSTANTCHANGE)
    NewStModel=pCSF->GetInt("Options","NewStModel",NewStModel);
#endif
    eslTOS_Descend=pCSF->GetBool("Options","TOSSortDescend",eslTOS_Descend);
    eslTOS_Sort=(ESLSortEnum)pCSF->GetInt("Options","TOSSort",(int)eslTOS_Sort);

#if defined(SSE_GUI_QUICKSETTINGS)
    for(int i=0;i<TOS_LIST_LEN;i++)
    {
      TosList.Version[i]=pCSF->GetWord("Options",Str("TosListVer")+i,TosList.Version[i]);
      TosList.Path[i]=pCSF->GetStr("Options",Str("TosListFile")+i,TosList.Path[i]);
    }
#endif

    bEnableShiftSwitching=pCSF->GetBool("Machine","ShiftSwitching",0);
    if(KeyboardLangID==0) 
    {
#ifdef WIN32
      KeyboardLangID=GetUserDefaultLangID();
#endif
#ifdef UNIX
      KeyboardLangID=MAKELONG(LANG_ENGLISH,SUBLANG_ENGLISH_UK);
#endif
    }
    KeyboardLangID=(LANGID)pCSF->GetInt("Machine","KeyboardLanguage",KeyboardLangID);
#if defined(SSE_IKBD_MAPPINGFILE)
    KeyboardMappingPath=pCSF->GetStr("Machine","KeyboardMappingPath",KeyboardMappingPath);
#endif
    InitKeyTable();
#if !defined(SSE_LIBRETRONUKE)
    if(FirstLoad) 
      CheckResetIcon();
#endif
    BYTE st_model=pCSF->GetByte("Machine","STType",ST_MODEL);
    SSEConfig.SwitchSTModel(st_model);
    PreciseModel=pCSF->GetStr("Machine","PreciseModel","");
    Mfp.xtal=pCSF->GetInt("Machine","Mfp_xtal",Mfp.xtal);
    CpuCustomHz/=TICKS8;
    CpuNormalHz=CpuCustomHz=pCSF->GetInt("Machine","CpuCustomHz",CpuCustomHz)*TICKS8;
    if(!SSEConfig.CpuBoosted)
      nSysCyclesPerSecond=CpuNormalHz;
#if defined(SSE_HD6301_LL) && !defined(SSE_NO_OLD_IKBD)
    if(HD6301_OK)
      OPTION_C1=pCSF->GetBool("Options","Chipset1",OPTION_C1);
#endif
#if defined(SSE_IKBD_RTC)
    OPTION_BATTERY6301=pCSF->GetByte("Options","Battery6301",OPTION_BATTERY6301);
#endif
    OPTION_SPURIOUS=pCSF->GetBool("Options","Spurious",OPTION_SPURIOUS);
#if defined(SSE_ACSI_LASER)
    OPTION_LASER=pCSF->GetBool("Machine","Laser",OPTION_LASER);
#endif
#if defined(SSE_PRINTER)
    OPTION_PRINTER=pCSF->GetBool("Machine","Printer",OPTION_PRINTER);
#endif
  }
  //SEC(PSEC_STVIDEO)
#if defined(SSE_GUI_421_REORDER)
  if(!SecDisabled[PSEC_DISPFULL])
#else
  if(!SecDisabled[PSEC_STVIDEO])
#endif
  {
    //if(FirstLoad) 
    {
      bool ColourMonitor=pCSF->GetBool("Machine","Colour_Monitor",1);
      SSEConfig.UpdateMonitor(ColourMonitor);
#ifndef NO_CRAZY_MONITOR
      extended_monitor=pCSF->GetBool("Machine","ExMon",!!extended_monitor);//don't want 2
      em_width=pCSF->GetInt("Machine","ExMonWidth",em_width);
      em_height=pCSF->GetInt("Machine","ExMonHeight",em_height);
      em_planes=pCSF->GetByte("Machine","ExMonPlanes",em_planes);
      if(extended_monitor&&em_width&&em_height&&em_planes>0&&em_planes<5)
        Disp.ScreenChange();
      else
        extended_monitor=0;
#endif
    }
    OPTION_WS=pCSF->GetByte("Machine","Wakeup",OPTION_WS);
    OPTION_WS=((OPTION_WS-1)&3)+1; // make valid
    OPTION_SHIFTER_WU=pCSF->GetByte("Machine","ShifterWU",OPTION_SHIFTER_WU);
    OPTION_RANDOM_WU=pCSF->GetBool("Machine","RandomWakeup",OPTION_RANDOM_WU);
    OPTION_BLITTER_WU=pCSF->GetByte("Machine","BlitterWakeup",4);
    OPTION_HWOVERSCAN=pCSF->GetByte("Options","HwOverscan",OPTION_HWOVERSCAN);
    OPTION_VLE=pCSF->GetByte("Options","VideoLogicEmu",OPTION_VLE);
    if(!SSEConfig.Stvl &&OPTION_VLE==2)
      OPTION_VLE=1;
    OPTION_UNSTABLE_SHIFTER=pCSF->GetBool("Machine","UnstableShifter",OPTION_UNSTABLE_SHIFTER);
#if defined(SSE_OPTION_FASTBLITTER)
    OPTION_FASTBLITTER=pCSF->GetBool("Machine","FastBlitter",OPTION_FASTBLITTER);
#endif
    OPTION_SCANLINES=pCSF->GetBool("Display","Scanlines",OPTION_SCANLINES);
    OPTION_ST_ASPECT_RATIO=pCSF->GetBool("Display","STAspectRatio",OPTION_ST_ASPECT_RATIO);
    // monochrome - ini-only option
    SSEOptions.MonochromeDisableBorder=pCSF->GetBool("Main","MonochromeDisableBorder",false);
    OPTION_GREYSCREEN=pCSF->GetBool("Display","GreyScreen",OPTION_GREYSCREEN);
#if defined(SSE_VID_D3D_SWEETFX)
    OPTION_CRT_EMU=pCSF->GetBool("Display","CrtEmu",OPTION_CRT_EMU);
#endif
  }
  //SEC(PSEC_GENERAL) 
  if(!SecDisabled[PSEC_GENERAL])
  {
#ifdef WIN32
    bAllowTaskSwitch=pCSF->GetBool("Options","AllowTaskSwitch",bAllowTaskSwitch);
#endif
    bPauseWhenInactive=pCSF->GetBool("Options","PauseWhenInactive",bPauseWhenInactive);
    slow_motion_speed=pCSF->GetInt("Options","SlowMotionSpeed",slow_motion_speed);
    fast_forward_max_speed=pCSF->GetInt("Options","MaxFastForward",fast_forward_max_speed);
    HighPriority=pCSF->GetBool("Options","HighPriority",HighPriority);
    OPTION_EMUTHREAD=pCSF->GetBool("Options","EmuThread",OPTION_EMUTHREAD);
    run_speed_ticks_per_second=pCSF->GetInt("Options","RunSpeed",run_speed_ticks_per_second);
    StartEmuOnClick=pCSF->GetBool("Options","StartOnClick",StartEmuOnClick);
    OPTION_HACKS=pCSF->GetByte("Options","SpecificHacks",OPTION_HACKS);
    OPTION_CAPTURE_MOUSE=pCSF->GetByte("Options","CaptureMouse",OPTION_CAPTURE_MOUSE);
    OPTION_RTC_HACK=pCSF->GetBool("Options","RtcHack",OPTION_RTC_HACK);
    OPTION_EMU_DETECT=pCSF->GetBool("Options","EmuDetect",OPTION_EMU_DETECT);
#if defined(SSE_GUI_DEFAULT_ST_CONFIG)
    OPTION_ST_PRESELECT=pCSF->GetBool("Options","StPreselect",OPTION_ST_PRESELECT);
#endif
#if !defined(SSE_GUI_421_REORDER)
    OPTION_ADVANCED=pCSF->GetBool("Main","AdvancedSettings",OPTION_ADVANCED);
#endif
#if !defined(SSE_420R5) // buggy "restore"
    if(!OPTION_ADVANCED)
      SSEOptions.Restore();
#endif
    SSEOptions.PauseRun=pCSF->GetBool("Main","ToggleF12",SSEOptions.PauseRun); // old option name
    SSEOptions.F12Run=pCSF->GetBool("Main","F12Run",SSEOptions.F12Run);
#if defined(SSE_GUI_OPTION_FOR_TESTS)
    SSEOptions.TestingNewFeatures=pCSF->GetInt("Options","TestingNewFeatures",
      SSEOptions.TestingNewFeatures);
#endif
    OPTION_VMMOUSE=pCSF->GetBool("Options","VMMouse",OPTION_VMMOUSE);
#if defined(SSE_GUI_INSTANTCHANGE)
    SSEOptions.InstantMachineChange=pCSF->GetBool("Options","InstantMachineChange",
                                                  SSEOptions.InstantMachineChange);
#endif
#if defined(SSE_GUI_DRAG_PAGES)
    for(int k=0;k<MAX_OPTION_LABELS;k++)
    {
      int LoadedIconNum=pCSF->GetInt("Options",Str("OptionLabel")+k,OptionLabel[k].IconNum);
      if(LoadedIconNum>-1) // associate original caption
        for(int l=0;l<MAX_OPTION_LABELS;l++)
          if(StartOptionLabel[l].IconNum==LoadedIconNum)
          {
            OptionLabel[k].IconNum=LoadedIconNum;
            OptionLabel[k].Caption=StartOptionLabel[l].Caption;
            break;
          }
    }
#endif
#if defined(SSE_DEBUGGER)
    TRACE_FILE_REWIND=pCSF->GetBool("Debug","TraceFileRewind",TRACE_FILE_REWIND);
    CheckMenuItem(sse_menu,1518,MF_BYCOMMAND|MF_CHECK(TRACE_FILE_REWIND));
#if defined(SSE_DEBUGGER_MONITOR_VALUE)
    Debug.MonitorValueSpecified=pCSF->GetByte("Debug","MonitorValueSpecified",Debug.MonitorValueSpecified);
    CheckMenuItem(sse_menu,1522,MF_BYCOMMAND|MF_CHECK(Debug.MonitorValueSpecified));
#endif
#if defined(SSE_DEBUGGER_MONITOR_RANGE)
    Debug.MonitorRange=pCSF->GetByte("Debug","MonitorRange",Debug.MonitorRange);
    CheckMenuItem(sse_menu,1523,MF_BYCOMMAND|MF_CHECK(Debug.MonitorRange));
#endif
#if defined(SSE_DEBUGGER_FAKE_IO)
    for(int i=0;i<FAKE_IO_LENGTH/2;i++)
    {
      char buffer[15];
      sprintf(buffer,"ControlMask%d",i);
      Debug.ControlMask[i]=(WORD)pCSF->GetInt("Debug",buffer,Debug.ControlMask[i]);
      if((DEBUGGER_CONTROL_MASK2&DEBUGGER_CONTROL_NEXT_PRG_RUN))
      {
        stop_on_next_program_run=1;
        CheckMenuItem(boiler_op_menu,1513,MF_BYCOMMAND|MF_CHECKED); //update
      }
    }
#endif
#if defined(SSE_DEBUGGER_TOGGLE)
    BOOL newDebuggerVisible=pCSF->GetInt("Debug","DebuggerVisible",DebuggerVisible);
    if(DebuggerVisible!=newDebuggerVisible)
      DebuggerToggle(newDebuggerVisible);
#endif
#if defined(SSE_DEBUGGER_STATUS_BAR)
    Debug.DialogOnStopEvent=pCSF->GetByte("Debug","DialogOnStopEvent",Debug.DialogOnStopEvent);
    CheckMenuItem(sse_menu,1535,MF_BYCOMMAND|MF_CHECK(Debug.DialogOnStopEvent));
#endif
#endif//#if defined(SSE_DEBUGGER)
  }
  //SEC(PSEC_STARTUP) 
  if(!SecDisabled[PSEC_STARTUP])
  {
    AutoLoadSnapShot=pCSF->GetBool("Options","AutoLoadSnapShot",AutoLoadSnapShot);
#ifdef SSE_X64
    AutoSnapShotName=pCSF->GetStr("Options","AutoSnapShotName64",AutoSnapShotName);
#else
    AutoSnapShotName=pCSF->GetStr("Options","AutoSnapShotName32",AutoSnapShotName);
#endif
    DefaultSnapshotFile=pCSF->GetStr("Main","DefaultSnapshot","");
/*  Request
    For instance, in the [Main] part of steem.ini you add the line
    WindowTitle=Steem SSE teh beST
    and this will be the title of the window
    limited to WINDOW_TITLE_MAX_CHARS (100)
*/
    EasyStr tmp=pCSF->GetStr("Main","WindowTitle",WINDOW_TITLE);
    strncpy(stem_window_title,tmp.Text,WINDOW_TITLE_MAX_CHARS);
#ifdef WIN32     
    SetWindowText(StemWin,stem_window_title);
#endif
  }
  //SEC(PSEC_DISPFULL) 
  if(!SecDisabled[PSEC_DISPFULL])
  {
    OPTION_WIN_VSYNC=pCSF->GetBool("Display","WinVSync",OPTION_WIN_VSYNC);
#if defined(SSE_VID_D3D_VSYNC)
    OPTION_AUTOVSYNC=pCSF->GetBool("Display","AutoVSync",OPTION_AUTOVSYNC);
    OPTION_AUTOVSYNC_FS=pCSF->GetBool("Display","AutoVSyncFS",OPTION_AUTOVSYNC_FS);
#endif
#if defined(SSE_VID_OLDSYNC)
    SSEOptions.OldSync=pCSF->GetBool("Display","OldSync",SSEOptions.OldSync);
#endif
    OPTION_TIMINGLOOP=pCSF->GetByte("Display","TimingLoop",OPTION_TIMINGLOOP); //BYTE
#if defined(SSE_TIMINGS_US)
    OPTION_MICROSECONDS=pCSF->GetBool("Display","Microseconds",OPTION_MICROSECONDS);
#endif
#if defined(SSE_VID_BFI)
    OPTION_BFI=pCSF->GetBool("Display","BlackFrameInsertion",OPTION_BFI);
#endif
    OPTION_3BUFFER_FS=pCSF->GetBool("Display","TripleBufferFS",OPTION_3BUFFER_FS);
#if defined(SSE_VID_DD)
    OPTION_3BUFFER_WIN=pCSF->GetBool("Display","TripleBufferWin",OPTION_3BUFFER_WIN);
#endif
    //OPTION_BLOCK_RESIZE=pCSF->GetByte("Display","BlockResize",OPTION_BLOCK_RESIZE);
    OPTION_LOCK_AR=pCSF->GetBool("Display","LockAspectRatio",OPTION_LOCK_AR);
    update_winsize();
#if defined(SSE_VID_D3D)
    Disp.D3DMode=pCSF->GetInt("Display","D3DMode",Disp.D3DMode);
#if defined(SSE_VID_2SCREENS)
    Disp.oldD3DMode=pCSF->GetInt("Display","oldD3DMode",Disp.oldD3DMode);
#endif
    Disp.D3DUpdateWH(Disp.D3DMode); // function returns if no pD3D
  //TRACE_LOG("Options D3D mode = %d %dx%d\n",Disp.D3DMode,Disp.D3DFsW,Disp.D3DFsH);
#endif
    OPTION_FULLSCREEN_AR=pCSF->GetByte("Display","FullscreenAR",OPTION_FULLSCREEN_AR);
    OPTION_FULLSCREEN_GUI=pCSF->GetBool("Display","FullScreenGUI",OPTION_FULLSCREEN_GUI);
    SSEConfig.TrueFullScreenGui=pCSF->GetBool("Display","TrueFullScreenGui",false);
#if defined(SSE_VID_DD)
    draw_fs_blit_mode=pCSF->GetByte("Options","DrawFSMode",draw_fs_blit_mode);
#endif
    draw_stretch=pCSF->GetByte("Display","DrawStretch",draw_stretch);
    draw_stretch_fs=pCSF->GetByte("Display","DrawStretchFs",draw_stretch_fs);
    frameskip=pCSF->GetInt("Display","FrameSkip",frameskip);
    FSDoVsync=pCSF->GetBool("Display","FSDoVsync",FSDoVsync);
#if defined(SSE_VID_D3D)
    OPTION_FAKE_FULLSCREEN=pCSF->GetBool("Display","FakeFullScreen",OPTION_FAKE_FULLSCREEN);
    if(!OPTION_FAKE_FULLSCREEN && !SSEConfig.TrueFullScreenGui)
      OPTION_FULLSCREEN_GUI=0;
#endif
    OPTION_MAX_FS=pCSF->GetBool("Display","FullscreenOnMaximize",OPTION_MAX_FS);
#if defined(SSE_VID_DD) || defined(UNIX)
    prefer_res_640_400=pCSF->GetBool("Display","Prefer640x400",prefer_res_640_400);
#endif
    OPTION_FULLSCREEN_DEFAULT_HZ=pCSF->GetBool("Display","FullScreenDefaultHz",
                                               OPTION_FULLSCREEN_DEFAULT_HZ);
    ResChangeResize=pCSF->GetBool("Display","ResChangeResize",ResChangeResize);
#if defined(SSE_VID_DD)
    draw_fs_fx=pCSF->GetByte("Options","InterlaceMode",draw_fs_fx);
    if(draw_fs_fx==DFSFX_BLUR)
      draw_fs_fx=DFSFX_NONE;
    Disp.fs_res_choice=pCSF->GetByte("Options","fs_res_choice",Disp.fs_res_choice);
#endif
#ifdef UNIX
    ResChangeResize=true;
#endif
    WinSizeForRes[LORES]=pCSF->GetByte("Display","WinSizeLowRes",WinSizeForRes[LORES]);
    WinSizeForRes[MEDRES]=pCSF->GetByte("Display","WinSizeMedRes",WinSizeForRes[MEDRES]);
    WinSizeForRes[HIRES]=pCSF->GetByte("Display","WinSizeHighRes",WinSizeForRes[HIRES]);
#ifdef WIN32
    if(!WinSizeForRes[HIRES])
      WinSizeForRes[HIRES]++; // assume player wants normal (0 was normal before)
    int sl=pCSF->GetInt("Display","DrawWinMode_LowRes",-1);
    int sm=pCSF->GetInt("Display","DrawWinMode_MedRes",-1);
#if defined(SSE_VID_SIZE4)
    draw_win_mode[2]=pCSF->GetInt("Display","DrawWinMode_HiRes",draw_win_mode[2]);
#endif
    if(sl<0)
    {
      sl=sm=DWM_NOSTRETCH;
      if(draw_fs_fx==DFSFX_GRILLE && sl==DWM_NOSTRETCH)
        sl=sm=DWM_GRILLE;
    }
    draw_win_mode[0]=sl;
    draw_win_mode[1]=sm;
#endif//WIN32
#if defined(SSE_VID_SIZE4)
    DISPLAY_SIZE=pCSF->GetByte("Display","DisplaySize",2);
    SSEConfig.Size4=(DISPLAY_SIZE==4);
#endif
    StemWinResize(); // recreate surfaces or crash
    TRACE_LOG("DISPLAY_SIZE %d\n",DISPLAY_SIZE);
    SSEOptions.FullSpectrumPal=pCSF->GetBool("Display","VividColours",SSEOptions.FullSpectrumPal); // ancient name
    // Loading of border is now practically ignored (because it can be set to 0
    // by going to windowed mode). Only used first load of v2.06.
    border=MIN(pCSF->GetByte("Display","Border",border),(BYTE)BIGGEST_BORDER);
    border_last_chosen=MIN(pCSF->GetByte("Display","BorderLastChosen",border),(BYTE)BIGGEST_BORDER);
    if(!Disp.BorderPossible())
    {
      border=0;
      EnableBorderOptions(FALSE);
    }
    else
    {
      border=border_last_chosen;
      ChangeBorderSize(border);
    }
    col_brightness=(short)pCSF->GetInt("Options","Brightness",col_brightness);
    col_contrast=(short)pCSF->GetInt("Options","Contrast",col_contrast);
    col_gamma[0]=(short)pCSF->GetInt("Options","GammaR",col_gamma[0]);
    col_gamma[1]=(short)pCSF->GetInt("Options","GammaG",col_gamma[1]);
    col_gamma[2]=(short)pCSF->GetInt("Options","GammaB",col_gamma[2]);
    make_palette_table(col_brightness,col_contrast);
#if defined(SSE_VID_DD)
    for(int res=0;res<NPC_HZ_CHOICES;res++) 
    {
      prefer_pc_hz[res]=pCSF->GetInt("Options",Str("Hz_")+res,
        prefer_pc_hz[res]);
      tested_pc_hz[res]=(WORD)pCSF->GetInt("Options",Str("TestedHz_")+res,
        tested_pc_hz[res]);
    }
#endif
#ifdef UNIX
    Disp.DoAsyncBlit=pCSF->GetBool("Options","DoAsyncBlit",Disp.DoAsyncBlit);
#endif
    ScreenShotFol=pCSF->GetStr("Options","ScreenShotFol",UsersPath+SLASH+"screenshots");
    NO_SLASH(ScreenShotFol);
    if(GetFileAttributes(ScreenShotFol)==INVALID_FILE_ATTRIBUTES)
    {
#ifndef ONEGAME
      ScreenShotFol=UsersPath+SLASH+T("screenshots");
      CreateDirectory(ScreenShotFol,NULL);
#else
      ScreenShotFol=WriteDir;
#endif
    }
    Disp.ScreenShotMinSize=pCSF->GetBool("Options","ScreenShotMinSize",Disp.ScreenShotMinSize);
    Disp.ScreenShotFormat=pCSF->GetInt("Options","ScreenShotFormat",Disp.ScreenShotFormat);
#ifdef WIN32
    Disp.ScreenShotExt=pCSF->GetStr("Options","ScreenShotExt",Disp.ScreenShotExt);
#if !defined(SSE_NO_FREEIMAGE)
    Disp.ScreenShotFormatOpts=pCSF->GetInt("Options","ScreenShotFormatOpts",Disp.ScreenShotFormatOpts);
    //Disp.FreeImageLoad();
#endif
#if defined(SSE_VID_D3D)
    Disp.TextureFilter=pCSF->GetInt("Display","TextureFilter",Disp.TextureFilter);
#endif
#endif//WIN32
    FSQuitAskFirst=pCSF->GetBool("Options","FSQuitAskFirst",FSQuitAskFirst);
#if defined(SSE_VID_SINGLEPIX)
    SSEOptions.SinglePixels=pCSF->GetBool("Display","SinglePixels",SSEOptions.SinglePixels);
#endif
  }
#ifndef SSE_NO_OSD
  //SEC(PSEC_OSD) 
  if(!SecDisabled[PSEC_OSD])
  {
    OsdControl.show_disk_light=pCSF->GetBool("Options","OSDDiskLight",OsdControl.show_disk_light);
    OsdControl.show_plasma=pCSF->GetByte("Options","OSDPlasma",OsdControl.show_plasma);
    OsdControl.show_speed=pCSF->GetByte("Options","OSDSpeed",OsdControl.show_speed);
    OsdControl.show_icons=pCSF->GetByte("Options","OSDIcons",OsdControl.show_icons);
    OsdControl.show_cpu=pCSF->GetByte("Options","OSDCPU",OsdControl.show_cpu);
    OsdControl.show_scrollers=pCSF->GetBool("Options","OSDScroller",OsdControl.show_scrollers);
    OsdControl.show_jokes=pCSF->GetBool("Options","OSDjokes",OsdControl.show_jokes);
    OsdControl.ScrollerFrequency=pCSF->GetByte("Options","OSDScrollerFrequency",
                                               OsdControl.ScrollerFrequency);
    OsdControl.SecondsBetweenScrollers=pCSF->GetInt("Options","OSDSecondsBetweenScrollers",
                                                    OsdControl.SecondsBetweenScrollers);
    OsdControl.disable=pCSF->GetBool("Options","OSDDisable",OsdControl.disable);
    OPTION_DRIVE_INFO=pCSF->GetBool("Options","OsdDriveInfo",OPTION_DRIVE_INFO);
    OPTION_OSD_DEBUGINFO=pCSF->GetBool("Options","OsdDebugInfo",OPTION_OSD_DEBUGINFO);
#if defined(SSE421R2)
    OPTION_OSD_FPSINFO=pCSF->GetInt("Options","OsdFpsInfo",OPTION_OSD_FPSINFO);
#endif
    OPTION_NO_OSD_ON_STOP=pCSF->GetBool("Options","OsdNoneOnStop",OPTION_NO_OSD_ON_STOP);
  }
#endif//#ifndef SSE_NO_OSD
  //SEC(PSEC_SOUND) 
  if(!SecDisabled[PSEC_SOUND])
  {
    SoundVolume=pCSF->GetInt("Options","Volume",SoundVolume);
    SoundVolume=MIN(SoundVolume,0);
    psg_hl_filter=pCSF->GetByte("Options","SoundMode",psg_hl_filter);
    OPTION_SOUNDMUTE=pCSF->GetBool("Sound","SoundMute",OPTION_SOUNDMUTE);
    if(OPTION_SOUNDMUTE)
      SSEConfig.SoundMute=1;
    int slq=pCSF->GetInt("Options","SoundLowQuality",999);
    if(slq==0||slq==1) 
    {
      sound_chosen_freq=slq?25033:50066;
      pCSF->SetStr("Options","SoundLowQuality","999");
    }
    else if(sound_comline_freq==0)
      sound_chosen_freq=pCSF->GetInt("Sound","Freq",sound_chosen_freq);
    UpdateSoundFreq();
    sound_num_bits=(BYTE)pCSF->GetInt("Sound","Bits",sound_num_bits);
    sound_num_channels=(BYTE)pCSF->GetInt("Sound","Channels",sound_num_channels);
    sound_bytes_per_sample=(sound_num_bits/8)*sound_num_channels;
    psg_write_n_screens_ahead=pCSF->GetByte("Sound","WriteAhead",psg_write_n_screens_ahead);
    WAVOutputFile=pCSF->GetStr("Sound","WAVOutputFile",WAVOutputFile);
    if(WAVOutputFile.Empty()) 
      WAVOutputFile=UsersPath+SLASH "st.wav";
    RecordWarnOverwrite=pCSF->GetBool("Sound","RecordWarnOverwrite",RecordWarnOverwrite);
    WAVOutputDir=pCSF->GetStr("Sound","WAVOutputDir",WAVOutputDir);
    if(WAVOutputDir.Empty()) 
      WAVOutputDir=UsersPath;
#if defined(SSE_TOS_KEYBOARD_CLICK)
    OPTION_KEYBOARD_CLICK=pCSF->GetBool("Sound","KeyboardClick",OPTION_KEYBOARD_CLICK);
#endif
#ifdef UNIX
    x_sound_lib=pCSF->GetInt("Sound","Library",x_sound_lib);
#ifndef NO_RTAUDIO
    rt_unsigned_8bit=pCSF->GetInt("Sound","RtAudioUnsigned8Bit",rt_unsigned_8bit);
#endif
    sound_device_name=pCSF->GetStr("Sound","PADevice",Str(sound_device_name));
    if(!FirstLoad)
    {
      SoundStop();
      InitSound();
      SoundStart();
    }
#endif//UNIX
#if !defined(SSE_NO_INTERNAL_SPEAKER)
    sound_internal_speaker=pCSF->GetInt("Sound","InternalSpeaker",sound_internal_speaker);
    // Trying to write to ports on WINNT causes the program to be killed!
    WIN_ONLY(if(WinNT) sound_internal_speaker=0; )
#endif
    OPTION_SOUND_RECORD_FORMAT=(BYTE)pCSF->GetInt("Sound","SoundRecordFormat",OPTION_SOUND_RECORD_FORMAT);
#if defined(SSE_SOUND_MICROWIRE_HACKS)
    //OPTION_YM_12DB=pCSF->GetBool("Sound","YM12db",OPTION_YM_12DB); // before we load YM table...
    Microwire.PsgReduce=pCSF->GetByte("Sound","PsgReduce",Microwire.PsgReduce);
    SSEOptions.LmcSlowFade=pCSF->GetBool("Sound","LmcSlowFade",SSEOptions.LmcSlowFade);
#endif
#if defined(SSE_GUI_421_REORDER)
    Microwire.DmaMulti=pCSF->GetByte("Sound","DmaMulti",Microwire.DmaMulti);
#endif
#if defined(SSE_YM2149_LL)
    OPTION_MAME_YM=pCSF->GetBool("Sound","YmLowLevel",OPTION_MAME_YM);
    OPTION_SAMPLED_YM=OPTION_MAME_YM; // it used to be an apart option
    if(OPTION_SAMPLED_YM)
      Psg.LoadFixedVolTable();
    else
      Psg.FreeFixedVolTable();
    OPTION_LOWPASS=pCSF->GetWord("Sound","ym_low_pass_frequency",OPTION_LOWPASS);
#endif
#if defined(SSE_SOUND_MICROWIRE_OPTION)
    OPTION_MICROWIRE=pCSF->GetBool("Sound","Microwire",OPTION_MICROWIRE);
#endif
    Microwire.LowShelf=pCSF->GetInt("Sound","LmcLowShelf",Microwire.LowShelf);
    Microwire.HighShelf=pCSF->GetInt("Sound","LmcHighShelf",Microwire.HighShelf);
    MuteWhenInactive=pCSF->GetByte("Sound","MuteWhenInactive",MuteWhenInactive);
    SSEConfig.YmSoundOn=pCSF->GetByte("Sound","YmSoundOn",SSEConfig.YmSoundOn);
    SSEConfig.SteSoundOn=pCSF->GetByte("Sound","SteSoundOn",SSEConfig.SteSoundOn);
  }
  if(FirstLoad)
  {
    MIDIPort.Name=T("MIDI Port");
    ParallelPort.Name=T("Parallel Port");
    SerialPort.Name=T("Serial Port");
#if defined(SSE_DONGLE_PORT)
    //STPort[3].Name=T("Special Adapters");
    STPort[TSTPort::DONGLE].Name=T("Various");
#endif
  }
  //SEC(PSEC_PORTS) 
  if(!SecDisabled[PSEC_PORTS])
  {
#ifdef WIN32
    EasyStringList MidiOutList(eslNoSort),MidiInList(eslNoSort);
    Str MidiOutName,MidiInName;
    int c;
    MIDIOUTCAPS moc;
    ZeroMemory(&moc,sizeof(MIDIOUTCAPS));
    MIDIINCAPS mic;
    ZeroMemory(&mic,sizeof(MIDIINCAPS));
    c=midiOutGetNumDevs();
    for(int n=0;n<c;n++) 
    {
      midiOutGetDevCaps(n,&moc,sizeof(moc));
      MidiOutList.Add(moc.szPname);
    }
    c=midiInGetNumDevs();
    for(int n=0;n<c;n++) 
    {
      midiInGetDevCaps(n,&mic,sizeof(mic));
      MidiInList.Add(mic.szPname);
    }
#endif//WIN32
    for(int p=0;p<NSTPORTS;p++) 
    {
      EasyStr PNam=EasyStr("Port_")+p+"_";
      STPort[p].Type=pCSF->GetInt("MIDI",PNam+"Type",STPort[p].Type);
#ifdef WIN32
      int NewDev=-1;
      Str Name=pCSF->GetStr("MIDI",PNam+"MIDIOutName","");
      if(Name.NotEmpty()) 
        NewDev=MidiOutList.FindString_I(Name);
      if(NewDev>=0)
        STPort[p].MIDIOutDevice=NewDev;
      else
        STPort[p].MIDIOutDevice=pCSF->GetInt("MIDI",PNam+"MIDIOutDevice",STPort[p].MIDIOutDevice);
      NewDev=-1;
      Name=pCSF->GetStr("MIDI",PNam+"MIDIInName","");
      if(Name.NotEmpty())
        NewDev=MidiInList.FindString_I(Name);
      if(NewDev>=0)
        STPort[p].MIDIInDevice=NewDev;
      else
        STPort[p].MIDIInDevice=pCSF->GetInt("MIDI",PNam+"MIDIInDevice",STPort[p].MIDIInDevice);
      STPort[p].COMNum=pCSF->GetInt("MIDI",PNam+"COMNum",STPort[p].COMNum);
      STPort[p].LPTNum=pCSF->GetInt("MIDI",PNam+"LPTNum",STPort[p].LPTNum);
#endif
#ifdef UNIX
      for(int n=0;n<TPORTIO_NUM_TYPES;n++) {
        STPort[p].PortDev[n]=pCSF->GetStr("MIDI",PNam+"Dev_"+n,STPort[p].PortDev[n]);
        STPort[p].AllowIO[n][0]=(bool)pCSF->GetInt("MIDI",PNam+"Allow_"+n+"_Out",STPort[p].AllowIO[n][0]);
        STPort[p].AllowIO[n][1]=(bool)pCSF->GetInt("MIDI",PNam+"Allow_"+n+"_In",STPort[p].AllowIO[n][1]);
      }
      STPort[p].LANPipeIn=pCSF->GetStr("MIDI",PNam+"PipeInput",STPort[p].LANPipeIn);
#endif
      // This has to be initialised here because of RunDir/UsersPath
      STPort[p].File=UsersPath;
      switch(p) {
      case TSTPort::MIDI:
        STPort[p].File+=SLASH FILE_MIDIDUMP; //"midi.bin"
        break;
      case TSTPort::PARALLEL:
        STPort[p].File+=SLASH FILE_PRINTERDUMP; //"printer.txt"
#if defined(SSE_PRINTER)
        SSEConfig.PageRtf=pCSF->GetInt("MIDI","PageRtf",SSEConfig.PageRtf);
        SSEConfig.PagePbm=pCSF->GetInt("MIDI","PagePbm",SSEConfig.PagePbm);
#endif        
        break;
      case TSTPort::SERIAL:
        STPort[p].File+=SLASH FILE_SERIALDUMP; //"serial.bin"
        break;
      }
      STPort[p].File=pCSF->GetStr("MIDI",PNam+"File",STPort[p].File); //MIDI for all ports

#if defined(SSE_NETWORK)
      STPort[p].sIPAddr=pCSF->GetStr("PORT",PNam+"sIPAddr",STPort[p].sIPAddr);
      STPort[p].IPPort=pCSF->GetWord("PORT",PNam+"IPPort",STPort[p].IPPort);
#endif

    }//nxt p
    // Legacy
#ifdef WIN32
    if(pCSF->GetInt("MIDI","OutDevice",1000)<1000) 
    {
      MIDIPort.Type=PORTTYPE_MIDI;
      MIDIPort.MIDIOutDevice=pCSF->GetInt("MIDI","OutDevice",MIDIPort.MIDIOutDevice);
      MIDIPort.MIDIInDevice=pCSF->GetInt("MIDI","InDevice",MIDIPort.MIDIInDevice);
      int PrintDevice=pCSF->GetInt("MIDI","PrintDevice",0);
      switch(PrintDevice) {
      case 1:case 2:
        ParallelPort.Type=PORTTYPE_PARALLEL;
        ParallelPort.LPTNum=PrintDevice-1;
        break;
      case 3: ParallelPort.Type=PORTTYPE_FILE; break;
      case 4: ParallelPort.Type=PORTTYPE_MIDI; break;
      }
      ParallelPort.File=pCSF->GetStr("MIDI","PrintFilePath",ParallelPort.File);
      ParallelPort.MIDIOutDevice=pCSF->GetInt("MIDI","PrintMIDIDevice",ParallelPort.MIDIOutDevice);
      pCSF->SetStr("MIDI","OutDevice","1000");
    }
#endif
    for(int p=0;p<3;p++) 
    {
      if((STPort[p].Type==PORTTYPE_PARALLEL&&!AllowLPT)||(STPort[p].Type==PORTTYPE_COM&&!AllowCOM))
        STPort[p].Type=PORTTYPE_NONE;
    }
    if(pCSF->GetInt("MIDI","Port_0_MIDIMessMax",0)==0) 
    {
      MIDI_out_volume=(WORD)pCSF->GetInt("MIDI","OutVolume",MIDI_out_volume);
      MIDI_in_sysex_max=pCSF->GetInt("MIDI","InMaxSysEx",MIDI_in_sysex_max);
      MIDI_out_sysex_max=pCSF->GetInt("MIDI","OutMaxSysEx",MIDI_out_sysex_max);
    }
    else 
    {
      MIDI_out_volume=(WORD)pCSF->GetInt("MIDI","Port_0_MIDIVol",MIDI_out_volume);
      MIDI_in_sysex_max=pCSF->GetInt("MIDI","Port_0_MIDIMessMax",MIDI_in_sysex_max);
      MIDI_out_sysex_max=pCSF->GetInt("MIDI","Port_0_MIDIMessMax",MIDI_out_sysex_max);
      pCSF->SetInt("MIDI","Port_0_MIDIMessMax",0);
    }
    MIDI_out_running_status_flag=pCSF->GetByte("MIDI","OutRunningStatus",MIDI_out_running_status_flag);
    MIDI_in_running_status_flag=pCSF->GetByte("MIDI","InRunningStatus",MIDI_in_running_status_flag);
    SSEOptions.MidiUseTimer=pCSF->GetBool("MIDI","MidiUseTimer",SSEOptions.MidiUseTimer);
    SSEOptions.MidiUseSleep=pCSF->GetBool("MIDI","MidiUseSleep",SSEOptions.MidiUseSleep);
    MIDI_in_n_sysex=pCSF->GetInt("MIDI","InSysExBufs",MIDI_in_n_sysex);
    MIDI_out_n_sysex=pCSF->GetInt("MIDI","OutSysExBufs",MIDI_out_n_sysex);
    MIDI_in_speed=pCSF->GetInt("MIDI","InSpeed",MIDI_in_speed);
#if defined(SSE_DIRECTMIDI)
    OPTION_DIRECTMUSIC=pCSF->GetBool("MIDI","DirectMusic",OPTION_DIRECTMUSIC);
    if(!SSEConfig.DirectMusic)
      OPTION_DIRECTMUSIC=0;
    MIDI_UNITS_1SEC=(OPTION_DIRECTMUSIC)?(1000*10000):1000;
    DirectMidiClockIndex=pCSF->GetInt("MIDI","MasterClock",DirectMidiClockIndex);
     // init clock
    CLOCKINFO ClockInfo;
    DirectMidiClock.GetClockInfo(DirectMidiClockIndex,&ClockInfo);
    DirectMidiClock.ActivateMasterClock(&ClockInfo);
#endif
#if defined(SSE_MIDIRAW)
    OPTION_RAWMIDI=pCSF->GetBool("MIDI","RawMidi",OPTION_RAWMIDI);
#endif
  }
  //SEC(PSEC_MACRO) 
  if(!SecDisabled[PSEC_MACRO])
  {
    MacroDir=pCSF->GetStr(Section,"MacroDir",UsersPath+SLASH "macros");
    NO_SLASH(MacroDir);
    if(GetFileAttributes(MacroDir)==INVALID_FILE_ATTRIBUTES)
    {
#ifndef ONEGAME
      MacroDir=UsersPath+SLASH+T("macros");
      CreateDirectory(MacroDir,NULL);
#else
      MacroDir=WriteDir;
#endif
    }
    MacroSel=pCSF->GetStr(Section,"MacroSel","");
  }
  if(LastIconPath.Empty()) 
    LastIconPath=RunDir+SLASH;
  LastIconPath=pCSF->GetStr("Options","LastIconPath",LastIconPath);
  if(LastIconSchemePath.Empty()) 
    LastIconSchemePath=RunDir+SLASH;
  LastIconSchemePath=pCSF->GetStr("Options","LastIconSchemePath",LastIconSchemePath);
#ifdef UNIX
  for(int i=0;i<NUM_COMLINES;i++) {
    Comlines[i]=pCSF->GetStr("Paths",Str("Path")+i,Comlines[i]);
  }
#endif
#ifdef WIN32
  CheckMenuRadioItem(StemWin_SysMenu,IDSYS_BORDEROFF,IDSYS_BORDERON,
                     IDSYS_BORDEROFF+MIN((int)border,1),MF_BYCOMMAND);
#ifndef SSE_NO_OSD
  CheckMenuItem(StemWin_SysMenu,IDSYS_NOOSD, MF_BYCOMMAND|MF_CHECK(OsdControl.disable));
#endif
#endif
  SSEOptions.MfpStartTclk=(char)pCSF->GetInt("ControlPanel","MfpStartTclk",SSEOptions.MfpStartTclk);
  SSEOptions.MfpStopTclk=(char)pCSF->GetInt("ControlPanel","MfpStopTclk",SSEOptions.MfpStopTclk);
  SSEOptions.MfpIrqTclk=(char)pCSF->GetInt("ControlPanel","MfpIrqTclk",SSEOptions.MfpIrqTclk);
  SSEOptions.MfpTbTclk=(char)pCSF->GetInt("ControlPanel","MfpTbTclk",SSEOptions.MfpTbTclk);
  SSEOptions.MfpReadTclk=(char)pCSF->GetInt("ControlPanel","MfpReadTclk",SSEOptions.MfpReadTclk);
  SSEOptions.MfpStartCpu=(char)ConvertBitness(pCSF,"MfpStartCpu",SSEOptions.MfpStartCpu);
  SSEOptions.MfpStopCpu=(char)ConvertBitness(pCSF,"MfpStopCpu",SSEOptions.MfpStopCpu);
  SSEOptions.MfpIrqCpu=(char)ConvertBitness(pCSF,"MfpIrqCpu",SSEOptions.MfpIrqCpu);
  SSEOptions.MfpTbCpu=(char)ConvertBitness(pCSF,"MfpTbCpu",SSEOptions.MfpTbCpu);
  SSEOptions.MfpReadCpu=(char)ConvertBitness(pCSF,"MfpReadCpu",SSEOptions.MfpReadCpu);
  SSEOptions.MfpStartSync=(char)pCSF->GetInt("ControlPanel","MfpStartSync",SSEOptions.MfpStartSync);
  SSEOptions.MfpStopSync=(char)pCSF->GetInt("ControlPanel","MfpStopSync",SSEOptions.MfpStopSync);
  SSEOptions.MfpIrqSync=(char)pCSF->GetInt("ControlPanel","MfpIrqSync",SSEOptions.MfpIrqSync);
  SSEOptions.MfpTbSync=(char)pCSF->GetInt("ControlPanel","MfpTbSync",SSEOptions.MfpTbSync);
  SSEOptions.MfpReadSync=(char)pCSF->GetInt("ControlPanel","MfpReadSync",SSEOptions.MfpReadSync);
  for(int i=0;i<4;i++)
    SSEOptions.MfpWsTmg[i]=(BYTE)pCSF->GetInt("ControlPanel",Str("MfpWsTmg")+i,SSEOptions.MfpWsTmg[i]);
  SSEOptions.BlockInterrrupts=pCSF->GetBool("ControlPanel","BlockInterrrupts",SSEOptions.BlockInterrrupts);
  SSEOptions.DiscMaxTrack=pCSF->GetByte("ControlPanel","DiscMaxTrack",SSEOptions.DiscMaxTrack);
  SSEOptions.DriveRpm=pCSF->GetWord("ControlPanel","DriveRpm",SSEOptions.DriveRpm);
  SSEOptions.TrackBytes=pCSF->GetWord("ControlPanel","TrackBytes",SSEOptions.TrackBytes);
  SSEOptions.FuzzyBits=(char)pCSF->GetInt("ControlPanel","FuzzyBits",SSEOptions.FuzzyBits);
  SSEOptions.RandomizeTrack=(char)pCSF->GetInt("ControlPanel","RandomizeTrack",
                                               SSEOptions.RandomizeTrack);
#if defined(SSE_GUI_421_REORDER)
  //SSEOptions.ForceDiskDD=pCSF->GetBool("ControlPanel","ForceDiskDD",SSEOptions.ForceDiskDD);
#endif
  SSEOptions.GhostDiskRO=pCSF->GetBool("ControlPanel","GhostDiskRO",SSEOptions.GhostDiskRO);
  SSEOptions.TrackVC=(char)pCSF->GetBool("ControlPanel","TrackVC",SSEOptions.TrackVC);
  SSEOptions.RoundWriteSM=(char)pCSF->GetBool("ControlPanel","RoundWriteSM",SSEOptions.RoundWriteSM);
  SSEOptions.RoundWriteVC=(char)pCSF->GetBool("ControlPanel","RoundWriteVC",SSEOptions.RoundWriteVC);
  if(FirstLoad) 
  {
    ProfileDir=pCSF->GetStr(Section,"ProfileDir",UsersPath+SLASH "config");
    NO_SLASH(ProfileDir);
    if(GetFileAttributes(ProfileDir)==INVALID_FILE_ATTRIBUTES)
    {
#ifndef ONEGAME
      ProfileDir=UsersPath+SLASH+T("config");
      CreateDirectory(ProfileDir,NULL);
#else
      ProfileDir=WriteDir;
#endif
    }
    ProfileSel=pCSF->GetStr(Section,"ProfileSel","");
    Page=pCSF->GetInt("Options","Page",Page);
    UPDATE;
  }
}


void TOptionBox::SaveData(bool FinalSave,TConfigStoreFile *pCSF) {
  SavePosition(FinalSave,pCSF);
  pCSF->SetInt("Main","AdvancedSettings",OPTION_ADVANCED);
  pCSF->SetInt("Options","CPUBoost",nSysCyclesPerSecond/TICKS8);
#ifdef WIN32
  pCSF->SetInt("Options","AllowTaskSwitch",bAllowTaskSwitch);
#endif
  pCSF->SetInt("Options","PauseWhenInactive",bPauseWhenInactive);
  pCSF->SetInt("Sound","MuteWhenInactive",MuteWhenInactive);
  pCSF->SetInt("Sound","YmSoundOn",SSEConfig.YmSoundOn);
  pCSF->SetInt("Sound","SteSoundOn",SSEConfig.SteSoundOn);
  pCSF->SetInt("Options","AutoLoadSnapShot",AutoLoadSnapShot);
#if defined(SSE_VID_DD)
  pCSF->SetInt("Options","DrawFSMode",draw_fs_blit_mode);
#endif
  pCSF->SetInt("Display","DrawStretch",draw_stretch);
  pCSF->SetInt("Display","DrawStretchFs",draw_stretch_fs);
  pCSF->SetInt("Display","FrameSkip",frameskip);
  pCSF->SetInt("Display","FSDoVsync",FSDoVsync);
  pCSF->SetInt("Display","FullScreenDefaultHz",OPTION_FULLSCREEN_DEFAULT_HZ);
#if defined(SSE_VID_D3D)
  pCSF->SetInt("Display","FakeFullScreen",OPTION_FAKE_FULLSCREEN);
#endif
  pCSF->SetInt("Display","FullscreenOnMaximize",OPTION_MAX_FS);
#if defined(SSE_VID_DD) || defined(UNIX)
  pCSF->SetInt("Display","Prefer640x400",prefer_res_640_400);
#endif
#if defined(SSE_VID_SINGLEPIX)
  pCSF->SetInt("Display","SinglePixels",SSEOptions.SinglePixels);
#endif
  pCSF->SetInt("Options","ShowToolTips",ShowTips);
  pCSF->SetInt("Options","SpecificHacks",OPTION_HACKS);
  int current_option=(OPTION_CAPTURE_MOUSE&6)?(OPTION_CAPTURE_MOUSE&6):OPTION_CAPTURE_MOUSE;
  pCSF->SetInt("Options","CaptureMouse",current_option);
#if defined(SSE_HD6301_LL)
  pCSF->SetInt("Options","Chipset1",OPTION_C1);
#endif
  pCSF->SetInt("Options","RtcHack",OPTION_RTC_HACK);
  pCSF->SetInt("Options","Battery6301",OPTION_BATTERY6301);
#if defined(SSE_GUI_DEFAULT_ST_CONFIG)
  pCSF->SetInt("Options","StPreselect",OPTION_ST_PRESELECT);
#endif
  pCSF->SetInt("Options","EmuDetect",OPTION_EMU_DETECT);
#if defined(SSE_YM2149_LL)
  pCSF->SetInt("Sound","YmLowLevel",OPTION_MAME_YM);
  pCSF->SetInt("Sound","ym_low_pass_frequency",OPTION_LOWPASS);
#endif
#if defined(SSE_SOUND_MICROWIRE_OPTION)
  pCSF->SetInt("Sound","Microwire",OPTION_MICROWIRE);
#endif
  pCSF->SetInt("Sound","LmcLowShelf",Microwire.LowShelf);
  pCSF->SetInt("Sound","LmcHighShelf",Microwire.HighShelf);
#if defined(SSE_SOUND_MICROWIRE_HACKS)
  //pCSF->SetInt("Sound","YM12db",OPTION_YM_12DB);
  pCSF->SetInt("Sound","PsgReduce",Microwire.PsgReduce);
  pCSF->SetInt("Sound","LmcSlowFade",SSEOptions.LmcSlowFade);
#endif
#if defined(SSE_GUI_421_REORDER)
  pCSF->SetInt("Sound","DmaMulti",Microwire.DmaMulti);
#endif
  pCSF->SetInt("Options","OsdDriveInfo",OPTION_DRIVE_INFO);
#if defined(SSE_OSD_DEBUGINFO)
  pCSF->SetInt("Options","OsdDebugInfo",OPTION_OSD_DEBUGINFO);
#endif
#if defined(SSE_OSD_FPS_INFO)
  pCSF->SetInt("Options","OsdFpsInfo",OPTION_OSD_FPSINFO);
#endif
  pCSF->SetInt("Options","OsdNoneOnStop",OPTION_NO_OSD_ON_STOP);
  pCSF->SetInt("Display","Scanlines",OPTION_SCANLINES);
  pCSF->SetInt("Machine","UnstableShifter",OPTION_UNSTABLE_SHIFTER);
#if defined(SSE_GUI_STATUS_BAR) // Main, like window position & size
  pCSF->SetInt("Main","StatusBar",OPTION_STATUS_BAR);
#if defined(SSE_GUI_STATUS_BAR_CLICKABLE)
  pCSF->SetInt("Main","StatusBarMask",SSEConfig.StatusBarMask);
#endif
  pCSF->SetInt("Main","TosFlag",OPTION_TOSFLAG);
#endif
#if defined(SSE_GUI_TOOLBAR)
  pCSF->SetInt("Main","ToolBar",OPTION_TOOLBAR);
#endif
#if defined(SSE_GUI_MENUBAR)
  pCSF->SetInt("Main","MenuBar",OPTION_MENUBAR);
#endif
  pCSF->SetInt("Display","WinVSync",OPTION_WIN_VSYNC);
#if defined(SSE_VID_D3D_VSYNC)
  pCSF->SetInt("Display","AutoVSync",OPTION_AUTOVSYNC);
  pCSF->SetInt("Display","AutoVSyncFS",OPTION_AUTOVSYNC_FS);
#endif
#if defined(SSE_VID_OLDSYNC)
  pCSF->SetInt("Display","OldSync",SSEOptions.OldSync);
#endif
  pCSF->SetInt("Display","TimingLoop",OPTION_TIMINGLOOP);
#if defined(SSE_TIMINGS_US)
  pCSF->SetInt("Display","Microseconds",OPTION_MICROSECONDS);
#endif
#if defined(SSE_VID_BFI)
  pCSF->SetInt("Display","BlackFrameInsertion",OPTION_BFI);
#endif
  pCSF->SetInt("Display","TripleBufferFS",OPTION_3BUFFER_FS);
#if defined(SSE_VID_DD_3BUFFER_WIN)
  pCSF->SetInt("Display","TripleBufferWin",OPTION_3BUFFER_WIN);
#endif
#if defined(SSE_VID_D3D_SWEETFX)
  pCSF->SetInt("Display","CrtEmu",OPTION_CRT_EMU);
#endif
  pCSF->SetInt("Display","GreyScreen",OPTION_GREYSCREEN);
  pCSF->SetInt("Display","VividColours",SSEOptions.FullSpectrumPal);
#if defined(SSE_VID_D3D) // for older versions
  pCSF->SetInt("Display","Direct3D",true);
#endif
  pCSF->SetInt("Display","STAspectRatio",OPTION_ST_ASPECT_RATIO);
#if defined(SSE_GUI_OPTION_FOR_TESTS)
  pCSF->SetInt("Options","TestingNewFeatures",SSEOptions.TestingNewFeatures);
#endif
  pCSF->SetInt("Display","BlockResize",OPTION_BLOCK_RESIZE);
  pCSF->SetInt("Display","LockAspectRatio",OPTION_LOCK_AR);
#if defined(SSE_VID_D3D)
#if defined(SSE_VID_2SCREENS)
  pCSF->SetInt("Display","oldD3DMode",Disp.oldD3DMode); // still only for 2 (!)
#endif
  pCSF->SetInt("Display","D3DMode",Disp.D3DMode);
#endif
  pCSF->SetInt("Options","HwOverscan",OPTION_HWOVERSCAN);
  pCSF->SetInt("Options","VideoLogicEmu",OPTION_VLE);
  pCSF->SetInt("Display","FullscreenAR",OPTION_FULLSCREEN_AR);
  pCSF->SetInt("HardDrives","Acsi",SSEOptions.Acsi);
#if defined(SSE_TOS_KEYBOARD_CLICK)
  pCSF->SetInt("Sound","KeyboardClick",OPTION_KEYBOARD_CLICK);
#endif
  pCSF->SetInt("Display","FullScreenGUI",OPTION_FULLSCREEN_GUI);
  pCSF->SetInt("Options","VMMouse",OPTION_VMMOUSE);
  pCSF->SetInt("Options","Spurious",OPTION_SPURIOUS);
  pCSF->SetInt("Machine","Laser",OPTION_LASER);
  pCSF->SetInt("Machine","Printer",OPTION_PRINTER);
  pCSF->SetInt("Main","ToggleF12",SSEOptions.PauseRun); // old option name
  pCSF->SetInt("Main","F12Run",SSEOptions.F12Run);
#if defined(SSE_DEBUGGER)
  pCSF->SetInt("Debug","TraceFileRewind",TRACE_FILE_REWIND);
#if defined(SSE_DEBUGGER_MONITOR_VALUE)
  pCSF->SetInt("Debug","MonitorValueSpecified",Debug.MonitorValueSpecified);
#endif
#if defined(SSE_DEBUGGER_MONITOR_RANGE)
  pCSF->SetInt("Debug","MonitorRange",Debug.MonitorRange);
#endif
#if defined(SSE_DEBUGGER_FAKE_IO)
  for(int i=0;i<FAKE_IO_LENGTH/2;i++)
  {
    char buffer[15];
    sprintf(buffer,"ControlMask%d",i);
    pCSF->SetInt("Debug",buffer,Debug.ControlMask[i]);
  }
#endif
#if defined(SSE_DEBUGGER_TOGGLE)
  pCSF->SetInt("Debug","DebuggerVisible",DebuggerVisible);
#endif
#if defined(SSE_DEBUGGER_STATUS_BAR)
  pCSF->SetInt("Debug","DialogOnStopEvent",Debug.DialogOnStopEvent);
#endif
#endif//#if defined(SSE_DEBUGGER)
  pCSF->SetInt("Display","ResChangeResize",ResChangeResize);
  pCSF->SetInt("Display","WinSizeLowRes",WinSizeForRes[LORES]);
  pCSF->SetInt("Display","WinSizeMedRes",WinSizeForRes[MEDRES]);
  pCSF->SetInt("Display","WinSizeHighRes",WinSizeForRes[HIRES]);
#ifdef WIN32
  pCSF->SetInt("Display","DrawWinMode_LowRes",draw_win_mode[0]);
  pCSF->SetInt("Display","DrawWinMode_MedRes",draw_win_mode[1]);
#if defined(SSE_VID_SIZE4)
  pCSF->SetInt("Display","DrawWinMode_HiRes",draw_win_mode[2]);
#endif
#endif
#if defined(SSE_VID_SIZE4)
  pCSF->SetInt("Display","DisplaySize",DISPLAY_SIZE);
#endif
  pCSF->SetInt("Display","BorderLastChosen",border_last_chosen);
#ifdef UNIX
  pCSF->SetInt("Options","DrawToVidMem",Disp.DrawToVidMem);
#endif    
  pCSF->SetInt("Options","Brightness",col_brightness);
  pCSF->SetInt("Options","Contrast",col_contrast);
  pCSF->SetInt("Options","GammaR",col_gamma[0]);
  pCSF->SetInt("Options","GammaG",col_gamma[1]);
  pCSF->SetInt("Options","GammaB",col_gamma[2]);
  pCSF->SetInt("Options","SlowMotionSpeed",slow_motion_speed);
  pCSF->SetInt("Options","Page",Page);
#if defined(SSE_VID_DD)
  for(int res=0;res<NPC_HZ_CHOICES;res++) 
  {
    pCSF->SetInt("Options",Str("Hz_")+res,prefer_pc_hz[res]);
    pCSF->SetInt("Options",Str("TestedHz_")+res,tested_pc_hz[res]);
  }
  pCSF->SetInt("Options","InterlaceMode",draw_fs_fx);
  pCSF->SetInt("Options","fs_res_choice",Disp.fs_res_choice);
#endif
#ifdef UNIX
  pCSF->SetInt("Options","DoAsyncBlit",Disp.DoAsyncBlit);
#endif
  pCSF->SetInt("Options","Volume",SoundVolume);
  pCSF->SetInt("Options","SoundMode",psg_hl_filter);
  pCSF->SetInt("Sound","SoundMute",SSEOptions.SoundMute);
  pCSF->SetInt("Options","SoundLowQuality",999);
  if(sound_chosen_freq!=sound_comline_freq) 
    pCSF->SetInt("Sound","Freq",sound_chosen_freq);
  pCSF->SetInt("Sound","Bits",sound_num_bits);
  pCSF->SetInt("Sound","Channels",sound_num_channels);
  pCSF->SetInt("Sound","WriteAhead",psg_write_n_screens_ahead);
  pCSF->SetStr("Sound","WAVOutputFile",WAVOutputFile);
  pCSF->SetInt("Sound","RecordWarnOverwrite",RecordWarnOverwrite);
  pCSF->SetStr("Sound","WAVOutputDir",WAVOutputDir);
#ifdef UNIX
  pCSF->SetInt("Sound","Library",x_sound_lib);
#ifndef NO_RTAUDIO
  pCSF->SetInt("Sound","RtAudioUnsigned8Bit",rt_unsigned_8bit);
#endif
  pCSF->SetStr("Sound","PADevice",Str(sound_device_name));
#endif
#if !defined(SSE_NO_INTERNAL_SPEAKER)
  pCSF->SetStr("Sound","InternalSpeaker",Str(sound_internal_speaker));
#endif
  pCSF->SetInt("Sound","SoundRecordFormat",OPTION_SOUND_RECORD_FORMAT);
  for(int p=0;p<NSTPORTS;p++) 
  {
    EasyStr PNam=EasyStr("Port_")+p+"_";
    pCSF->SetInt("MIDI",PNam+"Type",STPort[p].Type);
#ifdef WIN32
    Str MidiOutName,MidiInName;
    if(STPort[p].MIDIOutDevice>=0)
    {
      MIDIOUTCAPS moc;
      ZeroMemory(&moc,sizeof(MIDIOUTCAPS));
      midiOutGetDevCaps(STPort[p].MIDIOutDevice,&moc,sizeof(moc));
      MidiOutName=moc.szPname;
    }
    if(STPort[p].MIDIInDevice>=0)
    {
      MIDIINCAPS mic;
      midiInGetDevCaps(STPort[p].MIDIInDevice,&mic,sizeof(mic));
      MidiInName=mic.szPname;
    }
    pCSF->SetStr("MIDI",PNam+"MIDIOutName",MidiOutName);
    pCSF->SetInt("MIDI",PNam+"MIDIOutDevice",STPort[p].MIDIOutDevice);
    pCSF->SetStr("MIDI",PNam+"MIDIInName",MidiInName);
    pCSF->SetInt("MIDI",PNam+"MIDIInDevice",STPort[p].MIDIInDevice);
    pCSF->SetInt("MIDI",PNam+"COMNum",STPort[p].COMNum);
    pCSF->SetInt("MIDI",PNam+"LPTNum",STPort[p].LPTNum);
#endif//WIN32
#ifdef UNIX
    for(int n=0;n<TPORTIO_NUM_TYPES;n++) {
      pCSF->SetStr("MIDI",PNam+"Dev_"+n,STPort[p].PortDev[n]);
      pCSF->SetInt("MIDI",PNam+"Allow_"+n+"_Out",STPort[p].AllowIO[n][0]);
      pCSF->SetInt("MIDI",PNam+"Allow_"+n+"_In",STPort[p].AllowIO[n][1]);
    }
    pCSF->SetStr("MIDI",PNam+"PipeInput",STPort[p].LANPipeIn);
#endif
    pCSF->SetStr("MIDI",PNam+"File",STPort[p].File);

#if defined(SSE_NETWORK)
    pCSF->SetStr("PORT",PNam+"sIPAddr",STPort[p].sIPAddr);
    pCSF->SetInt("PORT",PNam+"IPPort",STPort[p].IPPort);
#endif

  }//nxt p
  pCSF->SetInt("MIDI","OutRunningStatus",MIDI_out_running_status_flag);
  pCSF->SetInt("MIDI","InRunningStatus",MIDI_in_running_status_flag);
  pCSF->SetInt("MIDI","MidiUseTimer",SSEOptions.MidiUseTimer);
  pCSF->SetInt("MIDI","MidiUseSleep",SSEOptions.MidiUseSleep);
  pCSF->SetInt("MIDI","InSysExBufs",MIDI_in_n_sysex);
  pCSF->SetInt("MIDI","OutSysExBufs",MIDI_out_n_sysex);
  pCSF->SetInt("MIDI","InSpeed",MIDI_in_speed);
  pCSF->SetInt("MIDI","OutVolume",MIDI_out_volume);
  pCSF->SetInt("MIDI","InMaxSysEx",MIDI_in_sysex_max);
  pCSF->SetInt("MIDI","OutMaxSysEx",MIDI_out_sysex_max);
#if defined(SSE_PRINTER)
  pCSF->SetInt("MIDI","PageRtf",SSEConfig.PageRtf);
  pCSF->SetInt("MIDI","PagePbm",SSEConfig.PagePbm);
#endif
#if defined(SSE_DIRECTMIDI)
  pCSF->SetInt("MIDI","DirectMusic",OPTION_DIRECTMUSIC);
  pCSF->SetInt("MIDI","MasterClock",DirectMidiClockIndex);
#endif
#if defined(SSE_MIDIRAW)
  pCSF->SetInt("MIDI","RawMidi",OPTION_RAWMIDI);
#endif
  pCSF->SetInt("Options","MaxFastForward",fast_forward_max_speed);
  pCSF->SetInt("Options","HighPriority",HighPriority);
  pCSF->SetInt("Options","EmuThread",OPTION_EMUTHREAD);
#ifdef SSE_X64
  pCSF->SetStr("Options","AutoSnapShotName64",AutoSnapShotName);
#else
  pCSF->SetStr("Options","AutoSnapShotName32",AutoSnapShotName);
#endif
  pCSF->SetInt("Options","RunSpeed",run_speed_ticks_per_second);
  pCSF->SetStr("Options","ScreenShotFol",ScreenShotFol);
  pCSF->SetInt("Options","ScreenShotFormat",Disp.ScreenShotFormat);
#ifdef WIN32
  pCSF->SetStr("Options","ScreenShotExt",Disp.ScreenShotExt);
#if !defined(SSE_NO_FREEIMAGE)
  pCSF->SetInt("Options","ScreenShotFormatOpts",Disp.ScreenShotFormatOpts);
#endif
#if defined(SSE_VID_D3D)
  pCSF->SetInt("Display","TextureFilter",Disp.TextureFilter);
#endif
#endif//WIN32
  pCSF->SetInt("Options","ScreenShotMinSize",Disp.ScreenShotMinSize);
  pCSF->SetInt("Machine","STType",ST_MODEL); // but we keep STType string
  pCSF->SetStr("Machine","PreciseModel",PreciseModel);
  pCSF->SetInt("Machine","Wakeup",OPTION_WS);
  pCSF->SetInt("Machine","ShifterWU",(int)OPTION_SHIFTER_WU); // it's char
  pCSF->SetInt("Machine","RandomWakeup",(int)OPTION_RANDOM_WU); // it's char
  pCSF->SetInt("Machine","BlitterWakeup",(int)OPTION_BLITTER_WU);
  pCSF->SetInt("Machine","Mfp_xtal",Mfp.xtal);
  pCSF->SetInt("Machine","CpuCustomHz",CpuCustomHz/TICKS8);
#if defined(SSE_OPTION_FASTBLITTER)
  pCSF->SetStr("Machine","FastBlitter",EasyStr(OPTION_FASTBLITTER));
#endif
#if defined(SSE_GUI_CONFIG)
/*  v3.8.0 Remove path info if it's TOS browse path.
    We do that to make config files more portable: the full path
    is individual, but TOS files are universal.
    Only for those config files, not steem.ini (FinalSave).
*/
  EasyStr tmp=ROMFile;
  RemoveFileNameFromPath(tmp,REMOVE_SLASH);
  if(!FinalSave && tmp==TOSBrowseDir)
  {
    tmp=GetFileNameFromPath(ROMFile.Text); // don't change ROMFile itself
    pCSF->SetStr("Machine","ROM_File",tmp);
  }
  else
#endif
  {
    pCSF->SetStr("Machine","ROM_File",ROMFile);
    pCSF->SetStr("Machine","ROM_Add_Dir",TOSBrowseDir);//v410
  }
  pCSF->SetStr("Machine","Cart_File",CartFile);
  pCSF->SetStr("Machine","LastCartFile",LastCartFile);
  pCSF->SetInt("Machine","CartidgeOff",OPTION_CARTRIDGE_OFF);
  pCSF->SetInt("Machine","Colour_Monitor",!!(mfp_gpip_no_interrupt&MFP_GPIP_COLOUR));
  BYTE MemConf[2];
  GetCurrentMemConf(MemConf);
  pCSF->SetInt("Machine","Mem_Bank_1",MemConf[0]);
  pCSF->SetInt("Machine","Mem_Bank_2",MemConf[1]);
  pCSF->SetInt("Machine","ShiftSwitching",bEnableShiftSwitching);
  pCSF->SetInt("Machine","KeyboardLanguage",(int)KeyboardLangID);
#if defined(SSE_IKBD_MAPPINGFILE)
  pCSF->SetStr("Machine","KeyboardMappingPath",KeyboardMappingPath);
#endif
#ifndef NO_CRAZY_MONITOR
  pCSF->SetInt("Machine","ExMon",extended_monitor);
  pCSF->SetInt("Machine","ExMonWidth",em_width);
  pCSF->SetInt("Machine","ExMonHeight",em_height);
  pCSF->SetInt("Machine","ExMonPlanes",em_planes);
#endif
  pCSF->SetInt("Options","NewMemConf0",NewMemConf0);
  pCSF->SetInt("Options","NewMemConf1",NewMemConf1);
  pCSF->SetInt("Options","NewMonitorSel",NewMonitorSel);
  pCSF->SetStr("Options","NewROMFile",NewROMFile);
#if defined(SSE_GUI_INSTANTCHANGE)
  pCSF->SetInt("Options","NewStModel",NewStModel);
#endif
  pCSF->SetInt("Options","TOSSortDescend",eslTOS_Descend);
  pCSF->SetInt("Options","TOSSort",(int)eslTOS_Sort);

#if defined(SSE_GUI_QUICKSETTINGS)
  for(int i=0;i<TOS_LIST_LEN;i++)
  {
    pCSF->SetInt("Options",Str("TosListVer")+i,TosList.Version[i]);
    pCSF->SetStr("Options",Str("TosListFile")+i,TosList.Path[i]);
  }
#endif

  pCSF->SetInt("Options","StartOnClick",StartEmuOnClick);
  pCSF->SetStr("Options","MacroDir",MacroDir);
  pCSF->SetStr("Options","ProfileDir",ProfileDir);
  pCSF->SetStr("Options","MacroSel",MacroSel);
  pCSF->SetStr("Options","ProfileSel",ProfileSel);
  pCSF->SetInt("Options","FSQuitAskFirst",FSQuitAskFirst);
  pCSF->SetStr("Options","LastIconPath",LastIconPath);
  pCSF->SetStr("Options","LastIconSchemePath",LastIconSchemePath);
#if defined(SSE_GUI_INSTANTCHANGE)
  pCSF->SetInt("Options","InstantMachineChange",SSEOptions.InstantMachineChange);
#endif
#ifndef SSE_NO_OSD
  pCSF->SetInt("Options","OSDDiskLight",OsdControl.show_disk_light);
  pCSF->SetInt("Options","OSDPlasma",OsdControl.show_plasma);
  pCSF->SetInt("Options","OSDSpeed",OsdControl.show_speed);
  pCSF->SetInt("Options","OSDIcons",OsdControl.show_icons);
  pCSF->SetInt("Options","OSDCPU",OsdControl.show_cpu);
  pCSF->SetInt("Options","OSDScroller",OsdControl.show_scrollers);
  pCSF->SetInt("Options","OSDjokes",OsdControl.show_jokes);
  pCSF->SetInt("Options","OSDScrollerFrequency",OsdControl.ScrollerFrequency);
  pCSF->SetInt("Options","OSDSecondsBetweenScrollers",OsdControl.SecondsBetweenScrollers);
  pCSF->SetInt("Options","OSDDisable",OsdControl.disable);
#endif
#ifdef UNIX
  for(int i=0;i<NUM_COMLINES;i++) {
    pCSF->SetStr("Paths",Str("Path")+i,Comlines[i]);
  }
#endif
  pCSF->SetInt("ControlPanel","Page",Page);
  pCSF->SetInt("ControlPanel","Bitness",SSE_BITNESS);
  pCSF->SetInt("ControlPanel","MfpStartCpu",(int)SSEOptions.MfpStartCpu);
  pCSF->SetInt("ControlPanel","MfpStartTclk",(int)SSEOptions.MfpStartTclk);
  pCSF->SetInt("ControlPanel","MfpStartSync",(int)SSEOptions.MfpStartSync);
  pCSF->SetInt("ControlPanel","MfpStopCpu",(int)SSEOptions.MfpStopCpu);
  pCSF->SetInt("ControlPanel","MfpStopTclk",(int)SSEOptions.MfpStopTclk);
  pCSF->SetInt("ControlPanel","MfpStopSync",(int)SSEOptions.MfpStopSync);
  pCSF->SetInt("ControlPanel","MfpIrqCpu",(int)SSEOptions.MfpIrqCpu);
  pCSF->SetInt("ControlPanel","MfpIrqTclk",(int)SSEOptions.MfpIrqTclk);
  pCSF->SetInt("ControlPanel","MfpIrqSync",(int)SSEOptions.MfpIrqSync);
  pCSF->SetInt("ControlPanel","MfpTbCpu",(int)SSEOptions.MfpTbCpu);
  pCSF->SetInt("ControlPanel","MfpTbTclk",(int)SSEOptions.MfpTbTclk);
  pCSF->SetInt("ControlPanel","MfpTbSync",(int)SSEOptions.MfpTbSync);
  pCSF->SetInt("ControlPanel","MfpReadCpu",(int)SSEOptions.MfpReadCpu);
  pCSF->SetInt("ControlPanel","MfpReadTclk",(int)SSEOptions.MfpReadTclk);
  pCSF->SetInt("ControlPanel","MfpReadSync",(int)SSEOptions.MfpReadSync);
  for(int i=0;i<4;i++)
    pCSF->SetInt("ControlPanel",Str("MfpWsTmg")+i,(int)SSEOptions.MfpWsTmg[i]);
  pCSF->SetInt("ControlPanel","BlockInterrrupts",(int)SSEOptions.BlockInterrrupts);
  pCSF->SetInt("ControlPanel","DiscMaxTrack",(int)SSEOptions.DiscMaxTrack);
  pCSF->SetInt("ControlPanel","DriveRpm",(int)SSEOptions.DriveRpm);
  pCSF->SetInt("ControlPanel","TrackBytes",(int)SSEOptions.TrackBytes);
  pCSF->SetInt("ControlPanel","FuzzyBits",(int)SSEOptions.FuzzyBits);
  pCSF->SetInt("ControlPanel","RandomizeTrack",(int)SSEOptions.RandomizeTrack);
#if defined(SSE_GUI_421_REORDER)
  //pCSF->SetInt("ControlPanel","ForceDiskDD",(int)SSEOptions.ForceDiskDD);
#endif
  pCSF->SetInt("ControlPanel","GhostDiskRO",(int)SSEOptions.GhostDiskRO);
  pCSF->SetInt("ControlPanel","TrackVC",(int)SSEOptions.TrackVC);
  pCSF->SetInt("ControlPanel","RoundWriteSM",(int)SSEOptions.RoundWriteSM);
  pCSF->SetInt("ControlPanel","RoundWriteVC",(int)SSEOptions.RoundWriteVC);
#if defined(SSE_GUI_DRAG_PAGES)
  for(int k=0;k<MAX_OPTION_LABELS&&OptionLabel[k].IconNum!=-1;k++)
    pCSF->SetInt("Options",Str("OptionLabel")+k,OptionLabel[k].IconNum);
#endif

}


int TShortcutBox::LoadShortcutInfo(DynamicArray<TShortcutInfo> &LoadCuts,
                                   EasyStringList &StringsESL,char *File,char *Sect) {
  EasyStr ValName,MacFile;
  TConfigStoreFile CSF(File);
  int n=0;
  for(;;) 
  {
    TShortcutInfo si;
    ValName=EasyStr("Shortcut")+n++;
    si.Action=(BYTE)CSF.GetInt(Sect,ValName+"_Action",0xff);
    if(si.Action==0xff) 
      break;
    si.Id[0]=(WORD)CSF.GetInt(Sect,ValName+"_ID1",0xffff);
    si.Id[1]=(WORD)CSF.GetInt(Sect,ValName+"_ID2",0xffff);
    si.Id[2]=(WORD)CSF.GetInt(Sect,ValName+"_ID3",0xffff);
    si.PressKey=(WORD)CSF.GetInt(Sect,ValName+"_Key",0xffff);
    si.PressChar=(DWORD)CSF.GetInt(Sect,ValName+"_Char",0xffff);
    si.MacroFileIdx=-1;
    MacFile=CSF.GetStr(Sect,ValName+"_MacroFile","");
    if(MacFile.NotEmpty()) 
      si.MacroFileIdx=StringsESL.Add(MacFile);
    si.pESL=&StringsESL;
    si.Down=2;si.OldDown=2;
    LoadCuts.Add(si);
  }
  CSF.Close();
  UpdateDisableIfDownLists();
  return n;
}


void TShortcutBox::SaveShortcutInfo(DynamicArray<TShortcutInfo> &SaveCuts,char *File) {
  if(Dirty) // because it's easy to change a macro by accident (mouse wheel)
  {
    if(Alert(CurrentCutSel,T("Save changes?"),MB_YESNO|MB_ICONQUESTION)!=IDYES)
      return;
  }
  EasyStr ValName;
  TConfigStoreFile CSF(File);
  for(int n=0;n<SaveCuts.NumItems;n++) 
  {
    ValName=EasyStr("Shortcut")+n;
    CSF.SetStr("Shortcuts",ValName+"_ID1",EasyStr(SaveCuts[n].Id[0]));
    CSF.SetStr("Shortcuts",ValName+"_ID2",EasyStr(SaveCuts[n].Id[1]));
    CSF.SetStr("Shortcuts",ValName+"_ID3",EasyStr(SaveCuts[n].Id[2]));
    CSF.SetStr("Shortcuts",ValName+"_Action",EasyStr(SaveCuts[n].Action));
    CSF.SetStr("Shortcuts",ValName+"_Key",EasyStr(SaveCuts[n].PressKey));
    CSF.SetStr("Shortcuts",ValName+"_Char",EasyStr(SaveCuts[n].PressChar));
    if(SaveCuts[n].MacroFileIdx>=0)
      CSF.SetStr("Shortcuts",ValName+"_MacroFile",
        SaveCuts[n].pESL->Get(SaveCuts[n].MacroFileIdx).String);
    else
      CSF.SetStr("Shortcuts",ValName+"_MacroFile","");
  }
  CSF.SetStr("Shortcuts",EasyStr("Shortcut")+SaveCuts.NumItems+"_Action",EasyStr(0xff));
  CSF.Close();
}


void TShortcutBox::LoadAllCuts(bool NOT_ONEGAME(LoadCurrent)) {
#ifndef ONEGAME
  for(int cuts=0;cuts<2;cuts++) 
  {
    if(CurrentCutSelType!=2) 
      cuts++;
    TShortcutInfo *pCuts=(cuts==0) ? &(CurrentCuts[0]) : &(Cuts[0]);
    int NumItems=(cuts==0) ? CurrentCuts.NumItems : Cuts.NumItems;
    for(int n=0;n<NumItems;n++)
      if(pCuts[n].Down==1) 
        DoShortcutUp(pCuts[n]);
  }
  Cuts.DeleteAll();
  CutsStrings.DeleteAll();
  for(int i=0;i<CutFiles.NumStrings;i++)
    if(NotSameStr_I(CurrentCutSel,CutFiles[i].String)||!IsVisible())
      LoadShortcutInfo(Cuts,CutsStrings,CutFiles[i].String);
  if(LoadCurrent) 
  {
    CurrentCuts.DeleteAll();
    CurrentCutsStrings.DeleteAll();
    if(Handle && CurrentCutSelType>0) 
      LoadShortcutInfo(CurrentCuts,CurrentCutsStrings,CurrentCutSel);
  }
  UpdateDisableIfDownLists();
#endif
}


void TShortcutBox::LoadData(bool NOT_ONEGAME(FirstLoad),
                            TConfigStoreFile *pCSF,bool *SecDisabled) {
  //SEC(PSEC_CUT) 
  if(!SecDisabled[PSEC_CUT])
  {
    ScrollPos=pCSF->GetInt(Section,"ScrollPos0",ScrollPos);
    CurrentCutSel=pCSF->GetStr(Section,"CurrentCutSel",CurrentCutSel);
    Dirty=false;
    CurrentCutSelType=pCSF->GetInt(Section,"CurrentCutSelType",CurrentCutSelType);
    CutDir=pCSF->GetStr(Section,"CutDir",UsersPath+SLASH+"shortcuts");
    NO_SLASH(CutDir);
    NOT_ONEGAME(bool NoCutDir=false; )
    if(GetFileAttributes(CutDir)==INVALID_FILE_ATTRIBUTES)
    {
#ifndef ONEGAME
      CutDir=UsersPath+SLASH+T("shortcuts");
      CreateDirectory(CutDir,NULL);
      NoCutDir=true;
#else
      CutDir=WriteDir;
#endif
    }
    CutFiles.DeleteAll();
    for(int i=0;;i++) 
    {
      Str File=pCSF->GetStr(Section,Str("SelectedCutFile")+i,"");
      if(File.Empty()) 
        break;
      if(Exists(File)) 
        CutFiles.Add(File);
    }
#ifndef ONEGAME
    if(FirstLoad) 
    {
#ifdef DEADC0DE // forget it now!
      // Legacy, update to v2.5's new shortcuts system
      if(pCSF->GetInt("Shortcuts","Updated24Shortcuts",0)==0) 
      {
        Str OldCutDat=WriteDir+SLASH+"shortcuts.dat";
        if(Exists(OldCutDat)) 
        {
          TConfigStoreFile CSF(OldCutDat);
          if(CSF.GetInt("Shortcuts","Done25Update",0)==0) 
          {
            DynamicArray<TShortcutInfo> TempCuts;
            EasyStringList TempCutsStrings(eslNoSort);
            LoadShortcutInfo(TempCuts,TempCutsStrings,OldCutDat,"__Permanent__");
            Str NewFile=CutDir+SLASH+T("Main Shortcuts")+".stcut";int n=2;
            while(Exists(NewFile)) NewFile=CutDir+SLASH+T("Main Shortcuts")+" ("+(n++)+").stcut";
            SaveShortcutInfo(TempCuts,NewFile);
            CutFiles.Add(NewFile);
            CurrentCutSel=NewFile;
            CurrentCutSelType=2;
            EasyStringList SectList(eslNoSort);
            CSF.GetSectionNameList(&SectList);
            while((n=SectList.FindString_I("__Permanent__"))>=0) SectList.Delete(n);
            while((n=SectList.FindString_I("Shortcuts"))>=0) SectList.Delete(n);
            if(SectList.NumStrings) 
            {
              Str SetDir=CutDir+SLASH+T("Shortcut Sets");
              CreateDirectory(SetDir,NULL);
              for(int n=0;n<SectList.NumStrings;n++) {
                TempCuts.DeleteAll();
                TempCutsStrings.DeleteAll();
                LoadShortcutInfo(TempCuts,TempCutsStrings,OldCutDat,SectList[n].String);

                RemoveIllegalFromName(SectList[n].String);
                NewFile=SetDir+SLASH+SectList[n].String+".stcut";int i=2;
                while(Exists(NewFile)) NewFile=SetDir+SLASH+SectList[n].String+" ("+(i++)+").stcut";
                SaveShortcutInfo(TempCuts,NewFile);
              }
              NewFile=CSF.GetStr("Shortcuts","CurrentGame","");
              RemoveIllegalFromName(NewFile);
              if(NewFile.NotEmpty()) CutFiles.Add(SetDir+SLASH+NewFile+".stcut");
            }
            CSF.SetInt("Shortcuts","Done25Update",1);
            CSF.Close();
            NoCutDir=0;
          }
          pCSF->SetInt("Shortcuts","Updated24Shortcuts",1);
        }
      }
#endif//0
      // Default shortcuts if first run from this dir
      if(NoCutDir) 
      {
        DynamicArray<TShortcutInfo> TempCuts;
        TShortcutInfo si;
#ifdef UNIX
        int VK_PRIOR=XKeysymToKeycode(XD,XK_Page_Up);
        int VK_NEXT=XKeysymToKeycode(XD,XK_Page_Down);
        if(VK_PRIOR && VK_NEXT)
#endif
        {
          ClearSHORTCUTINFO(&si); 
          si.Id[0]=VK_PRIOR,si.Action=CUT_PRESSKEY,si.PressKey=VK_PRIOR; 
          TempCuts.Add(si);
          ClearSHORTCUTINFO(&si); 
          si.Id[0]=VK_NEXT,si.Action=CUT_PRESSKEY,si.PressKey=VK_NEXT;  
          TempCuts.Add(si);
        }
        ClearSHORTCUTINFO(&si); 
        si.Id[0]=VK_F11,si.Action=CUT_PRESSKEY,si.PressKey=VK_F11;   
        si.Id[1]=VK_RSHIFT;
        TempCuts.Add(si);
        ClearSHORTCUTINFO(&si); 
        si.Id[0]=VK_F12,si.Action=CUT_PRESSKEY,si.PressKey=VK_F12;  
        si.Id[1]=VK_RSHIFT;
        TempCuts.Add(si);
        ClearSHORTCUTINFO(&si); 
        si.Id[0]=VK_END,si.Action=CUT_TAKESCREENSHOT,TempCuts.Add(si);
        Str NewFile=CutDir+SLASH+T("Default")+".stcut";
        SaveShortcutInfo(TempCuts,NewFile);
        CutFiles.Add(NewFile);
        CurrentCutSel=NewFile;
        Dirty=false;
        CurrentCutSelType=2;
      }
    }
#endif
    LoadAllCuts(true);
    UPDATE;
  }
}


void TShortcutBox::SaveData(bool FinalSave,TConfigStoreFile *pCSF) {
  SavePosition(FinalSave,pCSF);
#ifdef WIN32
  pCSF->SetStr(Section,"ScrollPos0",EasyStr(ScrollPos));
#endif
  pCSF->SetStr(Section,"CurrentCutSel",CurrentCutSel);
  pCSF->SetInt(Section,"CurrentCutSelType",CurrentCutSelType);
  pCSF->SetStr(Section,"CutDir",CutDir);
  for(int i=0;i<CutFiles.NumStrings;i++) 
    pCSF->SetStr(Section,Str("SelectedCutFile")+i,CutFiles[i].String);
  pCSF->SetStr(Section,Str("SelectedCutFile")+CutFiles.NumStrings,"");
}


void TGeneralInfo::LoadData(bool FirstLoad,TConfigStoreFile *pCSF,bool*) {
  if(FirstLoad) 
  {
    LoadPosition(pCSF);
#ifdef WIN32
    SearchText=pCSF->GetStr(Section,"SearchText",SearchText);
#endif
    Page=pCSF->GetInt(Section,"Page",Page);
#if !defined(SSE_LIBRETRONUKE)
    if(pCSF->GetInt("GeneralInfo","Visible",0)) 
      Show();
#endif
  }
}


void TGeneralInfo::SaveData(bool FinalSave,TConfigStoreFile *pCSF) {
  SavePosition(FinalSave,pCSF);
#ifdef WIN32
  pCSF->SetStr(Section,"SearchText",SearchText);
#endif
  pCSF->SetInt(Section,"Page",Page);
}


void TPatchesBox::LoadData(bool,TConfigStoreFile *pCSF,bool *SecDisabled) {
  //SEC(PSEC_PATCH) 
  if(!SecDisabled[PSEC_PATCH])
  {
    SelPatch=pCSF->GetStr(Section,"SelPatch",SelPatch);
    PatchDir=pCSF->GetStr(Section,"PatchDir",RunDir+SLASH "patches");
    NO_SLASH(PatchDir);
    if(GetFileAttributes(PatchDir)==INVALID_FILE_ATTRIBUTES)
      if(GetFileAttributes(RunDir+SLASH+"patches")!=INVALID_FILE_ATTRIBUTES)
        PatchDir=RunDir+SLASH+"patches";
#if defined(SSE_GUI_STATUS_BAR_MOUSE)
    // mouse coordinates address can be specified in a config file
    SSEConfig.MouseAd=pCSF->GetInt(Section,"MouseAd",0);
#endif
    UPDATE;
  }
}


void TPatchesBox::SaveData(bool FinalSave,TConfigStoreFile *pCSF) {
  SavePosition(FinalSave,pCSF);
  pCSF->SetStr(Section,"SelPatch",SelPatch);
  pCSF->SetStr(Section,"PatchDir",PatchDir);
}

#undef UPDATE

#undef LOGSECTION
