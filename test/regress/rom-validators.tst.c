/* A ROM page's validators change with its content */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#undef ME_ROM
#define ME_ROM 1
#include "goahead.h"

static uchar v1[] = "version-1";
static uchar v2[] = "version-2";

WebsRomIndex websRomIndex[] = {
    { "/v1.js", v1, 9, 1700000000, 0x1111111111111111ULL },
    { "/v2.js", v2, 9, 1700000100, 0xf222222222222222ULL },
    { "/old.js", v1, 9 },
    { 0, 0, 0, 0, 0 }
};

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include "../../src/fs.c"
#include "../../src/file.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

int main(int argc, char **argv)
{
    WebsFileInfo one, two, old;
    Webs         wp;
    char         *tag1, *tag2;

    websFsOpen();

    teq(websStatFile("/v1.js", &one), 0, "stat the first page");
    teq(websStatFile("/v2.js", &two), 0, "stat the second page");
    teq(websStatFile("/old.js", &old), 0, "stat a page indexed without a time or hash");
    ttrue(one.size == two.size, "the pages are the same size");
    ttrue(one.hash == 0x1111111111111111ULL && two.hash == 0xf222222222222222ULL, "each page reports its hash");
    ttrue(one.mtime == 1700000000 && two.mtime == 1700000100, "each page reports its own modified time");

    tag1 = makeEtag(&one);
    tag2 = makeEtag(&two);
    ttrue(!smatch(tag1, tag2), "same-size pages with different content must have different ETags");
    tmatch(tag2, "W/\"f222222222222222-9\"", "the ETag is built from the hash, including its high bit");
    wfree(tag1);
    wfree(tag2);

    memset(&wp, 0, sizeof(wp));
    wp.method = "GET";
    wp.since = 1800000000;
    ttrue(old.mtime == 0, "a page indexed without a time has none");
    ttrue(!notModified(&wp, &old, "W/\"0-9\""), "If-Modified-Since cannot match a page with no known time");
    ttrue(notModified(&wp, &one, "W/\"1111111111111111-9\""), "If-Modified-Since still applies to a timed page");

    websFsClose();
    return 0;
}
