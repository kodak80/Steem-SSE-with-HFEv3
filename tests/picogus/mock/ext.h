/* Test mock of PureC/libcmini <ext.h>: delay() runs emulated time */
#pragma once
#include <stdint.h>
#include "pgemu.h"
extern uint64_t mock_time_us;
static inline void delay(unsigned ms) { mock_time_us += 1000ull * ms; pgemu_advance(mock_time_us); }
#include <strings.h>
#define stricmp strcasecmp
#define strnicmp strncasecmp
