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
FILE: options.cpp
DESCRIPTION: The code for Steem's option dialog (TOptionBox) that allows
the user to change Steem's many options to their heart's delight.
Also TOption and TConfig.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <conditions.h>
#include <draw.h>
#include <display.h>
#include <gui.h>
#include <loadsave.h>
#include <options.h> 
#include <emulator.h>
#include <options.h>
#include <debug.h>
#include <computer.h>
#include <osd.h>
#include <translate.h>
#include <dataloadsave.h>
#include <macros.h>
#include <choosefolder.h>
#include <diskman.h>
#include <key_table.h>
#include <harddiskman.h>
#include <notifyinit.h>
#if defined(SSE_VID_STVL1)
#include <interface_stvl.h>
#endif
#ifdef WIN32
#include <directory_tree.h>
#if defined(SSE_GUI_DRAG_PAGES)
#include <windowsx.h>
#include <commctrl.h>
#endif
#endif
#ifdef UNIX
#include <x/hxc_prompt.h>
#endif


EasyStringList DSDriverModuleList;
EasyStr PreciseModel;
EasyStr WAVOutputFile;

#if defined(SSE_GUI_DRAG_PAGES)

TOptionLabel const StartOptionLabel[MAX_OPTION_LABELS]={
  "Startup",PAGE_STARTUP,
#if defined(SSE_GUI_QUICKSETTINGS)
  "Quick Settings",PAGE_QUICK,
#endif
  "General",PAGE_GENERAL,
  "Display",PAGE_DISPLAY,
  "Sound",PAGE_SOUND,
#if defined(SSE_GEM_CONTROL_PANEL)
  "GEM Control Panel",PAGE_GEM_CP,
#endif
  "Fullscreen Mode",PAGE_FULLSCREEN,
  "Colour Control",PAGE_COLOUR,
  "Keyboard/Mouse",PAGE_INPUT,
  "Machine",PAGE_MACHINE,
  "TOS (ROM)",PAGE_TOS,
#ifndef SSE_NO_OSD
  "On Screen Display",PAGE_OSD,
#endif
  "I/O Ports",PAGE_PORTS, // was Ports
  "MIDI",PAGE_MIDI,
  "Record Input",PAGE_MACROS, // was Macros
  "Configurations",PAGE_CONFIG, // was Profiles
  "Icons",PAGE_ICONS,
  "File Associations",PAGE_ASSOC,
#if defined(SSE_GUI_EMUCONTROL)
  "Emu Parameters 1",PAGE_EMU_PARAM1,
  "Emu Parameters 2",PAGE_EMU_PARAM2,
#endif
#if defined(SSE_LOGSECTIONS_FOR_ALL)
  "Trace parameters",PAGE_TRACE,
#endif
#ifndef SSE_NO_UPDATE
  "Auto Update",7,
#endif
#if !defined(SSE_GUI_421_REORDER)
  "ST Video",PAGE_STVIDEO,
  "Misc.",PAGE_MISC, // was SSE
#endif
  "",-1
};

#endif//#if defined(SSE_GUI_DRAG_PAGES)


char* st_model_name[]={
  "STE","STF",
#if defined(SSE_MEGAST)
  "Mega ST",
#endif
  "STFM",
#if defined(SSE_MEGASTE)
  "Mega STE"
#endif
};
char* screen_type[]={"Monochrome","Colour"}; // it is translated where necessary
char* overscan_dev[]={"None","LaceScan","AutoSwitch"}; // it is translated where necessary

#ifdef UNIX
hxc_dir_lv TOptionBox::dir_lv;
#endif

#ifdef WIN32
DirectoryTree TOptionBox::DTree;
#endif

bool TOptionBox::USDateFormat=false;

TOption::TOption() {
  Init();
}


void TOption::Init() {
  ZeroMemory(this,sizeof(TOption));
  Hacks=TRUE;
#if defined(SSE_HD6301_LL)
  Chipset1=true;
#endif
  VideoLogicEmu=1; // high level by default
  OsdDriveInfo=true;
  StatusBar=TosFlag=true;
  DriveSound=true;
  SampledYM=true;
  Microwire=true;
  EmuDetect=true;
  PastiJustSTX=true;
  FakeFullScreen=true;
  KeyboardClick=true;
  Spurious=true;
#ifndef SSE_421R0
  CountDmaCycles=true;
#endif
  RandomWakeup=true;
  YmLowLevel=true;
  low_pass_frequency=YM_LOW_PASS_FREQ;
  WakeUpState=3;
  FontSize=GUI_SMALLFONT_SIZE;
  DisplaySize=2;
  BlitterWakeup=4;
  ResetBackup=true;
  CaptureMouse=2; // preferred behaviour
  F12Run=true;
#if defined(SSE_420R5)
  MfpStartCpu=MFP_TIMER_START_DELAY;
  MfpStopCpu=MFP_TIMER_STOP_DELAY;
  MfpStartSync=MFP_TIMER_START_SYNC;
  MfpStopSync=MFP_TIMER_STOP_SYNC;
#else
  MfpStartSync=FALSE;
#endif
  MfpIrqCpu=MFP_TMOUT_TO_IRQ;
  MfpIrqTclk=MFP_TMOUT_TO_IRQ_TCLK;
  MfpIrqSync=FALSE;
  MfpTbCpu=MFP_TIMER_B_COUNT_CYCLES;
  MfpTbTclk=MFP_TIMER_B_COUNT_CYCLES_TCLK;
  MfpTbSync=TRUE;
  MfpReadCpu=MFP_TIMER_READ_ADJUST;
  MfpReadTclk=MFP_TIMER_READ_ADJUST_TCLK;
  //MfpWsTmg[0]=0;
  MfpWsTmg[1]=4;
  MfpWsTmg[2]=4;
  //MfpWsTmg[3]=0;
  DiscMaxTrack=DRIVE_MAX_CYL;
  DriveRpm=DRIVE_RPM_CST;
  TrackBytes=DISK_BYTES_PER_TRACK_CST;
  dbi=DBI_DELAY_CST;
  TrackVC=RoundWriteVC=RoundWriteSM=true;
#if defined(SSE_SHIFTER_421)
  Shifter.RenderingDelay[Shifter.DISPATCHER_CPU]=MMU_PREFETCH_LATENCY/TICKS8;
  Shifter.RenderingDelay[Shifter.DISPATCHER_SET_SHIFT_MODE]=2;
  Shifter.RenderingDelay[Shifter.DISPATCHER_WRITE_VC]=SHIFTER_RASTER_PREFETCH_TIMING/TICKS8;
  //Shifter.RenderingDelay[Shifter.DISPATCHER_SET_PAL]=0;
  //Shifter.RenderingDelay[Shifter.DISPATCHER_DSTE]=0;
  //Shifter.RenderingDelay[Shifter.DISPATCHER_LINEWIDTH]=0;
  //Shifter.RenderingDelay[Shifter.DISPATCHER_DEBUGGER]=0;
  //Shifter.RenderingDelay[Shifter.DISPATCHER_SET_SYNC]=0;
  Shifter.RenderingDelay[Shifter.DISPATCHER_HSCROLL]=-8;
#endif
  FuzzyBits=RandomizeTrack=TRUE;
  FullSpectrumPal=true;
#ifdef UNIX // no such setting
  Advanced=true;
#endif
#ifdef WIN32
  ToolBar=true;
  TimingLoop=2;
#endif
  AudioInterface=1;
  LockAspectRatio=true;
  UnstableShifter=false;
#if defined(SSE_GUI_421_REORDER)
  ::Microwire.DmaMulti=64;
  ::Microwire.PsgReduce=1;
#else
  ::Microwire.PsgReduce=0;
#endif
#if defined(SSE_GUI_DRAG_PAGES)
  for(int k=0,stick=0;k<MAX_OPTION_LABELS;k++)
  {
    if(stick!=-1)
      stick=StartOptionLabel[k].IconNum;
    OptionBox.OptionLabel[k].Caption=StartOptionLabel[k].Caption;
    OptionBox.OptionLabel[k].IconNum=stick; // propagate -1... silly
  }
#endif
#if defined(SSE_DEFAULTS_421)
  DisplaySize=2;
  run_speed_ticks_per_second=1000;
  DiskMan.floppy_access_ff=false;
  nSysCyclesPerSecond=CpuNormalHz;
  CpuNormalHz=CpuCustomHz=CPU_CLOCK_STF_PAL;
  ST_MODEL=STF;
  WinSizeForRes[LORES]=WinSizeForRes[MEDRES]=WinSizeForRes[HIRES]=1;
  LockAspectRatio=true;
  border=border_last_chosen=3; // max
  SampledYM=YmLowLevel=Microwire=true;
  psg_write_n_screens_ahead=PSG_BUFFER_FRAMES;
  DiskMan.bTurboDrive=false; // slow but compatible
  mouse_speed=10;
  SSEConfig.YmSoundOn=SSEConfig.SteSoundOn=true;
  FloppyDrive[DRIVE_A].bSingleSided=FloppyDrive[DRIVE_B].bSingleSided=false;
  FloppyDrive[DRIVE_A].bFreeboot=FloppyDrive[DRIVE_B].bFreeboot=false;
  Shifter.Preload=0;
  frameskip=frameskip_count=1;
  ResChangeResize=false;
  draw_win_mode[0]=draw_win_mode[1]=1;
  draw_win_mode[2]=0;
#ifdef SSE_VID_DD
  Disp.DrawToVidMem=true;
  draw_fs_blit_mode=DFSM_FAKEFULLSCREEN;
#endif
#ifdef SSE_VID_D3D
  Disp.DrawToVidMem=false;
  draw_stretch=0;
  draw_stretch_fs=1;
#endif
#ifdef WIN32
  bAllowTaskSwitch=true;
  Advanced=true;
#endif
  extended_monitor=0;
#if USE_PASTI
  pasti_active=false;
#endif
#if defined(SSE_LIBRETRO)
  sound_freq=sound_chosen_freq=44100;
#else
  sound_freq=sound_chosen_freq=48000;
#endif
#endif//#if defined(SSE_DEFAULTS_421)
}


void TOption::Restore() {

#if defined(SSE_DEFAULTS_421)

  Spurious=false; // true on init
  BYTE saveBigIcons=BigIcons;
  BYTE saveFontSize=FontSize;
  Init();
  BigIcons=saveBigIcons;
  FontSize=saveFontSize;
  SSEConfig.SwitchSTModel(ST_MODEL);
  Mfp.UpdateXtal();
  AdaptCpuBoost();
  StemWinResize();
  Psg.LoadFixedVolTable();
  OptionBox.ChangeSoundFormat(16,2); // needs lots of declarations but proper
  DiskMan.SetNumFloppies(1); // more compatible
  for(int i=0;i<NSTPORTS;i++)
  {
#if defined(SSE_NETWORK)
    STPort[i].Close();
    STPort[i].IPPort=DEFAULT_IP_PORT;
#endif
    STPort[i].Type=0;
  }
  

#else//#if defined(SSE_DEFAULTS_421)

#if defined(SSE_VID_DD)
  draw_fs_blit_mode=DFSM_FAKEFULLSCREEN;
#else
  draw_stretch=0;
  draw_stretch_fs=1;
  FakeFullScreen=true;
  FullScreenGui=false;
#endif
  run_speed_ticks_per_second=1000;
  DiskMan.floppy_access_ff=false;
  SSEConfig.OverscanOn=false;
  SSEConfig.SwitchSTModel(ST_MODEL);
  nSysCyclesPerSecond=CpuNormalHz;
  AdaptCpuBoost();
  extended_monitor=0;
  WinSizeForRes[LORES]=1; // double low
  WinSizeForRes[MEDRES]=1; // double height med
#if defined(SSE_420R5)//0 is 1/2 size
  WinSizeForRes[HIRES]=1; // normal high
#else
  WinSizeForRes[HIRES]=0; // normal high
#endif
  DisplaySize=2;
#ifdef SSE_VID_DD
  Disp.DrawToVidMem=true;
#endif
#ifdef SSE_VID_D3D
  Disp.DrawToVidMem=false;
#endif
#if defined(SSE_VID_SIZE4)
  draw_win_mode[2]=0;//?
#endif
  BlockResize=false;
  LockAspectRatio=true;
  border=1;
  StemWinResize();
#if USE_PASTI
  pasti_active=false;
#endif
  SampledYM=true;
  Psg.LoadFixedVolTable();
  YmLowLevel=true;
  Microwire=true;
#if defined(SSE_LIBRETRO)
  sound_freq=44100;
#else
  sound_freq=48000;
#endif
  psg_write_n_screens_ahead=PSG_BUFFER_FRAMES;
  OptionBox.ChangeSoundFormat(16,2); // needs lots of declarations but proper
  OPTION_SHIFTER_WU=SHIFTER_DEFAULT_WAKEUP;
  UnstableShifter=false;
  Chipset1=true;
  VideoLogicEmu=1;
  VMMouse=false;
  DiskMan.bTurboDrive=false; // slow but compatible
  GhostDisk=false;
  EmuDetect=false;
  Hacks=TRUE;
  LoadSnapShotChangeCart(""); // remove cartridge
  WriteCSFStr("Options","NoDirectDraw","0",globalINIFile);
  WriteCSFStr("Options","NoDirectSound","0",globalINIFile);
  low_pass_frequency=YM_LOW_PASS_FREQ;
  AutoSTW=false;
#if !defined(SSE_420R5)
  SSEConfig.DiskImageCreated=EXT_ST;
#endif
  OsdDebugInfo=false;
  mouse_speed=10;
  KeyboardClick=true; // most compatible
  SSEConfig.YmSoundOn=SSEConfig.SteSoundOn=true;
  FloppyDrive[DRIVE_A].bSingleSided=FloppyDrive[DRIVE_B].bSingleSided=false;
  FloppyDrive[DRIVE_A].bFreeboot=FloppyDrive[DRIVE_B].bFreeboot=false;
  DiskMan.SetNumFloppies(1); // more compatible
  Shifter.Preload=0;
  FastBlitter=false;
  Spurious=true;
  OsdFpsInfo=false;
  SoundMute=false;
  //YM12db=false;
  MuteWhenInactive=0;
  frameskip=1;
  ResChangeResize=true;
  Battery6301=FALSE;
  Mfp.UpdateXtal();
  for(int i=0;i<NSTPORTS;i++)
  {
#if defined(SSE_NETWORK)
    STPort[i].Close();
    STPort[i].IPPort=DEFAULT_IP_PORT;
#endif
    STPort[i].Type=0;
  }
  SSEConfig.StatusBarMask=0;
  BlitterWakeup=4;
#ifdef WIN32
  draw_win_mode[0]=draw_win_mode[1]=1;
  bAllowTaskSwitch=true;
  OPTION_MENUBAR=false;
  ToolBar=true;
  TimingLoop=2;
#endif
  F12Run=true;
#if defined(SSE_420R5)
  MfpStartCpu=MFP_TIMER_START_DELAY;
  MfpStopCpu=MFP_TIMER_STOP_DELAY;
  MfpStartSync=MFP_TIMER_START_SYNC;
  MfpStopSync=MFP_TIMER_STOP_SYNC;
#else
  MfpStartSync=FALSE;
#endif
  MfpIrqCpu=MFP_TMOUT_TO_IRQ;
  MfpIrqTclk=MFP_TMOUT_TO_IRQ_TCLK;
  MfpIrqSync=FALSE;
  MfpTbCpu=MFP_TIMER_B_COUNT_CYCLES;
  MfpTbTclk=MFP_TIMER_B_COUNT_CYCLES_TCLK;
  MfpTbSync=TRUE;
  MfpReadCpu=MFP_TIMER_READ_ADJUST;
  MfpReadTclk=MFP_TIMER_READ_ADJUST_TCLK;
  MfpWsTmg[0]=0;
  MfpWsTmg[1]=4;
  MfpWsTmg[2]=4;
  MfpWsTmg[3]=0;
  DiscMaxTrack=DRIVE_MAX_CYL;
  DriveRpm=DRIVE_RPM_CST;
  TrackBytes=DISK_BYTES_PER_TRACK_CST;
  dbi=DBI_DELAY_CST;
  TrackVC=RoundWriteVC=RoundWriteSM=true;
  FuzzyBits=RandomizeTrack=TRUE;
  FullSpectrumPal=true;

#endif//#if defined(SSE_DEFAULTS_421)

}


TConfig::TConfig() {
  ZeroMemory(this,sizeof(TConfig));
  CpuBoost=1; // = 8MHz
#if !defined(SSE_420R5)
  DiskImageCreated=
#endif
  YmSoundOn=SteSoundOn=ShowNotify=1;
#if defined(SSE_DIRECTMIDI)
  DirectMusic=1;
  MidiUnitsSecond=1000;
#endif
#if defined(DEBUGGER) && defined(SSE_421R0)
  TraceFile=true;
#endif
#ifdef UNIX
#if defined(SSE_DISK_CAPS)
  CapsImgLib=1; // won't run if no caps library
#endif
#endif
}


TConfig::~TConfig() {
#ifdef WIN32
  if(hSteemGuiFont)
  {
    if(hSteemGuiFont!=(HFONT)GetStockObject(DEFAULT_GUI_FONT)) //v402
      DeleteObject(hSteemGuiFont); // free Windows resource
#ifndef LEAN_AND_MEAN
    hSteemGuiFont=NULL;
#endif
  }
#endif
}


#ifdef WIN32
// this replaces (HFONT)GetStockObject(DEFAULT_GUI_FONT)
HFONT TConfig::GuiFont() {
  if(hSteemGuiFont==NULL) // startup only
  {
    hSteemGuiFont=CreateFont(-(FONT_SIZE), 0, 0, 0, FW_NORMAL, 0, 0, 0, 
      ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
      FF_DONTCARE, "Segoe UI");
    if(hSteemGuiFont==NULL) // fall back
      hSteemGuiFont=CreateFont(-(FONT_SIZE), 0, 0, 0, FW_NORMAL, 0, 0, 0, 
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
        FF_DONTCARE, "MS Shell Dlg");
    //ASSERT(hSteemGuiFont);
    if(hSteemGuiFont==NULL) // fall back
      hSteemGuiFont=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
    // font size is in points, 1 pt = 1/72 inch
    // at  96 DPI (100%) 3 pt = 4 pixels
    // at 120 DPI (125%) 3 pt = 5 pixels
    // at 144 DPI (150%) 3 pt = 6 pixels
    GuiSM.mCharHeight=23+FONT_SIZE-10;
#if defined(SSE_421R0)
    GuiSM.mHorizontalSeparation=GUIMUL(5);
#else
    GuiSM.mHorizontalSeparation=5+(FONT_SIZE-10);
#endif
    GuiSM.mCbUnits=20+(FONT_SIZE-10);
  }
  return hSteemGuiFont;
}
#endif


#if defined(SSE_GUI_INSTANTCHANGE)
/*  We create new mem, we copy (part of or all) old mem, we delete old mem
*   ST Memory is written and read backwards on little endian PC
*/ 

void TConfig::make_Mem(BYTE conf0,BYTE conf1) {
  DWORD old_mem_len=mem_len;
  Mmu.Config=(BYTE)((conf0 << 2) | conf1);
  Mmu.bank_length[0]=bank_length[0]=Mmu.BankLength(conf0);
  Mmu.bank_length[1]=bank_length[1]=Mmu.BankLength(conf1);
  mem_len=bank_length[0]+bank_length[1];
  BYTE *newMem=new BYTE[mem_len+MEM_EXTRA_BYTES];
  if(STMem)
  {
    // copy old RAM
    int bytes_to_copy=MIN(old_mem_len,mem_len);
#ifndef BIG_ENDIAN_PROCESSOR
    memcpy(newMem+mem_len-bytes_to_copy,STMem+old_mem_len-bytes_to_copy,bytes_to_copy+MEM_EXTRA_BYTES);
#endif
    delete[] STMem;
  }
  STMem=newMem;
#ifndef BIG_ENDIAN_PROCESSOR
  Mem_End=STMem+mem_len+MEM_EXTRA_BYTES;
  Mem_End_minus_1=Mem_End-1;
  Mem_End_minus_2=Mem_End-2;
  Mem_End_minus_4=Mem_End-4;
#endif
  ZeroMemory(palette_exec_mem,64+PAL_EXTRA_BYTES);
  himem=mem_len;
#if defined(SSE_MMU_MONSTER_ALT_RAM)
  if(himem==MEM_12MB)
    himem=MEM_4MB; //alt-RAM needs to be activated
#endif
  Mmu.Confused=false;
#if defined(SSE_VID_STVL_DIRECT_RAM)
  if(SSEConfig.Stvl>=TStvl::VER_DIRECTRAM)
    StvlUpdate();
#endif
}

#else//#if defined(SSE_GUI_INSTANTCHANGE)

void TConfig::make_Mem(BYTE conf0,BYTE conf1) {
  if(STMem!=NULL)
    delete[] STMem;
  Mmu.Config=(BYTE)((conf0 << 2) | conf1);
  Mmu.bank_length[0]=bank_length[0]=Mmu.BankLength(conf0);
  Mmu.bank_length[1]=bank_length[1]=Mmu.BankLength(conf1);
  mem_len=bank_length[0]+bank_length[1];
  STMem=new BYTE[mem_len+MEM_EXTRA_BYTES];
  //memset(STMem,0xFF,MEM_EXTRA_BYTES);
#ifndef BIG_ENDIAN_PROCESSOR
  Mem_End=STMem+mem_len+MEM_EXTRA_BYTES;
  Mem_End_minus_1=Mem_End-1;
  Mem_End_minus_2=Mem_End-2;
  Mem_End_minus_4=Mem_End-4;
#endif
  ZeroMemory(palette_exec_mem,64+PAL_EXTRA_BYTES);
  himem=mem_len;
#if defined(SSE_MMU_MONSTER_ALT_RAM)
  if(himem==MEM_12MB)
    himem=MEM_4MB; //alt-RAM needs to be activated
#endif
  Mmu.Confused=false;
#if defined(SSE_VID_STVL_DIRECT_RAM)
  if(SSEConfig.Stvl>=TStvl::VER_DIRECTRAM)
    StvlUpdate();
#endif
}

#endif//#if defined(SSE_GUI_INSTANTCHANGE)


void TConfig::SwitchSTModel(BYTE new_type) { 
  ASSERTB(new_type<N_ST_MODELS);
#ifndef SSE_LEAN_AND_MEAN
  if(new_type>=N_ST_MODELS)
    new_type=0;
#endif
  bool change_ws=(new_type!=STE && ST_MODEL==STE && OPTION_WS==4);
  bool same_model=(ST_MODEL==new_type); // eg reboot
#if defined(SSE_MEGA)
#if defined(SSE_MEGA16)
  if(!same_model && (ST_MODEL==MEGA_ST||ST_MODEL==MEGA_STE)) // load snapshot
#else
  if(!same_model && ST_MODEL==MEGA_STE) // load snapshot
#endif
    Cpu16.Ready(false);
#endif
  ST_MODEL=new_type;
  BYTE &tos_language=SSEConfig.TosLanguage; // note Steem won't start without a TOS
  switch(ST_MODEL) {
#if defined(SSE_MEGAST)
  case MEGA_ST:
    CpuNormalHz=CPU_CLOCK_MEGA_ST;
    Blitter=TRUE;
    Ste=FALSE;
    Mega=TRUE;
    if(change_ws)
      OPTION_WS=(OPTION_RANDOM_WU) ? (rand()%4) : 3;
#if defined(SSE_MEGA16)
    if(!same_model)
      Cpu16.Ready(true); // allocate memory
#endif
    break;
#endif
  case STF:
  case STFM:
    CpuNormalHz=(tos_language) ? CPU_CLOCK_STF_PAL : CPU_CLOCK_STF_NTSC; // 0=USA
    Blitter=FALSE;
    Ste=FALSE;
    Mega=FALSE;
    if(change_ws)
      OPTION_WS=(OPTION_RANDOM_WU) ? (rand()%4) : 3;
    break;
  case STE:
  default:
    CpuNormalHz=(tos_language) ? CPU_CLOCK_STE_PAL : CPU_CLOCK_STE_NTSC;
    Blitter=TRUE;
    Ste=TRUE;
    Mega=FALSE;
    OPTION_WS=4; // = WS1
#if defined(SSE_HARDWARE_OVERSCAN)
    OPTION_HWOVERSCAN=0;
#endif
    break;
#if defined(SSE_MEGASTE)
  case MEGA_STE:
    CpuNormalHz=(tos_language) ? CPU_CLOCK_STE_PAL : CPU_CLOCK_STE_NTSC;
    Blitter=TRUE;
    Ste=TRUE;
    Mega=TRUE;
    OPTION_WS=4; // = WS1
#if defined(SSE_HARDWARE_OVERSCAN)
    OPTION_HWOVERSCAN=0;
#endif
    if(!same_model)
      Cpu16.Ready(true); // allocate memory
    break;
#endif
  }//sw
  if(!same_model)
  {
    Glue.Restore();
    Glue.Update();
    SetTimingFunctions();
    if(OPTION_SAMPLED_YM)
    {
#if defined(SSE_EMU_THREAD)
      if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
        SoundLock.Lock();
#endif
      Psg.LoadFixedVolTable(); // reload to adapt to frequency
#if defined(SSE_EMU_THREAD)
      SoundLock.Unlock();
#endif
    }
    make_palette_table(col_brightness,col_contrast);
    Mfp.UpdateXtal(); // default clock for the model
#if !defined(SSE_GUI_INSTANTCHANGE)
    OptionBox.NewMemConf0=0; // hack "need reset"
#endif
    CheckResetIcon();
  }
  CpuCustomHz=CpuNormalHz;
  CpuMfpRatio=(double)CpuNormalHz/(double)Mfp.xtal;
  //TRACE_INIT("%s CPU~%d Hz\n",st_model_name[new_type],CpuNormalHz/TICKS8);
  if(nSysCyclesPerSecond<9000000*TICKS8) // avoid interference with ST CPU Speed option
    nSysCyclesPerSecond=CpuNormalHz; // no wrong CPU speed icon in OSD (3.5.1)
}


void TConfig::UpdateMonitor(bool IsColour) { // refactoring
  ColourMonitor=IsColour;
  screen_res=(ColourMonitor)?LORES:HIRES;
  if(ColourMonitor)
    mfp_gpip_no_interrupt|=MFP_GPIP_COLOUR;
  else
    mfp_gpip_no_interrupt&=MFP_GPIP_NOT_COLOUR;
}


#ifdef WIN32

void TOptionBox::EnableControl(int nIDDlgItem,BOOL enabled) {// TODO use this everywhere
#ifndef SSE_LEAN_AND_MEAN
  if(Handle) 
#endif
    EnableWindow(GetDlgItem(Handle,nIDDlgItem),enabled);
}

#endif//WIN32



TOptionBox::TOptionBox() {
  Section="Options";
  Page=PAGE_GENERAL;
  NewMemConf0=-1,NewMemConf1=-1,NewMonitorSel=-1;
#if defined(SSE_GUI_INSTANTCHANGE)
  NewStModel=-1;
#endif
  RecordWarnOverwrite=true;
  eslTOS.Sort=eslSortByData0;
  eslTOS_Descend=false;
#if defined(SSE_GEM_CONTROL_PANEL)
  CurrentColour=0;
#endif
#ifdef WIN32
  eslTOS.Sort2=eslSortByData0;
  page_l=150;page_w=320;
#if !defined(SSE_420R4)
  Left=(GuiSM.cx_screen()-(3+page_l+page_w+10+3))/2;
  Top=(GuiSM.cy_screen()-(OPTIONS_HEIGHT+6+GuiSM.cy_caption()))/2;
  FSLeft=(640-(3+page_l+page_w+10+3))/2;
  FSTop=(480-(OPTIONS_HEIGHT+6+GuiSM.cy_caption()))/2;
#endif
  hBrightBmp=NULL;
  il=NULL;
#if defined(SSE_GUI_DRAG_PAGES)
  DragEntered=FALSE;
#endif
#endif//WIN32
}

// we use options.txt dump instead, the traces have never been useful!
//#define LOGSECTION LOGSECTION_OPTIONS
//TODO: remove

bool TOptionBox::ChangeBorderModeRequest(int const newborder) {
  int newval=(!FullScreen && !Disp.BorderPossible()) ? 0: newborder;
  bool proceed=true;
  if(MIN((int)border,BIGGEST_BORDER)==MIN(newval,BIGGEST_BORDER)) 
    proceed=false;
  if(proceed) 
    border_last_chosen=(BYTE)newborder;
  return proceed;
}


#ifndef SSE_NO_OSD
void TOptionBox::ChangeOSDDisable(bool disable) {
  OsdControl.disable=disable;
  osd_init_run(false);
#ifdef WIN32
#ifndef SSE_LEAN_AND_MEAN
  if(Handle)
#endif
  {
    if(HWND hwnd=GetDlgItem(Handle,IDC_NOOSD))
      SendMessage(hwnd,BM_SETCHECK,OsdControl.disable,0);
  }
  CheckMenuItem(StemWin_SysMenu,IDSYS_NOOSD,MF_BYCOMMAND|MF_CHECK(disable));
#endif
#ifdef UNIX
  osd_disable_but.set_check(OsdControl.disable);
#endif
  draw(true);
  CheckResetDisplay();
}
#endif


void TOptionBox::SetSoundRecord(bool On) {
  if(!On && bSoundRecord)
  {
    SoundRecordCloseFile();
    bSoundRecord=false;
  }
  else if(On && !bSoundRecord)
  {
#ifdef WIN32
#ifndef SSE_LEAN_AND_MEAN
    if(Handle)
#endif
      if(GetDlgItem(Handle,IDC_SOUNDRECORD))
        SendDlgItemMessage(Handle,IDC_SOUNDRECORD,BM_SETCHECK,true,0);
#endif
#ifdef UNIX
    record_but.set_check(true);
#endif
    int Ret=IDYES;
    if(RecordWarnOverwrite) 
    {
      if(Exists(WAVOutputFile)) 
      {
        Ret=Alert(WAVOutputFile+"\n\n"
          +T("This file already exists, would you like to overwrite it?"),
          T("Record Over?"),MB_ICONQUESTION|MB_YESNO);
      }
    }
    if(Ret==IDYES) 
    {
      timer=timeGetTime();
      sound_record_start_time=timer+100; //start recorfing in 100ms' time
      bSoundRecord=true;
      SoundRecordOpenFile();
    }
  }
#ifdef WIN32
  UpdateSoundRecordBut();
#endif
#ifdef UNIX
  record_but.set_check(bSoundRecord);
#endif
}


void TOptionBox::SoundMute(bool muting) {
  if(muting!=OPTION_SOUNDMUTE)
  {
#if defined(SSE_EMU_THREAD)
    if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
      SoundLock.Lock();
#endif
    if(muting)
      SoundStop();
    OPTION_SOUNDMUTE=muting;
#ifdef WIN32
    UpdateForNoSound();
    // SendDlgItemMessage(Handle,IDC_SOUNDMUTE,BM_SETCHECK,OPTION_SOUNDMUTE,0);
#endif
    if(!muting)
      SoundStart();
#if defined(SSE_EMU_THREAD)
    SoundLock.Unlock();
#endif
    SSEConfig.SoundMute=muting;
  }
}


void TOptionBox::UpdateSoundRecordBut() {
#ifdef WIN32
#ifndef SSE_LEAN_AND_MEAN
  if(Handle)
#endif
    if(GetDlgItem(Handle,IDC_SOUNDRECORD)) 
      SendDlgItemMessage(Handle,IDC_SOUNDRECORD,BM_SETCHECK,bSoundRecord,0);
#endif
#ifdef UNIX
  record_but.set_check(bSoundRecord);
#endif
}


void TOptionBox::UpdateSoundFreq() {
#if defined(SSE_EMU_THREAD)
  if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
    SoundLock.Lock();
#endif
  SoundStop();
#ifdef WIN32
#ifndef SSE_LEAN_AND_MEAN
  if(Handle) 
#endif
    if(HWND Win=GetDlgItem(Handle,IDC_SAMPLERATE)) 
      CBSelectItemWithData(Win,sound_chosen_freq);
#endif
#if defined(SSE_EMU_THREAD)
  SoundLock.Unlock();
#endif
  SoundStart();
}


void TOptionBox::ChangeSoundFormat(BYTE bits,BYTE channels) {
#if defined(SSE_EMU_THREAD)
  if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
    SoundLock.Lock();
#endif
  SoundStop();
  sound_num_bits=bits;
  sound_num_channels=channels;
  sound_bytes_per_sample=(sound_num_bits/8)*sound_num_channels;
#if defined(SSE_EMU_THREAD)
  SoundLock.Unlock();
#endif
  SoundStart();
}


Str TOptionBox::CreateMacroFile(bool Edit) {
  Str Path="";
#ifdef WIN32
#ifndef SSE_LEAN_AND_MEAN
  if(Handle) 
#endif
  {
    if(GetDlgItem(Handle,IDC_MACROTREE)) 
    {
      HTREEITEM Item=DTree.NewItem(T("New Macro"),DTree.RootItem,1,Edit);
      if(Item)
        return DTree.GetItemPath(Item);
      return "";
    }
  }
  Path=GetUniquePath(MacroDir,T("New Macro")+".stmac");
#endif
#ifdef UNIX
  EasyStr name=T("New Macro");
  if (Edit){
    hxc_prompt prompt;
    name=prompt.ask(XD,name,T("Enter Name"));
    if (name.Empty()) return "";
  }
  // Put in current folder
  EasyStr fol=MacroSel;
  RemoveFileNameFromPath(fol,REMOVE_SLASH);
  if (fol.Empty()) fol=MacroDir;
  Path=GetUniquePath(fol,name+".stmac");
#endif
  FILE *fp=fopen(Path,"wb");
  if(fp==NULL)
    return "";
  fclose(fp);
#ifdef UNIX
  if (dir_lv.lv.handle) dir_lv.refresh_fol();
#endif
  return Path;
}


int TOptionBox::GetCurrentMonitorSel() {
  int monitor_sel=MONO_MONITOR;
#ifndef NO_CRAZY_MONITOR
  if(extended_monitor)
  {
    monitor_sel=2;
    for(int n=0;n<EXTMON_RESOLUTIONS;n++)
    {
      if(em_width==extmon_res[n][0]&&em_height==extmon_res[n][1]&&em_planes==extmon_res[n][2])
        monitor_sel=n+2;
    }
  }
#endif
  return monitor_sel;
}


// map internal TOS language code to graphic resource
int TOptionBox::TOSLangToFlagIdx(int const Lang) {
  switch(Lang) {
  case 7: return 0;  //UK
  case 5: return 2;  //French
  case 0: return 1;  //US
  case 9: return 3;  //Spanish
  case 3: return 4;  //German
  case 11: return 5; //Italian
  case 13: return 6; //Swedish
  case 17: case 0x0F: return 7; //Swiss German + French
  case 27: return 8; //Dutch
/*  Adding support of more countries due to Atari compendium & EMUTOS project
    We have flags for Czech, Finland, Norway and Greece (thx avtandil)
    ID '0F' for 'Swiss French';
    ID '13' for 'Turkey';
    ID '15' for 'Finland';
    ID '17' for 'Norway';
    ID '19' for 'Denmark';
    ID '1D' for 'Nederland';
    ID '1F' for 'Czech';
    ID '21' for 'Hungary';
    ID '23' for 'Slovak';
    ID '25' for 'Greece';
    ID '27' for 'Russia';
    ID 'FF' for 'Multilanguage';
    ID '3F' for 'Greece';
*/
  case 39: return 9; //Russian
  case 0x1F: return 10;
  case 0x15: return 11;
  case 0x17: return 12;
  case 0x3F: return 13;
  }
  return -1;
}


void TOptionBox::RefreshTOSBox(EasyStr Sel) {
  // this creates the TOS list in the option page
  // links are in UsersPath, so rom files can be anywhere
#ifdef WIN32
  HWND Win=GetDlgItem(Handle,IDC_TOSLIST);
  if(Win==NULL || Quitting) // this is the only WIN32 use
    return;
  EnumDateFormats(EnumDateFormatsProc,LOCALE_USER_DEFAULT,DATE_SHORTDATE);
  SendMessage(Win,LB_RESETCONTENT,0,0);
  UpdateWindow(Win);
  SendMessage(Win,WM_SETREDRAW,0,0);
#endif
#ifdef UNIX
  if(tos_lv.handle==0)
    return;
  tos_lv.sl.DeleteAll(); //clear out the box
  tos_lv.display_mode=1;
  tos_lv.sl.Sort=eslNoSort;	
  tos_lv.lpig=&IcoTOSFlags;
  tos_lv.columns.DeleteAll();
  tos_lv.columns.Add(5+8+5+hxc::get_text_width(XD,"8.88")+15);
  tos_lv.columns.Add(page_w-hxc::get_text_width(XD,"12/12/2000")-15);
  tos_lv.text_trunc_mode=LVTTM_CUT;
//  EasyStringList eslTOS; // it's in the h...
  eslTOS.Sort2=eslSortByData0;
  //char LinkPath[MAX_PATH+1];
#endif
  EasyStr Fol=UsersPath;
  EasyStr VersionPath; // The first TOS found which matches the current TOS version
  eslTOS.DeleteAll();
  eslTOS.Sort=eslTOS_Sort;
  if(Sel.Empty())
    Sel=(NewROMFile.Empty()) ? ROMFile : NewROMFile;
  DirSearch ds;
  if(ds.Find(Fol+SLASH+"*.*")) 
  {
    EasyStr Path;
    do {
      Path=Tos.GetNextTos(ds);
      if(has_extension_list(Path,"IMG","ROM",NULL)) // could be anything
      {
        WORD Ver,Date;
        BYTE Country,Recognised;
        Tos.GetTosProperties(Path,Ver,Country,Date,Recognised);
#if !defined(SSE_GUI_INSTANTCHANGE) // forget it
        if(!(IS_STF&&Ver>=0x100&&Ver<=0x104
#if defined(SSE_TOS206)
          || Ver==0x206 //&& OPTION_HACKS
#endif
          || IS_STE&&Ver>=0x106&&Ver<=0x206))
          Recognised=0xFE; // mark non-compatible TOS files
#endif
        eslTOS.Add(3,Str(GetFileNameFromPath(Path))+"\01"+Path,Ver,Country|(Recognised<<16),Date);
        if(Ver==tos_version && VersionPath.Empty())
          VersionPath=Path;
      }
    } while(ds.Next());
    ds.Close();
  }
  int Selected=-1,VersionSel=-1,ROMFileSel=-1;
  int i=0,dir=1;
  if(eslTOS_Descend) 
  {
    i=eslTOS.NumStrings-1;
    dir=-1;
  }
  for(int idx=0;idx<eslTOS.NumStrings;idx++) 
  {
    char *FullPath=strrchr(eslTOS[i].String,'\01')+1;
#ifdef WIN32
    SendMessage(Win,LB_INSERTSTRING,idx,LPARAM(""));
#endif
#ifdef UNIX
    Str t;
    if(eslTOS[i].Data[0])
      t=HEXSl(eslTOS[i].Data[0],3).Insert(".",1);
    t+="\01";

    BYTE Recognised=(BYTE)(eslTOS[i].Data[1]>>16);
    if(Recognised)
      t+=(Recognised==1) ? "(V) " : "(x) "; // TODO colours
    t+=Str(GetFileNameFromPath(eslTOS[i].String));
    
    t+="\01";
    if(eslTOS[i].Data[0])
    {
      t+=Str((int)(eslTOS[i].Data[2]&0x1f))+"/";
      t+=Str((int)(eslTOS[i].Data[2]>>5)&0xf)+"/";
      t+=Str((int)(eslTOS[i].Data[2]>>9)+1980);
    }
    t+="\01";
    t+=FullPath;
    tos_lv.sl.Add(t,101+TOSLangToFlagIdx((WORD)eslTOS[i].Data[1]));
#endif
    if(IsSameStr_I(FullPath,Sel)) 
      Selected=idx;
    if(IsSameStr_I(FullPath,ROMFile)) 
      ROMFileSel=idx;
    if(IsSameStr_I(FullPath,VersionPath)) 
      VersionSel=idx;
    i+=dir;
  }
  static bool bRecursing=false;
  if(Selected<0 && ROMFileSel<0 && Exists(ROMFile)) 
  {
    if(!bRecursing) 
    {
#ifdef WIN32
      char* filename=GetFileNameFromPath(ROMFile);
      EasyStr LinkName=UsersPath+SLASH+filename+".lnk";
      int n=2;
      while(Exists(LinkName))
        LinkName=UsersPath+SLASH+filename+" ("+(n++)+")"+".lnk";
      CreateLink(LinkName,ROMFile,T("TOS Image"));
#endif
#ifdef UNIX
      Str Name=GetFileNameFromPath(ROMFile),Ext;
      char *dot=strrchr(Name,'.');
      if(dot)
      {
        Ext=dot;
        *dot='\0';
      }
      EasyStr LinkName=UsersPath+SLASH+Name+Ext;
      int n=2;
      while(Exists(LinkName))
        LinkName=UsersPath+SLASH+Name+"("+(n++)+")"+Ext;
      symlink(ROMFile,LinkName);
#endif
      bRecursing=true;
      RefreshTOSBox(ROMFile);
      bRecursing=false;
    }
  }
  else 
  {
    int iSel=Selected;
    if(iSel<0) 
      iSel=VersionSel;
    if(iSel<0) 
      iSel=MAX(ROMFileSel,0);
#ifdef WIN32
    SendMessage(Win,LB_SETCURSEL,iSel,0);
    SendMessage(Win,LB_SETCARETINDEX,iSel,0);
#endif
#ifdef UNIX
    tos_lv.contents_change();
    tos_lv.changesel(iSel);
#endif
  }
#ifdef WIN32
  SendMessage(Win,WM_SETREDRAW,1,0);
#endif
}


void TOptionBox::LoadProfile(char *File) {
#ifdef WIN32
  TNotify myNotify(T("Loading configuration"));
  // it can take some time, remove the possibility to act too fast!
  DestroyCurrentPage();
#endif
#if defined(SSE_DRIVE_SINGLESIDE)
  FloppyDrive[DRIVE_A].bSingleSided=FloppyDrive[DRIVE_B].bSingleSided=false;
#endif
  TConfigStoreFile CSF(File);
#if defined(SSE_420R4)
  bool DisableSections[256];
  for(BYTE i=0;ProfileSection[i].Name!=NULL;i++)
  {
    DisableSections[ProfileSection[i].ID]=
     (CSF.GetInt("ProfileSections",ProfileSection[i].Name,LVI_SI_CHECKED)==LVI_SI_UNCHECKED);
  }
#else
  bool DisableSections[PSEC_NSECT];
  for(int i=0;i<PSEC_NSECT;i++)
  {
    if(ProfileSection[i].ID>=0&&ProfileSection[i].ID<PSEC_NSECT)
      DisableSections[ProfileSection[i].ID]=(CSF.GetInt("ProfileSections",
        ProfileSection[i].Name,LVI_SI_CHECKED)==LVI_SI_UNCHECKED);
  }
#endif
  LoadAllDialogData(false,File,DisableSections,&CSF);
  // Get current settings
  BYTE CurMemConf[2];
  GetCurrentMemConf(CurMemConf);
  int CurMonSel=GetCurrentMonitorSel();
  Str ProfileROM=CSF.GetStr("Machine","ROM_File",ROMFile);
#if defined(SSE_GUI_CONFIG)
  if(strchr(ProfileROM.Text,SLASHCHAR)==NULL) // no slash = no path
    ProfileROM=TOSBrowseDir+SLASH+ProfileROM;
#endif
  Tos.UpdateTOSPath(&ProfileROM);
  BYTE ProfileMemConf[2]={(BYTE)CSF.GetInt("Machine","Mem_Bank_1",CurMemConf[0]),
                          (BYTE)CSF.GetInt("Machine","Mem_Bank_2",CurMemConf[1])};
  int ProfileMonSel=(CSF.GetInt("Machine","Colour_Monitor",
    mfp_gpip_no_interrupt & MFP_GPIP_COLOUR))==0;
#ifndef NO_CRAZY_MONITOR
  if(CSF.GetInt("Machine","ExMon",extended_monitor))
  {
    UINT pro_em_width=CSF.GetInt("Machine","ExMonWidth",em_width);
    UINT pro_em_height=CSF.GetInt("Machine","ExMonHeight",em_height);
    UINT pro_em_planes=CSF.GetInt("Machine","ExMonPlanes",em_planes);
    ProfileMonSel=2;
    for(int n=0;n<EXTMON_RESOLUTIONS;n++) 
    {
      if(pro_em_width==extmon_res[n][0]&&pro_em_height==extmon_res[n][1]&&
        pro_em_planes==extmon_res[n][2])
        ProfileMonSel=n+2;
    }
  }
#endif
#if defined(SSE_GUI_INSTANTCHANGE)
  if(NewStModel>=0)
    SSEConfig.SwitchSTModel((BYTE)NewStModel);
#endif

  if(NewROMFile.Empty()) 
    if(NotSameStr_I(ROMFile,ProfileROM)) 
      NewROMFile=ProfileROM;
  if(NewMemConf0==-1) 
  {
    if(ProfileMemConf[0]!=CurMemConf[0]||ProfileMemConf[1]!=CurMemConf[1]) 
    {
      NewMemConf0=ProfileMemConf[0];
      NewMemConf1=ProfileMemConf[1];
    }
  }
  if(NewMonitorSel==-1)
    if(ProfileMonSel!=CurMonSel)
      NewMonitorSel=ProfileMonSel;

  // If profile was saved with settings pending, check they still need to pend
#ifdef SSE421R1
  NeedReset();
#else
  if(IsSameStr_I(NewROMFile,ROMFile)) 
    NewROMFile="";
  if(NewMemConf0==CurMemConf[0]&&NewMemConf1==CurMemConf[1]) 
    NewMemConf0=-1;
  if(NewMonitorSel==CurMonSel) 
    NewMonitorSel=-1;
#if defined(SSE_GUI_INSTANTCHANGE)
  if(NewStModel==ST_MODEL)
    NewStModel=-1;
#endif
#endif


  CSF.Close();
  //if(Handle) 
    //SetForegroundWindow(Handle);
#if defined(SSE_REF_402) // fix wrong data on status bar after loading config
  if(runstate==RUNSTATE_STOPPED)
    reset_st(RESET_COLD); // a bit risky?
#else
#if !defined(SSE_LIBRETRONUKE)
  CheckResetIcon();
  CheckResetDisplay();
#endif
#endif
#if defined(SSE_DRIVE_SINGLESIDE)
  if(DiskMan.IsVisible()) // refresh for SF354 possibility
  {
#ifdef WIN32    
    for(int id=IDC_DRIVEA;id<=IDC_DRIVEB;id++)
      InvalidateRect(GetDlgItem(DiskMan.Handle,id),NULL,FALSE);
#endif
  }
#endif
#ifdef WIN32
  CreatePage(Page);
#endif
}


void TOptionBox::UpdateMacroRecordAndPlay(Str SelPath,int Type) {
  if(Handle==NULL) 
    return;
  TMacroFileOptions MFO;
#ifdef WIN32
  if(GetDlgItem(Handle,IDC_MACROTREE)==NULL) 
    return;
  if(SelPath.Empty()) 
  {
    HTREEITEM SelItem=(HTREEITEM)SendMessage(DTree.hTree,TVM_GETNEXTITEM,TVGN_CARET,0);
    SelPath=DTree.GetItemPath(SelItem);
    Type=DTree.GetItem(SelItem,TVIF_IMAGE).iImage;
  }
  bool CheckRec=false,CheckPlay=false;
  if(Type==1) 
  {
    if(macro_record && IsSameStr_I(macro_record_file,SelPath)) 
      CheckRec=true;
    if(macro_play && IsSameStr_I(macro_play_file,SelPath)) 
      CheckPlay=true;
  }
  SendDlgItemMessage(Handle,IDC_RECORDMACRO,BM_SETCHECK,CheckRec,0);
  SendDlgItemMessage(Handle,IDC_PLAYMACRO,BM_SETCHECK,CheckPlay,0);
  macro_file_options(MACRO_FILE_GET,SelPath,&MFO);
  CBSelectItemWithData(GetDlgItem(Handle,IDC_MACROPLAYSPEED),MFO.allow_same_vbls);
  CBSelectItemWithData(GetDlgItem(Handle,IDC_MACROMOUSESPEED),MFO.max_mouse_speed);
#endif//WIN32
#ifdef UNIX
  hxc_button *p_grp=(hxc_button*)hxc::find(page_p,2009);
  if (p_grp==NULL) return;
  hxc_button *p_rec=(hxc_button*)hxc::find(p_grp->handle,2010);
  hxc_button *p_play=(hxc_button*)hxc::find(p_grp->handle,2011);
  hxc_dropdown *p_ms=(hxc_dropdown*)hxc::find(p_grp->handle,2012);
  hxc_dropdown *p_ped=(hxc_dropdown*)hxc::find(p_grp->handle,2013);
  if (SelPath.Empty()){
    Type=-1;
    if (dir_lv.lv.sel>=0){
      SelPath=dir_lv.get_item_path(dir_lv.lv.sel);
      Type=dir_lv.sl[dir_lv.lv.sel].Data[DLVD_TYPE];
    }
  }
  ShowHideWindow(XD,p_grp->handle,Type==2);
  bool CheckRec=0,CheckPlay=0;
  if (Type==2){
    if (macro_record && IsSameStr_I(macro_record_file,SelPath)) CheckRec=true;
    if (macro_play && IsSameStr_I(macro_play_file,SelPath)) CheckPlay=true;
  }
  p_play->set_check(CheckPlay);
  p_rec->set_check(CheckRec);
  macro_file_options(MACRO_FILE_GET,SelPath,&MFO);
  p_ms->select_item_by_data(MFO.max_mouse_speed);
  p_ped->select_item_by_data(MFO.allow_same_vbls);
  p_ms->draw();
  p_ped->draw();
#endif
}


void TOptionBox::Show() {
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
  ManageWindowClasses(SD_REGISTER);
  Handle=CreateWindowEx(WS_EX_CONTROLPARENT,"Steem Options",T("Settings"),WS_CAPTION|WS_SYSMENU,
                        Left,Top,0,0,ParentWin,NULL,hInstance,NULL);
#if defined(SSE_420R4)
  UpdateLeftTop();
#endif
  if(HandleIsInvalid()) 
  {
    ManageWindowClasses(SD_UNREGISTER);
    return;
  }
  Font=SSEConfig.GuiFont();
  mOptionsHeight=(OPTIONS_HEIGHT0*(FONT_SIZE+5))/15;
  if(BIG_ICONS)
    mOptionsHeight=(OPTIONS_HEIGHT0*20)/10-150;//156;
  page_w=(320*(FONT_SIZE+6))/16;
  mLineHeight=30+(FONT_SIZE-10);
  mLineStart=10+(FONT_SIZE-10)*1;
  mGroupTitleHeight=15+(FONT_SIZE-10)+2;
  mSliderHeight=(FONT_SIZE<16) ? 18 : 28;
  //mSliderHeight=18 + FONT_SIZE -GUI_FONT_SIZE;
  SetWindowLongPtr(Handle,GWLP_USERDATA,(LONG_PTR)this);
  MakeParent((FullScreen) ? StemWin : NULL);
  LoadIcons();
#if defined(SSE_GUI_DRAG_PAGES) // drag&drog enabled
  PageTree=CreateWindowEx(WS_EX_CLIENTEDGE,WC_TREEVIEW,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP
                          |TVS_HASLINES|TVS_SHOWSELALWAYS|TVS_HASBUTTONS,0,0,100,OPTIONS_HEIGHT,
                          Handle,(HMENU)IDC_PAGETREE,hInstance,NULL);
#else
  PageTree=CreateWindowEx(WS_EX_CLIENTEDGE,WC_TREEVIEW,"",WS_CHILD|WS_VISIBLE|
    WS_TABSTOP|TVS_HASLINES|TVS_SHOWSELALWAYS|TVS_HASBUTTONS|TVS_DISABLEDRAGDROP
    ,0,0,100,OPTIONS_HEIGHT,Handle,(HMENU)IDC_PAGETREE,hInstance,NULL);
#endif
  SendMessage(PageTree,TVM_SETIMAGELIST,TVSIL_NORMAL,(LPARAM)il);
  SendMessage(PageTree,WM_SETFONT,(WPARAM)Font,0);

#if defined(SSE_GUI_DRAG_PAGES)
  for(int i=0;(i<MAX_OPTION_LABELS&&OptionLabel[i].IconNum>-1);i++)
    AddPageLabel(T(OptionLabel[i].Caption),OptionLabel[i].IconNum);
  int 
#else
  AddPageLabel(T("Startup"),PAGE_STARTUP);
#if defined(SSE_GUI_QUICKSETTINGS)
  AddPageLabel(T("Quick settings"),PAGE_QUICK);
#endif
  AddPageLabel(T("General"),PAGE_GENERAL);
  AddPageLabel(T("Machine"),PAGE_MACHINE);
  AddPageLabel("TOS (ROM)",PAGE_TOS);
#if !defined(SSE_GUI_421_REORDER)
  AddPageLabel(T("ST Video"),PAGE_STVIDEO); 
#endif
  AddPageLabel(T("Display"),PAGE_DISPLAY);
  AddPageLabel(T("Fullscreen Mode"),PAGE_FULLSCREEN);
  AddPageLabel(T("Colour Control"),PAGE_COLOUR);
#ifndef SSE_NO_OSD
  AddPageLabel(T("On Screen Display"),PAGE_OSD);
#endif
  AddPageLabel(T("Sound"),PAGE_SOUND);
  AddPageLabel(T("Keyboard/Mouse"),PAGE_INPUT);
  AddPageLabel(T("I/O Ports"),PAGE_PORTS); // was Ports
  AddPageLabel("MIDI",PAGE_MIDI);
  AddPageLabel(T("Configurations"),PAGE_CONFIG); // was Profiles
  AddPageLabel(T("Record Input"),PAGE_MACROS); // was Macros
  ALL_SETTINGS_BEGIN
  AddPageLabel(T("Icons"),PAGE_ICONS);
#ifndef SSE_NO_UPDATE
  AddPageLabel(T("Auto Update"),7);
#endif
  AddPageLabel(T("File Associations"),PAGE_ASSOC);
  ALL_SETTINGS_END
  int tree_w=TreeGetMaxItemWidth(PageTree)+GuiSM.mHorizontalSeparation*2;
#if defined(SSE_GEM_CONTROL_PANEL)
  tree_w+=GetSystemMetrics(SM_CXHSCROLL); // if we add more pages, we need a scroller (BIG GUI)
  AddPageLabel(T("GEM Control Panel"),PAGE_GEM_CP);
#endif
#if defined(SSE_GUI_EMUCONTROL)
  ALL_SETTINGS_BEGIN
  AddPageLabel(T("Emu parameters 1"),PAGE_EMU_PARAM1);
  AddPageLabel(T("Emu parameters 2"),PAGE_EMU_PARAM2);
  ALL_SETTINGS_END
#endif
#if !defined(SSE_GUI_421_REORDER)
  AddPageLabel("Misc.",PAGE_MISC); // was SSE
#endif
#endif//#if defined(SSE_GUI_DRAG_PAGES)

  tree_w=TreeGetMaxItemWidth(PageTree)+GuiSM.mHorizontalSeparation*2;
  tree_w+=GetSystemMetrics(SM_CXHSCROLL); // if we add more pages, we need a scroller (BIG GUI)
  page_l=tree_w+GuiSM.mHorizontalSeparation*2;

#if defined(SSE_421R0)
  int cxpaddedborder=GetSystemMetrics(92); // SM_CXPADDEDBORDER not defined in XP
  SetWindowPos(Handle,NULL,0,0,3+page_l+page_w+GuiSM.mHorizontalSeparation*2+3*cxpaddedborder+GUIMUL(10),
               OPTIONS_HEIGHT+6+3*cxpaddedborder+GuiSM.cy_caption(),SWP_NOZORDER|SWP_NOMOVE);
#elif defined(SSE_GUI_FIX)
  SetWindowPos(Handle,NULL,0,0,3+page_l+page_w+GuiSM.mHorizontalSeparation*2+3
    +3*GetSystemMetrics(92),OPTIONS_HEIGHT+6+3*GetSystemMetrics(92)+GuiSM.cy_caption(),
    SWP_NOZORDER|SWP_NOMOVE);
#else
  SetWindowPos(Handle,NULL,0,0,3+page_l+page_w+GuiSM.mHorizontalSeparation*2+3,
    OPTIONS_HEIGHT+6+GuiSM.cy_caption(),SWP_NOZORDER|SWP_NOMOVE);
#endif
  SetWindowPos(PageTree,NULL,0,0,tree_w,OPTIONS_HEIGHT,SWP_NOZORDER|SWP_NOMOVE);
  Focus=NULL;
  TreeSelectItemWithData(PageTree,Page);
  ShowWindow(Handle,SW_SHOW);
  SetFocus(Focus);
  if(StemWin!=NULL) 
    PostMessage(StemWin,WM_USER,MSG_UPDATETOOLBOXICONS,0);
#endif//WIN32

#ifdef UNIX
  page_lv.sl.DeleteAll();
  page_lv.sl.Sort=eslNoSort;

  page_lv.sl.Add(T("Machine"),101+ICO16_ST,PAGE_MACHINE);
  page_lv.sl.Add(T("ST Video"),101+ICO16_DISPLAY,PAGE_STVIDEO);
  page_lv.sl.Add("TOS (ROM)",101+ICO16_CHIP,PAGE_TOS);
  page_lv.sl.Add(T("Keyboard/Mouse"),101+ICO16_OPTIONS,PAGE_INPUT);
  page_lv.sl.Add(T("Record Input"),101+ICO16_MACROS,PAGE_MACROS);
  page_lv.sl.Add(T("I/O Ports"),101+ICO16_PORTS,PAGE_PORTS);
  page_lv.sl.Add(T("General"),101+ICO16_TOOLS,PAGE_GENERAL);
  page_lv.sl.Add(T("Sound"),101+ICO16_SOUND,PAGE_SOUND);
  page_lv.sl.Add(T("Display"),101+ICO16_DISPLAY,PAGE_DISPLAY);
  page_lv.sl.Add(T("On Screen Display"),101+ICO16_OSD,PAGE_OSD);
  page_lv.sl.Add(T("Colour Control"),101+ICO16_BRIGHTCON,PAGE_COLOUR);
  //page_lv.sl.Add(T("Profiles"),101+ICO16_PROFILE,PAGE_CONFIG);
  page_lv.sl.Add(T("Configurations"),101+ICO16_PROFILE,PAGE_CONFIG); // was Profiles
  page_lv.sl.Add(T("Startup"),101+ICO16_FUJI16,6);
  page_lv.sl.Add(T("Paths"),101+ICO16_FUJI16,PAGE_PATHS);

#if 1//defined(SSE_GUI_OPTION_PAGE) && defined(SSE_UNIX)
  page_lv.sl.Add(T("Misc."),101+ICO16_SSE_OPTION,PAGE_MISC);
#endif
  
  page_lv.lpig=&Ico16;
  page_lv.display_mode=1;

  int lv_w=page_lv.get_max_width(XD);
  page_w=380;
  int w=lv_w+10+page_w+10;

  if (StandardShow(w,OPTIONS_HEIGHT,T("Settings"),
      ICO16_OPTIONS,ButtonPressMask,(LPWINDOWPROC)WinProc)) return;
  control_parent.create(XD,Handle,lv_w,0,page_w+20,OPTIONS_HEIGHT,
                    NULL,this,BT_STATIC,"",0,hxc::col_bk);
  page_p=control_parent.handle;
  page_l=10;

  page_lv.select_item_by_data(Page,1);
  page_lv.id=IDC_PAGETREE;
  page_lv.create(XD,Handle,0,0,lv_w,OPTIONS_HEIGHT,listview_notify_proc,this);

  CreatePage(Page);

  if (StemWin) OptBut.set_check(true);

  XMapWindow(XD,Handle);
  XFlush(XD);
#endif
}


void TOptionBox::Hide() {
  if(Handle==NULL) 
    return;
#ifdef WIN32
  ShowWindow(Handle,SW_HIDE);
  if(FullScreen) 
    SetFocus(StemWin);
  DestroyCurrentPage();
  DestroyWindow(Handle);Handle=NULL;
  ImageList_Destroy(il);il=NULL;
  if(StemWin) 
    PostMessage(StemWin,WM_USER,MSG_UPDATETOOLBOXICONS,0);
  ManageWindowClasses(SD_UNREGISTER);
#endif
#ifdef UNIX
  if(XD==NULL)
    return;
  hints.remove_all_children(page_p);
  StandardHide();
  if(StemWin)
    OptBut.set_check(0);
#endif
}


void TOptionBox::EnableBorderOptions(BOOL enable) {
#ifdef WIN32
  border=(enable) ? border_last_chosen : 0;
  CheckMenuRadioItem(StemWin_SysMenu,IDSYS_BORDEROFF,IDSYS_BORDERON,
    IDSYS_BORDEROFF+enable,MF_BYCOMMAND);
#endif
}


#ifdef SSE421R1

bool TOptionBox::NeedReset() { 
  // check is the reasons for reset (still) exist
  BYTE CurMemConf[2];
  GetCurrentMemConf(CurMemConf);
  int CurMonSel=GetCurrentMonitorSel();
  if(IsSameStr_I(NewROMFile,ROMFile))
    NewROMFile="";
  if(NewMemConf0==CurMemConf[0]&&NewMemConf1==CurMemConf[1])
    NewMemConf0=-1;
  if(NewMonitorSel==CurMonSel)
    NewMonitorSel=-1;
  if(NewStModel==ST_MODEL)
    NewStModel=-1;
  return (NewStModel>=0 || NewMemConf0>=0 || NewMonitorSel>=0 || NewROMFile.NotEmpty());
}

#endif

#ifdef WIN32

void TOptionBox::ChangeScreenShotFormat(int const NewFormat,Str Ext) {
  Disp.ScreenShotFormat=NewFormat;
  char *dot=strrchr(Ext,'.');
  if(dot) 
  {
    Ext=dot+1;
    dot=strrchr(Ext,')');
    if(dot) 
      *dot='\0';
  }
  Disp.ScreenShotExt=Ext.LowerCase();
#if !defined(SSE_NO_FREEIMAGE)
  Disp.ScreenShotFormatOpts=0;
  //Disp.FreeImageLoad();
  FillScreenShotFormatOptsCombo();
#endif
#ifndef SSE_LEAN_AND_MEAN
  if(Handle)
#endif
  if(HWND h=GetDlgItem(Handle,IDC_SCREENSHOT_FORMAT)) 
    CBSelectItemWithData(h,NewFormat);
}


#if !defined(SSE_NO_FREEIMAGE)

void TOptionBox::ChangeScreenShotFormatOpts(int const NewOpt) {
  Disp.ScreenShotFormatOpts=NewOpt;
  //Disp.FreeImageLoad();
#ifndef SSE_LEAN_AND_MEAN
  if(Handle) 
#endif
    if(HWND hwnd=GetDlgItem(Handle,IDC_FI_SCREENSHOT_FORMAT)) 
      CBSelectItemWithData(hwnd,NewOpt);
}

#endif


void TOptionBox::ChooseScreenShotFolder(HWND const Win) {
  EnableAllWindows(false,Win);
  EasyStr NewFol=ChooseFolder((FullScreen) ? StemWin : Win,T("Pick a Folder"),ScreenShotFol);
  if(NewFol.NotEmpty()) 
  {
    NO_SLASH(NewFol);
#ifndef SSE_LEAN_AND_MEAN
    if(Handle) 
#endif
      if(GetDlgItem(Handle,IDP_SCREENSHOTDIR)) 
        SendDlgItemMessage(Handle,IDP_SCREENSHOTDIR,WM_SETTEXT,0,(LPARAM)(NewFol.Text));
    ScreenShotFol=NewFol;
  }
  SetForegroundWindow(Win);
  EnableAllWindows(true,Win);
}


void TOptionBox::UpdateForNoSound() {
  if(IsVisible()&&Page==PAGE_SOUND)
  {
    DestroyCurrentPage(); // rough
    CreatePage(Page);
  }
}


void TOptionBox::ManageWindowClasses(bool Unreg) {
#if !defined(SSE_LIBRETRONUKE)
  char *ClassName="Steem Options";
  if(Unreg)
    UnregisterClass(ClassName,hInstance);
  else
    RegisterMainClass(WndProc,ClassName,RC_ICO_SETTINGS);
#endif
}


void TOptionBox::LoadIcons() {
  if(Handle==NULL) 
    return;
  HIMAGELIST old_il=il;
  if(BIG_ICONS)
    il=ImageList_Create(18+16,20+16,BPPToILC|ILC_MASK,10,10);
  else
    il=ImageList_Create(18,20,BPPToILC|ILC_MASK,10,10);
  if(il) 
  {
    ImageList_AddPaddedIcons(il,PAD_ALIGN_RIGHT,      
      hGUIIcon[RC_ICO_OPS_GENERAL], // PAGE_GENERAL = 0 etc. It must match!
      hGUIIcon[RC_ICO_OPS_DISPLAY],
      hGUIIcon[RC_ICO_OPS_BRIGHTCON],
      hGUIIcon[RC_ICO_OPS_FULLSCREEN],
      hGUIIcon[RC_ICO_OPS_MIDI],
      hGUIIcon[RC_ICO_OPS_SOUND],
      hGUIIcon[RC_ICO_OPS_STARTUP],
      hGUIIcon[RC_ICO_OPS_UPDATE],//unused
      hGUIIcon[RC_ICO_OPS_ASSOC],
      hGUIIcon[RC_ICO_OPS_MACHINE],
      hGUIIcon[RC_ICO_CHIP],//tos
      hGUIIcon[RC_ICO_CFG],
      hGUIIcon[RC_ICO_EXTERNAL],//ports
      hGUIIcon[RC_ICO_OPS_MACROS],
      hGUIIcon[RC_ICO_OPS_ICONS],
      hGUIIcon[RC_ICO_OPS_OSD],
#if !defined(SSE_GUI_421_REORDER)
      hGUIIcon[RC_ICO_OPS_SSE],//misc
#endif
      hGUIIcon[RC_ICO_OPS_KBDMOUSE],//input
#if !defined(SSE_GUI_421_REORDER)
      hGUIIcon[RC_ICO_OPS_DISPLAY], // st video
#endif
#if defined(SSE_GEM_CONTROL_PANEL)
      hGUIIcon[RC_ICO_CONTROLPANEL],
#endif
#if defined(SSE_GUI_QUICKSETTINGS)
      hGUIIcon[RC_ICO_OPS_SSE],
#endif
#if defined(SSE_GUI_EMUCONTROL)
      hGUIIcon[RC_ICO_OPS_STARTUP],
      hGUIIcon[RC_ICO_OPS_STARTUP],
#endif
#if defined(SSE_LOGSECTIONS_FOR_ALL)
      hGUIIcon[RC_ICO_CONTROLPANEL],
#endif
      0);
  }
  if(GetDlgItem(Handle,IDC_PAGETREE)) 
    SendMessage(PageTree,TVM_SETIMAGELIST,TVSIL_NORMAL,(LPARAM)il);
  if(old_il) 
    ImageList_Destroy(old_il);

  if(GetDlgItem(Handle,IDC_SOUNDRECORD)) 
    SendDlgItemMessage(Handle,IDC_SOUNDRECORD,BM_RELOADICON,0,0); // Record
  if(GetDlgItem(Handle,IDC_RECORDMACRO)) 
    SendDlgItemMessage(Handle,IDC_RECORDMACRO,BM_RELOADICON,0,0); // Record macro
  if(GetDlgItem(Handle,IDC_PLAYMACRO)) 
    SendDlgItemMessage(Handle,IDC_PLAYMACRO,BM_RELOADICON,0,0); // Play macro
  if(HWND hscr=Scroller.GetControlPage()) 
  {
    for(int i=IDC_ICONSBASE;i<IDC_ICONSBASE+RC_NUM_ICONS;i++) 
      if(GetDlgItem(hscr,i)) 
        SendDlgItemMessage(hscr,i,BM_RELOADICON,0,0);
  }
  UpdateDirectoryTreeIcons(&DTree);
  int w=page_w-GuiSM.mHorizontalSeparation*2;
  int h=OPTIONS_HEIGHT/4;
  CreateBrightnessBitmap(w,h);
}


void TOptionBox::DestroyCurrentPage() {
  ToolsDeleteAllChildren(ToolTip,Handle);
  // Stop profiles saving out all check states when close
  if(HWND hwnd=GetDlgItem(Handle,IDC_CONFIGLISTVIEW)) 
    EnableWindow(hwnd,FALSE);
  TStemDialog::DestroyCurrentPage();
  if(hBrightBmp) 
    DeleteObject(hBrightBmp);
  hBrightBmp=NULL;
}


bool TOptionBox::HasHandledMessage(MSG *mess) {
  if(mess->message==WM_KEYDOWN)
  {
#if defined(SSE_GUI_KBD)
    // ???
    // arrows for radio buttons -> optionbox
    // return, esc for label edit -> system
    if(mess->wParam!=VK_RETURN && mess->wParam!=VK_ESCAPE)
#else
    if(mess->wParam==VK_TAB)
#endif
      return !!IsDialogMessage(Handle,mess);
  }
  return false;
}


void TOptionBox::SetBorder(int newborder) {
#if defined(SSE_EMU_THREAD)
#ifdef SSE421R1
  bool oldSuspendRendering=SuspendRendering;
  if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
  {
    VideoLock.Lock();
    SuspendRendering=true;
    VideoLock.Unlock();
  }
#else
  bool oldSuspendRendering=SuspendRendering;
  SuspendRendering=true;
  if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
  {
    VideoLock.Lock();
    VideoLock.Unlock();
  }
#endif
#endif
  BYTE oldborder=border;
  //TRACE_LOG("Option Border %d->%d\n",oldborder,newborder);
  if(ChangeBorderModeRequest(newborder))
  {
    border=(BYTE)newborder;
    ChangeBorderSize(newborder);
    if(FullScreen) 
      change_fullscreen_display_mode(true);
    //change_window_size_for_border_change(oldborder,newborder);
    ///StemWinResize();
    if(runstate==RUNSTATE_STOPPED)
      draw(false);
    InvalidateRect(StemWin,NULL,FALSE);
#if defined(SSE_VID_DD)
#ifndef SSE_LEAN_AND_MEAN
    if(Handle)
#endif
      if(HWND hwnd=GetDlgItem(Handle,IDC_FS640X400))
        EnableWindow(hwnd,border==0&&draw_fs_blit_mode!=DFSM_FAKEFULLSCREEN);
#endif
  }
  else 
    border=oldborder;
  CheckMenuRadioItem(StemWin_SysMenu,110,112,110+MIN((int)border,1),MF_BYCOMMAND);
  CheckRadioButton(Handle,IDC_RADIO_BORDER,IDC_RADIO_BORDER+3,IDC_RADIO_BORDER+border);
#if defined(SSE_EMU_THREAD)
  SuspendRendering=oldSuspendRendering;
#endif
}


#if defined(SSE_GEM_CONTROL_PANEL)

// helper function for the control panel page
void TOptionBox::UpdateColour(int colour) {
  HWND hColour=GetDlgItem(Handle,colour+IDC_COLOUR);
  InvalidateRect(hColour,FALSE,TRUE);
  if(colour==CurrentColour) // should update sliders too
  {
    WORD dat=STpal[CurrentColour];
    dat=(WORD)(((dat&0x888)>>3)|((dat&0x777)<<1));  //fix up stupid rRRRgGGGbBBB colour pattern
    for(int i=0;i<3;i++) // update sliders
    {
      WORD nib=(dat>>(2-i)*4)&0xF;
      HWND hSlider=GetDlgItem(Handle,i+IDC_RGB);
      int max_v=(int)SendMessage(hSlider,TBM_GETRANGEMAX,0,0);
      if(max_v==7)
        nib>>=1;
      SendMessage(hSlider,TBM_SETPOS,1,max_v-nib);
      SendMessage(Handle,WM_VSCROLL,0,(LPARAM)hSlider); // force update
    }
  }
}

#endif

#if !defined(SSE_LIBRETRONUKE)
LRESULT CALLBACK TOptionBox::WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar) {
  if(DTree.ProcessMessage(Mess,wPar,lPar)) 
    return DTree.WndProcRet;
  LRESULT Ret=DefStemDialogProc(Win,Mess,wPar,lPar);
  if(StemDialog_RetDefVal) 
    return Ret;
  TOptionBox *This=(TOptionBox*)GetWindowLongPtr(Win,GWLP_USERDATA);
  WORD wpar_lo=LOWORD(wPar),wpar_hi=HIWORD(wPar);
  switch(Mess) {
  case WM_COMMAND:
#if defined(SSE_SHIFTER_421B)
    if(wpar_lo>=IDC_VID_DELAY&&wpar_lo<IDC_VID_DELAY+7)
    {
      if(wpar_hi==EN_CHANGE)
      {
        int i=wpar_lo-IDC_VID_DELAY+Shifter.DISPATCHER_CPU;
        char buf[12];
        SendMessage((HWND)lPar,WM_GETTEXT,5,(LPARAM)buf);
        Shifter.RenderingDelay[i]=(char)atoi(buf);
      }
    }
#endif
#if defined(SSE_LOGSECTIONS_FOR_ALL)
    if(wpar_lo>=IDC_LOGSECTION &&wpar_lo<IDC_LOGSECTION+99)
    {
      int n=wpar_lo-IDC_LOGSECTION;
      if(wpar_hi==BN_CLICKED) 
        logsection_enabled[n]=!logsection_enabled[n];
    }
#endif
    switch(wpar_lo) {
#if defined(SSE_LOGSECTIONS_FOR_ALL)
    case IDC_RADIOLOG0:
    case IDC_RADIOLOG1:
    case IDC_RADIOLOG2:
    case IDC_RADIOLOG3:
      if(wpar_hi==BN_CLICKED && SendMessage((HWND)lPar,BM_GETCHECK,0,0)==BST_CHECKED)
        SSEConfig.TraceFile=(BYTE)(wpar_lo-IDC_RADIOLOG0);
      break;
    case IDC_CLEAR_TRACE_ON_RESET:
      if(wpar_hi==BN_CLICKED)
      {
        debug_wipe_log_on_reset=!debug_wipe_log_on_reset;
        SendMessage((HWND)lPar,BM_SETCHECK,debug_wipe_log_on_reset,0);
      }
      break;
#endif
    case IDC_CPU_SPEED:
      if(wpar_hi==CBN_SELENDOK)
      {
        LRESULT cursel=SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
        DWORD ItemData=(DWORD)SendMessage((HWND)lPar,CB_GETITEMDATA,cursel,0);
        nSysCyclesPerSecond=MAX(MIN(ItemData,(DWORD)CPU_MAX_HERTZ),(DWORD)CpuNormalHz/TICKS8)*TICKS8;
        /*DWORD input=(DWORD)SendMessage((HWND)lPar,CB_GETITEMDATA,SendMessage((HWND)lPar,CB_GETCURSEL,0,0));
        nSysCyclesPerSecond=MAX(MIN((DWORD)SendMessage((HWND)lPar,
          CB_GETITEMDATA,SendMessage((HWND)lPar,CB_GETCURSEL,0,0),0),
          (DWORD)CPU_MAX_HERTZ),(DWORD)CpuNormalHz/TICKS8)*TICKS8;*/
#if defined(SSE_MEGASTE)
#if !defined(SSE_MEGA16) // we have a button now
        if(nSysCyclesPerSecond==16000000*TICKS8 && IS_MEGASTE)
        {
          if(!Cpu16.ScuReg) // force cache 16MHz instead
          {
            Cpu16.ScuReg=3;
            nSysCyclesPerSecond=CpuNormalHz;
          }
          else
            Cpu16.ScuReg=0;
        }
#endif
#endif
#ifndef SSE_NO_OSD
        if(runstate==RUNSTATE_RUNNING) 
          osd_init_run(false);
#endif
        AdaptCpuBoost();
      }
      break;

#if defined(SSE_OPTION_FREQ)
    case IDC_VID_FREQUENCY: // hack
      if(wpar_hi==CBN_SELENDOK)
      {
        LRESULT cursel=SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
        BYTE newfreq=(BYTE)SendMessage((HWND)lPar,CB_GETITEMDATA,cursel,0);
        if(newfreq)
        {
          switch(newfreq) {
          case PAL_HZ:
            if(Glue.ShiftMode&HIRES)
              Glue.SetShiftMode(LORES);
            Glue.SetSyncMode(TGlue::SYNCPAL);
            break;
          case NTSC_HZ:
            if(Glue.ShiftMode&HIRES)
              Glue.SetShiftMode(LORES);
            Glue.SetSyncMode(0);
            break;
          case MONO_HZ:
            Glue.SetShiftMode(HIRES);
            break;
          }//sw
          Glue.Update();
          Glue.PreviousVideoFreq=newfreq;
#if defined(SSE_VID_STVL1)
          Stvl.framefreq=newfreq;
          StvlUpdate();
#endif
#if defined(SSE_VID_D3D_VSYNC)
          Draw.MarshalParameters();
          Disp.ScreenChange(); // create new surfaces
#endif
          UPDATE_STATUS_BAR_PART(SB_PART_FREQ);
        }
      }
      break;
#endif//#if defined(SSE_OPTION_FREQ)

    case IDC_SYSKEYS:
      if(wpar_hi==BN_CLICKED) 
      {
        bAllowTaskSwitch=!bAllowTaskSwitch;
        SendMessage((HWND)lPar,BM_SETCHECK,!bAllowTaskSwitch,0);
      }
      break;
    case IDC_AUTOPAUSE:
      if(wpar_hi==BN_CLICKED) 
      {
        bPauseWhenInactive=!bPauseWhenInactive;
        SendMessage((HWND)lPar,BM_SETCHECK,bPauseWhenInactive,0);
      }
      break;
    case IDC_AUTOMUTE:
      if(wpar_hi==BN_CLICKED) 
      {
#if 1
        MuteWhenInactive=(MuteWhenInactive==0); // MuteWhenInactive is BYTE
#else
        MuteWhenInactive=!(MuteWhenInactive!=0); // MuteWhenInactive is BYTE
#endif
        SendMessage((HWND)lPar,BM_SETCHECK,MuteWhenInactive,0);
      }
      break;
    case IDC_STARTONCLICK:
      if(wpar_hi==BN_CLICKED) 
      {
        StartEmuOnClick=!StartEmuOnClick;
        SendMessage((HWND)lPar,BM_SETCHECK,StartEmuOnClick,0);
      }
      break;
    case IDC_FRAMESKIP:
      if(wpar_hi==CBN_SELENDOK) 
      {
        frameskip=(int)SendMessage((HWND)lPar,CB_GETCURSEL,0,0)+1;
        if(frameskip==5)
          frameskip=AUTO_FRAMESKIP;
      }
      break;
#if defined(SSE_VID_DD)
    case IDC_BLITMODE:
      if(wpar_hi==CBN_SELENDOK) 
      {
        draw_fs_blit_mode=(BYTE)SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
        This->UpdateFullscreen();
        if(draw_grille_black<4) 
          draw_grille_black=4;
      }
      break;
#endif
#if defined(SSE_VID_D3D)
    case IDC_FSSTRETCH: // Fullscreen page
      if(wpar_hi==BN_CLICKED)
      {
        draw_stretch_fs=!draw_stretch_fs;
        SendMessage((HWND)lPar,BM_SETCHECK,draw_stretch_fs,0);
      }
      break;
#endif
    case IDC_STRETCH:
      if(wpar_hi==BN_CLICKED)
      {
        draw_stretch=!draw_stretch;
        SendMessage((HWND)lPar,BM_SETCHECK,draw_stretch,0);
        {
#if defined(SSE_EMU_THREAD)
#ifdef SSE421R1 // suspended rendering -> won't draw when stopped
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
          draw_win_mode[screen_res]=!draw_stretch;
          Disp.ScreenChange(); // This changes Texture size
#ifndef SSE421R1
          StemWinResize();
#endif
          if(runstate==RUNSTATE_STOPPED)
            draw(false);
#if defined(SSE_EMU_THREAD)
          SuspendRendering=oldSuspendRendering;
          VideoLock.Unlock();
#endif
        }
      }
      break;
    case IDC_FSVSYNC:
      if(wpar_hi==BN_CLICKED) 
      {
        FSDoVsync=!FSDoVsync;
        SendMessage((HWND)lPar,BM_SETCHECK,FSDoVsync,0);
      }
      break;
#if defined(SSE_VID_D3D)
    case IDC_DESKTOPHZ:
      if(wpar_hi==BN_CLICKED) 
      {
        OPTION_FULLSCREEN_DEFAULT_HZ=!OPTION_FULLSCREEN_DEFAULT_HZ;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_FULLSCREEN_DEFAULT_HZ,0);
        //TRACE_LOG("Option FullScreenDefaultHz = %d\n",OPTION_FULLSCREEN_DEFAULT_HZ);
        if(FullScreen && D3D9_OK)
          Disp.ScreenChange();
      }
      break;
    case IDC_FAKEFULL:
      if(wpar_hi==BN_CLICKED) 
      {
        OPTION_FAKE_FULLSCREEN=!OPTION_FAKE_FULLSCREEN;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_FAKE_FULLSCREEN,0);
        //TRACE_LOG("Option FakeFullScreen = %d\n",OPTION_FAKE_FULLSCREEN);
        if(FullScreen&&D3D9_OK)
          Disp.ScreenChange();
        else
        {
          if(!OPTION_FAKE_FULLSCREEN && !SSEConfig.TrueFullScreenGui)
            OPTION_FULLSCREEN_GUI=0;
          This->DestroyCurrentPage(); // rough
          This->CreatePage(This->Page);
        }
      }
      break;
#endif
    case IDC_FULSCREEN_ON_MAX:
      if(wpar_hi==BN_CLICKED) 
      {
        OPTION_MAX_FS=!OPTION_MAX_FS;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_MAX_FS,0);
        //TRACE_LOG("Option FullScreen on Maximize = %d\n",OPTION_MAX_FS);
      }
      break;
    case IDC_TOGGLE_FULLSCREEN: //Go Windowed now, Go Fullscreen now
      if(wpar_hi==BN_CLICKED) 
      {
        if(FullScreen)
          Disp.ChangeToWindowedMode(StatusInfo.MessageIndex==TStatusInfo::BLIT_ERROR);
        else
          PostMessage(StemWin,WM_SYSCOMMAND,SC_MAXIMIZE,2);
      }
      break;
#if !defined(SSE_GUI_421_REORDER) // no more reset video?
    case IDC_RESET_DISPLAY:
      if(wpar_hi==BN_CLICKED) 
      {
        Disp.nUseMethod=0;
#if defined(SSE_EMU_THREAD)
        if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
          VideoLock.Lock();
#endif
        Disp.Init();
#if defined(SSE_EMU_THREAD)
        VideoLock.Unlock();
#endif
        Disp.ScreenChange(); // surfaces
      }
      break;
#endif
#if defined(SSE_VID_DD)
    case IDC_FSSTRETCHRES:
      if(wpar_hi==CBN_SELENDOK) 
      {
        INT_PTR i=SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
        if(i<NFSRES && Disp.fs_res[i].x)
        {
          Disp.fs_res_choice=(BYTE)i;
          //TRACE_LOG("fs customp %d:%dx%d\n",i,Disp.fs_res[i].x,Disp.fs_res[i].y);
        }
      }
      break;
    case IDC_FS640X400:
      if(wpar_hi==BN_CLICKED) 
      {
        prefer_res_640_400=!prefer_res_640_400;
        //This->DestroyCurrentPage(); // rough
        //This->CreatePage(This->Page);
        SendMessage((HWND)lPar,BM_SETCHECK,prefer_res_640_400,0);
      }
      break;
#endif

    case (IDC_RADIO_ST_MODEL+STE): // option ST model, not advanced
    case (IDC_RADIO_ST_MODEL+STF):
#if defined(SSE_MEGAST)
    case (IDC_RADIO_ST_MODEL+MEGA_ST):
#endif
    //case (IDC_RADIO_ST_MODEL+STFM): // no room!
#if defined(SSE_MEGASTE)
    case (IDC_RADIO_ST_MODEL+MEGA_STE):
#endif
    case IDC_CB_ST_MODEL: // option ST model, advanced
    {
      BYTE new_st_model=0xFF;
      if(wpar_hi==BN_CLICKED && SendMessage((HWND)lPar,BM_GETCHECK,0,0)==BST_CHECKED)
      {
        new_st_model=(BYTE)(wpar_lo-IDC_RADIO_ST_MODEL);
#if defined(SSE_GUI_QUICKSETTINGS)
        WORD newtos=This->TosList.GetCompatibleTos(new_st_model);
        OptionBox.NewROMFile=This->TosList.Path[newtos];
        CheckRadioButton(Win,IDC_TOS_VERSION,IDC_TOS_VERSION+TOS_LIST_LEN,IDC_TOS_VERSION+newtos);
#endif
      }
      else if(HIWORD(wPar)==CBN_SELENDOK)
        new_st_model=(BYTE)SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
      if(new_st_model!=0xFF)
      {
        //TRACE_LOG("Option ST model = %d\n",new_st_model);
#if defined(SSE_GUI_INSTANTCHANGE)
        if(SSEOptions.InstantMachineChange)
          SSEConfig.SwitchSTModel(new_st_model);
        else
          This->NewStModel=new_st_model;
#else
        SSEConfig.SwitchSTModel(new_st_model);
        // try to load corresponding config (not if changing live)
        if(OPTION_ST_PRESELECT && runstate==RUNSTATE_STOPPED)
        {
          Str Cfg=This->ProfileDir;
          Cfg+=SLASH;
          Cfg+=st_model_name[ST_MODEL];
          Cfg+=".";
          Cfg+=CONFIG_FILE_EXT;
          if(Exists(Cfg.Text))
            This->LoadProfile(Cfg.Text);
        }
#endif
      }
      This->MachineUpdateIfVisible(); //anyway
      UPDATE_STATUS_BAR_PART(SB_PART_MAIN);
      if(DiskMan.IsVisible())//TODO
      {
        HWND hGemdos=GetDlgItem(DiskMan.Handle,IDC_HDGEMDOS);
        SendMessage(hGemdos,BM_SETCHECK,!HardDiskMan.DisableHardDrives||HardDiskMan.IsVisible(),0);
#if defined(SSE_ACSI_MNGR)
        HWND hAcsi=GetDlgItem(DiskMan.Handle,IDC_HDACSI);
        SendMessage(hAcsi,BM_SETCHECK,SSEOptions.Acsi||AcsiHardDiskMan.IsVisible(),0);
#endif
      }
      break;
    }
#if defined(SSE_VID_SIZE4) // new simplified display size choice
    case (IDC_RADIO_DISPLAY_SIZE+1):
    case (IDC_RADIO_DISPLAY_SIZE+2):
    case (IDC_RADIO_DISPLAY_SIZE+3):
    case (IDC_RADIO_DISPLAY_SIZE+4):
      if(wpar_hi==BN_CLICKED && SendMessage((HWND)lPar,BM_GETCHECK,0,0)==BST_CHECKED)
      {
        BYTE wished_size=(BYTE)(wpar_lo-IDC_RADIO_DISPLAY_SIZE);
      //  if(wished_size!=DISPLAY_SIZE)
        {
#if defined(SSE_EMU_THREAD)
#ifdef SSE421R1
          bool oldSuspendRendering=SuspendRendering;
          if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
          {
            VideoLock.Lock();
            SuspendRendering=true;
          }
#else
          if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
            VideoLock.Lock();
          bool oldSuspendRendering=SuspendRendering;
          SuspendRendering=true;
#endif
#endif
          switch(wished_size) {
          case 1:
            WinSizeForRes[HIRES]=WinSizeForRes[MEDRES]=WinSizeForRes[LORES]=0;
            draw_win_mode[MEDRES]=draw_win_mode[LORES]=draw_win_mode[HIRES]=0;
            break;
          case 2:
            WinSizeForRes[MEDRES]=WinSizeForRes[LORES]=WinSizeForRes[HIRES]=1;
            draw_win_mode[HIRES]=1;
            draw_win_mode[MEDRES]=draw_win_mode[LORES]=!draw_stretch;
            break;
          case 3:
            WinSizeForRes[MEDRES]=WinSizeForRes[LORES]=WinSizeForRes[HIRES]=2;
            //draw_win_mode[HIRES]=0;//draw_win_mode[MEDRES]=0;
            //draw_win_mode[LORES]=draw_win_mode[MEDRES]=1;
            draw_win_mode[HIRES]=draw_win_mode[MEDRES]
              =draw_win_mode[LORES]=!draw_stretch;
            break;
          case 4:
            WinSizeForRes[MEDRES]=WinSizeForRes[LORES]=WinSizeForRes[HIRES]=3;
#if defined(SSE_VID_SIZE4)
            draw_win_mode[HIRES]=draw_win_mode[MEDRES]
              =draw_win_mode[LORES]=!draw_stretch;
            //draw_win_mode[HIRES]=draw_win_mode[MEDRES]=draw_win_mode[LORES]=1;
#else
            draw_win_mode[HIRES]=draw_win_mode[MEDRES]=draw_win_mode[LORES]=0;
#endif
            break;
          }
          DISPLAY_SIZE=wished_size;
          //TRACE_LOG("Display size %d\n",wished_size);
          SSEConfig.Size4=(DISPLAY_SIZE==4);
          StemWinResize(); // fullscreen?
          if(draw_grille_black<4)
            draw_grille_black=4;
          if(runstate==RUNSTATE_STOPPED)
            draw(false);
#if defined(SSE_EMU_THREAD)
          VideoLock.Unlock();
          SuspendRendering=oldSuspendRendering;
#endif
        }
      }
      break;
#endif
    case IDC_GLU_WAKEUP0:
      if(wpar_hi==EN_CHANGE)
      {
        char buf[12];
        SendMessage((HWND)lPar,WM_GETTEXT,3,(LPARAM)buf);
        // order of OPTION_WS is different for historical reasons
        int wished_ws=atoi(buf);
        switch(wished_ws) {
        case 1: OPTION_WS=4; break;
        case 2: OPTION_WS=1; break;
        case 3: OPTION_WS=3; break;
        case 4: OPTION_WS=2; break;
        }
        //TRACE_LOG("Option WS = %d\n",OPTION_WS);
        Shifter.Preload=0; // reset the thing!
        Glue.Update();
      }
      break;

    case IDC_FONTSIZE0:
      if(wpar_hi==EN_CHANGE)
      {
        char buf[12];
        SendMessage((HWND)lPar,WM_GETTEXT,3,(LPARAM)buf);
        FONT_SIZE=(BYTE)atoi(buf);
        //TRACE_LOG("FontSize = %d\n",FONT_SIZE);
      }
      break;
#if defined(SSE_GUI_BIGICONS)
    case IDC_BIGGUI:
      if(wpar_hi==BN_CLICKED) 
      {
        BIG_ICONS=!BIG_ICONS;
        SendMessage((HWND)lPar,BM_SETCHECK,BIG_ICONS,0);
        // set default font size, player can change before leaving
        FONT_SIZE=BIG_ICONS?GUI_BIGFONT_SIZE:GUI_SMALLFONT_SIZE;
        SendMessageW(GetDlgItem(This->Handle,IDC_FONTSIZE1),UDM_SETPOS32,0,FONT_SIZE);
        //TRACE_LOG("BigIcons = %d\n",BIG_ICONS);
        //TConfigStoreFile CSF(globalINIFile);
        //LoadAllIcons(&CSF,false); // OK but we don't resize Steem windows + font
      }
      break;
#endif
    case IDC_F12RUN:
      if(wpar_hi==BN_CLICKED) 
      {
        SSEOptions.F12Run=!SSEOptions.F12Run;
        SendMessage((HWND)lPar,BM_SETCHECK,SSEOptions.F12Run,0);
      }
      break;
    case IDC_PAUSERUN:
      if(wpar_hi==BN_CLICKED) 
      {
        SSEOptions.PauseRun=!SSEOptions.PauseRun;
        SendMessage((HWND)lPar,BM_SETCHECK,SSEOptions.PauseRun,0);
      }
      break;
    case IDC_GREYSCREEN:
    case IDC_GREENSCREEN:
    case IDC_FULLSPECTRUM:
      if(wpar_hi==BN_CLICKED) 
      {
        if(wpar_lo==IDC_GREYSCREEN)
        {
          OPTION_GREYSCREEN=!OPTION_GREYSCREEN;
          SendMessage((HWND)lPar,BM_SETCHECK,OPTION_GREYSCREEN,0);
          //TRACE_LOG("Black & White = %d\n",OPTION_GREYSCREEN);
        }
        else if(wpar_lo==IDC_GREENSCREEN)
        {
          OPTION_GREENSCREEN=!OPTION_GREENSCREEN;
          SendMessage((HWND)lPar,BM_SETCHECK,OPTION_GREENSCREEN,0);
          //TRACE_LOG("Green screen = %d\n",OPTION_GREENSCREEN);
        }
        else
        {
          SSEOptions.FullSpectrumPal=!SSEOptions.FullSpectrumPal;
          SendMessage((HWND)lPar,BM_SETCHECK,SSEOptions.FullSpectrumPal,0);
          //TRACE_LOG("FullSpectrumPal colours = %d\n",OPTION_VIVID);
        }
        make_palette_table(col_brightness,col_contrast);
        if(!flashlight_flag) 
          palette_convert_all();
        This->DrawBrightnessBitmap(This->hBrightBmp);
        InvalidateRect(GetDlgItem(Win,ID_BRIGHTNESS_MAP),NULL,TRUE);
        if(runstate==RUNSTATE_STOPPED)
          draw(false);
      }
      break;
#if defined(SSE_VID_D3D)
    case IDC_TEXTUREFILTER:
      if(wpar_hi==CBN_SELENDOK)
      {
        LRESULT cursel=SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
        Disp.TextureFilter=(int)SendMessage((HWND)lPar,CB_GETITEMDATA,(WPARAM)cursel,0);
        if(runstate==RUNSTATE_STOPPED)
          draw(false);
        //TRACE_LOG("TextureFilter = %d\n",Disp.TextureFilter);
      }
      break;
#endif
#if defined(SSE_GUI_TOOLBAR)
    case IDC_TOOLBAR:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_TOOLBAR=!OPTION_TOOLBAR;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_TOOLBAR,0);
        CheckMenuItem(StemWin_SysMenu,IDSYS_TOOLBAR,MF_BYCOMMAND|MF_CHECK(OPTION_TOOLBAR));
        //TRACE_LOG("ToolBar = %d\n",OPTION_TOOLBAR);
#if defined(SSE_EMU_THREAD)
        if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
          VideoLock.Lock();
#endif
        GuiSM.Update();
        StemWinResize();
#if defined(SSE_EMU_THREAD)
        VideoLock.Unlock();
#endif
      }
      break;
#endif
#if defined(SSE_GUI_MENUBAR)
    case IDC_MENUBAR:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_MENUBAR=!OPTION_MENUBAR;
        //TRACE_LOG("MenuBar = %d\n",OPTION_MENUBAR);
        SteemSetMenu(OPTION_MENUBAR);
      }
      break;
#endif
#if defined(SSE_VID_DD)
    case (IDP_FSPREFERREDHZ+0):
    case (IDP_FSPREFERREDHZ+2):
    case (IDP_FSPREFERREDHZ+4):
    case (IDP_FSPREFERREDHZ+6):
      if(wpar_hi==CBN_SELENDOK) 
      {
        int i=(wpar_lo-IDP_FSPREFERREDHZ)/2;
        int new_hz=HzIdxToHz[SendMessage((HWND)lPar,CB_GETCURSEL,0,0)];
        if(prefer_pc_hz[i]!=new_hz) 
        {
          prefer_pc_hz[i]=new_hz;
          int current_i=(border) ? 2 : 1;
          if(FullScreen && current_i==i) 
          {
            if(IDYES==Alert(T("Do you want to test this video frequency now?"),
              T("Change Monitor Frequency"),MB_YESNO|MB_DEFBUTTON1
              |MB_ICONQUESTION)) {
              change_fullscreen_display_mode(false);
              palette_convert_all();
              if(runstate==RUNSTATE_STOPPED)
                draw(false);
              InvalidateRect(StemWin,NULL,FALSE);
            }
          }
          This->UpdateFullscreen();
        }
      }
      break;
#endif
    case IDC_CONFIRM_QUIT:
      if(wpar_hi==BN_CLICKED) 
      {
        FSQuitAskFirst=!FSQuitAskFirst;
        SendMessage((HWND)lPar,BM_SETCHECK,FSQuitAskFirst,0);
      }
      break;
    case IDC_AUTORESIZE:
      if(wpar_hi==BN_CLICKED) 
      {
        ResChangeResize=!ResChangeResize;
        SendMessage((HWND)lPar,BM_SETCHECK,ResChangeResize,0);
        if(ResChangeResize)
          StemWinResize();
      }
      break;
    case IDC_SIZELORES:case IDC_SIZEMEDRES:case IDC_SIZEHIRES:   // Window Size Low Medium High
      if(wpar_hi==CBN_SELENDOK) 
      {
#if defined(SSE_EMU_THREAD)
        if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
          VideoLock.Lock();
#endif
        BYTE Res=(BYTE)(wpar_lo-IDC_SIZELORES)>>1; // 0 1 2 for ST's resolutions
        bool redraw=false;
        DWORD dat=(DWORD)CBGetSelectedItemData((HWND)lPar);
        WinSizeForRes[Res]=(BYTE)LOWORD(dat);
#if !defined(SSE_VID_SIZE4)
        if(Res<2)
#endif
        {
          if(draw_win_mode[Res]!=HIWORD(dat)) 
          {
            draw_win_mode[Res]=HIWORD(dat); // 1 = crisp
            redraw=true;
          }
        }
        //TRACE_LOG("Window size Res%d size %d crisp %d\n",Res,WinSizeForRes[Res],draw_win_mode[Res]);
        if(Res==(int)(video_mixed_output ? MEDRES : screen_res)) // changing for the current res
        {
          DISPLAY_SIZE=WinSizeForRes[Res]+1;
          Disp.ScreenChange(); // This changes Texture size
#if defined(SSE_VID_SIZE4)
          if(ResChangeResize || SSEConfig.Size4)
            StemWinResize();
#endif
          if(redraw && FullScreen==0) 
          {
            if(draw_grille_black<4)
              draw_grille_black=4;
            if(runstate==RUNSTATE_STOPPED)
              draw(false);
          }
#if !defined(SSE_VID_SIZE4)
          if(ResChangeResize) 
            StemWinResize();
#endif
        }
#if defined(SSE_EMU_THREAD)
        VideoLock.Unlock();
#endif
      }
      break;
    case IDC_SHOWTIPS:
      if(wpar_hi==BN_CLICKED) 
      {
        ShowTips=!ShowTips;
        SendMessage((HWND)lPar,BM_SETCHECK,ShowTips,0);
        SendMessage(ToolTip,TTM_ACTIVATE,ShowTips,0);
      }
      break;
    case IDC_SCREENSHOTCHOOSEDIR:
      if(wpar_hi==BN_CLICKED) 
      {
        SendMessage((HWND)lPar,BM_SETCHECK,1,0);
        This->ChooseScreenShotFolder(Win);
        SetFocus((HWND)lPar);
        SendMessage((HWND)lPar,BM_SETCHECK,0,0);
      }
      break;
    case IDC_SCREENSHOTOPENDIR:
      if(wpar_hi==BN_CLICKED) 
        ShellExecute(NULL,NULL,ScreenShotFol,"","",SW_SHOWNORMAL);
      break;
    case IDC_MINSCREENSHOT:
      if(wpar_hi==BN_CLICKED) 
      {
        Disp.ScreenShotMinSize=!Disp.ScreenShotMinSize;
        SendMessage((HWND)lPar,BM_SETCHECK,Disp.ScreenShotMinSize,0);
      }
      break;
/*  Reset colour controls.
    We set the sliders to centre and send a message for each.
    TBM_SETPOSNOTIFY would do it in one line but it's only for Windows 7 and up.
*/
    case IDC_RESETCOLOURS:
      for(int i=0;i<5;i++)
      {
        HWND handle=GetDlgItem(This->Handle,IDC_BRIGHTNESS+i);
        //SendMessage(handle,TBM_SETPOS,1,128);
        SendMessage(handle,TBM_SETPOS,1,0);
        SendMessage(This->Handle,WM_HSCROLL,0,(LPARAM)handle);
      }
      // seems intuitive to also reset those
      if(!SSEOptions.FullSpectrumPal) // full spectrum should default
        PostMessage(Win,WM_COMMAND,MAKELONG(IDC_FULLSPECTRUM,BN_CLICKED),
          (LPARAM)GetDlgItem(Win,IDC_FULLSPECTRUM));
      if(OPTION_GREYSCREEN)
        PostMessage(Win,WM_COMMAND,MAKELONG(IDC_GREYSCREEN,BN_CLICKED),
          (LPARAM)GetDlgItem(Win,IDC_GREYSCREEN));
      if(OPTION_GREENSCREEN)
        PostMessage(Win,WM_COMMAND,MAKELONG(IDC_GREENSCREEN,BN_CLICKED),
          (LPARAM)GetDlgItem(Win,IDC_GREENSCREEN));
      break;
    case IDC_HACKS:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_HACKS=!OPTION_HACKS;
        //TRACE_LOG("Option Hacks %d\n",OPTION_HACKS);
        This->DestroyCurrentPage(); // rough
        This->CreatePage(This->Page);
        //SendMessage((HWND)lPar,BM_SETCHECK,OPTION_HACKS,0);
      }
      break;
#if defined(SSE_HD6301_LL) && !defined(SSE_NO_OLD_IKBD) // Option 6301 emu
    case IDC_6301:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_C1=!OPTION_C1;
        if(!HD6301_OK)
          OPTION_C1=0;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_C1,0);
        //TRACE_LOG("Option HD6301 emu: %d\n",OPTION_C1);
      }
      break;
#endif
    case IDC_EMUDETECT:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_EMU_DETECT=!OPTION_EMU_DETECT;
        //TRACE_LOG("%s %s: %d\n","Emu","detect",OPTION_EMU_DETECT);
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_EMU_DETECT,0);
        emudetect_reset();
      }
      break;
    case IDC_STASPECTRATIO:
      if(wpar_hi==BN_CLICKED)
      {
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
        OPTION_ST_ASPECT_RATIO=!OPTION_ST_ASPECT_RATIO;
        //TRACE_LOG("ST Aspect Ratio: %d\n",OPTION_ST_ASPECT_RATIO);
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_ST_ASPECT_RATIO,0);
        StemWinResize();
        if(runstate==RUNSTATE_STOPPED)
          draw(false);
#if defined(SSE_EMU_THREAD)
        SuspendRendering=oldSuspendRendering;
        VideoLock.Unlock();
#endif
      }
      break;
    case IDC_SCANLINES: // option scanlines
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_SCANLINES=!OPTION_SCANLINES;
        //TRACE_LOG("Scanlines: %d\n",OPTION_SCANLINES);
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_SCANLINES,0);
        Disp.ScreenChange(); // create new surfaces
        if(runstate==RUNSTATE_STOPPED)
          draw(false);
      }
      break;
    case IDC_VSYNC:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_WIN_VSYNC=!OPTION_WIN_VSYNC;
        //TRACE_LOG("Option Window VSync: %d\n",OPTION_WIN_VSYNC);
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_WIN_VSYNC,0);
#if defined(SSE_VID_D3D)
        Disp.ScreenChange(); // create new surfaces
#endif
        UPDATE_STATUS_BAR_PART(SB_PART_FREQ); // show change(s) in status bar
      }
      break;
#if defined(SSE_VID_D3D_VSYNC)
    case IDC_AUTOVSYNC:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_AUTOVSYNC=!OPTION_AUTOVSYNC;
        //TRACE_LOG("Option Auto VSync: %d\n",OPTION_AUTOVSYNC);
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_AUTOVSYNC,0);
#if defined(SSE_VID_D3D)
        Disp.ScreenChange(); // create new surfaces
#endif
        UPDATE_STATUS_BAR_PART(SB_PART_FREQ); // show change(s) in status bar
      }
      break;

    case IDC_AUTOVSYNC_FS:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_AUTOVSYNC_FS=!OPTION_AUTOVSYNC_FS;
        //TRACE_LOG("Option Auto VSync FullScreen: %d\n",OPTION_AUTOVSYNC_FS);
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_AUTOVSYNC_FS,0);
#if defined(SSE_VID_D3D)
        Disp.ScreenChange(); // create new surfaces
#endif
      }
      break;
#endif//SSE_VID_D3D_VSYNC
    case IDC_TRIPLE_BUFFERING:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_3BUFFER_FS=!OPTION_3BUFFER_FS;
        //TRACE_LOG("Option Triple Buffer FS: %d\n",OPTION_3BUFFER_FS);
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_3BUFFER_FS,0);
        Disp.ScreenChange(); // must delete and create surface
      }
      break;
#if defined(SSE_VID_DD_3BUFFER_WIN)
    case IDC_TRIPLE_BUFFERING_WIN:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_3BUFFER_WIN=!OPTION_3BUFFER_WIN;
        //TRACE_LOG("Option Triple Buffer Win: %d\n",OPTION_3BUFFER_WIN);
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_3BUFFER_WIN,0);
        Disp.ScreenChange(); // must delete and create surface
      }
      break;
#endif
#if defined(SSE_VID_D3D_SWEETFX)
    case IDC_CTR_EMU:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_CRT_EMU=!OPTION_CRT_EMU;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_CRT_EMU,0);
        Disp.ScreenChange(); // recreate surfaces, triggering init
        if(runstate==RUNSTATE_STOPPED)
          draw(false);
      }
      break;
#endif
    case IDC_VMMOUSE:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_VMMOUSE=!OPTION_VMMOUSE;
        //TRACE_LOG("Option VMMouse: %d\n",OPTION_VMMOUSE);
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_VMMOUSE,0);
      }
      break;
#if defined(SSE_OSD_SHOW_TIME)
    case IDC_SHOWTIME:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_OSD_TIME=!OPTION_OSD_TIME;
        OsdControl.StartingTime=timeGetTime(); //reset
        OsdControl.StoppingTime=0; //reset
        //TRACE_LOG("Option OsdTime: %d\n",OPTION_OSD_TIME);
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_OSD_TIME,0);
      }
      break;
#endif
#if defined(SSE_OSD_DEBUGINFO)
    case IDC_OSD_DEBUGINFO:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_OSD_DEBUGINFO=!OPTION_OSD_DEBUGINFO;
        //TRACE_LOG("OPTION_OSD_DEBUGINFO: %d\n",OPTION_OSD_DEBUGINFO);
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_OSD_DEBUGINFO,0);
      }
      break;
#endif
    case IDC_RESETBACKUP:
      if(wpar_hi==BN_CLICKED)
      {
        SSEOptions.ResetBackup=!SSEOptions.ResetBackup;
        SendMessage((HWND)lPar,BM_SETCHECK,SSEOptions.ResetBackup,0);
      }
      break;
#if defined(SSE_VID_LS) && defined(SSE_DEFAULTS_421)
    case IDC_SNAPSHOTPIC:
      if(wpar_hi==BN_CLICKED)
      {
        SSEOptions.ScreenshotWithSnapshot=!SSEOptions.ScreenshotWithSnapshot;
        SendMessage((HWND)lPar,BM_SETCHECK,SSEOptions.ScreenshotWithSnapshot,0);
      }
      break;
#endif
#if defined(SSE_GUI_INSTANTCHANGE)
    case IDC_INSTANTMACHINECHANGE:
      if(wpar_hi==BN_CLICKED)
      {
        SSEOptions.InstantMachineChange=!SSEOptions.InstantMachineChange;
        SendMessage((HWND)lPar,BM_SETCHECK,SSEOptions.InstantMachineChange,0);
      }
      break;
#endif
#if defined(SSE_VID_SINGLEPIX)
    case IDC_SINGLEPIXELS:
      if(wpar_hi==BN_CLICKED)
      {
        SSEOptions.SinglePixels=!SSEOptions.SinglePixels;
        //Draw.MarshalParameters();
        SendMessage((HWND)lPar,BM_SETCHECK,SSEOptions.SinglePixels,0);
        Disp.ScreenChange(); // create new surfaces
        if(runstate==RUNSTATE_STOPPED)
          draw(false);
      }
      break;
#endif
#if defined(SSE_OSD_FPS_INFO)
    case IDC_OSD_FPSINFO:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_OSD_FPSINFO=!OPTION_OSD_FPSINFO;
        //TRACE_LOG("OPTION_OSD_FPSINFO: %d\n",OPTION_OSD_FPSINFO);
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_OSD_FPSINFO,0);
      }
      break;
#endif
    case IDC_ADVANCED_SETTINGS: // toggle Advanced Settings
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_ADVANCED=!OPTION_ADVANCED;
        //TRACE_LOG("Option Advanced Settings: %d\n",OPTION_ADVANCED);
#if !defined(SSE_420R5) // breaks more than it fixes...
        if(!OPTION_ADVANCED)
        {
          SSEOptions.Restore();
          DISPLAY_SIZE=WinSizeForRes[screen_res]+1; // based on current res
        }
#endif
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_ADVANCED,0);
        This->Hide(); This->Show(); // only if changing tree or current page options
      }
      break;
    case IDC_ADVANCED_RESET: // reset Advanced Settings - so it's player's choice
      if(wpar_hi==BN_CLICKED)
      {
        SSEOptions.Restore();
#if defined(SSE_GUI_DRAG_PAGES)
        This->Hide(); // ultra rough but acceptable
        This->Show();
#else
        This->DestroyCurrentPage(); // rough
        This->CreatePage(This->Page);
#endif
      }
      break;

#if defined(SSE_LOGSECTIONS_FOR_ALL)
    case IDC_CLEAR_TRACE: // wipe log
      if(wpar_hi==BN_CLICKED)
        logfile_wipe();
      break;
#endif

#if defined(SSE_GUI_QUICKSETTINGS)
    case IDC_PREVDISC:
      DiskMan.ChangeDisk(0,-1,FALSE);
      break;
    case IDC_FILESELEC:
      if(wpar_hi==BN_CLICKED)
      {
        char* fstypes=FSTypes(2,NULL);
        EasyStr path=FileSelect(NULL,T("Select Disk Image"),DiskMan.DisksFol,fstypes,1,true,"");
        free(fstypes);
        EasyStr name=GetFileNameFromPath(path);
        DiskMan.InsertDisk(0,name,path,false,false,"",true); // can be ""
      }
      break;
    case IDC_NEXTDISC:
      DiskMan.ChangeDisk(0,1,FALSE);
      break;
    case IDC_HDGEMDOS:
    case IDC_HDACSI:
      if(wpar_hi==BN_CLICKED)
      {
        BOOL new_option;
        if(wpar_lo==IDC_HDACSI)
          new_option=SSEOptions.Acsi=!SSEOptions.Acsi;
        else
          new_option=!(HardDiskMan.DisableHardDrives=!HardDiskMan.DisableHardDrives);
        SendMessage((HWND)lPar,BM_SETCHECK,new_option,0);
        if(DiskMan.IsVisible())
        {
          HWND icon_handle=GetDlgItem(DiskMan.Handle,wpar_lo);
          SendMessage(icon_handle,BM_SETCHECK,new_option,0);
        }
      }
      break;
#endif
    case IDC_SCREENSHOT_FORMAT:
      if(wpar_hi==CBN_SELENDOK) 
      {
        Str Ext;
        Ext.SetLength(200);
        SendMessage((HWND)lPar,CB_GETLBTEXT,SendMessage((HWND)lPar,CB_GETCURSEL,0,0),(LPARAM)Ext.Text);
        This->ChangeScreenShotFormat((int)CBGetSelectedItemData((HWND)lPar),Ext);
      }
      break;
#if !defined(SSE_NO_FREEIMAGE)
    case IDC_FI_SCREENSHOT_FORMAT:
      if(wpar_hi==CBN_SELENDOK)
        This->ChangeScreenShotFormatOpts((int)CBGetSelectedItemData((HWND)lPar));
      break;
#endif
    case IDC_HIGHPRIORITY:
      if(wpar_hi==BN_CLICKED) 
      {
        HighPriority=!HighPriority;
        SendMessage((HWND)lPar,BM_SETCHECK,HighPriority,0);
        if(runstate==RUNSTATE_RUNNING)
          SetPriorityClass(GetCurrentProcess(),
          (HighPriority ? HIGH_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS));
      }
      break;
#if defined(SSE_EMU_THREAD)
    case IDC_EMU_THREAD:
      if(wpar_hi==BN_CLICKED) 
      {
        if(runstate==RUNSTATE_STOPPED)
          OPTION_EMUTHREAD=!OPTION_EMUTHREAD;
        else // should be disabled when running
          EnableWindow((HWND)lPar,FALSE);
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_EMUTHREAD,0);
        //TRACE_LOG("Option Emu Thread: %d\n",OPTION_EMUTHREAD);
      }
      break;
#endif
    case IDC_UNSTABLE_SHIFTER:
      if(wpar_hi==BN_CLICKED) 
      {
        OPTION_UNSTABLE_SHIFTER=!OPTION_UNSTABLE_SHIFTER;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_UNSTABLE_SHIFTER,0);
        //TRACE_LOG("UnstableShifter: %d\n",OPTION_UNSTABLE_SHIFTER);
      }
      break;
    case IDC_YM2149_ON:
      if(wpar_hi==BN_CLICKED) 
      {
        SSEConfig.YmSoundOn=!SSEConfig.YmSoundOn;
        SendMessage((HWND)lPar,BM_SETCHECK,SSEConfig.YmSoundOn,0);
      }
      break;
    case IDC_STESOUND_ON:
      if(wpar_hi==BN_CLICKED) 
      {
        SSEConfig.SteSoundOn=!SSEConfig.SteSoundOn;
        SendMessage((HWND)lPar,BM_SETCHECK,SSEConfig.SteSoundOn,0);
      }
      break;
    case IDC_RANDOM_WU:
      if(wpar_hi==BN_CLICKED) 
      {
        OPTION_RANDOM_WU=!OPTION_RANDOM_WU;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_RANDOM_WU,0);
      }
      break;
    case IDC_OSD_NONEONSTOP:
      if(wpar_hi==BN_CLICKED) 
      {
        OPTION_NO_OSD_ON_STOP=!OPTION_NO_OSD_ON_STOP;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_NO_OSD_ON_STOP,0);
      }
      break;
#if 0 && defined(SSE_GUI_EMUCONTROL)
    case IDC_MFPWSTMG:
      if(HIWORD(wPar)==CBN_SELENDOK)
      {
        SSEOptions.MfpWsTmg=(char)SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
      }
      break;
#endif
    case IDC_SOUNDDEVICE:
      if(wpar_hi==CBN_SELENDOK) 
      {
        EasyStr DriverName;
        BYTE CurSel=SSEOptions.AudioInterface=(BYTE)SendDlgItemMessage(Win,wpar_lo,CB_GETCURSEL,0,0);
        if(CurSel==0)
          DriverName="None";
        else if(CurSel==1)
          DriverName="";
        else
        {
#if defined(SSE_420R5)
          int l=(int)SendDlgItemMessage(Win,wpar_lo,CB_GETLBTEXTLEN,CurSel,0);
#else
          INT_PTR l=SendDlgItemMessage(Win,wpar_lo,CB_GETLBTEXTLEN,CurSel,0);
#endif
          DriverName.SetLength(l);
          SendDlgItemMessage(Win,wpar_lo,CB_GETLBTEXT,CurSel,(LPARAM)DriverName.Text);
        }
        WriteCSFStr("Options","DSDriverName",DriverName,globalINIFile);
        InitSound();
        This->UpdateForNoSound();
      }
      break;
    case IDC_RESTORESTATE:
      if(wpar_hi==BN_CLICKED) 
      {
        AutoLoadSnapShot=!AutoLoadSnapShot;
        SendMessage((HWND)lPar,BM_SETCHECK,AutoLoadSnapShot,0);
      }
      break;
#if !defined(SSE_NO_AUTOSNAPSHOTEDIT)
    case IDC_SNAPSHOTNAME:
      if(wpar_hi==EN_UPDATE) 
      {
        EasyStr NewName;
        int Len=(int)SendMessage((HWND)lPar,WM_GETTEXTLENGTH,0,0)+1;
        AutoSnapShotName.SetLength(Len);
        SendMessage((HWND)lPar,WM_GETTEXT,Len,(LPARAM)AutoSnapShotName.Text);
        bool SetText=false;
        for(int i=0;i<AutoSnapShotName.Length();i++) 
        {
          switch(AutoSnapShotName[i]) {
          case ':':case '/':
          case '"':case '<':
          case '>':case '|':
          case '*':case '?':
          case '\\':
            //AutoSnapShotName[i]='-';
            AutoSnapShotName[i]='_';
            SetText=true;
            break;
          }
        }
        if(SetText) 
        {
          DWORD Start,End;
          SendMessage((HWND)lPar,EM_GETSEL,WPARAM(&Start),(LPARAM)&End);
          SendMessage((HWND)lPar,WM_SETTEXT,0,(LPARAM)AutoSnapShotName.Text);
          SendMessage((HWND)lPar,EM_SETSEL,Start,End);
        }
      }
      break;
#endif//#if !defined(SSE_NO_AUTOSNAPSHOTEDIT)
    case IDC_GDI:
    case IDC_NODSOUND:
    case IDC_STARTFULL:
    case IDC_DRAWBUFFER:
    case IDC_BLITHIDEM:
    case IDC_TRACEFILE:
    case IDC_TRACESHOWPATH:
    case IDC_STARTRUN:
#if defined(SSE_GUI_RESTOREPOS)
    case IDC_RESTOREWINPOS:
#endif
      if(wpar_hi==BN_CLICKED) 
      {
        bool checked=!!SendMessage((HWND)lPar,BM_GETCHECK,0,0);
        char *key=NULL; //W4
        switch(wpar_lo) {
        case IDC_GDI:
          key="NoDirectDraw";
          EnableWindow(GetDlgItem(Win,IDC_STARTFULL),!checked);
          EnableWindow(GetDlgItem(Win,IDC_DRAWBUFFER),!checked);
          EnableWindow(GetDlgItem(Win,IDC_BLITHIDEM),!checked);
          break;
#if !defined(SSE_SOUND_NO_NOSOUND_OPTION)
        case IDC_NODSOUND:
          key="NoDirectSound";
          break;
#endif
        case IDC_STARTFULL:
          key="StartFullscreen";
          break;
        case IDC_DRAWBUFFER:
          key="DrawToVidMem";
          Disp.DrawToVidMem=checked; // // immediate effect (option moved)
          //TRACE_LOG("DrawToVidMem %d\n",Disp.DrawToVidMem);
          Disp.ScreenChange(); // -> create surfaces
          break;
        case IDC_BLITHIDEM:
          key="BlitHideMouse";
          Disp.BlitHideMouse=checked; // immediate effect (option moved)
          break;
#if defined(SSE_DEBUG_TRACE)
#if !defined(SSE_FORCE_TRACE_VIDEO_RENDERING)
        case IDC_TRACEFILE:
          key="TraceFile";
          SSEConfig.TraceFile=checked; // immediate effect!
          Debug.TraceStartStop(SSEConfig.TraceFile);
          break;
#endif
        case IDC_TRACESHOWPATH:
          key="TraceShowPath";
          SSEConfig.TraceShowPath=checked; // immediate effect!
          break;
#endif//#if defined(SSE_DEBUG_TRACE)
        case IDC_STARTRUN:
          key="RunOnStart";
          break;
#if defined(SSE_GUI_RESTOREPOS)
        case IDC_RESTOREWINPOS:
          key="RestoreWinPos";
          break;
#endif
        }
        WriteCSFStr("Options",key,EasyStr(checked),globalINIFile);
      }
      break;

#if defined(SSE_AUTOUPDATE)
    case IDC_CHECKUPDATE:
    {
      bool checked=!!SendMessage((HWND)lPar,BM_GETCHECK,0,0);
      WriteCSFInt("Update","AutoUpdateEnabled",checked,globalINIFile);
      break;
    }
#endif

#if !defined(SSE_NO_UPDATE)
    case 4200:case 4201:case 4202:case 4203:
      if(wpar_hi==BN_CLICKED) {
        TConfigStoreFile CSF(globalINIFile);
        CSF.SetStr("Update","AutoUpdateEnabled",
          LPSTR(SendMessage(GetDlgItem(Win,4200),BM_GETCHECK,0,0)==0? "1" : "0"));
        CSF.SetStr("Update","AlwaysOnline",
          LPSTR(SendMessage(GetDlgItem(Win,4201),BM_GETCHECK,0,0)? "1" : "0"));
        CSF.SetStr("Update","PatchDownload",
          LPSTR(SendMessage(GetDlgItem(Win,4202),BM_GETCHECK,0,0)? "1" : "0"));
        CSF.SetStr("Update","AskPatchInstall",
          LPSTR(SendMessage(GetDlgItem(Win,4203),BM_GETCHECK,0,0)? "1" : "0"));
        CSF.Close();
      }
      break;
    case 4400:
      if(wpar_hi==BN_CLICKED) {
        EasyStr Online=LPSTR(SendMessage(GetDlgItem(Win,4201),BM_GETCHECK,0,0)?" online":"");
        EasyStr NoPatch=LPSTR(SendMessage(GetDlgItem(Win,4202),BM_GETCHECK,0,0)==0?" nopatchcheck":"");
        EasyStr AskPatch=LPSTR(SendMessage(GetDlgItem(Win,4203),BM_GETCHECK,0,0)?" askpatchinstall":"");
        WinExec(EasyStr("\"")+RunDir+"\\SteemUpdate.exe\""+Online+NoPatch+AskPatch,SW_SHOW);
      }
      break;
#endif
    case IDC_ASSOCIATE:
    case (IDC_ASSOCIATE+1):
    case (IDC_ASSOCIATE+2):
    case (IDC_ASSOCIATE+3):
    case (IDC_ASSOCIATE+4):
    case (IDC_ASSOCIATE+5):
    case (IDC_ASSOCIATE+6):
    case (IDC_ASSOCIATE+7):
    case (IDC_ASSOCIATE+8):
    case (IDC_ASSOCIATE+9):
      if(wpar_hi==BN_CLICKED) 
      {
        EasyStr Ext;
        switch(wpar_lo) {
        case (IDC_ASSOCIATE+0): Ext=dot_ext(EXT_ST);AssociateSteem(Ext,"st_disk_image"); 
          break;
#if USE_PASTI
        case (IDC_ASSOCIATE+1): Ext=dot_ext(EXT_STX);AssociateSteem(Ext,"st_disk_image");
#else
        case (IDC_ASSOCIATE+1): Ext=dot_ext(EXT_STT);AssociateSteem(Ext,"st_disk_image"); 
#endif
          break;
        case (IDC_ASSOCIATE+2): Ext=dot_ext(EXT_MSA);AssociateSteem(Ext,"st_disk_image"); 
          break;
#if defined(SSE_DISK_STW) // STW instead because it's less likely to be zipped
        case (IDC_ASSOCIATE+3): Ext=dot_ext(EXT_STW);AssociateSteem(Ext,"stw_disk_image"); 
#elif USE_PASTI
        case (IDC_ASSOCIATE+3): Ext=dot_ext(EXT_STX);AssociateSteem(Ext,"st_pasti_disk_image");
#endif
          break;
        case (IDC_ASSOCIATE+4): Ext=dot_ext(EXT_DIM);AssociateSteem(Ext,"st_disk_image"); 
          break;
        case (IDC_ASSOCIATE+5): Ext=".STZ";AssociateSteem(Ext,"st_disk_image");
          break;
        case (IDC_ASSOCIATE+6): Ext=".STS";AssociateSteem(Ext,"steem_memory_snapshot"); 
          break;
#if defined(SSE_DISK_HFE) // more useful
        case (IDC_ASSOCIATE+7): Ext=dot_ext(EXT_HFE);AssociateSteem(Ext,"st_hfe_disk_image");
#else
        case (IDC_ASSOCIATE+7): Ext=".STC";AssociateSteem(Ext,"st_cartridge");
#endif
           break;
#if defined(SSE_TOS_PRG_AUTORUN)
        case (IDC_ASSOCIATE+8):
          Ext=dot_ext(EXT_PRG);AssociateSteem(Ext,"st_atari_prg_executable"); break;
        case (IDC_ASSOCIATE+9):
          Ext=dot_ext(EXT_TOS); AssociateSteem(Ext,"st_atari_prg_executable"); break;
#endif
        }
        HWND But=(HWND)lPar;
        if(IsSteemAssociated(Ext)) 
          SendMessage(But,WM_SETTEXT,0,LPARAM(T("Associated").Text));
        else
          SendMessage(But,WM_SETTEXT,0,LPARAM(T("Associate").Text));
        EnableWindow(But,TRUE);
        }
      break;
    case IDC_ONE_INSTANCE:
    {
      if(wpar_hi!=BN_CLICKED) 
        break;
      TConfigStoreFile CSF(globalINIFile);
#if defined(SSE_ONEINSTANCE)
      bool OneInstance=!CSF.GetInt("Main","OneInstance",true);
      CSF.SetInt("Main","OneInstance",OneInstance);
      CSF.Close();
      SendMessage((HWND)lPar,BM_SETCHECK,OneInstance,0);
#else
      bool OpenFilesInNew=!CSF.GetInt("Options","OpenFilesInNew",true);
      CSF.SetInt("Options","OpenFilesInNew",OpenFilesInNew);
      CSF.Close();
      SendMessage((HWND)lPar,BM_SETCHECK,OpenFilesInNew,0);
#endif
      break;
    }
    case IDC_SYSEXSTATUSOUT:
      if(wpar_hi==BN_CLICKED) 
      {
        MIDI_out_running_status_flag=!MIDI_out_running_status_flag;
        SendMessage((HWND)lPar,BM_SETCHECK,(MIDI_out_running_status_flag
          ==MIDI_ALLOW_RUNNING_STATUS) ? BST_CHECKED : BST_UNCHECKED,0);
      }
      break;
    case IDC_SYSEXSTATUSIN:
      if(wpar_hi==BN_CLICKED) 
      {
        MIDI_in_running_status_flag=!MIDI_in_running_status_flag;
        SendMessage((HWND)lPar,BM_SETCHECK,(MIDI_in_running_status_flag
          ==MIDI_ALLOW_RUNNING_STATUS) ? BST_CHECKED : BST_UNCHECKED,0);
      }
      break;
    case IDC_MIDIUSETIMER:
      if(wpar_hi==BN_CLICKED) 
      {
        SSEOptions.MidiUseTimer=!SSEOptions.MidiUseTimer;
        SendMessage((HWND)lPar,BM_SETCHECK,(SSEOptions.MidiUseTimer)
          ? BST_CHECKED : BST_UNCHECKED,0);
      }
      break;
    case IDC_MIDIUSESLEEP:
      if(wpar_hi==BN_CLICKED) 
      {
        SSEOptions.MidiUseSleep=!SSEOptions.MidiUseSleep;
        SendMessage((HWND)lPar,BM_SETCHECK,(SSEOptions.MidiUseSleep)
          ? BST_CHECKED : BST_UNCHECKED,0);
      }
      break;
    case IDC_SYSEXBUFOUT:case IDC_SYSEXSIZEOUT:
      if(wpar_hi==CBN_SELENDOK) 
      {
        MIDI_out_n_sysex=(int)SendDlgItemMessage(Win,IDC_SYSEXBUFOUT,CB_GETCURSEL,0,0)+2;
        MIDI_out_sysex_max=(16<<SendDlgItemMessage(Win,IDC_SYSEXSIZEOUT,CB_GETCURSEL,0,0))*1024;
        for(int i=0;i<3;i++)
          if(STPort[i].MIDI_Out) 
            STPort[i].MIDI_Out->ReInitSysEx();
      }
      break;
    case IDC_SYSEXBUFIN:case IDC_SYSEXSIZEIN:
      if(wpar_hi==CBN_SELENDOK) 
      {
        MIDI_in_n_sysex=(int)SendDlgItemMessage(Win,IDC_SYSEXBUFIN,CB_GETCURSEL,0,0)+2;
        MIDI_in_sysex_max=(16<<SendDlgItemMessage(Win,IDC_SYSEXSIZEIN,CB_GETCURSEL,0,0))*1024;
        for(int i=0;i<3;i++)
          if(STPort[i].MIDI_In) 
            STPort[i].MIDI_In->ReInitSysEx();
      }
      break;
#if defined(SSE_DIRECTMIDI)
    case IDC_DIRECTMUSIC:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_DIRECTMUSIC=!OPTION_DIRECTMUSIC;
        MIDI_UNITS_1SEC=(OPTION_DIRECTMUSIC)?(1000*10000):1000; // unit is 100 nanoseconds or 1 millisecond
        This->DestroyCurrentPage();
        This->CreatePage(This->Page); // other options change
      }
      break;
    case IDC_MIDICLOCK:
      if(wpar_hi==CBN_SELENDOK) // change Master clock: must release all first, update GUI
      {
        DirectMidiIn.ReleasePort();
        DirectMidiOut.ReleasePort();
        HWND hPortsbase=GetDlgItem(Win,IDC_PORTSBASE);
        PostMessage(GetDlgItem(hPortsbase,IDC_PORTSBASE+IDC_MIDIOUTPUT),CB_SETCURSEL,0,0);
        PostMessage(GetDlgItem(hPortsbase,IDC_PORTSBASE+IDC_MIDIINPUT),CB_SETCURSEL,0,0);
        MIDIPort.MIDIOutDevice=-1;
        MIDIPort.MIDIInDevice=-1;
        DirectMidiClock.ReleaseMasterClock();
        DirectMidiClockIndex=(DWORD)SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
        CLOCKINFO ClockInfo;
        DirectMidiClock.GetClockInfo(DirectMidiClockIndex,&ClockInfo);
        DirectMidiClock.ActivateMasterClock(&ClockInfo);
      }
      break;
#endif
#if defined(SSE_MIDIRAW)
    case IDC_RAWMIDI:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_RAWMIDI=!OPTION_RAWMIDI;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_RAWMIDI,0);
      }
      break;
#endif
#if defined(SSE_MEGA16)
    case IDC_TURBO16MHZ:
      if(wpar_hi==BN_CLICKED)
      {
        ASSERTB(SSEConfig.Mega);
        if((Cpu16.ScuReg&3)==3)
          Cpu16.ScuReg=0;
        else
          Cpu16.ScuReg=7; // one option only, keep it simple
#if defined(SSE_420R5)
        SendMessage((HWND)lPar,BM_SETCHECK,((Cpu16.ScuReg&3)==3),0); // checked if 16MHz + cache
#else
        This->MachineUpdateIfVisible(); // want to update 8 or 16 in cb
#endif
      }
      break;
#endif
    case ID_BRIGHTNESS_MAP:
      if(wpar_hi==BN_CLICKED) 
        This->FullscreenBrightnessBitmap();
      break;
    case IDC_SAMPLERATE:
      if(wpar_hi==CBN_SELENDOK)
      {
        LRESULT freq=CBGetSelectedItemData((HWND)lPar);
        if(freq) 
          sound_chosen_freq=(int)freq;
        This->UpdateSoundFreq();
      }
      break;
    case IDC_SAMPLEFORMAT:
      if(wpar_hi==CBN_SELENDOK) 
      {
        WORD dat=(WORD)CBGetSelectedItemData((HWND)lPar);
        This->ChangeSoundFormat(LOBYTE(dat),HIBYTE(dat));
      }
      break;
#if defined(SSE_GUI_421_REORDER)
    case IDC_SOUNDBUFFER0:
      if(wpar_hi==EN_CHANGE)
      {
        char buf[12];
        SendMessage((HWND)lPar,WM_GETTEXT,4,(LPARAM)buf);
        psg_write_n_screens_ahead=(char)atoi(buf);
        break;
      }
#else
    case IDC_SOUNDBUFFER:
      if(wpar_hi==CBN_SELENDOK)
        psg_write_n_screens_ahead=(BYTE)SendMessage((HWND)lPar,CB_GETCURSEL,0,0)+1;
      break;
#endif
    case IDC_WAVORYM:
      if(wpar_hi==CBN_SELENDOK) 
        OPTION_SOUND_RECORD_FORMAT=(BYTE)SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
      break;
    case IDC_OLDFILTER:
      if(wpar_hi==CBN_SELENDOK)
        psg_hl_filter=(BYTE)SendMessage((HWND)lPar,CB_GETCURSEL,0,0)+1;
      break;
    case IDC_SOUNDMUTE:
      if(wpar_hi==BN_CLICKED)
        This->SoundMute(!OPTION_SOUNDMUTE);
      break;
#if defined(SSE_OPTION_FASTBLITTER)
    case IDC_FASTBLITTER:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_FASTBLITTER=!OPTION_FASTBLITTER;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_FASTBLITTER,0);
        SetTimingFunctions();
      }
      break;
#endif
    case IDC_SOUNDRECORD:
      if(wpar_hi==BN_CLICKED)
        This->SetSoundRecord(!bSoundRecord);
      break;
    case IDC_SOUNDDIRCHOOSE:
      if(wpar_hi==BN_CLICKED)
      {
        SendMessage((HWND)lPar,BM_SETCHECK,1,0);
        EnableAllWindows(false,Win);
        char wildcard[6]="*.wav";
        if(OPTION_SOUND_RECORD_FORMAT)
          strcpy(wildcard,"*.ym");
        char filetype[6+5];
        sprintf(filetype,"%s File",wildcard+2);
        char *fstypes=FSTypes(1,filetype,wildcard,NULL);
        EasyStr NewWAV=FileSelect((FullScreen) ? StemWin : Win,T("Choose Sound Output File"),
          This->WAVOutputDir,fstypes,1,0,wildcard+2);
        free(fstypes);
        if(NewWAV.NotEmpty()) 
        {
          WAVOutputFile=NewWAV;
          This->WAVOutputDir=NewWAV;
          RemoveFileNameFromPath(This->WAVOutputDir,REMOVE_SLASH);
          SendMessage(GetDlgItem(Win,IDP_SOUNDRECORD),WM_SETTEXT,0,(LPARAM)WAVOutputFile.Text);
        }
        SetForegroundWindow(Win);
        EnableAllWindows(true,Win);
        SendMessage((HWND)lPar,BM_SETCHECK,0,0);
        SetFocus((HWND)lPar);
      }
      break;
    case IDC_SOUNDRECORDWARN:
      if(wpar_hi==BN_CLICKED) 
      {
        This->RecordWarnOverwrite=!This->RecordWarnOverwrite;
        SendMessage((HWND)lPar,BM_SETCHECK,This->RecordWarnOverwrite,0);
      }
      break;
#if !defined(SSE_NO_INTERNAL_SPEAKER)
    case 7300:
      if(wpar_hi==BN_CLICKED) {
        if(sound_internal_speaker) SoundStopInternalSpeaker();

        sound_internal_speaker=!sound_internal_speaker;
        SendMessage((HWND)lPar,BM_SETCHECK,sound_internal_speaker,0);
      }
      break;
#endif
#if defined(SSE_TOS_KEYBOARD_CLICK)
    case IDC_KEYBOARDCLICK: // Keyboard click on/off
      if(wpar_hi==BN_CLICKED) 
      {
        OPTION_KEYBOARD_CLICK=!OPTION_KEYBOARD_CLICK;
        //TRACE_LOG("Option Keyboard click %d\n",OPTION_KEYBOARD_CLICK);
        Tos.CheckKeyboardClick();
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_KEYBOARD_CLICK,0);
      }
      break;
#endif
#if defined(SSE_SOUND_MICROWIRE_OPTION)
    case IDC_MICROWIRE: // STE Microwire on/off 
      if(wpar_hi==BN_CLICKED) 
      {
        OPTION_MICROWIRE=!OPTION_MICROWIRE; 
        //TRACE_LOG("Option Microwire %d\n",OPTION_MICROWIRE);
#if !defined(SSE_GUI_421_REORDER)
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_MICROWIRE,0);
#endif
      }
      break;
#endif
#if defined(SSE_VID_OLDSYNC)
    case IDC_OLDSYNC:
      if(wpar_hi==BN_CLICKED) 
      {
        SSEOptions.OldSync=!SSEOptions.OldSync;
        SendMessage((HWND)lPar,BM_SETCHECK,SSEOptions.OldSync,0);
      }
      break;
#endif
#if defined(SSE_SOUND_MICROWIRE_HACKS) 
#if 0//!defined(SSE_GEM_CONTROL_PANEL)
    case IDC_YM_12DB: // STE YM-12db
      if(wpar_hi==BN_CLICKED) 
      {
        OPTION_YM_12DB=!OPTION_YM_12DB;
        //TRACE_LOG("Option STE YM-12db %d\n",OPTION_YM_12DB);
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_YM_12DB,0);
        if(OPTION_SAMPLED_YM)
          Psg.LoadFixedVolTable(); // reload to adapt
      }
      break;
#endif
    case IDC_LMCSLOWFADE:
      if(wpar_hi==BN_CLICKED)
      {
        SSEOptions.LmcSlowFade=!SSEOptions.LmcSlowFade;
        //TRACE_LOG("Option LmcSlowFade %d\n",SSEOptions.LmcSlowFade);
        SendMessage((HWND)lPar,BM_SETCHECK,SSEOptions.LmcSlowFade,0);
      }
      break;
#endif
#if defined(SSE_YM2149_LL)
    case IDC_YMLL: // option Low-level YM
      if(wpar_hi==BN_CLICKED) 
      {
#if defined(SSE_EMU_THREAD)
        if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
          SoundLock.Lock();
#endif
        //SoundStop();
        OPTION_SAMPLED_YM=OPTION_MAME_YM=!OPTION_MAME_YM;
        //TRACE_LOG("Option Low-level YM %d\n",OPTION_MAME_YM);
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_MAME_YM,0);
        if(OPTION_SAMPLED_YM)
          Psg.LoadFixedVolTable();
        else
          Psg.FreeFixedVolTable();
        This->DestroyCurrentPage();
        This->CreatePage(This->Page);
        //SoundStart();
#if defined(SSE_EMU_THREAD)
        SoundLock.Unlock();
#endif
      }
      break;
#endif
#if defined(SSE_GUI_STATUS_BAR)
    case IDC_STATUSBAR:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_STATUS_BAR=!OPTION_STATUS_BAR;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_STATUS_BAR,0);
        //TRACE_LOG("Option status bar %d\n",OPTION_STATUS_BAR);
#if defined(SSE_EMU_THREAD) //!
        if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
          VideoLock.Lock();
#endif
        GuiSM.Update();
        StemWinResize();    
#if defined(SSE_EMU_THREAD)
        VideoLock.Unlock();
#endif
      }
      break;
    case IDC_TOSFLAG:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_TOSFLAG=!OPTION_TOSFLAG;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_TOSFLAG,0);
        UPDATE_STATUS_BAR_PART(SB_PART_ICONS);
      }
      break;
#endif
#if defined(SSE_DRIVE_SOUND)
    case IDC_DRIVE_SOUND: //  option Drive Sound  - also see IDC_DRIVE_SOUND_SLIDER for volume slider
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_DRIVE_SOUND=!OPTION_DRIVE_SOUND;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_DRIVE_SOUND,0);
        //TRACE_LOG("Option Drive Sound %d\n",OPTION_DRIVE_SOUND);
      }
      break;
#endif
#if defined(SSE_GUI_OPTION_FOR_TESTS) && !defined(SSE_GUI_421_REORDER)
    case IDC_BETATESTS: // Option Beta Tests: TODO new place?
      if(wpar_hi==BN_CLICKED)
      {
        SSEOptions.TestingNewFeatures=!SSEOptions.TestingNewFeatures;
        SendMessage((HWND)lPar,BM_SETCHECK,SSEOptions.TestingNewFeatures,0);
        //TRACE_LOG("Option Beta Tests %d\n",SSEOptions.TestingNewFeatures);
      }
      break;
#endif
    case IDC_LOCKWINDOW: // Option No resize
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_BLOCK_RESIZE=!OPTION_BLOCK_RESIZE;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_BLOCK_RESIZE,0);
        //EnableWindow(GetDlgItem(Win,IDC_LOCKAR),!OPTION_BLOCK_RESIZE);
        //TRACE_LOG("Option No resize %d\n",OPTION_BLOCK_RESIZE);
      }
      break;
    case IDC_LOCKAR: // Option Lock aspect ratio of the window
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_LOCK_AR=!OPTION_LOCK_AR;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_LOCK_AR,0);
        //TRACE_LOG("Option Lock aspect ratio %d\n",OPTION_LOCK_AR);
        ALL_SETTINGS_BEGIN
        This->DestroyCurrentPage();
        This->CreatePage(This->Page); // new options
        ALL_SETTINGS_END
        update_winsize();
        ChangeBorderSize(border); // which calls StemWinResize();
      }
      break;
#if defined(SSE_VID_D3D)
    case IDC_D3DMODE: // Option D3D mode
      if(wpar_hi==CBN_SELENDOK)
      {
        UINT old_mode=Disp.D3DMode;
        Disp.D3DMode=(UINT)SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
        Disp.D3DUpdateWH(Disp.D3DMode);
        //TRACE_LOG("Option D3D mode = %d %dx%d\n",Disp.D3DMode,Disp.D3DFsW,Disp.D3DFsH);
        if(FullScreen && old_mode!=Disp.D3DMode)
        {
          Disp.ScreenChange();
#if !defined(SSE_VID_2SCREENS) // done in D3DCreateSurfaces()
          SetWindowPos(StemWin,HWND_TOPMOST,0,0,Disp.D3DFsW,Disp.D3DFsH,SWP_FRAMECHANGED);
          InvalidateRect(StemWin,NULL,FALSE);
#endif
        }
      }
      break;
#endif
#if defined(SSE_ACSI_LASER)
    case IDC_LASERPRINTER:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_LASER=!OPTION_LASER;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_LASER,0);
      }
      break;
#endif
#if defined(SSE_PRINTER)
    case IDC_PRINTER:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_PRINTER=!OPTION_PRINTER;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_PRINTER,0);
      }
      break;
#endif
    case (IDC_RADIO_SWOVERSCAN+0): // Software Overscan = Shifter tricks
    case (IDC_RADIO_SWOVERSCAN+1):
    case (IDC_RADIO_SWOVERSCAN+2):
      if(wpar_hi==BN_CLICKED)
      {
        if(SendMessage((HWND)lPar,BM_GETCHECK,0,0)==BST_CHECKED)
        {
#if defined(SSE_EMU_THREAD)
          if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
            VideoLock.Lock();
#endif
          OPTION_VLE=(BYTE)(wpar_lo-IDC_RADIO_SWOVERSCAN); // 0=none, 1=high-level, 2=low-level
          //TRACE_LOG("Option VLE %d\n",OPTION_VLE);
          if(OPTION_C2 && (LINECYCLE0&1) && SSEConfig.Stvl) // when going C3->C2
            LINECYCLE0-=1; // high-level video emu wants even cycles
          SetTimingFunctions();
          Glue.Restore();
          Glue.Update();
          Disp.ScreenChange();
#if defined(SSE_EMU_THREAD)
          VideoLock.Unlock(); // it's in ScreenChange() too
#endif
        }
      }
      break;
    case IDC_SHIFTER_WU0:
      if(wpar_hi==EN_CHANGE)
      {
        char buf[12];
        SendMessage((HWND)lPar,WM_GETTEXT,3,(LPARAM)buf);
        OPTION_SHIFTER_WU=(char)atoi(buf);
        //TRACE_LOG("Option Shifter WU = %d\n",OPTION_SHIFTER_WU);
        Glue.Update(); // which will also update the Shifter...
      }
      break;
    case IDC_TIMINGLOOP0:
      if(wpar_hi==EN_CHANGE)
      {
        char buf[12];
        SendMessage((HWND)lPar,WM_GETTEXT,3,(LPARAM)buf);
        OPTION_TIMINGLOOP=(char)atoi(buf);
        //TRACE_LOG("Option TimingLoop = %d\n",OPTION_TIMINGLOOP);
      }
      break;
#if defined(SSE_TIMINGS_US)
    case IDC_MICROSECONDS:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_MICROSECONDS=!OPTION_MICROSECONDS;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_MICROSECONDS,0);
      }
      break;
#endif
#if defined(SSE_VID_BFI)
    case IDC_BFI:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_BFI=!OPTION_BFI;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_BFI,0);
      }
      break;
#endif
    case IDC_BLITTER_WU0:
      if(wpar_hi==EN_CHANGE)
      {
        char buf[12];
        SendMessage((HWND)lPar,WM_GETTEXT,3,(LPARAM)buf);
        OPTION_BLITTER_WU=(char)atoi(buf);
        //TRACE_LOG("Option Blitter WU = %d\n",OPTION_BLITTER_WU);
      }
      break;
#ifndef SSE_NO_OSD
    case IDC_SCROLLERSFREQ0:
      if(wpar_hi==EN_CHANGE)
      {
        char buf[12];
        SendMessage((HWND)lPar,WM_GETTEXT,12,(LPARAM)buf);
        OsdControl.ScrollerFrequency=(BYTE)atoi(buf);
      }
      break;
    case IDC_SCROLLERSSEC0:
      if(wpar_hi==EN_CHANGE)
      {
        char buf[12];
        SendMessage((HWND)lPar,WM_GETTEXT,12,(LPARAM)buf);
        OsdControl.SecondsBetweenScrollers=(DWORD)atoi(buf);
      }
      break;
#endif
    case (IDC_RADIO_FS_AR+0):
    case (IDC_RADIO_FS_AR+1):
    case (IDC_RADIO_FS_AR+2):
      if(wpar_hi==BN_CLICKED && SendMessage((HWND)lPar,BM_GETCHECK,0,0)==BST_CHECKED)
      {
        OPTION_FULLSCREEN_AR=(BYTE)(wpar_lo-IDC_RADIO_FS_AR);
        //TRACE_LOG("OPTION_FULLSCREEN_AR = %d\n",OPTION_FULLSCREEN_AR);
#if defined(SSE_VID_D3D)
        if(FullScreen && D3D9_OK)
          Disp.D3DSpriteInit();
#endif
      }
      break;
    case IDC_RADIO_BORDER:
    case (IDC_RADIO_BORDER+1):
    case (IDC_RADIO_BORDER+2):
    case (IDC_RADIO_BORDER+3):
      if(wpar_hi==BN_CLICKED && SendMessage((HWND)lPar,BM_GETCHECK,0,0)==BST_CHECKED)
      {
        This->SetBorder(wpar_lo-IDC_RADIO_BORDER);
        if(draw_grille_black<4) 
          draw_grille_black=4;
      }
      break;
#if defined(SSE_GUI_421_REORDER) // back because radio buttons = quick settings
    case IDC_CB_BORDER:
      if(wpar_hi==CBN_SELENDOK)
      {
         BYTE border_wished=(BYTE)SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
         This->SetBorder(border_wished);
      }
      break;
#endif
    case IDC_RADIO_CAPTURE_MOUSE:
    case (IDC_RADIO_CAPTURE_MOUSE+1):
    case (IDC_RADIO_CAPTURE_MOUSE+2):
    case (IDC_RADIO_CAPTURE_MOUSE+4):
      if(wpar_hi==BN_CLICKED && SendMessage((HWND)lPar,BM_GETCHECK,0,0)==BST_CHECKED)
      {
        OPTION_CAPTURE_MOUSE=(BYTE)(wpar_lo-IDC_RADIO_CAPTURE_MOUSE);
        //TRACE_LOG("OPTION_CAPTURE_MOUSE %d\n",OPTION_CAPTURE_MOUSE);
      }
      break;

#if defined(SSE_GUI_QUICKSETTINGS)
    case IDC_TOS_VERSION:
    case (IDC_TOS_VERSION+1):
    case (IDC_TOS_VERSION+2):
    case (IDC_TOS_VERSION+3):
    case (IDC_TOS_VERSION+4):
    case (IDC_TOS_VERSION+5):
    case (IDC_TOS_VERSION+6):
    case (IDC_TOS_VERSION+7):
      if(wpar_hi==BN_CLICKED && SendMessage((HWND)lPar,BM_GETCHECK,0,0)==BST_CHECKED)
      {
        This->TosList.Selected=wpar_lo-IDC_TOS_VERSION;
        This->NewROMFile=This->TosList.Path[This->TosList.Selected];
        if(IsSameStr_I(ROMFile,This->NewROMFile)) 
          This->NewROMFile="";
#if defined(SSE_GUI_INSTANTCHANGE)
        else if(SSEOptions.InstantMachineChange)
        {
          load_TOS(This->NewROMFile);
#if defined(SSE_420R12)
          ROMFile=This->NewROMFile;
#endif
          This->NewROMFile="";
          REFRESH_STATUS_BAR;
        }
#endif
        CheckResetIcon();
      }
      break;
#endif//#if defined(SSE_GUI_QUICKSETTINGS)

    case IDC_FULLSCREENGUI:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_FULLSCREEN_GUI=!OPTION_FULLSCREEN_GUI;
        //TRACE_LOG("Option FullScreen GUI = %d\n",OPTION_FULLSCREEN_GUI);
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_FULLSCREEN_GUI,0);
        EnableWindow(GetDlgItem(Win,IDC_CONFIRM_QUIT),OPTION_FULLSCREEN_GUI);
      }
      break;
#if defined(SSE_IKBD_RTC)
    case IDC_RADIO_6301BTRY:
    case (IDC_RADIO_6301BTRY+1):
    case (IDC_RADIO_6301BTRY+2):
      if(wpar_hi==BN_CLICKED)
      {
        if(SendMessage((HWND)lPar,BM_GETCHECK,0,0)==BST_CHECKED)
          OPTION_BATTERY6301=(BYTE)(wpar_lo-IDC_RADIO_6301BTRY);
      }
      break;
    case IDC_RTCHACK:
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_RTC_HACK=!OPTION_RTC_HACK;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_RTC_HACK,0);
        //TRACE_LOG("Option RTC hack = %d\n",OPTION_RTC_HACK);
      }
      break;
#endif
#if defined(SSE_HARDWARE_OVERSCAN)
    case IDC_RADIO_HWOVERSCAN:
    case (IDC_RADIO_HWOVERSCAN+1):
    case (IDC_RADIO_HWOVERSCAN+2):
      if(wpar_hi==BN_CLICKED)
      {
        if(SendMessage((HWND)lPar,BM_GETCHECK,0,0)==BST_CHECKED)
          OPTION_HWOVERSCAN=(BYTE)(wpar_lo-IDC_RADIO_HWOVERSCAN);
        StvlUpdate();
      }
      break;
#endif
    case IDC_RADIO_STSCREEN://colour
    case (IDC_RADIO_STSCREEN+1): //mono
      if(wpar_hi==BN_CLICKED)
      {
        if(SendMessage((HWND)lPar,BM_GETCHECK,0,0)==BST_CHECKED)
        {
          This->NewMonitorSel=wpar_lo-IDC_RADIO_STSCREEN;
          //TRACE_LOG("Option monitor %d\n",This->NewMonitorSel);
          extended_monitor=0;
          //This->UpdateSTVideoPage();
#if defined(SSE_GUI_INSTANTCHANGE)
          if(SSEOptions.InstantMachineChange)
#else
          if(runstate==RUNSTATE_RUNNING)
#endif
          {
            // can change live, funny effects (in general crash)
            switch(This->NewMonitorSel) {
            case 0:
              //screen_res=LORES;
              screen_res=Shifter.ShiftMode&1; // low or medium
              init_screen();
              mfp_gpip_no_interrupt|=MFP_GPIP_COLOUR;
              SSEConfig.ColourMonitor=TRUE;
              //EnableWindow(GetDlgItem((HWND)lPar,IDC_RADIO_BORDER+2),TRUE);
              break;
            case 1:
              screen_res=HIRES;
              init_screen();
              mfp_gpip_no_interrupt&=MFP_GPIP_NOT_COLOUR;
              SSEConfig.ColourMonitor=FALSE;
              break;
            }//sw
            mfp_gpip_set_bit(MFP_GPIP_MONO_BIT,!!SSEConfig.ColourMonitor);
#ifdef SSE421R1
#if defined(SSE_EMU_THREAD)
            bool oldSuspendRendering=SuspendRendering;
            if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
            {
              VideoLock.Lock();
              SuspendRendering=true;
            }
#endif
            StemWinResize();
#if defined(SSE_EMU_THREAD)
            VideoLock.Unlock();
            SuspendRendering=oldSuspendRendering;
#endif
#endif
          }
#if !defined(SSE_GUI_421_REORDER)
          This->UpdateSTVideoPage();
#endif
#ifdef SSE421R1
          CheckResetIcon();
#else
#if defined(SSE_EMU_THREAD)
          if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
            VideoLock.Lock();
          bool oldSuspendRendering=SuspendRendering;
          SuspendRendering=true;
#endif
          StemWinResize();
#if defined(SSE_EMU_THREAD)
          VideoLock.Unlock();
          SuspendRendering=oldSuspendRendering;
#endif
#endif//#ifdef SSE421R1
        }
      }
      break;
#ifndef NO_CRAZY_MONITOR
    case (IDC_RADIO_STSCREEN+2)://extended
      if(SendMessage((HWND)lPar,BM_GETCHECK,0,0)==BST_CHECKED)
      {
        This->NewMonitorSel=2;
#if !defined(SSE_GUI_421_REORDER)
        This->UpdateSTVideoPage();
#endif
        //extended_monitor=1; // when mode selected?
      }
      break;
#endif/#ifndef NO_CRAZY_MONITOR
#if defined(SSE_GUI_DEFAULT_ST_CONFIG)
    case IDC_DEFCON: // ST Preselect
      if(wpar_hi==BN_CLICKED)
      {
        OPTION_ST_PRESELECT=!OPTION_ST_PRESELECT;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_ST_PRESELECT,0);
        //TRACE_LOG("Option ST Preselect = %d\n",OPTION_ST_PRESELECT);
      }
      break;
#endif
    case IDC_MEMORY_SIZE: // Memory size
    case (IDC_MEMORY_SIZE+1): // (simplified radio buttons)
    case (IDC_MEMORY_SIZE+2):
    case (IDC_MEMORY_SIZE+3):
#if !defined(SSE_GUI_QUICKSETTINGS)
      ALL_SETTINGS_BEGIN
#endif
      if(wpar_hi==CBN_SELENDOK)
      {
        DWORD Conf=(DWORD)CBGetSelectedItemData((HWND)lPar);
        This->NewMemConf0=LOWORD(Conf);
        This->NewMemConf1=HIWORD(Conf);
        if(SSEConfig.bank_length[0]==Mmu.BankLength(This->NewMemConf0) 
          && SSEConfig.bank_length[1]==Mmu.BankLength(This->NewMemConf1))
        {
          This->NewMemConf0=-1; // no change
        }
#if !defined(SSE_GUI_INSTANTCHANGE)
        CheckResetIcon();
#endif
      }
#if !defined(SSE_GUI_QUICKSETTINGS)
      ALL_SETTINGS_ELSE
#endif
      if(wpar_hi==BN_CLICKED)
      {
        if(SendMessage((HWND)lPar,BM_GETCHECK,0,0)==BST_CHECKED)
        {
          int i=wpar_lo-IDC_MEMORY_SIZE;
          This->NewMemConf0=(i<2)?MEMCONF_512:MEMCONF_2MB;
          This->NewMemConf1=(i&1)?This->NewMemConf0:MEMCONF_0;
          if(SSEConfig.bank_length[0]==Mmu.BankLength(This->NewMemConf0) 
            && SSEConfig.bank_length[1]==Mmu.BankLength(This->NewMemConf1))
          {
            This->NewMemConf0=-1;
          }
#if !defined(SSE_GUI_INSTANTCHANGE)
          CheckResetIcon();
#endif
        }
      }
#if !defined(SSE_GUI_QUICKSETTINGS)
      ALL_SETTINGS_END
#endif
#if defined(SSE_GUI_INSTANTCHANGE)
      if(This->NewMemConf0>=0)
      {
        if(SSEOptions.InstantMachineChange)
        {
          SSEConfig.make_Mem((BYTE)This->NewMemConf0,(BYTE)This->NewMemConf1);
          This->NewMemConf0=-1;
        }
      }
      CheckResetIcon(); // anyway
#endif
      break;
    case IDC_EXTENDED_MONITOR:
      if(wpar_hi==CBN_SELENDOK)
      {
#if defined(SSE_GUI_421_REORDER) // as before
        This->NewMonitorSel=(int)SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
#else
        // if Extended button checked, get selection
        if(SendMessage(GetDlgItem(This->Handle,(IDC_RADIO_STSCREEN+2)),
          BM_GETCHECK,0,0)==BST_CHECKED)
          This->NewMonitorSel=(int)SendMessage((HWND)lPar,CB_GETCURSEL,0,0)+2;
#endif
        if(This->NewMonitorSel==This->GetCurrentMonitorSel()) 
          This->NewMonitorSel=-1;
        CheckResetIcon();
      }
      break;
    case IDC_CHOOSECART: // Choose cart
      if(wpar_hi==BN_CLICKED)
      {
        SendMessage((HWND)lPar,BM_SETCHECK,1,0);
        EnableAllWindows(false,Win);
        Str LastCartFol=This->LastCartFile;
        RemoveFileNameFromPath(LastCartFol,REMOVE_SLASH);
        char *fstypes=FSTypes(0,T("ST Cartridge Images").Text,"*.stc",NULL);
        EasyStr NewCart=FileSelect((FullScreen) ? StemWin : Win,T("Find a Cartridge"),
          LastCartFol,fstypes,1,true,"stc",GetFileNameFromPath(This->LastCartFile));
        free(fstypes);
        if(NewCart.NotEmpty())
        {
          SetWindowText(GetDlgItem(Win,IDP_CARTRIDGE),NewCart);
          EnableWindow(GetDlgItem(Win,IDC_REMOVECART),TRUE);
          EnableWindow(GetDlgItem(Win,IDC_FREEZECART),TRUE);
          EnableWindow(GetDlgItem(Win,IDC_SWITCHCART),TRUE);
          This->LastCartFile=NewCart;
          if(!load_cart(NewCart))
            Alert(T("There was an error loading the cartridge."),T("ERROR"),MB_ICONEXCLAMATION);
          else
            CartFile=NewCart;
          CheckResetDisplay();
        }
        SetForegroundWindow(Win);
        EnableAllWindows(true,Win);
        SetFocus((HWND)lPar);
        SendMessage((HWND)lPar,BM_SETCHECK,0,0);
      }
      break;
    case IDC_REMOVECART: // Remove cart
      if(wpar_hi==BN_CLICKED)
      {
        SetWindowText(GetDlgItem(Win,IDP_CARTRIDGE),"");
        SetFocus(GetDlgItem(Win,IDC_CHOOSECART));
        for(int i=wpar_lo;i<=wpar_lo+2;i++)
          EnableWindow(GetDlgItem(Win,i),FALSE);
        if(cart_save)
          cart=cart_save;
        cart_save=NULL;
#if defined(SSE_CARTRIDGE_ACTIVE2)
        VirtualFree(cart,0,MEM_RELEASE);
#else
        delete[] cart;
#endif
        cart=NULL;
#if defined(SSE_SOUND_CARTRIDGE)
        SSEConfig.mv16=SSEConfig.mr16=false;
#endif
#if defined(SSE_DONGLE_CUBASE2)
        SSEConfig.Cubase2Cart=false;
#endif
#if defined(SSE_DONGLE_CUBASE3)
        SSEConfig.Cubase3Cart=false;
#endif
        CartFile="";
        CheckResetDisplay();
      }
      break;
#if defined(SSE_DONGLE)
/*  The Multiface ST cartridge features a 'freeze' button that messes with the
    monochrome monitor detection interrupt. That's the purpose of the cable
    that intercepts the monitor connection.
    The bit is cleared only as long as the player is pressing the button.
    Same idea on the Ultimate Ripper cartridge, but the button hits a line of
    the serial port instead.
    The switches are considered as dongles to simplify the GUI.
*/
    case IDC_FREEZECART:
      switch(DONGLE_ID) {
#if defined(SSE_DONGLE_URC)
      case TDongle::URC:
        mfp_gpip_set_bit(MFP_GPIP_RING_BIT,false); // Ultimate Ripper
        break;
#endif
#if defined(SSE_DONGLE_MULTIFACE)
      case TDongle::MULTIFACE:
        mfp_gpip_set_bit(MFP_GPIP_MONO_BIT,false); // Multiface
        break;
#endif
      }//sw
      break;
#endif
    case IDC_SWITCHCART:
      if(OPTION_CARTRIDGE_OFF)
      {
        if(cart_save)
          cart=cart_save;
        cart_save=NULL;
      }
      else
      {
        if(cart)
          cart_save=cart;
        cart=NULL;
      }
      OPTION_CARTRIDGE_OFF=!OPTION_CARTRIDGE_OFF;
      OptionBox.MachineUpdateIfVisible();
      break;
    case IDC_REBOOT: // Cold reset
      if(wpar_hi==BN_CLICKED)
      {
        //TRACE_LOG("Option reboot\n");
        reset_st(RESET_COLD|RESET_STOP|RESET_CHANGESETTINGS|RESET_BACKUP|RESET_COUNT);
      }
      break;
    case IDC_KBDLANG: // Keyboard language
      if(wpar_hi==CBN_SELENDOK)
      {
#if defined(SSE_IKBD_MAPPINGFILE)
        LRESULT cursel=SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
        int ix=(int)SendMessage((HWND)lPar,CB_GETITEMDATA,cursel,0);
        HWND hLang=GetDlgItem(Win,IDC_KBDLANG);
        if(ix==LANG_CUSTOM)
        {
          char *fstypes=FSTypes(0,T("Configuration files").Text,
            "*." CONFIG_FILE_EXT,NULL);
          EasyStr path=FileSelect(NULL,T("Select Keyboard Mapping"),
            KeyboardMappingPath,fstypes,1,true,"ini",KeyboardMappingPath);
          free(fstypes);
          if(path.IsNotEmpty())
          {
            KeyboardMappingPath=path;
            KeyboardLangID=(LANGID)ix; // else don't change
            ToolAddWindow(ToolTip,hLang,KeyboardMappingPath);
          }
        }
        else
        {
          KeyboardLangID=(LANGID)ix;
          ToolAddWindow(ToolTip,hLang,"");
        }
#else
        KeyboardLangID=(LANGID)SendMessage((HWND)lPar,CB_GETITEMDATA,
          SendMessage((HWND)lPar,CB_GETCURSEL,0,0),0);
#endif
        InitKeyTable();
      }
      break;
    case IDC_KBDALTSHIFT: // Keyboard alt and shift correction
      if(wpar_hi==BN_CLICKED)
      {
        bEnableShiftSwitching=!!SendMessage((HWND)lPar,BM_GETCHECK,0,0);
        InitKeyTable();
      }
      break;
    case IDC_TOSLIST:
      if(wpar_hi==LBN_SELCHANGE)
      {
        int cursel=(int)SendMessage((HWND)lPar,LB_GETCURSEL,0,0);
        if(cursel!=LB_ERR)
        {
          int idx=cursel;
          if(This->eslTOS_Descend) 
            idx=This->eslTOS.NumStrings-1-cursel;
          This->NewROMFile=strchr(This->eslTOS[idx].String,'\01')+1;
#if defined(SSE_GUI_QUICKSETTINGS)
          // as soon as clicked, this file replaces the quick settings slot for that version
          This->TosList.AddPath(This->NewROMFile);
#endif
          if(IsSameStr_I(ROMFile,This->NewROMFile)) 
            This->NewROMFile="";
#if defined(SSE_GUI_INSTANTCHANGE)
          else if(SSEOptions.InstantMachineChange)
          {
            load_TOS(This->NewROMFile);
#if defined(SSE_420R12)
            ROMFile=This->NewROMFile;
#endif
            This->NewROMFile="";
            REFRESH_STATUS_BAR;
          }
#endif
        }
        CheckResetIcon();
      }
      break;
    case IDC_ADDTOS:
      if(wpar_hi==BN_CLICKED)
      {
        SendMessage((HWND)lPar,BM_SETCHECK,1,0);
        EnableAllWindows(false,Win);
        OPENFILENAME ofn;
        ZeroMemory(&ofn,sizeof(OPENFILENAME));
        char *files=new char[MAX_PATH*80+1];
        ZeroMemory(files,MAX_PATH*80+1);
        ofn.lStructSize=sizeof(OPENFILENAME);
        ofn.hwndOwner=(FullScreen) ? StemWin : Win;
        ofn.hInstance=(HINSTANCE)GetModuleHandle(NULL);
        char *fstypes=FSTypes(3,NULL);
        ofn.lpstrFilter=fstypes;
        ofn.lpstrCustomFilter=NULL;
        ofn.nMaxCustFilter=0;
        ofn.nFilterIndex=1;
        ofn.lpstrFile=files;
        ofn.nMaxFile=MAX_PATH*80;
        ofn.lpstrFileTitle=NULL;
        ofn.nMaxFileTitle=0;
        ofn.lpstrInitialDir=This->TOSBrowseDir;
        ofn.lpstrTitle=StaticT("Select One or More TOS Images");
        ofn.Flags=OFN_HIDEREADONLY|OFN_NOCHANGEDIR|OFN_FILEMUSTEXIST
          |OFN_PATHMUSTEXIST|OFN_ALLOWMULTISELECT|OFN_EXPLORER;
        ofn.lpstrDefExt="IMG";
        ofn.lpfnHook=NULL;
        ofn.lpTemplateName=NULL;
        if(GetOpenFileName(&ofn))
        {
          int nfiles=0;
          char *f=files;
          while(f[0]!='\0')
          {
            f+=strlen(f)+1;
            nfiles++;
          }
          This->TOSBrowseDir=files;
          if(nfiles==1)
          {
            RemoveFileNameFromPath(This->TOSBrowseDir,REMOVE_SLASH);
            files+=strlen(This->TOSBrowseDir)+1; // only want name
          }
          else
            files+=strlen(files)+1; // skip to files
          HWND TOSBox=GetDlgItem(Win,IDC_TOSLIST);
          Str file;
          char *cur_file=files;
          while(cur_file[0]!='\0')
          {
            file=This->TOSBrowseDir+SLASH+cur_file;
            int c=(int)SendMessage(TOSBox,LB_GETCOUNT,0,0);
            for(int i=0;i<c;i++)
            {
              if(IsSameStr_I(file,strrchr(This->eslTOS[i].String,'\01')+1))
              {
                if(nfiles==1)
                {
                  SendMessage(TOSBox,LB_SETCARETINDEX,i,0);
                  SendMessage(TOSBox,LB_SETCURSEL,i,0);
                }
                file=""; // skip this file
                break;
              }
            }
            if(file[0])
            {
              WIN32_FIND_DATA wfd;
              HANDLE hFind=FindFirstFile(file,&wfd);
              if(hFind!=INVALID_HANDLE_VALUE)
              {
                FindClose(hFind);
                if(get_TOS_address(file))
                {
                  int n=2;
                  EasyStr LinkFileName=UsersPath+SLASH+GetFileNameFromPath(file)+".lnk";
                  while(Exists(LinkFileName))
                    LinkFileName=UsersPath+SLASH+GetFileNameFromPath(file)+" ("+(n++)+").lnk";
                  CreateLink(LinkFileName,file,T("TOS Image"));
                  if(nfiles==1) 
                    This->NewROMFile=file;
                }
                else if(nfiles==1)
                  Alert(Str(file)+" "+T("is not a valid TOS image."),T("ERROR"),MB_ICONEXCLAMATION);
              }
            }
            cur_file+=strlen(cur_file)+1;
          }
          This->RefreshTOSBox();
        }
        free(fstypes);
        SetForegroundWindow(Win);
        EnableAllWindows(true,Win);
        SendMessage((HWND)lPar,BM_SETCHECK,0,0);
        SetFocus((HWND)lPar);
        CheckResetIcon();
      }
      break;
    case IDC_REMOVETOS:
      if(wpar_hi==BN_CLICKED)
      {
        SendMessage((HWND)lPar,BM_SETCHECK,1,0);
        HWND TOSBox=GetDlgItem(Win,IDC_TOSLIST);
        int cursel=(int)SendMessage(TOSBox,LB_GETCURSEL,0,0);
        if(This->eslTOS_Descend) 
          cursel=This->eslTOS.NumStrings-1-cursel;
        char *RemovePath=strrchr(This->eslTOS[cursel].String,'\01')+1;
        int idx=cursel+int(This->eslTOS_Descend?1:-1);
        if(idx<0) 
          idx=1;
        if(idx>=This->eslTOS.NumStrings) 
          idx=This->eslTOS.NumStrings-2;
        Str NewSel=strrchr(This->eslTOS[idx].String,'\01')+1;
        DirSearch ds;
        EasyStringList dellist;
        if(ds.Find(UsersPath+SLASH "*.*"))
        {
          Str Path,RealPath;
          do
          {
            if((ds.Attrib & (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_HIDDEN))==0)
            {
              Path=UsersPath+SLASH+ds.Name;
              if(has_extension(Path,"LNK"))
              {
                WIN32_FIND_DATA wfd;
                RealPath=GetLinkDest(Path,&wfd);
              }
              else
                RealPath=Path;
              if(IsSameStr_I(RealPath,RemovePath)) 
                dellist.Add(Path,IsSameStr_I(RealPath,Path));
            }
          } while(ds.Next());
          ds.Close();
        }
        for(int i=0;i<dellist.NumStrings;i++)
        {
          if(dellist[i].Data[0])
            // Not link
            SetFileAttributes(dellist[i].String,
              GetFileAttributes(dellist[i].String)|FILE_ATTRIBUTE_HIDDEN);
          else
            DeleteFile(dellist[i].String);
        }
        This->NewROMFile=NewSel;
        This->RefreshTOSBox();
        SendMessage((HWND)lPar,BM_SETCHECK,0,0);
      }
      break;
    case IDC_TOSSORTBY:
      if(wpar_hi==CBN_SELENDOK)
      {
        This->eslTOS_Sort=(ESLSortEnum)(signed short)LOWORD(CBGetSelectedItemData((HWND)lPar));
        This->eslTOS_Descend=(HIWORD(CBGetSelectedItemData((HWND)lPar))!=0);
        This->RefreshTOSBox("");
      }
      break;
    case IDC_NEWMACRO:
      if(wpar_hi!=BN_CLICKED) 
        break;
      DTree.NewItem(T("New Macro"),DTree.RootItem,1);
      break;
    case IDC_CHOOSEMACRODIR:case IDC_CHOOSECONFIGDIR:
      if(wpar_hi==BN_CLICKED)
      {
        SendMessage((HWND)lPar,BM_SETCHECK,1,0);
        EnableAllWindows(false,Win);
        Str *Dir=(Str*)((wpar_lo==IDC_CHOOSEMACRODIR)?&(This->MacroDir):&(This->ProfileDir));
        EasyStr NewFol=ChooseFolder((FullScreen) ? StemWin : Win,T("Pick a Folder"),Dir->Text);
        if(NewFol.NotEmpty())
        {
          NO_SLASH(NewFol);
          *Dir=NewFol;
          DTree.RootFol=NewFol;
          DTree.RefreshDirectory();
        }
        SetForegroundWindow(Win);
        EnableAllWindows(true,Win);
        SetFocus((HWND)lPar);
        SendMessage((HWND)lPar,BM_SETCHECK,0,0);
      }
      break;
    case IDC_RECORDMACRO:
      if(wpar_hi!=BN_CLICKED) 
        break;
      if(macro_record==0)
      {
        macro_record_file=This->MacroSel;
        macro_advance(MACRO_STARTRECORD);
      }
      else
        macro_end(MACRO_ENDRECORD);
      break;
    case IDC_PLAYMACRO:
      if(wpar_hi!=BN_CLICKED)
        break;
      if(macro_play==0)
      {
        macro_play_file=This->MacroSel;
        macro_advance(MACRO_STARTPLAY);
      }
      else
        macro_end(MACRO_ENDPLAY);
      break;
    case IDC_MACROPLAYSPEED:
    case IDC_MACROMOUSESPEED:
      if(wpar_hi==CBN_SELENDOK)
      {
        int setting=(int)CBGetSelectedItemData((HWND)lPar);
        TMacroFileOptions MFO;
        macro_file_options(MACRO_FILE_GET,This->MacroSel,&MFO);
        switch(wpar_lo) {
        case IDC_MACROPLAYSPEED:
          MFO.allow_same_vbls=setting;
          break;
        case IDC_MACROMOUSESPEED:
          MFO.max_mouse_speed=setting;
          break;
        }//sw
        macro_file_options(MACRO_FILE_SET,This->MacroSel,&MFO);
      }
      break;
    case IDC_NEWCONFIG:
    case IDC_SAVECONFIG:
      if(wpar_hi==BN_CLICKED)
      {
        Str Path;
        if(wpar_lo==IDC_SAVECONFIG)
        {
          Path=This->ProfileSel;
          if(IDNO==Alert(T("Overwrite file?"),GetFileNameFromPath(Path),MB_ICONQUESTION|MB_YESNO)) 
            break;
        }
        else
        {
          HTREEITEM Item=DTree.NewItem(T("New Profile"),DTree.RootItem,1);
          if(Item==NULL) 
            break;
          Path=DTree.GetItemPath(Item);
        }
        SaveAllDialogData(false,Path);
      }
      break;
    case IDC_CONFIGTOGGLE:
      if(wpar_hi==BN_CLICKED)
      {
        HWND LV=GetDlgItem(This->Handle,IDC_CONFIGLISTVIEW); // listview of profiles
        // count checked profiles, check all if missing, none if all checked
        int state,ctr=0;
        LVITEM lvi;
        ZeroMemory(&lvi,sizeof(lvi));
        lvi.mask=LVIF_TEXT;
#if defined(SSE_420R4)
        for(BYTE i=0;ProfileSection[i].Name!=NULL;i++)
        {
          state=(int)SendMessage(LV,LVM_GETITEMSTATE,i,(LPARAM)LVI_SI_CHECKED);
          lvi.iItem=i;
          SendMessage(LV,LVM_GETITEMTEXT,i,(LPARAM)&lvi);
          if(!(state&LVI_SI_CHECKED))
            ctr++; // at least one is not checked
        }
        state=(ctr)?LVI_SI_CHECKED:LVI_SI_UNCHECKED;
#else
        for(int i=0;i<PSEC_NSECT;i++)
        {
          state=(int)SendMessage(LV,LVM_GETITEMSTATE,i,(LPARAM)LVI_SI_CHECKED);
          lvi.iItem=i;
          SendMessage(LV,LVM_GETITEMTEXT,i,(LPARAM)&lvi);
          if(state&LVI_SI_CHECKED)
            ctr++;
        }
        state=(ctr<PSEC_NSECT-1)?LVI_SI_CHECKED:LVI_SI_UNCHECKED;
#endif
        ListView_SetItemState(LV,-1,state,LVIS_STATEIMAGEMASK);
      }
      break;
    case IDC_LOADCONFIG:
      if(wpar_hi==BN_CLICKED) 
        This->LoadProfile(This->ProfileSel);
      break;
    case IDC_LOADICONS:
      if(wpar_hi==BN_CLICKED)
      {
        SendMessage((HWND)lPar,BM_SETCHECK,1,0);
        EnableAllWindows(false,Win);
        Str LastIconSchemeFol=This->LastIconSchemePath;
        RemoveFileNameFromPath(LastIconSchemeFol,REMOVE_SLASH);
        char *fstypes=FSTypes(1,T("Steem Icon Scheme").Text,"*.stico",NULL);
        Str NewFile=FileSelect((FullScreen) ? StemWin : Win,T("Load Icon"),
          LastIconSchemeFol,fstypes,1,true,"stico",GetFileNameFromPath(This->LastIconSchemePath));
        free(fstypes);
        if(NewFile.NotEmpty())
        {
          This->LastIconSchemePath=NewFile;
          TConfigStoreFile SchemeCSF(NewFile);
          TConfigStoreFile CSF(globalINIFile);
          Str TransSect=T("Patch Text Section=");
          if(TransSect=="Patch Text Section=") 
            TransSect="Text";
#ifndef SSE_LEAN_AND_MEAN
          Str Desc;
          for(int i=0;i<2;i++)
          {
            Desc=SchemeCSF.GetStr(TransSect,"Description","");
            if(Desc.NotEmpty()) 
              break;
            TransSect="Text";
          }
          if(Desc.NotEmpty())
            Desc+="\r\n\r\n";
          Desc+=T("Do you want to load this icon scheme?");
          if(Alert(Desc,T("Are you sure?"),MB_ICONQUESTION|MB_YESNO)==IDYES)
#endif
          {
            EasyStringList Fols(eslNoSort);
            Str Fol=This->LastIconSchemePath;
            RemoveFileNameFromPath(Fol,REMOVE_SLASH);
            CSF.SetInt("Icons","UseDefaultIn256",SchemeCSF.GetInt("Options","UseDefaultIn256",0));
            Str File;
            for(int icn=1;;icn++)
            {
              File=SchemeCSF.GetStr("Options",Str("SearchFolder")+(icn),"");
              if(File.Empty())
                break;
              Fols.Add(Fol+SLASH+File);
            }
            Fols.Add(Fol);
            for(int n=1;n<RC_NUM_ICONS;n++)
            {
              File=SchemeCSF.GetStr("Icons",Str("Icon")+n,".");
              if(File!=".")
              {
                Fol="";
                for(int i=0;i<Fols.NumStrings;i++)
                {
                  if(Exists(Str(Fols[i].String)+SLASH+File))
                  {
                    Fol=Fols[i].String;
                    break;
                  }
                }
                if(Fol.NotEmpty()) 
                  CSF.SetStr("Icons",Str("Icon")+n,Fol+SLASH+File);
              }
            }
            LoadAllIcons(&CSF);
          }
          SchemeCSF.Close();
          CSF.Close();
        }
        SetForegroundWindow(Win);
        EnableAllWindows(true,Win);
        SendMessage((HWND)lPar,BM_SETCHECK,0,0);
      }
      break;
    case IDC_ICONSDEF:
      if(wpar_hi==BN_CLICKED)
      {
        TConfigStoreFile CSF(globalINIFile);
        CSF.SetInt("Icons","UseDefaultIn256",0);
        for(int n=1;n<RC_NUM_ICONS;n++) 
          CSF.SetStr("Icons",Str("Icon")+n,"");
        LoadAllIcons(&CSF,0);
        CSF.Close();
      }
      break;
    }//sw(wpar_lo) - we're still handling WM_COMMAND
    REFRESH_STATUS_BAR_GX; // overkill
    if(wpar_lo>=IDC_ICONSBASE&&wpar_lo<IDC_ICONSBASE+RC_NUM_ICONS)
    { // Icons
      if(wpar_hi==BN_CLICKED)
      {
        bool AllowDefault=true;
        Str NewFile;
        SendMessage((HWND)lPar,BM_SETCHECK,1,0);
        if(SendMessage((HWND)lPar,BM_GETCLICKBUTTON,0,0)==BM_CLICKBUTTON_LEFT)
        {
          AllowDefault=false;
          EnableAllWindows(false,Win);
          Str LastIconFol=This->LastIconPath;
          RemoveFileNameFromPath(LastIconFol,REMOVE_SLASH);
          char *fstypes=FSTypes(1,T("Icon File").Text,"*.ico",NULL);
          NewFile=FileSelect((FullScreen) ? StemWin : Win,T("Load Icon"),
            LastIconFol,fstypes,1,TRUE,"ico",GetFileNameFromPath(This->LastIconPath));
          free(fstypes);
          if(NewFile.NotEmpty()) 
            This->LastIconPath=NewFile;
          SetForegroundWindow(Win);
          EnableAllWindows(true,Win);
        }
        if(NewFile.NotEmpty()||AllowDefault)
        {
          TConfigStoreFile CSF(globalINIFile);
          CSF.SetStr("Icons",Str("Icon")+(wpar_lo-IDC_ICONSBASE),NewFile);
          LoadAllIcons(&CSF);
          CSF.Close();
        }
        SendMessage((HWND)lPar,BM_SETCHECK,0,0);
      }

    }//sw
    else if(wpar_lo>=IDC_PORTSBASE&&wpar_lo<IDC_PORTSBASE+NSTPORTS*100)
    { // Ports
      Str ErrorText,ErrorTitle;
      BYTE Port=(BYTE)((wpar_lo-IDC_PORTSBASE)/100);
      ASSERTB(Port<NSTPORTS);
      int Control=(wpar_lo%100);
      LRESULT cursel=SendMessage((HWND)lPar,CB_GETCURSEL,0,0); // code size opt
#if defined(SSE_DONGLE_PORT)
      if(Port==TSTPort::DONGLE)
        STPort[Port].Type=(int)SendMessage((HWND)lPar,CB_GETITEMDATA,cursel,0);
      else
#endif
      switch(Control) {
      case IDC_CONNECTTO:
        if(wpar_hi==CBN_SELENDOK)
        {
          STPort[Port].Type=(int)SendMessage((HWND)lPar,CB_GETITEMDATA,cursel,0);
          This->PortsMakeTypeVisible(Port);
          STPort[Port].Create(Port,ErrorText,ErrorTitle,true);
        }
        break;
      case IDC_MIDIOUTPUT:
        if(wpar_hi==CBN_SELENDOK)
        {
          int NewDevice=
#if defined(SSE_DIRECTMIDI)
            (OPTION_DIRECTMUSIC)?(int)CBGetSelectedItemData((HWND)lPar):
#endif
            (int)cursel-2; // -2=none, -1=midi mapper
          if(NewDevice!=STPort[Port].GetMIDIOutDeviceID())
          {
            STPort[Port].MIDIOutDevice=NewDevice;
            //STPort[Port].Create(Port,ErrorText,ErrorTitle,true);
            if(!STPort[Port].Create(Port,ErrorText,ErrorTitle,true))
            {
              STPort[Port].MIDIOutDevice=
#if defined(SSE_DIRECTMIDI)
                (OPTION_DIRECTMUSIC)?(-1): //?
#endif
                (-2);
              SendMessage((HWND)lPar,CB_SETCURSEL,0,0);
            }
          }
        }
        break;
      case IDC_MIDIINPUT:
        if(wpar_hi==CBN_SELENDOK)
        {
#if defined(SSE_DIRECTMIDI)
          int NewDevice=(OPTION_DIRECTMUSIC)?(int)CBGetSelectedItemData((HWND)lPar):((int)cursel-1);
#else
          int NewDevice=(int)cursel-1;
#endif
          if(NewDevice!=STPort[Port].GetMIDIInDeviceID())
          {
            STPort[Port].MIDIInDevice=NewDevice;
            STPort[Port].Create(Port,ErrorText,ErrorTitle,true);
            if(!STPort[Port].Create(Port,ErrorText,ErrorTitle,true))
            {
              STPort[Port].MIDIInDevice=-1; //?
              SendMessage((HWND)lPar,CB_SETCURSEL,0,0);
            }
          }
        }
        break;
      case IDC_PARALLELOUTPUT:
        if(wpar_hi==CBN_SELENDOK)
        {
          STPort[Port].LPTNum=(int)cursel;
          STPort[Port].Create(Port,ErrorText,ErrorTitle,true);
        }
        break;
      case IDC_COMOUTPUT:
        if(wpar_hi==CBN_SELENDOK)
        {
          STPort[Port].COMNum=(int)cursel;
          STPort[Port].Create(Port,ErrorText,ErrorTitle,true);
        }
        break;
      case IDC_FILECHANGE:
        if(wpar_hi==BN_CLICKED)
        {
          SendMessage((HWND)lPar,BM_SETCHECK,BST_CHECKED,0);
          EasyStr CurFol=STPort[Port].File;
          EnableAllWindows(false,Win);
          char *CurName=GetFileNameFromPath(CurFol);
          if(CurName>CurFol.Text) 
            *(CurName-1)='\0';
          char *fstypes=FSTypes(1,NULL);
          EasyStr FileName=FileSelect((FullScreen)?StemWin:Win,T("Select Output File"),CurFol,
            fstypes,1,2,Port==TSTPort::PARALLEL?"txt":"bin",CurName);
          free(fstypes);
          if(FileName.NotEmpty())
          {
            STPort[Port].File=FileName;
            SendDlgItemMessage(GetDlgItem(Win,IDC_PORTSBASE+Port*100),IDC_PORTSBASE+Port*100+40,
              WM_SETTEXT,0,(LPARAM)FileName.Text);
            STPort[Port].Create(Port,ErrorText,ErrorTitle,true);
          }
          SetForegroundWindow(Win);
          EnableAllWindows(true,Win);
          SetFocus((HWND)lPar);
          SendMessage((HWND)lPar,BM_SETCHECK,0,0);
        }
        break;
      case IDC_FILERESET:
        if(wpar_hi==BN_CLICKED)
        {
          SendMessage((HWND)lPar,BM_SETCHECK,BST_CHECKED,0);
#ifndef SSE_LEAN_AND_MEAN
          Ret=Alert(T("Are you sure? This will permanently delete the\
 contents of the file."),T("Delete Contents?"),MB_ICONQUESTION|MB_YESNO);
          if(Ret==IDYES)
#endif
          {
            STPort[Port].Close();
            DeleteFile(STPort[Port].File);
            STPort[Port].Create(Port,ErrorText,ErrorTitle,true);
#if defined(SSE_PRINTER)
            if(Port==TSTPort::PARALLEL)
            {
              Printer.Close(); // close current files (if open)
              SSEConfig.PageRtf=SSEConfig.PagePbm=0; // reset counter
            }
#endif
          }
          SendMessage((HWND)lPar,BM_SETCHECK,0,0);
        }
        break;

#if defined(SSE_NETWORK)
      case IDC_IPSTRING:
        if(wpar_hi==EN_KILLFOCUS) // after edit
        {
          int IpTextLen=(int)SendMessage((HWND)lPar,WM_GETTEXTLENGTH,0,0);
          EasyStr NewString;
          if(IpTextLen)
          {
            IpTextLen++;
            NewString.SetLength(IpTextLen);
            SendMessage((HWND)lPar,WM_GETTEXT,IpTextLen,(LPARAM)NewString.Text);
          }
          if(strcmp(NewString.Text,STPort[Port].sIPAddr.Text))
          {
            STPort[Port].sIPAddr=NewString;
            //TRACE_LOG("IP %s\n",STPort[Port].sIPAddr.Text);
          }
        }
        break;
      case IDC_IPPORT0:
      case IDC_NCLIENTS0:
        if(wpar_hi==EN_KILLFOCUS) // TODO could generalise this
        {
          char buf[16];
          SendMessage((HWND)lPar,WM_GETTEXT,12,(LPARAM)buf);
          DigitsOnly(buf);
          switch(wpar_lo) {
          case IDC_IPPORT0:
            STPort[Port].IPPort=(WORD)atoi(buf);
            break;
          case IDC_NCLIENTS0:
            STPort[Port].MaxClients=(BYTE)atoi(buf);
            break;
          }
          //TRACE_LOG("IP port %d\n",STPort[Port].IPPort);
        }
        break;
#endif//#if defined(SSE_NETWORK)

      }//sw

    }
#ifndef SSE_NO_OSD
    else if(wpar_lo>=IDC_DISKLIGHT&&wpar_lo<IDC_DISKLIGHT+100)
    {
      if(wpar_hi==BN_CLICKED) switch(wpar_lo) {
      case IDC_DISKLIGHT:
        OsdControl.show_disk_light=!OsdControl.show_disk_light;
        SendMessage((HWND)lPar,BM_SETCHECK,OsdControl.show_disk_light,0);
        break;
      case IDC_TRACKINFO:
        OPTION_DRIVE_INFO=!OPTION_DRIVE_INFO;
        SendMessage((HWND)lPar,BM_SETCHECK,OPTION_DRIVE_INFO,0);
        break;
      case IDC_OSD_SCROLLERS:
        OsdControl.show_scrollers=!OsdControl.show_scrollers;
        SendMessage((HWND)lPar,BM_SETCHECK,OsdControl.show_scrollers,0);
        if(OsdControl.show_scrollers)
          OsdControl.ScrollerPhase=TOsdControl::WANT_SCROLLER;
        else
          OsdControl.ScrollerPhase=TOsdControl::NO_SCROLLER;
        break;
      case IDC_OSD_JOKES:
        OsdControl.show_jokes=!OsdControl.show_jokes;
        SendMessage((HWND)lPar,BM_SETCHECK,OsdControl.show_jokes,0);
        if(OsdControl.show_jokes)
          OsdControl.ScrollerPhase=TOsdControl::WANT_SCROLLER;
        else
          OsdControl.ScrollerPhase=TOsdControl::NO_SCROLLER;
        break;
      case IDC_NOOSD:
        This->ChangeOSDDisable(!OsdControl.disable);
        break;
      }//sw//if
      else if(wpar_lo>=IDC_OSDSECONDS&&wpar_lo<IDC_OSDSECONDS+10 && wpar_hi==CBN_SELENDOK)
      {
        BYTE *p_element[4]={&OsdControl.show_plasma,&OsdControl.show_speed,
                            &OsdControl.show_icons,&OsdControl.show_cpu};
        *(p_element[wpar_lo-IDC_OSDSECONDS])=(BYTE)CBGetSelectedItemData((HWND)lPar);
        osd_init_run(false);
      }
    }
#endif//#ifndef SSE_NO_OSD
#if defined(SSE_GEM_CONTROL_PANEL)
      // click on colour
    if(wpar_lo>=IDC_COLOUR&&wpar_lo<IDC_COLOUR+PAL_SIZE)
    {
      HWND hBut=GetDlgItem(Win,This->CurrentColour+IDC_COLOUR);
      This->CurrentColour=(BYTE)(wpar_lo-IDC_COLOUR);
      ASSERTB(This->CurrentColour<PAL_SIZE);
      InvalidateRect(hBut,NULL,TRUE); // former colour not selected
      This->UpdateColour(This->CurrentColour);
    }
    else if(wpar_lo>=IDC_CLICK&&wpar_lo<=IDC_BELL)
    {
      WORD bit=wpar_lo-IDC_CLICK;
      BYTE conterm=SafePeek(0x000484);
      if(wpar_hi==BN_CLICKED)
      {
        BYTE mask=1<<bit;
        conterm^=mask; // toggle bit
        SafePoke(0x000484,conterm);
        InvalidateRect(GetDlgItem(Win,wpar_lo),NULL,FALSE); // because of the order of messages
      }
    }
#endif
#if defined(SSE_GUI_EMUCONTROL)
    else if(wpar_lo>=IDC_DBI0&&wpar_lo<=IDC_TRACKBYTES0) // spinners
    {
      if(wpar_hi==EN_CHANGE)
      {
        char buf[32];
        SendMessage((HWND)lPar,WM_GETTEXT,32,(LPARAM)buf);
        DigitsOnly(buf);
        switch(wpar_lo) {
        case IDC_DBI0:
          SSEOptions.dbi=(char)atoi(buf);
          break;
        case IDC_MFPSTARTCPU0:
          SSEOptions.MfpStartCpu=(char)atoi(buf);
          break;
        case IDC_MFPSTARTTCLK0:
          SSEOptions.MfpStartTclk=(char)atoi(buf);
          break;
        case IDC_MFPSTOPCPU0:
          SSEOptions.MfpStopCpu=(char)atoi(buf);
          break;
        case IDC_MFPSTOPTCLK0:
          SSEOptions.MfpStopTclk=(char)atoi(buf);
          break;
        case IDC_MFPIRQCPU0:
          SSEOptions.MfpIrqCpu=(char)atoi(buf);
          break;
        case IDC_MFPIRQTCLK0:
          SSEOptions.MfpIrqTclk=(char)atoi(buf);
          break;
        case IDC_MFPREADCPU0:
          SSEOptions.MfpReadCpu=(char)atoi(buf);
          break;
        case IDC_MFPREADTCLK0:
          SSEOptions.MfpReadTclk=(char)atoi(buf);
          break;
        case IDC_MFPTBCPU0:
          SSEOptions.MfpTbCpu=(char)atoi(buf);
          break;
        case IDC_MFPTBTCLK0:
          SSEOptions.MfpTbTclk=(char)atoi(buf);
          break;
        case IDC_MFPWSTMG0A:case IDC_MFPWSTMG1A:case IDC_MFPWSTMG2A:case IDC_MFPWSTMG3A:
          SSEOptions.MfpWsTmg[wpar_lo-IDC_MFPWSTMG0A]=(BYTE)atoi(buf);
          break;
        case IDC_MAXTRACK0:
          SSEOptions.DiscMaxTrack=(BYTE)atoi(buf);
          break;
        case IDC_DRIVERPM0:
          SSEOptions.DriveRpm=(WORD)atoi(buf);
          break;
        case IDC_TRACKBYTES0:
          SSEOptions.TrackBytes=(WORD)atoi(buf);
          break;
        }//sw
      }
      break;
    }
    else switch(wpar_lo) {
      // checkboxes
    case IDC_MFPSTARTSYNC:
      if(wpar_hi==BN_CLICKED)
        SSEOptions.MfpStartSync=!SSEOptions.MfpStartSync;
      break;
    case IDC_MFPSTOPSYNC:
      if(wpar_hi==BN_CLICKED)
        SSEOptions.MfpStopSync=!SSEOptions.MfpStopSync;
      break;
    case IDC_MFPIRQSYNC:
      if(wpar_hi==BN_CLICKED)
        SSEOptions.MfpIrqSync=!SSEOptions.MfpIrqSync;
      break;
    case IDC_MFPTBSYNC:
      if(wpar_hi==BN_CLICKED)
        SSEOptions.MfpTbSync=!SSEOptions.MfpTbSync;
      break;
    case IDC_MFPREADSYNC:
      if(wpar_hi==BN_CLICKED)
        SSEOptions.MfpReadSync=!SSEOptions.MfpReadSync;
      break;
    case IDC_SPURIOUS:
      if(wpar_hi==BN_CLICKED)
        OPTION_SPURIOUS=!OPTION_SPURIOUS;
      break;
    case IDC_BLOCKINTERRUPTS:
      if(wpar_hi==BN_CLICKED)
        SSEOptions.BlockInterrrupts=!SSEOptions.BlockInterrrupts;
      break;
    case IDC_FUZZYBITS:
      if(wpar_hi==BN_CLICKED)
        SSEOptions.FuzzyBits=!SSEOptions.FuzzyBits;
      break;
    case IDC_RANDOMIZETRACK:
      if(wpar_hi==BN_CLICKED)
        SSEOptions.RandomizeTrack=!SSEOptions.RandomizeTrack;
      break;
#if !defined(SSE_GUI_421_REORDER)
    case IDC_GHOSTDISKRO:
      if(wpar_hi==BN_CLICKED)
        SSEOptions.GhostDiskRO=!SSEOptions.GhostDiskRO;
      break;
#else
    //case IDC_FORCEDISKDD:
    case IDC_BADDMA:
      if(wpar_hi==BN_CLICKED)
        //SSEOptions.ForceDiskDD=!SSEOptions.ForceDiskDD;
        SSEOptions.BadDma=!SSEOptions.BadDma;
      break;
#endif
    case IDC_TRACKINGVIDEOCOUNTER:
      if(wpar_hi==BN_CLICKED)
      {
        SSEOptions.TrackVC=!SSEOptions.TrackVC;
        Glue.Restore(); // for live effect
      }
      break;
    case IDC_BLOCKPAL:
      if(wpar_hi==BN_CLICKED)
        SSEOptions.BlockPal=!SSEOptions.BlockPal;
      break;
    case IDC_ROUNDWRITEVC:
      if(wpar_hi==BN_CLICKED)
        SSEOptions.RoundWriteVC=!SSEOptions.RoundWriteVC;
      break;
    case IDC_ROUNDWRITESM:
      if(wpar_hi==BN_CLICKED)
        SSEOptions.RoundWriteSM=!SSEOptions.RoundWriteSM;
      break;
    }//sw
#endif//#if defined(SSE_GUI_EMUCONTROL)
    break;

#if defined(SSE_GUI_DRAG_PAGES)
  case WM_MOUSEMOVE: // TODO: an "insert" symbol would be better?
  {
    HWND hwndTV=This->hDragFrom;
    SHORT x=GET_X_LPARAM(lPar),y=GET_Y_LPARAM(lPar);
    HTREEITEM htiTarget;  // Handle to target item. 
    TVHITTESTINFO tvht;   // Hit test information. 

    if (This->DragEntered) 
    { 
       // Drag the item to the current position of the mouse pointer. 
       // First convert the dialog coordinates to control coordinates. 
       POINT point;
       point.x = x;
       point.y = y;
       ClientToScreen(Win, &point);
       ScreenToClient(hwndTV, &point);
       ImageList_DragMove(point.x, point.y);
       // Turn off the dragged image so the background can be refreshed.
       ImageList_DragShowNolock(FALSE); 
                
       // Find out if the pointer is on the item. If it is, 
       // highlight the item as a drop target. 
       tvht.pt.x = point.x;
       tvht.pt.y = point.y;
       if((htiTarget = TreeView_HitTest(hwndTV,&tvht)) != NULL)
       {
         TreeView_SelectDropTarget(hwndTV,htiTarget);
       }
       ImageList_DragShowNolock(TRUE);
    } 
    break;
  }
  case WM_LBUTTONUP:
    if(This->DragEntered)
    {
      HWND hwndTV=This->hDragFrom; // as recorded
      // Get destination item.
      HTREEITEM htiDest = TreeView_GetDropHilight(hwndTV);
      if(htiDest != NULL)
      {
        // To do: handle the actual moving of the dragged node.
        
        //get info from source
        TV_ITEM Inf;
        Inf.mask=TVIF_HANDLE|TVIF_IMAGE;
        Inf.stateMask=0xffffffff;
        Inf.pszText=NULL;
        Inf.cchTextMax=0;
        Inf.hItem=This->hTreeItemFrom; // as recorded
        TreeView_GetItem(This->PageTree,&Inf);
        int IconNumFrom=(int)Inf.lParam;
        //get info from dest
        Inf.hItem=htiDest;
        TreeView_GetItem(This->PageTree,&Inf);
        int IconNumTo=(int)Inf.lParam;
        // get source caption and index
        char *t="ERROR";
        int orderfrom=-1;
        for(int k=0;k<MAX_OPTION_LABELS;k++)
        {
          if(This->OptionLabel[k].IconNum==IconNumFrom)
          {
            t=This->OptionLabel[k].Caption;
            orderfrom=k;
          }
        }
        ASSERTB(orderfrom>-1);
        // addlabel after target
        //int i=IconNumFrom;
        TV_INSERTSTRUCT tvis;
        tvis.hParent=TVI_ROOT;
        tvis.hInsertAfter=htiDest;//TVI_LAST;
        tvis.item.mask=TVIF_TEXT|TVIF_PARAM|TVIF_IMAGE|TVIF_SELECTEDIMAGE;
        //tvis.item.pszText=T(t).Text; // BCC memory is trashed right after this
        EasyStr tr=T(t); // keep object around
        tvis.item.pszText=tr.Text;
        tvis.item.lParam=IconNumFrom;
        tvis.item.iImage=tvis.item.iSelectedImage=IconNumFrom;
        HTREEITEM new_h=TreeView_InsertItem(This->PageTree,&tvis);//same
        ASSERTB(new_h);
        // delete source
        TreeView_DeleteItem(This->PageTree, This->hTreeItemFrom);
        // update OptionLabel[]
        int orderto=-1;
        for(int k=0;k<MAX_OPTION_LABELS;k++)
        {
          if(This->OptionLabel[k].IconNum==IconNumTo)
          {
            t=This->OptionLabel[k].Caption;
            orderto=k;
            break;
          }
        }
        ASSERTB(orderto>-1);
        TOptionLabel tmp=This->OptionLabel[orderfrom];
        // remove
        for(int k=orderfrom;k<MAX_OPTION_LABELS-1;k++)
          This->OptionLabel[k]=This->OptionLabel[k+1];
        //insert
        for(int k=MAX_OPTION_LABELS-2;k>=orderto;k--)
          This->OptionLabel[k+1]=This->OptionLabel[k];
        This->OptionLabel[orderto]=tmp;
      }
      ImageList_EndDrag();
      TreeView_SelectDropTarget(hwndTV,NULL);
      ReleaseCapture();
      ShowCursor(TRUE);
      This->DragEntered = FALSE;
    }
    break;
#endif//#if defined(SSE_GUI_DRAG_PAGES)

  case WM_NOTIFY:
  {
    NMHDR *pnmh=(NMHDR*)lPar;
    if(wPar==IDC_PAGETREE)
    {
      switch(pnmh->code) {
      case TVN_SELCHANGING:
      {
        NM_TREEVIEW *Inf=(NM_TREEVIEW*)lPar;
        if(Inf->action==4096)
        { //DODGY!!!!!! Undocumented!!!!!
          return true;
        }
        return 0;
      }
      case TVN_SELCHANGED:
      {
        NM_TREEVIEW *Inf=(NM_TREEVIEW*)lPar;
        if(Inf->itemNew.hItem)
        {
          TV_ITEM tvi;
          tvi.mask=TVIF_PARAM;
          tvi.hItem=(HTREEITEM)Inf->itemNew.hItem;
          SendMessage(This->PageTree,TVM_GETITEM,0,(LPARAM)&tvi);
          This->DestroyCurrentPage();
          This->Page=(int)tvi.lParam;
          This->CreatePage(This->Page);
        }
        break;
      }
#if defined(SSE_GUI_DRAG_PAGES)
      case TVN_BEGINDRAG:
      {
        HWND hwndTV=((LPNMHDR)lPar)->hwndFrom;
        This->hDragFrom=hwndTV; // record handle...
        LPNMTREEVIEW lpnmtv=(LPNMTREEVIEW)lPar;
        HIMAGELIST himl;    // handle to image list 
        RECT rcItem;        // bounding rectangle of item 

        // Tell the tree-view control to create an image to use 
        // for dragging. 
        himl = TreeView_CreateDragImage(hwndTV,lpnmtv->itemNew.hItem);

        // Get the bounding rectangle of the item being dragged. 
        TreeView_GetItemRect(hwndTV,lpnmtv->itemNew.hItem,&rcItem,TRUE);

        This->hTreeItemFrom=lpnmtv->itemNew.hItem; // record handle...

        // Start the drag operation. 
        ImageList_BeginDrag(himl,0,0,0);
        ImageList_DragEnter(hwndTV,lpnmtv->ptDrag.x,lpnmtv->ptDrag.x);

        // Hide the mouse pointer, and direct mouse input to the 
        // parent window. 
        ShowCursor(FALSE);
        SetCapture(GetParent(hwndTV));
        This->DragEntered=TRUE;
        break;
      }

#endif
      }//sw
    }
    else if(wPar==IDC_CONFIGLISTVIEW)
    {
      if(!IsWindowEnabled(pnmh->hwndFrom)) 
        break;
      if(pnmh->code==LVN_ITEMCHANGED)
      {
        NM_LISTVIEW *pLV=(NM_LISTVIEW*)lPar;
        if(pLV->uChanged & LVIF_STATE)
        {
          LV_ITEM lvi;
          lvi.iItem=pLV->iItem;
          lvi.iSubItem=0;
          lvi.mask=LVIF_PARAM|LVIF_STATE;
          lvi.stateMask=LVIS_STATEIMAGEMASK;
          SendMessage(pnmh->hwndFrom,LVM_GETITEM,0,(LPARAM)&lvi);
          WriteCSFInt("ProfileSections",ProfileSection[pLV->iItem].Name,lvi.state,This->ProfileSel);
        }
      }
    }
    break;
  }
#if defined(SSE_GEM_CONTROL_PANEL)
  case WM_VSCROLL:
#endif
  case WM_HSCROLL:
    switch(int ID=GetDlgCtrlID((HWND)lPar)) {
    case IDC_BRIGHTNESS:
    case IDC_CONTRAST:
    case IDC_GAMMA:
    case (IDC_GAMMA+1):
    case (IDC_GAMMA+2):
    {
      col_brightness=(short)SendDlgItemMessage(Win,IDC_BRIGHTNESS,TBM_GETPOS,0,0);
      col_contrast=(short)SendDlgItemMessage(Win,IDC_CONTRAST,TBM_GETPOS,0,0);
      for(int i=0;i<3;i++)
      {
        int j=OPTION_ADVANCED ? i : 0;
        col_gamma[i]=(short)SendDlgItemMessage(Win,IDC_GAMMA+j,TBM_GETPOS,0,0);
      }
      make_palette_table(col_brightness,col_contrast);
      if(!flashlight_flag) 
        palette_convert_all();
      This->DrawBrightnessBitmap(This->hBrightBmp);
      HWND hBrightnessMap=GetDlgItem(Win,ID_BRIGHTNESS_MAP);
      InvalidateRect(hBrightnessMap,NULL,TRUE);
#ifdef SSE421R1
      if(runstate==RUNSTATE_STOPPED)
        draw(false);
#endif
      //UpdateWindow(hBrightnessMap); //overkill anyway
      break;
    }
    case IDC_SLOWSPEED:
      slow_motion_speed=(int)(SendDlgItemMessage(Win,1001,TBM_GETPOS,0,0)+1)*10;
      SendDlgItemMessage(Win,1000,WM_SETTEXT,0,(LPARAM)((T("Slow motion speed")
        +": "+(slow_motion_speed/10)+"%").Text));
      break;
    case IDC_MOUSESPEED:
      mouse_speed=(BYTE)SendMessage((HWND)lPar,TBM_GETPOS,0,0);
      break;
#if defined(SSE_YM2149_LL)
    case IDC_NEWFILTER:
    {
      OPTION_LOWPASS=(WORD)SendMessage((HWND)lPar,TBM_GETPOS,0,0);
#if defined(SSE_EMU_THREAD)
      if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
      {  // change live!
        SoundLock.Lock();
        if(Psg.AntiAlias)
          delete Psg.AntiAlias;
        Psg.AntiAlias=NULL;
        if(OPTION_LOWPASS<YM_LOW_PASS_MAX)
          Psg.AntiAlias=new Filter(LPF,51,250.0,(double)OPTION_LOWPASS/1000);
        SoundLock.Unlock();
      }
#endif
      break;
    }
#endif
    case IDC_FASTFWD:
    {
      fast_forward_max_speed=1000/(int)(SendDlgItemMessage(Win,ID,TBM_GETPOS,0,0)+2);
      Str Text=T("Maximum fast forward speed")+": ";
      if(fast_forward_max_speed>50)
        Text+=Str((1000/fast_forward_max_speed)*100)+"%";
      else
      {
        Text+=T("Unlimited");
        fast_forward_max_speed=0;
      }
      SendDlgItemMessage(Win,IDC_FASTFWDTXT,WM_SETTEXT,0,(LPARAM)Text.Text);
      break;
    }
    case IDC_SPEED:
    {
      run_speed_ticks_per_second=100000/(5+(int)SendDlgItemMessage(Win,ID,TBM_GETPOS,0,0)*5);
      SendDlgItemMessage(Win,IDC_SPEEDTXT,WM_SETTEXT,0,
                         (LPARAM)(T("Run speed")+": "+(100000/run_speed_ticks_per_second)+"%").Text);
      REFRESH_STATUS_BAR_GX;
      break;
    }
    case IDC_SOUNDVOL:
    {
      int position=(int)SendMessage((HWND)lPar,TBM_GETPOS,0,0)+1;
      int db=(int)-(10000-10000*log10((float)position)/log10((float)101));
      SoundVolume=db;
      SoundChangeVolume();
      break;
    }
#if defined(SSE_DRIVE_SOUND)
    case IDC_DRIVESOUNDVOL:
    {
      int position=(int)SendMessage((HWND)lPar,TBM_GETPOS,0,0)+1;
      int db=(int)-(10000-10000*log10((float)position)/log10((float)101));
      FloppyDrive[DRIVE_A].SoundVolume=FloppyDrive[DRIVE_B].SoundVolume=db;
      FloppyDrive[DRIVE_A].SoundChangeVolume();
      FloppyDrive[DRIVE_B].SoundChangeVolume();
      break;
    }
#endif
#if !defined(SSE_GUI_EMUCONTROL)
    case IDC_MFPXTAL:
    {
      Mfp.xtal=(DWORD)SendMessage((HWND)lPar,TBM_GETPOS,0,0);
      char stmp[2][64];
      InsertCommas(stmp[0],Mfp.xtal);
      sprintf(stmp[1], "%s %s %sHz","MFP","XTAL",stmp[0]);
      SetWindowText(GetDlgItem(Win,IDC_MFPXTAL_S),stmp[1]);
      CpuMfpRatio=(double)CpuNormalHz/(double)Mfp.xtal;
      break;
    }
    case IDC_CPU_CLOCK:
    {
      char stmp[2][64];
      DWORD displayHz=(DWORD)SendMessage((HWND)lPar,TBM_GETPOS,0,0);
      InsertCommas(stmp[0],displayHz);
      sprintf(stmp[1], "System %s %sHz","XTAL",stmp[0]);
      SetWindowText(GetDlgItem(Win,IDC_CPU_CLOCK_S),stmp[1]);
      if(wpar_lo==SB_ENDSCROLL)
      {
        CpuCustomHz=(DWORD)(displayHz/4)*TICKS8;
        CpuNormalHz=CpuCustomHz;
        nSysCyclesPerSecond=CpuNormalHz;
        CpuMfpRatio=(double)CpuCustomHz/(double)Mfp.xtal;
        if(SSEConfig.CpuBoost>1)
          AdaptCpuBoost();
        // update CPU speed control
        HWND win=GetDlgItem(This->Handle,IDC_CPU_SPEED);
        SendMessage(win,CB_SETITEMDATA,0,nSysCyclesPerSecond/TICKS8);
        This->MachineUpdateIfVisible();
      }
      break;
    }
#endif
    case IDC_MIDIVOL:
      MIDI_out_volume=(WORD)SendMessage((HWND)lPar,TBM_GETPOS,0,0);
      if(runstate==RUNSTATE_RUNNING)
      {
        for(int i=0;i<3;i++)
          if(STPort[i].MIDI_Out) 
            STPort[i].MIDI_Out->SetVolume(MIDI_out_volume);
      }
      break;
    case IDC_MIDISPEED:
      MIDI_in_speed=(int)SendMessage((HWND)lPar,TBM_GETPOS,0,0);//+1;
      SendDlgItemMessage(Win,IDS_MIDISPEED,WM_SETTEXT,0,
                         (LPARAM)(T("Input speed")+": "+Str(MIDI_in_speed)+"%").Text);
      break;
#if defined(SSE_GEM_CONTROL_PANEL)
    case IDC_RGB:
    case (IDC_RGB+1):
    case (IDC_RGB+2):
    {
      int max_v=(int)SendDlgItemMessage(Win,ID,TBM_GETRANGEMAX,0,0);
      int val=max_v-(int)SendDlgItemMessage(Win,ID,TBM_GETPOS,0,0);
      char txt[32];
      char rgb[3]={'R','G','B'};
      DWORD ix=ID-IDC_RGB;
      ASSERTB(ix<3);
      DWORD shift=2-ix;
      sprintf(txt,"%c%d",rgb[ix],val);
      //ASSERT(IDC_RGB==IDS_RGB+3);
      SetWindowText(GetDlgItem(Win,ID-3),txt);
      // update ST palette
      ASSERTB(This->CurrentColour<PAL_SIZE);
      WORD dat=STpal[This->CurrentColour];
      WORD mask=(0xF<<(shift*4));
      dat&=~mask;
      if(max_v==7)
        val<<=1;
      val=(val>>1) | ((val&1)<<3); // normal to STE
      val<<=(shift*4);
      dat|=val;
      STpal[This->CurrentColour]=dat;
      PAL_DPEEK(This->CurrentColour*2)=dat;
      palette_convert(This->CurrentColour);
      //if(wpar_lo==SB_ENDSCROLL)
      {
        HWND hBut=GetDlgItem(Win,This->CurrentColour+IDC_COLOUR);
        if(hBut)
          InvalidateRect(hBut,NULL,TRUE);
      }
      break;
    }
    case IDC_REPEAT_DELAY:
    case IDC_REPEAT_RATE:
    {
      //ASSERT(Mess==WM_HSCROLL);
      BYTE val=(BYTE)SendMessage((HWND)lPar,TBM_GETPOS,0,0);//
      if(val)
        SafePoke(This->TosKeyRepeat+ID-IDC_REPEAT_DELAY,val);
      break;
    }
    case IDC_BASS:
      Microwire.bass=(BYTE)SendDlgItemMessage(Win,ID,TBM_GETPOS,0,0);
      break;
    case IDC_TREBLE:
      Microwire.treble=(BYTE)SendDlgItemMessage(Win,ID,TBM_GETPOS,0,0);
      break;
    case IDC_PSGREDUCE:
#if defined(SSE_GUI_421_REORDER)
      Microwire.PsgReduce=16-(BYTE)SendDlgItemMessage(Win,ID,TBM_GETPOS,0,0);
#else
      Microwire.PsgReduce=(BYTE)SendDlgItemMessage(Win,ID,TBM_GETPOS,0,0);
#endif
      if(OPTION_SAMPLED_YM)
      {
#if defined(SSE_EMU_THREAD)
        if(OPTION_EMUTHREAD && runstate==RUNSTATE_RUNNING)
          SoundLock.Lock();
#endif
        Psg.LoadFixedVolTable(); // reload to adapt
#if defined(SSE_EMU_THREAD)
        SoundLock.Unlock();
#endif
      }
      break;
#endif
#if defined(SSE_GUI_421_REORDER)
    case IDC_MWVOLUME:
      Microwire.volume=(BYTE)SendDlgItemMessage(Win,ID,TBM_GETPOS,0,0);
      break;
    case IDC_MWBALANCE:
    {
      char b=(char)SendDlgItemMessage(Win,ID,TBM_GETPOS,0,0);
      if(b<0) // there must be a clever way...
      {
        Microwire.volume_l=0x14;
        Microwire.volume_r=0x14+b;
      }
      else
      {
        Microwire.volume_l=0x14-b;
        Microwire.volume_r=0x14;
      }
      break;
    }
#endif
#if defined(SSE_GUI_421_REORDER)
    case IDC_DMAMULTI:
      Microwire.DmaMulti=(BYTE)SendDlgItemMessage(Win,ID,TBM_GETPOS,0,0);
      break;
#endif
#if defined(SSE_GUI_EMUCONTROL)
    case IDC_CPU_CLOCK:
    {
      char stmp[2][64];
      DWORD displayHz=(DWORD)SendMessage((HWND)lPar,TBM_GETPOS,0,0);
      if(wpar_lo==SB_ENDSCROLL)
      {
        //CpuCustomHz=(DWORD)(displayHz/4)*TICKS8;
        CpuCustomHz=displayHz;
        CpuNormalHz=CpuCustomHz;
        nSysCyclesPerSecond=CpuNormalHz;
        CpuMfpRatio=(double)CpuCustomHz/(double)Mfp.xtal;
        if(SSEConfig.CpuBoost>1)
          AdaptCpuBoost();
      }
      InsertCommas(stmp[0],displayHz);

#ifdef SSE_TIMINGS32
      //sprintf(stmp[1], "System %sHz CPU %d",stmp[0],nSysCyclesPerSecond/TICKS8);
      sprintf(stmp[1], "CLK32 %s\n CLK8 %d",stmp[0],nSysCyclesPerSecond/TICKS8);
#else
      //sprintf(stmp[1], "CPU clock %sHz",stmp[0]);
      sprintf(stmp[1], "CLK8 %s",stmp[0]);
#endif

      //sprintf(stmp[1], "System %sHz",stmp[0]);
      SetWindowText(GetDlgItem(Win,IDC_CPU_CLOCK_S),stmp[1]);
      break;
    }
    case IDC_MFPXTAL:
    {
      Mfp.xtal=(DWORD)SendMessage((HWND)lPar,TBM_GETPOS,0,0);
      char stmp[2][64];
      InsertCommas(stmp[0],Mfp.xtal);
      sprintf(stmp[1], "MFP %s %sHz","XTAL",stmp[0]);
      SetWindowText(GetDlgItem(Win,IDC_MFPXTAL_S),stmp[1]);
      CpuMfpRatio=(double)CpuNormalHz/(double)Mfp.xtal;
      break;
    }
    case IDC_LOWSHELF:
      Microwire.LowShelf=(DWORD)SendMessage((HWND)lPar,TBM_GETPOS,0,0);
      break;
    case IDC_HIGHSHELF:
      Microwire.HighShelf=(DWORD)SendMessage((HWND)lPar,TBM_GETPOS,0,0);
      break;
#endif//#if defined(SSE_GUI_EMUCONTROL)
    }//sw
    break;
  case WM_DRAWITEM:
  {
    DRAWITEMSTRUCT* di=(DRAWITEMSTRUCT*)lPar;
    if(wPar==IDC_TOSLIST)
    {
      COLORREF oldtextcol=GetTextColor(di->hDC),oldmode=GetBkMode(di->hDC);
      HBRUSH br;
      if(di->itemState & ODS_SELECTED)
      {
        br=CreateSolidBrush(GetSysColor(COLOR_HIGHLIGHT));
        SetTextColor(di->hDC,GetSysColor(COLOR_HIGHLIGHTTEXT));
      }
      else
      {
        br=CreateSolidBrush(GetSysColor(COLOR_WINDOW));
        SetTextColor(di->hDC,GetSysColor(COLOR_WINDOWTEXT));
      }
      SetBkMode(di->hDC,TRANSPARENT);
      FillRect(di->hDC,&(di->rcItem),br);
      DeleteObject(br);
      if(di->itemID<0xffffffff)
      {
        int idx=di->itemID;
        if(This->eslTOS_Descend)
          idx=This->eslTOS.NumStrings-1-di->itemID;
        WORD Ver=(WORD)This->eslTOS[idx].Data[0];
        WORD Lang=(WORD)This->eslTOS[idx].Data[1];
        BYTE Recognised=(BYTE)(This->eslTOS[idx].Data[1]>>16); //!
        WORD Date=(WORD)This->eslTOS[idx].Data[2];
        Str Text=This->eslTOS[idx].String;
        char* NameEnd=strchr(Text,'\01');
        if(NameEnd)
          *NameEnd='\0';
        RECT shiftrect=di->rcItem;shiftrect.left+=2;
        int Right=shiftrect.right;
        shiftrect.right=shiftrect.left+60;
        Str szVer=HEXSl(Ver,3).Insert(".",1);
        DrawText(di->hDC,szVer,(int)strlen(szVer),&shiftrect,
          DT_LEFT|DT_SINGLELINE|DT_VCENTER);
        shiftrect.right=shiftrect.left+GetTextSize((HFONT)GetCurrentObject
        (di->hDC,OBJ_FONT),szVer).Width+2;
        HDC TempDC=CreateCompatibleDC(di->hDC);
        HANDLE OldBmp=SelectObject(TempDC,LoadBitmap(hInstance,"TOSFLAGS"));
        int FlagIdx=This->TOSLangToFlagIdx(Lang);
        if(FlagIdx>=0)
        {
          BitBlt(di->hDC,shiftrect.right,shiftrect.top+((shiftrect.bottom-
            shiftrect.top)/2)-RC_FLAG_HEIGHT/2,RC_FLAG_WIDTH,RC_FLAG_HEIGHT,
            TempDC,FlagIdx*RC_FLAG_WIDTH,0,SRCCOPY);
        }
        DeleteObject(SelectObject(TempDC,OldBmp));
        DeleteDC(TempDC);
        shiftrect.left+=60;shiftrect.right=Right-105;
        COLORREF colour;
        switch(Recognised) {
        case 1:
          colour=RGB(0,200,0);
          break;
        case 0xFE: //not compatible
          colour=RGB(200,100,0);;
          break;
        case 0xFF: //bad
          colour=RGB(200,0,0);
          break;
        default:
          colour=0; //black
        }
        SetTextColor(di->hDC,colour);
        DrawText(di->hDC,Text,(int)strlen(Text),&shiftrect,DT_LEFT|DT_SINGLELINE|DT_VCENTER);
        if(Recognised)
          SetTextColor(di->hDC,oldtextcol);
        FILETIME ft;
        if(DosDateTimeToFileTime(Date,0,&ft))
        {
          SYSTEMTIME st;
          FileTimeToSystemTime(&ft,&st);
          Str szDate=(USDateFormat) ? Str(st.wDay)+"/"+st.wMonth+"/"+st.wYear
                                    : Str(st.wMonth)+"/"+st.wDay+"/"+st.wYear;
          shiftrect.left=Right-100;shiftrect.right=Right;
          DrawText(di->hDC,szDate,(int)strlen(szDate),&shiftrect,DT_LEFT|DT_SINGLELINE|DT_VCENTER);
        }
      }
      SetTextColor(di->hDC,oldtextcol);
      SetBkMode(di->hDC,oldmode);
      if(di->itemState & ODS_FOCUS)
        DrawFocusRect(di->hDC,&(di->rcItem));
      return 0;
    }
#if defined(SSE_GEM_CONTROL_PANEL) //TODO optimise
    else if(wPar>=IDS_RABBIT&&wPar<=IDS_MOUSEFAST)
    {
      HDC TempDC=CreateCompatibleDC(di->hDC);
      HANDLE OldBmp=SelectObject(TempDC,LoadBitmap(hInstance,"CONTROLPANELPICS"));
      int Idx=6+(int)wPar-IDS_RABBIT; // see STControlPanel.bmp
      BitBlt(di->hDC,0,0,32,32,TempDC,Idx*32,0,SRCCOPY);
      DeleteObject(SelectObject(TempDC,OldBmp));
      DeleteDC(TempDC);
      break;
    }
    else switch(wPar) {
    case IDC_BELL: // control panel graphics
    case IDC_REPEAT:
    case IDC_CLICK:
    case IDS_KEYDELAYLOW:
    case IDS_KEYDELAYHI:
    case IDS_KEYRATEHI:
    case IDS_KEYRATELOW:
    {
      HDC TempDC=CreateCompatibleDC(di->hDC);
      HANDLE OldBmp=SelectObject(TempDC,LoadBitmap(hInstance,"CONTROLPANELPICS"));
      int Idx;
      BYTE conterm=SafePeek(0x000484);
      switch(wPar) { // see STControlPanel.bmp
      case IDC_BELL:
        Idx=(conterm&BIT_2) ? 0 : 2;
        break;
      case IDC_REPEAT:
        Idx=(conterm&BIT_1) ? 5 : 4;
        break;
      case IDC_CLICK:
        Idx=(conterm&BIT_0) ? 1 : 3;
        break;
      default:
        Idx=4+(int)wPar-IDS_KEYDELAYLOW;
      }
      BitBlt(di->hDC,0,0,32,32,TempDC,Idx*32,0,SRCCOPY);
      DeleteObject(SelectObject(TempDC,OldBmp));
      DeleteDC(TempDC);
      break;
    }
    default:
      if(wPar>=IDS_RABBIT&&wPar<=IDS_MOUSEFAST)
      {
        HDC TempDC=CreateCompatibleDC(di->hDC);
        HANDLE OldBmp=SelectObject(TempDC,LoadBitmap(hInstance,"CONTROLPANELPICS"));
        int Idx=6+(int)wPar-IDS_RABBIT; // see STControlPanel.bmp
        BitBlt(di->hDC,0,0,32,32,TempDC,Idx*32,0,SRCCOPY);
        DeleteObject(SelectObject(TempDC,OldBmp));
        DeleteDC(TempDC);
        break;
      }
      else // coloured rectangles
      {
        int palnum=(int)wPar-IDC_COLOUR;
        ASSERTB(palnum>=0 && palnum<PAL_SIZE);
        RECT rc=di->rcItem;
        rc.top+=2,rc.left+=2,rc.bottom-=2,rc.right-=2;
        WORD dat=STpal[palnum];
        ASSERTB(dat<4096);
        dat=(WORD)(((dat&0x888)>>3)|((dat&0x777)<<1));  //fix up stupid rRRRgGGGbBBB colour pattern
        HBRUSH cl=CreateSolidBrush(RGB((dat&0xf00)>>4,(dat&0x0f0),(dat&0xf)<<4));
        FillRect(di->hDC,&rc,cl);
        DeleteObject(cl);
        FrameRect(di->hDC,&rc,(HBRUSH)GetStockObject(BLACK_BRUSH));
        if(palnum==This->CurrentColour)
          FrameRect(di->hDC,&di->rcItem,(HBRUSH)GetStockObject(BLACK_BRUSH));
        break;
      }
    }//sw
#endif
#if defined(SSE_ST_CONTROL_PANEL_) //TODO optimise
    else if(wPar>=IDS_MOUSESLOW&&wPar<=IDS_MOUSEFAST)
    {
      DRAWITEMSTRUCT* pDIS=(DRAWITEMSTRUCT*)lPar;
      RECT rc=pDIS->rcItem;
      HDC TempDC=CreateCompatibleDC(pDIS->hDC);
      HANDLE OldBmp=SelectObject(TempDC,LoadBitmap(hInstance,"CONTROLPANELPICS"));
      int Idx=6+(int)wPar-IDS_RABBIT; // see STControlPanel.bmp
      BitBlt(pDIS->hDC,0,0,32,32,TempDC,Idx*32,0,SRCCOPY);
      DeleteObject(SelectObject(TempDC,OldBmp));
      DeleteDC(TempDC);
      break;
    }
#endif
    break;
  }
  case WM_DELETEITEM:
    //      if (wPar==IDC_TOSLIST) delete (Str*)(((DELETEITEMSTRUCT*)lPar)->itemData);
    break;
  case WM_ACTIVATE:
    if(wPar!=WA_INACTIVE)
      This->DrawBrightnessBitmap(This->hBrightBmp);
    break;
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
#if defined(SSE_GEM_CONTROL_PANEL)
  case WM_SETFOCUS:
    This->MachineUpdateIfVisible(); // update keyboard icons
    return 0;
#endif
  }
  return DefWindowProc(Win,Mess,wPar,lPar);
}
#endif

int TOptionBox::DTreeNotifyProc(DirectoryTree*,void *t,int Mess,INT_PTR i1,
                                INT_PTR) {
  TOptionBox *This=(TOptionBox*)t;
  if(Mess==DTM_SELCHANGED||Mess==DTM_NAMECHANGED)
  {
    Str NewSel=DTree.GetItemPath((HTREEITEM)i1);
    int Type=DTree.GetItem((HTREEITEM)i1,TVIF_IMAGE).iImage;
    int DisableLo=0,DisableHi=0;
    switch(GetDlgCtrlID(DTree.hTree)) {
    case IDC_MACROTREE:
      This->MacroSel=NewSel;
      This->UpdateMacroRecordAndPlay(NewSel,Type);
      DisableLo=IDC_MACROTREE+10;DisableHi=IDC_MACROTREE+30;
      break;
    case IDC_CONFIGTREE:
    {
      This->ProfileSel=NewSel;
      DisableLo=IDC_CONFIGTREE+10;DisableHi=IDC_CONFIGTREE+30;
      int i=0;
      HWND LV=GetDlgItem(This->Handle,IDC_CONFIGLISTVIEW);
      EnableWindow(LV,FALSE);
      TConfigStoreFile CSF;
      if(Type==1)
        CSF.Open(This->ProfileSel);
      while(ProfileSection[i].Name)
      {
        int Check=LVI_SI_CHECKED;
        if(Type==1)
          Check=CSF.GetInt("ProfileSections",ProfileSection[i].Name,LVI_SI_CHECKED);
        ListView_SetItemState(LV,i++,Check,LVIS_STATEIMAGEMASK);
      }
      if(Type==1)
        CSF.Close();
      break;
    }
    }//sw
    for(int n=DisableLo;n<DisableHi;n++)
      if(HWND hwnd=GetDlgItem(This->Handle,n)) 
        EnableWindow(hwnd,Type);
  }
  return 0;
}

#endif//WIN32


#ifdef UNIX

/*
for unix we need to create unix objects, it's duplication
if we change an option, we must change it in the unix build too
ids are different from windows build
*/


void TOptionBox::UpdateProfileDisplay(Str Sel,int Type)
{
  if (Handle==0) return;

  hxc_button *p_grp=(hxc_button*)hxc::find(page_p,2109);
  if (p_grp==NULL) return;

  if (Sel.Empty()){
    Type=-1;
    if (dir_lv.lv.sel>=0){
      Sel=dir_lv.get_item_path(dir_lv.lv.sel);
      Type=dir_lv.sl[dir_lv.lv.sel].Data[DLVD_TYPE];
    }
  }
  ShowHideWindow(XD,p_grp->handle,Type==2);

  if (Type==2){
    TConfigStoreFile CSF;
    CSF.Open(Sel);
    int i=0;
    while (ProfileSection[i].Name){
      int icon=101+ICO16_TICKED;
      if (CSF.GetInt("ProfileSections",ProfileSection[i].Name,PROFILESECT_ON)==PROFILESECT_OFF){
        icon=101+ICO16_UNTICKED;
      }
      profile_sect_lv.sl[i++].Data[0]=icon;
    }
    CSF.Close();
    profile_sect_lv.draw(true);
  }
}
//---------------------------------------------------------------------------
void TOptionBox::UpdatePortDisplay(int p)
{
	if (PortGroup[0].handle==0) return;

	int PortIOType=GetPortIOType(STPort[p].Type);
	if (PortIOType>=0){
    if (STPort[p].Type==PORTTYPE_LAN){
      int Base=1200+p*20;
    	hxc_edit *p_ed_out=(hxc_edit*)hxc::find(LANGroup[p].handle,Base+10);
    	hxc_edit *p_ed_in=(hxc_edit*)hxc::find(LANGroup[p].handle,Base+12);
    	hxc_button *p_but_open=(hxc_button*)hxc::find(LANGroup[p].handle,Base+14);
      if (p_ed_out->text!=STPort[p].PortDev[PortIOType]){
        p_ed_out->set_text(STPort[p].PortDev[PortIOType]);
      }
      if (p_ed_in->text!=STPort[p].LANPipeIn){
        p_ed_in->set_text(STPort[p].LANPipeIn);
      }
      p_but_open->set_check(STPort[p].IsPCPort());
  	  XUnmapWindow(XD,FileGroup[p].handle);
  	  XUnmapWindow(XD,IOGroup[p].handle);
      XMapWindow(XD,LANGroup[p].handle);
    }else{
      if (IODevEd[p].text!=STPort[p].PortDev[PortIOType]){
        IODevEd[p].set_text(STPort[p].PortDev[PortIOType]);
      }
      IOAllowIOBut[p][0].set_check(STPort[p].AllowIO[PortIOType][0]);
      IOAllowIOBut[p][1].set_check(STPort[p].AllowIO[PortIOType][1]);
      IOOpenBut[p].set_check(STPort[p].IsPCPort());
      XUnmapWindow(XD,LANGroup[p].handle);
      XUnmapWindow(XD,FileGroup[p].handle);
      XMapWindow(XD,IOGroup[p].handle);
    }
	}else if (STPort[p].Type==PORTTYPE_FILE){
    XUnmapWindow(XD,LANGroup[p].handle);
	  XUnmapWindow(XD,IOGroup[p].handle);
	  XMapWindow(XD,FileGroup[p].handle);
	}else{
    XUnmapWindow(XD,LANGroup[p].handle);
	  XUnmapWindow(XD,FileGroup[p].handle);
	  XUnmapWindow(XD,IOGroup[p].handle);
	}
}


//---------------------------------------------------------------------------
int TOptionBox::WinProc(TOptionBox *This,Window Win,XEvent *Ev)
{
  switch (Ev->type){
    case ClientMessage:
      if (Ev->xclient.message_type==hxc::XA_WM_PROTOCOLS){
        if (Atom(Ev->xclient.data.l[0])==hxc::XA_WM_DELETE_WINDOW){
          This->Hide();
        }
      }
  }
  return PEEKED_MESSAGE;
}


//---------------------------------------------------------------------------
int TOptionBox::listview_notify_proc(hxc_listview* LV,int Mess,INT_PTR i)
{
  TOptionBox *This=(TOptionBox*)(LV->owner);
  if (LV->id==IDC_PAGETREE){
    if (Mess==LVN_SELCHANGE || Mess==LVN_SINGLECLICK){
      int NewPage=LV->sl[LV->sel].Data[1];
      if (This->Page!=NewPage){
        XUnmapWindow(XD,This->page_p);
        hxc::destroy_children_of(This->page_p);
        hints.remove_all_children(This->page_p);
			  This->brightness_ig.FreeIcons();
        This->Page=NewPage;
        This->CreatePage(This->Page);
        XMapWindow(XD,This->page_p);
      }
    }
  }else if (LV->id==1000){
    if (Mess==LVN_SELCHANGE || Mess==LVN_SINGLECLICK){
      This->NewROMFile=strrchr(LV->sl[LV->sel].String,'\01')+1;
      if (This->NewROMFile[0]!='/') This->NewROMFile.Insert(UsersPath+"/",0);
      if (IsSameStr_I(ROMFile,This->NewROMFile)) This->NewROMFile="";
      CheckResetIcon();
    }
  }else if (LV->id==2112){
    if (Mess==LVN_ICONCLICK){
      int icon=LV->sl[i].Data[0]-101;
      if (icon==ICO16_TICKED){
        icon=ICO16_UNTICKED;
      }else{
        icon=ICO16_TICKED;
      }
      WriteCSFInt("ProfileSections",ProfileSection[i].Name,
                  int((icon==ICO16_TICKED) ? PROFILESECT_ON:PROFILESECT_OFF),
                  This->ProfileSel);
      LV->sl[i].Data[0]=101+icon;
      LV->draw(0);
      return 1;
    }
  }else if (LV->id>=15000 && LV->id<15100){
    if (Mess==LVN_RETURN || Mess==LVN_CB_RETRACT){
      int n=(LV->id-15000)/10;
      hxc_edit *p_ed=(hxc_edit*)hxc::find(This->page_p,15000+n*10);
      hxc_button *p_but=(hxc_button*)hxc::find(This->page_p,15001+n*10);

      LV->destroy(LV);

      if (i>=0){
        p_ed->set_text(Comlines_Default[n][i]);
        p_ed->notifyproc(p_ed,EDN_CHANGE,0);
      }
      p_but->set_check(0);
    }

  }
  return 0;
}
//---------------------------------------------------------------------------
int TOptionBox::button_notify_proc(hxc_button*b,int mess,int* ip)
{
  TOptionBox *This=(TOptionBox*)(b->owner);
  if (mess==BN_CLICKED){
    if (b->id==100){ //auto load snapshot
      AutoLoadSnapShot=b->checked;
    }
    else if(b->id==101) { //never use MIT Shared Memory Extension
      WriteCSFStr("Options","NoSHM",EasyStr(b->checked),globalINIFile);
#if defined(SSE_AUTOUPDATE)
    }else if (b->id==IDC_CHECKUPDATE){
      WriteCSFStr("Update","AutoUpdateEnabled",EasyStr(b->checked),globalINIFile);
#endif
    }else if (b->id==110){
      bPauseWhenInactive=b->checked;
    }else if (b->id==120){
      HighPriority=b->checked;
    }else if (b->id==121){
      ShowTips=b->checked;
      if (ShowTips){
        hints.start();
      }else{
        hints.stop();
      }
    }else if (b->id==130){
      DiskMan.floppy_access_ff=b->checked;
    }else if (b->id==140){
      StartEmuOnClick=b->checked;
    }
    else if(b->id>=210&&b->id<=211){
      if(b->id==210)
      {
        draw_fs_fx=(b->checked ? DFSFX_GRILLE:DFSFX_NONE);
        OPTION_SCANLINES=b->checked;
      }
#if defined(SSE_VID_SINGLEPIX)      
      else if(b->id==211)
        SSEOptions.SinglePixels=b->checked;
#endif
      Draw.MarshalParameters();
      if (draw_grille_black<4) draw_grille_black=4;
      if (runstate!=RUNSTATE_RUNNING) draw(false);
    }
    else if (b->id==220){
      Disp.DoAsyncBlit=b->checked;
    }else if (b->id==230){
      ResChangeResize=b->checked;
    }else if (b->id==122){
      This->FullscreenBrightnessBitmap();
#if defined(SSE_VID_SIZE4)
    }else if (b->id>=250 && b->id<=253) {
      DISPLAY_SIZE=1+(b->id-250); // 1-4
      WinSizeForRes[LORES]=WinSizeForRes[MEDRES]=WinSizeForRes[HIRES]=DISPLAY_SIZE-1;
      SSEConfig.Size4=(DISPLAY_SIZE==4);
      for(int i=0;i<4;i++) // as radio buttons
        This->DisplaySize_but[i].set_check( (DISPLAY_SIZE==(1+i)) );
      Draw.MarshalParameters();
      if (ResChangeResize){
#if defined(SSE_VID_SIZE4)
        Disp.ScreenChange();
#endif        
        StemWinResize();
      }
      if(runstate==RUNSTATE_STOPPED)
        draw(false);
#else      
    }else if (b->id==250 || b->id==251){
      int res=(b->id)-250;
      if (b->checked){
        WinSizeForRes[res]=1;
      }else{
        WinSizeForRes[res]=0;
      }
      if (ResChangeResize && res==screen_res){
        StemWinResize();
      }
#endif      
    }else if (b->id==254){
      fileselect.set_corner_icon(&Ico16,ICO16_FOLDER);
      EasyStr Path=fileselect.choose(XD,ScreenShotFol,"",T("Pick a Folder"),
          FSM_CHOOSE_FOLDER | FSM_CONFIRMCREATE,folder_parse_routine,"");
      if (Path.NotEmpty()){
        NO_SLASH(Path);
        ScreenShotFol=Path;
        CreateDirectory(ScreenShotFol,NULL);
        This->screenshots_fol_display.set_text(ScreenShotFol);
      }
    }else if (b->id==255){
      prefer_res_640_400=b->checked;
    }else if (b->id==5100){
      This->SetSoundRecord(!bSoundRecord);
    }else if (b->id==5101){
      fileselect.set_corner_icon(&Ico16,ICO16_SOUND);
      EasyStr NewFile=fileselect.choose(XD,This->WAVOutputDir,
                GetFileNameFromPath(WAVOutputFile),T("Choose WAV Output File"),FSM_OK,
                wavfile_parse_routine,".wav");
      if (NewFile.NotEmpty()){
        WAVOutputFile=NewFile;
        This->WAVOutputDir=NewFile;
        RemoveFileNameFromPath(This->WAVOutputDir,REMOVE_SLASH);
        This->wav_output_label.set_text(NewFile);
      }
    }else if (b->id==5102){
      This->RecordWarnOverwrite=b->checked;
#if !defined(SSE_NO_INTERNAL_SPEAKER)
    }else if (b->id==5300){
      if (sound_internal_speaker) internal_speaker_sound_by_period(0);

      sound_internal_speaker=!sound_internal_speaker;
      b->set_check(sound_internal_speaker);
#endif
#if defined(SSE_DONGLE)
    }else if (b->id==IDC_CHOOSECART){
    	b->set_check(true);
      Str LastCartFol=This->LastCartFile;
      RemoveFileNameFromPath(LastCartFol,REMOVE_SLASH);

      fileselect.set_corner_icon(&Ico16,ICO16_CART);
      EasyStr fn=fileselect.choose(XD,LastCartFol,GetFileNameFromPath(This->LastCartFile),
        T("Find a Cartridge"),FSM_LOAD | FSM_LOADMUSTEXIST,cartfile_parse_routine,".stc");
      if (fn[0]){
        This->LastCartFile=fn;
        if (!load_cart(fn)){
          Alert(T("There was an error loading the cartridge."),T("Cannot Load Cartridge"),MB_ICONEXCLAMATION);
        }else{
          CartFile=fn;
          This->cart_display.set_text(fn);
        }
        CheckResetDisplay();
      }
    	b->set_check(0);
    }else if (b->id==IDC_REMOVECART){
      This->cart_display.set_text("");

      delete[] cart;
      cart=NULL;
#if defined(SSE_SOUND_CARTRIDGE)
      SSEConfig.mv16=SSEConfig.mr16=false;
#endif
#if defined(SSE_DONGLE_CUBASE2)
      SSEConfig.Cubase2Cart=false;
#endif
#if defined(SSE_DONGLE_CUBASE3)
      SSEConfig.Cubase3Cart=false;
#endif
      CartFile="";
      CheckResetDisplay();
    }else if (b->id==IDC_SWITCHCART){
      if(OPTION_CARTRIDGE_OFF)
      {
        if(cart_save)
          cart=cart_save;
        cart_save=NULL;
      }
      else
      {
        if(cart)
          cart_save=cart;
        cart=NULL;
      }
      OPTION_CARTRIDGE_OFF=!OPTION_CARTRIDGE_OFF;
      OptionBox.MachineUpdateIfVisible();
    }else if (b->id==IDC_FREEZECART){
      switch(DONGLE_ID) {
#if defined(SSE_DONGLE_URC)
      case TDongle::URC:
        mfp_gpip_set_bit(MFP_GPIP_RING_BIT,false);
        break;
#endif
#if defined(SSE_DONGLE_MULTIFACE)
      case TDongle::MULTIFACE:
        mfp_gpip_set_bit(MFP_GPIP_MONO_BIT,false);
        break;
#endif
      }//sw
#else
    }else if (b->id==737){ // Choose cart
    	b->set_check(true);
      Str LastCartFol=This->LastCartFile;
      RemoveFileNameFromPath(LastCartFol,REMOVE_SLASH);

      fileselect.set_corner_icon(&Ico16,ICO16_CART);
      EasyStr fn=fileselect.choose(XD,LastCartFol,GetFileNameFromPath(This->LastCartFile),
        T("Find a Cartridge"),FSM_LOAD | FSM_LOADMUSTEXIST,cartfile_parse_routine,".stc");
      if (fn[0]){
        This->LastCartFile=fn;
        if (!load_cart(fn)){
          Alert(T("There was an error loading the cartridge."),T("Cannot Load Cartridge"),MB_ICONEXCLAMATION);
        }else{
          CartFile=fn;
          This->cart_display.set_text(fn);
        }
        CheckResetDisplay();
      }
    	b->set_check(0);
    }else if (b->id==747){ // Remove cart
      This->cart_display.set_text("");

      delete[] cart;
      cart=NULL;
#if defined(SSE_SOUND_CARTRIDGE)
      SSEConfig.mv16=SSEConfig.mr16=false;
#endif
#if defined(SSE_DONGLE_CUBASE3)
      SSEConfig.Cubase3Cart=false;
#endif
      CartFile="";
      CheckResetDisplay();
#endif//#if defined(SSE_DONGLE)
    }else if (b->id==960){
    	bEnableShiftSwitching=b->checked;
      InitKeyTable();
    }else if (b->id==1000){
      reset_st(RESET_COLD | RESET_STOP | RESET_CHANGESETTINGS | RESET_BACKUP|RESET_COUNT);
    }else if (b->id==1010){
    	b->set_check(true);
      fileselect.set_corner_icon(&Ico16,ICO16_CHIP);
      EasyStr fn=fileselect.choose(XD,This->TOSBrowseDir,"",
        		T("Find a TOS"),FSM_LOAD | FSM_LOADMUSTEXIST,
		        romfile_parse_routine,".img");
      if (fn[0]){
        This->TOSBrowseDir=fn;
        RemoveFileNameFromPath(This->TOSBrowseDir,true);

        bool Found=0;
        for (int i=0;i<This->tos_lv.sl.NumStrings;i++){
        	if (IsSameStr_I(strrchr(This->tos_lv.sl[i].String,'\01')+1,fn)){
        		Found=true;
        		This->tos_lv.changesel(i);
        	}
        }
        if (Found==0){
          if (get_TOS_address(fn)){
            Str Name=GetFileNameFromPath(fn),Ext;
            char *dot=strrchr(Name,'.');
            if (dot){
              Ext=dot;
              *dot='\0';
            }
            EasyStr LinkName=UsersPath+SLASH+Name+Ext;
            int n=2;
            while (Exists(LinkName)){
              LinkName=UsersPath+SLASH+Name+"("+(n++)+")"+Ext;
            }
            symlink(fn,LinkName);
            This->RefreshTOSBox(fn);
            This->NewROMFile=fn;
          }else{
            Alert(fn+" "+T("is not a valid TOS image."),T("Cannot use TOS"),MB_ICONEXCLAMATION);
          }
        }
	      CheckResetIcon();
      }
    	b->set_check(0);
    }else if (b->id==1011){
      b->set_check(true);
      This->RefreshTOSBox();
      b->set_check(0);
    }else if (b->id>=1200 && b->id<1300){
      int p=(b->id-1200)/20;
      int i=b->id % 20;
      int IOType=GetPortIOType(STPort[p].Type);
      bool ClosePort=0,UpdateDisplay=0;
      switch (i){
        case 1:case 11:case 13:	// Choose device
        {
          Str CurDev=STPort[p].PortDev[IOType];
          if (i==13) CurDev=STPort[p].LANPipeIn;
          b->set_check(true);
          fileselect.set_corner_icon(&Ico16,ICO16_PORTS);
          Str CurFol=CurDev;
          RemoveFileNameFromPath(CurFol,REMOVE_SLASH);
          EasyStr fn=fileselect.choose(XD,CurFol,GetFileNameFromPath(CurDev),
                        T("Choose Device"),FSM_OK | FSM_LOADMUSTEXIST,NULL,"");
          if (fileselect.chose_option==FSM_OK){
            if (i!=13){
              STPort[p].PortDev[IOType]=fn;
            }else{
              STPort[p].LANPipeIn=fn;
            }
            UpdateDisplay=true;
            ClosePort=true;
          }
          b->set_check(0);
          break;
        }
        case 3: STPort[p].AllowIO[IOType][0]=b->checked;ClosePort=true; break;
        case 4: STPort[p].AllowIO[IOType][1]=b->checked;ClosePort=true; break;
        case 5:case 14: // Open device
        {
          if (STPort[p].IsPCPort()){
            ClosePort=true;
          }else{
            Str ErrText,ErrTitle;
            b->set_check(STPort[p].Create(p,ErrText,ErrTitle,true));
          }
          break;
        }
        case 6: // Choose file
        {
          b->set_check(true);
          fileselect.set_corner_icon(&Ico16,ICO16_PORTS);
          Str CurFol=STPort[p].File;
          RemoveFileNameFromPath(CurFol,REMOVE_SLASH);
          EasyStr fn=fileselect.choose(XD,CurFol,GetFileNameFromPath(STPort[p].File),
                        T("Select Output File"),FSM_OK | FSM_LOADMUSTEXIST,NULL,".dmp");
          if (fileselect.chose_option==FSM_OK){
            STPort[p].File=fn;
            This->FileDisplay[p].set_text(fn);
          }
          b->set_check(0);
          break;
        }
        case 7: // Empty file
        {
          b->set_check(true);

          /*int Ret=Alert(T("Are you sure? This will permanently delete the contents of the file."),
                          T("Delete Contents?"),MB_ICONQUESTION | MB_YESNO);
          if (Ret==IDYES)*/
          {
            STPort[p].Close();
            DeleteFile(STPort[p].File);
            Str ErrText,ErrTitle;
            STPort[p].Create(p,ErrText,ErrTitle,true);
#if defined(SSE_PRINTER)
            if(p==TSTPort::PARALLEL)
            {
              Printer.Close(); // close current files (if open)
              SSEConfig.PageRtf=SSEConfig.PagePbm=0; // reset counter
            }
#endif
          }
          b->set_check(0);
          break;
        }
      }
      if (ClosePort){
        if (STPort[p].IsPCPort()){
          STPort[p].Close();
          UpdateDisplay=true;
        }
      }
      if (UpdateDisplay) This->UpdatePortDisplay(p);
    }else if (b->id==2001){
      b->set_check(true);
      Str Path=This->CreateMacroFile(true);
      dir_lv.select_item_by_name(GetFileNameFromPath(Path));
      This->MacroSel=Path;
      This->UpdateMacroRecordAndPlay();
      b->set_check(0);
    }else if (b->id==2002 || b->id==2102){ // Change store folder
      bool Macro=(b->id==2002);
      char *Current=This->ProfileDir;
      if (Macro) Current=This->MacroDir;
      fileselect.set_corner_icon(&Ico16,ICO16_FOLDER);
      EasyStr new_path=fileselect.choose(XD,Current,"",T("Pick a Folder"),
          FSM_CHOOSE_FOLDER | FSM_CONFIRMCREATE,folder_parse_routine,"");
      if (new_path.NotEmpty()){
        NO_SLASH(new_path);
        if (Macro){
          This->MacroDir=new_path;
          This->MacroSel="";
        }else{
          This->ProfileDir=new_path;
          This->ProfileSel="";
        }

        This->dir_lv.base_fol=new_path;
        This->dir_lv.fol=new_path;
        This->dir_lv.lv.sel=-1;
        This->dir_lv.refresh_fol();

        This->UpdateMacroRecordAndPlay();
        This->UpdateProfileDisplay();
      }
    }else if (b->id==2010){
      if (macro_record==0){
        macro_record_file=This->MacroSel;
        macro_advance(MACRO_STARTRECORD);
      }else{
        macro_end(MACRO_ENDRECORD);
      }
    }else if (b->id==2011){
      if (macro_play==0){
        macro_play_file=This->MacroSel;
        macro_advance(MACRO_STARTPLAY);
      }else{
        macro_end(MACRO_ENDPLAY);
      }
    }else if (b->id==2101 || b->id==2111){
      b->set_check(true);
      Str Path;
      if (b->id==2111){ // Save over
        if (IDYES==Alert(T("Are you sure?"),T("Overwrite File"),MB_ICONQUESTION | MB_YESNO)){
          Path=This->ProfileSel;
        }
      }else{ // Save new
        hxc_prompt prompt;
        Path=prompt.ask(XD,T("New Profile"),T("Enter Name"));
        if (Path.NotEmpty()){
          Path=GetUniquePath(dir_lv.fol,Path+".ini");
        }
      }
      if (Path.NotEmpty()){
        SaveAllDialogData(0,Path);
        dir_lv.refresh_fol();
        dir_lv.select_item_by_name(GetFileNameFromPath(Path));
        This->ProfileSel=Path;
        This->UpdateProfileDisplay();
      }
      b->set_check(0);
    }else if (b->id==2110){
      b->set_check(true);
      This->LoadProfile(This->ProfileSel);
      b->set_check(0);

    }else if (b->id==IDC_DISKLIGHT){
      OsdControl.show_disk_light=b->checked;
    }else if (b->id==IDC_TRACKINFO){
      OPTION_DRIVE_INFO=b->checked;
    }else if (b->id==IDC_OSD_SCROLLERS){
      OsdControl.show_scrollers=b->checked;
      if(OsdControl.show_scrollers)
        OsdControl.ScrollerPhase=TOsdControl::WANT_SCROLLER;
      else
        OsdControl.ScrollerPhase=TOsdControl::NO_SCROLLER;
    }else if (b->id==IDC_OSD_JOKES){
      OsdControl.show_jokes=b->checked;
#if defined(SSE_OSD_DEBUGINFO)      
    }else if (b->id==IDC_OSD_DEBUGINFO){
      OPTION_OSD_DEBUGINFO=b->checked;
#endif
    }else if (b->id==12030){
      This->ChangeOSDDisable(b->checked);
    }else if (b->id>=15000 && b->id<15100){
      int i=(b->id-15000)/10;
      hxc_listview *p_lv=&(This->drop_lv);
      hxc_edit *p_ed=(hxc_edit*)hxc::find(b->parent,15000+i*10);

      b->set_check(true);

      p_lv->sl.DeleteAll();
      for (int cl=0;cl<16;cl++){
        if (Comlines_Default[i][cl]==NULL) break;
        p_lv->additem(Comlines_Default[i][cl]);
      }
      p_lv->itemheight=(b->font->ascent)+(b->font->descent)+2; //use the listview's font!
      p_lv->in_combo=true;
      p_lv->sel=0;
      p_lv->id=15000+i*10;
      p_lv->create(XD,p_ed->handle,0,p_ed->h+1,p_ed->w + b->w,
                p_lv->itemheight*p_lv->sl.NumStrings + p_lv->border*2,listview_notify_proc,This);
      XSetInputFocus(XD,p_lv->handle,RevertToParent,CurrentTime);
      XFlush(XD);
    }
#if defined(SSE_UNIX)
    else switch(b->id) {
    case 4003:
      OPTION_HACKS=b->checked;
      break;
    case 4004:
      OPTION_EMU_DETECT=b->checked;
      break;
#if defined(SSE_HD6301_LL) && !defined(SSE_NO_OLD_IKBD)
    case 4006:
      b->checked=OPTION_C1=b->checked&HD6301_OK;
      break;
#endif
#if defined(SSE_TOS_KEYBOARD_CLICK)
    case 4007:
      OPTION_KEYBOARD_CLICK=b->checked;
      Tos.CheckKeyboardClick(); // immediate effect
      break;
#endif
    case 4013:
      //SSEOptions.PSGFixedVolume=b->checked;
      break;
#ifdef SSE_SOUND_MICROWIRE_OPTION
    case 4009:
      OPTION_MICROWIRE=b->checked;
      break;
#endif
#if defined(SSE_SOUND_MICROWIRE_HACKS)
    case IDC_YM_12DB: // STE YM-12db
      //OPTION_YM_12DB=!OPTION_YM_12DB;
      Microwire.PsgReduce=(Microwire.PsgReduce)?0:2;
      //TRACE_LOG("Option STE YM-12db %d\n",OPTION_YM_12DB);
      if(OPTION_SAMPLED_YM)
        Psg.LoadFixedVolTable(); // reload to adapt
      break;
#endif
#if defined(SSE_DRIVE_SOUND)
    case IDC_DRIVE_SOUND:
      OPTION_DRIVE_SOUND=b->checked;
      break;
#endif
    case 4014:
      OPTION_VLE=b->checked;
      break;
    case 4015:
      OPTION_VMMOUSE=b->checked;
      break;
#if defined(SSE_ACSI_LASER)
    case 4018:
      OPTION_LASER=b->checked;
      break;
#endif      
#if defined(SSE_PRINTER)
    case 4020:
      OPTION_PRINTER=b->checked;
      break;
#endif
#if defined(SSE_OSD_FPS_INFO)
    case 12003:
      OPTION_OSD_FPSINFO=b->checked;
      break;
#endif
#if defined(SSE_YM2149_LL)
    case 4019:
      OPTION_MAME_YM=b->checked;
      //This->DestroyCurrentPage();
      This->CreatePage(This->Page);
      break;
#endif
#if defined(SSE_IKBD_RTC)
    case 4021:
      OPTION_RTC_HACK=b->checked;
      break;
#endif
    case 4022:case 4023:case 4025:
      switch(b->id){
      case 4022:
        OPTION_GREYSCREEN=b->checked;
        break;
      case 4023:
        SSEOptions.FullSpectrumPal=b->checked;
        break;
      case 4025:
        OPTION_GREENSCREEN=b->checked;
        break;
      }
      make_palette_table(col_brightness,col_contrast);
      if(!flashlight_flag) 
        palette_convert_all();
      if(runstate==RUNSTATE_STOPPED)
        draw(false);      
      break;
    case 4024:
      Disp.DrawToVidMem=!Disp.DrawToVidMem;
      Draw.MarshalParameters();
      break;
    case IDC_RANDOM_WU:
      OPTION_RANDOM_WU=b->checked;
      break;
#if defined(SSE_MEGA16)
    case IDC_TURBO16MHZ:
      if(SSEConfig.Mega)
      {
        if((Cpu16.ScuReg&3)==3)
          Cpu16.ScuReg=0;
        else
          Cpu16.ScuReg=7; // one option only, keep it simple
        This->CreatePage(This->Page); // update 8/16
      }
      break;
#endif
    }//sw
#endif//SSE_UNIX

  }
  return 0;
}
//---------------------------------------------------------------------------
int TOptionBox::dd_notify_proc(hxc_dropdown*dd,int mess,INT_PTR i)
{
  TOptionBox*This=(TOptionBox*)(dd->owner);
	if (mess!=DDN_SELCHANGE) return 0;

  if (dd->id==8){
#if defined(SSE_CPU_4GHZ)
    nSysCyclesPerSecond=MAX(MIN(dd->sl[dd->sel].Data[0],4096000000l),8000000l)*TICKS8;
#elif defined(SSE_CPU_3GHZ)
    nSysCyclesPerSecond=MAX(MIN(dd->sl[dd->sel].Data[0],3072000000l),8000000l)*TICKS8;
#elif defined(SSE_CPU_2GHZ)
    nSysCyclesPerSecond=MAX(MIN(dd->sl[dd->sel].Data[0],2048000000l),8000000l)*TICKS8;
#elif defined(SSE_CPU_1GHZ)
    nSysCyclesPerSecond=MAX(MIN(dd->sl[dd->sel].Data[0],1024000000l),8000000l)*TICKS8;
#elif defined(SSE_CPU_512MHZ)
    nSysCyclesPerSecond=MAX(MIN(dd->sl[dd->sel].Data[0],512000000l),8000000l)*TICKS8;
#elif defined(SSE_CPU_256MHZ)
    nSysCyclesPerSecond=MAX(MIN(dd->sl[dd->sel].Data[0],256000000l),8000000l)*TICKS8;
#else
    nSysCyclesPerSecond=MAX(MIN(dd->sl[dd->sel].Data[0],128000000l),8000000l)*TICKS8;
#endif
    if (runstate==RUNSTATE_RUNNING) osd_init_run(false);
    AdaptCpuBoost();
  }else if (dd->id==5001){ //sound mode
    psg_hl_filter=dd->sel+1; // waste time on this :)
  }else if (dd->id==5067){ //sound lib
    SoundStop();
    SoundRelease();
    x_sound_lib=dd->sl[dd->sel].Data[0];
    InitSound();
#if 1
    This->CreatePage(This->Page); // way to force update of display
#else
    This->FillSoundDevicesDD();
#endif
    SoundStart();
  }else if (dd->id==5000){ //sound device
    sound_device_name=dd->sl[dd->sel].String;
  }else if (dd->id==5004){ //sound delay
    psg_write_n_screens_ahead=dd->sl[dd->sel].Data[0];
  }else if (dd->id==5002){ //sound freq
    sound_chosen_freq=dd->sl[i].Data[0];
    This->UpdateSoundFreq();
  }else if (dd->id==5003){ //sound format
#ifndef NO_RTAUDIO
    if (HIWORD(dd->sl[dd->sel].Data[0])){
      rt_unsigned_8bit=1;
    }else if (x_sound_lib==XS_RT){
      rt_unsigned_8bit=0;
    }
#endif
    This->ChangeSoundFormat(LOBYTE(dd->sl[dd->sel].Data[0]),HIBYTE(dd->sl[dd->sel].Data[0]));
  }else if (dd->id>=IDC_OSDSECONDS && dd->id<IDC_OSDSECONDS+10){
    BYTE *p_element[4]={&OsdControl.show_plasma,&OsdControl.show_speed,&OsdControl.show_icons,&OsdControl.show_cpu};
    *(p_element[dd->id - IDC_OSDSECONDS])=dd->sl[dd->sel].Data[0];
  }
  if (dd==&(This->frameskip_dd)){
    if (i==4){
      frameskip=AUTO_FRAMESKIP;
    }else{
      frameskip=i+1;
    }
#if !defined(SSE_VID_BORDERS)    
  }else if (dd==&(This->border_dd)){
    int newborder=dd->sel,oldborder=border;
    if (This->ChangeBorderModeRequest(newborder)){
      border=newborder;
     // ChangeBorderSize(newborder);
      if (FullScreen) change_fullscreen_display_mode(true);
      //change_window_size_for_border_change(oldborder,newborder);
      else
        StemWinResize();
      if(runstate==RUNSTATE_STOPPED)
        draw(false);
    }else{
#if 0
      dd->changesel(oldborder);dd->draw();
      dd->lv.changesel(oldborder);dd->lv.draw(true,true);
#elif defined(SSE_VID_DISABLE_AUTOBORDER)
      dd->changesel(MIN(oldborder,1));dd->draw();
      dd->lv.changesel(MIN(oldborder,1));dd->lv.draw(true,true);
#else
      dd->changesel(MIN(oldborder,2));dd->draw();
      dd->lv.changesel(MIN(oldborder,2));dd->lv.draw(true,true);
#endif
      border=oldborder;
    }
#endif    
  }else if (dd->id==910){
    This->NewMemConf0=dd->lv.sl[dd->sel].Data[0];
    This->NewMemConf1=dd->lv.sl[dd->sel].Data[1];
    if (SSEConfig.bank_length[0]==Mmu.BankLength(This->NewMemConf0)
      &&  SSEConfig.bank_length[1]==Mmu.BankLength(This->NewMemConf1)){
      This->NewMemConf0=-1;
    }
    CheckResetIcon();
  }else if (dd->id==920){
    This->NewMonitorSel=dd->sel;
    if (This->NewMonitorSel==This->GetCurrentMonitorSel()) This->NewMonitorSel=-1;
    CheckResetIcon();
  }else if (dd->id==940){
#if defined(SSE_IKBD_MAPPINGFILE)
    int ix=(This->keyboard_language_dd.lv.sl[This->keyboard_language_dd.sel]
      .Data[0]);
    if(ix==MAKELANGID(LANG_CUSTOM,SUBLANG_NEUTRAL))
    {
      fileselect.set_corner_icon(&Ico16,ICO16_STCONFIG);
      RemoveFileNameFromPath(KeyboardMappingPath,REMOVE_SLASH);
      EasyStr path=fileselect.choose(XD,KeyboardMappingPath,GetFileNameFromPath
        (KeyboardMappingPath),T("Select Keyboard Mapping"),FSM_LOAD
        |FSM_LOADMUSTEXIST,inifile_parse_routine,".ini");      
      if(path.IsNotEmpty())
      {
        KeyboardMappingPath=path;
        KeyboardLangID=(LANGID)ix;
      }
    }
    else
      KeyboardLangID=(LANGID)ix;
#else
    KeyboardLangID=(LANGID)(This->keyboard_language_dd.lv.sl[This->keyboard_language_dd.sel].Data[0]);
#endif    
    InitKeyTable();
  }else if (dd->id==1020){
    This->eslTOS_Sort=(ESLSortEnum)dd->sl[dd->sel].Data[0];
    This->eslTOS_Descend=(bool)dd->sl[dd->sel].Data[1];
    This->RefreshTOSBox();
  }else if (dd->id>=1200 && dd->id<1300){
    int p=(dd->id-1200)/20;
    int NewType=dd->sl[dd->sel].Data[0];
    if (STPort[p].Type!=NewType){
      STPort[p].Close();
      STPort[p].Type=NewType;
      This->UpdatePortDisplay(p);

      // Don't open devices straight away, everything else is okay
      if (GetPortIOType(NewType)==-1){
        Str ErrText,ErrTitle;
        STPort[p].Create(p,ErrText,ErrTitle,true);
      }
    }
  }else if (dd->id==2012 || dd->id==2013){
    TMacroFileOptions MFO;
    macro_file_options(MACRO_FILE_GET,This->MacroSel,&MFO);
    if (dd->id==2012) MFO.max_mouse_speed=dd->sl[dd->sel].Data[0];
    if (dd->id==2013) MFO.allow_same_vbls=dd->sl[dd->sel].Data[0];
    macro_file_options(MACRO_FILE_SET,This->MacroSel,&MFO);
  }
  // SSE
  else switch(dd->id) {
  case 4001: // border size
    border=dd->sel; // sel==data
    border_last_chosen=border;
    ChangeBorderSize(border);
    if(runstate==RUNSTATE_STOPPED)
      draw(false);
    break;
  case 4005: // ST model
    SSEConfig.SwitchSTModel(dd->sl[dd->sel].Data[0]);
    This->CreatePage(This->Page); // default clocks
    break;
#if defined(SSE_HARDWARE_OVERSCAN)
  case 4016:
    OPTION_HWOVERSCAN=dd->sel; // sel==data
    break;
#endif      
  case 4017: // wake-up state
    OPTION_WS=dd->sl[dd->sel].Data[0];
    OPTION_RANDOM_WU=(OPTION_WS==0);
    break;
#if defined(SSE_IKBD_RTC)  
  case 4028:
    OPTION_BATTERY6301=dd->sel; // sel==data
    break;
#endif
  case 4026:
    //OPTION_SHIFTER_WU=dd->sel;
    OPTION_SHIFTER_WU=dd->sl[dd->sel].Data[0];
    break;
  case 4027:
    //OPTION_CAPTURE_MOUSE=dd->sel; // NO! sel is 0-based index TODO make it more intuitive
    OPTION_CAPTURE_MOUSE=dd->sl[dd->sel].Data[0];
    break;
  }//sw
  return 0;
}
//---------------------------------------------------------------------------
int TOptionBox::edit_notify_proc(hxc_edit *ed,int Mess,INT_PTR Inf)
{
  switch (ed->id){
    case 5000:
      if (Mess==EDN_CHANGE) sound_device_name=ed->text;
      if (Mess==EDN_RETURN || Mess==EDN_LOSTFOCUS){
        SoundStop();
        SoundStart();
      }
      break;
#if !defined(SSE_NO_AUTOSNAPSHOTEDIT)
    case 100:
      if (Mess==EDN_CHANGE) AutoSnapShotName=ed->text;
      break;
#endif
  }
  if (ed->id>=1200 && ed->id<1300){
    if (Mess!=EDN_CHANGE) return 0;

    TOptionBox *This=(TOptionBox*)(ed->owner);
    int p=(ed->id-1200)/20;
    int i=ed->id % 20;
    switch (i){
      case 2:
      {
        int IOType=GetPortIOType(STPort[p].Type);
        if (IOType>=0){
          STPort[p].PortDev[IOType]=ed->text;
          if (STPort[p].IsPCPort()){
            STPort[p].Close();
            This->IOOpenBut[p].set_check(0);
          }
        }
        break;
      }
      case 10:
        STPort[p].PortDev[TPORTIO_TYPE_PIPE]=ed->text;
        // no break?
      case 12:
        if (i==12) STPort[p].LANPipeIn=ed->text;
        if (STPort[p].IsPCPort()){
          STPort[p].Close();
          This->UpdatePortDisplay(p);
        }
        break;
    }
  }else if (ed->id>=15000 && ed->id<16000){
    if (Mess==EDN_CHANGE) Comlines[(ed->id-15000)/10]=ed->text;
  }
	return 0;
}
//---------------------------------------------------------------------------
int TOptionBox::scrollbar_notify_proc(hxc_scrollbar *SB,int Mess,INT_PTR I)
{
	TOptionBox *This=(TOptionBox*)(SB->owner);
	if (Mess==SBN_SCROLLBYONE){
		SB->pos+=I;
		SB->rangecheck();
	}else if (Mess==SBN_SCROLL){
		SB->pos=I;
	}else{
		return 0;
	}
	bool UpdatePalette=0;
	switch (SB->id){
		case 0: slow_motion_speed=SB->pos*10 + 10; break;
    case 1: fast_forward_max_speed=1000/(SB->pos+2);
      break;
    case 2: run_speed_ticks_per_second=100000/(50 + SB->pos*5); 
      break;
    case 10: col_brightness=SB->pos - 128;UpdatePalette=true; break;
    case 11: col_contrast=SB->pos - 128;UpdatePalette=true; break;
    case 12:
    case 13:
    case 14:
      col_gamma[SB->id-12]=SB->pos-128;
      UpdatePalette=true;
      break;
#if defined(SSE_DRIVE_SOUND)      
    case 15:
      FloppyDrive[1].SoundVolume=FloppyDrive[0].SoundVolume=SB->pos-10000;
      FloppyDrive[DRIVE_A].SoundChangeVolume();
      FloppyDrive[DRIVE_B].SoundChangeVolume();
      break;
#endif
#if defined(SSE_YM2149_LL)
    case 16:
      OPTION_LOWPASS=SB->pos;
      break;
#endif
    case 17:
    {
      int position=SB->pos;
      //int db=(int)-(10000-10000*log10((float)(position+1))/log10((float)101));
      //SoundVolume=db;
      SoundVolume=SB->pos-10000;
      SoundChangeVolume();
      break;
    }
    case 18:
      Mfp.xtal=SB->pos+2457500;
      CpuMfpRatio=(double)CpuNormalHz/(double)Mfp.xtal;
      break;
    case 19:
      CpuCustomHz=SB->pos+8000000;
      CpuCustomHz*=TICKS8;
      CpuNormalHz=CpuCustomHz;
      nSysCyclesPerSecond=CpuNormalHz;
      if(SSEConfig.CpuBoost>1)
        AdaptCpuBoost();
      CpuMfpRatio=(double)CpuNormalHz/(double)Mfp.xtal;
      break;
  }
	SB->draw();
	if (UpdatePalette){
		make_palette_table(col_brightness,col_contrast);
		if (!flashlight_flag) palette_convert_all();
		This->DrawBrightnessBitmap(This->brightness_image);
		This->brightness_picture.draw();
	}

	This->RunSpeedLabel.set_text(T("Run speed")+": "+(100000/run_speed_ticks_per_second)+"%");
	This->SMSpeedLabel.set_text(T("Slow motion speed")+": "+(slow_motion_speed/10)+"%");

  Str Text=T("Maximum fast forward speed")+": ";
  if (fast_forward_max_speed>50){
    Text+=Str((1000/fast_forward_max_speed)*100)+"%";
  }else{
    Text+=T("Unlimited");
    fast_forward_max_speed=0;
  }
	This->FFMaxSpeedLabel.set_text(Text);

	This->brightness_label.set_text(T("Brightness")+": "+col_brightness);
	This->contrast_label.set_text(T("Contrast")+": "+col_contrast);

  This->gamma_label[0].set_text(T("Gamma red")+": "+col_gamma[0]);
  This->gamma_label[1].set_text(T("Gamma green")+": "+col_gamma[1]);
  This->gamma_label[2].set_text(T("Gamma blue")+": "+col_gamma[2]);


	return 0;
}
//---------------------------------------------------------------------------
int TOptionBox::dir_lv_notify_proc(hxc_dir_lv *lv,int Mess,INT_PTR i)
{
	TOptionBox *This=(TOptionBox*)(lv->owner);
  switch (Mess){
    case DLVN_SELCHANGE:
    {
      Str new_sel;
      if (i>=0){
        if (lv->sl[i].Data[DLVD_TYPE]==0){ // Up folder
          new_sel=lv->fol+"/..";
        }else{
          new_sel=lv->get_item_path(i);
        }
      }
      if (lv->id==2000){ // Macro
        if (new_sel==This->MacroSel) break;
        This->MacroSel=new_sel;
        This->UpdateMacroRecordAndPlay();
      }else{
        if (new_sel==This->ProfileSel) break;
        This->ProfileSel=new_sel;
        This->UpdateProfileDisplay();
      }
      break;
    }
  }
  return 0;
}
//---------------------------------------------------------------------------

#endif//UNIX

#undef LOGSECTION

