/*
    closed-stdio.tst.c - websOsOpen reserves descriptors 0-2 so no socket or file can take them
 */

#include "testme.h"
#include "goahead.h"

#if ME_UNIX_LIKE
static bool isDevNull(int fd, struct stat *null)
{
    struct stat info;

    return fstat(fd, &info) == 0 && info.st_rdev == null->st_rdev && info.st_ino == null->st_ino;
}


int main(int argc, char **argv)
{
    struct stat null, before, after;
    bool        reserved[3], above;
    int         saved[3], fd, i;

    ttrue(stat("/dev/null", &null) == 0, "stat /dev/null");
    for (i = 0; i < 3; i++) {
        saved[i] = dup(i);
    }
    close(0);
    close(1);
    close(2);

    websOsOpen();
    for (i = 0; i < 3; i++) {
        reserved[i] = isDevNull(i, &null);
    }
    fd = open("/dev/null", O_RDONLY);
    above = fd > 2;
    close(fd);

    for (i = 0; i < 3; i++) {
        dup2(saved[i], i);
        close(saved[i]);
    }
    websOsClose();

    ttrue(reserved[0], "stdin closed at startup is reserved on /dev/null");
    ttrue(reserved[1], "stdout closed at startup is reserved on /dev/null");
    ttrue(reserved[2], "stderr closed at startup is reserved on /dev/null");
    ttrue(above, "the next descriptor opened is above 2");

    /* Open stdio is left alone */
    fstat(1, &before);
    websOsOpen();
    fstat(1, &after);
    ttrue(before.st_ino == after.st_ino && before.st_dev == after.st_dev, "open stdout is not replaced");
    websOsClose();
    return 0;
}
#else
int main(int argc, char **argv)
{
    tskip("POSIX only");
    return 0;
}
#endif
