/*---------------------------------------------------------------------------
PROJECT: Steem SSE
Atari ST emulator
Copyright (C) 2026 by Anthony Hayward and Russel Hayward + SSE

FILE: isa_picogus.h
DESCRIPTION: PicoGUS (ISA sound card) on an emulated ISA bus window, as on
the Raven060. See isa_picogus.cpp.
---------------------------------------------------------------------------*/

#pragma once
#ifndef ISA_PICOGUS_H
#define ISA_PICOGUS_H

#if defined(SSE_PICOGUS)

#include "conditions.h" // MEM_ADDRESS

// ISA I/O space at $F20000-$F2FFFF (port = address & $FFFF), lane swapped
// little endian like the Raven060's ISA bus. Run ISA_BIOS.PRG with
// isa.iobase=0xF20000 and isa.endian=lels in C:\ISA_BIOS.INF.
#define PICOGUS_ISA_IOBASE 0xF20000
#define PICOGUS_ISA_DECODE ((BYTE)(PICOGUS_ISA_IOBASE>>16))

BYTE PicoGusIsaRead(MEM_ADDRESS ad);
void PicoGusIsaWrite(MEM_ADDRESS ad,BYTE x);
WORD PicoGusIsaReadW(MEM_ADDRESS ad); // ad can be odd (68060 Raven)
void PicoGusIsaWriteW(MEM_ADDRESS ad,WORD x);

void PicoGusPowerOff(); // ST power cycle: the card is powered on at its next access
void PicoGusMixSound(void *DatAdr[2],DWORD LockLength[2],DWORD write_time);

#endif//SSE_PICOGUS

#endif//ISA_PICOGUS_H
