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
FILE: acc.cpp
DESCRIPTION: Completely random accessory functions.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <computer.h>
#include <display.h>
#ifdef DEBUG_BUILD
#include <debugger.h>
#endif


#if !defined(SSE_NO_UPDATE)

EasyStr time_or_times(int n) {
  if(n==1)return T("time");
  return T("times");
}

#endif


LONG colour_convert(int red,int green,int blue) {
  return ((red<<16)|(green<<8)|blue)<<rgb32_bluestart_bit;
}


EasyStr HEXSl(LONG n,int ln) {
  char bf[17];
  strcpy(bf,"00000000"); // 8 zeroes, 8 digits + null free
  itoa(n,bf+8,16); // write hex right after zeroes
#ifdef DEBUG_BUILD
  if(debug_uppercase_disa)
#endif
    strupr(bf);
#if defined(SSE_420R5)
  EasyStr rv(bf+8-ln+strlen(bf+8)); // create an object to be returned
  return rv;
#else
  return bf+8-ln+strlen(bf+8); // return value is not an EasyStr but pointer to local. buggy?
#endif
}


#ifdef SSE_X64

EasyStr HEXSll(long long n,int ln) {
  char bf[17+8];
  strcpy(bf,"0000000000000000");
#ifdef UNIX
  sprintf(bf+8,"%lld",n);
#else
  _i64toa(n,bf+8,16);
#endif
#ifdef DEBUG_BUILD
  if(debug_uppercase_disa)
#endif
    strupr(bf);
#if defined(SSE_420R5)
  EasyStr rv(bf+8-ln+strlen(bf+8));
  return rv;
#else
  return bf+8-ln+strlen(bf+8);
#endif
}

#endif


void parse_search_string(Str OriginalText,DynamicArray<BYTE> &ByteList,bool &WordOnly) {
  // used by debugger and patches
  bool ReturnLengths=WordOnly;
  ByteList.DeleteAll();
  WordOnly=false;
  char *Buf=new char[OriginalText.Length()+1];
  strcpy(Buf,OriginalText);
  for(INT_PTR i=0;i<OriginalText.Length();i++)
  {
    if(Buf[i]==' '||Buf[i]=='\t')
      Buf[i]='\0';
  }
  char *pBuf=Buf,*pBufEnd=Buf+OriginalText.Length();
  while(pBuf<pBufEnd)
  {
    Str Text=pBuf;
    if(Text[0]=='\"'||(Text[0]>'F' && Text[0]<='Z')||(Text[0]>'f' && Text[0]<='z'))
    {
      if(Text[0]=='\"')
        Text.Delete(0,1);
      if(Text.RightChar()=='\"')
        *(Text.Right())='\0';
      for(INT_PTR i=0;i<Text.Length();i++)
      {
        ByteList.Add(Text[i]);
        if(ReturnLengths)
          ByteList.Add(1);
      }
    }
#if defined(SSE_DEBUG_SYMBOLS)
    else if(Text[0]=='@')
    {
      char SymbolName[24];
      strncpy(SymbolName,&Text[1],22);
      for(int i=0;i<MAX_SYMBOLS&&TosSymbol[i].ad!=0xFFFFFFFF;i++) // check all symbols
      {
        TTosSymbol &x=TosSymbol[i];
        if(!strncmp(SymbolName,x.name,22))
        {
          MEM_ADDRESS ad=x.ad;
          for(INT_PTR j=0;j<3;j++)
          {
            ByteList.Add(ad&0xFF);
            ad>>=8;
          }
        }
      }
    }
#endif
    else if(Text[0])
    {
      strupr(Text);
      if(Text.RightChar()=='W')
      {
        *(Text.Right())='\0'; // Just in case this messes with the atoi etc..
        WordOnly=true;
      }
      DWORD Num=0;
      INT_PTR NumLen=0;
      if(Text.Lefts(2)=="0X"||Text[0]=='$'||Text[0]>='A' && Text[0]<='F')
      {
        char *t=Text.Text;
        if(t[1]=='X')
          t+=2;
        else if(t[0]=='$')
          t++;
        int HexLen=0;
        while(*t)
        {
          if(((*t)>='A'&&(*t)<='F')==0&&((*t)>='0'&&(*t)<='9')==0)
            break;
          HexLen++;
          t++;
        }
        if(HexLen>0)
        {
          NumLen=MIN((HexLen+1)/2,4);
          Num=HexToVal(Text);
        }
      }
      else if(Text[0]=='%')
      {  // Binary
        INT_PTR BinLen=0;
        for(;BinLen<Text.Length();BinLen++)
          if(Text[BinLen+1]!='0' && Text[BinLen+1]!='1')
            break;
        if(BinLen>0&&BinLen<=32)
        {
          NumLen=(BinLen+7)/8;
          int Bit=0;
          for(INT_PTR n=BinLen;n>0;n--)
          {
            if(Text[n]=='1')
              Num|=1<<Bit;
            Bit++;
          }
        }
      }
      else
      {                    // Decimal
        NumLen=0;
        if(Text.Rights(2)==(char*)".W")
        {
          NumLen=2;
          *(Text.Right()-1)='\0';
        }
        else if(Text.Rights(2)==(char*)".L")
        {
          NumLen=4;
          *(Text.Right()-1)='\0';
        }
        Num=(DWORD)atoi(Text);
        if((Num||Text[0]=='0'||Text.Lefts(2)=="-0")&&NumLen==0)
        {
          if(Num<=0xff)
            NumLen=1;
          else if(Num<=0xffff)
            NumLen=2;
          else if(Num<=0xffffff)
            NumLen=3;
          else
            NumLen=4;
        }
      }
      if(NumLen)
      {
#ifndef BIG_ENDIAN_PROCESSOR
        BYTE *lpHiNum=(LPBYTE)&Num+NumLen-1;
        int mem_dir=-1;
#else
        BYTE *lpHiNum=(LPBYTE)&Num;
        int mem_dir=1;
#endif
        for(int i=0;i<NumLen;i++)
        {
          ByteList.Add(*(lpHiNum+i*mem_dir));
          if(ReturnLengths)
            ByteList.Add((BYTE)NumLen);
        }
      }
    }
    pBuf+=strlen(pBuf)+1;
  }//wend
  delete[] Buf;
}


MEM_ADDRESS acc_find_bytes(DynamicArray<BYTE> &BytesToFind,bool WordOnly,MEM_ADDRESS ad,int dir) {
  // used by debugger and patches
  BYTE ToFind=BytesToFind[0];
  bool Found=false;
  int n;
  for(;;)
  {
    if(ad>=himem && ad<rom_addr)
      ad=(MEM_ADDRESS)( (dir>0) ? rom_addr : himem-1);
    if(ad>=rom_addr_end||ad>0xffffff)
      break;
    if(((ad&1)&&WordOnly)==0)
    { // if odd and word-only then skip byte
      n=-1;
      if(ad<himem)
      {
        if(PEEK(ad)==ToFind)
        {
          if(ad+BytesToFind.NumItems<=himem)
          {
            for(n=1;n<BytesToFind.NumItems;n++)
              if(PEEK(ad+n)!=BytesToFind[n])
                break;
          }
        }
      }
      else
      {
        if(ROM_PEEK(ad-rom_addr)==ToFind)
        {
          if(ad+BytesToFind.NumItems<=rom_addr_end)
          {
            for(n=1;n<BytesToFind.NumItems;n++)
              if(ROM_PEEK(ad+n-rom_addr)!=BytesToFind[n])
                break;
          }
        }
      }
      if(n>=BytesToFind.NumItems)
      {
        Found=true;
        break;
      }
    }
    ad+=dir;
  }
  if(Found)
    return ad;
  return 0xffffffff;
}


#ifdef WIN32

int get_text_width(char *t) {
  SIZE sz;
  HDC dc=GetDC(StemWin);
  HANDLE oldfnt=SelectObject(dc,fnt);
  GetTextExtentPoint32(dc,t,(int)strlen(t),&sz);
  SelectObject(dc,oldfnt);
  ReleaseDC(StemWin,dc);
  return sz.cx+1; // For grayed string
}


// first look in /plugins32 or /plusings64, then in /plugins
// then same folder as plugin name (pasti/pasti) then
// steem root; lpLibFileName should have no extension
HMODULE SteemLoadLibrary(LPCSTR lpLibFileName) {
  HMODULE hm=NULL;
  char rel_path[512];
  sprintf(rel_path,"%s\\%s",SSE_PLUGIN_DIR1,lpLibFileName);
  hm=LoadLibrary(rel_path);
  if(hm==NULL)
  {
    sprintf(rel_path,"%s\\%s",SSE_PLUGIN_DIR2,lpLibFileName);
    hm=LoadLibrary(rel_path);
  }
  if(hm==NULL)
  {
    sprintf(rel_path,"%s\\%s",lpLibFileName,lpLibFileName);
    hm=LoadLibrary(rel_path);
  }
  if(hm==NULL)
  {
    sprintf(rel_path,"%s",lpLibFileName);
    hm=LoadLibrary(rel_path);
  }
  // feedback, useful if a DLL won't load
  DWORD Err=(hm)?0:GetLastError();
#ifndef SSE_LEAN_AND_MEAN // verbose
  HLOCAL lpMsgBuf=NULL;
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,NULL,Err,
                MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(char*)&lpMsgBuf,0,NULL);
  TRACE("%s %s: %p %s","Load",rel_path,hm,lpMsgBuf);
  LocalFree(lpMsgBuf);
#else
  TRACE("%s %s: %p %s %d\n","Load",rel_path,hm,"ERROR",Err);
#endif
  return hm;
}


// just adding hLibModule=NULL
BOOL SteemFreeLibrary(HMODULE& hLibModule) {
  BOOL rv=FreeLibrary(hLibModule);
  hLibModule=NULL;
  return rv;
}


BOOL SteemFFlush(HANDLE fp) {
  return FlushFileBuffers(fp);
}


#endif//WIN32

int SteemFFlush(FILE* fp) {
#if defined(VC_BUILD)
  return _fflush_nolock(fp);
#else
  return fflush(fp);
#endif
}
