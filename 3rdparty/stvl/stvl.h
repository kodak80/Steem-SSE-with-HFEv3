/*---------------------------------------------------------------------------
PROJECT: STVL
Copyright (C) 2026 by Sensei Software Edition
FILE: stvl.h
DESCRIPTION: Declarations for low-level Atari ST video logic emulation
---------------------------------------------------------------------------*/

#pragma once
#ifndef STVL_H
#define STVL_H

#include <conditions.h>
#include <data_union.h>

#pragma pack(push, 8)

struct TStvl {
  // enums
  enum EStvl { 
    VERSION=0x111,
    VER_STESND=0x101,
    VER_DIRECTRAM=0x111,
    // for debug trace
    EVENT_BLANK=1,EVENT_DE,EVENT_HS,EVENT_VS,EVENT_PX,EVENT_LD,EVENT_RL,
    EVENT_GR,EVENT_GS,EVENT_SM,EVENT_X,EVENT_TB
  };
  // callback function pointers
  void(CALLBACK *cbHInt)(); // callback for horizontal interrupt
  void(CALLBACK *cbVInt)(); // callback for vertical interrupt
  void(CALLBACK *cbDe)();   // callback for Display Enable change
  WORD(CALLBACK *cbPeekWord)(DWORD addr); // callback for fetching video memory
  void(CALLBACK *cbSetPal)(int n, WORD NewPal); // optional callback for setting a palette
  void(CALLBACK *cbTraceVideoEvent)(int event_type,int event_value); // optional callback
                            // for debugging (useful only in debug build)
  // data pointers
  DWORD *draw_mem_ptr_min;  // points to start of line_buffer by default
  DWORD *draw_mem_ptr_max;  // points to end of line_buffer -8 by default
  DWORD *draw_mem_ptr;      // points to next pixel to be rendered
  LONG *pPCpal;             // points to PC_pal by default
  // 32bit
  DWORD PC_pal[16];         // default 32bit PC palette for 16 ST colours
  int tick8;                // 8mhz clock counter
  int tick32;               // 32mhz clock counter relative to the CPU
  int nticks;               // 32mhz cycles to run
  DU32 vbase;               // start of video RAM
  DU32 vcount;              // video counter
  DWORD abus;               // 23bit system address bus
  // as tested, using small data types doesn't necessarily hurt performance
  // 16bit
  WORD ST_pal[16];          // 9bit/12bit ST palette
  WORD output_bit_mask[2];  // internal mask for output timing in colour resolutions
  short video_linecycles;   // number of 8mhz cycles since HSYNC OFF
  short de_linecycle;       // in video_linecycles, time of last DE change
  short dbg_fetched_words;  // number of fetched video memory words (16bit) this scanline
  short vdec;               // vertical DE counter
  short vsc;                // vertical sync counter
#if defined(SSE_VID_STVL_DIRECT_RAM)
  BYTE mono_col;            // Shifter register for monochrome (only bit 0 is relevant)
  BYTE mem_len2;            // byte #2 of ST memory len, 2=128K, 40=4MB 
                            // (if >0, cbPeekWord points to Steem's Mem_End_minus_2 = end-2byte)
#else
  short mono_col;           // Shifter register for monochrome
#endif
  WORD render_y;            // runs from VSYNC OFF to VSYNC ON (max)
  DU16 udbus;               // 16bit system data bus (see data_union.h)
  DU16 rambus;              // data bus for RAM and Shifter (not the main bus)
  // 8bit
  BYTE hwoverscan;          // harware overscan (0: none; 1: LaceScan; 2: AutoSwitch; higher: reserved)
  BYTE VideoOut;            // screen (0: none; 1: monochrome; 2: colour; higher: reserved)
  BYTE shift_mode;          // Shifter Mode (0-3)
  BYTE hsc;                 // horizontal sync counter
  BYTE hdec;                // horizontal DE counter
  BYTE hde_ctr;             // STE delayed DE counter
  BYTE load_ctr;            // Shifter 4bit LOAD counter (not a shift counter here)
  BYTE output_bit[3];       // rendering buffer
  BYTE pixel_ctr;           // Shifter 4bit pixel counter
  BYTE ctrDeOn;             // call at nth 8mhz tick after event (0: don't call)
  BYTE ctrDeOff;            // call at nth 8mhz tick after event (0: don't call)
  BYTE ctrHInt;             // call at nth 8mhz tick after event (0: don't call, otherwise it
  BYTE ctrVInt;             // must be set after each call to STVL_update() to override defaults)
  BYTE m2clock;             // M2CLK counter (2Mhz) active tick (0-15)
  BYTE m2clockb;            // M2CLKB counter (2Mhz) active tick (0-15) 
  BYTE load;                // LOAD timing counter
  BYTE ClockTick[3];        // mode 0 and 1, which ticks?
  BYTE subcycle;            // 32mhz clock counter relative to the video system
  BYTE autoswitch_ctr;      // internal counter for Autoswitch hardware overscan
  BYTE linewid;             // STE video register
  BYTE framefreq;           // records frequency (50,60,72) when vertical DE stops
  BYTE scroll_reg_index;    // STE Shifter scroll register index
  BYTE hscroll;             // video register
  BYTE hscroll_complement;  // 16-hscroll, to spare some computing
  BYTE PixelCtrActiveTimer; // internal counter for Shifter pixel counter activation
  BYTE shifter_px_ctr_delay;// starting value of PixelCtrActiveTimer
  char hbl_timer;           // internal counter before cbHInt is called
  char vbl_timer;           // internal counter before cbVInt is called
  char timer_b_itimer;      // internal counter before cbDe is called
  BYTE bus_mask;            // bus activity mask - bit 0 active, bit 1 write,
                            // bit 2 fetch, bit 3 low byte, bit 4 high byte
  BYTE st_model;            // 0=unknown,1=STF,2=STE
  BYTE wakestate;           // 1-4 for STF, 1 for STE
  char shifter_wakeup;      // -3 -> +3
  bool hde;                 // horizontal DE
  bool vde;                 // vertical DE
  bool de;                  // Display Enable signal (hde && vde)
  bool hblank;              // horizontal Blank
  bool vblank;              // vertical Blank
  bool blank;               // Blank signal (hblank || vblank)
  BYTE hsync;               // horizontal sync signal (>1 if already asserted)
  bool vsync;               // vertical Sync signal
  bool sync;                // internal Sync state (hsync || vsync)
  bool viden;               // MMU's latched DE
  bool mde0;                // bit 0 of shift mode register in GLUE (MEDRES)
  BYTE mde1;                // bit 1 of shift mode register in GLUE (HIRES)
  bool exts;                // bit 0 of sync mode register in GLUE (external sync)
  BYTE pal;                 // bit 1 of sync mode register in GLUE (internal sync 50hz)
  bool de2;                 // internal copy of de, it's the same as de except for hardware overscan
  bool write_vcount;        // internal flag signaling a write to the video counter (STE)
  bool reloading;           // true between reloading of the Shifter shift registers and clearing of the LOAD counter
  bool pixel_ctr_active;    // true when the Shifter pixel counter is running
  bool noscroll;            // MCU register indicating if horizontal scrolling is inactive (STE)
  bool scroll_active;       // internal Shifter variable, activates on first Shifter RELOAD (STE)
  bool busy;                // true when STVL_stf_run() or STVL_ste_run() executing
  bool bWriteShifter;       // internal flag, write to Shifter register executing
  bool UnstableShifter;     // Shifter bands and other glitches likelier
  DU64 scr;                 // 4 16bit STE Shifter scroll registers // 64
  DU64 ir;                  // 4 16bit Shifter input registers      // 64
  DUS64 sr;                 // 4 16bit Shifter shift registers      // 64
  DU64 output;              // 4 16bit Shifter output variables     // 64
  // internal, for hscroll optimization:
  DU64 right_border_mask;                                           // 64
  DU64 output_bit_mask04,output_bit_mask14;                          // 64 + 64
  unsigned __int64 res_hscroll_mask;                                 // 64
  DWORD line_buffer[1024];  // internal scanline rendering buffer for 32bit pixels
  // dbg_... variables are updated only in the debug build
  int dbg_vertclks;         // number of scanlines since VSYNC OFF
  int dbg_vde;              // number of scanlines with DE since VSYNC_OFF
  int dbg_framecycles;      // number of 8mhz cycles since VSYNC OFF
  int dbg_npixels;          // number of rendered pixels this scanline, whatever the resolution
  int dbg_m2clock;          // 2MHz counter
  // this has been added with Steem v402, at the end for compatibility:
  WORD version;             // where Client writes back version (if different, disable sound fetching)
  bool sreq;                // GSTShifter sound request
  bool snden;               // MCU latch of sreq
  void(CALLBACK *cbFetchSound)(); // optional callback for fetching sound samples
};



#pragma pack(pop)

#undef DllImport
#define DllImport __declspec( dllimport )

#define STVL_CALLCONV __cdecl

extern "C" {
  DllImport DWORD STVL_CALLCONV STVL_init(TStvl *pStvl);
  DllImport void STVL_CALLCONV STVL_reset(TStvl *pStvl, bool Cold);
  // cycles are in 32MHz units
  DllImport void STVL_CALLCONV STVL_stf_run(TStvl *pStvl, int cycles);
  DllImport void STVL_CALLCONV STVL_stf_hwo_run(TStvl *pStvl, int cycles);
  DllImport void STVL_CALLCONV STVL_ste_run(TStvl *pStvl, int cycles);
  DllImport void STVL_CALLCONV STVL_update(TStvl *pStvl);
  DllImport void STVL_CALLCONV STVL_update_pal(TStvl *pStvl, int n, WORD pal);
}

#undef DllImport

#endif//#ifndef STVL_H
