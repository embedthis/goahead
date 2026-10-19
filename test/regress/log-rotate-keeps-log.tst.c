/*
    Failed log rotation must preserve the live log. Exercise unwritable paths and a non-empty
    path.old directory using permission bits so the tests run unprivileged.
 */

#include "testme.h"
#include "goahead.h"

#if ME_UNIX_LIKE && ME_GOAHEAD_LOGGING && !ME_ROM
#include <sys/stat.h>

#define DIR_PATH  "log-rotate-keeps-log.dir"
#define LOG_PATH  DIR_PATH "/server.log"
#define OLD_PATH  LOG_PATH ".old"
#define OLD_INNER OLD_PATH "/keep"

static ssize fileSize(cchar *path)
{
    struct stat sbuf;

    return (stat(path, &sbuf) == 0) ? (ssize) sbuf.st_size : -1;
}


static void cleanup(void)
{
    chmod(DIR_PATH, 0755);
    chmod(LOG_PATH, 0640);
    unlink(LOG_PATH);
    unlink(OLD_INNER);
    rmdir(OLD_PATH);
    unlink(OLD_PATH);
    rmdir(DIR_PATH);
}


static void fillPastLimit(WebsLogFile *log)
{
    char  line[4096];
    ssize total;

    memset(line, 'x', sizeof(line) - 1);
    line[sizeof(line) - 1] = '\n';
    for (total = 0; total <= ME_GOAHEAD_LIMIT_LOG_SIZE + (ssize) sizeof(line); total += (ssize) sizeof(line)) {
        websWriteLogFile(log, line, (ssize) sizeof(line));
    }
}


static void suiteA_unwritable(void)
{
    WebsLogFile log;
    ssize       before, after;

    tinfo("\n--- Suite A: the log and its directory cannot be written after a privilege drop ---\n");
    cleanup();
    ttrue(mkdir(DIR_PATH, 0755) == 0, "create the log directory");
    memset(&log, 0, sizeof(log));
    log.fd = -1;
    teq(websStartLogFile(&log, LOG_PATH), 0, "open the log while privileged");
    websWriteLogFile(&log, "first line\n", 11);
    before = fileSize(LOG_PATH);
    /* Now the server runs as another account: the file and directory are not writable by it */
    ttrue(chmod(LOG_PATH, 0440) == 0 && chmod(DIR_PATH, 0555) == 0, "make the log read-only");

    fillPastLimit(&log);
    ttrue(log.fd >= 0, "a rotation that cannot rename or reopen the log must keep the open descriptor");
    after = fileSize(LOG_PATH);
    ttrue(after > before, "and the log must keep growing in place");
    websWriteLogFile(&log, "last line\n", 10);
    ttrue(fileSize(LOG_PATH) == after + 10, "every later message must still reach the log");
    websCloseLogFile(&log);
    cleanup();
}


static void suiteB_rename_fails(void)
{
    WebsLogFile log;
    FILE        *fp;

    tinfo("\n--- Suite B: a failed rename must not unlink the live log ---\n");
    cleanup();
    ttrue(mkdir(DIR_PATH, 0755) == 0, "create the log directory");
    /* A non-empty directory in the way of path.old makes the rename fail */
    ttrue(mkdir(OLD_PATH, 0755) == 0, "create path.old as a directory");
    if ((fp = fopen(OLD_INNER, "w")) != 0) {
        fputs("keep\n", fp);
        fclose(fp);
    }
    memset(&log, 0, sizeof(log));
    log.fd = -1;
    teq(websStartLogFile(&log, LOG_PATH), 0, "open the log");
    websWriteLogFile(&log, "audit line\n", 11);

    fillPastLimit(&log);
    ttrue(fileSize(LOG_PATH) > ME_GOAHEAD_LIMIT_LOG_SIZE, "the log written before the failed rotation must survive");
    ttrue(log.fd >= 0, "and logging must continue");
    websCloseLogFile(&log);
    cleanup();
}
#endif


int main(int argc, char **argv)
{
#if ME_UNIX_LIKE && ME_GOAHEAD_LOGGING && !ME_ROM
    if (geteuid() == 0) {
        tskip("root ignores file permissions, so the rotation failure cannot be simulated");
        return 0;
    }
    suiteA_unwritable();
    suiteB_rename_fails();
#else
    tskip("Log files are POSIX only");
#endif
    return 0;
}
