/*  Steem SSE PicoGUS emulation: the PicoGUS SB firmware's Sound Blaster DSP
    (sw/sbdsp/sbdsp.cpp), built as in the "picogus-sb" firmware. */
#include "pg_rename.h"
#define SOUND_SB 1
#define SOUND_DSP 1
#define SB_BUFFERLESS 1
#define SB_BUFFERLESS_NG 1
#define PICO_ON_DEVICE 0
#include "pg_shim.h"
uint pg_LED_PIN = 25;
#include "sw/sbdsp/sbdsp.cpp"
