/*  Steem SSE PicoGUS emulation: Nuked OPL3 (sw/opl/opl3.c, Copyright (C)
    2013-2018 Alexey Khokholov, GPL v2 or later), the OPL2 core. Built as
    C++ with C linkage, like the rest of the card. */
#include "pg_rename.h"
extern "C" {
#include "sw/opl/opl3.c"
}
