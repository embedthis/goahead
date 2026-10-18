/*
    socket-parse-empty.tst.c - An empty address parses as absent, on every form
 */

#include "testme.h"
#include "goahead.h"

static void check(cchar *address, cchar *expectedIp, int expectedPort)
{
    char *ip;
    int  port, secure;

    ip = (char*) "unset";
    port = -2;
    teq(socketParseAddress(address, &ip, &port, &secure, 80), 0, "parse \"%s\"", address);
    if (expectedIp == 0) {
        ttrue(ip == 0, "\"%s\" names no address and must yield null, got \"%s\"", address, ip ? ip : "null");
    } else {
        ttrue(ip && smatch(ip, expectedIp), "\"%s\" must yield \"%s\", got \"%s\"", address, expectedIp,
              ip ? ip : "null");
    }
    teq(port, expectedPort, "\"%s\" port", address);
    wfree(ip);
}


int main(int argc, char **argv)
{
    check("[]:8080", 0, 8080);
    check("[]", 0, 80);
    check(":8080", 0, 8080);
    check("", 0, 80);
    check("*:8080", 0, 8080);

    check("[::1]:8080", "::1", 8080);
    check("[::1]", "::1", 80);
    check("127.0.0.1:8080", "127.0.0.1", 8080);
    check("127.0.0.1", "127.0.0.1", 80);
    check("http://127.0.0.1:8080", "127.0.0.1", 8080);
    return 0;
}
