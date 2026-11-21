Steem SSE HFEv3 - PicoGUS ISA sound card (test build)
=====================================================

Steem can emulate a PicoGUS 2.0 ISA sound card on an ISA bus like the
Raven060's, so the Raven's ISA drivers and tools (ISA_BIOS, pgusinit...)
run unmodified on an emulated ST. The card's GUS, Sound Blaster and AdLib
sound comes from the PicoGUS firmware's own code and is mixed into Steem's
sound output.

Setup
-----
1. Options > Sound: tick "PicoGUS ISA card".
2. On drive C: (a GEMDOS hard drive is easiest) put ISA_BIOS.PRG, from the
   Raven060 software (sw/isa/isa_bios), in the AUTO folder, and
   ISA_BIOS.INF (included) in the root: it tells ISA_BIOS where the bus is
   (isa.iobase=0xF20000, isa.endian=lels).
3. Use EmuTOS or a TOS with a cookie jar, and boot. ISA_BIOS installs the
   _ISA cookie; then run pgusinit.prg as on the Raven:
     pgusinit            card status
     pgusinit /mode sb   switch to Sound Blaster mode (gus, sb, adlib...)
     pgusinit /save      keep the settings across power cycles

The card
--------
- PicoGUS 2.0, firmware 4.2.0-beta.3 (multi-firmware). pgusinit shows
  "picogus-gus v4.2.0-beta.3 (Steem SSE emulation)".
- GUS mode: GF1 with 1MB of DRAM, at 240 by default.
- SB mode: Sound Blaster (SB16 DSP by default, /sbtype to change it) at
  220, OPL2 at 388 and 220.
- AdLib mode: OPL2 at 388.
- MPU-401, PSG and USB modes: only the control interface answers. The SB
  mode's CD-ROM emulation sees no USB drive. /flash is refused.
- No ISA DMA and no IRQ, as with ISA_BIOS on machines other than the
  Raven: software must use programmed I/O (GUS DRAM pokes, SB direct DAC).
- /save writes PicoGUS.ini next to Steem's settings: that is the card's
  flash. A cold reset of the ST powers the card off and on: it boots in
  the saved mode with the saved settings, like the real card.

The ISA bus
-----------
The ISA I/O space is at $F20000-$F2FFFF (unused on STF/STE/Mega ST): port
p is at $F20000+p. Words are lane swapped little endian, as on the Raven.
Like the Raven's 68060, word accesses at odd addresses are allowed there
(pgusinit makes them), although Steem emulates a 68000. Accesses to ports
without a device read $FF.

Not saved in snapshots. Sound recording to WAV/YM does not include the
card (AVI recording does).
