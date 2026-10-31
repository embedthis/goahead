/*
    random-device-eof.tst.c - A random device that returns end of file is a failure, not something to wait on
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

#define EMPTY_DEVICE "random-device-eof.tmp"

/*
    Stand in for the random device with an empty regular file, as a chroot or container image may provide
 */
static int fakeOpen(cchar *path, int flags, int mode)
{
    return open(EMPTY_DEVICE, O_RDONLY);
}
#define open fakeOpen
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/crypt.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef open

int main(int argc, char **argv)
{
    FILE *fp;
    char buf[16];
    int  rc;

    if ((fp = fopen(EMPTY_DEVICE, "w")) != 0) {
        fclose(fp);
    }
    //  A reader that treats end of file as progress never returns and is killed here
    alarm(5);
    rc = websGetRandomBytes(buf, sizeof(buf), 0);
    alarm(0);
    teq(rc, -1, "an empty random device is reported as a failure");
    unlink(EMPTY_DEVICE);
    return 0;
}
#else
int main(int argc, char **argv)
{
    tskip("POSIX only");
    return 0;
}
#endif
