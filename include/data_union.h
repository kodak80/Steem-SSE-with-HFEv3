/*---------------------------------------------------------------------------
PROJECT: Steem SSE
Atari ST emulator
Copyright (C) 2019 by Anthony Hayward, Russel Hayward + SSE

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
FILE: data_union.h
DESCRIPTION: Those smart unions allow direct assignment of part of the data,
as opposed to masking and shifting.
Not sure it's recommended by the standard but we're doing low-level stuff.
For instance, d8[1] is the high byte of d16 (little endian systems)
you do: 

var.d8[1]=my_byte;

instead of: 

var&=0x00FF; 
var|=(my_byte)<<8;

You can also use C++ references this way:

BYTE &varh=var.d8[1];
BYTE &varl=var.d8[0];

Then:

varh=my_byte;

In Steem SSE we also define LO (low order) and HI (high order) 
for potential portability.
---------------------------------------------------------------------------*/

#pragma once
#ifndef DATA_UNION_H
#define DATA_UNION_H


union DU16 {
  unsigned short d16;
  unsigned char d8[2];
  DU16() { d16=0; }
  DU16(WORD w) {d16=w;}
};

union DU32 {
  DWORD d32;
  unsigned short d16[2];
  unsigned char d8[4];
  DU32() { d32=0; }
  DU32(DWORD dw) {d32=dw;}
};

union DU64 {
  DWORDLONG d64;
  unsigned int d32[2];
  unsigned short d16[4];
  unsigned char d8[8];
};

union DUS16 {
  signed short d16;
  signed char d8[2];
};

union DUS32 {
  LONG d32;
  signed short d16[2];
  signed char d8[4];
};

#ifdef UNIX
union DUS64 {
  signed long long d64;
  signed int d32[2];
  signed short d16[4];
  signed char d8[8];
};
#endif

#ifdef WIN32
union DUS64 {
  signed __int64 d64;
  LONG d32[2];
  signed short d16[4];
  signed char d8[8];
};
#endif

#endif//#ifndef DATA_UNION_H
