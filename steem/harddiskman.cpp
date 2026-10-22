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
FILE: harddiskman.cpp
DESCRIPTION: The code for the hard drive manager dialog.
If SSE_ACSI_MNGR is defined, it is used for ACSI hard drives as well
(Windows + Linux now)
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <diskman.h>
#include <harddiskman.h>
#include <choosefolder.h>
#include <computer.h>
#include <translate.h>
#ifdef WIN32
#include <input_prompt.h>
#endif
#define CharHeight GuiSM.mCharHeight
#define LineHeight GUIMUL(30)

#define IDC_ADD_HD 10
#define IDC_ENABLE_HD 90

#if !defined(SSE_ACSI_MNGR)
const bool acsi=false;
#endif


THardDiskManager::THardDiskManager() {
  Section="HardDrives";
#if defined(SSE_ACSI_MNGR)
  acsi=false; // here is the GEMDOS HD manager
#endif
  OldDriveInfo=NULL;
  nDrives=nOldDrives=0;
  DisableHardDrives=true; // first start, no HD
  ApplyChanges=false;
  for(DWORD i=0;i<MAX_GEMDOS_HARDDRIVES;i++)
  {
    HDrive[i].Path="";
    HDrive[i].Letter=(char)('C'+i);
  }
  update_mount();

#ifdef WIN32
#if !defined(SSE_420R4)
  Left=800/2-258;
  Top=GuiSM.cy_screen()/2-90+GuiSM.cy_caption();
  FSLeft=320 - 258;
  FSTop=240 - 90+GuiSM.cy_caption();
#endif
#endif

}


void THardDiskManager::update_mount() {
  for(BYTE n=DRIVE_C;!acsi&&n<GEMDOS_MAXDRIVES;n++)
  {
    BYTE letter=n+'A';
    if(IsMountedDrive(letter)) 
    {
      Stemdos.DriveMounted[n]=true;
      Stemdos.MountPath[n]=GetMountedDrivePath(letter);
    }
    else 
    {
      Stemdos.DriveMounted[n]=false;
      Stemdos.MountPath[n]="";
    }
  }
  CheckResetDisplay();
}


bool THardDiskManager::IsMountedDrive(char d) {
  if(d>='C'&&!DisableHardDrives)
  {
    for(DWORD n=0;n<nDrives;n++)
    {
      if(d==HDrive[n].Letter)
        return true;
    }
  }
  return false;
}


EasyStr THardDiskManager::GetMountedDrivePath(char d) {
  if(d>='C') 
  {
    for(DWORD n=0;n<nDrives;n++) 
    {
      if(d==HDrive[n].Letter)
        return HDrive[n].Path;
    }
  }
  return RunDir;
}


bool THardDiskManager::NewDrive(char *Path) {
  if(acsi&&nDrives>=MAX_ACSI_DEVICES||!acsi&&nDrives>=MAX_GEMDOS_HARDDRIVES)
    return false;
  BYTE Idx=nDrives;
  bool Found=false;
  HDrive[Idx].Path=Path;
  NO_SLASH(HDrive[Idx].Path);
  for(char myLetter='C';myLetter<='Z' && !Found;myLetter++)
  {
    for(BYTE i=0;i<nDrives;i++)
    {
      if(HDrive[i].Letter==myLetter)
        break;
      else if(i==nDrives-1)
      {
        HDrive[Idx].Letter=myLetter;
        Found=true;
      }
    }
  }
  nDrives++;
  return true;
}


void THardDiskManager::Show() {
  if(Handle!=NULL)
  {
#ifdef WIN32
    SetForegroundWindow(Handle);
#endif
    return;
  }
#ifdef WIN32
#if !defined(SSE_GUI_MENUBAR)
  else if(DiskMan.Handle==NULL)
    return;
#endif
  Font=SSEConfig.GuiFont();
  HWND Win;
#if defined(SSE_GUI_MENUBAR)
  if(DiskMan.Handle)
#endif
    EnableWindow(DiskMan.Handle,FALSE);
  ManageWindowClasses(SD_REGISTER);
#if defined(SSE_ACSI_MNGR)
  Handle=CreateWindowEx(WS_EX_CONTROLPARENT,"Steem Hard Disk Manager",
                        (acsi?T("ACSI Hard Drives"):T("GEMDOS Hard Drives")),WS_CAPTION|WS_SYSMENU,
                        Left,Top,516,90+GuiSM.cy_caption(),DiskMan.Handle,0,hInstance,NULL);
#else
  Handle=CreateWindowEx(WS_EX_CONTROLPARENT,"Steem Hard Disk Manager",
    T("Hard Drives"),WS_CAPTION|WS_SYSMENU,Left,Top,516,90+GuiSM.cy_caption(),
    DiskMan.Handle,0,hInstance,NULL);
#endif
#if defined(SSE_420R4)
  UpdateLeftTop();
#endif
  if(HandleIsInvalid())
  {
    ManageWindowClasses(SD_UNREGISTER);
    return;
  }
  SetWindowLongPtr(Handle, GWLP_USERDATA, (LONG_PTR)this);
  if(FullScreen)
    MakeParent(StemWin);
#if defined(SSE_ACSI_MNGR)
  int w=GetCheckBoxSize(Font,((acsi)?T("&Enable ACSI Hard Drives"):T("&Enable GEMDOS Hard Drives"))).Width;
  Win=CreateWindow("Button",((acsi) ? T("&Enable ACSI Hard Drives"):T("&Enable GEMDOS Hard Drives")),
                   WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,10,10,w,CharHeight,Handle,
                   (HMENU)IDC_ENABLE_HD,hInstance,NULL);
  SendMessage(Win,BM_SETCHECK,(UINT_PTR)((acsi&&!SSEOptions.Acsi||!acsi&&DisableHardDrives)
                                         ? BST_UNCHECKED : BST_CHECKED),0);
#else
  int w=GetCheckBoxSize(Font,T("&Disable All Hard Drives")).Width;
  Win=CreateWindow("Button",T("&Disable All Hard Drives"),WS_CHILD|WS_VISIBLE
    |WS_TABSTOP|BS_AUTOCHECKBOX,10,10,w,23,Handle,(HMENU)IDC_ENABLE_HD,hInstance,NULL);
  SendMessage(Win,BM_SETCHECK,(UINT)(DisableHardDrives
    ?BST_CHECKED:BST_UNCHECKED),0);
#endif
  SendMessage(Win,WM_SETFONT,(UINT_PTR)Font,0);
  HWND x=CreateWindow("Button",T("&Add"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
                      0,0,GUIMUL(75),CharHeight,Handle,(HMENU)IDC_ADD_HD,hInstance,NULL);
  SendMessage(x,WM_SETFONT,(UINT_PTR)Font,0);
  if(acsi)
  {
    x=CreateWindow("Button",T("&Create"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
                   0,0,GUIMUL(75),CharHeight,Handle,(HMENU)11,hInstance, NULL);
    SendMessage(x,WM_SETFONT,(UINT_PTR)Font,0);
  }
  int Wid=get_text_width(T("When drive A is empty boot from"));
  if(!acsi)
  {
    Win=CreateWindow("Static",T("When drive A is empty boot from"),WS_CHILD|WS_VISIBLE,
                     10,44,Wid,CharHeight,Handle,(HMENU)91,hInstance,NULL);
    SendMessage(Win,WM_SETFONT,(UINT_PTR)Font,0);
    Win=CreateWindow("Combobox","",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST
                     |CBS_HASSTRINGS,15+Wid,GUIMUL(40),GUIMUL(40),GUIMUL(300),
                     Handle,(HMENU)92,hInstance,NULL);
    SendMessage(Win,WM_SETFONT,(UINT_PTR)Font,0);
    char DriveName[8];
    DriveName[1]=':';DriveName[2]='\0';
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Off"));
    for(DWORD i=0;i<MAX_GEMDOS_HARDDRIVES;i++) 
    {
      DriveName[0]=(char)('C'+i);
      SendMessage(Win,CB_ADDSTRING,0,(LPARAM)DriveName);
    }
#ifndef DISABLE_STEMDOS
    SendMessage(Win,CB_SETCURSEL,Stemdos.BootDrive-1,0);
#else
    SendMessage(Win,CB_SETCURSEL,0,0);
    EnableWindow(Win,0);
#endif
  }
  Win=CreateWindow("Button",T("OK"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,290,40,100,23,
                   Handle,(HMENU)IDOK,hInstance,NULL);
  SendMessage(Win,WM_SETFONT,(UINT_PTR)Font,0);
  Win=CreateWindow("Button",T("Cancel"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,400,40,100,23,
                   Handle,(HMENU)IDCANCEL,hInstance,NULL);
  SendMessage(Win,WM_SETFONT,(UINT_PTR)Font,0);
  for(BYTE i=0;i<nDrives;i++)
    CreateDriveControls(i);
  SetWindowHeight();
  nOldDrives=nDrives;
  OldDriveInfo=(nDrives) ? new THardDiskInfo[nOldDrives] : NULL;
  for(BYTE i=0;i<nDrives;i++) 
    OldDriveInfo[i]=HDrive[i];
#if defined(SSE_ACSI_MNGR)
  if(acsi)
    DisableHardDrives=!SSEOptions.Acsi;
  OldDisableHardDrives=DisableHardDrives;
#else
  OldDisableHardDrives=DisableHardDrives;
#endif
  ShowWindow(Handle,SW_SHOW);
  SetFocus(GetDlgItem(Handle,(nDrives) ? 100 : IDOK));
#endif//WIN32

#ifdef UNIX

#if defined(SSE_ACSI_MNGR)
  if(StandardShow(590,10+60+(nDrives*30)+5,
    (acsi?T("ACSI Hard Drives"):T("GEMDOS Hard Drives")),
    ICO16_HARDDRIVE,0,(LPWINDOWPROC)WinProc,true))
  {
    return;
  }
#else
  if(StandardShow(590,10+60+(nDrives*30)+5,T("Hard Drives"),
      ICO16_HARDDRIVE,0,(LPWINDOWPROC)WinProc,true)) return;
#endif

  XSizeHints *pHints=XAllocSizeHints(); //?
  if(pHints)
  {
    pHints->flags=PMinSize | PMaxSize;
    pHints->min_width=590;
    pHints->min_height=10+60+5;
    pHints->max_width=590;
    pHints->max_height=10+60+(MAX_GEMDOS_HARDDRIVES*30)+5;
    XSetWMSizeHints(XD,Handle,pHints,XA_WM_NORMAL_HINTS);
    XSetWMSizeHints(XD,Handle,pHints,XA_WM_ZOOM_HINTS);
    XFree(pHints);
  }

  int y=10;
  for(DWORD n=0;n<nDrives;n++,y+=LineHeight)
    CreateDriveControls(n);
  
#if defined(SSE_ACSI_MNGR)
  all_off_but.create(XD,Handle,10,y,0,25,button_notify_proc,this,BT_CHECKBOX,
    ((acsi) ? StripAndT("&Enable ACSI Hard Drives")
            : StripAndT("&Enable GEMDOS Hard Drives")),400,BkCol);  
  all_off_but.set_check(acsi&&SSEOptions.Acsi||!acsi&&!DisableHardDrives);
#else
  all_off_but.create(XD,Handle,10,y,0,25,button_notify_proc,this,BT_CHECKBOX,
    StripAndT("&Disable All Hard Drives"),400,BkCol);  
#endif  
  SetWindowGravity(XD,all_off_but.handle,SouthEastGravity);
  
  new_but.create(XD,Handle,345,y,235,25,button_notify_proc,this,BT_TEXT,
    StripAndT("&New Hard Drive"),401,BkCol);
  SetWindowGravity(XD,new_but.handle,SouthEastGravity);
  
  y+=LineHeight;
  
#if defined(SSE_ACSI_MNGR)
  if(!acsi)
#endif
  {
    boot_label.create(XD,Handle,10,y,0,25,NULL,this,BT_LABEL,
      T("When drive A is empty boot from"),402,BkCol);
    SetWindowGravity(XD,boot_label.handle,SouthEastGravity);
    
    boot_dd.make_empty();
    boot_dd.additem("Off");
    for(int i=0;i<24;i++)
      boot_dd.additem(Str(char('C'+i))+":");
#ifndef DISABLE_STEMDOS
    boot_dd.changesel(Stemdos.BootDrive-1);
#endif
    boot_dd.create(XD,Handle,10+boot_label.w+5,y,50,200,NULL);
    SetWindowGravity(XD,boot_dd.handle,SouthEastGravity);
  }//if(!acsi)

  ok_but.create(XD,Handle,410,y,80,25,button_notify_proc,this,BT_TEXT,
    T("Ok"),403,BkCol);
  SetWindowGravity(XD,ok_but.handle,SouthEastGravity);
  
  cancel_but.create(XD,Handle,500,y,80,25,button_notify_proc,this,BT_TEXT,
    T("Cancel"),404,BkCol);
  SetWindowGravity(XD,cancel_but.handle,SouthEastGravity);

  XMapWindow(XD,Handle);

  nOldDrives=nDrives;
  if(nDrives)
    OldDriveInfo=new THardDiskInfo[nOldDrives];
  else
    OldDriveInfo=NULL;
  for(DWORD i=0;i<nDrives;i++)
    OldDriveInfo[i]=HDrive[i];

  if(StemWin)
    DiskMan.HardBut.set_check(true);

#endif
}


void THardDiskManager::CreateDriveControls(INT_PTR Idx) {
  int y=10+((int)Idx*LineHeight);

#ifdef WIN32
  HWND Win;
  if(GetDlgItem(Handle,300+(int)Idx)!=NULL)
    return;
  Win=CreateWindow("Combobox","",WS_CHILDWINDOW|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST
                   |CBS_HASSTRINGS,10,y,GUIMUL(40),GUIMUL(300),Handle,(HMENU)(300+Idx),hInstance,NULL);
  SendMessage(Win,WM_SETFONT,(UINT_PTR)Font,0);
  char DriveName[8];
  DriveName[1]=(acsi) ? '\0' : ':';
  DriveName[2]='\0';
  SendMessage(Win,CB_ADDSTRING,0,(LPARAM)CStrT("Off"));
  for(int i=0;i<((acsi) ? MAX_ACSI_DEVICES : MAX_GEMDOS_HARDDRIVES);i++) 
  {
    if(acsi)
      DriveName[0]=(char)('0'+i); // devices
    else
      DriveName[0]=(char)('C'+i); // partitions
    SendMessage(Win,CB_ADDSTRING,0,(LPARAM)DriveName);
  }
  SendMessage(Win,CB_SETCURSEL,(HDrive[Idx].Letter-'C')+1,0);
  Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Edit",HDrive[Idx].Path,WS_CHILD|WS_VISIBLE|WS_TABSTOP
                     |ES_AUTOHSCROLL,GUIMUL(55),y,GUIMUL(205),CharHeight,
                     Handle,(HMENU)(100+Idx),hInstance,NULL);
  SendMessage(Win,WM_SETFONT,(UINT_PTR)Font,0);
  SendMessage(Win,EM_LIMITTEXT,SSE_MAX_PATH,0);
  int Len=(int)SendMessage(Win,WM_GETTEXTLENGTH,0,0);
  SendMessage(Win,EM_SETSEL,Len,Len);
  SendMessage(Win,EM_SCROLLCARET,0,0);
  Win=CreateWindow("Button",T("Browse"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE,
                   GUIMUL(265),y,GUIMUL(75),CharHeight,Handle,(HMENU)(150+Idx),hInstance,NULL);
  SendMessage(Win,WM_SETFONT,(UINT_PTR)Font,0);
  Win=CreateWindow("Button",T("Open"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
                   GUIMUL(345),y,GUIMUL(75),CharHeight,Handle,(HMENU)(250+Idx),hInstance,NULL);
  SendMessage(Win,WM_SETFONT,(UINT_PTR)Font,0);
  Win=CreateWindow("Button",T("Remove"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,
                   GUIMUL(425),y,GUIMUL(75),CharHeight,Handle,(HMENU)(200+Idx),hInstance,NULL);
  SendMessage(Win,WM_SETFONT,(UINT_PTR)Font,0);
  SetWindowHeight();
#endif//WIN32

#ifdef UNIX
  drive_dd[Idx].make_empty();
  drive_dd[Idx].additem("Off");
  
#if defined(SSE_ACSI_MNGR)
  for(int i=0;i<((acsi) ? MAX_ACSI_DEVICES : 24);i++) 
  {
   if(acsi)
     drive_dd[Idx].additem(Str(char('0'+i))+":");
    else
     drive_dd[Idx].additem(Str(char('C'+i))+":");
  }
#else  
  for(int i=0;i<24;i++) drive_dd[Idx].additem(Str(char('C'+i))+":");
#endif
  drive_dd[Idx].changesel((HDrive[Idx].Letter-'C')+1);
  drive_dd[Idx].create(XD,Handle,10,y,50,200,NULL);

  drive_ed[Idx].create(XD,Handle,70,y,240,25,NULL);
  drive_ed[Idx].set_text(HDrive[Idx].Path+"/");

  drive_browse_but[Idx].create(XD,Handle,320,y,80,25,button_notify_proc,this,
    BT_TEXT,T("Browse"),Idx*10,BkCol);

  drive_open_but[Idx].create(XD,Handle,410,y,80,25,button_notify_proc,this,
    BT_TEXT,T("Open"),Idx*10+2,BkCol);

  drive_remove_but[Idx].create(XD,Handle,500,y,80,25,button_notify_proc,this,
    BT_TEXT,T("Remove"),Idx*10+1,BkCol);
#endif//UNIX
}


void THardDiskManager::SetWindowHeight() {

#ifdef WIN32
  RECT rc;
  SetWindowPos(Handle,0,0,0,GUIMUL(516),GUIMUL(80)+GuiSM.cy_caption()
               +(nDrives*LineHeight),SWP_NOZORDER | SWP_NOMOVE | SWP_NOCOPYBITS);
  SetWindowPos(GetDlgItem(Handle,IDC_ENABLE_HD),0,10,12+(nDrives*LineHeight),0,0,
               SWP_NOZORDER | SWP_NOSIZE | SWP_NOCOPYBITS);
  SetWindowPos(GetDlgItem(Handle,IDC_ADD_HD),0,GUIMUL(265),12+(nDrives*LineHeight),0,0,
               SWP_NOZORDER | SWP_NOSIZE | SWP_NOCOPYBITS);
  SetWindowPos(GetDlgItem(Handle,11),0,GUIMUL(345),12+(nDrives*LineHeight),0,0,
               SWP_NOZORDER | SWP_NOSIZE | SWP_NOCOPYBITS);
  GetClientRect(GetDlgItem(Handle,91),&rc);
  SetWindowPos(GetDlgItem(Handle,91),0,10,GUIMUL(46)+(nDrives*LineHeight),0,0,
               SWP_NOZORDER | SWP_NOSIZE | SWP_NOCOPYBITS);
  SetWindowPos(GetDlgItem(Handle,92),0,15+rc.right,GUIMUL(42)+
               (nDrives*LineHeight),0,0,SWP_NOZORDER | SWP_NOSIZE | SWP_NOCOPYBITS);
  SetWindowPos(GetDlgItem(Handle,IDOK),0,GUIMUL(290),GUIMUL(42)
               +(nDrives*LineHeight),0,0,SWP_NOZORDER | SWP_NOSIZE | SWP_NOCOPYBITS);
  SetWindowPos(GetDlgItem(Handle,IDCANCEL),0,GUIMUL(400),GUIMUL(42)
               +(nDrives*LineHeight),0,0,SWP_NOZORDER | SWP_NOSIZE | SWP_NOCOPYBITS);
#endif

#ifdef UNIX
  XResizeWindow(XD,Handle,590,10+(nDrives*30)+60+5);
#endif
}


void THardDiskManager::GetDriveInfo() {
  for(BYTE i=0;i<nDrives;i++)
  {
#ifdef WIN32
    HDrive[i].Path.SetLength(SSE_MAX_PATH);
    SendMessage(GetDlgItem(Handle,100+i),WM_GETTEXT,SSE_MAX_PATH,(LPARAM)HDrive[i].Path.Text);
    NO_SLASH(HDrive[i].Path);
    if(HDrive[i].Path.Length()==1) 
      HDrive[i].Path+=":";
    HDrive[i].Letter=char(SendMessage(GetDlgItem(Handle,300+i),CB_GETCURSEL,0,0)+'C'-1);
#endif

#ifdef UNIX
    HDrive[i].Letter='B'+drive_dd[i].sel;
    HDrive[i].Path=drive_ed[i].text;
    NO_SLASH(HDrive[i].Path.Text);
#endif
  }
}


void THardDiskManager::Hide() {
  
#ifdef UNIX
  if(XD==NULL)
    return;
#endif

  if(Handle==NULL)
    return;

  if(ApplyChanges) // only if pressed OK
  {

#if defined(SSE_GUI_QUICKSETTINGS)
    if(OptionBox.IsVisible() && OptionBox.Page==PAGE_QUICK)
    {
      HWND icon_handle=NULL;
#if defined(SSE_ACSI_MNGR)
      if(acsi)
      {
        icon_handle=GetDlgItem(OptionBox.Handle,IDC_HDACSI);
        SendMessage(icon_handle,BM_SETCHECK,SSEOptions.Acsi,0);
      }
      else // only if !acsi!
#endif
      {
        icon_handle=GetDlgItem(OptionBox.Handle,IDC_HDGEMDOS);
        SendMessage(icon_handle,BM_SETCHECK,!HardDiskMan.DisableHardDrives,0);
      }
    }
#endif

    GetDriveInfo();
    for(BYTE i=0;i<nDrives;i++) 
    {
#ifdef WIN32
      if(HDrive[i].Path.IsEmpty())
      {
        Alert(T("One of the mounted paths is empty!"),T("Empty Path"),MB_ICONEXCLAMATION);
        return;
      }
      else 
      {
#if defined(SSE_LONG_PATH)
        EasyStr sTmp=HDrive[i].Path;
        PathPrePend(sTmp,false);
        UINT Type=GetDriveType(EasyStr(sTmp[0])+":\\");
#else
        UINT Type=GetDriveType(EasyStr(HDrive[i].Path[0])+":\\");
#endif
        if(Type==1) 
        {
          Alert(EasyStr(HDrive[i].Path[0])+" "+T("is not a valid drive letter."),
                T("Invalid Drive"),MB_ICONEXCLAMATION);
          return;
        }
        else if(Type!=DRIVE_REMOVABLE && Type!=DRIVE_CDROM &&!acsi)
        {
          DWORD Attrib=GetFileAttributes(HDrive[i].Path);
          if(Attrib==INVALID_FILE_ATTRIBUTES)
          {
            if(Alert(HDrive[i].Path+" "+T("does not exist. Do you want to create it?"),T("New Folder?"),
                     MB_ICONQUESTION|MB_YESNO)==IDYES)
            {
              if(!CreateDirectory(HDrive[i].Path,NULL))
              {
                Alert(T("Could not create the folder")+" "+HDrive[i].Path,
                      T("Invalid Path"),MB_ICONEXCLAMATION);
                return;
              }
            }
            else 
              return;
          }
          else if((Attrib&FILE_ATTRIBUTE_DIRECTORY)==0) 
          {
            Alert(HDrive[i].Path+" "+T("is not a folder."),T("Invalid Path"),MB_ICONEXCLAMATION);
            return;
          }
        }
#if defined(SSE_ACSI_MNGR)
        else if(acsi && AcsiHdc[i].Init(i,HDrive[i].Path))
          SSEConfig.AcsiImg=true;
#endif
      }
#endif//WIN32

#ifdef UNIX
      if(HDrive[i].Path.Text[0])
      {
#if defined(SSE_ACSI_MNGR)
        if(acsi)
        {
          if(AcsiHdc[i].Init(i,HDrive[i].Path))
            SSEConfig.AcsiImg=true;
        }
        else
#endif        
        if(!Exists(HDrive[i].Path))
        {
          if(Alert(HDrive[i].Path+" "
            +T("does not exist. Do you want to create it?"),T("New Folder?"),
            MB_ICONQUESTION|MB_YESNO)==IDYES)
          {
            if(CreateDirectory(HDrive[i].Path,NULL)==0)
            {
              Alert(T("Could not create the folder")+" "+HDrive[i].Path,
                T("Invalid Path"),MB_ICONEXCLAMATION);
              return;
            }
          }
          else
            return;
        }
      }
#endif//UNIX
    }//nxt
    if(!acsi)
    {
      //Remove old stemdos drives from ST memory
      DWORD DrvMask=SafeLPeek(SV_DRVBITS);
      for(DWORD i=0;i<nOldDrives;i++)
      {
#ifdef WIN32
        if(OldDriveInfo[i].Letter>='C') // Don't remove it if off!
          DrvMask&=~(1<<(2+OldDriveInfo[i].Letter-'C'));
#endif
#ifdef UNIX
        DrvMask &= ~(1 << (OldDriveInfo[i].Letter-'A'));
#endif
      }
      SafeLPoke(SV_DRVBITS,DrvMask);
      update_mount();
#ifndef DISABLE_STEMDOS
#ifdef WIN32
      Stemdos.BootDrive=(BYTE)SendDlgItemMessage(Handle,92,CB_GETCURSEL,0,0)+1;
#endif
#ifdef UNIX
      Stemdos.BootDrive=boot_dd.sel+1;
#endif
      Stemdos.UpdateDrvbits();
      Stemdos.CheckPaths();
#endif
    }//if(acsi)
  }//if(ApplyChanges) 
  else
  {// cancel
    nDrives=nOldDrives;
    for(DWORD i=0;i<nOldDrives;i++) 
      HDrive[i]=OldDriveInfo[i];
#if defined(SSE_ACSI_MNGR)
    DisableHardDrives=OldDisableHardDrives;
    if(acsi)
      SSEOptions.Acsi=!OldDisableHardDrives;
#else
    DisableHardDrives=OldDisableHardDrives;
#endif
    update_mount();
  }//if(ApplyChanges) 
  ApplyChanges=false;
  if(OldDriveInfo) 
    delete[] OldDriveInfo;

#ifdef WIN32
  if(DiskMan.Handle)
  {
    EnableWindow(DiskMan.Handle,TRUE);
    if(FullScreen)
      SetFocus(DiskMan.Handle);
    else
      SetForegroundWindow(DiskMan.Handle);
  }
  ShowWindow(Handle,SW_HIDE);
  DestroyWindow(Handle);Handle=NULL;
  if(DiskMan.Handle)
    PostMessage(DiskMan.Handle,WM_USER,0,0);
  ManageWindowClasses(SD_UNREGISTER);
#endif//WIN32

#ifdef UNIX
  StandardHide();
  if(StemWin)
    DiskMan.HardBut.set_check(0);
#endif
}


#ifdef WIN32

void THardDiskManager::ManageWindowClasses(bool Unreg) {
  char *ClassName="Steem Hard Disk Manager";
  if(Unreg)
    UnregisterClass(ClassName,hInstance);
  else
    RegisterMainClass(WndProc,ClassName,RC_ICO_HARDDRIVE16);
}


LRESULT CALLBACK THardDiskManager::WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar) {
  LRESULT Ret=DefStemDialogProc(Win,Mess,wPar,lPar);
  if(StemDialog_RetDefVal) 
    return Ret;
  THardDiskManager *This=(THardDiskManager*)GetWindowLongPtr(Win,GWLP_USERDATA);
  switch(Mess) {
  case WM_COMMAND: {
    WORD ID=LOWORD(wPar);
    if(ID==IDC_ADD_HD) // Add/New
    {
      if(HIWORD(wPar)==BN_CLICKED) 
      {
        if(This->acsi && This->nDrives<MAX_ACSI_DEVICES
           ||!This->acsi && This->nDrives<MAX_GEMDOS_HARDDRIVES) 
        {
          This->GetDriveInfo();
          if(This->acsi)
            This->NewDrive(AcsiDir);
          else
            This->NewDrive(UsersPath);
          This->CreateDriveControls(This->nDrives-1);
          SetFocus(GetDlgItem(Win,100+This->nDrives-1));
          SendMessage(GetDlgItem(Win,IDC_ADD_HD),BM_SETSTYLE,0,TRUE);
          SendMessage(GetDlgItem(Win,IDOK),BM_SETSTYLE,1,TRUE);
        }
      }
    }
    else if(ID==11) // create new hdd img file of zeroes
    {
      char *fstypes=FSTypes(4,NULL);
      EasyStr sFilepath=FileSelect(This->Handle,T("Select Disk Image"),
        This->HDrive[0].Path,fstypes,1,false,"img");
      free(fstypes);
      if(sFilepath.NotEmpty())
      {
        EasyStr sMegas;
        if(InputPrompt_Choose(This->Handle,T("How many megabytes?"),sMegas))
        {
          int nMegas=atoi(sMegas.Text);
          // HD manufacturers' definition of mega = 1000000 bytes, also for ICD
          int nSecs=nMegas*1000000/SECTOR_SIZE;
          FILE* fp=fopen(sFilepath.Text,"wb");
          if(fp!=NULL)
          {
            BYTE data[SECTOR_SIZE];
            memset(data,0,SECTOR_SIZE);
            for(int i=0;i<nSecs;i++)
              FWRITE(data,SECTOR_SIZE,1,fp); // fill with 0
            fclose(fp);
            Alert(T("ACSI image created, now you may add it\r\n")+T("It only contains zeroes"),
                  T("Warning"),MB_OK); // You're on your own!
          }
        }
      }
    }
    else if(ID==IDC_ENABLE_HD) 
    {
      bool bEnabled=(SendMessage((HWND)lPar,BM_GETCHECK,0,0)==BST_CHECKED);
      This->DisableHardDrives=!bEnabled;
#if defined(SSE_ACSI_MNGR)
      if(This->acsi)
        SSEOptions.Acsi=bEnabled;
#endif
      REFRESH_STATUS_BAR_GX;
    }
    else if(ID==IDOK||ID==IDCANCEL) 
    {
      if(HIWORD(wPar)==BN_CLICKED) 
      {
        if(ID==IDOK)
          This->ApplyChanges=true;
        PostMessage(Win,WM_CLOSE,0,0);
      }
    }
    else if(ID>=150&&ID<300) 
    {
      if(HIWORD(wPar)==BN_CLICKED) 
      {
        if(ID<200) //160
        {
          ID-=150;
          // Browse
          SendMessage((HWND)lPar,BM_SETCHECK,1,0);
          EnableAllWindows(false,Win);
          This->GetDriveInfo();
          EasyStr NewPath;
#if defined(SSE_ACSI_MNGR)
          if(This->acsi)
          {
            char* fstypes=FSTypes(4,NULL);
            NewPath=FileSelect(NULL,T("Select Disk Image"),AcsiDir.Text,fstypes,1,TRUE);
            free(fstypes);
          }
          else
#endif
            NewPath=ChooseFolder((FullScreen)?StemWin:Win,T("Pick a Folder"),This->HDrive[ID].Path);
          if(NewPath.NotEmpty())
          {
            SendMessage(GetDlgItem(This->Handle,100+ID),WM_SETTEXT,0,(LPARAM)NewPath.Text);
            if(This->acsi)
            {
              AcsiDir=NewPath;
              RemoveFileNameFromPath(AcsiDir.Text,true);
            }
          }
          SetForegroundWindow(Win);
          EnableAllWindows(true,Win);
          SetFocus((HWND)lPar);
          SendMessage((HWND)lPar,BM_SETCHECK,0,0);
        }
        else if(ID<250) 
        {
          // Remove
          ID-=200;
#if defined(SSE_LONG_PATH)
          EasyStr sText;
          sText.SetLength(SSE_MAX_PATH);
          char *Text=sText.Text;
#else
          char Text[MAX_PATH+1];
#endif
          This->nDrives--;
          for(DWORD i=ID;i<This->nDrives;i++) 
          {
            SendMessage(GetDlgItem(This->Handle,100+i+1),WM_GETTEXT,SSE_MAX_PATH,(LPARAM)Text);
            SendMessage(GetDlgItem(This->Handle,100+i),WM_SETTEXT,0,(LPARAM)Text);
            SendMessage(GetDlgItem(This->Handle,300+i),CB_SETCURSEL,
            SendMessage(GetDlgItem(This->Handle,300+i+1),CB_GETCURSEL,0,0),0);
          }
          DestroyWindow(GetDlgItem(This->Handle,100+This->nDrives));
          DestroyWindow(GetDlgItem(This->Handle,150+This->nDrives));
          DestroyWindow(GetDlgItem(This->Handle,200+This->nDrives));
          DestroyWindow(GetDlgItem(This->Handle,250+This->nDrives));
          DestroyWindow(GetDlgItem(This->Handle,300+This->nDrives));
          This->GetDriveInfo();
          This->SetWindowHeight();
          if(This->nDrives)
            SetFocus(GetDlgItem(This->Handle,200+MIN(ID,(WORD)(This->nDrives-1))));
          else
            SetFocus(GetDlgItem(Win,IDOK));
          SendMessage(GetFocus(),BM_SETSTYLE,1,TRUE);
        }
        else 
        {
          ID-=250;
          This->GetDriveInfo();
          ShellExecute(NULL,NULL,This->HDrive[ID].Path,"","",SW_SHOWNORMAL);
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
  case DM_GETDEFID:
    return MAKELONG(IDOK,DC_HASDEFID);
  }
  return DefWindowProc(Win,Mess,wPar,lPar);
}

#endif//WIN32

#ifdef UNIX

void THardDiskManager::RemoveLine(int dn) {
  for(DWORD n=dn;n<nDrives-1;n++)
  {
    // copy info from line n+1 to line n
    drive_dd[n].changesel(drive_dd[n+1].sel);
    drive_dd[n].draw();
    drive_ed[n].set_text(drive_ed[n+1].text);
  }
  nDrives--;
  drive_dd[nDrives].destroy(&(drive_dd[nDrives]));
  drive_ed[nDrives].destroy(&(drive_ed[nDrives]));
  drive_browse_but[nDrives].destroy(&(drive_browse_but[nDrives]));
  drive_open_but[nDrives].destroy(&(drive_open_but[nDrives]));
  drive_remove_but[nDrives].destroy(&(drive_remove_but[nDrives]));
  SetWindowHeight();
}


int THardDiskManager::button_notify_proc(hxc_button *But,int Mess,int *Inf) {
  THardDiskManager *This=(THardDiskManager*)But->owner;
  if(Mess==BN_CLICKED)
  {
    switch(But->id) {
    case 400:
#if defined(SSE_ACSI_MNGR)
      This->DisableHardDrives=!But->checked;
      if(This->acsi)
        SSEOptions.Acsi=!This->DisableHardDrives;
#else      
      This->DisableHardDrives=But->checked;
#endif
      break;
    case 401:
#if defined(SSE_ACSI_MNGR)
      if(This->acsi && This->nDrives<MAX_ACSI_DEVICES
        ||!This->acsi && This->nDrives<MAX_GEMDOS_HARDDRIVES) 
#else      
      if(This->nDrives<MAX_GEMDOS_HARDDRIVES)
#endif
      {
        This->GetDriveInfo();
        This->NewDrive(UsersPath);
        This->CreateDriveControls(This->nDrives-1);
        This->SetWindowHeight();
      }	
      break;
    case 403: //OK
      This->ApplyChanges=true;
      //no break
    case 404: //Cancel
      This->Hide();
      break;			
    default:
    {
      if(But->id<MAX_GEMDOS_HARDDRIVES*10)
      {
        int r=((But->id)%10);
        int dn=((But->id)/10);
        if(r==0)
        { //browse
          char*path=This->drive_ed[dn].text.Text;
#if defined(SSE_ACSI_MNGR)
          if(This->acsi)
          {
            fileselect.set_corner_icon(&Ico16,ICO16_HARDDRIVE);
            EasyStr new_path=fileselect.choose(XD,UsersPath,path,
              T("Select ACSI Image"),FSM_LOAD|FSM_LOADMUSTEXIST,
              acsi_parse_routine,".img");
            This->drive_ed[dn].set_text(new_path);
          }
          else
#endif
          {
            fileselect.set_corner_icon(&Ico16,ICO16_FOLDER);
            EasyStr new_path=fileselect.choose(XD,path,"",T("Pick a Folder"),
              FSM_CHOOSE_FOLDER | FSM_CONFIRMCREATE,folder_parse_routine,"");
            if(new_path[0])
            {
              NO_SLASH(new_path); // this removes the last / if there's one
              This->drive_ed[dn].set_text(new_path+"/"); // this adds a /
            }
          }
        }
        else if(r==1)  //remove
          This->RemoveLine(dn);
        else if(r==2)  // open
            shell_execute(Comlines[COMLINE_FM],
              Str("[PATH]\n")+This->drive_ed[dn].text);
      }//if
    }//case
    }//sw
  }
  return 0;
}


int THardDiskManager::WinProc(THardDiskManager *This,Window Win,XEvent*Ev) {
  switch(Ev->type) {
  case ClientMessage:
    if(Ev->xclient.message_type==hxc::XA_WM_PROTOCOLS)
    {
      if(Atom(Ev->xclient.data.l[0])==hxc::XA_WM_DELETE_WINDOW)
        This->Hide();
    }
    break;
  }
  return PEEKED_MESSAGE;
}

#endif//UNIX

#undef LOGSECTION
#undef CharHeight
#undef LineHeight
#undef IDC_ADD_HD
#undef IDC_ENABLE_HD
