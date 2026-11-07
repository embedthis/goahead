/*
    trusted-proxy-literal.tst.c - A trusted proxy address that is not a literal IP address is reported and ignored, and
    an IPv6 address matches the peer in any spelling

    websSetTrustedProxies stored each token as text and websIsTrustedProxy compared the peer address as text, so a
    CIDR range or host name trusted nothing in silence and "2001:0DB8::1" never matched the peer "2001:db8::1".
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

#define CAPTURE_SIZE 4096

#if ME_GOAHEAD_LOGGING
static char captured[CAPTURE_SIZE];

static void captureLog(int level, cchar *msg)
{
    ssize len;

    len = slen(captured);
    if (msg && len < CAPTURE_SIZE - 1) {
        scopy(&captured[len], CAPTURE_SIZE - len, msg);
        len = slen(captured);
        if (len < CAPTURE_SIZE - 1) {
            captured[len] = '\n';
            captured[len + 1] = '\0';
        }
    }
}
#endif

int main(int argc, char **argv)
{
#if ME_GOAHEAD_LOGGING
    logSetHandler(captureLog);
    captured[0] = '\0';
#endif

    websSetTrustedProxies("10.0.0.0/8, proxy.example, 2001:0DB8::1, [::FFFF:192.0.2.9], 127.0.0.1");

#if ME_GOAHEAD_LOGGING
    ttrue(strstr(captured, "10.0.0.0/8") != 0, "a CIDR range is reported: it is not a proxy address");
    ttrue(strstr(captured, "proxy.example") != 0, "a host name is reported: it is not a proxy address");
    ttrue(strstr(captured, "127.0.0.1") == 0, "a literal address is not reported");
#endif

    ttrue(websIsTrustedProxy("127.0.0.1"), "a literal IPv4 address is trusted");
    ttrue(websIsTrustedProxy("2001:db8::1"), "an IPv6 address is trusted in the spelling the socket layer reports");
    ttrue(websIsTrustedProxy("192.0.2.9"), "an IPv4-mapped IPv6 spelling trusts the IPv4 peer it names");
    ttrue(!websIsTrustedProxy("10.1.2.3"), "an address inside an ignored CIDR range is not trusted");
    ttrue(!websIsTrustedProxy("proxy.example"), "a host name never matches a peer address");
    ttrue(!websIsTrustedProxy("2001:db8::2"), "an unlisted IPv6 address is not trusted");

    websSetTrustedProxies(0);
    ttrue(!websIsTrustedProxy("127.0.0.1"), "clearing the list trusts no one");
    return 0;
}
