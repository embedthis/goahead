/*
    MbedTLS sends report WANT_WRITE for blocked or timed-out sockets in either mode, and resets for reset peers.
 */
#include "goahead.h"
#include "mbedtls/ssl.h"

static int sendErrno, recvErrno;

static ssize fakeSend(int sock, const void *buf, size_t len, int flags)
{
    if (sendErrno) {
        errno = sendErrno;
        return -1;
    }
    return (ssize) len;
}


static ssize fakeRecv(int sock, void *buf, size_t len, int flags)
{
    if (recvErrno) {
        errno = recvErrno;
        return -1;
    }
    return 0;
}
#define send fakeSend
#define recv fakeRecv
#include "../../src/mbedtls.c"
#undef send
#undef recv

#define CHECK(cond) if (!(cond)) { fprintf(stderr, "FAILED: %s at line %d\n", #cond, __LINE__); return 1; }

int main(int argc, char **argv)
{
    WebsSocket *sp;
    uchar      buf[64];
    int        sid;

    CHECK(socketOpen() == 0);
    sid = socketAlloc("127.0.0.1", 0, NULL, 0);
    CHECK(sid >= 0);
    sp = socketPtr(sid);
    socketSetBlock(sid, 1);

    sendErrno = 0;
    CHECK(mbedSend(sp, buf, sizeof(buf)) == (int) sizeof(buf));
    sendErrno = EAGAIN;
    CHECK(mbedSend(sp, buf, sizeof(buf)) == MBEDTLS_ERR_SSL_WANT_WRITE);
    sendErrno = ETIMEDOUT;
    CHECK(mbedSend(sp, buf, sizeof(buf)) == MBEDTLS_ERR_SSL_WANT_WRITE);
    sendErrno = EPIPE;
    CHECK(mbedSend(sp, buf, sizeof(buf)) == MBEDTLS_ERR_NET_CONN_RESET);
    sendErrno = ECONNRESET;
    CHECK(mbedSend(sp, buf, sizeof(buf)) == MBEDTLS_ERR_NET_CONN_RESET);
    sendErrno = EBADF;
    CHECK(mbedSend(sp, buf, sizeof(buf)) == MBEDTLS_ERR_NET_SEND_FAILED);

    recvErrno = EAGAIN;
    CHECK(mbedRecv(sp, buf, sizeof(buf)) == MBEDTLS_ERR_SSL_WANT_READ);
    recvErrno = ECONNRESET;
    CHECK(mbedRecv(sp, buf, sizeof(buf)) == MBEDTLS_ERR_NET_CONN_RESET);
    recvErrno = 0;
    CHECK(mbedRecv(sp, buf, sizeof(buf)) == 0);

    socketFree(sid);
    socketClose();
    puts("transport passed");
    return 0;
}
