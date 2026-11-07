/*
    The Digest nonce counter parser must saturate on an over-long hex field, as hextoi does, so an
    unauthenticated client cannot make it wrap. The wrap is defined C, but the asan build runs with
    -fsanitize=integer and no recovery, so a request carrying a 17-digit counter aborts that server.
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
#undef ME_COMPILER_HAS_PAM
#define ME_COMPILER_HAS_PAM 0
#include "../../src/auth.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_AUTH && ME_GOAHEAD_DIGEST

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

/*
    A Digest request for the route whose nonce field is the given value and whose other fields are well formed.
 */
static Webs *request(WebsRoute *route, cchar *nonce)
{
    Webs *wp;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->route = route;
    wp->method = sclone("GET");
    wp->url = sclone("/d/x");
    wp->authDetails = sfmt("username=\"joshua\", realm=\"%s\", nonce=\"%s\", uri=\"/d/x\", response=\"x\", "
                           "qop=auth, nc=00000001, cnonce=\"abc\"", ME_GOAHEAD_REALM, nonce);
    return wp;
}

int main(int argc, char **argv)
{
    WebsRoute *route;
    Webs      *wp;
    char      *plain, *nonce;
    uint64    value;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    teq(websOpen("../web", NULL), 0, "open the server");
    route = websAddRoute("/d/", "continue", 0);
    teq(websSetRouteAuth(route, "digest"), 0, "make the route Digest");

    //  Exact values parse exactly, up to the full width of the counter
    ttrue(parseHex64("0") == 0, "zero parses");
    ttrue(parseHex64("1000") == 0x1000, "a short counter parses");
    ttrue(parseHex64("ffffffffffffffff") == MAXUINT64, "the widest counter parses");

    //  One digit more than the counter can hold must saturate, never wrap to a small value
    value = parseHex64("10000000000000000");
    ttrue(value == MAXUINT64, "a 17 digit counter must saturate, parsed to %llx", (unsigned long long) value);
    value = parseHex64("ffffffffffffffffffff");
    ttrue(value == MAXUINT64, "a 20 digit counter must saturate, parsed to %llx", (unsigned long long) value);
    value = parseHex64("0000000000000000000000000001");
    ttrue(value == 1, "leading zeros are not width");

    //  The same counter inside a request nonce is refused without reaching the response check
    plain = sfmt("00000000000000000000000000000000:%x:10000000000000000", (uint) time(0));
    nonce = websEncode64(plain);
    wp = request(route, nonce);
    ttrue(!parseDigestDetails(wp), "a nonce with an over-long counter must be refused");
    ttrue(wp->authReason && smatch(wp->authReason, "nonce mismatch"), "refused as a mismatch, got %s",
          wp->authReason ? wp->authReason : "-");
    websFree(wp);
    wfree(nonce);
    wfree(plain);

    websClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Digest authentication is not enabled");
    return 0;
}
#endif
