/* A CGI that exits without output does not stall other requests */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
#include "http-client-fixture.h"

#if ME_GOAHEAD_CGI

#define SILENT_URL      "/cgi-bin/cgitest?HTTP_SWITCHES=-x"
#define STATIC_URL      "/index.html"

//  Well under the second the stalled event loop held every connection for
#define PROMPT_MSEC     500
#define GIVE_UP_MSEC    10000
#define RECV_TIMEOUT    15

/*
    Pace requests to avoid testing connection-flood limits on Windows.
 */
#define PACE_MSEC       10

static char readBuf[4096];

static int64 msecNow(void)
{
#if ME_WIN_LIKE
    return (int64) GetTickCount64();
#else
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}



static int readStatus(Socket sock)
{
    ssize nbytes, len;
    int   status;

    len = 0;
    status = -1;
    while ((nbytes = recv(sock, &readBuf[len], IOLEN((ssize) sizeof(readBuf) - 1 - len), 0)) > 0) {
        if (status < 0) {
            len += nbytes;
            readBuf[len] = '\0';
            if (len >= 12 && strncmp(readBuf, "HTTP/1.", 7) == 0) {
                status = atoi(&readBuf[9]);
            }
        }
        if (status >= 0 || len >= (ssize) sizeof(readBuf) - 1) {
            len = 0;
        }
    }
    return status;
}


static bool readable(Socket sock)
{
    struct timeval tv;
    fd_set         fds;

    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
#if ME_WIN_LIKE
    return select(0, &fds, NULL, NULL, &tv) > 0;
#else
    return select(sock + 1, &fds, NULL, NULL, &tv) > 0;
#endif
}


int main(int argc, char **argv)
{
    Socket silent, sock;
    int64  started, begin, elapsed, slowest;
    int    count, status, rc;

    teq(socketOpen(), 0, "open the socket layer");
    silent = openConn(0, RECV_TIMEOUT);
    ttrue(silent != SOCKET_ERROR, "connect for the silent CGI");
    rc = sendRequest(silent, SILENT_URL, "HTTP/1.0");
    teq(rc, 0, "request the silent CGI");

    slowest = 0;
    count = 0;
    started = msecNow();
    while (!readable(silent) && msecNow() - started < GIVE_UP_MSEC) {
        begin = msecNow();
        sock = openConn(0, RECV_TIMEOUT);
        ttrue(sock != SOCKET_ERROR, "connect for a static request");
        rc = sendRequest(sock, STATIC_URL, "HTTP/1.0");
        teq(rc, 0, "send a static request");
        status = readStatus(sock);
        closeConn(sock);
        teq(status, 200, "a static request succeeds");
        elapsed = msecNow() - begin;
        slowest = max(slowest, elapsed);
        count++;
        napMsec(PACE_MSEC);
    }
    status = readStatus(silent);
    closeConn(silent);
    teq(status, 500, "a CGI that writes nothing ends in 500");
    ttrue(slowest < PROMPT_MSEC, "the slowest of %d static requests took %d msec while the silent CGI was outstanding",
          count, (int) slowest);
    socketClose();
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("CGI is not enabled");
    return 0;
}
#endif
