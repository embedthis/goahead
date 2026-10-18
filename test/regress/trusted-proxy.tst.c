/* A trusted proxy's X-Forwarded-Proto sets the client's scheme, and no one else's does */

#define ME_GOAHEAD_HSTS "max-age=31536000"

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
static int testListenPort(int sid) { return 80; }
#define socketGetPort testListenPort
#include "../../src/http.c"
#undef socketGetPort
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

/*
    Parse a request from a peer, with an optional X-Forwarded-Proto
 */
static Webs *parse(cchar *peer, cchar *proto)
{
    Webs *wp;
    char *request;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->listenSid = -1;
    scopy(wp->ipaddr, sizeof(wp->ipaddr), peer);
    request = sfmt("GET /index.html HTTP/1.1\r\nHost: dev.example\r\n%s%s%s\r\n", proto ? "X-Forwarded-Proto: " : "",
        proto ? proto : "", proto ? "\r\n" : "");
    bufPutStr(&wp->rxbuf, request);
    wfree(request);
    parseFirstLine(wp);
    parseHeaders(wp);
    return wp;
}


static bool sendsHsts(Webs *wp)
{
    wp->code = 200;
    websWriteHeaders(wp, 0, NULL);
    bufAddNull(&wp->output);
    return strstr(bufStart(&wp->output), "Strict-Transport-Security: max-age=31536000\r\n") != 0;
}


static bool secureCookie(Webs *wp)
{
    WebsKey *cookie;

    if (websGetSession(wp, 1) == 0 || (cookie = hashLookup(wp->responseCookies, WEBS_SESSION)) == 0) {
        return 0;
    }
    return strstr(cookie->content.value.string, "; secure") != 0;
}


static void expectScheme(cchar *peer, cchar *proto, bool https, cchar *why)
{
    Webs *wp;

    wp = parse(peer, proto);
    ttrue(((wp->flags & WEBS_HTTPS) != 0) == https, "%s: WEBS_HTTPS must be %d", why, https);
    tmatch(wp->protocol, https ? "https" : "http", "%s: the protocol must follow the scheme", why);
    ttrue(secureCookie(wp) == https, "%s: the session cookie secure attribute must be %d", why, https);
    ttrue(sendsHsts(wp) == https, "%s: Strict-Transport-Security must be sent only over https", why);
    ttrue((wp->flags & WEBS_SECURE) == 0, "%s: the connection is not TLS", why);
    websFree(wp);
}


int main(int argc, char **argv)
{
#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    if (websOpen("../web", NULL) < 0) {
        tfail("cannot open the server");
        return 1;
    }
    expectScheme("127.0.0.1", "https", 0, "no trusted proxy configured");

    websSetTrustedProxies("127.0.0.1, [::1]");
    ttrue(websIsTrustedProxy("127.0.0.1"), "a listed address is trusted");
    ttrue(websIsTrustedProxy("::1"), "a bracketed IPv6 address is trusted");
    ttrue(websIsTrustedProxy("::ffff:127.0.0.1"), "an IPv4-mapped peer is trusted as its IPv4 address");
    ttrue(!websIsTrustedProxy("127.0.0.10"), "an address that only begins with a listed one is not trusted");
    ttrue(!websIsTrustedProxy("10.0.0.1"), "an unlisted address is not trusted");

    expectScheme("127.0.0.1", "https", 1, "a trusted proxy forwarding https");
    expectScheme("127.0.0.1", "http, https", 1, "the right-most value is the nearest proxy's");
    expectScheme("127.0.0.1", "https, http", 0, "a trusted proxy forwarding http");
    expectScheme("127.0.0.1", "gopher", 0, "an unknown scheme is ignored");
    expectScheme("127.0.0.1", 0, 0, "a trusted proxy without the header");
    expectScheme("10.0.0.1", "https", 0, "an untrusted peer");

    websSetTrustedProxies(0);
    ttrue(!websIsTrustedProxy("127.0.0.1"), "an empty list trusts no one");
    websClose();
    return 0;
}
