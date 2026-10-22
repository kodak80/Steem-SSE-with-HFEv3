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
FILE: options.h
DESCRIPTION: Declarations for Steem's settings dialog (TOptionBox) that allows
the user to change Steem's many options to their heart's delight.
struct TOptionBox, TOption, TConfig, TTosList, TOptionLabel
This file is also included by 6301.c
---------------------------------------------------------------------------*/

#pragma once
#ifndef SSEOPTION_H
#define SSEOPTION_H


#pragma pack(push, 8)

#ifdef __cplusplus // 6301.c

#include <easystr.h>
#include <stemdialogs.h>

#ifdef WIN32
#include <scrollingcontrolswin.h>
#define OPTIONS_HEIGHT0 395
#define OPTIONS_HEIGHT mOptionsHeight
#endif

#ifdef UNIX
#include <x/hxc_dir_lv.h>
#define OPTIONS_HEIGHT 400
#include <stports.h>
#endif

#include "gui.h"
#include "conditions.h"
#include "SSE.h"

enum EOptions {
  //TODO, and it would be nice to use same numbers in Windows and Linux builds
//TODO member emu
  PAGE_GENERAL,PAGE_DISPLAY,PAGE_COLOUR,PAGE_FULLSCREEN,
  PAGE_MIDI,PAGE_SOUND,PAGE_STARTUP,
  PAGE_ASSOC=8,PAGE_MACHINE,PAGE_TOS,PAGE_CONFIG,
  PAGE_PORTS,PAGE_MACROS,PAGE_ICONS,PAGE_OSD,
#if !defined(SSE_GUI_421_REORDER)
  PAGE_MISC,PAGE_STVIDEO,
#endif
  PAGE_INPUT,
#if defined(SSE_GEM_CONTROL_PANEL)
  PAGE_GEM_CP,
#endif
#if defined(SSE_GUI_QUICKSETTINGS)
  PAGE_QUICK,
#endif
#if defined(SSE_GUI_EMUCONTROL)
  PAGE_EMU_PARAM1,PAGE_EMU_PARAM2,
#endif
#if defined(SSE_LOGSECTIONS_FOR_ALL)
  PAGE_TRACE,
#endif
  PAGE_PATHS,

  IDC_FRAMESKIP=202,
  IDC_FSSTRETCH=203,
  IDC_BLITMODE,
  IDC_STRETCH,
  IDC_FSVSYNC,
  IDC_CB_BORDER,
  IDC_CB_ST_MODEL=211,
  IDC_FSSTRETCHRES,IDC_FS640X400, // DD builds
  IDC_GLU_WAKEUP,
  IDC_FAKEFULL,
  IDC_FULSCREEN_ON_MAX,
  IDC_TOGGLE_FULLSCREEN,
  IDP_FSPREFERREDHZ=220, // 220-227
  IDC_CONFIRM_QUIT=230,

  IDC_AUTORESIZE=300,
  IDC_SIZELORES=302,
  IDC_SIZEMEDRES=304,
  IDC_SIZEHIRES=306,
  
  IDC_SHOWTIPS=400,

  IDC_CPU_SPEED=404,


  IDC_SYSKEYS=700, // AllowTaskSwitch
  IDC_AUTOPAUSE=800,
  IDC_AUTOMUTE,
  IDC_STARTONCLICK=901,
  IDC_MOUSESPEED=1000,
  IDC_SLOWSPEED,
  IDC_NEWFILTER,
  IDC_FASTFWDTXT=1010,IDC_FASTFWD=101,
  IDP_SCREENSHOTDIR=1021,
  IDC_SCREENSHOTCHOOSEDIR,
  IDC_SCREENSHOTOPENDIR,
  IDC_MINSCREENSHOT,
  IDC_RESETCOLOURS,
  IDC_HACKS=1027,
  IDC_6301=1029,
  IDC_HIGHPRIORITY,
  IDC_EMUDETECT,
  IDC_SCANLINES,
  IDC_VSYNC,
  IDC_TRIPLE_BUFFERING_WIN,
  IDC_VMMOUSE,
  IDC_SHOWTIME,
  IDC_TRIPLE_BUFFERING,
  IDC_ADVANCED_SETTINGS,
  IDC_ADVANCED_RESET,

  IDC_SPEEDTXT,IDC_SPEED, //1040-1!

  IDC_STASPECTRATIO,

  IDC_EMU_THREAD,
  IDC_UNSTABLE_SHIFTER,
  IDC_LEGACY_TOOLBAR,
  IDC_SCREENSHOT_FORMAT,
  IDC_FI_SCREENSHOT_FORMAT,
  IDC_WARNINGS=1053,
  IDC_OSD_DEBUGINFO,
  IDC_VID_FREQUENCY,
  IDC_OSD_FPSINFO,
  IDC_YM2149_ON,
  IDC_STESOUND_ON,
  IDC_RANDOM_WU,
  IDC_OSD_NONEONSTOP,
  ID_TOOLBAR,
  IDC_FLIPEX,
  IDC_RESET_DISPLAY,
  IDC_TOOLBAR_TASKBAR,
  IDC_TOOLBAR_VERTICAL,
  IDC_SOUNDMUTE,
  IDC_FASTBLITTER,
  IDC_CTR_EMU,
  IDC_MFPXTAL,
  IDC_MFPXTAL_S,
  IDC_FONTSIZE0,
  IDC_FONTSIZE1,
  IDC_BIGGUI,
  IDC_F12RUN,IDC_PAUSERUN,
  IDC_GREYSCREEN,
  IDC_GREENSCREEN,
  IDC_FULLSPECTRUM,
  IDC_TEXTUREFILTER,
  IDC_TOOLBAR,
  IDC_MENUBAR,
  IDC_AUTOVSYNC,
  IDC_AUTOVSYNC_FS,
  IDC_TOSFLAG,
  IDC_TIMINGLOOP,
  IDC_TIMINGLOOP0, // buddy edit must have different number!
  IDC_MICROSECONDS,
  IDC_BFI,
  IDC_BLITTER_WU,
  IDC_BLITTER_WU0,
  IDC_OSD_HINTS,
  IDC_RESETBACKUP,
#if defined(SSE_VID_LS) && defined(SSE_DEFAULTS_421)
  IDC_SNAPSHOTPIC,
#endif
#if defined(SSE_GUI_INSTANTCHANGE)
  IDC_INSTANTMACHINECHANGE,
#endif
#if defined(SSE_VID_SINGLEPIX)
  IDC_SINGLEPIXELS,
#endif
  IDC_SCROLLERSFREQ0,IDC_SCROLLERSFREQ1,IDC_SCROLLERSSEC0,IDC_SCROLLERSSEC1,
#if defined(SSE_GEM_CONTROL_PANEL) // order is important
  IDS_RABBIT,IDS_TORTOISE,IDS_MOUSESLOW,IDS_MOUSEFAST, // GEM graphics
  IDC_COLOUR,
  IDS_RGB=(IDC_COLOUR+PAL_SIZE),
  IDC_RGB=(IDS_RGB+3),
  IDC_CLICK=(IDC_RGB+3),IDC_REPEAT,IDC_BELL,  // conterm bits
  IDC_REPEAT_DELAY,IDC_REPEAT_RATE,
  IDS_KEYDELAYLOW,IDS_KEYDELAYHI,IDS_KEYRATEHI,IDS_KEYRATELOW, // GEM graphics
  IDC_BASS,IDC_TREBLE,
#endif
  IDC_PSGREDUCE,
#if defined(SSE_GUI_421_REORDER)
  IDC_MWVOLUME,IDC_MWBALANCE,IDC_DMAMULTI,IDC_BADDMA,//IDC_FORCEDISKDD,
#endif
#if defined(SSE_GUI_EMUCONTROL)
  IDC_DBI0,IDC_DBI1,
  IDC_MFPSTARTCPU0,IDC_MFPSTARTCPU1,IDC_MFPSTARTTCLK0,IDC_MFPSTARTTCLK1,
  IDC_MFPSTOPCPU0,IDC_MFPSTOPCPU1,IDC_MFPSTOPTCLK0,IDC_MFPSTOPTCLK1,
  IDC_MFPIRQCPU0,IDC_MFPIRQCPU1,IDC_MFPIRQTCLK0,IDC_MFPIRQTCLK1,
  IDC_MFPREADCPU0,IDC_MFPREADCPU1,IDC_MFPREADTCLK0,IDC_MFPREADTCLK1,
  IDC_MFPTBCPU0,IDC_MFPTBCPU1,IDC_MFPTBTCLK0,IDC_MFPTBTCLK1,
  IDC_MFPWSTMG0A,IDC_MFPWSTMG1A,IDC_MFPWSTMG2A,IDC_MFPWSTMG3A,
  IDC_MFPWSTMG0B,IDC_MFPWSTMG1B,IDC_MFPWSTMG2B,IDC_MFPWSTMG3B,
  IDC_MAXTRACK0,IDC_MAXTRACK1,
  IDC_DRIVERPM0,IDC_DRIVERPM1,
  IDC_TRACKBYTES0,IDC_TRACKBYTES1,
#if defined(SSE_SHIFTER_421B)
  IDC_VID_DELAY,
  DUMMY=IDC_VID_DELAY+13, // reserve 14 IDCs for edit + arrow
#endif
  IDC_SEEKSNDDIR,IDC_GHOSTDISKRO,
  IDC_MFPSTARTSYNC,IDC_MFPSTOPSYNC,IDC_MFPIRQSYNC,IDC_MFPTBSYNC,IDC_MFPREADSYNC,
  IDC_SPURIOUS,IDC_BLOCKINTERRUPTS,
  IDC_TRACKINGVIDEOCOUNTER,IDC_BLOCKPAL,
  IDC_ROUNDWRITEVC,IDC_ROUNDWRITESM,
  IDC_FUZZYBITS,IDC_RANDOMIZETRACK,
  IDC_MFMLL,
  IDC_LOWSHELF,IDC_HIGHSHELF,
#endif//#if defined(SSE_GUI_EMUCONTROL)
  IDC_DIRECTMUSIC,IDC_MIDICLOCK,IDC_RAWMIDI,
  IDC_LASERACSI0,IDC_LASERACSI1,IDC_TURBO16MHZ,
  IDC_BRIGHTNESS=2001,
  IDC_CONTRAST,
  IDC_GAMMA,
  ID_BRIGHTNESS_MAP=2010,

  IDC_DESKTOPHZ=2090,

  IDC_SOUNDDEVICE=3001,
  IDC_GDI=3300,
  IDC_NODSOUND,
  IDC_STARTFULL,
  IDC_RESTORESTATE,
#if defined(SSE_GUI_RESTOREPOS)
  IDC_RESTOREWINPOS,
#endif
  IDC_DRAWBUFFER,
  IDC_BLITHIDEM,
  IDC_TRACEFILE=3307,IDC_TRACESHOWPATH,
  IDC_STARTRUN,
  IDC_SNAPSHOTNAME=3311,

#if defined(SSE_AUTOUPDATE)
  IDC_CHECKUPDATE=4000,
#endif

  IDC_ASSOCIATE=5100,
  IDC_ASSOCSCRL=5500,
  IDC_ONE_INSTANCE=5502,

  IDC_MIDIVOL=6001,
  IDC_SYSEXSTATUSOUT=6010,
  IDC_SYSEXSTATUSIN,
  IDC_MIDIUSETIMER,IDC_MIDIUSESLEEP,
  IDC_SYSEXBUFOUT=6021,
  IDC_SYSEXSIZEOUT=6023,
  IDC_SYSEXBUFIN=6031,
  IDC_SYSEXSIZEIN=6033,
  IDS_MIDISPEED=6040,
  IDC_MIDISPEED,
  IDC_OLDFILTER=7099,
  IDC_SOUNDVOL=7100,
  IDC_SAMPLERATE=7101,
  IDC_SAMPLEFORMAT=7061,
#if defined(SSE_GUI_421_REORDER)
  IDC_SOUNDBUFFER0=7103,
#endif
  IDC_SOUNDBUFFER=7104,
  IDC_WAVORYM,
#if defined(SSE_LOGSECTIONS_FOR_ALL)
  IDC_RADIOLOG0,IDC_RADIOLOG1,IDC_RADIOLOG2,IDC_RADIOLOG3,
  IDC_CLEAR_TRACE_ON_RESET,
  IDC_CLEAR_TRACE,
  IDC_LOGSECTION, //... +90
#endif
  IDC_GROUPSOUNDRECORD=7200,
  IDC_SOUNDRECORD,
  IDP_SOUNDRECORD,
  IDC_SOUNDDIRCHOOSE,
  IDC_SOUNDRECORDWARN,
  IDC_KEYBOARDCLICK,
  IDC_MICROWIRE,
  IDC_OLDSYNC,
  IDC_STATUSBAR=7307,
  IDC_DRIVE_SOUND=7310,
  IDC_DRIVESOUNDVOL,
  IDC_YMLL,
  IDC_BETATESTS=7316,
  IDC_LOCKWINDOW,
  IDC_LOCKAR,
  IDC_D3DMODE,
  IDC_CPU_CLOCK=7320,
  IDC_CPU_CLOCK_S,
  IDC_LASERPRINTER=7323,
  IDC_PRINTER,
  IDC_FULLSCREENGUI,
  IDC_RADIO_SWOVERSCAN, // 7326-7328
  IDC_SHIFTER_WU=7330,
  IDC_RADIO_6301BTRY, //7331-7333
  IDC_RTCHACK=7334,
  IDC_RADIO_HWOVERSCAN, //7335-7337
  IDC_RADIO_STSCREEN=7341, //7341-7343
  IDC_DEFCON=7344,
  IDC_YM_12DB,
  IDC_LMCSLOWFADE,
  IDC_SHIFTER_WU0, // buddy edit must have different number!
  IDC_GLU_WAKEUP0, // buddy edit must have different number!
  

  IDC_GROUPBOX=8093,
  IDC_MEMORY_SIZE=8100, // ->8103 if used as radio

  IDC_EXTENDED_MONITOR=8200,
  IDC_TOSLIST=8300,
  IDC_ADDTOS,
  IDC_REMOVETOS,
  IDC_TOSSORTBY=8311,
  IDC_KBDLANG=8401,
  IDC_KBDALTSHIFT,
  IDP_CARTRIDGE=8500,
  IDC_CHOOSECART,
  IDC_REMOVECART,
  IDC_FREEZECART,
  IDC_SWITCHCART,
  IDC_REBOOT=8601,
  IDC_PORTSBASE=9000,
  IDS_CONNECTTO=1,
  IDC_CONNECTTO,
  IDS_MIDIOUTPUT=10,
  IDC_MIDIOUTPUT,
  IDS_MIDIINPUT,
  IDC_MIDIINPUT,
  IDS_PARALLELOUTPUT=20,
  IDC_PARALLELOUTPUT,
  IDS_COMOUTPUT=30,
  IDC_COMOUTPUT,
  IDS_FILEOUTPUT=40,
  IDC_FILECHANGE,
  IDC_FILERESET,

#if defined(SSE_NETWORK)
  IDS_IPSTRING=70,   // =PORTTYPE_TCPIP*10
  IDC_IPSTRING,
  IDS_IPPORT,
  IDC_IPPORT,
  IDC_IPPORT0,
  IDS_NCLIENTS,
  IDC_NCLIENTS,
  IDC_NCLIENTS0,
  IDS_IPSTATUS,
#endif

  IDC_MACROTREE=10000,
  IDC_NEWMACRO,
  IDC_CHOOSEMACRODIR,
  IDC_RECORDMACRO=10011,
  IDC_PLAYMACRO,
  IDC_MACROMOUSESPEED=10014,
  IDC_MACROPLAYSPEED=10016,
  IDC_CONFIGTREE=11000,
  IDC_NEWCONFIG,
  IDC_CHOOSECONFIGDIR,
  IDC_CONFIGTOGGLE=11005,
  //IDC_LOADCONFIG=11011,
  IDC_CONFIGLISTVIEW=11013,
  //IDC_SAVECONFIG,
  IDC_DISKLIGHT=12000,
  IDC_OSDSECONDS=12010,
  IDC_TRACKINFO,
  IDC_OSD_SCROLLERS=12020,
  IDC_OSD_JOKES,
  IDC_NOOSD=12030,

  IDC_LOADICONS=14020,
  IDC_ICONSDEF,
  IDC_ICONSBASE=14100,
  
  IDC_RADIO_ST_MODEL=17340, //-> +N_ST_MODELS-1
  IDC_RADIO_FS_AR=17350,
  IDC_RADIO_BORDER=17360, //->17363
  IDC_RADIO_DISPLAY_SIZE=17371, // -> 17364
  IDC_RADIO_CAPTURE_MOUSE=17380, //-> 17382 // off on auto //1784

#if defined(SSE_GUI_QUICKSETTINGS)
  IDC_TOS_VERSION=17385, // ->+8=17393
  TOS_LIST_LEN=8,
  IDC_PREVDISC=IDC_TOS_VERSION+TOS_LIST_LEN,IDC_FILESELEC,IDC_NEXTDISC,IDS_DISCNAME,
  ETOS_FAKE_VERSION=5, // TODO
#endif

  IDC_PAGETREE=60000 // no control should have IDC above 60000
};

extern EasyStr WAVOutputFile;
extern EasyStringList DSDriverModuleList;

#if defined(SSE_GUI_QUICKSETTINGS)

struct TTosList {
  WORD Selected; // or byte selected, running
  WORD Version[TOS_LIST_LEN];
  BYTE CompatibleModel[TOS_LIST_LEN],RecommendedModel[TOS_LIST_LEN];
  EasyStr Path[TOS_LIST_LEN];
  TTosList();
  void AddPath(EasyStr path);
  WORD GetCompatibleTos(BYTE stmodel);
};

#endif

#if defined(SSE_GUI_DRAG_PAGES)

struct TOptionLabel {
  char* Caption;
  int IconNum;
};

#define MAX_OPTION_LABELS 32 //too much
extern TOptionLabel const StartOptionLabel[MAX_OPTION_LABELS]; // original order

#endif

struct TOptionBox : public TStemDialog {
  TOptionBox();
  ~TOptionBox() {
    Hide();
  }
  void Show(),Hide();
  void ToggleVisible() {
    IsVisible() ? Hide() : Show();
  }
  void EnableBorderOptions(BOOL enable);
  bool ChangeBorderModeRequest(int newborder);
  void ChangeOSDDisable(bool disable);
  void LoadData(bool FirstLoad,TConfigStoreFile *pCSF,bool *SecDisabled=NULL);
  void SaveData(bool FinalSave,TConfigStoreFile *pCSF);
  void CreatePage(int n);
  void CreateRebootButton(EasyStr protip);
  void CreateMachinePage(),CreateTOSPage(),CreateGeneralPage();
  void CreateSoundPage(),CreateDisplayPage(),CreateBrightnessPage();
  void CreateMacrosPage(),CreateProfilesPage(),CreateStartupPage();
  void CreateOSDPage(),CreatePortsPage();
  void CreatePortOptions(int p,int& y);
#if !defined(SSE_GUI_421_REORDER)
  void CreateSSEPage();
#endif
  void CreateInputPage();
  void CreateFullscreenPage();
#if !defined(SSE_GUI_421_REORDER)
  void CreateSTVideoPage(),UpdateSTVideoPage();
#endif
#if defined(SSE_GEM_CONTROL_PANEL)
  void CreateGEMControlPanel();
  void UpdateColour(int colour);
  MEM_ADDRESS TosKeyRepeat;
  BYTE CurrentColour; // which colour is selected?
#endif
#if defined(SSE_GUI_QUICKSETTINGS)
  void CreateQuickSettings();
  TTosList TosList;
#endif
#if defined(SSE_GUI_DRAG_PAGES)
  BOOL DragEntered;
  HWND hDragFrom;
  HTREEITEM hTreeItemFrom;
  TOptionLabel OptionLabel[MAX_OPTION_LABELS]; // player's order
#endif
#if defined(SSE_GUI_EMUCONTROL)
  void CreateParameters1(),CreateParameters2();
#endif
#if defined(SSE_LOGSECTIONS_FOR_ALL)
  void CreateTracePage();
#endif
  void FullscreenBrightnessBitmap();
  void UpdateSoundFreq();
  void ChangeSoundFormat(BYTE bits,BYTE channels);
  void UpdateSoundRecordBut();
  void SetSoundRecord(bool On);
  void SoundMute(bool muting);
  void UpdateMacroRecordAndPlay(Str SelPath="",int Type=0);
  Str CreateMacroFile(bool Edit);
  void LoadProfile(char *File);
  void UpdateParallel();
  void MachineUpdateIfVisible();
  void SSEUpdateIfVisible();
  void RefreshTOSBox(EasyStr Sel="");
#ifdef SSE421R1
  bool NeedReset();
#else
  bool NeedReset() { 
#if defined(SSE_GUI_INSTANTCHANGE)
    return (NewStModel>=0 || NewMemConf0>=0 || NewMonitorSel>=0 || NewROMFile.NotEmpty());
#else
    return (NewMemConf0>=0 || NewMonitorSel>=0 || NewROMFile.NotEmpty());
#endif
  }
#endif
  void NextLine();
  int GetCurrentMonitorSel();
	int TOSLangToFlagIdx(int Lang);
  void EnableControl(int nIDDlgItem,BOOL enabled);

#ifdef WIN32
  static LRESULT CALLBACK WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar);
  static LRESULT CALLBACK Fullscreen_WndProc(HWND Win,UINT Mess,WPARAM wPar,
                                             LPARAM lPar);
  static LRESULT CALLBACK GroupBox_WndProc(HWND Win,UINT Mess,WPARAM wPar,
                                           LPARAM lPar);
  static int DTreeNotifyProc(DirectoryTree*,void *t,int Mess,INT_PTR i1,INT_PTR);
  void DestroyCurrentPage();
  void ManageWindowClasses(bool Unreg);
  void AssAddToExtensionsLV(char *Ext,char *Desc,INT_PTR Num);
  void DrawBrightnessBitmap(HBITMAP);
  void CreateBrightnessBitmap(int w,int h);
  void PortsMakeTypeVisible(int p);

  HWND CreateButton(EasyStr caption,INT_PTR hMenu,int X,int Y,int &Wid,
                    DWORD dwStyle=WS_CHILD|WS_TABSTOP|BS_CHECKBOX,int nHeight=-1);
  HWND CreateButton(EasyStr caption,INT_PTR hMenu,DWORD dwStyle=WS_CHILD|WS_TABSTOP|BS_CHECKBOX);

  HWND CreateStatic(EasyStr caption,INT_PTR hMenu);
  HWND CreateStatic(EasyStr caption);
  void CreateMIDIPage(),CreateAssocPage();
  void CreateIconsPage();
  void IconsAddToScroller();
  bool HasHandledMessage(MSG *mess);
  void LoadIcons();
  void ChangeScreenShotFormat(int NewFormat,Str Ext);
  void ChooseScreenShotFolder(HWND Win);
  static BOOL CALLBACK EnumDateFormatsProc(char *DateFormat);
  void UpdateWindowSizeAndBorder();
  void SetBorder(int newborder);
  void UpdateForNoSound();
#if defined(SSE_VID_DD)
  void UpdateFullscreen();
#endif
#if !defined(SSE_NO_FREEIMAGE)
  void ChangeScreenShotFormatOpts(int NewOpt);
  void FillScreenShotFormatOptsCombo();
#endif
#ifndef SSE_NO_UPDATE
  void CreateUpdatePage();
#endif
  HIMAGELIST il;
  HBITMAP hBrightBmp;
  WNDPROC Old_GroupBox_WndProc;
  ScrollControlWin Scroller;
  static DirectoryTree DTree;
#endif//WIN32  

#ifdef UNIX
  static int WinProc(TOptionBox*,Window,XEvent*);
	static int listview_notify_proc(hxc_listview*,int,INT_PTR);
  static int dd_notify_proc(hxc_dropdown*,int,INT_PTR);
  static int button_notify_proc(hxc_button*,int,int*);
	static int edit_notify_proc(hxc_edit *,int,INT_PTR);
	static int scrollbar_notify_proc(hxc_scrollbar*,int,INT_PTR);
  static int dir_lv_notify_proc(hxc_dir_lv*,int,INT_PTR);
  void DrawBrightnessBitmap(XImage*);
  void UpdateProfileDisplay(Str="",int=-1);
  void FillSoundDevicesDD();
  void CreatePathsPage();
  void UpdatePortDisplay(int);
  int page_p;
  hxc_listview page_lv;
  hxc_button control_parent;
  hxc_button cpu_boost_label,pause_inactive_but;
  hxc_dropdown cpu_boost_dd;
	hxc_button memory_label,monitor_label,tos_group;
  hxc_dropdown memory_dd,monitor_dd;
	hxc_button cart_group,cart_display,cart_change_but,cart_remove_but;
#if defined(SSE_DONGLE)
  hxc_button cart_switch_but,cart_freeze_but;
#endif
#if defined(SSE_MEGA16)
  hxc_button mega_cache_but;
#endif
	hxc_button keyboard_language_label,keyboard_sc_but;
  hxc_dropdown keyboard_language_dd;
	hxc_button coldreset_but;
  hxc_textdisplay mustreset_td;
  hxc_dropdown tos_sort_dd;
  hxc_listview tos_lv;
	hxc_button tosadd_but,tosrefresh_but;
  hxc_button PortGroup[NSTPORTS],ConnectLabel[NSTPORTS];
  hxc_dropdown ConnectDD[NSTPORTS];
  hxc_button IOGroup[NSTPORTS],IOChooseBut[NSTPORTS],IOAllowIOBut[NSTPORTS][2],
    IOOpenBut[NSTPORTS];
  hxc_edit IODevEd[NSTPORTS];
  hxc_button LANGroup[NSTPORTS];
  hxc_button FileGroup[NSTPORTS],FileDisplay[NSTPORTS],FileChooseBut[NSTPORTS],
    FileEmptyBut[NSTPORTS];
  hxc_button high_priority_but,start_click_but;
  hxc_button FFMaxSpeedLabel,SMSpeedLabel,RunSpeedLabel;
  hxc_scrollbar FFMaxSpeedSB,SMSpeedSB,RunSpeedSB;
  hxc_button ff_on_fdc_but;
  hxc_button fs_label;hxc_dropdown frameskip_dd;
  hxc_button bo_label;hxc_dropdown border_dd;
  hxc_button size_group,reschangeresize_but;
#if defined(SSE_VID_SIZE4)
  hxc_button DisplaySize_but[4];
#else  
  hxc_button lowres_doublesize_but,medres_doublesize_but;
#endif
  hxc_button screenshots_group,screenshots_fol_display;
  hxc_button screenshots_fol_label,screenshots_fol_but;
  hxc_button sound_group,sound_mode_label,sound_freq_label,sound_format_label,
    sound_vol_label;
  hxc_button mfpxtal_label;
  hxc_button cpuclock_label;
  hxc_dropdown sound_mode_dd,sound_freq_dd,sound_format_dd;
	hxc_button device_label,record_group,record_but;
	hxc_button wav_output_label,wav_choose_but,overwrite_ask_but;
  hxc_edit device_ed;
  hxc_listview profile_sect_lv;
  IconGroup brightness_ig;
  XImage *brightness_image;
  hxc_button brightness_picture,brightness_picture_label;
  hxc_button brightness_label;
  hxc_scrollbar brightness_sb;
  hxc_button contrast_label;
  hxc_scrollbar contrast_sb;
  hxc_button gamma_label[3];
  hxc_scrollbar gamma_sb[3];
#if defined(SSE_DRIVE_SOUND)
  hxc_scrollbar drivevol_sb;
#endif
#if defined(SSE_YM2149_LL)
  hxc_scrollbar antialias_sb;
#endif
  hxc_scrollbar mainvol_sb;
  hxc_scrollbar mfpxtal_sb;
  hxc_scrollbar cpuclock_sb;
  hxc_button MouseSpeedLabel[4];
  hxc_scrollbar MouseSpeedSB;
  
  hxc_button auto_sts_but;
  hxc_button auto_sts_filename_label;
  hxc_edit auto_sts_filename_edit;
  hxc_button no_shm_but;
#if defined(SSE_AUTOUPDATE)
  hxc_button auto_update;
#endif  
  hxc_button osd_disable_but;
  hxc_listview drop_lv;
  static hxc_dir_lv dir_lv;
  hxc_button internal_speaker_but; // changed in SoundStart
  hxc_button border_size_label; 
  hxc_dropdown border_size_dd;
  hxc_dropdown capture_mouse_dd;
  hxc_button capture_mouse_label;
  hxc_button specific_hacks_but;
  hxc_button emudetect_but;
  hxc_button st_type_label;
#if defined(SSE_HARDWARE_OVERSCAN)
  hxc_button hw_overscan_label;
  hxc_dropdown hw_overscan_dd;
#endif  
  hxc_dropdown st_type_dd;
  hxc_button wake_up_label; 
  hxc_dropdown wake_up_dd;
  hxc_button shifter_wu_label;
  hxc_dropdown shifter_wu_dd; // spinner?
#if defined(SSE_HD6301_LL) 
  hxc_button hd6301emu_but;
#endif
#if defined(SSE_IKBD_RTC)
  hxc_button keyboard_battery_label;
  hxc_dropdown keyboard_battery_dd;
  hxc_button rtc_correct_but;
#endif
  hxc_button optionC2_but,randomWU_but;
  hxc_button optionBW_but,vivid_but;
  hxc_button optionGreen_but;
  hxc_button keyboard_click_but; 
#if defined(SSE_YM2149_FIXED_VOL_TABLE)
  hxc_button psg_samples_but;
#endif
#if defined(SSE_YM2149_LL)
  hxc_button ymll_but;
#endif
  hxc_button ste_microwire_but;
#if defined(SSE_SOUND_MICROWIRE_HACKS)
  hxc_button ste_ym12db_but;
#endif
  hxc_button vm_mouse_but;
#if defined(SSE_DRIVE_SOUND)
  hxc_button drive_sound_but;
#endif  
#if defined(SSE_ACSI_LASER)
  hxc_button laser_but;
#endif
#if defined(SSE_PRINTER)
  hxc_button printer_but;
#endif
#endif//UNIX

  EasyStringList eslTOS;
  ESLSortEnum eslTOS_Sort;
  EasyStr WAVOutputDir;
  Str NewROMFile;
  Str TOSBrowseDir,LastCartFile;
  Str LastIconPath,LastIconSchemePath;
  Str MacroDir,MacroSel;
  Str ProfileDir,ProfileSel;
  int Page;
  int page_l,page_w;
  int mOffset,mY,mWid;
  int NewMemConf0,NewMemConf1,NewMonitorSel;
#if defined(SSE_GUI_INSTANTCHANGE)
  int NewStModel;
#endif
  short mOptionsHeight,mLineHeight,mLineStart,mSliderHeight,mGroupTitleHeight;
  bool eslTOS_Descend;
  bool RecordWarnOverwrite;
  static bool USDateFormat;
};

extern TOptionBox OptionBox;

#endif//#ifdef __cplusplus


///////////////////////
// SSE extra Options //
///////////////////////

struct TOption {
  BYTE Hacks; // used in 6301
  BYTE Battery6301;
  WORD low_pass_frequency; //in Hz
  BYTE STModel;
  BYTE DisplaySize;
  BYTE VideoLogicEmu;
  BYTE WakeUpState; 
  BYTE FontSize;
  BYTE SoundRecordFormat;
  BYTE CaptureMouse;
  BYTE HwOverscan;
  BYTE FullscreenAR;
  BYTE BigIcons;
  BYTE TimingLoop; // 0-20ms
  BYTE BlitterWakeup; // 1-4
  BYTE AudioInterface;
#if defined(__cplusplus)
  char dbi;
  char MfpStartCpu,MfpStartTclk,MfpStartSync;
  char MfpStopCpu,MfpStopTclk,MfpStopSync;
  char MfpIrqCpu,MfpIrqTclk,MfpIrqSync;
  char MfpTbCpu,MfpTbTclk,MfpTbSync;
  char MfpReadCpu,MfpReadTclk,MfpReadSync;
  BYTE MfpWsTmg[4]; // R/W before/after RB RA WB WA
  BYTE DiscMaxTrack;
  WORD DriveRpm;
  WORD TrackBytes;
  bool TrackVC; // VC=video counter
  bool BlockPal;
  bool RoundWriteVC,RoundWriteSM; // VC=video counter, SM=shift mode, rounding cycles on rendering
  bool BlockInterrrupts;
  bool GhostDiskRO; // RO=read-only
#if defined(SSE_GUI_421_REORDER)
  //bool ForceDiskDD;
  bool BadDma;
#endif
  char FuzzyBits,RandomizeTrack;
  bool Chipset1;
  bool Microwire;
  bool CountDmaCycles;
  bool RandomWakeup;
  bool OsdNoneOnStop;
  bool SoundMute;
  bool FastBlitter;
  bool CrtEmu;
  bool Laser;
  bool Printer;  
  bool Spurious;
  bool EmuDetect;
  bool TraceFileLimit;
  bool OsdDriveInfo;
  bool PastiJustSTX;
  bool Scanlines;
  bool StatusBar;
  bool WinVSync;
  bool TripleBufferWin;
  bool DriveSound;
  bool SampledYM;
  bool GhostDisk;
  bool STAspectRatio;
  bool TestingNewFeatures;
  bool BlockResize;
  bool LockAspectRatio;
  bool PRG_support;
  bool Acsi;
  bool KeyboardClick;
  bool MonochromeDisableBorder;
  bool FullScreenGui;
  bool VMMouse;
  bool OsdTime;
  bool CartidgeOff;
  bool FullScreenDefaultHz;
  bool TripleBufferFS;
  bool FakeFullScreen;
  bool Advanced;
  bool YmLowLevel;
  bool FullscreenOnMaximize;
  bool RtcHack; 
  bool StPreselect;
  //bool YM12db;
  bool EmuThread;
  bool UnstableShifter;
  bool AutoSTW;
  bool OsdDebugInfo;
  bool OsdFpsInfo;
  bool F12Run,PauseRun;
  bool GreyScreen,GreenScreen,FullSpectrumPal;
  bool ToolBar;
  bool MenuBar;
  bool AutoVSync,AutoVSyncFS;
  bool TosFlag;
  bool Microseconds;
  bool Bfi;
  bool ScreenshotWithSnapshot;
  bool ResetBackup;
  bool SinglePixels;
  bool MfmLowLevel;
  bool LmcSlowFade; // apart option from 'hacks' now
  bool DirectMusic;
  bool RawMidi;
  bool OldSync;
#if defined(SSE_GUI_INSTANTCHANGE)
  bool InstantMachineChange;
#endif
  bool MidiUseTimer,MidiUseSleep;
  enum EOption {SoundFormatWav,SoundFormatYm};
  TOption();
  void Init();
  void Restore();
#endif
};

// C linkage to be accessible by 6301 emu
#ifdef __cplusplus
extern "C" TOption SSEOptions;
#else
extern struct TOption SSEOptions;
#endif

// if an option is defined as always true or always false, a smart compiler
// can optimise, the macros are also shorter

#define OPTION_HACKS (SSEOptions.Hacks)

#if defined(SSE_NO_OLD_IKBD)
#define OPTION_C1 (TRUE)
#elif !defined(SSE_HD6301_LL)
#define OPTION_C1 (FALSE)
#else
#define OPTION_C1 (SSEOptions.Chipset1) // = low-level 6031 emu + ACIA, MIDI mods
#endif

#define OPTION_C2 (OPTION_VLE==1) // = high-level overscan emulation

#if defined(SSE_VID_STVL1)
#define OPTION_C3 (OPTION_VLE==2) // = low-level overscan emulation
#else
#define OPTION_C3 (FALSE)
#endif
#if !defined(SSE_SOUND_MICROWIRE_OPTION)
#define OPTION_MICROWIRE (TRUE)
#else
#define OPTION_MICROWIRE (SSEOptions.Microwire)
#endif
#define ST_MODEL (SSEOptions.STModel)

#define OPTION_CAPTURE_MOUSE (SSEOptions.CaptureMouse)
#define OPTION_VMMOUSE (SSEOptions.VMMouse)
#define DISPLAY_SIZE (SSEOptions.DisplaySize)
#define OPTION_EMU_DETECT SSEOptions.EmuDetect
#define TRACE_FILE_REWIND (SSEOptions.TraceFileLimit)
#define OPTION_WS (SSEOptions.WakeUpState)
#define OPTION_CARTRIDGE_OFF (SSEOptions.CartidgeOff)
#define OPTION_DRIVE_INFO (SSEOptions.OsdDriveInfo)
#define OPTION_PASTI_JUST_STX (SSEOptions.PastiJustSTX)
#define OPTION_SCANLINES (SSEOptions.Scanlines)
#define OPTION_ST_ASPECT_RATIO (SSEOptions.STAspectRatio)
#define OPTION_LOWPASS (SSEOptions.low_pass_frequency)
#define OPTION_STATUS_BAR (SSEOptions.StatusBar)
#define OPTION_WIN_VSYNC (SSEOptions.WinVSync)
#define OPTION_3BUFFER (SSEOptions.TripleBufferWin)
#if defined(SSE_VID_DD_3BUFFER_WIN)
#define OPTION_3BUFFER_WIN (SSEOptions.TripleBufferWin)
#else
#define OPTION_3BUFFER_WIN (FALSE)
#endif
#define OPTION_3BUFFER_FS (SSEOptions.TripleBufferFS)
#define OPTION_DRIVE_SOUND (SSEOptions.DriveSound)
#define OPTION_GHOST_DISK (SSEOptions.GhostDisk)
#define OPTION_SAMPLED_YM (SSEOptions.SampledYM)
#define OPTION_SOUND_RECORD_FORMAT (SSEOptions.SoundRecordFormat)
#define SSE_TEST_ON (SSEOptions.TestingNewFeatures) //use macro only for actual tests
#define OPTION_BLOCK_RESIZE (SSEOptions.BlockResize)
#define OPTION_LOCK_AR (SSEOptions.LockAspectRatio)
#define OPTION_PRG_SUPPORT (SSEOptions.PRG_support)
#define OPTION_FULLSCREEN_AR (SSEOptions.FullscreenAR)
#define OPTION_KEYBOARD_CLICK (SSEOptions.KeyboardClick)
#define OPTION_FULLSCREEN_GUI (SSEOptions.FullScreenGui)
#define OPTION_OSD_TIME (SSEOptions.OsdTime)
#define OPTION_FULLSCREEN_DEFAULT_HZ (SSEOptions.FullScreenDefaultHz)

#ifdef SSE_VID_DD
#define OPTION_FAKE_FULLSCREEN (draw_fs_blit_mode==DFSM_FAKEFULLSCREEN)
#else
#define OPTION_FAKE_FULLSCREEN (SSEOptions.FakeFullScreen)
#endif

#define OPTION_MAME_YM (SSEOptions.YmLowLevel)
#define RENDER_SIGNED_SAMPLES (sound_num_bits==16)
#define OPTION_ADVANCED (SSEOptions.Advanced)
#define OPTION_MAX_FS (SSEOptions.FullscreenOnMaximize)
#define OPTION_RTC_HACK (SSEOptions.RtcHack)
#define OPTION_VLE (SSEOptions.VideoLogicEmu)
#define OPTION_HWOVERSCAN (SSEOptions.HwOverscan)
#define LACESCAN 1
#define AUTOSWITCH 2
#define OPTION_BATTERY6301 (SSEOptions.Battery6301)
#define OPTION_ST_PRESELECT (SSEOptions.StPreselect)
//#define OPTION_YM_12DB (SSEOptions.YM12db)
#define OPTION_EMUTHREAD (SSEOptions.EmuThread)
#define OPTION_SPURIOUS (SSEOptions.Spurious)
#define OPTION_UNSTABLE_SHIFTER (SSEOptions.UnstableShifter)
#define OPTION_AUTOSTW (SSEOptions.AutoSTW)
#define OPTION_OSD_DEBUGINFO (SSEOptions.OsdDebugInfo)
#define OPTION_OSD_FPSINFO (SSEOptions.OsdFpsInfo)
#define OPTION_COUNT_DMA_CYCLES (SSEOptions.CountDmaCycles)
#define OPTION_RANDOM_WU (SSEOptions.RandomWakeup)
#define OPTION_BLITTER_WU (SSEOptions.BlitterWakeup)
#define OPTION_NO_OSD_ON_STOP (SSEOptions.OsdNoneOnStop)
#define OPTION_SOUNDMUTE (SSEOptions.SoundMute)
#define OPTION_FASTBLITTER (SSEOptions.FastBlitter)
#define OPTION_CRT_EMU (SSEOptions.CrtEmu)
#define OPTION_LASER (SSEOptions.Laser)
#define OPTION_PRINTER (SSEOptions.Printer)
#if defined(SSE_GUI_BIGICONS)
#define BIG_ICONS (SSEOptions.BigIcons)
#else
#define BIG_ICONS 0
#endif
#define FONT_SIZE (SSEOptions.FontSize)
#define OPTION_GREYSCREEN (SSEOptions.GreyScreen)
#define OPTION_GREENSCREEN (SSEOptions.GreenScreen)
//#define OPTION_VIVID (SSEOptions.FullSpectrumPal)
#define OPTION_TOOLBAR (SSEOptions.ToolBar)
#define OPTION_MENUBAR (SSEOptions.MenuBar)
#define OPTION_AUTOVSYNC (SSEOptions.AutoVSync)
#define OPTION_AUTOVSYNC_FS (SSEOptions.AutoVSyncFS)
#define OPTION_TOSFLAG (SSEOptions.TosFlag)
#define OPTION_TIMINGLOOP (SSEOptions.TimingLoop)
#define OPTION_MICROSECONDS (SSEOptions.Microseconds)
#define OPTION_BFI (SSEOptions.Bfi)
#define OPTION_DIRECTMUSIC (SSEOptions.DirectMusic)
#define OPTION_RAWMIDI (SSEOptions.RawMidi)


////////////
// Config //
////////////

struct TConfig {
  int WindowsVersion;
  int CpuBoost; // coarse eg 2=16MHz
  int PageRtf,PagePbm;
  DWORD MidiUnitsSecond;
  WORD Stvl; //0 or version
  BYTE UnrarDll;
  BYTE Hd6301v1Img;
  BYTE unzipd32Dll;
  BYTE CapsImgLib;
  BYTE PastiDll;
  BYTE FreeImageDll;
  BYTE Direct3d9;
  BYTE ArchiveAccess;
  BYTE AcsiImg;
  BYTE VideoCard8bit;
  BYTE VideoCard16bit;
  BYTE VideoCard32bit;
  BYTE ym2149_fixed_vol;
  BYTE mv16; // B.A.T cartridge
  BYTE mr16; // Microdeal Replay 16 cartridge
  BYTE Cubase2Cart;
  BYTE Cubase3Cart;
  BYTE Port0Joy;
  BYTE TraceFile;
  BYTE TraceShowPath;
  BYTE OverscanOn;
  BYTE CpuBoosted;
  BYTE ColourMonitor;
  BYTE old_DisableHardDrives;
  BYTE Blitter;
  BYTE Ste;
  BYTE Mega;
  BYTE CurrentWs;
  BYTE TosRecognised;
  BYTE TosLanguage;
  BYTE IsInit;
#if !defined(SSE_420R5)
  BYTE DiskImageCreated; //1 ST, 2 MSA, 3 DIM
#endif
  BYTE YmSoundOn;
  BYTE SteSoundOn;
  BYTE ShowNotify;
  BYTE TrueFullScreenGui;
  BYTE Size4; // TODO, a bit messy
  BYTE StatusBarMask;
  BYTE FullScreenSize;
  BYTE Border60Hz;
  BYTE SoundMute;
  char MaxJoy; // max enabled ST joystick
  char separator; // . or , for numbers
  BYTE translated;
#if defined(SSE_DIRECTMIDI)
  BYTE DirectMusic;
#endif
#ifdef __cplusplus // visible only to C++ objects
  MEM_ADDRESS MouseAd; // for emu detect, configs
  MEM_ADDRESS bank_length[2];
  TConfig();
  ~TConfig();
  HFONT GuiFont();
  void make_Mem(BYTE conf0,BYTE conf1);
  void SwitchSTModel(BYTE new_type);
  void UpdateMonitor(bool IsColour);
  bool CrtEmu;
#if defined(SSE421R2)
  bool curl;
#endif
#endif
};

// C linkage to be accessible by 6301 emu
#ifdef __cplusplus
extern "C" TConfig SSEConfig;
#else
extern struct TConfig SSEConfig;
#endif

#define CAPSIMG_OK (SSEConfig.CapsImgLib)
#define DX_FULLSCREEN (SSEConfig.FullscreenMask)
#define HD6301_OK (SSEConfig.Hd6301v1Img)
#ifdef SSE_DISK_RAR_SUPPORT_UNIX
#define UNRAR_OK (true)
#else
#define UNRAR_OK (SSEConfig.UnrarDll)
#endif
#define D3D9_OK (SSEConfig.Direct3d9)
#define ARCHIVEACCESS_OK (SSEConfig.ArchiveAccess)
#define ACSI_EMU_ON (SSEConfig.AcsiImg && SSEOptions.Acsi)
#define COLOUR_MONITOR (SSEConfig.ColourMonitor)
#define MONO_MONITOR (!SSEConfig.ColourMonitor)

enum ESTModels {
  STE,STF,
#if defined(SSE_MEGAST)
  MEGA_ST,
#endif
  STFM,
#if defined(SSE_MEGASTE)
  MEGA_STE,
#endif
  N_ST_MODELS};

#define IS_STE (SSEConfig.Ste)
#define IS_STF (1-SSEConfig.Ste)

#if defined(SSE_MEGASTE)
#define IS_MEGASTE (ST_MODEL==MEGA_STE)
#else
#define IS_MEGASTE false
#endif

extern char *st_model_name[],*screen_type[],*overscan_dev[];

#ifdef __cplusplus // 6301.c
extern EasyStr PreciseModel;
#endif

#pragma pack(pop)

#endif//#ifndef SSEOPTION_H
