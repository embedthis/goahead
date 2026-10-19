/* The formatter reads %ll conversions as 64-bit long long */

#include "testme.h"
#include "goahead.h"

static void suiteA_wide(void)
{
    char buf[80];

    tinfo("\n--- Suite A: values past 32 bits ---\n");
    fmt(buf, sizeof(buf), "%lld", (long long) -3000000000LL);
    ttrue(smatch(buf, "-3000000000"), "%%lld must print a value past 32 bits whole, got %s", buf);

    fmt(buf, sizeof(buf), "%llu", (unsigned long long) 4294967301ULL);
    ttrue(smatch(buf, "4294967301"), "%%llu must print a value past 32 bits whole, got %s", buf);

    fmt(buf, sizeof(buf), "%llx", (long long) 0x123456789LL);
    ttrue(smatch(buf, "123456789"), "%%llx must print a value past 32 bits whole, got %s", buf);
}


static void suiteB_arguments(void)
{
    char buf[80];

    tinfo("\n--- Suite B: argument consumption ---\n");
    fmt(buf, sizeof(buf), "%x:%llx:%s", 0xabcU, (long long) 0x100000002LL, "end");
    ttrue(smatch(buf, "abc:100000002:end"), "%%llx must consume one 64-bit argument, got %s", buf);

    fmt(buf, sizeof(buf), "%ld", (long) -7);
    ttrue(smatch(buf, "-7"), "a single l must still read a long, got %s", buf);
}


/*
    %X and %x must read the same width, honoring the ll modifier on 32-bit builds.
 */
static void suiteC_upper(void)
{
    char buf[80];

    tinfo("\n--- Suite C: upper case hex ---\n");
    fmt(buf, sizeof(buf), "%02X:%llX:%s", 0x9fU, (long long) 0x100000002LL, "end");
    ttrue(smatch(buf, "9F:100000002:end"), "%%X must read an unsigned int and %%llX a long long, got %s", buf);
}


int main(int argc, char **argv)
{
    suiteA_wide();
    suiteB_arguments();
    suiteC_upper();
    return 0;
}
