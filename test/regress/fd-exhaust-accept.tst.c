/* Running out of descriptors neither spins the listener nor is invited by the cap */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

#if ME_UNIX_LIKE
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/http.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "fd-exhaust-fixture.h"

#define LOW_LIMIT 128

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static int refuse(int sid, cchar *ip, int port, int listenSid)
{
    return -1;
}

static void testCapFitsLimit(void)
{
    struct rlimit saved, low;

    getrlimit(RLIMIT_NOFILE, &saved);
    low = saved;
    low.rlim_cur = LOW_LIMIT;
    setrlimit(RLIMIT_NOFILE, &low);
    websSetConnectionLimit(0);
    setFileLimits();
    ttrue(connLimit > 0 && connLimit < LOW_LIMIT, "the connection cap fits the descriptor limit, got %d", connLimit);
    setrlimit(RLIMIT_NOFILE, &saved);
    websSetConnectionLimit(0);
}

static void testExhaustedAccept(void)
{
    struct sockaddr_in addr;
    struct timeval     tv;
    Socklen            len;
    Exhaust            ex;
    char               byte;
    ssize              n;
    int                sid, client;

    teq(socketOpen(), 0, "open the socket layer");
    sid = socketListen("127.0.0.1", 0, refuse, 0);
    ttrue(sid >= 0, "listen on an ephemeral port");
    len = sizeof(addr);
    getsockname(socketPtr(sid)->sock, (struct sockaddr*) &addr, &len);

    client = socket(AF_INET, SOCK_STREAM, 0);
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char*) &tv, sizeof(tv));
    ttrue(connect(client, (struct sockaddr*) &addr, sizeof(addr)) == 0, "queue a connection");

    exhaustDescriptors(&ex);
    if (socketSelect(-1, 1000) > 0) {
        socketProcess();
    }
    releaseDescriptors(&ex);

    n = recv(client, &byte, 1, 0);
    ttrue(n == 0 || (n < 0 && errno == ECONNRESET),
          "a connection that arrives with no descriptor free is closed, not left queued");
    close(client);
    socketCloseConnection(sid);
    socketClose();
}

int main(int argc, char **argv)
{
#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    testCapFitsLimit();
    testExhaustedAccept();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Descriptor limits are POSIX");
    return 0;
}
#endif
