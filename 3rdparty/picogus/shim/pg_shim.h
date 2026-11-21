/*  Steem SSE PicoGUS emulation - Pico SDK shim (core declarations).

    The PicoGUS firmware sources in ../sw are built unmodified on the host
    against these replacement headers. Time comes from the emulated machine
    (pg_shim_now_us), timer events run when the emulator advances time,
    PSRAM is a 1MB array and the ISA DMA engine never completes a transfer
    (the Raven060 ISA bus has no DMA).

    Copyright (C) 2026 - GPL v2 or later, like the PicoGUS sources.
*/
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int uint;

/* emulated time, microseconds since the card was powered on */
extern uint64_t pg_shim_now_us;

/* IRQ line driven by the firmware (gpio IRQ_PIN) */
extern volatile bool pg_shim_irq_line;

/* 1MB of PSRAM (GUS DRAM on a PicoGUS 2.0) */
#define PG_SHIM_PSRAM_SIZE (1024u*1024u)
extern uint8_t pg_shim_psram[PG_SHIM_PSRAM_SIZE];

#ifdef __cplusplus
}
#endif
