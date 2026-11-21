/*  Steem SSE: replacement for the firmware's sw/include/pg_debug.h, which
    prints to the Pico's UART. Debug output is compiled out here; define
    PGDEBUG to send it to stderr. */
#pragma once
#include <stdio.h>
#define ERR_PUTS(s) fprintf(stderr, "PicoGUS: %s\n", s)
#ifdef PGDEBUG
#define DBG_PRINTF(fmt, ...)  fprintf(stderr, fmt, ##__VA_ARGS__)
#define DBG_PUTS(s)           fprintf(stderr, "%s\n", s)
#define DBG_PUTCHAR(c)        fputc(c, stderr)
#else
#define DBG_PRINTF(fmt, ...)  ((void)0)
#define DBG_PUTS(s)           ((void)0)
#define DBG_PUTCHAR(c)        ((void)0)
#endif
