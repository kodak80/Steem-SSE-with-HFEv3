// test build of steem/floppybridge_drive.cpp on Linux: the real FloppyBridge
// client is compiled (it finds no DLL), the pure functions are tested
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
typedef unsigned char BYTE;
typedef unsigned short WORD;
#define SSE_FLOPPYBRIDGE
