/*
    buf-limit-width.tst.c - A buffer's limit keeps its full width
 */

#include "testme.h"
#include "goahead.h"

int main(int argc, char **argv)
{
    WebsBuf buf;
    ssize   limit;

    if (sizeof(ssize) < 8) {
        tskip("64-bit ssize only");
        return 0;
    }
    limit = (ssize) 3 * 1024 * 1024 * 1024;
    teq(bufCreate(&buf, 0, limit), 0, "create a buffer with a 3 GiB limit");
    ttrue(buf.maxsize == limit, "a 3 GiB limit must be kept, not wrapped (got %lld)", (long long) buf.maxsize);
    bufFree(&buf);

    teq(bufCreate(&buf, 0, -1), 0, "create an unlimited buffer");
    ttrue(buf.maxsize == -1, "-1 must mean unlimited");
    bufFree(&buf);

    ttrue(bufCreate(&buf, 0, -2) < 0, "a negative limit other than -1 must be refused");
    ttrue(bufCreate(&buf, 0, (ssize) (int) limit) < 0, "a limit that wrapped through int must be refused");
    return 0;
}
