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
FILE: easystr.cpp
DESCRIPTION: A (supposedly) easy to use string class.
---------------------------------------------------------------------------*/


#include <pch.h>
#pragma hdrstop

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <easystr.h>

#ifdef MINGW_BUILD
#undef NULL
#define NULL 0
#endif


char EasyStr::numbuf[32];

#ifdef SSE_X64 // v411 (correct?)
char EasyStr::Empty_Text[8]={0,0,0,0,0,0,0,0}; // It is done like this so it can be written
                                        // to (in the case of EasyStr[0]=0
#else
char EasyStr::Empty_Text[4]={0,0,0,0}; // It is done like this so it can be written
                                        // to (in the case of EasyStr[0]=0
#endif

#if defined(SSE_420R5) // reduce x64 size

//------------------------------- Constructors ------------------------------

EasyStr::EasyStr() {
  bufsize=0;
  Text=Empty_Text;
}


EasyStr::EasyStr(char *nt) {
  if(nt!=NULL) //v4, no crash if null
  {
    bufsize=(int)strlen(nt);
    Text=new char[bufsize+1];
    strcpy(Text,nt);
  }
  else
  {
    bufsize=0;
    Text=Empty_Text;
  }
}


EasyStr::EasyStr(const char *nt) {
  bufsize=(int)strlen(nt);
  Text=new char[bufsize+1];
  strcpy(Text,nt);
}


EasyStr::EasyStr(char c) {
  bufsize=1;
  Text=new char[bufsize+1];
  Text[0]=c;Text[1]=0;
}


EasyStr::EasyStr(char *nt1,char *nt2) {
  bufsize=(int)strlen(nt1)+(int)strlen(nt2);
  Text=new char[bufsize+1];
  strcpy(Text,nt1);
  strcat(Text,nt2);
}


EasyStr::EasyStr(const EasyStr& nt) {
  bufsize=(int)strlen(nt.Text);
  Text=new char[bufsize+1];
  strcpy(Text,nt.Text);
}


EasyStr::EasyStr(int num) {
  itoa(num,numbuf,10);
  bufsize=(int)strlen(numbuf);
  Text=new char[bufsize+1];
  strcpy(Text,numbuf);
}


EasyStr::EasyStr(unsigned int num) {
#ifdef MINGW_BUILD
  _ultoa(num,numbuf,10);
#else
  ultoa(num,numbuf,10);
#endif
  bufsize=(int)strlen(numbuf);
  Text=new char[bufsize+1];
  strcpy(Text,numbuf);
}


EasyStr::EasyStr(long num) {
  itoa(num,numbuf,10);
  bufsize=(int)strlen(numbuf);
  Text=new char[bufsize+1];
  strcpy(Text,numbuf);
}


EasyStr::EasyStr(unsigned long num) {
#ifdef MINGW_BUILD
  _ultoa(num,numbuf,10);
#else
  ultoa(num,numbuf,10);
#endif
  bufsize=(int)strlen(numbuf);
  Text=new char[bufsize+1];
  strcpy(Text,numbuf);
}


EasyStr::EasyStr(bool num) {
  bufsize=1;
  Text=new char[bufsize+1];
  Text[0]=char(num?'1':'0');Text[1]=0;
}


#ifdef _WIN64

EasyStr::EasyStr(INT_PTR num) {
  _i64toa(num,numbuf,10);
  bufsize=(int)strlen(numbuf);
  Text=new char[bufsize+1];
  strcpy(Text,numbuf);
}


EasyStr::EasyStr(DWORD_PTR num) {
  _ui64toa(num,numbuf,10);
  bufsize=(int)strlen(numbuf);
  Text=new char[bufsize+1];
  strcpy(Text,numbuf);
}

#endif


//---------------------------- Member Functions -----------------------------

EasyStr::~EasyStr() { 
  DeleteBuf(); 
}


char *EasyStr::c_str() { 
  return Text; 
}


bool EasyStr::IsEmpty() { 
  return !(Text[0]); 
}


bool EasyStr::Empty() { 
  return !(Text[0]); 
}


bool EasyStr::IsNotEmpty() { 
  return (Text[0]!=0); 
}


bool EasyStr::NotEmpty() { 
  return (Text[0]!=0); 
}


int EasyStr::Length() { 
  return (int)strlen(Text); 
}


void EasyStr::SetLength(int size) { 
  ResizeBuf(size); 
}


void EasyStr::SetBufSize(int size) { 
  ResizeBuf(size); 
}


#ifdef DEADC0DE
INT_PTR EasyStr::GetBufSize() { 
  return bufsize; 
}
#endif


int EasyStr::CompareNoCase(char *otext) { 
  return strcmpi(Text,otext); 
}


void EasyStr::ResizeBuf(int size) {
  char *Old=Text;
  if(size==0)
    Text=Empty_Text;
  else
    Text=new char[size+1];
  bufsize=size;
  if(Text!=Empty_Text)
  {
    memcpy(Text,Old,MIN((int)strlen(Old)+1,bufsize));
    Text[bufsize]=0;
  }
  if(Old!=Empty_Text) 
    delete[] Old;
}


EasyStr& EasyStr::Delete(int start,int count) {
  start-=EASYSTR_BASE;
  if(count<=0||start<0) 
    return *this;
  int Len=(int)strlen(Text);
  if(start>=Len) 
    return *this;
  if(start+count>Len)
    //Delete to end of string
    Text[start]=0;
  else
    memmove(Text+start,Text+start+count,(Len-(start+count))+1); //(int)strlen(Text+start+count)+1
  return *this;
}


EasyStr& EasyStr::Insert(EasyStr ToAdd,int Pos) {
  int AddLen=(int)strlen(ToAdd.Text),CurLen=(int)strlen(Text);
  Pos-=EASYSTR_BASE;
  if(Pos<0||Pos>CurLen||AddLen==0) 
    return *this;
  if(CurLen+AddLen>bufsize) 
    ResizeBuf(CurLen+AddLen);
  memmove(Text+Pos+AddLen,Text+Pos,(CurLen-Pos)+1); //(int)strlen(Text+Pos)+1
  memcpy(Text+Pos,ToAdd.Text,AddLen);
  return *this;
}


char* EasyStr::Right() {
  if(Text[0])
    return Text+(int)strlen(Text)-1;
  else
    return Text;
}


char* EasyStr::Rights(int numchars) {
  if(Text[0])
  {
    int len=(int)strlen(Text);
    return Text+len-MIN(numchars,len);
  }
  return Text;
}


char EasyStr::RightChar() {
  if(Text[0]) 
    return Text[(int)strlen(Text)-1];
  return 0;
}


EasyStr EasyStr::Lefts(int numchars) {
  if(numchars>=(int)strlen(Text)) 
    return Text;
  EasyStr Temp;
  Temp.SetLength(numchars);
  memcpy(Temp.Text,Text,numchars);
  return Temp;
}


EasyStr EasyStr::Mids(int firstchar,int numchars) {
  firstchar-=EASYSTR_BASE;
  if(firstchar>=(int)strlen(Text)) 
    return "";
  EasyStr temp(Text+firstchar);
  temp.SetLength(numchars);
  return temp;
}


EasyStr EasyStr::UpperCase() {
  EasyStr temp(Text);
  strupr(temp.Text);
  return temp;
}


EasyStr EasyStr::LowerCase() {
  EasyStr temp(Text);
  strlwr(temp.Text);
  return temp;
}


EasyStr& EasyStr::LPad(int NewLen,char c) {
  int CurLen=(int)strlen(Text);
  if(NewLen<=CurLen) 
    return *this;
  int LenToAdd=NewLen-CurLen;
  EasyStr AddChars;
  AddChars.SetLength(LenToAdd);
  memset(AddChars.Text,c,LenToAdd);
  Insert(AddChars,EASYSTR_BASE);
  return *this;
}


EasyStr& EasyStr::RPad(int NewLen,char c) {
  int CurLen=(int)strlen(Text);
  if(NewLen<=CurLen) 
    return *this;
  SetLength(NewLen);
  memset(Text+CurLen,c,NewLen-CurLen);
  return *this;
}


int EasyStr::InStr(char *ss) {
  char *Pos=strstr(Text,ss);
  if(Pos) 
    return (int)((INT_PTR)(Pos)-(INT_PTR)(Text));
  return -1;
}


//----------------------------- Operators -----------------------------------

EasyStr::operator char*() {
  return Text; 
}


#if !defined(VC_BUILD) && !defined(MINGW_BUILD) && !defined(UNIX)

EasyStr::operator struct EasyStr_Dummy_Struct*() { 
  return NULL; 
}

#if defined(SSE_420R5)
char& EasyStr::operator[](DWORD idx) {
#else
char& EasyStr::operator[](UINT_PTR idx) {
#endif
#if EASYSTR_BASE>0
  idx-=EASYSTR_BASE;
#endif
#ifdef EASYSTR_FOOLPROOF
  idx=MAX(min(idx,Length()),0);
#endif
  return Text[idx];
}

#endif


EASYSTR_OPERATORS(char*,p)

EASYSTR_OPERATORS(const char*,p)

EASYSTR_OPERATORS(EasyStr&,p.Text)

EASYSTR_OPERATORS(EasyStr*,p->Text)

EASYSTR_OPERATORS(signed int,itoa(p,numbuf,10))

#ifdef _WIN64
EASYSTR_OPERATORS(INT_PTR,_i64toa(p,numbuf,10))
EASYSTR_OPERATORS(DWORD_PTR,_ui64toa(p,numbuf,10))
#endif

EASYSTR_OPERATORS(signed short,itoa(p,numbuf,10))

EASYSTR_OPERATORS(signed long,itoa(p,numbuf,10))


#ifdef MINGW_BUILD
EASYSTR_OPERATORS(unsigned int,_ultoa(p,numbuf,10))
EASYSTR_OPERATORS(unsigned short,_ultoa((unsigned long)p,numbuf,10))
EASYSTR_OPERATORS(unsigned long,_ultoa((unsigned long)p,numbuf,10))
EASYSTR_OPERATORS(unsigned char,_ultoa((unsigned long)p,numbuf,10))
#else
EASYSTR_OPERATORS(unsigned int,ultoa(p,numbuf,10))
EASYSTR_OPERATORS(unsigned short,ultoa(p,numbuf,10))
EASYSTR_OPERATORS(unsigned long,ultoa(p,numbuf,10))
EASYSTR_OPERATORS(unsigned char,ultoa(p,numbuf,10))
#endif


EASYSTR_OPERATORS(bool,(char*)(p? "1" : "0"))

EASYSTR_OPERATORS(char,CharToString(p))

EasyStr& EasyStr::EqualsString(const char *nt) {
  int len=nt?(int)strlen(nt):0; //v4, no crash if null
  bool DoResize=false;
  if(len>bufsize||len<bufsize-128||len==0) 
    DoResize=true;
  if(nt>=Text && nt<Text+bufsize) 
    DoResize=false; // Setting to itself
  if(DoResize)
  {
    DeleteBuf();
    bufsize=len;
    if(len)
      Text=new char[bufsize+1];
    else
      Text=Empty_Text;
  }
  if(Text!=Empty_Text && nt)
    strcpy(Text,nt);
  return *this;
}


EasyStr EasyStr::PlusString(const char *nt) {
  return EasyStr(Text,(char*)nt);
}


EasyStr& EasyStr::PlusEqualsString(const char *new_text) {
  EasyStr nt=new_text;  // Just in case new_text is pointer in this string
  int len=nt.Length();
  if((int)strlen(Text)+len>bufsize) 
    ResizeBuf(len+(int)strlen(Text));
  strcat(Text,nt);
  return *this;
}


bool EasyStr::SameAsString(const char *otext) {
  return (strcmp(Text,otext)==0);
}


bool EasyStr::NotSameAsString(const char *otext) {
  return (strcmp(Text,otext)!=0);
}

#else//#if defined(SSE_420R5)

//------------------------------- Constructors ------------------------------

EasyStr::EasyStr() {
  bufsize=0;
  Text=Empty_Text;
}


EasyStr::EasyStr(char *nt) {
  if(nt!=NULL) //v4, no crash if null
  {
    bufsize=strlen(nt);
    Text=new char[bufsize+1];
    strcpy(Text,nt);
  }
  else
  {
    bufsize=0;
    Text=Empty_Text;
  }
}


EasyStr::EasyStr(const char *nt) {
  bufsize=strlen(nt);
  Text=new char[bufsize+1];
  strcpy(Text,nt);
}


EasyStr::EasyStr(char c) {
  bufsize=1;
  Text=new char[bufsize+1];
  Text[0]=c;Text[1]=0;
}


EasyStr::EasyStr(char *nt1,char *nt2) {
  bufsize=strlen(nt1)+strlen(nt2);
  Text=new char[bufsize+1];
  strcpy(Text,nt1);
  strcat(Text,nt2);
}


EasyStr::EasyStr(const EasyStr& nt) {
  bufsize=strlen(nt.Text);
  Text=new char[bufsize+1];
  strcpy(Text,nt.Text);
}


EasyStr::EasyStr(int num) {
  itoa(num,numbuf,10);
  bufsize=strlen(numbuf);
  Text=new char[bufsize+1];
  strcpy(Text,numbuf);
}


EasyStr::EasyStr(unsigned int num) {
#ifdef MINGW_BUILD
  _ultoa(num,numbuf,10);
#else
  ultoa(num,numbuf,10);
#endif
  bufsize=strlen(numbuf);
  Text=new char[bufsize+1];
  strcpy(Text,numbuf);
}


EasyStr::EasyStr(long num) {
  itoa(num,numbuf,10);
  bufsize=strlen(numbuf);
  Text=new char[bufsize+1];
  strcpy(Text,numbuf);
}


EasyStr::EasyStr(unsigned long num) {
#ifdef MINGW_BUILD
  _ultoa(num,numbuf,10);
#else
  ultoa(num,numbuf,10);
#endif
  bufsize=strlen(numbuf);
  Text=new char[bufsize+1];
  strcpy(Text,numbuf);
}


EasyStr::EasyStr(bool num) {
  bufsize=1;
  Text=new char[bufsize+1];
  Text[0]=char(num?'1':'0');Text[1]=0;
}


#ifdef _WIN64

EasyStr::EasyStr(INT_PTR num) {
  _i64toa(num,numbuf,10);
  bufsize=strlen(numbuf);
  Text=new char[bufsize+1];
  strcpy(Text,numbuf);
}


EasyStr::EasyStr(DWORD_PTR num) {
  _ui64toa(num,numbuf,10);
  bufsize=strlen(numbuf);
  Text=new char[bufsize+1];
  strcpy(Text,numbuf);
}

#endif


//---------------------------- Member Functions -----------------------------

EasyStr::~EasyStr() { 
  DeleteBuf(); 
}


char *EasyStr::c_str() { 
  return Text; 
}


bool EasyStr::IsEmpty() { 
  return !(Text[0]); 
}


bool EasyStr::Empty() { 
  return !(Text[0]); 
}


bool EasyStr::IsNotEmpty() { 
  return (Text[0]!=0); 
}


bool EasyStr::NotEmpty() { 
  return (Text[0]!=0); 
}


INT_PTR EasyStr::Length() { 
  return strlen(Text); 
}


void EasyStr::SetLength(INT_PTR size) { 
  ResizeBuf(size); 
}


void EasyStr::SetBufSize(INT_PTR size) { 
  ResizeBuf(size); 
}


#ifdef DEADC0DE
INT_PTR EasyStr::GetBufSize() { 
  return bufsize; 
}
#endif


int EasyStr::CompareNoCase(char *otext) { 
  return strcmpi(Text,otext); 
}


void EasyStr::ResizeBuf(INT_PTR size) {
  char *Old=Text;
  if(size==0)
    Text=Empty_Text;
  else
    Text=new char[size+1];
  bufsize=size;
  if(Text!=Empty_Text)
  {
    memcpy(Text,Old,MIN((INT_PTR)strlen(Old)+1,bufsize));
    Text[bufsize]=0;
  }
  if(Old!=Empty_Text) 
    delete[] Old;
}


EasyStr& EasyStr::Delete(INT_PTR start,INT_PTR count) {
  start-=EASYSTR_BASE;
  if(count<=0||start<0) 
    return *this;
  INT_PTR Len=strlen(Text);
  if(start>=Len) 
    return *this;
  if(start+count>Len)
    //Delete to end of string
    Text[start]=0;
  else
    memmove(Text+start,Text+start+count,(Len-(start+count))+1); //strlen(Text+start+count)+1
  return *this;
}


EasyStr& EasyStr::Insert(EasyStr ToAdd,INT_PTR Pos) {
  INT_PTR AddLen=strlen(ToAdd.Text),CurLen=strlen(Text);
  Pos-=EASYSTR_BASE;
  if(Pos<0||Pos>CurLen||AddLen==0) 
    return *this;
  if(CurLen+AddLen>bufsize) 
    ResizeBuf(CurLen+AddLen);
  memmove(Text+Pos+AddLen,Text+Pos,(CurLen-Pos)+1); //strlen(Text+Pos)+1
  memcpy(Text+Pos,ToAdd.Text,AddLen);
  return *this;
}


char* EasyStr::Right() {
  if(Text[0])
    return Text+strlen(Text)-1;
  else
    return Text;
}


char* EasyStr::Rights(INT_PTR numchars) {
  if(Text[0])
  {
    INT_PTR len=strlen(Text);
    return Text+len-MIN(numchars,len);
  }
  return Text;
}


char EasyStr::RightChar() {
  if(Text[0]) 
    return Text[strlen(Text)-1];
  return 0;
}


EasyStr EasyStr::Lefts(INT_PTR numchars) {
  if(numchars>=(INT_PTR)strlen(Text)) 
    return Text;
  EasyStr Temp;
  Temp.SetLength(numchars);
  memcpy(Temp.Text,Text,numchars);
  return Temp;
}


EasyStr EasyStr::Mids(INT_PTR firstchar,INT_PTR numchars) {
  firstchar-=EASYSTR_BASE;
  if(firstchar>=(INT_PTR)strlen(Text)) 
    return "";
  EasyStr temp(Text+firstchar);
  temp.SetLength(numchars);
  return temp;
}


EasyStr EasyStr::UpperCase() {
  EasyStr temp(Text);
  strupr(temp.Text);
  return temp;
}


EasyStr EasyStr::LowerCase() {
  EasyStr temp(Text);
  strlwr(temp.Text);
  return temp;
}


EasyStr& EasyStr::LPad(INT_PTR NewLen,char c) {
  INT_PTR CurLen=strlen(Text);
  if(NewLen<=CurLen) 
    return *this;
  INT_PTR LenToAdd=NewLen-CurLen;
  EasyStr AddChars;
  AddChars.SetLength(LenToAdd);
  memset(AddChars.Text,c,LenToAdd);
  Insert(AddChars,EASYSTR_BASE);
  return *this;
}


EasyStr& EasyStr::RPad(INT_PTR NewLen,char c) {
  INT_PTR CurLen=strlen(Text);
  if(NewLen<=CurLen) 
    return *this;
  SetLength(NewLen);
  memset(Text+CurLen,c,NewLen-CurLen);
  return *this;
}


INT_PTR EasyStr::InStr(char *ss) {
  char *Pos=strstr(Text,ss);
  if(Pos) 
    return (LONG_PTR)(Pos)-(LONG_PTR)(Text);
  return -1;
}


//----------------------------- Operators -----------------------------------

EasyStr::operator char*() {
  return Text; 
}


#if !defined(VC_BUILD) && !defined(MINGW_BUILD) && !defined(UNIX)

EasyStr::operator struct EasyStr_Dummy_Struct*() { 
  return NULL; 
}


char& EasyStr::operator[](UINT_PTR idx) {
#if EASYSTR_BASE>0
  idx-=EASYSTR_BASE;
#endif
#ifdef EASYSTR_FOOLPROOF
  idx=MAX(min(idx,Length()),0);
#endif
  return Text[idx];
}

#endif


EASYSTR_OPERATORS(char*,p)

EASYSTR_OPERATORS(const char*,p)

EASYSTR_OPERATORS(EasyStr&,p.Text)

EASYSTR_OPERATORS(EasyStr*,p->Text)

EASYSTR_OPERATORS(signed int,itoa(p,numbuf,10))

#ifdef _WIN64
EASYSTR_OPERATORS(INT_PTR,_i64toa(p,numbuf,10))
EASYSTR_OPERATORS(DWORD_PTR,_ui64toa(p,numbuf,10))
#endif

EASYSTR_OPERATORS(signed short,itoa(p,numbuf,10))

EASYSTR_OPERATORS(signed long,itoa(p,numbuf,10))


#ifdef MINGW_BUILD
EASYSTR_OPERATORS(unsigned int,_ultoa(p,numbuf,10))
EASYSTR_OPERATORS(unsigned short,_ultoa((unsigned long)p,numbuf,10))
EASYSTR_OPERATORS(unsigned long,_ultoa((unsigned long)p,numbuf,10))
EASYSTR_OPERATORS(unsigned char,_ultoa((unsigned long)p,numbuf,10))
#else
EASYSTR_OPERATORS(unsigned int,ultoa(p,numbuf,10))
EASYSTR_OPERATORS(unsigned short,ultoa(p,numbuf,10))
EASYSTR_OPERATORS(unsigned long,ultoa(p,numbuf,10))
EASYSTR_OPERATORS(unsigned char,ultoa(p,numbuf,10))
#endif


EASYSTR_OPERATORS(bool,(char*)(p? "1" : "0"))

EASYSTR_OPERATORS(char,CharToString(p))

EasyStr& EasyStr::EqualsString(const char *nt) {
  INT_PTR len=nt?strlen(nt):0; //v4, no crash if null
  bool DoResize=false;
  if(len>bufsize||len<bufsize-128||len==0) 
    DoResize=true;
  if(nt>=Text && nt<Text+bufsize) 
    DoResize=false; // Setting to itself
  if(DoResize)
  {
    DeleteBuf();
    bufsize=len;
    if(len)
      Text=new char[bufsize+1];
    else
      Text=Empty_Text;
  }
  if(Text!=Empty_Text 
    && nt) //402R6
    strcpy(Text,nt);
  return *this;
}


EasyStr EasyStr::PlusString(const char *nt) {
  return EasyStr(Text,(char*)nt);
}


EasyStr& EasyStr::PlusEqualsString(const char *new_text) {
  EasyStr nt=new_text;  // Just in case new_text is pointer in this string
  INT_PTR len=nt.Length();
  if((INT_PTR)strlen(Text)+len>bufsize) 
    ResizeBuf(len+strlen(Text));
  strcat(Text,nt);
  return *this;
}


bool EasyStr::SameAsString(const char *otext) {
  return (strcmp(Text,otext)==0);
}


bool EasyStr::NotSameAsString(const char *otext) {
  return (strcmp(Text,otext)!=0);
}

#endif//#if defined(SSE_420R5)
