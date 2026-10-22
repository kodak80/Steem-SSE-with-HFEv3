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
FILE: macros.cpp
DESCRIPTION: These functions are used by Steem's macro system, to record,
replay, load and save user input.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <macros.h>
#include <stjoy.h>
#include <computer.h>


int macro_record=0,macro_play=0,macro_play_until;
int macro_start_after_ikbd_read_count=0;
int macro_play_max_mouse_speed=MACRO_DEFAULT_MAX_MOUSE;
bool macro_play_has_mouse=false,macro_play_has_keys=false;
bool macro_play_has_joys=false;

DWORD macro_jagpad[2];
Str macro_play_file,macro_record_file;
DynamicArray<TMacroVblInfo> macro_record_store,macro_play_store;
TMacroVblInfo *mrsc=NULL,*mpsc=NULL;


bool macro_mvi_blank(TMacroVblInfo *mvi) {
  if(mvi->xdiff||mvi->ydiff||mvi->keys) 
    return false;
  for(int i=0;i<8;i++)
    if(mvi->stick[i]) 
      return false;
  for(int i=0;i<2;i++)
    if(mvi->jagpad[i]) 
      return false;
  return true;
}


void macro_file_options(int GetSet,char *Path,TMacroFileOptions *lpMFO,FILE *fp) {
  if(GetSet==MACRO_FILE_GET)
  {
    lpMFO->add_mouse_together=MACRO_DEFAULT_ADD_MOUSE;
    lpMFO->allow_same_vbls=MACRO_DEFAULT_ALLOW_VBLS;
    lpMFO->max_mouse_speed=MACRO_DEFAULT_MAX_MOUSE;
  }
  if(Path)
  {
    if(GetSet==MACRO_FILE_GET)
      fp=fopen(Path,"rb");
    else if(Exists(Path))
      fp=fopen(Path,"r+b");
    else
      fp=fopen(Path,"wb");
  }
  if(fp)
  {
    unsigned int Version=2;
    if(GetSet==MACRO_FILE_SET && GetFileLength(fp)==0)
    { // Blank file?
      int Dummy=0;
      FWRITE(&Version,1,4,fp);FWRITE(&Dummy,1,4,fp);
      FWRITE(&Dummy,1,4,fp);FWRITE(&Dummy,1,4,fp);
    }
    FSEEK(fp,0,SEEK_SET);
    FREAD(&Version,1,4,fp);
    if(Version>=2)
    {
      FSEEK(fp,4+4+4+4,SEEK_SET);
#define LoadSave(var) if (GetSet==MACRO_FILE_SET) FWRITE(&(var),1,4,fp); else FREAD(&(var),1,4,fp);
      LoadSave(lpMFO->add_mouse_together);
      LoadSave(lpMFO->max_mouse_speed);
      LoadSave(lpMFO->allow_same_vbls);
#undef LoadSave
    }
    if(Path) 
      fclose(fp);
  }
}


void macro_advance(int StartCode) {
  int max_mouse=0;
  if(macro_record||(StartCode&MACRO_STARTRECORD))
  {
    bool advance=true;
    switch(macro_record) {
    case 0:
    {
      macro_record_store.Resize(Glue.VideoFreq*MACRO_RECORD_BUF_INC_SECS);
      TMacroFileOptions MFO;
      macro_file_options(MACRO_FILE_GET,macro_record_file,&MFO);
      max_mouse=MFO.max_mouse_speed;
      break;
    }
    case 1:
      // Don't move on to 2 until there is input
      if(macro_mvi_blank(mrsc)) 
        advance=false;
      break;
    default:
      if(macro_record>=macro_record_store.GetSize())
        macro_record_store.Resize(macro_record_store.GetSize()
          +Glue.VideoFreq*MACRO_RECORD_BUF_INC_SECS);
    }
    if(advance)
    {
      mrsc=&(macro_record_store[macro_record]);
      macro_record++;
    }
    mrsc->keys=0;
    mrsc->xdiff=0xffff;
  }
  if(macro_play||(StartCode & MACRO_STARTPLAY))
  {
    if(macro_play==0)
    {
      if(!macro_play_start())
        return;
      if(macro_play_has_mouse)
        max_mouse=macro_play_max_mouse_speed;
    }
    if(macro_play>=macro_play_until)
    {
      macro_end(MACRO_ENDPLAY);
      return;
    }
    mpsc=&(macro_play_store[macro_play]);
    macro_play++;
  }
  if(max_mouse)
  {
    mousek=0;
    ikbd_mouse_move(-MACRO_LEFT_INIT_MOVE,-MACRO_UP_INIT_MOVE,mousek,max_mouse);
    macro_start_after_ikbd_read_count=keyboard_buffer_length;
  }
  if(StartCode) 
    OptionBox.UpdateMacroRecordAndPlay();
}


void macro_end(int EndCode) {
  if(macro_record && (EndCode&MACRO_ENDRECORD))
  {
    // Cut off current frame if it hasn't been set yet
    // (macro_record=current frame +1, should be last recorded frame +1)
    if(macro_record_store[macro_record-1].xdiff==0xffff) 
      macro_record--;
    // Cut blank frames off the end
    for(int n=macro_record-1;n>=0;n--)
    {
      if(macro_mvi_blank(&macro_record_store[n]))
        macro_record--;
      else
        break;
    }
    if(macro_record>0)
    {
      TMacroFileOptions MFO;
      macro_file_options(MACRO_FILE_GET,macro_record_file,&MFO);
      FILE *fp=fopen(macro_record_file,"wb");
      if(fp)
      {
        unsigned int Version=2,SizeMVI=sizeof(TMacroVblInfo),StructOffset=7*4;
        FWRITE(&Version,1,4,fp);
        FWRITE(&SizeMVI,1,4,fp);
        FWRITE(&StructOffset,1,4,fp);
        FWRITE(&macro_record,1,4,fp);
        FWRITE(&MFO.add_mouse_together,1,4,fp);
        FWRITE(&MFO.max_mouse_speed,1,4,fp);
        FWRITE(&MFO.allow_same_vbls,1,4,fp);
        for(int n=0;n<macro_record;n++) 
          FWRITE(&(macro_record_store[n]),1,SizeMVI,fp);
        fclose(fp);
      }
    }
    macro_record=0;
    macro_record_store.DeleteAll();
  }
  if(EndCode & MACRO_ENDPLAY)
  {
    macro_play=0;
    macro_play_store.DeleteAll();
    macro_play_has_mouse=macro_play_has_keys=macro_play_has_joys=false;
  }
  if(macro_play==0 && macro_record==0) 
    macro_start_after_ikbd_read_count=0;
  OptionBox.UpdateMacroRecordAndPlay();
}


void macro_record_joy() {
  for(int Port=0;Port<8;Port++) 
    mrsc->stick[Port]=stick[Port];
  mrsc->jagpad[0]=macro_jagpad[0];
  mrsc->jagpad[1]=macro_jagpad[1];
}


void macro_record_mouse(int x_change,int y_change) {
  mrsc->xdiff=x_change;
  mrsc->ydiff=y_change;
}


void macro_record_key(BYTE STCode) {
  if(mrsc->keys<32) 
    mrsc->keycode[mrsc->keys++]=STCode;
}


bool macro_play_start() {
  FILE *fp=fopen(macro_play_file,"rb");
  if(fp==NULL) 
    return false;
  unsigned int Version=0,StructOffset=0;
  DWORD SizeMVI=0;
  FREAD(&Version,1,4,fp);
  FREAD(&SizeMVI,1,4,fp);
  FREAD(&StructOffset,1,4,fp);
  if(Version==0||SizeMVI==0||StructOffset==0)
  {
    fclose(fp);
    return false;
  }
  FREAD(&macro_play_until,1,4,fp);
  macro_play_store.Resize(macro_play_until+16);
  TMacroFileOptions MFO;
  macro_file_options(MACRO_FILE_GET,NULL,&MFO,fp);
  macro_play_max_mouse_speed=MFO.max_mouse_speed;
  FSEEK(fp,StructOffset,SEEK_SET);
  int idx=0;
  TMacroVblInfo last_cut;
  int mxa=0,mya=0,no_event_vbls=0xffff,cut_vbls=0;
  BYTE oldstick[8]={0,0,0,0,0,0,0,0};
  DWORD oldjag[2]={0,0};
  for(int m=0;m<macro_play_until;m++)
  {
    TMacroVblInfo *lpMVI=&(macro_play_store[idx]);
    ZeroMemory(lpMVI,sizeof(TMacroVblInfo)); // Zero what isn't loaded
    FREAD(lpMVI,1,MIN(sizeof(TMacroVblInfo),(size_t)SizeMVI),fp);
    if(sizeof(TMacroVblInfo)<SizeMVI)
      FSEEK(fp,SizeMVI-sizeof(TMacroVblInfo),SEEK_CUR);
    bool new_event=false;
    if(lpMVI->keys>0)
    {
      new_event=true;
      macro_play_has_keys=true;
    }
    for(int Port=0;Port<8;Port++)
    {
      if(lpMVI->stick[Port]!=oldstick[Port])
      {
        new_event=true;
        macro_play_has_joys=true;
      }
      oldstick[Port]=lpMVI->stick[Port];
    }
    for(int Jag=0;Jag<2;Jag++)
    {
      if(lpMVI->jagpad[Jag]!=oldjag[Jag])
      {
        new_event=true;
        macro_play_has_joys=true;
      }
      oldjag[Jag]=lpMVI->jagpad[Jag];
    }
    bool CanCutFrame=false;
    if(MFO.allow_same_vbls>0)
    {
      if(new_event)
        no_event_vbls=0;
      else
      {
        no_event_vbls++;
        if(no_event_vbls>=MFO.allow_same_vbls) 
          CanCutFrame=true;
      }
    }
    if(CanCutFrame)
    {
      mxa+=lpMVI->xdiff;
      mya+=lpMVI->ydiff;
      // Store stick and jagpad settings (mouse and keys ignored)
      if(MFO.allow_same_vbls>0)
        last_cut=*lpMVI;
      cut_vbls++;
    }
    else
    {
      if(mxa||mya||lpMVI->xdiff||lpMVI->ydiff)
        macro_play_has_mouse=true;
      if(MFO.allow_same_vbls>0&&cut_vbls)
      {
        // Store current frame
        TMacroVblInfo MVI=macro_play_store[idx];
        // Must insert frame(s) before pressing button to move mouse to new position
        cut_vbls=MIN(cut_vbls,MFO.allow_same_vbls);
        for(int n=0;n<cut_vbls;n++)
        {
          // Make all the movement occur on the first frame
          last_cut.xdiff=mxa;mxa=0;
          last_cut.ydiff=mya;mya=0;
          macro_play_store[idx++]=last_cut;
        }
        macro_play_store[idx]=MVI;
      }
      idx++; // Insert current frame
      cut_vbls=0;
      if(idx>=macro_play_store.GetSize())
        macro_play_store.Resize(macro_play_store.GetSize()+16);
    }
  }
  if(mxa||mya)
  { // Left over movement
    macro_play_has_mouse=true;
    macro_play_store[idx].xdiff=mxa;
    macro_play_store[idx].ydiff=mya;
    idx++;
  }
  fclose(fp);
  macro_play_until=idx;
  return true;
}


void macro_play_joy() {
  for(int Port=0;Port<8;Port++) 
    stick[Port]=(BYTE)(mpsc->stick[Port]);
  macro_jagpad[0]=mpsc->jagpad[0];
  macro_jagpad[1]=mpsc->jagpad[1];
}


void macro_play_mouse(int &x_change,int &y_change) {
  x_change=mpsc->xdiff;
  y_change=mpsc->ydiff;
}


void macro_play_keys() {
  for(DWORD n=0;n<mpsc->keys;n++)
  {
    keyboard_buffer_write(mpsc->keycode[n]);
  }
}

#undef LOGSECTION
