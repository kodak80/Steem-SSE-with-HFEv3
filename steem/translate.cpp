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
FILE: translate.cpp
DESCRIPTION: This handles translations.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <translate.h>
#include <dirsearch.h>
#include <gui.h>
#include <options.h>


EasyStr TranslateString,TranslateFileName;
char *TranslateBuf=NULL,*TranslateUpperBuf=NULL;
int TranslateBufLen=0;

EasyStr StripAndT(char *s) {
  EasyStr Ret=Translation(s);
  for(;;)
  {
#if defined(SSE_420R5)
    int i=Ret.InStr("&");
    if(i==(int)-1) 
#else
    size_t i=Ret.InStr("&");
    if(i==(size_t)-1) 
#endif   
      break;
    Ret.Delete(i,1);
  }
  return Ret;
}


EasyStr Translation(char *s) {
#ifdef TRANSLATION_TEST
  if(TranslateBuf==NULL) 
    return Str(s).UpperCase();
#else
  if(TranslateBuf==NULL) 
    return s;
#endif
  if(s[0]=='\0') 
    return s;
  EasyStr UpperS=s;
  strupr(UpperS);
  char *EStart=TranslateUpperBuf-1,*EEnd=NULL,*TStart,*TEnd;
  bool Match;
  do {
    EStart=strstr(EStart+1,UpperS);
    if(EStart==NULL) 
      break;
    Match=true;
    if(EStart>TranslateUpperBuf) 
    {
      if(*(EStart-1)!='\n') 
        Match=false;
    }
    EEnd=EStart+strlen(s);
    if(EEnd>=TranslateUpperBuf+TranslateBufLen)
    {
      EStart=NULL;
      break;
    }
    if(*EEnd=='\r')
      EEnd+=2;
    else if(*EEnd=='\n')
      EEnd++;
    else
      Match=false;
    if(EEnd>=TranslateUpperBuf+TranslateBufLen)
    {
      EStart=NULL;
      break;
    }
    if(*EEnd=='\r'||*EEnd=='\n') 
      Match=false;
  } while(!Match);
  if(EStart==NULL) 
  {
    FILE *fp=fopen(TranslateFileName,"ab");
    if(fp) 
    {
      fprintf(fp,"\r\n\r\n%s\r\n%s",s,s);
      fclose(fp);
      if(strlen(TranslateBuf)+6+strlen(s)*2>=size_t(TranslateBufLen-8)) 
      {
        TranslateBufLen=(int)strlen(TranslateBuf)+6+(int)strlen(s)*2+2000;
        char *Temp=new char[TranslateBufLen+8];
        strcpy(Temp,TranslateBuf);
        delete[] TranslateBuf;
        TranslateBuf=Temp;
      }
      strcat(TranslateBuf,EasyStr("\r\n\r\n")+s+"\r\n"+s);
      delete[] TranslateUpperBuf;
      TranslateUpperBuf=new char[(int)strlen(TranslateBuf)+1];
      strcpy(TranslateUpperBuf,TranslateBuf);
      strupr(TranslateUpperBuf);
    }
#ifdef TRANSLATION_TEST
    return EasyStr(s).UpperCase();
#else
    return s;
#endif
  }
  EEnd-=DWORD_PTR(TranslateUpperBuf);
  EEnd+=DWORD_PTR(TranslateBuf);
  TStart=EEnd;
  EasyStr Ret;
  TEnd=strchr(TStart,'\n');
  if(TEnd) 
  {
    if(*(TEnd-1)=='\r')
      TEnd--;
    char OldEndChar=*TEnd;
    *TEnd=0;
    Ret=TStart;
    *TEnd=OldEndChar;
  }
  else
    Ret=TStart;
#ifdef TRANSLATION_TEST
  return Ret.UpperCase();
#else
  return Ret;
#endif
}

void InitTranslations() {
  if(!Exists(TranslateFileName))
  {
    TranslateFileName="";
    DirSearch ds;
    if(ds.Find(UsersPath+SLASH "Translate_*.txt")) 
      TranslateFileName=UsersPath+SLASH+ds.Name;
  }
  if(TranslateFileName.Empty()) 
    return;
  FILE *fp=fopen(TranslateFileName,"rb");
  if(fp==NULL) 
    return;
  int FileLen=(int)GetFileLength(fp);
  TranslateBufLen=FileLen+2000;
  TranslateBuf=new char[TranslateBufLen+8];
  FREAD(TranslateBuf,FileLen,1,fp);
  TranslateBuf[FileLen]='\0';
  fclose(fp);
  TranslateUpperBuf=new char[strlen(TranslateBuf)+1];
  strcpy(TranslateUpperBuf,TranslateBuf);
  strupr(TranslateUpperBuf);
  SSEConfig.translated=TRUE;
}

#undef LOGSECTION
