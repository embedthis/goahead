/*
    access-log-escape.tst.c - A request field cannot forge an access log entry
 */

#define ME_GOAHEAD_ACCESS_LOG 1

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
#include "../../src/http.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif

static char *readLog(void)
{
    char  *buf;
    ssize len;
    int   fd;

    if ((fd = open("access.log", O_RDONLY | O_BINARY, 0)) < 0) {
        return 0;
    }
    buf = walloc(8192);
    len = read(fd, buf, 8191);
    close(fd);
    buf[len > 0 ? len : 0] = '\0';
    return buf;
}


static int countLines(cchar *text)
{
    int count;

    for (count = 0; *text; text++) {
        if (*text == '\n') {
            count++;
        }
    }
    return count;
}


int main(int argc, char **argv)
{
    Webs *wp;
    char *log;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    teq(websOpen("../web", NULL), 0, "initialize the server with the access log");

    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));
    scopy(wp->ipaddr, sizeof(wp->ipaddr), "127.0.0.1");
    wp->method = sclone("GET");
    wp->path = sclone("/a\"b\\c");
    wp->protoVersion = sclone("HTTP/1.1");
    wp->username = sclone("joe\r\n10.0.0.1 - admin [01/Jan/2026:00:00:00 +0000] \"GET /x HTTP/1.1\" 200 1\x1b[2J");

    logRequest(wp, 200);
    log = readLog();
    ttrue(log != 0, "the access log must be readable");
    if (log) {
        teq(countLines(log), 1, "one request must write exactly one line");
        ttrue(strchr(log, '\r') == 0, "no raw CR may reach the log");
        ttrue(strchr(log, '\x1b') == 0, "no raw escape character may reach the log");
        ttrue(strstr(log, "joe\\x0d\\x0a10.0.0.1") != 0, "CR LF in the username must be written as \\xHH");
        ttrue(strstr(log, "\"GET /a\\x22b\\x5cc HTTP/1.1\"") != 0,
              "'\"' and '\\' in a quoted field must be written as \\xHH");
        wfree(log);
    }

    wfreeset(wp->username);
    wfreeset(wp->path);
    wp->path = sclone("/ok");
    logRequest(wp, 200);
    log = readLog();
    ttrue(log && strstr(log, "127.0.0.1 - - [") != 0, "a missing username must be written as \"-\"");
    wfree(log);

    wfree(wp->method);
    wfree(wp->path);
    wfree(wp->protoVersion);
    wfree(wp);
    websClose();
    unlink("access.log");
    return 0;
}
