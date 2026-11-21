/*  Runs the Atari (Raven060) build of pgusinit against the emulated PicoGUS:
    pgusinit.c is compiled into this file so that each run starts from its
    initial state, while the card keeps its state between runs, like a
    machine where pgusinit is run several times. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include "pgemu.h"

uint64_t mock_time_us = 0;
extern "C" int pgusinit_run(int argc, char **argv); // pgusinit_wrap.c
// run pgusinit with the given arguments, return its output (stdout+stderr)
static std::string run(const char *args, int *code) {
    char *argv[16];
    int argc = 0;
    static char buf[256];
    snprintf(buf, sizeof(buf), "pgusinit %s", args);
    for (char *t = strtok(buf, " "); t && argc < 15; t = strtok(NULL, " "))
        argv[argc++] = t;
    argv[argc] = NULL;
    fflush(stdout); fflush(stderr);
    FILE *tmp = tmpfile();
    int so = dup(1), se = dup(2);
    dup2(fileno(tmp), 1); dup2(fileno(tmp), 2);
    int exit_code = pgusinit_run(argc, argv);
    fflush(stdout); fflush(stderr);
    dup2(so, 1); dup2(se, 2); close(so); close(se);
    std::string out;
    rewind(tmp);
    char line[512];
    while (fgets(line, sizeof(line), tmp)) out += line;
    fclose(tmp);
    if (code) *code = exit_code;
    return out;
}

static int failures = 0;
static void expect(const char *what, const std::string &out, const char *needle, bool present = true) {
    bool found = out.find(needle) != std::string::npos;
    if (found != present) {
        failures++;
        printf("FAIL %s: %s \"%s\"\n--- output ---\n%s--------------\n", what,
            present ? "missing" : "unexpected", needle, out.c_str());
    } else
        printf("ok   %s\n", what);
}

int main(int argc, char **argv) {
    const char *flash = argc > 1 ? argv[1] : "pg_flash_test.ini";
    remove(flash);
    pgemu_set_flash_file(flash);
    pgemu_power_on(0);
    int code;
    std::string o;

    o = run("", &code);
    if (getenv("PG_VERBOSE")) fputs(o.c_str(), stdout);
    expect("detects the card", o, "PicoGUS detected: Firmware version: picogus-gus v");
    expect("PicoGUS 2.0 hardware", o, "Hardware: PicoGUS v2.0");
    expect("GUS mode at 240", o, "Running in GUS mode on port 240");
    expect("GUS DRAM probe", o, "GUS-like card detected on port 240");
    expect("GUS initialised", o, "PicoGUS initialized!");
    expect("no GUS error", o, "ERROR", false);

    o = run("/gusport 260", &code);
    expect("GUS port changed", o, "Running in GUS mode on port 260");
    o = run("", &code);
    expect("GUS port kept in RAM", o, "Running in GUS mode on port 260");

    o = run("/mode sb", &code);
    if (getenv("PG_VERBOSE")) fputs(o.c_str(), stdout);
    expect("reboot to SB", o, "Rebooting to fw: SB");
    expect("SB firmware answers", o, "picogus-sb v");
    o = run("", &code);
    if (getenv("PG_VERBOSE")) fputs(o.c_str(), stdout);
    expect("SB mode status", o, "Running in Sound Blaster 16 mode on port 220, IRQ 5, DMA 1");
    expect("OPL in SB mode", o, "AdLib port 388");
    expect("SB initialised", o, "PicoGUS initialized!");
    expect("no CD image", o, "No CD image loaded");
    expect("no SB error", o, "ERROR", false);

    o = run("/mode adlib", &code);
    expect("reboot to AdLib", o, "picogus-adlib v");
    o = run("", &code);
    if (getenv("PG_VERBOSE")) fputs(o.c_str(), stdout);
    expect("AdLib mode status", o, "Running in AdLib/OPL2 mode on port 388");

    // unsaved changes are lost on a mode change (settings come from flash)
    o = run("/mode gus", &code);
    o = run("", &code);
    expect("GUS port back to default after reboot", o, "Running in GUS mode on port 240");

    // saved settings survive a power cycle
    o = run("/gusport 280 /save", &code);
    if (getenv("PG_VERBOSE")) fputs(o.c_str(), stdout);
    expect("settings saved", o, "Settings saved");
    pgemu_power_on(mock_time_us);
    o = run("", &code);
    expect("saved GUS port after power cycle", o, "Running in GUS mode on port 280");

    o = run("/mode sb /save", &code);
    pgemu_power_on(mock_time_us);
    o = run("", &code);
    expect("saved mode after power cycle", o, "picogus-sb v");

    o = run("/defaults", &code);
    if (getenv("PG_VERBOSE")) fputs(o.c_str(), stdout);
    o = run("/mode gus", &code);
    o = run("/mainvol 50", &code);
    o = run("", &code);
    if (getenv("PG_VERBOSE")) fputs(o.c_str(), stdout);
    expect("volume set", o, "Main: 50 ");

    printf("%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
    return failures != 0;
}
