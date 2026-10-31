/*
    redirect-listener-port.tst.c - A same-scheme redirect names the port the request arrived on
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

static Webs *makeRequest(int port, int flags)
{
    Webs *wp;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone("GET");
    wp->url = sclone("/dir");
    wp->path = sclone("/dir");
    wp->protoVersion = sclone("HTTP/1.1");
    wp->flags |= flags;
    wp->port = port;
    scopy(wp->ipaddr, sizeof(wp->ipaddr), "192.0.2.77");
    scopy(wp->ifaddr, sizeof(wp->ifaddr), "198.51.100.5");
    // Let websDone complete without a socket
    wp->connError = 1;
    return wp;
}

static void check(int port, int flags, cchar *target, cchar *expected, cchar *why)
{
    Webs  *wp;
    cchar *location;

    wp = makeRequest(port, flags);
    websRedirect(wp, target);
    bufAddNull(&wp->output);
    location = strstr(wp->output.servp, "Location: ");
    location = location ? location + 10 : "";
    ttrue(sstarts(location, expected), "%s: expected Location %s, got %.60s", why, expected, location);
    websFree(wp);
}

int main(int argc, char **argv)
{
    /*
        Two http listeners: the first one registered is the default, the request arrives on the second.
     */
    defaultHttpPort = 8080;
    defaultSslPort = 4443;

    check(8081, 0, "/dir/index.html", "http://198.51.100.5:8081/dir/index.html\r\n",
          "a relative redirect must name the port the request arrived on");
    check(8081, 0, "http:///dir/index.html", "http://198.51.100.5:8081/dir/index.html\r\n",
          "the http shorthand must name the port the request arrived on");
    check(8081, 0, "https://", "https://198.51.100.5:4443/dir\r\n",
          "a scheme switch must name the first listener of the new scheme");
    check(0, 0, "/dir/index.html", "http://198.51.100.5:8080/dir/index.html\r\n",
          "an unknown arrival port must fall back to the scheme's first listener");
    check(8081, WEBS_PROXIED, "/dir/index.html", "http://198.51.100.5/dir/index.html\r\n",
          "behind a trusted proxy the port is implied, not the listener's");
    check(8081, WEBS_PROXIED, "https://", "https://198.51.100.5/dir\r\n",
          "behind a trusted proxy a scheme switch implies the new scheme's default port");
    check(4444, WEBS_PROXIED | WEBS_HTTPS, "/dir/index.html", "https://198.51.100.5/dir/index.html\r\n",
          "behind a trusted proxy that forwarded https the port is implied");

    websSetHostUrl("www.example.com:8443");
    check(8081, 0, "/dir/index.html", "http://www.example.com:8443/dir/index.html\r\n",
          "the configured host URL must still win over the arrival port");
    return 0;
}
