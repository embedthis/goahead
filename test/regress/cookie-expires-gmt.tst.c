/*
    cookie-expires-gmt.tst.c - A cookie's expires= date is in GMT
 */

#include "testme.h"
#include "goahead.h"

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

/*
    Use a fixed offset without needing a timezone database.
 */
static void setZone(void)
{
#if ME_WIN_LIKE
    _putenv("TZ=XYZ-10");
    _tzset();
#else
    setenv("TZ", "XYZ-10", 1);
    tzset();
#endif
}


int main(int argc, char **argv)
{
    Webs     *wp;
    WebsKey  *kp;
    WebsTime expected, when;
    cchar    *cookie, *expires;
    char     date[64], *end;
    ssize    len;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    setZone();

    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));
    wp->responseCookies = hashCreate(7);

    when = 0;
    expected = time(0) + 3600;
    websSetCookie(wp, "id", "1", "/", NULL, 3600, 0);

    kp = hashLookup(wp->responseCookies, "id");
    cookie = kp ? kp->content.value.string : 0;
    ttrue(cookie != 0, "the cookie must be set");
    expires = cookie ? strstr(cookie, "; expires=") : 0;
    ttrue(expires != 0, "a cookie with a lifespan must carry expires=");

    if (expires) {
        expires += 10;
        end = strchr(expires, ';');
        len = end ? (ssize) (end - expires) : slen(expires);
        ttrue(len < (ssize) sizeof(date), "the date must be of sensible length");
        if (len < (ssize) sizeof(date)) {
            memcpy(date, expires, (size_t) len);
            date[len] = '\0';
            ttrue(websParseHttpDate(&when, date) == 0, "expires= must be an HTTP date, got \"%s\"", date);
            ttrue(when >= expected - 2 && when <= expected + 2,
                  "expires= must be now + lifespan in GMT, off by %d seconds: \"%s\"", (int) (when - expected), date);
            ttrue(strstr(date, " GMT") != 0, "expires= must name GMT, got \"%s\"", date);
        }
    }
    hashFree(wp->responseCookies);
    wfree(wp);
    return 0;
}
