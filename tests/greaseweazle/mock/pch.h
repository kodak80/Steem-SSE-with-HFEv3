// test build of steem/greaseweazle_flux.cpp on Linux: the pure functions
// (flux stream decoding, revolutions, PLL) are tested, no device
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
typedef unsigned char BYTE;
typedef unsigned short WORD;
#define SSE_FLOPPYBRIDGE
#define SSE_GREASEWEAZLE_FLUX
