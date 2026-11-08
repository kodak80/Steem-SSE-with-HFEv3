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

DOMAIN: Disk image
FILE: greaseweazle_flux.h
CONDITION: SSE_GREASEWEAZLE_FLUX must be defined (device part: WIN32 only)
DESCRIPTION: A real floppy drive on a Greaseweazle, read as raw flux (the
times between the magnetic transitions), with no decoding on the way. The
SCP reader (disk_scp.cpp) uses it in "flux mode": each track is read from
the drive as a few revolutions of flux and played to the emulated WD1772
exactly like a track of an SCP image, so timing protections (Copylock,
split bit-rate sectors, fuzzy bits...) are seen as on a real ST.
It talks to the Greaseweazle directly (USB serial, Greaseweazle protocol),
not through FloppyBridge. Read-only.
Kept to plain types so it can be unit-tested on Linux.
---------------------------------------------------------------------------*/

#pragma once
#ifndef SSEGREASEWEAZLEFLUX_H
#define SSEGREASEWEAZLEFLUX_H

#include <vector>

// Signature at the start of a flux-mode descriptor (a small text file given
// the .scp extension, so Steem's disk manager inserts it like an SCP image).
#define GREASEWEAZLE_FLUX_SIGNATURE "GREASEWEAZLE"

#define GWFLUX_MAX_REVS 5
// SCP time unit: 25ns (40 per microsecond)
#define GWFLUX_UNITS_PER_US 40

// one revolution: time of each flux transition from the index pulse, in SCP
// units (25ns), increasing; the last one may lie after the next index (the
// first transition of the following revolution, for continuity)
typedef std::vector<unsigned int> TGwFluxRev;

// Connect to the Greaseweazle described by the descriptor text. Returns a
// handle, or NULL with a message in error. log_path: log of what is read
// (log=off in the descriptor to disable), NULL for none.
void *GwFluxOpen(const char *descriptor,const char *log_path,char *error,
  int error_size);
void GwFluxClose(void *handle);
int GwFluxRevs(void *handle);        // revolutions read per track (revs=)
int GwFluxCylinders(void *handle);   // cylinders Steem may use (tracks=)
// Read side/track (side 0 = lower head): revolutions of flux from the index.
// Returns the number of revolutions read, 0 if none (no disk, error).
int GwFluxReadTrack(void *handle,int side,int track,TGwFluxRev *revs,
  int max_revs);
// The real motor follows the emulated one: GwFluxReadTrack turns it on,
// Steem turns it off when the ST's FDC does (or emulation stops).
void GwFluxMotorOff(void *handle);
// Disk change and write-protect (pins 34 and 28 of a PC drive).
#define GWFLUX_POLL_UNKNOWN -1 // can't tell (no disk-change line)
#define GWFLUX_POLL_NODISK 0
#define GWFLUX_POLL_DISK 1     // a disk is in (the same one)
#define GWFLUX_POLL_NEWDISK 2  // a disk is in, it was taken out since
int GwFluxPoll(void *handle,bool *write_protected);
void GwFluxLogMessage(void *handle,const char *msg);

// pure functions (tested)
// Decode a Greaseweazle flux stream (as sent after READ_FLUX, without the
// final 0): absolute times in sample ticks of each transition and of each
// index pulse. Returns false if the stream is malformed.
bool GwDecodeFluxStream(const unsigned char *s,int n,
  std::vector<unsigned long long> &flux,std::vector<unsigned long long> &index);
// Cut the decoded stream into revolutions (index to index), converted to SCP
// units with the sample frequency. Returns the number of revolutions.
int GwSplitRevolutions(const std::vector<unsigned long long> &flux,
  const std::vector<unsigned long long> &index,unsigned int sample_freq,
  TGwFluxRev *revs,int max_revs);
// Decode a revolution to MFM bits (simple PLL, 2us cells) for the log's
// sector check; first bit = MSB of mfm[0]. Returns the number of bits.
int GwFluxToBits(const TGwFluxRev &rev,unsigned int rev_units,
  unsigned char *mfm,int max_bytes);

#endif//#ifndef SSEGREASEWEAZLEFLUX_H
