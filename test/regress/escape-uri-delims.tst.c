/*
    escape-uri-delims.tst.c - websEscapeUri must encode the characters that end a path: ?, # and the brackets
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

static void check(cchar *input, cchar *expected, cchar *why)
{
    char *escaped;

    escaped = websEscapeUri(input);
    tmatch(escaped, expected, "%s: expected %s, got %s", why, expected, escaped);
    wfree(escaped);
}

int main(int argc, char **argv)
{
    check("/q?d/index.html", "/q%3Fd/index.html", "a question mark in a path segment would start a query");
    check("/h#d/index.html", "/h%23d/index.html", "a hash in a path segment would start a fragment");
    check("/b[1]/x", "/b%5B1%5D/x", "brackets are gen-delims that do not belong in a path segment");
    check("/plain-._~/x", "/plain-._~/x", "unreserved characters and the segment separator stay as they are");
    check("/a b%c", "/a%20b%25c", "space and percent are still encoded");
    check("/sub/x;y=1,2+3", "/sub/x;y=1,2+3", "sub-delims permitted in a path segment stay as they are");
    return 0;
}
