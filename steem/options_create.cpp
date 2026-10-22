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
FILE: options_create.cpp
DESCRIPTION: Functions to create the pages of the settings dialog box.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <draw.h>
#include <display.h>
#include <options.h>
#include <debug.h>
#include <computer.h>
#include <osd.h>
#include <translate.h>
#include <dataloadsave.h>
#include <stjoy.h>
#include <key_table.h>
#include <notifyinit.h>
#include <harddiskman.h>

#ifdef UNIX
#include <diskman.h>
const BYTE HorizontalSeparation=5;
const BYTE LineHeight=30,LineStart=10;
#endif

#ifdef WIN32
#define LineHeight mLineHeight
#define HorizontalSeparation GuiSM.mHorizontalSeparation
#define LineStart mLineStart
#define CharHeight GuiSM.mCharHeight
#define CbUnits    GuiSM.mCbUnits
#define SliderHeight mSliderHeight
#define GROUP_HEIGHT(n) mLineHeight*(n)+mGroupTitleHeight //lazy
#endif

void TOptionBox::CreatePage(int n) {
  
#ifdef WIN32
  //delete page
  RECT rc;
  GetClientRect(Handle,&rc);
  rc.left=page_l;
  HDC dc=GetDC(Handle);
  HBRUSH br=CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
  FillRect(dc,&rc,br);
  DeleteObject(br);
  ReleaseDC(Handle,dc);
#endif
  mWid=0;
  mOffset=page_l;
  mY=LineStart;
  bool bNoShow=false; // if we don't want ALL controls to get SW_SHOW
  switch(n) {
  case PAGE_MACHINE:    CreateMachinePage();    
    bNoShow=true;
    break;
  case PAGE_TOS:        CreateTOSPage();        break;
  case PAGE_MACROS:     CreateMacrosPage();     break;
  case PAGE_PORTS:      CreatePortsPage();
    //bNoShow=true;
    break;
  case PAGE_GENERAL:    CreateGeneralPage();    break;
  case PAGE_SOUND:      CreateSoundPage();      break;
  case PAGE_DISPLAY:    CreateDisplayPage();    break;
  case PAGE_COLOUR:     CreateBrightnessPage(); break;
  case PAGE_CONFIG:     CreateProfilesPage();   break;
  case PAGE_STARTUP:    CreateStartupPage();    break;
#ifndef SSE_NO_OSD
  case PAGE_OSD:        CreateOSDPage();        break;
#endif
#if !defined(SSE_GUI_421_REORDER)
  case PAGE_MISC:       CreateSSEPage();        break;
  case PAGE_STVIDEO:    CreateSTVideoPage();    break;
#endif
  case PAGE_INPUT:      CreateInputPage();      break;
#if defined(SSE_GEM_CONTROL_PANEL)
  case PAGE_GEM_CP:     CreateGEMControlPanel();break;
#endif
#if defined(SSE_GUI_QUICKSETTINGS)
  case PAGE_QUICK:      CreateQuickSettings();  break;
#endif
#if defined(SSE_GUI_EMUCONTROL)
  case PAGE_EMU_PARAM1: CreateParameters1();    break;
  case PAGE_EMU_PARAM2: CreateParameters2();    break;
#endif
#if defined(SSE_LOGSECTIONS_FOR_ALL)
  case PAGE_TRACE:      CreateTracePage();      break;
#endif
#ifdef WIN32
  case PAGE_MIDI:       CreateMIDIPage();       break;
  case PAGE_FULLSCREEN: CreateFullscreenPage(); break;
  case PAGE_ICONS:      CreateIconsPage();      break;
  case PAGE_ASSOC:      CreateAssocPage();      break;
#endif

#ifdef UNIX
  case PAGE_PATHS:      CreatePathsPage();      break;
#endif
#if !defined(SSE_NO_UPDATE)
  case 7:CreateUpdatePage();break;
#endif
  }
#ifdef WIN32
  Focus=PageTree;
  SetPageControlsFont();
  if(!bNoShow)
    ShowPageControls();
#endif
#ifdef UNIX
  XFlush(XD);
#endif
}


void TOptionBox::NextLine() {
  mY+=LineHeight;
  mOffset=page_l;
}


// helper
void TOptionBox::CreateRebootButton(EasyStr protip) {
#ifdef UNIX
  int &y=mY;
#endif
  if(protip.NotEmpty())
  {
#ifdef WIN32
    CreateStatic(protip);
    NextLine();
#endif
#ifdef UNIX
    mustreset_td.text=protip;
    mustreset_td.sy=0;
    mustreset_td.wordwrapped=false;
    mustreset_td.create(XD,page_p,page_l,y,page_w,45,hxc::col_white,0);
    y+=55;
#endif
  }
#ifdef WIN32  
  //CreateWindow("Button",T("Reboot now"),WS_CHILD|WS_TABSTOP|BS_CHECKBOX
  CreateWindow("Button",T("Reboot ST now"),WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE,page_l,
               mY,page_w,CharHeight,Handle,(HMENU)IDC_REBOOT,hInstance,NULL);
#endif
#ifdef UNIX
  coldreset_but.create(XD,page_p,page_l,y,page_w,25,button_notify_proc,this,
    //BT_TEXT,T("Reboot now"),1000,hxc::col_bk);
    BT_TEXT,T("Reboot ST now"),1000,hxc::col_bk);
#endif
}


void TOptionBox::CreateMachinePage() {
  
  int &y=mY,&Wid=mWid,&Offset=mOffset;

#ifdef WIN32
  HWND Win;
  int mask;

#if defined(SSE_GUI_421_REORDER)

  EasyStr hint=T("The STE was more elaborated than the older STF but some\
 programs are compatible only with the STF");
#if defined(SSE_MEGA)
  hint+=T("\nThe Mega line was professional");
#endif

  // all models in a list ("STFM" too), radio buttons are in Quick settings
  CreateStatic(T("Model"));
  Wid=CbUnits*4;
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,Offset,y,
                   Wid,200,Handle,(HMENU)IDC_CB_ST_MODEL,hInstance,NULL);
  for(int i=0;i<N_ST_MODELS;i++)
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT(st_model_name[i]));
  //SendMessage(Win,CB_SETCURSEL,MIN((int)ST_MODEL,N_ST_MODELS-1),0);
  ToolAddWindow(ToolTip,Win,hint);

  // All possible RAM configs in a list, radio buttons are in Quick settings
  Offset+=Wid+HorizontalSeparation;
  CreateStatic(T("RAM Memory"));
  Wid=CbUnits*5;
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,
                   Offset,y,Wid,200,Handle,(HMENU)IDC_MEMORY_SIZE,hInstance,NULL);
  CBAddString(Win,"256K",MAKELONG(MEMCONF_128,MEMCONF_128)); // curiosity
  CBAddString(Win,"512K",MAKELONG(MEMCONF_512,MEMCONF_0));
  CBAddString(Win,"1MB",MAKELONG(MEMCONF_512,MEMCONF_512));
  CBAddString(Win,"2MB",MAKELONG(MEMCONF_2MB,MEMCONF_0));
  CBAddString(Win,"2.5MB",MAKELONG(MEMCONF_512,MEMCONF_2MB)); // curiosity
  CBAddString(Win,"4MB",MAKELONG(MEMCONF_2MB,MEMCONF_2MB));
#if defined(SSE_MMU_MONSTER_ALT_RAM)
  CBAddString(Win,"12MB (ALT-RAM)",MAKELONG(MEMCONF_6MB,MEMCONF_6MB));
#endif
#if !defined(SSE_GUI_NO14MB)
  if(OPTION_HACKS)
    CBAddString(Win, "14MB (hack)", MAKELONG(MEMCONF_7MB, MEMCONF_7MB));
#endif

  // dbi should be left alone, there's a setting bc it's more precise in 64bit builds
  NextLine();
  short const spinw=GuiSM.mCbUnits*2;
  CreateStatic("dbi");
  HWND hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,Wid=spinw,CharHeight,Handle,(HMENU)IDC_DBI0,hInstance,NULL);
  ToolAddWindow(ToolTip,hEdit,T("Check interrupt timing"));
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_DBI1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  LPARAM lpar_range=MAKELPARAM(32*TICKS8,-32*TICKS8);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.dbi);

  // hack, super fast CPU
  Offset+=Wid+HorizontalSeparation;
  CreateStatic(T("Clock (MHz)"));
  // the list will be filled by MachineUpdateIfVisible()
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,Offset,y,
    CbUnits*3,400,Handle,(HMENU)IDC_CPU_SPEED,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("The basic ST ran at 8MHz, higher speeds are a hack"));

  // Blitter start delay: considered as HW difference and not WU
  Offset+=Wid+HorizontalSeparation;
  CreateStatic(T("Blitter start"));
  Wid=CbUnits*2;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,Wid,CharHeight,Handle,(HMENU)IDC_BLITTER_WU0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_BLITTER_WU,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(4,1));
  SendMessageW(Win,UDM_SETPOS32,0,OPTION_BLITTER_WU);

  // clock fine tuning
  NextLine();
  TWidthHeight wh;
  char caption[128];
#ifdef SSE_TIMINGS32
  sprintf(caption,"System 9,999,999,999Hz");
#else
  sprintf(caption,"CPU clock 99,999,999Hz");
#endif
  wh=GetTextSize(Font,caption);
  CreateWindow("Static",caption,WS_CHILD,Offset,y,wh.Width,wh.Height*2,Handle, // notice *2
    (HMENU)(IDC_CPU_CLOCK_S),hInstance,NULL);
  Offset+=wh.Width+HorizontalSeparation;
  int w=32,d=10;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,
    (HMENU)(IDS_TORTOISE),hInstance,NULL);
  Offset+=w+d;
  Wid=page_w-Offset+page_l-w-OptionBox.mLineStart-d;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ,
    Offset,y,Wid,OptionBox.mSliderHeight,Handle,(HMENU)IDC_CPU_CLOCK,hInstance,NULL);
  Offset+=Wid;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,+Offset,y,w,w,Handle,
    (HMENU)(IDS_RABBIT),hInstance,NULL);
  y+=w-OptionBox.mLineHeight/2;
  SendMessage(Win,TBM_SETRANGEMIN,FALSE,8000000*TICKS8); // Steem original
  SendMessage(Win,TBM_SETRANGEMAX,FALSE,8025000*TICKS8);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETTIC,0,CPU_CLOCK_MEGA_ST);
  SendMessage(Win,TBM_SETTIC,0,CPU_CLOCK_STF_PAL);
  SendMessage(Win,TBM_SETPOS,1,CpuCustomHz);
  SendMessage(Win,TBM_SETPAGESIZE,0,1);
  SendMessage(Handle,WM_HSCROLL,0,(LPARAM)Win); // force update

  // MFP clock fine tuning
  NextLine();
  CreateStatic(T("System 9,999,999,999Hz"),IDC_MFPXTAL_S);
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,(HMENU)(IDS_TORTOISE),hInstance,NULL);
  Offset+=w+d;
  Wid=page_w-Offset+page_l-w-OptionBox.mLineStart-d;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ,
                   Offset,y,Wid,OptionBox.mSliderHeight,Handle,(HMENU)IDC_MFPXTAL,hInstance,NULL);
  Offset+=Wid;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,+Offset,y,w,w,Handle,(HMENU)(IDS_RABBIT),hInstance,NULL);
  y+=w-OptionBox.mLineHeight/2;
  SendMessage(Win,TBM_SETRANGEMIN,FALSE,2457500);
  SendMessage(Win,TBM_SETRANGEMAX,FALSE,2457800);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETTIC,0,MFP_XTAL1);
  SendMessage(Win,TBM_SETTIC,0,MFP_XTAL2);
  SendMessage(Win,TBM_SETPOS,1,Mfp.xtal);
  SendMessage(Win,TBM_SETPAGESIZE,0,1);
  SendMessage(Win,TBM_SETPOS,1,Mfp.xtal);
  SendMessage(Handle,WM_HSCROLL,0,(LPARAM)Win); // force update

  // Screen (mono/color/extended)
  NextLine();
  CreateStatic("Monitor");
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,Offset,y,page_w
                   -Offset+page_l-LineStart,200,Handle,(HMENU)IDC_EXTENDED_MONITOR,hInstance,NULL);
  CBAddString(Win,T("Colour")+" ("+T("Low/Med Resolution")+")");
  CBAddString(Win,T("Monochrome")+" ("+T("High Resolution")+")");
#ifndef NO_CRAZY_MONITOR
  if(!OPTION_C3)
  for(int n=0;n<EXTMON_RESOLUTIONS;n++)
  {
    char sres[64];
    sprintf(sres,"-> %u x %u x %u",+extmon_res[n][0],extmon_res[n][1],extmon_res[n][2]);
    CBAddString(Win,sres);
  }
#endif

#if defined(SSE_GUI_INSTANTCHANGE)
  NextLine();
  Win=CreateButton(T("Instant machine change"),IDC_INSTANTMACHINECHANGE);
  SendMessage(Win,BM_SETCHECK,SSEOptions.InstantMachineChange,0);
  ToolAddWindow(ToolTip,Win,T("Don't wait for reboot on ST/RAM/ROM changes, useful in select cases"));
#endif

  // Cartridge group
  NextLine();
  CreateWindow("Button",T("Cartridge"),WS_CHILD|BS_GROUPBOX,
    page_l,y,page_w,GROUP_HEIGHT(2),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
  y+=mGroupTitleHeight;
  Offset+=LineStart;
  CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Path Display","",WS_CHILD,
    Offset,y,page_w-LineStart*2,CharHeight,Handle,(HMENU)IDP_CARTRIDGE,hInstance,NULL);
  NextLine();
  Offset+=LineStart;
  CreateButton(("Choose"),IDC_CHOOSECART,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);
  CreateButton(T("Remove"),IDC_REMOVECART,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);
  CreateButton(T("Switch off"),IDC_SWITCHCART,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);
  CreateButton(T("Freeze"),IDC_FREEZECART,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);

#if defined(SSE_HARDWARE_OVERSCAN)
  // Hardware overscan (STF)
  NextLine();
  mask=WS_CHILD|BS_AUTORADIOBUTTON;
  CreateStatic(T("Hardware overscan"));
  hint=T("Those devices trick the DE signal to get permanent overscan.\
 Need drivers on the ST side. TOS 1.04 or 2.06");
  Win=CreateButton(T(overscan_dev[0]),IDC_RADIO_HWOVERSCAN,mask|WS_GROUP);
  ToolAddWindow(ToolTip,Win,hint);
  Win=CreateButton(T(overscan_dev[1]),IDC_RADIO_HWOVERSCAN+1,mask);
  ToolAddWindow(ToolTip,Win,hint);
  Win=CreateButton(T(overscan_dev[2]),IDC_RADIO_HWOVERSCAN+2,mask);
  ToolAddWindow(ToolTip,Win,hint);
  SendMessage(GetDlgItem(Handle,IDC_RADIO_HWOVERSCAN+OPTION_HWOVERSCAN),BM_SETCHECK,TRUE,0);  
#endif

  // Wakeups
  NextLine();
  CreateStatic(T("Wakeup GLU"));
  Wid=CbUnits*2;
  HWND hEdit0=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,Wid,CharHeight,Handle,(HMENU)IDC_GLU_WAKEUP0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS
    |UDS_SETBUDDYINT|UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_GLU_WAKEUP,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit0,0);
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(4,1));
  Offset+=Wid+HorizontalSeparation;
  CreateStatic(T("Shifter"));
  Wid=CbUnits*2;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,Wid,CharHeight,Handle,(HMENU)IDC_SHIFTER_WU0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_SHIFTER_WU,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(SHIFTER_MAX_WU_SHIFT,-SHIFTER_MAX_WU_SHIFT));
  Offset+=Wid+HorizontalSeparation;
  Win=CreateButton(T("Random on boot"),IDC_RANDOM_WU);
  SendMessage(Win,BM_SETCHECK,OPTION_RANDOM_WU,0);
  ToolAddWindow(ToolTip,Win,T("This affects both the GLUE and the Shifter"));

#else//#if defined(SSE_GUI_421_REORDER)

  // set option 'Show all settings' if some setting are advanced-only
  if(ST_MODEL==STFM||mem_len!=MEM_512KB&&mem_len!=MEM_1MB&&mem_len!=MEM_2MB&&mem_len!=MEM_4MB)
    SSEOptions.Advanced=true;

  EasyStr hint=T("The STE was more elaborated than the older STF but some\
 programs are compatible only with the STF");
#if defined(SSE_MEGA)
  EasyStr hint2=T("The Mega line was professional");
#endif
  CreateStatic(T("Model"));
  ALL_SETTINGS_BEGIN // all models in a list ("STFM" too)

  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,
    Offset,y,CbUnits*6,200,Handle,(HMENU)IDC_CB_ST_MODEL,hInstance,NULL);

  for(int i=0;i<N_ST_MODELS;i++)
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT(st_model_name[i]));
  SendMessage(Win,CB_SETCURSEL,MIN((int)ST_MODEL,N_ST_MODELS-1),0);
  ToolAddWindow(ToolTip,Win,hint);

  ALL_SETTINGS_ELSE // radio buttons but only 4 models

  mask=WS_CHILD|BS_AUTORADIOBUTTON;
  Win=CreateButton(st_model_name[STF],IDC_RADIO_ST_MODEL+STF,mask|WS_GROUP);
  ToolAddWindow(ToolTip,Win,hint);
#if defined(SSE_MEGAST)
  Win=CreateButton(st_model_name[MEGA_ST],IDC_RADIO_ST_MODEL+MEGA_ST,mask);
  ToolAddWindow(ToolTip,Win,hint2);
#endif
  Win=CreateButton(st_model_name[STE],IDC_RADIO_ST_MODEL+STE,mask);
  ToolAddWindow(ToolTip,Win,hint);
#if defined(SSE_MEGASTE)
  Win=CreateButton(st_model_name[MEGA_STE],IDC_RADIO_ST_MODEL+MEGA_STE,mask);
  ToolAddWindow(ToolTip,Win,hint2);
#endif
  SendMessage(GetDlgItem(Handle,IDC_RADIO_ST_MODEL+ST_MODEL),BM_SETCHECK,TRUE,0);

  ALL_SETTINGS_END

  NextLine();
  CreateStatic(T("RAM Memory"));

  ALL_SETTINGS_BEGIN // all memory sizes in a list
  
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,
    page_l+5+Wid,y,CbUnits*10,200,Handle,(HMENU)IDC_MEMORY_SIZE,hInstance,NULL);
  CBAddString(Win,"256K",MAKELONG(MEMCONF_128,MEMCONF_128)); // curiosity
  CBAddString(Win,"512K",MAKELONG(MEMCONF_512,MEMCONF_0));
  CBAddString(Win,"1 MB",MAKELONG(MEMCONF_512,MEMCONF_512));
  CBAddString(Win,"2 MB",MAKELONG(MEMCONF_2MB,MEMCONF_0));
  CBAddString(Win,"2.5 MB",MAKELONG(MEMCONF_512,MEMCONF_2MB)); // curiosity
  CBAddString(Win,"4 MB",MAKELONG(MEMCONF_2MB,MEMCONF_2MB));
#if defined(SSE_MMU_MONSTER_ALT_RAM)
  CBAddString(Win,"12 MB (MonSTer alt-RAM)",MAKELONG(MEMCONF_6MB,MEMCONF_6MB));
#endif
#if !defined(SSE_GUI_NO14MB)
  if(OPTION_HACKS)
    CBAddString(Win, "14 MB (hack)", MAKELONG(MEMCONF_7MB, MEMCONF_7MB));
#endif

  ALL_SETTINGS_ELSE // radio buttons, only 4 sizes

  mask=WS_CHILD|BS_AUTORADIOBUTTON;
  Win=CreateButton("512K",IDC_MEMORY_SIZE+0,mask|WS_GROUP);
  Win=CreateButton("1 MB",IDC_MEMORY_SIZE+1,mask);
  Win=CreateButton("2 MB",IDC_MEMORY_SIZE+2,mask);
  Win=CreateButton("4 MB",IDC_MEMORY_SIZE+3,mask);

  ALL_SETTINGS_END
  NextLine();

#if defined(SSE_GUI_EMUCONTROL) // CPU & MFP speed controls moved to emu param.

  HACK_BEGIN

  CreateStatic(T("CPU clock (MHz)"));
  // the list will be filled by MachineUpdateIfVisible()
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,Offset,y,
    CbUnits*5,400,Handle,(HMENU)IDC_CPU_SPEED,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("The basic ST ran at 8MHz, higher speeds are a hack"));

#if defined(SSE_MEGA16) && !defined(SSE_GEM_CONTROL_PANEL)
  Offset+=HorizontalSeparation+CbUnits*5;
  Win=CreateButton(T("Turbo"),IDC_TURBO16MHZ);
  ToolAddWindow(ToolTip,Win,T("This button was sadly lacking on real machines"));
#endif

  HACK_END

#else

  CreateWindow("Button",T("Clocks"),WS_CHILD|BS_GROUPBOX,
    page_l,y,page_w,GROUP_HEIGHT(5),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
  y+=mGroupTitleHeight;

  HACK_BEGIN

  Offset+=LineStart;
  CreateStatic(T("CPU (MHz)"));
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST
    ,Offset,y,CbUnits*5,400,Handle,(HMENU)IDC_CPU_SPEED,hInstance,
    NULL);
  ToolAddWindow(ToolTip,Win,T("The basic ST ran at 8MHz, higher speeds are a hack"));

  HACK_END

  NextLine();
  Offset+=LineStart;
  // user can fine tune CPU clock
  // to display clock so that player can use the keyboard to fine tune
  CreateStatic("The basic ST ran at 8MHz, higher speeds are a hack",IDC_CPU_CLOCK_S); // arbitrary long string
  NextLine();
  Offset+=LineStart;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,
    Offset,y,page_w-Offset+page_l-LineStart,mSliderHeight,Handle,
    (HMENU)IDC_CPU_CLOCK,hInstance,NULL);
  SendMessage(Win,TBM_SETRANGEMIN,FALSE,8000000*4); // Steem original
  SendMessage(Win,TBM_SETRANGEMAX,FALSE,8025000*4);
  SendMessage(Win,TBM_SETLINESIZE,0,4);
  SendMessage(Win,TBM_SETTIC,0,(CPU_CLOCK_MEGA_ST*4)/TICKS8);
  SendMessage(Win,TBM_SETTIC,0,(CPU_CLOCK_STF_PAL*4)/TICKS8);
  SendMessage(Win,TBM_SETPOS,1,(CpuCustomHz*4)/TICKS8);
  SendMessage(Win,TBM_SETPAGESIZE,0,1);
  //y+=LineHeight;
  NextLine();

  // user can fine tune MFP timer XTAL
  Offset+=LineStart;
  CreateStatic("12 MB (MonSTer alt-RAM)",IDC_MFPXTAL_S); // arbitrary long string
  NextLine();
  Offset+=LineStart;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_TABSTOP|TBS_HORZ,
    Offset,y,page_w-Offset+page_l-LineStart,mSliderHeight,Handle,
    (HMENU)IDC_MFPXTAL,hInstance,NULL);
  SendMessage(Win,TBM_SETRANGEMIN,FALSE,2457500);
  SendMessage(Win,TBM_SETRANGEMAX,FALSE,2457800);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETTIC,0,MFP_XTAL1);
  SendMessage(Win,TBM_SETTIC,0,MFP_XTAL2);
  SendMessage(Win,TBM_SETPOS,1,Mfp.xtal);
  SendMessage(Win,TBM_SETPAGESIZE,0,1);

#endif//#if defined(SSE_GUI_EMUCONTROL)

  NextLine();
  CreateWindow("Button",T("Cartridge"),WS_CHILD|BS_GROUPBOX,
    page_l,y,page_w,GROUP_HEIGHT(2),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
  y+=mGroupTitleHeight;
  Offset+=LineStart;
  CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Path Display","",WS_CHILD,
    Offset,y,page_w-LineStart*2,CharHeight,Handle,(HMENU)IDP_CARTRIDGE,hInstance,NULL);
  NextLine();
  Offset+=LineStart;
  CreateButton(("Choose"),IDC_CHOOSECART,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);
  CreateButton(T("Remove"),IDC_REMOVECART,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);
  CreateButton(T("Switch off"),IDC_SWITCHCART,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);
  CreateButton(T("Freeze"),IDC_FREEZECART,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);

  NextLine();

#endif//#if defined(SSE_GUI_421_REORDER)

#endif//WIN32

#ifdef UNIX

  Wid=hxc::get_text_width(XD,T("ST model"));
  st_type_label.create(XD,page_p,page_l,y,Wid,25,NULL,this,BT_STATIC 
    | BT_TEXT,T("ST Model"),0,BkCol);

  st_type_dd.id=4005;
  st_type_dd.make_empty();

  st_type_dd.additem(st_model_name[STF],STF);
#if defined(SSE_MEGAST)  
  st_type_dd.additem(st_model_name[MEGA_ST],MEGA_ST);
#endif
  st_type_dd.additem(st_model_name[STE],STE);
#if defined(SSE_MEGASTE)  
  st_type_dd.additem(st_model_name[MEGA_STE],MEGA_STE);
#endif

  st_type_dd.select_item_by_data(ST_MODEL);

  st_type_dd.create(XD,page_p,page_l+5+Wid,y,180-(15+Wid+10),350,
    dd_notify_proc,this);

  EasyStr hint=T("The STE was more elaborated than the older STF but some\
 programs are compatible only with the STF");
#if defined(SSE_MEGA)
  hint+=T("\nThe Mega line was professional");
#endif
  hints.add(st_type_dd.handle,hint,page_p);

  memory_label.create(XD,page_p,page_l+160+5,y,0,25,NULL,this,BT_LABEL,
    T("RAM"),0,BkCol);
  memory_dd.id=910;
  memory_dd.make_empty();
  memory_dd.lv.sl.Add("512Kb",MEMCONF_512,MEMCONF_0);
  memory_dd.lv.sl.Add("1 MB",MEMCONF_512,MEMCONF_512);
  memory_dd.lv.sl.Add("2 MB",MEMCONF_2MB,MEMCONF_0);
  memory_dd.lv.sl.Add("4 MB",MEMCONF_2MB,MEMCONF_2MB);
#if defined(SSE_MMU_MONSTER_ALT_RAM)
  memory_dd.lv.sl.Add("12 MB (MonSTer alt-RAM)",MEMCONF_6MB,MEMCONF_6MB);
  memory_dd.lv.sl.Add("14 MB (hack)",MEMCONF_7MB,MEMCONF_7MB);
#else
  memory_dd.lv.sl.Add("14 MB",MEMCONF_7MB,MEMCONF_7MB);
#endif
  memory_dd.create(XD,page_p,page_l+5+memory_label.w+160,y,
    page_w-(5+memory_label.w+160),200,dd_notify_proc,this);

  y+=LineHeight;

  Wid=hxc::get_text_width(XD,T("CPU (MHz)"));
  cpu_boost_label.create(XD,page_p,page_l,y,Wid,25,NULL,this,BT_STATIC | BT_TEXT,
    T("CPU (MHz)"),0,BkCol);
  cpu_boost_dd.id=8;
  cpu_boost_dd.make_empty();
#if defined(SSE_MEGA16)
  if((SSEConfig.Mega) && (Cpu16.ScuReg&2))
    cpu_boost_dd.additem("16",CpuNormalHz/TICKS8);
  else
#endif  
  cpu_boost_dd.additem("8",CpuNormalHz/TICKS8);
  int Mhz=8;
  char sMhz[32];
  COUNTER_VAR Hz=0;
  
  for(int i=1; Hz<CPU_MAX_HERTZ; i++)
  {
    Mhz<<=1;
    sprintf(sMhz,PRICV,Mhz);
    Hz=Mhz*1000000;
    cpu_boost_dd.additem(sMhz,Hz);
  }  
  if (cpu_boost_dd.select_item_by_data(nSysCyclesPerSecond/TICKS8)<0){
    EasyStr Cycles=nSysCyclesPerSecond/TICKS8;
    Cycles=Cycles.Lefts(Cycles.Length()-6);
    cpu_boost_dd.additem(Cycles+" "+Mhz,nSysCyclesPerSecond/TICKS8);
    cpu_boost_dd.changesel(cpu_boost_dd.lv.sl.NumStrings-1);
  }

  cpu_boost_dd.create(XD,page_p,page_l+5+Wid,y,400-(15+Wid+10),350,dd_notify_proc,this);
  y+=35;

#if defined(SSE_MEGA16)
  if(SSEConfig.Mega)
  {
    mega_cache_but.create(XD,page_p,page_l+20,y,0,25,
      button_notify_proc,this,BT_CHECKBOX,T("Mega Turbo"),IDC_TURBO16MHZ,BkCol);
    mega_cache_but.set_check(((Cpu16.ScuReg&3)==3));
    hints.add(st_type_dd.handle,T("This button was sadly lacking on real machines"),page_p);
    y+=LineHeight;
  }
#endif

  // user can fine tune CPU clock
  cpuclock_label.create(XD,page_p,page_l+10,y,0,25,NULL,this,BT_LABEL,T("CPU (Hz)"),0,BkCol);
  cpuclock_label.create(XD,page_p,page_l+10,y,0,25,NULL,this,BT_LABEL,T("CPU (Hz)"),0,BkCol);
  cpuclock_sb.horizontal=true;
  //int _range,int _viewrange,int _pos
  cpuclock_sb.init(30000,0,CpuCustomHz-8000000);
  cpuclock_sb.create(XD,page_p,page_l+15+cpuclock_label.w,y,370-(15+cpuclock_label.w),25,scrollbar_notify_proc,this);
  cpuclock_sb.id=19;
  y+=35;

  // user can fine tune MFP timer XTAL
  mfpxtal_label.create(XD,page_p,page_l+10,y,0,25,NULL,this,BT_LABEL,
      T("MFP XTAL"),0,BkCol);
  mfpxtal_sb.horizontal=true;
  //int _range,int _viewrange,int _pos
  mfpxtal_sb.init(300,0,Mfp.xtal-2457500);
  mfpxtal_sb.create(XD,page_p,page_l+15+mfpxtal_label.w,y,370-(15+mfpxtal_label.w),25,scrollbar_notify_proc,this);
  mfpxtal_sb.id=18;
  y+=35;

  cart_group.create(XD,page_p,page_l,y,page_w,90,NULL,this,BT_GROUPBOX,T("Cartridge (optional)"),0,BkCol);

  cart_display.create(XD,cart_group.handle,10,25,
    page_w-20,25,NULL,this,BT_STATIC|BT_TEXT|BT_BORDER_INDENT|BT_TEXT_PATH,
    CartFile.Text,0,WhiteCol);

#if defined(SSE_DONGLE)

  cart_change_but.create(XD,cart_group.handle,10,55,
    page_w/4-10-5,25,button_notify_proc,this,BT_TEXT,T("Choose"),IDC_CHOOSECART,BkCol);

  cart_remove_but.create(XD,cart_group.handle,page_w/4+5,55,
    page_w/4-10-5,25,button_notify_proc,this,BT_TEXT,T("Remove"),IDC_REMOVECART,BkCol);

  cart_switch_but.create(XD,cart_group.handle,2*page_w/4+5,55,
    page_w/4-10-5,25,button_notify_proc,this,BT_TEXT,T("Switch off"),IDC_SWITCHCART,BkCol);

  cart_freeze_but.create(XD,cart_group.handle,3*page_w/4+5,55,
    page_w/4-10-5,25,button_notify_proc,this,BT_TEXT,T("Freeze"),IDC_FREEZECART,BkCol);

#else
  cart_change_but.create(XD,cart_group.handle,10,55,
    page_w/2-10-5,25,button_notify_proc,this,BT_TEXT,T("Choose"),737,BkCol);

  cart_remove_but.create(XD,cart_group.handle,page_w/2+5,55,
    page_w/2-10-5,25,button_notify_proc,this,BT_TEXT,T("Remove"),747,BkCol);
#endif

  y+=100;

  mY=y;
#endif//UNIX

#if defined(SSE_GUI_421_REORDER)
  NextLine();
  CreateRebootButton("");
#elif defined(SSE_GUI_INSTANTCHANGE)
  if(!SSEOptions.InstantMachineChange)
  {
    EasyStr protip=T("ST Model and memory changes don't take effect until the next reboot of the ST.");
    CreateRebootButton(protip);
  }
#else
  // ST model change takes effect immediately ("interesting" effects)
  EasyStr protip=T("Memory changes don't take effect until the next reboot of the ST.");
  CreateRebootButton(protip);
#endif

#ifdef WIN32
  ShowPageControls();
#endif
  MachineUpdateIfVisible();
}


void TOptionBox::MachineUpdateIfVisible() {

  RefreshTOSBox();

#ifdef WIN32
  HWND Win;
  if(Handle==NULL)
    return;
  if(0) {}
#if !defined(SSE_GUI_421_REORDER)
  else if(GetDlgItem(Handle,IDC_STASPECTRATIO)!=NULL) // it's ST Video Page instead
  {
    UpdateSTVideoPage(); // then update it
    return;
  }
#endif
#if defined(SSE_GEM_CONTROL_PANEL)
  else if(HWND h=GetDlgItem(Handle,IDC_CLICK)) // it's GEM Control Panel Page instead
  {
    InvalidateRect(GetDlgItem(Handle,IDC_BELL),NULL,FALSE);
    InvalidateRect(GetDlgItem(Handle,IDC_REPEAT),NULL,FALSE);
    InvalidateRect(h,NULL,FALSE);
    return;
  }
#endif

  HWND cb_mem=GetDlgItem(Handle,IDC_MEMORY_SIZE);
  if(cb_mem==NULL) // not machine page
    return;

#if defined(SSE_GUI_INSTANTCHANGE)
  int st_model=(NewStModel>=0) ? NewStModel : ST_MODEL;
  ALL_SETTINGS_BEGIN
  // combobox
  Win=GetDlgItem(Handle,IDC_CB_ST_MODEL);
  SendMessage(Win,CB_SETCURSEL,st_model,0);
  ALL_SETTINGS_ELSE
  // radio buttons
  for(int i=0;i<N_ST_MODELS;i++) // check one, uncheck others
  {
    if(HWND h=GetDlgItem(Handle,IDC_RADIO_ST_MODEL+i))
      SendMessage(h,BM_SETCHECK,(i==st_model),0);
  }
  ALL_SETTINGS_END
#else
  for(int i=0;i<N_ST_MODELS;i++) // check one, uncheck others
  {
    if(HWND h=GetDlgItem(Handle,IDC_RADIO_ST_MODEL+i))
      SendMessage(h,BM_SETCHECK,(i==ST_MODEL),0);
  }
#endif

  // super CPU clock
  Win=GetDlgItem(Handle,IDC_CPU_SPEED);
  SendMessage(Win,CB_RESETCONTENT,0,0);
#if defined(SSE_MEGA)
#if defined(SSE_MEGA16)
  CBAddString(Win,((SSEConfig.Mega) && (Cpu16.ScuReg&2))?"16":"8",CpuNormalHz/TICKS8);
#else
  CBAddString(Win,(IS_MEGASTE && (Cpu16.ScuReg&2))?"16":"8",CpuNormalHz/TICKS8); // <- for this
#endif
#endif
  int Mhz=8;
  char sMhz[32];
  COUNTER_VAR Hz=0;
  for(int i=1; Hz<CPU_MAX_HERTZ; i++)
  {
    Mhz<<=1;
    sprintf(sMhz, "%d", Mhz);
    Hz=Mhz*1000000;
    CBAddString(Win,sMhz,Hz);
  }
  if(CBSelectItemWithData(Win,nSysCyclesPerSecond/TICKS8)<0) 
    CBSelectItemWithData(Win,nSysCyclesPerSecond/TICKS8);

  // Memory - should work with different builds (w/wo 256KB, 2.5MB...)
  BYTE MemConf[2];
  if(NewMemConf0==-1) // no new memory config selected
    GetCurrentMemConf(MemConf);
  else
  {
    MemConf[0]=(BYTE)NewMemConf0;
    MemConf[1]=(BYTE)NewMemConf1;
  }
  DWORD dwMemConf=MAKELONG(MemConf[0],MemConf[1]);

  ALL_SETTINGS_BEGIN
  // by Steem authors - could be used more?
  LRESULT curs_index=CBFindItemWithData(cb_mem,dwMemConf); 
  SendMessage(cb_mem,CB_SETCURSEL,curs_index,0);
  ALL_SETTINGS_ELSE // quick settings it's autoradio
  int myMemLen=Mmu.BankLength(MemConf[0])+Mmu.BankLength(MemConf[1]);
  for(int i=0;i<4;i++) // check one, uncheck others
    SendMessage(GetDlgItem(Handle,IDC_MEMORY_SIZE+i),BM_SETCHECK,(myMemLen==((512*1024)<<i)),0);
  ALL_SETTINGS_END

#if defined(SSE_GUI_421_REORDER)
  int monitor_sel=NewMonitorSel;
  if(monitor_sel<0)
    monitor_sel=GetCurrentMonitorSel();
  SendMessage(GetDlgItem(Handle,IDC_EXTENDED_MONITOR),CB_SETCURSEL,monitor_sel,0);
#endif

  SetWindowText(GetDlgItem(Handle,IDP_CARTRIDGE),CartFile);
  EnableWindow(GetDlgItem(Handle,IDC_REMOVECART),CartFile.NotEmpty());
  EnableWindow(GetDlgItem(Handle,IDC_FREEZECART),CartFile.NotEmpty());
  HWND hSwitch=GetDlgItem(Handle,IDC_SWITCHCART);
  SendMessage(hSwitch,WM_SETTEXT,0,OPTION_CARTRIDGE_OFF
    ? (LPARAM)T("Switch on").Text : (LPARAM)T("Switch off").Text);
  EnableWindow(hSwitch,CartFile.NotEmpty());
#if !defined(SSE_GUI_EMUCONTROL)
  HWND hSlider=GetDlgItem(Handle,IDC_CPU_CLOCK);
  SendMessage(hSlider,TBM_SETPOS,1,(CpuCustomHz*4)/TICKS8); // put new value
  SendMessage(Handle,WM_HSCROLL,0,(LPARAM)hSlider); // force update
  hSlider=GetDlgItem(Handle,IDC_MFPXTAL);
  SendMessage(hSlider,TBM_SETPOS,1,Mfp.xtal);
  SendMessage(Handle,WM_HSCROLL,0,(LPARAM)hSlider); // force update
#endif

#if defined(SSE_GUI_421_REORDER)
  bool sel_is_stf=(st_model==STF)||(st_model==MEGA_ST);
  Win=GetDlgItem(Handle,IDC_SHIFTER_WU);
  SendMessageW(Win,UDM_SETPOS32,0,OPTION_SHIFTER_WU);
  Win=GetDlgItem(Handle,IDC_UNSTABLE_SHIFTER);
  SendMessage(Win,BM_SETCHECK,OPTION_UNSTABLE_SHIFTER,0);
  Win=GetDlgItem(Handle,IDC_GLU_WAKEUP0);
  EnableWindow(Win,sel_is_stf);
  Win=GetDlgItem(Handle,IDC_GLU_WAKEUP);
  SendMessageW(Win,UDM_SETPOS32,0,Mmu.WS[OPTION_WS]); // triggers option!
  Win=GetDlgItem(Handle,IDC_RADIO_HWOVERSCAN+1); // also "advanced"
  EnableWindow(Win,sel_is_stf);
  Win=GetDlgItem(Handle,IDC_RADIO_HWOVERSCAN+2);
  EnableWindow(Win,sel_is_stf);
#endif

#endif//WIN32

#ifdef UNIX
  int memconf=4;
  if (NewMemConf0<0){
    if (mem_len<1024*1024){
      memconf=0;
    }else if (mem_len<2048*1024){
      memconf=1;
    }else if (mem_len<4096*1024){
      memconf=2;
    }else if (mem_len<14*1024*1024){
      memconf=3;
    }
  }else{
    if (NewMemConf0==MEMCONF_512) memconf=int((NewMemConf1==MEMCONF_512) ? 1:0); // 1Mb:512Kb
    if (NewMemConf0==MEMCONF_2MB) memconf=int((NewMemConf1==MEMCONF_2MB) ? 3:2); // 4Mb:2Mb
  }

  memory_dd.changesel(memconf);

  int monitor_sel=NewMonitorSel;

  if (monitor_sel<0) monitor_sel=GetCurrentMonitorSel();
  monitor_dd.changesel(monitor_sel);

  cart_display.set_text(CartFile.Text);
  
#if defined(SSE_DONGLE)  
  cart_switch_but.set_text( (OPTION_CARTRIDGE_OFF)
    ? T("Switch on").Text : T("Switch off").Text) ;
#endif
#endif//UNIX
}


void TOptionBox::CreateTOSPage() {

  int &y=mY,&Offset=mOffset;

#ifdef WIN32
  HWND Win;
  TNotify myNotify(T("Checking TOS files")); // it's slow

  ALL_SETTINGS_BEGIN // because it's a bit over the top
  CreateStatic(T("Sort by"));
  Win=CreateWindow("Combobox","",WS_CHILD|CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,
    Offset,y,page_w-Offset+page_l,200,Handle,(HMENU)IDC_TOSSORTBY,hInstance,NULL);
  CBAddString(Win,T("Version (Ascending)"),MAKELONG((WORD)eslSortByData0,0));
  CBAddString(Win,T("Version (Descending)"),MAKELONG((WORD)eslSortByData0,1));
  CBAddString(Win,T("Language"),MAKELONG((WORD)eslSortByData1,0));
  CBAddString(Win,T("Date (Ascending)"),MAKELONG((WORD)eslSortByData2,0));
  CBAddString(Win,T("Date (Descending)"),MAKELONG((WORD)eslSortByData2,1));
  CBAddString(Win,T("Name (Ascending)"),MAKELONG((WORD)(signed short)eslSortByNameI,0));
  CBAddString(Win,T("Name (Descending)"),MAKELONG((WORD)(signed short)eslSortByNameI,1));
  if(CBSelectItemWithData(Win,MAKELONG(eslTOS_Sort,eslTOS_Descend))<0)
  {
    SendMessage(Win,CB_SETCURSEL,0,0);
    eslTOS_Sort=eslSortByData0;
    eslTOS_Descend=false;
  }
  y+=LineHeight;
  ALL_SETTINGS_END

  int TOSBoxHeight=(OPTIONS_HEIGHT*3)/5;
  Win=CreateWindowEx(WS_EX_CLIENTEDGE,"ListBox","",WS_CHILD|WS_VSCROLL|
    WS_TABSTOP|LBS_NOINTEGRALHEIGHT|LBS_OWNERDRAWFIXED|LBS_NOTIFY|LBS_SORT,
    page_l,y,page_w,TOSBoxHeight,Handle,(HMENU)IDC_TOSLIST,hInstance,NULL);
  SendMessage(Win,LB_SETITEMHEIGHT,0,MAX((int)GetTextSize(Font,"HyITljq").Height
    +4,RC_FLAG_HEIGHT+4));
  y+=TOSBoxHeight+LineStart;
  CreateWindow("Button",T("Add"),WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE,
    page_l,y,page_w/2-5,CharHeight,Handle,(HMENU)IDC_ADDTOS,hInstance,NULL);
  CreateWindow("Button",T("Remove"),WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE,
    page_l+page_w/2+5,y,page_w/2-5,CharHeight,Handle,(HMENU)IDC_REMOVETOS,hInstance,NULL);
  NextLine();
#if defined(SSE_GUI_INSTANTCHANGE)
  int st_model=(NewStModel>=0) ? NewStModel : ST_MODEL;
  EasyStr protip;
  if(!SSEOptions.InstantMachineChange)
    protip=T("TOS changes don't take effect until the next reboot. ");
  protip+="You should choose a TOS compatible with the ";
  protip+=st_model_name[st_model];
  CreateRebootButton(protip);
#else
  CreateRebootButton(T("TOS changes don't take effect until the next reboot.\
 You should choose a TOS compatible with the ")+st_model_name[ST_MODEL]);
#endif
  MachineUpdateIfVisible();
#endif//WIN32

#ifdef UNIX
  int tosbox_h=OPTIONS_HEIGHT-10-35-10-35-55-25-10;
  hxc_button *label=new hxc_button(XD,page_p,page_l,y,0,25,NULL,this,
    BT_TEXT | BT_STATIC | BT_BORDER_NONE,T("Sort by"),0,BkCol);
  tos_sort_dd.make_empty();
  tos_sort_dd.lv.sl.Add(T("Version (Ascending)"),eslSortByData0,0);
  tos_sort_dd.lv.sl.Add(T("Version (Descending)"),eslSortByData0,1);
  tos_sort_dd.lv.sl.Add(T("Language"),eslSortByData1,0);
  tos_sort_dd.lv.sl.Add(T("Date (Ascending)"),eslSortByData2,0);
  tos_sort_dd.lv.sl.Add(T("Date (Descending)"),eslSortByData2,1);
  tos_sort_dd.lv.sl.Add(T("Name (Ascending)"),eslSortByNameI,0);
  tos_sort_dd.lv.sl.Add(T("Name (Descending)"),eslSortByNameI,1);
  bool Found=false;
  for(int i=0;i<tos_sort_dd.lv.sl.NumStrings;i++)
  {
    if(tos_sort_dd.lv.sl[i].Data[0]==(long)eslTOS_Sort)
    {
      if(tos_sort_dd.lv.sl[i].Data[1]==(long)eslTOS_Descend)
      {
        Found=true;
        tos_sort_dd.sel=i;
        break;
      }
    }
  }
  if(!Found)
  {
    tos_sort_dd.sel=0;
    eslTOS_Sort=eslSortByData0;
    eslTOS_Descend=0;
  }
  tos_sort_dd.create(XD,page_p,page_l+label->w+5,y,page_w-(label->w+5),200,
    dd_notify_proc,this);
  tos_sort_dd.id=1020;
  y+=35;

  tos_lv.id=1000;
  tos_lv.create(XD,page_p,page_l,y,page_w,tosbox_h,listview_notify_proc,this);
  y+=tosbox_h+5;

  tosadd_but.create(XD,page_p,page_l,y,page_w/2-5,25,button_notify_proc,this,
    BT_TEXT,T("Add To List"),1010,hxc::col_bk);

  tosrefresh_but.create(XD,page_p,page_l+page_w/2+5,y,page_w/2-5,25,
    button_notify_proc,this,BT_TEXT,T("Refresh"),1011,hxc::col_bk);
  y+=35;

  mustreset_td.text=T("TOS changes don't take effect until the next cold reset of the ST");
  mustreset_td.sy=0;
  mustreset_td.wordwrapped=false;
  mustreset_td.create(XD,page_p,page_l,y,page_w,45,hxc::col_white,0);
  y+=55;

  coldreset_but.create(XD,page_p,page_l,y,page_w,25,button_notify_proc,this,
    //BT_TEXT,T("Reboot now"),1000,hxc::col_bk);
    BT_TEXT,T("Reboot ST now"),1000,hxc::col_bk);

  RefreshTOSBox();

#endif//UNIX
}


void TOptionBox::CreateGeneralPage() {
  int &y=mY,&Offset=mOffset,&Wid=mWid;

#ifdef WIN32
  HWND Win;

#if defined(SSE_GUI_421_REORDER)
  EasyStr caption=T("Reset settings");
  Wid=GetTextSize(Font,caption).Width;
  Offset+=page_w/2-Wid/2; //center
  Win=CreateButton(caption,IDC_ADVANCED_RESET,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);
  ToolAddWindow(ToolTip,Win,T("When it's FUBAR"));

  NextLine(); 
#if defined(SSE_GUI_TOOLBAR)
  Win=CreateButton(T("Tool bar"),IDC_TOOLBAR);
  SendMessage(Win,BM_SETCHECK,OPTION_TOOLBAR,0);
#endif

#if defined(SSE_GUI_MENUBAR)
  Win=CreateButton(T("Menu bar"),IDC_MENUBAR);
  SendMessage(Win,BM_SETCHECK,OPTION_MENUBAR,0);
#endif

#if defined(SSE_GUI_STATUS_BAR)
  Win=CreateButton(T("Status bar"),IDC_STATUSBAR);
  SendMessage(Win,BM_SETCHECK,OPTION_STATUS_BAR,0);
  Win=CreateButton(T("TOS flag"),IDC_TOSFLAG);
  SendMessage(Win,BM_SETCHECK,OPTION_TOSFLAG,0);
#endif

  NextLine();
  Win=CreateButton(T("Save backup snapshot on reset"),IDC_RESETBACKUP);
  SendMessage(Win,BM_SETCHECK,SSEOptions.ResetBackup,0);

#if defined(SSE_VID_LS) && defined(SSE_DEFAULTS_421)
  NextLine();
  Win=CreateButton(T("Save screenshot with snapshot"),IDC_SNAPSHOTPIC);
  SendMessage(Win,BM_SETCHECK,SSEOptions.ScreenshotWithSnapshot,0);
#endif

#else//#if defined(SSE_GUI_421_REORDER)

#if defined(SSE_GEM_CONTROL_PANEL)
  int w=32,d=10;
  CreateWindow("Button","Speed",WS_CHILD|BS_GROUPBOX,page_l,y,page_w,
               GROUP_HEIGHT(5)+(w-LineHeight/2),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
#else
  CreateWindow("Button","Speed",WS_CHILD|BS_GROUPBOX,page_l,y,page_w,GROUP_HEIGHT(5),
               Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
#endif
  y+=mGroupTitleHeight;
  Offset+=LineStart;

  DWORD SimplDis=0;
  CreateWindow("Static","",WS_CHILD|SS_CENTER|SimplDis,page_l+LineStart,y,page_w-LineStart*2,
               20,Handle,(HMENU)IDC_SPEEDTXT,hInstance,NULL);
  y+=20;

#if defined(SSE_GEM_CONTROL_PANEL)
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,(HMENU)(IDS_TORTOISE),
               hInstance,NULL);
  Offset+=w+d;
  Wid=page_w-Offset+page_l-w-LineStart-d;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ,Offset,y,Wid,
                   mSliderHeight,Handle,(HMENU)IDC_SPEED,hInstance,NULL);
  Offset+=Wid;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,(HMENU)(IDS_RABBIT),
               hInstance,NULL);
#else
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_TABSTOP|TBS_HORZ|SimplDis,page_l+LineStart,y,
                   page_w-LineStart*2,mSliderHeight,Handle,(HMENU)1041,hInstance,NULL);
#endif
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,199));
  SendMessage(Win,TBM_SETPOS,1,((100000/run_speed_ticks_per_second)-5)/5);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  for(int n=9;n<200;n+=10)
    SendMessage(Win,TBM_SETTIC,0,n);
  SendMessage(Win,TBM_SETPAGESIZE,0,1);
  SendMessage(Handle,WM_HSCROLL,0,(LPARAM)Win);

  NextLine();
  CreateWindow("Static",T("Slow motion speed")+": "+(slow_motion_speed/10)+"%", WS_CHILD|SS_CENTER,
               page_l+LineStart,y,page_w-LineStart*2,20,Handle,(HMENU)1000,hInstance,NULL);
  y+=20;
#if defined(SSE_GEM_CONTROL_PANEL)
  Offset+=LineStart;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,(HMENU)(IDS_TORTOISE),
               hInstance,NULL);
  Offset+=w+d;
  Wid=page_w-Offset+page_l-w-LineStart-d;
  y+=5;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ,Offset,y,Wid,
                   mSliderHeight,Handle,(HMENU)IDC_SLOWSPEED,hInstance,NULL);
  Offset+=Wid;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,(HMENU)(IDS_RABBIT),
               hInstance,NULL);
#else
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_TABSTOP|TBS_HORZ,page_l+LineStart,y,page_w-
                   LineStart*2,mSliderHeight,Handle,(HMENU)IDC_SLOWSPEED,hInstance,NULL);
#endif
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,79));
  SendMessage(Win,TBM_SETPOS,1,(slow_motion_speed-10)/10);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  for(int n=4;n<79;n+=5)
    SendMessage(Win,TBM_SETTIC,0,n);
  SendMessage(Win,TBM_SETPAGESIZE,0,10);

  NextLine();
  CreateWindow("Static","",WS_CHILD | SS_CENTER,page_l+LineStart,y,page_w-LineStart*2,20,
               Handle,(HMENU)IDC_FASTFWDTXT,hInstance,NULL);
  y+=20;
#if defined(SSE_GEM_CONTROL_PANEL)
  Offset+=LineStart;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,(HMENU)(IDS_TORTOISE),
               hInstance,NULL);
  Offset+=w+d;
  Wid=page_w-Offset+page_l-w-LineStart-d;
  y+=5;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_AUTOTICKS,Offset,
                   y,Wid,mSliderHeight,Handle,(HMENU)IDC_FASTFWD,hInstance,NULL);
  Offset+=Wid;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,(HMENU)(IDS_RABBIT),
               hInstance,NULL);
  y+=(w-LineHeight/2);
#else
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_TABSTOP|TBS_HORZ|TBS_AUTOTICKS,page_l+LineStart,
                   y,page_w-LineStart*2,mSliderHeight,Handle,(HMENU)1011,hInstance,NULL);
#endif
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,18));
  SendMessage(Win,TBM_SETPOS,1,(1000/MAX(fast_forward_max_speed,50))-2);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETTICFREQ,1,0); // only place in Steem where it's used
  SendMessage(Win,TBM_SETPAGESIZE,0,3);
  SendMessage(Handle,WM_HSCROLL,0,(LPARAM)Win);
  y+=LineStart;
#endif//#if !defined(SSE_GUI_421_REORDER)

  NextLine();
  DWORD mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX;
  Wid=GetCheckBoxSize(Font,T("Show pop-up hints")).Width;
  Win=CreateWindow("Button",T("Show pop-up hints"),mask,page_l,y,Wid,25,Handle,(HMENU)IDC_SHOWTIPS,
                   hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("This is a pop-up hint"));
  SendMessage(Win,BM_SETCHECK,ShowTips,0);
  NextLine();

  Wid=GetCheckBoxSize(Font,T("Make Steem high priority")).Width;
  Win=CreateWindow("Button",T("Make Steem high priority"),mask,page_l,y,Wid,25,Handle,
                   (HMENU)IDC_HIGHPRIORITY,hInstance,NULL);
  SendMessage(Win,BM_SETCHECK,HighPriority,0);
  ToolAddWindow(ToolTip,Win,T("When this option is ticked Steem will get first\
 use of the CPU ahead of other applications, this means Steem will still run\
 smoothly even if you start doing something else at the same time, but everything\
 else will run slower."));
  NextLine();

#if defined(SSE_EMU_THREAD)
  Wid=GetCheckBoxSize(Font,T("Emulation thread")).Width;
  Win=CreateWindow("Button",T("Emulation thread"), mask | ((runstate!=RUNSTATE_STOPPED)?WS_DISABLED:0),
                   page_l,y,Wid,25,Handle,(HMENU)IDC_EMU_THREAD,hInstance,NULL);
  SendMessage(Win,BM_SETCHECK,OPTION_EMUTHREAD,0);
  ToolAddWindow(ToolTip,Win,
#ifdef DEBUG_BUILD
    T("Debug build: Not recommended! Always stop emulation before messing with Debugger windows!"));
#else
    //T("This could make Steem more responsive. A bit experimental."));
    T("This could make Steem more responsive, or not responsive at all when things go wrong"));
#endif
  NextLine();
#endif

  Wid=GetCheckBoxSize(Font,T("Pause emulation when inactive")).Width;
  Win=CreateWindow("Button",T("Pause emulation when inactive"),mask,page_l,y,Wid,25,Handle,
                   (HMENU)IDC_AUTOPAUSE,hInstance,NULL);
  SendMessage(Win,BM_SETCHECK,bPauseWhenInactive,0);
  NextLine();

  Wid=GetCheckBoxSize(Font,T("Disable system keys when running")).Width;
  Win=CreateWindow("Button",T("Disable system keys when running"),mask,page_l,y,Wid,25,Handle,
                   (HMENU)IDC_SYSKEYS,hInstance,NULL);
  SendMessage(Win,BM_SETCHECK,!bAllowTaskSwitch,0);
  ToolAddWindow(ToolTip,Win,
T("When this option is ticked Steem will disable the Alt-Tab, Alt-Esc and Ctrl-Esc key combinations\
 when it is running, this allows the ST to receive those keys. This option doesn't work in fullscreen\
 mode."));
  NextLine();

  Wid=GetCheckBoxSize(Font,T("Start emulation on mouse click")).Width;
  Win=CreateWindow("Button",T("Start emulation on mouse click"),mask,page_l,y,Wid,25,Handle,
                   (HMENU)IDC_STARTONCLICK,hInstance,NULL);
  SendMessage(Win,BM_SETCHECK,StartEmuOnClick,0);
  ToolAddWindow(ToolTip,Win,T("When this option is ticked clicking a mouse button on Steem's\
 main window will start emulation."));

#endif//WIN32

#ifdef UNIX
  RunSpeedLabel.create(XD,page_p,page_l,y,page_w,25,NULL,this,
      BT_STATIC | BT_TEXT | BT_BORDER_NONE | BT_TEXT_CENTRE,"",0,BkCol);
  y+=25;

  RunSpeedSB.horizontal=true;
  RunSpeedSB.init(189+10,10,((100000/run_speed_ticks_per_second)-50) / 5); // 1 tick per 5%
  RunSpeedSB.create(XD,page_p,page_l,y,page_w,25,scrollbar_notify_proc,this);
  RunSpeedSB.id=2;
  y+=35;

  SMSpeedLabel.create(XD,page_p,page_l,y,page_w,25,NULL,this,
      BT_STATIC | BT_TEXT | BT_BORDER_NONE | BT_TEXT_CENTRE,"",0,BkCol);
  y+=25;

  SMSpeedSB.horizontal=true;
  SMSpeedSB.init(79+5,5,(slow_motion_speed-10)/10);
  SMSpeedSB.create(XD,page_p,page_l,y,page_w,25,scrollbar_notify_proc,this);
  SMSpeedSB.id=0;
  y+=35;

  FFMaxSpeedLabel.create(XD,page_p,page_l,y,page_w,25,NULL,this,
        BT_STATIC | BT_TEXT | BT_BORDER_NONE | BT_TEXT_CENTRE,"",0,BkCol);
  y+=25;

  FFMaxSpeedSB.horizontal=true;
  FFMaxSpeedSB.init(18+4,4,(1000/MAX(fast_forward_max_speed,50))-2);
  FFMaxSpeedSB.create(XD,page_p,page_l,y,page_w,25,scrollbar_notify_proc,this);
  FFMaxSpeedSB.id=1;
  y+=35;
  scrollbar_notify_proc(&SMSpeedSB,SBN_SCROLL,SMSpeedSB.pos);

  hxc_button *p_but=new hxc_button(XD,page_p,page_l,y,0,25,button_notify_proc,this,
        BT_CHECKBOX,T("Show pop-up hints"),121,BkCol);
  p_but->set_check(ShowTips);
  y+=35;

  high_priority_but.create(XD,page_p,page_l,y,0,25,button_notify_proc,this,
        BT_CHECKBOX,T("Make Steem high priority"),120,BkCol);
  high_priority_but.set_check(HighPriority);
  hints.add(high_priority_but.handle,T("When this option is ticked Steem will get first use of the CPU ahead of other applications, this means Steem will still run smoothly even if you start doing something else at the same time, but everything else will run slower."),
              page_p);
  y+=35;

  pause_inactive_but.create(XD,page_p,page_l,y,0,25,button_notify_proc,this,
        BT_CHECKBOX,T("Pause emulation when inactive"),110,BkCol);
  pause_inactive_but.set_check(bPauseWhenInactive);
  y+=35;

  ff_on_fdc_but.create(XD,page_p,page_l,y,0,25,
          button_notify_proc,this,BT_CHECKBOX,
          T("Automatic fast forward on disk access"),130,BkCol);
  ff_on_fdc_but.set_check(DiskMan.floppy_access_ff);
  y+=35;

  start_click_but.create(XD,page_p,page_l,y,0,25,
          button_notify_proc,this,BT_CHECKBOX,
          T("Start emulation on mouse click"),140,BkCol);
  start_click_but.set_check(StartEmuOnClick);
  hints.add(start_click_but.handle,T("When this option is ticked clicking a mouse button on Steem's main window will start emulation."),page_p);

  
#endif//UNIX
}


void TOptionBox::CreatePortOptions(int port,int& y) {

#ifdef WIN32
  // we keep groupbox here because it's used as parent window

  HWND Win;
  int& Wid=mWid;
  int GroupHeight=(OPTIONS_HEIGHT-10)/3-10;
  INT_PTR base=IDC_PORTSBASE+port*100;
  HWND CtrlParent=CreateWindowEx(WS_EX_CONTROLPARENT,"Button",STPort[port].Name,WS_CHILD|BS_GROUPBOX,
                                 page_l,y,page_w,GroupHeight,Handle,(HMENU)(base),hInstance,NULL);
  SetWindowLongPtr(CtrlParent, GWLP_USERDATA,(LONG_PTR)this);
  Old_GroupBox_WndProc=(WNDPROC)SetWindowLongPtr(CtrlParent,GWLP_WNDPROC,(LONG_PTR)GroupBox_WndProc);
  y+=GroupHeight;
  Wid=get_text_width(T("Connect to"));
  int y2=GUIMUL(14); // relative to group window
  Win=CreateWindow("Combobox","",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,
    15+Wid,y2,page_w-10-(15+Wid),200,CtrlParent,(HMENU)(base+IDC_CONNECTTO),hInstance,NULL);
  CreateWindow("Static",T("Connect to"),WS_CHILD|WS_VISIBLE|SS_CENTERIMAGE,10,
    y2,Wid,CharHeight,CtrlParent, (HMENU)(base+IDS_CONNECTTO),hInstance,NULL);
  CBAddString(Win,T("None"),PORTTYPE_NONE);
#if defined(SSE_DONGLE_PORT)
  if(port==TSTPort::DONGLE)
  {
#if defined(SSE_DONGLE_LEADERBOARD)
    CBAddString(Win,T("10th Frame"),TDongle::TENTHFRAME);
#endif
#if defined(SSE_DONGLE_BAT2)
    CBAddString(Win,T("B.A.T II"),TDongle::BAT2);
#endif
#if 0 && defined(SSE_DONGLE_CRICKET)
    CBAddString(Win,T("Cricket Captain"),TDongle::CRICKET);
#endif
#if defined(SSE_DONGLE_LEADERBOARD)
    CBAddString(Win,T("Leader Board"),TDongle::LEADERBOARD);
#endif
#if defined(SSE_DONGLE_JEANNEDARC)
    CBAddString(Win,T("Jeanne d'Arc"),TDongle::JEANNEDARC);
#endif
#if defined(SSE_DONGLE_CRICKET)
    CBAddString(Win,T("Rugby Coach"),TDongle::RUGBY);
#endif
#if defined(SSE_DONGLE_CRICKET)
    CBAddString(Win,T("Multi Player Soccer Manager"),TDongle::SOCCER);
#endif
#if defined(SSE_DONGLE_MUSIC_MASTER)
    CBAddString(Win,T("Music Master"),TDongle::MUSIC_MASTER);
#endif
#if defined(SSE_DONGLE_PROSOUND)
    CBAddString(Win,T("Pro Sound Designer (WOD/LXS)"),TDongle::PROSOUND);
#endif
#if defined(SSE_DONGLE_MULTIFACE)
    CBAddString(Win,T("Multiface Cartridge switch"),TDongle::MULTIFACE);
#endif
#if defined(SSE_DONGLE_URC)
    CBAddString(Win,T("Ultimate Ripper Cartridge switch"),TDongle::URC);
#endif
#if defined(SSE_420R5)
    ToolAddWindow(ToolTip,Win,T("Some dongles are only emulated if option C1 is enabled"));
#endif
  }
  else
#endif
  {
    CBAddString(Win,T("MIDI Device"),PORTTYPE_MIDI);
    if(AllowLPT)
      CBAddString(Win,T("Parallel Port (LPT)"),PORTTYPE_PARALLEL);
    if(AllowCOM)
      CBAddString(Win,T("COM Port"),PORTTYPE_COM);
    CBAddString(Win,T("File output"),PORTTYPE_FILE);
    CBAddString(Win,T("Loopback (Output->Input)"),PORTTYPE_LOOP);
#if defined(SSE_NETWORK)
    CBAddString(Win,T("TCP/IP"),PORTTYPE_TCPIP);
#endif

  }
  if(CBSelectItemWithData(Win,STPort[port].Type)<0)
    SendMessage(Win,CB_SETCURSEL,0,0);

  // MIDI
  y2+=LineHeight;
  int y2reset=y2;
  Wid=get_text_width(T("Output device"));
  CreateWindow("Static",T("Output device"),WS_CHILD|SS_CENTERIMAGE,
    10,y2,Wid,CharHeight,CtrlParent,(HMENU)(base+IDS_MIDIOUTPUT),hInstance,NULL);
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,
    15+Wid,y2,page_w-10-(15+Wid),200,CtrlParent,(HMENU)(base+IDC_MIDIOUTPUT),hInstance,NULL);
  
#if defined(SSE_DIRECTMIDI)
  if(OPTION_DIRECTMUSIC)
  {
    TNotify myNotify(T("MIDI Device")); // it is slow!
    CBAddString(Win,CStrT("None"),-1);
    INT c=DirectMidiOut.GetNumPorts();
    INFOPORT PortInfo;
    for(INT n=0;n<c;n++)
    {
      DirectMidiOut.GetPortInfo(n,&PortInfo);
      if(PortInfo.dwClass==DMUS_PC_OUTPUTCLASS)
        CBAddString(Win,PortInfo.szPortDescription,n);
    }
    CBSelectItemWithData(Win,STPort[port].MIDIOutDevice);

    y2+=LineHeight;
    Wid=get_text_width(T("Input device"));
    CreateWindow("Static",T("Input device"),WS_CHILD|SS_CENTERIMAGE,
      10,y2,Wid,CharHeight,CtrlParent,(HMENU)(base+IDS_MIDIINPUT),hInstance,NULL);
    Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,
      15+Wid,y2,page_w-10-(15+Wid),200,CtrlParent,(HMENU)(base+IDC_MIDIINPUT),hInstance,NULL);
    CBAddString(Win,CStrT("None"),-1);
    c+=DirectMidiIn.GetNumPorts(); // notice +=
    for(INT n=0;n<c;n++)
    {
      DirectMidiIn.GetPortInfo(n,&PortInfo);
      if(PortInfo.dwClass==DMUS_PC_INPUTCLASS)
        CBAddString(Win,PortInfo.szPortDescription,n);
    }
    CBSelectItemWithData(Win,STPort[port].MIDIInDevice);
  } else
#endif//#if defined(SSE_DIRECTMIDI)
  {
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("None"));
    INT c=midiOutGetNumDevs();
    MIDIOUTCAPS moc;
    ZeroMemory(&moc,sizeof(MIDIOUTCAPS)); //just in case really
    // MIDI_MAPPER is (UINT)-1 = $FFFFFFFF, same in VS2015, bad for x64 build
    for(INT_PTR n=-1;n<c;n++) // use -1 instead of MIDI_MAPPER
    {
      midiOutGetDevCaps(n,&moc,sizeof(MIDIOUTCAPS));
      SendMessage(Win,CB_ADDSTRING,0,(LPARAM)moc.szPname);
    }
    SendMessage(Win,CB_SETCURSEL,STPort[port].MIDIOutDevice+2,0);
    y2+=LineHeight;
    Wid=get_text_width(T("Input device"));
    CreateWindow("Static",T("Input device"),WS_CHILD|SS_CENTERIMAGE,
      10,y2,Wid,CharHeight,CtrlParent,(HMENU)(base+IDS_MIDIINPUT),hInstance,NULL);
    Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,
      15+Wid,y2,page_w-10-(15+Wid),200,CtrlParent,(HMENU)(base+IDC_MIDIINPUT),hInstance,NULL);
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("None"));
    c=midiInGetNumDevs();
    MIDIINCAPS mic;
    ZeroMemory(&mic,sizeof(MIDIINCAPS));
    for(INT_PTR n=0;n<c;n++)
    {
      midiInGetDevCaps(n,&mic,sizeof(MIDIINCAPS));
      SendMessage(Win,CB_ADDSTRING,0,(LPARAM)mic.szPname);
    }
    SendMessage(Win,CB_SETCURSEL,STPort[port].MIDIInDevice+1,0);
  }

  //Parallel
  y2=y2reset;
  Wid=get_text_width(T("Select port"));
  CreateWindow("Static",T("Select port"),WS_CHILD|SS_CENTERIMAGE,page_w/2-(Wid
    +GUIMUL(72))/2,y2,Wid,CharHeight,CtrlParent,(HMENU)(base+IDS_PARALLELOUTPUT),hInstance,NULL);
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST,page_w/2-
    (Wid+GUIMUL(72))/2+Wid+5,y2,GUIMUL(68),200,CtrlParent,(HMENU)(base+IDC_PARALLELOUTPUT),
    hInstance,NULL);
  for(int n=1;n<10;n++) 
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)((EasyStr("LPT")+n).Text));
  SendMessage(Win,CB_SETCURSEL,STPort[port].LPTNum,0);

  //COM
  Wid=get_text_width(T("Select port"));
  CreateWindow("Static",T("Select port"),WS_CHILD,page_w/2-(Wid+GUIMUL(72))/2,
    y2,Wid,CharHeight,CtrlParent,(HMENU)(base+IDS_COMOUTPUT),hInstance,NULL);
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST,page_w/2-
    (Wid+GUIMUL(72))/2+Wid+5,y2,GUIMUL(68),200,CtrlParent,(HMENU)(base+IDC_COMOUTPUT),
    hInstance,NULL);
  for(int n=1;n<10;n++) 
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)((EasyStr("COM")+n).Text));
  SendMessage(Win,CB_SETCURSEL,STPort[port].COMNum,0);

#if defined(SSE_NETWORK)
  //Internet
  Wid=get_text_width(T("Address"));
  CreateWindow("Static",T("Address"),WS_CHILD,10,
    y2,Wid,CharHeight,CtrlParent,(HMENU)(base+IDS_IPSTRING),hInstance,NULL);
  Win=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER|WS_VISIBLE|ES_AUTOHSCROLL,Wid+
    15,y2,page_w-Wid-20-10,CharHeight,CtrlParent,(HMENU)(base+IDC_IPSTRING),hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Address of the server (Steem SSE running on another computer)\
 or SERVER to be the server"));
  SendMessage(Win,WM_SETTEXT,STPort[port].sIPAddr.Length(),(LPARAM)STPort[port].sIPAddr.Text);
  y2+=LineHeight;

  Wid=get_text_width(T("Port"));
  CreateWindow("Static",T("Port"),WS_CHILD,10,
    y2,Wid,CharHeight,CtrlParent,(HMENU)(base+IDS_IPPORT),hInstance,NULL);
  HWND hEdit0=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Wid+15,y2,CbUnits*3,CharHeight,CtrlParent,(HMENU)(base+IDC_IPPORT0),hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,CtrlParent,(HMENU)(base+IDC_IPPORT),hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit0,0);
  //SendMessageW(Win,UDM_SETRANGE32,1024,49151);
  SendMessageW(Win,UDM_SETRANGE32,1,0xffff); // full range
  SendMessageW(Win,UDM_SETPOS32,0,STPort[port].IPPort);

  int x0=Wid+15+CbUnits*3+15;

  Wid=get_text_width(T("Clients"));
  CreateWindow("Static",T("Clients"),WS_CHILD,x0,
    y2,Wid,CharHeight,CtrlParent,(HMENU)(base+IDS_NCLIENTS),hInstance,NULL);
  hEdit0=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,Wid+x0+5,y2,
    CbUnits*2,CharHeight,CtrlParent,(HMENU)(base+IDC_NCLIENTS0),hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,CtrlParent,(HMENU)(base+IDC_NCLIENTS),hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit0,0);
  SendMessageW(Win,UDM_SETRANGE32,1,MAXCLIENT_SOCKETS);
  SendMessageW(Win,UDM_SETPOS32,0,STPort[port].MaxClients);

  x0+=CbUnits*2+Wid+15;

  Wid=get_text_width(T("Not connected"));
  CreateWindow("Static",T("Not connected"),WS_CHILD,x0,
    y2,Wid,CharHeight,CtrlParent,(HMENU)(base+IDS_IPSTATUS),hInstance,NULL);

  y2=y2reset;
#endif//#if defined(SSE_NETWORK)

  //File
  CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Path Display",STPort[port].File,WS_CHILD,
    10,y2,page_w-20,CharHeight,CtrlParent,(HMENU)(base+IDS_FILEOUTPUT),hInstance,NULL);
  y2+=LineHeight;
  CreateWindow("Button",T("Change File"),WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE,
    10,y2,page_w/2-15,CharHeight,CtrlParent,(HMENU)(base+IDC_FILECHANGE),hInstance,NULL);
  CreateWindow("Button",T("Reset Current File"),WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE,
    page_w/2+5,y2,page_w/2-15,CharHeight,CtrlParent,(HMENU)(base+IDC_FILERESET),hInstance,NULL);
  
  // Disabled (parallel only)
  if(port==TSTPort::PARALLEL) 
  {
    CreateWindow("Steem Path Display",T("Disabled due to parallel joystick"),
      WS_CHILD|PDS_VCENTRESTATIC,10,20,page_w-20,CharHeight,CtrlParent,(HMENU)IDS_STATIC,
      hInstance,NULL);
  }
  
  SetWindowAndChildrensFont(CtrlParent,Font);
  PortsMakeTypeVisible(port);

#endif//WIN32

}


void TOptionBox::CreatePortsPage() {

  int &y=mY,&Offset=mOffset;

#ifdef WIN32
  for(int p=TSTPort::PARALLEL;p<NSTPORTS;p++) // MIDI on another page now (v410)
    CreatePortOptions(p,y);
  HWND Win;
  y-=LineHeight*2;
  Offset+=HorizontalSeparation*2;
#if defined(SSE_PRINTER)
  Win=CreateButton(T("SMM804 Printer (Parallel port)"),IDC_PRINTER);
  SendMessage(Win,BM_SETCHECK,OPTION_PRINTER,0);
  ToolAddWindow(ToolTip,Win,T("Epson-compatible"));
#endif
  NextLine();
  Offset+=HorizontalSeparation*2;
#if defined(SSE_ACSI_LASER)
  Win=CreateButton(T("SLM804 Laser Printer (ACSI port)"),IDC_LASERPRINTER);
  SendMessage(Win,BM_SETCHECK,OPTION_LASER,0);
  ToolAddWindow(ToolTip,Win,T("Print on the legend!"));
#endif

#endif//WIN32

#ifdef UNIX

#if defined(SSE_DONGLE_PORT) // must squeeze
  int h0=15+90+5;
  int h1=h0+5;
  int h2=15;
#else
  int h0=25+90+5;
  int h1=h0+10;
  int h2=25;
#endif

  PortGroup[TSTPort::MIDI].create(XD,page_p,page_l,y,page_w,h0,NULL,this,BT_GROUPBOX,
    T("MIDI Port"),0,BkCol);
  y+=h1;
  PortGroup[TSTPort::PARALLEL].create(XD,page_p,page_l,y,page_w,h0,NULL,this,BT_GROUPBOX,
    T("Parallel Port"),0,BkCol);
  y+=h1;
  PortGroup[TSTPort::SERIAL].create(XD,page_p,page_l,y,page_w,h0,NULL,this,BT_GROUPBOX,
    T("Serial Port"),0,BkCol);
#if defined(SSE_DONGLE_PORT)
  y+=h1;
  PortGroup[TSTPort::DONGLE].create(XD,page_p,page_l,y,page_w,43,NULL,this,BT_GROUPBOX,
    T("Special Adapter"),0,BkCol);  
#endif
  for(int p=0;p<NSTPORTS;p++)
  {
    int IDBase=1200+p*20;
    y=h2;
    ConnectLabel[p].create(XD,PortGroup[p].handle,10,y,0,25,NULL,this,BT_LABEL,
      T("Connect to"),0,BkCol);
    ConnectDD[p].make_empty();
    ConnectDD[p].additem(T("None"),PORTTYPE_NONE);
#if defined(SSE_DONGLE_PORT)
    if(p==TSTPort::DONGLE)
    {
#if defined(SSE_DONGLE_LEADERBOARD)
    ConnectDD[p].additem(T("10th Frame"),TDongle::TENTHFRAME);
#endif
#if defined(SSE_DONGLE_BAT2)
    ConnectDD[p].additem(T("B.A.T II"),TDongle::BAT2);
#endif
#if 0 && defined(SSE_DONGLE_CRICKET)
    ConnectDD[p].additem(T("Cricket Captain"),TDongle::CRICKET);
#endif
#if defined(SSE_DONGLE_LEADERBOARD)
    ConnectDD[p].additem(T("Leader Board"),TDongle::LEADERBOARD);
#endif
#if defined(SSE_DONGLE_JEANNEDARC)
    ConnectDD[p].additem(T("Jeanne d'Arc"),TDongle::JEANNEDARC);
#endif
#if defined(SSE_DONGLE_CRICKET)
    ConnectDD[p].additem(T("Rugby Coach"),TDongle::RUGBY);
#endif
#if defined(SSE_DONGLE_CRICKET)
    ConnectDD[p].additem(T("Multi Player Soccer Manager"),TDongle::SOCCER);
#endif
#if defined(SSE_DONGLE_MUSIC_MASTER)
    ConnectDD[p].additem(T("Music Master"),TDongle::MUSIC_MASTER);
#endif
#if defined(SSE_DONGLE_PROSOUND)
    ConnectDD[p].additem(T("Pro Sound Designer (WOD/LXS)"),TDongle::PROSOUND);
#endif
#if defined(SSE_DONGLE_MULTIFACE)
    ConnectDD[p].additem(T("Multiface Cartridge switch"),TDongle::MULTIFACE);
#endif
#if defined(SSE_DONGLE_URC)
    ConnectDD[p].additem(T("Ultimate Ripper Cartridge switch"),TDongle::URC);
#endif
    }
    else
#endif      
    {
    ConnectDD[p].additem(T("MIDI Port Device"),PORTTYPE_MIDI);
//    ConnectDD[p].additem(T("MIDI Sequencer Device"),PORTTYPE_UNIX_SEQUENCER);
    if (AllowLPT) ConnectDD[p].additem(T("Parallel Port Device"),PORTTYPE_PARALLEL);
    if (AllowCOM) ConnectDD[p].additem(T("Serial Port Device"),PORTTYPE_COM);
    ConnectDD[p].additem(T("Named Pipes"),PORTTYPE_LAN);
    ConnectDD[p].additem(T("Other Device"),PORTTYPE_UNIX_OTHER);
    ConnectDD[p].additem(T("File output"),PORTTYPE_FILE);
    ConnectDD[p].additem(T("Loopback (Output->Input)"),PORTTYPE_LOOP);
    }
    ConnectDD[p].select_item_by_data(STPort[p].Type);
    ConnectDD[p].grandfather=page_p;
    ConnectDD[p].id=IDBase+0;
    ConnectDD[p].create(XD,PortGroup[p].handle,15+ConnectLabel[p].w,
      y,page_w-10-(15+ConnectLabel[p].w),200,dd_notify_proc,this);
    y+=LineHeight;
    IOGroup[p].create(XD,PortGroup[p].handle,10,y,PortGroup[p].w-20,60,NULL,this,BT_STATIC,"",0,BkCol);

    IOChooseBut[p].create(XD,IOGroup[p].handle,IOGroup[p].w,0,0,25,button_notify_proc,this,BT_TEXT,T("Choose"),IDBase+1,BkCol);
    IOChooseBut[p].x-=IOChooseBut[p].w;
    XMoveResizeWindow(XD,IOChooseBut[p].handle,IOChooseBut[p].x,IOChooseBut[p].y,IOChooseBut[p].w,IOChooseBut[p].h);

    IODevEd[p].create(XD,IOGroup[p].handle,0,0,IOChooseBut[p].x-10,25,edit_notify_proc,this);
    IODevEd[p].id=IDBase+2;
    IOAllowIOBut[p][0].create(XD,IOGroup[p].handle,0,30,0,25,button_notify_proc,this,BT_CHECKBOX,T("Output"),IDBase+3,BkCol);
    IOAllowIOBut[p][1].create(XD,IOGroup[p].handle,IOGroup[p].w/3,30,0,25,button_notify_proc,this,BT_CHECKBOX,T("Input"),IDBase+4,BkCol);

    IOOpenBut[p].create(XD,IOGroup[p].handle,(IOGroup[p].w/3)*2,30,(IOGroup[p].w/3),25,button_notify_proc,this,BT_TEXT,T("Open"),IDBase+5,BkCol);

    //---------------------------------------------------------------------------
    LANGroup[p].create(XD,PortGroup[p].handle,10,y,PortGroup[p].w-20,60,NULL,this,BT_STATIC,"",0,BkCol);

    hxc_button *p_but=new hxc_button(XD,LANGroup[p].handle,LANGroup[p].w,0,0,55,button_notify_proc,this,BT_TEXT,T("Open"),IDBase+14,BkCol);
    p_but->x-=p_but->w;
    XMoveResizeWindow(XD,p_but->handle,p_but->x,p_but->y,p_but->w,p_but->h);
    int lan_wid=p_but->x-5;

    p_but=new hxc_button(XD,LANGroup[p].handle,lan_wid,0,0,25,button_notify_proc,this,BT_TEXT,T("Choose"),IDBase+11,BkCol);
    p_but->x-=p_but->w;
    XMoveResizeWindow(XD,p_but->handle,p_but->x,p_but->y,p_but->w,p_but->h);
    hxc_button *p_lab=new hxc_button(XD,LANGroup[p].handle,0,0,0,25,NULL,this,BT_LABEL,T("Output"),0,BkCol);
    hxc_edit *p_ed=new hxc_edit(XD,LANGroup[p].handle,p_lab->w+5,0,p_but->x-5-(p_lab->w+5),25,edit_notify_proc,this);
    p_ed->id=IDBase+10;
    p_but=new hxc_button(XD,LANGroup[p].handle,lan_wid,30,0,25,button_notify_proc,this,BT_TEXT,T("Choose"),IDBase+13,BkCol);
    p_but->x-=p_but->w;
    XMoveResizeWindow(XD,p_but->handle,p_but->x,p_but->y,p_but->w,p_but->h);
    p_lab=new hxc_button(XD,LANGroup[p].handle,0,30,0,25,NULL,this,BT_LABEL,T("Input"),0,BkCol);
    p_ed=new hxc_edit(XD,LANGroup[p].handle,p_lab->w+5,30,p_but->x-5-(p_lab->w+5),25,edit_notify_proc,this);
    p_ed->id=IDBase+12;

    //---------------------------------------------------------------------------
    FileGroup[p].create(XD,PortGroup[p].handle,10,y,PortGroup[p].w-20,60,NULL,this,BT_STATIC,"",0,BkCol);
    FileDisplay[p].create(XD,FileGroup[p].handle,0,0,FileGroup[p].w,25,NULL,this,BT_TEXT | BT_BORDER_INDENT | BT_STATIC | BT_TEXT_PATH,STPort[p].File,0,WhiteCol);

    FileChooseBut[p].create(XD,FileGroup[p].handle,0,30,FileGroup[p].w/2-5,25,button_notify_proc,this,BT_TEXT,T("Choose"),IDBase+6,BkCol);

    FileEmptyBut[p].create(XD,FileGroup[p].handle,FileGroup[p].w/2+5,30,FileGroup[p].w/2-5,25,button_notify_proc,this,BT_TEXT,T("Empty"),IDBase+7,BkCol);

    UpdatePortDisplay(p);
  }//next
#endif//UNIX
}


void TOptionBox::FullscreenBrightnessBitmap() {

#ifdef WIN32
#if defined(SSE_VID_2SCREENS)
  Disp.CheckCurrentMonitorConfig(Handle);
  int w=Disp.rcMonitor.right-Disp.rcMonitor.left;
  int h=Disp.rcMonitor.bottom-Disp.rcMonitor.top;
  WNDCLASS wc;
  wc.style=CS_DBLCLKS;
  wc.lpfnWndProc=Fullscreen_WndProc;
  wc.cbClsExtra=0;
  wc.cbWndExtra=0;
  wc.hInstance=hInstance;
  wc.hIcon=NULL;
  wc.hCursor=LoadCursor(NULL,IDC_ARROW);
  wc.hbrBackground=NULL;
  wc.lpszMenuName=NULL;
  wc.lpszClassName="Steem Temp Fullscreen Window";
  RegisterClass(&wc);
  HWND Win=CreateWindow("Steem Temp Fullscreen Window","",0,
    Disp.rcMonitor.left,Disp.rcMonitor.top,w,h,Handle,NULL,hInstance,NULL);
  SetWindowLong(Win,GWL_STYLE,0);
  HDC ScrDC=GetDC(NULL);
  HBITMAP hBmp=CreateCompatibleBitmap(ScrDC,w,h);
  ReleaseDC(NULL,ScrDC);
  DrawBrightnessBitmap(hBmp);
  SetProp(Win,"Bitmap",hBmp);
  ShowWindow(Win,SW_SHOW);
  bool DoneMouseUp=false;
  MSG mess;
  for(;;)
  {
    PeekMessage(&mess,Win,0,0,PM_REMOVE);
    DispatchMessage(&mess);
    short MouseBut=(GetKeyState(VK_LBUTTON)|GetKeyState(VK_RBUTTON)|GetKeyState(VK_MBUTTON));
    if(MouseBut>=0) 
      DoneMouseUp=true;
    if(MouseBut<0&&DoneMouseUp) 
      break;
  }
  RemoveProp(Win,"Bitmap");
  DestroyWindow(Win);
  DeleteObject(hBmp);
  UnregisterClass("Steem Temp Fullscreen Window",hInstance);
#else
  int w=GuiSM.cx_screen(),h=GuiSM.cy_screen();
  WNDCLASS wc;
  wc.style=CS_DBLCLKS;
  wc.lpfnWndProc=Fullscreen_WndProc;
  wc.cbClsExtra=0;
  wc.cbWndExtra=0;
  wc.hInstance=hInstance;
  wc.hIcon=NULL;
  wc.hCursor=LoadCursor(NULL,IDC_ARROW);
  wc.hbrBackground=NULL;
  wc.lpszMenuName=NULL;
  wc.lpszClassName="Steem Temp Fullscreen Window";
  RegisterClass(&wc);
  HWND Win=CreateWindow("Steem Temp Fullscreen Window","",0,
    0,0,w,h,Handle,NULL,hInstance,NULL);
  SetWindowLong(Win,GWL_STYLE,0);
  HDC ScrDC=GetDC(NULL);
  HBITMAP hBmp=CreateCompatibleBitmap(ScrDC,w,h);
  ReleaseDC(NULL,ScrDC);
  DrawBrightnessBitmap(hBmp);
  SetProp(Win,"Bitmap",hBmp);
  ShowWindow(Win,SW_SHOW);
  SetWindowPos(Win,HWND_TOPMOST,0,0,w,h,0);
  UpdateWindow(Win);
  bool DoneMouseUp=false;
  MSG mess;
  for(;;)
  {
    PeekMessage(&mess,Win,0,0,PM_REMOVE);
    DispatchMessage(&mess);
    short MouseBut=(GetKeyState(VK_LBUTTON)|GetKeyState(VK_RBUTTON)
      |GetKeyState(VK_MBUTTON));
    if(MouseBut>=0) 
      DoneMouseUp=true;
    if(MouseBut<0&&DoneMouseUp) 
      break;
  }
  RemoveProp(Win,"Bitmap");
  DestroyWindow(Win);
  DeleteObject(hBmp);
  UnregisterClass("Steem Temp Fullscreen Window",hInstance);
#endif
#endif//WIN32

#ifdef UNIX
  int sw=XDisplayWidth(XD,XDefaultScreen(XD));
  int sh=XDisplayHeight(XD,XDefaultScreen(XD));

  XSetWindowAttributes swa;
  swa.backing_store=NotUseful;
  swa.override_redirect=True;
  Window handle=XCreateWindow(XD,XDefaultRootWindow(XD),0,0,sw,sh,0,
                           CopyFromParent,InputOutput,CopyFromParent,
                           CWBackingStore | CWOverrideRedirect,&swa);

  SetProp(XD,handle,cWinProc,(DWORD_PTR)WinProc);
  SetProp(XD,handle,cWinThis,(DWORD_PTR)this);
  SetProp(XD,handle,hxc::cModal,(DWORD)0xffffffff);

  XSelectInput(XD,handle,KeyPressMask | KeyReleaseMask |
                            ButtonPressMask | ButtonReleaseMask |
                            FocusChangeMask | LeaveWindowMask);

  brightness_image=brightness_ig.NewIconImage(XD,sw,sh);
  DrawBrightnessBitmap(brightness_image);

  hxc_button *p_but=new hxc_button(XD,handle,0,0,sw,sh,NULL,this,
                      BT_STATIC | BT_ICON | BT_BORDER_NONE | BT_NOBACKGROUND,"",0,BkCol);
  p_but->set_icon(&brightness_ig,0);

  XMapWindow(XD,handle);
  XGrabPointer(XD,handle,False,ButtonPressMask | ButtonReleaseMask,
                GrabModeAsync,GrabModeAsync,None,None,CurrentTime);
  XEvent Ev;
  while(1){
    if (hxc::wait_for_event(XD,&Ev)){
      if (Ev.xany.window==handle){
        break;
      }else{
        ProcessEvent(&Ev);
      }
    }
  }
  hxc::destroy_children_of(handle);

  hxc::RemoveProp(XD,handle,cWinProc);
  hxc::RemoveProp(XD,handle,cWinThis);
  hxc::RemoveProp(XD,handle,hxc::cModal);
  hxc::kill_timer(handle,HXC_TIMER_ALL_IDS);
  XDestroyWindow(XD,handle);

  brightness_image=brightness_ig.NewIconImage(XD,136+136,120);
  DrawBrightnessBitmap(brightness_image);

#endif//UNIX
}


// Colour Control
void TOptionBox::CreateBrightnessPage() {

  int &y=mY;

#ifdef WIN32
  int mid=page_l+page_w/2;
  int w=((page_w-HorizontalSeparation*2)/16)*16;
  int h=OPTIONS_HEIGHT/3;
  RECT rc={mid-w/2,LineStart,mid+w/2,LineStart+h};
  AdjustWindowRectEx(&rc,WS_CHILD|SS_BITMAP,0,512);
  HWND Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Static","",WS_CHILD|SS_BITMAP|SS_NOTIFY,rc.left,
    rc.top,rc.right-rc.left,rc.bottom-rc.top,Handle,(HMENU)ID_BRIGHTNESS_MAP,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Click to view fullscreen"));
  CreateBrightnessBitmap(w,h);
  GetWindowRect(Win,&rc);
  POINT pt={0,0};
  ClientToScreen(Handle,&pt);
  y=(rc.bottom-pt.y)+LineStart;
  char tmp[30];
  CreateButton(T("Reset"),IDC_RESETCOLOURS,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);

  Win=CreateButton(T("Full spectrum"),IDC_FULLSPECTRUM);
  SendMessage(Win,BM_SETCHECK,SSEOptions.FullSpectrumPal,0);

  Win=CreateButton(T("B/W"),IDC_GREYSCREEN);
  SendMessage(Win,BM_SETCHECK,OPTION_GREYSCREEN,0);
  ToolAddWindow(ToolTip,Win,T("Black and white TV emulation"));
  Win=CreateButton(T("Green"),IDC_GREENSCREEN); // we don't do radio buttons for this
  SendMessage(Win,BM_SETCHECK,OPTION_GREENSCREEN,0);
  ToolAddWindow(ToolTip,Win,T("Emulation of the Philips CM8833 Green button"));

  NextLine();
  CreateWindow("Static",T("Brightness"),WS_CHILD|SS_CENTER,
    page_l,y,page_w,CharHeight,Handle,(HMENU)IDS_STATIC,hInstance,NULL);
  y+=CharHeight;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,
    page_l,y,page_w,mSliderHeight,Handle,(HMENU)IDC_BRIGHTNESS,hInstance,NULL);
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(-128,127));
  // go around Windows "optimisation" (if 0)
  // https://forums.codeguru.com/showthread.php?236599-problems-with-CSliderCtrl&p=700244#post700244
  SendMessage(Win,TBM_SETPOS,1,1);
  SendMessage(Win,TBM_SETPOS,1,col_brightness);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETPAGESIZE,0,10);
  SendMessage(Win,TBM_SETTIC,0,0);
  y+=mSliderHeight;

  CreateWindow("Static",T("Contrast"),WS_CHILD|SS_CENTER,
    page_l,y,page_w,CharHeight,Handle,(HMENU)IDS_STATIC,hInstance,NULL);
  y+=CharHeight;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,
    page_l,y,page_w,mSliderHeight,Handle,(HMENU)IDC_CONTRAST,hInstance,NULL);

  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(-128,127));
  SendMessage(Win,TBM_SETPOS,1,1);
  SendMessage(Win,TBM_SETPOS,1,col_contrast);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETPAGESIZE,0,10);
  SendMessage(Win,TBM_SETTIC,0,0);
  y+=mSliderHeight;

  BYTE nsliders=3;
  SIMPLE_SETTINGS_BEGIN
  nsliders=1; // single gamma slider
  ALL_SETTINGS_END
  for(INT_PTR i=0;i<nsliders;i++)
  {
    if(nsliders>1)
      sprintf(tmp,"%s %s",T("Gamma").Text,T(rgb_txt[i]).Text); // red, green, blue
    else
      sprintf(tmp,"%s",T("Gamma").Text); // all colours
    CreateWindow("Static",tmp,WS_CHILD|SS_CENTER,
      page_l,y,page_w,CharHeight,Handle,(HMENU)(IDS_STATIC),hInstance,NULL);
    y+=CharHeight;
    
    Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,
      page_l,y,page_w,mSliderHeight,Handle,(HMENU)(IDC_GAMMA+i),hInstance,NULL);
    SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(-127,128));
    SendMessage(Win,TBM_SETPOS,1,1);
    SendMessage(Win,TBM_SETPOS,1,col_gamma[i]);
    SendMessage(Win,TBM_SETLINESIZE,0,1);
    SendMessage(Win,TBM_SETPAGESIZE,0,10);
    SendMessage(Win,TBM_SETTIC,0,0);
    y+=mSliderHeight;
  }

#endif//WIN32

#ifdef UNIX

  make_palette_table(col_brightness,col_contrast);
  brightness_image=brightness_ig.NewIconImage(XD,136+136,120-40);
  DrawBrightnessBitmap(brightness_image);

  brightness_picture.set_icon(&brightness_ig,0);
  brightness_picture.create(XD,page_p,page_l+page_w/2-137,y,137+137,122-40,
    button_notify_proc,this,BT_STATIC|BT_ICON|BT_BORDER_INDENT|BT_NOBACKGROUND,
    "",122,BkCol);
  hints.add(brightness_picture.handle,T("Click to view fullscreen"),page_p);
  y+=125-40;
/* //no room!
  brightness_picture_label.create(XD,page_p,page_l,y,page_w,25,NULL,this,BT_STATIC | BT_TEXT | BT_BORDER_NONE | BT_TEXT_CENTRE,
                        T("There should be 16 vertical strips (one black)"),0,BkCol);
  y+=25;
*/
  brightness_label.create(XD,page_p,page_l,y,page_w,25,NULL,this,BT_STATIC
    |BT_TEXT|BT_BORDER_NONE|BT_TEXT_CENTRE,"",0,BkCol);
  y+=LineHeight;

  brightness_sb.horizontal=true;
  brightness_sb.init(256+10,10,col_brightness+128);

  brightness_sb.create(XD,page_p,page_l,y,page_w,25,scrollbar_notify_proc,this);
  brightness_sb.id=10;
  y+=LineHeight;

  contrast_label.create(XD,page_p,page_l,y,page_w,25,NULL,this,BT_STATIC|BT_TEXT
    |BT_BORDER_NONE|BT_TEXT_CENTRE,"",0,BkCol);
  y+=LineHeight;

  contrast_sb.horizontal=true;
  contrast_sb.init(256+10,10,col_contrast+128);
  contrast_sb.create(XD,page_p,page_l,y,page_w,25,scrollbar_notify_proc,this);
  contrast_sb.id=11;

  y+=LineHeight;

  // gamma control RGB
  for(int i=0;i<3;i++)
  {
    gamma_label[i].create(XD,page_p,page_l,y,page_w,25,NULL,this,BT_STATIC
      |BT_TEXT|BT_BORDER_NONE|BT_TEXT_CENTRE,"",0,BkCol);
    y+=LineHeight;
    gamma_sb[i].horizontal=true;
    gamma_sb[i].init(256+10,10,col_gamma[i]+128);
    gamma_sb[i].create(XD,page_p,page_l,y,page_w,25,scrollbar_notify_proc,this);
    gamma_sb[i].id=12+i;
    y+=LineHeight;
  }

  scrollbar_notify_proc(&contrast_sb,SBN_SCROLL,contrast_sb.pos); // update the label text

#endif//UNIX
}


void TOptionBox::CreateDisplayPage() {
  
  int &y=mY,&Offset=mOffset,&Wid=mWid;

#ifdef WIN32

  HWND Win;
  int mask;

  CreateStatic(T("Frameskip"));
  Wid=CbUnits*4;
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,Offset,y,
                   Wid,200,Handle,(HMENU)IDC_FRAMESKIP,hInstance,NULL);
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("None"));
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Draw 1/2"));
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Draw 1/3"));
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Draw 1/4"));
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Auto"));
  SendMessage(Win,CB_SETCURSEL,MIN(frameskip-1,4),0);
  Offset+=Wid+HorizontalSeparation;
  ToolAddWindow(ToolTip,Win,T("None is the best"));

#if defined(SSE_GUI_421_REORDER)
  CreateStatic("Border");
  Win=CreateWindow("Combobox","",WS_CHILD  | WS_TABSTOP | CBS_DROPDOWNLIST,
                   Offset,y,CbUnits*5,200,Handle,(HMENU)IDC_CB_BORDER,hInstance,NULL);
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Off"));
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Normal"));
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Large"));
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Max"));
  SendMessage(Win,CB_SETCURSEL,border,0);
  NextLine();

#if defined(SSE_VID_D3D)
  CreateStatic(T("Stretch Filter"));
  mask=WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL;
  
  Win=CreateWindow("Combobox","",mask,Offset,y,Wid=CbUnits*5,200,Handle,
    (HMENU)IDC_TEXTUREFILTER,hInstance,NULL);
  CBAddString(Win,T("None"),D3DTEXF_NONE);
  CBAddString(Win,T("Point"),D3DTEXF_POINT);
  CBAddString(Win,T("Linear"),D3DTEXF_LINEAR);
  CBAddString(Win,T("Anisotropic"),D3DTEXF_ANISOTROPIC); //3
  CBAddString(Win,T("Pyramidal quad"),D3DTEXF_PYRAMIDALQUAD); //6
  CBAddString(Win,T("Gaussian quad"),D3DTEXF_GAUSSIANQUAD);
#if !defined(D3D_DISABLE_9EX)
  CBAddString(Win,T("Convolution"),D3DTEXF_CONVOLUTIONMONO);
#endif
  SendMessage(Win,CB_SETCURSEL,Disp.TextureFilter,0);
#endif

  Offset+=Wid+HorizontalSeparation;

  CreateStatic(T("Timing Loop (ms)"));
  Wid=CbUnits*2;
  HWND hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,Wid,CharHeight,Handle,(HMENU)IDC_TIMINGLOOP0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_TIMINGLOOP,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(20,0));
  SendMessageW(Win,UDM_SETPOS32,0,OPTION_TIMINGLOOP);
  ToolAddWindow(ToolTip,Win,T("Polling time for video rendering timings\
 1ms is legacy, 2ms is recommended, more will drain your CPU")
 +T("\nIrrelevant if VSync or frameskip are engaged"));

  NextLine();
  mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX;

// this option is not relevant when STVL is enabled
  Win=CreateButton(T("Scanline buffer"),IDC_DRAWBUFFER,WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX
                   | (OPTION_C3?WS_DISABLED:0) );
  SendMessage(Win,BM_SETCHECK,GetCSFInt("Options","DrawToVidMem",Disp.DrawToVidMem,globalINIFile),0);
    ToolAddWindow(ToolTip,Win,T("Each scanline is rendered first in a buffer\
 before being copied to the video memory\nThis can impact performance both ways"));

#if defined(SSE_TIMINGS_US)
  Win=CreateButton("Microseconds",IDC_MICROSECONDS);
  SendMessage(Win,BM_SETCHECK,OPTION_MICROSECONDS,0);
  ToolAddWindow(ToolTip,Win,T("Compute video rendering timings using a more\
 precise counter, useful if you have FreeSync or G-Sync hardware")
 +T("\nIrrelevant if VSync or frameskip are engaged"));
#endif

#if defined(SSE_VID_OLDSYNC)
  ALL_SETTINGS_BEGIN
  Win=CreateButton("Old sync",IDC_OLDSYNC);
  SendMessage(Win,BM_SETCHECK,SSEOptions.OldSync,0);
  ToolAddWindow(ToolTip,Win,T("If you prefer the results of older versions\n\
Makes hard loop and microseconds options irrelevant"));
  ALL_SETTINGS_END
#endif
  NextLine();
  Win=CreateButton(T("Aspect ratio"),IDC_STASPECTRATIO);
  SendMessage(Win,BM_SETCHECK,OPTION_ST_ASPECT_RATIO,0);
  ToolAddWindow(ToolTip,Win,
    T("Reproduces the familiar vertical distortion on standard colour screens"));

  Win=CreateButton(T("Scanlines"),IDC_SCANLINES);
  SendMessage(Win,BM_SETCHECK,OPTION_SCANLINES,0);
  ToolAddWindow(ToolTip,Win,T("Reproduces scanlines of cathodic screens"));

#if defined(SSE_VID_SINGLEPIX)
  Win=CreateButton(T("Pixels"),IDC_SINGLEPIXELS);
  SendMessage(Win,BM_SETCHECK,SSEOptions.SinglePixels,0);
  ToolAddWindow(ToolTip,Win,T("Same as scanlines but vertical"));
#endif
  //NextLine();
#if defined(SSE_VID_D3D_SWEETFX)
  Win=CreateButton(T("CRT"),IDC_CTR_EMU,mask|ENABLE_CHECK(Disp.hD3Dhack));
  SendMessage(Win,BM_SETCHECK,OPTION_CRT_EMU,0);
  ToolAddWindow(ToolTip,Win,T("Cathode Ray Tube screen emulation using SweetFX by CeeJay.dk"));
#endif
  
//  Offset=align;
#if defined(SSE_VID_BFI)
  Win=CreateButton("BFI",IDC_BFI);
  SendMessage(Win,BM_SETCHECK,OPTION_BFI,0);
  ToolAddWindow(ToolTip,Win,T("Software Black Frame Insertion - VSync or Microseconds"));
#endif

#else//#if defined(SSE_GUI_421_REORDER)

  mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX;
    Win=CreateButton(T("Reset Video"),IDC_RESET_DISPLAY,WS_CHILD|WS_TABSTOP|BS_CHECKBOX
    |BS_PUSHLIKE);
  ToolAddWindow(ToolTip,Win,T("It's hopeless, better restart Steem")); // sometimes it works!
  NextLine();
  
  // this option is not relevant when STVL is enabled
  Win=CreateButton(T("Use video buffer"),IDC_DRAWBUFFER,WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX
                   | (OPTION_C3?WS_DISABLED:0) );
  SendMessage(Win,BM_SETCHECK,GetCSFInt("Options","DrawToVidMem",Disp.DrawToVidMem,globalINIFile),0);
    ToolAddWindow(ToolTip,Win,T("Each scanline is rendered first in a buffer\
 before being copied to the video memory\nThis can impact performance both ways"));

#if defined(SSE_TIMINGS_US)
  Win=CreateButton("Microseconds",IDC_MICROSECONDS);
  SendMessage(Win,BM_SETCHECK,OPTION_MICROSECONDS,0);
  ToolAddWindow(ToolTip,Win,T("Compute video rendering timings using a more\
 precise counter, useful if you have FreeSync or G-Sync hardware")
 +T("\nIrrelevant if VSync or frameskip are engaged"));
#endif

#if defined(SSE_VID_BFI)
  Win=CreateButton("BFI",IDC_BFI);
  SendMessage(Win,BM_SETCHECK,OPTION_BFI,0);
  ToolAddWindow(ToolTip,Win,T("Software Black Frame Insertion - VSync or Microseconds"));
#endif

  NextLine();
  CreateStatic(T("Timing hard loop (ms)"));
  Wid=CbUnits*2;
  HWND hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,Wid,CharHeight,Handle,(HMENU)IDC_TIMINGLOOP0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_TIMINGLOOP,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(20,0));
  SendMessageW(Win,UDM_SETPOS32,0,OPTION_TIMINGLOOP);
  ToolAddWindow(ToolTip,Win,T("Polling time for video rendering timings\
 1ms is legacy, 2ms is recommended, more will drain your CPU")
 +T("\nIrrelevant if VSync or frameskip are engaged"));

#if defined(SSE_VID_D3D)
  NextLine();
  CreateStatic(T("Stretch Filter"));
  mask=WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL;
  
  Win=CreateWindow("Combobox","",mask,Offset,y,Wid=CbUnits*5,200,Handle,
    (HMENU)IDC_TEXTUREFILTER,hInstance,NULL);
  CBAddString(Win,T("None"),D3DTEXF_NONE);
  CBAddString(Win,T("Point"),D3DTEXF_POINT);
  CBAddString(Win,T("Linear"),D3DTEXF_LINEAR);
  CBAddString(Win,T("Anisotropic"),D3DTEXF_ANISOTROPIC); //3
  CBAddString(Win,T("Pyramidal quad"),D3DTEXF_PYRAMIDALQUAD); //6
  CBAddString(Win,T("Gaussian quad"),D3DTEXF_GAUSSIANQUAD);
#if !defined(D3D_DISABLE_9EX)
  CBAddString(Win,T("Convolution"),D3DTEXF_CONVOLUTIONMONO);
#endif
  SendMessage(Win,CB_SETCURSEL,Disp.TextureFilter,0);
#endif

#endif//#if defined(SSE_GUI_421_REORDER)

  NextLine();
  int groupheight=(OPTION_ADVANCED)?(GROUP_HEIGHT(5)):(GROUP_HEIGHT(3));
  CreateWindow("Button",T("Windowed Mode"),WS_CHILD|BS_GROUPBOX,
    page_l,y,page_w,groupheight,Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
  y+=mGroupTitleHeight;
  Offset+=LineStart;

  ALL_SETTINGS_BEGIN

  CreateStatic(T("Low resolution"));
  int o=Offset;
  int w=page_w-Offset+page_l-LineStart;
  mask=WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL;
#ifndef NO_CRAZY_MONITOR
  if(extended_monitor)
    mask|=WS_DISABLED;
#endif
  Win=CreateWindow("Combobox","",mask,o,y,w,200,Handle,(HMENU)IDC_SIZELORES,hInstance,NULL);
  CBAddString(Win,T("Small (1:1)"),0);
  CBAddString(Win,T("Normal (2:2)")+" - "+T("Stretch"),1);
  CBAddString(Win,T("Normal (2:2)")+" - "+T("No Stretch"),MAKELONG(1,DWM_NOSTRETCH));
  CBAddString(Win,T("Big (3:3)")+" - "+T("Stretch"),2);
  CBAddString(Win,T("Big (3:3)")+" - "+T("No Stretch"),MAKELONG(2,DWM_NOSTRETCH));
#if defined(SSE_VID_SIZE4)
  CBAddString(Win,T("Biggest (4:4)")+" - "+T("Stretch"),3);
  CBAddString(Win,T("Biggest (4:4)")+" - "+T("No Stretch"),MAKELONG(3,DWM_NOSTRETCH));
#else
  CBAddString(Win,T("Quadruple"),3);
#endif
  
  NextLine();
  Offset+=LineStart;
  CreateStatic(T("Medium"));
  Win=CreateWindow("Combobox","",mask,o,y,w,200,Handle,(HMENU)IDC_SIZEMEDRES,hInstance,NULL);
  if(OPTION_LOCK_AR)
    CBAddString(Win,T("Small (0.5:1)")+" - "+T("Shrink"),0);
  else
    CBAddString(Win,T("Small (1:1)"),0);
  CBAddString(Win,T("Normal (1:2)")+" - "+T("Stretch"),1);
  CBAddString(Win,T("Normal (1:2)")+" - "+T("No Stretch"),MAKELONG(1,DWM_NOSTRETCH));
#if defined(SSE_VID_SIZE4)
  if(OPTION_LOCK_AR)
  {
    CBAddString(Win,T("Big (1.5:3)")+" - "+T("Stretch"),2);
    CBAddString(Win,T("Big (1.5:3)")+" - "+T("Shrink"),MAKELONG(2,DWM_NOSTRETCH));
  }
  else
  {
    CBAddString(Win,T("Big (2:2)")+" - "+T("Stretch"),2);
    CBAddString(Win,T("Big (2:2)")+" - "+T("No Stretch"),MAKELONG(2,DWM_NOSTRETCH));
  }
  CBAddString(Win,T("Biggest (2:4)")+" - "+T("Stretch"),3);
  CBAddString(Win,T("Biggest (2:4)")+" - "+T("No Stretch"),MAKELONG(3,DWM_NOSTRETCH));
#else
  CBAddString(Win,T("Double (2:2)"),2);
  CBAddString(Win,T("Quadruple Height (2:4)"),3);
#endif

  NextLine();
  Offset+=LineStart;
  CreateStatic(T("High"));
  Win=CreateWindow("Combobox","",mask,o,y,w,200,Handle,(HMENU)IDC_SIZEHIRES,hInstance,NULL);
  if(OPTION_LOCK_AR)
    CBAddString(Win,T("Small (0.5:0.5)")+" - "+T("Stretch down"),0);
  CBAddString(Win,T("Normal (1:1)"),MAKELONG(1,DWM_NOSTRETCH));
#if defined(SSE_VID_SIZE4)
  if(OPTION_LOCK_AR)
  {
    CBAddString(Win,T("Big (1.5:1.5)")+" - "+T("Stretch"),2);
    CBAddString(Win,T("Big (1.5:1.5)")+" - "+T("Shrink"),MAKELONG(2,DWM_NOSTRETCH));
  }
  CBAddString(Win,T("Biggest (2:2)")+" - "+T("Stretch"),3);
  CBAddString(Win,T("Biggest (2:2)")+" - "+T("No Stretch"),MAKELONG(3,DWM_NOSTRETCH));
#else
  CBAddString(Win,T("Double Size"),1);
#endif

  ALL_SETTINGS_ELSE
  
#if !defined(SSE_GUI_421_REORDER)

  mask=WS_CHILD|BS_AUTORADIOBUTTON;
#ifndef NO_CRAZY_MONITOR
  if(extended_monitor)
    mask|=WS_DISABLED;
#endif
  Win=CreateButton(T("Small"),(IDC_RADIO_DISPLAY_SIZE+1),mask|WS_GROUP);
  Win=CreateButton(T("Normal"),(IDC_RADIO_DISPLAY_SIZE+2),mask);
  Win=CreateButton(T("Big"),(IDC_RADIO_DISPLAY_SIZE+3),mask);
  Win=CreateButton(T("Biggest"),(IDC_RADIO_DISPLAY_SIZE+4),mask);
#if defined(SSE_420R5) // BM_CLICK triggers handling in options.cpp
  PostMessage(GetDlgItem(Handle,IDC_RADIO_DISPLAY_SIZE+DISPLAY_SIZE),BM_CLICK,0,0);
#else
  SendMessage(GetDlgItem(Handle,IDC_RADIO_DISPLAY_SIZE+DISPLAY_SIZE),BM_SETCHECK,TRUE,0);
#endif
#endif

  ALL_SETTINGS_END

  NextLine();
  Offset+=LineStart;
#if defined(SSE_420R5) // Simple settings one size fits all res
  mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX|ENABLE_CHECK(OPTION_ADVANCED);
  Win=CreateButton(T("Automatic resize on resolution change"),IDC_AUTORESIZE,mask);
#else
  Win=CreateButton(T("Automatic resize on resolution change"),IDC_AUTORESIZE);
#endif
  SendMessage(Win,BM_SETCHECK,ResChangeResize,0);

#if !defined(SSE_GUI_421_REORDER)
#if defined(SSE_VID_OLDSYNC)
  ALL_SETTINGS_BEGIN
  Win=CreateButton("Old sync",IDC_OLDSYNC);
  SendMessage(Win,BM_SETCHECK,SSEOptions.OldSync,0);
  ToolAddWindow(ToolTip,Win,T("If you prefer the results of older versions\n\
Makes hard loop and microseconds options irrelevant"));
  ALL_SETTINGS_END
#endif
#endif

  NextLine();
  Offset+=LineStart;
  mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX;
  if(OPTION_BLOCK_RESIZE)
    mask|=WS_DISABLED;
  Win=CreateButton(T("Lock aspect ratio"),IDC_LOCKAR,mask);
  SendMessage(Win,BM_SETCHECK,OPTION_LOCK_AR,0);
  ToolAddWindow(ToolTip,Win,T("You can resize the window at will and Steem will keep the proportions"));


  SIMPLE_SETTINGS_BEGIN
  Win=CreateButton(T("Stretch"),IDC_STRETCH);
  SendMessage(Win,BM_SETCHECK,draw_stretch,0);
  ToolAddWindow(ToolTip,Win,T("Internal rendering mode, the picture can be\
 stretched anyway according to other settings"));
  ALL_SETTINGS_END

  Win=CreateButton("VSync",IDC_VSYNC);
  SendMessage(Win,BM_SETCHECK,OPTION_WIN_VSYNC,0);
  ToolAddWindow(ToolTip,Win,T("NOTE it syncs on your main display"));

#if defined(SSE_VID_D3D_VSYNC)
  Win=CreateButton("Auto VSync",IDC_AUTOVSYNC);
  SendMessage(Win,BM_SETCHECK,OPTION_AUTOVSYNC,0);
  ToolAddWindow(ToolTip,Win,T("VSync only if the PC frequency is appropriate\n")
    +T("NOTE it syncs on your main display"));
#endif


#if defined(SSE_VID_DD_3BUFFER_WIN) // DirectDraw-only, old systems
  ALL_SETTINGS_BEGIN
  Win=CreateButton(T("Triple Buffering"),IDC_TRIPLE_BUFFERING_WIN);
  SendMessage(Win,BM_SETCHECK,OPTION_3BUFFER_WIN,0);
  //ToolAddWindow(ToolTip,Win,T("For the window. High CPU use."));
  ALL_SETTINGS_END
#endif

#if !defined(SSE_NO_FREEIMAGE)
  EasyStringList format_sl;
  Disp.ScreenShotGetFormats(&format_sl);
  bool FIAvailable=(format_sl.NumStrings>2);
  NextLine();
  
  CreateWindow("Button",T("Screenshots"),WS_CHILD|BS_GROUPBOX,
    page_l,y,page_w,GROUP_HEIGHT(3),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
  y+=mGroupTitleHeight;
  Offset+=LineStart;
  CreateStatic(T("Folder"));
  CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Path Display",ScreenShotFol,WS_CHILD,Offset,
    y,page_w-Offset+page_l-LineStart,CharHeight,Handle,(HMENU)IDP_SCREENSHOTDIR,hInstance,NULL);
  NextLine();
  Offset+=LineStart;
  CreateButton(T("Choose"),IDC_SCREENSHOTCHOOSEDIR,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);
  CreateButton(T("Open"),IDC_SCREENSHOTOPENDIR,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);
  Win=CreateButton(T("Minimum size screenshots"),IDC_MINSCREENSHOT);
  SendMessage(Win,BM_SETCHECK,Disp.ScreenShotMinSize,0);
  ToolAddWindow(ToolTip,Win,T("This option, when checked, ensures all screenshots will be taken at the smallest size possible for the resolution.")+" "+
    T("WARNING: Some video cards may cause the screenshots to look terrible in certain drawing modes."));
  NextLine();
  Offset+=LineStart;
  CreateStatic(T("Format"));
  Wid=CbUnits*6;
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,Offset,y,
                   Wid,300,Handle,(HMENU)IDC_SCREENSHOT_FORMAT,hInstance,NULL);
  Offset+=Wid+HorizontalSeparation;
  for(int i=0;i<format_sl.NumStrings;i++)
    CBAddString(Win,format_sl[i].String,format_sl[i].Data[0]);
  INT_PTR n,c=SendMessage(Win,CB_GETCOUNT,0,0);
  for(n=0;n<c;n++)
    if(SendMessage(Win,CB_GETITEMDATA,n,0)==Disp.ScreenShotFormat) 
      break;
  if(n>=c)
  {
    Disp.ScreenShotFormat=FIF_BMP;
#if !defined(SSE_NO_FREEIMAGE)
    Disp.ScreenShotFormatOpts=0;
#endif
    Disp.ScreenShotExt="bmp";
    n=1;    
  }
  SendMessage(Win,CB_SETCURSEL,n,0);
#if !defined(SSE_NO_FREEIMAGE)
  if(FIAvailable)
  {
    CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,
      Offset,y,Wid,200,Handle,(HMENU)IDC_FI_SCREENSHOT_FORMAT,hInstance,NULL);
    FillScreenShotFormatOptsCombo();
  }
#endif
  NextLine();
  Offset+=LineStart;
#endif //#if !defined(SSE_GUI_NO2SCREENSHOT_SETTINGS)

  UpdateWindowSizeAndBorder();

#endif//WIN32

#ifdef UNIX
  fs_label.create(XD,page_p,page_l,y,0,25,
    NULL,this,BT_LABEL,T("Frameskip"),0,BkCol);

  frameskip_dd.make_empty();
  frameskip_dd.additem(T("Draw Every Frame"));
  frameskip_dd.additem(T("Draw Every Second Frame"));
  frameskip_dd.additem(T("Draw Every Third Frame"));
  frameskip_dd.additem(T("Draw Every Fourth Frame"));
  frameskip_dd.additem(T("Auto Frameskip"));
  frameskip_dd.changesel(MIN(frameskip-1,4));

  frameskip_dd.create(XD,page_p,page_l+5+fs_label.w,y,page_w-(5+fs_label.w),
    200,dd_notify_proc,this);
  y+=35;
#if !defined(SSE_VID_BORDERS)
  bo_label.create(XD,page_p,page_l,y,0,25,NULL,this,
    BT_LABEL,T("Border"),0,BkCol);

  border_dd.make_empty();
#if defined(SSE_VID_DISABLE_AUTOBORDER)
  border_dd.additem(T("Off"));
  border_dd.additem(T("On"));
  //border_dd.additem(T("Normal")); //TODO
  //border_dd.additem(T("Large"));
  //border_dd.additem(T("Max"));
#else
  border_dd.additem(T("Never Show Borders"));
  border_dd.additem(T("Always Show Borders"));
  border_dd.additem(T("Auto Borders"));
#endif
#if defined(SSE_VID_DISABLE_AUTOBORDER)
  border_dd.changesel(MIN(border,1));
#else
  border_dd.changesel(MIN(border,2));
#endif
  border_dd.create(XD,page_p,page_l+5+bo_label.w,y,
    page_w-(5+bo_label.w),210,dd_notify_proc,this);
  y+=35;
#endif
#ifndef SSE_BUILD
  hxc_button *p_but=new hxc_button(XD,page_p,page_l,y,0,25,button_notify_proc,this,
    BT_CHECKBOX,T("Scanline Grille"),210,BkCol);
  p_but->set_check(draw_fs_fx==DFSFX_GRILLE);
  y+=35;  
#endif
  hxc_button *p_but=new hxc_button(XD,page_p,page_l,y,0,25,button_notify_proc,this,
    BT_CHECKBOX,T("Asynchronous blitting (can be faster)"),220,BkCol);
  p_but->set_check(Disp.DoAsyncBlit);
  y+=35;
  
  hxc_button *p_but2=new hxc_button(XD,page_p,page_l,y,0,25,button_notify_proc,this,
    BT_CHECKBOX,T("Use video buffer"),4024,BkCol);
  p_but2->set_check(Disp.DrawToVidMem);
  hints.add(p_but2->handle,T("Each scanline is rendered first in a buffer\
 before being copied to the video memory This can impact performance both ways"),page_p);
  y+=35;

  {

#if defined(SSE_VID_SIZE4)

    size_group.create(XD,page_p,page_l,y,page_w,55,
      NULL,this,BT_STATIC | BT_TEXT | BT_BORDER_OUTDENT |
    BT_TEXT_VTOP,T("Window Size"),0,BkCol);


    DisplaySize_but[0].create(XD,size_group.handle,10,25,0,25,
      button_notify_proc,this,BT_CHECKBOX,T("Small"),250,BkCol);
      
    DisplaySize_but[1].create(XD,size_group.handle,10+80,25,0,25,
      button_notify_proc,this,BT_CHECKBOX,T("Normal"),251,BkCol);

    DisplaySize_but[2].create(XD,size_group.handle,10+80*2,25,0,25,
      button_notify_proc,this,BT_CHECKBOX,T("Big"),252,BkCol);

    DisplaySize_but[3].create(XD,size_group.handle,10+80*3,25,0,25,
      button_notify_proc,this,BT_CHECKBOX,T("Biggest"),253,BkCol);
      
    for(int i=0;i<4;i++)
      DisplaySize_but[i].set_check( (DISPLAY_SIZE==(1+i)) );
      
#else

    size_group.create(XD,page_p,page_l,y,page_w,120+30,
      NULL,this,BT_STATIC | BT_TEXT | BT_BORDER_OUTDENT |
    BT_TEXT_VTOP,T("Window Size"),0,BkCol);
  
  
    lowres_doublesize_but.create(XD,size_group.handle,10,25,0,25,
            button_notify_proc,this,BT_CHECKBOX,T("Low-res double size"),
            250,BkCol);
    lowres_doublesize_but.set_check(WinSizeForRes[LORES]);

    medres_doublesize_but.create(XD,size_group.handle,10,55,0,25,
            button_notify_proc,this,BT_CHECKBOX,T("Med-res double height"),
            251,BkCol);
    medres_doublesize_but.set_check(WinSizeForRes[MEDRES]);

    hxc_button *p_but=new hxc_button(XD,size_group.handle,10,115,0,25,
            //button_notify_proc,this,BT_CHECKBOX,T("Fullscreen 640x400 (never show borders only)"),
            button_notify_proc,this,BT_CHECKBOX,T("Fullscreen 640x400"),
            255,BkCol);
    p_but->set_check(prefer_res_640_400);
    hints.add(p_but->handle,T("When this option is ticked Steem will use the 600x400 PC screen resolution in fullscreen if it can"),page_p);


#endif

  }

  y+=55;
  hxc_button *p_but_scan=new hxc_button(XD,page_p,page_l,y,0,25,button_notify_proc,this,
    BT_CHECKBOX,T("Scanlines"),210,BkCol);
  p_but_scan->set_check(OPTION_SCANLINES);
  hints.add(p_but_scan->handle,T("Reproduces scanlines of cathodic screens"),page_p);

#if defined(SSE_VID_SINGLEPIX)
  hxc_button *p_but_pix=new hxc_button(XD,page_p,page_l+100,y,0,25,button_notify_proc,this,
    BT_CHECKBOX,T("Pixels"),211,BkCol);
  p_but_pix->set_check(SSEOptions.SinglePixels);
  hints.add(p_but_pix->handle,T("Same as scanlines but vertical"),page_p);
#endif
 
  y+=LineHeight;
  optionBW_but.create(XD,page_p,page_l,y,0,25,
    button_notify_proc,this,BT_CHECKBOX,T("B/W"),4022,BkCol);
  optionBW_but.set_check(OPTION_GREYSCREEN);
  hints.add(optionBW_but.handle,T("Black and white TV emulation"),page_p);

  optionGreen_but.create(XD,page_p,page_l+100,y,0,25,
    button_notify_proc,this,BT_CHECKBOX,T("Green"),4025,BkCol);
  optionGreen_but.set_check(OPTION_GREENSCREEN);
  hints.add(optionGreen_but.handle,T("Emulation of the Philips CM8833 Green button"),page_p);

  y+=LineHeight;
  vivid_but.create(XD,page_p,page_l,y,0,25,
    //button_notify_proc,this,BT_CHECKBOX,T("FullSpectrumPal colours"),4023,BkCol);
    button_notify_proc,this,BT_CHECKBOX,T("Full spectrum"),4023,BkCol);
  vivid_but.set_check(SSEOptions.FullSpectrumPal);

  screenshots_group.create(XD,page_p,page_l,y,page_w,60,
                        NULL,this,BT_STATIC | BT_TEXT | BT_BORDER_OUTDENT |
                        BT_TEXT_VTOP,T("Screenshots"),0,BkCol);

  screenshots_fol_label.create(XD,screenshots_group.handle,10,25,0,25,NULL,this,
                    BT_LABEL,T("Folder"),0,BkCol);

  screenshots_fol_but.create(XD,screenshots_group.handle,screenshots_group.w-10,
                    25,0,25,button_notify_proc,this,BT_TEXT,T("Choose"),254,BkCol);
  screenshots_fol_but.x-=screenshots_fol_but.w;
  XMoveWindow(XD,screenshots_fol_but.handle,
              screenshots_fol_but.x,screenshots_fol_but.y);

  screenshots_fol_display.create(XD,screenshots_group.handle,
                    15+screenshots_fol_label.w,25,
                    screenshots_fol_but.x-10-(15+screenshots_fol_label.w),25,NULL,this,
                    BT_STATIC | BT_BORDER_INDENT | BT_TEXT_PATH | BT_TEXT,
                    ScreenShotFol,0,WhiteCol);
  
#endif//UNIX
}


#ifdef WIN32

void TOptionBox::UpdateWindowSizeAndBorder() {
  if(!IsVisible())
    return;
  ALL_SETTINGS_BEGIN
  for(int res=LORES;res<=HIRES;res++) 
  {
    DWORD dat=WinSizeForRes[res];
#if !defined(SSE_VID_SIZE4)
    if(res<2)
#endif
      dat=MAKELONG(dat,draw_win_mode[res]);
    HWND Win=GetDlgItem(Handle,IDC_SIZELORES+(res<<1));
    LRESULT ec=CBSelectItemWithData(Win,dat);
    if(ec<0) // not found
    {
      dat^=0x10000; // toggle draw_win_mode...
      ec=CBSelectItemWithData(Win,dat);
    }
  }
  ALL_SETTINGS_ELSE
  for(int i=1;i<=4;i++) // check one, uncheck others
  {
    SendMessage(GetDlgItem(Handle,IDC_RADIO_DISPLAY_SIZE+i),BM_SETCHECK,(i==DISPLAY_SIZE),0);
  }
  ALL_SETTINGS_END
}

#endif//WIN32


void TOptionBox::CreateStartupPage() {

  int &y=mY,&Offset=mOffset/*,&Wid=mWid*/;
#ifdef WIN32
  TConfigStoreFile CSF(globalINIFile);
  HWND Win;
  BOOL NoDD=!!CSF.GetInt("Options","NoDirectDraw",0);

  CreateStatic(T("You need to leave and restart Steem for those options to take effect"));

#if defined(SSE_ONEINSTANCE)
  NextLine();
  Win=CreateButton(T("Only one Steem SSE"),IDC_ONE_INSTANCE);
  SendMessage(Win,BM_SETCHECK,CSF.GetInt("Main","OneInstance",0),0);
#endif

#if defined(SSE_GUI_BIGICONS)
  NextLine();
  Win=CreateButton(T("Big GUI"),IDC_BIGGUI);
  SendMessage(Win,BM_SETCHECK,BIG_ICONS,0);
  ToolAddWindow(ToolTip,Win,T("Bigger icons and font for those high DPI screens"));
#endif
  CreateStatic(T("Font size")); // there's a setting because I started with this
  HWND hEdit0=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,CbUnits*2,CharHeight,Handle,(HMENU)IDC_FONTSIZE0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS
    |UDS_SETBUDDYINT|UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_FONTSIZE1,
    hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit0,0);
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(64,1));
  SendMessageW(Win,UDM_SETPOS32,0,FONT_SIZE);

  NextLine();
  Win=CreateButton(T("Restore previous state"),IDC_RESTORESTATE);
  SendMessage(Win,BM_SETCHECK,AutoLoadSnapShot,0);
  ToolAddWindow(ToolTip,Win,
    T("When this is checked, Steem saves the state when leaving and loads it\
 when starting. Without a hiccup."));
  Offset+=HorizontalSeparation;

#if !defined(SSE_NO_AUTOSNAPSHOTEDIT)
  NextLine();
  CreateStatic(T("Filename"));
  Offset+=HorizontalSeparation;
  Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Edit",AutoSnapShotName,WS_CHILD|
    WS_TABSTOP|ES_AUTOHSCROLL,Offset,y,page_w-Offset+page_l,CharHeight,Handle,
    (HMENU)IDC_SNAPSHOTNAME,hInstance,NULL);
  SendMessage(Win,WM_SETFONT,(UINT_PTR)Font,0);
  SendMessage(Win,EM_LIMITTEXT,100,0);
  INT_PTR Len=SendMessage(Win,WM_GETTEXTLENGTH,0,0);
  SendMessage(Win,EM_SETSEL,Len,Len);
  SendMessage(Win,EM_SCROLLCARET,0,0);
#endif

#if defined(SSE_GUI_RESTOREPOS) // request
  NextLine();
  Win=CreateButton(T("Restore window positions"),IDC_RESTOREWINPOS,WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX);
  SendMessage(Win,BM_SETCHECK,CSF.GetInt("Options","RestoreWinPos",0),0);
#endif

  NextLine();
  Win=CreateButton(T("Run on startup"),IDC_STARTRUN,WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX);
  SendMessage(Win,BM_SETCHECK,CSF.GetInt("Options","RunOnStart",0),0);
  //ToolAddWindow(ToolTip,Win,T("No need to press play"));

  NextLine();
  Win=CreateButton(T("Start in fullscreen mode"),IDC_STARTFULL,/*0,y,Wid,*/WS_CHILD
                   |WS_TABSTOP|BS_AUTOCHECKBOX|(DWORD)(NoDD?WS_DISABLED:0));
  SendMessage(Win,BM_SETCHECK,GetCSFInt("Options","StartFullscreen",0,globalINIFile),0);

  NextLine();
  Win=CreateButton(T("Lock window size"),IDC_LOCKWINDOW);
  SendMessage(Win,BM_SETCHECK,OPTION_BLOCK_RESIZE,0);
  
#if defined(SSE_VID_D3D)
  NextLine();
  Win=CreateButton(T("Never use Direct3D"),IDC_GDI,WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX);
  ToolAddWindow(ToolTip,Win,T("Radical!"));
  SendMessage(Win,BM_SETCHECK,NoDD,0);
#endif

#if defined(SSE_VID_DD)
  NextLine();
  Win=CreateButton(T("Never use DirectDraw"),IDC_GDI,WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX);
  ToolAddWindow(ToolTip,Win,T("Radical!"));
  SendMessage(Win,BM_SETCHECK,NoDD,0);
#endif
  
#if !defined(SSE_SOUND_NO_NOSOUND_OPTION)
  NextLine();
  Win=CreateButton(T("Never use DirectSound"),IDC_NODSOUND,WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX);
  ToolAddWindow(ToolTip,Win,T("Radical!"));
  SendMessage(Win,BM_SETCHECK,CSF.GetInt("Options","NoDirectSound",0),0);
#endif

#if !defined(SSE_FORCE_TRACE_FILE)
  NextLine();
  Win=CreateButton(SSE_TRACE_FILE_NAME,IDC_TRACEFILE,WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX);
  SendMessage(Win,BM_SETCHECK,SSEConfig.TraceFile,0);
  ToolAddWindow(ToolTip,Win,T("Debug info"));
  Win=CreateButton(T("Show paths"),IDC_TRACESHOWPATH,WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX);
  SendMessage(Win,BM_SETCHECK,SSEConfig.TraceShowPath,0);
  ToolAddWindow(ToolTip,Win,T("File paths could contain private info"));
#endif

#if defined(SSE_AUTOUPDATE)
  NextLine();

#if defined(SSE421R2)
  DWORD mask=WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX;
  if(!SSEConfig.curl)
    mask|=WS_DISABLED;
  Win=CreateButton(T("Check for updates"),IDC_CHECKUPDATE,mask);
#else
  Win=CreateButton(T("Check for updates"),IDC_CHECKUPDATE,WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX);
#endif
#if defined(SSE421R2)
  bool bAutoUpdate=CSF.GetInt("Update","AutoUpdateEnabled",true);
  SendMessage(Win,BM_SETCHECK,CSF.GetInt("Update","AutoUpdateEnabled",bAutoUpdate),0);
#else
  SendMessage(Win,BM_SETCHECK,CSF.GetInt("Update","AutoUpdateEnabled",false),0);
#endif
#endif

  CSF.Close();
#endif

#ifdef UNIX
  auto_sts_but.create(XD,page_p,page_l,y,0,25,
          button_notify_proc,this,BT_CHECKBOX,T("Restore previous state"),100,BkCol);
  auto_sts_but.set_check(AutoLoadSnapShot);
  y+=35;
#if !defined(SSE_NO_AUTOSNAPSHOTEDIT)
  auto_sts_filename_label.create(XD,page_p,page_l,y,0,25,NULL,this,
    BT_LABEL,T("Filename"),0,BkCol);

  auto_sts_filename_edit.create(XD,page_p,page_l+5+auto_sts_filename_label.w,y,
    page_w-(5+auto_sts_filename_label.w),25,edit_notify_proc,this);
  auto_sts_filename_edit.set_text(AutoSnapShotName);
  auto_sts_filename_edit.id=100;
#endif
  y+=40;

  no_shm_but.create(XD,page_p,page_l,y,0,25,
              button_notify_proc,this,BT_CHECKBOX,
              T("Never use shared memory extension"),101,BkCol);
  no_shm_but.set_check(GetCSFInt("Options","NoSHM",0,globalINIFile));
  y+=35;

#if defined(SSE_AUTOUPDATE)
  auto_update.create(XD,page_p,page_l,y,0,25,button_notify_proc,this,BT_CHECKBOX,
      T("Check for updates"),IDC_CHECKUPDATE,BkCol);
  auto_update.set_check(GetCSFInt("Update","AutoUpdateEnabled",0,globalINIFile));
#endif

#endif//UNIX
}


void TOptionBox::CreateMacrosPage() {

  int &y=mY,&Offset=mOffset;

#ifdef WIN32
  int x;
  int ctrl_h=OPTIONS_HEIGHT/3;
  HWND Win;
  DTree.FileMasksESL.DeleteAll();
  DTree.FileMasksESL.Add("",0,RC_ICO_PCFOLDER);
  DTree.FileMasksESL.Add("stmac",0,RC_ICO_OPS_MACROS);
  UpdateDirectoryTreeIcons(&DTree);
  int h=OPTIONS_HEIGHT-ctrl_h-y-LineHeight;
  DTree.Create(Handle,page_l,y,page_w,h,(HMENU)IDC_MACROTREE,
    WS_TABSTOP,DTreeNotifyProc,this,MacroDir,T("Macros"));
  y+=h+LineStart;
  DWORD mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE;
  CreateButton(T("New Macro"),IDC_NEWMACRO,mask);
  CreateButton(T("Change Store Folder"),IDC_CHOOSEMACRODIR,mask);
  NextLine();
  x=page_l;
  CreateWindow("Steem Flat PicButton",Str(RC_ICO_RECORD),WS_CHILD|WS_TABSTOP,
    x,y,25,25,Handle,(HMENU)IDC_RECORDMACRO,hInstance,NULL);
  x+=40;
  CreateWindow("Steem Flat PicButton",Str(RC_ICO_PLAY_BIG),WS_CHILD|WS_TABSTOP,
    x,y,25,25,Handle,(HMENU)IDC_PLAYMACRO,hInstance,NULL);

  ALL_SETTINGS_BEGIN
  NextLine();
  CreateStatic(T("Mouse speed"));
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,
    Offset,y,CbUnits*6,400,Handle,(HMENU)IDC_MACROMOUSESPEED,hInstance,NULL);
  CBAddString(Win,T("Safe"),15);
  CBAddString(Win,T("Slow"),32);
  CBAddString(Win,T("Medium"),64);
  CBAddString(Win,T("Fast"),96);
  CBAddString(Win,T("V.Fast"),127);
  CBSelectItemWithData(Win,127);
  NextLine();
  CreateStatic(T("Playback event delay"));
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,
    Offset,y,CbUnits*7,400,Handle,(HMENU)IDC_MACROPLAYSPEED,hInstance,NULL);
  // Number of VBLs that input is allowed to be the same
  CBAddString(Win,T("As Recorded"),0);
  EasyStr Ms=Str(" ")+T("Milliseconds");
  for(int n=1;n<=25;n++) 
    CBAddString(Win,Str(n*20)+Ms,n);
  CBSelectItemWithData(Win,0); // 'As Recorded' works best with 'C1'

  ALL_SETTINGS_END

  DTree.SelectItemByPath(MacroSel);
#endif//WIN32

#ifdef UNIX
  dir_lv.ext_sl.DeleteAll();
  dir_lv.ext_sl.Add(3,T("Parent Directory"),1,1,0);
  dir_lv.ext_sl.Add(3,"",ICO16_FOLDER,ICO16_FOLDERLINK,0);
  dir_lv.ext_sl.Add(3,"stmac",ICO16_MACROS,ICO16_MACROLINK,0);
  dir_lv.lpig=&Ico16;
  dir_lv.base_fol=MacroDir;
  dir_lv.fol=MacroDir;
  dir_lv.allow_type_change=0;
  dir_lv.show_broken_links=0;
  dir_lv.lv.sel=-1;
  if(MacroSel.NotEmpty())
  {
    dir_lv.fol=MacroSel;
    RemoveFileNameFromPath(dir_lv.fol,REMOVE_SLASH);
  }
  dir_lv.id=2000;
  int dir_lv_h=OPTIONS_HEIGHT-10-10-30-20-30-30;
  dir_lv.create(XD,page_p,page_l,y,page_w,dir_lv_h-5,
                dir_lv_notify_proc,this);
  y+=dir_lv_h;

  hxc_button *p_but,*p_grp;

  new hxc_button(XD,page_p,page_l,y,page_w/2-5,25,button_notify_proc,this,
                      BT_TEXT,T("New Macro"),2001,BkCol);

  new hxc_button(XD,page_p,page_l+page_w/2+5,y,page_w/2-5,25,
                      button_notify_proc,this,BT_TEXT,
                      T("Change Store Folder"),2002,BkCol);
  y+=LineHeight;

  p_grp=new hxc_button(XD,page_p,page_l,y,page_w,20+30+30,NULL,this,
                      BT_GROUPBOX,T("Controls"),2009,BkCol);
  y=20;

  int x=10;
  p_but=new hxc_button(XD,p_grp->handle,x,y,25,25,
                      button_notify_proc,this,BT_ICON,"",2010,BkCol);
  p_but->set_icon(&Ico32,ICO32_RECORD);
  x+=p_but->w+5;

  p_but=new hxc_button(XD,p_grp->handle,x,y,25,25,
                      button_notify_proc,this,BT_ICON,"",2011,BkCol);
  p_but->set_icon(&Ico32,ICO32_PLAY);
  x+=p_but->w+5;

  p_but=new hxc_button(XD,p_grp->handle,x,y,0,25,NULL,this,
                      BT_LABEL,T("Mouse speed"),0,BkCol);
  x+=p_but->w+5;

  hxc_dropdown *p_dd=new hxc_dropdown(XD,p_grp->handle,x,y,
                          page_w-10-x,300,dd_notify_proc,this);
  p_dd->id=2012;
  p_dd->make_empty();
  p_dd->additem(T("Safe"),15);
  p_dd->additem(T("Slow"),32);
  p_dd->additem(T("Medium"),64);
  p_dd->additem(T("Fast"),96);
  p_dd->additem(T("V.Fast"),127);
  p_dd->select_item_by_data(127);
  y+=LineHeight;

  x=10;
  p_but=new hxc_button(XD,p_grp->handle,x,y,0,25,NULL,this,
                      BT_LABEL,T("Playback event delay"),0,BkCol);
  x+=p_but->w+5;

  p_dd=new hxc_dropdown(XD,p_grp->handle,x,y,page_w-10-x,300,
                    dd_notify_proc,this);
  p_dd->id=2013;
  p_dd->make_empty();
  p_dd->additem(T("As Recorded"),0);
  EasyStr Ms=Str(" ")+T("Milliseconds");
  for (int n=1;n<=25;n++) p_dd->additem(Str(n*20)+Ms,n);
  p_dd->select_item_by_data(1);

  if(MacroSel.NotEmpty())
    dir_lv.select_item_by_name(GetFileNameFromPath(MacroSel));
  UpdateMacroRecordAndPlay();

#endif//UNIX
}


void TOptionBox::CreateProfilesPage() {

  int &y=mY;

#ifdef WIN32
  HWND Win;
  int ctrl_h=OPTIONS_HEIGHT/2-LineHeight;
  DTree.FileMasksESL.DeleteAll();
  DTree.FileMasksESL.Add("",0,RC_ICO_PCFOLDER);
  DTree.FileMasksESL.Add("ini",0,RC_ICO_CFG);
  UpdateDirectoryTreeIcons(&DTree);
  int h=ctrl_h-y;
  DTree.Create(Handle,page_l,y,page_w,h,(HMENU)IDC_CONFIGTREE,WS_TABSTOP,DTreeNotifyProc,
  this,ProfileDir,T("Configurations"));
  y+=h+LineStart;
  DWORD mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE;
  CreateButton(T("Toggle all"),IDC_CONFIGTOGGLE,mask);
  CreateButton(T("New"),IDC_NEWCONFIG,mask);
  CreateButton(T("Change Folder"),IDC_CHOOSECONFIGDIR,mask);
  CreateButton(T("Load"),IDC_LOADCONFIG,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);
  CreateButton(T("Save"),IDC_SAVECONFIG,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);
  NextLine();
  Win=CreateWindowEx(WS_EX_CLIENTEDGE,WC_LISTVIEW,"",WS_CHILD|WS_VISIBLE|
    WS_TABSTOP|WS_DISABLED|LVS_SINGLESEL|LVS_REPORT|LVS_NOCOLUMNHEADER,page_l,y,
    page_w,OPTIONS_HEIGHT-y-LineStart,Handle,(HMENU)IDC_CONFIGLISTVIEW,hInstance,NULL);
  ListView_SetExtendedListViewStyle(Win,LVS_EX_CHECKBOXES);
  RECT rc;
  GetClientRect(Win,&rc);
  LV_COLUMN lvc;
  lvc.mask=LVCF_FMT|LVCF_SUBITEM|LVCF_TEXT|LVCF_WIDTH;
  lvc.fmt=LVCFMT_LEFT;
  lvc.cx=rc.right-GuiSM.cx_vscroll();
  lvc.pszText="";
  lvc.iSubItem=0;
  SendMessage(Win,LVM_INSERTCOLUMN,0,LPARAM(&lvc));
  LV_ITEM lvi;
  lvi.mask=LVIF_TEXT|LVIF_PARAM;
  int i=0;
  for(;;) {
    if(ProfileSection[i].Name==NULL)
      break;
    lvi.iSubItem=0;
    lvi.pszText=StaticT(ProfileSection[i].Name);
    lvi.lParam=(DWORD)ProfileSection[i].ID;
    lvi.iItem=i++;
    SendMessage(Win,LVM_INSERTITEM,0,(LPARAM)&lvi);
  }
  DTree.SelectItemByPath(ProfileSel);
#endif//WIN32

#ifdef UNIX
  dir_lv.ext_sl.DeleteAll();
  dir_lv.ext_sl.Add(3,T("Parent Directory"),1,1,0);
  dir_lv.ext_sl.Add(3,"",ICO16_FOLDER,ICO16_FOLDERLINK,0);
  dir_lv.ext_sl.Add(3,"ini",ICO16_PROFILE,ICO16_PROFILELINK,0);
  dir_lv.lpig=&Ico16;
  dir_lv.base_fol=ProfileDir;
  dir_lv.fol=ProfileDir;
  dir_lv.allow_type_change=0;
  dir_lv.show_broken_links=0;
  dir_lv.lv.sel=-1;
  if(ProfileSel.NotEmpty())
  {
    dir_lv.fol=ProfileSel;
    RemoveFileNameFromPath(dir_lv.fol,REMOVE_SLASH);
  }
  dir_lv.id=2100;
  int dir_lv_h=OPTIONS_HEIGHT-10-10-30-20-30-130;
  dir_lv.create(XD,page_p,page_l,y,page_w,dir_lv_h-5,
                dir_lv_notify_proc,this);
  y+=dir_lv_h;

  hxc_button *p_grp;

  new hxc_button(XD,page_p,page_l,y,page_w/2-5,25,button_notify_proc,this,
                      BT_TEXT,T("Save New Profile"),2101,BkCol);

  new hxc_button(XD,page_p,page_l+page_w/2+5,y,page_w/2-5,25,
                      button_notify_proc,this,BT_TEXT,
                      T("Change Store Folder"),2102,BkCol);
  y+=LineHeight;

  p_grp=new hxc_button(XD,page_p,page_l,y,page_w,20+30+130,NULL,this,
                      BT_GROUPBOX,T("Controls"),2109,BkCol);
  y=20;
  Window par=p_grp->handle;
  int par_l=10,par_w=page_w-20;

  new hxc_button(XD,par,par_l,y,par_w/2-5,25,button_notify_proc,this,
                      BT_TEXT,T("Load Profile"),2110,BkCol);

  new hxc_button(XD,par,par_l+par_w/2+5,y,par_w/2-5,25,
                      button_notify_proc,this,BT_TEXT,
                      T("Save Over Profile"),2111,BkCol);
  y+=LineHeight;

  profile_sect_lv.lpig=&Ico16;
  profile_sect_lv.display_mode=1;
  profile_sect_lv.checkbox_mode=true;
  profile_sect_lv.id=2112;
  profile_sect_lv.sl.DeleteAll();
  profile_sect_lv.sl.Sort=eslNoSort;
  for(int i=0;ProfileSection[i].Name!=NULL;i++)
    profile_sect_lv.sl.Add(T(ProfileSection[i].Name),0,ProfileSection[i].ID);
 
  profile_sect_lv.create(XD,par,par_l,y,par_w,125,listview_notify_proc,this);

  if(ProfileSel.NotEmpty())
    dir_lv.select_item_by_name(GetFileNameFromPath(ProfileSel));

  UpdateProfileDisplay();
#endif//UNIX
}



#ifdef WIN32

// helper compute width and create checkbox control
// todo convert all CreateWindow("Button"...

HWND TOptionBox::CreateButton(EasyStr caption,INT_PTR hMenu,int X,int Y,int &Wid,
                              DWORD dwStyle,int nHeight) {
  if(dwStyle&BS_PUSHLIKE)
    Wid=GetTextSize(Font,caption.Text).Width+HorizontalSeparation;
  else
    Wid=GetCheckBoxSize(Font,caption.Text).Width;
  if(nHeight==-1)
    nHeight=CharHeight;
  HWND Win=CreateWindow("Button",caption,dwStyle,X,Y,Wid,nHeight,Handle,(HMENU)hMenu,hInstance,NULL);
  mOffset+=Wid+HorizontalSeparation;
  return Win; 
}


HWND TOptionBox::CreateButton(EasyStr caption,INT_PTR hMenu,DWORD dwStyle) {
  HWND Win=CreateButton(caption,hMenu,mOffset,mY,mWid,dwStyle,CharHeight);
  return Win; 
}



HWND TOptionBox::CreateStatic(EasyStr caption,INT_PTR hMenu) {
  TWidthHeight wh=GetTextSize(Font,caption.Text);
  mWid=wh.Width;
  int h,mask,y=mY;
  int w0=mOffset-page_l+mWid;
  if(w0>page_w)
  {
    int nlines=mWid/w0+1;
    h=(wh.Height+1)*nlines;
    mY+=wh.Height*(nlines-1);
    mWid-=mOffset-page_l+mWid-page_w+HorizontalSeparation;
    mask=WS_CHILD;
  }
  else
  {
    mask=WS_CHILD|SS_CENTERIMAGE;
    h=CharHeight;
  }
  HWND Win=CreateWindow("Static",caption,mask,mOffset,y,mWid,h,Handle,(HMENU)hMenu,hInstance,NULL);
  mOffset+=mWid+HorizontalSeparation;
  return Win; 
}


HWND TOptionBox::CreateStatic(EasyStr caption) {
  HWND Win=CreateStatic(caption,IDS_STATIC);
  return Win;
}


BOOL CALLBACK TOptionBox::EnumDateFormatsProc(char *DateFormat) {
  USDateFormat=(strchr(DateFormat,'m')<strchr(DateFormat,'d'));
  return FALSE;
}


void TOptionBox::PortsMakeTypeVisible(int p) {
  int base=IDC_PORTSBASE+p*100;
  HWND CtrlParent=GetDlgItem(Handle,base);
  if(CtrlParent==NULL)
    return;

#if defined(SSE_NETWORK)
  char* sInternetStatus[3]={"Not connected","Connecting","Connected"};
  if(HWND Win=GetDlgItem(CtrlParent,base+IDS_IPSTATUS))
    SetWindowText(Win,T(sInternetStatus[STPort[p].Connected]).Text);
#endif

  bool Disabled=(p==TSTPort::PARALLEL //&& ST_MODEL==STE 
    && (Joy[N_JOY_PARALLEL_0].ToggleKey!=TJoystickConfig::ACTIVE_NEVER
      ||Joy[N_JOY_PARALLEL_1].ToggleKey!=TJoystickConfig::ACTIVE_NEVER));
  for(int n=base+10;n<base+100;n++)
    if(HWND Win=GetDlgItem(CtrlParent,n)) 
      ShowWindow(Win,SW_HIDE);
  if(!Disabled)
    for(int n=base+STPort[p].Type*10;n<base+STPort[p].Type*10+10;n++) 
    {
      if(HWND Win=GetDlgItem(CtrlParent,n)) 
        ShowWindow(Win,SW_SHOW);
    }
  if(p==TSTPort::PARALLEL) 
  {
    if(Disabled) 
    {
      ShowWindow(GetDlgItem(CtrlParent,base+1),SW_HIDE);
      ShowWindow(GetDlgItem(CtrlParent,base+2),SW_HIDE);
      ShowWindow(GetDlgItem(CtrlParent,IDS_STATIC),SW_SHOW);
    }
    else 
    {
      ShowWindow(GetDlgItem(CtrlParent,IDS_STATIC),SW_HIDE);
      ShowWindow(GetDlgItem(CtrlParent,base+1),SW_SHOW);
      ShowWindow(GetDlgItem(CtrlParent,base+2),SW_SHOW);
    }
  }
  // Redraw the groupbox
  RECT rc;
  GetWindowRect(CtrlParent,&rc);
#if defined(SSE_DONGLE_PORT)
  rc.left+=8;rc.right-=8;rc.top+=20+25-5;rc.bottom-=5;
#else
  rc.left+=8;rc.right-=8;rc.top+=20+25;rc.bottom-=5;
#endif
  POINT pt={0,0};
  ClientToScreen(Handle,&pt);
  OffsetRect(&rc,-pt.x,-pt.y);
  InvalidateRect(Handle,&rc,TRUE);
}


LRESULT CALLBACK TOptionBox::GroupBox_WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar) {
  TOptionBox *This=(TOptionBox*)GetWindowLongPtr(Win,GWLP_USERDATA);
  switch(Mess) {
  case WM_COMMAND:
  case WM_HSCROLL:
    return SendMessage(This->Handle,Mess,wPar,lPar);
  }
  return CallWindowProc(This->Old_GroupBox_WndProc,Win,Mess,wPar,lPar);
}


void TOptionBox::DrawBrightnessBitmap(HBITMAP hBmp) {
  // display colours in 16 (STE) or 8 (STF) stripes from dark to bright
  // 4 rows: grey, red, green, blue
  if(hBmp==NULL)
    return;
  BITMAP bi;
  GetObject(hBmp,sizeof(BITMAP),&bi);
  int w=bi.bmWidth,h=bi.bmHeight,bpp=bi.bmBitsPixel;
  int text_h=h/8;
  int band_w=(IS_STF)?(w/8):(w/16); // each RGB has 8 positions on the STF, 16 on the STE
  int col_h=(h-text_h)/4;
  int BytesPP=(bpp+7)/8;
  //ASSERT(BytesPP==4);
  BYTE *PicMem=new BYTE[w*h*BytesPP+16];
  ZeroMemory(PicMem,w*h*BytesPP);
  BYTE *pMem=PicMem;
  int pc_pal_start_idx=10+118+(118-65); // End of the second half of the palette
  PALETTEENTRY *pbuf=(PALETTEENTRY*)&logpal[pc_pal_start_idx];
  for(int y=0;y<h-text_h;y++)
  {
    for(int i=0;i<w;i++)
    {
      int red;
      if(IS_STF)
        red=(((i/band_w)*2)>>1)+((((i/band_w)*2)&1)<<3); // else it's too dim
      else
        red=((i/band_w)>>1)+(((i/band_w)&1)<<3);
      int green=red,blue=red;
      int pal_offset=0;
      if(y>col_h*3)
      {
        green=0,blue=0;
        pal_offset=48;
      }
      else if(y>col_h*2)
      {
        red=0,blue=0;
        pal_offset=32;
      }
      else if(y>col_h)
      {
        red=0,green=0;
        pal_offset=16;
      }
      LONG Col=palette_table[red|(green<<4)|(blue<<8)];
      switch(BytesPP) {
      case 1:
      {
        int ncol=pal_offset+(i/band_w);
        pbuf[ncol].peFlags=PC_RESERVED;
        pbuf[ncol].peRed=(BYTE)((Col&0xff0000)>>16);
        pbuf[ncol].peGreen=(BYTE)((Col&0x00ff00)>>8);
        pbuf[ncol].peBlue=(BYTE)((Col&0x0000ff));
        *pMem=BYTE(1+pc_pal_start_idx+ncol);
        break;
      }
      case 2:
        *(LPWORD)pMem=(WORD)Col;
        break;
      case 3:case 4:
        *(LPDWORD)pMem=(DWORD)Col;
        break;
      }
      pMem+=BytesPP;
    }//nxt i
  }//nxt y
  SetBitmapBits(hBmp,w*h*BytesPP,PicMem);
  delete[] PicMem;
  if(BytesPP==1)
    AnimatePalette(winpal,pc_pal_start_idx,64,pbuf);
  int gap_w=band_w/4,gap_h=text_h/8;
  HFONT f=MakeFont("Arial",-(text_h-gap_h),band_w/2-gap_w);
  HDC ScrDC=GetDC(NULL);
  HDC BmpDC=CreateCompatibleDC(ScrDC);
  ReleaseDC(NULL,ScrDC);
  SelectObject(BmpDC,hBmp);
  SelectObject(BmpDC,f);
  SetTextColor(BmpDC,RGB(224,224,224));
  SetBkMode(BmpDC,TRANSPARENT);
  for(int i=0;i<16;i++)
    TextOut(BmpDC,i*band_w+(band_w-GetTextSize(f,EasyStr(i+1)).Width)/2,
      h-text_h-1+gap_h/2,EasyStr(i+1),(int)EasyStr(i+1).Length());
  DeleteDC(BmpDC);
  DeleteObject(f);
}


LRESULT CALLBACK TOptionBox::Fullscreen_WndProc(HWND Win,UINT Mess,
                                                WPARAM wPar,LPARAM lPar) {
  if(Mess==WM_PAINT||Mess==WM_NCPAINT)
  {
    HDC WinDC=GetWindowDC(Win);
    HDC BmpDC=CreateCompatibleDC(WinDC);
    SelectObject(BmpDC,GetProp(Win,"Bitmap"));
#if defined(SSE_VID_2SCREENS)
    Disp.CheckCurrentMonitorConfig(Win);
    BitBlt(WinDC,0,0,Disp.rcMonitor.right-Disp.rcMonitor.left,
      Disp.rcMonitor.bottom-Disp.rcMonitor.top,BmpDC,0,0,SRCCOPY);
#else
    BitBlt(WinDC,0,0,GuiSM.cx_screen(),GuiSM.cy_screen(),BmpDC,0,0,SRCCOPY);
#endif
    DeleteDC(BmpDC);
    ReleaseDC(Win,WinDC);
    ValidateRect(Win,NULL);
    return 0;
  }
  return DefWindowProc(Win,Mess,wPar,lPar);
}


void TOptionBox::CreateBrightnessBitmap(int w,int h) {
  if(Handle==NULL)
    return;
  HWND Win=GetDlgItem(Handle,ID_BRIGHTNESS_MAP);
  if(Win==NULL)
    return;
  if(hBrightBmp)
    DeleteObject(hBrightBmp);
  HDC ScrDC=GetDC(NULL);
  hBrightBmp=CreateCompatibleBitmap(ScrDC,w/*136+136*/,h/*COLOUR_CONTROL_BITMAP_H*/);
  ReleaseDC(NULL,ScrDC);
  make_palette_table(col_brightness,col_contrast);
  DrawBrightnessBitmap(hBrightBmp);
  SendMessage(Win,STM_SETIMAGE,IMAGE_BITMAP,LPARAM(hBrightBmp));
}

#endif//WIN32


#ifdef UNIX

void TOptionBox::DrawBrightnessBitmap(XImage *Img)
{
  if (Img==NULL) return;

  int w=Img->width,h=Img->height;

  int band_w=w/16;
  int col_h=h/4;
  int BytesPP=(Img->bits_per_pixel+7)/8;
  ZeroMemory(Img->data,w*h*BytesPP);
  if (BytesPP>1){
    BYTE *pMem=(LPBYTE)Img->data;
    for (int y=0;y<h;y++){
      for (int i=0;i<w;i++){
        int r=((i/band_w) >> 1)+(((i/band_w) & 1) << 3),g=r,b=r;
        if (y>col_h*3){
          g=0,b=0;
        }else if (y>col_h*2){
          r=0,b=0;
        }else if (y>col_h){
          r=0,g=0;
        }
        LONG Col=palette_table[r | (g << 4) | (b << 8)];
        switch (BytesPP){
          case 1:
            *pMem=(BYTE)(Col);
            break;
          case 2:
            *LPWORD(pMem)=(WORD)(Col);
            break;
          case 3:case 4:
            *LPDWORD(pMem)=(DWORD)(Col);
            break;
        }
        pMem+=BytesPP;
      }
    }
  }
}

#endif//UNIX


void TOptionBox::UpdateParallel() {
#ifdef WIN32
  if(Handle)
    PortsMakeTypeVisible(TSTPort::PARALLEL);
#endif
}


#if !defined(SSE_NO_FREEIMAGE)

void TOptionBox::FillScreenShotFormatOptsCombo() {
  HWND Win=GetDlgItem(Handle,IDC_FI_SCREENSHOT_FORMAT);
  if(Win==NULL)
    return;
  EasyStringList sl;
  sl.Sort=eslNoSort;
  Disp.ScreenShotGetFormatOpts(&sl);
  SendMessage(Win,CB_RESETCONTENT,0,0);
  if(sl.NumStrings)
  {
    EnableWindow(Win,TRUE);
    for(int i=0;i<sl.NumStrings;i++)
      CBAddString(Win,sl[i].String,sl[i].Data[0]);
  }
  else
  {
    EnableWindow(Win,FALSE);
    CBAddString(Win,T("Normal"),0);
  }
  if(CBSelectItemWithData(Win,Disp.ScreenShotFormatOpts)<0)
    SendMessage(Win,CB_SETCURSEL,0,0);
}

#endif


#ifndef SSE_NO_OSD
void TOptionBox::CreateOSDPage() {
  
  int &y=mY,&Wid=mWid;

#ifdef WIN32
 
  HWND Win;
  BYTE *p_element[4]={&OsdControl.show_plasma,&OsdControl.show_speed,
    &OsdControl.show_icons,&OsdControl.show_cpu};
  Str osd_name[4];
  osd_name[0]=T("Logo");
  osd_name[1]=T("Speed bar");
  osd_name[2]=T("State icons");
  osd_name[3]=T("CPU speed indicator");
  for(INT_PTR i=0;i<4;i++)
  {
    CreateStatic(osd_name[i]);
    Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,page_l+Wid+5,
                     y,page_w-(Wid+5),250,Handle,(HMENU)(IDC_OSDSECONDS+i),hInstance,NULL);
    CBAddString(Win,T("Off"),0);
    CBAddString(Win,Str("1 ")+T("Second"),1); // as seem in ini?
    CBAddString(Win,Str("2 ")+T("Seconds"),2);
    CBAddString(Win,Str("3 ")+T("Seconds"),3);
    CBAddString(Win,Str("4 ")+T("Seconds"),4);
    CBAddString(Win,Str("5 ")+T("Seconds"),5);
    CBAddString(Win,Str("6 ")+T("Seconds"),6);
    CBAddString(Win,Str("8 ")+T("Seconds"),8);
    CBAddString(Win,Str("10 ")+T("Seconds"),10);
    CBAddString(Win,Str("12 ")+T("Seconds"),12);
    CBAddString(Win,Str("15 ")+T("Seconds"),15);
    CBAddString(Win,Str("20 ")+T("Seconds"),20);
    CBAddString(Win,Str("30 ")+T("Seconds"),30);
    CBAddString(Win,T("Always Shown"),OSD_SHOW_ALWAYS);
    if(CBSelectItemWithData(Win,*(p_element[i]))<0)
      SendMessage(Win,CB_SETCURSEL,0,0);
    NextLine();
  }

#if defined(SSE_OSD_SHOW_TIME)
  ALL_SETTINGS_BEGIN
  Win=CreateButton(T("Time"),IDC_SHOWTIME);
  SendMessage(Win,BM_SETCHECK,OPTION_OSD_TIME,0);
  ToolAddWindow(ToolTip,Win,T("Measure the time you waste"));
  ALL_SETTINGS_END
#endif

#if defined(SSE_OSD_DEBUGINFO)
  Win=CreateButton(T("Debug info"),IDC_OSD_DEBUGINFO);
  SendMessage(Win,BM_SETCHECK,OPTION_OSD_DEBUGINFO,0);
  ToolAddWindow(ToolTip,Win,T("See manual for the meaning of symbols"));
#endif

#if defined(SSE_OSD_FPS_INFO)
  Win=CreateButton(T("FPS"),IDC_OSD_FPSINFO);
  SendMessage(Win,BM_SETCHECK,OPTION_OSD_FPSINFO,0);
  ToolAddWindow(ToolTip,Win,T("ST Frame counter\nDoesn't work on everything!")); 
    //  in particular not with option C3, or STE programs that manipulate VCOUNT
#endif

  NextLine();
  Win=CreateButton(T("Disk access light"),IDC_DISKLIGHT);
  SendMessage(Win,BM_SETCHECK,OsdControl.show_disk_light,0);
  Win=CreateButton(T("Disk drive track info"),IDC_TRACKINFO);
  SendMessage(Win,BM_SETCHECK,OPTION_DRIVE_INFO,0);
  ToolAddWindow(ToolTip,Win,T("See what the floppy drives are doing with this option"));

  NextLine();
  Win=CreateButton(T("Scrolling messages"),IDC_OSD_SCROLLERS);
  SendMessage(Win,BM_SETCHECK,OsdControl.show_scrollers,0);
  ToolAddWindow(ToolTip,Win,T("Useful tips"));

  Win=CreateButton(T("Bad jokes"),IDC_OSD_JOKES);
  SendMessage(Win,BM_SETCHECK,OsdControl.show_jokes,0);
  ToolAddWindow(ToolTip,Win,T("Less useful"));

  NextLine();
  CreateStatic(T("Chance of scroller (%)"));
  Wid=CbUnits*2;
  HWND hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    mOffset,y,Wid,CharHeight,Handle,(HMENU)IDC_SCROLLERSFREQ0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT|UDS_ALIGNRIGHT,
    0,0,0,0,Handle,(HMENU)IDC_SCROLLERSFREQ1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(100,0));
  SendMessageW(Win,UDM_SETPOS32,0,OsdControl.ScrollerFrequency);
  mOffset+=Wid+HorizontalSeparation;
  CreateStatic(T("every (seconds)"));
  Wid=CbUnits*3;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    mOffset,y,Wid,CharHeight,Handle,(HMENU)IDC_SCROLLERSSEC0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT|UDS_ALIGNRIGHT,
    0,0,0,0,Handle,(HMENU)IDC_SCROLLERSSEC1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE32,0,60*60*24);
  SendMessageW(Win,UDM_SETPOS32,0,OsdControl.SecondsBetweenScrollers);

  NextLine();
  Win=CreateButton(T("Disable on screen display"),IDC_NOOSD);
  SendMessage(Win,BM_SETCHECK,OsdControl.disable,0);
  Win=CreateButton(T("No OSD on stop"),IDC_OSD_NONEONSTOP);
  SendMessage(Win,BM_SETCHECK,OPTION_NO_OSD_ON_STOP,0);
  //ToolAddWindow(ToolTip,Win,T("One frame delay on stop")); 

#endif//WIN32

#ifdef UNIX
  hxc_button *p_but;
  hxc_dropdown *p_dd;

  p_but=new hxc_button(XD,page_p,page_l,y,0,25,button_notify_proc,this,
                          BT_CHECKBOX,T("Disk access light"),IDC_DISKLIGHT,BkCol);
  p_but->set_check(OsdControl.show_disk_light);
  y+=35;

  p_but=new hxc_button(XD,page_p,page_l,y,0,25,button_notify_proc,this,
                          BT_CHECKBOX,T("Disk drive track info"),IDC_TRACKINFO,BkCol);
  p_but->set_check(OPTION_DRIVE_INFO);
  y+=35;

  BYTE *p_element[4]={&OsdControl.show_plasma,&OsdControl.show_speed,&OsdControl.show_icons,&OsdControl.show_cpu};
  Str osd_name[4];
  osd_name[0]=T("Logo");
  osd_name[1]=T("Speed bar");
  osd_name[2]=T("State icons");
  osd_name[3]=T("CPU speed indicator");
  for (int i=0;i<4;i++){
    p_but=new hxc_button(XD,page_p,page_l,y,0,25,NULL,NULL,BT_LABEL,osd_name[i],0,BkCol);

    p_dd=new hxc_dropdown(XD,page_p,page_l+p_but->w+5,y,page_w-(p_but->w+5),200,dd_notify_proc,this);
    p_dd->id=IDC_OSDSECONDS+i;
    p_dd->additem(T("Off"),0);
    p_dd->additem(Str("2 ")+T("Seconds"),2);
    p_dd->additem(Str("3 ")+T("Seconds"),3);
    p_dd->additem(Str("4 ")+T("Seconds"),4);
    p_dd->additem(Str("5 ")+T("Seconds"),5);
    p_dd->additem(Str("6 ")+T("Seconds"),6);
    p_dd->additem(Str("8 ")+T("Seconds"),8);
    p_dd->additem(Str("10 ")+T("Seconds"),10);
    p_dd->additem(Str("12 ")+T("Seconds"),12);
    p_dd->additem(Str("15 ")+T("Seconds"),15);
    p_dd->additem(Str("20 ")+T("Seconds"),20);
    p_dd->additem(Str("30 ")+T("Seconds"),30);
    p_dd->additem(T("Always Shown"),OSD_SHOW_ALWAYS);
    if (p_dd->select_item_by_data(*(p_element[i]),0)<0) p_dd->changesel(0);
    y+=35;
  }

  p_but=new hxc_button(XD,page_p,page_l,y,0,25,button_notify_proc,this,
             BT_CHECKBOX,T("Scrolling messages"),IDC_OSD_SCROLLERS,BkCol);
  p_but->set_check(OsdControl.show_scrollers);
  p_but=new hxc_button(XD,page_p,page_l+200,y,0,25,button_notify_proc,this,
             BT_CHECKBOX,T("Bad jokes"),IDC_OSD_JOKES,BkCol);
  p_but->set_check(OsdControl.show_jokes);
  y+=35;

#if defined(SSE_OSD_SCROLLER_DISK_IMAGE)
  p_but=new hxc_button(XD,page_p,page_l,y,0,25,button_notify_proc,this,
                          BT_CHECKBOX,T("Disk image names"),12002,BkCol);
  p_but->set_check(OSD_IMAGE_NAME);
  y+=35;
#endif

#if defined(SSE_OSD_FPS_INFO)
  p_but=new hxc_button(XD,page_p,page_l,y,0,25,button_notify_proc,this,
                          BT_CHECKBOX,T("FPS"),12003,BkCol);
  p_but->set_check(OPTION_OSD_FPSINFO);
  y+=35;
#endif

#if defined(SSE_OSD_DEBUGINFO)
  y-=35;
  p_but=new hxc_button(XD,page_p,page_l+200,y,0,25,button_notify_proc,this,
             BT_CHECKBOX,T("Debug info"),IDC_OSD_DEBUGINFO,BkCol);
  p_but->set_check(OPTION_OSD_DEBUGINFO);
  y+=35;
#endif
  osd_disable_but.create(XD,page_p,page_l,y,0,25,button_notify_proc,this,
                          BT_CHECKBOX,T("Disable on screen display"),12030,BkCol);
  osd_disable_but.set_check(OsdControl.disable);

#endif//UNIX
}
#endif//#ifndef SSE_NO_OSD


void TOptionBox::CreateFullscreenPage() {

  int &y=mY,&Offset=mOffset,&Wid=mWid;

// 2 versions, one for DD, one for D3D
#if defined(SSE_VID_DD) || defined(SSE_VID_D3D)
  HWND Win;
  DWORD mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE;
  EasyStr caption=FullScreen ? T("Go Windowed now") : T("Go Fullscreen now");
  Wid=GetTextSize(Font,caption).Width;
  Offset+=page_w/2-Wid/2;
  Win=CreateButton(caption,IDC_TOGGLE_FULLSCREEN,mask);

  NextLine();
  Win=CreateButton(T("Fullscreen on Maximize Window"),IDC_FULSCREEN_ON_MAX);
  SendMessage(Win,BM_SETCHECK,OPTION_MAX_FS,0);

  NextLine();

  mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX;
#ifdef SSE_VID_D3D
  if(!OPTION_FAKE_FULLSCREEN&&!SSEConfig.TrueFullScreenGui)
    mask|=WS_DISABLED;
#endif
  Win=CreateButton(T("Fullscreen GUI"),IDC_FULLSCREENGUI,mask);
  SendMessage(Win,BM_SETCHECK,OPTION_FULLSCREEN_GUI,0);
  ToolAddWindow(ToolTip,Win,T("Depends on your system, leaving this unchecked\
 is safer but if it works it's quite handy"));

  if(!OPTION_FULLSCREEN_GUI)
    mask|=WS_DISABLED;
  Win=CreateButton(T("Confirm Before Quit"),IDC_CONFIRM_QUIT,mask);
  SendMessage(Win,BM_SETCHECK,FSQuitAskFirst,0);

  NextLine();

#if defined(SSE_VID_D3D)
  int disable=(OPTION_FAKE_FULLSCREEN) ? WS_DISABLED : 0;
  mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX;

  Win=CreateButton(T("Stretch"),IDC_FSSTRETCH);
  SendMessage(Win,BM_SETCHECK,draw_stretch_fs,0);
  ToolAddWindow(ToolTip,Win,T("Internal rendering mode, if unchecked Steem tries\
 not to stretch"));
#endif

  Win=CreateButton(T("VSync"),IDC_FSVSYNC);
  SendMessage(Win,BM_SETCHECK,FSDoVsync,0);
  ToolAddWindow(ToolTip,Win,T("NOTE it syncs on your main display"));

#if defined(SSE_VID_D3D_VSYNC)
  Win=CreateButton("Auto VSync",IDC_AUTOVSYNC_FS);
  SendMessage(Win,BM_SETCHECK,OPTION_AUTOVSYNC_FS,0);
  ToolAddWindow(ToolTip,Win,T("VSync only if the PC frequency is appropriate\n")
    +T("NOTE it syncs on your main display"));
#endif

  Win=CreateButton(T("Triple Buffering"),IDC_TRIPLE_BUFFERING);
  SendMessage(Win,BM_SETCHECK,OPTION_3BUFFER_FS,0);
  ToolAddWindow(ToolTip,Win,T("Yes, we just add a buffer :)\nYou decide if it's\
 better or not."));

  NextLine();
  CreateStatic(T("Aspect ratio"));
  mask=WS_CHILD|BS_AUTORADIOBUTTON;
  Win=CreateButton(T("Screen"),IDC_RADIO_FS_AR,mask|WS_GROUP);
  EasyStr hint=T("Screen: resolution of your PC monitor\nCorrect: resolution of\
 the ST\nAdjusted: integral scaling");
  ToolAddWindow(ToolTip,Win,hint);
  Win=CreateButton(T("Correct"),IDC_RADIO_FS_AR+1,mask);
  ToolAddWindow(ToolTip,Win,hint);
  //Win=CreateButton(T("Crisp"),IDC_RADIO_FS_AR+2,mask);
  Win=CreateButton(T("Adjusted"),IDC_RADIO_FS_AR+2,mask);
  ToolAddWindow(ToolTip,Win,hint);
  SendMessage(GetDlgItem(Handle,IDC_RADIO_FS_AR+OPTION_FULLSCREEN_AR),BM_SETCHECK,TRUE,0);

#endif//#if defined(SSE_VID_DD) || defined(SSE_VID_D3D)

#if defined(SSE_VID_DD)

  if(Disp.DDDisplayModePossible[2])
  { // available on some systems
    NextLine();
    Win=CreateButton(T("Use 640x400 (no borders only)"),IDC_FS640X400);
    ToolAddWindow(ToolTip,Win,
      T("When this option is ticked Steem will use the 600x400 PC screen\
        resolution in fullscreen if it can"));
    SendMessage(Win,BM_SETCHECK,prefer_res_640_400,0);
  }

  NextLine();

  //ALL_SETTINGS_BEGIN

  //CreateWindow("Static",T("DirectDraw"),WS_CHILD,
  CreateStatic(T("Mode"));
  Wid=CbUnits*8;
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST,
    Offset,y,Wid,200,Handle,(HMENU)IDC_BLITMODE,hInstance,NULL);
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Screen Flip"));
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Straight Blit"));
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Stretch Blit"));
#if defined(SSE_VID_2SCREENS)
  //SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Fake fullscreen"));
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Windowed Borderless")); // same name as in D3D
#endif
  ToolAddWindow(ToolTip, Win,T("First two options draw double pixels (in low\
 res), Stretch adapts to your chosen resolution, last uses your desktop screen"));
  SendMessage(Win,CB_SETCURSEL,draw_fs_blit_mode,0);
  Offset+=Wid+HorizontalSeparation;
  CreateStatic(T("Stretch"));
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,Offset,y,
                   CbUnits*4,200,Handle,(HMENU)IDC_FSSTRETCHRES,hInstance,NULL);
  for(int i=0;i<NFSRES&&Disp.fs_res[i].x;i++)
  {
    char res_txt[40];
    sprintf((char*)res_txt,"%dx%d",Disp.fs_res[i].x,Disp.fs_res[i].y);
    SendMessage(Win,CB_ADDSTRING,i,(LPARAM)res_txt);
  }
  SendMessage(Win,CB_SETCURSEL,Disp.fs_res_choice,0);

  //ALL_SETTINGS_END

  //ALL_SETTINGS_BEGIN

  NextLine();
  CreateStatic(T("Preferred PC refresh rates:"));
  const int Wid2=CbUnits*4;
  if(Disp.DDDisplayModePossible[2])
  {
    NextLine();
    CreateStatic("640x400");
    Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL
      ,Offset,y,Wid2,200,Handle,(HMENU)IDP_FSPREFERREDHZ,hInstance,NULL);
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Default"));
    for(int n=1;n<NUM_HZ;n++) 
      SendMessage(Win,CB_ADDSTRING,0,(LPARAM)((Str(HzIdxToHz[n])+"Hz").Text));
    Offset+=Wid2+HorizontalSeparation;
    CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Path Display","",WS_CHILD|PDS_VCENTRESTATIC,
      Offset,y,Wid2,CharHeight,Handle,(HMENU)(IDP_FSPREFERREDHZ+1),hInstance,NULL);
  }
  NextLine();
  CreateStatic("640x480");
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,Offset,y,Wid2,200,
                   Handle,(HMENU)(IDP_FSPREFERREDHZ+2),hInstance,NULL);
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Default"));
  for(int n=1;n<NUM_HZ;n++)
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)((Str(HzIdxToHz[n])+"Hz").Text));
  Offset+=Wid2+HorizontalSeparation;
  CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Path Display","",WS_CHILD|PDS_VCENTRESTATIC,
    Offset,y,Wid2,CharHeight,Handle,(HMENU)(IDP_FSPREFERREDHZ+3),hInstance,NULL);
  NextLine();
  CreateStatic("800x600");
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,Offset,y,
                   Wid2,200,Handle,(HMENU)(IDP_FSPREFERREDHZ+4),hInstance,NULL);
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Default"));
  for(int n=1;n<NUM_HZ;n++)
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)((Str(HzIdxToHz[n])+"Hz").Text));
  Offset+=Wid2+HorizontalSeparation;
  CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Path Display","",WS_CHILD|PDS_VCENTRESTATIC,
    Offset,y,Wid2,CharHeight,Handle,(HMENU)(IDP_FSPREFERREDHZ+5),hInstance,NULL);
  NextLine();
  CreateStatic(T("Stretch"));
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,Offset,y,
                   Wid2,200,Handle,(HMENU)(IDP_FSPREFERREDHZ+6),hInstance,NULL);
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Default"));
  for(int n=1;n<NUM_HZ;n++)
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)((Str(HzIdxToHz[n])+"Hz").Text));
  Offset+=Wid2+HorizontalSeparation;
  CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Path Display","",WS_CHILD|PDS_VCENTRESTATIC,
    Offset,y,Wid2,CharHeight,Handle,(HMENU)(IDP_FSPREFERREDHZ+7),hInstance,NULL);
  //ALL_SETTINGS_END
  UpdateFullscreen();
#endif//DD


#if defined(SSE_VID_D3D)
  
  mask=WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX;

  NextLine();

  D3DFORMAT DisplayFormat=D3DFMT_X8R8G8B8;
/*  We do some D3D here, listing all modes only when necessary to save memory.
  Or we could have another function in display, but then code bloat...
*/
  UINT nD3Dmodes=(Disp.pD3D)
    ? Disp.pD3D->GetAdapterModeCount(Disp.m_Adapter,DisplayFormat) : 0;
  CreateStatic(T("Mode"));
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL|disable,
    Offset,y,CbUnits*7,200,Handle,(HMENU)IDC_D3DMODE,hInstance,NULL);
  ToolAddWindow(ToolTip,Win,
    T("You have the choice between all the 32bit modes your video card can\
handle. Try to be realistic."));
  D3DDISPLAYMODE Mode;
  for(UINT i=0;i<nD3Dmodes;i++)
  {
    Disp.pD3D->EnumAdapterModes(Disp.m_Adapter,DisplayFormat,i,&Mode);
    char tmp[20];
    sprintf(tmp,"%ux%u %uHz",Mode.Width,Mode.Height,Mode.RefreshRate);
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)tmp);
  }
  SendMessage(Win,CB_SETCURSEL,Disp.D3DMode,0);

  NextLine();
  mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX;

  Win=CreateButton(T("Use Desktop Refresh Rate"),IDC_DESKTOPHZ,WS_CHILD|WS_TABSTOP
    |BS_CHECKBOX|disable);
  ToolAddWindow(ToolTip,Win,T("This will bypass the Hz setting in Mode, useful\
 for some video cards"));
  SendMessage(Win,BM_SETCHECK,OPTION_FULLSCREEN_DEFAULT_HZ,0);

  NextLine();
  Win=CreateButton(T("Windowed Borderless Mode"),IDC_FAKEFULL,WS_CHILD|WS_TABSTOP
    |BS_CHECKBOX|(FullScreen?WS_DISABLED:0));
  ToolAddWindow(ToolTip,Win,T("Safer fullscreen mode, like a big window without frame"));
  SendMessage(Win,BM_SETCHECK,OPTION_FAKE_FULLSCREEN,0);

#endif//D3D
}


#if defined(SSE_VID_DD)

void TOptionBox::UpdateFullscreen() {
  if(Handle==NULL) 
    return;
  //EnableWindow(GetDlgItem(Handle,280),(draw_fs_blit_mode<DFSM_STRETCHBLIT)); //was scanlines
  //EnableWindow(GetDlgItem(Handle,281),(draw_fs_blit_mode<DFSM_STRETCHBLIT)); //? probably ST A.R
  EnableWindow(GetDlgItem(Handle,IDC_FS640X400),(border==0 
    && draw_fs_blit_mode!=DFSM_FAKEFULLSCREEN));
  EnableWindow(GetDlgItem(Handle,IDC_BLITMODE),(!FullScreen));
  EnableWindow(GetDlgItem(Handle,IDC_FSSTRETCHRES),
    (draw_fs_blit_mode==DFSM_STRETCHBLIT&&!FullScreen));
  BOOL can_change_ar=(draw_fs_blit_mode>=DFSM_STRETCHBLIT);
  EnableWindow(GetDlgItem(Handle,IDC_RADIO_FS_AR),can_change_ar);
  EnableWindow(GetDlgItem(Handle,IDC_RADIO_FS_AR+1),can_change_ar);
  EnableWindow(GetDlgItem(Handle,IDC_RADIO_FS_AR+2),can_change_ar);
  EnableWindow(GetDlgItem(Handle,IDC_TRIPLE_BUFFERING),!can_change_ar);
  for(int i=0;i<NPC_HZ_CHOICES;i++) 
  {
    for(int n=0;n<NUM_HZ;n++) 
    {
      if(HzIdxToHz[n]==prefer_pc_hz[i]) 
      {
        SendDlgItemMessage(Handle,IDP_FSPREFERREDHZ+i*2,CB_SETCURSEL,n,0);
        break;
      }
    }
    EasyStr Text=T("UNTESTED");
    if(prefer_pc_hz[i]) 
    {
      if(LOBYTE(tested_pc_hz[i])==prefer_pc_hz[i]) 
      {
        if(HIBYTE(tested_pc_hz[i])) 
        {
          if(real_pc_hz[i]>0)
            Text=EasyStr(T("OK"))+" ("+real_pc_hz[i]+"Hz)";
          else
            Text=T("OK");
        }
        else
          Text=T("FAILED");
      }
    }
    else
      Text=T("OK");
    SendDlgItemMessage(Handle,IDP_FSPREFERREDHZ+1+i*2,WM_SETTEXT,0,LPARAM(Text.Text));
  }
}

#endif


void TOptionBox::CreateSoundPage() {
  
  int &y=mY,&Offset=mOffset,&Wid=mWid;

#ifdef WIN32

  HWND Win;
  int mask;
  DWORD DisableIfMute=(DWORD)((OPTION_SOUNDMUTE||!UseSound)?WS_DISABLED:0);

  TConfigStoreFile CSF(globalINIFile);

#if defined(SSE_GUI_421_REORDER)
  DWORD DisableIfNosound=(DWORD)((!UseSound)?WS_DISABLED:0);
  CreateStatic(T("Device"));
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,Offset,y,
                   page_w-Offset+page_l-LineStart,200,Handle,(HMENU)IDC_SOUNDDEVICE,hInstance,NULL);
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("None")); // can eject soundcard...
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Default"));
  for(int i=0;i<DSDriverModuleList.NumStrings;i++)
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)DSDriverModuleList[i].String);
  SendMessage(Win,CB_SETCURSEL,SSEOptions.AudioInterface,0);
  EasyStr DSDriverModName=CSF.GetStr("Options","DSDriverName","");
  if(DSDriverModName.NotEmpty()) 
  {
    for(int i=0;i<DSDriverModuleList.NumStrings;i++)
      if(IsSameStr_I(DSDriverModuleList[i].String,DSDriverModName)) 
      {
        SendMessage(Win,CB_SETCURSEL,2+i,0); // after None and Default
        break;
      }
  }
  CSF.Close();

  NextLine();
  
  //CreateStatic(T("Sample Rate"));
  CreateStatic(T("Format"));
  Wid=3*CbUnits;
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|WS_VSCROLL|DisableIfMute|CBS_DROPDOWNLIST,
                   Offset,y,Wid,200,Handle,(HMENU)IDC_SAMPLERATE,hInstance,NULL);
  if(sound_comline_freq)
    CBAddString(Win,Str(sound_comline_freq)+"Hz",sound_comline_freq);
  CBAddString(Win,"384KHz",384000);
  CBAddString(Win,"250KHz",250000);
  CBAddString(Win,"192KHz",192000);
  CBAddString(Win,"96KHz",96000);
  CBAddString(Win,"50KHz",50066);
  CBAddString(Win,"48KHz",48000);
  CBAddString(Win,"44.1 KHz",44100);
  CBAddString(Win,"25KHz",25033);
  CBAddString(Win,"22KHz",22050);
  if(CBSelectItemWithData(Win,sound_chosen_freq)==-1)
    SendMessage(Win,CB_SETCURSEL,CBAddString(Win,Str(sound_chosen_freq)+"Hz",sound_chosen_freq),0);
  Offset+=Wid+HorizontalSeparation;
  //CreateStatic(T("Format"));
  Wid=9*CbUnits/2;
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|DisableIfMute|WS_VSCROLL|CBS_DROPDOWNLIST,
                   Offset,y,Wid,200,Handle,(HMENU)IDC_SAMPLEFORMAT,hInstance,NULL);
  CBAddString(Win,T("8-Bit Mono"),MAKEWORD(8,1));
  CBAddString(Win,T("8-Bit Stereo"),MAKEWORD(8,2));
  CBAddString(Win,T("16-Bit Mono"),MAKEWORD(16,1));
  CBAddString(Win,T("16-Bit Stereo"),MAKEWORD(16,2));
  SendMessage(Win,CB_SETCURSEL,(sound_num_bits-8)/4+(sound_num_channels-1),0);

  Offset+=Wid+HorizontalSeparation;
  CreateStatic(T("Delay"));
  Wid=CbUnits*2;
  HWND hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,Wid,CharHeight,Handle,(HMENU)IDC_SOUNDBUFFER0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_SOUNDBUFFER,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(15,1));
  SendMessageW(Win,UDM_SETPOS32,0,psg_write_n_screens_ahead);
  ToolAddWindow(ToolTip,Win,
    T("Unfortunately, sound emulation cannot be instant, there is always a\
 delay, expressed in video frames"));

  NextLine();
    
  mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX|DisableIfNosound;
  Win=CreateButton(T("Mute"),IDC_SOUNDMUTE,mask);
  SendMessage(Win,BM_SETCHECK,OPTION_SOUNDMUTE,0);
  Win=CreateButton(T("when inactive"),IDC_AUTOMUTE,mask);
  SendMessage(Win,BM_SETCHECK,MuteWhenInactive,0);

  CreateStatic(T("Vol"));
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_TABSTOP|DisableIfMute|TBS_HORZ|TBS_TOOLTIPS,
    Offset,y,page_w-Offset+page_l-LineStart,mSliderHeight,Handle,(HMENU)IDC_SOUNDVOL,hInstance,NULL);
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,100));
  int db=SoundVolume;
  db+=10000;
  int position=(int)pow(10.0,log10(101.0)*db/10000)-1;
  SendMessage(Win,TBM_SETPOS,1,position);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETPAGESIZE,0,10);


  NextLine();
  mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX|DisableIfMute;
  CreateStatic(T("YM-2149"));

  Win=CreateButton(T("Low-level"),IDC_YMLL,mask);
  SendMessage(Win,BM_SETCHECK,OPTION_MAME_YM,0);
  ToolAddWindow(ToolTip,Win,
    T("Using MAME's AY8910 emu thx Couriersud, YM samples thx LJBK\
 and anti-aliasing filter thx Mike Perkins"));

  CreateStatic("Vol");
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,
                   Offset,y,Wid=GUIMUL(40),mSliderHeight,Handle,(HMENU)IDC_PSGREDUCE,hInstance,NULL);
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,16));
  SendMessage(Win,TBM_SETPOS,1,16-Microwire.PsgReduce);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETPAGESIZE,0,1);
  SendMessage(Win,TBM_SETTIC,0,0);
  ToolAddWindow(ToolTip,Win,T("That control wasn't on the ST"));

  Offset+=Wid;
  CreateStatic(T("Filter"));
  if(!OPTION_MAME_YM)
  {
    Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|DisableIfMute,
      Offset,y,4*CbUnits,200,Handle,(HMENU)IDC_OLDFILTER,hInstance,NULL),
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("None"));
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Coaxial"));
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("SCART"));
    SendMessage(Win,CB_SETCURSEL,psg_hl_filter-1,0);
  }
  else
  {
    Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ
      |TBS_TOOLTIPS|DisableIfMute,Offset,y,page_w-Offset+page_l-LineStart,
      mSliderHeight,Handle,(HMENU)IDC_NEWFILTER,hInstance,NULL);
    SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,YM_LOW_PASS_MAX));
    SendMessage(Win,TBM_SETPOS,1,OPTION_LOWPASS);
    SendMessage(Win,TBM_SETLINESIZE,0,1);
    SendMessage(Win,TBM_SETPAGESIZE,0,1);
    SendMessage(Win,TBM_SETTIC,0,YM_LOW_PASS_FREQ);
    ToolAddWindow(ToolTip,Win,T("That control wasn't on the ST"));
  }

  // Generally called DMA sound but we don't because DMA on the ST
  // implies bus arbitration, digital sound is handled like video memory
  NextLine();
  CreateStatic(T("Digital sound"));

  CreateStatic("Vol");
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,
                   Offset,y,Wid=GUIMUL(40),mSliderHeight,Handle,(HMENU)IDC_DMAMULTI,hInstance,NULL);
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,255));
  SendMessage(Win,TBM_SETPOS,1,Microwire.DmaMulti);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETPAGESIZE,0,1);
  SendMessage(Win,TBM_SETTIC,0,0);
  ToolAddWindow(ToolTip,Win,T("That control wasn't on the ST"));
  Offset+=Wid+HorizontalSeparation;

#if defined(SSE_SOUND_MICROWIRE_OPTION)
  Win=CreateButton("MicroWire",IDC_MICROWIRE,WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX|DisableIfMute);
  SendMessage(Win,BM_SETCHECK,SSEOptions.Microwire,0);
  ToolAddWindow(ToolTip,Win,T("Incomplete emulation"));
  
  Win=CreateButton(T("Slow fade"),IDC_LMCSLOWFADE,mask);
  SendMessage(Win,BM_SETCHECK,SSEOptions.LmcSlowFade,0);
  ToolAddWindow(ToolTip,Win,T("If you want to avoid pops when the program changes the volume too fast"));
  NextLine();

  CreateStatic(T("Bass shelf"));
  Offset+=HorizontalSeparation;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,Offset,
    y,Wid=GUIMUL(100),mSliderHeight,Handle,(HMENU)IDC_LOWSHELF,hInstance,NULL);
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(20,1000));
  SendMessage(Win,TBM_SETPOS,1,Microwire.LowShelf);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETPAGESIZE,0,1);
  ToolAddWindow(ToolTip,Win,T("That control wasn't on the ST"));
  Offset+=Wid+HorizontalSeparation;
  CreateStatic(T("Treble shelf"));
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,Offset,
    y,GUIMUL(100),mSliderHeight,Handle,(HMENU)IDC_HIGHSHELF,hInstance,NULL);
  //SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(20,20000));
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(2000,20000));
  SendMessage(Win,TBM_SETPOS,1,Microwire.HighShelf);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETPAGESIZE,0,1);
  ToolAddWindow(ToolTip,Win,T("That control wasn't on the ST"));

#endif

#if defined(SSE_DRIVE_SOUND)
  NextLine();
  mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX|DisableIfMute;
  Win=CreateButton(T("Drive sound"),IDC_DRIVE_SOUND,mask);
  SendMessage(Win,BM_SETCHECK,OPTION_DRIVE_SOUND,0);
  ToolAddWindow(ToolTip,Win,T("Bad imitation of a SainT feature. You can choose\
 the sounds directory in the disk manager"));  
  mask&=~BS_CHECKBOX;
  CreateStatic(T("Vol"));
  Wid=GUIMUL(100);
  Win=CreateWindow(TRACKBAR_CLASS,"",mask|TBS_HORZ|TBS_TOOLTIPS,Offset,y,
    Wid,mSliderHeight,Handle,(HMENU)IDC_DRIVESOUNDVOL,hInstance,NULL);
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,100));
  db=FloppyDrive[DRIVE_A].SoundVolume;
  db+=10000;
  position=(int)pow(10.0,log10(101.0)*db/10000)-1;
  SendMessage(Win,TBM_SETPOS,1,position);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETPAGESIZE,0,10);
#endif

  NextLine();
  Offset+=LineStart;
  mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX|DisableIfMute;

  CreateWindow("Button",T("Record"),WS_CHILD|BS_GROUPBOX,
    page_l,y,page_w,GROUP_HEIGHT(2),Handle,(HMENU)IDC_GROUPSOUNDRECORD,hInstance,NULL);
  y+=mGroupTitleHeight;
  Offset+=LineStart;
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_RECORD),WS_CHILD|DisableIfMute,
    Offset,y,32,32,Handle,(HMENU)IDC_SOUNDRECORD,hInstance,NULL);
  SendMessage(Win,BM_SETCHECK,bSoundRecord,0);
  if(WAVOutputFile.Empty()) 
    WAVOutputFile=UsersPath+((OPTION_SOUND_RECORD_FORMAT)?"\\ST.wav":"\\ST.ym");
  Offset+=40; // icon size (TODO?)
  Wid=GetTextSize(Font,T("Choose")).Width+HorizontalSeparation*2;
  int Wid2=page_w-Offset+page_l-Wid-HorizontalSeparation*2;
  CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Path Display",WAVOutputFile,WS_CHILD|DisableIfMute,
    Offset,y,Wid2,CharHeight,Handle,(HMENU)IDP_SOUNDRECORD,hInstance,NULL);
  Offset+=Wid2+HorizontalSeparation;
  CreateButton(T("Choose"),IDC_SOUNDDIRCHOOSE,Offset,y,Wid,WS_CHILD|WS_TABSTOP|BS_CHECKBOX
    |BS_PUSHLIKE|DisableIfMute);
/*  Add record to YM functionality using the same GUI elements as for WAV.
    We add a combobox to select format rather than radio buttons, this way
    we can add more formats. TODO either way
*/
  NextLine();
  Offset+=LineStart;
  CreateStatic(T("Format"));
  mask=WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|DisableIfMute;
  Wid=3*CbUnits;
  Win=CreateWindow("Combobox","",mask,Offset,y,Wid,200,Handle,
    (HMENU)IDC_WAVORYM,hInstance,NULL);
  Offset+=Wid+HorizontalSeparation;
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Wav"));
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("YM"));
  SendMessage(Win,CB_SETCURSEL,OPTION_SOUND_RECORD_FORMAT,0);
  Win=CreateButton(T("Warn before overwrite"),IDC_SOUNDRECORDWARN,Offset,y,Wid,WS_CHILD
    |WS_TABSTOP|BS_CHECKBOX|DisableIfMute);
  SendMessage(Win,BM_SETCHECK,RecordWarnOverwrite,0);

#else//#if defined(SSE_GUI_421_REORDER)

  CreateWindow("Button",T("Device"),WS_CHILD|BS_GROUPBOX,
    page_l,y,page_w,GROUP_HEIGHT(3),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
  y+=mGroupTitleHeight;
  Offset+=LineStart;
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,Offset,y,
                   page_w-Offset+page_l-LineStart,200,Handle,(HMENU)IDC_SOUNDDEVICE,hInstance,NULL);
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("None")); // can eject soundcard...
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Default"));
  for(int i=0;i<DSDriverModuleList.NumStrings;i++)
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)DSDriverModuleList[i].String);
  SendMessage(Win,CB_SETCURSEL,SSEOptions.AudioInterface,0);
  EasyStr DSDriverModName=CSF.GetStr("Options","DSDriverName","");
  if(DSDriverModName.NotEmpty()) 
  {
    for(int i=0;i<DSDriverModuleList.NumStrings;i++)
      if(IsSameStr_I(DSDriverModuleList[i].String,DSDriverModName)) 
      {
        SendMessage(Win,CB_SETCURSEL,2+i,0); // after None and Default
        break;
      }
  }
  CSF.Close();

  NextLine();
  Offset+=LineStart;
  
  CreateStatic(T("Sample Rate"));
  Wid=9*CbUnits/2;
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|WS_VSCROLL|DisableIfMute
    |CBS_DROPDOWNLIST,Offset,y,Wid,200,Handle,(HMENU)IDC_SAMPLERATE,hInstance,NULL);
  if(sound_comline_freq)
    CBAddString(Win,Str(sound_comline_freq)+"Hz",sound_comline_freq);
  CBAddString(Win,"384KHz",384000);
  CBAddString(Win,"250KHz",250000);
  CBAddString(Win,"192KHz",192000);
  CBAddString(Win,"96KHz",96000);
  CBAddString(Win,"50KHz",50066);
  CBAddString(Win,"48KHz",48000);
  CBAddString(Win,"44.1 KHz",44100);
  CBAddString(Win,"25KHz",25033);
  CBAddString(Win,"22KHz",22050);
  if(CBSelectItemWithData(Win,sound_chosen_freq)==-1)
    SendMessage(Win,CB_SETCURSEL,CBAddString(Win,Str(sound_chosen_freq)+"Hz",
      sound_chosen_freq),0);
  Offset+=Wid+HorizontalSeparation;
  CreateStatic(T("Format"));
  Wid=9*CbUnits/2;
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|DisableIfMute|WS_VSCROLL
    |CBS_DROPDOWNLIST,Offset,y,Wid,200,Handle,(HMENU)IDC_SAMPLEFORMAT,
    hInstance,NULL);
  CBAddString(Win,T("8-Bit Mono"),MAKEWORD(8,1));
  CBAddString(Win,T("8-Bit Stereo"),MAKEWORD(8,2));
  CBAddString(Win,T("16-Bit Mono"),MAKEWORD(16,1));
  CBAddString(Win,T("16-Bit Stereo"),MAKEWORD(16,2));
  SendMessage(Win,CB_SETCURSEL,(sound_num_bits-8)/4+(sound_num_channels-1),0);

  NextLine();
  Offset+=LineStart;
  CreateStatic(T("Delay"));
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|DisableIfMute|WS_VSCROLL
    |CBS_DROPDOWNLIST,Offset,y,5*CbUnits,300,Handle,(HMENU)IDC_SOUNDBUFFER,hInstance,NULL);
  EasyStr Fr=T("Frame(s)");
  for(int i=0;i<=15;i++)
    CBAddString(Win,Str(i)+" "+Fr);
  SendMessage(Win,CB_SETCURSEL,psg_write_n_screens_ahead-1,0);
  ToolAddWindow(ToolTip,Win,
    T("Unfortunately, sound emulation cannot be instant, there is always a\
 delay, expressed in video frames"));

  NextLine();

  CreateWindow("Button",T("Volume"),WS_CHILD|BS_GROUPBOX,
    page_l,y,page_w,GROUP_HEIGHT(2),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
  y+=mGroupTitleHeight;
  Offset+=LineStart;

  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_TABSTOP|DisableIfMute|TBS_HORZ|TBS_TOOLTIPS,
    Offset,y,page_w-Offset+page_l-LineStart,mSliderHeight,Handle,(HMENU)IDC_SOUNDVOL,hInstance,NULL);
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,100));
  int db=SoundVolume;
  db+=10000;
  int position=(int)pow(10.0,log10(101.0)*db/10000)-1;
  SendMessage(Win,TBM_SETPOS,1,position);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETPAGESIZE,0,10);

  NextLine();
  Offset+=LineStart;
  DWORD DisableIfNosound=(DWORD)((!UseSound)?WS_DISABLED:0);
  mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX|DisableIfNosound;
  Win=CreateButton(T("Mute sound"),IDC_SOUNDMUTE,mask);
  SendMessage(Win,BM_SETCHECK,OPTION_SOUNDMUTE,0);
  Win=CreateButton(T("Mute sound when inactive"),IDC_AUTOMUTE,mask);
  SendMessage(Win,BM_SETCHECK,MuteWhenInactive,0);

  NextLine();

#if defined(SSE_YM2149_LL)
  CreateWindow("Button","Sources",WS_CHILD|BS_GROUPBOX,
    page_l,y,page_w,GROUP_HEIGHT(3),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
  y+=mGroupTitleHeight;
  Offset+=LineStart;
  mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX|DisableIfMute;

  HACK_BEGIN
  Win=CreateButton(T("YM-2149"),IDC_YM2149_ON,mask);
  SendMessage(Win,BM_SETCHECK,SSEConfig.YmSoundOn,0);
  ToolAddWindow(ToolTip,Win,T("That control wasn't on the ST"));
  HACK_ELSE
    CreateStatic(T("YM-2149"));
  HACK_END

  Win=CreateButton(T("Low-level"),IDC_YMLL,mask);
  SendMessage(Win,BM_SETCHECK,OPTION_MAME_YM,0);
  ToolAddWindow(ToolTip,Win,
    T("Using MAME's AY8910 emu thx Couriersud, YM samples thx LJBK\
 and anti-aliasing filter thx Mike Perkins"));
  CreateStatic(T("Filter"));
  if(!OPTION_MAME_YM)
  {
    Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|DisableIfMute,
      Offset,y,5*CbUnits,200,Handle,(HMENU)IDC_OLDFILTER,hInstance,NULL),
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Line (direct)"));
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("TV (coaxial)"));
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Monitor (SCART)"));
    SendMessage(Win,CB_SETCURSEL,psg_hl_filter+1,0);
  }
  else
  {
    Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ
      |TBS_TOOLTIPS|DisableIfMute,Offset,y,page_w-Offset+page_l-LineStart,
      mSliderHeight,Handle,(HMENU)IDC_NEWFILTER,hInstance,NULL);
    SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,YM_LOW_PASS_MAX));
    SendMessage(Win,TBM_SETPOS,1,OPTION_LOWPASS);
    SendMessage(Win,TBM_SETLINESIZE,0,1);
    SendMessage(Win,TBM_SETPAGESIZE,0,1);
    SendMessage(Win,TBM_SETTIC,0,YM_LOW_PASS_FREQ);
  }
#endif//#if defined(SSE_YM2149_LL)
  NextLine();
  Offset+=LineStart;
  mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX|DisableIfMute;

  // Generally called DMA sound but we don't because DMA on the ST
  // implies bus arbitration, digital sound is handled like video memory

  HACK_BEGIN
  Win=CreateButton(T("Digital sound"),IDC_STESOUND_ON,mask);
  SendMessage(Win,BM_SETCHECK,SSEConfig.SteSoundOn,0);
  ToolAddWindow(ToolTip,Win,T("That control wasn't on the ST"));
  HACK_ELSE
  CreateStatic(T("Digital sound"));
  HACK_END

#if defined(SSE_SOUND_MICROWIRE_OPTION)
  Win=CreateButton("MicroWire",IDC_MICROWIRE,mask);
  SendMessage(Win,BM_SETCHECK,SSEOptions.Microwire,0);
  ToolAddWindow(ToolTip,Win,T("Incomplete emulation"));
#endif

#if defined(SSE_SOUND_MICROWIRE_HACKS)
#if 0//!defined(SSE_GEM_CONTROL_PANEL)
  if(OPTION_HACKS)
  {
    Win=CreateButton(T("YM-12db"),IDC_YM_12DB,mask);
    SendMessage(Win,BM_SETCHECK,OPTION_YM_12DB,0);
    ToolAddWindow(ToolTip,Win,T("That control wasn't on the ST"));
  }
#endif
    CreateStatic("PSG reduce");
    Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,
      Offset,y,GUIMUL(50),mSliderHeight,Handle,(HMENU)IDC_PSGREDUCE,hInstance,NULL);
    SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,12));
    SendMessage(Win,TBM_SETPOS,1,Microwire.PsgReduce);
    SendMessage(Win,TBM_SETLINESIZE,0,1);
    SendMessage(Win,TBM_SETPAGESIZE,0,1);
    SendMessage(Win,TBM_SETTIC,0,0);
    ToolAddWindow(ToolTip,Win,T("That control wasn't on the ST"));
    //Offset+=GUIMUL(50)+HorizontalSeparation;
#if !defined(SSE_GUI_EMUCONTROL)
  Win=CreateButton(T("Slow fade"),IDC_LMCSLOWFADE,mask);
  SendMessage(Win,BM_SETCHECK,SSEOptions.LmcSlowFade,0);
  ToolAddWindow(ToolTip,Win,T("If you want to avoid pops when the program changes the volume too fast"));
#endif
#endif

#if defined(SSE_DRIVE_SOUND)
  NextLine();
  Offset+=LineStart;
  mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX|DisableIfMute;
  Win=CreateButton(T("Drive sound"),IDC_DRIVE_SOUND,mask);
  SendMessage(Win,BM_SETCHECK,OPTION_DRIVE_SOUND,0);
  ToolAddWindow(ToolTip,Win,T("Bad imitation of a SainT feature. You can choose\
 the sounds directory in the disk manager"));  
  mask&=~BS_CHECKBOX;
  CreateStatic(T("Volume"));
  Win=CreateWindow(TRACKBAR_CLASS,"",mask|TBS_HORZ|TBS_TOOLTIPS,Offset,y,
    page_w-Offset+page_l-LineStart,mSliderHeight,Handle,(HMENU)IDC_DRIVESOUNDVOL,hInstance,NULL);
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,100));
  db=FloppyDrive[DRIVE_A].SoundVolume;
  db+=10000;
  position=(int)pow(10.0,log10(101.0)*db/10000)-1;
  SendMessage(Win,TBM_SETPOS,1,position);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETPAGESIZE,0,10);
#endif

  NextLine();
  CreateWindow("Button",T("Record"),WS_CHILD|BS_GROUPBOX,
    page_l,y,page_w,GROUP_HEIGHT(2),Handle,(HMENU)IDC_GROUPSOUNDRECORD,hInstance,NULL);
  y+=mGroupTitleHeight;
  Offset+=LineStart;
  Win=CreateWindow("Steem Flat PicButton",Str(RC_ICO_RECORD),WS_CHILD|DisableIfMute,
    Offset,y,32,32,Handle,(HMENU)IDC_SOUNDRECORD,hInstance,NULL);
  SendMessage(Win,BM_SETCHECK,bSoundRecord,0);
  if(WAVOutputFile.Empty()) 
    WAVOutputFile=UsersPath+((OPTION_SOUND_RECORD_FORMAT)?"\\ST.wav":"\\ST.ym");
  Offset+=40; // icon size (TODO?)
  Wid=GetTextSize(Font,T("Choose")).Width+HorizontalSeparation*2;
  int Wid2=page_w-Offset+page_l-Wid-HorizontalSeparation*2;
  CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Path Display",WAVOutputFile,WS_CHILD|DisableIfMute,
    Offset,y,Wid2,CharHeight,Handle,(HMENU)IDP_SOUNDRECORD,hInstance,NULL);
  Offset+=Wid2+HorizontalSeparation;
  CreateButton(T("Choose"),IDC_SOUNDDIRCHOOSE,Offset,y,Wid,WS_CHILD|WS_TABSTOP|BS_CHECKBOX
    |BS_PUSHLIKE|DisableIfMute);
/*  Add record to YM functionality using the same GUI elements as for WAV.
    We add a combobox to select format rather than radio buttons, this way
    we can add more formats. TODO either way
*/
  NextLine();
  Offset+=LineStart;
  CreateStatic(T("Format"));
  mask=WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|DisableIfMute;
  Wid=3*CbUnits;
  Win=CreateWindow("Combobox","",mask,Offset,y,Wid,200,Handle,
    (HMENU)IDC_WAVORYM,hInstance,NULL);
  Offset+=Wid+HorizontalSeparation;
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Wav"));
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("YM"));
  SendMessage(Win,CB_SETCURSEL,OPTION_SOUND_RECORD_FORMAT,0);
  Win=CreateButton(T("Warn before overwrite"),IDC_SOUNDRECORDWARN,Offset,y,Wid,WS_CHILD
    |WS_TABSTOP|BS_CHECKBOX|DisableIfMute);
  SendMessage(Win,BM_SETCHECK,RecordWarnOverwrite,0);

#endif//#if defined(SSE_GUI_421_REORDER)

#if !defined(SSE_NO_INTERNAL_SPEAKER)
  y+=LineHeight;
  Wid=GetCheckBoxSize(Font,T("Internal speaker sound")).Width;
  Win=CreateWindow("Button",T("Internal speaker sound"),WS_CHILD|WS_TABSTOP|
    BS_CHECKBOX|DisableIfNT,
    page_l,y,Wid,25,Handle,(HMENU)7300,hInstance,NULL);
  SendMessage(Win,BM_SETCHECK,sound_internal_speaker,0);
#endif

#endif//WIN32

#ifdef UNIX
  sound_group.create(XD,page_p,page_l,y,page_w,210-35,NULL,this,BT_GROUPBOX,
    T("Configuration"),0,BkCol);

  int sgy=25;

  hxc_button *but=new hxc_button(XD,sound_group.handle,10,sgy,0,25,NULL,this,BT_LABEL,T("Library"),0,BkCol);

  hxc_dropdown *dd=new hxc_dropdown(XD,sound_group.handle,15+but->w,sgy,(sound_group.w-10-(15+but->w)),300,dd_notify_proc,this);
  dd->id=5067;
  dd->additem("None",0);
#ifndef NO_RTAUDIO
  dd->additem("RtAudio",XS_RT);
#endif
#ifndef NO_PORTAUDIO
  dd->additem("PortAudio",XS_PA);
#endif
#if defined(SSE_UNIX_PULSEAUDIO)
  dd->additem("PulseAudio",XS_PULSE);
#endif
  dd->select_item_by_data(x_sound_lib);
  sgy+=LineHeight;


  device_label.create(XD,sound_group.handle,10,sgy,0,25,NULL,this,
                      BT_LABEL,T("Device"),0,BkCol);

  dd=new hxc_dropdown(XD,sound_group.handle,15+device_label.w,sgy,
                        (sound_group.w-10-(15+device_label.w)),300,dd_notify_proc,this);
  dd->id=5000;
  sgy+=LineHeight;

  sound_freq_label.create(XD,sound_group.handle,10,sgy,0,25,NULL,this,
    BT_LABEL,T("Frequency"),0,BkCol);

  sound_freq_dd.create(XD,sound_group.handle,15+sound_freq_label.w,
    sgy,370-(15+sound_freq_label.w+200),200,dd_notify_proc,this);
  sound_freq_dd.id=5002;
  sound_freq_dd.make_empty();
  if (sound_comline_freq){
    sound_freq_dd.additem(Str(sound_comline_freq)+"Hz",sound_comline_freq);
  }

  sound_freq_dd.additem("192KHz",192000);
  sound_freq_dd.additem("96KHz",96000);
  sound_freq_dd.additem("50KHz",50066);
  sound_freq_dd.additem("48Khz",48000);
  sound_freq_dd.additem("44.1KHz",44100);
  sound_freq_dd.additem("25KHz",25033);
  sound_freq_dd.additem("22KHz",22050);
  sound_freq_dd.select_item_by_data(sound_chosen_freq);
  sound_freq_dd.grandfather=page_p;

  sound_format_label.create(XD,sound_group.handle,10+170,sgy,0,25,NULL,this,
    BT_LABEL,T("Format"),0,BkCol);

  sound_format_dd.create(XD,sound_group.handle,15+sound_format_label.w+170,
    sgy,370-(15+sound_format_label.w+170),200,dd_notify_proc,this);
  sound_format_dd.id=5003;
  FillSoundDevicesDD();

  sgy+=LineHeight;
  but=new hxc_button(XD,sound_group.handle,10,sgy,0,25,NULL,this,BT_LABEL,T("Buffer"),0,BkCol);

  dd=new hxc_dropdown(XD,sound_group.handle,15+but->w,sgy,150/*(sound_group.w-10-(15+but->w))*/,
    300,dd_notify_proc,this);
  dd->id=5004;
  EasyStr Fr=T(" Frame(s)");
  for(int i=1;i<=15;i++) 
    dd->additem(Str(i)+Fr,i);
  dd->sel=0;
  dd->select_item_by_data(psg_write_n_screens_ahead);
  
  sgy+=LineHeight;

  sound_vol_label.create(XD,sound_group.handle,10,sgy,0,25,NULL,this,BT_LABEL,
    T("Volume"),0,BkCol);
    
  mainvol_sb.horizontal=true;
  
  
    //int db=SoundVolume;
    //int position=(int)pow(10.0,log10(101.0)*(db+10000)/10000)-1;
  int position=SoundVolume+10000;
  //int _range,int _viewrange,int _pos
  mainvol_sb.init(10000+1,0,position);
  mainvol_sb.create(XD,sound_group.handle,15+sound_vol_label.w,sgy,370-(15+sound_vol_label.w),25,scrollbar_notify_proc,this);
  mainvol_sb.id=17;    


  y+=180;

#if defined(SSE_YM2149_LL)
  ymll_but.create(XD,page_p,page_l,y,0,25,
    button_notify_proc,this,BT_CHECKBOX,T("Low-level YM"),4019,BkCol);
  if(!Psg.LoadFixedVolTable())
    OPTION_MAME_YM=FALSE;
  ymll_but.set_check(OPTION_MAME_YM);
  hints.add(ymll_but.handle,
    T("Using MAME's AY8910 emu (thx Couriersud) and LJBK's YM samples"),page_p);

  if(!OPTION_MAME_YM)
  {
    Psg.FreeFixedVolTable();
    sound_mode_label.create(XD,page_p,page_l+130,y,0,25,NULL,this,BT_LABEL,
      T("Output type"),0,BkCol);
    sound_mode_dd.create(XD,page_p,page_l+130+5+sound_mode_label.w,y,
      page_w-(5+sound_mode_label.w+130),200,dd_notify_proc,this);
    sound_mode_dd.id=5001;
    sound_mode_dd.make_empty();
    sound_mode_dd.additem(T("Line (direct)"));
    sound_mode_dd.additem(T("TV (coaxial)"));
    sound_mode_dd.additem(T("Monitor (SCART)"));
    sound_mode_dd.changesel(psg_hl_filter-1);
  }
  else
  {
    sound_mode_label.create(XD,page_p,page_l+150,y,0,25,NULL,this,BT_LABEL,
      T("Filter"),0,BkCol);
    antialias_sb.horizontal=true;
    //int _range,int _viewrange,int _pos
    antialias_sb.init(YM_LOW_PASS_MAX+1,0,OPTION_LOWPASS);
    antialias_sb.create(XD,page_p,page_l+210,y,page_w-210,25,scrollbar_notify_proc,this);
    antialias_sb.id=16;    
  }
  y+=LineHeight;

#endif



#ifdef SSE_SOUND_MICROWIRE_OPTION
  ste_microwire_but.create(XD,page_p,page_l,y,0,25,
    button_notify_proc,this,BT_CHECKBOX,T("Microwire"),4009,BkCol);
  ste_microwire_but.set_check(OPTION_MICROWIRE);
  hints.add(ste_microwire_but.handle,
  T("Microwire (STE sound), incomplete emulation"),page_p);
#endif

#if defined(SSE_SOUND_MICROWIRE_HACKS)
  if(OPTION_HACKS)
  {
    ste_ym12db_but.create(XD,page_p,page_l+150,y,0,25,
      button_notify_proc,this,BT_CHECKBOX,T("YM-12db"),IDC_YM_12DB,BkCol);
    //ste_ym12db_but.set_check(OPTION_YM_12DB);
    ste_ym12db_but.set_check(Microwire.PsgReduce==2); // TODO sliders (v421)
    hints.add(ste_ym12db_but.handle,T("Hack, not available on regular STE"),page_p);
  }
#endif
  y+=LineHeight;

  record_group.create(XD,page_p,page_l,y,page_w,90,NULL,this,
    BT_GROUPBOX,T("Record"),0,BkCol);

  record_but.set_icon(&Ico32,ICO32_RECORD);
  record_but.create(XD,record_group.handle,10,25,25,25,button_notify_proc,this,
    BT_ICON,"",5100,BkCol);
  record_but.set_check(bSoundRecord);

  wav_choose_but.create(XD,record_group.handle,record_group.w-10,25,0,25,button_notify_proc,this,
    BT_TEXT,T("Choose"),5101,BkCol);
  wav_choose_but.x-=wav_choose_but.w;
  XMoveWindow(XD,wav_choose_but.handle,wav_choose_but.x,wav_choose_but.y);

  wav_output_label.create(XD,record_group.handle,40,25,wav_choose_but.x-10-40,25,NULL,this,
    BT_TEXT | BT_BORDER_INDENT | BT_STATIC | BT_TEXT_PATH,
    WAVOutputFile,0,WhiteCol);

  overwrite_ask_but.create(XD,record_group.handle,10,55,0,25,button_notify_proc,this,
    BT_CHECKBOX,T("Warn before overwrite"),5102,BkCol);
  overwrite_ask_but.set_check(RecordWarnOverwrite);
  y+=100;
#if !defined(SSE_NO_INTERNAL_SPEAKER)
  internal_speaker_but.create(XD,page_p,page_l,y,0,25,button_notify_proc,this,
    BT_CHECKBOX,T("Internal speaker sound"),5300,BkCol);
  internal_speaker_but.set_check(sound_internal_speaker);
#endif

#if defined(SSE_DRIVE_SOUND)
  drive_sound_but.create(XD,page_p,page_l,y,0,25,
    button_notify_proc,this,BT_CHECKBOX,T("Drive Sound"),IDC_DRIVE_SOUND,BkCol);
  drive_sound_but.set_check(OPTION_DRIVE_SOUND);
  hints.add(drive_sound_but.handle,
    T("Bad imitation of a SainT feature. You can choose the sounds directory in\
 the disk manager. PulseAudio recommended."),
    page_p); 
    
  drivevol_sb.horizontal=true;
  //int _range,int _viewrange,int _pos
  drivevol_sb.init(10000,0,10000+FloppyDrive[0].SoundVolume);
  drivevol_sb.create(XD,page_p,page_l+200,y,page_w-200,25,scrollbar_notify_proc,this);
  drivevol_sb.id=15;
#endif

#endif//UNIX
}


#ifdef WIN32

void TOptionBox::CreateMIDIPage() {

  int& y=mY,& Offset=mOffset,& Wid=mWid;
  HWND Win;

#if defined(SSE_DIRECTMIDI)
  DWORD style=(SSEConfig.DirectMusic) ? (WS_CHILD|WS_TABSTOP|BS_CHECKBOX)
                                     : (WS_CHILD|WS_TABSTOP|BS_CHECKBOX|WS_DISABLED);
  Win=CreateButton("Use DirectMusic",IDC_DIRECTMUSIC,style); // general option
  SendMessage(Win,BM_SETCHECK,OPTION_DIRECTMUSIC,0);
  ToolAddWindow(ToolTip,Win,T("Using DirectMIDI by Carlos Jimenez de Parga"));
#endif

  Win=CreateButton(T("Use timer"),IDC_MIDIUSETIMER);
  SendMessage(Win,BM_SETCHECK,((SSEOptions.MidiUseTimer)?BST_CHECKED:BST_UNCHECKED),0);
  ToolAddWindow(ToolTip,Win,T("Could make MIDI output smoother... or not"));

  Win=CreateButton(T("Use Sleep"),IDC_MIDIUSESLEEP);
  SendMessage(Win,BM_SETCHECK,((SSEOptions.MidiUseSleep)?BST_CHECKED:BST_UNCHECKED),0);
  ToolAddWindow(ToolTip,Win,T("Could make MIDI input smoother... or not"));

#if defined(SSE_DIRECTMIDI)
  NextLine();

  if(OPTION_DIRECTMUSIC)
  {
    CreateStatic(T("Master clock"));
    Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,
      Offset,y,8*CbUnits,200,Handle,(HMENU)IDC_MIDICLOCK,hInstance,NULL);
    DWORD c=DirectMidiClock.GetNumClocks();
    for(DWORD i=0;i<c;i++)
    {
      CLOCKINFO ClockInfo;
      DirectMidiClock.GetClockInfo(i,&ClockInfo);
      SendMessage(Win,CB_ADDSTRING,0,(LPARAM)ClockInfo.szClockDescription);
    }
    SendMessage(Win,CB_SETCURSEL,DirectMidiClockIndex,0);
  }
  else
#endif//#if defined(SSE_DIRECTMIDI)  
  {
    CreateStatic(T("Volume"));
    Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,
      Offset,y,page_w-Offset+page_l,SliderHeight,Handle,(HMENU)IDC_MIDIVOL,hInstance,NULL);
    SendMessage(Win,TBM_SETRANGEMAX,0,0xffff);
    SendMessage(Win,TBM_SETPOS,TRUE,MIDI_out_volume);
    SendMessage(Win,TBM_SETLINESIZE,0,0xff);
    SendMessage(Win,TBM_SETPAGESIZE,0,0xfff);
  }
  NextLine();

  CreatePortOptions(TSTPort::MIDI,y);

  y+=LineStart;

  CreateWindow("Button","Sysex",WS_CHILD|BS_GROUPBOX,
    page_l,y,page_w,GROUP_HEIGHT(6),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
  y+=mGroupTitleHeight;
  Offset+=LineStart;

#if defined(SSE_MIDIRAW)
/*  this option seems more definitive than the "Allow" ones which seem permissive
*   but which we don't want to break... */
  Win=CreateButton("Raw MIDI",IDC_RAWMIDI);
  SendMessage(Win,BM_SETCHECK,OPTION_RAWMIDI,0);
  ToolAddWindow(ToolTip,Win,T("No SYSEX interpretation at all"));  
#endif

  Win=CreateButton(T("Allow running status for output"),IDC_SYSEXSTATUSOUT);
  SendMessage(Win,BM_SETCHECK,((MIDI_out_running_status_flag==
    MIDI_ALLOW_RUNNING_STATUS)?BST_CHECKED:BST_UNCHECKED),0);
  //NextLine();
  //Offset+=LineStart;
  //Win=CreateButton(T("Allow running status for input"),IDC_SYSEXSTATUSIN);
  Win=CreateButton(T("input"),IDC_SYSEXSTATUSIN);
  SendMessage(Win,BM_SETCHECK,((MIDI_in_running_status_flag==
    MIDI_ALLOW_RUNNING_STATUS)?BST_CHECKED:BST_UNCHECKED),0);
  NextLine();
  Offset+=LineStart;
  CreateStatic(T("Available for output"));
  Wid=CbUnits*2;
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,
  Offset,y,Wid,200,Handle,(HMENU)IDC_SYSEXBUFOUT,hInstance,NULL);
  for(int n=2;n<MAX_SYSEX_BUFS;n++)
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)Str(n).Text);
  SendMessage(Win,CB_SETCURSEL,MIDI_out_n_sysex-2,0);
  Offset+=Wid+HorizontalSeparation;
  CreateStatic(T("size"));
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,
    Offset,y,3*CbUnits,200,Handle,(HMENU)IDC_SYSEXSIZEOUT,hInstance,NULL);
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"16Kb");
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"32Kb");
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"64Kb");
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"128Kb");
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"256Kb");
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"512Kb");
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"1Mb");
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"2Mb");
  SendMessage(Win,CB_SETCURSEL,log_to_base_2(MIDI_out_sysex_max/1024)-4,0);
  NextLine();
  Offset+=LineStart;
  CreateStatic(T("Available for input"));
  Wid=CbUnits*2;
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|WS_VSCROLL
    |CBS_DROPDOWNLIST,Offset,y,Wid,200,Handle,(HMENU)IDC_SYSEXBUFIN,hInstance,NULL);
  for(int n=2;n<MAX_SYSEX_BUFS;n++)
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)Str(n).Text);
  SendMessage(Win,CB_SETCURSEL,MIDI_in_n_sysex-2,0);
  Offset+=Wid+HorizontalSeparation;
  CreateStatic(T("size"));
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,
    Offset,y,3*CbUnits,200,Handle,(HMENU)IDC_SYSEXSIZEIN,hInstance,NULL);
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"16Kb");
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"32Kb");
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"64Kb");
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"128Kb");
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"256Kb");
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"512Kb");
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"1Mb");
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"2Mb");
  SendMessage(Win,CB_SETCURSEL,log_to_base_2(MIDI_in_sysex_max/1024)-4,0);
  NextLine();
  Offset+=LineStart;

#if defined(SSE_GEM_CONTROL_PANEL)
  CreateStatic("Input speed");
  int w=32,d=10;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,
    (HMENU)(IDS_TORTOISE),hInstance,NULL);
  Offset+=w+d;
  Wid=page_w-Offset+page_l-w-LineStart-d;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,Offset,
    y,Wid,mSliderHeight,Handle,(HMENU)IDC_MIDISPEED,hInstance,NULL);
  Offset+=Wid;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,
    (HMENU)(IDS_RABBIT),hInstance,NULL);
#else
  CreateWindow("Static",T("Input speed")+": "+Str(MIDI_in_speed)+"%",WS_CHILD|SS_CENTER
    ,page_l+LineStart,y,page_w-LineStart*2,20,Handle,(HMENU)IDS_MIDISPEED,hInstance,NULL);
  y+=20;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,page_l+LineStart,
    y,page_w-LineStart*2,mSliderHeight,Handle,(HMENU)IDC_MIDISPEED,hInstance,NULL);
#endif

  //SendMessage(Win,TBM_SETRANGEMAX,0,99);
  //SendMessage(Win,TBM_SETRANGEMAX,1,100);
  SendMessage(Win,TBM_SETRANGEMAX,0,100);
  SendMessage(Win,TBM_SETPOS,TRUE,MIDI_in_speed);//-1);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETPAGESIZE,0,5);
}


void TOptionBox::AssAddToExtensionsLV(char *Ext,char *Desc,INT_PTR Num) {

  int &y=mY,&Offset=mOffset;
  Offset-=page_l-HorizontalSeparation;
  EasyStr Text=Str(Ext)+" ("+Desc+")";
  y=LineStart+LineHeight*(int)Num;
  int ButWid=MAX(GetTextSize(Font,T("Associated")).Width,
    GetTextSize(Font,T("Associate")).Width)+HorizontalSeparation;
  //Offset+=HorizontalSeparation;
  HWND But=CreateWindow("Button","",WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE,
    Offset,y,ButWid,CharHeight,Scroller.GetControlPage(),(HMENU)(IDC_ASSOCIATE+Num),hInstance,NULL);
  Offset+=ButWid+HorizontalSeparation;
  int w=GetTextSize(Font,Text).Width+HorizontalSeparation;
  HWND Stat=CreateWindow("Static",Text,WS_CHILD|SS_CENTERIMAGE,Offset,
    mY,w,CharHeight,Scroller.GetControlPage(),(HMENU)5000,hInstance,NULL);
  SendMessage(Stat,WM_SETFONT,WPARAM(Font),0);
  SendMessage(But,WM_SETFONT,WPARAM(Font),0);
  SendMessage(But,WM_SETTEXT,0,(LPARAM) ((IsSteemAssociated(Ext))
    ?(T("Associated").Text):T("Associate").Text)) ;
  ShowWindow(Stat,SW_SHOW);
  ShowWindow(But,SW_SHOW);
  NextLine();
}


void TOptionBox::CreateAssocPage() {

  int &y=mY;

#if defined(SSE_GUI_KBD)
  Scroller.CreateEx(WS_EX_CONTROLPARENT|WS_EX_CLIENTEDGE,WS_CHILD|WS_VSCROLL|WS_HSCROLL,
    page_l,y,page_w,OPTIONS_HEIGHT-y-LineHeight,Handle,(HMENU)IDC_ASSOCSCRL,hInstance);
#else
  Scroller.CreateEx(512,WS_CHILD|WS_VSCROLL|WS_HSCROLL,
    page_l,10,page_w,OPTIONS_HEIGHT-10-10-25-10,Handle,(HMENU)IDC_ASSOCSCRL,hInstance);
#endif
  //Scroller.SetBkColour(GetSysColor(COLOR_WINDOW));
  Scroller.SetBkColour(GetSysColor(COLOR_BTNFACE));
  AssAddToExtensionsLV(dot_ext(EXT_ST),T("Disk Image"),0);
#if USE_PASTI || defined(SSE_DISK_STX)
  AssAddToExtensionsLV(dot_ext(EXT_STX),T("Disk Image"),1);
#else
  AssAddToExtensionsLV(dot_ext(EXT_STT),T("Disk Image"),1);
#endif
  AssAddToExtensionsLV(dot_ext(EXT_MSA),T("Disk Image"),2);
#if defined(SSE_DISK_STW) // STW instead because it's less likely to be zipped
  AssAddToExtensionsLV(dot_ext(EXT_STW),T("STW Disk Image"),3);
#elif USE_PASTI
  if(hPasti) AssAddToExtensionsLV(dot_ext(EXT_STX),T("Pasti Disk Image"),3);
#endif
  AssAddToExtensionsLV(dot_ext(EXT_DIM),T("Disk Image"),4);
  AssAddToExtensionsLV(".STZ",T("Zipped Disk Image"),5); // anybody uses that?
  AssAddToExtensionsLV(".STS",T("Memory Snapshot"),6);
#if defined(SSE_DISK_HFE) // more useful
  AssAddToExtensionsLV(dot_ext(EXT_HFE),T("ST/HxC Disk Image"),7);
#else
  AssAddToExtensionsLV(".STC",T("Cartridge Image"),7); //too rare
#endif
#if defined(SSE_TOS_PRG_AUTORUN)
  AssAddToExtensionsLV(dot_ext(EXT_PRG),T("Atari PRG executable"),8);
  AssAddToExtensionsLV(dot_ext(EXT_TOS),T("Atari TOS executable"),9);
#endif
  Scroller.AutoSize(5,5);
#if !defined(SSE_ONEINSTANCE)
  y=OPTIONS_HEIGHT-LineHeight;
  HWND Win=CreateButton(T("Always open files in new window"),5502);
  SendMessage(Win,BM_SETCHECK,GetCSFInt("Options","OpenFilesInNew",TRUE,
    globalINIFile),0);
#endif
}


void TOptionBox::IconsAddToScroller() {
  for(int n=IDC_ICONSBASE;n<IDC_ICONSBASE+RC_NUM_ICONS;n++)
    if(HWND h=GetDlgItem(Scroller.GetControlPage(),n)) 
      DestroyWindow(h);
  int x=3,y=3;
  for(int want_size=16;want_size;want_size<<=1) 
  {
    for(INT_PTR n=1;n<RC_NUM_ICONS;n++) 
    {
      int size=RCGetSizeOfIcon(n) & ~1;
#ifdef DEADC0DE // Sorry Frenchies
      switch(n) {
      case RC_ICO_HARDDRIVES:
      case RC_ICO_HARDDRIVES_FR:
        int want_ico=RC_ICO_HARDDRIVES;
        if(IsSameStr_I(T("File"),"Fichier"))
          want_ico=RC_ICO_HARDDRIVES_FR;
        if(n!=want_ico)
          size=0;
        break;
      }
#endif
      if(n==RC_ICO_HARDDRIVES_FR)
        continue; // skip it or there's a blank icon
      if(BIG_ICONS && size==16)
        size=32; // quick fix, TODO
      if(size==want_size) 
      {
#if defined(SSE_GUI_KBD)
        /*HWND Win=*/CreateWindow("Steem Flat PicButton",Str(n),WS_CHILD|PBS_RIGHTCLICK|WS_TABSTOP,
          x,y,size+4,size+4,Scroller.GetControlPage(),(HMENU)(IDC_ICONSBASE+n),hInstance,NULL);
        x+=size+4+3;
#else
        CreateWindow("Steem Flat PicButton",Str(n),WS_CHILD|PBS_RIGHTCLICK,
          x,y,size+4,size+4,Scroller.GetControlPage(),HMENU(IDC_ICONSBASE+n),hInstance,NULL);
        x+=size+4+3;
#endif
      }
      if(x+size+4+3>=page_w-GuiSM.cx_vscroll()||n==RC_NUM_ICONS-1) 
      {
        x=3;
        y+=size+4+3;
      }
    }
  }
  for(int n=IDC_ICONSBASE;n<IDC_ICONSBASE+RC_NUM_ICONS;n++)
    if(HWND h=GetDlgItem(Scroller.GetControlPage(),n)) 
      ShowWindow(h,SW_SHOWNA);
  Scroller.AutoSize(0,5);
}


void TOptionBox::CreateIconsPage() { // not for BIG_ICONS
  
  int &y=mY;

  CreateStatic(T("Left click to change, right to reset"));
  NextLine();
  int scroller_h=OPTIONS_HEIGHT-y-CharHeight*2-LineStart;
#if defined(SSE_GUI_KBD)
  Scroller.CreateEx(WS_EX_CONTROLPARENT|WS_EX_CLIENTEDGE,WS_CHILD|WS_VSCROLL|
    WS_HSCROLL,page_l,y,page_w,scroller_h,Handle,(HMENU)14010,hInstance);
#else
  Scroller.CreateEx(512,WS_CHILD|WS_VSCROLL|WS_HSCROLL,page_l,y,
    page_w,scroller_h,Handle,(HMENU)14010,hInstance);
#endif
  Scroller.SetBkColour(GetSysColor(COLOR_BTNFACE));
  IconsAddToScroller();
  y+=scroller_h+LineStart;
  DWORD mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE;
  CreateButton(T("Load Icon Scheme"),IDC_LOADICONS,mask);
  CreateButton(T("All Icons To Default"),IDC_ICONSDEF,mask);
}

#endif//WIN32


#ifdef UNIX

void TOptionBox::FillSoundDevicesDD()
{
  hxc_dropdown *dd=(hxc_dropdown*)hxc::find(sound_group.handle,5000);
  dd->make_empty();
#ifndef NO_PORTAUDIO  //NO_PORTAUDIO
  if (UseSound==XS_PA){
    int c=Pa_GetDeviceCount(),isel=Pa_GetDefaultOutputDevice();
    for (PaDeviceIndex i=0;i<c;i++){
      const PaDeviceInfo *pdev=Pa_GetDeviceInfo(i);
      if (pdev->maxOutputChannels>0){
        dd->additem((char*)(pdev->name),i);
        if (IsSameStr_I(pdev->name,sound_device_name)) isel=i;
      }
    }
    dd->select_item_by_data(MAX(isel,0));
    sound_device_name=dd->sl[dd->sel].String; // Just in case changed to default
  }
#endif
#ifndef NO_RTAUDIO
  if (UseSound==XS_RT){
    RtAudio::DeviceInfo radi;
    int c=rt_audio->getDeviceCount(),isel=0; //isel=default device, find while walking through list
#if defined(SSE_UNIX)
    for (int i=0;i<c;i++){ // odd bug, the system must have changed
#else
    for (int i=1;i<=c;i++){
#endif
      radi=rt_audio->getDeviceInfo(i);
      if (radi.outputChannels>0){
        dd->additem((char*)(radi.name.c_str()),i);
        if (radi.isDefaultOutput) isel=i;
      }
    }
    for (int i=1;i<=c;i++){
      if (IsSameStr_I(radi.name.c_str(),sound_device_name)) isel=i;
    }
    dd->select_item_by_data(MAX(isel,0));
    sound_device_name=dd->sl[dd->sel].String; // Just in case changed to default
  }
#endif

#if defined(SSE_UNIX_PULSEAUDIO)
  if (UseSound==XS_PULSE){
    char *DevName=(char*)malloc(16*512);
    int c=Pulse_DeviceList(DevName),isel=0;
    for (int i=0;i<c;i++){
      dd->additem(&DevName[512*i],i);
      if (IsSameStr_I(&DevName[512*i],sound_device_name)) isel=i;
    }
    free(DevName);
    dd->select_item_by_data(MAX(isel,0));
    sound_device_name=dd->sl[dd->sel].String; // Just in case changed to default
  }
#endif

  if (UseSound==0) dd->additem(T("None"));

#ifdef NO_RTAUDIO
  int rt_unsigned_8bit=0;
#endif
  sound_format_dd.make_empty();
  sound_format_dd.additem(T("8-Bit Mono"),MAKEWORD(8,1));
  sound_format_dd.additem(T("8-Bit Stereo"),MAKEWORD(8,2));
  if (x_sound_lib==XS_RT){
    sound_format_dd.additem(T("8-Bit Mono Unsigned"),MAKELONG(MAKEWORD(8,1),1));
    sound_format_dd.additem(T("8-Bit Stereo Unsigned"),MAKELONG(MAKEWORD(8,2),1));
  }
  sound_format_dd.additem(T("16-Bit Mono"),MAKEWORD(16,1));
  sound_format_dd.additem(T("16-Bit Stereo"),MAKEWORD(16,2));
  sound_format_dd.sel=-1;
  sound_format_dd.select_item_by_data(MAKELONG(MAKEWORD(sound_num_bits,sound_num_channels),rt_unsigned_8bit));
  if (sound_format_dd.sel==-1){
    sound_format_dd.select_item_by_data(MAKEWORD(sound_num_bits,sound_num_channels));
  }
}


void TOptionBox::CreatePathsPage()
{
  int y=LineStart;
  hxc_edit *p_ed;
  hxc_button *p_but;
  
  Str Comline_Desc[NUM_COMLINES];
  Comline_Desc[COMLINE_HTTP]=T("Web");
  Comline_Desc[COMLINE_FTP]=T("FTP");
  Comline_Desc[COMLINE_MAILTO]=T("E-mail");
  Comline_Desc[COMLINE_FM]=T("File Manager");
  Comline_Desc[COMLINE_FIND]=T("Find File(s)");

  for (int i=0;i<NUM_COMLINES;i++){
    p_but=new hxc_button(XD,page_p,page_l,y,0,25,NULL,this,BT_LABEL,Comline_Desc[i],0,BkCol);

    p_ed=new hxc_edit(XD,page_p,page_l+p_but->w+5,y,page_w-p_but->w-5-20,25,edit_notify_proc,this);
    p_ed->set_text(Comlines[i]);
    p_ed->id=15000+i*10;
  
    p_but=new hxc_button(XD,page_p,page_l+page_w-20,y,20,25,button_notify_proc,this,
                        BT_ICON,"",15001+i*10,BkCol);
    p_but->set_icon(NULL,1);
    y+=LineHeight;
  }
  
  
}

#endif//UNIX


#if !defined(SSE_NO_UPDATE)

void TOptionBox::CreateUpdatePage() {
  int Wid;

  TConfigStoreFile CSF(globalINIFile);
  DWORD Disable=DWORD(Exists(RunDir+"\\SteemUpdate.exe") ? 0:WS_DISABLED);
  int Runs=CSF.GetInt("Update","Runs",0),
      Offline=CSF.GetInt("Update","Offline",0),
      WSError=CSF.GetInt("Update","WSError",0),
      y=10;

  EasyStr Info=EasyStr(" ");
  Info+=T("Update has checked for a new Steem")+" "+Runs+" "+time_or_times(Runs)+"\n ";
  Info+=T("It thought you were off-line")+" "+Offline+" "+time_or_times(Offline)+"\n ";
  Info+=T("It encountered an error")+" "+WSError+" "+time_or_times(WSError)+"\n ";
  CreateWindowEx(512,"Static",Info,WS_CHILD | Disable,
                  page_l,y,page_w,80,Handle,(HMENU)4100,hInstance,NULL);
  y+=90;


  Wid=GetCheckBoxSize(Font,T("Disable automatic update checking")).Width;
  HWND ChildWin=CreateWindow("Button",T("Disable automatic update checking"),WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX | Disable,
                          page_l,y,Wid,20,Handle,(HMENU)4200,hInstance,NULL);
  SendMessage(ChildWin,BM_SETCHECK,!CSF.GetInt("Update","AutoUpdateEnabled",true),0);
  y+=LineHeight;

  Wid=GetCheckBoxSize(Font,T("This computer is never off-line")).Width;
  ChildWin=CreateWindow("Button",T("This computer is never off-line"),WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX | Disable,
                          page_l,y,Wid,20,Handle,(HMENU)4201,hInstance,NULL);
  SendMessage(ChildWin,BM_SETCHECK,CSF.GetInt("Update","AlwaysOnline",0),0);
  y+=LineHeight;

  Wid=GetCheckBoxSize(Font,T("Download new patches")).Width;
  ChildWin=CreateWindow("Button",T("Download new patches"),WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX | Disable,
                          page_l,y,Wid,20,Handle,(HMENU)4202,hInstance,NULL);
  SendMessage(ChildWin,BM_SETCHECK,CSF.GetInt("Update","PatchDownload",1),0);
  y+=LineHeight;

  Wid=GetCheckBoxSize(Font,T("Ask before installing new patches")).Width;
  ChildWin=CreateWindow("Button",T("Ask before installing new patches"),WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX | Disable,
                          page_l,y,Wid,20,Handle,(HMENU)4203,hInstance,NULL);
  SendMessage(ChildWin,BM_SETCHECK,CSF.GetInt("Update","AskPatchInstall",0),0);
  y+=LineHeight;

  HANDLE UpdateMutex=OpenMutex(MUTEX_ALL_ACCESS,0,"SteemUpdate_Running");
  if (UpdateMutex){
    CloseHandle(UpdateMutex);
    Disable=WS_DISABLED;
  }else if (UpdateWin || FullScreen){
    Disable=WS_DISABLED;
  }
  CreateWindow("Button",T("Check For Update Now"),WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON | int(UpdateWin || FullScreen ? WS_DISABLED:0) | Disable,
                    page_l,y,page_w,23,Handle,(HMENU)4400,hInstance,NULL);

  CSF.Close();
}

#endif


#if !defined(SSE_GUI_421_REORDER)
void TOptionBox::CreateSSEPage() { // Misc.

  int &y=mY,&Offset=mOffset,&Wid=mWid;

#ifdef WIN32
  HWND Win;
  EasyStr tip_text;

#if defined(SSE_GUI_OPTION_FOR_TESTS)
  Win=CreateButton("Beta tests",IDC_BETATESTS);
  SendMessage(Win,BM_SETCHECK,SSEOptions.TestingNewFeatures,0);
  NextLine();
#else
  //ASSERT(!SSEOptions.TestingNewFeatures);
#endif

#if defined(SSE_GUI_TOOLBAR)
  Win=CreateButton(T("Tool bar"),IDC_TOOLBAR);
  SendMessage(Win,BM_SETCHECK,OPTION_TOOLBAR,0);
#endif

#if defined(SSE_GUI_MENUBAR)
  Win=CreateButton(T("Menu bar"),IDC_MENUBAR);
  SendMessage(Win,BM_SETCHECK,OPTION_MENUBAR,0);
#endif

#if defined(SSE_GUI_STATUS_BAR)
  Win=CreateButton(T("Status bar"),IDC_STATUSBAR);
  SendMessage(Win,BM_SETCHECK,OPTION_STATUS_BAR,0);
  Win=CreateButton(T("TOS Flag"),IDC_TOSFLAG);
  SendMessage(Win,BM_SETCHECK,OPTION_TOSFLAG,0);
#endif

  NextLine();
  Win=CreateButton(T("Show all settings"),IDC_ADVANCED_SETTINGS);
  SendMessage(Win,BM_SETCHECK,OPTION_ADVANCED,0);
  ToolAddWindow(ToolTip,Win,T("For even more options!"));

  Win=CreateButton(T("Reset settings"),IDC_ADVANCED_RESET,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);
  ToolAddWindow(ToolTip,Win,T("When it's FUBAR"));

  NextLine();
  Win=CreateButton(T("Hacks"),IDC_HACKS);
  SendMessage(Win,BM_SETCHECK,OPTION_HACKS,0);
  ToolAddWindow(ToolTip,Win,T("Some options not available on a real ST,\
 conveniences for the player or the programmer. Unchecking the option won't\
 remove all hacks that could be currently engaged, for that you need to reset\
 settings"));

  NextLine();
  Win=CreateButton(T("Emu detect"),IDC_EMUDETECT);
  SendMessage(Win,BM_SETCHECK,OPTION_EMU_DETECT,0);
  ToolAddWindow(ToolTip,Win,T("Enable communication between Steem and ST programs"));

#if defined(SSE_IKBD_RTC)
  if(OPTION_HACKS)
  {
    NextLine();
    Win=CreateButton(T("Clock always correct"),IDC_RTCHACK);
    SendMessage(Win,BM_SETCHECK,OPTION_RTC_HACK,0);
    ToolAddWindow(ToolTip,Win,T("Handy hack voiding any RTC emulation and its\
 problems, but can interfere with some programs"));
  }
#endif

#if defined(SSE_GUI_DEFAULT_ST_CONFIG)
  NextLine();
  Win=CreateButton(T("Default ST configs"),IDC_DEFCON);
  SendMessage(Win,BM_SETCHECK,OPTION_ST_PRESELECT,0);
  ToolAddWindow(ToolTip,Win,T("Look into the current configuration folder for a config with the same\
 name as the ST model when you change"));
#endif

  NextLine();
  Win=CreateButton(T("F11 F12 Emu control"),IDC_F12RUN);
  SendMessage(Win,BM_SETCHECK,SSEOptions.F12Run,0);
  ToolAddWindow(ToolTip,Win,T("F12: start/stop - F11: toggle capture mouse"));
  Win=CreateButton(T("Pause Emu control"),IDC_PAUSERUN);
  SendMessage(Win,BM_SETCHECK,SSEOptions.PauseRun,0);
  ToolAddWindow(ToolTip,Win,T("Pause: start/stop - Shift Pause: toggle capture mouse"));

  NextLine();
  Win=CreateButton(T("Save backup snapshot on reset"),IDC_RESETBACKUP);
  SendMessage(Win,BM_SETCHECK,SSEOptions.ResetBackup,0);

#if defined(SSE_GUI_INSTANTCHANGE)
  NextLine();
  Win=CreateButton(T("Instant machine change"),IDC_INSTANTMACHINECHANGE);
  SendMessage(Win,BM_SETCHECK,SSEOptions.InstantMachineChange,0);
  ToolAddWindow(ToolTip,Win,T("Don't wait for reboot on ST/RAM/ROM changes, useful in select cases"));
#endif

#if 1 // fancy
  NextLine();
  y=OPTIONS_HEIGHT-LineHeight*3;
  int size;
  HICON hIcon;
  int yIcon=y;
  char steem[64];
  sprintf(steem,"%s %s",gAppName,gAppBuildInfo);
  Wid=get_text_width(steem);
  if(BIG_ICONS)
  {
    hIcon=hGUIIcon[RC_ICO_APP];
    size=32;
  }
  else
  {
    hIcon=hGUIIconSmall[RC_ICO_APP];
    size=16;
    yIcon+=GUI_BIGFONT_SIZE-FONT_SIZE;
  }
  Offset+=page_w/2-(size+4+Wid)/2; // centre like a pro
  Win=CreateWindow("Static",NULL,WS_CHILD|WS_VISIBLE|SS_ICON|SS_CENTERIMAGE,
    Offset,yIcon,size,size,Handle,(HMENU)IDS_STATIC,hInstance,NULL);
  SendMessage(Win,STM_SETICON,(WPARAM)hIcon,0);
  Offset+=size+4; // > icon
  CreateStatic(steem);
#endif

#endif//WIN32

#ifdef UNIX

  specific_hacks_but.create(XD,page_p,page_l,y,0,25,
    button_notify_proc,this,BT_CHECKBOX,T("Hacks"),4003,BkCol);
  specific_hacks_but.set_check(OPTION_HACKS);
  hints.add(specific_hacks_but.handle,
    T("For an edgier emulation, recommended!"),page_p);
  y+=LineHeight;

  emudetect_but.create(XD,page_p,page_l,y,0,25,
    button_notify_proc,this,BT_CHECKBOX,T("Emu detect"),4004,BkCol);
  emudetect_but.set_check(OPTION_EMU_DETECT);
  hints.add(emudetect_but.handle,
    T("Enable easy detection of Steem by ST programs"),page_p);
  y+=LineHeight;
  
#if defined(SSE_IKBD_RTC)
  rtc_correct_but.create(XD,page_p,page_l,y,0,25,
    button_notify_proc,this,BT_CHECKBOX,T("Clock always correct"),4021,BkCol);
  rtc_correct_but.set_check(OPTION_RTC_HACK);
  hints.add(rtc_correct_but.handle,
    T("Handy hack voiding any RTC emulation and its\
 problems, but can interfere with some programs"),page_p);
  NextLine();
#endif

#if defined(SSE_PRINTER)
  printer_but.create(XD,page_p,page_l,y,0,25,button_notify_proc,this,
    BT_CHECKBOX,T("SMM804 Printer (Parallel port)"),4020,BkCol);
  hints.add(printer_but.handle,T("Epson-compatible"),page_p);
  printer_but.set_check(OPTION_PRINTER);
#endif
  y+=LineHeight;
#if defined(SSE_ACSI_LASER)
  laser_but.create(XD,page_p,page_l,y,0,25,button_notify_proc,this,
    BT_CHECKBOX,T("SLM804 Laser Printer (ACSI port)"),4018,BkCol);
  laser_but.set_check(OPTION_LASER);
#endif

#endif//UNIX

}
#endif//#if !defined(SSE_GUI_421_REORDER)


void TOptionBox::SSEUpdateIfVisible() {
  if(Handle==NULL)
    return;
#ifdef WIN32
#if defined(SSE_GUI_421_REORDER)
  // Update current page (not just SSE) simply by recreating it
  if(Page!=PAGE_MACHINE) // create calls update for Machine
  {
    DestroyCurrentPage();
    CreatePage(Page);
  }
  //REFRESH_STATUS_BAR; //overkill?
#else
  HWND Win;
#if defined(SSE_HD6301_LL) && !defined(SSE_NO_OLD_IKBD)
  Win=GetDlgItem(Handle,IDC_6301); //HD6301 emu (keyboard page now)
  if(Win!=NULL)
  {
    if(!HD6301_OK)
      SendMessage(Win,BN_DISABLE,0,0);
    else
      SendMessage(Win,BM_SETCHECK,OPTION_C1,0);
  }
#endif
  Win=GetDlgItem(Handle,IDC_VMMOUSE);
  if(Win!=NULL)
    SendMessage(Win,BM_SETCHECK,OPTION_VMMOUSE,0);
  REFRESH_STATUS_BAR_GX; //overkill
  UpdateSTVideoPage();
#if defined(SSE_GUI_STATUS_BAR)
  Win=GetDlgItem(Handle,IDC_STATUSBAR);
  SendMessage(Win,BM_SETCHECK,OPTION_STATUS_BAR,0);
#endif
#if defined(SSE_GUI_TOOLBAR)
  Win=GetDlgItem(Handle,IDC_TOOLBAR);
  SendMessage(Win,BM_SETCHECK,OPTION_TOOLBAR,0);
#endif
#if defined(SSE_GUI_MENUBAR)
  Win=GetDlgItem(Handle,IDC_MENUBAR);
  SendMessage(Win,BM_SETCHECK,OPTION_MENUBAR,0);
#endif
#endif//!SSE_GUI_421_REORDER
#endif//WIN32  
}


// page Keyboard/Mouse
void TOptionBox::CreateInputPage() {
  
  int &y=mY,&Offset=mOffset,&Wid=mWid;

#ifdef WIN32
  DWORD mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX;
  HWND Win;
#if defined(SSE_GUI_421_REORDER)
  CreateWindow("Button",T("Chip HD6301"),WS_CHILD|BS_GROUPBOX,
    page_l,y,page_w,GROUP_HEIGHT(2),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
#else
  CreateWindow("Button",T("Chip"),WS_CHILD|BS_GROUPBOX,
    page_l,y,page_w,GROUP_HEIGHT(2),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
#endif
  y+=mGroupTitleHeight;
  Offset+=LineStart;

#if defined(SSE_HD6301_LL)
#if 0 && defined(SSE_NO_OLD_IKBD) // option visible and checked but can't be changed and tip won't show
  Win=CreateButton(T("Low-level emulation (C1)"),IDC_6301,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|WS_DISABLED);
#elif defined(SSE_GUI_421_REORDER)
  Win=CreateButton(T("Low-level emulation (C1)"),IDC_6301);
#else
  Win=CreateButton(T("Low-level 6301 emulation (C1)"),IDC_6301);
#endif
  SendMessage(Win,BM_SETCHECK,OPTION_C1,0);
  ToolAddWindow(ToolTip,Win,
    T("This enables a low-level emulation of the IKBD keyboard chip (using\
 the Sim6xxx code by Arne Riiber, thx dude!), and more precise ACIA timings (\
important for MIDI emulation too)."));
#endif

#if defined(SSE_IKBD_RTC)
  NextLine();
  Offset+=LineStart;
  CreateStatic(T("Battery"));
  mask=WS_CHILD|BS_AUTORADIOBUTTON;
  Win=CreateButton(T("No"),IDC_RADIO_6301BTRY,mask|WS_GROUP);
  ToolAddWindow(ToolTip,Win,T("Like on most real STs"));
  Win=CreateButton(T("Yes"),IDC_RADIO_6301BTRY+1,mask);
  ToolAddWindow(ToolTip,Win,
    T("Implies that the 6301 clock is set at the correct time on power on"));
  if(OPTION_HACKS)
  {
    Win=CreateButton(T("Yes, 2000-ready"),IDC_RADIO_6301BTRY+2,mask);
    ToolAddWindow(ToolTip,Win,T("Based on TzOk's HW hack to circle around the Y2K bug"));
  }
  SendMessage(GetDlgItem(Handle,IDC_RADIO_6301BTRY+OPTION_BATTERY6301),BM_SETCHECK,TRUE,0);
#endif  

  NextLine();
#if defined(SSE_GUI_421_REORDER)
  CreateWindow("Button",T("Keyboard"),WS_CHILD|BS_GROUPBOX,page_l,y,page_w,GROUP_HEIGHT(3),
               Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
#else
  CreateWindow("Button",T("Keyboard"),WS_CHILD|BS_GROUPBOX,
    page_l,y,page_w,GROUP_HEIGHT(2),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
#endif
  y+=mGroupTitleHeight;
  Offset+=LineStart;
  CreateStatic(T("Host layout"));
  Wid=CbUnits*8;
  HWND Combo=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST
    |WS_VSCROLL,Offset,y,Wid,200,Handle,(HMENU)IDC_KBDLANG,hInstance,NULL);
  CBAddString(Combo,T("United States"),MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US));
  CBAddString(Combo,T("United Kingdom"),MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_UK));
  //CBAddString(Combo,T("Australia (UK TOS)"),MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_AUS));
  CBAddString(Combo,T("Australia"),MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_AUS));
  CBAddString(Combo,T("German"),MAKELANGID(LANG_GERMAN,SUBLANG_GERMAN));
  CBAddString(Combo,T("French"),MAKELANGID(LANG_FRENCH,SUBLANG_FRENCH));
  CBAddString(Combo,T("Spanish"),MAKELANGID(LANG_SPANISH,SUBLANG_SPANISH));
  CBAddString(Combo,T("Italian"),MAKELANGID(LANG_ITALIAN,SUBLANG_ITALIAN));
  CBAddString(Combo,T("Swedish"),MAKELANGID(LANG_SWEDISH,SUBLANG_SWEDISH));
  CBAddString(Combo,T("Norwegian"),MAKELANGID(LANG_NORWEGIAN,SUBLANG_NEUTRAL));
  //CBAddString(Combo,T("Belgian (French TOS)"),MAKELANGID(LANG_FRENCH,SUBLANG_FRENCH_BELGIAN));
  CBAddString(Combo,T("Belgian"),MAKELANGID(LANG_FRENCH,SUBLANG_FRENCH_BELGIAN));
#if defined(SSE_IKBD_MAPPINGFILE)
  CBAddString(Combo,T("Mapping file"),MAKELANGID(LANG_CUSTOM,SUBLANG_NEUTRAL));
  if(KeyboardLangID==LANG_CUSTOM)  // hovering gives the file name
    ToolAddWindow(ToolTip,Combo,GetFileNameFromPath(KeyboardMappingPath.Text));
#endif
  if(CBSelectItemWithData(Combo,KeyboardLangID)<0)
    SendMessage(Combo,CB_SETCURSEL,0,0);
  NextLine();
  Offset+=LineStart;
  Win=CreateButton(T("Shift and alternate correction"),IDC_KBDALTSHIFT,WS_CHILD
    |WS_TABSTOP|BS_AUTOCHECKBOX);
  SendMessage(Win,BM_SETCHECK,bEnableShiftSwitching,0);
  ToolAddWindow(ToolTip,Win,
    T("When checked this allows Steem to emulate all keys correctly, it does \
this by changing the shift and alternate state of the ST when you press them.")
+" "+  T("This could interfere with games and other programs, only use it if \
you are doing lots of typing.")+" "+ T("Please note that instead of pressing \
Alt-Gr or Control to access characters on the right-hand side of a key, you \
have to press Alt or Alt+Shift (this is how it was done on an ST)."));

#if defined(SSE_TOS_KEYBOARD_CLICK)
  if(OPTION_HACKS)
  {
    Win=CreateButton(T("Click"),IDC_KEYBOARDCLICK);
    SendMessage(Win,BM_SETCHECK,OPTION_KEYBOARD_CLICK,0);
    ToolAddWindow(ToolTip,Win,
      T("This uses address $484, changing before reset is safer - \
MUST be checked for some programs"));
  }
#endif

#if defined(SSE_GUI_421_REORDER)
  NextLine();
  Offset+=LineStart;
  Win=CreateButton(T("F11 F12 Emu control"),IDC_F12RUN);
  SendMessage(Win,BM_SETCHECK,SSEOptions.F12Run,0);
  ToolAddWindow(ToolTip,Win,T("F12: start/stop - F11: toggle capture mouse"));
  Win=CreateButton(T("Pause Emu control"),IDC_PAUSERUN);
  SendMessage(Win,BM_SETCHECK,SSEOptions.PauseRun,0);
  ToolAddWindow(ToolTip,Win,T("Pause: start/stop - Shift Pause: toggle capture mouse"));
#endif

  NextLine();
#if defined(SSE_GEM_CONTROL_PANEL)
  CreateWindow("Button",T("Mouse"),WS_CHILD|BS_GROUPBOX,page_l,y,page_w,
    GROUP_HEIGHT(4),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
#else
  CreateWindow("Button",T("Mouse"),WS_CHILD|BS_GROUPBOX,page_l,y,page_w,
    GROUP_HEIGHT(3),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
#endif
  y+=mGroupTitleHeight;
  Offset+=LineStart;
  CreateStatic(T("Capture"));
  mask=WS_CHILD|BS_AUTORADIOBUTTON;
  Win=CreateButton(T("Off"),IDC_RADIO_CAPTURE_MOUSE,mask|WS_GROUP); // 0
  ToolAddWindow(ToolTip,Win,T("Mouse is free until you press F11"));
  Win=CreateButton(T("On"),IDC_RADIO_CAPTURE_MOUSE+1,mask); // 1
  ToolAddWindow(ToolTip,Win,T("Mouse is grabbed when emulation starts"));
  Win=CreateButton(T("Auto"),IDC_RADIO_CAPTURE_MOUSE+2,mask); // 2
  ToolAddWindow(ToolTip,Win,T("Mouse state the same as on last stop,\
 mouse grabbed if you click in the window"));
  Win=CreateButton(T("Auto-release"),IDC_RADIO_CAPTURE_MOUSE+4,mask);
  ToolAddWindow(ToolTip,Win,T("Same but mouse is released on program run"));
  int current_option=(OPTION_CAPTURE_MOUSE&6) ? (OPTION_CAPTURE_MOUSE&6) : OPTION_CAPTURE_MOUSE;
  SendMessage(GetDlgItem(Handle,IDC_RADIO_CAPTURE_MOUSE+current_option),
    BM_SETCHECK,TRUE,0);

  NextLine();
  Offset+=LineStart;
  Win=CreateButton(T("VM-friendly"),IDC_VMMOUSE);
  SendMessage(Win,BM_SETCHECK,OPTION_VMMOUSE,0);
  ToolAddWindow(ToolTip,Win,T("Alternative mouse handling - cursor not bound to the window. \
Better for virtual machines"));

  // a long time ago probably...
  Win=CreateButton(T("Hide pointer on blit"),IDC_BLITHIDEM,WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX);
  SendMessage(Win,BM_SETCHECK,GetCSFInt("Options","BlitHideMouse",
    Disp.BlitHideMouse,globalINIFile),0);
  ToolAddWindow(ToolTip,Win,
    T("On some video cards, it makes a mess if the mouse pointer is over the area where the card is trying to draw.")+" "+
    T("This option, when checked, makes Steem hide the mouse before it draws to the screen.")+" "+
    T("Unfortunately this can make the mouse pointer flicker when Steem is running."));

  NextLine();
  Offset+=LineStart;
#if defined(SSE_GEM_CONTROL_PANEL)
  CreateStatic("Pointer");
  int w=32,d=10;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,
    (HMENU)(IDS_MOUSESLOW),hInstance,NULL);
  Offset+=w+d;
  Wid=page_w-Offset+page_l-w-LineStart-d;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,
    Offset,y,Wid,mSliderHeight,Handle,(HMENU)IDC_MOUSESPEED,hInstance,NULL);
  Offset+=Wid;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,
    (HMENU)(IDS_MOUSEFAST),hInstance,NULL);
#else
  CreateStatic(T("Speed"));
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ
    |TBS_TOOLTIPS,Offset,y,page_w-Offset+page_l-LineStart,mSliderHeight,Handle,
    (HMENU)IDC_MOUSESPEED,hInstance,NULL);
#endif
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(1,19));
  SendMessage(Win,TBM_SETPOS,1,mouse_speed);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETPAGESIZE,0,1);
  SendMessage(Win,TBM_SETTIC,0,10);

  y+=LineStart;

#endif//WIN32

#ifdef UNIX

  hxc_button *kg=new hxc_button(XD,page_p,page_l,y,page_w,85+35,NULL,this,
          BT_GROUPBOX,T("Keyboard"),0,hxc::col_bk);

  keyboard_language_dd.id=940;
  keyboard_language_dd.make_empty();
  keyboard_language_dd.additem(T("United States"),MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US));
  keyboard_language_dd.additem(T("United Kingdom"),MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_UK));
  keyboard_language_dd.additem(T("Australia (UK TOS)"),MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_AUS));
  keyboard_language_dd.additem(T("German"),MAKELANGID(LANG_GERMAN,SUBLANG_GERMAN));
  keyboard_language_dd.additem(T("French"),MAKELANGID(LANG_FRENCH,SUBLANG_FRENCH));
  keyboard_language_dd.additem(T("Spanish"),MAKELANGID(LANG_SPANISH,SUBLANG_SPANISH));
  keyboard_language_dd.additem(T("Italian"),MAKELANGID(LANG_ITALIAN,SUBLANG_ITALIAN));
  keyboard_language_dd.additem(T("Swedish"),MAKELANGID(LANG_SWEDISH,SUBLANG_SWEDISH));
  keyboard_language_dd.additem(T("Norwegian"),MAKELANGID(LANG_NORWEGIAN,SUBLANG_NEUTRAL));
  keyboard_language_dd.additem(T("Belgian (French TOS)"),MAKELANGID(LANG_FRENCH,SUBLANG_FRENCH_BELGIAN));
#if defined(SSE_IKBD_MAPPINGFILE)
  keyboard_language_dd.additem(T("Mapping File"),MAKELANGID(LANG_CUSTOM,SUBLANG_NEUTRAL));
#endif
  if (keyboard_language_dd.select_item_by_data(KeyboardLangID)<0){
    // if can't find the language
    keyboard_language_dd.sel=0;
    KeyboardLangID=MAKELANGID(LANG_ENGLISH,SUBLANG_ENGLISH_US);
  }
  keyboard_language_dd.grandfather=page_p;
  Wid=hxc::get_text_width(XD,T("Language"));
  keyboard_language_label.create(XD,kg->handle,10,20,Wid,25,NULL,this,
    BT_TEXT | BT_STATIC | BT_BORDER_NONE,T("Language"),0,BkCol);
  keyboard_language_dd.create(XD,kg->handle,15+Wid,20,page_w-20-(5+Wid),200,dd_notify_proc,this);

  keyboard_sc_but.set_check(bEnableShiftSwitching);
  keyboard_sc_but.create(XD,kg->handle,10,50,0,25,button_notify_proc,this,
    BT_CHECKBOX,T("Shift and alternate correction"),960,BkCol);
  hints.add(keyboard_sc_but.handle,T("When checked this allows Steem to emulate all keys correctly, it does this by changing the shift and alternate state of the ST when you press them.")+" "+
    T("This could interfere with games and other programs, only use it if you are doing lots of typing.")+" "+
    T("Please note that instead of pressing Alt-Gr or Control to access characters on the right-hand side of a key, you have to press Alt or Alt+Shift (this is how it was done on an ST)."),
    page_p);
  
#if defined(SSE_TOS_KEYBOARD_CLICK)
  keyboard_click_but.create(XD,kg->handle,10,80,0,25,
    button_notify_proc,this,BT_CHECKBOX,T("Keyboard click"),4007,BkCol);
  keyboard_click_but.set_check(OPTION_KEYBOARD_CLICK);
  hints.add(keyboard_click_but.handle,
    T("This uses address $484, changing before reset is safer - \
MUST be checked for some programs"),page_p);
  y+=LineHeight;
#endif
  y+=95;

  hxc_button *mg=new hxc_button(XD,page_p,page_l,y,page_w,85+35,NULL,this,
          BT_GROUPBOX,T("Mouse"),0,hxc::col_bk);

  Wid=hxc::get_text_width(XD,T("Capture mouse"));
  capture_mouse_label.create(XD,mg->handle,10,20,Wid,25,NULL,this,BT_STATIC 
    | BT_TEXT,T("Capture mouse"),0,BkCol);
  capture_mouse_dd.id=4027;
  capture_mouse_dd.make_empty();
  capture_mouse_dd.additem(T("Off"),0);
  capture_mouse_dd.additem(T("On"),1);
  capture_mouse_dd.additem(T("Auto"),2);
  capture_mouse_dd.additem(T("Auto-release"),4);
  int current_option=(OPTION_CAPTURE_MOUSE&6) ? (OPTION_CAPTURE_MOUSE&6) : OPTION_CAPTURE_MOUSE;
  capture_mouse_dd.select_item_by_data(current_option);
  capture_mouse_dd.create(XD,mg->handle,10+Wid+5,20,150,350,dd_notify_proc,this);
  y+=LineHeight;
  hints.add(capture_mouse_dd.handle,T("Off or On: use F11; Auto: remember state; Auto-release: release on PRG run"),page_p);

  vm_mouse_but.create(XD,mg->handle,10,50,0,25,
    button_notify_proc,this,BT_CHECKBOX,T("VM-friendly mouse"),4015,BkCol);
  vm_mouse_but.set_check(OPTION_VMMOUSE);
  hints.add(vm_mouse_but.handle,
    T("Alternative mouse handling. Better for virtual machines"),page_p);
  


  MouseSpeedLabel[0].create(XD,mg->handle,10,80,0,25,NULL,this,
                        BT_LABEL,T("Mouse speed"),0,BkCol);

  MouseSpeedSB.horizontal=true;
  MouseSpeedSB.init(19+4,4,mouse_speed-1);
  MouseSpeedSB.create(XD,mg->handle,15+MouseSpeedLabel[0].w,80,
                      mg->w-(10+15+MouseSpeedLabel[0].w),
                      25,scrollbar_notify_proc,this);

  y+=95;  
  y+=LineHeight;
  
#if defined(SSE_HD6301_LL) && !defined(SSE_NO_OLD_IKBD) 
  hd6301emu_but.create(XD,page_p,page_l,y,0,25,
    button_notify_proc,this,BT_CHECKBOX,T("Low-level 6301 emulation (C1)"),4006,BkCol);
  hd6301emu_but.set_check(OPTION_C1);
  hints.add(hd6301emu_but.handle,
  T("Chipset 1 - This enables a low level emulation of the IKBD keyboard chip (using\
 the Sim6xxx code by Arne Riiber, thx dude!), precise E-Clock, as well as ACIA\
 improvements or bugs"),
    page_p);
#endif

#if defined(SSE_IKBD_RTC)

  NextLine();
  Wid=hxc::get_text_width(XD,T("Battery"));
  keyboard_battery_label.create(XD,page_p,page_l,y,Wid,25,NULL,this,
    BT_TEXT | BT_STATIC | BT_BORDER_NONE,T("Battery"),0,BkCol);
  keyboard_battery_dd.id=4028;
  keyboard_battery_dd.make_empty();
  keyboard_battery_dd.additem(T("No"),0);
  keyboard_battery_dd.additem(T("Yes"),1);
  keyboard_battery_dd.additem(T("Yes, 2000-ready"),2);
  keyboard_battery_dd.sel=OPTION_BATTERY6301;
  keyboard_battery_dd.grandfather=page_p;
  keyboard_battery_dd.create(XD,page_p,15+Wid,y,page_w-20-(5+Wid),200,dd_notify_proc,this);

#endif  
#endif//UNIX
#if !defined(SSE_GUI_421_REORDER)
  NextLine();
#if defined(SSE_GEM_CONTROL_PANEL)
  NextLine();
#endif
  CreateRebootButton(T("If you change the low-level emulation setting,\
 you should reset the ST"));
#endif
}


#if !defined(SSE_GUI_421_REORDER)
void TOptionBox::CreateSTVideoPage() {

  int &y=mY,&Offset=mOffset,&Wid=mWid;

#ifdef WIN32
  int mask;
  HWND Win;
  CreateWindow("Button",T("ST Screen"),WS_CHILD|BS_GROUPBOX,
    page_l,y,page_w,GROUP_HEIGHT(4),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
  y+=mGroupTitleHeight;
  Offset+=LineStart;
  mask=WS_CHILD|BS_AUTORADIOBUTTON;
  Win=CreateButton(T(screen_type[1])+" SC1224",IDC_RADIO_STSCREEN,mask|WS_GROUP);
  ToolAddWindow(ToolTip,Win,T("Low/Med Resolution, 50/60Hz"));
  Win=CreateButton(T(screen_type[0])+" SM124",IDC_RADIO_STSCREEN+1,mask);
  ToolAddWindow(ToolTip,Win,T("High Resolution, 72Hz"));
  int monitor_sel=NewMonitorSel;
  if(monitor_sel<0) 
    monitor_sel=GetCurrentMonitorSel();
#ifndef NO_CRAZY_MONITOR
  ALL_HACKS_BEGIN
  Win=CreateButton(T("Extended"),IDC_RADIO_STSCREEN+2,mask);
  ToolAddWindow(ToolTip,Win,T("TOS 1.04 or beyond"));
  NextLine();
  Offset+=LineStart;
  CreateStatic(T("Extended monitor"));
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,Offset,y,
                   page_w-Offset+page_l-LineStart,200,Handle,(HMENU)IDC_EXTENDED_MONITOR,hInstance,NULL);

  for(int n=0;n<EXTMON_RESOLUTIONS;n++)
  {
    char sres[64];
    sprintf(sres,"-> %u x %u x %u",+extmon_res[n][0],extmon_res[n][1],
      extmon_res[n][2]);
    CBAddString(Win,sres);
  }
  ALL_SETTINGS_END
#endif

  int rbut=monitor_sel;
  if(rbut>1)
    rbut=2;
  SendMessage(GetDlgItem(Handle,IDC_RADIO_STSCREEN+rbut),BM_SETCHECK,TRUE,0);

  NextLine();

  Offset+=LineStart;
  CreateStatic(T("Borders"));
  mask=WS_CHILD|BS_AUTORADIOBUTTON;
  Wid=GetCheckBoxSize(Font,T("Off")).Width;
  Win=CreateButton(T("Off"),IDC_RADIO_BORDER,mask|WS_GROUP);
#if !defined(SSE_VID_BORDERS)
  Win=CreateButton(T("On"),IDC_RADIO_BORDER+1,mask|WS_GROUP);
#else
  ToolAddWindow(ToolTip,Win,T("Fine for most apps and games"));
  Win=CreateButton(T("Normal"),IDC_RADIO_BORDER+1,mask);
  ToolAddWindow(ToolTip,Win,T("Typical ST monitor"));
  Win=CreateButton(T("Large"),IDC_RADIO_BORDER+2,mask);
  Win=CreateButton(T("Max"),IDC_RADIO_BORDER+3,mask);
#endif

  NextLine();
  Offset+=LineStart;
  Win=CreateButton(T("ST aspect ratio"),IDC_STASPECTRATIO);
  SendMessage(Win,BM_SETCHECK,OPTION_ST_ASPECT_RATIO,0);
  ToolAddWindow(ToolTip,Win,
    T("Reproduces the familiar vertical distortion on standard colour screens"));

  Win=CreateButton(T("Scanlines"),IDC_SCANLINES);
  SendMessage(Win,BM_SETCHECK,OPTION_SCANLINES,0);
  ToolAddWindow(ToolTip,Win,T("Reproduces scanlines of cathodic screens"));

#if defined(SSE_VID_SINGLEPIX)
  Win=CreateButton(T("Pixels"),IDC_SINGLEPIXELS);
  SendMessage(Win,BM_SETCHECK,SSEOptions.SinglePixels,0);
  ToolAddWindow(ToolTip,Win,T("Same as scanlines but vertical"));
#endif

#if defined(SSE_VID_D3D_SWEETFX)
  mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX|(Disp.hD3Dhack ? 0 : WS_DISABLED);
#if defined(SSE_VID_SINGLEPIX) // no room
  Win=CreateButton(T("CRT"),IDC_CTR_EMU,mask);
#else
  Win=CreateButton(T("CRT emu"),IDC_CTR_EMU,mask);
#endif
  SendMessage(Win,BM_SETCHECK,OPTION_CRT_EMU,0);
  ToolAddWindow(ToolTip,Win,T("Cathode Ray Tube screen emulation using SweetFX by CeeJay.dk"));
#endif
  NextLine();

  ALL_HACKS_BEGIN
  ALL_SETTINGS_ELSE
  NextLine();
  ALL_SETTINGS_END

  CreateWindow("Button",T("Overscan emulation"),WS_CHILD|BS_GROUPBOX,
    page_l,y,page_w,GROUP_HEIGHT(2),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
  y+=mGroupTitleHeight;
  
  mask=WS_CHILD|BS_AUTORADIOBUTTON;

#if defined(SSE_HARDWARE_OVERSCAN)

  ALL_SETTINGS_BEGIN  
  Offset+=LineStart;
  CreateStatic(T("Hardware"));
  EasyStr hint=T("Those devices trick the DE signal to get permanent overscan.\
 Need drivers on the ST side. TOS 1.04 or beyond");
  Win=CreateButton(T(overscan_dev[0]),IDC_RADIO_HWOVERSCAN,mask|WS_GROUP);
  ToolAddWindow(ToolTip,Win,hint);
  Win=CreateButton(T(overscan_dev[1]),IDC_RADIO_HWOVERSCAN+1,mask);
  ToolAddWindow(ToolTip,Win,hint);
  Win=CreateButton(T(overscan_dev[2]),IDC_RADIO_HWOVERSCAN+2,mask);
  ToolAddWindow(ToolTip,Win,hint);
  
  NextLine();
  ALL_SETTINGS_END

#endif

  Offset+=LineStart;
  CreateStatic(T("Software"));
  Win=CreateButton(T("None"),(IDC_RADIO_SWOVERSCAN+0),mask|WS_GROUP);
  ToolAddWindow(ToolTip,Win,
    T("This actually is the correct choice for most applications and games"));

  Win=CreateButton(T("High-level (C2)"),(IDC_RADIO_SWOVERSCAN+1),mask);
  ToolAddWindow(ToolTip,Win,T("This should run almost all known demos"));

  Win=CreateButton(T("Low-level (C3)"),(IDC_RADIO_SWOVERSCAN+2),mask);
  ToolAddWindow(ToolTip,Win,T("It uses more CPU for, hopefully, maximum accuracy"));

  NextLine();

  ALL_SETTINGS_BEGIN

  CreateWindow("Button",T("Wakeup"),WS_CHILD|BS_GROUPBOX,
    page_l,y,page_w,GROUP_HEIGHT(2),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
  y+=mGroupTitleHeight;
  Offset+=LineStart;

  CreateStatic(T("GLU"));
  Wid=CbUnits*2;
  HWND hEdit0=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,Wid,CharHeight,Handle,(HMENU)IDC_GLU_WAKEUP0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS
    |UDS_SETBUDDYINT|UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_GLU_WAKEUP,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit0,0);
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(4,1));
  Offset+=Wid+HorizontalSeparation;

  CreateStatic(T("Shifter"));
  Wid=CbUnits*2;
  HWND hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,Wid,CharHeight,Handle,(HMENU)IDC_SHIFTER_WU0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_SHIFTER_WU,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(SHIFTER_MAX_WU_SHIFT,-SHIFTER_MAX_WU_SHIFT));
  
  NextLine();
  Offset+=LineStart;
  Win=CreateButton(T("Random on boot"),IDC_RANDOM_WU);
  SendMessage(Win,BM_SETCHECK,OPTION_RANDOM_WU,0);
  ToolAddWindow(ToolTip,Win,T("This affects both the GLUE and the Shifter"));
  Win=CreateButton(T("Shifter effects"),IDC_UNSTABLE_SHIFTER);
  SendMessage(Win,BM_SETCHECK,OPTION_UNSTABLE_SHIFTER,0);
  ToolAddWindow(ToolTip,Win,T("Used for some aspects of software overscan emulation"));

  ALL_SETTINGS_END

#if defined(SSE_OPTION_FREQ)
  
  HACK_BEGIN
  
  NextLine();
  CreateWindow("Button",T("Hacks"),WS_CHILD|BS_GROUPBOX,
    page_l,y,page_w,GROUP_HEIGHT(1),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
  y+=mGroupTitleHeight;
  Offset+=LineStart;
  CreateStatic(T("Frequency"));
  Wid=CbUnits*3;
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|CBS_DROPDOWNLIST,
    Offset,y,Wid,200,Handle,(HMENU)IDC_VID_FREQUENCY,hInstance,NULL);
  CBAddString(Win,T("50Hz"),MAKELONG(PAL_HZ,0));
  CBAddString(Win,T("60Hz"),MAKELONG(NTSC_HZ,0));
  CBAddString(Win,T("72Hz"),MAKELONG(MONO_HZ,0));

#if defined(SSE_OPTION_FASTBLITTER)
  Offset+=Wid+HorizontalSeparation;
  Win=CreateButton("Fast Blit",IDC_FASTBLITTER);
  SendMessage(Win,BM_SETCHECK,OPTION_FASTBLITTER,0);
#if defined(SSE_OPTION_FASTLINEA)
  ToolAddWindow(ToolTip,Win,T("Free cycles for blitter or line A use"));
#else
  ToolAddWindow(ToolTip,Win,T("Free cycles for blitter use"));
#endif
#endif

  HACK_END

#endif//#if defined(SSE_OPTION_FREQ)

  y+=LineStart;
  UpdateSTVideoPage();

#endif//WIN32

#ifdef UNIX
  monitor_label.create(XD,page_p,page_l,y,0,25,NULL,this,BT_LABEL,
                          T("Monitor"),0,BkCol);
  monitor_dd.id=920;
  monitor_dd.make_empty();
  monitor_dd.additem(T("Colour")+" ("+T("Low/Med Resolution")+")");
  monitor_dd.additem(T("Monochrome")+" ("+T("High Resolution")+")");
#ifndef NO_CRAZY_MONITOR
  for(int n=0;n<EXTMON_RESOLUTIONS;n++){
    monitor_dd.additem(T("Extended Monitor At")+" "+extmon_res[n][0]+"x"
      +extmon_res[n][1]+"x"+extmon_res[n][2]);
  }
#endif

  int monitor_sel=NewMonitorSel;
  if(monitor_sel<0) 
    monitor_sel=GetCurrentMonitorSel();
  monitor_dd.sel=monitor_sel;
    
  monitor_dd.create(XD,page_p,page_l+5+monitor_label.w,y,page_w-(5+monitor_label.w),200,dd_notify_proc,this);
  y+=LineHeight;

#if defined(SSE_VID_BORDERS)
  Wid=hxc::get_text_width(XD,T("Border"));
  border_size_label.create(XD,page_p,page_l,y,Wid,25,NULL,this,BT_STATIC 
    | BT_TEXT,T("Border"),0,BkCol);
  border_size_dd.id=4001;
  border_size_dd.make_empty();
  border_size_dd.additem("Off",0);
  border_size_dd.additem("Normal",1);
  border_size_dd.additem("Large",2);
  border_size_dd.additem("Max",3);
  border_size_dd.select_item_by_data(border);
  border_size_dd.create(XD,page_p,page_l+5+Wid,y,400-(15+Wid+10),350,
    dd_notify_proc,this);
  y+=LineHeight;
#endif

  Wid=hxc::get_text_width(XD,T("Wake-up"));
  wake_up_label.create(XD,page_p,page_l,y,Wid,25,NULL,this,BT_STATIC 
    | BT_TEXT,T("Wake-up"),0,BkCol);
  wake_up_dd.id=4017;
  wake_up_dd.make_empty();
  //wake_up_dd.additem("Ignore",0);//?
  //wake_up_dd.additem("Random",0);//?
  wake_up_dd.additem("DL3 WU2 WS2",1);
  wake_up_dd.additem("DL4 WU2 WS4",2);
  wake_up_dd.additem("DL5 WU1 WS3",3);
  wake_up_dd.additem("DL6 WU1 WS1",4);
  wake_up_dd.select_item_by_data(OPTION_WS);
  wake_up_dd.create(XD,page_p,page_l+Wid+5,y,120,350,dd_notify_proc,this);
    
  Wid=hxc::get_text_width(XD,T("Shifter"));
  shifter_wu_label.create(XD,page_p,page_l+190,y,Wid,25,NULL,this,BT_STATIC 
    | BT_TEXT,T("Shifter"),0,BkCol);
  shifter_wu_dd.id=4026;
  shifter_wu_dd.make_empty();
  shifter_wu_dd.additem("-2",-2);
  shifter_wu_dd.additem("-1",-1);
  shifter_wu_dd.additem("0",0);
  shifter_wu_dd.additem("1",1);
  shifter_wu_dd.additem("2",2);
  shifter_wu_dd.select_item_by_data(OPTION_SHIFTER_WU);
  shifter_wu_dd.create(XD,page_p,page_l+190+5+Wid,y,50,350,dd_notify_proc,this);
    
  randomWU_but.create(XD,page_p,page_l+310,y,0,25,
    button_notify_proc,this,BT_CHECKBOX,T("Random"),IDC_RANDOM_WU,BkCol);
  randomWU_but.set_check(OPTION_RANDOM_WU);
  y+=LineHeight;

#if defined(SSE_HARDWARE_OVERSCAN)
  Wid=hxc::get_text_width(XD,T("Hardware overscan"));
  hw_overscan_label.create(XD,page_p,page_l,y,Wid,25,NULL,this,BT_STATIC 
    | BT_TEXT,T("Hardware overscan"),0,BkCol);
  hw_overscan_dd.id=4016;
  hw_overscan_dd.make_empty();
  hw_overscan_dd.additem("None",0);
  hw_overscan_dd.additem("LaceScan",1);
  hw_overscan_dd.additem("AutoSwitch",2);
  hw_overscan_dd.select_item_by_data(OPTION_HWOVERSCAN);
  hw_overscan_dd.create(XD,page_p,page_l+5+Wid,y,150,350,
    dd_notify_proc,this);
  y+=LineHeight;
  hints.add(hw_overscan_dd.handle,T("Works only on STF/Mega ST, with drivers"),page_p);
#endif

  optionC2_but.create(XD,page_p,page_l,y,0,25,
    button_notify_proc,this,BT_CHECKBOX,T("Software overscan"),4014,BkCol);
  optionC2_but.set_check(OPTION_VLE);
  hints.add(optionC2_but.handle,
  T("Check for a high level emulation of Shifter tricks"),page_p);
  y+=LineHeight;

  hxc_button *p_but=new hxc_button(XD,page_p,page_l,y,0,25,button_notify_proc,this,
    BT_CHECKBOX,T("Scanlines"),210,BkCol);
  p_but->set_check(OPTION_SCANLINES);
  hints.add(p_but->handle,T("Reproduces scanlines of cathodic screens"),page_p);

#if defined(SSE_VID_SINGLEPIX)
  hxc_button *p_but_pix=new hxc_button(XD,page_p,page_l+100,y,0,25,button_notify_proc,this,
    BT_CHECKBOX,T("Pixels"),211,BkCol);
  p_but_pix->set_check(SSEOptions.SinglePixels);
  hints.add(p_but_pix->handle,T("Same as scanlines but vertical"),page_p);
#endif
 
  y+=LineHeight;
  optionBW_but.create(XD,page_p,page_l,y,0,25,
    button_notify_proc,this,BT_CHECKBOX,T("B/W"),4022,BkCol);
  optionBW_but.set_check(OPTION_GREYSCREEN);
  hints.add(optionBW_but.handle,T("Black and white TV emulation"),page_p);

  optionGreen_but.create(XD,page_p,page_l+100,y,0,25,
    button_notify_proc,this,BT_CHECKBOX,T("Green"),4025,BkCol);
  optionGreen_but.set_check(OPTION_GREENSCREEN);
  hints.add(optionGreen_but.handle,T("Emulation of the Philips CM8833 Green button"),page_p);

  y+=LineHeight;
  vivid_but.create(XD,page_p,page_l,y,0,25,
    //button_notify_proc,this,BT_CHECKBOX,T("FullSpectrumPal colours"),4023,BkCol);
    button_notify_proc,this,BT_CHECKBOX,T("Full spectrum"),4023,BkCol);
  vivid_but.set_check(SSEOptions.FullSpectrumPal);
#endif//UNIX
  NextLine();
  CreateRebootButton("");
}


void TOptionBox::UpdateSTVideoPage() {
#ifdef WIN32  
  HWND Win=GetDlgItem(Handle,IDC_RADIO_STSCREEN);
  if(Win==NULL)
    return;
  int monitor_sel=NewMonitorSel; // 0 = colour
  if(monitor_sel<0) 
    monitor_sel=GetCurrentMonitorSel();
  BOOL c3ok;
  if(monitor_sel>1)
  {
    EnableControl(IDC_EXTENDED_MONITOR,TRUE);
    Win=GetDlgItem(Handle,IDC_EXTENDED_MONITOR);
    SendMessage(Win,CB_SETCURSEL,monitor_sel-2,0);
    EnableControl(IDC_DESKTOPHZ,TRUE);
    EnableControl(IDC_SCANLINES,FALSE);
    c3ok=FALSE;
  }
  else
  {
    EnableControl(IDC_EXTENDED_MONITOR,FALSE);
    EnableControl(IDC_DESKTOPHZ,FALSE);
    EnableControl(IDC_SCANLINES,TRUE);
    c3ok=(SSEConfig.Stvl!=0);
  }
  for(int i=0;i<=BIGGEST_BORDER;i++)
  {
    EnableControl(IDC_RADIO_BORDER+i,(FullScreen==0)&&(i<2||!monitor_sel)&&(i<1||monitor_sel<2));
    SendMessage(GetDlgItem(Handle,IDC_RADIO_BORDER+i),BM_SETCHECK,(i==border),0);
  }
  EnableControl(IDC_STASPECTRATIO,(monitor_sel==0));
#if !defined(SSE_VID_SIZE4)
  EnableControl(IDC_SCANLINES,(monitor_sel==0));
#endif
  EnableControl((IDC_RADIO_SWOVERSCAN+2),c3ok);
  Win=GetDlgItem(Handle,IDC_VID_FREQUENCY);
  CBSelectItemWithData(Win,Glue.PreviousVideoFreq);
  Win=GetDlgItem(Handle,IDC_SHIFTER_WU);
  if(Win)
  {
    SendMessageW(Win,UDM_SETPOS32,0,OPTION_SHIFTER_WU);
    Win=GetDlgItem(Handle,IDC_UNSTABLE_SHIFTER);
    SendMessage(Win,BM_SETCHECK,OPTION_UNSTABLE_SHIFTER,0);
    Win=GetDlgItem(Handle,IDC_GLU_WAKEUP0);
    EnableWindow(Win,IS_STF);
    Win=GetDlgItem(Handle,IDC_GLU_WAKEUP);
    SendMessageW(Win,UDM_SETPOS32,0,Mmu.WS[OPTION_WS]); // triggers option!
    Win=GetDlgItem(Handle,IDC_RADIO_HWOVERSCAN+1); // also "advanced"
    EnableWindow(Win,IS_STF);
    Win=GetDlgItem(Handle,IDC_RADIO_HWOVERSCAN+2);
    EnableWindow(Win,IS_STF);
  }
  SendMessage(GetDlgItem(Handle,IDC_RADIO_HWOVERSCAN+OPTION_HWOVERSCAN),BM_SETCHECK,TRUE,0);
  SendMessage(GetDlgItem(Handle,IDC_RADIO_SWOVERSCAN+OPTION_VLE),BM_SETCHECK,TRUE,0);

#if 0 && defined(SSE_OPTION_FASTBLITTER)
  Win=GetDlgItem(Handle,IDC_FASTBLITTER);
  if(Win)
    EnableWindow(Win,SSEConfig.Blitter); // enabled if there's a blitter
#endif
#endif//WIN32

}
#endif//#if !defined(SSE_GUI_421_REORDER)

#if defined(SSE_GEM_CONTROL_PANEL)

void TOptionBox::CreateGEMControlPanel() {

  int &y=mY,/*&Wid=mWid,*/&Offset=mOffset;

#ifdef WIN32
  HWND Win;
  const BYTE w=32;
  const short w2=w+HorizontalSeparation; // size of ST icons
  const short SliderSize=w+w2+w2;//*3;//GUIMUL(120);

  ASSERTB(CurrentColour<PAL_SIZE); // if we make CurrentColour a 4bit integer, there's more overhead
  WORD dat=STpal[CurrentColour];
  dat=(WORD)(((dat&0x888)>>3)|((dat&0x777)<<1));  //fix up stupid rRRRgGGGbBBB colour pattern
  int max_v=0xF>>(IS_STF); // 8*8*8=512, 16*16*16=4096
  for(int i=0;i<3;i++) // // 3 sliders R,G,B to edit the colour
  {
    CreateWindow("Static","",WS_CHILD,page_l+i*w2,
      y,w,CharHeight,Handle,(HMENU)(INT_PTR)(IDS_RGB+i),hInstance,NULL);
    // TBS_DOWNISLEFT and TBS_REVERSED don't work! 
    Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_TABSTOP|TBS_VERT,
      page_l+i*w2,y+CharHeight,w,SliderSize,Handle,(HMENU)(INT_PTR)(IDC_RGB+i),hInstance,NULL);
    SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,max_v));
    SendMessage(Win,TBM_SETPOS,1,1); // go around Windows "optimisation" (if 0)
    WORD nib=(dat>>(2-i)*4)&0xF;
    if(max_v==7)
      nib>>=1;
    SendMessage(Win,TBM_SETPOS,1,max_v-nib);
    SendMessage(Win,TBM_SETLINESIZE,0,1);
    SendMessage(Win,TBM_SETPAGESIZE,0,3);
    //SendMessage(Win,TBM_SETTIC,0,0);
    SendMessage(Handle,WM_VSCROLL,0,(LPARAM)Win); // force update
  }

  // palette of 16 colours, player can select one
  y+=CharHeight+SliderSize;
  for(int pal=0;pal<PAL_SIZE;pal++)
  {
    int x=page_l+w2*(pal/2);
    int y2=y;
    if(pal&1)
      y2+=CharHeight+LineStart;
    Win=CreateWindow("Button","",WS_CHILD|WS_TABSTOP|BS_PUSHBUTTON|BS_OWNERDRAW,
      x,y2,w,CharHeight,Handle,(HMENU)(INT_PTR)(IDC_COLOUR+pal),hInstance,NULL);
  }
  int savey=y;

  y=LineStart;
  Offset+=3*w2;
  int x=Offset;

  // date time TODO?

  // keyboard delay, rate
#if defined(SSE_420R5)
  BYTE conterm=SafePeek(0x484);
#else
  BYTE conterm=PEEK(0x484);
#endif
  EasyStr tip=T("Change only with TOS-compliant programs");

  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,x,y,w,w,Handle,
    (HMENU)(IDS_KEYDELAYLOW),hInstance,NULL);
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,
      x+w2,y,SliderSize,CharHeight,Handle,(HMENU)(IDC_REPEAT_DELAY),hInstance,NULL);
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(1,0x2E));
  SendMessage(Win,TBM_SETPOS,1,PEEK(TosKeyRepeat));
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETPAGESIZE,0,3);
  ToolAddWindow(ToolTip,Win,T("Delay ")+tip);
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,x+w2+SliderSize+HorizontalSeparation,y,w,w,Handle,
    (HMENU)(IDS_KEYDELAYHI),hInstance,NULL);

  y+=w2;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,x,y,w,w,Handle,
    (HMENU)(IDS_KEYRATEHI),hInstance,NULL);
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,
      x+w2,y,SliderSize,CharHeight,Handle,(HMENU)(IDC_REPEAT_RATE),hInstance,NULL);
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(1,0x15));
  SendMessage(Win,TBM_SETPOS,1,PEEK(TosKeyRepeat+1));
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETPAGESIZE,0,3);
  ToolAddWindow(ToolTip,Win,T("Rate ")+tip);
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,x+w2+SliderSize+HorizontalSeparation,y,w,w,Handle,
    (HMENU)(IDS_KEYRATELOW),hInstance,NULL);

  // mouse click speed TODO? it's an internal AES variable set by evnt_dclick()

  // keyboard bell, click - those won't be updated live (performance) but on stop
  y+=w2;
  
  Win=CreateWindow("Button","",WS_CHILD|WS_TABSTOP|BS_OWNERDRAW,
    x,y,w,w,Handle,(HMENU)(IDC_BELL),hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Alarm bell ")+tip);
  SendMessage(Win,BM_SETCHECK,(conterm&BIT_2) ? BST_CHECKED : BST_UNCHECKED,0);

  Win=CreateWindow("Button","",WS_CHILD|WS_TABSTOP|BS_OWNERDRAW,
    x+w2,y,w,w,Handle,(HMENU)(IDC_CLICK),hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Click ")+tip);
  SendMessage(Win,BM_SETCHECK,(conterm&BIT_0) ? BST_CHECKED : BST_UNCHECKED,0);
  
  // Repeat on/off wasn't on the ST control panel
  Win=CreateWindow("Button","",WS_CHILD|WS_TABSTOP|BS_OWNERDRAW,
    x+w2*2,y,w,w,Handle,(HMENU)(IDC_REPEAT),hInstance,NULL);
  ToolAddWindow(ToolTip,Win,T("Repeat ")+tip);
  SendMessage(Win,BM_SETCHECK,(conterm&BIT_0) ? BST_CHECKED : BST_UNCHECKED,0);

  y=savey+w2*2;
  Offset=page_l;//+LineStart;

#if defined(SSE_GUI_421_REORDER)
  DWORD mask=WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX;
  Win=CreateButton(T("Block palette"),IDC_BLOCKPAL,mask);
  SendMessage(Win,BM_SETCHECK,(SSEOptions.BlockPal) ? BST_CHECKED : BST_UNCHECKED,0);
  ToolAddWindow(ToolTip,Win,T("Hack"));
#if defined(SSE_IKBD_RTC)
  if(OPTION_HACKS)
  {
    Win=CreateButton(T("Clock always correct"),IDC_RTCHACK);
    SendMessage(Win,BM_SETCHECK,OPTION_RTC_HACK,0);
    ToolAddWindow(ToolTip,Win,T("Handy hack voiding any RTC emulation and its\
 problems, but can interfere with some programs"));
  }
#endif
  Win=CreateButton(T("Emu detect"),IDC_EMUDETECT);
  SendMessage(Win,BM_SETCHECK,OPTION_EMU_DETECT,0);
  ToolAddWindow(ToolTip,Win,T("Enable communication between Steem and ST programs"));
  NextLine();
#endif

#if defined(SSE_MEGA16)
 #if defined(SSE_420R5) // forgot setcheck
  if(SSEConfig.Mega)
  {
    Win=CreateButton(T("Mega Turbo"),IDC_TURBO16MHZ);
    SendMessage(Win,BM_SETCHECK,((Cpu16.ScuReg&3)==3) ? BST_CHECKED : BST_UNCHECKED,0);
  }
#else
  if(SSEConfig.Mega)
    Win=CreateButton(T("Turbo"),IDC_TURBO16MHZ);
  NextLine();
#endif
#endif

#if defined(SSE_GUI_421_REORDER)
  if(IS_STE)
  {
    CreateWindow("Button",T("MicroWire"),WS_CHILD|BS_GROUPBOX,
                 page_l,y,page_w,GROUP_HEIGHT(3),Handle,(HMENU)IDC_GROUPBOX,hInstance,NULL);
    y+=mGroupTitleHeight;
    Offset+=LineStart;
    int start=Offset,tab=GUIMUL(80),w3=GUIMUL(50);
    CreateStatic(T("Volume"));
    Offset=start+tab;
    CreateStatic(T("Balance"));
    Offset=start+2*tab;
    CreateStatic(T("Bass"));
    Offset=start+3*tab;
    CreateStatic(T("Treble"));    
    NextLine();
    Offset=start;
    Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,
      Offset,y,w3,mSliderHeight,Handle,(HMENU)IDC_MWVOLUME,hInstance,NULL);
    SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,0x28));
    SendMessage(Win,TBM_SETPOS,1,Microwire.volume);
    SendMessage(Win,TBM_SETLINESIZE,0,1);
    SendMessage(Win,TBM_SETPAGESIZE,0,1);
    SendMessage(Win,TBM_SETTIC,0,0);    
    Offset=start+tab;
    Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,
      Offset,y,w3,mSliderHeight,Handle,(HMENU)IDC_MWBALANCE,hInstance,NULL);
    SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(-0x14,0x14));
    SendMessage(Win,TBM_SETPOS,1,2); // doesn't work with 1
    SendMessage(Win,TBM_SETPOS,1,Microwire.volume_r-Microwire.volume_l);
    SendMessage(Win,TBM_SETLINESIZE,0,1);
    SendMessage(Win,TBM_SETPAGESIZE,0,1);
    SendMessage(Win,TBM_SETTIC,0,0);
    Offset=start+2*tab;
    Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,
      Offset,y,w3,mSliderHeight,Handle,(HMENU)IDC_BASS,hInstance,NULL);
    SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,12));
    SendMessage(Win,TBM_SETPOS,1,Microwire.bass);
    SendMessage(Win,TBM_SETLINESIZE,0,1);
    SendMessage(Win,TBM_SETPAGESIZE,0,1);
    SendMessage(Win,TBM_SETTIC,0,0);
    Offset=start+3*tab;
    Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,
      Offset,y,w3,mSliderHeight,Handle,(HMENU)IDC_TREBLE,hInstance,NULL);
    SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,12));
    SendMessage(Win,TBM_SETPOS,1,Microwire.treble);
    SendMessage(Win,TBM_SETLINESIZE,0,1);
    SendMessage(Win,TBM_SETPAGESIZE,0,1);
    SendMessage(Win,TBM_SETTIC,0,0);
#if defined(SSE_SOUND_MICROWIRE_OPTION)
    NextLine();
    Offset+=HorizontalSeparation;
    Win=CreateButton("Bypass",IDC_MICROWIRE,WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX);
    SendMessage(Win,BM_SETCHECK,!SSEOptions.Microwire,0);
#endif
  }

#else//#if defined(SSE_GUI_421_REORDER)

  // Microwire
  if(IS_STE)
  {
    CreateStatic("Bass");
    Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,
      Offset,y,GUIMUL(50),mSliderHeight,Handle,(HMENU)IDC_BASS,hInstance,NULL);
    SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,12));
    SendMessage(Win,TBM_SETPOS,1,Microwire.bass);
    SendMessage(Win,TBM_SETLINESIZE,0,1);
    SendMessage(Win,TBM_SETPAGESIZE,0,1);
    SendMessage(Win,TBM_SETTIC,0,0);
    Offset+=GUIMUL(50)+HorizontalSeparation;
    CreateStatic("Treble");
    Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,
      Offset,y,GUIMUL(50),mSliderHeight,Handle,(HMENU)IDC_TREBLE,hInstance,NULL);
    SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,12));
    SendMessage(Win,TBM_SETPOS,1,Microwire.treble);
    SendMessage(Win,TBM_SETLINESIZE,0,1);
    SendMessage(Win,TBM_SETPAGESIZE,0,1);
    SendMessage(Win,TBM_SETTIC,0,0);
  }

#endif//#if defined(SSE_GUI_421_REORDER)

  // cancel TODO?
#endif//WIN32
}

#endif//#if defined(SSE_GEM_CONTROL_PANEL)


#if defined(SSE_GUI_EMUCONTROL)

void TOptionBox::CreateParameters1() {
  
#ifdef WIN32

#if defined(SSE_GUI_421_REORDER)
  int &y=mY,&Offset=mOffset;
  DWORD mask=WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX;
  HWND hGroup,Win,hEdit;
  LPARAM lpar_range=MAKELPARAM(32*TICKS8,-32*TICKS8);
  short const spinw=GuiSM.mCbUnits*2;
  const WORD GroupMfpH=GROUP_HEIGHT(8)-GUIMUL(20)/*GUIMUL(235)*/;

#if defined(SSE_SHIFTER_421B)
  const WORD GroupVideoH=GROUP_HEIGHT(5) /*GUIMUL(150)*/;

  // group Video
  hGroup=CreateWindow("Button","Video",WS_CHILD|BS_GROUPBOX,page_l,y,page_w,
    GroupVideoH,Handle,(HMENU)IDS_STATIC,hInstance,NULL);
  mask=WS_CHILD|BS_AUTORADIOBUTTON|WS_TABSTOP;
  y+=GUIMUL(20);
  Offset+=HorizontalSeparation;
  
  // Software Overscan
  CreateStatic(T("Overscan"));
  Win=CreateButton(T("None"),(IDC_RADIO_SWOVERSCAN+0),mask|WS_GROUP);
  ToolAddWindow(ToolTip,Win,
    T("This actually is the correct choice for most applications and games"));
  Win=CreateButton(T("High-level (C2)"),(IDC_RADIO_SWOVERSCAN+1),mask);
  ToolAddWindow(ToolTip,Win,T("This should run almost all known demos"));
  Win=CreateButton(T("Low-level (C3)"),(IDC_RADIO_SWOVERSCAN+2),mask|ENABLE_CHECK((SSEConfig.Stvl!=0)));
  ToolAddWindow(ToolTip,Win,T("It uses more CPU for, hopefully, maximum accuracy"));
  CheckRadioButton(Handle,IDC_RADIO_SWOVERSCAN,IDC_RADIO_SWOVERSCAN+2,
                   IDC_RADIO_SWOVERSCAN+OPTION_VLE);

  NextLine();
  Offset+=HorizontalSeparation;
  mask=WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX;
  Win=CreateButton(T("Tracking VC"),IDC_TRACKINGVIDEOCOUNTER,mask);
  SendMessage(Win,BM_SETCHECK,(SSEOptions.TrackVC) ? BST_CHECKED : BST_UNCHECKED,0);
  ToolAddWindow(ToolTip,Win,T("Video counter"));
  Offset+=HorizontalSeparation;
  Win=CreateButton(T("Unstable Shifter"),IDC_UNSTABLE_SHIFTER);
  SendMessage(Win,BM_SETCHECK,OPTION_UNSTABLE_SHIFTER,0);
  ToolAddWindow(ToolTip,Win,T("Used for some aspects of software overscan emulation"));

  // 7 spinners for delay rendering, one for each "dispatcher"
  char *dispatcher_name[7]={"CPU","RES","VC","PAL","END","LW","SCR"};
  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic("Rendering delay");
  for(BYTE i=0;i<7;i++)
  {
    if(i==3)
    {
      NextLine();
      Offset+=HorizontalSeparation;
    }
    CreateStatic(dispatcher_name[i]);
    Offset+=HorizontalSeparation;
    hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,Offset,y,spinw,CharHeight,
                       Handle,(HMENU)(INT_PTR)(IDC_VID_DELAY+i),hInstance,NULL);
    Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT|UDS_ALIGNRIGHT,
                     0,0,0,0,Handle,(HMENU)(INT_PTR)(IDC_VID_DELAY+i+7),hInstance,NULL);
    ToolAddWindow(ToolTip,hEdit,T("High-level rendering is quirky/hacky\n8MHz"));
    SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
    SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(64,-64));
    SendMessageW(Win,UDM_SETPOS32,0,Shifter.RenderingDelay[Shifter.DISPATCHER_CPU+i]);
    Offset+=spinw+HorizontalSeparation;
  }//nxt i
  y=GroupVideoH+LineStart;
#else

  // group MC68901 (MFP) - if we had a better emulation, we wouldn't have all those parameters!
  y=GroupCpuH+LineStart;
#endif
  hGroup=CreateWindow("Button","MC68901",WS_CHILD|BS_GROUPBOX,page_l,y,page_w,
    GroupMfpH,Handle,(HMENU)IDS_STATIC,hInstance,NULL);
  y+=GUIMUL(20);
  Offset=page_l+HorizontalSeparation;
  CreateStatic(T("Timer start"));
  Offset+=GUIMUL(40);
  LONG align=Offset;
#ifdef SSE_X64
  char *sCpuClock="32MHz",*sMfpClock="2.457MHz";
#else
  char *sCpuClock="8MHz",*sMfpClock="2.457MHz";
#endif
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPSTARTCPU0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPSTARTCPU1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpStartCpu);
  ToolAddWindow(ToolTip,hEdit,sCpuClock);
  
  Offset+=spinw+HorizontalSeparation;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPSTARTTCLK0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPSTARTTCLK1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpStartTclk);
  ToolAddWindow(ToolTip,hEdit,sMfpClock);

  Offset+=spinw+HorizontalSeparation;
  Win=CreateButton(T("Sync"),IDC_MFPSTARTSYNC,mask);
  SendMessage(Win,BM_SETCHECK,SSEOptions.MfpStartSync ? BST_CHECKED : BST_UNCHECKED,0);

  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Timer stop"));
  Offset=align;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPSTOPCPU0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS
    |UDS_SETBUDDYINT|UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPSTOPCPU1,
    hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpStopCpu);
  ToolAddWindow(ToolTip,hEdit,sCpuClock);
  
  Offset+=spinw+HorizontalSeparation;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPSTOPTCLK0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPSTOPTCLK1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpStopTclk);
  Offset+=spinw+HorizontalSeparation;
  Win=CreateButton(T("Sync"),IDC_MFPSTOPSYNC,mask);
  SendMessage(Win,BM_SETCHECK,SSEOptions.MfpStopSync ? BST_CHECKED : BST_UNCHECKED,0);
  ToolAddWindow(ToolTip,hEdit,sMfpClock);

  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Timeout to IRQ"));
  Offset=align;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPIRQCPU0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPIRQCPU1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpIrqCpu);
  ToolAddWindow(ToolTip,hEdit,sCpuClock);

  Offset+=spinw+HorizontalSeparation;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPIRQTCLK0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPIRQTCLK1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpIrqTclk);
  ToolAddWindow(ToolTip,hEdit,sMfpClock);

  Offset+=spinw+HorizontalSeparation;
  Win=CreateButton(T("Sync"),IDC_MFPIRQSYNC,mask);
  SendMessage(Win,BM_SETCHECK,SSEOptions.MfpIrqSync ? BST_CHECKED : BST_UNCHECKED,0);

  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Timer B tick"));
  Offset=align;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPTBCPU0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPTBCPU1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpTbCpu);
  ToolAddWindow(ToolTip,hEdit,sCpuClock);

  Offset+=spinw+HorizontalSeparation;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPTBTCLK0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPTBTCLK1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpTbTclk);
  ToolAddWindow(ToolTip,hEdit,sMfpClock);

  Offset+=spinw+HorizontalSeparation;
  Win=CreateButton(T("Sync"),IDC_MFPTBSYNC,mask);
  SendMessage(Win,BM_SETCHECK,SSEOptions.MfpTbSync ? BST_CHECKED : BST_UNCHECKED,0);

  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Timer Read"));
  Offset=align;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPREADCPU0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPREADCPU1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpReadCpu);
  ToolAddWindow(ToolTip,hEdit,sCpuClock);

  Offset+=spinw+HorizontalSeparation;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPREADTCLK0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPREADTCLK1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpReadTclk);
  ToolAddWindow(ToolTip,hEdit,sMfpClock);

  Offset+=spinw+HorizontalSeparation;
  Win=CreateButton(T("Sync"),IDC_MFPREADSYNC,mask);
  SendMessage(Win,BM_SETCHECK,SSEOptions.MfpReadSync ? BST_CHECKED : BST_UNCHECKED,0);

  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Waitstates"));
  lpar_range=MAKELPARAM(12,0);
  Offset=align;
  
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPWSTMG0A,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPWSTMG0B,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpWsTmg[0]);
  ToolAddWindow(ToolTip,hEdit,T("Before read (8MHz)"));
  Offset+=spinw+HorizontalSeparation;

  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPWSTMG1A,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPWSTMG1B,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpWsTmg[1]);
  ToolAddWindow(ToolTip,hEdit,T("After read (8MHz)"));
  Offset+=spinw+HorizontalSeparation;

  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPWSTMG2A,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPWSTMG2B,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpWsTmg[2]);
  ToolAddWindow(ToolTip,hEdit,T("Before write (8MHz)"));
  Offset+=spinw+HorizontalSeparation;

  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPWSTMG3A,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPWSTMG3B,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpWsTmg[3]);
  ToolAddWindow(ToolTip,hEdit,T("After write (8MHz)"));

  NextLine();
  Offset+=HorizontalSeparation;
  Win=CreateButton(T("Spurious interrupt"),IDC_SPURIOUS,mask); // the option is back!
  SendMessage(Win,BM_SETCHECK,(OPTION_SPURIOUS) ? BST_CHECKED : BST_UNCHECKED,0);

  Win=CreateButton(T("No interrupt on OS intercept"),IDC_BLOCKINTERRUPTS,mask);
  SendMessage(Win,BM_SETCHECK,(SSEOptions.BlockInterrrupts) ? BST_CHECKED : BST_UNCHECKED,0);

#else//#if defined(SSE_GUI_421_REORDER)

  int &y=mY,&Wid=mWid,&Offset=mOffset;
  DWORD mask=WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX;
  HWND hGroup,Win,hEdit;
  LPARAM lpar_range=MAKELPARAM(32*TICKS8,-32*TICKS8);
  short const spinw=GuiSM.mCbUnits*2;
  TWidthHeight wh;
  char caption[128];
  const WORD GroupCpuH=GUIMUL(110),GroupMfpH=GUIMUL(270+10);

  // group MC68000 (CPU)
  hGroup=CreateWindow("Button","MC68000",WS_CHILD|BS_GROUPBOX,page_l,y,page_w,
    GroupCpuH,Handle,(HMENU)IDS_STATIC,hInstance,NULL);

  NextLine();
  Offset+=HorizontalSeparation;
  //sprintf(caption,"System 9,999,999,999Hz"); // for 32bit and 64bit?
#ifdef SSE_TIMINGS32
  sprintf(caption,"System 9,999,999,999Hz");
#else
  sprintf(caption,"CPU clock 99,999,999Hz");
#endif
  wh=GetTextSize(Font,caption);
  CreateWindow("Static",caption,WS_CHILD,Offset,y,wh.Width,wh.Height*2,Handle, // notice *2
    (HMENU)(IDC_CPU_CLOCK_S),hInstance,NULL);
  Offset+=wh.Width+HorizontalSeparation;
  int w=32,d=10;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,
    (HMENU)(IDS_TORTOISE),hInstance,NULL);
  Offset+=w+d;
  Wid=page_w-Offset+page_l-w-OptionBox.mLineStart-d;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ,
    Offset,y,Wid,OptionBox.mSliderHeight,Handle,(HMENU)IDC_CPU_CLOCK,hInstance,NULL);
  Offset+=Wid;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,+Offset,y,w,w,Handle,
    (HMENU)(IDS_RABBIT),hInstance,NULL);
  y+=w-OptionBox.mLineHeight/2;
  SendMessage(Win,TBM_SETRANGEMIN,FALSE,8000000*TICKS8); // Steem original
  SendMessage(Win,TBM_SETRANGEMAX,FALSE,8025000*TICKS8);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETTIC,0,CPU_CLOCK_MEGA_ST);
  SendMessage(Win,TBM_SETTIC,0,CPU_CLOCK_STF_PAL);
  SendMessage(Win,TBM_SETPOS,1,CpuCustomHz);
  SendMessage(Win,TBM_SETPAGESIZE,0,1);
  SendMessage(Handle,WM_HSCROLL,0,(LPARAM)Win); // force update

  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic("dbi");
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_DBI0,hInstance,NULL);
  ToolAddWindow(ToolTip,hEdit,T("Check interrupt timing"));
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_DBI1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.dbi);

  // group MC68901 (MFP) - if we had a better emulation, we wouldn't have all those parameters!
  y=GroupCpuH+LineStart;
  hGroup=CreateWindow("Button","MC68901",WS_CHILD|BS_GROUPBOX,page_l,y,page_w,
    GroupMfpH,Handle,(HMENU)IDS_STATIC,hInstance,NULL);

  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("System 9,999,999,999Hz"),IDC_MFPXTAL_S);
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,
    (HMENU)(IDS_TORTOISE),hInstance,NULL);
  Offset+=w+d;
  Wid=page_w-Offset+page_l-w-OptionBox.mLineStart-d;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ,
    Offset,y,Wid,OptionBox.mSliderHeight,Handle,(HMENU)IDC_MFPXTAL,hInstance,NULL);
  Offset+=Wid;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,+Offset,y,w,w,Handle,
    (HMENU)(IDS_RABBIT),hInstance,NULL);
  y+=(w-OptionBox.mLineHeight/2);
  SendMessage(Win,TBM_SETRANGEMIN,FALSE,2457500);
  SendMessage(Win,TBM_SETRANGEMAX,FALSE,2457800);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETTIC,0,MFP_XTAL1);
  SendMessage(Win,TBM_SETTIC,0,MFP_XTAL2);
  SendMessage(Win,TBM_SETPOS,1,Mfp.xtal);
  SendMessage(Win,TBM_SETPAGESIZE,0,1);
  SendMessage(Win,TBM_SETPOS,1,Mfp.xtal);
  SendMessage(Handle,WM_HSCROLL,0,(LPARAM)Win); // force update

  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Timer start"));
  Offset+=GUIMUL(40);
  LONG align=Offset;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPSTARTCPU0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPSTARTCPU1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpStartCpu);
  ToolAddWindow(ToolTip,hEdit,T("System clock"));
  Offset+=spinw+HorizontalSeparation;
  
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPSTARTTCLK0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPSTARTTCLK1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpStartTclk);
  ToolAddWindow(ToolTip,hEdit,T("tCLK"));

  Offset+=spinw+HorizontalSeparation;
  Win=CreateButton(T("Sync"),IDC_MFPSTARTSYNC,mask);
  SendMessage(Win,BM_SETCHECK,SSEOptions.MfpStartSync ? BST_CHECKED : BST_UNCHECKED,0);

  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Timer stop"));
  Offset=align;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPSTOPCPU0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS
    |UDS_SETBUDDYINT|UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPSTOPCPU1,
    hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpStopCpu);
  ToolAddWindow(ToolTip,hEdit,T("System clock"));
  
  Offset+=spinw+HorizontalSeparation;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPSTOPTCLK0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPSTOPTCLK1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpStopTclk);
  Offset+=spinw+HorizontalSeparation;
  Win=CreateButton(T("Sync"),IDC_MFPSTOPSYNC,mask);
  SendMessage(Win,BM_SETCHECK,SSEOptions.MfpStopSync ? BST_CHECKED : BST_UNCHECKED,0);
  ToolAddWindow(ToolTip,hEdit,T("tCLK"));

  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Timeout to IRQ"));
  Offset=align;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPIRQCPU0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPIRQCPU1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpIrqCpu);
  ToolAddWindow(ToolTip,hEdit,T("System clock"));

  Offset+=spinw+HorizontalSeparation;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPIRQTCLK0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPIRQTCLK1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpIrqTclk);
  ToolAddWindow(ToolTip,hEdit,T("tCLK"));

  Offset+=spinw+HorizontalSeparation;
  Win=CreateButton(T("Sync"),IDC_MFPIRQSYNC,mask);
  SendMessage(Win,BM_SETCHECK,SSEOptions.MfpIrqSync ? BST_CHECKED : BST_UNCHECKED,0);

  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Timer B tick"));
  Offset=align;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPTBCPU0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPTBCPU1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpTbCpu);
  ToolAddWindow(ToolTip,hEdit,T("System clock"));

  Offset+=spinw+HorizontalSeparation;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPTBTCLK0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPTBTCLK1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpTbTclk);
  ToolAddWindow(ToolTip,hEdit,T("tCLK"));

  Offset+=spinw+HorizontalSeparation;
  Win=CreateButton(T("Sync"),IDC_MFPTBSYNC,mask);
  SendMessage(Win,BM_SETCHECK,SSEOptions.MfpTbSync ? BST_CHECKED : BST_UNCHECKED,0);

  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Timer Read"));
  Offset=align;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPREADCPU0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPREADCPU1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpReadCpu);
  ToolAddWindow(ToolTip,hEdit,T("System clock"));

  Offset+=spinw+HorizontalSeparation;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPREADTCLK0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPREADTCLK1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpReadTclk);
  ToolAddWindow(ToolTip,hEdit,T("tCLK"));

  Offset+=spinw+HorizontalSeparation;
  Win=CreateButton(T("Sync"),IDC_MFPREADSYNC,mask);
  SendMessage(Win,BM_SETCHECK,SSEOptions.MfpReadSync ? BST_CHECKED : BST_UNCHECKED,0);

  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Waitstates"));
  lpar_range=MAKELPARAM(12,0);
  Offset=align;
  
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPWSTMG0A,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPWSTMG0B,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpWsTmg[0]);
  ToolAddWindow(ToolTip,hEdit,T("Before read"));
  Offset+=spinw+HorizontalSeparation;

  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPWSTMG1A,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPWSTMG1B,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpWsTmg[1]);
  ToolAddWindow(ToolTip,hEdit,T("After read"));
  Offset+=spinw+HorizontalSeparation;

  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPWSTMG2A,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPWSTMG2B,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpWsTmg[2]);
  ToolAddWindow(ToolTip,hEdit,T("Before write"));
  Offset+=spinw+HorizontalSeparation;

  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MFPWSTMG3A,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MFPWSTMG3B,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,lpar_range);
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.MfpWsTmg[3]);
  ToolAddWindow(ToolTip,hEdit,T("After write"));

  NextLine();
  Offset+=HorizontalSeparation;
  Win=CreateButton(T("Spurious interrupt"),IDC_SPURIOUS,mask); // the option is back!
  SendMessage(Win,BM_SETCHECK,(OPTION_SPURIOUS) ? BST_CHECKED : BST_UNCHECKED,0);

  Win=CreateButton(T("No interrupt on OS intercept"),IDC_BLOCKINTERRUPTS,mask);
  SendMessage(Win,BM_SETCHECK,(SSEOptions.BlockInterrrupts) ? BST_CHECKED : BST_UNCHECKED,0);

#endif//#if defined(SSE_GUI_421_REORDER)

#endif//WIN32
}


void TOptionBox::CreateParameters2() {

#if defined(SSE_GUI_421_REORDER)

  int &y=mY,&Wid=mWid,&Offset=mOffset;

#ifdef WIN32
  HWND Win,hEdit;
  short const spinw=GuiSM.mCbUnits*2;
  HWND hGroup;
  const WORD GroupDiskH=GROUP_HEIGHT(4),GroupSpeedH=GROUP_HEIGHT(6);

  // group Speed: general, slow, fast
  hGroup=CreateWindow("Button","Speed",WS_CHILD|BS_GROUPBOX,page_l,y,page_w,
    GroupSpeedH,Handle,(HMENU)IDS_STATIC,hInstance,NULL);
  y+=GUIMUL(20);
  DWORD SimplDis=0;
  CreateWindow("Static","",WS_CHILD|SS_CENTER|SimplDis,page_l+LineStart,y,page_w-LineStart*2,
               20,Handle,(HMENU)IDC_SPEEDTXT,hInstance,NULL);
  y+=20;
#if defined(SSE_GEM_CONTROL_PANEL) // use icons
  int w=32,d=10;
  Offset+=LineStart;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,(HMENU)(IDS_TORTOISE),
               hInstance,NULL);
  Offset+=w+d;
  Wid=page_w-Offset+page_l-w-LineStart-d;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ,Offset,y,Wid,
                   mSliderHeight,Handle,(HMENU)IDC_SPEED,hInstance,NULL);
  Offset+=Wid;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,(HMENU)(IDS_RABBIT),
               hInstance,NULL);
#else
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_TABSTOP|TBS_HORZ|SimplDis,page_l+LineStart,y,
                   page_w-LineStart*2,mSliderHeight,Handle,(HMENU)1041,hInstance,NULL);
#endif
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,199));
  SendMessage(Win,TBM_SETPOS,1,((100000/run_speed_ticks_per_second)-5)/5);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  for(int n=9;n<200;n+=10)
    SendMessage(Win,TBM_SETTIC,0,n);
  SendMessage(Win,TBM_SETPAGESIZE,0,1);
  SendMessage(Handle,WM_HSCROLL,0,(LPARAM)Win);
  NextLine();
  CreateWindow("Static",T("Slow motion speed")+": "+(slow_motion_speed/10)+"%", WS_CHILD|SS_CENTER,
               page_l+LineStart,y,page_w-LineStart*2,20,Handle,(HMENU)1000,hInstance,NULL);
  y+=20;
#if defined(SSE_GEM_CONTROL_PANEL) // use icons
  Offset+=LineStart;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,(HMENU)(IDS_TORTOISE),
               hInstance,NULL);
  Offset+=w+d;
  Wid=page_w-Offset+page_l-w-LineStart-d;
  y+=5;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ,Offset,y,Wid,
                   mSliderHeight,Handle,(HMENU)IDC_SLOWSPEED,hInstance,NULL);
  Offset+=Wid;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,(HMENU)(IDS_RABBIT),
               hInstance,NULL);
#else
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_TABSTOP|TBS_HORZ,page_l+LineStart,y,page_w-
                   LineStart*2,mSliderHeight,Handle,(HMENU)IDC_SLOWSPEED,hInstance,NULL);
#endif
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,79));
  SendMessage(Win,TBM_SETPOS,1,(slow_motion_speed-10)/10);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  for(int n=4;n<79;n+=5)
    SendMessage(Win,TBM_SETTIC,0,n);
  SendMessage(Win,TBM_SETPAGESIZE,0,10);
  NextLine();
  CreateWindow("Static","",WS_CHILD | SS_CENTER,page_l+LineStart,y,page_w-LineStart*2,20,
               Handle,(HMENU)IDC_FASTFWDTXT,hInstance,NULL);
  y+=20;
#if defined(SSE_GEM_CONTROL_PANEL) // use icons
  Offset+=LineStart;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,(HMENU)(IDS_TORTOISE),
               hInstance,NULL);
  Offset+=w+d;
  Wid=page_w-Offset+page_l-w-LineStart-d;
  y+=5;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_AUTOTICKS,Offset,
                   y,Wid,mSliderHeight,Handle,(HMENU)IDC_FASTFWD,hInstance,NULL);
  Offset+=Wid;
  CreateWindow("Static","",WS_CHILD|SS_OWNERDRAW,Offset,y,w,w,Handle,(HMENU)(IDS_RABBIT),
               hInstance,NULL);
  y+=(w-LineHeight/2);
#else
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_TABSTOP|TBS_HORZ|TBS_AUTOTICKS,page_l+LineStart,
                   y,page_w-LineStart*2,mSliderHeight,Handle,(HMENU)1011,hInstance,NULL);
#endif
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(0,18));
  SendMessage(Win,TBM_SETPOS,1,(1000/MAX(fast_forward_max_speed,50))-2);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETTICFREQ,1,0); // only place in Steem where it's used
  SendMessage(Win,TBM_SETPAGESIZE,0,3);
  SendMessage(Handle,WM_HSCROLL,0,(LPARAM)Win);

  // group Disk
  y=GroupSpeedH+LineStart;
  DWORD mask=WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX;
  hGroup=CreateWindow("Button","Disk",WS_CHILD|BS_GROUPBOX,page_l,y,page_w,
    GroupDiskH,Handle,(HMENU)IDS_STATIC,hInstance,NULL);
  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Max track"));
  Offset+=HorizontalSeparation;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MAXTRACK0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MAXTRACK1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(FLOPPY_MAX_TRACK_NUM_CST,79));
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.DiscMaxTrack);
  Offset+=spinw+HorizontalSeparation+GUIMUL(50);
  int tab=Offset;
  Win=CreateButton(T("Fuzzy bits"),IDC_FUZZYBITS,mask);
  SendMessage(Win,BM_SETCHECK,(SSEOptions.FuzzyBits) ? BST_CHECKED : BST_UNCHECKED,0);
  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Track bytes"));
  Offset+=HorizontalSeparation;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,(spinw*3)/2,CharHeight,Handle,(HMENU)IDC_TRACKBYTES0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_TRACKBYTES1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0); 
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(15000,1000));
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.TrackBytes);
  Offset=tab;
  Win=CreateButton(T("Randomize Track"),IDC_RANDOMIZETRACK,mask);
  SendMessage(Win,BM_SETCHECK,(SSEOptions.RandomizeTrack) ? BST_CHECKED : BST_UNCHECKED,0);
  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Drive RPM"));
  Offset+=HorizontalSeparation;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw+10,CharHeight,Handle,(HMENU)IDC_DRIVERPM0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_DRIVERPM1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(320,280));
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.DriveRpm);
  Offset=tab;
  //Win=CreateButton(T("DD disks only"),IDC_FORCEDISKDD,mask);
  //SendMessage(Win,BM_SETCHECK,(SSEOptions.ForceDiskDD) ? BST_CHECKED : BST_UNCHECKED,0);
  Win=CreateButton(T("Bad DMA chip"),IDC_BADDMA,mask);
  SendMessage(Win,BM_SETCHECK,((SSEOptions.BadDma) ? BST_CHECKED : BST_UNCHECKED),0);
  //ToolAddWindow(ToolTip,Win,T(""));
  NextLine();
  NextLine();
  Win=CreateButton(T("Hacks"),IDC_HACKS);
  SendMessage(Win,BM_SETCHECK,OPTION_HACKS,0);
  ToolAddWindow(ToolTip,Win,T("Some options not available on a real ST,\
 conveniences for the player or the programmer. Unchecking the option won't\
 remove all hacks that could be currently engaged, for that you need to reset\
 settings"));
  NextLine();

#endif//WIN32

#else//#if defined(SSE_GUI_421_REORDER)

  int &y=mY/*,&Wid=mWid*/,&Offset=mOffset;
#ifdef WIN32
  HWND Win,hEdit;
  short const spinw=GuiSM.mCbUnits*2;
  const WORD GroupDiskH=GUIMUL(120+10+13),GroupMicrowireH=GUIMUL(120),GroupVideoH=GUIMUL(60+30+30);
  HWND hGroup;

  // group Video
  hGroup=CreateWindow("Button","Video",WS_CHILD|BS_GROUPBOX,page_l,y,page_w,
    GroupVideoH,Handle,(HMENU)IDS_STATIC,hInstance,NULL);

  NextLine();
  Offset+=HorizontalSeparation;
  DWORD mask=WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX;
  Win=CreateButton(T("Tracking VC"),IDC_TRACKINGVIDEOCOUNTER,mask);
  SendMessage(Win,BM_SETCHECK,(SSEOptions.TrackVC) ? BST_CHECKED : BST_UNCHECKED,0);
  ToolAddWindow(ToolTip,Win,T("Video counter"));

  Win=CreateButton(T("Block palette"),IDC_BLOCKPAL,mask);
  SendMessage(Win,BM_SETCHECK,(SSEOptions.BlockPal) ? BST_CHECKED : BST_UNCHECKED,0);

  // Blitter start delay: considered as HW difference and not WU
  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Blitter start"));
  LONG Wid=CbUnits*2;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,Wid,CharHeight,Handle,(HMENU)IDC_BLITTER_WU0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_BLITTER_WU,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(4,1));
  SendMessageW(Win,UDM_SETPOS32,0,OPTION_BLITTER_WU);

  Offset+=HorizontalSeparation+Wid;
  Wid=Offset;

  Win=CreateButton(T("Round rendering on write VC"),IDC_ROUNDWRITEVC,mask);
  ToolAddWindow(ToolTip,Win,T("Video counter"));
  SendMessage(Win,BM_SETCHECK,(SSEOptions.RoundWriteVC) ? BST_CHECKED : BST_UNCHECKED,0);
  NextLine();
  Offset=Wid;
  Win=CreateButton(T("Round rendering on write SM"),IDC_ROUNDWRITESM,mask);
  SendMessage(Win,BM_SETCHECK,(SSEOptions.RoundWriteSM) ? BST_CHECKED : BST_UNCHECKED,0);
  ToolAddWindow(ToolTip,Win,T("Shift mode"));

  // group Disk
  y=GroupVideoH+LineStart;
  hGroup=CreateWindow("Button","Disk",WS_CHILD|BS_GROUPBOX,page_l,y,page_w,
    GroupDiskH,Handle,(HMENU)IDS_STATIC,hInstance,NULL);

  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Max track"));
  Offset+=HorizontalSeparation;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_MAXTRACK0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_MAXTRACK1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(85,79));
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.DiscMaxTrack);

  Offset+=spinw+HorizontalSeparation+GUIMUL(50);
  int tab=Offset;
  Win=CreateButton(T("Fuzzy bits"),IDC_FUZZYBITS,mask);
  SendMessage(Win,BM_SETCHECK,(SSEOptions.FuzzyBits) ? BST_CHECKED : BST_UNCHECKED,0);

  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Track bytes"));
  Offset+=HorizontalSeparation;
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,(spinw*3)/2,CharHeight,Handle,(HMENU)IDC_TRACKBYTES0,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_TRACKBYTES1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0); 
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(15000,1000));
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.TrackBytes);
  
  Offset=tab;
  Win=CreateButton(T("Randomize Track"),IDC_RANDOMIZETRACK,mask);
  SendMessage(Win,BM_SETCHECK,(SSEOptions.RandomizeTrack) ? BST_CHECKED : BST_UNCHECKED,0);

  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Drive RPM"));
  Offset+=HorizontalSeparation;
#if defined(SSE_420R5) // need more space
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw+10,CharHeight,Handle,(HMENU)IDC_DRIVERPM0,hInstance,NULL);
#else
  hEdit=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER,
    Offset,y,spinw,CharHeight,Handle,(HMENU)IDC_DRIVERPM0,hInstance,NULL);
#endif
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT,0,0,0,0,Handle,(HMENU)IDC_DRIVERPM1,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit,0);
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(320,280));
  SendMessageW(Win,UDM_SETPOS32,0,SSEOptions.DriveRpm);

  NextLine();
  Offset=tab;
  Win=CreateButton(T("Ghost disk on read-only"),IDC_GHOSTDISKRO,mask); // more like an option
  SendMessage(Win,BM_SETCHECK,(SSEOptions.GhostDiskRO) ? BST_CHECKED : BST_UNCHECKED,0);

  // group MicroWire - if we had a better emulation, we wouldn't have all those parameters!
  y=GroupVideoH+GroupDiskH+LineStart;
  hGroup=CreateWindow("Button","LMC1992",WS_CHILD|BS_GROUPBOX,page_l,y,page_w,
    GroupMicrowireH,Handle,(HMENU)IDS_STATIC,hInstance,NULL);
  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Bass shelf"));
  Offset+=HorizontalSeparation;
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,Offset,
    y,page_w-Offset+page_l-LineStart,mSliderHeight,Handle,(HMENU)IDC_LOWSHELF,hInstance,NULL);
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(20,1000));
  SendMessage(Win,TBM_SETPOS,1,Microwire.LowShelf);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETPAGESIZE,0,1);
  NextLine();
  Offset+=HorizontalSeparation;
  CreateStatic(T("Treble shelf"));
  Win=CreateWindow(TRACKBAR_CLASS,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TBS_HORZ|TBS_TOOLTIPS,Offset,
    y,page_w-Offset+page_l-LineStart,mSliderHeight,Handle,(HMENU)IDC_HIGHSHELF,hInstance,NULL);
  //SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(20,20000));
  SendMessage(Win,TBM_SETRANGE,0,MAKELPARAM(2000,20000));
  SendMessage(Win,TBM_SETPOS,1,Microwire.HighShelf);
  SendMessage(Win,TBM_SETLINESIZE,0,1);
  SendMessage(Win,TBM_SETPAGESIZE,0,1);
#if defined(SSE_SOUND_MICROWIRE_HACKS)
  NextLine();
  Offset+=HorizontalSeparation;
  Win=CreateButton(T("Slow fade"),IDC_LMCSLOWFADE,mask);
  SendMessage(Win,BM_SETCHECK,SSEOptions.LmcSlowFade,0);
  //ToolAddWindow(ToolTip,Win,"Sea of Colour");
#endif
#endif//WIN32

#endif//#if defined(SSE_GUI_421_REORDER)
}

#endif//#if defined(SSE_GUI_EMUCONTROL)

#if defined(SSE_LOGSECTIONS_FOR_ALL)

void TOptionBox::CreateTracePage() {

  int &y=mY,/*&Wid=mWid,*/&Offset=mOffset;
#ifdef WIN32
  HWND Win;
  DWORD mask=WS_CHILD|WS_TABSTOP|BS_AUTOCHECKBOX;
  int tab=page_l;
  int endy=y;
  for(int n=1;logsections[n].Name[0]!='*';n++)
  {
    int i=logsections[n].Index;
    if(i>=0)
    {
      Offset=tab;
      Win=CreateButton(logsections[n].Name,(IDC_LOGSECTION+i),mask);
      SendMessage(Win,BM_SETCHECK,(logsection_enabled[i]) ? BST_CHECKED : BST_UNCHECKED,0);
      NextLine();
      if(n%11==0) // new column
      {
        tab+=GUIMUL(100);
        endy=y;
        y=LineStart;
      }
    }
  }//nxt
  y=endy;

  mask=WS_CHILD|BS_AUTORADIOBUTTON;
  CreateStatic(T("Detail level"));
  Win=CreateButton("0",IDC_RADIOLOG0,mask|WS_GROUP);
  Win=CreateButton("1",IDC_RADIOLOG1,mask);
  Win=CreateButton("2",IDC_RADIOLOG2,mask);
  Win=CreateButton("3",IDC_RADIOLOG3,mask);
  SendMessage(GetDlgItem(Handle,IDC_RADIOLOG0+SSEConfig.TraceFile),BM_SETCHECK,TRUE,0);
  
  NextLine();
  Win=CreateButton(T("Clear on reset"),IDC_CLEAR_TRACE_ON_RESET);
  SendMessage(Win,BM_SETCHECK,debug_wipe_log_on_reset,0);
  Offset=tab;
  Win=CreateButton(T("Clear now"),IDC_CLEAR_TRACE,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);
#endif//WIN32

}

#endif//#if defined(SSE_LOGSECTIONS_FOR_ALL)


#if defined(SSE_GUI_QUICKSETTINGS)

void TOptionBox::CreateQuickSettings() {

  int &y=mY,/*&Wid=mWid,*/&Offset=mOffset;

#ifdef WIN32
  HWND Win;
  int mask=WS_CHILD|BS_AUTORADIOBUTTON|WS_TABSTOP;
  int align=page_l+GUIMUL(70);
  
  // ST Model
  CreateStatic(T("ST Model"));
  Offset=align;
  Win=CreateButton(st_model_name[STF],IDC_RADIO_ST_MODEL+STF,mask|WS_GROUP);
  ToolAddWindow(ToolTip,Win,T("STF means 16/32 with floppy drive, it was one of\
 the first ST to appear"));
#if defined(SSE_MEGAST)
  Win=CreateButton(st_model_name[MEGA_ST],IDC_RADIO_ST_MODEL+MEGA_ST,mask);
  ToolAddWindow(ToolTip,Win,T("The Mega line was professional"));
#endif
  Win=CreateButton(st_model_name[STE],IDC_RADIO_ST_MODEL+STE,mask);
  ToolAddWindow(ToolTip,Win,T("The STE was more elaborated than the older STF but some\
 programs are compatible only with the STF"));
#if defined(SSE_MEGASTE)
  Win=CreateButton(st_model_name[MEGA_STE],IDC_RADIO_ST_MODEL+MEGA_STE,mask);
  ToolAddWindow(ToolTip,Win,T("The Mega line was professional"));
#endif
  int st_model=(NewStModel>=0) ? NewStModel : ST_MODEL;
  //SendMessage(GetDlgItem(Handle,IDC_RADIO_ST_MODEL+st_model),BM_SETCHECK,TRUE,0);
  CheckRadioButton(Handle,IDC_RADIO_ST_MODEL,IDC_RADIO_ST_MODEL+N_ST_MODELS,
                   IDC_RADIO_ST_MODEL+st_model);

  // RAM
  NextLine();
  CreateStatic(T("RAM"));
  Offset=align;
  Win=CreateButton("512K",IDC_MEMORY_SIZE+0,mask|WS_GROUP);
  ToolAddWindow(ToolTip,Win,T("Not always enough"));
  Win=CreateButton("1 MB",IDC_MEMORY_SIZE+1,mask);
  ToolAddWindow(ToolTip,Win,T("Ought to be enough for everybody"));
  Win=CreateButton("2 MB",IDC_MEMORY_SIZE+2,mask);
  ToolAddWindow(ToolTip,Win,T("More than enough"));
  Win=CreateButton("4 MB",IDC_MEMORY_SIZE+3,mask);
  ToolAddWindow(ToolTip,Win,T("Too much for some programs"));
  // should work with different builds (w/wo 256KB, 2.5MB...)
  BYTE MemConf[2];
  if(NewMemConf0==-1) // no new memory config selected
    GetCurrentMemConf(MemConf);
  else
  {
    MemConf[0]=(BYTE)NewMemConf0;
    MemConf[1]=(BYTE)NewMemConf1;
  }
  //DWORD dwMemConf=MAKELONG(MemConf[0],MemConf[1]);
  int myMemLen=Mmu.BankLength(MemConf[0])+Mmu.BankLength(MemConf[1]);
  for(int i=0;i<4;i++) // check one, uncheck others
    SendMessage(GetDlgItem(Handle,IDC_MEMORY_SIZE+i),BM_SETCHECK,(myMemLen==((512*1024)<<i)),0);
  
  // ROM
  NextLine();
  CreateStatic(T("ROM"));
  Offset=align;
  int k=IDC_TOS_VERSION;
  EasyStr Sel=(NewROMFile.Empty()) ? ROMFile : NewROMFile;
  WORD Ver=0,Date=0;
  BYTE Country,Recognised;
  Tos.GetTosProperties(Sel,Ver,Country,Date,Recognised); //todo need only Ver

  char *local_hint[TOS_LIST_LEN]={
  "EmuTOS",
  "Not as bad as some say",
  "Handles the blitter, released for the Mega ST",
  "Solid for STF/Mega STF",
  "First STE version had ugly bugs",
  "Solid for STE",
  "Released for Mega STE",
  "Ultimate Atari TOS",
  };

  for(WORD i=0;i<TOS_LIST_LEN;i++)
  {
    int mask2=mask;
    char sVer[5];
    sprintf(sVer,"%x.%02x",TosList.Version[i]>>8,TosList.Version[i]&0xFF);
    if(i==0)
    {
      mask2|=WS_GROUP;
      strcpy(sVer,"Emu"); // this hides fake version
    }
    if(i==4)
    {
      NextLine();
      Offset=align;
    }
    if(TosList.Path[i].IsEmpty())
      mask2|=WS_DISABLED;
    Win=CreateButton(sVer,k++,mask2);
    ToolAddWindow(ToolTip,Win,T(local_hint[i]));
    if(Ver==TosList.Version[i])
    {
      SendMessage(Win,BM_SETCHECK,TRUE,0);
      TosList.Selected=i;
    }
  }//nxt i

  // Screen
  NextLine();
  CreateStatic(T("Screen"));
  Offset=align;
  mask=WS_CHILD|BS_AUTORADIOBUTTON;
  Win=CreateButton(T(screen_type[1])+" SC1224",IDC_RADIO_STSCREEN,mask|WS_GROUP);
  ToolAddWindow(ToolTip,Win,T("For your little games"));
  Win=CreateButton(T(screen_type[0])+" SM124",IDC_RADIO_STSCREEN+1,mask);
  ToolAddWindow(ToolTip,Win,T("For the serious stuff"));
  int monitor_sel=NewMonitorSel;
  if(monitor_sel<0) 
    monitor_sel=GetCurrentMonitorSel();
  //if(monitor_sel<2)
    //SendMessage(GetDlgItem(Handle,IDC_RADIO_STSCREEN+monitor_sel),BM_SETCHECK,TRUE,0);
  CheckRadioButton(Handle,IDC_RADIO_STSCREEN,IDC_RADIO_STSCREEN+1,IDC_RADIO_STSCREEN+monitor_sel);

  // Window size
  NextLine();
  CreateStatic(T("Size"));
  Offset=align;
  Win=CreateButton("1x",(IDC_RADIO_DISPLAY_SIZE+1),mask|WS_GROUP);
  if(!OPTION_BLOCK_RESIZE)
    ToolAddWindow(ToolTip,Win,T("You can also directly resize the window"));
  Win=CreateButton("2x",(IDC_RADIO_DISPLAY_SIZE+2),mask);
  if(!OPTION_BLOCK_RESIZE)
    ToolAddWindow(ToolTip,Win,T("You can also directly resize the window"));
  Win=CreateButton("3x",(IDC_RADIO_DISPLAY_SIZE+3),mask);
  if(!OPTION_BLOCK_RESIZE)
    ToolAddWindow(ToolTip,Win,T("You can also directly resize the window"));
  Win=CreateButton("4x",(IDC_RADIO_DISPLAY_SIZE+4),mask);
  if(!OPTION_BLOCK_RESIZE)
    ToolAddWindow(ToolTip,Win,T("You can also directly resize the window"));
  if(!OPTION_BLOCK_RESIZE)
    ToolAddWindow(ToolTip,Win,T("You can also directly resize the window"));
  //SendMessage(GetDlgItem(Handle,IDC_RADIO_DISPLAY_SIZE+DISPLAY_SIZE),BM_SETCHECK,TRUE,0);
  CheckRadioButton(Handle,IDC_RADIO_DISPLAY_SIZE,IDC_RADIO_DISPLAY_SIZE+4,
                   IDC_RADIO_DISPLAY_SIZE+DISPLAY_SIZE);

  // Border size
  NextLine();
  CreateStatic(T("Border"));
  Offset=align;
  Win=CreateButton(T("Off"),IDC_RADIO_BORDER,mask|WS_GROUP);
  ToolAddWindow(ToolTip,Win,T("Fine for most games but you will miss menu scrollers"));
  Win=CreateButton(T("Normal"),IDC_RADIO_BORDER+1,mask);
  ToolAddWindow(ToolTip,Win,T("Typical Atari colour monitor"));
  Win=CreateButton(T("Large"),IDC_RADIO_BORDER+2,mask);
  ToolAddWindow(ToolTip,Win,T("A bit hacky in the execution"));
  Win=CreateButton(T("Max"),IDC_RADIO_BORDER+3,mask);
  ToolAddWindow(ToolTip,Win,T("CM8833-like, sometimes too revealing"));
  //SendMessage(GetDlgItem(Handle,IDC_RADIO_BORDER+border),BM_SETCHECK,TRUE,0);
  CheckRadioButton(Handle,IDC_RADIO_BORDER,IDC_RADIO_BORDER+3,IDC_RADIO_BORDER+border);

  // Effects
  NextLine();
  CreateStatic(T("Effects"));
  Offset=align;
  mask=WS_CHILD|WS_TABSTOP|BS_CHECKBOX;
  Win=CreateButton(T("Ratio"),IDC_STASPECTRATIO);
  SendMessage(Win,BM_SETCHECK,OPTION_ST_ASPECT_RATIO,0);
  ToolAddWindow(ToolTip,Win,
    T("Reproduces the familiar vertical distortion on standard colour screens"));
  Win=CreateButton(T("Scanlines"),IDC_SCANLINES);
  SendMessage(Win,BM_SETCHECK,OPTION_SCANLINES,0);
  ToolAddWindow(ToolTip,Win,T("Reproduces scanlines of cathodic screens"));
#if defined(SSE_VID_SINGLEPIX)
  Win=CreateButton(T("Pixels"),IDC_SINGLEPIXELS);
  SendMessage(Win,BM_SETCHECK,SSEOptions.SinglePixels,0);
  ToolAddWindow(ToolTip,Win,T("Same as scanlines but vertical"));
#endif
#if defined(SSE_VID_D3D_SWEETFX)
  Win=CreateButton(T("CRT"),IDC_CTR_EMU,mask|ENABLE_CHECK(Disp.hD3Dhack));
  SendMessage(Win,BM_SETCHECK,OPTION_CRT_EMU,0);
  ToolAddWindow(ToolTip,Win,T("Cathode Ray Tube screen emulation using SweetFX by CeeJay.dk"));
#endif
  NextLine();
  Offset=align;
#if defined(SSE_VID_BFI)
  Win=CreateButton("BFI",IDC_BFI);
  SendMessage(Win,BM_SETCHECK,OPTION_BFI,0);
  ToolAddWindow(ToolTip,Win,T("Software Black Frame Insertion - VSync or Microseconds"));
#endif
  Win=CreateButton(T("Stretch"),IDC_STRETCH);
  SendMessage(Win,BM_SETCHECK,draw_stretch,0);
  ToolAddWindow(ToolTip,Win,T("Use the video card to convert the image from the ST output size to the size of the window"));
  Win=CreateButton("VSync",IDC_VSYNC);
  ToolAddWindow(ToolTip,Win,T("Adjust emulation speed to your screen refresh rate"));
  SendMessage(Win,BM_SETCHECK,OPTION_WIN_VSYNC,0);

  // Disk
  NextLine();
  CreateStatic(T("Disk"));
  Offset=align;
  //CreateButton(T("Disk A"),IDC_FILESELECA,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);
  //CreateButton(T("Disk B"),IDC_FILESELECB,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);
  CreateWindow("Static",FloppyDisk[0].DiskName,WS_CHILD|WS_VISIBLE|SS_CENTERIMAGE,Offset,y,
               page_w-Offset+page_l-LineStart,CharHeight,Handle,(HMENU)IDS_DISCNAME,hInstance,NULL);
  
  //CreateStatic(FloppyDisk[0].DiskName,IDS_DISCNAME);
  NextLine();
  Offset=align;
  Win=CreateButton(T("PREV"),IDC_PREVDISC,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);
  ToolAddWindow(ToolTip,Win,T("Disk swapper"));
  CreateButton(T("Change"),IDC_FILESELEC,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);
  Win=CreateButton(T("NEXT"),IDC_NEXTDISC,WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE);
  ToolAddWindow(ToolTip,Win,T("Disk swapper"));

  NextLine();
#ifndef SSE421R1
  Offset=align;
#endif
  CreateStatic(T("Hard drive"));
#ifdef SSE421R1
  Offset=align;
#endif
  Win=CreateButton("ACSI",IDC_HDACSI);
  SendMessage(Win,BM_SETCHECK,SSEOptions.Acsi,0);
  ToolAddWindow(ToolTip,Win,T("Low-level using a disk image"));

  Win=CreateButton("GEMDOS",IDC_HDGEMDOS);
  SendMessage(Win,BM_SETCHECK,!HardDiskMan.DisableHardDrives,0);
  ToolAddWindow(ToolTip,Win,T("High-level using a your own folders"));

  // Reboot
  NextLine();
  CreateRebootButton("");

#endif//#ifdef WIN32

}


TTosList::TTosList() {
  int k=0;
  CompatibleModel[k]=(1<<STF)|(1<<STE)|(1<<STFM)|(1<<MEGA_ST)|(1<<MEGA_STE);
  RecommendedModel[k]=0;
  Version[k++]=ETOS_FAKE_VERSION;
  CompatibleModel[k]=(1<<STF)|(1<<STFM)|(1<<MEGA_ST);
  RecommendedModel[k]=(1<<STF);
  Version[k++]=0x100; // recommended is not the best but the most typical
  CompatibleModel[k]=(1<<STF)|(1<<STFM)|(1<<MEGA_ST);
  RecommendedModel[k]=(1<<MEGA_ST);
  Version[k++]=0x102;
  CompatibleModel[k]=(1<<STF)|(1<<STFM)|(1<<MEGA_ST);
  RecommendedModel[k]=0;
  Version[k++]=0x104;
  CompatibleModel[k]=(1<<STE)|(1<<MEGA_STE);;
  RecommendedModel[k]=0;
  Version[k++]=0x106;
  CompatibleModel[k]=(1<<STE)|(1<<MEGA_STE);
  RecommendedModel[k]=(1<<STE);
  Version[k++]=0x162;
  RecommendedModel[k]=CompatibleModel[k]=(1<<MEGA_STE);
  Version[k++]=0x205;
  CompatibleModel[k]=(1<<STF)|(1<<STE)|(1<<STFM)|(1<<MEGA_ST)|(1<<MEGA_STE);
  RecommendedModel[k]=0;
  Version[k++]=0x206;
}


// bAlways true when loading a TOS, false when checking a TOS
void TTosList::AddPath(EasyStr path) {
  WORD Ver=0,Date=0;
  BYTE Country,Recognised;
  Tos.GetTosProperties(path,Ver,Country,Date,Recognised);
  for(int i=0;i<TOS_LIST_LEN;i++)
    if(Version[i]==Ver)
      Path[i]=path;
}

WORD TTosList::GetCompatibleTos(BYTE stmodel) {
  WORD rv=Selected;
  //if(!(CompatibleModel[rv]&(1<<stmodel)))
  {
    for(WORD i=0;i<TOS_LIST_LEN;i++)
    {
      if(CompatibleModel[i]&(1<<stmodel))
        rv=i;
      if(RecommendedModel[i]&(1<<stmodel))
        break;
    }
  }
  return rv;
}

#endif//#if defined(SSE_GUI_QUICKSETTINGS)



#undef LOGSECTION
#ifdef WIN32
#undef LineHeight
#undef HorizontalSeparation
#undef LineStart
#undef CharHeight
#undef CbUnits
#undef SliderHeight
#undef GROUP_HEIGHT
#endif
