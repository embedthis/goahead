/*
    buf-grow-limit.tst.c - A buffer never allocates more than its limit, however growth is requested
 */
#include "testme.h"
#include "goahead.h"

int main(int argc, char **argv)
{
    WebsBuf buf;
    char    block[4096];
    ssize   put, total;
    int     i;

    memset(block, 'a', sizeof(block));

    teq(bufCreate(&buf, 1025, 16385), 0, "create a buffer with a 16 KB limit");
    total = 0;
    for (i = 0; i < 100; i++) {
        if ((put = bufPutBlk(&buf, block, sizeof(block))) < (ssize) sizeof(block)) {
            total += put;
            break;
        }
        total += put;
    }
    ttrue(buf.buflen <= 16385, "implicit growth stays within the limit, got %lld", (long long) buf.buflen);
    ttrue(total >= 16000 && total < 16385, "the buffer fills up to its limit, got %lld", (long long) total);
    bufFree(&buf);

    teq(bufCreate(&buf, 1025, 1025), 0, "create a buffer already at its limit");
    ttrue(!bufGrow(&buf, 1 << 20), "explicit growth of a full-size buffer is refused");
    ttrue(buf.buflen <= 2048, "the allocation is unchanged, got %lld", (long long) buf.buflen);
    bufFree(&buf);

    teq(bufCreate(&buf, 1025, 100000), 0, "create a buffer with room to grow");
    //  A caller relies on the room it asked for, so a request past the limit is refused, not cut short
    ttrue(!bufGrow(&buf, 500000), "explicit growth past the limit is refused");
    ttrue(bufGrow(&buf, 50000), "explicit growth within the limit succeeds");
    ttrue(bufRoom(&buf) >= 50000, "with the room requested");
    ttrue(buf.buflen <= 100000, "explicit growth stays within the limit, got %lld", (long long) buf.buflen);
    bufFree(&buf);

    teq(bufCreate(&buf, 1025, -1), 0, "create an unlimited buffer");
    ttrue(bufGrow(&buf, 300000), "an unlimited buffer grows on request");
    ttrue(buf.buflen >= 300000, "by at least the requested room");
    bufFree(&buf);
    return 0;
}
