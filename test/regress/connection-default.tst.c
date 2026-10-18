/* Exercise the HTTP engine with a constrained descriptor-table configuration. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

enum { desktopDefault = ME_GOAHEAD_LIMIT_CONNECTIONS, desktopSize = FD_SETSIZE };
/* System headers are already loaded: change the configuration without changing their ABI. */
#undef FD_SETSIZE
#define FD_SETSIZE 16
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/http.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

int main(int argc, char **argv)
{
    struct sockaddr_in address;
    Socklen length;
    Socket client;
    int listener, rc;

    teq(desktopDefault, desktopSize > 40 ? desktopSize - 32 : 8, "native default");
    teq(connLimit, 8, "small-table startup default");
    websSetConnectionLimit(3);
    teq(connLimit, 3, "positive runtime override");
    websSetConnectionLimit(0);
    teq(connLimit, 8, "zero resets to a usable default");
    websSetConnectionLimit(-1);
    teq(connLimit, 8, "negative resets to a usable default");
    rc = websOpen("../web", NULL);
    teq(rc, 0, "initialize HTTP engine");
    listener = socketListen("127.0.0.1", 0, websAccept, 0);
    ttrue(listener >= 0, "create listener");
    length = sizeof(address);
    rc = getsockname(socketPtr(listener)->sock, (struct sockaddr*) &address, &length);
    teq(rc, 0, "read listening address");
    client = socket(AF_INET, SOCK_STREAM, 0);
    ttrue(client != SOCKET_ERROR, "create client");
    rc = connect(client, (struct sockaddr*) &address, sizeof(address));
    teq(rc, 0, "connect client");
    socketSelect(-1, 1000);
    socketProcess();
    teq(connCount, 1, "small-table default accepts the first connection");
    closesocket(client);
    socketCloseConnection(listener);
    websClose();
    return 0;
}
