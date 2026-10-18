/* An error response survives the rest of the request body arriving after it */

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

#define ATTEMPTS        20
#define BODY_CHUNKS     64
#define RECV_TIMEOUT    10

static char body[64 * 1024];
static char readBuf[4096];

/*
    Return 1 for 413 plus clean close, 0 for missing 413, or -1 for 413 plus reset.
 */
static int refusedBodyReadsResponse(void)
{
    cchar  *head;
    Socket sock;
    ssize  nbytes, len;
    int    i;

    if ((sock = openConn(0, RECV_TIMEOUT)) == SOCKET_ERROR) {
        return 0;
    }
    head = "POST /index.html HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: text/plain\r\n"
           "Content-Length: 100000000\r\n\r\n";
    send(sock, head, IOLEN(slen(head)), 0);
    for (i = 0; i < BODY_CHUNKS; i++) {
        if (send(sock, body, IOLEN(sizeof(body)), 0) < 0) {
            break;
        }
    }
    napMsec(50);
    len = 0;
    nbytes = 0;
    while (len < (ssize) sizeof(readBuf) - 1 &&
           (nbytes = recv(sock, &readBuf[len], IOLEN((ssize) sizeof(readBuf) - 1 - len), 0)) > 0) {
        len += nbytes;
    }
    readBuf[len] = '\0';
    closeConn(sock);
    if (strncmp(readBuf, "HTTP/1.1 413", 12) != 0) {
        return 0;
    }
    return nbytes == 0 ? 1 : -1;
}


int main(int argc, char **argv)
{
    int i, lost, reset, rc;

#if ME_UNIX_LIKE
    signal(SIGPIPE, SIG_IGN);
#endif
    memset(body, 'x', sizeof(body));
    rc = socketOpen();
    teq(rc, 0, "open the socket layer");

    lost = reset = 0;
    for (i = 0; i < ATTEMPTS; i++) {
        rc = refusedBodyReadsResponse();
        if (rc == 0) {
            lost++;
        } else if (rc < 0) {
            reset++;
        }
    }
    teq(lost, 0, "every refused request reads its 413");
    teq(reset, 0, "and the connection then ends cleanly, not with a reset");
    socketClose();
    return 0;
}
