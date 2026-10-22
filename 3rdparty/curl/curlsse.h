#pragma once

#include "curl.h"

#ifdef WIN32

typedef CURLcode  (__cdecl* PFN_CURL_GLOBAL_INIT)(long flags);
typedef void*(__cdecl* PFN_CURL_EASY_INIT)();
typedef CURLcode  (__cdecl* PFN_CURL_EASY_SETOPT)(void* curl, int option, ...);
typedef CURLcode  (__cdecl* PFN_CURL_EASY_PERFORM)(void* curl);
typedef void (__cdecl* PFN_CURL_EASY_CLEANUP)(void* curl);
typedef void (__cdecl* PFN_CURL_GLOBAL_CLEANUP)();

extern PFN_CURL_GLOBAL_INIT  dyn_curl_global_init;
extern PFN_CURL_EASY_INIT    dyn_curl_easy_init;
extern PFN_CURL_EASY_SETOPT  dyn_curl_easy_setopt;
extern PFN_CURL_EASY_PERFORM dyn_curl_easy_perform;
extern PFN_CURL_EASY_CLEANUP dyn_curl_easy_cleanup;
extern PFN_CURL_GLOBAL_CLEANUP dyn_curl_global_cleanup;

#define curl_global_init  dyn_curl_global_init
#define curl_easy_init    dyn_curl_easy_init
#define curl_easy_setopt  dyn_curl_easy_setopt
#define curl_easy_perform dyn_curl_easy_perform
#define curl_easy_cleanup dyn_curl_easy_cleanup
#define curl_global_cleanup dyn_curl_global_cleanup

extern HMODULE curl_dll_init();

#endif//WIN32
