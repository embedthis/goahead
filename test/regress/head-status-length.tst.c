/* Status-based Content-Length suppression applies to HEAD and GET alike. */
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
int main(int argc, char **argv)
{
    int codes[] = { 100, 204, 304, 200 }, i, j, wid;
    Webs *wp;
    char *header, *out;
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 4; j++) {
            wid = websAlloc(-1);
            ttrue(wid >= 0, "allocate request");
            wp = webs[wid];
            wp->method = sclone(i ? "GET" : "HEAD");
            wp->code = codes[j];
            websWriteHeaders(wp, 42, NULL);
            bufAddNull(&wp->output);
            header = strstr((char*) wp->output.servp, "Content-Length:");
            if (codes[j] == 200) {
                tnotnull(header, "ordinary response preserves length");
                if (header) tcontains(header, "42", "ordinary length unchanged");
            } else {
                tnull(header, "bodyless status does not advertise a misleading length");
            }
            websFree(wp);
        }
    }


    for (i = 0; i < 3; i++) {
        wid = websAlloc(-1);
        wp = webs[wid];
        wp->method = sclone(i == 0 ? "HEAD" : "GET");
        wp->code = i == 2 ? 304 : (i == 1 ? 204 : 200);
        wp->protoVersion = sclone("HTTP/1.1");
        wp->flags |= WEBS_HTTP11 | WEBS_KEEP_ALIVE;
        websWriteHeaders(wp, -1, NULL);
        websWriteEndHeaders(wp);
        teqz(websWriteBlock(wp, "body", 4), 4, "the handler's write is reported written");
        bufAddNull(&wp->output);
        out = (char*) wp->output.servp;
        ttrue(strstr(out, "body") == 0, "case %d: no body bytes may be sent", i);
        ttrue(slen(out) >= 4 && smatch(&out[slen(out) - 4], "\r\n\r\n"), "case %d: the headers must be terminated", i);
        ttrue(!(wp->flags & WEBS_CHUNKING), "case %d: no chunked framing", i);
        if (i > 0) {
            ttrue(strstr(out, "Transfer-Encoding") == 0, "case %d: no Transfer-Encoding", i);
        }
        websFree(wp);
    }
    return 0;
}
