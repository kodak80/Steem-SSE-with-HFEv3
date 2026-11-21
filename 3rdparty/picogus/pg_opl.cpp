/*  Steem SSE PicoGUS emulation: OPL2, see pg_opl.h.
    Timer code follows the PicoGUS firmware's sw/opl/opl_pico.c
    (Copyright (C) 2005-2014 Simon Howard, (C) 2021-2022 Graham Sanderson).

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/
#include "pg_rename.h"
#include "pg_opl.h"
#include "pg_shim.h"
extern "C" {
#include "sw/opl/opl3.h"
}
#include "sw/opl/opl.h"

typedef struct {
    unsigned int rate;      // timer ticks per second
    unsigned int enabled;
    unsigned int value;
    uint64_t expire_time;
} opl_timer_t;

static opl3_chip pg_opl_chip;
static opl_timer_t timer1 = { 12500, 0, 0, 0 };
static opl_timer_t timer2 = { 3125, 0, 0, 0 };

void pg_opl_init(uint32_t sample_rate) {
    OPL3_Reset(&pg_opl_chip, sample_rate);
    timer1.enabled = timer2.enabled = 0;
    timer1.value = timer2.value = 0;
}

uint8_t pg_opl_read_status(bool bank2) {
    // OPL2 has 0x06 in its status register. If this is 0, it'll get detected as an OPL3...
    unsigned int result = 0x06;
    if (bank2)
        return 0xff;
    uint64_t now = pg_shim_now_us;
    if (timer1.enabled && now > timer1.expire_time)
        result |= 0x80 | 0x40;
    if (timer2.enabled && now > timer2.expire_time)
        result |= 0x80 | 0x20;
    return (uint8_t)result;
}

static void OPLTimer_CalculateEndTime(opl_timer_t *timer) {
    if (timer->enabled) {
        int tics = 0x100 - timer->value;
        timer->expire_time = pg_shim_now_us + ((uint64_t)tics * OPL_SECOND) / timer->rate;
    }
}

void pg_opl_write(uint16_t reg, uint8_t value) {
    if (reg & 0x100)
        return; // OPL2: no second register bank
    switch (reg) {
    case OPL_REG_TIMER1:
        timer1.value = value;
        OPLTimer_CalculateEndTime(&timer1);
        break;
    case OPL_REG_TIMER2:
        timer2.value = value;
        OPLTimer_CalculateEndTime(&timer2);
        break;
    case OPL_REG_TIMER_CTRL:
        if (value & 0x80) {
            timer1.enabled = 0;
            timer2.enabled = 0;
        } else {
            if ((value & 0x40) == 0) {
                timer1.enabled = (value & 0x01) != 0;
                OPLTimer_CalculateEndTime(&timer1);
            }
            if ((value & 0x20) == 0) {
                timer2.enabled = (value & 0x02) != 0;
                OPLTimer_CalculateEndTime(&timer2);
            }
        }
        break;
    default:
        OPL3_WriteReg(&pg_opl_chip, reg, value);
        break;
    }
}

void pg_opl_generate(int16_t frame[2]) {
    OPL3_GenerateResampled(&pg_opl_chip, frame);
}
