/* Steem PicoGUS shim: pico/platform.h */
#pragma once
#include "../pg_shim.h"
#ifndef __force_inline
#define __force_inline inline
#endif
#ifndef __not_in_flash_func
#define __not_in_flash_func(f) f
#endif
#ifndef __time_critical_func
#define __time_critical_func(f) f
#endif
#ifndef __scratch_x
#define __scratch_x(s)
#define __scratch_y(s)
#endif
#ifndef __CONCAT
#define __CONCAT1(a,b) a##b
#define __CONCAT(a,b) __CONCAT1(a,b)
#endif
static inline void tight_loop_contents(void) {}
static inline void __dsb(void) {}
static inline void __dmb(void) {}
