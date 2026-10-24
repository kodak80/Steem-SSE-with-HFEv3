# CAPS/IPF investigation — Judge Dredd (Virgin) "Disc error during boot sequence"

Branch to investigate IPF images that work in Hatari but fail in Steem SSE.
Test case: `JudgeDreddVirginGames-D1.IPF`. Fails identically in stock Steem SSE
4.2.1 and in the HFEv3 build, so this is a pre-existing CAPS-integration issue,
not related to the HFEv3 work.

## Symptom

The game boots, its own loader runs, then prints:

    Disc error during boot sequence
    Please Re-boot with correct disc in drive!

The drive display sits at `A:0-00-00`. Works in Hatari (which uses the same
capsimg/SPS library), so the IPF is good and the library decodes it — the
difference is in how Steem drives CAPS.

## Reproduction (Linux, headless)

Steem's Linux port (X-build) builds against the shipped CAPS source in
`3rdparty/caps_linux/.../CAPSImg`. It can be built and run under Xvfb with
EmuTOS, which reproduces the exact "Disc error" screen. This gives a fast
iterate/trace loop without Windows. (Build it 32-bit to match the real
releases — see the 64-bit caveat below.)

## What the loader does (FDC command trace)

The protection is a Rob Northen **Copylock** check on **track 2**. The loader
loops:

    seek track 0  (0x13)
    Read Track    (0xE0)  of track 2
    ... derive key, compare, fail ...
    retry

`Read Track` (type III, 0xE0) returns the raw track; the Copylock key is
derived from it. `CapsGetInfo(cgiitRevolution)` reports `max = -1` for this
track, i.e. it is a **density-based** Copylock (the key lives in bit-cell
density variations carried by the CAPS timing buffer, `DI_LOCK_DENVAR` /
`timebuf`), not flakey multi-revolution bits.

## Differences from Hatari's IPF driver (src/floppies/ipf_caps.c)

Reviewed against Hatari, which plays this game. Found three divergences:

1. **CapsLoadImage** — Hatari calls
   `CAPSLoadImage(id, DI_LOCK_DENALT|DI_LOCK_DENVAR|DI_LOCK_UPDATEFD)` after
   locking the image. Steem never calls `CapsLoadImage` at all.
2. **Revolution reset location** — Hatari resets the revolution counter
   (`CAPSSetRevolution(id,0)`) when a type II/III command starts on a
   track/side that changed since the last reset (SPS-recommended). Steem
   resets it inside the track-change callback instead.
3. **FDC clock** — Hatari sets `fdc.clockfrq = 8000000` (WD1772 is 8 MHz).
   Steem sets `CpuNormalHz/TICKS8` = 8021247, with the author's own comment
   "should have different clock?".

## Tested, did NOT fix it (stabilised harness, still "Disc error")

- Adding `CapsLoadImage(...)` after the lock (Hatari-style).
- Resetting the revolution counter on the type II/III command (Hatari-style),
  in `TCaps::WriteWD1772`, instead of in the track callback.
- Setting `fdc.clockfrq = 8000000`.
- All three together.

So the Copylock key is still derived wrong after these. The remaining
difference must be in the **type-III Read Track path** or the **DRQ/DMA
transfer** during it, or in how the density/timing buffer is applied — i.e.
the raw bytes Steem's CAPS hands back for track 2 differ from Hatari's.

## Next step

Build Hatari with CAPS here and capture its `Read Track` output for track 2,
side 0, byte-for-byte, then diff against Steem's. That pinpoints exactly where
the two decoders/readers diverge and what the fix is.

## 64-bit harness caveat (not a Steem bug)

When the Linux harness is built 64-bit, a stray single-byte write at power-on
zeroes one byte of `fdc.cbtrk` (a function pointer). On the real 32-bit
releases the pointer is 4 bytes so that byte is harmless; on 64-bit it
truncates the pointer and the CAPS FDC read loop jumps to a bad address once a
track-change callback fires. This only appears in a 64-bit build of the Linux
harness — re-assert the three `fdc.cbxxx` hooks each emulate step to work
around it while testing, or build the harness 32-bit. It is NOT the cause of
the Judge Dredd failure and does not affect the Windows releases.
