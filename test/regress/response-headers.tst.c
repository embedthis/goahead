/*
    response-headers.tst.c - Configured headers are sent on every response
 */

#define ME_GOAHEAD_RESPONSE_HEADERS "Content-Security-Policy: default-src 'self'\n Referrer-Policy : no-referrer \r\n" \
    "no value here"

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

static char headers[4096];

static cchar *respond(int code, cchar *location)
{
    Webs *wp;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone("GET");
    wp->code = code;
    websWriteHeaders(wp, 0, location);
    bufAddNull(&wp->output);
    scopy(headers, sizeof(headers), bufStart(&wp->output));
    websFree(wp);
    return headers;
}


static int count(cchar *str, cchar *pattern)
{
    cchar *cp;
    int   n;

    for (n = 0, cp = str; (cp = strstr(cp, pattern)) != 0; cp++) {
        n++;
    }
    return n;
}


static void suiteA_setting(void)
{
    tinfo("\n--- Suite A: build setting ---\n");
    ttrue(strstr(respond(200, NULL), "\r\nContent-Security-Policy: default-src 'self'\r\n") != 0,
          "a success response must carry the configured Content-Security-Policy");
    ttrue(strstr(headers, "\r\nReferrer-Policy: no-referrer\r\n") != 0,
          "the name and value must be trimmed, got %s", headers);
    ttrue(strstr(respond(404, NULL), "\r\nContent-Security-Policy: default-src 'self'\r\n") != 0,
          "an error response must carry the configured headers");
    ttrue(strstr(respond(302, "/elsewhere"), "\r\nReferrer-Policy: no-referrer\r\n") != 0,
          "a redirect must carry the configured headers");
}


static void suiteB_api(void)
{
    tinfo("\n--- Suite B: websAddResponseHeader ---\n");
    websAddResponseHeader("Permissions-Policy", "camera=()");
    websAddResponseHeader("Referrer-Policy", "same-origin");
    websAddResponseHeader("Content-Security-Policy", NULL);
    respond(200, NULL);
    ttrue(strstr(headers, "\r\nPermissions-Policy: camera=()\r\n") != 0, "an added header must be sent");
    ttrue(strstr(headers, "\r\nReferrer-Policy: same-origin\r\n") != 0, "adding a header again must replace it");
    ttrue(count(headers, "Referrer-Policy:") == 1, "a replaced header must be sent once, got %s", headers);
    ttrue(strstr(headers, "Content-Security-Policy") == 0, "a removed header must not be sent");
}


static void suiteC_invalid(void)
{
    tinfo("\n--- Suite C: invalid lines ---\n");
    ttrue(strstr(respond(200, NULL), "no value here") == 0, "a line without a value must be ignored");
}


int main(int argc, char **argv)
{
    ttrue(websOpen("../web", NULL) == 0, "initialize HTTP engine");
    suiteA_setting();
    suiteB_api();
    suiteC_invalid();
    websClose();
    ttrue(responseHeaders < 0, "websClose must free the configured headers");
    return 0;
}
