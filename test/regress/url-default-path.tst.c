/* Missing URL paths must resolve to the root path. */
/* TestMe's variadic reporting helpers forward format strings. */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#include "testme.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
#include "goahead.h"
int main(int argc, char **argv)
{
    cchar *urls[] = { "http://example.com", "https://example.com:443", "http://example.com/", "http://example.com/a", "", NULL };
    cchar *paths[] = { "/", "/", "/", "/a", "/", "/" };
    char *buf, *path;
    int i, rc;
    for (i = 0; i < 6; i++) {
        rc = websUrlParse(urls[i], &buf, NULL, NULL, NULL, &path, NULL, NULL, NULL);
        teq(rc, 0, "parse URL");
        tmatch(path, paths[i], "default or explicit path");
        wfree(buf);
    }
    return 0;
}
