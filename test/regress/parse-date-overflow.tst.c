/* Date parsing bounds every field and computes in 64 bits, so no input overflows the arithmetic. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

/*
    Parse a date and return the result. A WebsTime wider than 32 bits is required for the
    post-2038 cases; a 32-bit target saturates instead, which is checked separately.
 */
static WebsTime parse(cchar *date)
{
    WebsTime when;

    when = 0;
    websParseDateTime(&when, date, NULL);
    return when;
}

int main(int argc, char **argv)
{
    WebsTime base, prev, when;
    int      year;

    websTimeOpen();

    /*
        Well-formed dates parse to the correct instant. The 2038 and 2050 cases overflowed the
        int arithmetic in makeTime before the fix.
     */
    teqll((int64) parse("Mon, 01 Jan 2024 00:00:00 GMT"), 1704067200, "date before 2038");
    teqll((int64) parse("Thu, 01 Jan 1970 00:00:00 GMT"), 0, "epoch");
    if (sizeof(WebsTime) > 4) {
        teqll((int64) parse("Tue, 19 Jan 2038 03:14:08 GMT"), 2147483648LL, "one second past INT_MAX");
        teqll((int64) parse("Sat, 01 Jan 2050 00:00:00 GMT"), 2524608000LL, "date well past 2038");
        teqll((int64) parse("Fri, 01 Jan 2100 00:00:00 GMT"), 4102444800LL, "date past 2100");
    }

    /*
        An out-of-range time component is discarded and the field falls back to its default,
        leaving the rest of the date intact. Before the fix each of these overflowed makeTime
        because validateTime cleared tm_sec rather than the field it had tested.
     */
    base = parse("Mon, 01 Jan 2024 00:00:00 GMT");
    teqll((int64) parse("Mon, 01 Jan 2024 99999999:00:00 GMT"), (int64) base, "oversized hour discarded");
    teqll((int64) parse("Mon, 01 Jan 2024 00:99999999:00 GMT"), (int64) base, "oversized minute discarded");
    teqll((int64) parse("Mon, 01 Jan 2024 00:00:99999999 GMT"), (int64) base, "oversized second discarded");

    /*
        An unbounded year reaches daysSinceEpoch through the separator date form. It is treated
        as unset rather than multiplied out.
     */
    when = parse("99999999/01/01");
    ttrue(when >= 0, "unbounded year does not produce a negative time");
    ttrue(when < 253402300800LL, "unbounded year does not produce a far-future time");

    /*
        The zone offset is scaled by 60 twice, so it is bounded before use.
     */
    teqll((int64) parse("Mon, 01 Jan 2024 00:00:00 GMT+99999999:00"), (int64) base, "oversized zone hours ignored");
    teqll((int64) parse("Mon, 01 Jan 2024 00:00:00 GMT+9999999999"), (int64) base, "oversized zone digits ignored");

    /*
        A legitimate zone offset still applies.
     */
    teqll((int64) parse("Mon, 01 Jan 2024 00:00:00 GMT+01:00"), (int64) base - 3600, "positive zone offset");
    teqll((int64) parse("Mon, 01 Jan 2024 00:00:00 GMT-01:00"), (int64) base + 3600, "negative zone offset");

    /*
        Parsed time must increase with the requested year. It did not before the fix: the wrap
        made 2170 compare greater than 2180, so conditional GET answered 304 and 200 by turns.
     */
    if (sizeof(WebsTime) > 4) {
        prev = 0;
        for (year = 2020; year <= 2220; year += 5) {
            char date[64];
            fmt(date, sizeof(date), "Mon, 01 Jan %d 00:00:00 GMT", year);
            when = parse(date);
            ttrue(when > prev, "parsed time increases with year");
            prev = when;
        }
    }

    websTimeClose();
    return 0;
}
