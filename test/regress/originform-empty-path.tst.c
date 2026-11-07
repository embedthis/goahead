/*
    originform-empty-path.tst - An absolute-form target with an empty path is read as "/" by every consumer

    "http://host?query" is a legal absolute-form target whose path is empty. websUrlParse routes it as the path "/"
    with the query, so routing, authentication and the file open read "/". The consumers that reconstruct the
    origin form from the stored target (REQUEST_URI, the https upgrade redirect and the Digest uri binding) must read
    the same request: "/?query", never a path taken from inside the query and never a form that drops the query.
 */

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

/*
    Parse a request line through the real parser and return the request
 */
static Webs *request(cchar *target)
{
    Webs *wp;
    char *line;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    scopy(wp->ipaddr, sizeof(wp->ipaddr), "192.0.2.77");
    scopy(wp->ifaddr, sizeof(wp->ifaddr), "198.51.100.5");
    // Let websDone complete without a socket
    wp->connError = 1;
    line = sfmt("GET %s HTTP/1.1\r\n", target);
    bufPutStr(&wp->rxbuf, line);
    wfree(line);
    parseFirstLine(wp);
    return wp;
}

static void check(cchar *target, cchar *path, cchar *query, cchar *originForm)
{
    Webs  *wp;
    cchar *location, *requestUri;
    char  *expected;

    wp = request(target);
    ttrue(!wp->finalized, "%s: the request must parse, got status %d", target, wp->code);
    tmatch(wp->path, path, "the routed path");
    tmatch(wp->query ? wp->query : "", query, "the query");

    /* REQUEST_URI is the target in origin form */
    websSetEnv(wp);
    requestUri = websGetVar(wp, "REQUEST_URI", "");
    ttrue(smatch(requestUri, originForm), "%s: REQUEST_URI must be %s, got %s", target, originForm, requestUri);

    /* The https upgrade redirect keeps the request's own path and query */
    websRedirect(wp, "https://");
    bufAddNull(&wp->output);
    location = strstr(wp->output.servp, "Location: ");
    location = location ? location + 10 : "";
    expected = sfmt("https://198.51.100.5%s\r\n", originForm);
    ttrue(sstarts(location, expected), "%s: expected Location %s, got %.80s", target, expected, location);
    wfree(expected);
    websFree(wp);
}

int main(int argc, char **argv)
{
    defaultHttpPort = 80;
    defaultSslPort = 443;

    /* An empty path with a query: the query must not supply the path, and must not be dropped */
    check("http://x?/admin/login.html", "/", "/admin/login.html", "/?/admin/login.html");
    check("http://x:80?q=1", "/", "q=1", "/?q=1");
    check("http://x?", "/", "", "/?");

    /* An empty path without a query */
    check("http://x", "/", "", "/");

    /* Paths are unchanged */
    check("http://x/p?q", "/p", "q", "/p?q");
    check("http://x/", "/", "", "/");
    check("/p?q", "/p", "q", "/p?q");
    check("/?/admin/login.html", "/", "/admin/login.html", "/?/admin/login.html");
    return 0;
}
