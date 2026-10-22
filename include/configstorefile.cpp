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

DOMAIN: File
FILE: configstorefile.cpp
DESCRIPTION: A class to read and write ini properties file in standard Windows
format. This was necessary due to write-delay problems with
WritePrivateProfileString and other related functions.
SS: sometimes Steem authors were radical! 
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#ifndef LINUX
#include <io.h>
#endif
#include <conditions.h>
#include "easystr.h"
#include "mymisc.h"
#include "easystringlist.h"
#include "dynamicarray.h"
#include "configstorefile.h"
#include <acc.h>
#ifndef CSF_LOG
#define CSF_LOG(s)
#define CSF_LOG_DEFINED
#endif

#pragma warning (disable : 4996)

TConfigStoreFile::TConfigStoreFile(char *NewPath) {
  Changed=false;
  if(NewPath)
    Open(NewPath);
#if defined(SSE_TRACE_DUMP_OPTIONS)
  NoPath=false;
#endif
}


TConfigStoreFile::~TConfigStoreFile() {
  Close();
}


void TConfigStoreFile::SetInt(char* const Sect,char* const Key,int const val) {
  char Val[64]; // 64 bytes should be enough to represent an int...
#ifdef WIN32
  _snprintf(Val,64,"%d",val);
#endif
#ifdef UNIX
  snprintf(Val,64,"%d",val);
#endif
  SetStr(Sect,Key,Val);
}


bool TConfigStoreFile::Open(char *NewPath) {
  bool steem_ini=false;
  if(Path.NotEmpty())
  {
    CSF_LOG(EasyStr("INI: File already open, can't open ")+NewPath);
    return steem_ini;
  }
  CSF_LOG(EasyStr("INI: Opening ")+NewPath);
  Path=NewPath;
  FILE *fp=fopen(NewPath,"rb");
  if(fp==NULL)
  {
    CSF_LOG(EasyStr("INI: Couldn't open file, it probably doesn't exist"));
    return steem_ini;
  }
  // Load in all text
  CSF_LOG(EasyStr("INI: Loading contents..."));
  int Len=(int)GetFileLength(fp);
  FileBuf.SetLength(Len);
  ZeroMemory(FileBuf.Text,Len);
  fread(FileBuf.Text,Len,1,fp);
  fclose(fp);
  FileUprBuf.SetLength(Len);
  CSF_LOG(EasyStr("INI: Processing returns..."));
  // Find all returns and change to NULL
  int nSects=0,nKeys=0;
  char *tp=FileBuf.Text;
  for(;;)
  {
    tp=strchr(tp,'\n');
    if(tp==NULL)
      break;
    if(*(tp+1)=='[')
      nSects++;
    else
      nKeys++;
    *tp='\0';
    if((tp-1)>=FileBuf.Text) 
      if(*(tp-1)=='\r') 
        *(tp-1)='\0';
    tp++;
  }
  CSF_LOG(EasyStr("INI: Making space for ")+(nSects+2)+" sections and "+nKeys+" keys");
  Sects.Resize(nSects+2);
  Keys.Resize(nKeys);
  CSF_LOG(EasyStr("INI: Getting sections and keys"));
  char *tend=FileBuf.Text+Len,*pUpr=FileUprBuf.Text;
  tp=FileBuf.Text;
  nSects=-1;
  for(;;)
  {
    if(*tp=='[')
    {
      int line_len=(int)strlen(tp);
      char *end_name=strchr(tp,']');
      if(end_name) 
        *end_name='\0';
      TCsfSects s;
      s.szName=tp+1;
      strcpy(pUpr,tp+1);
      strupr(pUpr);
      s.szNameUpr=pUpr;
      pUpr+=strlen(tp)+1;
      Sects.Add(s);
      nSects++;
      tp+=line_len; // Ignore rest of line
    }
    else if(nSects>=0)
    {
      char *eq=strchr(tp,'=');
      if(eq)
      {
        *eq='\0';
        TCsfKeys k;
        k.szName=tp;
        if(!strcmp(tp,"Mem_Bank_1"))
          steem_ini=true;
        strcpy(pUpr,tp);
        strupr(pUpr);
        k.szNameUpr=pUpr;
        pUpr+=strlen(tp)+1;
        k.szValue=eq+1;
        k.iSect=nSects;
        Keys.Add(k);
        tp=eq+1;
      }
    }
    do
    {
      tp+=strlen(tp)+1;
      if(tp>=tend) break;
    } while(tp[0]=='\0');
    if(tp>=tend)
      break;
  }
  CSF_LOG(EasyStr("INI: All done loading"));
  return steem_ini;
}


bool TConfigStoreFile::Close() {
  CSF_LOG(EasyStr("INI: Closing"));
  bool SavedOkay=true;
  if(Path.NotEmpty())
  {
    if(Changed)
      SavedOkay=SaveTo(Path);
    CSF_LOG(EasyStr("INI: Deleting ")+szNewMem.NumItems+" new keys");
    for(int n=0;n<szNewMem.NumItems;n++) 
      delete[] szNewMem[n];
    CSF_LOG(EasyStr("INI: Deleting all other memory"));
    Sects.DeleteAll();
    Keys.DeleteAll();
    szNewMem.DeleteAll();
    FileBuf="";
    FileUprBuf="";
  }
  Path="";
  Changed=false;
  CSF_LOG(EasyStr("INI: Closing complete, returning ")+SavedOkay);
  return SavedOkay;
}


bool TConfigStoreFile::SaveTo(char *File) {
  CSF_LOG(EasyStr("INI: Saving settings to ")+File);
  FILE *fp=fopen(File,"wb");
  if(fp==NULL)
  {
    CSF_LOG(EasyStr("INI: Can't open, losing all changes!"));
    return false;
  }
  for(int i=0;i<Sects.NumItems;i++)
  {
    bool First=true;
    for(int k=0;k<Keys.NumItems;k++)
    {
      if(Keys[k].iSect==i)
      {
        if(First)
        {
          fprintf(fp,"[%s]\r\n",Sects[i].szName);
          First=false;
        }
        fprintf(fp,"%s=%s\r\n",Keys[k].szName,Keys[k].szValue);
      }
    }
    fprintf(fp,"\r\n");
  }
  fclose(fp);
  CSF_LOG(EasyStr("INI: Saved and closed file"));
  return true;
}


bool TConfigStoreFile::FindKey(EasyStr Sect,char *KeyVal,TCsfFind *pSK) {
  strupr(Sect);
  for(pSK->iSect=Sects.NumItems-1;pSK->iSect>=0;pSK->iSect--)
    if(strcmp(Sects[pSK->iSect].szNameUpr,Sect)==0) 
      break;
  if(pSK->iSect<0) 
    return false;
  EasyStr Key=KeyVal;
  strupr(Key);
  for(pSK->iKey=Keys.NumItems-1;pSK->iKey>=0;pSK->iKey--)
    if(Keys[pSK->iKey].iSect==pSK->iSect) 
      if(strcmp(Keys[pSK->iKey].szNameUpr,Key)==0) 
        break;
  return (pSK->iKey>=0);
}


bool TConfigStoreFile::GetBool(char *Sect,char *Key,bool DefVal) {
  CSF_LOG(EasyStr("INI: GetBool(")+Sect+","+Key+")");
  TCsfFind sk;
  if(FindKey(Sect,Key,&sk)) 
    return (atoi(Keys[sk.iKey].szValue)!=0);
  return DefVal;
}


BYTE TConfigStoreFile::GetByte(char *Sect,char *Key,BYTE DefVal) {
  CSF_LOG(EasyStr("INI: GetByte(")+Sect+","+Key+")");
#ifdef SSE_LEAN_AND_MEAN
  return (BYTE)GetInt(Sect,Key,DefVal);
#else
  TCsfFind sk;
  if(FindKey(Sect,Key,&sk)) 
    return (BYTE)atoi(Keys[sk.iKey].szValue);
  return DefVal;
#endif
}


int TConfigStoreFile::GetInt(char *Sect,char *Key,int DefVal) {
  CSF_LOG(EasyStr("INI: GetInt(")+Sect+","+Key+")");
  TCsfFind sk;
  if(FindKey(Sect,Key,&sk)) 
    return atoi(Keys[sk.iKey].szValue);
  return DefVal;
}


EasyStr TConfigStoreFile::GetStr(char *Sect,char *Key,char *DefVal) {
  CSF_LOG(EasyStr("INI: GetStr(")+Sect+","+Key+")");
  TCsfFind sk;
  if(FindKey(Sect,Key,&sk)) 
    return Keys[sk.iKey].szValue;
  return DefVal;
}


WORD TConfigStoreFile::GetWord(char *Sect,char *Key,WORD DefVal) {
  CSF_LOG(EasyStr("INI: GetWord(")+Sect+","+Key+")");
#ifdef SSE_LEAN_AND_MEAN
  return (WORD)GetInt(Sect,Key,DefVal);
#else
  TCsfFind sk;
  if(FindKey(Sect,Key,&sk)) 
    return (WORD)atoi(Keys[sk.iKey].szValue);
  return DefVal;
#endif
}


void TConfigStoreFile::SetStr(char *Sect,char *Key,char *Val) {
  CSF_LOG(EasyStr("INI: SetStr(")+Sect+","+Key+")");

#if defined(SSE_TRACE_DUMP_OPTIONS)
  // redact paths for privacy, keeping only the file names
  if(NoPath && Val)
  {
    int ValLen=(int)strlen(Val);
    bool bReplace=false;
    if(ValLen) for(int i=ValLen-1;i>=0;i--)
    {
      if(Val[i]=='=')
        break;
      else if(Val[i]=='\\')
        bReplace=true;
      else if(bReplace)
        Val[i]='*';
    }//nxt i
  }
#endif

  TCsfFind sk;
  if(FindKey(Sect,Key,&sk))
  {
    if(strcmp(Keys[sk.iKey].szValue,Val))
    { // If value is different
      char *NewStr=new char[strlen(Val)+1];
      szNewMem.Add(NewStr);
      Keys[sk.iKey].szValue=NewStr;
      strcpy(NewStr,Val);
      Changed=true;
    }
  }
  else
  {
    char *Buf,*pBuf;
    int SectLen=0,KeyLen=(int)strlen(Key)+1,ValLen=(int)strlen(Val)+1;
    if(sk.iSect<0) 
      SectLen=(int)strlen(Sect)+1;
    Buf=new char[SectLen*2+KeyLen*2+ValLen];pBuf=Buf;
    if(sk.iSect<0)
    {
      sk.iSect=Sects.NumItems;
      TCsfSects s;
      s.szName=pBuf;
      pBuf+=SectLen;
      s.szNameUpr=pBuf;
      pBuf+=SectLen;
      strcpy(s.szName,Sect);
      strcpy(s.szNameUpr,Sect);
      strupr(s.szNameUpr);
      CSF_LOG(EasyStr("INI: Adding new section ")+Sect);
      Sects.Add(s);
    }
    TCsfKeys k;
    k.szName=pBuf;
    pBuf+=KeyLen;
    k.szNameUpr=pBuf;
    pBuf+=KeyLen;
    strcpy(k.szName,Key);
    strcpy(k.szNameUpr,Key);
    strupr(k.szNameUpr);
    k.iSect=sk.iSect;
    k.szValue=pBuf;
    strcpy(k.szValue,Val);
    szNewMem.Add(Buf);
    CSF_LOG(EasyStr("INI: Adding new key ")+Key+"="+Val);
    Keys.Add(k);
    Changed=true;
  }
}


void TConfigStoreFile::GetSectionNameList(EasyStringList *pESL) {
  for(int i=0;i<Sects.NumItems;i++) 
    pESL->Add(Sects[i].szName);
}


bool TConfigStoreFile::GetWholeSect(EasyStringList *pESL,EasyStr Sect,bool Upr) {
  strupr(Sect);
  int iSect=Sects.NumItems-1;
  for(;iSect>=0;iSect--)
    if(strcmp(Sects[iSect].szNameUpr,Sect)==0)
      break;
  if(iSect<0) 
    return false;
  for(int i=0;i<Keys.NumItems;i++)
  {
    if(Keys[i].iSect==iSect)
      pESL->Add(Upr?Keys[i].szNameUpr:Keys[i].szName,atoi(Keys[i].szValue));
  }
  return true;
}


void TConfigStoreFile::DeleteSection(EasyStr Sect) {
  int s;
  strupr(Sect);
  for(s=Sects.NumItems-1;s>=0;s--)
    if(strcmp(Sects[s].szNameUpr,Sect)==0) 
      break;
  if(s<0) 
    return;
  for(int k=0;k<Keys.NumItems;k++)
    if(Keys[k].iSect==s) 
      Keys.Delete(k--);
  Changed=true;
}


// Global functions to replace WriteP... and GetP...
void WriteCSFStr(char *Sect,char *Key,char *Val,char *File) {
  TConfigStoreFile CSF(File);
  CSF.SetStr(Sect,Key,Val);
  CSF.Close();
}


EasyStr GetCSFStr(char *Sect,char *Key,char *DefVal,char *File) {
  TConfigStoreFile CSF(File);
  return CSF.GetStr(Sect,Key,DefVal);
}


int GetCSFInt(char *Sect,char *Key,int DefVal,char *File) {
  TConfigStoreFile CSF(File);
  return CSF.GetInt(Sect,Key,DefVal);
}

#ifdef CSF_LOG_DEFINED
#undef CSF_LOG
#undef CSF_LOG_DEFINED
#endif
