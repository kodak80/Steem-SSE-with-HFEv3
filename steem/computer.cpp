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
FILE: computer.cpp
DESCRIPTION: Instantiation of various objects making up the virtual ST.
Steem also uses many global functions and variables.
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#include <conditions.h>
#include <debug.h>
#include <computer.h>
#include <draw.h>
#include <stjoy.h>


TGlue Glue;
TMC68000 Cpu;
TMmu Mmu;
TShifter Shifter;
TMC68901 Mfp;
TDma Dma;
TWD1772 Fdc;
TYM2149 Psg;
TBlitter Blitter;
TMC6850 acia[2];
THD6301 Ikbd;
TSF314 FloppyDrive[3];
TFloppyDisk FloppyDisk[3];
#if defined(SSE_ACSI)
TAcsiHdc AcsiHdc[MAX_ACSI_DEVICES];
#if defined(SSE_ACSI_LASER)
TSLM804 Laser;
#endif
#endif
TLMC1992 Microwire;
TTos Tos;
#if defined(SSE_MEGASTE)
TMegaSte MegaSte;
#endif
#if defined(SSE_MEGA)
TCpu16 Cpu16;
#endif
#if defined(SSE_PRINTER)
TSMM804 Printer;
#endif


// called at init, reset, run
void ComputerRestore() {
  Mfp.Restore();
  Glue.Restore();
  Mmu.Restore();
  Shifter.Restore();
  Psg.Restore(); //necessary
  for(BYTE id=0;id<2;id++) // 2 of each
  {
    acia[id].Id=id;
#ifdef SSE_DISK_GHOST
    GhostDisk[id].Id=id;
#endif
  }
  for(BYTE id=0;id<=2;id++) // 3 of each
  {
    FloppyDrive[id].Restore(id);
    FloppyDisk[id].Id=id;
    if(FloppyDisk[id].TrackBytes<=0)
      FloppyDisk[id].TrackBytes=DISK_BYTES_PER_TRACK;
#if defined(SSE_DISK_STW)
    ImageSTW[id].Id=id;
#endif
#if defined(SSE_DISK_HFE)
    ImageHFE[id].Id=id;
#endif
#if defined(SSE_DISK_SCP)
    ImageSCP[id].Id=id;
#endif
#if defined(SSE_DISK_STX)
    ImageSTX[id].Id=id;
#endif
  }
#ifdef SSE_ACSI
  for(BYTE i=0;i<MAX_ACSI_DEVICES;i++)
    AcsiHdc[i].Id=i;
  acsi_dev&=(MAX_ACSI_DEVICES-1); // OK for 4 or 8
#endif
  frameskip=MAX(1,frameskip); // frameskip=0 has a funny effect
#if defined(SSE_OPTION_FASTLINEA)
  lineA=false;
#endif
  SetTimingFunctions();
  Draw.MarshalParameters();
#if !defined(SSE_LIBRETRONUKE)
  joy_check_controllers();
#endif
  if(SSEOptions.DriveRpm<=0)
    SSEOptions.DriveRpm=DRIVE_RPM_CST;
  if(SSEOptions.TrackBytes<=0)
    SSEOptions.TrackBytes=DISK_BYTES_PER_TRACK_CST;
  if(!MIDI_in_speed)
    MIDI_in_speed=100;
  if(!nSysCyclesPerSecond)
    nSysCyclesPerSecond=CpuNormalHz=CPU_CLOCK_STE_PAL;
}
