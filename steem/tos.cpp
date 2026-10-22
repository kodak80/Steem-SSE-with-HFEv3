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

DOMAIN: OS
FILE: tos.cpp
DESCRIPTION: TOS (The Operating System) utilities, loading TOS images,
also cartridge utilities
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <computer.h>
#include <translate.h>
#include <notifyinit.h>
#include <iolist.h>

/*
#define SVmemvalid 0x420
#define SVmemctrl 0x424
#define SV_membot 0x432
#define SVmemval2 0x43a
#define SVsshiftmd 0x44c
*/

#define SV_MEMTOP 0x436
#define SV_SCREENPT 0x45e
#define SV_V_BAS_AD 0x44e

// BIOS Functions by Opcode
//#define GETMPB   0x00 //Return the address of the MPB (Memory Parameter Block) structure
//#define BCONSTAT 0x01 //Determine if a character is waiting from a device
//#define BCONIN   0x02 //Input a character from a device
#define BCONOUT  0x03 //Output a character from a device
//#define RWABS    0x04 //Read/write sectors to a device
//#define SETEXC   0x05 //Set or read a system exception vector
//#define TICKAL   0x06 //Return the current system timer calibration
#define GETBPB   0x07 //Return the address of the BPB (BIOS Parameter Block)
//#define BCOSTAT  0x08 //Determine if a device is ready to receive a character
#define MEDIACH  0x09 //Determine if a drive's media has been changed
#define DRVMAP   0x0A //Return a bitmap of mounted drives
//#define KBSHIFT  0x0B //Return the state of the keyboard shift keys

// XBIOS Functions by Opcode
#define RANDOM   0x11 //Return a random number
#define GETTIME  0x17 //Get the time of day and current date
#if defined(SSE_GEM_CONTROL_PANEL)
#define KBRATE   0x23
#endif
#define VSYNC    0x25 //Hold the process until the next vertical blank

BYTE *STRom=NULL;  
#ifndef BIG_ENDIAN_PROCESSOR
BYTE *Rom_End,*Rom_End_minus_1,*Rom_End_minus_2,*Rom_End_minus_4;
#endif
BYTE *cart=NULL,*cart_save=NULL;
#ifndef BIG_ENDIAN_PROCESSOR
BYTE *Cart_End_minus_1,*Cart_End_minus_2,*Cart_End_minus_4;
#endif
DWORD tos_len=0;
MEM_ADDRESS rom_addr=0,rom_addr_end=0;
// init at first trap:
MEM_ADDRESS os_gemdos_vector=0,os_bios_vector=0,os_xbios_vector=0;
WORD tos_version;
bool tos_high;
int aes_calls_since_reset=0;


void intercept_gemdos(),intercept_bios(),intercept_xbios();

#if defined(SSE_ENABLE_TRACE_LOG)
void log_os_call(int trap);
#endif

// enumerate TOS files for the GUI
EasyStr TTos::GetNextTos(DirSearch &ds) {
  EasyStr Path;
  if((ds.Attrib & (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_HIDDEN))==0)
  {
    Path=UsersPath+SLASH+ds.Name;
#ifdef WIN32
    if(has_extension(Path,"LNK")) // links are hosted in UsersPath, so TOS files can be anywhere
    {
      WIN32_FIND_DATA wfd;
      EasyStr DestPath=GetLinkDest(Path,&wfd);
      if(has_extension_list(DestPath,"IMG","ROM",NULL))
        if(Exists(DestPath)) 
          Path=DestPath;
    }
#endif
#ifdef UNIX
    char LinkPath[MAX_PATH+1];
    memset(LinkPath,0,MAX_PATH+1);
    if(readlink(Path,LinkPath,MAX_PATH)>0)
    {
      if(has_extension_list(LinkPath,"IMG","ROM",NULL))
      {
        if(Exists(LinkPath))
          Path=LinkPath;
        else
          Path="";
      }
    }
#endif
  }
  return Path;
}


void TTos::GetTosProperties(EasyStr Path,WORD &Ver,BYTE &Country,WORD &Date,BYTE &Recognised) {
  FILE *fp=fopen(Path,"rb");
  if(fp!=NULL)
  {
    DWORD Len=(DWORD)GetFileLength(fp);
    if(Len<0x1f)
      return;
    BYTE *buf=new BYTE[Len];
    FREAD(buf,1,Len,fp);
    Ver=*(WORD*)&buf[2];
    SWAP_BIG_ENDIAN_WORD(Ver);
#if defined(SSE_GUI_QUICKSETTINGS)
    if(!strncmp((char*)&buf[0x2c],"ETOS",4))
      Ver=ETOS_FAKE_VERSION;
#endif
    Country=buf[0x1d];
    Date=*(WORD*)&buf[0x1e];
    SWAP_BIG_ENDIAN_WORD(Date);
    DWORD checksum=~0u;
    add_to_crc32(checksum,buf,Len);
    checksum=~checksum;
    switch(checksum) {
    case 0x1A586C64: // 1.0 UK
    case 0x3B5CD0C5: // 1.02 UK
    case 0xA50D1D43: // 1.04 UK
    case 0xD72FEA29: // 1.06 UK
    case 0x1C1A4EBA: // 1.62 UK
    case 0x90822603: // 2.05 UK
    case 0x08538E39: // 2.06 UK
    case 0xD13FA742: // EmuTOS 192K 1.0 //update?
    case 0x6FAFB37A: // EmuTOS 256K 1.0
    // ... could add some other countries - but nobody certified any TOS they use
      Recognised=1; // will display in green
      break;
    case 0xD1C6F2FA: // 1.62 UK
    case 0x0296915D: // HD6301V1ST.img
      Recognised=0xff; // bad in red
      break;
    default:
      Recognised=(Len%1024) ? 0xff : 0; // bad if not multiple of 1024
    }
    delete [] buf;
    TRACE_SECTION(LOGSECTION_INIT,"TOS %03X country %X %02d/%02d/%04d %s %08X path %s\n",Ver,
      Country,(Date&0x1F),(Date>>5)&0xF,(Date>>9)+1980,"CRC32",checksum,GetFileNameFromPath(Path.Text));
    //TRACE_INIT("TOS %03X country %X date %04X CRC32 %08X path %s\n",Ver,Country,
      //Date,checksum,Path.Text);
    fclose(fp);
  }
}


#if defined(SSE_TOS_KEYBOARD_CLICK)

void TTos::CheckKeyboardClick() {
  if(LPEEK(0x44E)==vbase) // not perfect, at least it's a check //v402 placed here //v420 Windows: replaced
  {
    if(OPTION_KEYBOARD_CLICK)
      PEEK(0x484)|=0x01;
    else
      PEEK(0x484)&=0xFE;
  }
}

#endif


#ifndef NO_CRAZY_MONITOR

void TTos::HackMemoryForExtendedMonitor() {
  TRACE_SECTION(LOGSECTION_INIT,"EM mem_len %X vbase %X phystop %X _memtop %X\n",mem_len,vbase,
    LPEEK(SV_PHYSTOP),LPEEK(SV_MEMTOP));
  MEM_ADDRESS bytes_needed=MAX((int)((em_width*em_height*em_planes)/8),0x8000);
  //ASSERT(bytes_needed>0x8000);
  MEM_ADDRESS xbios2a=mem_len-(bytes_needed+256);
  vbase=(xbios2a+255)&-256;
  //ASSERT(vbase+bytes_needed<=mem_len);
  SafeLPoke(SV_MEMTOP,vbase); //_memtop
  SafeLPoke(SV_V_BAS_AD,vbase);
  SafeLPoke(SV_SCREENPT,vbase);
  if(em_planes==1)
    Mfp.reg[MFPR_GPIP]|=MFP_GPIP_MONO_MASK;
  TRACE_SECTION(LOGSECTION_INIT,"EM bytes_needed %d vbase %X phystop %X _memtop %X\n",bytes_needed,
    vbase,LPEEK(SV_PHYSTOP),LPEEK(SV_MEMTOP));
}

#endif


#define LOGSECTION LOGSECTION_TRAP

#define INVALID_SP (mem_len-128)

// This always returns a valid address
MEM_ADDRESS GetSPBeforeTrap(bool* const pInvalid) {
  if(pInvalid!=NULL)
    *pInvalid=false;
  MEM_ADDRESS my_sp=(SP&0xffffff)+6;
  // my_sp now points to first byte after the exception stack frame
  if(my_sp<mem_len)
  {
    // First byte on stack is high byte of sr
    if((SafePeek(SP&0xffffff) & BIT_5)==0)
      // Supervisor bit not set in stacked sr
      my_sp=other_sp&0xffffff;
  }
  if(my_sp>=mem_len)
  {
    if(pInvalid!=NULL)
      *pInvalid=true;
    return INVALID_SP;
  }
  return my_sp;
}

#undef INVALID_SP

void intercept_os() {
  ioaccess&=~IOACCESS_INTERCEPT_OS;
  if(LITTLE_PC==os_gemdos_vector)
  {
    intercept_gemdos();
    ioaccess|=IOACCESS_INTERCEPT_OS;
  }
  else if(LITTLE_PC==os_bios_vector)
  {
    intercept_bios();
    ioaccess|=IOACCESS_INTERCEPT_OS;
  }
  else if(LITTLE_PC==os_xbios_vector)
  {
    intercept_xbios();
    ioaccess|=IOACCESS_INTERCEPT_OS;
  }
  else if(IRD==0x4E42)
  {
#if defined(SSE_ENABLE_TRACE_LOG)
    if(TRACE_LOG_ENABLED(LOGSECTION_GEM)||TRACE_LOG_ENABLED(LOGSECTION_TRAP))
      log_os_call(TRAP_GEM);
#endif
    if(REGW(0)==0x73) // VDI
    {
#if defined(SSE_GUI_STATUS_BAR_MOUSE) && !defined(SSE_GUI_STATUS_BAR_MOUSE2)
      MEM_ADDRESS contrl=SafeLPeek(REGL(1)); //Cpu.r[1] has vdi parameter block.
      WORD vdi_op=SafeDPeek(contrl+0);  //opcode
      if(vdi_op==124) // vq_mouse
      {
        on_rte=ON_RTE_MOUSE;
        on_rte_interrupt_depth=interrupt_depth;
        vdi_ptsout=SafeLPeek(REGL(1)+16);
      }
#endif
    }
#ifndef NO_CRAZY_MONITOR
    if(extended_monitor)
    { //instruction is TRAP #2 (VDI or AES) (Megar)
      if(REGW(0)==0x73) 
      { //vdi
        MEM_ADDRESS adddd=SafeLPeek(Cpu.r[1]); //r[1] has vdi parameter block.  
        if(SafeDPeek(adddd+0)==1) //adddd points to the control array
        { //v_opnwk OPCODE
#if defined(SSE_STATS)
          Stats.nVdii++;
#endif
          on_rte=ON_RTE_EMHACK;
          on_rte_interrupt_depth=interrupt_depth;
          vdi_intout=SafeLPeek(Cpu.r[1]+12);
        }
      }
      ioaccess|=IOACCESS_INTERCEPT_OS;
    }
#endif
  }
}


void intercept_gemdos() {
#if defined(SSE_ENABLE_TRACE_LOG)
  if(TRACE_LOG_ENABLED(LOGSECTION_GEMDOS)||TRACE_LOG_ENABLED(LOGSECTION_TRAP))
    log_os_call(TRAP_GEMDOS);
#endif
#ifndef DISABLE_STEMDOS
  StemdosCheckTrap1();
#endif
}

#if defined(SSE_ENABLE_TRACE_LOG)
bool vt52=false; // to skip VT-52 emulator escape characters
#endif

void intercept_bios() {
/*The ST BIOS routines can be called from user mode, and
are reentrant to three levels. They use registers A0-A2 and
D0-D2 as scratch registers. You should
note that the BIOS changes the command number and return
address on the stack.
*/
  bool Invalid;
  MEM_ADDRESS my_sp=GetSPBeforeTrap(&Invalid);
  if(Invalid)
    return;
  WORD func=SafeDPeek(my_sp);
#if defined(SSE_ENABLE_TRACE_LOG)
  // ouput characters to trace like on console, except no ESC/VT52
  if(func==BCONOUT && SafeDPeek(my_sp+2)==2)
  {
    char c=(char)SafeDPeek(my_sp+4);
    if(c==27)
    {
      TRACE("-");
      vt52=true;
    }
    else if(!vt52)
    {
      TRACE("%c",c);
    }
    else 
      vt52=false;
  }
  else if(TRACE_LOG_ENABLED(LOGSECTION_BIOS)||TRACE_LOG_ENABLED(LOGSECTION_TRAP))
    log_os_call(TRAP_BIOS);
#endif
#if defined(SSE_DEBUG_SYMBOLS)
  Tos.LastFunc=func; // BIOS-only for the moment
#endif
  switch(func) {
  case DRVMAP:
#ifndef DISABLE_STEMDOS
    // TOS calls this first before a HD check so this is the right place to do it
    Stemdos.UpdateDrvbits(); 
#endif
    break;
#ifndef DISABLE_STEMDOS
  case GETBPB:
  case MEDIACH:
  {
    WORD d=SafeDPeek(my_sp+2);
    if(d>=2)
    {
      if(Stemdos.IsMounted((BYTE)d))
      {
        if(func==GETBPB)
        { 
#ifdef DEADC0DE // fix old bug GEMDOS emu interception of GETBPB (Geneva/NeoDesk)
          // Make it get the BPB of A: instead, this might contain nonsense!
          SafeDPoke(my_sp+2,0); 
#endif
        }
        else
        {
          Cpu.r[0]=0; // Hasn't changed - everything ignores this anyway
          m68kPerformRte();  //don't need to check interrupts because sr won't actually have changed
        }
#if defined(SSE_STATS)
        Stats.nBios++;
#endif
      }
    }
    break;
  }
#endif//#ifndef DISABLE_STEMDOS
  default:
    break;
  }//sw
}


void intercept_xbios() {
  bool Invalid;
  MEM_ADDRESS my_sp=GetSPBeforeTrap(&Invalid);
  if(Invalid)
    return;
  WORD func=SafeDPeek(my_sp);
#if defined(SSE_ENABLE_TRACE_LOG)
  if(TRACE_LOG_ENABLED(LOGSECTION_XBIOS)||TRACE_LOG_ENABLED(LOGSECTION_TRAP))
    log_os_call(TRAP_XBIOS);
#endif
  switch(func) {
  case VSYNC:
    if(OPTION_EMU_DETECT && Cpu.r[6]==Cpu.r[7] && Cpu.r[7]==0x456d753f) // Emu?
    { // Vsync with Emu? in D6, D7 ->emudetect on
      Cpu.r[6]=0x53544565; // STEe
      Cpu.r[7]=0x6d456e67; // mEng
      areg[0]=0xffc100; // base
      emudetect_called=true;
#if !defined(SSE_NO_FALCONMODE)
      emudetect_init();
#endif
#if defined(SSE_STATS)
      Stats.mskSpecial|=Stats.EMU_DETECT;
#endif
      m68kPerformRte();  //don't need to check interrupts because sr won't actually have changed
    }
    break;

#if defined(SSE_GEM_CONTROL_PANEL)
  case KBRATE:
    if(OptionBox.IsVisible() && OptionBox.Page==PAGE_GEM_CP)
    {
      WORD rate=SafeDPeek(my_sp+4);
      WORD delay=SafeDPeek(my_sp+2);
      HWND hwnd=GetDlgItem(OptionBox.Handle,IDC_REPEAT_DELAY);
      if(delay!=0xFFFF)
        PostMessage(hwnd,TBM_SETPOS,1,delay);
      PostMessage(OptionBox.Handle,WM_HSCROLL,0,(LPARAM)hwnd); // force update
      hwnd=GetDlgItem(OptionBox.Handle,IDC_REPEAT_RATE);
      if(rate!=0xFFFF)
      {
        PostMessage(hwnd,TBM_SETPOS,1,rate);
        PostMessage(OptionBox.Handle,WM_HSCROLL,0,(LPARAM)hwnd); // force update
      }
    }
    break;
#endif

#if !defined(DISABLE_STEMDOS)
  case GETTIME:
    if(Stemdos.bInterceptDateTime && OPTION_RTC_HACK)
    { // Get clock time
      time_t t=time(NULL);
      struct tm *lpTime=localtime(&t);
      Cpu.r[0]=TMToDOSDateTime(lpTime);
      m68kPerformRte();  //don't need to check interrupts because sr won't actually have changed
#if defined(SSE_STATS)
      Stats.nXbios++;
#endif
    }
    break;
#endif
//#define SSE_TOS_RANDOM
#if defined(SSE_TOS_RANDOM)
  //Random() returns a 24 bit random number
  case RANDOM:
    if(OPTION_HACKS)
    {
#ifdef SSE_BETA
      TRACE_OSD("RND");
      r[0]=rand()&0xffffff;
      m68kPerformRte();  //don't need to check interrupts because sr won't actually have changed
#if defined(SSE_STATS)
      Stats.nXbios++;
#endif
#endif
    }
    break;
#endif
  default:
    break;
  }//sw
}


#if defined(SSE_ENABLE_TRACE_LOG)

void log_os_call(int trap) {
  if(!SSEConfig.TraceFile||(IRD&0x4E40)!=0x4E40) // TRAP instruction only
    return;
  int detail=1;
  char stmp[512];
  EasyStr l="",a="";
  LONG lpar=0;
  bool Invalid;
  MEM_ADDRESS my_sp=GetSPBeforeTrap(&Invalid);
  if(Invalid)
    return;
  MEM_ADDRESS my_spp=my_sp+2;
  unsigned int call=SafeDPeek(my_sp);
#if SSE_VERSION>=420
  l=HEXSl(old_pc,6)+"-"+HEXSl(IRD,4) +": ";
#else
  l=HEXSl(old_pc,6)+": ";
#endif
  switch(trap) {
  case TRAP_GEMDOS:
    if(call<0x58ul)
      a=(char*)gemdos_calls[call];
    l+="GEMDOS $";
    l+=HEXSl(call,3); //3 for later OS bigger numbers, but only know to $57
    break;
  case TRAP_BIOS:
    if(call<12ul)
      a=(char*)bios_calls[call];
    l+="BIOS $";
    l+=HEXSl(call,2); //1 digit would be enough
    //l+=(int)call;
    if(call==9||call==1||call==3) //Mediach + Bconstat + Bconout
      detail=2;
    break;
  case TRAP_XBIOS:
    if(call<40ul)
      a=(char*)xbios_calls[call];
    l+="XBIOS $";
    l+=HEXSl(call,2); //2 digits are enough
    //l+=(int)call;
    break;
  case TRAP_GEM:
    //switch(REGL(0)) {
    switch(REGW(0)) {
    case 0x73: // VDI
    {
      /*
.data
_contrl: ds.w 12
_intin: ds.w 128
_ptsin: ds.w 256
_intout: ds.w 128
_ptsout: ds.w 256
_VDIpb: dc.l _contrl, _intin, _ptsin
dc.l _intout, _ptsout
.end
      */

      a="VDI ";
      MEM_ADDRESS contrl=SafeLPeek(Cpu.r[1]); //Cpu.r[1] has vdi parameter block.
      WORD vdi_op=SafeDPeek(contrl+0);  //opcode
      a+=vdi_op;
     // a+=", subopcode ";
      a+=",";
      WORD subop=SafeDPeek(contrl+10); //subopcode
      a+=subop;
      WORD contrl1=SafeDPeek(contrl+2);
      a+=",";
      a+=contrl1;

      MEM_ADDRESS intin=SafeLPeek(Cpu.r[1]+4);
      vdi_intout=SafeLPeek(Cpu.r[1]+12);

      switch(vdi_op) {
      case 1: a+=" v_opnwk"; break; // Open physical workstation. 7.66
      case 2: a+=" v_clswk"; break; // Close a physical workstation. 7.35
      case 3: a+=" v_clrwk"; break; // Close a physical workstation. 7.34
      case 4: a+=" v_updwk"; break; // Update workstation. 7.78
      case 5:
        switch(subop) {
        case 1: a+=" vq_chcells"; break; // Return alpha screen size. 7.87
        case 2: a+=" v_exit_cur"; break; // Exit text mode. 7.46
        case 3: a+=" v_enter_cur"; break; // Enter text mode. 7.45
        case 4: a+=" v_curup"; break; // Move text cursor up one row. 7.40
        case 5: a+=" v_curdown"; break; // Move text cursor down one row. 7.37
        case 6: a+=" v_curright"; break; // Move text cursor right one row. 7.38
        case 7: a+=" v_curleft"; break; // Move text cursor up one row. 7.38
        case 8: a+=" v_curhome"; break; // Home text cursor. 7.37
        case 9: a+=" v_eeos"; break; // Erase to end of screen. 7.42
        case 10: a+=" v_eeol"; break; // Erase to end of line. 7.41
        case 11: a+=" vs_curaddress"; break; // Position text cursor. 7.126
        case 12: a+=" v_curtext"; break; // Output text (alpha mode). 7.39
        case 13: a+=" v_rvon"; break; // Reverse text on (alpha mode). 7.75
        case 14: a+=" v_rvoff"; break; // Reverse text off (alpha mode). 7.75
        case 15: a+=" vq_curaddress"; break; // Inquire text cursor location. 7.89
        case 16: a+=" vq_tabstatus"; break; // Get availability of tablet. 7.95
        case 17: a+=" v_hardcopy"; break; // Output screen to printer. 7.57
        case 18: a+=" v_dspcur"; break; // Display text cursor. 7.40
        case 19: a+=" v_rmcur"; break; // Remove text cursor. 7.74
        case 20: a+=" v_form_adv"; break; // Advance printer page. 7.48
        case 21: a+=" v_output_window"; break; // Output window of page to printer. 7.68
        case 22: a+=" v_clear_disp_list"; break; // Clear display list. 7.34
        case 23: a+=" v_bit_image"; break; // Render bit-image file. 7.31
        case 24: a+=" vq_scan"; break; // Return printer scan heights. 7.94
        case 25: a+=" v_alpha_text"; break; // Output printer text (alpha mode). 7.23
        case 60: a+=" vs_palette"; break; // Set color palette. 7.127
        case 81: a+=" vt_resolution"; break; // Set tablet resolution. 7.165
        case 82: a+=" vt_axis"; break; // Set tablet axis resolution. 7.164
        case 83: a+=" vt_origin"; break; // Set tablet origin. 7.164
        case 84: a+=" vq_tdimensions"; break; // Return tablet X and Y dimensions. 7.96
        case 85: a+=" vt_alignment"; break; // Set tablet alignment. 7.163
        case 91: a+=" vqp_films"; break; // Return camera film types. 7.101
        case 92: a+=" vqp_state"; break; // Return camera driver state. 7.101
        case 93: a+=" vsp_state"; break; // Set camera driver state. 7.145
        case 94: a+=" vsp_save"; break; // Save camera driver state. 7.145
        case 95: a+=" vsp_message"; break; // Supress camera screen messages. 7.144
        case 96: a+=" vqp_error"; break; // Return camera error status. 7.100
        case 98: a+=" v_meta_extents"; break; // Specify metafile bounding box. 7.60
        case 99:
          switch(contrl1) {
          case 0: a+=" vm_pagesize"; break; // Set metafile page size. 7.85
          case 1: a+=" vm_coords"; break; // Set metafile coordinate system. 7.83
          case 32: a+=" v_bez_qual"; break; // Set bezier quality. 7.30
          default: a+=" v_write_meta"; break; // Write metafile item. 7.79
          }
          break;
        case 100: a+=" vm_filename"; break; // Set metafile filename. 7.84
        case 102: a+=" v_fontinit"; break; // Select a new system font. 7.48
        case 2000: a+=" v_pgcount"; break; // Specify laser printer copies. 7.69
        }
        break;
      case 6: a+=(subop==13) ? " v_bez" : " v_pline"; break; // Draw a polyline. 7.71
      //case 6, 13: a+=" v_bez"; break; // Draw a bezier curve. 7.26
      case 7: a+=" v_pmarker"; break; // Draw polymarkers. 7.72
      case 8: // Output graphic text. 7.56
      {
        WORD n=SafeDPeek(contrl+6);
        sprintf(stmp," v_gtext (%d) ",n);
        char *stmp2=stmp+strlen(stmp);
        for(int i=0;i<n;i++,stmp2++)
        {
          char b=(char)SafeDPeek(intin+i*2);
          sprintf(stmp2,"%c",(b>=' ') ? b : '?');
        }
        *stmp2='\0';
        a+=stmp;//" v_gtext";
        break;
      }
      case 9: a+=(subop==13) ? " v_bez_fill" : " v_fillarea"; break; // Draw a filled polygon. 7.46
      //9, 13: a+=" v_bez_fill"; break; // Draw a filled bezier curve. 7.27
      case 10: a+=" v_cellarray"; break; // Draw a cell array. 7.32
      case 11:
        switch(subop) {
        case 1: a+=" v_bar"; break; // Draw a rectangle. 7.25
        case 2: a+=" v_arc"; break; // Draw an arc. 7.24
        case 3: a+=" v_pieslice"; break; // Draw a pieslice. 7.70
        case 4: a+=" v_circle"; break; // Draw a circle. 7.33
        case 5: a+=" v_ellipse"; break; // Draw an ellipse 7.43
        case 6: a+=" v_ellarc"; break; // Draw an elliptical arc. 7.42
        case 7: a+=" v_ellpie"; break; // Draw an elliptical pie segment. 7.44
        case 8: a+=" v_rbox"; break; // Draw a rounded-rectangle. 7.72
        case 9: a+=" v_rfbox"; break; // Draw a filled rounded-rectangle. 7.73
        case 10: a+=" v_justified"; break; // Output justified text. 7.58
        case 13: a+=(contrl1==0) ? " v_bez_off" : " v_bez_on"; break; // Disable bezier drawing. 7.28
        }
        break;
      case 12: a+=" vst_height"; break; // Set graphic text height (in pixels). 7.153
      case 13: a+=" vst_rotation"; break; // Set graphic text rotation. 7.156
      case 14: a+=" vs_color"; break; // Set color palette index. 7.126
      case 15: a+=" vsl_type"; break; // Set line type. 7.135
      case 16: a+=" vsl_width"; break; // Set line width. 7.137
      case 17: a+=" vsl_color"; break; // Set line color. 7.134
      case 18: a+=" vsm_type"; break; // Set marker type. 7.142
      case 19: a+=" vsm_height"; break; // Set marker height. 7.139
      case 20: a+=" vsm_color"; break; // Set marker color. 7.138
      case 21: a+=" vst_font"; break; // Set graphic text font. 7.152
      case 22: a+=" vst_color"; break; // Set graphic text color. 7.150
      case 23: a+=" vsf_interior"; break; // Set fill interior type. 7.129
      case 24: a+=" vsf_style"; break; // Set fill style type. 7.131
      case 25: a+=" vsf_color"; break; // Set fill color. 7.129
      case 26: a+=" vq_color"; break; // Inquire palette index. 7.88
      case 27: a+=" vq_cellarray"; break; // Inquire cell array. 7.86
      case 28: a+=" vrq_locator"; break; // Poll for mouse/keyboard input. 7.121
      //28: a+=" vsm_locator"; break; // Sample mouse/keyboard input. 7.140
      case 29: a+=" vrq_valuator"; break; // Poll for valuator input. 7.123
      //29: a+=" vsm_valuator"; break; // Sample valuator input. 7.143
      case 30: a+=" vrq_choice"; break; // Poll for choice input. 7.121
      //30: a+=" vsm_choice"; break; // Sample input from choice device. 7.138
      case 31: a+=" vsm_string"; break; // Sample keyboard string input. 7.141
      case 32: a+=" vswr_mode"; break; // Set writing mode. 7.162
      case 33: // Set input mode. 7.133
      { //Using this function will cause the AES to function improperly.
        WORD handle=SafeDPeek(contrl+12);
        WORD device=SafeDPeek(intin);
        WORD mode=SafeDPeek(intin+2);
        sprintf(stmp," vsin_mode (%d,%d,%d)",handle,device,mode);
        a+=stmp;//" vsin_mode";
        break;
      }
      case 35: a+=" vql_attributes"; break; // Return line attributes. 7.98
      case 36: a+=" vqm_attributes"; break; // Return marker attributes. 7.99
      case 37: a+=" vqf_attributes"; break; // Return fill area attributes. 7.96
      case 38: a+=" vqt_attributes"; break; // Return text attributes. 7.104
      case 39: a+=" vst_alignment"; break; // Set graphic text alignment. 7.146
      case 100: a+=" v_opnvwk"; break; // Open: a+=" virtual workstation. 7.61
      case 101: a+=" v_clsvwk"; break; // Close a: a+=" virtual workstation. 7.35
      case 102: a+=" vq_extnd"; break; // Inquire workstation attributes. 7.89
      case 103: a+=" v_contourfill"; break; // Fill an irregularly shaped region. 7.36
      case 104: a+=" vsf_perimeter"; break; // Set fill perimeter: a+=" visibility. 7.130
      case 105: a+=" v_get_pixel"; break; // Read screen pixel: a+=" value. 7.55
      case 106: a+=" vst_effects"; break; // Set graphic text effects. 7.150
      case 107:// Set graphic text height (by point). 7.155
      {
        WORD handle=SafeDPeek(contrl+12);
        WORD point=SafeDPeek(intin);
        sprintf(stmp," vst_point (%d,%d)",handle,point);
        a+=stmp;//" vst_point";
        break;
      }
      case 108: a+=" vsl_ends"; break; // Set line end style. 7.134
      case 109: a+=" vro_cpyfm"; break; // Copy raster (opaque mode). 7.119
      case 110: a+=" vr_trnfm"; break; // Transform raster form. 7.117
      case 111: a+=" vsc_form"; break; // Set mouse form. 7.128
      case 112: a+=" vsf_udpat"; break; // Set user defined fill pattern 7.132
      case 113: a+=" vsl_udsty"; break; // Set user-defined line style. 7.136
      case 114: a+=" vr_recfl"; break; // Output filled rectangle. 7.117
      case 115: a+=" vqin_mode"; break; // Return input mode for device. 7.97
      case 116: a+=" vqt_extent"; break; // Return graphic text extent. 7.107
      case 117: a+=" vqt_width"; break; // Return graphic character width. 7.115
      case 118: a+=" vex_timv"; break; // Install timer tick routine. 7.83
      case 119: a+=" vst_load_fonts"; break; // Load fonts from disk. 7.154
      case 120: a+=" vst_unload_fonts"; break; // Unload fonts. 7.160
      case 121: a+=" vrt_cpyfm"; break; // Copy raster (transparent mode). 7.124
      case 122: a+=" void v_show_c"; break; // Show mouse cursor. 7.77
      case 123: a+=" void v_hide_c"; break; // Hide mouse cursor. 7.57
      case 124: a+=" vq_mouse"; break; // Get mouse position and state. 7.93
      case 125: a+=" vex_butv"; break; // Install mouse button routine. 7.80
      case 126: a+=" vex_motv"; break; // Install mouse movement routine. 7.82
      case 127: a+=" vex_curv"; break; // Install mouse rendering routine. 7.81
      case 128: a+=" vq_key_s"; break; // Get shift key status. 7.93
      case 129: a+=" vs_clip"; break; // Set clipping rectangle. 7.125
      case 130: a+=" vqt_name"; break; // Return font name and index. 7.113
      case 131: a+=" vqt_fontinfo"; break; // Return font size information. 7.111
      case 232: a+=" vqt_fontheader"; break; // Copy the Speedo font header into a user defined buffer. 7.110
      case 234: a+=" vqt_trackkern"; break; // Inquire about current track kerning. 7.114
      case 235: a+=" vqt_pairkern"; break; // Inquire about current pair kerning. 7.115
      case 236: a+=" vst_charmap"; break; // Set ASCII/Speedo index interpretation mode. 7.149
      case 237: a+=" vst_kern"; break; // Set kerning modes. 7.154
      case 239: a+=" v_getbitmap_info"; break; // Return Speedo font bitmap extents. 7.53
      case 240: a+=" vqt_f_extent"; break; // Return outline text extent. 7.108
      case 241: a+=" v_ftext"; break; // Output outlined text. 7.49
      //241: a+=" v_ftext16"; break; // Output 16-bit outlined text. 7.50
      //241: a+=" v_ftext_offset"; break; // Output outlined text with individual character offsets. 7.51
      //241: a+=" v_ftext_offset16"; break; // Output 16-bit outlined text with individual character offsets. 7.52
      case 242: a+=" v_killoutline"; break; // Free character outline (no longer used with SpeedoGDOS). 7.59
      case 243: a+=" v_getoutline"; break; // Return character outline. 7.54
      case 244: a+=" vst_scratch"; break; // Set outline scratch buffer. 7.157
      case 245: a+=" vst_error"; break; // Set GDOS error reporting mode. 7.151
      case 246: a+=" vst_arbpt"; break; // Set outline text point size. 7.147
      //246: a+=" vst_arbpt32"; break; // Set outline text point size to a fix31: a+=" value. 7.148
      case 247: a+=" vqt_advance"; break; // Return character advance: a+=" vector. 7.102
      //247: a+=" vqt_advance32"; break; // Return character advance: a+=" vector as a fix31: a+=" value. 7.103
      case 248: a+=" vqt_devinfo"; break; // Return device information. 7.106
      case 249: a+=" v_savecache"; break; // Save bitmap cache to disk. 7.76
      case 250: a+=" v_loadcache"; break; // Load bitmap cache from disk. 7.59
      case 251: a+=" v_flushcache"; break; // Flush outline font cache. 7.47
      case 252: a+=" vst_setsize"; break; // Set outline text proportion. 7.158
      //252: a+=" vst_setsize32"; break; // Set outline text proportion to a fix31: a+=" value. 7.159
      case 253: a+=" vst_skew"; break; // Set outline text skew factor. 7.160
      case 254: a+=" vqt_get_table"; break; // Return character mappings. 7.112
      case 255: a+=" vqt_cachesize"; break; // Return bitmap cache size 7.105
      }//switch(vdi_op)
      break;
    }
    case 0xC8: //AES
    {
/*
struct aespb
{
WORD *contrl;
WORD *global;
WORD *intin;
WORD *intout;
LONG *addrin;
LONG *addrout;
};
*/
      aes_calls_since_reset++;
      a="AES ";
      MEM_ADDRESS contrl=SafeLPeek(Cpu.r[1]); //Cpu.r[1] has aes parameter block.
      MEM_ADDRESS intin=SafeLPeek(Cpu.r[1]+8);
      aes_intout=SafeLPeek(Cpu.r[1]+12);
      WORD aes_op=SafeDPeek(contrl+0);  //opcode
      a+=aes_op;
      switch(aes_op) {
      case 10: a+=" appl_init"; break;
      case 11: a+=" appl_read"; break;
      case 12: a+=" appl_write"; break;
      case 13: a+=" appl_find"; break;
      case 14: a+=" appl_tplay"; break;
      case 15: a+=" appl_trecord"; break;
      case 18: a+=" appl_search"; break;
      case 19: a+=" appl_exit"; break;
      case 20: a+=" evnt_keybd"; break;
      case 21: a+=" evnt_button"; break;
      case 22: a+=" evnt_mouse"; break;
      case 23: a+=" evnt_mesag"; break;
      case 24: a+=" evnt_timer"; break;
      case 25: a+=" evnt_multi"; break;
      case 26: a+=" evnt_dclick"; break;
      case 30: a+=" menu_bar"; break;
      case 31: a+=" menu_icheck"; break;
      case 32: a+=" menu_ienable"; break;
      case 33: a+=" menu_tnormal"; break;
      case 34: a+=" menu_text"; break;
      case 35: a+=" menu_register"; break;
      case 36: a+=" menu_popup"; break;
      case 37: a+=" menu_attach"; break;
      case 38: a+=" menu_istart"; break;
      case 39: a+=" menu_settings"; break;
      case 40: a+=" objc_add"; break;
      case 41: a+=" objc_delete"; break;
      case 42: a+=" objc_draw"; break;
      case 43: a+=" objc_find"; break;
      case 44: a+=" objc_offset"; break;
      case 45: a+=" objc_order"; break;
      case 46: a+=" objc_edit"; break;
      case 47: a+=" objc_change"; break;
      case 48: a+=" objc_sysvar"; break;
      case 50: a+=" form_do"; break;
      case 51: a+=" form_dial"; break;
      case 52: a+=" form_alert"; break;
      case 53: a+=" form_error"; break;
      case 54: a+=" form_center"; break;
      case 55: a+=" form_keybd"; break;
      case 56: a+=" form_button"; break;
      case 70: a+=" graf_rubberbox"; break;
      case 71: a+=" graf_dragbox"; break;
      case 72: a+=" graf_movebox"; break;
      case 73: a+=" graf_growbox"; break;
      case 74: a+=" graf_shrinkbox"; break;
      case 75: a+=" graf_watchbox"; break;
      case 76: a+=" graf_slidebox"; break;
      case 77: a+=" graf_handle"; break;
      case 78: a+=" graf_mouse"; break;
      case 79: a+=" graf_mkstate"; break;
      case 80: a+=" scrp_read"; break;
      case 81: a+=" scrp_write"; break;
      case 90: a+=" fsel_input"; break;
      case 91: a+=" fsel_exinput"; break;
      case 100: a+=" wind_create"; break;
      case 101: a+=" wind_open"; break;
      case 102: a+=" wind_close"; break;
      case 103: a+=" wind_delete"; break;
      case 104:
      {
        WORD handle=SafeDPeek(intin+0);
        WORD mode=SafeDPeek(intin+2);
        sprintf(stmp," wind_get (%d, %d) (0=ERR)",handle,mode);
        a+=stmp;//" wind_get";
        break;
      }
      case 105: a+=" wind_set"; break;
      case 106: a+=" wind_find"; break;
      case 107: a+=" wind_update"; break;
      case 108: a+=" wind_calc"; break;
      case 109: a+=" wind_new"; break;
      case 110: a+=" rsrc_load"; break;
      case 111: a+=" rsrc_free"; break;
      case 112: a+=" rsrc_gaddr"; break;
      case 113: a+=" rsrc_saddr"; break;
      case 114: a+=" rsrc_obfix"; break;
      case 115: a+=" rsrc_rcfix"; break;
      case 120: a+=" shel_read"; break;
      case 121: a+=" shel_write"; break;
      case 122: a+=" shel_get"; break;
      case 123: a+=" shel_put"; break;
      case 124: a+=" shel_find"; break;
      case 125: a+=" shel_envrn"; break;
      case 130: a+=" appl_getinfo"; break;
      }//switch(aes_op)
      break;
    }//scope
    }//switch(REGL(0))
    break;
  }//switch(trap)
  if(a.IsEmpty())
  {
    //l+=" (unrecognised)";
    char stmp2[128];
    sprintf(stmp2," ? D0 %X",REGL(0));
    l+=stmp2;
    //l+=" (unknown)";
  }
  else
  {
    l+="  ";
    for(int i=0;i<a.Length();i++)
    {
      if(a[i]=='%')
      {
        lpar=SafeLPeek(my_spp);
        my_spp+=4;
        l+=HEXSl(lpar,8);
      }
      else if(a[i]=='#') // for Bconout
      {
        WORD w=SafeDPeek(my_spp);
        char c=(char)w;
        if(c>=' ')
          l+=c;
        else
          l+=w;
        my_spp+=2;
      }
      else if(a[i]=='&')
      {
        l+=HEXSl(SafeDPeek(my_spp),4);
        my_spp+=2;
      }
      else if(a[i]=='$')
      {
        char c;
        int ii;
        for(ii=0;ii<30;ii++)
        {
          c=(char)SafePeek(lpar+ii);
          if(!c)
            break;
          l+=c;
        }
        if(ii>=30)
          l+="...";
      }
      else
        l+=a[i];
    }
  }

  if( (detail<2||TRACE_LOG2_ENABLED)&& (detail<3||TRACE_LOG3_ENABLED))
  {
    TRACE("TRAP #%d %s\n",trap,l.Text);
  }
}

#endif//#if defined(SSE_ENABLE_TRACE_LOG)

#ifndef ONEGAME

MEM_ADDRESS get_TOS_address(char* const File) {
  MEM_ADDRESS ad=0;
  FILE *fp=(File[0]=='\0') ? NULL :fopen(File,"rb");
  if(fp)
  {
    BYTE HiHi=0,LoHi=0,HiLo=0,LoLo=0;
    FREAD(&HiLo,1,1,fp);
    FREAD(&LoLo,1,1,fp);
    if(HiLo==0x60&&LoLo==0x06) // Pre-tos machines, need boot disk, no header
      ad=0xFC0000; // hardcoded in GLUE
    else // read TOS header
    {
      FSEEK(fp,8,SEEK_SET);
      FREAD(&HiHi,1,1,fp);
      FREAD(&LoHi,1,1,fp);
      FREAD(&HiLo,1,1,fp);
      FREAD(&LoLo,1,1,fp);
      MEM_ADDRESS new_rom_addr=MAKELONG(MAKEWORD(LoLo,HiLo),MAKEWORD(LoHi,HiHi))&0xffffff;
      if(LoHi==0xFC||LoHi==0xE0)
        ad=new_rom_addr;
    }
    fclose(fp);
  }
  return ad;
}

#undef LOGSECTION
#define LOGSECTION LOGSECTION_INIT

#if defined(SSE_GUI_INSTANTCHANGE)

#ifdef SSE_421R0 //ref for trace

bool load_TOS(char* const File) { // true: succeeded
  DWORD checksum=~0u;
  bool success=true;
  try {
    if(File[0]=='\0')
      throw false;
    FILE* fp=FOPEN(File,"rb");
    if(fp==NULL) // for example loading alien snapshot
      throw false;
    MEM_ADDRESS new_rom_addr=get_TOS_address(File);
    if(new_rom_addr==0)
      throw false;
    ASSERTB(new_rom_addr==0xE00000 || new_rom_addr==0xFC0000);
    DWORD Len=((DWORD)GetFileLength(fp)/1024)*1024;
    BYTE* newRom=new BYTE[Len];
    FREAD(newRom,1,Len,fp); // read all TOS
    add_to_crc32(checksum,newRom,Len); // for trace
    checksum=~checksum;
#ifndef BIG_ENDIAN_PROCESSOR //  memory is reversed on little-endian PC
    FSEEK(fp,0,SEEK_SET);
    BYTE* newRom_End_minus_1=newRom+Len-1;
    for(DWORD m=0;m<Len;m++)
      *(BYTE*)(newRom_End_minus_1-m)=(BYTE)FGETC(fp); // slow
#endif
    FCLOSE(fp);
    BYTE* oldRom=STRom;
    STRom=newRom;
    tos_len=Len;
    tos_high=(new_rom_addr>0xE00000); // $FC0000 is higher than $E00000, "tos high" is STF TOS
    rom_addr=new_rom_addr;
    rom_addr_end=rom_addr+tos_len;
#ifndef BIG_ENDIAN_PROCESSOR
    Rom_End=STRom+tos_len;
    Rom_End_minus_1=newRom_End_minus_1; // already computed!
    Rom_End_minus_2=Rom_End-2;
    Rom_End_minus_4=Rom_End-4;
#endif
    if(oldRom)
      delete[] oldRom;
    tos_version=ROM_DPEEK(2);
    SSEConfig.TosLanguage=ROM_PEEK(0x1D);
#if defined(SSE_GUI_QUICKSETTINGS)
    OptionBox.TosList.AddPath(File);
#endif
    /*  If we don't need ACSI hard drive emulation, neutralise TOS check for
        a faster boot (from Hatari)*/
    if(OPTION_HACKS && (tos_version==0x106||tos_version==0x162) && !pasti_active  && !ACSI_EMU_ON)
    {
      TRACE_LOG("STE tos boot patch %X %X %X\n",0x576,ROM_LPEEK(0x576),0x4E714E71);
      ROM_LPEEK(0x576)=0x4E714E71; // bsr +$e4 -> nop, "dma boot"
    }
    // nuke hd spinup delay of T205, on T206 there's a time bar so it's not so useful
    MEM_ADDRESS ta=0x0007A0;
    if(OPTION_HACKS && tos_version==0x205 && ROM_DPEEK(ta)==0x631E)
    {
      TRACE_LOG("MSTE tos boot patch %X %X %X\n",ta,ROM_DPEEK(ta),0x631E);
      ROM_DPEEK(ta)=0x601E;
    }
    Glue.Restore(); // for Decode before Power On so that IR is correct!
#if defined(SSE_GEM_CONTROL_PANEL)
    // find system variables for key repeat
    for(DWORD m=0;m<Len-8;m+=4)
    {
      if(ROM_LPEEK(m)==0x1B7C000F)
      {
        if(ROM_LPEEK(m+6)==0x1B7C0002)
        {
          OptionBox.TosKeyRepeat=ROM_DPEEK(m+4)&0xFFFF;
          TRACE_LOG("TOS key repeat address: $%X\n",OptionBox.TosKeyRepeat);
        }
      }
    }
#endif
  }
  catch(bool ec) {
    success=ec;
  }
  TRACE("%s %s v%x/%d %s %08X %s %d\n","Load",CHECKPATH(File),tos_version,
         SSEConfig.TosLanguage,"CRC32",checksum,"OK",success);
  return success;
}


#else//#ifdef SSE_421R0 //ref 

bool load_TOS(char* const File) { // true: succeeded
  if(File[0]=='\0') 
    return false;
  MEM_ADDRESS new_rom_addr=get_TOS_address(File);
  FILE *fp=fopen(File,"rb");
  if(fp==NULL) // for example loading alien snapshot
    return false;
  DWORD Len=((DWORD)GetFileLength(fp)/1024)*1024;
  BYTE *newRom=new BYTE[Len];
  DWORD checksum=~0u;
  FREAD(newRom,1,Len,fp);
  add_to_crc32(checksum,newRom,Len);
  checksum=~checksum;
#ifndef BIG_ENDIAN_PROCESSOR
  FSEEK(fp,0,SEEK_SET);
  BYTE *newRom_End_minus_1=newRom+Len-1;
  for(DWORD m=0;m<Len;m++) 
    *(BYTE*)(newRom_End_minus_1-m)=(BYTE)FGETC(fp); // slow but memory is reversed on little-endian PC
#endif
  fclose(fp);
  BYTE *oldRom=STRom;
  // would need critical section if running...
  STRom=newRom;
  tos_len=Len;
  tos_high=(new_rom_addr==0xfc0000); // $FC0000 is higher than $E00000
  rom_addr=new_rom_addr;
  rom_addr_end=rom_addr+tos_len;
#ifndef BIG_ENDIAN_PROCESSOR
  Rom_End=STRom+tos_len;
  Rom_End_minus_1=newRom_End_minus_1; // already computed!
  Rom_End_minus_2=Rom_End-2;
  Rom_End_minus_4=Rom_End-4;
#endif
  if(oldRom)
    delete[] oldRom;
  tos_version=ROM_DPEEK(2);
  SSEConfig.TosLanguage=ROM_PEEK(0x1D);
#if defined(SSE_GUI_QUICKSETTINGS)
  OptionBox.TosList.AddPath(File);
#endif
  //SSEConfig.SwitchSTModel(ST_MODEL); // to adapt CPU clock
  TRACE("%s %s v%x/%d %s %08X\n","Load",CHECKPATH(File),tos_version,
    SSEConfig.TosLanguage,"CRC32",checksum);
/*  If we don't need ACSI hard drive emulation, neutralise TOS check for
    a faster boot (from Hatari)*/
  if(OPTION_HACKS && (tos_version==0x106||tos_version==0x162) && !pasti_active  && !ACSI_EMU_ON)
  {
    TRACE_LOG("STE tos boot patch %X %X %X\n",0x576,ROM_LPEEK(0x576),0x4E714E71);
    ROM_LPEEK(0x576)=0x4E714E71; // bsr +$e4 -> nop, "dma boot"
  }
  // nuke hd spinup delay of T205, on T206 there's a time bar so it's not so useful
  MEM_ADDRESS ta=0x0007A0;
  if(OPTION_HACKS && tos_version==0x205 && ROM_DPEEK(ta)==0x631E)
  {
    TRACE_LOG("MSTE tos boot patch %X %X %X\n",ta,ROM_DPEEK(ta),0x631E);
    ROM_DPEEK(ta)=0x601E; 
  }
  Glue.Restore(); // for Decode before Power On so that IR is correct!
#if defined(SSE_GEM_CONTROL_PANEL)
  // find system variables for key repeat
  for(DWORD m=0;m<Len-8;m+=4) 
  {
    if(ROM_LPEEK(m)==0x1B7C000F)
    {
      if(ROM_LPEEK(m+6)==0x1B7C0002)
      {
        OptionBox.TosKeyRepeat=ROM_DPEEK(m+4)&0xFFFF;
        TRACE_LOG("TOS key repeat address: $%X\n",OptionBox.TosKeyRepeat);
      }
    }
  }
#endif
  return true;
}

#endif//#ifdef SSE_421R0 //ref 

#else//#if defined(SSE_GUI_INSTANTCHANGE)

bool load_TOS(char* const File) { // true: succeeded
  if(File[0]=='\0') 
    return false;
  MEM_ADDRESS new_rom_addr=get_TOS_address(File);
  FILE *fp=fopen(File,"rb");
  if(fp==NULL) // for example loading alien snapshot
    return false;
  DWORD Len=((DWORD)GetFileLength(fp)/1024)*1024;
  //ASSERT(STRom || !tos_len);
  if(Len!=tos_len || !STRom)
  {
    if(STRom)
      delete[] STRom;
    STRom=new BYTE[Len];
    tos_len=Len;
  }
  tos_high=(new_rom_addr==0xfc0000); // $FC0000 is higher than $E00000
  rom_addr=new_rom_addr;
  rom_addr_end=rom_addr+tos_len;
#ifndef BIG_ENDIAN_PROCESSOR
  Rom_End=STRom+tos_len;
  Rom_End_minus_1=Rom_End-1;
  Rom_End_minus_2=Rom_End-2;
  Rom_End_minus_4=Rom_End-4;
#endif
  DWORD checksum=~0u;
  FREAD(STRom,1,Len,fp);
  add_to_crc32(checksum,STRom,Len);
  checksum=~checksum;
  FSEEK(fp,0,SEEK_SET);
  memset(STRom,0xff,Len);
  for(DWORD m=0;m<Len;m++) 
  {
    ROM_PEEK(m)=(BYTE)FGETC(fp); // slow but memory is reversed on little-endian PC
  }
  fclose(fp);
  tos_version=ROM_DPEEK(2);
  SSEConfig.TosLanguage=ROM_PEEK(0x1D);
  SSEConfig.SwitchSTModel(ST_MODEL); // to adapt CPU clock
  TRACE("%s %s v%x/%d %s %08X\n","Load",CHECKPATH(File),tos_version,
    SSEConfig.TosLanguage,"CRC32",checksum);
/*  If we don't need ACSI hard drive emulation, neutralise TOS check for
    a faster boot (from Hatari)*/
  if(OPTION_HACKS && (tos_version==0x106||tos_version==0x162) && !pasti_active  && !ACSI_EMU_ON)
  {
    TRACE_LOG("STE tos boot patch %X %X %X\n",0x576,ROM_LPEEK(0x576),0x4E714E71);
    ROM_LPEEK(0x576)=0x4E714E71; // bsr +$e4 -> nop, "dma boot"
  }
  // nuke hd spinup delay of T205, on T206, there's a time bar so it's not useful
  MEM_ADDRESS ta=0x0007A0;
  if(OPTION_HACKS && tos_version==0x205 && ROM_DPEEK(ta)==0x631E)
  {
    TRACE_LOG("MSTE tos boot patch %X %X %X\n",ta,ROM_DPEEK(ta),0x631E);
    ROM_DPEEK(ta)=0x601E; 
  }
  Glue.Restore(); // for Decode before Power On so that IR is correct!
#if defined(SSE_GEM_CONTROL_PANEL)
  // find system variables for key repeat
  for(DWORD m=0;m<Len-8;m+=4) 
  {
    if(ROM_LPEEK(m)==0x1B7C000F)
    {
      if(ROM_LPEEK(m+6)==0x1B7C0002)
      {
        OptionBox.TosKeyRepeat=ROM_DPEEK(m+4)&0xFFFF;
        TRACE_LOG("TOS key repeat address: $%X\n",OptionBox.TosKeyRepeat);
      }
    }
  }
#endif
  return true;
}

#endif//#if defined(SSE_GUI_INSTANTCHANGE)

#else //ONEGAME

bool load_TOS(char *) {
  tos_len=192*1024;
  tos_high=true;
  rom_addr=0xFC0000;
  Rom_End=STRom+tos_len;
  Rom_End_minus_1=Rom_End-1;
  Rom_End_minus_2=Rom_End-2;
  Rom_End_minus_4=Rom_End-4;
  tos_version=0x0102;
  return true;
}

#endif


void GetTOSKeyTableAddresses(MEM_ADDRESS* const lpUnshiftTable,MEM_ADDRESS* const lpShiftTable) {
  MEM_ADDRESS addr=0;
  while(addr<tos_len)
  {
    if(ROM_PEEK(addr++)=='u')
    {
      if(ROM_PEEK(addr)=='i')
      {
        addr++;
        if(ROM_PEEK(addr)=='o')
        {
          addr++;
          if(ROM_PEEK(addr)=='p')
          {
            *lpUnshiftTable=addr-25;
            break;
          }
        }
      }
    }
  }
  addr=(*lpUnshiftTable)+127;
  while(addr<tos_len)
  {
    if(ROM_PEEK(addr++)==27)
    {
      *lpShiftTable=addr-2;
      break;
    }
  }
}


// check that file exists, if not open file selector
void TTos::UpdateTOSPath(EasyStr* const pPath) {
  ASSERTB(pPath);
  if(pPath==NULL)
    return;
  if(!Exists(pPath->Text))
  {
#ifdef WIN32
    char *fstypes=FSTypes(3,NULL);
#if defined(SSE_VID_2SCREENS)
    // Windows deduces screen from handle
    *pPath=FileSelect(NotifyWin,T("Select TOS Image"),UsersPath.Text,fstypes,1,
      true,"img",pPath->Text);
#else
    *pPath=FileSelect(NULL,T("Select TOS Image"),UsersPath.Text,fstypes,1,
      true,"img",pPath->Text);
#endif
    free(fstypes);
#endif//WIN32
#ifdef UNIX
    fileselect.set_corner_icon(&Ico16,ICO16_CHIP);
    *pPath=fileselect.choose(XD,UsersPath,"",T("Select TOS Image"),FSM_LOAD|FSM_LOADMUSTEXIST,
                            romfile_parse_routine,".img");
#endif
  }
}


#undef LOGSECTION
#undef SV_MEMTOP
#undef SV_SCREENPT
#undef SV_V_BAS_AD
#undef BCONOUT
#undef GETBPB
#undef MEDIACH
#undef DRVMAP
#undef RANDOM
#undef GETTIME
#undef VSYNC
#if defined(SSE_GEM_CONTROL_PANEL)
#undef KBRATE
#endif


#if defined(SSE_DEBUG_SYMBOLS) // debugger-only

#define LOGSECTION LOGSECTION_GEMDOS

TTosSymbol TosSymbol[MAX_SYMBOLS];

SHORT NextTosSymbol=0;

TTos::TTos() {
  fpPrgCopy=NULL;
  Reset();
}


TTos::~TTos() {
  Reset();
}


void TTos::Reset() {
  if(fpPrgCopy)
    FCLOSE(fpPrgCopy);
  memset(this,0,sizeof(TTos));
  for(int i=0;i<MAX_SYMBOLS;i++)
    TosSymbol[i].ad=0xffffffff;
  NextTosSymbol=0;
}


// TTos::GetSymbols subfunction
void GetSymbolsSub(MEM_ADDRESS textbase,MEM_ADDRESS ad,MEM_ADDRESS fixup_ad) {
  ad=SafeLPeek(fixup_ad+textbase); //relocate this address
  TRACE_LOG2("fixup $%X at $%X was $%X\n",ad,fixup_ad+textbase,ad-textbase);
  for(int i=0;i<MAX_SYMBOLS&&TosSymbol[i].ad!=0xFFFFFFFF;i++) // check all symbols
  {
    if(ad-textbase==TosSymbol[i].value2)
    {
      // TOS fixup is always based on TEXT, even for DATA and BSS addresses
      // there's no segment information in the fixup segment, only in optional symbols
      // So we don't need to check the symbol type
      TTosSymbol& x=TosSymbol[i];
      x.ad=ad; // record absolute address of symbol
      TRACE_LOG2("#%d %s $%X $%X $%X at %X\n",i,x.name,x.type,x.value1,x.value2,x.ad);
      break;
    }
  }//nxt i
}


// update boiler symbols using file pointer to GEMDOS file or temp copy
// of file in disk image
bool TTos::GetSymbols(FILE* fpPrg, MEM_ADDRESS const basepage) {
  TraceBasepage(basepage);
  bool ok=false;
  INT64 oldpos=FTELL(fpPrg);
  Basepage=basepage;
  FSEEK(fpPrg,0,SEEK_SET);
  WORD PRG_magic=STfileReadWord(fpPrg);
  if(PRG_magic==0x601a) //executable
    ok=true;
  FSEEK(fpPrg,0x1C,SEEK_SET); //seek to end of header (TEXT Segment)
  ASSERTB((basepage+0x100UL+PRG_tsize+PRG_dsize+PRG_bsize)<=SafeLPeek(basepage+0x4)); //basepage+4 contains hi-tpa
  MEM_ADDRESS textbase=basepage+0x100,ad=textbase; //start of text area
  FSEEK(fpPrg,PRG_tsize,SEEK_CUR); //skip text segment
  ad+=PRG_tsize;
  MEM_ADDRESS ad_of_data=ad;
  FSEEK(fpPrg,PRG_dsize,SEEK_CUR); //skip data segment
  ad+=PRG_dsize;
  if(ok && PRG_ssize && (PRG_ssize%14)==0) // TOS symbols present
  {
    int nParts=PRG_ssize/14;
    int ctr=0;
    for(int i=0;i<nParts;i++) // load symbols
    {
      TTosSymbol x;
      memset(&x,0,sizeof(x));
      FREAD(&x.name,8,1,fpPrg);
      FREAD(&x.type,6,1,fpPrg); // risky?
      SWAP_BIG_ENDIAN_WORD(x.type);
      if(x.type&0x48) // long GST symbol
      {
        FREAD(&x.name[8],14,1,fpPrg);
        i++;
      }
      SWAP_BIG_ENDIAN_WORD(x.value1);
      SWAP_BIG_ENDIAN_WORD(x.value2);
      x.ad=x.value2+ad_of_data;
      TRACE("#%d %s $%X $%X $%X at %X\n",ctr,x.name,x.type,x.value1,x.value2,x.ad);
      TosSymbol[NextTosSymbol++]=x; // memorize
      if(NextTosSymbol>=MAX_SYMBOLS)
        NextTosSymbol=0;
      ctr++;
    }
    ASSERTB(ctr<MAX_SYMBOLS);
    TRACE("%d SYMBOLS in %d bytes\n",ctr,Tos.PRG_ssize);
  }
  // we don't really do fixups but we need to update the symbol addresses
  LONG fixup_ad=STfileReadLong(fpPrg);
  TRACE("FIXUP %X\n",fixup_ad);
  if(ok && fixup_ad && !STfileReadError)
  {
    GetSymbolsSub(textbase,ad,fixup_ad);
    BYTE b;
    for(bool bStop=false;!bStop&&ok;)
    {
      if(!FREAD(&b,1,1,fpPrg))
      {
        TRACE("%s\n","ERROR");
        ok=false;
      }
      else if(b==0)
        bStop=true;
      else if(b==1)
        fixup_ad+=254;
      else
      {
        if(b&1)
        {
          TRACE("%d %s\n",b,"ERROR");
          ok=false;
        }
        fixup_ad+=b;
        GetSymbolsSub(textbase,ad,fixup_ad);
      }
    }//nxt
  }
  FSEEK(fpPrg,oldpos,SEEK_SET); // be polite
  return ok; // not used
}


void HandlePrgCopy() {
  MEM_ADDRESS basepage=pc-0x100; // or (SP+4)
  Tos.GetSymbols(Tos.fpPrgCopy,basepage);
  FCLOSE(Tos.fpPrgCopy);
  Tos.fpPrgCopy=NULL;
  Tos.TrackSymbols=0;
}


void TTos::TraceBasepage(MEM_ADDRESS const bp) {
  // dump basepage info

/*The GEMDOS BASEPAGE structure has the following members:
Name Offset Meaning
p_lowtpa 0x00 This LONG contains a pointer to the Transient
Program Area (TPA).
p_hitpa 0x04 This LONG contains a pointer to the top of the
TPA + 1.
p_tbase 0x08 This LONG contains a pointer to the base of
the text segment
p_tlen 0x0C This LONG contains the length of the text
segment.
p_dbase 0x10 This LONG contains a pointer to the base of
the data segment.
p_dlen 0x14 This LONG contains the length of the data
segment.
p_bbase 0x18 This LONG contains a pointer to the base of
the BSS segment.
p_blen 0x1C This LONG contains the length of the BSS
segment.
p_dta 0x20 This LONG contains a pointer to the
processes’ DTA.
p_parent 0x24 This LONG contains a pointer to the
processes’ parent’s basepage.
p_reserved 0x28 This LONG is currently unused and is
reserved.
p_env 0x2C This LONG contains a pointer to the
processes’ environment string.
p_undef 0x30 This area contains 80 unused, reserved bytes.
p_cmdlin 0x80 This area contains a copy of the 128 byte
command line image.
*/
  TRACE("Basepage %06X p_lowtpa %06X p_hitpa %06X p_tbase %06X p_tlen %06X p_dbase %06X p_dlen %06X\n",
    bp,SafeLPeek(bp),SafeLPeek(bp+4),SafeLPeek(bp+8),SafeLPeek(bp+12),SafeLPeek(bp+16),SafeLPeek(bp+20));
  TRACE("p_bbase %06X p_blen %06X p_dta %06X p_parent %06X p_env %06X\n",
    SafeLPeek(bp+24),SafeLPeek(bp+28),SafeLPeek(bp+32),SafeLPeek(bp+36),SafeLPeek(bp+40));
  EasyStr cmdline=ReadStringFromMemory(bp+0x80+1,128); // need to read memory backwards
  TRACE("p_cmdlin %s\n",cmdline.Text);
}

#endif//#if defined(SSE_DEBUG_SYMBOLS)


///////////////
// Cartridge //
///////////////

bool load_cart(char* const filename) { // true: succeeded (402R10)
/*  Loading a ROM cartridge.
    Steem original format STC has 4 null bytes at the start, then the 128 KB
    of the cartridge.
    Now we accept files where there are no extra null bytes
    We also accept 64KB cartridges (like test kits).
    We recognise the MV16 and RP16 sound cartridges (our custom, fake dumps, condition SSE_SOUND_CARTRIDGE).
    We can recognise the fake Cubase 2 cartridge (our custom, fake dump, condition SSE_DONGLE_CUBASE2).
    We recognise the fake Cubase 3 cartridge (our custom, fake dump, condition SSE_DONGLE_CUBASE3).
*/
  bool failed=false;
#if defined(SSE_CARTRIDGE_ACTIVE)
  CartridgeCheck=NULL;
#endif
  SSEConfig.mv16=SSEConfig.mr16=false;
#if defined(SSE_DONGLE_CUBASE2)
  SSEConfig.Cubase2Cart=false;
#endif
#if defined(SSE_DONGLE_CUBASE3)
  SSEConfig.Cubase3Cart=false;
#endif
  FILE *fp=fopen(filename,"rb");
  if(fp==NULL)
    failed=true;
  else
  {
    LONG const FileLen=(LONG)GetFileLength(fp); //can be 64KB, 128KB, 128KB+4bytes
    LONG Len=FileLen;
    DWORD FirstBytes;
    int offset=0;
    switch(FileLen) {
    case 64*1024:
      offset=FileLen;
      break;
    case 128*1024:
      break;
    case 128*1024+4: // legacy Steem format
      Len-=4;
      FREAD(&FirstBytes,4,1,fp);
      if(FirstBytes) // must be 0, don't ask why
        failed=true;
      break;
    default:
      TRACEB("??? Cartridge file size %d bytes\n",FileLen);
      failed=true;
    }
    if(!failed)
    {
      FREAD(&FirstBytes,4,1,fp);
      SWAP_LITTLE_ENDIAN_DWORD(FirstBytes);
      if(0) {}
#if defined(SSE_SOUND_CARTRIDGE)
      else if(FirstBytes==0x3631564D) // "MV16"
        SSEConfig.mv16=true;
      else if(FirstBytes==0x3631524D) // "MR16"
        SSEConfig.mv16=SSEConfig.mr16=true;
#endif
#if defined(SSE_DONGLE_CUBASE2)
      else if(FirstBytes==0x32425543) // "CUB2" => check on every UDS
        SSEConfig.Cubase2Cart=true;
#endif
#if defined(SSE_DONGLE_CUBASE3)
      else if(FirstBytes==0x33425543) // "CUB3" => check on every cartridge read
        SSEConfig.Cubase3Cart=true;
#endif
      if(cart_save)
        cart=cart_save;
      cart_save=NULL;
#if defined(SSE_CARTRIDGE_ACTIVE2)
      if(cart)
        VirtualFree(cart,0,MEM_RELEASE);
      cart=(BYTE*)VirtualAlloc(NULL,128*1024,MEM_COMMIT | MEM_RESERVE,PAGE_EXECUTE_READWRITE);
#else
      if(cart)
        delete[] cart;
      cart=new BYTE[128*1024]; // even if cartridge is smaller
#endif
      memset(cart,0xFF,128*1024);
      FSEEK(fp,-4,SEEK_CUR); //hehe
      for(int bn=Len-1;bn>=0;bn--)
        FREAD(cart+bn+offset,1,1,fp); // backwards
#if defined(SSE_CARTRIDGE_ACTIVE2) // executable code inside stc file!
      if(0
#if defined(SSE_DONGLE_CUBASE2)
        ||SSEConfig.Cubase2Cart
#endif
#if defined(SSE_DONGLE_CUBASE3)
        ||SSEConfig.Cubase3Cart
#endif
        )
      {
        int ProcOffset,ProcLen;
#ifdef UNIX
        FSEEK(fp,0x20+SSE_BITNESS,SEEK_SET); // one cartridge can be used by all builds
#else
        FSEEK(fp,SSE_BITNESS,SEEK_SET);
#endif
        FREAD(&ProcOffset,1,4,fp);
        SWAP_BIG_ENDIAN_DWORD(ProcOffset);
        FREAD(&ProcLen,1,4,fp);
        SWAP_BIG_ENDIAN_DWORD(ProcLen);
        FSEEK(fp,ProcOffset,SEEK_SET);
        FREAD(cart+ProcOffset,1,ProcLen,fp); // forwards
        CartridgeCheck=(WORD(*)(DWORD ad,BYTE *rpin))(cart+ProcOffset);
      }
#endif
#if defined(SSE_DONGLE_CUBASE2_BUILD)
      if(SSEConfig.Cubase2Cart)
        CartridgeCheck=&CartridgeCheckCubase2;
#endif
#if defined(SSE_DONGLE_CUBASE3_BUILD)
      if(SSEConfig.Cubase3Cart)
        CartridgeCheck=&CartridgeCheckCubase3;
#endif
#ifndef BIG_ENDIAN_PROCESSOR
      Cart_End_minus_1=cart+(128*1024-1);
      Cart_End_minus_2=Cart_End_minus_1-1;
      Cart_End_minus_4=Cart_End_minus_1-3;
#endif
      fclose(fp);
      DWORD crc32=GetCRCFromFile(filename);
      TRACE("%s %s %s %X\n","Load",CHECKPATH(filename),"CRC32",crc32);
    }
  }
  return !failed;
}


#if defined(SSE_CARTRIDGE_ACTIVE)
BYTE CartridgeData[CARTRIDGE_DATA_SIZE];
WORD (*CartridgeCheck)(DWORD ad,BYTE *rpin);
#endif
