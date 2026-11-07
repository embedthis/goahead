/*
    A staged PUT body must be removed before a refusal reaches the socket.
    Interpose socket writes to count staging files as a PUT the file handler refuses receives 404. The file handler is
    compiled into the test beside http.c so that its websError and the flush run through the interposed write.
    Sibling of the multipart ordering rule: the staged upload files of a refused multipart request are released
    before the refusal is written; the staged PUT body is not.
 */

#define ME_GOAHEAD_PUT_DIR "put-release-before-refusal.dir"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"

#if !ME_ROM && ME_UNIX_LIKE
#include <dirent.h>

static ssize recordingWrite(int sid, void *buf, ssize len);

#define socketWrite recordingWrite
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/http.c"
#include "../../src/file.c"
#undef socketWrite
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#define PASSES          200         /* Event loop passes of 50 ms the exchange may take */

static int  refusals;               /* Writes that begin a 404 response */
static int  stagedAtRefusal;        /* Staging files present when such a write happened */
static int  listener;
static int  port;

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif


static int countStaged(void)
{
    DIR           *dir;
    struct dirent *dp;
    int           count;

    count = 0;
    if ((dir = opendir(ME_GOAHEAD_PUT_DIR)) == 0) {
        return 0;
    }
    while ((dp = readdir(dir)) != 0) {
        if (dp->d_name[0] != '.') {
            count++;
        }
    }
    closedir(dir);
    return count;
}


static void emptyStaging(void)
{
    DIR           *dir;
    struct dirent *dp;
    char          path[512];

    if ((dir = opendir(ME_GOAHEAD_PUT_DIR)) == 0) {
        return;
    }
    while ((dp = readdir(dir)) != 0) {
        if (dp->d_name[0] != '.') {
            fmt(path, sizeof(path), "%s/%s", ME_GOAHEAD_PUT_DIR, dp->d_name);
            unlink(path);
        }
    }
    closedir(dir);
}


/*
    The socket write http.c uses. Note what is staged when a refusal starts.
 */
static ssize recordingWrite(int sid, void *buf, ssize len)
{
    if (len >= 12 && strncmp((char*) buf, "HTTP/1.1 404", 12) == 0) {
        refusals++;
        stagedAtRefusal += countStaged();
    }
    return socketWrite(sid, buf, len);
}


static void drive(int passes)
{
    while (passes-- > 0) {
        if (socketSelect(-1, 50)) {
            socketProcess();
        }
        websRunEvents();
    }
}


static Socket openClient(void)
{
    struct sockaddr_in addr;
    struct timeval     tv;
    Socket             sock;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR) {
        return SOCKET_ERROR;
    }
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*) &tv, sizeof(tv));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((ushort) port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        closesocket(sock);
        return SOCKET_ERROR;
    }
    return sock;
}


/*
    Read until the connection closes or the passes are spent. Return the status code read, or 0.
 */
static int readStatus(Socket client)
{
    char  buf[4096];
    ssize nbytes, total;
    int   passes, status;

    total = 0;
    status = 0;
    for (passes = 0; passes < PASSES; passes++) {
        drive(1);
        nbytes = recv(client, &buf[total], (size_t) ((ssize) sizeof(buf) - 1 - total), 0);
        if (nbytes == 0) {
            break;
        }
        if (nbytes > 0) {
            total += nbytes;
            buf[total] = '\0';
            if (status == 0 && total >= 12 && strncmp(buf, "HTTP/1.1 ", 9) == 0) {
                status = atoi(&buf[9]);
            }
            if (total >= (ssize) sizeof(buf) - 1) {
                break;
            }
        }
    }
    return status;
}


/*
    The body is staged while it is received. The file handler then refuses the target, a template name, with 404.
 */
static void refusedAfterStagedBody(void)
{
    Socket client;
    char   *request;
    cchar  *body;
    int    status;

    tinfo("\n--- A PUT refusal is written only once the staged body is gone ---\n");

    client = openClient();
    ttrue(client != SOCKET_ERROR, "connect a client");
    if (client == SOCKET_ERROR) {
        return;
    }
    body = "<% write('staged'); %>\n";
    request = sfmt("PUT /zput/zstaged.jst HTTP/1.1\r\n"
                   "Host: 127.0.0.1\r\n"
                   "Content-Type: text/plain\r\n"
                   "Content-Length: %zd\r\n"
                   "Connection: close\r\n"
                   "\r\n%s", slen(body), body);
    ttrue(send(client, request, (size_t) slen(request), 0) == slen(request), "send the PUT request");
    status = readStatus(client);
    teq(status, 404, "a PUT of a template is refused");
    teq(refusals, 1, "the refusal is written once");
    teq(stagedAtRefusal, 0, "the staged body must be gone before the refusal reaches the socket");
    drive(4);
    teq(countStaged(), 0, "nothing stays staged after the refusal");
    closesocket(client);
    wfree(request);
}

#endif /* !ME_ROM && ME_UNIX_LIKE */


int main(int argc, char **argv)
{
    tinfo("put-release-before-refusal: a staged PUT body is released before a refusal is written\n");
#if !ME_ROM && ME_UNIX_LIKE
    {
        struct sockaddr_in address;
        Socklen            length;
        WebsRoute          *route;

#if ME_GOAHEAD_LOGGING
        logSetHandler(quietLog);
#endif
        mkdir(ME_GOAHEAD_PUT_DIR, 0700);
        emptyStaging();
        //  The target directory exists, so the file handler stages the body and refuses only the template name
        mkdir("../web/zput", 0700);
        teq(websOpen("../web", NULL), 0, "initialize the server");
        route = websAddRoute("/zput/", "file", 0);
        ttrue(route != 0, "define the PUT route");
        if (route) {
            route->methods = hashCreate(7);
            hashEnter(route->methods, "PUT", valueString("PUT", 0), 0);
        }
        listener = socketListen("127.0.0.1", 0, websAccept, 0);
        ttrue(listener >= 0, "create the listener");
        if (listener >= 0) {
            length = sizeof(address);
            teq(getsockname(socketPtr(listener)->sock, (struct sockaddr*) &address, &length), 0, "read the port");
            port = ntohs(address.sin_port);
            refusedAfterStagedBody();
            socketCloseConnection(listener);
        }
        websClose();
        emptyStaging();
        rmdir(ME_GOAHEAD_PUT_DIR);
        rmdir("../web/zput");
    }
#else
    tskip("ROM builds stage no PUT body, or the platform has no POSIX sockets");
#endif
    return 0;
}
