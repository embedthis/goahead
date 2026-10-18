/*
    http-date-strict.tst - websParseHttpDate accepts only the three HTTP-date formats
 */
#include    "testme.h"
#include    "goahead.h"

/*
    Use RFC 9110's example instant, epoch 784111777, in each supported format.
 */
static cchar *valid[] = {
    "Sun, 06 Nov 1994 08:49:37 GMT",            /* IMF-fixdate */
    "Sunday, 06-Nov-94 08:49:37 GMT",           /* obsolete RFC 850 */
    "Sun Nov  6 08:49:37 1994",                 /* obsolete asctime */
    0
};

static cchar *invalid[] = {
    "2147483647",                               /* Bare epoch values are not HTTP dates. */
    "0",
    "",
    "garbage",
    "Sun, 06 Nov 1994 08:49:37",                /* no zone */
    "Sun, 06 Nov 1994 08:49:37 EST",            /* HTTP dates are GMT */
    "Sun, 06 Nov 1994 08:49 GMT",               /* no seconds */
    "Sun, 6 Nov 1994 08:49:37 GMT",             /* day of month is two digits */
    "Sun, 06 Nov 94 08:49:37 GMT",              /* IMF-fixdate year is four digits */
    "Sun, 06 Nov 1994 08:49:37 GMT extra",
    "Sun, 06 Xxx 1994 08:49:37 GMT",            /* not a month */
    "Sun, 06 Nov 1994 25:49:37 GMT",            /* hour out of range */
    "Sun, 06 Nov 1994 08:49:37 GM",
    "2009-05-21T16:06:05.000Z",                 /* ISO 8601 is not an HTTP-date */
    "Thu, 01 Jan 1900 00:00:00 GMT",            /* before the epoch */
    0
};

int main(int argc, char **argv)
{
    WebsTime when, untouched;
    int      i;

    for (i = 0; valid[i]; i++) {
        when = 0;
        teqi(websParseHttpDate(&when, valid[i]), 0, "accept wire format %s", valid[i]);
        teqll((int64) when, (int64) 784111777, "parse %s to the right instant", valid[i]);
    }

    untouched = 12345;
    for (i = 0; invalid[i]; i++) {
        when = untouched;
        teqi(websParseHttpDate(&when, invalid[i]), -1, "reject \"%s\"", invalid[i]);
        teqll((int64) when, (int64) untouched, "leave the time untouched for \"%s\"", invalid[i]);
    }
    when = untouched;
    teqi(websParseHttpDate(&when, NULL), -1, "reject a null date");
    teqll((int64) when, (int64) untouched, "leave the time untouched for a null date");

    /*
        Check RFC 850's two-digit year boundary at 70.
     */
    teqi(websParseHttpDate(&when, "Thursday, 01-Jan-70 00:00:00 GMT"), 0, "accept the RFC 850 epoch");
    teqll((int64) when, (int64) 0, "RFC 850 year 70 is 1970");
    teqi(websParseHttpDate(&when, "Friday, 01-Jan-38 00:00:00 GMT"), 0, "accept an RFC 850 date past 2000");
    teqll((int64) when, (int64) 2145916800, "RFC 850 year 38 is 2038");
    return 0;
}
