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
FILE: reset.cpp
DESCRIPTION: Emulation of the various ways to reset the ST: 
power-on                  = cold reset
pressing the reset button = warm reset
CPU instruction reset     = reset peripherals         
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <computer.h>
#include <osd.h>
#include <draw.h>
#include <gui.h>
#include <loadsave.h>
#if defined(SSE_VID_DD)
#include <display.h>
#endif
#if defined(SSE_VID_STVL1)
#include <interface_stvl.h>
#endif


/*
Atari ST Boot Up Operation
The Motorola 68000 on boot up requires initial
values to load into its supervisor stack pointer
and reset vector address. These come in the
form of two long words at address 0x000000 to
0x000007.

Boot up sequence
Figure 9 - Atari ST boot up sequence
(1)
. Load SSP with long word value from 0xFC0000.
. Load PC with long word value from 0xFC0004 (Garbage value, memory not yet
sized).
. CPU Supervisor Mode Interrupts disabled (IPL=7).
. RESET instruction to reset all peripheral chips.
. Check for magic number 0xFA52235F on cartridge port, if present jump to
diagnostic cartridge.
(2).
. Test for warm start, if memvalid (0x000420) and memval2 (0x00043A) contain
the Magic numbers 0x7520191F3 and 0x237698AA respectively, then load the
memconf (0xFF8001) contents with data from memctrl (0x000424).
(3)
. If the resvalid (0x000426) contains the Magic number 0x31415926, jump to reset
vector taken from Resvector (0x00042A).
(4)
. YM2149 sound chip initialized (Floppy deselected).
. The vertical synchronization frequency in syncmode (0xFF820A) is adjusted to
50Hz or 60Hz depending on region.
. Shifter palette initialized.
. Shifter Base register (0xFF8201 and 0xFF8203) are initialized to 0x010000.
. The following steps 5 to 8 are only done on a coldstart to initialize memory.
(5)
. Write 0x000a (2 Mbyte & 2 Mbyte) to the MMU Memory Configuration Register
0xff8001).
(6)
. Write Pattern to 0x000000 - 0x000lff.
. Read Pattern from 0x000200 - 0x0003ff.
. If Match then Bank0 contains 128 Kbyte; goto step 7.
. Read Pattern from 0x000400 - 0x0005ff.
. If Match then Bank0 contains 512 Kbyte; goto step 7.
. Read Pattern from 0x000000 - 0x0001ff.
. If Match then Bank0 contains 2 Mbyte; goto step 7.
. panic: RAM error in Bank0.
(7)
. Write Pattern to 0x200000 - 0x200lff.
. Read Pattern from 0x200200 - 0x2003ff.
. If Match then Bank1 contains 128 Kbyte; goto step 8.
. Read Pattern from 0x200400 - 0x2005ff.
. If Match then Bank1 contains 512 Kbyte; goto step 8.
. Read Pattern from 0x200000 - 0x2001ff.
. If Match then Bank1 contains 2 Mbyte; goto step 8.
. note: Bank1 not fitted.
(8)
. Write Configuration to MMU Memory Configuration Register (0xff8001).
. Note Total Memory Size (Top of RAM) for future reference in phystop
(0x00042E).
. Set magic values in memvalid (0x000420) and memval2 (0x00043A).
(9)
. Clear the first 64 Kbytes of RAM from top of operating system variables
(0x00093A) to Shifter base address (0x010000).
. Initialize operating system variables.
. Change and locate Shifter Base register to 32768 bytes from top of physical ram.
. Initialize interrupt CPU vector table.
. Initialize BIOS.
. Initialize MFP.
(10)
. Cartridge port checked, if software with bit 2 set in CA_INIT then start.
(11)
. Identify type of monitor attached for mode of operation for the Shifter video chip
and initialize.
(12)
. Cartridge port checked, if software with CA_INIT clear (execute prior to display
memory and interrupt vector initialization) then start.
(13)
. CPU Interrupt level (IPL) lowered to 3 (HBlank interrupts remain masked).
(14)
. Cartridge port checked, if software with bit 1 set in CA_INIT (Execute prior to
GEMDOS initialization) then start.
(15)
. The GEMDOS Initialization routines are completed.
(16)
. Attempt boot from floppy disk if operating system variable _bootdev (0x000446)
smaller than 2 (for floppy disks) is. Before a boot attempt is made bit 3 in
CA_INIT (Execute prior to boot disk) checked, if set, start cartridge.
. The ACSI Bus is examined for devices, if successful search and load boot sector.
. If system variable _cmdload (0x000482) is 0x0000, skip step 17.
(17)
. Turn screen cursor on
. Start any program in AUTO folder of boot device
. Start COMMAND.PRG for a shell
(18)
. Start any program in AUTO folder of boot device
. AES (in the ROM) starts.

It is important to have these two different reset signals, as some parts of the design only
need to be reset on power up to known states. One of these components was the clock
signal component. It was important for the CPU that the clock was running while a reset
is issued, and that the reset was active for at least 132 clock cycles [27].
*/

/*
  1. power_on() is called by initialise() at start, if we don't restore
  a state snaphsot, reset_st() isn't called
  2. power_on() is called by reset_st() when player right clicks on the reset icon or uses
  the shortcut or when restoring a state snapshot (cold reset)
*/

void power_on() {
  //TRACE("POWER ON\n");
  if(STMem)
    ZeroMemory(STMem+MEM_EXTRA_BYTES,mem_len);
  on_rte=ON_RTE_NONE;
  // The GLUE selects ROM for addr 0-7, we copy into RAM at reset so our core routines are shorter
  LPEEK(0)=ROM_LPEEK(0); // starting stack address
  LPEEK(4)=ROM_LPEEK(4); // starting PC address
  //we updated a7 but then cleared it here, fixes TOS1.0 4MB
  //for (int n=0;n<16;n++) r[n]=0; //registers not reset!
  SP=ROM_LPEEK(0);
  SET_PC(ROM_LPEEK(4));
  SR=0x2700;
  UPDATE_FLAGS;
  vbase=0;
  ioaccess=0;
  //ASSERT(SSEConfig.IsInit); //asserts
#if defined(SSE_420R2B)
  SET_PC(LPEEK(4)); // hardware reality
#else
  SET_PC(rom_addr); // this is no hardware reality but a trick to detect cold reset in emulator!
#endif
  //vbase=mem_len-0x8000;
#ifndef NO_CRAZY_MONITOR
  if(extended_monitor)
    Tos.HackMemoryForExtendedMonitor();
#endif
#if defined(DEBUG_BUILD) || defined(SSE_LOGSECTIONS_FOR_ALL)
  if(debug_wipe_log_on_reset)
    logfile_wipe();
#endif
#ifndef SSE_NO_OSD
  osd_init_run(false);
#endif
  video_mixed_output=0;
  floppy_mediach[DRIVE_A]=floppy_mediach[DRIVE_B]=0;
  os_gemdos_vector=os_bios_vector=os_xbios_vector=0;
#ifndef DISABLE_STEMDOS
  Stemdos.Reset();
#endif
  interrupt_depth=0;
  hbl_count=0;
  agenda_length=0;
  emudetect_reset();
  snapshot_loaded=0;
  for(BYTE floppyno=DRIVE_A;floppyno<=DRIVE_B;floppyno++)
  {
    FloppyDrive[floppyno].Id=floppyno;
    FloppyDrive[floppyno].bMotor=false;
  }
  psg_reg_select=psg_reg_data=0;
  Dma.mcr=0; // see reset_peripherals()
  Fdc.tr=Fdc.sr=Fdc.dr=0;
  SSEConfig.SwitchSTModel(ST_MODEL);
  ZeroMemory(&Mfp.reg[MFPR_TADR],4); // timer data registers... and counters?
#if defined(SSE_SHIFTER_421) // colour registers never reset?
  for(int i=0;i<PAL_SIZE;STpal[i++]=rand()&((IS_STF) ? 0x0777 : 0x0FFF));
#if defined(SSE_GEM_CONTROL_PANEL)
  if(OptionBox.Page==PAGE_GEM_CP && OptionBox.IsVisible())
    OptionBox.UpdateColour(0);
#endif
#endif
  reset_peripherals(true);
  init_screen();
  init_timings();
#if defined(SSE_VID_DD_3BUFFER_WIN)
  Disp.VSyncTiming=0;
#endif
  
  if(SSEOptions.MonochromeDisableBorder)
  {
    BYTE save=border_last_chosen;
    BYTE newborder=(MONO_MONITOR) ? 0 : border_last_chosen;
#ifdef SSE_UNIX
    border=newborder;
    ChangeBorderSize(border);
#else
    OptionBox.SetBorder(newborder);
#endif
    border_last_chosen=save;
  }
  disable_input_vbl_count=(OPTION_HACKS) ? Glue.VideoFreq : 0;
#if defined(SSE_HD6301_LL) && defined(SSE_IKBD_RTC)
  if(OPTION_C1)
  {
    if(OPTION_BATTERY6301)
    {
      ikbd_set_clock_to_correct_time();
      hd6301_poke(0x88, 0xaa); //init
    }
    else
      hd6301_poke(0x88, 0x00); //not init
  }
#endif
#if defined(SSE_GUI_STATUS_BAR_MOUSE2)
  SSEConfig.MouseAd=0;
#endif
}


#define LOGSECTION LOGSECTION_ALWAYS

void reset_peripherals(bool const Cold) {
  Debug.Reset(Cold);
  Glue.Reset(Cold);
#ifndef NO_CRAZY_MONITOR
  if(extended_monitor)
  {
    if(em_planes==1)
    {
      screen_res=HIRES;
      Glue.VideoFreq=MONO_HZ;
      VideoFreqIdx=Glue.FREQ_IDX_71;
#if defined(SSE_420R5)
      COLOUR_MONITOR=FALSE;
#endif
    }
    else
    {
      screen_res=LORES;
      Glue.VideoFreq=PAL_HZ;
      VideoFreqIdx=Glue.FREQ_IDX_50;
#if defined(SSE_420R5)
      COLOUR_MONITOR=TRUE;
#endif
    }
    Glue.ShiftMode=screen_res; // important for extended monitor
    Tos.HackMemoryForExtendedMonitor();
  } else 
#endif
  if(COLOUR_MONITOR) //TODO
  {
    screen_res=LORES;
    Glue.VideoFreq=NTSC_HZ;
    VideoFreqIdx=Glue.FREQ_IDX_60;
  }
  else
  {
    screen_res=HIRES;
    Glue.VideoFreq=MONO_HZ;
    VideoFreqIdx=Glue.FREQ_IDX_71;
  }
  SSEConfig.OverscanOn=FALSE;
  Cpu.Reset(Cold);
  Mmu.Reset(Cold);
  Shifter.Reset(Cold);
#if !defined(SSE_GUI_421_REORDER)
  OptionBox.UpdateSTVideoPage();
#endif
  Fdc.Reset();
  Mfp.Reset(Cold);
  if(Cold) // note: no reset pin on that chip
  {
    Dma.sr=1;
    Dma.mcr=Dma.Counter=Dma.ByteCount=0;
  }
#if USE_PASTI
  if(hPasti)
    pasti->HwReset(Cold);
#endif
#if defined(SSE_DISK_CAPS)
  if(CAPSIMG_OK)
    Caps.Reset();
#endif
#if defined(SSE_ACSI)
  for(int i=0;i<MAX_ACSI_DEVICES;i++)
    if(ACSI_EMU_ON || i==ACSI_ID_LASER && OPTION_LASER)
      AcsiHdc[i].Reset();
#endif
  if(Cold) // note: the MC6850 has no reset pin
  {
    ACIA_Reset(ACIA_IKBD,true);
    ACIA_Reset(ACIA_MIDI,true);
  }
  if(!SSEConfig.Mega||Cold) // interesting remark by MasterOfGizmo, left without an answer at AF
    ikbd_reset(true); // Always cold reset, soft reset is different
#if defined(SSE_HD6301_LL)
  Ikbd.ResetChip(Cold);
#endif
#if 1 //?
  for(int i=0;i<NSTPORTS;i++)
    STPort[i].Reset();
#else
  MIDIPort.Reset();
  ParallelPort.Reset();
  SerialPort.Reset();
#endif
  RS232_CalculateBaud(Mfp.GetTimerControlRegister(MFP_TIMER_D),true);
  RS232_VBL();
  ZeroMemory(&Blitter,sizeof(Blitter));
  if(runstate==RUNSTATE_RUNNING)
    prepare_next_event();
  Psg.Reset();
#if defined(SSE_DRIVE_SOUND)
  if(Cold)
  {
    FloppyDrive[DRIVE_A].SoundStopBuffers();
    FloppyDrive[DRIVE_B].SoundStopBuffers();
  }
#endif
#if defined(SSE_VID_STVL1)
  acc_cycles=0;
  if(hStvl)
    video_logic_reset(&Stvl,Cold);
  draw_mem_line_ptr=(DWORD*)draw_mem;
  palette_convert_all(); // C3, if changing monitor triggers CPU op reset
#endif
#if defined(SSE_MEGA)
  Cpu16.Reset(); // if necessary
#endif
#if defined(SSE_MEGASTE)
  MegaSte.FdHd=0;
#endif
#if defined(SSE_PRINTER)
  if(Cold)
    Printer.Reset();
#endif
  DiskEmu.AcsiBsy=false;
#if defined(SSE_GUI_STATUS_BAR_MOUSE) && !defined(SSE_GUI_STATUS_BAR_MOUSE2)
  Tos.MouseX=Tos.MouseY=0;
#endif
#if defined(SSE_CARTRIDGE_ACTIVE)
  if(Cold)
    memset(CartridgeData,0,CARTRIDGE_DATA_SIZE);
#endif
}

#undef LOGSECTION


void reset_st(DWORD const flags) {
  // first get out of instruction function if needs be
  if(runstate==RUNSTATE_RUNNING&&!(flags&RESET_STAGE2))
  {
    TRACEB("F%d y%d Reset setup\n",FRAME,scan_y);
    agenda_add(agenda_reset,2,flags);
    return;
  }
#if defined(SSE_STATS)
  if(flags&RESET_COUNT)
    StatsStatic.nReset++;
#endif
#if defined(SSE_EMU_THREAD)
  // don't remember why it was necessary but I made it safer (no more ++ --)
  BYTE old_vlock=VideoLock.disabled;
  if(OPTION_EMUTHREAD && runstate!=RUNSTATE_STOPPED)
  {
    if(GetCurrentThreadId()==EmuThreadId)
      VideoLock.disabled=1;
  }
#endif
  bool Stop=((flags & RESET_NOSTOP)==0);
  bool Warm=((flags & RESET_WARM)!=0);
  bool ChangeSettings=((flags & RESET_NOCHANGESETTINGS)==0);
  bool Backup=((flags & RESET_NOBACKUP)==0 && SSEOptions.ResetBackup);
  if(runstate==RUNSTATE_RUNNING && Stop) 
    runstate=RUNSTATE_STOPPING;
#if !defined(SSE_LIBRETRONUKE)
  if(Backup) 
    GUISaveResetBackup();
#endif

  //ipl_timing_index++;
  //ipl_timing_ipl[ipl_timing_index]=IPL_RESET;
  //ipl_timing_time[ipl_timing_index]=a_s_t;

  // same for cold & warm reset
  // "Because no assumptions can be made about the validity of register contents, in
  //  particular the SSP, neither the program counter nor the status register is saved.
  //  The address in the first two words of the reset exception vector is fetched as the
  //  initial SSP,"
  SP=ROM_LPEEK(0); // until changed by TOS or program ; we count no timing
  // "and the address in the last two words of the reset exception vector is fetched as
  // the initial program counter."
  SET_PC(ROM_LPEEK(4)); // we count no timing
  // "The processor is forced into the supervisor state, and the trace state is forced off.
  //  The interrupt priority mask is set at level 7."
  SR=0x2700;
#if defined(SSE_420R5)
  UPDATE_FLAGS;
#endif
  vbase=0; // anyway

  if(Warm)
  {
#if !defined(SSE_LIBRETRONUKE) && !defined(SSE_HALT_CPUONLY)
    if(Cpu.ProcessingState==TMC68000::HALTED)
    {
      CLICK_PLAY_BUTTON(); // ST can be reset on HALT
    }
#endif
    Cpu.ProcessingState=TMC68000::NORMAL;
    reset_peripherals(false);
#ifndef DISABLE_STEMDOS
    Stemdos.SetDriveReset();
#endif
    if(runstate==RUNSTATE_STOPPED && OPTION_HACKS) // Hack alert! Show user reset has happened
    {
      draw_end();
      draw(false);
    }
  }
  else // cold
  {
#if !defined(SSE_LIBRETRONUKE)
    if(ChangeSettings) 
      GUIColdResetChangeSettings();
#endif
    power_on();
    palette_convert_all();
    if(ResChangeResize) 
      StemWinResize();
#if !defined(SSE_LIBRETRONUKE)
    if(runstate!=RUNSTATE_RUNNING)
    {
      draw_end();
      draw(true); // black now
    }
#endif
    ComputerRestore();
#if defined(SSE_STATS)
    StatsStatic.nPatchedBytes=0;
#endif
  }
  //SP=ROM_LPEEK(0); // until changed by TOS or program
#if defined(SSE_420R2B)
  //SET_PC(ROM_LPEEK(4)); // demo Extended Knobhead option C3 wants it to be that and not rom_addr
                          // why? the only difference is a BRA in rom
  //SR=0x2700;
#endif
  UPDATE_FLAGS;
#ifdef SSE_DEBUGGER
  debug_reset();
#endif
  VideoFreqAtStartOfVbl=Glue.VideoFreq;
#if !defined(SSE_LIBRETRONUKE)
  PasteIntoSTAction(STPASTE_STOP);
  CheckResetIcon();
  CheckResetDisplay();
#endif
  //scan_y=0;
#ifndef NO_CRAZY_MONITOR
  line_a_base=vdi_intout=0;
  aes_calls_since_reset=0;
  if(extended_monitor)
  {
    extended_monitor=1; //first stage of extmon init
    SafeLPoke(SV_PHYSTOP,mem_len); //$42E
  }
#endif
#ifndef DISABLE_STEMDOS
  Stemdos.Reset();
#endif
  Draw.MarshalParameters();
#if defined(SSE_EMU_THREAD)
  VideoLock.disabled=old_vlock;
#endif
  DiskEmu.crc16.Reset();
  DiskEmu.crc16.Add(0xFB);

  TRACE("%s F%d y%d ","RESET",FRAME,scan_y); 
  Debug.TraceGeneralInfos(TDebug::RESET);
}


/*  exception() executes a C longjmp, must be called from the thread where
    it was set up - we use agenda, not event, in part because we have a 
    parameter.
*/

void agenda_reset(int flags) { // from GUI
  exception(EXCEPTION_RESET,0,flags);
}

#undef LOGSECTION
