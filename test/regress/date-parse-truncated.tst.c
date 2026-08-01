/*
    date-parse-truncated.tst.c - Regression: a truncated date must not branch on an indeterminate value.

    websParseDateTime() declares its alpha/alpha2/alpha3 flags uninitialized and fills them only
    through getNumOrSym()'s out-parameter. That function had an early return - taken when the date
    string runs out of tokens - that left the out-parameter unwritten, so the dd/mm/yy versus
    yy/mm/dd branch read whatever was on the stack. Request date headers (If-Modified-Since) reach
    this parser unauthenticated.

    The defect is a read of an indeterminate value, which no assertion can observe directly. What is
    assertable is the property that made it a defect: the result must be a function of the input
    alone. Each truncated date is parsed repeatedly with the stack deliberately dirtied between
    calls; every parse of the same input must return the same verdict and the same time.
 */

#include "testme.h"
#include "goahead.h"

/*
    Dirty the stack region websParseDateTime will use, so an indeterminate read has a different
    value on the next call. Written through a volatile pointer so the compiler cannot elide it.
 */
static void dirtyStack(int pattern)
{
    volatile int scratch[256];
    int          i;

    for (i = 0; i < 256; i++) {
        scratch[i] = pattern;
    }
}

/*
    Parse the same string twice with opposing stack fill patterns. Returns 1 when both parses agree
    on both the return code and the parsed time.
 */
static int parseIsStable(cchar *date)
{
    WebsTime first, second;
    int      rc1, rc2;

    first = second = 0;
    dirtyStack(0);
    rc1 = websParseDateTime(&first, date, NULL);
    dirtyStack(-1);
    rc2 = websParseDateTime(&second, date, NULL);

    return rc1 == rc2 && first == second;
}

int main(int argc, char **argv)
{
    WebsTime when;
    cchar    *truncated[] = {
        "Mon, 01/",
        "01/",
        "01/02/",
        "Mon, 01/02/",
        "Jan/",
        "Mon, ",
        "01/02/03 10:",
        "",
        NULL
    };
    int i;

    tinfo("date-parse-truncated: a truncated date must parse deterministically\n");

    for (i = 0; truncated[i]; i++) {
        ttrue(parseIsStable(truncated[i]), "truncated date [%s] must parse deterministically",
              truncated[i]);
    }

    /*
        Control: a well-formed date is unaffected by the fix and still parses.
     */
    when = 0;
    ttrue(websParseDateTime(&when, "Mon, 12 Jan 2026 10:20:30 GMT", NULL) == 0,
          "a well-formed date must still parse");
    ttrue(when > 0, "a well-formed date must yield a non-zero time");
    ttrue(parseIsStable("Mon, 12 Jan 2026 10:20:30 GMT"), "a well-formed date must be deterministic");

    return 0;
}
