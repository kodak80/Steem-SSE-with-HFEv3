/* Steem PicoGUS shim: pico/time.h - time is the emulated machine's */
#pragma once
#include "platform.h"
static inline uint32_t time_us_32(void) { return (uint32_t)pg_shim_now_us; }
static inline uint64_t time_us_64(void) { return pg_shim_now_us; }
static inline void busy_wait_us(uint32_t us) { (void)us; }
static inline void busy_wait_us_32(uint32_t us) { (void)us; }
static inline void busy_wait_ms(uint32_t ms) { (void)ms; }
