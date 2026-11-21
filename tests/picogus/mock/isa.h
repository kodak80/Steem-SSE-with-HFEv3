/*  Test mock of the Raven ISA_BIOS header (sw/isa/isa.h) for pgusinit:
    outp/inp go to the emulated card. A word access is two byte accesses,
    low port first, which is what Steem's ISA window does with the LELS
    (lane swapped) word accesses of ISA_BIOS. Every access takes 1us of
    emulated time; delay() takes its milliseconds. */
#pragma once
#include <stdint.h>
#include "pgemu.h"
extern uint64_t mock_time_us;
static inline void outp(uint16_t port, uint8_t data) { mock_time_us++; pgemu_outb(port, data, mock_time_us); }
static inline uint8_t inp(uint16_t port) { mock_time_us++; return pgemu_inb(port, mock_time_us); }
static inline void outpw(uint16_t port, uint16_t data) { outp(port, data & 0xff); outp(port + 1, data >> 8); }
static inline uint16_t inpw(uint16_t port) { uint16_t lo = inp(port); return lo | (inp(port + 1) << 8); }
