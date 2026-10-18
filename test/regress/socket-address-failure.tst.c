/* Verify accept receives correctly sized storage and propagates the peer address. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

static int accepted, expectedPort, failAddress;
static Socket checkedAccept(Socket fd, struct sockaddr *addr, Socklen *length, size_t storageSize)
{
    teq((int) storageSize, (int) sizeof(Socklen), "accept length storage matches platform ABI");
    teq((int) *length, (int) sizeof(struct sockaddr_storage), "accept receives full address capacity");
    return accept(fd, addr, length);
}
#define accept(fd, addr, length) checkedAccept(fd, addr, length, sizeof(len))
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#define getnameinfo(...) (failAddress ? EAI_FAIL : getnameinfo(__VA_ARGS__))
#include "../../src/socket.c"
#undef getnameinfo
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef accept

static int onAccept(int sid, cchar *ip, int port, int listenSid)
{
    ttrue(!failAddress, "failed address must not reach callback");
    accepted++;
    ttrue(strcmp(ip, "127.0.0.1") == 0, "callback receives peer address");
    teq(port, expectedPort, "callback receives peer port");
    return -1;
}

int main(int argc, char **argv)
{
    struct sockaddr_in address;
    Socklen length;
    Socket client;
    int listener, rc;

    rc = socketOpen();
    teq(rc, 0, "initialize sockets");
    listener = socketListen("127.0.0.1", 0, onAccept, SOCKET_BLOCK);
    ttrue(listener >= 0, "create listener");
    if (listener < 0) return 0;
    length = sizeof(address);
    rc = getsockname(socketPtr(listener)->sock, (struct sockaddr*) &address, &length);
    teq(rc, 0, "read listening address");
    client = socket(AF_INET, SOCK_STREAM, 0);
    ttrue(client != SOCKET_ERROR, "create client");
    if (client == SOCKET_ERROR) return 0;
    rc = connect(client, (struct sockaddr*) &address, sizeof(address));
    teq(rc, 0, "connect client");
    length = sizeof(address);
    rc = getsockname(client, (struct sockaddr*) &address, &length);
    teq(rc, 0, "read client address");
    expectedPort = ntohs(address.sin_port);
    failAddress = 1;
    {
        char ip[64] = "stale";
        int port = 99;
        rc = socketAddress((struct sockaddr*) &address, sizeof(address), ip, sizeof(ip), &port);
        teq(rc, -1, "address formatter reports failure");
        teq(ip[0], 0, "failure clears address output");
        teq(port, 0, "failure clears port output");
    }
    socketAccept(socketPtr(listener));
    teq(accepted, 0, "address failure does not invoke callback");
    {
        char byte;
        rc = (int) recv(client, &byte, 1, 0);
        teq(rc, 0, "failed accepted socket is closed");
    }
    closesocket(client);
    client = socket(AF_INET, SOCK_STREAM, 0);
    length = sizeof(address);
    getsockname(socketPtr(listener)->sock, (struct sockaddr*) &address, &length);
    rc = connect(client, (struct sockaddr*) &address, sizeof(address));
    teq(rc, 0, "listener still accepts after address failure");
    length = sizeof(address);
    getsockname(client, (struct sockaddr*) &address, &length);
    expectedPort = ntohs(address.sin_port);
    failAddress = 0;
    socketAccept(socketPtr(listener));
    teq(accepted, 1, "accept callback invoked once");
    closesocket(client);
    socketCloseConnection(listener);
    socketClose();
    return 0;
}
