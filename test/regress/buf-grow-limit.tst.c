/*
    bufGrow must honor maxsize for both explicit and incremental growth.
 */

#include "testme.h"
#include "goahead.h"

int main(int argc, char **argv)
{
    WebsBuf buf;
    int     i;

    teq(bufCreate(&buf, 64, 256), 0, "create a buffer with a 256 byte limit");
    ttrue(bufGrow(&buf, 32), "growth within the limit is allowed");
    for (i = 0; i < 64 && bufGrow(&buf, 32); i++) { }
    ttrue(buf.buflen <= buf.maxsize, "explicit growth must stop at the limit, buflen %lld limit %lld",
          (long long) buf.buflen, (long long) buf.maxsize);
    ttrue(!bufGrow(&buf, 1), "growth past the limit must be refused");
    bufFree(&buf);

    teq(bufCreate(&buf, 16, -1), 0, "create an unlimited buffer");
    ttrue(bufGrow(&buf, 4096), "an unlimited buffer grows on request");
    bufFree(&buf);
    return 0;
}
