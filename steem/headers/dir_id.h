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
FILE: dir_id.h
DESCRIPTION: Declarations for Steem's DirID system.
---------------------------------------------------------------------------*/

#pragma once
#ifndef DIRID_DECLA_H
#define DIRID_DECLA_H

void init_DirID_to_text();
bool IsDirIDPressed(int ID,int DeadZonePercent,bool CheckDisable,bool DiagonalPOV=false);

#if defined(SSE_GUI_STATUS_BAR)
extern bool bPCInputExists; // used by joy_check_controllers()
#endif

#ifdef WIN32
void RegisterButtonPicker(),UnregisterButtonPicker();
LRESULT CALLBACK ButtonPickerWndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar);
#define BLANK_DIRID(i) (i==0 || HIBYTE(i)==0xff)
#define NOT_BLANK_DIRID(i) (i!=0 && HIBYTE(i)!=0xff)
#endif

#ifdef UNIX
#define BLANK_DIRID(i) (HIBYTE(i)==0xff)
#define NOT_BLANK_DIRID(i) (HIBYTE(i)!=0xff)
EasyStr DirID_to_text(int ID,bool st_key);
extern char *KeyboardButtonName[256];
#endif


//int ConvertDirID(int OldDirID,int Type);


#endif//DIRID_DECLA_H
