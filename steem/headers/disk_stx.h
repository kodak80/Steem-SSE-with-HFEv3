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
FILE: disk_stx.h
CONDITION: SSE_DISK_STX must be defined
DESCRIPTION: Declarations for native STX disk image support
Native suport is necessary because we don't think there will be a Pasti
plugin for Linux, and for the moment there's no Windows 64bit plugin either.
This is based on the unofficial Pasti file format documentation by DrCoolZic
and especially sarnau (see 3rdparty/doc).
namespace NStx, struct TStxTrackDesc, TStxSectorDesc and TImageSTX
---------------------------------------------------------------------------*/

#pragma once
#ifndef SSE_DISK_STX_H
#define SSE_DISK_STX_H

#if defined(SSE_DISK_STX)
#include <easystr.h>
#include <fdc.h>

namespace NStx
{
  const WORD INVALID_TRACKBYTES=0xCCCC; // incorrect Discovery track length, we take it as 'impossibly high'
  const BYTE SECT_FLAG_MACRODOS=BIT_0; // fake flag
  const BYTE SECT_FLAG_FUZZY=BIT_7; // fake flag
  const BYTE TRK_SYNC=0x80; // track image header contains sync offset info
  const BYTE TRK_IMAGE=0x40; // track record contains track image
  const BYTE TRK_PROT=0x20; // track contains protections ? not used?
  const BYTE TRK_SECT=0x01; // track record contains sector descriptor
  const WORD NORMAL_TIMING=1000; // our arbitrary value
}

// STX file header description (used only in disk_stx.cpp)
struct TStxFileDesc {
  char pastiFileId[4]; // File Identifier "RSY\0"
  WORD version; // File version number
  WORD tool; // Tool used to create image
  WORD reserved_1; // reserved 1
  BYTE trackCount; // Number of track records following
  BYTE revision; // File revision number
  DWORD reserved_2; // reserved 2
};


// track record description
struct TStxTrackDesc {
  DWORD recordSize; // Track record size
  DWORD fuzzyCount; // number of bytes in fuzzy mask record
  WORD sectorCount; // number of sector in track
  WORD trackFlags; // bitmask info for track record
  WORD trackLength; // Total bytes (Bit rate)
  BYTE trackNumber; // track number (coded for both side)
  BYTE trackType; // track image type
};


// sector description
struct TStxSectorDesc {
  DWORD dataOffset; // offset of sector data in the track data record
  WORD bitPosition; // Position in bits of the sector from start of track
  WORD readTime; // sector read time in ms
  TWD1772IDField id; // Copy of the id field of a sector (6 bytes)
  BYTE fdcFlags; // fdc status and flags
  BYTE reserved; // Not used Always 00
};


/*  Manager of STX disk images (Pasti)
*   This is no MFM emulation contrary to other TImage... objects.* 
*   The TImageSTX object just delivers data, status byte, and some timings in "bytes", which
*   must be converted (to scanlines in Steem's current implementation, but it could be cycles).
*   The object was designed to be useful both for live emulation and for disk image conversion.
*   TImageSTX objects use the TFloppyDisk objects.
*/
struct TImageSTX {

  // interface

/*  Close deletes the memory buffer where the file was copied and resets the STX manager.
*   It doesn't save the image nor close the file because the file handle belongs to the 
*   corresponding TFloppyDisk object.
*/
  void Close();

/*  Find the first ID track-num from the indicated starting point
*   The function works with the currently loaded track.
*   bytes is an in/out parameter; in: current track byte, out: distance to first ID byte.
*   For other parameters, -1=don't care.
*   For emulation side should always be -1, the WD1772 doesn't check side (Turrican II).
*   You know which side you're on because you loaded the correct track.
*   track and num are the values that would be in the ID.
*   This function doesn't call LoadTrack().
*   Returns the ID order, zero-based, $FF means not found if bytes is INVALID_TRACKBYTES.
*/
  BYTE FindID(WORD& bytes,SHORT side,SHORT track,SHORT num);

/*  Find sector and update sector variables
*   side and track are the physical values (where the data is on the disk).
*   order is the zero-based sector ID position, regardless of the num field of the ID.
*   The function calls GetSector(BYTE side,BYTE track,BYTE sr,WORD &bytes), with the
*   effects described, and returns the same value.
*/
  bool GetSector(BYTE side,BYTE track,BYTE order);

/*  Find sector sr and compute data access timing
*   side and track are the physical values (where the data is on the disk).
*   sr is the sector number as recorded in the ID; possibly there are several sectors
*   with the same number in the track.
*   bytes is an in/out parameter; in: current track byte, out: distance to first data byte.
*   If the current track byte input parameter is invalid, sr is the zero-based sector
*   position instead of the number as recorded in the ID (this is used by the overload).
*   If the sector is found, pSectorData is set, and the file pointer is set to
*   the start of the sector data bytes.
*   Also bFuzzySector, bMacrodos, pFuzzyTable, SectorFlags, SectorTiming
*   and #sector bytes are updated.
*   That function calls LoadTrack() and FindID() if bytes is valid
*   Returns true if the sector was found.
*/
  bool GetSector(BYTE side,BYTE track,BYTE sr,WORD &bytes);

/*  Prepare access to a track record
*   side and track are the physical values (where the data is on the disk).
*   If there is a track image, pTrackBytes is set, and the file pointer is set to
*   the start of the image data bytes.
*   Also pSectorDesc, FirstSync, pFuzzyTable, TrackTiming, #sectors
*   and #track bytes are updated.
*   Returns true if the track record exists.
*/
  bool LoadTrack(BYTE side,BYTE track);

/*  The STX image file must be opened before it can be used.
*   It may be open already in the TFloppyDisk object.
*   It will be opened in read/write mode if possible.
*   The file is copied into memory, where it is parsed for basic track information
*   pTrackDescTable is filled.
*   The file stays open and Steem SSE directly works with the file (as for ST, MSA).
*   Returns false if the operation failed.
*/
  bool Open(EasyStr FilePath);
  
  // internal
  
  TImageSTX() {
    Init();
  }
  ~TImageSTX() {
    Close(); // the object can be a temporary, important to close
  }
  void Init();
  
  // data
  
  BYTE* pStxImage; // pointer to the full image file copy; Steem doesn't update it with writes
  BYTE* pStxTrackData; // pointer to the current track record
  BYTE* pFuzzyTable; // pointer to the fuzzy bytes table, if available
  BYTE* pTrackBytes; // pointer to track image, if available

/*  Track descriptors
*   pTrackDesc is a pointer to the current track descriptor.
*   pTrackDescTable[256] holds direct pointers to all possible descriptors, it is filled
*   up on opening.
*   The table holds NULL values when there's no track record (single-sided disks, tracks
*   beyond 79).
*/
  TStxTrackDesc *pTrackDesc,*pTrackDescTable[256];

/*  Pointer to the sector descriptor if it exists, otherwise NULL
*   It may exist only for some tracks (Dungeon Master).
*   The number of sector descriptors is given in the track descriptor.
*/
  TStxSectorDesc *pSectorDesc;

/*  TrackTiming is 1000 for normal tracks, lower for slow tracks, higher for fast tracks.
*   It is set by LoadTrack().
*/
  WORD TrackTiming;

/*  SectorTiming is 1000 for normal sectors, higher for slow sectors, lower for fast sectors.
*   So the opposite of TrackTiming! (because TrackTiming is based on #bytes while SectorTiming
*   is based on time measurement).
*   It is set by GetSector().
*/
  WORD SectorTiming;

/*  FirstSync is the byte position of the first $A1 sync mark of the track, 0 if no information
*   about it on the disk image.
*/
  WORD FirstSync;

/*  0 for drive A etc. Makes communicating with other disk-related objects easier.
*   Not to be confused with id, the WD1772 sector header.
*/
  BYTE Id;

/*  Those are the flags set by the WD1772 when trying to read the sector (without the pasti
*   internal flags), in practice bit 3 (CRC), bit 4 (RNF) and bit 5 (RT) are of interest.
*/
  BYTE SectorFlags;

/*  LoadTrack always updates those variables with its parameters, they are used to
*   limit logging.
*/
  BYTE CurrentSide,CurrentTrack;

/*  bFuzzySector & bMacrodos are set for each sector by GetSector().
*   If bFuzzySector is set, then pFuzzyTable points to the fuzzy bits mask, a table
*   as long as the sector data.
*   If bMacrodos is set, then no more information is available, client code handles
*   the timing variations by itself (in fdc.cpp).
*   The fact that this information is sector-based only is a shortcoming of the STX format.
*   Only some titles can't be properly imaged because of that: Power Drift, Vroom
*/
  bool bFuzzySector,bMacrodos;

/*  ID of the current sector, we try to keep it up-to-date */
  TWD1772IDField id;
};

#endif//#if defined(SSE_DISK_STX)

#endif//#ifndef SSE_DISK_STX_H
