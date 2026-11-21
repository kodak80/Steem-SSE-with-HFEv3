/*  Steem SSE: PicoGUS card emulation, see pgemu.h.

    The control interface (ports 1D0-1D2), the port decode and the audio
    mixing follow the PicoGUS firmware: sw/picogus.cpp, sw/gusplay.cpp,
    sw/sbplay.cpp, sw/system/flash_settings.c and multifw/bootloader.c
    (Copyright (C) 2022-2025 Ian Scott and contributors).

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/
#include "pg_rename.h"
#include "pgemu.h"
#include "pg_opl.h"
#include "system/pico_pic.h"
#include "system/flash_settings.h"
#include "common/picogus.h"
#include "audio/volctrl.h"
#include "sbdsp/sbdsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// pg_gus.cpp / gus-x.cpp
void pg_gus_power_on(void);
uint8_t pg_gus_read(uint16_t port);
void pg_gus_write(uint16_t port, uint8_t value);
uint32_t pg_gus_rate(void);
extern uint32_t GUS_sample_stereo(void);
extern void GUS_SetFixed44k(const bool new_force44k);
extern void GUS_SetAudioBuffer(const uint16_t new_buffer_size);
extern void GUS_SetDMAInterval(const uint16_t newInterval);

// Firmware version reported to pgusinit: the PicoGUS release the emulated
// code comes from (see README-STEEM.txt).
#define PG_VERSION "4.2.0-beta.3"

Settings pg_settings; // the running firmware's settings (RAM)

static const char *flash_path = NULL;
static card_mode_t mode = INVALID_MODE; // running firmware

// control interface (picogus.cpp)
static bool control_active = false;
static uint8_t sel_reg = 0;
static uint32_t cur_read = 0;
static uint32_t cur_write = 0;
static uint8_t basePort_low;
static uint8_t mouseSensitivity_low;
static char firmware_string[64];

// port decode
static uint16_t gus_port_test = 0xFFFF;
static uint16_t sb_port_test = 0xFFFF;
static uint16_t opl_addr = 0;

// CD-ROM image manager of the SB firmware, without a USB drive
static uint8_t cd_status = 0; // CD_STATUS_IDLE
static const char cd_error[] = "no USB drive (emulated PicoGUS)";


/////////////////////////////
// Settings (flash_settings.c)
/////////////////////////////

void getDefaultSettings(Settings *s) {
    memset(s, 0, sizeof(*s));
    s->magic = SETTINGS_MAGIC;
    s->version = SETTINGS_VERSION;
    s->startupMode = 1;
    s->Global.waveTableVolume = 100;
    s->Joy.basePort = 0xffff;
    s->GUS.basePort = 0x240;
    s->GUS.audioBuffer = 4;
    s->GUS.dmaInterval = 0;
    s->GUS.force44k = false;
    s->SB.basePort = 0x220;
    s->SB.oplBasePort = 0x388;
    s->SB.oplSpeedSensitive = false;
    s->MPU.basePort = 0x330;
    s->MPU.delaySysex = false;
    s->MPU.fakeAllNotesOff = false;
    s->MPU.waitBusyStatus = false;
    s->CMS.basePort = 0x220;
    s->Tandy.basePort = 0x2c0;
    s->Mouse.basePort = 0xffff;
    s->Mouse.protocol = 0;
    s->Mouse.reportRate = 60;
    s->Mouse.sensitivity = 0x100;
    s->NE2K.basePort = 0x300;
    s->CD.basePort = 0x250;
    s->CD.autoAdvance = true;
    s->MMB.basePort = 0xffff;
    s->Volume.mainVol = 100;
    s->Volume.oplVol = 100;
    s->Volume.sbVol = 90;
    s->Volume.cdVol = 100;
    s->Volume.gusVol = 100;
    s->Volume.psgVol = 80;
    s->SB16.irq = 5;
    s->SB16.dma = 1;
    s->SB16.sbType = 6;
    s->SB16.options = 0;
}

// The flash is a text file, one "key=value" per line (decimal or 0x hex).
// Unknown keys are ignored and missing ones keep their default, like the
// firmware's settings migration.

#define PG_FIELDS(X) \
    X(startupMode, startupMode) \
    X(waveTableVolume, Global.waveTableVolume) \
    X(joyPort, Joy.basePort) \
    X(gusPort, GUS.basePort) \
    X(gusBuffer, GUS.audioBuffer) \
    X(gusDma, GUS.dmaInterval) \
    X(gus44k, GUS.force44k) \
    X(sbPort, SB.basePort) \
    X(oplPort, SB.oplBasePort) \
    X(oplSpeedSensitive, SB.oplSpeedSensitive) \
    X(mpuPort, MPU.basePort) \
    X(mpuDelaySysex, MPU.delaySysex) \
    X(mpuFakeAllNotesOff, MPU.fakeAllNotesOff) \
    X(mpuWaitBusyStatus, MPU.waitBusyStatus) \
    X(cmsPort, CMS.basePort) \
    X(tandyPort, Tandy.basePort) \
    X(mousePort, Mouse.basePort) \
    X(mouseProtocol, Mouse.protocol) \
    X(mouseReportRate, Mouse.reportRate) \
    X(mouseSensitivity, Mouse.sensitivity) \
    X(ne2kPort, NE2K.basePort) \
    X(cdPort, CD.basePort) \
    X(cdAutoAdvance, CD.autoAdvance) \
    X(mmbPort, MMB.basePort) \
    X(mainVol, Volume.mainVol) \
    X(oplVol, Volume.oplVol) \
    X(sbVol, Volume.sbVol) \
    X(cdVol, Volume.cdVol) \
    X(gusVol, Volume.gusVol) \
    X(psgVol, Volume.psgVol) \
    X(sb16Irq, SB16.irq) \
    X(sb16Dma, SB16.dma) \
    X(sbType, SB16.sbType) \
    X(sbOptions, SB16.options)

void loadSettings(Settings *s, bool migrate) {
    (void)migrate;
    getDefaultSettings(s);
    if (!flash_path)
        return;
    FILE *f = fopen(flash_path, "r");
    if (!f)
        return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#' || line[0] == ';')
            continue;
        *eq = 0;
        char *key = line;
        while (*key == ' ' || *key == '\t') key++;
        char *end = eq - 1;
        while (end >= key && (*end == ' ' || *end == '\t')) *end-- = 0;
        long v = strtol(eq + 1, NULL, 0);
#define PG_LOAD(name, field) if (!strcmp(key, #name)) { s->field = v; continue; }
        PG_FIELDS(PG_LOAD)
#undef PG_LOAD
        if (!strcmp(key, "wifiSsid") || !strcmp(key, "wifiPassword")) {
            char *val = eq + 1, *nl = strpbrk(val, "\r\n");
            if (nl) *nl = 0;
            if (key[4] == 'S')
                snprintf(s->WiFi.ssid, sizeof(s->WiFi.ssid), "%s", val);
            else
                snprintf(s->WiFi.password, sizeof(s->WiFi.password), "%s", val);
        }
    }
    fclose(f);
}

void saveSettings(const Settings *s) {
    if (!flash_path)
        return;
    FILE *f = fopen(flash_path, "w");
    if (!f)
        return;
    fprintf(f, "# Emulated PicoGUS flash settings (Steem SSE), written by pgusinit /save\n");
#define PG_SAVE(name, field) fprintf(f, "%s=%ld\n", #name, (long)s->field);
    PG_FIELDS(PG_SAVE)
#undef PG_SAVE
    fprintf(f, "wifiSsid=%s\nwifiPassword=%s\n", s->WiFi.ssid, s->WiFi.password);
    fclose(f);
}

void pgemu_set_flash_file(const char *path) {
    free((void*)flash_path);
    flash_path = NULL;
    if (path) {
        size_t n = strlen(path) + 1;
        char *copy = (char*)malloc(n);
        if (copy) memcpy(copy, path, n);
        flash_path = copy;
    }
}


///////////////////
// Firmware boot //
///////////////////

static const char *mode_program_name(card_mode_t m) {
    switch (m) {
    case GUS_MODE: return "picogus-gus";
    case ADLIB_MODE: return "picogus-adlib";
    case MPU_MODE: return "picogus-mpu401";
    case PSG_MODE: return "picogus-psg";
    case SB_MODE: return "picogus-sb";
    case USB_MODE: return "picogus-usb";
    default: return "picogus";
    }
}

static bool mode_has_opl(void) {
    return mode == SB_MODE || mode == ADLIB_MODE;
}

// picogus.cpp processSettings()
static void processSettings(void) {
    settings.startupMode = mode;
    switch (mode) {
    case GUS_MODE:
        set_volume(CMD_GUSVOL);
        break;
    case PSG_MODE:
        set_volume(CMD_PSGVOL);
        break;
    case SB_MODE:
        set_volume(CMD_SBVOL);
        set_volume(CMD_OPLVOL);
        break;
    case ADLIB_MODE:
        set_volume(CMD_OPLVOL);
        break;
    default:
        break;
    }
    sb_port_test = (mode == SB_MODE) ? (settings.SB.basePort >> 4) : 0xFFFF;
    if (mode == GUS_MODE) {
        gus_port_test = settings.GUS.basePort >> 4 | 0x10;
        GUS_SetFixed44k(settings.GUS.force44k);
        GUS_SetAudioBuffer(settings.GUS.audioBuffer);
        GUS_SetDMAInterval(settings.GUS.dmaInterval);
    } else
        gus_port_test = 0xFFFF;
}

// Start firmware number fw (card_mode_t), as the multifw bootloader does;
// settings come from flash, so unsaved changes are lost, as on the card.
static void boot_firmware(card_mode_t fw, uint64_t now_us) {
    pg_shim_clear_events();
    pg_shim_run_events(now_us);
    loadSettings(&settings, true);
    if (fw < GUS_MODE || fw > USB_MODE) // NR_OF_FIRMWARES = 6
        fw = GUS_MODE;
    mode = fw;
    snprintf(firmware_string, sizeof(firmware_string), "%s v%s (SteemSSE)",
        mode_program_name(mode), PG_VERSION);
    control_active = false;
    sel_reg = 0;
    cur_read = cur_write = 0;
    opl_addr = 0;
    cd_status = 0;
    set_volume(CMD_MAINVOL); // sets the master and every source volume
    switch (mode) {
    case GUS_MODE:
        pg_gus_power_on();
        break;
    case SB_MODE:
        sbdsp_init();
        sbdsp_set_type(settings.SB16.sbType);
        sbdsp_set_irq(settings.SB16.irq);
        sbdsp_set_dma(settings.SB16.dma);
        sbdsp_set_options(settings.SB16.options);
        pg_opl_init(44100);
        break;
    case ADLIB_MODE:
        pg_opl_init(44100);
        break;
    default:
        break;
    }
    processSettings();
}

void pgemu_power_on(uint64_t now_us) {
    Settings flash;
    loadSettings(&flash, false);
    // bootloader.c read_permMode()
    card_mode_t fw = (flash.startupMode >= 1 && flash.startupMode <= 6)
        ? (card_mode_t)flash.startupMode : GUS_MODE;
    boot_firmware(fw, now_us);
}


///////////////////////
// Control interface //
///////////////////////

static void select_picogus(uint8_t value) {
    sel_reg = value;
    switch (sel_reg) {
    case CMD_MAGIC: case CMD_PROTOCOL: case CMD_BOOTMODE: case CMD_JOYEN:
    case CMD_GUSBUF: case CMD_GUSDMA: case CMD_GUS44K:
    case CMD_WTVOL: case CMD_MPUDELAY: case CMD_MPUFAKE: case CMD_MPUWAIT:
    case CMD_OPLWAIT: case CMD_MOUSEPROTO: case CMD_MOUSERATE: case CMD_MOUSESEN:
    case CMD_WIFIAPPLY: case CMD_WIFISTAT:
    case CMD_SBTYPE: case CMD_SBIRQ: case CMD_SBDMA: case CMD_SBOPTS:
    case CMD_CDSTATUS: case CMD_CDLOAD: case CMD_CDAUTOADV:
    case CMD_MAINVOL: case CMD_OPLVOL: case CMD_SBVOL: case CMD_CDVOL:
    case CMD_GUSVOL: case CMD_PSGVOL:
    case CMD_SAVE: case CMD_REBOOT: case CMD_DEFAULTS:
    case CMD_HWTYPE:
        break;
    case CMD_FWSTRING:
        cur_read = 0;
        break;
    case CMD_GUSPORT: case CMD_OPLPORT: case CMD_SBPORT: case CMD_MPUPORT:
    case CMD_TANDYPORT: case CMD_CMSPORT: case CMD_MOUSEPORT: case CMD_NE2KPORT:
        basePort_low = 0;
        break;
    case CMD_WIFISSID:
        memset(settings.WiFi.ssid, 0, sizeof(settings.WiFi.ssid));
        cur_write = 0;
        break;
    case CMD_WIFIPASS:
        memset(settings.WiFi.password, 0, sizeof(settings.WiFi.password));
        cur_write = 0;
        break;
    case CMD_CDPORT:
        basePort_low = 0;
        break;
    case CMD_CDLIST:
        if (mode == SB_MODE)
            cd_status = 0; // listing a missing drive completes at once
        cur_read = 0;
        break;
    case CMD_CDNAME:
        cur_write = 0;
        cur_read = 0;
        break;
    case CMD_CDERROR:
        cur_read = 0;
        break;
    case CMD_FLASH: // firmware update: not possible on the emulated card
    default:
        control_active = false;
        break;
    }
}

static void write_picogus_low(uint8_t value) {
    switch (sel_reg) {
    case CMD_GUSPORT: case CMD_OPLPORT: case CMD_SBPORT: case CMD_MPUPORT:
    case CMD_TANDYPORT: case CMD_CMSPORT: case CMD_MOUSEPORT: case CMD_CDPORT:
        basePort_low = value;
        break;
    case CMD_MOUSESEN:
        mouseSensitivity_low = value;
        break;
    }
}

static uint16_t port_from(uint8_t value) {
    return (value || basePort_low) ? (uint16_t)((value << 8) | basePort_low) : 0xFFFF;
}

static void write_picogus_high(uint8_t value, uint64_t now_us) {
    switch (sel_reg) {
    case CMD_GUSPORT:
        settings.GUS.basePort = port_from(value);
        if (mode == GUS_MODE)
            gus_port_test = settings.GUS.basePort >> 4 | 0x10;
        break;
    case CMD_OPLPORT:
        settings.SB.oplBasePort = port_from(value);
        break;
    case CMD_SBPORT:
        settings.SB.basePort = port_from(value);
        if (mode == SB_MODE)
            sb_port_test = settings.SB.basePort >> 4;
        break;
    case CMD_MPUPORT:
        settings.MPU.basePort = port_from(value);
        break;
    case CMD_TANDYPORT:
        settings.Tandy.basePort = port_from(value);
        break;
    case CMD_CMSPORT:
        settings.CMS.basePort = port_from(value);
        break;
    case CMD_JOYEN:
        settings.Joy.basePort = value ? 0x201u : 0xffff;
        break;
    case CMD_GUSBUF: // sent by pgusinit as the size - 1
        settings.GUS.audioBuffer = value + 1;
        if (mode == GUS_MODE)
            GUS_SetAudioBuffer(settings.GUS.audioBuffer);
        break;
    case CMD_GUSDMA:
        settings.GUS.dmaInterval = value;
        if (mode == GUS_MODE)
            GUS_SetDMAInterval(settings.GUS.dmaInterval);
        break;
    case CMD_GUS44K:
        settings.GUS.force44k = value;
        if (mode == GUS_MODE)
            GUS_SetFixed44k(settings.GUS.force44k);
        break;
    case CMD_WTVOL:
        settings.Global.waveTableVolume = value;
        break;
    case CMD_MPUDELAY:
        settings.MPU.delaySysex = value;
        break;
    case CMD_MPUFAKE:
        settings.MPU.fakeAllNotesOff = value;
        break;
    case CMD_MPUWAIT:
        settings.MPU.waitBusyStatus = value;
        break;
    case CMD_OPLWAIT:
        settings.SB.oplSpeedSensitive = value;
        break;
    case CMD_SBTYPE:
        settings.SB16.sbType = value;
        if (mode == SB_MODE) sbdsp_set_type(value);
        break;
    case CMD_SBIRQ:
        settings.SB16.irq = value;
        if (mode == SB_MODE) sbdsp_set_irq(value);
        break;
    case CMD_SBDMA:
        settings.SB16.dma = value;
        if (mode == SB_MODE) sbdsp_set_dma(value);
        break;
    case CMD_SBOPTS:
        settings.SB16.options = value;
        if (mode == SB_MODE) sbdsp_set_options(value);
        break;
    case CMD_MOUSEPORT:
        settings.Mouse.basePort = port_from(value);
        break;
    case CMD_MOUSEPROTO:
        settings.Mouse.protocol = value;
        break;
    case CMD_MOUSERATE:
        settings.Mouse.reportRate = value;
        break;
    case CMD_MOUSESEN:
        settings.Mouse.sensitivity = (int16_t)((value << 8) | (mouseSensitivity_low & 0xFF));
        break;
    case CMD_NE2KPORT:
        settings.NE2K.basePort = port_from(value);
        break;
    case CMD_WIFISSID:
        if (cur_write < sizeof(settings.WiFi.ssid))
            settings.WiFi.ssid[cur_write++] = value;
        break;
    case CMD_WIFIPASS:
        if (cur_write < sizeof(settings.WiFi.password))
            settings.WiFi.password[cur_write++] = value;
        break;
    case CMD_CDPORT:
        settings.CD.basePort = port_from(value);
        break;
    case CMD_CDLOAD:
    case CMD_CDNAME:
        if (mode == SB_MODE)
            cd_status = (uint8_t)CD_STATUS_ERROR; // no images to load
        break;
    case CMD_CDAUTOADV:
        settings.CD.autoAdvance = value;
        break;
    case CMD_MAINVOL:
        settings.Volume.mainVol = value;
        set_volume(CMD_MAINVOL);
        break;
    case CMD_OPLVOL:
        settings.Volume.oplVol = value;
        if (mode_has_opl()) set_volume(CMD_OPLVOL);
        break;
    case CMD_SBVOL:
        settings.Volume.sbVol = value;
        if (mode == SB_MODE) set_volume(CMD_SBVOL);
        break;
    case CMD_CDVOL:
        settings.Volume.cdVol = value;
        break;
    case CMD_GUSVOL:
        settings.Volume.gusVol = value;
        if (mode == GUS_MODE) set_volume(CMD_GUSVOL);
        break;
    case CMD_PSGVOL:
        settings.Volume.psgVol = value;
        if (mode == PSG_MODE) set_volume(CMD_PSGVOL);
        break;
    case CMD_BOOTMODE: // for multifw
        settings.startupMode = value;
        break;
    case CMD_SAVE:
        saveSettings(&settings);
        break;
    case CMD_REBOOT: // the bootloader starts the requested firmware
        boot_firmware((card_mode_t)settings.startupMode, now_us);
        break;
    case CMD_DEFAULTS:
        getDefaultSettings(&settings);
        processSettings();
        set_volume(CMD_MAINVOL);
        break;
    }
}

static uint8_t port_lo(uint16_t p) { return p == 0xFFFF ? 0 : (p & 0xFF); }
static uint8_t port_hi(uint16_t p) { return p == 0xFFFF ? 0 : (p >> 8); }

static uint8_t read_picogus_low(void) {
    switch (sel_reg) {
    case CMD_GUSPORT: return port_lo(settings.GUS.basePort);
    case CMD_OPLPORT: return port_lo(settings.SB.oplBasePort);
    case CMD_SBPORT: return port_lo(settings.SB.basePort);
    case CMD_MPUPORT: return port_lo(settings.MPU.basePort);
    case CMD_TANDYPORT: return port_lo(settings.Tandy.basePort);
    case CMD_CMSPORT: return port_lo(settings.CMS.basePort);
    case CMD_MOUSEPORT: return port_lo(settings.Mouse.basePort);
    case CMD_MOUSESEN: return settings.Mouse.sensitivity & 0xFF;
    case CMD_NE2KPORT: return port_lo(settings.NE2K.basePort);
    case CMD_CDPORT: return port_lo(settings.CD.basePort);
    default: return 0x0;
    }
}

static uint8_t read_string(const char *s) {
    uint8_t ret = (uint8_t)s[cur_read++];
    if (ret == 0)
        cur_read = 0;
    return ret;
}

static uint8_t read_picogus_high(void) {
    switch (sel_reg) {
    case CMD_MAGIC: return 0xdd;
    case CMD_PROTOCOL: return PICOGUS_PROTOCOL_VER;
    case CMD_FWSTRING: return read_string(firmware_string);
    case CMD_BOOTMODE: return settings.startupMode;
    case CMD_GUSPORT: return port_hi(settings.GUS.basePort);
    case CMD_OPLPORT: return port_hi(settings.SB.oplBasePort);
    case CMD_SBPORT: return port_hi(settings.SB.basePort);
    case CMD_MPUPORT: return port_hi(settings.MPU.basePort);
    case CMD_TANDYPORT: return port_hi(settings.Tandy.basePort);
    case CMD_CMSPORT: return port_hi(settings.CMS.basePort);
    case CMD_JOYEN: return settings.Joy.basePort == 0x201u;
    case CMD_GUSBUF: return settings.GUS.audioBuffer - 1;
    case CMD_GUSDMA: return settings.GUS.dmaInterval;
    case CMD_GUS44K: return settings.GUS.force44k;
    case CMD_WTVOL: return settings.Global.waveTableVolume; // PicoGUS 2.0
    case CMD_MPUDELAY: return settings.MPU.delaySysex;
    case CMD_MPUFAKE: return settings.MPU.fakeAllNotesOff;
    case CMD_MPUWAIT: return settings.MPU.waitBusyStatus;
    case CMD_OPLWAIT: return settings.SB.oplSpeedSensitive;
    case CMD_MOUSEPORT: return port_hi(settings.Mouse.basePort);
    case CMD_MOUSEPROTO: return settings.Mouse.protocol;
    case CMD_MOUSERATE: return settings.Mouse.reportRate;
    case CMD_MOUSESEN: return (uint16_t)settings.Mouse.sensitivity >> 8;
    case CMD_NE2KPORT: return port_hi(settings.NE2K.basePort);
    case CMD_SBTYPE: return settings.SB16.sbType;
    case CMD_SBIRQ: return settings.SB16.irq;
    case CMD_SBDMA: return settings.SB16.dma;
    case CMD_SBOPTS: return settings.SB16.options;
    case CMD_WIFISTAT: return 0; // not a Pico W
    case CMD_CDPORT: return port_hi(settings.CD.basePort);
    case CMD_CDSTATUS:
        if (mode == SB_MODE) return cd_status;
        break;
    case CMD_CDLIST:
        if (mode == SB_MODE) { cur_read = 0; return 0x04; } // EOT: no images
        break;
    case CMD_CDLOAD:
        if (mode == SB_MODE) return 0; // no image loaded
        break;
    case CMD_CDNAME:
        if (mode == SB_MODE) return read_string("");
        break;
    case CMD_CDERROR:
        if (mode == SB_MODE) return read_string(cd_error);
        break;
    case CMD_CDAUTOADV: return settings.CD.autoAdvance;
    case CMD_MAINVOL: return settings.Volume.mainVol;
    case CMD_OPLVOL: return settings.Volume.oplVol;
    case CMD_SBVOL: return settings.Volume.sbVol;
    case CMD_CDVOL: return settings.Volume.cdVol;
    case CMD_GUSVOL: return settings.Volume.gusVol;
    case CMD_PSGVOL: return settings.Volume.psgVol;
    case CMD_HWTYPE: return PICOGUS_2;
    default:
        break;
    }
    return 0xff;
}


/////////////////
// Port decode //
/////////////////

static void sb_pump(void) {
    // the SB firmware runs sbdsp_process() in a loop on its second core
    for (int i = 0; i < 4; i++)
        sbdsp_process();
}

void pgemu_advance(uint64_t now_us) {
    pg_shim_run_events(now_us);
    if (mode == SB_MODE)
        sb_pump();
}

void pgemu_outb(uint16_t port, uint8_t value, uint64_t now_us) {
    pgemu_advance(now_us);
    port &= 0x3FF;
    if (mode == GUS_MODE && (port >> 4 | 0x10) == gus_port_test) {
        pg_gus_write(port - settings.GUS.basePort, value);
        return;
    }
    if (mode == SB_MODE && (port >> 4) == sb_port_test) {
        switch (port - settings.SB.basePort) {
        case 0x0: case 0x8: opl_addr = value; break;
        case 0x1: case 0x9: pg_opl_write(opl_addr, value); break;
        case 0x2: opl_addr = 0x100 | value; break;
        case 0x3: pg_opl_write(opl_addr, value); break;
        default:
            sbdsp_write(port & 0xF, value);
            sb_pump();
            break;
        }
        return;
    }
    if (mode_has_opl() && (port & 0x3fc) == settings.SB.oplBasePort) {
        switch (port & 3) {
        case 0: opl_addr = value; break;
        case 2: opl_addr = 0x100 | value; break;
        default: pg_opl_write(opl_addr, value); break;
        }
        return;
    }
    if (port == CONTROL_PORT) {
        if (value == 0xCC)
            control_active = true;
        else if (control_active)
            select_picogus(value);
    } else if (port == DATA_PORT_LOW) {
        if (control_active)
            write_picogus_low(value);
    } else if (port == DATA_PORT_HIGH) {
        if (control_active)
            write_picogus_high(value, now_us);
    }
}

uint8_t pgemu_inb(uint16_t port, uint64_t now_us) {
    pgemu_advance(now_us);
    port &= 0x3FF;
    if (mode == GUS_MODE && (port >> 4 | 0x10) == gus_port_test)
        return pg_gus_read(port - settings.GUS.basePort);
    if (mode == SB_MODE && (port >> 4) == sb_port_test) {
        switch (port - settings.SB.basePort) {
        case 0x0: case 0x8: return pg_opl_read_status(false);
        case 0x2: return pg_opl_read_status(true);
        default: {
            uint8_t v = sbdsp_read(port & 0xF);
            sb_pump();
            return v;
        }
        }
    }
    if (mode_has_opl()) {
        if (port == settings.SB.oplBasePort)
            return pg_opl_read_status(false);
        if (port == settings.SB.oplBasePort + 2)
            return pg_opl_read_status(true);
    }
    if (port == CONTROL_PORT)
        return sel_reg;
    if (port == DATA_PORT_LOW)
        return read_picogus_low();
    if (port == DATA_PORT_HIGH)
        return read_picogus_high();
    return 0xFF; // nothing drives the bus
}

int pgemu_mode(void) {
    return mode;
}

int pgemu_irq_line(void) {
    return pg_shim_irq_line;
}


///////////
// Audio //
///////////

static int16_t clamp16i(int32_t v) {
    return (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
}

// sbplay.cpp audio_sample_handler(): one 44.1kHz frame of the SB/AdLib firmware
static void sb_adlib_frame(int32_t lr[2]) {
    int32_t l = 0, r = 0;
    if (mode == SB_MODE) {
        uint32_t card_stereo = sbdsp_sample_stereo();
        l = scale_sample((int16_t)(card_stereo & 0xFFFF), volume.sb_pcm[0], 0);
        r = scale_sample((int16_t)(card_stereo >> 16), volume.sb_pcm[1], 0);
    }
    int16_t opl[2];
    pg_opl_generate(opl);
    // 1.5x boost to match DSP levels (DOSBox-X uses SetScale(1.5f) for FM)
    l += scale_sample((int32_t)opl[0] * 3 >> 1, volume.opl[0], 0);
    r += scale_sample((int32_t)opl[1] * 3 >> 1, volume.opl[1], 0);
    if (mode == SB_MODE) {
        l = scale_sample(l, volume.sb_master[0], 1);
        r = scale_sample(r, volume.sb_master[1], 1);
    } else {
        l = clamp16i(l);
        r = clamp16i(r);
    }
    lr[0] = l;
    lr[1] = r;
}

// Linear resampling of the firmware's output (GUS: its GF1 rate, SB and
// AdLib: 44.1kHz) to the emulator's rate.
static uint32_t rs_phase = 0; // 16.16 position between prev and cur
static int32_t rs_prev[2], rs_cur[2];
static int rs_mode = -1;

static void source_frame(int32_t lr[2]) {
    if (mode == GUS_MODE) {
        uint32_t s = GUS_sample_stereo();
        lr[0] = (int16_t)(s & 0xFFFF);
        lr[1] = (int16_t)(s >> 16);
    } else if (mode_has_opl())
        sb_adlib_frame(lr);
    else
        lr[0] = lr[1] = 0;
}

void pgemu_render(int16_t *out, unsigned frames, unsigned sample_rate) {
    if (!sample_rate)
        return;
    if (rs_mode != (int)mode) { // new firmware: restart the stream
        rs_mode = mode;
        rs_phase = 0;
        rs_prev[0] = rs_prev[1] = rs_cur[0] = rs_cur[1] = 0;
    }
    if (mode == SB_MODE)
        sb_pump();
    for (unsigned i = 0; i < frames; i++) {
        uint32_t src_rate = (mode == GUS_MODE) ? pg_gus_rate() : 44100;
        if (!src_rate)
            src_rate = 44100;
        rs_phase += (uint32_t)(((uint64_t)src_rate << 16) / sample_rate);
        while (rs_phase >= 0x10000) {
            rs_phase -= 0x10000;
            rs_prev[0] = rs_cur[0];
            rs_prev[1] = rs_cur[1];
            source_frame(rs_cur);
        }
        for (int c = 0; c < 2; c++)
            out[2 * i + c] = clamp16i(rs_prev[c]
                + (int32_t)(((int64_t)(rs_cur[c] - rs_prev[c]) * (int32_t)rs_phase) >> 16));
    }
}
