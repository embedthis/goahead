/*
    trusted-proxy-not-local.tst.c - A legacy build does not mark a trusted proxy's connections as local
 */

#define ME_GOAHEAD_LEGACY 1

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

#define TEST_PORT 18971

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

/*
    Connect to the listener from the loopback address with a plain socket and return the accepted request, or null.
 */
static Webs *acceptFromLoopback(int *client)
{
    struct sockaddr_in addr;
    Webs               *wp;
    int                i, wid;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TEST_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if ((*client = (int) socket(AF_INET, SOCK_STREAM, 0)) < 0 ||
        connect(*client, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        return 0;
    }
    for (i = 0; i < 50; i++) {
        socketSelect(-1, 100);
        socketProcess();
        for (wid = 0; wid < websMax; wid++) {
            if ((wp = webs[wid]) != 0 && wp->sid >= 0 && smatch(wp->ipaddr, "127.0.0.1")) {
                return wp;
            }
        }
    }
    return 0;
}


static void release(Webs *wp, int client)
{
    if (wp) {
        websFree(wp);
    }
    if (client >= 0) {
        closesocket(client);
    }
}


int main(int argc, char **argv)
{
    Webs *wp;
    char endpoint[64];
    int  client, listener;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    if (websOpen("../web", NULL) < 0) {
        tfail("cannot open the server");
        return 1;
    }
    fmt(endpoint, sizeof(endpoint), "http://127.0.0.1:%d", TEST_PORT);
    if ((listener = websListen(endpoint)) < 0) {
        tfail("cannot listen on %s", endpoint);
        return 1;
    }

    /* Control: with no trusted proxy, a loopback peer is a local client */
    wp = acceptFromLoopback(&client);
    ttrue(wp != 0, "a loopback connection is accepted");
    ttrue(wp && (wp->flags & WEBS_LOCAL), "without a trusted proxy a loopback peer is local");
    release(wp, client);

    /* A co-located trusted proxy forwards remote clients: its connections are not local */
    websSetTrustedProxies("127.0.0.1");
    wp = acceptFromLoopback(&client);
    ttrue(wp != 0, "a connection from the trusted proxy is accepted");
    ttrue(wp && !(wp->flags & WEBS_LOCAL), "a trusted proxy's connection must not be marked local");
    release(wp, client);

    websSetTrustedProxies(0);
    websClose();
    return 0;
}
