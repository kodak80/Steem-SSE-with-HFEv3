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
FILE: mymisc.h
DESCRIPTION: Many miscellaneous functions from all areas of programming that
just refuse to be categorized.
struct TWinPositionData, TWidthHeight, TModifierState, pbm_image
---------------------------------------------------------------------------*/

#pragma once
#ifndef MYMISC_H
#define MYMISC_H

#include <stdio.h>
#include "easystr.h"
#include <SSE.h>
#include <conditions.h>

#ifdef WIN32
#include <Windows.h>
#include <io.h>
#define WIN_ONLY(a) a
#define UNIX_ONLY(a)
#define SLASH "\\"
#define SLASHCHAR '\\'
#include <time.h>
#include <CommCtrl.h>
#include <ShlObj.h>
#endif

#ifdef UNIX
#ifndef WIN_ONLY
#define WIN_ONLY(a)
#endif
#define UNIX_ONLY(a) a
#define SLASH "/"
#define SLASHCHAR '/'
typedef Window WINDOWTYPE;
#endif

#ifdef MINGW_BUILD
#undef NULL
#define NULL 0
#endif


void nuke_last_slash(char *path);
#define NO_SLASH(path) nuke_last_slash(path)

#define REMOVE_LAST_SLASH true
#define REMOVE_SLASH true
#define WITH_SLASH false
#define KEEP_SLASH false
#define WITHOUT_SLASH true


//#define CenterWindow CentreWindow //not if NO_US defined!

#define BOUND(Val,Min,Max) ( ((Val)<(Min))?(Min):( ((Val)>(Max))?(Max):(Val) ) )




// SWAP_BIG_ENDIAN: swap big endian data if we use a little endian processor (such as Intel, AMD)
// SWAP_LITTLE_ENDIAN: swap little endian data if we use a big endian processor (such as ...?)
#if defined(BIG_ENDIAN_PROCESSOR) // not used, not tested
#define SWAP_BIG_ENDIAN_WORD(val)
#define SWAP_BIG_ENDIAN_DWORD(val)
//#define SWAP_LITTLE_ENDIAN_WORD(val) val=_byteswap_ushort(val) // intrinsics
//#define SWAP_LITTLE_ENDIAN_DWORD(val) val=_byteswap_ulong(val)
#define BYTESWAP16(n) (((n&0xFF00)>>8)|((n&0x00FF)<<8))
#define BYTESWAP32(n) ((BYTESWAP16((n&0xFFFF0000)>>16))|((BYTESWAP16(n&0x0000FFFF))<<16))
#define BYTESWAP64(n) ((BYTESWAP32((n&0xFFFFFFFF00000000)>>32))|((BYTESWAP32(n&0x00000000FFFFFFFF))<<32))
#define SWAP_LITTLE_ENDIAN_WORD(val) val=BYTESWAP16(val)
#define SWAP_LITTLE_ENDIAN_DWORD(val) val=BYTESWAP32(val)
#elif defined(SSE_VC_INTRINSICS) // Intel = little endian
#define SWAP_BIG_ENDIAN_WORD(val) val=_byteswap_ushort(val)
#define SWAP_BIG_ENDIAN_DWORD(val) val=_byteswap_ulong(val)
#define SWAP_LITTLE_ENDIAN_WORD(val)
#define SWAP_LITTLE_ENDIAN_DWORD(val)
#else // Intel = little endian
#if 1 // warning BCC, don't understand (acsi 117)
#define BYTESWAP16(n) ( (((n)&0xFF00)>>8) | (((n)&0x00FF)<<8) )
#else
#define BYTESWAP16(n) (((n&0xFF00)>>8)|((n&0x00FF)<<8))
#endif
#define BYTESWAP32(n) ((BYTESWAP16((n&0xFFFF0000)>>16))|((BYTESWAP16(n&0x0000FFFF))<<16))
#define BYTESWAP64(n) ((BYTESWAP32((n&0xFFFFFFFF00000000)>>32))|((BYTESWAP32(n&0x00000000FFFFFFFF))<<32))
#define SWAP_BIG_ENDIAN_WORD(val) val=BYTESWAP16(val)
#define SWAP_BIG_ENDIAN_DWORD(val) val=BYTESWAP32(val)
#define SWAP_LITTLE_ENDIAN_WORD(val)
#define SWAP_LITTLE_ENDIAN_DWORD(val)
#endif


#pragma pack(push, 8)

struct TWinPositionData {
  int Left,Top;
  int Width,Height;
  bool Maximized,Minimized;
};


struct TWidthHeight {
  LONG Width;
  LONG Height;
};

#pragma pack(pop)


DWORD HexToVal(char *HexStr);
#ifdef SSE_X64
unsigned long long HexToVall(char *HexStr);
#endif


int InsertCommas(char *dest,LONGLONG bignumber);
//int InsertCommas(char *dest,int bignumber);


//bool LoadBool(FILE *f);
//void SaveBool(bool b,FILE *f);
//int LoadInt(FILE *f);
//void SaveInt(int i,FILE *f);
//void LoadChars(char *buf,FILE *f);
//void SaveChars(char *buf,FILE *f);
#if defined(SSE_BIGFILES)
INT64 GetFileLength(FILE *fp);
#else
LONG GetFileLength(FILE *fp);
#endif
#if defined(SSE_LONG_PATH)
INT64 GetFileLength(HANDLE fp);
void PathPrePend(EasyStr path,bool bAddOrRemove); // true=add, false=remove
#endif
char *GetFileNameFromPath(char *fil);
void RemoveFileNameFromPath(char *fil,bool rem);
bool has_extension_list(char *Filename,char *Ext,...);
bool has_extension(char *Filename,char *Ext);
//bool MatchesAnyString(char *StrToCompare,char *Str,...);
bool MatchesAnyString_I(char *StrToCompare,char *Str,...);
int log_to_base_2(unsigned long x);
EasyStr GetUniquePath(EasyStr path,EasyStr name);
DWORD TMToDOSDateTime(struct tm *lpTime);

#pragma pack(push, 8)

struct TModifierState {
  bool LShift,RShift;
  bool LCtrl,RCtrl;
  bool LAlt,RAlt;
};

#pragma pack(pop)

#ifdef WIN32
#define SetPropI(w,s,dw) SetProp(w,s,(HANDLE)(dw))
#define GetPropI(w,s) UINT_PTR(GetProp(w,s))
void RemoveProps(HWND Win,char *Prop1,...);
//void Border3D(HDC dc,int x,int y,int w,int h,
//              DWORD col0,DWORD col1,DWORD col2,DWORD col3);
//void Box3D(HDC dc,int x,int y,int w,int h,bool d);
void CentreTextOut(HDC dc,int x,int y,int w,int h,char *text,int len);
#ifdef MINGW_BUILD
#undef GetLongPathName
#endif
#ifndef SSE_WINDOWS_2000_MIN
void GetLongPathName(char *src,char *dest,int maxlen);
#endif
void SetWindowAndChildrensFont(HWND Win,HFONT font);
void RemoveAllMenuItems(HMENU menu),DeleteAllMenuItems(HMENU menu);
#if !defined(SSE_VID_2SCREENS)
void CentreWindow(HWND Win,bool Redraw);
#endif
#define RegValueExists(Key,Name) \
  (RegQueryValueEx(Key,Name,NULL,NULL,NULL,NULL)==ERROR_SUCCESS)
//bool RegKeyExists(HKEY Key,char *Name);
//bool WindowIconsAre256();
void DisplayLastError(char *TitleText=NULL);
HFONT MakeFont(char *Typeface,int Height,int Width=0,int Boldness=FW_NORMAL,
               bool Italic=false,bool Underline=false,bool Strikeout=false);
COLORREF GetMidColour(COLORREF RGB1,COLORREF RGB2);
COLORREF DimColour(COLORREF Col,double DimAmount);
//EasyStr GetCurrentDir();
EasyStr GetEXEDir();

bool GetWindowPositionData(HWND Win,TWinPositionData *wpd);
//EasyStr GetPPEasyStr(char *SectionName,char *KeyName,char *Default,char *FileName);
EasyStr FileSelect(HWND Owner,char *Title,char *DefaultDir,char *Types,
                   int InitType,int LoadFlag,EasyStr DefExt="",char *DefFile="");

EasyStr GetLinkDest(EasyStr LinkFileName,WIN32_FIND_DATA *wfd,HWND UIParent=NULL,
                     IShellLink *Link=NULL,IPersistFile* File=NULL);
HRESULT CreateLink(char *LinkFileName,char *TargetFileName,char *Description=NULL,
                    IShellLink *Link=NULL,IPersistFile* File=NULL,
                    char *IconPath=NULL,int IconIdx=0,bool NoOverwrite=false);

//void DeleteDirAndContents(char *Dir);
void CentreLVItem(HWND LV,int iItem,LRESULT State=-1);
void GetTabControlPageSize(HWND Tabs,RECT *rc);



TWidthHeight GetTextSize(HFONT Font,char *Text);
TWidthHeight GetCheckBoxSize(HFONT Font=NULL,char *Text=NULL);

#ifdef DEADC0DE
typedef bool (WINAPI *LPTOOLHELPMODULEWALK)(HANDLE,LPMODULEENTRY32);
typedef HANDLE (WINAPI *LPTOOLHELPCREATESNAPSHOT)(DWORD,DWORD);
void GetWindowExePaths(HWND Win,char *Buf,int BufLen);
#endif

char *RemoveIllegalFromPath(char *Path,bool DriveIncluded,bool RemoveWild=true,
                            char ReplaceChar='-',bool STPath=false);
char *RemoveIllegalFromName(char *Name,bool RemoveWild=true,char ReplaceChar='-');
LPARAM lParamPointsToParent(HWND Win,LPARAM lPar);
LRESULT CBAddString(HWND Combo,char *String);
LRESULT CBAddString(HWND Combo,char *String,LONG_PTR Data);
//LRESULT CBAddString(HWND Combo,wchar_t *String,LONG_PTR Data); // overload
LRESULT CBFindItemWithData(HWND Combo,LONG_PTR Data);
LRESULT CBSelectItemWithData(HWND Combo,LONG_PTR Data);
LRESULT CBGetSelectedItemData(HWND Combo);
//void MoveWindowClient(HWND Win,int x,int y,int w,int h);
void GetWindowRectRelativeToParent(HWND Win,RECT *pRc);
#define IsCachedPrivateProfile() (0)
#define UnCachePrivateProfile()
#define CachePrivateProfile(a)


//void ToolsDeleteWithIDs(HWND,HWND,DWORD,...);
void ToolsDeleteAllChildren(HWND hToolTip,HWND hParent);
void ToolAddWindow(HWND hToolTip,HWND Handle,char *Text);

HTREEITEM TreeSelectItemWithData(HWND Tree,long n,HTREEITEM Item=TVI_ROOT);
int TreeGetMaxItemWidth(HWND Tree,HTREEITEM Item=TVI_ROOT,int Level=0);

#define PAD_ALIGN_CENTRE b0000
#define PAD_ALIGN_LEFT   b0001
#define PAD_ALIGN_RIGHT  b0010
#define PAD_ALIGN_TOP    b0100
#define PAD_ALIGN_BOTTOM b1000
#define PAD_NAMES    b10000000

void ImageList_AddPaddedIcons(HIMAGELIST il,int Align,char *sIco,...);
void ImageList_AddPaddedIcons(HIMAGELIST il,int Align,int nIco,...);
void ImageList_AddPaddedIcons(HIMAGELIST il,int Align,HICON Icon1,...);

//int LVGetSelItem(HWND);
//EasyStr LVGetItemText(HWND,int);

EasyStr GetWindowTextStr(HWND Win);
//EasyStr LoadWholeFileIntoStr(char *File);
//bool SaveStrAsFile(EasyStr &s,char *File);
EasyStr ShortenPath(EasyStr Path,HFONT Font,int MaxWidth);
TModifierState GetLRModifierStates();
HDC CreateScreenCompatibleDC();
//HBITMAP CreateScreenCompatibleBitmap(int,int);
BOOL SetClipboardText(LPCTSTR pszText);

#else

#include "notwin_mymisc.h"

#endif//WIN32

EasyStr GetEXEFileName();

#ifdef UNIX
#include "x/x_mymisc.h"
#endif

#ifdef BEOS
#include "beos/be_mymisc.h"
#endif


int abs_quicki(int i);
COUNTER_VAR abs_quick(COUNTER_VAR i);

typedef struct {
    int width;
    int height;
    uint8_t *data;
    size_t size;
} pbm_image;


size_t pbm_save(pbm_image *img, FILE *outfile);


void make_crc32_table();
void add_to_crc32(DWORD &crc, const BYTE *buf, size_t len);
void ms_to_hms(DWORD ms, DWORD &h,DWORD &m,DWORD &s);
DWORD GetCRCFromFile(char *Fil);
void GetNewFileName(EasyStr &filename);
void DigitsOnly(char *string);
void StartRtf(FILE *fp);

#endif//#ifndef MYMISC_H
