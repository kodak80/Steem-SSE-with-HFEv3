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

DOMAIN: Various
FILE: mymisc.cpp
DESCRIPTION: Many miscellaneous functions from all areas of programming that
just refuse to be categorized.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#ifdef MINGW_BUILD
#include <conditions.h>
#endif
#include <mymisc.h>
#include <stdarg.h>
#include <options.h> // TODO? steem-specific (mymisc in is /include) but who cares?
#ifdef WIN32
#include <windowsx.h>
#endif


#pragma warning (disable : 4996) // deprecated

DWORD HexToVal(char *HexStr) {
  if(strlen(HexStr)>100) 
    return 0;
  char *numstring=new char[strlen(HexStr)+1];
  char *hex=numstring;
  strcpy(hex,HexStr);
  strupr(hex);
  // Get rid of 0x or $
  if(hex[0]=='$') 
    hex++;
  else if(hex[1]=='X') 
    hex+=2;
  // Search from end to get first allowable character
  char *l;
  for(l=hex+strlen(hex);l>=hex;l--)
  {
    if((*l>='0' && *l<='9')||(*l>='A' && *l<='F')) 
      break;
  }
  if(l<hex)
  {  // No allowable characters
    delete[] numstring;
    return 0;
  }
  unsigned long ret=0,val;
  char numdid=(char)MIN((int)(l-hex)+1,8);
  char *let;
  for(int n=0;n<numdid;n++)
  {
    let=l-n;
    if(*let>='0' && *let<='9')
      val=(*let)-'0';
    else if(*let>='A' && *let<='F')
    {
      val=(*let)-'A';
      val+=10;
    }
    else
      break;
    if(n>0) 
      val*=(unsigned long)pow((float)16,n);
    ret+=val;
  }
  delete[] numstring;
  return ret;
}


#ifdef SSE_X64
unsigned long long HexToVall(char *HexStr) {
  if(strlen(HexStr)>100) 
    return 0;
  char *numstring=new char[strlen(HexStr)+1];
  char *hex=numstring;
  strcpy(hex,HexStr);
  strupr(hex);
  // Get rid of 0x or $
  if(hex[0]=='$') 
    hex++;
  else if(hex[1]=='X') 
    hex+=2;
  // Search from end to get first allowable character
  char *l;
  for(l=hex+strlen(hex);l>=hex;l--)
  {
    if((*l>='0' && *l<='9')||(*l>='A' && *l<='F')) 
      break;
  }
  if(l<hex)
  {  // No allowable characters
    delete[] numstring;
    return 0;
  }
  unsigned long long ret=0,val;
  char numdid=(char)MIN((int)(l-hex)+1,8+8);
  char *let;
  for(int n=0;n<numdid;n++)
  {
    let=l-n;
    if(*let>='0' && *let<='9')
      val=(*let)-'0';
    else if(*let>='A' && *let<='F')
    {
      val=(*let)-'A';
      val+=10;
    }
    else
      break;
    if(n>0) 
      val*=(unsigned long long)pow((float)16,n);
    ret+=val;
  }
  delete[] numstring;
  return ret;
}
#endif


// 123456789 -> "123,456,789"

int InsertCommas(char *dest,LONGLONG bignumber) {
  char buf[64];
  sprintf(buf,"%lld",bignumber);
  int l=(int)strlen(buf);
  int commas=(l-1)/3;
  int l_dest=l+commas;
  int commactr=4; // first char copied is null
  do
  {
    dest[l_dest--]=buf[l--];
    if(!--commactr)
    {
      dest[l_dest--]=SSEConfig.separator; //  ',' or '.' or... according to locale
      commactr=3;
    }
  } while(l_dest>=0);
  return commas;
}


#ifdef DEADC0DE
int InsertCommas(char *dest,int bignumber) {
  char buf[64];
  sprintf(buf,"%d",bignumber);
  int l=(int)strlen(buf);
  int commas=(l-1)/3;
  int l_dest=l+commas;
  int commactr=4; // first char copied is null
  do
  {
    dest[l_dest--]=buf[l--];
    if(!--commactr)
    {
      dest[l_dest--]=',';
      commactr=3;
    }
  } while(l_dest>=0);
  return commas;
}


bool LoadBool(FILE *fp) {
  static char b;
  fread(&b,1,1,fp);
  return b!=0;
}


void SaveBool(bool b,FILE *fp) {
  static char buf;
  buf=b;
  fwrite(&buf,1,1,fp);
}


int LoadInt(FILE *fp) {
  static int i;
  fread(&i,sizeof(i),1,fp);
  return i;
}


void LoadChars(char *buf,FILE *fp) {
  static int p;
  p=0;
  do
  {
    fread(buf+p,1,1,fp);
  } while(buf[p++]);
}


void SaveChars(char *buf,FILE *fp) {
  static int p;
  static char c;
  p=0;
  do
  {
    c=buf[p++];
    fwrite(&c,1,1,fp);
  } while(c);
}
#endif


#if defined(SSE_BIGFILES)

INT64 GetFileLength(FILE *fp) {
  INT64 pos=FTELL(fp);
  FSEEK(fp,0,SEEK_END);
  INT64 len=FTELL(fp);
  FSEEK(fp,pos,SEEK_SET); // restore
  return len;
}

#else

LONG GetFileLength(FILE *fp) {
  LONG pos=ftell(fp);
  fseek(fp,0,SEEK_END);
  LONG len=ftell(fp);
  fseek(fp,pos,SEEK_SET); // restore
  return len;
}

#endif

#if defined(SSE_LONG_PATH)

INT64 GetFileLength(HANDLE fp) {
  LARGE_INTEGER FileSize;
  GetFileSizeEx(fp,&FileSize);
  return FileSize.QuadPart;
}


void PathPrePend(EasyStr path,bool bAddOrRemove) { // true=add, false=remove

  if(path==NULL || path.Length()<4)
    return;

  // we add only if necessary, we remove only if present, so we need to test if present
  bool bPrePendPresent=!strncmp(path.Text,LONG_PATH_PREPEND,4);
  
  if(bAddOrRemove && !bPrePendPresent)
  {
    //EasyStr sTmp=LONG_PATH_PREPEND;
    INT_PTR l=path.Length(); // only mess with path if it's long
    EasyStr sTmp=(l>MAX_PATH)?LONG_PATH_PREPEND:"";
    sTmp+=path;
    path=sTmp;
  }
  else if(!bAddOrRemove && bPrePendPresent)
  {
    EasyStr sTmp(path+4);
    path=sTmp;
  }
}


#endif

void RemoveFileNameFromPath(char *fil,bool rem) {
  if(fil[0]!='\0')
    *(GetFileNameFromPath(fil)-(rem?1:0))='\0';
}


bool has_extension_list(char *Filename,char *Ext,...) {
  char *FileExt=strrchr(GetFileNameFromPath(Filename),'.');
  if(FileExt==NULL) 
    return false;
  FileExt++;
  va_list vl;
  va_start(vl,Ext);
  char* arg=Ext;
  while(arg)
  {
    if(arg[0]=='.') 
      arg++;
    if(IsSameStr_I(FileExt,arg)) 
    {
      va_end(vl);    
      return true;
    }
    arg=va_arg(vl,char*);
  }
  va_end(vl);
  return false;
}


bool has_extension(char *Filename,char *Ext) {
  return has_extension_list(Filename,Ext,NULL);
}


// if we forget NULL as final argument, crash likely, to assert that
// we could put it in a try/catch pair, or give the comparing function
// as last argument but see, only one is used...

#ifdef DEADC0DE
bool MatchesAnyString(char *StrToCompare,char *Str,...) {
  va_list vl;
  va_start(vl,Str);
  char* arg=Str;
  while(arg)
  {
    if(IsSameStr(StrToCompare,arg)) 
    {
      va_end(vl);
      return true;
    }
    arg=va_arg(vl,char*);
  }
  va_end(vl);
  return false;
}
#endif


bool MatchesAnyString_I(char *StrToCompare,char *Str,...) {
  va_list vl;
  va_start(vl,Str);
  char* arg=Str;
  while(arg)
  {
    if(IsSameStr_I(StrToCompare,arg)) 
    {
      va_end(vl);
      return true;
    }
    arg=va_arg(vl,char*);
  }
  va_end(vl);
  return false;
}


#include <sys/stat.h>

EasyStr GetUniquePath(EasyStr path,EasyStr name) {
  NO_SLASH(path);
  EasyStr ext;
  char *p=strrchr(name,'.');
  if(p)
  {
    ext=p;
    *p='\0';
  }
  EasyStr ret=path+SLASH+name+ext;
  struct stat s;
  int i=2;
  while(stat(ret,&s)==0) 
    ret=path+SLASH+name+" ("+(i++)+")"+ext;
  return ret;
}


DWORD TMToDOSDateTime(struct tm *lpTime) {
  DWORD Ret=(lpTime->tm_sec/2)&0x1f;
  Ret|=(lpTime->tm_min&0x3f)<<5;
  Ret|=(lpTime->tm_hour&0x1f)<<11;
  Ret|=(lpTime->tm_mday&0x1f)<<16;
  Ret|=((lpTime->tm_mon+1)&0xf)<<21;
  Ret|=((lpTime->tm_year+1900-1980)&0x3f)<<25;
  return Ret;
}


int log_to_base_2(unsigned long x) {
  int n;
  for(n=0;x>>n;n++);
  return n-1;
}


#ifdef WIN32

char *GetFileNameFromPath(char *fil) {
  if(fil==NULL) //401
    return NULL;
  int Len=(int)strlen(fil);
  if(Len==0) 
    return fil;
  char *pos=fil+Len-1;
  for(;pos>=fil;pos--)
    if(*pos=='\\'||*pos=='/'||*pos==':') 
      break;
  return pos+1;
}


void RemoveProps(HWND Win,char *Prop1,...) {
  va_list vl;
  va_start(vl,Prop1);
  char* arg=Prop1;
  while(arg)
  {
    RemoveProp(Win,arg);
    arg=va_arg(vl,char*);
  }
  va_end(vl);
}


#ifdef DEADC0DE
void Border3D(HDC dc,int x,int y,int w,int h,
  DWORD col0,DWORD col1,DWORD col2,DWORD col3) {
  int x1=(x+w)-1,y1=(y+h)-1;
  HPEN o_pen=(HPEN)SelectObject(dc,CreatePen(PS_SOLID,1,col0));
  MoveToEx(dc,x+1,y1-1,0);
  LineTo(dc,x+1,y+1);
  LineTo(dc,x1-1,y+1);
  DeleteObject(SelectObject(dc,o_pen));
  o_pen=(HPEN)SelectObject(dc,CreatePen(PS_SOLID,1,col2));
  MoveToEx(dc,x+1,y1-1,0);
  LineTo(dc,x1-1,y1-1);
  LineTo(dc,x1-1,y);
  DeleteObject(SelectObject(dc,o_pen));
  o_pen=(HPEN)SelectObject(dc,CreatePen(PS_SOLID,1,col1));
  MoveToEx(dc,x,y1,0);
  LineTo(dc,x,y);
  LineTo(dc,x1,y);
  DeleteObject(SelectObject(dc,o_pen));
  o_pen=(HPEN)SelectObject(dc,CreatePen(PS_SOLID,1,col3));
  MoveToEx(dc,x,y1,0);
  LineTo(dc,x1,y1);
  LineTo(dc,x1,y-1);
  DeleteObject(SelectObject(dc,o_pen));
}


void Box3D(HDC dc,int x,int y,int w,int h,bool d) {
  RECT rc={x,y,x+w+1,y+h+1};
  DrawEdge(dc,&rc,d?EDGE_SUNKEN:EDGE_RAISED,BF_RECT);
}
#endif


void CentreTextOut(HDC dc,int x,int y,int w,int h,char *text,int len) {
  if(len==-1) 
    len=(int)strlen(text);
  SIZE sz;
  GetTextExtentPoint32(dc,text,len,&sz);
  TextOut(dc,x+(w/2)-(sz.cx/2),y+(h/2)-(sz.cy/2),text,len);
}


void CentreTextOut(HDC dc,RECT *lpRect,char *text) {
  CentreTextOut(dc,(int)lpRect->left,(int)lpRect->top,
    (int)(lpRect->right-lpRect->left),(int)(lpRect->bottom-lpRect->top),text,-1);
}


/* "The GetLongPathName API call is only available on Windows 98/ME and Windows 2000/XP.
 It is not available on Windows 95 & NT."*/
#ifndef SSE_WINDOWS_2000_MIN //v420
void GetLongPathName(char *src,char *dest,int maxlen) {
  if(src[0]==SLASHCHAR)
  {
    if(src[1]==SLASHCHAR)
    {
      // Network file, leave alone
      int i=0;
      while(i<maxlen)
      {
        dest[i]=src[i];
        if(src[i]=='\0')
          break;
        i++;
      }
      dest[i]='\0'; // make sure null-terminated
      return;
    }
  }
  bool TooLong=false,FileName=true;
  int Mov;
  char *longpath=new char[maxlen+1];
  longpath[0]='\0';
  char *shortpath=new char[strlen(src)+1];strcpy(shortpath,src);
  char *spp=shortpath+strlen(shortpath)-1;
  WIN32_FIND_DATA *wfd=new WIN32_FIND_DATA;
  for(;;)
  {
    do
    {
      spp--;
    } while(*spp!='\\' && *spp!='/' && spp>shortpath);
    if(spp<=shortpath) 
      break;
    HANDLE fh=FindFirstFile(shortpath,wfd);
    if(fh!=INVALID_HANDLE_VALUE)
      FindClose(fh);
    else
    {
      wfd->cFileName[0]=0;
      if(FileName)
        strcpy(wfd->cFileName,GetFileNameFromPath(shortpath));
    }
    if(wfd->cFileName[0])
    {
      Mov=(int)strlen(wfd->cFileName);
      if((int)strlen(longpath)+Mov+1>=maxlen)
      {
        TooLong=true;
        break;
      }
      memmove(longpath+Mov+1,longpath,strlen(longpath)+1);
      longpath[0]=*spp;
      memcpy(longpath+1,wfd->cFileName,Mov);
    }
    *spp='\0';
    FileName=false;
  }
  delete wfd;
  Mov=(int)strlen(shortpath);
  if((int)strlen(longpath)+Mov>=maxlen)
    TooLong=true;
  else
  {
    memmove(longpath+Mov,longpath,strlen(longpath)+1);
    memcpy(longpath,shortpath,Mov);
  }
  if(!TooLong)
    strcpy(dest,longpath);
  else if((int)strlen(src)<maxlen)
    strcpy(dest,src);
  else
    dest[0]='\0';
  delete[] shortpath;
  delete[] longpath;
}
#endif

void SetWindowAndChildrensFont(HWND Win,HFONT font) {
  if(Win==NULL) 
    return;
  SendMessage(Win,WM_SETFONT,(WPARAM)font,0);
  HWND Child=GetWindow(Win,GW_CHILD);
  while(Child)
  {
    SendMessage(Child,WM_SETFONT,(WPARAM)font,0);
    Child=GetWindow(Child,GW_HWNDNEXT);
  }
}


void RemoveAllMenuItems(HMENU menu) {
  int n=GetMenuItemCount(menu);
  for(int i=0;i<n;i++) 
    RemoveMenu(menu,0,MF_BYPOSITION);
}


void DeleteAllMenuItems(HMENU menu) {
  int n=GetMenuItemCount(menu);
  for(int i=0;i<n;i++) 
    DeleteMenu(menu,0,MF_BYPOSITION);
}

#if !defined(SSE_VID_2SCREENS)

void CentreWindow(HWND Win,bool Redraw) {
  RECT rc;GetWindowRect(Win,&rc);
  int W=rc.right-rc.left,H=rc.bottom-rc.top;
  MoveWindow(Win,(GetSystemMetrics(SM_CXSCREEN)-W)/2,
    (GetSystemMetrics(SM_CYSCREEN)-H)/2,W,H,Redraw);
}

#endif

#ifdef DEADC0DE
bool RegKeyExists(HKEY Key,char *Name) {
  HKEY K;
  if(RegOpenKey(Key,Name,&K)==ERROR_SUCCESS)
  {
    RegCloseKey(K);
    return 1;
  }
  return 0;
}


bool WindowIconsAre256() {
  HKEY Key;
  if(RegOpenKey(HKEY_CURRENT_USER,"Control Panel\\desktop\\WindowMetrics",&Key)
    ==ERROR_SUCCESS)
  {
    char BPP[200]={0,0,0,0,0,0,0};
    DWORD Size=200;
    if(RegQueryValueEx(Key,"Shell Icon BPP",NULL,NULL,(BYTE*)BPP,&Size)
      !=ERROR_SUCCESS) BPP[0]='4';
    RegCloseKey(Key);
    return atoi(BPP)>4;
  }
  return 0;
}
#endif


// only used by debug_plugin_load()
void DisplayLastError(char *TitleText) {
  HLOCAL lpMsgBuf;
  DWORD Err;
  char Title[50];
  Err=GetLastError();
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
    NULL,Err/*GetLastError()*/,MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
    (char*)&lpMsgBuf,0,NULL);
  if(TitleText==NULL)
  {
    strcpy(Title,"Error #");
#ifdef MINGW_BUILD
    _ultoa(Err,Title+7,10);
#else
    ultoa(Err,Title+7,10);
#endif
  }
  else
    strcpy(Title,TitleText);
  MessageBox(NULL,(char*)lpMsgBuf,Title,MB_OK|MB_ICONINFORMATION);
  LocalFree(lpMsgBuf);
}


HFONT MakeFont(char *Typeface,int Height,int Width,int Boldness,bool Italic,
               bool Underline,bool Strikeout) {
  return CreateFont(Height,Width,0,0,Boldness,Italic,Underline,Strikeout,
    ANSI_CHARSET, OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,PROOF_QUALITY,
    DEFAULT_PITCH|FF_DONTCARE,Typeface);
}


COLORREF GetMidColour(COLORREF RGB1,COLORREF RGB2) {
  int C1=GetRValue(RGB1),C2=GetRValue(RGB2);
  int R=(max(C1,C2)-min(C1,C2))/2+min(C1,C2);
  C1=GetGValue(RGB1),C2=GetGValue(RGB2);
  int G=(max(C1,C2)-min(C1,C2))/2+min(C1,C2);
  C1=GetBValue(RGB1),C2=GetBValue(RGB2);
  int B=(max(C1,C2)-min(C1,C2))/2+min(C1,C2);
  return RGB(R,G,B);
}


COLORREF DimColour(COLORREF Col,double DimAmount) {
  if(DimAmount<0) 
    return Col;
  if(DimAmount<1)
  {
    return RGB(double(GetRValue(Col))*DimAmount,
      double(GetGValue(Col))*DimAmount,
      double(GetBValue(Col))*DimAmount);
  }
  else
  {
    return RGB(max(GetRValue(Col)-(BYTE)DimAmount,0),
      MAX(GetGValue(Col)-(BYTE)DimAmount,0),
      MAX(GetBValue(Col)-(BYTE)DimAmount,0));
  }
}


#ifdef DEADC0DE
EasyStr GetCurrentDir() {
  EasyStr Path;
  Path.SetLength(MAX_PATH);
  GetCurrentDirectory(MAX_PATH,Path);
  return Path;
}
#endif


// only called once in WinMain()
EasyStr GetEXEDir() {
  EasyStr Path;
  Path.SetLength(SSE_MAX_PATH);
  GetModuleFileName(NULL,Path,SSE_MAX_PATH);
  RemoveFileNameFromPath(Path,REMOVE_SLASH);
  GetLongPathName(Path,Path,SSE_MAX_PATH);
  return Path;
}


EasyStr GetEXEFileName() {
  EasyStr Path;
  Path.SetLength(SSE_MAX_PATH);
  GetModuleFileName(NULL,Path,SSE_MAX_PATH);
  GetLongPathName(Path,Path,SSE_MAX_PATH);
  return Path;
}


bool GetWindowPositionData(HWND Win,TWinPositionData *wpd) {
  if(!IsWindow(Win))
    return true;
  RECT rc;
  SystemParametersInfo(SPI_GETWORKAREA,0,&rc,0);
  WINDOWPLACEMENT wp;
  wp.length=sizeof(WINDOWPLACEMENT);
  GetWindowPlacement(Win,&wp);
  wpd->Left=rc.left+wp.rcNormalPosition.left;
  wpd->Top=rc.top+wp.rcNormalPosition.top;
  wpd->Width=wp.rcNormalPosition.right-wp.rcNormalPosition.left;
  wpd->Height=wp.rcNormalPosition.bottom-wp.rcNormalPosition.top;
  LONG l=GetWindowLong(Win,GWL_STYLE);
  wpd->Maximized=(l & WS_MAXIMIZE)!=0;
  if(wp.showCmd==SW_SHOWMINIMIZED&&(wp.flags & WPF_RESTORETOMAXIMIZED))
    wpd->Maximized=true;
  wpd->Minimized=(l & WS_MINIMIZE)!=0;
  return false;
}


#ifdef DEADC0DE
EasyStr GetPPEasyStr(char *SectionName,char *KeyName,char *Default,
  char *FileName) {
  EasyStr Temp;
  Temp.SetLength(5000);
  GetPrivateProfileString(SectionName,KeyName,Default,Temp,5000,FileName);
  return Temp;
}
#endif


EasyStr FileSelect(HWND Owner,char *Title,char *DefaultDir,char *Types,
                   int InitType,int LoadFlag,EasyStr DefExt,char *DefFile) {
#if defined(SSE_LONG_PATH)
  EasyStr sText;
  sText.SetLength(SSE_MAX_PATH);
  char *fil=sText.Text;
#else
  char fil[MAX_PATH+1];
#endif
  if(DefFile[0])
    strcpy(fil,DefFile);
  else
    fil[0]='\0';
  OPENFILENAME ofn;
  ZeroMemory(&ofn,sizeof(OPENFILENAME));
  ofn.lStructSize=sizeof(OPENFILENAME);
  ofn.hwndOwner=Owner;
  ofn.hInstance=(HINSTANCE)GetModuleHandle(NULL);
  ofn.lpstrFilter=Types;
  ofn.lpstrCustomFilter=NULL;
  ofn.nMaxCustFilter=0;
  ofn.nFilterIndex=InitType;
  ofn.lpstrFile=fil;
  ofn.nMaxFile=SSE_MAX_PATH;
  ofn.lpstrFileTitle=NULL;
  ofn.nMaxFileTitle=0;
#if defined(SSE_LONG_PATH)
  EasyStr sDefaultDir=DefaultDir;
  PathPrePend(sDefaultDir,false); // remove long path prepend if present
  ofn.lpstrInitialDir=sDefaultDir.Text;
#else
  ofn.lpstrInitialDir=DefaultDir;
#endif
  ofn.lpstrTitle=Title;
  ofn.Flags=OFN_HIDEREADONLY|OFN_NOCHANGEDIR;
  if(LoadFlag==1)
    ofn.Flags|=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
  else if(LoadFlag==0)
    ofn.Flags|=OFN_OVERWRITEPROMPT;
  ofn.lpstrDefExt=DefExt.IsEmpty() ? NULL : DefExt.Text;
  ofn.lpfnHook=NULL;
  ofn.lpTemplateName=NULL;
  if((LoadFlag ? GetOpenFileName(&ofn) : GetSaveFileName(&ofn))==0) 
    fil[0]='\0';
#if defined(SSE_LONG_PATH)
  //EasyStr a=LONG_PATH_PREPEND;
  INT_PTR l=strlen(fil); // only mess with path if it's long
  EasyStr a=(fil[0] && l>MAX_PATH)?LONG_PATH_PREPEND:"";
  a+=fil;
#else
  EasyStr a=fil;
#endif
  return a;
}


EasyStr GetLinkDest(EasyStr LinkFileName,WIN32_FIND_DATA *wfd,HWND UIParent,
                    IShellLink *Link,IPersistFile* File) {
  HRESULT hres;
  bool ReleaseLink=false,ReleaseFile=true;
  EasyStr Path;
  // Get a pointer to the IShellLink interface.
  if(Link==NULL)
  {
    hres=CoCreateInstance(CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,IID_IShellLink,(void**)&Link);
    if(!SUCCEEDED(hres))
      Link=NULL;
    else
      ReleaseLink=true;
  }
  if(Link)
  {
    if(File==NULL)
    {
      // Query IShellLink for the IPersistFile interface for saving the shortcut in persistent storage.
      hres=Link->QueryInterface(IID_IPersistFile,(void**)&File);
      if(!SUCCEEDED(hres))
        File=NULL;
    }
    else
      ReleaseFile=false;
    if(File)
    {
      // Ensure that the path is Unicode.
#if defined(SSE_LONG_PATH)
      wchar_t *WideName=(wchar_t*)calloc(SSE_MAX_PATH,sizeof(wchar_t)); // no EasyStr here
#else
      wchar_t WideName[MAX_PATH+1];
#endif
      MultiByteToWideChar(CP_ACP,0,LinkFileName,-1,WideName,SSE_MAX_PATH);
      // Load the shortcut.
      hres=File->Load(WideName,STGM_READ);
#if defined(SSE_LONG_PATH)
      free(WideName);
#endif
      if(SUCCEEDED(hres))
      {
        // Resolve the link.
        if(UIParent!=NULL) 
          hres=Link->Resolve(UIParent,SLR_ANY_MATCH|SLR_UPDATE);
        if(SUCCEEDED(hres))
        {
          // Get the path to the link target.
          Path.SetLength(SSE_MAX_PATH);
          ZeroMemory(wfd,sizeof(WIN32_FIND_DATA));
          hres=Link->GetPath(Path.Text,SSE_MAX_PATH,wfd,(DWORD)0);
          if(!SUCCEEDED(hres))
            Path="";
        }
      }
      if(ReleaseFile) 
        File->Release();
    }
    if(ReleaseLink) 
      Link->Release();
  }
  return Path;
}


HRESULT CreateLink(char *LinkFileName,char *TargetFileName,char *Description,
                   IShellLink *Link,IPersistFile* File,char *IconPath,
                   int IconIdx,bool NoOverwrite) {
  HRESULT hres=0;
  bool ReleaseLink=true,ReleaseFile=true;
  // Get a pointer to the IShellLink interface.
  if(Link==NULL)
  {
    hres=CoCreateInstance(CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,IID_IShellLink,(void**)&Link);
    if(!SUCCEEDED(hres))
      Link=NULL;
  }
  else
    ReleaseLink=false;
  if(Link)
  {
    if(File==NULL)
    {
      // Query IShellLink for the IPersistFile interface for saving the
      // shortcut in persistent storage.
      hres=Link->QueryInterface(IID_IPersistFile,(void**)&File);
      if(!SUCCEEDED(hres))
        File=NULL;
    }
    else
      ReleaseFile=false;
    if(File)
    {
#if defined(SSE_LONG_PATH)
      wchar_t *WideName=(wchar_t*)calloc(SSE_MAX_PATH,sizeof(wchar_t));
#else
      wchar_t WideName[MAX_PATH+1];
#endif
      MultiByteToWideChar(CP_ACP,0,LinkFileName,-1,WideName,SSE_MAX_PATH);
      hres=0;
      if(NoOverwrite)
      {
        if(access(LinkFileName,0)==0)
        {
          hres=File->Load(WideName,STGM_READ);
          if(SUCCEEDED(hres))
          {
            WORD HotKey=0;
            char OldDesc[500],Args[500];
#if defined(SSE_LONG_PATH)
            EasyStr sText1;
            sText1.SetLength(SSE_MAX_PATH);
            char* OldIconPath=sText1.Text;
            EasyStr sText2;
            sText2.SetLength(SSE_MAX_PATH);
            char* WorkDir=sText2.Text;
#else
            char OldIconPath[MAX_PATH],WorkDir[MAX_PATH];
#endif
            int OldIconIdx,ShowCmd;
            Link->GetHotkey(&HotKey);
            Link->GetArguments(Args,500);
            Link->GetDescription(OldDesc,500);
            Link->GetIconLocation(OldIconPath,SSE_MAX_PATH,&OldIconIdx);
            Link->GetShowCmd(&ShowCmd);
            Link->GetWorkingDirectory(WorkDir,SSE_MAX_PATH);
            Link->SetPath(TargetFileName);
            if(Description)
              Link->SetDescription(Description);
            else
              Link->SetDescription(OldDesc);
            if(IconPath)
              Link->SetIconLocation(IconPath,IconIdx);
            else
              Link->SetIconLocation(OldIconPath,OldIconIdx);
            Link->SetHotkey(HotKey);
            Link->SetArguments(Args);
            Link->SetShowCmd(ShowCmd);
            Link->SetWorkingDirectory(WorkDir);
            hres=S_FALSE;
          }
        }
      }
      if(hres==0)
      {
        Link->SetPath(TargetFileName);
        if(Description) 
          Link->SetDescription(Description);
        if(IconPath) 
          Link->SetIconLocation(IconPath,IconIdx);
      }
#if defined(SSE_LONG_PATH)
/*  IPersistFile can't save to a long path, so we save the link as a tmp file
*   and copy it to the (possibly) long path*/
      EasyStr sTmp,swTmp;
      sTmp.SetLength(MAX_PATH);
      swTmp.SetLength(MAX_PATH*2); // should calloc
      GetTempFileName(TempPath,"TMP",0,sTmp);
      MultiByteToWideChar(CP_ACP,0,sTmp.Text,-1,(wchar_t*)swTmp.Text,SSE_MAX_PATH);
      File->Save((wchar_t*)swTmp.Text,true);
      CopyFileW((wchar_t*)swTmp.Text,WideName,TRUE);
      DeleteFileW((wchar_t*)swTmp.Text);
      free(WideName);
#else
      File->Save(WideName,true);
#endif
      if(ReleaseFile) 
        File->Release();
    }
    if(ReleaseLink) 
      Link->Release();
  }
  return hres;
}


#ifdef DEADC0DE
void DeleteDirAndContents(char *Dir) {
  static WIN32_FIND_DATA wfd;
  bool PutSlashBack=false;
  char NewDir[MAX_PATH+1];
  if(Dir[strlen(Dir)-1]==SLASHCHAR)
  {
    Dir[strlen(Dir)-1]=0;
    PutSlashBack=true;
  }
  if(GetFileAttributes(Dir)==0xffffffff)
  {
    if(PutSlashBack) Dir[strlen(Dir)]=SLASHCHAR;
    return;
  }
  strcpy(NewDir,Dir);
  strcat(NewDir,"\\*.*");
  HANDLE FHan=FindFirstFile(NewDir,&wfd);
  if(FHan!=INVALID_HANDLE_VALUE)
  {
    do
    {
      if(strcmp(wfd.cFileName,".")&&strcmp(wfd.cFileName,".."))
      {
        strcpy(NewDir,Dir);
        strcat(NewDir,SLASH);
        strcat(NewDir,wfd.cFileName);
        if(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
          DeleteDirAndContents(NewDir);
        else
          DeleteFile(NewDir);
      }
    } while(FindNextFile(FHan,&wfd));
    FindClose(FHan);
  }
  RemoveDirectory(Dir);
  if(PutSlashBack) 
    Dir[strlen(Dir)]=SLASHCHAR;
}
#endif


void CentreLVItem(HWND LV,int iItem,LRESULT State) {
  RECT rc,Client;
  LV_ITEM lvi;
  if(State==-1)
  {
    lvi.iItem=iItem;
    lvi.iSubItem=0;
    lvi.stateMask=LVIS_SELECTED|LVIS_FOCUSED;
    State=SendMessage(LV,LVM_GETITEMSTATE,iItem,(LPARAM)&lvi);
  }
  lvi.iItem=iItem;
  lvi.iSubItem=0;
  lvi.stateMask=LVIS_FOCUSED|LVIS_SELECTED;
  lvi.state=0;
  SendMessage(LV,LVM_SETITEMSTATE,iItem,(LPARAM)&lvi);
  ListView_GetItemRect(LV,iItem,&rc,LVIR_BOUNDS);
  rc.bottom=rc.bottom-rc.top-2;
  GetClientRect(LV,&Client);
  Client.right/=2;
  if(rc.bottom<Client.bottom)
  {
    rc.bottom/=2;
    Client.bottom/=2;
  }
  SendMessage(LV,LVM_SETITEMPOSITION,iItem,(LONG)(WORD)(Client.right-16) 
    | ((DWORD)((WORD)(Client.bottom-rc.bottom))<<16));
  lvi.iItem=iItem;
  lvi.iSubItem=0;
  lvi.stateMask=LVIS_FOCUSED|LVIS_SELECTED;
  lvi.state=(UINT)State;
  SendMessage(LV,LVM_SETITEMSTATE,iItem,(LPARAM)&lvi);
}


void GetTabControlPageSize(HWND Tabs,RECT *rc) {
  POINT pt={0,0};
  GetWindowRect(Tabs,rc);
  ClientToScreen(GetParent(Tabs),&pt);
  OffsetRect(rc,-pt.x,-pt.y);
  SendMessage(Tabs,TCM_ADJUSTRECT,0,(LPARAM)rc);
}


// Allow for &'s?
TWidthHeight GetTextSize(HFONT Font,char *Text) {
  TWidthHeight wh;
  HDC TempDC=CreateCompatibleDC(NULL);
  HFONT OldFont=(HFONT)SelectObject(TempDC,Font);
  GetTextExtentPoint32(TempDC,Text,(int)strlen(Text),(SIZE*)&wh);
  SelectObject(TempDC,OldFont);
  DeleteDC(TempDC);
  wh.Width++; // Allow for disabled text
  return wh;
}


TWidthHeight GetCheckBoxSize(HFONT Font,char *Text) {
  TWidthHeight wh;
  BITMAP bm;
  HBITMAP BoxBmp=LoadBitmap(NULL,MAKEINTRESOURCE(32759) /*OBM_CHECKBOXES*/);
  GetObject(BoxBmp,sizeof(BITMAP),&bm);
  DeleteObject(BoxBmp);
  wh.Width=bm.bmWidth/4;
  wh.Height=bm.bmHeight/3;
  if(Text)
  {
    TWidthHeight TextWH=GetTextSize(Font,Text);
    wh.Width+=5+1+TextWH.Width+1;
    if(TextWH.Height>wh.Height) 
      wh.Height=TextWH.Height;
  }
  return wh;
}


#ifdef DEADC0DE

typedef bool (WINAPI *LPTOOLHELPMODULEWALK)(HANDLE,LPMODULEENTRY32);
typedef HANDLE(WINAPI *LPTOOLHELPCREATESNAPSHOT)(DWORD,DWORD);
void GetWindowExePaths(HWND Win,char *Buf,int BufLen) {
  DWORD ProcID;
  HANDLE Snap;
  MODULEENTRY32 me;
  LPTOOLHELPCREATESNAPSHOT pCreateToolhelp32Snapshot;
  LPTOOLHELPMODULEWALK pModule32First;
  LPTOOLHELPMODULEWALK pModule32Next;
  HINSTANCE hKernel;
  GetWindowThreadProcessId(Win,&ProcID);
  hKernel=GetModuleHandle("KERNEL32.DLL");
  if(hKernel)
  {
    pCreateToolhelp32Snapshot=(LPTOOLHELPCREATESNAPSHOT)GetProcAddress(hKernel,"CreateToolhelp32Snapshot");
    pModule32First=(LPTOOLHELPMODULEWALK)GetProcAddress(hKernel,"Module32First");
    pModule32Next=(LPTOOLHELPMODULEWALK)GetProcAddress(hKernel,"Module32Next");
    if(pModule32First==NULL||pModule32Next==NULL||pCreateToolhelp32Snapshot==NULL) return;
  }
  else
  {
    return;
  }
  Snap=pCreateToolhelp32Snapshot(TH32CS_SNAPMODULE,ProcID);
  Buf[0]=0;
  char *pBuf=Buf,*pBufEnd=Buf+BufLen;
  me.dwSize=sizeof(MODULEENTRY32);
  if(pModule32First(Snap,&me))
  {
    do
    {
      if(strcmpi(me.szExePath+(strlen(me.szExePath)-3),"EXE")==0)
      {
        if(pBuf+strlen(me.szExePath)+1>=pBufEnd) break;
        strcpy(pBuf,me.szExePath);
        pBuf+=strlen(me.szExePath)+1;
        pBuf[0]=0;
      }
      me.dwSize=sizeof(MODULEENTRY32);
    } while(pModule32Next(Snap,&me));
  }
  CloseHandle(Snap);
}

#endif


char *RemoveIllegalFromPath(char *Path,bool DriveIncluded,bool RemoveWild,
                            char ReplaceChar,bool STPath) {
  char *Name,*PathStart=Path,*PathEnd=Path+strlen(Path)-1,*FilNam;
  bool GotSlash;
  if(DriveIncluded) 
    PathStart+=3;
  Name=PathEnd;
  do
  {
    GotSlash=false;
    while(Name>=PathStart)
    {
      if(*Name=='\\'||(*Name=='/' && !STPath))
      {
        GotSlash=true;
        break;
      }
      Name--;
    }
    FilNam=(LPSTR)(GotSlash ? (Name+1) : PathStart);
    while(FilNam<=PathEnd)
    {
      char c=*FilNam;
      if(c=='\\'||(c=='/' && !STPath))
        break;
      else
      {
        switch(c) {
        case ':':case '/':
        case '"':case '<':
        case '>':case '|':
          *FilNam=ReplaceChar;
          break;
        case '*':case '?':
          if(RemoveWild)
            *FilNam=ReplaceChar;
          break;
        }
      }
      FilNam++;
    }
    Name--;
  } while(Name>PathStart);
  return Path;
}


// This takes just a file name, not the folder it is in/drive it is on!
char *RemoveIllegalFromName(char *Name,bool RemoveWild,char ReplaceChar) {
  int Len=(int)strlen(Name);
  for(int i=0;i<Len;i++)
  {
    switch(Name[i]) {
    case ':':case '/':case '"':case '<':case '>':case '|':case '\\':
      Name[i]=ReplaceChar;
      break;
    case '*':case '?':
      if(RemoveWild)
        Name[i]=ReplaceChar;
      break;
    }
  }
  return Name;
}


LPARAM lParamPointsToParent(HWND Win,LPARAM lPar) {
  POINT WinPT={0,0},ParentPT={0,0};
  ClientToScreen(Win,&WinPT);
  ClientToScreen(GetParent(Win),&ParentPT);
  return (LPARAM)((GET_X_LPARAM(lPar)+(WinPT.x-ParentPT.x))
                |((GET_Y_LPARAM(lPar)+(WinPT.y-ParentPT.y))<<16));
}


LRESULT CBAddString(HWND Combo,char *String) {
  return SendMessage(Combo,CB_ADDSTRING,0,LPARAM(String));
}


#ifdef DEADC0DE
LRESULT CBAddString(HWND Combo,wchar_t *String,LONG_PTR Data) {
  LRESULT Idx=SendMessageW(Combo,CB_ADDSTRING,0,LPARAM(String));
  if(Idx>=0) 
    SendMessage(Combo,CB_SETITEMDATA,Idx,Data);
  return Idx;
}
#endif


LRESULT CBAddString(HWND Combo,char *String,LONG_PTR Data) {
  LRESULT Idx=SendMessage(Combo,CB_ADDSTRING,0,LPARAM(String));
  if(Idx>=0) 
    SendMessage(Combo,CB_SETITEMDATA,Idx,Data);
  return Idx;
}


LRESULT CBFindItemWithData(HWND Combo,LONG_PTR Data) {
  LRESULT n,c=SendMessage(Combo,CB_GETCOUNT,0,0);
  for(n=0;n<c;n++)
    if(SendMessage(Combo,CB_GETITEMDATA,n,0)==Data) 
      break;
  if(n>=c) 
    return CB_ERR;
  return n;
}


LRESULT CBSelectItemWithData(HWND Combo,LONG_PTR Data) {
  LRESULT Idx=CBFindItemWithData(Combo,Data);
  if(Idx>=0) 
    SendMessage(Combo,CB_SETCURSEL,Idx,0);
  return Idx;
}


LRESULT CBGetSelectedItemData(HWND Combo) {
  LRESULT SelIdx=SendMessage(Combo,CB_GETCURSEL,0,0);
  if(SelIdx>-1) 
    return SendMessage(Combo,CB_GETITEMDATA,SelIdx,0);
  return 0;
}


#ifdef DEADC0DE
void MoveWindowClient(HWND Win,int x,int y,int w,int h) {
  RECT rcWin;
  GetWindowRect(Win,&rcWin);
  RECT rcClient;
  GetClientRect(Win,&rcClient);
  POINT ptTL={0,0},ptBR={rcClient.right,rcClient.bottom};
  ClientToScreen(Win,&ptTL);
  ClientToScreen(Win,&ptBR);
  MoveWindow(Win,x-(ptTL.x-rcWin.left),y-(ptTL.x-rcWin.left),
    w+(rcWin.right-ptBR.x),h+(rcWin.bottom-ptBR.y),true);
}
#endif


void GetWindowRectRelativeToParent(HWND Win,RECT *pRc) {
  GetWindowRect(Win,pRc);
  RECT rcPar;
  GetWindowRect(GetParent(Win),&rcPar);
  pRc->left-=rcPar.left;
  pRc->right-=rcPar.left;
  pRc->top-=rcPar.top;
  pRc->bottom-=rcPar.top;
}


#ifdef DEADC0DE
void ToolsDeleteWithIDs(HWND ToolTip,HWND Parent,DWORD FirstID,...) {
  va_list vl;
  va_start(vl,FirstID);
  DWORD arg=FirstID;
  TOOLINFO ti;
  ti.cbSize=sizeof(TOOLINFO);
  ti.uFlags=TTF_IDISHWND;
  ti.hwnd=Parent;
  while(arg)
  {
    ti.uId=(UINT_PTR)GetDlgItem(Parent,arg);
    if(ti.uId) 
      SendMessage(ToolTip,TTM_DELTOOL,0,(LPARAM)&ti);
    arg=va_arg(vl,DWORD);
  }
  va_end(vl);
}
#endif


void ToolsDeleteAllChildren(HWND hToolTip,HWND hParent) {
  LRESULT c=SendMessage(hToolTip,TTM_GETTOOLCOUNT,0,0);
  TOOLINFO *pDelTI=new TOOLINFO[c];
  int ndel=0;
  // Get tools to be deleted (don't delete immediately as that would change index)
  for(LRESULT i=0;i<c;i++)
  {
    TOOLINFO ti;
    ti.cbSize=sizeof(TOOLINFO);
    ti.lpszText=NULL;
    SendMessage(hToolTip,TTM_ENUMTOOLS,i,(LPARAM)&ti);
    if(ti.hwnd==hParent) 
      pDelTI[ndel++]=ti;
  }
  // Delete tools
  for(LRESULT i=0;i<ndel;i++)
    SendMessage(hToolTip,TTM_DELTOOL,0,LPARAM(pDelTI+i));
  delete[] pDelTI;
}


void ToolAddWindow(HWND hToolTip,HWND Handle,char *Text) {
  TOOLINFO ti;
  ti.cbSize=sizeof(TOOLINFO);
  ti.uFlags=TTF_IDISHWND|TTF_SUBCLASS;
  ti.hwnd=GetParent(Handle);
  ti.uId=(UINT_PTR)Handle;
  ti.lpszText=Text;
  SendMessage(hToolTip,TTM_ADDTOOL,0,(LPARAM)&ti);
}


HTREEITEM TreeSelectItemWithData(HWND Tree,long n,HTREEITEM Item) {
  TV_ITEM tvi;
  if(Item==TVI_ROOT) 
    Item=(HTREEITEM)SendMessage(Tree,TVM_GETNEXTITEM,TVGN_CHILD,(LPARAM)Item);
  tvi.mask=TVIF_PARAM;
  while(Item)
  {
    tvi.hItem=Item;
    SendMessage(Tree,TVM_GETITEM,0,LPARAM(&tvi));
    if(tvi.lParam==n)
    {
      SendMessage(Tree,TVM_SELECTITEM,TVGN_CARET,LPARAM(Item));
      SendMessage(Tree,TVM_ENSUREVISIBLE,0,LPARAM(Item));
      return Item;
    }
    HTREEITEM ChildChosen=TreeSelectItemWithData(Tree,n,
      (HTREEITEM)SendMessage(Tree,TVM_GETNEXTITEM,TVGN_CHILD,(LPARAM)Item));
    if(ChildChosen) 
      return ChildChosen;
    Item=(HTREEITEM)SendMessage(Tree,TVM_GETNEXTITEM,TVGN_NEXT,(LPARAM)Item);
  }
  return NULL;
}


int TreeGetMaxItemWidth(HWND Tree,HTREEITEM Item,int Level) {
  int MaxWidth=0;
  RECT rc;
  HTREEITEM ChildItem;
  if(Item==TVI_ROOT) 
    Item=(HTREEITEM)SendMessage(Tree,TVM_GETNEXTITEM,TVGN_CHILD,(LPARAM)Item);
  while(Item)
  {
    if(TreeView_GetItemRect(Tree,Item,&rc,TRUE))
    {
      int Width=rc.right;
      MaxWidth=MAX(Width,MaxWidth);
    }
    ChildItem=(HTREEITEM)SendMessage(Tree,TVM_GETNEXTITEM,TVGN_CHILD,(LPARAM)Item);
    if(ChildItem) 
      MaxWidth=MAX(TreeGetMaxItemWidth(Tree,ChildItem,Level+1),MaxWidth);
    Item=(HTREEITEM)SendMessage(Tree,TVM_GETNEXTITEM,TVGN_NEXT,(LPARAM)Item);
  }
  return MaxWidth;
}


void ImageList_AddPaddedIcons(HIMAGELIST il,int Align,HICON Icon1,...) {
  va_list vl;
  va_start(vl,Icon1);
  HICON arg=Icon1;
  int il_w,il_h;
  ICONINFO o_ii,n_ii;
  BITMAP bi;
  ImageList_GetIconSize(il,&il_w,&il_h);
  RECT rc={0,0,il_w,il_h};
  HDC ScrDC=GetDC(NULL);
  HDC FromDC=CreateCompatibleDC(ScrDC);
  HDC ToDC=CreateCompatibleDC(ScrDC);
  n_ii.hbmColor=CreateCompatibleBitmap(ScrDC,il_w,il_h);
  n_ii.hbmMask=CreateBitmap(il_w,il_h,1,1,NULL);
  ReleaseDC(NULL,ScrDC);
  SetBkMode(ToDC,OPAQUE);SetROP2(ToDC,R2_COPYPEN);
  SetBkMode(FromDC,OPAQUE);SetROP2(FromDC,R2_COPYPEN);
  
  while(arg)
  {
    GetIconInfo(arg,&o_ii);
    n_ii.fIcon=o_ii.fIcon;
    n_ii.xHotspot=o_ii.xHotspot;
    n_ii.yHotspot=o_ii.yHotspot;
    GetObject(o_ii.hbmColor,sizeof(BITMAP),&bi);
    int xo=(il_w-bi.bmWidth)/2,yo=(il_h-bi.bmHeight)/2;
    if((Align & b0011)==b0001) 
      xo=0;
    if((Align & b0011)==b0010) 
      xo=(il_w-bi.bmWidth);
    if((Align & b1100)==b0100) 
      yo=0;
    if((Align & b1100)==b1000) 
      yo=(il_h-bi.bmHeight);
    HBITMAP FromDefBmp=(HBITMAP)SelectObject(FromDC,o_ii.hbmMask);
    HBITMAP ToDefBmp=(HBITMAP)SelectObject(ToDC,n_ii.hbmMask);
    FillRect(ToDC,&rc,(HBRUSH)GetStockObject(WHITE_BRUSH));
    BitBlt(ToDC,xo,yo,bi.bmWidth,bi.bmHeight,FromDC,0,0,SRCCOPY);
    SelectObject(FromDC,o_ii.hbmColor);
    SelectObject(ToDC,n_ii.hbmColor);
    FillRect(ToDC,&rc,(HBRUSH)GetStockObject(BLACK_BRUSH));
    BitBlt(ToDC,xo,yo,bi.bmWidth,bi.bmHeight,FromDC,0,0,SRCCOPY);
    SelectObject(FromDC,FromDefBmp);
    SelectObject(ToDC,ToDefBmp);
    DeleteObject(o_ii.hbmMask);
    DeleteObject(o_ii.hbmColor);
    HICON i=CreateIconIndirect(&n_ii);
    ImageList_AddIcon(il,i);
    DestroyIcon(i);
    arg=va_arg(vl,HICON);
  }
  va_end(vl);
  DeleteDC(FromDC);
  DeleteDC(ToDC);
  DeleteObject(n_ii.hbmColor);
  DeleteObject(n_ii.hbmMask);
}


void ImageList_AddPaddedIcons(HIMAGELIST il,int Align,int nIco,...) {
  va_list vl;
  va_start(vl,nIco);
  int arg=nIco;
  while(arg)
  {
    ImageList_AddPaddedIcons(il,Align,(HICON)LoadImage(GetModuleHandle(NULL),
      MAKEINTRESOURCE(arg),IMAGE_ICON,0,0,0), NULL);
    arg=va_arg(vl,int);
  }
  va_end(vl);
}


void ImageList_AddPaddedIcons(HIMAGELIST il,int Align,char *sIco,...) {
  va_list vl;
  va_start(vl,sIco);
  char* arg=sIco;
  while(arg)
  {
    ImageList_AddPaddedIcons(il,Align,(HICON)LoadImage(GetModuleHandle(NULL),
      arg,IMAGE_ICON,0,0,0),NULL);
    arg=va_arg(vl,char*);
  }
  va_end(vl);
}


#ifdef DEADC0DE
int LVGetSelItem(HWND LV) {
  LRESULT c=SendMessage(LV,LVM_GETITEMCOUNT,0,0);
  for(int i=0;i<c;i++)
    if(SendMessage(LV,LVM_GETITEMSTATE,i,LVIS_SELECTED)) 
      return i;
  return -1;
}


EasyStr LVGetItemText(HWND LV,int i) {
  EasyStr Ret;
  Ret.SetLength(5000);
  LV_ITEM lvi;
  lvi.mask=LVIF_TEXT;
  lvi.iItem=i;
  lvi.iSubItem=0;
  lvi.pszText=Ret.Text;
  lvi.cchTextMax=5000;
  SendMessage(LV,LVM_GETITEM,0,LPARAM(&lvi));
  return Ret;
}
#endif


EasyStr GetWindowTextStr(HWND Win) {
  EasyStr Text;
  int Len=GetWindowTextLength(Win)+1;
  Text.SetLength(Len);
  GetWindowText(Win,Text,Len);
  return Text;
}


#ifdef DEADC0DE
EasyStr LoadWholeFileIntoStr(char *File) {
  EasyStr Ret;
  FILE *f=fopen(File,"rb");
  if(f==NULL) 
    return "";
  int len=GetFileLength(f);
  Ret.SetLength(len);
  fread(Ret.Text,1,len,f);
  fclose(f);
  return Ret;
}


bool SaveStrAsFile(EasyStr &s,char *File) {
  FILE *f=fopen(File,"wb");
  if(f==NULL) 
    return 0;
  fwrite(s.Text,1,s.Length(),f);
  fclose(f);
  return true;
}
#endif


#if !defined(VC_BUILD) && !defined(MINGW_BUILD)
void _RTLENTRY __int__(int);
#endif

#if !defined(VC_BUILD)
extern bool no_ints;
#endif

TModifierState GetLRModifierStates() {
  TModifierState mss;
#if !defined(VC_BUILD)
  OSVERSIONINFO osvi;
  osvi.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);
  GetVersionEx(&osvi);
  if(osvi.dwPlatformId!=VER_PLATFORM_WIN32_NT)
  {
    BYTE ShiftFlags,CtrlAltFlags=0;
    ShiftFlags=BYTE(int(GetKeyState(VK_LSHIFT)<0?2:0)|int(GetKeyState(VK_RSHIFT)<0?1:0));
#if !defined(MINGW_BUILD)
  /*
      _AH=0x12; __int__(0x16); // Extended Get Keyboard Status  (AT+)
      ShiftFlags=_AL;CtrlAltFlags=_AH;
  */
    if(no_ints==0)
    { // SS removed _
      _AH=0x2; __int__(0x16);
      ShiftFlags=_AL;
    }
#elif defined(_MINGW_INTS)
    if(no_ints==0) ShiftFlags=(BYTE)int_16_2(); // SS removed _
#endif
    if(GetKeyState(VK_CONTROL)<0) CtrlAltFlags|=(1<<0)|(1<<2);
    if(GetKeyState(VK_MENU)<0) CtrlAltFlags|=(1<<1)|(1<<3);
    mss.LShift=bool(ShiftFlags & (1<<1));
    mss.RShift=bool(ShiftFlags & (1<<0));
    mss.LCtrl=bool(CtrlAltFlags & (1<<0));
    mss.RCtrl=bool(CtrlAltFlags & (1<<2));
    mss.LAlt=bool(CtrlAltFlags & (1<<1));
    mss.RAlt=bool(CtrlAltFlags & (1<<3));
    return mss;
  }
#endif
  mss.LShift=(GetKeyState(VK_LSHIFT)<0);
  mss.RShift=(GetKeyState(VK_RSHIFT)<0);
  mss.LCtrl=(GetKeyState(VK_LCONTROL)<0);
  mss.RCtrl=(GetKeyState(VK_RCONTROL)<0);
  mss.LAlt=(GetKeyState(VK_LMENU)<0);
  mss.RAlt=(GetKeyState(VK_RMENU)<0);
  return mss;
}


#ifdef DEADC0DE
void DrawLine(HDC dc,int x1,int y1,int x2,int y2) {
  MoveToEx(dc,x1,y1,0);
  LineTo(dc,x2,y2);
}
#endif


EasyStr ShortenPath(EasyStr Path,HFONT Font,int MaxWidth) {
  HDC ScrDC=GetDC(NULL);
  HDC DC=CreateCompatibleDC(ScrDC);
  HBITMAP Bmp=CreateCompatibleBitmap(ScrDC,MaxWidth,30);
  ReleaseDC(NULL,ScrDC);
  SelectObject(DC,Bmp);
  SelectObject(DC,Font);
  RECT rc={0,0,MaxWidth,30};
  DrawText(DC,Path,-1,&rc,DT_PATH_ELLIPSIS|DT_LEFT|DT_MODIFYSTRING|DT_NOPREFIX|DT_SINGLELINE);
  DeleteDC(DC);
  DeleteObject(Bmp);
  return Path;
}


HDC CreateScreenCompatibleDC() {
  HDC ScrDC=GetDC(NULL);
  HDC NewDC=CreateCompatibleDC(ScrDC);
  ReleaseDC(NULL,ScrDC);
  return NewDC;
}


#ifdef DEADC0DE
HBITMAP CreateScreenCompatibleBitmap(int w,int h) {
  HDC ScrDC=GetDC(NULL);
  HBITMAP NewBMP=CreateCompatibleBitmap(ScrDC,w,h);
  ReleaseDC(NULL,ScrDC);
  return NewBMP;
}


void FillRectWithColour(HDC dc,RECT *lpRect,COLORREF colour) {
  HBRUSH br=CreateSolidBrush(colour);
  FillRect(dc,lpRect,br);
  DeleteObject(br);
}
#endif


BOOL SetClipboardText(LPCTSTR pszText) {// from the 'net
/*  Used for -copy 68000 code from browser (Debugger)
             -copy disk name (excluding extension) from Disk manager = more or less useful feature...
*/
  BOOL ok=FALSE;
  if(OpenClipboard(NULL))
  {
    // the text should be placed in "global" memory
    HGLOBAL hMem=GlobalAlloc(GMEM_SHARE|GMEM_MOVEABLE,(lstrlen(pszText)+1)*sizeof(pszText[0]));
    if(hMem)
    {
      EmptyClipboard(); // SS added this
      LPTSTR ptxt=(LPTSTR)GlobalLock(hMem);
      if(ptxt)
        lstrcpy(ptxt,pszText);
      GlobalUnlock(hMem);
      // set data in clipboard; we are no longer responsible for hMem
      ok=(SetClipboardData(CF_TEXT,hMem)!=0);
      CloseClipboard(); // relinquish it for other windows
    }
  }
  return ok;
}

#endif//WIN32

#ifdef UNIX

#ifdef DEADC0DE

EasyStr GetEXEDir() {
  EasyStr Path=_argv[0];
  RemoveFileNameFromPath(Path,REMOVE_SLASH);
  return Path;
}

EasyStr GetCurrentDir() {
  EasyStr Path=GetEXEDir();
  Path.SetLength(MAX_PATH);
  getcwd(Path,MAX_PATH);
  return Path;
}

#endif

EasyStr GetEXEFileName() {
  return _argv[0];
}

#endif


#ifdef BEOS
#include "beos/be_mymisc.cpp"
#endif

size_t pbm_save(pbm_image *img, FILE *outfile) {
    size_t n = 0;
    n += fprintf(outfile, "P4 %d %d\n",img->width,img->height);
    n += fwrite(img->data, 1, img->size, outfile);
    return n;
}



/* CRC32
https://stackoverflow.com/a/26051190
we use it for di_get_contents and archiveaccess*/

DWORD crcTable[256]; // 1 KB overhead isn't much for fast CRC32 computing

void make_crc32_table() { // call at init
  DWORD POLYNOMIAL = 0xEDB88320;
  DWORD remainder;
  BYTE b = 0;
  do {
    // Start with the data byte
    remainder = b;
    for (unsigned long bit = 8; bit > 0; --bit) {
      if (remainder & 1)
        remainder = (remainder >> 1) ^ POLYNOMIAL;
      else
        remainder = (remainder >> 1);
    }
    crcTable[(size_t)b] = remainder;
  } while(0 != ++b);
}


void add_to_crc32(DWORD &crc, const BYTE *buf, size_t len) {
  // crc should be init to ones and final value should be bit reversed
  size_t i;
  for(i = 0; i < len; i++)
    crc = crcTable[*buf++ ^ (crc&0xff)] ^ (crc>>8);
}


DWORD GetCRCFromMemory(BYTE *mem,size_t len) {
  DWORD CRC=~0u;
  add_to_crc32(CRC,mem,len);
  CRC=~CRC;
  return CRC;
}


DWORD GetCRCFromFile(char *Fil) {
  FILE *fp;
  LONG_PTR Len;
  BYTE *Block;
  DWORD crc=0;
  fp=fopen(Fil,"rb");
  if(fp==NULL)
    return 0;
  FSEEK(fp,0,SEEK_END);
  Len=(LONG_PTR)FTELL(fp);
  FSEEK(fp,0,SEEK_SET);
  if((Block=(BYTE*)malloc(Len))!=NULL)
  {
    FREAD(Block,1,Len,fp);
    crc=GetCRCFromMemory(Block,Len);
    free(Block);
  }
  fclose(fp);
  return crc;
}


void ms_to_hms(DWORD ms, DWORD &h,DWORD &m,DWORD &s) {
  s=ms/1000;
  h=s/(60*60);
  s=s%(60*60);
  m=s/60;
  s=s%60;
}


void GetNewFileName(EasyStr &filename) {
  // build a new name based on filename, by putting a number between
  // () before the extension Eg: TRACE(1).txt
  // filename is altered
  int l=(int)filename.Length();
  const char *t=filename.Text;
  int dot_pos=0,newnum=0;
  for(int i=l;i>0;i--)
  {
    if(t[i]==SLASHCHAR)
      break;
    if(t[i]=='.')
    {
      dot_pos=i;
      break;
    }
  }
  int p=(dot_pos)?dot_pos:l;
  if(p>0)
  {
    int q=p;
    if(t[p-1]==')') // assume it's a number
    {
      char s[64];
      memset(s,0,64);
      while(q>0 && t[q]!='(')
        q--;
      if(q>0 && p-q>1 && p-q<64)
      {
        strncpy(s,&t[q+1],p-q-1);
        newnum=atoi(s);
        if(!newnum)
          q=p; // add ()
      }
    }
    newnum++;
    EasyStr et=filename.Mids(0,q)+"("+newnum+")";
    if(dot_pos)
      et+=filename.Mids(dot_pos,l-dot_pos);
    filename=et;
  }
}


void nuke_last_slash(char *path) { // = NO_SLASH
  if(path[0])
  {
    int last=(int)strlen(path)-1;
    if(path[last]=='/'
#ifdef WIN32
      || path[last]=='\\'
#endif
      )
      path[last]='\0';
  }
}


void DigitsOnly(char* string) {
  // keep only digits in string
  if(string==NULL)
    return;
  size_t l=strlen(string);
  char *dst=string;
  char *src=string;
  char *end=src+l;
  while(src<end)
  {
    if(*src>='0'&&*src<='9'||*src=='-')
      *dst++=*src;
    src++;
  }
  if(dst<end)
    *dst='\0';
}


// write header for an RTF file
void StartRtf(FILE* fp) {
  fprintf(fp,"{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\froman Times;}\
{\\f1\\fswiss Arial;}{<\\f2<\\fmodern Courier New;}}\
{\\colortbl;\\red0\\green0\\blue0;\\red0\\green0\\blue255;\
\\red0\\green255\\blue255;\\red0\\green255\\blue0;\\red255\\green0\\blue255;\
\\red255\\green0\\blue0;\\red255\\green255\\blue0;\\red255\\green255\\blue255;\
\\red0\\green0\\blue128;\\red0\\green128\\blue128;\\red0\\green128\\blue0;\
\\red128\\green0\\blue128;\\red128\\green0\\blue0;\\red128\\green128\\blue0;\
\\red128\\green128\\blue128;\\red192\\green192\\blue192;}");
}


int abs_quicki(int i) {
  if(i>=0) 
    return i;
  return -i;
}


COUNTER_VAR abs_quick(COUNTER_VAR i) {
  if(i>=0) 
    return i;
  return -i;
}
