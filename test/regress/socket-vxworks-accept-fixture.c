/*
    socket-vxworks-accept-fixture.c - Drive the VxWorks build of the socket layer on the host with no descriptor free.

    Compiled with the VxWorks platform defines against test/regress/vxworks and vxworks-host-stubs.h, so the arms
    that VxWorks compiles are the ones that run. A connection is queued on the listener, every descriptor is then
    used up, and the event loop is turned for 300 ms. A listener that cannot accept must not keep the loop awake,
    and the queued connection must be dropped rather than left waiting.

    Linux, 4.4BSD-derived and IPNET kernels fail accept() before dequeuing the connection when no descriptor is free,
    so the listener stays readable. XNU dequeues first and drops the connection when it cannot allocate the
    descriptor, which hides the problem on a macOS host, so accept() is wrapped to apply the common order here.
 */
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>

static int vxAccept(int s, struct sockaddr *addr, void *len)
{
    int probe;

    if ((probe = open("/dev/null", O_RDONLY)) < 0) {
        return -1;
    }
    close(probe);
    return accept(s, addr, (socklen_t*) len);
}
#define accept vxAccept

#include "../../src/socket.c"

#include <sys/resource.h>
#include <sys/time.h>

/*********************************** Host stand-ins ****************************/

int vxIoctl(int fd, int function, int *arg)
{
    int flags;

    if (function != FIONBIO) {
        return ERROR;
    }
    flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, *arg ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
}


int hostGetByName(char *name)
{
    return ERROR;
}


DEV_HDR *iosDevFind(char *name, const char **pNameTail)
{
    *pNameTail = name;
    return NULL;
}

/*********************************** Fixture ***********************************/

#define EXHAUST_MAX 128
#define TURN_MS     300
#define MAX_WAKEUPS 20

static int           held[EXHAUST_MAX];
static int           heldCount;
static struct rlimit savedLimit;

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif


static int refuse(int sid, cchar *ip, int port, int listenSid)
{
    return -1;
}


static void exhaustDescriptors(void)
{
    struct rlimit limit;
    int           fd;

    getrlimit(RLIMIT_NOFILE, &savedLimit);
    limit = savedLimit;
    limit.rlim_cur = 64;
    setrlimit(RLIMIT_NOFILE, &limit);
    for (heldCount = 0; heldCount < EXHAUST_MAX && (fd = open("/dev/null", O_RDONLY)) >= 0; heldCount++) {
        held[heldCount] = fd;
    }
}


static void releaseDescriptors(void)
{
    while (heldCount > 0) {
        close(held[--heldCount]);
    }
    setrlimit(RLIMIT_NOFILE, &savedLimit);
}


static double nowMs(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (double) tv.tv_sec * 1000.0 + (double) tv.tv_usec / 1000.0;
}


int main(int argc, char **argv)
{
    struct sockaddr_in addr;
    struct timeval     tv;
    socklen_t          len;
    double             start;
    ssize              n;
    char               byte;
    int                sid, client, wakeups, closed, pass;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    if (socketOpen() < 0) {
        printf("cannot open the socket layer\n");
        return 2;
    }
    if ((sid = socketListen("127.0.0.1", 0, refuse, 0)) < 0) {
        printf("cannot listen on an ephemeral port\n");
        return 2;
    }
    len = sizeof(addr);
    getsockname(socketPtr(sid)->sock, (struct sockaddr*) &addr, &len);

    client = socket(AF_INET, SOCK_STREAM, 0);
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (char*) &tv, sizeof(tv));
    if (connect(client, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        printf("cannot queue a connection\n");
        return 2;
    }

    /*
        Turn the loop with the listener readable and no descriptor free. Each socketSelect waits up to 50 ms, so a
        loop that only wakes for real events turns about six times; a loop that wakes for the listener on every
        turn goes round thousands of times.
     */
    exhaustDescriptors();
    if (heldCount == 0 || heldCount >= EXHAUST_MAX) {
        printf("cannot exhaust descriptors: held %d\n", heldCount);
        releaseDescriptors();
        return 2;
    }
    wakeups = 0;
    start = nowMs();
    while (nowMs() - start < TURN_MS) {
        if (socketSelect(-1, 50) > 0) {
            socketProcess();
        }
        wakeups++;
    }
    releaseDescriptors();

    n = recv(client, &byte, 1, 0);
    closed = (n == 0 || (n < 0 && errno == ECONNRESET));
    printf("wakeups %d in %d ms, queued connection %s\n", wakeups, TURN_MS, closed ? "closed" : "still queued");

    pass = wakeups <= MAX_WAKEUPS && closed;
    printf("%s\n", pass ? "accept exhaustion passed" : "accept exhaustion failed");

    close(client);
    socketCloseConnection(sid);
    socketClose();
    return pass ? 0 : 1;
}
