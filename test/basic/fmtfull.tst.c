/*
    fmtfull.tst.c - Regression tests for BPUT evaluating its character exactly once.
 */

#include "testme.h"
#include "goahead.h"

/********************************* Suite A: signed prefix ***********************/

static void suiteA_signedPrefix(void)
{
    char buf[16];
    ssize i;

    tinfo("\n--- Suite A: full buffer with a signed prefix does not hang ---\n");

    /*
        Fill the buffer during padding, before the sign.
     */
    fmt(buf, sizeof(buf), "%*d", 100, -1);
    ttrue(buf[sizeof(buf) - 1] == '\0', "A1 result is null terminated");
    for (i = 0; i < (ssize) sizeof(buf) - 1; i++) {
        ttrue(buf[i] == ' ', "A2 byte %zd is a pad space, got '%c'", i, buf[i]);
    }

    /* A leading-space flag also routes through the prefix loop */
    fmt(buf, sizeof(buf), "% *d", 100, 5);
    ttrue(buf[sizeof(buf) - 1] == '\0', "A3 result is null terminated");
}

/********************************* Suite B: alternate prefix ********************/

static void suiteB_alternatePrefix(void)
{
    char buf[16];

    tinfo("\n--- Suite B: full buffer with a '0x' alternate prefix does not hang ---\n");

    /* Fill the buffer before the 0x prefix. */
    fmt(buf, sizeof(buf), "%#*x", 100, 1);
    ttrue(buf[sizeof(buf) - 1] == '\0', "B1 result is null terminated");

    fmt(buf, sizeof(buf), "%#*X", 100, 1);
    ttrue(buf[sizeof(buf) - 1] == '\0', "B2 result is null terminated");
}

/********************************* Suite C: %c consumes its vararg **************/

static void suiteC_charConsumesVararg(void)
{
    char buf[4];
    char *result;

    tinfo("\n--- Suite C: %%c still consumes its vararg once the buffer is full ---\n");

    /*
        Fill the fixed buffer before %c and %d to check argument consumption.
     */
    fmt(buf, sizeof(buf), "%s%c%d", "abcdef", 'x', 7);
    ttrue(strcmp(buf, "abc") == 0, "C1 truncated result got=[%s]", buf);

    /*
        Check the complete output separately from its truncated prefix.
     */
    result = sfmt("%s%c%d", "abcdef", 'x', 7);
    ttrue(result && strcmp(result, "abcdefx7") == 0,
          "C2 untruncated result got=[%s]", result ? result : "(NULL)");
    ttrue(result && slen(result) == 8, "C3 untruncated length is 8, got=%zd",
          result ? slen(result) : (ssize) -1);
    wfree(result);
}

/********************************* Main ******************************************/

int main(int argc, char **argv)
{
    tinfo("fmtfull: regression - BPUT evaluates its character exactly once\n");

    suiteA_signedPrefix();
    suiteB_alternatePrefix();
    suiteC_charConsumesVararg();

    return 0;
}
