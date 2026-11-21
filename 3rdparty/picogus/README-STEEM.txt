PicoGUS card emulation for Steem SSE
====================================

Emulates a PicoGUS 2.0 (https://github.com/polpo/picogus) running the
multi-firmware, as seen from the ISA bus: the control interface that
pgusinit uses (ports 1D0-1D2), mode switching through the bootloader,
settings saved to "flash" (a text file), and the modes that make sound:

  GUS     GF1 with 1MB DRAM (PSRAM), the firmware's sw/gus/gus-x.cpp
  SB      Sound Blaster DSP (sw/sbdsp/sbdsp.cpp) and OPL2 at 388 and
          the SB base port
  AdLib   OPL2 at 388

MPU-401, PSG and USB modes answer on the control port only. The CD-ROM
interface of the SB firmware reports no USB drive. There is no ISA DMA
and no IRQ: on the Raven060 the ISA bus has no DMA, and ISA_BIOS only
routes IRQs on the Raven itself. Firmware updates (/flash) are refused.

Sources
-------
sw/ and common/ come from polpo/picogus, commit
44bbe5d53087e1e8ce3af0708911b31d642d24dc (2026-11-19, firmware
4.2.0-beta.3), GPL v2 or later (sbdsp: MIT). The OPL2 is synthesised by
Nuked OPL3 (sw/opl/opl3.c, from the same repository); the timer and status
handling follows the firmware's opl_pico.c.

Local changes to those files are in steem-changes.diff (MSVC has no
compound literals or case ranges in C++; pg_debug.h prints to stderr
instead of the Pico's UART). Everything else is used unmodified.

Steem's own files
-----------------
pgemu.h/.cpp    the card: control interface, port decode, audio mixing
                (from the firmware's picogus.cpp, gusplay.cpp, sbplay.cpp,
                flash_settings.c, multifw/bootloader.c)
pg_*.cpp        the firmware sources built as in each firmware
pg_rename.h     included first: generic global names get a pg_ prefix
shim/           replacement Pico SDK headers: emulated time and timer
                events, PSRAM as an array, the RP2040 interpolator's clamp
                mode (the GUS firmware's output stage), no ISA DMA

steem/isa_picogus.cpp puts the card on an ISA bus window at $F20000 of the
ST's address space. tests/picogus has the tests, including one that builds
the Atari (Raven060) pgusinit.c against the emulated card.

To update from upstream: copy the files listed above from a new picogus
checkout, apply steem-changes.diff, run tests/picogus.
