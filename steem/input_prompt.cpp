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
FILE: input_prompt.cpp
DESCRIPTION: A generic string (EasyStr) input prompt.
Used by debugger and ACSI HD manager
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <conditions.h>
#include <mymisc.h>
#include <input_prompt.h>
#include <translate.h> // so this couldn't be in /include!
#include <options.h>

#define IDC_EDIT 100
#define CharHeight GuiSM.mCharHeight

LRESULT CALLBACK InputPrompt_WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar) {
  switch(Mess) {
  case WM_COMMAND:
  {
    switch(LOWORD(wPar)) {
    case IDCANCEL:
      *(bool*)GetProp(Win,"pSuccess")=false;
      SetForegroundWindow((HWND)GetProp(Win,"Parent"));
      DestroyWindow(Win);
      return 0;
    case IDOK:
    {
      EasyStr *pRet=(EasyStr*)GetProp(Win,"pReturnStr");
#if defined(SSE_420R5)
      int Len=(int)SendMessage(GetDlgItem(Win,IDC_EDIT),WM_GETTEXTLENGTH,0,0)+1;
#else
      size_t Len=SendMessage(GetDlgItem(Win,IDC_EDIT),WM_GETTEXTLENGTH,0,0)+1;
#endif
      pRet->SetLength(Len);
      SendMessage(GetDlgItem(Win,IDC_EDIT),WM_GETTEXT,Len,(LPARAM)pRet->Text);
      DestroyWindow(Win);
      return 0;
    }//case 
    }//sw
    break;
  }
  case WM_SETFOCUS:
    SetFocus(GetDlgItem(Win,IDC_EDIT));
    break;
  case DM_GETDEFID:
    return MAKELONG(IDOK,DC_HASDEFID);
  case WM_CLOSE:
    PostMessage(Win,WM_COMMAND,IDCANCEL,0);
    return 0;
  case WM_DESTROY:
    RemoveProp(Win,"pReturnStr");
    RemoveProp(Win,"pSuccess");
    *(HWND*)GetProp(Win,"pWin")=NULL;
    RemoveProp(Win,"pWin");
    RemoveProp(Win,"Parent");
    break;
  }
  return DefWindowProc(Win,Mess,wPar,lPar);
}


bool InputPrompt_Choose(HWND Parent,char *Title,EasyStr &Ret) {
  WNDCLASS wc={
    0,
    InputPrompt_WndProc,
    0,
    0,
    GetModuleHandle(NULL),
    NULL,
    LoadCursor(NULL,IDC_ARROW),
    HBRUSH(COLOR_BTNFACE+1),
    NULL,
    "Generic Input Prompt"
  };
  RegisterClass(&wc);
#ifdef SSE_GUI_FIX
  int padded4_3=GetSystemMetrics(92)*3;
#else
  int padded4_3=0;
#endif
  HWND Win=CreateWindowEx(0,"Generic Input Prompt",Title,DS_MODALFRAME,100,10,GUIMUL(326+padded4_3),
   GUIMUL(10+25+5+25+5+6+padded4_3)+GuiSM.cy_caption(),Parent,NULL,GetModuleHandle(NULL),NULL);
  if(Win==NULL||!IsWindow(Win)) 
    return false;
  bool Success=true;
  SetProp(Win,"pReturnStr",&Ret);
  SetProp(Win,"pSuccess",&Success);
  SetProp(Win,"pWin",&Win);
  SetProp(Win,"Parent",Parent);
  HWND hEd=CreateWindowEx(WS_EX_CLIENTEDGE,"Edit",Ret,WS_CHILD|WS_VISIBLE|WS_TABSTOP
   |ES_AUTOHSCROLL, 10,10,GUIMUL(300),CharHeight,Win,(HMENU)IDC_EDIT,GetModuleHandle(NULL),NULL);
  SendMessage(hEd,EM_SETSEL,0,0xffffffff);
  CreateWindow("Button",T("OK"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,
   100,GUIMUL(40),GUIMUL(100),CharHeight,Win,(HMENU)IDOK,GetModuleHandle(NULL),NULL);
  CreateWindow("Button",T("Cancel"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
   GUIMUL(210),GUIMUL(40),GUIMUL(100),CharHeight,Win,(HMENU)IDCANCEL,GetModuleHandle(NULL),NULL);
  SetWindowAndChildrensFont(Win,SSEConfig.GuiFont());
  CentreWindow(Win,false);
  ShowWindow(Win,SW_SHOW);
  MSG mess;
  while(GetMessage(&mess,NULL,0,0))
  {
    if(!IsDialogMessage(Win,&mess))
    {
      TranslateMessage(&mess);
      DispatchMessage(&mess);
    }
    if(Win==NULL) 
      break;
  }
  UnregisterClass("Generic Input Prompt",GetModuleHandle(NULL));
  return Success;
}

#undef CharHeight
#undef IDC_EDIT
#undef LOGSECTION
