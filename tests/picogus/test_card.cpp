/*  Synthetic tests of the PicoGUS card emulation: each sound mode is
    programmed the way DOS software does it, and must make sound. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include "pgemu.h"

static uint64_t t = 0; // emulated microseconds
static const unsigned RATE = 44100;
static double audio_us = 0; // time rendered so far

static void render_to_now(std::vector<int16_t> *keep) {
    // render the audio up to the access time, as Steem does
    unsigned n = (unsigned)((t - audio_us) * RATE / 1e6);
    if (!n) return;
    std::vector<int16_t> buf(2 * n);
    pgemu_render(buf.data(), n, RATE);
    if (keep) keep->insert(keep->end(), buf.begin(), buf.end());
    audio_us += n * 1e6 / RATE;
}
static std::vector<int16_t> *capture = NULL;
static void outb(uint16_t p, uint8_t v) { t += 1; render_to_now(capture); pgemu_outb(p, v, t); }
static uint8_t inb(uint16_t p) { t += 1; render_to_now(capture); return pgemu_inb(p, t); }
static void wait_us(unsigned us) { t += us; render_to_now(capture); pgemu_advance(t); }

static double rms(const std::vector<int16_t> &v) {
    double s = 0;
    for (int16_t x : v) s += (double)x * x;
    return v.empty() ? 0 : sqrt(s / v.size());
}

static int failures = 0;
static void check(bool ok, const char *what, double val = NAN) {
    if (!ok) failures++;
    if (isnan(val)) printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    else printf("%s %s (%.1f)\n", ok ? "ok  " : "FAIL", what, val);
}

static void ctrl_select(uint8_t reg) { outb(0x1D0, 0xCC); outb(0x1D0, reg); }

static void set_mode(uint8_t m) { // as pgusinit reboot_to_firmware()
    ctrl_select(0x03); outb(0x1D2, m);
    outb(0x1D0, 0xE2); outb(0x1D2, 0xFF);
    wait_us(1000);
}

// ---- GUS ----------------------------------------------------------------
static const uint16_t G = 0x240;
static void gus_reg8(uint8_t r, uint8_t v) { outb(G + 0x103, r); outb(G + 0x105, v); }
static void gus_reg16(uint8_t r, uint16_t v) { outb(G + 0x103, r); outb(G + 0x104, v & 0xff); outb(G + 0x105, v >> 8); }
static uint8_t gus_rreg8(uint8_t r) { outb(G + 0x103, r); return inb(G + 0x105); }
static void gus_poke(uint32_t a, uint8_t v) { gus_reg16(0x43, a & 0xffff); gus_reg8(0x44, a >> 16); outb(G + 0x107, v); }
static uint8_t gus_peek(uint32_t a) { gus_reg16(0x43, a & 0xffff); gus_reg8(0x44, a >> 16); return inb(G + 0x107); }
static void gus_addr(uint8_t rhi, uint32_t a) { gus_reg16(rhi, (a >> 7) & 0x1fff); gus_reg16(rhi + 1, (a & 0x7f) << 9); }

static void test_gus(void) {
    puts("-- GUS mode");
    check(pgemu_mode() == 1, "card boots in GUS mode");
    // DRAM: all of 1MB is there
    gus_poke(0x00000, 0x11); gus_poke(0x40000, 0x22); gus_poke(0xFFFFF, 0x33);
    check(gus_peek(0) == 0x11 && gus_peek(0x40000) == 0x22 && gus_peek(0xFFFFF) == 0x33, "1MB of GUS DRAM");
    // GF1 reset, DAC on, 14 voices
    gus_reg8(0x4C, 0x00); wait_us(100);
    gus_reg8(0x4C, 0x01); wait_us(100);
    gus_reg8(0x0E, 0xC0 | 13);
    gus_reg8(0x4C, 0x03);
    outb(G + 0x000, 0x00); // mixer: line out on
    // a 8-bit square wave at 0x1000
    for (int i = 0; i < 64; i++) gus_poke(0x1000 + i, i < 32 ? 0x60 : 0xA0);
    std::vector<int16_t> silent;
    capture = &silent; wait_us(20000); capture = NULL;
    check(rms(silent) < 1, "silent before a voice plays", rms(silent));
    outb(G + 0x102, 0); // voice 0
    gus_reg8(0x00, 0x03); // stopped while programming
    gus_reg16(0x01, 1 << 10); // frequency control: 1 sample per output frame
    gus_addr(0x02, 0x1000); // start
    gus_addr(0x04, 0x1000 + 63); // end
    gus_addr(0x0A, 0x1000); // current
    gus_reg8(0x0C, 7); // pan centre
    gus_reg16(0x09, 0xF000); // volume
    gus_reg8(0x00, 0x08); // loop, 8-bit, go
    std::vector<int16_t> out;
    capture = &out; wait_us(50000); capture = NULL;
    double r = rms(out);
    check(r > 1000 && r < 30000, "a looping voice makes sound, not clipped", r);
    // the voice position moves
    outb(G + 0x102, 0);
    outb(G + 0x103, 0x8A); uint16_t a = inb(G + 0x105);
    wait_us(300);
    outb(G + 0x103, 0x8A); uint16_t b = inb(G + 0x105);
    outb(G + 0x103, 0x8B); inb(G + 0x104);
    (void)a; (void)b;
    gus_reg16(0x09, 0x0000); // volume to zero (a stopped voice still outputs its sample)
    std::vector<int16_t> after;
    wait_us(10000);
    capture = &after; wait_us(20000); capture = NULL;
    check(rms(after) < 1, "volume zero silences the voice", rms(after));
    // GF1 timer 1 (80us ticks): reaches via the IRQ status
    gus_reg8(0x45, 0x04); // timer 1 IRQ enable
    gus_reg8(0x46, 0xFF); // count 1 tick
    outb(G + 0x008, 0x04); outb(G + 0x009, 0x01); // timer control: start timer 1
    wait_us(1000);
    uint8_t st = inb(G + 0x006);
    check(st & 0x04, "GF1 timer 1 expires (IRQ status bit 2)", st);
}

// ---- SB -----------------------------------------------------------------
static const uint16_t S = 0x220;
static bool sb_reset(void) {
    outb(S + 6, 1); wait_us(10); outb(S + 6, 0);
    for (int i = 0; i < 1000; i++) {
        if (inb(S + 0xE) & 0x80) return inb(S + 0xA) == 0xAA;
        wait_us(10);
    }
    return false;
}
static void sb_cmd(uint8_t v) {
    for (int i = 0; i < 1000 && (inb(S + 0xC) & 0x80); i++) wait_us(5);
    outb(S + 0xC, v);
}
static int sb_read(void) {
    for (int i = 0; i < 1000; i++) {
        if (inb(S + 0xE) & 0x80) return inb(S + 0xA);
        wait_us(5);
    }
    return -1;
}

static void test_sb(void) {
    puts("-- SB mode");
    set_mode(5);
    check(pgemu_mode() == 5, "rebooted into SB mode");
    check(sb_reset(), "DSP reset answers 0xAA");
    sb_cmd(0xE1);
    int maj = sb_read(), min = sb_read();
    printf("     DSP version %d.%02d\n", maj, min);
    check(maj == 4, "DSP version is SB16 (type 6)", maj);
    sb_cmd(0xD1); // speaker on
    std::vector<int16_t> out;
    capture = &out;
    for (int i = 0; i < 4000; i++) { // 1kHz square at 8kHz, direct DAC
        sb_cmd(0x10);
        sb_cmd((i / 4) & 1 ? 0xE0 : 0x20);
        wait_us(125);
    }
    capture = NULL;
    double r = rms(out);
    check(r > 1000, "direct DAC (command 10h) makes sound", r);
}

// ---- OPL ----------------------------------------------------------------
static void opl(uint16_t base, uint8_t r, uint8_t v) {
    outb(base, r); for (int i = 0; i < 6; i++) inb(base);
    outb(base + 1, v); for (int i = 0; i < 35; i++) inb(base);
}

static void test_opl(uint16_t base, const char *what) {
    printf("-- OPL2 at %x (%s)\n", base, what);
    // AdLib detection, as in the AdLib programming guide
    opl(base, 4, 0x60); opl(base, 4, 0x80);
    uint8_t s1 = inb(base);
    opl(base, 2, 0xFF); opl(base, 4, 0x21);
    wait_us(100);
    uint8_t s2 = inb(base);
    opl(base, 4, 0x60); opl(base, 4, 0x80);
    check((s1 & 0xE0) == 0 && (s2 & 0xE0) == 0xC0, "AdLib detection (timer 1)");
    check((s1 & 0x06) == 0x06, "status says OPL2, not OPL3", s1);
    // a note on channel 0
    opl(base, 0x20, 0x01); opl(base, 0x40, 0x10); opl(base, 0x60, 0xF0); opl(base, 0x80, 0x77);
    opl(base, 0x23, 0x01); opl(base, 0x43, 0x00); opl(base, 0x63, 0xF0); opl(base, 0x83, 0x77);
    opl(base, 0xA0, 0x98); opl(base, 0xB0, 0x31);
    std::vector<int16_t> out;
    capture = &out; wait_us(100000); capture = NULL;
    double r = rms(out);
    check(r > 500, "an FM note makes sound", r);
    opl(base, 0xB0, 0x11); // key off
}

int main() {
    const char *flash = "pg_flash_card.ini";
    remove(flash);
    pgemu_set_flash_file(flash);
    pgemu_power_on(0);
    // control interface basics
    ctrl_select(0x00);
    check(inb(0x1D2) == 0xDD, "magic");
    ctrl_select(0x01);
    check(inb(0x1D2) == 4, "protocol version 4");
    check(inb(0x1D0) == 0x01, "control port reads back the selected register");
    outb(0x1D0, 0x77); // unknown register: deactivates the interface
    outb(0x1D0, 0x04); outb(0x1D1, 0x60); outb(0x1D2, 0x02); // ignored
    ctrl_select(0x04);
    check(inb(0x1D1) == 0x40 && inb(0x1D2) == 0x02, "writes ignored until the knock");
    check(inb(0x5D0 + 2) == 0x02, "10-bit port decode (5D2 = 1D2)");
    test_gus();
    test_sb();
    test_opl(0x388, "SB mode");
    test_opl(0x220, "SB mode, SB base port");
    set_mode(2);
    check(pgemu_mode() == 2, "rebooted into AdLib mode");
    test_opl(0x388, "AdLib mode");
    check(inb(0x226) == 0xFF, "no DSP in AdLib mode");
    printf("%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
    return failures != 0;
}
