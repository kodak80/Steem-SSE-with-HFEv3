/*---------------------------------------------------------------------------
PROJECT: Steem SSE
Atari ST emulator
Copyright (C) 2020 by Anthony Hayward and Russel Hayward + SSE

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

DOMAIN: All
FILE: QuickEmu.cpp
CONDITION: Quick project/script
DESCRIPTION: Amalgamation for much faster compilation
(back to the old Steem way)
---------------------------------------------------------------------------*/

#include "pch.h"
#pragma hdrstop

// Include
#include "circularbuffer.cpp"
#include "portio.cpp"

// Steem
#include "acia.cpp"
#include "blitter.cpp"
#include "computer.cpp"
#include "cpu.cpp"
#include "cpu_ea.cpp"
#include "cpu_op.cpp"
#include "cpuinit.cpp"
#include "dma.cpp"
#include "disk_ghost.cpp"
#include "disk_hfe.cpp"
#include "disk_scp.cpp"
#include "disk_stw.cpp"
#if defined(SSE_DISK_STX)
#include "disk_stx.cpp"
#endif
#include "display.cpp" // blitting in emu module
#include "draw.cpp"
#include "emulator.cpp"
#include "fdc.cpp"
#include "floppy_disk.cpp"
#include "floppy_drive.cpp"
#include "glue.cpp"
#include "acsi.cpp"
#include "hd_gemdos.cpp"
#include "ikbd.cpp"
#include "interface_caps.cpp"
#ifdef UNIX
#include "interface_pa.cpp"
#include "interface_rta.cpp"
#include "interface_pulse.cpp"
#endif
#include "interface_stvl.cpp"
#include "ior.cpp"
#include "iow.cpp"
#include "mfp.cpp"
#include "midi.cpp"
#include "mmu.cpp"
#include "osd.cpp"
#include "palette.cpp"
#include "printer.cpp"
#include "psg.cpp"
#include "reset.cpp"
#include "rs232.cpp"
#include "run.cpp"
#include "shifter.cpp"
#include "sound.cpp"  // sound rendering in emu module
#include "stjoy.cpp"
#include "stjoy_directinput.cpp"
#include "stports.cpp"
#include "tos.cpp"
