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
FILE: dir_id.cpp
DESCRIPTION: This file contains the code for the GUI and the implementation
of Steem's DirID system. A DirID is an integer representing a PC input that
can be mapped to perform a function.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <gui.h>
#include <dir_id.h>
#include <stjoy.h>
#include <key_table.h>
#include <shortcutbox.h>
#include <emulator.h>
#include <stdarg.h>
#include <translate.h>

#if defined(SSE_420R5) // ref
char *KeyboardButtonName[256];
#else
char *KeyboardButtonName[256]={NULL}; // array of 256 pointers, first must be null for correct init
#endif

struct TOldJoystickPosition {
  UINT AxisPos[6];
  UINT Buttons;
  DWORD POV;
  bool Valid;
};

TOldJoystickPosition OldJoyPos;
TOldJoystickPosition JoyOldPos[MAX_PC_JOYS];

#if defined(SSE_GUI_STATUS_BAR)
bool bPCInputExists;
#endif


bool IsDirIDPressed(int ID,int DeadZonePercent,bool CheckDisable,bool DiagonalPOV) {
#if defined(SSE_GUI_STATUS_BAR)
  bPCInputExists=true; // used by joy_check_controllers()
#endif
  if(BLANK_DIRID(ID)) 
    return false;
  BYTE hibyte=HIBYTE(ID),lobyte=LOBYTE(ID);
  switch(hibyte) {
  case 0: // Key: instant reading of the keyboard
#ifdef WIN32
    switch(lobyte) {
    case VK_LSHIFT:case VK_RSHIFT:
    case VK_LCONTROL:case VK_RCONTROL:
    case VK_LMENU:case VK_RMENU:
    {
      TModifierState mss=GetLRModifierStates();
      switch(lobyte) {
      case VK_LSHIFT:   if(mss.LShift) return true; break;
      case VK_RSHIFT:   if(mss.RShift) return true; break;
      case VK_LCONTROL: if(mss.LCtrl) return true; break;
      case VK_RCONTROL: if(mss.RCtrl) return true; break;
      case VK_LMENU:    if(mss.LAlt) return true; break;
      case VK_RMENU:    if(mss.RAlt) return true; break;
      }//sw
      break;
    }
    }
#endif
    return ((GetAsyncKeyState(lobyte)&MSB_W)!=0);
  case 1: // Key with extend
  case 3: case 4: case 5: case 6: case 7: case 8: case 9:
    break;
  case 2: // Mouse
    if(lobyte==0)
      return (GetKeyState(VK_MBUTTON)<0);
    else
      return ((MouseWheelMove>0 && lobyte==1) || (MouseWheelMove<0 && lobyte==2));
  default: 
    if(hibyte>=10)
    { // Joystick: based on last polling done by JoyGetPoses()
      int DirID=lobyte;
      if(hibyte&1)
        DirID=-DirID;
      if(DirID)
      {
        int JoyNum=(hibyte-10)/10;
        if(!JoyExists[JoyNum]) // controller not connected
        {
#if defined(SSE_GUI_STATUS_BAR)
// we do it that way because the function has already a default argument TODO
          bPCInputExists=false;
#endif
          break;
        }
        if(DirID>=200)
        {    //POV
          DirID-=200;
          if(DirID>=8) 
            break;
          if(CheckDisable && CutDisablePOV[JoyNum][DirID]) 
            break;
          if(!JoyInfo[JoyNum].AxisExists[AXIS_POV])
            break;
          int pos=POV_CONV(JoyPos[JoyNum].dwPOV);
          if(pos==0xffff) 
            break;
          if(pos<0) 
            pos=7;
          pos%=8;
          if(pos==DirID) 
            return true;
          if(!DiagonalPOV)
            break;
          int prev=DirID-1,next=(DirID+1)%8;
          if(prev<0) 
            prev=7;
          prev%=8;
          return (pos==next||pos==prev);
        }
        else if(DirID>=100)
        { // Button
          DirID-=100;
          if(DirID>=JoyInfo[JoyNum].NumButtons) 
            break;
          if(CheckDisable)
            return ((JoyPos[JoyNum].dwButtons & CutButtonMask[JoyNum])>>DirID)&1;
          else
            return (JoyPos[JoyNum].dwButtons>>DirID)&1;
        }
        else
        {           // Axis
          if(CheckDisable && CutDisableAxis[JoyNum][DirID+10]) 
            break;
          int AxNum=abs_quicki(DirID)-1;
          if(!JoyInfo[JoyNum].AxisExists[AxNum])
            break;
#ifdef UNIX
          DeadZonePercent=10;
#endif
          int DeadSize=((JoyInfo[JoyNum].AxisLen[AxNum]/2) * DeadZonePercent)/100;
          DWORD pos=GetAxisPosition(AxNum,&JoyPos[JoyNum]);
          if(DirID<0)
            return (pos<JoyInfo[JoyNum].AxisMid[AxNum]-DeadSize);
          else
            return (pos>JoyInfo[JoyNum].AxisMid[AxNum]+DeadSize);
        }
      }
    }
    break;
  }//sw
  return false;
}


#ifdef UNIX

void set_KeyboardButtonName(int bum,...) { 
  // number, string, number, string, ..., -1
  va_list vl;
  int arg1=bum;
  va_start(vl,bum);
  char* arg2;
  while(arg1!=-1)
  {
    arg2=va_arg(vl,char*);
    KeyboardButtonName[XKeysymToKeycode(XD,(KeySym)(arg1))]=arg2;
    arg1=va_arg(vl,int);
  }
  va_end(vl);
}

#endif


void init_DirID_to_text() {
  for(int n=0;n<256;n++) 
    KeyboardButtonName[n]="";
  KeyboardButtonName[VK_SHIFT]="Shift";
  KeyboardButtonName[VK_CONTROL]="Ctrl";
  KeyboardButtonName[VK_MENU]="Alt";
#ifdef WIN32
  KeyboardButtonName[VK_F1]="F1";KeyboardButtonName[VK_F2]="F2";
  KeyboardButtonName[VK_F3]="F3";KeyboardButtonName[VK_F4]="F4";
  KeyboardButtonName[VK_F5]="F5";KeyboardButtonName[VK_F6]="F6";
  KeyboardButtonName[VK_F7]="F7";KeyboardButtonName[VK_F8]="F8";
  KeyboardButtonName[VK_F9]="F9";KeyboardButtonName[VK_F10]="F10";
  KeyboardButtonName[VK_F11]="F11";KeyboardButtonName[VK_F12]="F12";
  KeyboardButtonName[VK_F13]="F13";KeyboardButtonName[VK_F14]="F14";
  KeyboardButtonName[VK_F15]="F15";KeyboardButtonName[VK_F16]="F16";
  KeyboardButtonName[VK_F17]="F17";KeyboardButtonName[VK_F18]="F18";
  KeyboardButtonName[VK_F19]="F19";KeyboardButtonName[VK_F20]="F20";
  KeyboardButtonName[VK_F21]="F21";KeyboardButtonName[VK_F22]="F22";
  KeyboardButtonName[VK_F23]="F23";KeyboardButtonName[VK_F24]="F24";
  KeyboardButtonName[VK_LSHIFT]="L-Shift";KeyboardButtonName[VK_RSHIFT]="R-Shift";
  KeyboardButtonName[VK_LCONTROL]="L-Ctrl";KeyboardButtonName[VK_RCONTROL]="R-Ctrl";
  KeyboardButtonName[VK_LMENU]="L-Alt";KeyboardButtonName[VK_RMENU]="R-Alt";
  KeyboardButtonName[VK_BACK]="Bksp";KeyboardButtonName[VK_CLEAR]="Clear";
  KeyboardButtonName[VK_ESCAPE]="Esc";KeyboardButtonName[VK_SPACE]="Space";
  KeyboardButtonName[VK_RETURN]="Ret";KeyboardButtonName[VK_TAB]="Tab";
  KeyboardButtonName[VK_CAPITAL]="Caps";
  KeyboardButtonName[VK_PRIOR]="PgUp";KeyboardButtonName[VK_NEXT]="PgDn";
  KeyboardButtonName[VK_END]="End";KeyboardButtonName[VK_HOME]="Home";
  KeyboardButtonName[VK_LEFT]="Left";KeyboardButtonName[VK_UP]="Up";
  KeyboardButtonName[VK_RIGHT]="Right";KeyboardButtonName[VK_DOWN]="Down";
  KeyboardButtonName[VK_INSERT]="Ins";KeyboardButtonName[VK_DELETE]="Del";
  KeyboardButtonName[VK_NUMPAD0]="Pad 0";KeyboardButtonName[VK_NUMPAD1]="Pad 1";
  KeyboardButtonName[VK_NUMPAD2]="Pad 2";KeyboardButtonName[VK_NUMPAD3]="Pad 3";
  KeyboardButtonName[VK_NUMPAD4]="Pad 4";KeyboardButtonName[VK_NUMPAD5]="Pad 5";
  KeyboardButtonName[VK_NUMPAD6]="Pad 6";KeyboardButtonName[VK_NUMPAD7]="Pad 7";
  KeyboardButtonName[VK_NUMPAD8]="Pad 8";KeyboardButtonName[VK_NUMPAD9]="Pad 9";
  KeyboardButtonName[VK_MULTIPLY]="Pad *";KeyboardButtonName[VK_ADD]="Pad +";
  KeyboardButtonName[VK_SUBTRACT]="Pad -";KeyboardButtonName[VK_DECIMAL]="Pad .";
  KeyboardButtonName[VK_DIVIDE]="Pad /";
#endif
#ifdef UNIX
  set_KeyboardButtonName(XK_F1,"F1",XK_F2,"F2",XK_F3,"F3",
    XK_F4,"F4",XK_F5,"F5",XK_F6,"F6",XK_F7,"F7",XK_F8,"F8",
    XK_F9,"F9",XK_F10,"F10",XK_F11,"F11",XK_F12,"F12",XK_F13,"F13",
    XK_F14,"F14",XK_F15,"F15",XK_F16,"F16",XK_F17,"F17",XK_F18,"F18",
    XK_F19,"F19",XK_F20,"F20",XK_F21,"F21",XK_F22,"F22",XK_F23,"F23",
    XK_F24,"F24",XK_Home,"Home",XK_Left,"Left",
    XK_Right,"Right",XK_Up,"Up",XK_Down,"Down",
    XK_Page_Up,"PgUp",XK_Page_Down,"PgDn",
    XK_End,"End",XK_Begin,"Begin",XK_Print,"Print",
    XK_Insert,"Ins",XK_Undo,"Undo",
    XK_Redo,"Redo",XK_Menu,"Menu",XK_Find,"Find",
    XK_Cancel,"Cancel",XK_Help,"Help",XK_KP_Space,"Pad Spc",
    XK_KP_Tab,"Pad Tab",XK_KP_Enter,"Enter",XK_KP_F1,"Pad F1",
    XK_KP_F2,"Pad F2",XK_KP_F3,"Pad F3",XK_KP_F4,"Pad F4",
    XK_KP_Home,"Pad Home",XK_KP_Left,"Pad Left",XK_KP_Up,"Pad Up",
    XK_KP_Right,"Pad Right",XK_KP_Down,"Pad Down",XK_KP_Page_Up,"Pad PgUp",
    XK_KP_Page_Down,"Pad PgDn",XK_KP_End,"Pad End",XK_KP_Begin,"Pad Bgn",
    XK_KP_Insert,"Pad Ins",XK_KP_Delete,"Pad Del",XK_KP_Equal,"Pad =",
    XK_KP_Multiply,"Pad *",XK_KP_Add,"Pad +",XK_KP_Separator,"Pad ,",
    XK_KP_Subtract,"Pad -",XK_KP_Decimal,"Pad .",XK_KP_Divide,"Pad /",
    XK_KP_0,"Pad 0",XK_KP_1,"Pad 1",XK_KP_2,"Pad 2",XK_KP_3,"Pad 3",
    XK_KP_4,"Pad 4",XK_KP_5,"Pad 5",XK_KP_6,"Pad 6",XK_KP_7,"Pad 7",
    XK_KP_8,"Pad 8",XK_KP_9,"Pad 9",XK_BackSpace,"Bksp",
    XK_Tab,"Tab",XK_Linefeed,"LF",XK_Clear,"Clear",
    XK_Return,"Return",XK_Sys_Req,"SysReq",XK_Escape,"Esc",
    XK_Delete,"Del",XK_Shift_L,"L-Shift",XK_Shift_R,"R-Shift",
    XK_Control_L,"L-Ctrl",XK_Control_R,"R-Ctrl",
    XK_Caps_Lock,"Caps",XK_Alt_L,"L-Alt",XK_Alt_R,"R-Alt",
    XK_space,"Space",XK_Scroll_Lock,"Scroll",XK_Num_Lock,"Num Lock",
    -1);
#endif
}


EasyStr DirID_to_text(int ID,bool st_key) {
#if !defined(SSE_420R5)
  if(KeyboardButtonName[0]==NULL)
    init_DirID_to_text();
#endif
  if(BLANK_DIRID(ID)) 
    return "";
  EasyStr t;
  BYTE hibyte=HIBYTE(ID),lobyte=LOBYTE(ID);
  switch(hibyte) {
  case 0:
  {
    char *KeyName=KeyboardButtonName[lobyte];
    if(KeyName[0]=='\0')
    {
#ifdef WIN32
      WORD Key;
      BYTE Keys[256];
      ZeroMemory(Keys,256);
      if(ToAscii(lobyte,0,Keys,&Key,0)==1)
      {
        t=".";
        t[0]=(char)toupper(Key);
      }
      else
        t=EasyStr("#")+(lobyte);
#endif
#ifdef UNIX
      WORD key=XKeycodeToKeysym(XD,lobyte,0);
      if((key&0xff00)==0)
      { //ASCII
        t=".";t[0]=(char)toupper(key);
      }
      else
        t=EasyStr("#")+(lobyte);
//    KeyboardButtonName[XKeysymToKeycode(XD,(KeySym)(*p))]=
//      (char*)(*(p+1));
#endif
//      KeyName=t.Text;
    }
    else
      t=T(KeyName);
    if(st_key)
    {  // ST Keys Only
#ifdef UNIX
      int VK_PRIOR=XKeysymToKeycode(XD,XK_Page_Up),
        VK_NEXT=XKeysymToKeycode(XD,XK_Page_Down),
        VK_F11=XKeysymToKeycode(XD,XK_F11),
        VK_F12=XKeysymToKeycode(XD,XK_F12);
#endif
#if !defined(UNIX) && defined(SSE_420R5)
      switch(lobyte) {
      case VK_PRIOR:
        t=T("Help");
        break;
      case VK_NEXT:
        t=T("Undo");
        break;
      case VK_F11:
        t=T("Pad (");
        break;
      case VK_F12:
        t=T("Pad )");
        break;
      }//sw
#else
      if(lobyte==VK_PRIOR)   
        t=T("Help");
      if(lobyte==VK_NEXT) 
        t=T("Undo");
      if(lobyte==VK_F11)
        t=T("Pad (");
      if(lobyte==VK_F12)
        t=T("Pad )");
#endif
    }
    break;
  }
  case 1:
#ifdef WIN32
    if(lobyte==VK_RETURN)
      t=T("Pad Ret");
#endif
    break;
  case 2:
    if(lobyte==0)
      t="MMB";
    else
      t=EasyStr("Wheel ")+(LPSTR)((lobyte==1) ? "Up" : "Down");
    break;
  case 3: case 4: case 5: case 6: case 7: case 8: case 9:
    break;
  default:
  {
    int DirID=(int)((hibyte&1)==0 ? lobyte :-lobyte);
    int JoyNum=(hibyte-10)/10;
    if(DirID)
    {
      if(DirID>=200)
        t=EasyStr("J  ")+T("Hat")+" "+((DirID-200)*45);
      else if(DirID>=100)
        t=EasyStr("J  ")+T("But")+" "+(DirID-99);
      else if(DirID<0)
        t="J    -";
      else if(DirID<7)
        t="J    +";
      else
        t="J     ";
      t[1]=char('1'+JoyNum);
      if(abs_quick(DirID)<7) 
        t[3]=AxisToName[abs_quick(DirID)-1];
    }
    break;
  }
  }//sw
  return t;
}


#ifdef WIN32

void RegisterButtonPicker() {
  WNDCLASS wc;
  wc.style=CS_DBLCLKS|CS_CLASSDC;
  wc.lpfnWndProc=ButtonPickerWndProc;
  wc.cbClsExtra=0;
  wc.cbWndExtra=2;
  wc.hInstance=hInstance;
  wc.hIcon=NULL;
  wc.hCursor=LoadCursor(NULL,IDC_ARROW);
  wc.hbrBackground=NULL;
  wc.lpszMenuName=NULL;
  wc.lpszClassName="Steem Button Picker";
  RegisterClass(&wc);
}


void UnregisterButtonPicker() {
  UnregisterClass("Steem Button Picker",hInstance);
}


LRESULT CALLBACK ButtonPickerWndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar) {
  LONG style=GetWindowLong(Win,GWL_STYLE);
  static int WaitingForStill;
  switch(Mess) {
  case WM_SETFOCUS:
  {
    SetTimer(Win,1,100,NULL);
    if(style&BPS_INSHORTCUT)
    {
      EasyStr Message;
      if((style&1)==0)
      {
        if(NumJoysticks)
          Message=T("Press any key, the middle mouse button or a joystick button/direction.");
        else
          Message=T("Press any key or the middle mouse button.");
/* bcc warning
        Message=(NumJoysticks)
                 ? T("Press any key, the middle mouse button or a joystick button/direction.")
                 : T("Press any key or the middle mouse button.");*/
        Message+="\r\n\r\n";
/*        if(NumJoysticks)
        {
          Message=T("Press any key, the middle mouse button or a joystick \
button/direction.")+"\r\n\r\n";
        }
        else
        {
          Message=T("Press any key or the middle mouse button.")+"\r\n\r\n";
        }*/
      }
      else
#if defined(SSE_GUI_KBD)
        Message=T("Press a key that was on the ST keyboard or shift+F11 for (, \
shift+F12 for ), Page Up for Help or Page Down for Undo.")+"\r\n\r\n";
      Message+=T("For F11 and F12, press shift and the key")+"\r\n\r\n";
      Message+=T("Press F11 or pause/break to clear your selection.");
      //Message+=T("Press the pause/break key to clear your selection.");
#else
        Message=T("Press a key that was on the ST keyboard or F11, F12, Page Up or Page Down.")+"\r\n\r\n";
      Message+=T("Press the pause/break key to clear your selection.");
#endif
      SendMessage(TShortcutBox::InfoWin,WM_SETTEXT,0,(LPARAM)Message.Text);
      TShortcutBox::Picking=true;
    }
  }
  case WM_KILLFOCUS:
    for(int n=0;n<MAX_PC_JOYS;n++) 
      JoyOldPos[n].Valid=0;
    SendMessage(Win,WM_TIMER,0,0);
    WaitingForStill=1;
    //no break
  case WM_ENABLE:
    InvalidateRect(Win,NULL,FALSE);
    if(Mess==WM_KILLFOCUS)
    {
      KillTimer(Win,1);
      if(style&BPS_INSHORTCUT)
      {
        SendMessage(TShortcutBox::InfoWin,WM_SETTEXT,0,(LPARAM)"");
        TShortcutBox::Picking=false;
      }
      SendMessage(GetParent(Win),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(Win),0),(LPARAM)Win);
    }
    break;
  case WM_PAINT:
  {
    RECT rc;
    HBRUSH Bk;
    GetClientRect(Win,&rc);
    PAINTSTRUCT ps;
    BeginPaint(Win,&ps);
    if(!IsWindowEnabled(Win))
      Bk=CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    else if(GetFocus()==Win)
      Bk=CreateSolidBrush(GetSysColor(COLOR_WINDOW));
    else
      Bk=CreateSolidBrush(MidGUIRGB);
    FillRect(ps.hdc,&rc,Bk);
    DeleteObject(Bk);
    SelectObject(ps.hdc,fnt);
    SetBkMode(ps.hdc,TRANSPARENT);
    SetTextColor(ps.hdc,GetSysColor(COLOR_BTNTEXT));
    CentreTextOut(ps.hdc,0,0,rc.right-1,rc.bottom-1,
      DirID_to_text(GetWindowWord(Win,0),style&1),-1);
    EndPaint(Win,&ps);
    return 0;
  }
  case WM_KEYDOWN:case WM_SYSKEYDOWN:
    if(wPar!=VK_NUMLOCK && wPar!=VK_SCROLL && wPar!=VK_LWIN && wPar!=VK_RWIN)
    {
      SHORT vk_menu_state=GetKeyState(VK_MENU);
      SHORT vk_control_state=GetKeyState(VK_CONTROL);
      SHORT vk_shift_state=GetKeyState(VK_SHIFT);
      if( (wPar==VK_PAUSE || wPar==VK_F11) // v402
        && vk_shift_state>=0 && vk_control_state>=0 && vk_menu_state>=0)
        SetWindowWord(Win,0,0xffff);
      else if(style&1)
      {  // ST Keys Only
        if(wPar==VK_SHIFT)
        {
          TModifierState mss=GetLRModifierStates();
          if(!mss.LShift && mss.RShift) 
            wPar=VK_RSHIFT;
          if(mss.LShift && !mss.RShift) 
            wPar=VK_LSHIFT;
        }
        wPar&=0xff;
        if(wPar==VK_RETURN && lPar&0x1000000) 
          wPar=MAKEWORD(VK_RETURN,1);  // Enter
        if(key_table[LOBYTE(wPar)]!=0) 
          SetWindowWord(Win,0,(WORD)wPar);
      }
      else
      {
        if(wPar==VK_SHIFT||wPar==VK_CONTROL||wPar==VK_MENU)
        {
          TModifierState mss=GetLRModifierStates();
          switch(wPar) {
          case VK_SHIFT:
            if(!mss.LShift && mss.RShift)
              wPar=VK_RSHIFT;
            if(mss.LShift && !mss.RShift)
              wPar=VK_LSHIFT;
            break;
          case VK_CONTROL:
            if(!mss.LCtrl && mss.RCtrl)
              wPar=VK_RCONTROL;
            if(mss.LCtrl && !mss.RCtrl)
              wPar=VK_LCONTROL;
            break;
          case VK_MENU:
            if(!mss.LAlt && mss.RAlt)
              wPar=VK_RMENU;
            if(mss.LAlt && !mss.RAlt)
              wPar=VK_LMENU;
            break;
          }//sw
        }
        wPar&=0xff;
        SetWindowWord(Win,0,(WORD)wPar);
      }
      InvalidateRect(Win,NULL,FALSE);
      SendMessage(GetParent(Win),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(Win),0),(LPARAM)Win);
      return 0;
    }
    break;
  case WM_TIMER:
    if((style&1)==0)
    {
      for(int j=0;j<MAX_PC_JOYS;j++)
      {
        if(JoyExists[j])
        {
          JOYINFOEX &ji=JoyPos[j];
          if(JoyOldPos[j].Valid)
          {
            int DirID=0;
            for(int n=0;n<6;n++) // TODO 6?
            {
              if(JoyInfo[j].AxisExists[n])
              {
                DWORD Pos=GetAxisPosition(n,&ji);
                if(Pos!=JoyOldPos[j].AxisPos[n])
                {
                  if(Pos<JoyInfo[j].AxisMid[n]-(JoyInfo[j].AxisLen[n]/4))
                    DirID=-(n+1);
                  else if(Pos>JoyInfo[j].AxisMid[n]+(JoyInfo[j].AxisLen[n]/4))
                    DirID=n+1;
                }
              }
            }
            if(ji.dwButtons!=JoyOldPos[j].Buttons)
            {
              for(int n=0;n<JoyInfo[j].NumButtons;n++)
              {
                if((((ji.dwButtons^JoyOldPos[j].Buttons)>>n)&1)&&(ji.dwButtons>>n)==1)
                {
                  DirID=100+n;
                  break;
                }
              }
            }
            if(JoyInfo[j].AxisExists[AXIS_POV]&&ji.dwPOV<36000)
            {
              if(POV_CONV(ji.dwPOV)!=POV_CONV(OldJoyPos.POV)) 
                DirID=200+MIN(POV_CONV(ji.dwPOV),8ul);
            }
            if(DirID)
            {  // Changed
              if(WaitingForStill==0||(DirID>=100&&DirID<200))
              {
                SetWindowWord(Win,0,MAKEWORD((BYTE)abs_quick(DirID),
                  (BYTE)((DirID>0) ? 10 : 11)+j*10));
                InvalidateRect(Win,NULL,FALSE);
                SendMessage(GetParent(Win),WM_COMMAND,
                  MAKEWPARAM(GetDlgCtrlID(Win),0),(LPARAM)Win);
                if(DirID<100||DirID>=200) 
                  WaitingForStill=2;
              }
            }
            else if(WaitingForStill)
              WaitingForStill--;
          }
          JoyOldPos[j].AxisPos[AXIS_X]=ji.dwXpos;
          JoyOldPos[j].AxisPos[AXIS_Y]=ji.dwYpos;
          if(JoyInfo[j].AxisExists[AXIS_Z]) 
            JoyOldPos[j].AxisPos[AXIS_Z]=ji.dwZpos;
          if(JoyInfo[j].AxisExists[AXIS_R]) 
            JoyOldPos[j].AxisPos[AXIS_R]=ji.dwRpos;
          if(JoyInfo[j].AxisExists[AXIS_U]) 
            JoyOldPos[j].AxisPos[AXIS_U]=ji.dwUpos;
          if(JoyInfo[j].AxisExists[AXIS_V]) 
            JoyOldPos[j].AxisPos[AXIS_V]=ji.dwVpos;
          JoyOldPos[j].Buttons=ji.dwButtons;
          if(JoyInfo[j].AxisExists[AXIS_POV]) 
            OldJoyPos.POV=ji.dwPOV;
          JoyOldPos[j].Valid=true;
        }
      }
      return 0;
    }
    break;
  case WM_LBUTTONDOWN:
    SetFocus(Win);
    break;
  case WM_MBUTTONDOWN:case WM_MBUTTONDBLCLK:
    if(GetFocus()==Win && (style&1)==0)
    {
      SetWindowWord(Win,0,MAKEWORD(0,2));
      InvalidateRect(Win,NULL,FALSE);
      SendMessage(GetParent(Win),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(Win),0),(LPARAM)Win);
    }
    break;
  case WM_MOUSEWHEEL:
    if(GetFocus()==Win && (style&1)==0)
    {
      SHORT Dir=(SHORT)HIWORD(wPar);
      SetWindowWord(Win,0,MAKEWORD((BYTE)((Dir>0)?1:2),2));
      InvalidateRect(Win,NULL,FALSE);
      SendMessage(GetParent(Win),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(Win),0),(LPARAM)Win);
    }
    return 0;
  }
  return DefWindowProc(Win,Mess,wPar,lPar);
}

#endif//WIN32


#ifdef DEADC0DE
#define DIRID_JOY_KEY  1
#define DIRID_JOY_1    2
#define DIRID_JOY_2    3
int ConvertDirID(int OldDirID,int Type) {
  switch(Type) {
  case DIRID_JOY_KEY:
    return MAKEWORD(OldDirID,0);
  case DIRID_JOY_1:case DIRID_JOY_2:
  {
    int JoyNum=Type-DIRID_JOY_1;
    bool Neg=(OldDirID<0);
    return MAKEWORD(abs_quick(OldDirID),10+(JoyNum*10)+(Neg?1:0));
  }
  }
  return OldDirID;
}
#endif

#undef LOGSECTION
