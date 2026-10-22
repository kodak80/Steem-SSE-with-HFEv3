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

DOMAIN: Disk image
FILE: di_get_contents.cpp
DESCRIPTION: This is the code for the disk image recognition system that uses
the TOSEC database. It is written as a library to enable it to be used in
other emulators at a later date.
The database is a text file, outdated now. TOSEC drifted from its original
form, it is better to directly edit the text file.
TODO
---------------------------------------------------------------------------*/


#include <pch.h>
#pragma hdrstop

#define SSE_REF_402

#include "di_get_contents.h"
#include <stdio.h>
#if defined(SSE_REF_402) // Steem specific... TODO
#include <archive.h>
#include <diskman.h>
#include <acc.h>
#endif
#include <notifyinit.h>
#include <translate.h>
#pragma warning (disable : 4996)


#if !defined(SSE_LIBRETRONUKE)

//char GetContents_ListFile[512]={0};
//char GetContents_ListFile[MAX_PATH]={0};
char *GetContents_ListFile=NULL;
GETZIPCRCSPROC *GetContents_GetZipCRCsProc=NULL;

#ifdef DEADC0DE
CONVERTTOSTPROC *GetContents_ConvertToSTProc=NULL;
#endif

#if !defined(SSE_REF_402) // use the other CRC32 utility now...

DWORD GetContents_Reflect(DWORD ref,char ch) {
  DWORD value=0;
  int i;
  // Swap bit 0 for bit 7 bit 1 for bit 6, etc.
  for(i=1;i<(ch+1);i++)
  {
    if(ref&1) value|=1<<(ch-i);
    ref>>=1;
  }
  return value;
}


void GetContents_InitCRC32Table(DWORD *lpTable) {
  // This is the official polynomial used by CRC-32
  // in PKZip, WinZip and Ethernet.
  DWORD Polynomial=0x04c11db7;
  int i,j;
  // 256 values representing ASCII character codes.
  for(i=0;i<=0xFF;i++)
  {
    lpTable[i]=GetContents_Reflect(i,8)<<24;
    for(j=0;j<8;j++) lpTable[i]=(lpTable[i]<<1)^((lpTable[i]&(1<<31))?
      Polynomial:0);
    lpTable[i]=GetContents_Reflect(lpTable[i],32);
  }
}


int GetContents_GetCRC(BYTE *Block,int BlockLen,DWORD *lpTable) {
  // Once the lookup table has been filled in by the two functions above,
  // this function creates all CRCs using only the lookup table.
  // Be sure to use unsigned variables,
  // because negative values introduce high bits
  // where zero bits are required.
  // Start out with all bits set high.
  DWORD CRC=0xffffffff;
  // Perform the algorithm on each byte in the block using the lookup table values.
  while(BlockLen--) 
    CRC=(CRC>>8)^lpTable[(CRC&0xFF)^*(Block++)];
  // Exclusive OR the result with the beginning value.
  return CRC^0xffffffff;
}

#endif//#if !defined(SSE_REF_402)


char *GetContents_GetNameAndContent(char *t,char *Text,LONG_PTR Len,char **pName,
                                    char **pContent) {
  char *line;
  // Find the start of the line
  while(t>Text) // Text is the DB
  {
    if(*t=='\r'||*t=='\n'||*t=='\0')
    {
      t++;
      break;
    }
    t--;
  }
  line=t;
  while(t<Text+Len)
  {
    if(*t=='\r'||*t=='\n'||*t=='\0')
    {
      *(t++)='\0';
      break;
    }
    t++;
  }
  *pContent=strchr(line+1,'\"')+2;
  if((*pContent)[0]=='\"')
  {
    (*pContent)++;

    *strchr((*pContent),'\"')='\0';
  }
  else
    (*pContent)[0]='\0';
  (*pName)=line+1;
#if 0 // unnecessary
  if(line[0]=='/' && line[1]=='/')
    (*pContent)[0]='\0';
#endif
  *strchr((*pName),'\"')='\0';
  return t;
}


// Format of string: "TOSEC Name","Content","CRC1","CRC2",...,"CRCn"
// GetContentsFromDiskImage - get the name and a contents list of a disk image
//      Fil - Full path of disk image, can be any format including an archive
//      szRetBuf - Buffer in which to return NULL-terminated list of contents
//      iRetBufLen - Number of bytes in the buffer
//      OnAmbiguity - What to do if there is more than one match
//      crc32 - function will give crc32 of disk image
int GetContentsFromDiskImage(char *Fil,char *szRetBuf,int iRetBufLen,
                               //DWORD &crc32) {
                               DWORD *dwCRCs) {
                               //int /*OnAmbiguity*/) {
  enum {GC_TOOSMALL=-1,GC_CANTFINDCONTENTS=-0};
  TNotify myNotify(T("Disk operation"));
  char szCRC[16];
  int nFound=0;
  //DWORD dwCRCs[30];
  //char ext[16];
  char *pRet=szRetBuf;
  char *pRetEnd=szRetBuf+iRetBufLen-1; // -1 for double NULL
  memset(szRetBuf,0,iRetBufLen);
  //crc32=0;
  //dwCRCs[0]=0;
  { // Get CRC
    char *dot;
    //memset(dwCRCs,0,sizeof(dwCRCs));
#if defined(SSE_REF_402)
    dot=strrchr(Fil,'.');
    int Type=ExtensionIsDisk(dot); // use Steem's function TODO
    if(Type==2) //DISK_COMPRESSED
    {
      zippy.bCRC=true;
      // Extract CRC from zip file (quicker than decompressing, but if archiveaccess
      // is in charge, must decompress as CRC32 info is missing!)
      if(GetContents_GetZipCRCsProc)
        GetContents_GetZipCRCsProc(Fil,dwCRCs,30);
      zippy.bCRC=false;
      if(dwCRCs[0]==0) 
        return GC_CANTFINDCONTENTS;
    }
    else
      dwCRCs[0]=GetCRCFromFile(Fil);
#else
    dot=strrchr(Fil,'.');
    if(dot)
    {
      strcpy(ext,dot+1);
      if(strcmpi(ext,"zip")==0)
      {
        // Extract CRC from zip file (quicker than decompressing)
        if(GetContents_GetZipCRCsProc)
          GetContents_GetZipCRCsProc(Fil,dwCRCs,30);
        if(dwCRCs[0]==0) 
          return GC_CANTFINDCONTENTS;
      }
      else
        dwCRCs[0]=GetCRCFromFile(Fil);
    }
#endif
    //crc32=dwCRCs[0];
  }
  {
    FILE *fp;
    fp=fopen(GetContents_ListFile,"rb");
    if(fp)
    {
      char *Text,*t,*name,*content;
      LONG_PTR Len;
      FSEEK(fp,0,SEEK_END);
      Len=(LONG_PTR)FTELL(fp);
      FSEEK(fp,0,SEEK_SET);
      Text=(char*)malloc(Len+1);
      FREAD(Text,sizeof(char),Len,fp); // copy full DB in memory
      FCLOSE(fp);
      Text[Len]='\0';
      for(int test=0;test<2;test++)
      { // Check list for match
        for(int i=0;i<30;i++)
        {
          if(dwCRCs[i]==0) 
            break;
          sprintf(szCRC,"%8.8X",(unsigned int)(dwCRCs[i]));
          t=Text;
          for(;;)
          {
            t=strstr(t,szCRC);
            if(t==NULL)
              break;
            t=GetContents_GetNameAndContent(t,Text,Len,&name,&content);
            if(nFound==0)
            {
              if(pRet+strlen(name)>=pRetEnd) 
              { 
                nFound=GC_TOOSMALL;
                break; 
              }
              strcpy(pRet,name);
              pRet+=strlen(pRet)+1;
              nFound++;
              if(content[0])
              {
                if(strcmpi(name,content)!=0)
                { // not the same
                  if(pRet+strlen(content)>=pRetEnd)
                  {
                    nFound=GC_TOOSMALL;
                    break; 
                  }
                  strcpy(pRet,content);
                  pRet+=strlen(pRet)+1;
                  nFound++;
                }
              }
            }
            else if(content[0])
            {
              if(strcmpi(name,szRetBuf)==0)
              { // same disk
                if(pRet+strlen(content)>=pRetEnd) 
                {
                  nFound=GC_TOOSMALL;
                  break; 
                }
                strcpy(pRet,content);
                pRet+=strlen(pRet)+1;
                nFound++;
              }
              else
              {
                // Ambiguity!
                if(pRet+strlen(content)>=pRetEnd) 
                { 
                  nFound=GC_TOOSMALL;
                  break; 
                }
                strcpy(pRet,content);
                pRet+=strlen(pRet)+1;
                nFound++;
              }
            }
          }
        }//nxt i
        if(nFound
#ifdef DEADC0DE
          ||GetContents_ConvertToSTProc==NULL
#endif
          ||test>0) 
          break;
        //if(strcmpi(ext,"st")==0) // ext undefined
          //break;
      }//nxt test
      free(Text);
    }//if(fp)
  }
  if(nFound==0) 
    return GC_CANTFINDCONTENTS;
  return nFound;
}


void GetContents_SearchDatabase(char *szFind,char *szRetBuf,int iRetBufLen) {
  char *pRet=szRetBuf;
  char *pRetEnd=szRetBuf+iRetBufLen-1; // -2 for treble NULL!
  memset(szRetBuf,0,iRetBufLen);
  FILE *fp;
  fp=fopen(GetContents_ListFile,"rb");
  if(fp)
  {
    char *name,*content,LastName[200]={'\0'};
    LONG_PTR Len;
    FSEEK(fp,0,SEEK_END);
    Len=(LONG_PTR)FTELL(fp);
    FSEEK(fp,0,SEEK_SET);
    char *Text=(char*)malloc(Len+1);
    fread(Text,1,Len,fp); // copy full DB in memory
    fclose(fp);
    Text[Len]=0;
    char *TextUpr=(char*)malloc(Len+1);
    strcpy(TextUpr,Text);
    strupr(TextUpr); // full DB in caps
    char *FindUpr=(char*)malloc(strlen(szFind)+1);
    strcpy(FindUpr,szFind);
    strupr(FindUpr);
    char *pTextUpr=TextUpr;
    for(;;)
    {
      pTextUpr=strstr(pTextUpr,FindUpr);
      if(pTextUpr==NULL)
        break;
      char *pNextLineUpr=
        (GetContents_GetNameAndContent(Text+(pTextUpr-TextUpr),Text,Len,&name,&content)-Text)+TextUpr;
      if(pTextUpr-TextUpr<=content+strlen(content)-Text)
      { // Not CRC!  
        if(strcmpi(name,LastName))
        { // Different name
          if(LastName[0]) 
            pRet++; // Terminate list for last name
          if(pRet+strlen(name)>=pRetEnd) 
            break;
          strcpy(pRet,name); pRet+=strlen(pRet)+1;
        }
        strcpy(LastName,name);
        if(content[0])
        {
          if(pRet+strlen(content)>=pRetEnd) 
            break;
          strcpy(pRet,content);
          pRet+=strlen(pRet)+1;
        }
      }
      pTextUpr=pNextLineUpr;
    }
    free(Text);
    free(TextUpr);
    free(FindUpr);
  }
}

#endif//#if !defined(SSE_LIBRETRONUKE)
