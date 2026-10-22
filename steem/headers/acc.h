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

DOMAIN: Various
FILE: acc.h
DESCRIPTION: Declarations for completely random accessory functions.
---------------------------------------------------------------------------*/

#pragma once
#ifndef ACC_DECLA_H
#define ACC_DECLA_H


#include <easystr.h>
#include <dynamicarray.h>
#include "conditions.h"
#include "SSE.h"

EasyStr HEXSl(LONG n,int ln);
#ifdef SSE_X64
EasyStr HEXSll(long long n,int ln);
#endif


void parse_search_string(Str OriginalText,DynamicArray<BYTE> &ByteList,bool &WordOnly);
MEM_ADDRESS acc_find_bytes(DynamicArray<BYTE> &BytesToFind,bool WordOnly,MEM_ADDRESS ad,int dir);
LONG colour_convert(int red,int green,int blue);


#ifdef WIN32
HMODULE SteemLoadLibrary(LPCSTR lpLibFileName);
BOOL SteemFreeLibrary(HMODULE& hLibModule); // just adding hLibModule=NULL
int get_text_width(char*t);
#endif

#if !defined(SSE_NO_UPDATE)
EasyStr time_or_times(int n);
#endif

int SteemFFlush(FILE *fp);
BOOL SteemFFlush(HANDLE fp);

#endif//ACC_DECLA_H
