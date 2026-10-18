/*
    Each open of a ROM document has its own read position, so concurrent reads of one document are independent.
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

// Evaluate cursor-moving calls outside assertions because TestMe macros may evaluate arguments repeatedly.

static uchar page[] = "0123456789abcdefghij";

WebsRomIndex websRomIndex[] = {
    { "/dir", 0, 0 },
    { "/page.txt", page, 20 },
    { 0, 0, 0 }
};

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/fs.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

int main(int argc, char **argv)
{
    WebsFileInfo info;
    char         a[32], b[32];
    int          fa, fb, fc, n;

    websFsOpen();

    fa = websOpenFile("/page.txt", O_RDONLY, 0);
    fb = websOpenFile("/page.txt", O_RDONLY, 0);
    ttrue(fa >= 0 && fb >= 0, "both opens succeed");
    ttrue(fa != fb, "each open has its own descriptor");

    n = (int) websReadFile(fa, a, 4);
    teq(n, 4, "first reader reads 4 bytes");
    n = (int) websReadFile(fb, b, 6);
    teq(n, 6, "second reader reads 6 bytes");
    ttrue(memcmp(a, "0123", 4) == 0, "first reader starts at offset 0");
    ttrue(memcmp(b, "012345", 6) == 0, "second reader starts at offset 0, not where the first stopped");

    n = (int) websReadFile(fa, a, 32);
    teq(n, 16, "first reader gets the rest of the document");
    ttrue(memcmp(a, "456789abcdefghij", 16) == 0, "first reader continues from its own position");
    n = (int) websReadFile(fa, a, 32);
    teq(n, 0, "first reader is at end of file");

    n = (int) websReadFile(fb, b, 32);
    teq(n, 14, "second reader is not drained by the first");
    ttrue(memcmp(b, "6789abcdefghij", 14) == 0, "second reader continues from its own position");

    n = (int) websSeekFile(fa, 10, SEEK_SET);
    teq(n, 10, "seek one descriptor");
    n = (int) websReadFile(fa, a, 2);
    teq(n, 2, "read after seek");
    ttrue(memcmp(a, "ab", 2) == 0, "read follows the seek");
    n = (int) websReadFile(fb, b, 32);
    teq(n, 0, "seek does not move the other descriptor");

    n = (int) websSeekFile(fa, 5, SEEK_END);
    teq(n, 25, "seek past end");
    n = (int) websReadFile(fa, a, 32);
    teq(n, 0, "read past end returns nothing");

    n = websStatOpenFile(fa, &info);
    teq(n, 0, "stat an open document");
    teq((int) info.size, 20, "stat reports the document size");

    websCloseFile(fa);
    n = (int) websReadFile(fa, a, 4);
    teq(n, -1, "a closed descriptor is refused");
    n = (int) websSeekFile(fa, 0, SEEK_SET);
    teq(n, -1, "seek on a closed descriptor is refused");
    n = websStatOpenFile(fa, &info);
    teq(n, -1, "stat on a closed descriptor is refused");

    fc = websOpenFile("/page.txt", O_RDONLY, 0);
    teq(fc, fa, "a closed descriptor is reused");
    n = (int) websReadFile(fc, a, 4);
    teq(n, 4, "reused descriptor reads");
    ttrue(memcmp(a, "0123", 4) == 0, "reused descriptor starts at offset 0");

    fa = websOpenFile("/dir", O_RDONLY, 0);
    ttrue(fa >= 0, "a directory entry opens");
    n = websStatOpenFile(fa, &info);
    teq(n, -1, "an open directory is not a servable document");

    n = websOpenFile("/missing", O_RDONLY, 0);
    teq(n, -1, "a missing document does not open");

    websCloseFile(fa);
    websCloseFile(fb);
    websCloseFile(fc);
    teq(romFileMax, 0, "every descriptor is released");
    ttrue(romFiles == NULL, "the descriptor table is released");
    websFsClose();
    return 0;
}
