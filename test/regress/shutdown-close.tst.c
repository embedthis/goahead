/* Shutdown stays inside the socket and connection tables when they are exactly full */
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
#include "../../src/http.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#define TABLE_FULL  16                      /* One full block of handle table entries */
#define TEST_PORT   18391
#define TEST_URL    "http://127.0.0.1:18391"

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static void suiteA_socketTable(void)
{
    int i, rc;

    tinfo("\n--- Suite A: socketClose with a full socket table ---\n");

    rc = socketOpen();
    teq(rc, 0, "open the socket layer");
    for (i = 0; i < TABLE_FULL; i++) {
        ttrue(socketAlloc("127.0.0.1", 0, NULL, 0) >= 0, "allocate a socket");
    }
    teq(socketMax, TABLE_FULL, "the table is exactly full");
    socketClose();
    ttrue(socketList == NULL, "every socket is freed");
}


static Socket connectToServer(void)
{
    struct sockaddr_in addr;
    Socket             sock;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR) {
        return SOCKET_ERROR;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TEST_PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock, (struct sockaddr*) &addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock);
        return SOCKET_ERROR;
    }
    return sock;
}


static void suiteB_connectionTable(void)
{
    Socket clients[TABLE_FULL];
    int    i, rc, spins;

    tinfo("\n--- Suite B: websClose with a full connection table ---\n");

    rc = websOpen("../web", NULL);
    teq(rc, 0, "open the server");
    websSetConnectionLimitPerIp(0);
    rc = websListen(TEST_URL);
    teq(rc, 0, "listen on a loopback port");

    for (i = 0; i < TABLE_FULL; i++) {
        clients[i] = connectToServer();
        ttrue(clients[i] != SOCKET_ERROR, "connect a client");
    }
    for (spins = 0; spins < 200 && websGetConnectionCount() < TABLE_FULL; spins++) {
        if (socketSelect(-1, 50)) {
            socketProcess();
        }
    }
    teq(websGetConnectionCount(), TABLE_FULL, "the server accepts every connection");
    teq(websMax, TABLE_FULL, "the connection table is exactly full");

    websClose();
    teq(websGetConnectionCount(), 0, "shutdown frees every connection");
    ttrue(webs == NULL, "the connection table is freed");
    ttrue(socketList == NULL, "the socket table is freed");

    for (i = 0; i < TABLE_FULL; i++) {
        closesocket(clients[i]);
    }
}


int main(int argc, char **argv)
{
#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    suiteA_socketTable();
    suiteB_connectionTable();
    return 0;
}
