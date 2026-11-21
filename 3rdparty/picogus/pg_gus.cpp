/*  Steem SSE PicoGUS emulation: the PicoGUS GUS firmware's GF1 emulation
    (sw/gus/gus-x.cpp, from DOSBox-X), built as in the "picogus-gus"
    firmware: PicoGUS 2.0 with 1MB PSRAM. */
#include "pg_rename.h"
#define SOUND_GUS 1
#define PSRAM 1
#define SCALE_22K_TO_44K 1
#define INTERP_CLAMP 1
#define DACK_PIN 0
#define PICO_ON_DEVICE 0
#include "hardware/interp.h"
pg_interp_hw pg_interp0, pg_interp1;
#include "psram_spi.h"
psram_spi_inst_t psram_spi;
#include "isa/isa_dma.h"
dma_inst_t dma_config;
#include "sw/gus/gus-x.cpp"

// --- Steem: entry points with access to gus-x.cpp's statics ---------------

void pg_gus_power_on(void) {
    if (test) { // firmware restarted: free the previous instance
        delete test;
        test = NULL;
    }
    memset(pg_shim_psram, 0, PG_SHIM_PSRAM_SIZE); // as picogus.cpp main()
    GUS_OnReset();
    GUS_Setup();
}

uint8_t pg_gus_read(uint16_t port) {
    return (uint8_t)read_gus(port);
}

void pg_gus_write(uint16_t port, uint8_t value) {
    write_gus(port, value);
}

// the GF1's output rate: 617400 Hz / active voices (44.1kHz from 14 voices
// down), or 44.1kHz in fixed mode - the rate the firmware clocks its DAC at
uint32_t pg_gus_rate(void) {
    return myGUS.basefreq;
}
