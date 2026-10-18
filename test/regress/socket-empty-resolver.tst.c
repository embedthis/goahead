/* Inject empty and populated resolver lists into the real address selector. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

#if ME_UNIX_LIKE || WINDOWS
static struct addrinfo entries[2];
static struct sockaddr_storage addresses[2];
static int mode, freed;
static int fakeGetaddrinfo(cchar *host, cchar *service, const struct addrinfo *hints, struct addrinfo **result)
{
    if (mode == 0) { *result = NULL; return 0; }
    if (mode == 4) { *result = NULL; return EAI_FAIL; }
    memset(entries, 0, sizeof(entries));
    memset(addresses, 0, sizeof(addresses));
    entries[0].ai_family = mode == 3 ? AF_INET6 : AF_INET;
    entries[1].ai_family = AF_INET6;
    entries[0].ai_addr = (struct sockaddr*) &addresses[0];
    entries[1].ai_addr = (struct sockaddr*) &addresses[1];
    entries[0].ai_addrlen = entries[1].ai_addrlen = sizeof(struct sockaddr_in);
    entries[0].ai_protocol = 10;
    entries[1].ai_protocol = 20;
    entries[0].ai_next = mode == 3 ? NULL : &entries[1];
    *result = entries;
    return 0;
}
static void fakeFreeaddrinfo(struct addrinfo *result) { freed++; }
#define getaddrinfo fakeGetaddrinfo
#define freeaddrinfo fakeFreeaddrinfo
/* Use the production module's normal signed-conversion policy. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/socket.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#undef getaddrinfo
#undef freeaddrinfo
int main(int argc, char **argv)
{
    struct sockaddr_storage addr;
    Socklen len;
    int family, protocol, rc;
    for (mode = 0; mode <= 4; mode++) {
        freed = 0;
        rc = socketInfo(mode == 2 ? "::1" : "127.0.0.1", 80, &family, &protocol, &addr, &len);
        if (mode == 0 || mode == 4) {
            teq(rc, -1, "empty or failed resolution returns error");
            teq(freed, 0, "no absent list to release");
        } else {
            teq(rc, 0, "populated list resolves");
            teq(family, mode == 1 ? AF_INET : AF_INET6, "family selection or fallback");
            teq(protocol, mode == 2 ? 20 : 10, "selected entry copied");
            teq(freed, 1, "result list released once");
        }
    }
    return 0;
}
#else
int main(int argc, char **argv) { tskip("getaddrinfo backend unavailable"); return 0; }
#endif
