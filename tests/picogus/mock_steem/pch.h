/* Test mock of the Steem globals isa_picogus.cpp uses (see ../test_steem.cpp) */
#pragma once
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <string>
#define SSE_PICOGUS
#define SSE_TIMINGS_US
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef unsigned long long DWORDLONG;
typedef int64_t COUNTER_VAR;
typedef DWORD MEM_ADDRESS;
#define MIN(a,b) ((a)<(b)?(a):(b))
#define HIBYTE(w) ((BYTE)(((WORD)(w))>>8))
#define LOBYTE(w) ((BYTE)(w))
#define ZeroMemory(p,n) memset((p),0,(n))
#define SLASH "/"
#define TRACE_LOG(...) ((void)0)
#define TRACE(...) ((void)0)
extern MEM_ADDRESS pc;
struct EasyStr {
    std::string s; const char *Text;
    EasyStr(const char *c = "") : s(c), Text(s.c_str()) {}
    EasyStr(const EasyStr &o) : s(o.s), Text(s.c_str()) {}
    EasyStr operator+(const char *c) const { return EasyStr((s + c).c_str()); }
};
extern EasyStr UsersPath;
extern COUNTER_VAR mock_sys_time, sys_time_of_last_vbl;
#define ABSOLUTE_SYS_TIME (mock_sys_time)
extern DWORD nSysCyclesPerSecond;
extern BYTE VideoFreqAtStartOfVbl;
struct TGlueMock { int nFrameCycles; };
extern TGlueMock Glue;
extern DWORD psg_time_of_last_vbl_for_writing, psg_n_samples_this_vbl, sound_freq;
extern BYTE sound_num_channels, sound_num_bits, sound_bytes_per_sample, UseSound;
