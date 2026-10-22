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

DOMAIN: I/O
FILE: ior.cpp
DESCRIPTION: The 68000 uses memory-mapped I/O
On the ST, addresses from $ff8000 onwards are mapped to peripherals.
They are decoded by the Glue and some other chips like the Blitter.
Waitstates are possible with some devices.
For performance, we could have different IO functions for STF/STE (as usual
see trade-off with code bloat though, same consideration for one unique
R/W function).
This file handles reading from device registers.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <computer.h>
#include <stjoy.h>
#include <debug.h>
#include <loadsave.h>
#if defined(SSE_DEBUGGER_FRAME_REPORT)
#include <debug_framereport.h>
#include <iolist.h>
#endif
#if defined(SSE_VID_STVL1)
#include <interface_stvl.h>
#endif
#include <osd.h>


// read word or byte (according to BUS_MASK) from memory-mapped device
WORD io_read(MEM_ADDRESS const addr) {
#ifdef DEBUG_BUILD
  DEBUG_CHECK_READ_IO_W(addr);
#endif
  //ASSERT(!(addr&1));
  DU16 ud16;
  BYTE& hibyte=ud16.d8[HI];
  BYTE& lobyte=ud16.d8[LO];
  WORD& d16=ud16.d16;
  d16=0xffff; //default return value
  //ASSERT((BUS_MASK&BUS_MASK_WRITE)==0);
  //ASSERT(BUS_MASK&BUS_MASK_WORD);

  if(!SUPERFLAG
#ifdef SSE_MMU_MONSTER_ALT_RAM
    && (addr&0xfffffe)!=0xfffe00 // alt-RAM doesn't require supervisor mode (grr)
#endif
    )
    exception(EXCEPTION_BUS_ERROR,EA_READ,addr);

  bool lds=((BUS_MASK&BUS_MASK_LOBYTE)==BUS_MASK_LOBYTE); // LOWER DATA STROBE
  bool uds=((BUS_MASK&BUS_MASK_HIBYTE)==BUS_MASK_HIBYTE); // UPPER DATA STROBE
  a_s_t=A_S_T; // this gets updated in case of waitstates

  // Main switch: address groups

#ifdef BIG_ENDIAN_PROCESSOR
#error TODO
#else
  BYTE ad_group=*(((BYTE*)&addr)+1); // first byte of 24bit address
  BYTE ad_byte=*(((BYTE*)&addr)); // last byte of address
#endif
  switch(ad_group) {


  //////////////////////
  // RAM (MMU CONFIG) //
  //////////////////////

  case 0x80:

    if(ad_byte==0x00)
      lobyte=(mem_len>MEM_4MB) ? (BYTE)(MEMCONF_2MB|(MEMCONF_2MB<<2)) : Mmu.Config;
    else if(ad_byte>((IS_STE||(ST_MODEL==STF)) ? 0x0eU : 0x0cU)) //forbidden range
      exception(EXCEPTION_BUS_ERROR,EA_READ,addr);
    else if(IS_STE) //v402, not tested...
      lobyte=0;
    break;


  //////////////////////////////
  // Video (MMU-GLUE-Shifter) //
  //////////////////////////////

#define LOGSECTION LOGSECTION_MMU

  case 0x82:

    if(IS_STE)
    {
      // Below $10 - Odd bytes return value or 0, even bytes return 0xfe/0x7e
      // Above $40 - Unused return 0
      lobyte=0;
      hibyte=(ad_byte>0x40) ? 0 : 0xfe; //?
    }
    if(ad_byte>=0x40 && ad_byte<0x80) // Shifter - always word access
    {  
      DWORD shifter_reg=(ad_byte-0x40)>>1;
      // Shifter access -> wait states possible
      if(sys_cycles&(4*TICKS8-1))
      {
        BUS_WAIT_STATES((sys_cycles&(4*TICKS8-1))/TICKS8); //self-optimising or use other def
      }
      Blitter.BlitCycles=0;
      switch(shifter_reg) {
      case 16: // rez
        hibyte=Shifter.ShiftMode;//&3;
        if(IS_STF)
          d16|=0xFCFF&dbus;
        break;
      case 18: // scroll
        if(IS_STE)
        {
          d16=shifter_hscroll;
/*  MCU schematics show that SCRLSEL is the clock of the NOSCROLL register (sheet 10).
    This tests the address and LDS but not R/W. That means that the value is changed
    on reading the GSTShifter register!
    Case: Kultur Melk */
          if(lds)
          {
            if(stem_runmode==STEM_MODE_CPU)
              Glue.hscroll=(d16!=0);
#if defined(SSE_VID_STVL_UPD)
            Stvl.noscroll=!Glue.hscroll;
#endif
#if defined(SSE_DEBUGGER_FAKE_IO)
            if(TRACE_MASK1&TRACE_CONTROL_GLUREG)
            {
              TRACE_LOG("%d %d %d MCU NOSCROLL %d\n",TIMING_INFO,!Glue.hscroll);
            }
#endif
          }
        }
        else
          d16=dbus; // Shifter unused bits replaced with data bus bits
        break;
      default: 
        if(shifter_reg<16) // palette
        {
          d16=STpal[shifter_reg];
          if(IS_STF)
            // Shifter unused bits replaced with data bus bits
            // eg UMD 8730, Wings of Death-SUP, KCD001
            d16|=(0xF888&dbus);
        }
        else if(IS_STF)
          d16=dbus; 
        break;
      }//sw
    }
    else if((ad_byte>0x0e && ad_byte<0x40) //forbidden gap
          ||(ad_byte>0x7e)) //forbidden area after SHIFTER
      exception(EXCEPTION_BUS_ERROR,EA_READ,addr);
    else if(ad_byte==0x0a && uds) // GLUE synchronization mode
    {
      hibyte&=~3;
      hibyte|=Glue.SyncMode;
    }
    else if(lds) // MMU video registers
    {
      switch(ad_byte) { 
      case 0x00:  //high byte of screen memory address
        lobyte=Mmu.uVBase.d8[B2];
        break;
      case 0x02:  //mid byte of screen memory address
        lobyte=Mmu.uVBase.d8[B1];
        break;
      // vcount: should be fast, that's why code is duplicated for 3 registers
      case 0x04: // video counter high byte
#if defined(SSE_VID_STVL1)       
        if(OPTION_C3)
          lobyte=Stvl.vcount.d8[B2];
        else
#endif          
        {
          DU32 vc(Mmu.ReadVideoCounter(LINECYCLES));
          lobyte=vc.d8[B2];
        }
#if defined(SSE_STATS)
        Stats.nReadvc++;
        Stats.nReadvc1++;
#endif
        if(stem_runmode==STEM_MODE_CPU)
        {
#if defined(SSE_DEBUGGER_FRAME_REPORT)
          if((FRAME_REPORT_MASK1&FRAME_REPORT_MASK_VCOUNT))
            FrameEvents.Add(scan_y,LINECYCLES/TICKS8,'c',(0x0500|lobyte));
#endif
        }
        break;
      case 0x06: // video counter mid byte
#if defined(SSE_VID_STVL1)       
        if(OPTION_C3)
          lobyte=Stvl.vcount.d8[B1];
        else
#endif          
        {
          DU32 vc(Mmu.ReadVideoCounter(LINECYCLES));
          lobyte=vc.d8[B1];
        }
#if defined(SSE_STATS)
        Stats.nReadvc++;
        Stats.nReadvc1++;
#endif
        if(stem_runmode==STEM_MODE_CPU)
        {
#if defined(SSE_DEBUGGER_FRAME_REPORT)
          if((FRAME_REPORT_MASK1&FRAME_REPORT_MASK_VCOUNT))
            FrameEvents.Add(scan_y,LINECYCLES/TICKS8,'c',(0x0700|lobyte));
#endif
        }
        break;
      case 0x08: // video counter low byte - used by demos to synchronise
#if defined(SSE_VID_STVL1)       
        if(OPTION_C3)
          lobyte=Stvl.vcount.d8[B0];
        else
#endif          
        {
          DU32 vc(Mmu.ReadVideoCounter(LINECYCLES));
          lobyte=vc.d8[B0];
        }
#if defined(SSE_STATS)
        Stats.nReadvc++;
        Stats.nReadvc1++;
#endif
        if(stem_runmode==STEM_MODE_CPU)
        {
#if defined(SSE_DEBUGGER_FRAME_REPORT)
          if((FRAME_REPORT_MASK1&FRAME_REPORT_MASK_VCOUNT))
            FrameEvents.Add(scan_y,LINECYCLES/TICKS8,'c',(0x0900|lobyte));
#endif
        }
        break;
      case 0x0c:  //low byte of screen memory address - it is here because it was later added on the STE
        if(IS_STE)
          lobyte=Mmu.uVBase.d8[B0];
        break;
      case 0x0e: // LINEWID
        if(IS_STE)
          lobyte=Mmu.linewid;
        else if(ST_MODEL!=STF) // STFM, Mega: simplification
          exception(EXCEPTION_BUS_ERROR,EA_READ,addr);
        break;
      }//sw
    }//if(lds)
    break;

#undef LOGSECTION


  ////////////////////////
  // Disk (MMU-DMA-FDC) //
  ////////////////////////

#define LOGSECTION LOGSECTION_FDC

  case 0x86:
  {

    const BYTE &drive=Psg.SelectedDrive;

    // test for bus error (there's a STF/STFM difference)
    if(ad_byte>((IS_STE||(ST_MODEL==STF))?0x0e:0x0c) || ad_byte<0x04
      || ad_byte<0x08 && (BUS_MASK&BUS_MASK_WORD)!=BUS_MASK_WORD)
      exception(EXCEPTION_BUS_ERROR,EA_READ,addr);
    switch(ad_byte) {
    case 0x04:
      // sector counter
      if(Dma.mcr&Dma.CR_COUNT_OR_REGS)
      {
        ; // keep ffff?
      }
      // HD access
      else if(Dma.mcr&TDma::CR_HDC_OR_FDC)
      {
#if defined(SSE_ACSI)
        if(ACSI_EMU_ON || OPTION_LASER && acsi_dev==ACSI_ID_LASER)
          lobyte=AcsiHdc[acsi_dev].IORead();
#endif
      }
      else if(!(Dma.mcr&Dma.CR_DRQ_FDC_OR_HDC)) // we can't
      {
        TRACE_LOG("No FDC access DMA MCR %x\n",Dma.mcr);
      }
      // Read FDC register
      else
        lobyte=Fdc.IORead((Dma.mcr&(Dma.CR_A1|Dma.CR_A0))/2);
      break;
    case 0x06:  // DMA status
      ASSERTB((Dma.sr&b00000111)==Dma.sr);
#ifdef SSE_LEAN_AND_MEAN
      d16=Dma.sr;
#else
      d16=(Dma.sr&b00000111);
#endif
      break;
    case 0x08:  // DMA pointer high
      lobyte=Mmu.uDmaCounter.d8[B2];
      break;
    case 0x0a:  // DMA pointer Mid
      lobyte=Mmu.uDmaCounter.d8[B1];
      break;
    case 0x0c:  // DMA pointer Low
      lobyte=Mmu.uDmaCounter.d8[B0];
      break;
    case 0x0e: //frequency/density control
#if defined(SSE_MEGASTE)
      if(IS_MEGASTE)
        lobyte=MegaSte.FdHd;
      else
#endif
      if(OPTION_HACKS)
        lobyte=(FloppyDisk[drive].Density==2) ? 3 : 0;
      break;
    }//sw
#if USE_PASTI 
/*  Pasti handles all Dma reads */
#if defined(SSE_DISK_GHOST)
    if(!Fdc.Lines.CommandWasIntercepted)
#endif
    {
      if(DiskEmu.PastiOperation())
      {
        struct pastiIOINFO pioi;
        pioi.stPC=pc;
        pioi.cycles=a_s_t/TICKS8;
        // pasti uses odd addresses for byte access
        if(ad_byte<0x08)
        {
          pioi.addr=addr;
          pasti->Io(PASTI_IOREAD,&pioi);
          d16=(WORD)pioi.data;
        }
        else
        {
          pioi.addr=addr;
          if((BUS_MASK&BUS_MASK_LOBYTE))
            pioi.addr+=1;
          pasti->Io(PASTI_IOREAD,&pioi);
          if((BUS_MASK&BUS_MASK_WORD)==BUS_MASK_WORD)
            d16=(WORD)pioi.data;
          else if((BUS_MASK&BUS_MASK_LOBYTE))
            lobyte=(BYTE)pioi.data;
          else
            hibyte=(BYTE)pioi.data;
        }
        pasti_handle_return(&pioi);
      }
    }
#endif//USE_PASTI 
    // read FDC STR for Pasti or Caps
    if(ad_byte==0x04 && !(Dma.mcr&(Dma.CR_A1|Dma.CR_A0))
      && (FloppyDrive[drive].ImageType.Manager==MNGR_PASTI
      ||FloppyDrive[drive].ImageType.Manager==MNGR_CAPS))
    {
/*  Media change (changing the floppy disk) on the ST is managed in
    an intricate way, using a timed interrupt to check "write protect"
    status. A change in this status indicates that a disk is being moved
    before the diode in the drive that detects "write protect".    */
      if(floppy_mediach[drive]
        &&!(Dma.mcr&Dma.CR_COUNT_OR_REGS)&&!(Dma.mcr & (Dma.CR_A0|Dma.CR_A1)))
      {
        lobyte&=~FDC_STR_WP;
        if(floppy_mediach[drive]/10!=1)
          lobyte|=FDC_STR_WP;
      }
#if defined(SSE_MEGASTE)
      if(IS_MEGASTE && fdc_check_wrong_density())
        lobyte|=FDC_STR_SE;
#endif
    }
#if defined(SSE_DEBUGGER_FAKE_IO)
    if(stem_runmode==STEM_MODE_CPU)
    if(!((Dma.mcr&Dma.CR_COUNT_OR_REGS) ||
      (Dma.mcr&TDma::CR_HDC_OR_FDC) ||(!(Dma.mcr&Dma.CR_DRQ_FDC_OR_HDC))))
    { // Read FDC register
      if(TRACE_MASK3&TRACE_CONTROL_FDCREGS)
      switch((Dma.mcr&(Dma.CR_A1|Dma.CR_A0))/2) {
      case 1:
        TRACE_LOG("FDC %s R TR %d PC %X\n",disk_manager[DiskEmu.LastManager],lobyte,old_pc);
        break;
      case 2:
        TRACE_LOG("FDC %s R SR %d PC %X\n",disk_manager[DiskEmu.LastManager],lobyte,old_pc);
        break;
      case 3:
        TRACE_LOG("FDC %s R DR %d PC %X\n",disk_manager[DiskEmu.LastManager],lobyte,old_pc);
        break;
      }//sw
      if((TRACE_MASK3&TRACE_CONTROL_FDCSTR)&&!(Dma.mcr&(Dma.CR_A1|Dma.CR_A0)))
      {
        static BYTE str2=0;
        static MEM_ADDRESS old_pc2=0;
        if(lobyte!=str2||old_pc!=old_pc2)
        {
          TRACE_LOG("FDC %s STR %X ",disk_manager[DiskEmu.LastManager],lobyte);
          DiskEmu.TraceStatus(lobyte);
          TRACE_LOG("PC %X (...)\n",old_pc);
          str2=lobyte;
          old_pc2=old_pc;
        }
      }
    }//if
#endif
    break;
  }


#undef LOGSECTION
#define LOGSECTION LOGSECTION_SOUND


  ////////////////
  // YM2149 PSG //
  ////////////////

  case 0x88:

    BUS_WAIT_STATES(1); // GLUE delays DTACK by one cycle (often rounded up)
    if(!(ad_byte&BIT_1))
    { //read data / register select, mirrored at 4,8,12,...
      if(psg_reg_select==PSGR_PORT_A && !(psg_reg[PSGR_MIXER]&BIT_6))
      {
        // Drive A, drive B, side, RTS, DTR, strobe and monitor GPO
        // are normally set by ST
        hibyte=psg_reg[PSGR_PORT_A];
        // Parallel port 0 joystick fire (strobe)
        if(stick[N_JOY_PARALLEL_0]&BIT_4)
        {
          if(stick[N_JOY_PARALLEL_0]&BIT_7)
            hibyte&=~BIT_5;
          else
            hibyte|=BIT_5;
        }
      }
      else if(psg_reg_select==PSGR_PORT_B && !(psg_reg[PSGR_MIXER]&BIT_7))
      {
        if(!(stick[N_JOY_PARALLEL_0]&BIT_4) && !(stick[N_JOY_PARALLEL_1]&BIT_4)
          && ParallelPort.IsOpen())
        {
          ParallelPort.NextByte();
          UpdateCentronicsBusyBit();
          hibyte=ParallelPort.ReadByte();
        }
        else
          hibyte=~((stick[N_JOY_PARALLEL_0]&0xF)|((stick[N_JOY_PARALLEL_1]&0xF)<<4));
      }
      else
        hibyte=psg_reg_data;
    }
    break;


  ///////////////////////
  // STE Digital Sound //
  ///////////////////////

  case 0x89:

    if(ad_byte>0x3e||IS_STF)
      exception(EXCEPTION_BUS_ERROR,EA_READ,addr);

    if((ad_byte&0x20)==0x20) // Shifter sound registers
    {
      // Shifter access -> wait states possible
      if(sys_cycles&(4*TICKS8-1))
      {
        BUS_WAIT_STATES((sys_cycles&(4*TICKS8-1))/TICKS8);
      }
      Blitter.BlitCycles=0;
      switch(ad_byte) {
      case 0x20:   //Sound mode control
        lobyte=shifter_sound_mode;
        break;
      case 0x22:          // MicroWire data
      case 0x24:          // MicroWire mask
      {
        WORD dat=0;
        WORD mask=Microwire.Mask;
        if(Microwire.StartTime)
        {
          int nShifts=(int)(a_s_t-Microwire.StartTime)/(8*TICKS8);
          if(nShifts>15)
            Microwire.StartTime=0;
          else
          {
            dat=Microwire.Data<<nShifts;
            while(nShifts--)
            {
              bool lobit=((mask&BIT_15)!=0);
              mask<<=1;
              mask|=(WORD)lobit;
            }//wend
          }//if
        }//if
        d16=(ad_byte&2) ? dat : mask;
        break;
      }//case
      }//sw
    }//if
    else if(lds)
    {
      switch(ad_byte) {
      case 0x00:   //DMA control register
        lobyte=Mmu.SoundControl;
        break;
      case 0x02:   //HiByte of frame start address
        lobyte=Mmu.uNextSoundFrameStart.d8[B2];
        break;
      case 0x04:   //MidByte of frame start address
        lobyte=Mmu.uNextSoundFrameStart.d8[B1];
        break;
      case 0x06:   //LoByte of frame start address
        lobyte=Mmu.uNextSoundFrameStart.d8[B0];
        break;
      case 0x08:   //HiByte of frame address counter
        lobyte=Mmu.uSoundFetchAd.d8[B2];
        break;
      case 0x0a:   //MidByte of frame address counter
        lobyte=Mmu.uSoundFetchAd.d8[B1];
        break;
      case 0x0c:   //LoByte of frame address counter
        lobyte=Mmu.uSoundFetchAd.d8[B0];
        if(stem_runmode==STEM_MODE_CPU)
        {
          TRACE_LOG("F%d Y%d PC%X C%d Read sound frame counter %X (%X->%X)\n",FRAME,scan_y,old_pc,
                    LINECYCLES,SteSndFetchAd,SteSndFrameStart,SteSndFrameEnd);
        }
        break;
      case 0x0e:   //HiByte of frame end address
        lobyte=Mmu.uNextSoundFrameEnd.d8[B2]; 
        break;
      case 0x10:   //MidByte of frame end address
        lobyte=Mmu.uNextSoundFrameEnd.d8[B1];
        break;
      case 0x12:   //LoByte of frame end address
        lobyte=Mmu.uNextSoundFrameEnd.d8[B0];
        break;
      }//sw
    }//if
    break;

#undef  LOGSECTION


  /////////////
  // BLiTTER //
  /////////////

  case 0x8a: 
  {

#ifdef DISABLE_BLITTER

    exception(EXCEPTION_BUS_ERROR,EA_READ,addr);
    break;

#else

    if(!SSEConfig.Blitter)
    {
      exception(EXCEPTION_BUS_ERROR,EA_READ,addr);
      break; // useful?
    }
    int Offset=ad_byte>>1;
    // uds or lds or both OK
    if(Offset<0x10)
      d16=Blitter.HalfToneRAM[Offset];
    else switch(Offset) {
    case 0x10:
      d16=Blitter.SrcXInc;
      break;
    case 0x11:
      d16=Blitter.SrcYInc;
      break;
    case 0x12:
      d16=Blitter.SrcAdr.d16[HI];
      break;
    case 0x13:
      d16=Blitter.SrcAdr.d16[LO];
      break;
    case 0x14:
      d16=Blitter.EndMask[0];
      break;
    case 0x15:
      d16=Blitter.EndMask[1];
      break;
    case 0x16:
      d16=Blitter.EndMask[2];
      break;
    case 0x17:
      d16=Blitter.DestXInc;
      break;
    case 0x18:
      d16=Blitter.DestYInc;
      break;
    case 0x19:
      d16=Blitter.DestAdr.d16[HI];
      break;
    case 0x1a:
      d16=Blitter.DestAdr.d16[LO];
      break;
    case 0x1b:
      d16=(WORD)Blitter.XCounter;
      break;
    case 0x1c:
      d16=(WORD)Blitter.YCounter;
      break;
    case 0x1d:
      hibyte=Blitter.Hop;
      lobyte=Blitter.Op;
      break;
    case 0x1e:
      hibyte=Blitter.LineNumber|Blitter.Smudge|Blitter.Hog|Blitter.rBusy;
      lobyte=Blitter.Skew|Blitter.NFSR|Blitter.FXSR;
      break;
    case 0x1f:
      d16=0;
      break;
    default:
      exception(EXCEPTION_BUS_ERROR,EA_READ,addr);
    }//sw
#if defined(SSE_DEBUGGER_FRAME_REPORT)
    if(FRAME_REPORT_MASK2 & FRAME_REPORT_MASK_BLITTER)
      FrameEvents.Add(scan_y,LINECYCLES/TICKS8,'b',((Offset<<16)|d16));
#endif

#endif//#ifdef DISABLE_BLITTER
    break;
  }//case


  //////////////
  // MEGA STE //
  //////////////

#if defined(SSE_MEGASTE)
  case 0x8C:
    if(IS_MEGASTE && lds && (ad_byte&0x80))
    { // 1 3 5 7 -> 0 2 4 6 -> 0 1 2 3
      int reg=(ad_byte&0x7)>>1;
      lobyte=MegaSte.Scc[reg]; // what was written... TODO
    }
    else 
      exception(EXCEPTION_BUS_ERROR,EA_READ,addr);
    break;

  case 0x8E:
    if(IS_MEGASTE && lds && ad_byte<=0x20)
    {
      switch(ad_byte) {
      case 0x00:
        lobyte=MegaSte.VmeSysMask; // VME bus not emulated
        break;
      case 0x02:
        lobyte=MegaSte.VmeSysStat;
        break;
      case 0x04:
        lobyte=MegaSte.VmeSysInt;
        break;
      case 0x0c:
        lobyte=MegaSte.VmeMask;
        break;
      case 0x0e:
        lobyte=MegaSte.VmeStat;
        break;
      case 0x20:
        lobyte&=~3;
        lobyte|=Cpu16.ScuReg&3;
        break;
      }//sw
    }
    else 
      exception(EXCEPTION_BUS_ERROR,EA_READ,addr);
    break;
#endif


  //////////////
  // GAMECART //
  //////////////

  case 0x90:
    if(IS_STF || ad_byte>0)
      exception(EXCEPTION_BUS_ERROR,EA_READ,addr);
    if(uds)
      hibyte=Glue.gamecart;
    break;


  /////////////////////////////////////////////
  // STE controllers - Mega STE DIP switches //
  /////////////////////////////////////////////

  case 0x92:
  {
    bool Illegal=false;
#if defined(SSE_MEGASTE)
    if(IS_MEGASTE)
      d16=0xBFFF; // DIP switches - HD FD enabled is DIP7 bit6
    else
#endif
      d16=JoyReadSTEAddress(addr,&Illegal);
    if(Illegal||IS_STF) // thx Petari
      exception(EXCEPTION_BUS_ERROR,EA_READ,addr);
    break;
  }


  ///////////////////////////////
  // Falcon 256 colour palette //
  ///////////////////////////////

#if !defined(SSE_NO_FALCONMODE)
  case 0x98:
  case 0x99:
  case 0x9a:
  case 0x9b:
//#if !defined(SSE_NO_FALCONMODE)
    if(emudetect_falcon_mode)
    {
      int n=(addr-0xff9800)/4;
      DWORD val=emudetect_falcon_stpal[n];
      d16=(addr&2)?(val>>16):(val&0xffff);
    }
    else
//#endif
      exception(EXCEPTION_BUS_ERROR,EA_READ,addr);
    break;
#endif


  ////////////////////////
  // Emu Fake Registers //
  ////////////////////////

  case 0xc1:

#ifdef DEBUG_BUILD
    if(ad_byte==0x22 && OPTION_EMU_DETECT)
      d16=(WORD)runstate;
#endif
    if(emudetect_called)
    {
      switch(ad_byte) {
      case 0x00:
        hibyte=(stem_version_text[0]-'0');
        {
          Str minor_ver=(char*)stem_version_text+2;
          for(int i=0;i<minor_ver.Length();i++) 
          {
            if(minor_ver[i]<'0'||minor_ver[i]>'9') 
            {
              minor_ver.SetLength(i);
              break;
            }
          }
          int ver=atoi(minor_ver.RPad(2,'0'));
          lobyte=(BYTE)(((ver/10)<<4)|(ver%10));
          break;
        }
        break;
      case 0x02:
        hibyte=(BYTE)slow_motion;
        lobyte=(BYTE)(slow_motion_speed/10);
        break;
      case 0x04:
        hibyte=(BYTE)fast_forward;
        lobyte=(BYTE)(nSysCyclesPerSecond/1000000);
        break;
      case 0x06:
        hibyte=0 DEBUG_ONLY(+1);
        lobyte=snapshot_loaded;
        break;
      case 0x08:
        ASSERTB(run_speed_ticks_per_second);
#ifndef SSE_LEAN_AND_MEAN
        if(run_speed_ticks_per_second)
#endif
          d16=(WORD)(100000/run_speed_ticks_per_second);
        break;
      case 0x0a:
        ASSERTB(avg_frame_time && Glue.VideoFreq);
#ifndef SSE_LEAN_AND_MEAN
        if(avg_frame_time && Glue.VideoFreq)
#endif
          d16=(WORD)(((((NFRAME_TIME_AVG*1000)/avg_frame_time)*100)/Glue.VideoFreq));
        break;
      // 32bit - TODO x64?
      case 0x0c:
        d16=HIWORD(a_s_t);
        break;
      case 0x0e:
        d16=LOWORD(a_s_t);
        break;
      case 0x10:
        d16=HIWORD(sys_time_of_last_vbl);
        break;
      case 0x12:
        d16=LOWORD(sys_time_of_last_vbl);
        break;
      case 0x14:
        d16=HIWORD(TimeOfHSyncOff);
        break;
      case 0x16:
        d16=LOWORD(TimeOfHSyncOff);
        break;
      case 0x18:
        d16=scan_y;
        break;
      case 0x1a:
        hibyte=emudetect_write_logs_to_printer;
#if !defined(SSE_NO_FALCONMODE)
        lobyte=emudetect_falcon_mode;
#endif
        break;
      case 0x1c:
#if !defined(SSE_NO_FALCONMODE)
        hibyte=((emudetect_falcon_mode_size-1)+(emudetect_falcon_extra_height?2:0));
        lobyte=emudetect_overscans_fixed;
#endif
        break;
      default: 
        d16=0;
      }//sw
      break;
    }//if(emudetect_called)
    exception(EXCEPTION_BUS_ERROR,EA_READ,addr);


  /////////
  // MFP //
  /////////

#define LOGSECTION LOGSECTION_MFP

  case 0xfa:
  {
    if(ad_byte<0x40 && lds) // conditions for MFP DTACK
    {
      if(SSEOptions.MfpWsTmg[0])
      {
        BUS_WAIT_STATES(SSEOptions.MfpWsTmg[0]);
      }
      if(ad_byte==0x00)
      {
        lobyte=Mfp.reg[MFPR_GPIP]&(~Mfp.reg[MFPR_DDR]);
        lobyte|=Mfp.gpip_buffer&Mfp.reg[MFPR_DDR];
#if defined(SSE_DONGLE)
/*  Some dongles modify the GPIP register.
    The dongle for BAT2 on the ST is simplistic. It just permanently changes
    a bit.
    The dongle for Music Master looks more like the dongle for BAT2 on the
    Amiga. The program changes a line and checks the effect on another line
    at different times.
*/
        switch(DONGLE_ID) {
#if defined(SSE_DONGLE_BAT2)
        case TDongle::BAT2:
          lobyte&=~MFP_GPIP_CTS_MASK;
          break;
#endif
#if defined(SSE_DONGLE_MUSIC_MASTER)
        case TDongle::MUSIC_MASTER:
        { //inspired by WinUAE
          int bit=(a_s_t-Dongle.Timing>200*TICKS8) ? (Dongle.Value&1) : (Dongle.Value&2);
          if(bit)
            lobyte|=MFP_GPIP_DCD_MASK;
          else
            lobyte&=~MFP_GPIP_DCD_MASK;
        }
        break;
#endif
        }//sw
#endif
      }
      else if(ad_byte<0x30)
      {
        BYTE n=((ad_byte&0xFE)>>1);
        if(n>=MFPR_TADR && n<=MFPR_TDDR)
        { // timer data registers
          BYTE ti=n-MFPR_TADR;
          int adjust=0;
          if(SSEOptions.MfpReadTclk||SSEOptions.MfpReadSync)
            adjust=Mfp.SyncXtal(a_s_t,SSEOptions.MfpReadTclk);
          adjust+=SSEOptions.MfpReadCpu;
          Mfp.CalcTimerCounter(ti,a_s_t+adjust);
          lobyte=(BYTE)(mfp_timer_counter[ti]/64);
        }
        else if(n>=MFPR_SCR)
        {
          lobyte=RS232_ReadReg(n);
#undef LOGSECTION
#define LOGSECTION LOGSECTION_PORTS
          TRACE_LOG("RS232 %s R %X\n",mfp_reg_name[n],lobyte);
        }
        else
          lobyte=Mfp.reg[n];
      }
      if(SSEOptions.MfpWsTmg[1]) // default - because reading is twice slower?
      {
        BUS_WAIT_STATES(SSEOptions.MfpWsTmg[1]);
      }
    }
    else
      exception(EXCEPTION_BUS_ERROR,EA_READ,addr);
    break;
  }


  ///////////////////////////
  // ACIAs (IKBD and MIDI) //
  ///////////////////////////

#undef LOGSECTION
#define LOGSECTION LOGSECTION_ACIA

  case 0xfc:
    TMC6850::SyncEClock(); // VPA cycle, no DTACK, no BERR - we do it before reading
    if(uds)
    {
      BYTE acia_num=((ad_byte&4)>>2);
      switch(ad_byte) {// ACIA registers
      case 0x00: // Keyboard ACIA Control
      case 0x04: // MIDI ACIA Control
        //ASSERT(acia_num==0||acia_num==1);
        if(OPTION_C1)
        {
          hibyte=acia[acia_num].sr;
          break;
        }//C1
#if !defined(SSE_NO_OLD_IKBD)
        hibyte=0;
        if(acia[acia_num].rx_not_read||acia[acia_num].overrun==ACIA_OVERRUN_YES) 
          hibyte|=ACIA_RDRF;
        if(acia[acia_num].tx_flag==FALSE)
          hibyte|=ACIA_TDRE; 
        if(acia[acia_num].irq)
          hibyte|=ACIA_IRQ;
        if(acia[acia_num].overrun==ACIA_OVERRUN_YES)
          hibyte|=ACIA_OVRN;
#endif
        break;
      case 0x02:  // Keyboard ACIA Data
      case 0x06:  // MIDI ACIA Data
        //ASSERT(acia_num==0||acia_num==1);
        if(stem_runmode!=STEM_MODE_CPU)
        {
          hibyte=(OPTION_C1)?acia[acia_num].rdr:acia[acia_num].data;
          break;
        }
        if(OPTION_C1)
        {
          // Update status BIT 5 (overrun)
          if(acia[acia_num].overrun==ACIA_OVERRUN_COMING) // keep this, it's right
          {
            acia[acia_num].overrun=ACIA_OVERRUN_YES;
            acia[acia_num].sr|=ACIA_OVRN; // set overrun (only now, conform to doc)
            TRACE_LOG("%d %d %d PC %X reads ACIA %d RDR %X, OVR\n",
                      TIMING_INFO,old_pc,acia_num,acia[acia_num].rdr);
          }
          // no overrun, normal
          else
          {
            /*"The Overrun indication is reset after the reading of data from the
            Receive Data Register."
            ACIA02.TOS: reading ACIA RDR once after overrun bit is set is enough
            to clear overrun*/
            acia[acia_num].overrun=ACIA_OVERRUN_NO;
            acia[acia_num].sr&=~(ACIA_RDRF|ACIA_OVRN);
            TRACE_LOG("%d %d %d PC %X CPU reads ACIA %d RDR %X\n",
                      TIMING_INFO,old_pc,acia_num,acia[acia_num].rdr);
          }
          ACIA_CheckIrq(acia_num);
          hibyte=acia[acia_num].rdr;
          break;
        }//C1
#if !defined(SSE_NO_OLD_IKBD)
        {//scope
          acia[acia_num].rx_not_read=FALSE;
          if(acia[acia_num].overrun==ACIA_OVERRUN_COMING) 
          {
            acia[acia_num].overrun=ACIA_OVERRUN_YES;
            if(acia[acia_num].rx_irq_enabled)
              acia[acia_num].irq=TRUE;
            TRACE_LOG("%d %d %d PC %X reads ACIA %d RDR %X, OVR\n",
                      TIMING_INFO,old_pc,acia_num,acia[acia_num].data);
          }
          else 
          {
            acia[acia_num].overrun=ACIA_OVERRUN_NO;
            // IRQ should be off for receive, but could be set for tx empty interrupt
            acia[acia_num].irq=(acia[acia_num].tx_irq_enabled && acia[acia_num].tx_flag==FALSE);
            TRACE_LOG("%d %d %d PC %X CPU reads ACIA %d RDR %X\n",
              TIMING_INFO,old_pc,acia_num,acia[acia_num].data);
          }
          mfp_gpip_set_bit(MFP_GPIP_ACIA_BIT,!(acia[ACIA_IKBD].irq||acia[ACIA_MIDI].irq));
          hibyte=acia[acia_num].data;
          break;
        }
#endif//#if !defined(SSE_NO_OLD_IKBD)
        break;
      }
    }
    break;


  //////////////
  // MEGA RTC //
  //////////////

  case 0xfd:
    TMC6850::SyncEClock();
    if(SSEConfig.Mega&&lds&&ad_byte>=0x20&&ad_byte<=0x3E)
    {
#if defined(SSE_MEGA_RTC)
      d16=MegaRtc.Read(addr);
#endif
      break;
    }


  /////////////
  // Alt-RAM //
  /////////////

#if defined(SSE_MMU_MONSTER_ALT_RAM)
  case 0xfe: 
    if(mem_len!=0xC00000 || (bus_mask&BUS_MASK_WORD)!=BUS_MASK_WORD
      || ad_byte>8 || (ad_byte==8&&!SUPERFLAG))
      exception(EXCEPTION_BUS_ERROR,EA_READ,addr);
    else switch(ad_byte) {
    case 0:
      d16=(WORD)((Mmu.MonSTerHimem) ? (Mmu.MonSTerHimem/0x100000-4) : 0);
      break;
    case 8:
      d16=1; // "firmware version"
      break;
    default:
      d16=0;
    }//sw
    break;
#endif

  default: //not in allowed area - no DTACK
    exception(EXCEPTION_BUS_ERROR,EA_READ,addr);

  }//sw

#if defined(SSE_DEBUGGER) || defined(SSE_LOGSECTIONS_FOR_ALL)
  // because tracing IO is heavy the dedicated section must be enabled too
  if(TRACE_LOG_ENABLED(LOGSECTION_IOR) && stem_runmode==STEM_MODE_CPU)
  {
    bool dotrace=false;
    switch(ad_group) {
    case 0x80: if(TRACE_LOG_ENABLED(LOGSECTION_MMU))
      dotrace=true; break;
    case 0x82: if(TRACE_LOG_ENABLED(LOGSECTION_GLUE)) //shifter/GLU video
      dotrace=true; break;
    case 0x86: if(TRACE_LOG_ENABLED(LOGSECTION_DMA))
      dotrace=true; break;
    case 0x88: if(TRACE_LOG_ENABLED(LOGSECTION_SOUND))//psg
      dotrace=true; break;
    case 0x89: if(TRACE_LOG_ENABLED(LOGSECTION_SOUND))//dma
      dotrace=true; break;
    case 0x8a: if(TRACE_LOG_ENABLED(LOGSECTION_BLITTER))
      dotrace=true; break;
    case 0xFA: if(TRACE_LOG_ENABLED(LOGSECTION_MFP))
      dotrace=true; break;
    case 0xfc: if(TRACE_LOG_ENABLED(LOGSECTION_ACIA))
      dotrace=true; break;
    default:
      dotrace=true;
    }//sw
    if(dotrace)
    {
      TRACE(PRICV " PC %X IOR %X: %X\n",a_s_t,old_pc,addr,d16);
    }
  }
#endif

  return d16;
}


#ifdef DEBUG_BUILD

BYTE io_read_b(MEM_ADDRESS addr) {
  // this is only called by d2_peek()
  DEBUG_CHECK_READ_IO_B(addr);
  BYTE ior_byte;
  DU16 ior_word;
  if(addr&1)
  {
    ior_word.d16=io_read(addr&0xfffffe);
    ior_byte=ior_word.d8[LO];
  }
  else
  {
    ior_word.d16=io_read(addr&0xfffffe);
    ior_byte=ior_word.d8[HI];
  }
  return ior_byte;
}

#endif

#undef LOGSECTION
