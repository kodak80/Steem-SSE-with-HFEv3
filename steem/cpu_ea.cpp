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
FILE: cpu_ea.cpp
DESCRIPTION: Functions for MC68000 Effective Address (EA), Read/Write (RW)
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <debug.h>
#include <computer.h>
#if defined(SSE_VID_STVL1)
#include <interface_stvl.h>
#endif


// pointers to functions
BYTE (*m68k_peek)(MEM_ADDRESS ad);
WORD (*m68k_dpeek)(MEM_ADDRESS ad);
WORD (*m68k_fetch)(MEM_ADDRESS ad);
void (*m68k_poke_abus)(BYTE x);
void (*m68k_dpoke_abus)(WORD x);
void (*m68k_jsr_get_source_b[8])();
void (*m68k_jsr_get_source_w[8])();
void (*m68k_jsr_get_source_l[8])();
void (*m68k_jsr_get_source_b_not_a[8])();
void (*m68k_jsr_get_source_w_not_a[8])();
void (*m68k_jsr_get_source_l_not_a[8])();
void (*m68k_jsr_get_dest_b[8])();
void (*m68k_jsr_get_dest_w[8])();
void (*m68k_jsr_get_dest_l[8])();
void (*m68k_jsr_get_dest_b_not_a[8])();
void (*m68k_jsr_get_dest_w_not_a[8])();
void (*m68k_jsr_get_dest_l_not_a[8])();
void (*m68k_jsr_get_dest_b_not_a_or_d[8])();
void (*m68k_jsr_get_dest_w_not_a_or_d[8])();
void (*m68k_jsr_get_dest_l_not_a_or_d[8])();


///////////////////////
// PEEK, POKE, FETCH //
///////////////////////

/*  Peek/poke functions should be used only for CPU emulation, use the Safe
    versions for other needs.
    STF and STE have different ROM addresses for both the TOS and, potentially,
    the cartridge.
    ad can be odd.
    Using a look-up table for the high byte of the 24bit address
*/

void m68k_poke(MEM_ADDRESS ad,BYTE x) {
  iabus=ad;
  m68k_poke_abus(x);
}


void m68k_dpoke(MEM_ADDRESS ad,WORD x) {
  iabus=ad;
  m68k_dpoke_abus(x);
}


#define LOGSECTION LOGSECTION_CARTRIDGE

BYTE d8; // meta 8bit register for peek (can be dbush or dbusl)

BYTE m68k_peek_st(MEM_ADDRESS const ad) {
  abus=ad&0xfffffe; // without bit 0
  MEM_ADDRESS bit0=ad&1;
#if defined(BIG_ENDIAN_PROCESSOR)
  MEM_ADDRESS byte_index=bit0;
#else
  MEM_ADDRESS byte_index=bit0^1;
#endif
  MEM_ADDRESS fake_abus=abus+bit0;
  d8=0xFF; // default
#if defined(SSE_DONGLE_CUBASE2)
  bool DoCheckCub2=!!SSEConfig.Cubase2Cart && !bit0; // UDS only
#endif
  BYTE b=(BYTE)(abus>>16); // abus is MEM_ADDRESS type
  switch(Glue.Decode[b]) { // assume switch more efficient than if else ladder
  case TGlue::MMU_CONFUSED:
#if !defined(SSE_DEBUGGER)
    d8=mmu_confused_peek(fake_abus);
#else
    d8=mmu_confused_peek(fake_abus,true);
#endif
    break;
  case TGlue::STRAM_OR_ROM:
    if(abus>=MEM_START_OF_USER_AREA||(SUPERFLAG))
    {
      d8=PEEK(fake_abus); // also 0-7 which is ROM, been copied, faster that way
      DEBUG_CHECK_READ_B(fake_abus);
    }
    else
      exception(EXCEPTION_BUS_ERROR,EA_READ,abus);
    break;
  case TGlue::DEV:
  {
    DU16 io_word(io_read(abus));
    d8=io_word.d8[byte_index];
    break;
  }
  case TGlue::ROM:
  {
    MEM_ADDRESS rombus=fake_abus-rom_addr;
    if(rombus<tos_len)
    {
      d8=ROM_PEEK(rombus);
      DEBUG_CHECK_READ_B(fake_abus);
    }
    break;
  }
  case TGlue::CART:
  {
    MEM_ADDRESS cartbus=fake_abus-Glue.cartbase;
#if defined(SSE_SOUND_CARTRIDGE)
/*  See m68k_dpeek().
    B.A.T I and II and Music Master use MOVE.W.
    Drumbeat for the Replay 16 cartridge uses MOVE.B.
*/
    if(SSEConfig.mv16)
    {
      mv16_fetch((WORD)cartbus);
#if defined(SSE_420R5) // argh!
      d8=CART_PEEK(cartbus);
#endif
    }
#endif
#if defined(SSE_DONGLE_CUBASE2)
    else if(DoCheckCub2)
    {
      dbus=CartridgeCheck(abus,CartridgeData);
      DoCheckCub2=false;
      d8=bit0?0:dbus>>8;
    }
#endif
#if defined(SSE_DONGLE_CUBASE3)
    else if(SSEConfig.Cubase3Cart)
    {
      dbus=CartridgeCheck(abus,CartridgeData);
      d8=bit0?0:dbus>>8;
    }
    else
#endif
      d8=CART_PEEK(cartbus);
    DEBUG_CHECK_READ_B(fake_abus);
    TRACE_LOG("%06X: PEEK (%06X)=%02X\n",old_pc,fake_abus,d8);
    break;
  }
  case TGlue::CART2:
  {
    MEM_ADDRESS cartbus=fake_abus-Glue.cartbase;
    if(Glue.gamecart && cartbus>256*1024)
      cartbus-=(256*1024-64*1024);
    d8=CART_PEEK(cartbus);
    DEBUG_CHECK_READ_B(fake_abus);
    TRACE_LOG("%06X: PEEK (%06X)=%02X\n",old_pc,fake_abus,d8);
    break;
  }
  case TGlue::CART3:
    DEBUG_CHECK_READ_B(fake_abus); //?
    TRACE_LOG("%06X: PEEK (%06X)=%04X\n",old_pc,fake_abus,d8);
    break;
  case TGlue::STRAM:
  case TGlue::STRAM_C2:
    d8=PEEK(fake_abus); // faster than light!
    DEBUG_CHECK_READ_B(fake_abus);
    break;
  case TGlue::STRAM_CHECK:
  case TGlue::STRAM_CHECK_C2:
    if(abus<himem)
    {
      d8=PEEK(fake_abus);
      DEBUG_CHECK_READ_B(fake_abus);
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
    default:
      break;
    }//sw
    break;
  case TGlue::ALTRAM:
    if(abus<Mmu.MonSTerHimem)
    {
      d8=PEEK(fake_abus);
      DEBUG_CHECK_READ_B(fake_abus);
    }
    break;
  case TGlue::ROM_CHECK:
    if(abus<0xFE2000)
      break; // no bus error
  case TGlue::BERR:
    exception(EXCEPTION_BUS_ERROR,EA_READ,abus);
    break;
  default:
    break;
  }//sw
  udbus.d8[byte_index]=d8;
#if defined(SSE_DONGLE_CUBASE2)
  if(DoCheckCub2)
    CartridgeCheck(abus,CartridgeData);
#endif
#if defined(SSE_STATS_EXT)
  Stats.BusRead++;
#endif
  return d8;
}


WORD m68k_dpeek_st(MEM_ADDRESS const ad) {
  abus=ad&0xfffffe;
  dbus=0xffff; // default
  if(ad&1)
    exception(EXCEPTION_ADDRESS_ERROR,EA_READ,abus);
#if defined(SSE_DONGLE_CUBASE2)
  bool DoCheckCub2=!!SSEConfig.Cubase2Cart;
#endif
  BYTE b=(BYTE)(abus>>16);
  switch(Glue.Decode[b]) {
  case TGlue::MMU_CONFUSED:
#if !defined(SSE_DEBUGGER)
    dbus=mmu_confused_dpeek(abus);
#else
    dbus=mmu_confused_dpeek(abus,true);
#endif
    break;
  case TGlue::STRAM_OR_ROM:
    if(abus>=MEM_START_OF_USER_AREA||(SUPERFLAG))
    {
      dbus=DPEEK(abus);
      DEBUG_CHECK_READ_W(abus);
    }
    else
      exception(EXCEPTION_BUS_ERROR,EA_READ,abus);
    break;
  case TGlue::DEV:
    dbus=io_read(abus);
    break;
  case TGlue::ROM:
  {
    MEM_ADDRESS rombus=abus-rom_addr;
    if(rombus<tos_len)
      dbus=ROM_DPEEK(rombus);
    DEBUG_CHECK_READ_W(abus);
    break;
  }
  case TGlue::CART:
  {
    MEM_ADDRESS cartbus=abus-Glue.cartbase;
#if defined(SSE_SOUND_CARTRIDGE)
    /*  The MV16 cartridge was designed for the game B.A.T.
        It plays samples sent through reading an address on the
        cartridge (address=data).
    */
    if(SSEConfig.mv16)
    {
      mv16_fetch((WORD)cartbus);
#if defined(SSE_420R5) //argh!
      dbus=CART_DPEEK(cartbus);
#endif
    }
#endif
#if defined(SSE_DONGLE_CUBASE2)
    else if(DoCheckCub2)
    {
      dbus=CartridgeCheck(abus,CartridgeData);
      DoCheckCub2=false;
    }
#endif
#if defined(SSE_DONGLE_CUBASE3)
    else if(SSEConfig.Cubase3Cart)
      dbus=CartridgeCheck(abus,CartridgeData);
    else
#endif
      dbus=CART_DPEEK(cartbus);
    DEBUG_CHECK_READ_W(abus);
    TRACE_LOG("%06X: DPEEK (%06X)=%04X\n",old_pc,abus,dbus);
    break;
  }
  case TGlue::CART2:
  {
    MEM_ADDRESS cartbus=abus-Glue.cartbase;
    if(Glue.gamecart && cartbus>256*1024)
      cartbus-=(256*1024-64*1024);
    dbus=CART_DPEEK(cartbus);
    DEBUG_CHECK_READ_W(abus);
    TRACE_LOG("%06X: DPEEK (%06X)=%04X\n",old_pc,abus,dbus);
    break;
  }
  case TGlue::CART3:
    DEBUG_CHECK_READ_W(abus); //?
    TRACE_LOG("%06X: DPEEK (%06X)=%04X\n",old_pc,abus,dbus);
    break;
  case TGlue::STRAM:
  case TGlue::STRAM_C2:
    dbus=DPEEK(abus);
    DEBUG_CHECK_READ_W(abus);
    break;
  case TGlue::STRAM_CHECK:
  case TGlue::STRAM_CHECK_C2:
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
    default:
      break;
    }//sw
    break;
  case TGlue::ALTRAM:
    if(abus<Mmu.MonSTerHimem)
    {
      dbus=DPEEK(abus);
      DEBUG_CHECK_READ_W(abus);
    }
    break;    
  case TGlue::ROM_CHECK:
    if(abus<0xFE2000)
      break; // no bus error
  case TGlue::BERR:
    exception(EXCEPTION_BUS_ERROR,EA_READ,abus);
    break;
  default:
    break;
  }//sw
#if defined(SSE_DONGLE_CUBASE2)
  if(DoCheckCub2)
    CartridgeCheck(abus,CartridgeData);
#endif
#if defined(SSE_STATS_EXT)
  Stats.BusRead++;
#endif
  return dbus;
}


// Safe versions, no crash, not used for direct emulation but for
// utilities

#ifdef VC_BUILD
#pragma warning (disable : 4459) // declaration of 'abus' hides global declaration etc.
#endif

BYTE SafePeek(MEM_ADDRESS const ad) {
  BYTE old_stem_runmode=stem_runmode;
  stem_runmode=STEM_MODE_INSPECT;
  MEM_ADDRESS MyAbus=ad&0xfffffe; // without bit 0
  MEM_ADDRESS bit0=ad&1;
#if defined(BIG_ENDIAN_PROCESSOR)
  MEM_ADDRESS byte_index=bit0;
#else
  MEM_ADDRESS byte_index=bit0^1;
#endif
  MEM_ADDRESS fake_abus=MyAbus+bit0;
  BYTE d8=0xFF; // default
  BYTE b=(BYTE)(MyAbus>>16); // abus is MEM_ADDRESS type
  switch(Glue.Decode[b]) { // assume switch more efficient than if else ladder
  case TGlue::MMU_CONFUSED:
#if !defined(SSE_DEBUGGER)
    d8=mmu_confused_peek(fake_abus);
#else
    d8=mmu_confused_peek(fake_abus,true);
#endif
    break;
  case TGlue::STRAM_OR_ROM:
    d8=PEEK(fake_abus); // also 0-7 which is ROM, been copied, faster that way
    break;
  case TGlue::DEV:
  {
    DU16 io_word(io_read(MyAbus));
    d8=io_word.d8[byte_index];
    break;
  }
  case TGlue::ROM:
  {
    MEM_ADDRESS rombus=fake_abus-rom_addr;
    if(rombus<tos_len)
      d8=ROM_PEEK(rombus);
    break;
  }
  case TGlue::CART:
  {
    MEM_ADDRESS cartbus=fake_abus-Glue.cartbase;
    d8=CART_PEEK(cartbus);
    break;
  }
  case TGlue::CART2:
  {
    MEM_ADDRESS cartbus=fake_abus-Glue.cartbase;
    if(Glue.gamecart && cartbus>256*1024)
      cartbus-=(256*1024-64*1024);
    d8=CART_PEEK(cartbus);
    break;
  }
  case TGlue::CART3:
    break;
  case TGlue::STRAM:
  case TGlue::STRAM_C2:
    d8=PEEK(fake_abus); // faster than light!
    break;
  case TGlue::STRAM_CHECK:
  case TGlue::STRAM_CHECK_C2:
    if(MyAbus<himem)
      d8=PEEK(fake_abus);
    break;
  case TGlue::ALTRAM:
    if(MyAbus<Mmu.MonSTerHimem)
      d8=PEEK(fake_abus);
    break;
  case TGlue::ROM_CHECK:
    break;
  case TGlue::BERR:
    break;
  default:
    break;
  }//sw
  stem_runmode=old_stem_runmode;
  return d8;
}


WORD SafeDPeek(MEM_ADDRESS const ad) {
  BYTE old_stem_runmode=stem_runmode;
  stem_runmode=STEM_MODE_INSPECT;
  MEM_ADDRESS MyAbus=ad&0xfffffe;
  WORD dbus=0xffff; // default
  if(ad&1)
    return dbus;
  BYTE b=(BYTE)(MyAbus>>16);
  switch(Glue.Decode[b]) {
  case TGlue::MMU_CONFUSED:
#if !defined(SSE_DEBUGGER)
    dbus=mmu_confused_dpeek(MyAbus);
#else
    dbus=mmu_confused_dpeek(MyAbus,true);
#endif
    break;
  case TGlue::STRAM_OR_ROM:
    dbus=DPEEK(MyAbus);
    break;
  case TGlue::DEV:
    dbus=io_read(MyAbus);
    break;
  case TGlue::ROM:
  {
    MEM_ADDRESS rombus=MyAbus-rom_addr;
    if(rombus<tos_len)
      dbus=ROM_DPEEK(rombus);
    break;
  }
  case TGlue::CART:
  {
    MEM_ADDRESS cartbus=MyAbus-Glue.cartbase;
    dbus=CART_DPEEK(cartbus);
    break;
  }
  case TGlue::CART2:
  {
    MEM_ADDRESS cartbus=MyAbus-Glue.cartbase;
    if(Glue.gamecart && cartbus>256*1024)
      cartbus-=(256*1024-64*1024);
    dbus=CART_DPEEK(cartbus);
    break;
  }
  case TGlue::CART3:
    break;
  case TGlue::STRAM:
  case TGlue::STRAM_C2:
    dbus=DPEEK(MyAbus);
    break;
  case TGlue::STRAM_CHECK:
  case TGlue::STRAM_CHECK_C2:
    if(MyAbus<himem)
      dbus=DPEEK(MyAbus);
    break;
  case TGlue::ALTRAM:
    if(MyAbus<Mmu.MonSTerHimem)
      dbus=DPEEK(MyAbus);
    break;    
  case TGlue::ROM_CHECK:
    break;
  case TGlue::BERR:
    break;
  default:
    break;
  }//sw
  stem_runmode=old_stem_runmode;
  return dbus;
}

#ifdef VC_BUILD
#pragma warning (default : 4459)
#endif


DWORD m68k_lpeek(MEM_ADDRESS const ad) {
  DWORD l=m68k_dpeek(ad)<<16;
  l|=m68k_dpeek(ad+2);
  return l;
}


WORD m68k_fetch_st(MEM_ADDRESS const ad) { // almost like dpeek
  dbus=0xffff; // default
  if(ad&1)
    exception(EXCEPTION_ADDRESS_ERROR,EA_FETCH,ad); // different function code and stack
  BYTE b=(BYTE)(ad>>16);
  switch(Glue.Decode[b]) {
  case TGlue::MMU_CONFUSED:
#if !defined(SSE_DEBUGGER)
    dbus=mmu_confused_dpeek(abus);
#else
    dbus=mmu_confused_dpeek(abus,true);
#endif
    break;
  case TGlue::STRAM_OR_ROM:
    if(abus>=MEM_START_OF_USER_AREA||(SUPERFLAG))
      dbus=DPEEK(abus);
    else
      exception(EXCEPTION_BUS_ERROR,EA_FETCH,ad);
    break;
  case TGlue::DEV:
    dbus=io_read(abus);
    break;
  case TGlue::ROM:
  {
    MEM_ADDRESS rombus=abus-rom_addr;
    if(rombus<tos_len)
      dbus=ROM_DPEEK(rombus);
    break;
  }
  case TGlue::CART:
  {
    MEM_ADDRESS cartbus=abus-Glue.cartbase;
    dbus=CART_DPEEK(cartbus);
    TRACE_LOG("%06X: FETCH (%06X)=%04X\n",old_pc,abus,dbus);
    break;
  }
  case TGlue::CART2:
  {
    MEM_ADDRESS cartbus=abus-Glue.cartbase;
    if(Glue.gamecart && cartbus>256*1024)
      cartbus-=(256*1024-64*1024);
    dbus=CART_DPEEK(cartbus);
    DEBUG_CHECK_READ_W(abus);
    TRACE_LOG("%06X: FETCH (%06X)=%04X\n",old_pc,abus,dbus);
    break;
  }
  case TGlue::CART3:
    DEBUG_CHECK_READ_W(abus); //?
    TRACE_LOG("%06X: FETCH (%06X)=%04X\n",old_pc,abus,dbus);
    break;
  case TGlue::STRAM:
  case TGlue::STRAM_C2:
    dbus=DPEEK(abus);
    DEBUG_CHECK_READ_W(abus);
    break;
  case TGlue::STRAM_CHECK:
  case TGlue::STRAM_CHECK_C2:
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
    default:
      break;
    }
    break;
  case TGlue::ALTRAM:
    dbus=DPEEK(abus);
    DEBUG_CHECK_READ_W(abus);
    break;    
  case TGlue::ROM_CHECK:
    if(abus<0xFE2000)
      break; // no bus error
  case TGlue::BERR:
    exception(EXCEPTION_BUS_ERROR,EA_FETCH,ad);
    break;
  default:
    break;
  }//sw
#if defined(SSE_DONGLE_CUBASE2)
  if(SSEConfig.Cubase2Cart) // we don't test fetching from cartridge
    CartridgeCheck(abus,CartridgeData);
#endif
#if defined(SSE_STATS_EXT)
  Stats.BusRead++;
#endif
  return dbus;
}


// 0 because m68k_poke_abus is a function pointer, because of the silly
// Fast Blit option
void m68k_poke_abus0(BYTE const x) {
  MEM_ADDRESS fake_abus=iabus&0xffffff;
  abus=iabus&0xfffffe;
#if defined(SSE_DONGLE_CUBASE2)
  if(SSEConfig.Cubase2Cart && !(fake_abus&1)) // UDS only
    CartridgeCheck(abus,CartridgeData);
#endif
  BYTE b=(BYTE)(abus>>16);
  switch(Glue.Decode[b]) {
  case TGlue::MMU_CONFUSED:
    mmu_confused_poke_abus(x); 
    break;
  case TGlue::DEV:
    io_write(abus,udbus);
    break;
  case TGlue::STRAM_CHECK_C2:
    if(abus>=himem)
      break;
  case TGlue::STRAM_C2:
  {
#if defined(SSE_VID_CHECK_VIDEO_RAM)
    // If the program is racing the shifter, we must update video before the write
    SHORT linecycles=LINECYCLES;
    if(Glue.bFetchingLine && abus>=shifter_draw_pointer && abus<VCountAtHSync+(linecycles>>1))
      Shifter.Render(linecycles,TShifter::DISPATCHER_CPU);
#endif
    PEEK(fake_abus)=x;
    DEBUG_CHECK_WRITE_B(fake_abus);
    break;
  }
  case TGlue::STRAM_CHECK:
    if(abus>=himem)
      break;
  case TGlue::STRAM:
    PEEK(fake_abus)=x;
    DEBUG_CHECK_WRITE_B(fake_abus);
    break;
  case TGlue::STRAM_OR_ROM:
    if(abus>=MEM_START_OF_USER_AREA || SUPERFLAG&&abus>=MEM_FIRST_WRITEABLE) // at least we have this aggravation only for the first 64K
    {
      PEEK(fake_abus)=x;
      DEBUG_CHECK_WRITE_B(fake_abus);
    }
    else
      exception(EXCEPTION_BUS_ERROR,EA_WRITE,abus);
    break;
  case TGlue::ALTRAM:
    PEEK(fake_abus)=x;
    DEBUG_CHECK_WRITE_B(fake_abus);
    break;
  case TGlue::ROM:
  case TGlue::ROM1:
  case TGlue::ROM_CHECK:
  case TGlue::CART:
  case TGlue::CART2:
  case TGlue::CART3:
  case TGlue::BERR:
    exception(EXCEPTION_BUS_ERROR,EA_WRITE,abus);
    break;
  default:
    break;
  }//sw
#if defined(SSE_STATS_EXT)
  Stats.BusWrite++;
#endif
}


void m68k_dpoke_abus0(WORD const x) {
  abus=iabus&0xfffffe;
  if(iabus&1)
    exception(EXCEPTION_ADDRESS_ERROR,EA_WRITE,abus);
#if defined(SSE_DONGLE_CUBASE2)
  if(SSEConfig.Cubase2Cart)
    CartridgeCheck(abus,CartridgeData);
#endif
  BYTE b=(BYTE)(abus>>16);
  switch(Glue.Decode[b]) {
  case TGlue::MMU_CONFUSED:
    mmu_confused_dpoke_abus(x);
    break;
  case TGlue::DEV:
    io_write(abus,udbus);
    break;
  case TGlue::STRAM_CHECK_C2:
    if(abus>=himem)
      break;
  case TGlue::STRAM_C2:
  {
#if defined(SSE_VID_CHECK_VIDEO_RAM)
    // If the program is racing the shifter, we must update video before the write
    // 3615GEN4 ULM (without this, there's a horrible split)
    SHORT linecycles=LINECYCLES;
    if(Glue.bFetchingLine && abus>=shifter_draw_pointer && abus<VCountAtHSync+(linecycles>>1))
      Shifter.Render(linecycles,TShifter::DISPATCHER_CPU);
#endif
    DPEEK(abus)=x;
    DEBUG_CHECK_WRITE_W(abus);
    break;
  }
  case TGlue::STRAM_CHECK:
    if(abus>=himem)
      break;
  case TGlue::STRAM:
    DPEEK(abus)=x;
    DEBUG_CHECK_WRITE_W(abus);
    break;
  case TGlue::STRAM_OR_ROM:
    if(abus>=MEM_START_OF_USER_AREA || SUPERFLAG&&abus>=MEM_FIRST_WRITEABLE)
    {
      DPEEK(abus)=x;
      DEBUG_CHECK_WRITE_W(abus);
    }
    else
      exception(EXCEPTION_BUS_ERROR,EA_WRITE,abus);
    break;
  case TGlue::ALTRAM:
    DPEEK(abus)=x;
    DEBUG_CHECK_WRITE_W(abus);
    break;
  case TGlue::ROM:
  case TGlue::ROM1:
  case TGlue::ROM_CHECK:
  case TGlue::CART:
  case TGlue::CART2:
  case TGlue::CART3:
  case TGlue::BERR:
    exception(EXCEPTION_BUS_ERROR,EA_WRITE,abus);
    break;
  default:
    break;
  }//sw
#if defined(SSE_STATS_EXT)
  Stats.BusWrite++;
#endif
}


#if defined(SSE_OPTION_FASTLINEA)
// this is nothing serious, funny hack, but even GEM doesn't use line A much

BYTE m68k_peek_st_lineA(MEM_ADDRESS const ad) {
  BYTE x=m68k_peek_st(ad);
  sys_cycles+=4*TICKS8;
#if defined(SSE_STATS)
  Stats.nFastBlit++;
#endif
#if defined(SSE_STATS_EXT)
  Stats.BusRead++;
#endif
  return x;
}


WORD m68k_dpeek_st_lineA(MEM_ADDRESS const ad) {
  WORD x=m68k_dpeek_st(ad);
  sys_cycles+=4*TICKS8;
#if defined(SSE_STATS)
  Stats.nFastBlit++;
#endif
  return x;
}


WORD m68k_fetch_st_lineA(MEM_ADDRESS const ad) {
  WORD x=m68k_fetch_st(ad);
  sys_cycles+=4*TICKS8;
#if defined(SSE_STATS)
  Stats.nFastBlit++;
#endif
#if defined(SSE_STATS_EXT)
  Stats.BusRead++;
#endif
  return x;
}


void m68k_poke_abus_lineA(BYTE const x) {
  m68k_poke_abus0(x);
  sys_cycles+=4*TICKS8;
#if defined(SSE_STATS)
  Stats.nFastBlit++;
#endif
#if defined(SSE_STATS_EXT)
  Stats.BusWrite++;
#endif
}


void m68k_dpoke_abus_lineA(WORD const x) {
  m68k_dpoke_abus0(x);
  sys_cycles+=4*TICKS8;
#if defined(SSE_STATS)
  Stats.nFastBlit++;
#endif
#if defined(SSE_STATS_EXT)
  Stats.BusWrite++;
#endif
}

#endif


/*

*******************************************************************************
                   OPERAND EFFECTIVE ADDRESS CALCULATION TIMES
*******************************************************************************
-------------------------------------------------------------------------------
       <ea>       |    Exec Time    |               Data Bus Usage
------------------+-----------------+------------------------------------------
.B or .W :        |                 |
  Dn              |          0(0/0) |
  An              |          0(0/0) |
  (An)            |          4(1/0) |                              nr           
  (An)+           |          4(1/0) |                              nr           
  -(An)           |          6(1/0) |                   n          nr           
  (d16,An)        |          8(2/0) |                        np    nr           
  (d8,An,Xn)      |         10(2/0) |                   n    np    nr           
  (xxx).W         |          8(2/0) |                        np    nr           
  (xxx).L         |         12(3/0) |                     np np    nr           
  #<data>         |          4(1/0) |                        np                 
.L :              |                 |
  Dn              |          0(0/0) |
  An              |          0(0/0) |
  (An)            |          8(2/0) |                           nR nr           
  (An)+           |          8(2/0) |                           nR nr           
  -(An)           |         10(2/0) |                   n       nR nr           
  (d16,An)        |         12(3/0) |                        np nR nr           
  (d8,An,Xn)      |         14(3/0) |                   n    np nR nr           
  (xxx).W         |         12(3/0) |                        np nR nr           
  (xxx).L         |         16(4/0) |                     np np nR nr           
  #<data>         |          8(2/0) |                     np np                 

*/

/*  To avoid using function pointers for EA, we could also create
    instructions for all EA but it gets heavy!
*/

void m68kGetDestByte() {
  m68k_jsr_get_dest_b[(IRD&BITS_543)>>3]();
}


void m68kGetDestWord() {
  m68k_jsr_get_dest_w[(IRD&BITS_543)>>3]();
}


void m68kGetDestLong() {
  m68k_jsr_get_dest_l[(IRD&BITS_543)>>3]();
}


void m68kGetDestByteNotA() {
  m68k_jsr_get_dest_b_not_a[(IRD&BITS_543)>>3]();
}


void m68kGetDestWordNotA() {
  m68k_jsr_get_dest_w_not_a[(IRD&BITS_543)>>3]();
}


void m68kGetDestLongNotA() {
  m68k_jsr_get_dest_l_not_a[(IRD&BITS_543)>>3]();
}


void m68kGetDestByteNotAOrD() {
  m68k_jsr_get_dest_b_not_a_or_d[(IRD&BITS_543)>>3]();
}


void m68kGetDestWordNotAOrD() {
  m68k_jsr_get_dest_w_not_a_or_d[(IRD&BITS_543)>>3]();
}


void m68kGetDestLongNotAOrD() {
  m68k_jsr_get_dest_l_not_a_or_d[(IRD&BITS_543)>>3]();
}


void m68kGetSourceByte() {
  m68k_jsr_get_source_b[(IRD&BITS_543)>>3]();
}


void m68kGetSourceWord() {
  m68k_jsr_get_source_w[(IRD&BITS_543)>>3]();
}


void m68kGetSourceLong() {
  m68k_jsr_get_source_l[(IRD&BITS_543)>>3]();
}


void m68kGetSourceByteNotA() {
  m68k_jsr_get_source_b_not_a[(IRD&BITS_543)>>3]();
}


void m68kGetSourceWordNotA() {
  m68k_jsr_get_source_w_not_a[(IRD&BITS_543)>>3]();
}


void m68kGetSourceLongNotA() {
  m68k_jsr_get_source_l_not_a[(IRD&BITS_543)>>3]();
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//////////////////////////    GET SOURCE     ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/*
Explanation for TRUE_PC, it's based on microcodes and confirmed by tests, 
so there's no contest!
It's useful in case of crash during the read.

b543	b210         Mode           .B, .W             .L

000	R            Dn              PC                PC
001	R            An              PC                PC
010	R            (An)            PC                PC
011	R            (An)+           PC                PC
100	R            -(An)           PC+2              PC
101	R            (d16, An)       PC                PC
110	R            (d8, An, Xn)    PC                PC
111	000          (xxx).W         PC+2              PC+2
111	001          (xxx).L         PC+4              PC+4
111	010          (d16, PC)       PC                PC
111	011          (d8, PC, Xn)    PC                PC
111	100          #<data>         PC+2              PC+4


Dn		          000	R	  0(0/0)	0(0/0)	Data Register Direct
An		          001	R	  0(0/0)	0(0/0)	Address Register Direct
(An)		        010	R	  4(1/0)	8(2/0)	Address Register Indirect
(An)+		        011	R	  4(1/0)	8(2/0)	Address Register Indirect with Postincrement
-(An)		        100	R	  6(1/0)	10(2/0)	Address Register Indirect with Predecrement
(d16, An)	      101	R	  8(2/0)	12(3/0)	Address Register Indirect with Displacement
(d8, An, Xn)*		110	R	  10(2/0)	14(3/0)	Address Register Indirect with Index
(xxx).W		      111	000	8(2/0)	12(3/0)	Absolute Short
(xxx).L		      111	001	12(3/0)	16(4/0)	Absolute Long
(d16, PC)		    111	010	8(2/0)	12(3/0)	Program Counter Indirect with Displacement
(d8, PC, Xn)*		111	011	10(2/0)	14(3/0)	Program Counter Indirect with Index - 8-Bit Displacement
#<data>		      111	100	4(1/0)	8(2/0)	Immediate
*/

// Dn

void m68k_get_source_000_b() { // .B Dn
  //  Dn              |          0(0/0) |
  m68k_src_b=(BYTE)(Cpu.r[PARAM_M]);
}


void m68k_get_source_000_w() { //.W Dn
  //  Dn              |          0(0/0) |
  m68k_src_w=(WORD)(Cpu.r[PARAM_M]);
}


void m68k_get_source_000_l() { //.L Dn
  //  Dn              |          0(0/0) |
  m68k_src_l=(LONG)(Cpu.r[PARAM_M]);
}


// An

void m68k_get_source_001_b() { // .B An
  //  An              |          0(0/0) |
  m68k_src_b=(BYTE)(AREG(PARAM_M));
}


void m68k_get_source_001_w() { // .W An
  //  An              |          0(0/0) |
  m68k_src_w=(WORD)(AREG(PARAM_M));
}


void m68k_get_source_001_l() { // .L An
  //  An              |          0(0/0) |
  m68k_src_l=(LONG)(AREG(PARAM_M));
}


// (An)

void m68k_get_source_010_b() { // .B (An)
  //  (An)            |          4(1/0) |                              nr       
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ_B; //nr
  m68k_src_b=d8;
}


void m68k_get_source_010_w() { // .W (An)
  //  (An)            |          4(1/0) |                              nr       
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ; //nr
  m68k_src_w=dbus;
}


void m68k_get_source_010_l() { // .L (An)
  //  (An)            |          8(2/0) |                           nR nr         
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ; //nR
  m68k_src_lh=dbus;
  iabus+=2;
  CPU_BUS_ACCESS_READ; //nr
  m68k_src_ll=dbus;
}


// (An)+

void m68k_get_source_011_b() { // .B (An)+
  //  (An)+           |          4(1/0) |                              nr           
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ_B; //nr
  m68k_src_b=d8;
  AREG(PARAM_M)++;
  if(PARAM_M==7)
    AREG(PARAM_M)++;
}


void m68k_get_source_011_w() { // .W (An)+
  //  (An)+           |          4(1/0) |                              nr           
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ; //nr
  m68k_src_w=dbus;
  AREG(PARAM_M)+=2;
}


void m68k_get_source_011_l() { // .L (An)+
  //  (An)+           |          8(2/0) |                           nR nr           
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ; //nR
  m68k_src_lh=dbus;
  iabus+=2;
  CPU_BUS_ACCESS_READ; //nr
  m68k_src_ll=dbus;
  AREG(PARAM_M)+=4; // for .L, we assume ++ post read
}


// -(An)

void m68k_get_source_100_b() { // .B -(An)
  //  -(An)           |          6(1/0) |                   n          nr           
  TRUE_PC+=2;
  CPU_BUS_IDLE(2); //n
  AREG(PARAM_M)--;
  if(PARAM_M==7)
    AREG(PARAM_M)--;
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ_B; //nr
  m68k_src_b=d8;
}


void m68k_get_source_100_b_a7() { // .B -(An)
  //  -(An)           |          6(1/0) |                   n          nr           
  TRUE_PC+=2;
  CPU_BUS_IDLE(2); //n
  AREG(PARAM_M)-=2;
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ_B; //nr
  m68k_src_b=d8;
}


void m68k_get_source_100_w() { // .W -(An)
  //  -(An)           |          6(1/0) |                   n          nr           
  TRUE_PC+=2;
  CPU_BUS_IDLE(2); //n
  AREG(PARAM_M)-=2;
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ; //nr
  m68k_src_w=dbus;
}


void m68k_get_source_100_l() { // .L -(An)
  //  -(An)           |         10(2/0) |                   n       nR nr           
  CPU_BUS_IDLE(2); //n
  AREG(PARAM_M)-=4;
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ; //nR
  m68k_src_lh=dbus;
  iabus+=2;
  CPU_BUS_ACCESS_READ; //nr
  m68k_src_ll=dbus;
}


// (d16, An)

void m68k_get_source_101_b() { //.B (d16, An)
  //  (d16,An)        |          8(2/0) |                        np    nr           
  iabus=AREG(PARAM_M)+(signed short)IRC;
  PREFETCH; //np
  CPU_BUS_ACCESS_READ_B; //nr
  m68k_src_b=d8;
}


void m68k_get_source_101_w() { // .W (d16, An)
  //  (d16,An)        |          8(2/0) |                        np    nr           
  iabus=AREG(PARAM_M)+(signed short)IRC;
  PREFETCH; //np
  CPU_BUS_ACCESS_READ; //nr
  m68k_src_w=dbus;
}


void m68k_get_source_101_l() { // .L (d16, An)
  //  (d16,An)        |         12(3/0) |                        np nR nr           
  iabus=AREG(PARAM_M)+(signed short)IRC;
  PREFETCH; //np
  CPU_BUS_ACCESS_READ; //nR
  m68k_src_lh=dbus;
  iabus+=2;
  CPU_BUS_ACCESS_READ; //nr
  m68k_src_ll=dbus;
}


// (d8,An,Xn)

void m68k_get_source_110_b() { // .B (d8,An,Xn)
  //  (d8,An,Xn)      |         10(2/0) |                   n    np    nr           
  CPU_BUS_IDLE(2); //n
  if(IRC&BIT_b)   //.l
    iabus=AREG(PARAM_M)+(signed char)(IRC)+(int)Cpu.r[IRC>>12];
  else          //.w
    iabus=AREG(PARAM_M)+(signed char)(IRC)+(signed short)Cpu.r[IRC>>12];
  PREFETCH; //np
  CPU_BUS_ACCESS_READ_B; //nr
  m68k_src_b=d8;
}


void m68k_get_source_110_w() { // .W (d8,An,Xn)
  //  (d8,An,Xn)      |         10(2/0) |                   n    np    nr           
  CPU_BUS_IDLE(2); //n
  if(IRC&BIT_b)   //.l
    iabus=AREG(PARAM_M)+(signed char)(IRC)+(int)Cpu.r[IRC>>12];
  else          //.w
    iabus=AREG(PARAM_M)+(signed char)(IRC)+(signed short)Cpu.r[IRC>>12];
  PREFETCH; //np
  CPU_BUS_ACCESS_READ; //nr
  m68k_src_w=dbus;
}


void m68k_get_source_110_l() { // .L (d8,An,Xn)
  //  (d8,An,Xn)      |         14(3/0) |                   n    np nR nr           
  CPU_BUS_IDLE(2); //n
  if(IRC&BIT_b)  //.l
    iabus=AREG(PARAM_M)+(signed char)IRC+(int)Cpu.r[IRC>>12];
  else         //.w
    iabus=AREG(PARAM_M)+(signed char)IRC+(signed short)Cpu.r[IRC>>12];
  PREFETCH; //np
  CPU_BUS_ACCESS_READ; //nR
  m68k_src_lh=dbus;
  iabus+=2;
  CPU_BUS_ACCESS_READ; //nr
  m68k_src_ll=dbus;
}


// (xxx).W, (xxx).L, (d16, PC), (d8, PC, Xn), #<data>

void m68k_get_source_111_b() {
  switch(IRD&0x7) {
  case 0:  // .B (xxx).W
    //  (xxx).W         |          8(2/0) |                        np    nr           
    TRUE_PC+=2;
    iabus=(signed int)(signed short)IRC; // sign extension
    PREFETCH; //np
    CPU_BUS_ACCESS_READ_B; //nr
    m68k_src_b=d8;
    break;
  case 1:  // .B (xxx).L
    //  (xxx).L         |         12(3/0) |                     np np    nr    
    TRUE_PC+=4;
    iabush=IRC;
    PREFETCH; //np
    iabusl=IRC;
    PREFETCH; //np
    CPU_BUS_ACCESS_READ_B; //nr
    m68k_src_b=d8;
    break;
  case 2:  // .B (d16, PC)
    //  (d16,An)        |          8(2/0) |                        np    nr           
    iabus=pc+(signed short)IRC;
    PREFETCH; //np
    CPU_BUS_ACCESS_READ_B; //nr
    m68k_src_b=d8;
    break;
  case 3:  // .B (d8, PC, Xn)
    //  (d8,An,Xn)      |         10(2/0) |                   n    np    nr           
    CPU_BUS_IDLE(2); //n
    m68k_iriwo=IRC;
    if(m68k_iriwo&BIT_b)   //.l
      iabus=pc+(signed char)(m68k_iriwo)+(int)Cpu.r[m68k_iriwo>>12];
    else          //.w
      iabus=pc+(signed char)(m68k_iriwo)+(signed short)Cpu.r[m68k_iriwo>>12];
    PREFETCH; //np
    CPU_BUS_ACCESS_READ_B; //nr
    m68k_src_b=d8;
    break;
  case 4:  // .B #<data>
    //  #<data>         |          4(1/0) |                        np                 
    TRUE_PC+=2; // after prefetch?
    m68k_src_b=(BYTE)IRC;
    PREFETCH; //np
    break;
#ifndef SSE_LEAN_AND_MEAN
  default: // shouldn't happen anymore
    BREAKPOINT(EA illegal);
    m68k_trap1();
#endif
  }//sw
}


void m68k_get_source_111_w() {
  switch(IRD&0x7) {
  case 0:  // .W (xxx).W
    //  (xxx).W         |          8(2/0) |                        np    nr           
    TRUE_PC+=2;
    iabus=(signed short)IRC; // sign extension
    PREFETCH; //np
    CPU_BUS_ACCESS_READ; //nr
    m68k_src_w=dbus;
    break;
  case 1:  // .W (xxx).L
    //  (xxx).L         |         12(3/0) |                     np np    nr    
    TRUE_PC+=4;
    iabush=IRC;
    PREFETCH; //np
    iabusl=IRC;
    PREFETCH; //np
    CPU_BUS_ACCESS_READ; //nr
    m68k_src_w=dbus;
    break;
  case 2:  // .W (d16, PC)
    //  (d16,An)        |          8(2/0) |                        np    nr           
    iabus=pc+(signed short)IRC;
    PREFETCH; //np
    CPU_BUS_ACCESS_READ; //nr
    m68k_src_w=dbus;
    break;
  case 3: // .W (d8, PC, Xn)
    //  (d8,An,Xn)      |         10(2/0) |                   n    np    nr           
    CPU_BUS_IDLE(2); //n
    m68k_iriwo=IRC;
    if(m68k_iriwo&BIT_b)   //.l
      iabus=pc+(signed char)(m68k_iriwo)+(int)Cpu.r[m68k_iriwo>>12];
    else          //.w
      iabus=pc+(signed char)(m68k_iriwo)+(signed short)Cpu.r[m68k_iriwo>>12];
    PREFETCH; //np
    CPU_BUS_ACCESS_READ; //nr
    m68k_src_w=dbus;
    break;
  case 4:  // .W #<data>
    //  #<data>         |          4(1/0) |                        np                 
    TRUE_PC+=2;
    m68k_src_w=IRC;
    PREFETCH; //np    
    break;
#ifndef SSE_LEAN_AND_MEAN
  default:
    BREAKPOINT(EA illegal);
    m68k_trap1();
#endif
  }//sw
}


void m68k_get_source_111_l() {
  switch(IRD&0x7) {
  case 0:  // .L (xxx).W
    //  (xxx).W         |         12(3/0) |                        np nR nr           
    TRUE_PC+=2;
    iabus=(signed short)IRC;
    PREFETCH; //np
    CPU_BUS_ACCESS_READ; //nR
    m68k_src_lh=dbus;
    iabus+=2;
    CPU_BUS_ACCESS_READ; //nr
    m68k_src_ll=dbus;
    break;
  case 1:  // .L (xxx).L
    //  (xxx).L         |         16(4/0) |                     np np nR nr           
    TRUE_PC+=4;
    iabush=IRC;
    PREFETCH; //np
    iabusl=IRC;
    PREFETCH; //np
    CPU_BUS_ACCESS_READ; //nR
    m68k_src_lh=dbus;
    iabus+=2;
    CPU_BUS_ACCESS_READ; //nr
    m68k_src_ll=dbus;
    break;
  case 2:  // .L (d16, PC)
    //  (d16,An)        |         12(3/0) |                        np nR nr           
    iabus=pc+(signed short)IRC;
    PREFETCH; //np
    CPU_BUS_ACCESS_READ; //nR
    m68k_src_lh=dbus;
    iabus+=2;
    CPU_BUS_ACCESS_READ; //nr
    m68k_src_ll=dbus;
    break;
  case 3:  // .L (d8, PC, Xn)
    //  (d8,An,Xn)      |         14(3/0) |                   n    np nR nr           
    CPU_BUS_IDLE(2); //n
    m68k_iriwo=IRC;
    if(m68k_iriwo&BIT_b)  //.l
      iabus=pc+(signed char)(m68k_iriwo)+(int)Cpu.r[m68k_iriwo>>12];
    else          //.w
      iabus=pc+(signed char)(m68k_iriwo)+(signed short)Cpu.r[m68k_iriwo>>12];
    PREFETCH; //np
    CPU_BUS_ACCESS_READ; //nR
    m68k_src_lh=dbus;
    iabus+=2;
    CPU_BUS_ACCESS_READ; //nr
    m68k_src_ll=dbus;
    break;
  case 4:  // .B #<data>
    //  #<data>         |          8(2/0) |                     np np                 
    TRUE_PC+=4;
    m68k_src_lh=IRC;
    PREFETCH; //np
    m68k_src_ll=IRC;
    PREFETCH; //np
    break;
#ifndef SSE_LEAN_AND_MEAN
  default:
    BREAKPOINT(EA illegal);
    m68k_trap1();
#endif
  }//sw
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//////////////////////////    GET DEST       ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


void m68k_get_dest_000_b() {
  //  Dn              |          0(0/0) |
  m68k_dst_b=cpureg[PARAM_M].d8[B0];
}


void m68k_get_dest_000_w() {
  //  Dn              |          0(0/0) |
  m68k_dst_w=cpureg[PARAM_M].d16[LO];
}


void m68k_get_dest_000_l() {
  //  Dn              |          0(0/0) |
  m68k_dst_l=cpureg[PARAM_M].d32;
}


void m68k_get_dest_001_b() {
  //  An              |          0(0/0) |
  m68k_dst_b=cpureg[PARAM_M+8].d8[B0];
}


void m68k_get_dest_001_w() {
  //  An              |          0(0/0) |
  m68k_dst_w=cpureg[PARAM_M+8].d16[LO];
}


void m68k_get_dest_001_l() {
  //  An              |          0(0/0) |
  m68k_dst_l=cpureg[PARAM_M+8].d32;
}


void m68k_get_dest_010_b() {
  //  (An)            |          4(1/0) |                              nr           
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ_B; //nr
  m68k_dst_b=d8;
}


void m68k_get_dest_010_w() {
  //  (An)            |          4(1/0) |                              nr           
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ; //nr
  m68k_dst_w=dbus;
}


void m68k_get_dest_010_l() {
  //  (An)            |          8(2/0) |                           nR nr           
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ; //nR
  m68k_dst_lh=dbus;
  iabus+=2;
  CPU_BUS_ACCESS_READ; //nr
  m68k_dst_ll=dbus;
}


void m68k_get_dest_011_b() {
  //  (An)+           |          4(1/0) |                              nr           
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ_B; //nr
  m68k_dst_b=d8;
  AREG(PARAM_M)++;
  if(PARAM_M==7)
    AREG(PARAM_M)++;
}


void m68k_get_dest_011_w() {
  //  (An)+           |          4(1/0) |                              nr           
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ; //nr
  m68k_dst_w=dbus;
  AREG(PARAM_M)+=2;
}


void m68k_get_dest_011_l() {
  //  (An)+           |          8(2/0) |                           nR nr           
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ; //nR
  m68k_dst_lh=dbus;
  iabus+=2;
  CPU_BUS_ACCESS_READ; //nr
  m68k_dst_ll=dbus;
  AREG(PARAM_M)+=4;
}


void m68k_get_dest_100_b() {
  //  -(An)           |          6(1/0) |                   n          nr           
  TRUE_PC+=2;
  CPU_BUS_IDLE(2); //n
  AREG(PARAM_M)--;
  if(PARAM_M==7)
    AREG(PARAM_M)--;
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ_B; //nr
  m68k_dst_b=d8;
}


void m68k_get_dest_100_w() {
  //  -(An)           |          6(1/0) |                   n          nr           
  TRUE_PC+=2;
  CPU_BUS_IDLE(2); //n
  AREG(PARAM_M)-=2;
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ;
  m68k_dst_w=dbus;
}


void m68k_get_dest_100_l() {
  //  -(An)           |         10(2/0) |                   n       nR nr           
  CPU_BUS_IDLE(2); //n
  AREG(PARAM_M)-=4;
  iabus=AREG(PARAM_M);
  CPU_BUS_ACCESS_READ; //nR
  m68k_dst_lh=dbus;
  iabus+=2;
  CPU_BUS_ACCESS_READ; //nr
  m68k_dst_ll=dbus;
}


void m68k_get_dest_101_b() {
  //  (d16,An)        |          8(2/0) |                        np    nr           
  iabus=AREG(PARAM_M)+(signed short)IRC;
  PREFETCH; //np
  CPU_BUS_ACCESS_READ_B; //nr
  m68k_dst_b=d8;
}


void m68k_get_dest_101_w() {
  //  (d16,An)        |          8(2/0) |                        np    nr           
  iabus=AREG(PARAM_M)+(signed short)IRC;
  PREFETCH; //np
  CPU_BUS_ACCESS_READ; //nr
  m68k_dst_w=dbus;
}


void m68k_get_dest_101_l() {
  //  (d16,An)        |         12(3/0) |                        np nR nr           
  iabus=AREG(PARAM_M)+(signed short)IRC;
  PREFETCH; //np
  CPU_BUS_ACCESS_READ; //nR
  m68k_dst_lh=dbus;
  iabus+=2;
  CPU_BUS_ACCESS_READ; //nr
  m68k_dst_ll=dbus;
}


void m68k_get_dest_110_b() {
  //  (d8,An,Xn)      |         10(2/0) |                   n    np    nr           
  CPU_BUS_IDLE(2); //n 
  m68k_iriwo=IRC;
  if(m68k_iriwo&BIT_b)   //.l
    iabus=AREG(PARAM_M)+(signed char)(m68k_iriwo)+(int)Cpu.r[m68k_iriwo>>12];
  else          //.w
    iabus=AREG(PARAM_M)+(signed char)(m68k_iriwo)+(signed short)Cpu.r[m68k_iriwo>>12];
  PREFETCH; //np
  CPU_BUS_ACCESS_READ_B; //nr
  m68k_dst_b=d8;
}


void m68k_get_dest_110_w() {
  //  (d8,An,Xn)      |         10(2/0) |                   n    np    nr           
  CPU_BUS_IDLE(2); //n
  m68k_iriwo=IRC;
  if(m68k_iriwo&BIT_b)  //.l
    iabus=AREG(PARAM_M)+(signed char)(m68k_iriwo)+(int)Cpu.r[m68k_iriwo>>12];
  else         //.w
    iabus=AREG(PARAM_M)+(signed char)(m68k_iriwo)+(signed short)Cpu.r[m68k_iriwo>>12];
  PREFETCH; //np
  CPU_BUS_ACCESS_READ; //nr
  m68k_dst_w=dbus;
}


void m68k_get_dest_110_l() {
  //  (d8,An,Xn)      |         14(3/0) |                   n    np nR nr           
  CPU_BUS_IDLE(2); //n
  m68k_iriwo=IRC;
  if(m68k_iriwo&BIT_b)   //.l
    iabus=AREG(PARAM_M)+(signed char)(m68k_iriwo)+(int)Cpu.r[m68k_iriwo>>12];
  else          //.w
    iabus=AREG(PARAM_M)+(signed char)(m68k_iriwo)+(signed short)Cpu.r[m68k_iriwo>>12];
  PREFETCH; //np
  CPU_BUS_ACCESS_READ; //nR
  m68k_dst_lh=dbus;
  iabus+=2;
  CPU_BUS_ACCESS_READ; //nr
  m68k_dst_ll=dbus;
}


void m68k_get_dest_111_b() {
  switch(IRD&0x7) {
  case 0:
    //  (xxx).W         |          8(2/0) |                        np    nr           
    iabus=(signed short)IRC; //cast important for sign extension!
    PREFETCH; //np
    TRUE_PC+=2;
    CPU_BUS_ACCESS_READ_B; //nr
    m68k_dst_b=d8;
    break;
  case 1:
    //  (xxx).L         |         12(3/0) |                     np np    nr           
    iabush=IRC;
    PREFETCH; //np
    iabusl=IRC;
    PREFETCH; //np
    TRUE_PC+=4;
    CPU_BUS_ACCESS_READ_B; //nr
    m68k_dst_b=d8;
    break;
#ifndef SSE_LEAN_AND_MEAN
  default:
    BREAKPOINT(EA illegal);
    m68k_trap1();
#endif
  }
}


void m68k_get_dest_111_w() {
  switch(IRD&0x7) {
  case 0:
    //  (xxx).W         |          8(2/0) |                        np    nr           
    iabus=(signed short)IRC;
    PREFETCH; //np
    TRUE_PC+=2;
    CPU_BUS_ACCESS_READ; //nr
    m68k_dst_w=dbus;
    break;
  case 1:
    //  (xxx).L         |         12(3/0) |                     np np    nr           
    iabush=IRC;
    PREFETCH; //np
    iabusl=IRC;
    PREFETCH; //np
    TRUE_PC+=4;
    CPU_BUS_ACCESS_READ; //nr
    m68k_dst_w=dbus;
    break;
#ifndef SSE_LEAN_AND_MEAN
  default:
    BREAKPOINT(EA illegal);
    m68k_trap1();
#endif
  }
}


void m68k_get_dest_111_l() {
  switch(IRD&0x7) {
  case 0:
    //  (xxx).W         |         12(3/0) |                        np nR nr           
    iabus=(signed short)IRC;
    PREFETCH; //np
    TRUE_PC+=2;
    CPU_BUS_ACCESS_READ; //nR
    m68k_dst_lh=dbus;
    iabus+=2;
    CPU_BUS_ACCESS_READ; //nr
    m68k_dst_ll=dbus;
    break;
  case 1:
    //  (xxx).L         |         16(4/0) |                     np np nR nr           
    iabush=IRC;
    PREFETCH; //np
    iabusl=IRC;
    PREFETCH; //np
    TRUE_PC+=4;
    CPU_BUS_ACCESS_READ; //nR
    m68k_dst_lh=dbus;
    iabus+=2;
    CPU_BUS_ACCESS_READ; //nr
    m68k_dst_ll=dbus;
    break;
#ifndef SSE_LEAN_AND_MEAN
  default:
    BREAKPOINT(EA illegal);
    m68k_trap1();
#endif
  }
}


// read DEST

BYTE m68k_read_dest_b() { //only used by tst.b, cmpi.b
  BYTE x=0;
  switch(IRD&BITS_543) {
  case BITS_543_000:
    x=cpureg[PARAM_M].d8[B0];
    break;
#ifndef SSE_LEAN_AND_MEAN
  case BITS_543_001:
    BREAKPOINT(EA illegal);
    m68k_trap1();
    break;
#endif
  case BITS_543_010:
    iabus=AREG(PARAM_M);
    CPU_BUS_ACCESS_READ_B; //nr
    x=d8;
    break;
  case BITS_543_011: // (An)++
    iabus=AREG(PARAM_M);
    CPU_BUS_ACCESS_READ_B; //nr
    x=d8;
    AREG(PARAM_M)++;
    if(PARAM_M==7)
      SP++; // stack pointer must always be even
    break;
  case BITS_543_100: // --(An)
    AREG(PARAM_M)--;
    if(PARAM_M==7)
      SP--; // stack pointer must always be even
    CPU_BUS_IDLE(2); //n
    iabus=AREG(PARAM_M);
    CPU_BUS_ACCESS_READ_B; //nr
    x=d8;
    break;
  case BITS_543_101:
    //  (d16,An)        | 101 | reg |   8(2/0)   |              np    nr     
    iabus=AREG(PARAM_M)+(signed short)IRC;
    PREFETCH; //np
    CPU_BUS_ACCESS_READ_B; //nr
    x=d8;
    break;
  case BITS_543_110:
    //  (d8,An,Xn)      |         10(2/0) |                   n    np    nr           
    CPU_BUS_IDLE(2); //n
    m68k_iriwo=IRC;
    PREFETCH; //np
    if(m68k_iriwo&BIT_b)   //.l
      iabus=AREG(PARAM_M)+(signed char)(m68k_iriwo)+(int)Cpu.r[m68k_iriwo>>12];
    else          //.w
      iabus=AREG(PARAM_M)+(signed char)(m68k_iriwo)+(signed short)Cpu.r[m68k_iriwo>>12];
    CPU_BUS_ACCESS_READ_B; //nr
    x=d8;
    break;
  case BITS_543_111:
    switch(IRD&0x7) {
    case 0:
      //  (xxx).W         | 111 | 000 |   8(2/0)   |              np    nr       
      iabus=(DWORD)((LONG)((signed short)IRC));
      PREFETCH; //np
      CPU_BUS_ACCESS_READ_B; //nr
      x=d8;
      break;
    case 1:
      //  (xxx).L         | 111 | 001 |  12(3/0)   |           np np    nr    
      iabush=IRC;
      PREFETCH; //np
      iabusl=IRC;
      PREFETCH; //np
      CPU_BUS_ACCESS_READ_B; //nr
      x=d8;
      break;
#ifndef SSE_LEAN_AND_MEAN
    default:
      BREAKPOINT(EA illegal);
      m68k_trap1();
#endif
    }
  }
  m68k_dst_b=x;
  return x;
}


WORD m68k_read_dest_w() { //only used by tst.w, cmpi.w
  WORD x=0;
  switch(IRD&BITS_543) {
  case BITS_543_000:
    x=LOWORD(Cpu.r[PARAM_M]);
    break;
#ifndef SSE_LEAN_AND_MEAN
  case BITS_543_001:
    BREAKPOINT(EA illegal);
    m68k_trap1();
    break;
#endif
  case BITS_543_010:
    iabus=AREG(PARAM_M);
    CPU_BUS_ACCESS_READ; //nr
    x=dbus;
    break;
  case BITS_543_011:
    iabus=AREG(PARAM_M);
    CPU_BUS_ACCESS_READ; //nr
    x=dbus;
    AREG(PARAM_M)+=2;
    break;
  case BITS_543_100:
    CPU_BUS_IDLE(2); //n
    AREG(PARAM_M)-=2;
    iabus=AREG(PARAM_M);
    CPU_BUS_ACCESS_READ; //nr
    x=dbus;
    break;
  case BITS_543_101:
    //  (d16,An)        | 101 | reg |   8(2/0)   |              np    nr     
    iabus=AREG(PARAM_M)+(signed short)IRC;
    PREFETCH; //np
    CPU_BUS_ACCESS_READ; //nr
    x=dbus;
    break;
  case BITS_543_110:
    //  (d8,An,Xn)      |         10(2/0) |                   n    np    nr           
    CPU_BUS_IDLE(2); //n
    m68k_iriwo=IRC;
    PREFETCH; //np
    if(m68k_iriwo&BIT_b)   //.l
      iabus=AREG(PARAM_M)+(signed char)(m68k_iriwo)+(int)Cpu.r[m68k_iriwo>>12];
    else          //.w
      iabus=AREG(PARAM_M)+(signed char)(m68k_iriwo)+(signed short)Cpu.r[m68k_iriwo>>12];
    CPU_BUS_ACCESS_READ; //nr
    x=dbus;
    break;
  case BITS_543_111:
    switch(IRD&0x7) {
    case 0:
      //  (xxx).W         | 111 | 000 |   8(2/0)   |              np    nr       
      iabus=(DWORD)((LONG)((signed short)IRC));
      PREFETCH; //np
      CPU_BUS_ACCESS_READ; //nr
      x=dbus;
      break;
    case 1:
      //  (xxx).L         | 111 | 001 |  12(3/0)   |           np np    nr    
      iabush=IRC;
      PREFETCH; //np
      iabusl=IRC;
      PREFETCH; //np
      CPU_BUS_ACCESS_READ; //nr
      x=dbus;
      break;
#ifndef SSE_LEAN_AND_MEAN
    default:
      BREAKPOINT(EA illegal);
      m68k_trap1();
#endif
    }
  }
  m68k_dst_w=x;
  return x;
}


LONG m68k_read_dest_l() { //only used by tst.l, cmpi.l
  DU32 ux;
  DWORD &x=ux.d32;
  WORD &xh=ux.d16[HI];
  WORD &xl=ux.d16[LO];
  switch(IRD&BITS_543) {
  case BITS_543_000:
    x=Cpu.r[PARAM_M];
    break;
#ifndef SSE_LEAN_AND_MEAN
  case BITS_543_001:
    BREAKPOINT(EA illegal);
    m68k_trap1();
    break;
#endif
  case BITS_543_010:
    iabus=AREG(PARAM_M);
    CPU_BUS_ACCESS_READ; //nR
    xh=dbus;
    iabus+=2;
    CPU_BUS_ACCESS_READ; //nr
    xl=dbus;
    break;
  case BITS_543_011:
    iabus=AREG(PARAM_M);
    CPU_BUS_ACCESS_READ; //nR
    xh=dbus;
    iabus+=2;
    CPU_BUS_ACCESS_READ; //nr
    xl=dbus;
    AREG(PARAM_M)+=4;
    break;
  case BITS_543_100:
    CPU_BUS_IDLE(2); //n
    AREG(PARAM_M)-=4;
    iabus=AREG(PARAM_M);
    CPU_BUS_ACCESS_READ; //nR
    xh=dbus;
    iabus+=2;
    CPU_BUS_ACCESS_READ; //nr
    xl=dbus;
    break;
  case BITS_543_101:
    //  (d16,An)        | 101 | reg |  12(3/0)   |              np nR nr          
    iabus=AREG(PARAM_M)+(signed short)IRC;
    PREFETCH; //np
    CPU_BUS_ACCESS_READ; //nR
    xh=dbus;
    iabus+=2;
    CPU_BUS_ACCESS_READ; //nr
    xl=dbus;
    break;
  case BITS_543_110:
    //  (d8,An,Xn)      | 110 | reg |  14(3/0)   |         n    np nR nr           
    CPU_BUS_IDLE(2); //n
    m68k_iriwo=IRC;
    PREFETCH; //np
    if(m68k_iriwo&BIT_b)   //.l
      iabus=AREG(PARAM_M)+(signed char)(m68k_iriwo)+(int)Cpu.r[m68k_iriwo>>12];
    else          //.w
      iabus=AREG(PARAM_M)+(signed char)(m68k_iriwo)+(signed short)Cpu.r[m68k_iriwo>>12];
    CPU_BUS_ACCESS_READ; //nR
    xh=dbus;
    iabus+=2;
    CPU_BUS_ACCESS_READ; //nr
    xl=dbus;
    break;
  case BITS_543_111:
    switch(IRD&0x7) {
    case 0:
      //  (xxx).W         | 111 | 000 |  12(3/0)   |              np nR nr      
      iabus=(DWORD)((LONG)((signed short)IRC));
      PREFETCH; //np
      CPU_BUS_ACCESS_READ; //nR
      xh=dbus;
      iabus+=2;
      CPU_BUS_ACCESS_READ; //nr
      xl=dbus;
      break;
    case 1:
      //(xxx).L         | 111 | 001 |  16(4/0)   |           np np nR nr           
      iabush=IRC;
      PREFETCH; //np
      iabusl=IRC;
      PREFETCH; //np
      CPU_BUS_ACCESS_READ; //nR
      xh=dbus;
      iabus+=2;
      CPU_BUS_ACCESS_READ; //nr
      xl=dbus;
      break;
#ifndef SSE_LEAN_AND_MEAN
    default:
      BREAKPOINT(EA illegal);
      m68k_trap1();
      break;
#endif
    }
    break;
  }
  m68k_dst_l=x;
  return x;
}

#undef LOGSECTION
