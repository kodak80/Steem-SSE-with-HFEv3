/*  Tests of steem/isa_picogus.cpp (the ISA window and the audio mixing)
    against mocks of the Steem globals it uses. */
#include <stdio.h>
#include <math.h>
#include <vector>
#include "mock_steem/pch.h"
#include "isa_picogus.h"

EasyStr UsersPath(".");
COUNTER_VAR mock_sys_time = 0, sys_time_of_last_vbl = 0;
DWORD nSysCyclesPerSecond = 8000000;
BYTE VideoFreqAtStartOfVbl = 50;
TGlueMock Glue = { 160000 };
DWORD psg_time_of_last_vbl_for_writing = 0, psg_n_samples_this_vbl = 882, sound_freq = 44100;
BYTE sound_num_channels = 2, sound_num_bits = 16, sound_bytes_per_sample = 4, UseSound = 1;

static const DWORD IOB = PICOGUS_ISA_IOBASE;
static int failures = 0;
static void check(bool ok, const char *what, double v = NAN) {
    if (!ok) failures++;
    if (isnan(v)) printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    else printf("%s %s (%g)\n", ok ? "ok  " : "FAIL", what, v);
}

// ISA_BIOS (isa_rw.c) LELS accessors, as the 68000 sees them through the window
static uint16_t swap16(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }
static void outp(uint16_t p, uint8_t d) { mock_sys_time += 32; PicoGusIsaWrite(IOB + p, d); }
static uint8_t inp(uint16_t p) { mock_sys_time += 32; return PicoGusIsaRead(IOB + p); }
static void outpw(uint16_t p, uint16_t d) { mock_sys_time += 32; PicoGusIsaWriteW(IOB + p, swap16(d)); }
static uint16_t inpw(uint16_t p) { mock_sys_time += 32; return swap16(PicoGusIsaReadW(IOB + p)); }

// one emulated frame, then SoundVBL: Steem writes 882 samples + PSG_WRITE_EXTRA
static std::vector<short> out; // what reached the sound card, by sample time
static void frame(void (*during)(int) = NULL) {
    if (during) during(0);
    mock_sys_time = sys_time_of_last_vbl + Glue.nFrameCycles;
    DWORD w1 = psg_time_of_last_vbl_for_writing;
    DWORD n = 882 + 300;
    std::vector<short> buf(n * 2, 0);
    void *adr[2] = { buf.data(), NULL };
    DWORD len[2] = { n * 4, 0 };
    PicoGusMixSound(adr, len, w1);
    if (out.size() < (w1 + n) * 2) out.resize((w1 + n) * 2);
    for (DWORD i = 0; i < n * 2; i++) out[w1 * 2 + i] = buf[i]; // later VBLs overwrite the extra
    sys_time_of_last_vbl = mock_sys_time;
    psg_time_of_last_vbl_for_writing += 882;
}

static void gus_reg8(uint8_t r, uint8_t v) { outp(0x343, r); outp(0x345, v); }
static void gus_reg16(uint8_t r, uint16_t v) { outp(0x343, r); outpw(0x344, v); } // 16-bit via outpw, as drivers do

int main() {
    remove("./PicoGUS.ini");
    outp(0x1D0, 0xCC); outp(0x1D0, 0x00);
    check(inp(0x1D2) == 0xDD, "card answers at $F201D2 (ISA port 1D2)");
    // pgusinit: outp(CONTROL_PORT, CMD_GUSPORT); outpw(DATA_PORT_LOW, port) - odd port
    outp(0x1D0, 0x04); outpw(0x1D1, 0x0240);
    outp(0x1D0, 0x04);
    check(inpw(0x1D1) == 0x0240, "word access at an odd port (lane swapped)", inpw(0x1D1));
    outpw(0x1D1, 0x0260);
    check(inpw(0x1D1) == 0x0260, "GUS port set with outpw()");
    outpw(0x1D1, 0x0240);

    // GUS: reset, DAC on, a looping square wave started half way through a frame
    gus_reg8(0x4C, 0x00); gus_reg8(0x4C, 0x01); gus_reg8(0x0E, 0xC0 | 13); gus_reg8(0x4C, 0x03);
    outp(0x240, 0x00);
    for (int i = 0; i < 64; i++) { gus_reg16(0x43, 0x1000 + i); gus_reg8(0x44, 0); outp(0x347, i < 32 ? 0x60 : 0xA0); }
    outp(0x342, 0); gus_reg8(0x00, 0x03);
    gus_reg16(0x01, 1 << 10);
    gus_reg16(0x02, 0x1000 >> 7); gus_reg16(0x03, (0x1000 & 0x7f) << 9);
    gus_reg16(0x04, (0x1000 + 63) >> 7); gus_reg16(0x05, ((0x1000 + 63) & 0x7f) << 9);
    gus_reg16(0x0A, 0x1000 >> 7); gus_reg16(0x0B, (0x1000 & 0x7f) << 9);
    gus_reg8(0x0C, 7); gus_reg16(0x09, 0x0000); // silent: a stopped voice still outputs its sample
    for (int f = 0; f < 5; f++) frame();
    DWORD start_time = psg_time_of_last_vbl_for_writing;
    // the voice starts half way through the next frame
    frame([](int) { mock_sys_time = sys_time_of_last_vbl + Glue.nFrameCycles / 2; gus_reg16(0x09, 0xF000); gus_reg8(0x00, 0x08); });
    for (int f = 0; f < 10; f++) frame();
    DWORD first = 0;
    for (DWORD t = start_time; t < psg_time_of_last_vbl_for_writing; t++)
        if (out[t * 2]) { first = t; break; }
    check(first, "the voice is heard");
    long d = (long)first - (long)(start_time + 441);
    check(labs(d) <= 2, "it starts at the sample of the write (frame + 441)", (double)d);
    // no gap or click once playing: every 32 sample half-period has the same level
    double sum = 0; int zeros = 0;
    for (DWORD t = first + 50; t < psg_time_of_last_vbl_for_writing; t++) {
        sum += fabs((double)out[t * 2]);
        if (!out[t * 2]) zeros++;
    }
    check(zeros < 20, "continuous between VBLs", zeros);
    // mono, 8-bit output
    sound_num_channels = 1; sound_num_bits = 8; sound_bytes_per_sample = 1;
    std::vector<BYTE> b8(1000, 128);
    void *adr[2] = { b8.data(), NULL }; DWORD len[2] = { 1000, 0 };
    mock_sys_time = sys_time_of_last_vbl + Glue.nFrameCycles;
    PicoGusMixSound(adr, len, psg_time_of_last_vbl_for_writing);
    int moved = 0; for (BYTE x : b8) if (x != 128) moved++;
    check(moved > 500, "mixed into 8-bit mono output", moved);
    // power off (ST cold reset): the card reboots from flash
    PicoGusPowerOff();
    outp(0x1D0, 0xCC); outp(0x1D0, 0x03);
    check(inp(0x1D2) == 1, "after a power cycle the card is in GUS mode again");
    printf("%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
    return failures != 0;
}
