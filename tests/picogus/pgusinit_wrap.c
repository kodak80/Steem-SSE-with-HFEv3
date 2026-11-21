/*  Wraps pgusinit.c (Atari build) so it can run several times in one
    process: exit() returns to the caller and the program's globals are
    reset before each run. */
#include <setjmp.h>
#include <stdlib.h>
static jmp_buf pg_exit_jmp;
static int pg_exit_code;
static void pg_exit(int code) { pg_exit_code = code; longjmp(pg_exit_jmp, 1); }
#define exit(c) pg_exit(c)
#define main pgusinit_main
#include "pgusinit.c"
#undef main
#undef exit

int pgusinit_run(int argc, char **argv) {
    gMode = INVALID_MODE;
    newMode = INVALID_MODE;
    wifichg = permanent = resources_from_env = false;
    if (!setjmp(pg_exit_jmp))
        pg_exit_code = pgusinit_main(argc, argv);
    return pg_exit_code;
}
