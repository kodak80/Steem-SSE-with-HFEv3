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
// The real motor follows the emulated one: FloppyBridgeReadTrack turns it
// on, Steem turns it off when the ST's FDC does (or emulation stops).
void FloppyBridgeMotorOff(void *handle);
// Read one revolution of side/track (side 0 = lower head) starting at the
// index pulse, MFM bits first bit = MSB of mfm[0]. speed[i] is the time taken
// by byte i, FLOPPYBRIDGE_SPEED_NOMINAL = nominal. weak (may be NULL), same
// layout as mfm: bits that read differently from one revolution to the next
// (weak bits protection). Returns the number of bits in the revolution, 0 if
// no data could be read (no disk, timeout...).
int FloppyBridgeReadTrack(void *handle,int side,int track,
  unsigned char *mfm,unsigned short *speed,int max_bytes,unsigned char *weak);
// a line in the log (from Steem)
void FloppyBridgeLogMessage(void *handle,const char *msg);
// Disk swaps. Without a disk-change line (pin 34) FloppyBridge believes a
// disk is always in, so Steem also checks the disk itself: forget what
// FloppyBridge has cached (the next read comes from the disk), the state of
// the write-protect sensor (it changes as a disk goes in or out), and
// whether two reads of a track are the same disk (same sector IDs and data).
void FloppyBridgeForget(void *handle);
bool FloppyBridgeWriteProtected(void *handle);
bool FloppyBridgeSameTrack(const unsigned char *a,int abits,
  const unsigned char *b,int bbits);

// What the ST would find on a revolution (for the log, and to decide on a
// re-read): sector IDs with a good CRC, data fields with a good/bad CRC.
#define FLOPPYBRIDGE_CHECK_MAX 32
struct TFloppyBridgeCheck {
  int ids,bad_ids,good,bad;
  int id_track,id_side; // of the last good ID
  unsigned char id_list[FLOPPYBRIDGE_CHECK_MAX],bad_list[FLOPPYBRIDGE_CHECK_MAX];
  // each data field found: sector number and where it is (bits)
  int secs;
  unsigned char sec_id[FLOPPYBRIDGE_CHECK_MAX];
  int sec_start[FLOPPYBRIDGE_CHECK_MAX],sec_end[FLOPPYBRIDGE_CHECK_MAX];
  int sec_id_pos[FLOPPYBRIDGE_CHECK_MAX]; // first sync of the sector's ID
  int sec_crc[FLOPPYBRIDGE_CHECK_MAX]; // CRC of the data (read from disk)
  unsigned char sec_good[FLOPPYBRIDGE_CHECK_MAX]; // data CRC OK
};
void FloppyBridgeCheckTrack(const unsigned char *mfm,int nbits,
  TFloppyBridgeCheck &c);
// average speed of each sector's data, relative to the track (1000 = track
// average), eg "1:1001 2:999 ... 6:1052": shows a Copylock slow sector
void FloppyBridgeSectorSpeeds(const TFloppyBridgeCheck &c,
  const unsigned short *speed,int nbytes,char *out,int size);
// how far the most deviant sector is from the track's median sector speed,
// in 1/1000 (eg 55 = 5.5%); -1 if fewer than 3 sectors were found
int FloppyBridgeSectorDeviation(const TFloppyBridgeCheck &c,
  const unsigned short *speed,int nbytes);
// Writing (write=on in the descriptor). A track is always rewritten whole,
// from the index (the Greaseweazle can't start a write at a given sector).
// Flags describing the track being written:
#define FLOPPYBRIDGE_WRITE_FORMATTED 1 // rewritten whole by the ST (WRITE TRACK)
#define FLOPPYBRIDGE_WRITE_VARIABLE 2  // read as variable density: flattened
#define FLOPPYBRIDGE_WRITE_UNREAD 4    // couldn't be read: we don't have it
// true if Steem may write to the disk: write=on and no write-protect tab
bool FloppyBridgeCanWrite(void *handle);
// Write one track (bits as kept by Steem, first bit = MSB of mfm[0]), then
// read it back and check every sector. 1: written and verified, 0: failed,
// -1: not written (write=off, write-protected, or a track that couldn't be
// read: see the log). Variable-density tracks are written at the normal
// density (a warning is logged).
int FloppyBridgeWriteTrack(void *handle,int side,int track,
  const unsigned char *mfm,int nbits,int flags);
// Build the bits to write: the track rotated so that it starts in the
// middle of its largest gap, then cut or padded with gap so that it lasts
// ~1% more than a revolution (rev_bits): the end overwrites the start inside
// the gap and nothing of the old track is left. Returns the number of bits,
// 0 if out_size is too small.
int FloppyBridgePrepareWrite(const unsigned char *mfm,int nbits,int rev_bits,
  unsigned char *out,int out_size,int *gap_bits,int *rotate_bits);
// Bad sectors of a revolution, compared with other revolutions of the track:
// a sector read correctly on another revolution is copied from it (a weak
// read); a sector bad on every revolution that reads differently each time
// has weak bits: the bytes that differ are marked in weak. One that reads
// the same every time (a deliberate CRC error, eg Copylock) is left alone.
struct TFloppyBridgeMerge {
  int repaired,weak_sectors,weak_bytes;
  unsigned char repaired_list[FLOPPYBRIDGE_CHECK_MAX];
  unsigned char weak_list[FLOPPYBRIDGE_CHECK_MAX];
};
void FloppyBridgeMergeRevolutions(unsigned char *base,int nbits,
  const unsigned char *const *others,const int *others_bits,int nothers,
  unsigned char *weak,TFloppyBridgeMerge &m);
// Fuzzy bits found by timing (see the .cpp): in each sector with a bad CRC,
// data bytes whose measured time is >10% off the sector's median are marked
// in weak if there are at least 8. speed: raw per-MFM-byte speeds. Returns
// the number of sectors marked (their numbers in list), the bytes marked in
// weak_bytes; stats (4 per sector of c, may be NULL): median, bytes off,
// bytes, max deviation in 1/1000.
int FloppyBridgeTimingWeak(const TFloppyBridgeCheck &c,const unsigned short *speed,
  int nbytes,int nbits,unsigned char *weak,int *weak_bytes,
  unsigned char *list,int *stats);
// smooth/normalise per-byte speeds (1000 = nominal) as density= says; in
// auto mode c (the sectors found on the track) decides, see the .cpp
void FloppyBridgeSmoothSpeeds(unsigned short *speed,int nbytes,int mode,
  int *min_out,int *max_out,const TFloppyBridgeCheck *c=0);

#endif//#ifndef SSEFLOPPYBRIDGE_H
