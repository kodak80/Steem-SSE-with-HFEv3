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
FILE: iow.cpp
DESCRIPTION: The 68000 uses memory-mapped I/O
On the ST, addresses from $ff8000 onwards are mapped to peripherals.
They are decoded by the Glue (STE: MCU) and some other chips like the Blitter,
add-on cards...
Waitstates are possible with some devices.
For performance, we could have different IO functions for STF/STE (as usual
see trade-off with code bloat/management though, same consideration for one
unique R/W function).
This file handles writing to device registers.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <SSE.h> // for IDE
#include <computer.h>
#include <stjoy.h>
#include <draw.h>
#include <debug.h>
#include <loadsave.h>
#include <diskman.h>
#include <osd.h>
#if defined(SSE_VID_STVL1)
#include <interface_stvl.h>
#endif
#if defined(SSE_DEBUGGER_FRAME_REPORT)
#include <debug_framereport.h>
#endif
#include <iolist.h>


#define LOGSECTION LOGSECTION_IOW

// write word or byte (according to BUS_MASK) on memory-mapped device
void io_write(MEM_ADDRESS const addr, DU16 uio_src_w) {
  WORD const &io_src_w=uio_src_w.d16;
  BYTE &hibyte=uio_src_w.d8[HI];
  BYTE &lobyte=uio_src_w.d8[LO];
#ifdef DEBUG_BUILD
  DEBUG_CHECK_WRITE_IO_W(addr,io_src_w);
#endif
  // could be global, but it's tested only in iow/ior
  bool const lds=((BUS_MASK&BUS_MASK_LOBYTE)!=0); // LOWER DATA STROBE
  bool const uds=((BUS_MASK&BUS_MASK_HIBYTE)!=0); // UPPER DATA STROBE
  if(!SUPERFLAG)
  {
    bool DontCrash=false;
    switch(addr&0xfffffe) {
#ifdef SSE_MMU_MONSTER_ALT_RAM
    case 0xfffe00: // alt-RAM doesn't require supervisor mode
      DontCrash=true; // condition (mem_len==0xC00000&&uds&&lds) tested later
      break;
#endif    
#if defined(SSE_DEBUGGER)
    case 0xffc122: // breakpoint should be possible without supervisor mode
      if(lds)
        DontCrash=true;
      break;
#endif
    case 0xffc1f0: // ST trace should be possible without supervisor mode
    case 0xffc1f2: // it's a long
      if(OPTION_EMU_DETECT) // debugger or not
        DontCrash=true;
      break;
    }//sw
    if(!DontCrash)
      exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
  }//if

  a_s_t=A_S_T; // this is updated in case of waitstates

  // Main switch: address groups

#ifdef BIG_ENDIAN_PROCESSOR
#error TODO
#else
  BYTE ad_group=*(((BYTE*)&addr)+1); // first byte of 24bit address
  BYTE ad_byte=*(((BYTE*)&addr)); // last byte of address
#endif

#if defined(SSE_DEBUGGER) || defined(SSE_LOGSECTIONS_FOR_ALL)
  // because tracing IO is heavy the dedicated section must be enabled too
  if(TRACE_LOG_ENABLED(LOGSECTION_IOW))// && (stem_runmode==STEM_MODE_CPU))
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
      if(uds&&lds)
        TRACE("PC %X IOW %06X: %04X\n",old_pc,addr,io_src_w);
      else if(uds)
        TRACE("PC %X IOW %06X: %02X\n",old_pc,addr,hibyte);
      else if(lds)
        TRACE("PC %X IOW %06X: %02X\n",old_pc,addr+1,lobyte);
    }
  }
#endif

  switch(ad_group) {

  //////////////////////
  // RAM (MMU CONFIG) //
  //////////////////////

  case 0x80:

#undef LOGSECTION
#define LOGSECTION LOGSECTION_MMU

    if(lds && ad_byte==0x00) //Memory Configuration CONFIG
    {
      if(mem_len<=MEM_4MB||mem_len==MEM_12MB) // so not the 14MB hack
      {
        Mmu.Config=lobyte & 0xF;
        Mmu.bank_length[0]=Mmu.BankLength((Mmu.Config&b1100)>>2);
        Mmu.bank_length[1]= (ST_MODEL==STFM) ? Mmu.bank_length[0] // logic simplified on some MMU
                                             : Mmu.BankLength(Mmu.Config&b0011);
        TRACE_LOG("PC %X write %X to CONFIG (bank 0: %d bank 1: %d)\n",pc,lobyte,
                  Mmu.bank_length[0]/1024,Mmu.bank_length[1]/1024);
        // The MMU is "confused" when the value in its CONFIG register doesn't match
        // the hardware memory configuration (Steem: chosen in settings)
        Mmu.Confused=(SSEConfig.bank_length[0] && Mmu.bank_length[0]!=SSEConfig.bank_length[0]
                   || SSEConfig.bank_length[1] && Mmu.bank_length[1]!=SSEConfig.bank_length[1])
                      ? true : false;
        himem=(MEM_ADDRESS)mem_len;
#if defined(SSE_MMU_MONSTER_ALT_RAM)
        if(himem==MEM_12MB) //12MB monSTer
        {
          himem=MEM_4MB;
          Mmu.Confused=false;
        }
#endif
        Glue.Restore(); // routines depend on Mmu.Confused
        TRACE_LOG("CONFIG PC %X Byte %X RAM %dK Bank 0 %d Bank 1 %d confused %d\n",old_pc,lobyte,
                  mem_len>>10,SSEConfig.bank_length[0]>>10,SSEConfig.bank_length[1]/1024,Mmu.Confused);
      }
    }
    //forbidden range
    else if(ad_byte>((IS_STE||(ST_MODEL==STF)) ? 0x0e : 0x0c))
      exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
    break;


  //////////////////////////////
  // Video (MMU-GLUE-Shifter) //
  //////////////////////////////

  // On the STE, the MMU and the GLUE functions are taken over by the GSTMCU.
  // Its databus is 10bit because the MCU inherits from the GLUE (2bit, UDS)
  // and the MMU (8bit, LDS).
  case 0x82: 
    if((ad_byte>=0x10 && ad_byte<0x40) || ad_byte>=0x80)
      exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
    else if(ad_byte>=0x40 && ad_byte<0x80) // Shifter
    {
      switch(ad_byte) {
      case 0x60: // Glue too
        if(uds) // czietz: if you write a byte on $FF8261, the Glue isn't affected
          Glue.SetShiftMode(hibyte);
        break;
      case 0x64: // MCU too
        if(IS_STE && lds) 
        {
          Glue.hscroll=(lobyte!=0);
#if defined(SSE_VID_STVL_UPD)
          Stvl.noscroll=!Glue.hscroll; // it's NOSCROLL on Atari schematics
#endif
#if defined(SSE_DEBUGGER_FAKE_IO)
          if(TRACE_MASK1&TRACE_CONTROL_GLUREG)
          {
            TRACE_LOG("%d %d %d MCU NOSCROLL %d\n",TIMING_INFO,!Glue.hscroll);
          }
#endif
        }
        break;
      }//sw
      int shifter_reg=(ad_byte-0x40)>>1;
      // Shifter access -> wait states possible
      if(sys_cycles&(4*TICKS8-1))
      {
        BUS_WAIT_STATES((sys_cycles&(4*TICKS8-1))/TICKS8);
      }
      Blitter.BlitCycles=0;
      // The Shifter only knows CS as DS, not LDS and UDS, it works with words.
      // This and the fact that the MC68000 puts the same byte on both the low
      // order and high order of the data bus when writing a single byte 
      // explain some strange video effects (palette, hscroll, shift mode...).
      switch(shifter_reg) {
      case 16: // rez
        Shifter.ShiftMode=hibyte&3; // 2bit
#if defined(SSE_VID_STVL_UPD)
        Stvl.shift_mode=Shifter.ShiftMode;
#endif
        /*if(!(Shifter.ShiftMode&HIRES)&&(Glue.ShiftMode&HIRES))
        {
          // 160x400 colour, not properly rendered, wouldn't work on regular HW
        }
        else if((Shifter.ShiftMode&HIRES)&&!(Glue.ShiftMode&HIRES))
        {
          // 1280x200 mono, not properly rendered, wouldn't work on regular HW
        }*/
        if((Shifter.ShiftMode&HIRES)^(Glue.ShiftMode&HIRES))
        {
          TRACE("%s\n","BLIT ERROR"); // "secret" video mode
        }
        break;
      case 18: // scroll
        if(IS_STE) // no crash if STF, just ignore
        {
          BYTE former_hscroll=shifter_hscroll;
          shifter_hscroll=io_src_w&0xf;
#if defined(SSE_STATS)
          if(shifter_hscroll)
            Stats.nHscroll++;
#endif
          if(!OPTION_C3)
          {
            /*  should new HSCROLL apply on current line
                It's a bit complicated (hacky) because of the "real-time but 
                not quite"rendering.
                Cases to check: Krig, We Were dist, D4/Tekila...
            */
            //ASSERT(a_s_t==ABSOLUTE_SYS_TIME);
            SHORT cycles_in=(SHORT)(a_s_t-TimeOfHSyncOff);
#if defined(SSE_SHIFTER_421)
            Shifter.Render(cycles_in,TShifter::DISPATCHER_HSCROLL); // Krig
            if(1) // VibeCoding
#else
            if(cycles_in<=Glue.CurrentScanline.StartCycle+24*TICKS8) 
#endif
            {
#if defined(SSE_SHIFTER_421)
              if(cycles_in<=Glue.CurrentScanline.StartCycle+24*TICKS8) 
#endif
                Glue.AdaptScanlineValues(cycles_in); // ST Magazin
              if(left_border>=SideBorderSize)
              { // Don't do this if left border removed!
                left_border=LeftBorderSize;
                if(HSCROLL)
                  left_border+=16;
                if(Glue.hscroll)
                  left_border-=16;
              }
              // update shifter_tick8 for new HSCROLL
              if(shifter_tick8)
              {
                shifter_tick8-=former_hscroll>>screen_res;
                shifter_tick8+=HSCROLL>>screen_res;
              }
            }
          }
#if defined(SSE_DEBUGGER_FRAME_REPORT)
          if(FRAME_REPORT_MASK1 & FRAME_REPORT_MASK_HSCROLL)
            FrameEvents.Add(scan_y,LINECYCLES/TICKS8,(lds)?'H':'h',shifter_hscroll);
#endif
#if defined(SSE_VID_STVL_UPD)
          Stvl.hscroll=shifter_hscroll;
          Stvl.hscroll_complement=16-shifter_hscroll;
#endif
        }
        break;
      default:
        if(shifter_reg<PAL_SIZE) // palette - if not, do nothing (no crash)
        {
#if defined(SSE_DEBUGGER_FRAME_REPORT)
          if(FRAME_REPORT_MASK1 & FRAME_REPORT_MASK_PAL)
            FrameEvents.Add(scan_y,LINECYCLES/TICKS8,'P',(shifter_reg<<16)|io_src_w);
#endif
          Shifter.SetPal(shifter_reg,io_src_w);
        }
        break;
       }//sw
    }
    else if(ad_byte==0x0a && uds) // GLUE synchronization mode
      Glue.SetSyncMode(hibyte);
    else if(lds) // MMU video registers
    {
      switch(ad_byte) {
      case 0x00:  //high byte of screen memory address
#if defined(SSE_DEBUGGER_FRAME_REPORT)
        if(FRAME_REPORT_MASK1 & FRAME_REPORT_MASK_VIDEOBASE)
          FrameEvents.Add(scan_y,LINECYCLES/TICKS8,'V',lobyte);
#endif
#if defined(SSE_MMU_MONSTER_ALT_RAM)
        if(mem_len<MEM_14MB)  // no limit only for 14MB hack
          lobyte&=b00111111;
#else
        if(mem_len<=MEM_4MB)
          lobyte&=b00111111;
#endif
        Mmu.uVBase.d8[B2]=lobyte;
        if(!extended_monitor)
          Mmu.uVBase.d8[B0]=0; // for STE
#if defined(SSE_VID_STVL_UPD)
        Stvl.vbase.d32=vbase;
#endif
#if defined(SSE_DEBUGGER_FAKE_IO)
        if(TRACE_MASK1&TRACE_CONTROL_GLUREG)
        {
          TRACE_LOG("%d %d %d MMU VBASE $%X\n",TIMING_INFO,vbase);
        }
#endif
        break;
      case 0x02:  //mid byte of screen memory address
#if defined(SSE_DEBUGGER_FRAME_REPORT)
        if(FRAME_REPORT_MASK1 & FRAME_REPORT_MASK_VIDEOBASE)
          FrameEvents.Add(scan_y,LINECYCLES/TICKS8,'M',lobyte);
#endif
        if(!extended_monitor)
        {
          Mmu.uVBase.d8[B1]=lobyte;
          Mmu.uVBase.d8[B0]=0; // for STE
        }
#if defined(SSE_VID_STVL_UPD)
        Stvl.vbase.d32=vbase;
#endif
#if defined(SSE_DEBUGGER_FAKE_IO)
        if(TRACE_MASK1&TRACE_CONTROL_GLUREG)
        {
          TRACE_LOG("%d %d %d MMU VBASE $%X\n",TIMING_INFO,vbase);
        }
#endif
        break;
      case 0x06:  //mid byte of video counter
#if defined(SSE_STATS)
        if(lobyte)
        {
          Stats.nVbaseMid++; 
          Stats.nVbaseMid1++; 
        }
        //no break
#endif
      case 0x04:  //high byte of video counter          
#if defined(SSE_STATS)
        Stats.nVbaseHi++;
#endif
        //no break
      case 0x08:  //low byte of video counter
#if defined(SSE_DEBUGGER_FRAME_REPORT)
        if(FRAME_REPORT_MASK1 & FRAME_REPORT_MASK_VCOUNT)
          FrameEvents.Add(scan_y,LINECYCLES/TICKS8,'C',(((addr+1)&0xF)<<8)|lobyte);
#endif
        if(!OPTION_C3)
        {
          Mmu.WriteVideoCounter(ad_byte,lobyte);
#if defined(SSE_VID_STVL_UPD)
          Stvl.vcount.d32=Mmu.VideoCounter;
#endif
        }
#if defined(SSE_DEBUGGER_FAKE_IO)
        if(TRACE_MASK1&TRACE_CONTROL_GLUREG)
        {
          TRACE_LOG("%d %d %d MCU VCOUNT $%X\n",TIMING_INFO,Mmu.VideoCounter);
        }
#endif
        break;
      case 0x0c:  //low byte of screen memory address
#if defined(SSE_DEBUGGER_FRAME_REPORT)
        if(FRAME_REPORT_MASK1 & FRAME_REPORT_MASK_VIDEOBASE)
          FrameEvents.Add(scan_y,LINECYCLES/TICKS8,'v',lobyte);
#endif
        if(IS_STE)
        {
#if defined(SSE_STATS)
          if(lobyte)
            Stats.nVbaseLo++;
#endif
          Mmu.uVBase.d8[B0]=lobyte&0xFE;
#if defined(SSE_VID_STVL_UPD)
          Stvl.vbase.d32=vbase;
#endif
#if defined(SSE_DEBUGGER_FAKE_IO)
          if(TRACE_MASK1&TRACE_CONTROL_GLUREG)
          {
            TRACE_LOG("%d %d %d MMU VBASE $%X\n",TIMING_INFO,vbase);
          }
#endif
        }
        break;
      case 0x0e: // LINEWID
#if defined(SSE_DEBUGGER_FRAME_REPORT)
        if(FRAME_REPORT_MASK1 & FRAME_REPORT_MASK_HSCROLL)
          FrameEvents.Add(scan_y,LINECYCLES/TICKS8,'L',lobyte);
#endif
        if(IS_STE)
        {
          if(!OPTION_C3)
            Shifter.Render(LINECYCLES,TShifter::DISPATCHER_LINEWIDTH); // eg Beat Demo //TODO
          Mmu.linewid=lobyte;
          if(LINECYCLES<Glue.CurrentScanline.EndCycle+MMU_PREFETCH_LATENCY)
            LINEWID=Mmu.linewid;
#if defined(SSE_VID_STVL_UPD)
          Stvl.linewid=lobyte;
#endif
#if defined(SSE_DEBUGGER_FAKE_IO)
          if(TRACE_MASK1&TRACE_CONTROL_GLUREG)
          {
            TRACE_LOG("%d %d %d MCU LINEWID %d\n",TIMING_INFO,Mmu.linewid);
          }
#endif
        }
        else if(ST_MODEL!=STF) // crash on STFM, Mega ST
          exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
        break;
      }//sw
    }//if lds
    break;

#undef LOGSECTION
#define LOGSECTION LOGSECTION_DMA

    
  ////////////////////////
  // Disk (MMU-DMA-FDC) //
  ////////////////////////

  case 0x86:
  {
    // test for bus error
    if(ad_byte>((IS_STE||(ST_MODEL==STF))?0x0e:0x0c) || ad_byte<0x04
      || ad_byte<0x08 && (BUS_MASK&BUS_MASK_WORD)!=BUS_MASK_WORD)
    {
      exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
    }

#if defined(SSE_BAD_DMA)
    bool TransferedDma=false;
    static COUNTER_VAR TimeAtDmaRegWrite=0;
#endif

    switch(ad_byte) {
    case 0x04:
      if(Dma.mcr&TDma::CR_COUNT_OR_REGS)
      {
        Dma.ByteCount=0;
        Dma.Counter=lobyte; // high byte isn't implemented
        if(Dma.Counter)
        {
          Dma.sr|=TDma::SR_COUNT;
          // writing sector count triggers DMA
#if USE_PASTI
          if(!DiskEmu.PastiOperation()) // but not when it's pasti
#endif
          if((Dma.mcr&TDma::CR_WRITE) && !Dma.Fifo_idx[Dma.BufferInUse]
            && (IRD&0xF000)==0x3000) // MOVE.W, hacky
          {
            BYTE manager=
#if defined(SSE_ACSI_LASER)
              Laser.busy ? MNGR_ACSI : 
#endif
              DiskEmu.LastManager;
            Dma.RequestTransfer(manager,Dma.BufferInUse);
            Dma.RequestTransfer(manager,!Dma.BufferInUse); // both FIFO are filled (32 bytes)
#if defined(SSE_BAD_DMA)
            TransferedDma=true;
#endif
          }
        }
        else
          Dma.sr&=~TDma::SR_COUNT; //status register bit for 0 count 
//        Dma.ByteCount=0;
        break;
      }
      // HD access
      if(Dma.mcr&TDma::CR_HDC_OR_FDC)
      {
#if defined(SSE_ACSI)
        if(ACSI_EMU_ON || OPTION_LASER)
        {
          int device=acsi_dev;
          if(!(Dma.mcr&TDma::CR_A0)&&(io_src_w>>5)<MAX_ACSI_DEVICES)
            device=(io_src_w>>5); // assume new command
          TRACE_LOG("%06X ACSI write %X to device %d line %X\n",old_pc,lobyte,device,(Dma.mcr&TDma::CR_A0));
          if(ACSI_EMU_ON||device==ACSI_ID_LASER)
            AcsiHdc[device].IOWrite((Dma.mcr&TDma::CR_A0),lobyte);
        }
#endif
        break;
      }
      // Write FDC register
      if(Dma.mcr&TDma::CR_DRQ_FDC_OR_HDC)
        Fdc.IOWrite((Dma.mcr&(TDma::CR_A1|TDma::CR_A0))>>1,lobyte);
      break;
    case 0x06:  //DMA mode
      // detect toggling of bit 8 (DMA reset)
      if((Dma.mcr&TDma::CR_WRITE)^(io_src_w&TDma::CR_WRITE))
      {
        TRACE_SECTION(LOGSECTION_DMA,"DMA Reset\n");
        Dma.Counter=Dma.ByteCount=0;
        Dma.sr=TDma::SR_NO_ERROR;
        Dma.Request=false;
        Dma.BufferInUse=Dma.Fifo_idx[0]=Dma.Fifo_idx[1]=0;
      }
      Dma.mcr=(io_src_w&0x1FF);
      // writing sector count triggers DMA
      if((Dma.mcr&Dma.CR_WRITE) && !Dma.Fifo_idx[Dma.BufferInUse] && (IRD&0xF000)==0x2000) // MOVE.L, hacky
      {
        BYTE manager=
#if defined(SSE_ACSI_LASER)
          Laser.busy ? MNGR_ACSI : 
#endif
          DiskEmu.LastManager;
#ifndef SSE_NO_OSD
#ifdef WIN32
        if(!OsdControl.bPrinting)
        {
          UPDATE_STATUS_BAR_PART(SB_PART_ICONS);
          SetTimer(StemWin,STATUSBAR_TIMER_ID,3000,NULL); //3s
        }
        OsdControl.bPrinting=true;
#elif defined(SSE_OSD_DEBUGINFO)
        if(Laser.busy)
        {
          TRACE_OSD_DBG("PRT");
        }
#endif
#endif
        Dma.RequestTransfer(manager,Dma.BufferInUse);
        Dma.RequestTransfer(manager,!Dma.BufferInUse); // both FIFO are filled (32 bytes)
#if defined(SSE_BAD_DMA)
        TransferedDma=true;
#endif
      }
      break;
    case 0x08:  // DMA Base High - (not limited to 4MB)
      Mmu.uDmaCounter.d16[HI]=lobyte;
      TRACE_LOG("DMA base address: %X\n",dma_address);
      break;
    case 0x0a:  // DMA Base Mid
/*
DMA pointer has to be initialized in order low, mid, high, why
Short answer. Writing to a lower byte might increase (not clear) the upper one.
This will happen when bit 7 (uppermost bit) of the written byte is changed from one to zero.
The reason is how exactly the counters are implemented on Mmu. They use a ripple carry.
This saves quite some logic at the cost of not being fully synchronous.
The upper bit of the lowermost byte, inverted, clocks directly the middle byte of
the counter. Any change from high to low on that bit, anytime, would clock the counter
on the middle byte. Similarly, the inverted uppermost bit of the middle byte clocks the
high byte of the counter.

Actually, the ripple carry is not only across bytes, but across nibbles as well.
But between nibbles doesn't have that collateral effect because both nibbles are written
at the same time, and the written value overrides the ripple carry.

In case you are wondering, the video counter uses the same ripple mechanism. But there
is no such effect because they are read only on the ST.
(ijor)
*/
      if(IS_STF&&(dma_address&0x008000)&&!(io_src_w&0x80) // 1 to 0
        &&(!pasti_active && FloppyDrive[DRIVE].ImageType.Extension!=EXT_STX))
      {
        DU16 tmp(Mmu.uDmaCounter.d8[B2]+1);
        io_write(0xff8608,tmp);
      }
      Mmu.uDmaCounter.d8[B1]=lobyte;
      break;
    case 0x0c:  // DMA Base Low
      if(IS_STF&&(dma_address&0x000080)&&!(io_src_w&0x80) // 1 to 0
        &&!pasti_active && FloppyDrive[DRIVE].ImageType.Extension!=EXT_STX)
      { 
        DU16 tmp(Mmu.uDmaCounter.d8[B1]+1);
        io_write(0xff860a,tmp);
      }
      Mmu.uDmaCounter.d8[B0]=(io_src_w&0xfe);
      break;
    case 0x0e: // frequency/density control
      // TOS will check for HD even if DIP switch not set
      TRACE_SECTION(LOGSECTION_FDC,"PC %X IOW fd density %X\n",old_pc,lobyte);
#if defined(SSE_MEGASTE)
      MegaSte.FdHd=(IS_MEGASTE) ? (lobyte&0x03) : 0;
#endif
#if defined(SSE_DISK_CAPS)
#if defined(SSE_MEGASTE)
      if(MegaSte.FdHd&BIT_0)
        Caps.fdc.clockfrq=STE_CLOCK8*2;
      else
#endif
        Caps.fdc.clockfrq=(IS_STE) ? (STE_CLOCK8) : (CpuNormalHz/TICKS8);
#endif
      break; //else ignore
    }//sw

#if defined(SSE_BAD_DMA)
    //if(IS_STE && !TransferedDma)
    if(SSEOptions.BadDma && !TransferedDma)
    {
      if(a_s_t-TimeAtDmaRegWrite==4*TICKS8)
        Dma.IncAddress(); // HW bug
      TimeAtDmaRegWrite=a_s_t;
    }
#endif

#undef LOGSECTION
#define LOGSECTION LOGSECTION_FDC

#if USE_PASTI 
/*  Pasti handles all DMA writes, still we want to update our variables
    and go through TRACE.*/
    if(hPasti&&(pasti_active||FloppyDrive[DRIVE].ImageType.Extension==EXT_STX))
    {
      struct pastiIOINFO pioi;
      pioi.stPC=pc; //debug info only
      pioi.cycles=a_s_t/TICKS8;
#if defined(SSE_DISK_GHOST)
      if(OPTION_GHOST_DISK && Fdc.Lines.CommandWasIntercepted
        && ad_byte==0x04&&!(Dma.mcr&(BIT_1+BIT_2+BIT_3))) // FDC commands
      {
        TRACE_LOG("Pasti doesn't get command %X\n",Fdc.cr);
      }
      else
#endif
      {
        // Pasti expects byte writes to odd addresses, which isn't the way in Steem SSE
        if(ad_byte<0x08)
        {
          pioi.addr=addr;
          pioi.data=io_src_w;
          pasti->Io(PASTI_IOWRITE,&pioi);
        }
        else
        {
          pioi.addr=addr;
          if(BUS_MASK&BUS_MASK_HIBYTE)
          {
            pioi.data=hibyte;
            pasti->Io(PASTI_IOWRITE,&pioi);
          }
          if(BUS_MASK&BUS_MASK_LOBYTE)
          {
            pioi.addr++;
            pioi.data=lobyte;
            pasti->Io(PASTI_IOWRITE,&pioi);
          }
        }
        pasti_handle_return(&pioi);
      }
    }
#endif
    break;
  }


  ////////////////
  // YM2149 PSG //
  ////////////////

  case 0x88:

    BUS_WAIT_STATES(1); // GLUE delays DTACK by one cycle (often rounded up)

    if(!(ad_byte&BIT_1)) //read data / register select
    {
      psg_reg_select=hibyte;
      psg_reg_data=(psg_reg_select<16) ? (psg_reg[psg_reg_select]) : (0xFF);
    }
    else  //write data
    {
      if(psg_reg_select>PSGR_PORT_B)
        break; // PSG has only 16 registers
      psg_reg_data=hibyte;
      BYTE old_val=psg_reg[psg_reg_select];
      if(psg_reg_select<PSGR_PORT_A)
      {
        static BYTE ValidBits[16]={0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0x1F,0xFF,
                                   0x1F,0x1F,0x1F,0xFF,0xFF,0x0F,0xFF,0xFF}; // see YM2149 doc
        hibyte&=ValidBits[psg_reg_select];
        psg_set_reg(old_val,hibyte);
      }
#if defined(SSE_DEBUGGER_MUTE_SOUNDCHANNELS)
      if(psg_reg_select<PSGR_AMPLITUDE_A||psg_reg_select>PSGR_AMPLITUDE_C
        ||(!((1<<9)&d2_dpeek(FAKE_IO_START+20)))) // negate vol change (samples)
#endif
      psg_reg[psg_reg_select]=hibyte;
      // compute tone & env periods
      if(psg_reg_select>=PSGR_TONE_PERIOD_A_LOW&&psg_reg_select<=PSGR_TONE_PERIOD_C_HIGH)
      {
        BYTE abc=psg_reg_select>>1;
        BYTE abcx2=abc<<1;
#if defined(BIG_ENDIAN_PROCESSOR)
        Psg.tone_period[abc]=(((int)psg_reg[abcx2+1])<<8)+psg_reg[abcx2];
#else
        Psg.tone_period[abc]=*(WORD*)&psg_reg[abcx2];
#endif
      }
      else if(psg_reg_select==PSGR_ENVELOPE_PERIOD_LOW||psg_reg_select==PSGR_ENVELOPE_PERIOD_HIGH)
#if defined(BIG_ENDIAN_PROCESSOR)
        Psg.env_period=psg_reg[PSGR_ENVELOPE_PERIOD_LOW]|(psg_reg[PSGR_ENVELOPE_PERIOD_HIGH]<<8);
#else
        Psg.env_period=*(WORD*)&psg_reg[PSGR_ENVELOPE_PERIOD_LOW];
#endif
      else if(psg_reg_select==PSGR_PORT_A  && (psg_reg[PSGR_MIXER]&BIT_6))
      {
        BYTE myhibyte=hibyte;
#if defined(SSE_DRIVE_FREEBOOT)
        if(DiskMan.nFloppyDrives>1 && FloppyDrive[DRIVE_B].bFreeboot)
        { // freeboot on B: -> we swap drives
          myhibyte&=~(BIT_1|BIT_2);
          myhibyte|=(hibyte>>1)&BIT_1;
          myhibyte|=(hibyte<<1)&BIT_2;
        }
        else if(FloppyDrive[DRIVE_A].bFreeboot)
        { // freeboot on A: -> we swap sides
          if(!(myhibyte&BIT_1))
            myhibyte^=BIT_0;
        }
#endif
#if USE_PASTI
        if(hPasti)
          pasti->WritePorta(myhibyte,a_s_t/TICKS8);
#endif
#if defined(SSE_DISK_CAPS)
        if(Caps.Active)
          Caps.WritePsgA(myhibyte);
#endif
        SerialPort.SetDTR((hibyte & BIT_4)!=0);
        SerialPort.SetRTS((hibyte & BIT_3)!=0);
#if defined(SSE_DONGLE_JEANNEDARC)
        if(DONGLE_ID==TDongle::JEANNEDARC)
        {
          BYTE Old=Dongle.Value & 0xFF;
          BYTE New=hibyte & (BIT_4|BIT_3);
          Dongle.Value=New;
          mfp_gpip_set_bit(MFP_GPIP_DCD_BIT,!(New && New<Old));
        }
#endif
        Psg.SelectedSide=(floppy_current_side()==1);
        if((old_val&(BIT_1|BIT_2))!=(hibyte&(BIT_1|BIT_2)))
        {
          BYTE cur_drv=Psg.CurrentDrive();
          Psg.SelectedDrive=(cur_drv==DRIVE_B);
          DiskEmu.Update();
          bool motor_on=((Fdc.str&FDC_STR_MO)!=0);
          FloppyDrive[DRIVE_A].Motor(motor_on && cur_drv==DRIVE_A);
          FloppyDrive[DRIVE_B].Motor(motor_on && cur_drv==DRIVE_B);
#if defined(SSE_DEBUGGER_FAKE_IO)
          if(TRACE_MASK3 & TRACE_CONTROL_FDCPSG)
          {
            if(Psg.CurrentDrive()!=TYM2149::NO_VALID_DRIVE)
              TRACE_LOG("PC %X PSG %X -> %c%d:\n",old_pc,
                hibyte,'A'+Psg.SelectedDrive,Psg.SelectedSide);
            else
              TRACE_LOG("PC %X PSG %X -> X\n",old_pc,hibyte);
          }
#endif
        }//if
#if defined(SSE_DRIVE_FREEBOOT)
        Psg.CheckFreeboot();
#endif
      }
      else if(psg_reg_select==PSGR_PORT_B && (psg_reg[PSGR_MIXER]&BIT_7))
      {
#if defined(SSE_SOUND_CARTRIDGE)
/*  Wings of Death, Lethal Xcess can use the Pro Sound Centronics adapter
    to play 8bit samples on the STF. */
        if(DONGLE_ID==TDongle::PROSOUND)
          mv16_fetch(hibyte<<3);
        else 
#endif
        {
          if(ParallelPort.IsOpen()) 
          {
            if(!ParallelPort.OutputByte(hibyte))
            {
              BREAKPOINT(printer char lost);
            }
          }
          UpdateCentronicsBusyBit();
        }
      }
      else if(psg_reg_select==PSGR_MIXER) // mixer register also configures I/O of ports
      {
        // decode mixer on writes
        // bit=1=..._ENABLEQ() in TYM2149::psg_write_buffer()
        Psg.tone_enabled[0]=!!(psg_reg[psg_reg_select]&BIT_0);
        Psg.tone_enabled[1]=!!(psg_reg[psg_reg_select]&BIT_1);
        Psg.tone_enabled[2]=!!(psg_reg[psg_reg_select]&BIT_2);
        Psg.noise_enabled[0]=!!(psg_reg[psg_reg_select]&BIT_3);
        Psg.noise_enabled[1]=!!(psg_reg[psg_reg_select]&BIT_4);
        Psg.noise_enabled[2]=!!(psg_reg[psg_reg_select]&BIT_5);
        UpdateCentronicsBusyBit();
      }
    }
    break;


  ///////////////////////
  // STE Digital Sound //
  ///////////////////////

  case 0x89:
#undef LOGSECTION
#define LOGSECTION LOGSECTION_SOUND
    if(IS_STF || ad_byte>0x3e)
      exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
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
        Shifter.SoundSetMode(lobyte);
        break;
      case 0x22: // Set MicroWire_Data
      {
#if defined(SSE_STATS)
        Stats.nMicrowire++;
        if(LITTLE_PC>rom_addr)
          Stats.nMicrowireT++;
#endif
        if(abs_quick(a_s_t-Microwire.StartTime)<MW_LATENCY_CYCLES)
        { // XMas2004-pdx
          TRACE_LOG("%X Microwire write %X at %X denied, %d cycles after write\n",
            old_pc,io_src_w,addr,(int)(a_s_t-Microwire.StartTime));
          break;
        }
        Microwire.Data=io_src_w;
        Microwire.StartTime=a_s_t;
        int dat=(Microwire.Data&Microwire.Mask);
        for(int b=15;b>=10;b--) // ID '10' can be on bits 15-14 to 10-9
        {
          if((Microwire.Mask&(3<<(b-1)))==(3<<(b-1)))
          {
            if( (dat & (1<<b)) && (dat & (1<<(b-1)))==0) // '10'
            {
              int dat_b=b-2;
              for(;dat_b>=8;dat_b--) // must be on bit8 or higher (need 9 bits)
              { // Find function address (start of data)
                if(Microwire.Mask & (1<<dat_b)) 
                  break;
              }
              dat>>=dat_b-8; // Move 9 highest bits of data to the start
              int nController=(dat>>6) & b0111;
              switch(nController) {
              case b0011: // Master Volume
              case b0101: // Left Volume
              case b0100: // Right Volume
                if(nController==b0011)
                {
                  // 20 is practically silent!
                  Microwire.volume=dat&b00111111;
                  if(Microwire.volume>47) 
                    Microwire.volume=0; // 47 101111
                  else if(Microwire.volume>40) 
                    Microwire.volume=40;
                }
                else 
                {
                  BYTE new_val=dat&b00011111;
                  if(new_val>23) 
                    new_val=0;
                  else if(new_val>20) 
                    new_val=20;
                  if(nController==b0101) 
                    Microwire.volume_l=new_val;
                  else if(nController==b0100) 
                    Microwire.volume_r=new_val;
                }
                if(!SSEOptions.LmcSlowFade)
                  Microwire.top_val_l=Microwire.top_val_r=128;
                else
                {
                  long double lv,rv,mv;
                  lv=Microwire.volume_l;
                  lv=lv*lv*lv*lv;
                  lv/=(20.0*20.0*20.0*20.0);
                  rv=Microwire.volume_r;
                  rv=rv*rv*rv*rv;
                  rv/=(20.0*20.0*20.0*20.0);
                  mv=Microwire.volume;
                  mv=mv*mv*mv*mv*mv*mv*mv*mv;
                  mv/=(40.0*40.0*40.0*40.0*40.0*40.0*40.0*40.0);
                  // lv rv and mv are numbers between 0 and 1
                  Microwire.top_val_l=(BYTE)(128.0*lv*mv);
                  Microwire.top_val_r=(BYTE)(128.0*rv*mv);
                }
                TRACE_LOG("%X Microwire volume:  master %d L %d R %d\n",
                  old_pc,Microwire.volume,Microwire.volume_l,Microwire.volume_r);
#if defined(SSE_GEM_CONTROL_PANEL) && defined(SSE_GUI_421_REORDER)
                if(OptionBox.IsVisible() && OptionBox.Page==PAGE_GEM_CP)
                {
                  HWND hwnd=GetDlgItem(OptionBox.Handle,IDC_MWVOLUME);
                  SendMessage(hwnd,TBM_SETPOS,1,Microwire.volume);
                  PostMessage(OptionBox.Handle,WM_HSCROLL,0,(LPARAM)hwnd); // force update
                  hwnd=GetDlgItem(OptionBox.Handle,IDC_MWBALANCE);
                  SendMessage(hwnd,TBM_SETPOS,1,Microwire.volume_r-Microwire.volume_l);
                }
#endif
                break;
              case b0010: // Treble
                TRACE_LOG("%X DMA snd Treble $%X\n",old_pc,dat);
                dat&=0xF;
                if(dat>0xC)
                  dat=0x6; // ?
                Microwire.treble=(BYTE)dat;
#if defined(SSE_GEM_CONTROL_PANEL) // move control on Control Panel
                if(OptionBox.IsVisible() && OptionBox.Page==PAGE_GEM_CP)
                {
                  HWND hwnd=GetDlgItem(OptionBox.Handle,IDC_TREBLE);
                  SendMessage(hwnd,TBM_SETPOS,1,Microwire.treble);
                  PostMessage(OptionBox.Handle,WM_HSCROLL,0,(LPARAM)hwnd); // force update
                }
#endif
                break;
              case b0001: // Bass
                TRACE_LOG("%X DMA snd Bass $%X\n",old_pc,dat);
                dat&=0xF;
                if(dat>0xC)
                  dat=0x6; // ?
                Microwire.bass=(BYTE)dat;
#if defined(SSE_GEM_CONTROL_PANEL)
                if(OptionBox.IsVisible() && OptionBox.Page==PAGE_GEM_CP)
                {
                  HWND hwnd=GetDlgItem(OptionBox.Handle,IDC_BASS);
                  SendMessage(hwnd,TBM_SETPOS,1,Microwire.bass);
                  PostMessage(OptionBox.Handle,WM_HSCROLL,0,(LPARAM)hwnd); // force update
                }
#endif
                break;
              case b0000: // Mixer
              {
                BYTE old=Microwire.mixer;
                Microwire.mixer=dat&b00000011; // 1=PSG too, anything else only DMA
                if(Microwire.mixer!=old)
                {
                  TRACE_LOG("%X STE SND mixer %X->%X\n",old_pc,old,Microwire.mixer);
                  if(OPTION_SAMPLED_YM)
                    Psg.LoadFixedVolTable(true);
                }
              }
                break;
              }//sw
            }//if
            break;
          }//if
        }//nxt
        break;
      }
      case 0x24:  // Set MicroWire_Mask
        if(abs_quick(a_s_t-Microwire.StartTime)<MW_LATENCY_CYCLES)
        {
          TRACE_LOG("Microwire write %X at %X denied, %d cycles after write\n",
            io_src_w,addr,(int)(a_s_t-Microwire.StartTime));
          break;
        }
        Microwire.Mask=io_src_w;
        break;
      default:
        TRACE_LOG("STE SND %X %X\n",addr,io_src_w);
      }//sw
    }//if
    else if(lds) // MCU sound registers
    {
      switch(ad_byte) {
      case 0x00:  //DMA control register
        Mmu.SetSoundControl(lobyte);
        break;
      case 0x02:   //HiByte of frame start address
      case 0x04:   //MidByte of frame start address
      case 0x06:   //LoByte of frame start address
        switch(ad_byte) {
        case 0x2:
          Mmu.uNextSoundFrameStart.d8[B2]=lobyte&0x3F;
          break;
        case 0x4:
          Mmu.uNextSoundFrameStart.d8[B1]=lobyte;
          break;
        case 0x6:
          Mmu.uNextSoundFrameStart.d8[B0]=lobyte&0xFE;
          break;
        }//sw
        //TRACE_LOG("DMA frame start %X\n",NextSoundFrameStart);
        if((Mmu.SoundControl & TMmu::SOUNDPLAY)==0) 
          SteSndFetchAd=SteSndFrameStart=NextSteSndFrameStart;
        break;
      case 0x0e:   //HiByte of frame end address
      case 0x10:   //MidByte of frame end address
      case 0x12:   //LoByte of frame end address
        switch(ad_byte) {
        case 0x0e:
          Mmu.uNextSoundFrameEnd.d8[B2]=lobyte&0x3F;
          break;
        case 0x10:
          Mmu.uNextSoundFrameEnd.d8[B1]=lobyte;
          break;
        case 0x12:
          Mmu.uNextSoundFrameEnd.d8[B0]=lobyte&0xFE;
          break;
        }//sw
        //TRACE_LOG("PC %X DMA frame end %X\n",old_pc,NextSoundFrameEnd);
        if((Mmu.SoundControl & TMmu::SOUNDPLAY)==0) 
          SteSndFrameEnd=NextSteSndFrameEnd;
        break;
      }//sw
    }//lds
    break;


  /////////////
  // BLiTTER //
  /////////////

#undef LOGSECTION
#define LOGSECTION LOGSECTION_BLITTER

  case 0x8a: // at some point we moved blitter io to iow.cpp and ior.cpp
             // tempting to move it back to blitter.cpp, but we need uds, lds, addr,
             // io_src_w, lobyte, hibyte...
             // could hurt performance a bit?
  {

#ifdef DISABLE_BLITTER
    
    exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);

#else

    // Mega ST and STE have a blitter, we don't emulate STF + added blitter by choice
    // if there's no blitter to do the decoding, access is ignored and times out (bus error)
    if(!SSEConfig.Blitter)
      exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
    MEM_ADDRESS Offset=ad_byte>>1;
#if defined(SSE_DEBUGGER_FRAME_REPORT)
    if(FRAME_REPORT_MASK2 & FRAME_REPORT_MASK_BLITTER)
      FrameEvents.Add(scan_y,LINECYCLES/TICKS8,'B',((Offset<<16)|io_src_w));
#endif
    WORD io_fffe=io_src_w&0xFFFE; // code size optimisation
    if(Offset<0x1d&&(BUS_MASK&BUS_MASK_WORD)!=BUS_MASK_WORD)
    {} //no write but no bus error
    else if(Offset<0x10)
      /*  FF 8A00   |oooooooo||oooooooo|     HALFTONE RAM
          FF 8A02   |oooooooo||oooooooo|
          FF 8A04   |oooooooo||oooooooo|
                    :        ::        :
          FF 8A1E   |oooooooo||oooooooo| */
      Blitter.HalfToneRAM[Offset]=io_src_w;
    else switch(Offset) {
      
      // word access only
    case 0x10:
      //  FF 8A20   |oooooooo||ooooooo-|     SOURCE X INCREMENT
      Blitter.SrcXInc=io_fffe;
      break;
    case 0x11:
      //  FF 8A22   |oooooooo||ooooooo-|     SOURCE Y INCREMENT
      Blitter.SrcYInc=io_src_w&0xFFFE;
      break;
    case 0x12:
      //  FF 8A24   |--------||oooooooo|     SOURCE ADDRESS
      Blitter.SrcAdr.d8[B2]=lobyte;
      break;
    case 0x13:
      //  FF 8A26   |oooooooo||ooooooo-|
      Blitter.SrcAdr.d16[LO]=io_fffe;
      break;
    case 0x14:
      //  FF 8A28   |oooooooo||oooooooo|     ENDMASK 1
      Blitter.EndMask[0]=io_src_w;
      break;
    case 0x15:
      //  FF 8A2A   |oooooooo||oooooooo|     ENDMASK 2
      Blitter.EndMask[1]=io_src_w;
      break;
    case 0x16:
      //  FF 8A2C   |oooooooo||oooooooo|     ENDMASK 3
      Blitter.EndMask[2]=io_src_w;
      break;
    case 0x17:
      //  FF 8A2E   |oooooooo||ooooooo-|     DESTINATION X INCREMENT
      Blitter.DestXInc=io_fffe;
      break;
    case 0x18:
      //  FF 8A30   |oooooooo||ooooooo-|     DESTINATION Y INCREMENT
      Blitter.DestYInc=io_fffe;
      break;
    case 0x19:
      //  FF 8A32   |--------||oooooooo|     DESTINATION ADDRESS
      Blitter.DestAdr.d8[B2]=lobyte;
      break;
    case 0x1a:
      //  FF 8A34   |oooooooo||ooooooo-|
      Blitter.DestAdr.d16[LO]=io_fffe;
      break;
    case 0x1b:
      //  FF 8A36   |oooooooo||oooooooo|     X COUNT
      Blitter.XCount=io_src_w;
      Blitter.XCounter=Blitter.XCount;
      if(Blitter.XCounter==0)
        Blitter.XCounter=0xFFFF+1; //65536;
      break;
    case 0x1c:
      //  FF 8A38   |oooooooo||oooooooo|     Y COUNT
      Blitter.YCount=io_src_w;
      Blitter.YCounter=Blitter.YCount;
      if(Blitter.YCounter==0)
        Blitter.YCounter=0xFFFF+1; //65536;
      break;

      // byte access OK
    case 0x1d:
      //  FF 8A3A   |------oo|               HOP
      //  FF 8A3B   |----oooo|               OP
      if(uds)
        Blitter.Hop=hibyte&TBlitter::MSK_HOP; // 2bit
      if(lds)
      {
        Blitter.Op=lobyte&TBlitter::MSK_OP; // 4bit
        Blitter.NeedDestRead=(Blitter.Op&&(Blitter.Op!=3)&&(Blitter.Op!=12)&&(Blitter.Op!=15));
      }
      break;
    case 0x1e:
      //  FF 8A3C   |ooo-oooo|
      //             ||| |__|____________ LINE NUMBER
      //             |||_________________ SMUDGE
      //             ||__________________ HOG
      //             |___________________ BUSY
      //
      if(uds)
      {
        Blitter.LineNumber=hibyte&TBlitter::MSK_LINE; // 4bit
        // As an optimisation, Smudge etc. aren't booleans but bytes which hold the bit as set by
        // the program. rBusy is $80
        Blitter.Smudge=hibyte&TBlitter::MSK_SMUDGE; // persistent
        Blitter.Hog=hibyte&TBlitter::MSK_HOG; // volatile
        Blitter.Busy=FALSE; // line
        Blitter.BusAccessCounter=0; // TMOUT reset when Busy line cleared
        if(!Blitter.rBusy) // register
        {
          if(hibyte&TBlitter::MSK_BUSY)
          { //start new
#if defined(SSE_ENABLE_TRACE_LOG)
            if(Blitter.YCount)
            {
              if(!Blitter.LineStarted)
                Blitter.nWordsToBlit=Blitter.XCount*Blitter.YCount;
              TRACE_LOG("PC %X F%d y%d c%d Blt %X Hop%d Op%X %dx%d=%d from %X(%d,%d) to %X(%d,%d) NF%X FX%X Sk%X Msk %X %X %X\n",
                old_pc,TIMING_INFO,hibyte,Blitter.Hop,Blitter.Op,Blitter.XCount,Blitter.YCount,Blitter.nWordsToBlit,Blitter.SrcAdr.d32,Blitter.SrcXInc,Blitter.SrcYInc,Blitter.DestAdr.d32,Blitter.DestXInc,Blitter.DestYInc,Blitter.NFSR,Blitter.FXSR,Blitter.Skew,Blitter.EndMask[0],Blitter.EndMask[1],Blitter.EndMask[2]);
            }
#endif
            if(Blitter.YCount)
            {
              Blitter.rBusy=TBlitter::MSK_BUSY;
              Blitter.Request=1;
              ASSERTB(A_S_T==a_s_t);
              Blitter.TimeToSwapBus=a_s_t+BLITTER_LATCH_LATENCY;
#if defined(SSE_STATS)
              Stats.nBlit++;
              Stats.nBlit1++;
              if(Blitter.Hog)
                Stats.nBlith++;
              if(LITTLE_PC>rom_addr)
                Stats.nBlitT++; 
#endif
            }
          }
        }
        else //there's already a blit in progress
        { 
          if(hibyte&TBlitter::MSK_BUSY) // Restart
          { 
 /* busy bit was already set, but by setting it again the blitter starts
    blitting after the same latency as if it was starting for the first time
    possible explanation from schematics (based on sheets 1 and 10 of '4082.pdf'):
    BUSY and hence TMOUT are reset for a while by LINEW when the program
    writes on the register containing BUSY bit 
    = HW trick used by Atari to allow prematurely restarting the blitter
 */
#if defined(SSE_ENABLE_TRACE_LOG)
            TRACE_LOG("PC %X F%d y%d c%d ReBlt %X Hop%d Op%X %dx%d=%d from %X(%d,%d) to %X(%d,%d) NF%d FX%d Sk%d Msk %X %X %X\n",
              old_pc,TIMING_INFO,hibyte,Blitter.Hop,Blitter.Op,Blitter.XCount,Blitter.YCount,Blitter.XCount*Blitter.YCount,Blitter.SrcAdr.d32,Blitter.SrcXInc,Blitter.SrcYInc,Blitter.DestAdr.d32,Blitter.DestXInc,Blitter.DestYInc,Blitter.NFSR,Blitter.FXSR,Blitter.Skew,Blitter.EndMask[0],Blitter.EndMask[1],Blitter.EndMask[2]);
#endif
            mfp_gpip_set_bit(MFP_GPIP_BLITTER_BIT,false);
            Blitter.Request=3; //3 : use Blit_Draw(), no init
            Blitter.TimeToSwapBus=A_S_T+BLITTER_LATCH_LATENCY;
          }
          else // Stop
          {    // e.g. Lethal XCess interrupts blit for Timer B that removes border
            TRACE_LOG("Blit stopped, %d/%d words blitted, phase %d, xc %d\n",
                      Blitter.nWordsBlitted,Blitter.nWordsToBlit,Blitter.BlittingPhase,Blitter.XCounter);
            Blitter.Request=0;
            Blitter.rBusy=0;
            mfp_gpip_set_bit(MFP_GPIP_BLITTER_BIT,false);
          }
        }
      }//uds
      //  FF 8A3D   |oo--oooo|
      //             ||  |__|_____________ SKEW
      //             ||___________________ NFSR
      //             |____________________ FXSR
      if(lds)
      {
        Blitter.Skew=lobyte&TBlitter::MSK_SKEW; //persistent, 4bit
        Blitter.NFSR=lobyte&TBlitter::MSK_NFSR; // persistent
        Blitter.FXSR=lobyte&TBlitter::MSK_FXSR; // persistent
      }
    case 0x1f:
      break;
    default:
      exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
    }//sw

#endif//DISABLE_BLITTER
    
    break;
  }


  //////////////
  // MEGA STE //
  //////////////

#if defined(SSE_MEGASTE)
  case 0x8C:
    if(IS_MEGASTE && lds && (ad_byte&0x80))
    { // 1 3 5 7 -> 0 2 4 6 -> 0 1 2 3
      int reg=(ad_byte&0x7)>>1;
      MegaSte.Scc[reg]=lobyte; // TODO
    }
    else 
      exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
    break;

  case 0x8E:
    if(IS_MEGASTE && lds && ad_byte<=0x20)
    {
      switch(ad_byte) {
      case 0x00:
        MegaSte.VmeSysMask=lobyte; // VME bus not emulated
        break;
      case 0x02:
        //VmeSysStat is RO
        exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
        break;
      case 0x04:
        MegaSte.VmeSysInt=lobyte;
        break;
      case 0x0c:
        MegaSte.VmeMask=lobyte;
        break;
      case 0x0e:
        //VmeStat is RO
        exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
        break;
      case 0x20:
#if defined(SSE_MEGA16)
        Cpu16.ScuReg&=~3; // clear bits 0 1
        Cpu16.ScuReg|=lobyte&3;
        if((lobyte&3)!=3)
          Cpu16.ScuReg&=~BIT_2; // clear turbo bit
#else
        Cpu16.ScuReg=(lobyte&3);
#endif
#if defined(SSE_MEGA_421)
//        if((Cpu16.ScuReg&BIT_0)==0)
//          Cpu16.Reset(); // too slow
#endif
        TRACE_SECTION(LOGSECTION_MMU,"Cache register %d",Cpu16.ScuReg);
        break;
      }//sw
    }
    else
      exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
    break;
#endif


  //////////////
  // GAMECART //
  //////////////

  case 0x90:
    if(IS_STF||ad_byte>0x00)
      exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
    if(uds)
    {
      Glue.gamecart=hibyte&1; // undocumented, spotted by czietz (not tested - no case)
      if(Glue.gamecart)
      {
        Glue.cartbase=0xD80000;
        Glue.cartend=Glue.cartbase+0x80000;
      }
      else
      {
        Glue.cartbase=GLU_CARTBASE;
        Glue.cartend=Glue.cartbase+0x20000;
      }
    }
    break;


  /////////////////////
  // STE controllers //
  /////////////////////
/*
Joystick 0 and Joystick 2 direction bits are read/write. If written to they will
be driven until a read is performed. Similarly, they will not be driven after a
read until a write is performed. 
*/
  case 0x92:
    // TODO Mega STE crash?
    if(ad_byte==0x02 && IS_STE)
      paddles_ReadMask=lobyte;
    else
      exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
    break;


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
      DWORD src=io_src_w&0xFEFE;
      int n=(addr-0xff9800)/4;
      DWORD val=emudetect_falcon_stpal[n];
      if(addr&2)
      {
        val&=0x0000FFFF;
        val|=(src<<16);
      }
      else
      {
        val&=0xFFFF0000;
        val|=src;
      }
      emudetect_falcon_stpal[n]=val;
      emudetect_falcon_palette_convert(n);
    }
    else
//#endif
      exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
    break;
#endif


  ////////////////////////
  // Emu Fake Registers //
  ////////////////////////

/*
  Secret addresses:
    poke byte into FFC123 - stops program running
    poke long into FFC1F0 - logs the string at the specified memory address,
                            which must be null-terminated
*/
  case 0xc1: //secret Steem registers!

#ifdef DEBUG_BUILD
    if(ad_byte==0x22 && lds && OPTION_EMU_DETECT)
    { //stop
      if(runstate==RUNSTATE_RUNNING)
      {
        runstate=RUNSTATE_STOPPING;
        SET_WHY_STOP("Software break - write to $FFC123");
        break;
      }
    }
    else if(ad_byte==0xf4 && uds  && OPTION_EMU_DETECT)
    {
      logfile_wipe();
      break;
    }
    if((ad_byte&0xfd)==0xf0 && uds && lds && OPTION_EMU_DETECT && !emudetect_write_logs_to_printer)
    {
      if(ad_byte==0xf0)
        TRACE("%s\n",+ReadStringFromMemory(m68k_src_l,500).Text);
      break; // no crash
    }
#endif//#ifdef DEBUG_BUILD
    if(emudetect_called) // option + procedure
    {
      TRACE("%X (%X)-> %X\n",io_src_w,BUS_MASK,addr);
      switch(ad_byte) {
      // 100.l = create disk image
      case 0x04:
        if(uds)
          emudetect_reset();
        if(lds)
          new_n_cpu_cycles_per_second=(int)lobyte*1000000;
        break;
      case 0x06:
        if(lds)
          snapshot_loaded=!!lobyte;
        break;
      case 0x08: // Run speed percent
        run_speed_ticks_per_second=100000/MAX((int)(io_src_w),50);
        break;
      case 0x0a:
        if(uds)
        {
          switch(hibyte) {
#if defined(SSE_GUI_STATUS_BAR_MOUSE)
          case 'M': //$4D
            SSEConfig.MouseAd=REGL(5); // Dungeon Master 16MHz
            break;
#endif
          case 'Q': //$51
            QuitSteem(); // request
            break;
          case 'R': //$52
            reset_st(RESET_COLD|RESET_NOSTOP|RESET_CHANGESETTINGS|RESET_BACKUP|RESET_COUNT);
            break;
          case 'r': //$72
            reset_st(RESET_WARM|RESET_NOSTOP|RESET_CHANGESETTINGS|RESET_BACKUP|RESET_COUNT);
            break; //411R10
          case 'S': //$53
            runstate=RUNSTATE_STOPPING;
            break;
          }
        }
        break;
      case 0x1a:
        if(uds)
          emudetect_write_logs_to_printer=!!hibyte;
#if !defined(SSE_NO_FALCONMODE)
        if(lds && extended_monitor==0 && screen_res<2)
          emudetect_falcon_mode=BYTE(lobyte);
#endif
        break;
      case 0x1c:
#if !defined(SSE_NO_FALCONMODE)
        if(uds)
        {
          emudetect_falcon_mode_size=(hibyte&1)+1;
          emudetect_falcon_extra_height=((hibyte&2)!=0);
          // Make sure we don't mess up video memory. It is possible that the height of
          // scanlines is doubled, if we change to 400 with double height lines then arg!
          if(draw_lock)
            draw_set_jumps_and_source();
        }
        if(lds)
          emudetect_overscans_fixed=(lobyte!=0);
#endif
        break;
      case 0x00:
      {
        DWORD ad=m68k_src_l;
        Str Name=ReadStringFromMemory(ad,500);
        ad+=(int)Name.Length()+1;
        int Param[10]={0,0,0,0,0, 0,0,0,0,0};
        Str Num;
        for(int n=0;n<10;n++)
        {
          Num=ReadStringFromMemory(ad,16);
          if(Num.Length()==0)
            break;
          ad+=(int)Num.Length()+1;
          Param[n]=atoi(Num);
        }
        WORD Sides=2,TracksPerSide=80,SectorsPerTrack=9;
        if(Param[0]==1||Param[0]==2)
          Sides=(WORD)Param[0];
        if(Param[1]>=10&&Param[1]<=FLOPPY_MAX_TRACK_NUM+1)
          TracksPerSide=(WORD)Param[1];
        if(Param[2]>=1&&Param[2]<=FLOPPY_MAX_SECTOR_NUM)
          SectorsPerTrack=(WORD)Param[2];
        Str DiskPath=DiskMan.HomeFol+SLASH+Name+".st";
        DiskMan.CreateDiskImage(Name,TracksPerSide*Sides*SectorsPerTrack,SectorsPerTrack,Sides);
        DiskMan.InsertDisk(DRIVE_A,Name,DiskPath);
        break;
      }
      case 0xf0:
#ifdef DEBUG_BUILD
        if(emudetect_write_logs_to_printer)
        {
          // This can't be turned on unless you call emudetect, so 0xffc1f0 will still work normally
          Str Text=ReadStringFromMemory(m68k_src_l,500);
          for(INT_PTR i=0;i<Text.Length();i++) 
            ParallelPort.OutputByte(Text[i]);
          ParallelPort.OutputByte(13); //CR
          ParallelPort.OutputByte(10); //LF
        }
#else
        if(emudetect_write_logs_to_printer)
        {
          // This can't be turned on unless you call emudetect, so 0xffc1f0 will still work normally
          Str Text=ReadStringFromMemory(m68k_src_l,500);
          for(INT_PTR i=0;i<Text.Length();i++) 
            ParallelPort.OutputByte(Text[i]);
          ParallelPort.OutputByte(13); //CR
          ParallelPort.OutputByte(10); //LF
        }
        else
        {
          TRACE("ST -- %s\n",+ReadStringFromMemory(m68k_src_l,500).Text);
        }
#endif
        break;
      }//sw
      if(ad_byte<0x20||ad_byte==0xf0)
        break; // No exception!
    }//emudetect
    exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr); // no break


  /////////////
  // ADSPEED //
  /////////////

#if defined(SSE_MEGA16)
  // software control of the AdSpeed board
  // by choice only for Mega ST
  case 0xf0: // set 16MHz
    if(ST_MODEL==MEGA_ST)
      Cpu16.ScuReg|=3;
    else
       exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
    break;
  case 0xf1: // set 8MHz
    if(ST_MODEL==MEGA_ST)
      Cpu16.ScuReg=0;
    else
       exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
    break;
#endif


  /////////
  // MFP //
  /////////

  case 0xfa:
  {
    if(ad_byte<0x40 && lds) // conditions for MFP DTACK
    {
      if(SSEOptions.MfpWsTmg[2])
      {
        BUS_WAIT_STATES(SSEOptions.MfpWsTmg[2]);
      }
      if(ad_byte<0x30) // actual registers
      {
        BYTE n=ad_byte>>1;
        if(stem_runmode==STEM_MODE_CPU)
#if defined(SSE_TIMINGS32A) && !defined(SSE_MFP_421) // Why? With -1 we'll miss events
          if(sys_cycles<=-1)
#else
          if(sys_cycles<=0)
#endif
          {
#if defined(SSE_DEBUGGER_FAKE_IO)
            if(TRACE_MASK2&TRACE_CONTROL_EVENT)
            {
              TRACE_EVENT("Write MFP",event_vector);
            }
#endif
            event_vector(); // event could alter mfp regs + IPL
        }
        a_s_t=A_S_T; // just in case (DMA cycles?)
        Mfp.UpdateNextIrq(a_s_t-4*TICKS8); // update first before the write
        if(n==MFPR_GPIP||n==MFPR_AER||n==MFPR_DDR)
        {
          // The output from the AER is eored with the GPIP/input buffer state
          // and that input goes into a 1-0 transition detector. So if the result
          // used to be 1 and now it is 0 an interrupt will occur (if the
          // interrupt is enabled of course).
          BYTE old_gpip=(Mfp.reg[MFPR_GPIP]&(~Mfp.reg[MFPR_DDR]));
          old_gpip|=(Mfp.gpip_buffer&Mfp.reg[MFPR_DDR]);
          BYTE old_aer=Mfp.reg[MFPR_AER];
          if(n==MFPR_GPIP)  // Write to GPIP (can only change bits set to 1 in DDR)
          {
            BYTE x=lobyte&Mfp.reg[MFPR_DDR];
            // Don't change the bits that are 0 in the DDR
            x|=Mfp.gpip_buffer&(~Mfp.reg[MFPR_DDR]);
            Mfp.gpip_buffer=x;
          }
          else
          {
            Mfp.reg[n]=lobyte;
            // maybe Timer B's AER bit changed? (bit 3 is blitter bit in GPIP)           
            if(n==MFPR_AER)
            {
              if((old_aer&MFP_GPIP_BLITTER_MASK)!=(lobyte&MFP_GPIP_BLITTER_MASK))
              {
#if defined(SSE_VID_STVL1)                  
                if(OPTION_C3)
                  StvlUpdate();
                else
#endif
                  Mfp.ComputeNextTimerB();
              }
            }
          }
          BYTE new_gpip=Mfp.reg[MFPR_GPIP]&(~Mfp.reg[MFPR_DDR]);
          new_gpip|=Mfp.gpip_buffer&Mfp.reg[MFPR_DDR];
          BYTE new_aer=Mfp.reg[MFPR_AER];
          for(int bit=0;bit<8;bit++)
          {
            BYTE irq=mfp_gpip_irq[bit];
            if(mfp_interrupt_enabled[irq])
            {
              BYTE mask=(BYTE)(1<<bit);
              bool old_1_to_0_detector_input=(((old_gpip&mask)^(old_aer&mask))==mask);
              bool new_1_to_0_detector_input=(((new_gpip&mask)^(new_aer&mask))==mask);
              if(old_1_to_0_detector_input && !new_1_to_0_detector_input)
              {
                //ASSERT(a_s_t==ABSOLUTE_SYS_TIME);
                // Transition the right way! Set pending (interrupts happen later)
                Mfp.SetPending(irq,a_s_t);
              }//if
            }//if
          }//nxt
        }
        else if(n>=MFPR_IERA && n<=MFPR_IERB)  //enable
        {
#if defined(SSE_DEBUGGER) || defined(SSE_LOGSECTIONS_FOR_ALL)
          if(TRACE_LOG_ENABLED(LOGSECTION_MFP))
            TRACE_LOG(PRICV " PC %X MFP IER%c %X -> %X\n",A_S_T,old_pc,'A'+n-MFPR_IERA,Mfp.reg[n],lobyte);
#endif
          Mfp.reg[n]=lobyte;
          Mfp.CalcInterruptsEnabled();
          for(int ti=0;ti<N_MFP_TIMERS;ti++)
          {
            bool new_enabled=(mfp_interrupt_enabled[mfp_timer_irq[ti]]
              &&(Mfp.GetTimerControlRegister(ti)&MFP_TIMER_DELAY_MASK));
            if(new_enabled && mfp_timer_enabled[ti]==0)
            {
              ASSERTB(a_s_t==ABSOLUTE_SYS_TIME);
              // Timer should have been running but isn't, must put into future
              COUNTER_VAR stage=mfp_timer_timeout[ti]-a_s_t;
              if(stage<=0)
                stage+=((-stage/mfp_timer_period[ti])+1)*mfp_timer_period[ti];
              else
                stage%=mfp_timer_period[ti];
              mfp_timer_timeout[ti]=a_s_t+stage;
            }
            mfp_timer_enabled[ti]=new_enabled;
            mfp_timer_check[ti]=mfp_timer_enabled[ti]||mfp_timer_period_change[ti];
          }
          *Mfp.ipr&=*Mfp.ier; //no pending on disabled registers (optimised)
          //Mfp.reg[MFPR_IPRA]&=Mfp.reg[MFPR_IERA]; //no pending on disabled registers
          //Mfp.reg[MFPR_IPRB]&=Mfp.reg[MFPR_IERB]; //no pending on disabled registers
        }
        else if(n>=MFPR_IPRA && n<=MFPR_ISRB) //can only clear bits in IPR, ISR
        {
#if defined(SSE_DEBUGGER) || defined(SSE_LOGSECTIONS_FOR_ALL)
          if(TRACE_LOG_ENABLED(LOGSECTION_MFP))
          {
            if(n>=MFPR_IPRA && n<=MFPR_IPRB)
              TRACE_LOG(PRICV " PC %X MFP IPR%c %X -> %X\n",A_S_T,old_pc,'A'+n-MFPR_IPRA,Mfp.reg[n],Mfp.reg[n]&lobyte);
            else
              TRACE_LOG(PRICV " PC %X MFP ISR%c %X -> %X\n",A_S_T,old_pc,'A'+n-MFPR_ISRA,Mfp.reg[n],Mfp.reg[n]&lobyte);
          }
#endif
          Mfp.reg[n]&=lobyte;
        }
        else if(n>=MFPR_TADR && n<=MFPR_TDDR)
        {
#if defined(SSE_DEBUGGER) || defined(SSE_LOGSECTIONS_FOR_ALL)
          if(TRACE_LOG_ENABLED(LOGSECTION_MFP))
            TRACE_LOG(PRICV " PC %X MFP T%cDR %X -> %X\n",A_S_T,old_pc,'A'+n-MFPR_TADR,Mfp.reg[n],lobyte);
#endif
          Mfp.SetTimerReg(n,lobyte);
        }
        else if(n==MFPR_TACR||n==MFPR_TBCR)
        {
#if defined(SSE_DEBUGGER) || defined(SSE_LOGSECTIONS_FOR_ALL)
          if(TRACE_LOG_ENABLED(LOGSECTION_MFP))
            TRACE_LOG(PRICV " PC %X MFP T%cCR %X -> %X\n",A_S_T,old_pc,'A'+n-MFPR_TACR,Mfp.reg[n],lobyte);
#endif
          lobyte&=0x0F;
          Mfp.SetTimerReg(n,lobyte);
        }
        else if(n==MFPR_TCDCR)
        {
#if defined(SSE_DEBUGGER) || defined(SSE_LOGSECTIONS_FOR_ALL)
          if(TRACE_LOG_ENABLED(LOGSECTION_MFP))
            TRACE_LOG(PRICV " PC %X MFP TCDCR %X -> %X\n",A_S_T,old_pc,Mfp.reg[n],lobyte);
#endif
          lobyte&=b01110111;
          Mfp.SetTimerReg(n,lobyte);
        }
        else if(n==MFPR_VR)
        {
#if defined(SSE_DEBUGGER) || defined(SSE_LOGSECTIONS_FOR_ALL)
          if(TRACE_LOG_ENABLED(LOGSECTION_MFP))
            TRACE_LOG(PRICV " PC %X MFP VR %X -> %X\n",A_S_T,old_pc,Mfp.reg[n],lobyte);
#endif
          Mfp.reg[n]=lobyte;
          if(!(lobyte&BIT_3)) // clearing this bit clears In Service registers
            *Mfp.isr=0;   //Mfp.reg[MFPR_ISRA]=Mfp.reg[MFPR_ISRB]=0;
        }
        else if(n>=MFPR_SCR)
          RS232_WriteReg(n,lobyte);
        else
        {
          //ASSERT(n<16);
#if defined(SSE_DEBUGGER) || defined(SSE_LOGSECTIONS_FOR_ALL)
          if(TRACE_LOG_ENABLED(LOGSECTION_MFP)&&(n>=MFPR_IMRA && n<=MFPR_IMRB))
            TRACE_LOG(PRICV " PC %X MFP IMR%c %X -> %X (IER%c %X)\n",A_S_T,old_pc,'A'+n-MFPR_IMRA,Mfp.reg[n],lobyte,'A'+n-MFPR_IMRA,Mfp.reg[MFPR_IERA+n-MFPR_IMRA]);
#endif
          Mfp.reg[n]=lobyte;
        }
        Mfp.UpdateTimerControls();
        // randomisation for spurious interrupt
        COUNTER_VAR t=(OPTION_SPURIOUS) ? (a_s_t-((a_s_t&(8*TICKS8))!=0)*TICKS8) : a_s_t;
        Mfp.UpdateNextIrq(t);
        prepare_next_event();
      }//if(addr<0xfffa30) 
      if(SSEOptions.MfpWsTmg[3])
      {
        BUS_WAIT_STATES(SSEOptions.MfpWsTmg[3]); // not by default
      }
    }
    else // beyond allowed range
      exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
    break;
  }

#undef LOGSECTION
#define LOGSECTION LOGSECTION_IOW


  ///////////////////////////
  // ACIAs (IKBD and MIDI) //
  ///////////////////////////

  case 0xfc:
#undef LOGSECTION
#define LOGSECTION LOGSECTION_ACIA
  {  
    TMC6850::SyncEClock(); // VPA cycle, no DTACK, no BERR
    BYTE acia_num=(ad_byte&4)>>2;
    if(uds) 
    {
      switch(ad_byte) {
      case 0x00: // Keyboard ACIA Control
      case 0x04: // MIDI ACIA Control
        //ASSERT(acia_num==0 || acia_num==1);
        acia[acia_num].cr=hibyte; // no option test
        if((hibyte&(ACIA_CD1|ACIA_CD2))==(ACIA_CD1|ACIA_CD2))  // 'Master reset'
          ACIA_Reset(acia_num,false);
        else
          ACIA_SetControl(acia_num,hibyte); // TOS: $95 for MIDI, $96 for IKBD
        break;
      case 0x02:  // Keyboard ACIA Data
      case 0x06:  // MIDI ACIA Data
        //ASSERT(acia_num==0 || acia_num==1);
        acia[acia_num].tdr=hibyte;
        TRACE_LOG("ACIA %d PC %X TDR %X\n",acia_num,old_pc,hibyte);
        if(OPTION_C1)
        {
          acia[acia_num].sr&=~ACIA_TDRE;
          ACIA_CheckIrq(acia_num); // writing on TDR clears the TX IRQ
          // line was free
          if(!acia[acia_num].LineTxBusy)
          {
            // delay before transmission starts
            // "within 1-bit time of the trailing edge of the Write command"
            int copy2tdr_delay=1024>> ((acia[acia_num].cr&1)<<1);
            //ASSERT(copy2tdr_delay==(16*((acia[acia_num].cr&1)?16:64)));
            //ASSERT(copy2tdr_delay==1024||copy2tdr_delay==256); // IKBD - MIDI
            acia[acia_num].TimeTx=a_s_t+copy2tdr_delay;
            if(acia[acia_num].TimeTx-time_of_event_acia<=0)
              time_of_event_acia=acia[acia_num].TimeTx;
            acia[acia_num].LineTxBusy=2; // indicates we're waiting for TDR->TDRS
          }
          break;
        }//option C1
#if !defined(SSE_NO_OLD_IKBD)
        {
          bool TXEmptyAgenda=(agenda_get_queue_pos(acia_num==ACIA_IKBD
            ? agenda_acia_tx_delay_IKBD : agenda_acia_tx_delay_MIDI)>=0);
          if(!TXEmptyAgenda)
          {
            if(acia[acia_num].tx_irq_enabled)
            {
              acia[acia_num].irq=FALSE;
              mfp_gpip_set_bit(MFP_GPIP_ACIA_BIT,!(acia[ACIA_IKBD].irq || acia[ACIA_MIDI].irq));
            }
            agenda_add((acia_num==ACIA_IKBD) ? agenda_acia_tx_delay_IKBD : agenda_acia_tx_delay_MIDI,
              ACIAClockToHBLS(acia[acia_num].clock_divide),false);
          }
          acia[acia_num].tx_flag=TRUE; //flag for transmitting
          //Steem 3.2, not C1, different paths for IKBD and MIDI:
          if(acia_num==ACIA_IKBD)
          {
            // If send new byte before last one has finished being sent
            if(abs_quick(a_s_t-acia[acia_num].last_tx_write_time)<ACIA_CYCLES_NEEDED_TO_START_TX)
            {
                // replace old byte with new one
                int n=agenda_get_queue_pos(agenda_ikbd_process);
                if(n>=0)
                  agenda[n].param=hibyte;
            }
            else
            {
              // there is a delay before the data gets to the IKBD
              acia[acia_num].last_tx_write_time=a_s_t;
              agenda_add(agenda_ikbd_process,IKBD_HBLS_FROM_COMMAND_WRITE_TO_PROCESS,hibyte);
            }
          }
          else
            MIDIPort.OutputByte(hibyte);
          break;
        }
#endif//#if !defined(SSE_NO_OLD_IKBD)
      default:
        break;  //all writes allowed
      }//sw
    }//if
    break;
  }


  //////////////
  // MEGA RTC //
  //////////////

  case 0xfd:
    TMC6850::SyncEClock(); // GSTMCU schematics: those addresses also trigger a VPA cycle
#if defined(SSE_MEGA_RTC)
    if(SSEConfig.Mega && lds && ad_byte>=0x20 && ad_byte<=0x3E)
      MegaRtc.Write(addr,lobyte);
#endif
    break;


  /////////////
  // Alt-RAM //
  /////////////
    
#if defined(SSE_MMU_MONSTER_ALT_RAM)
/*  MonSTer board special register to activate alt-RAM.
    Specify size in megabytes.
*/
  case 0xfe: 
  {
    if(mem_len!=MEM_12MB || (bus_mask&BUS_MASK_WORD)!=BUS_MASK_WORD || ad_byte>3) //don't know reg size
      exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
    else if(ad_byte==0)
    {
      if(io_src_w<=8) // max 8MB // TODO, maybe the device auto-limits?
        Mmu.MonSTerHimem=(4+io_src_w)*0x100000;
    }
    break;
  }
#endif

  default: //unrecognised - no DTACK
    exception(EXCEPTION_BUS_ERROR,EA_WRITE,addr);
  }//switch(addr&0xffff00)
}


// called by d2_poke() (debugger) and PatchPoke() (all builds)
void io_write_b(MEM_ADDRESS addr,BYTE io_src_b) { 
  DU16 udb;
  udb.d8[LO]=udb.d8[HI]=io_src_b;
  io_write(addr&0xfffffe,udb);
}


// called by d2_dpoke(), d2_lpoke() (debugger) and PatchPoke() (all builds)
void io_write_w(MEM_ADDRESS addr,WORD io_src_w) {
  DU16 udb(io_src_w);
  io_write(addr&0xfffffe,udb);
}

#undef LOGSECTION
