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
FILE: diskman.h
DESCRIPTION: Declarations for Steem's Disk Manager.
struct TDiskManager, TDiskManFileInfo
---------------------------------------------------------------------------*/

#pragma once
#ifndef DISKMAN_DECLA_H
#define DISKMAN_DECLA_H

#include <stemdialogs.h>
#include <floppy_disk.h>
#ifdef SSE_UNIX
#include <x/hxc_dir_lv.h>
#endif
#include "SSE.h"

#define FileIsDisk(s) ExtensionIsDisk(strrchr(s,'.'))

#define IDC_MOVEHERE 4000
#define IDC_COPYHERE 4001
#define IDC_CREATESHORTCUT 4002

int ExtensionIsDisk(char *TestedExt);
bool ExtensionIsPastiDisk(char *Ext);


#pragma pack(push, 8)

struct TDiskManFileInfo {
  EasyStr Name,Path,LinkPath;
#ifdef WIN32
  FILETIME Date;
#endif
  int Image;
  bool UpFolder,Folder,ReadOnly,BrokenLink,Zip;

// Could implement the next few, in a million years!
//
//  EasyStr IconPath;int IconIdx;
//  EasyStr Description
};




struct TDiskManager : public TStemDialog {
  // ENUM
  enum { 
    NICONSIZES=4,
    IDC_HOME=80,IDC_CONTENTA=100,IDC_CONTENTB,IDC_DISKVIEW,
    IDC_QUICKFOL_BASE=6000,IDC_QUICKFOL_DIV=20,IDC_QUICKFOL_MOVEPATH=0,IDC_QUICKFOL_COPYPATH,
    IDC_QUICKFOL_LINK_TO_PATH,IDC_QUICKFOL_MOVE_LINKPATH,IDC_QUICKFOL_COPY_LINKPATH,
    IDC_CONTENT_BASE=7000,
    IDC_SET_HOME=81,IDC_BACK,IDC_FORWARD,IDC_OPTIONS,IDC_DISKMANTOOLS,
    IDC_PCDRIVE=90,IDP_DISK=97,IDC_HISTBUT=100,IDC_HIST_BASE=200,
    IDC_NEWFOLDER=1000,IDC_NEWST,IDC_NEWCUSTOM,IDC_NEWSTW,IDC_NEWHFE,IDC_REFRESH,IDC_NEWHDST,
    IDC_NEWHDSTW,IDC_BACKGROUND,
    IDC_INSERTA=1010,IDC_INSERTB,IDC_INSERTRUN,IDC_CONTENT=1015,IDC_RENAME=1020,IDC_DELETE=1030,
    IDC_READONLY=1040,IDC_CONVERTSTW,IDC_STOPMOTOR=1046,IDC_SINGLESIDE=1048,
    IDC_READONLYA=1050,IDC_READONLYB,IDC_FREEBOOT,IDC_SOUNDDIR=1054,IDC_FILESELEC=1056,
    IDC_EXPLORER=1060,IDC_FIND,IDC_FIX_SHORTCUT=1070,IDC_EXTRACT=1080,IDC_COPYCLP=1082,
    IDC_LINKGOTODISK=1090,IDC_GOTODISK,IDC_OPENFOLDER,IDC_PROPERTIES=1099,
    IDC_SWAP,IDC_EJECT,IDC_EJECTNOSAVE,IDC_PREVIOUS,IDC_NEXT,
    IDC_HIDE_ARCHIVE=2002,IDC_FOLEXPLORER,IDC_PANE,IDC_SIZE_LARGE,IDC_SIZE_SMALL,
#if defined(SSE_GUI_DM_BIGICONS)
    IDC_SIZE_32,IDC_SIZE_64,IDC_SIZE_128, //before refactoring
#endif
    IDC_DBCLICK_NONE,IDC_DBCLICK_INSERTA,IDC_DBCLICK_RUN,IDC_FINDCUR,IDC_AUTOEJECT,
    IDC_CONNECT_DRIVEB,IDC_TURBODRIVE,IDC_ARCHIRW,IDC_AUTOCLOSE,IDC_AUTOINSERTB,IDC_HIDE_EXT,
    IDC_AUTOFFWD,IDC_DMACYCLES,IDC_SIZE_THIN,IDC_SIZE_NORMAL,IDC_SIZE_WIDE,IDC_PASTI,
    IDC_PASTICONFIG,IDC_DATABASE,IDC_MFMEMU,IDC_GHOSTDISK,IDC_GHOSTDISKRO,
    IDC_RUNPRG,IDC_HIDEHIDDEN,
    IDC_MSACONV,IDC_MSACONVOPENDI,IDC_MSACONVZIPTODI,
    IDC_EXTRACT_HD=2040,IDC_PATTERNS=2100,IDC_SORT_NAME,IDC_SORT_DATE,
    IDC_INSERTA_MULTI=9000,IDC_INSERTB_MULTI=9200,IDC_INSERTRUN_MULTI=9400,IDC_TOSTW_MULTI=9600,
    IDC_MFMLOWLEVEL=2202,IDC_WRITEPROTECT=2204,IDC_PROTECTIMAGES=2206,
    IDC_ZIPPYBASE=4000,IDC_MOVESHORTCUT=4003,IDC_COPYSHORTCUT,
    IDC_CREATESHORTCUTS=4010,IDC_CREATESHORTCUTS2,IDC_MOVEGETCONTENT,IDC_ZIPPYCANCEL=4099,
    DISKVIEWSCROLL_TIMER_ID=1,MSACONV_TIMER_ID=2,
    ACTION_INSERT_A=0,ACTION_INSERT_B,ACTION_INSERT_RUN,ACTION_DOUBLE_CLICK_INSERT_A=1,
    IMG_FOLDER=0,IMG_DISK,IMG_PARENTDIR,IMG_FOLDERLINK,IMG_DISKLINK,IMG_DISKREADONLY,
    IMG_FOLDERBROKEN,IMG_DISKBROKEN,IMG_DISKZIPPED_RW,
#ifdef SSE_421R0
    IMG_ZIPPED_FILE,
#endif
    IMG_PRGFILEICO,IMG_CFG,
#if !defined(SSE_GUI_DM_BIGICONS_LESS)
    IMG_GREYDISK,IMG_BLUEDISK,
#endif
#if defined(SSE_DISK_M3U)
    IMG_PLAYLIST,
#endif
#if defined(SSE_GUI_DM_PICS)
    IMG_CUSTOM,
#endif
  };
  // FUNCTIONS
  void PerformInsertAction(int Action,EasyStr Name,EasyStr Path,EasyStr DiskInZip);
  void ExtractArchiveToSTHardDrive(Str Path);
  static void GCGetCRC(char *Path,DWORD *lpCRC,int nCRCs);
#ifdef DEADC0DE
  static BYTE* GCConvertToST(char *Path,int Num,int *pLen);
#endif
  void GetContentsSL(Str Path);
  bool GetContentsCheckExist();
  //Str GetContentsGetAppendName(Str TOSECName);
#ifdef WIN32
  static LRESULT CALLBACK WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar);
  static LRESULT CALLBACK Drive_Icon_WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar);
  static LRESULT CALLBACK DiskView_WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar);
  static LRESULT CALLBACK DriveView_WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar);
  static LRESULT CALLBACK Dialog_WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar);
  static int CALLBACK CompareFunc(LPARAM lPar1,LPARAM lPar2,LPARAM lPar3);
  void BeginDrag(int Item,HWND From);
  void MoveDrag();
  void EndDrag(SHORT x,SHORT y,bool RightDrag);
  bool DoCreateMultiLinks();
  void AddFoldersToMenu(HMENU Pop,int StartID,EasyStr NoAddFol,bool Setting);
  bool MoveOrCopyFile(bool Moving,char *From,char *To,char *DiskPath,bool SameFol);
  void PropShowFileInfo(int i);
  void AddFileOrFolderContextMenu(HMENU Pop,TDiskManFileInfo *Inf);
  void UpdateBPBFiles(Str CurDisk,Str NewDisk,bool Moving);
  void ManageWindowClasses(bool bUnreg);
  Str GetMSAConverterPath();
  void GoToDisk(Str Path,bool bRefresh,bool bFocusView=true);
  void AdaptBackground();
#ifndef SSE_NO_WINSTON_IMPORT
  HRESULT CreateLinkCheckForOverwrite(char *,char *,IShellLink *,IPersistFile *);
  bool ImportDiskExists(char *,EasyStr &),DoImport();
  void ShowImportDiag(); //public
#endif
#endif//WIN32
#ifdef UNIX
  static int WinProc(TDiskManager*,Window,XEvent*);
  void set_path(EasyStr,bool=true,bool=true);
  void UpdateDiskNames(int);
  void ToggleReadOnly(int);
  Str GetCustomDiskImage(int*,int*,int*);
  void set_home(Str);
  static int dir_lv_notify_handler(hxc_dir_lv*,int,INT_PTR);
  static int button_notify_handler(hxc_button*,int,int*);
	static int menu_popup_notifyproc(hxc_popup*,int,INT_PTR);
  static int diag_lv_np(hxc_listview *,int,INT_PTR);
  static int diag_but_np(hxc_button *,int,int*);
  static int diag_ed_np(hxc_edit *,int,INT_PTR);
  void RefreshDiskView(Str=""); //public
#endif//UNIX
  TDiskManager();
  ~TDiskManager();
  void Show(),Hide(),ToggleVisible();
  void LoadData(bool FirstLoad,TConfigStoreFile *pCSF,bool *SecDisabled=NULL);
  void SaveData(bool FinalSave,TConfigStoreFile *pCSF);
  void SwapDisks(int FocusDrive);
#if defined(SSE_DISK_SWAPPER)
  BOOL ChangeDisk(int floppyno,int direction,BOOL bStatusbar);
#endif

  // returns true on success
  bool InsertDisk(int drive,EasyStr Name,EasyStr Path,bool bDontChangeDisk=false,bool bMakeFocus=true,
                  EasyStr DiskInZip="",bool bSuppressError=false,bool bAllowInsert2=false);

  void EjectDisk(bool floppy_no,bool losechanges=false);
  bool AreNewDisksInHistory(int d);
  void InsertHistoryAdd(int d,char *Name,char *Path,char *DiskInZip="");
  void InsertHistoryDelete(int d,char *Name,char *Path,char *DiskInZip="");
  bool CreateDiskImage(char *STName,WORD Sectors,WORD SecsPerTrack,WORD Sides);
  EasyStr CreateDiskName(char *Name,char *DiskInZip);
  void SetNumFloppies(BYTE NewNum);
  void ExtractDisks(Str Path);
  void InitGetContents();
  void ShowDatabaseDiag(),ShowContentDiag();

#ifdef WIN32
  bool HasHandledMessage(MSG *mess); // public
  void SetDir(EasyStr NewFol,bool AddToHistory,EasyStr SelPath="",
              bool EditLabel=false,EasyStr SelLinkPath="",int iSelItem=0);
  bool SelectItemWithPath(char *Path,bool EditLabel=false,char *LinkPath=NULL);
  bool SelectItemWithLinkPath(char *LinkPath,bool EditLabel=false) {
    return SelectItemWithPath(NULL,EditLabel,LinkPath);
  }
  void RefreshDiskView(EasyStr SelPath="",bool EditLabel=false,
                       EasyStr SelLinkPath="",int iItem=0);
  int GetSelectedItem();
  TDiskManFileInfo *GetItemInf(int iItem,HWND LV=NULL); //v402 not inline
  void ShowLinksDiag(),ShowPropDiag(),ShowDiskDiag();
  int GetDiskSelectionSize();
  void SetDiskViewMode(int Mode);
  void LoadIcons();
  void SetDriveViewEnable(int drive,bool EnableIt);
  HWND VisibleDiag() { // used by HasHandledMessage()
    return (HWND)((DWORD_PTR)DiskDiag^(DWORD_PTR)LinksDiag^(DWORD_PTR)PropDiag
#if !defined(SSE_NO_WINSTON_IMPORT)
      ^(DWORD_PTR)ImportDiag
#endif
      ^(DWORD_PTR)ContentDiag^(DWORD_PTR)DatabaseDiag);
  }
#endif//WIN32
  //DATA
  EasyStr HistBack[DM_HISTORY_LEN],HistForward[DM_HISTORY_LEN];
  EasyStr SaveSelPath;
  EasyStr ContentsLinksPath;
  EasyStr DisksFol,HomeFol,ContentListsFol;
  EasyStr QuickFol[DM_QUICKFOL_LEN];
  struct {
    EasyStr Name,Path,DiskInZip;
  }InsertHist[2][DM_HISTORY_LEN];
  TDiskManFileInfo PropInf;
  bool HideBroken,CloseAfterIRR,HideExtension,ShowHiddenFiles;
  bool Maximized,FSMaximized;
#if defined(SSE_GUI_DM_BIGICONS)
  char IconSize; // -1: undefined 0: 16x16 1: 32x32 2: 64x64 3: 128x128
#else
  bool SmallIcons;
#endif
  BYTE AutoInsert2,nFloppyDrives;
#if defined(SSE_DISK_SWAPPER)
  bool bSwapperPattern; // look for filename with just different digit or letter
  bool ArchiveNoMore;
  int IsDiskImage(char *name); // also checks inside archive
#endif
  bool EjectDisksWhenQuit;
  bool bArchiveRW,bTurboDrive,floppy_access_ff;
  bool bDiskProtectImage;
#if defined(SSE_DISK_STX)
  bool bDiskProtectImageStx;
#endif
  BYTE mBackground;
  BYTE DiskImageType;
  EasyStringList contents_sl;
  TBpbInfo bpbi,file_bpbi,final_bpbi;
  int IconSpacing, DoubleClickAction,ContentConflictAction,SaveScroll;
  int SortBy;
  int Width,Height,FSWidth,FSHeight;
  WORD SecsPerTrackIdx,TracksIdx,SidesIdx; // Idx = actual numbers now
  EasyStringList MenuESL;
#ifdef WIN32
  EasyStr MultipleLinksPath,LinksTargetPath;
  WNDPROC Old_ListView_WndProc;
  HIMAGELIST il[NICONSIZES];
  HWND DragLV;
  HIMAGELIST DragIL;
  HWND DiskView;
  HICON DriveIcon[2],AccurateFDCIcon,DisableDiskIcon;
  HWND DatabaseDiag,ContentDiag,DiskDiag,LinksDiag,PropDiag,DiagFocus;
  HANDLE MSAConvProcess;
  int Dragging,DragWidth,DragHeight,DropTarget;
  int LastOverID;
  int MenuTarget;  
  Str MSAConvPath,MSAConvSel;
  Str DatabaseFind;
  void drag_check_for_deselect(int DeselectDropTarget);
  bool DragEntered,EndingDrag;
  bool AtHome;
  bool ExplorerFolders;
  bool DoExtraShortcutCheck;
#if !defined(SSE_NO_WINSTON_IMPORT)
  HWND ImportDiag;
  bool Importing;
  EasyStr WinSTonPath,WinSTonDiskPath,ImportPath;
  bool ImportOnlyIfExist;
  int ImportConflictAction;
#endif
#endif//WIN32
#ifdef UNIX
  int HistBackLength,HistForwardLength;
  int ArchiveTypeIdx;
  bool TempEject_InDrive[2];
  Str TempEject_Name,TempEject_DiskInZip[2];
  hxc_dir_lv dir_lv;
  hxc_button UpBut,BackBut,ForwardBut,eject_but[2];
  hxc_button DirOutput,disk_name[2],drive_icon[2];
  hxc_button HomeBut,SetHomeBut,MenuBut;
  hxc_button HardBut;
#if defined(SSE_ACSI_MNGR)
  hxc_button HardButAcsi;
#endif
#endif//UNIX
};

extern TDiskManager DiskMan; // singleton

#pragma pack(pop)

#endif//DISKMAN_DECLA_H
