/*---------------------------------------------------------------------------
PROJECT: Steem SSE
Atari ST emulator
Copyright (C) 2026 by Anthony Hayward and Russel Hayward + SSE

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see https://www.gnu.org/licenses/.

DOMAIN: Various
FILE: parameters.h
DESCRIPTION: Some parameters for emulation and the rest.
---------------------------------------------------------------------------*/

#pragma once
#ifndef SSEPARAMETERS_H
#define SSEPARAMETERS_H

#include "SSE.h"


//////////
// ACSI //
//////////

#define MAX_ACSI_DEVICES 8
#define ACSI_ID_LASER    7 // hard-coded


/////////////
// BLITTER //
/////////////

// possibly effect of LINEW, which delays Blitter start until end of Write bus
// cycle - could be different if no CPU bus access at once?
#if defined(SSE_MEGASTE)
#define BLITTER_LATCH_LATENCY ((OPTION_BLITTER_WU*TICKS8)>>((Cpu16.ScuReg&2)>>1))
//#define BLITTER_LATCH_LATENCY ((4*TICKS8)>>((Cpu16.ScuReg&2)>>1))
#else
#define BLITTER_LATCH_LATENCY (OPTION_BLITTER_WU*TICKS8) // should be 2 or 4
//#define BLITTER_LATCH_LATENCY (4*TICKS8))
#endif
// we can see the delay on schematics but it's hard to follow - 4 clocks seems 
// to be most compatible
#define BLITTER_START_WAIT (4)
#define BLITTER_END_WAIT (4)


/////////
// CPU //
/////////

//#define CPU_MAX_HERTZ (2000000000) // 2ghz
#define CPU_MAX_HERTZ   (2048000000) // >2ghz

/*
The master clock crystal and derived CPU clock table is:
PAL (all variants)       32.084988   8.021247
NTSC (pre-STE)           32.0424     8.0106
Mega ST                  32.04245    8.0106125
NTSC (STE)               32.215905   8.053976
Peritel (STE) (as PAL)   32.084988   8.021247
Some STFs                32.02480    8.0071
*/

#if defined(SSE_TIMINGS32) // x64 builds use the 32MHz clock
#define CPU_CLOCK_STF_NTSC  ((32042400*TICKS8)/4)
#define CPU_CLOCK_STF_PAL   ((32084988*TICKS8)/4)
#define CPU_CLOCK_MEGA_ST   ((32042450*TICKS8)/4)
#define CPU_CLOCK_STE_PAL   (CPU_CLOCK_STF_PAL)
#define CPU_CLOCK_STE_NTSC  ((32215905*TICKS8)/4)
#else
#define CPU_CLOCK_STF_NTSC   (8010600)
#define CPU_CLOCK_STF_PAL    (8021247) // precise
#define CPU_CLOCK_MEGA_ST    (8010613) // rounded
#define CPU_CLOCK_STE_PAL    (CPU_CLOCK_STF_PAL)
#define CPU_CLOCK_STE_NTSC   (8053976) // rounded
#endif

#if defined(SSE_TIMINGS32)
#define DBI_DELAY_CST (3) // fine-tune, problem it hangs together with other params
#else
#define DBI_DELAY_CST (1 /* *TICKS8 */) // ~8mhz; notice we use < for comparison, not <=
#endif
#if defined(SSE_GUI_EMUCONTROL)
#define DISK_BYTES_PER_TRACK SSEOptions.TrackBytes
#define DBI_DELAY SSEOptions.dbi
#else
#define DBI_DELAY DBI_DELAY_CST
#endif
#define CPU_FAST_CYCLES 12


///////////
// DEBUG //
///////////

//#define STEEM_CRASH_TXT "STEEM SSE CRASHED AGAIN"
#define STEEM_CRASH_TXT "SYSTEM ERROR" // more pro and that way it looks like it's not my fault

#if defined(SSE_DEBUG_TRACE)
#define MAX_TRACE_CHARS 512
#endif

#if defined(SSE_DEBUGGER)
#define MAX_MEMORY_BROWSERS 40 // 20 // note max 50 in menu
#define TRACE_MAX_WRITES 200000 // to avoid too big file (debugger-only)
#define IOLIST_MAX_ENTRIES 650
#define MAGIC_HIST_BLIT 0x98764321
#define MAGIC_HIST_DMA 0x12346789
#define MAGIC_HIST_INIT 0xFFFFFF71
#endif

#if defined(SSE_DEBUGGER_FAKE_IO)
#define FAKE_IO_START 0xfffb00
#define FAKE_IO_LENGTH 64*2 // in bytes
#define FAKE_IO_END (FAKE_IO_START+FAKE_IO_LENGTH-2) // starting address of last one
#define STR_FAKE_IO_CONTROL "Control mask browser"
#endif


//////////
// DISK //
//////////

#if defined(SSE_GUI_EMUCONTROL)
#define DISK_BYTES_PER_TRACK SSEOptions.TrackBytes
#define DRIVE_RPM SSEOptions.DriveRpm
#define FLOPPY_MAX_TRACK_NUM (SSEOptions.DiscMaxTrack)
#else
#define DISK_BYTES_PER_TRACK DISK_BYTES_PER_TRACK_CST
#define DRIVE_RPM DRIVE_RPM_CST
#define FLOPPY_MAX_TRACK_NUM FLOPPY_MAX_TRACK_NUM_CST
#endif
/*  #bytes/track
    The value generally seen is 6250.
    The value for 11 sectors is 6256. It's possible if the clock is higher than
    8mhz, which is the case on the ST.
*/
#define DISK_BYTES_PER_TRACK_CST (6256)
#define DRIVE_RPM_CST 300
#define FLOPPY_FF_VBL_COUNT 20
#define FLOPPY_MEDIACH_VBL 30
#define DISK_11SEC_INTERLEAVE 6
#define DRIVE_MAX_CYL 83
#define DRIVE_FAST_CYCLES_PER_BYTE (4*TICKS8)
#define DRIVE_FAST_IP_MULTIPLIER 8
#define FLOPPY_MAX_TRACK_NUM_CST 85
#define FLOPPY_MAX_SECTOR_NUM 26
#define PASTI_FILE_EXTS_BUFFERSIZE 160
#define STE_CLOCK8 8010613


/////////
// DMA //
/////////

#define DMA_BUFFER_LEN 16 // 8 words in each buffer
#define SECTOR_SIZE 512 // for floppy and hard disk


///////////
// FILES //
///////////

#if defined(SSE_UNIX)
#define SSE_TRACE_FILE_NAME "./TRACE.txt"
#else
#define SSE_TRACE_FILE_NAME "TRACE.txt"
#endif
#define ACSI_HD_DIR "ACSI"
#define SSE_VID_RECORD_AVI_FILENAME "SteemVideo.avi"
#define DISK_HFE_BOOT_FILENAME "HFE_boot.bin"
#define DISK_IMAGE_DB "disk image list.txt"
#define HD6301_ROM_FILENAME "HD6301V1ST.img"
//#define DRIVE_SOUND_DIRECTORY "DriveSound" // default
#define YM2149_FIXED_VOL_FILENAME "ym2149_fixed_vol.bin"
#define SSE_DISK_CAPS_PLUGIN_FILE "CAPSImg"
#define ARCHIVEACCESS_DLL "ArchiveAccess"
#define UNZIP_DLL "unzipd32" 
#define SSE_PLUGIN_DIR2 "plugins"
#define PASTI_DLL "pasti"
#ifdef SSE_X64
#define SSE_PLUGIN_DIR1 "plugins64"
#define UNRAR_DLL "unrar64"
#define CURL_DLL "libcurl-x64"
#define MEMORY_SNAPSHOTS "snapshots64"
#if defined(SSE_DEBUG)
#define VIDEO_LOGIC_DLL "stvl64d"
#else
#define VIDEO_LOGIC_DLL "stvl64"
#endif
#else
#define SSE_PLUGIN_DIR1 "plugins32"
#define UNRAR_DLL "unrar" 
#define CURL_DLL "libcurl"
#define MEMORY_SNAPSHOTS "snapshots32"
#if defined(SSE_DEBUG)
#define VIDEO_LOGIC_DLL "stvl32d"
#else
#define VIDEO_LOGIC_DLL "stvl32"
#endif
#endif
#define STEEM_SSE_FAQ "FAQ (SSE)"
#define STEEM_HINTS "Hints"
#define STEEM_MANUAL_SSE "Steem SSE Manual 4.2.1"
#define FREE_IMAGE_DLL "FreeImage"
#if defined(SSE_STATS_RTF)
#define STEEM_STATS_FILENAME "stats.rtf"
#elif defined(SSE_UNIX)
#define STEEM_STATS_FILENAME "./stats.txt"
#else
#define STEEM_STATS_FILENAME "stats.txt"
#endif
#define SSE_SWEETFX_D3D_HACK "d3d9sweetfx" // original name d3d9.dll
#define SSE_STARTUPSCREEN "startup.jpg"
#define MSA_CONVERTER "msa.exe"
#if defined(SSE_DEBUGGER_FRAME_REPORT)
#if defined(SSE_UNIX)
#define FRAME_REPORT_FILENAME "./FrameReport.txt"
#else
#define FRAME_REPORT_FILENAME "FrameReport.txt"
#endif
#endif
#define FILE_MIDIDUMP "midi.bin"
#define FILE_SERIALDUMP "serial.bin"
#define FILE_PRINTERDUMP "printer.txt"
#define FILE_DUMPOPTIONS "options.txt"
#ifdef SSE_X64
#define FILE_AUTOSNAPSHOT "auto64"
#else
#define FILE_AUTOSNAPSHOT "auto32"
#endif
#define FILE_BACKUPSNAPSHOT ".stsbackup"
#define FILE_RESETSNAPSHOT "auto_reset_backup.sts"
#define FILE_LOADUNDOSNAPSHOT "auto_loadsnapshot_backup.sts"

#if defined(SSE_LONG_PATH) // we directly replace MAX_PATH with SSE_MAX_PATH in some places
#define SSE_MAX_PATH 4096 // arbitrary
#define LONG_PATH_PREPEND "\\\\?\\" // \\?\ is special signal for long path in Windows
#else
#define SSE_MAX_PATH MAX_PATH // 260
#endif

//#define 


/////////
// GLU //
/////////

//#define CYCLES_FROM_START_OF_HBL_IRQ_TO_WHEN_PEND_IS_CLEARED (28*TICKS8)
#define MEM_FIRST_WRITEABLE 8 // bus error if writing on bytes 0-7, it's ROM, RAM 0-7 is wasted
#define MEM_IO_BASE 0xff8000 // IO is memory-mapped, starting at this address
#define MEM_START_OF_USER_AREA 0x800
/*  Real frequencies on PAL ST:
    50Hz  8021247 / (313*512)    50.05270941
    60Hz  8021247 / 263*508)     60.03747642
    71Hz  8021247 / (501*224)    71.47532613
    (Mega) 8010612.5 / (501*224) 71.38056
    We call the high resolution frequency 71 or 72, there are not two different frequencies
*/
#define GLU_SCANLINE_CYCLES_50HZ (512*TICKS8) // LineCycles are expressed in CPU cycles
#define GLU_SCANLINE_CYCLES_60HZ (508*TICKS8) // on 64bit builds, the clock is 32MHz, not 8MHz
#define GLU_SCANLINE_CYCLES_72HZ (224*TICKS8)
#define PAL_HZ 50
#define NTSC_HZ 60
#define MONO_HZ 72 //71  // it's between 71 and 72, 72 is more common on video cards
#define GLU_PAL_SCANLINES 313
#define GLU_NTSC_SCANLINES 263
#define GLU_MONO_SCANLINES 501
#define GLU_PAL_TOPSCANLINES 63
#define GLU_NTSC_TOPSCANLINES 34
#define GLU_MONO_TOPSCANLINES 35
#define GLU_HBL_SHIFT 0  // -8 for old timings
#define GLU_DE_ON_MONO 14
#define GLU_DE_ON_NTSC 60
#define GLU_DE_ON_PAL (60+4)
#define GLU_HSYNC_DURATION_LO 40
#define GLU_HSYNC_DURATION_HI 24
#define GLU_SCANLINE_CYCLES_CHECK 62
#define GLU_HBLANKOFF_50 36
#define GLU_HSYNCON_50 472
#define GLU_RELOADVC_50 70
#define GLU_RELOADVC_70 0 //14
#define GLU_ENABLEVBI_50 64
#define GLU_CARTBASE 0xFA0000


/////////
// GUI //
/////////

#define README_FONT_NAME "Courier New"
#define README_FONT_HEIGHT 16
#define EXT_TXT ".txt"
#define EXT_RTF ".rtf"
#define CONFIG_FILE_EXT "ini" // ini, cfg?
#if defined(SSE_DISK_M3U)
#define EXT_M3U "m3u"
#endif
#define SHORTCUT_VBLS_BETWEEN_CHECKS 3 // ?
#define WINDOW_TITLE_MAX_CHARS 100
#if defined(SSE_BUILD)
#define SSE_VERSION_TXT_LEN 8// "3.7.0"
#define WINDOW_TITLE stem_window_title
#ifdef SSE_BETA
#define APP_NAME "Steem SSE beta"
#else
#define APP_NAME "Steem SSE"
#endif
//#define STEEM_WEB "https:/""/sourceforge.net/p/steemsse/"
#define STEEM_WEB "https://sourceforge.net/projects/steemsse/"
#define STEEM_CHECKURL "https://downloads.sourceforge.net/project/steemsse/"
#define STEEM_VERINFO "version.txt"
#define STEEM_WEB_BUG_REPORTS "https://sourceforge.net/p/steemsse/forum/bugs/"
// HFEv3 fork: https://github.com/kodak80/Steem-SSE-with-HFEv3
// The release workflow passes the release name as HFEV3_VER (e.g. 1, test5,
// dev-41cd931), shown after the Steem version as "HFEv3-1".
#define HFEV3_WEB "https://github.com/kodak80/Steem-SSE-with-HFEv3"
#define HFEV3_STR2(x) #x
#define HFEV3_STR(x) HFEV3_STR2(x)
#ifdef HFEV3_VER
#define HFEV3_VERSION HFEV3_STR(HFEV3_VER)
#else
#define HFEV3_VERSION "dev"
#endif
#define DIDATABASE_WEB "http://steem.atari.st/database.htm" // still online
#define STEEM_WEB_LEGACY "http://steem.atari.st/" // for links in Disk Image Database
#else
extern const char *stem_version_text;
#define STEEM_EMAIL "steem@gmx.net"
#define STEEM_WEB "http://steem.atari.st/"
#define DIDATABASE_WEB STEEM_WEB "database.htm"
#endif
#define MSACONV_WEB "http://msaconverter.free.fr/" //390 still valid
#define MAX_DIALOGS 20
#define COLOUR_CONTROL_BITMAP_H 150
#define GUI_FONT_SIZE 11
#define GUI_SMALLFONT_SIZE 12
#define GUI_BIGFONT_SIZE 16
#define STATE_HISTORY_LEN 10
#define DM_QUICKFOL_LEN 10
#define DM_HISTORY_LEN 10

#ifdef SSE_X64
#define SSE_BITNESS 64
#else
#define SSE_BITNESS 32
#endif


//////////
// IKBD //
//////////

#define HD6301_CLOCK 1000000
#define HD6301_ROM_CHECKSUM 0x0296915D  // CRC32 - BTW this rom sends $F1 on reset
#define MAX_PC_JOYS 8
#define MAX_ST_JOYS 8 // 2 standard + 4 STE + 2 Parallel
#define JOYSTICK_SETUPS 6 // the sky is the limit
#define MAX_KEYBOARD_BUFFER_SIZE 128 // not like hardware
#define MAX_ST_KEYS 128 // 95?
#define DIJOY_MAX_DATAOBJECT 43

// not OPTION_C1

#define IKBD_HBLS_FROM_COMMAND_WRITE_TO_PROCESS 5
#define IKBD_SCANLINES_FROM_ABS_MOUSE_POLL_TO_SEND ((MONO_MONITOR) ? 50 : 30)
#define IKBD_SCANLINES_FROM_JOY_POLL_TO_SEND ((MONO_MONITOR) ? 32 : 20)   // 32:20
#define IKBD_DEFAULT_MOUSE_MOVE_MAX 15
#define IKBD_RESET_MESSAGE 0xf1
#define ACIA_CYCLES_NEEDED_TO_START_TX 512


////////
// IO //
////////

#define PORT_LOOPBUFSIZE 8192

#if defined(SSE_NETWORK)
#define DEFAULT_IP_PORT 1040 // see what I did?
#define MAXCLIENT_SOCKETS 16
#endif


/////////
// MFP //
/////////

// timer crystal frequency - the MC68901 itself runs at 4mhz on the ST
// we consider many STE have a faster crystal (Japtro)
// player can change the frequency between 2457500 Hz and 2457800 Hz
#define MFP_XTAL1 2457600 // default for STF
#define MFP_XTAL2 2457700 // default for STE

// player can change other parameters
#define MFP_TMOUT_TO_IRQ_TCLK 0 // timeout to IRQ in xtal cycles (tCLK)
#define MFP_TIMER_START_SYNC TRUE // "sync" for Froggies menu, problem Audio Sculpture can HALT :(
#define MFP_TIMER_STOP_SYNC TRUE
#ifdef SSE_X64
#define MFP_TIMER_START_DELAY 4
#define MFP_TIMER_STOP_DELAY 4
#else
#define MFP_TIMER_START_DELAY 1 
#define MFP_TIMER_STOP_DELAY 1
#endif



/* MFP doc: The information read is the value of the
counter which was captured on the last low-to-high transition of the DS pin.
-> assume the value is one 4MHz tick late

Note This is interpreted in the MiSTer project as:
// from datasheet: 
// read value when the DS pin last gone high prior to the current read cycle
always @(posedge CLK) begin
	reg DS_last;
	DS_last <= DS;
	if (~DS_last & DS) cur_counter <= down_counter;
end

And cur_counter is what's put on the bus when the CPU is reading
Generally there's a fetch cycle before the current read cycle

*/
#define MFP_TIMER_READ_ADJUST (-2*TICKS8)
#define MFP_TMOUT_TO_IRQ (2*TICKS8) // in CPU cycles // Cool STE some flicker OK not too much

#define MFP_TIMER_B_COUNT_CYCLES_TCLK 5 // logically 5 ticks for 4 intervals
#define MFP_TIMER_B_COUNT_CYCLES (2*TICKS8) // difference GLU/CPU
#define MFP_TIMER_B_COUNT_CYCLES_STVL (-2*TICKS8) // should be 0, compensates default +2

#define MFP_TIMER_READ_ADJUST_TCLK 0
#define MFP_TIMER_PRECISION 100000 //10000000 // 1000 in previous versions //is it important?


//////////
// MIDI //
//////////

#define MAX_SYSEX_BUFS 10
#if defined(SSE_DIRECTMIDI)
#define MIDI_UNITS_1SEC SSEConfig.MidiUnitsSecond
#else
#define MIDI_UNITS_1SEC 1000 // 1000 milliseconds
#endif


//////////
// MISC //
//////////

#define MAX_AGENDA_LENGTH 32
#define MACRO_DEFAULT_ADD_MOUSE 1
#define MACRO_DEFAULT_ALLOW_VBLS 1
#define MACRO_DEFAULT_MAX_MOUSE 15
#define MACRO_RECORD_BUF_INC_SECS 20

#if defined(SSE_CARTRIDGE_ACTIVE)
#define CARTRIDGE_DATA_SIZE 64
#endif


/////////
// MMU //
/////////

// delay between GLUE 'DE' decision and first LOAD signal emitted by the MMU
// without waitstates
#define MMU_PREFETCH_LATENCY (8*TICKS8)
#define MEM_EXTRA_BYTES 320


/////////
// OSD //
/////////

#define OSD_MESSAGE_LENGTH 40 // in bytes excluding /0
#define OSD_MESSAGE_TIME 1 // in seconds
#define HD_TIMER 100 // Yellow hard disk led (imperfect timing)


/////////////
// SHIFTER //
/////////////

#define SHIFTER_DEFAULT_WAKEUP (0) // 0: Spectrum512 compatible; -1: Mega STE
#define SHIFTER_MAX_WU_SHIFT (3) // - or +
#define PAL_EXTRA_BYTES 16 // what for?
#define PAL_SIZE 16 // don't change


///////////
// SOUND //
///////////

#define SOUND_DESIRED_LQ_FREQ (50066/2)
#define YM_LOW_PASS_FREQ (10500) //in Hz, default
#define YM_LOW_PASS_MAX (22000)
#define MW_LATENCY_CYCLES ((128+16)*TICKS8) // quite the hack, TODO

#define MW_LOW_SHELF_FREQ0 80 // officially 50 Hz
#define MW_HIGH_SHELF_FREQ0 (MIN(10000,(int)sound_freq/2)) // officially  15 kHz
#if defined(SSE_GUI_EMUCONTROL)
#define MW_LOW_SHELF_FREQ Microwire.LowShelf
#define MW_HIGH_SHELF_FREQ Microwire.HighShelf
#else
#define MW_LOW_SHELF_FREQ MW_LOW_SHELF_FREQ0
#define MW_HIGH_SHELF_FREQ MW_HIGH_SHELF_FREQ0
#endif

#define PSG_CHANNEL_AMPLITUDE 40 // see note in TYM2149::LoadFixedVolTable() 
#define VOLTAGE_ZERO_LEVEL 0
#if defined(SSE_LIBRETROSOUND1) 
#define PSG_BUFFER_FRAMES 3 // TODO option 60ms -> 3 frames
#define PSG_WRITE_EXTRA 0 // 
#else
#define PSG_BUFFER_FRAMES 3
#ifndef ONEGAME
#define PSG_WRITE_EXTRA 300
#else
#define PSG_WRITE_EXTRA OGExtraSamplesPerVBL
#endif
#endif
#define DEFAULT_SOUND_BUFFER_LENGTH (32768*SCREENS_PER_SOUND_VBL) // see conditions.h


/////////
// TOS //
/////////

#define AUTORUN_HD DRIVE_Z // Z: is used for PRG support (can't be a valid GEMDOS drive)
#define MAX_STEMDOS_FSNEXT_STRUCTS 100 // one for each process?
#define MAX_STEMDOS_PEXEC_LIST 76 //Change loadsave_emu.cpp if change this! 
#define MAX_STEMDOS_FILES 255 // was 46, found no reference for it
/*Dgetpath() As there is no way to specify the buffer size to this function you
should allow at least 128 bytes of buffer space. This will allow for up to 8
folders deep.*/
#define STEMDOS_MAX_PATH 128 // note gemlib: 121
#define MAX_GEMDOS_HARDDRIVES (26-2) // necessary for running .PRG and .TOS files
#define VOLUME_NAME "STEMDISK.MNT"
#define TOS_FILENAME_LEN 14


///////////
// VIDEO //
///////////

#define ORIGINAL_BORDER_SIDE 32
#define VERY_LARGE_BORDER_SIDE 50 // 52+48+320=420
#define LARGE_BORDER_SIDE 46 // 48+44+320=412
#define ORIGINAL_BORDER_BOTTOM 40
#define LARGE_BORDER_BOTTOM 45
#define VERY_LARGE_BORDER_BOTTOM 45
#define ORIGINAL_BORDER_TOP 30
#define BIG_BORDER_TOP 38 // 50hz
#define BORDER60_MINUS_TOP 12
#define BORDER60_MINUS_BOTTOM 16
#define BORDER71_MINUS_TOP 32
#define BORDER71_SHIFT 26
#define BIGGEST_BORDER 3 //420
#define ST_ASPECT_RATIO_DISTORTION 1.10f // multiplier for Y axis
#define ST_ASPECT_RATIO_DISTORTION_60HZ 1.25f // with no border
#if defined(SSE_DRAW_C)
/* max is hires + borders "size 4" 640+32+32=704*2=1408
* if border doubled by mistake: 640+64+64=768*2=1536 <2048... why the overflow????
* -> just in case we go back to 4096 and make it dynamic (saving resources and
* putting the buffer far from other variables)
*/
#define DRAW_TEMP_LINE_BUF_LEN (4*1024) // 32bit pixels for 1 scanline, overkill
#elif defined(SSE_VID_SIZE4)
#define DRAW_TEMP_LINE_BUF_LEN (sizeof(DWORD)*2*1024)
#else
#define DRAW_TEMP_LINE_BUF_LEN (4*1024) // 32bit pixels for 1 scanline, overkill
#endif
#define NMODECHANGES 32 // shift mode and sync mode changes are recorded in tables
                        // should be 16, 32, 64...
#define NFRAME_TIME_AVG 18 // 18 for hires, originally 12
#define HOR_PIXELS_LO 320 // don't change
#define HOR_PIXELS_MED 640
#define HOR_PIXELS_HI 640
#define VER_PIXELS_LO 200
#define VER_PIXELS_HI 400

#endif//#ifndef SSEPARAMETERS_H
