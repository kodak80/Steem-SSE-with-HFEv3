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

DOMAIN: All
FILE: SSE.h
DESCRIPTION: Compilation directives (similar to conditions.h, this one is
specific to Steem SSE)
---------------------------------------------------------------------------*/

// for v4.2.1
#pragma once
#ifndef SSE_H
#define SSE_H

/*
ST Enhanced EMulator Sensei Software Edition (Steem SSE)
--------------------------------------------------------

SVN code repository is at:
https://sourceforge.net/projects/steemsse/

This is based on the source code for Steem R63 as released by Steem authors,
Ant & Russ Hayward, at
https://github.com/steem-engine/steem-engine

The following directives shoud be defined or not in the project/makefile:

SSE_DEBUG for debug facilities on both the Debugger and the Visual Studio _DEBUG builds
DEBUG_BUILD or SSE_DEBUGGER for the Debugger (Windows-only)
SSE_DD for DirectDraw builds
SSE_D3D for Direct3D builds (main builds)
SSE_RELEASE for non-beta builds (SSE_BETA for betas, not necessary)
BCC_BUILD for Borland C++ builds (old compiler)
MINGW_BUILD for MinGW32 builds (only maintained from time to time)
VC_BUILD for Visual C++ builds (main builds, not necessary)
SSE_STDCALL (VC 32bit compiler settings: __stdcall), not necessary
SSE_DRAW_C for the C draw routines (instead of assembly modules), all builds except _DEBUG
SSE_LINUX_DYN for the reguar Linux builds (silly switch due to some experiments)
SSE_X64 for 64bit Linux builds (not necessary for Windows)
SSE_LIBRETRO for a libretro core

BIG_ENDIAN_PROCESSOR as it says, never attempted yet (Intel is little-endian)

See Steem.cpp for other project defines

SSE.h is supposed to mainly be a collection of compiling switches (defines).
It is included in pch (precompiled header), any change in SSE.h triggers
compilation of about everything.
*/

// guess some switches
#if !defined(SSE_BUILD)
#define SSE_BUILD // necessary for all builds, it just marks some differences with old Steem
#endif
#if !defined(WIN32) && defined(_WIN32)
#define WIN32
#endif
#if !defined(SSE_X64) && defined(_WIN64)
#define SSE_X64
#endif
#if !defined(SSE_UNIX) && defined(UNIX)
#define SSE_UNIX
#endif
#if !defined(VC_BUILD) && (_MSC_VER>0)
#define VC_BUILD
#endif
#if !defined(NO_DEBUG_BUILD) && !defined(DEBUG_BUILD) && !defined(SSE_DEBUGGER)
#define NO_DEBUG_BUILD
#endif
#if !defined(NO_DEBUG_BUILD) && !defined(DEBUG_BUILD) && defined(SSE_DEBUGGER)
#define DEBUG_BUILD
#endif
#if !defined(SSE_RELEASE) && !defined(SSE_BETA) && !defined(SSE_LIBRETRO)
#define SSE_BETA
#endif


#define SSE_VERSION 421
#ifdef SSE_BETA
#define SSE_VERSION_R 6
#else
#define SSE_VERSION_R 2
#endif



#ifdef SSE_BETA
#define SSE_LEAN_AND_MEAN
#else
#define SSE_LEAN_AND_MEAN
#endif

#ifdef WIN32

// The Windows builds can optionally use either DirectDraw (DD) or 
// Direct3D9 (D3D), not both
// Use SSE_DD as compile directive (config, makefile) for a DirectDraw build
// Use SSE_D3D for a Direct3D9 build
// If none of them is defined, a GDI-only build will be produced
#if defined(SSE_DD)
#define SSE_VID_DD
#ifdef VC_BUILD
#define SSE_VID_DD7 // if not defined, DirectDraw2
#endif
#elif defined(SSE_D3D)
#define SSE_VID_D3D
#endif

#ifndef BCC_BUILD
#define SSE_WINDOWS_2000_MIN // needed for 2 screens + CPU % + INPUT + ...
#endif
#if defined(VC_BUILD) && _MSC_VER>=1500 //VS2008+
#define SSE_VC_INTRINSICS
#define SSE_WINDOWS_XP_MIN // breaking change necessary for DirectMusic
#endif
#define SSE_WINDOWS_XP_MAX // staying XP-compatible (32bit + 64bit, there are users)
#define SSE_WIN32_A // technical stuff

#if !defined(SSE_X64) && (_MSC_VER>=1928) // VS2019+ (maybe 2015 too)
#define SSE_GUI_FIX // necessary for 32bit builds too
#endif

#define SSE_FILES_IN_RC
#define SSE_ONEINSTANCE

#endif//WIN32

#ifdef UNIX
#define SSE_UNIX
#define SSE_UNIX_STATIC_VAR_INIT //odd todo
#endif

#ifdef SSE_X64
#define SSE_X64_DEBUG
#ifndef SSE_DRAW_C
#define SSE_DRAW_C
#endif
#if (_MSC_VER>=1900) // VS2015+
#define SSE_GUI_FIX // horrible problems
#endif
#define SSE_NO_UNZIPD32 // remove code for unzipd32.dll, there's no unzipd64.dll
#define SSE_TIMINGS32 // internally run at 32MHz instead of 8MHz; it's confusing because 64bit->TIMINGS32
#define SSE_TIMINGS32A
#endif


// Exception management...
//#define SSE_M68K_EXCEPTION_TRY_CATCH //works but too slow, especially if _DEBUG
#ifndef _DEBUG
#define SSE_MAIN_LOOP //disable to debug crashes
#if defined(SSE_MAIN_LOOP)
#define SSE_MAIN_LOOP1
#if _MSC_VER >= 1500
#define SSE_MAIN_LOOP2 //VC only
#endif
#endif
#endif


/////////////////
// NO FEATURES //
/////////////////

#define NO_RARLIB // don't use rarlib
#define SSE_NO_AOT // Nuke Always On Top option
//#define NO_CRAZY_MONITOR // extended monitor hack
#define SSE_NOSTEPBYSTEP // no step by step recovery
#define SSE_NO_FALCONMODE //v402
//#define SSE_NO_FREEIMAGE
#define SSE_NO_INTERNAL_SPEAKER
//#define SSE_NO_INTRO
#define SSE_NO_JOYSTICK_MM //circle around unsolved bug (ours or theirs?)
//#define SSE_NO_MICROWIRE
#ifdef SSE_BETA
#define SSE_NO_OLD_IKBD // nuke high level IKBD emulation (SSE_HD6301_LL must be defined)
#endif
//#define SSE_NO_OSD // remove all On Screen Display
#define SSE_NO_SCREENSAVER
#define SSE_NO_UPDATE // remove all update code
#define SSE_NO_WINSTON_IMPORT // nuke WinSTon import
#ifdef UNIX
// modern pulse only, old drivers maybe not available anymore?
#define NO_PORTAUDIO
#define NO_RTAUDIO
#endif


//////////////
// FEATURES //
//////////////

#define SSE_ACSI // DMA port (hard drive low-level, laser printer)
#ifndef BCC_BUILD
#define SSE_BADALLOC // no local try/catch blocks
#define SSE_BIGFILES // 2GB is not enough
#endif
#define SSE_CARTRIDGE_ACTIVE  // memory and pointer to function
#define SSE_DISK_CAPS // IPF, CTR disk images
#define SSE_DISK_GHOST // save hiscores of STX etc.
#define SSE_DISK_HFE // HxC floppy emulator HFE (v.1) image support
#define SSE_DISK_RAR_SUPPORT
#define SSE_DISK_STW // MFM disk image format
#define SSE_DISK_STX // native support for Pasti disk images (goal is 100% OK but timing is soso)
#define SSE_DISK_SWAPPER // smart
#define SSE_DONGLE // special adapters
#define SSE_DRIVE_FREEBOOT // select side, select drive
#define SSE_DRIVE_SINGLESIDE
#define SSE_DRIVE_SOUND // poor imitation of a SainT feature
#define SSE_GUI_INSTANTCHANGE // option for ST/TOS/RAM changes
#define SSE_HARDWARE_OVERSCAN // LaceScan, AutoSwitch
#define SSE_HD6301_LL // using 3rd party code
#define SSE_IKBDI // IKBD command interpreter, useful for logging and a clock hack
#define SSE_IKBD_MAPPINGFILE // v402
#define SSE_IKBD_RTC // battery-powered 6301
#define SSE_JOYSTICK_JUMP_BUTTON
#define SSE_MEGA // Mega ST (later called Mega) and Mega STE models
#define SSE_MMU_MONSTER_ALT_RAM // HW hack for ST
#ifndef SSE_NO_OSD
#define SSE_OSD_DEBUGINFO // in rlz build
#define SSE_OSD_DRIVELED
#define SSE_OSD_FPS_INFO
#endif
#define SSE_PRINTER // SMM804 emu; bold, italics... on RTF + graphics on PBM
#define SSE_SHIFTER_HIRESRASTERS // hack
#define SSE_SHIFTER_UNSTABLE
#define SSE_SOUND_CARTRIDGE // B.A.T etc.
#if !defined(SSE_NO_MICROWIRE)
#define SSE_SOUND_MICROWIRE_HACKS
#define SSE_SOUND_MICROWIRE_OPTION // There is an option because emulation is not that faithful
#endif
#define SSE_STATS // v401 file + window telling what the ST program is doing
#define SSE_TOS206 // STF can run TOS206 because installation includes a GLUE hack
#define SSE_TOS_PRG_AUTORUN // Atari PRG + TOS file direct support
#define SSE_VID_BORDERS // different border sizes available
#define SSE_VID_CHECK_VIDEO_RAM // update display before writes to video memory
#define SSE_VID_NEOPIC // save screen as *.NEO file
#define SSE_VID_SIZE4 // treble & quadruple drawing
#define SSE_YM2149_LL // low-level emu (3rd party-inspired)

#ifdef WIN32
#define SSE_ARCHIVEACCESS_SUPPORT // 7z + ...
#define SSE_CARTRIDGE_ACTIVE2 // code inside cartridge
#if defined(SSE_DISK_RAR_SUPPORT)
#define SSE_DISK_RAR_SUPPORT_WIN // using unrar.dll or unrar64.dll
#endif
#define SSE_EMU_THREAD // reduces hiccups
#define SSE_GEM_CONTROL_PANEL // fun!
#define SSE_GUI_BIGICONS // for high DPI screens
#define SSE_GUI_CONFIG // config (profile) mods
//#define SSE_GUI_CONFIG_WRENCH // 420: removing the wrench icon
//#define SSE_GUI_DEFAULT_ST_CONFIG // option Default ST config // disabled v420
#define SSE_GUI_EMUCONTROL // to change some emulation parameters
#define SSE_GUI_KBD // better keyboard control
#define SSE_GUI_MENUBAR // feature + option
#define SSE_GUI_RICHEDIT
#define SSE_GUI_RICHEDIT2 // links mod
#define SSE_GUI_STATUS_BAR // feature + option
#define SSE_GUI_TOOLBAR // feature + option
#define SSE_LONG_PATH
#define SSE_MIDIRAW // (not really tested)
//#define SSE_OPTION_FASTBLITTER // hack, silly feature disabled v420
//#define SSE_OPTION_FASTLINEA // hack, even sillier disabled v420
//#define SSE_OPTION_FREQ // hack, mostly silly feature disabled v420
#ifndef SSE_NO_OSD
#define SSE_OSD_SHOW_TIME // Measure the time you waste
#endif
#define SSE_STATS_QP // using performance timers
#define SSE_STATS_RTF
#define SSE_TIMINGS_US // option Microseconds
#define SSE_VID_OLDSYNC // for people having better experience with older Steem sync method
#define SSE_VID_STVL // use low-level video logic plugin
#define SSE_WRITEDIR // WriteDir becomes TempPath and UsersPath
#endif//WIN32

#ifdef UNIX
#define SSE_DISK_7Z_SUPPORT_UNIX
#ifndef SSE_LINUX_DYN // environment setting (could reverse that - TODO)
#define SSE_DISK_CAPS_STATIC // bloat
#endif
#define SSE_TOS_KEYBOARD_CLICK // hack to suppress the click (in Control Panel in WIN32 builds)
#define SSE_UNIX_PULSEAUDIO
#if defined(SSE_DISK_RAR_SUPPORT)
#define SSE_DISK_RAR_SUPPORT_UNIX
#endif
#define SSE_VID_DISABLE_AUTOBORDER
#define SSE_NO_FREEIMAGE
#endif//UNIX

#if defined(SSE_WINDOWS_2000_MIN)
#ifndef MINGW_BUILD
#define SSE_STATS_CPU // using cool 3rd party function
#define SSE_VID_2SCREENS
#define SSE_NETWORK // TCP/IP option for ports (not really tested)
#endif
#endif

#if defined(SSE_WINDOWS_XP_MIN)
#define SSE_DIRECTMIDI // supposed to improve MIDI timings but buggy
#define SSE_DIRECTMIDI2 // better error handling than throwing an exception and bye bye...
#endif

#if defined(SSE_ACSI)
#define SSE_ACSI_FMT_AG // format using agenda, useless, untested
#define SSE_ACSI_ICD
#define SSE_ACSI_LASER // Printing - output = PBM files
#define SSE_ACSI_MNGR
#endif

#if defined(SSE_DISK_CAPS) 
//#define SSE_DISK_CAPS_MEMORY // file in memory
#define SSE_WD1772_LL // low-level elements dependency on CAPS and MAME code
#if defined(SSE_WD1772_LL)
#define SSE_DISK_SCP // Supercard Pro disk image format support
#endif
#endif

#ifdef SSE_DISK_STW
#define SSE_DISK_AUTOSTW // MFM emulation for ST, MSA, DIM
#define SSE_DISK_STW2 // manage v2 of the specification
#if defined(SSE_DISK_STX)
#define SSE_DISK_STX2STW // conversion of Pasti disk images (not all will work, it's a guessing game)
#endif
#if defined(SSE_DISK_SCP)
#define SSE_DISK_SCP2STW
#endif
#endif

#if defined(SSE_DONGLE)
#define SSE_DONGLE_PORT // dongles grouped in "virtual" port
#define SSE_DONGLE_BAT2
//#define SSE_DONGLE_CRICKET // silly games clutter the menu
#define SSE_DONGLE_JEANNEDARC
#define SSE_DONGLE_PROSOUND // Wings of Death, Lethal Xcess  STF
#define SSE_DONGLE_LEADERBOARD
#define SSE_DONGLE_MULTIFACE
#define SSE_DONGLE_MUSIC_MASTER
#define SSE_DONGLE_URC
// cubase dongles in cartridge port
//#define SSE_DONGLE_CUBASE2 // heavy, little use
#define SSE_DONGLE_CUBASE3 // for v4.2.0, but need special cartridge file
#endif//SSE_DONGLE

#ifdef SSE_DRAW_C // didn't write assembly routines for that
#define SSE_VID_SINGLEPIX
#endif

#if defined(SSE_GUI_STATUS_BAR)
//#define SSE_GUI_STATUS_BAR_DRAW_FREQ // for different colour
//#define SSE_GUI_STATUS_BAR_DRAW_MAIN  // ?
#define SSE_GUI_STATUS_BAR_DRAW_ICONS // anyway
//#define SSE_GUI_STATUS_BAR_DRAW_CAPS  // too heavy
#define SSE_GUI_STATUS_BAR_MOUSE // doesn't work with EmuTOS so...
#define SSE_GUI_STATUS_BAR_MOUSE2 // only MouseAd since v4.2.0
#endif

#if defined(SSE_MEGA) // the professional models
#define SSE_MEGAST
#define SSE_MEGASTE
#define SSE_MEGA16 // Mega ST "AdSpeed" and turbo button that never was on HW
#define SSE_MEGA_RTC // Ricoh chip
#endif

#if defined(SSE_VID_DD) // DirectDraw
#define SSE_VID_DD_3BUFFER_WIN // window Triple Buffering (DD-only)
#define SSE_VID_DD_MISC // compatibility issues
#ifndef MINGW_BUILD
#define SSE_VID_RECORD_AVI //avifile not so good 
#endif
#endif

#if defined(SSE_VID_D3D) // Direct3D
#define SSE_VID_BFI // software Black Frame Insertion
#define SSE_VID_D3D_SWEETFX // D3D9 hack
#define SSE_VID_D3D_VSYNC
#define SSE_VID_LS // Screenshot with snapshot - only in D3D builds for now
#endif

#if defined(SSE_VID_STVL) // never released ST video logic plugin
#if !defined(SSE_STDCALL) && !defined(BCC_BUILD)
#define SSE_STDCALL // appropriate functions are declared CALLBACK
#endif
#define SSE_VID_STVL1 // main
#define SSE_VID_STVL_UPD // keep STVL regs up-to-date
#ifdef SSE_DEBUG
#define SSE_VID_STVL_DBG
#endif
#define SSE_VID_STVL_DIRECT_RAM
#endif

#define SSE_420R2B  // fix power on/reset PC init confusion
#define SSE_420R3
#define SSE_420R4
#define SSE_420R5   // bugfixes & optimizations
#ifdef UNIX // 420R6: embed resources also in Linux build
#define SSE_FILES_IN_RC
#define SSE_UNIX_STATUSBAR
#endif
#define SSE_420R12 // fix option instant machine change stuck on TOS


////////////////////
// SPECIAL BUILDS //
////////////////////

//#define TEST_STEEM_INTRO
//#define SSE_OSD_EXTRACT_GRAPHICS // one-time switch
//#define SSE_DSKOS9 // one-time switch convert to STW
//#define SSE_DONGLE_CUBASE2_BUILD // object check code
//#define SSE_DONGLE_CUBASE3_BUILD // object check code
//#define SSE_HALT_CPUONLY //system runs without CPU


///////////
// DEBUG //
///////////

// SSE_DEBUG is defined or not by the environment

#define SSE_DEBUG_TRACE // all builds now
#if defined(SSE_DEBUG_TRACE)
#ifdef WIN32
#define SSE_DEBUG_TRACE_LOCK // using Windows handle for file instead of C FILE*
#ifdef _DEBUG // VC
#define SSE_DEBUG_TRACE_IDE
#endif
#endif//WIN32
#if defined(SSE_UNIX)
#define SSE_UNIX_TRACE
#endif
#endif//#if defined(SSE_DEBUG_TRACE)

#if defined(SSE_DEBUG) // Debugger + debug build

#if defined(DEBUG_BUILD) && !defined(SSE_DEBUGGER)
#define SSE_DEBUGGER
#endif

#if defined(SSE_DEBUGGER)
#define SSE_DEBUGGER_FAKE_IO //to control some debug options
#define SSE_DEBUGGER_MONITOR_RANGE // will stop for every address between 2 stops
#define SSE_DEBUGGER_MONITOR_VALUE // specify value (RW) that triggers stop
#define SSE_DEBUGGER_NODRAW // great improvement!
#define SSE_DEBUGGER_SHOWBITMAP // display ST graphics anywhere in RAM
#define SSE_DEBUGGER_STATUS_BAR
#define SSE_DEBUG_SYMBOLS
#define SSE_DEBUGGER_TOGGLE // click on bomb to toggle
#define SSE_DEBUGGER_TRACE_EVENTS
//#define SSE_IKBD_6301_DISASSEMBLE_ROM // once is enough
#define SSE_STATS_EXT // bus access stats (r/w, waitstates), debugger-only because of the load
#endif//Debugger

#if defined(SSE_DEBUGGER_FAKE_IO)
#define SSE_DEBUGGER_MUTE_SOUNDCHANNELS
#define SSE_DEBUGGER_FRAME_REPORT
#define SSE_DEBUGGER_TOPOFF
#endif//fake io

#endif//SSE_DEBUG

#if defined(SSE_DEBUG_TRACE) && (defined(SSE_DEBUG) || defined(SSE_BETA))
#define SSE_ENABLE_TRACE_LOG
#define SSE_LOGSECTIONS_FOR_ALL // democracy in action
#endif
#define SSE_TRACE_DUMP_OPTIONS // file options.txt on stop, like steem.ini without paths (privacy)


///////////////////
// LIBRETRO CORE //
///////////////////

#ifdef SSE_LIBRETRO
#define SSE_LIBRETRONUKE
#define SSE_LIBRETROSOUND1 // feeding libretro buffer (which chooses driver)
//#define SSE_LIBRETROSOUND1B // no batch test: more load...
//#define SSE_LIBRETROSOUND2 // using Steem's own DirectSound interface and buffers for ST sound
#define SSE_LIBRETRO_DRIVESOUND // using Steem's own DirectSound interface and buffers for drive sound
#define SSE_LIBRETROMULTIDISK
#define SSE_DISK_M3U // for libretro, but why not as standalone too?
#undef SSE_DEBUG_TRACE_LOCK
#undef SSE_EMU_THREAD
#undef SSE_MAIN_LOOP
#undef SSE_MAIN_LOOP2
#undef SSE_GUI_STATUS_BAR
#undef SSE_GUI_STATUS_BAR_DRAW_ICONS
#undef SSE_GUI_STATUS_BAR_MOUSE
#undef SSE_GUI_STATUS_BAR_MOUSE2
#undef SSE_DIRECTMIDI
#undef SSE_ACSI //?
#undef SSE_ACSI_LASER
#undef SSE_ACSI_MNGR
#undef SSE_ACSI_ICD
#undef SSE_STATS
#undef SSE_STATS_CPU
#undef SSE_STATS_RTF
#define NO_PASTI
//...

#endif//#ifdef SSE_LIBRETRO


////////////////////
// DEV BUILD BETA //
////////////////////

#if defined(SSE_BETA)

#define TEST01//quick switch
#define TEST02//track bug
#define TEST03
//#define TEST04
//#define TEST05
//#define TEST06 
//#define TEST07
//#define TEST08
//#define TEST09
//#define SSE_GUI_OPTION_FOR_TESTS
//#define OSD_TEST_SCROLLERS
//#define SSE_INT_MFP_TIMER_B_PULSE
//#define SSE_VID_TRACE_SIZE
#ifndef DEBUG_BUILD
//#define SSE_FORCE_TRACE_FILE // force TRACE.txt, choose log section in debug.cpp
#ifdef WIN32
#endif
#endif

#endif//#if defined(SSE_BETA)


////////////
// v4.2.1 //
////////////

#if (SSE_VERSION>=421)

#define SSE_421R0   // misc.

#ifdef WIN32
// eliminate debugger gadgets, see if there are complaints
#define SSE_BAD_DMA // simulate bad DMA chip in some STE
#define SSE_DBG_NOMONITORSCREEN
#define SSE_DBG_NOSENDKEYS
#define SSE_DBG_NOSENDMIDI
#define SSE_DBG_NOREDSCREEN
#define SSE_DBG_NOSIMULTRACE
#define SSE_DBG_NOLOADPIC
#define SSE_OSD_NORESETINFO // we have status bar (Windows)
#define SSE_GUI_QUICKSETTINGS // new option page lets you control a lot
#define SSE_GUI_421_REORDER // we rearrange all setting pages
#define SSE_GUI_DRAG_PAGES // player can drag & drop setting pages
#define SSE_GUI_RESTOREPOS // setting for Steem to save/load window positions
//#define SSE_GUI_STATUS_BAR_CLICKABLE // to undef confusing feature
#define SSE_NO_AUTOSNAPSHOTEDIT // can't edit AutoSnapShotName

#define SSE_GUI_DM_BIGICONS // bigger icons 64x64 or 128x128 in disk manager
#define SSE_GUI_DM_BIGICONS_LESS // reduce # disk icons for exe size
#if defined(SSE_GUI_DM_BIGICONS) && defined(SSE_WINDOWS_XP_MIN) // GDI+ dependency
#define SSE_GUI_DM_PICS // display custom pic of the floppy disk in disk manager when available
#endif

#endif//WIN32

#define SSE_DEFAULTS_421
#define SSE_DISK_M3U // disk swapper using m3u files
#define SSE_MEGA_421
#define SSE_MFP_421 // fix bad timer B tick suppression
#define SSE_SHIFTER_421 // C2 adjusting hacks for R0/R1 split scanlines
#if defined(SSE_GUI_EMUCONTROL)
#define SSE_SHIFTER_421B // C2 video rendering parameters GUI
#endif

#define SSE_AUTOUPDATE // will be replaced with:
#define SSE_CHECKUPDATE
#define SSE_GUI_FIX // 
#undef SSE_ONEINSTANCE

#define SSE421R1
#define SSE421R2


#endif//#if (SSE_VERSION>=421)

#endif//#ifndef SSE_H 
