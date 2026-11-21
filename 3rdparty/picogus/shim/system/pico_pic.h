/*  Steem PicoGUS shim: replaces sw/system/pico_pic.h.
    Timer events run on emulated time (pg_shim_run_events), the IRQ pin is
    a flag the ISA bus glue reads. Same API as the firmware's version. */
#pragma once
#include <stdint.h>
#include "../pico/time.h"
#include "../hardware/gpio.h"
#include "include/dosbox-x-compat.h"

#define IRQ_PIN PG_SHIM_IRQ_PIN

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t (* PIC_EventHandler)(Bitu val);

typedef struct PIC_TimerEvent {
    PIC_EventHandler handler;
    Bitu value;
    int alarm_id;              /* non-zero while scheduled */
    uint64_t expire_us;        /* shim only */
    struct PIC_TimerEvent *next_registered; /* shim only */
    bool registered;           /* shim only */
} PIC_TimerEvent;

static inline void PIC_ActivateIRQ(void) { gpio_put(IRQ_PIN, 1); }
static inline void PIC_DeActivateIRQ(void) { gpio_put(IRQ_PIN, 0); }

void PIC_AddEvent(PIC_TimerEvent* event, uint32_t delay, Bitu val);
void PIC_RemoveEvent(PIC_TimerEvent* event);
void PIC_Init(void);
double PIC_FullIndex(void);

/* shim: run every event due up to now_us, then set the clock to now_us */
void pg_shim_run_events(uint64_t now_us);
/* shim: cancel every event (card reboot) */
void pg_shim_clear_events(void);

#ifdef __cplusplus
}
#endif
