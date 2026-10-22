#pragma once
#ifndef SOUND_H
#define SOUND_H

struct TWavFileFormat { // from ?
  char riff[4];		    // 4 bytes
  LONG filesize;	    // 4 bytes
  char wave[4];		    // 4 bytes
  char fmt[4];		    // 4 bytes
  LONG chunkSize;       // 4 bytes
  short wFormatTag;     // 2 bytes
  short nChannels;      // 2 bytes
  LONG nSamplesPerSec;  // 4 bytes
  LONG nAvgBytesPerSec; // 4 bytes
  short nBlockAlign;    // 2 bytes
  short wBitsPerSample; // 2 bytes
  char data[4];		    // 4 bytes
  LONG length;			// 4 bytes
} ;

#endif//#ifndef SOUND_H
