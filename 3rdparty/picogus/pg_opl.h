/*  Steem SSE PicoGUS emulation: OPL2 (AdLib) as on the PicoGUS SB and AdLib
    firmwares, with Nuked OPL3 (sw/opl/opl3.c) as the synthesis core and the
    timer/status handling of the firmware's sw/opl/opl_pico.c.

    Copyright (C) 2026 - GPL v2 or later, like the PicoGUS sources.
*/
#pragma once
#include <stdint.h>

void pg_opl_init(uint32_t sample_rate);
// status port: bank 1 gives the OPL2 status (0x06 + timer flags), bank 2 0xFF
uint8_t pg_opl_read_status(bool bank2);
// reg: 0x000-0x0FF bank 1, 0x100-0x1FF bank 2 (ignored, the card is an OPL2)
void pg_opl_write(uint16_t reg, uint8_t value);
// one stereo frame at the sample rate given to pg_opl_init
void pg_opl_generate(int16_t frame[2]);
