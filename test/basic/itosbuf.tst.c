/*
    itosbuf.tst.c - Regression test for itosbuf() and INT64_MIN.
 */

#include "testme.h"
#include "goahead.h"

static void itosbufExtremes(void)
{
    char buf[32];

    tinfo("\n--- itosbuf: INT64_MIN and neighbouring extremes ---\n");

    ttrue(itosbuf(buf, sizeof(buf), INT64(-9223372036854775807) - 1, 10) == buf,
          "A1 itosbuf(INT64_MIN, 10) succeeds");
    tmatch(buf, "-9223372036854775808");

    ttrue(itosbuf(buf, sizeof(buf), INT64(-9223372036854775807) - 1, 16) == buf,
          "A2 itosbuf(INT64_MIN, 16) succeeds");
    tmatch(buf, "-8000000000000000");

    ttrue(itosbuf(buf, sizeof(buf), MAXINT64, 10) == buf, "A3 itosbuf(INT64_MAX, 10) succeeds");
    tmatch(buf, "9223372036854775807");

    ttrue(itosbuf(buf, sizeof(buf), -1, 10) == buf, "A4 itosbuf(-1, 10) succeeds");
    tmatch(buf, "-1");

    ttrue(itosbuf(buf, sizeof(buf), 0, 10) == buf, "A5 itosbuf(0, 10) succeeds");
    tmatch(buf, "0");

    ttrue(itosbuf(buf, 4, INT64(-9223372036854775807) - 1, 10) == 0,
          "A6 itosbuf(INT64_MIN, 10) refuses an undersized buffer");
}

int main(int argc, char **argv)
{
    tinfo("itosbuf: regression - convert through an unsigned magnitude for INT64_MIN\n");

    itosbufExtremes();

    return 0;
}
