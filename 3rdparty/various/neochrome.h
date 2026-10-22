#pragma once
#ifndef NEOCHROME_H
#define NEOCHROME_H
//http://wiki.multimedia.cx/index.php?title=Neochrome

#pragma pack(push, 8)

struct neochrome_file{
 uint16_t flags;                      /* always 0 */
 uint16_t resolution;                 /* 0 = low (320x200x16), 1 = medium (640x200x4), 2 = high (640x400x2) */
 uint16_t palette[16];                /* 9-bit RGB 00000RRR0GGG0BBB */
 uint8_t filename[12];                /* 8 '.' 3 */
 uint16_t color_animation_limits;
 uint16_t color_animation_speeddir;
 uint16_t color_animation_steps;
 uint16_t x_offset;                   /* always 0 */
 uint16_t y_offset;                   /* always 0 */
 uint16_t width;
 uint16_t height;
 uint16_t reserved[33];
 uint16_t data[16000];
} ;

#if 0
// just in case, no plan to add those formats for the moment
// https://www.fileformat.info/format/atari/egff.htm
typedef struct _DegasHeader
{
	WORD Resolution;      /* Image resolution */
	WORD Palette[16];     /* Color palette */
} DEGASHEAD;

typedef struct _DegasEliteFooter /* Degas Elite */
{
	WORD LeftColor[4];    /* Left color animation limit table */
	WORD RightColor[4];   /* Right color animation limit table */
	WORD Direction[4];    /* Animation channel direction flag */
	WORD Delay[4];        /* Animation channel delay */
} DEGASELITEFOOT;

/*
*  The DEGAS Elite Compressed format contains the same header and footer as the DEGAS Elite format, with one variation in the header data.

The Resolution field uses the following bit values to indicate the resolution of the image data:

8000h 	Low resolution
8001h 	Medium resolution
8002h 	High resolution

The compression algorithm used is identical to RLE scheme found in the Interchange file format (IFF);
*/

#endif

#pragma pack(pop)

#endif//#ifndef NEOCHROME_H
