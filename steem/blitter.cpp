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
FILE: blitter.cpp
DESCRIPTION: High level emulation of the Atari BLiTTER (Bit-Block Transfer
Processor) chip present on the Mega ST (hopefully) and the STE.
When the Blitter uses the bus, the CPU can't and is essentially paralysed.
No miracle, the bus is shared with the CPU and the video logic.
This limits the speed gains offered by the chip, but the Blitter can also
efficiently manipulate data it blits. The ST Blitter can access the full
address bus range, including devices (palette...)
The low-level stBlitter.sv by ijor was used as inspiration for some
parts like NFSR business, busy register... (SVN r973)
We emulate the timing difference of the Mega STE.
The I/O part is in iow.cpp and ior.cpp.
Bus arbitration timing between the CPU and the Blitter is rather precisely
emulated, it is needed for correctly rendering some games and demos.
The disk DMA/Blitter arbitration (disk DMA has higher priority) is more
or less emulated, depending on the type of disk image. 
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <computer.h>
#include <iolist.h>
#if defined(SSE_VID_STVL1)
#include <interface_stvl.h>
#endif

#define LOGSECTION LOGSECTION_BLITTER

void Blitter_Start_Line();
void Blitter_Blit_Word();
void Blitter_ReadSource(MEM_ADDRESS SrcAdr);
WORD Blitter_DPeek(MEM_ADDRESS ad);
void Blitter_DPoke();
void Blitter_Draw();
void Blitter_Start_Now();

void Blitter_CheckRequest() {
  // this function is called by CPU timing functions
  // the latency is probably due to the 'restart' possibility, see
  // iow.cpp
  a_s_t=ABSOLUTE_SYS_TIME;
  if(Blitter.Busy) // blit mode (non-hog), autostart, rare in demos, frequent in GEM
  {
    if(Blitter.Request==1&&!(Blitter.BusAccessCounter&TBlitter::MSK_TMOUT)) // ~TMOUT
    {
      Blitter.BusAccessCounter&=0x7F; // counter is 7 bit
      Blitter.Request++;
      Blitter.TimeToSwapBus=a_s_t+BLITTER_LATCH_LATENCY; // OK MSTE?
    }
    else if(Blitter.Request==2&&(a_s_t-Blitter.TimeToSwapBus>=0))
    {
      Blitter.BusAccessCounter&=0x7F;
#if defined(SSE_ENABLE_TRACE_LOG)
      TRACE_LOG("PC %X F%d y%d c%d AutoBlt Hop%d Op%X %dx%d=%d from %X(%d,%d) to %X(%d,%d) NF%X FX%X Sk%X Msk %X %X %X\n",
        old_pc,TIMING_INFO,Blitter.Hop,Blitter.Op,Blitter.XCount,Blitter.YCount,Blitter.XCount*Blitter.YCount,Blitter.SrcAdr.d32,
        Blitter.SrcXInc,Blitter.SrcYInc,Blitter.DestAdr.d32,Blitter.DestXInc,Blitter.DestYInc,Blitter.NFSR,Blitter.FXSR,Blitter.Skew,
        Blitter.EndMask[0],Blitter.EndMask[1],Blitter.EndMask[2]);
#endif
      Blitter_Draw();
    }
  }
  // hog mode + blit mode, start, restart triggered
  else if((a_s_t-Blitter.TimeToSwapBus)>=0)
  {
    Blitter.BusAccessCounter&=0x7F;
    if(Blitter.Request==3) // restarting the blit
      Blitter_Draw();
    else
      Blitter_Start_Now(); // starting a blit (init line)
  }
}


void Blitter_Start_Now() {
  Blitter.Request=0;
  Blitter.YCounter=Blitter.YCount;
  if(Blitter.YCounter==0)
    Blitter.YCounter=0x10000;
  /*Only want to start the line if not in the middle of one.
    Lethal Xcess: blit could be interrupted before writing 1st word! */
  if(!Blitter.LineStarted)
  {
    Blitter_Start_Line();
    Blitter.nWordsBlitted=0;
  }
  Blitter_Draw();
}


void Blitter_Draw() {
  Blitter.Request=0;
  BUS_SAVE; // because we use the same variables for the bus, not a set for each chip
  if(Blitter.YCount==0)
  { // NO BLIT/BLIT FINISHED
    Blitter.rBusy=Blitter.Busy=Blitter.Hog=0;
    TRACE_LOG("Nothing to blit, %d/%d words blitted, phase %d\n",Blitter.nWordsBlitted,Blitter.nWordsToBlit,Blitter.BlittingPhase);
    Blitter.nWordsBlitted=0;
    mfp_gpip_set_bit(MFP_GPIP_BLITTER_BIT,false);
    return;
  }
  Blitter.Busy=TRUE;
  mfp_gpip_set_bit(MFP_GPIP_BLITTER_BIT,true);
  Blitter.TimeAtBlit=A_S_T; // to record #blit cycles
#if defined(SSE_MEGASTE)
  //Cpu16.Reset(); // too slow
  if(IS_MEGASTE) // The Bus Grant signal goes through PAL U002 and U011 -> more delay
  {
    CPU_BUS_IDLE((2+4)*(1<<((Cpu16.ScuReg&2)>>1))); //  or 4+4?
  }
  else
#endif
  {
    CPU_BUS_IDLE(BLITTER_START_WAIT); // not bus_jam
  }
  Blitter.HasBus=TRUE;
  while(Blitter.HasBus)
  {
    while(Blitter.HasBus && sys_cycles>0)
    {
      Blitter_Blit_Word();
      if(Blitter.Busy)
      {
        // time to stop?
        if(!Blitter.Hog && (Blitter.BusAccessCounter&TBlitter::MSK_TMOUT)) // TMOUT
        {
          TRACE_LOG("Blit paused, %d/%d words blitted, phase %d, xc %d\n",
                    Blitter.nWordsBlitted,Blitter.nWordsToBlit,Blitter.BlittingPhase,Blitter.XCounter);
          BUS_WAIT_STATES(BLITTER_END_WAIT); //arbitration
          Blitter.HasBus=FALSE;
          Blitter.Request=1; // blit not finished
          Blitter.BlitCycles=A_S_T-Blitter.TimeAtBlit;
          Blitter.BusAccessCounter&=0x7F;
        }
      }
      else // finished
      {
        Blitter.HasBus=FALSE;
        break;
      }
    }
    while(sys_cycles<=0) 
    {
#if defined(SSE_DEBUGGER_FAKE_IO)
      if(TRACE_MASK2&TRACE_CONTROL_EVENT)
        TRACE_EVENT("BLiT",event_vector);
#endif
      event_vector();
      prepare_next_event();
    }
  }

#if defined(SSE_DEBUGGER)
  // write the BLiT in history
  //pc_history_y[pc_history_idx]=scan_y; // irrelevant
  pc_history_c[pc_history_idx]=(SHORT)(A_S_T-Blitter.TimeAtBlit); // we can display blit cycles
  pc_history[pc_history_idx++]=MAGIC_HIST_BLIT;
  if(pc_history_idx>=HISTORY_SIZE)
    pc_history_idx=0;
#endif

  BUS_RESTORE;
}


void Blitter_Start_Line() {
  if(Blitter.YCounter<=0) 
  { // Blit finished?
    Blitter.rBusy=Blitter.Hog=Blitter.Busy=Blitter.HasBus=0; // hog bit also reset (BLTBENCH.TOS)
#ifdef SSE_ENABLE_TRACE_LOG
    TRACE_LOG("Blit done, %d/%d words blitted, phase %d\n",
              Blitter.nWordsBlitted,Blitter.nWordsToBlit,Blitter.BlittingPhase);
    if(Blitter.nWordsBlitted!=Blitter.nWordsToBlit)
      TRACE_LOG("ERROR: %d to blit\n",Blitter.nWordsToBlit);
#endif
    Blitter.nWordsBlitted=0;
    mfp_gpip_set_bit(MFP_GPIP_BLITTER_BIT,false); // nobody uses that
#if defined(SSE_MEGASTE)
    //Cpu16.Reset(); // too slow
    if(IS_MEGASTE)
    {
      CPU_BUS_IDLE(BLITTER_END_WAIT*(1<<((Cpu16.ScuReg&2)>>1)));
    }
    else
#endif
    {
      CPU_BUS_IDLE(BLITTER_END_WAIT);
    }
/*  Record # blit cycles during which the CPU could work without
    accessing the bus. More like real emulation, but it has a cost.
    A bit hacky.
*/
    Blitter.BlitCycles=A_S_T-Blitter.TimeAtBlit;
    Blitter.LineStarted=false;
#ifdef DEBUG_BUILD
    if(stop_on_blitter_flag && runstate==RUNSTATE_RUNNING) 
    {
      runstate=RUNSTATE_STOPPING;
      runstate_why_stop="BLiT";
    }
#endif
  }
  else 
  { //prepare next line
    Blitter.Mask=Blitter.EndMask[0]; // ENDMASK 1 mask for 1st word
    Blitter.Last=FALSE;
    if(Blitter.FXSR && (Blitter.Op%5)!=0 && (Blitter.Hop>1||Blitter.Hop==1&&Blitter.Smudge))
      Blitter.BlittingPhase=TBlitter::PRIME;
    else
      Blitter.BlittingPhase=TBlitter::READ_SOURCE;
    Blitter.LineStarted=true;
  }
}


void Blitter_Blit_Word() { // only called by Blitter_Draw()
  switch(Blitter.BlittingPhase) {
  case TBlitter::PRIME: // = prefetch = FXSR = Force Extra Source Read
    abus=Blitter.SrcAdr.d32;
    BLT_BUS_ACCESS_READ;
    Blitter_ReadSource(abus);
    Blitter.SrcAdr.d32+=Blitter.SrcXInc;
    Blitter.BlittingPhase++;
    break;
  case TBlitter::READ_SOURCE:
    if(Blitter.XCounter==1) // last word
    {
      Blitter.Last=TRUE;
      if(Blitter.XCount>1)
        Blitter.Mask=Blitter.EndMask[2]; // ENDMASK 3 for final write of the line
    }
    if((Blitter.Op%5)!=0&&(Blitter.Hop>1||(Blitter.Hop==1&&Blitter.Smudge))) 
    {
      if(Blitter.NFSR && Blitter.Last) // NFSR = No Final Source Read
      {
        if(Blitter.SrcXInc>=0)
          Blitter.SrcBuffer.d32<<=16;
        else 
          Blitter.SrcBuffer.d32>>=16;
      }
      if(!Blitter.Last || !Blitter.NFSR || Blitter.XCount==1)
      {
        abus=Blitter.SrcAdr.d32;
        BLT_BUS_ACCESS_READ;
        Blitter_ReadSource(Blitter.SrcAdr.d32);
      }
      if(Blitter.NFSR && Blitter.XCounter==2 // 4 cycles before (doesn't matter)
        || Blitter.Last && !Blitter.NFSR)
        Blitter.SrcAdr.d32+=Blitter.SrcYInc;
      else if(!(Blitter.NFSR && Blitter.XCounter==1))
        Blitter.SrcAdr.d32+=Blitter.SrcXInc;
    }
    ASSERTB((Blitter.LineNumber&0xf)==Blitter.LineNumber);
#ifndef SSE_LEAN_AND_MEAN
    Blitter.LineNumber&=0xf;
#endif
    switch(Blitter.Hop) {
    case 0:
      Blitter.SrcDat=0xffff; //fill
      break;
    case 1:
      if(Blitter.Smudge)  //strange but as documented
        Blitter.SrcDat=Blitter.HalfToneRAM[(Blitter.SrcBuffer.d32>>Blitter.Skew)&0xf];
      else
        Blitter.SrcDat=Blitter.HalfToneRAM[Blitter.LineNumber];
      break;
    default:
      Blitter.SrcDat=(WORD)(Blitter.SrcBuffer.d32>>Blitter.Skew);
      if(Blitter.Hop==3) 
      {
        if(Blitter.Smudge)
          Blitter.SrcDat&=Blitter.HalfToneRAM[Blitter.SrcDat&0xf];
        else
          Blitter.SrcDat&=Blitter.HalfToneRAM[Blitter.LineNumber];
      }
    }//sw
    Blitter.BlittingPhase++;
    break;
  case TBlitter::READ_DEST:
    Blitter.DestDat=0;
    if(Blitter.NeedDestRead||Blitter.Mask!=0xffff) 
    {
      abus=Blitter.DestAdr.d32;
      BLT_BUS_ACCESS_READ;
      Blitter.DestDat=Blitter_DPeek(Blitter.DestAdr.d32);
      dbus=Blitter.DestDat;
      Blitter.NewDat=Blitter.DestDat & (~Blitter.Mask);
    }
    else
      Blitter.NewDat=0;
    switch(Blitter.Op) {
    case 0: // 0 0 0 0    - Target will be zeroed out (blind copy)
      Blitter.NewDat|=0; 
      break;
    case 1: // 0 0 0 1    - Source AND Target         (inverse copy)
      Blitter.NewDat|=(Blitter.SrcDat & Blitter.DestDat) & Blitter.Mask; 
      break;
    case 2: // 0 0 1 0    - Source AND NOT Target     (mask copy)
      Blitter.NewDat|=(Blitter.SrcDat & ~Blitter.DestDat) & Blitter.Mask; 
      break;
    case 3: // 0 0 1 1    - Source only               (replace copy)
      Blitter.NewDat|=Blitter.SrcDat & Blitter.Mask; 
      break;
    case 4: // 0 1 0 0    - NOT Source AND Target     (mask copy)
      Blitter.NewDat|=(~Blitter.SrcDat & Blitter.DestDat) & Blitter.Mask; 
      break;
    case 5: // 0 1 0 1    - Target unchanged          (null copy)
      Blitter.NewDat|=Blitter.DestDat & Blitter.Mask; 
      break;
    case 6: // 0 1 1 0    - Source XOR Target         (xor copy)
      Blitter.NewDat|=(Blitter.SrcDat ^ Blitter.DestDat) & Blitter.Mask; 
      break;
    case 7: // 0 1 1 1    - Source OR Target          (combine copy)
      Blitter.NewDat|=(Blitter.SrcDat|Blitter.DestDat) & Blitter.Mask; 
      break;
    case 8: // 1 0 0 0    - NOT Source AND NOT Target (complex mask copy)
      Blitter.NewDat|=(~Blitter.SrcDat & ~Blitter.DestDat) & Blitter.Mask; 
      break;
    case 9: // 1 0 0 1    - NOT Source XOR Target     (complex combine copy)
      Blitter.NewDat|=(~Blitter.SrcDat ^ Blitter.DestDat) & Blitter.Mask; 
      break;
    case 10: // 1 0 1 0    - NOT Target                (reverse, no copy)
      Blitter.NewDat=Blitter.DestDat^Blitter.Mask; 
      break;  // ~DestAdr & Blitter.Mask
    case 11: // 1 0 1 1    - Source OR NOT Target      (mask copy)
      Blitter.NewDat|=(Blitter.SrcDat|~Blitter.DestDat) & Blitter.Mask; 
      break;
    case 12: // 1 1 0 0    - NOT Source                (reverse direct copy)
      Blitter.NewDat|=(~Blitter.SrcDat) & Blitter.Mask; 
      break;
    case 13: // 1 1 0 1    - NOT Source OR Target      (reverse combine)
      Blitter.NewDat|=(~Blitter.SrcDat|Blitter.DestDat) & Blitter.Mask; 
      break;
    case 14: // 1 1 1 0    - NOT Source OR NOT Target  (complex reverse copy)
      Blitter.NewDat|=(~Blitter.SrcDat|~Blitter.DestDat) & Blitter.Mask; 
      break;
    case 15: // 1 1 1 1    - Target is set to "1"      (blind copy)
      Blitter.NewDat|=Blitter.Mask; 
      break;
    }//sw
    Blitter.BlittingPhase++;
    break;
  case TBlitter::WRITE_DEST:
    abus=Blitter.DestAdr.d32;
    dbus=Blitter.NewDat;
    BLT_BUS_ACCESS_WRITE;
    Blitter_DPoke();
    Blitter.nWordsBlitted++;
    SHORT inc;
    if(Blitter.Last)
    {
      // BLiTTER bug/feature: NFSR spares a memory read but whatever garbage is on the
      // data bus is still latched into the skew buffer (stBlitter.sv, bugrepro.msa)
      if(Blitter.NFSR)
      {
        DWORD x=dbus; // "bltOut"
        if(Blitter.SrcXInc<0)
          x<<=16;
        Blitter.SrcBuffer.d32|=x;
      }
      inc=Blitter.DestYInc;
    }
    else
      inc=Blitter.DestXInc;
    Blitter.DestAdr.d32+=inc;
    Blitter.Mask=Blitter.EndMask[1]; // ENDMASK 2 for all writes except first & last
    if((--Blitter.XCounter)<=0) 
    {
      if(Blitter.DestYInc<0)
        Blitter.LineNumber--;
      else
        Blitter.LineNumber++;
      Blitter.LineNumber&=TBlitter::MSK_LINE;
      Blitter.YCounter--;
      Blitter.YCount=(WORD)Blitter.YCounter;
      Blitter.XCounter=(Blitter.XCount) ? Blitter.XCount : 0x10000;  //init blitter for line
      Blitter.LineStarted=false; // line finished
      Blitter_Start_Line();
    }
    if(Blitter.BlittingPhase!=TBlitter::PRIME)
      Blitter.BlittingPhase=TBlitter::READ_SOURCE;
    break;
  default: // recover from bug (old snapshot...)
    TRACE_LOG("%s Blitter.BlittingPhase %d\n","ERROR",Blitter.BlittingPhase);
    Blitter.rBusy=Blitter.BlittingPhase=Blitter.Busy=0;
  }//sw
}


void Blitter_ReadSource(MEM_ADDRESS SrcAdr) {
  WORD x=Blitter_DPeek(SrcAdr);
  if(Blitter.SrcXInc>=0) 
  {
    Blitter.SrcBuffer.d32<<=16; //shift former value to the left
    Blitter.SrcBuffer.d16[LO]=x; //load new value on the right
  }
  else 
  {
    Blitter.SrcBuffer.d32>>=16; //shift former value to the right
    Blitter.SrcBuffer.d16[HI]=x; //load new value on the left
  }
}


WORD Blitter_DPeek(MEM_ADDRESS ad) {
  abus=ad&0xfffffe; // 23bit address
  dbus=0xffff; // default
  TRY_M68K_EXCEPTION
    if(abus<MEM_4MB)
    {
      if(Mmu.Confused)
#if !defined(SSE_DEBUGGER)
        dbus=mmu_confused_dpeek(abus);
#else
        dbus=mmu_confused_dpeek(abus,true);
#endif
      else if(abus>=MEM_START_OF_USER_AREA||(SUPERFLAG))
      {
        if(abus<himem)
        {
          dbus=DPEEK(abus);
          DEBUG_CHECK_READ_W(abus);
        }
        else switch(OPTION_VLE) { // reflect MMU/Shifter bus
        case 1: // OPTION_C2
          Mmu.UpdateVideoCounter(LINECYCLES);
          if(Mmu.VideoCounter<himem)
            dbus=DPEEK(Mmu.VideoCounter);
          break;
#if defined(SSE_VID_STVL1)
        case 2: // OPTION_C3
          dbus=Stvl.rambus.d16;
          break;
#endif
        }
      }
      else
        exception(EXCEPTION_BUS_ERROR,EA_READ,abus);
    }
    // IO
    else if(abus>=MEM_IO_BASE) // FFXXXX
      dbus=io_read(abus);
    // TOS
    else if(abus>=rom_addr && abus<rom_addr_end) 
    {
      dbus=ROM_DPEEK(abus-rom_addr);
      DEBUG_CHECK_READ_W(abus);
    }
    // CART
    else if(abus>=Glue.cartbase && abus<Glue.cartend)
    {
      if(cart)
      {
        MEM_ADDRESS cartbus=abus-Glue.cartbase;
        if(Glue.gamecart && cartbus>256*1024)
          cartbus-=(256*1024-64*1024);
        dbus=CART_DPEEK(cartbus);
        DEBUG_CHECK_READ_W(abus);
      }
    }
    else if(abus<himem // 14MB hack
#if defined(SSE_MMU_MONSTER_ALT_RAM)
      || abus<Mmu.MonSTerHimem
#endif
      )
    {
      dbus=DPEEK(abus);
      DEBUG_CHECK_READ_W(abus);
    }
    else if(IS_STE && abus>=rom_addr&&abus<0xEC0000) // beyond actual TOS
    {}
    else if(IS_STE && abus>=0xD00000&&abus<0xD80000)
    {}
    else if(IS_STE && abus>=0xFE0000&&abus<0xFE2000)
    {}
    else
      exception(EXCEPTION_BUS_ERROR,EA_READ,abus);
  CATCH_M68K_EXCEPTION // no crash, just delay, blitter takes BERR as DTACK
    int ncycles=(64+4+2);
    for(int i=0;i<ncycles;i+=2)
    {
      BUS_WAIT_STATES(2); // just in case, avoid too long CPU timings
    }
  END_M68K_EXCEPTION
#if defined(SSE_DONGLE_CUBASE2)
  if(SSEConfig.Cubase2Cart)
    CartridgeCheck(abus,CartridgeData); // not really necessary
#endif
  return dbus;
}


void Blitter_DPoke() {
  abus&=0xfffffe; // 23bit address but can write only to RAM or IO
#if defined(SSE_DONGLE_CUBASE2)
  if(SSEConfig.Cubase2Cart)
    CartridgeCheck(abus,CartridgeData); // not really necessary
#endif
  TRY_M68K_EXCEPTION
  if(abus<himem) 
  {
    if(abus>=MEM_FIRST_WRITEABLE)
    {
#if defined(SSE_VID_CHECK_VIDEO_RAM)
      // If the program is racing the shifter, we must update video before the write
      // Appendix last screen (glitches are correct emulation)
      SHORT linecycles=LINECYCLES;
      if(SSEOptions.TrackVC && !OPTION_C3 && Glue.bFetchingLine
        && abus>=shifter_draw_pointer && abus<VCountAtHSync+(linecycles>>1))
        Shifter.Render(linecycles,TShifter::DISPATCHER_CPU);
#endif
      DPEEK(abus)=dbus;
      DEBUG_CHECK_WRITE_W(abus);
    }
    else
      exception(EXCEPTION_BUS_ERROR,EA_WRITE,abus);
  }
  else if(abus>=MEM_IO_BASE) 
    io_write(abus,udbus);
#if defined(SSE_MMU_MONSTER_ALT_RAM)
  else if(abus<Mmu.MonSTerHimem)
  {
    DEBUG_CHECK_WRITE_W(abus);
    DPEEK(abus)=dbus;
  }
#endif
  CATCH_M68K_EXCEPTION
    int ncycles=(64+4+2);
    for(int i=0;i<ncycles;i+=2)
    {
      BUS_WAIT_STATES(2);
    }
  END_M68K_EXCEPTION
}


void TBlitter::check_blitter_start() {
  if(Request)
    Blitter_CheckRequest();
}

#undef LOGSECTION
