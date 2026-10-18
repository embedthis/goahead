/* A path holding a code point that case folds to ASCII is refused on a case-insensitive file system */

#ifndef ME_GOAHEAD_CASE_INSENSITIVE_FS
    #define ME_GOAHEAD_CASE_INSENSITIVE_FS 1
#endif

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

static const uint folding[] = {
    0x00DF, 0x0130, 0x0131, 0x0149, 0x017F, 0x01F0, 0x1E96, 0x1E97, 0x1E98, 0x1E99, 0x1E9A, 0x1E9E, 0x212A,
    0xFB00, 0xFB01, 0xFB02, 0xFB03, 0xFB04, 0xFB05, 0xFB06
};

/*
    Percent-encode one code point as UTF-8 inside a path segment
 */
static char *pathWith(uint c)
{
    char buf[16];

    if (c < 0x800) {
        fmt(buf, sizeof(buf), "%%%02X%%%02X", 0xC0 | (c >> 6), 0x80 | (c & 0x3F));
    } else {
        fmt(buf, sizeof(buf), "%%%02X%%%02X%%%02X", 0xE0 | (c >> 12), 0x80 | ((c >> 6) & 0x3F), 0x80 | (c & 0x3F));
    }
    return sfmt("/auth/ba%sic/page.html", buf);
}

static bool accepted(uint c)
{
    char *uri, *path;

    uri = pathWith(c);
    path = websValidateUriPath(uri);
    wfree(uri);
    wfree(path);
    return path != 0;
}

/*
    Name the websValidateUriPath stage that refuses the code point, and the bytes it saw, for the failure message
 */
static cchar *refusal(uint c)
{
    static char result[160];
    char        *uri, *decoded, *normalized, *cp;
    cchar       *stage;
    ssize       len;

    uri = pathWith(c);
    decoded = walloc(slen(uri) + 1);
    decodeUrl(decoded, uri, -1, 0);
    normalized = websNormalizeUriPath(decoded);
    if (normalized == 0) {
        stage = "normalize";
    } else if (!validCanonicalPath(normalized)) {
        stage = "canonical";
    } else if (hasUnsafeEscape(normalized)) {
        stage = "escape";
    } else {
        stage = "none";
    }
    fmt(result, sizeof(result), "%s, uri %s, path", stage, uri);
    len = slen(result);
    for (cp = normalized; cp && *cp && len < (ssize) sizeof(result) - 4; cp++) {
        len += slen(fmt(&result[len], sizeof(result) - len, " %02X", (uchar) *cp));
    }
    wfree(uri);
    wfree(decoded);
    wfree(normalized);
    return result;
}

int main(int argc, char **argv)
{
    int i;

    for (i = 0; i < (int) (sizeof(folding) / sizeof(folding[0])); i++) {
        if (ME_GOAHEAD_CASE_INSENSITIVE_FS) {
            ttrue(!accepted(folding[i]), "U+%04X folds to ASCII and must be refused", folding[i]);
        } else {
            ttrue(accepted(folding[i]), "U+%04X names a distinct file and must be accepted, refused by %s", folding[i],
                  refusal(folding[i]));
        }
    }
    ttrue(accepted(0x00E9), "U+00E9 does not fold to ASCII and must be accepted, refused by %s", refusal(0x00E9));
    ttrue(accepted(0x212B), "U+212B, the Angstrom sign, does not fold to ASCII and must be accepted, refused by %s",
          refusal(0x212B));
    ttrue(accepted(0x0100), "U+0100 does not fold to ASCII and must be accepted, refused by %s", refusal(0x0100));
    return 0;
}
