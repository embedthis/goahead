/*
    Only document handlers infer Content-Type from the request extension; computed responses default to
    application/octet-stream. Set DOCROOT to an absolute path when running outside the suite.
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
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "../../src/http.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#ifndef DOCROOT
    #define DOCROOT "../web"
#endif

static char headers[4096];

#if ME_GOAHEAD_LOGGING
static void quietLog(int level, cchar *msg)
{
}
#endif


static bool untypedService(Webs *wp)
{
    return 1;
}


/*
    Write an untyped response of unknown length on the given route and return the Content-Type it was given.
 */
static cchar *contentType(WebsRoute *route, cchar *ext, int code)
{
    Webs  *wp;
    char  *line, *end;
    int   wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone("GET");
    wp->protoVersion = sclone("HTTP/1.1");
    wp->flags |= WEBS_HTTP11;
    wp->ext = sclone(ext);
    wp->route = route;
    wp->code = code;
    wp->error = code >= 400;
    websWriteHeaders(wp, -1, 0);
    websWriteEndHeaders(wp);
    bufAddNull(&wp->output);
    headers[0] = '\0';
    if ((line = strstr(bufStart(&wp->output), "\r\nContent-Type: ")) != 0) {
        line += 16;
        if ((end = strstr(line, "\r\n")) != 0) {
            sncopy(headers, sizeof(headers), line, end - line);
        }
    }
    wp->route = 0;
    websFree(wp);
    return headers;
}


int main(int argc, char **argv)
{
    WebsRoute *file, *untyped;

#if ME_GOAHEAD_LOGGING
    logSetHandler(quietLog);
#endif
    ttrue(websOpen(DOCROOT, NULL) == 0, "websOpen");
    websDefineHandler("untyped", 0, untypedService, 0, 0);
    file = websAddRoute("/docs/", "file", -1);
    untyped = websAddRoute("/api/", "untyped", -1);
    ttrue(file != 0 && untyped != 0, "routes");

    //  The file handler serves the path, so its extension names the body
    tmatch(contentType(file, ".html", 200), "text/html", "a document's extension names its type");
    tmatch(contentType(file, ".svg", 200), "image/svg+xml", "every table entry applies to a document");
    tmatch(contentType(file, ".zzz", 200), "application/octet-stream", "an unknown extension is not sniffable");

    //  A handler that ignores the trailing path segments must not let the client pick a scriptable type
    tmatch(contentType(untyped, ".html", 200), "application/octet-stream",
           "a client-chosen .html must not type an action or custom handler's response");
    tmatch(contentType(untyped, ".svg", 200), "application/octet-stream",
           "a client-chosen .svg must not type an action or custom handler's response");
    tmatch(contentType(untyped, NULL, 200), "application/octet-stream", "no extension stays octet-stream");

    //  An error page is the server's own HTML whatever the handler or the extension
    tmatch(contentType(untyped, ".svg", 404), "text/html", "an error page is HTML on any route");
    tmatch(contentType(file, ".svg", 404), "text/html", "an error page is HTML for a document too");
    tmatch(contentType(NULL, ".svg", 404), "text/html", "an error page before routing is HTML");
    tmatch(contentType(NULL, ".html", 200), "application/octet-stream", "an unrouted untyped body is octet-stream");

    websClose();
    return 0;
}
