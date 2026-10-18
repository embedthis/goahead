/*
    socket-accept-refused.tst.c - A connection accepted when the socket table is full is closed, not abandoned
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
#include "../../src/socket.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static int accepted;

static int onAccept(int sid, cchar *ip, int port, int listenSid)
{
    accepted++;
    return -1;
}


int main(int argc, char **argv)
{
    struct sockaddr_in address;
    WebsSocket         *lp;
    Socklen            length;
    Socket             client;
    char               byte;
    int                listener, rc, saveMax;

#if ME_WIN_LIKE
    DWORD timeout = 5000;
#else
    struct timeval timeout = { 5, 0 };
#endif

    rc = socketOpen();
    teq(rc, 0, "initialize sockets");
    listener = socketListen("127.0.0.1", 0, onAccept, SOCKET_BLOCK);
    ttrue(listener >= 0, "create listener");
    if (listener < 0) return 0;
    lp = socketPtr(listener);
    length = sizeof(address);
    rc = getsockname(lp->sock, (struct sockaddr*) &address, &length);
    teq(rc, 0, "read listening address");

    client = socket(AF_INET, SOCK_STREAM, 0);
    ttrue(client != SOCKET_ERROR, "create client");
    if (client == SOCKET_ERROR) return 0;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char*) &timeout, sizeof(timeout));
    rc = connect(client, (struct sockaddr*) &address, sizeof(address));
    teq(rc, 0, "connect client");

    saveMax = socketMax;
    socketMax = FD_SETSIZE;
    socketAccept(lp);
    socketMax = saveMax;

    teq(accepted, 0, "a refused connection must not reach the accept callback");
    rc = (int) recv(client, &byte, 1, 0);
    teq(rc, 0, "a refused connection must be closed, not left open");

    closesocket(client);
    socketCloseConnection(listener);
    socketClose();
    return 0;
}
