/* Inject recv results at the real socketRead boundary, without signal timing races. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

#if !ME_WIN_LIKE
static int nextError, readCalls;
static ssize fakeRecv(Socket sock, void *buf, size_t len, int flags)
{
    readCalls++;
    if (nextError) {
        errno = nextError;
        nextError = 0;
        return -1;
    }
    if (len) ((char*) buf)[0] = 'x';
    return len ? 1 : 0;
}
#define recv fakeRecv
/* Use the production module's normal signed-conversion policy. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/socket.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#undef recv
int main(int argc, char **argv)
{
    WebsSocket sp, *list[1];
    char buf[8];
    ssize rc;
    int errors[] = { EINTR, EAGAIN, EWOULDBLOCK, ECONNRESET }, i;
    memset(&sp, 0, sizeof(sp));
    list[0] = &sp;
    socketList = list;
    socketMax = 1;
    for (i = 0; i < 4; i++) {
        sp.flags = 0;
        nextError = errors[i];
        readCalls = 0;
        rc = socketRead(0, buf, sizeof(buf));
        if (errors[i] == ECONNRESET) {
            ttrue(rc < 0, "fatal error returned");
            ttrue((sp.flags & SOCKET_EOF) != 0, "fatal error marks EOF");
        } else {
            ttrue(rc >= 0, "transient error does not fail connection");
            teq(sp.flags & SOCKET_EOF, 0, "transient error leaves socket open");
            if (rc == 0) rc = socketRead(0, buf, sizeof(buf));
            teq(rc, 1, "read succeeds after transient error");
            teq(buf[0], 'x', "received byte is preserved");
        }
    }
    socketList = NULL;
    socketMax = 0;
    return 0;
}
#else
int main(int argc, char **argv) { tskip("POSIX recv error injection"); return 0; }
#endif
