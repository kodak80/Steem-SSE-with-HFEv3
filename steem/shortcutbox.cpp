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
FILE: shortcutbox.cpp
DESCRIPTION: Functions to implement Steem's flexible shortcuts system that
maps all sorts of user input to all sorts of emulator functions.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <choosefolder.h>
#include <shortcutbox.h>
#include <computer.h>
#include <translate.h>
#include <harddiskman.h>
#include <dir_id.h>
#include <key_table.h>
#include <diskman.h>
#include <osd.h>
#include <macros.h>
#include <draw.h>
#include <display.h>
#include <stjoy.h>
#ifdef DEBUG_BUILD
#include <debugger.h>
#include <debugger_trace.h>
#endif
#if defined(SSE_VID_RECORD_AVI)
#include <AVI/AviFile.h> // AVI (DD-only)
#endif
#ifdef UNIX
#include <stjoy.h>
#include <x/hxc_prompt.h>
#define SHORTCUT_ACTION_DD_WID 300
#endif


TShortcutBox ShortcutBox;

enum EShortcutBox {
  CUT_EXTRA_START=130,
  CUT_TOGGLEHARDDRIVES,
  CUT_DEFAULT_SNAPSHOT,
  CUT_SELECT_DISK_A,
  CUT_SELECT_DISK_B,
  CUT_TOGGLE_VSYNC,
  CUT_TOGGLE_BORDERS,
#if defined(SSE_DISK_SWAPPER)
  CUT_DISK_SWAPPER_PREVIOUS,CUT_DISK_SWAPPER_NEXT,
#endif
#if defined(SSE_GUI_KBD)
  CUT_CONTEXT_MENU,
#endif
  CUT_DBL_CLICK,
#if defined(SSE_VID_RECORD_AVI) // DD-only
  CUT_RECORD_VIDEO,
#endif
  CUT_JOY0,CUT_JOY1,CUT_JOY_PLUS,//CUT_JOY_MINUS,
  CUT_EXTRA_END,
  ID_PRESSKEY=4,ID_PRESSCHAR=10,ID_PLAYMACRO=11,
  CUT_OSD=26,
  HIGHEST_CUT_CONTROL_ID=ID_PLAYMACRO
};

//TODO?
#define NUM_SHORTCUTS (54+1+1 DEBUG_ONLY(+5+2+1)+(CUT_EXTRA_END-CUT_EXTRA_START))


bool CutDisableKey[256];
int shortcut_vbl_count=0,cut_slow_motion_speed=0;
DWORD CutPauseUntilSysEx_Time=0;
int CutModDown=0;

DynamicArray<TShortcutInfo> Cuts,CurrentCuts;
EasyStringList CutsStrings(eslNoSort);
EasyStringList CurrentCutsStrings(eslNoSort);
EasyStringList CutFiles;

#ifdef WIN32

HWND TShortcutBox::InfoWin;
DirectoryTree *TShortcutBox::pChooseMacroTree=NULL;
DirectoryTree TShortcutBox::DTree;

#endif

bool CutDisableAxis[MAX_PC_JOYS][20];
bool CutDisablePOV[MAX_PC_JOYS][9];
DWORD CutButtonMask[MAX_PC_JOYS];
int MouseWheelMove=0;
bool CutButtonDown[2]={0,0};
bool TShortcutBox::Picking=false;


const char *ShortcutNames[NUM_SHORTCUTS*2]=
{
  // input
  "Press ST Key ->",(char*)CUT_PRESSKEY,
  "Type ST Character ->",(char*)CUT_PRESSCHAR,
  "Press ST Left Mouse Button",(char*)18,
  "Press ST Right Mouse Button",(char*)19,
#if defined(SSE_GUI_KBD) // replaces said key on a laptop
  "Context menu (Disk manager)",(char*)CUT_CONTEXT_MENU,
#endif
  "Double click in ST",(char*)CUT_DBL_CLICK, // maybe it's silly, wanted it for myself! (on MMB)
  "Play Input ->",(char*)CUT_PLAYMACRO,
  "Stop Playing Input",(char*)48,
  "Record New Input Start/Stop",(char*)45,
  "Record New Input (Hold)",(char*)46,
  "Stop Input Recording",(char*)47,
  "Start Paste",(char*)40,
  "Stop Paste",(char*)41,
  "Toggle Paste Start/Stop",(char*)42,
  "Pause Until Next SysEx (MIDI)",(char*)38,
  // emulation control
  "Start Emulation",(char*)1,
  "Stop Emulation",(char*)2,
  "Toggle Emulation Start/Stop",(char*)CUT_TOGGLESTARTSTOP,
  "Capture Mouse",(char*)7,
  "Release Mouse",(char*)8,
  "Toggle Mouse Capture",(char*)9,
#ifdef WIN32
  "Toggle Joystick 0 select",(char*)CUT_JOY0,
  "Toggle Joystick 1 select",(char*)CUT_JOY1,
#endif
  //"Previous Joystick Setup",(char*)CUT_JOY_MINUS, // it loops anyway
  "Next Joystick Setup",(char*)CUT_JOY_PLUS,
  "Cold Reset",(char*)4,
  "Cold Reset and Run",(char*)5,
  "Warm Reset",(char*)27,
  "Fast Forward",(char*)6,
  "Fast Forward (Toggle)",(char*)51,
  "Searchlight Fast Forward",(char*)28,
  "Searchlight Fast Forward (Toggle)",(char*)52,
  "Slow Motion",(char*)33,
  "Slow Motion 10%",(char*)35,
  "Slow Motion 25%",(char*)36,
  "Slow Motion 50%",(char*)37,
  "Increase ST CPU Speed",(char*)20,
  "Decrease ST CPU Speed",(char*)21,
  "Normal ST CPU Speed",(char*)22,
  "Exit Steem",(char*)23,
  // display
  "Take Screenshot",(char*)CUT_TAKESCREENSHOT,
  "Take Multiple Screenshots (Hold)",(char*)43,
  "Toggle Border",(char*)CUT_TOGGLE_BORDERS,
  "Switch to Fullscreen",(char*)15,
  "Switch to Windowed",(char*)16,
  "Toggle Fullscreen/Windowed",(char*)CUT_TOGGLEFULLSCREEN,
  "Toggle VSync",(char*)CUT_TOGGLE_VSYNC,
#if defined(SSE_VID_RECORD_AVI)
  "Record Video",(char*)CUT_RECORD_VIDEO,
#endif
  "Hide Scrolling Message",(char*)14,
  "Show OSD",(char*)24,
  "Hide OSD",(char*)25,
  "Toggle OSD Hide/Show",(char*)CUT_OSD,
  // sound
  "Toggle Mute",(char*)12,
  "Start Sound Recording",(char*)30,
  "Stop Sound Recording",(char*)31,
  "Toggle Sound Record On/Off",(char*)32,
  // disk
  "Select Disk A",(char*)CUT_SELECT_DISK_A,
  "Select Disk B",(char*)CUT_SELECT_DISK_B,
#ifdef UNIX
  "Toggle Port 1 Joystick Active",(char*)50,"Toggle Port 0 Joystick Active",(char*)49,
#endif
  "Swap Disks In Drives",(char*)13,
  "Toggle Hard Drives On/Off",(char*)CUT_TOGGLEHARDDRIVES,
#if defined(SSE_DISK_SWAPPER)
  "Disk Swapper Previous",(char*)CUT_DISK_SWAPPER_PREVIOUS,
  "Disk Swapper Next",(char*)CUT_DISK_SWAPPER_NEXT,
#endif
  // memory snapshot
  "Load Memory Snapshot",(char*)10,
  "Save Memory Snapshot",(char*)11,
  "Save Over Last Memory Snapshot",(char*)53,
  "Load Last Memory Snapshot",(char*)54,
  "Load Default Memory Snapshot",(char*)CUT_DEFAULT_SNAPSHOT,
#ifdef DEBUG_BUILD
  "Trace Into",(char*)200,
  "Step Over",(char*)203,
  "Run to/for",(char*)204,
  "Run to RTE",(char*)201,
  "Toggle Suspend Logging",(char*)202,
  "Run to RTS",(char*)205,//sse
  "Quick Step Over",(char*)206, //sse
#if defined(SSE_DEBUGGER_TOGGLE)
  "Toggle Debugger",(char*)207,//sse
#endif
#endif
  NULL,NULL
};


TShortcutBox::TShortcutBox() {
  Section="Shortcuts";
#ifdef WIN32
#if !defined(SSE_420R4)
  Left=(GuiSM.cx_screen()-586)/2;
  Top=(GuiSM.cy_screen()-(406+GuiSM.cy_caption()))/2;
  FSLeft=(640-586)/2;
  FSTop=(480-(406+GuiSM.cy_caption()))/2;
#endif
  ScrollPos=0;
  CutFiles.Sort=eslNoSort;
  PopupOpen=false;
#endif
#ifdef UNIX
  CurrentCutSelType=0;
#endif
}


void TShortcutBox::Show() {
  int y=10;
  if(Handle!=NULL)
  {
#ifdef WIN32
    ShowWindow(Handle,SW_SHOWNORMAL);
    SetForegroundWindow(Handle);
#endif
    return;
  }
#ifdef WIN32
  if(FullScreen) 
    Top=MAX(Top,(int)MENUHEIGHT);
  Font=SSEConfig.GuiFont();
  ManageWindowClasses(SD_REGISTER);
#if defined(SSE_GUI_FIX)
  int d=3*GetSystemMetrics(92);
  Handle=CreateWindowEx(WS_EX_CONTROLPARENT,"Steem Shortcuts",T("Shortcuts"),WS_CAPTION|WS_SYSMENU,
                        Left,Top,586+d,406+GuiSM.cy_caption()+d,ParentWin,NULL,hInstance,NULL);
#else
  Handle=CreateWindowEx(WS_EX_CONTROLPARENT,"Steem Shortcuts",T("Shortcuts"),
    WS_CAPTION|WS_SYSMENU,Left,Top,586,406+GuiSM.cy_caption(),
    ParentWin,NULL,hInstance,NULL);
#endif
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
  DTree.AllowTypeChange=true;
  DTree.FileMasksESL.DeleteAll();
  DTree.FileMasksESL.Add("",0,RC_ICO_PCFOLDER);
  DTree.FileMasksESL.Add("stcut",0,RC_ICO_SHORTCUT_OFF);
  DTree.FileMasksESL.Add("stcut",0,RC_ICO_SHORTCUT_ON);
  UpdateDirectoryTreeIcons(&DTree);
  DTree.Create(Handle,10,y,300,100,(HMENU)100,WS_VISIBLE|WS_TABSTOP,
    DTreeNotifyProc,this,CutDir,T("Shortcuts"));
  InfoWin=CreateWindowEx(WS_EX_CLIENTEDGE,"Static","",WS_CHILD|WS_VISIBLE,
    320,y,250,130,Handle,(HMENU)50,hInstance,NULL);
  y+=105;
  CreateWindow("Button",T("New Shortcuts"),WS_CHILD|WS_VISIBLE|WS_TABSTOP
    |BS_CHECKBOX|BS_PUSHLIKE,10,y,145,23,Handle,(HMENU)70,hInstance,NULL);
  CreateWindow("Button",T("Change Store Folder"),WS_CHILD|WS_VISIBLE|WS_TABSTOP
    |BS_CHECKBOX|BS_PUSHLIKE,165,y,145,23,Handle,(HMENU)71,hInstance,NULL);
  y+=30;
#if defined(SSE_GUI_KBD)
  Scroller.CreateEx(WS_EX_CONTROLPARENT|WS_EX_DLGMODALFRAME,WS_CHILD|WS_VISIBLE
    |WS_VSCROLL,10,y,560,390-y,Handle,(HMENU)101,hInstance);
#else
  Scroller.CreateEx(WS_EX_DLGMODALFRAME,WS_CHILD|WS_VISIBLE|WS_VSCROLL,
    10,y,560,390-y,Handle,101,hInstance);
#endif
  Scroller.SetVDisableNoScroll(true);
  CreateWindow("Button",T("Add New"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON
    ,4,4,275-GuiSM.cx_vscroll(),23,Scroller,(HMENU)60,hInstance,NULL);
  CreateWindow("Button",T("Add Copy"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|
    BS_PUSHBUTTON,279,4,275-GuiSM.cx_vscroll(),23,Scroller,(HMENU)61,hInstance,    NULL);
  DTree.SelectItemByPath(CurrentCutSel);
  Scroller.SetVPos(ScrollPos);
  SetWindowAndChildrensFont(Handle,Font);
  Focus=DTree.hTree;
  ShowWindow(Handle,SW_SHOW);
  SetFocus(Focus);
  if(StemWin) 
    PostMessage(StemWin,WM_USER,1234,0);
#endif
#ifdef UNIX
  if (StandardShow(600,400,T("Shortcuts"),
      ICO16_CUT,ButtonPressMask,(LPWINDOWPROC)WinProc)) return;

  st_chars_ig.LoadIconsFromMemory(XD,Get_st_charset_bmp(),16,RGB(255,255,255));
	st_chars_ig.IconHeight=16;
	st_chars_ig.NumIcons=256-32;

  dir_lv.ext_sl.DeleteAll();
  dir_lv.ext_sl.Add(3,T("Parent Directory"),1,1,0);
  dir_lv.ext_sl.Add(3,"",ICO16_FOLDER,ICO16_FOLDERLINK,0);
  dir_lv.ext_sl.Add(3,"stcut",ICO16_CUTON,ICO16_CUTONLINK,0);
  dir_lv.ext_sl.Add(3,"stcut",ICO16_CUTOFF,ICO16_CUTOFFLINK,0);
  dir_lv.lpig=&Ico16;
  dir_lv.base_fol=CutDir;
  dir_lv.fol=CutDir;
  dir_lv.allow_type_change=true;
  dir_lv.show_broken_links=0;
	if (CurrentCutSel.NotEmpty()){
		dir_lv.fol=CurrentCutSel;
		RemoveFileNameFromPath(dir_lv.fol,REMOVE_SLASH);
	}
  dir_lv.create(XD,Handle,10,y,325,120,dir_lv_notify_proc,this);

  new_cut_but.create(XD,Handle,10,y+125,160,25,button_notify_proc,this,
  										BT_TEXT,T("New Shortcuts"),20000,BkCol);

  change_fol_but.create(XD,Handle,10+165,y+125,160,25,button_notify_proc,this,
  										BT_TEXT,T("Change Store Folder"),20001,BkCol);

	help_td.create(XD,Handle,345,y,245,150,BkCol);
	help_td.set_text(T("Note: Shortcuts only work when Steem's main window is activated."));
	y+=160;

  sa_border.create(XD,Handle,9,y,602-20,390-y,
                    NULL,this,BT_GROUPBOX,"",0,BkCol);

	sa.create(XD,sa_border.handle,1,1,sa_border.w-2,sa_border.h-2,NULL,this);

	if (CurrentCutSel.NotEmpty()){
		dir_lv.select_item_by_name(GetFileNameFromPath(CurrentCutSel));
	}
  LoadCutsAndCreateCutControls();

  XMapWindow(XD,Handle);

  if (StemWin) CutBut.set_check(true);

#endif
}


void TShortcutBox::Hide() {
  if(Handle==NULL) 
    return;
#ifdef WIN32
  ScrollPos=Scroller.GetVPos();
  ShowWindow(Handle,SW_HIDE);
  if(FullScreen) 
    SetFocus(StemWin);
  DestroyWindow(Handle);Handle=NULL;
  TranslatedCutNamesSL.DeleteAll();
  if(CurrentCutSelType) 
    SaveShortcutInfo(CurrentCuts,CurrentCutSel);
  LoadAllCuts();
  if(StemWin) 
    PostMessage(StemWin,WM_USER,1234,0);
  ManageWindowClasses(SD_UNREGISTER);
#endif
#ifdef UNIX
  if (XD==NULL) return;

  StandardHide();

  TranslatedCutNamesSL.DeleteAll();
	st_chars_ig.FreeIcons();

  if (CurrentCutSelType>0 && CurrentCutSel.NotEmpty()){
    SaveShortcutInfo(CurrentCuts,CurrentCutSel);
  }
	LoadAllCuts();

  if (StemWin) CutBut.set_check(0);
#endif
}


void TShortcutBox::AddPickerLine(INT_PTR p) {
#ifdef WIN32
  HWND Win;
  const int Base=1000;
  const int x=4,y=4;
  const bool PressKey=(CurrentCuts[p].Action==CUT_PRESSKEY);
  const bool PressChar=(CurrentCuts[p].Action==CUT_PRESSCHAR);
  const bool PlayMacro=(CurrentCuts[p].Action==CUT_PLAYMACRO);
#if defined(SSE_GUI_KBD)
  Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Button Picker","",WS_CHILD|BPS_INSHORTCUT|WS_TABSTOP,
    x,(int)p*30+y,65,23,Scroller,(HMENU)(Base+p*100),hInstance,NULL);
#else
  Win=CreateWindowEx(512,"Steem Button Picker","",WS_CHILD|BPS_INSHORTCUT,
    x,p*30+y,65,23,Scroller,HMENU(Base+p*100),hInstance,NULL);
#endif
  SetWindowWord(Win,0,CurrentCuts[p].Id[0]);
  CreateWindow("Static","+",WS_CHILD|SS_CENTER,
    65+x,3+(int)p*30+y,9,23,Scroller,(HMENU)(Base+p*100+6),hInstance,NULL);
#if defined(SSE_GUI_KBD)
  Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Button Picker","",WS_CHILD|BPS_INSHORTCUT|WS_TABSTOP,
    75+x,(int)p*30+y,65,23,Scroller,(HMENU)(Base+p*100+1),hInstance,NULL);
#else
  Win=CreateWindowEx(512,"Steem Button Picker","",WS_CHILD|BPS_INSHORTCUT,
    75+x,p*30+y,65,23,Scroller,HMENU(Base+p*100+1),hInstance,NULL);
#endif
  SetWindowWord(Win,0,CurrentCuts[p].Id[1]);
  CreateWindow("Static","+",WS_CHILD|SS_CENTER,
    140+x,3+(int)p*30+y,9,23,Scroller,(HMENU)(Base+p*100+7),hInstance,NULL);
#if defined(SSE_GUI_KBD)
  Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Button Picker","",WS_CHILD|
    BPS_INSHORTCUT|WS_TABSTOP,150+x,(int)p*30+y,65,23,Scroller,
    (HMENU)(Base+p*100+2),hInstance,NULL);
#else
  Win=CreateWindowEx(512,"Steem Button Picker","",WS_CHILD|BPS_INSHORTCUT,
    150+x,p*30+y,65,23,Scroller,HMENU(Base+p*100+2),hInstance,NULL);
#endif
  SetWindowWord(Win,0,CurrentCuts[p].Id[2]);
  CreateWindow("Static","=",WS_CHILD|SS_CENTER,
    215+x,3+(int)p*30+y,9,23,Scroller,(HMENU)(Base+p*100+8),hInstance,NULL);
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|WS_VSCROLL
    |CBS_DROPDOWNLIST,225+x,(int)p*30+y,(PressKey||PressChar||PlayMacro) ?
    210 : 270,300,Scroller.GetControlPage(),(HMENU)(Base+p*100+3),hInstance,NULL);
  TranslateCutNames();
  for(int n=0;n<TranslatedCutNamesSL.NumStrings;n++)
    CBAddString(Win,TranslatedCutNamesSL[n].String,
      TranslatedCutNamesSL[n].Data[0]);
  for(int i=0;i<2;i++)
  {
    if(CBSelectItemWithData(Win,CurrentCuts[p].Action)!=CB_ERR) 
      break;
    CBAddString(Win,T("Other"),CurrentCuts[p].Action);
  }
  SendMessage(Win,CB_SETDROPPEDWIDTH,270,0);
  Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Button Picker","",WS_CHILD|BPS_NOJOY|BPS_INSHORTCUT,
    440+x,(int)p*30+y,55,23,Scroller,(HMENU)(Base+p*100+ID_PRESSKEY),hInstance,NULL);
  SetWindowWord(Win,0,CurrentCuts[(int)p].PressKey);
  Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Steem ST Character Chooser","",WS_CHILD|WS_TABSTOP,440+x,
    (int)p*30+y,55,25,Scroller,(HMENU)(Base+p*100+ID_PRESSCHAR),hInstance,NULL);
  SendMessage(Win,CB_SETCURSEL,0,CurrentCuts[(int)p].PressChar);
  Win=CreateWindow("Button",T("Choose"),WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE,440+x,
    (int)p*30+y,55,25,Scroller,(HMENU)(Base+p*100+ID_PLAYMACRO),hInstance,NULL);
  SetMacroFileButtonText(Win,(int)p);
  CreateWindow("Button",T("Del"),WS_CHILD|WS_TABSTOP|BS_PUSHBUTTON,500+x,
    (int)p*30+y,49-GuiSM.cx_vscroll(),23,Scroller,(HMENU)(Base+p*100+5),hInstance,NULL);
  for(INT_PTR i=Base+p*100;i<=Base+p*100+HIGHEST_CUT_CONTROL_ID;i++)
  {
    switch(i%100) {
    case ID_PRESSKEY:
      ShowWindow(GetDlgItem(Scroller.GetControlPage(),(int)i),
        (PressKey ? SW_SHOW : SW_HIDE));
      break;
    case ID_PRESSCHAR:
      ShowWindow(GetDlgItem(Scroller.GetControlPage(),(int)i),
        (PressChar ? SW_SHOW : SW_HIDE));
      break;
    case ID_PLAYMACRO:
      ShowWindow(GetDlgItem(Scroller.GetControlPage(),(int)i),
        (PlayMacro ? SW_SHOW : SW_HIDE));
      break;
    default:
      if(GetDlgItem(Scroller.GetControlPage(),(int)i))
        ShowWindow(GetDlgItem(Scroller.GetControlPage(),(int)i),SW_SHOW);
    }
  }
#endif
#ifdef UNIX
	if (add_but[0].handle==0){
    add_but[0].create(XD,sa.handle,0,0,(sa.w-10-HXC_SCROLLBAR_WIDTH)/2-5,25,button_notify_proc,this,
                         BT_TEXT,T("Add New"),9998,BkCol);
    add_but[1].create(XD,sa.handle,0,0,(sa.w-10-HXC_SCROLLBAR_WIDTH)/2-5,25,button_notify_proc,this,
                         BT_TEXT,T("Add Copy"),9999,BkCol);
  }
  if (p<0) return;

  int Base=p*100;
  int x=5,y=5+p*30;
  hxc_buttonpicker *p_pick;
  hxc_dropdown *p_dd;

  for (int n=0;n<3;n++){
    p_pick=new hxc_buttonpicker(XD,sa.handle,x,y,80,25,picker_notify_proc,this,Base+n);
    x+=80;
    p_pick->allow_joy=true;
    p_pick->DirID=CurrentCuts[p].Id[n];

    new hxc_button(XD,sa.handle,x,y,10,25,NULL,this,
                          BT_TEXT | BT_STATIC | BT_BORDER_NONE | BT_TEXT_CENTRE,
                          (char*)((n==2) ? "=":"+"),Base+90+n,BkCol);
    x+=10;
  }

  TranslateCutNames();

  p_dd=new hxc_dropdown(XD,sa.handle,x,y,165,300,dd_notify_proc,this);
	p_dd->id=Base+3;
	p_dd->make_empty();
  for (int s=0;s<TranslatedCutNamesSL.NumStrings;s++){
    long i=TranslatedCutNamesSL[s].Data[0];
    switch (i){
  		// Fullscreen not available
    	case 15:case 16:case 17:
    		break;

    	default:
			  p_dd->sl.Add(TranslatedCutNamesSL[s].String,i);
		}
  }
  for (int i=0;i<2;i++){
  	if (p_dd->select_item_by_data(CurrentCuts[p].Action,0)>=0) break;
    p_dd->sl.Add(T("Other"),CurrentCuts[p].Action);
  }
  p_dd->dropped_w=165+5+75+40;
  x+=165+5;

  p_pick=new hxc_buttonpicker(XD,sa.handle,x,y,70,25,
								       picker_notify_proc,this,Base+4);
	p_pick->st_keys_only=true;
	p_pick->DirID=CurrentCuts[p].PressKey;

  p_dd=new hxc_dropdown(XD,sa.handle,x,y,70,400,dd_notify_proc,this);
	p_dd->make_empty();

	DynamicArray<DWORD> Chars;
	GetAvailablePressChars(&Chars);
	for (int i=0;i<Chars.NumItems;i++){
		p_dd->sl.Add("",101+(BYTE)(HIWORD(Chars[i]))-32,Chars[i]);
	}
 	p_dd->select_item_by_data(CurrentCuts[p].PressChar,1);

	p_dd->lpig=&st_chars_ig;
	p_dd->lv.lpig=&st_chars_ig;
	p_dd->lv.display_mode=1;

	p_dd->id=Base+8;

  hxc_button *p_but=new hxc_button(XD,sa.handle,x,y,70,25,button_notify_proc,
                          this,BT_TEXT,"",Base+9,BkCol);
  SetMacroFileButtonText(p_but,p);
  x+=75;

  new hxc_button(XD,sa.handle,x,y,40,25,button_notify_proc,this,
												BT_TEXT,T("Del"),Base+5,BkCol);

  ShowHidePressSTKeyPicker(p);

#endif
}



void TShortcutBox::UpdateAddButsPosition() {
#ifdef WIN32
  if(GetDlgItem(Scroller.GetControlPage(),60)==NULL) 
    return;
  SetWindowPos(GetDlgItem(Scroller.GetControlPage(),60),0,
    4,4+CurrentCuts.NumItems*30,0,0,SWP_NOZORDER|SWP_NOSIZE);
  SetWindowPos(GetDlgItem(Scroller.GetControlPage(),61),0,
    279,4+CurrentCuts.NumItems*30,0,0,SWP_NOZORDER|SWP_NOSIZE);
#endif
#ifdef UNIX
  add_but[0].x=5,add_but[0].y=5+CurrentCuts.NumItems*30;
  XMoveWindow(XD,add_but[0].handle,add_but[0].x,add_but[0].y);

  add_but[1].x=5+(sa.w-10-HXC_SCROLLBAR_WIDTH)/2+5;
  add_but[1].y=5+CurrentCuts.NumItems*30;
  XMoveWindow(XD,add_but[1].handle,add_but[1].x,add_but[1].y);
#endif
}


void ClearSHORTCUTINFO(TShortcutInfo *pSI) {
  pSI->Id[0]=pSI->Id[1]=pSI->Id[2]=0xffff;
  pSI->PressKey=0xffff;
  pSI->PressChar=0;
  pSI->OldDown=2;
  pSI->Down=2;
  pSI->Action=0;
  for(int n=0;n<5;n++) 
    pSI->DisableIfCutDownList[n]=NULL;
  pSI->MacroFileIdx=-1;
}


void TShortcutBox::TranslateCutNames() {
  if(TranslatedCutNamesSL.NumStrings==0)
  {
    TranslatedCutNamesSL.Sort=eslNoSort;
    for(int n=0;n<NUM_SHORTCUTS*2;n+=2)
    {
      if(ShortcutNames[n]==NULL)
        break;
      LONG_PTR i=(LONG_PTR)ShortcutNames[n+1];
      if(i<200)
        TranslatedCutNamesSL.Add(T((char*)ShortcutNames[n]),i);
      else
        TranslatedCutNamesSL.Add((char*)ShortcutNames[n],i); // Debug only shortcuts
    }
  }
}


void CutDisableID(WORD ID) {
  if(BLANK_DIRID(ID)) 
    return;
  if(HIBYTE(ID)==0)
  {
    CutDisableKey[ID]=true;
    BYTE STKey=key_table[ID];
    if(STKey)
    {
      if(ST_Key_Down[STKey])
        HandleKeyPress(ID,true,IGNORE_EXTEND);
    }
#ifdef WIN32
    if(ID==VK_SHIFT)
    { // 
      if(ST_Key_Down[key_table[VK_LSHIFT]]) 
        HandleKeyPress(VK_LSHIFT,true,IGNORE_EXTEND);
      if(ST_Key_Down[key_table[VK_RSHIFT]]) 
        HandleKeyPress(VK_RSHIFT,true,IGNORE_EXTEND);
    }
#endif
  }
  else if(HIBYTE(ID)>=10)
  {
    int DirID=int((HIBYTE(ID)&1)==0?LOBYTE(ID):-LOBYTE(ID));
    int JoyNum=(HIBYTE(ID)-10)/10;
    if(DirID>=200 && DirID<209)
      CutDisablePOV[JoyNum][DirID-200]=true;
    else if(DirID>=100)
      CutButtonMask[JoyNum]&=~(1<<(DirID-100));
    else if(DirID<10)
      CutDisableAxis[JoyNum][DirID+10]=true;
  }
}


#if !defined(SSE_LIBRETRONUKE)

void ShortcutsCheck() { // called by event_vbl_interrupt() + timer
#ifdef WIN32
  // only accept shortcuts when main window or disk manager active
  HWND h=GetForegroundWindow();
  if(FullScreen==0 && h!=StemWin && h!=DiskMan.Handle
#if defined(DEBUG_BUILD)
    && h!=DWin && h!=trace_window_handle
#endif
    )
    return;
#endif    
  ZeroMemory(CutDisableKey,sizeof(CutDisableKey));
  ZeroMemory(CutDisableAxis,sizeof(CutDisableAxis));
  ZeroMemory(CutDisablePOV,sizeof(CutDisablePOV));
  for(int n=0;n<MAX_PC_JOYS;n++) 
    CutButtonMask[n]=0xffffffff;
  DoSaveScreenShot&=~2; // Clear animation screenshot
  CutModDown=0;
  for(int cuts=0;cuts<2;cuts++)
  {
    if(ShortcutBox.CurrentCutSelType!=2) 
      cuts++;
    TShortcutInfo *pCuts=(TShortcutInfo*)((cuts==0)
      ? &(CurrentCuts[0]) : &(Cuts[0]));
    int NumItems=int((cuts==0)?CurrentCuts.NumItems:Cuts.NumItems);
    for(int n=0;n<NumItems;n++)
    {
      int NotPressed=-1,NumBlank=0;
      for(int b=0;b<3;b++)
      {
        if(NOT_BLANK_DIRID(pCuts[n].Id[b]))
        {
          if(!IsDirIDPressed(pCuts[n].Id[b],50,false) || !bAppActive)
            NotPressed=(int)((NotPressed==-1) ? b : 1000);
        }
        else
          NumBlank++;
        if(NotPressed==1000) 
          break;
      }
      BYTE Down=(NotPressed==-1 && NumBlank<3);
      if(NotPressed>=0 && NotPressed<=2)
        CutDisableID(pCuts[n].Id[NotPressed]);
      else if(Down)
      {
        for(int b=0;b<3;b++) 
          CutDisableID(pCuts[n].Id[b]);
      }
      pCuts[n].OldDown=pCuts[n].Down;
      if(pCuts[n].OldDown==2)
      {
        if(Down==0)
        {
          pCuts[n].OldDown=0;
          pCuts[n].Down=0;
        }
      }
      else
        pCuts[n].Down=Down;
    }
    for(int n=0;n<NumItems;n++)
    {
      for(int c=0;c<5;c++)
      {
        if(pCuts[n].DisableIfCutDownList[c]==NULL) 
          break;
        if(pCuts[n].DisableIfCutDownList[c]->Down)
        {
          if(pCuts[n].OldDown==1)
            DoShortcutUp(pCuts[n]);
          pCuts[n].Down=2;
          break;
        }
      }
    }
    for(int n=0;n<NumItems;n++)
    {
      if(pCuts[n].Down==1)
        DoShortcutDown(pCuts[n]);
      else if(pCuts[n].Down==0&&pCuts[n].OldDown==1)
        DoShortcutUp(pCuts[n]);
    }
    for(int n=0;n<NumItems;n++)
    {
      if(pCuts[n].Down==2)
      {
        bool Clash=false;
        for(int c=0;c<5;c++)
        {
          if(pCuts[n].DisableIfCutDownList[c]==NULL) 
            break;
          if(pCuts[n].DisableIfCutDownList[c]->Down)
          {
            Clash=true;
            break;
          }
        }
        if(Clash==0)
        {
          pCuts[n].OldDown=0;
          pCuts[n].Down=1;
          DoShortcutDown(pCuts[n]);
        }
      }
    }
  }
  MouseWheelMove=0;
}

#endif//#if !defined(SSE_LIBRETRONUKE)

void DoShortcutDown(TShortcutInfo &Inf) {
#ifdef WIN32
  if(GetWindowLong(StemWin,GWL_STYLE) & WS_DISABLED) 
    return;
#endif
  if(ShortcutBox.Picking) 
    return;
  if(!bAppActive) 
    return;
  if(Inf.Action==0)
  {
    if(Inf.PressKey==VK_SHIFT||Inf.PressKey==VK_LSHIFT) 
      CutModDown|=1;
    if(Inf.PressKey==VK_SHIFT||Inf.PressKey==VK_RSHIFT) 
      CutModDown|=2;
    if(Inf.PressKey==VK_CONTROL) 
      CutModDown|=b00001100;
    if(Inf.PressKey==VK_MENU) 
      CutModDown|=b00110000;
  }
  switch(Inf.Action) {
  case 20:case 21:case 34:case 35:case 36:case 37:case 43:
#ifdef SSE_DEBUGGER
  case 206: // don't press key for each NOP: key repeat
#endif
    break;
  default:
    if(Inf.OldDown) 
      return;
  }
  switch(Inf.Action) {
  case 0:
    if(NOT_BLANK_DIRID(Inf.PressKey)&&runstate==RUNSTATE_RUNNING &&
      GetForegroundWindow()==StemWin)
    {
      int Extend=IGNORE_EXTEND;
      if(HIBYTE(Inf.PressKey)==1) 
        Extend=1;
      if(Inf.PressKey==VK_SHIFT)
      {
        HandleKeyPress(VK_LSHIFT,false,Extend);
        HandleKeyPress(VK_RSHIFT,false,Extend);
      }
      else
        HandleKeyPress(Inf.PressKey,0,Extend|NO_SHIFT_SWITCH);
#ifdef WIN32
      int n=0;
      while(TaskSwitchVKList[n])
      {
        if(Inf.PressKey==TaskSwitchVKList[n])
        {
          CutTaskSwitchVKDown[n]=true;
          break;
        }
        n++;
      }
#endif
    }
    break;
  case 1:
    if(fast_forward==0)
    {
      SetForegroundWindow(StemWin);
      if(runstate!=RUNSTATE_RUNNING)
      {
        CLICK_PLAY_BUTTON();
      }
    }
    break;
  case 2:
    if(runstate==RUNSTATE_RUNNING)
    {
      CLICK_PLAY_BUTTON();
    }
    break;
  case 3:
    if(runstate!=RUNSTATE_RUNNING) 
    {
      SetForegroundWindow(StemWin);
    }
    CLICK_PLAY_BUTTON();
    break;
  case 4:
    reset_st(RESET_COLD|RESET_STOP|RESET_CHANGESETTINGS|RESET_BACKUP|RESET_COUNT);
    break;
  case 5:
    SetForegroundWindow(StemWin);
    reset_st(RESET_COLD|RESET_NOSTOP|RESET_CHANGESETTINGS|RESET_BACKUP|RESET_COUNT);
    if(runstate!=RUNSTATE_RUNNING)
    {
      CLICK_PLAY_BUTTON();
    }
    break;
  case 6:
    fast_forward_change(true,false);
    break;
  case 7:
    if(FullScreen==0)
    {
      if(stem_mousemode==STEM_MOUSEMODE_DISABLED && runstate==RUNSTATE_RUNNING)
      {
        SetForegroundWindow(StemWin);
        SetStemMouseMode(STEM_MOUSEMODE_WINDOW);
      }
    }
    else
    {
      if(runstate!=RUNSTATE_RUNNING)
      {
        SetForegroundWindow(StemWin);
        CLICK_PLAY_BUTTON();
      }
    }
    break;
  case 8:
    if(stem_mousemode!=STEM_MOUSEMODE_DISABLED && runstate==RUNSTATE_RUNNING)
    {
      if(FullScreen==0)
        SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
      else
      {
        CLICK_PLAY_BUTTON();
      }
    }
    break;
  case 9:
    if(FullScreen==0)
    {
      if(runstate==RUNSTATE_RUNNING)
      {
        if(stem_mousemode==STEM_MOUSEMODE_DISABLED)
        {
          SetForegroundWindow(StemWin);
          SetStemMouseMode(STEM_MOUSEMODE_WINDOW);
        }
        else
          SetStemMouseMode(STEM_MOUSEMODE_DISABLED);
      }
    }
    else
    {
      if(runstate!=RUNSTATE_RUNNING) 
        SetForegroundWindow(StemWin);
      CLICK_PLAY_BUTTON();
    }
    break;
  case 10:case 11:case 53:case 54:case CUT_DEFAULT_SNAPSHOT:
  {
    ShortcutBox.bSnapshotLoading=true;
    int i=IDC_LOADSNAPSHOT-10+Inf.Action; // 200=Load, 201=Save
    if(Inf.Action==53) 
      i=IDC_SNAPSHOTSAVEOVER; // 205=Save Over Last
    if(Inf.Action==54)
    {
      if(StateHist[0].Empty()) 
        break;
      i=IDC_SNAPSHOT_HISTORY; // 210=Load StateHist[0]
    }
    if(Inf.Action==CUT_DEFAULT_SNAPSHOT)
      i=IDC_DEFAULTSNAPHOT;
#ifdef WIN32
    PostMessage(StemWin,WM_COMMAND,i,0);
#endif
#ifdef UNIX
    SnapShotBut.set_check(true);
    SnapShotProcess(i);
    SnapShotBut.set_check(0);
#endif
    break;
  }
  case 12: //Toggle Mute
  {
    OptionBox.SoundMute(!SSEOptions.SoundMute);
    break;
  }
  case 13: //Swap Disks
    DiskMan.SwapDisks(-1);
    break;
#ifndef SSE_NO_OSD
  case 14:  // Stop current scroller
    if(timeGetTime()<osd_scroller_finish_time) 
      osd_scroller_finish_time=0;
    break;
#endif
  case 15:case 16:case 17:
  {
    int i=Inf.Action;
    if(i==17)
      i=(FullScreen?16 /*to windowed*/:15 /*to fullscreen*/);
    if(i==15)
    {
      if(FullScreen==0&&Disp.CanGoToFullScreen())
      {
#ifdef WIN32
        PostMessage(StemWin,WM_SYSCOMMAND,SC_MAXIMIZE,2);
#endif
#ifdef UNIX
        Disp.GoToFullscreenOnRun=true;
        FullScreenBut.set_check(true);
        if(runstate==RUNSTATE_RUNNING)
        {
          runstate=RUNSTATE_STOPPING;
          RunWhenStop=true;
        }
#endif
      }
    }
    else if(FullScreen)
    {
#ifdef WIN32
      if(runstate==RUNSTATE_RUNNING)
        Disp.RunOnChangeToWindow=true;
      PostMessage(StemWin,WM_COMMAND,MAKEWPARAM(IDC_BACKTOWIN,BN_CLICKED),
        (LPARAM)GetDlgItem(StemWin,IDC_BACKTOWIN));
#endif
#ifdef UNIX
      Disp.GoToFullscreenOnRun=false;
      FullScreenBut.set_check(0);
      if(runstate==RUNSTATE_RUNNING)
      {
        runstate=RUNSTATE_STOPPING;
        RunWhenStop=true;
      }
#endif
    }
    break;
  }
  case 18:
    CutButtonDown[0]=true;
    break;
  case 19:
    CutButtonDown[1]=true;
    break;
  case 20:
    if(nSysCyclesPerSecond<CPU_MAX_HERTZ)
    {
      nSysCyclesPerSecond+=1000000*TICKS8;
#ifndef SSE_NO_OSD
      if(runstate==RUNSTATE_RUNNING) 
        osd_init_run(false);
#endif
      AdaptCpuBoost();
    }
    break;
  case 21:
    if(nSysCyclesPerSecond>CpuNormalHz)
    {
      nSysCyclesPerSecond-=1000000*TICKS8;
#ifndef SSE_NO_OSD
      if(runstate==RUNSTATE_RUNNING) 
        osd_init_run(false);
#endif
      AdaptCpuBoost();
    }
    break;
  case 22:
    nSysCyclesPerSecond=CpuNormalHz;
    AdaptCpuBoost();
    break;
  case 23:
    QuitSteem();
    break;
#ifndef SSE_NO_OSD
  case 24:
    if(runstate==RUNSTATE_RUNNING)
      osd_init_run(false);
     break;
  case 25:
    if(runstate==RUNSTATE_RUNNING)
      osd_hide();
    break;
  case CUT_OSD:
    if(runstate==RUNSTATE_RUNNING)
    {
      if(!OsdControl.bOsdDrawn)
        osd_init_run(false);
      else
        osd_hide();
    }
    break;
#endif
  case 27:
    reset_st(RESET_WARM|RESET_NOSTOP|RESET_CHANGESETTINGS|RESET_BACKUP|RESET_COUNT);
    break;
  case 28:
    fast_forward_change(true,true);
    break;
  case 43: //SS multiple
    DoSaveScreenShot|=2;
    break;
  case CUT_TAKESCREENSHOT:
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
    break;
  case 30:
    OptionBox.SetSoundRecord(true);
    break;
  case 31:
    OptionBox.SetSoundRecord(0);
    break;
  case 32:
    OptionBox.SetSoundRecord(!bSoundRecord);
    break;
  case 33:
    if(slow_motion==0) 
      slow_motion_change(true);
    break;
  case 35:
    cut_slow_motion_speed=100;
    if(slow_motion==0) 
      slow_motion_change(true);
    break;
  case 36:
    cut_slow_motion_speed=250;
    if(slow_motion==0) 
      slow_motion_change(true);
    break;
  case 37:
    cut_slow_motion_speed=500;
    if(slow_motion==0) 
      slow_motion_change(true);
    break;
  case 38:
    CutPauseUntilSysEx_Time=timeGetTime()+5000;
    break;
  case CUT_PRESSCHAR:
  {
    if(Inf.PressChar==0||runstate!=RUNSTATE_RUNNING) 
      break;
    int ModifierRestoreArray[3]={0,0,0};
    BYTE STCode=LOBYTE(LOWORD(Inf.PressChar));
    BYTE Modifiers=HIBYTE(LOWORD(Inf.PressChar));
    ShiftSwitchChangeModifiers(Modifiers & BIT_0,(Modifiers & BIT_1)!=0,ModifierRestoreArray);
    keyboard_buffer_write_n_record(STCode);
    keyboard_buffer_write_n_record(STCode|BIT_7);
    ShiftSwitchRestoreModifiers(ModifierRestoreArray);
    break;
  }
  case 40:case 41:case 42:
  {
    int n=STPASTE_START;
    if(Inf.Action==41) 
      n=STPASTE_STOP;
    if(Inf.Action==42) 
      n=STPASTE_TOGGLE;
    PasteIntoSTAction(n);
    break;
  }
  case 44:
    if(Inf.MacroFileIdx<0) 
      break;
    if(macro_play) 
      macro_end(MACRO_ENDPLAY);
    macro_play_file=Inf.pESL->Get(Inf.MacroFileIdx).String;
    macro_advance(MACRO_STARTPLAY);
    break;
  case 45:case 46:
  {
    if(macro_record)
    {
      macro_end(MACRO_ENDRECORD);
      break;
    }
    Str File=OptionBox.CreateMacroFile(0);
    if(File.Empty()) 
      break;
    macro_record_file=File;
    macro_advance(MACRO_STARTRECORD);
    break;
  }
  case 47:
    if(macro_record) 
      macro_end(MACRO_ENDRECORD);
    break;
  case 48:
    if(macro_play) 
      macro_end(MACRO_ENDPLAY);
    break;
#ifdef UNIX
  case 49:case 50:
  {
    int i=(Inf.Action)-49;
    Joy[i].ToggleKey=!Joy[i].ToggleKey;
    JoyConfig.enable_but[i].set_check(bool(Joy[i].ToggleKey));
    break;
  }
#endif
  case 51:case 52:
    if(fast_forward_stuck_down)
    {
      fast_forward_stuck_down=false;
      fast_forward_change(false,false);
    }
    else
    {
      fast_forward_stuck_down=true;
      fast_forward_change(true,(Inf.Action==52));
    }
    break;
  case CUT_TOGGLEHARDDRIVES:	// toggle hard drives on/off
    HardDiskMan.DisableHardDrives=!HardDiskMan.DisableHardDrives;	// toggle
    HardDiskMan.update_mount();
    REFRESH_STATUS_BAR_GX;
    break;
#if defined(SSE_VID_RECORD_AVI)
  case CUT_RECORD_VIDEO:
    if(video_recording)
    {
      video_recording=2; // stop
    }
    else
      video_recording=1; // start
    break;
#undef LOGSECTION
#endif  
  case CUT_TOGGLE_VSYNC:
    OPTION_WIN_VSYNC=!OPTION_WIN_VSYNC;
#if defined(SSE_VID_D3D)
    Disp.ScreenChange(); // create new surfaces
#endif
    UPDATE_STATUS_BAR_PART(SB_PART_FREQ);
#ifdef WIN32
    if(OptionBox.IsVisible())
    {
      OptionBox.DestroyCurrentPage();
      OptionBox.CreatePage(OptionBox.Page);
    }
#endif    
    break;
  case CUT_TOGGLE_BORDERS:
#ifdef WIN32  
    OptionBox.SetBorder((border) ? 0 : 1);
    OptionBox.UpdateWindowSizeAndBorder();
#endif    
    break;
/*  Request. v3.7.3.
    So one can insert disk images in drives without using the disk manager.
    Scripts can then be used in Window's file selector.
    This is useful for those who use Steem in a cabinet.
    Update v4.0: there's also alt menu now
*/
  case CUT_SELECT_DISK_A:
  case CUT_SELECT_DISK_B:
  {
#ifdef WIN32    //TODO
    char *fstypes=FSTypes(2,NULL);
    EasyStr path=FileSelect(NULL,T("Select Disk Image"),DiskMan.DisksFol,fstypes,1,true,"");
    free(fstypes);
    EasyStr name=GetFileNameFromPath(path);
    DiskMan.InsertDisk(Inf.Action-CUT_SELECT_DISK_A,name,path,false,false,"",true);
#endif    
    break;
  }
#if defined(SSE_GUI_KBD)
  case CUT_CONTEXT_MENU: //Context menu (Disk manager)
  {
    HWND h=GetFocus();
    HWND p=GetParent(h);
    if(p==GetDlgItem(DiskMan.Handle,98)||p==GetDlgItem(DiskMan.Handle,99))
      PostMessage(p,WM_CONTEXTMENU,0,0); // to Handle of drive icon
    else
      PostMessage(DiskMan.Handle,WM_CONTEXTMENU,(WPARAM)h,0);
    break;
  }
#endif
#if defined(SSE_DISK_SWAPPER)
  case CUT_DISK_SWAPPER_PREVIOUS:
    DiskMan.ChangeDisk(DRIVE_A,-1,TRUE);
    break;
  case CUT_DISK_SWAPPER_NEXT:
    DiskMan.ChangeDisk(DRIVE_A,1,TRUE);
    break;
#endif
#ifdef WIN32
  case CUT_DBL_CLICK: // simulate double click, works only with separate emu thread
    if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
    {
      mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);
      Sleep(20);
      mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,0);
      Sleep(20);
      mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);
      Sleep(20);
      mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,0);
    }
    break;
#if defined(SSE_WINDOWS_2000_MIN) && !defined(MINGW_BUILD)
  case CUT_JOY0: case CUT_JOY1:
  { // simulate key press Num Lock or Scroll Lock (for keyboards without)
    INPUT input[2];
    ZeroMemory(&input, sizeof(input));
    input[0].type = input[1].type = INPUT_KEYBOARD;
    input[0].ki.wVk = input[1].ki.wVk = (Inf.Action==CUT_JOY0) ? VK_NUMLOCK : VK_SCROLL;
    input[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, input, sizeof(INPUT));
    break;
  }
#endif
#endif//WIN32
/*  case CUT_JOY_MINUS: // select previous joystick setup
    JoyConfig.JoySetupUpdate(true);
    if(nJoySetup>1)
      nJoySetup--;
    else
      nJoySetup=JOYSTICK_SETUPS-1; //loop
    JoyConfig.JoySetupUpdate(false);
    break;*/
  case CUT_JOY_PLUS: // select next joystick setup
    JoyConfig.JoySetupUpdate(true);
    if(nJoySetup<JOYSTICK_SETUPS-1)
      nJoySetup++;
    else
      nJoySetup=0; //loop
    JoyConfig.JoySetupUpdate(false);
#ifdef SSE_GUI_STATUS_BAR
    if(OPTION_STATUS_BAR)
    {
      char string[256];
      sprintf(string,"%s #%d",T("Joystick setup").Text,nJoySetup+1);
      strncpy(StatusInfo.text, string ,255);
      StatusInfo.MessageIndex=TStatusInfo::MESSAGE_MISC;
      SetTimer(StemWin,STATUSBAR_TIMER_ID,4000,NULL); //4s to read
      UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
    }
#endif
    break;
#ifdef DEBUG_BUILD
  case 200: // Trace Into
    if(runstate!=RUNSTATE_RUNNING||GetForegroundWindow()!=StemWin)
      PostMessage(DWin,WM_COMMAND,MAKEWPARAM(1002,BN_CLICKED),0);
    break;
  case 203: // Step Over
  case 206: // Quick Step Over
    if(runstate!=RUNSTATE_RUNNING||GetForegroundWindow()!=StemWin)
      PostMessage(DWin,WM_COMMAND,MAKEWPARAM(1003,BN_CLICKED),0);
    break;
  case 201: // Run to RTE
    if(runstate!=RUNSTATE_RUNNING||GetForegroundWindow()!=StemWin)
      PostMessage(DWin,WM_COMMAND,MAKEWPARAM(1011,BN_CLICKED),0);
    break;
  case 202: // Suspend logging
    if(runstate!=RUNSTATE_RUNNING||GetForegroundWindow()!=StemWin)
      PostMessage(DWin,WM_COMMAND,1012,0);
    break;
  case 204: // Run to/for
    if(runstate!=RUNSTATE_RUNNING||GetForegroundWindow()!=StemWin)
      PostMessage(DWin,WM_COMMAND,MAKEWPARAM(1022,BN_CLICKED),0);
    break;
  case 205: // Run to RTS
    if(runstate!=RUNSTATE_RUNNING||GetForegroundWindow()!=StemWin)
      PostMessage(DWin,WM_COMMAND,MAKEWPARAM(1015,BN_CLICKED),0);
    break;
#if defined(SSE_DEBUGGER_TOGGLE)
  case 207: // Toggle Debugger
    ShowWindow(DWin,DebuggerVisible?SW_HIDE:SW_SHOW);
    break;
#endif
#endif
  }
}


void DoShortcutUp(TShortcutInfo &Inf) {
  switch(Inf.Action) {
  case 0:
    if(NOT_BLANK_DIRID(Inf.PressKey)&&runstate==RUNSTATE_RUNNING && GetForegroundWindow()==StemWin)
    {
      int Extend=IGNORE_EXTEND;
      if(HIBYTE(Inf.PressKey)==1)
        Extend=1;
      if(Inf.PressKey==VK_SHIFT)
      {
        HandleKeyPress(VK_LSHIFT,true,Extend);
        HandleKeyPress(VK_RSHIFT,true,Extend);
      }
      else
        HandleKeyPress(Inf.PressKey,true,Extend|NO_SHIFT_SWITCH);
#ifdef WIN32
      int n=0;
      while(TaskSwitchVKList[n])
      {
        if(Inf.PressKey==TaskSwitchVKList[n])
        {
          CutTaskSwitchVKDown[n]=0;
          break;
        }
        n++;
      }
#endif
    }
    break;
  case 6:
    if(!flashlight_flag) 
      fast_forward_change(false,false);
    break;
  case 18:
    CutButtonDown[0]=0;
    break;
  case 19:
    CutButtonDown[1]=0;
    break;
  case 28:
    if(flashlight_flag) 
      fast_forward_change(false,false);
    break;
  case 33: 
    if(cut_slow_motion_speed==0) 
      slow_motion_change(0); 
    break;
  case 35:
    if(cut_slow_motion_speed==100)
    {
      slow_motion_change(0);
      cut_slow_motion_speed=0;
    }
    break;
  case 36:
    if(cut_slow_motion_speed==250)
    {
      slow_motion_change(0);
      cut_slow_motion_speed=0;
    }
    break;
  case 37:
    if(cut_slow_motion_speed==500)
    {
      slow_motion_change(0);
      cut_slow_motion_speed=0;
    }
    break;
  case 46:
    if(macro_record) 
      macro_end(MACRO_ENDRECORD);
    break;
  }
}


void TShortcutBox::UpdateDisableIfDownLists() {
  for(int cuts=0;cuts<2;cuts++)
  {
    if(CurrentCutSelType!=2) 
      cuts++;
    TShortcutInfo *pCuts=(TShortcutInfo*)((cuts==0)?&(CurrentCuts[0]):&(Cuts[0]));
    int NumItems=int((cuts==0)?CurrentCuts.NumItems:Cuts.NumItems);
    for(int n=0;n<NumItems;n++)
    {
      for(int i=0;i<5;i++) 
        pCuts[n].DisableIfCutDownList[i]=NULL;
      int Num0s=0;
      for(int i=0;i<3;i++)
      {
        if(BLANK_DIRID(pCuts[n].Id[i])) 
          Num0s++;
      }
      if(Num0s)
      {
        for(int cuts2=0;cuts2<2;cuts2++)
        {
          if(CurrentCutSelType!=2) 
            cuts2++;
          TShortcutInfo *pCuts2=(TShortcutInfo*)((cuts2==0)?&(CurrentCuts[0]):&(Cuts[0]));
          int NumItems2=int((cuts2==0)?CurrentCuts.NumItems:Cuts.NumItems);
          for(int c=0;c<NumItems2;c++)
          {
            if(pCuts2!=pCuts||c!=n)
            {
              int CutNum0s=0;
              for(int i=0;i<3;i++)
              {
                if(BLANK_DIRID(pCuts2[c].Id[i])) 
                  CutNum0s++;
              }
              if(Num0s>CutNum0s)
              {
                int NumSame=0;
                for(int tn_i=0;tn_i<3;tn_i++)
                {
                  if(NOT_BLANK_DIRID(pCuts[n].Id[tn_i]))
                  {
                    for(int tc_i=0;tc_i<3;tc_i++)
                    {
                      if(pCuts[n].Id[tn_i]==pCuts2[c].Id[tc_i]) 
                        NumSame++;
                    }
                  }
                }
                if(NumSame+Num0s>=3)
                {
                  for(int i=0;i<5;i++)
                  {
                    if(pCuts[n].DisableIfCutDownList[i]==NULL)
                    {
                      pCuts[n].DisableIfCutDownList[i]=&pCuts2[c];
                      break;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}


void TShortcutBox::ChangeCutFile(Str NewSel,int Type,bool SaveOld) {

#ifdef WIN32
  if(CurrentCutSelType && SaveOld && CurrentCutSel.NotEmpty())
  {
    {
      SaveShortcutInfo(CurrentCuts,CurrentCutSel);
    }
  }
  ShowWindow(Scroller.GetControlPage(),SW_HIDE);
  // Delete current cut controls
  DynamicArray<HWND> ChildList;
  HWND FirstChild=GetWindow(Scroller.GetControlPage(),GW_CHILD);
  HWND Child=FirstChild;
  while(Child)
  {
    if(GetDlgCtrlID(Child)>=1000) 
      ChildList.Add(Child);
    Child=GetWindow(Child,GW_HWNDNEXT);
    if(Child==FirstChild) 
      break;
  }
  for(int i=0;i<ChildList.NumItems;i++) 
    DestroyWindow(ChildList[i]);
  CurrentCutSel=NewSel;
  CurrentCutSelType=Type;
  LoadAllCuts();
  for(int i=0;i<CurrentCuts.NumItems;i++) 
    AddPickerLine(i);
  UpdateAddButsPosition();
  SetWindowAndChildrensFont(Scroller.GetControlPage(),Font);
  Scroller.AutoSize();
  EnableWindow(GetDlgItem(Scroller.GetControlPage(),60),(Type>0));
  EnableWindow(GetDlgItem(Scroller.GetControlPage(),61),(Type>0));
  ShowWindow(Scroller.GetControlPage(),SW_SHOW);
#endif//WIN32

#ifdef UNIX
  if (CurrentCutSel.NotEmpty() && CurrentCutSelType>0 && SaveOld){
    SaveShortcutInfo(CurrentCuts,CurrentCutSel);
  }

  hxc::destroy_children_of(sa.handle);

  CurrentCutSel=NewSel;

  CurrentCutSelType=Type;

  LoadCutsAndCreateCutControls();
#endif//UNIX
  Dirty=false;
}



#ifdef WIN32

void TShortcutBox::ManageWindowClasses(bool Unreg) {
  char *ClassName="Steem Shortcuts";
  if(Unreg)
    UnregisterClass(ClassName,hInstance);
  else
    RegisterMainClass(WndProc,ClassName,RC_ICO_SHORTCUT);
}


void TShortcutBox::SetMacroFileButtonText(HWND But,int p) {
  if(CurrentCuts[p].MacroFileIdx>=0)
  {
    Str Text=CurrentCutsStrings[CurrentCuts[p].MacroFileIdx].String;
    Str Name=GetFileNameFromPath(Text);
    char *dot=strrchr(Name,'.');
    if(dot) 
      *dot='\0';
    SendMessage(But,WM_SETTEXT,0,LPARAM(Name.Text));
  }
  else
    SendMessage(But,WM_SETTEXT,0,LPARAM(T("Choose").Text));
}


bool TShortcutBox::HasHandledMessage(MSG *mess) {
  if(mess->message==WM_KEYDOWN)
  {
    if(mess->wParam==VK_TAB)
    {
      if(GetKeyState(VK_CONTROL)>=0) 
        return !!IsDialogMessage(Handle,mess);
    }
#if defined(SSE_GUI_KBD)
    else if(mess->wParam==VK_DOWN||mess->wParam==VK_UP)
    {
      HWND a=GetFocus();
      HMENU b=GetMenu(a);
      INT_PTR c=((((INT_PTR)b-5-1000)%100)==0);
      if(c)
      {
        // a bit slow but at least it works
        WORD wScrollNotify=(mess->wParam==VK_DOWN)?SB_LINEDOWN:SB_LINEUP;
        SendMessage(Scroller,WM_VSCROLL,MAKELONG(wScrollNotify,0),0L);
      }
    }
#endif
  }
  return false;
}


Str TShortcutBox::ShowChooseMacroBox(Str CurrentMacro) {
  EnableAllWindows(false,Handle);
  PopupOpen=true;
  WNDCLASS wc={0,ChooseMacroWndProc,0,0,hInstance,NULL,PCArrow,HBRUSH(
    COLOR_BTNFACE+1),NULL,"Steem Shortcuts Choose Macro Dialog"};
  RegisterClass(&wc);
  HWND Win=CreateWindowEx(WS_EX_CONTROLPARENT|int(FullScreen?WS_EX_TOPMOST:0),
    "Steem Shortcuts Choose Macro Dialog",T("Choose a Macro"),WS_CAPTION,
    100,100,326,356+GuiSM.cy_caption(),Handle,NULL,hInstance,NULL);
  if(Win==NULL||!IsWindow(Win)) 
    return "";
  SetWindowLongPtr(Win,GWLP_USERDATA,(LONG_PTR)this);
  CreateWindow("Button",T("OK"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,
    100,320,100,23,Win,(HMENU)IDOK,hInstance,NULL);
  CreateWindow("Button",T("Cancel"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON
    ,210,320,100,23,Win,(HMENU)IDCANCEL,hInstance,NULL);
  DirectoryTree Tree;
  pChooseMacroTree=&Tree;
  Tree.FileMasksESL.DeleteAll();
  Tree.FileMasksESL.Add("",0,RC_ICO_PCFOLDER);
  Tree.FileMasksESL.Add("stmac",0,RC_ICO_OPS_MACROS);
  UpdateDirectoryTreeIcons(&Tree);
  Tree.Create(Win,10,10,300,300,(HMENU)100,WS_VISIBLE|WS_TABSTOP,
    ChooseMacroTreeNotifyProc,this, OptionBox.MacroDir,T("Macros"),true);
  Tree.SelectItemByPath(CurrentMacro);
  SetWindowAndChildrensFont(Win,Font);
  CentreWindow(Win,false);
  PopupFocus=Tree.hTree;
  ShowWindow(Win,SW_SHOW);
  EnableWindow(Handle,FALSE);
  MSG mess;
  while(GetMessage(&mess,NULL,0,0))
  {
    if(!IsDialogMessage(Win,&mess))
    {
      TranslateMessage(&mess);
      DispatchMessage(&mess);
    }
    if(!PopupOpen)
      break;
  }
  if(mess.message==WM_QUIT)
  {
    QuitSteem();
    return "";
  }
  EnableWindow(Handle,TRUE);
  SetForegroundWindow(Handle);
  EnableAllWindows(true,Handle);
  pChooseMacroTree=NULL;
  DestroyWindow(Win);
  UnregisterClass(wc.lpszClassName,hInstance);
  Str Temp=ChooseMacroSel;
  ChooseMacroSel="";
  return Temp;
}


LRESULT CALLBACK TShortcutBox::WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar) {
  LRESULT Ret=DefStemDialogProc(Win,Mess,wPar,lPar);
  if(StemDialog_RetDefVal) 
    return Ret;
  if(DTree.ProcessMessage(Mess,wPar,lPar)) 
    return DTree.WndProcRet;
  TShortcutBox *This=(TShortcutBox*)GetWindowLongPtr(Win,GWLP_USERDATA);
  WORD wpar_lo=LOWORD(wPar);
  switch(Mess) {
  case WM_COMMAND:
  {
    switch(wpar_lo) {
    case 60:case 61:
    {
      TShortcutInfo si;
      if(wpar_lo==61&&CurrentCuts.NumItems)
      { // Copy
        si=CurrentCuts[CurrentCuts.NumItems-1];
        if(si.MacroFileIdx>=0)
        {
          Str MacroFile=CurrentCutsStrings[si.MacroFileIdx].String;
          si.MacroFileIdx=CurrentCutsStrings.Add(MacroFile);
        }
      }
      else
        ClearSHORTCUTINFO(&si);
      si.pESL=&CurrentCutsStrings;
      CurrentCuts.Add(si);
      This->AddPickerLine(CurrentCuts.NumItems-1);
      SetWindowAndChildrensFont(This->Scroller.GetControlPage(),This->Font);
      This->UpdateAddButsPosition();
      This->Scroller.AutoSize();
      This->Scroller.SetVPos(32000);
      This->UpdateDisableIfDownLists();
      break;
    }
    case 70:
      if(HIWORD(wPar)!=BN_CLICKED) 
        break;
      DTree.NewItem(T("New Shortcuts"),DTree.RootItem,1);
      break;
    case 71:
    {
      if(HIWORD(wPar)!=BN_CLICKED) 
        break;
      SendMessage((HWND)lPar,BM_SETCHECK,1,0);
      EnableAllWindows(false,Win);
      EasyStr NewFol=ChooseFolder((FullScreen) ? StemWin : Win,
        T("Pick a Folder"),This->CutDir);
      if(NewFol.NotEmpty()&&NotSameStr_I(NewFol,This->CutDir))
      {
        NO_SLASH(NewFol);
        This->CutDir=NewFol;
        CutFiles.DeleteAll();
        This->LoadAllCuts();
        DTree.RootFol=NewFol;
        DTree.RefreshDirectory();
      }
      SetForegroundWindow(Win);
      EnableAllWindows(true,Win);
      SetFocus((HWND)lPar);
      SendMessage((HWND)lPar,BM_SETCHECK,0,0);
      break;
    }//case
    }//sw
    if(wpar_lo>=1000&&wpar_lo<40000)
    {
      This->Dirty=true;
      int Base=1000;
      int Num=(wpar_lo-Base)/100;
      int ID=wpar_lo%100;
      HWND Par=This->Scroller.GetControlPage();
      switch(ID) {
      case 0:case 1:case 2: // Buttons picked
      {
        CurrentCuts[Num].Id[ID]=GetWindowWord((HWND)lPar,0);
        CurrentCuts[Num].Down=2;
        This->UpdateDisableIfDownLists();
        break;
      }
      case 3:
        if(HIWORD(wPar)==CBN_SELENDOK)
        {
          int OldAction=CurrentCuts[Num].Action;
          CurrentCuts[Num].Action=(BYTE)SendMessage((HWND)lPar,CB_GETITEMDATA,
            SendMessage((HWND)lPar,CB_GETCURSEL,0,0),0);
          if(CurrentCuts[Num].Action!=OldAction)
          {
            ShowWindow(GetDlgItem(Par,Base+Num*100+ID_PRESSKEY),SW_HIDE);
            ShowWindow(GetDlgItem(Par,Base+Num*100+ID_PRESSCHAR),SW_HIDE);
            ShowWindow(GetDlgItem(Par,Base+Num*100+ID_PLAYMACRO),SW_HIDE);
            switch(CurrentCuts[Num].Action) {
            case CUT_PRESSKEY:
              SetWindowPos(GetDlgItem(Par,wpar_lo),0,0,0,210,300,SWP_NOZORDER|SWP_NOMOVE);
              ShowWindow(GetDlgItem(Par,Base+Num*100+ID_PRESSKEY),SW_SHOW);
              break;
            case CUT_PRESSCHAR:
              SetWindowPos(GetDlgItem(Par,wpar_lo),0,0,0,210,300,SWP_NOZORDER|SWP_NOMOVE);
              ShowWindow(GetDlgItem(Par,Base+Num*100+ID_PRESSCHAR),SW_SHOW);
              break;
            case CUT_PLAYMACRO:
              SetWindowPos(GetDlgItem(Par,wpar_lo),0,0,0,210,300,SWP_NOZORDER|SWP_NOMOVE);
              ShowWindow(GetDlgItem(Par,Base+Num*100+ID_PLAYMACRO),SW_SHOW);
              break;
            default:
              SetWindowPos(GetDlgItem(Par,wpar_lo),0,0,0,270,300,SWP_NOZORDER|SWP_NOMOVE);
            }
          }
        }
        break;
      case ID_PRESSKEY:
        CurrentCuts[Num].PressKey=(WORD)GetWindowWord((HWND)lPar,0);
        CurrentCuts[Num].Down=2;
        break;
      case 5:           // Del
        if(HIWORD(wPar)==BN_CLICKED)
        {
          for(int n=Num;n<CurrentCuts.NumItems-1;n++)
          {
            CurrentCuts[n].Id[0]=CurrentCuts[n+1].Id[0];
            SetWindowWord(GetDlgItem(Par,Base+n*100),0,CurrentCuts[n].Id[0]);
            CurrentCuts[n].Id[1]=CurrentCuts[n+1].Id[1];
            SetWindowWord(GetDlgItem(Par,Base+n*100+1),0,CurrentCuts[n].Id[1]);
            CurrentCuts[n].Id[2]=CurrentCuts[n+1].Id[2];
            SetWindowWord(GetDlgItem(Par,Base+n*100+2),0,CurrentCuts[n].Id[2]);
            CurrentCuts[n].Action=CurrentCuts[n+1].Action;
            CBSelectItemWithData(GetDlgItem(Par,Base+n*100+3),CurrentCuts[n].Action);
            bool PressKey=(CurrentCuts[n].Action==CUT_PRESSKEY);
            bool PressChar=(CurrentCuts[n].Action==CUT_PRESSCHAR);
            bool PlayMacro=(CurrentCuts[n].Action==CUT_PLAYMACRO);
            SetWindowPos(GetDlgItem(Par,Base+n*100+3),NULL,0,0,int((PressKey||
              PressChar||PlayMacro)?210:270),300,SWP_NOZORDER|SWP_NOMOVE);
            CurrentCuts[n].PressKey=CurrentCuts[n+1].PressKey;
            SetWindowWord(GetDlgItem(Par,Base+n*100+ID_PRESSKEY),0,(WORD)CurrentCuts[n].PressKey);
            ShowWindow(GetDlgItem(Par,Base+n*100+ID_PRESSKEY),(PressKey) ? SW_SHOW : SW_HIDE);
            CurrentCuts[n].PressChar=CurrentCuts[n+1].PressChar;
            ShowWindow(GetDlgItem(Par,Base+n*100+ID_PRESSCHAR),(PressChar) ? SW_SHOW : SW_HIDE);
            SendMessage(GetDlgItem(Par,Base+n*100+ID_PRESSCHAR),CB_SETCURSEL,0,
              CurrentCuts[n+1].PressChar);
            CurrentCuts[n].MacroFileIdx=CurrentCuts[n+1].MacroFileIdx;
            ShowWindow(GetDlgItem(Par,Base+n*100+ID_PLAYMACRO),(PlayMacro) ? SW_SHOW : SW_HIDE);
            This->SetMacroFileButtonText(GetDlgItem(Par,Base+n*100+ID_PLAYMACRO),n);
            for(int c=Base+n*100;c<=(Base+n*100+HIGHEST_CUT_CONTROL_ID);c++)
            {
              if(GetDlgItem(Par,c)) 
                InvalidateRect(GetDlgItem(Par,c),NULL,FALSE);
            }
            CurrentCuts[n].Down=CurrentCuts[n+1].Down;
          }
          CurrentCuts.NumItems--;
          for(int c=Base+CurrentCuts.NumItems*100;
            c<=(Base+CurrentCuts.NumItems*100+HIGHEST_CUT_CONTROL_ID);c++)
          {
            if(GetDlgItem(Par,c)) 
              DestroyWindow(GetDlgItem(Par,c));
          }
          This->UpdateAddButsPosition();
          This->Scroller.AutoSize();
          This->UpdateDisableIfDownLists();
        }
        break;
      case ID_PRESSCHAR:
        if(HIWORD(wPar)==CBN_SELENDOK)
        {
          CurrentCuts[Num].PressChar=(DWORD)SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
          CurrentCuts[Num].Down=2;
        }
        break;
      case ID_PLAYMACRO: // Macro choose button
      {
        SendMessage((HWND)lPar,BM_SETCHECK,1,0);
        int StrIdx=CurrentCuts[Num].MacroFileIdx;
        Str CurFile;
        if(StrIdx>=0) 
          CurFile=CurrentCutsStrings[StrIdx].String;
        EasyStr NewFile=This->ShowChooseMacroBox(CurFile);
        SetFocus((HWND)lPar);
        if(NewFile.NotEmpty())
        {
          if(StrIdx>=0)
            CurrentCutsStrings.SetString(StrIdx,NewFile);
          else
            CurrentCuts[Num].MacroFileIdx=CurrentCutsStrings.Add(NewFile);
          This->SetMacroFileButtonText((HWND)lPar,Num);
        }
        SendMessage((HWND)lPar,BM_SETCHECK,0,0);
        break;
      }//case
      }//sw
    }
    break;
  }
  case (WM_USER+1011):
  {
    HWND NewParent=(HWND)lPar;
    if(NewParent)
    {
      This->CheckFSPosition(NewParent);
      SetWindowPos(Win,NULL,This->FSLeft,This->FSTop,0,0,SWP_NOZORDER|SWP_NOSIZE);
    }
    else
      SetWindowPos(Win,NULL,This->Left,This->Top,0,0,SWP_NOZORDER|SWP_NOSIZE);
    This->ChangeParent(NewParent);
    break;
  }
  case WM_CLOSE:
    This->Hide();
    return 0;
  case DM_GETDEFID:
    return 0;
  }
  return DefWindowProc(Win,Mess,wPar,lPar);
}


int TShortcutBox::DTreeNotifyProc(DirectoryTree *pTree,void *t,int Mess,INT_PTR i1,INT_PTR i2) {
  TShortcutBox *This=(TShortcutBox*)t;
  if(Mess==DTM_GETTYPE)
  {
    if(i2==1||i2==2)
    {
      if(CutFiles.FindString_I((char*)i1)>=0)
      {
        return 2;
      }
      return 1;
    }
    return 0;
  }
  else if(Mess==DTM_FOLDERMOVED||Mess==DTM_ITEMDELETED)
  {
    Str From=Str((char*)i1).UpperCase();
    for(int i=0;i<CutFiles.NumStrings;i++)
    {
      if(strstr(Str(CutFiles[i].String).UpperCase(),From))
      {
        if(i2)
        {
          Str NewPath=CutFiles[i].String;
          NewPath.Delete(0,(int)strlen(From));
          NewPath.Insert((char*)i2,0);
          CutFiles.SetString(i,NewPath);
        }
        else
          CutFiles.Delete(i--);
      }
    }
    return 0;
  }
  if(Mess!=DTM_SELCHANGED && Mess!=DTM_NAMECHANGED && Mess!=DTM_TYPECHANGED)
  {
    return 0;
  }
  Str NewSel=pTree->GetItemPath((HTREEITEM)i1);
  int Type=pTree->GetItem((HTREEITEM)i1,TVIF_IMAGE).iImage;
  if(Mess==DTM_SELCHANGED)
    This->ChangeCutFile(NewSel,Type,i2!=0);
  else if(Mess==DTM_NAMECHANGED)
  {
    if(This->CurrentCutSelType==2)
    {
      for(int i=0;i<CutFiles.NumStrings;i++)
      {
        if(IsSameStr_I(CutFiles[i].String,This->CurrentCutSel)) 
          CutFiles.Delete(i--);
      }
      CutFiles.Add(NewSel);
    }
    This->CurrentCutSel=NewSel;
  }
  else if(Mess==DTM_TYPECHANGED)
  {
    for(int i=0;i<CutFiles.NumStrings;i++)
    {
      if(IsSameStr_I(CutFiles[i].String,NewSel)) 
        CutFiles.Delete(i--);
    }
    if(Type==2)
    {
      CutFiles.Add(NewSel);
    }
    if(IsSameStr_I(NewSel,This->CurrentCutSel)) 
      This->CurrentCutSelType=Type;
    This->LoadAllCuts(0);
  }
  return 0;
}


LRESULT CALLBACK TShortcutBox::ChooseMacroWndProc(HWND Win,UINT Mess,
                                                  WPARAM wPar,LPARAM lPar) {
  TShortcutBox *This=(TShortcutBox*)GetWindowLongPtr(Win,GWLP_USERDATA);
  if(pChooseMacroTree)
  {
    if(pChooseMacroTree->ProcessMessage(Mess,wPar,lPar)) 
      return pChooseMacroTree->WndProcRet;
  }
  switch(Mess) {
  case WM_COMMAND:
    switch(LOWORD(wPar)) {
    case IDCANCEL:
      This->ChooseMacroSel="";
    case IDOK:
      This->PopupOpen=0;
      return 0;
    }
    break;
#if !defined(SSE_VID_2SCREENS)
  case WM_MOVING:case WM_SIZING:
    if(FullScreen)
    {
      RECT *rc=(RECT*)lPar;
      if(rc->top<MENUHEIGHT)
      {
        if(Mess==WM_MOVING) 
          rc->bottom+=MENUHEIGHT-rc->top;
        rc->top=MENUHEIGHT;
        return true;
      }
      RECT LimRC={0,MENUHEIGHT+GuiSM.cy_frame(),GuiSM.cx_screen(),
        GuiSM.cy_screen()};
      ClipCursor(&LimRC);
    }
    break;
  case WM_CAPTURECHANGED:   //Finished
    if(FullScreen) 
      ClipCursor(NULL);
    break;
#endif
  case WM_ACTIVATE:
    if(wPar==WA_INACTIVE) 
      This->PopupFocus=GetFocus();
    break;
  case WM_SETFOCUS:
    SetFocus(This->PopupFocus);
    break;
  case DM_GETDEFID:
    return MAKELONG(IDOK,DC_HASDEFID);
  }
  return DefWindowProc(Win,Mess,wPar,lPar);
}


int TShortcutBox::ChooseMacroTreeNotifyProc(DirectoryTree *pTree,void *t,
                                            int Mess,INT_PTR i1,INT_PTR) {
  TShortcutBox *This=(TShortcutBox*)t;
  if(Mess==DTM_SELCHANGED||Mess==DTM_NAMECHANGED)
  {
    Str NewSel=pTree->GetItemPath((HTREEITEM)i1);
    if(pTree->GetItem((HTREEITEM)i1,TVIF_IMAGE).iImage==1)
    {
      This->ChooseMacroSel=NewSel;
      EnableWindow(GetDlgItem(GetParent(pTree->hTree),IDOK),TRUE);
    }
    else
    {
      This->ChooseMacroSel="";
      EnableWindow(GetDlgItem(GetParent(pTree->hTree),IDOK),FALSE);
    }
  }
  return 0;
}

#endif//WIN32


#ifdef UNIX

void TShortcutBox::SetMacroFileButtonText(hxc_button *p_but,int p)
{
  if (CurrentCuts[p].MacroFileIdx>=0){
    Str Text=CurrentCutsStrings[CurrentCuts[p].MacroFileIdx].String;
    Str Name=GetFileNameFromPath(Text);
    char *dot=strrchr(Name,'.');
    if (dot) *dot='\0';
    p_but->set_text(Name);
  }else{
    p_but->set_text(T("Choose"));
  }
}
//---------------------------------------------------------------------------
PICKERLINE TShortcutBox::GetLine(int p)
{
	PICKERLINE pl;
	int Base=p*100;
	for (int n=0;n<3;n++){
		pl.p_sign[n]=(hxc_button*)hxc::find(sa.handle,Base+90+n);
		pl.p_id[n]=(hxc_buttonpicker*)hxc::find(sa.handle,Base+n);
	}
	pl.p_action=(hxc_dropdown*)hxc::find(sa.handle,Base+3);
	pl.p_stchar=(hxc_dropdown*)hxc::find(sa.handle,Base+8);
	pl.p_macro=(hxc_button*)hxc::find(sa.handle,Base+9);
	pl.p_del=(hxc_button*)hxc::find(sa.handle,Base+5);
	pl.p_stkey=(hxc_buttonpicker*)hxc::find(sa.handle,Base+4);
	return pl;
}

//---------------------------------------------------------------------------
void TShortcutBox::ShowHidePressSTKeyPicker(int p)
{
  PICKERLINE pl=GetLine(p);
  if (pl.p_stkey==NULL || pl.p_stchar==NULL || pl.p_action==NULL) return;

	XUnmapWindow(XD,pl.p_stkey->handle);
	XUnmapWindow(XD,pl.p_stchar->handle);
	XUnmapWindow(XD,pl.p_macro->handle);

	int w=235;
	if (CurrentCuts[p].Action==CUT_PRESSKEY) w=160;
	if (CurrentCuts[p].Action==CUT_PRESSCHAR) w=160;
	if (CurrentCuts[p].Action==CUT_PLAYMACRO) w=160;
	XResizeWindow(XD,pl.p_action->handle,w,pl.p_action->h);

	if (CurrentCuts[p].Action==CUT_PRESSKEY){
		XMapWindow(XD,pl.p_stkey->handle);
	}else if (CurrentCuts[p].Action==CUT_PRESSCHAR){
		XMapWindow(XD,pl.p_stchar->handle);
	}else if (CurrentCuts[p].Action==CUT_PLAYMACRO){
		XMapWindow(XD,pl.p_macro->handle);
	}
}

//---------------------------------------------------------------------------
int TShortcutBox::WinProc(TShortcutBox *This,Window Win,XEvent*Ev)
{
  switch (Ev->type){
    case ClientMessage:
      if (Ev->xclient.message_type==hxc::XA_WM_PROTOCOLS){
        if (Atom(Ev->xclient.data.l[0])==hxc::XA_WM_DELETE_WINDOW){
          This->Hide();
        }
      }
      break;
  }
  return PEEKED_MESSAGE;
}
//---------------------------------------------------------------------------
int TShortcutBox::picker_notify_proc(hxc_buttonpicker *bp,int mess,INT_PTR i)
{
	TShortcutBox *This=(TShortcutBox*)bp->owner;
  if (mess==BPN_CHANGE){
    int p=bp->id / 100;
    int idn=bp->id % 100;
		if (idn<3){
			CurrentCuts[p].Id[idn]=i;
		}else{
			CurrentCuts[p].PressKey=i;
		}
    CurrentCuts[p].Down=2;
    This->UpdateDisableIfDownLists();
  }else if (mess==BPN_FOCUSCHANGE){
  	if (i==FocusOut){
  		This->help_td.set_text("");
  	}else if (i==FocusIn){
      EasyStr Message;
  		if (bp->st_keys_only==0){
        if (NumJoysticks){
          Message=T("Press any key, the middle mouse button or a joystick button/direction.")+"\n\n";
        }else{
          Message=T("Press any key or the middle mouse button.")+"\n\n";
        }
      }else{
        Message=T("Press a key that was on the ST keyboard or F11, F12, Page Up or Page Down.")+"\n\n";
      }
      Message+=T("Press the pause/break key to clear your selection.");
	 		This->help_td.set_text(Message);
  	}
		This->help_td.draw();
  }
  return 0;
}
//---------------------------------------------------------------------------
void TShortcutBox::DeleteCut(int p)
{
  Dirty=true;
  for (int n=p;n<CurrentCuts.NumItems-1;n++){
  	PICKERLINE pl=GetLine(n);
    for (int i=0;i<3;i++){
      CurrentCuts[n].Id[i]=CurrentCuts[n+1].Id[i];
      pl.p_id[i]->DirID=CurrentCuts[n].Id[i];
    }
    CurrentCuts[n].Action=CurrentCuts[n+1].Action;
    pl.p_action->select_item_by_data(CurrentCuts[n].Action,0);

    CurrentCuts[n].PressKey=CurrentCuts[n+1].PressKey;
    pl.p_stkey->DirID=CurrentCuts[n].PressKey;

    CurrentCuts[n].PressChar=CurrentCuts[n+1].PressChar;
	 	pl.p_stchar->select_item_by_data(CurrentCuts[n].PressChar,1);

    CurrentCuts[n].MacroFileIdx=CurrentCuts[n+1].MacroFileIdx;
    SetMacroFileButtonText(pl.p_macro,n);

    ShowHidePressSTKeyPicker(n);

    pl.p_action->draw();
    for (int i=0;i<3;i++) pl.p_id[i]->draw(true);
    pl.p_stkey->draw(true);

    CurrentCuts[n].Down=CurrentCuts[n+1].Down;
  }
  CurrentCuts.NumItems--;
 	PICKERLINE pl=GetLine(CurrentCuts.NumItems);
	for (int i=0;i<3;i++){
		hxc::destroy(pl.p_id[i]);
		hxc::destroy(pl.p_sign[i]);
	}
  hxc::destroy(pl.p_action);
  hxc::destroy(pl.p_stkey);
	hxc::destroy(pl.p_stchar);
	hxc::destroy(pl.p_macro);
	hxc::destroy(pl.p_del);

  UpdateAddButsPosition();
  int old_sy=sa.sy;
  sa.adjust();
  sa.scrollto(0,old_sy);
  UpdateDisableIfDownLists();
}
//---------------------------------------------------------------------------
int TShortcutBox::button_notify_proc(hxc_button *But,int Mess,int *Inf)
{
	TShortcutBox *This=(TShortcutBox*)But->owner;

  if (Mess==BN_CLICKED){
    if (But->id==9998 || But->id==9999){
  		if (This->CurrentCutSelType<=0) return 0;
  			
  		TShortcutInfo si;
      if ((But->id & 1) && CurrentCuts.NumItems){ // Copy
        si=CurrentCuts[CurrentCuts.NumItems-1];
        if (si.MacroFileIdx>=0){
          Str MacroFile=CurrentCutsStrings[si.MacroFileIdx].String;
          si.MacroFileIdx=CurrentCutsStrings.Add(MacroFile);
        }
      }else{
        ClearSHORTCUTINFO(&si);
      }
      si.pESL=&CurrentCutsStrings;
      CurrentCuts.Add(si);

      This->AddPickerLine(CurrentCuts.NumItems-1);
      This->UpdateAddButsPosition();
      This->sa.adjust();
      This->sa.scrollto(0,CurrentCuts.NumItems*30+25-This->sa.h);
      This->UpdateDisableIfDownLists();
    }else if (But->id<20000){
      if ((But->id % 100)==5){
        This->DeleteCut(But->id/100);
      }else if ((But->id % 100)==9){
        But->set_check(true);

        int Num=But->id/100;
        int StrIdx=CurrentCuts[Num].MacroFileIdx;
        Str CurFile;
        if (StrIdx>=0) CurFile=CurrentCutsStrings[StrIdx].String;

        Str NewFile=This->ChooseMacro(CurFile);
        SetForegroundWindow(This->Handle);
        if (NewFile.NotEmpty()){
          if (StrIdx>=0){
            CurrentCutsStrings.SetString(StrIdx,NewFile);
          }else{
            CurrentCuts[Num].MacroFileIdx=CurrentCutsStrings.Add(NewFile);
          }
          This->SetMacroFileButtonText(But,Num);
        }
        But->set_check(0);
      }
    }else if (But->id==20000){
      hxc_prompt prompt;
      EasyStr new_name=prompt.ask(XD,T("New Shortcuts"),T("Enter Name"));
      if (new_name.NotEmpty()){
        EasyStr new_path=GetUniquePath(This->dir_lv.fol,new_name+".stcut");
        FILE *fp=fopen(new_path,"wb");
        if (fp){
          fclose(fp);
          This->dir_lv.refresh_fol();
      		This->dir_lv.select_item_by_name(GetFileNameFromPath(new_path));
          This->ChangeCutFile(new_path,1,true);
        }
      }
    }else if (But->id==20001){
      fileselect.set_corner_icon(&Ico16,ICO16_FOLDER);
      EasyStr new_path=fileselect.choose(XD,This->CutDir,"",T("Pick a Folder"),
        FSM_CHOOSE_FOLDER | FSM_CONFIRMCREATE,folder_parse_routine,"");
      if (new_path.NotEmpty()){
        NO_SLASH(new_path);
        This->CutDir=new_path;
        CutFiles.DeleteAll();
        This->LoadAllCuts();

        This->dir_lv.base_fol=This->CutDir;
        This->dir_lv.fol=This->CutDir;
        This->dir_lv.refresh_fol();
      }
    }
  }
	return 0;
}
//---------------------------------------------------------------------------
int TShortcutBox::dd_notify_proc(hxc_dropdown *DD,int Mess,INT_PTR Inf)
{
	TShortcutBox *This=(TShortcutBox*)DD->owner;
  if (DD->id<20000){
    int p=DD->id / 100;
    if (Mess==DDN_DROPWHERE){
      dd_drop_position *dp=(dd_drop_position*)Inf;
      dp->parent=This->Handle;
      dp->x=(DD->x + (This->sa_border.x+1)) - This->sa.sx;
      dp->y=(DD->y + (This->sa_border.y+1)) - This->sa.sy;
      if ((DD->id % 100)==3) dp->w=SHORTCUT_ACTION_DD_WID;
      dp->h=300;
      return 1;
    }else if (Mess==DDN_SELCHANGE){
      This->Dirty=true;
      if ((DD->id % 100)==3){ //Action DD
        CurrentCuts[p].Action=DD->sl[DD->sel].Data[0];
		    This->ShowHidePressSTKeyPicker(p);
      }else if ((DD->id % 100)==8){ //PressChar
        CurrentCuts[p].PressChar=DD->sl[DD->sel].Data[1];
      }
    }
  }
	return 0;
}

void TShortcutBox::LoadCutsAndCreateCutControls()
{
  AddPickerLine(-1); // create add buts
	LoadAllCuts();
  for (int i=0;i<CurrentCuts.NumItems;i++) AddPickerLine(i);
  UpdateAddButsPosition();
	sa.adjust();
	sa.scrollto(0,0);
}


int TShortcutBox::dir_lv_notify_proc(hxc_dir_lv *lv,int Mess,INT_PTR i)
{
	TShortcutBox *This=(TShortcutBox*)(lv->owner);
	switch (Mess){
		case DLVN_SELCHANGE:
		{
      Str new_sel;
      int new_type=0;
      if (i>=0){
	      new_type=lv->sl[i].Data[DLVD_TYPE]-1;
        if (new_type==-1){ // Up folder
        	new_sel=lv->fol+"/..";
        }else{
        	new_sel=lv->get_item_path(i);
        }
	    }
	    if (new_sel==This->CurrentCutSel) break;

      This->ChangeCutFile(new_sel,new_type,true);
			break;
		}
    case DLVN_NAMECHANGED:
    {
      Str new_name=lv->get_item_path(i);
      if (This->CurrentCutSelType==2){
        for (int i=0;i<CutFiles.NumStrings;i++){
          if (IsSameStr_I(CutFiles[i].String,This->CurrentCutSel)) CutFiles.Delete(i--);
        }
        CutFiles.Add(new_name);
      }
      This->CurrentCutSel=new_name;
      break;
    }

    case DLVN_GETTYPE:
      return int((CutFiles.FindString((char*)i)>-1) ? 3:2);
    case DLVN_TYPECHANGE:
    {
     	EasyStr changed=lv->get_item_path(i,true);
      int new_type=lv->sl[i].Data[DLVD_TYPE]-1;
      for (int i=0;i<CutFiles.NumStrings;i++){
        if (IsSameStr_I(CutFiles[i].String,changed)) CutFiles.Delete(i--);
      }
      if (new_type==2) CutFiles.Add(changed);
      if (IsSameStr_I(changed,This->CurrentCutSel)) This->CurrentCutSelType=new_type;
      This->LoadAllCuts(0);
      break;
    }
    case DLVN_FOLDERMOVED:
    case DLVN_ITEMDELETED:
    {
      char *path=(char*)i;
      for (int i=0;i<CutFiles.NumStrings;i++){
        if (strstr(CutFiles[i].String,path)){
          if (Mess==DLVN_FOLDERMOVED){
            Str new_path=CutFiles[i].String;
            new_path.Delete(0,strlen(path));
            new_path.Insert(path+strlen(path)+1,0);
            CutFiles.SetString(i,new_path);
          }else{
            CutFiles.Delete(i--);
          }
        }
      }
      break;
    }
	}
  return 0;
}
//---------------------------------------------------------------------------
Str TShortcutBox::ChooseMacro(Str Current)
{
  int dlv_h=200;
  int w=300,h=10+dlv_h+10;

  Window handle=hxc::create_modal_dialog(XD,w,h,T("Choose a Macro"),true);
  if (handle==0) return "";

  int y=10;

  hxc_dir_lv dlv;
  dlv.ext_sl.Add(3,T("Parent Directory"),1,1,0);
  dlv.ext_sl.Add(3,"",ICO16_FOLDER,ICO16_FOLDERLINK,0);
  dlv.ext_sl.Add(3,"stmac",ICO16_MACROS,ICO16_MACROLINK,0);
  dlv.lpig=&Ico16;
  dlv.base_fol=OptionBox.MacroDir;
  dlv.fol=OptionBox.MacroDir;
  dlv.allow_type_change=0;
  dlv.show_broken_links=0;
  dlv.choose_only=true;
	if (Current.NotEmpty()){
		dlv.fol=Current;
		RemoveFileNameFromPath(dlv.fol,REMOVE_SLASH);
	}
  dlv.create(XD,handle,10,y,w-20,dlv_h,NULL,NULL);
  dlv.select_item_by_name(GetFileNameFromPath(Current));
  if (dlv.lv.sel<0) dlv.lv.changesel(0);
  y+=dlv_h+10;

  EasyStr ret;
  bool show=true;
  for (;;){
    int chosen=hxc::show_modal_dialog(XD,handle,show,dlv.lv.handle);
    if (chosen!=1) break;

    ret=dlv.get_item_path(dlv.lv.sel);
    if (ret.NotEmpty()) break;
    show=0;
  }

  hxc::destroy_modal_dialog(XD,handle);
  return ret;
}

#endif//UNIX

#undef LOGSECTION
