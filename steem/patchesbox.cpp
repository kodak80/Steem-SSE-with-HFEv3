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
FILE: patchesbox.cpp
DESCRIPTION: The code for Steem's patches dialog that allows the user to
apply patches to fix ST programs that don't work or are incompatible with
Steem. Patches can also be used to defeat a game protection.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <dirsearch.h>
#include <steemh.h>
#include <patchesbox.h>
#include <gui.h>
#include <choosefolder.h>
#include <display.h>
#include <emulator.h>
#include <computer.h>
#include <translate.h>

enum EPatchBox {
  IDC_PATCH_LIST=100,IDC_PATCH_DESCRIPTION=200,IDC_PATCH_APPLYWHEN=210,
  IDC_PATCH_VERSION=220,IDC_PATCH_AUTHOR=230,IDC_PATCH_APPLY=300,
  IDP_PATCH=401,IDC_PATCH_CHOOSE=402
};


TPatchesBox::TPatchesBox() {
  Section="Patches";
#ifdef WIN32
#if !defined(SSE_420R4)
  Left=(GuiSM.cx_screen()-456)/2;
  Top=(GuiSM.cy_screen()-(411+GuiSM.cy_caption()))/2;
  FSLeft=(640-456)/2;
  FSTop=(480-(411+GuiSM.cy_caption()))/2;
#endif
#endif
#ifdef UNIX
  PatchList.owner=this;
  ApplyBut.owner=this;
  PatchDirBut.owner=this;
#endif
}


void TPatchesBox::RefreshPatchList() {
  if(Handle==WINDOWTYPE(0)) 
    return;
#ifndef SSE_LEAN_AND_MEAN
  EasyStr ThisVerText=(char*)stem_version_text;
  for(INT_PTR n=0;n<ThisVerText.Length();n++) 
  { // Cut off beta number
    if(ThisVerText[n]<'0'||ThisVerText[n]>'9')
    {
      if(ThisVerText[n]!='.')
      {
        ThisVerText[n]=0;
        break;
      }
    }
  }
  double ThisVer=atof(ThisVerText);
#endif  
#ifdef WIN32
  EasyStringList sl;
  SendDlgItemMessage(Handle,IDC_PATCH_LIST,LB_RESETCONTENT,0,0);
#endif
#ifdef UNIX
  EasyStringList &sl=PatchList.sl;
  sl.DeleteAll();
#endif
  sl.Sort=eslSortByNameI;
  DirSearch ds;
  if(ds.Find(PatchDir+SLASH "*.stp")) 
  {
    do {
#ifndef SSE_LEAN_AND_MEAN
      if(ThisVer<atof(GetCSFStr("Text","Obsolete","9999",PatchDir+SLASH+ds.Name))) 
#endif
      {
        *strrchr(ds.Name,'.')='\0'; // remove extension
        if(ds.Name[0]) 
          sl.Add(ds.Name);
      }
    } while(ds.Next());
  }
#ifdef WIN32
  HWND hPatchList=GetDlgItem(Handle,IDC_PATCH_LIST);
#endif
  if(sl.NumStrings) 
  {
    int iSel=-1;
    for(int s=0;s<sl.NumStrings;s++) 
    {
#ifdef WIN32
      SendDlgItemMessage(Handle,IDC_PATCH_LIST,LB_ADDSTRING,0,(LPARAM)sl[s].String);
#endif
      if(IsSameStr_I(sl[s].String,SelPatch)) 
        iSel=s;
    }
    if(iSel==-1) 
    {
      iSel=0;
      SelPatch=sl[0].String;
    }
#ifdef WIN32
    EnableWindow(hPatchList,TRUE);
    SendDlgItemMessage(Handle,IDC_PATCH_LIST,LB_SETCURSEL,iSel,0);
#endif
#ifdef UNIX
    PatchList.changesel(iSel);
    PatchList.draw(true,true);
#endif
  }
  else 
  {
#ifdef WIN32
    EnableWindow(hPatchList,FALSE);
#endif
    SelPatch="";
  }
  ShowPatchFile();
#ifdef WIN32
#ifndef SSE_LEAN_AND_MEAN
  WriteCSFStr(Section,"LastKnownVersion",GetPatchVersion(),globalINIFile);
  SetButtonIcon();
#endif
#endif
}


bool TPatchesBox::PatchPoke(MEM_ADDRESS &ad,int Len,DWORD Data,DWORD DataCheck) {
  // Data:       data to write
  // DataCheck   if not 0, data to read, we write new value only if it matches
  ad&=0xffffff;
  bool ok=false;
  if(ad<himem) 
  {
    switch(Len) {
    case 1:
      if(!DataCheck||PEEK(ad)==(BYTE)DataCheck)
      {
        PEEK(ad)=(BYTE)Data;
        ok=true;
      }
      break;
    case 2:
      if(!DataCheck||DPEEK(ad)==(WORD)DataCheck)
      {
        DPEEK(ad)=(WORD)Data;
        ok=true;
      }
      break;
    case 4:
      if(!DataCheck||LPEEK(ad)==(DWORD)DataCheck)
      {
        LPEEK(ad)=(DWORD)Data;
        ok=true;
      }
      break;
    }
  }
  else if(ad>=MEM_IO_BASE)
  {
    ok=true; // no check for io
    BYTE old_bus_mask=BUS_MASK;
    TRY_M68K_EXCEPTION // thread-safe
      switch(Len) {
      case 1:
        BUS_MASK=(ad&1) ? (BUS_MASK_ACCESS|BUS_MASK_LOBYTE|BUS_MASK_WRITE)
                        : (BUS_MASK_ACCESS|BUS_MASK_HIBYTE|BUS_MASK_WRITE);
        io_write_b(ad,(BYTE)Data);
        break;
      case 2: 
        BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
        io_write_w(ad,(WORD)Data);  
        break;
      case 4:
        BUS_MASK=BUS_MASK_ACCESS|BUS_MASK_WRITE|BUS_MASK_WORD;
        io_write_w(ad,Data>>16);
        io_write_w(ad+2,Data&0xffff);
        break;
      }
    CATCH_M68K_EXCEPTION
      ok=false;
    END_M68K_EXCEPTION
    BUS_MASK=old_bus_mask;
  }
  ad+=Len;
  TRACE("patch %d bytes %06x %x -> %x OK %d\n",Len,ad,DataCheck,Data,ok);
  return ok;
}


void TPatchesBox::ApplyPatch() {
  if(SelPatch.Empty()) 
    return;
  EasyStr pf=PatchDir+SLASH+SelPatch+".stp";
  FILE *fp=fopen(pf,"rb");
  if(fp)
  {
    EasyStr Text;
    int Len=(int)GetFileLength(fp);
    Text.SetLength(Len);
    FREAD(Text.Text,1,Len,fp);
    fclose(fp);
    strupr(Text);
    int NumBytesChanged=0;
    DynamicArray<BYTE> Bytes;
    DynamicArray<BYTE> CheckBytes; // same for bytes to read
    EasyStringList Offsets;
    Offsets.Sort=eslNoSort;
    // the new breed, patch may contain old values too for check
    char *Check=strstr(Text,"\n[CHECK]");
    char *Sect=strstr(Text,"\n[PATCH]");
#ifndef SSE_LEAN_AND_MEAN
    bool WordOnly;
    char *OffsetSect=strstr(Text,"\n[OFFSETS]"); // ?? never seen
    if(OffsetSect) 
    {
      OffsetSect+=2; // skip \n[
      char *OffsetSectEnd=strstr(OffsetSect,"\n[");
      if(OffsetSectEnd==NULL) OffsetSectEnd=Text.Right()+1; // point to NULL
      char *tp=OffsetSect;
      while(tp<OffsetSectEnd) {
        if(*tp==13||*tp==10) *tp=0;
        tp++;
      }
      tp=OffsetSect+strlen(OffsetSect)+1;
      while(tp<OffsetSectEnd) {
        char *next_line=tp+strlen(tp)+1;
        char *eq=strchr(tp,'=');
        if(eq) {
          *eq=0;eq++;
          // Offset name = tp
          WordOnly=false;
          parse_search_string(eq,Bytes,WordOnly);
          MEM_ADDRESS offset_ad=acc_find_bytes(Bytes,WordOnly,0,1);
          if(offset_ad<=0xffffff) {
            while(tp[0]==' ') tp++;
            while(*(tp+strlen(tp)-1)==' ') *(tp+strlen(tp)-1)=0;
            Offsets.Add(tp,(LONG_PTR)offset_ad);
          }
        }
        tp=next_line;
      }
    }
#endif
    bool ReturnLengths;
    if(Sect)
    {
      Sect+=2; // skip \n[
      char *SectEnd=strstr(Sect,"\n[");
      if(SectEnd==NULL) 
        SectEnd=Sect+strlen(Sect); // point to NULL
      char *tp=Sect;
      while(tp<SectEnd) {
        if(*tp==13||*tp==10)
          *tp='\0';
        tp++;
      }
      tp=Sect+strlen(Sect);
      while(*tp=='\0' && tp<SectEnd) // skip cr and or lf
        tp++;
      char *tp2=NULL,*CheckEnd=NULL,*eq2=NULL; // init W4
      if(Check) // we do the same for the CHECK section
      {
        Check+=2; // skip \n[
        CheckEnd=strstr(Check,"\n[");
        if(CheckEnd==NULL) 
          CheckEnd=Check+strlen(Check); // point to NULL
        tp2=Check;
        while(tp2<CheckEnd) {
          if(*tp2==13||*tp2==10)
            *tp2='\0';
          tp2++;
        }
        tp2=Check+strlen(Check);
        while(*tp2=='\0' && tp2<CheckEnd)
          tp2++;
      }//if(Check)
      while(tp<SectEnd) {
        char *next_line=tp+strlen(tp);
        while(*next_line=='\0' && next_line<SectEnd)
          next_line++;
        char *next_line2=NULL;
        if(Check)
        {
          next_line2=tp2+strlen(tp2);
          while(*next_line2=='\0' && next_line2<CheckEnd)
            next_line2++;
        }
        char *eq=strchr(tp,'=');
        if(eq) 
        {
          if(Check)
          {
            eq2=strchr(tp2,'=');
            if(eq2) 
            {
              *eq2='\0';
              eq2++;
              while(tp2[0]==' ')
                tp2++;
              while(*(tp2+strlen(tp2)-1)==' ')
                *(tp2+strlen(tp2)-1)='\0';
              ReturnLengths=true;
              parse_search_string(eq2,CheckBytes,ReturnLengths);
            }
            else
              Check=NULL;
          }//if(Check)
          *eq='\0';
          eq++;
#ifndef SSE_LEAN_AND_MEAN
          // tp can = Off+$x= or Off= or $x=.
          MEM_ADDRESS offset_ad=0xffffffff;
          int dir=-1;
          char *sym=strchr(tp,'-');
          if(sym==NULL) 
            sym=strchr(tp,'+'),dir=1;
          if(sym) 
            *sym=0;
          // sym points to + or -, dir is 1 for + and -1 for -. If sym==null tp is either Off= or $x=.
#endif
          while(tp[0]==' ')
            tp++;
          while(*(tp+strlen(tp)-1)==' ')
            *(tp+strlen(tp)-1)='\0';
#ifndef SSE_LEAN_AND_MEAN
          for(int i=0;i<Offsets.NumStrings;i++) 
          {
            if(IsSameStr(Offsets[i].String,tp)) 
            {
              offset_ad=(MEM_ADDRESS)Offsets[i].Data[0];
              if(sym==NULL) 
                dir=0; // no offset
              break;
            }
          }
          // if offset_ad is 0xffffffff then tp hasn't been found. When sym is set
          // this should cause this part of the patch to be skipped. If sym isn't set then
          // we assume tp is an absolute address.
          if(sym)
            tp=sym+1;
          else if(offset_ad==0xffffffff&&dir)
            offset_ad=0; // not found so treat tp as an absolute address
          if(offset_ad<=0xffffff) 
#endif
          {
#ifndef SSE_LEAN_AND_MEAN
            MEM_ADDRESS ad=offset_ad+HexToVal(tp)*dir;  // dir=0 if there is no offset
#else
            MEM_ADDRESS ad=HexToVal(tp);
#endif
            ReturnLengths=true;
            parse_search_string(eq,Bytes,ReturnLengths);
            // Bytes is now a list of bytes in big endian format, between each 
            // byte is a length byte (in nibbles)
            int i=0;
            while(i<Bytes.NumItems) {
              switch(Bytes[i+1]) {
              case 1:
              {
                BYTE CheckData=(Check) ? CheckBytes[i] : 0; //0 = no check
                if(PatchPoke(ad,1,Bytes[i],CheckData))
                  NumBytesChanged+=Bytes[i+1];
                i+=2;
                break;
              }
              case 2:
              {
                WORD CheckData=(Check) ? MAKEWORD(CheckBytes[i+2],CheckBytes[i]) : 0;
                if(PatchPoke(ad,2,MAKEWORD(Bytes[i+2],Bytes[i]),CheckData))
                  NumBytesChanged+=Bytes[i+1];
                i+=4;
                break;
              }
              case 4:
              {
                DWORD CheckData=(Check) ? MAKELONG(MAKEWORD(CheckBytes[i+6],
                  CheckBytes[i+4]),MAKEWORD(CheckBytes[i+2],CheckBytes[i])) : 0;
                if(PatchPoke(ad,4,MAKELONG(MAKEWORD(Bytes[i+6],Bytes[i+4]),
                             MAKEWORD(Bytes[i+2],Bytes[i])),CheckData))
                  NumBytesChanged+=Bytes[i+1];
                i+=8;
              }}//sw
            }//wend
          }
        }
        tp=next_line;
        if(Check)
          tp2=next_line2;
      }
    }//if(Sect)
    if(NumBytesChanged)
      Alert(T("Number of bytes changed: ")+NumBytesChanged,T("Patch applied"),MB_ICONINFORMATION|MB_OK);
    else 
      Alert(T("Data not found"),T("Patch Error"), MB_ICONEXCLAMATION|MB_OK);
#if defined(SSE_STATS)
    StatsStatic.nPatchedBytes+=NumBytesChanged;
#endif
  }
  else
    Alert(T("Couldn't open the patch file!"),T("Patch Error"),MB_ICONEXCLAMATION|MB_OK);
}


void TPatchesBox::GetPatchText(char *File,Str Text[4]) {
  TConfigStoreFile CSF(File);
  char *Name[3]={"Description","ApplyWhen","Version"};
  Str NewSect=T("Patch Text Section=");
  if(NewSect=="Patch Text Section=") 
    NewSect="";
  char *Sect[2]={NewSect,"Text"};
  for(int s=0;s<2;s++) 
  {
    if(Sect[s][0]=='\0')
      s++;
    for(int n=0;n<3;n++)
      if(Text[n].Empty()) 
        Text[n]=CSF.GetStr(Sect[s],Name[n],"");
  }
  Text[3]=CSF.GetStr("Text","PatchAuthor","");
  if(NewSect.NotEmpty()) 
  {
    Str TransBy=CSF.GetStr(NewSect,"PatchAuthor","");
    if(TransBy.NotEmpty()) 
      Text[3]+=Str(WIN_ONLY("\r") "\n")+TransBy;
  }
  CSF.Close();
}


void TPatchesBox::Show() {
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
  Font=SSEConfig.GuiFont();
  int extra_width=(FONT_SIZE>12)?64:0;
#if defined(SSE_GUI_FIX)
  int d=3*GetSystemMetrics(92);
  Handle=CreateWindowEx(WS_EX_CONTROLPARENT,"Steem Patches",T("Patches"),WS_CAPTION|WS_SYSMENU,
                        Left,Top,456+extra_width+d,411+GuiSM.cy_caption()+d,ParentWin,NULL,hInstance,NULL);
#else
  Handle=CreateWindowEx(WS_EX_CONTROLPARENT,"Steem Patches",T("Patches"),WS_CAPTION
    |WS_SYSMENU,Left,Top,456+extra_width,411+GuiSM.cy_caption(),ParentWin,NULL,hInstance,NULL);
#endif
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
  CreateWindow("Static",T("Available Patches"),WS_VISIBLE|WS_CHILD,10,10,
               200,20,Handle,(HMENU)99,hInstance,NULL);
  CreateWindowEx(WS_EX_CLIENTEDGE,"Listbox","",WS_VSCROLL|WS_TABSTOP|WS_VISIBLE
                 |WS_CHILD|LBS_NOINTEGRALHEIGHT|LBS_NOTIFY,10,30,180+extra_width,323,Handle,
                 (HMENU)IDC_PATCH_LIST,hInstance,NULL);
  CreateWindow("Static",T("Description"),WS_VISIBLE|WS_CHILD,
               200+extra_width,10,240,20,Handle,(HMENU)199,hInstance,NULL);
  HWND Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Edit","",WS_VISIBLE|WS_CHILD
                          |ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,200+extra_width,30,240,80,Handle,
                          (HMENU)IDC_PATCH_DESCRIPTION,hInstance,NULL);
  MakeEditNoCaret(Win);
  CreateWindow("Static",T("Apply When"),WS_VISIBLE|WS_CHILD,200+extra_width,120,
               240,20,Handle,(HMENU)209,hInstance,NULL);
  Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Edit","",WS_VISIBLE|WS_CHILD|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,
                     200+extra_width,140,240,40,Handle,(HMENU)IDC_PATCH_APPLYWHEN,hInstance,NULL);
  MakeEditNoCaret(Win);
  CreateWindow("Static",T("Version"),WS_VISIBLE|WS_CHILD,200+extra_width,190,
               240,20,Handle,(HMENU)219,hInstance,NULL);
  Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Edit","",WS_VISIBLE|WS_CHILD|ES_MULTILINE|ES_AUTOVSCROLL
                     |WS_VSCROLL,200+extra_width,210,240,40,Handle,(HMENU)IDC_PATCH_VERSION,hInstance,NULL);
  MakeEditNoCaret(Win);
  CreateWindow("Static",T("Patch Author(s)"),WS_VISIBLE|WS_CHILD,
               200+extra_width,260,240,20,Handle,(HMENU)229,hInstance,NULL);
  Win=CreateWindowEx(WS_EX_CLIENTEDGE,"Edit","",WS_VISIBLE|WS_CHILD|ES_MULTILINE|ES_AUTOVSCROLL
                     |WS_VSCROLL,200+extra_width,280,240,40,Handle,(HMENU)IDC_PATCH_AUTHOR,hInstance,NULL);
  MakeEditNoCaret(Win);
  CreateWindow("Button",T("Apply Now"),WS_TABSTOP|WS_VISIBLE|WS_CHILD|BS_PUSHBUTTON,
               200+extra_width,330,240,23,Handle,(HMENU)IDC_PATCH_APPLY,hInstance,NULL);
  CreateWindow("Static","",WS_CHILD|WS_VISIBLE|SS_ETCHEDHORZ,
               1,360,450,2,Handle,(HMENU)399,hInstance,NULL);
  int Wid=GetTextSize(Font,T("Patch folder")).Width;
  CreateWindow("Static",T("Patch folder"),WS_VISIBLE|WS_CHILD,10,375,Wid,23,
               Handle,(HMENU)400,hInstance,NULL);
  CreateWindowEx(WS_EX_CLIENTEDGE,"Steem Path Display",PatchDir,WS_CHILD|WS_VISIBLE,
                 15+Wid,370,340-(15+Wid),25,Handle,(HMENU)IDP_PATCH,hInstance,NULL);
  CreateWindow("Button",T("Choose"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_CHECKBOX|BS_PUSHLIKE,
               350,371,90,23,Handle,(HMENU)IDC_PATCH_CHOOSE,hInstance,NULL);
  SetWindowAndChildrensFont(Handle,Font);
  RefreshPatchList();
  Focus=GetDlgItem(Handle,IDC_PATCH_LIST);
  ShowWindow(Handle,SW_SHOW);
  SetFocus(Focus);
  if(StemWin!=NULL) 
    PostMessage(StemWin,WM_USER,MSG_UPDATETOOLBOXICONS,0);
#endif//WIN32

#ifdef UNIX
  if(StandardShow(500,420,T("Patches"),ICO16_PATCHES,0,(LPWINDOWPROC)WinProc))
    return;

  PatchLabel.create(XD,Handle,10,10,200,25,NULL,this,BT_LABEL,T("Available Patches"),0,BkCol);

  PatchList.create(XD,Handle,10,35,200,340,ListviewNotifyHandler,this);

  DescLabel.create(XD,Handle,220,10,270,25,NULL,this,BT_LABEL,T("Description"),0,BkCol);

	DescText.create(XD,Handle,220,35,270,80,WhiteCol);

  ApplyWhenLabel.create(XD,Handle,220,125,270,25,NULL,this,BT_LABEL,T("Apply When"),0,BkCol);

	ApplyWhenText.create(XD,Handle,220,150,270,40,WhiteCol);

  VersionLabel.create(XD,Handle,220,200,270,25,NULL,this,BT_LABEL,T("Version"),0,BkCol);

	VersionText.create(XD,Handle,220,225,270,40,WhiteCol);

  AuthorLabel.create(XD,Handle,220,275,270,25,NULL,this,BT_LABEL,T("Patch Author"),0,BkCol);

	AuthorText.create(XD,Handle,220,300,270,40,WhiteCol);

  ApplyBut.create(XD,Handle,220,350,270,25,ButtonNotifyHandler,this,BT_TEXT,
                  T("Apply Now"),100,BkCol);
  
  PatchDirLabel.create(XD,Handle,10,385,0,25,NULL,this,BT_LABEL,T("Patch folder"),0,BkCol);

  PatchDirBut.create(XD,Handle,490,385,0,25,ButtonNotifyHandler,this,BT_TEXT,T("Choose"),200,BkCol);
  PatchDirBut.x-=PatchDirBut.w;
  XMoveWindow(XD,PatchDirBut.handle,PatchDirBut.x,PatchDirBut.y);

  PatchDirText.create(XD,Handle,15+PatchDirLabel.w,385,
  									PatchDirBut.x-10-(15+PatchDirLabel.w),25,NULL,this,
                    BT_STATIC | BT_BORDER_INDENT | BT_TEXT_PATH | BT_TEXT,
                    PatchDir,0,WhiteCol);

	RefreshPatchList();

  if(StemWin)
    PatBut.set_check(true);

  XMapWindow(XD,Handle);
  XFlush(XD);
#endif//UNIX

}


void TPatchesBox::Hide() {
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
#endif
#ifdef UNIX
  if(XD==NULL)
    return;
  StandardHide();
  if(StemWin)
    PatBut.set_check(0);
#endif
}


void TPatchesBox::ShowPatchFile() {

#ifdef WIN32
  BOOL enable=SelPatch.NotEmpty();
  EnableWindow(GetDlgItem(Handle,IDC_PATCH_DESCRIPTION),enable);
  EnableWindow(GetDlgItem(Handle,IDC_PATCH_APPLYWHEN),enable);
  EnableWindow(GetDlgItem(Handle,IDC_PATCH_VERSION),enable);
  EnableWindow(GetDlgItem(Handle,IDC_PATCH_AUTHOR),enable);
  EnableWindow(GetDlgItem(Handle,IDC_PATCH_APPLY),enable);
#endif

  if(SelPatch.NotEmpty()) 
  {
    Str Text[4];
    GetPatchText(PatchDir+SLASH+SelPatch+".stp",Text);

#ifdef WIN32
    SendDlgItemMessage(Handle,IDC_PATCH_DESCRIPTION,WM_SETTEXT,0,(LPARAM)Text[0].Text);
    SendDlgItemMessage(Handle,IDC_PATCH_APPLYWHEN,WM_SETTEXT,0,(LPARAM)Text[1].Text);
    SendDlgItemMessage(Handle,IDC_PATCH_VERSION,WM_SETTEXT,0,(LPARAM)Text[2].Text);
    SendDlgItemMessage(Handle,IDC_PATCH_AUTHOR,WM_SETTEXT,0,(LPARAM)Text[3].Text);
#endif

#ifdef UNIX
  DescText.set_text(Text[0]);      DescText.draw();
  ApplyWhenText.set_text(Text[1]); ApplyWhenText.draw();
  VersionText.set_text(Text[2]);   VersionText.draw();
  AuthorText.set_text(Text[3]);    AuthorText.draw();
#endif
  }
}


#ifdef WIN32

void TPatchesBox::ManageWindowClasses(bool Unreg) {
  char *ClassName="Steem Patches";
  if(Unreg)
    UnregisterClass(ClassName,hInstance);
  else
    RegisterMainClass(WndProc,ClassName,RC_ICO_PATCHES);
}


#ifndef SSE_LEAN_AND_MEAN

EasyStr TPatchesBox::GetPatchVersion() {
  DWORD Attrib=GetFileAttributes(PatchDir);
  if(Attrib<INVALID_FILE_ATTRIBUTES&&(Attrib & FILE_ATTRIBUTE_DIRECTORY)) 
  {
    FILE *fp=fopen(PatchDir+SLASH+"version","rb");
    if(fp) 
    {
      char Text[100];
      ZeroMemory(Text,100);
      FREAD(Text,1,100,fp);
      fclose(fp);
      return EasyStr(Text);
    }
  }
  return "";
}


void TPatchesBox::SetButtonIcon() {
  if(StemWin==NULL) 
    return;
  Str LastVerSeen=GetCSFStr("Patches","LastKnownVersion","",globalINIFile);
  if(LastVerSeen.NotEmpty()) 
  {
    if(NotSameStr_I(GetPatchVersion(),LastVerSeen)) 
    {
      SendDlgItemMessage(StemWin,113,WM_SETTEXT,0,
        LPARAM(Str(RC_ICO_PATCHESNEW).Text));
      return;
    }
  }
  SendDlgItemMessage(StemWin,113,WM_SETTEXT,0,LPARAM(Str(RC_ICO_PATCHES).Text));
}

#endif//#ifndef SSE_LEAN_AND_MEAN


LRESULT CALLBACK TPatchesBox::WndProc(HWND Win,UINT Mess,WPARAM wPar,LPARAM lPar) {
  LRESULT Ret=DefStemDialogProc(Win,Mess,wPar,lPar);
  if(StemDialog_RetDefVal) 
    return Ret;
  TPatchesBox *This=(TPatchesBox*)GetWindowLongPtr(Win,GWLP_USERDATA);
  switch(Mess) {
  case WM_COMMAND:
    switch(LOWORD(wPar)) {
    case IDC_PATCH_LIST:
      if(HIWORD(wPar)==LBN_SELCHANGE) 
      {
        EasyStr NewSel;
        NewSel.SetLength(MAX_PATH);
        SendMessage((HWND)lPar,LB_GETTEXT,SendMessage((HWND)lPar,LB_GETCURSEL,0,0),
                    LPARAM(NewSel.Text));
        if(NotSameStr_I(NewSel,This->SelPatch)) 
        {
          This->SelPatch=NewSel;
          This->ShowPatchFile();
        }
      }
      break;
    case IDC_PATCH_APPLY:
      if(This->SelPatch.NotEmpty()) 
        This->ApplyPatch();
      break;
    case IDC_PATCH_CHOOSE:
      SendMessage((HWND)lPar,BM_SETCHECK,1,0);
      EnableAllWindows(false,Win);
      EasyStr NewFol=ChooseFolder((FullScreen) ? StemWin : Win,T("Pick a Folder"),This->PatchDir);
      if(NewFol.NotEmpty()) 
      {
        NO_SLASH(NewFol);
        SendDlgItemMessage(Win,IDP_PATCH,WM_SETTEXT,0,(LPARAM)NewFol.Text);
        SendDlgItemMessage(Win,IDC_PATCH_DESCRIPTION,WM_SETTEXT,0,(LPARAM)"");
        SendDlgItemMessage(Win,IDC_PATCH_APPLYWHEN,WM_SETTEXT,0,(LPARAM)"");
        SendDlgItemMessage(Win,IDC_PATCH_VERSION,WM_SETTEXT,0,(LPARAM)"");
        SendDlgItemMessage(Win,IDC_PATCH_AUTHOR,WM_SETTEXT,0,(LPARAM)"");
        This->PatchDir=NewFol;
        This->RefreshPatchList();
      }
      SetForegroundWindow(Win);
      EnableAllWindows(true,Win);
      SetFocus((HWND)lPar);
      SendMessage((HWND)lPar,BM_SETCHECK,0,0);
      break;
    }
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
  }
  return DefWindowProc(Win,Mess,wPar,lPar);
}

#endif//WIN32

#ifdef UNIX

//---------------------------------------------------------------------------
int TPatchesBox::WinProc(TPatchesBox *This,Window Win,XEvent *Ev)
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
int TPatchesBox::ListviewNotifyHandler(hxc_listview* LV,int Mess,INT_PTR I)
{
  TPatchesBox *This=(TPatchesBox*)(LV->owner);
  if (LV->sel>=0){
    if (NotSameStr_I(LV->sl[LV->sel].String,This->SelPatch)){
      This->SelPatch=LV->sl[LV->sel].String;
      This->ShowPatchFile();
    }
  }		
  return 0;
}
//---------------------------------------------------------------------------
int TPatchesBox::ButtonNotifyHandler(hxc_button* But,int Mess,int I[])
{
  TPatchesBox *This=(TPatchesBox*)(But->owner);
  if (Mess==BN_CLICKED){
  	if (But->id==100){
	    This->ApplyPatch();
	  }else if (But->id==200){
			fileselect.set_corner_icon(&Ico16,ICO16_FOLDER);
		  EasyStr Path=fileselect.choose(XD,This->PatchDir,"",T("Pick a Folder"),
		    FSM_CHOOSE_FOLDER | FSM_CONFIRMCREATE,folder_parse_routine,"");  	
		  if (Path.NotEmpty()){
        NO_SLASH(Path);
		  	This->PatchDir=Path;
		  	CreateDirectory(This->PatchDir,NULL);
		  	This->PatchDirText.set_text(This->PatchDir);
		  	
			  This->DescText.set_text("");This->DescText.draw();
			  This->ApplyWhenText.set_text("");This->ApplyWhenText.draw();
			  This->VersionText.set_text("");This->VersionText.draw();
			  This->AuthorText.set_text("");This->AuthorText.draw();
		  	This->RefreshPatchList();
		  }
	  }
  }
  return 0;
}

#endif
