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
FILE: diskman_diags.cpp
DESCRIPTION: The dialogs that can be shown by the Disk Manager:
Disk Database, Disk content, Create disk, Create Shortcuts, Disk properties
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <diskman.h>
#include <computer.h>
#include <translate.h>
#include <archive.h>
#include <choosefolder.h>
#include <di_get_contents.h>
#include <display.h>
#include <debug.h>

#define CharHeight GuiSM.mCharHeight
#define LineHeight GUIMUL(30)

#define IDC_DB_SEARCH 103
#define IDC_DB_LIST 111
#define IDC_DB_STEEMSITE 301
#define IDC_CONTENT_PATH 103
#define IDC_CONTENT_TOSEC 101
#define IDC_CONTENT_CRC 104
#define IDC_CONTENT_LIST 111
#define IDC_CONTENT_DESTFOLDER 201
#define IDC_CONTENT_BROWSE 202
#define IDC_CONTENT_ONCONFLICT 211
#define IDC_CONTENT_APPENDNAME 220
#define IDC_CONTENT_SHORTNAME 221
#define IDC_CREATE_EXT_ST 7341
#define IDC_CREATE_EXT_MSA 7342
#define IDC_CREATE_EXT_DIM 7343
#define IDC_CREATE_SIDES 101
#define IDC_CREATE_SECTORS 103
#define IDC_CREATE_TRACKS 105
#define IDS_CREATE_SIZE 106
#define IDC_LINKS_TARGET 101
#define IDC_LINKS_BROWSE 102
#define IDC_LINKS_TARGETMULTI 201
#define IDC_LINKS_BROWSEMULTI 202
#define IDC_LINKS_EDITBASE 301
#define IDC_PROP_PATH 101
#define IDC_PROP_LINKPATH 111
#define IDS_PROP_SIZE 112
#define IDC_PROP_LIST 121
#define IDS_PROP_GROUP 130
#define IDC_PROP_BPB 131
#define IDC_PROP_DATABYTES 132
#define IDC_PROP_SIDES 141
#define IDC_PROP_TRACKS 151
#define IDC_PROP_SECTORS 161
#define IDC_PROP_BYTES 171
#define IDC_PROP_DETECT 180
#define IDC_PROP_APPLY 181
#define IDC_PROP_CONTENT 190

#if !defined(SSE_LIBRETRONUKE)
void TDiskManager::ShowDatabaseDiag() {
  if(!GetContentsCheckExist()) // database should be available
    return;

#ifdef WIN32
  if(DatabaseDiag!=NULL) // not modal, only one window
    return;
  int th=GetTextSize(Font,T("To download disks see Steem's ")).Height;
  // a main window so it doesn't go over DM
  DatabaseDiag=CreateWindowEx(WS_EX_CONTROLPARENT,"Steem Disk Manager Dialog",
    T("Search Disk Image Database"),WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,GUIMUL(506)
    +GuiSM.cx_frame(),GUIMUL(350+10+6)+th+GuiSM.cy_caption(),ParentWin,NULL,hInstance,NULL);

#ifndef SSE_LEAN_AND_MEAN
  if(DatabaseDiag==NULL || !IsWindow(DatabaseDiag))
    return;
#endif
  //EnableWindow(Handle,FALSE);
  SetWindowLongPtr(DatabaseDiag,GWLP_USERDATA,(LONG_PTR)this);
  int y=10,w,page_r=GUIMUL(500)-10;
  HWND Win;
  w=GetTextSize(Font,T("Search for")).Width;
  CreateWindow("Static",T("Search for"),WS_CHILD|WS_VISIBLE,
    10,y,w,CharHeight,DatabaseDiag,(HMENU)IDS_STATIC,hInstance,NULL);
  CreateWindowEx(WS_EX_CLIENTEDGE,"Edit","",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL, 
    w+15,y,page_r-GUIMUL(65)-(w+15),CharHeight,DatabaseDiag,(HMENU)IDC_DB_SEARCH,hInstance,NULL);
  SendDlgItemMessage(DatabaseDiag,IDC_DB_SEARCH,WM_SETTEXT,0,(LPARAM)(DatabaseFind.Text));
  //CreateWindow("Button",T("Go"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,
  CreateWindow("Button",T("Find"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,
    page_r-GUIMUL(60),y,GUIMUL(50),CharHeight,DatabaseDiag,(HMENU)IDOK,hInstance,NULL);
  y+=LineHeight;
  Win=CreateWindowEx(WS_EX_CLIENTEDGE,WC_LISTVIEW,"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|LVS_SINGLESEL
    |LVS_REPORT,10,y,page_r-10,GUIMUL(300),DatabaseDiag,(HMENU)IDC_DB_LIST,hInstance,NULL);
  RECT rc;
  GetClientRect(Win,&rc);
  LV_COLUMN lvc;
  lvc.mask=LVCF_FMT|LVCF_SUBITEM|LVCF_TEXT|LVCF_WIDTH;
  lvc.fmt=LVCFMT_LEFT;
  lvc.cx=GUIMUL(180);
  lvc.pszText=StaticT("Name");
  lvc.iSubItem=0;
  SendMessage(Win,LVM_INSERTCOLUMN,0,(LPARAM)&lvc);
  lvc.fmt=LVCFMT_LEFT;
  lvc.cx=GUIMUL(300);
  lvc.pszText=StaticT("Contents");
  lvc.iSubItem=1;
  SendMessage(Win,LVM_INSERTCOLUMN,1,(LPARAM)&lvc);
  y+=GUIMUL(310);
  w=GetTextSize(Font,T("To download disks see Steem's ")).Width;
  CreateWindow("Static",T("To download disks see Steem's "),WS_CHILD|WS_VISIBLE,
    10,y,w,th,DatabaseDiag,(HMENU)IDS_STATIC,hInstance,NULL);
  CreateWindowEx(0,"Steem HyperLink",T("links page")+"|"+STEEM_WEB_LEGACY+"links.htm",
    WS_CHILD|WS_VISIBLE,10+w,y,GUIMUL(200),th,DatabaseDiag,(HMENU)IDC_DB_STEEMSITE,hInstance,NULL);
  SetWindowAndChildrensFont(DatabaseDiag,Font);
  DiagFocus=GetDlgItem(DatabaseDiag,IDC_DB_SEARCH);
  ShowWindow(DatabaseDiag,SW_SHOW);
#endif

#ifdef UNIX
  int w=600,h=10+30+300+10+hxc::font->ascent+hxc::font->descent+10,y=10;
  Window handle=hxc::create_modal_dialog(XD,w,h,T("Search Disk Image Database"),0);
  if (handle==0) return;

  hxc_button *p_but,*p_but2;
  hxc_edit *p_ed;
  hxc_textdisplay result_td;

  p_but=new hxc_button(XD,handle,10,y,0,25,NULL,this,BT_LABEL,T("Search for"),0,hxc::col_bk);

  //p_but2=new hxc_button(XD,handle,w-10,y,0,25,diag_but_np,this,BT_TEXT,T("Go"),100,hxc::col_bk);
  p_but2=new hxc_button(XD,handle,w-10,y,0,25,diag_but_np,this,BT_TEXT,T("Find"),100,hxc::col_bk);
  p_but2->x-=p_but2->w;
  XMoveWindow(XD,p_but2->handle,p_but2->x,p_but2->y);

  p_ed=new hxc_edit(XD,handle,10+p_but->w+5,y,w-10-10-p_but->w-5-p_but2->w-5,25,diag_ed_np,this);
  p_ed->set_text("");
  p_ed->id=101;
  y+=LineHeight;

  result_td.id=200;
  result_td.border=1;
  result_td.pad_x=5;
  result_td.sy=0;
  result_td.textheight=(hxc::font->ascent)+(hxc::font->descent)+2;
  result_td.create(XD,handle,10,y,w-20,300,hxc::col_white,true);
  y+=310;

  p_but=new hxc_button(XD,handle,10,y,0,0,NULL,this,BT_LABEL,
          T("To download disks see Steem's "),0,hxc::col_bk);
  new hxc_button(XD,handle,10+p_but->w,y,0,0,hyperlink_np,this,BT_LINK|BT_TEXT,
          T("links page")+"|"+STEEM_WEB_LEGACY+"links.htm",0,hxc::col_bk);
  hxc::show_modal_dialog(XD,handle,true,p_ed->handle);
  hxc::destroy_modal_dialog(XD,handle);
#endif//UNIX
}
#endif//#if !defined(SSE_LIBRETRONUKE)

void TDiskManager::ShowContentDiag() { // "Get CRC32 and contents"

#ifdef WIN32
  int h=GetTextSize(Font,T("Contents")).Height;
  ContentDiag=CreateWindowEx(WS_EX_CONTROLPARENT,"Steem Disk Manager Dialog",
    T("Disk Image Contents"),WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,GUIMUL(406)
    +GuiSM.cx_frame(),h+GUIMUL(382)+GuiSM.cy_frame()+GuiSM.cy_caption()+6,Handle,NULL,
    hInstance,NULL);
#ifndef SSE_LEAN_AND_MEAN
  if(ContentDiag==NULL||IsWindow(ContentDiag)==0)
    return;
#endif
  EnableWindow(Handle,FALSE);
  SetWindowLongPtr(ContentDiag,GWLP_USERDATA,(LONG_PTR)this);
  if(FullScreen) //?
    SetParent(ContentDiag,StemWin);
  int y=10,w,page_r=GUIMUL(400)-10;
  HWND Win;
  w=GetTextSize(Font,T("Disk path")).Width;
  CreateWindow("Static",T("Disk path"),WS_CHILD|WS_VISIBLE,
    10,y,w,CharHeight,ContentDiag,(HMENU)IDS_STATIC,hInstance,NULL);
  CreateWindowEx(WS_EX_CLIENTEDGE,"Edit",contents_sl[0].String,
    WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL|ES_READONLY,
    w+15,y,page_r-(w+15),CharHeight,ContentDiag,(HMENU)IDC_CONTENT_PATH,hInstance,NULL);
  y+=LineHeight;
  // most from TOSEC, full
  w=GetTextSize(Font,T("TOSEC Name")).Width;
  CreateWindow("Static",T("TOSEC Name"),WS_CHILD|WS_VISIBLE,
    10,y,w,CharHeight,ContentDiag,(HMENU)IDS_STATIC,hInstance,NULL);
  CreateWindowEx(WS_EX_CLIENTEDGE,"Edit",contents_sl[1].String,
    WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL|ES_READONLY,
    w+15,y,page_r-(w+15),CharHeight,ContentDiag,(HMENU)IDC_CONTENT_TOSEC,hInstance,NULL);
  y+=LineHeight;

  w=GetTextSize(Font,T("CRC32")).Width;
  CreateWindow("Static",T("CRC32"),WS_CHILD|WS_VISIBLE,
    10,y,w,CharHeight,ContentDiag,(HMENU)IDS_STATIC,hInstance,NULL);
  char sCrc[32];
  sprintf(sCrc,"%08X",(DWORD)contents_sl[0].Data[0]);
  CreateWindowEx(WS_EX_CLIENTEDGE,"Edit",sCrc,WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL
    |ES_READONLY,w+15,y,GUIMUL(60),CharHeight,ContentDiag,(HMENU)IDC_CONTENT_CRC,hInstance,NULL);
  y+=LineHeight;

  CreateWindow("Static",T("Contents"),WS_CHILD|WS_VISIBLE,
    10,y,page_r-10,h+1,ContentDiag,(HMENU)IDS_STATIC,hInstance,NULL);
  y+=h+2;
  Win=CreateWindowEx(WS_EX_CLIENTEDGE,WC_LISTVIEW,"",WS_CHILD|WS_VISIBLE
    |WS_TABSTOP|LVS_SINGLESEL|LVS_REPORT|LVS_NOCOLUMNHEADER,
    10,y,page_r-10,GUIMUL(150),ContentDiag,(HMENU)IDC_CONTENT_LIST,hInstance,NULL);
  ListView_SetExtendedListViewStyle(Win,LVS_EX_CHECKBOXES);
  RECT rc;
  GetClientRect(Win,&rc);
  LV_COLUMN lvc;
  lvc.mask=LVCF_FMT|LVCF_SUBITEM|LVCF_TEXT|LVCF_WIDTH;
  lvc.fmt=LVCFMT_LEFT;
  lvc.cx=rc.right-GuiSM.cx_vscroll();
  lvc.pszText="";
  lvc.iSubItem=0;
  SendMessage(Win,LVM_INSERTCOLUMN,0,(LPARAM)&lvc);
  LV_ITEM lvi;
  lvi.mask=LVIF_TEXT|LVIF_PARAM;
  for(int i=2;i<contents_sl.NumStrings;i++) // list individual games on the compil (for instance)
  {
    lvi.iSubItem=0;
    lvi.pszText=contents_sl[i].String;
    lvi.lParam=i;
    lvi.iItem=i-2;
    SendMessage(Win,LVM_INSERTITEM,0,(LPARAM)&lvi);
    ListView_SetItemState(Win,i-2,LVI_SI_CHECKED,LVIS_STATEIMAGEMASK);
  }
  y+=GUIMUL(160);
  int Disable=(contents_sl.NumStrings<=2)?WS_DISABLED:0;
  CreateWindow("Button",T("Create Shortcuts To Selected Contents"),WS_CHILD|WS_VISIBLE|BS_GROUPBOX
    |Disable,10,y,page_r-10,GUIMUL(115),ContentDiag,(HMENU)IDS_STATIC,hInstance,NULL);
  y+=LineHeight;
  w=GetTextSize(Font,T("In folder")).Width;
  CreateWindow("Static",T("In folder"),WS_CHILD|WS_VISIBLE|Disable,
    20,y,w,CharHeight,ContentDiag,(HMENU)IDS_STATIC,hInstance,NULL);
  Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Edit",ContentsLinksPath,WS_CHILD|WS_VISIBLE|WS_TABSTOP
    |ES_AUTOHSCROLL|Disable,25+w,y,page_r-GUIMUL(80+5)-(w+25),CharHeight,ContentDiag,
    (HMENU)IDC_CONTENT_DESTFOLDER,hInstance,NULL);
  SendMessage(Win,EM_LIMITTEXT,SSE_MAX_PATH,0);
  CreateWindow("Button",T("Browse"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE|Disable,
    page_r-GUIMUL(80),y,GUIMUL(70),CharHeight,ContentDiag,(HMENU)IDC_CONTENT_BROWSE,hInstance,NULL);
  y+=LineHeight;
  w=GetCheckBoxSize(Font,T("Append disk name")).Width;
  Win=CreateWindow("Button",T("Append disk name"),WS_CHILD|WS_VISIBLE|Disable|BS_AUTOCHECKBOX,20,
    y,w,CharHeight,ContentDiag,(HMENU)IDC_CONTENT_APPENDNAME,hInstance,NULL);
  SendMessage(Win,BM_SETCHECK,BST_CHECKED,0);
  Str ShortName=contents_sl[1].String;
  Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Edit",ShortName,WS_CHILD|WS_VISIBLE|WS_TABSTOP
    |ES_AUTOHSCROLL|Disable,25+w,y,page_r-10-(w+25),CharHeight,ContentDiag,
    (HMENU)IDC_CONTENT_SHORTNAME,hInstance,NULL);
  SendMessage(Win,EM_LIMITTEXT,50,0);
  y+=LineHeight;
  w=GetTextSize(Font,T("On name conflict")).Width;
  CreateWindow("Static",T("On name conflict"),WS_CHILD|WS_VISIBLE|Disable,
    20,y,w,CharHeight,ContentDiag,(HMENU)IDS_STATIC,hInstance,NULL);
  Win=CreateWindow("Combobox","",WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|Disable,25+w,y,
    page_r-GUIMUL(100)-(25+w)-10,GUIMUL(200),ContentDiag,(HMENU)IDC_CONTENT_ONCONFLICT,hInstance,NULL);
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Skip"));
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Overwrite"));
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Rename new"));
  SendMessage(Win,CB_SETCURSEL,ContentConflictAction,0);
  CreateWindow("Button",T("Create"),WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON|WS_TABSTOP|Disable,
    page_r-GUIMUL(100),y,GUIMUL(90),CharHeight,ContentDiag,(HMENU)IDOK,hInstance,NULL);
  SetWindowAndChildrensFont(ContentDiag,Font);
  CentreWindow(ContentDiag,false);
  DiagFocus=GetDlgItem(ContentDiag,IDC_CONTENT_TOSEC);
  ShowWindow(ContentDiag,SW_SHOW);
#endif//WIN32

#ifdef UNIX
  if (contents_sl.NumStrings<2) return;

  int w=500,h=10+30+20+185+30+30+30+30+10,y=10;
  if (contents_sl.NumStrings==2) h=10+30+10;
  Window handle=hxc::create_modal_dialog(XD,w,h,T("Disk Image Contents"),0);
  if (handle==0) return;

  hxc_button *p_but,*p_but2,*p_group,*p_append_but=NULL;
  hxc_edit *p_path_ed=NULL,*p_append_ed=NULL;
  hxc_dropdown *p_conflict_dd=NULL;
  hxc_listview cont_lv;
  Window focus=0;
  new hxc_button(XD,handle,10,y,0,25,NULL,this,BT_LABEL,T("TOSEC Name")+" - "+contents_sl[1].String,0,hxc::col_bk);
  y+=LineHeight;

  if (contents_sl.NumStrings>2){
    p_but=new hxc_button(XD,handle,10,y,0,0,NULL,this,BT_LABEL,T("Contents"),0,hxc::col_bk);
    y+=p_but->h+2;

    cont_lv.lpig=&Ico16;
    cont_lv.display_mode=1;
    cont_lv.checkbox_mode=true;
    cont_lv.sl.DeleteAll();
    cont_lv.sl.Sort=eslNoSort;
    for (int i=2;i<contents_sl.NumStrings;i++){
      cont_lv.additem(contents_sl[i].String,101+ICO16_TICKED);
    }
    cont_lv.create(XD,handle,10,y,w-20,180,diag_lv_np,this);
    y+=185;

    p_group=new hxc_button(XD,handle,10,y,w-20,120,NULL,this,BT_GROUPBOX,T("Create Links To Selected Contents"),0,hxc::col_bk);

    w=p_group->w;
    y=25;

    p_but=new hxc_button(XD,p_group->handle,10,y,0,25,NULL,this,BT_LABEL,T("In folder"),0,hxc::col_bk);

    p_but2=new hxc_button(XD,p_group->handle,w-10,y,0,25,diag_but_np,this,BT_TEXT,T("Browse"),200,hxc::col_bk);
    p_but2->x-=p_but2->w;
    XMoveWindow(XD,p_but2->handle,p_but2->x,p_but2->y);

    p_path_ed=new hxc_edit(XD,p_group->handle,10+p_but->w+5,y,w-10-10-p_but->w-5-p_but2->w-5,25,NULL,this);
    p_path_ed->set_text(ContentsLinksPath);
    p_path_ed->id=201;
    y+=LineHeight;
    focus=p_path_ed->handle;

    p_append_but=new hxc_button(XD,p_group->handle,10,y,0,25,NULL,this,BT_CHECKBOX,T("Append disk name"),101,hxc::col_bk);
    p_append_but->set_check(true);

    p_append_ed=new hxc_edit(XD,p_group->handle,10+p_append_but->w+5,y,w-10-10-p_append_but->w-5,25,NULL,this);
    p_append_ed->set_text(contents_sl[1].String);
    y+=LineHeight;

    p_but=new hxc_button(XD,p_group->handle,10,y,0,25,NULL,this,BT_LABEL,T("On name conflict"),0,hxc::col_bk);

    p_but2=new hxc_button(XD,p_group->handle,w-10,y,0,25,hxc::modal_but_np,this,BT_TEXT,T("Create"),1,hxc::col_bk);
    p_but2->x-=p_but2->w;
    XMoveWindow(XD,p_but2->handle,p_but2->x,p_but2->y);

    p_conflict_dd=new hxc_dropdown(XD,p_group->handle,10+p_but->w+5,y,w-10-10-p_but->w-5-p_but2->w-10,200,NULL,this);
    p_conflict_dd->additem(T("Skip"));
    p_conflict_dd->additem(T("Overwrite"));
    p_conflict_dd->additem(T("Rename new"));
    p_conflict_dd->changesel(ContentConflictAction);
  }

  Str DestFol="///",DiskName,NewLink;
  for(;;){
    int id=hxc::show_modal_dialog(XD,handle,true,focus);

    if (contents_sl.NumStrings<=2) break;

    ContentConflictAction=p_conflict_dd->sel;
    if (id==1){ // Create links
      DestFol=p_path_ed->text;
      NO_SLASH(DestFol);
      if (p_append_but->checked) DiskName=Str(" (")+p_append_ed->text+")";
      CreateDirectory(DestFol,NULL);
      if (GetFileAttributes(DestFol)==0xffffffff){
        Alert(T("Invalid directory"),T("ERROR"),MB_ICONEXCLAMATION);
      }else{
        char *ext;
        ext=strrchr(contents_sl[0].String,'.');
        if (ext==NULL) ext="";
        for (int i=2;i<contents_sl.NumStrings;i++){
          if (cont_lv.sl[i].Data[0]==101+ICO16_TICKED){
            NewLink=DestFol+SLASH+Str(contents_sl[i].String)+DiskName+ext;
            if (Exists(NewLink)){
              if (ContentConflictAction==0){
                NewLink="";
              }else if (ContentConflictAction==2){
                NewLink=GetUniquePath(DestFol,Str(contents_sl[i].String)+DiskName+ext);
              }
            }
            if (NewLink.NotEmpty()) symlink(contents_sl[0].String,NewLink);
          }
        }
        break;
      }
    }else{
      break;
    }
  }
  hxc::destroy_modal_dialog(XD,handle);

  if (IsSameStr_I(DestFol,DisksFol)){
    set_path(DisksFol);
    dir_lv.select_item_by_name(GetFileNameFromPath(NewLink));
  }
#endif//UNIX
}


#ifdef WIN32

void TDiskManager::ShowDiskDiag() {
  DiskDiag=CreateWindowEx(WS_EX_CONTROLPARENT,"Steem Disk Manager Dialog",
    T("Create Custom Disk Image"),WS_CAPTION|WS_SYSMENU,100,100,GUIMUL(256)
    +GuiSM.cx_frame(),GUIMUL(171+30)+GuiSM.cy_caption(),Handle,NULL,hInstance,NULL);
#ifndef SSE_LEAN_AND_MEAN
  if(DiskDiag==NULL || !IsWindow(DiskDiag))
    return;
#endif
  EnableWindow(Handle,FALSE);
  SetWindowLongPtr(DiskDiag,GWLP_USERDATA,(LONG_PTR)this);
  if(FullScreen) 
    SetParent(DiskDiag,StemWin);
  int y=14,x=10,HorizontalSeparation=GUIMUL(5);
  
  // radio buttons for image type, sticky but not persistent
  int mask=WS_CHILD|BS_AUTORADIOBUTTON|WS_VISIBLE; 
  int Wid=GetCheckBoxSize(Font,T(extension_list[EXT_ST])).Width;
  HWND Win=CreateWindow("Button",T(extension_list[EXT_ST]),mask|WS_GROUP,x,y,Wid,CharHeight,
                        DiskDiag,(HMENU)IDC_CREATE_EXT_ST,hInstance,NULL);
  int Offset=Wid+HorizontalSeparation;
  Wid=GetCheckBoxSize(Font,T(extension_list[EXT_MSA])).Width;
  Win=CreateWindow("Button",T(extension_list[EXT_MSA]),mask,x+Offset,y,Wid,CharHeight,
                   DiskDiag,(HMENU)IDC_CREATE_EXT_MSA,hInstance,NULL);
  Offset+=Wid+HorizontalSeparation;
  Wid=GetCheckBoxSize(Font,T(extension_list[EXT_DIM])).Width;
  Win=CreateWindow("Button",T(extension_list[EXT_DIM]),mask,x+Offset,y,Wid,CharHeight,
                   DiskDiag,(HMENU)IDC_CREATE_EXT_DIM,hInstance,NULL);
  Win=GetDlgItem(DiskDiag,IDC_CREATE_EXT_ST+DiskImageType-1);
  SendMessage(Win,BM_SETCHECK,TRUE,0);
  
  // updown controls for sides/tracks/sectors
  y+=LineHeight;
  Wid=get_text_width(T("Sides"));
  CreateWindow("Static",T("Sides"),WS_CHILD|WS_VISIBLE,
    10,y,Wid,CharHeight,DiskDiag,(HMENU)IDS_STATIC,hInstance,NULL);
  HWND hEdit0=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER|WS_VISIBLE,
    GUIMUL(150),y,GUIMUL(40),CharHeight,DiskDiag,(HMENU)IDC_CREATE_SIDES,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT|WS_VISIBLE,0,0,0,0,DiskDiag,(HMENU)IDC_CREATE_SIDES,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit0,0);
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(2,1));
  SendMessageW(Win,UDM_SETPOS32,0,SidesIdx+1);
  y+=LineHeight;
  Wid=get_text_width(T("Tracks"));
  CreateWindow("Static",T("Tracks"),WS_CHILD|WS_VISIBLE,
    10,y,Wid,CharHeight,DiskDiag,(HMENU)IDS_STATIC,hInstance,NULL);
  HWND hEdit1=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER|WS_VISIBLE,
    GUIMUL(150),y,GUIMUL(40),CharHeight,DiskDiag,(HMENU)IDC_CREATE_TRACKS,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT
    |UDS_ALIGNRIGHT|WS_VISIBLE,0,0,0,0,DiskDiag,(HMENU)IDC_CREATE_TRACKS,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit1,0);
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(FLOPPY_MAX_TRACK_NUM,40));
  SendMessageW(Win,UDM_SETPOS32,0,TracksIdx);
  y+=LineHeight;
  Wid=get_text_width(T("Sectors"));
  CreateWindow("Static",T("Sectors"),WS_CHILD|WS_VISIBLE,
    10,y,Wid,CharHeight,DiskDiag,(HMENU)IDS_STATIC,hInstance,NULL);
  HWND hEdit2=CreateWindow("Edit",NULL,WS_CHILD|WS_TABSTOP|WS_BORDER|WS_VISIBLE,
    GUIMUL(150),y,GUIMUL(40),CharHeight,DiskDiag,(HMENU)IDC_CREATE_SECTORS,hInstance,NULL);
  Win=CreateWindow(UPDOWN_CLASS,NULL,WS_CHILD|WS_TABSTOP|UDS_ARROWKEYS|UDS_SETBUDDYINT|UDS_ALIGNRIGHT
                   |WS_VISIBLE,0,0,0,0,DiskDiag,(HMENU)IDC_CREATE_SECTORS,hInstance,NULL);
  SendMessage(Win,UDM_SETBUDDY,(WPARAM)hEdit2,0);
  SendMessageW(Win,UDM_SETRANGE,0,MAKELPARAM(FLOPPY_MAX_SECTOR_NUM,6));
  SendMessageW(Win,UDM_SETPOS32,0,SecsPerTrackIdx);
  int databytes=GetDiskSelectionSize();
  y+=LineHeight;
  char s[64];
  sprintf(s,"%s: %d %s",T("Disk size").Text,databytes>>10,T(" KB").Text);
  CreateWindow("Static",s,WS_CHILD|WS_VISIBLE,
    10,y,GUIMUL(230),CharHeight,DiskDiag,(HMENU)IDS_CREATE_SIZE,hInstance,NULL);
  y+=LineHeight;
  CreateWindow("Button",T("OK"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,
    GUIMUL(70),y,GUIMUL(80),CharHeight,DiskDiag,(HMENU)IDOK,hInstance,NULL);
  CreateWindow("Button",T("Cancel"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
    GUIMUL(160),y,GUIMUL(80),CharHeight,DiskDiag,(HMENU)IDCANCEL,hInstance,NULL);
  SetWindowAndChildrensFont(DiskDiag,Font);
  CentreWindow(DiskDiag,false);
  DiagFocus=GetDlgItem(DiskDiag,IDC_CREATE_SIDES);
  ShowWindow(DiskDiag,SW_SHOW);
}


void TDiskManager::ShowLinksDiag() { // on right dragging to disk manager? TODO test
  LinksDiag=CreateWindowEx(WS_EX_CONTROLPARENT,"Steem Disk Manager Dialog",
    T("Create Multiple Shortcuts"),WS_CAPTION,100,100,GUIMUL(406)+GuiSM.cx_frame(),
    GUIMUL(376)+GuiSM.cy_caption(),Handle,NULL,hInstance,NULL);
#ifndef SSE_LEAN_AND_MEAN
  if(LinksDiag==NULL || !IsWindow(LinksDiag))
    return;
#endif
  EnableWindow(Handle,FALSE);
  SetWindowLongPtr(LinksDiag,GWLP_USERDATA,(LONG_PTR)this);
  if(FullScreen) 
    SetParent(LinksDiag,StemWin);
  LONG Wid=GetTextSize(Font,T("Create shortcuts to")).Width;
  CreateWindow("Static",T("Create shortcuts to"),WS_CHILD|WS_VISIBLE,
    10,14,Wid,CharHeight,LinksDiag,(HMENU)IDS_STATIC,hInstance,NULL);
  HWND Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Edit",LinksTargetPath,
    WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,
    Wid+15,10,GUIMUL(300)-(Wid+5),CharHeight,LinksDiag,(HMENU)IDC_LINKS_TARGET,hInstance,NULL);
  SendMessage(Win,EM_LIMITTEXT,SSE_MAX_PATH,0);
  CreateWindow("Button",T("Browse"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE,
    GUIMUL(315),10,GUIMUL(75),CharHeight,LinksDiag,(HMENU)IDC_LINKS_BROWSE,hInstance,NULL);
  Wid=GetTextSize(Font,T("In folder")).Width;
  CreateWindow("Static",T("In folder"),WS_CHILD|WS_VISIBLE,10,GUIMUL(44),Wid,CharHeight,LinksDiag,
    (HMENU)IDS_STATIC,hInstance,NULL);
  if(MultipleLinksPath.IsEmpty()) 
    MultipleLinksPath=HomeFol;
  Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Edit",MultipleLinksPath,WS_CHILD|WS_VISIBLE|WS_TABSTOP
    |ES_AUTOHSCROLL,Wid+15,GUIMUL(40),GUIMUL(300)-(Wid+5),CharHeight,LinksDiag,
    (HMENU)IDC_LINKS_TARGETMULTI,hInstance,NULL);
  SendMessage(Win,EM_LIMITTEXT,SSE_MAX_PATH,0);
  CreateWindow("Button",T("Browse"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE,
    GUIMUL(315),GUIMUL(40),GUIMUL(75),CharHeight,LinksDiag,(HMENU)IDC_LINKS_BROWSEMULTI,
    hInstance,NULL);
  EasyStr TargetName=GetFileNameFromPath(LinksTargetPath);
  char *dot=strrchr(TargetName,'.');
  if(dot) 
    *dot='\0';
  for(INT_PTR n=0;n<9;n++) //?
  {
    Wid=GetTextSize(Font,EasyStr("#")+(n+1)).Width;
    CreateWindow("Static",EasyStr("#")+(n+1),WS_CHILD|WS_VISIBLE,
      10,GUIMUL(74)+(int)n*GUIMUL(30),Wid,CharHeight,LinksDiag,(HMENU)IDS_STATIC,hInstance,NULL);
    Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Edit",(LPSTR)((n==0)?TargetName.Text:""),
      WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,Wid+15,GUIMUL(70)+(int)n*GUIMUL(30),
      GUIMUL(380)-(Wid+5),CharHeight,LinksDiag,(HMENU)(IDC_LINKS_EDITBASE+n*100),hInstance,NULL);
    SendMessage(Win,EM_LIMITTEXT,100,0);
  }
  CreateWindow("Button",T("OK"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,
    GUIMUL(200),GUIMUL(340),GUIMUL(90),CharHeight,LinksDiag,(HMENU)IDOK,hInstance,NULL);
  CreateWindow("Button",T("Cancel"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
    GUIMUL(300),GUIMUL(340),GUIMUL(90),CharHeight,LinksDiag,(HMENU)IDCANCEL,hInstance,NULL);
  SetWindowAndChildrensFont(LinksDiag,Font);
  Wid=(LONG)SendMessage(GetDlgItem(LinksDiag,IDC_LINKS_TARGET),WM_GETTEXTLENGTH,0,0);
  SendMessage(GetDlgItem(LinksDiag,IDC_LINKS_TARGET),EM_SETSEL,Wid,Wid);
  SendMessage(GetDlgItem(LinksDiag,IDC_LINKS_TARGET),EM_SCROLLCARET,0,0);
  Wid=(LONG)SendMessage(GetDlgItem(LinksDiag,IDC_LINKS_TARGETMULTI),WM_GETTEXTLENGTH,0,0);
  SendMessage(GetDlgItem(LinksDiag,IDC_LINKS_TARGETMULTI),EM_SETSEL,Wid,Wid);
  SendMessage(GetDlgItem(LinksDiag,IDC_LINKS_TARGETMULTI),EM_SCROLLCARET,0,0);
  DiagFocus=GetDlgItem(LinksDiag,IDC_LINKS_EDITBASE);
  SendMessage(DiagFocus,EM_SETSEL,0,-1);
  SendMessage(DiagFocus,EM_SCROLLCARET,0,0);
  CentreWindow(LinksDiag,false);
  ShowWindow(LinksDiag,SW_SHOW);
}


#ifndef SSE_NO_WINSTON_IMPORT

void TDiskManager::ShowImportDiag()
{
  ImportDiag=CreateWindowEx(WS_EX_CONTROLPARENT,"Steem Disk Manager Dialog",T("Import WinSTon Favourites"),WS_CAPTION,
                           100,100,406,196+GuiSM.cy_caption(),
                           Handle,NULL,hInstance,NULL);
  if (ImportDiag==NULL || IsWindow(ImportDiag)==0){
    return;
  }
  EnableWindow(Handle,FALSE);

  SetWindowLongPtr(ImportDiag,GWLP_USERDATA,(LONG_PTR)this);

  if (FullScreen) SetParent(ImportDiag,StemWin);

  long Wid;
  HWND Win;

  Wid=GetTextSize(Font,T("WinSTon folder")).Width;
  CreateWindow("Static",T("WinSTon folder"),WS_CHILD | WS_VISIBLE,
                          10,14,Wid,CharHeight,ImportDiag,(HMENU)100,hInstance,NULL);

  Win=CreateWindowEx(512,"Edit",WinSTonPath,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                Wid+15,10,300-(Wid+5),CharHeight,ImportDiag,(HMENU)101,hInstance,NULL);
  SendMessage(Win,EM_LIMITTEXT,MAX_PATH,0);

  CreateWindow("Button",T("Browse"),WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_CHECKBOX | BS_PUSHLIKE,
                    315,10,75,CharHeight,ImportDiag,(HMENU)102,hInstance,NULL);


  Wid=GetTextSize(Font,T("WinSTon discs folder")).Width;
  CreateWindow("Static",T("WinSTon discs folder"),WS_CHILD | WS_VISIBLE,
                          10,44,Wid,CharHeight,ImportDiag,(HMENU)150,hInstance,NULL);

  Win=CreateWindowEx(512,"Edit",WinSTonDiskPath,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                Wid+15,40,300-(Wid+5),CharHeight,ImportDiag,(HMENU)151,hInstance,NULL);
  SendMessage(Win,EM_LIMITTEXT,MAX_PATH,0);

  CreateWindow("Button",T("Browse"),WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_CHECKBOX | BS_PUSHLIKE,
                    315,40,75,CharHeight,ImportDiag,(HMENU)152,hInstance,NULL);


  Wid=GetTextSize(Font,T("Import to")).Width;
  CreateWindow("Static",T("Import to"),WS_CHILD | WS_VISIBLE,
                          10,74,Wid,CharHeight,ImportDiag,(HMENU)200,hInstance,NULL);

  if (ImportPath.IsEmpty()) ImportPath=HomeFol+"\\"+T("Favourites");
  Win=CreateWindowEx(512,"Edit",ImportPath,
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                Wid+15,70,300-(Wid+5),CharHeight,ImportDiag,(HMENU)201,hInstance,NULL);
  SendMessage(Win,EM_LIMITTEXT,MAX_PATH,0);

  CreateWindow("Button",T("Browse"),WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_CHECKBOX | BS_PUSHLIKE,
                    315,70,75,CharHeight,ImportDiag,(HMENU)202,hInstance,NULL);

  Wid=GetCheckBoxSize(Font,T("Only downloaded disks")).Width;  //If off create broken links!
  Win=CreateWindow("Button",T("Only downloaded disks"),WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                          10,100,Wid,CharHeight,ImportDiag,(HMENU)300,hInstance,NULL);
  SendMessage(Win,BM_SETCHECK,ImportOnlyIfExist,0);

  Wid=GetTextSize(Font,T("On name conflict")).Width;
  CreateWindow("Static",T("On name conflict"),WS_CHILD | WS_VISIBLE,
                          10,134,Wid,CharHeight,ImportDiag,(HMENU)301,hInstance,NULL);

  Win=CreateWindow("Combobox","",WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                          15+Wid,130,200,200,ImportDiag,(HMENU)302,hInstance,NULL);
  SendMessage(Win,CB_ADDSTRING,0,(LONG_PTR)CStrT("Skip"));
  SendMessage(Win,CB_ADDSTRING,0,(LONG_PTR)CStrT("Overwrite"));
  SendMessage(Win,CB_ADDSTRING,0,(LONG_PTR)CStrT("Rename new"));
  SendMessage(Win,CB_ADDSTRING,0,(LONG_PTR)CStrT("Rename existing"));
  SendMessage(Win,CB_SETCURSEL,ImportConflictAction,0);


  CreateWindow("Button",T("OK"),WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    200,160,90,CharHeight,ImportDiag,(HMENU)IDOK,hInstance,NULL);

  CreateWindow("Button",T("Cancel"),WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    300,160,90,CharHeight,ImportDiag,(HMENU)IDCANCEL,hInstance,NULL);

  CreateWindow(PROGRESS_CLASS,"",WS_CHILD | PBS_SMOOTH,10,160,280,CharHeight,ImportDiag,(HMENU)400,hInstance,NULL);


  SetWindowAndChildrensFont(ImportDiag,Font);

  Wid=SendMessage(GetDlgItem(ImportDiag,101),WM_GETTEXTLENGTH,0,0);
  SendMessage(GetDlgItem(ImportDiag,101),EM_SETSEL,Wid,Wid);
  SendMessage(GetDlgItem(ImportDiag,101),EM_SCROLLCARET,0,0);

  Wid=SendMessage(GetDlgItem(ImportDiag,201),WM_GETTEXTLENGTH,0,0);
  SendMessage(GetDlgItem(ImportDiag,201),EM_SETSEL,Wid,Wid);
  SendMessage(GetDlgItem(ImportDiag,201),EM_SCROLLCARET,0,0);

  CentreWindow(ImportDiag,0);
  DiagFocus=GetDlgItem(ImportDiag,IDOK);
  ShowWindow(ImportDiag,SW_SHOW);
}

#endif


void TDiskManager::ShowPropDiag() {
#if USE_PASTI
  if(hPasti)
  {
    // sl will contain all pasti disks in the archive (if PropInf.Path is an
    //archive)
    EasyStringList sl(eslNoSort);
    if(FileIsDisk(PropInf.Path)==DISK_COMPRESSED)
    {
      // disks_sl will contain all disks (pasti and non-pasti) in the archive
      EasyStringList disks_sl(eslNoSort);
      zippy.list_contents(PropInf.Path,&disks_sl,true);
      for(int i=0;i<disks_sl.NumStrings;i++)
      {
        if(FileIsDisk(disks_sl[i].String)==DISK_PASTI)
        {
          // have to pass correct name to pasti
          Str temp_out=TempPath+SLASH+GetFileNameFromPath(disks_sl[i].String);
          sl.Add(temp_out);
          zippy.extract_file(PropInf.Path,(int)disks_sl[i].Data[0],temp_out,0);
        }
      }
    }
    if(sl.NumStrings||FileIsDisk(PropInf.Path)==DISK_PASTI)
    {
      // pass null-term list, disks first, followed by archive 
      char buf[8192],*p;
      ZeroMemory(buf,sizeof(buf));
      p=buf;
      for(int i=0;i<sl.NumStrings;i++)
      {
        strcpy(p,sl[i].String);
        p+=strlen(p)+1;
      }
      strcpy(p,PropInf.Path);
      pasti->DlgFileProps(Handle,buf);
      // clean up unwanted files
      for(int i=0;i<sl.NumStrings;i++) 
        DeleteFile(sl[i].String);
      return;
    }
  }
  else
  {
#if !defined(SSE_DISK_STX)
    if(has_extension(PropInf.Path,dot_ext(EXT_STX))) 
      return;
#endif
  }
#endif
  PropDiag=CreateWindowEx(WS_EX_CONTROLPARENT,"Steem Disk Manager Dialog",
    T("Disk Properties"), WS_CAPTION|WS_SYSMENU,100,100,GUIMUL(100)
    +GuiSM.cx_frame(),GUIMUL(199),Handle,NULL,hInstance,NULL);
#ifndef SSE_LEAN_AND_MEAN
  if(PropDiag==NULL||!IsWindow(PropDiag))
    return;
#endif
  EnableWindow(Handle,FALSE);
  SetWindowLongPtr(PropDiag,GWLP_USERDATA,(LONG_PTR)this);
  if(FullScreen) 
    SetParent(PropDiag,StemWin);
  HWND Win;
  int y=10;
  LONG Wid=GetTextSize(Font,T("Disk path")).Width;
  CreateWindow("Static",T("Disk path"),WS_CHILD|WS_VISIBLE,
    10,y,Wid,CharHeight,PropDiag,(HMENU)IDS_PROP_GROUP,hInstance,NULL);
  CreateWindowEx(WS_EX_CLIENTEDGE,"Edit",PropInf.Path,
    WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL|ES_READONLY,
    Wid+15,y,GUIMUL(290)-(Wid+15),CharHeight,PropDiag,(HMENU)IDC_PROP_PATH,hInstance,NULL);
  y+=LineHeight;
  if(PropInf.LinkPath.NotEmpty())
  {
    Wid=GetTextSize(Font,T("Shortcut path")).Width;
    CreateWindow("Static",T("Shortcut path"),WS_CHILD|WS_VISIBLE,
      10,y,Wid,CharHeight,PropDiag,(HMENU)(IDC_PROP_LINKPATH-1),hInstance,NULL);
    CreateWindowEx(WS_EX_CLIENTEDGE,"Edit",PropInf.LinkPath,
      WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL|ES_READONLY,
      Wid+15,y,GUIMUL(290)-(Wid+15),CharHeight,PropDiag,(HMENU)IDC_PROP_LINKPATH,hInstance,NULL);
    y+=LineHeight;
  }
  if(!has_extension(PropInf.Path,dot_ext(EXT_STT))
      && !has_extension(PropInf.Path,dot_ext(EXT_CTR))
      && !has_extension(PropInf.Path,dot_ext(EXT_IPF))
      && !has_extension(PropInf.Path,dot_ext(EXT_SCP))
      && !has_extension(PropInf.Path,dot_ext(EXT_HFE))
      && !has_extension(PropInf.Path,dot_ext(EXT_STW)))
  {
    if(FileIsDisk(PropInf.Path)==DISK_COMPRESSED)
    {
      TWidthHeight wh=GetTextSize(Font,T("Contents"));
      CreateWindow("Static",T("Contents"),WS_CHILD|WS_VISIBLE,
        10,y,wh.Width,wh.Height,PropDiag,(HMENU)IDS_STATIC,hInstance,NULL);
      y+=wh.Height;
      Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Listbox","",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|LBS_NOINTEGRALHEIGHT|LBS_NOTIFY,
        10,y,GUIMUL(280),GUIMUL(50),PropDiag,(HMENU)IDC_PROP_LIST,hInstance,NULL);
      SendMessage(Win,WM_SETFONT,(WPARAM)Font,0);
      y+=GUIMUL(60);
      EasyStringList esl;
      esl.Sort=eslSortByNameI;
      zippy.list_contents(PropInf.Path,&esl,false);
      for(int i=0;i<esl.NumStrings;i++) // images in archive
      {
        LRESULT idx=SendMessage(Win,LB_ADDSTRING,0,(LPARAM)esl[i].String);
        ASSERTB(idx!=LB_ERR && idx!=LB_ERRSPACE);
        SendMessage(Win,LB_SETITEMDATA,idx,esl[i].Data[0]);
      }
    }
    CreateWindow("Button",T("Disk Parameters"),WS_CHILD|WS_VISIBLE|BS_GROUPBOX,
      10,y,GUIMUL(280),GUIMUL(215-10),PropDiag,(HMENU)IDS_PROP_GROUP,hInstance,NULL);
    CreateWindowEx(WS_EX_CLIENTEDGE,"Edit","",WS_CHILD|WS_VISIBLE|WS_TABSTOP|
      ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL|ES_READONLY,
      10,y,GUIMUL(280),GUIMUL(215),PropDiag,(HMENU)IDC_PROP_CONTENT,hInstance,NULL);
    y+=LineHeight;
    CreateWindow("Static","",WS_CHILD|WS_VISIBLE,
      20,y,GUIMUL(190),CharHeight,PropDiag,(HMENU)IDC_PROP_BPB,hInstance,NULL);
    y+=LineHeight;
    CreateWindow("Static","",WS_CHILD|WS_VISIBLE,
      20,y,GUIMUL(260),CharHeight,PropDiag,(HMENU)IDC_PROP_DATABYTES,hInstance,NULL);
    y+=LineHeight;
    Wid=GetTextSize(Font,T("Sides")).Width;
    CreateWindow("Static",T("Sides"),WS_CHILD|WS_VISIBLE,20,y,Wid,CharHeight,PropDiag,
      (HMENU)(IDC_PROP_SIDES-1),hInstance,NULL);
    Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Combobox","",
      WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST,
      GUIMUL(200),y,GUIMUL(80),GUIMUL(200),PropDiag,(HMENU)IDC_PROP_SIDES,hInstance,NULL);
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"1");
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)"2");
    y+=LineHeight;
    Wid=GetTextSize(Font,T("Tracks per side")).Width;
    CreateWindow("Static",T("Tracks per side"),WS_CHILD|WS_VISIBLE,
      20,y,Wid,CharHeight,PropDiag,(HMENU)(IDC_PROP_TRACKS-1),hInstance,NULL);
    Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Combobox","",
      WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,
      GUIMUL(200),y,GUIMUL(80),GUIMUL(300),PropDiag,(HMENU)IDC_PROP_TRACKS,hInstance,NULL);
    char c[16];
    for(int i=10;i<=FLOPPY_MAX_TRACK_NUM+1;i++)
    {
      sprintf(c,"%d",i);
      SendMessage(Win,CB_ADDSTRING,0,(LPARAM)c);
    }
    y+=LineHeight;
    Wid=GetTextSize(Font,T("Sectors per track")).Width;
    CreateWindow("Static",T("Sectors per track"),WS_CHILD|WS_VISIBLE,
      20,y,Wid,CharHeight,PropDiag,(HMENU)(IDC_PROP_SECTORS-1),hInstance,NULL);
    Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Combobox","",WS_CHILD|WS_VISIBLE|WS_TABSTOP
      |CBS_DROPDOWNLIST|WS_VSCROLL,GUIMUL(200),y,GUIMUL(80),GUIMUL(300),PropDiag,
      (HMENU)IDC_PROP_SECTORS,hInstance,NULL);
    for(int i=3;i<=FLOPPY_MAX_SECTOR_NUM;i++)
    {
      sprintf(c,"%d",i);
      SendMessage(Win,CB_ADDSTRING,0,(LPARAM)c);
    }
    y+=LineHeight;
    Wid=GetTextSize(Font,T("Bytes per sector")).Width;
    CreateWindow("Static",T("Bytes per sector"),WS_CHILD|WS_VISIBLE,
      20,y,Wid,CharHeight,PropDiag,(HMENU)IDS_STATIC,hInstance,NULL);
    Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Combobox","",WS_CHILD|WS_VISIBLE|WS_TABSTOP
      |CBS_DROPDOWNLIST,GUIMUL(200),y,GUIMUL(80),GUIMUL(200),PropDiag,
      (HMENU)IDC_PROP_BYTES,hInstance,NULL);
    TWD1772IDField w;
    for(BYTE i=0;i<4;i++)
    {
      w.len=i;
      sprintf(c,"%d",w.nBytes());
      SendMessage(Win,CB_ADDSTRING,0,(LPARAM)c);
    }
    y+=LineHeight+GuiSM.cy_frame();
    CreateWindow("Button",T("Auto Detect"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHLIKE
      |BS_CHECKBOX,20,y,GUIMUL(125),CharHeight,PropDiag,(HMENU)IDC_PROP_DETECT,hInstance,NULL);
    CreateWindow("Button",T("Apply Changes"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHLIKE
      |BS_CHECKBOX|WS_DISABLED,GUIMUL(155),y,GUIMUL(125),CharHeight,PropDiag,
      (HMENU)IDC_PROP_APPLY,hInstance,NULL);
    y+=GUIMUL(50);
  }
  else // just  give size in bytes
  {
    FILE *fp=fopen(PropInf.Path,"rb");
    if(fp)
    {
      CreateWindow("Static",T("Size in bytes")+": "+(int)GetFileLength(fp),WS_CHILD
        |WS_VISIBLE,10,y,GUIMUL(280),CharHeight,PropDiag,(HMENU)IDS_PROP_SIZE,hInstance,NULL);
      fclose(fp);
      y+=LineHeight;
    }
    else
      y+=5;
  }
  SetWindowPos(PropDiag,NULL,0,0,GUIMUL(306)+GuiSM.cx_frame(),y+GuiSM.cy_caption()
    +GuiSM.cy_frame(),SWP_NOZORDER|SWP_NOMOVE);
  PropShowFileInfo(0);
  SetWindowAndChildrensFont(PropDiag,Font);
  DiagFocus=GetDlgItem(PropDiag,IDC_PROP_PATH);
  Wid=(int)SendMessage(DiagFocus,WM_GETTEXTLENGTH,0,0);
  SendMessage(DiagFocus,EM_SETSEL,0,Wid);
  SendMessage(DiagFocus,EM_SCROLLCARET,0,0);
  if(PropInf.LinkPath.NotEmpty())
  {
    HWND hLinkPath=GetDlgItem(PropDiag,IDC_PROP_LINKPATH);
    Wid=(int)SendMessage(hLinkPath,WM_GETTEXTLENGTH,0,0);
    SendMessage(hLinkPath,EM_SETSEL,0,Wid);
    SendMessage(hLinkPath,EM_SCROLLCARET,0,0);
  }
  CentreWindow(PropDiag,false);
  //DiagFocus=GetDlgItem(PropDiag,IDC_PROP_PATH);
  ShowWindow(PropDiag,SW_SHOW);
  SetFocus(DiagFocus);
}


void TDiskManager::PropShowFileInfo(int i) {
  EasyStr FileInZip;
  DWORD FileHOffset=0;
  HWND hContent=GetDlgItem(PropDiag,IDC_PROP_CONTENT);
  if(FileIsDisk(PropInf.Path)==DISK_COMPRESSED)
  {
    FileInZip.SetLength(SSE_MAX_PATH);
    SendDlgItemMessage(PropDiag,IDC_PROP_LIST,LB_GETTEXT,i,(LPARAM)FileInZip.Text);
    FileHOffset=(int)SendDlgItemMessage(PropDiag,IDC_PROP_LIST,LB_GETITEMDATA,i,0);
  }
  if(FileInZip.Empty()||FileIsDisk(FileInZip))
  {
    HWND hDatabytes=GetDlgItem(PropDiag,IDC_PROP_DATABYTES);
    ShowWindow(hContent,SW_HIDE);
    for(int j=IDS_PROP_GROUP;j<IDC_PROP_CONTENT;j++)
      if(GetDlgItem(PropDiag,j)) //!
        ShowWindow(GetDlgItem(PropDiag,j),SW_SHOW);
    TSF314 &TempDrive=FloppyDrive[2];
    TFloppyDisk &TempDisk=FloppyDisk[2];
    Str ErrMsg;
    Str DiskPath=PropInf.Path;
    if(FileInZip.NotEmpty()) 
      DiskPath=FileInZip;
    if(!has_extension(DiskPath,dot_ext(EXT_STT)) && FileIsDisk(DiskPath)!=DISK_PASTI
      && !has_extension(DiskPath,dot_ext(EXT_CTR))
      && !has_extension(DiskPath,dot_ext(EXT_IPF))
      && !has_extension(DiskPath,dot_ext(EXT_HFE))
      && !has_extension(DiskPath,dot_ext(EXT_STW))
      && !has_extension(DiskPath,dot_ext(EXT_SCP)))
    {
      // bpbi is what Steem detects the BPB should be (not including the .steembpb file)
      // file_bpbi is what the raw BPB from the disk is
      if(TempDrive.SetDisk(PropInf.Path,FileInZip,&bpbi,&file_bpbi)!=FIMAGE_OK)
        ErrMsg=T("No BPB information");
    }
    else
      ErrMsg=T("No BPB information");
    if(ErrMsg.Empty())
    {
      final_bpbi.BytesPerSector=TempDisk.BytesPerSector;
      final_bpbi.Sectors=TempDisk.SectorsPerTrack*TempDisk.TracksPerSide*TempDisk.Sides;
      final_bpbi.SectorsPerTrack=TempDisk.SectorsPerTrack;
      final_bpbi.Sides=TempDisk.Sides;
      BOOL can_edit=(has_extension(DiskPath,dot_ext(EXT_STW))==0);
      for(int j=IDS_PROP_GROUP;j<IDC_PROP_CONTENT;j++)
        if(GetDlgItem(PropDiag,j)) 
          EnableWindow(GetDlgItem(PropDiag,j),can_edit);
      EnableWindow(GetDlgItem(PropDiag,IDC_PROP_APPLY),FALSE);
      EasyStr StrValBPB=T("BPB is valid");
      if(!TempDisk.ValidBPB)
      {
        int TracksPerSide=0;
        if(file_bpbi.SectorsPerTrack>0&&file_bpbi.Sides>0&&file_bpbi.Sectors>0)
          TracksPerSide=(file_bpbi.Sectors/file_bpbi.SectorsPerTrack)/file_bpbi.Sides;
        StrValBPB=T("BPB is not valid")+" ("+file_bpbi.Sides+","+TracksPerSide
          +","+file_bpbi.SectorsPerTrack+","+file_bpbi.BytesPerSector+")";
      }
      SetWindowText(GetDlgItem(PropDiag,IDC_PROP_BPB),StrValBPB);
      char t1[64];
      InsertCommas(t1,TempDisk.DiskFileLen);
      char t2[128];
      sprintf(t2,"%s%s",T("Data bytes: ").Text,t1);
      SetWindowText(hDatabytes,t2);
      SetWindowLongPtr(hDatabytes,GWLP_USERDATA,(LONG_PTR)TempDisk.DiskFileLen);
      SendDlgItemMessage(PropDiag,IDC_PROP_SIDES,CB_SETCURSEL,TempDisk.Sides-1,0);
      SendDlgItemMessage(PropDiag,IDC_PROP_TRACKS,CB_SETCURSEL,TempDisk.TracksPerSide-10,0);
      SendDlgItemMessage(PropDiag,IDC_PROP_SECTORS,CB_SETCURSEL,TempDisk.SectorsPerTrack-3,0);
      TWD1772IDField w;
      int len=w.GetLen(TempDisk.BytesPerSector);
      SendDlgItemMessage(PropDiag,IDC_PROP_BYTES,CB_SETCURSEL,len,0);
      TempDrive.RemoveDisk(true);
    }
    else
    {
      SetWindowText(GetDlgItem(PropDiag,IDC_PROP_BPB),ErrMsg);
      SetWindowText(hDatabytes,"");
      for(int j=(IDC_PROP_SIDES-1);j<IDC_PROP_CONTENT;j++)
        if(GetDlgItem(PropDiag,j)) 
          EnableWindow(GetDlgItem(PropDiag,j),FALSE);
    }
  }
  else
  {
    Str ZipTemp;
    ZipTemp.SetLength(SSE_MAX_PATH);
    GetTempFileName(TempPath,"ZIP",0,ZipTemp);
    if(zippy.extract_file(PropInf.Path,FileHOffset,ZipTemp)==ZIPPY_SUCCEED)
    {
      char* Text=new char[20001];
      FILE *fp=fopen(ZipTemp,"rb");
      int Len=(int)FREAD(Text,1,20000,fp); // 20000 max
      Text[Len]='\0';
      fclose(fp);
      SetWindowText(hContent,Text);
      delete[] Text;
    }
    DeleteFile(ZipTemp);
    for(int j=(IDC_PROP_BPB-1);j<IDC_PROP_CONTENT;j++) 
      if(GetDlgItem(PropDiag,j)) 
        ShowWindow(GetDlgItem(PropDiag,j),SW_HIDE);
    ShowWindow(hContent,SW_SHOW);
  }
}


#if !defined(SSE_LIBRETRONUKE)
LRESULT CALLBACK TDiskManager::Dialog_WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar) {
  WORD wpar_lo=LOWORD(wPar);
  WORD wpar_hi=HIWORD(wPar);
  TDiskManager *This=(TDiskManager*)GetWindowLongPtr(Win,GWLP_USERDATA);
  if(This==NULL)
  {
    if(Mess==WM_CREATE)
      SetClassLongPtr(Win,GCLP_HICON,(LONG_PTR)(hGUIIconSmall[RC_ICO_DRIVE]));
    return DefWindowProc(Win,Mess,wPar,lPar);
  }
  if(Win==This->DatabaseDiag) // TODO not so nice C++
  {
    switch(Mess) {
    case WM_COMMAND:
      switch(wpar_lo) {
      case IDOK:
      {
        Str Find=GetWindowTextStr(GetDlgItem(Win,IDC_DB_SEARCH));
        This->DatabaseFind=Find; // remember
        /*if(Find.Empty())
        {
          MessageBeep(0);
          break;
        }*/
        char* buf=new char[65536],*p=buf;
        ZeroMemory(buf,65536);
        if(!Find.Empty())
          GetContents_SearchDatabase(Find,buf,65536);
        int n_items=0;
        while(p[0])
        {
          p+=strlen(p)+1;
          while(p[0])
            p+=strlen(p)+1;
          p++;
          n_items++;
        }
        if(n_items==0)
        {
          strcpy(p,T("No Record").Text);
          strcpy(p+strlen(p)+1,T("No Record").Text);
          //MessageBeep(0);
          //break;        }
        }
        SendDlgItemMessage(Win,IDC_DB_LIST,LVM_DELETEALLITEMS,0,0);
        SendDlgItemMessage(Win,IDC_DB_LIST,LVM_SETITEMCOUNT,n_items,0);
        SendDlgItemMessage(Win,IDC_DB_LIST,WM_SETREDRAW,FALSE,0);
        int i=0;
        LV_ITEM lvi;
        lvi.mask=LVIF_TEXT;
        Str Contents;
        p=buf;
        while(p[0])
        {
          lvi.iItem=i;
          lvi.iSubItem=0;
          lvi.pszText=p;
          SendDlgItemMessage(Win,IDC_DB_LIST,LVM_INSERTITEM,0,(LPARAM)&lvi);
          p+=strlen(p)+1;
          Contents="";
          while(p[0])
          {
            if(Contents[0])
              Contents+=", ";
            Contents+=p;
            p+=strlen(p)+1;
          }
          lvi.iSubItem=1;
          lvi.pszText=Contents;
          SendDlgItemMessage(Win,IDC_DB_LIST,LVM_SETITEM,0,(LPARAM)&lvi);
          p++; // skip content list null
          i++;
        }
        SendDlgItemMessage(Win,IDC_DB_LIST,LVM_SETCOLUMNWIDTH,0,LVSCW_AUTOSIZE);
        SendDlgItemMessage(Win,IDC_DB_LIST,LVM_SETCOLUMNWIDTH,1,LVSCW_AUTOSIZE);
        SendDlgItemMessage(Win,IDC_DB_LIST,WM_SETREDRAW,TRUE,0);
        delete[] buf;
        break;
      }
      case IDCANCEL:
        SetForegroundWindow(This->Handle);
        //EnableWindow(This->Handle,TRUE);
        DestroyWindow(Win);
        return 0;
      }
      break;
    case WM_CLOSE:
      SetForegroundWindow(This->Handle);
      //EnableWindow(This->Handle,TRUE);
      break;
    case WM_DESTROY:
      This->DatabaseDiag=NULL;
      break;
    }
  }
  else if(Win==This->ContentDiag)
  {
    switch(Mess) {
    case WM_COMMAND:
      switch(wpar_lo) {
      case IDOK:
      {
        Str DestFol=GetWindowTextStr(GetDlgItem(Win,IDC_CONTENT_DESTFOLDER));
        NO_SLASH(DestFol);
        Str NewLink;
        Str DiskName;
        if(SendDlgItemMessage(Win,IDC_CONTENT_APPENDNAME,BM_GETCHECK,BST_CHECKED,0)==BST_CHECKED)
          DiskName=Str(" (")+GetWindowTextStr(GetDlgItem(Win,IDC_CONTENT_SHORTNAME))+")";
        CreateDirectory(DestFol,NULL);
        if(GetFileAttributes(DestFol)==INVALID_FILE_ATTRIBUTES)
        {
          Alert(T("Invalid directory"),T("ERROR"),0);
          break;
        }
        for(int i=2;i<This->contents_sl.NumStrings;i++)
        {
          LV_ITEM lvi;
          lvi.iItem=i-2;
          lvi.iSubItem=0;
          lvi.mask=LVIF_STATE;
          lvi.stateMask=LVIS_STATEIMAGEMASK;
          SendDlgItemMessage(Win,IDC_CONTENT_LIST,LVM_GETITEM,0,(LPARAM)&lvi);
          if(lvi.state & LVI_SI_CHECKED)
          {
            NewLink=DestFol+SLASH+Str(This->contents_sl[i].String)+DiskName+".lnk";
            if(Exists(NewLink))
            {
              if(This->ContentConflictAction==0)
                NewLink="";
              else if(This->ContentConflictAction==2)
                NewLink=GetUniquePath(DestFol,Str(This->contents_sl[i].String)+DiskName+".lnk");
            }
            if(NewLink.NotEmpty())
              CreateLink(NewLink,This->contents_sl[0].String);
          }
        }
        if(IsSameStr_I(DestFol,This->DisksFol))
          This->RefreshDiskView("",false,NewLink);
      }
      case IDCANCEL:
        EnableWindow(This->Handle,TRUE);
        SetForegroundWindow(This->Handle);
        DestroyWindow(Win);
        return 0;
      case IDC_CONTENT_BROWSE:
      {
        SendMessage((HWND)lPar,BM_SETCHECK,1,0);
        HWND Edit=GetDlgItem(Win,IDC_CONTENT_DESTFOLDER);
        EnableAllWindows(false,Win);
        Str CurText=GetWindowTextStr(Edit);
        NO_SLASH(CurText);
        EasyStr NewFol=ChooseFolder((FullScreen) ? StemWin : Win,T("Pick a Folder"),CurText);
        if(NewFol.NotEmpty())
          SendMessage(Edit,WM_SETTEXT,0,(LPARAM)NewFol.Text);
        SetForegroundWindow(Win);
        EnableAllWindows(true,Win);
        SetFocus((HWND)lPar);
        SendMessage((HWND)lPar,BM_SETCHECK,0,0);
        break;
      }
      case IDC_CONTENT_ONCONFLICT:
        if(wpar_hi==CBN_SELENDOK)
          This->ContentConflictAction=(int)SendMessage((HWND)lPar,CB_GETCURSEL,0,0);
        break;
      }
      break;
    case WM_CLOSE:
      SendMessage(Win,WM_COMMAND,IDCANCEL,0);
      break;
    case WM_DESTROY:
      This->ContentDiag=NULL;
      This->contents_sl.DeleteAll();
      break;
    }
  }
  else if(Win==This->DiskDiag)
  {
    switch(Mess) {
    case WM_COMMAND:
      switch(wpar_lo) {
      case IDC_CREATE_EXT_ST:
      case IDC_CREATE_EXT_MSA:
      case IDC_CREATE_EXT_DIM:
        if(wpar_hi==BN_CLICKED)
          if(SendMessage((HWND)lPar,BM_GETCHECK,0,0)==BST_CHECKED)
            This->DiskImageType=(BYTE)(wpar_lo-IDC_CREATE_EXT_ST+1); // used by CreateDiskImage()
        break;
      case IDCANCEL:
      case IDOK:
      {
        EasyStr STName=This->DisksFol+SLASH+T("Blank Disk.")+extension_list[This->DiskImageType];
        if(wpar_lo==IDOK)
        {
          int n=2;
          while(Exists(STName))
            STName=This->DisksFol+SLASH+T("Blank Disk")+" ("+(n++)+")."
                   + extension_list[This->DiskImageType];
          char buf[12];
          SendDlgItemMessage(Win,IDC_CREATE_SIDES,WM_GETTEXT,2,(LPARAM)buf);
          WORD Sides=(WORD)atoi(buf);
          This->SidesIdx=Sides-1;
          SendDlgItemMessage(Win,IDC_CREATE_SECTORS,WM_GETTEXT,3,(LPARAM)buf);
          This->SecsPerTrackIdx=(WORD)atoi(buf);
          SendDlgItemMessage(Win,IDC_CREATE_TRACKS,WM_GETTEXT,3,(LPARAM)buf);
          This->TracksIdx=(WORD)atoi(buf);
          WORD Sectors=(WORD)(Sides*(This->TracksIdx)*(This->SecsPerTrackIdx));
          if(!This->CreateDiskImage(STName,Sectors,This->SecsPerTrackIdx,Sides))
          {
            Alert(EasyStr(T("Could not create the disk image"))+" "+STName,
              T("ERROR"),MB_ICONEXCLAMATION);
            return 0;
          }
        }
        EnableWindow(This->Handle,TRUE);
        SetForegroundWindow(This->Handle);
        DestroyWindow(Win);
        if(wpar_lo==IDOK)
          This->RefreshDiskView(STName,true);
        return 0;
      }
      case IDC_CREATE_SIDES:case IDC_CREATE_SECTORS:case IDC_CREATE_TRACKS:
        if(wpar_hi==EN_CHANGE)
        {
          int databytes=This->GetDiskSelectionSize();
          char s[64];
          sprintf(s,"%s: %d %s",T("Disk size").Text,databytes>>10,T(" KB").Text);
          SendDlgItemMessage(Win,IDS_CREATE_SIZE,WM_SETTEXT,0,(LPARAM)&s);
        }
        break;
      }
      break;
    case WM_DESTROY:
      This->DiskDiag=NULL;
      EnableWindow(This->Handle,TRUE);
      break;
    case WM_CLOSE:
      SendMessage(Win,WM_COMMAND,IDCANCEL,0);
      return 0;
    }
  }
  else if(Win==This->LinksDiag)
  {
    switch(Mess) {
    case WM_COMMAND:
      switch(wpar_lo) {
      case IDOK:
        if(!This->DoCreateMultiLinks())
          break;
      case IDCANCEL:
        EnableWindow(This->Handle,TRUE);
        SetForegroundWindow(This->Handle);
        DestroyWindow(Win);
        return 0;
      case IDC_LINKS_BROWSE:
      case IDC_LINKS_BROWSEMULTI:
        SendMessage((HWND)lPar,BM_SETCHECK,1,0);
#if defined(SSE_LONG_PATH)
        EasyStr sCurText;
        sCurText.SetLength(SSE_MAX_PATH);
        char *CurText=sCurText.Text;
#else
        char CurText[MAX_PATH+1];
#endif
        HWND Edit=GetDlgItem(Win,wpar_lo-1);
        EnableAllWindows(false,Win);
        SendMessage(Edit,WM_GETTEXT,SSE_MAX_PATH,(LPARAM)CurText);
        NO_SLASH(CurText);
        if(wpar_lo==IDC_LINKS_BROWSEMULTI)
        {
          EasyStr NewFol=ChooseFolder((FullScreen) ? StemWin : Win,T("Pick a Folder"),CurText);
          if(NewFol.NotEmpty())
            SendMessage(Edit,WM_SETTEXT,0,(LPARAM)NewFol.Text);
        }
        else
        {
          EasyStr CurFol=CurText;
          char *CurDiskName=GetFileNameFromPath(CurFol);
          if(CurDiskName>CurFol.Text)
            *(CurDiskName-1)='\0';
          char *fstypes=FSTypes(2,NULL);
          EasyStr Target=FileSelect((FullScreen) ? StemWin : Win,T("Select Shortcut Target"),
            CurFol,fstypes,1,true,"st",CurDiskName);
          free(fstypes);
          if(Target.NotEmpty())
            SendMessage(Edit,WM_SETTEXT,0,(LPARAM)Target.Text);
        }
        SetForegroundWindow(Win);
        EnableAllWindows(true,Win);
        SetFocus((HWND)lPar);
        SendMessage((HWND)lPar,BM_SETCHECK,0,0);
        break;
      }
      break;
    case WM_CLOSE:
      SendMessage(Win,WM_COMMAND,IDCANCEL,0);
      return 0;
    case WM_DESTROY:
      This->LinksDiag=NULL;
      EnableWindow(This->Handle,TRUE);
      break;
    }
  }
#if !defined(SSE_NO_WINSTON_IMPORT)
  else if(Win==This->ImportDiag)
  {
    switch(Mess) {
    case WM_COMMAND:
      switch(wpar_lo) {
      case IDOK:
        if(This->DoImport()==0) break;
      case IDCANCEL:
        if(This->Importing)
          This->Importing=0;
        else
        {
          EnableWindow(This->Handle,TRUE);
          SetForegroundWindow(This->Handle);
          DestroyWindow(Win);
        }
        return 0;
      case 102:
      case 152:
      case 202:
      {
        SendMessage((HWND)lPar,BM_SETCHECK,1,true);
        char CurFol[MAX_PATH+1];
        HWND Edit=GetDlgItem(Win,wpar_lo-1);
        EnableAllWindows(false,Win);
        SendMessage(Edit,WM_GETTEXT,MAX_PATH,(LONG_PTR)CurFol);
        NO_SLASH(CurFol);
        EasyStr NewFol=ChooseFolder((FullScreen) ? StemWin : Win,T("Pick a Folder"),CurFol);
        if(NewFol.NotEmpty())
          SendMessage(Edit,WM_SETTEXT,0,(LONG_PTR)NewFol.Text);
        if(wpar_lo==102)
          SendDlgItemMessage(Win,151,WM_SETTEXT,0,(LPARAM)((NewFol+"\\Discs").Text));
        SetForegroundWindow(Win);
        EnableAllWindows(true,Win);
        SetFocus((HWND)lPar);
        SendMessage((HWND)lPar,BM_SETCHECK,0,true);
        break;
      }
      }
      break;
    case WM_CLOSE:
      SendMessage(Win,WM_COMMAND,IDCANCEL,0);
      return 0;
    case WM_DESTROY:
      This->ImportDiag=NULL;
      EnableWindow(This->Handle,TRUE);
      break;
    }
  }
#endif
  else if(Win==This->PropDiag)
  {
    switch(Mess) {
    case WM_COMMAND:
      switch(wpar_lo) {
      case IDC_PROP_LIST:
        if(wpar_hi==LBN_SELCHANGE)
          This->PropShowFileInfo((int)SendMessage((HWND)lPar,LB_GETCURSEL,0,0));
        break;
      case IDC_PROP_SIDES:case IDC_PROP_TRACKS:case IDC_PROP_SECTORS:case IDC_PROP_BYTES:
        if(wpar_hi==CBN_SELENDOK)
        {
          int TracksPerSide=0;
          if(This->final_bpbi.SectorsPerTrack>0&&This->final_bpbi.Sides>0
            &&This->final_bpbi.Sectors>0)
            TracksPerSide=(This->final_bpbi.Sectors/
              This->final_bpbi.SectorsPerTrack)/This->final_bpbi.Sides;
          BOOL Enable=((SendDlgItemMessage(Win,IDC_PROP_SIDES,CB_GETCURSEL,0,0)+1)
            !=This->final_bpbi.Sides)
            |((SendDlgItemMessage(Win,IDC_PROP_TRACKS,CB_GETCURSEL,0,0)+10)!=TracksPerSide)
            |((SendDlgItemMessage(Win,IDC_PROP_SECTORS,CB_GETCURSEL,0,0)+3)
              !=This->final_bpbi.SectorsPerTrack)
            |((128<<SendDlgItemMessage(Win,IDC_PROP_BYTES,CB_GETCURSEL,0,0))
            !=This->final_bpbi.BytesPerSector);
          EnableWindow(GetDlgItem(This->PropDiag,IDC_PROP_APPLY),Enable);
        }
        break;
      case IDC_PROP_APPLY:
        if(wpar_hi==BN_CLICKED)
        {
          int nFile=0;
          EasyStr DiskInZip;
          if(FileIsDisk(This->PropInf.Path)==DISK_COMPRESSED)
          {
            nFile=(int)SendDlgItemMessage(Win,IDC_PROP_LIST,LB_GETCURSEL,0,0);
            DiskInZip.SetLength(SSE_MAX_PATH);
            SendDlgItemMessage(Win,IDC_PROP_LIST,LB_GETTEXT,nFile,(LPARAM)DiskInZip.Text);
          }
          EasyStr File=This->PropInf.Path+DiskInZip+".steembpb";
          int Sides=(int)SendDlgItemMessage(Win,IDC_PROP_SIDES,CB_GETCURSEL,0,0)+1;
          int TracksPerSide=(int)SendDlgItemMessage(Win,IDC_PROP_TRACKS,CB_GETCURSEL,0,0)+10;
          int SectorsPerTrack=(int)SendDlgItemMessage(Win,IDC_PROP_SECTORS,CB_GETCURSEL,0,0)+3;
          int BytesPerSector=128<<(int)SendDlgItemMessage(Win,IDC_PROP_BYTES,CB_GETCURSEL,0,0);
          int Sectors=Sides*TracksPerSide*SectorsPerTrack;
          int Ret=IDYES;
          if((Sectors*BytesPerSector)>GetWindowLongPtr(GetDlgItem(Win,IDC_PROP_DATABYTES),
            GWLP_USERDATA))
            Ret=Alert(T("This disk configuration is too big for the size of the\
 file, this could cause disk problems. Would you like to use it anyway?"),
              T("Use Configuration?"),MB_ICONQUESTION|MB_YESNO);
          if(Ret==IDYES)
          {
            TConfigStoreFile CSF(File);
            CSF.SetStr("BPB","Sides",Str(Sides));
            CSF.SetStr("BPB","SectorsPerTrack",Str(SectorsPerTrack));
            CSF.SetStr("BPB","BytesPerSector",Str(BytesPerSector));
            CSF.SetStr("BPB","Sectors",Str(Sectors));
            CSF.Close();
            This->PropShowFileInfo(nFile);
            for(int d=DRIVE_A;d<=DRIVE_B;d++)
            {
              if(IsSameStr_I(FloppyDrive[d].GetDisk(),This->PropInf.Path))
              {
                EasyStr DiskInZip2=FloppyDisk[d].DiskInZip;
                FloppyDrive[d].RemoveDisk();
                FloppyDrive[d].SetDisk(This->PropInf.Path,DiskInZip2);
              }
            }
          }
        }
        break;
      case IDC_PROP_DETECT:
        if(wpar_hi==BN_CLICKED)
        {
          int TracksPerSide=0;
          if(This->bpbi.SectorsPerTrack>0&&This->bpbi.Sides>0&&This->bpbi.Sectors>0)
            TracksPerSide=(This->bpbi.Sectors/This->bpbi.SectorsPerTrack)/This->bpbi.Sides;
          SendDlgItemMessage(Win,IDC_PROP_SIDES,CB_SETCURSEL,This->bpbi.Sides-1,0);
          SendDlgItemMessage(Win,IDC_PROP_TRACKS,CB_SETCURSEL,TracksPerSide-10,0);
          SendDlgItemMessage(Win,IDC_PROP_SECTORS,CB_SETCURSEL,This->bpbi.SectorsPerTrack-3,0);
          TWD1772IDField w;
          int len=w.GetLen(This->bpbi.BytesPerSector);
          SendDlgItemMessage(Win,IDC_PROP_BYTES,CB_SETCURSEL,len,0);
          SendMessage(Win,WM_COMMAND,MAKEWPARAM(IDC_PROP_SIDES,CBN_SELENDOK),
            (LPARAM)GetDlgItem(Win,IDC_PROP_SIDES));
        }
        break;
      }
      break;
    case WM_CLOSE:
      This->PropDiag=NULL;
      SetForegroundWindow(This->Handle); // also if database
      EnableWindow(This->Handle,TRUE);
      break;
    }
  }
  switch(Mess) {
#if !defined(SSE_VID_2SCREENS) //TODO?
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
      RECT LimRC={0,MENUHEIGHT+GuiSM.cy_frame(),GuiSM.cx_screen(),GuiSM.cy_screen()};
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
      This->DiagFocus=GetFocus();
    break;
  case WM_SETFOCUS:
    SetFocus(This->DiagFocus);
    break;
  case DM_GETDEFID:
    return MAKELONG(IDOK,DC_HASDEFID);
  }
  return DefWindowProc(Win,Mess,wPar,lPar);
}
#endif//#if !defined(SSE_LIBRETRONUKE)

int TDiskManager::GetDiskSelectionSize() {
  char buf[12];
  SendDlgItemMessage(DiskDiag,IDC_CREATE_SIDES,WM_GETTEXT,2,(LPARAM)buf);
  WORD Sides=(WORD)atoi(buf);
  SendDlgItemMessage(DiskDiag,IDC_CREATE_TRACKS,WM_GETTEXT,3,(LPARAM)buf);
  WORD Tracks=(WORD)atoi(buf);
  SendDlgItemMessage(DiskDiag,IDC_CREATE_SECTORS,WM_GETTEXT,3,(LPARAM)buf);
  WORD Sectors=(WORD)atoi(buf);
  int bytes=Sides*Tracks*Sectors*SECTOR_SIZE;
  return bytes;
}


bool TDiskManager::DoCreateMultiLinks() {
  LinksTargetPath.SetLength(SSE_MAX_PATH);
  SendMessage(GetDlgItem(LinksDiag,IDC_LINKS_TARGET),WM_GETTEXT,MAX_PATH,
    (LPARAM)LinksTargetPath.Text);
  NO_SLASH(LinksTargetPath);
  if(LinksTargetPath[0]=='\0')
  {
    Alert(T("Please enter a file/folder to be the target for the shortcuts."),
      T("Multiple Shortcuts Error"),MB_ICONEXCLAMATION);
    return false;
  }
  else if(GetFileAttributes(LinksTargetPath)==INVALID_FILE_ATTRIBUTES)
  {
    Alert(LinksTargetPath+" "+T("does not exist."),T("Multiple Shortcuts Error"),
      MB_ICONEXCLAMATION);
    return false;
  }
  MultipleLinksPath.SetLength(SSE_MAX_PATH);
  SendMessage(GetDlgItem(LinksDiag,IDC_LINKS_TARGETMULTI),WM_GETTEXT,SSE_MAX_PATH,
    (LPARAM)MultipleLinksPath.Text);
  NO_SLASH(MultipleLinksPath);
  if(MultipleLinksPath[0]=='\0')
  {
    Alert(T("Please enter a folder to create the shortcuts in."),
      T("Multiple Shortcuts Error"),MB_ICONEXCLAMATION);
    return false;
  }
  if(GetFileAttributes(MultipleLinksPath)==INVALID_FILE_ATTRIBUTES)
  {
    if(!CreateDirectory(MultipleLinksPath,NULL))
    {
      Alert(T("Couldn't create the folder to create the shortcuts in")+" "+
        MultipleLinksPath,T("Multiple Shortcuts Error"),MB_ICONEXCLAMATION);
      return false;
    }
  }
  EasyStr LinkFileName,Name;
  for(int n=0;n<9;n++)
  {
    LinkFileName=MultipleLinksPath+SLASH;
    Name.SetLength(200);
    SendMessage(GetDlgItem(LinksDiag,IDC_LINKS_EDITBASE+n*100),WM_GETTEXT,200,(LPARAM)Name.Text);
    if(Name.NotEmpty())
    {
      RemoveIllegalFromPath(Name,false,true,'-');
      while(strchr(Name,SLASHCHAR)) 
        *(strchr(Name,SLASHCHAR))='-';
      LinkFileName+=Name+".lnk";
      CreateLink(LinkFileName,LinksTargetPath);
    }
  }
  if(IsSameStr_I(MultipleLinksPath,DisksFol))
    PostMessage(Handle,WM_COMMAND,IDCANCEL,0);
  return true;
}

#endif//WIN32


#ifdef UNIX

int TDiskManager::diag_but_np(hxc_button *b,int mess,int *p_i)
{
  if (mess!=BN_CLICKED) return 0;
  if (b->id==100){
//    TDiskManager *This=(TDiskManager*)(b->owner);
    hxc_textdisplay *p_td=(hxc_textdisplay*)hxc::find(b->parent,200);
    hxc_edit *p_ed=(hxc_edit*)hxc::find(b->parent,101);

    Str Find=p_ed->text;
    if (Find.Empty()) return 0;

    char buf[65536],*p=buf;
    GetContents_SearchDatabase(Find,buf,sizeof(buf));

    Str Name,Contents;
    Str outtext;
    while (p[0]){
      Name=p;
      p+=strlen(p)+1;

      Contents="";
      while (p[0]){
        if (Contents[0]) Contents+=", ";
        Contents+=p;
        p+=strlen(p)+1;
      }
      p++; // skip content list null

      outtext+=Name+" - "+Contents+"\n\n";
    }
    p_td->sy=0;
    p_td->set_text(outtext);
    p_td->draw(true);
  }else if (b->id==200){
    b->set_check(true);
    hxc_edit *p_ed=(hxc_edit*)hxc::find(b->parent,201);
    fileselect.set_corner_icon(&Ico16,ICO16_FOLDER);
    EasyStr new_path=fileselect.choose(XD,p_ed->text,"",T("Pick a Folder"),
      FSM_CHOOSE_FOLDER | FSM_CONFIRMCREATE,folder_parse_routine,"");
    if (new_path[0]){
      NO_SLASH(new_path);
      p_ed->set_text(new_path+"/");
    }
    b->set_check(0);
  }
  return 0;
}
//---------------------------------------------------------------------------
int TDiskManager::diag_ed_np(hxc_edit *ed,int mess,INT_PTR i)
{
  if (mess==EDN_RETURN){
    hxc_button *p_but=(hxc_button*)hxc::find(ed->parent,100);
    int i[1]={Button1};
    diag_but_np(p_but,BN_CLICKED,i);
  }
  return 0;
}

#endif//UNIX


#ifndef SSE_NO_WINSTON_IMPORT

bool TDiskManager::ImportDiskExists(char *Disk,EasyStr &FullDisk) {
  if(Disk==NULL) return 0;
  if(FloppyDisk[DRIVE_B]==':')
    FullDisk=Disk;
  else
    FullDisk=WinSTonDiskPath+"\\"+Disk;
  if(Exists(FullDisk)) return true;
  // Default to zip if no extension and disk doesn't exist
  EasyStr OldExt=".zip";
  // Strip current extension and save it
  char *dot=strrchr(FullDisk,'.');
  if(dot)
  {
    OldExt=dot;
    *dot='\0';
  }
  // Go through list of extensions and see if any of them exist
  char *WinSTonDiskExts[4]={".msa",".st",".zip",NULL};
  int i=0;
  while(WinSTonDiskExts[i]) {
    if(Exists(FullDisk+WinSTonDiskExts[i]))
    {
      FullDisk+=WinSTonDiskExts[i];
      return true;
    }
    i++;
  }
  // Doesn't exist, still set most likely file name
  FullDisk+=OldExt;
  return 0;
}


HRESULT TDiskManager::CreateLinkCheckForOverwrite(char *LinkPath,char *TargetPath,
                          IShellLink *Link,IPersistFile* File){
  if (ImportConflictAction!=1){ // Not overwrite
    EasyStr NewLinkPath=LinkPath;
    if (Exists(LinkPath)){
      switch (ImportConflictAction){
        case 0:
          NewLinkPath="";
          break;
        case 2:case 3:
        {
          EasyStr UniqueLink=LinkPath;
          int n=2;
          UniqueLink.Insert(" (2)",UniqueLink.Length()-4);
          while (Exists(UniqueLink)) UniqueLink[UniqueLink.Length()-6]=char('1'+(n++));
          if (ImportConflictAction==2){
            NewLinkPath=UniqueLink;
          }else{
            if (MoveFile(LinkPath,UniqueLink)==0) NewLinkPath="";
          }
          break;
        }
      }
    }
    if (NewLinkPath.NotEmpty()) return CreateLink(NewLinkPath,TargetPath,NULL,Link,File);
    return 0;
  }
  return CreateLink(LinkPath,TargetPath,NULL,Link,File);
}


bool TDiskManager::DoImport() {
  WinSTonPath.SetLength(MAX_PATH);
  SendMessage(GetDlgItem(ImportDiag,101),WM_GETTEXT,MAX_PATH,(LPARAM)WinSTonPath.Text);
  NO_SLASH(WinSTonPath);
  if (WinSTonPath.Text[0]==0){
    Alert(T("Please enter the full path of the folder WinSTon is in."),T("Import Error"),MB_ICONEXCLAMATION);
    return 0;
  }else if (GetFileAttributes(WinSTonPath)==0xffffffff){
    Alert(WinSTonPath+" "+T("does not exist."),T("Import Error"),MB_ICONEXCLAMATION);
    return 0;
  }
  WinSTonDiskPath.SetLength(MAX_PATH);
  SendMessage(GetDlgItem(ImportDiag,151),WM_GETTEXT,MAX_PATH,(LPARAM)WinSTonDiskPath.Text);
  NO_SLASH(WinSTonDiskPath);
  if (WinSTonDiskPath.Text[0]==0){
    Alert(T("Please enter the full path of the folder WinSTon stores its disks in."),T("Import Error"),MB_ICONEXCLAMATION);
    return 0;
  }else if (GetFileAttributes(WinSTonDiskPath)==0xffffffff){
    Alert(WinSTonDiskPath+" "+T("does not exist."),T("Import Error"),MB_ICONEXCLAMATION);
    return 0;
  }
  ImportPath.SetLength(MAX_PATH);
  SendMessage(GetDlgItem(ImportDiag,201),WM_GETTEXT,MAX_PATH,(LPARAM)ImportPath.Text);
  NO_SLASH(ImportPath);
  if (ImportPath.Text[0]==0){
    Alert(T("Please specify a folder to import the favourites to."),T("Import Error"),MB_ICONEXCLAMATION);
    return 0;
  }
  if (GetFileAttributes(ImportPath)==0xffffffff){
    if (CreateDirectory(ImportPath,NULL)==0){
      Alert(T("Couldn't create the import folder")+" "+ImportPath,T("Import Error"),MB_ICONEXCLAMATION);
      return 0;
    }
  }
  ImportOnlyIfExist=SendMessage(GetDlgItem(ImportDiag,300),BM_GETCHECK,0,0);
  ImportConflictAction=SendMessage(GetDlgItem(ImportDiag,302),CB_GETCURSEL,0,0);
  FILE *fp=fopen(WinSTonPath+"\\favourites.txt","rb");
  if (fp==NULL){
    Alert(T("Couldn't open favourites.txt, please check that the WinSTon folder is correct."),
            T("Import Error"),MB_ICONEXCLAMATION);
    return 0;
  }
  long Len=GetFileLength(fp);
  char *Data=new char[Len+1];
  FREAD(Data,1,Len,fp);
  Data[Len]=0;
  fclose(fp);
  Importing=true;
  EnableAllWindows(false,ImportDiag);
  EnableWindow(GetDlgItem(ImportDiag,100),0);
  EnableWindow(GetDlgItem(ImportDiag,101),0);
  EnableWindow(GetDlgItem(ImportDiag,102),0);
  EnableWindow(GetDlgItem(ImportDiag,150),0);
  EnableWindow(GetDlgItem(ImportDiag,151),0);
  EnableWindow(GetDlgItem(ImportDiag,152),0);
  EnableWindow(GetDlgItem(ImportDiag,200),0);
  EnableWindow(GetDlgItem(ImportDiag,201),0);
  EnableWindow(GetDlgItem(ImportDiag,202),0);
  EnableWindow(GetDlgItem(ImportDiag,300),0);
  EnableWindow(GetDlgItem(ImportDiag,301),0);
  EnableWindow(GetDlgItem(ImportDiag,302),0);
  long OneBit=MAX(Len/280,1L);
  SendMessage(GetDlgItem(ImportDiag,400),PBM_SETRANGE,0,MAKELPARAM(0,280));
  SendMessage(GetDlgItem(ImportDiag,400),PBM_SETPOS,0,0);
  ShowWindow(GetDlgItem(ImportDiag,IDOK),SW_HIDE);
  ShowWindow(GetDlgItem(ImportDiag,400),SW_SHOW);
  IShellLink *LinkObj=NULL;
  IPersistFile *FileObj=NULL;
  HRESULT hres=CoCreateInstance(CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,IID_IShellLink,(void**)&LinkObj);
  if (SUCCEEDED(hres)==0) LinkObj=NULL;
  if (LinkObj){
    hres=LinkObj->QueryInterface(IID_IPersistFile,(void**)&FileObj);
    if (SUCCEEDED(hres)==0) FileObj=NULL;
  }
  EasyStr CurFol=ImportPath,LinkPath;
  char *FloppyDisk[2][2]={{NULL,NULL},{NULL,NULL}};
  char *Command=Data,*pRet,*CommandStart,*NextCommand;
  long BitPos,OldBitPos=0;
  while (Importing){
    pRet=strchr(Command,'\n');
    if (pRet==NULL) break;
    NextCommand=pRet+1;
    if (*(pRet-1)=='\r') pRet--;
    *pRet=0;
    if ((CommandStart=strstr(Command,"[FOLDER]"))!=NULL){
      CurFol+="\\";
      CurFol+=RemoveIllegalFromPath(CommandStart+8,0);
      CreateDirectory(CurFol,NULL);
    }else if ((CommandStart=strstr(Command,"[ENDFOLDER]"))!=NULL){
      *(GetFileNameFromPath(CurFol)-1)=0;
      FloppyDisk[DRIVE_A][0]=NULL;FloppyDisk[DRIVE_B][0]=NULL;FloppyDisk[DRIVE_A][1]=NULL;FloppyDisk[DRIVE_B][1]=NULL;
    }else if ((CommandStart=strstr(Command,"[TITLE]"))!=NULL){
      LinkPath=CurFol+"\\"+RemoveIllegalFromPath(CommandStart+7,0)+".lnk";
    }else if ((CommandStart=strstr(Command,"[ENDTITLE]"))!=NULL){
      if (FloppyDisk[DRIVE_B][0] || FloppyDisk[DRIVE_B][1]) LinkPath.Insert(" (Disk 1)",LinkPath.Length()-4);
      EasyStr TargetPath,NewLinkPath;
      for (int d=0;d<2;d++){
        if (FloppyDisk[d][0]){
          if (ImportDiskExists(FloppyDisk[d][0],TargetPath)){
            CreateLinkCheckForOverwrite(LinkPath,TargetPath,LinkObj,FileObj);
          }else if (ImportDiskExists(FloppyDisk[d][1],TargetPath)){
            CreateLinkCheckForOverwrite(LinkPath,TargetPath,LinkObj,FileObj);
          }else if (ImportOnlyIfExist==0){
            ImportDiskExists(FloppyDisk[d][0],TargetPath);
            CreateLinkCheckForOverwrite(LinkPath,TargetPath,LinkObj,FileObj);
          }
        }
        LinkPath[LinkPath.Length()-6]='2';
      }
      LinkPath[0]=0;
      FloppyDisk[DRIVE_A][0]=NULL;FloppyDisk[DRIVE_B][0]=NULL;FloppyDisk[DRIVE_A][1]=NULL;FloppyDisk[DRIVE_B][1]=NULL;
    }else if ((CommandStart=strstr(Command,"[DISC1]"))!=NULL){
      FloppyDisk[DRIVE_A][0]=RemoveIllegalFromPath(CommandStart+7,CommandStart[8]==':');
    }else if ((CommandStart=strstr(Command,"[DISC2]"))!=NULL){
      FloppyDisk[DRIVE_B][0]=RemoveIllegalFromPath(CommandStart+7,CommandStart[8]==':');
    }else if ((CommandStart=strstr(Command,"[ALTDISC1]"))!=NULL){
      FloppyDisk[DRIVE_A][1]=RemoveIllegalFromPath(CommandStart+10,CommandStart[11]==':');
    }else if ((CommandStart=strstr(Command,"[ALTDISC2]"))!=NULL){
      FloppyDisk[DRIVE_B][1]=RemoveIllegalFromPath(CommandStart+10,CommandStart[11]==':');
    }
    Command=NextCommand;
    BitPos=(long(Command)-long(Data))/OneBit;
    if (BitPos!=OldBitPos){
      SendMessage(GetDlgItem(ImportDiag,400),PBM_SETPOS,WPARAM(BitPos),0);
      OldBitPos=BitPos;
    }
    PeekEvent();
  }
  if (LinkObj) LinkObj->Release();
  if (FileObj) FileObj->Release();
  delete[] Data;
  EnableWindow(GetDlgItem(ImportDiag,100),true);
  EnableWindow(GetDlgItem(ImportDiag,101),true);
  EnableWindow(GetDlgItem(ImportDiag,102),true);
  EnableWindow(GetDlgItem(ImportDiag,150),true);
  EnableWindow(GetDlgItem(ImportDiag,151),true);
  EnableWindow(GetDlgItem(ImportDiag,152),true);
  EnableWindow(GetDlgItem(ImportDiag,200),true);
  EnableWindow(GetDlgItem(ImportDiag,201),true);
  EnableWindow(GetDlgItem(ImportDiag,202),true);
  EnableWindow(GetDlgItem(ImportDiag,300),true);
  EnableWindow(GetDlgItem(ImportDiag,301),true);
  EnableWindow(GetDlgItem(ImportDiag,302),true);
  EnableAllWindows(true,ImportDiag);
  if (Importing==0){ //Cancelled
    ShowWindow(GetDlgItem(ImportDiag,400),SW_HIDE);
    ShowWindow(GetDlgItem(ImportDiag,IDOK),SW_SHOW);
    return 0;
  }
  SetDir(ImportPath,true);
  Importing=0;
  return true;
}

#endif//#ifndef SSE_NO_WINSTON_IMPORT

#undef LOGSECTION
#undef CharHeight
#undef LineHeight

#undef IDC_DB_SEARCH
#undef IDC_DB_LIST
#undef IDC_DB_STEEMSITE
#undef IDC_CONTENT_PATH
#undef IDC_CONTENT_TOSEC
#undef IDC_CONTENT_CRC
#undef IDC_CONTENT_LIST
#undef IDC_CONTENT_DESTFOLDER
#undef IDC_CONTENT_BROWSE
#undef IDC_CONTENT_ONCONFLICT
#undef IDC_CONTENT_APPENDNAME
#undef IDC_CONTENT_SHORTNAME
#undef IDC_CREATE_EXT_ST
#undef IDC_CREATE_EXT_MSA
#undef IDC_CREATE_EXT_DIM
#undef IDC_CREATE_SIDES
#undef IDC_CREATE_SECTORS
#undef IDC_CREATE_TRACKS
#undef IDS_CREATE_SIZE
#undef IDC_LINKS_TARGET
#undef IDC_LINKS_BROWSE
#undef IDC_LINKS_TARGETMULTI
#undef IDC_LINKS_BROWSEMULTI
#undef IDC_LINKS_EDITBASE
#undef IDC_PROP_PATH
#undef IDC_PROP_LINKPATH
#undef IDS_PROP_SIZE
#undef IDC_PROP_LIST
#undef IDS_PROP_GROUP
#undef IDC_PROP_BPB
#undef IDC_PROP_DATABYTES
#undef IDC_PROP_SIDES
#undef IDC_PROP_TRACKS
#undef IDC_PROP_SECTORS
#undef IDC_PROP_BYTES
#undef IDC_PROP_DETECT
#undef IDC_PROP_APPLY
#undef IDC_PROP_CONTENT
