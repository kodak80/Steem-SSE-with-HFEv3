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
FILE: diskman_drag.cpp
CONDITION: WIN32 must be defined
DESCRIPTION: Routines to handle dragging in the Disk Manager.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <diskman.h>
#include <gui.h>
#include <computer.h>
#include <archive.h>
#include <translate.h>
#include <display.h>

#if !defined(SSE_LIBRETRONUKE)


void TDiskManager::BeginDrag(int Item,HWND From) { // handling LVN_BEGINDRAG, LVN_BEGINRDRAG
  LV_ITEM lvi;
  lvi.mask=LVIF_PARAM;
  lvi.iItem=Item;
  lvi.iSubItem=0;
  SendMessage(From,LVM_GETITEM,0,(LPARAM)&lvi);
  if(((TDiskManFileInfo*)lvi.lParam)->UpFolder)
  {
    SetFocus(DiskView);
    return;
  }
  SendMessage(From,LVM_ENSUREVISIBLE,Item,FALSE);
  UpdateWindow(From);
  POINT pt={0,0};
  Dragging=Item;
  DragLV=From;
  DragIL=(HIMAGELIST)SendMessage(DragLV,LVM_CREATEDRAGIMAGE,Dragging,(LPARAM)&pt);
  EndingDrag=false;
  SetCapture(Handle);
  ImageList_GetIconSize(DragIL,&DragWidth,&DragHeight);
#if defined(SSE_GUI_DM_BIGICONS)
  if(From==DiskView && IconSize==0)
#else
  if(From==DiskView && SmallIcons)
#endif
  {
    DragWidth=(18+GetTextSize(Font,((TDiskManFileInfo*)lvi.lParam)->Name).Width)/2;
    DragHeight=-(DragHeight-2);
  }
  else
  {
    DragWidth/=2;
    DragWidth-=5;
    DragHeight=0;
  }
  ImageList_BeginDrag(DragIL,0,0,0);
  GetCursorPos(&pt);
  ScreenToClient(Handle,&pt);
  ImageList_DragEnter(Handle,pt.x-DragWidth,pt.y-DragHeight);
  DragEntered=true;
  SetTimer(Handle,DISKVIEWSCROLL_TIMER_ID,30,NULL);
}


void TDiskManager::drag_check_for_deselect(int DeselectDropTarget) {
  if(DeselectDropTarget) {
      LV_ITEM lvi;
      lvi.iSubItem=0;
      lvi.stateMask=LVIS_DROPHILITED;
      lvi.state=0;
      SendMessage(DiskView,LVM_SETITEMSTATE,DropTarget,(LPARAM)&lvi);
      UpdateWindow(DiskView);
      DropTarget=-1;
  }
}

#define DRAG_CHECK_FOR_DESELECT drag_check_for_deselect(DeselectDropTarget)


void TDiskManager::MoveDrag() { // handling WM_MOUSEMOVE, WM_VSCROLL
  POINT pt,spt;
  bool Okay=false,DeselectDropTarget=(DropTarget>-1);
  LV_ITEM lvi1;
  lvi1.mask=LVIF_PARAM;
  lvi1.iItem=Dragging;
  lvi1.iSubItem=0;
  SendMessage(DragLV,LVM_GETITEM,0,(LPARAM)&lvi1);
  TDiskManFileInfo *DragInf=(TDiskManFileInfo*)lvi1.lParam;
  GetCursorPos(&spt);pt=spt;
  ScreenToClient(Handle,&pt);
  int OverID=GetDlgCtrlID(ChildWindowFromPoint(Handle,pt));
  if(((OverID==IDC_CONTENTA||OverID==IDC_CONTENTB)&&!DragInf->Folder)
    || OverID==IDC_DISKVIEW
    ||(OverID==IDC_HOME&&!AtHome&&DragLV==DiskView))
  {
    Okay=true;
    if(OverID==IDC_DISKVIEW&&GetDlgCtrlID(DragLV)==IDC_DISKVIEW)
    { // Dragging from DiskView to DiskView
      LV_HITTESTINFO hti;
      LV_ITEM lvi2;
      lvi2.iSubItem=0;
      hti.pt=spt;
      ScreenToClient(DiskView,&hti.pt);
      int Item=(int)SendMessage(DiskView,LVM_HITTEST,0,(LPARAM)&hti);
      if(Item!=DropTarget)
      {
        if(Item>-1)
        {
          lvi2.mask=LVIF_PARAM;
          lvi2.iItem=Item;
          lvi2.iSubItem=0;
          SendMessage(DiskView,LVM_GETITEM,0,(LPARAM)&lvi2);
          if(((TDiskManFileInfo*)lvi2.lParam)->Folder==0||Item==Dragging)
            Item=-1;
        }
        if(Item!=DropTarget)
        {
          if(DragEntered)
          {
            ImageList_DragLeave(Handle);
            DragEntered=false;
          }

          lvi2.stateMask=LVIS_DROPHILITED;
          if(DropTarget>-1)
          {
            lvi2.state=0;
            SendMessage(DiskView,LVM_SETITEMSTATE,DropTarget,(LPARAM)&lvi2);
          }
          if(Item>-1)
          {
            lvi2.state=LVIS_DROPHILITED;
            SendMessage(DiskView,LVM_SETITEMSTATE,Item,(LPARAM)&lvi2);
          }
          DropTarget=Item;
          UpdateWindow(DiskView);
        }
      }
      DeselectDropTarget=false;
    }
  }
  if(LastOverID==IDC_HOME&&OverID!=IDC_HOME)
  {
    if(DragEntered)
    {
      ImageList_DragLeave(Handle);
      DragEntered=false;
    }
    SendMessage(GetDlgItem(Handle,IDC_HOME),BM_SETCHECK,0,0);
  }
  if(Okay)
  {
    if(OverID==IDC_HOME&&LastOverID!=IDC_HOME)
    {
      if(DragEntered)
      {
        ImageList_DragLeave(Handle);
        DragEntered=false;
      }
      SendMessage(GetDlgItem(Handle,IDC_HOME),BM_SETCHECK,1,0);
    }
    SetCursor(PCArrow);
    DRAG_CHECK_FOR_DESELECT;
    if(!DragEntered)
    {
      ImageList_DragEnter(Handle,pt.x-DragWidth,pt.y-DragHeight);
      DragEntered=true;
    }
    else
      ImageList_DragMove(pt.x-DragWidth,pt.y-DragHeight);
  }
  else
  {
    SetCursor(LoadCursor(NULL,IDC_NO));
    if(DragEntered)
    {
      ImageList_DragLeave(Handle);
      DragEntered=false;
    }
    DRAG_CHECK_FOR_DESELECT;
    if(DragLV==DiskView)
    {
      RECT rc;
      GetWindowRect(DiskView,&rc);
      if(spt.x>=rc.left && spt.y<=rc.right)
      {
        if(spt.y<=rc.top+2&&spt.y>=rc.top-20)
          SendMessage(DiskView,LVM_SCROLL,0,-8);
        else if(spt.y>=rc.bottom-2&&spt.y<=rc.bottom+10)
          SendMessage(DiskView,LVM_SCROLL,0,8);
        UpdateWindow(DiskView);
      }
    }
  }
  LastOverID=OverID;
}


/*#define IDC_ZIPPYBASE 4000
#define IDC_ZIPPYCANCEL 4099
#define IDC_MOVESHORTCUT 4003
#define IDC_COPYSHORTCUT 4004
#define IDC_CREATESHORTCUTS 4010
#define IDC_CREATESHORTCUTS2 4011
#define IDC_MOVEGETCONTENT 4012
*/

void TDiskManager::EndDrag(SHORT x,SHORT y,bool RightDrag) { // handling WM_LBUTTONUP, WM_RBUTTONUP
  TDiskManFileInfo *DragInf=GetItemInf(Dragging,DragLV);
  EndingDrag=true;
  if(DragEntered) 
    ImageList_DragLeave(Handle);
  ImageList_EndDrag();
  ImageList_Destroy(DragIL);
  ReleaseCapture();
  KillTimer(Handle,1);
  POINT pt={x,y};
  HWND LV;
  for(int i=IDC_HOME;i<=IDC_DISKVIEW;i++)
  {
    LV=GetDlgItem(Handle,i);
    if(ChildWindowFromPoint(Handle,pt)==LV)
    {
      if(DragLV!=LV && i>IDC_HOME&&!DragInf->Folder)
      {
        if(i<IDC_DISKVIEW)
        {  // Dragged from DiskView or other Floppy to Floppy ListView
          bool InsertIt=true;
          EasyStr DiskInZip;
          if(DragInf->Zip && enable_zip)
          {
            if(DragLV!=DiskView)
             // Dragged from one floppy ListView to another
              DiskInZip=FloppyDisk[GetDlgCtrlID(DragLV)-IDC_CONTENTA].DiskInZip;
            else
            {
              EasyStringList esl;
              esl.Sort=eslSortByNameI;
              // Get all disks in archive
              zippy.list_contents(DragInf->Path,&esl,true);
              if(esl.NumStrings>1)
              {
                HMENU Pop=CreatePopupMenu();
                for(int n=0;n<esl.NumStrings;n++)
                  AppendMenu(Pop,MF_STRING,(IDC_ZIPPYBASE+n),esl[n].String);
                AppendMenu(Pop,MF_SEPARATOR,IDS_STATIC,NULL);
                AppendMenu(Pop,MF_STRING,IDC_ZIPPYCANCEL,T("Cancel"));
                GetCursorPos(&pt);
                TrackPopupMenu(Pop,TPM_LEFTALIGN|TPM_LEFTBUTTON,pt.x,pt.y,0,Handle,NULL);
                DestroyMenu(Pop);
                MenuTarget=0;
                MSG mess;
                while(PeekMessage(&mess,Handle,WM_COMMAND,WM_COMMAND,PM_REMOVE))
                  DispatchMessage(&mess);
                if(MenuTarget>=IDC_ZIPPYBASE&&MenuTarget-IDC_ZIPPYBASE<esl.NumStrings)
                  DiskInZip=esl[MenuTarget-IDC_ZIPPYBASE].String;
                else
                  InsertIt=false;
              }
            }
          }
          if(InsertIt)
          {
            if(InsertDisk(i-IDC_CONTENTA,DragInf->Name,DragInf->Path,false,true,
              DiskInZip,false,DragLV==DiskView))
            {
              if(DragLV!=DiskView)
               // Dragged from one floppy ListView to another
                EjectDisk(!!(GetDlgCtrlID(DragLV)-IDC_CONTENTA));
            }
          }
        }
        else
         // Dragged from Floppy to DiskView
          EjectDisk(!!(GetDlgCtrlID(DragLV)-IDC_CONTENTA));
        break;
      }
      else if((LV==DiskView&&(DropTarget>-1||RightDrag))||
        (i==IDC_HOME && !AtHome && DragLV==DiskView))
      {
        bool DraggedToFolder=false;
        TDiskManFileInfo *DestInf=NULL;
        EasyStr DestFol;
        if(DropTarget>-1)
        {
          DestInf=GetItemInf(DropTarget);
          if(DestInf->UpFolder)
          {
            DestFol=DisksFol;
            RemoveFileNameFromPath(DestFol,REMOVE_SLASH);
          }
          else
            DestFol=DestInf->Path;
          DraggedToFolder=true;
        }
        else if(i==IDC_HOME)
          DestFol=HomeFol;
        else
          DestFol=DisksFol;
        if(RightDrag)
        {
          MenuTarget=0;
          SetFocus(DiskView);
          GetCursorPos(&pt);
          HMENU OpMenu=CreatePopupMenu();
          if(DragInf->LinkPath.IsEmpty())
          {
            if(DestFol!=DisksFol) 
              AppendMenu(OpMenu,MF_STRING,IDC_MOVEHERE,T("&Move Here"));
            AppendMenu(OpMenu,MF_STRING,IDC_COPYHERE,T("&Copy Here"));
            AppendMenu(OpMenu,MF_STRING,IDC_CREATESHORTCUT,T("Create &Shortcut Here"));
            if(!DragInf->Folder) 
              AppendMenu(OpMenu,MF_STRING,IDC_CREATESHORTCUTS,T("Create M&ultiple Shortcuts Here"));
            if(DestFol!=DisksFol && !DragInf->Folder)
            {
              AppendMenu(OpMenu,MF_STRING,IDC_MOVEGETCONTENT,T("Move Disk Here and &Get Contents"));
              AppendMenu(OpMenu,MF_STRING,IDC_CREATESHORTCUTS2,
                T("Move Disk Here and Create Multiple Shortcuts to it"));
            }
          }
          else
          {
            if(DestFol!=DisksFol) 
              AppendMenu(OpMenu,MF_STRING,IDC_MOVESHORTCUT,T("&Move Shortcut Here"));
            AppendMenu(OpMenu,MF_STRING,IDC_COPYSHORTCUT,T("&Copy Shortcut Here"));
            AppendMenu(OpMenu,MF_SEPARATOR,IDS_STATIC,NULL);
            EasyStr FolDisk=DragInf->Path;
            RemoveFileNameFromPath(FolDisk,REMOVE_SLASH);
            if(NotSameStr_I(FolDisk,DestFol))
              AppendMenu(OpMenu,MF_STRING,IDC_MOVEHERE,(LPSTR)((DragInf->Folder)
                ?T("Move &Folder Here"):T("Move &Disk Here")));
            AppendMenu(OpMenu,MF_STRING,IDC_COPYHERE,(LPSTR)((DragInf->Folder)
              ?T("Copy F&older Here"):T("Copy D&isk Here")));
            if(!DragInf->Folder) 
              AppendMenu(OpMenu,MF_STRING,IDC_CREATESHORTCUTS,
                T("Create Mu&ltiple Shortcuts To The Disk Here"));
          }
          AppendMenu(OpMenu,MF_SEPARATOR,IDS_STATIC,NULL);
          AppendMenu(OpMenu,MF_STRING,IDC_ZIPPYCANCEL,T("Cancel"));
          TrackPopupMenu(OpMenu,TPM_LEFTALIGN|TPM_LEFTBUTTON,pt.x,pt.y,0,Handle,NULL);
          DestroyMenu(OpMenu);
          MSG mess;//TODO
          while(PeekMessage(&mess,Handle,WM_COMMAND,WM_COMMAND,PM_REMOVE))
            DispatchMessage(&mess);
        }
        else
          MenuTarget=int(DragInf->LinkPath.IsEmpty()?IDC_MOVEHERE:IDC_MOVESHORTCUT);
        EasyStr SrcFol=(LPSTR)((MenuTarget==IDC_MOVEHERE||MenuTarget==IDC_COPYHERE
          ||MenuTarget==IDC_CREATESHORTCUTS2||MenuTarget==IDC_MOVEGETCONTENT)
          ?DragInf->Path:DragInf->LinkPath);
        RemoveFileNameFromPath(SrcFol,REMOVE_SLASH);
        if(MenuTarget==IDC_CREATESHORTCUT)
        {  //Create Shortcut
          EasyStr LinkName=DestFol+SLASH+DragInf->Name+".lnk";
          int n=2;
          while(Exists(LinkName))
            LinkName=DestFol+SLASH+DragInf->Name+" ("+(n++)+").lnk";
          CreateLink(LinkName,DragInf->Path);
          if(IsSameStr_I(DestFol,DisksFol))
          {
            RefreshDiskView("",true,LinkName);
            DropTarget=-1;
          }
        }
        else if(((MenuTarget==IDC_COPYHERE&&!DragInf->Folder)||MenuTarget==IDC_COPYSHORTCUT)
          &&SrcFol==DestFol)
        {
          EasyStr Name=GetFileNameFromPath((LPSTR)((MenuTarget==IDC_COPYHERE)
            ?DragInf->Path:DragInf->LinkPath));
          EasyStr Ext;
          char *dot=strrchr(Name,'.');
          if(dot)
          {
            Ext=dot;
            *dot='\0';
          }
          EasyStr Path;
          int n=2;
          do
          {
            Path=DestFol+SLASH+Name+" ("+(n++)+")"+Ext;
          } while(Exists(Path));
          CopyFile((LPSTR)((MenuTarget==IDC_COPYHERE)?DragInf->Path:DragInf->LinkPath),Path,true);
          if(MenuTarget==IDC_COPYHERE) 
            UpdateBPBFiles(DragInf->Path,Path,false);
          if(MenuTarget==IDC_COPYHERE)
            RefreshDiskView(Path,true);
          else
            RefreshDiskView("",true,Path);
          DropTarget=-1;
        }
        else if(MenuTarget==IDC_MOVEHERE||MenuTarget==IDC_COPYHERE||MenuTarget
          ==IDC_CREATESHORTCUTS2||MenuTarget==IDC_MOVEGETCONTENT|| //Move/Copy Path
          MenuTarget==IDC_MOVESHORTCUT||MenuTarget==IDC_COPYSHORTCUT)//Move/Copy LinkPath
        {  
          bool Moving=(MenuTarget==IDC_MOVEHERE||MenuTarget==IDC_CREATESHORTCUTS2
            ||MenuTarget==IDC_MOVESHORTCUT);
          bool DiskIsTarget=(MenuTarget<IDC_MOVESHORTCUT||MenuTarget==IDC_CREATESHORTCUTS2);
          bool DoIt=true;
          if(MenuTarget==IDC_MOVEGETCONTENT)
          { // Get contents
            Moving=true;
            DiskIsTarget=true;
            GetContentsSL(DragInf->Path);
            if(contents_sl.NumStrings==0) 
              DoIt=false;
          }
          EasyStr To;
#if defined(SSE_LONG_PATH)
          EasyStr sFrom;
          sFrom.SetLength(SSE_MAX_PATH);
          char *From=sFrom.Text;
          ZeroMemory(From,SSE_MAX_PATH);
#else
          char From[MAX_PATH+2];
          ZeroMemory(From,MAX_PATH+2);
#endif
          if(DiskIsTarget)
          {
            strcpy(From,DragInf->Path);
            To=DestFol;
            if(!DragInf->Folder) 
              To+=EasyStr(SLASH)+GetFileNameFromPath(DragInf->Path);
          }
          else
          {
            strcpy(From,DragInf->LinkPath);
            To=DestFol+SLASH+GetFileNameFromPath(DragInf->LinkPath);
          }
          char *DiskPath="";
          if(DiskIsTarget) 
            DiskPath=DragInf->Path;
          if(DoIt)
          {
            if(MoveOrCopyFile(Moving,From,To,DiskPath,IsSameStr_I(SrcFol,DestFol)))
            {
              if(Moving && DiskIsTarget && DragInf->LinkPath.NotEmpty())
                //Update shortcut for the new location of disk
                CreateLink(DragInf->LinkPath,To);
              LinksTargetPath=To;
              if(IsSameStr_I(SrcFol,DisksFol)||IsSameStr_I(DestFol,DisksFol))
              {
                //Refresh the DiskView
                if(DraggedToFolder)
                {
                  if(DestInf->LinkPath.NotEmpty())
                    RefreshDiskView("",false,DestInf->LinkPath);
                  else
                    RefreshDiskView(DestInf->Path);
                }
                else
                {
                  if(DiskIsTarget)
                  { // Path operation
                    if(DragInf->LinkPath.NotEmpty())
                    {
                      if(IsSameStr_I(DestFol,DisksFol))
                        RefreshDiskView(To);
                      else
                        RefreshDiskView("",false,DragInf->LinkPath);
                    }
                    else
                      RefreshDiskView("",false,"",GetSelectedItem());
                  }
                  else
                  {                // LinkPath operation
                    if(MenuTarget==IDC_MOVESHORTCUT)
                      RefreshDiskView("",false,"",GetSelectedItem());
                    else
                      RefreshDiskView("",false,DragInf->LinkPath);
                  }
                }
                DropTarget=-1;
              }
              if(MenuTarget==IDC_CREATESHORTCUTS2)
              {
                MultipleLinksPath=SrcFol;
                ShowLinksDiag();
                RefreshDiskView("",false,"",GetSelectedItem());
              }
              else if(MenuTarget==IDC_MOVEGETCONTENT)
              {
                contents_sl.SetString(0,To);
                ContentsLinksPath=DisksFol;
                EnableWindow(Handle,FALSE);
                ShowContentDiag();
              }
            }
          }
        }
        else if(MenuTarget==IDC_CREATESHORTCUTS)
        {
          MultipleLinksPath=DestFol;
          LinksTargetPath=DragInf->Path;
          ShowLinksDiag();
        }
        if(DropTarget>-1)
        {
          LV_ITEM lvi;
          lvi.stateMask=LVIS_DROPHILITED;
          lvi.state=0;
          SendMessage(DiskView,LVM_SETITEMSTATE,DropTarget,(LPARAM)&lvi);
          DropTarget=-1;
        }
        else if(i==IDC_HOME)
          SendMessage(GetDlgItem(Handle,IDC_HOME),BM_SETCHECK,0,0);
        break;
      }
    }
    if(i==IDC_HOME)
      i=IDC_CONTENTA-1;
  }
  Dragging=-1;
}

#undef IDC_ZIPPYBASE
#undef IDC_ZIPPYCANCEL
#undef IDC_MOVESHORTCUT
#undef IDC_CREATESHORTCUTS
#undef IDC_CREATESHORTCUTS2
#undef IDC_MOVEGETCONTENT


void TDiskManager::UpdateBPBFiles(Str CurDisk,Str NewDisk,bool Moving) {
  //SS TODO, not too sure of the purpose
  EasyStringList cur_sl,new_sl;
  cur_sl.Add(CurDisk+".steembpb");
  if(NewDisk.NotEmpty()) 
    new_sl.Add(NewDisk+".steembpb");
  if(Exists(CurDisk) && FileIsDisk(CurDisk)==DISK_COMPRESSED) //402R18
  {
    EasyStringList zsl;
    zippy.list_contents(CurDisk,&zsl,true); // asserts there if not existing
    for(int i=0;i<zsl.NumStrings;i++)
    {
      cur_sl.Add(CurDisk+zsl[i].String+".steembpb");
      if(NewDisk.NotEmpty()) 
        new_sl.Add(NewDisk+zsl[i].String+".steembpb");
    }
  }
  for(int i=0;i<cur_sl.NumStrings;i++)
  {
    if(NewDisk.NotEmpty())
      CopyFile(cur_sl[i].String,new_sl[i].String,0);
    if(Moving||NewDisk.Empty())
      DeleteFile(cur_sl[i].String);
  }
}


bool TDiskManager::MoveOrCopyFile(bool Moving,char *From,char *To,
                                  char *DiskPath,bool SameFol) {
  bool InDrive[2]={false,false};
  EasyStr OldName[2],DiskInZip[2];
  if(Moving && DiskPath[DRIVE_A])
  { // Moving disk
    for(int d=DRIVE_A;d<=DRIVE_B;d++)
    {
      if(IsSameStr_I(FloppyDrive[d].GetDisk(),DiskPath))
      {
        InDrive[d]=true;
        OldName[d]=FloppyDisk[d].DiskName;
        DiskInZip[d]=FloppyDisk[d].DiskInZip;
        FloppyDrive[d].RemoveDisk();
      }
    }
  }
  EasyStr Dest=To;
  if(SameFol)
  {
    RemoveFileNameFromPath(Dest,KEEP_SLASH);
    Str FromName=GetFileNameFromPath(From);
    EasyStr Ext;
    char *dot=strrchr(FromName,'.');
    if(dot)
    {
      Ext=dot;
      *dot='\0';
    }
    Str f;
    int i=2;
    do
    {
      f=FromName+" ("+(i++)+")"+Ext;
    } while(Exists(Dest+f));
    Dest+=f;
  }
  SHFILEOPSTRUCT fos;
  fos.hwnd=(FullScreen) ? StemWin : Handle;
  fos.wFunc=(Moving) ? FO_MOVE : FO_COPY;
  fos.pFrom=From;
  fos.pTo=Dest;
  fos.fFlags=FILEOP_FLAGS(FOF_ALLOWUNDO|(int)((FullScreen)?FOF_SILENT:0));
  fos.hNameMappings=NULL;
  fos.lpszProgressTitle=(LPSTR)((Moving) ? StaticT("Moving...") : StaticT("Copying..."));
  EnableWindow(Handle,FALSE);
  int Err=SHFileOperation(&fos);
  EnableWindow(Handle,TRUE);
  if(Err||fos.fAnyOperationsAborted)
  {
    for(int d=DRIVE_A;d<=DRIVE_B;d++)
    {
      if(InDrive[d])
      {
        FloppyDrive[d].SetDisk(DiskPath,DiskInZip[d]);
        FloppyDisk[d].DiskName=OldName[d];
      }
    }
    return false;
  }
  else
  {
    if(DiskPath[DRIVE_A])
      // Doing something with disk image, do the same to
      // associated steembpb files
      UpdateBPBFiles(DiskPath,Dest,Moving);
    for(int d=DRIVE_A;d<=DRIVE_B;d++)
    {
      if(InDrive[d])
      {
        InsertHistoryDelete(d,OldName[d],DiskPath,DiskInZip[d]);
        InsertHistoryAdd(d,OldName[d],Dest,DiskInZip[d]);
        FloppyDrive[d].SetDisk(Dest,DiskInZip[d]);
        FloppyDisk[d].DiskName=OldName[d];
        TDiskManFileInfo *DriveInf=GetItemInf(0,GetDlgItem(Handle,IDC_CONTENTA+d));
        if(DriveInf) 
          DriveInf->Path=Dest;
      }
    }
    UPDATE_STATUS_BAR_PART(SB_PART_CAPS);
    return true;
  }
}

#undef LOGSECTION

#endif//#if !defined(SSE_LIBRETRONUKE)
