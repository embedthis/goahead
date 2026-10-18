/*
    uripath-utf8-canonical.tst.c - Only the shortest UTF-8 form of a scalar value is a canonical path
 */

#include "testme.h"
#include "goahead.h"

static cchar *rejected[] = {
    "/a%C0%AFb",                        /* overlong '/' in two bytes */
    "/%C0%AF..%C0%AFetc",               /* the classic overlong traversal shape */
    "/%C1%BF",                          /* overlong, highest two-byte lead that is still overlong */
    "/a%E0%80%AEb",                     /* overlong '.' in three bytes */
    "/%E0%9F%BF",                       /* overlong: U+07FF in three bytes */
    "/%F0%8F%BF%BF",                    /* overlong: U+FFFF in four bytes */
    "/%ED%A0%80",                       /* surrogate U+D800 */
    "/%ED%BF%BF",                       /* surrogate U+DFFF */
    "/%F4%90%80%80",                    /* U+110000, past the last scalar value */
    "/%F5%80%80%80",                    /* F5 lead byte */
    "/%F7%BF%BF%BF",                    /* F7 lead byte */
    NULL
};

static cchar *accepted[] = {
    "/caf%C3%A9",                       /* U+00E9, the shortest two-byte form */
    "/%C2%80x",                         /* U+0080, the smallest two-byte value */
    "/%E0%A0%80",                       /* U+0800, the smallest three-byte value */
    "/%ED%9F%BF",                       /* U+D7FF, just below the surrogates */
    "/%EE%80%80",                       /* U+E000, just above them */
    "/%F0%90%80%80",                    /* U+10000, the smallest four-byte value */
    "/%F4%8F%BF%BF",                    /* U+10FFFF, the last scalar value */
    "/%F0%9F%98%80",                    /* an emoji */
    "/index.html",
    NULL
};

int main(int argc, char **argv)
{
    char *path;
    int  i;

    for (i = 0; rejected[i]; i++) {
        path = websValidateUriPath(rejected[i]);
        ttrue(path == NULL, "\"%s\" has no canonical UTF-8 spelling and must be refused", rejected[i]);
        wfree(path);
    }
    for (i = 0; accepted[i]; i++) {
        path = websValidateUriPath(accepted[i]);
        ttrue(path != NULL, "\"%s\" is well-formed UTF-8 and must be accepted", accepted[i]);
        wfree(path);
    }
    return 0;
}
