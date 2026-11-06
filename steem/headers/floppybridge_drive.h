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
FILE: floppybridge_drive.h
CONDITION: SSE_FLOPPYBRIDGE must be defined
DESCRIPTION: A real floppy drive on a Greaseweazle (or DrawBridge, SuperCard
Pro) used as a Steem drive, through Rob Smith's FloppyBridge. The HFE reader
(disk_hfe.cpp) uses it in "bridge mode": each track is read from the drive
as an MFM bit stream plus the measured speed of each byte, and stored like a
decoded HFEv3 track, so the density-aware reader handles it the same way.
Kept to plain types so the HFE unit tests can mock it.
---------------------------------------------------------------------------*/

#pragma once
#ifndef SSEFLOPPYBRIDGE_H
#define SSEFLOPPYBRIDGE_H

// Signature at the start of a bridge descriptor file (a small text file
// given the .hfe extension, so Steem's disk manager lists and inserts it like
// a disk image).
#define FLOPPYBRIDGE_SIGNATURE "FLOPPYBRIDGE"

// nominal speed returned by FloppyBridgeReadTrack (cell time = 100%)
#define FLOPPYBRIDGE_SPEED_NOMINAL 1000

// density= in the descriptor: what is kept of the measured speed
#define FLOPPYBRIDGE_DENSITY_AUTO 0 // smoothed, only real density changes
#define FLOPPYBRIDGE_DENSITY_OFF 1  // always nominal
#define FLOPPYBRIDGE_DENSITY_RAW 2  // as measured, byte by byte

// Connect to the drive described by the descriptor text. Returns a handle,
// or NULL with a message in error. log_path: where to write the log of what
// is read (log=off in the descriptor to disable), NULL for none.
void *FloppyBridgeOpen(const char *descriptor,const char *log_path,
  char *error,int error_size);
void FloppyBridgeClose(void *handle);
// Number of cylinders the drive can reach (eg 80, 82, 84).
int FloppyBridgeMaxCylinder(void *handle);
bool FloppyBridgeDiskInDrive(void *handle);
// Read one revolution of side/track (side 0 = lower head) starting at the
// index pulse, MFM bits first bit = MSB of mfm[0]. speed[i] is the time taken
// by byte i, FLOPPYBRIDGE_SPEED_NOMINAL = nominal. Returns the number of bits
// in the revolution, 0 if no data could be read (no disk, timeout...).
int FloppyBridgeReadTrack(void *handle,int side,int track,
  unsigned char *mfm,unsigned short *speed,int max_bytes);

// What the ST would find on a revolution (for the log, and to decide on a
// re-read): sector IDs with a good CRC, data fields with a good/bad CRC.
#define FLOPPYBRIDGE_CHECK_MAX 32
struct TFloppyBridgeCheck {
  int ids,bad_ids,good,bad;
  int id_track,id_side; // of the last good ID
  unsigned char id_list[FLOPPYBRIDGE_CHECK_MAX],bad_list[FLOPPYBRIDGE_CHECK_MAX];
};
void FloppyBridgeCheckTrack(const unsigned char *mfm,int nbits,
  TFloppyBridgeCheck &c);
// smooth/normalise per-byte speeds (1000 = nominal) as density= says
void FloppyBridgeSmoothSpeeds(unsigned short *speed,int nbytes,int mode,
  int *min_out,int *max_out);

#endif//#ifndef SSEFLOPPYBRIDGE_H
