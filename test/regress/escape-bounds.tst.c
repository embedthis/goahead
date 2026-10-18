/*
    escape-bounds.tst.c - websEscapeHtml and websEscapeUri write exactly what they size
 */

#include "testme.h"
#include "goahead.h"

static cchar *entityFor(int c)
{
    switch (c) {
    case '&': return "&amp;";
    case '<': return "&lt;";
    case '>': return "&gt;";
    case '"': return "&quot;";
    case '\'': return "&#39;";
    }
    return 0;
}


int main(int argc, char **argv)
{
    char  one[2], run[513], expected[16], *result;
    cchar *entity;
    int   c, i, bad;

    for (bad = 0, c = 1; c < 256; c++) {
        one[0] = (char) c;
        one[1] = '\0';
        entity = entityFor(c);
        scopy(expected, sizeof(expected), entity ? entity : one);

        result = websEscapeHtml(one);
        if (!smatch(result, expected)) {
            tinfo("websEscapeHtml of byte %d gave [%s], expected [%s]", c, result, expected);
            bad++;
        }
        wfree(result);

        memset(run, c, sizeof(run) - 1);
        run[sizeof(run) - 1] = '\0';
        result = websEscapeHtml(run);
        if (slen(result) != (ssize) slen(expected) * 512) {
            tinfo("websEscapeHtml of 512 bytes of %d gave %d bytes", c, (int) slen(result));
            bad++;
        }
        wfree(result);

        result = websEscapeUri(run);
        for (i = 0; result[i]; i++) {
            if (result[i] != c && result[i] != '%' && !isxdigit((uchar) result[i])) {
                break;
            }
        }
        if (result[i] || (slen(result) != 512 && slen(result) != 512 * 3)) {
            tinfo("websEscapeUri of 512 bytes of %d gave %d bytes", c, (int) slen(result));
            bad++;
        }
        wfree(result);
    }
    teq(bad, 0, "every byte must escape to exactly its expected text");
    return 0;
}
