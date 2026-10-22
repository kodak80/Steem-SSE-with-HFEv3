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
FILE: stjoy.h
DESCRIPTION: This file contains the declarations for the Steem joysticks 
dialog and for the PC joysticks.
struct TJoystickInfo, TJoystick, TOldJoystickPosition
class TJoystickConfig
---------------------------------------------------------------------------*/
#pragma once
#ifndef STJOY_DECLA_H
#define STJOY_DECLA_H


#include "stemdialogs.h"
#include "conditions.h"
#include "SSE.h"
#include "parameters.h"
#ifdef UNIX
#include <x/x_controls.h>
#endif

#pragma pack(push, 8)

#define N_JOY_PORT_0 0
#define N_JOY_PORT_1 1
#define N_JOY_STE_A_0 2
#define N_JOY_STE_A_1 3
#define N_JOY_STE_B_0 4
#define N_JOY_STE_B_1 5
#define N_JOY_PARALLEL_0 6
#define N_JOY_PARALLEL_1 7

#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2
#define AXIS_R 3
#define AXIS_U 4
#define AXIS_V 5
#define AXIS_POV 6

#define PCJOY_READ_DONT 2

#define JOY_MODE_OFF 0
#define JOY_MODE_KEYBOARD 1
#define JOY_MODE_JOY1 2
#define JOY_MODE_JOY2 3

#define JOY_TYPE_JOY 0
#define JOY_TYPE_JAGPAD 1

#define JAGPAD_BUT_FIRE_A 0
#define JAGPAD_BUT_FIRE_B 1
#define JAGPAD_BUT_FIRE_C 2
#define JAGPAD_BUT_KEY_OPTION 3
#define JAGPAD_BUT_KEY_PAUSE 4
#define JAGPAD_BUT_KEY_0 5
#define JAGPAD_BUT_KEY_1 6
#define JAGPAD_BUT_KEY_2 7
#define JAGPAD_BUT_KEY_3 8
#define JAGPAD_BUT_KEY_4 9
#define JAGPAD_BUT_KEY_5 10
#define JAGPAD_BUT_KEY_6 11
#define JAGPAD_BUT_KEY_7 12
#define JAGPAD_BUT_KEY_8 13
#define JAGPAD_BUT_KEY_9 14
#define JAGPAD_BUT_KEY_HASH 15
#define JAGPAD_BUT_KEY_AST 16

#define JAGPAD_BUT_FIRE_A_BIT (1 << JAGPAD_BUT_FIRE_A)
#define JAGPAD_BUT_FIRE_B_BIT (1 << JAGPAD_BUT_FIRE_B)
#define JAGPAD_BUT_FIRE_C_BIT (1 << JAGPAD_BUT_FIRE_C)
#define JAGPAD_BUT_KEY_OPTION_BIT (1 << JAGPAD_BUT_KEY_OPTION)
#define JAGPAD_BUT_KEY_PAUSE_BIT (1 << JAGPAD_BUT_KEY_PAUSE)
#define JAGPAD_BUT_KEY_0_BIT (1 << JAGPAD_BUT_KEY_0)
#define JAGPAD_BUT_KEY_1_BIT (1 << JAGPAD_BUT_KEY_1)
#define JAGPAD_BUT_KEY_2_BIT (1 << JAGPAD_BUT_KEY_2)
#define JAGPAD_BUT_KEY_3_BIT (1 << JAGPAD_BUT_KEY_3)
#define JAGPAD_BUT_KEY_4_BIT (1 << JAGPAD_BUT_KEY_4)
#define JAGPAD_BUT_KEY_5_BIT (1 << JAGPAD_BUT_KEY_5)
#define JAGPAD_BUT_KEY_6_BIT (1 << JAGPAD_BUT_KEY_6)
#define JAGPAD_BUT_KEY_7_BIT (1 << JAGPAD_BUT_KEY_7)
#define JAGPAD_BUT_KEY_8_BIT (1 << JAGPAD_BUT_KEY_8)
#define JAGPAD_BUT_KEY_9_BIT (1 << JAGPAD_BUT_KEY_9)
#define JAGPAD_BUT_KEY_HASH_BIT (1 << JAGPAD_BUT_KEY_HASH)
#define JAGPAD_BUT_KEY_AST_BIT (1 << JAGPAD_BUT_KEY_AST)

#define JAGPAD_DIR_U_BIT BIT_17
#define JAGPAD_DIR_D_BIT BIT_18
#define JAGPAD_DIR_L_BIT BIT_19
#define JAGPAD_DIR_R_BIT BIT_20

#if defined(SSE_JOYSTICK_JUMP_BUTTON)
#define JOY_N_DIR_ID 7 // Up Down Left Right Fire AutoFire Jump
#else                  //  0    1    2     3    4        5    6
#define JOY_N_DIR_ID 6 // Up Down Left Right Fire AutoFire
#endif

#define JAGPAD_N_DIR 17

#define POV_CONV(POV) (((POV)<0xffff) ? ((((POV)+2250)/4500) % 8):0xffff)


void InitJoysticks(BYTE Method=0),FreeJoysticks();
void JoyPosReset(int joy);
WORD JoyReadSTEAddress(MEM_ADDRESS addr,bool *pIllegal);
extern "C" void JoyGetPoses(); // C for 6301
extern "C" void joy_read_buttons(int njoys=MAX_ST_JOYS);
extern "C" BYTE joy_get_pos(int joy);
bool IsJoyActive(int joy);
DWORD GetJagPadDown(int n,DWORD Mask);
bool joy_is_key_used(BYTE Key);
void SetJoyToDefaults(int joy,int Defs);
void joy_check_controllers();
DWORD GetAxisPosition(int AxNum,JOYINFOEX *ji);


extern bool JoyExists[MAX_PC_JOYS];
#if defined(SSE_GUI_STATUS_BAR)
// for status bar crossed joystick icons (2 would be enough)
extern bool bSTjoyNoInput[MAX_ST_JOYS];
#endif
extern WORD paddles_ReadMask;
extern "C" BYTE stick[MAX_ST_JOYS]; // used by 6301
extern "C" BYTE NumJoysticks; // USB controllers
extern const char AxisToName[7];
extern JOYINFOEX JoyPos[MAX_PC_JOYS];
extern bool DisablePCJoysticks;
extern BYTE JoyReadMethod,nJoySetup;


#ifdef WIN32
#ifndef NO_DIRECTINPUT
extern IDirectInput *DIObj;
extern IDirectInputDevice *DIJoy[MAX_PC_JOYS];
extern IDirectInputDevice2 *DIJoy2[MAX_PC_JOYS];
extern DIJOYSTATE DIJoyPos[MAX_PC_JOYS];
extern int DIPOVNum,DIAxisNeg[MAX_PC_JOYS][6],DI_RnMap[MAX_PC_JOYS][3],
DIDisconnected[MAX_PC_JOYS];
void DIInitJoysticks();
void DIFreeJoysticks();
void DIJoyGetPos(int j);
#endif
#define PCJOY_READ_WINMM 0
#define PCJOY_READ_DI 1
#endif//WIN32


#ifdef UNIX
void XOpenJoystick(int),XCloseJoystick(int);
void JoyInitAxisRange(int),JoyInitCalibration(int);
#define PCJOY_READ_KERNELDRIVER 0
#endif


struct TJoystickInfo {
  UINT AxisMin[6],AxisMax[6]; // On X user sets these
  UINT AxisMid[6],AxisLen[6];  
  int NumButtons;  
#ifdef WIN32
  UINT ExFlags;
  int WaitRead,WaitReadTime;
  bool NeedsEx;
#endif
  bool AxisExists[7];  
#ifdef UNIX
  bool On,NoEvent;
  Str DeviceFile;
  int Dev;
  UINT AxisDZ[6+1],Range;
#endif
};

extern TJoystickInfo JoyInfo[MAX_PC_JOYS];

class TJoystickConfig : public TStemDialog {
private:
#ifdef WIN32
  static LRESULT CALLBACK WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar);
  static LRESULT CALLBACK DeadZoneWndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar);
  static LRESULT CALLBACK GroupBoxWndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar);
  void ManageWindowClasses(bool);

  HWND JagBut,Group[2];
  WNDPROC OldGroupBoxWndProc;
#endif
#ifdef UNIX
  static int WinProc(TJoystickConfig*,Window,XEvent*);
  static int button_notify_proc(hxc_button*,int,int*);
  static int picker_notify_proc(hxc_buttonpicker*,int,INT_PTR);
  static int dd_notify_proc(hxc_dropdown*,int,INT_PTR);
  static int scrollbar_notify_proc(hxc_scrollbar*,int,INT_PTR);
  static int edit_notify_proc(hxc_edit*,int,INT_PTR);
  static int timerproc(void*,Window,INT_PTR);

  void ShowAndUpdatePage();
  bool AttemptOpenJoy(int);

  bool ConfigST;
  int PCJoyEdit;

	hxc_button st_group,pc_group,config_group;
  hxc_dropdown config_dd,setup_dd;
	hxc_button dir_par[2],fire_par[2],jagpad_par[2];

  hxc_edit device_ed;

	hxc_button joy_group[2],fire_but_label[2],autofire_but_label[2][2];
	hxc_dropdown autofire_dd[2];
	//hxc_scrollbar MouseSpeedSB;
//	hxc_button MouseSpeedLabel[4];
#endif
public:
  enum EJoystickConfig {ACTIVE_NEVER=0,ACTIVE_ALWAYS=1};
  TJoystickConfig();
  ~TJoystickConfig() {
    Hide();
  };
  void Show(),Hide();
  void ToggleVisible() {
    IsVisible() ? Hide() : Show();
  }
  void LoadData(bool FirstLoad,TConfigStoreFile *pCSF,bool *SecDisabled=NULL);
  void SaveData(bool FinalSave,TConfigStoreFile *pCSF);
  static void CreateJoyAnyButtonMasks();
  void JoySetupUpdate(bool memorise);
  static BYTE BasePort;
#ifdef WIN32
  bool HasHandledMessage(MSG *mess);
  void FillJoyTypeCombo(),CheckJoyType();
  void JoyModeChange(int Port);
#endif
#ifdef UNIX
  void UpdateJoyPos();
  hxc_buttonpicker picker[2][JOY_N_DIR_ID];
  hxc_button enable_but[2];
  hxc_button centre_icon[2];
#endif
};

extern TJoystickConfig JoyConfig;


struct TJoystick {
  TJoystick();
  ~TJoystick(){};
  int ToggleKey; //,KeyAutoFire;
  int DirID[JOY_N_DIR_ID];
  int AnyFireOnJoy,AutoFireSpeed;
  int DeadZone;
  int JagDirID[17];
  int Type;
};


extern TJoystick Joy[MAX_ST_JOYS];
extern TJoystick JoySetup[JOYSTICK_SETUPS][MAX_ST_JOYS];

// Bitmasks in which all set bits represent a button that can be pressed to
// cause fire in Any Button On... mode
extern DWORD JoyAnyButtonMask[MAX_PC_JOYS];

#pragma pack(pop)

#endif//#ifndef STJOY_DECLA_H
