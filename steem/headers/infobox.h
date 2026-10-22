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
FILE: infobox.h
DESCRIPTION: Declarations for Steem's general information dialog.
class TGeneralInfo
---------------------------------------------------------------------------*/

#pragma once
#ifndef INFOBOX_H
#define INFOBOX_H

#include <scrollingcontrolswin.h>
#include "stemdialogs.h"
#include "SSE.h"


#pragma pack(push, 8)

class TGeneralInfo : public TStemDialog {
public: //TODO
enum EGeneralInfo {
  ABOUT,DRAWSPEED,LINKS,README,UNIXREADME,HOWTO_DISK,HOWTO_CART,FAQ,FAQ_SSE,HINTS,
  README_SSE,LICENCE,TRACEFILE,BUGS,STATS,NUM_INFOPAGE,
  ID_SCROLLER=203,ID_TEXT=500,ID_RICHTEXT,ID_FIND,ID_SEARCH,ID_EDIT,IDC_TEXTCONTROL
};
#if !defined(SSE_GUI_RICHEDIT2)
  int MaxLinkID;
  void GetHyperlinkLists(EasyStringList &,EasyStringList &);
#endif
#ifdef WIN32
  static WNDPROC OldEditWndProc;
  HBRUSH BkBrush;
  HIMAGELIST il;
#if !defined(SSE_GUI_RICHEDIT)
  HFONT hFontCourier;
#endif
  EasyStr SearchText;
  int page_l,page_w,page_h;
  int Page;
  ScrollControlWin Scroller;
  static LRESULT CALLBACK WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar);
  void DestroyCurrentPage();
  void ManageWindowClasses(bool Unreg);
#endif
#ifdef UNIX
  int page_l,page_w;
  int Page;
  static int WinProc(TGeneralInfo*,Window,XEvent*);
  static int button_notifyproc(hxc_button*,int,int*);
  static int listview_notify_proc(hxc_listview*,int,INT_PTR);
  static int edit_notify_proc(hxc_edit*,int,INT_PTR);
	void ShowTheReadme(char*,bool=false);
	hxc_button gb,thanks_label;
	hxc_textdisplay about,thanks;
	hxc_textdisplay readme;
	hxc_button steem_link,email_link;
	hxc_scrollarea sa;
	hxc_listview page_lv;
  int last_find_idx;
#endif
public:
  TGeneralInfo();
  ~TGeneralInfo() { 
    Hide();
#ifdef WIN32
    DeleteObject(BkBrush);
#endif
  }
  void Show(),Hide();
  void ToggleVisible() {
    IsVisible() ? Hide() : Show();
  }
  void LoadData(bool FirstLoad,TConfigStoreFile *pCSF,bool*);
  void SaveData(bool FinalSave,TConfigStoreFile *pCSF);
  void CreatePage(int pg);
  void CreateSpeedPage(),CreateAboutPage(),CreateLinksPage();
  void CreateReadmePage(int pg);
  void UpdatePositions();
#ifdef WIN32
  INT_PTR DrawColumn(int x,int y,INT_PTR id,char *t1,...);
  EasyStr dp4_disp(int val);
  void LoadIcons();
  bool HasHandledMessage(MSG *mess);
#endif
};

extern TGeneralInfo InfoBox;


#pragma pack(pop)


#endif//#ifndef INFOBOX_H
