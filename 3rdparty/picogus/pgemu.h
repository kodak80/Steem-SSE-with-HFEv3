/*  Steem SSE: PicoGUS card emulation (ISA side), independent of Steem.

    Emulates a PicoGUS 2.0 running the multi-firmware ("pg-multi"): the
    control interface pgusinit talks to (ports 1D0-1D2) and the modes that
    make sound - GUS (GF1 + 1MB DRAM), Sound Blaster 2.0 DSP + OPL2, AdLib.
    The sound chips are the PicoGUS firmware's own code. MPU-401, PSG and
    USB modes answer on the control port only. There is no ISA DMA or IRQ
    (as with the Raven060's ISA bus under ISA_BIOS on other machines).

    Time is given in microseconds of emulated time; every access must give
    a time no earlier than the previous one.

    Copyright (C) 2026 - GPL v2 or later, like the PicoGUS sources.
*/
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Settings "flash" (what pgusinit /save writes): a text file. NULL: none.
void pgemu_set_flash_file(const char *path);

// Power on (cold boot of the card): settings from flash, boot into the
// saved mode.
void pgemu_power_on(uint64_t now_us);

// ISA I/O, 10-bit decode like the card.
uint8_t pgemu_inb(uint16_t port, uint64_t now_us);
void pgemu_outb(uint16_t port, uint8_t value, uint64_t now_us);

// Run the card's timers up to now_us without an access.
void pgemu_advance(uint64_t now_us);

// Render the card's audio output: frames stereo frames (L,R interleaved)
// at sample_rate. Audio is a stream separate from the access time; the
// caller renders up to "now" before each access.
void pgemu_render(int16_t *out, unsigned frames, unsigned sample_rate);

// running mode (card_mode_t: 1 GUS, 2 AdLib, 3 MPU, 4 PSG, 5 SB, 6 USB)
int pgemu_mode(void);

// state of the card's IRQ line (not wired to anything in Steem)
int pgemu_irq_line(void);

#ifdef __cplusplus
}
#endif
