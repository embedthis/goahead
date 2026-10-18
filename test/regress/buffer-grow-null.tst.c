/* Poison fresh storage so a missing terminator is deterministic. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
static void *poisonAlloc(ssize size)
{
    void *p = walloc(size);
    if (p) memset(p, 0xA5, (size_t) size);
    return p;
}
#define walloc poisonAlloc
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/runtime.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef walloc
int main(int argc, char **argv)
{
    WebsBuf buf;
    int rc, i;
    bufCreate(&buf, 16, -1);
    bufPutStr(&buf, "partial request");
    bufAddNull(&buf);
    for (i = 0; i < 3; i++) {
        rc = bufGrow(&buf, 32);
        teq(rc, 1, "buffer grows");
        teq(*buf.endp, 0, "growth preserves terminator");
        tmatch((char*) buf.servp, "partial request", "growth preserves content");
    }
    bufFree(&buf);
    return 0;
}
