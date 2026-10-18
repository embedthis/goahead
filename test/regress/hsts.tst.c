/*
    hsts.tst.c - ME_GOAHEAD_HSTS sends Strict-Transport-Security on HTTPS responses only
 */

#define ME_GOAHEAD_HSTS "max-age=31536000"

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

static bool sendsHsts(bool secure)
{
    Webs *wp;
    bool found;
    int  wid;

    wid = websAlloc(-1);
    wp = webs[wid];
    wp->method = sclone("GET");
    wp->code = 200;
    if (secure) {
        wp->flags |= WEBS_SECURE | WEBS_HTTPS;
    }
    websWriteHeaders(wp, 0, NULL);
    bufAddNull(&wp->output);
    found = strstr(bufStart(&wp->output), "Strict-Transport-Security: max-age=31536000\r\n") != 0;
    websFree(wp);
    return found;
}


int main(int argc, char **argv)
{
    ttrue(sendsHsts(1), "an HTTPS response must carry the configured Strict-Transport-Security");
    ttrue(!sendsHsts(0), "a plain HTTP response must not carry Strict-Transport-Security");
    return 0;
}
