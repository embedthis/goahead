/*
    redirect-default-port.tst.c - A redirect names a port only when it is known and not the default
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

static void check(cchar *scheme, int port, cchar *expected, cchar *why)
{
    char *uri;

    uri = makeUri(scheme, "example.com", port, "/secure/");
    ttrue(smatch(uri, expected), "%s: expected %s, got %s", why, expected, uri);
    wfree(uri);
}


int main(int argc, char **argv)
{
    defaultSslPort = 0;
    defaultHttpPort = 8080;

    check("https", 0, "https://example.com/secure/", "an unknown https port must be left implied");
    check("http", 0, "http://example.com:8080/secure/", "a known http port must be named");

    check("https", 443, "https://example.com/secure/", "https on 443 is the default");
    check("http", 80, "http://example.com/secure/", "http on 80 is the default");
    check("http", 443, "http://example.com:443/secure/", "http on 443 must keep its port");
    check("https", 80, "https://example.com:80/secure/", "https on 80 must keep its port");
    check("https", 4443, "https://example.com:4443/secure/", "a non-default https port must be named");

    defaultSslPort = 4443;
    check("https", 0, "https://example.com:4443/secure/", "the listener's https port must be named");
    return 0;
}
