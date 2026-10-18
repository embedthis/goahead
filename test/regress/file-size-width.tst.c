/*
    file-size-width.tst.c - File sizes and response lengths keep their full 64-bit width
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

static void suiteA_types(void)
{
    WebsFileInfo info;
    Webs         wp;

    tinfo("\n--- Suite A: types ---\n");
    ttrue(sizeof(info.size) == sizeof(int64), "WebsFileInfo.size must be 64 bits");
    ttrue(sizeof(wp.txLen) == sizeof(int64), "the response length must be 64 bits");
    ttrue(sizeof(wp.txRemaining) == sizeof(int64), "the remaining count must be 64 bits");
    info.size = -1;
    wp.txLen = -1;
    ttrue(info.size < 0 && wp.txLen < 0, "sizes must be signed so -1 can mean unknown");
}


static void suiteB_largeFile(void)
{
#if ME_UNIX_LIKE
    WebsFileInfo info;
    char         path[] = "file-size-width.tmp";
    Offset       size;
    int          fd;

    tinfo("\n--- Suite B: a file past 4 GB ---\n");
    size = INT64(4294967301);
    if ((fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0600)) < 0 || ftruncate(fd, (off_t) size) < 0) {
        tinfo("Cannot create a sparse file here, skipping");
        if (fd >= 0) {
            close(fd);
            unlink(path);
        }
        return;
    }
    close(fd);
    websFsOpen();
    ttrue(websStatFile(path, &info) == 0, "stat the sparse file");
    ttrue(info.size == size, "the size of a file past 4 GB must be whole");
    unlink(path);
#endif
}


int main(int argc, char **argv)
{
    suiteA_types();
    suiteB_largeFile();
    return 0;
}
