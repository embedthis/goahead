/* A client's Connection header cannot strip the trusted proxy's X-Forwarded-Proto before the scheme is read */

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
//  The request has no listening socket, so answer the port query the header parser makes
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
    Parse a request from a trusted proxy that forwards https and carries the given extra header line.
 */
static Webs *parse(cchar *extra)
{
    Webs *wp;
    char *request;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->listenSid = -1;
    scopy(wp->ipaddr, sizeof(wp->ipaddr), "127.0.0.1");
    request = sfmt("GET /index.html HTTP/1.1\r\nHost: dev.example\r\nX-Forwarded-Proto: https\r\n%s\r\n", extra);
    bufPutStr(&wp->rxbuf, request);
    wfree(request);
    parseFirstLine(wp);
    parseHeaders(wp);
    return wp;
}


int main(int argc, char **argv)
{
    Webs *wp;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    if (websOpen("../web", NULL) < 0) {
        tfail("cannot open the server");
        return 1;
    }
    websSetTrustedProxies("127.0.0.1");

    wp = parse("");
    ttrue((wp->flags & WEBS_HTTPS) != 0, "the trusted proxy's https is honoured");
    websFree(wp);

    wp = parse("Connection: X-Forwarded-Proto\r\n");
    ttrue((wp->flags & WEBS_HTTPS) != 0, "a Connection header naming X-Forwarded-Proto does not strip it first");
    tmatch(wp->protocol, "https", "the protocol still follows the proxy's scheme");
    websFree(wp);

    wp = parse("Connection: X-Forwarded-Proto, X-Custom\r\nX-Custom: 1\r\n");
    ttrue((wp->flags & WEBS_HTTPS) != 0, "the protected field survives a list that also names a real hop field");
    ttrue(websGetVar(wp, "HTTP_X_CUSTOM", 0) == 0, "an unprotected hop-by-hop field is still removed");
    websFree(wp);

    websSetTrustedProxies(0);
    websClose();
    return 0;
}
