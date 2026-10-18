/*
    local-host-mdns.tst.c - The mDNS retry for the local host address frees the name it builds
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

#if MACOSX
static int  lookups;
static char *retryName;
static char retryCopy[160];
static int  retryFreed;

static int fakeGetaddrinfo(cchar *node, cchar *service, const struct addrinfo *hints, struct addrinfo **res)
{
    if (++lookups == 1) {
        return EAI_NONAME;
    }
    retryName = (char*) node;
    scopy(retryCopy, sizeof(retryCopy), node);
    return getaddrinfo("127.0.0.1", service, hints, res);
}


static void trackedFree(void *ptr)
{
    if (ptr && ptr == retryName) {
        retryFreed = 1;
    }
    wfree(ptr);
}

#define getaddrinfo fakeGetaddrinfo
#define wfree trackedFree
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/http.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef getaddrinfo
#undef wfree
#endif


int main(int argc, char **argv)
{
#if MACOSX
    setLocalHost();
    teq(lookups, 2, "an unresolved host name must be retried in its .local form");
    ttrue(strstr(retryCopy, ".local") != 0, "the retry must look up the .local name, got %s", retryCopy);
    ttrue(retryFreed, "the .local name must be freed");
#else
    tinfo("The .local retry is macOS only");
#endif
    return 0;
}
