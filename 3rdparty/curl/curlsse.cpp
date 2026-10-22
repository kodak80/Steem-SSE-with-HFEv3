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

DOMAIN: Network
FILE: curlsse.cpp
CONDITION: WIN32 && SSE_CHECKUPDATE must be defined
DESCRIPTION: Code interfacing Steem SSE with the curl DLL (just init)
---------------------------------------------------------------------------*/

#include <pch.h>
#pragma hdrstop

#if defined(SSE_CHECKUPDATE)

#include "curlsse.h"
#include <parameters.h>
#include <acc.h>

PFN_CURL_GLOBAL_INIT    dyn_curl_global_init    = NULL;
PFN_CURL_EASY_INIT      dyn_curl_easy_init      = NULL;
PFN_CURL_EASY_SETOPT    dyn_curl_easy_setopt    = NULL;
PFN_CURL_EASY_PERFORM   dyn_curl_easy_perform   = NULL;
PFN_CURL_EASY_CLEANUP   dyn_curl_easy_cleanup   = NULL;
PFN_CURL_GLOBAL_CLEANUP dyn_curl_global_cleanup = NULL;


#define LOGSECTION LOGSECTION_INIT

HMODULE curl_dll_init() {
  HMODULE hCurl=SteemLoadLibrary(CURL_DLL);
  if(hCurl!=NULL)
  {
    dyn_curl_global_init  = (PFN_CURL_GLOBAL_INIT)GetProcAddress(hCurl,"curl_global_init");
    dyn_curl_easy_init    = (PFN_CURL_EASY_INIT)GetProcAddress(hCurl,"curl_easy_init");
    dyn_curl_easy_setopt  = (PFN_CURL_EASY_SETOPT)GetProcAddress(hCurl,"curl_easy_setopt");
    dyn_curl_easy_perform = (PFN_CURL_EASY_PERFORM)GetProcAddress(hCurl,"curl_easy_perform");
    dyn_curl_easy_cleanup = (PFN_CURL_EASY_CLEANUP)GetProcAddress(hCurl,"curl_easy_cleanup");
    dyn_curl_global_cleanup = (PFN_CURL_GLOBAL_CLEANUP)GetProcAddress(hCurl,"curl_global_cleanup");
#ifndef SSE_LEAN_AND_MEAN
    if(dyn_curl_global_init&&dyn_curl_easy_init&&dyn_curl_easy_setopt&&dyn_curl_easy_perform
       &&dyn_curl_easy_cleanup&&dyn_curl_global_cleanup)
    {    } else hCurl=NULL;
#endif
  }
  return hCurl;
}

#undef LOGSECTION

#endif//#if defined(SSE_CHECKUPDATE)
