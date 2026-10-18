/*
    redirect-server-host.tst.c - A redirect names this server, never the client's address
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

static Webs *makeRequest(cchar *client, cchar *server)
{
    Webs *wp;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone("GET");
    wp->url = sclone("/dir");
    wp->path = sclone("/dir");
    wp->protoVersion = sclone("HTTP/1.1");
    scopy(wp->ipaddr, sizeof(wp->ipaddr), client);
    scopy(wp->ifaddr, sizeof(wp->ifaddr), server);
    // Let websDone complete without a socket
    wp->connError = 1;
    return wp;
}

static void check(cchar *client, cchar *server, cchar *target, cchar *expected, cchar *why)
{
    Webs  *wp;
    cchar *location;

    wp = makeRequest(client, server);
    websRedirect(wp, target);
    bufAddNull(&wp->output);
    location = strstr(wp->output.servp, "Location: ");
    location = location ? location + 10 : "";
    ttrue(sstarts(location, expected), "%s: expected Location %s, got %.60s", why, expected, location);
    ttrue(strstr(wp->output.servp, client) == 0, "%s: the client address must not appear in the response", why);
    websFree(wp);
}


int main(int argc, char **argv)
{
    defaultHttpPort = 80;
    defaultSslPort = 443;

    check("192.0.2.77", "198.51.100.5", "/dir/index.html", "http://198.51.100.5/dir/index.html\r\n",
          "a relative redirect must name the accepting interface");
    check("192.0.2.77", "198.51.100.5", "https://", "https://198.51.100.5/dir\r\n",
          "an https upgrade must name the accepting interface");
    check("2001:db8::77", "2001:db8::5", "/dir/index.html", "http://[2001:db8::5]/dir/index.html\r\n",
          "an IPv6 interface must be bracketed");

    websSetHostUrl("www.example.com:8443");
    check("192.0.2.77", "198.51.100.5", "/dir/index.html", "http://www.example.com:8443/dir/index.html\r\n",
          "the configured host URL must win over the interface address");
    return 0;
}
