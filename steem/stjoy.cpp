
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
FILE: stjoy.cpp
DESCRIPTION: This file contains both the code for the Steem joysticks dialog
and the code to read the PC joysticks (multimedia, disabled).
See stjoy_directinput.cpp for Direct Input.
VK_NUMLOCK and VK_SCROLL may be used to select joysticks at any time
Scroll Lock isn't on all keyboards. Also, messing with those keys isn't so
user-friendly in a multitask OS. In v412, more setups are available and a
shortcut can be used to select the setup.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <steemh.h>
#include <stjoy.h>
#include <gui.h>
#include <translate.h>
#include <display.h>
#include <options.h>
#include <mymisc.h>
#include <acc.h>
#include <dir_id.h>
#include <shortcutbox.h>
#include <macros.h>
#include <data_union.h>
#include <ikbd.h>
#include <emulator.h>
#include <stports.h>
#ifdef WIN32
#include <windowsx.h>
#endif
#ifdef UNIX
#include <ikbd.h>
extern KeyCode VK_F11,VK_F12,VK_END;
extern KeyCode VK_LEFT,VK_RIGHT,VK_UP,VK_DOWN,VK_TAB;
extern KeyCode VK_SHIFT,VK_LSHIFT,VK_RSHIFT;
extern KeyCode VK_MENU,VK_LMENU,VK_RMENU;
extern KeyCode VK_CONTROL,VK_LCONTROL,VK_RCONTROL;
extern KeyCode VK_NUMLOCK,VK_SCROLL;
//#define UNIX_FAKE_JOY
#endif//UNIX



#define CbUnits    GuiSM.mCbUnits
#define CharHeight GuiSM.mCharHeight

/*  the joystick direction bits aren't even mentionned in Atari's IKBD doc
    it is in the "compendium" p. 305, the ID correspond to the bit for
    each position, the sequence is also used in for() loops  */
#define ID_UP             0 // 0001
#define ID_DOWN           1 // 0010
#define ID_LEFT           2 // 0100
#define ID_RIGHT          3 // 1000
#define ID_FIRE           4
#define ID_AUTOFIRE       5
#define ID_JUMP           6
/* as usual in Steem menu numbers can be used in calculations */
#define IDC_RESCAN       91
#define IDC_JAGPAD       98
#define IDC_JAGFIRES    160 // -162
#define IDC_JAGKEYS     163 // -176
#define IDS_JAGFIRES    180 // -183
#define IDS_JAGKEYS     184 // -197
#define IDC_JOYTABS      99
#define IDS_ACTIVE        1
#define IDC_ACTIVE        2
#define IDC_JOYBUTTON    10
#define IDC_DEADZONE     20
#define IDS_JOYBUTTON    50
#define IDC_AUTOFIRE     17
#define IDS_AUTOFIRE     51
#define IDC_ANYBUTTON    18
#define IDS_ANYBUTTON  1060
#define IDC_SETUP      1101
#define TOGGLE_NEVER      0
#define TOGGLE_ALWAYS     1
#define GROUP_BASE      100 // 100=left, 200=right


BYTE stick[MAX_ST_JOYS]; // FxxxRLDU

#if defined(SSE_GUI_STATUS_BAR)
bool bSTjoyNoInput[MAX_ST_JOYS];
#endif
TJoystickInfo JoyInfo[MAX_PC_JOYS];
JOYINFOEX JoyPos[MAX_PC_JOYS];
TJoystick Joy[MAX_ST_JOYS];
TJoystick JoySetup[JOYSTICK_SETUPS][MAX_ST_JOYS];
// Bitmasks in which all set bits represent a button that can be pressed to
// cause fire in Any Button On... mode
DWORD JoyAnyButtonMask[MAX_PC_JOYS];
bool JoyExists[MAX_PC_JOYS]={false,false,false,false,false,false,false,false};
const char AxisToName[7]={'X','Y','Z','R','U','V','P'};
bool DisablePCJoysticks=false;
BYTE TJoystickConfig::BasePort=0;
BYTE NumJoysticks=0,nJoySetup=0;
#ifdef WIN32
#ifndef NO_DIRECTINPUT
BYTE JoyReadMethod=PCJOY_READ_DI;
#else
BYTE JoyReadMethod=PCJOY_READ_WINMM;
#endif
#endif
WORD paddles_ReadMask=0;


TJoystick::TJoystick() {
  ToggleKey=TJoystickConfig::ACTIVE_NEVER;
  AutoFireSpeed=AnyFireOnJoy=0;
  for(int n=N_JOY_PORT_0;n<JOY_N_DIR_ID;n++)
    DirID[n]=0x0000ffff;
  for(int n=0;n<JAGPAD_N_DIR;n++) 
    JagDirID[n]=0x0000ffff;
  DeadZone=50;
  Type=JOY_TYPE_JOY;
  TJoystickConfig::CreateJoyAnyButtonMasks();
#if defined(SSE_GUI_STATUS_BAR)
  *(DWORDLONG*)&bSTjoyNoInput[0]=0;
#endif
}


bool IsJoyActive(int const joy) {
  // is the ST joystick active or not according to the joystick options?
  // note that a joystick may be active even if it is controlled by a USB
  // controller that isn't connected at the moment
  ASSERTB(joy<MAX_ST_JOYS);
  if(joy==N_JOY_STE_A_1||joy==N_JOY_STE_B_1)
  {
    // These joysticks are disabled when a JagPad is in their port
    if(Joy[joy-1].Type==JOY_TYPE_JAGPAD) 
      return false;
  }
#if defined(SSE_DONGLE)
  switch(DONGLE_ID) {
#if defined(SSE_DONGLE_LEADERBOARD)
  case TDongle::LEADERBOARD:
  case TDongle::TENTHFRAME:
#endif
#if defined(SSE_DONGLE_CRICKET)
  case TDongle::RUGBY: // port 1 = $D or $C (?)
#endif
    if(joy==1)
      return true;
    break;
#if defined(SSE_DONGLE_CRICKET)
  case TDongle::CRICKET: // port 0 bit 0 toggles (?)
  case TDongle::SOCCER: // port 0 = $D or $C
    if(joy==0)
      return true;
    break;
#endif
  }
#endif
  if(Joy[joy].ToggleKey<=TJoystickConfig::ACTIVE_ALWAYS) 
    return (Joy[joy].ToggleKey!=TJoystickConfig::ACTIVE_NEVER);
  bool Toggled=((GetKeyState(Joy[joy].ToggleKey)&1)!=0);
  if(Joy[joy].ToggleKey==VK_NUMLOCK)
    return !Toggled; // active when no numlock
  return Toggled;
}


void joy_read_buttons(int const njoys) {
  if(!bAppActive)
  {
    // No fire buttons held down
    *(DWORDLONG*)&stick[N_JOY_PORT_0]&=0x8080808080808080;
    return;
  }
  if(OPTION_C1)
    *(WORD*)&old_stick[N_JOY_PORT_0]=*(WORD*)&stick[N_JOY_PORT_0];
  static int AutoFireCount[MAX_ST_JOYS]={0,0,0,0,0,0,0,0};
  int max_joy=MAX((char)1,SSEConfig.MaxJoy); // must read mouse buttons at least...
  for(int joy=N_JOY_PORT_0;joy<=max_joy;joy++)
  {
    // Change high bit of stick[joy] according to joystick button state
    bool JoyDown=false;
    if(IsJoyActive(joy))
    {
      JoyDown=IsDirIDPressed(Joy[joy].DirID[ID_FIRE],Joy[joy].DeadZone,true);
      if(Joy[joy].AnyFireOnJoy && !JoyDown)
      {
        int JoyNum=Joy[joy].AnyFireOnJoy-1;
        if((JoyPos[JoyNum].dwButtons & CutButtonMask[JoyNum] & JoyAnyButtonMask[JoyNum])!=0)
          JoyDown=true;
      }
      if(njoys==MAX_ST_JOYS && Joy[joy].AutoFireSpeed)
      {
        if(IsDirIDPressed(Joy[joy].DirID[ID_AUTOFIRE],Joy[joy].DeadZone,true))
        {
          if(AutoFireCount[joy]<Joy[joy].AutoFireSpeed)
          {
            JoyDown=true;
            if(AutoFireCount[joy]<=0) 
              AutoFireCount[joy]=Joy[joy].AutoFireSpeed*2;
          }
          else
            JoyDown=false;
          AutoFireCount[joy]--;
        }
        else
          AutoFireCount[joy]=0;
      }
    }
    if(joy<=N_JOY_PORT_1)
    {
      if(stem_mousemode==STEM_MOUSEMODE_WINDOW && !disable_input_vbl_count)
      {
        if(GetKeyState((joy) ? VK_RBUTTON : VK_LBUTTON)<0)
          JoyDown=true;
      }
      if(CutButtonDown[joy]) 
        JoyDown=true;
    }
    if(JoyDown)
      stick[joy]|=BIT_7;
    else
      stick[joy]&=~BIT_7;
  }//nxt joy
#if defined(SSE_STATS)
  if(OPTION_C1)
  {
    for(int i=N_JOY_PORT_0;i<=N_JOY_PORT_1;i++)
    {
      if((stick[i]&BIT_7) && (old_stick[i]&BIT_7)!=(stick[i]&BIT_7))
        Stats.nClick[i]++;
    }
  }
#endif
}


BYTE joy_get_pos(int const joy) {
  // return high bit of stick[joy] or'd with low 4 bits joystick position
  if(!bAppActive) 
    return 0;
  BYTE Ret=stick[joy]&BIT_7;
  if(!IsJoyActive(joy))
    return Ret;
  if(joy==N_JOY_PORT_1 && OPTION_C1)
    old_stick[joy]=stick[joy];
  for(int n=0;n<4;n++) // up, down, left, right
  {
    if(IsDirIDPressed(Joy[joy].DirID[n],Joy[joy].DeadZone,true,true)) 
      Ret|=1<<n; // ID n corresponds the nth direction bit set by the 6301 program
  }
#if defined(SSE_JOYSTICK_JUMP_BUTTON)
  if(IsDirIDPressed(Joy[joy].DirID[ID_JUMP],Joy[joy].DeadZone,true))
    Ret|=1<<ID_UP;
#endif
#ifdef DEADC0DE
  // Don't allow both up and down or left and right to be pressed at the same time
  if((Ret & (1|2))==(1|2)) 
    Ret&=~(1|2);
  if((Ret & (4|8))==(4|8)) 
    Ret&=~(4|8);
#endif
  if(joy<=N_JOY_PORT_1)
  {
#if defined(SSE_STATS)
    if(OPTION_C1 && (Ret&0x7F))
    {
      for(int i=N_JOY_PORT_0;i<=N_JOY_PORT_1;i++)
      {
        if((stick[i]&0x7F) && (old_stick[i]&0x7F)!=(stick[i]&0x7F))
          Stats.nJoy[i]++;
      }
    }
#endif

#if defined(SSE_DONGLE)
    switch(DONGLE_ID) {
#if defined(SSE_DONGLE_LEADERBOARD) 
    case TDongle::LEADERBOARD:
    case TDongle::TENTHFRAME:
      if(joy==1)
        Ret|=3; // up and down
      break;
#endif
#if defined(SSE_DONGLE_CRICKET)
#ifdef DEADC0DE
    case TDongle::CRICKET: // port 0 bit 0 toggles (?)
      if(joy==0)
        Ret|=Dongle.Value;
      break;
#endif
    case TDongle::SOCCER: // port 0 = $D or $C
    case TDongle::RUGBY: // port 1 = $D or $C (?)
      if((joy==0)^(DONGLE_ID==TDongle::RUGBY))
        Ret|=Dongle.Value;
      break;
#endif
    }//sw
#endif
  }
  return Ret;
}


DWORD GetAxisPosition(int const AxNum,JOYINFOEX* const ji) {
  switch(AxNum) {
  case 0: return ji->dwXpos;
  case 1: return ji->dwYpos;
  case 2: return ji->dwZpos;
  case 3: return ji->dwRpos;
  case 4: return ji->dwUpos;
  case 5: return ji->dwVpos;
  default: return 0;
  }
}


void JoyPosReset(int const joy) {
  JoyPos[joy].dwXpos=JoyInfo[joy].AxisMid[0];
  JoyPos[joy].dwYpos=JoyInfo[joy].AxisMid[1];
  JoyPos[joy].dwZpos=JoyInfo[joy].AxisMid[2];
  JoyPos[joy].dwRpos=JoyInfo[joy].AxisMid[3];
  JoyPos[joy].dwUpos=JoyInfo[joy].AxisMid[4];
  JoyPos[joy].dwVpos=JoyInfo[joy].AxisMid[5];
  JoyPos[joy].dwButtons=JoyPos[joy].dwButtonNumber=0;
  JoyPos[joy].dwPOV=0xffffffff;
}


bool joy_is_key_used(BYTE const Key) {
  for(int i=N_JOY_PORT_0;i<MAX_ST_JOYS;i++)
  {
    if(IsJoyActive(i))
    { // note jump button makes sense only on real joystick
      int Last=(Joy[i].AutoFireSpeed) ? ID_JUMP : ID_AUTOFIRE;
      for(int n=0;n<Last;n++)
      {
        if(Joy[i].DirID[n]==Key) 
          return true;
      }
      if(Joy[i].Type==JOY_TYPE_JAGPAD)
      {
        for(int n=0;n<JAGPAD_N_DIR;n++)
        {
          if(Joy[i].JagDirID[n]==Key) 
            return true;
        }
      }
    }
  }
  return false;
}


void joy_check_controllers() { // v4.1.2, only called by ComputerRestore()
  SSEConfig.MaxJoy=-1;
  for(int joy=N_JOY_PORT_0;joy<MAX_ST_JOYS;joy++)
  {
#if defined(SSE_GUI_STATUS_BAR)
    // check if there's an unconnected USB controller
    int nMissPCJoy=0;
    bSTjoyNoInput[joy]=false;
    for(int n=0;n<4;n++)
    {
      IsDirIDPressed(Joy[joy].DirID[n],Joy[joy].DeadZone,true,true);
      if(!bPCInputExists)
        nMissPCJoy++;
    }
    if(nMissPCJoy) // one is enough
      bSTjoyNoInput[joy]=true;
#endif
    // record max active joystick
    if(IsJoyActive(joy))
      SSEConfig.MaxJoy=(BYTE)joy;
  }//nxt joy
}


void SetJoyToDefaults(int const joy,int const Defs) {
  switch(Defs) {
  case 0:
    JoySetup[0][joy].DirID[ID_UP]=MAKEWORD(2,11);  
    JoySetup[0][joy].DirID[ID_DOWN]=MAKEWORD(2,10);
    JoySetup[0][joy].DirID[ID_LEFT]=MAKEWORD(1,11);  
    JoySetup[0][joy].DirID[ID_RIGHT]=MAKEWORD(1,10);
    JoySetup[0][joy].DirID[ID_FIRE]=MAKEWORD(100,10);
    JoySetup[0][joy].DirID[ID_AUTOFIRE]=MAKEWORD(101,10);
    break;
  case 1:
    JoySetup[0][joy].DirID[ID_UP]=MAKEWORD(2,21);  
    JoySetup[0][joy].DirID[ID_DOWN]=MAKEWORD(2,20);
    JoySetup[0][joy].DirID[ID_LEFT]=MAKEWORD(1,21);  
    JoySetup[0][joy].DirID[ID_RIGHT]=MAKEWORD(1,20);
    JoySetup[0][joy].DirID[ID_FIRE]=MAKEWORD(100,20);
    JoySetup[0][joy].DirID[ID_AUTOFIRE]=MAKEWORD(101,20);
    break;
  case 2:
    JoySetup[0][joy].DirID[ID_UP]=VK_UP;           
    JoySetup[0][joy].DirID[ID_DOWN]=VK_DOWN;
    JoySetup[0][joy].DirID[ID_LEFT]=VK_LEFT;         
    JoySetup[0][joy].DirID[ID_RIGHT]=VK_RIGHT;
#ifdef WIN32
    JoySetup[0][joy].DirID[ID_FIRE]=VK_CONTROL; 
    JoySetup[0][joy].DirID[ID_AUTOFIRE]=VK_RETURN;
#endif
#ifdef UNIX
    JoySetup[0][joy].DirID[ID_FIRE]=XKeysymToKeycode(XD,XK_Control_R);
    JoySetup[0][joy].DirID[5]=XKeysymToKeycode(XD,XK_Return);
#endif
    break;
  case 3:
#ifdef WIN32
    JoySetup[0][joy].DirID[ID_UP]='W';             
    JoySetup[0][joy].DirID[ID_DOWN]='Z';
    JoySetup[0][joy].DirID[ID_LEFT]='A';             
    JoySetup[0][joy].DirID[ID_RIGHT]='S';
    JoySetup[0][joy].DirID[ID_FIRE]=VK_SHIFT;        
    JoySetup[0][joy].DirID[ID_AUTOFIRE]=VK_TAB;
#endif
#ifdef UNIX
    JoySetup[0][joy].DirID[ID_UP]=XKeysymToKeycode(XD,XK_W);
    JoySetup[0][joy].DirID[ID_DOWN]=XKeysymToKeycode(XD,XK_Z);
    JoySetup[0][joy].DirID[ID_LEFT]=XKeysymToKeycode(XD,XK_A);
    JoySetup[0][joy].DirID[ID_RIGHT]=XKeysymToKeycode(XD,XK_S);
    JoySetup[0][joy].DirID[ID_FIRE]=VK_LSHIFT;
    JoySetup[0][joy].DirID[ID_AUTOFIRE]=VK_TAB;
#endif
    break;
  }
}


DWORD GetJagPadDown(int const n,DWORD const Mask) {
  //ASSERT(n==N_JOY_STE_A_0||n==N_JOY_STE_B_0);
  if((macro_play_has_joys||macro_record) && Mask<0xffffffff)
    return macro_jagpad[(n==N_JOY_STE_B_0) ? 1 : 0]&Mask;
  DWORD Ret=0;
  if(!IsJoyActive(n))
    return Ret;
  // JagPad only buttons
  for(int b=0;b<JAGPAD_N_DIR;b++)
  {
    if(Mask&(1<<b)) 
      if(IsDirIDPressed(Joy[n].JagDirID[b],Joy[n].DeadZone,true)) 
        Ret|=1<<b;
  }
  // Directions
  for(int b=0;b<4;b++)
  {
    if(Mask&(1<<(JAGPAD_N_DIR+b))) 
      if(IsDirIDPressed(Joy[n].DirID[b],Joy[n].DeadZone,true,true)) 
        Ret|=1<<(JAGPAD_N_DIR+b);
  }
#ifdef DEADC0DE
  // Don't allow both up and down or left and right to be pressed at the same time
#define UD_MASK ((1 << 17) | (1 << 18))
#define LR_MASK ((1 << 19) | (1 << 20))
  if((Ret&UD_MASK)==UD_MASK) 
    Ret&=~UD_MASK;
  if((Ret&LR_MASK)==LR_MASK) 
    Ret&=~LR_MASK;
#undef UD_MASK
#undef LR_MASK
#endif
  return Ret;
}


DWORD ReadJagPad(int const n) {
  //ASSERT(n==N_JOY_STE_A_0||n==N_JOY_STE_B_0);
  int Offset4=0,Offset2=0;
  DWORD Ret=0;
  if(n==N_JOY_STE_B_0) 
  { 
    Offset4=4;
    Offset2=2; 
  }
  DWORD DownMask;
/*
  0xFFFE	Selects group 4, joypad A for reading
  0xFFFD	Selects group 3, joypad A
  0xFFFB	Selects group 2, joypad A
  0xFFF7	Selects group 1, joypad A

  0xFFEF	Selects group 4, joypad B for reading
  0xFFDF	Selects group 3, joypad B
  0xFFBF	Selects group 2, joypad B
  0XFF7F	Selects group 1, joypad B

  Problem: Substation writes 0X instead of FX to read the joypad.
  Solution: only consider nibble for joypad n
*/
  BYTE nibble=(n==N_JOY_STE_A_0)?(paddles_ReadMask&0x0F):((paddles_ReadMask&0xF0)>>4);
  switch(nibble) {
  case 0xd:// group 3
    DownMask=GetJagPadDown(n,JAGPAD_BUT_KEY_AST_BIT|JAGPAD_BUT_KEY_7_BIT|
      JAGPAD_BUT_KEY_4_BIT|JAGPAD_BUT_KEY_1_BIT|JAGPAD_BUT_FIRE_B_BIT);
    if(DownMask&JAGPAD_BUT_KEY_AST_BIT) 
      Ret|=BIT_8<<Offset4;
    if(DownMask&JAGPAD_BUT_KEY_7_BIT) 
      Ret|=BIT_9<<Offset4;
    if(DownMask&JAGPAD_BUT_KEY_4_BIT) 
      Ret|=BIT_10<<Offset4;
    if(DownMask&JAGPAD_BUT_KEY_1_BIT) 
      Ret|=BIT_11<<Offset4;
    if(DownMask&JAGPAD_BUT_FIRE_B_BIT) 
      Ret|=BIT_17<<Offset2;
    break;
  case 0xb:// group 2
    DownMask=GetJagPadDown(n,JAGPAD_BUT_KEY_0_BIT|JAGPAD_BUT_KEY_8_BIT|
      JAGPAD_BUT_KEY_5_BIT|JAGPAD_BUT_KEY_2_BIT|JAGPAD_BUT_FIRE_C_BIT);
    if(DownMask&JAGPAD_BUT_KEY_0_BIT) 
      Ret|=BIT_8<<Offset4;
    if(DownMask&JAGPAD_BUT_KEY_8_BIT) 
      Ret|=BIT_9<<Offset4;
    if(DownMask&JAGPAD_BUT_KEY_5_BIT) 
      Ret|=BIT_10<<Offset4;
    if(DownMask&JAGPAD_BUT_KEY_2_BIT) 
      Ret|=BIT_11<<Offset4;
    if(DownMask&JAGPAD_BUT_FIRE_C_BIT) 
      Ret|=BIT_17<<Offset2;
    break;
  case 0x7:// group 1
    DownMask=GetJagPadDown(n,JAGPAD_BUT_KEY_HASH_BIT|JAGPAD_BUT_KEY_9_BIT|
      JAGPAD_BUT_KEY_6_BIT|JAGPAD_BUT_KEY_3_BIT|JAGPAD_BUT_KEY_OPTION_BIT);
    if(DownMask&JAGPAD_BUT_KEY_HASH_BIT) 
      Ret|=BIT_8<<Offset4;
    if(DownMask&JAGPAD_BUT_KEY_9_BIT) 
      Ret|=BIT_9<<Offset4;
    if(DownMask&JAGPAD_BUT_KEY_6_BIT) 
      Ret|=BIT_10<<Offset4;
    if(DownMask&JAGPAD_BUT_KEY_3_BIT) 
      Ret|=BIT_11<<Offset4;
    if(DownMask&JAGPAD_BUT_KEY_OPTION_BIT) 
      Ret|=BIT_17<<Offset2;
    break;
  case 0xe:
    DownMask=GetJagPadDown(n,JAGPAD_DIR_U_BIT|JAGPAD_DIR_D_BIT|JAGPAD_DIR_L_BIT|JAGPAD_DIR_R_BIT
      |JAGPAD_BUT_KEY_PAUSE_BIT|JAGPAD_BUT_FIRE_A_BIT);
    if(DownMask&JAGPAD_DIR_U_BIT) 
      Ret|=BIT_8<<Offset4;
    if(DownMask&JAGPAD_DIR_D_BIT) 
      Ret|=BIT_9<<Offset4;
    if(DownMask&JAGPAD_DIR_L_BIT) 
      Ret|=BIT_10<<Offset4;
    if(DownMask&JAGPAD_DIR_R_BIT) 
      Ret|=BIT_11<<Offset4;
    if(DownMask&JAGPAD_BUT_KEY_PAUSE_BIT) 
      Ret|=BIT_16<<Offset2;
    if(DownMask&JAGPAD_BUT_FIRE_A_BIT) 
      Ret|=BIT_17<<Offset2;
    break;
  }
  return Ret;
}


WORD JoyReadSTEAddress(MEM_ADDRESS const addr,bool* const pIllegal) { // called by io_read()
  DU16 state(0xFFFF);
  switch(addr) {
  case 0xff9200: // $FF9200|word |Fire buttons 1-4        Bit 3 2 1 0
  {
    WORD Ret=0;
    if(Joy[N_JOY_STE_A_0].Type==JOY_TYPE_JAGPAD)
      Ret|=HIWORD(ReadJagPad(N_JOY_STE_A_0));
    else
    {
      Ret|=((stick[N_JOY_STE_A_0]&BIT_7)!=0);
      Ret|=((stick[N_JOY_STE_A_1]&BIT_7)!=0)*BIT_1;
    }
    if(Joy[N_JOY_STE_B_0].Type==JOY_TYPE_JAGPAD)
      Ret|=HIWORD(ReadJagPad(N_JOY_STE_B_0));
    else
    {
      Ret|=((stick[N_JOY_STE_B_0]&BIT_7)!=0)*BIT_2;
      Ret|=((stick[N_JOY_STE_B_1]&BIT_7)!=0)*BIT_3;
    }
    state.d8[LO]=~LOBYTE(Ret);
    break;
  }
  case 0xff9202:
  {
    WORD Ret=0;
    if(Joy[N_JOY_STE_A_0].Type==JOY_TYPE_JAGPAD)
      Ret|=ReadJagPad(N_JOY_STE_A_0);
    else
    {
      Ret|=(stick[N_JOY_STE_A_0]&b1000)>>3;
      Ret|=(stick[N_JOY_STE_A_0]&b0100)>>1;
      Ret|=(stick[N_JOY_STE_A_0]&b0010)<<1;
      Ret|=(stick[N_JOY_STE_A_0]&b0001)<<3;
      Ret|=(stick[N_JOY_STE_A_1]&b1000)<<5;
      Ret|=(stick[N_JOY_STE_A_1]&b0100)<<7;
      Ret|=(stick[N_JOY_STE_A_1]&b0010)<<9;
      Ret|=(stick[N_JOY_STE_A_1]&b0001)<<11;
    }
    if(Joy[N_JOY_STE_B_0].Type==JOY_TYPE_JAGPAD)
      Ret|=ReadJagPad(N_JOY_STE_B_0);
    else
    {
      Ret|=(stick[N_JOY_STE_B_0]&b1000)<<1;
      Ret|=(stick[N_JOY_STE_B_0]&b0100)<<3;
      Ret|=(stick[N_JOY_STE_B_0]&b0010)<<5;
      Ret|=(stick[N_JOY_STE_B_0]&b0001)<<7;
      Ret|=(stick[N_JOY_STE_B_1]&b1000)<<9;
      Ret|=(stick[N_JOY_STE_B_1]&b0100)<<11;
      Ret|=(stick[N_JOY_STE_B_1]&b0010)<<13;
      Ret|=(stick[N_JOY_STE_B_1]&b0001)<<15;
    }
    state.d16=~Ret;
    break;
  }
/*
FF9210 ---- ---- xxxx xxxx (X Paddle 0)
FF9212 ---- ---- xxxx xxxx (Y Paddle 0)
FF9214 ---- ---- xxxx xxxx (X Paddle 1)
FF9216 ---- ---- xxxx xxxx (Y Paddle 1)

 Paddles. One pair of paddles can be plugged into Joystick 0 (Paddle 0).
 A second set can be plugged into Joystick 1 (Paddle 1).
 The current position of each of the four paddles is reported at these
 locations. The fire buttons are the same as for the respective joystick.
 The triggers for the paddles are read as bits one and two of FF9202
 (JOY0 Left and Right)
 Thx Petari for proof of concept (check his versions of Oids:
 https://atari.8bitchip.info/SCRSH/oids.html ).
*/
  case 0xff9210:
  case 0xff9212:
  case 0xff9214:
  case 0xff9216:
  {
    int a=addr-0xff9210;
    int st_joystick=(a<4)?N_JOY_STE_A_0:N_JOY_STE_B_0;
    int axis=(a&2)?1:0;
    int DirID=Joy[st_joystick].DirID[ID_FIRE];
    int joynum=(HIBYTE(DirID)-10)/10;
    BYTE rv;
    if(!IsJoyActive(st_joystick))
      // my STE with no STE joystick in
      // TODO in fact there's some counter?
      rv=0x80+(rand()%15)+(0x10*(a==0))+(0x20*(a==2));
    else
      rv=4+(HIBYTE(GetAxisPosition(axis,&JoyPos[joynum]))/4);
    state.d8[LO]=rv;
    break;
  }
  // Lightgun/pen: there are no good ones on the PC anymore,
  // since we have LCD screens.
  // There's no software either?
  case 0xff9220: 
    state.d16=0xFBFE; // Don't ask why
    break;
  case 0xff9222: 
    state.d16=0xFBFE; // Don't ask why
    break;
  default: 
    *pIllegal=true;
  }
  return state.d16;
}


void FreeJoysticks() {

#ifdef WIN32
#ifndef NO_DIRECTINPUT
  DIFreeJoysticks();
#endif
  NumJoysticks=0;
  *(DWORDLONG*)&JoyExists[0]=0;
#endif//WIN32

#ifdef UNIX
  for(int j=0;j<MAX_PC_JOYS;j++)
    XCloseJoystick(j);
#endif
}


// InitJoysticks() can be called on player request, when he rescans USB joysticks,
// and on loading an ini (including on startup)
void InitJoysticks(BYTE const Method) {
  //ASSERT(N_JOY_PORT_0==0);
  //ASSERT(MAX_ST_JOYS==8 && MAX_PC_JOYS==8);
#ifdef WIN32
  FreeJoysticks();
  JoyReadMethod=Method;
  if(Method==PCJOY_READ_DONT) // ARG_NOPCJOYSTICKS
    return;
  if(Method==PCJOY_READ_DI)
  {
#ifndef NO_DIRECTINPUT
    DIInitJoysticks();
#endif
  }
#if !defined(SSE_NO_JOYSTICK_MM)
/*  We disabled Multimedia joystick support because joyGetDevCaps (or joyGetPos
    and joyGetPosEx for that matter) returns unexpected value JOYERR_PARMS or
    crashes on recent systems, reason unknown.
    The same system that returns JOYERR_PARMS when there's no joystick works for
    the connected joystick, so this doesn't seem to be a parameter problem.
    We leave the code in case it could be fixed in a future version.
*/
  else if(Method==PCJOY_READ_WINMM)
  {
    JOYCAPS jc;
    for(UINT i=0;i<2;i++)
    { // 2 is max for MMSystem
      TRACE("joyGetDevCaps(%d,%p,%u)=%d\n",i,&jc,sizeof(JOYCAPS),joyGetDevCaps(i,&jc,sizeof(JOYCAPS)));
      if(joyGetDevCaps(i,&jc,sizeof(JOYCAPS))==JOYERR_NOERROR)
      {
        JoyExists[i]=true;
        JoyInfo[i].NumButtons=jc.wNumButtons;
        JoyInfo[i].AxisExists[AXIS_X]=true;
        JoyInfo[i].AxisExists[AXIS_Y]=true;
        JoyInfo[i].AxisExists[AXIS_Z]=(jc.wCaps & JOYCAPS_HASZ)!=0;
        JoyInfo[i].AxisExists[AXIS_R]=(jc.wCaps & JOYCAPS_HASR)!=0;
        JoyInfo[i].AxisExists[AXIS_U]=(jc.wCaps & JOYCAPS_HASU)!=0;
        JoyInfo[i].AxisExists[AXIS_V]=(jc.wCaps & JOYCAPS_HASV)!=0;
        JoyInfo[i].AxisExists[AXIS_POV]=(jc.wCaps & JOYCAPS_HASPOV)!=0;
        JoyInfo[i].NeedsEx=JoyInfo[i].AxisExists[AXIS_R]||JoyInfo[i].AxisExists[AXIS_U]||
          JoyInfo[i].AxisExists[AXIS_V]||JoyInfo[i].AxisExists[AXIS_POV]||
          (JoyInfo[i].NumButtons>4);
        JoyInfo[i].ExFlags=JOY_RETURNBUTTONS|JOY_RETURNCENTERED|JOY_RETURNX|JOY_RETURNY;
        if(JoyInfo[i].AxisExists[AXIS_Z]) JoyInfo[i].ExFlags|=JOY_RETURNZ;
        if(JoyInfo[i].AxisExists[AXIS_R]) JoyInfo[i].ExFlags|=JOY_RETURNR;
        if(JoyInfo[i].AxisExists[AXIS_U]) JoyInfo[i].ExFlags|=JOY_RETURNU;
        if(JoyInfo[i].AxisExists[AXIS_V]) JoyInfo[i].ExFlags|=JOY_RETURNV;
        if(JoyInfo[i].AxisExists[AXIS_POV]) JoyInfo[i].ExFlags|=JOY_RETURNPOVCTS;
        JoyInfo[i].AxisMin[AXIS_X]=jc.wXmin;JoyInfo[i].AxisMax[AXIS_X]=jc.wXmax;
        JoyInfo[i].AxisMin[AXIS_Y]=jc.wYmin;JoyInfo[i].AxisMax[AXIS_Y]=jc.wYmax;
        if(JoyInfo[i].AxisExists[AXIS_Z])
        {
          JoyInfo[i].AxisMin[AXIS_Z]=jc.wZmin;JoyInfo[i].AxisMax[AXIS_Z]=jc.wZmax;
        }
        if(JoyInfo[i].AxisExists[AXIS_R])
        {
          JoyInfo[i].AxisMin[AXIS_R]=jc.wRmin;JoyInfo[i].AxisMax[AXIS_R]=jc.wRmax;
        }
        if(JoyInfo[i].AxisExists[AXIS_U])
        {
          JoyInfo[i].AxisMin[AXIS_U]=jc.wUmin;JoyInfo[i].AxisMax[AXIS_U]=jc.wUmax;
        }
        if(JoyInfo[i].AxisExists[AXIS_V])
        {
          JoyInfo[i].AxisMin[AXIS_V]=jc.wVmin;JoyInfo[i].AxisMax[AXIS_V]=jc.wVmax;
        }
        for(int n=0;n<6;n++)
        {
          if(JoyInfo[i].AxisExists[n])
          {
            if(JoyInfo[i].AxisMin[n]>JoyInfo[i].AxisMax[n])
            {
              UINT Temp=JoyInfo[i].AxisMin[n];
              JoyInfo[i].AxisMin[n]=JoyInfo[i].AxisMax[n];
              JoyInfo[i].AxisMax[n]=Temp;
            }
            JoyInfo[i].AxisMid[n]=(JoyInfo[i].AxisMax[n]+JoyInfo[i].AxisMin[n])/2;
            JoyInfo[i].AxisLen[n]=JoyInfo[i].AxisMax[n]-JoyInfo[i].AxisMin[n];
          }
        }
        JoyInfo[i].WaitRead=0;
        JoyInfo[i].WaitReadTime=50; // 50 VBLs
        NumJoysticks++;
      }
    }
  }
#endif//#if !defined(SSE_NO_JOYSTICK_MM)
  JoyGetPoses();
#endif//WIN32

#ifdef UNIX
  NumJoysticks=0;
  if(Method!=PCJOY_READ_DONT)
  {
    for(int j=0;j<MAX_PC_JOYS;j++)
    {
      XOpenJoystick(j);
      if(JoyExists[j]==0)
        JoyInfo[j].On=0;
    }
    JoyGetPoses();
  }
#endif

}


void JoyGetPoses() {

#ifdef WIN32
#if !defined(SSE_NO_JOYSTICK_MM)
  static JOYINFO ji;
  bool Fail;
#endif
  for(int n=0;n<MAX_PC_JOYS;n++)
  {
    if(JoyExists[n])
    {
#if !defined(SSE_NO_JOYSTICK_MM)
      if(JoyReadMethod==PCJOY_READ_WINMM)
      {
        if(JoyInfo[n].WaitRead)
        {
          JoyInfo[n].WaitRead--;
        }
        else
        {
          if(JoyInfo[n].NeedsEx==0)
          {
            Fail=(joyGetPos(n,&ji)!=JOYERR_NOERROR);
            if(!Fail)
            {
              JoyPos[n].dwXpos=ji.wXpos;
              JoyPos[n].dwYpos=ji.wYpos;
              if(JoyInfo[n].AxisExists[AXIS_Z]) JoyPos[n].dwZpos=ji.wZpos;
              JoyPos[n].dwButtons=ji.wButtons;
            }
          }
          else
          {
            JoyPos[n].dwSize=sizeof(JOYINFOEX);
            JoyPos[n].dwFlags=JoyInfo[n].ExFlags;
            Fail=(joyGetPosEx(n,&JoyPos[n])!=JOYERR_NOERROR);
          }
          if(Fail)
          {
            JoyPosReset(n); // Nothing stuck on
            JoyInfo[n].WaitRead=JoyInfo[n].WaitReadTime; // Wait this many VBLs
            // Max 3 minutes between reads
            JoyInfo[n].WaitReadTime=MIN(JoyInfo[n].WaitReadTime*2,50*60*3);
          }
          else
            JoyInfo[n].WaitReadTime=20; // 20 VBLs
        }
      }
      else
#endif//#if defined(SSE_NO_JOYSTICK_MM)
      if(JoyReadMethod==PCJOY_READ_DI)
        DIJoyGetPos(n);
    }
  }
#endif//WIN32

#ifdef UNIX
  for (int n=0;n<MAX_PC_JOYS;n++){
    if (JoyExists[n]){
#ifndef UNIX_FAKE_JOY

#ifdef LINUX
      if (JoyInfo[n].NoEvent){
      	struct JS_DATA_TYPE js;
    		if (read(JoyInfo[n].Dev,&js,JS_RETURN)==JS_RETURN){
          JoyPos[n].dwButtons=(DWORD)js.buttons;

          TJoystickInfo &ji=JoyInfo[n];
          for (int a=0;a<2;a++){
            DWORD val=(DWORD)((a==0) ? js.x:js.y);
            if (val>ji.AxisMax[a]) val=ji.AxisMax[a];
            if (val<ji.AxisMin[a]) val=ji.AxisMin[a];
            int dzlen=((ji.AxisLen[a]/2)*ji.AxisDZ[a])/100;
            if (val>ji.AxisMid[a]-dzlen && val<ji.AxisMid[a]+dzlen){
              val=ji.AxisMid[a];
            }
            if (a==0) JoyPos[n].dwXpos=val;
            if (a==1) JoyPos[n].dwYpos=val;
          }
  	   	}
      }else{
        struct js_event js;
        if (read(JoyInfo[n].Dev,&js,sizeof(struct js_event))==sizeof(struct js_event)){

          switch (js.type & ~JS_EVENT_INIT){

            case JS_EVENT_BUTTON:
              if (js.number>31) break;
              if (js.value){
                JoyPos[n].dwButtons|=1 << js.number;
              }else{
                JoyPos[n].dwButtons&=~(1 << js.number);
              }
              break;
            case JS_EVENT_AXIS:
            {
              if (js.number>5) break;

              TJoystickInfo &ji=JoyInfo[n];

              int a=js.number;
              // js.value is a number between -32767 and +32768
              UINT val=js.value + 32767; // make unsigned
              if (val>ji.AxisMax[a]) val=ji.AxisMax[a];
              if (val<ji.AxisMin[a]) val=ji.AxisMin[a];
              int dzlen=((ji.AxisLen[a]/2)*ji.AxisDZ[a])/100;
              if (val>ji.AxisMid[a]-dzlen && val<ji.AxisMid[a]+dzlen){
                val=ji.AxisMid[a];
              }
              switch (js.number){
                case 0: JoyPos[n].dwXpos=val; break;
                case 1: JoyPos[n].dwYpos=val; break;
                case 2: JoyPos[n].dwZpos=val; break;
                case 3: JoyPos[n].dwRpos=val; break;
                case 4: JoyPos[n].dwUpos=val; break;
                case 5: JoyPos[n].dwVpos=val; break;
              }
              break;
            }
          }
        }
      }
#endif

#else
      TJoystickInfo &ji=JoyInfo[n];
      for (int a=0;a<6;a++){
        UINT val=rand() % 65535;
        if (val>ji.AxisMax[a]) val=ji.AxisMax[a];
        if (val<ji.AxisMin[a]) val=ji.AxisMin[a];
        int dzlen=((ji.AxisLen[a]/2)*ji.AxisDZ[a])/100;
        if (val>ji.AxisMid[a]-dzlen && val<ji.AxisMid[a]+dzlen){
        	val=ji.AxisMid[a];
        }
        switch (a){
          case 0: JoyPos[n].dwXpos=val; break;
          case 1: JoyPos[n].dwYpos=val; break;
          case 2: JoyPos[n].dwZpos=val; break;
          case 3: JoyPos[n].dwRpos=val; break;
          case 4: JoyPos[n].dwUpos=val; break;
          case 5: JoyPos[n].dwVpos=val; break;
        }
      }
      JoyPos[n].dwButtons=rand();
#endif
    }
  }

#endif//UNIX

}


//--------------------------------- CONFIG ----------------------------------

#define JOYCONFIG_HEIGHT(hot) (10+10+30+(hot)+285+10+28+10+6+GuiSM.cy_caption())

TJoystickConfig::TJoystickConfig() {
  Section="Joysticks";

#ifdef WIN32
#if !defined(SSE_420R4)
  Left=(GuiSM.cx_screen()-546)/2;
  Top=(GuiSM.cy_screen()-JOYCONFIG_HEIGHT(20))/2;
  FSLeft=(640-546)/2;
  FSTop=(480-JOYCONFIG_HEIGHT(20))/2;
#endif
#endif

#ifdef UNIX 
  for(int j=0;j<MAX_PC_JOYS;j++)
  {
    JoyInfo[j].Range=65535;
    JoyInitCalibration(j);
    JoyInfo[j].DeviceFile=Str("/dev/js")+char('0'+j);
    JoyInfo[j].Dev=-1;
  }
  ConfigST=true;
  PCJoyEdit=0;
  for(int n=0;n<256;n++)
    KeyboardButtonName[n]="";
  for(int s=0;s<2;s++)
    for (int n=0;n<JOY_N_DIR_ID;n++)
      picker[s][n].allow_joy=true;
#endif
}


void TJoystickConfig::Show() {
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
  short &wsep=GuiSM.mHorizontalSeparation;
  int Twenty=(BIG_ICONS)?36:20;
  ManageWindowClasses(SD_REGISTER);
  Handle=CreateWindowEx(WS_EX_CONTROLPARENT|WS_EX_APPWINDOW,"Steem Joystick Config",T("Joysticks"),
                        WS_CAPTION|WS_SYSMENU,Left,Top,0,0,ParentWin,NULL,hInstance,NULL);
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
#if defined(SSE_NO_JOYSTICK_MM)
  HWND Win=CreateWindow("Button",T("Rescan"),WS_CHILD|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE|WS_VISIBLE,
                        10,14,90,CharHeight,Handle,(HMENU)IDC_RESCAN,hInstance,NULL);
#else
  int w=get_text_width(T("Read PC joystick(s) using"));
  CreateWindow("Static",T("Read PC joystick(s) using"),WS_CHILD|WS_VISIBLE,
    10,14,w,23,Handle,(HMENU)90,hInstance,NULL);
  HWND Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|WS_VISIBLE|
    CBS_HASSTRINGS|CBS_DROPDOWNLIST,
    15+w,10,530-(15+w),200,Handle,(HMENU)91,hInstance,NULL);
  CBAddString(Win,T("Nothing (PC Joysticks Off)"),PCJOY_READ_DONT);
  CBAddString(Win,T("Windows Multimedia"),PCJOY_READ_WINMM);
#ifndef NO_DIRECTINPUT
  CBAddString(Win,T("DirectInput"),PCJOY_READ_DI);
#endif
  CBSelectItemWithData(Win,JoyReadMethod);
#endif
  int w=get_text_width(T("Current configuration"));
  CreateWindow("Static",T("Current configuration"),WS_CHILD|WS_VISIBLE,
    10+120,44-30+5,w,CharHeight,Handle,(HMENU)IDS_STATIC,hInstance,NULL);
  Win=CreateWindow("Combobox","",WS_CHILD|WS_TABSTOP|WS_VISIBLE|CBS_HASSTRINGS|CBS_DROPDOWNLIST,
    120+15+w,40-30+5,530-(15+w)-120,200,Handle,(HMENU)IDC_SETUP,hInstance,NULL);
  for(int n=1;n<=JOYSTICK_SETUPS;n++) 
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)(T("Joystick Setup")+" #"+n).Text);
  SendMessage(Win,CB_SETCURSEL,nJoySetup,0);
  HWND Tabs=CreateWindow(WC_TABCONTROL,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|TCS_FOCUSONBUTTONDOWN,
    10,70-25,520,310+20+25,Handle,(HMENU)IDC_JOYTABS,hInstance,NULL);
  TC_ITEM tci;
  tci.mask=TCIF_TEXT;
  tci.pszText=StaticT("Standard Ports");
  SendMessage(Tabs,TCM_INSERTITEM,0,(LPARAM)&tci);
  tci.pszText=StaticT("STE Port A");
  SendMessage(Tabs,TCM_INSERTITEM,1,(LPARAM)&tci);
  tci.pszText=StaticT("STE Port B");
  SendMessage(Tabs,TCM_INSERTITEM,2,(LPARAM)&tci);
  tci.pszText=StaticT("Parallel Ports");
  SendMessage(Tabs,TCM_INSERTITEM,3,(LPARAM)&tci);
  SendMessage(Tabs,TCM_SETCURSEL,BasePort/2,0);
  RECT rc;
  GetTabControlPageSize(Tabs,&rc);
  // rc.top=70+height of tab buttons
  int HeightOfTabButs=(rc.top-70);
  int tabheight=HeightOfTabButs+GUIMUL(330);
  SetWindowPos(Tabs,0,0,0,GUIMUL(520),tabheight,SWP_NOZORDER|SWP_NOMOVE);
  SetWindowPos(Handle,0,0,0,GUIMUL(546),JOYCONFIG_HEIGHT(HeightOfTabButs)
    *FONT_SIZE/GUI_FONT_SIZE,SWP_NOZORDER|SWP_NOMOVE);
  int x=Twenty,y,FireY;
  for(INT_PTR p=0;p<2;p++)
  {
    INT_PTR group=(p+1)*GROUP_BASE; // 100 (left) or 200 (right)
    y=rc.top;
    Group[p]=CreateWindow("Button","",WS_CHILD|WS_VISIBLE|BS_GROUPBOX|WS_CLIPCHILDREN,
      x,y,GUIMUL(245),GUIMUL(275+CharHeight),Handle,(HMENU)(group),hInstance,NULL);
    if(p==0) // left pane
    {
      SetWindowLongPtr(Group[p],GWLP_USERDATA,(LONG_PTR)this);
      OldGroupBoxWndProc=(WNDPROC)SetWindowLongPtr(Group[p],GWLP_WNDPROC,(LONG_PTR)GroupBoxWndProc);
      w=GetCheckBoxSize(Font,"JagPad").Width;
      JagBut=CreateWindow("Button","JagPad",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
        x+GUIMUL(235)-w,y+GUIMUL(275),w,CharHeight,Handle,(HMENU)IDC_JAGPAD,hInstance,NULL);
      SendMessage(JagBut,WM_SETFONT,(WPARAM)Font,0);
    }
    y+=Twenty;
    w=get_text_width(T("Active"));
    CreateWindow("Static",T("Active"),WS_CHILD|WS_VISIBLE|SS_CENTERIMAGE,
      x+10,y,w,CharHeight,Handle,(HMENU)(group+IDS_ACTIVE),hInstance,NULL);
    Win=CreateWindow("Combobox","",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|CBS_HASSTRINGS
      |CBS_DROPDOWNLIST,x+15+w,y,CbUnits*8,200,Handle,(HMENU)(group+IDC_ACTIVE),hInstance,NULL);
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Never"));
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Always"));
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("When Scroll Lock On"));
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("When Num Lock Off"));
    switch(Joy[BasePort+p].ToggleKey) {
    case VK_SCROLL:
      SendMessage(Win,CB_SETCURSEL,2,0);
      break;
    case VK_NUMLOCK:
      SendMessage(Win,CB_SETCURSEL,3,0);
      break;
    default:
      SendMessage(Win,CB_SETCURSEL,Joy[BasePort+p].ToggleKey,0);
    }
    y+=Twenty+10;
#if defined(SSE_JOYSTICK_JUMP_BUTTON) // move up button to the left
    Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Button Picker","",WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP
      |WS_DISABLED,x+90-35,y,65,CharHeight,Handle,(HMENU)(group+IDC_JOYBUTTON+ID_UP),hInstance,NULL);
#else
    Win=CreateWindowEx(512,"Steem Button Picker","",
      WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|WS_DISABLED,
      x+90,y,65,23,Handle,(HMENU)(10+(p+1)*100),hInstance,NULL);
#endif
    SetWindowWord(Win,0,(WORD)Joy[BasePort+p].DirID[ID_UP]);
#if defined(SSE_JOYSTICK_JUMP_BUTTON) // create jump button (=up)
    Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Button Picker","",
      WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|WS_DISABLED,
      x+90+70-35,y,65,CharHeight,Handle,(HMENU)(group+IDC_JOYBUTTON+ID_JUMP),hInstance,NULL);
    ToolAddWindow(ToolTip,Win,T("Here you can assign a jump button to go up"));
    SetWindowWord(Win,0,(WORD)Joy[BasePort+p].DirID[ID_JUMP]);
#endif
    y+=Twenty+8;
    // DeadZone
    Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Joystick DeadZone","",WS_CHILDWINDOW|WS_VISIBLE
      |WS_DISABLED,x+88,y,70,70,Handle,(HMENU)(group+IDC_DEADZONE),hInstance,NULL);
    SetWindowLongPtr(Win,GWLP_USERDATA,(LONG_PTR)this);
    y+=20+4;
    Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Button Picker","",WS_CHILD|WS_VISIBLE|WS_TABSTOP
      |WS_DISABLED,x+10,y,65,CharHeight,Handle,(HMENU)(group+IDC_JOYBUTTON+ID_LEFT),hInstance,NULL);
    SetWindowWord(Win,0,(WORD)Joy[BasePort+p].DirID[ID_LEFT]);
    Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Button Picker","",WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP
      |WS_DISABLED,x+170,y,65,CharHeight,Handle,(HMENU)(group+IDC_JOYBUTTON+ID_RIGHT),hInstance,NULL);
    SetWindowWord(Win,0,(WORD)Joy[BasePort+p].DirID[ID_RIGHT]);
    y+=Twenty+20+3+3+5;
    Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Button Picker","",WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP
      |WS_DISABLED,x+90,y,65,CharHeight,Handle,(HMENU)(group+IDC_JOYBUTTON+ID_DOWN),hInstance,NULL);
    SetWindowWord(Win,0,(WORD)Joy[BasePort+p].DirID[ID_DOWN]);
    y+=Twenty+10;
    FireY=y;
    w=get_text_width(T("Fire button"));
    CreateWindow("Static",T("Fire button"),WS_CHILD|WS_VISIBLE|SS_CENTERIMAGE,
      x+10,y,w,CharHeight,Handle,(HMENU)(group+IDS_JOYBUTTON),hInstance,NULL);
    Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Button Picker","",WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP
      |WS_DISABLED,x+w+15,y,65,CharHeight,Handle,(HMENU)(group+IDC_JOYBUTTON+ID_FIRE),hInstance,NULL);
    SetWindowWord(Win,0,(WORD)Joy[p].DirID[ID_FIRE]);
    y+=Twenty+10;
    EasyStr Text=T("Or any button on"); // real joysticks only
    CreateWindow("Static",Text,WS_CHILD|WS_VISIBLE|SS_CENTERIMAGE
      |((NumJoysticks==0)? WS_DISABLED : 0),x+10,y,GetTextSize(Font,Text).Width,
      CharHeight,Handle,(HMENU)(group+IDS_ANYBUTTON),hInstance,NULL);
    w=GetTextSize(Font,Text).Width;
    Win=CreateWindow("Combobox","",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL
      |CBS_DROPDOWNLIST|((NumJoysticks==0) ? WS_DISABLED : 0),x+w+15,y,
      235-(w+15),200,Handle,(HMENU)(group+IDC_ANYBUTTON),hInstance,NULL);
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"-");
    for(int n=0;n<MAX_PC_JOYS;n++) 
      if(JoyExists[n]) 
        SendMessage(Win,CB_ADDSTRING,0,(LPARAM)((T("Joystick")+" "+(n+1)).Text));
    y+=Twenty+10;
    w=get_text_width(T("Autofire"));
    CreateWindow("Static",T("Autofire"),WS_CHILD|WS_VISIBLE|SS_CENTERIMAGE,x+10,
      y,w,CharHeight,Handle,(HMENU)(group+IDS_AUTOFIRE),hInstance,NULL);
    int Offset=x+15+w;
    w=CbUnits*4;
    Win=CreateWindow("Combobox","",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,
      Offset,y,w,200,Handle,(HMENU)(group+IDC_AUTOFIRE),hInstance,NULL);
    Offset+=w+wsep;
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Off"));
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("V.Fast"));
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Fast"));
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Medium"));
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Slow"));
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("V.Slow"));
    Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Button Picker","",WS_CHILDWINDOW
      |WS_VISIBLE|WS_TABSTOP|WS_DISABLED,Offset,y,65,CharHeight,Handle,
      (HMENU)(group+IDC_JOYBUTTON+ID_AUTOFIRE),hInstance,NULL);
    SetWindowWord(Win,0,(WORD)Joy[p].DirID[ID_AUTOFIRE]);
    x+=GUIMUL(255);
  }//nxt p
  // Jaguar buttons/keys
  x=Twenty+10;
  w=get_text_width(T("Fire buttons"));
  CreateWindow("Static",T("Fire buttons"),WS_CHILD|SS_CENTER,x,FireY,225,
    CharHeight,Handle,(HMENU)IDS_JAGFIRES,hInstance,NULL);
  for(INT_PTR n=0;n<3;n++)
  {
    EasyStr But=(char)('A'+n);
    w=get_text_width(But);
    CreateWindow("Static",But,WS_CHILD,x,FireY+29,w,CharHeight,Handle,
      (HMENU)(IDS_JAGFIRES+1+n),hInstance,NULL);
    x+=w+2;
    CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Button Picker","",WS_CHILD|WS_TABSTOP,
      x,FireY+25,62,CharHeight,Handle,(HMENU)(IDC_JAGFIRES+n),hInstance,NULL);
    x+=65;
  }
  y=rc.top+25;
  EasyStr Option=T("Option"),Pause=T("Pause");
  char *JagButName[14]={Option,Pause,"0","1","2","3","4","5","6","7","8","9","#","*"};
  w=0;
  for(int n=0;n<14;n++) 
    w=MAX(w,get_text_width(JagButName[n]));
  for(INT_PTR n=0;n<14;n++)
  {
    x=GUIMUL(285)+(115*(n&1));
    CreateWindow("Static",JagButName[n],WS_CHILD|SS_CENTER,x,y+4,w,CharHeight,
      Handle,(HMENU)(IDS_JAGKEYS+n),hInstance,NULL);
    x+=w+2;
    CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Button Picker","",WS_CHILD|WS_TABSTOP,
      x,y,65,CharHeight,Handle,(HMENU)(IDC_JAGKEYS+n),hInstance,NULL);
    if(n&1)
      y+=Twenty+10;
  }
  y=rc.top+285+10;
  for(int p=0;p<2;p++) 
    JoyModeChange(BasePort+p); // VC2008 compiles as a loop, and it's shorter than 2 calls!
  SetWindowAndChildrensFont(Handle,Font);
  Focus=GetDlgItem(Handle,IDC_JOYTABS);
  ShowWindow(Handle,SW_SHOW);
  SetFocus(Focus);
  if(StemWin!=NULL) 
    PostMessage(StemWin,WM_USER,1234,0);
#endif//WIN32

#ifdef UNIX
  int win_h=405;
  if (StandardShow(520,win_h,T("Joysticks"),ICO16_JOY,ExposureMask,
      (LPWINDOWPROC)WinProc)) return;

  config_group.create(XD,Handle,10,20,500,win_h-20-10-25-10,NULL,this,
                      BT_GROUPBOX,"",0,BkCol);
  int page_w=config_group.w-20;

#ifndef X_NO_PC_JOYSTICKS
  config_dd.make_empty();
  config_dd.additem(T("Configure PC Joysticks"));
  config_dd.additem(T("Configure ST Joysticks"));
  config_dd.changesel(ConfigST);
  config_dd.id=6200;
  config_dd.create(XD,Handle,20,10,page_w/2-5,200,dd_notify_proc,this);
#endif

  setup_dd.make_empty();

  for(int n=1;n<=JOYSTICK_SETUPS;n++) 
    setup_dd.additem(Str("ST ")+T("Joystick Setup")+" #"+n);
/*  setup_dd.additem(Str("ST ")+T("Joystick Setup")+" #1");
  setup_dd.additem(Str("ST ")+T("Joystick Setup")+" #2");
  setup_dd.additem(Str("ST ")+T("Joystick Setup")+" #3");*/
  setup_dd.changesel(nJoySetup);
  setup_dd.id=6201;
  setup_dd.create(XD,Handle,20+page_w/2+5,10,page_w/2-5,200,dd_notify_proc,this);

  pc_group.create(XD,config_group.handle,10,25,page_w,config_group.h-30-2,
                    NULL,this,BT_STATIC | BT_TEXT | BT_BORDER_NONE,"",0,BkCol);

  //---------------------------------------------------------------------------

  Window par=pc_group.handle;
  int x=0,y=0,w;
  
  hxc_button *p_lab=new hxc_button(XD,par,page_w,y,0,25,button_notify_proc,this,BT_TEXT | BT_TOGGLE,T("Open"),6002,BkCol);
  p_lab->w+=20;
  p_lab->x-=p_lab->w;
  XMoveResizeWindow(XD,p_lab->handle,p_lab->x,p_lab->y,p_lab->w,p_lab->h);
  w=p_lab->w;

  p_lab=new hxc_button(XD,par,x,y,0,25,
                    NULL,this,BT_LABEL,T("Edit"),0,BkCol);
  x+=p_lab->w+5;

  hxc_dropdown *p_dd=new hxc_dropdown(XD,par,x,y,
                page_w-10-w-x,300,dd_notify_proc,this);
	p_dd->id=6000;
	p_dd->make_empty();
  for (int j=0;j<MAX_PC_JOYS;j++) p_dd->additem(T("PC Joystick")+" "+Str("#")+(j+1));

  y+=30;

  x=0;
  p_lab=new hxc_button(XD,par,x,y,0,25,NULL,this,BT_LABEL,T("Device"),0,BkCol);
  x+=p_lab->w+5;

  int but_w=hxc::get_text_width(XD,T("Choose"))+10;
  p_lab=new hxc_button(XD,par,page_w-but_w,y,but_w,25,
                        button_notify_proc,this,BT_TEXT,T("Choose"),6001,BkCol);

  device_ed.create(XD,par,x,y,p_lab->x-x-5,25,edit_notify_proc,this);
  y+=30;

  x=0;
  p_lab=new hxc_button(XD,par,x,y,0,25,
                    NULL,this,BT_LABEL,T("Axes"),6005,BkCol);
  x+=p_lab->w+5;

  p_dd=new hxc_dropdown(XD,par,x,y,page_w/2-5-x,300,dd_notify_proc,this);
	p_dd->id=6003;
  x+=p_dd->w+10;

  p_lab=new hxc_button(XD,par,x,y,0,25,NULL,this,BT_LABEL,T("Buttons"),6006,BkCol);
  x+=p_lab->w+5;

  p_dd=new hxc_dropdown(XD,par,x,y,page_w-x,300,dd_notify_proc,this);
	p_dd->id=6004;
	p_dd->make_empty();
  for (int b=0;b<=32;b++) p_dd->additem(Str(b));
  y+=35;

  int bar_y=y;
  int bar_w=332;
  for (int a=0;a<6;a++){
    x=0;
    p_lab=new hxc_button(XD,par,x,bar_y,0,25,NULL,this,
                BT_LABEL,Str(AxisToName[a])+" -",6107+a*10,BkCol);
    x+=p_lab->w+2;

    p_lab=new hxc_button(XD,par,bar_w-5-hxc::get_text_width(XD,"+"),bar_y,0,25,
                NULL,this,BT_LABEL,"+",6108+a*10,BkCol);

    hxc_button *p_grp=new hxc_button(XD,par,x,bar_y,bar_w-x-5-p_lab->w-2,25,NULL,this,
                BT_STATIC | BT_BORDER_INDENT,"",6101+a*10,hxc::col_border_dark);

    p_lab=new hxc_button(XD,par,0,bar_y-5,5,5,NULL,this,
                BT_ICON | BT_STATIC,"",6100+a*10,BkCol);
    p_lab->set_icon(NULL,1);

    p_lab=new hxc_button(XD,p_grp->handle,0,1,5,23,NULL,this,
                  BT_STATIC | BT_TEXT | BT_BORDER_NONE,
                  "Deadzone",6102+a*10+4,hxc::col_sel_back);
    p_lab->col_text=hxc::col_sel_fore;
    for (int i=0;i<4;i++){
      p_lab=new hxc_button(XD,p_grp->handle,0,1,5,23,button_notify_proc,this,
                  0,"",6102+a*10+i,BkCol);
      p_lab->want_drag_notify=true;
    }
    bar_y+=35;
  }

  x=bar_w+10;
  int but_y=y;
  for (int b=0;b<32;b++){
    new hxc_button(XD,par,x,but_y,25,25,NULL,this,
                  BT_STATIC | BT_TEXT,EasyStr(b+1),6300+b,BkCol);
    x+=28;
    if (x+25>page_w){
      x=bar_w+10;
      but_y+=28;
    }
  }

  y=pc_group.h-30;
  x=0;
  p_lab=new hxc_button(XD,par,x,y,0,25,NULL,this,BT_LABEL,T("Axis range"),6008,BkCol);
  x+=p_lab->w+5;

  p_dd=new hxc_dropdown(XD,par,x,y,bar_w-5-x,300,dd_notify_proc,this);
	p_dd->id=6007;
	p_dd->make_empty();
  p_dd->additem("0-100",100);
  p_dd->additem("0-256",256);
  p_dd->additem("0-512",512);
  for (int n=1000;n<65000;n+=1000){
    p_dd->additem(Str("0-")+n,n);
  }
  p_dd->additem("0-65535",65535);
  p_dd->w=p_dd->get_min_width()+5;
  XMoveResizeWindow(XD,p_dd->handle,p_dd->x,p_dd->y,p_dd->w,p_dd->h);
  x+=p_dd->w+10;

  //---------------------------------------------------------------------------

  st_group.create(XD,config_group.handle,10,25,page_w,config_group.h-30-2,NULL,this,
                    BT_STATIC | BT_BORDER_NONE,"",0,BkCol);

  par=st_group.handle;
  int st_y=0;

  p_dd=new hxc_dropdown(XD,par,0,st_y,st_group.w,300,dd_notify_proc,this);
	p_dd->id=6202;
	p_dd->make_empty();
  p_dd->additem(T("Standard Ports"));
  p_dd->additem(T("STE Port A"));
  p_dd->additem(T("STE Port B"));
  p_dd->additem(T("Parallel Ports"));
  p_dd->sel=BasePort/2;

  hxc_button *p_but=new hxc_button(XD,par,page_w,st_y,0,25,button_notify_proc,this,
                          BT_CHECKBOX,T("JagPad"),6203,BkCol);
  p_but->x-=p_but->w;
  XMoveWindow(XD,p_but->handle,p_but->x,p_but->y);
  st_y+=35;

	int GroupWid=page_w/2-5;
  int xx1=0;
  for (int s=0;s<2;s++){
  	joy_group[s].create(XD,st_group.handle,xx1,st_y,GroupWid,270,NULL,this,BT_GROUPBOX,"",0,BkCol);

    dir_par[s].create(XD,st_group.handle,xx1+10,st_y+20,GroupWid-20,30+25+40+30,NULL,this,
                        BT_BORDER_NONE | BT_STATIC,"",0,BkCol);
    par=dir_par[s].handle;
    int par_w=dir_par[s].w;
    y=0;

    enable_but[s].create(XD,par,0,y,0,25,
            button_notify_proc,this,BT_CHECKBOX,T("Active"),5000+s,BkCol);
    y+=30;

        
#if defined(SSE_JOYSTICK_JUMP_BUTTON)

    picker[s][0].create(XD,par,par_w/2-45-50,y,90,25,
        picker_notify_proc,this,s*64);
    picker[s][6].create(XD,par,par_w/2+5,y,90,25,
        picker_notify_proc,this,s*64+6);

#else

    picker[s][0].create(XD,par,par_w/2-45,y,90,25,
        picker_notify_proc,this,s*64);

#endif        
        
    y+=25;

    picker[s][2].create(XD,par,0,y+40/2-12,90,25,
        picker_notify_proc,this,s*64+2);

		centre_icon[s].create(XD,par,par_w/2-16,y+40/2-16,32,32,
            NULL,this,BT_ICON | BT_STATIC,"",-7,BkCol);
		centre_icon[s].set_icon(&Ico32,ICO32_JOYDIR);

    picker[s][3].create(XD,par,par_w-90,y+40/2-12,90,25,
        picker_notify_proc,this,s*64+3);
    y+=40;

    picker[s][1].create(XD,par,par_w/2-45,y,90,25,
        picker_notify_proc,this,s*64+1);
    y+=30;

    fire_par[s].create(XD,st_group.handle,xx1+10,dir_par[s].y+dir_par[s].h,
                        GroupWid-20,30+30+30,NULL,this,
                        BT_BORDER_NONE | BT_STATIC,"",0,BkCol);
    par=fire_par[s].handle;
    par_w=fire_par[s].w;
    y=0;

    fire_but_label[s].create(XD,par,0,y,0,25,NULL,this,
    										BT_STATIC | BT_TEXT,T("Fire"),0,BkCol);

    picker[s][4].create(XD,par,fire_but_label[s].w+5,y,90,25,
                          picker_notify_proc,this,s*64+4);
    picker[s][4].allow_joy=true;
    y+=30;

    autofire_but_label[s][0].create(XD,par,0,y,0,25,NULL,this,
    										BT_STATIC | BT_TEXT,T("Autofire speed"),0,BkCol);

	  autofire_dd[s].make_empty();
	  autofire_dd[s].additem(T("Off"));
	  autofire_dd[s].additem(T("V.Fast"));
	  autofire_dd[s].additem(T("Fast"));
	  autofire_dd[s].additem(T("Medium"));
	  autofire_dd[s].additem(T("Slow"));
	  autofire_dd[s].additem(T("V.Slow"));
	  autofire_dd[s].create(XD,par,autofire_but_label[s][0].w+5,y,
	  				MAX(autofire_dd[s].get_min_width()+5,90),200,dd_notify_proc,this);
		autofire_dd[s].id=s;
    y+=30;

    autofire_but_label[s][1].create(XD,par,0,y,0,25,NULL,this,
    										BT_STATIC | BT_TEXT,T("Autofire button"),0,BkCol);

    picker[s][5].create(XD,par,autofire_but_label[s][1].w+5,y,90,25,
						        picker_notify_proc,this,s*64+5);

    hxc_buttonpicker *p_bp;
    if (s==0){
      jagpad_par[s].create(XD,st_group.handle,xx1+10,dir_par[s].y+dir_par[s].h,
                              GroupWid-20,30+30+30,NULL,this,
                              BT_BORDER_NONE | BT_STATIC,"",0,BkCol);
      par=jagpad_par[s].handle;
      par_w=jagpad_par[s].w;
      y=5;

      for (int i=0;i<3;i++){
        p_lab=new hxc_button(XD,par,0,y,0,25,NULL,this,
                        BT_LABEL,T("Button")+" "+char('A'+i),0,BkCol);
        p_bp=new hxc_buttonpicker(XD,par,5+p_lab->w,y,90,25,picker_notify_proc,this,6+i);
        p_bp->allow_joy=true;
        y+=30;
      }
    }else{
      jagpad_par[s].create(XD,st_group.handle,xx1+10,dir_par[s].y,GroupWid-20,
                            30*7+30,NULL,this,
                            BT_BORDER_NONE | BT_STATIC,"",0,BkCol);
      par=jagpad_par[s].handle;
      par_w=jagpad_par[s].w;
      y=0;

      EasyStr Option=T("Option"),Pause=T("Pause");
      char *JagButName[14]={Option,Pause,"0","1","2","3","4","5","6","7","8","9","#","*"};
      int x=0;
      for (int n=0;n<14;n++){
        p_lab=new hxc_button(XD,par,x,y,0,25,NULL,this,
                        BT_LABEL,JagButName[n],0,BkCol);
        p_bp=new hxc_buttonpicker(XD,par,x+2+p_lab->w,y,90,25,picker_notify_proc,this,9+n);
        p_bp->allow_joy=true;

        x+=par_w/2;
        if ((n & 1) || n<2){
          x=0;
          y+=30;
        }
      }
    }
    xx1+=GroupWid+10;
  }

  ShowAndUpdatePage();

  y=config_group.y+config_group.h+10;
  
#if 0  
  MouseSpeedLabel[0].create(XD,Handle,10,y,0,25,NULL,this,
    										BT_LABEL,T("Mouse speed")+": "+T("Min"),0,BkCol);

  int Wid=hxc::get_text_width(XD,T("Max"));
  MouseSpeedLabel[1].create(XD,Handle,config_group.x+config_group.w-Wid,y,Wid,25,NULL,this,
    										BT_LABEL,T("Max"),0,BkCol);

  MouseSpeedSB.horizontal=true;
  MouseSpeedSB.init(19+4,4,mouse_speed-1);
  MouseSpeedSB.create(XD,Handle,15+MouseSpeedLabel[0].w,y,
                      config_group.x+config_group.w-(15+MouseSpeedLabel[0].w)-(Wid+5),
                      25,scrollbar_notify_proc,this);
#endif
  XMapWindow(XD,Handle);

  if (StemWin) JoyBut.set_check(true);
#endif
}


void TJoystickConfig::Hide() {
  if(Handle==NULL) 
    return;
#ifdef WIN32
  ShowWindow(Handle,SW_HIDE);
  if(FullScreen) 
    SetFocus(StemWin);
  DestroyWindow(Handle);
  Handle=NULL;
  if(StemWin) 
    PostMessage(StemWin,WM_USER,1234,0);
  ManageWindowClasses(SD_UNREGISTER);
#endif//WIN32

#ifdef UNIX
  if (XD==NULL) return;

  hxc::kill_timer(Handle,HXC_TIMER_ALL_IDS);
  StandardHide();

  if (StemWin) JoyBut.set_check(0);
#endif
}


void TJoystickConfig::CreateJoyAnyButtonMasks() {
  // Create a bitmask so that buttons used for directions/autofire don't count
  // as fire when Any Button On.. is used
  int n;
  for(int j=0;j<MAX_PC_JOYS;j++)
  {
    JoyAnyButtonMask[j]=(DWORD)(pow(2.0,(double)(JoyInfo[j].NumButtons))-1);
    for(int Port=N_JOY_PORT_0;Port<MAX_ST_JOYS;Port++)
    {
      for(n=0;n<JOY_N_DIR_ID;n++)
      {
        if(n==ID_FIRE) 
          n++;
        if(n==ID_AUTOFIRE && Joy[Port].AutoFireSpeed==0)
          break;
        int DirID=Joy[Port].DirID[n];
        if(HIBYTE(DirID)==10+(j*10))
        {
          if(LOBYTE(DirID)>=100 && LOBYTE(DirID)<200)
            JoyAnyButtonMask[j]&=~(1<<(LOBYTE(DirID)-100));
        }
      }
    }
  }
}


// 4.1.2 refactoring for the new shortcuts
void TJoystickConfig::JoySetupUpdate(bool const memorise) {
  for(int n=N_JOY_PORT_0;n<MAX_ST_JOYS;n++)
  {
    if(memorise)
      JoySetup[nJoySetup][n]=Joy[n];
    else
      Joy[n]=JoySetup[nJoySetup][n];
  }
#ifdef WIN32
  if(!memorise && IsVisible())
  {
    JoyModeChange(BasePort); // update GUI
    JoyModeChange(BasePort+1);
    if(HWND hwnd=GetDlgItem(Handle,IDC_SETUP))
    {
      BYTE CurrentSetup=(BYTE)SendMessage(hwnd,CB_GETCURSEL,0,0);
      if(CurrentSetup!=nJoySetup)
        SendMessage(hwnd,CB_SETCURSEL,(WPARAM)nJoySetup,0);
    }
    UPDATE_STATUS_BAR_PART(SB_PART_CAPS);
  }
#endif
}


#ifdef WIN32

void TJoystickConfig::ManageWindowClasses(bool const Unreg) {
  WNDCLASS wc;
  char *ClassName[2]={"Steem Joystick Config","Steem Joystick DeadZone"};
  if(Unreg)
  {
    for(int n=0;n<2;n++) 
      UnregisterClass(ClassName[n],hInstance);
  }
  else
  {
    RegisterMainClass(WndProc,ClassName[0],RC_ICO_JOY);
    wc.style=CS_DBLCLKS;
    wc.hInstance=(HINSTANCE)GetModuleHandle(NULL);
    wc.hIcon=NULL;
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName=NULL;
    wc.lpfnWndProc=TJoystickConfig::DeadZoneWndProc;
    wc.cbClsExtra=0;
    wc.cbWndExtra=4;
    wc.lpszClassName=ClassName[1];
    RegisterClass(&wc);
  }
}


bool TJoystickConfig::HasHandledMessage(MSG* const mess) {
  if(mess->message==WM_KEYDOWN && mess->wParam==VK_TAB)
  {
    if(GetKeyState(VK_CONTROL)>=0) 
      return !!IsDialogMessage(Handle,mess);
  }
  return false;
}


void TJoystickConfig::FillJoyTypeCombo() {
  if(BasePort==N_JOY_STE_A_0||BasePort==N_JOY_STE_B_0)
  {
    SendMessage(JagBut,BM_SETCHECK,(Joy[BasePort].Type==JOY_TYPE_JAGPAD)?BST_CHECKED:BST_UNCHECKED,0);
    EnableWindow(JagBut,TRUE);
    ShowWindow(JagBut,SW_SHOW);
  }
  else
  {
    EnableWindow(JagBut,FALSE);
    ShowWindow(JagBut,SW_HIDE);
  }
}


void TJoystickConfig::CheckJoyType() { // only called by JoyModeChange()
  HWND hwnd=GetDlgItem(Handle,IDC_JAGKEYS); // one of the jagpad buttons (was 170)
  bool Port1Jagpad=((GetWindowLong(hwnd,GWL_STYLE)&WS_VISIBLE)!=0);
  int JagShowType=SW_HIDE,NormShowType=SW_SHOW;
  bool Port1Alter=Port1Jagpad;
  if(BasePort==N_JOY_STE_A_0||BasePort==N_JOY_STE_B_0)
  {
    if(Joy[BasePort].Type==JOY_TYPE_JAGPAD)
    {
      if(Port1Jagpad)
        Port1Alter=false;
      else
      {
        Port1Alter=true;
        JagShowType=SW_SHOW;
        NormShowType=SW_HIDE;
      }
    }
  }
  if(Port1Alter)
  {
    BOOL DoJag=(JagShowType==SW_HIDE);
    for(int m=0;m<2;m++)
    {
      if(DoJag)
      {
        for(int n=IDS_JAGFIRES;n<IDS_JAGKEYS+14;n++)
          if(GetDlgItem(Handle,n)) 
            ShowWindow(GetDlgItem(Handle,n),JagShowType);
        for(int n=IDC_JAGFIRES;n<IDC_JAGKEYS+14;n++)
          if(GetDlgItem(Handle,n)) 
            ShowWindow(GetDlgItem(Handle,n),JagShowType);
      }
      else
      {
        int const group[2]={GROUP_BASE,GROUP_BASE*2};
        for(int n=group[0]+IDC_JOYBUTTON+4;n<=group[0]+IDC_ANYBUTTON;n++)
          ShowWindow(GetDlgItem(Handle,n),NormShowType);
        for(int n=group[0]+IDS_JOYBUTTON;n<=group[0]+IDS_AUTOFIRE;n++)
          ShowWindow(GetDlgItem(Handle,n),NormShowType);
        for(int n=group[1]+IDS_ACTIVE;n<=group[1]+IDC_ACTIVE;n++)
          ShowWindow(GetDlgItem(Handle,n),NormShowType);
        for(int n=group[1]+IDC_JOYBUTTON;n<=group[1]+IDC_ANYBUTTON;n++)
          ShowWindow(GetDlgItem(Handle,n),NormShowType);
        ShowWindow(GetDlgItem(Handle,group[1]+IDC_DEADZONE),NormShowType);
        for(int n=group[1]+IDS_JOYBUTTON;n<=group[1]+IDS_AUTOFIRE;n++)
          ShowWindow(GetDlgItem(Handle,n),NormShowType);
      }
      ShowWindow(GetDlgItem(Handle,IDS_ANYBUTTON+(m+1)*GROUP_BASE),NormShowType);
      DoJag=!DoJag;
    }
  }
  EasyStr PortName[2];
  if(BasePort==N_JOY_PORT_0)
  {
    PortName[0]=T("Port 0 (disables mouse!)");
    PortName[1]=T("Port 1");
  }
  else if(BasePort==N_JOY_STE_A_0||BasePort==N_JOY_STE_B_0)
  {
    PortName[0]=LPSTR(Joy[BasePort].Type ? T("Pad") : T("Stick 0"));
    PortName[1]=LPSTR(Joy[BasePort].Type ? T("Pad Keyboard") : T("Stick 1"));
  }
  else
  {
    PortName[0]=T("Parallel 0");
    PortName[1]=T("Parallel 1");
  }
  SendMessage(Group[0],WM_SETTEXT,0,LPARAM(PortName[0].Text));
  SendMessage(Group[1],WM_SETTEXT,0,LPARAM(PortName[1].Text));
}


void TJoystickConfig::JoyModeChange(int const Port) {
  int group=GROUP_BASE+(Port&1)*GROUP_BASE;
  BOOL NewMode=(Joy[Port].ToggleKey>=TJoystickConfig::ACTIVE_NEVER);
  // no need to optimize the call in the switch, VS2019 does it
  switch(Joy[Port].ToggleKey) {
  default: //0,1
    SendDlgItemMessage(Handle,group+IDC_ACTIVE,CB_SETCURSEL,Joy[Port].ToggleKey,0);
    break;
  case VK_SCROLL:
    SendDlgItemMessage(Handle,group+IDC_ACTIVE,CB_SETCURSEL,2,0);
    break;
  case VK_NUMLOCK:
    SendDlgItemMessage(Handle,group+IDC_ACTIVE,CB_SETCURSEL,3,0);
    break;
  }
  HWND hwnd;
  for(int n=group+IDC_JOYBUTTON;n<group+IDC_JOYBUTTON+JOY_N_DIR_ID;n++)
  {
    if((hwnd=GetDlgItem(Handle,n))!=NULL)
    {
      EnableWindow(hwnd,NewMode);
      InvalidateRect(hwnd,NULL,FALSE);
      SetWindowWord(hwnd,0,(WORD)((NewMode)?Joy[Port].DirID[n-(group+IDC_JOYBUTTON)]:0));
    }
  }
  if((hwnd=GetDlgItem(Handle,group+IDC_AUTOFIRE))!=NULL)
  {
    EnableWindow(hwnd,NewMode);
    SendMessage(hwnd,CB_SETCURSEL,(WPARAM)(NewMode?Joy[Port].AutoFireSpeed:0),0);
  }
  if((hwnd=GetDlgItem(Handle,group+IDC_ANYBUTTON))!=NULL)
  {
    if(NumJoysticks) 
      EnableWindow(hwnd,NewMode);
    SendMessage(hwnd,CB_SETCURSEL,(WPARAM)(NewMode?Joy[Port].AnyFireOnJoy:0),0);
  }
  if(Port==N_JOY_STE_A_0||Port==N_JOY_STE_B_0)
  {
    for(int n=IDC_JAGFIRES;n<IDC_JAGKEYS+14;n++)
    {
      if((hwnd=GetDlgItem(Handle,n))!=NULL)
      {
        SetWindowWord(hwnd,0,(WORD)(NewMode?Joy[Port].JagDirID[n-IDC_JAGFIRES]:0));
        EnableWindow(hwnd,NewMode);
        InvalidateRect(hwnd,NULL,FALSE);
      }
    }
  }
  FillJoyTypeCombo();
  CheckJoyType();
  if((hwnd=GetDlgItem(Handle,group+IDC_DEADZONE))!=NULL)
  {
    EnableWindow(hwnd,NewMode);
    InvalidateRect(hwnd,NULL,FALSE);
  }
  CreateJoyAnyButtonMasks();
  if(Port==N_JOY_PARALLEL_0||Port==N_JOY_PARALLEL_1)
    OptionBox.UpdateParallel();
}


LRESULT CALLBACK TJoystickConfig::WndProc(HWND const Win,UINT const Mess,WPARAM const wPar,
                                          LPARAM const lPar) {
  LRESULT Ret=DefStemDialogProc(Win,Mess,wPar,lPar);
  if(StemDialog_RetDefVal) 
    return Ret;
  TJoystickConfig *This=(TJoystickConfig*)GetWindowLongPtr(Win,GWLP_USERDATA);
  WORD const wpar_lo=LOWORD(wPar),wpar_hi=HIWORD(wPar);
  switch(Mess) {
  case WM_COMMAND:
    switch(wpar_lo) {
    case IDC_RESCAN:
#if !defined(SSE_NO_JOYSTICK_MM)
      if(HIWORD(wPar)==CBN_SELENDOK)
        InitJoysticks((int)CBGetSelectedItemData((HWND)lPar));
#else
      if(wpar_hi==BN_CLICKED) // pushlike button now, rescan
        InitJoysticks(PCJOY_READ_DI);
#endif
      break;
    case IDC_SETUP:
      if(wpar_hi==CBN_SELENDOK)
      {
        This->JoySetupUpdate(true);
        nJoySetup=(BYTE)SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
        This->JoySetupUpdate(false);
      }
      break;
    case (GROUP_BASE+IDC_ACTIVE):case (2*GROUP_BASE+IDC_ACTIVE):
      if(wpar_hi==CBN_SELENDOK)
      {
        int Idx=(int)SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
        int Port=BasePort+(wpar_lo/GROUP_BASE-1);
        switch(Idx) {
        default: // 0,1
          Joy[Port].ToggleKey=Idx;
          break;
        case 2:
          Joy[Port].ToggleKey=VK_SCROLL;
          break;
        case 3:
          Joy[Port].ToggleKey=VK_NUMLOCK;
          break;
        }
        This->JoyModeChange(Port);
        UPDATE_STATUS_BAR_PART(SB_PART_CAPS);
      }
      break;
    case (GROUP_BASE+IDC_ANYBUTTON):case (2*GROUP_BASE+IDC_ANYBUTTON):
    {
      int Port=BasePort+(wpar_lo/GROUP_BASE-1);
      Joy[Port].AnyFireOnJoy=(int)SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
      break;
    }
    case (GROUP_BASE+IDC_AUTOFIRE):case (2*GROUP_BASE+IDC_AUTOFIRE):
    {
      int Port=BasePort+(wpar_lo/GROUP_BASE-1);
      Joy[Port].AutoFireSpeed=(int)SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
      CreateJoyAnyButtonMasks();
      break;
    }
    case (GROUP_BASE+IDC_JOYBUTTON+ID_UP):
    case (GROUP_BASE+IDC_JOYBUTTON+ID_DOWN):
    case (GROUP_BASE+IDC_JOYBUTTON+ID_LEFT):
    case (GROUP_BASE+IDC_JOYBUTTON+ID_RIGHT):
    case (GROUP_BASE+IDC_JOYBUTTON+ID_FIRE):
    case (GROUP_BASE+IDC_JOYBUTTON+ID_AUTOFIRE):
#if defined(SSE_JOYSTICK_JUMP_BUTTON)
    case (GROUP_BASE+IDC_JOYBUTTON+ID_JUMP):
#endif
    case (2*GROUP_BASE+IDC_JOYBUTTON+ID_UP):
    case (2*GROUP_BASE+IDC_JOYBUTTON+ID_DOWN):
    case (2*GROUP_BASE+IDC_JOYBUTTON+ID_LEFT):
    case (2*GROUP_BASE+IDC_JOYBUTTON+ID_RIGHT):
    case (2*GROUP_BASE+IDC_JOYBUTTON+ID_FIRE):
    case (2*GROUP_BASE+IDC_JOYBUTTON+ID_AUTOFIRE):
#if defined(SSE_JOYSTICK_JUMP_BUTTON)
    case (2*GROUP_BASE+IDC_JOYBUTTON+ID_JUMP):
#endif
    {
      int Port=BasePort+(wpar_lo/GROUP_BASE-1);
      Port&=7;
      WORD a=(wpar_lo-10)%GROUP_BASE;
      Joy[Port].DirID[a]=GetWindowWord((HWND)lPar,0);
      CreateJoyAnyButtonMasks();
      break;
    }//case
    case IDC_JAGPAD:
      if(wpar_hi==BN_CLICKED)
      {
        int NewType=(int)SendMessage((HWND)lPar,BM_GETCHECK,0,0);
        if(NewType!=Joy[BasePort].Type)
        {
          Joy[BasePort].Type=NewType;
          This->JoyModeChange(BasePort);
          This->JoyModeChange(BasePort+1);
        }
      }
      break;
    }//sw
    if(wpar_lo>=IDC_JAGFIRES&&wpar_lo<IDC_JAGKEYS+14)
    { //Jagpad
      int Port=BasePort+(wpar_lo/100-1);
      int a=wpar_lo-IDC_JAGFIRES;
      Joy[Port].JagDirID[a]=GetWindowWord((HWND)lPar,0);
      CreateJoyAnyButtonMasks();
    }
    break;
  case WM_NOTIFY:
  {
    NMHDR *hdr=(NMHDR*)lPar;
    if(hdr->idFrom==IDC_JOYTABS)
    {
      if(hdr->code==TCN_SELCHANGE)
      {
        BYTE NewBase=(BYTE)SendMessage(GetDlgItem(This->Handle,IDC_JOYTABS),TCM_GETCURSEL,0,0)*2;
        if(BasePort!=NewBase)
        {
          BasePort=NewBase;
          This->JoyModeChange(BasePort);
          This->JoyModeChange(BasePort+1);
        }
      }
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
  }
  return DefWindowProc(Win,Mess,wPar,lPar);
}


LRESULT CALLBACK TJoystickConfig::DeadZoneWndProc(HWND const Win,UINT const Mess,
                                                  WPARAM const wPar,LPARAM const lPar) {
  bool DoPaint=false;
  switch(Mess) {
  case WM_CREATE:
  {
    SetTimer(Win,1,60,NULL);
    int *OldDirPos=new int[4];
    ZeroMemory(OldDirPos,sizeof(int)*4);
    SetWindowLongPtr(Win,DWLP_USER,(LONG_PTR)OldDirPos);
    break;
  }
  case WM_PAINT:case WM_ENABLE: case WM_TIMER:
    DoPaint=true;
    break;
  case WM_LBUTTONDOWN:
  case WM_MOUSEMOVE:
    if(wPar & MK_LBUTTON)
    {
      RECT rc;
      GetClientRect(Win,&rc);
      int Port=BasePort+(GetDlgCtrlID(Win)/GROUP_BASE-1);
      double NewDZ=MAX(abs(GET_X_LPARAM(lPar)-rc.right/2),abs(GET_Y_LPARAM(lPar)-rc.bottom/2));
      Joy[Port].DeadZone=int((NewDZ/double(rc.right/2))*100);
      DoPaint=true;
    }
    break;
  case WM_DESTROY:
    KillTimer(Win,1);
    delete[](int*)GetWindowLongPtr(Win,DWLP_USER);
    break;
  }
  if(DoPaint)
  {
    HDC DC;
    RECT rc;
    HBRUSH Bk;
    int Port=BasePort+(GetDlgCtrlID(Win)/GROUP_BASE-1);
    GetClientRect(Win,&rc);
    int hx=rc.right/2,hy=rc.bottom/2;
    int DirPos[4];
    int *OldDirPos=(int*)GetWindowLongPtr(Win,DWLP_USER); // NULL on error

    int n;
    for(n=0;n<4;n++)
    {
      DirPos[n]=0;
      int DirID=Joy[Port].DirID[n];
      BYTE dirIDhi=HIBYTE(DirID);
      if(dirIDhi>=10&&DirID<0xffff)
      {
        BYTE ID=LOBYTE(DirID);
        int JoyNum=(dirIDhi-10)/10;
        ASSERTB((JoyNum&7)==JoyNum);
#ifndef SSE_LEAN_AND_MEAN
        JoyNum&=7;
#endif
        if(ID && JoyExists[JoyNum])
        {
          if(ID<100)
          {  // Axis
            int AxNum=(ID-1)%6;
            if(JoyInfo[JoyNum].AxisExists[AxNum])
            {
              double pos;
              if(dirIDhi&1)
                pos=JoyInfo[JoyNum].AxisLen[AxNum]/2-GetAxisPosition(AxNum,
                  &JoyPos[JoyNum])-JoyInfo[JoyNum].AxisMin[AxNum];
              else
                pos=GetAxisPosition(AxNum,&JoyPos[JoyNum])-JoyInfo[
                  JoyNum].AxisMin[AxNum]-JoyInfo[JoyNum].AxisLen[AxNum]/2;
              if(pos<=JoyInfo[JoyNum].AxisLen[AxNum])
              {
#if 1
                DirPos[n]=(int)((pos/(double)(JoyInfo[JoyNum].AxisLen[AxNum]/2))*(double)((n<2)?hy:hx));
#else
                if(n<2)
                  DirPos[n]=(int)((pos/(double)(JoyInfo[JoyNum].AxisLen[AxNum]
                    /2)) * (double)hy);
                else
                  DirPos[n]=(int)((pos/(double)(JoyInfo[JoyNum].AxisLen[AxNum]
                    /2)) * (double)hx);
#endif
              }
            }
          }
          else
          {  // Other
            if(IsDirIDPressed(DirID,Joy[Port].DeadZone,0,true)) 
              DirPos[n]=(n<2) ? hy : hx;
          }
        }
      }
      else if(IsDirIDPressed(DirID,Joy[Port].DeadZone,0)) 
        DirPos[n]=(n<2) ? hy : hx;
    }
    for(n=0;n<4;n++)
      if((DirPos[n]*100)>=(hx * Joy[Port].DeadZone)) 
        break;
    if(n==4)
      DirPos[0]=DirPos[1]=DirPos[2]=DirPos[3]=0;
    if(Mess==WM_TIMER)
    {
      for(n=0;n<4;n++)
        if(OldDirPos && DirPos[n]!=OldDirPos[n]) 
          break;
      if(n==4) 
        return 0;
      if(OldDirPos)
        memcpy(OldDirPos,DirPos,sizeof(DirPos));
    }
    DC=GetDC(Win);
    Bk=CreateSolidBrush(GetSysColor((IsWindowEnabled(Win)) ? COLOR_WINDOW : COLOR_BTNFACE));
    FillRect(DC,&rc,Bk);
    DeleteObject(Bk);
    if(Joy[Port].ToggleKey)
    {
      int dx=int(hx * Joy[Port].DeadZone)/100;
      int dy=int(hy * Joy[Port].DeadZone)/100;
      HANDLE Old=SelectObject(DC,CreateSolidBrush(MidGUIRGB));
      Rectangle(DC,hx-dx,hy-dy,hx+dx,hy+dy);
      DeleteObject(SelectObject(DC,Old));
      int x3=rc.right/3,y3=rc.bottom/3;
      HANDLE OldPen=SelectObject(DC,CreatePen(PS_SOLID,1,GetSysColor(COLOR_WINDOWTEXT)));
      MoveToEx(DC,hx,hy,0);LineTo(DC,x3,0);
      MoveToEx(DC,hx,hy,0);LineTo(DC,x3*2,0);
      MoveToEx(DC,hx,hy,0);LineTo(DC,x3,rc.bottom-1);
      MoveToEx(DC,hx,hy,0);LineTo(DC,x3*2,rc.bottom-1);
      MoveToEx(DC,hx,hy,0);LineTo(DC,0,y3);
      MoveToEx(DC,hx,hy,0);LineTo(DC,0,y3*2);
      MoveToEx(DC,hx,hy,0);LineTo(DC,rc.right-1,y3);
      MoveToEx(DC,hx,hy,0);LineTo(DC,rc.right-1,y3*2);
      int xpos=hx,ypos=hy;
      ypos-=DirPos[0];
      ypos+=DirPos[1];
      xpos-=DirPos[2];
      xpos+=DirPos[3];
      Old=SelectObject(DC,CreateSolidBrush(GetSysColor(COLOR_WINDOWTEXT)));
      Ellipse(DC,xpos-4,ypos-4,xpos+4,ypos+4);
      DeleteObject(SelectObject(DC,Old));
      DeleteObject(SelectObject(DC,OldPen));
    }
    ReleaseDC(Win,DC);
    ValidateRect(Win,NULL);
    if(Mess==WM_PAINT) 
      return 0;
  }
  return DefWindowProc(Win,Mess,wPar,lPar);
}


LRESULT CALLBACK TJoystickConfig::GroupBoxWndProc(HWND const Win,UINT const Mess,
                                                  WPARAM const wPar,LPARAM const lPar) {
  TJoystickConfig *This=(TJoystickConfig*)GetWindowLongPtr(Win,GWLP_USERDATA);
  return CallWindowProc(This->OldGroupBoxWndProc,Win,Mess,wPar,lPar);
}

#endif//WIN32

#ifdef UNIX

void XOpenJoystick(int j)
{
  XCloseJoystick(j);

  if (JoyInfo[j].On==0) return;

#ifndef UNIX_FAKE_JOY

#ifdef LINUX
  JoyInfo[j].Dev=open(JoyInfo[j].DeviceFile,O_RDONLY | O_NONBLOCK);
  if (JoyInfo[j].Dev==-1) return;

  JoyExists[j]=true;

  int ret;
#ifdef JSIOCGVERSION
  ret=-1;
	ioctl(JoyInfo[j].Dev,JSIOCGVERSION,&ret);
  JoyInfo[j].NoEvent=(ret==-1);
#else
  JoyInfo[j].NoEvent=true;
#endif
  if (JoyInfo[j].NoEvent==0) JoyInfo[j].Range=65535;

  if (JoyInfo[j].NumButtons<0){
    if (JoyInfo[j].NoEvent) JoyInfo[j].Range=10000;
    JoyInitAxisRange(j); // Reset min and max

    if (JoyInfo[j].NoEvent){
      for (int n=0;n<6;n++) JoyInfo[j].AxisExists[n]=(n<2);
      JoyInfo[j].NumButtons=2;
    }else{
#ifdef JSIOCGAXES
      ret=0;
      ioctl(JoyInfo[j].Dev,JSIOCGAXES,&ret);
      for (int n=0;n<6;n++) JoyInfo[j].AxisExists[n]=(n<ret);

      ret=0;
      ioctl(JoyInfo[j].Dev,JSIOCGBUTTONS,&ret);
      JoyInfo[j].NumButtons=ret;
#endif
    }
  }
#else
  JoyInfo[j].Dev=-1;
  return;
#endif

#else
  JoyInfo[j].Dev=-1;
  JoyExists[j]=true;
  JoyInfo[j].NoEvent=0;
  if (JoyInfo[j].NoEvent==0) JoyInfo[j].Range=65535;
  if (JoyInfo[j].NumButtons<0){
    if (JoyInfo[j].NoEvent) JoyInfo[j].Range=10000;
    JoyInitAxisRange(j); // Reset min and max

    if (JoyInfo[j].NoEvent){
      for (int n=0;n<6;n++) JoyInfo[j].AxisExists[n]=(n<2);
      JoyInfo[j].NumButtons=2;
    }else{
      for (int n=0;n<6;n++) JoyInfo[j].AxisExists[n]=true;
      JoyInfo[j].NumButtons=32;
    }
  }
#endif

  // This is in case the joystick was already configured when NoEvent==0
  if (JoyInfo[j].NoEvent){
    for (int n=2;n<6;n++) JoyInfo[j].AxisExists[n]=0;
  }
  JoyPosReset(j); // Make sure changing range hasn't messed anything up
  NumJoysticks++;
}
//---------------------------------------------------------------------------
void XCloseJoystick(int j)
{
  if (JoyExists[j]==0) return;

  if (JoyInfo[j].Dev!=-1) close(JoyInfo[j].Dev);
  JoyInfo[j].Dev=-1;
  JoyExists[j]=0;
  JoyPosReset(j); // Make sure nothing stuck on
  NumJoysticks--;
}

//---------------------------------------------------------------------------
void JoyInitAxisRange(int j)
{
  for (int a=0;a<6;a++){
    JoyInfo[j].AxisMin[a]=0;
    JoyInfo[j].AxisMax[a]=JoyInfo[j].Range;
    JoyInfo[j].AxisMid[a]=JoyInfo[j].AxisMax[a]/2;
    JoyInfo[j].AxisLen[a]=JoyInfo[j].Range;
  }
}
//---------------------------------------------------------------------------
void JoyInitCalibration(int j)
{
  JoyInitAxisRange(j);
  for(int a=0;a<7;a++)
  {
    JoyInfo[j].AxisDZ[a]=25;
    //JoyInfo[j].AxisExists[6]=0; //??
    JoyInfo[j].AxisExists[a]=0;
  }
  JoyInfo[j].NumButtons=-1;
  JoyInfo[j].On=0;
}


//---------------------------------------------------------------------------
void TJoystickConfig::ShowAndUpdatePage()
{
  if (ConfigST==0){
    hxc_dropdown *p_dd=(hxc_dropdown*)hxc::find(pc_group.handle,6000);
    p_dd->changesel(PCJoyEdit);
    p_dd->draw();

    device_ed.set_text(JoyInfo[PCJoyEdit].DeviceFile);
    device_ed.draw();

    hxc_button *p_but=(hxc_button*)hxc::find(pc_group.handle,6002);
    p_but->set_check(JoyInfo[PCJoyEdit].On);

    int n=0;
    for (int a=0;a<6;a++){
      if (JoyInfo[PCJoyEdit].AxisExists[a]){
        n++;
      }else{
        break;
      }
    }
    p_dd=(hxc_dropdown*)hxc::find(pc_group.handle,6003);
    int max_axes=6;
    if (JoyInfo[PCJoyEdit].NoEvent) max_axes=2;
    if (p_dd->sl.NumStrings!=max_axes+1){
      p_dd->sl.DeleteAll();
      for (int i=0;i<=max_axes;i++) p_dd->sl.Add(Str(i));
    }
    p_dd->changesel(n);
    p_dd->draw();
    ShowHideWindow(XD,p_dd->handle,JoyExists[PCJoyEdit]);
    ShowHideWindow(XD,((hxc_button*)(hxc::find(pc_group.handle,6005)))->handle,JoyExists[PCJoyEdit]);

    p_dd=(hxc_dropdown*)hxc::find(pc_group.handle,6004);
    p_dd->changesel(JoyInfo[PCJoyEdit].NumButtons);
    p_dd->draw();
    ShowHideWindow(XD,p_dd->handle,JoyExists[PCJoyEdit]);
    ShowHideWindow(XD,((hxc_button*)(hxc::find(pc_group.handle,6006)))->handle,JoyExists[PCJoyEdit]);

    p_dd=(hxc_dropdown*)hxc::find(pc_group.handle,6007);
    p_dd->select_item_by_data(JoyInfo[PCJoyEdit].Range);
    p_dd->draw();
    ShowHideWindow(XD,p_dd->handle,JoyExists[PCJoyEdit] && JoyInfo[PCJoyEdit].NoEvent);
    ShowHideWindow(XD,((hxc_button*)(hxc::find(pc_group.handle,6008)))->handle,
                            JoyExists[PCJoyEdit] && JoyInfo[PCJoyEdit].NoEvent);

    UpdateJoyPos();

    for (int a=0;a<6;a++){
      hxc_button *pointer=(hxc_button*)(hxc::find(pc_group.handle,6100+a*10));
      hxc_button *base=(hxc_button*)(hxc::find(pc_group.handle,6101+a*10));
      hxc_button *lab1=(hxc_button*)(hxc::find(pc_group.handle,6107+a*10));
      hxc_button *lab2=(hxc_button*)(hxc::find(pc_group.handle,6108+a*10));
      ShowHideWindow(XD,pointer->handle,JoyInfo[PCJoyEdit].AxisExists[a] && JoyExists[PCJoyEdit]);
      ShowHideWindow(XD,base->handle,JoyInfo[PCJoyEdit].AxisExists[a] && JoyExists[PCJoyEdit]);
      ShowHideWindow(XD,lab1->handle,JoyInfo[PCJoyEdit].AxisExists[a] && JoyExists[PCJoyEdit]);
      ShowHideWindow(XD,lab2->handle,JoyInfo[PCJoyEdit].AxisExists[a] && JoyExists[PCJoyEdit]);
    }
    for (int b=0;b<32;b++){
      hxc_button *p_but=(hxc_button*)(hxc::find(pc_group.handle,6300+b));
      ShowHideWindow(XD,p_but->handle,b<JoyInfo[PCJoyEdit].NumButtons && JoyExists[PCJoyEdit]);
    }
    if (JoyExists[PCJoyEdit]==0){
      pc_group.text=T("Device not opened");
    }else{
      pc_group.text="";
    }

    XUnmapWindow(XD,setup_dd.handle);
    XMapWindow(XD,pc_group.handle);
    XUnmapWindow(XD,st_group.handle);
    pc_group.draw();

    hxc::set_timer(Handle,0,50,timerproc,this);
  }else{
    hxc::kill_timer(Handle,0);
    
    Str PortName[MAX_ST_JOYS]={T("Port 0 (mouse)"),T("Port 1"),
                         T("Stick 0"),T("Stick 1"),
                         T("Stick 0"),T("Stick 1"),
                         T("Parallel 0"),T("Parallel 1")};

    hxc_dropdown *p_dd=(hxc_dropdown*)hxc::find(st_group.handle,6202);
    hxc_button *p_but=(hxc_button*)hxc::find(st_group.handle,6203);
    if(BasePort==N_JOY_STE_A_0||BasePort==N_JOY_STE_B_0){
      XMoveResizeWindow(XD,p_dd->handle,p_dd->x,p_dd->y,p_but->x-p_dd->x-10,p_dd->h);
    }else{
      XMoveResizeWindow(XD,p_dd->handle,p_dd->x,p_dd->y,st_group.w,p_dd->h);
    }

    if (Joy[BasePort].Type==JOY_TYPE_JAGPAD){
      PortName[BasePort]=T("Pad");
      PortName[BasePort+1]=T("Pad Keyboard");
      p_but->set_check(true);
    }else{
      p_but->set_check(0);
    }
    ShowHideWindow(XD,p_but->handle,(BasePort==N_JOY_STE_A_0||BasePort==N_JOY_STE_B_0));
    for (int s=0;s<2;s++){
      int j=BasePort+s;
      joy_group[s].set_text(PortName[j]);
      joy_group[s].draw();

      if (Joy[BasePort].Type==JOY_TYPE_JAGPAD){
        ShowHideWindow(XD,jagpad_par[s].handle,true);
        ShowHideWindow(XD,fire_par[s].handle,0);
        ShowHideWindow(XD,dir_par[s].handle,(s==0));
        j=BasePort;
      }else{
        ShowHideWindow(XD,fire_par[s].handle,true);
        ShowHideWindow(XD,dir_par[s].handle,true);
        ShowHideWindow(XD,jagpad_par[s].handle,0);
      }

      enable_but[s].set_check(bool(Joy[j].ToggleKey));
      for (int i=0;i<JOY_N_DIR_ID;i++){
        picker[s][i].DirID=Joy[j].DirID[i];
        picker[s][i].draw();
      }
      autofire_dd[s].sel=Joy[j].AutoFireSpeed;
      autofire_dd[s].draw();
      
      if (s==0){
        for (int i=0;i<3;i++){
          hxc_buttonpicker *bp=(hxc_buttonpicker*)hxc::find(jagpad_par[s].handle,6+i);
          bp->DirID=Joy[j].JagDirID[i];
        }
      }else{
        for (int i=0;i<14;i++){
          hxc_buttonpicker *bp=(hxc_buttonpicker*)hxc::find(jagpad_par[s].handle,9+i);
          bp->DirID=Joy[j].JagDirID[3+i];
        }
      }
    }

    XMapWindow(XD,setup_dd.handle);
    XMapWindow(XD,st_group.handle);
    XUnmapWindow(XD,pc_group.handle);
  }
}
//---------------------------------------------------------------------------
void TJoystickConfig::UpdateJoyPos()
{
  if (Handle==0 || JoyExists[PCJoyEdit]==0) return;

  hxc_button *pointer,*base,*amin,*amax,*dzmin,*dzmax,*dzfill;
  for (int a=0;a<6;a++){
    if (JoyInfo[PCJoyEdit].AxisExists[a]){
      pointer=(hxc_button*)(hxc::find(pc_group.handle,6100+a*10));
      base=(hxc_button*)(hxc::find(pc_group.handle,6101+a*10));
      amin=(hxc_button*)(hxc::find(base->handle,6102+a*10));
      amax=(hxc_button*)(hxc::find(base->handle,6103+a*10));
      dzmin=(hxc_button*)(hxc::find(base->handle,6104+a*10));
      dzmax=(hxc_button*)(hxc::find(base->handle,6105+a*10));
      dzfill=(hxc_button*)(hxc::find(base->handle,6106+a*10));

      int w=base->w-amin->w-2;
      double conv=double(JoyInfo[PCJoyEdit].Range) / w;

      int pos=int(double(GetAxisPosition(a,&(JoyPos[PCJoyEdit])))/conv);
      pos=MIN(pos,w);
      if (base->x+1+amin->w+pos-2!=pointer->x){
        XMoveWindow(XD,pointer->handle,base->x+1+amin->w+pos-2,pointer->y);
      }

      pos=int(double(JoyInfo[PCJoyEdit].AxisMin[a])/conv)+1;
      if (pos!=amin->x) XMoveWindow(XD,amin->handle,pos,amin->y);

      pos=int(double(JoyInfo[PCJoyEdit].AxisMax[a])/conv)+1;
      if (pos!=amax->x) XMoveWindow(XD,amax->handle,pos,amax->y);

      pos=int(double(JoyInfo[PCJoyEdit].AxisMid[a]-
            ((JoyInfo[PCJoyEdit].AxisLen[a]/2)*JoyInfo[PCJoyEdit].AxisDZ[a])/100)/conv)+1;
      if (pos!=dzmin->x) XMoveWindow(XD,dzmin->handle,pos,dzmin->y);

      int pos2=int(double(JoyInfo[PCJoyEdit].AxisMid[a]+
            ((JoyInfo[PCJoyEdit].AxisLen[a]/2)*JoyInfo[PCJoyEdit].AxisDZ[a])/100)/conv)+1;
      if (pos2!=dzmax->x) XMoveWindow(XD,dzmax->handle,pos2,dzmax->y);

      if (pos2-pos-5!=dzfill->w || pos+5!=dzfill->x){
        if (pos2-pos-5>0){
          XMapWindow(XD,dzfill->handle);
          XMoveResizeWindow(XD,dzfill->handle,pos+5,dzfill->y,pos2-pos-5,dzfill->h);
        }else{
          XUnmapWindow(XD,dzfill->handle);
        }
      }
    }
    for (int b=0;b<JoyInfo[PCJoyEdit].NumButtons;b++){
      pointer=(hxc_button*)(hxc::find(pc_group.handle,6300+b));
      int old_type=pointer->type;
      pointer->type&=~BT_BORDER_MASK;
      pointer->type|=int(bool(JoyPos[PCJoyEdit].dwButtons & (1 << b)) ? BT_BORDER_INDENT:0);
      if (pointer->type!=old_type) pointer->draw();
    }
  }
}
//---------------------------------------------------------------------------
bool TJoystickConfig::AttemptOpenJoy(int j)
{
  XOpenJoystick(j);
  return JoyExists[PCJoyEdit];
}

//---------------------------------------------------------------------------
int TJoystickConfig::button_notify_proc(hxc_button*b,int mess,int*ip)
{
  TJoystickConfig *This=(TJoystickConfig*)(b->owner);
	if (mess==BN_CLICKED){
    if((b->id)>=5000 && (b->id)<=5001){
      Joy[(b->id)-5000].ToggleKey=b->checked;
    }else if (b->id==6001){ // Choose
      b->set_check(true);
      fileselect.set_corner_icon(&Ico16,ICO16_JOY);
      Str CurFol=JoyInfo[This->PCJoyEdit].DeviceFile;
      RemoveFileNameFromPath(CurFol,REMOVE_SLASH);
      EasyStr fn=fileselect.choose(XD,CurFol,
                    GetFileNameFromPath(JoyInfo[This->PCJoyEdit].DeviceFile),
                    T("Choose Device"),FSM_OK | FSM_LOADMUSTEXIST,NULL,"");
      if (fileselect.chose_option==FSM_OK){
        This->device_ed.set_text(fn);
        edit_notify_proc(&(This->device_ed),EDN_CHANGE,0);
      }
      b->set_check(0);
    }else if (b->id==6002){ // Open
      XCloseJoystick(This->PCJoyEdit);
      JoyInfo[This->PCJoyEdit].On=b->checked;
      bool OpenFailed=0;
      if (b->checked){
        if (This->AttemptOpenJoy(This->PCJoyEdit)==0){
          JoyInfo[This->PCJoyEdit].On=0;
          b->set_check(0);
          OpenFailed=true;
        }
      }
      This->ShowAndUpdatePage();
      if (OpenFailed) This->pc_group.set_text(T("ERROR: Can't open joystick device."));
    }else if (b->id==6203){
      Joy[BasePort].Type=int((b->checked) ? JOY_TYPE_JAGPAD:JOY_TYPE_JOY);
      This->ShowAndUpdatePage();
    }
  }else if (mess==BN_MOTION){
    XEvent *Ev=(XEvent*)ip;
    int mv=Ev->xmotion.x-3;
    int a=(b->id-6100)/10;

    hxc_button *base=(hxc_button*)(hxc::find(This->pc_group.handle,6101+a*10));
    hxc_button *amin=(hxc_button*)(hxc::find(base->handle,6102+a*10));
    int w=base->w - amin->w - 2;
    UINT r=JoyInfo[This->PCJoyEdit].Range;
    UINT min_l=r/8;
    double conv=double(r) / w;
    int val=int((b->x+mv)*conv);
    switch (b->id % 10){
      case 2:
      {
        int max_val=JoyInfo[This->PCJoyEdit].AxisMax[a]-min_l;
        int min_val=0;
        if (val>max_val) val=max_val;
        if (val<min_val) val=min_val;
        JoyInfo[This->PCJoyEdit].AxisMin[a]=val;
        break;
      }
      case 3:
      {
        int max_val=JoyInfo[This->PCJoyEdit].AxisMin[a]+min_l;
        int min_val=r;
        if (val>max_val) val=max_val;
        if (val<min_val) val=min_val;
        JoyInfo[This->PCJoyEdit].AxisMax[a]=val;
        break;
      }
      case 4:
      case 5:
      {
        int midpos=1+int(double(JoyInfo[This->PCJoyEdit].AxisMid[a])/conv);
        int pos=abs(midpos-(b->x+mv));
        pos*=100;
        pos/=w/2;
        JoyInfo[This->PCJoyEdit].AxisDZ[a]=MAX(MIN(pos,90),10);
        break;
      }
    }
    JoyInfo[This->PCJoyEdit].AxisLen[a]=(JoyInfo[This->PCJoyEdit].AxisMax[a]-JoyInfo[This->PCJoyEdit].AxisMin[a]);
    JoyInfo[This->PCJoyEdit].AxisMid[a]=JoyInfo[This->PCJoyEdit].AxisMin[a]+JoyInfo[This->PCJoyEdit].AxisLen[a]/2;
    This->UpdateJoyPos();
  }
  return 0;
}

int TJoystickConfig::dd_notify_proc(hxc_dropdown *dd,int mess,INT_PTR i)
{
  TJoystickConfig *This=(TJoystickConfig*)(dd->owner);
  if (mess==DDN_SELCHANGE){
    if (dd->id<2){
      Joy[This->BasePort+dd->id].AutoFireSpeed=dd->sel;
      CreateJoyAnyButtonMasks();
    }else if (dd->id==6000){
      This->PCJoyEdit=dd->sel;
      This->ShowAndUpdatePage();
    }else if (dd->id==6003){
      for (int a=0;a<6;a++) JoyInfo[This->PCJoyEdit].AxisExists[a]=(a < dd->sel);
      This->ShowAndUpdatePage();
    }else if (dd->id==6004){
      JoyInfo[This->PCJoyEdit].NumButtons=dd->sel;
      This->ShowAndUpdatePage();
    }else if (dd->id==6007){
      JoyInfo[This->PCJoyEdit].Range=dd->sl[dd->sel].Data[0];
      JoyInitAxisRange(This->PCJoyEdit);
      JoyGetPoses();
      This->ShowAndUpdatePage();
    }else if (dd->id==6200){
      This->ConfigST=bool(dd->sel);
      This->ShowAndUpdatePage();
    }else if (dd->id==6201){ // ST Config
      for (int n=N_JOY_PORT_0;n<MAX_ST_JOYS;n++) JoySetup[nJoySetup][n]=Joy[n];
      nJoySetup=dd->sel;
      for (int n=N_JOY_PORT_0;n<MAX_ST_JOYS;n++) Joy[n]=JoySetup[nJoySetup][n];
      This->ShowAndUpdatePage();
    }else if (dd->id==6202){ // Change BasePort
      BasePort=dd->sel*2;
      This->ShowAndUpdatePage();
    }
  }
  return 0;
}

int TJoystickConfig::picker_notify_proc(hxc_buttonpicker *bp,int mess,INT_PTR i)
{
  TJoystickConfig *This=(TJoystickConfig*)(bp->owner);
  if (mess==BPN_CHANGE){
    int jn=(bp->id) / 64 + This->BasePort;
    int bn=(bp->id) & 63;
    if (bn<JOY_N_DIR_ID){
      Joy[jn].DirID[bn]=i;
    }else{ // JagPad
      Joy[jn].JagDirID[bn-6]=i;
    }
    CreateJoyAnyButtonMasks();
  }
  return 0;
}

int TJoystickConfig::edit_notify_proc(hxc_edit *ed,int mess,INT_PTR i)
{
  TJoystickConfig *This=(TJoystickConfig*)(ed->owner);
  if (mess==EDN_CHANGE){
    XCloseJoystick(This->PCJoyEdit);
    JoyInfo[This->PCJoyEdit].DeviceFile=ed->text;
    JoyInfo[This->PCJoyEdit].On=0;
    JoyInitCalibration(This->PCJoyEdit);
    This->ShowAndUpdatePage();
  }
  return 0;
}

int TJoystickConfig::scrollbar_notify_proc(hxc_scrollbar *SB,int Mess,INT_PTR I)
{
	if (Mess==SBN_SCROLL){
    SB->pos=I;
		mouse_speed=SB->pos+1;
		SB->draw();
	}else if (Mess==SBN_SCROLLBYONE){
		mouse_speed=MAX(MIN(mouse_speed+I,(INT_PTR)19),(INT_PTR)1);
		SB->pos=mouse_speed-1;
		SB->draw();
	}
	return 0;
}
//---------------------------------------------------------------------------
int TJoystickConfig::WinProc(TJoystickConfig *This,Window Win,XEvent *Ev)
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
int TJoystickConfig::timerproc(void *t,Window,INT_PTR)
{
  TJoystickConfig *This=(TJoystickConfig*)t;
  This->UpdateJoyPos();
  return HXC_TIMER_REPEAT;
}

#endif//UNIX

#undef LOGSECTION
#undef CbUnits
#undef CharHeight
#undef ID_UP
#undef ID_DOWN
#undef ID_LEFT
#undef ID_RIGHT
#undef ID_FIRE
#undef ID_AUTOFIRE
#undef ID_JUMP
#undef IDC_RESCAN
#undef IDC_JAGPAD
#undef IDC_JAGFIRES
#undef IDC_JAGKEYS
#undef IDS_JAGFIRES
#undef IDS_JAGKEYS
#undef IDC_JOYTABS
#undef IDS_ACTIVE
#undef IDC_ACTIVE
#undef IDC_JOYBUTTON
#undef IDC_DEADZONE
#undef IDS_JOYBUTTON
#undef IDC_AUTOFIRE
#undef IDS_AUTOFIRE
#undef IDC_ANYBUTTON
#undef IDS_ANYBUTTON
#undef IDC_SETUP
#undef TOGGLE_NEVER
#undef TOGGLE_ALWAYS
#undef GROUP_BASE


