/*
    date-token-assert.tst.c - The tolerant date parser must not assert on input it handles

    websParseDateTime() is documented as a tolerant parser. Its tokenizer getNumOrSym() asserts that a
    token remains and then tests the same condition explicitly, so a date whose separators run out early
    logs an assertion in a debug build and aborts the process under ME_GOAHEAD_ASSERT_ABORT. Assertions
    guard programmer invariants, not input; the explicit test is the correct handling.
 */

#include "testme.h"
#include "goahead.h"

#if ME_GOAHEAD_LOGGING
static int assertions;

static void capture(int level, cchar *msg)
{
    if (strstr(msg, "Assertion") != NULL) {
        assertions++;
    }
}
#endif


int main(int argc, char **argv)
{
#if ME_GOAHEAD_LOGGING
    static cchar *inputs[] = {
        "12/25-2026 10:30",
        "Mon, 31 Feb /9999 23:59p60 GMT",
        "200905-21T16:06:J5.000Z",
        NULL
    };
    WebsTime when;
    int      i;

    websTimeOpen();
    logSetHandler(capture);

    for (i = 0; inputs[i]; i++) {
        assertions = 0;
        websParseDateTime(&when, inputs[i], NULL);
        ttrue(assertions == 0, "a malformed date must be rejected or tolerated, not asserted");
    }
    websTimeClose();
#else
    tskip("assertions are reported through logging, which is not compiled in");
#endif
    return 0;
}
