/*  Steem SSE PicoGUS emulation: included first by every PicoGUS translation
    unit (the firmware sources are built unmodified around it).
    - firmware globals with names generic enough to collide with the
      emulator's get a pg_ prefix
    - GCC builtins the firmware uses, for MSVC */
#pragma once
#define settings   pg_settings
#define volume     pg_volume
#define LED_PIN    pg_LED_PIN
#define dma_config pg_dma_config
#define psram_spi  pg_psram_spi

#if defined(_MSC_VER)
#include <stdlib.h>
#ifndef __builtin_expect
#define __builtin_expect(x, y) (x)
#endif
#define __builtin_bswap16 _byteswap_ushort
#pragma warning(disable : 4100 4127 4189 4201 4244 4245 4267 4305 4389 4456 4457 4458 4505 4701 4706)
#endif
