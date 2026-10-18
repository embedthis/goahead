/*
    http-client-fixture.h - Raw socket client helpers for C regression tests
 */

#ifndef _h_HTTP_CLIENT_FIXTURE
#define _h_HTTP_CLIENT_FIXTURE 1

/*
    Cast lengths to match Winsock int and POSIX size_t parameters.
 */
#if ME_WIN_LIKE
    #define IOLEN(len)  ((int) (len))
#else
    #define IOLEN(len)  ((size_t) (len))
#endif

//  A test uses only the helpers it needs
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

static void napMsec(int msec)
{
#if ME_WIN_LIKE
    Sleep((DWORD) msec);
#else
    struct timespec ts;
    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;
    nanosleep(&ts, NULL);
#endif
}


static int serverPort(void)
{
    cchar *http, *colon;

    http = tget("TM_HTTP", "http://127.0.0.1:18080");
    if ((colon = strrchr(http, ':')) != 0) {
        return atoi(&colon[1]);
    }
    return 18080;
}


/*
    Set receive-buffer size before connecting to force server backpressure. Return SOCKET_ERROR on failure.
 */
static Socket openConn(int rcvbuf, int timeout)
{
    struct sockaddr_in addr;
    Socket             sock;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR) {
        return SOCKET_ERROR;
    }
    if (rcvbuf) {
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*) &rcvbuf, sizeof(rcvbuf));
    }
    if (timeout) {
#if ME_WIN_LIKE
        DWORD tv = (DWORD) (timeout * 1000);
#else
        struct timeval tv;
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
#endif
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*) &tv, sizeof(tv));
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((ushort) serverPort());
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr*) &addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return SOCKET_ERROR;
    }
    return sock;
}


static void closeConn(Socket sock)
{
    if (sock != SOCKET_ERROR) {
        closesocket(sock);
    }
}


/*
    Use HTTP/1.0 to compare bodies without timing-dependent chunk framing.
 */
static int sendRequest(Socket sock, cchar *url, cchar *protocol)
{
    char  request[512];
    ssize len;

    fmt(request, sizeof(request), "GET %s %s\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n", url, protocol);
    len = (ssize) slen(request);
    return send(sock, request, IOLEN(len), 0) == len ? 0 : -1;
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif /* _h_HTTP_CLIENT_FIXTURE */
