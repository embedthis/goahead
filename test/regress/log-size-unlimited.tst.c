/*
    log-size-unlimited.tst.c - With ME_GOAHEAD_LIMIT_LOG_SIZE set to 0 a log file is never rotated
 */

#define ME_GOAHEAD_LIMIT_LOG_SIZE 0

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/runtime.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_LOGGING && !ME_ROM
#define LOG_PATH "log-size-unlimited.tmp"
#define OLD_PATH "log-size-unlimited.tmp.old"

static ssize fileSize(cchar *path)
{
    struct stat sbuf;

    return (stat(path, &sbuf) == 0) ? (ssize) sbuf.st_size : -1;
}
#endif


int main(int argc, char **argv)
{
#if ME_GOAHEAD_LOGGING && !ME_ROM
    int i;

    unlink(LOG_PATH);
    unlink(OLD_PATH);
    logSetPath(LOG_PATH ":5");
    teq(logOpen(), 0, "open the log");
    for (i = 0; i < 400; i++) {
        logmsg(1, "line %d %s", i, "0123456789012345678901234567890123456789");
    }
    logClose();
    ttrue(fileSize(LOG_PATH) > 16384, "a log with no limit must keep growing");
    teq(fileSize(OLD_PATH), -1, "a log with no limit must never be rotated");
    unlink(LOG_PATH);
#else
    tskip("Logging to a file is not compiled in");
#endif
    return 0;
}
