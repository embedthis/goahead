/* The https upgrade redirect uses the origin form of the request target */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/http.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static void check(cchar *url, cchar *expected, cchar *why)
{
    Webs  *wp;
    cchar *location;
    int   wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone("GET");
    wp->url = sclone(url);
    wp->path = sclone("/secure/index.html");
    wp->protoVersion = sclone("HTTP/1.1");
    scopy(wp->ipaddr, sizeof(wp->ipaddr), "192.0.2.77");
    scopy(wp->ifaddr, sizeof(wp->ifaddr), "198.51.100.5");
    // Let websDone complete without a socket
    wp->connError = 1;

    websRedirect(wp, "https://");
    bufAddNull(&wp->output);
    location = strstr(wp->output.servp, "Location: ");
    location = location ? location + 10 : "";
    ttrue(sstarts(location, expected), "%s: expected Location %s, got %.80s", why, expected, location);
    ttrue(strstr(wp->output.servp, "evil.example") == 0, "%s: the target's authority must not appear", why);
    websFree(wp);
}


int main(int argc, char **argv)
{
    defaultHttpPort = 80;
    defaultSslPort = 443;

    check("http://evil.example:80/secure/index.html?x", "https://198.51.100.5/secure/index.html?x\r\n",
          "an absolute-form target must redirect to its path and query");
    check("/secure/index.html?x", "https://198.51.100.5/secure/index.html?x\r\n",
          "an origin-form target is unchanged");
    return 0;
}
