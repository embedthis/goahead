/*
    A ROM build has no log files. A log path that names a file must be refused when the log opens, not accepted and
    then written to standard error as if the file had been opened.
 */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef ME_ROM
#define ME_ROM 1
#include "goahead.h"

#if ME_GOAHEAD_LOGGING
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "../../src/runtime.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

int main(int argc, char **argv)
{
    cchar *path = "rom-log-file-probe.log";

    unlink(path);
    logSetPath("stderr:1");
    teq(logOpen(), 0, "a ROM build logs to stderr");
    logClose();

    logSetPath("rom-log-file-probe.log:1");
    teq(logOpen(), -1, "a ROM build refuses a log file path instead of discarding it");
    ttrue(access(path, F_OK) != 0, "no log file is created");
    logClose();
    unlink(path);
    return 0;
}
#else
int main(int argc, char **argv)
{
    tskip("Logging is not enabled");
    return 0;
}
#endif
