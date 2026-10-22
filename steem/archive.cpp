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
FILE: archive.cpp
DESCRIPTION: This file contains the code for zipclass, Steem's abstraction
of the various unarchiving libraries it can use.
Steem SSE can read ZIP, STZ, RAR, 7Z archives.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <archive.h>
#include <debug.h>
#include <iolist.h>
#include <computer.h>
#include <diskman.h>
#include <gui.h>


EasyStr last_error;

zipclass zippy;
#ifdef UNIX
bool enable_zip=true;
#else
bool enable_zip=false;
#endif

#define LOGSECTION LOGSECTION_IMAGE_INFO

#ifdef WIN32

#if !defined(SSE_NO_UNZIPD32) // x64: no unzipd64.dll available

// function pointers
int (PASCAL *GetFirstInZip)(char*,TPackRec*); //find out what files are in the ZIP file (first file)
int (PASCAL *GetNextInZip)(TPackRec*);        //get next file in ZIP
void (PASCAL *CloseZipFile)(TPackRec*);       //free buffers and close ZIP after GetFirstInZip()
BYTE (PASCAL *isZip)(char*);                    //determine if a file is a ZIP file
int (PASCAL *UnzipFile)(char*,char*,WORD,long,void*,long);        //unzipping

HINSTANCE hUnzip=NULL;

void LoadUnzipDLL() {
  hUnzip=SteemLoadLibrary(UNZIP_DLL); // unzipd32.dll
  if(hUnzip)
  {
    GetFirstInZip=(int(PASCAL*)(char*,TPackRec*))GetProcAddress(hUnzip,"GetFirstInZip");
    GetNextInZip=(int(PASCAL*)(TPackRec*))GetProcAddress(hUnzip,"GetNextInZip");
    CloseZipFile=(void(PASCAL*)(TPackRec*))GetProcAddress(hUnzip,"CloseZipFile");
    isZip=(BYTE(PASCAL*)(char*))GetProcAddress(hUnzip,"isZip");
    UnzipFile=(int(PASCAL*)(char*,char*,WORD,long,void*,long))GetProcAddress(hUnzip,"unzipfile");
    if(!(GetFirstInZip && GetNextInZip && CloseZipFile && isZip && UnzipFile))
      SteemFreeLibrary(hUnzip);
    else
      enable_zip=SSEConfig.unzipd32Dll=true;
  }
}

#endif//#if !defined(SSE_NO_UNZIPD32)

#if defined(SSE_DISK_RAR_SUPPORT)

HINSTANCE hUnrar=NULL;
HANDLE (PASCAL *rarOpenArchive)(struct RAROpenArchiveData *ArchiveData)=NULL;
int (PASCAL *rarCloseArchive)(HANDLE hArcData)=NULL;
int (PASCAL *rarReadHeader)(HANDLE hArcData,struct RARHeaderData *HeaderData)=NULL;
int (PASCAL *rarProcessFile)(HANDLE hArcData,int Operation,char *DestPath,char *DestName)=NULL;

void LoadUnrarDLL() {
  UNRAR_OK=false;
  hUnrar=SteemLoadLibrary(UNRAR_DLL);
  if(hUnrar)
  {
    // get dll functions
    rarOpenArchive=(HANDLE(PASCAL*)(struct RAROpenArchiveData* ArchiveData))
      GetProcAddress(hUnrar,"RAROpenArchive");
    rarCloseArchive=(int(PASCAL*)(HANDLE hArcData))
      GetProcAddress(hUnrar,"RARCloseArchive");
    rarReadHeader=(int(PASCAL*)(HANDLE hArcData,struct RARHeaderData *HeaderData))
      GetProcAddress(hUnrar,"RARReadHeader");
    rarProcessFile=(int(PASCAL*)(HANDLE hArcData,int Operation,char *DestPath,char *DestName))
      GetProcAddress(hUnrar,"RARProcessFile");
    if(rarOpenArchive&&rarCloseArchive&&rarReadHeader&&rarProcessFile)
    {
      // prefill structures for all archives
      ZeroMemory(&zippy.ArchiveData,sizeof(zippy.ArchiveData));
      ZeroMemory(&zippy.HeaderData,sizeof(zippy.HeaderData));
      zippy.ArchiveData.OpenMode=RAR_OM_EXTRACT;
      enable_zip=true;
      UNRAR_OK=true;
    }
    else
      SteemFreeLibrary(hUnrar);
  }
}

#endif//#if defined(SSE_DISK_RAR_SUPPORT)

#endif//WIN32


zipclass::zipclass() {
  ZeroMemory(this,sizeof(zipclass));
}


#if defined(SSE_ARCHIVEACCESS_SUPPORT)

bool zipclass::for_archiveaccess() { // little helper function, v401
  return (ARCHIVEACCESS_OK &&
      (MatchesAnyString_I(type,"7Z","BZ2","GZ","TAR","ARJ",NULL)
      || !SSEConfig.unzipd32Dll && (strcmp(type,"ZIP")==0||strcmp(type,"STZ")==0)
      || !SSEConfig.UnrarDll && strcmp(type,"RAR")==0 ));
}

#endif


#if defined(SSE_DISK_7Z_SUPPORT_UNIX)

bool zipclass::for_7za() {
  return (MatchesAnyString_I(type,"7Z","BZ2","GZ","TAR","ARJ",NULL));
}

#endif


#if defined(SSE_421R0)

/*
*  first(path) finds the type (extension in caps) and handler (plugin), opens 
*  the archive and finds the first file in the archive
*  it is only called internally and by SetDisk()
*  next() iterates files in the archive
*  filename_in_zip() is called internally and by SetDisk after first() and next()
*  first(), next() and extract_file() return ZIPPY_SUCCEED or ZIPPY_FAIL
*  we use try/catch to streamline code
*/

bool zipclass::first(char *path) {
  bool bResult=ZIPPY_FAIL;
  if(!enable_zip) 
    return bResult;
  if(is_open) 
    close();
  type[0]='\0';
  char *dot=strrchr(path,'.');
  if(dot) 
  {
    if(strlen(dot+1)<11) 
      strcpy(type,dot+1);
  }
  if(type[0]=='\0')
    strcpy(type,"ZIP");
  strupr(type);
  try {

#if defined(SSE_DISK_RAR_SUPPORT)
    if(UNRAR_OK && !strcmp(type,"RAR"))
    {
#ifdef WIN32
      ArchiveData.ArcName=path;
      hArcData=rarOpenArchive(&ArchiveData);
      ASSERTB(hArcData); // can assert if no ST files in archive
      ASSERTB(!ArchiveData.OpenResult);
      if(!hArcData || ArchiveData.OpenResult)
        throw ZIPPY_FAIL;
      int ec=rarReadHeader(hArcData,&HeaderData);
      handler=UNRAR;
#endif
#ifdef UNIX
      // first() provokes extraction of archive
      char command_string[1024]; // unsafe
      sprintf(command_string,"unrar e -o+ \"%s\" %s/unzip/",path,TempPath.Text);
      int ec=system(command_string);
      handler=CMDLINE_RAR;
#endif//UNIX
      if(!ec)
      {
        is_open=true;
        current_file_n=current_file_offset=0;
#ifdef WIN32
        m_attrib=(WORD)HeaderData.FileAttr;
        crc=HeaderData.FileCRC;
#endif
        bResult=ZIPPY_SUCCEED;
      }
      else
        throw ZIPPY_FAIL;
    } else
#endif//#if defined(SSE_DISK_RAR_SUPPORT)

#if defined(SSE_DISK_7Z_SUPPORT_UNIX)
    if(for_7za())
    {
      // first() provokes extraction of archive
      char command_string[1024]; // unsafe
      // could be 7z for extra?
      sprintf(command_string,"7za x -o%s/unzip/ \"%s\" ",TempPath.Text,path);
      int ec=system(command_string);
      if(!ec)
      {
        is_open=true;
        current_file_n=current_file_offset=0;
        crc=0; //?
        bResult=ZIPPY_SUCCEED;
        handler=CMDLINE_7Z;
      }
      else
        throw ZIPPY_FAIL;
    } else
#endif

#if defined(SSE_ARCHIVEACCESS_SUPPORT)
    if(for_archiveaccess()) // ArchiveAccess handles zip, stz and rar if other plugins missing
    {
      if(ArchiveAccess_Open(path))
      {
        //TRACE_LOG("sizeof(FileInfo)=%d\n",sizeof(FileInfo)); // surprise! was 2100 in bcc (2104 in vc)
        is_open=true;
        current_file_n=current_file_offset=0;
        m_attrib=(WORD)FileInfo.attributes;
        crc=0; //?
        bResult=ZIPPY_SUCCEED;
        handler=ARCHIVEACCESS;
      }
      else
        throw ZIPPY_FAIL;
    } else
#endif

    if(strcmp(type,"ZIP")==0||strcmp(type,"STZ")==0)
    {
#ifdef WIN32
#if !defined(SSE_NO_UNZIPD32)
      ZeroMemory(&PackInfo,sizeof(PackInfo));
      if(GetFirstInZip(path,&PackInfo)!=UNZIP_Ok)
        throw ZIPPY_FAIL;
      is_open=true;
      current_file_n=0;
      current_file_offset=PackInfo.HeaderOffset;
      m_attrib=PackInfo.Attr;
      crc=PackInfo.Crc;
#endif
#endif//WIN32
#ifdef UNIX
      uf=unzOpen(path);
      if (uf==NULL){
        last_error="Couldn't open ";
        last_error+=path;
        throw ZIPPY_FAIL;
      }else{
        is_open=true;
        err=unzGetGlobalInfo(uf,&gi);
        current_file_n=0;
        current_file_offset=0;
        err=unzGetCurrentFileInfo(uf,&fi,filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);
        if (err){
          close();
          throw ZIPPY_FAIL;
        }
        crc=fi.crc;
      }
#endif//UNIX
      bResult=ZIPPY_SUCCEED;
      handler=UNZIP32D;
    }

#ifdef RARLIB_SUPPORT
    else if (strcmp(type,"RAR")==0){
      if (urarlib_list(name,(ArchiveList_struct*)&rar_list)<=0) throw ZIPPY_FAIL;
      // There are some files in this RAR
      while (rar_list){
        if ((rar_list->item.FileAttr & 0x10)==0) break; // Not directory
        rar_list=rar_list->next;
      }
      if (rar_list==NULL) throw ZIPPY_FAIL;
      is_open=true;
      rar_current=rar_list;
      current_file_n=0;
      current_file_offset=0;
      m_attrib=WORD(rar_current->item.FileAttr);
      crc=rar_current->item.FileCRC;
      bResult=ZIPPY_SUCCEED;
      handler=UNRAR;
    } 
#endif
  }
  catch(bool b) {
    bResult=b;
    handler=NONE;
  }
  TRACE_LOG2("%s first %s handler %d %s %d\n","Zippy",path,handler,"ERROR",bResult);
  return bResult;
}


bool zipclass::next() {
  bool bResult=ZIPPY_FAIL;
  try {
    if(!enable_zip)
      throw ZIPPY_FAIL;
    
#if defined(SSE_DISK_RAR_SUPPORT_UNIX) || defined(SSE_DISK_7Z_SUPPORT_UNIX)
    if(0
#if defined(SSE_DISK_RAR_SUPPORT_UNIX)  
       ||handler==CMDLINE_RAR
#endif
#if defined(SSE_DISK_7Z_SUPPORT_UNIX)
       ||handler==CMDLINE_7Z
#endif
       )
    {
      bool ok=false;
      DirSearch ds;
      EasyStr Fol=TempPath+SLASH+"unzip"+SLASH;
      if(ds.Find(Fol+"*.*"))
      {
        while(ds.Name=="." || ds.Name=="..")
          ds.Next();
        for(int i=0;i<=current_file_n;i++)
          ok=ds.Next();
      }
      ds.Close();
      if(ok)
      {
        current_file_n++;
        current_file_offset=current_file_n;
        bResult=ZIPPY_SUCCEED;
      }
      throw ZIPPY_FAIL;
    } else
#endif

#if defined(SSE_ARCHIVEACCESS_SUPPORT)
    if(handler==ARCHIVEACCESS) 
    {
      if(!is_open||!ArchiveHandle)
        throw ZIPPY_FAIL;
      ASSERTB(current_archived_file==current_file_n); // not if folders TODO?
      if(ArchiveAccess_Select(current_file_n+1))
      {
        current_file_n++;
        current_archived_file++;      
        m_attrib=(WORD)FileInfo.attributes;
        current_file_offset=current_file_n;
        bResult=ZIPPY_SUCCEED;
      }
    } else
#endif

#if defined(SSE_DISK_RAR_SUPPORT_WIN)
    if(handler==UNRAR)
    {
      if(!is_open||!UNRAR_OK||!hArcData)
        throw ZIPPY_FAIL;
      int ec;
      // we must 'process' the current one or reading will fail
      ec=rarProcessFile(hArcData,RAR_TEST,NULL,NULL);
      if(!ec)
        ec=rarReadHeader(hArcData,&HeaderData);
      current_file_n++;
      if(!ec)
      {
        current_file_offset=current_file_n;
        m_attrib=(WORD)HeaderData.FileAttr;
        crc=HeaderData.FileCRC;
        bResult=ZIPPY_SUCCEED;
      }
      else
        throw ZIPPY_FAIL;
    } else
#endif

    if(handler==UNZIP32D)
    {
#ifdef UNIX
      if(is_open==0) throw ZIPPY_FAIL;
      if(current_file_n>=(int)(gi.number_entry-1)) return ZIPPY_FAIL;
      err=unzGoToNextFile(uf);
      if(err) throw ZIPPY_FAIL;
      err=unzGetCurrentFileInfo(uf,&fi,filename_inzip,
                                sizeof(filename_inzip),NULL,0,NULL,0);
      if(err) throw ZIPPY_FAIL;
      current_file_n++;
      current_file_offset=current_file_n;
      crc=fi.crc;
#endif
#ifdef WIN32
#if !defined(SSE_NO_UNZIPD32)
      err=GetNextInZip(&PackInfo);
      if(err!=UNZIP_Ok)
        return ZIPPY_FAIL;
      m_attrib=PackInfo.Attr;
      crc=PackInfo.Crc;
      current_file_n++;
      current_file_offset=PackInfo.HeaderOffset;
#endif
#endif
      bResult=ZIPPY_SUCCEED;
    }

#ifdef RARLIB_SUPPORT
    else if (handler==UNRAR){
      if (is_open==0 || rar_current==NULL) throw ZIPPY_FAIL;
      do{
        rar_current=rar_current->next;
        current_file_n++;
        if (rar_current==NULL) throw ZIPPY_FAIL;
      } while (rar_current->item.FileAttr & 0x10); // Skip if directory
      current_file_offset=current_file_n;
      m_attrib=WORD(rar_current->item.FileAttr);
      crc=rar_current->item.FileCRC;
      bResult=ZIPPY_SUCCEED;
    }
#endif

  }
  catch(bool b) {
    bResult=b; // note failing in next() means end of list
  }
  TRACE_LOG2("%s next %d offset %d %s %d\n","Zippy",current_file_n,current_file_offset,"ERROR",bResult);
  return bResult;
}


EasyStr zipclass::filename_in_zip() {
  EasyStr ansi_string;
  if(!enable_zip) 
    return ansi_string;
  ansi_string.SetLength(SSE_MAX_PATH); // necessary before we strcpy stuff

#if defined(SSE_DISK_RAR_SUPPORT_UNIX) || defined(SSE_DISK_7Z_SUPPORT_UNIX)
  if(0
#if defined(SSE_DISK_RAR_SUPPORT_UNIX)  
    ||handler==CMDLINE_RAR
#endif
#if defined(SSE_DISK_7Z_SUPPORT_UNIX)
    ||handler==CMDLINE_7Z
#endif
    )    
  {
    DirSearch ds; 
    EasyStr Fol=TempPath+SLASH+"unzip"+SLASH;
    if(ds.Find(Fol+"*.*"))
    {
      while(ds.Name=="." || ds.Name=="..")
        ds.Next();
      for(int i=0;i<current_file_n;i++)
        ds.Next();
      strcpy(ansi_string.Text,ds.Name.Text);
    }
    ds.Close();
  }
  else
#endif      
 
#if defined(SSE_ARCHIVEACCESS_SUPPORT)
  if(handler==ARCHIVEACCESS)
  {
    if(ArchiveHandle) // need to convert name from unicode
    {
      WideCharToMultiByte ( CP_ACP,                // ANSI code page
        WC_COMPOSITECHECK,     // Check for accented characters
        (LPCWSTR)FileInfo.path,         // Source Unicode string
        -1,                    // -1 means string is zero-terminated
        ansi_string.Text,
        SSE_MAX_PATH,  // Size of buffer
        NULL,                  // No default character
        NULL );                // Don't care about this flag
      //TRACE_LOG("%S -> %s\n",FileInfo.path,CHECKPATH(ansi_string));
    }
  } else
#endif

#if defined(SSE_DISK_RAR_SUPPORT_WIN)
  if(handler==UNRAR)
  {
    if(hArcData) 
      ansi_string=HeaderData.FileName;
  } else
#endif

  if(handler==UNZIP32D) 
  {
#ifdef UNIX
    return filename_inzip;
#endif
#ifdef WIN32
#if !defined(SSE_NO_UNZIPD32)
    ansi_string=PackInfo.FileName;
#endif
#endif
  }

#ifdef RARLIB_SUPPORT
  else if(handler==UNRAR) 
  {
    if (rar_current) ansi_string=rar_current->item.Name;
  } else
#endif
  TRACE_LOG2("Zippy file name %s\n",ansi_string.Text);
  return ansi_string;
}


bool zipclass::extract_file(char *path,int offset,char *dest_dir,DWORD attrib,BOOL DontClose) {
  bool bResult=ZIPPY_FAIL;

  try {

    if(!enable_zip)
      throw ZIPPY_FAIL;

#ifdef UNIX // RAR + 7Z Unix
    if(0
#if defined(SSE_DISK_RAR_SUPPORT_UNIX)  
       ||handler==CMDLINE_RAR
#endif
#if defined(SSE_DISK_7Z_SUPPORT_UNIX)
       ||handler==CMDLINE_7Z
#endif
       )
    {
      bool ok=false;
      if(is_open)
        close();

      if(first(path)==ZIPPY_SUCCEED)
      {
        DirSearch ds;
        EasyStr Fol=TempPath+SLASH+"unzip"+SLASH;
        if(ds.Find(Fol+"*.*"))
        {
          ok=true;
          while(ds.Name=="." || ds.Name=="..")
            ds.Next();
          for(int i=0;i<offset;i++)
            ok=ds.Next();
          if(ok)
          {
            char command_string[1024]; // unsafe
            sprintf(command_string,"cp \"%s/unzip/%s\" \"%s\"",TempPath.Text,ds.Name.Text,dest_dir);
            int ec=system(command_string);
          }
        }
      }
      if(!DontClose)
        close();
      bResult= (ok) ? ZIPPY_SUCCEED : ZIPPY_FAIL;
    }
#endif//UNIX

#ifdef WIN32
#if defined(SSE_ARCHIVEACCESS_SUPPORT) // 7Z + RAR + ZIP Win32
    if(handler==ARCHIVEACCESS)
    {
      if(is_open)
        close();
      if(first(path)==ZIPPY_SUCCEED)
      {
        while(offset>0) {
          if(next()==ZIPPY_FAIL)
          {
            close();
            throw ZIPPY_FAIL;
          }
          offset--;
        }
      }
      ArchiveAccess_Extract(dest_dir);
      if(!DontClose)
        close();
      if(attrib)
        SetFileAttributes(dest_dir,attrib);
      bResult=ZIPPY_SUCCEED;
    } else
#endif//#if defined(SSE_ARCHIVEACCESS_SUPPORT)
#if defined(SSE_DISK_RAR_SUPPORT_WIN)
    if(handler==UNRAR)
    {
      if(is_open)
        close();
      if(first(path)==ZIPPY_SUCCEED)
      {
        while(offset>0) {
          if(next()==ZIPPY_FAIL)
          {
            close();
            throw ZIPPY_FAIL;
          }
          offset--;
        }
      }
      int ec=rarProcessFile(hArcData,RAR_EXTRACT,NULL,dest_dir);
      if(ec)
        throw ZIPPY_FAIL;
      if(!DontClose)
        close();
      if(attrib)
        SetFileAttributes(dest_dir,attrib);
      bResult=ZIPPY_SUCCEED;
    }
#endif//#if defined(SSE_DISK_RAR_SUPPORT_WIN)
#endif//WIN32

    // unzipd32 ZIP Win32 + Unix
    else if(handler==UNZIP32D)
    {
      if(is_open)
        close();
#ifdef UNIX
      uf=unzOpen(path);
      if(uf==NULL) {
        last_error=Str("Couldn't open ")+path;
        throw ZIPPY_FAIL;
      }
      is_open=true;
      err=unzGetGlobalInfo(uf,&gi);
      if(err) {
        close();
        throw ZIPPY_FAIL;
      }
      unz_global_info gi;
      int err=unzGetGlobalInfo(uf,&gi);
      if(err!=UNZ_OK) {
        close();
        last_error="couldn't get global info";
        throw ZIPPY_FAIL;
      }
      if(offset>=(int)gi.number_entry) {
        close();
        last_error="too few files in zip";
        throw ZIPPY_FAIL;
      }
      //  unzGoToFirstFile(uf);
      for(int i=0;i<offset;i++) unzGoToNextFile(uf);
#define UNZIP_BUF_SIZE 8192
      BYTE buf[UNZIP_BUF_SIZE];
      err=unzGetCurrentFileInfo(uf,&fi,filename_inzip,
                                sizeof(filename_inzip),NULL,0,NULL,0);
      if(err) throw ZIPPY_FAIL;
      EasyStr dest_file=dest_dir;
      if(dest_dir[0]==0 || dest_dir[strlen(dest_dir)-1]=='/') {
        char* t=strrchr(filename_inzip,'/');
        if(t) {
          t++;
        }
        else {
          t=strrchr(filename_inzip,'\\');
          if(t) {
            t++;
          }
          else {
            t=filename_inzip;
          }
        }
        dest_file+=t;
      }
      err=unzOpenCurrentFile(uf);
      if(err) {
        close();
        last_error="error opening file in ZIP";
        printf("%s\n",last_error.Text);
        throw ZIPPY_FAIL;
      }
      FILE* fout=fopen(dest_file,"wb");
      if(fout==NULL) {
        close();unzCloseCurrentFile(uf);
        last_error="error opening file ";last_error+=dest_file.Text;
        printf("%s\n",last_error.Text);
        throw ZIPPY_FAIL;
      }
      do {
        err=unzReadCurrentFile(uf,buf,UNZIP_BUF_SIZE);
        if(err<0) {
          last_error=EasyStr("error #")+err+" with zipfile in unzReadCurrentFile";
          printf("%s\n",last_error.Text);
          break;
        }
        else if(err>0) {
          fwrite(buf,err,1,fout);
        }
      } while(err>0);
      fclose(fout);
      err=unzCloseCurrentFile(uf);
      close();
      if(err) throw ZIPPY_FAIL;
#endif//UNIX

#ifdef WIN32
#if !defined(SSE_NO_UNZIPD32)
      EasyStr dest_file=dest_dir;
      err=UnzipFile(path,dest_file.Text,(WORD)attrib,offset,NULL,0);
      if(err!=UNZIP_Ok)
        throw ZIPPY_FAIL;
#endif
#endif//WIN32
      bResult=ZIPPY_SUCCEED;
    }

#ifdef RARLIB_SUPPORT
    else if (handler==UNRAR){
      if(is_open) close();
      if(first(path)==ZIPPY_SUCCEED) {
        while(offset > 0) {
          if(next()) { // Failed
            close();
            throw ZIPPY_FAIL;
          }
          offset--;
        }
      }
      char* data_ptr;
      DWORD data_size;
      if(urarlib_get(&data_ptr,&data_size,rar_current->item.Name,path,"")==0) throw ZIPPY_FAIL;
      close();
      EasyStr dest_file=dest_dir;
      FILE* fp=fopen(dest_file,"wb");
      if(fp==NULL) throw ZIPPY_FAIL;
      fwrite(data_ptr,1,data_size,fp);
      fclose(fp);
#ifdef WIN32
      if(hide) {
        SetFileAttributes(dest_file,FILE_ATTRIBUTE_HIDDEN);
      }
      else {
        SetFileAttributes(dest_file,m_attrib);
      }
#endif
      bResult=ZIPPY_SUCCEED;
    }
#endif//#ifdef RARLIB_SUPPORT

  }//try
  catch(bool b) {
    bResult=b;
  }
  TRACE_LOG("%s extract %s to %s %s %d\n","Zippy",path,CHECKPATH(dest_dir),"ERROR",bResult);
  return bResult;
}


bool zipclass::close() {
  bool bResult=ZIPPY_FAIL;
  try {
    if(!enable_zip) 
      throw ZIPPY_FAIL;
    if(is_open)
    {
#if defined(SSE_DISK_RAR_SUPPORT_UNIX) || defined(SSE_DISK_7Z_SUPPORT_UNIX)
      if(0
#if defined(SSE_DISK_RAR_SUPPORT_UNIX)  
         ||handler==CMDLINE_RAR
#endif
#if defined(SSE_DISK_7Z_SUPPORT_UNIX)
         ||handler==CMDLINE_7Z
#endif
         )
      {
        char command_string[1024]; // unsafe
        sprintf(command_string,"rm -r %s/unzip/*.*",TempPath.Text);
        int ec=system(command_string);
        is_open=false;
        bResult=ZIPPY_SUCCEED;
      } else
#endif    
#if defined(SSE_ARCHIVEACCESS_SUPPORT)
      if(handler==ARCHIVEACCESS)
      {
        ArchiveAccess_Close();
        is_open=false;
        bResult=ZIPPY_SUCCEED;
      } else
#endif
#if defined(SSE_DISK_RAR_SUPPORT_WIN)
      if(handler==UNRAR)
      {
        rarCloseArchive(hArcData);
        is_open=false;
        bResult=ZIPPY_SUCCEED;
      } else
#endif
      if(handler==UNZIP32D)
      {
#ifdef UNIX
        unzClose(uf);
#endif
#ifdef WIN32
#if !defined(SSE_NO_UNZIPD32)
        CloseZipFile(&PackInfo);
#endif
#endif
        is_open=false;
        bResult=ZIPPY_SUCCEED;
      }
#ifdef RARLIB_SUPPORT
      else if(handler==UNRAR)
      {
        is_open=false;
        bResult=ZIPPY_SUCCEED;
      } else
#endif
    }
  }
  catch(bool b) {
    bResult=b;
  }
  TRACE_LOG2("%s close %s %d\n","Zippy","ERROR",bResult);
  return bResult;
}


void zipclass::list_contents(char *path,EasyStringList *eslp,bool st_disks_only) {
// used for Insert Into context menu, ExtractArchiveToSTHardDrive,ExtractDisks,
// GCGetCRC, GCConvertToST, ShowPropDiag, EndDrag, UpdateBPBFiles
  if(!enable_zip) 
    return;
  eslp->DeleteAll();
  if(first(path)==ZIPPY_SUCCEED) 
  {
    do {
      EasyStr filename=filename_in_zip();
      bool addflag=true;
      if(st_disks_only) 
      {
        int st_disk=FileIsDisk(filename);
        if(st_disk==DISK_UNCOMPRESSED
#if defined(SSE_TOS_PRG_AUTORUN)
          // no interference with context menu, MSA Converter
          && !MatchesAnyString_I(type,DISK_EXT_PRG,DISK_EXT_TOS,NULL)
#endif
          || st_disk==DISK_PASTI)
        {}
        else
          addflag=false;
      }
      if(addflag) 
      {
        if(bCRC) // in case we're using archiveaccess and we want CRC32 (GetContentsFromDiskImage)
        {
          EasyStr sText;
          sText.SetLength(SSE_MAX_PATH);
          char *tempfile=sText.Text;
          GetTempFileName(TempPath,"ZIP",0,tempfile);
          extract_file(path,current_file_n,tempfile,0,TRUE); // Don't close!
          DeleteFile(tempfile);
        }
        eslp->Add(filename,current_file_offset,m_attrib,crc);
        TRACE_LOG2("%s list content %s CRC%X\n","Zippy",filename.Text,crc);
      }
    } while (next()==ZIPPY_SUCCEED);
  }
  close();
}

#else//#if defined(SSE_421R0)

bool zipclass::first(char *name) {
  if(!enable_zip) 
    return ZIPPY_FAIL;
  if(is_open) 
    close();
  type[0]='\0';
  char *dot=strrchr(name,'.');
  if(dot) 
  {
    if(strlen(dot+1)<11) 
      strcpy(type,dot+1);
  }
  if(type[0]=='\0')
    strcpy(type,"ZIP");
  strupr(type);
#if defined(SSE_DISK_RAR_SUPPORT_UNIX)
  if(UNRAR_OK && !strcmp(type,"RAR")) 
  {
    // first() provokes extraction of archive
    char command_string[1024]; // unsafe
    sprintf(command_string,"unrar e -o+ \"%s\" %s/unzip/",name,TempPath.Text);
    int ec=system(command_string);
    if(!ec)
    {
      is_open=true;
      current_file_n=0;
      current_file_offset=0;
      crc=0; //?
      return ZIPPY_SUCCEED;
    }
    else
      return ZIPPY_FAIL;//tmp
  }
  else
#endif
#if defined(SSE_DISK_7Z_SUPPORT_UNIX)
  if(for_7za())
  {
    // first() provokes extraction of archive
    char command_string[1024]; // unsafe
    // could be 7z for extra?
    sprintf(command_string,"7za x -o%s/unzip/ \"%s\" ",TempPath.Text,name);
    int ec=system(command_string);
    if(!ec)
    {
      is_open=true;
      current_file_n=0;
      current_file_offset=0;
      crc=0; //?
      return ZIPPY_SUCCEED;
    }
    else
      return ZIPPY_FAIL;//tmp
  } 
  else
#endif
#if defined(SSE_ARCHIVEACCESS_SUPPORT)
  // ArchiveAccess handles zip and rar if other plugins missing
  if(for_archiveaccess())
  {
    if(ArchiveAccess_Open(name))
    {
      //TRACE_LOG("sizeof(FileInfo)=%d\n",sizeof(FileInfo)); // surprise! was 2100 in bcc (2104 in vc)
      is_open=true;
      current_file_n=0;
      current_file_offset=0;
      m_attrib=(WORD)FileInfo.attributes;
      crc=0; //?
      return ZIPPY_SUCCEED;
    }
    else
      return ZIPPY_FAIL;
  }
  else
#endif
  if(strcmp(type,"ZIP")==0||strcmp(type,"STZ")==0)
  {
#ifdef UNIX
    uf=unzOpen(name);
    if (uf==NULL){
      last_error="Couldn't open ";
      last_error+=name;
      return ZIPPY_FAIL;
    }else{
      is_open=true;
      err=unzGetGlobalInfo(uf,&gi);
      current_file_n=0;
      current_file_offset=0;
      err=unzGetCurrentFileInfo(uf,&fi,filename_inzip,
            sizeof(filename_inzip),NULL,0,NULL,0);
      if (err){
        close();
        return ZIPPY_FAIL;
      }
      crc=fi.crc;
    }
#endif
#ifdef WIN32
#if !defined(SSE_NO_UNZIPD32)
    ZeroMemory(&PackInfo,sizeof(PackInfo));
    if(GetFirstInZip(name,&PackInfo)!=UNZIP_Ok)
      return ZIPPY_FAIL;
    is_open=true;
    current_file_n=0;
    current_file_offset=PackInfo.HeaderOffset;
    m_attrib=PackInfo.Attr;
    crc=PackInfo.Crc;
#endif
#endif
    return ZIPPY_SUCCEED;
  } else
#ifdef RARLIB_SUPPORT
  if (strcmp(type,"RAR")==0){
    if (urarlib_list(name,(ArchiveList_struct*)&rar_list)<=0) return ZIPPY_FAIL;

    // There are some files in this RAR
    while (rar_list){
      if ((rar_list->item.FileAttr & 0x10)==0) break; // Not directory
      rar_list=rar_list->next;
    }
    if (rar_list==NULL) return ZIPPY_FAIL;

    is_open=true;
    rar_current=rar_list;
    current_file_n=0;
    current_file_offset=0;
    m_attrib=WORD(rar_current->item.FileAttr);
    crc=rar_current->item.FileCRC;

    return ZIPPY_SUCCEED;
  } else
#endif
#if defined(SSE_DISK_RAR_SUPPORT_WIN)
  if(UNRAR_OK && strcmp(type,"RAR")==0) 
  {
    ArchiveData.ArcName=name;
    hArcData=rarOpenArchive(&ArchiveData);
    ASSERTB(hArcData); // can assert if no ST files in archive
    ASSERTB(!ArchiveData.OpenResult);
    if(!hArcData || ArchiveData.OpenResult)
      return ZIPPY_FAIL;
    int ec=rarReadHeader(hArcData,&HeaderData);
    if(!ec)
    {
      is_open=true;
      current_file_n=0;
      current_file_offset=0;
      m_attrib=(WORD)HeaderData.FileAttr;
      crc=HeaderData.FileCRC;
      return ZIPPY_SUCCEED;
    }
  }
#else
  {}
#endif
  return ZIPPY_FAIL;
}


bool zipclass::next() {
  if(!enable_zip)
    return ZIPPY_FAIL;
    
#if defined(SSE_DISK_RAR_SUPPORT_UNIX) || defined(SSE_DISK_7Z_SUPPORT_UNIX)
  if(0
#if defined(SSE_DISK_RAR_SUPPORT_UNIX)  
    ||(strcmp(type,"RAR")==0)
#endif
#if defined(SSE_DISK_7Z_SUPPORT_UNIX)
    ||(for_7za())
#endif
    )
  {
    bool ok=false;
    DirSearch ds; 
    EasyStr Fol=TempPath+SLASH+"unzip"+SLASH;
    if(ds.Find(Fol+"*.*"))
    {
      while(ds.Name=="." || ds.Name=="..")
        ds.Next();      
      for(int i=0;i<=current_file_n;i++)
        ok=ds.Next();
    }
    ds.Close();
    if(ok)
    {
      current_file_n++;
      current_file_offset=current_file_n;
      return ZIPPY_SUCCEED;      
    }
    return ZIPPY_FAIL;
  }
  else
#endif        
#if defined(SSE_ARCHIVEACCESS_SUPPORT)
  if(for_archiveaccess()) 
  {
    if(!is_open||!ArchiveHandle)
      return ZIPPY_FAIL;
    ASSERTB(current_archived_file==current_file_n); // not if folders TODO?
    if(ArchiveAccess_Select(current_file_n+1))
    {
      current_file_n++;
      current_archived_file++;      
      m_attrib=(WORD)FileInfo.attributes;
      current_file_offset=current_file_n;
      return ZIPPY_SUCCEED;
    }
  } else
#endif
  if(strcmp(type,"ZIP")==0||strcmp(type,"STZ")==0)
  {
#ifdef UNIX
    if (is_open==0) return ZIPPY_FAIL;
  	if (current_file_n>=(int)(gi.number_entry-1)) return ZIPPY_FAIL;
    err=unzGoToNextFile(uf);
    if (err) return ZIPPY_FAIL;
    err=unzGetCurrentFileInfo(uf,&fi,filename_inzip,
          sizeof(filename_inzip),NULL,0,NULL,0);
    if (err) return ZIPPY_FAIL;
    current_file_n++;
    current_file_offset=current_file_n;
    crc=fi.crc;
#endif
#ifdef WIN32
#if !defined(SSE_NO_UNZIPD32)
    err=GetNextInZip(&PackInfo);
    if(err!=UNZIP_Ok)
      return ZIPPY_FAIL;
    m_attrib=PackInfo.Attr;
    crc=PackInfo.Crc;
    current_file_n++;
    current_file_offset=PackInfo.HeaderOffset;
#endif
#endif
    return ZIPPY_SUCCEED;
  }else
#ifdef RARLIB_SUPPORT
  if (strcmp(type,"RAR")==0){
    if (is_open==0 || rar_current==NULL) return ZIPPY_FAIL;
    do{
      rar_current=rar_current->next;
      current_file_n++;
      if (rar_current==NULL) return ZIPPY_FAIL;
    } while (rar_current->item.FileAttr & 0x10); // Skip if directory
    current_file_offset=current_file_n;
    m_attrib=WORD(rar_current->item.FileAttr);
    crc=rar_current->item.FileCRC;
    return ZIPPY_SUCCEED;
  }else
#endif
#if defined(SSE_DISK_RAR_SUPPORT_WIN)
  if(strcmp(type,"RAR")==0) 
  {
    if(!is_open||!UNRAR_OK||!hArcData)
      return ZIPPY_FAIL;
    int ec;
    // we must 'process' the current one or reading will fail
    ec=rarProcessFile(hArcData,RAR_TEST,NULL,NULL);
    if(!ec)
      ec=rarReadHeader(hArcData,&HeaderData);
    current_file_n++;
    if(!ec)
    {
      current_file_offset=current_file_n;
      m_attrib=(WORD)HeaderData.FileAttr;
      crc=HeaderData.FileCRC;
      return ZIPPY_SUCCEED;
    }
  }
#else
  {}
#endif
  return ZIPPY_FAIL;
}


char* zipclass::filename_in_zip() {
  if(!enable_zip) 
    return "";

#if defined(SSE_DISK_RAR_SUPPORT_UNIX) || defined(SSE_DISK_7Z_SUPPORT_UNIX)
  if(0
#if defined(SSE_DISK_RAR_SUPPORT_UNIX)  
    ||(strcmp(type,"RAR")==0)
#endif
#if defined(SSE_DISK_7Z_SUPPORT_UNIX)
    ||(for_7za())
#endif
    )    
  {
    static char ansi_string[MAX_PATH];
    DirSearch ds; 
    EasyStr Fol=TempPath+SLASH+"unzip"+SLASH;
    if(ds.Find(Fol+"*.*"))
    {
      while(ds.Name=="." || ds.Name=="..")
        ds.Next();
      for(int i=0;i<current_file_n;i++)
        ds.Next();
      strcpy(ansi_string,ds.Name.Text);
    }
    ds.Close();
    return ansi_string;
  }
  else
#endif      
 
#if defined(SSE_ARCHIVEACCESS_SUPPORT)
  if(for_archiveaccess())
  {
    if(ArchiveHandle) // need to convert name from unicode
    {
      static char ansi_string[SSE_MAX_PATH]; // Size of buffer
      WideCharToMultiByte ( CP_ACP,                // ANSI code page
        WC_COMPOSITECHECK,     // Check for accented characters
        (LPCWSTR)FileInfo.path,         // Source Unicode string
        -1,                    // -1 means string is zero-terminated
        ansi_string,
        SSE_MAX_PATH,  // Size of buffer
        NULL,                  // No default character
        NULL );                // Don't care about this flag
      TRACE_LOG("%S -> %s\n",FileInfo.path,CHECKPATH(ansi_string));
      //TRACE_LOG("%S -> %s\n",FileInfo.path,ansi_string);
      return ansi_string;
    }
  } else
#endif
  if(strcmp(type,"ZIP")==0||strcmp(type,"STZ")==0) 
  {
#ifdef UNIX
    return filename_inzip;
#endif
#ifdef WIN32
#if !defined(SSE_NO_UNZIPD32)
    return PackInfo.FileName;
#endif
#endif
  } else
#ifdef RARLIB_SUPPORT
  if(strcmp(type,"RAR")==0) 
  {
    if (rar_current) return rar_current->item.Name;
  } else
#endif
#if defined(SSE_DISK_RAR_SUPPORT_WIN)
  if(strcmp(type,"RAR")==0) 
  {
    if(UNRAR_OK&&hArcData) 
      return HeaderData.FileName;
  }
#else
  {}
#endif
  return "";
}


bool zipclass::extract_file(char *fn,int offset,char *dest_dir,DWORD attrib,BOOL DontClose) {
//  TRACE_LOG("zippy extract %s (#%d) to %s\n",fn,offset,dest_dir);
  if(!enable_zip) 
    return ZIPPY_FAIL;
#if defined(SSE_DISK_RAR_SUPPORT_UNIX) || defined(SSE_DISK_7Z_SUPPORT_UNIX)
  if(0
#if defined(SSE_DISK_RAR_SUPPORT_UNIX)  
    ||(strcmp(type,"RAR")==0)
#endif
#if defined(SSE_DISK_7Z_SUPPORT_UNIX)
    ||(for_7za())
#endif
    )    
  {
    bool ok=false;
    if(is_open)
      close();    
      
    if(first(fn)==ZIPPY_SUCCEED)
    {
      DirSearch ds; 
      EasyStr Fol=TempPath+SLASH+"unzip"+SLASH;
      
      if(ds.Find(Fol+"*.*"))
      {
        ok=true;
        while(ds.Name=="." || ds.Name=="..")
          ds.Next();        
        for(int i=0;i<offset;i++)
          ok=ds.Next();
        if(ok)
        {
          char command_string[1024]; // unsafe
          sprintf(command_string,"cp \"%s/unzip/%s\" \"%s\"",TempPath.Text,ds.Name.Text,dest_dir);
          int ec=system(command_string);
        }
      }
      ds.Close();
    }
    if(!DontClose)
      close();
    return (ok) ? ZIPPY_SUCCEED : ZIPPY_FAIL;
  }
  else
#endif      
#if defined(SSE_ARCHIVEACCESS_SUPPORT)
  if(for_archiveaccess()) 
  {
    if(is_open)
      close();
    if(first(fn)==ZIPPY_SUCCEED) 
    {
      while(offset>0) {
        if(next()==ZIPPY_FAIL)
        {
          close();
          return ZIPPY_FAIL;
        }
        offset--;
      }
    }
    ArchiveAccess_Extract(dest_dir);
    if(!DontClose)
      close();
#ifdef WIN32
#if !defined(SSE_WRITEDIR) // ignore hide => TODO remove param
    if(hide)
      SetFileAttributes(dest_dir,FILE_ATTRIBUTE_HIDDEN);
    else
#endif
      SetFileAttributes(dest_dir,attrib);
#endif
    return ZIPPY_SUCCEED;
  } else
#endif
  if(strcmp(type,"ZIP")==0||strcmp(type,"STZ")==0) 
  {
    if(is_open) 
      close();
#ifdef UNIX
    uf=unzOpen(fn);
    if (uf==NULL){
      last_error=Str("Couldn't open ")+fn;
      return ZIPPY_FAIL;
    }
    is_open=true;
    err=unzGetGlobalInfo(uf,&gi);
    if (err){
      close();
      return ZIPPY_FAIL;
    }
    unz_global_info gi;
    int err=unzGetGlobalInfo(uf,&gi);
    if (err!=UNZ_OK){
      close();
      last_error="couldn't get global info";
      return ZIPPY_FAIL;
    }
    if (offset>=(int)gi.number_entry){
      close();
      last_error="too few files in zip";
      return ZIPPY_FAIL;
    }
  //  unzGoToFirstFile(uf);
    for (int i=0;i<offset;i++) unzGoToNextFile(uf);
#define UNZIP_BUF_SIZE 8192
    BYTE buf[UNZIP_BUF_SIZE];
    err=unzGetCurrentFileInfo(uf,&fi,filename_inzip,
          sizeof(filename_inzip),NULL,0,NULL,0);
    if (err) return ZIPPY_FAIL;
    EasyStr dest_file=dest_dir;
    if (dest_dir[0]==0 || dest_dir[strlen(dest_dir)-1]=='/'){
      char *t=strrchr(filename_inzip,'/');
      if (t){
        t++;
      }else{
        t=strrchr(filename_inzip,'\\');
        if(t){
          t++;
        }else{
          t=filename_inzip;
        }
      }
      dest_file+=t;
    }
    err=unzOpenCurrentFile(uf);
    if (err){
      close();
      last_error="error opening file in ZIP";
      printf("%s\n",last_error.Text);
      return ZIPPY_FAIL;
    }
    FILE *fout=fopen(dest_file,"wb");
    if (fout==NULL){
      close();unzCloseCurrentFile(uf);
      last_error="error opening file ";last_error+=dest_file.Text;
      printf("%s\n",last_error.Text);
      return ZIPPY_FAIL;
    }
    do{
      err=unzReadCurrentFile(uf,buf,UNZIP_BUF_SIZE);
      if (err<0){
        last_error=EasyStr("error #")+err+" with zipfile in unzReadCurrentFile";
        printf("%s\n",last_error.Text);
        break;
      }else if(err>0){
        fwrite(buf,err,1,fout);
      }
    } while (err>0);
    fclose(fout);
    err=unzCloseCurrentFile(uf);
    close();
    if (err) return ZIPPY_FAIL;
#endif
#ifdef WIN32
#if !defined(SSE_NO_UNZIPD32)
    EasyStr dest_file=dest_dir;
#if defined(SSE_WRITEDIR) && defined(SSE_420R5)
    err=UnzipFile(fn,dest_file.Text,(WORD)attrib,offset,NULL,0);
#else
    err=UnzipFile(fn,dest_file.Text,(WORD)(hide ? 2:attrib),offset,NULL,0);
#endif
    if(err!=UNZIP_Ok) 
      return ZIPPY_FAIL;
#endif
#endif
    return ZIPPY_SUCCEED;
  } else
#ifdef RARLIB_SUPPORT
  if (strcmp(type,"RAR")==0){
    if (is_open) close();
    if (first(fn)==ZIPPY_SUCCEED){
      while (offset > 0){
        if (next()){ // Failed
          close();
          return ZIPPY_FAIL;
        }
        offset--;
      }
    }
    char *data_ptr;
    DWORD data_size;
    if (urarlib_get(&data_ptr,&data_size,rar_current->item.Name,fn,"")==0) return ZIPPY_FAIL;
    close();
    EasyStr dest_file=dest_dir;
    FILE *fp=fopen(dest_file,"wb");
    if (fp==NULL) return ZIPPY_FAIL;
    fwrite(data_ptr,1,data_size,fp);
    fclose(fp);
#ifdef WIN32
    if (hide){
      SetFileAttributes(dest_file,FILE_ATTRIBUTE_HIDDEN);
    }else{
      SetFileAttributes(dest_file,m_attrib);
    }
#endif
    return ZIPPY_SUCCEED;
  } else 
#endif
#if defined(SSE_DISK_RAR_SUPPORT_WIN)
  if(UNRAR_OK && strcmp(type,"RAR")==0)
  {
    if(is_open)
      close();
    if(first(fn)==ZIPPY_SUCCEED) 
    {
      while(offset>0) {
        //if(next()==ZIPPY_SUCCEED) // argh! fix 402R18
        if(next()==ZIPPY_FAIL)
        {
          close();
          return ZIPPY_FAIL;
        }
        offset--;
      }
    }
    //ASSERT(hArcData);
    int ec=rarProcessFile(hArcData,RAR_EXTRACT,NULL,dest_dir); 
//    TRACE_LOG("%s -> %s : %d\n",HeaderData.FileName,dest_dir,ec);
    //ASSERT(!ec);
    if(ec)
      return ZIPPY_FAIL;    
    if(!DontClose)
      close();
#ifdef WIN32
#if defined(SSE_WRITEDIR) && defined(SSE_420R5)
    SetFileAttributes(dest_dir,attrib);
#else
    SetFileAttributes(dest_dir,(hide)?FILE_ATTRIBUTE_HIDDEN:attrib);
#endif
#endif
    return ZIPPY_SUCCEED;
  }
#endif
  return ZIPPY_FAIL;
}


bool zipclass::close() {
  if(!enable_zip) 
    return ZIPPY_FAIL;
  if(is_open) 
  {
#if defined(SSE_DISK_RAR_SUPPORT_UNIX) || defined(SSE_DISK_7Z_SUPPORT_UNIX)
    if(0
#if defined(SSE_DISK_RAR_SUPPORT_UNIX)  
      ||(strcmp(type,"RAR")==0)
#endif
#if defined(SSE_DISK_7Z_SUPPORT_UNIX)
      ||(for_7za())
#endif
      )    
    {
      char command_string[1024]; // unsafe
      sprintf(command_string,"rm -r %s/unzip/*.*",TempPath.Text);
      int ec=system(command_string);
      is_open=false;
      return ZIPPY_SUCCEED;    
    }
    else
#endif    
#if defined(SSE_ARCHIVEACCESS_SUPPORT)
    if(for_archiveaccess()) 
    {
      ArchiveAccess_Close();
      is_open=false;
      return ZIPPY_SUCCEED;
    }
    else
#endif
    if(strcmp(type,"ZIP")==0||strcmp(type,"STZ")==0)
    {
#ifdef UNIX
      unzClose(uf);
#endif
#ifdef WIN32
#if !defined(SSE_NO_UNZIPD32)
      CloseZipFile(&PackInfo);
#endif
#endif
      is_open=false;
      return ZIPPY_SUCCEED;
    } else
#ifdef RARLIB_SUPPORT
    if(strcmp(type,"RAR")==0)
    {
      is_open=false;
      return ZIPPY_SUCCEED;
    } else
#endif
#if defined(SSE_DISK_RAR_SUPPORT_WIN)
    if(UNRAR_OK && strcmp(type,"RAR")==0)
    {
//      TRACE_LOG("closing RAR archive\n");
      rarCloseArchive(hArcData);
      is_open=false;
      return ZIPPY_SUCCEED;
    }
#else
    {}
#endif
  }
  return ZIPPY_FAIL;
}


void zipclass::list_contents(char *path,EasyStringList *eslp,bool st_disks_only) {
// used for Insert Into context menu, ExtractArchiveToSTHardDrive,ExtractDisks,
// GCGetCRC, GCConvertToST, ShowPropDiag, EndDrag, UpdateBPBFiles
  if(!enable_zip) 
    return;
  eslp->DeleteAll();
  if(first(path)==ZIPPY_SUCCEED) 
  {
    do {
      EasyStr filename=filename_in_zip();
      bool addflag=true;
      if(st_disks_only) 
      {
        if(FileIsDisk(filename)==DISK_UNCOMPRESSED
#if defined(SSE_TOS_PRG_AUTORUN)
          // no interference with context menu, MSA Converter
          && !MatchesAnyString_I(strrchr(filename.Text,'.')+1,DISK_EXT_PRG,DISK_EXT_TOS,NULL)
#endif
          || FileIsDisk(filename)==DISK_PASTI)
        {}
        else
          addflag=false;
      }
      if(addflag) 
      {
        if(bCRC) // in case we're using archiveaccess and we want CRC32 (GetContentsFromDiskImage)
        {
#if defined(SSE_LONG_PATH)
          EasyStr sText;
          sText.SetLength(SSE_MAX_PATH);
          char *tempfile=sText.Text;
#else
          char tempfile[MAX_PATH];
#endif
          GetTempFileName(TempPath,"ZIP",0,tempfile);
          extract_file(path,current_file_n,tempfile,0,TRUE); // Don't close!
          DeleteFile(tempfile);
        }
        eslp->Add(filename,current_file_offset,m_attrib,crc);
      }
    } while (next()==ZIPPY_SUCCEED);
  }
  close();
}

#endif//#if defined(SSE_421R0)


#undef LOGSECTION
