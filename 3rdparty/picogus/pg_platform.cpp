/*  Steem SSE PicoGUS emulation: implementation of the Pico SDK shim
    (emulated time, timer events, PSRAM, IRQ line).

    Copyright (C) 2026 - GPL v2 or later, like the PicoGUS sources.
*/
#include "pg_rename.h"
#include "system/pico_pic.h"
#include "isa/isa_dma.h"
#include <string.h>

static PIO_t pg_pio[2];
PIO pio0 = &pg_pio[0], pio1 = &pg_pio[1];

extern "C" {

uint64_t pg_shim_now_us = 0;
volatile bool pg_shim_irq_line = false;
uint8_t pg_shim_psram[PG_SHIM_PSRAM_SIZE];

// every event ever scheduled; the firmware's events are static objects
static PIC_TimerEvent *pg_events = NULL;
static int pg_next_alarm_id = 1;

void PIC_AddEvent(PIC_TimerEvent* event, uint32_t delay, Bitu val) {
    if (!event->registered) {
        event->registered = true;
        event->next_registered = pg_events;
        pg_events = event;
    }
    event->value = val;
    event->expire_us = pg_shim_now_us + delay;
    event->alarm_id = pg_next_alarm_id++;
    if (pg_next_alarm_id <= 0)
        pg_next_alarm_id = 1;
}

void PIC_RemoveEvent(PIC_TimerEvent* event) {
    event->alarm_id = 0;
}

void PIC_Init(void) {}

double PIC_FullIndex(void) { // milliseconds, as in DOSBox
    return (double)pg_shim_now_us / 1000.0;
}

void pg_shim_run_events(uint64_t now_us) {
    if (now_us < pg_shim_now_us)
        now_us = pg_shim_now_us; // time never goes backwards
    for (;;) {
        PIC_TimerEvent *due = NULL;
        for (PIC_TimerEvent *e = pg_events; e; e = e->next_registered)
            if (e->alarm_id && e->expire_us <= now_us && (!due || e->expire_us < due->expire_us))
                due = e;
        if (!due)
            break;
        // the handler sees the time it was due at, as with the Pico's alarm pool
        pg_shim_now_us = due->expire_us;
        int id = due->alarm_id;
        uint32_t ret = due->handler(due->value);
        if (due->alarm_id != id)
            continue; // rescheduled or removed by the handler
        if (ret) // re-arm from the time it triggered (negative return in pico_pic.c)
            due->expire_us += ret;
        else
            due->alarm_id = 0;
    }
    pg_shim_now_us = now_us;
}

void pg_shim_clear_events(void) {
    for (PIC_TimerEvent *e = pg_events; e; e = e->next_registered)
        e->alarm_id = 0;
    pg_shim_irq_line = false;
}

} // extern "C"
