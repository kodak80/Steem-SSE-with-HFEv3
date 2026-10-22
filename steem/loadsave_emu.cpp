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
FILE: loadsave_emu.cpp
DESCRIPTION: Functions to load and save emulation variables. This is mainly
for Steem's memory snapshots system.
There's some redundancy and bloat because of backward compatibility (which
is far from perfect).
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <computer.h>
#include <draw.h>
#include <display.h>
#include <loadsave.h>
#include <stjoy.h>
#include <diskman.h>
#include <translate.h>
#if defined(SSE_VID_STVL1)
#include <interface_stvl.h>
#endif

#if defined(SSE_LIBRETRO)
// we work with a pointer, not a file
// this was all coded by Steem Authors for "ONEGAME"
// but only for LS_LOAD

void ReadWriteVar(void *lpVar,DWORD szVar,BYTE* &pMem,int LoadOrSave,int Type,int Version) {
  ASSERTB(Version>17);
  bool SaveSize;
  if(Type==0)  // Variable
    SaveSize=(Version==17);
  else if(Type==1)  // Array
    SaveSize=(Version>=3);
  else   // Struct
    SaveSize=(Version>=5);

  if(LoadOrSave==LS_LOAD)
  {
    BYTE *pVar=(BYTE*)lpVar;
    if(!SaveSize)
    {
      ASSERTB(szVar<=8 || szVar==200); // 8 double, 200 hd6301 buffer (old mistake)
      for(DWORD n=0;n<szVar;n++)
        *pVar++=*pMem++; //it's one of these...
    }
    else if(Type==3)  // byte loaded/saved as int
    {
      //ASSERT(szVar==4);
      int temp;
      BYTE *tp=(BYTE*)&temp;
      for(DWORD n=0;n<szVar;n++)
        *tp++=*pMem++;
      *(BYTE*)lpVar=(BYTE)temp;
    }
    else if(Type==4) // word loaded/saved as int
    {
      int temp;
      BYTE *tp=(BYTE*)&temp;
      for(DWORD n=0;n<szVar;n++)
        *tp++=*pMem++;
      *(WORD*)lpVar=(WORD)temp;
    }
    else
    { 
      DWORD l=*(LPDWORD)pMem; // get size
      pMem+=4;
      for(DWORD n=0;n<l;n++)
      {
        BYTE b=*pMem++;
        if(n<szVar)
          *pVar++=b;
      }
      
    }
  }
  else //LS_SAVE
  {
    BYTE *pVar=(BYTE*)lpVar;
    if(!SaveSize)
    {
      for(DWORD n=0;n<szVar;n++)
        *pMem++=*pVar++;
    }
    else if(Type==3)  // byte loaded/saved as int
    {
      //ASSERT(szVar==4);
      int temp=*(BYTE*)lpVar;
      BYTE *tp=(BYTE*)&temp;
      for(DWORD n=0;n<szVar;n++)
        *pMem++=*tp++;
    }
    else if(Type==4) // word loaded/saved as int
    {
      int temp=*(WORD*)lpVar;
      BYTE *tp=(BYTE*)&temp;
      for(DWORD n=0;n<szVar;n++)
        *pMem++=*tp++;
    }
    else
    {
      DWORD l=szVar;
      *(LPDWORD)pMem=l; // record size
      pMem+=4;
      for(DWORD n=0;n<l;n++)
      {
        BYTE b=*pVar++;
       // if(n<szVar)
          *pMem++=b;
      }
    }
  }
}


int ReadWriteEasyStr(EasyStr &s,BYTE* &pMem,int LoadOrSave,int) {

  if (LoadOrSave==LS_LOAD){
    int l=*(int*)(pMem);pMem+=4;
    if(l<0 || l>260)
      throw 2; // Corrupt snapshot
    s.SetLength(l);
    char *pT=s.Text;
    for(int n=0;n<l;n++)
      *pT++=(char)*pMem++;
  }
  else //LS_SAVE
  {
    int l=(int)s.Length();
    *(LPDWORD)pMem=l; // record size
    pMem+=4;
    for(int n=0;n<l;n++)
    {
      BYTE b=s.Text[n];
      *pMem++=b;
    }
  }
  return ERR_OK;
}


#else

void ReadWriteVar(void *lpVar,DWORD szVar,NOT_ONEGAME( FILE *fp ) 
              ONEGAME_ONLY( BYTE* &pMem ),int LoadOrSave,int Type,int Version) {
  // v402: throw 2 (corrupt snapshot) on R/W error
  bool SaveSize;
  if(Type==0)  // Variable
    SaveSize=(Version==17);
  else if(Type==1)  // Array
    SaveSize=(Version>=3);
  else   // Struct
    SaveSize=(Version>=5);
#ifndef ONEGAME
  if(SaveSize==0) 
  {
    if(LoadOrSave==LS_SAVE)
    {
      if(FWRITE(lpVar,1,szVar,fp)!=szVar)
        throw 2;
    }
    else
    {
      if(FREAD(lpVar,1,szVar,fp)!=szVar)
        throw 2;
    }
  }
  else if(Type==3)  // byte loaded/saved as int
  {
    //ASSERT(szVar==4);
    int temp=*(BYTE*)lpVar;
    if(LoadOrSave==LS_SAVE)
    {
      if(FWRITE(&temp,1,szVar,fp)!=szVar)
        throw 2;
    }
    else
    {
      if(FREAD(&temp,1,szVar,fp)!=szVar)
        throw 2;
    }
    *(BYTE*)lpVar=(BYTE)temp;
  }
  else if(Type==4) // word loaded/saved as int
  {
    //ASSERT(szVar==4);
    int temp=*(WORD*)lpVar;
    if(LoadOrSave==LS_SAVE)
    {
      if(FWRITE(&temp,1,szVar,fp)!=szVar)
        throw 2;
    }
    else
    {
      if(FREAD(&temp,1,szVar,fp)!=szVar)
        throw 2;
    }
    *(WORD*)lpVar=(WORD)temp;
  }
  else if(LoadOrSave==LS_SAVE)
  {
    if(FWRITE(&szVar,1,sizeof(szVar),fp)!=sizeof(szVar))
      throw 2;
    if(FWRITE(lpVar,1,szVar,fp)!=szVar)
      throw 2;
  }
  else
  {
    DWORD l=0;
    if(FREAD(&l,1,sizeof(l),fp)!=sizeof(l))
      throw 2;
    if(szVar<l) // bigger on file
    {
      if(FREAD(lpVar,1,szVar,fp)!=szVar)
        throw 2;
      FSEEK(fp,l-szVar,SEEK_CUR); // skip rest
    }
    else
    {
      if(FREAD(lpVar,1,l,fp)!=l)
        throw 2;
    }
  }
#else
  if(LoadOrSave==LS_LOAD) {
    BYTE *pVar=(BYTE*)lpVar;
    if(SaveSize==0) {
      for(DWORD n=0;n<szVar;n++) *(pVar++)=*(pMem++);
    }
    else {
      DWORD l=*LPDWORD(pMem);pMem+=4;
      for(DWORD n=0;n<l;n++) {
        BYTE b=*(pMem++);
        if(n<szVar) *(pVar++)=b;
      }
    }
  }
#endif
}


int ReadWriteEasyStr(EasyStr &s,NOT_ONEGAME( FILE *fp ) 
                  ONEGAME_ONLY( BYTE* &pMem ),int LoadOrSave,int) {
#ifndef ONEGAME
  size_t l; // must remain size_t for snapshot compatibility
  if(LoadOrSave==LS_SAVE)
  {
    l=s.Length();
    FWRITE(&l,1,sizeof(l),fp);
    FWRITE(s.Text,1,l,fp);
  }
  else
  {
#if defined(SSE_420R5)
    l=0;
#else
    l=(size_t)-1;
#endif
    if(FREAD(&l,1,sizeof(l),fp)!=sizeof(l))
      throw 2;
#if defined(SSE_420R5) // long path possible
    s.SetLength((int)l);
#else
    if(l>260) 
      throw 2; // Corrupt snapshot
    s.SetLength(l);
#endif
    if(l)
    {
      if(FREAD(s.Text,1,l,fp)!=l)
        throw 2;
    }
  }
#else
  if (LoadOrSave==LS_LOAD){
    int l=*(int*)(pMem);pMem+=4;
    //if (l<0 || l>260) return 2; // Corrupt snapshot
    if (l<0 || l>260) throw 2; // Corrupt snapshot
    s.SetLength(l);
    char *pT=s.Text;
    for (int n=0;n<l;n++) *(pT++)=(char)*(pMem++);
  }
#endif
  return ERR_OK;
}

#endif//#if defined(SSE_LIBRETRO)

// it would be hard to do without those macros, IDE code stepping still OK so...
#define ReadWrite(var) ReadWriteVar(&(var),sizeof(var),fp,LoadOrSave,0,Version)
#define ReadWriteByteAsInt(var) ReadWriteVar(&(var),sizeof(int),fp,LoadOrSave,3,Version)
#define ReadWriteWordAsInt(var) ReadWriteVar(&(var),sizeof(int),fp,LoadOrSave,4,Version)
#define ReadWriteArray(var) ReadWriteVar(var,sizeof(var),fp,LoadOrSave,1,Version)
#define ReadWriteStruct(var) ReadWriteVar(&(var),sizeof(var),fp,LoadOrSave,2,Version)
#define ReadWriteStr(s) {int i_of_ReadWriteStr=ReadWriteEasyStr(s,fp,LoadOrSave\
,Version);if (i_of_ReadWriteStr) return i_of_ReadWriteStr; }

#ifdef ONEGAME
int LoadSaveAllStuff(BYTE* &fp,bool LoadOrSave,int Version,bool,int *pVerRet)
#elif defined(SSE_LIBRETRO)
int LoadSaveAllStuff(BYTE* fp,bool LoadOrSave,int Version,
                     bool ChangeDisksAndCart,int *pVerRet)
#else
int LoadSaveAllStuff(FILE *fp,bool LoadOrSave,int Version,
                     bool ChangeDisksAndCart,int *pVerRet)
#endif
{
  try { // some functions called may throw integers 1 or 2
    ONEGAME_ONLY(BYTE *pStartByte=fp; )
#if defined(SSE_LIBRETRO)
    BYTE *pStartByte=fp;
    //LoadOrSave=LS_SAVE;
#endif
    a_s_t=A_S_T;
    int dummy_int=0;
//    WORD dummy_word=0;
    BYTE dummy_byte=0;
//    bool dummy_bool=false;
    if(Version==-1)
      Version=SNAPSHOT_VERSION;
    //TRACE_INIT("%s memory snaphot V%d\n",(LoadOrSave==LS_LOAD?"Load":"Save"),Version);
    ReadWrite(Version); 
    if(pVerRet)
      *pVerRet=Version;
    ReadWrite(pc);
    ReadWrite(pc_high_byte);
    ReadWriteArray(Cpu.r);
    UPDATE_SR;
    ReadWrite(SR);
    UPDATE_FLAGS;
    ReadWrite(other_sp);
    ReadWrite(vbase);
    ReadWriteArray(STpal);
    ReadWrite(interrupt_depth);
    ReadWrite(on_rte);
    ReadWrite(on_rte_interrupt_depth);
    ReadWrite(shifter_draw_pointer);
    ReadWriteByteAsInt(Glue.VideoFreq);
    if(Glue.VideoFreq>65)
      Glue.VideoFreq=MONO_HZ;
    ReadWriteWordAsInt(shifter_x);
    ReadWriteWordAsInt(shifter_y);
    if(Version>=61)
    {
      // recognize wrong bitness snapshot if it was recorded (since v402)
#ifdef SSE_X64
      int bitness=0x64;
#else
      int bitness=0x32;
#endif
      ReadWrite(bitness);
#ifdef SSE_X64
      switch(bitness&0xff) {
      case 0x32:
        throw 3;
      case 0x64:
        break; // OK
      default:
        Alert(T("Snapshot not recognised as Steem64 compatible"),T("Warning"),0);
      }//sw
#else
      if((bitness&0xff)==0x64)
        throw 3;
#endif
    }
    else
      ReadWrite(dummy_int); //was int shifter_scanline_width_in_bytes;
    ReadWriteByteAsInt(Mmu.linewid);
    ReadWriteByteAsInt(shifter_hscroll);
    ReadWrite(screen_res);
    ReadWrite(Mmu.Config);
    ReadWriteArray(Mfp.reg);
    {
    int dummy[4]; //was mfp_timer_precounter[4];
    ReadWriteArray(dummy);
    }
    // Make sure saving can't affect current emulation
    int save_mfp_timer_counter[4];
    int save_mfp_timer_period[4];
    if(LoadOrSave==LS_SAVE)
    {
      memcpy(save_mfp_timer_counter,mfp_timer_counter,sizeof(mfp_timer_counter));
      memcpy(save_mfp_timer_period,mfp_timer_period,sizeof(mfp_timer_period));
      for(BYTE ti=0;ti<4;ti++)
        Mfp.CalcTimerCounter(ti,a_s_t);
    }
    ReadWriteArray(mfp_timer_counter);
    if(LoadOrSave==LS_SAVE)
    {
      memcpy(mfp_timer_counter,save_mfp_timer_counter,sizeof(mfp_timer_counter));
      memcpy(mfp_timer_period,save_mfp_timer_period,sizeof(mfp_timer_period));
    }
    ReadWrite(mfp_gpip_no_interrupt);
    SSEConfig.ColourMonitor=((mfp_gpip_no_interrupt&MFP_GPIP_COLOUR)!=0);
    ReadWriteByteAsInt(psg_reg_select);        //4
    ReadWriteArray(psg_reg); //16
    ReadWrite(Mmu.SoundControl);
    ReadWrite(SteSndFrameStart);
    ReadWrite(SteSndFrameEnd);
    ReadWrite(shifter_sound_mode);
    // handle v394 snapshots, verbose but helps player
    if(LoadOrSave==LS_LOAD && Version>=42 && Version<60) //v340->394
    {
      struct IKBD_STRUCT{
        BYTE ram[128]; 
        DWORD cursor_key_joy_time[6];
        DWORD cursor_key_joy_ticks[4];
        int command_read_count,command_parameter_counter;
        int mouse_mode;
        int joy_mode;
        int abs_mouse_max_x,abs_mouse_max_y;
        int cursor_key_mouse_pulse_count_x,cursor_key_mouse_pulse_count_y;
        int relative_mouse_threshold_x,relative_mouse_threshold_y;
        int abs_mouse_scale_x,abs_mouse_scale_y;
        int abs_mouse_x,abs_mouse_y;
        int duration;
        int abs_mousek_flags;
        int psyg_hack_stage;
        int clock_vbl_count;
        int reset_121A_hack;
        int reset_0814_hack;
        int reset_1214_hack;
        int joy_packet_pos;
        int mouse_packet_pos;
        WORD load_memory_address;
        BYTE command_param[8];
        BYTE command;
        BYTE mouse_button_press_what_message;
        BYTE clock[6];
        bool mouse_upside_down;
        bool send_nothing;
        bool port_0_joy;
        bool resetting;
      };
      IKBD_STRUCT ikbd;
      ReadWriteStruct(ikbd);
      // simple but tedious - no need to copy RAM
      for(int i=0;i<6;i++) // struct -> no memcpy
        Ikbd.cursor_key_joy_time[i]=ikbd.cursor_key_joy_time[i];
      for(int i=0;i<4;i++)
        Ikbd.cursor_key_joy_ticks[i]=ikbd.cursor_key_joy_ticks[i];
      Ikbd.command_read_count=ikbd.command_read_count;
      Ikbd.command_parameter_counter=ikbd.command_parameter_counter;
      Ikbd.mouse_mode=ikbd.mouse_mode;
      Ikbd.joy_mode=ikbd.joy_mode;
      Ikbd.abs_mouse_max_x=ikbd.abs_mouse_max_x;
      Ikbd.abs_mouse_max_y=ikbd.abs_mouse_max_y;
      Ikbd.cursor_key_mouse_pulse_count_x=ikbd.cursor_key_mouse_pulse_count_x;
      Ikbd.cursor_key_mouse_pulse_count_y=ikbd.cursor_key_mouse_pulse_count_y;
      Ikbd.relative_mouse_threshold_x=ikbd.relative_mouse_threshold_x;
      Ikbd.relative_mouse_threshold_y=ikbd.relative_mouse_threshold_y;
      Ikbd.abs_mouse_scale_x=ikbd.abs_mouse_scale_x;
      Ikbd.abs_mouse_scale_y=ikbd.abs_mouse_scale_y;
      Ikbd.abs_mouse_x=ikbd.abs_mouse_x;
      Ikbd.abs_mouse_y=ikbd.abs_mouse_y;
      Ikbd.duration=ikbd.duration;
      Ikbd.abs_mousek_flags=ikbd.abs_mousek_flags;
      Ikbd.psyg_hack_stage=ikbd.psyg_hack_stage;
      Ikbd.clock_vbl_count=ikbd.clock_vbl_count;
      Ikbd.reset_121A_hack=ikbd.reset_121A_hack;
      Ikbd.reset_0814_hack=ikbd.reset_0814_hack;
      Ikbd.reset_1214_hack=ikbd.reset_1214_hack;
      Ikbd.joy_packet_pos=ikbd.joy_packet_pos;
      Ikbd.mouse_packet_pos=ikbd.mouse_packet_pos;
      Ikbd.load_memory_address=ikbd.load_memory_address;
      for(int i=0;i<8;i++)
        Ikbd.command_param[i]=ikbd.command_param[i];
      Ikbd.command=ikbd.command;
      Ikbd.mouse_button_press_what_message=ikbd.mouse_button_press_what_message;
      for(int i=0;i<6;i++)
        Ikbd.clock[i]=ikbd.clock[i];
      Ikbd.mouse_upside_down=ikbd.mouse_upside_down;
      Ikbd.send_nothing=ikbd.send_nothing;
      Ikbd.port_0_joy=ikbd.port_0_joy;
      Ikbd.resetting=ikbd.resetting;
    }
    else
      ReadWriteStruct(Ikbd); // merged
    ReadWriteArray(keyboard_buffer);
    ReadWriteWordAsInt(keyboard_buffer_length);
    if(Version<8) {
      keyboard_buffer[0]=0;
      keyboard_buffer_length=0;
    }
    ReadWrite(Dma.mcr);
    ReadWrite(Dma.sr);
    ReadWrite(dma_address);
    ReadWriteWordAsInt(Dma.Counter);
    ReadWrite(Fdc.cr);
    ReadWrite(Fdc.tr);
    ReadWrite(Fdc.sr);
    ReadWrite(Fdc.str);
    ReadWrite(Fdc.dr);
    ReadWrite(Fdc.Lines.direction); //was char fdc_last_step_inwards_flag
    BYTE floppy_head_track[2]={FloppyDrive[DRIVE_A].track,FloppyDrive[DRIVE_B].track};
    ReadWriteArray(floppy_head_track);
    FloppyDrive[DRIVE_A].track=floppy_head_track[DRIVE_A];
    FloppyDrive[DRIVE_B].track=floppy_head_track[DRIVE_B];
    ReadWriteArray(floppy_mediach);
    ReadWrite(Stemdos.PexecListPtr);
    ReadWriteArray(Stemdos.PexecList);
    ReadWriteByteAsInt(Stemdos.CurrentDrive);
    EasyStr NewROM=ROMFile;
    ReadWriteEasyStr(NewROM,fp,LoadOrSave,Version);
    WORD NewROMVer=tos_version;
    if(Version>=7)
      ReadWrite(NewROMVer);
    else
      NewROMVer=0x701;
    if(LoadOrSave==LS_LOAD && NewROMVer<0x106&&Version<41)
      ST_MODEL=STF; //for older snapshots pre SSE
    ReadWrite(SSEConfig.bank_length[0]);
    ReadWrite(SSEConfig.bank_length[1]);
    if(LoadOrSave==LS_LOAD)
    {
      BYTE MemConf[2];
      GetCurrentMemConf(MemConf);
      SSEConfig.make_Mem(MemConf[0],MemConf[1]);
    }
    EasyStr NewDiskName[2],NewDisk[2];
    if(Version>=1)
    {
      for(int d=DRIVE_A;d<=DRIVE_B;d++)
      {
        NewDiskName[d]=FloppyDisk[d].DiskName;
        ReadWriteStr(NewDiskName[d]);
        NewDisk[d]=FloppyDrive[d].GetDisk();
        ReadWriteStr(NewDisk[d]);
      }
    }
    if(Version>=2)
    {
      for(int n=0;n<GEMDOS_MAXDRIVES;n++)
        ReadWriteStr(Stemdos.CurrentPath[n]);
#ifndef DISABLE_STEMDOS
      if(LoadOrSave==LS_LOAD)
        Stemdos.CheckPaths();
#endif
    }
    if(Version>=4)
      ReadWriteStruct(Blitter);
    if(Version>=5)
      ReadWriteArray(ST_Key_Down);
    if(Version>=8)
      ReadWriteStruct(acia[ACIA_IKBD]);
    if(Version<44&&LoadOrSave==LS_LOAD) //v3.5.1
    {
      acia[ACIA_IKBD].cr=0x96; // usually
      acia[ACIA_IKBD].sr=2; // usually
    }
    if(Version>=9)
    {
      ReadWrite(Stemdos.Dta); //4
    }
    if(Version>=10) 
    {
      ReadWrite(SteSndFetchAd);    //4
      ReadWrite(NextSteSndFrameEnd);   //4
      ReadWrite(NextSteSndFrameStart); //4
    }
    else if(LoadOrSave==LS_LOAD) 
    {
      NextSteSndFrameEnd=SteSndFrameEnd;
      NextSteSndFrameStart=SteSndFrameStart;
      SteSndFetchAd=SteSndFrameStart;
    }
    //SteSoundFreq=SteSoundModeToFreq[shifter_sound_mode&3];
    SteSoundOutputCountdown=0;
    //INT_PTR StartOfData=0;
    DWORD StartOfData=0; // stay compatible with v412 64bit...
#ifndef ONEGAME
#if defined(SSE_LIBRETRO)
    DWORD StartOfDataPos=(DWORD)(fp-pStartByte);
#else
    NOT_ONEGAME(DWORD StartOfDataPos=ftell(fp); )
#endif
#endif
    if(Version>=11) 
      ReadWrite(StartOfData);
    if(Version>=12) 
    {
      ReadWrite(os_gemdos_vector);
      ReadWrite(os_bios_vector);
      ReadWrite(os_xbios_vector);
    }
    if(Version>=13) 
      ReadWrite(paddles_ReadMask);
    EasyStr NewCart=CartFile;
    if(Version>=14) 
      ReadWriteStr(NewCart);
    if(Version>=15) 
    {
      ReadWrite(rs232_recv_byte);
      ReadWrite(rs232_recv_overrun);
      ReadWrite(rs232_bits_per_word);
      ReadWrite(rs232_hbls_per_word);
    }
    EasyStr NewDiskInZip[2];
    if(Version>=20) 
    {
      for(int d=DRIVE_A;d<=DRIVE_B;d++) {
        NewDiskInZip[d]=FloppyDisk[d].DiskInZip;
        ReadWriteStr(NewDiskInZip[d]);
      }
    }
#ifndef ONEGAME
    bool ChangeTOS=true,ChangeCart=ChangeDisksAndCart,ChangeDisks=ChangeDisksAndCart;
#endif
    DWORD ExtraFlags=0;
    if(Version>=21) 
      ReadWrite(ExtraFlags);
#ifndef ONEGAME
    if(ExtraFlags & BIT_0) 
      ChangeDisks=false;
    // Flag here for saving disks in this file? (huge!)
    if(ExtraFlags & BIT_1) 
      ChangeTOS=false;
    // Flag here for only asking user to locate version and country code TOS?
    // Flag here for saving TOS in this file?
    if(ExtraFlags & BIT_2) 
      ChangeCart=false;
    // Flag here for saving the cart in this file?
#endif
    if(Version>=22) 
    {
      int max_fsnexts=MAX_STEMDOS_FSNEXT_STRUCTS;
      ReadWrite(max_fsnexts);
      for(int n=0;n<max_fsnexts;n++) 
      {
        ReadWrite(Stemdos.FsnextData[n].dta);
        // If this is invalid then it will just return "no more files"
        ReadWriteStr(Stemdos.FsnextData[n].path);
        ReadWriteStr(Stemdos.FsnextData[n].NextFile);
        ReadWrite(Stemdos.FsnextData[n].attr);
        ReadWrite(Stemdos.FsnextData[n].start_hbl);
      }
    }
    if(Version>=23) 
      ReadWrite(Glue.hscroll);
#ifdef NO_CRAZY_MONITOR
    int em_width=480,em_height=480,em_planes=4,my_extended_monitor=0,aes_calls_since_reset=0;
    LONG save_r[16];
    MEM_ADDRESS line_a_base=0,vdi_intout=0;
#else
    BYTE &my_extended_monitor=extended_monitor;
#endif
    bool old_em=(my_extended_monitor!=0);
    if(Version>=24) 
    {
      ReadWriteWordAsInt(em_width);
      ReadWriteWordAsInt(em_height);
      ReadWriteByteAsInt(em_planes);
      ReadWriteByteAsInt(my_extended_monitor);
      ReadWrite(aes_calls_since_reset);
      ReadWriteArray(save_r);
      ReadWrite(line_a_base);
      ReadWrite(vdi_intout);
      if(LoadOrSave==LS_LOAD)
        vdi_intout=line_a_base=0; //?
    }
    else if(LoadOrSave==LS_LOAD)
      extended_monitor=0;
    if(Version>=25) 
    {
      if(LoadOrSave==LS_SAVE) 
      {
        memcpy(save_mfp_timer_counter,mfp_timer_counter,sizeof(mfp_timer_counter));
        memcpy(save_mfp_timer_period,mfp_timer_period,sizeof(mfp_timer_period));
      }
      for(BYTE ti=0;ti<4;ti++)
      {
        BYTE prescale_ticks=0;
        if(LoadOrSave==LS_SAVE) 
          prescale_ticks=Mfp.CalcTimerCounter(ti,a_s_t);
        ReadWrite(prescale_ticks);
      }
      if(LoadOrSave==LS_SAVE) 
      {
        memcpy(mfp_timer_counter,save_mfp_timer_counter,sizeof(mfp_timer_counter));
        memcpy(mfp_timer_period,save_mfp_timer_period,sizeof(mfp_timer_period));
      }
    }
    if(Version>=26)
      ReadWriteArray(mfp_timer_period);
    else if(LoadOrSave==LS_LOAD) 
    {
      for(int ti=0;ti<4;ti++)
        Mfp.CalcTimerPeriod(ti);
    }
    if(Version>=27)
      ReadWriteArray(mfp_timer_period_change);
    else if(LoadOrSave==LS_LOAD) 
    {
      for(int t=0;t<4;t++) 
        mfp_timer_period_change[t]=0;
    }
    if(Version>=28) 
    {
      int rel_time=0;
      ReadWrite(rel_time);
    }
    if(Version>=29) 
    {
      ReadWrite(emudetect_called);
#if !defined(SSE_NO_FALCONMODE)
      if(LoadOrSave==LS_LOAD) 
        emudetect_init();
#endif
    }
    if(Version>=30) 
    {
      ReadWrite(Microwire.Mask);
      ReadWrite(Microwire.Data);
      ReadWriteByteAsInt(Microwire.volume);
      ReadWriteByteAsInt(Microwire.volume_l);
      ReadWriteByteAsInt(Microwire.volume_r);
      ReadWriteByteAsInt(Microwire.top_val_l);
      ReadWriteByteAsInt(Microwire.top_val_r);
      ReadWriteByteAsInt(Microwire.mixer);
    }
    int NumFloppyDrives=DiskMan.nFloppyDrives;
    if(Version>=31)
    {
      if(Version==60) // bug in v400-401
      {
        BYTE as_saved;
        ReadWrite(as_saved);
        NumFloppyDrives=as_saved;
      }
      else
        ReadWrite(NumFloppyDrives);
    }
    NumFloppyDrives=MIN(2,NumFloppyDrives);
    //ASSERT(NumFloppyDrives==1||NumFloppyDrives==2);
    bool spin_up=(fdc_spinning_up>0);
    if(Version>=32) 
      ReadWrite(spin_up);
    if(LoadOrSave==LS_LOAD)
      fdc_spinning_up=0x99;
    if(Version>=33) 
      ReadWriteByteAsInt(fdc_spinning_up);
    else if(LoadOrSave==LS_LOAD) 
      fdc_spinning_up=spin_up;
    if(Version>=34) 
      ReadWrite(emudetect_write_logs_to_printer);
    if(Version>=35) 
    {
      ReadWrite(psg_reg_data);
      ReadWriteByteAsInt(Fdc.StatusType); // floppy_type1_command_active
      ReadWriteByteAsInt(dummy_byte); //fdc_read_address_buffer_len
      BYTE fdc_read_address_buffer_fake[20];
      ReadWriteArray(fdc_read_address_buffer_fake);
      ReadWriteWordAsInt(Dma.ByteCount);
    }
    if(Version>=36) 
    {
      struct TAgenda temp_agenda[MAX_AGENDA_LENGTH];
      int temp_agenda_length=agenda_length;
      for(int i=0;i<agenda_length;i++) 
        temp_agenda[i]=agenda[i];
      if(LoadOrSave==LS_SAVE) 
      {
        // Convert vectors to indexes and hbl_counts to relative
        for(int i=0;i<temp_agenda_length;i++) 
        {
#if defined(SSE_X64)
          INT64 l=0;
          while((DWORD_PTR)(agenda_list[l])!=1) {
            if(temp_agenda[i].perform==agenda_list[l]) {
              temp_agenda[i].perform=(LPAGENDAPROC)l;
              break;
            }
            l++;
          }
          if(DWORD_PTR(agenda_list[l])==1) temp_agenda[i].perform=(LPAGENDAPROC)-1;
#else
          int l=0;
          while((DWORD)agenda_list[l]!=1) {
            if(temp_agenda[i].perform==agenda_list[l]) {
              temp_agenda[i].perform=(LPAGENDAPROC)l;
              break;
            }
            l++;
          }
          if((DWORD)agenda_list[l]==1)
            temp_agenda[i].perform=(LPAGENDAPROC)-1;
#endif
          temp_agenda[i].time-=hbl_count;
        }
      }
      ReadWrite(temp_agenda_length);
      for(int i=0;i<temp_agenda_length;i++)
        ReadWriteStruct(temp_agenda[i]);
      if(LoadOrSave==LS_LOAD) 
      {
        int list_len=0;
#if defined(SSE_X64)
        while(DWORD_PTR(agenda_list[++list_len])!=1);
        for(int i=0;i<temp_agenda_length;i++) {
          INT64 idx=(INT64)temp_agenda[i].perform;
          if(idx>=list_len||idx<0)
            temp_agenda[i].perform=NULL;
          else
            temp_agenda[i].perform=agenda_list[idx];
        }
#else
        while((DWORD)agenda_list[++list_len]!=1);
        for(int i=0;i<temp_agenda_length;i++) {
          int idx=int(temp_agenda[i].perform);
          if(idx>=list_len||idx<0)
            temp_agenda[i].perform=NULL;
          else
            temp_agenda[i].perform=agenda_list[idx];
        }
#endif
        agenda_length=(WORD)temp_agenda_length;
        for(int i=0;i<agenda_length;i++) 
          agenda[i]=temp_agenda[i];
        agenda_next_time=0xffffffff;
        if(agenda_length) 
          agenda_next_time=agenda[agenda_length-1].time;
      }
    }
    if(Version>=37) 
    {
      ReadWrite(Stemdos.bInterceptDateTime);
    }
    if(Version>=38) 
    {
#if defined(SSE_NO_FALCONMODE)
      ReadWrite(dummy_byte);
      ReadWrite(dummy_byte);
#else
      ReadWrite(emudetect_falcon_mode);
      ReadWrite(emudetect_falcon_mode_size);
#endif
      DWORD l=256;
      if(emudetect_called==0) 
        l=0;
      ReadWrite(l);
#if defined(SSE_NO_FALCONMODE)
      DWORD dw_dummy=0;
      for(DWORD n=0;n<l;n++)
        ReadWrite(dw_dummy);
#else
      for(DWORD n=0;n<l;n++)
        ReadWrite(emudetect_falcon_stpal[n]);
#endif

    }
    if(Version>=39) 
    {
      ReadWriteArray(Shifter.SoundFifo);
      ReadWriteByteAsInt(Shifter.SoundFifoIdx);
    }
#if !defined(SSE_LIBRETRO)
    BYTE *pasti_block=NULL;
    DWORD pasti_block_len=0;
#if USE_PASTI
    bool pasti_old_active=pasti_active;
#endif
    if(Version>=40) 
    {
#if USE_PASTI==0
      bool pasti_active=false;
#endif
      ReadWrite(pasti_active);
#if USE_PASTI
      if(hPasti==NULL) 
        pasti_active=false;
#endif
      if(LoadOrSave==LS_SAVE) 
      {
        //ask Pasti for variable block, save length as a long, followed by block
#if USE_PASTI
        if(hPasti&&(pasti_active
          ||FloppyDrive[DRIVE_A].ImageType.Manager==MNGR_PASTI
          ||FloppyDrive[DRIVE_B].ImageType.Manager==MNGR_PASTI))
        {
#if 1
          pastiSTATEINFO psi;
          psi.bufSize=0;
          psi.buffer=NULL;
          psi.cycles=a_s_t/TICKS8;
          pasti->SaveState(&psi);
          if(psi.bufSize) // for unknown reasons, this tends to be 0 when drive used
          {
            BYTE *buf=new BYTE[psi.bufSize];
            psi.buffer=(void*)buf;
            pasti->SaveState(&psi);
            TRACEB("Pasti SaveState %d bytes\n",psi.bufSize);
            ReadWriteVar(buf,psi.bufSize,fp,LS_SAVE,1,Version);
            delete[] buf;
          }
          else
          {
            ReadWrite(psi.bufSize); //0
          }
#else
          DWORD l=0;
          pastiSTATEINFO psi;
          psi.bufSize=0;
          psi.buffer=NULL;
          psi.cycles=ABSOLUTE_SYS_TIME/TICKS8;
          pasti->SaveState(&psi);
          l=psi.bufSize;
          BYTE*buf=new BYTE[l];
          psi.buffer=(void*)buf;
          if(pasti->SaveState(&psi))
            ReadWriteVar(buf,l,fp,LS_SAVE,1,Version);
          else 
          {
            l=0;
            ReadWrite(l);
          }
          delete[]buf;
#endif
        }
        else
#endif
        {
          DWORD l=0;
          ReadWrite(l);
        }
      }
      else 
      { //load
       //read in length, read in block, pass it to pasti.
        ReadWrite(pasti_block_len);
        if(pasti_block_len) 
        { //something to load in
          TRACEB("Pasti LoadState %d bytes\n",pasti_block_len);
          if(pasti_block_len<1024*1024) // avoid bad crash
          {
            pasti_block=new BYTE[pasti_block_len];
            FREAD(pasti_block,1,pasti_block_len,fp);
#if USE_PASTI
            if(hPasti==NULL)
#endif
            {
              delete[] pasti_block;
              pasti_block=NULL;
            }
          }
        }
      }
    }
    else 
    {
#if USE_PASTI
      pasti_active=false;
#endif
    }
#endif//#if !defined(SSE_LIBRETRO)
    if(Version>=41) // Steem 3.3
    {
      BYTE st=ST_MODEL;
      ReadWrite(st);
      if(LoadOrSave==LS_LOAD)
      {
        SSEConfig.SwitchSTModel(st);
      }
      ReadWrite(dummy_int); //dummy for former Program ID
    }
#if SSE_VERSION>=340
    if(Version>=42) // Steem 3.4
    {
      ReadWrite(SampleRate); // global of 3rd party
      SampleRate=sound_freq;
      ReadWriteByteAsInt(Microwire.bass);
      if(Microwire.bass>=0xC)
        Microwire.bass=6;
      ReadWriteByteAsInt(Microwire.treble);
      if(Microwire.treble>=0xC)
        Microwire.treble=6;
#if defined(SSE_HD6301_LL)
/*  If it must work with ReadWrite, we must use a variable that
    can be used with sizeof, so we take on the stack.
*/
      BYTE buffer_for_hd6301[200];
      if(LoadOrSave==LS_SAVE) // 1=save
      {
        if(HD6301_OK)
          hd6301_load_save(LoadOrSave,buffer_for_hd6301);
        ReadWrite(buffer_for_hd6301); // ReadWriteArray would have been better
      }
      else // 0=load
      {
        ReadWrite(buffer_for_hd6301);
        if(HD6301_OK)
          hd6301_load_save(LoadOrSave,buffer_for_hd6301);
      }
#endif
    }
    else
    {
#if defined(SSE_HD6301_LL) && !defined(SSE_NO_OLD_IKBD)
      OPTION_C1=0;
#endif
    }
#endif
    //3.5.0: nothing special
    if(Version>=44) // Steem 3.5.1
    {
      ReadWriteStruct(Shifter); // for res & sync
      if(LoadOrSave==LS_LOAD)
      {
        if(Version<60) //4.0
          OPTION_SHIFTER_WU=SHIFTER_DEFAULT_WAKEUP;
      }
#if defined(SSE_HD6301_LL)
      WORD HD6301EMU_ON_tmp=OPTION_C1;
      ReadWrite(HD6301EMU_ON_tmp);
#if !defined(SSE_NO_OLD_IKBD)
      OPTION_C1=(HD6301EMU_ON_tmp!=0);
      if(!HD6301_OK)
        OPTION_C1=0;
#endif
#endif
      ReadWriteStruct(acia[ACIA_MIDI]);
      ReadWriteStruct(Dma); // variables already written
    }
    else
    {
      acia[ACIA_MIDI].cr=0x95; // usually
      acia[ACIA_MIDI].sr=2; // usual
    }
    if(LoadOrSave==LS_LOAD)
    {
      Glue.ShiftMode=Shifter.ShiftMode=screen_res;
      Glue.SyncMode=(Glue.VideoFreq==PAL_HZ) ? BIT_1 : 0;
    }
    if(Version>=44)
    {
      int magic=123456;
      ReadWrite(magic); // Stupid!
      //ASSERT(magic==123456);
    }
    if(Version>=45) //3.5.2
    {
      struct oldTSF314 {
        BYTE Id;
        BYTE ImageType;
        BYTE MotorOn;
      } oldSF314[2];
      for(BYTE d=0;d<2;d++)
      {
        oldSF314[d].Id=d;
        ReadWriteStruct(oldSF314[d]);
        if(LoadOrSave==LS_LOAD)
        {
          FloppyDrive[d].bMotor=(oldSF314[d].MotorOn!=0);
          FloppyDrive[d].UpdateAdat(d==DRIVE_B);
        }
      }
    }
    if(Version>=46) // 3.5.4
    {
      ReadWrite(OPTION_WS);
    }
    if(Version>=49) // 3.7.0
    {
#if defined(SSE_DISK_CAPS)
      // This just restore registers, not internal state. TODO
      if(LoadOrSave==LS_LOAD && CAPSIMG_OK && FloppyDrive[DRIVE_A].ImageType.Manager==MNGR_CAPS)
      {
        Caps.WritePsgA(psg_reg[PSGR_PORT_A]);
        Caps.fdc.r_command=Fdc.cr;
        Caps.fdc.r_track=Fdc.tr;
        Caps.fdc.r_sector=Fdc.sr;
        Caps.fdc.r_data=Fdc.dr;
        Caps.Drive[DRIVE_A].track=FloppyDrive[DRIVE_A].track;
        Caps.Drive[DRIVE_B].track=FloppyDrive[DRIVE_B].track;
      }
#endif
      WORD *tmp=Psg.p_fixed_vol_3voices;
#if defined(SSE_YM2149_LL)
      Filter *tmp2=Psg.AntiAlias;
#endif
      ReadWriteStruct(Psg);
      Psg.p_fixed_vol_3voices=tmp;
      Psg.SelectedDrive=(floppy_current_drive()==DRIVE_B);
      Psg.SelectedSide=(floppy_current_side()==1);
#if defined(SSE_YM2149_LL)
      if(Version<56 && LoadOrSave==LS_LOAD)
        Psg.Reset(); //restore sane values
      Psg.AntiAlias=tmp2;
#endif
      ReadWriteStruct(Mfp);
      Mfp.Restore();
    }//3.7.0
    if(Version>=50) // 3.7.1
    {
      ReadWriteStruct(Fdc); // it includes cr, str... again
    }
    if(Version>=52) //380
    {
      // handle v394 snapshots
      if(LoadOrSave==LS_LOAD && Version<60) //v380->394
      {
        struct THD6301_394 {
          enum  {
            CUSTOM_PROGRAM_NONE,
            CUSTOM_PROGRAM_LOADING,
            CUSTOM_PROGRAM_LOADED,
            CUSTOM_PROGRAM_RUNNING
          }custom_program_tag;
          COUNTER_VAR ChipCycles,MouseNextTickX,MouseNextTickY;
          int MouseCyclesPerTickX, MouseCyclesPerTickY;
          short MouseVblDeltaX;
          short MouseVblDeltaY;
          BYTE Initialised;
          BYTE Crashed;
          BYTE click_x,click_y;
          BYTE rdr,rdrs,tdr,tdrs; 
        };
        THD6301_394 ikbd;
        ReadWriteStruct(ikbd);
        Ikbd.custom_program_tag=(THD6301::EProgramTag)ikbd.custom_program_tag;
        Ikbd.ChipCycles=ikbd.ChipCycles;
        Ikbd.MouseNextTickX=ikbd.MouseNextTickX;
        Ikbd.MouseNextTickY=ikbd.MouseNextTickY;
        Ikbd.MouseCyclesPerTickX=ikbd.MouseCyclesPerTickX;
        Ikbd.MouseCyclesPerTickY=ikbd.MouseCyclesPerTickY;
        Ikbd.MouseVblDeltaX=ikbd.MouseVblDeltaX;
        Ikbd.MouseVblDeltaY=ikbd.MouseVblDeltaY;
        Ikbd.Initialised=ikbd.Initialised;
        Ikbd.Crashed=ikbd.Crashed;
        Ikbd.click_x=ikbd.click_x;
        Ikbd.click_y=ikbd.click_y;
        Ikbd.rdr=ikbd.rdr;
        Ikbd.rdrs=ikbd.rdrs;
        Ikbd.tdr=ikbd.tdr;
        Ikbd.tdrs=ikbd.tdrs; 
      }
      else
      {
        struct Tdummy {
          BYTE dummy[44]; // former HD6301
        } mydummy;
        ZeroMemory(&mydummy,sizeof(Tdummy));
        ReadWriteStruct(mydummy); // registers... 
      }
    }
    if(Version>=53) //382
    { 
      // didn't work OK, we lose eclock sync on load
      ReadWrite(dummy_int);
      ReadWrite(dummy_int);
    }
    //390
//    int NewROMCountry=Tos.DefaultCountry;
    int NewROMCountry=0;
    if(Version>=54) //390
    {
      if(LoadOrSave==LS_SAVE)
        NewROMCountry=ROM_PEEK(0x1D);
      ReadWrite(NewROMCountry);
    }
    Blitter.BlitCycles=0;
    if(Version>=56) //392
    {
#if defined(SSE_MMU_MONSTER_ALT_RAM)
      ReadWrite(Mmu.MonSTerHimem);
#else
      ReadWrite(dummy_int);
#endif
    }
    if(Version>=57) //393
    {
      ReadWrite(SSEConfig.OverscanOn);
    }
    if(Version>=59) //395-400
    {
#if defined(SSE_MEGA)
      ReadWriteStruct(MegaRtc);
#endif
      ReadWrite(Glue.gamecart);
      WORD dummy=0;
      ReadWrite(dummy); // former SSEOptions.SingleSideDriveMap, SSEOptions.FreebootDriveMap
#ifdef WIN32
      if(LoadOrSave==LS_LOAD && DiskMan.IsVisible())
      {
        InvalidateRect(GetDlgItem(DiskMan.Handle,98),NULL,FALSE);
        InvalidateRect(GetDlgItem(DiskMan.Handle,99),NULL,FALSE);
      }
#endif      
#if defined(SSE_VID_STVL1)
      // skip the pointers, don't RW scanline rendering memory
      // we also lose dbg_ vars and v402 additions
      const int stvl_skip=6*sizeof(void(*))+4*sizeof(void*);
      ReadWriteVar(&Stvl.PC_pal,sizeof(Stvl)-stvl_skip-sizeof(DWORD)*1024
        -sizeof(int)*5-sizeof(WORD)-sizeof(bool)*2,fp,LoadOrSave,2,Version);
      if(LoadOrSave==LS_LOAD)
        StvlUpdate();
#endif
      if(LoadOrSave==LS_LOAD)
      {
        Glue.Update();
        update_ipl(0);
      }
/*  Now we save the current CPU time too, and assorted variables.
    To improve Resuming snapshot.
*/

      ReadWrite(sys_timer);
      ReadWrite(time_of_event_acia);
      ReadWrite(time_of_last_hbl_interrupt);
      ReadWrite(time_of_last_vbl_interrupt);

      ReadWrite(time_of_next_timer_b);
      //ASSERT(LoadOrSave!=LS_LOAD);
      //COUNTER_VAR x=TimeOfHSyncOff;
      ReadWrite(TimeOfHSyncOff);
      //TimeOfHSyncOff=x;
      //if(LoadOrSave==LS_LOAD) return 0;
      ReadWrite(tvn_latch_time);
#if USE_PASTI==0
      COUNTER_VAR pasti_update_time=0;
#endif
      ReadWrite(pasti_update_time);
      ReadWrite(a_s_t);
      ReadWrite(sys_time_of_first_mfp_tick);
      ReadWriteArray(GlueFreqChangeTime);
      ReadWriteArray(ShifterModeChangeTime);
      {
        // would have been simpler with two arrays to begin with
        int size=sizeof(ipl_timing_time)*2;
        ReadWrite(size);
        for(int i=0;i<256;i++)
        {
          ReadWrite(ipl_timing_time[i]);
          COUNTER_VAR x=ipl_timing_ipl[i];
          ReadWrite(x);
          ipl_timing_ipl[i]=(BYTE)x;
        }
      }
      COUNTER_VAR mfp_time_of_start_of_last_interrupt[16];
      ZeroMemory(mfp_time_of_start_of_last_interrupt,sizeof(COUNTER_VAR)*16);
      ReadWriteArray(mfp_time_of_start_of_last_interrupt);
      ReadWriteArray(glue_freq_change);
      ReadWriteArray(shifter_mode_change);
      ReadWriteArray(mfp_timer_timeout);
      ReadWrite(sys_cycles); // int

#if defined(SSE_TIMINGS32)
      if(Version<62)
      { // no guarantee, at least we try
        sys_timer*=TICKS8;
        time_of_event_acia*=TICKS8;
        time_of_last_hbl_interrupt*=TICKS8;
        time_of_last_vbl_interrupt*=TICKS8;
        time_of_next_timer_b*=TICKS8;
        TimeOfHSyncOff*=TICKS8;
        tvn_latch_time*=TICKS8;
        pasti_update_time*=TICKS8;
        sys_cycles*=TICKS8; // int!
        a_s_t*=TICKS8;
        sys_time_of_first_mfp_tick*=TICKS8;
        for(int i=0;i<NMODECHANGES;i++)
        {
          GlueFreqChangeTime[i]*=TICKS8;
          ShifterModeChangeTime[i]*=TICKS8;
        }
        for(int i=0;i<256;i++)
          ipl_timing_time[i]*=TICKS8;
        for(int i=0;i<16;i++)
          mfp_time_of_start_of_last_interrupt[i]*=TICKS8;
        for(int i=0;i<4;i++)
          mfp_timer_timeout[i]*=TICKS8;
      }
#endif
      ReadWrite(freq_change_this_scanline); // why?
      ReadWrite(GlueFreqChangeIdx);
      ReadWrite(ShifterModeChangeIdx);
      ReadWrite(VideoFreqIdx);
      ReadWrite(ipl_timing_index);
      ReadWriteStruct(Cpu);
      if(Cpu.ProcessingState==TMC68000::HALTED)
        StatusInfo.MessageIndex=TStatusInfo::MC68000_CRASH; // so player knows why it doesn't work
      ReadWriteStruct(Glue);
      Glue.m_Status.stop_emu=0;
      ReadWriteStruct(Mmu);
      for(BYTE drive=DRIVE_A;drive<=DRIVE_B;drive++)
      {
        TSF314 tmpSF314=FloppyDrive[drive];
        ReadWriteStruct(FloppyDrive[drive]); // v402R14, load FloppyDrive again
        FloppyDrive[drive].ImageType=tmpSF314.ImageType;
        FloppyDrive[drive].Restore(drive);   // Restore been restored too
        if(LoadOrSave==LS_LOAD)
          FloppyDrive[drive].bGhost=false;
      }
    }
    else
    {
      CALC_VIDEO_FREQ_IDX; // so we get correct Hz in status bar
    }//v4.0
    Glue.PreviousVideoFreq=Glue.Freq[VideoFreqIdx];
    if(Version>=61) //402
    {
      ReadWrite(sys_time_of_last_vbl); // forgotten, causing delay in ll YM emu
      if(Version>=62)
      {
        ReadWrite(time_of_next_event);
        ReadWrite(FloppyDrive[DRIVE_A].time_of_next_ip); // useless now
        ReadWrite(FloppyDrive[DRIVE_B].time_of_next_ip);
#ifdef DEBUG_BUILD
        ReadWrite(debug_run_until_val);
#else // compatibility debugger/regular
        COUNTER_VAR dummy=0;
        ReadWrite(dummy);
#endif
      }
    }
    else
    {
      sys_time_of_last_vbl=time_of_last_vbl_interrupt-64; // not far
    }
    if(Version<62)
    {
      time_of_next_event=A_S_T+EIGHT_MILLION; //1 sec off
      FloppyDrive[DRIVE_A].time_of_next_ip=time_of_next_event;
      FloppyDrive[DRIVE_B].time_of_next_ip=time_of_next_event;
#ifdef DEBUG_BUILD
      debug_run_until_val=time_of_next_event;
#endif
    }
    if(Version>=63) //410
    {
#if defined(SSE_MEGASTE)
      ReadWriteStruct(MegaSte);
#endif
#if defined(SSE_MEGA)
      MEM_ADDRESS *pAdlist=Cpu16.pAdlist; // save pointers
      bool *pIsCached=Cpu16.pIsCached;
      ReadWriteStruct(Cpu16);
      Cpu16.pAdlist=pAdlist;
      Cpu16.pIsCached=pIsCached;
#endif
#ifndef DISABLE_STEMDOS
#define LOGSECTION LOGSECTION_GEMDOS
      // save and restore open HD files
      int nFiles=Stemdos.AnyFilesOpen();
      ReadWrite(nFiles); // if 0, nothing else to R/W
      BYTE myh=GEMDOS_STD_HANDLES; // GEMDOS file handle
      for(int i=0;i<nFiles;i++)
      {
        INT64 pos=0;
        // find handle
        if(LoadOrSave==LS_LOAD)
        {
          ReadWrite(myh); // read handle
          //ASSERT(myh>5);
          StemdosFile[myh].h=myh;
        }
        else
        {
          for(BYTE j=myh;j<MAX_STEMDOS_FILES;j++)
          {
            if(StemdosFile[j].open)
            {
              myh=j; // found
              ReadWrite(StemdosFile[myh].h); // write handle
              break;
            }
          }
          pos=FTELL(StemdosFile[myh].fp);
        }
        // R/W the rest of struct + position in file
        ReadWrite(StemdosFile[myh].attrib);
        ReadWrite(StemdosFile[myh].date);
        ReadWrite(StemdosFile[myh].time);
        ReadWrite(StemdosFile[myh].open);
        //ASSERT(StemdosFile[myh].open);
        ReadWrite(StemdosFile[myh].Pexec);
        ReadWrite(pos); // position in file
        ReadWriteStr(StemdosFile[myh].filename);
        TRACE_LOG("STEMDOS %d %s %d\n",myh,CHECKPATH(StemdosFile[myh].filename.Text),pos);
        if(LoadOrSave==LS_LOAD && StemdosFile[myh].open)
        { // try to reopen file
          char *pc_mode="r+b"; // same as in hd_gemdos
          if(StemdosComlineReadRb && StemdosFile[myh].attrib==0) 
            pc_mode="rb";
          if((StemdosFile[myh].fp=fopen(StemdosFile[myh].filename,pc_mode))!=NULL)
            FSEEK(StemdosFile[myh].fp,pos,SEEK_SET);
          else
            StemdosFile[myh].open=false;
        }
        myh++; // when saving
      }//nxt i
#endif
#if defined(SSE_ACSI)
      ReadWrite(acsi_dev);
      // save and restore ACSI image file positions
      // note: limited to 32bit! (legacy) but those positions don't matter so much, we
      // expect the program to always specify the position of the sectors it
      // wants, right?
      for(int i=0;i<MAX_ACSI_DEVICES;i++) 
      {
        long pos;
        if(LoadOrSave==LS_SAVE)
        {
          pos=(AcsiHdc[i].fpAcsiImg!=NULL) ? (long)FTELL(AcsiHdc[i].fpAcsiImg) : 0;
          ReadWrite(pos);
        }
        else
        {
          ReadWrite(pos);
          if(AcsiHdc[i].fpAcsiImg!=NULL)
            FSEEK(AcsiHdc[i].fpAcsiImg,pos,SEEK_SET);
        }
        ReadWrite(AcsiHdc[i].block);
        ReadWrite(AcsiHdc[i].cmd_ctr);
        ReadWriteArray(AcsiHdc[i].cmd_block);
        ReadWrite(AcsiHdc[i].DR);
        ReadWrite(AcsiHdc[i].STR);
        ReadWrite(AcsiHdc[i].error_code);
      }//nxt i
#endif
#if defined(SSE_ACSI_LASER)
      ReadWriteStruct(Laser);
#endif
      ReadWriteStruct(Dma); // for the FIFO
#if defined(SSE_DISK_SCP)
      for(int d=0;d<2;d++)
      {
        ReadWrite(ImageSCP[d].rev);
        if(FloppyDrive[d].MfmManager && !FloppyDrive[d].Empty()
          && FloppyDrive[d].ImageType.Manager==MNGR_WD1772)
        {
          if(LoadOrSave==LS_LOAD && FloppyDrive[d].ImageType.Extension==EXT_SCP)
          { // SCP: try to load the correct rev
            bool reload=!!ImageSCP[d].rev;
            if(reload)
              ImageSCP[d].rev--;
            FloppyDrive[d].MfmManager->LoadTrack(CURRENT_SIDE,FloppyDrive[d].track,reload);
          }
          ReadWrite(FloppyDrive[d].MfmManager->Position);
        }
      }//nxt
#endif
      ReadWrite(fdc_irq);
      ReadWrite(hdc_irq);
      ReadWrite(SSEConfig.MouseAd);
#if defined(SSE_STATS)
      ReadWrite(StatsStatic.nPatchedBytes);
#endif
    }//if(Version>=63) //410
    if(Version>=64) //411
    {
#if defined(SSE_ACSI)
      for(int i=0;i<MAX_ACSI_DEVICES;i++) 
      {
        ReadWrite(AcsiHdc[i].Ready);
        ReadWrite(AcsiHdc[i].n_cmd_bytes);
        ReadWrite(AcsiHdc[i].block_count);
      }
#endif
    }//if(Version>=64) //411
    if(Version>=65) //412
    {
      ReadWrite(FloppyDisk[DRIVE_A].WriteProtect);
      ReadWrite(FloppyDisk[DRIVE_B].WriteProtect);
    }
    if(Version>=66) //420
    {
#if defined(SSE_CARTRIDGE_ACTIVE)
      ReadWriteArray(CartridgeData);
#endif
    }
    // End of data, seek to compressed memory
    if(Version>=11) 
    {
#ifndef ONEGAME
#if defined(SSE_LIBRETRO)
      if(LoadOrSave==LS_SAVE)
      {
        StartOfData=(DWORD)(fp-pStartByte);
        fp=pStartByte+StartOfDataPos;
        ReadWrite(StartOfData);
        fp=pStartByte+StartOfData;
      }
      *pVerRet=StartOfData;
#else
      if(LoadOrSave==LS_SAVE) 
      {
        StartOfData=(DWORD)FTELL(fp);
        FSEEK(fp,StartOfDataPos,SEEK_SET);
        ReadWrite(StartOfData);
      }
      // Seek to start of compressed data (this was loaded earlier if LS_LOAD)
      FSEEK(fp,StartOfData,SEEK_SET);
#endif
#else
      f=pStartByte+StartOfData;
#endif
    }
    if(LoadOrSave==LS_SAVE) 
      return ERR_OK;
    init_screen();
    if(old_em||extended_monitor)
    {
      if(FullScreen)
        change_fullscreen_display_mode(true);
      else
        Disp.ScreenChange(); // For extended monitor
    }
#ifndef ONEGAME
#if !defined(SSE_LIBRETRO)
    if(ChangeTOS) 
      LoadSnapShotChangeTOS(NewROM,NewROMVer,NewROMCountry);
    if(ChangeDisks) 
      LoadSnapShotChangeDisks(NewDisk,NewDiskInZip,NewDiskName);
    if(ChangeCart) 
      LoadSnapShotChangeCart(NewCart);
#endif
    DiskMan.SetNumFloppies((BYTE)NumFloppyDrives);
#endif
#if USE_PASTI
    if(hPasti && pasti_block) 
    {
      pastiSTATEINFO psi;
      psi.bufSize=pasti_block_len;
      psi.buffer=pasti_block;
      psi.cycles=a_s_t/TICKS8;
      pasti->LoadState(&psi);
      DiskEmu.LastManager=MNGR_PASTI;
      DiskEmu.Update(2);
    }
    if(pasti_active!=pasti_old_active) 
      LoadSavePastiActiveChange();
#endif


#ifdef WIN32
    if(FullScreen)
      InvalidateRect(StemWin,NULL,FALSE); // erase pic we just drew...
#endif      

  }
  catch(int error) {
#ifndef BCC_BUILD //why?
    TRACE("%s %d %d\n","ERROR",error,LoadOrSave);
    return error;
#else
    return -777;
#endif
  }
  catch(...) {
    TRACE("%s\n",STEEM_CRASH_TXT);
    return -1;
  }
  return ERR_OK;
}
#undef ReadWrite
#undef ReadWritePtr
#undef ReadWriteStruct
#undef ReadWriteByteAsInt
#undef ReadWriteWordAsInt
#undef ReadWriteArray
#undef ReadWriteStr


void LoadSnapShotUpdateVars(int Version) { // only called by  LoadSnapShot()
  SET_PC(pc);
  if(Version>=59) //395-400
  {
    // some necessary parts of init_timings()
    ScreenResAtStartOfVbl=screen_res;
    VideoFreqAtStartOfVbl=Glue.VideoFreq;
    CyclesPerScanlineAtStartOfVbl=CyclesPerScanline[VideoFreqIdx];
    Mfp.CalcInterruptsEnabled();
    Mfp.CalcTimersEnabled();
    prepare_next_event();

    //scanline_drawn_so_far=0;
  }
  else
    init_timings();
  UpdateSTKeys();
  if(Version<36)
  {
    // No agendas saved
    if(Ikbd.resetting) 
      ikbd_reset(false);
    if(Ikbd.mouse_mode==IKBD_MOUSE_MODE_OFF) 
      Ikbd.port_0_joy=TRUE;
    if(keyboard_buffer_length) 
      agenda_add(agenda_keyboard_replace,ACIAClockToHBLS(acia[ACIA_IKBD].clock_divide)+1,false);
    if(MIDIPort.AreBytesToCome()) 
      agenda_add(agenda_midi_replace,ACIAClockToHBLS(acia[ACIA_MIDI].clock_divide,true)+1,false);
    if(fdc_spinning_up)
      agenda_add(agenda_fdc_spun_up,milliseconds_to_hbl(40),fdc_spinning_up==2);
    if(acia[ACIA_MIDI].tx_flag) 
      agenda_add(agenda_acia_tx_delay_MIDI,2,0);
    if(acia[ACIA_IKBD].tx_flag) 
      agenda_add(agenda_acia_tx_delay_IKBD,2,0);
  }
#if USE_PASTI
  if(hPasti && FloppyDrive[DRIVE].ImageType.Manager==MNGR_PASTI)
  {
    pastiPEEKINFO ppi;
    pasti->Peek(&ppi);
    if(ppi.intrqState)
      Mfp.reg[MFPR_GPIP]&=~(MFP_GPIP_FDC_MASK);
    else
      Mfp.reg[MFPR_GPIP]|=MFP_GPIP_FDC_MASK;
    pasti_motor_proc(ppi.motorOn);
  }
#endif
  SteSoundOutputCountdown=SteSoundSamplesCountdown=0;
  SteSoundChannelBufIdx=0;
  prepare_next_event();
  snapshot_loaded=1;
  res_change();
  palette_convert_all();
#if !defined(SSE_VID_LS) && !defined(SSE_LIBRETRONUKE)
  if(runstate==RUNSTATE_STOPPED)
    draw(false);
#endif
  for(int n=0;n<PAL_SIZE;n++)
  {
    PAL_DPEEK(n*2)=STpal[n];
  }
}

#undef LOGSECTION
