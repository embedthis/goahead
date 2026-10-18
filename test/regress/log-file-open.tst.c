/*
    log-file-open.tst.c - Log files are created mode 0640, appended to, and never opened through a symlink
 */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

#if ME_UNIX_LIKE
#include <sys/stat.h>

#define LOG_PATH    "log-file-open.tmp"
#define CANARY_PATH "log-file-open.canary"
#define LINK_PATH   "log-file-open.link"

static void cleanup(void)
{
    unlink(LOG_PATH);
    unlink(CANARY_PATH);
    unlink(LINK_PATH);
}


static void writeFile(cchar *path, cchar *text)
{
    FILE *fp;

    if ((fp = fopen(path, "w")) != 0) {
        fputs(text, fp);
        fclose(fp);
    }
}


static char *readFile(cchar *path)
{
    char  *buf;
    ssize len;
    int   fd;

    if ((fd = open(path, O_RDONLY)) < 0) {
        return sclone("");
    }
    buf = walloc(256);
    len = read(fd, buf, 255);
    buf[len > 0 ? len : 0] = '\0';
    close(fd);
    return buf;
}
#endif


int main(int argc, char **argv)
{
#if ME_UNIX_LIKE
    struct stat sbuf;
    char        *text;
    int         fd, oldMask;

    cleanup();
    oldMask = (int) umask(0);

    fd = websOpenLogFile(LOG_PATH);
    ttrue(fd >= 0, "a new log file must open");
    teq(fstat(fd, &sbuf), 0, "the new log file must be readable");
    teq(sbuf.st_mode & 0777, 0640, "a new log file must be created mode 0640 whatever the umask");
    ttrue((fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0, "the log descriptor must not be inherited by child processes");
    close(fd);
    unlink(LOG_PATH);

    writeFile(LOG_PATH, "keep\n");
    fd = websOpenLogFile(LOG_PATH);
    ttrue(fd >= 0, "an existing log file must open");
    ttrue(write(fd, "more\n", 5) == 5, "the log file must be writable");
    close(fd);
    text = readFile(LOG_PATH);
    tmatch(text, "keep\nmore\n", "an existing log must be appended to, not truncated");
    wfree(text);
    unlink(LOG_PATH);

#if O_NOFOLLOW
    writeFile(CANARY_PATH, "canary\n");
    teq(symlink(CANARY_PATH, LINK_PATH), 0, "create a symlink to the canary file");
    teq(websOpenLogFile(LINK_PATH), -1, "a symlink at the log path must be refused");
    text = readFile(CANARY_PATH);
    tmatch(text, "canary\n", "the file behind a symlinked log path must be unchanged");
    wfree(text);
#endif
    umask((mode_t) oldMask);
    cleanup();
#else
    tskip("Log file modes are checked on POSIX only");
#endif
    return 0;
}
