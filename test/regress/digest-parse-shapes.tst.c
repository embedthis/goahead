/* The real Digest parser handles every quoted-string and backslash shape */

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
#undef ME_COMPILER_HAS_PAM
#define ME_COMPILER_HAS_PAM 0
#include "../../src/auth.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#if ME_GOAHEAD_AUTH && ME_GOAHEAD_DIGEST
typedef struct Shape {
    cchar *header;
    cchar *username;                    /* Expected username, or NULL for none */
    cchar *realm;                       /* Expected realm, or NULL for none */
} Shape;

static Shape shapes[] = {
    { "username=\"joshua\\", "joshua\\", NULL },                     /* trailing backslash at end of header */
    { "username=\"\\", "\\", NULL },                                 /* lone backslash, whole value */
    { "username=joshua\\", "joshua\\", NULL },                       /* unquoted trailing backslash */
    { "username=\"a\\\"b\", realm=\"x\"", "a\"b", "x" },             /* escaped quote inside a quoted string */
    { "username=\"a\\\\b\"", "a\\b", NULL },                         /* doubled backslash */
    { "username=\"a\\\\\\", "a\\\\", NULL },                         /* triple backslash trailing */
    { "username=\"joshua, nonce=\"abc\"", "joshua, nonce=", NULL },  /* quoted string ends at the next quote */
    { "username=\"\"", "", NULL },                                   /* empty quoted value */
    { "username=\"", "", NULL },                                     /* bare quote only */
    { "username=\"\\\\\\\\\\", "\\\\\\", NULL },                     /* only backslashes */
    { "username=\"a,b\"", "a,b", NULL },                             /* comma inside a quoted string */
    { "username=\"a\\,b\"", "a,b", NULL },                           /* escaped comma */
    { "username=\"a\\\", realm=\"evil\"", "a\", realm=", NULL },     /* an escaped quote keeps the string open */
    { "zzzz=\"x\\", NULL, NULL },                                    /* unknown field, trailing backslash */
    { "username=\"ok\", response=\"\\", "ok", NULL },                /* trailing backslash in the last field */
    { ",,,", NULL, NULL },
    { "=", NULL, NULL },
    { "\"", NULL, NULL },
    { "\\", NULL, NULL },
    { "", NULL, NULL },
    { " ", NULL, NULL },
    { "username", "", NULL },
    { "username=", "", NULL },
};


static bool same(cchar *actual, cchar *expected)
{
    return expected ? actual && smatch(actual, expected) : actual == 0;
}


int main(int argc, char **argv)
{
    Webs      wp;
    WebsRoute route;
    char      *heap;
    ssize     len;
    int       i;

    websRuntimeOpen();
    websOpenAuth(1);
    masterSecret = sclone("c3ludGhldGljLW1hc3Rlci1zZWNyZXQ=");
    memset(&wp, 0, sizeof(wp));
    memset(&route, 0, sizeof(route));
    wp.route = &route;
    wp.method = "GET";
    wp.url = "/";
    for (i = 0; i < (int) (sizeof(shapes) / sizeof(shapes[0])); i++) {
        len = slen(shapes[i].header) + 1;
        heap = walloc(len);
        memcpy(heap, shapes[i].header, (size_t) len);
        wp.authDetails = heap;
        parseDigestDetails(&wp);
        ttrue(same(wp.username, shapes[i].username), "[%s] gives username [%s], expected [%s]", shapes[i].header,
              wp.username ? wp.username : "(none)", shapes[i].username ? shapes[i].username : "(none)");
        ttrue(same(wp.realm, shapes[i].realm), "[%s] gives realm [%s], expected [%s]", shapes[i].header,
              wp.realm ? wp.realm : "(none)", shapes[i].realm ? shapes[i].realm : "(none)");
        wfreeset(wp.username);
        wfreeset(wp.realm);
        wfreeset(wp.nonce);
        wfreeset(wp.password);
        wfreeset(wp.digestUri);
        wfreeset(wp.qop);
        wfreeset(wp.nc);
        wfreeset(wp.cnonce);
        wfreeset(wp.opaque);
        wfree(heap);
    }
    wfreeset(masterSecret);
    return 0;
}

#else
int main(int argc, char **argv)
{
    tskip("Digest authentication is not enabled");
    return 0;
}
#endif
