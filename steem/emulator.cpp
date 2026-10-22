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

DOMAIN: Emu
FILE: emulator.cpp
DESCRIPTION: Miscellaneous core emulator functions. An important function is
init_timings that sets up all Steem's counters and clocks. Also included are
the code for Steem's agenda system that schedules tasks to be performed at
the end of scanlines, intercept functions.
Some emulation objects are instantiated here.
v401: Function that collects statistics displayed in the infobox.
v410: CPU cache of the Mega STE (v420: also for Mega ST)
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <computer.h>
#include <draw.h>
#include <display.h>
#if defined(SSE_VID_STVL1)
#include <interface_stvl.h>
#endif
#include <harddiskman.h>
#include <caps/CapsPlug.h>
#include <osd.h>
#include <diskman.h>
#include <key_table.h>

#if defined(DEBUG_BUILD) && defined(PEEK_RANGE_TEST)

#define RANGE_CHECK_MESSAGE(hi,len,hiadd) if (ad<0 || (ad+(len))>=((hi)+(hiadd))) RangeError(ad,hi-len)

void RangeError(DWORD &ad,DWORD hi_ad) {
//  ad/=0;
  ad=hi_ad-1;
}

#ifndef BIG_ENDIAN_PROCESSOR
BYTE& PEEK(DWORD ad){ RANGE_CHECK_MESSAGE(mem_len+MEM_EXTRA_BYTES,0,0);return *LPBYTE(Mem_End_minus_1-ad); }
WORD& DPEEK(DWORD ad){ RANGE_CHECK_MESSAGE(mem_len+MEM_EXTRA_BYTES,1,0);return *LPWORD(Mem_End_minus_2-ad); }
DWORD& LPEEK(DWORD ad){ RANGE_CHECK_MESSAGE(mem_len+MEM_EXTRA_BYTES,3,0);return *LPDWORD(Mem_End_minus_4-ad); }
BYTE* lpPEEK(DWORD ad){ RANGE_CHECK_MESSAGE(mem_len+MEM_EXTRA_BYTES,0,0);return LPBYTE(Mem_End_minus_1-ad); }
WORD* lpDPEEK(DWORD ad){ RANGE_CHECK_MESSAGE(mem_len+MEM_EXTRA_BYTES,1,0);return LPWORD(Mem_End_minus_2-ad); }
DWORD* lpLPEEK(DWORD ad){ RANGE_CHECK_MESSAGE(mem_len+MEM_EXTRA_BYTES,3,0);return LPDWORD(Mem_End_minus_4-ad); }

BYTE& ROM_PEEK(DWORD ad){ RANGE_CHECK_MESSAGE(tos_len,0,0);return *LPBYTE(Rom_End_minus_1-ad); }
WORD& ROM_DPEEK(DWORD ad){ RANGE_CHECK_MESSAGE(tos_len,1,0);return *LPWORD(Rom_End_minus_2-ad); }
DWORD& ROM_LPEEK(DWORD ad){ RANGE_CHECK_MESSAGE(tos_len,3,0);return *LPDWORD(Rom_End_minus_4-ad); }
BYTE* lpROM_PEEK(DWORD ad){ RANGE_CHECK_MESSAGE(tos_len,0,0);return LPBYTE(Rom_End_minus_1-ad); }
WORD* lpROM_DPEEK(DWORD ad){ RANGE_CHECK_MESSAGE(tos_len,1,2);return LPWORD(Rom_End_minus_2-ad); }
DWORD* lpROM_LPEEK(DWORD ad){ RANGE_CHECK_MESSAGE(tos_len,3,0);return LPDWORD(Rom_End_minus_4-ad); }

BYTE& CART_PEEK(DWORD ad){ RANGE_CHECK_MESSAGE(128*1024,0,0);return *LPBYTE(Cart_End_minus_1-ad); }
WORD& CART_DPEEK(DWORD ad){ RANGE_CHECK_MESSAGE(128*1024,1,0);return *LPWORD(Cart_End_minus_2-ad); }
DWORD& CART_LPEEK(DWORD ad){ RANGE_CHECK_MESSAGE(128*1024,3,0);return *LPDWORD(Cart_End_minus_4-ad); }
BYTE* lpCART_PEEK(DWORD ad){ RANGE_CHECK_MESSAGE(128*1024,0,0);return LPBYTE(Cart_End_minus_1-ad); }
WORD* lpCART_DPEEK(DWORD ad){ RANGE_CHECK_MESSAGE(128*1024,1,2);return LPWORD(Cart_End_minus_2-ad); }
DWORD* lpCART_LPEEK(DWORD ad){ RANGE_CHECK_MESSAGE(128*1024,3,0);return LPDWORD(Cart_End_minus_4-ad); }
#endif//#ifndef BIG_ENDIAN_PROCESSOR

#endif


#if defined(SSE_LOGSECTIONS_FOR_ALL) //?

DWORD SafeLPeek(MEM_ADDRESS const ad) {
  DWORD l=SafeDPeek(ad)<<16;
  l|=SafeDPeek(ad+2);
  return l;
}

#else

DWORD SafeLPeek(MEM_ADDRESS const ad) {
  DWORD val=0;
  if(ad-2<himem && !(ad&1))
    val=LPEEK(ad);
  return val;
}

#endif

void SafePoke(MEM_ADDRESS const ad,BYTE const val) {
  if(ad<himem)
    PEEK(ad)=val;
}

void SafeDPoke(MEM_ADDRESS const ad,WORD const val) {
  if(ad<himem && !(ad&1))
    DPEEK(ad)=val;
}

void SafeLPoke(MEM_ADDRESS const ad,DWORD const val) {
  if(ad-2<himem && !(ad&1))
    LPEEK(ad)=val;
}


/////////
// Bus //
/////////

#if defined(SSE_VID_STVL1)
BYTE &bus_mask=Stvl.bus_mask; // using struct Stvl's
#else
BYTE bus_mask;
MEM_ADDRESS abus;
DU16 udbus;
WORD& dbus=udbus.d16;
BYTE& dbusl=udbus.d8[LO];
BYTE& dbush=udbus.d8[HI];
#endif


/////////////
// Agendas //
/////////////

DWORD hbl_count=0;
//TODO
const WORD hbl_per_second[4]={GLU_PAL_SCANLINES*PAL_HZ,GLU_NTSC_SCANLINES*NTSC_HZ,
                              GLU_MONO_SCANLINES*MONO_HZ,GLU_MONO_SCANLINES*MONO_HZ};
  //TGlue::SCANLINES50*50,TGlue::SCANLINES60*60,
  //TGlue::SCANLINES71*71,TGlue::SCANLINES71*71}; //HBL_PER_SECOND
TAgenda agenda[MAX_AGENDA_LENGTH];
WORD agenda_length=0;
DWORD agenda_next_time=0x7fffffff;
LPAGENDAPROC agenda_list[]={
  agenda_fdc_spun_up,
  agenda_fdc_motor_flag_off,
  agenda_fdc_finished,
  agenda_fdc_seek,
  agenda_fdc_readwrite_sector,
  agenda_fdc_read_address,
  agenda_fdc_read_track,
  agenda_fdc_write_track,
  agenda_serial_sent_byte,
  agenda_serial_break_boundary,
  agenda_serial_loopback_byte,
  agenda_midi_replace,
  agenda_check_centronics_interrupt,
  agenda_ikbd_process,
  agenda_keyboard_reset,
  agenda_acia_tx_delay_IKBD,
  agenda_acia_tx_delay_MIDI,
#if !defined(SSE_NO_OLD_IKBD)
  ikbd_send_joystick_message,
  ikbd_report_abs_mouse,
#endif
  agenda_keyboard_replace,
  agenda_fdc_verify,
  agenda_reset,
#if defined(SSE_ACSI)
  agenda_acsi,
#endif
  (LPAGENDAPROC)1};

  
#define LOGSECTION LOGSECTION_AGENDA

int milliseconds_to_hbl(int ms) {
  return ms*HBL_PER_SECOND/1000;
}

// note: critical section disabled for emu thread
// v4.2: critical section removed

void agenda_add(LPAGENDAPROC const action,int const pause,int const param) {
#if defined(SSE_ENABLE_TRACE_LOG)
  //ASSERT(pause>=0);
  //ASSERT(action!=agenda_fdc_motor_flag_off);
  int i;
  for(i=0;i<256 && agenda_list[i]!=(LPAGENDAPROC)1;i++)
    if(agenda_list[i]==action)
      break;
  TRACE_LOG("agenda add #%d #%d %p in %d hbl data $%X\n",agenda_length,i,action,pause,param);
#endif
  ASSERTB(agenda_length<MAX_AGENDA_LENGTH);
  if(agenda_length>=MAX_AGENDA_LENGTH)
    return;
  DWORD target_time=hbl_count+pause;
  int n=0;
  while(n<agenda_length && (signed int)(agenda[n].time-target_time)>0)
    n++;
  //budge the n, n+1, ... along
  for(int nn=agenda_length;nn>n;nn--)
    agenda[nn]=agenda[nn-1];
  agenda[n].perform=action; // pointer to the function to call
  agenda[n].time=target_time;
  agenda[n].param=param;
  agenda_next_time=agenda[agenda_length].time;
  agenda_length++;
}


void agenda_delete(LPAGENDAPROC const job) {
  for(WORD n=0;n<agenda_length;n++)
  {
    if(agenda[n].perform==job)
    {
      TRACE_LOG("agenda delete #%d %p\n",n,job);
      for(int nn=n; nn<agenda_length; nn++)
        agenda[nn]=agenda[nn+1];
      agenda_length--;
      n--;
    }
  }
  agenda_next_time=(agenda_length&&agenda_length<(MAX_AGENDA_LENGTH+1)) 
                  ? agenda[agenda_length-1].time : (hbl_count-1); //wait 42 hours
}


int agenda_get_queue_pos(LPAGENDAPROC const job) {
  int n=agenda_length-1;
  for(;n>=0;n--)
    if(agenda[n].perform==job)
      break;
  return n;
}


void agenda_acia_tx_delay_IKBD(int) {
  acia[ACIA_IKBD].tx_flag=FALSE; //finished transmitting
  if(acia[ACIA_IKBD].tx_irq_enabled) 
    acia[ACIA_IKBD].irq=TRUE;
  mfp_gpip_set_bit(MFP_GPIP_ACIA_BIT,!(acia[ACIA_IKBD].irq||acia[ACIA_MIDI].irq));
}


void agenda_acia_tx_delay_MIDI(int) {
  acia[ACIA_MIDI].tx_flag=FALSE; //finished transmitting
  if(OPTION_C1)
  {
    acia[ACIA_MIDI].sr|=BIT_1; // TDRE
    if((acia[ACIA_MIDI].cr&BIT_5)&&!(acia[ACIA_MIDI].cr&BIT_6)) // IRQ transmit enabled
    {
      acia[ACIA_MIDI].sr|=BIT_7; // IRQ
      mfp_gpip_set_bit(MFP_GPIP_ACIA_BIT,!((acia[ACIA_IKBD].sr&BIT_7)||(acia[ACIA_MIDI].sr&BIT_7)));
    }
    return;
  }
#if !defined(SSE_NO_OLD_IKBD)
  if(acia[ACIA_MIDI].tx_irq_enabled) 
    acia[ACIA_MIDI].irq=TRUE;
  mfp_gpip_set_bit(MFP_GPIP_ACIA_BIT,!(acia[ACIA_IKBD].irq||acia[ACIA_MIDI].irq));
#endif
}

#undef LOGSECTION


/////////////
// Timings //
/////////////

int sys_cycles; // was cpu_cycles
COUNTER_VAR sys_timer; // was cpu_timer
DWORD nSysCyclesPerSecond=CPU_CLOCK_STE_PAL; // C
DWORD new_n_cpu_cycles_per_second=0,n_millions_cycles_per_sec=8;
double cpu_cycles_multiplier=1.0; // used by 6301
const int EIGHT_MILLION=8000000*TICKS8;

void init_timings() {
  TRACE_SECTION(LOGSECTION_INIT,"init_timings()\n");
  // don't do anything to agendas here!
  Fdc.str&=~FDC_STR_MO; //?
  video_first_draw_line=0;
  video_last_draw_line=shifter_y;
  Glue.VideoFreq=(MONO_MONITOR)?MONO_HZ:NTSC_HZ;
  VideoFreqAtStartOfVbl=Glue.VideoFreq;
  CALC_VIDEO_FREQ_IDX;
  ScreenResAtStartOfVbl=screen_res;
  CyclesPerScanlineAtStartOfVbl=CyclesPerScanline[VideoFreqIdx];
  Glue.hbl_pending=true;
  if(OPTION_C3)
  {
    event_vector=event_dummy;
    time_of_next_event=EIGHT_MILLION;
  }
  else
    Glue.GetNextVideoEvent();
  SetTimingFunctions();
  sys_cycles=(int)time_of_next_event;
  Glue.CurrentScanline.Cycles=CyclesPerScanlineAtStartOfVbl;
  //ASSERT(Glue.CurrentScanline.Cycles>=224);
  //sys_timer=time_of_next_event;
  sys_timer=0;
  sys_time_of_first_mfp_tick=shifter_cycle_base=sys_time_of_last_vbl=a_s_t=A_S_T;
  time_of_last_hbl_interrupt=time_of_last_vbl_interrupt=a_s_t;
  time_of_next_timer_b=sys_time_of_last_vbl+160000*TICKS8;
  scan_y=-TopScanlines[VideoFreqIdx];
  Mfp.InitTimers();
  shifter_draw_pointer=vbase;
  VCountAtHSync=shifter_draw_pointer;
  shifter_tick8=shifter_hscroll; //start by drawing this pixel
  left_border=LeftBorderSize;
  right_border=RightBorderSize;
  scanline_drawn_so_far=0;
  TimeOfHSyncOff=0;
  GlueFreqChangeIdx=0;
  for(int n=0;n<NMODECHANGES;n++) 
  {
    glue_freq_change[n]=Glue.VideoFreq;
    GlueFreqChangeTime[n]=a_s_t;
  }
  ikbd_joy_poll_line=0;
  ste_sound_on_this_screen=false;
  SteSoundOutputCountdown=SteSoundSamplesCountdown=0;
  SteSoundChannelBufIdx=0;
#if USE_PASTI
  pasti_update_time=a_s_t+EIGHT_MILLION; // future
#endif
  hbl_count=0;
  Fdc.update_time=Fdc.current_time=a_s_t;
  FloppyDrive[DRIVE_A].time_of_next_ip=FloppyDrive[DRIVE_B].time_of_next_ip=a_s_t;
}


///////////
// Video //
///////////

COUNTER_VAR sys_time_of_last_vbl,shifter_cycle_base;
COUNTER_VAR TimeOfHSyncOff; // was sys_timer_at_start_of_hbl
COUNTER_VAR GlueFreqChangeTime[NMODECHANGES],ShifterModeChangeTime[NMODECHANGES];
BYTE glue_freq_change[NMODECHANGES],shifter_mode_change[NMODECHANGES];
int video_first_draw_line,video_last_draw_line;
int CyclesPerScanlineAtStartOfVbl;
#if defined(SSE_VID_D3D_VSYNC)
DWORD MasterSync=60; // ST (no VSync) or PC (VSync) frequency
#endif


const WORD CyclesPerScanline8MHz[4]={GLU_SCANLINE_CYCLES_50HZ,
                                     GLU_SCANLINE_CYCLES_60HZ,
                                     GLU_SCANLINE_CYCLES_72HZ,
                                     128};

int CyclesPerScanline[4]={GLU_SCANLINE_CYCLES_50HZ,
                          GLU_SCANLINE_CYCLES_60HZ,
                          GLU_SCANLINE_CYCLES_72HZ,
                          128};
MEM_ADDRESS VCountAtHSync;
SHORT scan_y;
SHORT TimerBLinecycle;
BYTE ScreenResAtStartOfVbl=0;
BYTE VideoFreqAtStartOfVbl=TGlue::FREQ_IDX_60;
BYTE VideoFreqIdx=TGlue::FREQ_IDX_60;
BYTE screen_res=LORES;
BYTE video_mixed_output=0;
BYTE GlueFreqChangeIdx=0,ShifterModeChangeIdx=0;

bool freq_change_this_scanline=false;
#if defined(SSE_OPTION_FASTLINEA)
bool lineA=false;
#endif
WORD HiresPixelMask=0;


//////////////////////
// Extended monitor //
//////////////////////

BYTE extended_monitor;

#ifndef NO_CRAZY_MONITOR

LONG save_r[16];
DWORD em_width=480,em_height=480;//?
MEM_ADDRESS line_a_base=0;
MEM_ADDRESS em_rte_return_address;
UINT extmon_res[EXTMON_RESOLUTIONS][3]={ 
  {800,600,1},{1024,720,1},{1024,768,1},{1280,960,1},
  {640,400,4},{800,600,4},{1024,720,4},{1024,768,4},{1280,960,4},
  {0,0,1},{0,0,4} //max screen, must be init
};
BYTE em_planes=4;

void call_a000() {
  em_rte_return_address=pc;
  //now save regs a0,a1,d0 ?
  on_rte=ON_RTE_LINEA_HACK;
  DPEEK(0)=0xa000; //SS ?
//    m68k_interrupt(LPEEK(EXCEPTION_LINE_A*4));
  UPDATE_SR;
  WORD saved_sr=SR;
  change_to_supervisor_mode();
  m68k_PUSH_L(0);
  m68k_PUSH_W(saved_sr);
  SET_PC(LPEEK(EXCEPTION_LINE_A*4));
  CLEAR_T;
  interrupt_depth++;
  memcpy(save_r,Cpu.r,16*4);
  on_rte_interrupt_depth=interrupt_depth;
}


void extended_monitor_hack() {
  em_width&=-16;
  if(line_a_base==0)
  {
    line_a_base=areg[0];
    LPEEK(0)=ROM_LPEEK(0);
    memcpy(Cpu.r,save_r,15*4); // not 16?
  }
  int real_planes=em_planes;
  if(screen_res==MEDRES)
    real_planes=2;
  SafeDPoke(line_a_base-12,(WORD)em_width);            //V_REZ_HZ -12  WORD  Horizontal pixel resolution.
  SafeDPoke(line_a_base-4,(WORD)em_height);            //V_REZ_VT -4  WORD  Vertical pixel resolution.
  SafeDPoke(line_a_base-2,(WORD)(em_width*real_planes/8)); //BYTES_LIN -2  WORD  Bytes per screen line.
  SafeDPoke(line_a_base,(WORD)real_planes);              //PLANES 0  WORD  Number of planes in the current resolution
  SafeDPoke(line_a_base+2,(WORD)(em_width*real_planes/8)); //WIDTH 2  WORD  Width of the destination form in bytes

  int h=(em_planes==1)?16:8; //height of a character
  SafeDPoke(line_a_base-40,(WORD)(em_width*real_planes*h/8)); //V_CEL_WR -40  WORD  Number of bytes between character cells
  SafeDPoke(line_a_base-44,(WORD)(em_width/8-1));        //V_CEL_MX -44  WORD  Number of text columns - 1.
  SafeDPoke(line_a_base-42,(WORD)(em_height/h-1));      //V_CEL_MY -42  WORD  Number of text rows - 1.
  if(!vdi_intout)
    Tos.HackMemoryForExtendedMonitor();
  else
  {
    SafeDPoke(line_a_base-692,(WORD)(em_width-1));
    SafeDPoke(line_a_base-690,(WORD)(em_height-1));
    SafeDPoke(vdi_intout,(WORD)(em_width-1));
    SafeDPoke(vdi_intout+2,(WORD)(em_height-1));
  }
}

#else
#define extended_monitor FALSE
#endif


////////////////
// Emu detect //
////////////////

bool emudetect_called=false;
bool emudetect_write_logs_to_printer=false;
bool emudetect_overscans_fixed=false;

void emudetect_reset() {
  emudetect_called=false;
  emudetect_write_logs_to_printer=false;
#if !defined(SSE_NO_FALCONMODE)
  emudetect_falcon_stpal.DeleteAll();
  emudetect_falcon_pcpal.DeleteAll();
  emudetect_falcon_mode=EMUD_FALC_MODE_OFF;
  emudetect_falcon_mode_size=1;
#endif
}


////////////
// Falcon //
////////////

#if !defined(SSE_NO_FALCONMODE)

#define MAKEBINW(high,low) ((BYTE(high) << 8) | BYTE(low))

BYTE emudetect_falcon_mode=EMUD_FALC_MODE_OFF;
BYTE emudetect_falcon_mode_size=0;
bool emudetect_falcon_extra_height=0;
DynamicArray<DWORD> emudetect_falcon_stpal;
DynamicArray<DWORD> emudetect_falcon_pcpal;


void emudetect_init() {
  emudetect_falcon_stpal.Resize(256);
  emudetect_falcon_pcpal.Resize(256);
}


void emudetect_falcon_palette_convert(int n) {
  if(emudetect_called==0) 
    return; // 256 mode could cause slow down, can't make it work.
  DWORD val=emudetect_falcon_stpal[n];
  emudetect_falcon_pcpal[n]=colour_convert(DWORD_B(&val,0),DWORD_B(&val,1),DWORD_B(&val,3));
}


void ASMCALL emudetect_falcon_draw_scanline(int border1,int picture,
                                            int border2,int hscroll) {
  if(emudetect_called==0||draw_lock==0) 
    return;
  int st_line_bytes=emudetect_falcon_mode_size*320*emudetect_falcon_mode;
  MEM_ADDRESS source=shifter_draw_pointer&0xffffff;
  if(source+st_line_bytes>mem_len) 
    return;
  int wh_mul=1;
  if(emudetect_falcon_mode_size==1
    &&draw_blit_source_rect.right>320+SideBorderSizeWin+SideBorderSizeWin)
    wh_mul=2;
  // border always multiple of 4 so write using longs only
  DWORD bord_col=0xffffffff;
  if(emudetect_falcon_mode==1) bord_col=emudetect_falcon_pcpal[0];
  int DestInc=4;
  for(int y=0;y<wh_mul;y++)
  {
    LPDWORD plDest=LPDWORD(draw_dest_ad);
    for(int x=0;x<wh_mul;x++)
    {
      for(int n=border1;n>0;n--)
      {
        *plDest=bord_col;
        plDest=LPDWORD(LPBYTE(plDest)+DestInc);
      }
    }
    source+=hscroll*emudetect_falcon_mode;
    if(emudetect_falcon_mode==1)
    {
      DWORD col;
      for(int n=picture;n>0;n--)
      {
        col=emudetect_falcon_pcpal[PEEK(source++)];
        for(int x=0;x<wh_mul;x++)
        {
          *(plDest++)=col;
        }
      }
    }
    else if(emudetect_falcon_mode==2)
    {
      DWORD src;
      for(int n=picture;n>0;n--)
      {
        src=DPEEK(source);source+=2;
        int red=(src & MAKEBINW(b11111000,b00000000))>>11;
        int green=(src & MAKEBINW(b00000111,b11100000))>>5;
        int blue=(src & MAKEBINW(b00000000,b00011111));
        for(int x=0;x<wh_mul;x++)
        {
          *(plDest)=((red<<(24-5))|(green<<(16-6))|(blue<<(8-5)))<<rgb32_bluestart_bit;
          plDest=LPDWORD(LPBYTE(plDest)+DestInc);
        }
      }
    }
    for(int x=0;x<wh_mul;x++)
    {
      for(int n=border2;n>0;n--)
      {
        *plDest=bord_col;
        plDest=LPDWORD(LPBYTE(plDest)+DestInc);
      }
    }
    draw_dest_ad+=draw_line_length;
  }
}

#endif


//////////
// Disc //
//////////

#if defined(SSE_DISK_CAPS) 
TCaps Caps; //this object includes a controller and 2 drives
#endif
#if defined(SSE_DISK_GHOST)
// Each drive has its own optional ghost image
// Most will use A: but e.g. Lethal Xcess could save on B:
TGhostDisk GhostDisk[2];
#endif
#if defined(SSE_DISK_STW2)
TImageSTW2 ImageSTW[3]; // STW v1 and v2 images are handled by the TImageSTW2 object
#elif defined(SSE_DISK_STW)
TImageSTW ImageSTW[3];
#endif
#if defined(SSE_DISK_SCP)
TImageSCP ImageSCP[3];
#endif
#if defined(SSE_DISK_HFE)
TImageHFE ImageHFE[3];
#endif
#if defined(SSE_DISK_STX)
TImageSTX ImageSTX[3];
#endif
THardDiskManager HardDiskMan;
#if defined(SSE_ACSI_MNGR)
TAcsiHardDiskManager AcsiHardDiskMan;
#endif
TStemdos Stemdos;
TDiskEmu DiskEmu;

bool fdc_irq=false,hdc_irq=false;

#undef LOGSECTION
#define LOGSECTION LOGSECTION_FDC


void update_disk_irq() {
  // one line goes into MFP for both floppy and hard disk controllers
  // on STF, directly, on STE via the GSTMCU
  bool disk_irq=(fdc_irq||hdc_irq);
  mfp_gpip_set_bit(MFP_GPIP_FDC_BIT,!disk_irq); // active low
}


bool TDiskEmu::WritingToDisk() {// could do this at DMA level?
  return((Fdc.cr&0xF0)==0xF0 || (Fdc.cr&0xE0)==0xA0 || (Fdc.cr&0xE0)==0xB0);
}


#if defined(SSE_ENABLE_TRACE_LOG)

void TDiskEmu::TraceRegs() {
  if(Dma.mcr&Dma.CR_HDC_OR_FDC)
    TRACE_LOG("HDC IRQ\n");
  else
  {
    TRACE_LOG("FDC %s IRQ CR %X %c:STR %X ",disk_manager[LastManager],Fdc.cr,'A'+DRIVE,Fdc.str);
    TraceStatus(Fdc.str);
    TRACE_LOG("TR %d (CYL %d) sr %d DR %d\n",Fdc.tr,track,Fdc.sr,Fdc.dr);
  }
}


void TDiskEmu::TraceStatus(BYTE MyStr) {
  int type=Fdc.CommandType();
  TRACE_LOG("( ");
  if(MyStr&0x80)
    TRACE_LOG("MO "); //Motor On
  if(MyStr&0x40)
    TRACE_LOG("WP "); // write protect
  if(MyStr&0x20)
  {
    if(type==1)
      TRACE_LOG("SU "); // Spin-up (meaning up to speed)
    else
      TRACE_LOG("RT "); //Record Type (1=deleted data)
  }
  if(MyStr&0x10)
  {
    if(type==1)
      TRACE_LOG("SE "); //Seek Error
    else
      TRACE_LOG("RNF "); //Record Not Found
  }
  if(MyStr&0x08)
    TRACE_LOG("CRC "); //CRC Error
  if(MyStr&0x04)
  {
    if(type==1)
      TRACE_LOG("T0 "); // track zero
    else
      TRACE_LOG("LD "); //Lost Data, normally impossible on ST
  }
  if(MyStr&0x02)
  {
    if(type==1 || type==4)
      TRACE_LOG("IP "); // index
    else
      TRACE_LOG("DRQ "); // data request
  }
  if(MyStr&0x01)
    TRACE_LOG("BSY "); // busy
  TRACE_LOG(") "); 
}

#endif//#if defined(SSE_ENABLE_TRACE_LOG)


void TDiskEmu::Update(int const op) { // 0=just like that 1=irq 2=change disk
  bool on_irq=(op==1);
  bool on_disk_change=(op==2);
  if(!LastManager) // resume snapshot -> no LastManager -> wrong track
    LastManager=FloppyDrive[DRIVE].ImageType.Manager;
  switch(LastManager) {
    case MNGR_STEEM:
    case MNGR_WD1772:
      track=FloppyDrive[DRIVE].track;
      maxtrack=SSEOptions.DiscMaxTrack;
      break;
#if USE_PASTI
    case MNGR_PASTI:
    {
      pastiPEEKINFO ppi;
      pasti->Peek(&ppi);
      Fdc.cr=ppi.commandReg;
      Fdc.str=ppi.statusReg;
      Fdc.tr=ppi.trackReg;
      Fdc.sr=ppi.sectorReg;
      Fdc.dr=ppi.dataReg;
      track=(DRIVE) ?  ppi.drvbTrack : ppi.drvaTrack;
      maxtrack=82;
      break;
    }
 #endif
 #ifdef SSE_DISK_CAPS
    case MNGR_CAPS:
    {
      int ext=0;
      Fdc.cr=(BYTE)CapsFdcGetInfo(cfdciR_Command,&Caps.fdc,ext);
      Fdc.str=(BYTE)CapsFdcGetInfo(cfdciR_ST,&Caps.fdc,ext);
      Fdc.tr=(BYTE)CapsFdcGetInfo(cfdciR_Track,&Caps.fdc,ext);
      Fdc.sr=(BYTE)CapsFdcGetInfo(cfdciR_Sector,&Caps.fdc,ext);
      Fdc.dr=(BYTE)CapsFdcGetInfo(cfdciR_Data,&Caps.fdc,ext);
      track=(BYTE)Caps.Drive[DRIVE].track;
      maxtrack=CAPSDRIVE_35DD_HST;
      break;
    }
#endif    
    case MNGR_PRG:
      break; //?
  }
  if(on_irq)
  {
    FloppyDrive[DRIVE].track=track;
#if defined(SSE_OSD_DEBUGINFO)
    if((Fdc.str&FDC_STR_CRC))
    {
      TRACE_OSD_DBG("CRC"); 
    }
#endif
  }
  // update all managers in case we change disk
  if(on_disk_change)
  {
#ifdef SSE_DISK_CAPS
    Caps.fdc.r_command=Fdc.cr;
    Caps.fdc.r_track=Fdc.tr;
    Caps.fdc.r_sector=Fdc.sr;
    Caps.fdc.r_data=Fdc.dr;
    Caps.Drive[DRIVE].track=track;
#endif
  }

  if(!op&&(DRIVE<DiskMan.nFloppyDrives))
  {
    // update track info, for status bar or OSD
#ifdef SSE_DEBUG_ // add current command (CR)
    sprintf(sTrackinfo,"%2X-%C:%d-%02d-%02d",Fdc.cr,'A'+DRIVE,CURRENT_SIDE,track,Fdc.sr);
#else
    if(OPTION_OSD_DEBUGINFO) // add current command (CR)
      sprintf(sTrackinfo,"%2X-%C:%d-%02d-%02d",Fdc.cr,'A'+DRIVE,CURRENT_SIDE,track,Fdc.sr);
    else
      sprintf(sTrackinfo,"%C:%u-%02u-%02u",'A'+DRIVE,CURRENT_SIDE,track,Fdc.sr);
#endif
#if defined(SSE_GUI_STATUS_BAR)
#if defined(SSE_GUI_STATUS_BAR_DRAW_CAPS)
    if(/*OPTION_DRIVE_INFO &&*/ OPTION_STATUS_BAR
      && (SSEConfig.StatusBarMask&(1<<SB_PART_CAPS)))
    {
      BOOL on=(Fdc.str&FDC_STR_MO);
      if(on && strcmp(sTrackinfo,status_bar_text[SB_PART_CAPS]))
      {
        strcpy(status_bar_text[SB_PART_CAPS],sTrackinfo);
        StatusBar=true;
        UPDATE_STATUS_BAR_PART(SB_PART_CAPS);
      }
      else if(!on && StatusBar) // only once
      {
        SetTimer(StemWin,STATUSBAR_TIMER_ID,3000,NULL); //3s to read
        StatusBar=false;
      }
    }
#else
    static bool bTimerSent;
    if(OPTION_STATUS_BAR
#if defined(SSE_GUI_STATUS_BAR_CLICKABLE)
       && (SSEConfig.StatusBarMask&(1<<SB_PART_CAPS))
#else
       && !(OPTION_DRIVE_INFO) // not on statusbar if on OSD
#endif
       )
    {
      BOOL on=(Fdc.str&FDC_STR_MO);
      if(on && strcmp(sTrackinfo,status_bar_text[SB_PART_CAPS]))
      {
        strcpy(status_bar_text[SB_PART_CAPS],sTrackinfo);
        PostMessage(hStatusBar,SB_SETTEXT,SB_PART_CAPS,(LPARAM)sTrackinfo);
        bTimerSent=false;
      }
      else if(!on && !bTimerSent) // only once
      {
        SetTimer(StemWin,STATUSBAR_TIMER_ID,3000,NULL); //3s to read
        bTimerSent=true;
      }
    }
#endif
#endif//#if defined(SSE_GUI_STATUS_BAR)
  }
}

#if USE_PASTI

bool TDiskEmu::PastiOperation() { // used by io_read() & io_write()
  // hacky but what can we do?
  bool Probably=hPasti&&(pasti_active||FloppyDrive[DRIVE].ImageType.Extension==EXT_STX
                                       && !AcsiBsy && LastManager==MNGR_PASTI);
  return Probably;
}

#endif


///////////
// Misc. //
///////////

int ioaccess=0;
int interrupt_depth=0;
int on_rte;
int on_rte_interrupt_depth;
MEM_ADDRESS old_pc,pc_high_byte;


//////////
// Mega //
/////////

#if defined(SSE_MEGA_RTC)
/*  Partial emulation of the Ricoh Real Time Clock of the Mega ST and Mega STE.
    The time will always read as your PC time, whatever you write.
-------+-----+-----------------------------------------------------+----------
##############Realtime Clock                                       ###########
-------+-----+-----------------------------------------------------+----------
$FFFC21|byte |S_Units                                              |???
$FFFC23|byte |S_Tens                                               |???
$FFFC25|byte |M_Units                                              |???
$FFFC27|byte |M_Tens                                               |???
$FFFC29|byte |H_Units                                              |???
$FFFC2B|byte |H_Tens                                               |???
$FFFC2D|byte |Weekday                                              |???
$FFFC2F|byte |Day_Units                                            |???
$FFFC31|byte |Day_Tens                                             |???
$FFFC33|byte |Mon_Units                                            |???
$FFFC35|byte |Mon_Tens                                             |???
$FFFC37|byte |Yr_Units                                             |???
$FFFC39|byte |Yr_Tens                                              |???
$FFFC3B|byte |Cl_Mod                                               |???
$FFFC3D|byte |Cl_Test                                              |???
$FFFC3F|byte |Cl_Reset                                             |???
*/

// masks of valid bits, non-valid read as zero (unimportant)

// and high byte?

BYTE trp5c15_mask[2][16]={{0xF,0x7,0xF,0x7,0xF,0x3,0x7,0xF,0x3,0xF,0x1,0xF,0xF,0xD,0xF,0xF},
                          {0x7,0x1,0xF,0x7,0xF,0x3,0x7,0xF,0x3,0x0,0x1,0x3,0x0,0xD,0xF,0xF}};

TRp5c15 MegaRtc;

#undef LOGSECTION
#define LOGSECTION LOGSECTION_IKBD

WORD TRp5c15::Read(MEM_ADDRESS const addr) {
  int bank=reg[0][0xD]&1;
  int regn=(addr-0xFFFC20)/2;
  WORD x=reg[bank][regn];
  if(!bank) // bank 1 used for tests, but diagnostic cartridge also uses 0
  {         // which we don't emulate
    time_t t=time(NULL);
    struct tm *lpTime=localtime(&t);
    switch(regn) {
    case 0x0: // S_Units                                              |???
      x=(WORD)lpTime->tm_sec%10;
      break;
    case 0x1: // S_Tens                                               |???
      x=(WORD)lpTime->tm_sec/10;
      break;
    case 0x2: // M_Units                                              |???
      x=(WORD)lpTime->tm_min%10;
      break;
    case 0x3: // M_Tens                                               |???
      x=(WORD)lpTime->tm_min/10;
      break;
    case 0x4: // H_Units                                              |???
      x=(WORD)lpTime->tm_hour%10;
      break;
    case 0x5: // H_Tens                                               |???
      x=(WORD)lpTime->tm_hour/10;
      break;
    case 0x6: // Weekday                                              |???
      x=(WORD)lpTime->tm_wday;
      break;
    case 0x7: // Day_Units                                            |???
      x=(WORD)lpTime->tm_mday%10;
      break;
    case 0x8: // Day_Tens                                             |???
      x=(WORD)lpTime->tm_mday/10;
      break;
    case 0x9: // Mon_Units                                            |???
      x=(WORD)(lpTime->tm_mon+1)%10;
      break;
    case 0xA: // Mon_Tens                                             |???
      x=(WORD)(lpTime->tm_mon+1)/10;
      break;
    case 0xB: // Yr_Units                                             |???
      x=(WORD)(lpTime->tm_year+1900-1980)%10;
      break;
    case 0xC: // Yr_Tens                                              |???
      x=(WORD)(lpTime->tm_year+1900-1980)/10;
      break;
    case 0xD: // Cl_Mod                                               |???
    case 0xE: // Cl_Test                                              |???
    case 0xF: // Cl_Reset                                             |???
      break;
    }
  }
  x&=trp5c15_mask[bank][regn];
  TRACE_LOG("PC %X read addr %X reg %d-%X = %d\n",old_pc,addr,bank,regn,x);
  return x;
}


void TRp5c15::Write(MEM_ADDRESS const addr,BYTE const io_src_b) {
  int bank=reg[0][0xD]&1;
  int regn=(addr-0xFFFC20)/2;
  reg[bank][regn]=io_src_b&trp5c15_mask[bank][regn];
  if(regn>=0xD&&regn<=0xF)
    reg[1-bank][regn]=reg[bank][regn];
  TRACE_LOG("PC %X write addr %X reg %d-%X = %d\n",old_pc,addr,bank,regn,reg[bank][regn]);
}

#endif


///////////////////////////////////////////
// Mega CPU 16K cache + 16MHz ROM access //
///////////////////////////////////////////

#if defined(SSE_MEGA)

TCpu16::~TCpu16() {
  if(pAdlist)
    free(pAdlist);
  if(pIsCached)
    free(pIsCached);
}  


void TCpu16::Add(MEM_ADDRESS ad) {
  // only ~16MHz and cache enabled
  // we also cache <$00000008 which is an approximation
#if defined(SSE_MEGA16)
  if((ScuReg&3)!=3 || ad>himem || pAdlist==NULL)
#else
  if(ScuReg!=3 || ad>himem || pAdlist==NULL)
#endif
    return;
  ad>>=1;
  if(!pIsCached[ad])
  {
    pAdlist[iPos++]=ad;
    if(iPos==8*1024)
      iPos=0;
    pIsCached[ad]=true;
    pIsCached[pAdlist[iPos]]=false; // FIFO
  }
}


BOOL TCpu16::IsFast(MEM_ADDRESS const ad) {
  BYTE b=(BYTE)(ad>>16);
  switch(Glue.Decode[b]) {
  case TGlue::STRAM_OR_ROM:
  case TGlue::STRAM:
  case TGlue::STRAM_C2:
  case TGlue::STRAM_CHECK:
  case TGlue::STRAM_CHECK_C2:
  case TGlue::ROM_CHECK:
    // We use a lookup table, a lookup function is too slow
    if((ScuReg&BIT_0) && pIsCached && pIsCached[ad>>1])
      return TRUE;
    break;
  case TGlue::ROM:
  case TGlue::CART:
  case TGlue::CART2:
  case TGlue::CART3:
    if(ScuReg&BIT_1)
      return TRUE; // ROM is much faster than RAM, no need for a cache
  }
  return FALSE;
}


void TCpu16::Reset() { // too slow for live emu, called at reset and model change
#if defined(SSE_MEGA16)
  if(!SSEConfig.Mega)
#else
  if(!IS_MEGASTE)
#endif
    return;
  iPos=0;
  if(pAdlist)
    ZeroMemory(pAdlist,8*1024*sizeof(MEM_ADDRESS)); // 8K * address tag size
  if(pIsCached)
    ZeroMemory(pIsCached,1024*1024*4/2*sizeof(bool));
#if defined(SSE_MEGA16)
  // survive reset if player used turbo button
  if(ScuReg!=7||!SSEConfig.Mega)
#endif
  ScuReg=0;
}


void TCpu16::Ready(bool const enabling) {
  // enabled for Mega STE respectless of cache use, the goal here is not
  // to waste memory when using another model
  if(enabling && pAdlist==NULL) 
  {
    pAdlist=(MEM_ADDRESS*)calloc(8*1024,sizeof(MEM_ADDRESS)); // 32K
    pIsCached=(bool*)calloc(1024*1024*4/2,sizeof(bool)); // 2MB
    Reset();
  }
  else if(!enabling && pAdlist)
  {
    free(pAdlist);
    pAdlist=NULL;
    free(pIsCached);
    pIsCached=NULL;
  }
}

#endif//#if defined(SSE_MEGA)


///////////
// Stats //
///////////

#if defined(SSE_STATS)
/*  v4.0.1's unique feature, it was much fun doing it and it can be
    quite revealing, like FPS.
    The report is rich text RTF in Windows, plain text in Linux.
    RTF format is verbose so that looks like a lot of code but the idea is simple.
    While running, object Stats is updated at low overhead cost.
    When stopped, if the Status page is open, the report is saved as an RTF
    file then loaded in the page.
    When the Status page gets open, the report is remade (also while running,
    but then some stats are not updated).
*/

TStats Stats;
TStatsStatic StatsStatic;
EasyStr sStatsFile=STEEM_STATS_FILENAME;


void TStats::Report() {
  EasyStr stats_file=UsersPath+SLASH+sStatsFile;
  FILE *fp=fopen(stats_file, "w");
  if(fp)
  {
#if defined(SSE_STATS_RTF)
    StartRtf(fp);
    char nl[]="\\par\n";
//    char sSte[]="\\b (STE)\\b0";
//    char sSte[]="\\cf6 (STE) \\cf0 "; //red
    char sSte[]="(STE) ";
#else //non RTF: Linux build
    char nl[]="\n";
    char sSte[]="(STE) ";
#endif
    char s1frame[]="1 frame";
#if defined(SSE_STATS_RTF)
    fprintf(fp,"\\b %s STATISTICS\\b0 %s",gAppName,nl);
#else
    fprintf(fp,"%s STATISTICS%s",gAppName,nl);
#endif
    fprintf(fp,"%s %uK T%X(%u) C%u C%d%s", // VLE%d WU%d-%d Hacks %d Dongle %d\n",
            st_model_name[ST_MODEL],mem_len>>10,tos_version,SSEConfig.TosLanguage,
            OPTION_C1,(OPTION_VLE?OPTION_VLE+1:0),nl);
    {
    DWORD h,m,s,ht,mt,st;
    ms_to_hms(run_time,h,m,s);
    ms_to_hms(StatsStatic.TotalRuntime,ht,mt,st);
    DWORD CpuUse=tCpuUsage;
#if defined(SSE_STATS_RTF)
    fprintf(fp,"\\b Time\\b0: %02d:%02d:%02d (\\cf9 total %02d:%02d:%02d\\cf0 )%s\\b CPU load\\b0: %d%%  \\b Slow-downs\\b0: "
            PRICV "  \\b Crashes\\b0: \\cf9 %d\\cf0 %s",
            h,m,s,ht,mt,st,nl,CpuUse,nSlowdown,StatsStatic.nSteemCrash,nl);
    if(StatsStatic.nPatchedBytes)
      fprintf(fp,"Patched bytes: \\cf9 %d\\cf0 %s",StatsStatic.nPatchedBytes,nl);
#else
    fprintf(fp,"Time: %02d:%02d:%02d (total %02d:%02d:%02d)%sCPU load: %d%%  Slow-downs: " PRICV "%s",
      h,m,s,ht,mt,st,nl,CpuUse,nSlowdown,nl);
#endif
    }
    DWORD tframe=avg_frame_time/NFRAME_TIME_AVG; // those are real averages
    if(tEmuN)
    {
#ifdef WIN32
      float temu=((float)(MicroTime.Us(tEmuT)/tEmuN))/1000;
      float tlock=((float)(MicroTime.Us(tLockT)/tEmuN))/1000;
      float tunlock=((float)(MicroTime.Us(tUnlockT)/tEmuN))/1000;
      float tblit=((float)(MicroTime.Us(tBlitT)/tEmuN))/1000;
#else
      float temu=(float)tEmuT/tEmuN;
      float tlock=(float)tLockT/tEmuN;
      float tunlock=(float)tUnlockT/tEmuN;
      float tblit=(float)tBlitT/tEmuN;
#endif
      fprintf(fp,"Frame %dms Emu %.2fms Lock %.2fms Unlock %.2fms Blit %.2fms%s",
              tframe,temu,tlock,tunlock,tblit,nl);
    }
#if defined(SSE_STATS_RTF)
    fprintf(fp,"InfoScroll: \\cf9 %d\\cf0 %s",StatsStatic.nScrollers,nl);
    fprintf(fp,"\\b Disk\\b0 %s",nl);
#else
    fprintf(fp,"InfoScroll: %d%s",StatsStatic.nScrollers,nl);
    fprintf(fp,"Disk%s",nl);
#endif
#ifdef SSE421R1
    if(DiskMan.bTurboDrive)
      fprintf(fp,"Turbo%s",nl);
#endif
    for(int d=DRIVE_A;d<=DRIVE_B;d++)
    {
      TSF314 &drive=FloppyDrive[d];
      TFloppyDisk &disk=FloppyDisk[d];
      if(drive.bDiskInDrive)
      {
        fprintf(fp,"(MNGR_%s/%s) %c: %s %s %d%cD %s %08X%s",
                disk_manager[drive.ImageType.Manager],extension_list[drive.ImageType.Extension],
#if defined(SSE_420R5) // path can interfere with RTF encoding!
                'A'+d,GetFileNameFromPath(disk.ImageFile.Text),disk.DiskInZip.Text,disk.Sides,
#else
                'A'+d,CHECKPATH(disk.ImageFile.Text),disk.DiskInZip.Text,disk.Sides,
#endif
                ((disk.Density==2) ? 'H' : 'D'),"CRC32",disk.crc32,nl);
        if(StatsStatic.maxSector[d])
        {
          fprintf(fp,
#if defined(SSE_STATS_RTF)
                  "%c: max side-track-sector: \\cf9 %c-%d-%d\\cf0  R%.1fK W%.1fK%s",
#else
                  "%c: max side-track-sector: %c-%d-%d\\ R%.1fK W%.1fK%s",
#endif
                  'A'+d,StatsStatic.maxSide[d]+'A',StatsStatic.maxTrack[d],
                  StatsStatic.maxSector[d],(float)nSectorR[d]/2,(float)nSectorW[d]/2,nl);
        }
        for(int side=0;side<2;side++)
        {
          if(boot_checksum[d][side])
            fprintf(fp,"%c%d: boot checksum %04X%s",'A'+d,side,boot_checksum[d][side],nl);
        }
      }
    }
    if(nHdsector)
      fprintf(fp,"HD sectors: " PRICV "%s",nHdsector,nl);
#if defined(SSE_STATS_RTF)
    fprintf(fp,"\\b OS\\b0 %s",nl);
#else
    fprintf(fp,"OS%s",nl);
#endif
    if(nPrg)
    {
      fprintf(fp,"Programs: " PRICV " Current: %s%s",nPrg,Tos.PrgName,nl);
#if defined(SSE_DEBUG_SYMBOLS)
      fprintf(fp,"Basepage: $%06X%s",Tos.Basepage,nl);
      for(int i=0;i<MAX_SYMBOLS&&TosSymbol[i].ad!=0xFFFFFFFF;i++)
      {
        TTosSymbol &x=TosSymbol[i];
        fprintf(fp,"#%d %s $%X $%X $%X at %X%s",i,x.name,x.type,x.value1,x.value2,x.ad,nl);
      }
#endif
    }
    fprintf(fp,"System calls%sGEMDOS: " PRICV " (intercepted: " PRICV ") BIOS: "
PRICV " (" PRICV ") XBIOS: " PRICV " (" PRICV ") VDI: " PRICV " ("
PRICV ") AES: " PRICV "%s",nl,nGemdos,nGemdosi,nBios,nBiosi,nXbios,
      nXbiosi,nVdi,nVdii,nAes,nl);
#if defined(SSE_STATS_RTF)
    fprintf(fp,"\\b Input\\b0%s",nl);
#else
    fprintf(fp,"Input%s",nl);
#endif
    fprintf(fp,"%s $%x %s%s","KBD",KeyboardLangID,((KeyboardLangID==LANG_CUSTOM)
#if defined(SSE_420R5) // path can interfere with RTF encoding!
            ?GetFileNameFromPath(KeyboardMappingPath.Text):""),nl);
#else
            ?CHECKPATH(KeyboardMappingPath.Text):""),nl);
#endif
    fprintf(fp,"Keys: " PRICV "%s",nKeyIn,nl);
    fprintf(fp,"Mouse h: " PRICV " v: " PRICV "%s",nMousex,nMousey,nl);
    fprintf(fp,"%s moves 0: " PRICV " 1: " PRICV "%s","Joystick",nJoy[0],nJoy[1],nl);
    fprintf(fp,"Left click/fire 0: " PRICV "%s",nClick[0],nl);
    fprintf(fp,"Right click/fire 1: " PRICV "%s",nClick[1],nl);
#ifdef SSE_BETA
    if(mskSpecial)
    {
#if defined(SSE_STATS_RTF)
      fprintf(fp,"\\b Special \\b0%s",nl);
#else
      fprintf(fp,"Special%s",nl);
#endif
      if(mskSpecial&IKBD_22)
        fprintf(fp,"%s reprogramming%s","IKBD",nl);
      if(mskSpecial&EMU_DETECT)
        fprintf(fp,"%s %s%s","Emu","detect",nl);
    }
#endif
#if defined(SSE_STATS_RTF)
    fprintf(fp,"\\b %s Video\\b0 %s",screen_type[SSEConfig.ColourMonitor],nl);
#else
    fprintf(fp,"%s Video%s",screen_type[SSEConfig.ColourMonitor],nl);
#endif
#ifdef SSE421R1
    fprintf(fp,"Resolution: %d%c%s",screen_res,video_mixed_output?'+':' ',nl);
#endif
    float computedFreq=(float)nFrame*1000;    
#ifdef WIN32
    if(Stats.tFrameT)
      computedFreq/=MicroTime.Ms(Stats.tFrameT);
#else
    if(run_time)
      computedFreq/=run_time;
#endif
    char cSyncType[64]="Milliseconds";
    if(Draw.VSync)
      strcpy(cSyncType,"VSync");
    else if(OPTION_MICROSECONDS)
      strcpy(cSyncType,"Microseconds");
    if(OPTION_BFI)
      strcat(cSyncType,"+BFI");
#ifdef SSE421R1
    fprintf(fp,"Host Frequency: %dHz ST: %dHz Average: %fHz Sync: %s%s",
            Disp.Freq,Glue.PreviousVideoFreq,computedFreq,cSyncType,nl);
#else
    fprintf(fp,"Host Frequency: %dHz ST: %dHz Average: %fHz Sync: %s Resolution: %d%c%s",
            Disp.Freq,Glue.PreviousVideoFreq,computedFreq,cSyncType,screen_res,
            video_mixed_output?'+':' ',nl);
#endif
    fprintf(fp,"Frames: " PRICV "%s",nFrame,nl);
    if(OPTION_OSD_FPSINFO && nFps) // it's optional because of the load
      fprintf(fp,"ST FPS: %d%s",nFps,nl);
    fprintf(fp,"%s %s: %d%s","Overscan","Emu",OPTION_VLE,nl);
    if(OPTION_HWOVERSCAN)
      fprintf(fp,"%s%s",overscan_dev[OPTION_HWOVERSCAN],nl);
    fprintf(fp,"%s mask: %X, %s: %X%s","Overscan",mskOverscan,s1frame,mskOverscan1,nl);
#if defined(SSE_OPTION_FASTBLITTER)
    if(nFastBlit)
      fprintf(fp,"Fast blit/line A: " PRICV "%s",nFastBlit,nl);
#endif
    if(mskOverscan1) // detail overscan tricks of last frame
    {
      fprintf(fp,"%s %s: ","Overscan",s1frame);
      if(mskOverscan1&TRICK_TOP_OVERSCAN)
        fprintf(fp,"Top ");
      if(mskOverscan1&(TRICK_BOTTOM_OVERSCAN|TRICK_BOTTOM_OVERSCAN_60HZ))
        fprintf(fp,"Bottom ");
      if(mskOverscan1&(TRICK_LINE_PLUS_26|TRICK_LINE_PLUS_20))
        fprintf(fp,"Left ");
      if(mskOverscan1&TRICK_LINE_PLUS_44)
        fprintf(fp,"Right ");
      if(mskOverscan1&TRICK_0BYTE_LINE)
        fprintf(fp,"0 byte ");
      if(mskOverscan1&(TRICK_LINE_MINUS_106|TRICK_4BIT_SCROLL|TRICK_LINE_PLUS_2
                       |TRICK_LINE_MINUS_106|TRICK_LINE_MINUS_2))
        fprintf(fp,"Sync-scroll"); // maybe
      if(nLinePlus16)
        fprintf(fp,"%s+16 pixels",sSte);
      fprintf(fp,"%s",nl);
    }
    if(nTimerb)
      fprintf(fp,"Timer B: " PRICV "%s",nTimerb,nl);
    if(nTimerbtick)
      fprintf(fp,"Timer B ticks (%s): %d%s",s1frame,nTimerbtick,nl);
    if(nPal)
      fprintf(fp,"Palette writes (%s): %d%s",s1frame,nPal,nl);
    if(nVbaseHi)
      fprintf(fp,"%s VBASE HI: " PRICV "%s","Write",nVbaseHi,nl);
    if(nVbaseMid) // vcount
      fprintf(fp,"%s VBASE MID: " PRICV ", %s: %d%s","Write",nVbaseMid,s1frame,nVbaseMid1,nl);
    if(nVbaseLo) // vbase
      fprintf(fp,"%s %s VBASE LO: " PRICV "%s","Write",sSte,nVbaseLo,nl);
    if(nReadvc)
      fprintf(fp,"Read VCOUNT: " PRICV ", %s: %d%s",nReadvc,s1frame,nReadvc1,nl);
#ifdef WIN32 //todo        
    // counting colours is not as easy!
    // we read back our DirectX backbuffer and count different pixels (slow)
    if(nFrame 
#ifndef SSE_NO_OSD
      && !OsdControl.bOsdDrawn
#endif
      && (Disp.Method==DISPMETHOD_DD||Disp.Method==DISPMETHOD_D3D))
    { 
      HRESULT DErr=~DD_OK;
      DWORD *pDataStart=NULL;
      DWORD dwpitch=0;
#if defined(SSE_VID_DD)
      Disp.OurBackSur=(OPTION_3BUFFER_WIN&&Disp.DDBackSur2&&Disp.SurfaceToggle)
        ? Disp.DDBackSur2 : Disp.DDBackSur;
      DErr=Disp.OurBackSur->Lock(NULL,&Disp.DDBackSurDesc,
        DDLOCK_READONLY,NULL);
      dwpitch=Disp.DDBackSurDesc.lPitch/4;
      pDataStart=(DWORD*)Disp.DDBackSurDesc.lpSurface;
#endif
#if defined(SSE_VID_D3D)
      D3DLOCKED_RECT LockedRect;
      if(draw_lock)
        draw_end();
      DErr=Disp.pD3DTexture->LockRect(0,&LockedRect,NULL,0);
      dwpitch=LockedRect.Pitch/4;
      pDataStart=(DWORD*)LockedRect.pBits;
#endif
      if(DErr==DD_OK)
      {
        DWORD *pData=pDataStart;
        WORD nColour=0;
        DWORD ix=0;
        DWORD *pPixel=new DWORD[4096]; // assume 32bit, 4096 col. max
        ZeroMemory(pPixel,4096*sizeof(DWORD)); // normally useless...
        LONG h=draw_blit_source_rect.bottom-draw_blit_source_rect.top;
        LONG w=draw_blit_source_rect.right-draw_blit_source_rect.left;
        for(LONG y=0;y<h;y++)
        {
          for(LONG x=0;x<w;x++)
          {
            DWORD data=(*(pData+y*dwpitch+x) & 0x00FFFFFF); // assume X8R8G8B8
            WORD i;
            BOOL recorded=FALSE;
            for(i=0;i<nColour&&!recorded;i++) // for first pixel, i=nColour=0
            {
              if(pPixel[i]==data)
                recorded=TRUE;
            }
            if(!recorded && nColour<4096) // theoretical max on STE but pixel shader + OSD can add colours
              pPixel[nColour++]=data;
            ix++;
          }//nxt x
        }//nxt y
        delete [] pPixel;
#if defined(SSE_VID_DD)
        Disp.OurBackSur->Unlock(NULL);
#endif
#if defined(SSE_VID_D3D)
        DErr=Disp.pD3DTexture->UnlockRect(0);
#endif
#if defined(SSE_STATS_RTF)
        if(nColour>16) // in blue
          fprintf(fp,"%s (%s): \\cf2 %d\\cf0%s","Colours",s1frame,nColour,nl);
        else
#endif
          fprintf(fp,"%s (%s): %d%s","Colours",s1frame,nColour,nl);
      }
      else
        fprintf(fp,"%s %s%s","Colours","ERROR",nl); // regularly happens
    }
#endif//WIN32
    if(nBlit)
      fprintf(fp,"%s Blit: " PRICV " (hog: " PRICV ", TOS: " PRICV "), %s: %d%s",
              ((IS_STE)?sSte:st_model_name[ST_MODEL]),nBlit,nBlith,nBlitT,s1frame,nBlit1,nl);
    if(nExtendedPal)
      fprintf(fp,"%s4096 colour palette: " PRICV " (TOS: " PRICV ")%s",sSte,
              nExtendedPal,nExtendedPalT,nl);
    if(nHscroll)
      fprintf(fp,"%sHorizontal scroll: " PRICV "%s",sSte,nHscroll,nl);
    if(StatsStatic.nBlitError)
#if defined(SSE_STATS_RTF)
      fprintf(fp,"%s: \\cf9 %d\\cf0 %s","BLIT ERROR",StatsStatic.nBlitError,nl);
#else
      fprintf(fp,"%s: %d%s","BLIT ERROR",StatsStatic.nBlitError,nl);
#endif
#if defined(SSE_STATS_RTF)
    fprintf(fp,"\\b Sound\\b0 %s",nl);
#else
    fprintf(fp,"Sound%s",nl);
#endif
    if(nPsgSound)
      fprintf(fp,"PSG: " PRICV "%s",nPsgSound,nl);
    for(int mode=0;mode<4;mode++)
      if(mskDigitalSound&(1<<mode))
        fprintf(fp,"%sDigital sound: %dHz %s%s",sSte,SteSoundModeToFreq[mode],
                ((mskDigitalSound)&(1<<(mode+4)))?"Mono":"Stereo",nl);
    if(nMicrowire)
      fprintf(fp,"%sMicroWire: " PRICV " (TOS: " PRICV ")%s",sSte,nMicrowire,nMicrowireT,nl);
#if defined(SSE_STATS_RTF)
    fprintf(fp,"\\b I/O\\b0 %s",nl);
#else
    fprintf(fp,"I/O%s",nl);
#endif
    for(int i=0;i<3;i++)
    {
      if(nPorti[i])
        fprintf(fp,"%s in: " PRICV "%s",STPort[i].Name.Text,nPorti[i],nl);
      if(nPorto[i])
        fprintf(fp,"%s out: " PRICV "%s",STPort[i].Name.Text,nPorto[i],nl);
    }
#if defined(SSE_STATS_RTF)
    fprintf(fp,"\\b Exceptions\\b0 %s",nl);
#else
    fprintf(fp,"Exceptions%s",nl);
#endif
#if defined(SSE_STATS_RTF)
    fprintf(fp,"%s: \\cf9 %d\\cf0 %s","RESET",StatsStatic.nReset,nl);
#else
    fprintf(fp,"%s: %d%s","RESET",StatsStatic.nReset,nl);
#endif
    fprintf(fp,"%s: " PRICV "%s","VSYNC",nVbi,nl);
    if(nHbi)
      fprintf(fp,"%s: " PRICV ", %s: %d%s","HSYNC",nHbi,s1frame,nHbi1,nl);
    for(int i=EXCEPTION_RESET;i<=EXCEPTION_LINE_F;i++)
    {
      if(nException[i])
        fprintf(fp,"Exception %d: " PRICV "%s",i,nException[i],nl);
    }
#if defined(SSE_420R5)
    for(int irq=0;irq<N_MFP_IRQS;irq++)
#else
    for(int irq=0;irq<15;irq++)
#endif
    {
      if(nMfpIrq[irq] || Mfp.IrqInfo[irq].IsTimer && nMfpTimeout[Mfp.IrqInfo[irq].Timer])
      {
        if(Mfp.IrqInfo[irq].IsTimer)
          fprintf(fp,"%s %s %d: " PRICV " (" PRICV " timeouts, %uHz)%s","MFP","irq",irq,
                  nMfpIrq[irq],nMfpTimeout[Mfp.IrqInfo[irq].Timer],fTimer[Mfp.IrqInfo[irq].Timer],nl);
        else
          fprintf(fp,"%s %s %d: " PRICV "%s","MFP","irq",irq,nMfpIrq[irq],nl);
      }
    }
    if(nSpurious)
      fprintf(fp,"Spurious: " PRICV "%s",nSpurious,nl);
    for(BYTE trap=0;trap<16;trap++)
    {
      if(nTrap[trap])
        fprintf(fp,"Trap #%d: " PRICV "%s",trap,nTrap[trap],nl);
    }
#if defined(SSE_STATS_EXT) // for debug builds because of the load
    if(!OPTION_C3) // TODO?
    {
      COUNTER_VAR ast=A_S_T-Ast0; // will be wrong if too long time (32bit)
      char stmp[3][64];
      InsertCommas(stmp[0],ast);
      InsertCommas(stmp[1],BusRead);
      InsertCommas(stmp[2],BusWrite);
      double prctr=(ast)?(BusRead*4*TICKS8*100.0)/ast:0; 
      double prctw=(ast)?(BusWrite*4*TICKS8*100.0)/ast:0; 
#if defined(SSE_STATS_RTF)
      fprintf(fp,"\\b Bus\\b0 %sCycles: %s%sR/W=%d cycles R: %s (%f%%) W: %s (%f%%)%s",
              nl,stmp[0],nl,4*TICKS8,stmp[1],prctr,stmp[2],prctw,nl);
#else
      fprintf(fp,"Bus%sR/W=%d cycles R: %s W: %s%s",nl,4*TICKS8,stmp[0],stmp[1],nl);
#endif
      InsertCommas(stmp[1],RamAccessWs);
      InsertCommas(stmp[2],ChipsAccessWs);
      double prctR=(ast)?(RamAccessWs*100.0)/ast:0; 
      double prctC=(ast)?(ChipsAccessWs*100.0)/ast:0; 
      fprintf(fp,"RAM waitstates: %s (%f%%)%sChips waitstates: %s (%f%%)%s",
              stmp[1],prctR,nl,stmp[2],prctC,nl);
    }
#endif
    if(nStop)
      fprintf(fp,"Stop: " PRICV "%s",nStop,nl); // Bus stop, another joke
#if defined(SSE_STATS_RTF)
    fprintf(fp,"\\b %sSTATE OF CHIPS%s\\f2 68000\\b0 %s",nl,nl,nl);
#else
    fprintf(fp,"%sSTATE OF CHIPS%s68000%s",nl,nl,nl);
#endif
    if(Cpu.ProcessingState==TMC68000::HALTED)
#if defined(SSE_STATS_RTF)
      fprintf(fp,"\\cf6 HALT \\cf0 %s",nl);
#else
      fprintf(fp,"HALT%s",nl);
#endif
    int ccs=0; // computer clock shift!
#if defined(SSE_MEGASTE)
    if(IS_MEGASTE)
    { // reminder: MSTE 16MHz uses same main clock in Steem, see 
      // BusMegaSteIdle(int t) in cpu.cpp
      ccs=(Cpu16.ScuReg&BIT_1)>>1;
      fprintf(fp,"Cache:%c ",(Cpu16.ScuReg&BIT_0) ? 'Y' : 'N');
    }
#endif
    fprintf(fp,"Clock: %dMHz (%dHz)%s",(SSEConfig.CpuBoost*8)<<ccs,(nSysCyclesPerSecond/TICKS8)<<ccs,nl);
    UPDATE_SR;
#if defined(SSE_DEBUGGER)
    EasyStr current_instr=disa_d2(pc,IR).Text;
    fprintf(fp,"PC:%08X IR:%04X %s SR:%04X IPL:%d%s",pc,IR,current_instr.Text,SR,ipl_timing_ipl[ipl_timing_index],nl);
#elif defined(SSE_LOGSECTIONS_FOR_ALL)
    EasyStr current_instr=disa(pc);
    fprintf(fp,"PC:%08X IR:%04X %s SR:%04X IPL:%d%s",pc,IR,current_instr.Text,SR,ipl_timing_ipl[ipl_timing_index],nl);
#else
    fprintf(fp,"PC:%08X IR:%04X SR:%04X IPL:%d%s",pc,IR,SR,ipl_timing_ipl[ipl_timing_index],nl);
#endif
    for(int i=0;i<4;i++)
      fprintf(fp,"D%d:%08X ",i,Cpu.r[i]);
    fprintf(fp,"%s",nl);
    for(int i=4;i<8;i++)
      fprintf(fp,"D%d:%08X ",i,Cpu.r[i]);
    fprintf(fp,"%s",nl);
    for(int i=0;i<4;i++)
      fprintf(fp,"A%d:%08X ",i,areg[i]);
    fprintf(fp,"%s",nl);
    for(int i=4;i<8;i++)
      fprintf(fp,"A%d:%08X ",i,areg[i]);
    fprintf(fp,"%sABUS:%06X DBUS:%04X USP:%08X SSP:%08X%s",nl,abus,dbus,USP,SSP,nl);
#if defined(SSE_STATS_RTF)
    fprintf(fp,"\\b 68901\\b0 %s%s %d%s",nl,"XTAL",Mfp.xtal,nl);
#else
    fprintf(fp,"68901%s%s %d%s",nl,"XTAL",Mfp.xtal,nl);
#endif
    for(int reg=0;reg<N_MFP_REGS;reg++)
    {
      fprintf(fp,"%02d:%02X ",reg,Mfp.reg[reg]);
      if((reg&7)==7) // 8 columns
        fprintf(fp,"%s",nl);
    }
    for(int ti=0;ti<N_MFP_TIMERS;ti++)
      fprintf(fp,"CTR %c:%02X ",'A'+ti,Mfp.Counter[ti]);
#if defined(SSE_HD6301_LL)
#if defined(SSE_STATS_RTF)
    fprintf(fp,"%s\\b 6301\\b0 %s",nl,nl);
#else
    fprintf(fp,"%s6301%s",nl,nl);
#endif
    if(OPTION_C1)
    {
      fprintf(fp,"PC:%04X OP:%02X CCR:%02X SP:%04X D:%04X X:%04X%s",
              hd6301_peek(-1),hd6301_peek(-6),hd6301_peek(-2),hd6301_peek(-3),
              hd6301_peek(-4),hd6301_peek(-5),nl);
      for(BYTE reg=0;reg<16;reg++)
      {
        fprintf(fp,"%02d:%02X ",reg,hd6301_peek(reg));
        if((reg&7)==7)
          fprintf(fp,"%s",nl);
      }
    }
    else
      fprintf(fp,"NOT ACTIVE%s",nl);
#endif
#if defined(SSE_STATS_RTF)
    fprintf(fp,"\\b 6850\\b0 %s",nl);
#else
    fprintf(fp,"6850%s",nl);
#endif
    for(int i=0;i<2;i++)
      fprintf(fp,"ACIA %d CR:%02X SR:%02X RDR:%02X TDR:%02X%s",
              acia[i].Id,acia[i].cr,acia[i].sr,acia[i].rdr,acia[i].tdr,nl);
#if defined(SSE_STATS_RTF)
    fprintf(fp,"\\b YM2149\\b0 %s",nl);
#else
    fprintf(fp,"YM2149%s",nl);
#endif
    for(BYTE reg=0;reg<16;reg++)
    {
      fprintf(fp,"%02d:%02X ",reg,psg_reg[reg]);
      if((reg&7)==7)
        fprintf(fp,"%s",nl);
    }
#if defined(SSE_STATS_RTF)
    fprintf(fp,"\\b WD1772\\b0 %s",nl);
#else
    fprintf(fp,"WD1772%s",nl);
#endif
    fprintf(fp,"CR:%02X STR:%02X TR:%02X SR:%02X DR:%02X DSR:%02X CRC:%04X%s",
            Fdc.cr,Fdc.str,Fdc.tr,Fdc.sr,Fdc.dr,Fdc.dsr,Fdc.CrcLogic.crc,nl);
#if defined(SSE_STATS_RTF)
    fprintf(fp,(IS_STE) ? "\\b GSTMCU\\b0 %s" : "\\b MMU\\b0 %sWS:%d ",nl,Mmu.WS[OPTION_WS]);
#else
    fprintf(fp,(IS_STE) ? "GSTMCU%s" : "MMU%s",nl);
#endif
    DU32 usdp(Mmu.VideoCounter);
    fprintf(fp,"001:%02X 201:%02X 203:%02X 205:%02X 207:%02X 209:%02X%s",
            Mmu.Config,Mmu.uVBase.d8[B2],Mmu.uVBase.d8[B1],usdp.d8[B2],usdp.d8[B1],usdp.d8[B0],nl);
    fprintf(fp,"609:%02X 60B:%02X 60D:%02X%s",
            Mmu.uDmaCounter.d8[B2],Mmu.uDmaCounter.d8[B1],Mmu.uDmaCounter.d8[B0],nl);
    if(IS_STE)
    {
      fprintf(fp,"20D:%02X 20F:%02X 901:%02X 903:%02X 905:%02X 907:%02X%s",
              Mmu.uVBase.d8[B0],Mmu.linewid,Mmu.SoundControl,
              Mmu.uNextSoundFrameStart.d8[B2],Mmu.uNextSoundFrameStart.d8[B1],
              Mmu.uNextSoundFrameStart.d8[B0],nl);
      fprintf(fp,"909:%02X 90B:%02X 90D:%02X 90F:%02X 911:%02X 913:%02X%s",
              Mmu.uSoundFetchAd.d8[B2],Mmu.uSoundFetchAd.d8[B1],
              Mmu.uSoundFetchAd.d8[B2],Mmu.uNextSoundFrameEnd.d8[B2],
              Mmu.uNextSoundFrameEnd.d8[B1],Mmu.uNextSoundFrameEnd.d8[B0],nl);
    }
    if(IS_STF)
#if defined(SSE_STATS_RTF)
      fprintf(fp,"\\b GLU\\b0 %s",nl);
#else
      fprintf(fp,"GLU%s",nl);
#endif
    fprintf(fp,"20A:%02X 260:%02X%s",Glue.SyncMode,Glue.ShiftMode,nl);
    if(IS_STE)
      fprintf(fp,"265:%02X%s",Glue.hscroll,nl);
#if defined(SSE_STATS_RTF)
    fprintf(fp,"\\b Shifter\\b0 %s",nl);
#else
    fprintf(fp,"Shifter%s",nl);
#endif
    for(int reg=0;reg<16;reg++)
    {
      fprintf(fp,"%02d:%04X ",reg,STpal[reg]);
      if((reg&3)==3)
        fprintf(fp,"%s",nl);
    }
    if(IS_STE)
      fprintf(fp,"16:%X 17:%X 18:%X 19:%04X 20:%04X WU:%d%s",Shifter.ShiftMode,shifter_hscroll,
              shifter_sound_mode,Microwire.Data,Microwire.Mask,Shifter.WakeupShift,nl);
    else
      fprintf(fp,"16:%X WU:%d%s",Shifter.ShiftMode,Shifter.WakeupShift,nl);
    //fprintf(fp,"16:%X%s",Shifter.ShiftMode);// TEST SEH exception in other module
    //int a=0; int b=5/a;  printf("yoho %d",b);// TEST SEH exception
#if defined(SSE_STATS_RTF)
    fprintf(fp,"\\b DMA\\b0 %s",nl);
#else
    fprintf(fp,"DMA%s",nl);
#endif
    fprintf(fp,"MCR:%03X SR:%02X CTR:%02X CNT:%02X FIFO:%d-%d bsy:%d%s",Dma.mcr,
            Dma.sr,Dma.Counter,Dma.ByteCount,Dma.BufferInUse,
            Dma.Fifo_idx[Dma.BufferInUse],DiskEmu.AcsiBsy,nl);
    for(int fifo_n=0;fifo_n<2;fifo_n++)
    {
      for(int i=0;i<8;i++) // as words but our fifos contain bytes
        fprintf(fp,"%02X%02X ",Dma.Fifo[fifo_n][i*2],Dma.Fifo[fifo_n][i*2+1]);
      fprintf(fp,"%s",nl);
    }
    if(SSEConfig.Blitter)
    {
#if defined(SSE_STATS_RTF)
      fprintf(fp,"\\b Blitter\\b0 %s",nl);
#else
      fprintf(fp,"Blitter%s",nl);
#endif
      for(int reg=0;reg<16;reg++)
      {
        fprintf(fp,"%02d:%04X ",reg,Blitter.HalfToneRAM[reg]);
        if((reg&3)==3)
          fprintf(fp,"%s",nl);
      }
      fprintf(fp,"16:%04X 17:%04X 18:%04X 19:%04X%s",(WORD)Blitter.SrcXInc,
              (WORD)Blitter.SrcYInc,Blitter.SrcAdr.d16[HI],Blitter.SrcAdr.d16[LO],nl);
      fprintf(fp,"20:%04X 21:%04X 22:%04X 23:%04X%s",Blitter.EndMask[0],
              Blitter.EndMask[1],Blitter.EndMask[2],(WORD)Blitter.DestXInc,nl);
      fprintf(fp,"24:%04X 25:%04X 26:%04X 27:%04X%s",(WORD)Blitter.DestYInc,
              Blitter.DestAdr.d16[HI],Blitter.DestAdr.d16[LO],Blitter.XCount,nl);
      BYTE hibyte=Blitter.LineNumber|Blitter.Smudge|Blitter.Hog|Blitter.rBusy;
      BYTE lobyte=Blitter.Skew|Blitter.NFSR|Blitter.FXSR;
      fprintf(fp,"28:%04X 29:%04X 30:%04X%s",Blitter.YCount,
              (Blitter.Hop<<8)|Blitter.Op,(hibyte<<8)|lobyte,nl);
    }
#if defined(SSE_STATS_RTF)
    fprintf(fp,"\\f0 %s }",nl);
#endif
    fclose(fp);
  }
}

#endif//#if defined(SSE_STATS)

#undef LOGSECTION
