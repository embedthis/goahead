/*
    buf-put.tst.c -- Formatted buffer writes preserve bytes and release temporary storage.
 */

#include "testme.h"
#include "goahead.h"

#if MACOSX
    #include <malloc/malloc.h>
#elif defined(__GLIBC__)
    #include <malloc.h>
#endif

#define ITERATIONS 10000
#define ALLOWANCE (64 * 1024)

static ssize allocatedBytes(void)
{
#if !ME_GOAHEAD_REPLACE_MALLOC && MACOSX
    malloc_statistics_t stats;

    malloc_zone_statistics(NULL, &stats);
    return (ssize) stats.size_in_use;
#elif !ME_GOAHEAD_REPLACE_MALLOC && defined(__GLIBC__)
    #if __GLIBC_PREREQ(2, 33)
    struct mallinfo2 stats;

    stats = mallinfo2();
    return (ssize) stats.uordblks;
    #else
    return -1;
    #endif
#else
    return -1;
#endif
}


static bool writeBuffer(int mode)
{
    WebsBuf bp;
    ssize count, expected;
    bool valid;

    bufCreate(&bp, 16, mode == 2 ? 16 : -1);
    if (mode == 0) {
        count = bufPut(&bp, "%s:%d", "cipher", 42);
        valid = count == 9 && strcmp(bufStart(&bp), "cipher:42") == 0;
    } else if (mode == 1) {
        count = bufPut(&bp, "%s", "");
        valid = count == 0 && bufLen(&bp) == 0;
    } else {
        expected = mode == 2 ? bp.buflen - 1 : 4096;
        count = bufPut(&bp, "%04096d", 7);
        valid = count == expected && bufLen(&bp) == expected && bp.buf[0] == '0';
        if (mode == 2) {
            count = bufPut(&bp, "%s", "full");
            valid = valid && count == 0;
        } else {
            valid = valid && bp.buf[4095] == '7' && bp.buf[4096] == '\0';
        }
    }
    bufFree(&bp);
    return valid;
}

static void checkWrites(cchar *name, int mode)
{
    ssize before, after;
    int i;
    bool valid = 1;

    for (i = 0; i < 100; i++) {
        valid = writeBuffer(mode) && valid;
    }
    ttrue(valid, "%s: formatted bytes and return counts are preserved", name);
    if (allocatedBytes() < 0 || smatch(tget("ME_SANITIZE", ""), "1") ||
        getenv("ASAN_OPTIONS") || getenv("UBSAN_OPTIONS")) {
        tskip("Live allocator statistics unavailable; output checks still run");
        return;
    }
    before = allocatedBytes();
    for (i = 0; i < ITERATIONS; i++) {
        valid = writeBuffer(mode) && valid;
    }
    after = allocatedBytes();
    ttrue(valid, "%s: repeated writes preserve output", name);
    tinfo("%s: live allocation growth %lld bytes\n", name, (long long) (after - before));
    ttrue(after - before < ALLOWANCE, "%s: live allocations grew %lld bytes over %d calls", name,
        (long long) (after - before), ITERATIONS);
}

int main(int argc, char **argv)
{
    checkWrites("formatted append", 0);
    checkWrites("empty output", 1);
    checkWrites("partial and full buffer", 2);
    checkWrites("formatter and buffer growth", 3);
    return 0;
}
