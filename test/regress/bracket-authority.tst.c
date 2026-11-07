/*
    bracket-authority.tst - An absolute-form authority is one host and one port, and the path starts where it ends

    parseFirstLine cuts the authority of an absolute-form target at the first "/", "?" or "#" and validates it;
    websUrlParse then parses the same target to find the path. The two must agree on where the authority ends. An
    IP-literal bracket that is not closed before that cut, or a port that is not decimal digits, is not an
    authority and the request is refused, so the host and the path the server reads are the ones a peer reads.
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
    // Let websError complete without a socket
    wp->connError = 1;
    line = sfmt("GET %s HTTP/1.1\r\n", target);
    bufPutStr(&wp->rxbuf, line);
    wfree(line);
    parseFirstLine(wp);
    return wp;
}

/*
    An accepted target routes the path that follows its authority
 */
static void accepted(cchar *target, cchar *host, cchar *path)
{
    Webs *wp;

    wp = request(target);
    ttrue(!wp->finalized, "%s: must be accepted, got status %d", target, wp->code);
    if (!wp->finalized) {
        tmatch(wp->host, host, "the authority");
        tmatch(wp->path, path, "the path after the authority");
    }
    websFree(wp);
}

static void refused(cchar *target, cchar *why)
{
    Webs *wp;

    wp = request(target);
    ttrue(wp->finalized && wp->code == HTTP_CODE_BAD_REQUEST, "%s: %s, must be refused with 400, got %d (path %s)",
          target, why, wp->code, wp->path ? wp->path : "none");
    websFree(wp);
}

int main(int argc, char **argv)
{
    /*
        A bracket that does not close before the first "/" is not an IP-literal. At f983cd89 validHost admitted it
        and websUrlParse read the host up to a "]" past the "/", so the path was taken from inside the authority.
     */
    refused("http://[a/x]/admin/zsecret.txt", "an unclosed bracket");
    refused("http://[HTTP://a]", "a scheme inside brackets");
    refused("http://[//[/[b]/index.html", "nested brackets");
    refused("http://[::1/index.html", "an unclosed IPv6 literal");
    refused("http://a]/index.html", "a closing bracket without an opening one");
    refused("http://[a]b/index.html", "text after the bracket");
    refused("http://[::1]x/index.html", "text after the IPv6 literal");
    refused("http://[]/index.html", "an empty literal");
    refused("http://h:x/index.html", "a port that is not decimal");
    refused("http://h:80:81/index.html", "two ports");
    refused("http://:80/index.html", "a port without a host");

    /* Well-formed authorities are unchanged */
    accepted("http://[::1]/admin/zsecret.txt", "[::1]", "/admin/zsecret.txt");
    accepted("http://[::1]:28700/index.html", "[::1]:28700", "/index.html");
    accepted("http://[fe80::1]/", "[fe80::1]", "/");
    accepted("http://localhost/index.html", "localhost", "/index.html");
    accepted("http://localhost:80/index.html", "localhost:80", "/index.html");
    accepted("http://localhost:/index.html", "localhost:", "/index.html");
    accepted("http://example.com./index.html", "example.com.", "/index.html");
    accepted("http://my_host-1/index.html", "my_host-1", "/index.html");
    accepted("HTTPS://X.Y/index.html", "X.Y", "/index.html");

    /* The Host header grammar is the same function */
    ttrue(validHost("[::1]:18090"), "an IPv6 literal with a port is a valid Host");
    ttrue(validHost("127.0.0.1:18080"), "an IPv4 address with a port is a valid Host");
    ttrue(validHost("localhost:"), "an empty port is permitted (RFC 3986 3.2.3)");
    ttrue(!validHost("[a"), "an unclosed bracket is not a valid Host");
    ttrue(!validHost("a]b"), "a stray closing bracket is not a valid Host");
    ttrue(!validHost("[::1]x"), "text after an IPv6 literal is not a valid Host");
    ttrue(!validHost("localhost:x"), "a non-decimal port is not a valid Host");
    ttrue(!validHost(""), "an empty Host is not valid");
    return 0;
}
