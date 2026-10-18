/*
    fmt-size.tst.c - The formatter prints ssize and size_t values whole with %z
 */

#include "testme.h"
#include "goahead.h"

static void suiteA_wide(void)
{
    char buf[80];

    tinfo("\n--- Suite A: values past 32 bits ---\n");
    if (sizeof(ssize) < 8) {
        tinfo("ssize is 32 bits on this platform; nothing wider to print");
        return;
    }
    fmt(buf, sizeof(buf), "%zd", (ssize) 3000000000LL);
    ttrue(smatch(buf, "3000000000"), "%%zd must print a 3 GB size whole, got %s", buf);

    fmt(buf, sizeof(buf), "%zu", (size_t) 4294967301ULL);
    ttrue(smatch(buf, "4294967301"), "%%zu must print a size past 4 GB whole, got %s", buf);

    fmt(buf, sizeof(buf), "%zx", (ssize) 0x100000001LL);
    ttrue(smatch(buf, "100000001"), "%%zx must print a chunk length past 4 GB whole, got %s", buf);
}


static void suiteB_arguments(void)
{
    char buf[80];

    tinfo("\n--- Suite B: argument consumption ---\n");
    fmt(buf, sizeof(buf), "%zd %s %zd", (ssize) 7, "and", (ssize) 9);
    ttrue(smatch(buf, "7 and 9"), "%%z must consume one argument each, got %s", buf);

    fmt(buf, sizeof(buf), "%zd", (ssize) -5);
    ttrue(smatch(buf, "-5"), "a negative size must print signed, got %s", buf);

    fmt(buf, sizeof(buf), "[%5zd]", (ssize) 42);
    ttrue(smatch(buf, "[   42]"), "width must apply to %%zd, got %s", buf);
}


static void suiteC_neighbours(void)
{
    char buf[80];

    tinfo("\n--- Suite C: other length modifiers ---\n");
    fmt(buf, sizeof(buf), "%d %ld %Ld %hd", 1, (long) 2, (int64) 3, (short) 4);
    ttrue(smatch(buf, "1 2 3 4"), "the other modifiers are unchanged, got %s", buf);
}


int main(int argc, char **argv)
{
    suiteA_wide();
    suiteB_arguments();
    suiteC_neighbours();
    return 0;
}
