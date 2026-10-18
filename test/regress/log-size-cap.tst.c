/*
    log-size-cap.tst.c - A log file is rotated to path.old when it reaches ME_GOAHEAD_LIMIT_LOG_SIZE
 */

#define ME_GOAHEAD_LIMIT_LOG_SIZE 4096

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
#define LOG_PATH "log-size-cap.tmp"
#define OLD_PATH "log-size-cap.tmp.old"

static ssize fileSize(cchar *path)
{
    struct stat sbuf;

    return (stat(path, &sbuf) == 0) ? (ssize) sbuf.st_size : -1;
}


static void cleanup(void)
{
    unlink(LOG_PATH);
    unlink(OLD_PATH);
}
#endif


int main(int argc, char **argv)
{
#if ME_GOAHEAD_LOGGING && !ME_ROM
    FILE *fp;
    char line[80];
    int  i;

    cleanup();


    if ((fp = fopen(LOG_PATH, "w")) != 0) {
        for (i = 0; i < 3000; i++) {
            fputc('x', fp);
        }
        fclose(fp);
    }
    logSetPath(LOG_PATH ":5");
    teq(logOpen(), 0, "open an existing log");
    for (i = 0; i < 100; i++) {
        memset(line, 'a', sizeof(line) - 1);
        line[sizeof(line) - 1] = '\0';
        logmsg(1, "%s", line);
    }
    ttrue(fileSize(OLD_PATH) > 0, "the log must be rotated to path.old when it reaches the limit");
    ttrue(fileSize(LOG_PATH) > 0, "logging must continue in a new file");
    ttrue(fileSize(OLD_PATH) <= ME_GOAHEAD_LIMIT_LOG_SIZE, "the rotated log must not exceed the limit");
    ttrue(fileSize(LOG_PATH) <= ME_GOAHEAD_LIMIT_LOG_SIZE, "the log must not exceed the limit");

    /* Past three times the limit both files exist and stay within it */
    for (i = 0; i < 200; i++) {
        logmsg(1, "line %d %s", i, "0123456789012345678901234567890123456789");
    }
    ttrue(fileSize(OLD_PATH) > 0 && fileSize(OLD_PATH) <= ME_GOAHEAD_LIMIT_LOG_SIZE,
          "path.old must stay within the limit");
    ttrue(fileSize(LOG_PATH) > 0 && fileSize(LOG_PATH) <= ME_GOAHEAD_LIMIT_LOG_SIZE,
          "the log must stay within the limit");
    logClose();
    cleanup();
#else
    tskip("Logging to a file is not compiled in");
#endif
    return 0;
}
