/*
    audit-field-forgery.tst.c - A client-chosen user name cannot forge the fields that follow it in an audit line

    The level-0 authentication line quotes the user name; the access log writes it as an unquoted, space-delimited
    field. A name carrying a quote or a space must be escaped so that a parser reading the line sees the server's
    client address, route and request, not ones the client wrote into its name.
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

#if ME_GOAHEAD_LOGGING && ME_GOAHEAD_AUTH
static char captured[1024];
static int  capturedCount;

static void captureLog(int flags, cchar *msg)
{
    scopy(captured, sizeof(captured), msg);
    capturedCount++;
}


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


static void setUser(Webs *wp, cchar *name)
{
    wfreeset(wp->username);
    wp->username = sclone(name);
}
#endif


int main(int argc, char **argv)
{
#if ME_GOAHEAD_LOGGING && ME_GOAHEAD_AUTH
    Webs  *wp;
    char  first[1024], *log;
    cchar *field, *quote, *user;

    logSetHandler(captureLog);
    websSetLogLevel(0);
    teq(websOpen("../web", NULL), 0, "initialize the server with the access log");

    wp = walloc(sizeof(Webs));
    memset(wp, 0, sizeof(Webs));
    scopy(wp->ipaddr, sizeof(wp->ipaddr), "192.0.2.77");
    wp->method = sclone("GET");
    wp->path = sclone("/ok");
    wp->protoVersion = sclone("HTTP/1.1");

    /*
        Authentication line: the name closes the quoted field and writes its own address, route and outcome
     */
    setUser(wp, "x\" from 10.9.9.9 on /admin (form): succeeded");
    capturedCount = 0;
    websLogAuth(wp, 0, "failed", "unknown user");
    teq(capturedCount, 1, "a refused login must write exactly one line");
    field = strstr(captured, "for user \"");
    ttrue(field != 0, "the line must carry the quoted user field");
    if (field) {
        field += 10;
        quote = strchr(field, '"');
        ttrue(quote != 0, "the user field must be closed");
        ttrue(quote && sncmp(quote, "\" from 192.0.2.77 on ", 21) == 0,
              "the quote closing the user field must be followed by the client address, not one from the name");
    }
    scopy(first, sizeof(first), captured);
    setUser(wp, "x\\x22 from 10.9.9.9 on /admin (form): succeeded");
    websLogAuth(wp, 0, "failed", "unknown user");
    ttrue(!smatch(first, captured), "a literal \\x22 in the name must not read as an escaped quote");

    /*
        Access log: the name is an unquoted field, so a space in it supplies a date, request and status
     */
    setUser(wp, "admin [01/Jan/2026:00:00:00 +0000] \"GET /x HTTP/1.1\" 200 1");
    logRequest(wp, 401);
    log = readLog();
    ttrue(log != 0, "the access log must be readable");
    if (log) {
        user = strstr(log, "192.0.2.77 - ");
        ttrue(user != 0, "the line must start with the client address and identity");
        if (user) {
            user += 13;
            field = strchr(user, ' ');
            ttrue(field && field[1] == '[', "the field after the user name must be the request time");
        }
        quote = strchr(log, '"');
        ttrue(quote && sncmp(quote, "\"GET /ok HTTP/1.1\" 401", 22) == 0,
              "the first quoted field must be the request that was served, not one written in the user name");
        ttrue(strstr(log, "\"GET /x HTTP/1.1\" 200") == 0, "the name must not supply a quoted request field");
        wfree(log);
    }
    wfreeset(wp->username);
    wfree(wp->method);
    wfree(wp->path);
    wfree(wp->protoVersion);
    wfree(wp);
    websClose();
    unlink("access.log");
#else
    tskip("Authentication or logging is not compiled in");
#endif
    return 0;
}
